/*
 * learner.c - learners' progress: points, unlocking lessons, badges, saving.
 *
 * Points: 10 for a right answer first time, 5 on the second try, 0 after.
 * A lesson is passed at 70% or more, and passing it unlocks the next one.
 * Each lesson keeps the learner's best attempt, so retrying can only help,
 * and repeating an easy lesson can't farm points.
 *
 * learners.txt, one block per learner:
 *   LEARNER Bolu 5        name and badges (one bit per badge)
 *   BEST 1 100 50         lesson number, best %, points from that attempt
 *   END
 */
#include <stdio.h>
#include <string.h>
#include "app.h"

#define BADGE_BONUS 25   /* extra points for each badge */

const char *BADGE_NAMES[BADGE_COUNT] = {
    "First Steps", "Perfect Score", "Loop Master", "Beginner Graduate",
    "Problem Solver", "Pointer Pro", "Exam Ready"
};

const char *BADGE_DESCRIPTIONS[BADGE_COUNT] = {
    "Pass lesson 1",
    "Get 100% on any lesson",
    "Pass lesson 3: Decisions and loops",
    "Pass every Beginner lesson",
    "Pass every Intermediate lesson",
    "Get 100% on lesson 9: Pointers",
    "Pass every lesson in the course"
};

void newLearner(Learner *learner, const char *name)
{
    memset(learner, 0, sizeof(*learner));
    strncpy(learner->name, name, NAME_LEN - 1);
    for (int i = 0; i < MAX_LESSONS; i++)
    {
        learner->bestPercent[i] = -1;
    }
}

/* Names are one word (letters, digits, - or ') so the save file can read them.
   Returns 1 if the name is fine. */
int validLearnerName(const char *name)
{
    size_t len = strlen(name);
    if (len == 0 || len >= NAME_LEN)
    {
        return 0;
    }
    for (size_t i = 0; i < len; i++)
    {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '\''))
        {
            return 0;
        }
    }
    return 1;
}

int findLearner(const Learner *list, int count, const char *name)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(list[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

/* Returns how many learners were loaded (0 if there's no file yet) */
int loadLearners(const char *filename, Learner *list, int max)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        return 0;
    }

    int count = 0;
    Learner *current = NULL;
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char name[NAME_LEN];
        unsigned badges;
        int number, percent, points;

        if (sscanf(line, "LEARNER %31s %u", name, &badges) == 2 && count < max && validLearnerName(name))
        {
            current = &list[count++];
            newLearner(current, name);
            current->badges = badges & ((1u << BADGE_COUNT) - 1);
        }
        else if (current != NULL && sscanf(line, "BEST %d %d %d", &number, &percent, &points) == 3 &&
                 number >= 1 && number <= MAX_LESSONS)
        {
            current->bestPercent[number - 1] = percent;
            current->bestPoints[number - 1] = points;
        }
        else if (strncmp(line, "END", 3) == 0)
        {
            current = NULL;
        }
    }
    fclose(fp);
    return count;
}

int saveLearners(const char *filename, const Learner *list, int count)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        return 0;
    }
    for (int i = 0; i < count; i++)
    {
        fprintf(fp, "LEARNER %s %u\n", list[i].name, list[i].badges);
        for (int l = 0; l < MAX_LESSONS; l++)
        {
            if (list[i].bestPercent[l] >= 0)
            {
                fprintf(fp, "BEST %d %d %d\n", l + 1, list[i].bestPercent[l], list[i].bestPoints[l]);
            }
        }
        fprintf(fp, "END\n");
    }
    fclose(fp);
    return 1;
}

/* attempt = 1 for the first try at a question, 2 for the second, ... */
int pointsForAttempt(int attempt)
{
    if (attempt == 1)
    {
        return POINTS_FIRST_TRY;
    }
    if (attempt == 2)
    {
        return POINTS_SECOND_TRY;
    }
    return 0;
}

/* Score out of 100, where getting every question right first time = 100 */
int scorePercent(int pointsEarned, int questionCount)
{
    if (questionCount <= 0)
    {
        return 0;
    }
    return pointsEarned * 100 / (questionCount * POINTS_FIRST_TRY);
}

int totalPoints(const Learner *learner)
{
    int total = 0;
    for (int i = 0; i < MAX_LESSONS; i++)
    {
        total += learner->bestPoints[i];
    }
    return total + badgeCount(learner) * BADGE_BONUS;
}

int badgeCount(const Learner *learner)
{
    int n = 0;
    for (int b = 0; b < BADGE_COUNT; b++)
    {
        n += (learner->badges >> b) & 1u;
    }
    return n;
}

bool lessonPassed(const Learner *learner, int index)
{
    return learner->bestPercent[index] >= PASS_PERCENT;
}

/* A lesson can be taken once it has questions (the rest are "coming soon") */
bool lessonReady(const Course *course, int index)
{
    return index >= 0 && index < course->lessonCount && course->lessons[index].questionCount > 0;
}

/* The first lesson is always open; each later one opens when the one before is passed */
bool lessonUnlocked(const Learner *learner, const Course *course, int index)
{
    if (!lessonReady(course, index))
    {
        return false;
    }
    return index == 0 || lessonPassed(learner, index - 1);
}

/* Passed every lesson from first to last (all must be ready) */
static bool passedRange(const Learner *learner, const Course *course, int first, int last)
{
    for (int i = first; i <= last; i++)
    {
        if (!lessonReady(course, i) || !lessonPassed(learner, i))
        {
            return false;
        }
    }
    return true;
}

/* Records a finished lesson: keeps the best attempt and awards badges.
   Returns the badges earned for the first time (one bit each). */
unsigned finishLesson(Learner *learner, const Course *course, int index, int pointsEarned)
{
    const Lesson *lesson = &course->lessons[index];
    int percent = scorePercent(pointsEarned, lesson->questionCount);

    if (percent > learner->bestPercent[index])
    {
        learner->bestPercent[index] = percent;
        learner->bestPoints[index] = pointsEarned;
    }

    unsigned before = learner->badges;
    if (lessonPassed(learner, 0))                         learner->badges |= 1u << BADGE_FIRST_STEPS;
    if (percent == 100)                                   learner->badges |= 1u << BADGE_PERFECT;
    if (course->lessonCount > 2 && lessonPassed(learner, 2)) learner->badges |= 1u << BADGE_LOOP_MASTER;
    if (passedRange(learner, course, 0, 3))               learner->badges |= 1u << BADGE_BEGINNER;
    if (passedRange(learner, course, 4, 7))               learner->badges |= 1u << BADGE_PROBLEM_SOLVER;
    if (course->lessonCount > 8 && learner->bestPercent[8] == 100) learner->badges |= 1u << BADGE_POINTER_PRO;
    if (course->lessonCount == MAX_LESSONS && passedRange(learner, course, 0, MAX_LESSONS - 1))
    {
        learner->badges |= 1u << BADGE_EXAM_READY;
    }
    return learner->badges & ~before;
}
