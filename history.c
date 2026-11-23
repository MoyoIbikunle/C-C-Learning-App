/*
 * history.c - a record of every finished lesson, practice run and exam.
 *
 * Attempts are kept in a linked list in the order they happened. A new
 * attempt is added at the tail (we keep a pointer to it, so there's no
 * need to walk the whole list) and appended to history.txt.
 *
 * history.txt, one line per attempt:
 *   ATTEMPT Bolu 0 3 80 4 5 20774    learner, kind, lesson (0 = mixed), %, right, total, day
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

const char *ATTEMPT_NAMES[ATTEMPT_KINDS] = { "Lesson", "Quick quiz", "Review", "Exam" };

static bool readAttempt(const char *line, Attempt *a)
{
    int kind, lesson;
    if (sscanf(line, "ATTEMPT %31s %d %d %d %d %d %d", a->learner, &kind, &lesson,
               &a->percent, &a->right, &a->total, &a->day) != 7)
    {
        return false;
    }
    if (kind < 0 || kind >= ATTEMPT_KINDS || lesson < 0 || lesson > MAX_LESSONS)
    {
        return false;
    }
    a->kind = (AttemptKind)kind;
    a->lesson = lesson - 1;   /* the file counts lessons from 1, with 0 for mixed */
    return true;
}

/* Returns how many attempts were loaded (0 if there's no file yet) */
int loadHistory(const char *filename, AttemptList *list)
{
    list->head = list->tail = NULL;
    list->count = 0;
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        return 0;
    }
    char line[256];
    Attempt a;
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (readAttempt(line, &a) && !addAttempt(list, &a))
        {
            break;   /* out of memory */
        }
    }
    fclose(fp);
    return list->count;
}

/* Copies the attempt into a new node at the end of the list */
bool addAttempt(AttemptList *list, const Attempt *attempt)
{
    Attempt *node = malloc(sizeof(Attempt));
    if (node == NULL)
    {
        return false;
    }
    *node = *attempt;
    node->next = NULL;
    if (list->tail == NULL)
    {
        list->head = node;
    }
    else
    {
        list->tail->next = node;
    }
    list->tail = node;
    list->count++;
    return true;
}

/* Adds one line to the end of the file, so the whole history isn't rewritten */
bool appendAttemptToFile(const char *filename, const Attempt *attempt)
{
    FILE *fp = fopen(filename, "a");
    if (fp == NULL)
    {
        return false;
    }
    fprintf(fp, "ATTEMPT %s %d %d %d %d %d %d\n", attempt->learner, (int)attempt->kind, attempt->lesson + 1,
            attempt->percent, attempt->right, attempt->total, attempt->day);
    fclose(fp);
    return true;
}

void freeHistory(AttemptList *list)
{
    Attempt *a = list->head;
    while (a != NULL)
    {
        Attempt *next = a->next;
        free(a);
        a = next;
    }
    list->head = list->tail = NULL;
    list->count = 0;
}

int attemptCount(const AttemptList *list, const char *learner)
{
    int n = 0;
    for (const Attempt *a = list->head; a != NULL; a = a->next)
    {
        n += strcmp(a->learner, learner) == 0;
    }
    return n;
}

/* The learner's last max attempts, oldest first (ready to draw as a chart).
   Returns how many. */
int recentAttempts(const AttemptList *list, const char *learner, const Attempt **out, int max)
{
    int total = attemptCount(list, learner);
    int skip = total > max ? total - max : 0;
    int n = 0;
    for (const Attempt *a = list->head; a != NULL && n < max; a = a->next)
    {
        if (strcmp(a->learner, learner) != 0)
        {
            continue;
        }
        if (skip > 0)
        {
            skip--;
            continue;
        }
        out[n++] = a;
    }
    return n;
}
