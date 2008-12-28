
 /**
  * @package subtle
  *
  * @file Layout functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING. 
  **/

#include "subtle.h"

 /** subLayoutNew {{{
  * @brief Create new tiling layout
  * @param[in]  c1    A #SubClient
  * @param[in]  c2    A #SubClient
  * @param[in]  mode  Tiling mode
  * @return Returns a #SubLayout or \p NULL
  **/

SubLayout *
subLayoutNew(SubClient *c1,
  SubClient *c2,
  int mode)
{
  SubLayout *l = NULL;

  assert(c1 && c2);

  l = LAYOUT(subSharedMemoryAlloc(1, sizeof(SubLayout)));
  l->c1    = c1;
  l->c2    = c2;
  l->flags = SUB_TYPE_LAYOUT|mode;

  subSharedLogDebug("new=layout, c1=%lx, c2=%lx, mode=%s\n", c1->win, c2->win,
    mode == SUB_TILE_HORZ ? "H" : (mode == SUB_TILE_VERT ? "V" : "S"));

  return l;
} /* }}} */

 /** subLayoutKill {{{
  * @brief Kill layout
  * @param[in]  l  A #SubLayout
  **/

void
subLayoutKill(SubLayout *l)
{
  assert(l);

  free(l);

  subSharedLogDebug("kill=layout\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
