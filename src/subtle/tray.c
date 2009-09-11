
 /**
  * @package subtle
  *
  * @file Tray functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subTrayNew {{{
  * @brief Create new tray
  * @param[in]  win  Tray window
  * @return Returns a new #SubTray or \p NULL
  **/

SubTray *
subTrayNew(Window win)
{
  SubTray *t = NULL;

  assert(win);

  t = TRAY(subSharedMemoryAlloc(1, sizeof(SubTray)));
  t->flags = SUB_TYPE_TRAY;
  t->win   = win;
  t->width = subtle->th; ///< Default width

  /* Fetch name */
  if(!XFetchName(subtle->dpy, t->win, &t->name))
    {
      free(t);

      return NULL;
    }

  /* Update tray window */
  subEwmhSetWMState(t->win, WithdrawnState);
  XSelectInput(subtle->dpy, t->win, EVENTMASK);
  XReparentWindow(subtle->dpy, t->win, subtle->panels.tray.win, 0, 0);
  XAddToSaveSet(subtle->dpy, t->win);
  XSaveContext(subtle->dpy, t->win, TRAYID, (void *)t);

  subEwmhMessage(t->win, t->win, SUB_EWMH_XEMBED, CurrentTime, XEMBED_EMBEDDED_NOTIFY,
    0, subtle->panels.tray.win, 0); ///< Start embedding life cycle 

  subSharedLogDebug("new=tray, name=%s, win=%#lx\n", t->name, win);

  return t;
} /* }}} */

 /** subTrayConfigure {{{
  * @brief Configure tray
  * @param[in]  t  A #SubTray
  **/

void
subTrayConfigure(SubTray *t)
{
  long supplied = 0;
  XSizeHints *hints = NULL;

  assert(t);
  
  /* Size hints */
  if(!(hints = XAllocSizeHints())) 
    subSharedLogError("Can't alloc memory. Exhausted?\n");

  XGetWMNormalHints(subtle->dpy, t->win, hints, &supplied);
  if(0 < supplied)
    {
      if(hints->flags & (USSize|PSize)) ///< User/program size
        t->width = MINMAX(hints->width, subtle->th, 2 * subtle->th);
      else if(hints->flags & PBaseSize) ///< Base size
        t->width = MINMAX(hints->base_width, subtle->th, 2 * subtle->th);
      else if(hints->flags & PMinSize) ///< Min size
        t->width = MINMAX(hints->min_width, subtle->th, 2 * subtle->th);
    }
  XFree(hints);

  subSharedLogDebug("Tray: width=%d, supplied=%ld\n", t->width, supplied);
} /* }}} */

 /** subTrayUpdate {{{
  * @brief Update tray window
  **/

void
subTrayUpdate(void)
{
  subtle->panels.tray.width = 0; ///< Reset width

  if(0 < subtle->trays->ndata)
    {
      int i;

      /* Resize every tray */
      for(i = 0, subtle->panels.tray.width = 3; i < subtle->trays->ndata; i++)
        {
          SubTray *t = TRAY(subtle->trays->data[i]);

          XMoveResizeWindow(subtle->dpy, t->win, subtle->panels.tray.width, 0, t->width, subtle->th);
          subtle->panels.tray.width += t->width;
        }

      XResizeWindow(subtle->dpy, subtle->panels.tray.win, subtle->panels.tray.width + 3, subtle->th);
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
  XSetSelectionOwner(subtle->dpy, sel, subtle->panels.tray.win, CurrentTime);
  if(XGetSelectionOwner(subtle->dpy, sel) == subtle->panels.tray.win)
    {
      subSharedLogDebug("Selection: type=%ld\n", sel);
    }
  else subSharedLogError("Failed getting tray selection\n");

  /* Send manager info */
  subEwmhMessage(ROOT, ROOT, SUB_EWMH_MANAGER, CurrentTime, 
    subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION), subtle->panels.tray.win, 0, 0);
} /* }}} */
  
 /** subTrayFocus {{{
  * @brief Set focus to tray
  * @param[in]  t  A #SubTray
  **/

void
subTrayFocus(SubTray *t)
{
  assert(t);

  XSetInputFocus(subtle->dpy, t->win, RevertToNone, CurrentTime);

  subSharedLogDebug("Focus: win=%#lx\n", t->win);
} /* }}} */
  
 /** subTraySetState {{{
  * @brief Set window state and map/unmap accordingly
  * @param[in]  t  A #SubTray
  **/

void
subTraySetState(SubTray *t)
{
  int opcode = 0;
  long flags = 0;

  assert(t);

  /* Get xembed data */
  if((flags = subEwmhGetXEmbedState(t->win)))
    {
      if(flags & XEMBED_MAPPED) ///< Map if wanted
        {
          opcode = XEMBED_WINDOW_ACTIVATE;
          XMapRaised(subtle->dpy, t->win); 
          subEwmhSetWMState(t->win, NormalState);
        }
      else 
        {
          opcode = XEMBED_WINDOW_DEACTIVATE;
          XUnmapWindow(subtle->dpy, t->win);
          subEwmhSetWMState(t->win, WithdrawnState);
        }

      subEwmhMessage(t->win, t->win, SUB_EWMH_XEMBED, CurrentTime, opcode, 0, 0, 0);
    }
} /* }}} */

 /** subTrayKill {{{
  * @brief Kill tray
  * @param[in]  t  A #SubTray
  **/

void
subTrayKill(SubTray *t)
{
  assert(t);

  /* Ignore further events and delete context */
  XSelectInput(subtle->dpy, t->win, NoEventMask);
  XDeleteContext(subtle->dpy, t->win, TRAYID);

  if(t->name) XFree(t->name);
  free(t);

  subSharedLogDebug("kill=tray\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
