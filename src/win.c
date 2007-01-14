#include "subtle.h"

 /**
	* Find a window via Xlib context manager.
	* @param win A #Window
	* @return Returns a #SubWin on success or otherwise NULL.
	**/

SubWin *
subWinFind(Window win)
{
	SubWin *w = NULL;
	return(XFindContext(d->dpy, win, 1, (void *)&w) != XCNOENT ? w : NULL);
}

 /**
	* Create a new window and append it to the window list.
	* @param win A #Window
	* @return Returns a #SubWin on success.
	**/

SubWin *
subWinNew(Window win)
{
	long mask = 0;
	XSetWindowAttributes attrs;
	SubWin *w = (SubWin *)calloc(1, sizeof(SubWin));
	if(!w) subLogError("Can't alloc memory. Exhausted?\n");

	/* Init with real values */
	w->width	= DisplayWidth(d->dpy, DefaultScreen(d->dpy));
	w->height	= DisplayHeight(d->dpy, DefaultScreen(d->dpy));

	attrs.override_redirect			= True;
	attrs.border_pixel					= d->colors.border;
	attrs.background_pixmap			= ParentRelative;
	attrs.background_pixel			= d->colors.norm;
	attrs.do_not_propagate_mask	=	ButtonPressMask|ButtonMotionMask|KeyPressMask;
	attrs.event_mask						= ButtonPressMask|ButtonMotionMask|KeyPressMask|EnterWindowMask|LeaveWindowMask|
		ExposureMask|VisibilityChangeMask|SubstructureRedirectMask|SubstructureNotifyMask;

	/* Create windows */
	mask			= CWOverrideRedirect|CWBackPixmap|CWDontPropagate|CWEventMask;
	w->frame	= SUBWINNEW(DefaultRootWindow(d->dpy), 0, 0, w->width, w->height, 0);
	
	mask			= CWOverrideRedirect|CWBackPixel;
	w->title	= SUBWINNEW(w->frame, 0, 0, w->width, d->th, 0);

	/* Create buttons */
	mask 			= CWOverrideRedirect|CWBorderPixel|CWBackPixel;
	w->icon		= SUBWINNEW(w->frame, 2, 2, d->th - 6, d->th - 6, 1);
	
	/* Create borders */
	mask 					|= CWCursor;
	attrs.cursor	= d->cursors.left;
	w->left				= SUBWINNEW(w->frame, 0, d->th, d->bw, w->height - d->th, 0);
	attrs.cursor	= d->cursors.right;
	w->right			= SUBWINNEW(w->frame, w->width - d->bw, d->th, d->bw, w->height - d->th, 0);
	attrs.cursor	= d->cursors.bottom;
	w->bottom			= SUBWINNEW(w->frame, 0, w->height - d->bw, w->width, d->bw, 0);
	
	if(!win)
		{
			mask = CWOverrideRedirect|CWBackPixmap;
			w->win = SUBWINNEW(w->frame, d->bw, d->th, w->width - 2 * d->bw, w->height - d->th - d->bw, 0);
		}
	else
		{
			w->win = win;
			XSetWindowBorderWidth(d->dpy, w->win, 0);
			XReparentWindow(d->dpy, w->win, w->frame, d->bw, d->th);
		}

	XSaveContext(d->dpy, w->frame, 1, (void *)w);
	return(w);
}

 /**
	* Delete a window.
	* @param w A #SubWin
	**/

void
subWinDelete(SubWin *w)
{
	/* Focus */
	if(w->parent) d->focus = w->parent->win;
	subWinRender(1, w->parent);

	XDeleteContext(d->dpy, w->frame, 1);
	XDestroySubwindows(d->dpy, w->frame);
	XDestroyWindow(d->dpy, w->frame);
	free(w);
}

 /**
	* Render a window.
	* @param mode Render mode
	* @param w A #SubWin
	**/

void
subWinRender(short mode,
	SubWin *w)
{
	unsigned long col = mode ? d->colors.norm : d->colors.act;

	/* Update color */
	XSetWindowBackground(d->dpy, w->title,	col);
	XSetWindowBackground(d->dpy, w->icon,		col);
	XSetWindowBackground(d->dpy, w->icon,		col);
	XSetWindowBackground(d->dpy, w->left, 	col);
	XSetWindowBackground(d->dpy, w->right, 	col);
	XSetWindowBackground(d->dpy, w->bottom,	col);

	/* Clear windows */
	XClearWindow(d->dpy, w->title);
	XClearWindow(d->dpy, w->icon);
	XClearWindow(d->dpy, w->left);
	XClearWindow(d->dpy, w->right);
	XClearWindow(d->dpy, w->bottom);

	/* Titlebar and buttons */
	XFillRectangle(d->dpy, w->title, d->gcs.border, d->th + 1, 2, w->width - d->th - 4, d->th - 4);	
	XDrawString(d->dpy, w->icon, d->gcs.font, (d->th - 8 - d->fx) / 2 + 1, d->fy - 5, "s", 1);
}

static void
DrawOutline(int mode,
	int x,
	int y,
	SubWin *w)
{
	char buf[32];
	Window nil;
	int rx, ry, len;
    
	XTranslateCoordinates(d->dpy, w->frame, DefaultRootWindow(d->dpy), 
		x, y, &rx, &ry, &nil);
	snprintf(buf, sizeof(buf), "%dx%d+%d+%d", w->width, w->height, rx, ry);
	printf("x=%3d, y=%3d, wx=%3d, wy=%3d, rx=%3d, ry=%3d\n", x, y, w->x, w->y, rx, ry);

	XDrawRectangle(d->dpy, DefaultRootWindow(d->dpy), d->gcs.invert,
		rx, ry, w->width, w->height);
}

static int
GetPointer(Window *win, 
	int *x,
  int *y)
{ 
  Window root;
	unsigned int unused;
  
  return(XQueryPointer(d->dpy, DefaultRootWindow(d->dpy) , &root, win, x, y, 
		&unused, &unused, &unused));
}

 /**
	* Movie/resize the window.
	* @param mode Attach mode
	* @param w A #SubWin
	**/

void
subWinDrag(short mode,
	SubWin *w)
{
	XEvent ev;
	Window win;
	SubWin *w2 = NULL, *p = NULL, *p2 = NULL;
	int mx = 0, my = 0, nx = w->x, ny = w->y, sw = w->width, sh = w->height;

	if(!XGrabPointer(d->dpy, w->frame, True, POINTER_MASK, GrabModeAsync, 
			GrabModeAsync, None, d->cursors.square, CurrentTime) == GrabSuccess)
		return;

	XGrabServer(d->dpy);
	GetPointer(&win, &mx, &my);
	for(;;)
		{
			XMaskEvent(d->dpy, (PointerMotionMask|ButtonReleaseMask|EnterWindowMask), &ev);
			switch(ev.type)
				{
					/* FIXME: ButtonRelease events doesn't return the real destination window */
					case EnterNotify:
						win = ev.xcrossing.window;
						break;
					case MotionNotify:
						nx = w->x + (ev.xmotion.x - mx);
						ny = w->y + (ev.xmotion.y - my);
						break;
					case ButtonRelease:
						/* Prevent from unneccessary config loops whithin double clicks */
						if(nx != w->x && ny != w->y)
							{ 
								w2 = subWinFind(win);
						
								if(w && w2 && w->parent && w2->parent)
									{
										/* Append a window to tile (* to tile) */
										if((mode || w->prop & SUB_WIN_CLIENT) && w2->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV))
											{
												subTileAdd(w2, w);
												subTileConfigure(w2->parent);
												if(mode) subTileConfigure(w->parent);
											}
										/* Swap two windows manually */
										else
											{
												subTileAdd(w->parent, w2);
												subTileAdd(w2->parent, w);

												/* Take care of shaded windows */
												if(w->prop & SUB_WIN_SHADED && !(w2->prop & SUB_WIN_SHADED)) 
													{
														subWinShade(w);
														subWinShade(w2);
													}
												else subTileConfigure(w2->parent);
												if(w2->prop & SUB_WIN_SHADED && !(w->prop & SUB_WIN_SHADED))
													{
														subWinShade(w2);
														subWinShade(w);
													}
												else subTileConfigure(w->parent);
											}
									}
							}
												
						XUngrabServer(d->dpy);
						XUngrabPointer(d->dpy, CurrentTime);
						return;
				}
		}
}

 /**
	* Shade/Unshade a window.
	* @param w A #SubWin
	**/

void
subWinShade(SubWin *w)
{
	if(w && w->parent)
		{
			XEvent event;
			XGrabServer(d->dpy);
			if(w->prop & SUB_WIN_SHADED)
				{
					w->prop &= ~SUB_WIN_SHADED;
					(w->parent->tile->shaded)--;

					/* Set state */
					if(w->prop & SUB_WIN_CLIENT) subClientSetWMState(w, NormalState);

					/* Map most of the windows */
					XMapWindow(d->dpy, w->win);
					XMapWindow(d->dpy, w->left);
					XMapWindow(d->dpy, w->right);
					XMapWindow(d->dpy, w->bottom);
				}
			else 
				{
					w->prop	|= SUB_WIN_SHADED;
					(w->parent->tile->shaded)++;

					/* Set state */
					if(w->prop & SUB_WIN_CLIENT) subClientSetWMState(w, WithdrawnState);

					/* Unmap most of the windows */
					XUnmapWindow(d->dpy, w->win);
					XUnmapWindow(d->dpy, w->left);
					XUnmapWindow(d->dpy, w->right);
					XUnmapWindow(d->dpy, w->bottom);
				}
			XUngrabServer(d->dpy);
			while(XCheckTypedEvent(d->dpy, UnmapNotify, &event));
			subTileConfigure(w->parent);
		}
}

 /**
	* Change the stacking order of the windows.
	* @param w A #SubWin
	**/

void
subWinRestack(SubWin *w)
{
	int n = 6;
	Window wins[6] = { w->icon, w->title, w->left, w->right, w->bottom, w->win };
	XRestackWindows(d->dpy, wins, n);
}

 /**
	* Resize the client window.
	* @param w A #SubWin
	**/

void
subWinResize(SubWin *w)
{
	XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, w->height);
	XMoveResizeWindow(d->dpy, w->title, 0, 0, w->width, d->th);
	XMoveResizeWindow(d->dpy, w->win, d->bw, d->th, w->width - 2 * d->bw, 
		w->height - d->th - d->bw);
	XMoveResizeWindow(d->dpy, w->right, w->width - d->bw, d->th, 
		d->bw, w->height - d->th);
	XMoveResizeWindow(d->dpy, w->bottom, 0, w->height - d->bw, w->width, d->bw);
	//subClientSendConfigure(w);
}

 /**
	* Map a window to the screen.
	* @param w A #SubWin
	**/

void
subWinMap(SubWin *w)
{
	XMapSubwindows(d->dpy, w->frame);
	XMapWindow(d->dpy, w->frame);
}

 /**
	* Unmap a window.
	* @param w A #SubWin
	**/

void
subWinUnmap(SubWin *w)
{
	/* Check for shaded state */
	if(w->prop & SUB_WIN_SHADED && w->parent && w->parent->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV))
		w->parent->tile->shaded--;

	XUnmapWindow(d->dpy, w->frame);
	//subClientSetWMState(w, IconicState);
}

 /**
	* Lower a window.
	* @param w A #SubWin
	**/

void
subWinLower(SubWin *w)
{
	XLowerWindow(d->dpy, w->frame);
	XLowerWindow(d->dpy, w->win);
}

 /**
	* Raise a window
	* @param w A #SubWin.
	**/

void
subWinRaise(SubWin *w)
{
	XRaiseWindow(d->dpy, w->frame);
	XRaiseWindow(d->dpy, w->win);
}
