
CC = clang++
INCLUDES ?= -I..
CCFLAGS += -std=c++11 -O3
LDFLAGS += -lc++


build: groupby

groupby: groupby.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

groupby.o: groupby.cc
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -f *.o groupby

debug:
	$(MAKE) clean
	CCFLAGS=-ggdb LDFLAGS=-ggdb \
	$(MAKE) build
