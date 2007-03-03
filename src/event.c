#include <sys/time.h>
#include <sys/types.h>
#include "subtle.h"

 /**
	* Get the current time in seconds
	* @return Returns the current time.
	**/

int
subEventGetTime(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);

  return(tv.tv_sec);
}

static void 
RenderWindow(SubWin *w)
{
	short mode;
	Window win;

	mode = (d->focus == w->frame) ? 0 : 1;
	if(SUBISSCREEN(w))
		{
			XExposeEvent event;
			unsigned int n = 0, i;
			Window nil, *wins = NULL;

			XSetWindowBackground(d->dpy, screen->statusbar, mode ? d->colors.norm : d->colors.focus);
			XClearWindow(d->dpy, screen->statusbar);

			/* Forward Expose to every sublet */
			event.type = Expose;
			XQueryTree(d->dpy, screen->statusbar, &nil, &nil, &wins, &n);
			for(i = 0; i < n; i++)
				{
					event.window = wins[i];
					XSendEvent(d->dpy, wins[i], False, ExposureMask, (XEvent *)&event);
				}
			XFree(wins);
			XFlush(d->dpy);
		}
	subWinRender(mode, w);
	if(SUBISTILE(w)) subTileRender(mode, w);
	else if(SUBISCLIENT(w)) subClientRender(mode, w);
}

static void
HandleButtonPress(XButtonEvent *ev)
{
	static Time last_time = 0;

	SubWin *w = subWinFind(ev->window);
	if(w)
		{
			switch(ev->button)
				{
					case Button1:
						if(last_time > 0 && ev->time - last_time <= 300) /* Double click */
							{
								subLogDebug("Double click: win=%#lx\n", ev->window);
								subWinToggle(SUB_WIN_SHADED, w);
								last_time = 0;
							}						
						else /* Single click */
							{
								subLogDebug("Single click: win=%#lx\n", ev->window);
								if(w->prop & SUB_WIN_FLOAT) subWinRaise(w);
								if(ev->subwindow == w->left) 				subWinDrag(SUB_WIN_DRAG_LEFT, w, ev);
								else if(ev->subwindow == w->right)	subWinDrag(SUB_WIN_DRAG_RIGHT, w, ev);
								else if(ev->subwindow == w->bottom) subWinDrag(SUB_WIN_DRAG_BOTTOM, w, ev);
								else if(ev->subwindow == w->icon) 	subWinDrag(SUB_WIN_DRAG_ICON, w, ev);
								else if(ev->subwindow == w->title)
									{
										subWinDrag(SUB_WIN_DRAG_MOVE, w, ev);
										last_time = ev->time;
									}
								else if(SUBISTILE(w))
									{
										if(ev->subwindow == w->tile->btnew) subTileAdd(w, subTileNewVert());
										else if(ev->subwindow == w->tile->btdel) 
											{
												if(SUBISSCREEN(w)) subScreenDelete(w); 
												else subTileDelete(w);
											}
									}
							}
						break;
					case Button2:
						if(SUBISSCREEN(w)) subScreenDelete(w); 
						else if(SUBISTILE(w)) subTileDelete(w);
						else subClientSendDelete(w);
						break;
					case Button3: 
						if(SUBISTILE(w) && ev->subwindow == w->tile->btnew) subTileAdd(w, subTileNewHoriz());
						else subWinToggle(SUB_WIN_FLOAT, w); 
						break;
					case Button4: subScreenSwitch(1);		break;
					case Button5: subScreenSwitch(-1);	break;
				}
		}
}

static void
HandleKeyPress(XKeyEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(w)
		{
			KeySym keysym;
			XComposeStatus compose;
			char buf[10];

			XLookupString(ev, buf, sizeof(buf), &keysym, &compose);
			if(!strcmp(buf, "add_vtile")) 
				{
					subTileAdd(w, subTileNewVert());
				}
			else if(!strcmp(buf, "add_htile"))
				{
					subTileAdd(w, subTileNewHoriz());
				}
			else if(!strcmp(buf, "del_tile") && SUBISTILE(w))
				{
					subTileDelete(w);
				}
			else subLogDebug("KeyPress: state=%d, keycode=%d\n", ev->state, ev->keycode);
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

			wc.x						= w->x;
			wc.y						= w->y;
			wc.width				= w->width;
			wc.height				= w->height;
			wc.sibling			= ev->above;
			wc.stack_mode		= ev->detail;
			wc.border_width	= d->bw;

			XConfigureWindow(d->dpy, w->frame, ev->value_mask, &wc);
			subClientSendConfigure(w);
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
			w = subClientNew(ev->window);
			subTileAdd(screen->active, w);
		}
}

static void
HandleDestroy(XDestroyWindowEvent *ev)
{
	SubWin *w = subWinFind(ev->event);
	if(SUBISCLIENT(w)) subClientDelete(w); 

}
	
static void
HandleMessage(XClientMessageEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(w && ev->message_type == subEwmhGetAtom(SUB_EWMH_WM_CHANGE_STATE) 
		&& ev->format == 32 && ev->data.l[0] == IconicState)
		subWinUnmap(w);
}

static void
HandleColormap(XColormapEvent *ev)
{	
	SubWin *w = subWinFind(ev->window);
	if(SUBISCLIENT(w) && ev->new)
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
	if(SUBISCLIENT(w))
		{
			subLogDebug("Property: atom=%ld\n", ev->atom);
			if(ev->atom == XA_WM_NAME || ev->atom == subEwmhGetAtom(SUB_EWMH_NET_WM_NAME)) subClientFetchName(w);
		}
}

static void
HandleCrossing(XCrossingEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(w)
		{
			if(d->focus) 
				{
					SubWin *w1 = subWinFind(d->focus);
					d->focus = 0;
					if(w1) RenderWindow(w1);
				}

			/* Make leave events to enter event of the parent window */
			if(ev->type == LeaveNotify && !ev->mode && w->parent) w = w->parent;
			d->focus = w->frame;
			RenderWindow(w);
			subEwmhSetWindow(DefaultRootWindow(d->dpy), SUB_EWMH_NET_ACTIVE_WINDOW, w->frame);

			/* Focus */
			if(SUBISCLIENT(w) && !(w->prop & SUB_WIN_SHADED))
				{
					if(w->prop & SUB_WIN_INPUT) XSetInputFocus(d->dpy, w->win, RevertToNone, CurrentTime);
					if(w->prop & SUB_WIN_SEND_FOCUS)
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
						w->prop & SUB_WIN_INPUT ? 1 : 0, w->prop & SUB_WIN_SEND_FOCUS ? 1 : 0);
				}
			else if(SUBISTILE(w) && !(w->prop & SUB_WIN_SHADED)) 
				XSetInputFocus(d->dpy, w->win, RevertToNone, CurrentTime);
		}
}

static void
HandleExpose(XEvent *ev)
{
	XEvent event;
	Window win = ev->type == Expose ? ev->xexpose.window : ev->xvisibility.window;
	SubWin *w = subWinFind(win);

	while(XCheckTypedWindowEvent(d->dpy, win, ev->type, &event));

	if(w) RenderWindow(w);
	else
		{
			SubSublet *s = subSubletFind(win);
			if(s) subSubletRender(s);
		}
}

 /**
	* Handle the X events.
	* @return Return zero on failure.
	**/

int subEventLoop(void)
{
	int cur;
	XEvent ev;
	struct timeval tv;
	SubSublet *s = NULL;

	while(1)
		{
			s = subSubletGetRecent();
			if(s)
				{
					fd_set fdset;
					cur	= subEventGetTime();

					while(s->time <= cur)
						{
							s->time = cur + s->interval;

							subLuaCall(s->ref, &s->data);
							subSubletRender(s);
							subSubletSift(1);

							s = subSubletGetRecent();
						}

					tv.tv_sec		= s->interval;
					tv.tv_usec	= 0;
					FD_ZERO(&fdset);
					FD_SET(ConnectionNumber(d->dpy), &fdset);

					if(select(ConnectionNumber(d->dpy) + 1, &fdset, NULL, NULL, &tv) == -1)
						subLogDebug("Can't select the connection\n");
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
