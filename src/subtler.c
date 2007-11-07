
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

#define ACTION_ACTIVATE	(1L << 1)		// Activate client
#define ACTION_CLIENTS	(1L << 2)		// List clients
#define ACTION_FIND			(1L << 3)		// Find client
#define ACTION_VIEWS		(1L << 4)		// List views
#define ACTION_SWITCH		(1L << 5)		// Switch view

static Display *disp = NULL;

/* Print messages {{{ */
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
/* }}} */

/* Print usage/version {{{ */
static void
Usage(void)
{
	printf("Usage: %s [OPTIONS]\n\n" \
					"Options:\n" \
					"  -c, --clients           \t List all clients managed by subtle\n" \
					"  -d, --display=DISPLAY   \t Connect to DISPLAY (default: $DISPLAY)\n" \
					"  -f, --find=CLIENT       \t Find client\n" \
					"  -h, --help              \t Show this help and exit\n\n" \
					"  -s, --switch=VIEW       \t Switch to view\n" \
					"  -v, --views             \t List all views managed by subtle\n" \
					"  -A, --active=CLIENT     \t Give focus to client\n" \
					"  -D, --debug             \t Print debugging messages\n" \
					"  -F, --float=CLIENT      \t Toggle float of a client\n" \
					"  -S, --shade=CLIENT      \t Toggle shade of a client\n" \
					"  -V, --version           \t Show version info and exit\n" \
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
/* }}} */

/* Handle signals {{{ */
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
/* }}} */

/* Alloc memory {{{ */
void *
Alloc(size_t n,
	size_t size)
{
	void *mem = calloc(n, size);
	if(!mem) Error("Can't alloc memory. Exhausted?\n");
	return(mem);
}
/* }}} */

/* Send client message {{{ */
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

	if(!XSendEvent(disp, DefaultRootWindow(disp), False, mask, &ev)) printf("Can't send client message %s\n", prop);
	Debug("Send: prop=%s, [0]=%ld [1]=%ld [2]=%ld [3]=%ld [4]=%ld\n", prop, data0, data1, data2, data3, data4);
}
/* }}} */

/* Get window propery {{{ */
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
/* }}} */

/* Get clients {{{ */
static Window *
GetClients(unsigned long *size)
{
	Window *clients = (Window *)GetProperty(DefaultRootWindow(disp), XA_WINDOW, "_NET_CLIENT_LIST", size);
	if(clients)
		{
			*size = *size / sizeof(Window);
			return(clients);
		}
	else
		{
			*size = 0;
			printf("Failed to get client list\n");
			return(NULL);
		}
}
/* }}} */

/* Get client name {{{ */
static char *
GetClientName(Window win)
{
	char *name = GetProperty(win, XA_STRING, "WM_NAME", NULL);
	return(name ? name : NULL);
}
/* }}} */

/* Get client class {{{ */
static char *
GetClientClass(Window win)
{
	char *class = GetProperty(win, XA_STRING, "WM_CLASS", NULL);
	return(class ? class : NULL);
}
/* }}} */

/* Find client {{{ */
static Window
FindClient(char *name)
{
	unsigned long size = 0;
	Window *clients = GetClients(&size);

	if(clients)
		{
			int i, errcode;
			regex_t *regex = (regex_t *)Alloc(1, sizeof(regex_t));

			/* Thread safe error handling.. */
			if((errcode = regcomp(regex, name, REG_EXTENDED|REG_NOSUB|REG_ICASE)))
				{
					size_t errsize = regerror(errcode, regex, NULL, 0);
					char *errbuf = (char *)Alloc(1, errsize);

					regerror(errcode, regex, errbuf, errsize);

					Warn("Can't compile regex `%s'\n", name);
					Debug("%s\n", errbuf);

					free(errbuf);
					regfree(regex);
					free(regex);
					free(clients);

					return(0);
				}
			for(i = 0; i < size; i++)
				{
					char *clientname = GetClientName(clients[i]);
					if(name && !regexec(regex, clientname, 0, NULL, 0)) 
						{
							Window win = clients[i];

							Debug("Found: win=%#lx\n", win);

							regfree(regex);
							free(regex);
							free(clients);
							free(clientname);

							return(win);
						}
					else if(clientname) free(clientname);
				}
			regfree(regex);
			free(regex);
		}
	free(clients);

	return(0);
}
/* }}} */

/* Print client info {{{ */
static void
PrintClient(Window win)
{
	Window nil;
	unsigned int width, height, border;
	unsigned long *cv = NULL, *rv = NULL;
	char *name = NULL, *class = NULL;

	assert(win);

	name = GetClientName(win);
	class = GetClientClass(win);

	XGetGeometry(disp, win, &nil, &width, &height, &width, &height, &border, &border);

	cv = (unsigned long*)GetProperty(win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL);
	rv = (unsigned long*)GetProperty(DefaultRootWindow(disp), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

	printf("%#lx %c %d %dx%d %s (%s)\n", win, (*cv == *rv ? '*' : ' '), *cv, width, height, name, class);

	if(name) free(name);
	if(class) free(class);
	if(cv) free(cv);
	if(rv) free(rv);
}
/* }}} */

/* Action: Get clients {{{ */
static void
ActionGetClients(void)
{
	unsigned long size = 0;
	Window *clients = NULL;

	Debug("Action: ActionGetClients\n");

	clients = GetClients(&size);

	if(clients)
		{
			int i;

			for(i = 0; i < size; i++) PrintClient(clients[i]);
			free(clients);
		}
}
/* }}} */

/* Action: Find client {{{ */
static void
ActionFindClient(char *name)
{
	Window win;

	assert(name);
	Debug("Action: ActionFindClient\n");
	
	win = FindClient(name);
	if(win) PrintClient(win);
	else printf("Can't find window `%s'\n", name);
}
/* }}} */

/* Action: Activate client {{{ */
static void
ActionActivateClient(char *name)
{
	Window win;

	assert(name);
	Debug("Action: ActionActivateClient\n");

	win = FindClient(name);
	if(win)
		{
			unsigned long *cv = NULL, *rv = NULL;

			cv = (unsigned long*)GetProperty(win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL);
			rv = (unsigned long*)GetProperty(DefaultRootWindow(disp), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

			if(*cv != *rv) 
				{
					Debug("Switching: active=%d, view=%d\n", *rv, *cv);
					SendMessage(DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", *cv, 0, 0, 0, 0);
				}
			else SendMessage(win, "_NET_ACTIVE_WINDOW", 0, 0, 0, 0, 0);
					
			if(cv) free(cv);
			if(rv) free(rv);
		}
}
/* }}} */

/* Action: Set view {{{ */
static void
ActionSetView(int view)
{
	assert(view >= 0);
	Debug("Action: ActionSetView\n");

	SendMessage(DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", (unsigned long)view, 0, 0, 0, 0);
}
/* }}} */

static void
ActionGetViews(void)
{
}

int
main(int argc,
	char *argv[])
{
	int c, action = 0, number = 0;
	char *display = NULL, *string = NULL, buf[256];
	struct sigaction act;
	static struct option long_options[] =
	{
		{ "clients",		no_argument,					0,	'c' },
		{ "display",		required_argument,		0,	'd' },
		{ "find",				required_argument,		0,	'f' },
		{ "help",				no_argument,					0,	'h' },
		{ "switch",			required_argument,		0,	's' },
		{ "views",			no_argument,					0,	'v' },
		{ "activate",		required_argument,		0,	'A' },
#ifdef DEBUG
		{ "debug",			no_argument,					0,	'D' },
#endif /* DEBUG */
		{ "float",			required_argument,		0,	'F' },
		{ "shade",			required_argument,		0,	'S' },
		{ "version",		no_argument,					0,	'V' },
		{ 0, 0, 0, 0}
	};

	while((c = getopt_long(argc, argv, "cd:hf:s:vA:DFS:V", long_options, NULL)) != -1)
		{
			if(optarg)
				{
					/* Pipe or argument */
					if(!strncmp(optarg, "-", 1)) 
						{
							if(!fgets(buf, sizeof(buf), stdin)) Error("Can't read from pipe\n");
							string = (char *)Alloc(strlen(buf), sizeof(char));
							strncpy(string, buf, strlen(buf) - 1);
							Debug("Read: string=%s\n", string, strlen(string), buf, strlen(buf));
						}
					else string = strdup(optarg);

					/* Convert string to int if possible */
					if(isdigit(string[0])) number = atoi(string);
				}

			switch(c)
				{
					case 'c':	action	= ACTION_CLIENTS;		break;
					case 'd': display	= string;						break;
					case 'f': action	= ACTION_FIND;			break;
					case 'h': Usage(); 										return(0);
					case 's': action	= ACTION_SWITCH;		break;
					case 'v':	action	= ACTION_VIEWS;			break;
					case 'A': action	= ACTION_ACTIVATE;	break;
#ifdef DEBUG					
					case 'D': debug = 1;									break;
#endif /* DEBUG */
					case 'V': Version(); 									return(0);
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
			case ACTION_ACTIVATE:	ActionActivateClient(string);		break;
			case ACTION_CLIENTS:	ActionGetClients();							break;
			case ACTION_FIND:			ActionFindClient(string);				break;
			case ACTION_VIEWS:		ActionGetViews();								break;
			case ACTION_SWITCH:		ActionSetView(number);					break;
			default:
				Warn("Action not implemented yet\n");
		}

	XCloseDisplay(disp);
	
	return(0);
}
