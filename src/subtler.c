
 /**
	* subtler - window manager remote
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <regex.h>
#include <getopt.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#define ACTION_CLIENTS	(1L << 1)		// List clients
#define ACTION_VIEWS		(1L << 2)		// List views

static Display *disp = NULL;

#ifdef DEBUG
static short debug = 0;
#endif /* DEBUG */

static void
Usage(void)
{
	printf("Usage: %s [OPTIONS]\n\n" \
					"Options:\n" \
					"  -d, --display   DISPLAY \t Connect to DISPLAY (default: $DISPLAY)\n" \
					"  -c, --clients           \t List all clients managed by subtle\n" \
					"  -v, --views             \t List all views managed by subtle\n" \
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

static char *
GetClientName(Window win)
{
	char *name = GetProperty(win, XA_STRING, "WM_NAME", NULL);
	return(name ? name : NULL);
}

static char *
GetClientClass(Window win)
{
	char *class = GetProperty(win, XA_STRING, "WM_CLASS", NULL);
	return(class ? class : NULL);
}

static void
GetClientList(void)
{
	unsigned long size = 0;
	Window *list = NULL;

	if((list = (Window *)GetProperty(DefaultRootWindow(disp), XA_WINDOW, "_NET_CLIENT_LIST", &size)))
		{
			int i;

			for(i = 0; i < size / sizeof(Window); i++)
				{
					char *name = GetClientName(list[i]);
					char *class = GetClientClass(list[i]);

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
GetViewList(void)
{
}

int
main(int argc,
	char *argv[])
{
	int c, action = 0;
	char *display_string = NULL;
	struct sigaction act;
	static struct option long_options[] =
	{
		{ "display",		required_argument,		0,	'd' },
		{ "clients",		no_argument,					0,	'c' },
		{ "views",			no_argument,					0,	'v' },
#ifdef DEBUG
		{ "debug",			no_argument,					0,	'D' },
#endif /* DEBUG */
		{ "version",		no_argument,					0,	'V' },
		{ "help",				no_argument,					0,	'h' },
		{ 0, 0, 0, 0}
	};

	while((c = getopt_long(argc, argv, "d:cvDVh", long_options, NULL)) != -1)
		{
			switch(c)
				{
					case 'd': display_string = optarg;	break;
					case 'c':	action = ACTION_CLIENTS;	break;
					case 'v':	action = ACTION_VIEWS;		break;
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

	if(!(disp = XOpenDisplay(display_string)))
		{
			printf("Can't open display `%s'.\n", (display_string) ? display_string : ":0.0");
			return(-1);
		}

	switch(action)
		{
			case ACTION_CLIENTS:	GetClientList();	break;
			case ACTION_VIEWS:		GetViewList();		break;
		}
	
	return(0);
}
