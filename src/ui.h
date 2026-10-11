#ifndef UI_H
#define UI_H

#include <windows.h>
#include "combat.h"

#define HUD_WIDTH   330
#define HUD_HEIGHT  410

void ui_init(void);
void ui_cleanup(void);
void ui_render(HDC hdc, int width, int height);
void ui_handle_click(HWND hwnd, int x, int y);

#endif
