#include "subtle.h"

static int size = 0, active = -1;
static SubScreen **screens = NULL;

 /**
	* Get active screen
	* @return Returns the active #SubWin
	**/

SubWin *
subScreenGetActive(void)
{
	return((active >= 0) ? screens[active]->w : NULL);
}

 /**
	* Init screen {{{
	**/

void
subScreenInit(void)
{
	/* Screen bar windows */
	d->bar.win			= XCreateSimpleWindow(d->dpy, DefaultRootWindow(d->dpy), 0, 0, 1, d->th, 0, 0, d->colors.norm);
	d->bar.screens	= XCreateSimpleWindow(d->dpy, d->bar.win, 0, 0, 1, d->th, 0, 0, d->colors.norm);
	d->bar.sublets	= XCreateSimpleWindow(d->dpy, d->bar.win, 0, 0, 1, d->th, 0, 0, d->colors.norm);

	XMapWindow(d->dpy, d->bar.screens);
	XMapWindow(d->dpy, d->bar.sublets);
} /* }}} */

 /**
	* Create a screen window {{{
	* @return Returns a new #SubScreen
	**/

SubScreen *
subScreenNew(void)
{
	int i;
	char buf[5];
	Window *wins = NULL;

	/* Create screen  */
	SubScreen *s = (SubScreen *)calloc(1, sizeof(SubScreen));
	if(!s) subLogError("Can't alloc memory. Exhausted?\n");

	snprintf(buf, sizeof(buf), "%d", size + 1);
	s->name = (char *)calloc(strlen(buf) + 1, sizeof(char));
	strncpy(s->name, buf, strlen(buf));

	s->w				= subTileNew(SUB_WIN_TILE_HORZ);
	s->w->flags |= SUB_WIN_TYPE_SCREEN;
	s->width		= strlen(s->name) * d->fx + 2;
	s->button		= XCreateSimpleWindow(d->dpy, d->bar.screens, 0, 0, 1, d->th - 6, 1, 
		d->colors.border, d->colors.norm);

	screens = (SubScreen **)realloc(screens, sizeof(SubScreen *) * (size + 1));
	if(!screens) subLogError("Can't alloc memory. Exhausted?\n");

	screens[size] = s;

	XMapRaised(d->dpy, s->button);

	/* EWMH WM information */
	if(size == 0)
		{
			long info[4] = { 0, 0, 0, 0 };

			subEwmhSetWindow(DefaultRootWindow(d->dpy), SUB_EWMH_NET_SUPPORTING_WM_CHECK, screens[0]->w->frame);
			subEwmhSetString(screens[0]->w->frame, SUB_EWMH_NET_WM_NAME, PACKAGE_STRING);
			subEwmhSetCardinal(screens[0]->w->frame, SUB_EWMH_NET_WM_PID, (long)getpid());
			subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_DESKTOP_VIEWPORT, (long *)&info, 2);

			info[0] = DisplayWidth(d->dpy, DefaultScreen(d->dpy));
			info[1] = DisplayHeight(d->dpy, DefaultScreen(d->dpy));
			subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&info, 2);

			info[2] = info[0]; info[3] = info[1];
			info[0] = 0; info[1] = 0;
			subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_WORKAREA, (long *)&info, 4);
		}
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, size + 1);
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, size + 1);

	wins = (Window *)calloc(size + 1, sizeof(Window));
	if(!wins) subLogError("Can'b alloc memory. Exhausted?\n");
	for(i = 0; i < size; i++)
		{
			wins[i] = screens[i]->w->frame;
		}
	subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_VIRTUAL_ROOTS, wins, size + 1);
	free(wins);

	/* Switch to new screen */
	if(active >= 0) subWinUnmap(screens[active]->w);
	XMapRaised(d->dpy, d->bar.win);
	XSetInputFocus(d->dpy, screens[size]->w->win, RevertToNone, CurrentTime);

	active = size;
	size++;

	subScreenConfigure();
	printf("Adding screen (%d)\n", size);
} /* }}} */

 /**
	* Delete a screen window {{{
	* @param w A #SubWin
	**/

void
subScreenDelete(SubWin *w)
{
	int i, j;

	for(i = 0; i < size; i++) 	
		if(screens[i]->w == w)
			{
				XUnmapWindow(d->dpy, screens[i]->button);
				XDestroyWindow(d->dpy, screens[i]->button);
				subTileDelete(screens[i]->w);
				free(screens[i]->name);

				for(j = i; j < size; j++, i++) screens[i] = screens[j];
			}

	screens = (SubScreen **)realloc(screens, sizeof(SubScreen *) * size);
	if(!screens) subLogError("Can't alloc memory. Exhausted?\n");

	size--;
	if(size == 0) raise(SIGTERM);

	active = size;
	subWinMap(screens[active]->w);

	subScreenConfigure();
} /* }}} */

 /**
	* Render a screen {{{
	* @param mode Render mode
	* @param w A #SubWin
	**/

void
subScreenRender(short mode,
	SubWin *w)
{
	if(w)
		{
			int i;
			unsigned int n = 0;
			Window nil, *wins = NULL;
			unsigned long col = mode ? d->colors.norm : d->colors.focus;

			if(w->flags & SUB_WIN_TYPE_SCREEN)
				{
					XSetWindowBackground(d->dpy, d->bar.screens, col);
					XClearWindow(d->dpy, d->bar.screens);

					/* Screens */
					for(i = 0; i < size; i++)
						{
							/* Hilight active screen */
							if(active == i) col = d->colors.cover;
							XSetWindowBackground(d->dpy, screens[i]->button, col);
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
									if(s) subSubletRender(mode, s);
								}
							XFree(wins);
						}					
				}
		}
} /* }}} */

 /**
	* Configure a #SubScreen {{{
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
} /* }}} */

 /**
	* Kill screens {{{
	**/

void
subScreenKill(void)
{
	int i;

	if(screens)
		{
			for(i = 0; i < size; i++) 
				{
					subTileDelete(screens[i]->w);
					XDestroyWindow(d->dpy, screens[i]->button);
					free(screens[i]->name);
				}

			XDestroyWindow(d->dpy, d->bar.screens);
			XDestroyWindow(d->dpy, d->bar.sublets);
			free(screens);
		}
} /* }}} */

 /**
	* Switch to screen {{{
	* @param dir Switch direction
	**/

void
subScreenSwitch(int dir)
{
	if(dir > 0) subScreenNew();
} /* }}} */
