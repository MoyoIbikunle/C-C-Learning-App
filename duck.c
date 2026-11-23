/*
 * duck.c - the Teach tab: the duck's broken programs.
 *
 * Problems are loaded from content/duck.txt into a linked list. Each one
 * has the duck's own (wrong) idea about what's broken, the code, the line
 * that's really wrong, some possible fixes, and what the duck learned.
 *
 *   DUCK Average                    title
 *   LESSON 2                        the lesson it's about
 *   SAYS I think float can't...     what the duck thinks is wrong
 *   CODE int a = 7, b = 8;          a line of code (as many as needed)
 *   BUG 2                           the line that's really wrong (from 1)
 *   FIX float average = ...         a possible replacement for that line
 *   ANSWER 3                        the right fix (from 1)
 *   THANKS So it's not the float!   shown once it's fixed
 *   END
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static void copyText(char *dst, const char *src, size_t n)
{
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

/* If line starts with keyword (then a space or the end), returns what follows */
static const char *after(const char *line, const char *keyword)
{
    size_t k = strlen(keyword);
    if (strncmp(line, keyword, k) != 0)
    {
        return NULL;
    }
    if (line[k] == '\0')
    {
        return line + k;
    }
    return line[k] == ' ' ? line + k + 1 : NULL;
}

/* Returns the number of problems loaded, or 0 with a reason in error */
int loadDucks(const char *filename, DuckList *ducks, char *error, int errorSize)
{
    ducks->head = NULL;
    ducks->count = 0;
    error[0] = '\0';

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        snprintf(error, errorSize, "Can't open %s", filename);
        return 0;
    }

    DuckProblem *duck = NULL;     /* the problem being read */
    DuckProblem *tail = NULL;     /* last problem in the list, to add after */
    char line[1024];
    int lineNo = 0;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        lineNo++;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }
        if (line[0] == '#' || line[0] == '\0')
        {
            continue;
        }

        const char *rest;
        if ((rest = after(line, "DUCK")) != NULL)
        {
            if (duck != NULL || ducks->count >= MAX_DUCKS)
            {
                snprintf(error, errorSize, "Line %d: DUCK before END, or too many problems", lineNo);
                break;
            }
            duck = calloc(1, sizeof(DuckProblem));
            if (duck == NULL)
            {
                snprintf(error, errorSize, "Out of memory");
                break;
            }
            copyText(duck->title, rest, sizeof(duck->title));
            duck->lesson = -1;
        }
        else if (duck == NULL)
        {
            snprintf(error, errorSize, "Line %d: expected DUCK first", lineNo);
            break;
        }
        else if ((rest = after(line, "LESSON")) != NULL)
        {
            duck->lesson = atoi(rest) - 1;
        }
        else if ((rest = after(line, "SAYS")) != NULL)
        {
            copyText(duck->says, rest, TEXT_LEN);
        }
        else if ((rest = after(line, "CODE")) != NULL)
        {
            if (duck->codeCount >= MAX_CODE)
            {
                snprintf(error, errorSize, "Line %d: too many CODE lines (max %d)", lineNo, MAX_CODE);
                break;
            }
            copyText(duck->code[duck->codeCount++], rest, LINE_LEN);
        }
        else if ((rest = after(line, "BUG")) != NULL)
        {
            duck->bugLine = atoi(rest);
        }
        else if ((rest = after(line, "FIX")) != NULL)
        {
            if (duck->fixCount >= MAX_OPTIONS)
            {
                snprintf(error, errorSize, "Line %d: too many FIX lines (max %d)", lineNo, MAX_OPTIONS);
                break;
            }
            copyText(duck->fixes[duck->fixCount++], rest, LINE_LEN);
        }
        else if ((rest = after(line, "ANSWER")) != NULL)
        {
            duck->answer = atoi(rest);
        }
        else if ((rest = after(line, "THANKS")) != NULL)
        {
            copyText(duck->thanks, rest, TEXT_LEN);
        }
        else if (strcmp(line, "END") == 0)
        {
            /* Check everything points at something real */
            if (duck->lesson < 0 || duck->lesson >= MAX_LESSONS ||
                duck->bugLine < 1 || duck->bugLine > duck->codeCount ||
                duck->fixCount < 2 || duck->answer < 1 || duck->answer > duck->fixCount ||
                duck->says[0] == '\0' || duck->thanks[0] == '\0')
            {
                snprintf(error, errorSize, "Line %d: duck problem '%s' is missing something or has a bad number",
                         lineNo, duck->title);
                break;
            }
            /* Add to the end of the linked list */
            if (tail == NULL)
            {
                ducks->head = duck;
            }
            else
            {
                tail->next = duck;
            }
            tail = duck;
            ducks->count++;
            duck = NULL;
        }
        else
        {
            snprintf(error, errorSize, "Line %d: didn't understand '%.40s'", lineNo, line);
            break;
        }
    }
    fclose(fp);

    free(duck);   /* a half-read problem, if we stopped early */
    if (error[0] == '\0' && ducks->count == 0)
    {
        snprintf(error, errorSize, "No problems in %s", filename);
    }
    if (error[0] != '\0')
    {
        freeDucks(ducks);
        return 0;
    }
    return ducks->count;
}

void freeDucks(DuckList *ducks)
{
    DuckProblem *duck = ducks->head;
    while (duck != NULL)
    {
        DuckProblem *next = duck->next;
        free(duck);
        duck = next;
    }
    ducks->head = NULL;
    ducks->count = 0;
}

const DuckProblem *duckAt(const DuckList *ducks, int index)
{
    const DuckProblem *duck = ducks->head;
    for (int i = 0; duck != NULL && i < index; i++)
    {
        duck = duck->next;
    }
    return duck;
}

/* A problem opens once its lesson has been passed */
bool duckUnlocked(const Learner *learner, const Course *course, const DuckProblem *duck)
{
    return lessonReady(course, duck->lesson) && lessonPassed(learner, duck->lesson);
}

/* Records a solved problem. firstTime says whether it's new (and earns points).
   Helping with every problem earns Duck Helper. Returns new badges. */
unsigned finishDuck(Learner *learner, int index, int duckCount, bool *firstTime)
{
    *firstTime = false;
    if (index < 0 || index >= MAX_DUCKS)
    {
        return 0;
    }
    unsigned bit = 1u << index;
    *firstTime = (learner->ducksHelped & bit) == 0;
    learner->ducksHelped |= bit;

    unsigned all = (duckCount >= 32) ? 0xFFFFFFFFu : (1u << duckCount) - 1;
    unsigned before = learner->badges;
    if (duckCount > 0 && (learner->ducksHelped & all) == all)
    {
        learner->badges |= 1u << BADGE_DUCK_HELPER;
    }
    return learner->badges & ~before;
}
