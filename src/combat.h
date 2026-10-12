#ifndef COMBAT_H
#define COMBAT_H

#include <windows.h>

#ifndef KEYEVENTF_SCANCODE
#define KEYEVENTF_SCANCODE 0x0008
#endif

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

#define GAME_NONE  0
#define GAME_AION2 1
#define GAME_AIKA  2

typedef struct {
    volatile BOOL macro_active;
    volatile BOOL auto_target;
    volatile BOOL auto_loot;
    volatile BOOL auto_heal;
    volatile BOOL running;
    volatile BOOL target_locked;
    volatile BOOL is_foreground;
    volatile int  detected_game;   // GAME_NONE, GAME_AION2, GAME_AIKA
    volatile int  mobs_killed;
    DWORD session_start_time;
    char last_action[128];
    HWND target_hwnd;
    CRITICAL_SECTION cs;
} CombatState;

typedef struct {
    // Aion 2
    WORD key_aion_heal;     // 'Q'
    WORD key_aion_skill_e;  // 'E'
    WORD key_aion_loot;     // 'F'
    int  cd_aion_skill_e;   // 2000ms
    int  cd_aion_rmb;       // 1400ms
    int  cd_aion_heal;      // 3500ms

    // Aika
    WORD key_aika_basic;    // '5'
    WORD key_aika_skill1;   // '1'
    WORD key_aika_skill2;   // '2'
    WORD key_aika_skill3;   // '3'
    WORD key_aika_heal;     // '4'
    WORD key_aika_loot;     // VK_SPACE
    int  cd_aika_skill_rot; // 900ms
    int  cd_aika_basic;     // 550ms
    int  cd_aika_heal;      // 5000ms

    int  loot_press_count;  // 2
} CombatConfig;

void combat_init(void);
void combat_cleanup(void);
void combat_toggle_macro(void);
void combat_toggle_target(void);
void combat_toggle_loot(void);
void combat_toggle_heal(void);
CombatState* combat_get_state(void);
CombatConfig* combat_get_config(void);
int detect_foreground_game(HWND *out_hwnd);
void combat_set_action(const char *msg);
BOOL is_target_locked(int game);
void send_game_key(HWND hwnd, WORD vk);
void click_mouse_left(void);
void click_mouse_right(void);
void start_walk_forward(void);
void stop_walk_forward(void);

#endif
