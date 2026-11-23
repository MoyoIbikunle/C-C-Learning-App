/*
 * main.c - C Quest: an interactive app for learning C and C++.
 *
 * Screens: sign in -> learning path -> lesson (tutorial cards, then
 * questions) -> results. The Practise tab has a quick quiz, a review of
 * mistakes and a timed exam. The Teach tab has the duck's broken programs
 * to fix. Plus a badges screen and a daily streak.
 * Progress is saved to learners.txt after every lesson, practice run and fix.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "raylib.h"
#include "app.h"
#include "ui.h"

#define COURSE_FILE "content/course.txt"
#define LEARNERS_FILE "learners.txt"
#define DUCK_FILE "content/duck.txt"
#define DUCK_NAME "Bugsy"          /* the duck in the Teach tab */
#define HISTORY_FILE "history.txt"
#define GLOSSARY_FILE "content/glossary.txt"
#define EXERCISE_FILE "content/exercises.txt"

typedef enum {
    SCREEN_SIGN_IN,
    SCREEN_PATH,
    SCREEN_CARDS,
    SCREEN_QUIZ,
    SCREEN_RESULTS,
    SCREEN_BADGES,
    SCREEN_PRACTISE,
    SCREEN_PRACTICE_RESULTS,
    SCREEN_EXAM_RESULTS,
    SCREEN_TEACH,
    SCREEN_DUCK,
    SCREEN_CODE,
    SCREEN_EDITOR,
    SCREEN_PROGRESS,
    SCREEN_GLOSSARY,
    SCREEN_ERROR
} Screen;

/* What kind of question run is going on */
typedef enum {
    RUN_LESSON,               /* a lesson's questions, in order, with points */
    RUN_QUICK,                /* random questions from passed lessons */
    RUN_REVIEW,               /* questions the learner got wrong before */
    RUN_EXAM,                 /* timed, one answer each, marked at the end */
    RUN_EXAM_REVIEW           /* stepping back through a finished exam */
} RunKind;

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

/* The questions being answered (a lesson or a practice run) */
static RunKind runKind = RUN_LESSON;
static QuestionRef runList[MAX_POOL];
static int runCount = 0;
static int questionIndex = 0;
static int attempt = 1;             /* 1 = first try, 2 = second try */
static int wrongPick = 0;           /* option or line picked wrongly on the first try */
static bool answered = false;       /* this question is finished */
static bool gotItRight = false;
static int pointsThisQuestion = 0;
static int pointsThisLesson = 0;
static int rightFirstTime = 0;      /* practice score */
static int mistakesCleared = 0;     /* in a review */

/* Exam mode */
static int examPicks[EXAM_QUESTIONS];   /* answer picked for each question, 0 = none */
static double examStart = 0;
static int examSeconds = 0;         /* time taken */
static int examRight = 0;
static bool examNewBest = false;
static bool examTimeUp = false;

/* Results */
static int resultPercent = 0;
static unsigned newBadges = 0;
static bool nextJustUnlocked = false;

/* Path screen: lesson under the mouse, for the hint at the bottom */
static int hoverLesson = -1;

/* Looking back at a lesson's cards from a quiz or the duck, then returning */
static bool peeking = false;
static Screen peekReturn = SCREEN_PATH;
static unsigned lessonsToRevisit = 0;   /* lessons with a wrong answer in this practice run */

/* Teach: the duck's problems */
static DuckList ducks;
static char duckError[200] = "";
static int duckIndex = 0;
static int duckStep = 1;            /* 1 find the line, 2 pick the fix, 3 fixed */
static int duckWrongLine = 0;
static int duckWrongFix = 0;
static bool duckFirstTime = false;
static unsigned duckBadges = 0;

/* History of attempts, for My progress */
static AttemptList history;

/* Glossary */
static Glossary glossary;
static char glossaryError[200] = "";
static const Term *popupTerm = NULL;      /* the term shown in a pop-up, if any */
static Screen glossaryReturn = SCREEN_PATH;
static char glossaryQuery[40] = "";
static float glossaryScroll = 0;

/* Code tab */
static ExerciseList exercises;
static char exerciseError[200] = "";
static int exerciseIndex = -1;            /* -1 = the playground */
static TextBuffer editor;
static int editorScroll = 0;
static bool editorFocused = false;
static char drafts[MAX_EXERCISES + 1][CODE_TEXT_LEN];   /* code kept while the app is open */
static bool hasDraft[MAX_EXERCISES + 1];
static bool playgroundCpp = false;
static RunResult runResult;
static bool hasResult = false;
static bool runPending = false;
static CheckLine checks[MAX_CHECKS];
static int checksPassed = 0;
static bool justSolvedFirst = false;
static bool showHint = false;
static bool showAnswer = false;
static int failedRuns = 0;

/* A short message that pops up for a few seconds */
static char toastText[120] = "";
static double toastUntil = 0;

static Learner *current(void)
{
    return &learners[me];
}

static void save(void)
{
    saveLearners(LEARNERS_FILE, learners, learnerCount);
}

static void toast(const char *message)
{
    snprintf(toastText, sizeof(toastText), "%s", message);
    toastUntil = GetTime() + 4.0;
}

static void drawToast(void)
{
    double left = toastUntil - GetTime();
    if (left <= 0)
    {
        return;
    }
    unsigned char alpha = (unsigned char)(left < 0.5 ? 255 * left / 0.5 : 255);
    float w = textWidth(toastText, 18, FONT_BOLD) + 60;
    Rectangle r = { (SCREEN_W - w) / 2, 80, w, 50 };
    DrawRectangleRounded(r, 0.5f, 8, (Color){ 31, 36, 51, (unsigned char)(alpha * 0.95f) });
    drawTextCentred(toastText, r, 18, FONT_BOLD, (Color){ 255, 255, 255, alpha });
}

/* Today's day number, from the computer's clock */
static int today(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    return dayNumber(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
}

/* Call when the learner finishes something: keeps the daily streak going */
static void didActivity(void)
{
    bool grew;
    unsigned got = recordActivity(current(), today(), &grew);
    newBadges |= got;
    char message[120];
    if (got & (1u << BADGE_ON_A_ROLL))
    {
        snprintf(message, sizeof(message), "%d days in a row! You earned the On a Roll badge.", current()->streak);
        toast(message);
    }
    else if (grew && current()->streak == 1)
    {
        toast("Day 1 of your streak! Come back tomorrow to keep it going.");
    }
    else if (grew)
    {
        snprintf(message, sizeof(message), "%d-day streak! Come back tomorrow to keep it going.", current()->streak);
        toast(message);
    }
}

/* Opens a lesson's cards from somewhere else; the last card brings you back */
static void peekLesson(int lesson, Screen back)
{
    peeking = true;
    peekReturn = back;
    lessonIndex = lesson;
    cardIndex = 0;
    screen = SCREEN_CARDS;
}

/* A little flame for the streak */
static void drawFlame(float cx, float cy, float s, Color outer, Color inner)
{
    DrawCircle((int)cx, (int)(cy + s * 0.25f), s * 0.5f, outer);
    DrawTriangle((Vector2){ cx, cy - s * 0.85f }, (Vector2){ cx - s * 0.48f, cy + s * 0.15f },
                 (Vector2){ cx + s * 0.48f, cy + s * 0.15f }, outer);
    DrawCircle((int)cx, (int)(cy + s * 0.38f), s * 0.25f, inner);
    DrawTriangle((Vector2){ cx, cy - s * 0.15f }, (Vector2){ cx - s * 0.23f, cy + s * 0.35f },
                 (Vector2){ cx + s * 0.23f, cy + s * 0.35f }, inner);
}

static void openGlossary(Screen back);
static void recordAttempt(AttemptKind kind, int lesson, int right, int total, int percent);
static void cardKeyWords(const Card *card, float x, float y, float maxX);

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

    /* Tabs: Learn, Practise and Teach */
    const char *tabs[4] = { "Learn", "Practise", "Code", "Teach" };
    const Screen tabScreens[4] = { SCREEN_PATH, SCREEN_PRACTISE, SCREEN_CODE, SCREEN_TEACH };
    int active = -1;
    if (screen == SCREEN_PATH || screen == SCREEN_RESULTS)
    {
        active = 0;
    }
    else if (screen == SCREEN_PRACTISE || screen == SCREEN_PRACTICE_RESULTS || screen == SCREEN_EXAM_RESULTS)
    {
        active = 1;
    }
    else if (screen == SCREEN_CODE)
    {
        active = 2;
    }
    else if (screen == SCREEN_TEACH)
    {
        active = 3;
    }
    float x = 220;
    for (int i = 0; i < 4; i++)
    {
        float w = textWidth(tabs[i], 18, FONT_BOLD);
        Rectangle hit = { x - 8, 8, w + 16, 52 };
        bool hover = mouseOver(hit);
        drawText(tabs[i], x, 21, 18, FONT_BOLD, (i == active || hover) ? COL_ACCENT : COL_MUTED);
        if (i == active)
        {
            DrawRectangle((int)x, 61, (int)w, 3, COL_ACCENT);
        }
        if (clicked(hit))
        {
            screen = tabScreens[i];
        }
        x += w + 30;
    }

    /* Points and badges */
    char text[40];
    snprintf(text, sizeof(text), "%d points", totalPoints(current()));
    float pw = textWidth(text, 17, FONT_BOLD) + 40;
    Rectangle points = { 760, 16, pw, 32 };

    /* Streak: orange once today counts, grey while it still needs doing today */
    int streak = currentStreak(current(), today());
    bool doneToday = current()->lastDay == today();
    char streakText[30];
    if (streak == 0)
    {
        snprintf(streakText, sizeof(streakText), "No streak yet");
    }
    else
    {
        snprintf(streakText, sizeof(streakText), "%d-day streak", streak);
    }
    float sw = textWidth(streakText, 17, FONT_BOLD) + 42;
    Rectangle streakChip = { points.x - sw - 10, 16, sw, 32 };
    Color orange = { 236, 112, 36, 255 };
    DrawRectangleRounded(streakChip, 0.5f, 8, doneToday ? (Color){ 253, 234, 220, 255 } : COL_BG);
    drawFlame(streakChip.x + 18, streakChip.y + 15, 14, doneToday ? orange : COL_LINE,
              doneToday ? COL_GOLD : COL_MUTED);
    drawText(streakText, streakChip.x + 32, streakChip.y + 5, 17, FONT_BOLD, doneToday ? orange : COL_MUTED);
    if (mouseOver(streakChip))
    {
        char tip[120];
        if (doneToday)
        {
            snprintf(tip, sizeof(tip), "Done for today! Best streak: %d days", current()->bestStreak);
        }
        else
        {
            snprintf(tip, sizeof(tip), "Finish a lesson, practice or duck problem today to keep it going");
        }
        float tw = textWidth(tip, 15, FONT_REGULAR) + 24;
        Rectangle tipBox = { streakChip.x + sw - tw, 56, tw, 32 };
        DrawRectangleRounded(tipBox, 0.4f, 8, COL_INK);
        drawTextCentred(tip, tipBox, 15, FONT_REGULAR, WHITE);
    }

    /* Points: click for My progress */
    bool onPoints = mouseOver(points) || screen == SCREEN_PROGRESS;
    DrawRectangleRounded(points, 0.5f, 8, onPoints ? COL_GOLD : COL_GOLD_SOFT);
    DrawCircle((int)points.x + 17, (int)points.y + 16, 7, onPoints ? WHITE : COL_GOLD);
    drawText(text, points.x + 30, points.y + 5, 17, FONT_BOLD, onPoints ? WHITE : (Color){ 140, 96, 10, 255 });
    if (clicked(points))
    {
        screen = SCREEN_PROGRESS;
    }
    if (mouseOver(points))
    {
        Rectangle tip = { points.x + pw / 2 - 70, 56, 140, 30 };
        DrawRectangleRounded(tip, 0.4f, 8, COL_INK);
        drawTextCentred("See my progress", tip, 15, FONT_REGULAR, WHITE);
    }

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
    peeking = false;
    lessonIndex = index;
    cardIndex = 0;
    screen = SCREEN_CARDS;
}

static void resetQuestion(void)
{
    attempt = 1;
    wrongPick = 0;
    answered = false;
    gotItRight = false;
    pointsThisQuestion = 0;
}

static void startQuiz(void)
{
    runKind = RUN_LESSON;
    runCount = 0;
    for (int k = 0; k < course.lessons[lessonIndex].questionCount && runCount < MAX_POOL; k++)
    {
        runList[runCount++] = (QuestionRef){ lessonIndex, k };
    }
    rightFirstTime = 0;
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
    if (button((Rectangle){ SCREEN_W - 200, SCREEN_H - 52, 140, 40 }, "Glossary", BTN_SECONDARY, true))
    {
        openGlossary(SCREEN_PATH);
    }
}

/* ---------- Lesson top bar ---------- */

static void lessonBar(const char *step)
{
    DrawRectangle(0, 0, SCREEN_W, 64, COL_PANEL);
    DrawRectangle(0, 64, SCREEN_W, 1, COL_LINE);

    /* Lessons go back to the path; practice runs go back to Practise;
       a lesson opened from a quiz or the duck goes back there */
    bool inLesson = screen == SCREEN_CARDS || runKind == RUN_LESSON;
    bool peek = screen == SCREEN_CARDS && peeking;
    const char *label = peek ? "< Back" : (inLesson ? "< Path" : "< Practise");
    if (button((Rectangle){ 20, 14, 150, 36 }, label, BTN_QUIET, true))
    {
        if (peek)
        {
            peeking = false;
            screen = peekReturn;
        }
        else
        {
            screen = inLesson ? SCREEN_PATH : (runKind == RUN_EXAM_REVIEW ? SCREEN_EXAM_RESULTS : SCREEN_PRACTISE);
        }
    }
    char title[100];
    if (inLesson)
    {
        snprintf(title, sizeof(title), "Lesson %d: %s", course.lessons[lessonIndex].number,
                 course.lessons[lessonIndex].title);
    }
    else
    {
        const char *names[] = { "", "Quick quiz", "Review my mistakes", "Exam", "Exam: your answers" };
        snprintf(title, sizeof(title), "%s", names[runKind]);
    }
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
    const char *lastLabel = "Start questions";
    if (peeking)
    {
        switch (peekReturn)
        {
            case SCREEN_QUIZ:     lastLabel = "Back to question"; break;
            case SCREEN_DUCK:     lastLabel = "Back to the duck"; break;
            case SCREEN_EDITOR:   lastLabel = "Back to your code"; break;
            case SCREEN_PROGRESS: lastLabel = "Back to progress"; break;
            case SCREEN_PATH:     lastLabel = "Back to the path"; break;
            case SCREEN_GLOSSARY: lastLabel = "Back to glossary"; break;
            default:              lastLabel = "Back to results"; break;
        }
    }
    if (button(next, last ? lastLabel : "Next", BTN_PRIMARY, true) || IsKeyPressed(KEY_RIGHT))
    {
        if (last && peeking)
        {
            peeking = false;
            screen = peekReturn;
        }
        else if (last)
        {
            startQuiz();
        }
        else
        {
            cardIndex++;
        }
    }
    if (!last && !peeking &&
        button((Rectangle){ SCREEN_W - 460, SCREEN_H - 76, 190, 48 }, "Skip to questions", BTN_QUIET, true))
    {
        startQuiz();
    }

    /* Key words from the glossary, between Back and the buttons on the right */
    if (screen == SCREEN_CARDS)
    {
        cardKeyWords(card, 230, SCREEN_H - 76, (!last && !peeking) ? SCREEN_W - 476 : SCREEN_W - 276);
    }
}

/* ---------- Questions ---------- */

static QuestionRef currentRef(void)
{
    return runList[questionIndex];
}

static void finishQuiz(void);
static void finishPractice(void);
static void finishExamRun(void);

/* In an exam review, show the learner's own answer against the right one */
static void showExamAnswer(void)
{
    const Question *q = questionFor(&course, currentRef());
    int chosen = examPicks[questionIndex];
    answered = true;
    gotItRight = chosen != 0 && isAnswer(q, chosen);
    wrongPick = gotItRight ? 0 : chosen;
}

/* Starts a practice run: picks the questions and shuffles them */
static void startRun(RunKind kind)
{
    static QuestionRef pool[MAX_POOL];
    int n = buildPool(current(), &course, kind == RUN_REVIEW ? POOL_MISSED : POOL_PASSED, pool, MAX_POOL);
    int want = kind == RUN_EXAM ? EXAM_QUESTIONS : (kind == RUN_REVIEW ? n : QUICK_QUESTIONS);
    if (n == 0)
    {
        return;
    }
    shuffleRefs(pool, n);
    runCount = n < want ? n : want;
    for (int i = 0; i < runCount; i++)
    {
        runList[i] = pool[i];
        if (kind == RUN_EXAM)
        {
            examPicks[i] = 0;
        }
    }
    runKind = kind;
    questionIndex = 0;
    rightFirstTime = 0;
    mistakesCleared = 0;
    lessonsToRevisit = 0;
    pointsThisLesson = 0;
    resetQuestion();
    if (kind == RUN_EXAM)
    {
        examStart = GetTime();
    }
    screen = SCREEN_QUIZ;
}

static void pick(const Question *q, int choice)
{
    if (runKind == RUN_EXAM)
    {
        examPicks[questionIndex] = choice;   /* can be changed until Next */
        return;
    }
    if (answered || choice == wrongPick)
    {
        return;
    }

    bool right = isAnswer(q, choice);
    if (attempt == 1)
    {
        /* First tries decide what goes into "Review my mistakes" */
        if (right && isMissed(current(), currentRef()))
        {
            mistakesCleared++;
        }
        recordAnswer(current(), currentRef(), right);
        if (right)
        {
            rightFirstTime++;
        }
        else
        {
            lessonsToRevisit |= 1u << currentRef().lesson;
        }
    }

    if (right)
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
    recordAttempt(ATTEMPT_LESSON, lessonIndex, rightFirstTime, runCount, resultPercent);
    didActivity();
    save();
    screen = SCREEN_RESULTS;
}

static void finishPractice(void)
{
    recordAttempt(runKind == RUN_REVIEW ? ATTEMPT_REVIEW : ATTEMPT_QUICK, -1, rightFirstTime, runCount,
                  runCount > 0 ? rightFirstTime * 100 / runCount : 0);
    newBadges = 0;
    didActivity();
    save();   /* mistakes added or cleared */
    screen = SCREEN_PRACTICE_RESULTS;
}

/* Marks the exam: every unanswered or wrong question goes into the review list */
static void finishExamRun(void)
{
    double used = GetTime() - examStart;
    examTimeUp = used >= EXAM_SECONDS;
    examSeconds = examTimeUp ? EXAM_SECONDS : (int)used;

    examRight = 0;
    for (int i = 0; i < runCount; i++)
    {
        const Question *q = questionFor(&course, runList[i]);
        bool right = examPicks[i] != 0 && isAnswer(q, examPicks[i]);
        recordAnswer(current(), runList[i], right);
        if (right)
        {
            examRight++;
        }
    }
    newBadges = finishExam(current(), examRight, runCount, &examNewBest);
    recordAttempt(ATTEMPT_EXAM, -1, examRight, runCount, runCount > 0 ? examRight * 100 / runCount : 0);
    didActivity();
    save();
    screen = SCREEN_EXAM_RESULTS;
}

static void nextQuestion(void)
{
    questionIndex++;
    resetQuestion();
    if (questionIndex < runCount)
    {
        if (runKind == RUN_EXAM_REVIEW)
        {
            showExamAnswer();
        }
        return;
    }
    switch (runKind)
    {
        case RUN_LESSON:      finishQuiz(); break;
        case RUN_EXAM:        finishExamRun(); break;
        case RUN_EXAM_REVIEW: screen = SCREEN_EXAM_RESULTS; break;
        default:              finishPractice(); break;
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
        if (runKind == RUN_LESSON)
        {
            snprintf(line, sizeof(line), "Correct!  +%d points", pointsThisQuestion);
        }
        else
        {
            snprintf(line, sizeof(line), attempt == 1 ? "Correct!" : "Correct on your second try");
        }
        title = line;
        fill = COL_GOOD_SOFT;
        ink = COL_GOOD;
    }
    else
    {
        if (runKind == RUN_EXAM_REVIEW && examPicks[questionIndex] == 0)
        {
            title = "Not answered. The answer is highlighted.";
        }
        else
        {
            title = (q->type == Q_BUG) ? "Not this time. The bug is highlighted." : "Not this time. The answer is highlighted.";
        }
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
    int chosen = (runKind == RUN_EXAM) ? examPicks[questionIndex] : 0;
    for (int i = 0; i < q->optionCount; i++)
    {
        int choice = i + 1;
        Rectangle r = { x, y + i * 62, w, 52 };

        /* Plain, hovered, chosen (exam), right (green) or wrong (red) */
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
        else if (choice == chosen)
        {
            fill = COL_ACCENT_SOFT; edge = COL_ACCENT; ink = COL_ACCENT; plain = false;
        }
        else if (!answered && mouseOver(r))
        {
            fill = COL_ACCENT_SOFT; edge = COL_ACCENT; plain = false;
        }

        DrawRectangleRounded(r, 0.25f, 8, fill);
        DrawRectangleRoundedLinesEx(r, 0.25f, 8, choice == chosen ? 2.5f : 1.5f, edge);

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

/* Top-right of the question screen: points, score, or the exam clock */
static void runStatus(void)
{
    char text[40];
    Color ink = (Color){ 140, 96, 10, 255 };
    Color dot = COL_GOLD;

    if (runKind == RUN_EXAM)
    {
        int left = EXAM_SECONDS - (int)(GetTime() - examStart);
        if (left < 0)
        {
            left = 0;
        }
        snprintf(text, sizeof(text), "%d:%02d left", left / 60, left % 60);
        bool hurry = left < 60;
        ink = hurry ? COL_BAD : COL_INK;
        dot = hurry ? COL_BAD : COL_ACCENT;
    }
    else if (runKind == RUN_LESSON)
    {
        snprintf(text, sizeof(text), "%d points", pointsThisLesson);
    }
    else if (runKind == RUN_EXAM_REVIEW)
    {
        snprintf(text, sizeof(text), "%d / %d right", examRight, runCount);
        ink = COL_INK;
        dot = COL_ACCENT;
    }
    else
    {
        snprintf(text, sizeof(text), "%d right", rightFirstTime);
        ink = COL_GOOD;
        dot = COL_GOOD;
    }
    float w = textWidth(text, 18, FONT_BOLD);
    DrawCircle((int)(SCREEN_W - 60 - w - 14), 95, 7, dot);
    drawText(text, SCREEN_W - 60 - w, 84, 18, FONT_BOLD, ink);
}

static void screenQuiz(void)
{
    if (questionIndex >= runCount)
    {
        nextQuestion();
        return;
    }
    QuestionRef ref = currentRef();
    const Question *q = questionFor(&course, ref);
    if (q == NULL)
    {
        nextQuestion();
        return;
    }
    if (runKind == RUN_EXAM && GetTime() - examStart >= EXAM_SECONDS)
    {
        finishExamRun();   /* time's up: mark what's been answered */
        return;
    }
    bool exam = runKind == RUN_EXAM;

    char step[40];
    snprintf(step, sizeof(step), "Question %d of %d", questionIndex + 1, runCount);
    lessonBar(step);

    /* Progress bar and the status on the right */
    float barW = 860;
    DrawRectangleRounded((Rectangle){ 60, 90, barW, 10 }, 1, 6, COL_LINE);
    float done = (questionIndex + (answered ? 1.0f : 0.0f)) / runCount;
    if (done > 0)
    {
        DrawRectangleRounded((Rectangle){ 60, 90, barW * done, 10 }, 1, 6, COL_ACCENT);
    }
    runStatus();

    /* Question type label (and which lesson it's from, in practice) and the question */
    const char *kind = q->type == Q_CHOICE ? "CHOOSE THE ANSWER" : q->type == Q_OUTPUT ? "PREDICT THE OUTPUT" : "SPOT THE BUG";
    drawText(kind, 60, 120, 14, FONT_BOLD, COL_ACCENT);
    if (runKind != RUN_LESSON)
    {
        char from[90];
        snprintf(from, sizeof(from), "from lesson %d: %s", course.lessons[ref.lesson].number,
                 course.lessons[ref.lesson].title);
        drawText(from, 70 + textWidth(kind, 14, FONT_BOLD), 120, 14, FONT_REGULAR, COL_MUTED);
    }
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
        else if (bug && exam && examPicks[questionIndex] != 0)
        {
            mark = examPicks[questionIndex];
            markColour = (Color){ 76, 91, 212, 170 };
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

    bool last = questionIndex == runCount - 1;
    Rectangle next = { SCREEN_W - 260, SCREEN_H - 76, 200, 48 };

    /* Quick quiz and review: a link back to the lesson this question is from */
    if (runKind == RUN_QUICK || runKind == RUN_REVIEW)
    {
        char link[100];
        snprintf(link, sizeof(link), "Go over lesson %d: %s", course.lessons[ref.lesson].number,
                 course.lessons[ref.lesson].title);
        Rectangle r = { 50, SCREEN_H - 76, textWidth(link, 19, FONT_BOLD) + 40, 48 };
        if (button(r, link, BTN_QUIET, true))
        {
            peekLesson(ref.lesson, SCREEN_QUIZ);
        }
    }

    if (exam)
    {
        /* Exams: move freely between questions; nothing is marked until the end */
        Rectangle back = { 60, SCREEN_H - 76, 150, 48 };
        if (button(back, "Back", BTN_SECONDARY, questionIndex > 0))
        {
            questionIndex--;
        }
        bool picked = examPicks[questionIndex] != 0;
        if (button(next, last ? "Finish exam" : (picked ? "Next" : "Skip"), BTN_PRIMARY, true) ||
            (IsKeyPressed(KEY_ENTER) && picked))
        {
            nextQuestion();
        }
        int answeredCount = 0;
        for (int i = 0; i < runCount; i++)
        {
            answeredCount += examPicks[i] != 0;
        }
        char note[60];
        snprintf(note, sizeof(note), "%d of %d answered", answeredCount, runCount);
        drawText(note, 230, SCREEN_H - 62, 16, FONT_REGULAR, COL_MUTED);
    }
    else if (answered)
    {
        const char *label = "Next question";
        if (last)
        {
            label = runKind == RUN_EXAM_REVIEW ? "Back to results" : "See results";
        }
        if (runKind == RUN_EXAM_REVIEW)
        {
            Rectangle back = { 60, SCREEN_H - 76, 150, 48 };
            if (button(back, "Back", BTN_SECONDARY, questionIndex > 0))
            {
                questionIndex--;
                resetQuestion();
                showExamAnswer();
            }
        }
        if (button(next, label, BTN_PRIMARY, true) || IsKeyPressed(KEY_ENTER))
        {
            nextQuestion();
        }
    }
}

/* ---------- Practise tab ---------- */

static void practiseCard(Rectangle r, const char *glyph, Color colour, const char *title,
                         const char *about, const char *stat, const char *label, bool enabled, RunKind kind)
{
    panel(r, COL_PANEL);
    DrawCircle((int)(r.x + 50), (int)(r.y + 56), 28, colour);
    drawTextCentred(glyph, (Rectangle){ r.x + 22, r.y + 28, 56, 58 }, 22, FONT_BOLD, WHITE);
    drawText(title, r.x + 28, r.y + 104, 24, FONT_BOLD, COL_INK);
    drawRich(about, r.x + 28, r.y + 146, r.width - 56, 17, COL_MUTED, true);
    drawRich(stat, r.x + 28, r.y + r.height - 132, r.width - 56, 17, COL_INK, true);
    if (button((Rectangle){ r.x + 28, r.y + r.height - 74, r.width - 56, 48 }, label, BTN_PRIMARY, enabled))
    {
        startRun(kind);
    }
}

static void screenPractise(void)
{
    header();
    drawText("Practise", 60, 92, 32, FONT_BOLD, COL_INK);
    drawText("Practice doesn't change your lesson scores, so have as many goes as you like.",
             60, 136, 17, FONT_REGULAR, COL_MUTED);

    static QuestionRef pool[MAX_POOL];
    int available = buildPool(current(), &course, POOL_PASSED, pool, MAX_POOL);
    int toReview = buildPool(current(), &course, POOL_MISSED, pool, MAX_POOL);
    char stat[3][140];

    if (available == 0)
    {
        snprintf(stat[0], sizeof(stat[0]), "Pass your first lesson on the Learn tab to start practising.");
    }
    else
    {
        snprintf(stat[0], sizeof(stat[0]), "%d questions to pick from in the lessons you've passed.", available);
    }

    if (toReview == 0)
    {
        snprintf(stat[1], sizeof(stat[1]), "Nothing to review yet. Wrong answers will appear here.");
    }
    else
    {
        snprintf(stat[1], sizeof(stat[1]), "%d question%s waiting for you.", toReview, toReview == 1 ? "" : "s");
    }

    int examSize = available < EXAM_QUESTIONS ? available : EXAM_QUESTIONS;
    const char *bestText = "";
    char best[40];
    if (current()->bestExam >= 0)
    {
        snprintf(best, sizeof(best), " Best so far: %d%%.", current()->bestExam);
        bestText = best;
    }
    if (available == 0)
    {
        snprintf(stat[2], sizeof(stat[2]), "Pass your first lesson to take an exam.");
    }
    else if (examSize < EXAM_QUESTIONS)
    {
        snprintf(stat[2], sizeof(stat[2]), "Pass more lessons for a full exam (%d questions for now).%s",
                 examSize, bestText);
    }
    else
    {
        snprintf(stat[2], sizeof(stat[2]), "Score 90%% or more to earn Exam Ace.%s", bestText);
    }

    float w = 340, gap = 30, top = 180, h = 440;
    practiseCard((Rectangle){ 60, top, w, h }, "?", COL_ACCENT, "Quick quiz",
                 "10 random questions mixed from the lessons you've passed, with two tries and explanations.",
                 stat[0], "Start quiz", available > 0, RUN_QUICK);
    practiseCard((Rectangle){ 60 + w + gap, top, w, h }, "!", COL_BAD, "Review my mistakes",
                 "Every question you've got wrong, from all your lessons, in one go. Each stays here until you get it right first time.",
                 stat[1], "Start review", toReview > 0, RUN_REVIEW);
    practiseCard((Rectangle){ 60 + 2 * (w + gap), top, w, h }, "20", COL_GOLD, "Exam mode",
                 "20 questions in 20 minutes, one answer each. No hints: it's all marked at the end.",
                 stat[2], "Start exam", available > 0, RUN_EXAM);
}

/* The white results card with a score ring. Returns the card. */
static Rectangle resultsCard(float boxH, const char *title, Color titleInk, const char *subtitle,
                             int percent, Color ring, const char *centre)
{
    Rectangle box = { 250, 64 + (SCREEN_H - 64 - boxH) / 2, 700, boxH };
    panel(box, COL_PANEL);
    drawTextCentred(title, (Rectangle){ box.x, box.y + 30, box.width, 40 }, 32, FONT_BOLD, titleInk);
    drawTextCentred(subtitle, (Rectangle){ box.x, box.y + 76, box.width, 24 }, 18, FONT_REGULAR, COL_MUTED);

    Vector2 c = { box.x + box.width / 2, box.y + 190 };
    DrawRing(c, 62, 74, 0, 360, 64, COL_LINE);
    DrawRing(c, 62, 74, -90, -90 + 360.0f * percent / 100, 64, ring);
    drawTextCentred(centre, (Rectangle){ c.x - 60, c.y - 30, 120, 60 }, 34, FONT_BOLD, COL_INK);
    return box;
}

static void screenPracticeResults(void)
{
    header();
    int percent = runCount > 0 ? rightFirstTime * 100 / runCount : 0;
    char centre[20], line[140];
    snprintf(centre, sizeof(centre), "%d / %d", rightFirstTime, runCount);

    bool review = runKind == RUN_REVIEW;
    int topics = 0;
    for (int l = 0; l < course.lessonCount; l++)
    {
        topics += (lessonsToRevisit >> l) & 1u;
    }
    if (topics > 3)
    {
        topics = 3;   /* room for three */
    }
    float boxH = 470 + (topics > 0 ? 40 + topics * 44 : 0);
    Rectangle box = resultsCard(boxH, review ? "Review done" : "Quiz done", COL_INK,
                                "Right first time", percent, percent >= PASS_PERCENT ? COL_GOOD : COL_GOLD, centre);

    float y = box.y + 290;
    int left = missedCount(current());
    if (review)
    {
        snprintf(line, sizeof(line), "You cleared %d mistake%s.", mistakesCleared, mistakesCleared == 1 ? "" : "s");
        drawTextCentred(line, (Rectangle){ box.x, y, box.width, 26 }, 20, FONT_BOLD, COL_INK);
        snprintf(line, sizeof(line), left == 0 ? "Nothing left to review. Well done!" : "%d still to review.", left);
    }
    else
    {
        drawTextCentred(percent >= PASS_PERCENT ? "Nice work!" : "Keep practising!",
                        (Rectangle){ box.x, y, box.width, 26 }, 20, FONT_BOLD, COL_INK);
        snprintf(line, sizeof(line), "Anything you got wrong is saved in Review my mistakes.");
    }
    drawTextCentred(line, (Rectangle){ box.x, y + 34, box.width, 24 }, 17, FONT_REGULAR, COL_MUTED);

    /* Links to the lessons behind the wrong answers */
    if (topics > 0)
    {
        float ty = y + 80;
        drawTextCentred("Go over these topics:", (Rectangle){ box.x, ty, box.width, 24 }, 17, FONT_BOLD, COL_INK);
        ty += 36;
        int shown = 0;
        for (int l = 0; l < course.lessonCount && shown < topics; l++)
        {
            if (!((lessonsToRevisit >> l) & 1u))
            {
                continue;
            }
            char link[100];
            snprintf(link, sizeof(link), "Lesson %d: %s", course.lessons[l].number, course.lessons[l].title);
            float w = textWidth(link, 19, FONT_BOLD) + 40;
            if (button((Rectangle){ box.x + (box.width - w) / 2, ty, w, 40 }, link, BTN_QUIET, true))
            {
                peekLesson(l, SCREEN_PRACTICE_RESULTS);
            }
            ty += 44;
            shown++;
        }
    }

    float by = box.y + box.height - 76;
    bool canAgain = !review || left > 0;
    if (button((Rectangle){ box.x + 120, by, 220, 48 }, "Back to Practise", BTN_SECONDARY, true))
    {
        screen = SCREEN_PRACTISE;
    }
    if (button((Rectangle){ box.x + 360, by, 220, 48 }, "Go again", BTN_PRIMARY, canAgain))
    {
        startRun(runKind);
    }
}

static void drawNewBadges(Rectangle box, float y);

static void screenExamResults(void)
{
    header();
    int percent = runCount > 0 ? examRight * 100 / runCount : 0;
    char centre[20], line[140];
    snprintf(centre, sizeof(centre), "%d%%", percent);

    bool anyBadges = newBadges != 0;
    Rectangle box = resultsCard(anyBadges ? 560 : 480, examTimeUp ? "Time's up!" : "Exam finished", COL_INK,
                                "Exam mode", percent, percent >= PASS_PERCENT ? COL_GOOD : COL_GOLD, centre);

    float y = box.y + 290;
    snprintf(line, sizeof(line), "%d of %d right in %d:%02d", examRight, runCount, examSeconds / 60, examSeconds % 60);
    drawTextCentred(line, (Rectangle){ box.x, y, box.width, 26 }, 20, FONT_BOLD, COL_INK);
    if (examNewBest)
    {
        snprintf(line, sizeof(line), "That's your best exam score yet!");
    }
    else
    {
        snprintf(line, sizeof(line), "Your best is %d%%. Wrong answers are saved in Review my mistakes.",
                 current()->bestExam);
    }
    drawTextCentred(line, (Rectangle){ box.x, y + 34, box.width, 24 }, 17, FONT_REGULAR, COL_MUTED);
    if (anyBadges)
    {
        drawNewBadges(box, y + 78);
    }

    float by = box.y + box.height - 76;
    if (button((Rectangle){ box.x + 40, by, 200, 48 }, "See answers", BTN_SECONDARY, true))
    {
        runKind = RUN_EXAM_REVIEW;
        questionIndex = 0;
        resetQuestion();
        showExamAnswer();
        screen = SCREEN_QUIZ;
    }
    if (button((Rectangle){ box.x + 250, by, 200, 48 }, "Back to Practise", BTN_SECONDARY, true))
    {
        screen = SCREEN_PRACTISE;
    }
    if (button((Rectangle){ box.x + 460, by, 200, 48 }, "New exam", BTN_PRIMARY, true))
    {
        startRun(RUN_EXAM);
    }
}

/* ---------- Results ---------- */

static void drawBadgeMedal(float cx, float cy, float radius, int badge, bool earned)
{
    static const char *glyphs[BADGE_COUNT] = { "1", "100", "for", "B", "fx", "*p", "A+", "90", "7d", "fix" };
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

/* "New badge!" and the badges just earned, centred in the box from y down */
static void drawNewBadges(Rectangle box, float y)
{
    int shown = 0;
    for (int b = 0; b < BADGE_COUNT; b++)
    {
        shown += (newBadges >> b) & 1u;
    }
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

    if (shown > 0)
    {
        drawNewBadges(box, y);
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

/* ---------- History ---------- */

static void recordAttempt(AttemptKind kind, int lesson, int right, int total, int percent)
{
    Attempt a;
    memset(&a, 0, sizeof(a));
    snprintf(a.learner, sizeof(a.learner), "%s", current()->name);
    a.kind = kind;
    a.lesson = lesson;
    a.right = right;
    a.total = total;
    a.percent = percent;
    a.day = today();
    addAttempt(&history, &a);
    appendAttemptToFile(HISTORY_FILE, &a);
}

/* ---------- Glossary ---------- */

static void openGlossary(Screen back)
{
    glossaryReturn = back;
    glossaryQuery[0] = '\0';
    glossaryScroll = 0;
    screen = SCREEN_GLOSSARY;
}

/* The pop-up for one term, drawn on top of everything */
static void drawTermPopup(void)
{
    if (popupTerm == NULL)
    {
        return;
    }
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 20, 24, 36, 120 });
    const Term *t = popupTerm;
    float w = 640;
    float meansH = drawRich(t->means, 0, 0, w - 64, 18, COL_INK, false);
    float h = 150 + meansH + (t->example[0] != '\0' ? 70 : 0);
    Rectangle box = { (SCREEN_W - w) / 2, (SCREEN_H - h) / 2, w, h };
    panel(box, COL_PANEL);

    drawText(t->word, box.x + 32, box.y + 26, 30, FONT_BOLD, COL_INK);
    char from[100];
    snprintf(from, sizeof(from), "From lesson %d: %s", course.lessons[t->lesson].number, course.lessons[t->lesson].title);
    drawText(from, box.x + 32, box.y + 66, 15, FONT_BOLD, COL_ACCENT);
    float y = box.y + 96;
    y += drawRich(t->means, box.x + 32, y, w - 64, 18, COL_INK, true) + 12;
    if (t->example[0] != '\0')
    {
        Rectangle code = { box.x + 32, y, w - 64, 46 };
        DrawRectangleRounded(code, 0.2f, 8, COL_CODE_BG);
        drawCodeLine(t->example, code.x + 16, code.y + 13, 17);
    }

    Rectangle close = { box.x + w - 152, box.y + h - 62, 120, 42 };
    Rectangle go = { box.x + 32, box.y + h - 62, 200, 42 };
    if (button(close, "Close", BTN_PRIMARY, true) || IsKeyPressed(KEY_ESCAPE) ||
        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), box)))
    {
        popupTerm = NULL;
        return;
    }
    char label[40];
    snprintf(label, sizeof(label), "Go to lesson %d", course.lessons[t->lesson].number);
    bool canGo = lessonUnlocked(current(), &course, t->lesson) && !(screen == SCREEN_CARDS && lessonIndex == t->lesson);
    if (canGo && button(go, label, BTN_QUIET, true))
    {
        Screen back = screen;
        popupTerm = NULL;
        if (back == SCREEN_CARDS)
        {
            back = peeking ? peekReturn : SCREEN_PATH;   /* don't stack one lesson on another */
        }
        peekLesson(t->lesson, back);
    }
}

static void screenGlossary(void)
{
    header();
    if (button((Rectangle){ 40, 84, 140, 36 }, "< Back", BTN_QUIET, true))
    {
        screen = glossaryReturn;
    }
    drawText("Glossary", 60, 126, 32, FONT_BOLD, COL_INK);

    if (glossary.count == 0)
    {
        drawText(glossaryError, 60, 180, 18, FONT_MONO, COL_BAD);
        return;
    }

    /* Search box: type straight away, Backspace to delete */
    if (popupTerm == NULL)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            size_t len = strlen(glossaryQuery);
            if (key >= 32 && key < 127 && len < sizeof(glossaryQuery) - 1)
            {
                glossaryQuery[len] = (char)key;
                glossaryQuery[len + 1] = '\0';
                glossaryScroll = 0;
            }
            key = GetCharPressed();
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && glossaryQuery[0] != '\0')
        {
            glossaryQuery[strlen(glossaryQuery) - 1] = '\0';
        }
    }
    Rectangle box = { 700, 126, 440, 44 };
    DrawRectangleRounded(box, 0.3f, 8, COL_PANEL);
    DrawRectangleRoundedLinesEx(box, 0.3f, 8, 2, COL_ACCENT);
    if (glossaryQuery[0] == '\0')
    {
        drawText("Type to search, e.g. pointer", box.x + 16, box.y + 11, 18, FONT_REGULAR, COL_MUTED);
    }
    drawText(glossaryQuery, box.x + 16, box.y + 11, 18, FONT_REGULAR, COL_INK);
    if ((int)(GetTime() * 2) % 2 == 0)
    {
        DrawRectangle((int)(box.x + 18 + textWidth(glossaryQuery, 18, FONT_REGULAR)), (int)box.y + 11, 2, 22, COL_ACCENT);
    }

    static const Term *found[200];
    int n = searchGlossary(&glossary, glossaryQuery, found, 200);
    char count[60];
    snprintf(count, sizeof(count), "%d term%s", n, n == 1 ? "" : "s");
    drawText(count, 60, 172, 16, FONT_REGULAR, COL_MUTED);

    /* The terms in three columns, scrolling with the mouse wheel */
    const float top = 204, bottom = SCREEN_H - 16, w = 346, h = 104, gap = 21;
    int rows = (n + 2) / 3;
    float contentH = rows * (h + gap);
    float maxScroll = contentH - (bottom - top);
    if (maxScroll < 0) maxScroll = 0;
    if (!uiBlocked()) glossaryScroll -= GetMouseWheelMove() * 60;
    if (glossaryScroll > maxScroll) glossaryScroll = maxScroll;
    if (glossaryScroll < 0) glossaryScroll = 0;

    BeginScissorMode(0, (int)top, SCREEN_W, (int)(bottom - top));
    for (int i = 0; i < n; i++)
    {
        Rectangle r = { 60 + (i % 3) * (w + gap), top + (i / 3) * (h + gap) - glossaryScroll, w, h };
        if (r.y + r.height < top || r.y > bottom)
        {
            continue;
        }
        bool hover = mouseOver(r) && GetMouseY() >= top && GetMouseY() <= bottom;
        panel(r, hover ? COL_ACCENT_SOFT : COL_PANEL);
        drawText(found[i]->word, r.x + 18, r.y + 14, 18, FONT_BOLD, COL_INK);
        char lesson[20];
        snprintf(lesson, sizeof(lesson), "LESSON %d", course.lessons[found[i]->lesson].number);
        drawText(lesson, r.x + w - 18 - textWidth(lesson, 12, FONT_BOLD), r.y + 18, 12, FONT_BOLD, COL_ACCENT);

        /* the start of the meaning: as much as fits in two lines */
        char shortText[110];
        snprintf(shortText, sizeof(shortText), "%.109s", found[i]->means);
        if (strlen(found[i]->means) > 95)
        {
            int cut = 92;
            while (cut > 0 && shortText[cut] != ' ') cut--;
            snprintf(shortText + cut, sizeof(shortText) - cut, "...");
        }
        drawRich(shortText, r.x + 18, r.y + 44, w - 36, 14, COL_MUTED, true);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !uiBlocked())
        {
            popupTerm = found[i];
        }
    }
    EndScissorMode();
    if (n == 0)
    {
        drawText("No terms match that. Try a shorter word.", 60, top + 10, 18, FONT_REGULAR, COL_MUTED);
    }
}

/* Key words on a lesson card: terms from this lesson or earlier that the
   card mentions, this lesson's first. Drawn as chips between x and maxX. */
static void cardKeyWords(const Card *card, float x, float y, float maxX)
{
    static char text[MAX_PARAS * TEXT_LEN];
    text[0] = '\0';
    for (int i = 0; i < card->textCount; i++)
    {
        strcat(text, card->text[i]);
        strcat(text, " ");
    }
    for (char *p = text; *p != '\0'; p++)
    {
        if (*p == '`') *p = ' ';   /* `array` counts as array */
    }

    const Term *found[40];
    int n = termsInText(&glossary, text, found, 40);
    const Term *order[40];
    int count = 0;
    for (int pass = 0; pass < 2; pass++)
    {
        for (int i = 0; i < n; i++)
        {
            bool same = found[i]->lesson == lessonIndex;
            if ((pass == 0 && same) || (pass == 1 && !same && found[i]->lesson < lessonIndex))
            {
                order[count++] = found[i];
            }
        }
    }
    if (count == 0)
    {
        return;
    }

    drawText("Key words:", x, y + 13, 15, FONT_REGULAR, COL_MUTED);
    float cx = x + textWidth("Key words:", 15, FONT_REGULAR) + 10;
    for (int i = 0; i < count; i++)
    {
        float w = textWidth(order[i]->word, 15, FONT_BOLD) + 24;
        if (cx + w > maxX)
        {
            break;
        }
        Rectangle chip = { cx, y + 8, w, 30 };
        DrawRectangleRounded(chip, 0.5f, 8, mouseOver(chip) ? COL_ACCENT : COL_ACCENT_SOFT);
        drawTextCentred(order[i]->word, chip, 15, FONT_BOLD, mouseOver(chip) ? WHITE : COL_ACCENT);
        if (clicked(chip))
        {
            popupTerm = order[i];
        }
        cx += w + 8;
    }
}

/* ---------- My progress ---------- */

static Color attemptColour(AttemptKind kind)
{
    switch (kind)
    {
        case ATTEMPT_LESSON: return COL_ACCENT;
        case ATTEMPT_QUICK:  return COL_GOOD;
        case ATTEMPT_REVIEW: return COL_BAD;
        default:             return COL_GOLD;
    }
}

static void statTile(Rectangle r, const char *value, const char *label)
{
    panel(r, COL_PANEL);
    drawText(value, r.x + 18, r.y + 12, 28, FONT_BOLD, COL_INK);
    drawText(label, r.x + 18, r.y + 52, 15, FONT_REGULAR, COL_MUTED);
}

static void screenProgress(void)
{
    header();
    const Learner *me_ = current();
    drawText("My progress", 60, 84, 32, FONT_BOLD, COL_INK);
    drawText("How you're doing, and what to go over before the exam.", 60, 128, 17, FONT_REGULAR, COL_MUTED);

    /* Tiles */
    int passed = 0;
    for (int i = 0; i < course.lessonCount; i++)
    {
        passed += lessonPassed(me_, i);
    }
    int solved = 0;
    for (int i = 0; i < exercises.count; i++)
    {
        solved += (me_->codeSolved >> i) & 1u;
    }
    char v[5][24];
    snprintf(v[0], sizeof(v[0]), "%d / %d", passed, course.lessonCount);
    snprintf(v[1], sizeof(v[1]), "%d / %d", solved, exercises.count);
    snprintf(v[2], sizeof(v[2]), me_->bestExam >= 0 ? "%d%%" : "None yet", me_->bestExam);
    snprintf(v[3], sizeof(v[3]), "%d", missedCount(me_));
    snprintf(v[4], sizeof(v[4]), "%d day%s", me_->bestStreak, me_->bestStreak == 1 ? "" : "s");
    const char *labels[5] = { "Lessons passed", "Exercises solved", "Best exam score", "Mistakes to review", "Best streak" };
    for (int i = 0; i < 5; i++)
    {
        statTile((Rectangle){ 60 + i * 219, 166, 204, 84 }, v[i], labels[i]);
    }

    /* Chart of the last scores */
    Rectangle chart = { 60, 270, 700, 420 };
    panel(chart, COL_PANEL);
    drawText("Your last scores", chart.x + 24, chart.y + 18, 20, FONT_BOLD, COL_INK);
    const Attempt *recent[15];
    int n = recentAttempts(&history, me_->name, recent, 15);
    Rectangle plot = { chart.x + 64, chart.y + 70, chart.width - 100, 250 };

    for (int g = 0; g <= 100; g += 25)
    {
        float gy = plot.y + plot.height * (100 - g) / 100.0f;
        DrawLineEx((Vector2){ plot.x, gy }, (Vector2){ plot.x + plot.width, gy }, 1, COL_LINE);
        char label[8];
        snprintf(label, sizeof(label), "%d%%", g);
        drawText(label, plot.x - 12 - textWidth(label, 13, FONT_REGULAR), gy - 8, 13, FONT_REGULAR, COL_MUTED);
    }
    /* the pass mark, dashed */
    float passY = plot.y + plot.height * (100 - PASS_PERCENT) / 100.0f;
    for (float dx = 0; dx < plot.width; dx += 14)
    {
        DrawLineEx((Vector2){ plot.x + dx, passY }, (Vector2){ plot.x + dx + 7, passY }, 2, COL_GOLD);
    }
    drawText("pass", plot.x + plot.width + 6, passY - 8, 13, FONT_BOLD, COL_GOLD);

    if (n == 0)
    {
        drawTextCentred("Finish a lesson, quiz or exam to see your scores here.",
                        (Rectangle){ plot.x, plot.y + plot.height / 2 - 30, plot.width, 30 }, 17, FONT_REGULAR, COL_MUTED);
    }
    else
    {
        float step = (n > 1) ? plot.width / (n - 1) : 0;
        Vector2 prev = { 0 };
        for (int i = 0; i < n; i++)
        {
            Vector2 p = { plot.x + (n > 1 ? i * step : plot.width / 2),
                          plot.y + plot.height * (100 - recent[i]->percent) / 100.0f };
            if (i > 0)
            {
                DrawLineEx(prev, p, 3, (Color){ 76, 91, 212, 110 });
            }
            prev = p;
        }
        for (int i = 0; i < n; i++)
        {
            Vector2 p = { plot.x + (n > 1 ? i * step : plot.width / 2),
                          plot.y + plot.height * (100 - recent[i]->percent) / 100.0f };
            DrawCircleV(p, 8, WHITE);
            DrawCircleV(p, 6, attemptColour(recent[i]->kind));
            if (CheckCollisionPointCircle(GetMousePosition(), p, 10) && !uiBlocked())
            {
                char tip[100];
                if (recent[i]->lesson >= 0)
                {
                    snprintf(tip, sizeof(tip), "%s %d: %d%% (%d of %d)", ATTEMPT_NAMES[recent[i]->kind],
                             course.lessons[recent[i]->lesson].number, recent[i]->percent, recent[i]->right, recent[i]->total);
                }
                else
                {
                    snprintf(tip, sizeof(tip), "%s: %d%% (%d of %d)", ATTEMPT_NAMES[recent[i]->kind],
                             recent[i]->percent, recent[i]->right, recent[i]->total);
                }
                float tw = textWidth(tip, 14, FONT_REGULAR) + 20;
                float tx = p.x - tw / 2;
                if (tx < chart.x + 8) tx = chart.x + 8;
                if (tx + tw > chart.x + chart.width - 8) tx = chart.x + chart.width - 8 - tw;
                Rectangle tipBox = { tx, p.y - 40, tw, 28 };
                DrawRectangleRounded(tipBox, 0.4f, 8, COL_INK);
                drawTextCentred(tip, tipBox, 14, FONT_REGULAR, WHITE);
            }
        }
    }

    /* Key */
    float kx = chart.x + 64;
    for (int k = 0; k < ATTEMPT_KINDS; k++)
    {
        DrawCircle((int)kx + 6, (int)(chart.y + chart.height - 34), 6, attemptColour((AttemptKind)k));
        drawText(ATTEMPT_NAMES[k], kx + 18, chart.y + chart.height - 43, 15, FONT_REGULAR, COL_INK);
        kx += textWidth(ATTEMPT_NAMES[k], 15, FONT_REGULAR) + 46;
    }

    /* Weakest topics */
    Rectangle side = { 790, 270, 350, 330 };
    panel(side, COL_PANEL);
    drawText("Topics to go over", side.x + 24, side.y + 18, 20, FONT_BOLD, COL_INK);
    int weak[3];
    int w = weakestTopics(me_, &course, weak, 3);
    if (w == 0)
    {
        drawRich("No weak spots yet. Once you've answered a few questions in a lesson, the ones you find hardest show up here.",
                 side.x + 24, side.y + 62, side.width - 48, 16, COL_MUTED, true);
    }
    for (int i = 0; i < w; i++)
    {
        int l = weak[i];
        float y = side.y + 64 + i * 84;
        char title[90];
        snprintf(title, sizeof(title), "%d. %s", course.lessons[l].number, course.lessons[l].title);
        drawText(title, side.x + 24, y, 17, FONT_BOLD, COL_INK);
        int pct = topicPercent(me_, l);
        Rectangle bar = { side.x + 24, y + 30, 190, 10 };
        DrawRectangleRounded(bar, 1, 6, COL_LINE);
        DrawRectangleRounded((Rectangle){ bar.x, bar.y, bar.width * pct / 100.0f, bar.height }, 1, 6,
                             pct >= PASS_PERCENT ? COL_GOLD : COL_BAD);
        char right[30];
        snprintf(right, sizeof(right), "%d%% right", pct);
        drawText(right, bar.x + bar.width + 10, y + 26, 14, FONT_REGULAR, COL_MUTED);
        if (button((Rectangle){ side.x + 20, y + 46, 110, 30 }, "Go over it", BTN_QUIET, true))
        {
            peekLesson(l, SCREEN_PROGRESS);
        }
    }

    if (button((Rectangle){ 790, 620, 350, 52 }, "Open the glossary", BTN_SECONDARY, true))
    {
        openGlossary(SCREEN_PROGRESS);
    }
}

/* ---------- Code tab ---------- */

#define PLAYGROUND MAX_EXERCISES   /* the playground's slot in drafts[] */

static const char *PLAYGROUND_C =
    "int main(void)\n{\n    printf(\"Hello from the playground!\\n\");\n    return 0;\n}\n";
static const char *PLAYGROUND_CPP =
    "int main()\n{\n    cout << \"Hello from the playground!\" << endl;\n    return 0;\n}\n";

static void saveDraft(void)
{
    int slot = exerciseIndex < 0 ? PLAYGROUND : exerciseIndex;
    tbGetText(&editor, drafts[slot], CODE_TEXT_LEN);
    hasDraft[slot] = true;
}

static void openEditor(int index)
{
    exerciseIndex = index;
    int slot = index < 0 ? PLAYGROUND : index;
    if (hasDraft[slot])
    {
        tbSetText(&editor, drafts[slot]);
    }
    else if (index < 0)
    {
        tbSetText(&editor, playgroundCpp ? PLAYGROUND_CPP : PLAYGROUND_C);
    }
    else
    {
        tbSetText(&editor, exerciseAt(&exercises, index)->starter);
    }
    editorScroll = 0;
    editorFocused = true;
    hasResult = false;
    runPending = false;
    showHint = false;
    showAnswer = false;
    failedRuns = 0;
    peeking = false;
    screen = SCREEN_EDITOR;
}

static void screenCode(void)
{
    header();
    drawText("Code", 60, 84, 32, FONT_BOLD, COL_INK);
    drawText("Write real C and C++, like in the coding exams. Your code is compiled with gcc and checked for you.",
             60, 128, 17, FONT_REGULAR, COL_MUTED);

    /* Playground banner */
    Rectangle banner = { 60, 166, 1080, 74 };
    panel(banner, COL_CODE_BG);
    drawText("Playground", banner.x + 24, banner.y + 14, 22, FONT_BOLD, WHITE);
    drawText("Write and run any C or C++ program and see what it prints.", banner.x + 24, banner.y + 44, 16,
             FONT_REGULAR, COL_CODE_NUM);
    if (button((Rectangle){ banner.x + banner.width - 224, banner.y + 15, 200, 44 }, "Open playground", BTN_PRIMARY, true))
    {
        openEditor(-1);
    }

    if (exercises.count == 0)
    {
        drawText(exerciseError, 60, 270, 18, FONT_MONO, COL_BAD);
        return;
    }

    int solved = 0;
    for (int i = 0; i < exercises.count; i++)
    {
        solved += (current()->codeSolved >> i) & 1u;
    }
    char line[80];
    snprintf(line, sizeof(line), "Exercises: %d of %d solved. Each opens when you pass its lesson.", solved, exercises.count);
    drawText(line, 60, 254, 16, FONT_BOLD, COL_INK);

    const float w = 258, h = 74, gap = 16, top = 284;
    for (int i = 0; i < exercises.count; i++)
    {
        const Exercise *ex = exerciseAt(&exercises, i);
        Rectangle r = { 60 + (i % 4) * (w + gap), top + (i / 4) * (h + 8), w, h };
        bool open = exerciseUnlocked(current(), ex);
        bool done = (current()->codeSolved >> i) & 1u;
        bool hover = open && mouseOver(r);
        panel(r, !open ? (Color){ 250, 249, 246, 255 } : (hover ? COL_ACCENT_SOFT : COL_PANEL));

        snprintf(line, sizeof(line), "LESSON %d  -  %s", course.lessons[ex->lesson].number, ex->cpp ? "C++" : "C");
        drawText(line, r.x + 16, r.y + 12, 12, FONT_BOLD, open ? COL_ACCENT : COL_MUTED);
        drawText(ex->title, r.x + 16, r.y + 38, 16, FONT_BOLD, open ? COL_INK : COL_MUTED);

        Color fill, ink;
        if (done)       { snprintf(line, sizeof(line), "Solved"); fill = COL_GOOD_SOFT; ink = COL_GOOD; }
        else if (open)  { snprintf(line, sizeof(line), "Start"); fill = COL_ACCENT; ink = WHITE; }
        else            { snprintf(line, sizeof(line), "Locked"); fill = COL_BG; ink = COL_MUTED; }
        float pw = textWidth(line, 13, FONT_BOLD) + 20;
        Rectangle pill = { r.x + w - pw - 12, r.y + 9, pw, 22 };
        DrawRectangleRounded(pill, 0.5f, 8, fill);
        drawTextCentred(line, pill, 13, FONT_BOLD, ink);

        if (open && clicked(r))
        {
            openEditor(i);
        }
    }
}

/* Compiles and runs what's in the editor, then checks it */
static void runEditorCode(void)
{
    static char code[TB_LINES * TB_COLS];
    static char program[TB_LINES * TB_COLS + CODE_TEXT_LEN + 1000];
    tbGetText(&editor, code, sizeof(code));
    saveDraft();

    const Exercise *ex = exerciseIndex >= 0 ? exerciseAt(&exercises, exerciseIndex) : NULL;
    bool cpp = ex != NULL ? ex->cpp : playgroundCpp;
    buildProgram(code, ex != NULL ? ex->tests : NULL, cpp, program, sizeof(program));
    runCode(program, cpp, RUN_TIME_LIMIT_MS, &runResult);
    hasResult = true;
    justSolvedFirst = false;

    if (ex != NULL && runResult.status == RUN_OK)
    {
        checksPassed = checkOutput(ex, runResult.output, checks);
        if (checksPassed == ex->expectCount)
        {
            justSolvedFirst = finishExercise(current(), exerciseIndex);
            newBadges = 0;
            didActivity();
            save();
            if (justSolvedFirst)
            {
                char message[100];
                snprintf(message, sizeof(message), "Solved! +%d points", CODE_POINTS);
                toast(message);
            }
            return;
        }
    }
    if (ex != NULL)
    {
        failedRuns++;
    }
}

/* Draws up to maxLines lines of text in the mono font, from y down */
static void drawMonoLines(const char *text, float x, float y, int maxLines, float size, Color colour, float maxW)
{
    char line[160];
    const char *p = text;
    for (int i = 0; i < maxLines && *p != '\0'; i++)
    {
        int len = (int)strcspn(p, "\n");
        int n = len < (int)sizeof(line) - 1 ? len : (int)sizeof(line) - 1;
        memcpy(line, p, n);
        line[n] = '\0';
        /* cut lines that are too wide */
        while (n > 0 && textWidth(line, size, FONT_MONO) > maxW)
        {
            line[--n] = '\0';
        }
        drawText(line, x, y + i * (size + 6), size, FONT_MONO, colour);
        p += len;
        if (*p == '\n') p++;
    }
}

/* The results box under the editor */
static void drawResults(Rectangle r, const Exercise *ex)
{
    panel(r, COL_PANEL);
    float x = r.x + 20, y = r.y + 16, maxW = r.width - 40;
    char title[120];

    if (runPending)
    {
        drawText("Compiling and running...", x, y, 19, FONT_BOLD, COL_ACCENT);
        return;
    }
    if (!hasResult)
    {
        drawRich(ex != NULL ? "Press Run (or Ctrl+Enter) to compile your code and test it." :
                              "Press Run (or Ctrl+Enter) to compile your program and see what it prints.",
                 x, y, maxW, 17, COL_MUTED, true);
        return;
    }

    switch (runResult.status)
    {
        case RUN_COMPILE_ERROR:
            drawText("Your code didn't compile. The first error is usually the one to fix:", x, y, 18, FONT_BOLD, COL_BAD);
            drawMonoLines(runResult.errors, x, y + 34, 6, 14, COL_INK, maxW);
            return;
        case RUN_NO_COMPILER:
        case RUN_FAILED:
            drawRich(runResult.errors, x, y, maxW, 17, COL_BAD, true);
            return;
        case RUN_TIMEOUT:
            drawText("Stopped after 3 seconds. Is there a loop that never ends?", x, y, 18, FONT_BOLD, COL_BAD);
            break;
        case RUN_TOO_MUCH:
            drawText("Stopped: it printed far too much. Is there a loop that never ends?", x, y, 18, FONT_BOLD, COL_BAD);
            break;
        case RUN_CRASHED:
            drawText("Your program crashed. Check for bad pointers or going past the end of an array.", x, y, 17, FONT_BOLD, COL_BAD);
            break;
        default:
            break;
    }

    if (ex == NULL || runResult.status != RUN_OK)
    {
        /* Playground, or a run that went wrong: show what it printed */
        float oy = (runResult.status == RUN_OK) ? y : y + 30;
        if (runResult.status == RUN_OK)
        {
            snprintf(title, sizeof(title), "Output  (finished in %.1fs, exit code %d)", runResult.seconds, runResult.exitCode);
            drawText(title, x, oy, 16, FONT_BOLD, COL_MUTED);
            oy += 28;
        }
        if (runResult.outputLen == 0)
        {
            drawText("(nothing was printed)", x, oy, 15, FONT_MONO, COL_MUTED);
        }
        else
        {
            drawMonoLines(runResult.output, x, oy, runResult.status == RUN_OK ? 6 : 5, 15, COL_INK, maxW);
        }
        return;
    }

    /* An exercise that ran: one row per check */
    bool all = checksPassed == ex->expectCount;
    snprintf(title, sizeof(title), all ? "All %d checks passed!" : "%d of %d checks passed", all ? ex->expectCount : checksPassed,
             ex->expectCount);
    drawText(title, x, y, 19, FONT_BOLD, all ? COL_GOOD : COL_INK);
    if (all)
    {
        drawText(justSolvedFirst ? "+30 points. Nice work!" : "You've solved this one before. Nice work!",
                 x + textWidth(title, 19, FONT_BOLD) + 16, y + 2, 16, FONT_REGULAR, COL_GOOD);
    }
    int shown = ex->expectCount < 6 ? ex->expectCount : 6;
    for (int i = 0; i < shown; i++)
    {
        float ry = y + 34 + i * 23;
        const CheckLine *c = &checks[i];
        DrawCircle((int)x + 7, (int)ry + 9, 7, c->passed ? COL_GOOD : COL_BAD);
        if (c->passed)
        {
            DrawLineEx((Vector2){ x + 3, ry + 9 }, (Vector2){ x + 6, ry + 12 }, 2, WHITE);
            DrawLineEx((Vector2){ x + 6, ry + 12 }, (Vector2){ x + 11, ry + 6 }, 2, WHITE);
            drawText(c->expected, x + 24, ry, 15, FONT_MONO, COL_INK);
        }
        else
        {
            DrawLineEx((Vector2){ x + 4, ry + 6 }, (Vector2){ x + 10, ry + 12 }, 2, WHITE);
            DrawLineEx((Vector2){ x + 10, ry + 6 }, (Vector2){ x + 4, ry + 12 }, 2, WHITE);
            char row[2 * LINE_LEN + 40];
            snprintf(row, sizeof(row), "expected: %s   got: %s", c->expected, c->gotLine ? c->got : "(nothing)");
            drawMonoLines(row, x + 24, ry, 1, 15, COL_BAD, maxW - 24);
        }
    }
    if (ex->expectCount > shown)
    {
        drawText("...", x + 24, y + 34 + shown * 23, 15, FONT_MONO, COL_MUTED);
    }
}

static void screenEditor(void)
{
    const Exercise *ex = exerciseIndex >= 0 ? exerciseAt(&exercises, exerciseIndex) : NULL;

    if (showAnswer)
    {
        uiBlock(true);   /* nothing underneath the answer reacts */
    }

    /* Run what was asked for last frame (so "Compiling and running..." shows first) */
    if (runPending)
    {
        runPending = false;
        runEditorCode();
    }

    /* Top bar */
    DrawRectangle(0, 0, SCREEN_W, 64, COL_PANEL);
    DrawRectangle(0, 64, SCREEN_W, 1, COL_LINE);
    if (button((Rectangle){ 20, 14, 150, 36 }, "< Code", BTN_QUIET, true))
    {
        saveDraft();
        screen = SCREEN_CODE;
        return;
    }
    drawTextCentred(ex != NULL ? ex->title : "Playground", (Rectangle){ 0, 0, SCREEN_W, 64 }, 20, FONT_BOLD, COL_INK);
    bool cpp = ex != NULL ? ex->cpp : playgroundCpp;
    if (ex == NULL)
    {
        /* the playground can switch language */
        Rectangle langC = { SCREEN_W - 190, 16, 70, 32 }, langCpp = { SCREEN_W - 114, 16, 84, 32 };
        if (button(langC, "C", cpp ? BTN_SECONDARY : BTN_PRIMARY, true) && cpp)
        {
            playgroundCpp = false;
            tbSetText(&editor, PLAYGROUND_C);
            hasResult = false;
        }
        if (button(langCpp, "C++", cpp ? BTN_PRIMARY : BTN_SECONDARY, true) && !cpp)
        {
            playgroundCpp = true;
            tbSetText(&editor, PLAYGROUND_CPP);
            hasResult = false;
        }
    }
    else
    {
        const char *lang = cpp ? "C++" : "C";
        Rectangle tag = { SCREEN_W - 30 - textWidth(lang, 15, FONT_BOLD) - 24, 18, textWidth(lang, 15, FONT_BOLD) + 24, 28 };
        DrawRectangleRounded(tag, 0.5f, 8, COL_ACCENT_SOFT);
        drawTextCentred(lang, tag, 15, FONT_BOLD, COL_ACCENT);
    }

    /* Left: the task */
    Rectangle left = { 40, 84, 380, 616 };
    panel(left, COL_PANEL);
    float x = left.x + 22, w = left.width - 44, y = left.y + 20;
    if (ex != NULL)
    {
        char from[90];
        snprintf(from, sizeof(from), "LESSON %d: %s", course.lessons[ex->lesson].number, course.lessons[ex->lesson].title);
        drawText(from, x, y, 13, FONT_BOLD, COL_ACCENT);
        y += 28;
        y += drawRich(ex->task, x, y, w, 17, COL_INK, true) + 10;
        drawRich("Your code is joined to hidden test code that calls it, and each line it prints is checked.",
                 x, y, w, 14, COL_MUTED, true);
        y += 52;

        if (!showHint)
        {
            if (button((Rectangle){ x - 8, y, 140, 36 }, "Show a hint", BTN_QUIET, true)) showHint = true;
            y += 44;
        }
        else
        {
            float hh = drawRich(ex->hint, 0, 0, w - 28, 15, COL_INK, false) + 24;
            DrawRectangleRounded((Rectangle){ x, y, w, hh }, 0.12f, 8, COL_GOLD_SOFT);
            drawRich(ex->hint, x + 14, y + 12, w - 28, 15, COL_INK, true);
            y += hh + 12;
        }
        if (failedRuns >= 2 && !showAnswer)
        {
            if (button((Rectangle){ x - 8, y, 170, 36 }, "Show the answer", BTN_QUIET, true)) showAnswer = true;
        }
        else if (failedRuns < 2)
        {
            drawText("The answer appears after 2 tries.", x, y + 8, 14, FONT_REGULAR, COL_MUTED);
        }

        char link[90];
        snprintf(link, sizeof(link), "Go over lesson %d", course.lessons[ex->lesson].number);
        if (button((Rectangle){ x - 8, left.y + left.height - 56, textWidth(link, 19, FONT_BOLD) + 40, 40 }, link, BTN_QUIET, true))
        {
            saveDraft();
            peekLesson(ex->lesson, SCREEN_EDITOR);
            return;
        }
    }
    else
    {
        drawText("Try anything", x, y, 20, FONT_BOLD, COL_INK);
        y += 36;
        y += drawRich("Write a whole program with `main`. The usual headers (`stdio.h`, `string.h`, `iostream`...) "
                      "are already included for you.", x, y, w, 16, COL_INK, true) + 14;
        y += drawRich("Your program can't read input, so `scanf` and `cin` get nothing. Set your values in the code instead.",
                      x, y, w, 16, COL_MUTED, true) + 14;
        drawRich("Programs are stopped after 3 seconds, in case of a loop that never ends.", x, y, w, 16, COL_MUTED, true);
    }

    /* Right: the editor, the buttons and the results */
    Rectangle edit = { 440, 84, 720, 372 };
    codeEditor(edit, &editor, &editorScroll, &editorFocused);

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (button((Rectangle){ 440, 468, 150, 46 }, "Run", BTN_PRIMARY, !runPending) ||
        (ctrl && IsKeyPressed(KEY_ENTER) && !uiBlocked()))
    {
        runPending = true;   /* runs at the start of the next frame */
    }
    if (button((Rectangle){ 602, 468, 150, 46 }, "Start again", BTN_SECONDARY, true))
    {
        tbSetText(&editor, ex != NULL ? ex->starter : (cpp ? PLAYGROUND_CPP : PLAYGROUND_C));
        hasResult = false;
    }
    drawText("Ctrl+V pastes, Ctrl+C copies all your code", 770, 482, 14, FONT_REGULAR, COL_MUTED);
    drawResults((Rectangle){ 440, 526, 720, 174 }, ex);

    /* The model answer, over everything else */
    if (showAnswer && ex != NULL)
    {
        uiBlock(false);
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 20, 24, 36, 120 });
        int count = 1;
        for (const char *p = ex->solution; *p != '\0'; p++)
        {
            count += (*p == '\n' && p[1] != '\0');
        }
        const float lineH = 21;
        float codeH = count * lineH + 24;
        Rectangle box = { 240, (SCREEN_H - codeH - 150) / 2, 720, codeH + 150 };
        panel(box, COL_PANEL);
        drawText("One way to answer it", box.x + 28, box.y + 22, 22, FONT_BOLD, COL_INK);
        Rectangle code = { box.x + 28, box.y + 62, box.width - 56, codeH };
        DrawRectangleRounded(code, 0.04f, 8, COL_CODE_BG);
        const char *p = ex->solution;
        for (int i = 0; *p != '\0'; i++)
        {
            char line[120];
            int len = (int)strcspn(p, "\n");
            snprintf(line, sizeof(line), "%.*s", len < 119 ? len : 119, p);
            drawCodeLine(line, code.x + 18, code.y + 12 + i * lineH, 15);
            p += len;
            if (*p == '\n') p++;
        }
        if (button((Rectangle){ box.x + 28, box.y + box.height - 66, 220, 44 }, "Put it in the editor", BTN_SECONDARY, true))
        {
            tbSetText(&editor, ex->solution);
            showAnswer = false;
            hasResult = false;
        }
        if (button((Rectangle){ box.x + box.width - 148, box.y + box.height - 66, 120, 44 }, "Close", BTN_PRIMARY, true) ||
            IsKeyPressed(KEY_ESCAPE))
        {
            showAnswer = false;
        }
    }
}

/* ---------- Teach tab: help the duck ---------- */

static const Color DUCK_YELLOW = { 250, 204, 60, 255 };
static const Color DUCK_WING   = { 232, 176, 36, 255 };
static const Color DUCK_BEAK   = { 240, 136, 40, 255 };

/* The duck, drawn from circles and a triangle, centred on (cx, cy) */
static void drawDuck(float cx, float cy, float s)
{
    DrawEllipse((int)cx, (int)(cy + s * 0.35f), s * 0.95f, s * 0.6f, DUCK_YELLOW);             /* body */
    DrawEllipse((int)(cx - s * 0.15f), (int)(cy + s * 0.38f), s * 0.45f, s * 0.3f, DUCK_WING);  /* wing */
    float hx = cx + s * 0.4f, hy = cy - s * 0.35f;
    DrawCircle((int)hx, (int)hy, s * 0.48f, DUCK_YELLOW);                                        /* head */
    DrawTriangle((Vector2){ hx + s * 0.38f, hy - s * 0.04f }, (Vector2){ hx + s * 0.38f, hy + s * 0.2f },
                 (Vector2){ hx + s * 0.85f, hy + s * 0.08f }, DUCK_BEAK);                        /* beak */
    DrawCircle((int)(hx + s * 0.15f), (int)(hy - s * 0.12f), s * 0.08f, COL_INK);                 /* eye */
}

static void startDuck(int index)
{
    duckIndex = index;
    duckStep = 1;
    duckWrongLine = 0;
    duckWrongFix = 0;
    duckBadges = 0;
    screen = SCREEN_DUCK;
}

static void screenTeach(void)
{
    header();
    drawDuck(130, 168, 62);
    drawText("Help the duck", 230, 92, 32, FONT_BOLD, COL_INK);
    drawRich(DUCK_NAME " is learning C too, and he keeps getting stuck. He always has a guess about what's wrong, "
             "but he isn't always right! Find the line that's really broken, then pick the fix.",
             230, 138, 900, 17, COL_MUTED, true);

    if (ducks.count == 0)
    {
        drawText(duckError, 230, 220, 18, FONT_MONO, COL_BAD);
        return;
    }

    int helped = 0;
    for (int i = 0; i < ducks.count; i++)
    {
        helped += (current()->ducksHelped >> i) & 1u;
    }
    char line[60];
    snprintf(line, sizeof(line), "%d of %d problems fixed", helped, ducks.count);
    drawText(line, 230, 208, 16, FONT_BOLD, COL_INK);

    const float w = 204, h = 170, gap = 15;
    for (int i = 0; i < ducks.count; i++)
    {
        const DuckProblem *duck = duckAt(&ducks, i);
        Rectangle r = { 60 + (i % 5) * (w + gap), 250 + (i / 5) * (h + gap), w, h };
        bool open = duckUnlocked(current(), &course, duck);
        bool done = (current()->ducksHelped >> i) & 1u;
        bool hover = open && mouseOver(r);

        panel(r, !open ? (Color){ 250, 249, 246, 255 } : (hover ? COL_ACCENT_SOFT : COL_PANEL));
        snprintf(line, sizeof(line), "LESSON %d", course.lessons[duck->lesson].number);
        drawText(line, r.x + 16, r.y + 16, 13, FONT_BOLD, open ? COL_ACCENT : COL_MUTED);
        drawRich(duck->title, r.x + 16, r.y + 40, w - 32, 17, open ? COL_INK : COL_MUTED, true);

        Color fill, ink;
        if (done)
        {
            snprintf(line, sizeof(line), "Fixed");
            fill = COL_GOOD_SOFT;
            ink = COL_GOOD;
        }
        else if (open)
        {
            snprintf(line, sizeof(line), "Help him");
            fill = COL_ACCENT;
            ink = WHITE;
        }
        else
        {
            snprintf(line, sizeof(line), "Pass lesson %d first", course.lessons[duck->lesson].number);
            fill = COL_BG;
            ink = COL_MUTED;
        }
        Rectangle pill = { r.x + 16, r.y + h - 40, textWidth(line, 14, FONT_BOLD) + 24, 26 };
        DrawRectangleRounded(pill, 0.5f, 8, fill);
        drawTextCentred(line, pill, 14, FONT_BOLD, ink);

        if (open && clicked(r))
        {
            startDuck(i);
        }
    }
}

/* Next problem after this one that's open and not fixed yet, or -1 */
static int nextDuck(void)
{
    for (int k = 1; k < ducks.count; k++)
    {
        int i = (duckIndex + k) % ducks.count;
        if (duckUnlocked(current(), &course, duckAt(&ducks, i)) && !((current()->ducksHelped >> i) & 1u))
        {
            return i;
        }
    }
    return -1;
}

static void screenDuck(void)
{
    const DuckProblem *duck = duckAt(&ducks, duckIndex);
    if (duck == NULL)
    {
        screen = SCREEN_TEACH;
        return;
    }

    /* Top bar */
    DrawRectangle(0, 0, SCREEN_W, 64, COL_PANEL);
    DrawRectangle(0, 64, SCREEN_W, 1, COL_LINE);
    if (button((Rectangle){ 20, 14, 150, 36 }, "< Teach", BTN_QUIET, true))
    {
        screen = SCREEN_TEACH;
    }
    char text[TEXT_LEN + 40];
    snprintf(text, sizeof(text), "Help " DUCK_NAME ": %s", duck->title);
    drawTextCentred(text, (Rectangle){ 0, 0, SCREEN_W, 64 }, 20, FONT_BOLD, COL_INK);
    const char *step = duckStep == 1 ? "Step 1 of 2" : (duckStep == 2 ? "Step 2 of 2" : "Fixed!");
    drawText(step, SCREEN_W - 30 - textWidth(step, 16, FONT_REGULAR), 23, 16, FONT_REGULAR, COL_MUTED);

    /* What the duck says depends on how it's going */
    if (duckStep == 1 && duckWrongLine == 0)
    {
        snprintf(text, sizeof(text), "%s", duck->says);
    }
    else if (duckStep == 1)
    {
        snprintf(text, sizeof(text), "Hmm, line %d looks fine to me. Which line is really wrong?", duckWrongLine);
    }
    else if (duckStep == 2 && duckWrongFix == 0)
    {
        snprintf(text, sizeof(text), "Line %d? Ooh, you might be right! How should I write it instead?", duck->bugLine);
    }
    else if (duckStep == 2)
    {
        snprintf(text, sizeof(text), "I tried that, but it still isn't right. Any other ideas?");
    }
    else
    {
        snprintf(text, sizeof(text), "%s", duck->thanks);
    }

    /* Duck and speech bubble */
    drawDuck(110, 148, 46);
    float bx = 200, bw = SCREEN_W - 60 - bx;
    float textH = drawRich(text, 0, 0, bw - 48, 19, COL_INK, false);
    Rectangle bubble = { bx, 86, bw, textH + 30 };
    DrawTriangle((Vector2){ bx + 2, 112 }, (Vector2){ bx - 22, 132 }, (Vector2){ bx + 2, 140 }, COL_PANEL);
    DrawRectangleRounded(bubble, 0.25f, 8, COL_PANEL);
    drawRich(text, bx + 24, bubble.y + 14, bw - 48, 19, COL_INK, true);

    float y = bubble.y + bubble.height + 24;
    if (y < 236)
    {
        y = 236;
    }

    /* The code: once fixed, the broken line is swapped for the right fix */
    char code[MAX_CODE][LINE_LEN];
    for (int i = 0; i < duck->codeCount; i++)
    {
        snprintf(code[i], LINE_LEN, "%s", duck->code[i]);
    }
    int mark = 0;
    Color markColour = BLANK;
    if (duckStep == 2)
    {
        mark = duck->bugLine;
        markColour = (Color){ 76, 91, 212, 170 };
    }
    else if (duckStep == 3)
    {
        /* keep the line's indentation, then the fix */
        const char *orig = duck->code[duck->bugLine - 1];
        int indent = 0;
        while (orig[indent] == ' ')
        {
            indent++;
        }
        char *fixed = code[duck->bugLine - 1];
        if (indent > 40)
        {
            indent = 40;
        }
        memset(fixed, ' ', indent);
        fixed[indent] = '\0';
        strncat(fixed, duck->fixes[duck->answer - 1], LINE_LEN - 1 - indent);
        mark = duck->bugLine;
        markColour = (Color){ 38, 145, 96, 150 };
    }
    Rectangle panelRect = { 60, y, 620, codePanelHeight(duck->codeCount) };
    int line = codePanel(panelRect, (const char (*)[120])code, duck->codeCount, duckStep == 1, mark, markColour,
                         duckStep == 1 ? duckWrongLine : 0);
    if (line > 0)
    {
        if (line == duck->bugLine)
        {
            duckStep = 2;
        }
        else
        {
            duckWrongLine = line;
        }
    }

    /* Right column: what to do now */
    float rx = 710, rw = 430;
    if (duckStep == 1)
    {
        drawText("Step 1: Which line is really wrong?", rx, y, 19, FONT_BOLD, COL_INK);
        drawRich("Don't just trust " DUCK_NAME "'s guess. Read the code and click the line that's broken.",
                 rx, y + 34, rw, 16, COL_MUTED, true);
    }
    else if (duckStep == 2)
    {
        snprintf(text, sizeof(text), "Step 2: How should line %d be written?", duck->bugLine);
        drawText(text, rx, y, 19, FONT_BOLD, COL_INK);
        for (int i = 0; i < duck->fixCount; i++)
        {
            int choice = i + 1;
            Rectangle r = { rx, y + 40 + i * 62, rw, 52 };
            bool wrong = choice == duckWrongFix;
            bool hover = !wrong && mouseOver(r);
            Color fill = wrong ? COL_BAD_SOFT : (hover ? COL_ACCENT_SOFT : COL_PANEL);
            Color edge = wrong ? COL_BAD : (hover ? COL_ACCENT : COL_LINE);
            DrawRectangleRounded(r, 0.25f, 8, fill);
            DrawRectangleRoundedLinesEx(r, 0.25f, 8, 1.5f, edge);

            /* shrink long fixes so they fit the button */
            float size = 17;
            float fw = textWidth(duck->fixes[i], size, FONT_MONO);
            if (fw > rw - 32)
            {
                size = size * (rw - 32) / fw;
            }
            drawText(duck->fixes[i], r.x + 16, r.y + (52 - size) / 2 - 1, size, FONT_MONO, wrong ? COL_BAD : COL_INK);
            if (!wrong && clicked(r))
            {
                if (choice == duck->answer)
                {
                    duckStep = 3;
                    bool firstTime;
                    duckBadges = finishDuck(current(), duckIndex, ducks.count, &firstTime);
                    duckFirstTime = firstTime;
                    didActivity();
                    save();
                    if (duckBadges & (1u << BADGE_DUCK_HELPER))
                    {
                        toast("You helped with every problem! You earned the Duck Helper badge.");
                    }
                }
                else
                {
                    duckWrongFix = choice;
                }
            }
        }
    }
    else
    {
        Rectangle done = { rx, y, rw, 96 };
        DrawRectangleRounded(done, 0.15f, 8, COL_GOOD_SOFT);
        drawText("You fixed it!", rx + 20, y + 16, 22, FONT_BOLD, COL_GOOD);
        if (duckFirstTime)
        {
            snprintf(text, sizeof(text), "+%d points for helping " DUCK_NAME, DUCK_POINTS);
        }
        else
        {
            snprintf(text, sizeof(text), "You've helped with this one before.");
        }
        drawText(text, rx + 20, y + 54, 17, FONT_REGULAR, COL_INK);
    }

    /* Bottom: a link back to the lesson, or move on */
    if (duckStep < 3)
    {
        snprintf(text, sizeof(text), "Stuck? Go over lesson %d: %s", course.lessons[duck->lesson].number,
                 course.lessons[duck->lesson].title);
        Rectangle link = { 50, SCREEN_H - 76, textWidth(text, 19, FONT_BOLD) + 40, 48 };
        if (button(link, text, BTN_QUIET, true))
        {
            peekLesson(duck->lesson, SCREEN_DUCK);
        }
    }
    else
    {
        int next = nextDuck();
        Rectangle back = { SCREEN_W - 480, SCREEN_H - 76, 200, 48 };
        Rectangle go = { SCREEN_W - 260, SCREEN_H - 76, 200, 48 };
        if (next >= 0)
        {
            if (button(back, "Back to Teach", BTN_SECONDARY, true))
            {
                screen = SCREEN_TEACH;
            }
            if (button(go, "Next problem", BTN_PRIMARY, true) || IsKeyPressed(KEY_ENTER))
            {
                startDuck(next);
            }
        }
        else if (button(go, "Back to Teach", BTN_PRIMARY, true) || IsKeyPressed(KEY_ENTER))
        {
            screen = SCREEN_TEACH;
        }
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
        Rectangle r = { 60 + (b % 5) * 219, 220 + (b / 5) * 220, 204, 200 };
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
    srand((unsigned)time(NULL));   /* different questions each practice run */
    loadFonts();

    if (loadCourse(COURSE_FILE, &course, courseError, sizeof(courseError)) == 0)
    {
        screen = SCREEN_ERROR;
    }
    learnerCount = loadLearners(LEARNERS_FILE, learners, MAX_LEARNERS);
    loadDucks(DUCK_FILE, &ducks, duckError, sizeof(duckError));   /* if it fails, Teach shows why */
    loadGlossary(GLOSSARY_FILE, &glossary, glossaryError, sizeof(glossaryError));
    loadExercises(EXERCISE_FILE, &exercises, exerciseError, sizeof(exerciseError));
    loadHistory(HISTORY_FILE, &history);
    tbClear(&editor);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(COL_BG);
        uiBeginFrame();
        uiBlock(popupTerm != NULL);   /* a glossary pop-up covers everything */

        switch (screen)
        {
            case SCREEN_SIGN_IN: screenSignIn(); break;
            case SCREEN_PATH:    screenPath(); break;
            case SCREEN_CARDS:   screenCards(); break;
            case SCREEN_QUIZ:    screenQuiz(); break;
            case SCREEN_RESULTS: screenResults(); break;
            case SCREEN_BADGES:  screenBadges(); break;
            case SCREEN_PRACTISE: screenPractise(); break;
            case SCREEN_PRACTICE_RESULTS: screenPracticeResults(); break;
            case SCREEN_EXAM_RESULTS: screenExamResults(); break;
            case SCREEN_TEACH:   screenTeach(); break;
            case SCREEN_DUCK:    screenDuck(); break;
            case SCREEN_CODE:    screenCode(); break;
            case SCREEN_EDITOR:  screenEditor(); break;
            case SCREEN_PROGRESS: screenProgress(); break;
            case SCREEN_GLOSSARY: screenGlossary(); break;
            case SCREEN_ERROR:   screenError(); break;
        }

        uiBlock(false);
        drawTermPopup();
        drawToast();
        uiEndFrame();
        EndDrawing();
    }

    freeCourse(&course);
    freeDucks(&ducks);
    freeGlossary(&glossary);
    freeExercises(&exercises);
    freeHistory(&history);
    cleanUpRun();
    unloadFonts();
    CloseWindow();
    return 0;
}
