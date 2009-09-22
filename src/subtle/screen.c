
 /**
  * @package subtle
  *
  * @file Display functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

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

 /** subScreenUpdate {{{
  * @brief Update screens
  **/

void
subScreenUpdate(void)
{
  assert(subtle);

  /* x => left, y => right, width => top, height => bottom */
  subtle->screen->geom.x      = subtle->screen->base.x + subtle->strut.x;
  subtle->screen->geom.y      = subtle->screen->base.y + subtle->strut.width;
  subtle->screen->geom.width  = subtle->screen->base.width - subtle->strut.x;
  subtle->screen->geom.height = subtle->screen->base.height - subtle->strut.height - 
    subtle->strut.width;    

  /* Add panel heights */
  if(subtle->flags & SUB_SUBTLE_PANEL1)
    {
      subtle->screen->geom.y      += subtle->th;
      subtle->screen->geom.height -= subtle->th;
    }

  if(subtle->flags & SUB_SUBTLE_PANEL2)
    subtle->screen->geom.height -= subtle->th;
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
} /* }}} */

 /** SubScreenLimit {{{
  * @brief Limit a rect to in screen boundaries
  * @param[in]  s  A #SubScreem
  * @param[in]  r  A XRectangle
  **/

void
subScreenLimit(SubScreen *s,
  XRectangle *r)
{
  assert(s && r);

  /* Check boundaries */
  if(r->x < s->geom.x || r->x > s->geom.x + s->geom.width)
    r->x = s->geom.x;

  if(r->y < s->geom.y || r->y > s->geom.y + s->geom.height)
    r->y = s->geom.y;

  /* Check sizes */
  if(r->x + r->width > s->geom.x + s->geom.width)
    r->width = s->geom.x + s->geom.width - r->x;

  if(r->y + r->height > s->geom.y + s->geom.height)
    r->height = s->geom.y + s->geom.height - r->y;
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
