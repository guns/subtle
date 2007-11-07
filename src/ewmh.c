
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

#define NATOMS 40

static Atom atoms[NATOMS];

 /**
	* Init ewmh
	**/

void
subEwmhInit(void)
{
	int n = NATOMS;
	char *names[] =
	{
		/* ICCCM */
		"WM_NAME", "WM_CLASS", "WM_STATE", "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "WM_NORMAL_HINTS", "WM_SIZE_HINTS",

		/* EWMH */
		"_NET_SUPPORTED", "_NET_CLIENT_LIST", "_NET_CLIENT_LIST_STACKING", "_NET_NUMBER_OF_DESKTOPS",
		"_NET_DESKTOP_NAMES", "_NET_DESKTOP_GEOMETRY", "_NET_DESKTOP_VIEWPORT", "_NET_CURRENT_DESKTOP", "_NET_ACTIVE_WINDOW",
		"_NET_WORKAREA", "_NET_SUPPORTING_WM_CHECK", "_NET_VIRTUAL_ROOTS", "_NET_CLOSE_WINDOW",
		"_NET_WM_PID", "_NET_WM_DESKTOP", 
		"_NET_WM_STATE", "_NET_WM_STATE_MODAL", "_NET_WM_STATE_SHADED", "_NET_WM_STATE_HIDDEN", "_NET_WM_STATE_FULLSCREEN",
		"_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DESKTOP", "_NET_WM_WINDOW_TYPE_NORMAL", "_NET_WM_WINDOW_TYPE_DIALOG",
		"_NET_WM_ALLOWED_ACTIONS", "_NET_WM_ACTION_MOVE", "_NET_WM_ACTION_RESIZE", "_NET_WM_ACTION_SHADE",
		"_NET_WM_ACTION_FULLSCREEN", "_NET_WM_ACTION_CHANGE_DESKTOP", "_NET_WM_ACTION_CLOSE",

		/* Misc */
		"UTF8_STRING",

		/* subtle */
	};
	long data[4] = { 0, 0, 0, 0 }, pid = (long)getpid();

	XInternAtoms(d->disp, names, n, 0, atoms);

	/* Window manager information */
	subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_SUPPORTING_WM_CHECK, &DefaultRootWindow(d->disp), 1);
	subEwmhSetString(DefaultRootWindow(d->disp), SUB_EWMH_WM_NAME, PACKAGE_STRING);
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_WM_PID, &pid, 1);
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_DESKTOP_VIEWPORT, (long *)&data, 2);

	/* Workarea size */
	data[2] = DisplayWidth(d->disp, DefaultScreen(d->disp)); 
	data[3] = DisplayHeight(d->disp, DefaultScreen(d->disp));
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_WORKAREA, (long *)&data, 4);

	/* Desktop sizes */
	data[0] = DisplayWidth(d->disp, DefaultScreen(d->disp));
	data[1] = DisplayHeight(d->disp, DefaultScreen(d->disp));
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&data, 2);

	/* Supported window states */
	data[0] = atoms[SUB_EWMH_NET_WM_STATE_MODAL];
	data[1] = atoms[SUB_EWMH_NET_WM_STATE_SHADED];
	data[2] = atoms[SUB_EWMH_NET_WM_STATE_HIDDEN];
	data[3] = atoms[SUB_EWMH_NET_WM_STATE_FULLSCREEN];
	subEwmhSetCardinals(DefaultRootWindow(d->disp), SUB_EWMH_NET_SUPPORTED, (long *)&data, 4);	
}

 /**
	* Find intern atom 
	* @param hint Hint number
	* @return The desired atom
	**/

Atom
subEwmhFind(int hint)
{
	assert(hint <= NATOMS);
	return(atoms[hint]);
}

 /**
	* Get property from window
	* @param win Window
	* @param type Atom type
	* @param hint Hint number
	* @param size Size of items
	* @return Return data associated with the property
	**/

char *
subEwmhGetProperty(Window win,
	Atom type,
	int hint,
	unsigned long *size)
{
	unsigned long nitems, bytes;
	unsigned char *data = NULL;
	int format;
	Atom rtype;

	if(XGetWindowProperty(d->disp, win, atoms[hint], 0L, 1024, False, type, &rtype, 
		&format, &nitems, &bytes, &data) != Success)
		{
			subUtilLogDebug("Failed to get property (%d)\n", hint);
			return(NULL);
		}
	if(type != rtype)
		{
			subUtilLogDebug("Invalid type for property (%d)\n", hint);
			XFree(data);
			return(NULL);
		}
	if(size) *size = (format / 8) * nitems;

	return(data);
}

 /**
	* Change window property
	* @param win Window
	* @param hint Hint number
	* @param values Window list
	* @param size Size of the list
	**/

void
subEwmhSetWindows(Window win,
	int hint,
	Window *values,
	int size)
{
	XChangeProperty(d->disp, win, atoms[hint], XA_WINDOW, 32, PropModeReplace, (unsigned char *)values, size);
}

 /**
	* Change window property
	* @param win Window
	* @param hint Hint number
	* @param values Cardinal list
	* @param size Size of the list
	**/

void
subEwmhSetCardinals(Window win,
	int hint,
	long *values,
	int size)
{
	XChangeProperty(d->disp, win, atoms[hint], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)values, size);
}

 /**
	* Change window property
	* @param win Window
	* @param hint Hint number
	* @param value String value
	**/

void
subEwmhSetString(Window win,
	int hint,
	char *value)
{
	XChangeProperty(d->disp, win, atoms[hint], atoms[SUB_EWMH_UTF8], 8, 
		PropModeReplace, (unsigned char *)value, strlen(value));
}

 /**
	* Change window property
	* @param win Window
	* @param hint Hint number
	* @param values String list
	* @param size Size of the list
	**/

void
subEwmhSetStrings(Window win,
	int hint,
	char **values,
	int size)
{
	int i, len = 0, pos = 0;
	char *str = NULL;

	for(i = 0; i < size; i++) len += strlen(values[i]);

	str = (char *)subUtilAlloc(len + i + 1, sizeof(char *));

	for(i = 0; i < size; i++)
		{
			strncpy(str + pos, values[i], strlen(values[i]));
			pos += strlen(values[i]);
			str[pos] = '\0';
			pos++;
		}

	XChangeProperty(d->disp, win, atoms[hint], atoms[SUB_EWMH_UTF8], 8, PropModeReplace, (unsigned char *)str, pos);
	free(str);
}
