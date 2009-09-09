
 /**
  * @package subtle
  *
  * @file Panel functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtle.h"

 /** subPanelUpdate {{{
  * @brief Configure panels
  **/

void
subPanelUpdate(void)
{
  SubPanel *p = NULL;
  int n = 0, x = 0, width[2] = { 0 }, spacer[2] = { 0 };

  assert(subtle);

  /* Gather width for spacer */
  for(p = subtle->panel; p; p = p->next)
    {
      if(p->flags & SUB_PANEL_BOTTOM) n = 1;
      if(p->flags & SUB_PANEL_SPACER1) spacer[n]++;
      if(p->flags & SUB_PANEL_SPACER2) spacer[n]++;

      width[n] += p->width;
    }

  /* Move and resize windows */
  for(p = subtle->panel, n = 0; p; p = p->next) 
    {
      if(p->flags & SUB_PANEL_BOTTOM)
        {
          n = 1;
          x = 0; ///< Reset for new panel
        }

      if(p->flags & SUB_PANEL_SPACER1) ///< Add before spacer
        x += (subtle->screen->base.width - width[n]) / spacer[n];

      if(p->x != x) ///< Cache x value
        {
          XMoveWindow(subtle->dpy, p->win, x, 0);
          p->x = x;
        }

      if(p->flags & SUB_PANEL_SPACER2) ///< Add after spacer
        x += (subtle->screen->base.width - width[n]) / spacer[n];

      /* Remap window only when needed */
      if(0 < p->width) XMapRaised(subtle->dpy, p->win);
      else XUnmapWindow(subtle->dpy, p->win);

      x += p->width;
    }
} /* }}} */

 /** subPanelRender {{{ 
  * @brief Render panels
  **/

void
subPanelRender(void)
{
  SubClient *c = NULL;

  assert(subtle);

  XClearWindow(subtle->dpy, subtle->windows.panel1);
  XClearWindow(subtle->dpy, subtle->windows.panel2);

  if(subtle->flags & SUB_SUBTLE_STIPPLE) ///< Draw stipple
    {
      XFillRectangle(subtle->dpy, subtle->windows.panel1, subtle->gcs.stipple, 0, 2,
        subtle->screen->base.width, subtle->th - 4);
      XFillRectangle(subtle->dpy, subtle->windows.panel2, subtle->gcs.stipple, 0, 2,
        subtle->screen->base.width, subtle->th - 4);        
    }

  /* Render panels */
  if(subtle->windows.focus && (c = CLIENT(subSharedFind(subtle->windows.focus, CLIENTID)))) 
    subClientRender(c);
  subSubletRender();
  subViewRender();
} /* }}} */
