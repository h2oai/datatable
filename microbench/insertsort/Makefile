
#-------------------------------------------------------------------------------

CC ?= clang
INCLUDES := -I..
LIBRARIES :=
CCFLAGS :=
LDFLAGS :=

ifdef iters
	ITERS := iters=$(iters)
endif

ifdef size
	SIZE := size=$(size)
endif

ifdef batches
	BATCHES := batches=$(batches)
endif


#-------------------------------------------------------------------------------

build: isort

isort.o: isort.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

isort: isort.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f isort.o isort

run: build
	./isort $(ITERS) $(SIZE) $(BATCHES)

munch: build
	python ../munch.py ./isort size={N} $(ITERS) $(BATCHES)
