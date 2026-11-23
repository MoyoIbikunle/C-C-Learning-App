/*
 * ui.c - drawing helpers for C Quest (fonts, buttons, text, code panels).
 *
 * Raylib is "immediate mode": every frame we redraw everything, and a
 * button is just a rectangle we draw and then check for a click.
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"

const Color COL_BG          = { 246, 244, 239, 255 };   /* warm off-white */
const Color COL_PANEL       = { 255, 255, 255, 255 };
const Color COL_INK         = {  31,  36,  51, 255 };
const Color COL_MUTED       = { 110, 116, 132, 255 };
const Color COL_LINE        = { 223, 220, 212, 255 };
const Color COL_ACCENT      = {  76,  91, 212, 255 };   /* indigo */
const Color COL_ACCENT_SOFT = { 230, 233, 251, 255 };
const Color COL_GOOD        = {  38, 145,  96, 255 };
const Color COL_GOOD_SOFT   = { 222, 243, 232, 255 };
const Color COL_BAD         = { 205,  76,  70, 255 };
const Color COL_BAD_SOFT    = { 251, 231, 229, 255 };
const Color COL_GOLD        = { 214, 155,  32, 255 };
const Color COL_GOLD_SOFT   = { 252, 242, 214, 255 };
const Color COL_CODE_BG     = {  30,  33,  48, 255 };
const Color COL_CODE_INK    = { 226, 230, 240, 255 };
const Color COL_CODE_NUM    = { 104, 112, 140, 255 };

static Font fonts[3];
static bool fontLoaded[3];
static bool wantHand;   /* something under the mouse is clickable this frame */
static bool blocked;    /* a pop-up is open, so nothing underneath reacts */
static bool wantText;   /* the mouse is over the code editor */

/* Loads a font big, with mipmaps, so it stays sharp when drawn smaller.
   Falls back to Raylib's built-in font if the file is missing. */
static void loadOne(FontKind kind, const char *path, int size)
{
    if (FileExists(path))
    {
        fonts[kind] = LoadFontEx(path, size, NULL, 0);
        fontLoaded[kind] = fonts[kind].texture.id != GetFontDefault().texture.id;
    }
    if (!fontLoaded[kind])
    {
        fonts[kind] = GetFontDefault();
        return;
    }
    GenTextureMipmaps(&fonts[kind].texture);
    SetTextureFilter(fonts[kind].texture, TEXTURE_FILTER_TRILINEAR);
}

void loadFonts(void)
{
    loadOne(FONT_REGULAR, "assets/Poppins-Regular.ttf", 48);
    loadOne(FONT_BOLD, "assets/Poppins-SemiBold.ttf", 64);
    loadOne(FONT_MONO, "assets/IBMPlexMono-Regular.ttf", 40);
}

void unloadFonts(void)
{
    for (int i = 0; i < 3; i++)
    {
        if (fontLoaded[i])
        {
            UnloadFont(fonts[i]);
        }
    }
}

void uiBeginFrame(void)
{
    wantHand = false;
    wantText = false;
}

void uiEndFrame(void)
{
    SetMouseCursor(wantHand ? MOUSE_CURSOR_POINTING_HAND : (wantText ? MOUSE_CURSOR_IBEAM : MOUSE_CURSOR_DEFAULT));
}

void uiBlock(bool on)
{
    blocked = on;
}

bool uiBlocked(void)
{
    return blocked;
}

bool mouseOver(Rectangle r)
{
    return !blocked && CheckCollisionPointRec(GetMousePosition(), r);
}

bool clicked(Rectangle r)
{
    if (!mouseOver(r))
    {
        return false;
    }
    wantHand = true;
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* A touch of extra letter spacing for the normal fonts */
static float spacingFor(FontKind kind)
{
    return kind == FONT_MONO ? 0.0f : 0.3f;
}

void drawText(const char *text, float x, float y, float size, FontKind kind, Color colour)
{
    DrawTextEx(fonts[kind], text, (Vector2){ x, y }, size, spacingFor(kind), colour);
}

float textWidth(const char *text, float size, FontKind kind)
{
    return MeasureTextEx(fonts[kind], text, size, spacingFor(kind)).x;
}

void drawTextCentred(const char *text, Rectangle box, float size, FontKind kind, Color colour)
{
    Vector2 m = MeasureTextEx(fonts[kind], text, size, spacingFor(kind));
    drawText(text, box.x + (box.width - m.x) / 2, box.y + (box.height - m.y) / 2, size, kind, colour);
}

/* Word-wrapping with `code` parts. The text is read word by word; a
   backtick switches between normal text and code. */
float drawRich(const char *text, float x, float y, float width, float size, Color colour, bool draw)
{
    float lineH = size * 1.5f;
    float codeSize = size * 0.9f;
    float cx = x, cy = y;
    bool inCode = false;
    char word[256];

    const char *p = text;
    while (*p != '\0')
    {
        if (*p == '`')
        {
            inCode = !inCode;
            cx += 4;   /* a little room either side of a code chip */
            p++;
            continue;
        }
        if (*p == ' ')
        {
            cx += textWidth(" ", inCode ? codeSize : size, inCode ? FONT_MONO : FONT_REGULAR);
            p++;
            continue;
        }

        FontKind kind = inCode ? FONT_MONO : FONT_REGULAR;
        float fs = inCode ? codeSize : size;

        /* Take a whole `code` part at once so it isn't split across lines,
           unless it's too long for a line; otherwise take one word. */
        int n = 0;
        if (inCode)
        {
            while (p[n] != '\0' && p[n] != '`' && n < (int)sizeof(word) - 1)
            {
                word[n] = p[n];
                n++;
            }
            word[n] = '\0';
            if (textWidth(word, fs, kind) > width)
            {
                n = 0;
            }
        }
        if (n == 0)
        {
            while (p[n] != '\0' && p[n] != ' ' && p[n] != '`' && n < (int)sizeof(word) - 1)
            {
                word[n] = p[n];
                n++;
            }
            word[n] = '\0';
        }
        p += n;

        float w = textWidth(word, fs, kind);
        if (cx + w > x + width && cx > x)
        {
            cx = x;
            cy += lineH;
        }
        if (draw)
        {
            if (inCode)
            {
                DrawRectangleRounded((Rectangle){ cx - 3, cy + size * 0.12f, w + 6, size * 1.22f },
                                     0.3f, 4, COL_ACCENT_SOFT);
                drawText(word, cx, cy + (size - fs) * 0.6f + size * 0.12f, fs, FONT_MONO, COL_ACCENT);
            }
            else
            {
                drawText(word, cx, cy, size, FONT_REGULAR, colour);
            }
        }
        cx += w;
    }
    return cy + lineH - y;
}

void panel(Rectangle r, Color fill)
{
    DrawRectangleRounded((Rectangle){ r.x, r.y + 3, r.width, r.height }, 0.06f, 8, (Color){ 0, 0, 0, 14 });
    DrawRectangleRounded(r, 0.06f, 8, fill);
}

bool button(Rectangle r, const char *label, ButtonStyle style, bool enabled)
{
    bool hover = enabled && mouseOver(r);
    bool hit = enabled && clicked(r);

    Color fill, ink;
    if (!enabled)
    {
        fill = (style == BTN_QUIET) ? (Color){ 0, 0, 0, 0 } : COL_LINE;
        ink = COL_MUTED;
    }
    else if (style == BTN_PRIMARY)
    {
        fill = hover ? (Color){ 60, 74, 190, 255 } : COL_ACCENT;
        ink = WHITE;
    }
    else if (style == BTN_SECONDARY)
    {
        fill = hover ? COL_ACCENT_SOFT : COL_PANEL;
        ink = COL_ACCENT;
    }
    else
    {
        fill = hover ? COL_ACCENT_SOFT : (Color){ 0, 0, 0, 0 };
        ink = COL_ACCENT;
    }

    DrawRectangleRounded(r, 0.35f, 8, fill);
    if (style == BTN_SECONDARY && enabled)
    {
        DrawRectangleRoundedLinesEx(r, 0.35f, 8, 1.5f, COL_ACCENT);
    }
    drawTextCentred(label, r, 19, FONT_BOLD, ink);
    return hit;
}

/* Biggest font (up to 19) that fits the widest line in the panel */
static float codeFontSize(const char lines[][120], int count, float width)
{
    float size = 19;
    for (int i = 0; i < count; i++)
    {
        float w = textWidth(lines[i], 19, FONT_MONO);
        if (w > 0 && w * size / 19 > width)
        {
            size = 19 * width / w;
        }
    }
    return size < 12 ? 12 : size;
}

float codePanelHeight(int count)
{
    return 28.0f + count * 27.0f;
}

int codePanel(Rectangle r, const char lines[][120], int count, bool clickable,
              int markLine, Color markColour, int crossLine)
{
    const float lineH = 27, numW = 44, pad = 14;
    int hit = 0;

    DrawRectangleRounded(r, 0.04f, 8, COL_CODE_BG);
    float size = codeFontSize(lines, count, r.width - numW - pad * 2);

    for (int i = 0; i < count; i++)
    {
        Rectangle row = { r.x + 6, r.y + pad + i * lineH, r.width - 12, lineH };
        int lineNo = i + 1;

        if (lineNo == markLine)
        {
            DrawRectangleRounded(row, 0.25f, 6, markColour);
        }
        else if (lineNo == crossLine)
        {
            DrawRectangleRounded(row, 0.25f, 6, (Color){ 205, 76, 70, 90 });
        }
        else if (clickable && mouseOver(row))
        {
            DrawRectangleRounded(row, 0.25f, 6, (Color){ 255, 255, 255, 28 });
        }
        if (clickable && clicked(row))
        {
            hit = lineNo;
        }

        char num[12];
        snprintf(num, sizeof(num), "%2d", lineNo);
        float ty = row.y + (lineH - size) / 2 - 1;
        drawText(num, r.x + pad, ty, size, FONT_MONO, COL_CODE_NUM);
        drawCodeLine(lines[i], r.x + pad + numW, ty, size);
    }
    return hit;
}

/* ---------- Coloured code ---------- */

static const Color CODE_KEYWORD = { 199, 146, 234, 255 };
static const Color CODE_STRING  = { 195, 232, 141, 255 };
static const Color CODE_NUMBER  = { 247, 140, 108, 255 };
static const Color CODE_COMMENT = { 112, 120, 150, 255 };
static const Color CODE_PREPROC = { 137, 221, 255, 255 };

static const char *KEYWORDS[] = {
    "int", "char", "float", "double", "void", "bool", "long", "short", "unsigned", "const", "static",
    "struct", "class", "public", "private", "return", "if", "else", "for", "while", "do", "switch",
    "case", "default", "break", "continue", "new", "delete", "nullptr", "NULL", "true", "false",
    "sizeof", "using", "namespace", "typedef", "enum", NULL
};

static bool isKeyword(const char *word, int len)
{
    for (int i = 0; KEYWORDS[i] != NULL; i++)
    {
        if ((int)strlen(KEYWORDS[i]) == len && strncmp(KEYWORDS[i], word, len) == 0)
        {
            return true;
        }
    }
    return false;
}

/* Draws one line of code, colouring keywords, strings, numbers, comments
   and #include lines. The font is monospaced, so character n is always
   at x + n * (width of one character). */
void drawCodeLine(const char *line, float x, float y, float size)
{
    float charW = textWidth("M", size, FONT_MONO);
    char piece[TB_COLS_UI];
    int len = (int)strlen(line);
    int i = 0;

    int first = 0;
    while (line[first] == ' ')
    {
        first++;
    }

    while (i < len)
    {
        int start = i;
        Color colour = COL_CODE_INK;
        if (line[first] == '#')
        {
            i = len;                                   /* the whole line */
            colour = CODE_PREPROC;
        }
        else if (line[i] == '/' && line[i + 1] == '/')
        {
            i = len;                                   /* comment to the end */
            colour = CODE_COMMENT;
        }
        else if (line[i] == '"' || line[i] == '\'')
        {
            char quote = line[i++];
            while (i < len && line[i] != quote)
            {
                i += (line[i] == '\\' && i + 1 < len) ? 2 : 1;   /* skip \n, \" and so on */
            }
            if (i < len) i++;
            colour = CODE_STRING;
        }
        else if (isdigit((unsigned char)line[i]))
        {
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '.')) i++;
            colour = CODE_NUMBER;
        }
        else if (isalpha((unsigned char)line[i]) || line[i] == '_')
        {
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            if (isKeyword(line + start, i - start)) colour = CODE_KEYWORD;
        }
        else
        {
            i++;                                       /* a symbol or space */
        }

        int n = i - start;
        if (n > (int)sizeof(piece) - 1) n = (int)sizeof(piece) - 1;
        memcpy(piece, line + start, n);
        piece[n] = '\0';
        drawText(piece, x + start * charW, y, size, FONT_MONO, colour);
    }
}

/* ---------- The code editor ---------- */

#define ED_SIZE 18
#define ED_LINE 25

/* Keys that repeat when held down */
static bool keyHit(int key)
{
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

void codeEditor(Rectangle r, TextBuffer *tb, int *scroll, bool *focused)
{
    const float pad = 12, numW = 46;
    float charW = textWidth("M", ED_SIZE, FONT_MONO);
    int visible = (int)((r.height - pad * 2) / ED_LINE);
    float textX = r.x + pad + numW;

    /* Click to focus and place the cursor; click elsewhere to unfocus */
    if (!blocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        *focused = CheckCollisionPointRec(GetMousePosition(), r);
        if (*focused)
        {
            Vector2 m = GetMousePosition();
            int row = *scroll + (int)((m.y - r.y - pad) / ED_LINE);
            int col = (int)((m.x - textX) / charW + 0.5f);
            tbSetCursor(tb, row, col);
        }
    }
    if (!blocked && mouseOver(r))
    {
        *scroll -= (int)(GetMouseWheelMove() * 3);
    }

    /* Typing */
    if (*focused && !blocked)
    {
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        int c = GetCharPressed();
        while (c > 0)
        {
            if (!ctrl)
            {
                tbInsertChar(tb, (char)c);
            }
            c = GetCharPressed();
        }
        if (ctrl && IsKeyPressed(KEY_V))
        {
            const char *clip = GetClipboardText();
            if (clip != NULL) tbInsertText(tb, clip);
        }
        if (ctrl && IsKeyPressed(KEY_C))
        {
            static char all[TB_LINES * TB_COLS];
            tbGetText(tb, all, sizeof(all));
            SetClipboardText(all);   /* copies all the code */
        }
        if (!ctrl && (keyHit(KEY_ENTER) || keyHit(KEY_KP_ENTER))) tbNewline(tb);
        if (keyHit(KEY_BACKSPACE)) tbBackspace(tb);
        if (keyHit(KEY_DELETE))    tbDelete(tb);
        if (keyHit(KEY_TAB))       tbTab(tb);
        if (keyHit(KEY_LEFT))      tbMove(tb, 0, -1);
        if (keyHit(KEY_RIGHT))     tbMove(tb, 0, 1);
        if (keyHit(KEY_UP))        tbMove(tb, -1, 0);
        if (keyHit(KEY_DOWN))      tbMove(tb, 1, 0);
        if (keyHit(KEY_PAGE_UP))   tbMove(tb, -visible, 0);
        if (keyHit(KEY_PAGE_DOWN)) tbMove(tb, visible, 0);
        if (IsKeyPressed(KEY_HOME)) tbHome(tb);
        if (IsKeyPressed(KEY_END))  tbEnd(tb);

        /* keep the cursor on screen */
        if (tb->row < *scroll) *scroll = tb->row;
        if (tb->row >= *scroll + visible) *scroll = tb->row - visible + 1;
    }
    if (*scroll > tb->count - visible) *scroll = tb->count - visible;
    if (*scroll < 0) *scroll = 0;

    /* Drawing */
    DrawRectangleRounded(r, 0.03f, 8, COL_CODE_BG);
    if (*focused)
    {
        DrawRectangleRoundedLinesEx(r, 0.03f, 8, 2, COL_ACCENT);
    }
    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
    for (int i = 0; i < visible && *scroll + i < tb->count; i++)
    {
        int row = *scroll + i;
        float y = r.y + pad + i * ED_LINE;
        if (*focused && row == tb->row)
        {
            DrawRectangle((int)r.x + 4, (int)y, (int)r.width - 8, ED_LINE, (Color){ 255, 255, 255, 14 });
        }
        char num[12];
        snprintf(num, sizeof(num), "%3d", row + 1);
        drawText(num, r.x + pad - 6, y + 3, ED_SIZE - 2, FONT_MONO, COL_CODE_NUM);
        drawCodeLine(tb->lines[row], textX, y + 2, ED_SIZE);
    }
    /* Blinking cursor */
    if (*focused && tb->row >= *scroll && tb->row < *scroll + visible && (int)(GetTime() * 2) % 2 == 0)
    {
        float cx = textX + tb->col * charW;
        float cy = r.y + pad + (tb->row - *scroll) * ED_LINE;
        DrawRectangle((int)cx, (int)cy + 2, 2, ED_LINE - 4, WHITE);
    }
    EndScissorMode();

    /* A scroll hint when there's more code than fits */
    if (tb->count > visible)
    {
        float barH = r.height * visible / tb->count;
        float barY = r.y + (r.height - barH) * (*scroll) / (tb->count - visible);
        DrawRectangleRounded((Rectangle){ r.x + r.width - 8, barY, 4, barH }, 1, 4, (Color){ 255, 255, 255, 50 });
    }
    if (mouseOver(r))
    {
        wantText = true;
    }
}
