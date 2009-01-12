
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
  * @return Returns a #SubSublet or \p NULL
  **/

SubSublet *
subSubletNew(void)
{
  SubSublet *s = SUBLET(subSharedMemoryAlloc(1, sizeof(SubSublet)));

  /* Init sublet */
  s->flags = SUB_TYPE_SUBLET;
  s->time  = subSharedTime();

  subSharedLogDebug("new=sublet\n");

  return s;
} /* }}} */ 

 /** subSubletUpdate {{{
  * @brief Update sublet bar
  **/

void
subSubletUpdate(void)
{
  if(0 < subtle->sublets->ndata)
    {
      int i, width = 3;

      for(i = 0; i < subtle->sublets->ndata; i++) ///< Calculate window width
        width += SUBLET(subtle->sublets->data[i])->width;

      XMoveResizeWindow(subtle->disp, subtle->windows.sublets, DisplayWidth(subtle->disp,
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

      XClearWindow(subtle->disp, subtle->windows.sublets);

      /* Render every sublet */
      while(s)
        {
          if(s->flags & SUB_DATA_FIXNUM && s->fixnum)
            {
              XDrawRectangle(subtle->disp, subtle->windows.sublets, subtle->gcs.font,
                width, 2, 60, subtle->th - 5);
              XFillRectangle(subtle->disp, subtle->windows.sublets, subtle->gcs.font,
                width + 2, 4, (56 * s->fixnum) / 100, subtle->th - 8);
            }
          else if(s->flags & SUB_DATA_STRING && s->string) 
            XDrawString(subtle->disp, subtle->windows.sublets, subtle->gcs.font, width,
              subtle->fy - 1, s->string, strlen(s->string));

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

 /** subSubletPublish {{{
  * @brief Publish sublets
  **/

void
subSubletPublish(void)
{
  int i;
  char **names = NULL;

  assert(0 < subtle->tags->ndata);

  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata, sizeof(char *));
  for(i = 0; i < subtle->sublets->ndata; i++) 
    names[i] = SUBLET(subtle->sublets->data[i])->name;

  /* EWMH: Sublet list */
  subEwmhSetStrings(ROOT, SUB_EWMH_SUBTLE_SUBLET_LIST, names, i);

  subSharedLogDebug("publish=sublet, n=%d\n", i);

  free(names);
} /* }}} */

 /** subSubletKill {{{
  * @brief Kill sublet
  * @param[in]  s  A #SubSublet
  **/

void
subSubletKill(SubSublet *s)
{
  assert(s);

  printf("Killing sublet (%s)\n", s->name);

  /* Update linked list */
  if(subtle->sublet == s) subtle->sublet = s->next;
  else
    {
      int i;
      SubSublet *prev = NULL;

      for(i = 0; i < subtle->sublets->ndata; i++)
        if((prev = SUBLET(subtle->sublets->data[i])) && prev->next == s)
          {
            prev->next = s->next;
            break;
          }
    }

  if(s->name)   free(s->name);
  if(s->string) free(s->string);
  free(s);

  subSharedLogDebug("kill=sublet\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
