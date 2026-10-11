#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "combat.h"
#include "ui.h"

#define ID_TIMER_REFRESH  1
#define ID_HOTKEY_F8      101
#define ID_HOTKEY_F7      102
#define ID_HOTKEY_F9      103
#define ID_HOTKEY_F10     104
#define ID_HOTKEY_F12     105

#ifndef TOKEN_QUERY
#define TOKEN_QUERY 0x0008
#endif

typedef struct _TOKEN_ELEVATION {
    DWORD TokenIsElevated;
} TOKEN_ELEVATION;

typedef BOOL (WINAPI *pfn_OpenProcessToken)(HANDLE, DWORD, PHANDLE);
typedef BOOL (WINAPI *pfn_GetTokenInformation)(HANDLE, int, LPVOID, DWORD, PDWORD);
typedef HINSTANCE (WINAPI *pfn_ShellExecuteW)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);

static BOOL IsProcessElevated(void) {
    BOOL fIsElevated = FALSE;
    HMODULE hAdvapi = LoadLibraryA("advapi32.dll");
    if (!hAdvapi) return FALSE;

    pfn_OpenProcessToken pOpenProcessToken = (pfn_OpenProcessToken)GetProcAddress(hAdvapi, "OpenProcessToken");
    pfn_GetTokenInformation pGetTokenInformation = (pfn_GetTokenInformation)GetProcAddress(hAdvapi, "GetTokenInformation");

    if (pOpenProcessToken && pGetTokenInformation) {
        HANDLE hToken = NULL;
        if (pOpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            TOKEN_ELEVATION elevation;
            DWORD cbSize = sizeof(TOKEN_ELEVATION);
            if (pGetTokenInformation(hToken, 20, &elevation, sizeof(elevation), &cbSize)) {
                fIsElevated = (BOOL)elevation.TokenIsElevated;
            }
            CloseHandle(hToken);
        }
    }
    FreeLibrary(hAdvapi);
    return fIsElevated;
}

static void RelaunchAsAdmin(void) {
    if (!IsProcessElevated()) {
        HMODULE hShell = LoadLibraryA("shell32.dll");
        if (hShell) {
            pfn_ShellExecuteW pShellExecuteW = (pfn_ShellExecuteW)GetProcAddress(hShell, "ShellExecuteW");
            if (pShellExecuteW) {
                WCHAR szPath[MAX_PATH];
                GetModuleFileNameW(NULL, szPath, MAX_PATH);
                pShellExecuteW(NULL, L"runas", szPath, NULL, NULL, SW_SHOWNORMAL);
                ExitProcess(0);
            }
            FreeLibrary(hShell);
        }
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            ui_init();
            combat_init();

            // Registra atalhos globais
            RegisterHotKey(hWnd, ID_HOTKEY_F8,  0, VK_F8);
            RegisterHotKey(hWnd, ID_HOTKEY_F7,  0, VK_F7);
            RegisterHotKey(hWnd, ID_HOTKEY_F9,  0, VK_F9);
            RegisterHotKey(hWnd, ID_HOTKEY_F10, 0, VK_F10);
            RegisterHotKey(hWnd, ID_HOTKEY_F12, 0, VK_F12);

            // Timer para atualização suave de tela (10 FPS com 0.0% de CPU)
            SetTimer(hWnd, ID_TIMER_REFRESH, 100, NULL);
            return 0;
        }

        case WM_HOTKEY: {
            switch (wParam) {
                case ID_HOTKEY_F8:
                    combat_toggle_macro();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case ID_HOTKEY_F7:
                    combat_toggle_heal();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case ID_HOTKEY_F9:
                    combat_toggle_target();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case ID_HOTKEY_F10:
                    combat_toggle_loot();
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                case ID_HOTKEY_F12:
                    DestroyWindow(hWnd);
                    break;
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            ui_handle_click(hWnd, x, y);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == ID_TIMER_REFRESH) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            ui_render(hdc, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            KillTimer(hWnd, ID_TIMER_REFRESH);
            UnregisterHotKey(hWnd, ID_HOTKEY_F8);
            UnregisterHotKey(hWnd, ID_HOTKEY_F7);
            UnregisterHotKey(hWnd, ID_HOTKEY_F9);
            UnregisterHotKey(hWnd, ID_HOTKEY_F10);
            UnregisterHotKey(hWnd, ID_HOTKEY_F12);

            combat_cleanup();
            ui_cleanup();
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"AikaHUDWindowClass";

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    // Posiciona no canto superior direito da tela por padrão
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int pos_x = screen_w - HUD_WIDTH - 24;
    int pos_y = 60;

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, // Sempre no topo e janela de ferramenta
        L"AikaHUDWindowClass",
        L"Aika Combat HUD",
        WS_POPUP | WS_VISIBLE,           // Janela personalizada sem bordas genéricas
        pos_x, pos_y, HUD_WIDTH, HUD_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        return 1;
    }

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

int main(int argc, char **argv) {
    return WinMain(GetModuleHandleW(NULL), NULL, GetCommandLineA(), SW_SHOW);
}
