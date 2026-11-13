# C Quest

An interactive app for learning C and C++, written in C99 with Raylib.
Learners read short tutorial cards, answer questions, earn points and badges,
and unlock the next lesson as they go.

## Building (Windows, w64devkit)

Raylib should be in `C:\raylib` (`include\raylib.h` and `lib\libraylib.a`).

```
make          # builds cquest.exe
make test     # runs the console tests
```

Run `cquest.exe` from this folder; it needs the `content` and `assets` folders next to it.

## What it does

- **Sign in** with your name. Each learner's progress is saved to `learners.txt`.
- **Learning path**: 12 lessons over three levels (Beginner, Intermediate, Advanced).
  The first lesson is open; passing a lesson with 70% or more unlocks the next one.
- **Lessons**: a few tutorial cards with code examples, then questions:
  - *Choose the answer*: multiple choice
  - *Predict the output*: read the code and pick what it prints
  - *Spot the bug*: click the line with the mistake
- **Points**: 10 for a right answer first time, 5 on the second try. Only your best
  attempt at each lesson counts, so points can't be farmed by repeating lessons.
- **Badges**: 7 to earn, each worth 25 bonus points.

All 12 lessons are written, with 76 questions in total:

- **Beginner** (Intro to C notes): First programs, Variables and maths, Decisions and loops, Arrays and strings
- **Intermediate** (Lab 1): Functions, Number problems, Array problems, Patterns and grids
- **Advanced** (Coding Exam 2 notes): Pointers, Memory management, Linked lists, Classes

The Advanced lessons use C++ for `new`, `delete`, `nullptr` and classes.
The Practise and Teach tabs are planned for later.

## Files

| File | What it does |
| --- | --- |
| `app.h` | Shared types: Course, Lesson, Card, Question, Learner, Badge |
| `course.c` | Loads `content/course.txt` into linked lists of cards and questions |
| `learner.c` | Points, unlocking, badges, saving and loading learners |
| `ui.c`, `ui.h` | Fonts, colours, buttons, wrapped text and code panels |
| `main.c` | The screens: sign in, path, cards, questions, results, badges |
| `test_app.c` | Console tests for the loader and progress logic |
| `content/course.txt` | All the lesson content (plain text, easy to add to) |
| `assets/` | Poppins and IBM Plex Mono fonts (SIL Open Font Licence) |

## Adding lessons

Lessons live in `content/course.txt`, so new content doesn't need any code changes.
The format is described at the top of `course.c`. A question looks like this:

```
QUESTION output
TEXT What does this print?
CODE printf("%d", 7 / 2);
OPTION 3.5
OPTION 3
ANSWER 2
EXPLAIN Both are ints, so / does integer division and drops the .5.
END
```

A lesson with no questions shows as "Coming soon" on the path.
