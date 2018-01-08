
#-------------------------------------------------------------------------------

CC = ${LLVM4}/bin/clang++
INCLUDES ?= -I..
CCFLAGS += -std=gnu++11 -stdlib=libc++ -fopenmp
LDFLAGS += -fopenmp -L${LLVM4}/lib -Wl,-rpath,${LLVM4}/lib

ifdef parser
	PARSER := parser=$(parser)
endif

ifdef n
	SIZE := n=$(n)
endif

ifdef batches
	BATCHES := batches=$(batches)
endif


#-------------------------------------------------------------------------------

build: fread

main.o: main.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

int32.o: int32.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

fread: main.o int32.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f *.o fread

debug: CC := $(CC) -ggdb -O0
debug: clean
debug: fread

run: build
	./fread $(PARSER) $(SIZE) $(BATCHES)
