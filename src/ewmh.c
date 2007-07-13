
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* This program is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
	* (at your option) any later version.
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	* GNU General Public License for more details.
	*
	* You should have received a copy of the GNU General Public License along
	* with this program; if not, write to the Free Software Foundation, Inc.,
	* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
	**/

#include "subtle.h"

static Atom atoms[37];

void
subEwmhInit(void)
{
	int n = 37;
	char *names[] =
	{
		/* ICCCM */
		"WM_STATE",
		"WM_PROTOCOLS",
		"WM_TAKE_FOCUS",
		"WM_DELETE_WINDOW",
		"WM_NORMAL_HINTS",
		"WM_SIZE_HINTS",

		/* EWMH */
		"_NET_SUPPORTED",
		"_NET_CLIENT_LIST",
		"_NET_CLIENT_LIST_STACKING",
		"_NET_NUMBER_OF_DESKTOPS",
		"_NET_DESKTOP_GEOMETRY", 
		"_NET_DESKTOP_VIEWPORT",
		"_NET_CURRENT_DESKTOP", 
		"_NET_ACTIVE_WINDOW",
		"_NET_WORKAREA",
		"_NET_SUPPORTING_WM_CHECK",
		"_NET_VIRTUAL_ROOTS",
		"_NET_CLOSE_WINDOW",

		"_NET_WM_NAME",
		"_NET_WM_PID",
		"_NET_WM_DESKTOP",

		"_NET_WM_STATE",
		"_NET_WM_STATE_MODAL",
		"_NET_WM_STATE_SHADED",
		"_NET_WM_STATE_HIDDEN",
		"_NET_WM_STATE_FULLSCREEN",

		"_NET_WM_WINDOW_TYPE",
		"_NET_WM_WINDOW_TYPE_DESKTOP",
		"_NET_WM_WINDOW_TYPE_NORMAL",
		"_NET_WM_WINDOW_TYPE_DIALOG",

		"_NET_WM_ALLOWED_ACTIONS",
		"_NET_WM_ACTION_MOVE",
		"_NET_WM_ACTION_RESIZE",
		"_NET_WM_ACTION_SHADE",
		"_NET_WM_ACTION_FULLSCREEN",
		"_NET_WM_ACTION_CHANGE_DESKTOP",
		"_NET_WM_ACTION_CLOSE"
	};

	XInternAtoms(d->dpy, names, n, 0, atoms);
}

Atom
subEwmhGetAtom(int hint)
{
	return(atoms[hint]);
}

static unsigned long
GetProperty(Atom type,
	Window win,
	int hint,
	unsigned long size,
	unsigned char **data)
{
	Atom ar;
	unsigned long n, b;
	int format;

	XGetWindowProperty(d->dpy, win, atoms[hint], 0L, size, False, type, &ar, &format, &n, &b, data);
	return(n);
}

int
subEwmhSetWindow(Window win,
	int hint,
	Window value)
{
	return(XChangeProperty(d->dpy, win, atoms[hint], XA_WINDOW, 32, PropModeReplace,
		(unsigned char *)&value, 1));
}

int
subEwmhSetWindows(Window win,
	int hint,
	Window *values,
	int size)
{
	return(XChangeProperty(d->dpy, win, atoms[hint], XA_WINDOW, 32, PropModeReplace,
		(unsigned char *)values, size));
}

int
subEwmhSetString(Window win,
	int hint,
	const char *value)
{
	return(XChangeProperty(d->dpy, win, atoms[hint], XA_STRING, 8, PropModeReplace,
		(unsigned char *)value, strlen(value)));
}

int
subEwmhGetString(Window win,
	int hint,
	char *value)
{
	unsigned char *data = NULL;

	if(GetProperty(XA_STRING, win, hint, 64L, &data))
		{
			value = (char *)data;
			XFree(data);
			return(True);
		}
	return(False);
}

int
subEwmhSetCardinal(Window win,
	int hint,
	long value)
{
	return(XChangeProperty(d->dpy, win, atoms[hint], XA_CARDINAL, 32, PropModeReplace,
		(unsigned char *)&value, 1));
}

int
subEwmhSetCardinals(Window win,
	int hint,
	long *values,
	int size)
{
	return(XChangeProperty(d->dpy, win, atoms[hint], XA_CARDINAL, 32, PropModeReplace,
		(unsigned char *)values, size));
}

int
subEwmhGetCardinal(Window win,
	int hint,
	long *value)
{
	long *data = NULL;

	if(GetProperty(XA_CARDINAL, win, hint, 1L, (unsigned char **)&data))
		{
			*value = *data;
			XFree(data);
			return(True);
		}
	return(False);
}

void *
subEwmhGetProperty(Window win,
	int hint,
	Atom type,
	int *num)
{
	unsigned char *data = NULL;

	*num = (int)GetProperty(type, win, hint, 0x7fffffff, &data);
	return((void *)data);
}

void
subEwmhDeleteProperty(Window win,
	int hint)
{
	XDeleteProperty(d->dpy, win, atoms[hint]);
}
