
 /**
  * @package subtle
  *
  * @file Event functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
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

/* Globals */
struct pollfd *watches = NULL;
XClientMessageEvent *queue = NULL;
int nwatches = 0, nqueue = 0;

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
  if(c->flags & SUB_TYPE_CLIENT)
    {
      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_TAGS,
        (long *)&c->tags, 1);
    }
} /* }}} */

/* EventFindSublet {{{ */
static SubPanel *
EventFindSublet(int id)
{
  int i = 0, j = 0, idx = 0;

  /* Find sublet in panels */
  for(i = 0; i < subtle->screens->ndata; i++)
    {
      SubScreen *s = SCREEN(subtle->screens->data[i]);

      for(j = 0; j < s->panels->ndata; j++)
        {
          SubPanel *p = PANEL(s->panels->data[j]);

          if(p->flags & SUB_PANEL_SUBLET &&
              !(p->flags & SUB_PANEL_COPY) && idx++ == id)
            return p;
        }
    }

  return NULL;
} /* }}} */

/* EventSwitchView {{{ */
static void
EventSwitchView(int vid,
  int sid,
  int focus)
{
  SubView *v = NULL;

  if(vid < subtle->views->ndata)
    {
      int i;
      SubScreen *sc1 = NULL;

      /* Get working screen */
      if(!(sc1 = subArrayGet(subtle->screens, sid)))
        sc1 = subScreenCurrent(NULL);

      /* Check if there is only one screen */
      if(1 < subtle->screens->ndata)
        {
          /* Check if view is visible on any screen */
          for(i = 0; i < subtle->screens->ndata; i++)
            {
              SubScreen *sc2 = SCREEN(subtle->screens->data[i]);

              if(sc1 != sc2 && sc2->vid == vid)
                {
                  int swap = 0;

                  /* Swap views */
                  swap     = sc1->vid;
                  sc1->vid = sc2->vid;
                  sc2->vid = swap;

                  v = VIEW(subArrayGet(subtle->views, vid));

                  break;
                }
            }
        }

      if(!v)
        {
          /* Set view and configure */
          sc1->vid = vid;
          v        = VIEW(subArrayGet(subtle->views, vid));
        }

      subScreenConfigure();
      subScreenRender();
      subScreenPublish();

      subViewFocus(v, focus);

      /* Hook: Jump */
      subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_FOCUS),
        (void *)v);
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

/* EventQueuePush {{{ */
static void
EventQueuePush(XClientMessageEvent *ev)
{
  /* Since we are dealing with race conditions we need to cache
   * client messages when a client/view/tag isn't read yet */
  queue = (XClientMessageEvent *)subSharedMemoryRealloc(queue,
    (nqueue + 1) * sizeof(XClientMessageEvent));
  queue[nqueue] = *ev;
  nqueue++;

  subSharedLogDebug("Queue: Store WINDOW_TAG id=%ld, tags=%ld, type=%ld\n",
    ev->data.l[0], ev->data.l[1], ev->data.l[2]);
} /* }}} */

/* EventQueuePop {{{ */
static void
EventQueuePop(int id,
  int type)
{
  /* Check queue */
  if(0 < nqueue)
    {
      int i;

      /* Check queued events */
      for(i = 0; i < nqueue; i++)
        {
          XClientMessageEvent ev = queue[i];

          /* Check if element id matches event */
          if(ev.data.l[0] == id && ev.data.l[2] == type)
            {
              int j;

              /* Sort queue */
              for(j = i; j < nqueue - 1; j++)
                queue[j] = queue[j + 1];

              nqueue--;

              queue = (XClientMessageEvent *)subSharedMemoryRealloc(queue,
                nqueue * sizeof(XClientMessageEvent));

              XPutBackEvent(subtle->dpy, (XEvent *)&ev);

              subSharedLogDebug("Queue: Putback WINDOW_TAG id=%ld, tags=%ld, type=%ld\n",
                ev.data.l[0], ev.data.l[1], ev.data.l[2]);

              i--;
            }
        }
    }
} /* }}} */

/* subSharedMatch {{{ */
static int
EventMatch(int type,
  XRectangle *origin,
  XRectangle *test)
{
  int dx = 0, dy = 0;

  /* This check is complicated and consists of two parts:
   * 1) Check if x/y values decrease in given direction
   * 2) Check if a corner of one of the rects is close enough to
   *    a side of the other rect */

  /* Check geometries */
  if((((SUB_WINDOW_LEFT  == type      && test->x   <= origin->x)                  ||
       (SUB_WINDOW_RIGHT == type      && test->x   >= origin->x))                 &&
       ((test->y         >= origin->y && test->y   <= origin->y + origin->height) ||
       (origin->y        >= test->y   && origin->y <= test->y   + test->height))) ||

     (((SUB_WINDOW_UP    == type      && test->y   <= origin->y)                  ||
       (SUB_WINDOW_DOWN  == type      && test->y   >= origin->y))                 &&
       ((test->x         >= origin->x && test->x   <= origin->x + origin->width)  ||
       (origin->x        >= test->x   && origin->x <= test->x   + test->width))))
    {
      /* Euclidean distance */
      dx = abs(origin->x - test->x);
      dy = abs(origin->y - test->y);

      /* Zero distance means same dimensions - score this bad */
      if(0 == dx && 0 == dy) dx = dy = 1L << 15;
    }
  else
    {
      /* No match - score bad as well */
      dx = 1L << 15;
      dy = 1L << 15;
    }

  return dx + dy;
} /* }}} */

/* Events */

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

  subSharedLogDebugEvents("Colormap: win=%#lx\n", ev->window);
} /* }}} */

/* EventConfigure {{{ */
static void
EventConfigure(XConfigureEvent *ev)
{
  /* Ckeck window */
  if(ROOT == ev->window)
    {
      int sw = DisplayWidth(subtle->dpy, DefaultScreen(subtle->dpy));
      int sh = DisplayHeight(subtle->dpy, DefaultScreen(subtle->dpy));

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
      /* Update RandR config */
      if(subtle->flags & SUB_SUBTLE_XRANDR)
        XRRUpdateConfiguration((XEvent *)ev);
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */

      /* Skip event if screen size doesn't change */
      if(subtle->width == sw && subtle->height == sh) return;

      /* Reload screens */
      subArrayClear(subtle->sublets, False);
      subArrayClear(subtle->screens, True);
      subScreenInit();

      subRubyReloadConfig();
      subScreenResize();

      /* Update size */
      subtle->width  = sw;
      subtle->height = sh;

      printf("Updated screens\n");
    }

  subSharedLogDebugEvents("Configure: win=%#lx\n", ev->window);
} /* }}} */

/* EventConfigureRequest {{{ */
static void
EventConfigureRequest(XConfigureRequestEvent *ev)
{
  SubClient *c = NULL;

  /* Complicated request! (see ICCCM 4.1.5)
   * No change    -> synthetic ConfigureNotify
   * Move/restack -> real + synthetic ConfigureNotify
   * Resize       -> real ConfigureNotify */

  /* Check window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      if(!(c->flags & (SUB_CLIENT_MODE_NORESIZE|SUB_CLIENT_TYPE_DESKTOP|
          SUB_CLIENT_MODE_FLOAT)) && (subtle->flags & SUB_SUBTLE_RESIZE ||
          c->flags & (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_RESIZE)))
        {
          SubScreen *s = SCREEN(subtle->screens->data[c->screen]);

          if(ev->value_mask & CWX)      c->geom.x      = s->geom.x + ev->x;
          if(ev->value_mask & CWY)      c->geom.y      = s->geom.y + ev->y;
          if(ev->value_mask & CWWidth)  c->geom.width  = ev->width;
          if(ev->value_mask & CWHeight) c->geom.height = ev->height;

          subClientResize(c, False);

          /* Send synthetic configure notify */
          if((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
            subClientConfigure(c);

          /* Send real configure notify */
          XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
            c->geom.width, c->geom.height);
        }
      else subClientConfigure(c);
    }
  else ///< Unmanaged windows
    {
      XWindowChanges wc;

      wc.x            = ev->x;
      wc.y            = ev->y;
      wc.width        = ev->width;
      wc.height       = ev->height;
      wc.border_width = 0;
      wc.sibling      = ev->above;
      wc.stack_mode   = ev->detail;

      XConfigureWindow(subtle->dpy, ev->window, ev->value_mask, &wc);
    }

  subSharedLogDebugEvents("ConfigureRequest: win=%#lx\n", ev->window);
} /* }}} */

/* EventCrossing {{{ */
static void
EventCrossing(XCrossingEvent *ev)
{
  SubClient *c = NULL;
  SubPanel *p = NULL;

  /* Handle both crossing events */
  switch(ev->type)
    {
      case EnterNotify:
        /* Handle crossing event */
        if(ROOT == ev->window) ///< Root
          {
            subGrabSet(ROOT);
          }
        else if((c = CLIENT(subSubtleFind(ev->window, CLIENTID)))) ///< Client
          {
            if(ALIVE(c)) subClientFocus(c);
          }
        else if((p = PANEL(subSubtleFind(ev->window, SUBLETID)))) ///< Sublet
          {
            if(p->sublet->flags & SUB_SUBLET_OVER)
              {
                subRubyCall(SUB_CALL_OVER, p->sublet->instance, NULL);
                subScreenUpdate();
                subScreenRender();
              }
          }
        break;
      case LeaveNotify:
        if((p = PANEL(subSubtleFind(ev->window, SUBLETID)))) ///< Sublet
          {
            if(p->sublet->flags & SUB_SUBLET_OUT)
              {
                subRubyCall(SUB_CALL_OUT, p->sublet->instance, NULL);
                subScreenUpdate();
                subScreenRender();
              }
          }
    }

  subSharedLogDebugEvents("Enter: win=%#lx\n", ev->window);
} /* }}} */

/* EventDestroy {{{ */
static void
EventDestroy(XDestroyWindowEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  int focus = (subtle->windows.focus[0] == ev->window); ///< Save

  /* Check if we know this window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID)))) ///< Client
    {
      /* Ignore remaining events */
      c->flags |= SUB_CLIENT_DEAD;
      XSelectInput(subtle->dpy, c->win, NoEventMask);

      /* Kill client */
      subArrayRemove(subtle->clients, (void *)c);
      subClientKill(c);
      subClientPublish();

      subScreenConfigure();
      subScreenUpdate();
      subScreenRender();

      /* Update focus if necessary */
      if(focus) subSubtleFocus(True);
    }
  else if((t = TRAY(subSubtleFind(ev->event, TRAYID)))) ///< Tray
    {
      /* Kill tray */
      subArrayRemove(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
      subTrayPublish();

      subScreenUpdate();
      subScreenRender();

      /* Update focus if necessary */
      if(focus) subSubtleFocus(True);
    }

  subSharedLogDebugEvents("Destroy: win=%#lx\n", ev->window);
} /* }}} */

/* EventExpose {{{ */
static void
EventExpose(XExposeEvent *ev)
{
  if(0 == ev->count) subScreenRender(); ///< Render once

  subSharedLogDebugEvents("Expose: win=%#lx\n", ev->window);
} /* }}} */

/* EventFocus {{{ */
static void
EventFocus(XFocusChangeEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  /* Check if we can skip this event */
  if(ev->window == subtle->windows.focus[0]) return;

  /* Check if focus client is known, alive and visible */
  if(((c = CLIENT(subSubtleFind(ev->window, CLIENTID))) &&
      ALIVE(c) && VISIBLE(subtle->visible_tags, c)) ||
      (t = TRAY(subSubtleFind(ev->window, TRAYID))))
    {
      SubClient *focus = NULL;

      /* Unset current focus */
      subGrabUnset(subtle->windows.focus[0]);
      if((focus = CLIENT(subSubtleFind(subtle->windows.focus[0], CLIENTID))))
        {
          int i;

          /* Reorder focus history */
          for(i = (HISTORYSIZE - 1); i > 0; i--)
            subtle->windows.focus[i] = subtle->windows.focus[i - 1];
          subtle->windows.focus[0] = 0;

          subClientRender(focus);
        }

      /* Handle focus event */
      if(c) ///< Clients
        {
          SubScreen *s = NULL;
          SubView *v = NULL;

          /* Update focus */
          subtle->windows.focus[0] = c->win;
          subGrabSet(c->win);
          subClientRender(c);

          /* EWMH: Active window */
          subEwmhSetWindows(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW,
            subtle->windows.focus, HISTORYSIZE);

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
          subHookCall((SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_FOCUS),
            (void *)c);
        }
      else if(t) ///< Trays
        {
          subtle->windows.focus[0] = t->win;
          subGrabSet(t->win);
        }

      /* Update screen */
      subScreenUpdate();
      subScreenRender();
    }

  subSharedLogDebugEvents("Focus: %#lx\n", ev->window);
} /* }}} */

/* EventGrab {{{ */
static void
EventGrab(XEvent *ev)
{
  int chained = -1; ///< Not found
  unsigned int code = 0, state = 0;
  SubGrab *g = NULL;
  SubClient *c = NULL;
  SubPanel *p = NULL;
  SubView *v = NULL;
  Window win = 0;
  KeySym sym = None;

  /* Distinct types {{{ */
  switch(ev->type)
    {
      case ButtonPress:
        if((v = VIEW(subSubtleFind(ev->xbutton.window, VIEWID))))
          {
            EventSwitchView(subArrayIndex(subtle->views,
              (void *)v), -1, False);

            return;
          }
        else if((p = PANEL(subSubtleFind(ev->xbutton.window, SUBLETID))))
          {
            if(p->sublet->flags & SUB_SUBLET_DOWN) ///< Call click method
              {
                subRubyCall(SUB_CALL_DOWN, p->sublet->instance,
                  (void *)&ev->xbutton);
                subScreenUpdate();
                subScreenRender();
               }

            return;
          }

        code  = XK_Pointer_Button1 + ev->xbutton.button - 1; ///< Build button number
        state = ev->xbutton.state;

        subSharedLogDebugEvents("Grab: type=mouse, win=%#lx\n",
          ev->xbutton.window);
        break;
      case KeyPress:
        code  = ev->xkey.keycode;
        state = ev->xkey.state;

        subSharedLogDebugEvents("Grab: type=key, keycode=%d, state=%d\n",
          ev->xkey.keycode, ev->xkey.state);
        break;
    } /* }}} */

  /* Find grab */
  g   = subGrabFind(code, state);
  win = ROOT == ev->xbutton.window ? ev->xbutton.subwindow : ev->xbutton.window;
  win = 0 == win ? ROOT : win;

  /* Check chain end {{{ */
  if(subtle->keychain)
    {
      int modifier = False;

      /* Check if key is just a modifier */
      if(!g)
        {
          sym      = XLookupKeysym(&(ev->xkey), 0);
          modifier = IsModifierKey(sym);

          if(modifier) return;
        }
      else chained = subArrayIndex(subtle->keychain->keys, (void *)g);

      /* Break chain on end or invalid link */
      if((!g && !modifier) || -1 == chained || !g->keys)
        {
          free(subtle->panels.keychain.keychain->keys);
          subtle->panels.keychain.keychain->keys = NULL;
          subtle->panels.keychain.keychain->len  = 0;
          subtle->keychain                       = NULL;

          subScreenUpdate();
          subScreenRender();

          /* Restore binds */
          subGrabUnset(win);
          subGrabSet(win);

          if(-1 == chained) return;
        }
    } /* }}} */

  /* Handle grab */
  if(g)
    {
      FLAGS flag = g->flags & ~(SUB_TYPE_GRAB|SUB_GRAB_KEY|SUB_GRAB_MOUSE|
        SUB_GRAB_CHAIN_START|SUB_GRAB_CHAIN_LINK|SUB_GRAB_CHAIN_END); ///< Clear mask

      subSharedLogDebug("chain_start=%ld, chain_link=%ld, chained=%d, "
        "subtle->keychain=%p, g->keys=%p, flag=%d\n",
        g->flags & SUB_GRAB_CHAIN_START, g->flags & SUB_GRAB_CHAIN_LINK,
        chained, subtle->keychain, g->keys, flag);

      /* Handle key chains {{{ */
      if(g->keys && (-1 != chained || g->flags & SUB_GRAB_CHAIN_START))
        {
          /* Update keychain panel if in use */
          if(subtle->panels.keychain.keychain)
            {
              char *key = NULL, buf[12] = { 0 };
              int len = 0, pos = 0;

              /* Convert event to key */
              sym = XLookupKeysym(&(ev->xkey), 0);
              key = XKeysymToString(sym);

              /* Append space before each key */
              if(0 < subtle->panels.keychain.keychain->len) buf[pos++] = ' ';

              /* Translate states to string {{{ */
              if(state & ShiftMask)
                {
                  snprintf(buf + pos, sizeof(buf) - pos, "%s", "S-");
                  pos += 2;
                }
              if(state & ControlMask)
                {
                  snprintf(buf + pos, sizeof(buf) - pos, "%s", "C-");
                  pos += 2;
                }
              if(state & Mod1Mask)
                {
                  snprintf(buf + pos, sizeof(buf) - pos, "%s", "A-");
                  pos += 2;
                }
              if(state & Mod3Mask)
                {
                  snprintf(buf + pos, sizeof(buf) - pos, "%s", "M-");
                  pos += 2;
                }
              if(state & Mod4Mask)
                {
                  snprintf(buf + pos, sizeof(buf) - pos, "%s", "W-");
                  pos += 2;
                } /* }}} */

              /* Assemble chain string */
              len += strlen(buf) + strlen(key);

              subtle->panels.keychain.keychain->keys = subSharedMemoryRealloc(
                subtle->panels.keychain.keychain->keys, (len +
                subtle->panels.keychain.keychain->len + 1) * sizeof(char));

              sprintf(subtle->panels.keychain.keychain->keys +
                subtle->panels.keychain.keychain->len, "%s%s", buf, key);

              subtle->panels.keychain.keychain->len += len;
              subtle->keychain                       = g;

              subScreenUpdate();
              subScreenRender();
            }

          /* Bind any keys to exit chain on invalid link */
          subGrabUnset(win);
          XGrabKey(subtle->dpy, AnyKey, AnyModifier, win, True,
            GrabModeAsync, GrabModeAsync);

          return;
        } /* }}} */

      /* Select action */
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
                  {
                    subClientToggle(c, SUB_CLIENT_MODE_FLOAT, True);
                    subScreenUpdate();
                    subScreenRender();
                  }

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
                if(VISIBLE(subtle->visible_tags, c) ||
                    SUB_CLIENT_MODE_STICK == g->data.num)
                  {
                    subScreenConfigure();

                    if(!VISIBLE(subtle->visible_tags, c))
                      subSubtleFocus(True);

                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_STACK: /* {{{ */
            if((c = CLIENT(subSubtleFind(win, CLIENTID))) &&
                !(c->flags & SUB_CLIENT_TYPE_DESKTOP) &&
                VISIBLE(subtle->visible_tags, c))
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
                    SubClient *k = CLIENT(subtle->clients->data[i]);

                    /* Check if both clients are different and visible */
                    if(c != k && (subtle->visible_tags & k->tags ||
                        k->flags & SUB_CLIENT_MODE_STICK))
                      {
                        /* Substract stack position index to get window
                         * on top of sorted stack */
                        score = EventMatch(g->data.num, &c->geom,
                          &k->geom) - i;

                        if(match > score)
                          {
                            match = score;
                            found = k;
                          }
                      }
                  }

                if(found)
                  {
                    subClientWarp(found, True);
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
                        subClientWarp(iter, True);
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
                  {
                    subClientToggle(c, c->flags &
                      (SUB_CLIENT_MODE_FLOAT|SUB_CLIENT_MODE_FULL), True);
                    subScreenUpdate();
                    subScreenRender();

                    c->gravity = -1; ///< Reset
                  }

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
                    subClientArrange(c, id, c->screen);
                    subClientWarp(c, True);
                    XRaiseWindow(subtle->dpy, c->win);

                    /* Hook: Tile */
                    subHookCall(SUB_HOOK_TILE, NULL);
                  }
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_KILL: /* {{{ */
            if((c = CLIENT(subSubtleFind(win, CLIENTID))))
              subClientClose(c);
            break; /* }}} */
          case SUB_GRAB_VIEW_JUMP: /* {{{ */
            if(g->data.num < subtle->views->ndata)
              subViewJump(VIEW(subtle->views->data[g->data.num]));
            break; /* }}} */
          case SUB_GRAB_VIEW_SWITCH: /* {{{ */
            if(g->data.num < subtle->views->ndata)
              EventSwitchView(g->data.num, -1, True);
            break; /* }}} */
          case SUB_GRAB_VIEW_SELECT: /* {{{ */
              {
                SubScreen *screen = subScreenCurrent(NULL);

                /* Select view */
                if(SUB_VIEW_NEXT == g->data.num)
                  {
                    int vid = 0;

                    /* Cycle if necessary */
                    if(screen->vid < (subtle->views->ndata - 1))
                      vid = screen->vid + 1;
                    else vid = 0;

                    EventSwitchView(vid, -1, True);
                  }
                else if(SUB_VIEW_PREV == g->data.num)
                  {
                    int vid = 0;

                    /* Cycle if necessary */
                    if(0 < screen->vid) vid = screen->vid - 1;
                    else vid = subtle->views->ndata - 1;

                    EventSwitchView(vid, -1, True);
                  }
              }
            break; /* }}} */
          case SUB_GRAB_SCREEN_JUMP: /* {{{ */
            if(g->data.num < subtle->screens->ndata)
              subScreenJump(SCREEN(subtle->screens->data[g->data.num]));
            break; /* }}} */
          case SUB_GRAB_SUBTLE_RELOAD: /* {{{ */
            if(subtle) subtle->flags |= SUB_SUBTLE_RELOAD;
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
          default:
            subSharedLogWarn("Failed finding grab action!\n");
        }
    }
} /* }}} */

/* EventMap {{{ */
static void
EventMap(XMapEvent *ev)
{
  SubTray *t = NULL;

  /* Check if we know the window */
  if((t = TRAY(subSubtleFind(ev->window, TRAYID)))) ///< Tray
    {
      t->flags &= ~SUB_TRAY_DEAD;

      subTrayUpdate();
      subScreenConfigure();
      subScreenRender();
    }

  subSharedLogDebugEvents("Map: win=%#lx\n", ev->window);
} /* }}} */

/* EventMapRequest {{{ */
static void
EventMapRequest(XMapRequestEvent *ev)
{
  SubClient *c = NULL;

  /* Check if we know the window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      c->flags &= ~SUB_CLIENT_DEAD;
      c->flags |= SUB_CLIENT_ARRANGE;

      subScreenConfigure();
      subScreenUpdate();
      subScreenRender();
    }
  else if((c = subClientNew(ev->window)))
    {
      subArrayPush(subtle->clients, (void *)c);
      subClientPublish();

      /* Reorder stacking */
      if(c->flags & SUB_CLIENT_TYPE_DESKTOP)
        subArraySort(subtle->clients, subClientCompare);

      subScreenConfigure();
      subScreenUpdate();
      subScreenRender();

      EventQueuePop(subtle->clients->ndata - 1, 0);

      /* Hook: Create */
      subHookCall((SUB_HOOK_TYPE_CLIENT|SUB_HOOK_ACTION_CREATE),
        (void *)c);
    }

  subSharedLogDebugEvents("MapRequest: win=%#lx\n", ev->window);
} /* }}} */

/* EventMessage {{{ */
static void
EventMessage(XClientMessageEvent *ev)
{
  SubClient *c = NULL;

  /* Messages for root window {{{ */
  if(ROOT == ev->window)
    {
      int id = subEwmhFind(ev->message_type);

      SubPanel   *p = NULL;
      SubTag     *t = NULL;
      SubTray    *r = NULL;
      SubView    *v = NULL;
      SubGravity *g = NULL;

      switch(id)
        {
          /* ICCCM */
          case SUB_EWMH_NET_CURRENT_DESKTOP: /* {{{ */
            /* Switchs views of screen */
            if(0 <= ev->data.l[0] && ev->data.l[0] < subtle->views->ndata &&
                0 <= ev->data.l[2] && ev->data.l[2] < subtle->screens->ndata)
              EventSwitchView(ev->data.l[0], ev->data.l[2], True);
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

                subClientWarp(c, True);
                subClientFocus(c);
              }
            else if((r = TRAY(subSubtleFind(ev->data.l[0], TRAYID))))
              {
                XSetInputFocus(subtle->dpy, r->win,
                  RevertToPointerRoot, CurrentTime);
              }
            break; /* }}} */
          case SUB_EWMH_NET_RESTACK_WINDOW: /* {{{ */
            if((c = CLIENT(subSubtleFind(ev->data.l[1], CLIENTID))))
              EventRestack(c, ev->data.l[2]);
            break; /* }}} */

          /* subtle */
          case SUB_EWMH_SUBTLE_WINDOW_TAGS: /* {{{ */
            switch(ev->data.l[2]) ///< Type
              {
                case 0: ///< Clients
                  if((c = CLIENT(subArrayGet(subtle->clients,
                      (int)ev->data.l[0]))))
                    {
                      int i, flags = 0, tags = 0;

                      /* Select only new tags */
                      tags = (c->tags ^ (int)ev->data.l[1]) & (int)ev->data.l[1];

                      /* Update tags and assign properties */
                      for(i = 0; i < 31; i++)
                        if(tags & (1L << (i + 1))) subClientTag(c, i, &flags);

                      subClientToggle(c, flags, True); ///< Toggle flags
                      c->tags = (int)ev->data.l[1]; ///< Write all tags

                      /* EWMH: Tags */
                      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_TAGS,
                        (long *)&c->tags, 1);

                      subScreenConfigure();

                      /* Check visibility of focus window after updating tags
                       * and reactivate grabs if necessary */
                      if(subtle->windows.focus[0] == c->win &&
                          !VISIBLE(subtle->visible_tags, c))
                        subSubtleFocus(True);

                      subScreenUpdate();
                      subScreenRender();
                    }
                  else EventQueuePush(ev);
                  break;
                case 1: ///< Views
                  if((v = VIEW(subArrayGet(subtle->views,
                      (int)ev->data.l[0]))))
                    {
                      v->tags = (int)ev->data.l[1]; ///< Action

                      subViewPublish();

                      /* Reconfigure if view is visible */
                      if(subtle->visible_views & (1L << (ev->data.l[0] + 1)))
                        subScreenConfigure();
                    }
                  else EventQueuePush(ev);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_RETAG: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients,
                (int)ev->data.l[0])))) ///< Clients
              {
                int flags = 0;

                c->tags = 0; ///> Reset tags

                subClientRetag(c, &flags);
                subClientToggle(c, (~c->flags & flags), True); ///< Toggle flags

                if(VISIBLE(subtle->visible_tags, c) ||
                    flags & SUB_CLIENT_MODE_FULL)
                  {
                    subScreenConfigure();
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_GRAVITY: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))) &&
                ((g = GRAVITY(subArrayGet(subtle->gravities, (int)ev->data.l[1])))) &&
                VISIBLE(subtle->visible_tags, c))
              {
                subClientArrange(c, (int)ev->data.l[1], c->screen);
                subClientWarp(c, True);
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
                if(ev->data.l[1] & SUB_EWMH_FULL)   flags |= SUB_CLIENT_MODE_FULL;
                if(ev->data.l[1] & SUB_EWMH_FLOAT)  flags |= SUB_CLIENT_MODE_FLOAT;
                if(ev->data.l[1] & SUB_EWMH_STICK)  flags |= SUB_CLIENT_MODE_STICK;
                if(ev->data.l[1] & SUB_EWMH_RESIZE) flags |= SUB_CLIENT_MODE_RESIZE;

                subClientToggle(c, (~c->flags & flags), False); ///< Enable only

                if(VISIBLE(subtle->visible_tags, c) ||
                    flags & SUB_CLIENT_MODE_FULL)
                  {
                    subScreenConfigure();
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            else EventQueuePush(ev);
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
                        subClientArrange(c, 0, -1); ///< Fallback to first gravity
                        subClientWarp(c, True);
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
            if(ev->data.b)
              {
                int duplicate = False;

                if((t = subTagNew(ev->data.b, &duplicate)) && !duplicate)
                  {
                    subArrayPush(subtle->tags, (void *)t);
                    subTagPublish();

                    /* Hook: Create */
                    subHookCall((SUB_HOOK_TYPE_TAG|SUB_HOOK_ACTION_CREATE),
                      (void *)t);
                  }
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
                subViewPublish();

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

                EventQueuePop(subtle->views->ndata - 1, 1);

                /* Hook: Create */
                subHookCall((SUB_HOOK_TYPE_VIEW|SUB_HOOK_ACTION_CREATE),
                  (void *)v);
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
            if((p = EventFindSublet((int)ev->data.l[0])) &&
                p->sublet->flags & SUB_SUBLET_DATA)
              subRubyCall(SUB_CALL_DATA, p->sublet->instance, NULL);
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
                subRubyUnloadSublet(p);
                subScreenUpdate();
                subScreenRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_RELOAD: /* {{{ */
            if(subtle) subtle->flags |= SUB_SUBTLE_RELOAD;
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
  else if(ev->window == subtle->panels.tray.win)
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
                      if((t = subTrayNew(ev->data.l[2])))
                        {
                          subArrayPush(subtle->trays, (void *)t);
                          subTrayPublish();
                          subTrayUpdate();
                          subScreenUpdate();
                          subScreenRender();
                        }
                    }
                  break;
                case XEMBED_REQUEST_FOCUS:
                  subEwmhMessage(t->win, SUB_EWMH_XEMBED, 0xFFFFFF,
                    CurrentTime, XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0);
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
          case SUB_EWMH_NET_WM_STATE: /* {{{ */
              {
                int flags = 0;

                /* Translate properties */
                subEwmhTranslateWMState(ev->data.l[1], &flags);
                subEwmhTranslateWMState(ev->data.l[2], &flags);

                /* Since we always toggle we need to be careful */
                switch(ev->data.l[0])
                  {
                    case _NET_WM_STATE_ADD:
                      flags = (~c->flags & flags);
                      break;
                    case _NET_WM_STATE_REMOVE:
                      flags = (c->flags & flags);
                      break;
                    case _NET_WM_STATE_TOGGLE: break;
                  }

                subClientToggle(c, flags, True);

                if(VISIBLE(subtle->visible_tags, c) ||
                    flags & SUB_CLIENT_MODE_STICK)
                  {
                    subScreenConfigure();
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          case SUB_EWMH_NET_CLOSE_WINDOW: /* {{{ */
            subClientClose(c);
            break; /* }}} */
          case SUB_EWMH_NET_MOVERESIZE_WINDOW: /* {{{ */
              {
                if(!(c->flags & SUB_CLIENT_MODE_FLOAT))
                  subClientToggle(c, SUB_CLIENT_MODE_FLOAT, True);

                c->geom.x      = ev->data.l[1];
                c->geom.y      = ev->data.l[2];
                c->geom.width  = ev->data.l[3];
                c->geom.height = ev->data.l[4];

                subClientResize(c, True);

                XMoveResizeWindow(subtle->dpy, c->win, c->geom.x, c->geom.y,
                  c->geom.width, c->geom.height);

                if(VISIBLE(subtle->visible_tags, c))
                  {
                    subScreenUpdate();
                    subScreenRender();
                  }
              }
            break; /* }}} */
          default: break;
        }
    } /* }}} */

#ifdef DEBUG
  {
    char *name = XGetAtomName(subtle->dpy, ev->message_type);

    subSharedLogDebugEvents("ClientMessage: name=%s, type=%ld,"
      "format=%d, win=%#lx\n",
      name ? name : "n/a", ev->message_type, ev->format, ev->window);
    subSharedLogDebugEvents("ClientMessage: [0]=%#lx, [1]=%#lx,"
      "[2]=%#lx, [3]=%#lx, [4]=%#lx\n",
      ev->data.l[0], ev->data.l[1], ev->data.l[2], 
      ev->data.l[3], ev->data.l[4]);

    if(name) XFree(name);
  }
#endif
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

            if(subtle->windows.focus[0] == c->win)
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
            subClientToggle(c, (~c->flags & flags), True); ///< Only enable

            if(VISIBLE(subtle->visible_tags, c))
              {
                subScreenUpdate();
                subScreenRender();
              }
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

            if(VISIBLE(subtle->visible_tags, c))
              {
                subScreenUpdate();
                subScreenRender();
              }
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
      case SUB_EWMH_MOTIF_WM_HINTS: /* {{{ */
        if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
          subClientSetMWMHints(c);
        break; /* }}} */
      case SUB_EWMH_XEMBED_INFO: /* {{{ */
        if((t = TRAY(subSubtleFind(ev->window, TRAYID))))
          {
            subTraySetState(t);
            subTrayUpdate();
            subScreenUpdate();
            subScreenRender();
          }
        break; /* }}} */
    }

#ifdef DEBUG
  {
    char *name = XGetAtomName(subtle->dpy, ev->atom);

    subSharedLogDebugEvents("Property: name=%s, type=%ld, win=%#lx\n",
      name ? name : "n/a", ev->atom, ev->window);

    if(name) XFree(name);
  }
#endif
} /* }}} */

/* EventSelection {{{ */
void
EventSelection(XSelectionClearEvent *ev)
{
  /* Handle selection clear events */
  if(ev->window == subtle->panels.tray.win) ///< Tray selection
    {
      subtle->flags &= ~SUB_SUBTLE_TRAY;
      subTrayDeselect();
    }
  else if(ev->window == subtle->windows.support) ///< Session selection
    {
      subSharedLogWarn("Leaving the field\n");
      subtle->flags &= ~SUB_SUBTLE_RUN; ///< Exit
    }

  subSharedLogDebugEvents("SelectionClear: win=%#lx, tray=%#lx, support=%#lx\n",
    ev->window, subtle->panels.tray.win, subtle->windows.support);
} /* }}} */

/* EventUnmap {{{ */
static void
EventUnmap(XUnmapEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  int focus = (subtle->windows.focus[0] == ev->window); ///< Save

  /* Check if we know this window */
  if((c = CLIENT(subSubtleFind(ev->window, CLIENTID))))
    {
      if(ev->send_event) ///< Client
        {
          /* Set withdrawn state (see ICCCM 4.1.4) */
          subEwmhSetWMState(c->win, WithdrawnState);

          /* Ignore our generated unmap events */
          if(c->flags & SUB_CLIENT_UNMAP)
            {
              c->flags &= ~SUB_CLIENT_UNMAP;

              return;
            }

          /* Kill client */
          c->flags |= SUB_CLIENT_DEAD;
          XSelectInput(subtle->dpy, c->win, NoEventMask);

          subArrayRemove(subtle->clients, (void *)c);
          subClientKill(c);
          subClientPublish();

          subScreenUpdate();
          subScreenRender();

          /* Update focus if necessary */
          if(focus) subSubtleFocus(True);
        }
    }
  else if((t = TRAY(subSubtleFind(ev->window, TRAYID)))) ///< Tray
    {
      /* Set withdrawn state (see ICCCM 4.1.4) */
      subEwmhSetWMState(t->win, WithdrawnState);

      /* Ignore out own generated unmap events */
      if(t->flags & SUB_TRAY_UNMAP)
        {
          t->flags &= ~SUB_TRAY_UNMAP;

          return;
        }

      /* FIXME: Shouldn't we kill the tray? */
      t->flags |= SUB_TRAY_DEAD;
      subTrayUpdate();
      subTrayPublish();

      subScreenUpdate();
      subScreenRender();

      /* Update focus if necessary */
      if(focus) subSubtleFocus(True);
    }

  subSharedLogDebugEvents("Unmap: win=%#lx\n", ev->window);
} /* }}} */

/* Public */

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
  int i, timeout = 1, nevents = 0;
  XEvent ev;
  time_t now;
  SubPanel *p = NULL;

#ifdef HAVE_SYS_INOTIFY_H
  char buf[BUFLEN];
#endif /* HAVE_SYS_INOTIFY_H */

  /* Update screens */
  subScreenConfigure();
  subScreenUpdate();
  subScreenRender();

  /* Add watches */
  subEventWatchAdd(ConnectionNumber(subtle->dpy));
#ifdef HAVE_SYS_INOTIFY_H
  subEventWatchAdd(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */

  /* Set tray selection */
  if(subtle->flags & SUB_SUBTLE_TRAY) subTraySelect();

  subtle->flags |= SUB_SUBTLE_RUN;
  XSync(subtle->dpy, False); ///< Sync before going on

  /* Set focus */
  subtle->windows.focus[0] = ROOT;
  subGrabSet(ROOT);
  subSubtleFocus(True);

  /* Hook: Start */
  subHookCall(SUB_HOOK_START, NULL);

  while(subtle && subtle->flags & SUB_SUBTLE_RUN)
    {
      now = subSubtleTime();

      /* Check if we need to reload */
      if(subtle->flags & SUB_SUBTLE_RELOAD)
        {
          int tray = subtle->flags & SUB_SUBTLE_TRAY;

          subtle->flags &= ~SUB_SUBTLE_RELOAD;
          subRubyReloadConfig();

          /* Update tray selection */
          if(tray && !(subtle->flags & SUB_SUBTLE_TRAY))
            subTrayDeselect();
          else if(subtle->flags & SUB_SUBTLE_TRAY)
            subTraySelect();
        }

      /* Data ready on any connection */
      if(0 < (nevents = poll(watches, nwatches, timeout * 1000)))
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
                              case ColormapNotify:    EventColormap(&ev.xcolormap);                 break;
                              case ConfigureNotify:   EventConfigure(&ev.xconfigure);               break;
                              case ConfigureRequest:  EventConfigureRequest(&ev.xconfigurerequest); break;
                              case EnterNotify:
                              case LeaveNotify:       EventCrossing(&ev.xcrossing);                 break;
                              case DestroyNotify:     EventDestroy(&ev.xdestroywindow);             break;
                              case Expose:            EventExpose(&ev.xexpose);                     break;
                              case FocusIn:           EventFocus(&ev.xfocus);                       break;
                              case ButtonPress:
                              case KeyPress:          EventGrab(&ev);                               break;
                              case MapNotify:         EventMap(&ev.xmap);                           break;
                              case MapRequest:        EventMapRequest(&ev.xmaprequest);             break;
                              case ClientMessage:     EventMessage(&ev.xclient);                    break;
                              case PropertyNotify:    EventProperty(&ev.xproperty);                 break;
                              case SelectionClear:    EventSelection(&ev.xselectionclear);          break;
                              case UnmapNotify:       EventUnmap(&ev.xunmap);                       break;
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
      else if(0 == nevents) ///< Timeout waiting for data or error {{{
        {
          if(0 < subtle->sublets->ndata)
            {
              p = PANEL(subtle->sublets->data[0]);

              /* Update all pending sublets */
              while(p && p->sublet->flags & SUB_SUBLET_INTERVAL &&
                  p->sublet->time <= now)
                {
                  subRubyCall(SUB_CALL_RUN, p->sublet->instance, NULL);

                  /* This may change during run */
                  if(p->sublet->flags & SUB_SUBLET_INTERVAL) 
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

          timeout = p->sublet->flags & SUB_SUBLET_INTERVAL ?
            p->sublet->time - now : 60;
          if(0 >= timeout) timeout = 1; ///< Sanitize
        }
      else timeout = 60;
    }

  /* Drop tray selection */
  if(subtle->flags & SUB_SUBTLE_TRAY) subTrayDeselect();
} /* }}} */

 /** subEventFinish {{{
  * @brief Finish event processing
  **/

void
subEventFinish(void)
{

  if(watches) free(watches);
  if(queue)   free(queue);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
