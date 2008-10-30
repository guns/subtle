
 /**
  * @package subtle
  *
  * @file View functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
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
  
  v  = VIEW(subUtilAlloc(1, sizeof(SubView)));
  v->flags  = SUB_TYPE_VIEW;
  v->name   = strdup(name);
  v->width  = strlen(v->name) * subtle->fx + 8; ///< Font offset
  v->layout = subArrayNew();

  /* Create windows */
  attrs.event_mask        = ExposureMask|VisibilityChangeMask|KeyPressMask;
  attrs.background_pixmap = CopyFromParent;
  attrs.background_pixel  = subtle->colors.bg;
 
  v->frame  = XCreateWindow(subtle->disp, ROOT, 0, subtle->th, SCREENW, SCREENH - subtle->th,
    0, CopyFromParent, InputOutput, CopyFromParent, CWBackPixel|CWBackPixmap|CWEventMask, 
    &attrs); 
  v->button = XCreateSimpleWindow(subtle->disp, subtle->bar.views, 0, 0, 1, 
    subtle->th, 0, subtle->colors.border, subtle->colors.norm);

  XSaveContext(subtle->disp, v->frame, VIEWID, (void *)v);
  XSaveContext(subtle->disp, v->button, BUTTONID, (void *)v);
  XMapRaised(subtle->disp, v->button);

  /* Tags */
  if(tags)
    {
      int i;
      regex_t *preg = subUtilRegexNew(tags);

      for(i = 0; i < subtle->tags->ndata; i++)
        if(subUtilRegexMatch(preg, TAG(subtle->tags->data[i])->name)) v->tags |= (1L << (i + 1));

      subUtilRegexKill(preg);
      subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS, (long *)&v->tags, 1);
    }

  printf("Adding view (%s)\n", v->name);
  subUtilLogDebug("new=view, name=%s\n", name);

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
  int x = 0, y = 0, width = 0, height = 0, cw = 0, comp = 0, total = 0, tiled = 0,
    resized = 0, full = 0, floated = 0, special = 0;

  assert(v);

  vid = subArrayIndex(subtle->views, (void *)v);

  if(0 < subtle->clients->ndata)
    {
       y      = subtle->th;
       width  = SCREENW;
       height = SCREENH - subtle->th;

      /* Find clients and count special states*/
      for(i = 0; i < subtle->clients->ndata; i++)
        {
          SubClient *c = CLIENT(subtle->clients->data[i]);

          /* Find matching clients */
          if(v->tags & c->tags || c->flags & SUB_STATE_URGENT)
            {
              if(c->flags & SUB_STATE_FULL) full++;
              else if(c->flags & SUB_STATE_FLOAT) floated++;
              c->flags &= ~SUB_STATE_TILED;
              total++;
            }
        }

      /* Mark clients in a tile */
      for(i = 0; i < v->layout->ndata; i++)
        {
          SubLayout *l = LAYOUT(v->layout->data[i]);

          if(!(l->flags & SUB_TILE_SWAP)) 
            {
              l->c2->flags |= SUB_STATE_TILED;
              tiled++;
            }
        }

      subUtilLogDebug("total=%d, layouts=%d, tiled=%d\n", total, v->layout->ndata, tiled);
      total -= tiled;

      /* Calculations */
      special = resized + full + floated;
      total   = total > special ? total - special : 1; ///< Prevent division by zero
      cw      = width / total;
      comp    = abs(width - total * cw); ///< Compensation for int rounding

      /* Set client stuff */
      for(i = 0; i < subtle->clients->ndata; i++)
        {
          SubClient *c = CLIENT(subtle->clients->data[i]);

          if(v->tags & c->tags || c->flags & SUB_STATE_URGENT)
            {
              if(!(c->flags & SUB_STATE_FULL)) ///< Don't overwrite root
                XReparentWindow(subtle->disp, c->win, v->frame, 0, 0);

              XMapWindow(subtle->disp, c->win);
              XLowerWindow(subtle->disp, c->win);

              /* EWMH: Desktop */
              subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

              /* Special flags */
              if(c->flags & (SUB_STATE_FLOAT|SUB_STATE_FULL))
                {
                  XRaiseWindow(subtle->disp, c->win);
                  subClientConfigure(c);
                  continue;
                }

              c->rect.x      = x;
              c->rect.y      = 0;
              c->rect.width  = i == total - 1 ? cw + comp : cw; ///< Compensation
              c->rect.height = height;

              if(!(c->flags & SUB_STATE_TILED)) 
                {
                  x += cw;
                  subClientConfigure(c);
                 }
            }
        }

      /* Make layout */
      for(i = 0; i < v->layout->ndata; i++)
        {
          SubLayout *l  = LAYOUT(v->layout->data[i]);

          /* Check if both clients are on this view */
          if(v->tags & l->c1->tags && v->tags & l->c2->tags)
            {
              if(l->flags & SUB_TILE_VERT) 
                {
                    
                  l->c1->rect.height  = l->c1->rect.height / 2;
                  l->c2->rect.x       = l->c1->rect.x;
                  l->c2->rect.y       = l->c1->rect.y + l->c1->rect.height;
                  l->c2->rect.width   = l->c1->rect.width;
                  l->c2->rect.height  = l->c1->rect.height;
                }
              else if(l->flags & SUB_TILE_HORZ) 
                {
                  l->c1->rect.width   = l->c1->rect.width / 2;
                  l->c2->rect.x       = l->c1->rect.width;
                  l->c2->rect.y       = l->c1->rect.y;
                  l->c2->rect.width   = l->c1->rect.width;          
                  l->c2->rect.height  = l->c1->rect.height;

                  if(l->c1->flags & SUB_STATE_RESIZE)
                    {
                      int size = l->c1->rect.width * l->c1->size / 100;

                      l->c1->rect.width = size;
                      l->c2->rect.x     = l->c1->rect.x + size;
                      l->c2->rect.width = 2 * l->c2->rect.width - size;

                      if(l->c2->flags & SUB_STATE_RESIZE)
                        {
                          l->c2->flags &= ~SUB_STATE_RESIZE;
                          l->c2->size  = 0;
                        }
                    }
                  else if(l->c2->flags & SUB_STATE_RESIZE)
                    {
                      int size = l->c2->rect.width * l->c2->size / 100;

                      l->c2->rect.width = size;
                      l->c1->rect.width = 2 * l->c1->rect.width - size;
                      l->c2->rect.x     = l->c1->rect.width;
                    }
                    
                }
              else if(l->flags & SUB_TILE_SWAP) 
                {
                  XRectangle r = l->c1->rect;

                  l->c1->rect = l->c2->rect;
                  l->c2->rect = r;
                }

              subClientConfigure(l->c1);
              subClientConfigure(l->c2);
            }
          else subArrayPop(v->layout, (void *)l); ///< Sanitize
        }
    }
} /* }}} */

 /** subViewArrange {{{
  * @brief Remove old layouts and add new
  * @param[in]  v   A #SubView
  * @param[in]  c1  A #SubClient
  * @param[in]  c2  A #SubClient
  **/

void
subViewArrange(SubView *v,
  SubClient *c1,
  SubClient *c2,
  int mode)
{
  int i;
  SubLayout *l = NULL;

  assert(v && c1 && c2);

  /* Remove old layouts */
  for(i = 0; i < v->layout->ndata; i++)
    {
      l = LAYOUT(v->layout->data[i]);

      if(!(l->flags & SUB_TILE_SWAP))
        {
          if(c1 == l->c1 || c2 == l->c1 || c1 == l->c2 || c2 == l->c2)
            subArrayPop(v->layout, (void *)l);
        }
    }
 
  l = subLayoutNew(c1, c2, mode);
  subArrayPush(v->layout, (void *) l);
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
          XResizeWindow(subtle->disp, subtle->bar.views, width, subtle->th);
          XMoveWindow(subtle->disp, subtle->bar.caption, width, 0); 
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
      XClearWindow(subtle->disp, subtle->bar.win);
      XFillRectangle(subtle->disp, subtle->bar.win, subtle->gcs.border, 0, 2, 
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
      XUnmapWindow(subtle->disp, subtle->cv->frame);
      XUnmapSubwindows(subtle->disp, subtle->cv->frame);
    }
  subtle->cv = v;

  subViewConfigure(v);
  XMapWindow(subtle->disp, subtle->cv->frame);

  /* EWMH: Current desktops */
  vid = subArrayIndex(subtle->views, (void *)v); ///< Get desktop number
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

  subViewRender();
  subGrabSet(subtle->cv->frame);

  printf("Switching view (%s)\n", subtle->cv->name);
} /* }}} */

 /** subViewFind {{{
  * @brief Find view
  * @param[in]   name  Name of view
  * @param[out]  id    View id
  * @return Returns a #SubView or \p NULL
  **/

SubView *
subViewFind(char *name,
  int *id)
{
  int i;
  SubView *v = NULL;

  /* @todo Linear search.. */
  for(i = 0; i < subtle->views->ndata; i++)
    {
      v = VIEW(subtle->views->data[i]);

      if(!strncmp(v->name, name, strlen(v->name))) 
        {
          if(id) *id = i;
          return v;
        }
    }
  
  return NULL;
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
  Window *frames = NULL;

  assert(0 < subtle->views->ndata);

  frames = (Window *)subUtilAlloc(subtle->views->ndata, sizeof(Window));
  names  = (char **)subUtilAlloc(subtle->views->ndata, sizeof(char *));

  for(i = 0; i < subtle->views->ndata; i++)
    {
      SubView *v = VIEW(subtle->views->data[i]);

      frames[i] = v->frame;
      names[i]  = v->name;
    }

  /* EWMH: Virtual roots */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_VIRTUAL_ROOTS, frames, i);

  /* EWMH: Desktops */
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_NUMBER_OF_DESKTOPS, (long *)&i, 1);
  subEwmhSetStrings(ROOT, SUB_EWMH_NET_DESKTOP_NAMES, names, i);

  /* EWMH: Current desktop */
  vid = subArrayIndex(subtle->views, (void *)subtle->cv); ///< Get desktop number
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

  subUtilLogDebug("publish=views, n=%d\n", i);

  free(frames);
  free(names);
} /* }}} */

 /** subViewSanitize {{{
  * @brief Remove any layout for c
  * @param[in]  c  A #SubClient
  **/

void
subViewSanitize(SubClient *c)
{
  int i, j;

  assert(c);

  for(i = 0; i < subtle->views->ndata; i++)
    {
      SubView *v = VIEW(subtle->views->data[i]);

      for(j = 0; j < v->layout->ndata; j++)
        {
          SubLayout *l = LAYOUT(v->layout->data[j]);

          if(l->c1 == c || l->c2 == c) 
            {
              subArrayPop(v->layout, (void *)l);
              subLayoutKill(l);
            }
        }
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

  printf("Killing view (%s)\n", v->name);

  XDeleteContext(subtle->disp, v->frame, 1);
  XDeleteContext(subtle->disp, v->button, 1);

  XDestroyWindow(subtle->disp, v->button);
  XDestroyWindow(subtle->disp, v->frame);

  subArrayKill(v->layout, True);

  free(v->name);
  free(v);          

  subUtilLogDebug("kill=view\n");
} /* }}} */
