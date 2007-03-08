#include "subtle.h"

 /**
	* Find a window via Xlib context manager.
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
	* Create a new window and append it to the window list.
	* @param win A #Window
	* @return Return a #SubWin on success.
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
	mask					|= CWCursor;
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
			XSelectInput(d->dpy, w->win, PropertyChangeMask);
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
	if(w)
		{
			SubWin *p = w->parent;

			XDeleteContext(d->dpy, w->frame, 1);
			XDestroySubwindows(d->dpy, w->frame);
			XDestroyWindow(d->dpy, w->frame);

			if(SUBISTILE(p)) subTileConfigure(p);
			free(w);
		}
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
	unsigned long col = mode ? (w->prop & SUB_WIN_SHADED ? d->colors.shade : d->colors.norm) : d->colors.focus;

	/* Update color */
	XSetWindowBackground(d->dpy, w->title,	col);
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
DrawOutline(short mode,
	SubWin *w,
	int ix,			// Init x position on root window
	int iy,			// Init y position on root window
	int rx,			// X position on root window
	int ry,			// Y position on root window
	int sx,			// Start x position of the dragged window
	int sy,			// Start y position of the dragged window
	int sw,			// Start width of the dragged window
	int sh)			// Start height of the dragged window
{
	w->x = sx;
	w->y = sy;
	switch(mode)
		{
			case SUB_WIN_DRAG_LEFT: 	w->x 			= sx - (ix - rx); w->width	= sx + (ix - rx);	break;
			case SUB_WIN_DRAG_RIGHT:	w->width	= sw + (rx - ix); 														break;
			case SUB_WIN_DRAG_BOTTOM: w->height	= sh + (ry - iy); 														break;
			case SUB_WIN_DRAG_MOVE:		w->x			= sx - (ix - rx); w->y			= sy - (iy - ry);	break; 
		}
	XDrawRectangle(d->dpy, DefaultRootWindow(d->dpy), d->gcs.invert, w->x, w->y, 
		w->width, (w->prop & SUB_WIN_SHADED) ? d->th : w->height);
}

 /**
	* Movie/resize the window.
	* @param mode Attach mode
	* @param w A #SubWin
	* @param bev A #XButtonEvent
	**/

void
subWinDrag(short mode,
	SubWin *w,
	XButtonEvent *bev)
{
	XEvent ev;
	Cursor cursor;
	Window win;
	SubWin *w2 = NULL, *p = NULL, *p2 = NULL;
	int ix = 0, iy = 0, rx = 0, ry = 0, sx = 0, sy = 0, sw = w->width, sh = w->height;
	unsigned int mask;

	/* Get window position on root window */
	XQueryPointer(d->dpy, DefaultRootWindow(d->dpy), &win, &win, &rx, &ry, &sx, &sy, &mask);
	ix = rx;
	iy = ry;
	sx = rx - bev->x;
	sy = ry - bev->y;

	/* Select cursor */
	switch(mode)
		{
			case SUB_WIN_DRAG_LEFT:		cursor = d->cursors.left;		break;
			case SUB_WIN_DRAG_RIGHT:	cursor = d->cursors.right;	break;
			case SUB_WIN_DRAG_BOTTOM:	cursor = d->cursors.bottom;	break;
			case SUB_WIN_DRAG_MOVE:		cursor = d->cursors.move;		break;
			default:									cursor = d->cursors.square;	break;
		}

	if(XGrabPointer(d->dpy, w->frame, True, SUBPOINTERMASK, GrabModeAsync, GrabModeAsync, None, 
		cursor, CurrentTime)) return;

	XGrabServer(d->dpy);
	if(mode <= SUB_WIN_DRAG_MOVE) DrawOutline(mode, w, ix, iy, rx, ry, sx, sy, sw, sh);
	for(;;)
		{
			XMaskEvent(d->dpy, PointerMotionMask|ButtonReleaseMask|EnterWindowMask, &ev);
			switch(ev.type)
				{
					/* Button release doesn't return our destination window */
					case EnterNotify: win = ev.xcrossing.window; break;
					case MotionNotify:
						if(mode <= SUB_WIN_DRAG_MOVE) DrawOutline(mode, w, ix, iy, rx, ry, sx, sy, sw, sh);
						rx = ev.xmotion.x_root;
						ry = ev.xmotion.y_root;
						if(mode <= SUB_WIN_DRAG_MOVE) DrawOutline(mode, w, ix, iy, rx, ry, sx, sy, sw, sh);
						break;
					case ButtonRelease:
						if(win != w->frame && mode > SUB_WIN_DRAG_MOVE)
							{
								w2 = subWinFind(win);
						
								if(w && w2 && w->parent && w2->parent)
									{
										/* Append a window to tile */
										if(SUBISTILE(w2) && mode == SUB_WIN_DRAG_ICON)
											{
												SubWin *p = w->parent;
		
												subTileAdd(w2, w);
												subTileConfigure(w2->parent);
												subTileConfigure(p);
											}
										/* Swap two windows manually */
										else
											{
												Window swap;
												SubTile *tile = NULL;
												SubClient *client = NULL;

												XReparentWindow(d->dpy, w->win, w2->frame, d->bw, d->th);
												XReparentWindow(d->dpy, w2->win, w->frame, d->bw, d->th);

												/* Swap titlebar fields */
												if(SUBISCLIENT(w) && SUBISCLIENT(w2))
													{
														XReparentWindow(d->dpy, w->client->caption, w2->frame, d->th, 0); 
														XReparentWindow(d->dpy, w2->client->caption, w->frame, d->th, 0); 

														client = w->client; w->client = w2->client; w2->client = client;
													}
												else if(!(SUBISTILE(w) && SUBISTILE(w2)))
													{
														if(SUBISTILE(w) && SUBISCLIENT(w2))
															{
																XReparentWindow(d->dpy, w->tile->btnew, w2->frame, d->th, 0);
																XReparentWindow(d->dpy, w->tile->btdel, w2->frame, d->th + 7 * d->fx, 0);
																XReparentWindow(d->dpy, w2->client->caption, w->frame, d->th, 0); 
															}
														else
															{
																XReparentWindow(d->dpy, w2->tile->btnew, w->frame, d->th, 0);
																XReparentWindow(d->dpy, w2->tile->btdel, w->frame, d->th + 7 * d->fx, 0);
																XReparentWindow(d->dpy, w->client->caption, w2->frame, d->th, 0); 
															}

														/* Swap union members carefully at the same time */
														tile			= w->tile;	client			= w->client;
														w->tile		= w2->tile; w->client		= w2->client;
														w2->tile	= tile; 		w2->client	= client;
													}

												swap = w->win;	w->win	= w2->win;	w2->win		= swap; 
												swap = w->prop;	w->prop	= w2->prop;	w2->prop	= swap;

												if(w->prop & SUB_WIN_FLOAT)
													{ 
														w->prop		&= ~SUB_WIN_FLOAT;
														w2->prop	|= SUB_WIN_FLOAT;

														subWinResize(w); 
														subTileConfigure(w);
													}
												else subTileConfigure(w->parent);

												if(w2->prop & SUB_WIN_FLOAT)
													{
														w2->prop	&= ~SUB_WIN_FLOAT;
														w->prop		|= SUB_WIN_FLOAT;

														subWinResize(w2); 
														subTileConfigure(w2);
													}
												else subTileConfigure(w2->parent);

												subLogDebug("Swap: %#lx <=> %#lx\n", w->win, w2->win);
											}
									}
								}
						else /* Resize */
							{
								DrawOutline(mode, w, ix, iy, rx, ry, sx, sy, sw, sh);
								if(w->prop & SUB_WIN_FLOAT) subWinResize(w);
								if(SUBISTILE(w)) subTileConfigure(w);
							}
												
						XUngrabServer(d->dpy);
						XUngrabPointer(d->dpy, CurrentTime);
						return;
				}
		}
}

 /**
	* Toggle shaded/float state of a window.
	* @param w A #SubWin
	**/

void
subWinToggle(short type,
	SubWin *w)
{
	if(w && w->parent)
		{
			XEvent event;
			XGrabServer(d->dpy);
			if(w->prop & type)
				{
					w->prop &= ~type;

					switch(type)
						{
							case SUB_WIN_SHADED:
								/* Set state */
								if(w->prop & SUB_WIN_CLIENT) subClientSetWMState(w, NormalState);

								/* Map most of the windows */
								XMapWindow(d->dpy, w->win);
								XMapWindow(d->dpy, w->left);
								XMapWindow(d->dpy, w->right);
								XMapWindow(d->dpy, w->bottom);

								/* Resize frame */
								XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, w->height);

								if(!(w->prop & SUB_WIN_FLOAT)) (w->parent->tile->excl)--;
								break;
							case SUB_WIN_FLOAT: XReparentWindow(d->dpy, w->frame, w->parent->win, w->x, w->y);
						}
				}
			else 
				{
					XSizeHints *hints = NULL;

					w->prop	|= type;

					switch(type)
						{
							case SUB_WIN_SHADED:
								/* Set state */
								if(w->prop & SUB_WIN_CLIENT) subClientSetWMState(w, WithdrawnState);

								/* Unmap most of the windows */
								XUnmapWindow(d->dpy, w->win);
								XUnmapWindow(d->dpy, w->left);
								XUnmapWindow(d->dpy, w->right);
								XUnmapWindow(d->dpy, w->bottom);

								/* Resize frame */
								XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, d->th);

								if(!(w->prop & SUB_WIN_FLOAT)) (w->parent->tile->excl)++;
								break;
							case SUB_WIN_FLOAT:
								/* Respect the user/program preferences */
								hints = XAllocSizeHints();
								if(!hints) subLogError("Can't alloc memory. Exhausted?\n");

								if(hints->flags & USSize || hints->flags & PSize)
									{
										w->width	= hints->width;
										w->height	= hints->height;
									}
								else if(hints->flags & PBaseSize)
									{
										w->width	= hints->base_width;
										w->height	= hints->base_height;
									}
								if(hints->flags & USPosition || hints->flags & PPosition)
									{
										w->x = hints->x;
										w->y = hints->y;
									}
								else if(hints->flags & PAspect)
									{
										w->x = (hints->min_aspect.x - hints->max_aspect.x) / 2;
										w->y = (hints->min_aspect.y - hints->max_aspect.y) / 2;
									}
								else
									{
										w->x = (DisplayWidth(d->dpy, DefaultScreen(d->dpy)) - w->width) / 2;
										w->y = (DisplayHeight(d->dpy, DefaultScreen(d->dpy)) - w->height) / 2;
									}

								subWinResize(w);
								XReparentWindow(d->dpy, w->frame, DefaultRootWindow(d->dpy), w->x, w->y);
								subWinRaise(w);

								XFree(hints);
						}
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
	XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, (w->prop & SUB_WIN_SHADED) ? d->th : w->height);
	XMoveResizeWindow(d->dpy, w->title, 0, 0, w->width, d->th);

	if(!(w->prop & SUB_WIN_SHADED))
		{
			XMoveResizeWindow(d->dpy, w->win, d->bw, d->th, w->width - 2 * d->bw, 
				w->height - d->th - d->bw);
			XMoveResizeWindow(d->dpy, w->right, w->width - d->bw, d->th, 
				d->bw, w->height - d->th);
			XMoveResizeWindow(d->dpy, w->bottom, 0, w->height - d->bw, w->width, d->bw);
			if(w->prop & SUB_WIN_CLIENT) subClientSendConfigure(w);
		}
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
	if(w && w->prop & SUB_WIN_SHADED && w->parent && w->parent->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV))
		w->parent->tile->excl;

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
