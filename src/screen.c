
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static int screens = 0;
static SubWin *first = NULL;

 /**
	* Init screen 
	**/

void
subScreenInit(void)
{
	long data[4] = { 0, 0, 0, 0 };

	/* Screen bar windows */
	d->bar.win			= XCreateSimpleWindow(d->dpy, DefaultRootWindow(d->dpy), 0, 0, 
		DisplayWidth(d->dpy, DefaultScreen(d->dpy)), d->th, 0, 0, d->colors.norm);
	d->bar.screens	= XCreateSimpleWindow(d->dpy, d->bar.win, 0, 0, 1, d->th, 0, 0, d->colors.norm);
	d->bar.sublets	= XCreateSimpleWindow(d->dpy, d->bar.win, 0, 0, 1, d->th, 0, 0, d->colors.norm);

	XSetWindowBackground(d->dpy, d->bar.win, d->colors.norm);
	XSetWindowBackground(d->dpy, d->bar.screens, d->colors.norm);
	XSetWindowBackground(d->dpy, d->bar.sublets, d->colors.norm);

	XSelectInput(d->dpy, d->bar.screens, ButtonPressMask); 

	XMapWindow(d->dpy, d->bar.screens);
	XMapWindow(d->dpy, d->bar.sublets);

	/* EWMH: Window manager information */
	subEwmhSetWindow(DefaultRootWindow(d->dpy), SUB_EWMH_NET_SUPPORTING_WM_CHECK, DefaultRootWindow(d->dpy));
	subEwmhSetString(DefaultRootWindow(d->dpy), SUB_EWMH_NET_WM_NAME, PACKAGE_STRING);
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_WM_PID, (long)getpid());
	subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_DESKTOP_VIEWPORT, (long *)&data, 2);

	/* EWMH: Workarea size */
	data[2] = DisplayWidth(d->dpy, DefaultScreen(d->dpy)); 
	data[3] = DisplayHeight(d->dpy, DefaultScreen(d->dpy));
	subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_WORKAREA, (long *)&data, 4);

	/* EWMH: Desktop sizes */
	data[0] = DisplayWidth(d->dpy, DefaultScreen(d->dpy));
	data[1] = DisplayHeight(d->dpy, DefaultScreen(d->dpy));
	subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&data, 2);

	/* EWMH: Supported window states */
	data[0] = subEwmhGetAtom(SUB_EWMH_NET_WM_STATE_MODAL);
	data[1] = subEwmhGetAtom(SUB_EWMH_NET_WM_STATE_SHADED);
	data[2] = subEwmhGetAtom(SUB_EWMH_NET_WM_STATE_HIDDEN);
	data[3] = subEwmhGetAtom(SUB_EWMH_NET_WM_STATE_FULLSCREEN);
	subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_SUPPORTED, (long *)&data, 4);
}

static void
UpdateName(SubScreen *s,
	int n)
{
	if(s)
		{
			char buf[5]; /* FIXME: static buffer */

			if(s->name) free(s->name);
			snprintf(buf, sizeof(buf), "%d", n);
			s->name = (char *)subUtilAlloc(strlen(buf) + 1, sizeof(char));
			s->n		= n;
			strncpy(s->name, buf, strlen(buf));
		}
}

static void
UpdateScreens(void)
{
	int i = 0;
	SubWin *s = first;
	Window *wins = NULL;

	/* EWMH: Virtual roots */
	wins = (Window *)subUtilAlloc(screens, sizeof(Window));
	while(s)
		{
			wins[i++] = s->frame;
			s = s->next;
		}
	subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_VIRTUAL_ROOTS, wins, screens);
	free(wins);

	/* EWMH: Desktops */
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, screens);
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, d->screen->screen->n);
}

 /**
	* Create a screen 
	* @return Returns a new #SubWin
	**/

SubWin *
subScreenNew(void)
{
	SubWin *w = subWinNew();

	w->screen = (SubScreen *)subUtilAlloc(1, sizeof(SubScreen));

	UpdateName(w->screen, ++screens);

	w->flags = SUB_WIN_TYPE_SCREEN|SUB_WIN_TYPE_TILE|SUB_WIN_TILE_HORZ;

	w->screen->width	= strlen(w->screen->name) * d->fx + 2;
	w->screen->button	= XCreateSimpleWindow(d->dpy, d->bar.screens, 0, 0, 1, d->th - 6, 1, d->colors.border, d->colors.norm);

	/* Hierarchy */
	if(d->screen) d->screen->next = w;
	w->prev = d->screen;
	if(!first) first = w;

	/* Switch to new screen */
	if(d->screen) XUnmapWindow(d->dpy, d->screen->frame);
	d->screen = w;
	XMapRaised(d->dpy, d->bar.win);
	XMapRaised(d->dpy, w->screen->button);

	UpdateScreens();

	subScreenConfigure();
	printf("Adding screen (%s)\n", w->screen->name);

	subWinMap(w);

	XSaveContext(d->dpy, w->screen->button, 1, (void *)w);
	return(w);
}

 /**
	* Delete a screen window 
	* @param w A #SubWin
	**/

void
subScreenDelete(SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_SCREEN)
		{
			printf("Removing screen (%s)\n", w->screen->name);

			XUnmapWindow(d->dpy, w->screen->button);
			XDeleteContext(d->dpy, w->screen->button, 1);
			XDestroyWindow(d->dpy, w->screen->button);
			free(w->screen->name);
			free(w->screen);

			if(d->screen == w)
				if(w->prev) 
					{
						d->screen = w->prev;
						XMapSubwindows(d->dpy, d->screen->frame);
					}

			UpdateScreens();

			printf("Switching to screen (%s)\n", d->screen->screen->name);

			screens--;

			subScreenConfigure();
		}
}

 /**
	* Render a screen 
	* @param mode Render mode
	* @param w A #SubWin
	**/

void
subScreenRender(SubWin *w)
{
	int i;
	unsigned int n = 0;
	Window nil, *wins = NULL;
	SubWin *s = first;

	XClearWindow(d->dpy, d->bar.win);
	XFillRectangle(d->dpy, d->bar.win, d->gcs.border, 0, 2, DisplayWidth(d->dpy, DefaultScreen(d->dpy)), d->th - 4);	

	while(s)
		{
			XSetWindowBackground(d->dpy, s->screen->button, (d->screen == s) ? d->colors.focus : d->colors.norm);
			XClearWindow(d->dpy, s->screen->button);
			XDrawString(d->dpy, s->screen->button, d->gcs.font, 1, d->fy - 4, s->screen->name, strlen(s->screen->name));
			s = s->next;
		}
}

 /**
	* Configure screen bar
	**/

void
subScreenConfigure(void)
{
	if(screens)
		{
			int i, width = 3;
			SubWin *s = first;

			while(s)
				{
					XMoveResizeWindow(d->dpy, s->screen->button, width, 2, s->screen->width, d->th - 6);
					width += s->screen->width + 6;
					s = s->next;
				}
			XMoveResizeWindow(d->dpy, d->bar.screens, 0, 0, width, d->th);
		}
}

 /**
	* Kill screens 
	**/

void
subScreenKill(void)
{
	if(screens)
		{
			SubWin *s = first;

			printf("Removing screens\n");

			while(s)
				{
					/* Manually removing all screens */
					XUnmapWindow(d->dpy, s->screen->button);
					XDeleteContext(d->dpy, s->screen->button, 1);
					XDeleteContext(d->dpy, s->frame, 1);
					XDestroyWindow(d->dpy, s->screen->button);
					XDestroyWindow(d->dpy, s->frame);

					free(s->screen->name);
					s = s->next;
					free(s);					
				}

			XDestroyWindow(d->dpy, d->bar.win);
			XDestroyWindow(d->dpy, d->bar.screens);
			XDestroyWindow(d->dpy, d->bar.sublets);
		}
}

 /**
	* Switch screen
	* @param w New active screen
	**/

void
subScreenSwitch(SubWin *w)
{
	if(!w) 
		{
			subScreenNew();
			return;
		}
	
	XUnmapWindow(d->dpy, d->screen->frame);

	XMapRaised(d->dpy, d->bar.win);
	XMapRaised(d->dpy, d->screen->screen->button);
	XMapSubwindows(d->dpy, d->screen->frame);

	/* EWMH: Desktops */
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, d->screen->screen->n);

	printf("Switching to screen (%s)\n", d->screen->screen->name);
}
