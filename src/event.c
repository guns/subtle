
 /**
	* @package subtle
	*
	* @file Event functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel
	* @version $Id$
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

#ifdef HAVE_SYS_INOTIFY_H
#define BUFLEN (sizeof(struct inotify_event))
#endif

/* Exec {{{ */
static void
Exec(char *cmd)
{
	pid_t pid = fork();

	switch(pid)
		{
			case 0:
				setsid();
				execlp("/bin/sh", "sh", "-c", cmd, NULL);

				/* Never to be reached */
				subUtilLogWarn("Can't exec command `%s'.\n", cmd);
				exit(1);
			case -1: subUtilLogWarn("Failed to fork.\n");
		}
} /* }}} */

/* GetParent {{{ */
static Window
GetParent(Window win)
{
	unsigned int nwins;
	Window parent, unused, *wins = NULL;

	/* Some events don't propagate but we need them to do so */
	XQueryTree(d->disp, win, &unused, &parent, &wins, &nwins);
	XFree(wins);

	return(parent);
} /* }}} */

/* HandleButtonPress {{{ */
static void
HandleButtonPress(XButtonEvent *ev)
{
	SubClient *c = NULL;
	static Time last_time = 0;

	if(ev->window == d->bar.views)
		{
			SubView *v = (SubView *)subUtilFind(ev->subwindow, 1);
			if(v) subViewJump(v);

			return;
		}

	c = (SubClient *)subUtilFind(ev->window, 1);
	if(c)
		{
			switch(ev->button)
				{
					case Button1:
						if(last_time > 0 && ev->time - last_time <= 300) /* Double click */
							{
								subUtilLogDebug("click=double, win=%#lx\n", ev->window);
								if((ev->subwindow == c->title && c->tile && c->tile->flags & SUB_TYPE_TILE) || c->flags & SUB_STATE_FLOAT) 
									subClientToggle(c, SUB_STATE_SHADE);
								last_time = 0;
							}						
						else  /* Single click */
							{
								subUtilLogDebug("click=single, win=%#lx\n", ev->window);
								if(c->flags & SUB_STATE_FLOAT) XRaiseWindow(d->disp, c->frame);
								if(c->tile && c->tile->flags & SUB_STATE_STACK) 
									{
										c->tile->top = c;
										if(c->flags & SUB_STATE_SHADE) c->flags &= ~SUB_STATE_SHADE;
										subTileConfigure(c->tile);
									}
								if(ev->subwindow == c->left) 				subClientDrag(c, SUB_DRAG_LEFT);
								else if(ev->subwindow == c->right)	subClientDrag(c, SUB_DRAG_RIGHT);
								else if(ev->subwindow == c->bottom) subClientDrag(c, SUB_DRAG_BOTTOM);
								else if(ev->subwindow == c->title || (c->flags & SUB_TYPE_CLIENT && ev->subwindow == c->caption))
									{ 
										/* Either drag and move or drag an swap windows */
										subClientDrag(c, (c->flags & SUB_STATE_FLOAT) ? SUB_DRAG_MOVE : SUB_DRAG_SWAP);
										last_time = ev->time;
									}
							}
						break;
					case Button2:
						if(ev->subwindow == c->title) subClientKill(c);
						break;
					case Button3: 
						if(ev->subwindow == c->title) subClientToggle(c, SUB_STATE_FLOAT);
						break;
/*					case Button4: 
					case Button5: 
						
						subViewJump(d->cv->next); break;
						if(d->cv->prev) subViewJump(d->cv->prev); break; */
				}
		}
} /* }}} */

/* HandleKeyPress  {{{ */
static void
HandleKeyPress(XKeyEvent *ev)
{
	SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
	if(c)
		{
			SubKey *k = subKeyFind(ev->keycode, ev->state);
			if(k) 
				{
					int mode = 0, type = SUB_TYPE_HORZ;

					switch(k->flags)
						{
							case SUB_KEY_DELETE_WIN: subClientKill(c); break;
							case SUB_KEY_TOGGLE_SHADE:	if(!mode) mode = SUB_STATE_SHADE;
							case SUB_KEY_TOGGLE_RAISE:			if(!mode) mode = SUB_STATE_FLOAT;
							case SUB_KEY_TOGGLE_FULL:			if(!mode) mode = SUB_STATE_FULL;
								subClientToggle(c, mode);
								break;									
							case SUB_KEY_TOGGLE_LAYOUT: 
								if(c->flags & SUB_TYPE_TILE)
									{
										type = (c->flags & SUB_TYPE_HORZ) ? SUB_TYPE_HORZ : SUB_TYPE_VERT;
										c->flags &= ~type;
										c->flags |= (type == SUB_TYPE_HORZ) ? SUB_TYPE_VERT : SUB_TYPE_HORZ;
										subTileConfigure(c->tile);
									}			
								break;
/*							case SUB_KEY_DESKTOP_NEXT: subViewJump(d->cv->next);	break;
							case SUB_KEY_DESKTOP_PREV: 
								if(d->cv->prev) subViewJump(d->cv->prev);		
								break;*/
							case SUB_KEY_EXEC:	if(k->string) Exec(k->string); break;
						}

					subUtilLogDebug("KeyPress: code=%d, mod=%d\n", k->code, k->mod);
				}
		}
} /* }}} */

/* HandleConfigure {{{ */
static void
HandleConfigure(XConfigureRequestEvent *ev)
{
	XWindowChanges wc;
	SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
	if(c)
		{
			if(ev->value_mask & CWX)			c->x			= ev->x;
			if(ev->value_mask & CWY)			c->y 			= ev->y;
			if(ev->value_mask & CWWidth)	c->width	= ev->width;
			if(ev->value_mask & CWHeight)	c->height = ev->height;

			subClientConfigure(c);
		}
	else
		{
			wc.x					= ev->x;
			wc.y					= ev->y;
			wc.width			= ev->width;
			wc.height			= ev->height;
			wc.sibling		= ev->above;
			wc.stack_mode	= ev->detail;

			XConfigureWindow(d->disp, ev->window, ev->value_mask, &wc);
		}
} /* }}} */

/* HandleMapNotify {{{ */
static void
HandleMapNotify(XMappingEvent *ev)
{
} /* }}} */

/* HandleMapRequest {{{ */
static void
HandleMapRequest(XMapRequestEvent *ev)
{
	SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
	if(!c) subViewMerge(ev->window);
} /* }}} */

/* HandleDestroy {{{ */
static void
HandleDestroy(XDestroyWindowEvent *ev)
{
	SubClient *c = (SubClient *)subUtilFind(ev->event, 1);
	if(c) 
		{
			c->flags |= SUB_STATE_DEAD;
			subClientKill(c); 
			subTileConfigure(d->cv->tile);
		}
} /* }}} */

/* HandleMessage {{{ */
static void
HandleMessage(XClientMessageEvent *ev)
{
	SubClient *c = NULL;

	subUtilLogDebug("ClientMessage: type=%ld\n", ev->message_type);

	if(ev->window == DefaultRootWindow(d->disp))
		{
			if(ev->message_type == subEwmhFind(SUB_EWMH_NET_CURRENT_DESKTOP))
				{
					/* Bound checking */
					if(ev->data.l[0] > 0 && ev->data.l[0] < d->views->ndata)
						subViewJump((SubView *)d->views->data[ev->data.l[0]]);
				}
			else if(ev->message_type == subEwmhFind(SUB_EWMH_NET_ACTIVE_WINDOW))
				{
					SubClient *focus = (SubClient *)subUtilFind(GetParent(ev->data.l[0]), 1);
					if(focus) subClientFocus(focus);
				}
			return;
		}
	c = (SubClient *)subUtilFind(GetParent(ev->window), 1);
	if(c && ev->format == 32)
		{
			subUtilLogDebug("ClientMessage: [0]=%ld, [1]=%ld, [2]=%ld, [3]=%ld, [4]=%ld\n", 
				ev->data.l[0], ev->data.l[1], ev->data.l[2], ev->data.l[3], ev->data.l[4]);

			if(ev->message_type == subEwmhFind(SUB_EWMH_NET_WM_STATE))
				{
					/* [0] => Remove = 0 / Add = 1 / Toggle = 2 -> we _always_ toggle */
					if(ev->data.l[1] == (long)subEwmhFind(SUB_EWMH_NET_WM_STATE_FULLSCREEN))
						{
							subClientToggle(c, SUB_STATE_FULL);
						}
					else if(ev->data.l[1] == (long)subEwmhFind(SUB_EWMH_NET_WM_STATE_SHADED))
						{
							subClientToggle(c, SUB_STATE_SHADE);
						}
				}
		}
} /* }}} */

/* HandleColormap {{{ */
static void
HandleColormap(XColormapEvent *ev)
{	
	SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
	if(c && ev->new)
		{
			c->cmap = ev->colormap;
			XInstallColormap(d->disp, c->cmap);
		}
} /* }}} */

/* HandleProperty {{{ */
static void
HandleProperty(XPropertyEvent *ev)
{
	/* Prevent expensive query if the atom isn't supported */
	if(ev->atom == XA_WM_NAME || ev->atom == subEwmhFind(SUB_EWMH_WM_NAME))
		{
			SubClient *c = (SubClient *)subUtilFind(GetParent(ev->window), 1);
			if(c) subClientFetchName(c);
		}
} /* }}} */

/* HandleCrossing {{{ */
static void
HandleCrossing(XCrossingEvent *ev)
{
	SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
	if(c && !(c->flags & SUB_STATE_DEAD))
		{
			XEvent event;
		
			if(d->focus != c) subClientFocus(c);

			/* Remove any other event of the same type and window */
			while(XCheckTypedWindowEvent(d->disp, ev->window, ev->type, &event));			
		}
} /* }}} */

/* HandleExpose {{{ */
static void
HandleExpose(XEvent *ev)
{
	XEvent event;

	if(ev->xany.window == d->bar.win)
		{
			subSubletConfigure();
			subSubletRender();
			subViewRender();
		}
	else
		{
  		SubClient *c = (SubClient *)subUtilFind(ev->xany.window, 1);
			if(c) subClientRender(c);
		}

	/* Remove any other event of the same type and window */
	while(XCheckTypedWindowEvent(d->disp, ev->xany.window, ev->type, &event));
} /* }}} */

/* HandleFocus {{{ */
static void
HandleFocus(XFocusInEvent *ev)
{
	SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
	if(c && c->flags & SUB_PREF_FOCUS)
		{
			if(c != d->focus) 
				{
					/* Remove focus from window */
					if(d->focus) 
					{
						SubClient *f = d->focus;
						d->focus = NULL;
						if(f && f->flags & SUB_TYPE_CLIENT) subClientRender(f);

						subKeyUngrab(f->frame);
					}

					d->focus = c;
					subClientRender(c);
					subEwmhSetWindows(DefaultRootWindow(d->disp), SUB_EWMH_NET_ACTIVE_WINDOW, &c->frame, 1);

					subKeyGrab(c->frame);	
				}
		}
	else if(ev->window == d->cv->frame) subKeyGrab(ev->window);

} /* }}} */

 /** subEventLoop {{{ 
	* @brief Handle all X events 
	**/

void
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
					current	= subUtilTime();

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
							case MapNotify:					HandleMapNotify(&ev.xmapping);					break;
							case MapRequest: 				HandleMapRequest(&ev.xmaprequest); 			break;
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
} /* }}} */
