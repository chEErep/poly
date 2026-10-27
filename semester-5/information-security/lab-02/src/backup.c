#include "app.h"
#include "miniz.h"
#include <wctype.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

// Удаляет пробелы в начале и конце строки конфигурации.
static wchar_t *trim(wchar_t *text)
{
    while (iswspace(*text))
        ++text;
    size_t length = wcslen(text);
    while (length && iswspace(text[length - 1]))
        text[--length] = L'\0';
    return text;
}

// Принимает обычный UNC-путь и убирает из него лишние точки и разделители.
static BOOL normalize_unc(wchar_t *path, wchar_t *error)
{
    if (wcslen(path) < 5 || path[0] != L'\\' || path[1] != L'\\' ||
        path[2] == L'?' || path[2] == L'.' || path[2] == L'\\')
        return fail(error, L"Use a UNC path: \\\\server\\share\\folder");
    wchar_t *share = wcschr(path + 2, L'\\');
    if (!share || !share[1] || share[1] == L'\\' || wcspbrk(path, L"/*?\":"))
        return fail(error, L"Invalid UNC path: %ls", path);
    wchar_t full[PATH_CAP];
    DWORD size = GetFullPathNameW(path, PATH_CAP, full, NULL);
    if (!size || size >= PATH_CAP)
        return fail(error, L"UNC path is too long or invalid.");
    size_t length = wcslen(full);
    while (length && full[length - 1] == L'\\')
        full[--length] = L'\0';
    wcscpy(path, full);
    return TRUE;
}

// Читает UTF-8 файл: source, archive и несколько строк mask.
BOOL load_config(const wchar_t *path, BackupConfig *config, wchar_t *error)
{
    memset(config, 0, sizeof(*config));
    config->interval_ms = 2000;
    FILE *file = _wfopen(path, L"rt, ccs=UTF-8");
    if (!file)
        return fail(error, L"Cannot open config: %ls", path);
    wchar_t line[2048];
    BOOL ok = TRUE;
    unsigned number = 0;
    BOOL interval_seen = FALSE;
    while (fgetws(line, 2048, file)) {
        ++number;
        if (!wcschr(line, L'\n') && !feof(file)) {
            ok = fail(error, L"Config line %u is too long.", number);
            break;
        }
        wchar_t *key = trim(line[0] == 0xFEFF ? line + 1 : line);
        if (!*key || *key == L'#' || *key == L';')
            continue;
        wchar_t *equal = wcschr(key, L'=');
        if (!equal) {
            ok = fail(error, L"Expected key=value at line %u.", number);
            break;
        }
        *equal = L'\0';
        key = trim(key);
        wchar_t *value = trim(equal + 1);
        if (_wcsicmp(key, L"interval_seconds") == 0 && !interval_seen) {
            wchar_t *end;
            errno = 0;
            unsigned long seconds = wcstoul(value, &end, 10);
            if (errno || end == value || *end || seconds < 1 || seconds > 3600) {
                ok = fail(error, L"interval_seconds must be an integer from 1 to 3600.");
                break;
            }
            config->interval_ms = (DWORD)seconds * 1000;
            interval_seen = TRUE;
            continue;
        }
        wchar_t *destination = NULL;
        size_t capacity = PATH_CAP;
        if (_wcsicmp(key, L"source") == 0 && !config->source[0])
            destination = config->source;
        else if (_wcsicmp(key, L"archive") == 0 && !config->archive[0])
            destination = config->archive;
        else if (_wcsicmp(key, L"mask") == 0 && config->mask_count < MAX_MASKS) {
            destination = config->masks[config->mask_count++];
            capacity = MASK_CAP;
            if (wcspbrk(value, L"\\/:"))
                destination = NULL;
        }
        if (!destination || !*value || wcslen(value) >= capacity) {
            ok = fail(error, L"Unknown, repeated or invalid setting at line %u.", number);
            break;
        }
        wcscpy(destination, value);
    }
    if (ferror(file))
        ok = fail(error, L"Cannot read config file.");
    fclose(file);
    if (!ok)
        return FALSE;
    if (!config->source[0] || !config->archive[0] || !config->mask_count)
        return fail(error, L"Config needs source, archive and at least one mask.");
    if (!normalize_unc(config->source, error) || !normalize_unc(config->archive, error))
        return FALSE;
    size_t length = wcslen(config->archive);
    if (length < 4 || _wcsicmp(config->archive + length - 4, L".zip") != 0)
        return fail(error, L"Archive name must end with .zip");
    length = wcslen(config->source);
    if (_wcsnicmp(config->source, config->archive, length) == 0 &&
        (config->archive[length] == L'\\' || config->archive[length] == L'\0'))
        return fail(error, L"Archive must be outside the source directory.");
    return TRUE;
}

// Сравнивает имя с маской: * означает любое число символов, ? означает один.
BOOL mask_matches(const wchar_t *name, const wchar_t *mask)
{
    const wchar_t *star = NULL;
    const wchar_t *retry = NULL;
    while (*name) {
        if (*mask == L'?' || (*mask && towlower(*mask) == towlower(*name))) {
            ++name;
            ++mask;
        } else if (*mask == L'*') {
            star = mask++;
            retry = name;
        } else if (star) {
            mask = star + 1;
            name = ++retry;
        } else {
            return FALSE;
        }
    }
    while (*mask == L'*')
        ++mask;
    return !*mask;
}

// Проверяет, подходит ли имя хотя бы под одну из масок.
static BOOL selected(const BackupConfig *config, const wchar_t *name)
{
    for (int i = 0; i < config->mask_count; ++i)
        if (mask_matches(name, config->masks[i]))
            return TRUE;
    return FALSE;
}

// Проверяет запрос остановки без ожидания и без нагрузки на процессор.
static BOOL stopping(HANDLE event)
{
    return event && WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
}

typedef struct {
    HANDLE file;
    HANDLE stop_event;
    DWORD error;
    BOOL different;
} FileReader;

// miniz вызывает эту функцию порциями, поэтому файл не загружается целиком в RAM.
static size_t read_chunk(void *context, mz_uint64 offset, void *buffer, size_t size)
{
    FileReader *reader = context;
    LARGE_INTEGER position;
    position.QuadPart = (LONGLONG)offset;
    DWORD bytes = 0;
    if (stopping(reader->stop_event)) {
        reader->error = ERROR_CANCELLED;
        return 0;
    }
    if (size > MAXDWORD || !SetFilePointerEx(reader->file, position, NULL, FILE_BEGIN) ||
        !ReadFile(reader->file, buffer, (DWORD)size, &bytes, NULL)) {
        reader->error = GetLastError();
        return 0;
    }
    if (bytes != size)
        reader->error = ERROR_HANDLE_EOF;
    return bytes;
}

// Превращает относительный путь Windows в имя записи ZIP.
static BOOL entry_name(const wchar_t *relative, char *entry, wchar_t *error)
{
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, relative, -1,
                            entry, PATH_CAP * 4, NULL, NULL))
        return fail(error, L"Cannot encode ZIP entry name.");
    for (char *p = entry; *p; ++p)
        if (*p == '\\')
            *p = '/';
    return TRUE;
}

// Добавляет один файл с относительным UTF-8 именем и временем изменения.
static BOOL add_file(mz_zip_archive *zip, const wchar_t *disk_path,
                     const wchar_t *relative, HANDLE event, wchar_t *error)
{
    char entry[PATH_CAP * 4];
    if (!entry_name(relative, entry, error))
        return FALSE;
    FileReader reader = {INVALID_HANDLE_VALUE, event, ERROR_SUCCESS, FALSE};
    // Разрешаем чтение, но запрещаем менять файл в процессе его копирования.
    reader.file = CreateFileW(disk_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (reader.file == INVALID_HANDLE_VALUE)
        return fail(error, L"Cannot open %ls. Windows error: %lu", disk_path, GetLastError());
    LARGE_INTEGER size;
    FILETIME modified;
    if (!GetFileSizeEx(reader.file, &size) || !GetFileTime(reader.file, NULL, NULL, &modified)) {
        DWORD code = GetLastError();
        CloseHandle(reader.file);
        return fail(error, L"Cannot get file information. Windows error: %lu", code);
    }
    ULARGE_INTEGER ticks;
    ticks.LowPart = modified.dwLowDateTime;
    ticks.HighPart = modified.dwHighDateTime;
    time_t timestamp = ticks.QuadPart >= 116444736000000000ULL ?
        (time_t)((ticks.QuadPart - 116444736000000000ULL) / 10000000ULL) : 0;
    BOOL ok = mz_zip_writer_add_read_buf_callback(zip, entry, read_chunk, &reader,
        (mz_uint64)size.QuadPart, &timestamp, NULL, 0, MZ_DEFAULT_LEVEL,
        NULL, 0, NULL, 0);
    CloseHandle(reader.file);
    if (!ok)
        return fail(error, L"Cannot archive %ls. Read error: %lu. ZIP: %hs",
                    relative, reader.error, mz_zip_get_error_string(mz_zip_peek_last_error(zip)));
    return TRUE;
}

// Сравнивает распакованную порцию ZIP с исходным файлом, не загружая его целиком.
static size_t compare_chunk(void *context, mz_uint64 offset, const void *data, size_t size)
{
    FileReader *reader = context;
    unsigned char buffer[65536];
    size_t done = 0;
    while (done < size) {
        size_t part = size - done < sizeof(buffer) ? size - done : sizeof(buffer);
        if (read_chunk(reader, offset + done, buffer, part) != part)
            return 0;
        if (memcmp(buffer, (const unsigned char *)data + done, part) != 0) {
            reader->different = TRUE;
            return 0;
        }
        done += part;
    }
    return size;
}

// Проверяет размер и сами байты файла: изменение той же длины тоже обнаружится.
static BOOL compare_file(mz_zip_archive *zip, const wchar_t *disk_path,
                        const wchar_t *relative, HANDLE event, BOOL *current, wchar_t *error)
{
    char entry[PATH_CAP * 4];
    if (!entry_name(relative, entry, error))
        return FALSE;
    int index = mz_zip_reader_locate_file(zip, entry, NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (index < 0) {
        *current = FALSE;
        return TRUE;
    }
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(zip, (mz_uint)index, &stat)) {
        *current = FALSE;
        return TRUE;
    }
    FileReader reader = {INVALID_HANDLE_VALUE, event, ERROR_SUCCESS, FALSE};
    reader.file = CreateFileW(disk_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (reader.file == INVALID_HANDLE_VALUE)
        return fail(error, L"Cannot check %ls. Windows error: %lu", disk_path, GetLastError());
    LARGE_INTEGER size;
    if (!GetFileSizeEx(reader.file, &size)) {
        DWORD code = GetLastError();
        CloseHandle(reader.file);
        return fail(error, L"Cannot get file size. Windows error: %lu", code);
    }
    BOOL same = (mz_uint64)size.QuadPart == stat.m_uncomp_size && !stat.m_is_directory;
    if (same)
        same = mz_zip_reader_extract_to_callback(zip, (mz_uint)index, compare_chunk, &reader, 0);
    CloseHandle(reader.file);
    if (reader.error != ERROR_SUCCESS)
        return fail(error, L"Cannot compare %ls. Windows error: %lu", disk_path, reader.error);
    *current = same && !reader.different;
    return TRUE;
}

// Обходит подпапки: current == NULL означает запись, иначе проверку готового ZIP.
static BOOL walk_directory(mz_zip_archive *zip, const BackupConfig *config,
                          const wchar_t *relative, HANDLE event, unsigned depth,
                          unsigned *count, BOOL *current, wchar_t *error)
{
    wchar_t directory[PATH_CAP], search[PATH_CAP];
    if (depth > 32 || !join_path(directory, config->source, relative) ||
        !join_path(search, directory, L"*"))
        return fail(error, L"Directory path is too long or nesting exceeds 32 levels.");
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(search, &data);
    if (find == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND)
            return TRUE;
        return fail(error, L"Cannot read %ls. Windows error: %lu", directory, code);
    }
    BOOL ok = TRUE;
    do {
        if (stopping(event)) {
            ok = fail(error, L"Backup cancelled by stop request.");
            break;
        }
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
            continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            continue;
        wchar_t child[PATH_CAP], disk_path[PATH_CAP];
        if (!join_path(child, relative, data.cFileName) ||
            !join_path(disk_path, config->source, child)) {
            ok = fail(error, L"A file path exceeds 259 characters.");
            break;
        }
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ok = walk_directory(zip, config, child, event, depth + 1, count, current, error);
        else if (selected(config, data.cFileName)) {
            ok = current ? compare_file(zip, disk_path, child, event, current, error) :
                           add_file(zip, disk_path, child, event, error);
            if (ok)
                ++*count;
        }
        if (!ok || (current && !*current))
            break;
    } while (FindNextFileW(find, &data));
    DWORD code = GetLastError();
    FindClose(find);
    if (ok && (!current || *current) && code != ERROR_NO_MORE_FILES)
        return fail(error, L"Directory enumeration failed. Windows error: %lu", code);
    return ok;
}

// Создаёт ZIP во временном файле; заменяет старый архив только после успеха.
BOOL create_backup(const BackupConfig *config, HANDLE stop_event,
                   unsigned *file_count, wchar_t *error)
{
    *file_count = 0;
    DWORD attributes = GetFileAttributesW(config->source);
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return fail(error, L"Source directory is unavailable. Windows error: %lu", GetLastError());
    wchar_t parent[PATH_CAP], temporary[PATH_CAP];
    wcscpy(parent, config->archive);
    wchar_t *slash = wcsrchr(parent, L'\\');
    if (!slash)
        return fail(error, L"Archive directory is missing.");
    *slash = L'\0';
    if (!GetTempFileNameW(parent, L"bsi", 0, temporary))
        return fail(error, L"Cannot create a temporary ZIP. Windows error: %lu", GetLastError());
    FILE *file = _wfopen(temporary, L"w+b");
    if (!file) {
        DeleteFileW(temporary);
        return fail(error, L"Cannot open a temporary ZIP.");
    }
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    BOOL initialized = mz_zip_writer_init_cfile(&zip, file, 0);
    BOOL ok = initialized;
    if (!initialized)
        fail(error, L"Cannot initialize ZIP writer.");
    if (ok)
        ok = walk_directory(&zip, config, L"", stop_event, 0, file_count, NULL, error);
    if (ok && stopping(stop_event))
        ok = fail(error, L"Backup cancelled by stop request.");
    if (ok && !mz_zip_writer_finalize_archive(&zip))
        ok = fail(error, L"Cannot finalize ZIP: %hs", mz_zip_get_error_string(mz_zip_peek_last_error(&zip)));
    if (initialized)
        mz_zip_writer_end(&zip);
    if (fclose(file) != 0 && ok)
        ok = fail(error, L"Cannot flush and close ZIP.");
    if (ok && stopping(stop_event))
        ok = fail(error, L"Backup cancelled by stop request.");
    if (ok && !MoveFileExW(temporary, config->archive, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        ok = fail(error, L"Cannot replace ZIP. Windows error: %lu", GetLastError());
    if (!ok)
        DeleteFileW(temporary);
    return ok;
}

// Сравнивает исходную папку с ZIP; отсутствие или повреждение ZIP требует новой копии.
BOOL backup_is_current(const BackupConfig *config, HANDLE stop_event,
                       BOOL *current, wchar_t *error)
{
    *current = FALSE;
    DWORD attributes = GetFileAttributesW(config->archive);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
            return TRUE;
        return fail(error, L"Cannot access ZIP. Windows error: %lu", code);
    }
    FILE *file = _wfopen(config->archive, L"rb");
    if (!file)
        return fail(error, L"Cannot open ZIP for checking.");
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    BOOL initialized = mz_zip_reader_init_cfile(&zip, file, 0, 0);
    BOOL ok = TRUE;
    if (initialized) {
        unsigned count = 0;
        *current = TRUE;
        ok = walk_directory(&zip, config, L"", stop_event, 0, &count, current, error);
        if (ok && *current)
            *current = count == mz_zip_reader_get_num_files(&zip);
        mz_zip_reader_end(&zip);
    }
    fclose(file);
    return ok;
}
