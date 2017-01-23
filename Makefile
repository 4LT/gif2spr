GIFLIB=giflib-5.0.5
CFLAGS ::= -std=c99 -Wall -pedantic -ggdb
LOCAL_OBJECTS= main.o sprite.o
GIFLIB_OBJECTS= $(GIFLIB)/lib/dgif_lib.o $(GIFLIB)/lib/gif_err.o \
	$(GIFLIB)/lib/gif_hash.o $(GIFLIB)/lib/gifalloc.o
OBJECTS ::= $(LOCAL_OBJECTS)
LD_FLAGS ::= -lm

ifeq ($(findstring CYGWIN, $(shell uname)), CYGWIN)
	COMPILE_GIFLIB=true
	CC=i686-w64-mingw32-gcc
else
	CC=cc
endif

ifdef COMPILE_GIFLIB
	CFLAGS  ::= $(CFLAGS) -DCOMPILE_GIFLIB
	OBJECTS ::= $(OBJECTS) $(GIFLIB_OBJECTS)
else
	LD_FLAGS ::= $(LD_FLAGS) -lgif
endif

.PHONY=all clean

all: gif2spr

$(GIFLIB_OBJECTS): $(GIFLIB)/Makefile
	make -C $(GIFLIB) -f Makefile

$(GIFLIB)/Makefile:
	cd $(GIFLIB); \
	./configure; \
	sed 's/^\(CC =\) gcc/\1 $(CC)/' -i Makefile; \
	cd lib; \
	sed 's/^\(CC =\) gcc/\1 $(CC)/' -i Makefile

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

sprite.o: sprite.c
	$(CC) $(CFLAGS) -c sprite.c

gif2spr: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o gif2spr $(OBJECTS)

clean:
	rm -f $(LOCAL_OBJECTS)
	rm -f gif2spr
	make -C $(GIFLIB) -f Makefile distclean
