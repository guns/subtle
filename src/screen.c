
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

static int size = 0, active = -1;
static SubScreen **screens = NULL;

 /**
	* Find a screen
	* @param s A window
	* @return Returns a #SubScreen or NULL
	**/

SubScreen *
subScreenFind(Window win)
{
	SubScreen *s = NULL;
	return(XFindContext(d->dpy, win, 1, (void *)&s) != XCNOENT ? s : NULL);
}

 /**
	* Init screen 
	**/

void
subScreenInit(void)
{
	long data[4] = { 0, 0, 0, 0 };

	/* Screen bar windows */
	d->bar.win			= XCreateSimpleWindow(d->dpy, DefaultRootWindow(d->dpy), 0, 0, 1, d->th, 0, 0, d->colors.norm);
	d->bar.screens	= XCreateSimpleWindow(d->dpy, d->bar.win, 0, 0, 1, d->th, 0, 0, d->colors.norm);
	d->bar.sublets	= XCreateSimpleWindow(d->dpy, d->bar.win, 0, 0, 1, d->th, 0, 0, d->colors.norm);

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
			s->name = (char *)calloc(strlen(buf) + 1, sizeof(char));
			if(!s->name) subLogError("Can't alloc memory. Exhausted?\n");
			strncpy(s->name, buf, strlen(buf));
		}
}

static void
UpdateScreens(void)
{
	int i;
	Window *wins = NULL;

	/* EWMH: Desktops */
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, size);
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, active);

	/* EWMH: Virtual roots */
	wins = (Window *)calloc(size, sizeof(Window));
	if(!wins) subLogError("Can't alloc memory. Exhausted?\n");
	for(i = 0; i < size; i++) wins[i] = screens[i]->w->frame;
	subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_VIRTUAL_ROOTS, wins, size);
	free(wins);
}

 /**
	* Create a screen window 
	* @return Returns a new #SubScreen
	**/

SubScreen *
subScreenNew(void)
{
	/* Create screen  */
	SubScreen *s = (SubScreen *)calloc(1, sizeof(SubScreen));
	if(!s) subLogError("Can't alloc memory. Exhausted?\n");

	UpdateName(s, size + 1);

	s->w				= subTileNew(SUB_WIN_TILE_HORZ);
	s->w->flags |= SUB_WIN_TYPE_SCREEN;
	s->width		= strlen(s->name) * d->fx + 2;
	s->button		= XCreateSimpleWindow(d->dpy, d->bar.screens, 0, 0, 1, d->th - 6, 1, d->colors.border, d->colors.norm);

	screens = (SubScreen **)realloc(screens, sizeof(SubScreen *) * (size + 1));
	if(!screens) subLogError("Can't alloc memory. Exhausted?\n");
	screens[size] = s;

	/* Switch to new screen */
	if(active >= 0) subWinUnmap(screens[active]->w);
	XMapRaised(d->dpy, d->bar.win);
	XMapRaised(d->dpy, s->button);

	d->focus	= s->w;
	active		= size;
	size++;

	subKeyGrab(s->w);
	UpdateScreens();

	subScreenConfigure();
	printf("Adding screen (%d)\n", size);

	XSaveContext(d->dpy, s->button, 1, (void *)s);
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
			int i, j;

			for(i = 0; i < size; i++) 	
				{
					UpdateName(screens[i], i + 1);
					if(screens[i]->w == w)
						{
							printf("Removing screen (%s)\n", screens[i]->name);
		
							XUnmapWindow(d->dpy, screens[i]->button);
							XDeleteContext(d->dpy, screens[i]->button, 1);
							XDestroyWindow(d->dpy, screens[i]->button);
							free(screens[i]->name);
							free(screens[i]);

							if(i == size) active = i - 1; /* Deletion of latest screen */
							else active = i;

							for(j = i; j < size - 1; j++) screens[j] = screens[j + 1];
						}
				}

			screens = (SubScreen **)realloc(screens, sizeof(SubScreen *) * size);
			if(!screens) subLogError("Can't alloc memory. Exhausted?\n");

			size--;
			if(size == 0) raise(SIGTERM); /* Exit if the last screen is deleted */

			UpdateScreens();

			subWinMap(screens[active]->w);
			printf("Switching to screen (%s)\n", screens[active]->name);

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
	if(w && w->flags & SUB_WIN_TYPE_SCREEN)
		{
			int i;
			unsigned int n = 0;
			Window nil, *wins = NULL;
			unsigned long col = d->focus && d->focus == w ? d->colors.focus : d->colors.norm;

			XSetWindowBackground(d->dpy, d->bar.screens, col);
			XClearWindow(d->dpy, d->bar.screens);

			/* Screens */
			for(i = 0; i < size; i++)
				{
					XSetWindowBackground(d->dpy, screens[i]->button, (active == i) ? d->colors.cover : col);
					XClearWindow(d->dpy, screens[i]->button);
					XDrawString(d->dpy, screens[i]->button, d->gcs.font, 1, d->fy - 4, 
						screens[i]->name, strlen(screens[i]->name));
				}

			/* Sublets */
			XQueryTree(d->dpy, d->bar.sublets, &nil, &nil, &wins, &n);
			if(n > 0)
				{
					for(i = 0; i < n; i++)
						{
							SubSublet *s = subSubletFind(wins[i]);
							if(s) subSubletRender(s);
						}
					XFree(wins);
				}					

			subTileRender(w);
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
			int i, scw = 3, suw = 0, width = 0;
			unsigned int n = 0;
			Window nil, *wins = NULL;

			/* Screens */
			for(i = 0; i < size; i++)
				{
					XMoveResizeWindow(d->dpy, screens[i]->button, scw, 2, screens[i]->width, d->th - 6);
					scw += screens[i]->width + 6;
				}
			XMoveResizeWindow(d->dpy, d->bar.screens, 0, 0, scw, d->th);

			/* Sublets */
			XQueryTree(d->dpy, d->bar.sublets, &nil, &nil, &wins, &n);
			if(n > 0)
				{
					for(i = 0; i < n; i++)
						{
							SubSublet *s = subSubletFind(wins[i]);
							if(s) 
								{
									XMoveResizeWindow(d->dpy, s->win, suw, 0, s->width, d->th);
									suw += s->width;
								}
						}
					XMoveResizeWindow(d->dpy, d->bar.sublets, scw, 0, suw, d->th);
					XFree(wins);
				}

			XMoveResizeWindow(d->dpy, d->bar.win, DisplayWidth(d->dpy, DefaultScreen(d->dpy)) - (scw + suw), 
				0, scw + suw, d->th);
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
			int i;

			for(i = 0; i < size; i++) 
				{
					/* Manually to skip useless reallocs */
					XUnmapWindow(d->dpy, screens[i]->button);
					XDeleteContext(d->dpy, screens[i]->button, 1);
					XDestroyWindow(d->dpy, screens[i]->button);
					free(screens[i]->name);
					free(screens[i]);					
				}

			XDestroyWindow(d->dpy, d->bar.screens);
			XDestroyWindow(d->dpy, d->bar.sublets);
			free(screens);
		}
}

 /**
	* Get screen pointer
	* @params pos Position
	* @return Returns a #SubScreen or NULL
	**/

SubScreen *
subScreenGetPtr(int pos)
{
	if(pos == SUB_SCREEN_ACTIVE) return(active >= 0 ? screens[active] : NULL);
	else return(pos - 1 < size ? screens[pos - 1] : NULL);
}

 /**
	* Get screen position
	* @param s A #SubScreen
	* @return Returns either a number on success or otherwise -1
	**/

int
subScreenGetPos(SubScreen *s)
{
	if(s)
		{
			int i;

			for(i = 0; i < size; i++)
				if(screens[i] == s) return(i);
			return(-1);
		}	
}

 /**
	* Switch screen
	* @param pos New active screen
	**/

void
subScreenSwitch(int pos)
{
	int next;

	/* Switch to screens */
	if(pos == active) return;
	else if(pos == SUB_SCREEN_NEXT) next = active + 1;
	else if(pos == SUB_SCREEN_PREV && active > 0) next = active - 1;
	else if(pos >= 0 && pos - 1 < size) next = pos;
	else return;

	if(next == size)
		{
			subScreenNew();
			return;
		}
	
	subWinUnmap(screens[active]->w);
	active = next;

	XMapRaised(d->dpy, d->bar.win);
	XMapRaised(d->dpy, screens[active]->button);
	subWinMap(screens[active]->w);

	/* Check focus */
	if(d->focus->flags & SUB_WIN_TYPE_SCREEN)
		{
			if(d->focus) subWinRender(d->focus);
			d->focus = screens[active]->w;
			subWinRender(screens[active]->w);				
		}

	/* EWMH: Desktops */
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, active);

	printf("Switching to screen (%s)\n", screens[active]->name);
}
