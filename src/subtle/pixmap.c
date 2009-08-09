
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

 /** subPixmapNew {{{
  * @brief Create new pixmap
  * @param[in]  path  Path to pixmap
  **/

SubPixmap *
subPixmapNew(const char *path)
{
  int hotx = 0, hoty = 0;
  SubPixmap *p = NULL;

  assert(path);

  /* Create pixmap */
  p = PIXMAP(subSharedMemoryAlloc(1, sizeof(SubPixmap)));
  p->flags  = SUB_TYPE_PIXMAP;
  p->quark  = XrmStringToQuark(path); ///< Create hash

  /* Reading pixmap file */
  if(BitmapSuccess != XReadBitmapFile(subtle->dpy, ROOT, path, 
    &p->width, &p->height, &p->pixmap, &hotx, &hoty))
    {
      subSharedLogWarn("Failed reading pixmap `%s'\n", path);

      free(p);

      return NULL;
    }

  subSharedLogDebug("new=pixmap, path=%s, width=%03d, height=%03d, quark=%d, th=%d\n",
    path, p->width, p->height, p->quark, subtle->th);

  return p;
} /* }}} */

 /** subPixmapRender {{{
  * @brief Render pixmap
  * @param[in]  p    A #SubPixmap
  * @param[in]  win  Target window
  * @param[in]  x    X position
  * @param[in]  y    Y position
  * @param[in]  fg   Foreground color
  * @param[in]  bg   Background color
  **/

void
subPixmapRender(SubPixmap *p,
  Window win,
  int x,
  int y,
  long fg,
  long bg)
{
  XGCValues gvals;

  assert(p);

  /* Plane color */
  gvals.foreground = fg;
  gvals.background = bg;
  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground|GCBackground, &gvals);

  XCopyPlane(subtle->dpy, p->pixmap, win, subtle->gcs.font, 0, 0, p->width, 
    subtle->th, x, y, 1);
} /* }}} */

 /** subPixmapFind {{{
  * @brief Find pixmap
  * @param[in]  path  description
  *  * @retval  -1  Not found
  * @retval  >0  Found id
  **/

int
subPixmapFind(const char *path)
{
  int found = -1;

  assert(path);

  if(0 < subtle->pixmaps->ndata)
    {
      int i, quark = XrmStringToQuark(path);

      for(i = 0; i < subtle->pixmaps->ndata; i++)
        {
          SubPixmap *p = PIXMAP(subtle->pixmaps->data[i]);

          /* Compare quarks */
          if(p->quark == quark)
            {
              found = i;
              break;
            }
        }
    }

  return found;
} /* }}} */

 /** subPixmapKill {{{
  * @brief Kill pixmap
  * @param[in]  p  A #SubPixmap
  **/

void
subPixmapKill(SubPixmap *p)
{
  assert(p);

  XFreePixmap(subtle->dpy, p->pixmap);
  free(p);

  subSharedLogDebug("kill=pixmap\n");
} /* }}} */
