
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

  subSharedLogDebug("new=view, name=%s\n", name);

  return v;
} /* }}} */

 /** subViewFocus {{{
  * @brief Restore view focus
  * @param[in]  v  A #SubView
  **/

void
subViewFocus(SubView *v,
  int focus)
{
  assert(v);

  /* Focus */
  if(focus && None != v->focus)
    {
      SubClient *c = NULL;

      if((c = CLIENT(subSubtleFind(v->focus, CLIENTID))) &&
          VISIBLE(v->tags, c))
        {
          subClientFocus(c);
          subClientWarp(c, False);

          return;
        }
      else v->focus = None;
    }

  subSubtleFocus(focus);
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

          /* Hook: Jump */
          subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_FOCUS),
            (void *)v);

          return;
        }
    }

  /* Get current screen */
  if((s = subScreenCurrent(NULL)))
    {
      s->vid = subArrayIndex(subtle->views, (void *)v);

      subScreenConfigure();
      subScreenRender();
      subScreenPublish();
      subViewFocus(v, True);
    }

  subSharedLogDebug("Jump: type=view\n");

  /* Hook: Jump, Tile */
  subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_FOCUS), (void *)v);
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
  long *tags = NULL;

  if(0 < subtle->views->ndata)
    {
      tags = (long *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(long));
      names = (char **)subSharedMemoryAlloc(subtle->views->ndata, sizeof(char *));

      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          tags[i]  = v->tags;
          names[i] = v->name;
        }

      /* EWMH: Tags */
      subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_VIEW_TAGS,
        tags, subtle->views->ndata);

      /* EWMH: Desktops */
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&i, 1);
      subSharedPropertySetStrings(subtle->dpy, ROOT,
        subEwmhGet(SUB_EWMH_NET_DESKTOP_NAMES), names, subtle->views->ndata);

      /* EWMH: Current desktop */
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

      XSync(subtle->dpy, False); ///< Sync all changes

      free(tags);
      free(names);

      subSharedLogDebug("publish=views, n=%d\n", subtle->views->ndata);
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
  subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_KILL),
    (void *)v);

  if(v->icon) free(v->icon);
  free(v->name);
  free(v);

  subSharedLogDebug("kill=view\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
