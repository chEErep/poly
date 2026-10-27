#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Безопасная тестовая программа: показывает окно и ничего не изменяет.
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    (void)instance;
    (void)previous;
    (void)command;
    (void)show;
    MessageBoxW(NULL, L"The EXE from the backup works.", L"BSIT Lab 2 demo", MB_OK);
    return 0;
}
