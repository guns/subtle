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
	subLogDebug("Redraw: win=%#lx, mode=%d\n", w->win, mode);
	if(w->prop & SUB_WIN_SCREEN) 
		{
			XExposeEvent event;
			unsigned int n = 0, i;
			Window nil, *wins = NULL;

			XSetWindowBackground(d->dpy, screen->statusbar, mode ? d->colors.norm : d->colors.act);
			XClearWindow(d->dpy, screen->statusbar);

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
	if(w->prop & SUB_WIN_CLIENT) subClientRender(mode, w);
	else if(w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV)) subTileRender(mode, w);
}

static void
RenderSublet(SubSublet *s)
{
	if(s && s->data)
		{
			XClearWindow(d->dpy, s->win);
			XDrawString(d->dpy, s->win, d->gcs.font, 3, d->fy - 1, s->data, strlen(s->data));
			XFlush(d->dpy);
		}
}

static void
HandleButtonPress(XButtonEvent *ev)
{
	static Time last_time = 0;

	SubWin *w = subWinFind(ev->window);
	if(w)
		{
			if(ev->button == 4) subScreenAdd();

			if(ev->subwindow == w->icon)
				{
					subWinDrag(1, w);
					return;
				}
			else if(ev->subwindow == w->title) 
				{
					/* Check double clicks */
					if(last_time > 0 && ev->time - last_time <= 300)
						{
							subLogDebug("Double click on %#lx\n", ev->window);
							subWinShade(w);
							last_time = 0;
						}						
					else
						{
							subLogDebug("Single click on %#lx\n", ev->window);
							switch(ev->button)
								{
									case 1: subWinDrag(0, w);	break;
									case 2: 
										if(w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV)) 
											{
												/* Check for screens */
												if(w->prop & SUB_WIN_SCREEN) subScreenDelete(w);
												else subTileDelete(w);
											}
										else subClientDelete(w);
										return;
								}
							last_time = ev->time;
						}
				}
			if(w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV))
				{
					if(ev->subwindow == w->tile->newcol) 
						switch(ev->button)
							{
								case 1: subTileAdd(w, subTileNewVert());	break; 
								case 3: subTileAdd(w, subTileNewHoriz());	break;
							}
					else if(ev->subwindow == w->tile->delcol) 
						{
							/* Check for screens */
							if(w->prop & SUB_WIN_SCREEN) subScreenDelete(w);
							else subTileDelete(w);
						}
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
			int len = 10;

			XLookupString(ev, buf, len, &keysym, &compose);
			if(!strcmp(buf, "add_vtile")) 
				{
					subTileAdd(w, subTileNewVert());
				}
			else if(!strcmp(buf, "add_htile"))
				{
					subTileAdd(w, subTileNewHoriz());
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
			if(ev->value_mask & CWX)			w->x = ev->x;
			if(ev->value_mask & CWY)			w->y = ev->y;
			if(ev->value_mask & CWWidth)	w->width = ev->width;
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

	wc.width				= ev->width;
	wc.height				= ev->height;
	wc.sibling			= ev->above;
	wc.stack_mode		= ev->detail;
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
	/*if(!(w->prop & SUB_WIN_SHADE))
		{
			XMapWindow(d->dpy, w->win);
			XMapRaised(d->dpy, w->frame);
			subClientSetWMState(w, NormalState);
		}*/
}

static void
HandleUnmap(XUnmapEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(w && w->prop & SUB_WIN_CLIENT) subTileConfigure(w->parent);
}

static void
HandleDestroy(XDestroyWindowEvent *ev)
{
	SubWin *w = subWinFind(ev->event);
	if(w && w->prop & SUB_WIN_CLIENT) subClientDelete(w); 
}
	
static void
HandleMessage(XClientMessageEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(w && ev->message_type == d->atoms.change && ev->format == 32 && ev->data.l[0] == IconicState)
		subWinUnmap(w);
}

static void
HandleColormap(XColormapEvent *ev)
{	
	SubWin *w = subWinFind(ev->window);
	if(w && w->prop & SUB_WIN_CLIENT && ev->new)
		{
			w->client->cmap = ev->colormap;
			XInstallColormap(d->dpy, w->client->cmap);
		}
}

static void
HandleProperty(XPropertyEvent *ev)
{
	SubWin *w = subWinFind(ev->window);
	if(w && (w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV)))
		switch(ev->atom)
			{
				case XA_WM_NAME:
					if(w->client->name) XFree(w->client->name);
					XFetchName(d->dpy, w->win, &w->client->name);
					RenderWindow(w);
					break;
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
					subLogDebug("Enter: focus: old=%#lx, new=%#lx\n", d->focus, w->frame);
					d->focus = 0;
					if(w1) RenderWindow(w1);
				}

			/* Make leave event to enter event of the parent window */
			if(ev->type == LeaveNotify && !ev->mode && w->parent) w = w->parent;
			d->focus = w->frame;
			RenderWindow(w);
			XSetInputFocus(d->dpy, w->win, RevertToNone, CurrentTime);
			printf("%%EVENT%: type=%d, state=%d, mode=%d\n", ev->type, ev->state, ev->mode);
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
			if(s) RenderSublet(s);
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
			fd_set fdset;

			s		= subSubletGetRecent();
			cur	= subEventGetTime();

			while(s->time <= cur)
				{
					s->time = cur + s->interval;

					subLuaCall(s->function, &s->data);
					RenderSublet(s);
					subSubletSift(1);

					s = subSubletGetRecent();
				}

			tv.tv_sec		= s->interval;
			tv.tv_usec	= 0;
			FD_ZERO(&fdset);
			FD_SET(ConnectionNumber(d->dpy), &fdset);

			if(select(ConnectionNumber(d->dpy) + 1, &fdset, NULL, NULL, &tv) == -1)
				subLogError("Can't select the connection\n");

			while(XPending(d->dpy))
				{
					XNextEvent(d->dpy, &ev);
					switch(ev.type)
						{
							case ButtonPress:				HandleButtonPress(&ev.xbutton);					break;
							case KeyPress:					HandleKeyPress(&ev.xkey);								break;
							case ConfigureRequest:	HandleConfigure(&ev.xconfigurerequest);	break;
							case MapRequest: 				HandleMap(&ev.xmaprequest); 						break;
							case UnmapNotify: 			HandleUnmap(&ev.xunmap); 								break;
							case DestroyNotify: 		HandleDestroy(&ev.xdestroywindow);			break;
							case ClientMessage: 		HandleMessage(&ev.xclient); 						break;
							case ColormapNotify: 		HandleColormap(&ev.xcolormap); 					break;
							case PropertyNotify: 		HandleProperty(&ev.xproperty); 					break;
																			/*case EnterNotify: 			HandleEnter(&ev.xcrossing);				 			break;*/
																			/*case LeaveNotify:				HandleLeave(&ev.xcrossing);							break;*/
							case EnterNotify:
							case LeaveNotify:				HandleCrossing(&ev.xcrossing);					break;
							case VisibilityNotify:	
							case Expose:						HandleExpose(&ev);											break;
						}
				}
		}
}
