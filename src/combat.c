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
    .mobs_killed = 0,
    .session_start_time = 0,
    .last_action = "Aguardando inicio [F8]",
    .aika_hwnd = NULL
};

static CombatConfig g_config = {
    .key_basic = '5',        // Slot 5: Ataque Normal
    .key_skill1 = '1',       // Slot 1: Skill 1
    .key_skill2 = '2',       // Slot 2: Skill 2
    .key_skill3 = '3',       // Slot 3: Skill 3
    .key_heal = '4',         // Slot 4: Cura / Poção
    .key_loot = VK_SPACE,    // Espaço: Coleta de itens
    .cd_skill_rot = 1000,    // 1000ms entre skills
    .cd_basic = 600,         // 600ms entre ataques normais (aproximação contínua)
    .cd_heal = 6000,         // 6000ms poção / cura
    .loot_press_count = 2    // 2 cliques de loot ao matar o mob
};

static HANDLE hCombatThread = NULL;
static WCHAR g_iniPath[MAX_PATH] = {0};

static void init_config_path(void) {
    GetModuleFileNameW(NULL, g_iniPath, MAX_PATH);
    WCHAR *lastSlash = wcsrchr(g_iniPath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
        wcscat(g_iniPath, L"config.ini");
    } else {
        wcscpy(g_iniPath, L"config.ini");
    }
}

static WORD parse_key_str(const WCHAR *str, WORD defaultVk) {
    if (!str || !str[0]) return defaultVk;
    if (_wcsicmp(str, L"SPACE") == 0) return VK_SPACE;
    if (_wcsicmp(str, L"TAB") == 0) return VK_TAB;
    if (_wcsicmp(str, L"ENTER") == 0 || _wcsicmp(str, L"RETURN") == 0) return VK_RETURN;
    WCHAR c = str[0];
    if (c >= L'a' && c <= L'z') c -= (L'a' - L'A');
    return (WORD)c;
}

static void load_or_create_config(void) {
    init_config_path();
    DWORD attr = GetFileAttributesW(g_iniPath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        FILE *f = _wfopen(g_iniPath, L"w");
        if (f) {
            fwprintf(f,
                L"[Combate]\n"
                L"; Tecla do Ataque Normal (arraste a habilidade basica do K para este slot)\n"
                L"KeyBasic=5\n"
                L"; Teclas das habilidades principais na barra de acao\n"
                L"KeySkill1=1\n"
                L"KeySkill2=2\n"
                L"KeySkill3=3\n"
                L"; Tecla de pocao de vida ou cura\n"
                L"KeyHeal=4\n"
                L"; Tecla de coleta de itens (Loot) - padrao SPACE\n"
                L"KeyLoot=SPACE\n"
                L"; Intervalo entre uso de habilidades em milissegundos\n"
                L"SkillRotationMs=1000\n"
                L"; Intervalo de reforco de ataque normal/aproximacao (ms)\n"
                L"BasicAttackMs=600\n"
                L"; Intervalo de uso de pocao/cura (ms)\n"
                L"HealCooldownMs=6000\n"
                L"; Quantidade de vezes que pressiona loot ao derrotar monstro\n"
                L"LootPressCount=2\n\n"
                L"[Modulos]\n"
                L"AutoTarget=1\n"
                L"AutoLoot=1\n"
                L"AutoHeal=1\n"
            );
            fclose(f);
        }
    }

    WCHAR buf[32] = {0};

    GetPrivateProfileStringW(L"Combate", L"KeyBasic", L"5", buf, 32, g_iniPath);
    g_config.key_basic = parse_key_str(buf, '5');

    GetPrivateProfileStringW(L"Combate", L"KeySkill1", L"1", buf, 32, g_iniPath);
    g_config.key_skill1 = parse_key_str(buf, '1');

    GetPrivateProfileStringW(L"Combate", L"KeySkill2", L"2", buf, 32, g_iniPath);
    g_config.key_skill2 = parse_key_str(buf, '2');

    GetPrivateProfileStringW(L"Combate", L"KeySkill3", L"3", buf, 32, g_iniPath);
    g_config.key_skill3 = parse_key_str(buf, '3');

    GetPrivateProfileStringW(L"Combate", L"KeyHeal", L"4", buf, 32, g_iniPath);
    g_config.key_heal = parse_key_str(buf, '4');

    GetPrivateProfileStringW(L"Combate", L"KeyLoot", L"SPACE", buf, 32, g_iniPath);
    g_config.key_loot = parse_key_str(buf, VK_SPACE);

    g_config.cd_skill_rot = GetPrivateProfileIntW(L"Combate", L"SkillRotationMs", 1000, g_iniPath);
    g_config.cd_basic = GetPrivateProfileIntW(L"Combate", L"BasicAttackMs", 600, g_iniPath);
    g_config.cd_heal = GetPrivateProfileIntW(L"Combate", L"HealCooldownMs", 6000, g_iniPath);
    g_config.loot_press_count = GetPrivateProfileIntW(L"Combate", L"LootPressCount", 2, g_iniPath);

    g_state.auto_target = GetPrivateProfileIntW(L"Modulos", L"AutoTarget", 1, g_iniPath);
    g_state.auto_loot   = GetPrivateProfileIntW(L"Modulos", L"AutoLoot", 1, g_iniPath);
    g_state.auto_heal   = GetPrivateProfileIntW(L"Modulos", L"AutoHeal", 1, g_iniPath);
}

HWND find_aika_window(void) {
    HWND hwnd = FindWindowW(L"AikaClient Window", NULL);
    if (!hwnd) {
        hwnd = FindWindowW(NULL, L"AikaClient");
    }
    g_state.aika_hwnd = hwnd;
    return hwnd;
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
// DETECÇÃO DE ALVO ULTRA-RÁPIDA (BITBLT MEMORY DC EM MEMÓRIA RAM)
// ==============================================================================
BOOL is_target_locked(void) {
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return FALSE;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Em 1080p, a barra carmesim do alvo fica centralizada em X = 960, Y = 48
    int midX   = screenW / 2;
    int barY   = (screenH == 1080) ? 48 : (int)(screenH * 0.0444f + 0.5f);
    int startX = midX - 100;
    int width  = 200;
    int height = 5;

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

    // Copia apenas a fatia da barra do alvo diretamente da tela
    BitBlt(memDC, 0, 0, width, height, hdcScreen, startX, barY - 2, SRCCOPY);

    // Amostra 5 pontos estratégicos da barra (X = 900, 940, 960, 980, 1020)
    int sampleOffsets[5] = { 40, 80, 100, 120, 160 };
    int redMatches = 0;
    for (int i = 0; i < 5; i++) {
        for (int y = 0; y < height; y++) {
            COLORREF c = GetPixel(memDC, sampleOffsets[i], y);
            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);

            // Vermelho carmesim vivo da barra de vida do alvo (R > 130, G < 40, B < 40)
            if (r > 130 && g < 40 && b < 40) {
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
// BYPASS GAMEGUARD: POSTMESSAGE DIRETO + SENDINPUT HARDWARE SCANCODE
// (SEM CHAMAR KEYBD_EVENT PARA NÃO DISPARAR O HOOK WH_KEYBOARD_LL DO GAMEGUARD)
// ==============================================================================
void send_game_key(HWND hwnd, WORD vk) {
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LPARAM lpDown = 1 | ((scan & 0xFF) << 16);
    LPARAM lpUp   = 1 | ((scan & 0xFF) << 16) | (1 << 30) | (1 << 31);

    // 1. PostMessage direto para a fila da janela (fura hook de teclado de terceiro)
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYDOWN, vk, lpDown);
    }

    // 2. SendInput com KEYEVENTF_SCANCODE (emulação pura de hardware controller)
    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = vk;
    inp.ki.wScan = (WORD)scan;
    inp.ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(1, &inp, sizeof(INPUT));

    Sleep(45);

    // Soltura da tecla
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYUP, vk, lpUp);
    }
    inp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(INPUT));
}

// ==============================================================================
// THREAD DE COMBATE & MÁQUINA DE ESTADOS
// ==============================================================================
static DWORD WINAPI combat_thread_proc(LPVOID lpParam) {
    srand((unsigned int)time(NULL));

    DWORD last_tab = 0;
    DWORD last_basic = 0;
    DWORD last_skill = 0;
    DWORD last_heal = 0;
    int skill_index = 0;
    BOOL was_locked = FALSE;

    while (g_state.running) {
        if (g_state.macro_active) {
            HWND hwnd_aika = find_aika_window();
            DWORD now = GetTickCount();

            // 1. Amostra o estado atual do alvo por leitura de pixel
            BOOL locked = is_target_locked();
            g_state.target_locked = locked;

            // 2. Transição: Mob acabou de morrer (estava travado e a barra sumiu)
            if (was_locked && !locked) {
                g_state.mobs_killed++;
                was_locked = FALSE;
                combat_set_action("Mob derrotado! Coletando Loot...");

                if (g_state.auto_loot) {
                    Sleep(150);
                    for (int i = 0; i < g_config.loot_press_count; i++) {
                        send_game_key(hwnd_aika, g_config.key_loot);
                        Sleep(120);
                    }
                }
                last_tab = 0; // Habilita busca do próximo alvo imediatamente
            }

            // 3. MODO BUSCA (Nenhum alvo selecionado)
            if (!locked) {
                if (g_state.auto_target && (now - last_tab > 1200)) {
                    combat_set_action("Buscando alvo com [TAB]...");
                    send_game_key(hwnd_aika, VK_TAB);
                    last_tab = now;

                    Sleep(100);
                    // Checa se o TAB já travou no monstro imediatamente
                    if (is_target_locked()) {
                        locked = TRUE;
                        g_state.target_locked = TRUE;
                        combat_set_action("Alvo encontrado! Correndo e atacando...");
                        send_game_key(hwnd_aika, g_config.key_basic);
                        last_basic = GetTickCount();
                    }
                }
            }

            // 4. MODO COMBATE (Alvo travado com barra vermelha visível)
            if (locked) {
                was_locked = TRUE;

                // 4.1. Ataque Normal Contínuo (faz o personagem correr até o mob e manter auto-ataque)
                if (now - last_basic > (DWORD)g_config.cd_basic) {
                    send_game_key(hwnd_aika, g_config.key_basic);
                    last_basic = now;
                }

                // 4.2. Rotação de Skills principais (1, 2, 3)
                if (now - last_skill > (DWORD)g_config.cd_skill_rot) {
                    WORD current_skill = g_config.key_skill1;
                    if (skill_index == 1) current_skill = g_config.key_skill2;
                    else if (skill_index == 2) current_skill = g_config.key_skill3;

                    send_game_key(hwnd_aika, current_skill);

                    char msg[64];
                    snprintf(msg, sizeof(msg), "Combate: Skill [%c] disparada", (char)current_skill);
                    combat_set_action(msg);

                    skill_index = (skill_index + 1) % 3;
                    last_skill = now;
                }

                // 4.3. Auto-Cura / Poção periódica
                if (g_state.auto_heal && (now - last_heal > (DWORD)g_config.cd_heal)) {
                    send_game_key(hwnd_aika, g_config.key_heal);
                    combat_set_action("Usou Pocao/Cura [Slot 4]");
                    last_heal = now;
                }
            }

            Sleep(40 + (rand() % 21));
        } else {
            Sleep(50);
        }
    }
    return 0;
}

void combat_init(void) {
    InitializeCriticalSection(&g_state.cs);
    load_or_create_config();
    find_aika_window();
    hCombatThread = CreateThread(NULL, 0, combat_thread_proc, NULL, 0, NULL);
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
        Beep(1200, 100);
        if (g_state.session_start_time == 0) {
            g_state.session_start_time = GetTickCount();
        }
        HWND hwnd = find_aika_window();
        if (is_target_locked()) {
            send_game_key(hwnd, g_config.key_basic);
            combat_set_action("Ativado! Alvo presente, iniciando combate");
        } else {
            send_game_key(hwnd, VK_TAB);
            combat_set_action("Ativado! Buscando primeiro monstro [TAB]");
        }
    } else {
        Beep(600, 150);
        combat_set_action("Pausado pelo usuario [F8]");
    }
}

void combat_toggle_target(void) {
    g_state.auto_target = !g_state.auto_target;
    Beep(900, 80);
    combat_set_action(g_state.auto_target ? "Auto-Target [TAB] LIGADO" : "Auto-Target [TAB] DESLIGADO");
}

void combat_toggle_loot(void) {
    g_state.auto_loot = !g_state.auto_loot;
    Beep(900, 80);
    combat_set_action(g_state.auto_loot ? "Auto-Loot [SPACE] LIGADO" : "Auto-Loot [SPACE] DESLIGADO");
}

void combat_toggle_heal(void) {
    g_state.auto_heal = !g_state.auto_heal;
    Beep(900, 80);
    combat_set_action(g_state.auto_heal ? "Auto-Cura [4] LIGADA" : "Auto-Cura [4] DESLIGADA");
}
