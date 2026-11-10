/*
 * main.c - C Quest: an interactive app for learning C and C++.
 *
 * Screens: sign in -> learning path -> lesson (tutorial cards, then
 * questions) -> results. Plus a badges screen.
 * Progress is saved to learners.txt after every finished lesson.
 */
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#include "app.h"
#include "ui.h"

#define COURSE_FILE "content/course.txt"
#define LEARNERS_FILE "learners.txt"

typedef enum {
    SCREEN_SIGN_IN,
    SCREEN_PATH,
    SCREEN_CARDS,
    SCREEN_QUIZ,
    SCREEN_RESULTS,
    SCREEN_BADGES,
    SCREEN_ERROR
} Screen;

/* ---------- App state ---------- */

static Course course;
static char courseError[200] = "";
static Learner learners[MAX_LEARNERS];
static int learnerCount = 0;
static int me = -1;                 /* index of the signed-in learner */
static Screen screen = SCREEN_SIGN_IN;

/* Sign in */
static char typedName[NAME_LEN] = "";
static char signInError[100] = "";

/* The lesson being taken */
static int lessonIndex = 0;
static int cardIndex = 0;

/* The quiz */
static int questionIndex = 0;
static int attempt = 1;             /* 1 = first try, 2 = second try */
static int wrongPick = 0;           /* option or line picked wrongly on the first try */
static bool answered = false;       /* this question is finished */
static bool gotItRight = false;
static int pointsThisQuestion = 0;
static int pointsThisLesson = 0;

/* Results */
static int resultPercent = 0;
static unsigned newBadges = 0;
static bool nextJustUnlocked = false;

/* Path screen: lesson under the mouse, for the hint at the bottom */
static int hoverLesson = -1;

static Learner *current(void)
{
    return &learners[me];
}

static void save(void)
{
    saveLearners(LEARNERS_FILE, learners, learnerCount);
}

/* ---------- Header bar (shown once signed in) ---------- */

static void drawBadgeMedal(float cx, float cy, float radius, int badge, bool earned);

static void header(void)
{
    DrawRectangle(0, 0, SCREEN_W, 64, COL_PANEL);
    DrawRectangle(0, 64, SCREEN_W, 1, COL_LINE);

    /* Logo */
    DrawRectangleRounded((Rectangle){ 24, 14, 36, 36 }, 0.3f, 6, COL_ACCENT);
    drawTextCentred("C", (Rectangle){ 24, 14, 36, 38 }, 24, FONT_BOLD, WHITE);
    drawText("C Quest", 70, 17, 24, FONT_BOLD, COL_INK);

    /* Tabs: only Learn works in this version */
    const char *tabs[3] = { "Learn", "Practise", "Teach" };
    float x = 230;
    for (int i = 0; i < 3; i++)
    {
        float w = textWidth(tabs[i], 18, FONT_BOLD);
        drawText(tabs[i], x, 21, 18, FONT_BOLD, i == 0 ? COL_ACCENT : COL_MUTED);
        if (i == 0)
        {
            DrawRectangle((int)x, 61, (int)w, 3, COL_ACCENT);
        }
        else
        {
            Rectangle soon = { x + w + 6, 22, 44, 20 };
            DrawRectangleRounded(soon, 0.5f, 6, COL_BG);
            drawTextCentred("soon", soon, 12, FONT_REGULAR, COL_MUTED);
            w += 50;
        }
        x += w + 34;
    }

    /* Points and badges */
    char text[40];
    snprintf(text, sizeof(text), "%d points", totalPoints(current()));
    float pw = textWidth(text, 17, FONT_BOLD) + 40;
    Rectangle points = { 760, 16, pw, 32 };
    DrawRectangleRounded(points, 0.5f, 8, COL_GOLD_SOFT);
    DrawCircle((int)points.x + 17, (int)points.y + 16, 7, COL_GOLD);
    drawText(text, points.x + 30, points.y + 5, 17, FONT_BOLD, (Color){ 140, 96, 10, 255 });

    snprintf(text, sizeof(text), "%d / %d badges", badgeCount(current()), BADGE_COUNT);
    Rectangle badges = { points.x + pw + 10, 16, textWidth(text, 17, FONT_BOLD) + 40, 32 };
    DrawRectangleRounded(badges, 0.5f, 8, mouseOver(badges) ? COL_ACCENT : COL_ACCENT_SOFT);
    drawBadgeMedal(badges.x + 16, badges.y + 16, 8, BADGE_FIRST_STEPS, true);
    drawText(text, badges.x + 30, badges.y + 5, 17, FONT_BOLD, mouseOver(badges) ? WHITE : COL_ACCENT);
    if (clicked(badges))
    {
        screen = SCREEN_BADGES;
    }

    /* Name and sign out */
    drawText(current()->name, SCREEN_W - 24 - textWidth(current()->name, 17, FONT_BOLD), 9, 17, FONT_BOLD, COL_INK);
    Rectangle out = { SCREEN_W - 94, 34, 72, 22 };
    drawText("Sign out", out.x + 6, out.y + 1, 14, FONT_REGULAR, mouseOver(out) ? COL_ACCENT : COL_MUTED);
    if (clicked(out))
    {
        me = -1;
        typedName[0] = '\0';
        signInError[0] = '\0';
        screen = SCREEN_SIGN_IN;
    }
}

/* ---------- Sign in ---------- */

static void signIn(int index)
{
    me = index;
    screen = SCREEN_PATH;
}

static void tryNewName(void)
{
    if (!validLearnerName(typedName))
    {
        snprintf(signInError, sizeof(signInError), "Use one word: letters, numbers, - or '");
        return;
    }
    int found = findLearner(learners, learnerCount, typedName);
    if (found >= 0)
    {
        signIn(found);   /* that name already exists, so carry on as them */
        return;
    }
    if (learnerCount >= MAX_LEARNERS)
    {
        snprintf(signInError, sizeof(signInError), "This computer already has %d learners", MAX_LEARNERS);
        return;
    }
    newLearner(&learners[learnerCount], typedName);
    learnerCount++;
    save();
    signIn(learnerCount - 1);
}

static void screenSignIn(void)
{
    /* Left side: welcome */
    DrawRectangle(0, 0, 520, SCREEN_H, COL_ACCENT);
    DrawRectangleRounded((Rectangle){ 70, 110, 64, 64 }, 0.3f, 8, WHITE);
    drawTextCentred("C", (Rectangle){ 70, 110, 64, 68 }, 42, FONT_BOLD, COL_ACCENT);
    drawText("C Quest", 70, 200, 52, FONT_BOLD, WHITE);
    drawRich("Learn C and C++ one small step at a time. Read a short card, answer a few questions, "
             "earn points and badges, and unlock the next lesson.",
             72, 280, 380, 20, (Color){ 230, 233, 251, 255 }, true);

    const char *steps[3] = { "Beginner: the basics", "Intermediate: problem solving", "Advanced: pointers, memory, classes" };
    for (int i = 0; i < 3; i++)
    {
        DrawCircle(84, 459 + i * 44, 12, (Color){ 255, 255, 255, 60 });
        char n[4];
        snprintf(n, sizeof(n), "%d", i + 1);
        drawTextCentred(n, (Rectangle){ 72, 447 + i * 44, 24, 26 }, 15, FONT_BOLD, WHITE);
        drawText(steps[i], 108, 448 + i * 44, 18, FONT_REGULAR, WHITE);
    }

    /* Right side: pick a learner or type a new name */
    float x = 600;
    drawText("Who's learning?", x, 90, 34, FONT_BOLD, COL_INK);

    if (learnerCount > 0)
    {
        drawText("Pick your name", x, 150, 17, FONT_REGULAR, COL_MUTED);
        int shown = learnerCount < 8 ? learnerCount : 8;
        for (int i = 0; i < shown; i++)
        {
            Rectangle r = { x + (i % 2) * 260, 180 + (i / 2) * 62, 248, 52 };
            panel(r, mouseOver(r) ? COL_ACCENT_SOFT : COL_PANEL);
            DrawCircle((int)r.x + 28, (int)r.y + 26, 16, COL_ACCENT_SOFT);
            char initial[2] = { learners[i].name[0], '\0' };
            drawTextCentred(initial, (Rectangle){ r.x + 12, r.y + 10, 32, 34 }, 18, FONT_BOLD, COL_ACCENT);
            drawText(learners[i].name, r.x + 54, r.y + 6, 18, FONT_BOLD, COL_INK);
            char pts[30];
            snprintf(pts, sizeof(pts), "%d points", totalPoints(&learners[i]));
            drawText(pts, r.x + 54, r.y + 28, 14, FONT_REGULAR, COL_MUTED);
            if (clicked(r))
            {
                signIn(i);
            }
        }
    }

    int rows = learnerCount < 8 ? (learnerCount + 1) / 2 : 4;
    float formY = learnerCount > 0 ? 180 + rows * 62 + 30 : 170;
    drawText(learnerCount > 0 ? "Or start as someone new" : "Type your name to start",
             x, formY, 17, FONT_REGULAR, COL_MUTED);

    /* Typing */
    int key = GetCharPressed();
    while (key > 0)
    {
        size_t len = strlen(typedName);
        if (key >= 32 && key < 127 && len < NAME_LEN - 1)
        {
            typedName[len] = (char)key;
            typedName[len + 1] = '\0';
            signInError[0] = '\0';
        }
        key = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && typedName[0] != '\0')
    {
        typedName[strlen(typedName) - 1] = '\0';
    }

    Rectangle box = { x, formY + 30, 360, 48 };
    DrawRectangleRounded(box, 0.25f, 8, COL_PANEL);
    DrawRectangleRoundedLinesEx(box, 0.25f, 8, 2, COL_ACCENT);
    if (typedName[0] == '\0')
    {
        drawText("Your name", box.x + 16, box.y + 12, 19, FONT_REGULAR, COL_LINE);
    }
    drawText(typedName, box.x + 16, box.y + 12, 19, FONT_REGULAR, COL_INK);
    if ((int)(GetTime() * 2) % 2 == 0)
    {
        float cx = box.x + 18 + textWidth(typedName, 19, FONT_REGULAR);
        DrawRectangle((int)cx, (int)box.y + 12, 2, 24, COL_ACCENT);
    }

    Rectangle go = { x + 372, formY + 30, 120, 48 };
    if (button(go, "Start", BTN_PRIMARY, typedName[0] != '\0') || (IsKeyPressed(KEY_ENTER) && typedName[0] != '\0'))
    {
        tryNewName();
    }
    if (signInError[0] != '\0')
    {
        drawText(signInError, x, formY + 90, 16, FONT_REGULAR, COL_BAD);
    }
}

/* ---------- Learning path ---------- */

static void startLesson(int index)
{
    lessonIndex = index;
    cardIndex = 0;
    screen = SCREEN_CARDS;
}

static void startQuiz(void)
{
    questionIndex = 0;
    attempt = 1;
    wrongPick = 0;
    answered = false;
    gotItRight = false;
    pointsThisQuestion = 0;
    pointsThisLesson = 0;
    screen = SCREEN_QUIZ;
}

static void lessonNode(Rectangle r, int i)
{
    const Lesson *lesson = &course.lessons[i];
    const Learner *l = current();
    bool ready = lessonReady(&course, i);
    bool open = lessonUnlocked(l, &course, i);
    bool done = lessonPassed(l, i);
    bool hover = mouseOver(r);

    if (hover)
    {
        hoverLesson = i;
    }

    Color fill = COL_PANEL;
    if (!ready || !open)
    {
        fill = (Color){ 250, 249, 246, 255 };
    }
    else if (hover)
    {
        fill = COL_ACCENT_SOFT;
    }
    panel(r, fill);
    if (open && !done)
    {
        DrawRectangleRoundedLinesEx(r, 0.06f, 8, 2, COL_ACCENT);   /* the next one to do */
    }

    /* Number circle: tick when passed */
    Vector2 c = { r.x + 30, r.y + 30 };
    Color circle = done ? COL_GOOD : (open ? COL_ACCENT : COL_LINE);
    DrawCircleV(c, 16, circle);
    char num[4];
    snprintf(num, sizeof(num), "%d", lesson->number);
    if (done)
    {
        DrawLineEx((Vector2){ c.x - 7, c.y }, (Vector2){ c.x - 2, c.y + 6 }, 3, WHITE);
        DrawLineEx((Vector2){ c.x - 2, c.y + 6 }, (Vector2){ c.x + 8, c.y - 6 }, 3, WHITE);
    }
    else
    {
        drawTextCentred(num, (Rectangle){ c.x - 16, c.y - 15, 32, 32 }, 16, FONT_BOLD, open ? WHITE : COL_MUTED);
    }

    Color titleInk = (ready && open) ? COL_INK : COL_MUTED;
    drawText(lesson->title, r.x + 56, r.y + 17, 18, FONT_BOLD, titleInk);
    drawRich(lesson->summary, r.x + 18, r.y + 54, r.width - 36, 14, COL_MUTED, true);

    /* Status pill */
    char status[30];
    Color pillFill, pillInk;
    if (!ready)
    {
        snprintf(status, sizeof(status), "Coming soon");
        pillFill = COL_BG;
        pillInk = COL_MUTED;
    }
    else if (done)
    {
        snprintf(status, sizeof(status), "Passed  %d%%", l->bestPercent[i]);
        pillFill = COL_GOOD_SOFT;
        pillInk = COL_GOOD;
    }
    else if (!open)
    {
        snprintf(status, sizeof(status), "Locked");
        pillFill = COL_BG;
        pillInk = COL_MUTED;
    }
    else if (l->bestPercent[i] >= 0)
    {
        snprintf(status, sizeof(status), "Try again  %d%%", l->bestPercent[i]);
        pillFill = COL_GOLD_SOFT;
        pillInk = (Color){ 140, 96, 10, 255 };
    }
    else
    {
        snprintf(status, sizeof(status), "Start");
        pillFill = COL_ACCENT;
        pillInk = WHITE;
    }
    Rectangle pill = { r.x + 18, r.y + r.height - 38, textWidth(status, 14, FONT_BOLD) + 24, 26 };
    DrawRectangleRounded(pill, 0.5f, 8, pillFill);
    drawTextCentred(status, pill, 14, FONT_BOLD, pillInk);

    if (open && clicked(r))
    {
        startLesson(i);
    }
}

static void screenPath(void)
{
    hoverLesson = -1;
    header();

    const float nodeW = 252, nodeH = 142, gap = 18, left = 60;
    for (int level = 0; level < LEVELS; level++)
    {
        float y = 86 + level * 196;
        drawText(LEVEL_NAMES[level], left, y, 20, FONT_BOLD, COL_INK);

        int col = 0;
        for (int i = 0; i < course.lessonCount; i++)
        {
            if (course.lessons[i].level != level)
            {
                continue;
            }
            Rectangle r = { left + col * (nodeW + gap), y + 34, nodeW, nodeH };
            if (col > 0)
            {
                DrawRectangle((int)(r.x - gap), (int)(r.y + nodeH / 2 - 1), (int)gap, 3, COL_LINE);
            }
            lessonNode(r, i);
            col++;
        }
    }

    /* Hint for the lesson under the mouse */
    const char *hint = "Pass a lesson with 70% or more to unlock the next one.";
    char text[120];
    if (hoverLesson >= 0 && !lessonReady(&course, hoverLesson))
    {
        hint = "This lesson is being written. It'll appear here when it's ready.";
    }
    else if (hoverLesson > 0 && !lessonUnlocked(current(), &course, hoverLesson))
    {
        snprintf(text, sizeof(text), "Pass lesson %d (%s) to unlock this one.",
                 hoverLesson, course.lessons[hoverLesson - 1].title);
        hint = text;
    }
    drawText(hint, left, SCREEN_H - 40, 16, FONT_REGULAR, COL_MUTED);
}

/* ---------- Lesson top bar ---------- */

static void lessonBar(const char *step)
{
    DrawRectangle(0, 0, SCREEN_W, 64, COL_PANEL);
    DrawRectangle(0, 64, SCREEN_W, 1, COL_LINE);
    if (button((Rectangle){ 20, 14, 140, 36 }, "< Path", BTN_QUIET, true))
    {
        screen = SCREEN_PATH;
    }
    char title[100];
    snprintf(title, sizeof(title), "Lesson %d: %s", course.lessons[lessonIndex].number,
             course.lessons[lessonIndex].title);
    drawTextCentred(title, (Rectangle){ 0, 0, SCREEN_W, 64 }, 20, FONT_BOLD, COL_INK);
    drawText(step, SCREEN_W - 30 - textWidth(step, 16, FONT_REGULAR), 23, 16, FONT_REGULAR, COL_MUTED);
}

/* ---------- Tutorial cards ---------- */

static void screenCards(void)
{
    const Lesson *lesson = &course.lessons[lessonIndex];
    const Card *card = cardAt(lesson, cardIndex);
    char step[40];
    snprintf(step, sizeof(step), "Card %d of %d", cardIndex + 1, lesson->cardCount);
    lessonBar(step);

    if (card == NULL)   /* a lesson with no cards goes straight to questions */
    {
        startQuiz();
        return;
    }

    /* Progress dots */
    for (int i = 0; i < lesson->cardCount; i++)
    {
        Color dot = i <= cardIndex ? COL_ACCENT : COL_LINE;
        DrawRectangleRounded((Rectangle){ 60 + i * 44, 90, 36, 6 }, 1, 4, dot);
    }

    drawText(card->title, 60, 116, 32, FONT_BOLD, COL_INK);

    bool hasCode = card->codeCount > 0;
    float textW = hasCode ? 470 : 760;
    float y = 176;
    for (int i = 0; i < card->textCount; i++)
    {
        y += drawRich(card->text[i], 60, y, textW, 19, COL_INK, true) + 14;
    }

    if (hasCode)
    {
        Rectangle code = { 580, 176, 560, codePanelHeight(card->codeCount) };
        codePanel(code, card->code, card->codeCount, false, 0, BLANK, 0);
    }

    /* Back / Next */
    Rectangle back = { 60, SCREEN_H - 76, 150, 48 };
    Rectangle next = { SCREEN_W - 260, SCREEN_H - 76, 200, 48 };
    if (button(back, "Back", BTN_SECONDARY, cardIndex > 0) || (IsKeyPressed(KEY_LEFT) && cardIndex > 0))
    {
        cardIndex--;
    }
    bool last = cardIndex == lesson->cardCount - 1;
    if (button(next, last ? "Start questions" : "Next", BTN_PRIMARY, true) || IsKeyPressed(KEY_RIGHT))
    {
        if (last)
        {
            startQuiz();
        }
        else
        {
            cardIndex++;
        }
    }
    if (!last && button((Rectangle){ SCREEN_W - 460, SCREEN_H - 76, 190, 48 }, "Skip to questions", BTN_QUIET, true))
    {
        startQuiz();
    }
}

/* ---------- Questions ---------- */

static void pick(const Question *q, int choice)
{
    if (answered || choice == wrongPick)
    {
        return;
    }
    if (isAnswer(q, choice))
    {
        answered = true;
        gotItRight = true;
        pointsThisQuestion = pointsForAttempt(attempt);
        pointsThisLesson += pointsThisQuestion;
    }
    else if (attempt == 1)
    {
        wrongPick = choice;   /* one more go */
        attempt = 2;
    }
    else
    {
        answered = true;
        gotItRight = false;
        pointsThisQuestion = 0;
    }
}

static void finishQuiz(void)
{
    int next = lessonIndex + 1;
    bool openBefore = next < course.lessonCount && lessonUnlocked(current(), &course, next);

    newBadges = finishLesson(current(), &course, lessonIndex, pointsThisLesson);
    resultPercent = scorePercent(pointsThisLesson, course.lessons[lessonIndex].questionCount);
    nextJustUnlocked = next < course.lessonCount && !openBefore && lessonUnlocked(current(), &course, next);
    save();
    screen = SCREEN_RESULTS;
}

static void nextQuestion(void)
{
    questionIndex++;
    attempt = 1;
    wrongPick = 0;
    answered = false;
    gotItRight = false;
    pointsThisQuestion = 0;
    if (questionIndex >= course.lessons[lessonIndex].questionCount)
    {
        finishQuiz();
    }
}

/* The feedback box under the answers. Returns its height. */
static float feedback(const Question *q, float x, float y, float w)
{
    const char *title;
    Color fill, ink;
    char line[60];

    if (!answered && attempt == 1)
    {
        return 0;
    }
    if (!answered)
    {
        title = "Not quite. Have another go!";
        fill = COL_GOLD_SOFT;
        ink = (Color){ 140, 96, 10, 255 };
    }
    else if (gotItRight)
    {
        snprintf(line, sizeof(line), "Correct!  +%d points", pointsThisQuestion);
        title = line;
        fill = COL_GOOD_SOFT;
        ink = COL_GOOD;
    }
    else
    {
        title = (q->type == Q_BUG) ? "Not this time. The bug is highlighted." : "Not this time. The answer is highlighted.";
        fill = COL_BAD_SOFT;
        ink = COL_BAD;
    }

    float textH = answered ? drawRich(q->explain, 0, 0, w - 36, 17, COL_INK, false) : 0;
    float h = 54 + (answered ? textH + 6 : 0);
    DrawRectangleRounded((Rectangle){ x, y, w, h }, 0.12f, 8, fill);
    drawText(title, x + 18, y + 14, 19, FONT_BOLD, ink);
    if (answered)
    {
        drawRich(q->explain, x + 18, y + 48, w - 36, 17, COL_INK, true);
    }
    return h;
}

/* Draws the answer buttons in a column. */
static void options(const Question *q, float x, float y, float w)
{
    FontKind kind = (q->type == Q_OUTPUT) ? FONT_MONO : FONT_REGULAR;
    for (int i = 0; i < q->optionCount; i++)
    {
        int choice = i + 1;
        Rectangle r = { x, y + i * 62, w, 52 };

        /* Plain, hovered, right (green) or wrong (red) */
        Color fill = COL_PANEL, edge = COL_LINE, ink = COL_INK;
        bool plain = true;
        if (answered && isAnswer(q, choice))
        {
            fill = COL_GOOD_SOFT; edge = COL_GOOD; ink = COL_GOOD; plain = false;
        }
        else if (choice == wrongPick)
        {
            fill = COL_BAD_SOFT; edge = COL_BAD; ink = COL_BAD; plain = false;
        }
        else if (!answered && mouseOver(r))
        {
            fill = COL_ACCENT_SOFT; edge = COL_ACCENT; plain = false;
        }

        DrawRectangleRounded(r, 0.25f, 8, fill);
        DrawRectangleRoundedLinesEx(r, 0.25f, 8, 1.5f, edge);

        /* Letter A, B, C... */
        char letter[2] = { (char)('A' + i), '\0' };
        Rectangle tag = { r.x + 12, r.y + 12, 28, 28 };
        DrawRectangleRounded(tag, 0.3f, 6, plain ? COL_BG : edge);
        drawTextCentred(letter, tag, 15, FONT_BOLD, plain ? COL_MUTED : WHITE);

        float size = (kind == FONT_MONO) ? 17 : 18;
        drawText(q->options[i], r.x + 54, r.y + (52 - size) / 2 - 1, size, kind, ink);

        if (!answered && clicked(r))
        {
            pick(q, choice);
        }
    }
}

static void screenQuiz(void)
{
    const Lesson *lesson = &course.lessons[lessonIndex];
    const Question *q = questionAt(lesson, questionIndex);
    if (q == NULL)
    {
        finishQuiz();
        return;
    }

    char step[40];
    snprintf(step, sizeof(step), "Question %d of %d", questionIndex + 1, lesson->questionCount);
    lessonBar(step);

    /* Progress bar and points so far */
    float barW = 900;
    DrawRectangleRounded((Rectangle){ 60, 90, barW, 10 }, 1, 6, COL_LINE);
    float done = (questionIndex + (answered ? 1.0f : 0.0f)) / lesson->questionCount;
    if (done > 0)
    {
        DrawRectangleRounded((Rectangle){ 60, 90, barW * done, 10 }, 1, 6, COL_ACCENT);
    }
    char pts[30];
    snprintf(pts, sizeof(pts), "%d points", pointsThisLesson);
    DrawCircle(SCREEN_W - 172, 95, 7, COL_GOLD);
    drawText(pts, SCREEN_W - 158, 84, 18, FONT_BOLD, (Color){ 140, 96, 10, 255 });

    /* Question type label and the question */
    const char *kind = q->type == Q_CHOICE ? "CHOOSE THE ANSWER" : q->type == Q_OUTPUT ? "PREDICT THE OUTPUT" : "SPOT THE BUG";
    drawText(kind, 60, 120, 14, FONT_BOLD, COL_ACCENT);
    float y = 144 + drawRich(q->text, 60, 144, 1080, 22, COL_INK, true) + 18;

    float rightX = 710, rightW = 430;
    float rightY = y;

    if (q->codeCount > 0)
    {
        Rectangle code = { 60, y, 620, codePanelHeight(q->codeCount) };
        bool bug = q->type == Q_BUG;
        int mark = 0;
        Color markColour = BLANK;
        if (bug && answered)
        {
            mark = q->answer;
            markColour = (Color){ 38, 145, 96, 150 };
        }
        int line = codePanel(code, q->code, q->codeCount, bug && !answered, mark, markColour, bug ? wrongPick : 0);
        if (line > 0)
        {
            pick(q, line);
        }
        if (bug && !answered)
        {
            drawText("Click a line in the code", 60, code.y + code.height + 10, 15, FONT_REGULAR, COL_MUTED);
        }
        if (!bug)
        {
            options(q, rightX, rightY, rightW);
            rightY += q->optionCount * 62 + 8;
        }
    }
    else
    {
        options(q, 60, y, 620);
    }

    feedback(q, rightX, rightY, rightW);

    if (answered)
    {
        bool last = questionIndex == lesson->questionCount - 1;
        Rectangle next = { SCREEN_W - 260, SCREEN_H - 76, 200, 48 };
        if (button(next, last ? "See results" : "Next question", BTN_PRIMARY, true) || IsKeyPressed(KEY_ENTER))
        {
            nextQuestion();
        }
    }
}

/* ---------- Results ---------- */

static void drawBadgeMedal(float cx, float cy, float radius, int badge, bool earned)
{
    static const char *glyphs[BADGE_COUNT] = { "1", "100", "for", "B", "fx", "*p", "A+" };
    Color ring = earned ? COL_GOLD : COL_LINE;
    Color face = earned ? COL_GOLD_SOFT : COL_BG;
    if (radius > 14)
    {
        /* ribbon tails */
        DrawTriangle((Vector2){ cx - radius * 0.7f, cy + radius * 0.4f }, (Vector2){ cx - radius * 0.8f, cy + radius * 1.5f },
                     (Vector2){ cx - radius * 0.1f, cy + radius * 0.9f }, earned ? COL_ACCENT : COL_LINE);
        DrawTriangle((Vector2){ cx + radius * 0.7f, cy + radius * 0.4f }, (Vector2){ cx + radius * 0.1f, cy + radius * 0.9f },
                     (Vector2){ cx + radius * 0.8f, cy + radius * 1.5f }, earned ? COL_ACCENT : COL_LINE);
    }
    DrawCircle((int)cx, (int)cy, radius, ring);
    DrawCircle((int)cx, (int)cy, radius * 0.8f, face);
    if (radius > 14)
    {
        float size = radius * (strlen(glyphs[badge]) > 2 ? 0.55f : 0.75f);
        drawTextCentred(glyphs[badge], (Rectangle){ cx - radius, cy - radius, radius * 2, radius * 2 },
                        size, FONT_MONO, earned ? (Color){ 140, 96, 10, 255 } : COL_MUTED);
    }
}

static void screenResults(void)
{
    header();
    const Lesson *lesson = &course.lessons[lessonIndex];
    bool pass = resultPercent >= PASS_PERCENT;

    int shown = 0;   /* new badges to show */
    for (int b = 0; b < BADGE_COUNT; b++)
    {
        if (newBadges & (1u << b))
        {
            shown++;
        }
    }
    float boxH = shown > 0 ? 570 : 490;
    Rectangle box = { 250, 64 + (SCREEN_H - 64 - boxH) / 2, 700, boxH };
    panel(box, COL_PANEL);

    drawTextCentred(pass ? "Lesson passed!" : "Nearly there", (Rectangle){ box.x, box.y + 30, box.width, 40 },
                    32, FONT_BOLD, pass ? COL_GOOD : COL_INK);
    char line[120];
    snprintf(line, sizeof(line), "Lesson %d: %s", lesson->number, lesson->title);
    drawTextCentred(line, (Rectangle){ box.x, box.y + 76, box.width, 24 }, 18, FONT_REGULAR, COL_MUTED);

    /* Score ring */
    Vector2 c = { box.x + box.width / 2, box.y + 190 };
    DrawRing(c, 62, 74, 0, 360, 64, COL_LINE);
    DrawRing(c, 62, 74, -90, -90 + 360.0f * resultPercent / 100, 64, pass ? COL_GOOD : COL_GOLD);
    snprintf(line, sizeof(line), "%d%%", resultPercent);
    drawTextCentred(line, (Rectangle){ c.x - 60, c.y - 30, 120, 60 }, 38, FONT_BOLD, COL_INK);

    float y = box.y + 290;
    snprintf(line, sizeof(line), "You earned %d points", pointsThisLesson);
    drawTextCentred(line, (Rectangle){ box.x, y, box.width, 26 }, 20, FONT_BOLD, COL_INK);
    y += 34;

    int next = lessonIndex + 1;
    if (!pass)
    {
        snprintf(line, sizeof(line), "You need %d%% to pass. Read the cards again and have another go.", PASS_PERCENT);
    }
    else if (nextJustUnlocked)
    {
        snprintf(line, sizeof(line), "Lesson %d unlocked: %s", next + 1, course.lessons[next].title);
    }
    else if (next < course.lessonCount && !lessonReady(&course, next))
    {
        snprintf(line, sizeof(line), "Lesson %d (%s) is coming soon.", next + 1, course.lessons[next].title);
    }
    else
    {
        snprintf(line, sizeof(line), "Your best score for this lesson is %d%%.", current()->bestPercent[lessonIndex]);
    }
    drawTextCentred(line, (Rectangle){ box.x, y, box.width, 24 }, 17, FONT_REGULAR, COL_MUTED);
    y += 44;

    /* New badges */
    if (shown > 0)
    {
        drawTextCentred(shown == 1 ? "New badge!" : "New badges!", (Rectangle){ box.x, y, box.width, 24 },
                        18, FONT_BOLD, COL_GOLD);
        y += 34;
        float bx = box.x + box.width / 2 - (shown * 170 - 10) / 2.0f;
        for (int b = 0; b < BADGE_COUNT; b++)
        {
            if (newBadges & (1u << b))
            {
                drawBadgeMedal(bx + 22, y + 22, 22, b, true);
                drawText(BADGE_NAMES[b], bx + 52, y + 10, 15, FONT_BOLD, COL_INK);
                bx += 170;
            }
        }
    }

    /* Buttons */
    float by = box.y + box.height - 76;
    bool canNext = pass && lessonUnlocked(current(), &course, next);
    if (button((Rectangle){ box.x + 40, by, 190, 48 }, "Try again", BTN_SECONDARY, true))
    {
        startLesson(lessonIndex);
    }
    if (button((Rectangle){ box.x + 250, by, 200, 48 }, "Back to path", canNext ? BTN_SECONDARY : BTN_PRIMARY, true))
    {
        screen = SCREEN_PATH;
    }
    if (canNext && button((Rectangle){ box.x + 470, by, 190, 48 }, "Next lesson", BTN_PRIMARY, true))
    {
        startLesson(next);
    }
}

/* ---------- Badges ---------- */

static void screenBadges(void)
{
    header();
    if (button((Rectangle){ 40, 84, 140, 36 }, "< Path", BTN_QUIET, true))
    {
        screen = SCREEN_PATH;
    }
    drawText("Your badges", 60, 130, 32, FONT_BOLD, COL_INK);
    drawText("Badges are worth 25 bonus points each.", 60, 176, 17, FONT_REGULAR, COL_MUTED);

    for (int b = 0; b < BADGE_COUNT; b++)
    {
        bool earned = (current()->badges >> b) & 1u;
        Rectangle r = { 60 + (b % 4) * 272, 220 + (b / 4) * 220, 254, 200 };
        panel(r, earned ? COL_PANEL : (Color){ 250, 249, 246, 255 });
        drawBadgeMedal(r.x + r.width / 2, r.y + 62, 34, b, earned);
        drawTextCentred(BADGE_NAMES[b], (Rectangle){ r.x, r.y + 118, r.width, 26 }, 18, FONT_BOLD,
                        earned ? COL_INK : COL_MUTED);
        drawTextCentred(BADGE_DESCRIPTIONS[b], (Rectangle){ r.x, r.y + 148, r.width, 22 }, 14, FONT_REGULAR, COL_MUTED);
    }
}

/* ---------- Error screen (course file missing or broken) ---------- */

static void screenError(void)
{
    drawText("C Quest couldn't load its lessons", 60, 80, 30, FONT_BOLD, COL_BAD);
    drawText(courseError, 60, 140, 20, FONT_MONO, COL_INK);
    drawRich("Check that the `content` folder is next to the program and that `course.txt` "
             "follows the format described at the top of `course.c`.", 60, 190, 900, 19, COL_MUTED, true);
}

/* ---------- Main ---------- */

int main(void)
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "C Quest");
    SetExitKey(KEY_NULL);   /* Esc shouldn't close the app mid-lesson */
    SetTargetFPS(60);

    /* Find content/ and assets/ even if started from somewhere else */
    ChangeDirectory(GetApplicationDirectory());
    loadFonts();

    if (loadCourse(COURSE_FILE, &course, courseError, sizeof(courseError)) == 0)
    {
        screen = SCREEN_ERROR;
    }
    learnerCount = loadLearners(LEARNERS_FILE, learners, MAX_LEARNERS);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(COL_BG);
        uiBeginFrame();

        switch (screen)
        {
            case SCREEN_SIGN_IN: screenSignIn(); break;
            case SCREEN_PATH:    screenPath(); break;
            case SCREEN_CARDS:   screenCards(); break;
            case SCREEN_QUIZ:    screenQuiz(); break;
            case SCREEN_RESULTS: screenResults(); break;
            case SCREEN_BADGES:  screenBadges(); break;
            case SCREEN_ERROR:   screenError(); break;
        }

        uiEndFrame();
        EndDrawing();
    }

    freeCourse(&course);
    unloadFonts();
    CloseWindow();
    return 0;
}
