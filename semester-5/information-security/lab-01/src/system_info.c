#include "system_info.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winternl.h>

#include "protocol.h"

typedef LONG (WINAPI *rtl_get_version_fn)(PRTL_OSVERSIONINFOW);

static int append_text(char *output,
                       size_t output_size,
                       size_t *used,
                       const char *format,
                       ...)
{
    int written;
    va_list arguments;

    if (*used >= output_size) {
        return 0;
    }

    va_start(arguments, format);
    written = vsnprintf(output + *used, output_size - *used, format, arguments);
    va_end(arguments);

    if (written < 0 || (size_t)written >= output_size - *used) {
        return 0;
    }

    *used += (size_t)written;
    return 1;
}

static int build_os(char *output, size_t output_size)
{
    HMODULE ntdll;
    FARPROC function_address;
    rtl_get_version_fn get_version;
    RTL_OSVERSIONINFOW version;
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
    if (get_version(&version) != 0) {
        return snprintf(output, output_size,
                        "status=error\ncode=rtl_get_version_failed\n") > 0;
    }

    if (version.dwMajorVersion == 10 && version.dwBuildNumber >= 22000) {
        name = "Windows 11";
    } else if (version.dwMajorVersion == 10) {
        name = "Windows 10 or Windows Server";
    } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 3) {
        name = "Windows 8.1 or Windows Server 2012 R2";
    } else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 1) {
        name = "Windows 7 or Windows Server 2008 R2";
    }

    return append_text(output, output_size, &used, "status=ok\n")
        && append_text(output, output_size, &used, "name=%s\n", name)
        && append_text(output, output_size, &used, "major=%lu\n",
                       (unsigned long)version.dwMajorVersion)
        && append_text(output, output_size, &used, "minor=%lu\n",
                       (unsigned long)version.dwMinorVersion)
        && append_text(output, output_size, &used, "build=%lu\n",
                       (unsigned long)version.dwBuildNumber);
}

static int build_time(char *output, size_t output_size)
{
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

static int build_uptime(char *output, size_t output_size)
{
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
                       (unsigned long long)milliseconds)
        && append_text(output, output_size, &used,
                       "total_seconds=%llu\n",
                       (unsigned long long)total_seconds)
        && append_text(output, output_size, &used,
                       "days=%llu\nhours=%llu\nminutes=%llu\nseconds=%llu\n",
                       (unsigned long long)days,
                       (unsigned long long)hours,
                       (unsigned long long)minutes,
                       (unsigned long long)seconds);
}

static int build_memory(char *output, size_t output_size)
{
    MEMORYSTATUSEX memory;
    size_t used = 0;

    ZeroMemory(&memory, sizeof(memory));
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory)) {
        return snprintf(output, output_size,
                        "status=error\ncode=memory_query_failed\nwin32_error=%lu\n",
                        (unsigned long)GetLastError()) > 0;
    }

    return append_text(output, output_size, &used, "status=ok\n")
        && append_text(output, output_size, &used, "load_percent=%lu\n",
                       (unsigned long)memory.dwMemoryLoad)
        && append_text(output, output_size, &used, "total_physical_bytes=%llu\n",
                       (unsigned long long)memory.ullTotalPhys)
        && append_text(output, output_size, &used, "available_physical_bytes=%llu\n",
                       (unsigned long long)memory.ullAvailPhys)
        && append_text(output, output_size, &used, "total_page_file_bytes=%llu\n",
                       (unsigned long long)memory.ullTotalPageFile)
        && append_text(output, output_size, &used, "available_page_file_bytes=%llu\n",
                       (unsigned long long)memory.ullAvailPageFile)
        && append_text(output, output_size, &used, "total_virtual_bytes=%llu\n",
                       (unsigned long long)memory.ullTotalVirtual)
        && append_text(output, output_size, &used, "available_virtual_bytes=%llu\n",
                       (unsigned long long)memory.ullAvailVirtual);
}

int lab_build_system_response(uint16_t request_type,
                              char *output,
                              size_t output_size,
                              int *is_error)
{
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
