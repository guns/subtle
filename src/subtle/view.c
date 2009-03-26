
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
  XSetWindowAttributes attrs;

  assert(name);
  
  v  = VIEW(subSharedMemoryAlloc(1, sizeof(SubView)));
  v->flags  = SUB_TYPE_VIEW;
  v->name   = strdup(name);
  v->width  = strlen(v->name) * subtle->fx + 8; ///< Font offset
  v->layout = subArrayNew();

  /* Create windows */
  attrs.event_mask        = ExposureMask|VisibilityChangeMask|KeyPressMask;
  attrs.background_pixmap = CopyFromParent;
  attrs.background_pixel  = subtle->colors.bg;
  attrs.override_redirect = True;
 
  v->win  = XCreateWindow(subtle->disp, ROOT, 0, subtle->th, SCREENW, SCREENH - subtle->th,
    0, CopyFromParent, InputOutput, CopyFromParent, 
    CWBackPixel|CWBackPixmap|CWEventMask|CWOverrideRedirect, &attrs); 
  v->button = XCreateSimpleWindow(subtle->disp, subtle->windows.views, 0, 0, 1,
    subtle->th, 0, subtle->colors.border, subtle->colors.norm);

  XSaveContext(subtle->disp, v->win, VIEWID, (void *)v);
  XSaveContext(subtle->disp, v->button, BUTTONID, (void *)v);
  XMapRaised(subtle->disp, v->button);

  /* Tags */
  if(tags)
    {
      int i;
      regex_t *preg = subSharedRegexNew(tags);

      for(i = 0; i < subtle->tags->ndata; i++)
        if(subSharedRegexMatch(preg, TAG(subtle->tags->data[i])->name)) 
          v->tags |= (1L << (i + 1));

      subSharedRegexKill(preg);
    }
  subEwmhSetCardinals(v->win, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1); ///< Init 

  printf("Adding view (%s)\n", v->name);
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
  long vid = 0;
  int i;

  assert(v);

  if(0 < subtle->clients->ndata)
    {
      vid = subArrayIndex(subtle->views, (void *)v);

      /* Clients */
      for(i = 0; i < subtle->clients->ndata; i++)
        {
          SubClient *c = CLIENT(subtle->clients->data[i]);

          /* Find matching clients */
          if(VISIBLE(v, c))
            {
              if(!(c->flags & SUB_STATE_FULL)) ///< Don't overwrite root
                XReparentWindow(subtle->disp, c->win, v->win, 0, 0);

              if(!(c->flags & (SUB_STATE_FULL|SUB_STATE_FLOAT))) 
                {
                  XMapWindow(subtle->disp, c->win);

                  /* Check current gravity */
                  if(c->gravity != c->gravities[vid]) 
                    {
                      c->gravity = c->gravities[vid];
                      subGravityCalc(&c->rect, c->gravities[vid]);
                    }
                  subClientConfigure(c);

                  /* EWMH: Desktop */
                  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);
                }
              else ///< Special flags
                {
                  XMapRaised(subtle->disp, c->win);
                }

            }
          else XUnmapWindow(subtle->disp, c->win); ///< Unmap other windows
        }
    }
} /* }}} */

 /** subViewUpdate {{{ 
  * @brief Update view bar
  **/

void
subViewUpdate(void)
{
  if(0 < subtle->views->ndata)
    {
      int i, width = 0;

      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          XMoveResizeWindow(subtle->disp, v->button, width, 0, v->width, subtle->th);
          width += v->width;
        }

      if(0 < width) 
        {
          XResizeWindow(subtle->disp, subtle->windows.views, width, subtle->th);
          XMoveWindow(subtle->disp, subtle->windows.caption, width, 0); 
        }
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

      /* Bar window */
      XClearWindow(subtle->disp, subtle->windows.bar);
      XFillRectangle(subtle->disp, subtle->windows.bar, subtle->gcs.stipple, 0, 2,
        SCREENW, subtle->th - 4);  

      /* View buttons */
      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          XSetWindowBackground(subtle->disp, v->button, 
            (subtle->cv == v) ? subtle->colors.focus : subtle->colors.norm);
          XClearWindow(subtle->disp, v->button);
          XDrawString(subtle->disp, v->button, subtle->gcs.font, 3, subtle->fy - 1,
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
  long vid = 0;

  assert(v);

  if(subtle->cv) 
    {
      XUnmapWindow(subtle->disp, subtle->cv->win);
    }
  subtle->cv = v;

  subViewConfigure(v);
  XMapWindow(subtle->disp, subtle->cv->win);

  /* EWMH: Current desktops */
  vid = subArrayIndex(subtle->views, (void *)v); ///< Get desktop number
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

  subViewRender();
  subGrabSet(subtle->cv->win);

  printf("Switching view (%s)\n", subtle->cv->name);
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

  assert(0 < subtle->views->ndata);

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
  vid = subArrayIndex(subtle->views, (void *)subtle->cv); ///< Get desktop number
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

  subSharedLogDebug("publish=views, n=%d\n", i);

  free(views);
  free(names);
} /* }}} */

 /** SubViewKill {{{
  * @brief Kill a view 
  * @param[in]  v  A #SubView
  **/

void
subViewKill(SubView *v)
{
  assert(v);

  printf("Killing view (%s)\n", v->name);

  XDeleteContext(subtle->disp, v->win, 1);
  XDeleteContext(subtle->disp, v->button, 1);

  XDestroyWindow(subtle->disp, v->button);
  XDestroyWindow(subtle->disp, v->win);

  subArrayKill(v->layout, True);

  free(v->name);
  free(v);          

  subSharedLogDebug("kill=view\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
