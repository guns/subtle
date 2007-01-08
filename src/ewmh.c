#include "subtle.h"

static Atom atoms[34];

void
subEwmhNew(void)
{
	int n = 34;
	char *names[] =
	{
		/* ICCCM */
		"WM_STATE",
		"WM_CHANGE_STATE",
		"WM_PROTOCOLS",
		"WM_COLORMAP_WINDOWS",
		"WM_TAKE_FOCUS",
		"WM_WINDOW_ROLE",
		"WM_DELETE_WINDOW",

		/* EWMH */
		"_NET_SUPPORTED",
		"_NET_CLIENT_LIST",
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
		"_NET_WM_DESKTOP",

		"_NET_WM_STATE",
		"_NET_WM_STATE_SHADED",
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

char *
subEwmhGet(Atom type,
	Window win,
	int hint)
{
	Atom ar;
	unsigned long n, b;
	unsigned char *data = NULL, *ret = NULL;
	int format;

	XGetWindowProperty(d->dpy, win, atoms[hint], 0L, 1L, False, type, &ar, &format, &n, &b, &data);
	if(data)
		{
			ret	= data[0];
			XFree(data);
		}
	return(ret);
}

/*void*/
/*subEwmhSetHint(Window win,*/
/*int hint)*/
/*{*/
/*XChangeProperty(d->dpy, win, hints[SUB_HINT_WIN_TYPE], XA_ATOM, 32, PropModeReplace,*/
/*(unsigned char *)&hints[hint], 1);*/
/*}*/

/*void*/
/*subEwmhSetCardinalHint(Window win,*/
/*int hint,*/
/*int value)*/
/*{*/
/*XChangeProperty(d->dpy, win, hints[hint], XA_CARDINAL, 32, PropModeReplace,*/
/*(unsigned char *)&value, 1);*/
/*}*/
