
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

build: rsort

rsort.o: rsort.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

rsort: rsort.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f rsort.o rsort

run: build
	./rsort $(ITERS) $(SIZE) $(BATCHES)

munch: build
	python ../munch.py ./rsort size={N} $(ITERS) $(BATCHES)
