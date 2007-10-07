## Process this file with automake to produce Makefile.in

bin_PROGRAMS = subtle

subtle_SOURCES = \
	client.c \
	display.c \
	event.c \
	ewmh.c \
	key.c \
	lua.c \
	main.c \
	rule.c \
	sublet.c \
	tile.c \
	util.c \
	view.c \
	win.c 

subtle_CFLAGS  = $(CFLAGS) $(LUA_CFLAGS)
subtle_LDADD = $(LUA_LIBS)

INCLUDES = -I$(top_srcdir)

noinst_HEADERS = subtle.h

AM_CFLAGS = -Wall -W -pedantic -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -O2 $(DEBUG_CFLAGS)
