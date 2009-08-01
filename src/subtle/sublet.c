
 /**
  * @package subtle
  *
  * @file Sublet functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
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
  s->text  = subArrayNew();

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
      int i;
      
      subtle->panels.sublets.width = 3;

      for(i = 0; i < subtle->sublets->ndata; i++) ///< Calculate window width
        subtle->panels.sublets.width += SUBLET(subtle->sublets->data[i])->width;

      XResizeWindow(subtle->dpy, subtle->panels.sublets.win, 
        subtle->panels.sublets.width + 3, subtle->th);
    }
  else XUnmapWindow(subtle->dpy, subtle->panels.sublets.win);
} /* }}} */

 /** subSubletRender {{{
  * @brief Render sublets
  **/

void
subSubletRender(void)
{
  if(0 < subtle->sublets->ndata)
    {
      int i, width = 3;
      XGCValues gvals;
      SubSublet *s = SUBLET(subtle->sublet);
      SubText *t = NULL;

      XClearWindow(subtle->dpy, subtle->panels.sublets.win);

      /* Init GC */
      gvals.foreground = subtle->colors.fg_sublets;
      XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);

      /* Render every sublet */
      while(s)
        {
          /* Render text part */
          for(i = 0; i < s->text->ndata; i++)
            {
              if((t = TEXT(s->text->data[i])) && t->flags & SUB_DATA_STRING)
                {
                  /* Update GC */
                  gvals.foreground = t->color;
                  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);

                  XDrawString(subtle->dpy, subtle->panels.sublets.win, subtle->gcs.font, width,
                    subtle->fy - 1, t->data.string, strlen(t->data.string));

                  width += t->width;
                }
            }

          s = s->next;
        }

      XFlush(subtle->dpy);
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
  
  /* Exclude notify sublets */
  if(s1->flags & SUB_SUBLET_INOTIFY) return 1;
  else if(s2->flags & SUB_SUBLET_INOTIFY) return -1;

  return s1->time < s2->time ? -1 : (s1->time == s2->time ? 0 : 1);
} /* }}} */

 /** subSubletPublish {{{
  * @brief Publish sublets
  **/

void
subSubletPublish(void)
{
  int i = 0;
  char **names = NULL;
  SubSublet *iter = NULL;
  
  iter  = subtle->sublet;
  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata, sizeof(char *));

  /* Get list in order */
  while(iter)
    {
      names[i++] = iter->name;
      iter       = iter->next;
    }

  /* EWMH: Sublet list */
  subEwmhSetStrings(ROOT, SUB_EWMH_SUBTLE_SUBLET_LIST, names, subtle->sublets->ndata);

  subSharedLogDebug("publish=sublet, n=%d\n", subtle->sublets->ndata);

  free(names);
} /* }}} */

 /** subSubletKill {{{
  * @brief Kill sublet
  * @param[in]  s       A #SubSublet
  * @param[in]  unlink  Unlink sublets
  **/

void
subSubletKill(SubSublet *s,
  int unlink)
{
  assert(s);

  if(unlink)
    {
      /* Update linked list */
      if(subtle->sublet == s) subtle->sublet = s->next;
      else
        {
          SubSublet *iter = subtle->sublet;

          while(iter && iter->next != s) iter = iter->next;

          iter->next = s->next;
        }

      subRubyRemove(s->name); ///< Remove class definition
    }

#ifdef HAVE_SYS_INOTIFY_H
  /* Tidy up inotify */
  if(s->flags & SUB_SUBLET_INOTIFY)
    {
      inotify_rm_watch(subtle->notify, s->interval);

      if(s->path) free(s->path);
    }
#endif /* HAVE_SYS_INOTIFY_H */ 

  if(s->name) free(s->name);
  subArrayKill(s->text, True);
  free(s);

  subSharedLogDebug("kill=sublet\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
