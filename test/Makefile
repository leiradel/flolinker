CFLAGS=-m64 -O0 -g -I. -I..
LFLAGS=-m64 -g

all: runflo.exe test.flo

runflo.exe: main.o floload.o
	gcc $(LFLAGS) -o $@ $+

main.o: main.c
	gcc $(CFLAGS) -o $@ -c $<

floload.o: floload.c
	gcc $(CFLAGS) -o $@ -c $<

test.flo: test.o
	../flolink.exe -v -o $@ $<

test.o: test.c
	gcc $(CFLAGS) -mcmodel=small -o $@ -c $<

clean:
	rm -f runflo.exe main.o floload.o test.flo test.o
