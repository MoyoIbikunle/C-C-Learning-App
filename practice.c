/*
 * practice.c - the Practise tab: picking questions, remembering mistakes
 * and scoring exams.
 *
 * A QuestionRef says where a question is (lesson and position), so a quiz
 * can mix questions from different lessons. Each learner keeps one
 * unsigned per lesson as a set of bits: bit k is on when question k needs
 * reviewing. Getting it wrong turns the bit on; getting it right first
 * time turns it off.
 */
#include <stdlib.h>
#include "app.h"

/* Fills out with the questions to choose from. Returns how many.
   POOL_PASSED: every question in the lessons the learner has passed.
   POOL_MISSED: every question they've got wrong, from any lesson. */
int buildPool(const Learner *learner, const Course *course, PoolKind kind, QuestionRef *out, int max)
{
    int count = 0;
    for (int l = 0; l < course->lessonCount; l++)
    {
        if (kind == POOL_PASSED && !lessonPassed(learner, l))
        {
            continue;   /* only practise lessons you've done */
        }
        for (int k = 0; k < course->lessons[l].questionCount && count < max; k++)
        {
            QuestionRef ref = { l, k };
            if (kind == POOL_PASSED || isMissed(learner, ref))
            {
                out[count++] = ref;
            }
        }
    }
    return count;
}

/* Fisher-Yates shuffle: each position swaps with a random one at or before it */
void shuffleRefs(QuestionRef *refs, int count)
{
    for (int i = count - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        QuestionRef temp = refs[i];
        refs[i] = refs[j];
        refs[j] = temp;
    }
}

const Question *questionFor(const Course *course, QuestionRef ref)
{
    if (ref.lesson < 0 || ref.lesson >= course->lessonCount)
    {
        return NULL;
    }
    return questionAt(&course->lessons[ref.lesson], ref.index);
}

void recordAnswer(Learner *learner, QuestionRef ref, bool rightFirstTime)
{
    if (ref.lesson < 0 || ref.lesson >= MAX_LESSONS || ref.index < 0 || ref.index >= MAX_QUESTIONS)
    {
        return;
    }
    /* Keep count of first tries per lesson, for the accuracy on the Progress tab */
    learner->topicTried[ref.lesson]++;
    if (rightFirstTime)
    {
        learner->topicRight[ref.lesson]++;
    }

    unsigned bit = 1u << ref.index;
    if (rightFirstTime)
    {
        learner->missed[ref.lesson] &= ~bit;   /* turn the bit off */
    }
    else
    {
        learner->missed[ref.lesson] |= bit;    /* turn it on */
    }
}

bool isMissed(const Learner *learner, QuestionRef ref)
{
    if (ref.lesson < 0 || ref.lesson >= MAX_LESSONS || ref.index < 0 || ref.index >= MAX_QUESTIONS)
    {
        return false;
    }
    return (learner->missed[ref.lesson] >> ref.index) & 1u;
}

int missedCount(const Learner *learner)
{
    int n = 0;
    for (int l = 0; l < MAX_LESSONS; l++)
    {
        for (int k = 0; k < MAX_QUESTIONS; k++)
        {
            n += (learner->missed[l] >> k) & 1u;
        }
    }
    return n;
}

/* Saves the exam score if it's the best yet. A full exam (all the
   questions and the coding task) at 90%+ earns Exam Ace. Returns the
   badges earned for the first time. */
unsigned finishExam(Learner *learner, int right, int total, bool *newBest)
{
    int percent = (total > 0) ? right * 100 / total : 0;
    *newBest = percent > learner->bestExam;
    if (*newBest)
    {
        learner->bestExam = percent;
    }

    unsigned before = learner->badges;
    if (total >= EXAM_FULL_MARKS && percent >= EXAM_ACE_PERCENT)
    {
        learner->badges |= 1u << BADGE_EXAM_ACE;
    }
    return learner->badges & ~before;
}

/* Percentage of first tries that were right in a lesson, or -1 if none yet */
int topicPercent(const Learner *learner, int lesson)
{
    if (lesson < 0 || lesson >= MAX_LESSONS || learner->topicTried[lesson] == 0)
    {
        return -1;
    }
    return learner->topicRight[lesson] * 100 / learner->topicTried[lesson];
}

/* The lessons with the lowest accuracy (at least 3 answers, under 100%),
   weakest first. Fills out with lesson indexes and returns how many. */
int weakestTopics(const Learner *learner, const Course *course, int *out, int max)
{
    int count = 0;
    int list[MAX_LESSONS];
    for (int l = 0; l < course->lessonCount; l++)
    {
        int p = topicPercent(learner, l);
        if (learner->topicTried[l] >= 3 && p < 100)
        {
            list[count++] = l;
        }
    }

    /* Insertion sort, lowest percentage first */
    for (int i = 1; i < count; i++)
    {
        int key = list[i];
        int j = i - 1;
        while (j >= 0 && topicPercent(learner, list[j]) > topicPercent(learner, key))
        {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = key;
    }

    int n = count < max ? count : max;
    for (int i = 0; i < n; i++)
    {
        out[i] = list[i];
    }
    return n;
}

/* Marks for the exam's coding task: a share of maxMarks for the checks
   that passed, rounded to the nearest mark. 4 of 6 checks = 3 of 5 marks. */
int codeMarks(int checksPassed, int checkCount, int maxMarks)
{
    if (checkCount <= 0 || checksPassed <= 0)
    {
        return 0;
    }
    if (checksPassed >= checkCount)
    {
        return maxMarks;
    }
    return (maxMarks * checksPassed * 2 + checkCount) / (checkCount * 2);
}
