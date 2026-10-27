#ifndef LAB2_APP_H
#define LAB2_APP_H

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#define SERVICE_NAME L"BSITLab2Backup"
#define SERVICE_TITLE L"BSIT Lab 2 - Backup"
#define PATH_CAP MAX_PATH
#define ERROR_CAP 512
#define MAX_MASKS 16
#define MASK_CAP 128

typedef struct {
    wchar_t source[PATH_CAP];
    wchar_t archive[PATH_CAP];
    wchar_t masks[MAX_MASKS][MASK_CAP];
    int mask_count;
    DWORD interval_ms;
} BackupConfig;

extern wchar_t app_directory[PATH_CAP];
extern wchar_t config_path[PATH_CAP];
extern wchar_t log_path[PATH_CAP];

BOOL fail(wchar_t *error, const wchar_t *format, ...);
BOOL join_path(wchar_t *result, const wchar_t *left, const wchar_t *right);
BOOL init_paths(wchar_t *error);
void write_log(const wchar_t *format, ...);
BOOL load_config(const wchar_t *path, BackupConfig *config, wchar_t *error);
BOOL mask_matches(const wchar_t *name, const wchar_t *mask);
BOOL create_backup(const BackupConfig *config, HANDLE stop_event,
                   unsigned *file_count, wchar_t *error);
BOOL backup_is_current(const BackupConfig *config, HANDLE stop_event,
                       BOOL *current, wchar_t *error);
BOOL monitor_backup(const BackupConfig *config, HANDLE stop_event, wchar_t *error);

#endif
