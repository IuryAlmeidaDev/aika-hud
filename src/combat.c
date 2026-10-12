#define UNICODE
#define _UNICODE
#include "combat.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static CombatState g_state = {
    .macro_active = FALSE,
    .auto_target = TRUE,
    .auto_loot = TRUE,
    .auto_heal = TRUE,
    .running = TRUE,
    .target_locked = FALSE,
    .is_foreground = FALSE,
    .detected_game = GAME_NONE,
    .mobs_killed = 0,
    .session_start_time = 0,
    .last_action = "Pronto! Pressione [F8] ou clique em ATIVAR",
    .target_hwnd = NULL
};

static CombatConfig g_config = {
    // Aion 2
    .key_aion_heal = 'Q',
    .key_aion_skill_e = 'E',
    .key_aion_loot = 'F',
    .cd_aion_skill_e = 2000,
    .cd_aion_rmb = 1400,
    .cd_aion_heal = 3500,

    // Aika
    .key_aika_basic = '5',
    .key_aika_skill1 = '1',
    .key_aika_skill2 = '2',
    .key_aika_skill3 = '3',
    .key_aika_heal = '4',
    .key_aika_loot = VK_SPACE,
    .cd_aika_skill_rot = 900,
    .cd_aika_basic = 550,
    .cd_aika_heal = 5000,

    .loot_press_count = 2
};

static HANDLE hCombatThread = NULL;
extern HWND g_hMainWnd;

static void log_debug(const char *msg) {
    FILE *f = fopen("C:\\Users\\iuryc\\aika-hud\\debug.log", "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d.%03d] [combat] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
        fclose(f);
    }
}

int detect_foreground_game(HWND *out_hwnd) {
    HWND fg = GetForegroundWindow();

    // Se o foreground for a nossa propria barra HUD ou NULL
    if (fg == g_hMainWnd || fg == NULL) {
        if (g_state.target_hwnd && IsWindow(g_state.target_hwnd)) {
            if (out_hwnd) *out_hwnd = g_state.target_hwnd;
            return g_state.detected_game;
        }
        // Na inicializacao: verifica se Aion 2 esta aberto
        HWND hAion = FindWindowW(L"UnrealWindow", NULL);
        if (hAion && IsWindow(hAion)) {
            g_state.target_hwnd = hAion;
            g_state.detected_game = GAME_AION2;
            if (out_hwnd) *out_hwnd = hAion;
            return GAME_AION2;
        }
        return g_state.detected_game;
    }

    // 1. Checagem INSTANTANEA por Classe e Titulo de Janela (sem depender de OpenProcess)
    WCHAR cls[128] = {0};
    WCHAR title[128] = {0};
    GetClassNameW(fg, cls, 128);
    GetWindowTextW(fg, title, 128);

    WCHAR lowerCls[128] = {0};
    WCHAR lowerTitle[128] = {0};
    for (int i = 0; cls[i] && i < 127; i++) lowerCls[i] = towlower(cls[i]);
    for (int i = 0; title[i] && i < 127; i++) lowerTitle[i] = towlower(title[i]);

    int result = GAME_NONE;

    // Aion 2 (Unreal Engine: Cls contem "unreal" ou Titulo contem "aion")
    if (wcsstr(lowerCls, L"unreal") != NULL || wcsstr(lowerTitle, L"aion") != NULL) {
        result = GAME_AION2;
    }
    // Aika Online (Cls contem "aika" ou Titulo contem "aika")
    else if (wcsstr(lowerCls, L"aika") != NULL || wcsstr(lowerTitle, L"aika") != NULL) {
        result = GAME_AIKA;
    }

    // 2. Fallback via QueryFullProcessImageNameW
    if (result == GAME_NONE) {
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        if (pid != 0) {
            HANDLE hProc = OpenProcess(0x1000, FALSE, pid);
            if (hProc) {
                typedef BOOL (WINAPI *pfn_QueryFullProcessImageNameW)(HANDLE, DWORD, LPWSTR, PDWORD);
                static pfn_QueryFullProcessImageNameW fnQueryW = NULL;
                if (!fnQueryW) {
                    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
                    if (hKernel) fnQueryW = (pfn_QueryFullProcessImageNameW)GetProcAddress(hKernel, "QueryFullProcessImageNameW");
                }
                if (fnQueryW) {
                    WCHAR pathW[1024] = {0};
                    DWORD sz = 1024;
                    if (fnQueryW(hProc, 0, pathW, &sz)) {
                        for (int i = 0; pathW[i] != L'\0'; i++) {
                            WCHAR c = pathW[i];
                            if ((c == L'a' || c == L'A') &&
                                (pathW[i+1] == L'i' || pathW[i+1] == L'I') &&
                                (pathW[i+2] == L'o' || pathW[i+2] == L'O') &&
                                (pathW[i+3] == L'n' || pathW[i+3] == L'N') &&
                                (pathW[i+4] == L'2' || pathW[i+4] == L'2')) {
                                result = GAME_AION2;
                                break;
                            }
                            if ((c == L'a' || c == L'A') &&
                                (pathW[i+1] == L'i' || pathW[i+1] == L'I') &&
                                (pathW[i+2] == L'k' || pathW[i+2] == L'K') &&
                                (pathW[i+3] == L'a' || pathW[i+3] == L'A')) {
                                result = GAME_AIKA;
                                break;
                            }
                        }
                    }
                }
                CloseHandle(hProc);
            }
        }
    }

    if (result != GAME_NONE) {
        g_state.target_hwnd = fg;
        g_state.detected_game = result;
        if (out_hwnd) *out_hwnd = fg;
    }
    return result;
}

void combat_set_action(const char *msg) {
    EnterCriticalSection(&g_state.cs);
    snprintf((char*)g_state.last_action, sizeof(g_state.last_action), "%s", msg);
    LeaveCriticalSection(&g_state.cs);
}

CombatState* combat_get_state(void) {
    return &g_state;
}

CombatConfig* combat_get_config(void) {
    return &g_config;
}

// ==============================================================================
// DETECCAO DE ALVO DUAL (AIKA ONLINE & AION 2)
// ==============================================================================
BOOL is_target_locked(int game) {
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return FALSE;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int midX   = screenW / 2;

    int startX, startY, width, height;
    if (game == GAME_AIKA) {
        // Aika Online 1080p: Barra de HP em Y = 44..52, centralizada em X = 960 (860..1060)
        startY = (screenH == 1080) ? 44 : (int)(screenH * 0.041f);
        startX = midX - 100;
        width  = 200;
        height = 8;
    } else {
        // Aion 2 1080p: Barra de HP em Y = 55..65, centralizada em X = 960 (800..1120)
        startY = (screenH == 1080) ? 55 : (int)(screenH * 0.051f);
        startX = midX - 160;
        width  = 320;
        height = 20;
    }

    HDC memDC = CreateCompatibleDC(hdcScreen);
    if (!memDC) {
        ReleaseDC(NULL, hdcScreen);
        return FALSE;
    }
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
    if (!hBmp) {
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return FALSE;
    }
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    BitBlt(memDC, 0, 0, width, height, hdcScreen, startX, startY, SRCCOPY);

    // Amostra 7 pontos ao longo da barra em memoria RAM
    int sampleOffsets[7] = {
        (int)(width * 0.15f), (int)(width * 0.25f), (int)(width * 0.35f),
        (int)(width * 0.50f), (int)(width * 0.65f), (int)(width * 0.75f),
        (int)(width * 0.85f)
    };
    int redMatches = 0;
    for (int i = 0; i < 7; i++) {
        for (int y = 0; y < height; y++) {
            COLORREF c = GetPixel(memDC, sampleOffsets[i], y);
            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);

            // Vermelho carmesim vivo da barra de HP do alvo
            if (r > 135 && g < 50 && b < 50) {
                redMatches++;
                break;
            }
        }
    }

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, hdcScreen);

    return (redMatches >= 2);
}

// ==============================================================================
// ENTREGA ROBUSTA DE INPUTS (HARDWARE SCANCODE + KEYBD_EVENT + POSTMESSAGE)
// ==============================================================================
void send_game_key(HWND hwnd, WORD vk) {
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LPARAM lpDown = 1 | ((scan & 0xFF) << 16);
    LPARAM lpUp   = 1 | ((scan & 0xFF) << 16) | (1 << 30) | (1 << 31);

    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYDOWN, vk, lpDown);
    }

    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = 0;
    inp.ki.wScan = (WORD)scan;
    inp.ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(1, &inp, sizeof(INPUT));

    keybd_event((BYTE)vk, (BYTE)scan, 0, 0);

    Sleep(70);

    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYUP, vk, lpUp);
    }
    inp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(INPUT));
    keybd_event((BYTE)vk, (BYTE)scan, KEYEVENTF_KEYUP, 0);
}

void click_mouse_left(void) {
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(35);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

void click_mouse_right(void) {
    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
    Sleep(35);
    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
}

void start_walk_forward(void) {
    UINT scan = MapVirtualKeyW('W', MAPVK_VK_TO_VSC);
    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = 0;
    inp.ki.wScan = (WORD)scan;
    inp.ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(1, &inp, sizeof(INPUT));
    keybd_event('W', (BYTE)scan, 0, 0);
}

void stop_walk_forward(void) {
    UINT scan = MapVirtualKeyW('W', MAPVK_VK_TO_VSC);
    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = 0;
    inp.ki.wScan = (WORD)scan;
    inp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(INPUT));
    keybd_event('W', (BYTE)scan, KEYEVENTF_KEYUP, 0);
}

// ==============================================================================
// THREAD DE COMBATE & AUTOMACAO DUAL
// ==============================================================================
static DWORD WINAPI combat_thread_proc(LPVOID lpParam) {
    srand((unsigned int)time(NULL));
    log_debug("combat_thread: Iniciada com suporte dual AION 2 e AIKA");

    DWORD last_tab = 0;
    DWORD last_heal = 0;
    DWORD last_skill_e = 0;
    DWORD last_rmb = 0;
    DWORD last_basic = 0;
    DWORD last_skill_aika = 0;
    DWORD combat_start = 0;
    int skill_aika_idx = 0;
    BOOL was_locked = FALSE;
    BOOL is_walking = FALSE;

    while (g_state.running) {
        // Atalhos globais
        if (GetAsyncKeyState(VK_F8) & 0x8000) {
            combat_toggle_macro();
            if (g_hMainWnd) InvalidateRect(g_hMainWnd, NULL, FALSE);
            Sleep(250);
        }
        if (GetAsyncKeyState(VK_F7) & 0x8000) {
            combat_toggle_heal();
            if (g_hMainWnd) InvalidateRect(g_hMainWnd, NULL, FALSE);
            Sleep(250);
        }
        if (GetAsyncKeyState(VK_F9) & 0x8000) {
            combat_toggle_target();
            if (g_hMainWnd) InvalidateRect(g_hMainWnd, NULL, FALSE);
            Sleep(250);
        }
        if (GetAsyncKeyState(VK_F10) & 0x8000) {
            combat_toggle_loot();
            if (g_hMainWnd) InvalidateRect(g_hMainWnd, NULL, FALSE);
            Sleep(250);
        }
        if (GetAsyncKeyState(VK_F12) & 0x8000) {
            log_debug("combat_thread: F12 pressionado! Encerrando...");
            if (is_walking) stop_walk_forward();
            if (g_hMainWnd) PostMessageW(g_hMainWnd, WM_CLOSE, 0, 0);
            break;
        }

        HWND hwnd_game = NULL;
        int game = detect_foreground_game(&hwnd_game);
        g_state.detected_game = game;
        g_state.is_foreground = (game != GAME_NONE);

        if (g_state.macro_active) {
            if (game == GAME_NONE) {
                if (is_walking) {
                    stop_walk_forward();
                    is_walking = FALSE;
                }
                combat_set_action("Aguardando jogo em foco (Aion 2 ou Aika)...");
                Sleep(50);
                continue;
            }

            DWORD now = GetTickCount();

            // 1. Amostra estado do alvo
            BOOL locked = is_target_locked(game);
            g_state.target_locked = locked;

            // 2. Transicao: Mob derrotado (barra sumiu)
            if (was_locked && !locked) {
                g_state.mobs_killed++;
                was_locked = FALSE;
                if (is_walking) {
                    stop_walk_forward();
                    is_walking = FALSE;
                }
                combat_set_action("Mob derrotado! Coletando Loot...");
                Beep(1000, 50);

                if (g_state.auto_loot) {
                    Sleep(120);
                    WORD loot_key = (game == GAME_AION2) ? g_config.key_aion_loot : g_config.key_aika_loot;
                    for (int i = 0; i < g_config.loot_press_count; i++) {
                        send_game_key(hwnd_game, loot_key);
                        Sleep(100);
                    }
                }
                last_tab = 0;
            }

            // 3. MODO BUSCA (Nenhum alvo selecionado)
            if (!locked) {
                if (is_walking) {
                    stop_walk_forward();
                    is_walking = FALSE;
                }
                if (g_state.auto_target && (now - last_tab > 1200)) {
                    combat_set_action("Buscando alvo com [TAB]...");
                    send_game_key(hwnd_game, VK_TAB);
                    last_tab = now;

                    Sleep(80);
                    if (is_target_locked(game)) {
                        locked = TRUE;
                        g_state.target_locked = TRUE;
                        combat_start = GetTickCount();
                        Beep(1400, 60);
                        combat_set_action("Alvo encontrado! Iniciando combate...");
                    }
                }
            }

            // 4. MODO COMBATE (Alvo presente)
            if (locked) {
                if (!was_locked) {
                    was_locked = TRUE;
                    combat_start = GetTickCount();
                }

                if (game == GAME_AION2) {
                    // ==============================================================
                    // MODO AION 2: APROXIMACAO + Q + E + RMB + LMB
                    // ==============================================================
                    // 4.1. Aproximacao com W nos primeiros 1.4s
                    if (now - combat_start < 1400) {
                        if (!is_walking) {
                            start_walk_forward();
                            is_walking = TRUE;
                        }
                    } else {
                        if (is_walking) {
                            stop_walk_forward();
                            is_walking = FALSE;
                        }
                    }

                    // 4.2. Cura / Defensiva (Q)
                    if (g_state.auto_heal && (now - last_heal > (DWORD)g_config.cd_aion_heal)) {
                        send_game_key(hwnd_game, g_config.key_aion_heal);
                        combat_set_action("Aion 2: Cura [Q] disparada");
                        last_heal = now;
                        Sleep(30);
                    }

                    // 4.3. Skill de Dano (E)
                    if (now - last_skill_e > (DWORD)g_config.cd_aion_skill_e) {
                        send_game_key(hwnd_game, g_config.key_aion_skill_e);
                        combat_set_action("Aion 2: Skill [E] disparada");
                        last_skill_e = now;
                        Sleep(30);
                    }

                    // 4.4. Skill Secundaria (RMB)
                    if (now - last_rmb > (DWORD)g_config.cd_aion_rmb) {
                        click_mouse_right();
                        combat_set_action("Aion 2: Skill [RMB] disparada");
                        last_rmb = now;
                        Sleep(30);
                    }

                    // 4.5. Ataque Basico Continuo (LMB)
                    click_mouse_left();
                    Sleep(70);
                }
                else if (game == GAME_AIKA) {
                    // ==============================================================
                    // MODO AIKA: ATAQUE NORMAL (5) + SKILLS 1,2,3 + CURA (4)
                    // ==============================================================
                    if (now - last_basic > (DWORD)g_config.cd_aika_basic) {
                        send_game_key(hwnd_game, g_config.key_aika_basic);
                        last_basic = now;
                    }

                    if (now - last_skill_aika > (DWORD)g_config.cd_aika_skill_rot) {
                        WORD cur = g_config.key_aika_skill1;
                        if (skill_aika_idx == 1) cur = g_config.key_aika_skill2;
                        else if (skill_aika_idx == 2) cur = g_config.key_aika_skill3;
                        send_game_key(hwnd_game, cur);
                        skill_aika_idx = (skill_aika_idx + 1) % 3;
                        last_skill_aika = now;
                    }

                    if (g_state.auto_heal && (now - last_heal > (DWORD)g_config.cd_aika_heal)) {
                        send_game_key(hwnd_game, g_config.key_aika_heal);
                        last_heal = now;
                    }
                    Sleep(50);
                }
            }

            Sleep(30 + (rand() % 15));
        } else {
            if (is_walking) {
                stop_walk_forward();
                is_walking = FALSE;
            }
            Sleep(50);
        }
    }

    if (is_walking) stop_walk_forward();
    return 0;
}

void combat_init(void) {
    log_debug("combat_init: Inicializando CS...");
    InitializeCriticalSection(&g_state.cs);
    hCombatThread = CreateThread(NULL, 0, combat_thread_proc, NULL, 0, NULL);
    log_debug("combat_init: Finalizado com sucesso.");
}

void combat_cleanup(void) {
    g_state.macro_active = FALSE;
    g_state.running = FALSE;
    if (hCombatThread) {
        WaitForSingleObject(hCombatThread, 1000);
        CloseHandle(hCombatThread);
        hCombatThread = NULL;
    }
    DeleteCriticalSection(&g_state.cs);
}

void combat_toggle_macro(void) {
    g_state.macro_active = !g_state.macro_active;
    if (g_state.macro_active) {
        Beep(1200, 80);
        if (g_state.session_start_time == 0) {
            g_state.session_start_time = GetTickCount();
        }
        HWND hwnd = NULL;
        int game = detect_foreground_game(&hwnd);
        if (is_target_locked(game)) {
            combat_set_action("Ativado! Alvo presente, iniciando combate");
        } else {
            send_game_key(hwnd, VK_TAB);
            combat_set_action("Ativado! Buscando monstro [TAB]");
        }
    } else {
        Beep(600, 100);
        combat_set_action("Pausado pelo usuario [F8]");
    }
}

void combat_toggle_target(void) {
    g_state.auto_target = !g_state.auto_target;
    Beep(900, 60);
    combat_set_action(g_state.auto_target ? "Auto-Target [TAB] LIGADO" : "Auto-Target [TAB] DESLIGADO");
}

void combat_toggle_loot(void) {
    g_state.auto_loot = !g_state.auto_loot;
    Beep(900, 60);
    combat_set_action(g_state.auto_loot ? "Auto-Loot LIGADO" : "Auto-Loot DESLIGADO");
}

void combat_toggle_heal(void) {
    g_state.auto_heal = !g_state.auto_heal;
    Beep(900, 60);
    combat_set_action(g_state.auto_heal ? "Auto-Cura LIGADA" : "Auto-Cura DESLIGADA");
}
