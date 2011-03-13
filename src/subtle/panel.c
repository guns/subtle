
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

  /* Create new panel */
  p = PANEL(subSharedMemoryAlloc(1, sizeof(SubPanel)));
  p->flags = (SUB_TYPE_PANEL|type);

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_ICON|SUB_PANEL_SUBLET|SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_ICON: /* {{{ */
        p->icon = ICON(subSharedMemoryAlloc(1, sizeof(SubIcon)));
        break; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
        p->sublet = SUBLET(subSharedMemoryAlloc(1, sizeof(SubSublet)));

        /* Sublet specific */
        p->sublet->time   = subSubtleTime();
        p->sublet->text   = subSharedTextNew();
        p->sublet->fg     = subtle->colors.fg_sublets;
        p->sublet->bg     = subtle->colors.bg_sublets;
        p->sublet->textfg = -1;
        p->sublet->iconfg = -1;
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        p->flags |= SUB_PANEL_DOWN;
        break; /* }}} */
    }

  subPanelConfigure(p);

  subSharedLogDebugSubtle("new=panel, type=%s\n",
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
  switch(p->flags & SUB_PANEL_VIEWS)
    {
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
                p->width += v->width;
              }
          }
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
  switch(p->flags & (SUB_PANEL_ICON|SUB_PANEL_KEYCHAIN|
      SUB_PANEL_SUBLET|SUB_PANEL_TITLE|SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_ICON: /* {{{ */
        p->width = p->icon->width + subtle->padding.x + subtle->padding.y + 4;
        break; /* }}} */
      case SUB_PANEL_KEYCHAIN: /* {{{ */
        p->width = 0;

        if(p->keychain && p->keychain->keys)
          {
            /* Font offset, panel border and padding */
            p->width = subSharedTextWidth(subtle->dpy, subtle->font,
              p->keychain->keys, p->keychain->len, NULL, NULL, True) + 6 + 2 *
              subtle->pbw + subtle->padding.x + subtle->padding.y;
          }
        break; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
        /* Copy width in case of shallow copies */
        p->width = p->sublet->width;
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        p->width = 0;

        if(0 < subtle->clients->ndata)
          {
            SubClient *c = NULL;

            /* Find focus window */
            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))))
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
                  }
              }
          }
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

                p->width += v->width;
              }
          }
        break; /* }}} */
    }
} /* }}} */

 /** subPanelRender {{{
  * @brief Render panel
  * @param[in]  p         A #SubPanel
  * @param[in]  drawable  Drawable for renderer
  **/

void
subPanelRender(SubPanel *p,
  Drawable drawable)
{
  assert(p);

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_ICON|SUB_PANEL_KEYCHAIN|
      SUB_PANEL_SUBLET|SUB_PANEL_TITLE|SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_ICON: /* {{{ */
        subSharedTextIconDraw(subtle->dpy, subtle->gcs.draw,
          drawable, p->x + 2 + subtle->padding.x,
          (subtle->ph - 2 * subtle->pbw - p->icon->height) / 2 + subtle->pbw,
          p->icon->width, p->icon->height, subtle->colors.fg_sublets,
          subtle->colors.bg_sublets, p->icon->pixmap, p->icon->bitmap);
        break; /* }}} */
      case SUB_PANEL_KEYCHAIN: /* {{{ */
        if(p->keychain && p->keychain->keys)
          {
            subSharedTextDraw(subtle->dpy, subtle->gcs.draw, subtle->font,
              drawable, p->x + 3 + subtle->padding.x, subtle->font->y +
              subtle->padding.width + subtle->pbw, subtle->colors.fg_title,
              subtle->colors.bg_title, p->keychain->keys);
          }
        break; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
        /* Set window background and border*/
        XSetForeground(subtle->dpy, subtle->gcs.draw,
          subtle->colors.bo_sublets);
        XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
          p->x, 0, p->width, subtle->ph);

        XSetForeground(subtle->dpy, subtle->gcs.draw,
          subtle->colors.bg_sublets);
        XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
          p->x + subtle->pbw, subtle->pbw,
          p->width - 2 * subtle->pbw, subtle->ph - 2 * subtle->pbw);

        /* Render text parts */
        subSharedTextRender(subtle->dpy, subtle->gcs.draw, subtle->font,
          drawable, p->x + 3 + subtle->padding.x, subtle->font->y +
          subtle->padding.width + subtle->pbw, p->sublet->fg, p->sublet->bg,
          p->sublet->text);
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        if(0 < subtle->clients->ndata)
          {
            SubClient *c = NULL;

            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))))
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
                if(c->flags & SUB_CLIENT_MODE_RESIZE)
                  {
                    snprintf(buf + x, sizeof(buf), "%c", '~');
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

                /* Set window background and border*/
                XSetForeground(subtle->dpy, subtle->gcs.draw, bo);
                XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
                  p->x, 0, p->width, subtle->ph);

                XSetForeground(subtle->dpy, subtle->gcs.draw, bg);
                XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
                  p->x + subtle->pbw, subtle->pbw, p->width - 2 * subtle->pbw,
                  subtle->ph - 2 * subtle->pbw);

                subSharedTextDraw(subtle->dpy, subtle->gcs.draw, subtle->font,
                  drawable, p->x + 3 + subtle->padding.x, subtle->font->y +
                  subtle->padding.width + subtle->pbw, fg, bg, buf);
              }
          }
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        if(0 < subtle->views->ndata)
          {
            int i, px = 0;
            long fg = 0, bg = 0, bo = 0;

            /* View buttons */
            for(i = 0; i < subtle->views->ndata; i++)
              {
                int vx = subtle->padding.x + subtle->pbw + 3;
                SubView *v = VIEW(subtle->views->data[i]);

                /* Check dynamic views */
                if(v->flags & SUB_VIEW_DYNAMIC &&
                    !(subtle->client_tags & v->tags))
                  continue;

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
                XSetForeground(subtle->dpy, subtle->gcs.draw, bo);
                XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
                  p->x + px, 0, v->width, subtle->ph);

                XSetForeground(subtle->dpy, subtle->gcs.draw, bg);
                XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
                  p->x + px + subtle->pbw, subtle->pbw,
                  v->width - 2 * subtle->pbw, subtle->ph - 2 * subtle->pbw);

                /* Draw view icon and/or text */
                if(v->flags & SUB_VIEW_ICON)
                  {
                    subSharedTextIconDraw(subtle->dpy, subtle->gcs.draw,
                      drawable, p->x + px + vx, ((subtle->ph -
                      2 * subtle->pbw - v->icon->height) / 2) +
                      subtle->padding.width, v->icon->width, v->icon->height,
                      fg, bg, v->icon->pixmap, v->icon->bitmap);

                    vx += v->icon->width;
                  }

                if(!(v->flags & SUB_VIEW_ICON_ONLY))
                  {
                    if(v->flags & SUB_VIEW_ICON) vx += 3;

                    subSharedTextDraw(subtle->dpy, subtle->gcs.draw,
                      subtle->font, drawable, p->x + px + vx, subtle->font->y +
                      subtle->padding.width + subtle->pbw, fg, bg, v->name);
                  }

                px += v->width;
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
  if(!(p1->sublet->flags & (SUB_SUBLET_INTERVAL))) return 1;
  if(!(p2->sublet->flags & (SUB_SUBLET_INTERVAL))) return -1;

  return p1->sublet->time < p2->sublet->time ? -1 :
    (p1->sublet->time == p2->sublet->time ? 0 : 1);
} /* }}} */

 /** subPanelAction {{{
  * @brief Handle panel action based on type
  * @param[in]  panels  A #SubArray
  * @param[in]  type    Action type
  * @param[in]  x       Pointer X position
  * @param[in]  y       Pointer Y position
  * @param[in]  button  Pointer button
  * @param[in]  bottom  Whether bottom panel or not
  **/

void
subPanelAction(SubArray *panels,
  int type,
  int x,
  int y,
  int button,
  int bottom)
{
  int i;

  /* FIXME: In order to find the correct panel item we
   * need to check all of them in a O(n) fashion and
   * check if they belong to the top or bottom panel.
   * Adding some kind of map for the x values would
   * be nice. */

  /* Check panel items */
  for(i = 0; i < panels->ndata; i++)
    {
      SubPanel *p = PANEL(panels->data[i]);

      /* Check if x is in panel rect */
      if(p->flags & type && x >= p->x && x <= p->x + p->width)
        {
          if(bottom && !(p->flags & SUB_PANEL_BOTTOM)) continue;

          /* Handle panel item type */
          switch(p->flags & (SUB_PANEL_SUBLET|SUB_PANEL_VIEWS))
            {
              case SUB_PANEL_SUBLET: /* {{{ */
                /* Handle action type */
                switch(type)
                  {
                    case SUB_PANEL_OUT:
                      subRubyCall(SUB_CALL_OUT, p->sublet->instance, NULL);
                      break;
                    case SUB_PANEL_OVER:
                      subRubyCall(SUB_CALL_OVER, p->sublet->instance, NULL);
                      break;
                    case SUB_PANEL_DOWN:
                        {
                          int args[3] = { x - p->x, y, button };

                          subRubyCall(SUB_CALL_DOWN, p->sublet->instance,
                            (void *)&args);
                        }
                      break;
                  }

                subScreenUpdate();
                subScreenRender();
                break; /* }}} */
              case SUB_PANEL_VIEWS: /* {{{ */
                  {
                    int j, vx = p->x;

                    for(j = 0; j < subtle->views->ndata; j++)
                      {
                        SubView *v = VIEW(subtle->views->data[j]);

                        /* Check dynamic views */
                        if(v->flags & SUB_VIEW_DYNAMIC &&
                            !(subtle->client_tags & v->tags))
                          continue;

                        /* Check if x is in view rect */
                        if(x >= vx && x <= vx + v->width)
                          {
                            subViewSwitch(v, -1, False);

                            break;
                          }

                        vx += v->width;
                      }
                  }
                break; /* }}} */
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

  /* Alloc space */
  names = (char **)subSharedMemoryAlloc(subtle->sublets->ndata,
    sizeof(char *));

  /* Find sublet in panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      if(s->panels)
        {
          for(j = 0; j < s->panels->ndata; j++)
            {
              SubPanel *p = PANEL(s->panels->data[j]);

              /* Include sublets, exclude shallow copies */
              if(p->flags & SUB_PANEL_SUBLET && !(p->flags & SUB_PANEL_COPY))
                names[idx++] = p->sublet->name;
            }
        }
    }

  /* EWMH: Sublet list and windows */
  subSharedPropertySetStrings(subtle->dpy, ROOT,
    subEwmhGet(SUB_EWMH_SUBTLE_SUBLET_LIST), names, subtle->sublets->ndata);

  subSharedLogDebugSubtle("publish=panel, n=%d\n", subtle->sublets->ndata);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(names);
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
  switch(p->flags & (SUB_PANEL_COPY|SUB_PANEL_ICON|
      SUB_PANEL_KEYCHAIN|SUB_PANEL_SUBLET|SUB_PANEL_TRAY))
    {
      case SUB_PANEL_COPY: break;
      case SUB_PANEL_ICON: /* {{{ */
        if(p->icon) free(p->icon);
        break; /* }}} */
      case SUB_PANEL_KEYCHAIN: /* {{{ */
        if(p->keychain)
          {
            if(p->keychain->keys) free(p->keychain->keys);
            free(p->keychain);
            p->keychain = NULL;
            p->screen   = NULL;
          }
        return; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
        if(!(p->flags & SUB_PANEL_COPY))
          {
            /* Call unload */
            if(p->sublet->flags & SUB_SUBLET_UNLOAD)
              subRubyCall(SUB_CALL_UNLOAD, p->sublet->instance, NULL);

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

            if(p->sublet->name)
              {
                printf("Unloaded sublet (%s)\n", p->sublet->name);
                free(p->sublet->name);
              }
            if(p->sublet->text) subSharedTextFree(p->sublet->text);

            free(p->sublet);
          }
        break; /* }}} */
      case SUB_PANEL_TRAY: /* {{{ */
        /* Reparent and return to avoid beeing destroyed */
        XReparentWindow(subtle->dpy, subtle->windows.tray, ROOT, 0, 0);
        p->screen = NULL;
        return; /* }}} */
    }

  free(p);

  subSharedLogDebugSubtle("kill=panel\n");
} /* }}} */
