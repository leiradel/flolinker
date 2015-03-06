FILE2C=../../etc/file2c.exe
CFLAGS=-m32 -O0 -g -I.
LFLAGS=-m32 -g

all: flolink.exe

flolink.exe: luacoff.o main.o
	gcc $(LFLAGS) -o $@ $+ -llua

luacoff.o: luacoff.c coff.h
	gcc $(CFLAGS) -o $@ -c $<

main.o: main.c main_lua.h
	gcc $(CFLAGS) -o $@ -c $<

main_lua.h: main.lua
	xxd -i $< | sed "s/unsigned/const/g" > $@

clean:
	rm -f flolink.exe luacoff.o main.o main_lua.h
