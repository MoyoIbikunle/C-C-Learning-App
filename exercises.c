/*
 * exercises.c - coding exercises for the Code tab.
 *
 * Each exercise has a task, some starter code, hidden test code (a main
 * that calls the learner's function and prints the results), and the
 * lines the tests should print when the code is right. The learner's
 * code and the tests are joined into one program, compiled and run.
 *
 * content/exercises.txt:
 *   EXERCISE Leap years             title
 *   LESSON 6                        the lesson it practises
 *   LANG c                          c or c++
 *   TASK Write isLeapYear...        what to do (one paragraph)
 *   HINT Use % to check...          shown if the learner asks
 *   STARTER bool isLeapYear(...)    starter code, one line each ("STARTER" alone = blank line)
 *   TEST printf("%d\n", ...);       hidden test code, one line each
 *   EXPECT isLeapYear(2024) = 1     a line the tests should print, in order
 *   SOLUTION ...                    a model answer, one line each
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

/* Adds a line of code to a block of text. Returns false if it's full. */
static bool addLine(char *block, const char *line)
{
    size_t used = strlen(block), len = strlen(line);
    if (used + len + 2 > CODE_TEXT_LEN)
    {
        return false;
    }
    memcpy(block + used, line, len);
    block[used + len] = '\n';
    block[used + len + 1] = '\0';
    return true;
}

int loadExercises(const char *filename, ExerciseList *list, char *error, int errorSize)
{
    list->head = NULL;
    list->count = 0;
    error[0] = '\0';

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        snprintf(error, errorSize, "Can't open %s", filename);
        return 0;
    }

    Exercise *ex = NULL, *tail = NULL;
    char line[1024];
    int lineNo = 0;
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        lineNo++;
        line[strcspn(line, "\r\n")] = '\0';
        if (ex == NULL && (line[0] == '#' || line[0] == '\0'))
        {
            continue;   /* comments only between exercises: # is real code inside one */
        }

        const char *rest;
        bool full = false;
        if ((rest = after(line, "EXERCISE")) != NULL)
        {
            if (ex != NULL || list->count >= MAX_EXERCISES)
            {
                snprintf(error, errorSize, "Line %d: EXERCISE before END, or too many exercises", lineNo);
                break;
            }
            ex = calloc(1, sizeof(Exercise));
            if (ex == NULL)
            {
                snprintf(error, errorSize, "Out of memory");
                break;
            }
            copyText(ex->title, rest, sizeof(ex->title));
            ex->lesson = -1;
        }
        else if (ex == NULL)
        {
            snprintf(error, errorSize, "Line %d: expected EXERCISE first", lineNo);
            break;
        }
        else if (line[0] == '\0')
        {
            continue;
        }
        else if ((rest = after(line, "LESSON")) != NULL)   ex->lesson = atoi(rest) - 1;
        else if ((rest = after(line, "LANG")) != NULL)     ex->cpp = strcmp(rest, "c++") == 0;
        else if ((rest = after(line, "TASK")) != NULL)     copyText(ex->task, rest, sizeof(ex->task));
        else if ((rest = after(line, "HINT")) != NULL)     copyText(ex->hint, rest, sizeof(ex->hint));
        else if ((rest = after(line, "STARTER")) != NULL)  full = !addLine(ex->starter, rest);
        else if ((rest = after(line, "TEST")) != NULL)     full = !addLine(ex->tests, rest);
        else if ((rest = after(line, "SOLUTION")) != NULL) full = !addLine(ex->solution, rest);
        else if ((rest = after(line, "EXPECT")) != NULL)
        {
            if (ex->expectCount >= MAX_CHECKS)
            {
                snprintf(error, errorSize, "Line %d: too many EXPECT lines (max %d)", lineNo, MAX_CHECKS);
                break;
            }
            copyText(ex->expect[ex->expectCount++], rest, LINE_LEN);
        }
        else if (strcmp(line, "END") == 0)
        {
            if (ex->lesson < 0 || ex->lesson >= MAX_LESSONS || ex->task[0] == '\0' || ex->tests[0] == '\0' ||
                ex->expectCount == 0 || ex->solution[0] == '\0')
            {
                snprintf(error, errorSize, "Line %d: exercise '%s' is missing something", lineNo, ex->title);
                break;
            }
            if (tail == NULL)
            {
                list->head = ex;
            }
            else
            {
                tail->next = ex;
            }
            tail = ex;
            list->count++;
            ex = NULL;
        }
        else
        {
            snprintf(error, errorSize, "Line %d: didn't understand '%.40s'", lineNo, line);
            break;
        }
        if (full)
        {
            snprintf(error, errorSize, "Line %d: too much code in '%s'", lineNo, ex->title);
            break;
        }
    }
    fclose(fp);
    free(ex);

    if (error[0] != '\0')
    {
        freeExercises(list);
        return 0;
    }
    return list->count;
}

void freeExercises(ExerciseList *list)
{
    Exercise *ex = list->head;
    while (ex != NULL)
    {
        Exercise *next = ex->next;
        free(ex);
        ex = next;
    }
    list->head = NULL;
    list->count = 0;
}

const Exercise *exerciseAt(const ExerciseList *list, int index)
{
    const Exercise *ex = list->head;
    for (int i = 0; ex != NULL && i < index; i++)
    {
        ex = ex->next;
    }
    return ex;
}

/* An exercise opens once its lesson has been passed */
bool exerciseUnlocked(const Learner *learner, const Exercise *ex)
{
    return lessonPassed(learner, ex->lesson);
}

/* Joins the usual #includes, the learner's code and the hidden tests into
   one program. #line makes compiler errors give the learner's own line
   numbers. The helper turns off output buffering, so anything printed
   before a crash still shows. Pass tests = NULL for the playground. */
int buildProgram(const char *userCode, const char *tests, bool cpp, char *out, int size)
{
    const char *prelude = cpp ?
        "#include <iostream>\n#include <cstdio>\n#include <cstdlib>\n#include <cstring>\nusing namespace std;\n" :
        "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <math.h>\n";
    const char *unbuffer =
        "static void cq_unbuffered(void) __attribute__((constructor));\n"
        "static void cq_unbuffered(void) { setvbuf(stdout, NULL, _IONBF, 0); }\n";
    int n = snprintf(out, size, "%s%s#line 1 \"your code\"\n%s\n%s%s", prelude, unbuffer, userCode,
                     tests != NULL ? "#line 1 \"tests\"\n" : "", tests != NULL ? tests : "");
    return n < size ? n : size - 1;
}

/* Copies line number index (from 0) of text into out, without \r or
   trailing spaces. Returns false if there's no such line. */
static bool lineOf(const char *text, int index, char *out, int size)
{
    const char *p = text;
    for (int i = 0; i < index; i++)
    {
        p = strchr(p, '\n');
        if (p == NULL)
        {
            return false;
        }
        p++;
    }
    if (*p == '\0')
    {
        return false;
    }
    int len = (int)strcspn(p, "\n");
    if (len > size - 1)
    {
        len = size - 1;
    }
    memcpy(out, p, len);
    out[len] = '\0';
    while (len > 0 && (out[len - 1] == '\r' || out[len - 1] == ' '))
    {
        out[--len] = '\0';
    }
    return true;
}

/* Compares what the program printed with what was expected, line by line.
   Fills checks (one per expected line) and returns how many passed. */
int checkOutput(const Exercise *ex, const char *output, CheckLine *checks)
{
    int passed = 0;
    for (int i = 0; i < ex->expectCount; i++)
    {
        CheckLine *c = &checks[i];
        copyText(c->expected, ex->expect[i], sizeof(c->expected));
        c->gotLine = lineOf(output, i, c->got, sizeof(c->got));
        if (!c->gotLine)
        {
            c->got[0] = '\0';
        }
        c->passed = c->gotLine && strcmp(c->got, c->expected) == 0;
        passed += c->passed;
    }
    return passed;
}

/* Records a solved exercise. Returns true the first time (worth points). */
bool finishExercise(Learner *learner, int index)
{
    if (index < 0 || index >= MAX_EXERCISES)
    {
        return false;
    }
    unsigned bit = 1u << index;
    bool first = (learner->codeSolved & bit) == 0;
    learner->codeSolved |= bit;
    return first;
}
