GIFLIB=giflib-5.1.4
CFLAGS := -std=c99 -Wall -pedantic -ggdb
LOCAL_OBJECTS= main.o sprite.o
GIFLIB_OBJECTS= $(GIFLIB)/lib/dgif_lib.o $(GIFLIB)/lib/gif_err.o \
	$(GIFLIB)/lib/gif_hash.o $(GIFLIB)/lib/gifalloc.o \
	$(GIFLIB)/lib/openbsd-reallocarray.o
OBJECTS := $(LOCAL_OBJECTS)

ifeq ($(findstring CYGWIN, $(shell uname)), CYGWIN)
	COMPILE_GIFLIB=true
	OUTPUT=gif2spr.exe
	CC=i686-w64-mingw32-gcc
else
	OUTPUT=gif2spr
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

.PHONY: all clean clean-giflib package

all: $(OUTPUT)

$(GIFLIB_OBJECTS): $(GIFLIB)/Makefile
	$(MAKE) -C $(GIFLIB) SUBDIRS=lib

$(GIFLIB)/Makefile:
	cd $(GIFLIB) && \
	CC=$(CC) ./configure

defpal.h: defpal.h_head defpal.h_foot quakepal
	xxd -i quakepal \
	| sed 's/\(^.*\)quakepal/static \1 const DEFPAL/' \
	| sed 's/^unsigned\ int.*//' \
	| cat defpal.h_head - defpal.h_foot \
	> defpal.h

main.o: main.c defpal.h sprite.h
	$(CC) $(CFLAGS) -c main.c

sprite.o: sprite.c sprite.h
	$(CC) $(CFLAGS) -c sprite.c

$(OUTPUT): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(OUTPUT) $(OBJECTS)

package: gif2spr.zip

gif2spr.zip: COPYING README.md $(OUTPUT)
	zip gif2spr.zip COPYING README.md $(OUTPUT)

clean-giflib:
	make -C $(GIFLIB) -f Makefile distclean

clean:
	rm -f $(LOCAL_OBJECTS)
	rm -f $(OUTPUT)
	rm -f defpal.h
	rm -f gif2spr.zip
