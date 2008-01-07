
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Header$
	**/

#include "subtle.h"

 /**
	* Create a new window and append it to the window list 
	* @param win A #Window
	* @return Return a #SubWin on success.
	**/

SubWin *
subWinNew(void)
{
	XSetWindowAttributes attrs;
	SubWin *w = (SubWin *)subUtilAlloc(1, sizeof(SubWin));

	/* Init with real values */
	w->x 			= 0;
	w->y 			= d->th;
	w->width	= DisplayWidth(d->disp, DefaultScreen(d->disp));
	w->height	= DisplayHeight(d->disp, DefaultScreen(d->disp)) - d->th;

	/* Create frame window */
	attrs.background_pixmap	= ParentRelative;
	attrs.save_under				= False;
	attrs.event_mask				= KeyPressMask|ButtonPressMask|EnterWindowMask|LeaveWindowMask|ExposureMask|
		VisibilityChangeMask|FocusChangeMask|SubstructureRedirectMask|SubstructureNotifyMask;

	w->frame = SUBWINNEW(DefaultRootWindow(d->disp), w->x, w->y, w->width, w->height, 0, 
		CWBackPixmap|CWSaveUnder|CWEventMask);

	return(w);
}

 /**
	* Delete wrapper 
	* @param w A #SubWin
	**/

void
subWinDelete(SubWin *w)
{
	SubWin *p = NULL;

	assert(w);

	subWinUnmap(w);

	/* Ignore further events */
	XSelectInput(d->disp, w->frame, NoEventMask);

	/* Check window flags */
	if(w->flags & SUB_WIN_TYPE_TILE) subTileDelete(w);
	else if(w->flags & SUB_WIN_TYPE_CLIENT) subClientDelete(w);	

	p = w->parent;
	subWinUnlink(w);
	subWinDestroy(w);

	if(p) subTileConfigure(p);
}

	/**
	 * Destroy data
	 * @param w A #SubWin
	 **/

void
subWinDestroy(SubWin *w)
{
	assert(w);

	XDestroySubwindows(d->disp, w->frame);
	XDestroyWindow(d->disp, w->frame);

	free(w);
}

 /**
	* Render wrapper
	* @param w A #SubWin
	**/

void
subWinRender(SubWin *w)
{
	assert(w);

	/* Check if window was meanwhile destroyed */
	if(w->flags & SUB_WIN_STATE_DEAD) return;

	/* Check window flags */
	if(w->flags & SUB_WIN_TYPE_VIEW) subViewRender();
	else if(w->flags & SUB_WIN_TYPE_CLIENT) subClientRender(w);	
}

 /**
	* Cut a window from the list
	* @param w A #SubWin
	**/

void
subWinUnlink(SubWin *w)
{
	assert(w);

	if(w->parent && w->parent->tile->first == w) w->parent->tile->first = w->next;
	if(w->prev) w->prev->next = w->next;
	if(w->next) w->next->prev = w->prev;
	if(w->parent && w->parent->tile->last == w) w->parent->tile->last = w->prev;

	w->next		= NULL;
	w->prev		= NULL;
	w->parent	= NULL;
}

 /**
	* Prepend window w2 before window w1
	* @param w1 A #SubWin
	* @param w2 A #SubWin
	**/

void
subWinPrepend(SubWin *w1,
	SubWin *w2)
{
	assert(w1 && w2);

	w2->prev = w1->prev;
	if(w1->prev) w1->prev->next = w2;
	w1->prev = w2;
	w2->next = w1;
	if(w1->parent && w1->parent->tile->first == w1) w1->parent->tile->first = w2;
	w2->parent = w1->parent;

	XReparentWindow(d->disp, w2->frame, w1->parent->frame, 0, 0); 
}

 /**
	* Append window w2 after window w1
	* @param w1 A #SubWin
	* @param w2 A #SubWin
	**/

void
subWinAppend(SubWin *w1,
	SubWin *w2)
{
	assert(w1 && w2);

	w2->next = w1->next;
	if(w1->next) w1->next->prev = w2;
	w1->next = w2;
	w2->prev = w1;
	if(w1->parent && w1->parent->tile->last == w1) w1->parent->tile->last = w2;
	w2->parent = w1->parent;

	XReparentWindow(d->disp, w2->frame, w1->parent->frame, 0, 0); 
}

 /** 
	* Replace first with second window
	* @param w1 A #SubWin
	* @param w2 A #SubWin
	**/

void
subWinReplace(SubWin *w1,
	SubWin *w2)
{
	assert(w1 && w2);

	if(w1->parent->tile->first == w1) w1->parent->tile->first	= w2;
	if(w1->parent->tile->last == w1) w1->parent->tile->last		= w2;

	w2->prev		= w1->prev;
	w2->next		= w1->next;
	w2->parent	= w1->parent;

	if(w1->prev) w1->prev->next = w2;
	if(w1->next) w1->next->prev = w2;

	w1->prev		= NULL;
	w1->next		= NULL;
	w1->parent = NULL;

	XReparentWindow(d->disp, w2->frame, w2->parent->frame, 0, 0);
}

 /**
	* Swap two windows
	* @param w1 A #SubWin
	* @param w2 A #SubWin
	**/

void
subWinSwap(SubWin *w1,
	SubWin *w2)
{
	SubWin *swap1 = NULL, *swap2 = NULL;

	assert(w1 && w2);

	swap1 = w2->prev;
	swap2 = w2->next;

	w2->prev = w1->prev;
	if(w1->prev && w1->prev != w2) w1->prev->next = w2;
	if(w1->next && w1->next != w2) w1->next->prev = w2;
	w2->next = w1->next;

	w1->prev = swap1;
	if(swap1 && swap1 != w1) swap1->next = w1;
	if(swap2 && swap2 != w1) swap2->prev = w1;
	w1->next = swap2;

	/* Check if both windows are neighbours */
	if(w1->prev == w1)    w1->prev   = w2;
	if(w1->next == w1)    w1->next   = w2;
	if(w2->prev == w2)  w2->prev  = w1;
	if(w2->next == w2)  w2->next  = w1;

	/* Check first and last child (1)*/
	if(w1->parent->tile->first == w1)   w1->parent->tile->first  = w2;
	if(w1->parent->tile->last == w1)    w1->parent->tile->last   = w2;
				
	/* Skip if parents are equal */
	if(w1->parent != w2->parent)
		{
			/* Check first and last child (2)*/
			if(w2->parent->tile->first == w2) w2->parent->tile->first = w1;
			if(w2->parent->tile->last == w2)  w2->parent->tile->last	= w1;

			XReparentWindow(d->disp, w1->frame, w2->parent->frame, 0, 0);
			XReparentWindow(d->disp, w2->frame, w1->parent->frame, 0, 0);

			swap1 = w1->parent; w1->parent = w2->parent; w2->parent = swap1;

			subTileConfigure(w2->parent);
		}
	else
		{
			/* Check first and last child (3) */
			if(w1->parent->tile->first == w2)   w1->parent->tile->first  = w1;
			if(w1->parent->tile->last == w2)    w1->parent->tile->last   = w1;
		}
	subTileConfigure(w1->parent);
}

 /**
	* Change the parent of a window
	* @param p A #SubWin
	* @param w A #SubWin
	**/

void
subWinReparent(SubWin *p,
	SubWin *w)
{
	assert(p && w);

	/* Hierarchy */
	if(w->parent && w->parent->tile->first == w)
		{
			w->parent->tile->first = w->next;
			if(w->next) w->next->prev = NULL;
		}
	else if(w->prev && w->next)
		{
			w->prev->next = w->next;
			w->next->prev = w->prev;
		}
	else if(w->parent)
		{
			w->parent->tile->last = w->prev;
			if(w->prev) w->prev->next = NULL;
		}

	if(w->parent) subTileConfigure(w->parent);
	w->parent = p;

	XReparentWindow(d->disp, w->frame, p->frame, 0, 0);
}

 /**
	* Set focus
	* @param w A #SubWin
	**/

void
subWinFocus(SubWin *w)
{
	assert(w);

	if(w->flags & SUB_WIN_PREF_FOCUS && !(w->flags & SUB_WIN_STATE_SHADE))
		{
			XEvent ev;

			ev.type									= ClientMessage;
			ev.xclient.window				= w->client->win;
			ev.xclient.message_type = subEwmhFind(SUB_EWMH_WM_PROTOCOLS);
			ev.xclient.format				= 32;
			ev.xclient.data.l[0]		= subEwmhFind(SUB_EWMH_WM_TAKE_FOCUS);
			ev.xclient.data.l[1]		= CurrentTime;

			XSendEvent(d->disp, w->client->win, False, NoEventMask, &ev);

			subUtilLogDebug("Focus: win=%#lx, input=%d, send=%d\n", w->client->win, 
				!!(w->flags & SUB_WIN_PREF_INPUT), !!(w->flags & SUB_WIN_PREF_FOCUS));
		}
	else
		{
			/* Remove focus from window */
			if(d->focus && d->focus != w) 
				{
					SubWin *f = d->focus;
					d->focus = NULL;
					if(f) subWinRender(f);

					subKeyUngrab(f);
				}

			XSetInputFocus(d->disp, w->frame, RevertToNone, CurrentTime);			

			d->focus = w;
			subWinRender(w);
			subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_ACTIVE_WINDOW, &w->frame, 1);

			subKeyGrab(w);
		}
}

 /**
	* Resize the client window 
	* @param w A #SubWin
	**/

void
subWinResize(SubWin *w)
{
	assert(w);

	XMoveResizeWindow(d->disp, w->frame, w->x, w->y, w->width, (w->flags & SUB_WIN_STATE_SHADE) ? d->th : w->height);

	if(w->flags & SUB_WIN_TYPE_CLIENT)
		{
			if(w->flags & SUB_WIN_STATE_FULL) XMoveResizeWindow(d->disp, w->client->win, 0, 0, w->width, w->height);
			else if(!(w->flags & SUB_WIN_STATE_SHADE))
				{
					XMoveResizeWindow(d->disp, w->client->win, d->bw, d->th, w->width - 2 * d->bw, w->height - d->th - d->bw);
					XMoveResizeWindow(d->disp, w->client->right, w->width - d->bw, d->th, d->bw, w->height - d->th);
					XMoveResizeWindow(d->disp, w->client->bottom, 0, w->height - d->bw, w->width, d->bw);
				}
			else XMoveResizeWindow(d->disp, w->client->title, 0, 0, w->width, d->th);
			if(!(w->flags & SUB_WIN_STATE_SHADE)) subClientConfigure(w);
		}
}

 /**
	* Map a window to the screen 
	* @param w A #SubWin
	**/

void
subWinMap(SubWin *w)
{
	assert(w);

	XMapSubwindows(d->disp, w->frame);
	XMapWindow(d->disp, w->frame);
}

 /**
	* Unmap a window 
	* @param w A #SubWin
	**/

void
subWinUnmap(SubWin *w)
{
	assert(w);

	XUnmapSubwindows(d->disp, w->frame);
	XUnmapWindow(d->disp, w->frame);
}
