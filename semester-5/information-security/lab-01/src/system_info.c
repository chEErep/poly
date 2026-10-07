#include "system_info.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winternl.h>

#include <aclapi.h>//нужен для получения владельца
#include <sddl.h>//нужен для перевода sid в строку

#include "protocol.h"

typedef LONG (WINAPI *rtl_get_version_fn)(PRTL_OSVERSIONINFOW);

// Безопасно дописывает форматированный текст в общий буфер: used хранит
// число уже занятых байтов; при нехватке места возвращается 0
static int append_text(char *output,
                       size_t output_size,
                       size_t *used,
                       const char *format,
                       ...) {
    int written;
    va_list arguments;

    if (*used >= output_size) {
        return 0;
    }

    va_start(arguments, format);
    written = vsnprintf(output + *used, output_size - *used, format, arguments);
    va_end(arguments);

    if (written < 0 || (size_t) written >= output_size - *used) {
        return 0;
    }

    *used += (size_t) written;
    return 1;
}

// Версия ОС. RtlGetVersion вызывается через адрес функции из ntdll.dll,
// потому что показанный в методичке GetVersionEx может неверно сообщать
// версию новых Windows без соответствующего manifest приложения
static int build_os(char *output, size_t output_size) {
    HMODULE ntdll;
    FARPROC function_address;
    rtl_get_version_fn get_version;
    RTL_OSVERSIONINFOEXW version;
    const char *name = "Windows";
    size_t used = 0;

    ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == NULL) {
        return snprintf(output, output_size,
                        "status=error\ncode=ntdll_not_found\n") > 0;
    }

    function_address = GetProcAddress(ntdll, "RtlGetVersion");
    if (function_address == NULL
        || sizeof(function_address) != sizeof(get_version)) {
        return snprintf(output, output_size,
                        "status=error\ncode=rtl_get_version_not_found\n") > 0;
    }
    memcpy(&get_version, &function_address, sizeof(get_version));

    ZeroMemory(&version, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    if (get_version((PRTL_OSVERSIONINFOW)&version) != 0) {
        return snprintf(output, output_size,
                        "status=error\ncode=rtl_get_version_failed\n") > 0;
    }

    if (version.wProductType == VER_NT_SERVER
        || version.wProductType == VER_NT_DOMAIN_CONTROLLER) {
        name = "Windows Server";
    } else if (version.dwMajorVersion == 10 && version.dwBuildNumber >= 22000) {
        name = "Windows 11";
    } else if (version.dwMajorVersion == 10) {
        name = "Windows 10";
    } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 3) {
        name = "Windows 8.1";
    } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 2) {
        name = "Windows 8";
    } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 1) {
        name = "Windows 7";
    }

    return append_text(output, output_size, &used, "status=ok\n")
           && append_text(output, output_size, &used, "name=%s\n", name)
           && append_text(output, output_size, &used, "major=%lu\n",
                          (unsigned long) version.dwMajorVersion)
           && append_text(output, output_size, &used, "minor=%lu\n",
                          (unsigned long) version.dwMinorVersion)
           && append_text(output, output_size, &used, "build=%lu\n",
                          (unsigned long) version.dwBuildNumber);
}

// В ответе два времени: UTC и локальное время той машины, где запущен сервер
static int build_time(char *output, size_t output_size) {
    SYSTEMTIME utc;
    SYSTEMTIME local;
    size_t used = 0;

    GetSystemTime(&utc);
    GetLocalTime(&local);

    return append_text(output, output_size, &used, "status=ok\n")
           && append_text(output, output_size, &used,
                          "utc=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\n",
                          utc.wYear, utc.wMonth, utc.wDay,
                          utc.wHour, utc.wMinute, utc.wSecond,
                          utc.wMilliseconds)
           && append_text(output, output_size, &used,
                          "local=%04u-%02u-%02uT%02u:%02u:%02u.%03u\n",
                          local.wYear, local.wMonth, local.wDay,
                          local.wHour, local.wMinute, local.wSecond,
                          local.wMilliseconds);
}

// GetTickCount64 возвращает миллисекунды с запуска ОС; остатками от деления
// раскладываем длительность на дни, часы, минуты и секунды
static int build_uptime(char *output, size_t output_size) {
    ULONGLONG milliseconds = GetTickCount64();
    ULONGLONG total_seconds = milliseconds / 1000u;
    ULONGLONG days = total_seconds / 86400u;
    ULONGLONG hours = (total_seconds % 86400u) / 3600u;
    ULONGLONG minutes = (total_seconds % 3600u) / 60u;
    ULONGLONG seconds = total_seconds % 60u;
    size_t used = 0;

    return append_text(output, output_size, &used, "status=ok\n")
           && append_text(output, output_size, &used,
                          "total_milliseconds=%llu\n",
                          (unsigned long long) milliseconds)
           && append_text(output, output_size, &used,
                          "total_seconds=%llu\n",
                          (unsigned long long) total_seconds)
           && append_text(output, output_size, &used,
                          "days=%llu\nhours=%llu\nminutes=%llu\nseconds=%llu\n",
                          (unsigned long long) days,
                          (unsigned long long) hours,
                          (unsigned long long) minutes,
                          (unsigned long long) seconds);
}

// GlobalMemoryStatusEx выдаёт загрузку и объёмы памяти в байтах
static int build_memory(char *output, size_t output_size) {
    MEMORYSTATUSEX memory;
    size_t used = 0;

    ZeroMemory(&memory, sizeof(memory));
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory)) {
        return snprintf(output, output_size,
                        "status=error\ncode=memory_query_failed\nwin32_error=%lu\n",
                        (unsigned long) GetLastError()) > 0;
    }

    return append_text(output, output_size, &used, "status=ok\n")
           && append_text(output, output_size, &used, "load_percent=%lu\n",
                          (unsigned long) memory.dwMemoryLoad)
           && append_text(output, output_size, &used, "total_physical_bytes=%llu\n",
                          (unsigned long long) memory.ullTotalPhys)
           && append_text(output, output_size, &used, "available_physical_bytes=%llu\n",
                          (unsigned long long) memory.ullAvailPhys)
           && append_text(output, output_size, &used, "total_page_file_bytes=%llu\n",
                          (unsigned long long) memory.ullTotalPageFile)
           && append_text(output, output_size, &used, "available_page_file_bytes=%llu\n",
                          (unsigned long long) memory.ullAvailPageFile)
           && append_text(output, output_size, &used, "total_virtual_bytes=%llu\n",
                          (unsigned long long) memory.ullTotalVirtual)
           && append_text(output, output_size, &used, "available_virtual_bytes=%llu\n",
                          (unsigned long long) memory.ullAvailVirtual);
}

// Перевод числового типа диска Windows в понятное значение протокола
static const char *drive_type_name(UINT type) {
    switch (type) {
        case DRIVE_REMOVABLE:
            return "removable";
        case DRIVE_FIXED:
            return "fixed";
        case DRIVE_REMOTE:
            return "network";
        case DRIVE_CDROM:
            return "cdrom";
        case DRIVE_RAMDISK:
            return "ramdisk";
        default:
            return "unknown";
    }
}

// Запрос 5: список дисков, их типов и файловых систем
static int build_drives(char *output, size_t output_size) {
    DWORD drive_mask = GetLogicalDrives(); //возвращает битовую маску букв дискорв а не массив строк
    size_t used = 0; //сколько байтов уже написали в текст ответа
    unsigned int count = 0; //скок дисков в ответе
    unsigned int i;

    if (drive_mask == 0) {
        DWORD error = GetLastError();
        return append_text(output, output_size, &used,
                           "status=error\ncode=logical_drives_failed\n"
                           "win32_error=%lu\n",
                           (unsigned long) error);
    }

    if (!append_text(output, output_size, &used, "status=ok\n")) {
        return 0;
    }

    for (i = 0; i < 26; ++i) {
        char root[] = "A:\\";
        char filesystem[MAX_PATH + 1] = "unavailable";
        UINT type;

        // Бит 0 соответствует A:, бит 1 — B: и так далее
        if ((drive_mask & (1u << i)) == 0) { //1u << i создаёт число, у которого включён только бит для текущей буквы
            continue;
        }

        root[0] = (char) ('A' + i);
        type = GetDriveTypeA(root);

        if (!GetVolumeInformationA(root, NULL, 0, NULL, NULL, NULL,
                                   filesystem, sizeof(filesystem))) {
            strcpy(filesystem, "unavailable");
        }

        if (!append_text(output, output_size, &used,
                         "drive_%u_root=%s\n"
                         "drive_%u_type=%s\n"
                         "drive_%u_filesystem=%s\n",
                         count, root,
                         count, drive_type_name(type),
                         count, filesystem)) {
            return 0;
        }

        ++count;
    }

    return append_text(output, output_size, &used, "count=%u\n", count);
}

// Запрос 6: свободное место на фиксированных и съёмных дисках
static int build_disk_space(char *output, size_t output_size) {
    DWORD drive_mask = GetLogicalDrives();
    size_t used = 0;
    unsigned int count = 0;
    unsigned int i;

    if (drive_mask == 0) {
        DWORD error = GetLastError();
        return append_text(output, output_size, &used,
                           "status=error\ncode=logical_drives_failed\n"
                           "win32_error=%lu\n",
                           (unsigned long) error);
    }

    if (!append_text(output, output_size, &used, "status=ok\n")) {
        return 0;
    }

    for (i = 0; i < 26; ++i) {
        char root[] = "A:\\";
        UINT type;
        ULARGE_INTEGER free_bytes;

        if ((drive_mask & (1u << i)) == 0) {
            continue;
        }

        root[0] = (char) ('A' + i);
        type = GetDriveTypeA(root);

        // Сетевые диски не включаем. Съёмный нужен для показа флешки
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE) {
            continue;
        }

        if (!append_text(output, output_size, &used,
                         "drive_%u_root=%s\n"
                         "drive_%u_type=%s\n",
                         count, root,
                         count, drive_type_name(type))) {
            return 0;
        }

        if (GetDiskFreeSpaceExA(root, NULL, NULL, &free_bytes)) {
            if (!append_text(output, output_size, &used,
                             "drive_%u_status=ok\n"
                             "drive_%u_free_bytes=%llu\n",
                             count, count,
                             (unsigned long long) free_bytes.QuadPart)) {
                return 0;
            }
        } else {
            DWORD error = GetLastError();
            if (!append_text(output, output_size, &used,
                             "drive_%u_status=error\n"
                             "drive_%u_win32_error=%lu\n",
                             count, count, (unsigned long) error)) {
                return 0;
            }
        }

        ++count;
    }

    return append_text(output, output_size, &used, "count=%u\n", count);
}

// Имя одного установленного бита маски для файла или папки
static const char *file_right_name(unsigned int bit, int is_directory)
{
    switch (bit) {
        case 0:  return is_directory ? "FILE_LIST_DIRECTORY" : "FILE_READ_DATA";
        case 1:  return is_directory ? "FILE_ADD_FILE" : "FILE_WRITE_DATA";
        case 2:  return is_directory ? "FILE_ADD_SUBDIRECTORY" : "FILE_APPEND_DATA";
        case 3:  return "FILE_READ_EA";
        case 4:  return "FILE_WRITE_EA";
        case 5:  return is_directory ? "FILE_TRAVERSE" : "FILE_EXECUTE";
        case 6:  return "FILE_DELETE_CHILD";
        case 7:  return "FILE_READ_ATTRIBUTES";
        case 8:  return "FILE_WRITE_ATTRIBUTES";
        case 16: return "DELETE";
        case 17: return "READ_CONTROL";
        case 18: return "WRITE_DAC";
        case 19: return "WRITE_OWNER";
        case 20: return "SYNCHRONIZE";
        case 24: return "ACCESS_SYSTEM_SECURITY";
        case 25: return "MAXIMUM_ALLOWED";
        case 28: return "GENERIC_ALL";
        case 29: return "GENERIC_EXECUTE";
        case 30: return "GENERIC_WRITE";
        case 31: return "GENERIC_READ";
        default: return "UNKNOWN_OR_RESERVED";
    }
}
// Для ключа реестра младшие биты маски имеют другие названия
static const char *registry_right_name(unsigned int bit)
{
    switch (bit) {
        case 0:  return "KEY_QUERY_VALUE";
        case 1:  return "KEY_SET_VALUE";
        case 2:  return "KEY_CREATE_SUB_KEY";
        case 3:  return "KEY_ENUMERATE_SUB_KEYS";
        case 4:  return "KEY_NOTIFY";
        case 5:  return "KEY_CREATE_LINK";
        case 16: return "DELETE";
        case 17: return "READ_CONTROL";
        case 18: return "WRITE_DAC";
        case 19: return "WRITE_OWNER";
        case 24: return "ACCESS_SYSTEM_SECURITY";
        case 25: return "MAXIMUM_ALLOWED";
        case 28: return "GENERIC_ALL";
        case 29: return "GENERIC_EXECUTE";
        case 30: return "GENERIC_WRITE";
        case 31: return "GENERIC_READ";
        default: return "UNKNOWN_OR_RESERVED";
    }
}
// Читает одну ACE и дописывает её поля в ответ
static int append_acl_ace(PACL dacl, DWORD index,
                          SE_OBJECT_TYPE object_type, int is_directory,
                          char *output, size_t output_size,
                          size_t *used)
{
    void *raw_ace = NULL;
    ACE_HEADER *header;
    PSID sid;
    DWORD mask;
    const char *type;
    LPSTR sid_text = NULL;
    WCHAR name_w[256];
    WCHAR domain_w[256];
    char name[768] = "unresolved";
    char domain[768] = "";
    DWORD name_size = 256;
    DWORD domain_size = 256;
    SID_NAME_USE sid_use;
    int success;
    unsigned int bit;
    if (!GetAce(dacl, index, &raw_ace)) {
        return 0;
    }

    header = (ACE_HEADER *)raw_ace;

    if (header->AceType == ACCESS_ALLOWED_ACE_TYPE) {
        ACCESS_ALLOWED_ACE *ace = (ACCESS_ALLOWED_ACE *)raw_ace;
        if (header->AceSize < sizeof(*ace)) {
            return 0;
        }
        type = "allow";
        mask = ace->Mask;
        sid = (PSID)&ace->SidStart;
    } else if (header->AceType == ACCESS_DENIED_ACE_TYPE) {
        ACCESS_DENIED_ACE *ace = (ACCESS_DENIED_ACE *)raw_ace;
        if (header->AceSize < sizeof(*ace)) {
            return 0;
        }
        type = "deny";
        mask = ace->Mask;
        sid = (PSID)&ace->SidStart;
    } else {
        // У других типов ACE может быть другая структура:
        // не притворяемся, что знаем, где лежит их SID
        return append_text(output, output_size, used,
                           "ace_%lu_type=unsupported_%u\n"
                           "ace_%lu_flags=0x%02X\n",
                           (unsigned long)index,
                           (unsigned int)header->AceType,
                           (unsigned long)index,
                           (unsigned int)header->AceFlags);
    }

    if (!IsValidSid(sid) ||
        !ConvertSidToStringSidA(sid, &sid_text)) {
        return 0;
    }

    if (LookupAccountSidW(NULL, sid,
                          name_w, &name_size,
                          domain_w, &domain_size,
                          &sid_use)) {
        if (WideCharToMultiByte(CP_UTF8, 0, name_w, -1,
                                name, (int)sizeof(name), NULL, NULL) == 0 ||
            WideCharToMultiByte(CP_UTF8, 0, domain_w, -1,
                                domain, (int)sizeof(domain), NULL, NULL) == 0) {
            strcpy(name, "unresolved");
            domain[0] = '\0';
        }
    }

    success = append_text(output, output_size, used,
                          "ace_%lu_type=%s\n"
                          "ace_%lu_sid=%s\n"
                          "ace_%lu_domain=%s\n"
                          "ace_%lu_name=%s\n"
                          "ace_%lu_flags=0x%02X\n"
                          "ace_%lu_mask=0x%08lX\n",
                          (unsigned long)index, type,
                          (unsigned long)index, sid_text,
                          (unsigned long)index, domain,
                          (unsigned long)index, name,
                          (unsigned long)index,
                          (unsigned int)header->AceFlags,
                          (unsigned long)index,
                          (unsigned long)mask);

    LocalFree(sid_text);
    if (!success) {
        return 0;
    }

    if (object_type == SE_REGISTRY_KEY) {
        success = append_text(output, output_size, used,
                              "ace_%lu_scope_self=%s\n"
                              "ace_%lu_scope_child_keys=%s\n"
                              "ace_%lu_inherited=%s\n"
                              "ace_%lu_no_propagate=%s\n",
                              (unsigned long)index,
                              (header->AceFlags & INHERIT_ONLY_ACE) ? "no" : "yes",
                              (unsigned long)index,
                              (header->AceFlags & CONTAINER_INHERIT_ACE) ? "yes" : "no",
                              (unsigned long)index,
                              (header->AceFlags & INHERITED_ACE) ? "yes" : "no",
                              (unsigned long)index,
                              (header->AceFlags & NO_PROPAGATE_INHERIT_ACE) ? "yes" : "no");
    } else {
        success = append_text(output, output_size, used,
                              "ace_%lu_scope_self=%s\n"
                              "ace_%lu_scope_child_files=%s\n"
                              "ace_%lu_scope_child_directories=%s\n"
                              "ace_%lu_inherited=%s\n"
                              "ace_%lu_no_propagate=%s\n",
                              (unsigned long)index,
                              (header->AceFlags & INHERIT_ONLY_ACE) ? "no" : "yes",
                              (unsigned long)index,
                              (header->AceFlags & OBJECT_INHERIT_ACE) ? "yes" : "no",
                              (unsigned long)index,
                              (header->AceFlags & CONTAINER_INHERIT_ACE) ? "yes" : "no",
                              (unsigned long)index,
                              (header->AceFlags & INHERITED_ACE) ? "yes" : "no",
                              (unsigned long)index,
                              (header->AceFlags & NO_PROPAGATE_INHERIT_ACE) ? "yes" : "no");
    }

    for (bit = 0; success && bit < 32; ++bit) {
        if ((mask & ((DWORD)1u << bit)) != 0) {
            success = append_text(output, output_size, used,
                                  "ace_%lu_bit_%u=%s\n",
                                  (unsigned long)index, bit,
                                  object_type == SE_REGISTRY_KEY
                                  ? registry_right_name(bit)
                                  : file_right_name(bit, is_directory));
        }
    }

    return success;
}

// Возвращает имя локального ключа без нашего префикса "reg:"
// NULL означает, что формат ключа нам не подходит
static const char *registry_object_name(const char *path)
{
    const char *name = path + 4; // пропускаем "reg:"

    if (strncmp(name, "CURRENT_USER\\", 13) == 0 ||
        strncmp(name, "MACHINE\\", 8) == 0 ||
        strncmp(name, "USERS\\", 6) == 0 ||
        strncmp(name, "CLASSES_ROOT\\", 13) == 0) {
        return name;
        }

    return NULL;
}

// GET_ACL: получаем DACL, разбираем ACE и формируем текстовый ответ
static int build_acl(const uint8_t *path_bytes,
                     size_t path_len,
                     char *output,
                     size_t output_size)
{
    char path[512];
    PACL dacl = NULL;
    PSECURITY_DESCRIPTOR descriptor = NULL;
    ACL_SIZE_INFORMATION info;
    DWORD error;
    size_t used = 0;
    int success;
    DWORD attributes;
    int is_directory = 0;

    SE_OBJECT_TYPE object_type = SE_FILE_OBJECT;
    const char *object_name;


    if (path_bytes == NULL || path_len == 0 ||
        path_len >= sizeof(path) ||
        memchr(path_bytes, '\0', path_len) != NULL) {
        return append_text(output, output_size, &used,
                           "status=error\ncode=invalid_path\n");
    }

    memcpy(path, path_bytes, path_len);
    path[path_len] = '\0';

    object_name = path;

    if (strncmp(path, "reg:", 4) == 0) {
        object_name = registry_object_name(path);
        if (object_name == NULL) {
            return append_text(output, output_size, &used,
                               "status=error\ncode=invalid_registry_path\n");
        }
        object_type = SE_REGISTRY_KEY;
    } else if (path_len < 3 || path[1] != ':' || path[2] != '\\') {
        return append_text(output, output_size, &used,
                           "status=error\ncode=path_must_be_absolute\n");
    }

    error = GetNamedSecurityInfoA(object_name, object_type,
                                  DACL_SECURITY_INFORMATION,
                                  NULL, NULL, &dacl, NULL,
                                  &descriptor);
    if (error != ERROR_SUCCESS) {
        if (descriptor != NULL) {
            LocalFree(descriptor);
        }
        return append_text(output, output_size, &used,
                           "status=error\ncode=security_info_failed\n"
                           "win32_error=%lu\n",
                           (unsigned long)error);
    }
    if (object_type == SE_FILE_OBJECT) {
        attributes = GetFileAttributesA(path);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            error = GetLastError();
            LocalFree(descriptor);
            return append_text(output, output_size, &used,
                               "status=error\ncode=file_attributes_failed\n"
                               "win32_error=%lu\n",
                               (unsigned long)error);
        }
        is_directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    if (dacl == NULL) {
        success = append_text(output, output_size, &used,
                              "status=ok\ndacl_state=null\nace_count=0\n");
    } else if (!GetAclInformation(dacl, &info, (DWORD)sizeof(info),
                                  AclSizeInformation)) {
        error = GetLastError();
        success = append_text(output, output_size, &used,
                              "status=error\ncode=acl_info_failed\n"
                              "win32_error=%lu\n",
                              (unsigned long)error);
    } else {
        DWORD i;

        success = append_text(output, output_size, &used,
                              "status=ok\ndacl_state=present\n"
                              "ace_count=%lu\n",
                              (unsigned long)info.AceCount);

        for (i = 0; success && i < info.AceCount; ++i) {
            success = append_acl_ace(dacl, i, object_type, is_directory,
                                     output, output_size, &used);
        }

        if (!success) {
            used = 0;
            success = append_text(output, output_size, &used,
                                  "status=error\ncode=ace_decode_failed\n");
        }
    }
    LocalFree(descriptor);
    return success;
}
// Получить владельца файла, папки или ключа реестра по пути из запроса
static int build_owner(const uint8_t *path_bytes,
                       size_t path_len,
                       char *output,
                       size_t output_size)
{
    char path[512];
    PSID owner_sid = NULL;
    PSECURITY_DESCRIPTOR descriptor = NULL;
    LPSTR sid_text = NULL;
    WCHAR name_w[256];
    WCHAR domain_w[256];
    char name[768] = "unresolved";
    char domain[768] = "";
    DWORD name_size = (DWORD)(sizeof(name_w) / sizeof(name_w[0]));
    DWORD domain_size = (DWORD)(sizeof(domain_w) / sizeof(domain_w[0]));
    SID_NAME_USE sid_use;
    DWORD error;
    size_t used = 0;
    int success;
    SE_OBJECT_TYPE object_type = SE_FILE_OBJECT;
    const char *object_name;


    // Не доверяем длине и содержимому, пришедшим по сети
    if (path_bytes == NULL || path_len == 0 ||
        path_len >= sizeof(path) ||
        memchr(path_bytes, '\0', path_len) != NULL) {
        return append_text(output, output_size, &used,
                           "status=error\ncode=invalid_path\n");
    }

    memcpy(path, path_bytes, path_len);
    path[path_len] = '\0';

    // Принимаем абсолютный путь с буквой диска или ключ с префиксом reg:
    object_name = path;

    if (strncmp(path, "reg:", 4) == 0) {
        object_name = registry_object_name(path);
        if (object_name == NULL) {
            return append_text(output, output_size, &used,
                               "status=error\ncode=invalid_registry_path\n");
        }
        object_type = SE_REGISTRY_KEY;
    } else if (path_len < 3 || path[1] != ':' || path[2] != '\\') {
        return append_text(output, output_size, &used,
                           "status=error\ncode=path_must_be_absolute\n");
    }

    error = GetNamedSecurityInfoA(object_name, object_type,
                                  OWNER_SECURITY_INFORMATION,
                                  &owner_sid, NULL, NULL, NULL,
                                  &descriptor);
    if (error != ERROR_SUCCESS) {
        if (descriptor != NULL) {
            LocalFree(descriptor);
        }
        return append_text(output, output_size, &used,
                           "status=error\ncode=security_info_failed\n"
                           "win32_error=%lu\n",
                           (unsigned long)error);
    }

    if (owner_sid == NULL) {
        LocalFree(descriptor);
        return append_text(output, output_size, &used,
                           "status=error\ncode=owner_missing\n");
    }

    if (!ConvertSidToStringSidA(owner_sid, &sid_text)) {
        error = GetLastError();
        LocalFree(descriptor);
        return append_text(output, output_size, &used,
                           "status=error\ncode=sid_conversion_failed\n"
                           "win32_error=%lu\n",
                           (unsigned long)error);
    }

    // Получаем Unicode-имя и кодируем его в UTF-8 для сетевого ответа
    // Если имя не найдено, сам SID всё равно остаётся полезным
    if (LookupAccountSidW(NULL, owner_sid,
                          name_w, &name_size,
                          domain_w, &domain_size,
                          &sid_use)) {
        if (WideCharToMultiByte(CP_UTF8, 0, name_w, -1,
                                name, (int)sizeof(name), NULL, NULL) == 0 ||
            WideCharToMultiByte(CP_UTF8, 0, domain_w, -1,
                                domain, (int)sizeof(domain), NULL, NULL) == 0) {
            strcpy(name, "unresolved");
            domain[0] = '\0';
        }
    }

    success = append_text(output, output_size, &used,
                          "status=ok\nowner_sid=%s\n"
                          "owner_domain=%s\nowner_name=%s\n",
                          sid_text, domain, name);

    LocalFree(sid_text);
    LocalFree(descriptor);
    return success;
}
// Диспетчер: номер запроса выбирает функцию сбора информации
// Запросы 7 и 8 передают путь в теле, остальные запросы не требуют пути
int lab_build_system_response(uint16_t request_type,
                              const uint8_t *request_payload,
                              size_t request_payload_len,
                              char *output,
                              size_t output_size,
                              int *is_error) {
    int success = 0;

    if (output == NULL || output_size == 0 || is_error == NULL) {
        return 0;
    }

    output[0] = '\0';
    *is_error = 0;

    switch (request_type) {
        case LAB_GET_OS:
            success = build_os(output, output_size);
            break;
        case LAB_GET_TIME:
            success = build_time(output, output_size);
            break;
        case LAB_GET_UPTIME:
            success = build_uptime(output, output_size);
            break;
        case LAB_GET_MEMORY:
            success = build_memory(output, output_size);
            break;
        case LAB_GET_DRIVES:
            success = build_drives(output, output_size);
            break;
        case LAB_GET_DISK_SPACE:
            success = build_disk_space(output, output_size);
            break;
        case LAB_GET_OWNER:
            success = build_owner(request_payload,
                                  request_payload_len,
                                  output,
                                  output_size);
            break;
        case LAB_GET_ACL:
            success = build_acl(request_payload,
                                request_payload_len,
                                output,
                                output_size);
            break;
        default:
            *is_error = 1;
            success = snprintf(output, output_size,
                               "status=error\ncode=unknown_request\ntype=%u\n",
                               request_type) > 0;
            break;
    }

    if (!success) {
        *is_error = 1;
        if (output_size > 0) {
            snprintf(output, output_size,
                     "status=error\ncode=response_buffer_too_small\n");
        }
    } else if (strncmp(output, "status=error", 12) == 0) {
        *is_error = 1;
    }

    return success;
}
