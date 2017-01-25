GIFLIB=giflib-5.1.4
CFLAGS := -std=c99 -Wall -pedantic -ggdb
LOCAL_OBJECTS= main.o sprite.o
GIFLIB_OBJECTS= $(GIFLIB)/lib/dgif_lib.o $(GIFLIB)/lib/gif_err.o \
	$(GIFLIB)/lib/gif_hash.o $(GIFLIB)/lib/gifalloc.o \
	$(GIFLIB)/lib/openbsd-reallocarray.o
OBJECTS := $(LOCAL_OBJECTS)

ifeq ($(findstring CYGWIN, $(shell uname)), CYGWIN)
	COMPILE_GIFLIB=true
	CC=i686-w64-mingw32-gcc
else
	CC=cc
endif

ifeq ($(shell uname), Darwin)
	COMPILE_GIFLIB=true
else
	LDFLAGS := $(LD_FLAGS) -lm
endif

ifdef COMPILE_GIFLIB
	CFLAGS  := $(CFLAGS) -DCOMPILE_GIFLIB
	OBJECTS := $(OBJECTS) $(GIFLIB_OBJECTS)
else
	LDFLAGS := $(LDFLAGS) -lgif
endif

.PHONY=all clean

all: gif2spr

$(GIFLIB_OBJECTS): $(GIFLIB)/Makefile
	make -C $(GIFLIB) -f Makefile

$(GIFLIB)/Makefile:
	cd $(GIFLIB) && \
	CC=$(CC) ./configure

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

sprite.o: sprite.c
	$(CC) $(CFLAGS) -c sprite.c

gif2spr: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o gif2spr $(OBJECTS)

clean-giflib:
	make -C $(GIFLIB) -f Makefile distclean

clean:
	rm -f $(LOCAL_OBJECTS)
	rm -f gif2spr
