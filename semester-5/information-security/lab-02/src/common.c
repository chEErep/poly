#include "app.h"
#include <stdarg.h>
#include <string.h>

wchar_t app_directory[PATH_CAP];
wchar_t config_path[PATH_CAP];
wchar_t log_path[PATH_CAP];

// Записывает понятное описание ошибки и возвращает FALSE.
BOOL fail(wchar_t *error, const wchar_t *format, ...)
{
    va_list args;
    va_start(args, format);
    vswprintf(error, ERROR_CAP, format, args);
    va_end(args);
    error[ERROR_CAP - 1] = L'\0';
    return FALSE;
}

// Соединяет папку и имя, проверяя длину полученного пути.
BOOL join_path(wchar_t *result, const wchar_t *left, const wchar_t *right)
{
    size_t length = wcslen(left);
    BOOL separator = length > 0 && left[length - 1] != L'\\';
    if (length + separator + wcslen(right) >= PATH_CAP)
        return FALSE;
    wcscpy(result, left);
    if (separator)
        wcscat(result, L"\\");
    wcscat(result, right);
    return TRUE;
}

// Ищет конфигурацию и журнал рядом с EXE, независимо от текущей папки.
BOOL init_paths(wchar_t *error)
{
    DWORD length = GetModuleFileNameW(NULL, app_directory, PATH_CAP);
    if (!length || length >= PATH_CAP)
        return fail(error, L"Cannot get EXE path. Windows error: %lu", GetLastError());
    wchar_t *slash = wcsrchr(app_directory, L'\\');
    if (!slash)
        return fail(error, L"EXE directory was not found.");
    *slash = L'\0';
    if (!join_path(config_path, app_directory, L"backup.ini") ||
        !join_path(log_path, app_directory, L"backup.log"))
        return fail(error, L"EXE directory is too long.");
    return TRUE;
}

// Добавляет сообщение с датой в UTF-8 журнал и сразу закрывает файл.
void write_log(const wchar_t *format, ...)
{
    wchar_t message[ERROR_CAP] = {0};
    wchar_t line[ERROR_CAP + 64];
    char utf8[(ERROR_CAP + 64) * 4];
    SYSTEMTIME now;
    va_list args;
    va_start(args, format);
    vswprintf(message, ERROR_CAP, format, args);
    va_end(args);
    message[ERROR_CAP - 1] = L'\0';
    GetLocalTime(&now);
    swprintf(line, ERROR_CAP + 64, L"%04u-%02u-%02u %02u:%02u:%02u %ls\r\n",
             now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
             now.wSecond, message);
    int size = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), NULL, NULL);
    FILE *file = _wfopen(log_path, L"ab");
    if (file) {
        if (size > 0)
            fwrite(utf8, 1, (size_t)size - 1, file);
        fclose(file);
    }
}
