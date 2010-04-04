
 /**
  * @package subtle
  *
  * @file Sublet functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
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
  SubSublet *s = NULL;

  /* Create new sublet */
  s = SUBLET(subSharedMemoryAlloc(1, sizeof(SubSublet)));
  s->flags = SUB_TYPE_SUBLET;
  s->time  = subSubtleTime();
  s->text  = subSharedTextNew();
  s->bg    = subtle->colors.bg_sublets;

  /* Create window */
  s->win = XCreateSimpleWindow(subtle->dpy, subtle->windows.panel1, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.bg_sublets);


  XSaveContext(subtle->dpy, s->win, SUBLETID, (void *)s);

  subSharedLogDebug("new=sublet\n");

  return s;
} /* }}} */

 /** subSubletUpdate {{{
  * @brief Update sublet panel
  **/

void
subSubletUpdate(void)
{
  if(0 < subtle->sublets->ndata)
    {
      int i;

      for(i = 0; i < subtle->sublets->ndata; i++)
        {
          SubSublet *s = SUBLET(subtle->sublets->data[i]);

          XResizeWindow(subtle->dpy, s->win, s->width, subtle->th - 2 * subtle->pbw);
        }
    }
} /* }}} */

 /** subSubletRender {{{
  * @brief Render sublets
  * @param[in]  s  A #SubSublet
  **/

void
subSubletRender(SubSublet *s)
{
  assert(s);

  /* Set color */
  XSetWindowBackground(subtle->dpy, s->win, s->bg);
  XClearWindow(subtle->dpy, s->win);

  /* Render text parts */
  subSharedTextRender(subtle->dpy, subtle->gcs.font, subtle->font, s->win, 3, 
    subtle->font->y, subtle->colors.fg_sublets, subtle->colors.bg_sublets, 
    s->text);

  XResizeWindow(subtle->dpy, s->win, s->width, subtle->th - 2 * subtle->pbw);
  XSync(subtle->dpy, False); ///< Sync before going on
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
  
  /* Include only interval sublets */
  if(!(s1->flags & (SUB_SUBLET_INTERVAL))) return 1;
  if(!(s2->flags & (SUB_SUBLET_INTERVAL))) return -1;

  return s1->time < s2->time ? -1 : (s1->time == s2->time ? 0 : 1);
} /* }}} */

 /** subSubletPublish {{{
  * @brief Publish sublets
  **/

void
subSubletPublish(void)
{
  int i = 0, idx = 0;
  char **names = NULL;

  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata, sizeof(char *));

  /* Find sublets in panel list */
  for(i = 0; i < subtle->panels->ndata; i++)
    {
      SubSublet *s = SUBLET(subtle->panels->data[i]);

      if(s->flags & SUB_TYPE_SUBLET) ///< Collect names
        names[idx++] = s->name;
    }  

  /* EWMH: Sublet list */
  subEwmhSetStrings(ROOT, SUB_EWMH_SUBTLE_SUBLET_LIST, names, subtle->sublets->ndata);

  subSharedLogDebug("publish=sublet, n=%d\n", subtle->sublets->ndata);

  XSync(subtle->dpy, False); ///< Sync all changes

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

  /* Tidy up */
  subArrayRemove(subtle->panels, (void *)s);
  subHookRemove(s->instance, (void *)s);
  subRubyRelease(s->instance);
  subRubyRemove(s->name);

  /* Remove socket watch */
  if(s->flags & SUB_SUBLET_SOCKET)
    {
      XDeleteContext(subtle->dpy, subtle->windows.panel1, s->watch);
      subEventWatchDel(s->watch);
    }

#ifdef HAVE_SYS_INOTIFY_H
  /* Remove inotify watch */
  if(s->flags & SUB_SUBLET_INOTIFY)
    {
      XDeleteContext(subtle->dpy, subtle->windows.panel1, s->watch);
      inotify_rm_watch(subtle->notify, s->interval);
    }
#endif /* HAVE_SYS_INOTIFY_H */ 

  XDeleteContext(subtle->dpy, s->win, SUBLETID);
  XDestroyWindow(subtle->dpy, s->win);

  printf("Unloaded sublet (%s)\n", s->name);

  if(s->name) free(s->name);
  subSharedTextFree(s->text);
  free(s);

  subSharedLogDebug("kill=sublet\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
