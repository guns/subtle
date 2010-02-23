
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

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

 /** subScreenInit {{{
  * @brief Init screens
  **/

void
subScreenInit(void)
{
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int event = 0, junk = 0;

  if(XineramaQueryExtension(subtle->dpy, &event, &junk) &&
      XineramaIsActive(subtle->dpy))
    {
      int i, n = 0;
      XineramaScreenInfo *screens = NULL;
      SubScreen *s = NULL; 

      /* Query screens */
      if((screens = XineramaQueryScreens(subtle->dpy, &n)))
        {
          for(i = 0; i < n; i++)
            {
              if((s = subScreenNew(screens[i].x_org, screens[i].y_org,
                  screens[i].width, screens[i].height)))
                subArrayPush(subtle->screens, (void *)s);
            }

          subtle->flags |= SUB_SUBTLE_XINERAMA;

          XFree(screens);
        }
    } 
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Create default screen */
  if(0 == subtle->screens->ndata)
    subArrayPush(subtle->screens, (void *)subScreenNew(0, 0, SCREENW, SCREENH));
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
  * @brief Update screens
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

  subSharedFocus();
} /* }}} */

 /** SubScreenFit {{{
  * @brief Fit a rect to in screen boundaries
  * @param[in]  s       A #SubScreen
  * @param[in]  r       A XRectangle
  * @param[in]  center  Center rect
  **/

void
subScreenFit(SubScreen *s,
  XRectangle *r,
  int center)
{
  assert(s && r);

  /* Check position */
  if(r->x <= s->geom.x) r->x = s->geom.x;
  if(r->y <= s->geom.y) r->y = s->geom.y;

  /* Check sizes and boundaries */
  if(r->x + r->width > s->geom.x + s->geom.width)
    {
      if(r->width > s->geom.width) r->width = s->geom.width;
      r->x = s->geom.x + s->geom.width - r->width;
    }

  if(r->y + r->height > s->geom.y + s->geom.height)
    {
      if(r->height > s->geom.height) r->height = s->geom.height;
      r->y = s->geom.y + s->geom.height - r->height;
    }

  /* Center */
  if(center)
    {
      if(r->x == s->geom.x)
        r->x = s->geom.x + (s->geom.width  - r->width  - 2 * subtle->bw) / 2;
      if(r->y == s->geom.y)
        r->y = s->geom.y + (s->geom.height - r->height - 2 * subtle->bw) / 2;
    }
  
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
