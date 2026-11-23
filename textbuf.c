/*
 * textbuf.c - the text behind the code editor: lines of text and a cursor.
 *
 * This file only changes the text; ui.c draws it and turns key presses
 * into calls here. Keeping it separate means it can be tested without a
 * window.
 */
#include <string.h>
#include "app.h"

static int lineLength(const TextBuffer *tb, int row)
{
    return (int)strlen(tb->lines[row]);
}

/* Number of spaces at the start of a line */
static int indentOf(const char *line)
{
    int n = 0;
    while (line[n] == ' ')
    {
        n++;
    }
    return n;
}

static void clampCursor(TextBuffer *tb)
{
    if (tb->row < 0) tb->row = 0;
    if (tb->row >= tb->count) tb->row = tb->count - 1;
    if (tb->col < 0) tb->col = 0;
    if (tb->col > lineLength(tb, tb->row)) tb->col = lineLength(tb, tb->row);
}

void tbClear(TextBuffer *tb)
{
    memset(tb, 0, sizeof(*tb));
    tb->count = 1;
}

void tbSetText(TextBuffer *tb, const char *text)
{
    tbClear(tb);
    tbInsertText(tb, text);
    tb->row = 0;
    tb->col = 0;
}

/* Copies all the text out, lines joined with \n. Returns its length. */
int tbGetText(const TextBuffer *tb, char *out, int size)
{
    int n = 0;
    out[0] = '\0';
    for (int r = 0; r < tb->count; r++)
    {
        int len = lineLength(tb, r);
        if (n + len + 2 > size)
        {
            break;
        }
        memcpy(out + n, tb->lines[r], len);
        n += len;
        out[n++] = '\n';
        out[n] = '\0';
    }
    return n;
}

void tbSetCursor(TextBuffer *tb, int row, int col)
{
    tb->row = row;
    tb->col = col;
    clampCursor(tb);
}

/* Types one character at the cursor. A } typed on an empty, indented line
   moves back one indent level so the braces line up. */
void tbInsertChar(TextBuffer *tb, char c)
{
    char *line = tb->lines[tb->row];
    int len = lineLength(tb, tb->row);
    if (len >= TB_COLS - 1 || c < 32 || c > 126)
    {
        return;
    }
    if (c == '}' && tb->col == len && indentOf(line) == len && len >= 4)
    {
        line[len - 4] = '\0';   /* dedent */
        len -= 4;
        tb->col = len;
    }
    memmove(line + tb->col + 1, line + tb->col, len - tb->col + 1);
    line[tb->col] = c;
    tb->col++;
}

/* Enter: splits the line at the cursor. The new line keeps the same
   indent, plus 4 more after a {. */
void tbNewline(TextBuffer *tb)
{
    if (tb->count >= TB_LINES)
    {
        return;
    }
    char *line = tb->lines[tb->row];
    int indent = indentOf(line);
    if (indent > tb->col)
    {
        indent = tb->col;
    }
    if (tb->col > 0 && line[tb->col - 1] == '{')
    {
        indent += 4;
    }
    if (indent > TB_COLS / 2)
    {
        indent = TB_COLS / 2;
    }

    /* Make room for the new line below */
    memmove(tb->lines[tb->row + 2], tb->lines[tb->row + 1], (size_t)(tb->count - tb->row - 1) * TB_COLS);
    char *next = tb->lines[tb->row + 1];
    memset(next, ' ', indent);
    next[indent] = '\0';
    strncat(next, line + tb->col, TB_COLS - 1 - indent);
    line[tb->col] = '\0';

    tb->count++;
    tb->row++;
    tb->col = indent;
}

/* Tab: spaces up to the next multiple of 4 */
void tbTab(TextBuffer *tb)
{
    do
    {
        tbInsertChar(tb, ' ');
    } while (tb->col % 4 != 0 && lineLength(tb, tb->row) < TB_COLS - 1);
}

void tbBackspace(TextBuffer *tb)
{
    char *line = tb->lines[tb->row];
    if (tb->col > 0)
    {
        memmove(line + tb->col - 1, line + tb->col, strlen(line + tb->col) + 1);
        tb->col--;
        return;
    }
    if (tb->row == 0)
    {
        return;
    }
    /* At the start of a line: join it onto the line above */
    char *prev = tb->lines[tb->row - 1];
    int prevLen = (int)strlen(prev);
    if (prevLen + lineLength(tb, tb->row) >= TB_COLS)
    {
        return;   /* wouldn't fit */
    }
    strcat(prev, line);
    memmove(tb->lines[tb->row], tb->lines[tb->row + 1], (size_t)(tb->count - tb->row - 1) * TB_COLS);
    tb->count--;
    tb->lines[tb->count][0] = '\0';
    tb->row--;
    tb->col = prevLen;
}

/* Delete key: like backspace from one place further on */
void tbDelete(TextBuffer *tb)
{
    if (tb->col < lineLength(tb, tb->row))
    {
        tb->col++;
        tbBackspace(tb);
    }
    else if (tb->row < tb->count - 1)
    {
        tb->row++;
        tb->col = 0;
        tbBackspace(tb);
    }
}

/* Pasting or loading: \n starts a new line, a tab becomes spaces, \r is skipped */
void tbInsertText(TextBuffer *tb, const char *text)
{
    for (const char *p = text; *p != '\0'; p++)
    {
        if (*p == '\n')
        {
            /* plain split, without auto-indent, so pasted code keeps its own spaces */
            if (tb->count >= TB_LINES)
            {
                return;
            }
            char *line = tb->lines[tb->row];
            memmove(tb->lines[tb->row + 2], tb->lines[tb->row + 1], (size_t)(tb->count - tb->row - 1) * TB_COLS);
            strcpy(tb->lines[tb->row + 1], line + tb->col);
            line[tb->col] = '\0';
            tb->count++;
            tb->row++;
            tb->col = 0;
        }
        else if (*p == '\t')
        {
            tbTab(tb);
        }
        else if (*p != '\r')
        {
            /* add without the } dedent, which is only for typing */
            char *line = tb->lines[tb->row];
            int len = lineLength(tb, tb->row);
            if (len < TB_COLS - 1 && *p >= 32 && *p <= 126)
            {
                memmove(line + tb->col + 1, line + tb->col, len - tb->col + 1);
                line[tb->col++] = *p;
            }
        }
    }
}

/* Arrow keys. Left at the start of a line goes to the end of the one above,
   and right at the end goes to the start of the next. */
void tbMove(TextBuffer *tb, int dRow, int dCol)
{
    if (dCol < 0 && tb->col == 0 && tb->row > 0)
    {
        tb->row--;
        tb->col = lineLength(tb, tb->row);
        return;
    }
    if (dCol > 0 && tb->col == lineLength(tb, tb->row) && tb->row < tb->count - 1)
    {
        tb->row++;
        tb->col = 0;
        return;
    }
    tb->row += dRow;
    tb->col += dCol;
    clampCursor(tb);
}

void tbHome(TextBuffer *tb)
{
    /* first press goes to the code (after the indent), second to column 0 */
    int indent = indentOf(tb->lines[tb->row]);
    tb->col = (tb->col == indent) ? 0 : indent;
}

void tbEnd(TextBuffer *tb)
{
    tb->col = lineLength(tb, tb->row);
}
