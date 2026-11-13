/*
 * test_app.c - console tests for the course loader and learner progress.
 * Build and run:  make test
 */
#include <stdio.h>
#include <string.h>
#include "app.h"

static int passed = 0, failed = 0;

#define CHECK(cond) do { \
    if (cond) { passed++; } \
    else { failed++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
} while (0)

static void writeFile(const char *name, const char *text)
{
    FILE *fp = fopen(name, "w");
    fputs(text, fp);
    fclose(fp);
}

static void testRealCourse(void)
{
    Course course;
    char error[200];
    int n = loadCourse("content/course.txt", &course, error, sizeof(error));
    if (n == 0) printf("course error: %s\n", error);
    CHECK(n == 12);

    /* All 12 lessons are written: 4 per level, numbered in order */
    for (int i = 0; i < 12; i++)
    {
        CHECK(course.lessons[i].number == i + 1);
        CHECK(course.lessons[i].level == i / 4);
        CHECK(course.lessons[i].cardCount >= 2);
        CHECK(course.lessons[i].questionCount >= 5);
        CHECK(course.lessons[i].summary[0] != '\0');
        CHECK(lessonReady(&course, i));
    }
    CHECK(strcmp(course.lessons[0].title, "First programs") == 0);

    /* Every question's answer points at a real option or line */
    for (int i = 0; i < course.lessonCount; i++)
    {
        const Lesson *lesson = &course.lessons[i];
        for (int k = 0; k < lesson->questionCount; k++)
        {
            const Question *q = questionAt(lesson, k);
            CHECK(q != NULL);
            int limit = (q->type == Q_BUG) ? q->codeCount : q->optionCount;
            CHECK(q->answer >= 1 && q->answer <= limit);
            CHECK(q->explain[0] != '\0');
            /* Answer buttons are one line: keep options short enough to fit */
            for (int o = 0; o < q->optionCount; o++)
            {
                int limit = q->codeCount > 0 ? 38 : 55;
                if ((int)strlen(q->options[o]) > limit) printf("Long option in lesson %d: %s\n", i + 1, q->options[o]);
                CHECK((int)strlen(q->options[o]) <= limit);
            }
            /* Code must fit the screen: 16 lines max on cards and questions */
            CHECK(q->codeCount <= 16);
        }
        for (int k = 0; k < lesson->cardCount; k++)
        {
            CHECK(cardAt(lesson, k)->codeCount <= 16);
        }
        CHECK(questionAt(lesson, lesson->questionCount) == NULL);
        CHECK(cardAt(lesson, lesson->cardCount) == NULL);
    }

    /* A known question: lesson 2's first question, 7 / 2 = 3 (option 2) */
    const Question *q = questionAt(&course.lessons[1], 0);
    CHECK(q->type == Q_OUTPUT);
    CHECK(isAnswer(q, 2));
    CHECK(!isAnswer(q, 1));

    /* Blank "CODE" lines are kept as empty code lines */
    const Card *first = cardAt(&course.lessons[0], 0);
    CHECK(first->codeCount == 6);
    CHECK(first->code[1][0] == '\0');

    freeCourse(&course);
    CHECK(course.lessonCount == 0);
}

static void testBadCourse(void)
{
    Course course;
    char error[200];

    CHECK(loadCourse("no_such_file.txt", &course, error, sizeof(error)) == 0);
    CHECK(strstr(error, "Can't open") != NULL);

    /* Answer 3 but only 2 options: rejected with a line number, nothing leaked */
    writeFile("test_course.txt",
              "LESSON 1 1 Test\nCARD A\nTEXT hi\nEND\n"
              "QUESTION choice\nTEXT Q?\nOPTION a\nOPTION b\nANSWER 3\nEXPLAIN x\nEND\n");
    CHECK(loadCourse("test_course.txt", &course, error, sizeof(error)) == 0);
    CHECK(strstr(error, "Line 11") != NULL);

    writeFile("test_course.txt", "LESSON 1 1 Test\nQUESTION essay\nEND\n");
    CHECK(loadCourse("test_course.txt", &course, error, sizeof(error)) == 0);
    CHECK(strstr(error, "unknown question type") != NULL);

    /* Too many code lines is an error, not silently cut off */
    {
        char text[2000] = "LESSON 1 1 Test\nCARD A\n";
        for (int k = 0; k < MAX_CODE + 1; k++) strcat(text, "CODE x;\n");
        strcat(text, "END\n");
        writeFile("test_course.txt", text);
        CHECK(loadCourse("test_course.txt", &course, error, sizeof(error)) == 0);
        CHECK(strstr(error, "too many CODE") != NULL);
    }

    writeFile("test_course.txt", "CARD Too early\n");
    CHECK(loadCourse("test_course.txt", &course, error, sizeof(error)) == 0);

    /* Bug question: answer is a line number */
    writeFile("test_course.txt",
              "LESSON 1 1 Test\nQUESTION bug\nTEXT Click\nCODE a\nCODE b\nANSWER 2\nEXPLAIN x\nEND\n");
    CHECK(loadCourse("test_course.txt", &course, error, sizeof(error)) == 1);
    CHECK(course.lessons[0].questionCount == 1);
    CHECK(isAnswer(questionAt(&course.lessons[0], 0), 2));
    freeCourse(&course);
    remove("test_course.txt");
}

static void testScoring(void)
{
    CHECK(pointsForAttempt(1) == 10);
    CHECK(pointsForAttempt(2) == 5);
    CHECK(pointsForAttempt(3) == 0);
    CHECK(scorePercent(50, 5) == 100);
    CHECK(scorePercent(35, 5) == 70);
    CHECK(scorePercent(30, 5) == 60);
    CHECK(scorePercent(0, 0) == 0);
}

static void testProgress(void)
{
    Course course;
    char error[200];
    loadCourse("content/course.txt", &course, error, sizeof(error));
    int q1 = course.lessons[0].questionCount;

    Learner l;
    newLearner(&l, "Bolu");
    CHECK(lessonUnlocked(&l, &course, 0));
    CHECK(!lessonUnlocked(&l, &course, 1));
    CHECK(totalPoints(&l) == 0);

    /* A fail doesn't unlock lesson 2 or give badges */
    unsigned got = finishLesson(&l, &course, 0, 10);
    CHECK(got == 0);
    CHECK(!lessonUnlocked(&l, &course, 1));

    /* A perfect score: First Steps + Perfect, lesson 2 opens */
    got = finishLesson(&l, &course, 0, q1 * 10);
    CHECK(got == ((1u << BADGE_FIRST_STEPS) | (1u << BADGE_PERFECT)));
    CHECK(lessonUnlocked(&l, &course, 1));
    CHECK(l.bestPercent[0] == 100);
    CHECK(totalPoints(&l) == q1 * 10 + 2 * 25);

    /* A worse retry keeps the best, and badges aren't given twice */
    got = finishLesson(&l, &course, 0, 10);
    CHECK(got == 0);
    CHECK(l.bestPercent[0] == 100);

    /* Pass lessons 2-4 at 70%+: Loop Master then Beginner Graduate */
    for (int i = 1; i < 4; i++)
    {
        int n = course.lessons[i].questionCount;
        got = finishLesson(&l, &course, i, (n * 10 * 8) / 10);   /* about 80% */
        CHECK(lessonPassed(&l, i));
        if (i == 2) CHECK(got & (1u << BADGE_LOOP_MASTER));
        if (i == 3) CHECK(got & (1u << BADGE_BEGINNER));
    }
    CHECK(badgeCount(&l) == 4);

    /* Lesson 5 opens after lesson 4; Exam Ready isn't earned yet */
    CHECK(lessonUnlocked(&l, &course, 4));
    CHECK(!lessonUnlocked(&l, &course, 5));
    CHECK(!(l.badges & (1u << BADGE_EXAM_READY)));

    /* Save and load round trip (before the rest of the course) */
    Learner list[2];
    list[0] = l;
    newLearner(&list[1], "Ada");
    CHECK(saveLearners("test_learners.txt", list, 2));
    Learner back[MAX_LEARNERS];
    CHECK(loadLearners("test_learners.txt", back, MAX_LEARNERS) == 2);
    CHECK(strcmp(back[0].name, "Bolu") == 0);
    CHECK(back[0].badges == l.badges);
    CHECK(back[0].bestPercent[0] == 100);
    CHECK(back[0].bestPercent[4] == -1);
    CHECK(totalPoints(&back[0]) == totalPoints(&l));
    CHECK(findLearner(back, 2, "Ada") == 1);
    CHECK(findLearner(back, 2, "Zed") == -1);
    remove("test_learners.txt");
    CHECK(loadLearners("test_learners.txt", back, MAX_LEARNERS) == 0);

    /* Intermediate lessons: Problem Solver after lesson 8 */
    for (int i = 4; i < 8; i++)
    {
        got = finishLesson(&l, &course, i, course.lessons[i].questionCount * 10 * 7 / 10 + 5);
        CHECK(lessonPassed(&l, i));
        CHECK(((got >> BADGE_PROBLEM_SOLVER) & 1u) == (i == 7));
    }

    /* Pointers at 90%: passed but no Pointer Pro; 100% earns it */
    int n9 = course.lessons[8].questionCount;
    got = finishLesson(&l, &course, 8, n9 * 10 - 5);
    CHECK(lessonPassed(&l, 8));
    CHECK(!(got & (1u << BADGE_POINTER_PRO)));
    got = finishLesson(&l, &course, 8, n9 * 10);
    CHECK(got & (1u << BADGE_POINTER_PRO));

    /* Last three lessons: Exam Ready only once lesson 12 is passed */
    for (int i = 9; i < 12; i++)
    {
        got = finishLesson(&l, &course, i, course.lessons[i].questionCount * 10);
        CHECK(((got >> BADGE_EXAM_READY) & 1u) == (i == 11));
    }
    CHECK(badgeCount(&l) == BADGE_COUNT);

    freeCourse(&course);
}

static void testNames(void)
{
    CHECK(validLearnerName("Bolu"));
    CHECK(validLearnerName("Mary-Jane"));
    CHECK(validLearnerName("O'Neill2"));
    CHECK(!validLearnerName(""));
    CHECK(!validLearnerName("Two words"));
    CHECK(!validLearnerName("abcdefghijabcdefghijabcdefghijabc"));   /* 33 chars */
}

int main(void)
{
    testRealCourse();
    testBadCourse();
    testScoring();
    testProgress();
    testNames();
    printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
