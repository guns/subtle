#include "subtle.h"

static SubSubletList *list = NULL;

SubSublet *
subSubletFind(Window win)
{
	SubSublet *s = NULL;
	return(XFindContext(d->dpy, win, 3, (void *)&s) != XCNOENT ? s : NULL);
}

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

SubSublet *
subSubletGetRecent(void)
{
	return(list->size > 0 ? list->content[1] : NULL);
}

void
subSubletNew(void)
{
	list = (SubSubletList *)calloc(1, sizeof(SubSubletList));
	if(!list) subLogError("Can't alloc memory. Exhausted?\n");
	list->content = (SubSublet **)calloc(1, sizeof(SubSublet *));
	if(!list->content) subLogError("Can't alloc memory. Exhausted?\n");
}

void
subSubletAdd(const char *function,
	unsigned int interval,
	unsigned int width)
{
	int i;
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

	s->time			= subEventGetTime();
	s->interval = interval;
	s->function	= function;
	s->win			= SUBWINNEW(screen->statusbar, total_width - width * d->fx, 0, width * d->fx, d->th, 0);
	
	subLuaCall(s->function, &s->data);
	XSaveContext(d->dpy, s->win, 3, (void *)s);

	list->content = (SubSublet **)realloc(list->content, sizeof(SubSublet *) * (list->size + 2));
	if(!list->content) subLogError("Can't alloc memory. Exhausted?\n");
	
	i = ++(list->size);

	while(i > 1 && s->time < list->content[i / 2]->time)
		{
			list->content[i] = list->content[i / 2];
			i /= 2;
		}
	list->content[i] = s;

	XMapRaised(d->dpy, s->win);

	printf("Loaded docklet %s (%d)\n", function, interval);
}

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
