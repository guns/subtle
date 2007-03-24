#include "subtle.h"

static SubSubletList *list = NULL;

 /**
	* Find a sublet via the Xlib content manager.
	* @param win A window
	* @return Return the #SubSublet associated with the window or NULL
	**/

SubSublet *
subSubletFind(Window win)
{
	SubSublet *s = NULL;
	return(XFindContext(d->dpy, win, 3, (void *)&s) != XCNOENT ? s : NULL);
}

 /**
	* Sift the sublet through the list
	* @param pos Position of the sublet in the list
	**/

void
subSubletSift(int pos)
{
	int l		= 2 * pos;
	int r		= l + 1;
	int max = (l <= list->size && list->content[l]->time < list->content[pos]->time) ? l : pos;

	if(r <= list->size && list->content[r]->time < list->content[max]->time) max = r;
	if(max != pos)
		{
			SubSublet *tmp			= list->content[pos];
			list->content[pos]	= list->content[max];
			list->content[max]	= tmp;
			subSubletSift(max);
		}
}

 /**
	* Get the recent sublet
	* @return Return either the recent sublet or NULL if the list is empty
	**/

SubSublet *
subSubletGetRecent(void)
{
	return(list->size > 0 ? list->content[1] : NULL);
}

 /**
	* Init sublet list
	**/

void
subSubletNew(void)
{
	list = (SubSubletList *)calloc(1, sizeof(SubSubletList));
	if(!list) subLogError("Can't alloc memory. Exhausted?\n");
	list->content = (SubSublet **)calloc(1, sizeof(SubSublet *));
	if(!list->content) subLogError("Can't alloc memory. Exhausted?\n");
}

 /**
	* Add a new sublet
	* @param type Type of the sublet
	* @param ref Lua object reference
	* @param interval Update interval
	* @param width Width of the sublet
	**/

void
subSubletAdd(int type,
	int ref,
	unsigned int interval,
	unsigned int width)
{
	int i, time;
	static total_width = 0;
	long mask = CWOverrideRedirect|CWBackPixmap|CWEventMask;
	XSetWindowAttributes attrs;
	SubSublet *s = (SubSublet *)calloc(1, sizeof(SubSublet));
	if(!s) subLogError("Can't alloc memory. Exhausted?\n");

	attrs.override_redirect	= True;
	attrs.background_pixmap	= ParentRelative;
	attrs.event_mask				= ExposureMask|VisibilityChangeMask;

	/* Init the sublet */
	total_width += width * d->fx;
	XMoveResizeWindow(d->dpy, screen->statusbar,
		DisplayWidth(d->dpy, DefaultScreen(d->dpy)) - total_width, 0, total_width, d->th);

	time				= subEventGetTime();
	s->type			= type;
	s->ref			= ref;
	s->interval = interval;
	s->width		= width * d->fx;
	s->time			= time;
	s->win			= SUBWINNEW(screen->statusbar, total_width - width * d->fx, 0, width * d->fx, d->th, 0);

	subLuaCall(s->ref, &s->data);
	XSaveContext(d->dpy, s->win, 3, (void *)s);

	/* Don't add teaser sublets to the list */
	if(type > SUB_SUBLET_TEXT)
		{
			list->content = (SubSublet **)realloc(list->content, sizeof(SubSublet *) * (list->size + 2));
			if(!list->content) subLogError("Can't alloc memory. Exhausted?\n");
	
			i = ++(list->size);

			while(i > 1 && s->time < list->content[i / 2]->time)
				{
					list->content[i] = list->content[i / 2];
					i /= 2;
				}
			list->content[i] = s;
		}

	XMapRaised(d->dpy, s->win);
}

 /**
	* Render a sublet.
	* @param s A #SubSublet
	**/

void
subSubletRender(SubSublet *s)
{
	if(s && (s->data.string || s->data.number))
		{
			XClearWindow(d->dpy, s->win);
			switch(s->type)
				{
					case SUB_SUBLET_TEXT:
					case SUB_SUBLET_TEASER:
						XDrawString(d->dpy, s->win, d->gcs.font, 3, d->fy - 1, s->data.string, strlen(s->data.string));
						break;
					case SUB_SUBLET_METER:
						XDrawRectangle(d->dpy, s->win, d->gcs.font, 2, 2, s->width - 4, d->th - 5);
						XFillRectangle(d->dpy, s->win, d->gcs.font, 4, 4, ((s->width - 7) * s->data.number) / 100, d->th - 8);
				}
			XFlush(d->dpy);
		}
}

 /**
	* Kill all sublets 
	**/

void
subSubletKill(void)
{
	int i;

	for(i = 1; i < list->size; i++)
		{
			XUnmapWindow(d->dpy, list->content[i]->win);
			XDeleteContext(d->dpy, list->content[i]->win, 3);
			XDestroyWindow(d->dpy, list->content[i]->win);
			free(list->content[i]);
		}
	free(list);
}
