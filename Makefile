GIFLIB=giflib-5.1.9
CFLAGS := -std=c99 -Wall -pedantic -ggdb
LOCAL_OBJECTS= main.o sprite.o
#GIFLIB_OBJECTS= $(GIFLIB)/dgif_lib.o $(GIFLIB)/gif_err.o \
#	$(GIFLIB)/gif_hash.o $(GIFLIB)/gifalloc.o \
#	$(GIFLIB)/openbsd-reallocarray.o
GIFLIB_A=$(GIFLIB)/libgif.a
OBJECTS := $(LOCAL_OBJECTS)
TARGET_PLAT = $(shell uname)
HOST_PLAT = $(shell uname)
SDX=sdx.kit

ifeq ($(findstring CYGWIN,$(TARGET_PLAT)), CYGWIN)
	WIN=1
else ifeq ($(findstring win32,$(TARGET_PLAT)), win32)
	WIN=1
else ifeq ($(findstring MINGW,$(TARGET_PLAT)), MINGW)
	WIN=1
else
	WIN=0
endif

ifeq ($(WIN),1)
	COMPILE_GIFLIB=true
	OUTPUT=gif2spr.exe
	CC=x86_64-w64-mingw32-gcc
	LD=x86_64-w64-mingw32-ld
	AR=x86_64-w64-mingw32-ar
else
	OUTPUT=gif2spr
	CC=cc
	LD=ld
	AR=ar
endif

ifeq ($(shell uname), Darwin)
	COMPILE_GIFLIB=true
else
	LDFLAGS := $(LD_FLAGS) -lm
endif

ifdef COMPILE_GIFLIB
	CFLAGS  := $(CFLAGS) -DCOMPILE_GIFLIB
	OBJECTS := $(OBJECTS) $(GIFLIB_A)
else
	LDFLAGS := $(LDFLAGS) -lgif
endif

.PHONY: all clean clean-giflib win-package win-gui

all: $(OUTPUT)

$(GIFLIB_A): $(GIFLIB)/Makefile
	$(MAKE) -C $(GIFLIB) CC=$(CC) LD=$(LD) AR=$(AR) libgif.a

main.o: main.c quakepal.h sprite.h
	$(CC) $(CFLAGS) -c main.c

sprite.o: sprite.c sprite.h
	$(CC) $(CFLAGS) -c sprite.c

$(OUTPUT): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(OUTPUT) $(OBJECTS)

win-package: gif2spr.zip

gif2spr.zip: COPYING README.md gif2spr.exe gif2spr-gui.exe
	zip gif2spr.zip COPYING README.md gif2spr.exe gif2spr-gui.exe

win-gui: gif2spr-gui.exe

gif2spr-gui.exe: gif2spr.exe
	$(SDX) qwrap gif2spr-gui.tcl gif2spr-gui -runtime tclkit-gui.exe &&\
		mv gif2spr-gui gif2spr-gui.exe

clean-giflib:
	make -C $(GIFLIB) -f Makefile clean

clean: clean-giflib
	rm -f $(LOCAL_OBJECTS)
	rm -f gif2spr
	rm -f gif2spr.exe
	rm -f gif2spr.zip
	rm -f gif2spr-gui.exe
