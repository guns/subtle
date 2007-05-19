#include "subtle.h"

static int size = 0;
static SubSublet **sublets = NULL;

 /**
	* Find a window via Xlib context manager {{{
	* @param win A #Window
	* @return Return the #SubSublet associated with the window or NULL
	**/

SubSublet *
subSubletFind(Window win)
{
	SubSublet *s = NULL;
	return(XFindContext(d->dpy, win, 2, (void *)&s) != XCNOENT ? s : NULL);
} /* }}} */

 /**
	* Sift the sublet through the sublets {{{
	* @param pos Position of the sublet in the sublets
	**/

void
subSubletSift(int pos)
{
	int left	= 2 * pos;
	int right	= left + 1;
	int max 	= (left <= size && sublets[left]->time < sublets[pos]->time) ? left : pos;

	if(right <= size && sublets[right]->time < sublets[max]->time) max = right;
	if(max != pos)
		{
			SubSublet *tmp	= sublets[pos];
			sublets[pos]		= sublets[max];
			sublets[max]		= tmp;
			subSubletSift(max);
		}
} /* }}} */

 /**
	* Get the next sublet {{{
	* @return Return either the next #SubSublet or NULL if the sublets is empty
	**/

SubSublet *
subSubletNext(void)
{
	return(size > 0 ? sublets[1] : NULL);
} /* }}} */

 /**
	* Init sublet sublets {{{
	**/

void
subSubletInit(void)
{
	sublets = (SubSublet **)calloc(1, sizeof(SubSublet *));
	if(!sublets) subLogError("Can't alloc memory. Exhausted?\n");
} /* }}} */

 /**
	* Create a new sublet {{{
	* @param type Type of the sublet
	* @param ref Lua object reference
	* @param interval Update interval
	* @param width Width of the sublet
	**/

void
subSubletNew(int type,
	int ref,
	time_t interval,
	unsigned int width)
{
	int i;

	SubSublet *s = (SubSublet *)calloc(1, sizeof(SubSublet));
	if(!s) subLogError("Can't alloc memory. Exhausted?\n");

	/* Init the sublet */
	s->flags		= type;
	s->ref			= ref;
	s->width		= width * d->fx;
	s->interval	= interval;
	s->time			= subEventGetTime();
	s->win			= XCreateSimpleWindow(d->dpy, d->bar.sublets, 0, 0, 1, d->th, 0, 0, d->colors.norm);

	subLuaCall(s);
	XSaveContext(d->dpy, s->win, 2, (void *)s);

	/* Don't add text sublets to the sublet list */
	if(type != SUB_SUBLET_TYPE_TEXT)
		{
			sublets = (SubSublet **)realloc(sublets, sizeof(SubSublet *) * (size + 2));
			if(!sublets) subLogError("Can't alloc memory. Exhausted?\n");
	
			i = ++size;

			while(i > 1 && s->time < sublets[i / 2]->time)
				{
					sublets[i] = sublets[i / 2];
					i /= 2;
				}
			sublets[i] = s;
		}
	XMapRaised(d->dpy, s->win);
} /* }}} */

 /**
	* Delete a sublet {{{
	* @param w A #Sublet
	**/

void
subSubletDelete(SubSublet *s)
{
}	/* }}} */

 /**
	* Render a sublet {{{
	* @param s A #SubSublet
	**/

void
subSubletRender(short mode,
	SubSublet *s)
{
	if(s)
		{
			unsigned long col = mode ? d->colors.norm : d->colors.focus;

			XSetWindowBackground(d->dpy, s->win, col);
			XClearWindow(d->dpy, s->win);

			if(s->flags & (SUB_SUBLET_TYPE_TEXT|SUB_SUBLET_TYPE_TEASER) && s->string)
				{
					XDrawString(d->dpy, s->win, d->gcs.font, 3, d->fy - 1, s->string, strlen(s->string));
				}
			else if(s->flags & SUB_SUBLET_TYPE_METER && s->number)
				{
					XDrawRectangle(d->dpy, s->win, d->gcs.font, 2, 2, s->width - 4, d->th - 5);
					XFillRectangle(d->dpy, s->win, d->gcs.font, 4, 4, ((s->width - 7) * s->number) / 100, d->th - 8);
				}
			XFlush(d->dpy);
		}
} /* }}} */

 /**
	* Kill all sublets {{{
	**/

void
subSubletKill(void)
{
	int i;

	for(i = 1; i < size; i++)
		{
			XUnmapWindow(d->dpy, sublets[i]->win);
			XDeleteContext(d->dpy, sublets[i]->win, 2);
			XDestroyWindow(d->dpy, sublets[i]->win);
			free(sublets[i]);
		}
	free(sublets);
} /* }}} */
