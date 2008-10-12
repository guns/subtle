
 /**
  * @package subtle
  *
  * @file Sublet functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subSubletNew {{{
  * @brief Create a new sublet 
  * @param[in]  recv      Ruby receiver
  * @param[in]  interval  Update interval
  * @param[in]  watch     Watch file
  * @return Returns a #SubSublet or \p NULL
  **/

SubSublet *
subSubletNew(unsigned long recv,
  time_t interval,
  char *watch)
{
  SubSublet *s = SUBLET(subUtilAlloc(1, sizeof(SubSublet)));

  /* Init sublet */
  s->flags    = SUB_TYPE_SUBLET;
  s->recv     = recv;
  s->interval = interval;
  s->time     = subUtilTime();

#ifdef HAVE_SYS_INOTIFY_H
  /* Create inotify watch */
  if(NULL != watch)
    {
      if(0 > (s->interval = inotify_add_watch(subtle->notify, watch, IN_MODIFY)))
        {
          subUtilLogWarn("Watch file `%s' does not exist\n", watch);
          subUtilLogDebug("%s\n", strerror(errno));

          subArrayPop(subtle->sublets, s);

          return NULL;
        }
      else XSaveContext(subtle->disp, subtle->bar.sublets, s->interval, (void *)s);
    }
#endif /* HAVE_SYS_INOTIFY_H */

  subRubyCall(s);
  subUtilLogDebug("new=sublet, ref=%ld, interval=%d, watch=%s\n", recv, interval, watch);    

  return s;
} /* }}} */ 

 /** subSubletConfigure {{{
  * @brief Calculate and update sublet bar
  **/

void
subSubletConfigure(void)
{
  if(0 < subtle->sublets->ndata)
    {
      int i, width = 3;

      for(i = 0; i < subtle->sublets->ndata; i++) ///< Calculate window width
        width += SUBLET(subtle->sublets->data[i])->width;

      XMoveResizeWindow(subtle->disp, subtle->bar.sublets, DisplayWidth(subtle->disp, 
        DefaultScreen(subtle->disp)) - width, 0, width, subtle->th);
    }
} /* }}} */

 /** subSubletRender {{{
  * @brief Render sublets
  **/

void
subSubletRender(void)
{
  if(0 < subtle->sublets->ndata)
    {
      int width = 3;
      SubSublet *s = SUBLET(subtle->sublet);

      XClearWindow(subtle->disp, subtle->bar.sublets);

      /* Render every sublet */
      while(s)
        {
          if(s->flags & SUB_DATA_FIXNUM && s->fixnum)
            {
              XDrawRectangle(subtle->disp, subtle->bar.sublets, subtle->gcs.font, width, 2, 60, subtle->th - 5);
              XFillRectangle(subtle->disp, subtle->bar.sublets, subtle->gcs.font, width + 2, 4, 
                (56 * s->fixnum) / 100, subtle->th - 8);
            }
          else if(s->flags & SUB_DATA_STRING && s->string) 
            XDrawString(subtle->disp, subtle->bar.sublets, subtle->gcs.font, width, subtle->fy - 1, 
              s->string, strlen(s->string));

          width += s->width;
          s     = s->next;
        }
      XFlush(subtle->disp);
    }
} /* }}} */

 /** subSubletCompare {{{
  * @brief Compare two sublets
  * @param[in]  a  A #SubSublet
  * @param[in]  b  A #SubSublet
  * @return Returns the result of the comparison of both sublets
  * @retval  -1  a is smaller
  * @retval  0   a and b are equal  
  * @retval  1   a is greater
  **/

int
subSubletCompare(const void *a,
  const void *b)
{
  SubSublet *s1 = *(SubSublet **)a, *s2 = *(SubSublet **)b;

  assert(a && b);

  return s1->time < s2->time ? -1 : (s1->time == s2->time ? 0 : 1);
} /* }}} */

 /** subSubletKill {{{
  * @brief Kill sublet
  * @param[in]  s  A #SubSublet
  **/

void
subSubletKill(SubSublet *s)
{
  assert(s);

  printf("Killing sublet\n");

  /* Update linked list */
  if(subtle->sublet == s) subtle->sublet = s->next;

  if(s->string) free(s->string);
  free(s);

  subUtilLogDebug("kill=sublet\n");
} /* }}} */
