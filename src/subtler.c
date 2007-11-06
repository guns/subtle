
 /**
	* subtler - window manager remote
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <regex.h>
#include <getopt.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#define ACTION_CLIENTS	(1L << 1)		// List clients
#define ACTION_VIEWS		(1L << 2)		// List views
#define ACTION_SWITCH		(1L << 3)		// Switch view

static Display *disp = NULL;

#ifdef DEBUG
static short debug = 0;
#define Debug(...) Log(0, __FILE__, __LINE__, __VA_ARGS__);
#else
#define Debug(...)
#endif /* DEBUG */

#define Error(...) Log(1, __FILE__, __LINE__, __VA_ARGS__);
#define Warn(...) Log(2, __FILE__, __LINE__, __VA_ARGS__);

void
Log(short type,
	const char *file,
	short line,
	const char *format,
	...)
{
	va_list ap;
	char buf[255];

#ifdef DEBUG
	if(!debug) return;
#endif /* DEBUG */

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	switch(type)
		{
#ifdef DEBUG
			case 0: fprintf(stderr, "<DEBUG:%s:%d> %s", file, line, buf);	break;
#endif /* DEBUG */
			case 1: fprintf(stderr, "<ERROR> %s", buf); raise(SIGTERM);		break;
			case 2: fprintf(stdout, "<WARNING> %s", buf);									break;
		}
}

static void
Usage(void)
{
	printf("Usage: %s [OPTIONS]\n\n" \
					"Options:\n" \
					"  -d, --display=DISPLAY   \t Connect to DISPLAY (default: $DISPLAY)\n" \
					"  -c, --clients           \t List all clients managed by subtle\n" \
					"  -v, --views             \t List all views managed by subtle\n" \
					"  -s, --switch=VIEW       \t Switch to view\n" \
					"  -f, --find=CLIENT       \t Find client\n" \
					"  -A, --active=CLIENT     \t Give focus to client\n" \
					"  -S, --shade=CLIENT      \t Toggle shade of a client\n" \
					"  -F, --float=CLIENT      \t Toggle float of a client\n" \
					"  -D, --debug             \t Print debugging messages\n" \
					"  -V, --version           \t Show version info and exit\n" \
					"  -h, --help              \t Show this help and exit\n\n" \
					"Please report bugs to <%s>\n", 
					PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME, PACKAGE_BUGREPORT);
}

static void
Version(void)
{
	printf("%sr %s - Copyright (c) 2005-2007 Christoph Kappel\n" \
					"Released under the GNU General Public License\n" \
					"Compiled for X%d\n", PACKAGE_NAME, PACKAGE_VERSION, X_PROTOCOL);
}

static void
HandleSignal(int signum)
{
	switch(signum)
		{
			case SIGTERM:
			case SIGINT: 
				exit(1);
			case SIGSEGV: 
				printf("Please report this bug to <%s>\n", PACKAGE_BUGREPORT);
				abort();
		}
}

static char *
GetProperty(Window win,
	Atom type,
	char *name,
	unsigned long *size)
{
	unsigned long nitems, bytes;
	unsigned char *data = NULL;
	int format;
	Atom rtype, prop = XInternAtom(disp, name, False);

	if(XGetWindowProperty(disp, win, prop, 0L, 1024, False, type, &rtype, 
		&format, &nitems, &bytes, &data) != Success)
		{
			printf("Failed to get property (%s)\n", name);
			return(NULL);
		}
	if(type != rtype)
		{
			printf("Invalid type for property (%s)\n", name);
			XFree(data);
			return(NULL);
		}
	if(size) *size = (format / 8) * nitems;

	return(data);
}

static Window *
GetClients(unsigned long *size)
{
	Window *clients = (Window *)GetProperty(DefaultRootWindow(disp), XA_WINDOW, "_NET_CLIENT_LIST", &size);
	if(list)
		{
			*size = *size / sizeof(Window);
			return(clients);
		}
	else
		{
			*size = 0;
			return(NULL);
		}
}

static char *
GetName(Window win)
{
	char *name = GetProperty(win, XA_STRING, "WM_NAME", NULL);
	return(name ? name : NULL);
}

static char *
GetClass(Window win)
{
	char *class = GetProperty(win, XA_STRING, "WM_CLASS", NULL);
	return(class ? class : NULL);
}

static void
SendMessage(Window win,
	char *prop,
	unsigned long data0,
	unsigned long data1,
	unsigned long data2,
	unsigned long data3,
	unsigned long data4)
{
	XEvent ev;
	long mask = SubstructureRedirectMask|SubstructureNotifyMask;

	ev.xclient.type					= ClientMessage;
	ev.xclient.serial				= 0;
	ev.xclient.send_event		= True;
	ev.xclient.message_type	= XInternAtom(disp, prop, False);
	ev.xclient.window				= win;
	ev.xclient.format				= 32;
	ev.xclient.data.l[0]		= data0;
	ev.xclient.data.l[1]		= data1;
	ev.xclient.data.l[2]		= data2;
	ev.xclient.data.l[3]		= data3;
	ev.xclient.data.l[4]		= data4;

	if(!XSendEvent(disp, DefaultRootWindow(disp), False, mask, &ev))
		printf("Can't send client message %s\n", prop);
	Debug("Send: prop=%s, data=%ld %ld %ld %ld %ld\n", prop, data0, data1, data2, data3, data4);
}

static void
ActionGetClients(void)
{
	unsigned long size = 0;
	Window *clients = GetClients(&size);

	if(list)
		{
			int i;

			for(i = 0; i < size; i++)
				{
					char *name = GetName(list[i]);
					char *class = GetClass(list[i]);

					printf("%#lx %s %s\n", list[i], name, class);

					if(name) free(name);
					if(class) free(class);
				}
			XFree(list);
		}
	else
		{
			printf("Failed to get client list\n");
		}
}

static void
ActionSetFocus(char name)
{
	unsigned long size = 0;
	Window *list = NULL;

	if((list = (Window *)GetProperty(DefaultRootWindow(disp), XA_WINDOW, "_NET_CLIENT_LIST", &size)))
		{
			int i;

			for(i = 0; i < size / sizeof(Window); i++)
				{
				}
		}

}

static void
GetViews(void)
{
}

static void
ActionSetView(int view)
{
	assert(view >= 0);
	SendMessage(DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", (unsigned long)view, 0, 0, 0, 0);
	Debug("Set: view=%d\n", view);
}
	
int
main(int argc,
	char *argv[])
{
	int c, action = 0, number = 0;
	char *display = NULL, string[256];
	struct sigaction act;
	static struct option long_options[] =
	{
		{ "display",		required_argument,		0,	'd' },
		{ "clients",		no_argument,					0,	'c' },
		{ "views",			no_argument,					0,	'v' },
		{ "switch",			required_argument,		0,	's' },
		{ "shade",			required_argument,		0,	'S' },
		{ "float",			required_argument,		0,	'F' },
#ifdef DEBUG
		{ "debug",			no_argument,					0,	'D' },
#endif /* DEBUG */
		{ "version",		no_argument,					0,	'V' },
		{ "help",				no_argument,					0,	'h' },
		{ 0, 0, 0, 0}
	};

	while((c = getopt_long(argc, argv, "d:cvs:S:DVh", long_options, NULL)) != -1)
		{
			if(optarg)
				{
					/* Pipe or argument */
					if(!strncmp(optarg, "-", 1)) 
						{
							if(!fgets(string, sizeof(string), stdin)) 
								Error("Can't read from pipe\n");
							Debug("Read: string=%s", string);
						}
					else snprintf(string, sizeof(string), "%s", optarg);

					/* Convert string to int if possible */
					if(isdigit(string)) number = atoi(string);
				}

			switch(c)
				{
					case 'd': display	= optarg;					break;
					case 'c':	action	= ACTION_CLIENTS;	break;
					case 'v':	action	= ACTION_VIEWS;		break;
					case 's': action	= ACTION_SWITCH;	break;
#ifdef DEBUG					
					case 'D': debug = 1;								break;
#endif /* DEBUG */
					case 'V': Version(); 								return(0);
					case 'h': Usage(); 									return(0);
					case '?':
						printf("Try `%sr --help for more information\n", PACKAGE_NAME);
						return(-1);
				}
		}
	if(!action)
		{
			Usage();
			return(0);
		}

	act.sa_handler	= HandleSignal;
	act.sa_flags		= 0;
	memset(&act.sa_mask, 0, sizeof(sigset_t)); /* Avoid uninitialized values */
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);

	if(!(disp = XOpenDisplay(display)))
		{
			printf("Can't open display `%s'.\n", (display) ? display : ":0.0");
			return(-1);
		}

	switch(action)
		{
			case ACTION_CLIENTS:	ActionGetClients();			break;
			case ACTION_VIEWS:		ActionGetViews();				break;
			case ACTION_SWITCH:		ActionSetView(number);	break;
		}

	XCloseDisplay(disp);
	
	return(0);
}
