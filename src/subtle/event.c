
 /**
  * @package subtle
  *
  * @file Event functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <unistd.h>
#include <X11/Xatom.h>
#include "subtle.h"

#ifdef HAVE_SYS_INOTIFY_H
#define BUFLEN (sizeof(struct inotify_event))
#endif /* HAVE_SYS_INOTIFY_H */

/* EventExec {{{ */
static void
EventExec(char *cmd)
{
  pid_t pid = fork();

  switch(pid)
    {
      case 0:
        setsid();
        execlp("/bin/sh", "sh", "-c", cmd, NULL);

        /* Never to be reached */
        subSharedLogWarn("Can't exec command `%s'.\n", cmd);
        exit(1);
      case -1: subSharedLogWarn("Failed to fork.\n");
    }
} /* }}} */

/* EventConfigure {{{ */
static void
EventConfigure(XConfigureRequestEvent *ev)
{
  XWindowChanges wc;
  SubClient *c = CLIENT(subSharedFind(ev->window, CLIENTID));

  if(c)
    {
      if(ev->value_mask & CWX)      c->rect.x      = ev->x;
      if(ev->value_mask & CWY)      c->rect.y      = ev->y;
      if(ev->value_mask & CWWidth)  c->rect.width  = ev->width;
      if(ev->value_mask & CWHeight) c->rect.height = ev->height;

      subClientConfigure(c);
    }
  else
    {
      wc.x          = ev->x;
      wc.y          = ev->y;
      wc.width      = ev->width;
      wc.height     = ev->height;
      wc.sibling    = ev->above;
      wc.stack_mode = ev->detail;

      XConfigureWindow(subtle->disp, ev->window, ev->value_mask, &wc);
    }
} /* }}} */

/* EventMapRequest {{{ */
static void
EventMapRequest(XMapRequestEvent *ev)
{
  SubClient *c = CLIENT(subSharedFind(ev->window, CLIENTID));
  if(!c) 
    {
      /* Create new client */
      c = subClientNew(ev->window);
      subArrayPush(subtle->clients, (void *)c);
      subClientPublish();

      /* Configure/render current view if tags match or client is urgent */
      if(subtle->cv && (subtle->cv->tags & c->tags || c->flags & SUB_STATE_URGENT))
        {
          subViewConfigure(subtle->cv); 
          subViewRender();
        }
    }
} /* }}} */

/* EventDestroy {{{ */
static void
EventDestroy(XDestroyWindowEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;
  
  if((c = CLIENT(subSharedFind(ev->event, CLIENTID))))
    {
      c->flags |= SUB_STATE_DEAD;
      subClientKill(c); 
      subClientPublish();
      subViewConfigure(subtle->cv);
      subViewUpdate();
    }
  else if((t = TRAY(subSharedFind(ev->event, TRAYID))))
    {
      subArrayPop(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
    }
} /* }}} */

/* EventMessage {{{ */
static void
EventMessage(XClientMessageEvent *ev)
{
  SubClient *c = NULL;

#ifdef DEBUG
  char *name = XGetAtomName(subtle->disp, ev->message_type);

  printf("ClientMessage: name=%s, type=%ld, format=%d, win=%#lx\n", 
    name ? name : "n/a", ev->message_type, ev->format, ev->window);
  if(name) XFree(name);
#endif /* DEBUG */

  if(32 != ev->format) return; ///< Forget about it

  /* Messages for root window {{{ */
  if(ROOT == ev->window)
    {
      int id = subEwmhFind(ev->message_type);
      SubView *v = NULL;
      SubTag *t = NULL;
      SubSublet *s = NULL;

      switch(id)
        {
          case SUB_EWMH_NET_CURRENT_DESKTOP: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views, ev->data.l[0])))) subViewJump(v);
            break; /* }}} */
          case SUB_EWMH_NET_ACTIVE_WINDOW: /* {{{ */
            if((c = CLIENT(subSharedFind(ev->data.l[0], CLIENTID))))
              subClientFocus(c);
            break; /* }}} */
          case SUB_EWMH_SUBTLE_CLIENT_TAG: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))))
              {
                c->tags |= (1L << (ev->data.l[1] + 1));
                subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_CLIENT_UNTAG: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))))
              {
                c->tags &= ~(1L << (ev->data.l[1] + 1));
                subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);
              }
            if(subtle->cv->tags & (1L << ev->data.l[1])) subViewConfigure(subtle->cv);
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_NEW: /* {{{ */
            t = subTagNew(ev->data.b, NULL); 
            subArrayPush(subtle->tags, (void *)t);
            subTagPublish();
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_KILL: /* {{{ */
            if((t = TAG(subArrayGet(subtle->tags, (int)ev->data.l[0]))))
              {
                int i;
                
                /* Views */
                id = ev->data.l[0] - 1;
                for(i = 0; i < subtle->views->ndata; i++)
                  {
                    v = VIEW(subtle->views->data[i]);

                    if(v->tags & id)
                      {
                        v->tags &= ~id;
                        subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS,
                          (long *)&v->tags, 1);
                      }
                  }

                /* Clients */
                for(i = 0; i < subtle->clients->ndata; i++)
                  {
                    c = CLIENT(subtle->clients->data[i]);

                    if(c->tags & id)
                      {
                        c->tags &= ~id;
                        subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, 
                          (long *)&c->tags, 1);
                      }
                  }

                subArrayPop(subtle->tags, (void *)t);
                subTagKill(t);
                subTagPublish();
                
                if(subtle->cv->tags & id)
                  subViewConfigure(subtle->cv); ///< Re-configure current view if needed
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_NEW: /* {{{ */
            if(ev->data.b)
              {
                v = subViewNew(ev->data.b, NULL); 
                subArrayPush(subtle->views, (void *)v);
                subViewUpdate();
                subViewPublish();
                subViewRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_TAG: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0]))))
              {
                v->tags |= (1L << (ev->data.l[1] + 1));
                subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS, (long *)&v->tags, 1);
                if(subtle->cv == v) subViewConfigure(v);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_UNTAG: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0]))))
              {
                v->tags &= ~(1L << (ev->data.l[1] + 1));
                subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS, (long *)&v->tags, 1);
                if(subtle->cv == v) subViewConfigure(v);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_KILL: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0]))))
              {
                subArrayPop(subtle->views, (void *)v);
                subViewKill(v);
                subViewUpdate();
                subViewPublish();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_KILL: /* {{{ */
            if((s = SUBLET(subArrayGet(subtle->sublets, (int)ev->data.l[0]))))
              {
                subArrayPop(subtle->sublets, (void *)s);
                subSubletKill(s);
                subSubletUpdate();
                subSubletPublish();
              }
            break; /* }}} */
        }
      return;
    } /* }}} */
  /* Messages for tray window {{{ */
  else if(ev->window == subtle->windows.tray)
    {
      int id = subEwmhFind(ev->message_type);
      SubTray *t = NULL;

      printf("[0]=%#lx, [1]=%#lx, [2]=%#lx, [3]=%#lx, [4]=%#lx\n", 
        ev->data.l[0], ev->data.l[1], ev->data.l[2], ev->data.l[3], ev->data.l[4]);

      switch(id)
        {
          case SUB_EWMH_NET_SYSTEM_TRAY_OPCODE: /* {{{ */
            switch(ev->data.l[1])
              {
                case XEMBED_EMBEDDED_NOTIFY:
                  if(!(t = TRAY(subSharedFind(ev->window, TRAYID))))
                    {
                      t = subTrayNew(ev->data.l[2]);
                      subArrayPush(subtle->trays, (void *)t);
                      subTrayUpdate();
                    }
                  break;
                default:
                  printf("Tray: data=%ld\n", ev->data.l[1]);
              }
            break; /* }}} */
        }
      return;
    } /* }}} */
  /* Messages for client windows {{{ */
  else if((c = CLIENT(subSharedFind(ev->window, CLIENTID))));
    {
      int id = subEwmhFind(ev->message_type);

      switch(id)
        {
          /* Actions: 0 = remove / 1 = add / 2 = toggle - we always toggle */
          case SUB_EWMH_NET_WM_STATE: /* {{{ */
            id = subEwmhFind(ev->data.l[1]); ///< Only the first property

            switch(id)
              {
                case SUB_EWMH_NET_WM_STATE_FULLSCREEN:
                  subClientToggle(c, SUB_STATE_FULL);
                  break;
                case SUB_EWMH_NET_WM_STATE_ABOVE:
                  subClientToggle(c, SUB_STATE_FLOAT);
                  break;
                case SUB_EWMH_NET_WM_STATE_STICKY:
                  subClientToggle(c, SUB_STATE_URGENT);
                  break;
              }

            if(subtle->cv->tags & c->tags) subViewConfigure(subtle->cv);
            break; /* }}} */
          case SUB_EWMH_NET_CLOSE_WINDOW: /* {{{ */
            subClientKill(c);               
            break; /* }}} */
        }
    } /* }}} */
} /* }}} */

/* EventColormap {{{ */
static void
EventColormap(XColormapEvent *ev)
{  
  SubClient *c = (SubClient *)subSharedFind(ev->window, 1);
  if(c && ev->new)
    {
      c->cmap = ev->colormap;
      XInstallColormap(subtle->disp, c->cmap);
    }
} /* }}} */

/* EventProperty {{{ */
static void
EventProperty(XPropertyEvent *ev)
{
#ifdef DEBUG
  char *name = XGetAtomName(subtle->disp, ev->atom);

  subSharedLogDebug("Property: name=%s, type=%ld, win=%#lx\n", 
    name ? name : "n/a", ev->atom, ev->window);
  if(name) XFree(name);
#endif /* DEBUG */

  /* Supported properties */
  if(XA_WM_NAME == ev->atom || subEwmhGet(SUB_EWMH_WM_NAME) == ev->atom) ///< Client
    {
      SubClient *c = CLIENT(subSharedFind(ev->window, CLIENTID));
      if(c) subClientFetchName(c);
    }
  else if(subEwmhGet(SUB_EWMH_XEMBED_INFO) == ev->atom) ///< Tray
    {
      SubTray *t = TRAY(subSharedFind(ev->window, TRAYID));
      if(t)
        {
          subTraySetState(t);
          subTrayUpdate();
        }
    }
  else if(subEwmhGet(SUB_EWMH_WM_NORMAL_HINTS) == ev->atom) ///< Tray
    {
      SubTray *t = TRAY(subSharedFind(ev->window, TRAYID));
      if(t)
        {
          subTrayConfigure(t);
          subTrayUpdate();
        }
    }    
} /* }}} */

/* EventCrossing {{{ */
static void
EventCrossing(XCrossingEvent *ev)
{
  XEvent event;
  SubClient *c = NULL;
  SubTray *t = NULL;

  if((c = CLIENT(subSharedFind(ev->window, CLIENTID))))
    {
      if(!(c->flags & SUB_STATE_DEAD))
        {
          Window win = 0, root = 0;
          int rx = 0, ry = 0, wx = 0, wy = 0;
          unsigned int mask = 0;

          /* Ensure that only the pointer window can get focus */
          XQueryPointer(subtle->disp, c->win, &root, &win, &rx, &ry, &wx, &wy, &mask);
          if(ev->subwindow == win) subClientFocus(c);

        }
     }
  else if((t = TRAY(subSharedFind(ev->window, TRAYID))))
    {
printf("name=%s\n", t->name);
    }

  /* Remove any other event of the same type and window */
  while(XCheckTypedWindowEvent(subtle->disp, ev->window, ev->type, &event));    
} /* }}} */

/* EventSelectionClear {{{ */
void
EventSelectionClear(XSelectionClearEvent *ev)
{
  if(subEwmhGet(SUB_EWMH_NET_SYSTEM_TRAY_SELECTION) == ev->selection)
    if(ev->window == subtle->windows.tray)
      {
        subSharedLogDebug("We lost the selection? Renew it!\n");
        subTraySelect();
      }
} /* }}} */

/* EventGrab {{{ */
static void
EventGrab(XEvent *ev)
{
  Window win = 0;
  SubGrab *g = NULL;
  SubClient *c = NULL;
  FLAGS flag = 0;
  unsigned int code = 0, state = 0;

  /* Distinct types */
  switch(ev->type)
    {
      case ButtonPress: 
        if(ev->xany.window == subtle->windows.views) ///< View buttons
          {
            SubView *v = VIEW(subSharedFind(ev->xbutton.subwindow, BUTTONID));
            if(subtle->cv != v) subViewJump(v); ///< Prevent jumping to current view
            return;
          }      

        win   = ev->xbutton.window == subtle->cv->frame ?
          ev->xbutton.subwindow : ev->xbutton.window;
        code  = XK_Pointer_Button1 + ev->xbutton.button;
        state = ev->xbutton.state;
        break;
      case KeyPress:    
        win   = ev->xkey.window == subtle->cv->frame ?
          ev->xkey.subwindow : ev->xkey.window;
        code  = ev->xkey.keycode;
        state = ev->xkey.state;
        break;
    }

  /* Find grab */
  g = subGrabFind(code, state);
  if(g) 
    {
      flag = g->flags & ~(SUB_TYPE_GRAB|SUB_GRAB_KEY|SUB_GRAB_MOUSE); ///< Clear mask
      switch(flag)
        {
          case SUB_GRAB_WINDOW_FLOAT:
          case SUB_GRAB_WINDOW_FULL:
            c = CLIENT(subSharedFind(win, CLIENTID));
            if(c) 
              {
                flag = SUB_GRAB_WINDOW_FLOAT == flag ? SUB_STATE_FLOAT : SUB_STATE_FULL;
                subClientToggle(c, flag);
                subViewConfigure(subtle->cv);
              }
            break;            
          case SUB_GRAB_WINDOW_KILL:
            c = CLIENT(subSharedFind(win, CLIENTID));
            if(c) 
              { 
                subClientKill(c);
                subClientPublish();
              }
            break;             
          case SUB_GRAB_WINDOW_MOVE:
          case SUB_GRAB_WINDOW_RESIZE:
            c = CLIENT(subSharedFind(win, CLIENTID));
            if(c && !(c->flags & SUB_STATE_FULL))
              {
                if(c->flags & (SUB_STATE_FLOAT|SUB_STATE_URGENT))
                  flag = SUB_GRAB_WINDOW_MOVE == flag ? SUB_DRAG_MOVE : SUB_DRAG_RESIZE;
                else flag = SUB_DRAG_TILE;

                subClientDrag(c, flag);
              }
            break;            
          case SUB_GRAB_EXEC:
            if(g->string) EventExec(g->string);
            break;
          case SUB_GRAB_VIEW_JUMP:
            if(0 <= g->number && g->number < subtle->views->ndata)
              subViewJump(VIEW(subtle->views->data[g->number]));
            break;
          default:  
            printf("Grab not implemented yet!\n");
        }

      subSharedLogDebug("Grab: code=%03d, mod=%03d\n", g->code, g->mod);
    }
} /* }}} */

/* EventExpose {{{ */
static void
EventExpose(XEvent *ev)
{
  XEvent event;

  if(ev->xany.window == subtle->windows.bar)
    {
      subViewRender();
      subSubletRender();
    }
  else if(ev->xany.window == subtle->cv->frame)
    {
      subViewRender();
    }

  /* Remove any other event of the same type and window */
  while(XCheckTypedWindowEvent(subtle->disp, ev->xany.window, ev->type, &event));
} /* }}} */

/* EventFocus {{{ */
static void
EventFocus(XFocusChangeEvent *ev)
{
  SubClient *c = CLIENT(subSharedFind(ev->window, CLIENTID));
  if(c)
    { 
      if(FocusOut == ev->type) ///< FocusOut event
        {
          /* Remove focus from client */
          if(subtle->windows.focus)
            {
              SubClient *f = NULL;
              Window oldfocus = subtle->windows.focus;
              subtle->windows.focus = 0;

              if((f = CLIENT(subSharedFind(oldfocus, CLIENTID))))
                {
                  if(!(f->flags & SUB_STATE_DEAD)) ///< Don't revive
                    {
                      subGrabUnset(oldfocus);
                      subClientRender(f);
                    }
                }
              else subGrabUnset(oldfocus);
              subViewRender();
            } 
        }
      else if(FocusIn == ev->type) ///< FocusIn event
        {
          subtle->windows.focus = c->win;

          subGrabSet(c->win);
          subClientRender(c);

          /* EWMH: Active window */
          subEwmhSetWindows(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW, &c->win, 1);

          subViewUpdate();
        }
    }
} /* }}} */

 /** subEventLoop {{{ 
  * @brief Event all X events
  **/

void
subEventLoop(void)
{
  int ret;
  time_t ctime;
  XEvent ev;
  fd_set rfds;
  struct timeval tv;
  SubSublet *s = NULL;

#ifdef HAVE_SYS_INOTIFY_H
  char buf[BUFLEN];
#endif /* HAVE_SYS_INOTIFY_H */

  s = 0 < subtle->sublets->ndata ? SUBLET(subtle->sublets->data[0]) : NULL;

  /* Init timeout and assemble FD_SET */
  tv.tv_sec    = 0;
  tv.tv_usec  = 0;

  FD_ZERO(&rfds);
  FD_SET(ConnectionNumber(subtle->disp), &rfds);

#ifdef HAVE_SYS_INOTIFY_H
  FD_SET(subtle->notify, &rfds); ///< Add inotify socket to set
#endif /* HAVE_SYS_INOTIFY_H */

  while(1)
    {
      ctime  = subSharedTime();
      ret = select(ConnectionNumber(subtle->disp) + 1, &rfds, NULL, NULL, &tv);
      if(-1 == ret) 
        {
          subSharedLogDebug("%s\n", strerror(errno)); ///< Ignore and print debugging message
        }
      else if(ret) ///< Data ready on any connection
        {
          if(FD_ISSET(ConnectionNumber(subtle->disp), &rfds)) ///< X connection
            {
              /* Event X events */
              while(XPending(subtle->disp))
                {
                  XNextEvent(subtle->disp, &ev);
                  switch(ev.type)
                    {
                      case ConfigureRequest:  EventConfigure(&ev.xconfigurerequest);     break;
                      case MapRequest:        EventMapRequest(&ev.xmaprequest);          break;
                      case DestroyNotify:     EventDestroy(&ev.xdestroywindow);          break;
                      case ClientMessage:     EventMessage(&ev.xclient);                 break;
                      case ColormapNotify:    EventColormap(&ev.xcolormap);              break;
                      case PropertyNotify:    EventProperty(&ev.xproperty);              break;
                      case EnterNotify:       EventCrossing(&ev.xcrossing);              break;
                      case SelectionClear:    EventSelectionClear(&ev.xselectionclear);  break;

                      case ButtonPress:
                      case KeyPress:          EventGrab(&ev);                            break;
                      case VisibilityNotify:  
                      case Expose:            EventExpose(&ev);                          break;
                      case FocusIn:           
                      case FocusOut:          EventFocus(&ev.xfocus);                    break;
                    }
                }
              }
#ifdef HAVE_SYS_INOTIFY_H
            else if(FD_ISSET(subtle->notify, &rfds)) ///< Inotify descriptor
              {
                /* Handle inotify events */
                if(read(subtle->notify, buf, BUFLEN) > 0)
                  {
                    struct inotify_event *event = (struct inotify_event *)&buf[0];
                    if(event)
                      {
                        SubSublet *ws = SUBLET(subSharedFind(subtle->windows.sublets, event->wd));
                        if(ws)
                          {
                            subRubyRun(ws);
                            subSubletUpdate();
                            subSubletRender();
                          }
                      }
                  }

              }
#endif /* HAVE_SYS_INOTIFY_H */
        }
      else ///< Timeout waiting for data
        {
          /* Update sublet data */
          s = 0 < subtle->sublets->ndata ? SUBLET(subtle->sublets->data[0]) : NULL;
          while(s && s->time <= ctime)
            {
              s->time = ctime + s->interval; ///< Adjust seconds
              s->time -= s->time % s->interval;

              subRubyRun(s);
              subArraySort(subtle->sublets, subSubletCompare);

              s = SUBLET(subtle->sublets->data[0]);
            }
          subSubletUpdate();
          subSubletRender();
        }

      /* Set timeout and assemble FD_SET */
      tv.tv_sec   = s ? abs(s->time - ctime) : 60;
      tv.tv_usec  = 0;

      FD_ZERO(&rfds);
      FD_SET(ConnectionNumber(subtle->disp), &rfds);

#ifdef HAVE_SYS_INOTIFY_H
      FD_SET(subtle->notify, &rfds); ///< Add inotify socket to set
#endif /* HAVE_SYS_INOTIFY_H */
    }
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
