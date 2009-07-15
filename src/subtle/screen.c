
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
  int i;
  SubScreen *s = NULL;

  assert(subtle);

  s = SCREEN(subtle->screens->data[0]); 

  /* x => left, y => right, width => top, height => bottom */
  s->geom.x     = s->base.x + subtle->strut.x; ///< Only first screen
  s->geom.width = s->base.width - subtle->strut.x;

  for(i = 0; i < subtle->screens->ndata; i++)
    {
      s = SCREEN(subtle->screens->data[i]);

      /* Adjusting sizes */
      if(0 < s->base.y)
        {
          s->geom.y = s->base.y + subtle->strut.width;
          s->geom.height = s->base.height - (subtle->bottom ? subtle->th : 0) - 
            subtle->strut.height - subtle->strut.width;    
        }
      else
        {
          s->geom.y = s->base.y + (subtle->bottom ? 0 : subtle->th) + subtle->strut.width;
          s->geom.height = s->base.height - subtle->th - 
            subtle->strut.height - subtle->strut.width;    
        }
    }

  s->geom.width = s->base.width - subtle->strut.y; ///< Only last screen
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
