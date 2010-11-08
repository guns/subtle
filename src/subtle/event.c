
 /**
  * @package subtle
  *
  * @file Event functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <unistd.h>
#include <X11/Xatom.h>
#include <sys/poll.h>
#include "subtle.h"

#ifdef HAVE_SYS_INOTIFY_H
#define BUFLEN (sizeof(struct inotify_event))
#endif /* HAVE_SYS_INOTIFY_H */

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
#include <X11/extensions/Xrandr.h>
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */

struct pollfd *watches = NULL;
int nwatches = 0;

/* EventUntag {{{ */
static void
EventUntag(SubClient *c,
  int id)
{
  int i, tag;

  /* Shift bits */
  for(i = id; i < subtle->tags->ndata - 1; i++)
    {
      tag = (1L << (i + 1));

      if(c->tags & (1L << (i + 2))) ///< Next bit
        c->tags |= tag;
      else
        c->tags &= ~tag;
    }

  /* EWMH: Tags */
  subEwmhSetCardinals(c->flags & SUB_TYPE_VIEW ? VIEW(c)->win : c->win,
    SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&c->tags, 1);
} /* }}} */

/* EventFindSublet {{{ */
static SubPanel *
EventFindSublet(int id)
{
  int i = 0, idx = 0;
  SubPanel *p = NULL;

  /* Find sublet in linked list */
  for(i = 0; i < subtle->sublets->ndata; i++)
    {
      p = PANEL(subtle->sublets->data[i]);

      if(p->flags & SUB_PANEL_SUBLET && idx++ == id) break;
    }

  return p;
} /* }}} */

/* EventSwitchView {{{ */
static void
EventSwitchView(int vid,
  int focus)
{
  SubView *v = NULL;

  if(vid < subtle->views->ndata)
    {
      int i, sid = 0;
      SubScreen *sc1 = subScreenCurrent(&sid);

      if(1 < subtle->screens->ndata)
        {
          /* Check if view is visible on any screen */
          for(i = 0; i < subtle->screens->ndata; i++)
            {
              SubScreen *sc2 = SCREEN(subtle->screens->data[i]);

              /* Swap views */
              if(sc1 != sc2 && sc2->vid == vid)
                {
                  int swap = sc1->vid;
                  sc1->vid = sc2->vid;
                  sc2->vid = swap;

                  v = VIEW(subArrayGet(subtle->views, vid));

                  subScreenConfigure();
                  subScreenRender();

                  subViewFocus(v, focus);

                  /* Hook: Jump */
                  subHookCall(SUB_HOOK_VIEW_JUMP, (void *)v);

                  return;
                }
            }
        }

      /* Set view and configure */
      sc1->vid = vid;
      v        = VIEW(subArrayGet(subtle->views, vid));

      subScreenConfigure();
      subScreenRender();

      subViewFocus(v, focus);

      /* Hook: Jump */
      subHookCall(SUB_HOOK_VIEW_JUMP, (void *)v);
    }
} /* }}} */

/* EventRestack {{{ */
static void
EventRestack(SubClient *c,
  long dir)
{
  int flags = 0;

  /* Save flags */
  flags     = c->flags & (SUB_CLIENT_TYPE_DESKTOP|SUB_CLIENT_MODE_FULL);
  c->flags &= ~flags;

  if(Above == dir)
    {
      XRaiseWindow(subtle->dpy, c->win);

      /* Sort stack up */
      c->flags |= SUB_CLIENT_MODE_FULL;
      subArraySort(subtle->clients, subClientCompare);
      c->flags &= ~SUB_CLIENT_MODE_FULL;
    }
  else if(Below == dir)
    {
      XLowerWindow(subtle->dpy, c->win);

      /* Sort stack down */
      c->flags |= SUB_CLIENT_TYPE_DESKTOP;
      subArraySort(subtle->clients, subClientCompare);
      c->flags &= ~SUB_CLIENT_TYPE_DESKTOP;
    }
  else subSharedLogDebug("Ignoring unknown stacking mode `%ld'", dir);

  /* Restore flags */
  c->flags |= flags;
} /* }}} */

/* Events */

/* EventConfigureRequest {{{ */
static void
EventConfigureRequest(XConfigureRequestEvent *ev)
{
  SubClient *c = NULL;

  /* Check window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      if(!(c->flags & (SUB_CLIENT_MODE_NORESIZE|SUB_CLIENT_TYPE_DESKTOP)) &&
          (subtle->flags & SUB_SUBTLE_RESIZE ||
          c->flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_RESIZE)))
        {
          SubScreen *s = SCREEN(subtle->screens->data[c->screen]);
          XRectangle geom = c->geom;

          /* We restrict this from graviated clients */
          if(c->flags & SUB_CLIENT_MODE_FLOAT)
            {
              if(ev->value_mask & CWX) geom.x = s->geom.x + ev->x;
              if(ev->value_mask & CWY) geom.y = s->geom.y + ev->y;
            }

          if(ev->value_mask & CWWidth)  geom.width  = ev->width;
          if(ev->value_mask & CWHeight) geom.height = ev->height;

          /* Check new values */
          if(RECTINRECT(geom, s->geom))
            {
              c->geom = geom;

              /* Resize client */
              subScreenFit(s, &c->geom);

              subClientConfigure(c);
            }
        }
    }
  else ///< Unmanaged windows
    {
      XWindowChanges wc;

      wc.x            = ev->x;
      wc.y            = ev->y;
      wc.width        = ev->width;
      wc.height       = ev->height;
      wc.border_width = subtle->bw;
      wc.sibling      = ev->above;
      wc.stack_mode   = ev->detail;

      XConfigureWindow(subtle->dpy, ev->window, ev->value_mask, &wc);
    }
} /* }}} */

/* EventConfigure {{{ */
static void
EventConfigure(XConfigureEvent *ev)
{
  /* Ckeck window */
  if(ROOT == ev->window)
    {
      if(subtle->flags & SUB_SUBTLE_XRANDR)
        {
#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
          XRRUpdateConfiguration((XEvent *)ev);
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */
        }

      /* Reload screens */
      subArrayClear(subtle->sublets, False);
      subArrayClear(subtle->screens, True);
      subScreenInit();
      subRubyReloadConfig();
      subScreenResize();

      printf("Updated screens\n");
    }
} /* }}} */

/* EventMapRequest {{{ */
static void
EventMapRequest(XMapRequestEvent *ev)
{
  SubClient *c = NULL;

  /* Check if client exists */
  if(!(c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      if((c = subClientNew(ev->window))) ///< Create new client
        {
          subArrayPush(subtle->clients, (void *)c);
          subClientPublish();

          /* Check visibility first */
          if(VISIBLE(subtle->visible_tags, c))
            {
              /* Hook: Tile */
              subHookCall(SUB_HOOK_TILE, NULL);
            }

          if(c->flags & SUB_CLIENT_TYPE_DESKTOP) ///< Reorder stacking
            subArraySort(subtle->clients, subClientCompare);

          subScreenConfigure();
          subScreenRender();

          /* Hook: Create */
          subHookCall(SUB_HOOK_CLIENT_CREATE, (void *)c);
        }
    }
} /* }}} */

/* EventMap {{{ */
static void
EventMap(XMapEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  /* Ignored because we use XUnmapWindow too */
  if(ev->window != ev->event && True != ev->send_event) return;

  /* Check if we know this window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      subEwmhSetWMState(c->win, NormalState);
    }
  else if((t = TRAY(subSubtleFind(ev->window, TRAYID))))
    {
      subEwmhSetWMState(t->win, NormalState);
      subEwmhMessage(t->win, t->win, SUB_EWMH_XEMBED, CurrentTime,
        XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
    }
} /* }}} */

/* EventUnmap {{{ */
static void
EventUnmap(XUnmapEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  /* Ignored because we use XUnmapWindow too */
  if(ev->window != ev->event && True != ev->send_event) return;

  /* Check if we know this window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID)))) ///< Client
    {
      subEwmhSetWMState(c->win, WithdrawnState);

      /* Ignore our generated unmap events */
      if(c->flags & SUB_CLIENT_UNMAP)
        {
          c->flags &= ~SUB_CLIENT_UNMAP;

          return;
        }

      subArrayRemove(subtle->clients, (void *)c);
      subClientPublish();
      subClientKill(c, False);
      subScreenConfigure();
      subScreenRender();
    }
  else if((t = TRAY(subSubtleFind(ev->window, TRAYID)))) ///< Tray
    {
      subEwmhSetWMState(t->win, WithdrawnState);

      /* Ignore out own generated unmap events */
      if(t->flags & SUB_TRAY_UNMAP)
        {
          t->flags &= ~SUB_TRAY_UNMAP;
          return;
        }

      subArrayRemove(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
      subTrayPublish();
      subScreenUpdate();
      subScreenRender();
    }
} /* }}} */

/* EventDestroy {{{ */
static void
EventDestroy(XDestroyWindowEvent *ev)
{
  int focus = (subtle->windows.focus == ev->window); ///< Save
  SubClient *c = NULL;
  SubTray *t = NULL;

  /* Check if we know this window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID)))) ///< Client
    {
      c->flags |= SUB_CLIENT_DEAD; ///< Ignore remaining events
      subArrayRemove(subtle->clients, (void *)c);
      subClientPublish();
      subClientKill(c, True);
      subScreenConfigure();
      subScreenRender();
    }
  else if((t = TRAY(subSubtleFind(ev->event, TRAYID)))) ///< Tray
    {
      subArrayRemove(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
      subTrayPublish();
      subScreenUpdate();
      subScreenRender();
    }

  /* Update focus if necessary */
  if(focus) subSubtleFocus(True);
} /* }}} */

/* EventMessage {{{ */
static void
EventMessage(XClientMessageEvent *ev)
{
  SubClient *c = NULL;

  if(32 != ev->format) return; ///< Just skip

  /* Messages for root window {{{ */
  if(ROOT == ev->window)
    {
      int tag = 0, id = subEwmhFind(ev->message_type);

      SubPanel   *p = NULL;
      SubTag     *t = NULL;
      SubTray    *r = NULL;
      SubView    *v = NULL;
      SubGravity *g = NULL;

      switch(id)
        {
          case SUB_EWMH_NET_CURRENT_DESKTOP: /* {{{ */
            if(0 <= ev->data.l[0] && ev->data.l[0] < subtle->views->ndata)
              {
                if((v = VIEW(subtle->views->data[ev->data.l[0]])))
                  subViewJump(v);
              }
            break; /* }}} */
          case SUB_EWMH_NET_ACTIVE_WINDOW: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[0], CLIENTID))))
              {
                if(!(VISIBLE(subtle->visible_tags, c))) ///< Client is on current view?
                  {
                    int i;

                    /* Find matching view */
                    for(i = 0; i < subtle->views->ndata; i++)
                      {
                        if(VISIBLE(VIEW(subtle->views->data[i])->tags, c))
                          {
                            subViewJump(VIEW(subtle->views->data[i]));
                            break;
                          }
                      }
                  }

                subClientWarp(c);
                subClientFocus(c);
              }
            else if((r = TRAY(subSubtleFind(ev->data.l[0], TRAYID))))
              subTrayFocus(r);
            break; /* }}} */
          case SUB_EWMH_NET_RESTACK_WINDOW: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[1], CLIENTID))))
              EventRestack(c, ev->data.l[2]);
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_TAG:
          case SUB_EWMH_SUBTLE_WINDOW_UNTAG: /* {{{ */
            if(0 <= ev->data.l[1] && subtle->tags->ndata > ev->data.l[1])
              {
                tag = (1L << (ev->data.l[1] + 1));

                switch(ev->data.l[2]) ///< Type
                  {
                    case 0: ///< Clients
                      if((c = CLIENT(subArrayGet(subtle->clients,
                          (int)ev->data.l[0])))) ///< Clients
                        {
                          int visible = 0;

                          /* Update tags and properties */
                          if(SUB_EWMH_SUBTLE_WINDOW_TAG == id)
                            {
                              int flags = subClientTag(c, ev->data.l[1]);

                              subClientToggle(c, flags, True); ///< Toggle flags
                            }
                          else c->tags &= ~tag;

                          /* Check visibility after updating tags */
                          visible = VISIBLE(subtle->visible_tags, c);

                          /* EWMH: Tags */
                          subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_TAGS,
                            (long *)&c->tags, 1);

                          if(visible)
                            {
                              /* Reactivate grabs on untag */
                              if(subtle->windows.focus == c->win &&
                                  !(c->tags & tag))
                                subSubtleFocus(True);

                              /* Hook: Tile */
                              subHookCall(SUB_HOOK_TILE, NULL);
                            }

                          subScreenConfigure();
                          subScreenRender();
                        }
                      break;
                    case 1: ///< Views
                      if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0])))) ///< Views
                        {
                          int visible = !!(subtle->visible_views & (1L << (ev->data.l[0] + 1)));

                          if(SUB_EWMH_SUBTLE_WINDOW_TAG == id) v->tags |= tag; ///< Action
                          else v->tags &= ~tag;

                          /* EWMH: Tags */
                          subEwmhSetCardinals(v->win, SUB_EWMH_SUBTLE_WINDOW_TAGS,
                            (long *)&v->tags, 1);

                          if(visible) subScreenConfigure();
                        }
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_RETAG: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients,
                (int)ev->data.l[0])))) ///< Clients
              {
                int flags = 0;

                c->tags = 0; ///> Reset tags

                subClientSetTags(c, &flags);
                subClientToggle(c, (~c->flags & flags), True); ///< Toggle flags

                if(VISIBLE(subtle->visible_tags, c)) subScreenConfigure();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_GRAVITY: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))) &&
                ((g = GRAVITY(subArrayGet(subtle->gravities, (int)ev->data.l[1])))) &&
                VISIBLE(subtle->visible_tags, c))
              {
                subClientSetGravity(c, (int)ev->data.l[1], c->screen, True);
                subClientConfigure(c);
                subClientWarp(c);
                XRaiseWindow(subtle->dpy, c->win);

                /* Hook: Tile */
                subHookCall(SUB_HOOK_TILE, NULL);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_FLAGS: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))))
              {
                int flags = 0;

                /* Translate flags */
                if(ev->data.l[1] & SUB_EWMH_FULL)  flags |= SUB_CLIENT_MODE_FULL;
                if(ev->data.l[1] & SUB_EWMH_FLOAT) flags |= SUB_CLIENT_MODE_FLOAT;
                if(ev->data.l[1] & SUB_EWMH_STICK) flags |= SUB_CLIENT_MODE_STICK;

                subClientToggle(c, flags, True);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_RESIZE: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))))
              {
                if(True == ev->data.l[1]) c->flags &= ~SUB_CLIENT_MODE_NORESIZE;
                else c->flags |= SUB_CLIENT_MODE_NORESIZE;
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_GRAVITY_NEW: /* {{{ */
            if(ev->data.b)
              {
                XRectangle geom = { 0 };
                char buf[30] = { 0 };

                sscanf(ev->data.b, "%hdx%hd+%hd+%hd#%s", &geom.x, &geom.y,
                  &geom.width, &geom.height, buf);

                /* Add gravity */
                g = subGravityNew(buf, &geom);

                subArrayPush(subtle->gravities, (void *)g);
                subGravityPublish();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_GRAVITY_KILL: /* {{{ */
            if((g = GRAVITY(subArrayGet(subtle->gravities, (int)ev->data.l[0]))))
              {
                int i;

                /* Check clients if gravity is in use */
                for(i = 0; i < subtle->clients->ndata; i++)
                  {
                    if((c = CLIENT(subtle->clients->data[i])) && c->gravity == ev->data.l[0])
                      {
                        subClientSetGravity(c, 0, -1, True); ///< Fallback to first gravity
                        subClientConfigure(c);
                        subClientWarp(c);
                        XRaiseWindow(subtle->dpy, c->win);
                      }
                  }

                /* Finallly remove gravity */
                subArrayRemove(subtle->gravities, (void *)g);
                subGravityKill(g);
                subGravityPublish();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_NEW: /* {{{ */
            if(ev->data.b && (t = subTagNew(ev->data.b, NULL)))
              {
                subArrayPush(subtle->tags, (void *)t);
                subTagPublish();

                /* Hook: Create */
                subHookCall(SUB_HOOK_TAG_CREATE, (void *)t);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_KILL: /* {{{ */
            if((t = TAG(subArrayGet(subtle->tags, (int)ev->data.l[0]))))
              {
                int i, reconf = False;

                /* Untag views */
                for(i = 0; i < subtle->views->ndata; i++) ///< Views
                  {
                    v      = VIEW(subtle->views->data[i]);
                    reconf = v->tags & (1L << ((int)ev->data.l[0] + 1));

                    EventUntag(CLIENT(v), (int)ev->data.l[0]);
                  }

                /* Untag clients */
                for(i = 0; i < subtle->clients->ndata; i++)
                  EventUntag(CLIENT(subtle->clients->data[i]),
                    (int)ev->data.l[0]);

                /* Remove tag */
                subArrayRemove(subtle->tags, (void *)t);
                subTagKill(t);
                subTagPublish();

                if(reconf) subScreenConfigure();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TRAY_KILL: /* {{{ */
            if((r = TRAY(subArrayGet(subtle->trays, (int)ev->data.l[0]))))
              {
                subArrayRemove(subtle->trays, (void *)r);
                subTrayKill(r);
                subTrayPublish();
                subTrayUpdate();
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_NEW: /* {{{ */
            if(ev->data.b && (v = subViewNew(ev->data.b, NULL)))
              {
                subArrayPush(subtle->views, (void *)v);
                subClientDimension(-1); ///< Grow
                subPanelDimension(-1); ///< Grow
                subViewPublish();
                subScreenUpdate();
                subScreenRender();

                /* Hook: Create */
                subHookCall(SUB_HOOK_VIEW_CREATE, (void *)v);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_KILL: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0]))))
              {
                int visible = !!(subtle->visible_views & (1L << (ev->data.l[0] + 1)));

                subArrayRemove(subtle->views, (void *)v);
                subClientDimension((int)ev->data.l[0]); ///< Shrink
                subPanelDimension((int)ev->data.l[0]); ///< Shrink
                subViewKill(v);
                subViewPublish();
                subScreenUpdate();
                subScreenRender();

                if(visible) subViewJump(VIEW(subtle->views->data[0]));
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_NEW: /* {{{ */
            if(ev->data.b)
              {
                subRubyLoadSublet(ev->data.b);
                subArraySort(subtle->sublets, subPanelCompare);
                subPanelPublish();
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_DATA: /* {{{ */
            if((p = EventFindSublet((int)ev->data.b[0])))
              {
                p->width = subSharedTextParse(subtle->dpy, subtle->font,
                  p->sublet->text, ev->data.b + 1) + 2 * subtle->pbw;
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_FOREGROUND: /* {{{ */
            if((p = EventFindSublet((int)ev->data.l[0])))
              {
                p->sublet->fg = ev->data.l[1];
                subPanelRender(p);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_BACKGROUND: /* {{{ */
            if((p = EventFindSublet((int)ev->data.l[0])))
              {
                p->sublet->bg = ev->data.l[1];
                subPanelRender(p);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_UPDATE: /* {{{ */
            if((p = EventFindSublet((int)ev->data.l[0])))
              {
                subRubyCall(SUB_CALL_RUN, p->sublet->instance, NULL);
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_KILL: /* {{{ */
            if((p = EventFindSublet((int)ev->data.l[0])))
              {
                int i;

                /* Remove sublet from panels to avoid overhead */
                for(i = 0; i < subtle->screens->ndata; i++)
                  {
                    subArrayRemove(SCREEN(subtle->screens->data[i])->panels,
                      (void *)p);
                  }

                /* Remove hooks */
                for(i = 0; i < subtle->hooks->ndata; i++)
                  {
                    SubHook *hook = HOOK(subtle->hooks->data[i]);

                    if(hook->flags & SUB_CALL_HOOKS &&
                        subRubyReceiver(p->sublet->instance, hook->proc))
                      {
                        subArrayRemove(subtle->hooks, (void *)hook);
                        subRubyRelease(hook->proc);
                        subHookKill(hook);
                        i--; ///< Prevent skipping of entries
                      }
                  }

                /* Remove grabs */
                for(i = 0; i < subtle->grabs->ndata; i++)
                  {
                    SubGrab *grab = GRAB(subtle->grabs->data[i]);

                    if(grab->flags & SUB_GRAB_PROC &&
                        subRubyReceiver(p->sublet->instance, grab->data.num))
                      {
                        subArrayRemove(subtle->grabs, (void *)grab);
                        subRubyRelease(grab->data.num);
                        subGrabKill(grab);
                        i--; ///< Prevent skipping of entries
                      }
                  }

                subArrayRemove(subtle->sublets, (void *)p);
                subPanelKill(p);
                subPanelPublish();
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_RELOAD: /* {{{ */
            subRubyReloadConfig();
            break; /* }}} */
          case SUB_EWMH_SUBTLE_RESTART: /* {{{ */
            if(subtle) 
              {
                subtle->flags &= ~SUB_SUBTLE_RUN;
                subtle->flags |= SUB_SUBTLE_RESTART;
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_QUIT: /* {{{ */
            if(subtle) subtle->flags &= ~SUB_SUBTLE_RUN;
            break; /* }}} */
        }
    } /* }}} */
  /* Messages for tray window {{{ */
  else if(ev->window == subtle->windows.tray.win)
    {
      SubTray *t = NULL;
      int id = subEwmhFind(ev->message_type);

      switch(id)
        {
          case SUB_EWMH_NET_SYSTEM_TRAY_OPCODE: /* {{{ */
            switch(ev->data.l[1])
              {
                case XEMBED_EMBEDDED_NOTIFY:
                  if(!(t = TRAY(subSubtleFind(ev->window, TRAYID))))
                    {
                      t = subTrayNew(ev->data.l[2]);
                      subArrayPush(subtle->trays, (void *)t);
                      subTrayPublish();
                      subTrayUpdate();
                      subScreenUpdate();
                      subScreenRender();
                    }
                  break;
                case XEMBED_REQUEST_FOCUS:
                  subEwmhMessage(t->win, t->win, SUB_EWMH_XEMBED, CurrentTime,
                    XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0);
                  break;
              }
            break; /* }}} */
        }
    } /* }}} */
  /* Messages for client windows {{{ */
  else if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))));
    {
      DEAD(c);

      switch(subEwmhFind(ev->message_type))
        {
          /* Actions: 0 = remove / 1 = add / 2 = toggle - we always toggle */
          case SUB_EWMH_NET_WM_STATE: /* {{{ */
            switch(subEwmhFind(ev->data.l[1])) ///< Only the first property
              {
                case SUB_EWMH_NET_WM_STATE_FULLSCREEN:
                  subClientToggle(c, SUB_CLIENT_MODE_FULL, True);
                  break;
                case SUB_EWMH_NET_WM_STATE_ABOVE:
                  subClientToggle(c, SUB_CLIENT_MODE_FLOAT, True);
                  break;
                case SUB_EWMH_NET_WM_STATE_STICKY:
                  subClientToggle(c, SUB_CLIENT_MODE_STICK, True);
                  break;
                default: break;
              }

            if(VISIBLE(subtle->visible_tags, c)) subScreenConfigure();
            break; /* }}} */
          case SUB_EWMH_NET_CLOSE_WINDOW: /* {{{ */
            subArrayRemove(subtle->clients, (void *)c);
            subClientPublish();
            subClientKill(c, True);
            subScreenConfigure();
            subScreenRender();
            break; /* }}} */
          case SUB_EWMH_NET_MOVERESIZE_WINDOW: /* {{{ */
            if(!(c->flags & SUB_CLIENT_MODE_FLOAT))
              subClientToggle(c, SUB_CLIENT_MODE_FLOAT, True);

            c->geom.x      = ev->data.l[1];
            c->geom.y      = ev->data.l[2];
            c->geom.width  = ev->data.l[3];
            c->geom.height = ev->data.l[4];

            subClientResize(c);
            subClientConfigure(c);
            break; /* }}} */
          default: break;
        }
    } /* }}} */

#ifdef DEBUG
  {
    char *name = XGetAtomName(subtle->dpy, ev->message_type);

    subSharedLogDebug("ClientMessage: name=%s, type=%ld, format=%d, win=%#lx\n",
      name ? name : "n/a", ev->message_type, ev->format, ev->window);
    subSharedLogDebug("ClientMessage: [0]=%#lx, [1]=%#lx, [2]=%#lx, [3]=%#lx, [4]=%#lx\n",
      ev->data.l[0], ev->data.l[1], ev->data.l[2], ev->data.l[3], ev->data.l[4]);

    if(name) XFree(name);
  }
#endif /* DEBUG */
} /* }}} */

/* EventColormap {{{ */
static void
EventColormap(XColormapEvent *ev)
{
  SubClient *c = (SubClient *)subSubtleFind(ev->window, 1);
  if(c && ev->new)
    {
      c->cmap = ev->colormap;
      XInstallColormap(subtle->dpy, c->cmap);
    }
} /* }}} */

/* EventProperty {{{ */
static void
EventProperty(XPropertyEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;
  int id = subEwmhFind(ev->atom);

  if(XA_WM_NAME == ev->atom) id = SUB_EWMH_WM_NAME;

  /* Supported properties */
  switch(id)
    {
      case SUB_EWMH_WM_NAME: /* {{{ */
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          {
            if(c->name) free(c->name);
            subSharedPropertyName(subtle->dpy, c->win, &c->name, c->klass);

            if(subtle->windows.focus == c->win)
              {
                subScreenUpdate();
                subScreenRender();
              }
          }
        break; /* }}} */
      case SUB_EWMH_WM_NORMAL_HINTS: /* {{{ */
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          {
            int flags = 0;

            subClientSetSizeHints(c, &flags);
            subClientToggle(c, (~c->flags & flags), True);
          }
        else if((t = TRAY(subSubtleFind(ev->window, TRAYID))))
          {
            subTrayConfigure(t);
            subTrayUpdate();
            subScreenUpdate();
            subScreenRender();
          }
        break; /* }}} */
      case SUB_EWMH_WM_HINTS: /* {{{ */
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          {
            int flags = 0;

            /* Check changes */
            subClientSetWMHints(c, &flags);
            subClientToggle(c, (~c->flags & flags), True);

            if(c->flags & (SUB_CLIENT_MODE_URGENT|SUB_CLIENT_MODE_URGENT_FOCUS))
              subViewHighlight(c->tags);
          }
        break; /* }}} */
      case SUB_EWMH_NET_WM_STRUT: /* {{{ */
         if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          {
            subClientSetStrut(c);
            subScreenUpdate();
            subSharedLogDebug("Hints: Updated strut hints\n");
          }
        break; /* }}} */
      case SUB_EWMH_XEMBED_INFO: /* {{{ */
        if((t = TRAY(subSubtleFind(ev->window, TRAYID))))
          {
            subTraySetState(t);
            subTrayUpdate();
            subScreenRender();
          }
        break; /* }}} */
    }
} /* }}} */

/* EventCrossing {{{ */
static void
EventCrossing(XCrossingEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;
  SubPanel *p = NULL;

  /* Handle both crossing events */
  switch(ev->type)
    {
      case EnterNotify:
        /* Handle crossing event */
        if(ROOT == ev->window) ///< Root
          {
            subGrabSet(ROOT, !(subtle->flags & SUB_SUBTLE_ESCAPE));
          }
        else if((c = CLIENT(subSubtleFind(ev->window, CLIENTID)))) ///< Client
          {
            if(!(c->flags & SUB_CLIENT_DEAD)) subClientFocus(c);
          }
        else if((t = TRAY(subSubtleFind(ev->window, TRAYID)))) ///< Tray
          {
            subTrayFocus(t);
          }
        else if((p = PANEL(subSubtleFind(ev->window, SUBLETID)))) ///< Sublet
          {
            if(p->flags & SUB_PANEL_OVER)
              subRubyCall(SUB_CALL_OVER, p->sublet->instance, NULL);
          }
        break;
      case LeaveNotify:
        if((p = PANEL(subSubtleFind(ev->window, SUBLETID)))) ///< Sublet
          {
            if(p->flags & SUB_PANEL_OUT)
              subRubyCall(SUB_CALL_OUT, p->sublet->instance, NULL);
          }
    }
} /* }}} */

/* EventSelection {{{ */
void
EventSelection(XSelectionClearEvent *ev)
{
  /* Handle selection clear events */
  if(ev->window == subtle->windows.tray.win) ///< Tray selection
    {
      subSharedLogDebug("SelectionClear: type=tray, win=%#lx\n", ev->window);
      subTraySelect();
    }
  else if(ev->window == subtle->windows.support) ///< Session selection
    {
      subSharedLogDebug("SelectionClear: type=session, win=%#lx\n", ev->window);
      subSharedLogWarn("Quitting the field\n");
      subtle->flags &= ~SUB_SUBTLE_RUN; ///< Exit
    }

  subSharedLogDebug("SelectionClear: win=%#lx, tray=%#lx, support=%#lx\n",
    ev->window, subtle->windows.tray.win, subtle->windows.support);
} /* }}} */

/* EventExpose {{{ */
static void
EventExpose(XExposeEvent *ev)
{
  if(0 == ev->count) subScreenRender(); ///< Render once
} /* }}} */

/* EventGrab {{{ */
static void
EventGrab(XEvent *ev)
{
  SubGrab *g = NULL;
  SubClient *c = NULL;
  SubPanel *p = NULL;
  SubView *v = NULL;
  unsigned int code = 0, mod = 0;

  /* Distinct types {{{ */
  switch(ev->type)
    {
      case ButtonPress:
        if((v = VIEW(subSubtleFind(ev->xbutton.window, VIEWID))))
          {
            EventSwitchView(subArrayIndex(subtle->views, (void *)v), False);

            return;
          }
        else if((p = PANEL(subSubtleFind(ev->xbutton.window, SUBLETID))))
          {
            if(p->flags & SUB_PANEL_DOWN) ///< Call click method
              {
                subRubyCall(SUB_CALL_DOWN, p->sublet->instance,
                  (void *)&ev->xbutton);
                subScreenUpdate();
                subScreenRender();
               }

            return;
          }

        code = XK_Pointer_Button1 + ev->xbutton.button;
        mod  = ev->xbutton.state;
        break;
      case KeyPress:
        code = ev->xkey.keycode;
        mod  = ev->xkey.state;
        break;
    } /* }}} */

  /* Find grab */
  if((g = subGrabFind(code, mod)))
    {
      Window win = 0;
      FLAGS flag = 0;

      win  = ev->xbutton.window == ROOT ? ev->xbutton.subwindow : ev->xbutton.window;
      flag = g->flags & ~(SUB_TYPE_GRAB|SUB_GRAB_KEY|SUB_GRAB_MOUSE); ///< Clear mask

      switch(flag)
        {
          case SUB_GRAB_SPAWN: /* {{{ */
            if(g->data.string) subSharedSpawn(g->data.string);
            break; /* }}} */
          case SUB_GRAB_PROC: /* {{{ */
            subRubyCall(SUB_CALL_HOOKS, g->data.num,
              subSubtleFind(win, CLIENTID));
            break; /* }}} */
          case SUB_GRAB_WINDOW_MOVE:
          case SUB_GRAB_WINDOW_RESIZE: /* {{{ */
            if((c = CLIENT(subSubtleFind(win, CLIENTID))) &&
                !(c->flags & (SUB_CLIENT_MODE_FULL|SUB_CLIENT_MODE_NOFLOAT)))
              {
                if(!(c->flags & SUB_CLIENT_MODE_FLOAT))
                  subClientToggle(c, SUB_CLIENT_MODE_FLOAT, True);

                /* Translate flags */
                if(SUB_GRAB_WINDOW_MOVE == flag)        flag = SUB_DRAG_MOVE;
                else if(SUB_GRAB_WINDOW_RESIZE == flag) flag = SUB_DRAG_RESIZE;

                subClientDrag(c, flag);
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_TOGGLE: /* {{{ */
            if((c = CLIENT(subSubtleFind(win, CLIENTID))))
              {
                subClientToggle(c, g->data.num, True);
                if(VISIBLE(subtle->visible_tags, c)) subScreenConfigure();
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_STACK: /* {{{ */
            if((c = CLIENT(subSubtleFind(win, CLIENTID))) &&
                !(c->flags & SUB_CLIENT_TYPE_DESKTOP) && VISIBLE(subtle->visible_tags, c))
              EventRestack(c, g->data.num);
            break; /* }}} */
          case SUB_GRAB_WINDOW_SELECT: /* {{{ */
            if((c = CLIENT(subSubtleFind(win, CLIENTID))))
              {
                int i, match = (1L << 30), score = 0;
                SubClient *found = NULL;

                /* Iterate once to find a client with smallest score */
                for(i = 0; i < subtle->clients->ndata; i++)
                  {
                    SubClient *iter = CLIENT(subtle->clients->data[i]);

                    /* Check if client and gravity are different */
                    if(c != iter && c->gravity != iter->gravity)
                      {
                        /* Substract stack position to get window on top */
                        if(match > (score = subSharedMatch(g->data.num,
                            &c->geom, &iter->geom) - i))
                          {
                            match = score;
                            found = iter;
                          }
                      }
                  }

                if(found)
                  {
                    subClientWarp(found);
                    subClientFocus(found);

                    subSharedLogDebug("Match: win=%#lx, score=%d\n",
                      found->win, match);
                  }
              }
            else ///< Select the first if no client has focus
              {
                int i;

                for(i = 0; i < subtle->clients->ndata; i++)
                  {
                    SubClient *iter = CLIENT(subtle->clients->data[i]);

                    if(VISIBLE(subtle->visible_tags, iter))
                      {
                        subClientFocus(iter);
                        subClientWarp(iter);
                        break;
                      }
                  }
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_GRAVITY: /* {{{ */
            if((c = CLIENT(subSubtleFind(win, CLIENTID))) &&
                !(c->flags & SUB_CLIENT_TYPE_DESKTOP))
              {
                int i, id = -1, cid = 0, fid = (int)g->data.string[0] - 65,
                  size = strlen(g->data.string);

                /* Remove float/fullscreen mode */
                if(c->flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FULL))
                  subClientToggle(c, c->flags &
                    (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FULL), True);

                /* Select next gravity */
                for(i = 0; -1 == id && i < size; i++)
                  {
                    cid = (int)g->data.string[i] - 65;

                    /* Toggle gravity */
                    if(c->gravity == cid)
                      {
                        /* Select first or next id */
                        if(i == size - 1) id = fid;
                        else id = (int)g->data.string[i + 1] - 65;
                      }
                  }
                if(-1 == id) id = fid; ///< Fallback

                /* Sanity check */
                if(0 <= id && id < subtle->gravities->ndata)
                  {
                    subClientSetGravity(c, id, c->screen, True);
                    subClientConfigure(c);
                    subClientWarp(c);
                    XRaiseWindow(subtle->dpy, c->win);

                    /* Hook: Tile */
                    subHookCall(SUB_HOOK_TILE, NULL);
                  }
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_KILL: /* {{{ */
            if((c = CLIENT(subSubtleFind(win, CLIENTID))))
              {
                subArrayRemove(subtle->clients, (void *)c);
                subClientPublish();
                if(VISIBLE(subtle->visible_tags, c)) subScreenConfigure();
                subClientKill(c, True);
              }
            break; /* }}} */
          case SUB_GRAB_VIEW_JUMP: /* {{{ */
            if(g->data.num < subtle->views->ndata)
              subViewJump(VIEW(subtle->views->data[g->data.num]));
            break; /* }}} */
          case SUB_GRAB_VIEW_SWITCH: /* {{{ */
            if(g->data.num < subtle->views->ndata)
              EventSwitchView(g->data.num, True);
            break; /* }}} */
          case SUB_GRAB_VIEW_SELECT: /* {{{ */
              {
                SubScreen *screen = subScreenCurrent(NULL);

                /* Select view */
                if(SUB_VIEW_NEXT == g->data.num &&
                    screen->vid < (subtle->views->ndata - 1))
                  {
                    EventSwitchView(screen->vid + 1, True);
                  }
                else if(SUB_VIEW_PREV == g->data.num && 0 < screen->vid)
                  EventSwitchView(screen->vid - 1, True);
              }
            break; /* }}} */
          case SUB_GRAB_SCREEN_JUMP: /* {{{ */
            if(g->data.num < subtle->screens->ndata)
              subScreenJump(SCREEN(subtle->screens->data[g->data.num]));
            break; /* }}} */
          case SUB_GRAB_SUBTLE_RELOAD: /* {{{ */
            subRubyReloadConfig();
            break; /* }}} */
          case SUB_GRAB_SUBTLE_QUIT: /* {{{ */
            if(subtle) subtle->flags &= ~SUB_SUBTLE_RUN;
            break; /* }}} */
          case SUB_GRAB_SUBTLE_RESTART: /* {{{ */
            if(subtle)
              {
                subtle->flags &= ~SUB_SUBTLE_RUN;
                subtle->flags |= SUB_SUBTLE_RESTART;
              }
            break; /* }}} */
          case SUB_GRAB_SUBTLE_ESCAPE: /* {{{ */
            subGrabSet(0 != win ? win : ROOT, True);
            break; /* }}} */
          default:
            subSharedLogWarn("Failed finding grab!\n");
        }

      subSharedLogDebug("Grab: code=%03d, mod=%03d\n", g->code, g->mod);
    }
} /* }}} */

/* EventFocus {{{ */
static void
EventFocus(XFocusChangeEvent *ev)
{
  SubClient *c = NULL;

  /* Check if window keeps focus */
  if(ev->window == subtle->windows.focus) return;

  /* Check if client is visible */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    if(!(VISIBLE(subtle->visible_tags, c))) return;

  /* Remove focus */
  subGrabUnset(subtle->windows.focus);
  if((c = CLIENT(subSubtleFind(subtle->windows.focus, CLIENTID))))
    {
      subtle->windows.focus = 0;
      subClientRender(c);

      /* Remove urgent after losing focus */
      if(c->flags & SUB_CLIENT_MODE_URGENT_FOCUS)
        {
          c->flags &= ~(SUB_CLIENT_MODE_URGENT|SUB_CLIENT_MODE_URGENT_FOCUS);

          subViewHighlight(0);
          subScreenConfigure();
        }
    }

  /* Handle focus event */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID)))) ///< Clients
    {
      if(!(c->flags & SUB_CLIENT_DEAD) && VISIBLE(subtle->visible_tags, c))
        {
          SubScreen *s = NULL;
          SubView *v = NULL;

          subtle->windows.focus = c->win;
          subGrabSet(c->win, !(subtle->flags & SUB_SUBTLE_ESCAPE));

          subClientRender(c);

          /* EWMH: Active window */
          subEwmhSetWindows(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW,
            &subtle->windows.focus, 1);

          /* EWMH: Current desktop */
          if((s = SCREEN(subArrayGet(subtle->screens, c->screen))))
            {
              subEwmhSetCardinals(ROOT, SUB_EWMH_NET_CURRENT_DESKTOP,
                (long *)&s->vid, 1);
            }

          /* Set view focus */
          if((v = VIEW(subArrayGet(subtle->views, s->vid))))
            v->focus = c->win;;

          /* Hook: Focus */
          subHookCall(SUB_HOOK_CLIENT_FOCUS, (void *)c);

          subScreenUpdate();
          subScreenRender();
        }
    }
} /* }}} */

/* Extern */

 /** subEventWatchAdd {{{
  * @brief Add descriptor to watch list
  * @param[in]  fd  File descriptor
  **/

void
subEventWatchAdd(int fd)
{
  /* Add descriptor to list */
  watches = (struct pollfd *)subSharedMemoryRealloc(watches,
    (nwatches + 1) * sizeof(struct pollfd));

  watches[nwatches].fd        = fd;
  watches[nwatches].events    = POLLIN;
  watches[nwatches++].revents = 0;
} /* }}} */

 /** subEventWatchDel {{{
  * @brief Del fd from watch list
  * @param[in]  fd  File descriptor
  **/

void
subEventWatchDel(int fd)
{
  int i, j;

  for(i = 0; i < nwatches; i++)
    {
      if(watches[i].fd == fd)
        {
          for(j = i; j < nwatches - 1; j++)
            watches[j] = watches[j + 1];
          break;
        }
    }

  nwatches--;
  watches = (struct pollfd *)subSharedMemoryRealloc(watches,
    nwatches * sizeof(struct pollfd));
} /* }}} */

 /** subEventLoop {{{
  * @brief Event all X events
  **/

void
subEventLoop(void)
{
  int i, timeout = 1, events = 0;
  XEvent ev;
  time_t now;
  SubPanel *p = NULL;

#ifdef HAVE_SYS_INOTIFY_H
  char buf[BUFLEN];
#endif /* HAVE_SYS_INOTIFY_H */

  subScreenRender(); ///< Initially render panels
  subEventWatchAdd(ConnectionNumber(subtle->dpy));

#ifdef HAVE_SYS_INOTIFY_H
  subEventWatchAdd(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */

  XSync(subtle->dpy, False); ///< Sync before going on

  subtle->flags |= SUB_SUBTLE_RUN;

  while(subtle && subtle->flags & SUB_SUBTLE_RUN)
    {
      now = subSubtleTime();

      /* Data ready on any connection */
      if(0 < (events = poll(watches, nwatches, timeout * 1000)))
        {
          for(i = 0; i < nwatches; i++) ///< Find descriptor
            {
              if(0 != watches[i].revents)
                {
                  if(watches[i].fd == ConnectionNumber(subtle->dpy)) ///< X events {{{
                    {
                      while(XPending(subtle->dpy)) ///< X events
                        {
                          XNextEvent(subtle->dpy, &ev);
                          switch(ev.type)
                            {
                              case ConfigureRequest:  EventConfigureRequest(&ev.xconfigurerequest); break;
                              case ConfigureNotify:   EventConfigure(&ev.xconfigure);               break;
                              case MapRequest:        EventMapRequest(&ev.xmaprequest);             break;
                              case MapNotify:         EventMap(&ev.xmap);                           break;
                              case UnmapNotify:       EventUnmap(&ev.xunmap);                       break;
                              case DestroyNotify:     EventDestroy(&ev.xdestroywindow);             break;
                              case ClientMessage:     EventMessage(&ev.xclient);                    break;
                              case ColormapNotify:    EventColormap(&ev.xcolormap);                 break;
                              case PropertyNotify:    EventProperty(&ev.xproperty);                 break;
                              case SelectionClear:    EventSelection(&ev.xselectionclear);          break;
                              case Expose:            EventExpose(&ev.xexpose);                     break;
                              case FocusIn:           EventFocus(&ev.xfocus);                       break;
                              case EnterNotify:
                              case LeaveNotify:       EventCrossing(&ev.xcrossing);                 break;
                              case ButtonPress:
                              case KeyPress:          EventGrab(&ev);                               break;
                              default: break;
                            }
                        }
                    } /* }}} */
#ifdef HAVE_SYS_INOTIFY_H
                  else if(watches[i].fd == subtle->notify) ///< Inotify {{{
                    {
                      if(0 < read(subtle->notify, buf, BUFLEN)) ///< Inotify events
                        {
                          struct inotify_event *event = (struct inotify_event *)&buf[0];

                          /* Skip unwatch events */
                          if(event && IN_IGNORED != event->mask)
                            {
                              if((p = PANEL(subSubtleFind(
                                  subtle->windows.support, event->wd))))
                                {
                                  subRubyCall(SUB_CALL_WATCH,
                                    p->sublet->instance, NULL);
                                  subScreenUpdate();
                                  subScreenRender();
                                }
                            }
                        }
                    } /* }}} */
#endif /* HAVE_SYS_INOTIFY_H */
                  else ///< Socket {{{
                    {
                      if((p = PANEL(subSubtleFind(subtle->windows.support,
                          watches[i].fd))))
                        {
                          subRubyCall(SUB_CALL_WATCH,
                            p->sublet->instance, NULL);
                          subScreenUpdate();
                          subScreenRender();
                        }
                    } /* }}} */
                }
            }
        }
      else if(0 == events) ///< Timeout waiting for data or error {{{
        {
          if(0 < subtle->sublets->ndata)
            {
              p = PANEL(subtle->sublets->data[0]);

              while(p && p->flags & SUB_PANEL_INTERVAL && p->sublet->time <= now)
                {
                  subRubyCall(SUB_CALL_RUN, p->sublet->instance, NULL);

                  if(p->flags & SUB_PANEL_INTERVAL) ///< This may change in run
                    {
                      p->sublet->time  = now + p->sublet->interval; ///< Adjust seconds
                      p->sublet->time -= p->sublet->time % p->sublet->interval;
                    }

                  subArraySort(subtle->sublets, subPanelCompare);
                }

              subScreenUpdate();
              subScreenRender();
            }
        } /* }}} */

      /* Set new timeout */
      if(0 < subtle->sublets->ndata)
        {
          p = PANEL(subtle->sublets->data[0]);
          timeout = p->flags & SUB_PANEL_INTERVAL ? p->sublet->time - now : 60;
          if(0 >= timeout) timeout = 1; ///< Sanitize
        }
      else timeout = 60;
    }
} /* }}} */

 /** subEventFinish {{{
  * @brief Finish event processing
  **/

void
subEventFinish(void)
{

  if(watches) free(watches);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
