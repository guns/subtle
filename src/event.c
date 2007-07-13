
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

#include <sys/types.h>
#include "subtle.h"

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

	if(ev->window == d->bar.screens)
	{
		SubScreen *s = subScreenFind(ev->subwindow);
		if(s) subScreenSwitch(subScreenGetPos(s));

		return;
	}
	w = subWinFind(ev->window);
	if(w)
		{
			switch(ev->button)
				{
					case Button1:
						if(last_time > 0 && ev->time - last_time <= 300) /* Double click */
							{
								subLogDebug("Double click: win=%#lx\n", ev->window);
								if((ev->subwindow == w->title && w->parent && w->parent->flags & SUB_WIN_TYPE_TILE) || w->flags & SUB_WIN_OPT_RAISE) 
									subWinToggle(SUB_WIN_OPT_COLLAPSE, w);
								else if(ev->subwindow == w->icon) subWinToggle(SUB_WIN_OPT_WEIGHT, w);
								last_time = 0;
							}						
						else  /* Single click */
							{
								subLogDebug("Single click: win=%#lx\n", ev->window);
								if(!(w->flags & SUB_WIN_TYPE_SCREEN))
									{
										if(w->flags & SUB_WIN_OPT_RAISE) 		subWinRaise(w);
										if(ev->subwindow == w->left) 				subWinDrag(SUB_WIN_DRAG_LEFT, w);
										else if(ev->subwindow == w->right)	subWinDrag(SUB_WIN_DRAG_RIGHT, w);
										else if(ev->subwindow == w->bottom) subWinDrag(SUB_WIN_DRAG_BOTTOM, w);
										else if(ev->subwindow == w->icon) 	last_time = ev->time;
										else if(ev->subwindow == w->title || (w->flags & SUB_WIN_TYPE_CLIENT && ev->subwindow == w->client->caption))
											{ /* Either drag and move or drag an swap windows */
												subWinDrag((w->flags & SUB_WIN_OPT_RAISE) ? SUB_WIN_DRAG_MOVE : SUB_WIN_DRAG_SWAP, w);
												last_time = ev->time;
											}
									}
								if(w->flags & SUB_WIN_TYPE_TILE)
									{
										if(ev->subwindow == w->tile->btnew) subTileAdd(w, subTileNew(SUB_WIN_TILE_VERT));
										else if(ev->subwindow == w->tile->btdel) subWinDelete(w);
									}
							}
						break;
					case Button2:
						if(ev->subwindow == w->title)
							{
								subWinDelete(w);
							}
						break;
					case Button3: 
						if(w->flags & SUB_WIN_TYPE_TILE && ev->subwindow == w->tile->btnew) subTileAdd(w, subTileNew(SUB_WIN_TILE_HORZ));
						else if(ev->subwindow == w->title) subWinToggle(SUB_WIN_OPT_RAISE, w); 
						break;
					case Button4: subScreenSwitch(SUB_SCREEN_NEXT); break;
					case Button5: subScreenSwitch(SUB_SCREEN_PREV); break;
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
				subLogWarn("Can't exec command `%s'.\n", cmd);
				exit(1);
			case -1:
				subLogWarn("Failed to fork.\n");
		}
}

static void
HandleKeyPress(XKeyEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(w)
		{
			SubKey *k = subKeyFind(ev->keycode, ev->state);
			if(k) 
				{
					int type = SUB_WIN_TILE_HORZ;
					SubWin *p = NULL;
					SubScreen *s = NULL;

					switch(k->flags)
						{
							case SUB_KEY_ACTION_ADD_VTILE: 
								type = SUB_WIN_TILE_VERT;
							case SUB_KEY_ACTION_ADD_HTILE: 
								if(w->flags & SUB_WIN_TYPE_TILE) subTileAdd(w, subTileNew(type));
								else if(w->flags & SUB_WIN_TYPE_CLIENT && w->parent && w->parent->flags & SUB_WIN_TYPE_TILE)
									subTileAdd(w->parent, subTileNew(type));
								break;
							case SUB_KEY_ACTION_DELETE_WIN: subWinDelete(w);												break;
							case SUB_KEY_ACTION_TOGGLE_COLLAPSE: 
								if(!(w->flags & SUB_WIN_TYPE_SCREEN)) subWinToggle(SUB_WIN_OPT_COLLAPSE, w);
								break;
							case SUB_KEY_ACTION_TOGGLE_RAISE: 
								if(!(w->flags & SUB_WIN_TYPE_SCREEN)) subWinToggle(SUB_WIN_OPT_RAISE, w); 						
								break;
							case SUB_KEY_ACTION_DESKTOP_NEXT: subScreenSwitch(SUB_SCREEN_NEXT);			break;
							case SUB_KEY_ACTION_DESKTOP_PREV: subScreenSwitch(SUB_SCREEN_PREV);			break;
							case SUB_KEY_ACTION_DESKTOP_MOVE:
								if(k->number && !(w->flags & SUB_WIN_TYPE_SCREEN))
									{
										s = subScreenGetPtr(k->number);
										if(s)
											{
												p = w->parent;
												subTileAdd(s->w, w);
												subTileConfigure(p);
											}
									}
								break;
							case SUB_KEY_ACTION_EXEC:	if(k->string) Exec(k->string);							break;
						}

					subLogDebug("KeyPress: code=%d, mod=%d\n", k->code, k->mod);
				}
		}
}

static void
HandleConfigure(XConfigureRequestEvent *ev)
{
	XWindowChanges wc;
	SubWin *w = subWinFind(ev->window);
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
	XConfigureWindow(d->dpy, ev->window, ev->value_mask, &wc);
}

static void
HandleMap(XMapRequestEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(!w) 
		{
			SubScreen *s = subScreenGetPtr(SUB_SCREEN_ACTIVE);
			w = subClientNew(ev->window);
			if(!(w->flags & SUB_WIN_OPT_TRANS)) subTileAdd(s->w, w);
			else 
				{
					w->parent = s->w;
					subWinToggle(SUB_WIN_OPT_RAISE, w);
				}
		}
}

static void
HandleDestroy(XDestroyWindowEvent *ev)
{
	SubWin *w = subWinFind(ev->event);
	if(w && w->flags & SUB_WIN_TYPE_CLIENT) subWinDelete(w); 
}
	
static void
HandleMessage(XClientMessageEvent *ev)
{
	SubWin *w = NULL;
	unsigned int n = 0, i;
	Window parent, nil, *wins = NULL;

	/* Client messages never propagate.. */
	XQueryTree(d->dpy, ev->window, &nil, &parent, &wins, &n);
	XFree(wins);

	if(parent) w = subWinFind(parent);
	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{
			printf("message_type=%d\n", ev->message_type);
			if(ev->format == 32)
				{
					for(i = 0; i < 5; i++)
						printf("-> l[%d]=%d\n", i, ev->data.l[i]);
				}
			if(ev->message_type == subEwmhGetAtom(SUB_EWMH_NET_WM_STATE))
				{
					if(ev->data.l[1] == subEwmhGetAtom(SUB_EWMH_NET_WM_STATE_FULLSCREEN))
						{
							if(ev->data.l[0] == 0) /* Remove state */
								{
									subWinToggle(SUB_WIN_OPT_FULL, w);
								}						

							else if(ev->data.l[0] == 1) /* Add state */
								{
									subWinToggle(SUB_WIN_OPT_FULL, w);
								}
						}
				}
		}
}

static void
HandleColormap(XColormapEvent *ev)
{	
	SubWin *w = subWinFind(ev->window);
	if(w && w->flags & SUB_WIN_TYPE_CLIENT && ev->new)
		{
			w->client->cmap = ev->colormap;
			XInstallColormap(d->dpy, w->client->cmap);
		}
}

static void
HandleProperty(XPropertyEvent *ev)
{
	SubWin *w = NULL;
	unsigned int n = 0, i;
	Window parent, nil, *wins = NULL;

	/* Property changes never propagate.. */
	XQueryTree(d->dpy, ev->window, &nil, &parent, &wins, &n);
	XFree(wins);

	if(parent) w = subWinFind(parent);
	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{
			printf("Property: atom=%ld\n", ev->atom);
			if(ev->atom == XA_WM_NAME || ev->atom == subEwmhGetAtom(SUB_EWMH_NET_WM_NAME)) subClientFetchName(w);
		}
}

static void
HandleCrossing(XCrossingEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(w)
		{
			/* Make leave events to enter event of the parent window */
			if(ev->type == LeaveNotify && !ev->mode && w->parent) w = w->parent; 

			/* Remove focus from window */
			if(d->focus) 
				{
					SubWin *f = d->focus;
					d->focus = NULL;
					if(f) subWinRender(f);

					subKeyUngrab(f);
				}

			d->focus = w;
			subWinRender(w);
			subEwmhSetWindow(DefaultRootWindow(d->dpy), SUB_EWMH_NET_ACTIVE_WINDOW, w->frame);

			/* Grab key */
			subKeyGrab(w);

			/* Focus */
			if(w->flags & SUB_WIN_TYPE_CLIENT && !(w->flags & SUB_WIN_OPT_COLLAPSE))
				{
					if(w->flags & SUB_WIN_PREF_INPUT) XSetInputFocus(d->dpy, w->win, RevertToNone, CurrentTime);
					if(w->flags & SUB_WIN_PREF_FOCUS)
						{
							XEvent ev;

							ev.type									= ClientMessage;
							ev.xclient.window				= w->win;
							ev.xclient.message_type = subEwmhGetAtom(SUB_EWMH_WM_PROTOCOLS);
							ev.xclient.format				= 32;
							ev.xclient.data.l[0]		= subEwmhGetAtom(SUB_EWMH_WM_TAKE_FOCUS);
							ev.xclient.data.l[1]		= CurrentTime;

							XSendEvent(d->dpy, w->win, False, NoEventMask, &ev);
						}
					subLogDebug("Focus: win=%#lx, input=%d, send=%d\n", w->win, 
						w->flags & SUB_WIN_PREF_INPUT ? 1 : 0, w->flags & SUB_WIN_PREF_FOCUS ? 1 : 0);
				}
			else if(!(w->flags & SUB_WIN_OPT_COLLAPSE)) XSetInputFocus(d->dpy, w->win, RevertToNone, CurrentTime);
		}
}

static void
HandleExpose(XEvent *ev)
{
	XEvent event;
	Window win = ev->type == Expose ? ev->xexpose.window : ev->xvisibility.window;
	SubWin *w = subWinFind(win);
	if(w) subWinRender(w);

	/* Remove any other event of the same type and window */
	while(XCheckTypedWindowEvent(d->dpy, win, ev->type, &event));
}

 /**
	* Handle the X events 
	* @return Return zero on failure
	**/

int subEventLoop(void)
{
	time_t current;
	XEvent ev;
	struct timeval tv;
	SubSublet *s = NULL;

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
							subSubletRender(s);
							subSubletSift(1);

							s = subSubletNext();
						}

					tv.tv_sec		= s->interval;
					tv.tv_usec	= 0;
					FD_ZERO(&fdset);
					FD_SET(ConnectionNumber(d->dpy), &fdset);

					if(select(ConnectionNumber(d->dpy) + 1, &fdset, NULL, NULL, &tv) == -1)
						subLogDebug("Failed to select the connection\n");
				}

			while(XPending(d->dpy))
				{
					XNextEvent(d->dpy, &ev);
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
							case EnterNotify:
							case LeaveNotify:				HandleCrossing(&ev.xcrossing);					break;
							case VisibilityNotify:	
							case Expose:						HandleExpose(&ev);											break;
						}
				}
		}
}
