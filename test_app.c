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

    /* Lessons 1-4 are written, 5-12 are coming soon */
    for (int i = 0; i < 4; i++)
    {
        CHECK(course.lessons[i].cardCount >= 2);
        CHECK(course.lessons[i].questionCount >= 5);
        CHECK(lessonReady(&course, i));
        CHECK(course.lessons[i].level == 0);
    }
    for (int i = 4; i < 12; i++)
    {
        CHECK(!lessonReady(&course, i));
        CHECK(course.lessons[i].summary[0] != '\0');
    }
    CHECK(course.lessons[4].level == 1);
    CHECK(course.lessons[11].level == 2);
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

    /* Lesson 5 is coming soon, so it stays locked even after lesson 4 */
    CHECK(!lessonUnlocked(&l, &course, 4));
    CHECK(!(l.badges & (1u << BADGE_EXAM_READY)));

    /* Save and load round trip */
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
