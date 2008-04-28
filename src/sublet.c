
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

	/* Init sublet */
	s->flags		= SUB_TYPE_SUBLET|type;
	s->ref			= ref;
	s->interval	= interval;
	s->time			= subUtilTime();

	subArrayPush(d->sublets, (void *)s);

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
			int i, width = 3;

			/* Calc window width */
			for(i = 0; i < d->sublets->ndata; i++) width += SUBLET(d->sublets->data[i])->width;

			XMoveResizeWindow(d->disp, d->bar.sublets, DisplayWidth(d->disp, 
				DefaultScreen(d->disp)) - width, 0, width, d->th);
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
			SubSublet *s = SUBLET(d->sublet);

			XClearWindow(d->disp, d->bar.sublets);

			while(s)
				{
					if(s->flags & SUB_SUBLET_TYPE_METER && s->number)
						{
							XDrawRectangle(d->disp, d->bar.sublets, d->gcs.font, width, 2, 60, d->th - 5);
							XFillRectangle(d->disp, d->bar.sublets, d->gcs.font, width + 2, 4, (56 * s->number) / 100, d->th - 8);
						}
					else if(s->string) 
						XDrawString(d->disp, d->bar.sublets, d->gcs.font, width, d->fy - 1, s->string, strlen(s->string));

					width += s->width;
					s = s->next;
				}
			XFlush(d->disp);
		}
} /* }}} */

 /** subSubletCompare {{{
	* @brief Compare two sublets
	* @param[in] a	A #SubSublet
	* @param[in] b	A #SubSublet
	* @return Returns the result of the comparison of both sublets
	* @retval -1 a is smaller
	* @retval 0	a and b are equal	
	* @retval 1 a is greater
	**/

int
subSubletCompare(const void *a,
	const void *b)
{
	SubSublet *s1 = *(SubSublet **)a, *s2 = *(SubSublet **)b;

	assert(a && b);

	return(s1->time < s2->time ? -1 : (s1->time == s2->time ? 0 : 1));
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
