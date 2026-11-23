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
 *   MISSED 2 5            lesson number, questions to review (one bit each)
 *   EXAM 85               best exam score
 *   STREAK 3 5 20774      streak, best streak, day of last activity
 *   DUCKS 7               duck problems solved (one bit each)
 *   CODED 5               coding exercises solved (one bit each)
 *   TOPIC 3 8 10          lesson number, answers right first time, out of how many
 *   END
 */
#include <stdio.h>
#include <string.h>
#include "app.h"

#define BADGE_BONUS 25   /* extra points for each badge */

const char *BADGE_NAMES[BADGE_COUNT] = {
    "First Steps", "Perfect Score", "Loop Master", "Beginner Graduate",
    "Problem Solver", "Pointer Pro", "Exam Ready", "Exam Ace",
    "On a Roll", "Duck Helper"
};

const char *BADGE_DESCRIPTIONS[BADGE_COUNT] = {
    "Pass lesson 1",
    "100% on any lesson",
    "Pass lesson 3: Loops",
    "Pass all Beginner lessons",
    "Pass all Intermediate",
    "100% on lesson 9: Pointers",
    "Pass every lesson",
    "90%+ in a full exam",
    "A 7-day streak",
    "Help with every problem"
};

void newLearner(Learner *learner, const char *name)
{
    memset(learner, 0, sizeof(*learner));
    strncpy(learner->name, name, NAME_LEN - 1);
    for (int i = 0; i < MAX_LESSONS; i++)
    {
        learner->bestPercent[i] = -1;
    }
    learner->bestExam = -1;
    learner->lastDay = -1;
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
        unsigned bits;

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
        else if (current != NULL && sscanf(line, "MISSED %d %u", &number, &bits) == 2 &&
                 number >= 1 && number <= MAX_LESSONS)
        {
            current->missed[number - 1] = bits;
        }
        else if (current != NULL && sscanf(line, "EXAM %d", &percent) == 1)
        {
            current->bestExam = percent;
        }
        else if (current != NULL && sscanf(line, "STREAK %d %d %d", &number, &percent, &points) == 3)
        {
            current->streak = number;
            current->bestStreak = percent;
            current->lastDay = points;
        }
        else if (current != NULL && sscanf(line, "DUCKS %u", &bits) == 1)
        {
            current->ducksHelped = bits;
        }
        else if (current != NULL && sscanf(line, "CODED %u", &bits) == 1)
        {
            current->codeSolved = bits;
        }
        else if (current != NULL && sscanf(line, "TOPIC %d %d %d", &number, &percent, &points) == 3 &&
                 number >= 1 && number <= MAX_LESSONS)
        {
            current->topicRight[number - 1] = percent;
            current->topicTried[number - 1] = points;
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
            if (list[i].missed[l] != 0)
            {
                fprintf(fp, "MISSED %d %u\n", l + 1, list[i].missed[l]);
            }
        }
        if (list[i].bestExam >= 0)
        {
            fprintf(fp, "EXAM %d\n", list[i].bestExam);
        }
        if (list[i].lastDay >= 0)
        {
            fprintf(fp, "STREAK %d %d %d\n", list[i].streak, list[i].bestStreak, list[i].lastDay);
        }
        if (list[i].ducksHelped != 0)
        {
            fprintf(fp, "DUCKS %u\n", list[i].ducksHelped);
        }
        if (list[i].codeSolved != 0)
        {
            fprintf(fp, "CODED %u\n", list[i].codeSolved);
        }
        for (int l = 0; l < MAX_LESSONS; l++)
        {
            if (list[i].topicTried[l] > 0)
            {
                fprintf(fp, "TOPIC %d %d %d\n", l + 1, list[i].topicRight[l], list[i].topicTried[l]);
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
    for (int d = 0; d < MAX_DUCKS; d++)
    {
        total += ((learner->ducksHelped >> d) & 1u) * DUCK_POINTS;
    }
    for (int e = 0; e < MAX_EXERCISES; e++)
    {
        total += ((learner->codeSolved >> e) & 1u) * CODE_POINTS;
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

/* Days since 1 January 1970 for a date. Counting days in whole 400-year
   cycles takes care of leap years (the same rule as the Lab 1 leap year question). */
int dayNumber(int year, int month, int day)
{
    year -= (month <= 2);                                  /* treat Jan and Feb as the end of last year */
    int era = (year >= 0 ? year : year - 399) / 400;
    int yearOfEra = year - era * 400;                      /* 0 to 399 */
    int dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;   /* from 1 March */
    int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

/* Counts today towards the streak. grew is set if the streak went up
   (the first activity of a new day). Returns badges earned for the first time. */
unsigned recordActivity(Learner *learner, int today, bool *grew)
{
    *grew = false;
    if (learner->lastDay == today)
    {
        return 0;   /* already counted today */
    }
    if (learner->lastDay == today - 1)
    {
        learner->streak++;      /* came back the next day */
    }
    else
    {
        learner->streak = 1;    /* first day, or the streak was broken */
    }
    learner->lastDay = today;
    *grew = true;
    if (learner->streak > learner->bestStreak)
    {
        learner->bestStreak = learner->streak;
    }

    unsigned before = learner->badges;
    if (learner->streak >= STREAK_BADGE_DAYS)
    {
        learner->badges |= 1u << BADGE_ON_A_ROLL;
    }
    return learner->badges & ~before;
}

/* The streak as it stands today: still alive if the last activity was
   today or yesterday, otherwise 0 */
int currentStreak(const Learner *learner, int today)
{
    if (learner->lastDay < 0 || learner->lastDay < today - 1)
    {
        return 0;
    }
    return learner->streak;
}
