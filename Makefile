CC=cc
CFLAGS=-std=c99 -Wall -pedantic -ggdb
LDFLAGS=-lm -lgif
OBJECTS=main.o sprite.o

all: gif2spr

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

sprite.o: sprite.c
	$(CC) $(CFLAGS) -c sprite.c

gif2spr: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o gif2spr $(OBJECTS)

clean:
	rm -f $(OBJECTS)
	rm -f gif2spr
