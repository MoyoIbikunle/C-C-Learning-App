/*
 * ui.h - drawing helpers for C Quest: fonts, colours, buttons,
 * wrapped text and code panels. Built on Raylib.
 */
#ifndef UI_H
#define UI_H

#include "raylib.h"
#include <stdbool.h>

#define SCREEN_W 1200
#define SCREEN_H 720

/* Colours */
extern const Color COL_BG, COL_PANEL, COL_INK, COL_MUTED, COL_LINE;
extern const Color COL_ACCENT, COL_ACCENT_SOFT, COL_GOOD, COL_GOOD_SOFT;
extern const Color COL_BAD, COL_BAD_SOFT, COL_GOLD, COL_GOLD_SOFT;
extern const Color COL_CODE_BG, COL_CODE_INK, COL_CODE_NUM;

typedef enum { FONT_REGULAR, FONT_BOLD, FONT_MONO } FontKind;

typedef enum {
    BTN_PRIMARY,      /* filled accent colour */
    BTN_SECONDARY,    /* white with a border */
    BTN_QUIET         /* text only */
} ButtonStyle;

void loadFonts(void);
void unloadFonts(void);

/* Call at the start and end of each frame (handles the hand cursor) */
void uiBeginFrame(void);
void uiEndFrame(void);

bool mouseOver(Rectangle r);
bool clicked(Rectangle r);   /* also shows the hand cursor on hover */

void drawText(const char *text, float x, float y, float size, FontKind kind, Color colour);
float textWidth(const char *text, float size, FontKind kind);
void drawTextCentred(const char *text, Rectangle box, float size, FontKind kind, Color colour);

/* Wraps text to width. Parts inside `backticks` are drawn as code.
   Returns the height used. With draw = false it only measures. */
float drawRich(const char *text, float x, float y, float width, float size, Color colour, bool draw);

bool button(Rectangle r, const char *label, ButtonStyle style, bool enabled);

/* A code panel with line numbers. If clickable, returns the line clicked
   (from 1), else 0. markLine/markColour highlight one line (0 = none),
   and crossLine marks a line already picked wrongly (0 = none). */
int codePanel(Rectangle r, const char lines[][120], int count, bool clickable,
              int markLine, Color markColour, int crossLine);

/* Height a code panel needs for count lines */
float codePanelHeight(int count);

void panel(Rectangle r, Color fill);

#endif
