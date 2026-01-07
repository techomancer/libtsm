# Makefile for dash - Modern IRIX Terminal
# Builds on IRIX with MIPSpro 7.4.4

ROOT = ..
include $(ROOT)/common.mk

# Application name
TARGET = dash

# IRIX Executable Tag
TAG = 0x0001D450

# Object files
OBJS = dash.o dash-night.o dash-sunset.o dash-ocean.o dash-aurora.o dash-embers.o dash-static.o dash-themes.o

# Libraries
LIBS = ../src/tsm/libtsm.a \
       ../src/shared/libshl.a \
       ../external/wcwidth/libwcwidth.a

# Motif and OpenGL libraries (order matters: dependencies last)
MOTIF_LIBS = -lGLw -lXm -lXt -lX11 -lGL -lm

# Default target
all: $(TARGET) sigwinch_test termios_test

$(TARGET): $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS) $(MOTIF_LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

sigwinch_test: sigwinch_test.c
	$(CC) $(CFLAGS) -o $@ sigwinch_test.c

termios_test: termios_test.c
	$(CC) $(CFLAGS) -o $@ termios_test.c

clean:
	rm -f $(OBJS) $(TARGET) sigwinch_test termios_test

install: $(TARGET)
	mkdir -p /usr/bin
	cp $(TARGET) /usr/bin/
	/usr/sbin/tag $(TAG) /usr/bin/$(TARGET)
	mkdir -p /usr/share/man/man1
	cp dash.1 /usr/share/man/man1/
	tic dash.ti
	mkdir -p /usr/lib/filetype/local
	cp dash.ftr /usr/lib/filetype/local/dash.ftr
	mkdir -p /usr/lib/X11/app-chests
	cp dash.chest /usr/lib/X11/app-chests/dash.chest

uninstall:
	rm -f /usr/bin/$(TARGET)
	rm -f /usr/share/man/man1/dash.1
	rm -f /usr/share/lib/terminfo/d/dash
	rm -f /usr/share/lib/terminfo/d/dash-256color
	rm -f /usr/lib/filetype/local/dash.ftr
	rm -f /usr/lib/X11/app-chests/dash.chest

dist: $(TARGET)
	rm -rf dist images
	mkdir -p dist/usr/bin
	mkdir -p dist/usr/share/lib/terminfo/d
	mkdir -p dist/usr/share/man/man1
	mkdir -p dist/usr/lib/filetype/local
	mkdir -p dist/usr/lib/X11/app-chests
	cp $(TARGET) dist/usr/bin/
	strip dist/usr/bin/$(TARGET)
	/usr/sbin/tag $(TAG) dist/usr/bin/$(TARGET)
	TERMINFO=dist/usr/share/lib/terminfo tic dash.ti
	cp dash.1 dist/usr/share/man/man1/dash.1
	cp dash.ftr dist/usr/lib/filetype/local/
	cp dash.chest dist/usr/lib/X11/app-chests/
	cp dash.idb dist/dash.idb
	sort -k 5,6 dist/dash.idb -o dist/dash.idb
	mkdir images
	gendist -verbose -sbase dist -idb dist/dash.idb -spec dash.spec -dist images
	cd images && tar cvf ../dash.tardist *

# Dependencies
dash.o: dash.c dash.h
dash-night.o: dash-night.c dash.h
dash-sunset.o: dash-sunset.c dash.h
dash-ocean.o: dash-ocean.c dash.h
dash-aurora.o: dash-aurora.c dash.h
dash-embers.o: dash-embers.c dash.h
dash-static.o: dash-static.c dash.h
dash-themes.o: dash-themes.c dash.h
