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
    BADGE_COUNT
} Badge;

extern const char *BADGE_NAMES[BADGE_COUNT];
extern const char *BADGE_DESCRIPTIONS[BADGE_COUNT];

typedef struct {
    char name[NAME_LEN];
    int bestPercent[MAX_LESSONS];   /* best score per lesson, -1 = not tried */
    int bestPoints[MAX_LESSONS];    /* points from that best attempt */
    unsigned badges;                /* one bit per Badge */
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

#endif
