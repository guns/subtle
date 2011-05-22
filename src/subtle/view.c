
 /**
  * @package subtle
  *
  * @file View functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
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
  v->width = subSharedTextWidth(subtle->dpy, subtle->font,
    v->name, strlen(v->name), NULL, NULL, True);

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

  subSharedLogDebugSubtle("new=view, name=%s\n", name);

  return v;
} /* }}} */

 /** subViewFocus {{{
  * @brief Restore view focus
  * @param[in]  v      A #SubView
  * @param[in]  focus  Whether to focus next client
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

  /* Hook: Jump, Tile */
  subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_FOCUS), (void *)v);
  subHookCall(SUB_HOOK_TILE, NULL);

  subSharedLogDebugSubtle("Jump: type=view\n");
} /* }}} */

 /** subViewSwitch {{{
  * @brief Switch views or jump
  * @param[in]  v      A #SubView
  * @param[in]  sid    Screen id
  * @param[in]  focus  Whether to focus next client
  **/

void
subViewSwitch(SubView *v,
  int sid,
  int focus)
{
  int i, swap = -1;
  SubScreen *s1 = NULL;

  assert(v);

  /* Get working screen */
  if(!(s1 = subArrayGet(subtle->screens, sid)))
    s1 = subScreenCurrent(NULL);

  /* Check if there is only one screen */
  if(1 < subtle->screens->ndata)
    {
      /* Check if view is visible on any screen */
      for(i = 0; i < subtle->screens->ndata; i++)
        {
          SubScreen *s2 = SCREEN(subtle->screens->data[i]);

          if(s1 != s2 && subArrayGet(subtle->views, s2->vid) == v)
            {
              /* Swap views */
              swap    = s1->vid;
              s1->vid = s2->vid;
              s2->vid = swap;

              break;
            }
        }
    }

  /* Set view and configure */
  if(-1 == swap) s1->vid = subArrayIndex(subtle->views, (void *)v);

  subScreenConfigure();
  subScreenRender();
  subScreenPublish();

  subViewFocus(v, focus);

  /* Hook: Jump */
  subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_FOCUS), (void *)v);
} /* }}} */

 /** subViewPublish {{{
  * @brief Update EWMH infos
  **/

void
subViewPublish(void)
{
  int i;
  long vid = 0, *tags = NULL, *icons = NULL;
  char **names = NULL;

  if(0 < subtle->views->ndata)
    {
      tags  = (long *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(long));
      icons = (long *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(long));
      names = (char **)subSharedMemoryAlloc(subtle->views->ndata, sizeof(char *));

      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          tags[i]  = v->tags;
          icons[i] = v->icon ? v->icon->pixmap : -1;
          names[i] = v->name;
        }

      /* EWMH: Tags */
      subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_VIEW_TAGS,
        tags, subtle->views->ndata);

      /* EWMH: Icons */
      subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_VIEW_ICONS,
        icons, subtle->views->ndata);

      /* EWMH: Desktops */
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&i, 1);
      subSharedPropertySetStrings(subtle->dpy, ROOT,
        subEwmhGet(SUB_EWMH_NET_DESKTOP_NAMES), names, subtle->views->ndata);

      /* EWMH: Current desktop */
      subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

      XSync(subtle->dpy, False); ///< Sync all changes

      free(tags);
      free(icons);
      free(names);
    }

  subSharedLogDebugSubtle("publish=views, n=%d\n", subtle->views->ndata);
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

  subSharedLogDebugSubtle("kill=view\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
