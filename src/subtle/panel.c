
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

 /** subPanelNew {{{
  * @brief Create a new panel
  * @param[in]  type  Type of the panel
  * @return Returns a #SubPanel or \p NULL
  **/

SubPanel *
subPanelNew(int type)
{
  SubPanel *p = NULL;
  XSetWindowAttributes sattrs;

  assert(SUB_PANEL_VIEWS == type || SUB_PANEL_TITLE == type);

  /* Create new panel */
  p = PANEL(subSharedMemoryAlloc(1, sizeof(SubPanel)));
  p->flags = (SUB_TYPE_PANEL|type);

  sattrs.override_redirect = True;

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_VIEWS|SUB_PANEL_TITLE))
    {
      case SUB_PANEL_VIEWS: /* {{{ */
        if(0 < subtle->views->ndata)
          {
            int i;

            p->win = XCreateSimpleWindow(subtle->dpy, ROOT, 0, 0, 1, 1, 0, 0,
              subtle->colors.bg_views);

            /* Create button windows */
            p->wins = (Window *)subSharedMemoryAlloc(subtle->views->ndata,
              sizeof(Window));

            for(i = 0; i < subtle->views->ndata; i++)
              {
                p->wins[i] = XCreateSimpleWindow(subtle->dpy, p->win,
                  0, 0, 1, subtle->th, 0, 0, subtle->colors.bg_views);

                XSaveContext(subtle->dpy, p->wins[i], VIEWID,
                  subtle->views->data[i]);
                XSelectInput(subtle->dpy, p->wins[i], ButtonPressMask);

                XChangeWindowAttributes(subtle->dpy, p->wins[i],
                  CWOverrideRedirect, &sattrs);

                XMapRaised(subtle->dpy, p->wins[i]);
              }
          }
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
          {
            p->win = XCreateSimpleWindow(subtle->dpy, ROOT, 0, 0, 1, 1, 0, 0,
              subtle->colors.bg_focus);

            XChangeWindowAttributes(subtle->dpy, p->win,
              CWOverrideRedirect, &sattrs);

            /* Update title border */
            XSetWindowBorder(subtle->dpy, p->win, subtle->colors.bo_panel);
            XSetWindowBorderWidth(subtle->dpy, p->win, subtle->pbw);
          }
        break; /* }}} */
    }

  subSharedLogDebug("new=panel, type=%s\n", SUB_PANEL_VIEWS == type ? "views" : "title");

  return p;
} /* }}} */

 /** subPanelUpdate {{{
  * @brief Update panel
  * @param[in]  p  A #SubPanel
  **/

void
subPanelUpdate(SubPanel *p)
{
  assert(p);

  /* Handle panel item type */
  switch(p->flags & (SUB_TYPE_SUBLET|SUB_PANEL_VIEWS|SUB_PANEL_TITLE))
    {
      case SUB_TYPE_SUBLET: /* {{{ */
        subSubletUpdate(SUBLET(p));
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        p->width = 0;

        if(0 < subtle->views->ndata)
          {
            int i;

            /* Update for each view */
            for(i = 0; i < subtle->views->ndata; i++)
              {
                SubView *v = VIEW(subtle->views->data[i]);

                v->width = subSharedTextWidth(subtle->dpy, subtle->font,
                  v->name, strlen(v->name), NULL, NULL, True)
                  + 6 + 2 * subtle->pbw; ///< Font offset and panel border

                XMoveResizeWindow(subtle->dpy, p->wins[i], p->width, 0,
                  v->width - 2 * subtle->pbw, subtle->th - 2 * subtle->pbw);
                p->width += v->width;

                /* Set borders */
                XSetWindowBorder(subtle->dpy, p->wins[i],
                  subtle->colors.bo_panel);
                XSetWindowBorderWidth(subtle->dpy, p->wins[i], subtle->pbw);
              }

            XResizeWindow(subtle->dpy, p->win, p->width, subtle->th);
          }
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        p->width = 0;

        if(0 < subtle->clients->ndata)
          {
            SubClient *c = NULL;

            /* Find focus window */
            if((c = CLIENT(subSubtleFind(subtle->windows.focus, CLIENTID))))
              {
                assert(c);
                DEAD(c);

                /* Exclude desktop type windows */
                if(!(c->flags & SUB_CLIENT_TYPE_DESKTOP))
                  {
                    int len = strlen(c->name);

                    /* Title modes */
                    if(c->flags & SUB_CLIENT_MODE_FLOAT) len++;
                    if(c->flags & SUB_CLIENT_MODE_STICK) len++;

                    /* Update panel width */
                    p->width = subSharedTextWidth(subtle->dpy, subtle->font,
                      c->name, 50 >= len ? len : 50, NULL, NULL, True) +
                      6 + 2 * subtle->pbw; ///< Font offset and panel border

                    XResizeWindow(subtle->dpy, p->win,
                      p->width - 2 * subtle->pbw,
                      subtle->th - 2 * subtle->pbw);
                  }
              }
          }
        break; /* }}} */
    }
} /* }}} */

 /** subPanelRender {{{
  * @brief Render panel
  * @param[in]  p  A #SubPanel
  **/

void
subPanelRender(SubPanel *p)
{
  assert(p);

  /* Handle panel item type */
  switch(p->flags & (SUB_TYPE_SUBLET|SUB_PANEL_VIEWS|SUB_PANEL_TITLE))
    {
      case SUB_TYPE_SUBLET: /* {{{ */
        subSubletRender(SUBLET(p));
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        if(0 < subtle->views->ndata)
          {
            int i;
            long fg = 0, bg = 0;

            /* View buttons */
            for(i = 0; i < subtle->views->ndata; i++)
              {
                SubView *v = VIEW(subtle->views->data[i]);

                if(!(v->flags & SUB_PANEL_HIDDEN))
                  {
                    /* Select color pair */
                    if(v->flags & SUB_CLIENT_MODE_URGENT)
                      {
                        fg = subtle->colors.fg_urgent;
                        bg = subtle->colors.bg_urgent;
                      }
                    else if(p->screen->vid == i)
                      {
                        fg = subtle->colors.fg_focus;
                        bg = subtle->colors.bg_focus;
                      }
                    else
                      {
                        fg = subtle->colors.fg_views;
                        bg = subtle->colors.bg_views;
                      }

                    /* Set window background */
                    XSetWindowBackground(subtle->dpy, p->wins[i], bg);
                    XClearWindow(subtle->dpy, p->wins[i]);

                    subSharedTextDraw(subtle->dpy, subtle->gcs.font,
                      subtle->font, p->wins[i], 3, subtle->font->y,
                      fg, bg, v->name);
                  }
              }
          }
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        if(0 < subtle->clients->ndata)
          {
            SubClient *c = NULL;

            if((c = CLIENT(subSubtleFind(subtle->windows.focus, CLIENTID))))
              {
                int x = 0;
                char buf[50] = { 0 };
                long fg = 0, bg = 0;

                DEAD(c);

                /* Title modes */
                if(c->flags & SUB_CLIENT_MODE_FLOAT)
                  {
                    snprintf(buf + x, sizeof(buf), "%c", '^');
                    x++;
                  }
                if(c->flags & SUB_CLIENT_MODE_STICK)
                  {
                    snprintf(buf + x, sizeof(buf), "%c", '*');
                    x++;
                  }

                snprintf(buf + x, sizeof(buf), "%s", c->name);

                /* Select color pair */
                if(c->flags & SUB_CLIENT_MODE_URGENT)
                  {
                    fg = subtle->colors.fg_urgent;
                    bg = subtle->colors.bg_urgent;
                  }
                else
                  {
                    fg = subtle->colors.fg_focus;
                    bg = subtle->colors.bg_focus;
                  }

                /* Set window background */
                XSetWindowBackground(subtle->dpy, p->win, bg);
                XClearWindow(subtle->dpy, p->win);

                subSharedTextDraw(subtle->dpy, subtle->gcs.font, subtle->font,
                  p->win, 3, subtle->font->y, fg, bg, buf);
              }
          }
        break; /* }}} */
    }
} /* }}} */

 /** subPanelKill {{{
  * @brief Kill a panel
  * @param[in]  p  A #SubPanel
  **/

void
subPanelKill(SubPanel *p)
{
  assert(p);

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_VIEWS: /* {{{ */
        if(p->wins)
          {
            int i;

            for(i = 0; i < subtle->views->ndata; i++)
              XDestroyWindow(subtle->dpy, p->wins[i]);

            free(p->wins);
          }
        break; /* }}} */
    }

  XDestroyWindow(subtle->dpy, p->win);

  free(p);

  subSharedLogDebug("kill=panel\n");
} /* }}} */
