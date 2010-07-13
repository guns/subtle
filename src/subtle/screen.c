
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
} /* }}} */

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

  /* Create screen */
  s = SCREEN(subSharedMemoryAlloc(1, sizeof(SubScreen)));
  s->flags       = SUB_TYPE_SCREEN;
  s->geom.x      = x;
  s->geom.y      = y;
  s->geom.width  = width;
  s->geom.height = height;
  s->base        = s->geom; ///< Backup size

  subSharedLogDebug("new=screen, x=%d, y=%d, width=%u, height=%u\n",
    s->geom.x, s->geom.y, s->geom.width, s->geom.height);

  return s;
} /* }}} */

 /** subScreenConfigure {{{
  * @brief Configure screens
  **/

void
subScreenConfigure(void)
{
  assert(subtle);

  /* x => left, y => right, width => top, height => bottom */
  DEFSCREEN->geom.x      = DEFSCREEN->base.x + subtle->strut.x;
  DEFSCREEN->geom.y      = DEFSCREEN->base.y + subtle->strut.width;
  DEFSCREEN->geom.width  = DEFSCREEN->base.width - subtle->strut.x;
  DEFSCREEN->geom.height = DEFSCREEN->base.height - subtle->strut.height -
    subtle->strut.width;

  /* Add panel heights */
  if(subtle->flags & SUB_SUBTLE_PANEL1)
    {
      DEFSCREEN->geom.y      += subtle->th;
      DEFSCREEN->geom.height -= subtle->th;
    }

  if(subtle->flags & SUB_SUBTLE_PANEL2)
    DEFSCREEN->geom.height -= subtle->th;

  subScreenPublish();
} /* }}} */

 /** subScreenResize {{{
  * @brief Resize screens
  **/

void
subScreenResize(void)
{
  /* Update screens */
  subArrayClear(subtle->screens, True);
  subScreenInit();
  subScreenConfigure();

  /* Update panels */
  XMoveResizeWindow(subtle->dpy, subtle->windows.panel1, DEFSCREEN->base.x,
    DEFSCREEN->base.y, DEFSCREEN->geom.width, subtle->th);
  XMoveResizeWindow(subtle->dpy, subtle->windows.panel2, DEFSCREEN->base.x,
    DEFSCREEN->base.height - subtle->th, DEFSCREEN->geom.width, subtle->th);
  subPanelUpdate();

  /* Update positions */
  subViewConfigure(CURVIEW, True);
} /* }}} */

 /** subScreenJump {{{
  * @brief Jump to screen
  * @param[in]  s  A #SubScreen
  **/

void
subScreenJump(SubScreen *s)
{
  assert(s);

  /* Store screen */
  subtle->sid = subArrayIndex(subtle->screens, (void *)s);

  XWarpPointer(subtle->dpy, None, ROOT, 0, 0, s->geom.x, s->geom.y,
    s->geom.x + s->geom.width / 2, s->geom.y + s->geom.height / 2);

  subSubtleFocus(True);
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

  free(s);

  subSharedLogDebug("kill=screen\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
