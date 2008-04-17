
 /**
	* @package subtle
	*
	* @file Sublet functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel
	* @version $Id$
	*
	* See the COPYING file for the license in the latest tarball.
	**/

#include "subtle.h"

 /** subSubletNew {{{
	* @brief Create new sublet 
	* @param[in] type			Type of the sublet
	* @param[in] name			Name of the sublet
	* @param[in] ref			Lua object reference
	* @param[in] interval Update interval
	* @param[in] watch 		Watch file
	* @return Returns a #SubSublet or \p NULL
	**/

SubSublet *
subSubletNew(int type,
	char *name,
	int ref,
	time_t interval,
	char *watch)
{
	SubSublet *s = (SubSublet *)subUtilAlloc(1, sizeof(SubSublet));

	/* Algorithm needs an additional element */
	if(d->sublets->ndata == 0) subArrayPush(d->sublets, (void *)s);
	else
		{
			/* Use of unused array index */
			s->next = SUBLET(d->sublets->data[0]);
			d->sublets->data[0] = (void *)s;
		}

	/* Init the sublet */
	s->flags		= SUB_TYPE_SUBLET|type;
	s->ref			= ref;
	s->interval	= interval;
	s->time			= subUtilTime();

#ifdef HAVE_SYS_INOTIFY_H
	if(s->flags & SUB_SUBLET_TYPE_WATCH)
		{
			if((s->interval = inotify_add_watch(d->notify, watch, IN_MODIFY)) < 0)
				{
					subUtilLogWarn("Watch file `%s' does not exist\n", name);
					subUtilLogDebug("%s\n", strerror(errno));

					free(s);

					return(NULL);
				}
			else XSaveContext(d->disp, d->bar.sublets, s->interval, (void *)s);
		}
#endif /* HAVE_SYS_INOTIFY_H */

	subLuaCall(s);

  /* Don't add text and watch sublets to queue */
  if(!(type & SUB_SUBLET_TYPE_TEXT) && !(type & SUB_SUBLET_TYPE_WATCH))
    {
			int i = d->sublets->ndata;

			subArrayPush(d->sublets, (void *)s);

			while(i > 1 && s->time < SUBLET(d->sublets->data[i / 2])->time)
				{
					d->sublets->data[i] = d->sublets->data[i / 2];
					i /= 2;
				}
			d->sublets->data[i] = s;
		}
	
	printf("Loading sublet %s (%d)\n", name, (int)interval);
	subUtilLogDebug("new=sublet, name=%s, ref=%d, interval=%d, watch=%s\n", name, ref, interval, watch);		

	return(s);
} /* }}} */ 

 /** subSubletConfigure {{{
	* @brief Configure sublets bar
	**/

void
subSubletConfigure(void)
{
	if(d->sublets->ndata > 0)
		{
			int width = 3;
			SubSublet *s = (SubSublet *)d->sublets->data[1]; ///> Queue algorithm starts with the second element

			while(s)
				{
					if(s->flags & SUB_SUBLET_TYPE_METER) width += 66;
					else if(!(s->flags & SUB_SUBLET_TYPE_HELPER)) width += strlen(s->string) * d->fx + 12;
					s = s->next;
				}
			XMoveResizeWindow(d->disp, d->bar.sublets, DisplayWidth(d->disp, DefaultScreen(d->disp)) - width, 0, width, d->th);
		}
} /* }}} */

 /** subSubletRender {{{
	* @brief Render sublets
	**/

void
subSubletRender(void)
{
	if(d->sublets->ndata > 0)
		{
			int width = 3;
			SubSublet *s = (SubSublet *)d->sublets->data[1];  ///> Queue algorithm starts with the second element

			XClearWindow(d->disp, d->bar.sublets);

			while(s)
				{
					if(s->flags & SUB_SUBLET_TYPE_METER && s->number)
						{
							XDrawRectangle(d->disp, d->bar.sublets, d->gcs.font, width, 2, 60, d->th - 5);
							XFillRectangle(d->disp, d->bar.sublets, d->gcs.font, width + 2, 4, (56 * s->number) / 100, d->th - 8);
							width += 63; ///< Magic number
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
} /* }}} */

 /** subSubletMerge {{{
	* @brief Merge sublet into queue
	* @param[in] pos	Position of the sublet in the queue
	**/

void
subSubletMerge(int pos)
{
	int left = 2 * pos, right = left + 1, max = (left < d->sublets->ndata &&
		SUBLET(d->sublets->data[left])->time < SUBLET(d->sublets->data[pos])->time) ? left : pos;

	if(right < d->sublets->ndata && 
		SUBLET(d->sublets->data[right])->time < SUBLET(d->sublets->data[max])->time) max = right;
	if(max != pos)
		{
			void *tmp	= d->sublets->data[pos];
			d->sublets->data[pos]	= d->sublets->data[max];
			d->sublets->data[max]	= tmp;
			subSubletMerge(max);
		}
} /* }}} */

 /** subSubletNext {{{
	* @brief Get next sublet 
	* @return Returns a #SubSublet or \p NULL
	**/

SubSublet *
subSubletNext(void)
{
	return(d->sublets->ndata > 1 ? SUBLET(d->sublets->data[1]) : NULL);
} /* }}} */

 /** subSubletKill {{{
	* @brief Kill sublet
	* @param[in] s	A #SubSublet
	**/

void
subSubletKill(SubSublet *s)
{
	assert(s);

	printf("Killing sublet (#%d)\n", s->ref);

	if(!(s->flags & SUB_SUBLET_TYPE_METER) && s->string) free(s->string);
	free(s);

	subUtilLogDebug("kill=sublet\n");
} /* }}} */
