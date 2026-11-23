/*
 * app.h - shared types for C Quest, an interactive C/C++ learning app.
 *
 * The course (lessons, tutorial cards, questions) is loaded from a text file
 * into linked lists. Each learner's progress is saved to learners.txt.
 */
#ifndef APP_H
#define APP_H

#include <stdbool.h>

#define MAX_LESSONS 12
#define LEVELS 3
#define NAME_LEN 32
#define TITLE_LEN 64
#define LINE_LEN 120          /* one line of code or one answer option */
#define TEXT_LEN 600          /* one paragraph of text */
#define MAX_CODE 18           /* lines of code in a card or question */
#define MAX_OPTIONS 5
#define MAX_PARAS 6           /* paragraphs on a tutorial card */
#define MAX_LEARNERS 50
#define PASS_PERCENT 70       /* score needed to pass a lesson */
#define POINTS_FIRST_TRY 10
#define POINTS_SECOND_TRY 5
#define MAX_QUESTIONS 32      /* per lesson (wrong answers are kept as bits) */

/* ---------- The course ---------- */

typedef enum {
    Q_CHOICE,                 /* pick the right answer */
    Q_OUTPUT,                 /* read the code, pick what it prints */
    Q_BUG                     /* click the line with the mistake */
} QuestionType;

/* A tutorial card: a short explanation, usually with a code example */
typedef struct Card {
    char title[TITLE_LEN];
    char text[MAX_PARAS][TEXT_LEN];
    int textCount;
    char code[MAX_CODE][LINE_LEN];
    int codeCount;
    struct Card *next;        /* linked list of a lesson's cards */
} Card;

typedef struct Question {
    QuestionType type;
    char text[TEXT_LEN];
    char code[MAX_CODE][LINE_LEN];
    int codeCount;
    char options[MAX_OPTIONS][LINE_LEN];
    int optionCount;
    int answer;               /* 1-based: the right option, or the buggy line */
    char explain[TEXT_LEN];
    struct Question *next;    /* linked list of a lesson's questions */
} Question;

typedef struct {
    int number;               /* 1 to 12 */
    int level;                /* 0 Beginner, 1 Intermediate, 2 Advanced */
    char title[TITLE_LEN];
    char summary[TEXT_LEN];
    Card *cards;              /* head of the cards list */
    int cardCount;
    Question *questions;      /* head of the questions list */
    int questionCount;
} Lesson;

typedef struct {
    Lesson lessons[MAX_LESSONS];
    int lessonCount;
} Course;

extern const char *LEVEL_NAMES[LEVELS];

/* course.c */
int loadCourse(const char *filename, Course *course, char *error, int errorSize);
void freeCourse(Course *course);
const Card *cardAt(const Lesson *lesson, int index);
const Question *questionAt(const Lesson *lesson, int index);
bool isAnswer(const Question *q, int choice);

/* ---------- Learners and progress ---------- */

typedef enum {
    BADGE_FIRST_STEPS,        /* pass lesson 1 */
    BADGE_PERFECT,            /* 100% on any lesson */
    BADGE_LOOP_MASTER,        /* pass lesson 3 */
    BADGE_BEGINNER,           /* pass every Beginner lesson */
    BADGE_PROBLEM_SOLVER,     /* pass every Intermediate lesson */
    BADGE_POINTER_PRO,        /* 100% on lesson 9 */
    BADGE_EXAM_READY,         /* pass every lesson */
    BADGE_EXAM_ACE,           /* 90%+ in a full exam */
    BADGE_ON_A_ROLL,          /* a 7-day streak */
    BADGE_DUCK_HELPER,        /* help the duck with every problem */
    BADGE_COUNT
} Badge;

extern const char *BADGE_NAMES[BADGE_COUNT];
extern const char *BADGE_DESCRIPTIONS[BADGE_COUNT];

typedef struct {
    char name[NAME_LEN];
    int bestPercent[MAX_LESSONS];   /* best score per lesson, -1 = not tried */
    int bestPoints[MAX_LESSONS];    /* points from that best attempt */
    unsigned badges;                /* one bit per Badge */
    unsigned missed[MAX_LESSONS];   /* questions to review: bit k = question k */
    int bestExam;                   /* best exam score %, -1 = not taken */
    int streak;                     /* days in a row with some learning */
    int bestStreak;
    int lastDay;                    /* day number of the last activity, -1 = never */
    unsigned ducksHelped;           /* one bit per duck problem solved */
    unsigned codeSolved;            /* one bit per coding exercise solved */
    int topicRight[MAX_LESSONS];    /* first-try answers per lesson: right... */
    int topicTried[MAX_LESSONS];    /* ...out of how many */
} Learner;

/* learner.c */
void newLearner(Learner *learner, const char *name);
int validLearnerName(const char *name);
int findLearner(const Learner *list, int count, const char *name);
int loadLearners(const char *filename, Learner *list, int max);
int saveLearners(const char *filename, const Learner *list, int count);

int pointsForAttempt(int attempt);
int scorePercent(int pointsEarned, int questionCount);
int totalPoints(const Learner *learner);
int badgeCount(const Learner *learner);
bool lessonPassed(const Learner *learner, int index);
bool lessonReady(const Course *course, int index);
bool lessonUnlocked(const Learner *learner, const Course *course, int index);
unsigned finishLesson(Learner *learner, const Course *course, int index, int pointsEarned);

/* Streaks: a day counts if the learner finishes a lesson, a practice run,
   an exam or a duck problem. Days are numbered from 1 January 1970. */
#define STREAK_BADGE_DAYS 7
int dayNumber(int year, int month, int day);
unsigned recordActivity(Learner *learner, int today, bool *grew);
int currentStreak(const Learner *learner, int today);

/* ---------- Practice ---------- */

#define QUICK_QUESTIONS 10
#define EXAM_QUESTIONS 20
#define EXAM_SECONDS (20 * 60)
#define EXAM_ACE_PERCENT 90
#define MAX_POOL (MAX_LESSONS * MAX_QUESTIONS)

/* Points at one question: which lesson, and which question in it (both from 0) */
typedef struct {
    int lesson;
    int index;
} QuestionRef;

typedef enum {
    POOL_PASSED,              /* every question in the lessons the learner has passed */
    POOL_MISSED               /* only the questions they got wrong, from any lesson */
} PoolKind;

/* practice.c */
int buildPool(const Learner *learner, const Course *course, PoolKind kind, QuestionRef *out, int max);
void shuffleRefs(QuestionRef *refs, int count);
const Question *questionFor(const Course *course, QuestionRef ref);
void recordAnswer(Learner *learner, QuestionRef ref, bool rightFirstTime);
bool isMissed(const Learner *learner, QuestionRef ref);
int missedCount(const Learner *learner);
int topicPercent(const Learner *learner, int lesson);
int weakestTopics(const Learner *learner, const Course *course, int *out, int max);
unsigned finishExam(Learner *learner, int right, int total, bool *newBest);

/* ---------- Teach: help the duck ---------- */

#define MAX_DUCKS 32
#define DUCK_POINTS 20

/* One of the duck's broken programs */
typedef struct DuckProblem {
    char title[TITLE_LEN];
    int lesson;                       /* the lesson it's about, from 0 */
    char says[TEXT_LEN];              /* the duck's (wrong) idea of the problem */
    char code[MAX_CODE][LINE_LEN];
    int codeCount;
    int bugLine;                      /* the line that's really wrong, from 1 */
    char fixes[MAX_OPTIONS][LINE_LEN];
    int fixCount;
    int answer;                       /* the right fix, from 1 */
    char thanks[TEXT_LEN];            /* what the duck learned */
    struct DuckProblem *next;
} DuckProblem;

typedef struct {
    DuckProblem *head;
    int count;
} DuckList;

/* duck.c */
int loadDucks(const char *filename, DuckList *ducks, char *error, int errorSize);
void freeDucks(DuckList *ducks);
const DuckProblem *duckAt(const DuckList *ducks, int index);
bool duckUnlocked(const Learner *learner, const Course *course, const DuckProblem *duck);
unsigned finishDuck(Learner *learner, int index, int duckCount, bool *firstTime);

/* ---------- History: every finished attempt ---------- */

typedef enum {
    ATTEMPT_LESSON,
    ATTEMPT_QUICK,
    ATTEMPT_REVIEW,
    ATTEMPT_EXAM,
    ATTEMPT_KINDS
} AttemptKind;

typedef struct Attempt {
    char learner[NAME_LEN];
    AttemptKind kind;
    int lesson;                 /* the lesson (from 0), or -1 for mixed practice */
    int percent;
    int right;                  /* answers right first time... */
    int total;                  /* ...out of this many */
    int day;                    /* dayNumber of when it happened */
    struct Attempt *next;
} Attempt;

typedef struct {
    Attempt *head;
    Attempt *tail;              /* kept so adding to the end is quick */
    int count;
} AttemptList;

extern const char *ATTEMPT_NAMES[ATTEMPT_KINDS];

/* history.c */
int loadHistory(const char *filename, AttemptList *list);
bool addAttempt(AttemptList *list, const Attempt *attempt);
bool appendAttemptToFile(const char *filename, const Attempt *attempt);
void freeHistory(AttemptList *list);
int recentAttempts(const AttemptList *list, const char *learner, const Attempt **out, int max);
int attemptCount(const AttemptList *list, const char *learner);

/* ---------- Glossary ---------- */

#define TERM_LEN 40

typedef struct Term {
    char word[TERM_LEN];
    int lesson;                 /* from 0 */
    char means[TEXT_LEN];
    char example[LINE_LEN];
    struct Term *next;
} Term;

typedef struct {
    Term *head;
    int count;
} Glossary;

/* glossary.c */
int loadGlossary(const char *filename, Glossary *glossary, char *error, int errorSize);
void freeGlossary(Glossary *glossary);
const Term *termAt(const Glossary *glossary, int index);
int searchGlossary(const Glossary *glossary, const char *query, const Term **out, int max);
int termsInText(const Glossary *glossary, const char *text, const Term **out, int max);

/* ---------- Code tab: the editor's text ---------- */

#define TB_LINES 200
#define TB_COLS 120

typedef struct {
    char lines[TB_LINES][TB_COLS];
    int count;                  /* lines in use (always at least 1) */
    int row, col;               /* the cursor */
} TextBuffer;

/* textbuf.c */
void tbClear(TextBuffer *tb);
void tbSetText(TextBuffer *tb, const char *text);
int tbGetText(const TextBuffer *tb, char *out, int size);
void tbSetCursor(TextBuffer *tb, int row, int col);
void tbInsertChar(TextBuffer *tb, char c);
void tbInsertText(TextBuffer *tb, const char *text);
void tbNewline(TextBuffer *tb);
void tbTab(TextBuffer *tb);
void tbBackspace(TextBuffer *tb);
void tbDelete(TextBuffer *tb);
void tbMove(TextBuffer *tb, int dRow, int dCol);
void tbHome(TextBuffer *tb);
void tbEnd(TextBuffer *tb);

/* ---------- Code tab: compiling and running ---------- */

#define RUN_OUTPUT_LEN 8192
#define RUN_TIME_LIMIT_MS 3000

typedef enum {
    RUN_OK,                     /* ran and finished */
    RUN_COMPILE_ERROR,          /* didn't compile: see errors */
    RUN_TIMEOUT,                /* still running after the time limit */
    RUN_TOO_MUCH,               /* printed more than RUN_OUTPUT_LEN */
    RUN_CRASHED,                /* e.g. a bad pointer */
    RUN_NO_COMPILER,            /* gcc not found */
    RUN_FAILED                  /* something else went wrong */
} RunStatus;

typedef struct {
    RunStatus status;
    char output[RUN_OUTPUT_LEN];
    int outputLen;
    char errors[4096];
    int exitCode;
    double seconds;
} RunResult;

/* runner.c */
void runCode(const char *source, bool cpp, int timeLimitMs, RunResult *result);
void cleanUpRun(void);

/* ---------- Code tab: exercises ---------- */

#define MAX_EXERCISES 32
#define MAX_CHECKS 10
#define CODE_TEXT_LEN 2400
#define CODE_POINTS 30

typedef struct Exercise {
    char title[TITLE_LEN];
    int lesson;                       /* from 0 */
    bool cpp;                         /* C++ rather than C */
    char task[TEXT_LEN];
    char hint[TEXT_LEN];
    char starter[CODE_TEXT_LEN];
    char tests[CODE_TEXT_LEN];        /* hidden: a main that prints results */
    char solution[CODE_TEXT_LEN];
    char expect[MAX_CHECKS][LINE_LEN];
    int expectCount;
    struct Exercise *next;
} Exercise;

typedef struct {
    Exercise *head;
    int count;
} ExerciseList;

/* One line of the results: what the tests expected and what they got */
typedef struct {
    char expected[LINE_LEN];
    char got[LINE_LEN];
    bool gotLine;                     /* false if the output stopped early */
    bool passed;
} CheckLine;

/* exercises.c */
int loadExercises(const char *filename, ExerciseList *list, char *error, int errorSize);
void freeExercises(ExerciseList *list);
const Exercise *exerciseAt(const ExerciseList *list, int index);
bool exerciseUnlocked(const Learner *learner, const Exercise *ex);
int buildProgram(const char *userCode, const char *tests, bool cpp, char *out, int size);
int checkOutput(const Exercise *ex, const char *output, CheckLine *checks);
bool finishExercise(Learner *learner, int index);

#endif
