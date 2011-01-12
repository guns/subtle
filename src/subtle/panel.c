
 /**
  * @package subtle
  *
  * @file Panel functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
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

  /* Create new panel */
  p = PANEL(subSharedMemoryAlloc(1, sizeof(SubPanel)));
  p->flags = (SUB_TYPE_PANEL|type);

  sattrs.override_redirect = True;

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_COPY|
      SUB_PANEL_VIEWS|SUB_PANEL_TITLE|SUB_PANEL_ICON))
    {
      case SUB_PANEL_SUBLET: /* {{{ */
        p->sublet = SUBLET(subSharedMemoryAlloc(1, sizeof(SubSublet)));

        /* Sublet specific */
        p->sublet->time  = subSubtleTime();
        p->sublet->text  = subSharedTextNew();
        p->sublet->fg    = subtle->colors.fg_sublets;
        p->sublet->bg    = subtle->colors.bg_sublets; /* }}} */
      case SUB_PANEL_COPY: /* {{{ */
        /* Create window */
        p->win = XCreateSimpleWindow(subtle->dpy, ROOT, 0, 0, 1,
          subtle->th, 0, 0, subtle->colors.bg_sublets);

        XChangeWindowAttributes(subtle->dpy, p->win,
          CWOverrideRedirect, &sattrs);
        XSaveContext(subtle->dpy, p->win, SUBLETID, (void *)p);
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        if(0 < subtle->views->ndata)
          {
            int i;

            p->win = XCreateSimpleWindow(subtle->dpy, ROOT, 0, 0, 1, 1, 0, 0,
              subtle->colors.bg_views);

            /* Create button windows */
            p->views = (Window *)subSharedMemoryAlloc(subtle->views->ndata,
              sizeof(Window));

            for(i = 0; i < subtle->views->ndata; i++)
              {
                p->views[i] = XCreateSimpleWindow(subtle->dpy, p->win,
                  0, 0, 1, subtle->th, 0, 0, subtle->colors.bg_views);

                XSaveContext(subtle->dpy, p->views[i], VIEWID,
                  subtle->views->data[i]);
                XSelectInput(subtle->dpy, p->views[i], ButtonPressMask);

                XChangeWindowAttributes(subtle->dpy, p->views[i],
                  CWOverrideRedirect, &sattrs);

                XMapRaised(subtle->dpy, p->views[i]);
              }
          }
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        p->win = XCreateSimpleWindow(subtle->dpy, ROOT, 0, 0, 1, 1, 0, 0,
          subtle->colors.bg_title);

        XChangeWindowAttributes(subtle->dpy, p->win,
          CWOverrideRedirect, &sattrs);
        break; /* }}} */
      case SUB_PANEL_ICON:
        p->icon = ICON(subSharedMemoryAlloc(1, sizeof(SubIcon)));

        /* Create window */
        p->win = XCreateSimpleWindow(subtle->dpy, ROOT, 0, 0, 1, 1, 0, 0,
          subtle->colors.panel);

        XChangeWindowAttributes(subtle->dpy, p->win,
          CWOverrideRedirect, &sattrs);
        break;
    }

  subPanelConfigure(p);

  subSharedLogDebug("new=panel, type=%s\n",
    SUB_PANEL_VIEWS == type ? "views" : "title");

  return p;
} /* }}} */

 /** subPanelConfigure {{{
  * @brief Configure panel
  * @param[in]  p  A #SubPanel
  **/

void
subPanelConfigure(SubPanel *p)
{
  assert(p);

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS|
      SUB_PANEL_TITLE|SUB_PANEL_ICON))
    {
      case SUB_PANEL_SUBLET: /* {{{ */
        /* Update borders */
        XSetWindowBorder(subtle->dpy, p->win, subtle->colors.bo_sublets);
        XSetWindowBorderWidth(subtle->dpy, p->win, subtle->pbw);
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

                v->width = 0; ///< Reset width

                /* Add width of view icon and/or text */
                if(v->flags & SUB_VIEW_ICON)
                  v->width = v->icon->width;

                if(!(v->flags & SUB_VIEW_ICON_ONLY))
                  {
                    if(v->flags & SUB_VIEW_ICON) v->width += 3;

                    v->width += subSharedTextWidth(subtle->dpy, subtle->font,
                      v->name, strlen(v->name), NULL, NULL, True);
                  }

                v->width += subtle->padding.x + subtle->padding.y + 6
                  + 2 * subtle->pbw;

                XMoveResizeWindow(subtle->dpy, p->views[i], p->width, 0,
                  v->width - 2 * subtle->pbw, subtle->th - 2 * subtle->pbw);
                p->width += v->width;

                /* Update borders */
                XSetWindowBorder(subtle->dpy, p->views[i],
                  subtle->colors.bo_views);
                XSetWindowBorderWidth(subtle->dpy, p->views[i], subtle->pbw);
              }

            XResizeWindow(subtle->dpy, p->win, p->width, subtle->th);
          }
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        /* Update border */
        XSetWindowBorder(subtle->dpy, p->win, subtle->colors.bo_title);
        XSetWindowBorderWidth(subtle->dpy, p->win, subtle->pbw);
        break; /* }}} */
      case SUB_PANEL_ICON: /* {{{ */
        XResizeWindow(subtle->dpy, p->win, p->width, subtle->th);
        break; /* }}} */
    }
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
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS|
      SUB_PANEL_TITLE|SUB_PANEL_ICON))
    {
      case SUB_PANEL_SUBLET: /* {{{ */
        /* Copy width in case of shallow copies */
        p->width = p->sublet->width;

        XResizeWindow(subtle->dpy, p->win, p->width - 2 * subtle->pbw,
          subtle->th - 2 * subtle->pbw);
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

                /* Check dynamic views */
                if(v->flags & SUB_VIEW_DYNAMIC &&
                    !(subtle->client_tags & v->tags))
                  continue;

                XMoveResizeWindow(subtle->dpy, p->views[i], p->width, 0,
                  v->width - 2 * subtle->pbw, subtle->th - 2 * subtle->pbw);
                p->width += v->width;
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
                    if(c->flags & SUB_CLIENT_MODE_FULL)  len++;
                    if(c->flags & SUB_CLIENT_MODE_FLOAT) len++;
                    if(c->flags & SUB_CLIENT_MODE_STICK) len++;

                    /* Font offset, panel border and padding */
                    p->width = subSharedTextWidth(subtle->dpy, subtle->font,
                      c->name, 50 >= len ? len : 50, NULL, NULL, True) + 6 +
                      2 * subtle->pbw + subtle->padding.x + subtle->padding.y;

                    XResizeWindow(subtle->dpy, p->win,
                      p->width - 2 * subtle->pbw,
                      subtle->th - 2 * subtle->pbw);
                  }
              }
          }
        break; /* }}} */
      case SUB_PANEL_ICON: /* {{{ */
        p->width = p->icon->width + subtle->padding.x + subtle->padding.y + 4;

        XResizeWindow(subtle->dpy, p->win, p->width - 2 * subtle->pbw,
          subtle->th - 2 * subtle->pbw);
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
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS|
      SUB_PANEL_TITLE|SUB_PANEL_ICON))
    {
      case SUB_PANEL_SUBLET: /* {{{ */
        /* Set window background */
        XSetWindowBackground(subtle->dpy, p->win, p->sublet->bg);
        XClearWindow(subtle->dpy, p->win);

        /* Render text parts */
        subSharedTextRender(subtle->dpy, subtle->gcs.font, subtle->font,
          p->win, 3 + subtle->padding.x, subtle->font->y + subtle->padding.width,
          p->sublet->fg, p->sublet->bg, p->sublet->text);
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        if(0 < subtle->views->ndata)
          {
            int i;
            long fg = 0, bg = 0, bo = 0;

            /* View buttons */
            for(i = 0; i < subtle->views->ndata; i++)
              {
                int x = subtle->padding.x + 3;
                SubView *v = VIEW(subtle->views->data[i]);

                /* Select color pair */
                if(p->screen->vid == i)
                  {
                    fg = subtle->colors.fg_focus;
                    bg = subtle->colors.bg_focus;
                    bo = subtle->colors.bo_focus;
                  }
                else if(subtle->client_tags & v->tags)
                  {
                    fg = subtle->colors.fg_occupied;
                    bg = subtle->colors.bg_occupied;
                    bo = subtle->colors.bo_occupied;
                  }
                else
                  {
                    fg = subtle->colors.fg_views;
                    bg = subtle->colors.bg_views;
                    bo = subtle->colors.bo_views;
                  }

                /* Add urgent colors */
                if(subtle->urgent_tags & v->tags)
                  {
                    if(-1 != subtle->colors.fg_urgent)
                      fg = subtle->colors.fg_urgent;
                    if(-1 != subtle->colors.bg_urgent)
                      bg = subtle->colors.bg_urgent;
                    if(-1 != subtle->colors.bo_urgent)
                      bo = subtle->colors.bo_urgent;
                  }

                /* Set window background and border*/
                XSetWindowBackground(subtle->dpy, p->views[i], bg);
                XSetWindowBorder(subtle->dpy, p->views[i], bo);
                XClearWindow(subtle->dpy, p->views[i]);

                /* Draw view icon and/or text */
                if(v->flags & SUB_VIEW_ICON)
                  {
                    subSharedTextIconDraw(subtle->dpy, subtle->gcs.font,
                      p->views[i], x, subtle->font->y - v->icon->height +
                      subtle->padding.width, v->icon->width, v->icon->height,
                      fg, bg, v->icon->pixmap, v->icon->bitmap);

                    x += v->icon->width;
                  }

                if(!(v->flags & SUB_VIEW_ICON_ONLY))
                  {
                    if(v->flags & SUB_VIEW_ICON) x += 3;

                    subSharedTextDraw(subtle->dpy, subtle->gcs.font,
                      subtle->font, p->views[i], x, subtle->font->y +
                      subtle->padding.width, fg, bg, v->name);
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
                long fg = 0, bg = 0, bo = 0;

                DEAD(c);

                /* Title modes */
                if(c->flags & SUB_CLIENT_MODE_FULL)
                  {
                    snprintf(buf + x, sizeof(buf), "%c", '+');
                    x++;
                  }
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

                snprintf(buf + x, sizeof(buf) - x, "%s", c->name);

                /* Select color pair */
                fg = subtle->colors.fg_title;
                bg = subtle->colors.bg_title;
                bo = subtle->colors.bo_title;

                /* Add urgent colors */
                if(c->flags & SUB_CLIENT_MODE_URGENT)
                  {
                    if(-1 != subtle->colors.fg_urgent)
                      fg = subtle->colors.fg_urgent;
                    if(-1 != subtle->colors.bg_urgent)
                      bg = subtle->colors.bg_urgent;
                    if(-1 != subtle->colors.bo_urgent)
                      bo = subtle->colors.bo_urgent;
                  }

                /* Set window background */
                XSetWindowBackground(subtle->dpy, p->win, bg);
                XSetWindowBorder(subtle->dpy, p->win, bo);
                XClearWindow(subtle->dpy, p->win);

                subSharedTextDraw(subtle->dpy, subtle->gcs.font, subtle->font,
                  p->win, 3 + subtle->padding.x,
                  subtle->font->y + subtle->padding.width, fg, bg, buf);
              }
          }
        break; /* }}} */
      case SUB_PANEL_ICON: /* {{{ */
        subSharedTextIconDraw(subtle->dpy, subtle->gcs.font,
          p->win, 2 + subtle->padding.x,
          (subtle->th - p->icon->height) / 2,
          p->icon->width, p->icon->height, subtle->colors.fg_sublets,
          subtle->colors.bg_sublets, p->icon->pixmap, p->icon->bitmap);
        break; /* }}} */
    }
} /* }}} */

 /** subPanelCompare {{{
  * @brief Compare two panels
  * @param[in]  a  A #SubPanel
  * @param[in]  b  A #SubPanel
  * @return Returns the result of the comparison of both panels
  * @retval  -1  a is smaller
  * @retval  0   a and b are equal
  * @retval  1   a is greater
  **/

int
subPanelCompare(const void *a,
  const void *b)
{
  SubPanel *p1 = *(SubPanel **)a, *p2 = *(SubPanel **)b;

  assert(a && b);

  /* Include only interval sublets */
  if(!(p1->sublet->flags & (SUB_SUBLET_INTERVAL))) return 1;
  if(!(p2->sublet->flags & (SUB_SUBLET_INTERVAL))) return -1;

  return p1->sublet->time < p2->sublet->time ? -1 :
    (p1->sublet->time == p2->sublet->time ? 0 : 1);
} /* }}} */

 /** subPanelDimension {{{
  * @brief Redimension panel items
  * @param[in]  id  View id
  **/

void
subPanelDimension(int id)
{
  int i, j, k;

  /* Update all clients */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      for(j = 0; j < s->panels->ndata; j++)
        {
          SubPanel *p = PANEL(s->panels->data[j]);

          if(p->flags & SUB_PANEL_VIEWS)
            {
              if(-1 != id)
                {
                  XUnmapWindow(subtle->dpy, p->views[id]);
                  XDestroyWindow(subtle->dpy, p->views[id]);

                  /* Shift if necessary */
                  for(k = id; k < subtle->views->ndata; k++)
                    p->views[k] = p->views[k + 1];
                }

              /* Resize array */
              p->views = (Window *)subSharedMemoryRealloc((void *)p->views,
                subtle->views->ndata * sizeof(Window));

              /* Create new window */
              if(-1 == id)
                {
                  XSetWindowAttributes sattrs;
                  int vid = subtle->views->ndata - 1;

                  p->views[vid] = XCreateSimpleWindow(subtle->dpy,
                    p->win, 0, 0, 1, subtle->th, 0, 0, subtle->colors.bg_views);

                  XSaveContext(subtle->dpy, p->views[vid], VIEWID,
                    subtle->views->data[vid]);
                  XSelectInput(subtle->dpy, p->views[vid], ButtonPressMask);

                  sattrs.override_redirect = True;

                  XChangeWindowAttributes(subtle->dpy, p->views[vid],
                    CWOverrideRedirect, &sattrs);

                  XMapRaised(subtle->dpy, p->views[vid]);
                }

              subPanelConfigure(p);
              subPanelRender(p);
            }
        }
    }
} /* }}} */

 /** subPanelPublish {{{
  * @brief Publish panels
  **/

void
subPanelPublish(void)
{
  int i = 0, j = 0, idx = 0;
  char **names = NULL;
  Window *wins = NULL;

  /* Alloc space */
  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata,
    sizeof(char *));
  wins  = (Window *)subSharedMemoryAlloc(subtle->sublets->ndata,
    sizeof(Window));

  /* Find sublet in panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      for(j = 0; j < s->panels->ndata; j++)
        {
          SubPanel *p = PANEL(s->panels->data[j]);

          /* Include sublets, exclude shallow copies */
          if(p->flags & SUB_PANEL_SUBLET && !(p->flags & SUB_PANEL_COPY))
            {
              names[idx]  = p->sublet->name;
              wins[idx++] = p->win;
            }
        }
    }

  /* EWMH: Sublet list and windows */
  subSharedPropertySetStrings(subtle->dpy, ROOT,
    subEwmhGet(SUB_EWMH_SUBTLE_SUBLET_LIST), names, subtle->sublets->ndata);
  subEwmhSetWindows(ROOT, SUB_EWMH_SUBTLE_SUBLET_WINDOWS,
    wins, subtle->sublets->ndata);

  subSharedLogDebug("publish=panel, n=%d\n", subtle->sublets->ndata);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(names);
  free(wins);
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
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_COPY|
      SUB_PANEL_VIEWS|SUB_PANEL_TRAY|SUB_PANEL_ICON))
    {
      case SUB_PANEL_COPY: break;
      case SUB_PANEL_SUBLET: /* {{{ */
        if(!(p->flags & SUB_PANEL_COPY))
          {
            subRubyRelease(p->sublet->instance);

            /* Remove socket watch */
            if(p->sublet->flags & SUB_SUBLET_SOCKET)
              {
                XDeleteContext(subtle->dpy, subtle->windows.support,
                  p->sublet->watch);
                subEventWatchDel(p->sublet->watch);
              }

#ifdef HAVE_SYS_INOTIFY_H
            /* Remove inotify watch */
            if(p->sublet->flags & SUB_SUBLET_INOTIFY)
              {
                XDeleteContext(subtle->dpy, subtle->windows.support,
                  p->sublet->watch);
                inotify_rm_watch(subtle->notify, p->sublet->interval);
              }
#endif /* HAVE_SYS_INOTIFY_H */

            XDeleteContext(subtle->dpy, p->win, SUBLETID);

            if(p->sublet->name)
              {
                printf("Unloaded sublet (%s)\n", p->sublet->name);
                free(p->sublet->name);
              }
            if(p->sublet->text) subSharedTextFree(p->sublet->text);

            free(p->sublet);
          }
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        if(p->views)
          {
            int i;

            for(i = 0; i < subtle->views->ndata; i++)
              XDestroyWindow(subtle->dpy, p->views[i]);

            free(p->views);
          }
        break; /* }}} */
      case SUB_PANEL_TRAY: /* {{{ */
        /* Reparent and return to avoid destroy here */
        XReparentWindow(subtle->dpy, subtle->windows.tray.win, ROOT, 0, 0);
        return; /* }}} */
      case SUB_PANEL_ICON: /* {{{ */
        if(p->icon) free(p->icon);
        break; /* }}} */
    }

  XDestroyWindow(subtle->dpy, p->win);

  free(p);

  subSharedLogDebug("kill=panel\n");
} /* }}} */
