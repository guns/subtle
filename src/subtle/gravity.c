
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

  subSharedLogDebug("new=gravity\n");

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

  /* Sanitize sizes */
  g->modes[g->nmodes].x      = MINMAX(g->modes[g->nmodes].x,      0, 100);
  g->modes[g->nmodes].y      = MINMAX(g->modes[g->nmodes].y,      0, 100);
  g->modes[g->nmodes].width  = MINMAX(g->modes[g->nmodes].width,  0, 100);
  g->modes[g->nmodes].height = MINMAX(g->modes[g->nmodes].height, 0, 100);

  subSharedLogDebug("new=mode, x=%d, y=%d, width=%d, height%d\n", g->modes[g->nmodes].x, 
    g->modes[g->nmodes].y, g->modes[g->nmodes].width, g->modes[g->nmodes].height);

  g->nmodes++;
} /* }}} */

 /** subGravityDelMode {{{
  * @brief Delete mode from gravtiy
  * @param[in]  g   A #SubGravity
  * @param[in]  id  Mode id
  **/

void
subGravityDelMode(SubGravity *g,
  int id)
{
  assert(g);

  if(0 <= id && g->nmodes > id)
    {
      int i;

      for(i = id; i < g->nmodes - 1; i++)
        g->modes[i] = g->modes[i - 1];

      (g->nmodes)--;
      g->modes = (XRectangle *)subSharedMemoryRealloc(g->modes, (g->nmodes) * sizeof(XRectangle));

      subSharedLogDebug("kill=mode, id=%d\n", id);
    }

} /* }}} */


 /** subGravityPublish {{{
  * @brief Publish gravities
  **/

void
subGravityPublish(void)
{
  int i, j, pos = 0, len = 9;
  char **modes = NULL, buf[30];
  SubGravity *g = NULL;

  assert(0 < subtle->gravities->ndata);

  modes = (char **)subSharedMemoryAlloc(len, sizeof(char *));

  for(i = 0; i < subtle->gravities->ndata; i++)
    {
      g = GRAVITY(subtle->gravities->data[i]);

      for(j = 0; j < g->nmodes; j++)
        {
          /* Increase size of array */
          if(pos == len)
            {
              len   *= 2;
              modes  = (char **)subSharedMemoryRealloc(modes, len * sizeof(char *));
            }
          
          /* Add mode to list */
          snprintf(buf, sizeof(buf), "%d#%dx%d+%d+%d", i + 1, g->modes[j].x,  
            g->modes[j].y, g->modes[j].width, g->modes[j].height);

          modes[pos] = (char *)subSharedMemoryAlloc(strlen(buf) + 1, sizeof(char));
          strncpy(modes[pos], buf, strlen(buf));
          pos++;
        }
    }

  /* EWMH: Gravity list */
  subEwmhSetStrings(ROOT, SUB_EWMH_SUBTLE_GRAVITY_LIST, modes, pos);

  subSharedLogDebug("publish=gravities, n=%d\n", pos);

  /* Tidy up */
  for(i = 0; i < pos; i++)
    free(modes[i]);

  free(modes);
} /* }}} */

 /** subGravityClear {{{
  * @brief Delete all gravity modes
  * @param[in]  g  A #SubGravity
  **/

void
subGravityClear(SubGravity *g)
{
  assert(g);

  free(g->modes);

  g->modes  = NULL;
  g->nmodes = 0;
} /* }}} */

 /** subGravityKill {{{
  * @brief Kill gravity
  * @param[in]  g  A #SubGravity
  **/

void
subGravityKill(SubGravity *g)
{
  assert(g);

  subGravityClear(g);

  free(g);
} /* }}} */
