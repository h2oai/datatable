
#-------------------------------------------------------------------------------

CC ?= clang
INCLUDES ?= -I..

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

main.o: main.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

boolean.o: boolean.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

int8.o: int8.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

int16.o: int16.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

int32.o: int32.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

writecsv: main.o boolean.o int8.o int16.o int32.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f *.o writecsv

run: build
	./writecsv $(WRITER) $(SIZE) $(BATCHES)
