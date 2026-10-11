#include "ui.h"
#include <stdio.h>

// ==============================================================================
// PALETA OBSIDIAN GLASSMORPHISM (Paleta GhostHUD)
// ==============================================================================
#define COLOR_BG          RGB(15, 17, 21)       // #0F1115
#define COLOR_HEADER_BG   RGB(19, 23, 30)       // #13171E
#define COLOR_CARD_BG     RGB(22, 27, 36)       // #161B24
#define COLOR_CARD_BORDER RGB(34, 42, 54)       // #222A36
#define COLOR_BORDER      RGB(42, 50, 64)       // #2A3240

#define COLOR_TITLE       RGB(0, 229, 255)      // Ciano elétrico neon (#00E5FF)
#define COLOR_TEXT_WHITE  RGB(230, 235, 245)    // Texto branco suave
#define COLOR_TEXT_MUTED  RGB(124, 138, 156)    // Texto secundário / labels
#define COLOR_GREEN       RGB(46, 213, 115)     // Verde esmeralda (#2ED573)
#define COLOR_RED         RGB(255, 71, 87)      // Vermelho carmesim (#FF4757)
#define COLOR_YELLOW      RGB(255, 190, 118)    // Ouro / aviso (#FFBE76)
#define COLOR_CYAN        RGB(0, 240, 255)      // #00F0FF

static HFONT hFontTitle = NULL;
static HFONT hFontHeader = NULL;
static HFONT hFontRegular = NULL;
static HFONT hFontBold = NULL;
static HFONT hFontSmall = NULL;

void ui_init(void) {
    hFontTitle = CreateFontA(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    hFontHeader = CreateFontA(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    hFontRegular = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    hFontBold = CreateFontA(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

    hFontSmall = CreateFontA(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

void ui_cleanup(void) {
    if (hFontTitle)   { DeleteObject(hFontTitle);   hFontTitle = NULL; }
    if (hFontHeader)  { DeleteObject(hFontHeader);  hFontHeader = NULL; }
    if (hFontRegular) { DeleteObject(hFontRegular); hFontRegular = NULL; }
    if (hFontBold)    { DeleteObject(hFontBold);    hFontBold = NULL; }
    if (hFontSmall)   { DeleteObject(hFontSmall);   hFontSmall = NULL; }
}

static void draw_rounded_card(HDC hdc, int x, int y, int w, int h, COLORREF bg, COLORREF border, int radius) {
    HBRUSH hBrush = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, x, y, x + w, y + h, radius, radius);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

static void draw_toggle(HDC hdc, int x, int y, int w, int h, BOOL active) {
    COLORREF bg = active ? RGB(26, 75, 45) : RGB(45, 25, 30);
    COLORREF border = active ? COLOR_GREEN : COLOR_RED;
    draw_rounded_card(hdc, x, y, w, h, bg, border, 6);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, active ? COLOR_GREEN : COLOR_RED);
    SelectObject(hdc, hFontBold);

    const char *txt = active ? "LIGADO" : "DESLIG.";
    RECT r = { x, y, x + w, y + h };
    DrawTextA(hdc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void ui_render(HDC hdc, int width, int height) {
    CombatState *state = combat_get_state();
    CombatConfig *config = combat_get_config();

    // Double buffering para 0% de cintilação (zero flicker)
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    // 1. Fundo da Janela
    HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG);
    RECT rClient = { 0, 0, width, height };
    FillRect(memDC, &rClient, hBgBrush);
    DeleteObject(hBgBrush);

    // Borda Externa da Janela
    HPEN hBorderPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN oldPen = (HPEN)SelectObject(memDC, hBorderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Rectangle(memDC, 0, 0, width, height);
    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldPen);
    DeleteObject(hBorderPen);

    // 2. Barra de Título (Header)
    HBRUSH hHdrBrush = CreateSolidBrush(COLOR_HEADER_BG);
    RECT rHeader = { 1, 1, width - 1, 32 };
    FillRect(memDC, &rHeader, hHdrBrush);
    DeleteObject(hHdrBrush);

    // Linha divisória do header
    HPEN hDivPen = CreatePen(PS_SOLID, 1, COLOR_CARD_BORDER);
    oldPen = (HPEN)SelectObject(memDC, hDivPen);
    MoveToEx(memDC, 0, 32, NULL);
    LineTo(memDC, width, 32);
    SelectObject(memDC, oldPen);
    DeleteObject(hDivPen);

    SetBkMode(memDC, TRANSPARENT);

    // Título do HUD
    SelectObject(memDC, hFontTitle);
    SetTextColor(memDC, COLOR_TITLE);
    RECT rTitle = { 12, 6, 200, 28 };
    DrawTextA(memDC, "AIKA COMBAT HUD", -1, &rTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Indicador luminoso (bolinha verde/vermelha no header)
    int dot_cx = width - 48;
    int dot_cy = 16;
    HBRUSH hDotBrush = CreateSolidBrush(state->macro_active ? COLOR_GREEN : COLOR_RED);
    HPEN hDotPen = CreatePen(PS_SOLID, 1, state->macro_active ? COLOR_GREEN : COLOR_RED);
    SelectObject(memDC, hDotBrush);
    SelectObject(memDC, hDotPen);
    Ellipse(memDC, dot_cx - 4, dot_cy - 4, dot_cx + 5, dot_cy + 5);
    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldPen);
    DeleteObject(hDotBrush);
    DeleteObject(hDotPen);

    // Botão de Fechar [X]
    SelectObject(memDC, hFontBold);
    SetTextColor(memDC, COLOR_TEXT_MUTED);
    RECT rClose = { width - 28, 4, width - 6, 28 };
    DrawTextA(memDC, "x", -1, &rClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int btn_x = 12;
    int btn_w = width - 24;

    // 3. Card 1: Botão Principal de Ativação / Pausa (F8)
    int btn_y = 40;
    int btn_h = 58;

    COLORREF main_bg = state->macro_active ? RGB(16, 42, 28) : RGB(40, 18, 24);
    COLORREF main_border = state->macro_active ? COLOR_GREEN : COLOR_RED;
    draw_rounded_card(memDC, btn_x, btn_y, btn_w, btn_h, main_bg, main_border, 8);

    SelectObject(memDC, hFontHeader);
    SetTextColor(memDC, state->macro_active ? COLOR_GREEN : COLOR_RED);
    RECT rBtnText1 = { btn_x, btn_y + 6, btn_x + btn_w, btn_y + 30 };
    const char *status_header = state->macro_active ? "ATIVADO (F8)" : "PAUSADO [F8]";
    DrawTextA(memDC, status_header, -1, &rBtnText1, DT_CENTER | DT_SINGLELINE);

    SelectObject(memDC, hFontRegular);
    RECT rBtnText2 = { btn_x, btn_y + 32, btn_x + btn_w, btn_y + 52 };
    if (state->macro_active) {
        if (state->target_locked) {
            SetTextColor(memDC, COLOR_CYAN);
            DrawTextA(memDC, "EM COMBATE COM O ALVO", -1, &rBtnText2, DT_CENTER | DT_SINGLELINE);
        } else {
            SetTextColor(memDC, COLOR_YELLOW);
            DrawTextA(memDC, "BUSCANDO ALVO COM [TAB]...", -1, &rBtnText2, DT_CENTER | DT_SINGLELINE);
        }
    } else {
        SetTextColor(memDC, RGB(220, 170, 180));
        DrawTextA(memDC, "Clique ou aperte F8 para Iniciar", -1, &rBtnText2, DT_CENTER | DT_SINGLELINE);
    }

    // 4. Card 2: Status do Jogo & Alvo & Telemetria
    int aika_y = 106;
    int aika_h = 48;
    draw_rounded_card(memDC, btn_x, aika_y, btn_w, aika_h, COLOR_CARD_BG, COLOR_CARD_BORDER, 6);

    // Linha 1 do Card 2: Aika Client
    SelectObject(memDC, hFontRegular);
    SetTextColor(memDC, COLOR_TEXT_MUTED);
    RECT rAikaLabel = { btn_x + 10, aika_y + 4, btn_x + 100, aika_y + 24 };
    DrawTextA(memDC, "Aika Client:", -1, &rAikaLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HWND hAika = find_aika_window();
    SelectObject(memDC, hFontBold);
    RECT rAikaVal = { btn_x + 95, aika_y + 4, btn_x + btn_w - 10, aika_y + 24 };
    if (hAika) {
        SetTextColor(memDC, COLOR_GREEN);
        DrawTextA(memDC, "DETECTADO", -1, &rAikaVal, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(memDC, COLOR_YELLOW);
        DrawTextA(memDC, "PROCURANDO JOGO...", -1, &rAikaVal, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // Linha 2 do Card 2: Alvo e Mobs Eliminados
    SelectObject(memDC, hFontRegular);
    SetTextColor(memDC, COLOR_TEXT_MUTED);
    RECT rTargetLbl = { btn_x + 10, aika_y + 24, btn_x + 120, aika_y + 44 };
    DrawTextA(memDC, "Alvo:", -1, &rTargetLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDC, hFontBold);
    RECT rTargetVal = { btn_x + 42, aika_y + 24, btn_x + 130, aika_y + 44 };
    if (state->target_locked) {
        SetTextColor(memDC, COLOR_GREEN);
        DrawTextA(memDC, "TRAVADO", -1, &rTargetVal, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(memDC, COLOR_TEXT_MUTED);
        DrawTextA(memDC, "BUSCA", -1, &rTargetVal, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    char mobsBuf[32];
    snprintf(mobsBuf, sizeof(mobsBuf), "Mobs: %d", state->mobs_killed);
    SetTextColor(memDC, COLOR_TITLE);
    RECT rMobsVal = { btn_x + 140, aika_y + 24, btn_x + btn_w - 10, aika_y + 44 };
    DrawTextA(memDC, mobsBuf, -1, &rMobsVal, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // 5. Card 3: Módulos de Combate e Atalhos
    int mod_y = 162;
    int mod_h = 118;
    draw_rounded_card(memDC, btn_x, mod_y, btn_w, mod_h, COLOR_CARD_BG, COLOR_CARD_BORDER, 6);

    // Linha 1: Rotação de Ataque
    SelectObject(memDC, hFontRegular);
    SetTextColor(memDC, COLOR_TEXT_MUTED);
    RECT rRow1Lbl = { btn_x + 10, mod_y + 6, btn_x + 160, mod_y + 28 };
    DrawTextA(memDC, "Rotacao Skills:", -1, &rRow1Lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    char rotBuf[64];
    snprintf(rotBuf, sizeof(rotBuf), "[%c,%c,%c] + Basico [%c]",
        (char)config->key_skill1, (char)config->key_skill2, (char)config->key_skill3, (char)config->key_basic);
    SelectObject(memDC, hFontBold);
    SetTextColor(memDC, COLOR_TEXT_WHITE);
    RECT rRow1Val = { btn_x + 140, mod_y + 6, btn_x + btn_w - 10, mod_y + 28 };
    DrawTextA(memDC, rotBuf, -1, &rRow1Val, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Linha 2: Auto-Target [F9]
    SelectObject(memDC, hFontRegular);
    SetTextColor(memDC, COLOR_TEXT_MUTED);
    RECT rRow2Lbl = { btn_x + 10, mod_y + 34, btn_x + 180, mod_y + 56 };
    DrawTextA(memDC, "Auto-Target [F9]:", -1, &rRow2Lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_toggle(memDC, btn_x + btn_w - 65, mod_y + 34, 55, 20, state->auto_target);

    // Linha 3: Auto-Loot [F10]
    SelectObject(memDC, hFontRegular);
    SetTextColor(memDC, COLOR_TEXT_MUTED);
    RECT rRow3Lbl = { btn_x + 10, mod_y + 62, btn_x + 180, mod_y + 84 };
    DrawTextA(memDC, "Auto-Loot [F10]:", -1, &rRow3Lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_toggle(memDC, btn_x + btn_w - 65, mod_y + 62, 55, 20, state->auto_loot);

    // Linha 4: Auto-Cura [F7]
    SelectObject(memDC, hFontRegular);
    SetTextColor(memDC, COLOR_TEXT_MUTED);
    RECT rRow4Lbl = { btn_x + 10, mod_y + 90, btn_x + 180, mod_y + 112 };
    DrawTextA(memDC, "Auto-Cura [F7]:", -1, &rRow4Lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_toggle(memDC, btn_x + btn_w - 65, mod_y + 90, 55, 20, state->auto_heal);

    // 6. Card 4: Registro de Ação em Tempo Real
    int log_y = 288;
    int log_h = 54;
    draw_rounded_card(memDC, btn_x, log_y, btn_w, log_h, COLOR_CARD_BG, COLOR_CARD_BORDER, 6);

    SelectObject(memDC, hFontSmall);
    SetTextColor(memDC, COLOR_TITLE);
    RECT rLogTitle = { btn_x + 10, log_y + 4, btn_x + btn_w - 10, log_y + 18 };
    DrawTextA(memDC, "ULTIMA ATIVIDADE:", -1, &rLogTitle, DT_LEFT | DT_SINGLELINE);

    SelectObject(memDC, hFontRegular);
    SetTextColor(memDC, COLOR_TEXT_WHITE);
    RECT rLogText = { btn_x + 10, log_y + 20, btn_x + btn_w - 10, log_y + 48 };
    DrawTextA(memDC, state->last_action, -1, &rLogText, DT_LEFT | DT_WORDBREAK);

    // 7. Footer: Dica Importante
    int foot_y = 350;
    int foot_h = 46;
    draw_rounded_card(memDC, btn_x, foot_y, btn_w, foot_h, RGB(18, 22, 28), RGB(40, 48, 62), 6);

    SelectObject(memDC, hFontSmall);
    SetTextColor(memDC, COLOR_YELLOW);
    RECT rHint = { btn_x + 8, foot_y + 4, btn_x + btn_w - 8, foot_y + foot_h - 4 };
    char hintBuf[128];
    snprintf(hintBuf, sizeof(hintBuf), "DICA: Coloque 'Ataque Normal' (janela K) no Slot %c para o personagem correr ate o monstro!",
        (char)config->key_basic);
    DrawTextA(memDC, hintBuf, -1, &rHint, DT_CENTER | DT_WORDBREAK);

    // Copia o buffer de memória para a tela real
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
}

void ui_handle_click(HWND hwnd, int x, int y) {
    // 1. Botão Fechar [X] (canto superior direito)
    if (x >= HUD_WIDTH - 30 && x <= HUD_WIDTH - 4 && y >= 4 && y <= 28) {
        DestroyWindow(hwnd);
        return;
    }

    // 2. Header para arrastar a janela (qualquer clique na barra de título)
    if (y < 32) {
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return;
    }

    // 3. Botão Principal de Combate (F8)
    if (x >= 12 && x <= HUD_WIDTH - 12 && y >= 40 && y <= 98) {
        combat_toggle_macro();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    // 4. Toggle Auto-Target (F9)
    if (x >= HUD_WIDTH - 77 && x <= HUD_WIDTH - 22 && y >= 194 && y <= 218) {
        combat_toggle_target();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    // 5. Toggle Auto-Loot (F10)
    if (x >= HUD_WIDTH - 77 && x <= HUD_WIDTH - 22 && y >= 222 && y <= 246) {
        combat_toggle_loot();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    // 6. Toggle Auto-Cura (F7)
    if (x >= HUD_WIDTH - 77 && x <= HUD_WIDTH - 22 && y >= 250 && y <= 274) {
        combat_toggle_heal();
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
}
