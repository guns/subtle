
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
  XSetWindowAttributes sattrs;

  /* Create new sublet */
  s = SUBLET(subSharedMemoryAlloc(1, sizeof(SubSublet)));
  s->flags = SUB_TYPE_SUBLET;
  s->time  = subSubtleTime();
  s->text  = subSharedTextNew();
  s->bg    = subtle->colors.bg_sublets;

  /* Create window */
  s->win = XCreateSimpleWindow(subtle->dpy, ROOT, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.bg_sublets);

  sattrs.override_redirect = True;
  XChangeWindowAttributes(subtle->dpy, s->win, CWOverrideRedirect, &sattrs);
  XSaveContext(subtle->dpy, s->win, SUBLETID, (void *)s);

  subSharedLogDebug("new=sublet\n");

  return s;
} /* }}} */

 /** subSubletUpdate {{{
  * @brief Update sublet
  * @param[in]  s  A #SubSublet
  **/

void
subSubletUpdate(SubSublet *s)
{
  assert(s);

  XResizeWindow(subtle->dpy, s->win, s->width - 2 * subtle->pbw,
    subtle->th - 2 * subtle->pbw);

  /* Set borders */
  XSetWindowBorder(subtle->dpy, s->win, subtle->colors.bo_panel);
  XSetWindowBorderWidth(subtle->dpy, s->win, subtle->pbw);
} /* }}} */

 /** subSubletRender {{{
  * @brief Render sublets
  * @param[in]  s  A #SubSublet
  **/

void
subSubletRender(SubSublet *s)
{
  assert(s);

  /* Set window background */
  XSetWindowBackground(subtle->dpy, s->win, s->bg);
  XClearWindow(subtle->dpy, s->win);

  /* Render text parts */
  subSharedTextRender(subtle->dpy, subtle->gcs.font, subtle->font, s->win, 3,
    subtle->font->y, subtle->colors.fg_sublets, subtle->colors.bg_sublets,
    s->text);
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
  Window *wins = NULL;

  /* Alloc space */
  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata,
    sizeof(char *));
  wins  = (Window *)subSharedMemoryAlloc(subtle->sublets->ndata,
    sizeof(Window));

  /* Find sublets */
  for(i = 0; i < subtle->sublets->ndata; i++)
    {
      SubSublet *s = SUBLET(subtle->sublets->data[i]);

      names[idx]  = s->name;
      wins[idx++] = s->win;
    }

  /* EWMH: Sublet list and windows */
  subEwmhSetStrings(ROOT, SUB_EWMH_SUBTLE_SUBLET_LIST,
    names, subtle->sublets->ndata);
  subEwmhSetWindows(ROOT, SUB_EWMH_SUBTLE_SUBLET_WINDOWS,
    wins, subtle->sublets->ndata);

  subSharedLogDebug("publish=sublet, n=%d\n", subtle->sublets->ndata);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(names);
  free(wins);
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
  subHookRemove(s->instance, (void *)s);
  subRubyRelease(s->instance);

  /* Remove socket watch */
  if(s->flags & SUB_SUBLET_SOCKET)
    {
      XDeleteContext(subtle->dpy, subtle->windows.support, s->watch);
      subEventWatchDel(s->watch);
    }

  /* Unload event */
  if(s->flags & SUB_SUBLET_UNLOAD)
    subRubyCall(SUB_CALL_SUBLET_UNLOAD, s->instance, (void *)s, NULL);

#ifdef HAVE_SYS_INOTIFY_H
  /* Remove inotify watch */
  if(s->flags & SUB_SUBLET_INOTIFY)
    {
      XDeleteContext(subtle->dpy, subtle->windows.support, s->watch);
      inotify_rm_watch(subtle->notify, s->interval);
    }
#endif /* HAVE_SYS_INOTIFY_H */

  XDeleteContext(subtle->dpy, s->win, SUBLETID);
  XDestroyWindow(subtle->dpy, s->win);

  if(s->name)
    {
      printf("Unloaded sublet (%s)\n", s->name);
      free(s->name);
    }
  if(s->text) subSharedTextFree(s->text);
  free(s);

  subSharedLogDebug("kill=sublet\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
