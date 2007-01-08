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
	screen->wins = (SubWin **)realloc(screen->wins, sizeof(SubScreen *) * (screen->n + 1));
	if(!screen->wins) subLogError("Can't alloc memory. Exhausted?\n");
	if(screen->active) subWinUnmap(screen->active);

	screen->wins[screen->n] = subTileNewHoriz();
	screen->wins[screen->n]->prop |= SUB_WIN_SCREEN;

	screen->active = screen->wins[screen->n];
	screen->n++;
	XMapRaised(d->dpy, screen->statusbar);

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

	if(screen->n == 0) raise(SIGINT);

	screen->active = screen->wins[screen->n - 1];
	subWinMap(screen->active);
}

void
subScreenKill(void)
{
	int i;
	
	for(i = 0; i < screen->n; i++) subTileDelete(screen->wins[i]);
	XDestroyWindow(d->dpy, screen->statusbar);
	free(screen->wins);
	free(screen);
}

void
subScreenShift(int dir)
{
}
	
