
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

static int size = 0;
static SubSublet **sublets = NULL;
static SubSublet *first = NULL;

 /**
	* Find a window via Xlib context manager 
	* @param win A watch descriptor
	* @return Return the #SubSublet associated with the wd or NULL
	**/

SubSublet *
subSubletFind(int wd)
{
	SubSublet *s = NULL;
	return(XFindContext(d->dpy, d->bar.sublets, wd, (void *)&s) != XCNOENT ? s : NULL);
}

 /**
	* Sift the sublet through the queue
	* @param pos Position of the sublet in the queue
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
}

 /**
	* Get next sublet 
	* @return Return either the next #SubSublet or NULL if no sublets are loaded
	**/

SubSublet *
subSubletNext(void)
{
	return(size > 0 ? sublets[1] : NULL);
}

 /**
	* Create a new sublet 
	* @param type Type of the sublet
	* @param ref Lua object reference
	* @param interval Update interval
	* @param watch Watch file
	**/

void
subSubletNew(int type,
	int ref,
	time_t interval,
	char *watch)
{
	int i;
	SubSublet *s = (SubSublet *)subUtilAlloc(1, sizeof(SubSublet));

	if(!sublets) 
		{
			first		= s;
			sublets = (SubSublet **)subUtilAlloc(1, sizeof(SubSublet *));
		}

	/* Init the sublet */
	s->flags		= type;
	s->ref			= ref;
	s->interval	= interval;
	s->time			= subEventGetTime();

#ifdef HAVE_SYS_INOTIFY_H
	if(s->flags & SUB_SUBLET_TYPE_WATCH && ((s->interval = inotify_add_watch(d->notify, watch, IN_MODIFY)) < 0))
		{
			subUtilLogWarn("Can't create inotify watch\n");
			subUtilLogDebug("Inotify: %s\n", strerror(errno));

			return;
		}
	else XSaveContext(d->dpy, d->bar.sublets, s->interval, (void *)s);
#endif /* HAVE_SYS_INOTIFY_H */

	subLuaCall(s);

  /* Don't add text and watch type sublets to the queue */
  if(type != SUB_SUBLET_TYPE_TEXT && type != SUB_SUBLET_TYPE_WATCH)
    {
			sublets = (SubSublet **)realloc(sublets, sizeof(SubSublet *) * (size + 2));
			if(!sublets) subUtilLogError("Can't alloc memory. Exhausted?\n");

			i = ++size;

			while(i > 1 && s->time < sublets[i / 2]->time)
				{
					sublets[i] = sublets[i / 2];
					i /= 2;
				}
			sublets[i] = s;
		}
	
	/* Smart use of unused array index */
	if(first != s) sublets[0]->next = s;
	sublets[0] = s;
}

 /**
	* Delete a sublet 
	* @param w A #Sublet
	**/

void
subSubletDelete(SubSublet *s)
{
}

 /**
	* Configure sublet bar
	**/

void
subSubletConfigure(void)
{
	if(sublets)
		{
			int width = 3;
			SubSublet *s = first;

			while(s)
				{
					if(s->flags & SUB_SUBLET_TYPE_METER) width += 66;
					else width += strlen(s->string) * d->fx + 12;
					s = s->next;
					printf("width=%d\n", width);
				}
			XMoveResizeWindow(d->dpy, d->bar.sublets, DisplayWidth(d->dpy, DefaultScreen(d->dpy)) - width, 0, width, d->th);
		}
}

 /**
	* Render the sublet s
	**/

void
subSubletRender(void)
{
	if(sublets)
		{
			int i, width = 3;
			SubSublet *s = first;

			XClearWindow(d->dpy, d->bar.sublets);

			while(s)
				{
					if(s->flags & SUB_SUBLET_TYPE_METER && s->number)
						{
							XDrawRectangle(d->dpy, d->bar.sublets, d->gcs.font, width, 2, 60, d->th - 5);
							XFillRectangle(d->dpy, d->bar.sublets, d->gcs.font, width + 2, 4, (56 * s->number) / 100, d->th - 8);
							width += 63;
						}
					else if(s->string) 
						{
							XDrawString(d->dpy, d->bar.sublets, d->gcs.font, width, d->fy - 1, s->string, strlen(s->string));
							width += strlen(s->string) * d->fx + 6;
						}
					s = s->next;
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
	SubSublet *s = first;

	while(s)
		{
			free(s);
			s = s->next;
		}
	free(sublets);
}
