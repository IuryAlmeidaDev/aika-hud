#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "combat.h"
#include "ui.h"

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED 0x00080000
#endif

#ifndef LWA_ALPHA
#define LWA_ALPHA 0x00000002
#endif

typedef struct {
    DWORD TokenIsElevated;
} TOKEN_ELEVATION;

typedef BOOL (WINAPI *pfn_OpenProcessToken)(HANDLE, DWORD, PHANDLE);
typedef BOOL (WINAPI *pfn_GetTokenInformation)(HANDLE, int, LPVOID, DWORD, PDWORD);
typedef HINSTANCE (WINAPI *pfn_ShellExecuteW)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
typedef BOOL (WINAPI *pfn_SetLayeredWindowAttributes)(HWND, COLORREF, BYTE, DWORD);

HWND g_hMainWnd = NULL;
static pfn_SetLayeredWindowAttributes fn_SetLayeredWindowAttributes = NULL;

static void log_debug(const char *msg) {
    FILE *f = fopen("C:\\Users\\iuryc\\aika-hud\\debug.log", "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d.%03d] [main] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
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
            g_hMainWnd = hWnd;

            HMODULE hUser32 = GetModuleHandleA("user32.dll");
            if (hUser32) {
                fn_SetLayeredWindowAttributes = (pfn_SetLayeredWindowAttributes)GetProcAddress(hUser32, "SetLayeredWindowAttributes");
                if (fn_SetLayeredWindowAttributes) {
                    fn_SetLayeredWindowAttributes(hWnd, 0, (BYTE)(255 * 0.94f), LWA_ALPHA);
                }
            }

            ui_init();
            combat_init();
            SetTimer(hWnd, 1, 50, NULL);
            log_debug("WM_CREATE finalizado com sucesso");
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            ui_handle_click(hWnd, x, y);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                // Mantem overlay sempre no topo de jogos DirectX Fullscreen/Borderless
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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

        case WM_CLOSE: {
            log_debug("WndProc: WM_CLOSE recebido");
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            log_debug("WndProc: WM_DESTROY recebido");
            KillTimer(hWnd, 1);
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

    if (IsProcessElevated()) {
        log_debug("Executando com privilegios de Administrador [OK]");
    } else {
        log_debug("Aviso: Executando sem elevacao de Administrador");
    }

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
        log_debug("RegisterClassExW falhou!");
        return 1;
    }

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int posX = (sw - HUD_WIDTH) / 2;
    int posY = 10; // Barra centralizada no topo da tela

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        L"GhostHUD_CombatOverlay",
        L"GhostHUD Combat Assistant",
        WS_POPUP,
        posX, posY, HUD_WIDTH, HUD_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    char dbgHwnd[128];
    snprintf(dbgHwnd, sizeof(dbgHwnd), "CreateWindowExW retornou: %p, LastError: %lu", hWnd, GetLastError());
    log_debug(dbgHwnd);

    if (!hWnd) {
        log_debug("CreateWindowExW falhou!");
        return 1;
    }

    g_hMainWnd = hWnd;
    log_debug("Chamando ShowWindow...");
    ShowWindow(hWnd, SW_SHOWNORMAL);
    log_debug("ShowWindow retornou!");

    log_debug("Chamando UpdateWindow...");
    UpdateWindow(hWnd);
    log_debug("UpdateWindow retornou!");

    log_debug("Entrando no message loop");

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    log_debug("Message loop finalizado");
    return (int)msg.wParam;
}

int main(int argc, char **argv) {
    return WinMain(GetModuleHandleW(NULL), NULL, GetCommandLineA(), SW_SHOW);
}
