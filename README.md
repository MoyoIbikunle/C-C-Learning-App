# C Quest

An interactive app for learning C and C++, written in C99 with Raylib.
Learners read short tutorial cards, answer questions, earn points and badges,
and unlock the next lesson as they go.

## Building (Windows, w64devkit)

Raylib should be in `C:\raylib` (`include\raylib.h` and `lib\libraylib.a`).

```
make            # builds cquest.exe
make test       # runs the console tests
make test-code  # compiles and runs every coding exercise's model answer (slower)
```

Run `cquest.exe` from the w64devkit terminal in this folder. It needs the `content` and
`assets` folders next to it, and the Code tab needs `gcc` and `g++`, which w64devkit provides.

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
- **Badges**: 10 to earn, each worth 25 bonus points.
- **Daily streak**: finish a lesson, practice run or duck problem each day to keep it going.
  It shows in the header, and 7 days in a row earns the On a Roll badge.
- **Practise tab**: practice that doesn't change lesson scores:
  - *Quick quiz*: 10 random questions mixed from the lessons you've passed
  - *Review my mistakes*: every question you've got wrong, from all lessons, practice and exams,
    in one go; each one stays until you get it right first time
  - *Exam mode*: like the coding exams: 15 questions and one coding task in 25 minutes,
    all marked at the end. The coding task is a random exercise from a lesson you've passed;
    you can press Run to see compile errors and output, and when you finish, its hidden tests
    mark it out of 5 (a share for the checks that pass). Then you can step through your answers,
    your code and the model answer. Your best score is saved, and 90%+ earns Exam Ace.
  - In Quick quiz and Review, each question links back to its lesson's cards, and the results
    list the topics you got wrong.
- **Code tab**: write and run real C and C++ in a built-in editor (line numbers, coloured
  keywords, auto-indent, copy and paste):
  - *Exercises*: 18 exam-style tasks, at least one per lesson, such as `isPrime`, `rotateLeft`,
    `InsertAtEnd` and a `BankAccount` class. Your code is joined to hidden test code, compiled
    with gcc (or g++), run, and each line of output is checked. You see which checks passed,
    what was expected and what you got, or the compiler's errors. There's a hint, the model
    answer after two tries, and a link back to the lesson. Each one solved is worth 30 points.
  - *Playground*: write and run any C or C++ program and see what it prints.
  - Programs are stopped after 3 seconds or if they print too much, so an endless loop
    can't freeze the app, and a crash is reported rather than closing anything.
- **My progress** (click your points): lessons passed, exercises solved, best exam score,
  mistakes to review and best streak; a chart of your last 15 scores; and your weakest
  topics (lowest first-try accuracy), each with a link to go over that lesson.
- **Glossary**: about 50 key terms, each with a meaning, an example and its lesson. Search it
  from the path or My progress, and each lesson card shows its key words as buttons.
- **Teach tab: help the duck**: Bugsy the duck brings broken programs along with his own
  (wrong) guess about what's broken. Click the line that's really wrong, then pick the right fix.
  Each problem opens once you've passed its lesson. Each fix earns 20 points the first
  time, and fixing them all earns Duck Helper.

All 12 lessons are written, with 76 questions in total:

- **Beginner** (Intro to C notes): First programs, Variables and maths, Decisions and loops, Arrays and strings
- **Intermediate** (Lab 1): Functions, Number problems, Array problems, Patterns and grids
- **Advanced** (Coding Exam 2 notes): Pointers, Memory management, Linked lists, Classes

The Advanced lessons use C++ for `new`, `delete`, `nullptr` and classes.

## Files

| File | What it does |
| --- | --- |
| `app.h` | Shared types: Course, Lesson, Card, Question, Learner, Badge |
| `course.c` | Loads `content/course.txt` into linked lists of cards and questions |
| `learner.c` | Points, unlocking, badges, saving and loading learners |
| `duck.c` | Loads the duck's problems from `content/duck.txt`; points and the Duck Helper badge |
| `textbuf.c` | The text behind the code editor: typing, new lines with auto-indent, backspace, paste |
| `runner.c` | Compiles the learner's code with gcc and runs it with a time and output limit |
| `exercises.c` | Loads the coding exercises, joins code to the hidden tests, checks the output |
| `history.c` | Every finished lesson, quiz and exam, kept in a linked list and `history.txt` |
| `glossary.c` | Loads the glossary, searches it, finds key words in a card's text |
| `practice.c` | Picking and shuffling practice questions, tracking mistakes, exam scores |
| `ui.c`, `ui.h` | Fonts, colours, buttons, wrapped text and code panels |
| `main.c` | The screens: sign in, path, cards, questions, results, Practise, Code, Teach, progress, glossary, badges |
| `test_app.c` | Console tests for everything except drawing; `test_app code` really compiles the exercises |
| `content/course.txt` | All the lesson content (plain text, easy to add to) |
| `content/exercises.txt` | The coding exercises: task, starter code, hidden tests, expected output, answer |
| `content/glossary.txt` | The glossary terms |
| `content/duck.txt` | The duck's broken programs (format at the top of `duck.c`) |
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
