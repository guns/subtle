
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
} /*  */

 /**
	* Create a new window and append it to the window list 
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

	attrs.border_pixel			= d->colors.border;
	attrs.background_pixmap	= ParentRelative;
	attrs.background_pixel	= d->colors.norm;
	attrs.event_mask				= KeyPressMask|ButtonPressMask|ButtonMotionMask|EnterWindowMask|LeaveWindowMask|
		ExposureMask|VisibilityChangeMask|SubstructureRedirectMask|SubstructureNotifyMask;

	/* Create windows */
	mask			= CWBackPixmap|CWEventMask;
	w->frame	= SUBWINNEW(DefaultRootWindow(d->dpy), 0, 0, w->width, w->height, 0);
	mask			= CWBackPixel;
	w->title	= SUBWINNEW(w->frame, 0, 0, w->width, d->th, 0);
	mask 			= CWBorderPixel|CWBackPixel;
	w->icon		= SUBWINNEW(w->frame, 2, 2, d->th - 6, d->th - 6, 1);
	
	/* Create borders */
	mask					|= CWCursor;
	attrs.cursor	= d->cursors.horz;
	w->left				= SUBWINNEW(w->frame, 0, d->th, d->bw, w->height - d->th, 0);
	w->right			= SUBWINNEW(w->frame, w->width - d->bw, d->th, d->bw, w->height - d->th, 0);
	attrs.cursor	= d->cursors.vert;
	w->bottom			= SUBWINNEW(w->frame, 0, w->height - d->bw, w->width, d->bw, 0);
	
	if(!win)
		{
			mask		= CWBackPixmap;
			w->win	= SUBWINNEW(w->frame, d->bw, d->th, SUBWINWIDTH(w), SUBWINHEIGHT(w), 0);
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
	* Delete a window 
	* @param w A #SubWin
	**/

void
subWinDelete(SubWin *w)
{
	if(w)
		{
			SubWin *p = w->parent;

			if(d->focus == w && p) d->focus = p;

			XDeleteContext(d->dpy, w->frame, 1);
			XDestroySubwindows(d->dpy, w->frame);
			XDestroyWindow(d->dpy, w->frame);

			if(p && p->flags & SUB_WIN_TYPE_TILE) subTileConfigure(p);
			free(w);
		}
}

 /**
	* Render a window 
	* @param mode Render mode
	* @param w A #SubWin
	**/

void
subWinRender(short mode,
	SubWin *w)
{
	if(w)
		{
			char c = 's';
			unsigned long col = mode ? (w->flags & SUB_WIN_OPT_COLLAPSE ? d->colors.cover : d->colors.norm) : d->colors.focus;

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

			/* Titlebar */
			XFillRectangle(d->dpy, w->title, d->gcs.border, d->th + 1, 2, w->width - d->th - 4, d->th - 4);	

			/* Icon */
			XSetWindowBorder(d->dpy, w->icon, d->colors.border);
			if(w->flags & SUB_WIN_OPT_RAISE)					c = 'r';
			else if(w->flags & SUB_WIN_OPT_COLLAPSE)	c = 'c';
			else if(w->flags & SUB_WIN_OPT_PILE)			c = 'p';
			else if(w->flags & SUB_WIN_OPT_WEIGHT)		c = 'w';
	
			XDrawString(d->dpy, w->icon, d->gcs.font, (d->th - 8 - d->fx) / 2 + 1, d->fy - 5, &c, 1);
	}
}

void
DrawMask(short type,
	SubWin *w,
	XRectangle *box)
{
	if(w)
		{
			switch(type)
				{
					case SUB_WIN_DRAG_STATE_START: 
						XDrawRectangle(d->dpy, DefaultRootWindow(d->dpy), d->gcs.invert, box->x + 1, box->y + 1, 
							box->width - 3, (w->flags & SUB_WIN_OPT_COLLAPSE) ? d->th - 3 : box->height - 3);
						break;
					case SUB_WIN_DRAG_STATE_APPEND:
						XDrawRectangle(d->dpy, w->frame, d->gcs.invert, d->bw + 1, d->th + 1, 
							SUBWINWIDTH(w) - 3, SUBWINHEIGHT(w) - 3);
						XDrawLine(d->dpy, w->frame, d->gcs.invert, d->bw + 3, d->th + SUBWINHEIGHT(w) - 3, 
							SUBWINWIDTH(w) - 3, d->th + 3);
						break;
					case SUB_WIN_DRAG_STATE_BELOW:
						XDrawLine(d->dpy, w->frame, d->gcs.invert, 0, w->height - 3, w->width, w->height - 3);
						break;
					case SUB_WIN_DRAG_STATE_SWAP:
						XDrawRectangle(d->dpy, w->frame, d->gcs.invert, 1, 1, w->width - 3, w->height - 3);
						XDrawLine(d->dpy, w->frame, d->gcs.invert, 3, w->height - 3, w->width - 3, 3);
						break;
				}
		}
}

 /**
	* Movie/resize the window 
	* @param mode Attach mode
	* @param w A #SubWin
	* @param bev A #XButtonEvent
	**/

void
subWinDrag(short mode,
	SubWin *w)
{
	XEvent ev;
	Cursor cursor;
	Window win;
	SubWin *w2 = NULL, *p = NULL, *p2 = NULL, *last_w = NULL;

	short state = SUB_WIN_DRAG_STATE_START, last_state = SUB_WIN_DRAG_STATE_START;

	XRectangle box = { w->x, w->y, w->width, w->height };

	unsigned int mask;
	int wx = 0, wy = 0, rx = 0, ry = 0;

	/* Get window position on root window */
	XQueryPointer(d->dpy, w->frame, &win, &win, &rx, &ry, &wx, &wy, &mask);

	box.x = rx - wx;
	box.y = ry - wy;

	/* Select cursor */
	switch(mode)
		{
			case SUB_WIN_DRAG_LEFT:		
			case SUB_WIN_DRAG_RIGHT:	cursor = d->cursors.horz;		break;
			case SUB_WIN_DRAG_BOTTOM:	cursor = d->cursors.vert;		break;
			case SUB_WIN_DRAG_MOVE:		cursor = d->cursors.move;		break;
			default:									cursor = d->cursors.square;	break;
		}

	if(XGrabPointer(d->dpy, w->frame, True, SUBPOINTERMASK, GrabModeAsync, GrabModeAsync, None, 
		cursor, CurrentTime)) return;

	XGrabServer(d->dpy);
	if(mode <= SUB_WIN_DRAG_MOVE) DrawMask(SUB_WIN_DRAG_STATE_START, w, &box);

	for(;;)
		{
			XMaskEvent(d->dpy, PointerMotionMask|ButtonReleaseMask|EnterWindowMask, &ev);
			switch(ev.type)
				{
					/* Button release doesn't return our destination window */
					case EnterNotify: win = ev.xcrossing.window; break;
					case MotionNotify:
						if(mode <= SUB_WIN_DRAG_MOVE) 
							{
								DrawMask(SUB_WIN_DRAG_STATE_START, w, &box);
					
								/* Calculate dimensions of the selection box */
								switch(mode)
									{
										case SUB_WIN_DRAG_LEFT: 	
											box.x			= (rx - wx) - (rx - ev.xmotion.x_root);	
											box.width = w->width + ((rx - wx ) - ev.xmotion.x_root);	
											break;
										case SUB_WIN_DRAG_RIGHT:	box.width		= w->width + (ev.xmotion.x_root - rx);	break;
										case SUB_WIN_DRAG_BOTTOM: box.height	= w->height + (ev.xmotion.y_root - ry);	break;
										case SUB_WIN_DRAG_MOVE:
											box.x	= (rx - wx) - (rx - ev.xmotion.x_root);
											box.y	= (ry - wy) - (ry - ev.xmotion.y_root);
											break;
									}	
								DrawMask(SUB_WIN_DRAG_STATE_START, w, &box);
							}
						else if(mode == SUB_WIN_DRAG_SWAP)
							{
								/* Exclude own window to reduce pointless redraws */
								if(win != w->frame)
									{
										if(!w2 || w2->frame != win) w2 = subWinFind(win);
										if(w2)
											{
												if(state != SUB_WIN_DRAG_STATE_BELOW && ev.xmotion.y >= w2->height - d->th)
													{
														state = SUB_WIN_DRAG_STATE_BELOW;
													}
												else if(state != SUB_WIN_DRAG_STATE_APPEND && w2->flags & SUB_WIN_TYPE_TILE && 
													ev.xmotion.y >= d->th && ev.xmotion.y <= w2->height - d->th)
													{
														state = SUB_WIN_DRAG_STATE_APPEND;
													}
												else if(state != SUB_WIN_DRAG_STATE_SWAP && 
													((w2->flags & SUB_WIN_TYPE_CLIENT && ev.xmotion.y < w2->height - d->th) ||
													(w2->flags & SUB_WIN_TYPE_TILE && ev.xmotion.y <= d->th)))
													{
														state = SUB_WIN_DRAG_STATE_SWAP;
													}

												if(last_state != state || last_w != w2) 
													{
														if(last_state != SUB_WIN_DRAG_STATE_START) DrawMask(last_state, last_w, &box);
														DrawMask(state, w2, &box);

														last_w		 = w2;
														last_state = state;
													}
											}
									}
								}
						break;
					case ButtonRelease:
						if(win != w->frame && mode > SUB_WIN_DRAG_MOVE)
							{
								//w2 = subWinFind(win);
						
								if(w && w2 && w->parent && w2->parent)
									{
										/* Append a window to tile */
										if(w2->flags & SUB_WIN_TYPE_TILE && state == SUB_WIN_DRAG_STATE_APPEND)
											{
												SubWin *p = w->parent;
		
												subTileAdd(w2, w);
												subTileConfigure(w2->parent);
												subTileConfigure(p);
											}
										/* Append a window below */
										else if(state == SUB_WIN_DRAG_STATE_BELOW)
											{
												unsigned int n, i, j, k;
												Window nil, *wins = NULL;

												XQueryTree(d->dpy, w2->parent->win, &nil, &nil, &wins, &n);	

												/* Reverse array */
												for(i = 0; i < n / 2; i++)
													{
														nil							= wins[i];
														wins[i]					= wins[n - 1 - i];
														wins[n - 1 - i]	= nil;
													}

												wins = (Window *)realloc(wins, (n + 2) * sizeof(Window));
												wins[n + 1] = 0;

												/* Loop backwards */
												for(i = 0; i < n; i++)
													{
														if(wins[i] == w2->frame)
															{
																for(j = n + 1; j > i + 1; j--)
																	{	
																		wins[j] = wins[j - 1];
																	}
																wins[i] = w->frame;

																XReparentWindow(d->dpy, w->frame, w2->parent->win, 0, 0);
																XRestackWindows(d->dpy, wins, n + 1);
																subTileConfigure(w->parent);
																subTileConfigure(w2->parent);
																break;
															}
													}						
													
												XFree(wins);
											}
										/* Swap two windows manually */
										else if(state == SUB_WIN_DRAG_STATE_SWAP)
											{
												Window swap;
												SubTile *tile = NULL;
												SubClient *client = NULL;

												XReparentWindow(d->dpy, w->win, w2->frame, d->bw, d->th);
												XReparentWindow(d->dpy, w2->win, w->frame, d->bw, d->th);

												/* Swap titlebar fields */
												if(w->flags & SUB_WIN_TYPE_CLIENT && w2->flags & SUB_WIN_TYPE_CLIENT)
													{
														XReparentWindow(d->dpy, w->client->caption, w2->frame, d->th, 0); 
														XReparentWindow(d->dpy, w2->client->caption, w->frame, d->th, 0); 

														client = w->client; w->client = w2->client; w2->client = client;
													}
												else if(!(w->flags & SUB_WIN_TYPE_TILE && w2->flags & SUB_WIN_TYPE_TILE))
													{
														if(w->flags & SUB_WIN_TYPE_TILE && w2->flags & SUB_WIN_TYPE_CLIENT)
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

												swap = w->win;		w->win		= w2->win;		w2->win		= swap; 
												swap = w->flags;	w->flags	= w2->flags;	w2->flags	= swap;

												/* Swap some special flags */
												if(w->flags & SUB_WIN_OPT_RAISE)
													{ 
														w->flags		&= ~SUB_WIN_OPT_RAISE;
														w2->flags	|= SUB_WIN_OPT_RAISE;

														subWinResize(w2); 
														if(w->flags & SUB_WIN_TYPE_TILE) subTileConfigure(w2);
													}
												else if(w2->flags & SUB_WIN_OPT_RAISE)
													{
														w2->flags	&= ~SUB_WIN_OPT_RAISE;
														w->flags		|= SUB_WIN_OPT_RAISE;

														subWinResize(w); 
														if(w->flags & SUB_WIN_TYPE_TILE) subTileConfigure(w);
													}

												/* Configure only the non-raised tiles to save configure cycles */
												if(!(w->flags & SUB_WIN_OPT_RAISE)) subTileConfigure(w->parent);
												if(!(w2->flags & SUB_WIN_OPT_RAISE)) subTileConfigure(w2->parent);

												subLogDebug("Swap: %#lx (%#lx) <=> %#lx (%#lx)\n", 
													w->win, w->frame, w2->win, w2->frame);
											}
									}
								}
						else /* Resize */
							{
								XDrawRectangle(d->dpy, DefaultRootWindow(d->dpy), d->gcs.invert, box.x + 1, box.y + 1, 
									box.width - 3, (w->flags & SUB_WIN_OPT_COLLAPSE) ? d->th - 3 : box.height - 3);
					
								if(w->flags & SUB_WIN_OPT_RAISE) 
									{
										w->x			= box.x;
										w->y			= box.y;
										w->width	= box.width;
										w->height	= box.height;

										subWinResize(w);
										if(w->flags & SUB_WIN_TYPE_TILE) subTileConfigure(w);
									}
								else if(w->parent && w->parent->flags & SUB_WIN_TYPE_TILE && mode <= SUB_WIN_DRAG_BOTTOM)
									{
										if(!(w->flags & SUB_WIN_OPT_WEIGHT)) w->flags |= SUB_WIN_OPT_WEIGHT;

										/* Adjust window weight */
										if(w->parent->flags & SUB_WIN_TYPE_TILE && mode == SUB_WIN_DRAG_LEFT || mode == SUB_WIN_DRAG_RIGHT)
											w->weight = box.width * 100 / SUBWINWIDTH(w->parent);
										else if(w->parent->flags & SUB_WIN_TYPE_TILE && mode == SUB_WIN_DRAG_BOTTOM)
											w->weight = box.height * 100 / SUBWINHEIGHT(w->parent); 
										if(w->weight >= 80) w->weight = 80;

										subTileConfigure(w->parent);
									}
							}

						XUngrabServer(d->dpy);
						XUngrabPointer(d->dpy, CurrentTime);
						return;
				}
		}
}

 /**
	* Toggle states of a window 
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

			if(w->flags & type)
				{
					w->flags &= ~type;

					switch(type)
						{
							case SUB_WIN_OPT_COLLAPSE:
								/* Set state */
								if(w->flags & SUB_WIN_TYPE_CLIENT) subClientSetWMState(w, NormalState);

								/* Map most of the windows */
								XMapWindow(d->dpy, w->win);
								XMapWindow(d->dpy, w->left);
								XMapWindow(d->dpy, w->right);
								XMapWindow(d->dpy, w->bottom);

								/* Resize frame */
								XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, w->height);
								break;
							case SUB_WIN_OPT_RAISE: XReparentWindow(d->dpy, w->frame, w->parent->win, w->x, w->y);	break;
							case SUB_WIN_OPT_WEIGHT:
								w->weight = 0;
								if(w->parent) subTileConfigure(w->parent);
						}
				}
			else 
				{
					w->flags |= type;

					switch(type)
						{
							case SUB_WIN_OPT_COLLAPSE:
								/* Set state */
								if(w->flags & SUB_WIN_TYPE_CLIENT) subClientSetWMState(w, WithdrawnState);

								/* Unmap most of the windows */
								XUnmapWindow(d->dpy, w->win);
								XUnmapWindow(d->dpy, w->left);
								XUnmapWindow(d->dpy, w->right);
								XUnmapWindow(d->dpy, w->bottom);

								/* Resize frame */
								XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, d->th);
								break;
							case SUB_WIN_OPT_RAISE:
								/* Respect the user/program preferences */
								if(w->flags & SUB_WIN_TYPE_CLIENT)
									{
										XSizeHints *hints = NULL;
										long supplied;

										hints = XAllocSizeHints();
										if(!hints) subLogError("Can't alloc memory. Exhausted?\n");
										if(!(XGetWMSizeHints(d->dpy, w->win, hints, &supplied, subEwmhGetAtom(SUB_EWMH_WM_SIZE_HINTS))))
											{
												if(hints->flags & (USSize|PSize))
													{
														w->width	= hints->width;
														w->height	= hints->height;
													}
												else if(hints->flags & PBaseSize)
													{
														w->width	= hints->base_width;
														w->height	= hints->base_height;
													}
												if(hints->flags & (USPosition|PPosition))
													{
														w->x = hints->x;
														w->y = hints->y;
													}
												else if(hints->flags & PAspect)
													{
														w->x = (hints->min_aspect.x - hints->max_aspect.x) / 2;
														w->y = (hints->min_aspect.y - hints->max_aspect.y) / 2;
													}
											}
										XFree(hints);
									}
								else
									{
										w->x = (DisplayWidth(d->dpy, DefaultScreen(d->dpy)) - w->width) / 2;
										w->y = (DisplayHeight(d->dpy, DefaultScreen(d->dpy)) - w->height) / 2;
									}

								subWinResize(w);
								XReparentWindow(d->dpy, w->frame, DefaultRootWindow(d->dpy), w->x, w->y);
								subWinRaise(w);

						}
				}
			XUngrabServer(d->dpy);
			while(XCheckTypedEvent(d->dpy, UnmapNotify, &event));
			subTileConfigure(w->parent);
		}
}

 /**
	* Change the stacking order of the windows 
	* @param w A #SubWin
	**/

void
subWinRestack(SubWin *w)
{
	if(w)
		{
			int n = 6;
			Window wins[6] = { w->icon, w->title, w->left, w->right, w->bottom, w->win };
			XRestackWindows(d->dpy, wins, n);
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
			XMoveResizeWindow(d->dpy, w->frame, w->x, w->y, w->width, 
				(w->flags & SUB_WIN_OPT_COLLAPSE) ? d->th : w->height);
			XMoveResizeWindow(d->dpy, w->title, 0, 0, w->width, d->th);

			if(!(w->flags & SUB_WIN_OPT_COLLAPSE))
				{
					XMoveResizeWindow(d->dpy, w->win, d->bw, d->th, w->width - 2 * d->bw, w->height - d->th - d->bw);
					XMoveResizeWindow(d->dpy, w->right, w->width - d->bw, d->th, d->bw, w->height - d->th);
					XMoveResizeWindow(d->dpy, w->bottom, 0, w->height - d->bw, w->width, d->bw);
					if(w->flags & SUB_WIN_TYPE_CLIENT) subClientSendConfigure(w);
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

 /**
	* Lower a window 
	* @param w A #SubWin
	**/

void
subWinLower(SubWin *w)
{
	if(w)
		{
			XLowerWindow(d->dpy, w->frame);
			XLowerWindow(d->dpy, w->win);
		}
}

 /**
	* Raise a window 
	* @param w A #SubWin
	**/

void
subWinRaise(SubWin *w)
{
	if(w)
		{
			XRaiseWindow(d->dpy, w->frame);
			XRaiseWindow(d->dpy, w->win);
		}
}
