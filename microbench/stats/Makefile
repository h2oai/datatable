
#-------------------------------------------------------------------------------

CC = clang++
INCLUDES ?= -I..
LIBRARIES ?=
CCFLAGS ?= -std=gnu++11 
LDFLAGS ?= 

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


#-------------------------------------------------------------------------------

build: stats

main.out: main.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

macro_stats.out: macro_stats.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

class_stats.out: class_stats.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<
stats: main.out macro_stats.out class_stats.out
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f *.out stats

.PHONY: debug
debug:
	$(MAKE) clean
	CCFLAGS=-ggdb LDFLAGS=-ggdb \
	$(MAKE) build
	gdb ./stats


run: build
	./stats $(ALGO) $(SIZE) $(SIGBITS) $(BATCHES)

munch: build
	python ../munch.py ./stats n={N} $(ALGO) $(SIGBITS) $(BATCHES)
