
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

/* ViewEmptyCols {{{ */
static void
ViewEmptyCols(void)
{
  int i, j;

  for(i = 0; i < subtle->clients->ndata; i++)
    {
      if(0 == subtle->percol[i]) ///< Col empty
        {
          for(j = 0; j < subtle->clients->ndata; j++)
            {
              SubClient *c = CLIENT(subtle->clients->data[j]);

              if(c->c == i + 1)
                {
                  c->c = i;
                }
            }
          for(j = i; j < subtle->clients->ndata - 1; j++)
            {
              subtle->percol[j] = subtle->percol[j + 1];
            }
        }
    }
  
} /* }}} */

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
 
  v->frame  = XCreateWindow(subtle->disp, ROOT, 0, subtle->th, SCREENW, SCREENH - subtle->th,
    0, CopyFromParent, InputOutput, CopyFromParent, 
    CWBackPixel|CWBackPixmap|CWEventMask|CWOverrideRedirect, &attrs); 
  v->button = XCreateSimpleWindow(subtle->disp, subtle->windows.views, 0, 0, 1,
    subtle->th, 0, subtle->colors.border, subtle->colors.norm);

  XSaveContext(subtle->disp, v->frame, VIEWID, (void *)v);
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
  subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1); ///< Init 

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
  int i, j, k, once, swap, total = 0;

  assert(v);

  if(0 < subtle->clients->ndata)
    {
      vid = subArrayIndex(subtle->views, (void *)v);

      /* Clients {{{ */
      for(i = 0, j = 0; i < subtle->clients->ndata; i++)
        {
          SubClient *c = CLIENT(subtle->clients->data[i]);

          /* Reset perrow/percol */
          subtle->perrow[i] = 0;
          subtle->percol[i] = 0;

          /* Find matching clients */
          if(VISIBLE(v, c))
            {
              if(!(c->flags & SUB_STATE_FULL)) ///< Don't overwrite root
                XReparentWindow(subtle->disp, c->win, v->frame, 0, 0);

              if(!(c->flags & (SUB_STATE_FULL|SUB_STATE_FLOAT))) 
                {
                  c->r = 0;
                  c->c = j;
                  total++;

                  XMapWindow(subtle->disp, c->win);
                  XLowerWindow(subtle->disp, c->win);

                  /* EWMH: Desktop */
                  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

                  subtle->percol[j++] = 1;
                }
              else ///< Special flags
                {
                  XMapRaised(subtle->disp, c->win);
                  subClientConfigure(c);
                }
            }
          else ///< Unmap other windows
            {
              c->flags |= SUB_STATE_UNMAP; ///< Skip next unmap event
              XUnmapWindow(subtle->disp, c->win); 
            }

          subtle->chkrow[i] = 0;
          subtle->chkcol[i] = 0;
        }
      subtle->perrow[0] = j; 
      /* }}} */

      /* Layout {{{ */
      for(i = 0; i < v->layout->ndata; i++)
        {
          int col = 0;
          SubLayout *l = LAYOUT(v->layout->data[i]);

          switch(l->flags & ~SUB_TYPE_LAYOUT)
            {
              case SUB_DRAG_BOTTOM:
                subtle->perrow[l->c2->r]--;
                subtle->percol[l->c2->c]--;
                subtle->percol[l->c1->c]--;

                col      = MIN(l->c1->c, l->c2->c);
                l->c2->r = l->c1->r + 1;
                l->c1->c = col;
                l->c2->c = col;

                subtle->perrow[l->c2->r]++;
                subtle->percol[l->c2->c] += 2;
                break;
              case SUB_DRAG_SWAP:
                swap     = l->c1->r;
                l->c1->r = l->c2->r;
                l->c2->r = swap;

                swap     = l->c1->c;
                l->c1->c = l->c2->c;
                l->c2->c = swap;
                break;
            }
        } /* }}} */

//ViewEmptyCols();

/* Table {{{ */
printf("\n  ");
for(i = 0; i < subtle->clients->ndata; i++)
  {
    printf("%d ", subtle->percol[i]);
  }
printf("\n");
for(i = 0; i < subtle->clients->ndata; i++) ///< Row
  {
    printf("%d ", subtle->perrow[i]);

    for(j = 0; j < subtle->clients->ndata; j++) ///< Col
      { 
        once = 0;
        for(k = 0; k < subtle->clients->ndata; k++) ///< Clients
          {
            SubClient *d = CLIENT(subtle->clients->data[k]);

            if(VISIBLE(v, d) && d->r == i && d->c == j)
              {
                printf("x ");
                once++;
              }
          }
        if(0 == once) printf("  ");
      }
    printf("%d\n", i);
  } 
printf("  ");
for(i = 0; i < subtle->clients->ndata; i++)
  {
    printf("%d ", i);
  }  
printf("\n\n"); /* }}} */

      /* Calculations {{{ */
      for(i = 0; i < subtle->clients->ndata; i++)
        {
          SubClient *c = CLIENT(subtle->clients->data[i]);

          if(VISIBLE(v, c))
            {      
              if(!(c->flags & (SUB_STATE_FULL|SUB_STATE_FLOAT))) 
                {
                  c->rect.width  = SCREENW / ZERO(subtle->perrow[c->r]);
                  c->rect.height = (SCREENH - subtle->th) / ZERO(subtle->percol[c->c]);
                  c->rect.x      = c->c * c->rect.width;
                  c->rect.y      = c->r * c->rect.height;

                  subtle->chkcol[c->c]++;
                  subtle->chkrow[c->r]++;

                  /* Compensation for int rounding */
                  if(subtle->perrow[c->r] == c->c + 1) 
                    c->rect.width += abs(SCREENW - subtle->perrow[c->r] * c->rect.width); 
                  if(subtle->percol[c->c] == c->r + 1) 
                    c->rect.height += 
                      abs((SCREENH - subtle->th) - subtle->percol[c->c] * c->rect.height); 

                  subClientConfigure(c);
                }
            }
        } /* }}} */

printf("r c   x   y width height\n");
for(i = 0; i < subtle->clients->ndata; i++)
  {
    SubClient *e = CLIENT(subtle->clients->data[i]);
    printf("%d %d %3d %3d %6d %6d\n", 
      e->r, e->c, e->rect.x, e->rect.y, e->rect.width, e->rect.height);
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
  SubLayout *l = NULL;

  assert(v && c1 && c2);

  l = subLayoutNew(c1, c2, mode);
  subArrayPush(v->layout, (void *)l);
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
      XUnmapWindow(subtle->disp, subtle->cv->frame);
      //XUnmapSubwindows(subtle->disp, subtle->cv->frame);
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

  frames = (Window *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(Window));
  names  = (char **)subSharedMemoryAlloc(subtle->views->ndata, sizeof(char *));

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

  subSharedLogDebug("publish=views, n=%d\n", i);

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

  subSharedLogDebug("kill=view\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
