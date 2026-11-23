/*
 * runner.c - compiles and runs the learner's code for the Code tab.
 *
 * The code is saved to cq_run.c (or cq_run.cpp), compiled with gcc (or g++),
 * then run with a time limit, so an infinite loop is stopped instead of
 * freezing the app. Everything it prints is read through a pipe, up to a
 * limit, so a loop that prints forever is stopped too.
 *
 * Running a program is different on Windows and Linux, so that one part
 * has two versions. This file doesn't include raylib.h, because windows.h
 * and raylib.h use some of the same names.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L   /* for fork, pipe, kill and friends on Linux */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

#ifdef _WIN32
#include <windows.h>
#define EXE_NAME "cq_run.exe"
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define EXE_NAME "./cq_run"
#endif

#define COMPILE_LOG "cq_compile.txt"

/* Reads up to size-1 bytes of a file into out */
static void readFile(const char *path, char *out, int size)
{
    out[0] = '\0';
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        return;
    }
    size_t n = fread(out, 1, (size_t)size - 1, fp);
    out[n] = '\0';
    fclose(fp);
}

typedef enum { FINISHED, TIMED_OUT, TOO_MUCH_OUTPUT, COULD_NOT_START } RunEnd;

#ifdef _WIN32

static RunEnd runWithLimit(int timeLimitMs, RunResult *r, bool *crashed)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE readEnd, writeEnd;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0))
    {
        return COULD_NOT_START;
    }
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);   /* only the child gets the write end */
    HANDLE nul = CreateFileA("NUL", GENERIC_READ, 0, &sa, OPEN_EXISTING, 0, NULL);

    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writeEnd;
    si.hStdError = writeEnd;
    si.hStdInput = nul;   /* scanf gets nothing rather than waiting forever */
    PROCESS_INFORMATION pi;
    char command[] = EXE_NAME;

    /* No "has stopped working" box or crash report if the program crashes:
       those can take longer than the time limit. The child inherits this. */
    UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    BOOL started = CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    SetErrorMode(oldMode);   /* back to normal for the app itself */
    if (!started)
    {
        CloseHandle(readEnd);
        CloseHandle(writeEnd);
        CloseHandle(nul);
        return COULD_NOT_START;
    }
    CloseHandle(writeEnd);
    CloseHandle(nul);

    RunEnd end = FINISHED;
    DWORD start = GetTickCount();
    for (;;)
    {
        /* Read whatever has been printed so far */
        DWORD avail = 0;
        while (PeekNamedPipe(readEnd, NULL, 0, NULL, &avail, NULL) && avail > 0)
        {
            DWORD room = (DWORD)(RUN_OUTPUT_LEN - 1 - r->outputLen);
            if (room == 0)
            {
                end = TOO_MUCH_OUTPUT;
                break;
            }
            DWORD got = 0;
            ReadFile(readEnd, r->output + r->outputLen, avail < room ? avail : room, &got, NULL);
            r->outputLen += (int)got;
        }
        if (end == TOO_MUCH_OUTPUT)
        {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        if (WaitForSingleObject(pi.hProcess, 10) == WAIT_OBJECT_0)
        {
            /* finished: pick up anything still in the pipe */
            while (PeekNamedPipe(readEnd, NULL, 0, NULL, &avail, NULL) && avail > 0 && r->outputLen < RUN_OUTPUT_LEN - 1)
            {
                DWORD room = (DWORD)(RUN_OUTPUT_LEN - 1 - r->outputLen);
                DWORD got = 0;
                ReadFile(readEnd, r->output + r->outputLen, avail < room ? avail : room, &got, NULL);
                r->outputLen += (int)got;
            }
            break;
        }
        if (GetTickCount() - start > (DWORD)timeLimitMs)
        {
            TerminateProcess(pi.hProcess, 1);
            end = TIMED_OUT;
            break;
        }
    }
    WaitForSingleObject(pi.hProcess, 2000);

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    r->exitCode = (int)code;
    /* Windows crash codes start 0xC0000... (e.g. 0xC0000005 for a bad pointer) */
    *crashed = end == FINISHED && (code & 0xF0000000u) == 0xC0000000u;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(readEnd);
    r->seconds = (GetTickCount() - start) / 1000.0;
    return end;
}

#else

static double nowSeconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static RunEnd runWithLimit(int timeLimitMs, RunResult *r, bool *crashed)
{
    int fds[2];
    if (pipe(fds) != 0)
    {
        return COULD_NOT_START;
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        return COULD_NOT_START;
    }
    if (pid == 0)
    {
        /* the child: print into the pipe, read from nothing, become the program */
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        int nul = open("/dev/null", O_RDONLY);
        dup2(nul, 0);
        close(fds[0]);
        execl(EXE_NAME, EXE_NAME, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    fcntl(fds[0], F_SETFL, O_NONBLOCK);

    RunEnd end = FINISHED;
    double start = nowSeconds();
    int status = 0;
    bool exited = false;
    for (;;)
    {
        ssize_t got;
        while (r->outputLen < RUN_OUTPUT_LEN - 1 &&
               (got = read(fds[0], r->output + r->outputLen, RUN_OUTPUT_LEN - 1 - r->outputLen)) > 0)
        {
            r->outputLen += (int)got;
        }
        if (r->outputLen >= RUN_OUTPUT_LEN - 1)
        {
            end = TOO_MUCH_OUTPUT;
            break;
        }
        if (exited)
        {
            break;   /* exited last time round, and the pipe is now empty */
        }
        if (waitpid(pid, &status, WNOHANG) == pid)
        {
            exited = true;   /* one more read for anything left in the pipe */
            continue;
        }
        if ((nowSeconds() - start) * 1000 > timeLimitMs)
        {
            end = TIMED_OUT;
            break;
        }
        struct timespec pause = { 0, 10000000 };   /* 10 ms */
        nanosleep(&pause, NULL);
    }
    if (!exited)
    {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    close(fds[0]);

    r->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    *crashed = end == FINISHED && WIFSIGNALED(status);
    if (end == FINISHED && WIFEXITED(status) && WEXITSTATUS(status) == 127 && r->outputLen == 0)
    {
        return COULD_NOT_START;
    }
    r->seconds = nowSeconds() - start;
    return end;
}

#endif

/* Saves, compiles and runs source. The result says what happened. */
void runCode(const char *source, bool cpp, int timeLimitMs, RunResult *r)
{
    memset(r, 0, sizeof(*r));
    const char *file = cpp ? "cq_run.cpp" : "cq_run.c";

    FILE *fp = fopen(file, "w");
    if (fp == NULL)
    {
        r->status = RUN_FAILED;
        snprintf(r->errors, sizeof(r->errors), "Couldn't save %s in the C Quest folder.", file);
        return;
    }
    fputs(source, fp);
    fclose(fp);
    remove(EXE_NAME);
    remove(COMPILE_LOG);

    /* Compile, sending any error messages to a file */
    char command[300];
    snprintf(command, sizeof(command), "%s %s -w -o %s %s %s > %s 2>&1",
             cpp ? "g++" : "gcc", cpp ? "-std=c++17" : "-std=c99", EXE_NAME, file, cpp ? "" : "-lm", COMPILE_LOG);
    int compiled = system(command);
    if (compiled != 0)
    {
        readFile(COMPILE_LOG, r->errors, sizeof(r->errors));
        /* "'gcc' is not recognized..." on Windows, "gcc: not found" on Linux */
        bool noCompiler = strstr(r->errors, "not recognized") != NULL ||
                          (strstr(r->errors, "not found") != NULL && strstr(r->errors, "your code") == NULL &&
                           strstr(r->errors, "tests:") == NULL) ||
                          r->errors[0] == '\0';
        if (noCompiler)
        {
            r->status = RUN_NO_COMPILER;   /* e.g. "'gcc' is not recognized..." */
            snprintf(r->errors, sizeof(r->errors),
                     "Couldn't find %s. Start C Quest from the w64devkit terminal so it can compile your code.",
                     cpp ? "g++" : "gcc");
        }
        else
        {
            r->status = RUN_COMPILE_ERROR;
        }
        return;
    }

    bool crashed = false;
    RunEnd end = runWithLimit(timeLimitMs, r, &crashed);
    r->output[r->outputLen] = '\0';

    /* Windows turns each \n a program prints into \r\n; take the \r out
       so the output is the same on every computer */
    int j = 0;
    for (int i = 0; i < r->outputLen; i++)
    {
        if (r->output[i] != '\r')
        {
            r->output[j++] = r->output[i];
        }
    }
    r->outputLen = j;
    r->output[j] = '\0';
    switch (end)
    {
        case COULD_NOT_START:
            r->status = RUN_FAILED;
            snprintf(r->errors, sizeof(r->errors), "Your program compiled but couldn't be started.");
            break;
        case TIMED_OUT:       r->status = RUN_TIMEOUT; break;
        case TOO_MUCH_OUTPUT: r->status = RUN_TOO_MUCH; break;
        default:              r->status = crashed ? RUN_CRASHED : RUN_OK; break;
    }
}

/* Removes the files a run leaves behind (called when the app closes) */
void cleanUpRun(void)
{
    remove("cq_run.c");
    remove("cq_run.cpp");
    remove(EXE_NAME);
    remove(COMPILE_LOG);
}
