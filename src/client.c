#include "subtle.h"

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
	Atom *protos = NULL;
	SubWin *w = subWinNew(win);
	w->client = (SubClient *)calloc(1, sizeof(SubClient));
	if(!w->client) subLogError("Can't alloc memory. Exhausted?\n");

	XGrabServer(d->dpy);
	XAddToSaveSet(d->dpy, w->win);
	XGetWindowAttributes(d->dpy, w->win, &attr);
	w->client->cmap	= attr.colormap;
	w->flags				= SUB_WIN_TYPE_CLIENT; 

	/* Window caption */
	XFetchName(d->dpy, win, &w->client->name);
	if(!w->client->name) w->client->name = strdup("subtle");
	w->client->caption = XCreateSimpleWindow(d->dpy, w->frame, d->th, 0, 
		(strlen(w->client->name) + 1) * d->fx, d->th, 0, d->colors.border, d->colors.norm);

	/* Hints */
	hints = XGetWMHints(d->dpy, win);
	if(hints)
		{
			if(hints->flags & StateHint) subClientSetWMState(w, hints->initial_state);
			else subClientSetWMState(w, NormalState);
			if(hints->initial_state == IconicState) subLogDebug("Iconic: win=%#lx\n", win);			
			if(hints->input) w->flags |= SUB_WIN_PREF_INPUT;
			if(hints->flags & (XUrgencyHint|WindowGroupHint)) 
				{
					subWinToggle(SUB_WIN_OPT_RAISE, w);
					printf("Transient: flags=%s%s\n", hints->flags & XUrgencyHint ? "u" : "", 
						hints->flags & WindowGroupHint ? "g" : "");
				}
			XFree(hints);
		}
	
	/* Protocols */
	if(XGetWMProtocols(d->dpy, w->win, &protos, &n))
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
	if(unnused) subWinToggle(SUB_WIN_OPT_RAISE, w);

	printf("Transient: window=%d\n", unnused);

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
	if(w->flags & SUB_WIN_TYPE_CLIENT)
		{
			XEvent event;
			XGrabServer(d->dpy);

			subWinUnmap(w);

			if(w->client->name) XFree(w->client->name);
			free(w->client);
			subWinDelete(w);

			XSync(d->dpy, False);
			XUngrabServer(d->dpy);
			while(XCheckTypedEvent(d->dpy, EnterWindowMask|LeaveWindowMask, &event));
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

	XChangeProperty(d->dpy, w->win, subEwmhGetAtom(SUB_EWMH_WM_STATE), subEwmhGetAtom(SUB_EWMH_WM_STATE),
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

	if(XGetWindowProperty(d->dpy, w->win, subEwmhGetAtom(SUB_EWMH_WM_STATE), 0L, 2L, False, 
			subEwmhGetAtom(SUB_EWMH_WM_STATE), &type, &format, &bytes, &nil,
			(unsigned char **) &data) == Success && bytes)
		{
			state = *data;
			XFree(data);
		}
	return(state);
}

 /**
	* Send a configure request to the client
	* @param w A #SubWin
	**/

void
subClientSendConfigure(SubWin *w)
{
	XConfigureEvent ev;

	ev.type								= ConfigureNotify;
	ev.event							= w->win;
	ev.window							= w->win;
	ev.x									= w->x;
	ev.y									= w->y;
	ev.width							= SUBWINWIDTH(w);
	ev.height							= SUBWINHEIGHT(w);
	ev.above							= None;
	ev.border_width 			= 0;
	ev.override_redirect	= 0;

	XSendEvent(d->dpy, w->win, False, StructureNotifyMask, (XEvent *)&ev);
}

 /**
	* Send delete event to client if supported, otherwise just kill the window
	* @param w A #SubWin
	**/

void
subClientSendDelete(SubWin *w)
{
	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{

			subWinUnmap(w);
			if(w->flags & SUB_WIN_PREF_CLOSE)
				{
					XEvent ev;

					ev.type									= ClientMessage;
					ev.xclient.window				= w->win;
					ev.xclient.message_type = subEwmhGetAtom(SUB_EWMH_WM_PROTOCOLS);
					ev.xclient.format				= 32;
					ev.xclient.data.l[0]		= subEwmhGetAtom(SUB_EWMH_WM_DELETE_WINDOW);
					ev.xclient.data.l[1]		= CurrentTime;

					XSendEvent(d->dpy, w->win, False, NoEventMask, &ev);
				}
			else XKillClient(d->dpy, w->win);
		}
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
			XFetchName(d->dpy, w->win, &w->client->name);

			/* Check max length of the caption */
			width = (strlen(w->client->name) + 1) * d->fx;
			if(width > w->width - d->th - 4) width = w->width - d->th - 14;
			XMoveResizeWindow(d->dpy, w->client->caption, d->th, 0, width, d->th);

			mode = (d->focus && d->focus->frame == w->frame) ? 0 : 1;
			subWinRender(mode, w);
			subClientRender(mode, w);
		}
}

 /**
	* Render the client window
	* @param w A #SubWin
	**/

void
subClientRender(short mode,
	SubWin *w)
{
	unsigned long col = mode ? (w->flags & SUB_WIN_OPT_COLLAPSE ?  d->colors.cover : d->colors.norm) : 
		d->colors.focus;

	if(w && w->flags & SUB_WIN_TYPE_CLIENT)
		{
			XSetWindowBackground(d->dpy, w->client->caption, col);
			XClearWindow(d->dpy, w->client->caption);
			XDrawString(d->dpy, w->client->caption, d->gcs.font, 3, d->fy - 1, w->client->name, 
				strlen(w->client->name));
		}
}
