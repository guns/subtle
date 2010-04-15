
 /**
  * @package subtle
  *
  * @file Panel functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
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
  int i, n = 0, x = 0, separator[2] = { 0 }, width[2] = { 0 },
    spacer[2] = { 0 };

  assert(subtle);

  /* Collect width for spacer */
  for(i = 0; i < subtle->panels->ndata; i++)
    {
      SubPanel *p = PANEL(subtle->panels->data[i]);

      if(p->flags & SUB_PANEL_HIDDEN) continue;
      if(p->flags & SUB_PANEL_BOTTOM)  n = 1;
      if(p->flags & SUB_PANEL_SPACER1) spacer[n]++;
      if(p->flags & SUB_PANEL_SPACER2) spacer[n]++;
      if(p->flags & SUB_PANEL_SEPARATOR1)
        separator[n] += subtle->separator.width;
      if(p->flags & SUB_PANEL_SEPARATOR2)
        separator[n] += subtle->separator.width;

      width[n] += p->width;
    }

  /* Move and resize windows */
  for(i = 0, n = 0; i < subtle->panels->ndata; i++)
    {
      SubPanel *p = PANEL(subtle->panels->data[i]);

      if(p->flags & SUB_PANEL_HIDDEN) continue;
      if(p->flags & SUB_PANEL_BOTTOM)
        {
          n = 1;
          x = 0; ///< Reset for new panel
        }

      /* Before panel item */
      if(p->flags & SUB_PANEL_SEPARATOR1) ///< Add separator
        x += subtle->separator.width;

      if(p->flags & SUB_PANEL_SPACER1) ///< Add spacer
        x += (DEFSCREEN->base.width - width[n] - separator[n]) / spacer[n];

      /* Set window position */
      XMoveWindow(subtle->dpy, p->win, x, 0);
      p->x = x;

      /* After panel item */
      if(p->flags & SUB_PANEL_SEPARATOR2) ///< Add separator
        x += subtle->separator.width;

      if(p->flags & SUB_PANEL_SPACER2) ///< Add spacer
        x += (DEFSCREEN->base.width - width[n]) / spacer[n];

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
  int i;
  SubClient *c = NULL;
  Window panel = subtle->windows.panel1;

  assert(subtle);

  XClearWindow(subtle->dpy, subtle->windows.panel1);
  XClearWindow(subtle->dpy, subtle->windows.panel2);

  /* Draw stipple on panels */
  if(subtle->flags & SUB_SUBTLE_STIPPLE) ///< Draw stipple
    {
      XFillRectangle(subtle->dpy, subtle->windows.panel1, subtle->gcs.stipple, 0, 2,
        DEFSCREEN->base.width, subtle->th - 4);
      XFillRectangle(subtle->dpy, subtle->windows.panel2, subtle->gcs.stipple, 0, 2,
        DEFSCREEN->base.width, subtle->th - 4);
    }

  /* Draw separators */
  for(i = 0; i < subtle->panels->ndata; i++)
    {
      SubPanel *p = PANEL(subtle->panels->data[i]);

      if(p->flags & SUB_PANEL_HIDDEN) continue;
      if(p->flags & SUB_PANEL_BOTTOM) panel = subtle->windows.panel2;
      if(p->flags & SUB_PANEL_SEPARATOR1) ///< Draw separator before panel
        subSharedTextDraw(subtle->dpy, subtle->gcs.font, subtle->font,
          panel, p->x - subtle->separator.width + 3, subtle->font->y,
          subtle->colors.fg_panel, -1, subtle->separator.string);
      if(p->flags & SUB_PANEL_SEPARATOR2) ///< Draw separator after panel
        subSharedTextDraw(subtle->dpy, subtle->gcs.font, subtle->font,
          panel, p->x + p->width + 3, subtle->font->y,
          subtle->colors.fg_panel, -1, subtle->separator.string);
    }

  /* Render panels */
  if(subtle->windows.focus && (c = CLIENT(subSubtleFind(subtle->windows.focus, CLIENTID))))
    subClientRender(c);

  subViewRender();

  for(i = 0; i < subtle->sublets->ndata; i++)
    subSubletRender(SUBLET(subtle->sublets->data[i]));
} /* }}} */
