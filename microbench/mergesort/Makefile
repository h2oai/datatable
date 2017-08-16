
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

build: msort

msort.o: msort.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

msort: msort.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f msort.o msort

run: build
	./msort $(ITERS) $(SIZE) $(BATCHES)

munch: build
	python ../munch.py ./msort size={N} $(ITERS) $(BATCHES)
