
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

#ifdef HAVE_SYS_INOTIFY_H
#define BUFLEN (sizeof(struct inotify_event))
#endif

 /**
	* Get the current time in seconds 
	* @return Returns the current time
	**/

time_t
subEventGetTime(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);

  return(tv.tv_sec);
}

static void
HandleButtonPress(XButtonEvent *ev)
{
	SubWin *w = NULL;
	static Time last_time = 0;

	if(ev->window == d->bar.views)
		{
			SubView *v = (SubView *)subUtilFind(ev->subwindow, 1);
			if(v) subViewSwitch(v);

			return;
		}

	w = (SubWin *)subUtilFind(ev->window, 1);
	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{
			switch(ev->button)
				{
					case Button1:
						if(last_time > 0 && ev->time - last_time <= 300) /* Double click */
							{
								subUtilLogDebug("Double click: win=%#lx\n", ev->window);
								if((ev->subwindow == w->client->title && w->parent && w->parent->flags & SUB_WIN_TYPE_TILE) || w->flags & SUB_WIN_STATE_FLOAT) 
									subClientToggle(SUB_WIN_STATE_SHADE, w);
								last_time = 0;
							}						
						else  /* Single click */
							{
								subUtilLogDebug("Single click: win=%#lx\n", ev->window);
								if(!(w->flags & SUB_WIN_TYPE_VIEW))
									{
										if(w->flags & SUB_WIN_STATE_FLOAT) XRaiseWindow(d->disp, w->frame);
										if(w->parent && w->parent->flags & SUB_WIN_STATE_STACK) 
											{
												w->parent->tile->pile = w;
												if(w->flags & SUB_WIN_STATE_SHADE) w->flags &= ~SUB_WIN_STATE_SHADE;
												subTileConfigure(w->parent);
											}
										if(ev->subwindow == w->client->left) 				subClientDrag(SUB_CLIENT_DRAG_LEFT, w);
										else if(ev->subwindow == w->client->right)	subClientDrag(SUB_CLIENT_DRAG_RIGHT, w);
										else if(ev->subwindow == w->client->bottom) subClientDrag(SUB_CLIENT_DRAG_BOTTOM, w);
										else if(ev->subwindow == w->client->title || (w->flags & SUB_WIN_TYPE_CLIENT && ev->subwindow == w->client->caption))
											{ 
												/* Either drag and move or drag an swap windows */
												subClientDrag((w->flags & SUB_WIN_STATE_FLOAT) ? SUB_CLIENT_DRAG_MOVE : SUB_CLIENT_DRAG_SWAP, w);
												last_time = ev->time;
											}
									}
							}
						break;
					case Button2:
						if(ev->subwindow == w->client->title) subWinDelete(w);
						break;
					case Button3: 
						if(ev->subwindow == w->client->title) subClientToggle(SUB_WIN_STATE_FLOAT, w); 
						break;
					case Button4: subViewSwitch(d->cv->next); break;
					case Button5: if(d->cv->prev) subViewSwitch(d->cv->prev); break;
				}
		}
}

static void
Exec(char *cmd)
{
	pid_t pid = fork();

	switch(pid)
		{
			case 0:
				setsid();
				execlp("/bin/sh", "sh", "-c", cmd, NULL);

				/* Never to be reached statement */
				subUtilLogWarn("Can't exec command `%s'.\n", cmd);
				exit(1);
			case -1: subUtilLogWarn("Failed to fork.\n");
		}
}

static void
HandleKeyPress(XKeyEvent *ev)
{
	SubWin *w = (SubWin *)subUtilFind(ev->window, 1);
	if(w)
		{
			SubKey *k = subKeyFind(ev->keycode, ev->state);
			if(k) 
				{
					int mode = 0, type = SUB_WIN_TILE_HORZ;

					switch(k->flags)
						{
							case SUB_KEY_ACTION_FOCUS_ABOVE: if(w->prev) subWinFocus(w->prev); break;
							case SUB_KEY_ACTION_FOCUS_BELOW: if(w->next) subWinFocus(w->next); break;


							case SUB_KEY_ACTION_DELETE_WIN: subWinDelete(w); break;
							case SUB_KEY_ACTION_TOGGLE_COLLAPSE:	if(!mode) mode = SUB_WIN_STATE_SHADE;
							case SUB_KEY_ACTION_TOGGLE_RAISE:			if(!mode) mode = SUB_WIN_STATE_FLOAT;
							case SUB_KEY_ACTION_TOGGLE_FULL:			if(!mode) mode = SUB_WIN_STATE_FULL;
								if(!(w->flags & SUB_WIN_TYPE_VIEW)) subClientToggle(mode, w);
								break;									
							case SUB_KEY_ACTION_TOGGLE_PILE: 
								if(w->flags & SUB_WIN_TYPE_TILE) subClientToggle(SUB_WIN_STATE_STACK, w);		
								else if(w->parent && w->parent->flags & SUB_WIN_STATE_STACK) 
									subClientToggle(SUB_WIN_STATE_STACK, w->parent);
								break;
							case SUB_KEY_ACTION_TOGGLE_LAYOUT: 
								if(w->flags & SUB_WIN_TYPE_TILE)
									{
										type = (w->flags & SUB_WIN_TILE_HORZ) ? SUB_WIN_TILE_HORZ : SUB_WIN_TILE_VERT;
										w->flags &= ~type;
										w->flags |= (type == SUB_WIN_TILE_HORZ) ? SUB_WIN_TILE_VERT : SUB_WIN_TILE_HORZ;
										subTileConfigure(w);
									}			
								break;
							case SUB_KEY_ACTION_DESKTOP_NEXT: subViewSwitch(d->cv->next);	break;
							case SUB_KEY_ACTION_DESKTOP_PREV: 
								if(d->cv->prev) subViewSwitch(d->cv->prev);		
								break;
							case SUB_KEY_ACTION_DESKTOP_MOVE:
#if 0
								if(k->number && !(w->flags & SUB_WIN_TYPE_VIEW))
									{
										SubView *s = subViewGetPtr(k->number);
										if(s)
											{
												SubWin *p = w->parent;
												subTileAdd(s->w, w);
												if(p) subTileConfigure(p);
											}
									} 
#endif
								break;
							case SUB_KEY_ACTION_EXEC:	if(k->string) Exec(k->string); break;
						}

					subUtilLogDebug("KeyPress: code=%d, mod=%d\n", k->code, k->mod);
				}
		}
}

static void
HandleConfigure(XConfigureRequestEvent *ev)
{
	XWindowChanges wc;
	SubWin *w = (SubWin *)subUtilFind(ev->window, 1);
	if(w)
		{
			if(ev->value_mask & CWX)			w->x			= ev->x;
			if(ev->value_mask & CWY)			w->y 			= ev->y;
			if(ev->value_mask & CWWidth)	w->width	= ev->width;
			if(ev->value_mask & CWHeight)	w->height = ev->height;

			subClientConfigure(w);
			wc.x = 0;
			wc.y = d->th;
		}
	else
		{
			wc.x = ev->x;
			wc.y = ev->y;
		}

	wc.width			= ev->width;
	wc.height			= ev->height;
	wc.sibling		= ev->above;
	wc.stack_mode	= ev->detail;
	XConfigureWindow(d->disp, ev->window, ev->value_mask, &wc);
}

static void
HandleMap(XMapRequestEvent *ev)
{
	SubWin *w = (SubWin *)subUtilFind(ev->window, 1);
	if(!w) subViewMerge(ev->window);
}

static void
HandleDestroy(XDestroyWindowEvent *ev)
{
	SubWin *w = (SubWin *)subUtilFind(ev->event, 1);
	if(w && w->flags & SUB_WIN_TYPE_CLIENT) 
		{
			w->flags |= SUB_WIN_STATE_DEAD;
			subWinDelete(w); 
		}
}

/* Some events don't propagate but we need them to do so */
static Window
GetParent(Window win)
{
	unsigned int n;
	Window parent, unused, *wins = NULL;

	XQueryTree(d->disp, win, &unused, &parent, &wins, &n);
	XFree(wins);

	return(parent);
}
	
static void
HandleMessage(XClientMessageEvent *ev)
{
	SubWin *w = NULL;

	subUtilLogDebug("ClientMesg: type=%ld\n", ev->message_type);

	if(ev->window == DefaultRootWindow(d->disp))
		{
			if(ev->message_type == subEwmhFind(SUB_EWMH_NET_CURRENT_DESKTOP))
				{
					SubView *v = subViewFind(ev->data.l[0]);
					if(v) subViewSwitch(v);
				}
			else if(ev->message_type == subEwmhFind(SUB_EWMH_NET_ACTIVE_WINDOW))
				{
					SubWin *focus = (SubWin *)subUtilFind(GetParent(ev->data.l[0]), 1);
					if(focus) subWinFocus(focus);
				}
			return;
		}
	w = (SubWin *)subUtilFind(GetParent(ev->window), 1);
	if(w && w->flags & SUB_WIN_TYPE_CLIENT && ev->format == 32)
		{
			subUtilLogDebug("ClientMsg: [0]=%ld, [1]=%ld, [2]=%ld, [3]=%ld, [4]=%ld\n", 
				ev->data.l[0], ev->data.l[1], ev->data.l[2], ev->data.l[3], ev->data.l[4]);

			if(ev->message_type == subEwmhFind(SUB_EWMH_NET_WM_STATE))
				{
					/* [0] => Remove = 0 / Add = 1 / Toggle = 2 - but we always toggle */
					if(ev->data.l[1] == (long)subEwmhFind(SUB_EWMH_NET_WM_STATE_FULLSCREEN))
						{
							subClientToggle(SUB_WIN_STATE_FULL, w);
						}
					else if(ev->data.l[1] == (long)subEwmhFind(SUB_EWMH_NET_WM_STATE_SHADED))
						{
							subClientToggle(SUB_WIN_STATE_SHADE, w);
						}
				}
		}
}

static void
HandleColormap(XColormapEvent *ev)
{	
	SubWin *w = (SubWin *)subUtilFind(ev->window, 1);
	if(w && w->flags & SUB_WIN_TYPE_CLIENT && ev->new)
		{
			w->client->cmap = ev->colormap;
			XInstallColormap(d->disp, w->client->cmap);
		}
}

static void
HandleProperty(XPropertyEvent *ev)
{
	/* Prevent expensive query tree if the atom isn't supported */
	if(ev->atom == XA_WM_NAME || ev->atom == subEwmhFind(SUB_EWMH_WM_NAME))
		{
			SubWin *w = (SubWin *)subUtilFind(GetParent(ev->window), d->cv->xid);
			if(w && w->flags & SUB_WIN_TYPE_CLIENT) subClientFetchName(w);
		}
}

static void
HandleCrossing(XCrossingEvent *ev)
{
	SubWin *w = (SubWin *)subUtilFind(ev->window, 1);
	if(w)
		{
			XEvent event;
		
			if(d->focus != w) subWinFocus(w);

			/* Remove any other event of the same type and window */
			while(XCheckTypedWindowEvent(d->disp, ev->window, ev->type, &event));			
		}
}

static void
HandleExpose(XEvent *ev)
{
	XEvent event;
	SubWin *w = NULL;

	if(ev->xany.window == d->bar.win)
		{
			subSubletConfigure();
			subSubletRender();
		}
	else
		{
  		w = (SubWin *)subUtilFind(ev->xany.window, 1);
			if(w) subWinRender(w);
		}

	/* Remove any other event of the same type and window */
	while(XCheckTypedWindowEvent(d->disp, ev->xany.window, ev->type, &event));
}

static void
HandleFocus(XFocusInEvent *ev)
{
	SubWin *w = (SubWin *)subUtilFind(ev->window, 1);
	if(w && w->flags & SUB_WIN_PREF_FOCUS)
		{
			if(w != d->focus) 
				{
					/* Remove focus from window */
					if(d->focus) 
					{
						SubWin *f = d->focus;
						d->focus = NULL;
						if(f && f->flags & SUB_WIN_TYPE_CLIENT) subClientRender(f);

						subKeyUngrab(f);
					}

					d->focus = w;
					subClientRender(w);
					subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_ACTIVE_WINDOW, &w->frame, 1);

					subKeyGrab(w);					
				}
		}
}

 /**
	* Handle the X events 
	* @return Return zero on failure
	**/

int 
subEventLoop(void)
{
	time_t current;
	XEvent ev;
	struct timeval tv;
	SubSublet *s = NULL;

#ifdef HAVE_SYS_INOTIFY_H
	char buf[BUFLEN];
#endif /* HAVE_SYS_INOTIFY_H */

	while(1)
		{
			s = subSubletNext();
			if(s)
				{
					fd_set fdset;
					current	= subEventGetTime();

					while(s->time <= current)
						{
							s->time = current + s->interval;

							subLuaCall(s);
							subSubletMerge(1);

							s = subSubletNext();
						}
					subSubletRender();

					tv.tv_sec		= s->interval;
					tv.tv_usec	= 0;
					FD_ZERO(&fdset);
					FD_SET(ConnectionNumber(d->disp), &fdset);

#ifdef HAVE_SYS_INOTIFY_H
					/* Add inotify socket to the set */
					FD_SET(d->notify, &fdset);
#endif /* HAVE_SYS_INOTIFY_H */

					/* Forcefully ignore EINTR */
					if(select(ConnectionNumber(d->disp) + 1, &fdset, NULL, NULL, &tv) == -1 && errno != EINTR)
						subUtilLogDebug("%s\n", strerror(errno));

#ifdef HAVE_SYS_INOTIFY_H
					if(read(d->notify, buf, BUFLEN) > 0)
						{
							struct inotify_event *event = (struct inotify_event *)&buf[0];
							if(event)
								{
									SubSublet *ws = (SubSublet *)subUtilFind(d->bar.sublets, event->wd);
									if(ws)
										{
											subLuaCall(ws);
											subSubletConfigure();
											subSubletRender();
										}
								}
						}
#endif /* HAVE_SYS_INOTIFY_H */
				}

			while(XPending(d->disp))
				{
					XNextEvent(d->disp, &ev);
					switch(ev.type)
						{
							case ButtonPress:				HandleButtonPress(&ev.xbutton);					break;
							case KeyPress:					HandleKeyPress(&ev.xkey);								break;
							case ConfigureRequest:	HandleConfigure(&ev.xconfigurerequest);	break;
							case MapRequest: 				HandleMap(&ev.xmaprequest); 						break;
							case DestroyNotify: 		HandleDestroy(&ev.xdestroywindow);			break;
							case ClientMessage: 		HandleMessage(&ev.xclient); 						break;
							case ColormapNotify: 		HandleColormap(&ev.xcolormap); 					break;
							case PropertyNotify: 		HandleProperty(&ev.xproperty); 					break;
							case EnterNotify:				HandleCrossing(&ev.xcrossing);					break;
							case VisibilityNotify:	
							case Expose:						HandleExpose(&ev);											break;
							case FocusIn:						HandleFocus(&ev.xfocus);								break;
						}
				}
		}
}
