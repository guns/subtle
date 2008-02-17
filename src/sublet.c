
 /**
	* subtle - window manager
	* Copyright (c) 2005-2008 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Id$
	**/

#include "subtle.h"

static int nsublets = 0;
static SubSublet **sublets = NULL;
static SubSublet *root = NULL;

 /** subSubletMerge {{{
	* Merge into the views
	* @param[in] pos Position of the sublet in the queue
	**/

void
subSubletMerge(int pos)
{
	int left	= 2 * pos;
	int right	= left + 1;
	int max 	= (left <= nsublets && sublets[left]->time < sublets[pos]->time) ? left : pos;

	if(right <= nsublets && sublets[right]->time < sublets[max]->time) max = right;
	if(max != pos)
		{
			SubSublet *tmp	= sublets[pos];
			sublets[pos]		= sublets[max];
			sublets[max]		= tmp;
			subSubletMerge(max);
		}
}
/* }}} */

 /** subSubletNext {{{
	* Get next sublet 
	* @return Return either the next #SubSublet or NULL if no sublets are loaded
	**/

SubSublet *
subSubletNext(void)
{
	return(nsublets > 0 ? sublets[1] : NULL);
}
/* }}} */

 /** subSubletNew {{{
	* Create new sublet 
	* @param[in] type Type of the sublet
	* @param[in] name Name of the sublet
	* @param[in] ref Lua object reference
	* @param[in] interval Update interval
	* @param[in] watch Watch file
	**/

void
subSubletNew(int type,
	char *name,
	int ref,
	time_t interval,
	char *watch)
{
	int i;
	SubSublet *s = (SubSublet *)subUtilAlloc(1, sizeof(SubSublet));

	if(!sublets) 
		{
			root		= s;
			sublets = (SubSublet **)subUtilAlloc(1, sizeof(SubSublet *));
		}

	/* Init the sublet */
	s->flags		= type;
	s->ref			= ref;
	s->interval	= interval;
	s->time			= subEventGetTime();

#ifdef HAVE_SYS_INOTIFY_H
	if(s->flags & SUB_SUBLET_TYPE_WATCH)
		{
			FILE *fd = NULL;

			/* Create the file before trying to add the watch */
			if((fd = fopen(watch, "w"))) fclose(fd);
			if((s->interval = inotify_add_watch(d->notify, watch, IN_MODIFY)) < 0)
				{
					subUtilLogWarn("Watch file `%s' does not exist\n", name);
					subUtilLogDebug("%s\n", strerror(errno));

					free(s);

					return;
				}
			else XSaveContext(d->disp, d->bar.sublets, s->interval, (void *)s);
		}
#endif /* HAVE_SYS_INOTIFY_H */

	subLuaCall(s);

  /* Don't add text and watch type sublets to the queue */
  if(type != SUB_SUBLET_TYPE_TEXT && type != SUB_SUBLET_TYPE_WATCH)
    {
			sublets = (SubSublet **)subUtilRealloc(sublets, sizeof(SubSublet *) * (nsublets + 2));
			i = ++nsublets;

			while(i > 1 && s->time < sublets[i / 2]->time)
				{
					sublets[i] = sublets[i / 2];
					i /= 2;
				}
			sublets[i] = s;
		}
	
	/* Smart use of unused array index */
	if(root != s) sublets[0]->next = s;
	sublets[0] = s;

	printf("Loading sublet %s (%d)\n", name, (int)interval);
	subUtilLogDebug("name=%s, ref=%d, interval=%d, watch=%s\n", name, ref, interval, watch);		
}
/* }}} */ 

 /** subSubletDelete {{{
	* Delete a sublet 
	* @param[in] s A #Sublet
	**/

void
subSubletDelete(SubSublet *s)
{
	int i, j;

	if(root == s) root = s->next;
	else
		{
			SubSublet *prev = root;

			while(prev->next && prev->next != s) prev = prev->next;
			prev->next = s->next;
		}

	for(i = 1; i <= nsublets; i++) 
		if(sublets[i] == s) for(j = i; j < nsublets; j++) sublets[j] = sublets[j + 1];

	sublets = (SubSublet **)subUtilRealloc(sublets, sizeof(SubSublet *) * (nsublets + 1));
	if(!(s->flags & SUB_SUBLET_TYPE_METER) && s->string) free(s->string);

	printf("Unloading sublet #%d\n", s->ref);
	nsublets--;
	free(s);

	subSubletConfigure();
	subSubletRender();
}
/* }}} */ 

 /** subSubletConfigure {{{
	* Configure sublet bar
	**/

void
subSubletConfigure(void)
{
	if(sublets)
		{
			int width = 3;
			SubSublet *s = root;

			while(s)
				{
					if(s->flags & SUB_SUBLET_TYPE_METER) width += 66;
					else if(!(s->flags & SUB_SUBLET_TYPE_HELPER)) width += strlen(s->string) * d->fx + 12;
					s = s->next;
				}
			XMoveResizeWindow(d->disp, d->bar.sublets, DisplayWidth(d->disp, DefaultScreen(d->disp)) - width, 0, width, d->th);
		}
}
/* }}} */

 /** subSubletRender {{{
	* Render the sublets
	**/

void
subSubletRender(void)
{
	if(sublets)
		{
			int width = 3;
			SubSublet *s = root;

			XClearWindow(d->disp, d->bar.sublets);

			while(s)
				{
					if(s->flags & SUB_SUBLET_TYPE_METER && s->number)
						{
							XDrawRectangle(d->disp, d->bar.sublets, d->gcs.font, width, 2, 60, d->th - 5);
							XFillRectangle(d->disp, d->bar.sublets, d->gcs.font, width + 2, 4, (56 * s->number) / 100, d->th - 8);
							width += 63;
						}
					else if(s->string) 
						{
							XDrawString(d->disp, d->bar.sublets, d->gcs.font, width, d->fy - 1, s->string, strlen(s->string));
							width += strlen(s->string) * d->fx + 6;
						}
					s = s->next;
				}
			XFlush(d->disp);
		}
}
/* }}} */

 /** subSubletKill {{{
	* Kill all sublets 
	**/

void
subSubletKill(void)
{
	SubSublet *s = root;

	while(s)
		{
			if(!(s->flags & SUB_SUBLET_TYPE_METER) && s->string) free(s->string);
			free(s);
			s = s->next;
		}
	free(sublets);
}
/* }}} */
