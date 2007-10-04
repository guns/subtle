
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

 /**
	* Find a window via Xlib context manager 
	* @param win A #Window
	* @return Return the #SubWin associated with the window or NULL
	**/

SubWin *
subWinFind(Window win)
{
	SubWin *w = NULL;
	return(XFindContext(d->dpy, win, 1, (void *)&w) != XCNOENT ? w : NULL);
}

 /**
	* Create a new window and append it to the window list 
	* @param win A #Window
	* @return Return a #SubWin on success.
	**/

SubWin *
subWinNew(void)
{
	long mask = 0;
	XSetWindowAttributes attrs;
	SubWin *w = (SubWin *)subUtilAlloc(1, sizeof(SubWin));

	/* Init with real values */
	w->x 			= 0;
	w->y 			= d->th;
	w->width	= DisplayWidth(d->dpy, DefaultScreen(d->dpy));
	w->height	= DisplayHeight(d->dpy, DefaultScreen(d->dpy)) - d->th;

	/* Create frame window */
	attrs.background_pixmap	= ParentRelative;
	attrs.save_under				= False;
	attrs.event_mask				= KeyPressMask|ButtonPressMask|EnterWindowMask|LeaveWindowMask|ExposureMask|
		VisibilityChangeMask|FocusChangeMask|SubstructureRedirectMask|SubstructureNotifyMask;

	mask			= CWBackPixmap|CWSaveUnder|CWEventMask;
	w->frame	= SUBWINNEW(DefaultRootWindow(d->dpy), w->x, w->y, w->width, w->height, 0);

	XSaveContext(d->dpy, w->frame, 1, (void *)w);
	return(w);
}

 /**
	* Delete wrapper 
	* @param w A #SubWin
	**/

void
subWinDelete(SubWin *w)
{
	if(w)
		{
			SubWin *p = w->parent;

			/* Check window flags */
			if(w->flags & SUB_WIN_TYPE_SCREEN) subViewDelete(w);
			else if(w->flags & SUB_WIN_TYPE_TILE) subTileDelete(w);
			else if(w->flags & SUB_WIN_TYPE_CLIENT) subClientDelete(w);	

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

			XDeleteContext(d->dpy, w->frame, 1);
			XDestroySubwindows(d->dpy, w->frame);
			XDestroyWindow(d->dpy, w->frame);

			if(p) subTileConfigure(p);
			free(w);
		}
}

 /**
	* Render wrapper
	* @param w A #SubWin
	**/

void
subWinRender(SubWin *w)
{
	if(w)
		{
			/* Check window flags */
			if(w->flags & SUB_WIN_TYPE_SCREEN) subViewRender(w);
			else if(w->flags & SUB_WIN_TYPE_CLIENT) subClientRender(w);	
		}
}

 /**
	* Cut a window from the list
	* @param w A #SubWin
	**/

void
subWinCut(SubWin *w)
{
	if(w)
		{
			if(w->parent->tile->first == w) w->parent->tile->first = w->next;
			if(w->prev) w->prev->next = w->next;
			if(w->next) w->next->prev = w->prev;
			if(w->parent->tile->last == w) w->parent->tile->last = w->prev;

			w->next		= NULL;
			w->prev		= NULL;
			w->parent	= NULL;
		}
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
	if(w1 && w2)
		{
			w2->prev = w1->prev;
			if(w1->prev) w1->prev->next = w2;
			w1->prev = w2;
			w2->next = w1;
			if(w1->parent && w1->parent->tile->first == w1) w1->parent->tile->first = w2;
			w2->parent = w1->parent;

			XReparentWindow(d->dpy, w2->frame, w1->parent->frame, 0, w1->parent->flags & SUB_WIN_TYPE_SCREEN ? d->th : 0); 
		}
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
	if(w1 && w2)
		{
			w2->next = w1->next;
			if(w1->next) w1->next->prev = w2;
			w1->next = w2;
			w2->prev = w1;
			if(w1->parent && w1->parent->tile->last == w1) w1->parent->tile->last = w2;
			w2->parent = w1->parent;

			XReparentWindow(d->dpy, w2->frame, w1->parent->frame, 0, w1->parent->flags & SUB_WIN_TYPE_SCREEN ? d->th : 0); 
		}
}

 /** 
	* Replace first with second window
	* @param w A #SubWin
	* @param w2 A #SubWin
	**/

void
subWinReplace(SubWin *w,
	SubWin *w2)
{
	if(w && w2)
		{
			if(w->parent->tile->first == w) w->parent->tile->first	= w2;
			if(w->parent->tile->last == w) w->parent->tile->last		= w2;

			w2->prev		= w->prev;
			w2->next		= w->next;
			w2->parent	= w->parent;

			if(w->prev) w->prev->next = w2;
			if(w->next) w->next->prev = w2;

			w->prev		= NULL;
			w->next		= NULL;
			w->parent = NULL;

			XReparentWindow(d->dpy, w2->frame, w2->parent->frame, 0, 0);
		}
}

 /**
	* Swap two windows
	* @param a A #SubWin
	* @param b A #SubWin
	**/

void
subWinSwap(SubWin *w,
	SubWin *w2)
{
	if(w && w2)
		{
			SubWin *swap1 = NULL, *swap2 = NULL;

			swap1 = w2->prev;
			swap2 = w2->next;

			w2->prev = w->prev;
			if(w->prev && w->prev != w2) w->prev->next = w2;
			if(w->next && w->next != w2) w->next->prev = w2;
			w2->next = w->next;

			w->prev = swap1;
			if(swap1 && swap1 != w) swap1->next = w;
			if(swap2 && swap2 != w) swap2->prev = w;
			w->next = swap2;

			/* Check if both windows are neighbours */
			if(w->prev == w)    w->prev   = w2;
			if(w->next == w)    w->next   = w2;
			if(w2->prev == w2)  w2->prev  = w;
			if(w2->next == w2)  w2->next  = w;

			/* Check first and last child (1)*/
			if(w->parent->tile->first == w)   w->parent->tile->first  = w2;
			if(w->parent->tile->last == w)    w->parent->tile->last   = w2;
						
			/* Skip if parents are equal */
			if(w->parent != w2->parent)
				{
					/* Check first and last child (2)*/
					if(w2->parent->tile->first == w2) w2->parent->tile->first = w;
					if(w2->parent->tile->last == w2)  w2->parent->tile->last	= w;

					XReparentWindow(d->dpy, w->frame, w2->parent->frame, 0, 0);
					XReparentWindow(d->dpy, w2->frame, w->parent->frame, 0, 0);

					swap1 = w->parent; w->parent = w2->parent; w2->parent = swap1;

					subTileConfigure(w2->parent);
				}
			else
				{
					/* Check first and last child (3) */
					if(w->parent->tile->first == w2)   w->parent->tile->first  = w;
					if(w->parent->tile->last == w2)    w->parent->tile->last   = w;
				}
			subTileConfigure(w->parent);
		}
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
	if(p && w)
		{
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

			XReparentWindow(d->dpy, w->frame, p->frame, 0, 0);
		}
}

 /**
	* Set focus
	* @param w A #SubWin
	**/

void
subWinFocus(SubWin *w)
{
	if(w)
		{
			if(w->flags & SUB_WIN_PREF_FOCUS && !(w->flags & SUB_WIN_STATE_COLLAPSE))
				{
					XEvent ev;

					ev.type									= ClientMessage;
					ev.xclient.window				= w->client->win;
					ev.xclient.message_type = subEwmhGetAtom(SUB_EWMH_WM_PROTOCOLS);
					ev.xclient.format				= 32;
					ev.xclient.data.l[0]		= subEwmhGetAtom(SUB_EWMH_WM_TAKE_FOCUS);
					ev.xclient.data.l[1]		= CurrentTime;

					XSendEvent(d->dpy, w->client->win, False, NoEventMask, &ev);

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

					XSetInputFocus(d->dpy, w->frame, RevertToNone, CurrentTime);			

					d->focus = w;
					subWinRender(w);
					subEwmhSetWindow(DefaultRootWindow(d->dpy), SUB_EWMH_NET_ACTIVE_WINDOW, w->frame);

					subKeyGrab(w);
				}
		}
}

 /**
	* Resize the client window 
	* @param w A #SubWin
	**/

void
subWinResize(SubWin *w)
{
	if(w)
		{
			XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, (w->flags & SUB_WIN_STATE_COLLAPSE) ? d->th : w->height);

			if(w->flags & SUB_WIN_TYPE_CLIENT)
				{
					if(w->flags & SUB_WIN_STATE_FULL) XMoveResizeWindow(d->dpy, w->client->win, 0, 0, w->width, w->height);
					else if(!(w->flags & SUB_WIN_STATE_COLLAPSE))
						{
							XMoveResizeWindow(d->dpy, w->client->win, d->bw, d->th, w->width - 2 * d->bw, w->height - d->th - d->bw);
							XMoveResizeWindow(d->dpy, w->client->right, w->width - d->bw, d->th, d->bw, w->height - d->th);
							XMoveResizeWindow(d->dpy, w->client->bottom, 0, w->height - d->bw, w->width, d->bw);
						}
					else XMoveResizeWindow(d->dpy, w->client->title, 0, 0, w->width, d->th);

					if(!(w->flags & SUB_WIN_STATE_COLLAPSE)) subClientConfigure(w);
				}
		}
}

 /**
	* Map a window to the screen 
	* @param w A #SubWin
	**/

void
subWinMap(SubWin *w)
{
	if(w)
		{
			XMapSubwindows(d->dpy, w->frame);
			XMapWindow(d->dpy, w->frame);
		}
}

 /**
	* Unmap a window 
	* @param w A #SubWin
	**/

void
subWinUnmap(SubWin *w)
{
	if(w) XUnmapWindow(d->dpy, w->frame);
}
