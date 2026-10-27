#include "app.h"
#include <locale.h>
#include <fcntl.h>
#include <io.h>

static SERVICE_STATUS_HANDLE status_handle;
static HANDLE stop_event;

// Сообщает SCM состояние службы; команды остановки принимаются после запуска.
static void report_status(DWORD state, DWORD error)
{
    SERVICE_STATUS status = {0};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwWin32ExitCode = error;
    if (error == ERROR_SERVICE_SPECIFIC_ERROR)
        status.dwServiceSpecificExitCode = 1;
    if (state == SERVICE_RUNNING)
        status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    if (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING) {
        status.dwCheckPoint = 1;
        status.dwWaitHint = 30000;
    }
    SetServiceStatus(status_handle, &status);
}

// Быстро принимает запрос остановки; сам архив здесь не создаётся и не закрывается.
static DWORD WINAPI control_handler(DWORD control, DWORD type, void *data, void *context)
{
    (void)type;
    (void)data;
    (void)context;
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        report_status(SERVICE_STOP_PENDING, NO_ERROR);
        //переводит событие в состояние "сигнал имеется"
        SetEvent(stop_event);
        return NO_ERROR;
    }
    if (control == SERVICE_CONTROL_INTERROGATE)
        return NO_ERROR;
    return ERROR_CALL_NOT_IMPLEMENTED;
}

// Читает настройки и запускает автоматическое резервное копирование.
static void WINAPI service_main(DWORD argc, wchar_t **argv)
{
    (void)argc;
    (void)argv;
    //регистрируется обработчик команд
    status_handle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, control_handler, NULL);
    if (!status_handle)
        return;
    report_status(SERVICE_START_PENDING, NO_ERROR);
    //создается событие остановки
    stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!stop_event) {
        report_status(SERVICE_STOPPED, GetLastError());
        return;
    }
    //получение настроек службой
    BackupConfig config;
    wchar_t error[ERROR_CAP];
    if (!load_config(config_path, &config, error)) {
        write_log(L"ERROR: %ls", error);
        CloseHandle(stop_event);
        report_status(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR); //служба сообщает состояние
        return;
    }
    report_status(SERVICE_RUNNING, NO_ERROR);
    write_log(L"Service started. Creating the initial backup.");
    BOOL ok = monitor_backup(&config, stop_event, error);
    if (!ok)
        write_log(L"ERROR: %ls", error);
    //код завершения
    BOOL cancelled = WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0;
    CloseHandle(stop_event); //освобождаем дескриптор
    write_log(L"Service stopped.");
    report_status(SERVICE_STOPPED, ok || cancelled ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
}

// Открывает установленную службу только с правами, нужными для команды.
static SC_HANDLE open_service(DWORD access, wchar_t *error)
{
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!manager) {
        fail(error, L"Cannot open SCM. Windows error: %lu", GetLastError());
        return NULL;
    }
    SC_HANDLE service = OpenServiceW(manager, SERVICE_NAME, access);
    DWORD code = GetLastError();
    CloseServiceHandle(manager);
    if (!service)
        fail(error, L"Cannot open service. Windows error: %lu", code);
    return service;
}

// Регистрирует EXE как службу с ручным запуском от LocalSystem.
static BOOL install_service(wchar_t *error)
{
    wchar_t executable[PATH_CAP], command[PATH_CAP + 4];
    DWORD size = GetModuleFileNameW(NULL, executable, PATH_CAP);
    if (!size || size >= PATH_CAP)
        return fail(error, L"Cannot get service EXE path.");
    // Кавычки защищают путь с пробелами от запуска другого EXE.
    swprintf(command, PATH_CAP + 4, L"\"%ls\"", executable);
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!manager)
        return fail(error, L"Cannot install: open an administrator terminal. Windows error: %lu", GetLastError());
    SC_HANDLE service = CreateServiceW(manager, SERVICE_NAME, SERVICE_TITLE,
        SERVICE_QUERY_STATUS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL, command, NULL, NULL, NULL, NULL, NULL);
    DWORD code = GetLastError();
    if (service)
        CloseServiceHandle(service);
    CloseServiceHandle(manager);
    if (!service)
        return fail(error, L"CreateService failed. Windows error: %lu", code);
    return TRUE;
}

// Ждёт подтверждения запуска или остановки от Windows, но не бесконечно.
static BOOL wait_for_state(SC_HANDLE service, DWORD desired, wchar_t *error)
{
    ULONGLONG begin = GetTickCount64();
    SERVICE_STATUS_PROCESS status;
    DWORD bytes;
    do {
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                  (BYTE *)&status, sizeof(status), &bytes))
            return fail(error, L"Cannot query service. Windows error: %lu", GetLastError());
        if (status.dwCurrentState == desired)
            return TRUE;
        if (desired == SERVICE_RUNNING && status.dwCurrentState == SERVICE_STOPPED)
            return fail(error, L"Service failed to start. Windows error: %lu. See backup.log.", status.dwWin32ExitCode);
        Sleep(200);
    } while (GetTickCount64() - begin < 30000);
    return fail(error, L"Service transition timed out. See backup.log and Services.");
}

// Запрашивает запуск службы и проверяет, что Windows приняла запуск.
static BOOL start_service(wchar_t *error)
{
    SC_HANDLE service = open_service(SERVICE_START | SERVICE_QUERY_STATUS, error);
    if (!service)
        return FALSE;
    BOOL ok = StartServiceW(service, 0, NULL);
    DWORD code = GetLastError();
    if (!ok && code != ERROR_SERVICE_ALREADY_RUNNING)
        fail(error, L"StartService failed. Windows error: %lu", code);
    else
        ok = wait_for_state(service, SERVICE_RUNNING, error);
    CloseServiceHandle(service);
    return ok;
}

// Отправляет SERVICE_CONTROL_STOP; ждёт, пока служба завершит работу.
static BOOL stop_service(wchar_t *error)
{
    SC_HANDLE service = open_service(SERVICE_STOP | SERVICE_QUERY_STATUS, error);
    if (!service)
        return FALSE;
    SERVICE_STATUS status;
    BOOL ok = ControlService(service, SERVICE_CONTROL_STOP, &status);
    DWORD code = GetLastError();
    if (!ok && code != ERROR_SERVICE_NOT_ACTIVE)
        fail(error, L"ControlService failed. Windows error: %lu", code);
    else
        ok = wait_for_state(service, SERVICE_STOPPED, error);
    CloseServiceHandle(service);
    return ok;
}

// Удаляет регистрацию остановленной службы, сохраняя EXE, настройки и архив.
static BOOL remove_service(wchar_t *error)
{
    SC_HANDLE service = open_service(DELETE | SERVICE_QUERY_STATUS, error);
    if (!service)
        return FALSE;
    SERVICE_STATUS_PROCESS status;
    DWORD bytes;
    BOOL ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                   (BYTE *)&status, sizeof(status), &bytes);
    if (!ok)
        fail(error, L"Cannot query service. Windows error: %lu", GetLastError());
    else if (status.dwCurrentState != SERVICE_STOPPED)
        ok = fail(error, L"Stop the service before removing it.");
    else if (!DeleteService(service))
        ok = fail(error, L"DeleteService failed. Windows error: %lu", GetLastError());
    CloseServiceHandle(service);
    return ok;
}

// Выбирает команду SCP; без аргументов запускается служебный режим через SCM.
int wmain(int argc, wchar_t **argv)
{
    setlocale(LC_ALL, "");
    // У процесса службы может не быть стандартных дескрипторов консоли.
    if (_fileno(stdout) >= 0)
        _setmode(_fileno(stdout), _O_U8TEXT);
    if (_fileno(stderr) >= 0)
        _setmode(_fileno(stderr), _O_U8TEXT);
    wchar_t error[ERROR_CAP] = {0};
    if (!init_paths(error)) {
        fwprintf(stderr, L"ERROR: %ls\n", error);
        return 1;
    }
    BOOL ok = FALSE;
    if (argc == 1) {
        SERVICE_TABLE_ENTRYW table[] = {
            {SERVICE_NAME, service_main}, {NULL, NULL}
        };
        ok = StartServiceCtrlDispatcherW(table);
        if (!ok)
            fail(error, L"Run install/start commands, or use once. Windows error: %lu", GetLastError());
    } else if (argc == 2 && _wcsicmp(argv[1], L"install") == 0)
        ok = install_service(error);
    else if (argc == 2 && _wcsicmp(argv[1], L"start") == 0)
        ok = start_service(error);
    else if (argc == 2 && _wcsicmp(argv[1], L"stop") == 0)
        ok = stop_service(error);
    else if (argc == 2 && _wcsicmp(argv[1], L"remove") == 0)
        ok = remove_service(error);
    else if ((argc == 2 || argc == 3) && _wcsicmp(argv[1], L"once") == 0) {
        BackupConfig config;
        unsigned count;
        ok = load_config(argc == 3 ? argv[2] : config_path, &config, error);
        if (ok)
            ok = create_backup(&config, NULL, &count, error);
        if (ok)
            wprintf(L"Backup ready: %u files.\n", count);
    } else {
        fail(error, L"Usage: backup_service.exe install|start|stop|remove|once [config-file]");
    }
    if (!ok) {
        fwprintf(stderr, L"ERROR: %ls\n", error);
        return 1;
    }
    wprintf(L"OK\n");
    return 0;
}
