OPT = -g3 -O0
LIB_SOURCES =  ../iron/mem.c ../iron/process.c ../iron/array.c ../iron/math.c ../iron/time.c ../iron/log.c ../iron/fileio.c ../iron/linmath.c main.c
CC = gcc
TARGET = main
LIB_OBJECTS =$(LIB_SOURCES:.c=.o)
LDFLAGS= -L. $(OPT) -Wextra #-lmcheck #-ftlo #setrlimit on linux 
LIBS= -ldl -lm -lSDL2
ALL= $(TARGET) main
CFLAGS = -I.. -std=c11 -c $(OPT) -Wall -Wextra -Werror=implicit-function-declaration -Wformat=0 -D_GNU_SOURCE -fdiagnostics-color -Wextra -Werror -Wwrite-strings -fbounds-check  -ffast-math -msse4.1 #-DDEBUG

$(TARGET): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) $(LIB_OBJECTS) $(LIBS) -o $@

all: $(ALL)

.c.o: $(HEADERS)
	$(CC) $(CFLAGS) -fPIC $< -o $@ -MMD -MF $@.depends 
depend: h-depend
clean:
	rm -f $(LIB_OBJECTS) $(ALL) *.o.depends

-include $(LIB_OBJECTS:.o=.o.depends)

#main: $(TARGET) main.o
#	$(CC) $(LDFLAGS) $(LIBS) main.o -ludpc -Wl,-rpath,. -o main
