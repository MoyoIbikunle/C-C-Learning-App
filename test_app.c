/*
 * test_app.c - console tests for the course loader and learner progress.
 * Build and run:  make test
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
    CHECK(badgeCount(&l) == 7);   /* every lesson badge, but not Exam Ace, On a Roll or Duck Helper */

    freeCourse(&course);
}

static void testPractice(void)
{
    Course course;
    char error[200];
    loadCourse("content/course.txt", &course, error, sizeof(error));
    QuestionRef pool[MAX_POOL];

    /* A new learner hasn't passed anything, so there's nothing to practise yet */
    Learner l;
    newLearner(&l, "Tomi");
    CHECK(l.bestExam == -1);
    CHECK(buildPool(&l, &course, POOL_PASSED, pool, MAX_POOL) == 0);
    CHECK(buildPool(&l, &course, POOL_MISSED, pool, MAX_POOL) == 0);

    /* Failing lesson 1 still doesn't count */
    finishLesson(&l, &course, 0, 10);
    CHECK(buildPool(&l, &course, POOL_PASSED, pool, MAX_POOL) == 0);

    /* Passing lesson 1 gives its questions, but not lesson 2's (opened, not done) */
    finishLesson(&l, &course, 0, course.lessons[0].questionCount * 10);
    int n = buildPool(&l, &course, POOL_PASSED, pool, MAX_POOL);
    CHECK(n == course.lessons[0].questionCount);
    for (int i = 0; i < n; i++) CHECK(pool[i].lesson == 0);

    /* Shuffling keeps every question exactly once */
    QuestionRef copy[MAX_POOL];
    for (int i = 0; i < n; i++) copy[i] = pool[i];
    srand(42);
    shuffleRefs(copy, n);
    int found = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (copy[j].lesson == pool[i].lesson && copy[j].index == pool[i].index) found++;
    CHECK(found == n);
    for (int i = 0; i < n; i++) CHECK(questionFor(&course, copy[i]) != NULL);
    CHECK(questionFor(&course, (QuestionRef){ 99, 0 }) == NULL);

    /* Mistakes: wrong turns the bit on, right first time turns it off */
    QuestionRef a = { 0, 2 }, b = { 1, 4 };
    recordAnswer(&l, a, false);
    recordAnswer(&l, b, false);
    recordAnswer(&l, b, false);   /* twice still counts once */
    CHECK(isMissed(&l, a) && isMissed(&l, b));
    CHECK(missedCount(&l) == 2);
    n = buildPool(&l, &course, POOL_MISSED, pool, MAX_POOL);
    CHECK(n == 2);   /* mistakes come from any lesson, even one not passed yet */
    CHECK(pool[0].lesson == 0 && pool[0].index == 2);
    CHECK(pool[1].lesson == 1 && pool[1].index == 4);
    recordAnswer(&l, a, true);
    CHECK(!isMissed(&l, a));
    CHECK(missedCount(&l) == 1);
    recordAnswer(&l, (QuestionRef){ 0, MAX_QUESTIONS }, false);   /* out of range: ignored */
    CHECK(missedCount(&l) == 1);

    /* Exams: best score kept; Exam Ace needs a full exam at 90%+ */
    bool best;
    unsigned got = finishExam(&l, 6, 10, &best);
    CHECK(best && l.bestExam == 60 && got == 0);
    got = finishExam(&l, 10, 10, &best);
    CHECK(best && l.bestExam == 100);
    CHECK(got == 0);   /* too short for Exam Ace */
    got = finishExam(&l, 17, 20, &best);
    CHECK(!best && l.bestExam == 100 && got == 0);   /* 85%: not a new best */
    got = finishExam(&l, 18, 20, &best);
    CHECK(got == (1u << BADGE_EXAM_ACE));
    got = finishExam(&l, 19, 20, &best);
    CHECK(got == 0);   /* badge only given once */

    /* Coding task marks: a share of 5 for the checks passed, rounded */
    CHECK(EXAM_FULL_MARKS == 20);
    CHECK(codeMarks(6, 6, 5) == 5);
    CHECK(codeMarks(4, 6, 5) == 3);   /* 3.33 rounds to 3 */
    CHECK(codeMarks(3, 4, 5) == 4);   /* 3.75 rounds to 4 */
    CHECK(codeMarks(1, 2, 5) == 3);   /* 2.5 rounds up */
    CHECK(codeMarks(1, 6, 5) == 1);
    CHECK(codeMarks(0, 6, 5) == 0);
    CHECK(codeMarks(0, 0, 5) == 0);   /* didn't compile: no checks ran */

    /* Mistakes and exam score survive saving and loading */
    CHECK(saveLearners("test_learners.txt", &l, 1));
    Learner back[MAX_LEARNERS];
    CHECK(loadLearners("test_learners.txt", back, MAX_LEARNERS) == 1);
    CHECK(back[0].bestExam == 100);
    CHECK(isMissed(&back[0], b) && !isMissed(&back[0], a));
    CHECK(back[0].badges == l.badges);
    remove("test_learners.txt");

    /* Older save files without MISSED or EXAM lines still load */
    writeFile("test_learners.txt", "LEARNER Old 1\nBEST 1 80 40\nEND\n");
    CHECK(loadLearners("test_learners.txt", back, MAX_LEARNERS) == 1);
    CHECK(back[0].bestExam == -1 && missedCount(&back[0]) == 0);
    remove("test_learners.txt");

    freeCourse(&course);
}

static void testStreak(void)
{
    /* Day numbers, including leap years */
    CHECK(dayNumber(1970, 1, 1) == 0);
    CHECK(dayNumber(1970, 1, 2) == 1);
    CHECK(dayNumber(2000, 3, 1) - dayNumber(2000, 2, 28) == 2);   /* 2000 is a leap year */
    CHECK(dayNumber(1900, 3, 1) - dayNumber(1900, 2, 28) == 1);   /* 1900 isn't */
    CHECK(dayNumber(2024, 3, 1) - dayNumber(2024, 2, 28) == 2);
    CHECK(dayNumber(2026, 3, 1) - dayNumber(2026, 2, 28) == 1);
    CHECK(dayNumber(2027, 1, 1) - dayNumber(2026, 12, 31) == 1);
    CHECK(dayNumber(2027, 1, 1) - dayNumber(2026, 1, 1) == 365);

    Learner l;
    newLearner(&l, "Ada");
    int day = dayNumber(2026, 11, 17);
    bool grew;
    CHECK(currentStreak(&l, day) == 0);

    CHECK(recordActivity(&l, day, &grew) == 0);
    CHECK(grew && l.streak == 1);
    recordActivity(&l, day, &grew);                 /* twice in one day counts once */
    CHECK(!grew && l.streak == 1);

    for (int d = 1; d < 6; d++) recordActivity(&l, day + d, &grew);
    CHECK(l.streak == 6 && currentStreak(&l, day + 5) == 6);
    CHECK(currentStreak(&l, day + 6) == 6);         /* not done yet today: still alive */
    unsigned got = recordActivity(&l, day + 6, &grew);
    CHECK(l.streak == 7 && got == (1u << BADGE_ON_A_ROLL));

    CHECK(currentStreak(&l, day + 8) == 0);         /* missed a day: broken */
    recordActivity(&l, day + 8, &grew);
    CHECK(l.streak == 1 && l.bestStreak == 7);
    CHECK(l.badges & (1u << BADGE_ON_A_ROLL));     /* badge is kept */

    CHECK(saveLearners("test_learners.txt", &l, 1));
    Learner back[MAX_LEARNERS];
    CHECK(loadLearners("test_learners.txt", back, MAX_LEARNERS) == 1);
    CHECK(back[0].streak == 1 && back[0].bestStreak == 7 && back[0].lastDay == day + 8);
    remove("test_learners.txt");
}

static void testDucks(void)
{
    Course course;
    DuckList ducks;
    char error[200];
    loadCourse("content/course.txt", &course, error, sizeof(error));

    int n = loadDucks("content/duck.txt", &ducks, error, sizeof(error));
    if (n == 0) printf("duck error: %s\n", error);
    CHECK(n >= 8 && n <= MAX_DUCKS);
    for (int i = 0; i < n; i++)
    {
        const DuckProblem *d = duckAt(&ducks, i);
        CHECK(d != NULL);
        CHECK(d->bugLine >= 1 && d->bugLine <= d->codeCount);
        CHECK(d->answer >= 1 && d->answer <= d->fixCount);
        CHECK(d->lesson >= 0 && d->lesson < course.lessonCount);
        CHECK(d->codeCount <= 14);
        for (int f = 0; f < d->fixCount; f++)
        {
            if (strlen(d->fixes[f]) > 38) printf("Long fix: %s\n", d->fixes[f]);
            CHECK(strlen(d->fixes[f]) <= 38);
        }
    }
    CHECK(duckAt(&ducks, n) == NULL);

    /* Problems open with their lesson */
    Learner l;
    newLearner(&l, "Duckling");
    const DuckProblem *first = duckAt(&ducks, 0);
    CHECK(first->lesson == 0 && !duckUnlocked(&l, &course, first));   /* lesson 1 not passed yet */
    finishLesson(&l, &course, 0, course.lessons[0].questionCount * 10);
    CHECK(duckUnlocked(&l, &course, first));
    CHECK(!duckUnlocked(&l, &course, duckAt(&ducks, n - 1)));

    /* Points the first time only; Duck Helper once every problem is solved */
    bool firstTime;
    int before = totalPoints(&l);
    CHECK(finishDuck(&l, 0, n, &firstTime) == 0 && firstTime);
    CHECK(totalPoints(&l) == before + DUCK_POINTS);
    finishDuck(&l, 0, n, &firstTime);
    CHECK(!firstTime && totalPoints(&l) == before + DUCK_POINTS);
    unsigned got = 0;
    for (int i = 1; i < n; i++) got = finishDuck(&l, i, n, &firstTime);
    CHECK(got == (1u << BADGE_DUCK_HELPER));
    CHECK(finishDuck(&l, MAX_DUCKS, n, &firstTime) == 0 && !firstTime);   /* out of range: ignored */

    CHECK(saveLearners("test_learners.txt", &l, 1));
    Learner back[MAX_LEARNERS];
    CHECK(loadLearners("test_learners.txt", back, MAX_LEARNERS) == 1);
    CHECK(back[0].ducksHelped == l.ducksHelped && totalPoints(&back[0]) == totalPoints(&l));
    remove("test_learners.txt");

    /* A bad problem (bug line past the code) is rejected without leaking */
    DuckList bad;
    writeFile("test_duck.txt", "DUCK X\nLESSON 1\nSAYS hi\nCODE a\nBUG 2\nFIX a\nFIX b\nANSWER 1\nTHANKS t\nEND\n");
    CHECK(loadDucks("test_duck.txt", &bad, error, sizeof(error)) == 0);
    CHECK(strstr(error, "Line 10") != NULL);
    writeFile("test_duck.txt", "# nothing here\n");
    CHECK(loadDucks("test_duck.txt", &bad, error, sizeof(error)) == 0);
    remove("test_duck.txt");

    freeDucks(&ducks);
    CHECK(ducks.count == 0 && ducks.head == NULL);
    freeCourse(&course);
}

static void testHistory(void)
{
    remove("test_history.txt");
    AttemptList list;
    CHECK(loadHistory("test_history.txt", &list) == 0);

    /* 15 attempts for Bolu and 1 for Ada, added to the list and the file */
    for (int i = 0; i < 16; i++)
    {
        Attempt a = { "", ATTEMPT_LESSON, 2, 0, 0, 5, 20000 + i, NULL };
        snprintf(a.learner, NAME_LEN, "%s", i == 7 ? "Ada" : "Bolu");
        a.kind = (AttemptKind)(i % ATTEMPT_KINDS);
        a.lesson = (a.kind == ATTEMPT_LESSON) ? 2 : -1;
        a.percent = i * 5;
        a.right = i % 6;
        CHECK(addAttempt(&list, &a));
        CHECK(appendAttemptToFile("test_history.txt", &a));
    }
    CHECK(list.count == 16 && list.tail->day == 20015);
    CHECK(attemptCount(&list, "Bolu") == 15 && attemptCount(&list, "Ada") == 1);

    /* The last 10 of Bolu's, oldest first */
    const Attempt *recent[10];
    int n = recentAttempts(&list, "Bolu", recent, 10);
    CHECK(n == 10);
    CHECK(recent[0]->day == 20005 && recent[9]->day == 20015);   /* Ada's day 7 is skipped */
    for (int i = 0; i < n; i++) CHECK(strcmp(recent[i]->learner, "Bolu") == 0);
    CHECK(recentAttempts(&list, "Nobody", recent, 10) == 0);

    /* Reading the file back gives the same list, mixed lessons included */
    AttemptList back;
    CHECK(loadHistory("test_history.txt", &back) == 16);
    CHECK(back.head->lesson == 2 && back.head->next->lesson == -1);
    CHECK(back.tail->percent == 75 && back.tail->kind == ATTEMPT_EXAM);
    freeHistory(&back);
    freeHistory(&list);
    CHECK(list.head == NULL && list.count == 0);
    remove("test_history.txt");
}

static void testGlossary(void)
{
    Glossary g;
    char error[200];
    int n = loadGlossary("content/glossary.txt", &g, error, sizeof(error));
    if (n == 0) printf("glossary error: %s\n", error);
    CHECK(n >= 30);

    /* Every lesson has at least one term */
    int perLesson[MAX_LESSONS] = { 0 };
    for (int i = 0; i < n; i++)
    {
        const Term *t = termAt(&g, i);
        CHECK(t->lesson >= 0 && t->lesson < MAX_LESSONS);
        perLesson[t->lesson]++;
    }
    for (int l = 0; l < MAX_LESSONS; l++) CHECK(perLesson[l] > 0);
    CHECK(termAt(&g, n) == NULL);

    /* Search: matches on the word come before matches in the meaning */
    const Term *found[60];
    int k = searchGlossary(&g, "POINTER", found, 60);
    CHECK(k >= 3);
    CHECK(strcmp(found[0]->word, "pointer") == 0 || strstr(found[0]->word, "pointer") != NULL);
    CHECK(searchGlossary(&g, "", found, 60) == n);
    CHECK(searchGlossary(&g, "zzqq", found, 60) == 0);

    /* Terms in text: whole words only, plural s allowed */
    k = termsInText(&g, "Arrays and strings end with a null terminator.", found, 60);
    bool array = false, string = false, nullT = false;
    for (int i = 0; i < k; i++)
    {
        array |= strcmp(found[i]->word, "array") == 0;
        string |= strcmp(found[i]->word, "string") == 0;
        nullT |= strcmp(found[i]->word, "null terminator") == 0;
    }
    CHECK(array && string && nullT);
    k = termsInText(&g, "The headline mentions a mainframe", found, 60);
    for (int i = 0; i < k; i++)
    {
        CHECK(strcmp(found[i]->word, "head") != 0 && strcmp(found[i]->word, "main") != 0);
    }

    /* Errors: a term without a meaning */
    Glossary bad;
    writeFile("test_glossary.txt", "TERM x\nLESSON 1\nEND\n");
    CHECK(loadGlossary("test_glossary.txt", &bad, error, sizeof(error)) == 0);
    CHECK(strstr(error, "Line 3") != NULL);
    remove("test_glossary.txt");

    freeGlossary(&g);
    CHECK(g.count == 0 && g.head == NULL);
}

static void testTopics(void)
{
    Course course;
    char error[200];
    loadCourse("content/course.txt", &course, error, sizeof(error));
    Learner l;
    newLearner(&l, "Ada");
    CHECK(topicPercent(&l, 0) == -1);

    /* Lesson 1: 4 of 5 right; lesson 2: 1 of 4; lesson 3: 2 tries only (too few to judge) */
    for (int i = 0; i < 5; i++) recordAnswer(&l, (QuestionRef){ 0, i }, i != 0);
    for (int i = 0; i < 4; i++) recordAnswer(&l, (QuestionRef){ 1, i }, i == 0);
    recordAnswer(&l, (QuestionRef){ 2, 0 }, false);
    recordAnswer(&l, (QuestionRef){ 2, 1 }, false);
    CHECK(topicPercent(&l, 0) == 80 && topicPercent(&l, 1) == 25);

    int weak[3];
    int n = weakestTopics(&l, &course, weak, 3);
    CHECK(n == 2 && weak[0] == 1 && weak[1] == 0);

    /* 100% lessons aren't weak */
    for (int i = 0; i < 4; i++) recordAnswer(&l, (QuestionRef){ 3, i }, true);
    CHECK(weakestTopics(&l, &course, weak, 3) == 2);

    CHECK(saveLearners("test_learners.txt", &l, 1));
    Learner back[MAX_LEARNERS];
    CHECK(loadLearners("test_learners.txt", back, MAX_LEARNERS) == 1);
    CHECK(topicPercent(&back[0], 1) == 25 && back[0].topicTried[3] == 4);
    remove("test_learners.txt");
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

static void testTextBuffer(void)
{
    static TextBuffer tb;
    char text[4096];

    tbSetText(&tb, "int main()\n{\n}");
    CHECK(tb.count == 3 && tb.row == 0 && tb.col == 0);
    tbGetText(&tb, text, sizeof(text));
    CHECK(strcmp(text, "int main()\n{\n}\n") == 0);

    /* Enter after { indents the new line by 4 */
    tbSetCursor(&tb, 1, 1);
    tbNewline(&tb);
    CHECK(tb.row == 2 && tb.col == 4 && strcmp(tb.lines[2], "    ") == 0);
    tbInsertText(&tb, "return 0;");
    CHECK(strcmp(tb.lines[2], "    return 0;") == 0);

    /* Enter in an indented line keeps the indent */
    tbNewline(&tb);
    CHECK(tb.col == 4);
    /* Typing } on an empty indented line lines it up with the { */
    tbInsertChar(&tb, '}');
    CHECK(strcmp(tb.lines[3], "}") == 0);

    /* Backspace at the start of a line joins it to the line above */
    tbSetCursor(&tb, 3, 0);
    tbBackspace(&tb);
    CHECK(tb.row == 2 && strcmp(tb.lines[2], "    return 0;}") == 0);
    tbBackspace(&tb);
    CHECK(strcmp(tb.lines[2], "    return 0}") == 0);

    /* Delete at the end of a line pulls the next line up */
    tbSetText(&tb, "ab\ncd");
    tbSetCursor(&tb, 0, 2);
    tbDelete(&tb);
    CHECK(tb.count == 1 && strcmp(tb.lines[0], "abcd") == 0);

    /* Arrows wrap between lines; Home goes to the code, then column 0 */
    tbSetText(&tb, "x\n    y");
    tbSetCursor(&tb, 1, 0);
    tbMove(&tb, 0, -1);
    CHECK(tb.row == 0 && tb.col == 1);
    tbMove(&tb, 0, 1);
    CHECK(tb.row == 1 && tb.col == 0);
    tbEnd(&tb);
    CHECK(tb.col == 5);
    tbHome(&tb);
    CHECK(tb.col == 4);
    tbHome(&tb);
    CHECK(tb.col == 0);
    tbMove(&tb, -5, 99);   /* clamped to the text */
    CHECK(tb.row == 0 && tb.col == 1);

    /* Tab goes to the next multiple of 4; pasting turns tabs into spaces and skips \r */
    tbClear(&tb);
    tbInsertChar(&tb, 'a');
    tbTab(&tb);
    CHECK(tb.col == 4);
    tbClear(&tb);
    tbInsertText(&tb, "a\r\n\tb");
    CHECK(tb.count == 2 && strcmp(tb.lines[1], "    b") == 0);

    /* It never goes past its size */
    tbClear(&tb);
    for (int i = 0; i < TB_LINES + 10; i++) tbNewline(&tb);
    CHECK(tb.count == TB_LINES);
    tbClear(&tb);
    for (int i = 0; i < TB_COLS + 10; i++) tbInsertChar(&tb, 'x');
    CHECK((int)strlen(tb.lines[0]) == TB_COLS - 1);
}

static void testExercises(void)
{
    ExerciseList list;
    Course course;
    char error[200];
    loadCourse("content/course.txt", &course, error, sizeof(error));
    int n = loadExercises("content/exercises.txt", &list, error, sizeof(error));
    if (n == 0) printf("exercise error: %s\n", error);
    CHECK(n >= 12 && n <= MAX_EXERCISES);

    /* Every exercise is for a real lesson; at least one for each lesson */
    int perLesson[MAX_LESSONS] = { 0 };
    for (int i = 0; i < n; i++)
    {
        const Exercise *ex = exerciseAt(&list, i);
        CHECK(ex->lesson >= 0 && ex->lesson < course.lessonCount);
        CHECK(ex->starter[0] != '\0' && ex->hint[0] != '\0');
        perLesson[ex->lesson]++;
    }
    for (int l = 0; l < MAX_LESSONS; l++) CHECK(perLesson[l] > 0);

    /* Checking output line by line */
    const Exercise *first = exerciseAt(&list, 0);
    CheckLine checks[MAX_CHECKS];
    CHECK(checkOutput(first, "Hello, Ada!\r\nHello, Sam!  \n", checks) == 2);   /* \r and trailing spaces are fine */
    CHECK(checkOutput(first, "Hello, Ada!\nHello Sam!\n", checks) == 1);
    CHECK(checks[0].passed && !checks[1].passed && strcmp(checks[1].got, "Hello Sam!") == 0);
    CHECK(checkOutput(first, "Hello, Ada!", checks) == 1);
    CHECK(!checks[1].gotLine);   /* output stopped early */
    CHECK(checkOutput(first, "", checks) == 0);

    /* The program is the usual includes, then the learner's code, then the tests */
    char program[8000];
    buildProgram("int x;", "int main(void) { return 0; }", false, program, sizeof(program));
    CHECK(strstr(program, "#include <stdio.h>") != NULL);
    CHECK(strstr(program, "#line 1 \"your code\"\nint x;") != NULL);
    CHECK(strstr(program, "int x;") < strstr(program, "int main"));
    buildProgram("int x;", NULL, true, program, sizeof(program));
    CHECK(strstr(program, "#include <iostream>") != NULL && strstr(program, "tests") == NULL);

    /* Exercises open with their lesson; points the first time only */
    Learner l;
    newLearner(&l, "Coder");
    CHECK(!exerciseUnlocked(&l, first));
    finishLesson(&l, &course, 0, course.lessons[0].questionCount * 10);
    CHECK(exerciseUnlocked(&l, first));
    int before = totalPoints(&l);
    CHECK(finishExercise(&l, 0) && totalPoints(&l) == before + CODE_POINTS);
    CHECK(!finishExercise(&l, 0) && totalPoints(&l) == before + CODE_POINTS);
    CHECK(!finishExercise(&l, MAX_EXERCISES));
    CHECK(saveLearners("test_learners.txt", &l, 1));
    Learner back[MAX_LEARNERS];
    CHECK(loadLearners("test_learners.txt", back, MAX_LEARNERS) == 1 && back[0].codeSolved == 1u);
    remove("test_learners.txt");

    /* A broken exercise file is rejected */
    ExerciseList bad;
    writeFile("test_ex.txt", "EXERCISE X\nLESSON 1\nLANG c\nTASK t\nTEST main\nEND\n");
    CHECK(loadExercises("test_ex.txt", &bad, error, sizeof(error)) == 0);
    remove("test_ex.txt");

    freeExercises(&list);
    freeCourse(&course);
}

/* Runs real programs with gcc: slower, so only with "test_app code" */
static void testRunningCode(void)
{
    static RunResult r;
    char program[8000];

    buildProgram("int main(void) { printf(\"hi %d\\n\", 6 * 7); return 0; }", NULL, false, program, sizeof(program));
    runCode(program, false, RUN_TIME_LIMIT_MS, &r);
    CHECK(r.status == RUN_OK && strcmp(r.output, "hi 42\n") == 0);

    buildProgram("int main(void) {\n    int x = 5\n    return 0;\n}", NULL, false, program, sizeof(program));
    runCode(program, false, RUN_TIME_LIMIT_MS, &r);
    CHECK(r.status == RUN_COMPILE_ERROR);
    CHECK(strstr(r.errors, "your code:2") != NULL || strstr(r.errors, "your code:3") != NULL);   /* the learner's own line numbers */

    buildProgram("int main(void) { while (1) { } }", NULL, false, program, sizeof(program));
    runCode(program, false, 1000, &r);
    CHECK(r.status == RUN_TIMEOUT);

    buildProgram("int main(void) { while (1) printf(\"spam\\n\"); }", NULL, false, program, sizeof(program));
    runCode(program, false, RUN_TIME_LIMIT_MS, &r);
    CHECK(r.status == RUN_TOO_MUCH && r.outputLen > RUN_OUTPUT_LEN / 2);   /* stopped; Windows \r characters are taken out, so it can be a bit under the limit */

    buildProgram("int main(void) { int *p = NULL; printf(\"before\\n\"); *p = 5; return 0; }", NULL, false, program, sizeof(program));
    runCode(program, false, RUN_TIME_LIMIT_MS, &r);
    if (r.status != RUN_CRASHED || strstr(r.output, "before") == NULL)
    {
        printf("Crash test: status %d (4 = crashed, 2 = timed out), exit code %d, output [%s], took %.1fs\n",
               r.status, r.exitCode, r.output, r.seconds);
    }
    CHECK(r.status == RUN_CRASHED && strstr(r.output, "before") != NULL);   /* output before the crash survives */

    buildProgram("int main() { cout << \"C++ \" << 1 + 1 << endl; }", NULL, true, program, sizeof(program));
    runCode(program, true, RUN_TIME_LIMIT_MS, &r);
    CHECK(r.status == RUN_OK && strcmp(r.output, "C++ 2\n") == 0);

    /* scanf gets no input rather than waiting forever */
    buildProgram("int main(void) { int x = 9; int got = scanf(\"%d\", &x); printf(\"%d\\n\", got); }", NULL, false, program, sizeof(program));
    runCode(program, false, RUN_TIME_LIMIT_MS, &r);
    CHECK(r.status == RUN_OK && strcmp(r.output, "-1\n") == 0);

    /* Every exercise: the model answer passes all its checks,
       and the starter code compiles but doesn't pass yet */
    ExerciseList list;
    char error[200];
    int n = loadExercises("content/exercises.txt", &list, error, sizeof(error));
    for (int i = 0; i < n; i++)
    {
        const Exercise *ex = exerciseAt(&list, i);
        CheckLine checks[MAX_CHECKS];

        buildProgram(ex->solution, ex->tests, ex->cpp, program, sizeof(program));
        runCode(program, ex->cpp, RUN_TIME_LIMIT_MS, &r);
        int ok = (r.status == RUN_OK) ? checkOutput(ex, r.output, checks) : -1;
        if (ok != ex->expectCount)
        {
            printf("Solution for '%s' failed (status %d): %s%s\n", ex->title, r.status, r.errors, r.output);
        }
        CHECK(ok == ex->expectCount);

        buildProgram(ex->starter, ex->tests, ex->cpp, program, sizeof(program));
        runCode(program, ex->cpp, RUN_TIME_LIMIT_MS, &r);
        if (r.status == RUN_COMPILE_ERROR) printf("Starter for '%s' doesn't compile:\n%s\n", ex->title, r.errors);
        CHECK(r.status != RUN_COMPILE_ERROR);
        CHECK(r.status != RUN_OK || checkOutput(ex, r.output, checks) < ex->expectCount);
    }
    printf("(compiled and ran %d exercises twice each)\n", n);
    freeExercises(&list);
    cleanUpRun();
}

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "code") == 0)
    {
        testRunningCode();
        printf("%d passed, %d failed\n", passed, failed);
        return failed == 0 ? 0 : 1;
    }
    testRealCourse();
    testBadCourse();
    testScoring();
    testProgress();
    testNames();
    testPractice();
    testStreak();
    testDucks();
    testHistory();
    testGlossary();
    testTopics();
    testTextBuffer();
    testExercises();
    printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
