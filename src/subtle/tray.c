
 /**
  * @package subtle
  *
  * @file Tray functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
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

  /* Create new tray */
  t = TRAY(subSharedMemoryAlloc(1, sizeof(SubTray)));
  t->flags = SUB_TYPE_TRAY;
  t->win   = win;
  t->width = subtle->ph; ///< Default width

  /* Update tray properties */
  subSharedPropertyName(subtle->dpy, win, &t->name, PKG_NAME);
  subEwmhSetWMState(t->win, WithdrawnState);
  XSelectInput(subtle->dpy, t->win, TRAYMASK);
  XReparentWindow(subtle->dpy, t->win, subtle->windows.tray, 0, 0);
  XAddToSaveSet(subtle->dpy, t->win);
  XSaveContext(subtle->dpy, t->win, TRAYID, (void *)t);

  /* Start embedding life cycle */
  subEwmhMessage(t->win, SUB_EWMH_XEMBED, 0xFFFFFF, CurrentTime,
    XEMBED_EMBEDDED_NOTIFY, 0, subtle->windows.tray, 0);

  subSharedLogDebugSubtle("new=tray, name=%s, win=%#lx\n", t->name, win);

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
    {
      subSharedLogError("Can't alloc memory. Exhausted?\n");
      abort();
    }

  XGetWMNormalHints(subtle->dpy, t->win, hints, &supplied);
  if(0 < supplied)
    {
      if(hints->flags & (USSize|PSize)) ///< User/program size
        t->width = MINMAX(hints->width, subtle->ph, 2 * subtle->ph);
      else if(hints->flags & PBaseSize) ///< Base size
        t->width = MINMAX(hints->base_width, subtle->ph, 2 * subtle->ph);
      else if(hints->flags & PMinSize) ///< Min size
        t->width = MINMAX(hints->min_width, subtle->ph, 2 * subtle->ph);
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

          if(t->flags & SUB_TRAY_DEAD) continue;

          XMapWindow(subtle->dpy, t->win);
          XMoveResizeWindow(subtle->dpy, t->win, subtle->panels.tray.width,
            0, t->width, subtle->ph);
          subtle->panels.tray.width += t->width;
        }

      subtle->panels.tray.width += 3; ///< Add padding

      XMapRaised(subtle->dpy, subtle->windows.tray);
      XResizeWindow(subtle->dpy, subtle->windows.tray,
        subtle->panels.tray.width, subtle->ph);
    }
  else XUnmapWindow(subtle->dpy, subtle->windows.tray);
} /* }}} */

 /** subTraySetState {{{
  * @brief Set window state and map/unmap accordingly
  * @param[in]  t  A #SubTray
  **/

void
subTraySetState(SubTray *t)
{
  long flags = 0;

  assert(t);

  /* Get xembed data */
  if((flags = subEwmhGetXEmbedState(t->win)))
    {
      int opcode = 0;

      if(flags & XEMBED_MAPPED) ///< Map if wanted
        {
          opcode = XEMBED_WINDOW_ACTIVATE;
          XMapRaised(subtle->dpy, t->win);
          subEwmhSetWMState(t->win, NormalState);
        }
      else
        {
          t->flags |= SUB_TRAY_UNMAP;

          opcode = XEMBED_WINDOW_DEACTIVATE;
          XUnmapWindow(subtle->dpy, t->win);
          subEwmhSetWMState(t->win, WithdrawnState);
        }

      subEwmhMessage(t->win, SUB_EWMH_XEMBED, 0xFFFFFF,
        CurrentTime, opcode, 0, 0, 0);
    }
} /* }}} */

 /** subTraySelect {{{
  * @brief Set tray selection owner for screen
  **/

void
subTraySelect(void)
{
  Atom selection = subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION);

  /* Tray selection */
  XSetSelectionOwner(subtle->dpy, selection,
    subtle->windows.tray, CurrentTime);

  if(XGetSelectionOwner(subtle->dpy, selection) == subtle->windows.tray)
    {
      subSharedLogDebug("Selection: type=%ld\n", selection);
    }
  else subSharedLogError("Failed getting tray selection\n");

  /* Send manager info */
  subEwmhMessage(ROOT, SUB_EWMH_MANAGER, 0xFFFFFF, CurrentTime,
    subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION),
      subtle->windows.tray, 0, 0);
} /* }}} */

 /** subTrayDeselect {{{
  * @brief Unset tray selection owner for screen
  **/

void
subTrayDeselect(void)
{
  Atom selection = subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION);

  /* Tray selection */
  if(XGetSelectionOwner(subtle->dpy, selection) == subtle->windows.tray)
    {
      XSetSelectionOwner(subtle->dpy, selection, None, CurrentTime);
      subSharedPropertyDelete(subtle->dpy, ROOT, selection);
    }
} /* }}} */

 /** subTrayPublish {{{
  * @brief Publish trays
  **/

void
subTrayPublish(void)
{
  int i;
  Window *wins = (Window *)subSharedMemoryAlloc(subtle->trays->ndata, sizeof(Window));

  for(i = 0; i < subtle->trays->ndata; i++)
    wins[i] = TRAY(subtle->trays->data[i])->win;

  /* EWMH: Client list and client list stacking */
  subEwmhSetWindows(ROOT, SUB_EWMH_SUBTLE_TRAY_LIST, wins, subtle->trays->ndata);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(wins);

  subSharedLogDebugSubtle("publish=tray, trays=%d\n", subtle->trays->ndata);
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

  /* Unembed tray icon following xembed specs */
  XUnmapWindow(subtle->dpy, t->win);
  XReparentWindow(subtle->dpy, t->win, ROOT, 0, 0);
  XMapRaised(subtle->dpy, t->win);

  if(t->name) free(t->name);
  free(t);

  subSharedLogDebugSubtle("kill=tray\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
