
 /**
  * @package subtle
  *
  * @file Panel functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtle.h"

/* PanelRect {{{ */
static void
PanelRect(Drawable drawable,
  int x,
  int width,
  SubStyle *s)
{
  int mw = s->margin.left + s->margin.right;
  int mh = s->margin.top + s->margin.bottom;

  /* Filling */
  XSetForeground(subtle->dpy, subtle->gcs.draw, s->bg);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + s->margin.left, s->margin.top, width - mw, subtle->ph - mh);

  /* Borders */
  XSetForeground(subtle->dpy, subtle->gcs.draw, s->top);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + s->margin.left, s->margin.top, width - mw, s->border.top);

  XSetForeground(subtle->dpy, subtle->gcs.draw, s->right);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + width - s->border.right - s->margin.right, s->margin.top,
    s->border.right, subtle->ph - mh);

  XSetForeground(subtle->dpy, subtle->gcs.draw, s->bottom);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + s->margin.left, subtle->ph - s->border.bottom - s->margin.bottom,
    width - mw, s->border.bottom);

  XSetForeground(subtle->dpy, subtle->gcs.draw, s->left);
  XFillRectangle(subtle->dpy, drawable, subtle->gcs.draw,
    x + s->margin.left, s->margin.top, s->border.left, subtle->ph - mh);
} /* }}} */

/* PanelSeparator {{{ */
static void
PanelSeparator(int type,
  SubPanel *p,
  Drawable drawable)
{
  int x = 0;

  if(SUB_PANEL_SEPARATOR1 == type)
    x = p->x - subtle->separator.width;
  else x = p->x + p->width;

  /* Set window background and border*/
  PanelRect(drawable, x, subtle->separator.width, &subtle->styles.separator);

  subSharedTextDraw(subtle->dpy, subtle->gcs.draw, subtle->font,
    drawable, x + STYLE_LEFT(subtle->styles.separator), subtle->font->y +
    STYLE_TOP(subtle->styles.separator), subtle->styles.separator.fg,
    subtle->styles.separator.bg, subtle->separator.string);
} /* }}} */

/* Public */

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
        p->sublet->fg     = subtle->styles.sublets.fg;
        p->sublet->bg     = subtle->styles.sublets.bg;
        p->sublet->textfg = -1;
        p->sublet->iconfg = -1;
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        p->flags |= SUB_PANEL_DOWN;
        break; /* }}} */
    }

  subSharedLogDebugSubtle("new=panel, type=%s\n",
    SUB_PANEL_VIEWS == type ? "views" : "title");

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
  switch(p->flags & (SUB_PANEL_ICON|SUB_PANEL_KEYCHAIN|
      SUB_PANEL_SUBLET|SUB_PANEL_TITLE|SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_ICON: /* {{{ */
        p->width = p->icon->width + subtle->styles.separator.padding.left +
          subtle->styles.separator.padding.right + 4;
        break; /* }}} */
      case SUB_PANEL_KEYCHAIN: /* {{{ */
        p->width = 0;

        if(p->keychain && p->keychain->keys)
          {
            /* Font offset, panel border and padding */
            p->width = subSharedTextWidth(subtle->dpy, subtle->font,
              p->keychain->keys, p->keychain->len, NULL, NULL, True) +
              subtle->styles.separator.padding.left + subtle->styles.separator.padding.right;
          }
        break; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
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
                    unsigned long len = strlen(c->name);

                    /* Title modes */
                    if(c->flags & SUB_CLIENT_MODE_FULL)   len++;
                    if(c->flags & SUB_CLIENT_MODE_FLOAT)  len++;
                    if(c->flags & SUB_CLIENT_MODE_STICK)  len++;
                    if(c->flags & SUB_CLIENT_MODE_RESIZE) len++;

                    /* Font offset, panel border and padding */
                    p->width = subSharedTextWidth(subtle->dpy, subtle->font,
                      c->name, subtle->styles.clients.right >= len ? len :
                      subtle->styles.clients.right, NULL, NULL, True) +
                      STYLE_WIDTH(subtle->styles.title);
                  }
              }
          }
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        p->width = 0;

        if(0 < subtle->views->ndata)
          {
            int i;
            SubStyle *s = NULL;

            /* Update for each view */
            for(i = 0; i < subtle->views->ndata; i++)
              {
                SubView *v = VIEW(subtle->views->data[i]);

                /* Check dynamic views */
                if(v->flags & SUB_VIEW_DYNAMIC &&
                    !(subtle->client_tags & v->tags))
                  continue;

                /* Select style */
                if(subtle->urgent_tags & v->tags)
                  s = &subtle->styles.urgent;
                else if(p->screen->vid == i)
                  s = &subtle->styles.focus;
                else if(subtle->client_tags & v->tags)
                  s = &subtle->styles.occupied;
                else s = &subtle->styles.views;

                p->width += v->width + STYLE_WIDTH((*s));
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

  /* Draw separator before panel */
  if(0 < subtle->separator.width && p->flags & SUB_PANEL_SEPARATOR1)
    PanelSeparator(SUB_PANEL_SEPARATOR1, p, drawable);

  /* Handle panel item type */
  switch(p->flags & (SUB_PANEL_ICON|SUB_PANEL_KEYCHAIN|
      SUB_PANEL_SUBLET|SUB_PANEL_TITLE|SUB_PANEL_VIEWS))
    {
      case SUB_PANEL_ICON: /* {{{ */
          {
            int y = 0, icony = 0;

            y     = subtle->font->y + STYLE_TOP(subtle->styles.separator);
            icony = p->icon->height > y ? subtle->styles.separator.margin.top :
              y - p->icon->height;

            subSharedTextIconDraw(subtle->dpy, subtle->gcs.draw,
              drawable, p->x + 2 + subtle->styles.separator.padding.left, icony,
              p->icon->width, p->icon->height, subtle->styles.sublets.fg,
              subtle->styles.sublets.bg, p->icon->pixmap, p->icon->bitmap);
          }
        break; /* }}} */
      case SUB_PANEL_KEYCHAIN: /* {{{ */
        if(p->keychain && p->keychain->keys)
          {
            subSharedTextDraw(subtle->dpy, subtle->gcs.draw, subtle->font,
              drawable, p->x + STYLE_LEFT(subtle->styles.separator),
              subtle->font->y + STYLE_TOP(subtle->styles.separator),
              subtle->styles.title.fg, subtle->styles.title.bg,
              p->keychain->keys);
          }
        break; /* }}} */
      case SUB_PANEL_SUBLET: /* {{{ */
        /* Set window background and border*/
        PanelRect(drawable, p->x, p->width, &subtle->styles.sublets);

        /* Render text parts */
        subSharedTextRender(subtle->dpy, subtle->gcs.draw, subtle->font,
          drawable, p->x + STYLE_LEFT(subtle->styles.sublets), subtle->font->y +
          STYLE_TOP(subtle->styles.sublets), p->sublet->fg, p->sublet->bg,
          p->sublet->text);
        break; /* }}} */
      case SUB_PANEL_TITLE: /* {{{ */
        if(0 < subtle->clients->ndata)
          {
            SubClient *c = NULL;

            if((c = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))) &&
                !(c->flags & SUB_CLIENT_TYPE_DESKTOP))
              {
                int x = 0;
                char buf[100] = { 0 };
                SubStyle *s = NULL;

                DEAD(c);

                /* Title modes {{{ */
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
                  } /* }}} */

                snprintf(buf + x, sizeof(buf) - x, "%s", c->name);

                /* Limit size */
                if(x + subtle->styles.clients.right < sizeof(buf))
                  buf[x + subtle->styles.clients.right] = '\0';

                /* Add urgent colors */
                if(c->flags & SUB_CLIENT_MODE_URGENT)
                  s = &subtle->styles.urgent;
                else s = &subtle->styles.title;

                /* Set window background and border*/
                PanelRect(drawable, p->x, p->width, s);

                subSharedTextDraw(subtle->dpy, subtle->gcs.draw, subtle->font,
                  drawable, p->x + STYLE_LEFT(subtle->styles.title), subtle->font->y +
                  STYLE_TOP(subtle->styles.title), s->fg, s->bg, buf);
              }
          }
        break; /* }}} */
      case SUB_PANEL_VIEWS: /* {{{ */
        if(0 < subtle->views->ndata)
          {
            int i, vx = p->x;
            SubStyle *s = NULL;

            /* View buttons */
            for(i = 0; i < subtle->views->ndata; i++)
              {
                int x = 0;
                SubView *v = VIEW(subtle->views->data[i]);

                /* Check dynamic views */
                if(v->flags & SUB_VIEW_DYNAMIC &&
                    !(subtle->client_tags & v->tags))
                  continue;

                /* Select style */
                if(subtle->urgent_tags & v->tags)
                  s = &subtle->styles.urgent;
                else if(p->screen->vid == i)
                  s = &subtle->styles.focus;
                else if(subtle->client_tags & v->tags)
                  s = &subtle->styles.occupied;
                else s = &subtle->styles.views;

                /* Set window background and border*/
                PanelRect(drawable, vx, v->width + STYLE_WIDTH((*s)), s);

                x += STYLE_LEFT((*s));

                /* Draw view icon and/or text */
                if(v->flags & SUB_VIEW_ICON)
                  {
                    int y = 0, icony = 0;

                    y     = subtle->font->y + STYLE_TOP((*s));
                    icony = v->icon->height > y ? s->margin.top :
                      y - v->icon->height;

                    subSharedTextIconDraw(subtle->dpy, subtle->gcs.draw,
                      drawable, vx + x, icony, v->icon->width,
                      v->icon->height, s->fg, s->bg, v->icon->pixmap,
                      v->icon->bitmap);
                  }

                if(!(v->flags & SUB_VIEW_ICON_ONLY))
                  {
                    if(v->flags & SUB_VIEW_ICON) x += v->icon->width + 3;

                    subSharedTextDraw(subtle->dpy, subtle->gcs.draw,
                      subtle->font, drawable, vx + x, subtle->font->y +
                      STYLE_TOP((*s)), s->fg, s->bg, v->name);
                  }

                vx += v->width + STYLE_WIDTH((*s));
              }
          }
        break; /* }}} */
    }

  /* Draw separator after panel */
  if(0 < subtle->separator.width && p->flags & SUB_PANEL_SEPARATOR2)
    PanelSeparator(SUB_PANEL_SEPARATOR2, p, drawable);
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
          /* Check if action is for bottom panel */
          if((bottom && !(p->flags & SUB_PANEL_BOTTOM)) ||
            (!bottom && p->flags & SUB_PANEL_BOTTOM)) continue;

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
                    SubStyle *s = NULL;

                    for(j = 0; j < subtle->views->ndata; j++)
                      {
                        SubView *v = VIEW(subtle->views->data[j]);
                        int swidth = 0;

                        /* Check dynamic views */
                        if(v->flags & SUB_VIEW_DYNAMIC &&
                            !(subtle->client_tags & v->tags))
                          continue;

                        /* Select style */
                        if(subtle->urgent_tags & v->tags)
                          s = &subtle->styles.urgent;
                        else if(p->screen->vid == i)
                          s = &subtle->styles.focus;
                        else if(subtle->client_tags & v->tags)
                          s = &subtle->styles.occupied;
                        else s = &subtle->styles.views;

                        swidth = v->width + STYLE_WIDTH((*s)); ///< Get width with style

                        /* Check if x is in view rect */
                        if(x >= vx && x <= vx + swidth)
                          {
                            subViewSwitch(v, -1, False);

                            break;
                          }

                        vx += swidth;
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
