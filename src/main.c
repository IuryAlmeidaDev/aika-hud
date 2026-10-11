#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "combat.h"
#include "ui.h"

#define ID_TIMER_REFRESH  1

#ifndef TOKEN_QUERY
#define TOKEN_QUERY 0x0008
#endif

typedef struct _TOKEN_ELEVATION {
    DWORD TokenIsElevated;
} TOKEN_ELEVATION;

typedef BOOL (WINAPI *pfn_OpenProcessToken)(HANDLE, DWORD, PHANDLE);
typedef BOOL (WINAPI *pfn_GetTokenInformation)(HANDLE, int, LPVOID, DWORD, PDWORD);
typedef HINSTANCE (WINAPI *pfn_ShellExecuteW)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);

HWND g_hMainWnd = NULL;

static void log_debug(const char *msg) {
    FILE *f = fopen("C:\\Users\\iuryc\\aika-hud\\debug.log", "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
        fclose(f);
    }
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS *pExceptionInfo) {
    char buf[128];
    snprintf(buf, sizeof(buf), "CRASH! Code: 0x%08X at %p",
             (unsigned int)pExceptionInfo->ExceptionRecord->ExceptionCode,
             pExceptionInfo->ExceptionRecord->ExceptionAddress);
    log_debug(buf);
    return EXCEPTION_EXECUTE_HANDLER;
}

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
        log_debug("RelaunchAsAdmin: Solicitando elevacao UAC...");
        HMODULE hShell = LoadLibraryA("shell32.dll");
        if (hShell) {
            pfn_ShellExecuteW pShellExecuteW = (pfn_ShellExecuteW)GetProcAddress(hShell, "ShellExecuteW");
            if (pShellExecuteW) {
                WCHAR szPath[MAX_PATH];
                GetModuleFileNameW(NULL, szPath, MAX_PATH);
                HINSTANCE hInst = pShellExecuteW(NULL, L"runas", szPath, NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)hInst > 32) {
                    log_debug("Relaunch bem sucedido. Fechando processo nao-elevado.");
                    ExitProcess(0);
                } else {
                    log_debug("Usuario cancelou solicitacao UAC.");
                }
            }
            FreeLibrary(hShell);
        }
    } else {
        log_debug("Processo ja esta elevado como Administrador.");
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            log_debug("WndProc: WM_CREATE recebido");
            ui_init();
            combat_init();
            SetTimer(hWnd, ID_TIMER_REFRESH, 100, NULL);
            log_debug("WM_CREATE finalizado");
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
            log_debug("WndProc: WM_PAINT recebido - chamando ui_render");
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            ui_render(hdc, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hWnd, &ps);
            log_debug("WndProc: WM_PAINT finalizado");
            return 0;
        }

        case WM_CLOSE: {
            log_debug("WndProc: WM_CLOSE recebido");
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            log_debug("WndProc: WM_DESTROY recebido");
            KillTimer(hWnd, ID_TIMER_REFRESH);
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
    SetUnhandledExceptionFilter(CrashHandler);
    log_debug("WinMain: Iniciado");

    HDESK hDesk = OpenDesktopW(L"Default", 0, FALSE, GENERIC_ALL);
    if (hDesk) {
        SetThreadDesktop(hDesk);
        log_debug("Conectado com sucesso ao desktop Default!");
    }

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"GhostHUD_CombatOverlay";

    if (!RegisterClassExW(&wc)) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "RegisterClassExW falhou! Error=%lu", GetLastError());
        log_debug(errBuf);
        return 1;
    }
    log_debug("RegisterClassExW bem sucedido");

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int pos_x = screen_w - HUD_WIDTH - 24;
    int pos_y = 60;

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"GhostHUD_CombatOverlay",
        L"GhostHUD Combat Assistant",
        WS_POPUP,
        pos_x, pos_y, HUD_WIDTH, HUD_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        char errBuf[128];
        snprintf(errBuf, sizeof(errBuf), "CreateWindowExW falhou! Error=%lu", GetLastError());
        log_debug(errBuf);
        return 1;
    }

    g_hMainWnd = hWnd;

    char hwndBuf[128];
    snprintf(hwndBuf, sizeof(hwndBuf), "Janela criada! HWND=0x%p, Pos=(%d,%d)", hWnd, pos_x, pos_y);
    log_debug(hwndBuf);

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    log_debug("Entrando no message loop");

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    log_debug("Message loop finalizado com exito");
    return (int)msg.wParam;
}

int main(int argc, char **argv) {
    return WinMain(GetModuleHandleW(NULL), NULL, GetCommandLineA(), SW_SHOW);
}
