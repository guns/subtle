
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

 /** subTrayNew {{{
  * @brief Create new tray
  * @param[in]  win  Tray window
  * @return Returns a new #SubTray or \p NULL
  **/

SubTray *
subTrayNew(Window win)
{
  SubTray *t;

  assert(win);

  t = TRAY(subUtilAlloc(1, sizeof(SubTray)));
  t->flags = SUB_TYPE_TRAY;
  t->win   = win;

  printf("Adding tray\n");
  subUtilLogDebug("new=try, win=%#lx\n", win);

  return t;
} /* }}} */

 /** subTraySelect {{{
  * @brief Get tray selection for screen
  **/

void
subTraySelect(void)
{
  XClientMessageEvent ev;
  Atom sel = subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION);

  /* Tray selection */
  XSetSelectionOwner(subtle->disp, sel, subtle->windows.tray, CurrentTime);
  if(XGetSelectionOwner(subtle->disp, sel) == subtle->windows.tray)
    {
      printf("Got selection: %ld\n", sel);
    }
  else printf("Failed to get selection\n");

  /* Send manager info */
  ev.type         = ClientMessage;
  ev.message_type = XInternAtom(subtle->disp, "MANAGER", False);
  ev.window       = ROOT;
  ev.format       = 32;
  ev.data.l[0]    = CurrentTime;
  ev.data.l[1]    = subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION);
  ev.data.l[2]    = subtle->windows.tray;
  ev.data.l[3]    = 0;
  ev.data.l[4]    = 0;

  XSendEvent(subtle->disp, ROOT, False, 0xFFFFFF, (XEvent *)&ev); 
} /* }}} */

 /** subTrayKill {{{
  * @brief Kill tray
  * @param[in]  t  A #SubTray
  **/

void
subTrayKill(SubTray *t)
{
  assert(t);

  printf("Killing tray\n");

  free(t);

  subUtilLogDebug("kill=tray");
} /* }}} */
