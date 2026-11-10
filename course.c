/*
 * course.c - loads the course from content/course.txt.
 *
 * Each lesson keeps its tutorial cards and its questions in linked lists,
 * built with malloc as the file is read and released with free.
 *
 * File format (one keyword per line; # starts a comment):
 *
 *   LESSON 1 1 First programs        lesson number, level (1-3), title
 *   SUMMARY What the lesson covers
 *   CARD Your first C program        a tutorial card...
 *   TEXT A paragraph of explanation  (up to 6 per card)
 *   CODE int main() {                a line of example code ("CODE" alone = blank line)
 *   END
 *   QUESTION choice                  choice, output or bug
 *   TEXT The question
 *   CODE ...                         code to read (bug: the learner clicks a line)
 *   OPTION An answer                 (choice and output: 2 to 5 options)
 *   ANSWER 2                         right option, or the line with the bug (from 1)
 *   EXPLAIN Why that's the answer
 *   END
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

const char *LEVEL_NAMES[LEVELS] = { "Beginner", "Intermediate", "Advanced" };

/* Copies src into dst (size n), always ending with '\0' */
static void copyText(char *dst, const char *src, size_t n)
{
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

/* Removes the newline (and a Windows \r) from the end of a line */
static void trimEnd(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    {
        line[--len] = '\0';
    }
}

/* If line starts with keyword, returns what follows it (after one space), else NULL */
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
    if (line[k] == ' ')
    {
        return line + k + 1;
    }
    return NULL;   /* e.g. "CODEX" is not "CODE" */
}

static void appendCard(Lesson *lesson, Card *card)
{
    card->next = NULL;
    if (lesson->cards == NULL)
    {
        lesson->cards = card;
    }
    else
    {
        Card *last = lesson->cards;
        while (last->next != NULL)
        {
            last = last->next;
        }
        last->next = card;
    }
    lesson->cardCount++;
}

static void appendQuestion(Lesson *lesson, Question *q)
{
    q->next = NULL;
    if (lesson->questions == NULL)
    {
        lesson->questions = q;
    }
    else
    {
        Question *last = lesson->questions;
        while (last->next != NULL)
        {
            last = last->next;
        }
        last->next = q;
    }
    lesson->questionCount++;
}

/* Reads the course. Returns the number of lessons, or 0 with a reason in error. */
int loadCourse(const char *filename, Course *course, char *error, int errorSize)
{
    memset(course, 0, sizeof(*course));
    error[0] = '\0';

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        snprintf(error, errorSize, "Can't open %s", filename);
        return 0;
    }

    Lesson *lesson = NULL;
    Card *card = NULL;          /* the card being read, if any */
    Question *q = NULL;         /* the question being read, if any */
    char line[1024];
    int lineNo = 0;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        lineNo++;
        trimEnd(line);
        if (line[0] == '#' || line[0] == '\0')
        {
            continue;
        }

        const char *rest;
        if ((rest = after(line, "LESSON")) != NULL)
        {
            int number, level, used;
            if (course->lessonCount >= MAX_LESSONS ||
                sscanf(rest, "%d %d %n", &number, &level, &used) != 2 || level < 1 || level > LEVELS)
            {
                snprintf(error, errorSize, "Line %d: bad LESSON line", lineNo);
                break;
            }
            lesson = &course->lessons[course->lessonCount++];
            lesson->number = number;
            lesson->level = level - 1;
            copyText(lesson->title, rest + used, sizeof(lesson->title));
        }
        else if (lesson == NULL)
        {
            snprintf(error, errorSize, "Line %d: expected LESSON first", lineNo);
            break;
        }
        else if ((rest = after(line, "SUMMARY")) != NULL && card == NULL && q == NULL)
        {
            copyText(lesson->summary, rest, sizeof(lesson->summary));
        }
        else if ((rest = after(line, "CARD")) != NULL)
        {
            card = calloc(1, sizeof(Card));
            if (card == NULL)
            {
                snprintf(error, errorSize, "Out of memory");
                break;
            }
            copyText(card->title, rest, sizeof(card->title));
        }
        else if ((rest = after(line, "QUESTION")) != NULL)
        {
            q = calloc(1, sizeof(Question));
            if (q == NULL)
            {
                snprintf(error, errorSize, "Out of memory");
                break;
            }
            if (strcmp(rest, "choice") == 0)      q->type = Q_CHOICE;
            else if (strcmp(rest, "output") == 0) q->type = Q_OUTPUT;
            else if (strcmp(rest, "bug") == 0)    q->type = Q_BUG;
            else
            {
                snprintf(error, errorSize, "Line %d: unknown question type '%s'", lineNo, rest);
                free(q);
                q = NULL;
                break;
            }
        }
        else if ((rest = after(line, "TEXT")) != NULL)
        {
            if (card != NULL && card->textCount < MAX_PARAS)
            {
                copyText(card->text[card->textCount++], rest, TEXT_LEN);
            }
            else if (q != NULL)
            {
                copyText(q->text, rest, TEXT_LEN);
            }
        }
        else if ((rest = after(line, "CODE")) != NULL)
        {
            if (card != NULL && card->codeCount < MAX_CODE)
            {
                copyText(card->code[card->codeCount++], rest, LINE_LEN);
            }
            else if (q != NULL && q->codeCount < MAX_CODE)
            {
                copyText(q->code[q->codeCount++], rest, LINE_LEN);
            }
        }
        else if ((rest = after(line, "OPTION")) != NULL && q != NULL && q->optionCount < MAX_OPTIONS)
        {
            copyText(q->options[q->optionCount++], rest, LINE_LEN);
        }
        else if ((rest = after(line, "ANSWER")) != NULL && q != NULL)
        {
            q->answer = atoi(rest);
        }
        else if ((rest = after(line, "EXPLAIN")) != NULL && q != NULL)
        {
            copyText(q->explain, rest, TEXT_LEN);
        }
        else if (strcmp(line, "END") == 0)
        {
            if (card != NULL)
            {
                appendCard(lesson, card);
                card = NULL;
            }
            else if (q != NULL)
            {
                /* Check the answer points at something that exists */
                int limit = (q->type == Q_BUG) ? q->codeCount : q->optionCount;
                if (q->answer < 1 || q->answer > limit || q->text[0] == '\0' ||
                    (q->type != Q_BUG && q->optionCount < 2))
                {
                    snprintf(error, errorSize, "Line %d: question in lesson %d has a bad answer or options",
                             lineNo, lesson->number);
                    free(q);
                    q = NULL;
                    break;
                }
                appendQuestion(lesson, q);
                q = NULL;
            }
        }
        else
        {
            snprintf(error, errorSize, "Line %d: didn't understand '%.40s'", lineNo, line);
            break;
        }
    }
    fclose(fp);

    /* Anything half-read when we stopped is freed rather than leaked */
    free(card);
    free(q);

    if (error[0] != '\0')
    {
        freeCourse(course);
        return 0;
    }
    return course->lessonCount;
}

/* Frees every card and question in every lesson */
void freeCourse(Course *course)
{
    for (int i = 0; i < course->lessonCount; i++)
    {
        Lesson *lesson = &course->lessons[i];

        Card *card = lesson->cards;
        while (card != NULL)
        {
            Card *next = card->next;
            free(card);
            card = next;
        }

        Question *q = lesson->questions;
        while (q != NULL)
        {
            Question *next = q->next;
            free(q);
            q = next;
        }

        lesson->cards = NULL;
        lesson->questions = NULL;
        lesson->cardCount = 0;
        lesson->questionCount = 0;
    }
    course->lessonCount = 0;
}

/* The card at position index (from 0) by walking the list, or NULL */
const Card *cardAt(const Lesson *lesson, int index)
{
    const Card *card = lesson->cards;
    for (int i = 0; card != NULL && i < index; i++)
    {
        card = card->next;
    }
    return card;
}

const Question *questionAt(const Lesson *lesson, int index)
{
    const Question *q = lesson->questions;
    for (int i = 0; q != NULL && i < index; i++)
    {
        q = q->next;
    }
    return q;
}

/* choice = the option (or for a bug question, the line) picked, from 1 */
bool isAnswer(const Question *q, int choice)
{
    return choice == q->answer;
}
