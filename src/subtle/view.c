
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
  v->width = strlen(v->name) * subtle->fx + 8; ///< Font offset

  /* Create button */
  v->button = XCreateSimpleWindow(subtle->disp, subtle->windows.views, 0, 0, 1,
    subtle->th, 0, subtle->colors.border, subtle->colors.norm);

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

  /* EWMH: Tags */
  subEwmhSetCardinals(v->button, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1); ///< Init 

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
  int i, visible = 0;

  assert(v);

  vid = subArrayIndex(subtle->views, (void *)v);

  /* Clients */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      SubClient *c = CLIENT(subtle->clients->data[i]);

      /* Find matching clients */
      if(VISIBLE(v, c))
        {
          visible++;
          XMapWindow(subtle->disp, c->win);

          if(!(c->flags & (SUB_STATE_FULL|SUB_STATE_FLOAT)))
            {
              /* Check current gravity */
              if(c->gravity != c->gravities[vid])
                {
                  subClientSetGravity(c, c->gravities[vid]);
                  c->gravities[vid] = c->gravity;

                  XMapRaised(subtle->disp, c->win);
                }

              subClientConfigure(c);

              /* EWMH: Desktop */
              subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);
            }
          else XMapRaised(subtle->disp, c->win);
        }
      else XUnmapWindow(subtle->disp, c->win); ///< Unmap other windows
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

  subtle->cv = v;

  subViewConfigure(v);

  /* EWMH: Current desktop */
  vid = subArrayIndex(subtle->views, (void *)v); ///< Get desktop number
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP, &vid, 1);

  XSetInputFocus(subtle->disp, ROOT, RevertToNone, CurrentTime);

  subViewRender();

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

      XSync(subtle->disp, False);
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

  XDeleteContext(subtle->disp, v->button, 1);
  XDestroyWindow(subtle->disp, v->button);

  free(v->name);
  free(v);          

  subSharedLogDebug("kill=view\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
