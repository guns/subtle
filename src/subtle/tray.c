
 /**
  * @package subtle
  *
  * @file Tray functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

/* Typedef {{{ */
typedef struct xembedinfo_t
{
  CARD32 version, flags;
} XEmbedInfo; /* }}} */

 /** subTrayNew {{{
  * @brief Create new tray
  * @param[in]  win  Tray window
  * @return Returns a new #SubTray or \p NULL
  **/

SubTray *
subTrayNew(Window win)
{
  SubTray *t = NULL;
  XEmbedInfo *info = NULL;

  assert(win);

  /* Check xembed data */
  if((info = (XEmbedInfo *)subEwmhGetProperty(win, subEwmhGet(SUB_EWMH_XEMBED_INFO), 
    SUB_EWMH_XEMBED_INFO, NULL)))
    {
      t = TRAY(subUtilAlloc(1, sizeof(SubTray)));
      t->flags = SUB_TYPE_TRAY;
      t->win   = win;
      t->width = subtle->th; ///< Default width

      /* Fetch name */
      XFetchName(subtle->disp, t->win, &t->name);
      if(!t->name) t->name = strdup(PKG_NAME);

      /* Update tray window */
      XAddToSaveSet(subtle->disp, t->win);
      XSelectInput(subtle->disp, t->win, StructureNotifyMask|PropertyChangeMask);
      XReparentWindow(subtle->disp, t->win, subtle->windows.tray, 0, 0);
      XSaveContext(subtle->disp, t->win, TRAYID, (void *)t);

      subEwmhMessage(t->win, t->win, SUB_EWMH_XEMBED, CurrentTime, XEMBED_EMBEDDED_NOTIFY,
        0, subtle->windows.tray, 0);

      subTraySetState(t); ///< @todo Overhead for getting property twice

      printf("Adding tray (%s)\n", t->name);
      subUtilLogDebug("new=tray, name=%s, win=%#lx\n", t->name, win);

      free(info);
    }

  return t;
} /* }}} */

 /** subTrayUpdate {{{
  * @brief Update tray bar
  **/

void
subTrayUpdate(void)
{
  if(0 < subtle->trays->ndata)
    {
      int i, width = 0;
      XWindowAttributes attrs;

      XGetWindowAttributes(subtle->disp, subtle->windows.sublets, &attrs);

      /* Resize every tray client */
      for(i = 0; i < subtle->trays->ndata; i++)
        {
          SubTray *t = TRAY(subtle->trays->data[i]);

          XMoveResizeWindow(subtle->disp, t->win, width, 0, t->width, subtle->th);
          width += t->width;
        }

      XMoveResizeWindow(subtle->disp, subtle->windows.tray, attrs.x - width, 0, width, subtle->th);
    }
} /* }}} */

 /** subTraySelect {{{
  * @brief Get tray selection for screen
  **/

void
subTraySelect(void)
{
  Atom sel = subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION);

  /* Tray selection */
  XSetSelectionOwner(subtle->disp, sel, subtle->windows.tray, CurrentTime);
  if(XGetSelectionOwner(subtle->disp, sel) == subtle->windows.tray)
    {
      subUtilLogDebug("Selection: type=%ld\n", sel);
    }
  else subUtilLogError("Failed to get tray selection\n");

  /* Send manager info */
  subEwmhMessage(ROOT, ROOT, SUB_EWMH_MANAGER, CurrentTime, 
    subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION), subtle->windows.tray, 0, 0);
} /* }}} */

 /** subTraySetState {{{
  * @brief Set window state and map/unmap accordingly
  * @param[in]  t  A #SubTray
  **/

void
subTraySetState(SubTray *t)
{
  XEmbedInfo *info = NULL;

  assert(t);

  /* Get xembed data */
  if((info = (XEmbedInfo *)subEwmhGetProperty(t->win, subEwmhGet(SUB_EWMH_XEMBED_INFO), 
    SUB_EWMH_XEMBED_INFO, NULL)))
    {
       if(info->flags & XEMBED_MAPPED) ///< Map if wanted
        {
          subTrayGetSize(t);
          XMapRaised(subtle->disp, t->win); 
        }
      else XUnmapWindow(subtle->disp, t->win);

      printf("XEmbedInfo: version=%ld, flags=%ld, mapped=%ld\n", info->version, info->flags,
        info->flags & XEMBED_MAPPED);
    }
  XFree(info);
} /* }}} */

 /** subTrayGetSize {{{
  * @brief Get window size
  * @param[in]  t  A #SubTray
  **/

void
subTrayGetSize(SubTray *t)
{
  long supplied = 0;
  XSizeHints *hints = NULL;

  assert(t);
  
  hints = XAllocSizeHints();
  if(!hints) subUtilLogError("Can't alloc memory. Exhausted?\n");

  XGetWMNormalHints(subtle->disp, t->win, hints, &supplied);
  if(0 < supplied)
    {
      if(hints->flags & (USSize|PSize)) ///< User/program size
        t->width = MINMAX(hints->width, subtle->th, subtle->th * 2);
      else if(hints->flags & PBaseSize) ///< Base size
        t->width = MINMAX(hints->base_width, subtle->th, subtle->th * 2);
      else if(hints->flags & PMinSize) ///< Min size
        t->width = MINMAX(hints->min_width, subtle->th, subtle->th * 2);

    }
  XFree(hints);

  printf("Tray: width=%d, supplied=%ld\n", t->width, supplied);
} /* }}} */

 /** subTrayKill {{{
  * @brief Kill tray
  * @param[in]  t  A #SubTray
  **/

void
subTrayKill(SubTray *t)
{
  assert(t);

  printf("Killing tray (%s)\n", t->name);

  /* Ignore further events and delete context */
  XSelectInput(subtle->disp, t->win, NoEventMask);
  XDeleteContext(subtle->disp, t->win, TRAYID);

  if(t->name) XFree(t->name);
  free(t);

  subUtilLogDebug("kill=tray\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
