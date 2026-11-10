# Windows: run from the w64devkit terminal
# Raylib expected at C:\raylib (include\raylib.h and lib\libraylib.a)
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -IC:/raylib/include
LIBS = -LC:/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm
LOGIC = course.c learner.c

cquest.exe: main.c ui.c ui.h $(LOGIC) app.h
	$(CC) $(CFLAGS) main.c ui.c $(LOGIC) -o cquest.exe $(LIBS)

# Console tests of the course loader and progress (no Raylib needed)
test: test_app.c $(LOGIC) app.h
	$(CC) -std=c99 -Wall -Wextra test_app.c $(LOGIC) -o test_app.exe
	./test_app.exe

clean:
	rm -f cquest.exe test_app.exe
