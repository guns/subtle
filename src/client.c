
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static int size = 0;
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
	long mask = 0;
	Window unnused;
	XWMHints *hints = NULL;
	XWindowAttributes attr;
	XSetWindowAttributes attrs;
	Atom *protos = NULL;
	SubWin *w = subWinNew();
	w->client = (SubClient *)subUtilAlloc(1, sizeof(SubClient));

	XGrabServer(d->dpy);

	attrs.border_pixel			= d->colors.border;
	attrs.background_pixel	= d->colors.norm;

	w->client->win = win;
	XSelectInput(d->dpy, w->client->win, PropertyChangeMask);
	XSetWindowBorderWidth(d->dpy, w->client->win, 0);
	XReparentWindow(d->dpy, w->client->win, w->frame, d->bw, d->th);

	XAddToSaveSet(d->dpy, w->client->win);
	XGetWindowAttributes(d->dpy, w->client->win, &attr);
	w->client->cmap	= attr.colormap;
	w->flags				= SUB_WIN_TYPE_CLIENT; 

	/* Create windows */
	mask							= CWBackPixel;
	w->client->title	= SUBWINNEW(w->frame, 0, 0, w->width, d->th, 0);
	mask 							= CWBorderPixel|CWBackPixel;
	w->client->icon		= SUBWINNEW(w->frame, 2, 2, d->th - 6, d->th - 6, 1);
	
	/* Window caption */
	XFetchName(d->dpy, w->client->win, &w->client->name);
	if(!w->client->name) w->client->name = strdup("subtle");
	w->client->caption = XCreateSimpleWindow(d->dpy, w->frame, d->th, 0, 
		(strlen(w->client->name) + 1) * d->fx, d->th, 0, d->colors.border, d->colors.norm);
	
	/* Create borders */
	mask							|= CWCursor;
	attrs.cursor			= d->cursors.horz;
	w->client->left		= SUBWINNEW(w->frame, 0, d->th, d->bw, w->height - d->th, 0);
	w->client->right	= SUBWINNEW(w->frame, w->width - d->bw, d->th, d->bw, w->height - d->th, 0);
	attrs.cursor			= d->cursors.vert;
	w->client->bottom	= SUBWINNEW(w->frame, 0, w->height - d->bw, w->width, d->bw, 0);

	/* EWMH: Client list and client list stacking */
	clients = (Window *)realloc(clients, sizeof(Window) * (size + 1));
	if(!clients) subUtilLogError("Can't alloc memory. Exhausted?\n");
	clients[size] = w->client->win;
	size++;
	subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CLIENT_LIST, clients, size);
	subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CLIENT_LIST_STACKING, clients, size);

	/* Window manager hints */
	hints = XGetWMHints(d->dpy, win);
	if(hints)
		{
			if(hints->flags & StateHint) subClientSetWMState(w, hints->initial_state);
			else subClientSetWMState(w, NormalState);
			if(hints->initial_state == IconicState) subUtilLogDebug("Iconic: win=%#lx\n", win);			
			if(hints->input) w->flags |= SUB_WIN_PREF_INPUT;
			if(hints->flags & XUrgencyHint) w->flags |= SUB_WIN_STATE_TRANS;
			XFree(hints);
		}
	
	/* Protocols */
	if(XGetWMProtocols(d->dpy, w->client->win, &protos, &n))
		{
			for(i = 0; i < n; i++)
				{
					if(protos[i] == subEwmhGetAtom(SUB_EWMH_WM_TAKE_FOCUS)) 		w->flags |= SUB_WIN_PREF_FOCUS;
					if(protos[i] == subEwmhGetAtom(SUB_EWMH_WM_DELETE_WINDOW))	w->flags |= SUB_WIN_PREF_CLOSE;
				}
			XFree(protos);
		}

	subWinMap(w);

	/* Check for dialog windows etc. */
	XGetTransientForHint(d->dpy, win, &unnused);
	if(unnused && !(w->flags & SUB_WIN_STATE_TRANS)) w->flags |= SUB_WIN_STATE_TRANS;

	XSync(d->dpy, False);
	XUngrabServer(d->dpy);
	return(w);
}

 /**
	* Delete a client
	* @param w A #SubWin
	**/

void
subClientDelete(SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{
			int i, j;

			/* Update client list */
			for(i = 0; i < size; i++) 	
				if(clients[i] == w->client->win)
					{
						for(j = i; j < size - 1; j++) clients[j] = clients[j + 1];
						break; /* Leave loop */
					}

			/* Honor window preferences */
			if(w->flags & SUB_WIN_PREF_CLOSE)
				{
					XEvent ev;

					ev.type									= ClientMessage;
					ev.xclient.window				= w->client->win;
					ev.xclient.message_type = subEwmhGetAtom(SUB_EWMH_WM_PROTOCOLS);
					ev.xclient.format				= 32;
					ev.xclient.data.l[0]		= subEwmhGetAtom(SUB_EWMH_WM_DELETE_WINDOW);
					ev.xclient.data.l[1]		= CurrentTime;

					XSendEvent(d->dpy, w->client->win, False, NoEventMask, &ev);
				}
			else XDestroyWindow(d->dpy, w->client->win);

			if(w->client->name) XFree(w->client->name);
			free(w->client);

			clients = (Window *)realloc(clients, sizeof(Window) * size);
			if(!clients)  subUtilLogError("Can't alloc memory. Exhausted?\n");
			size--;

			/* EWMH: Client list and client list stacking */			
			subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CLIENT_LIST, clients, size);
			subEwmhSetWindows(DefaultRootWindow(d->dpy), SUB_EWMH_NET_CLIENT_LIST_STACKING, clients, size);					
		}
}

 /**
	* Render the client window
	* @param w A #SubWin
	**/

void
subClientRender(SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{
			char c = 's';
			unsigned long col = d->focus && d->focus == w ? d->colors.focus : 
				(w->flags & SUB_WIN_STATE_COLLAPSE ? d->colors.cover : d->colors.norm);

			/* Update color */
			XSetWindowBackground(d->dpy, w->client->title,	col);
			XSetWindowBackground(d->dpy, w->client->icon,		col);
			XSetWindowBackground(d->dpy, w->client->left, 	col);
			XSetWindowBackground(d->dpy, w->client->right, 	col);
			XSetWindowBackground(d->dpy, w->client->bottom,	col);
			XSetWindowBackground(d->dpy, w->client->caption, col);
	
			/* Clear windows */
			XClearWindow(d->dpy, w->client->title);
			XClearWindow(d->dpy, w->client->icon);
			XClearWindow(d->dpy, w->client->left);
			XClearWindow(d->dpy, w->client->right);
			XClearWindow(d->dpy, w->client->bottom);
			XClearWindow(d->dpy, w->client->caption);

			/* Titlebar */
			XFillRectangle(d->dpy, w->client->title, d->gcs.border, d->th + 1, 2, w->width - d->th - 4, d->th - 4);	

			/* Icon */
			XSetWindowBorder(d->dpy, w->client->icon, d->colors.border);
			if(w->flags & SUB_WIN_STATE_RAISE)					c = 'r';
			else if(w->flags & SUB_WIN_STATE_COLLAPSE)	c = 'c';
	
			XDrawString(d->dpy, w->client->icon, d->gcs.font, (d->th - 8 - d->fx) / 2 + 1, d->fy - 5, &c, 1);
			XDrawString(d->dpy, w->client->caption, d->gcs.font, 3, d->fy - 1, w->client->name, 
				strlen(w->client->name));
		}
}

static void
DrawMask(short type,
	SubWin *w,
	XRectangle *box)
{
	switch(type)
		{
			case SUB_CLIENT_DRAG_STATE_START: 
				XDrawRectangle(d->dpy, DefaultRootWindow(d->dpy), d->gcs.invert, box->x + 1, box->y + 1, 
					box->width - 3, (w->flags & SUB_WIN_STATE_COLLAPSE) ? d->th - d->bw : box->height - d->bw);
				break;
			case SUB_CLIENT_DRAG_STATE_TOP:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, 5, 5, w->width - 10, w->height * 0.5 - 5);
				break;
			case SUB_CLIENT_DRAG_STATE_BOTTOM:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, 5, w->height * 0.5, w->width - 10, w->height * 0.5 - 5);
				break;
			case SUB_CLIENT_DRAG_STATE_LEFT:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, 5, 5, w->width * 0.5 - 5, w->height - 10);
				break;
			case SUB_CLIENT_DRAG_STATE_RIGHT:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, w->width * 0.5, 5, w->width * 0.5 - 5, w->height - 10);
				break;
			case SUB_CLIENT_DRAG_STATE_SWAP:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, w->width * 0.35, w->height * 0.35, w->width * 0.3, w->height * 0.3);
				break;
			case SUB_CLIENT_DRAG_STATE_BEFORE:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, 5, 5, w->width * 0.1 - 5, w->height - 10);
				break;
			case SUB_CLIENT_DRAG_STATE_AFTER:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, w->width * 0.9, 5, w->width * 0.1 - 5, w->height - 10);
				break;
			case SUB_CLIENT_DRAG_STATE_ABOVE:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, 5, 5, w->width - 10, w->height * 0.1 - 5);
				break;
			case SUB_CLIENT_DRAG_STATE_BELOW:
				XFillRectangle(d->dpy, w->frame, d->gcs.invert, 5, w->height * 0.9, w->width - 10, w->height * 0.1 - 5);
				break;
		}
}

static void
AdjustWeight(short mode,
	SubWin *w,
	XRectangle *box)
{
	if(w && w->parent)
		{
			if((w->parent->flags & SUB_WIN_TILE_HORZ && (mode == SUB_CLIENT_DRAG_LEFT || mode == SUB_CLIENT_DRAG_RIGHT)) ||
				(w->parent->flags & SUB_WIN_TILE_VERT && mode == SUB_CLIENT_DRAG_BOTTOM))
				{
					if(w->parent->flags & SUB_WIN_TILE_HORZ) w->weight = box->width * 100 / SUBWINWIDTH(w->parent);
					else if(w->parent->flags & SUB_WIN_TILE_VERT) w->weight = box->height * 100 / SUBWINHEIGHT(w->parent);

					if(w->weight >= 80) w->weight = 80;
					if(!(w->flags & SUB_WIN_STATE_WEIGHT)) w->flags |= SUB_WIN_STATE_WEIGHT;
					subTileConfigure(w->parent);
					return;
				}
			else if(((w->parent->flags & SUB_WIN_TILE_VERT && (mode == SUB_CLIENT_DRAG_LEFT || mode == SUB_CLIENT_DRAG_RIGHT)) ||
				(w->parent->flags & SUB_WIN_TILE_HORZ && mode == SUB_CLIENT_DRAG_BOTTOM)) && w->parent->parent)
				AdjustWeight(mode, w->parent, box);
		}
}

 /**
	* Movie/resize the client
	* @param mode Attach mode
	* @param w A #SubWin
	* @param bev A #XButtonEvent
	**/

void
subClientDrag(short mode,
	SubWin *w)
{
	if(w)
		{
			XEvent ev;
			Cursor cursor;
			Window win, nil;
			int wx = 0, wy = 0, rx = 0, ry = 0;
			unsigned int mask;
			short state = SUB_CLIENT_DRAG_STATE_START, last_state = SUB_CLIENT_DRAG_STATE_START;
			XRectangle box = { w->x, w->y, w->width, w->height };
			SubWin *w2 = NULL, *p = NULL, *p2 = NULL, *last_w = NULL;

			/* Get window position on root window */
			XQueryPointer(d->dpy, w->frame, &win, &win, &rx, &ry, &wx, &wy, &mask);
			box.x = rx - wx;
			box.y = ry - wy;

			/* Select cursor */
			switch(mode)
				{
					case SUB_CLIENT_DRAG_LEFT:		
					case SUB_CLIENT_DRAG_RIGHT:		cursor = d->cursors.horz;		break;
					case SUB_CLIENT_DRAG_BOTTOM:	cursor = d->cursors.vert;		break;
					case SUB_CLIENT_DRAG_MOVE:		cursor = d->cursors.move;		break;
					default:											cursor = d->cursors.square;	break;
				}

			if(XGrabPointer(d->dpy, w->frame, True, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, 
				GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime)) return;

			XGrabServer(d->dpy);
			if(mode <= SUB_CLIENT_DRAG_MOVE) DrawMask(SUB_CLIENT_DRAG_STATE_START, w, &box);

			for(;;)
				{
					XMaskEvent(d->dpy, PointerMotionMask|ButtonReleaseMask|EnterWindowMask, &ev);
					switch(ev.type)
						{
							/* Button release doesn't return our destination window */
							case EnterNotify: win = ev.xcrossing.window; break;
							case MotionNotify:
								if(mode <= SUB_CLIENT_DRAG_MOVE) 
									{
										DrawMask(SUB_CLIENT_DRAG_STATE_START, w, &box);
							
										/* Calculate dimensions of the selection box */
										switch(mode)
											{
												case SUB_CLIENT_DRAG_LEFT: 	
													box.x			= (rx - wx) - (rx - ev.xmotion.x_root);	
													box.width = w->width + ((rx - wx ) - ev.xmotion.x_root);	
													break;
												case SUB_CLIENT_DRAG_RIGHT:	box.width		= w->width + (ev.xmotion.x_root - rx);	break;
												case SUB_CLIENT_DRAG_BOTTOM: box.height	= w->height + (ev.xmotion.y_root - ry);	break;
												case SUB_CLIENT_DRAG_MOVE:
													box.x	= (rx - wx) - (rx - ev.xmotion.x_root);
													box.y	= (ry - wy) - (ry - ev.xmotion.y_root);
													break;
											}	
										DrawMask(SUB_CLIENT_DRAG_STATE_START, w, &box);
									}
								else if(win != w->frame && mode == SUB_CLIENT_DRAG_SWAP)
									{
										if(!w2 || w2->frame != win) w2 = subWinFind(win);
										if(w2)
											{
												XQueryPointer(d->dpy, win, &nil, &nil, &rx, &ry, &wx, &wy, &mask);
												box.x = rx - wx;
												box.y = ry - wy;

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
														if(last_state != SUB_CLIENT_DRAG_STATE_START) DrawMask(last_state, last_w, &box);
														DrawMask(state, w2, &box);

														last_w		 = w2;
														last_state = state;
													}
												}
										}
								break;
							case ButtonRelease:
								if(win != w->frame && mode > SUB_CLIENT_DRAG_MOVE)
									{
										DrawMask(state, w2, &box); /* Erase mask */

										if(w && w2 && w->parent && w2->parent)
											{
												if(state == SUB_CLIENT_DRAG_STATE_TOP || state == SUB_CLIENT_DRAG_STATE_BOTTOM ||
													state == SUB_CLIENT_DRAG_STATE_LEFT || state == SUB_CLIENT_DRAG_STATE_RIGHT)
													{
														SubWin *p = w->parent;
														SubWin *t = subTileNew(state == SUB_CLIENT_DRAG_STATE_TOP || 
															state == SUB_CLIENT_DRAG_STATE_BOTTOM ? SUB_WIN_TILE_VERT : SUB_WIN_TILE_HORZ);

														subWinReplace(w2, t);
														subWinCut(w);

														/* Check weighted windows */
														if(w2->flags & SUB_WIN_STATE_WEIGHT)
															{
																t->flags 		|= SUB_WIN_STATE_WEIGHT;
																t->weight		= w2->weight;
																w2->flags		&= ~SUB_WIN_STATE_WEIGHT;
																w2->weight	= 0;
															}

														subTileAdd(t, state == SUB_CLIENT_DRAG_STATE_TOP || state == SUB_CLIENT_DRAG_STATE_LEFT ? w : w2);
														subTileAdd(t, state == SUB_CLIENT_DRAG_STATE_TOP || state == SUB_CLIENT_DRAG_STATE_LEFT ? w2 : w);

														subTileConfigure(t->parent);
														subTileConfigure(p);
													}
												else if(state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_AFTER ||
													state == SUB_CLIENT_DRAG_STATE_ABOVE || state == SUB_CLIENT_DRAG_STATE_BELOW)
													{
														SubWin *p = w->parent;

														if((((state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_AFTER) && 
															w2->parent->flags & SUB_WIN_TILE_HORZ)) ||
															((state == SUB_CLIENT_DRAG_STATE_ABOVE || state == SUB_CLIENT_DRAG_STATE_BELOW) && 
															w2->parent->flags & SUB_WIN_TILE_VERT))
															{
																subWinCut(w);

																if(state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_ABOVE) 
																	subWinPrepend(w2, w);
																else subWinAppend(w2, w);

																subTileConfigure(w->parent);
																if(w->parent != p) subTileConfigure(p); 
															}
														else if(w2->parent->parent && 
															(((state == SUB_CLIENT_DRAG_STATE_BEFORE || state == SUB_CLIENT_DRAG_STATE_AFTER) && 
															w2->parent->flags & SUB_WIN_TILE_VERT)) ||
															((state == SUB_CLIENT_DRAG_STATE_ABOVE || state == SUB_CLIENT_DRAG_STATE_BELOW) && 
															w2->parent->flags & SUB_WIN_TILE_HORZ))
															{
																subWinCut(w);

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
										XDrawRectangle(d->dpy, DefaultRootWindow(d->dpy), d->gcs.invert, box.x + 1, box.y + 1, 
											box.width - 3, (w->flags & SUB_WIN_STATE_COLLAPSE) ? d->th - 3 : box.height - 3);
							
										if(w->flags & SUB_WIN_STATE_RAISE) 
											{
												w->x			= box.x;
												w->y			= box.y;
												w->width	= box.width;
												w->height	= box.height;

												subWinResize(w);
												if(w->flags & SUB_WIN_TYPE_TILE) subTileConfigure(w);
											}
										else if(w->parent && mode <= SUB_CLIENT_DRAG_BOTTOM) AdjustWeight(mode, w, &box);
									}

								XUngrabServer(d->dpy);
								XUngrabPointer(d->dpy, CurrentTime);
								return;
						}
				}
		}
}

 /**
	* Toggle states of a window 
	* @param w A #SubWin
	**/

void
subClientToggle(short type,
	SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{
			XEvent event;

			XGrabServer(d->dpy);

			if(w->flags & type)
				{
					w->flags &= ~type;
					switch(type)
						{
							case SUB_WIN_STATE_COLLAPSE:
								subClientSetWMState(w, NormalState);

								/* Map most of the windows */
								XMapWindow(d->dpy, w->client->win);
								XMapWindow(d->dpy, w->client->left);
								XMapWindow(d->dpy, w->client->right);
								XMapWindow(d->dpy, w->client->bottom);

								/* Resize frame */
								XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, w->height);
								break;
							case SUB_WIN_STATE_RAISE: XReparentWindow(d->dpy, w->frame, w->parent->frame, w->x, w->y);	break;
							case SUB_WIN_STATE_FULL:
								/* Map most of the windows */
								XMapWindow(d->dpy, w->client->title);
								XMapWindow(d->dpy, w->client->icon);
								XMapWindow(d->dpy, w->client->left);
								XMapWindow(d->dpy, w->client->right);
								XMapWindow(d->dpy, w->client->bottom);

								XMapWindow(d->dpy, w->client->caption);

								subWinResize(w);
								
								subTileAdd(w->parent, w);
								subTileConfigure(w->parent);
								break;								
							case SUB_WIN_STATE_WEIGHT:
								w->weight = 0;
								if(w->parent) subTileConfigure(w->parent);
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
							case SUB_WIN_STATE_COLLAPSE:
								subClientSetWMState(w, WithdrawnState);

								/* Unmap most of the windows */
								XUnmapWindow(d->dpy, w->client->win);
								XUnmapWindow(d->dpy, w->client->left);
								XUnmapWindow(d->dpy, w->client->right);
								XUnmapWindow(d->dpy, w->client->bottom);

								XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, d->th);
								break;						
							case SUB_WIN_STATE_RAISE:
								/* Respect the user/program preferences */
								hints = XAllocSizeHints();

								if(!hints) subUtilLogError("Can't alloc memory. Exhausted?\n");
								if(XGetWMNormalHints(d->dpy, w->client->win, hints, &supplied))
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
												if(hints->base_width > 0 && hints->base_width <= DisplayWidth(d->dpy, DefaultScreen(d->dpy)) &&
													hints->base_height > 0 && hints->base_height <= DisplayHeight(d->dpy, DefaultScreen(d->dpy))) 
													{
														w->width	= hints->base_width + 2 * d->bw;
														w->height	= hints->base_width + d->th + d->bw;
														w->x			= (DisplayWidth(d->dpy, DefaultScreen(d->dpy)) - w->width) / 2;
														w->y			= (DisplayHeight(d->dpy, DefaultScreen(d->dpy)) - w->height) / 2;
													}
											}
									}

								subWinResize(w);
								XReparentWindow(d->dpy, w->frame, DefaultRootWindow(d->dpy), w->x, w->y);
								XRaiseWindow(d->dpy, w->frame);

								XFree(hints);
								break;
							case SUB_WIN_STATE_FULL:
								/* Unmap some windows */
								XUnmapWindow(d->dpy, w->client->title);
								XUnmapWindow(d->dpy, w->client->icon);
								XUnmapWindow(d->dpy, w->client->left);
								XUnmapWindow(d->dpy, w->client->right);
								XUnmapWindow(d->dpy, w->client->bottom);								

								XUnmapWindow(d->dpy, w->client->caption);

								/* Resize to display size */
								w->x			= 0;
								w->y			= 0;
								w->width	= DisplayWidth(d->dpy, DefaultScreen(d->dpy));
								w->height	= DisplayHeight(d->dpy, DefaultScreen(d->dpy));

								XReparentWindow(d->dpy, w->frame, DefaultRootWindow(d->dpy), 0, 0);
								subWinResize(w);
						}
				}
			XUngrabServer(d->dpy);
			while(XCheckTypedEvent(d->dpy, UnmapNotify, &event));
			if(type != SUB_WIN_STATE_FULL) subTileConfigure(w->parent);
		}
}

 /**
	* Send a configure request to the client
	* @param w A #SubWin
	**/

void
subClientConfigure(SubWin *w)
{
	XConfigureEvent ev;

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

	XSendEvent(d->dpy, w->client->win, False, StructureNotifyMask, (XEvent *)&ev);
}

	/**
	 * Fetch client name
	 * @param w A #SubWin
	 **/

void
subClientFetchName(SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{
			int width, mode;
			if(w->client->name) XFree(w->client->name);
			XFetchName(d->dpy, w->client->win, &w->client->name);

			/* Check max length of the caption */
			width = (strlen(w->client->name) + 1) * d->fx;
			if(width > w->width - d->th - 4) width = w->width - d->th - 14;
			XMoveResizeWindow(d->dpy, w->client->caption, d->th, 0, width, d->th);

			mode = (d->focus && d->focus->frame == w->frame) ? 0 : 1;
			subClientRender(w);
		}
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

	XChangeProperty(d->dpy, w->client->win, subEwmhGetAtom(SUB_EWMH_WM_STATE), subEwmhGetAtom(SUB_EWMH_WM_STATE),
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
	unsigned long nil, bytes;
	long *data = NULL, state = WithdrawnState;

	if(XGetWindowProperty(d->dpy, w->client->win, subEwmhGetAtom(SUB_EWMH_WM_STATE), 0L, 2L, False, 
			subEwmhGetAtom(SUB_EWMH_WM_STATE), &type, &format, &bytes, &nil,
			(unsigned char **) &data) == Success && bytes)
		{
			state = *data;
			XFree(data);
		}
	return(state);
}
