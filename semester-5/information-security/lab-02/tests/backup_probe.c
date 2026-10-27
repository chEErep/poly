#include "app.h"
#include <locale.h>

// Имитирует команду SCM во время чтения большого файла.
static DWORD WINAPI cancel_later(void *context)
{
    Sleep(20);
    SetEvent((HANDLE)context);
    return 0;
}

// Завершает тестовый цикл после строки из Python; это не команда пользователя службы.
static DWORD WINAPI stop_from_input(void *context)
{
    (void)getchar();
    SetEvent((HANDLE)context);
    return 0;
}

// Тестовый инструмент: вызывает архиватор с локальными путями без SCM и SMB.
int wmain(int argc, wchar_t **argv)
{
    setlocale(LC_ALL, "");
    if (argc < 4 || wcslen(argv[1]) >= PATH_CAP || wcslen(argv[2]) >= PATH_CAP)
        return 2;
    BackupConfig config = {0};
    wcscpy(config.source, argv[1]);
    wcscpy(config.archive, argv[2]);
    HANDLE event = NULL, thread = NULL;
    int first_mask = 3;
    BOOL watching = wcscmp(argv[3], L"--watch") == 0;
    if (watching || wcscmp(argv[3], L"--cancel") == 0 || wcscmp(argv[3], L"--cancel-later") == 0) {
        event = CreateEventW(NULL, TRUE, wcscmp(argv[3], L"--cancel") == 0, NULL);
        if (!event)
            return 2;
        if (watching || wcscmp(argv[3], L"--cancel-later") == 0) {
            thread = CreateThread(NULL, 0, watching ? stop_from_input : cancel_later, event, 0, NULL);
            if (!thread) {
                CloseHandle(event);
                return 2;
            }
        }
        ++first_mask;
    }
    for (int i = first_mask; i < argc && config.mask_count < MAX_MASKS; ++i) {
        if (wcslen(argv[i]) >= MASK_CAP)
            return 2;
        wcscpy(config.masks[config.mask_count++], argv[i]);
    }
    wchar_t error[ERROR_CAP];
    unsigned count = 0;
    config.interval_ms = 100;
    BOOL ok = watching ? init_paths(error) && monitor_backup(&config, event, error) :
                         create_backup(&config, event, &count, error);
    if (thread) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }
    if (event)
        CloseHandle(event);
    if (!ok)
        wprintf(L"ERROR: %ls\n", error);
    else
        wprintf(L"Archived %u files.\n", count);
    return ok ? 0 : 1;
}
