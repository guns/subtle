
 /**
  * @package subtle
  *
  * @file View functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subViewNew {{{
  * @brief Create a new view
  * @param[in]  name  Name of the view
  * @param[in]  tags  Tags for the view
  * @return Returns a #SubView or \p NULL
  **/

SubView *
subViewNew(char *name,
  char *tags)
{
  SubView *v = NULL;

  assert(name);

  /* Create new view */
  v = VIEW(subSharedMemoryAlloc(1, sizeof(SubView)));
  v->flags = SUB_TYPE_VIEW;
  v->name  = strdup(name);

  /* Create button */
  v->win = XCreateSimpleWindow(subtle->dpy, ROOT,
    0, 0, 1, subtle->th, 0, 0, subtle->colors.bg_views);

  XSaveContext(subtle->dpy, v->win, VIEWID, (void *)v);
  XSelectInput(subtle->dpy, v->win, ButtonPressMask);

  /* Tags */
  if(tags && strncmp("", tags, 1))
    {
      int i;
      regex_t *preg = subSharedRegexNew(tags);

      for(i = 0; i < subtle->tags->ndata; i++)
        if(subSharedRegexMatch(preg, TAG(subtle->tags->data[i])->name))
          v->tags |= (1L << (i + 1));

      subSharedRegexKill(preg);
    }

  /* EWMH: Tags */
  subEwmhSetCardinals(v->win, SUB_EWMH_SUBTLE_WINDOW_TAGS,
    (long *)&v->tags, 1); ///< Init

  subSharedLogDebug("new=view, name=%s\n", name);

  return v;
} /* }}} */

 /** subViewHighlight {{{
  * @brief Highlight views with urgent clients
  * @param[in]  tags  Matching tags
  **/

void
subViewHighlight(int tags)
{
  int i;

  for(i = 0; i < subtle->views->ndata; i++)
    {
      SubView *v = VIEW(subtle->views->data[i]);

      /* Enable/disable highlighting */
      if(v->tags & tags)
        v->flags |= SUB_CLIENT_MODE_URGENT;
      else
        v->flags &= ~SUB_CLIENT_MODE_URGENT;
    }

  subScreenRender();
} /* }}} */

/** subViewVisible {{{
  * @brief Whether a view is visible on a screen
  * @param[in]  v  A #SubView
  * @return Screen id or \p null False
  **/

int
subViewVisible(SubView *v)
{
  int i, ret = False;

  assert(v);

  /* Check screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      if(v == VIEW(subtle->views->data[s->vid]))
        {
          ret = True;

          break;
        }
    }

  return ret;
} /* }}} */

 /** subViewJump {{{
  * @brief Jump to view
  * @param[in]  v  A #SubView
  **/

void
subViewJump(SubView *v)
{
  int i;
  SubScreen *s = NULL;

  assert(v);

  /* Check if view is visible on a screen */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      s = SCREEN(subtle->screens->data[i]);

      if(VIEW(subtle->views->data[s->vid]) == v)
        {
          subScreenJump(s);

          return;
        }
    }

  /* Get current screen */
  if((s = subScreenCurrent(NULL)))
    {
      s->vid = subArrayIndex(subtle->views, (void *)v);

      subScreenConfigure();
      subScreenRender();
      subSubtleFocus(True);
    }

  subSharedLogDebug("Jump: type=view\n");

  /* Hook: Jump, Tile */
  subHookCall(SUB_HOOK_VIEW_JUMP, (void *)v);
  subHookCall(SUB_HOOK_TILE, NULL);
} /* }}} */

 /** subViewPublish {{{
  * @brief Update EWMH infos
  **/

void
subViewPublish(void)
{
  int i;
  long vid = 0;
  char **names = NULL;
  Window *views = NULL;

  if(0 < subtle->views->ndata)
    {
      views = (Window *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(Window));
      names = (char **)subSharedMemoryAlloc(subtle->views->ndata, sizeof(char *));

      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          views[i] = v->win;
          names[i] = v->name;
        }

      /* EWMH: Virtual roots */
      subEwmhSetWindows(ROOT, SUB_EWMH_NET_VIRTUAL_ROOTS, views, i);

      /* EWMH: Desktops */
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&i, 1);
      subEwmhSetStrings(ROOT, SUB_EWMH_NET_DESKTOP_NAMES, names, i);

      /* EWMH: Current desktop */
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

      subSharedLogDebug("publish=views, n=%d\n", i);

      XSync(subtle->dpy, False); ///< Sync all changes

      free(views);
      free(names);
    }
} /* }}} */

 /** SubViewKill {{{
  * @brief Kill a view
  * @param[in]  v  A #SubView
  **/

void
subViewKill(SubView *v)
{
  assert(v);

  /* Hook: Kill */
  subHookCall(SUB_HOOK_VIEW_KILL, (void *)v);

  XDeleteContext(subtle->dpy, v->win, VIEWID);
  XDestroyWindow(subtle->dpy, v->win);

  free(v->name);
  free(v);

  subSharedLogDebug("kill=view\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
