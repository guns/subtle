
 /**
  * @package subtle
  *
  * @file Gravity functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subGravityInit {{{
  * @brief Init gravities
  **/

void
subGravityInit(void)
{
  int i;

  /* Create gravities */
  for(i = 0; i < 9; i++)
    {
      SubGravity *g = subGravityNew();

      subArrayPush(subtle->gravities, (void *)g);
    }
} /* }}} */

 /** subGravityNew {{{
  * @brief Create new gravity
  * @return Returns a new #SubGravity or \p NULL
  **/

SubGravity *
subGravityNew(void)
{
  SubGravity *g = GRAVITY(subSharedMemoryAlloc(1, sizeof(SubGravity)));
  g->flags |= SUB_TYPE_GRAVITY;

  return g;
} /* }}} */

 /** subGravityAddMode {{{
  * @brief Add mode to gravtiy
  * @param[in]  g     A #SubGravity
  * @param[in]  mode  The new mode
  **/

void
subGravityAddMode(SubGravity *g,
  XRectangle *mode)
{
  assert(g && mode);

  /* Add mode */
  g->modes = (XRectangle *)subSharedMemoryRealloc(g->modes, (g->nmodes + 1) * sizeof(XRectangle));
  g->modes[g->nmodes] = *mode;
 
  g->nmodes++;

  printf("Mode: x=%d, y=%d, width=%d, height%d\n", 
    mode->x, mode->y, mode->width, mode->height);
} /* }}} */

 /** subGravityKill {{{
  * @brief Kill gravity
  * @param[in]  g  A #SubGravity
  **/

void
subGravityKill(SubGravity *g)
{
  assert(g);

  free(g);
} /* }}} */
