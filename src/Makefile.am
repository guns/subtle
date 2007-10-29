## Process this file with automake to produce Makefile.in

bin_PROGRAMS = subtle subtler

subtle_SOURCES = \
	client.c \
	display.c \
	event.c \
	ewmh.c \
	key.c \
	lua.c \
	main.c \
	sublet.c \
	tile.c \
	util.c \
	view.c \
	win.c 

subtler_SOURCES = subtler.c

subtle_CFLAGS  = $(CFLAGS) $(LUA_CFLAGS)
subtle_LDADD = $(LUA_LIBS)

subtler_CFLAGS = $(CFLAGS)

INCLUDES = -I$(top_srcdir)

noinst_HEADERS = subtle.h

AM_CFLAGS = -Wall -W -pedantic -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -O2 $(DEBUG_CFLAGS)
