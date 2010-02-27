
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
  
  v = VIEW(subSharedMemoryAlloc(1, sizeof(SubView)));
  v->flags = SUB_TYPE_VIEW;
  v->name  = strdup(name);
  v->width = subSharedTextWidth(v->name, strlen(v->name), NULL, 
    NULL, True) + 6; ///< Font offset

  /* Create button */
  v->button = XCreateSimpleWindow(subtle->dpy, subtle->windows.views.win, 
    0, 0, 1, subtle->th, 0, 0, subtle->colors.bg_views);

  XSaveContext(subtle->dpy, v->button, VIEWID, (void *)v);
  XSelectInput(subtle->dpy, v->button,  ButtonPressMask); 
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
  subEwmhSetCardinals(v->button, SUB_EWMH_SUBTLE_WINDOW_TAGS, 
    (long *)&v->tags, 1); ///< Init 

  subSharedLogDebug("new=view, name=%s\n", name);

  return v;
} /* }}} */

 /** subViewConfigure {{{
  * @brief Calculate client sizes and tiling layout
  * @param[in]  v      A #SubView
  * @param[in]  align  Re-align clients
  **/

void
subViewConfigure(SubView *v,
  int align)
{
  int i;
  long vid = 0;

  assert(v);

  vid = subArrayIndex(subtle->views, (void *)v);

  /* Clients */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      SubClient *c = CLIENT(subtle->clients->data[i]);

      /* Find matching clients */
      if(VISIBLE(v, c))
        {
          subClientSetScreen(c, c->screens[vid], align);

          if(!(c->flags & (SUB_MODE_FULL|SUB_MODE_FLOAT)))
            {
              subClientSetGravity(c, c->gravities[vid], align);

              XMapWindow(subtle->dpy, c->win);

              /* EWMH: Desktop */
              subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);
            }
          else XMapRaised(subtle->dpy, c->win); ///< Float/full

          subClientConfigure(c);
        }
      else XUnmapWindow(subtle->dpy, c->win); ///< Unmap other windows
    }

  subSharedFocus(True);

  subSharedLogDebug("Configure: type=view, vid=%d, name=%s\n", vid, v->name);

  /* Hook: Configure */
  subHookCall(SUB_HOOK_VIEW_CONFIGURE, (void *)v);
} /* }}} */

 /** subViewUpdate {{{ 
  * @brief Update view panel
  **/

void
subViewUpdate(void)
{
  subtle->windows.views.width = 0;

  if(0 < subtle->views->ndata)
    {
      int i;

      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          XMoveResizeWindow(subtle->dpy, v->button, subtle->windows.views.width, 
            0, v->width, subtle->th);
          subtle->windows.views.width += v->width;
        }

      XResizeWindow(subtle->dpy, subtle->windows.views.win, subtle->windows.views.width, subtle->th);
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
      long fg = 0, bg = 0;

      /* View buttons */
      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          /* Select color pair */
          if(v->flags & SUB_MODE_URGENT)
            {
              fg = subtle->colors.fg_urgent;
              bg = subtle->colors.bg_urgent;            
            }
          else if(CURVIEW == v)
            {
              fg = subtle->colors.fg_focus;
              bg = subtle->colors.bg_focus;
            }
          else
            {
              fg = subtle->colors.fg_views;
              bg = subtle->colors.bg_views;
            }

          subSharedTextDraw(v->button, 3, subtle->font.y, fg, bg, v->name);
        }
    }
} /* }}} */

 /** subViewJump {{{
  * @brief Jump to view
  * @param[in]  v      A #SubView
  * @param[in]  focus  Focus next client
  **/

void
subViewJump(SubView *v,
  int focus)
{
  assert(v);

  /* Store view */
  subtle->vid = subArrayIndex(subtle->views, (void *)v);

  /* EWMH: Current desktop */
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, 
    (long *)&subtle->vid, 1);

  subViewConfigure(v, False);

  subSharedFocus(focus);
  subViewRender();

  /* Hook: Jump */
  subHookCall(SUB_HOOK_VIEW_JUMP, (void *)v);
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

  XDeleteContext(subtle->dpy, v->button, VIEWID);
  XDestroyWindow(subtle->dpy, v->button);

  free(v->name);
  free(v);          

  subSharedLogDebug("kill=view\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
