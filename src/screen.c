#include "subtle.h"

SubScreen *screen = NULL;

void
subScreenNew(void)
{
	XSetWindowAttributes attrs;
	long mask = CWOverrideRedirect|CWBackPixel|CWEventMask;

	attrs.override_redirect	= True;
	attrs.background_pixel	= d->colors.norm;
	attrs.event_mask				= ExposureMask|VisibilityChangeMask;

	screen = (SubScreen *)calloc(1, sizeof(SubScreen));
	if(!screen) subLogError("Can't alloc memory. Exhausted?\n");
	screen->statusbar = SUBWINNEW(DefaultRootWindow(d->dpy), 1, 0, 5, d->th, 0);
	XMapRaised(d->dpy, screen->statusbar);
}

void
subScreenAdd(void)
{
	int i;
	Window *wins;
	screen->wins = (SubWin **)realloc(screen->wins, sizeof(SubScreen *) * (screen->n + 1));
	if(!screen->wins) subLogError("Can't alloc memory. Exhausted?\n");
	if(screen->active) subWinUnmap(screen->active);

	screen->wins[screen->n] = subTileNewHoriz();
	screen->wins[screen->n]->prop |= SUB_WIN_SCREEN;

	/* EWMH WM information */
	if(screen->n == 0)
		{
			long info[4] = { 0, 0, 0, 0 };

			subEwmhSetWindow(DefaultRootWindow(d->dpy), SUB_EWMH_NET_SUPPORTING_WM_CHECK, 
				screen->wins[0]->frame);
			subEwmhSetString(screen->wins[0]->frame, SUB_EWMH_NET_WM_NAME, PACKAGE_STRING);
			subEwmhSetCardinal(screen->wins[0]->frame, SUB_EWMH_NET_WM_PID, (long)getpid());
			subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_DESKTOP_VIEWPORT, (long *)&info, 2);

			info[0] = DisplayWidth(d->dpy, DefaultScreen(d->dpy));
			info[1] = DisplayHeight(d->dpy, DefaultScreen(d->dpy));
			subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&info, 2);

			info[2] = info[0]; info[3] = info[1];
			info[0] = 0; info[1] = 0;
			subEwmhSetCardinals(DefaultRootWindow(d->dpy), SUB_EWMH_NET_WORKAREA, (long *)&info, 4);
		}
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_NUMBER_OF_DESKTOPS, screen->n + 1);
	subEwmhSetCardinal(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CURRENT_DESKTOP, screen->n);

	wins	= (Window *)calloc(screen->n + 1, sizeof(Window));
	if(!wins) subLogError("Can'b alloc memory. Exhausted?\n");
	for(i = 0; i < screen->n; i++)
		{
			wins[i] = screen->wins[i]->frame;
		}
	subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_VIRTUAL_ROOTS, wins, screen->n);
	free(wins);

	screen->active = screen->wins[screen->n];
	XMapRaised(d->dpy, screen->statusbar);
	XSetInputFocus(d->dpy, screen->wins[screen->n]->win, RevertToNone, CurrentTime);

	screen->n++;

	subLogDebug("Adding screen (%d)\n", screen->n);
}

void
subScreenDelete(SubWin *w)
{
	int i, j;

	for(i = 0; i < screen->n; i++) 	
		if(screen->wins[i] == w)
			for(j = i; j < screen->n; j++, i++)
				screen->wins[i] = screen->wins[j];

	subTileDelete(w);

	screen->wins = (SubWin **)realloc(screen->wins, sizeof(SubScreen *) * screen->n);
	if(!screen->wins) subLogError("Can't alloc memory. Exhausted?\n");
	screen->n--;

	if(screen->n == 0) raise(SIGTERM);

	screen->active = screen->wins[screen->n - 1];
	subWinMap(screen->active);
}

void
subScreenKill(void)
{
	int i;

	if(screen)
		{
			for(i = 0; i < screen->n; i++) subTileDelete(screen->wins[i]);
			XDestroyWindow(d->dpy, screen->statusbar);
			free(screen->wins);
			free(screen);
		}
}

void
subScreenShift(int dir)
{
}
	
