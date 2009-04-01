
 /**
  * @package subtle
  *
  * @file Event functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
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

/* EventUntag {{{ */
static void
EventUntag(SubClient *c,
  int id)
{
  int i, tag;

  for(i = id; i < (sizeof(TAGS) * 8) - 1; i++)
    {
      tag = (1L << (i + 1));

      if(c->tags & (1L << (i + 2))) ///< Next bit
        c->tags |= tag;
      else
        c->tags &= ~tag;
    }

  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&c->tags, 1);
} /* }}} */

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

        subSharedLogWarn("Failed to exec command `%s'.\n", cmd); ///< Never to be reached
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
      if((c = subClientNew(ev->window)))
        {
          subArrayPush(subtle->clients, (void *)c);
          subClientPublish();

          /* Configure/render current view if tags match or client is urgent */
          if(VISIBLE(subtle->cv, c))
            {
              subViewConfigure(subtle->cv); 
              subViewRender();
            }
        }
    }
} /* }}} */

/* EventMap {{{ */
static void
EventMap(XMapEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  if(ev->window != ev->event && ev->send_event != True) return;

  if((c = CLIENT(subSharedFind(ev->window, CLIENTID))))
    {
      subEwmhSetWMState(c->win, NormalState);
    }
  else if((t = TRAY(subSharedFind(ev->window, TRAYID))))
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

  if(True != ev->send_event) return;

  if((c = CLIENT(subSharedFind(ev->window, CLIENTID))))
    {
      subEwmhSetWMState(c->win, WithdrawnState);
      subArrayPop(subtle->clients, (void *)c);
      subClientPublish();
      if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
      subClientKill(c, False);
    }
  else if((t = TRAY(subSharedFind(ev->window, TRAYID))))
    {
      subEwmhSetWMState(t->win, WithdrawnState);
      subArrayPop(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
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
      subArrayPop(subtle->clients, (void *)c);
      subClientPublish();
      if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
      subClientKill(c, True); 
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

  if(32 != ev->format) return; ///< Just skip

  /* Messages for root window {{{ */
  if(ROOT == ev->window)
    {
      int tag, id = subEwmhFind(ev->message_type);

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
              {
                if(!(VISIBLE(subtle->cv, c))) ///< Client is on current view?
                  {
                    int i;

                    /* Find matching view */
                    for(i = 0; i < subtle->views->ndata; i++)
                      if(VISIBLE(VIEW(subtle->views->data[i]), c))
                        {
                          subViewJump(VIEW(subtle->views->data[i]));
                          break;
                        }
                  }
                subClientFocus(c);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_TAG:
          case SUB_EWMH_SUBTLE_WINDOW_UNTAG: /* {{{ */
            tag = (1L << (ev->data.l[1] + 1));

            switch(ev->data.l[2]) ///< Type
              {
                case 0:          
                  if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0])))) ///< Clients
                    {
                      if(SUB_EWMH_SUBTLE_WINDOW_TAG == id) c->tags |= tag; ///< Action
                      else c->tags &= ~tag;

                      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&c->tags, 1);
                      if(subtle->cv->tags & tag) subViewConfigure(subtle->cv);
                    }
                  break;
                case 1:
                  if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0])))) ///< Views
                    {
                      if(SUB_EWMH_SUBTLE_WINDOW_TAG == id) v->tags |= tag; ///< Action
                      else v->tags &= ~tag;
                    
                      subEwmhSetCardinals(v->win, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1);
                      if(subtle->cv == v) subViewConfigure(v);
                    } 
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_GRAVITY: /* {{{ */  
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))))
              {
                int vid = ev->data.l[1];

                if(-1 == vid)
                  vid = subArrayIndex(subtle->views, (void *)subtle->cv);

                c->gravity        = -1; ///< Force 
                c->gravities[vid] = ev->data.l[2];

                if(subtle->cv->tags & tag) subViewConfigure(subtle->cv);
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_NEW: /* {{{ */
            t = subTagNew(ev->data.b, NULL); 
            subArrayPush(subtle->tags, (void *)t);
            subTagPublish();
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_KILL: /* {{{ */
            if((t = TAG(subArrayGet(subtle->tags, (int)ev->data.l[0]))))
              {
                int i, reconf = subtle->cv->tags & (1L << ((int)ev->data.l[0] + 1));

                /* Views */
                for(i = 0; i < subtle->views->ndata; i++)
                  EventUntag(CLIENT(subtle->views->data[i]), (int)ev->data.l[0]);

                /* Clients */
                for(i = 0; i < subtle->clients->ndata; i++)
                  EventUntag(CLIENT(subtle->clients->data[i]), (int)ev->data.l[0]);
              
                subArrayPop(subtle->tags, (void *)t);
                subTagKill(t);
                subTagPublish();
                
                if(reconf) subViewConfigure(subtle->cv);
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
          case SUB_EWMH_SUBTLE_VIEW_KILL: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0]))))
              {
                subArrayPop(subtle->views, (void *)v);
                subViewKill(v);
                subViewUpdate();
                subViewPublish();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_UPDATE: /* {{{ */
            if((s = SUBLET(subArrayGet(subtle->sublets, (int)ev->data.l[0]))))
              {
                subRubyRun(s);
                subSubletUpdate();
                subSubletRender();                
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_KILL: /* {{{ */
            if((s = SUBLET(subArrayGet(subtle->sublets, (int)ev->data.l[0]))))
              {
                subArrayPop(subtle->sublets, (void *)s);
                subSubletKill(s, True);
                subSubletUpdate();
                subSubletPublish();
              }
            break; /* }}} */
        }
    } /* }}} */
  /* Messages for tray window {{{ */
  else if(ev->window == subtle->windows.tray)
    {
      SubTray *t = NULL;
      int id = subEwmhFind(ev->message_type);

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
                case XEMBED_REQUEST_FOCUS:
                  if((t = TRAY(subSharedFind(ev->window, TRAYID))))
                    {
                    }
                  
              }
            break; /* }}} */
        }
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
                  subClientToggle(c, SUB_STATE_STICK);
                  break;
              }

            if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
            break; /* }}} */
          case SUB_EWMH_NET_CLOSE_WINDOW: /* {{{ */
            subClientKill(c, True);               
            break; /* }}} */
        }
    } /* }}} */

#ifdef DEBUG
  {
    char *name = XGetAtomName(subtle->disp, ev->message_type);

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
  SubClient *c = NULL;
  SubTray *t = NULL;
  int id = subEwmhFind(ev->atom);

  if(XA_WM_NAME == ev->atom) id = SUB_EWMH_WM_NAME;

  /* Supported properties */
  switch(id)
    {
      case SUB_EWMH_WM_NAME: ///< Client
        if((c = CLIENT(subSharedFind(ev->window, CLIENTID)))) 
          {
            char *name = NULL;

            if(XFetchName(subtle->disp, c->win, &name))
              {
                if(c->name) free(c->name);
                c->name = name;

                subClientRender(c);
              }
          }
        break;
      case SUB_EWMH_XEMBED_INFO: ///< Tray
        if((t = TRAY(subSharedFind(ev->window, TRAYID))))
          {
            subTraySetState(t);
            subTrayUpdate();
          }
        break;
      case SUB_EWMH_WM_NORMAL_HINTS: ///< Tray
        if((t = TRAY(subSharedFind(ev->window, TRAYID))))
          {
            subTrayConfigure(t);
            subTrayUpdate();
          }
        break;
#ifdef DEBUG
      default:
        {
          char *name = XGetAtomName(subtle->disp, ev->atom);

          subSharedLogDebug("Property: name=%s, type=%ld, win=%#lx\n", 
            name ? name : "n/a", ev->atom, ev->window);
          if(name) XFree(name);
        }
#endif /* DEBUG */
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
        subClientFocus(c);
     }
  else if((t = TRAY(subSharedFind(ev->window, TRAYID)))) subTrayFocus(t);

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
  SubGrab *g = NULL;
  unsigned int code = 0, state = 0;

  /* Distinct types */
  switch(ev->type)
    {
      case ButtonPress: 
        if(ev->xbutton.window == subtle->windows.views) ///< View buttons
          {
            SubView *v = VIEW(subSharedFind(ev->xbutton.subwindow, BUTTONID));
            if(subtle->cv != v) subViewJump(v); ///< Prevent jumping to current view
            return;
          }      

        code  = XK_Pointer_Button1 + ev->xbutton.button;
        state = ev->xbutton.state;
        break;
      case KeyPress:    
        code  = ev->xkey.keycode;
        state = ev->xkey.state;
        break;
    }

  /* Find grab */
  if((g = subGrabFind(code, state)))
    {
      Window win = 0;
      SubClient *c = NULL;
      FLAGS flag = 0;
      int wx = 0, wy = 0, vid = 0;

      /* Use similarity of both events */
      win = ev->xbutton.window == subtle->cv->win ? ev->xbutton.subwindow : ev->xbutton.window;
      wx  = ev->xbutton.x;
      wy  = ev->xbutton.y + subtle->th;

      flag = g->flags & ~(SUB_TYPE_GRAB|SUB_GRAB_KEY|SUB_GRAB_MOUSE); ///< Clear mask
      switch(flag)
        {
          case SUB_GRAB_WINDOW_FLOAT:
          case SUB_GRAB_WINDOW_FULL:
          case SUB_GRAB_WINDOW_STICK: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))))
              {
                flag = SUB_GRAB_WINDOW_FLOAT == flag ? SUB_STATE_FLOAT : 
                  (SUB_GRAB_WINDOW_FULL == flag ? SUB_STATE_FULL : SUB_STATE_STICK);
                subClientToggle(c, flag);
                if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_KILL: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))))
              { 
                subArrayPop(subtle->clients, (void *)c);
                subClientPublish();
                if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
                subClientKill(c, True);
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_MOVE:
          case SUB_GRAB_WINDOW_RESIZE: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))) && !(c->flags & SUB_STATE_FULL))
              {
                if(SUB_GRAB_WINDOW_MOVE == flag && c->flags & (SUB_STATE_FLOAT|SUB_STATE_STICK))
                  flag = SUB_DRAG_MOVE;
                else if(SUB_GRAB_WINDOW_RESIZE == flag) 
                  flag = wx < c->rect.width / 2 ? SUB_DRAG_RESIZE_LEFT : SUB_DRAG_RESIZE_RIGHT;

                subClientDrag(c, flag);
              }
            break; /* }}} */
          case SUB_GRAB_GRAVITY: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))))
              {
                vid = subArrayIndex(subtle->views, (void *)subtle->cv);

                c->gravity        = -1; ///< Force 
                c->gravities[vid] = g->number;

                if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
              }
            break; /* }}} */
          case SUB_GRAB_EXEC: /* {{{ */
            if(g->string) EventExec(g->string);
            break; /* }}} */
          case SUB_GRAB_VIEW_JUMP: /* {{{ */
            if(0 <= g->number && g->number < subtle->views->ndata)
              subViewJump(VIEW(subtle->views->data[g->number]));
            break; /* }}} */
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
  else if(ev->xany.window == subtle->cv->win)
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
  SubClient *c = NULL;
  SubTray *t = NULL;

  if((c = CLIENT(subSharedFind(ev->window, CLIENTID)))) ///< Clients
    { 
      if(FocusIn == ev->type) ///< FocusIn event
        {
          subtle->windows.focus = c->win;

          XMapWindow(subtle->disp, subtle->windows.caption);

          subGrabSet(c->win);
          subClientRender(c);

          /* EWMH: Active window */
          subEwmhSetWindows(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW, &c->win, 1);

          subViewUpdate();        
        }
      else if(FocusOut == ev->type) ///< FocusOut event
        {
          if(!(c->flags & SUB_STATE_DEAD)) ///< Don't revive
            {
              Window focus = subtle->windows.focus;

              subtle->windows.focus = 0;
              
              subGrabUnset(focus);
              subClientRender(c);
            }
            
          if(!(VISIBLE(subtle->cv, c))) ///< Unmap caption if window is not on view
            XUnmapWindow(subtle->disp, subtle->windows.caption); 

          subViewRender();
        }
    }
  else if((t = TRAY(subSharedFind(ev->window, TRAYID)))) ///< Tray
    {
      int opcode = XEMBED_WINDOW_DEACTIVATE;

      if(FocusIn == ev->type) 
        {
          opcode = XEMBED_WINDOW_ACTIVATE;

          /* EWMH: Active window */
          subEwmhSetWindows(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW, &t->win, 1);
        }

      subEwmhMessage(ev->window, ev->window, SUB_EWMH_XEMBED, CurrentTime, 
        opcode, 0, 0, 0);
    }
} /* }}} */

 /** subEventLoop {{{ 
  * @brief Event all X events
  **/

void
subEventLoop(void)
{
  int nfds;
  time_t ctime;
  XEvent ev;
  fd_set rfds;
  struct timeval tv;
  SubSublet *s = NULL;

#ifdef HAVE_SYS_INOTIFY_H
  char buf[BUFLEN];
#endif /* HAVE_SYS_INOTIFY_H */

  tv.tv_sec  = 0; 
  tv.tv_usec = 0;
  nfds       = ConnectionNumber(subtle->disp) + 1;
  s          = 0 < subtle->sublets->ndata ? SUBLET(subtle->sublets->data[0]) : NULL;

  FD_ZERO(&rfds);
  FD_SET(ConnectionNumber(subtle->disp), &rfds);

#ifdef HAVE_SYS_INOTIFY_H
  FD_SET(subtle->notify, &rfds); ///< Add inotify socket to set
  nfds = nfds < subtle->notify + 1 ? subtle->notify + 1: nfds; ///< Find biggest fd number
#endif /* HAVE_SYS_INOTIFY_H */

  while(1)
    {
      ctime = subSharedTime();
      if(select(nfds, &rfds, NULL, NULL, &tv)) ///< Data ready on any connection
        {
          if(FD_ISSET(ConnectionNumber(subtle->disp), &rfds)) ///< X connection
            {
              while(XPending(subtle->disp)) ///< X events
                {
                  XNextEvent(subtle->disp, &ev);
                  switch(ev.type)
                    {
                      case ConfigureRequest:  EventConfigure(&ev.xconfigurerequest);     break;
                      case MapRequest:        EventMapRequest(&ev.xmaprequest);          break;
                      case MapNotify:         EventMap(&ev.xmap);                        break;
                      case UnmapNotify:       EventUnmap(&ev.xunmap);                    break;
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
            if(FD_ISSET(subtle->notify, &rfds)) ///< Inotify descriptor
              {
                if(0 < read(subtle->notify, buf, BUFLEN)) ///< Inotify events
                  {
                    struct inotify_event *event = (struct inotify_event *)&buf[0];

                    if(event && (s = SUBLET(subSharedFind(subtle->windows.sublets, event->wd))))
                      {
                        subRubyRun(s);
                        subSubletUpdate();
                        subSubletRender();
                      }
                  }
              }
#endif /* HAVE_SYS_INOTIFY_H */
        }
      else ///< Timeout waiting for data or error
        {
          /* Update sublet data */
          s = 0 < subtle->sublets->ndata ? SUBLET(subtle->sublets->data[0]) : NULL;
          while(s && s->time <= ctime)
            {
              s->time  = ctime + s->interval; ///< Adjust seconds
              s->time -= s->time % s->interval;

              if(!(s->flags & SUB_DATA_INOTIFY)) subRubyRun(s);
              subArraySort(subtle->sublets, subSubletCompare);

              s = SUBLET(subtle->sublets->data[0]);
            }
          subSubletUpdate();
          subSubletRender();
        }

      /* Set timeout and assemble FD_SET */
      tv.tv_sec  = s ? abs(s->time - ctime) : 60;
      tv.tv_usec = 0;

      FD_ZERO(&rfds);
      FD_SET(ConnectionNumber(subtle->disp), &rfds);

#ifdef HAVE_SYS_INOTIFY_H
      FD_SET(subtle->notify, &rfds); ///< Add inotify socket to set
#endif /* HAVE_SYS_INOTIFY_H */
    }
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
