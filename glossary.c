/*
 * glossary.c - key words, what they mean, and which lesson covers them.
 *
 * Terms are loaded from content/glossary.txt into a linked list:
 *   TERM pointer
 *   LESSON 9
 *   MEANS A variable that stores the address of another variable.
 *   EXAMPLE int *p = &x;
 *   END
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static void copyText(char *dst, const char *src, size_t n)
{
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

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

int loadGlossary(const char *filename, Glossary *glossary, char *error, int errorSize)
{
    glossary->head = NULL;
    glossary->count = 0;
    error[0] = '\0';

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        snprintf(error, errorSize, "Can't open %s", filename);
        return 0;
    }

    Term *term = NULL, *tail = NULL;
    char line[1024];
    int lineNo = 0;
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        lineNo++;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0')
        {
            continue;
        }

        const char *rest;
        if ((rest = after(line, "TERM")) != NULL)
        {
            if (term != NULL)
            {
                snprintf(error, errorSize, "Line %d: TERM before END", lineNo);
                break;
            }
            term = calloc(1, sizeof(Term));
            if (term == NULL)
            {
                snprintf(error, errorSize, "Out of memory");
                break;
            }
            copyText(term->word, rest, sizeof(term->word));
            term->lesson = -1;
        }
        else if (term == NULL)
        {
            snprintf(error, errorSize, "Line %d: expected TERM first", lineNo);
            break;
        }
        else if ((rest = after(line, "LESSON")) != NULL)
        {
            term->lesson = atoi(rest) - 1;
        }
        else if ((rest = after(line, "MEANS")) != NULL)
        {
            copyText(term->means, rest, sizeof(term->means));
        }
        else if ((rest = after(line, "EXAMPLE")) != NULL)
        {
            copyText(term->example, rest, sizeof(term->example));
        }
        else if (strcmp(line, "END") == 0)
        {
            if (term->word[0] == '\0' || term->means[0] == '\0' || term->lesson < 0 || term->lesson >= MAX_LESSONS)
            {
                snprintf(error, errorSize, "Line %d: term '%s' needs a lesson and a meaning", lineNo, term->word);
                break;
            }
            if (tail == NULL)
            {
                glossary->head = term;
            }
            else
            {
                tail->next = term;
            }
            tail = term;
            glossary->count++;
            term = NULL;
        }
        else
        {
            snprintf(error, errorSize, "Line %d: didn't understand '%.40s'", lineNo, line);
            break;
        }
    }
    fclose(fp);
    free(term);

    if (error[0] != '\0')
    {
        freeGlossary(glossary);
        return 0;
    }
    return glossary->count;
}

void freeGlossary(Glossary *glossary)
{
    Term *t = glossary->head;
    while (t != NULL)
    {
        Term *next = t->next;
        free(t);
        t = next;
    }
    glossary->head = NULL;
    glossary->count = 0;
}

const Term *termAt(const Glossary *glossary, int index)
{
    const Term *t = glossary->head;
    for (int i = 0; t != NULL && i < index; i++)
    {
        t = t->next;
    }
    return t;
}

/* Finds needle in haystack ignoring capitals; returns where, or NULL */
static const char *findNoCase(const char *haystack, const char *needle)
{
    size_t n = strlen(needle);
    if (n == 0)
    {
        return haystack;
    }
    for (const char *p = haystack; *p != '\0'; p++)
    {
        size_t i = 0;
        while (i < n && p[i] != '\0' && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
        {
            i++;
        }
        if (i == n)
        {
            return p;
        }
    }
    return NULL;
}

/* Terms whose word or meaning contains query (an empty query matches all).
   Matches on the word come first. Returns how many. */
int searchGlossary(const Glossary *glossary, const char *query, const Term **out, int max)
{
    int n = 0;
    for (int pass = 0; pass < 2; pass++)
    {
        for (const Term *t = glossary->head; t != NULL && n < max; t = t->next)
        {
            bool inWord = findNoCase(t->word, query) != NULL;
            bool inMeaning = findNoCase(t->means, query) != NULL;
            if ((pass == 0 && inWord) || (pass == 1 && !inWord && inMeaning))
            {
                out[n++] = t;
            }
        }
    }
    return n;
}

static bool isWordChar(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Terms that appear in text as whole words (so "array" isn't found inside
   "arrays_x", but "arrays" does count for "array"). Returns how many. */
int termsInText(const Glossary *glossary, const char *text, const Term **out, int max)
{
    int n = 0;
    for (const Term *t = glossary->head; t != NULL && n < max; t = t->next)
    {
        size_t len = strlen(t->word);
        for (const char *p = findNoCase(text, t->word); p != NULL; p = findNoCase(p + 1, t->word))
        {
            bool startOk = p == text || !isWordChar(p[-1]);
            char end = p[len];
            bool endOk = !isWordChar(end) || ((end == 's' || end == 'S') && !isWordChar(p[len + 1]));
            if (startOk && endOk)
            {
                out[n++] = t;
                break;
            }
        }
    }
    return n;
}
