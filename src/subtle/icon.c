
 /**
  * @package subtle
  *
  * @file Ruby functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <X11/Xresource.h>
#include "subtle.h"

 /** subIconNew {{{
  * @brief Create new icon
  * @param[in]  path  Path to icon
  **/

SubIcon *
subIconNew(const char *path)
{
  int hotx = 0, hoty = 0;
  SubIcon *i = NULL;

  assert(path);

  /* Create icon */
  i = ICON(subSharedMemoryAlloc(1, sizeof(SubIcon)));
  i->flags = SUB_TYPE_ICON;
  i->quark = XrmStringToQuark(path); ///< Create hash

  /* Reading icon file */
  if(BitmapSuccess != XReadBitmapFile(subtle->dpy, ROOT, path, 
    &i->width, &i->height, &i->pixmap, &hotx, &hoty))
    {
      subSharedLogWarn("Failed reading icon `%s'\n", path);

      free(i);

      return NULL;
    }

  subSharedLogDebug("new=icon, path=%s, width=%03d, height=%03d, quark=%d, th=%d\n",
    path, i->width, i->height, i->quark, subtle->th);

  return i;
} /* }}} */

 /** subIconRender {{{
  * @brief Render icon
  * @param[in]  i    A #SubIcon
  * @param[in]  win  Target window
  * @param[in]  x    X position
  * @param[in]  y    Y position
  * @param[in]  fg   Foreground color
  * @param[in]  bg   Background color
  **/

void
subIconRender(SubIcon *i,
  Window win,
  int x,
  int y,
  long fg,
  long bg)
{
  XGCValues gvals;

  assert(i);

  /* Plane color */
  gvals.foreground = fg;
  gvals.background = bg;
  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground|GCBackground, &gvals);

  XCopyPlane(subtle->dpy, i->pixmap, win, subtle->gcs.font, 0, 0, i->width, 
    subtle->th, x, y, 1);
} /* }}} */

 /** subIconFind {{{
  * @brief Find icon id
  * @param[in]  path  Path to icon
  * @retval  -1  Not found
  * @retval  >0  Found id
  **/

int
subIconFind(const char *path)
{
  int found = -1;

  assert(path);

  if(0 < subtle->icons->ndata)
    {
      int j, quark = XrmStringToQuark(path);

      for(j = 0; j < subtle->icons->ndata; j++)
        {
          SubIcon *i = ICON(subtle->icons->data[j]);

          /* Compare quarks */
          if(i->quark == quark)
            {
              found = j;
              break;
            }
        }
    }

  return found;
} /* }}} */

 /** subIconKill {{{
  * @brief Kill icon
  * @param[in]  i  A #SubIcon
  **/

void
subIconKill(SubIcon *i)
{
  assert(i);

  XFreePixmap(subtle->dpy, i->pixmap);
  if(0 != i->gc) XFreeGC(subtle->dpy, i->gc);
  free(i);

  subSharedLogDebug("kill=icon\n");
} /* }}} */
