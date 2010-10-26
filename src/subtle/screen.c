
 /**
  * @package subtle
  *
  * @file Display functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subScreenInit {{{
  * @brief Init screens
  **/

void
subScreenInit(void)
{
  SubScreen *s = NULL;

#if defined HAVE_X11_EXTENSIONS_XINERAMA_H || defined HAVE_X11_EXTENSIONS_XRANDR_H

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  if(subtle->flags & SUB_SUBTLE_XINERAMA && XineramaIsActive(subtle->dpy))
    {
      int i, n = 0;
      XineramaScreenInfo *info = NULL;

      /* Query screens */
      if((info = XineramaQueryScreens(subtle->dpy, &n)))
        {
#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
          XRRScreenResources *res = NULL;

          res = XRRGetScreenResourcesCurrent(subtle->dpy, ROOT);

          /* Check if we have xrandr and if it knows more screens */
          if(subtle->flags & SUB_SUBTLE_XRANDR && res && res->ncrtc >= n)
            {
              XRRCrtcInfo *crtc = NULL;

              for(i = 0; i < res->ncrtc; i++)
                {
                  if((crtc = XRRGetCrtcInfo(subtle->dpy, res, res->crtcs[i])))
                    {
                      /* Create new screen if crtc is enabled */
                      if(None != crtc->mode && (s = subScreenNew(crtc->x,
                          crtc->y, crtc->width, crtc->height)))
                        subArrayPush(subtle->screens, (void *)s);

                      XRRFreeCrtcInfo(crtc);
                    }
                }

              XRRFreeScreenResources(res);
            }
          else
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */
            {
              for(i = 0; i < n; i++)
                {
                  /* Create new screen */
                  if((s = subScreenNew(info[i].x_org, info[i].y_org,
                      info[i].width, info[i].height)))
                    subArrayPush(subtle->screens, (void *)s);
                }
            }

          XFree(info);
        }
    }
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Create default screen */
  if(0 == subtle->screens->ndata)
    {
      /* Create new screen */
      if((s = subScreenNew(0, 0, SCREENW, SCREENH)))
        subArrayPush(subtle->screens, (void *)s);
    }

  printf("Runnning on %d screen(s)\n", subtle->screens->ndata);
} /* }}} ^*/

 /** subScreenNew {{{
  * @brief Create a new view
  * @param[in]  x       X position of screen
  * @param[in]  y       y position of screen
  * @param[in]  width   Width of screen
  * @param[in]  height  Height of screen
  * @return Returns a #SubScreen or \p NULL
  **/

SubScreen *
subScreenNew(int x,
  int y,
  unsigned int width,
  unsigned int height)
{
  SubScreen *s = NULL;
  XSetWindowAttributes sattrs;
  unsigned long mask = 0;

  /* Create screen */
  s = SCREEN(subSharedMemoryAlloc(1, sizeof(SubScreen)));
  s->flags       = SUB_TYPE_SCREEN;
  s->geom.x      = x;
  s->geom.y      = y;
  s->geom.width  = width;
  s->geom.height = height;
  s->base        = s->geom; ///< Backup size
  s->vid         = subtle->screens->ndata; ///< Init

  /* Create panels */
  sattrs.event_mask        = ButtonPressMask|ExposureMask;
  sattrs.override_redirect = True;
  sattrs.background_pixel  = subtle->colors.bg_panel;
  mask                     = CWEventMask|CWOverrideRedirect|CWBackPixel;

  s->panel1    = XCreateWindow(subtle->dpy, ROOT, 0, 1, 1, 1, 0,
    CopyFromParent, InputOutput, CopyFromParent, mask, &sattrs);
  s->panel2    = XCreateWindow(subtle->dpy, ROOT, 0, 0, 1, 1, 0,
    CopyFromParent, InputOutput, CopyFromParent, mask, &sattrs);

  subSharedLogDebug("new=screen, x=%d, y=%d, width=%u, height=%u\n",
    s->geom.x, s->geom.y, s->geom.width, s->geom.height);

  return s;
} /* }}} */

 /** subScreenFind {{{
  * @brief Find screen by coordinates
  * @param[in]     x    X coordinate
  * @param[in]     y    Y coordinate
  * @param[inout]  sid  Screen id
  * @return Returns a #SubScreen
  **/

SubScreen *
subScreenFind(int x,
  int y,
  int *sid)
{
  int i;
  SubScreen *ret = SCREEN(subtle->screens->data[0]);

  /* Check screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      /* Check if coordinates are in screen rects */
      if((x >= s->base.x && x < s->base.x + s->base.width) &&
          (y >= s->base.y && y < s->base.y + s->base.height))
        {
          ret = s;
          if(sid) *sid = i;

          break;
        }
    }

  return ret;
} /* }}} */

 /** subScreenCurrent {{{
  * @brief Find screen by coordinates
  * @param[inout]  sid  Screen id
  * @return Current #SubScreen or \p NULL
  **/

SubScreen *
subScreenCurrent(int *sid)
{
  SubScreen *ret = NULL;

  /* Check if there is only one screen */
  if(1 == subtle->screens->ndata)
    {
      if(sid) *sid = 0;

      ret = SCREEN(subtle->screens->data[0]);
    }
  else
    {
      int x = 0, y = 0;
      Window dummy = None;

      /* Get current screen */
      XQueryPointer(subtle->dpy, ROOT, &dummy, &dummy,
        &x, &y, (int *)&dummy, (int *)&dummy, (unsigned int *)&dummy);

      ret = subScreenFind(x, y, sid);
    }

  return ret;
} /* }}} */

 /** subScreenConfigure {{{
  * @brief Configure screens
  **/

void
subScreenConfigure(void)
{
  int i, j;
  SubScreen *s = NULL;
  SubView *v = NULL;

  /* Reset visible tags and views */
  subtle->visible_tags  = 0;
  subtle->visible_views = 0;

  /* Check each client */
  for(i = 0; i < subtle->clients->ndata; i++)
    {
      SubClient *c = CLIENT(subtle->clients->data[i]);
      int visible = 0;

      /* Check views of each screen */
      for(j = 0; j < subtle->screens->ndata; j++)
        {
          s = SCREEN(subtle->screens->data[j]);
          v = VIEW(subtle->views->data[s->vid]);

          /* Set visible tags and views to ease lookups */
          subtle->visible_tags  |= v->tags;
          subtle->visible_views |= (1L << (s->vid + 1));

          /* Find visible clients */
          if(VISIBLE(v->tags, c))
            {
              subClientSetGravity(c, c->gravities[s->vid], j, False);

              /* EWMH: Desktop */
              subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP,
                (long *)&s->vid, 1);

              visible++;
            }
        }

      /* Check visibility */
      if(0 < visible)
        {
          /* Wait until all screens are checked */
          subClientConfigure(c);

          /* Special treatment */
          if(c->flags & (SUB_CLIENT_MODE_FULL|SUB_CLIENT_MODE_FLOAT))
            XMapRaised(subtle->dpy, c->win);
          else XMapWindow(subtle->dpy, c->win);

          /* Warp after gravity and screen is set */
          if(c->flags & SUB_CLIENT_MODE_URGENT)
            subClientWarp(c);
        }
      else ///< Unmap other windows
        {
          c->flags |= SUB_CLIENT_UNMAP; ///< Ignore next unmap
          XUnmapWindow(subtle->dpy, c->win);
        }
    }

  subSharedLogDebug("Configure: type=screen\n");

  /* Hook: Configure */
  subHookCall(SUB_HOOK_VIEW_CONFIGURE, (void *)v);
} /* }}} */

 /** subScreenUpdate {{{
  * @brief Update screens
  **/

void
subScreenUpdate(void)
{
  int i;

  /* Update screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);
      SubPanel *p = NULL;
      int j, npanel = 0, center = False, offset = 0;
      int x[4] = { 0 }, nspacer[4] = { 0 }; ///< Waste four ints but it's easier for the algo
      int sw[4] = { 0 }, fix[4] = { 0 }, width[4] = { 0 }, spacer[4] = { 0 };

      /* Pass 1: Collect width for spacer sizes */
      for(j = 0; s->panels && j < s->panels->ndata; j++)
        {
          p = PANEL(s->panels->data[j]);

          subPanelUpdate(p);

          /* Check flags */
          if(p->flags & SUB_PANEL_HIDDEN)  continue;
          if(p->flags & SUB_PANEL_BOTTOM)
            {
              npanel = 1;
              center = False;
            }
          if(p->flags & SUB_PANEL_CENTER) center = !center;

          /* Offset selects panel variables for either center or not */
          offset = center ? npanel + 2 : npanel;

          if(p->flags & SUB_PANEL_SPACER1) spacer[offset]++;
          if(p->flags & SUB_PANEL_SPACER2) spacer[offset]++;
          if(p->flags & SUB_PANEL_SEPARATOR1)
            width[offset] += subtle->separator.width;
          if(p->flags & SUB_PANEL_SEPARATOR2)
            width[offset] += subtle->separator.width;

          width[offset] += p->width;
        }

      /* Calculate spacer and fix sizes */
      for(j = 0; j < 4; j++)
        {
          if(0 < spacer[j])
            {
              sw[j]  = (s->base.width - width[j]) / spacer[j];
              fix[j] = s->base.width - (width[j] + spacer[j] * sw[j]);
            }
        }

      /* Pass 2: Move and resize windows */
      for(j = 0, npanel = 0, center = False; s->panels && j < s->panels->ndata; j++)
        {
          p = PANEL(s->panels->data[j]);

          /* Check flags */
          if(p->flags & SUB_PANEL_HIDDEN) continue;
          if(p->flags & SUB_PANEL_BOTTOM)
            {
              /* Reset for new panel */
              npanel     = 1;
              nspacer[0] = 0;
              nspacer[2] = 0;
              x[0]       = 0;
              x[2]       = 0;
              center     = False;
            }
          if(p->flags & SUB_PANEL_CENTER) center = !center;

          /* Offset selects panel variables for either center or not */
          offset = center ? npanel + 2 : npanel;

          /* Set start position of centered panel items */
          if(center && 0 == x[offset])
            x[offset] = (s->base.width - width[offset]) / 2;

          /* Add separator before panel item */
          if(p->flags & SUB_PANEL_SEPARATOR1)
            x[offset] += subtle->separator.width;

          /* Add spacer before item */
          if(p->flags & SUB_PANEL_SPACER1)
            {
              x[offset] += sw[offset];

              /* Increase last spacer size by rounding fix */
              if(++nspacer[offset] == spacer[offset])
                x[offset] += fix[offset];
            }

          /* Set window position */
          XMoveWindow(subtle->dpy, p->win, x[offset], 0);
          p->x = x[offset];

          /* Add separator after panel item */
          if(p->flags & SUB_PANEL_SEPARATOR2)
            x[offset] += subtle->separator.width;

          /* Add spacer after item */
          if(p->flags & SUB_PANEL_SPACER2)
            {
              x[offset] += sw[offset];

              /* Increase last spacer size by rounding fix */
              if(++nspacer[offset] == spacer[offset])
                x[offset] += fix[offset];
            }

          x[offset] += p->width;

          /* Remap window only when needed */
          if(0 < p->width) XMapWindow(subtle->dpy, p->win);
          else XUnmapWindow(subtle->dpy, p->win);
        }
    }
} /* }}} */

 /** subScreenRender {{{
  * @brief Render screens
  **/

void
subScreenRender(void)
{
  int i;

  /* Render all screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      int j;
      SubScreen *s = SCREEN(subtle->screens->data[i]);
      Window panel = s->panel1;

      XClearWindow(subtle->dpy, s->panel1);
      XClearWindow(subtle->dpy, s->panel2);

      /* Draw stipple on panels */
      if(s->flags & SUB_SCREEN_STIPPLE)
        {
          XFillRectangle(subtle->dpy, s->panel1, subtle->gcs.stipple,
            0, 2, s->base.width, subtle->th - 4);
          XFillRectangle(subtle->dpy, s->panel2, subtle->gcs.stipple,
            0, 2, s->base.width, subtle->th - 4);
        }

      /* Render panel items */
      for(j = 0; s->panels && j < s->panels->ndata; j++)
        {
          SubPanel *p = PANEL(s->panels->data[j]);

          if(p->flags & SUB_PANEL_HIDDEN) continue;
          if(p->flags & SUB_PANEL_BOTTOM) panel = s->panel2;
          if(p->flags & SUB_PANEL_SEPARATOR1) ///< Draw separator before panel
            subSharedTextDraw(subtle->dpy, subtle->gcs.font, subtle->font,
              panel, p->x - subtle->separator.width + 3,
              subtle->font->y + subtle->pbw, subtle->colors.separator, -1,
              subtle->separator.string);

          subPanelRender(p);

          if(p->flags & SUB_PANEL_SEPARATOR2) ///< Draw separator after panel
            subSharedTextDraw(subtle->dpy, subtle->gcs.font, subtle->font,
              panel, p->x + p->width + 3, subtle->font->y + subtle->pbw,
              subtle->colors.separator, -1, subtle->separator.string);
        }
    }

  XSync(subtle->dpy, False); ///< Sync before going on
} /* }}} */

 /** subScreenResize {{{
  * @brief Resize screens
  **/

void
subScreenResize(void)
{
  int i;

  /* Update screens */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      /* Add strut: x => left, y => right, width => top, height => bottom */
      s->geom.x      = s->base.x + subtle->strut.x;
      s->geom.y      = s->base.y + subtle->strut.width;
      s->geom.width  = s->base.width - subtle->strut.x;
      s->geom.height = s->base.height - subtle->strut.height -
        subtle->strut.width;

      /* Update panels */
      if(s->flags & SUB_SCREEN_PANEL1)
        {
          XMoveResizeWindow(subtle->dpy, s->panel1, s->base.x, s->base.y,
            s->base.width, subtle->th);
          XSetWindowBackground(subtle->dpy, s->panel1,
            subtle->colors.bg_panel);
          XMapRaised(subtle->dpy, s->panel1);

          /* Update height */
          s->geom.y      += subtle->th;
          s->geom.height -= subtle->th;
        }
      else XUnmapWindow(subtle->dpy, s->panel1);

      if(s->flags & SUB_SCREEN_PANEL2)
        {
          XMoveResizeWindow(subtle->dpy, s->panel2, s->base.x,
            s->base.y + s->base.height - subtle->th, s->base.width, subtle->th);
          XSetWindowBackground(subtle->dpy, s->panel2,
            subtle->colors.bg_panel);
          XMapRaised(subtle->dpy, s->panel2);

          /* Update height */
          s->geom.height -= subtle->th;
        }
      else XUnmapWindow(subtle->dpy, s->panel2);
    }

  subScreenPublish();
} /* }}} */

 /** subScreenJump {{{
  * @brief Jump to screen
  * @param[in]  s  A #SubScreen
  **/

void
subScreenJump(SubScreen *s)
{
  assert(s);

  XWarpPointer(subtle->dpy, None, ROOT, 0, 0, s->geom.x, s->geom.y,
    s->geom.x + s->geom.width / 2, s->geom.y + s->geom.height / 2);

  subSubtleFocus(True);

  subSharedLogDebug("Jump: type=screen\n");
} /* }}} */

 /** SubScreenFit {{{
  * @brief Fit a rect to in screen boundaries
  * @param[in]  s  A #SubScreen
  * @param[in]  r  A XRectangle
  **/

void
subScreenFit(SubScreen *s,
  XRectangle *r)
{
  int maxx, maxy;

  assert(s && r);

  /* Check size */
  if(r->width > s->geom.width)   r->width  = s->geom.width;
  if(r->height > s->geom.height) r->height = s->geom.height;

  /* Check position */
  if(r->x < s->geom.x) r->x = s->geom.x;
  if(r->y < s->geom.y) r->y = s->geom.y;

  /* Check width and height */
  maxx = s->geom.x + s->geom.width;
  maxy = s->geom.y + s->geom.height;

  if(r->x > maxx || r->x + r->width > maxx)  r->x = s->geom.x;
  if(r->y > maxy || r->y + r->height > maxy) r->y = s->geom.y;
} /* }}} */

 /** subScreenPublish {{{
  * @brief Publish screens
  **/

void
subScreenPublish(void)
{
  int i;
  long *workareas = NULL, *viewports = NULL;

  assert(subtle);

  /* EWMH: Workarea size of every desktop */
  workareas = (long *)subSharedMemoryAlloc(4 * subtle->screens->ndata,
    sizeof(long));

  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      workareas[i * 4 + 0] = s->geom.x;
      workareas[i * 4 + 1] = s->geom.y;
      workareas[i * 4 + 2] = s->geom.width;
      workareas[i * 4 + 3] = s->geom.height;
    }

  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_WORKAREA, workareas,
    4 * subtle->screens->ndata);

  /* EWMH: Desktop viewport */
  viewports = (long *)subSharedMemoryAlloc(2 * subtle->screens->ndata,
    sizeof(long)); ///< Calloc inits with zero - great

  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_DESKTOP_VIEWPORT, viewports,
    2 * subtle->screens->ndata);

  free(workareas);
  free(viewports);
} /* }}} */

 /** SubScreenKill {{{
  * @brief Kill a screen
  * @param[in]  s  A #SubScreem
  **/

void
subScreenKill(SubScreen *s)
{
  assert(s);

  /* Destroy windows */
  if(s->panel1)
    {
      XDestroySubwindows(subtle->dpy, s->panel1);
      XDestroyWindow(subtle->dpy, s->panel1);
    }

  if(s->panel2)
    {
      XDestroySubwindows(subtle->dpy, s->panel2);
      XDestroyWindow(subtle->dpy, s->panel2);
    }

  if(s->panels) subArrayKill(s->panels, True);

  free(s);

  subSharedLogDebug("kill=screen\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
