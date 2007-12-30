
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Header$
	**/

#include "subtle.h"

static int nclients = 0;
static Window *clients = NULL;

 /**
	* Create a new client
	* @param win Window of the client
	* @return Returns either a #SubWin on success or otherwise NULL
	**/

SubWin *
subClientNew(Window win)
{
	int i, n = 0;
	Window unnused;
	XWMHints *hints = NULL;
	XWindowAttributes attr;
	XSetWindowAttributes attrs;
	Atom *protos = NULL;
	SubWin *w = subWinNew();
	w->client = (SubClient *)subUtilAlloc(1, sizeof(SubClient));

	w->client->win = win;
	XSelectInput(d->disp, w->client->win, PropertyChangeMask|StructureNotifyMask);
	XSetWindowBorderWidth(d->disp, w->client->win, 0);
	XReparentWindow(d->disp, w->client->win, w->frame, d->bw, d->th);
	XAddToSaveSet(d->disp, w->client->win);

	/* Create windows */
	attrs.border_pixel			= d->colors.border;
	attrs.background_pixel	= d->colors.norm;

	w->client->title		= SUBWINNEW(w->frame, 0, 0, w->width, d->th, 0, CWBackPixel);
	w->client->caption	= SUBWINNEW(w->frame, 0, 0, 1, d->th, 0, CWBackPixel);
	attrs.cursor				= d->cursors.horz;
	w->client->left			= SUBWINNEW(w->frame, 0, d->th, d->bw, w->height - d->th, 0, CWBackPixel|CWCursor);
	w->client->right		= SUBWINNEW(w->frame, w->width - d->bw, d->th, d->bw, w->height - d->th, 0, CWBackPixel|CWCursor);
	attrs.cursor				= d->cursors.vert;
	w->client->bottom		= SUBWINNEW(w->frame, 0, w->height - d->bw, w->width, d->bw, 0, CWBackPixel|CWCursor);

	/* Window attributes */
	XGetWindowAttributes(d->disp, w->client->win, &attr);
	w->client->cmap	= attr.colormap;
	w->flags				= SUB_WIN_TYPE_CLIENT; 

	/* Window caption */
	subClientFetchName(w);

	/* EWMH: Client list and client list stacking */
	clients = (Window *)subUtilRealloc(clients, sizeof(Window) * (nclients + 1));
	clients[nclients++] = w->client->win;
	subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_CLIENT_LIST, clients, nclients);
	subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_CLIENT_LIST_STACKING, clients, nclients);

	/* Window manager hints */
	hints = XGetWMHints(d->disp, win);
	if(hints)
		{
			if(hints->flags & StateHint) subClientSetWMState(w, hints->initial_state);
			else subClientSetWMState(w, NormalState);
			if(hints->input) w->flags |= SUB_WIN_PREF_INPUT;
			if(hints->flags & XUrgencyHint) w->flags |= SUB_WIN_STATE_TRANS;
			XFree(hints);
		}
	
	/* Protocols */
	if(XGetWMProtocols(d->disp, w->client->win, &protos, &n))
		{
			for(i = 0; i < n; i++)
				{
					if(protos[i] == subEwmhFind(SUB_EWMH_WM_TAKE_FOCUS)) 		w->flags |= SUB_WIN_PREF_FOCUS;
					if(protos[i] == subEwmhFind(SUB_EWMH_WM_DELETE_WINDOW))	w->flags |= SUB_WIN_PREF_CLOSE;
				}
			XFree(protos);
		}

	/* Check for dialog windows etc. */
	XGetTransientForHint(d->disp, win, &unnused);
	if(unnused && !(w->flags & SUB_WIN_STATE_TRANS)) w->flags |= SUB_WIN_STATE_TRANS;

	return(w);
}

 /**
	* Delete client
	* @param w A #SubWin
	**/

void
subClientDelete(SubWin *w)
{
	int i, j;

	assert(w && w->flags & SUB_WIN_TYPE_CLIENT);

	/* Update client list */
	for(i = 0; i < nclients; i++) 	
		if(clients[i] == w->client->win)
			{
				for(j = i; j < nclients - 1; j++) clients[j] = clients[j + 1];
				break; /* Leave loop */
			}

	
	if(!(w->flags & SUB_WIN_STATE_DEAD))
		{
			/* Honor window preferences */
			if(w->flags & SUB_WIN_PREF_CLOSE)
				{
					XEvent ev;

					ev.type									= ClientMessage;
					ev.xclient.window				= w->client->win;
					ev.xclient.message_type = subEwmhFind(SUB_EWMH_WM_PROTOCOLS);
					ev.xclient.format				= 32;
					ev.xclient.data.l[0]		= subEwmhFind(SUB_EWMH_WM_DELETE_WINDOW);
					ev.xclient.data.l[1]		= CurrentTime;

					XSendEvent(d->disp, w->client->win, False, NoEventMask, &ev);
				}
			else XKillClient(d->disp, w->client->win);
		}

	if(w->client->name) XFree(w->client->name);
	free(w->client);

	clients = (Window *)subUtilRealloc(clients, sizeof(Window) * nclients);
	nclients--;

	/* EWMH: Client list and client list stacking */			
	subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_CLIENT_LIST, clients, nclients);
	subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_CLIENT_LIST_STACKING, clients, nclients);					
}

 /**
	* Render the client window
	* @param w A #SubWin
	**/

void
subClientRender(SubWin *w)
{
	unsigned long col = 0;

	assert(w && w->flags & SUB_WIN_TYPE_CLIENT);

	/* Check if window was meanwhile destroyed */
	if(w->flags & SUB_WIN_STATE_DEAD) return;

	col = d->focus && d->focus == w ? d->colors.focus : (w->flags & SUB_WIN_STATE_SHADE ? d->colors.cover : d->colors.norm);

	/* Update color */
	XSetWindowBackground(d->disp, w->client->title,		col);
	XSetWindowBackground(d->disp, w->client->left, 		col);
	XSetWindowBackground(d->disp, w->client->right, 	col);
	XSetWindowBackground(d->disp, w->client->bottom,	col);
	XSetWindowBackground(d->disp, w->client->caption, col);

	/* Clear windows */
	XClearWindow(d->disp, w->client->title);
	XClearWindow(d->disp, w->client->left);
	XClearWindow(d->disp, w->client->right);
	XClearWindow(d->disp, w->client->bottom);
	XClearWindow(d->disp, w->client->caption);

	/* Titlebar */
	XFillRectangle(d->disp, w->client->title, d->gcs.border, d->th + 1, 2, w->width - d->th - 4, d->th - 4);	
	XDrawString(d->disp, w->client->caption, d->gcs.font, 5, d->fy - 1, w->client->name, strlen(w->client->name));
}

static void
DrawMask(int type,
	SubWin *w,
	XRectangle *r)
{
	switch(type)
		{
			case SUB_CLIENT_DRAG_STATE_START: 
				XDrawRectangle(d->disp, DefaultRootWindow(d->disp), d->gcs.invert, r->x + 1, r->y + 1, 
					r->width - 3, (w->flags & SUB_WIN_STATE_SHADE) ? d->th - d->bw : r->height - d->bw);
				break;
			case SUB_CLIENT_DRAG_STATE_TOP:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, 5, 5, w->width - 10, w->height * 0.5 - 5);
				break;
			case SUB_CLIENT_DRAG_STATE_BOTTOM:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, 5, w->height * 0.5, w->width - 10, w->height * 0.5 - 5);
				break;
			case SUB_CLIENT_DRAG_STATE_LEFT:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, 5, 5, w->width * 0.5 - 5, w->height - 10);
				break;
			case SUB_CLIENT_DRAG_STATE_RIGHT:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, w->width * 0.5, 5, w->width * 0.5 - 5, w->height - 10);
				break;
			case SUB_CLIENT_DRAG_STATE_SWAP:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, w->width * 0.35, w->height * 0.35, w->width * 0.3, w->height * 0.3);
				break;
			case SUB_CLIENT_DRAG_STATE_BEFORE:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, 5, 5, w->width * 0.1 - 5, w->height - 10);
				break;
			case SUB_CLIENT_DRAG_STATE_AFTER:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, w->width * 0.9, 5, w->width * 0.1 - 5, w->height - 10);
				break;
			case SUB_CLIENT_DRAG_STATE_ABOVE:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, 5, 5, w->width - 10, w->height * 0.1 - 5);
				break;
			case SUB_CLIENT_DRAG_STATE_BELOW:
				XFillRectangle(d->disp, w->frame, d->gcs.invert, 5, w->height * 0.9, w->width - 10, w->height * 0.1 - 5);
				break;
		}
}

static void
AdjustWeight(int mode,
	SubWin *w,
	XRectangle *r)
{
	if(w && w->parent)
		{
			if((w->parent->flags & SUB_WIN_TILE_HORZ && (mode == SUB_CLIENT_DRAG_LEFT || mode == SUB_CLIENT_DRAG_RIGHT)) ||
				(w->parent->flags & SUB_WIN_TILE_VERT && mode == SUB_CLIENT_DRAG_BOTTOM))
				{
					if(w->parent->flags & SUB_WIN_TILE_HORZ) w->resized = r->width * 100 / SUBWINWIDTH(w->parent);
					else if(w->parent->flags & SUB_WIN_TILE_VERT) w->resized = r->height * 100 / SUBWINHEIGHT(w->parent);

					if(w->resized >= 80) w->resized = 80;
					if(!(w->flags & SUB_WIN_STATE_RESIZE)) w->flags |= SUB_WIN_STATE_RESIZE;
					subTileConfigure(w->parent);
					return;
				}
			else if(((w->parent->flags & SUB_WIN_TILE_VERT && (mode == SUB_CLIENT_DRAG_LEFT || mode == SUB_CLIENT_DRAG_RIGHT)) ||
				(w->parent->flags & SUB_WIN_TILE_HORZ && mode == SUB_CLIENT_DRAG_BOTTOM)) && w->parent->parent)
				AdjustWeight(mode, w->parent, r);
		}
}

 /**
	* Movie/renclients the client
	* @param mode Attach mode
	* @param w A #SubWin
	* @param bev A #XButtonEvent
	**/

void
subClientDrag(int mode,
	SubWin *w)
{
	XEvent ev;
	Cursor cursor;
	Window win, unused;
	int wx = 0, wy = 0, rx = 0, ry = 0;
	unsigned int mask;
	int state = SUB_CLIENT_DRAG_STATE_START, last_state = SUB_CLIENT_DRAG_STATE_START;
	XRectangle r;
	SubWin *w2 = NULL, *p = NULL, *last_w = NULL;

	assert(w && w->flags & SUB_WIN_TYPE_CLIENT);
	
	/* Get window position on root window */
	XQueryPointer(d->disp, w->frame, &win, &win, &rx, &ry, &wx, &wy, &mask);
	r.x				= rx - wx;
	r.y				= ry - wy;
	r.width 	= w->width;
	r.height	= w->height;

	/* Select cursor */
	switch(mode)
		{
			case SUB_CLIENT_DRAG_LEFT:		
			case SUB_CLIENT_DRAG_RIGHT:		cursor = d->cursors.horz;		break;
			case SUB_CLIENT_DRAG_BOTTOM:	cursor = d->cursors.vert;		break;
			case SUB_CLIENT_DRAG_MOVE:		cursor = d->cursors.move;		break;
			default:											cursor = d->cursors.square;	break;
		}

	if(XGrabPointer(d->disp, w->frame, True, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, 
		GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime)) return;

	XGrabServer(d->disp);
	if(mode <= SUB_CLIENT_DRAG_MOVE) DrawMask(SUB_CLIENT_DRAG_STATE_START, w, &r);

	for(;;)
		{
			XMaskEvent(d->disp, PointerMotionMask|ButtonReleaseMask|EnterWindowMask, &ev);
			switch(ev.type)
				{
					/* Button release doesn't return our destination window */
					case EnterNotify: win = ev.xcrossing.window; break;
					case MotionNotify:
						if(mode <= SUB_CLIENT_DRAG_MOVE) 
							{
								DrawMask(SUB_CLIENT_DRAG_STATE_START, w, &r);
					
								/* Calculate dimensions of the selection rect */
								switch(mode)
									{
										case SUB_CLIENT_DRAG_LEFT: 	
											r.x			= (rx - wx) - (rx - ev.xmotion.x_root);	
											r.width = w->width + ((rx - wx ) - ev.xmotion.x_root);	
											break;
										case SUB_CLIENT_DRAG_RIGHT:	r.width		= w->width + (ev.xmotion.x_root - rx);	break;
										case SUB_CLIENT_DRAG_BOTTOM: r.height	= w->height + (ev.xmotion.y_root - ry);	break;
										case SUB_CLIENT_DRAG_MOVE:
											r.x	= (rx - wx) - (rx - ev.xmotion.x_root);
											r.y	= (ry - wy) - (ry - ev.xmotion.y_root);
											break;
									}	
								DrawMask(SUB_CLIENT_DRAG_STATE_START, w, &r);
							}
						else if(win != w->frame && mode == SUB_CLIENT_DRAG_SWAP)
							{
								if(!w2 || w2->frame != win) w2 = (SubWin *)subUtilFind(win, 1);
								if(w2)
									{
										XQueryPointer(d->disp, win, &unused, &unused, &rx, &ry, &wx, &wy, &mask);
										r.x = rx - wx;
										r.y = ry - wy;

										/* Change drag state */
										if(wx > w2->width * 0.35 && wx < w2->width * 0.65)
											{
												if(state != SUB_CLIENT_DRAG_STATE_TOP && wy > w2->height * 0.1 && wy < w2->height * 0.35)
													state = SUB_CLIENT_DRAG_STATE_TOP;
												else if(state != SUB_CLIENT_DRAG_STATE_BOTTOM && wy > w2->height * 0.65 && wy < w2->height * 0.9)
													state = SUB_CLIENT_DRAG_STATE_BOTTOM;
												else if(state != SUB_CLIENT_DRAG_STATE_SWAP && wy > w2->height * 0.35 && wy < w2->height * 0.65)
													state = SUB_CLIENT_DRAG_STATE_SWAP;
											}
										if(state != SUB_CLIENT_DRAG_STATE_ABOVE && wy < w2->height * 0.1) state = SUB_CLIENT_DRAG_STATE_ABOVE;
										else if(state != SUB_CLIENT_DRAG_STATE_BELOW && wy > w2->height * 0.9) state = SUB_CLIENT_DRAG_STATE_BELOW;
										if(wy > w2->height * 0.1 && wy < w2->height * 0.9)
											{
												if(state != SUB_CLIENT_DRAG_STATE_LEFT && wx > w2->width * 0.1 && wx < w2->width * 0.35)
													state = SUB_CLIENT_DRAG_STATE_LEFT;
												else if(state != SUB_CLIENT_DRAG_STATE_RIGHT && wx > w2->width * 0.65 && wx < w2->width * 0.9)
													state = SUB_CLIENT_DRAG_STATE_RIGHT;
												else if(state != SUB_CLIENT_DRAG_STATE_BEFORE && wx < w2->width * 0.1)
													state = SUB_CLIENT_DRAG_STATE_BEFORE;
												else if(state != SUB_CLIENT_DRAG_STATE_AFTER && wx > w2->width * 0.9)
													state = SUB_CLIENT_DRAG_STATE_AFTER;
											}

										if(last_state != state || last_w != w2) 
											{
												if(last_state != SUB_CLIENT_DRAG_STATE_START) DrawMask(last_state, last_w, &r);
												DrawMask(state, w2, &r);

												last_w		 = w2;
												last_state = state;
											}
										}
								}
						break;
					case ButtonRelease:
						if(win != w->frame && mode > SUB_CLIENT_DRAG_MOVE)
							{
								DrawMask(state, w2, &r); /* Erase mask */

								if(w && w2 && w->parent && w2->parent)
									{
										if(state == SUB_CLIENT_DRAG_STATE_TOP || state == SUB_CLIENT_DRAG_STATE_BOTTOM ||
											state == SUB_CLIENT_DRAG_STATE_LEFT || state == SUB_CLIENT_DRAG_STATE_RIGHT)
											{
												SubWin *t = subTileNew(state == SUB_CLIENT_DRAG_STATE_TOP || 
													state == SUB_CLIENT_DRAG_STATE_BOTTOM ? SUB_WIN_TILE_VERT : SUB_WIN_TILE_HORZ);
												p = w->parent;

												subWinReplace(w2, t);
												subWinUnlink(w);

												/* Check resizeded windows */
												if(w2->flags & SUB_WIN_STATE_RESIZE)
													{
														t->flags 		|= SUB_WIN_STATE_RESIZE;
														t->resized		= w2->resized;
														w2->flags		&= ~SUB_WIN_STATE_RESIZE;
														w2->resized	= 0;
													}

												subTileAdd(t, state == SUB_CLIENT_DRAG_STATE_TOP || state == SUB_CLIENT_DRAG_STATE_LEFT ? w : w2);
												subTileAdd(t, state == SUB_CLIENT_DRAG_STATE_TOP || state == SUB_CLIENT_DRAG_STATE_LEFT ? w2 : w);
												subWinMap(t);

												subTileConfigure(t->parent);
												subTileConfigure(p);
											}
										else if(state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_AFTER ||
											state == SUB_CLIENT_DRAG_STATE_ABOVE || state == SUB_CLIENT_DRAG_STATE_BELOW)
											{
												p = w->parent;

												if((((state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_AFTER) && 
													w2->parent->flags & SUB_WIN_TILE_HORZ)) ||
													((state == SUB_CLIENT_DRAG_STATE_ABOVE || state == SUB_CLIENT_DRAG_STATE_BELOW) && 
													w2->parent->flags & SUB_WIN_TILE_VERT))
													{
														subWinUnlink(w);

														if(state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_ABOVE) 
															subWinPrepend(w2, w);
														else subWinAppend(w2, w);

														subTileConfigure(w->parent);
														if(w->parent != p) subTileConfigure(p); 
													}
												else if(w2->parent->parent && 
													(((state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_AFTER) && 
													w2->parent->flags & SUB_WIN_TILE_VERT) ||
													((state == SUB_CLIENT_DRAG_STATE_ABOVE || state == SUB_CLIENT_DRAG_STATE_BELOW) && 
													w2->parent->flags & SUB_WIN_TILE_HORZ)))
													{
														subWinUnlink(w);

														if(state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_ABOVE) 
															subWinPrepend(w2->parent, w);
														else subWinAppend(w2->parent, w);
														
														subTileConfigure(w->parent);
														subTileConfigure(p); 
													}
											}
										else if(state == SUB_CLIENT_DRAG_STATE_SWAP) subWinSwap(w, w2);
									}
							}
						else /* Resize */
							{
								XDrawRectangle(d->disp, DefaultRootWindow(d->disp), d->gcs.invert, r.x + 1, r.y + 1, 
									r.width - 3, (w->flags & SUB_WIN_STATE_SHADE) ? d->th - 3 : r.height - 3);
					
								if(w->flags & SUB_WIN_STATE_FLOAT) 
									{
										w->x			= r.x;
										w->y			= r.y;
										w->width	= r.width;
										w->height	= r.height;

										subWinResize(w);
										if(w->flags & SUB_WIN_TYPE_TILE) subTileConfigure(w);
									}
								else if(w->parent && mode <= SUB_CLIENT_DRAG_BOTTOM) AdjustWeight(mode, w, &r);
							}

						XUngrabServer(d->disp);
						XUngrabPointer(d->disp, CurrentTime);
						return;
				}
		}
}

 /**
	* Toggle states of a window 
	* @param w A #SubWin
	**/

void
subClientToggle(int type,
	SubWin *w)
{
	XEvent event;

	assert(w && w->flags & SUB_WIN_TYPE_CLIENT);

	if(w->flags & type)
		{
			w->flags &= ~type;
			switch(type)
				{
					case SUB_WIN_STATE_SHADE:
						subClientSetWMState(w, NormalState);

						/* Map most of the windows */
						XMapWindow(d->disp, w->client->win);
						XMapWindow(d->disp, w->client->left);
						XMapWindow(d->disp, w->client->right);
						XMapWindow(d->disp, w->client->bottom);

						/* Resize frame */
						XMoveResizeWindow(d->disp, w->frame, w->x, w->y, w->width, w->height);
						break;
					case SUB_WIN_STATE_FLOAT: XReparentWindow(d->disp, w->frame, w->parent->frame, w->x, w->y);	break;
					case SUB_WIN_STATE_FULL:
						/* Map most of the windows */
						XMapWindow(d->disp, w->client->title);
						XMapWindow(d->disp, w->client->left);
						XMapWindow(d->disp, w->client->right);
						XMapWindow(d->disp, w->client->bottom);
						XMapWindow(d->disp, w->client->caption);

						subWinResize(w);
						subTileAdd(w->parent, w);
						subTileConfigure(w->parent);
						break;								
				}
		}
	else 
		{
			long supplied = 0;
			XSizeHints *hints = NULL;
			w->flags |= type;

			switch(type)
				{
					case SUB_WIN_STATE_SHADE:
						subClientSetWMState(w, WithdrawnState);

						/* Unmap most of the windows */
						XUnmapWindow(d->disp, w->client->win);
						XUnmapWindow(d->disp, w->client->left);
						XUnmapWindow(d->disp, w->client->right);
						XUnmapWindow(d->disp, w->client->bottom);

						XMoveResizeWindow(d->disp, w->frame, w->x, w->y, w->width, d->th);
						break;						
					case SUB_WIN_STATE_FLOAT:
						/* Respect the user/program preferences */
						hints = XAllocSizeHints();
						if(!hints) subUtilLogError("Can't alloc memory. Exhausted?\n");
						if(XGetWMNormalHints(d->disp, w->client->win, hints, &supplied))
							{
								if(hints->flags & (USPosition|PPosition))
									{
										w->x = hints->x + 2 * d->bw;
										w->y = hints->y + d->th + d->bw;
									}
								else if(hints->flags & PAspect)
									{
										w->x = (hints->min_aspect.x - hints->max_aspect.x) / 2;
										w->y = (hints->min_aspect.y - hints->max_aspect.y) / 2;
									}
								if(hints->flags & (USSize|PSize))
									{
										w->width	= hints->width + 2 * d->bw;
										w->height	= hints->height + d->th + d->bw;
									}
								else if(hints->flags & PBaseSize)
									{
										w->width	= hints->base_width + 2 * d->bw;
										w->height	= hints->base_height + d->th + d->bw;
									}
								else
									{
										/* Fallback for clients breaking the ICCCM (mostly Gtk+ stuff) */
										if(hints->base_width > 0 && hints->base_width <= DisplayWidth(d->disp, DefaultScreen(d->disp)) &&
											hints->base_height > 0 && hints->base_height <= DisplayHeight(d->disp, DefaultScreen(d->disp))) 
											{
												w->width	= hints->base_width + 2 * d->bw;
												w->height	= hints->base_width + d->th + d->bw;
												w->x			= (DisplayWidth(d->disp, DefaultScreen(d->disp)) - w->width) / 2;
												w->y			= (DisplayHeight(d->disp, DefaultScreen(d->disp)) - w->height) / 2;
											}
									}
							}

						subWinResize(w);
						XReparentWindow(d->disp, w->frame, DefaultRootWindow(d->disp), w->x, w->y);
						XRaiseWindow(d->disp, w->frame);

						XFree(hints);
						break;
					case SUB_WIN_STATE_FULL:
						/* Unmap some windows */
						XUnmapWindow(d->disp, w->client->title);
						XUnmapWindow(d->disp, w->client->left);
						XUnmapWindow(d->disp, w->client->right);
						XUnmapWindow(d->disp, w->client->bottom);								
						XUnmapWindow(d->disp, w->client->caption);

						/* Resize to display resolution */
						w->x			= 0;
						w->y			= 0;
						w->width	= DisplayWidth(d->disp, DefaultScreen(d->disp));
						w->height	= DisplayHeight(d->disp, DefaultScreen(d->disp));

						XReparentWindow(d->disp, w->frame, DefaultRootWindow(d->disp), 0, 0);
						subWinResize(w);
				}
		}
	XUngrabServer(d->disp);
	while(XCheckTypedEvent(d->disp, UnmapNotify, &event));
	if(type != SUB_WIN_STATE_FULL) subTileConfigure(w->parent);
}

 /**
	* Send a configure request to the client
	* @param w A #SubWin
	**/

void
subClientConfigure(SubWin *w)
{
	XConfigureEvent ev;

	assert(w && w->flags & SUB_WIN_TYPE_CLIENT);

	ev.type								= ConfigureNotify;
	ev.event							= w->client->win;
	ev.window							= w->client->win;
	ev.x									= w->x;
	ev.y									= w->y;
	ev.width							= (w->flags & SUB_WIN_STATE_FULL) ? w->width	: SUBWINWIDTH(w);
	ev.height							= (w->flags & SUB_WIN_STATE_FULL) ? w->height : SUBWINHEIGHT(w);
	ev.above							= None;
	ev.border_width 			= 0;
	ev.override_redirect	= 0;

	XSendEvent(d->disp, w->client->win, False, StructureNotifyMask, (XEvent *)&ev);
}

	/**
	 * Fetch client name
	 * @param w A #SubWin
	 **/

void
subClientFetchName(SubWin *w)
{
	int width;

	assert(w && w->flags & SUB_WIN_TYPE_CLIENT);

	if(w->client->name) XFree(w->client->name);
	XFetchName(d->disp, w->client->win, &w->client->name);
	if(!w->client->name) w->client->name = strdup("subtle");

	/* Check max length of the caption */
	width = (strlen(w->client->name) + 1) * d->fx + 3;
	if(width > w->width - d->th - 4) width = w->width - d->th - 14;
	XMoveResizeWindow(d->disp, w->client->caption, d->th, 0, width, d->th);

	subClientRender(w);
}

 /**
	* Set the WM state for the client
	* @param w A #SubWin
	* @param state New state for the client
	**/

void
subClientSetWMState(SubWin *w,
	long state)
{
	CARD32 data[2];
	data[0] = state;
	data[1] = None; /* No icons */

	assert(w && w->flags & SUB_WIN_TYPE_CLIENT);

	XChangeProperty(d->disp, w->client->win, subEwmhFind(SUB_EWMH_WM_STATE), subEwmhFind(SUB_EWMH_WM_STATE),
		32, PropModeReplace, (unsigned char *)data, 2);
}

 /**
	* Get the WM state of the client
	* @param w A #SubWin
	* @return Returns the state of the client
	**/

long
subClientGetWMState(SubWin *w)
{
	Atom type;
	int format;
	unsigned long unused, bytes;
	long *data = NULL, state = WithdrawnState;

	assert(w && w->flags & SUB_WIN_TYPE_CLIENT);

	if(XGetWindowProperty(d->disp, w->client->win, subEwmhFind(SUB_EWMH_WM_STATE), 0L, 2L, False, 
			subEwmhFind(SUB_EWMH_WM_STATE), &type, &format, &bytes, &unused, (unsigned char **)&data) == Success && bytes)
		{
			state = *data;
			XFree(data);
		}
	return(state);
}
