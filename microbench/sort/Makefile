
#-------------------------------------------------------------------------------

CC ?= clang
INCLUDES := -I..
LIBRARIES :=
CCFLAGS :=
LDFLAGS :=

ifdef algo
	ALGO := algo=$(algo)
endif

ifdef n
	SIZE := n=$(n)
endif

ifdef k
	SIGBITS := k=$(k)
endif

ifdef batches
	BATCHES := batches=$(batches)
endif

ifdef time
	TIME := time=$(time)
endif


#-------------------------------------------------------------------------------

build: sort

main.o: main.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

isort.o: isort.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

msort.o: msort.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

rsort.o: rsort.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

sort: isort.o msort.o rsort.o main.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f *.o sort

run: build
	./sort $(ALGO) $(SIZE) $(SIGBITS) $(BATCHES) $(TIME)

munch: build
	python ../munch.py ./sort n={N} $(ALGO) $(SIGBITS) $(BATCHES) $(TIME)
