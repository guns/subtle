
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

  assert(SUB_PANEL_SUBLET == type || SUB_PANEL_VIEWS == type ||
    SUB_PANEL_TITLE == type);

  /* Create new panel */
  p = PANEL(subSharedMemoryAlloc(1, sizeof(SubPanel)));
  p->flags = (SUB_TYPE_PANEL|type);

  sattrs.override_redirect = True;

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS|SUB_PANEL_TITLE))
    {
      case SUB_PANEL_SUBLET: /* {{{ */
        p->sublet = (struct subsublet_t *)subSharedMemoryAlloc(1,
          sizeof(struct subsublet_t));

        /* Sublet specific */
        p->sublet->time  = subSubtleTime();
        p->sublet->text  = subSharedTextNew();
        p->sublet->fg    = subtle->colors.fg_sublets;
        p->sublet->bg    = subtle->colors.bg_sublets;

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
          subtle->colors.bg_focus);

        XChangeWindowAttributes(subtle->dpy, p->win,
          CWOverrideRedirect, &sattrs);
        break; /* }}} */
    }

  subPanelConfigure(p);

  subSharedLogDebug("new=panel, type=%s\n", SUB_PANEL_VIEWS == type ? "views" : "title");

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
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS|SUB_PANEL_TITLE))
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

                /* Font offset, panel border and padding without icon */
                if(!v->text)
                  {
                    v->width = subSharedTextWidth(subtle->dpy, subtle->font,
                      v->name, strlen(v->name), NULL, NULL, True)
                      + 6 + 2 * subtle->pbw + subtle->padding.x + subtle->padding.y;
                  }

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
        XSetWindowBorder(subtle->dpy, p->win, subtle->colors.bo_focus);
        XSetWindowBorderWidth(subtle->dpy, p->win, subtle->pbw);
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
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_TITLE))
    {
      case SUB_PANEL_SUBLET: /* {{{ */
        XResizeWindow(subtle->dpy, p->win, p->width - 2 * subtle->pbw,
          subtle->th - 2 * subtle->pbw);
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

                    /* Font offset, panel border and padding */
                    p->width = subSharedTextWidth(subtle->dpy, subtle->font,
                      c->name, 50 >= len ? len : 50, NULL, NULL, True) +
                      6 + 2 * subtle->pbw  + subtle->padding.x + subtle->padding.y;

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
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS|SUB_PANEL_TITLE))
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
                SubView *v = VIEW(subtle->views->data[i]);

                /* Select color pair */
                if(subtle->urgent_tags & v->tags)
                  {
                    fg = subtle->colors.fg_urgent;
                    bg = subtle->colors.bg_urgent;
                    bo = subtle->colors.bo_urgent;
                  }
                else if(p->screen->vid == i)
                  {
                    fg = subtle->colors.fg_focus;
                    bg = subtle->colors.bg_focus;
                    bo = subtle->colors.bo_focus;
                  }
                else if(subtle->visible_clients & v->tags)
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

                /* Set window background and border*/
                XSetWindowBackground(subtle->dpy, p->views[i], bg);
                XSetWindowBorder(subtle->dpy, p->views[i], bo);
                XClearWindow(subtle->dpy, p->views[i]);

                /* Draw view name or icon text */
                if(!v->text)
                  {
                    subSharedTextDraw(subtle->dpy, subtle->gcs.font,
                      subtle->font, p->views[i], 3 + subtle->padding.x,
                      subtle->font->y + subtle->padding.width,
                      fg, bg, v->name);
                  }
                else
                  {
                    subSharedTextRender(subtle->dpy, subtle->gcs.font,
                      subtle->font, p->views[i], 3 + subtle->padding.x,
                      subtle->font->y + subtle->padding.width,
                      fg, bg, v->text);
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
                  p->win, 3 + subtle->padding.x,
                  subtle->font->y + subtle->padding.width, fg, bg, buf);
              }
          }
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
  if(!(p1->flags & (SUB_PANEL_INTERVAL))) return 1;
  if(!(p2->flags & (SUB_PANEL_INTERVAL))) return -1;

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
  int i = 0, idx = 0;
  char **names = NULL;
  Window *wins = NULL;

  /* Alloc space */
  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata,
    sizeof(char *));
  wins  = (Window *)subSharedMemoryAlloc(subtle->sublets->ndata,
    sizeof(Window));

  /* Find sublets */
  for(i = 0; i < subtle->sublets->ndata; i++)
    {
      SubPanel *p = PANEL(subtle->sublets->data[i]);

      names[idx]  = p->sublet->name;
      wins[idx++] = p->win;
    }

  /* EWMH: Sublet list and windows */
  subEwmhSetStrings(ROOT, SUB_EWMH_SUBTLE_SUBLET_LIST,
    names, subtle->sublets->ndata);
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
  switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS|SUB_PANEL_TRAY))
    {
      case SUB_PANEL_SUBLET: /* {{{ */
        subRubyRelease(p->sublet->instance);

        /* Remove socket watch */
        if(p->flags & SUB_PANEL_SOCKET)
          {
            XDeleteContext(subtle->dpy, subtle->windows.support, p->sublet->watch);
            subEventWatchDel(p->sublet->watch);
          }

#ifdef HAVE_SYS_INOTIFY_H
        /* Remove inotify watch */
        if(p->flags & SUB_PANEL_INOTIFY)
          {
            XDeleteContext(subtle->dpy, subtle->windows.support, p->sublet->watch);
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
    }

  XDestroyWindow(subtle->dpy, p->win);

  free(p);

  subSharedLogDebug("kill=panel\n");
} /* }}} */
