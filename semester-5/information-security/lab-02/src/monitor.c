#include "app.h"

// Создаёт первую копию, затем проверяет файлы до запроса остановки.
BOOL monitor_backup(const BackupConfig *config, HANDLE stop_event, wchar_t *error)
{
    BOOL first = TRUE;
    DWORD delay = 0;
    for (;;) {
        DWORD result = WaitForSingleObject(stop_event, delay);
        if (result == WAIT_OBJECT_0)
            return TRUE;
        if (result != WAIT_TIMEOUT)
            return fail(error, L"Cannot wait for stop. Windows error: %lu", GetLastError());
        delay = config->interval_ms ? config->interval_ms : 2000;
        BOOL current = FALSE;
        BOOL ok = first || backup_is_current(config, stop_event, &current, error);
        if (ok && current)
            continue;
        unsigned count = 0;
        if (ok)
            ok = create_backup(config, stop_event, &count, error);
        if (WaitForSingleObject(stop_event, 0) == WAIT_OBJECT_0)
            return TRUE;
        if (ok) {
            write_log(first ? L"Backup ready: %u files. Automatic checks every %lu ms." :
                              L"Backup updated: %u files. Same archive path.", count, delay);
            first = FALSE;
        } else {
            // Занятый файл или временный сбой не останавливает службу.
            write_log(L"RETRY: %ls", error);
        }
    }
}
