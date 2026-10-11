#ifndef COMBAT_H
#define COMBAT_H

#include <windows.h>

#ifndef KEYEVENTF_SCANCODE
#define KEYEVENTF_SCANCODE 0x0008
#endif

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

typedef struct {
    volatile BOOL macro_active;
    volatile BOOL auto_target;
    volatile BOOL auto_loot;
    volatile BOOL auto_heal;
    volatile BOOL running;
    volatile BOOL target_locked;
    volatile BOOL is_foreground;
    volatile int  mobs_killed;
    DWORD session_start_time;
    char last_action[128];
    HWND aika_hwnd;
    CRITICAL_SECTION cs;
} CombatState;

typedef struct {
    WORD key_basic;        // Padrão: '5' (Ataque Normal no Slot 5)
    WORD key_skill1;       // Padrão: '1'
    WORD key_skill2;       // Padrão: '2'
    WORD key_skill3;       // Padrão: '3'
    WORD key_heal;         // Padrão: '4'
    WORD key_loot;         // Padrão: VK_SPACE
    int  cd_skill_rot;     // Intervalo rotação skills (ms)
    int  cd_basic;         // Intervalo ataque básico (ms)
    int  cd_heal;          // Intervalo cura/poção (ms)
    int  loot_press_count; // Quantidade de disparos de loot
} CombatConfig;

void combat_init(void);
void combat_cleanup(void);
void combat_toggle_macro(void);
void combat_toggle_target(void);
void combat_toggle_loot(void);
void combat_toggle_heal(void);
CombatState* combat_get_state(void);
CombatConfig* combat_get_config(void);
HWND find_aika_window(void);
BOOL is_aika_foreground(void);
void combat_set_action(const char *msg);
BOOL is_target_locked(void);
void send_game_key(HWND hwnd, WORD vk);

#endif
