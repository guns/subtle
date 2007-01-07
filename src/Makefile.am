## Process this file with automake to produce Makefile.in

bin_PROGRAMS = subtle

subtle_SOURCES = \
	client.c \
	display.c \
	event.c \
	log.c \
	lua.c \
	main.c \
	screen.c \
	sublet.c \
	tile.c \
	win.c 

subtle_CFLAGS  = $(CFLAGS) $(LUA_CFLAGS)
#subtle_LDFLAGS = $(LUA_LIBS)
subtle_LDADD = $(LUA_LIBS)

INCLUDES = -I$(top_srcdir)

noinst_HEADERS = subtle.h

AM_CFLAGS = -Wall -Wpointer-arith -Wstrict-prototypes -Wunused -O2 -g
