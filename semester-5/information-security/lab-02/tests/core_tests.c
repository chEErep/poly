#include "app.h"
#include <locale.h>

static unsigned failures;

// Учитывает результат проверки, чтобы CTest получил ненулевой код при ошибке.
static void check(BOOL condition, const wchar_t *description)
{
    if (!condition) {
        ++failures;
        wprintf(L"FAIL: %ls\n", description);
    }
}

// Проверяет маски, включая возврат к звёздочке и регистр букв.
static void test_masks(void)
{
    const struct { const wchar_t *name, *mask; BOOL expected; } cases[] = {
        {L"notes.txt", L"*.txt", TRUE}, {L"notes.TXT", L"*.txt", TRUE},
        {L"photo.png", L"*.txt", FALSE}, {L"report_1.docx", L"report_?.docx", TRUE},
        {L"report_12.docx", L"report_?.docx", FALSE}, {L"report_.docx", L"report_?.docx", FALSE},
        {L"demo.exe", L"demo.exe", TRUE}, {L"other.exe", L"demo.exe", FALSE},
        {L"abbbc", L"a*bc", TRUE}, {L"abc", L"a**?c", TRUE},
        {L"", L"*", TRUE}, {L"", L"?", FALSE},
        {L"заметка.txt", L"*.txt", TRUE}, {L"file", L"*.*", FALSE}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        check(mask_matches(cases[i].name, cases[i].mask) == cases[i].expected, cases[i].mask);
}

// Проверяет конфигурацию без доступа к сети: здесь проверяются только настройки.
static void test_config(void)
{
    wchar_t directory[PATH_CAP], path[PATH_CAP], error[ERROR_CAP];
    if (!GetTempPathW(PATH_CAP, directory) || !GetTempFileNameW(directory, L"cfg", 0, path)) {
        check(FALSE, L"Create config fixture");
        return;
    }
    const struct { const wchar_t *text; BOOL expected; } cases[] = {
        {L"# comment\nsource=\\\\localhost\\C$\\исходники\narchive=\\\\localhost\\C$\\backup\\b.zip\nmask=*.txt\nmask=report_?.docx", TRUE},
        {L"source=C:\\source\narchive=\\\\host\\share\\b.zip\nmask=*\n", FALSE},
        {L"source=\\\\host\\share\\source\narchive=\\\\host\\share\\source\\b.zip\nmask=*\n", FALSE},
        {L"source=\\\\host\\share\\source\narchive=\\\\host\\share\\source2\\b.zip\nmask=*\n", TRUE},
        {L"source=\\\\host\\share\narchive=\\\\host\\other\\b.zip\n", FALSE},
        {L"source=\\\\host\\share\nsource=\\\\host\\other\narchive=\\\\host\\other\\b.zip\nmask=*\n", FALSE},
        {L"source=\\\\host\\share\narchive=\\\\host\\other\\b.zip\nmask=folder\\*.txt\n", FALSE},
        {L"source=\\\\host\\share\narchive=\\\\host\\other\\b.rar\nmask=*\n", FALSE},
        {L"source=\\\\host\\share\narchive=\\\\host\\other\\b.zip\nunknown=value\nmask=*\n", FALSE},
        {L"source=\\\\?\\C:\\source\narchive=\\\\host\\other\\b.zip\nmask=*\n", FALSE}
        ,{L"source=\\\\host\\share\narchive=\\\\host\\other\\b.zip\nmask=*\ninterval_seconds=1\n", TRUE}
        ,{L"source=\\\\host\\share\narchive=\\\\host\\other\\b.zip\nmask=*\ninterval_seconds=0\n", FALSE}
        ,{L"source=\\\\host\\share\narchive=\\\\host\\other\\b.zip\nmask=*\ninterval_seconds=3601\n", FALSE}
        ,{L"source=\\\\host\\share\narchive=\\\\host\\other\\b.zip\nmask=*\ninterval_seconds=2x\n", FALSE}
        ,{L"source=\\\\host\\share\narchive=\\\\host\\other\\b.zip\nmask=*\ninterval_seconds=2\ninterval_seconds=3\n", FALSE}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        FILE *file = _wfopen(path, L"wt, ccs=UTF-8");
        if (!file) {
            check(FALSE, L"Write config fixture");
            break;
        }
        fputws(cases[i].text, file);
        fclose(file);
        BackupConfig config;
        check(load_config(path, &config, error) == cases[i].expected, L"Config validation");
    }
    DeleteFileW(path);
}

// Запускает независимые проверки масок и разбора конфигурации.
int wmain(void)
{
    setlocale(LC_ALL, "");
    test_masks();
    test_config();
    wprintf(L"Core tests: %u failures.\n", failures);
    return failures ? 1 : 0;
}
