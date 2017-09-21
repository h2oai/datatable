
#-------------------------------------------------------------------------------

CC = clang++
INCLUDES ?= -I..
CCFLAGS += -std=gnu++11

ifdef writer
	WRITER := writer=$(writer)
endif

ifdef n
	SIZE := n=$(n)
endif

ifdef batches
	BATCHES := batches=$(batches)
endif


#-------------------------------------------------------------------------------

build: writecsv

main.o: main.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

boolean.o: boolean.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

int8.o: int8.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

int16.o: int16.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

int32.o: int32.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

double.o: double.cpp dtoa_milo.h
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

string.o: string.cpp
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

writecsv: main.o boolean.o int8.o int16.o int32.o double.o string.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f *.o writecsv

run: build
	./writecsv $(WRITER) $(SIZE) $(BATCHES)
