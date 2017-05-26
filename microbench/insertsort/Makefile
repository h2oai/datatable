
#-------------------------------------------------------------------------------

CC ?= clang
INCLUDES := -I..
LIBRARIES :=
CCFLAGS :=
LDFLAGS :=

#-------------------------------------------------------------------------------

build: isort

isort.o: isort.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

isort: isort.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f isort.o isort

run: build
	./isort
