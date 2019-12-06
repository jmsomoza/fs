CC      = g++
CFLAGS  = -Wall -O2 
SOURCES = $(wildcard *.cc) $(wildcard *.h)
OBJECTS = $(SOURCES:%.cc=%.o)
TARGET = fs

.PHONY: all clean

all: fs

clean:
	rm *.o

compile: $(SOURCES)
	${CC} ${CFLAGS} -c $< -o $@ -g

fs: $(OBJECTS)
	$(CC) -o fs $(OBJECTS)

compress: 
	tar -zcvf fs-sim.tar.gz $(SOURCES) Makefile 

