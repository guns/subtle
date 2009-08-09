
 /**
  * @package subtle
  *
  * @file View functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
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
  
  v  = VIEW(subSharedMemoryAlloc(1, sizeof(SubView)));
  v->flags = SUB_TYPE_VIEW;
  v->name  = strdup(name);
  v->width = XTextWidth(subtle->xfs, v->name, strlen(v->name)) + 6; ///< Font offset

  /* Create button */
  v->button = XCreateSimpleWindow(subtle->dpy, subtle->panels.views.win, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.bg_views);

  XSaveContext(subtle->dpy, v->button, BUTTONID, (void *)v);
  XMapRaised(subtle->dpy, v->button);

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
  subEwmhSetCardinals(v->button, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1); ///< Init 

  subSharedLogDebug("new=view, name=%s\n", name);

  return v;
} /* }}} */

 /** subViewConfigure {{{
  * @brief Calculate client sizes and tiling layout
  * @param[in]  v  A #SubView
  **/

void
subViewConfigure(SubView *v)
{
  int i;
  long vid = 0;

  assert(v);

  /* Hook: Configure */
  if(subtle->hooks.configure &&
    0 == subRubyCall(SUB_CALL_HOOK, subtle->hooks.configure, (void *)v))
    {
      subSharedLogDebug("Hook: name=configure, view=%#lx, state=ignored\n", v->button);

      return;
    }

  vid = subArrayIndex(subtle->views, (void *)v);

  /* Clients */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      SubClient *c = CLIENT(subtle->clients->data[i]);

      /* Find matching clients */
      if(VISIBLE(v, c))
        {
          if(!(c->flags & (SUB_MODE_FULL|SUB_MODE_FLOAT)))
            {
              subClientSetScreen(c, c->screens[vid], False);
              subClientSetGravity(c, c->gravities[vid], False);
              subClientConfigure(c);

              XMapWindow(subtle->dpy, c->win);

              /* EWMH: Desktop */
              subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);
            }
          else XMapRaised(subtle->dpy, c->win); ///< Float/full
        }
      else XUnmapWindow(subtle->dpy, c->win); ///< Unmap other windows
    }
} /* }}} */

 /** subViewUpdate {{{ 
  * @brief Update view bar
  **/

void
subViewUpdate(void)
{
  subtle->panels.views.width = 0;

  if(0 < subtle->views->ndata)
    {
      int i;

      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          XMoveResizeWindow(subtle->dpy, v->button, subtle->panels.views.width, 
            0, v->width, subtle->th);
          subtle->panels.views.width += v->width;
        }

      XResizeWindow(subtle->dpy, subtle->panels.views.win, 
        subtle->panels.views.width, subtle->th);
    }
} /* }}} */

 /** subViewRender {{{ 
  * @brief Render view button bar
  * @param[in]  v  A #SubView
  **/

void
subViewRender(void)
{
  if(0 < subtle->views->ndata)
    {
      int i;
      XGCValues gvals;

      /* View buttons */
      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          /* Select color pair */
          if(subtle->view == v)
            {
              gvals.foreground = subtle->colors.fg_focus;
              gvals.background = subtle->colors.bg_focus;
            }
          else
            {
              gvals.foreground = subtle->colors.fg_views;
              gvals.background = subtle->colors.bg_views;
            }

          XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);
          XSetWindowBackground(subtle->dpy, v->button, gvals.background); 
          XClearWindow(subtle->dpy, v->button);
          XDrawString(subtle->dpy, v->button, subtle->gcs.font, 3, subtle->fy,
            v->name, strlen(v->name));
        }
    }
} /* }}} */

 /** subViewJump {{{
  * @brief Jump to view
  * @param[in]  v  A #SubView
  **/

void
subViewJump(SubView *v)
{
  assert(v);

  /* Hook: Jump */
  if(subtle->hooks.jump &&
    0 == subRubyCall(SUB_CALL_HOOK, subtle->hooks.jump, (void *)v))
    {
      subSharedLogDebug("Hook: name=jump, view=%#lx, state=ignored\n", v->button);

      return;
    }

  /* Store view */
  subtle->vid  = subArrayIndex(subtle->views, (void *)v);
  subtle->view = v;

  subViewConfigure(v);

  /* EWMH: Current desktop */
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, (long *)&subtle->vid, 1);

  subSharedFocus();
  subViewRender();
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

          views[i] = v->button;
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

      free(views);
      free(names);

      XSync(subtle->dpy, False);
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

  XDeleteContext(subtle->dpy, v->button, BUTTONID);
  XDestroyWindow(subtle->dpy, v->button);

  free(v->name);
  free(v);          

  subSharedLogDebug("kill=view\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
