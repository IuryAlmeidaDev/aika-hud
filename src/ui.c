#include "ui.h"
#include <stdio.h>

#define COLOR_BG          RGB(15, 17, 21)       // #0F1115
#define COLOR_BORDER      RGB(42, 50, 64)       // #2A3240
#define COLOR_SLOT_BG     RGB(22, 27, 36)       // #161B24
#define COLOR_TEXT_DIM    RGB(124, 138, 156)    // #7C8A9C
#define COLOR_TEXT_WHITE  RGB(220, 226, 236)    // #DCE2EC
#define COLOR_CYAN        RGB(0, 240, 255)      // #00F0FF
#define COLOR_GREEN       RGB(46, 213, 115)     // #2ED573
#define COLOR_RED         RGB(255, 71, 87)      // #FF4757
#define COLOR_AMBER       RGB(255, 190, 118)    // #FFBE76
#define COLOR_ORANGE      RGB(255, 165, 2)      // #FFA502
#define COLOR_BADGE_BG    RGB(19, 29, 46)       // #131D2E

static HFONT hFontBold = NULL;
static HFONT hFontSmall = NULL;

void ui_init(void) {
    hFontBold = CreateFontA(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    hFontSmall = CreateFontA(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

void ui_cleanup(void) {
    if (hFontBold)  { DeleteObject(hFontBold);  hFontBold = NULL; }
    if (hFontSmall) { DeleteObject(hFontSmall); hFontSmall = NULL; }
}

static void draw_pill(HDC hdc, int x, int y, int w, int h, COLORREF bg, COLORREF border, const char *text, COLORREF textColor, HFONT hFont) {
    HBRUSH hBrush = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, x, y, x + w, y + h, 6, 6);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    RECT rc = { x, y, x + w, y + h };
    DrawTextA(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
}

void ui_render(HDC hdc, int width, int height) {
    CombatState *state = combat_get_state();

    // Double buffering para 0% de cintilação (zero flicker)
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // Fundo da barra
    HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG);
    RECT fullRc = { 0, 0, width, height };
    FillRect(memDC, &fullRc, hBgBrush);
    DeleteObject(hBgBrush);

    // Borda da barra
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(memDC, hBorderPen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Rectangle(memDC, 0, 0, width, height);
    SelectObject(memDC, oldPen);
    DeleteObject(hBorderPen);

    // 1. Badge AIKA HUD
    draw_pill(memDC, 6, 6, 100, 32, COLOR_BADGE_BG, COLOR_CYAN, "AIKA HUD", COLOR_CYAN, hFontBold);

    // 2. Status do Macro: [F8] COMBATE (ON) / [F8] ATIVAR (OFF)
    if (state->macro_active) {
        draw_pill(memDC, 112, 6, 160, 32, RGB(18, 48, 30), COLOR_GREEN, "[F8] COMBATE: ON", COLOR_GREEN, hFontBold);
    } else {
        draw_pill(memDC, 112, 6, 160, 32, RGB(48, 20, 24), COLOR_RED, "[F8] ATIVAR: OFF", COLOR_RED, hFontBold);
    }

    // 3. Status do Alvo & Mobs mortos
    char targetStr[64];
    if (state->target_locked) {
        snprintf(targetStr, sizeof(targetStr), "ALVO [OK] | %d KILLS", state->mobs_killed);
        draw_pill(memDC, 278, 6, 170, 32, COLOR_SLOT_BG, COLOR_CYAN, targetStr, COLOR_CYAN, hFontSmall);
    } else {
        snprintf(targetStr, sizeof(targetStr), "BUSCA [TAB] | %d KILLS", state->mobs_killed);
        draw_pill(memDC, 278, 6, 170, 32, COLOR_SLOT_BG, COLOR_BORDER, targetStr, COLOR_AMBER, hFontSmall);
    }

    // 4. Cura [F7]
    if (state->auto_heal) {
        draw_pill(memDC, 454, 6, 110, 32, COLOR_SLOT_BG, COLOR_BORDER, "[F7] CURA: ON", COLOR_GREEN, hFontSmall);
    } else {
        draw_pill(memDC, 454, 6, 110, 32, COLOR_SLOT_BG, COLOR_BORDER, "[F7] CURA: OFF", COLOR_TEXT_DIM, hFontSmall);
    }

    // 5. Loot [F10]
    if (state->auto_loot) {
        draw_pill(memDC, 570, 6, 110, 32, COLOR_SLOT_BG, COLOR_BORDER, "[F10] LOOT: ON", COLOR_GREEN, hFontSmall);
    } else {
        draw_pill(memDC, 570, 6, 110, 32, COLOR_SLOT_BG, COLOR_BORDER, "[F10] LOOT: OFF", COLOR_TEXT_DIM, hFontSmall);
    }

    // 6. Auto-Target [F9]
    if (state->auto_target) {
        draw_pill(memDC, 686, 6, 110, 32, COLOR_SLOT_BG, COLOR_BORDER, "[F9] TAB: ON", COLOR_GREEN, hFontSmall);
    } else {
        draw_pill(memDC, 686, 6, 110, 32, COLOR_SLOT_BG, COLOR_BORDER, "[F9] TAB: OFF", COLOR_TEXT_DIM, hFontSmall);
    }

    // 7. Janela do Aika: AIKA [OK] / ALT-TAB
    if (state->is_foreground) {
        draw_pill(memDC, 802, 6, 75, 32, COLOR_SLOT_BG, COLOR_BORDER, "AIKA [OK]", COLOR_CYAN, hFontSmall);
    } else {
        draw_pill(memDC, 802, 6, 75, 32, COLOR_SLOT_BG, COLOR_BORDER, "ALT-TAB", COLOR_ORANGE, hFontSmall);
    }

    // 8. Timer de Sessão
    DWORD elapsedSec = 0;
    if (state->session_start_time > 0) {
        elapsedSec = (GetTickCount() - state->session_start_time) / 1000;
    }
    int mins = (int)(elapsedSec / 60);
    int secs = (int)(elapsedSec % 60);
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", mins, secs);
    draw_pill(memDC, 883, 6, 50, 32, COLOR_SLOT_BG, COLOR_BORDER, timeStr, COLOR_TEXT_WHITE, hFontSmall);

    // 9. Botão Fechar [X]
    draw_pill(memDC, 938, 6, 18, 32, COLOR_SLOT_BG, COLOR_BORDER, "X", COLOR_TEXT_DIM, hFontBold);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void ui_handle_click(HWND hwnd, int x, int y) {
    // Clique no botão [X]
    if (x >= 938 && x <= 958 && y >= 6 && y <= 38) {
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        return;
    }

    // Clique no Status [F8]
    if (x >= 112 && x <= 272 && y >= 6 && y <= 38) {
        combat_toggle_macro();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    // Clique na Cura [F7]
    if (x >= 454 && x <= 564 && y >= 6 && y <= 38) {
        combat_toggle_heal();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    // Clique no Loot [F10]
    if (x >= 570 && x <= 680 && y >= 6 && y <= 38) {
        combat_toggle_loot();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    // Clique no Target [F9]
    if (x >= 686 && x <= 796 && y >= 6 && y <= 38) {
        combat_toggle_target();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    // Arrasta a barra em qualquer outra área
    ReleaseCapture();
    SendMessageA(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}
