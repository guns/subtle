
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
        subUtilLogWarn("Can't exec command `%s'.\n", cmd);
        exit(1);
      case -1: subUtilLogWarn("Failed to fork.\n");
    }
} /* }}} */

/* HandleGrab {{{ */
static void
HandleGrab(XEvent *ev)
{
  Window win = 0;
  SubGrab *g = NULL;
  SubClient *c = NULL;
  unsigned int code = 0, state = 0;

  /* Distinct types */
  switch(ev->type)
    {
      case ButtonPress: 
        if(ev->xany.window == subtle->bar.views) ///< View buttons
          {
            SubView *v = VIEW(subUtilFind(ev->xbutton.subwindow, 1));
            if(subtle->cv != v) subViewJump(v); ///< Prevent jumping to current view

            return;
          }      

        win   = ev->xbutton.subwindow;
        code  = XK_Pointer_Button1 + ev->xbutton.button;
        state = ev->xbutton.state;
        break;
      case KeyPress:    
        win   = ev->xkey.subwindow;
        code  = ev->xkey.keycode;  
        state = ev->xkey.state; 
        break;
    }

  /* Grabs */
  g = subGrabFind(code, state);
  if(g) 
    {
      switch(g->flags & ~(SUB_TYPE_GRAB|SUB_GRAB_KEY|SUB_GRAB_MOUSE)) ///< Clear
        {
          case SUB_GRAB_WINDOW_RAISE:
            c = CLIENT(subUtilFind(win, CLIENTID));
            if(c && (c->flags & SUB_STATE_FLOAT || c->tags & SUB_TAG_FLOAT))
              XRaiseWindow(subtle->disp, c->win);
            break;
          case SUB_GRAB_WINDOW_MOVE:
            c = CLIENT(subUtilFind(win, CLIENTID));
            if(c && (c->flags & SUB_STATE_FLOAT || c->tags & SUB_TAG_FLOAT))
              subClientDrag(c, SUB_DRAG_MOVE);
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

      subUtilLogDebug("Grab: code=%03d, mod=%03d\n", g->code, g->mod);
    }
} /* }}} */

/* HandleConfigure {{{ */
static void
HandleConfigure(XConfigureRequestEvent *ev)
{
  XWindowChanges wc;
  SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
  if(c)
    {
      if(ev->value_mask & CWX)      c->rect.x       = ev->x;
      if(ev->value_mask & CWY)      c->rect.y       = ev->y;
      if(ev->value_mask & CWWidth)  c->rect.width   = ev->width;
      if(ev->value_mask & CWHeight)  c->rect.height = ev->height;

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

/* HandleMapRequest {{{ */
static void
HandleMapRequest(XMapRequestEvent *ev)
{
  SubClient *c = CLIENT(subUtilFind(ev->window, 1));
  if(!c) 
    {
      /* Create new client */
      c = subClientNew(ev->window);
      subArrayPush(subtle->clients, c);
      subClientPublish();

      if(subtle->cv && subtle->cv->tags & c->tags) subViewConfigure(subtle->cv); ///< Configure current view if tags match
      subViewUpdate();
      subViewRender();
    }
} /* }}} */

/* HandleDestroy {{{ */
static void
HandleDestroy(XDestroyWindowEvent *ev)
{
  SubClient *c = (SubClient *)subUtilFind(ev->event, 1);
  if(c) 
    {
      c->flags |= SUB_STATE_DEAD;
      subClientKill(c); 
      subClientPublish();
      subViewConfigure(subtle->cv);
      subViewUpdate();
    }
} /* }}} */

/* HandleMessage {{{ */
static void
HandleMessage(XClientMessageEvent *ev)
{
  SubClient *c = NULL;

  subUtilLogDebug("ClientMessage: type=%ld, format=%d\n", ev->message_type, ev->format);

  /* ICCM */
  if(DefaultRootWindow(subtle->disp) == ev->window && 32 == ev->format)
    {
      if(ev->message_type == subEwmhFind(SUB_EWMH_NET_CURRENT_DESKTOP)) /* {{{ */
        {
          if(0 <= ev->data.l[0] && ev->data.l[0] < subtle->views->ndata) ///< Bound checking
            subViewJump(VIEW(subtle->views->data[ev->data.l[0]]));
        } /* }}} */
      else if(ev->message_type == subEwmhFind(SUB_EWMH_NET_ACTIVE_WINDOW)) /* {{{ */
        {
          SubClient *focus = CLIENT(subUtilFind(ev->data.l[0], 1));
          if(focus) subClientFocus(focus);
        } /* }}} */
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_CLIENT_TAG)) /* {{{ */
        {
          c = CLIENT(subUtilFind(ev->data.l[0], 1));
          if(c)
            {
              c->tags |= (1L << ev->data.l[1]);
              subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);
            }
        } /* }}} */        
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_CLIENT_UNTAG)) /* {{{ */
        {
          Window win = ev->data.l[0];

          c = CLIENT(subUtilFind(win, 1));
          if(c)
            {
              c->tags &= ~(1L << ev->data.l[1]);
              subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);
            }
          if(subtle->cv->tags & (1L << ev->data.l[1])) subViewConfigure(subtle->cv);
        } /* }}} */        
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_TAG_NEW)) /* {{{ */
        {
          SubTag *t = subTagNew(ev->data.b, NULL); 
          subArrayPush(subtle->tags, (void *)t);
          subTagPublish();
        } /* }}} */
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_TAG_KILL)) /* {{{ */
        {
          int id;
          SubTag *t = subTagFind(ev->data.b, &id);
          if(t)
            {
              int i;

              /* Find clients tagged with this tag */
              for(i = 0; i < subtle->clients->ndata; i++)
                {
                  c = CLIENT(subtle->clients->data[i]);

                  if(c->tags & id)
                    {
                      c->tags &= ~id;
                      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);
                    }
                }

              subArrayPop(subtle->tags, (void *)t);
              subTagKill(t);
              subTagPublish();
              subViewConfigure(subtle->cv); ///< Re-configure current view
            }
        }  /* }}} */      
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_VIEW_NEW)) /* {{{ */
        {
          if(ev->data.b)
            {
              SubView *v = subViewNew(ev->data.b, NULL); 
              subArrayPush(subtle->views, (void *)v);
              subViewUpdate();
              subViewPublish();
              subViewRender();
            }
        }  /* }}} */      
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_VIEW_TAG)) /* {{{ */
        {
          SubView *v = VIEW(subUtilFind(ev->data.l[0], 2));
          if(v)
            {
              v->tags |= (1L << ev->data.l[1]);
              subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS, (long *)&v->tags, 1);
              if(subtle->cv == v) subViewConfigure(v);
            }
        } /* }}} */              
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_VIEW_UNTAG)) /* {{{ */
        {
          SubView *v = VIEW(subUtilFind(ev->data.l[0], 2));
          if(v)
            {
              v->tags &= ~(1L << ev->data.l[1]);
              subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS, (long *)&v->tags, 1);
              if(subtle->cv == v) subViewConfigure(v);
            }
        } /* }}} */              
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_VIEW_KILL)) /* {{{ */
        {
          SubView *v = subViewFind(ev->data.b, NULL);

          if(v)
            {
              subArrayPop(subtle->views, (void *)v);
              subViewKill(v);
              subViewUpdate();
              subViewPublish();
            }
        }  /* }}} */        
      return;
    }

  c = (SubClient *)subUtilFind(ev->window, 1);
  if(c && 32 == ev->format)
    {
      if(ev->message_type == subEwmhFind(SUB_EWMH_NET_WM_STATE))
        {
          /* [0] => Remove = 0 / Add = 1 / Toggle = 2 -> we _always_ toggle */
          if(ev->data.l[1] == (long)subEwmhFind(SUB_EWMH_NET_WM_STATE_FULLSCREEN))
            {
              subClientToggle(c, SUB_STATE_FULL, True);
            }
        }
    }
} /* }}} */

/* HandleColormap {{{ */
static void
HandleColormap(XColormapEvent *ev)
{  
  SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
  if(c && ev->new)
    {
      c->cmap = ev->colormap;
      XInstallColormap(subtle->disp, c->cmap);
    }
} /* }}} */

/* HandleProperty {{{ */
static void
HandleProperty(XPropertyEvent *ev)
{
  /* Prevent expensive query if the atom isn't supported */
  if(ev->atom == XA_WM_NAME || ev->atom == subEwmhFind(SUB_EWMH_WM_NAME))
    {
      SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
      if(c) subClientFetchName(c);
    }
} /* }}} */

/* HandleCrossing {{{ */
static void
HandleCrossing(XCrossingEvent *ev)
{
  SubClient *c = CLIENT(subUtilFind(ev->window, 1));
  if(c && !(c->flags & SUB_STATE_DEAD))
    {
      XEvent event;
    
      if(subtle->focus != c->win) subClientFocus(c);

      /* Remove any other event of the same type and window */
      while(XCheckTypedWindowEvent(subtle->disp, ev->window, ev->type, &event));      
    }
} /* }}} */

/* HandleExpose {{{ */
static void
HandleExpose(XEvent *ev)
{
  XEvent event;

  if(ev->xany.window == subtle->bar.win)
    {
      subViewRender();
      subSubletRender();
    }
  else
    {
      SubClient *c = (SubClient *)subUtilFind(ev->xany.window, 1);
      if(c) subClientRender(c);
    }

  /* Remove any other event of the same type and window */
  while(XCheckTypedWindowEvent(subtle->disp, ev->xany.window, ev->type, &event));
} /* }}} */

/* HandleFocus {{{ */
static void
HandleFocus(XFocusChangeEvent *ev)
{
  SubClient *c = CLIENT(subUtilFind(ev->window, CLIENTID));
  if(c)
    { 
      if(FocusOut == ev->type) ///< FocusOut event
        {
          /* Remove focus from client */
          if(subtle->focus)
            {
              SubClient *f = NULL;
              Window oldfocus = subtle->focus;
              subtle->focus = 0;

              if((f = CLIENT(subUtilFind(oldfocus, CLIENTID))))
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
          subtle->focus = c->win;

          subGrabSet(c->win);
          subClientRender(c);
          subEwmhSetWindows(DefaultRootWindow(subtle->disp), 
            SUB_EWMH_NET_ACTIVE_WINDOW, &c->win, 1);
          subViewUpdate();
        }
    }
} /* }}} */

 /** subEventLoop {{{ 
  * @brief Handle all X events 
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
      ctime  = subUtilTime();
      ret = select(ConnectionNumber(subtle->disp) + 1, &rfds, NULL, NULL, &tv);
      if(-1 == ret) 
        {
          subUtilLogDebug("%s\n", strerror(errno)); ///< Ignore and print debugging message
        }
      else if(ret) ///< Data ready on any connection
        {
          if(FD_ISSET(ConnectionNumber(subtle->disp), &rfds)) ///< X connection
            {
              /* Handle X events */
              while(XPending(subtle->disp))
                {
                  XNextEvent(subtle->disp, &ev);
                  switch(ev.type)
                    {
                      case ConfigureRequest:  HandleConfigure(&ev.xconfigurerequest);  break;
                      case MapRequest:        HandleMapRequest(&ev.xmaprequest);       break;
                      case DestroyNotify:     HandleDestroy(&ev.xdestroywindow);       break;
                      case ClientMessage:     HandleMessage(&ev.xclient);              break;
                      case ColormapNotify:    HandleColormap(&ev.xcolormap);           break;
                      case PropertyNotify:    HandleProperty(&ev.xproperty);           break;
                      case EnterNotify:       HandleCrossing(&ev.xcrossing);           break;

                      case ButtonPress:
                      case KeyPress:          HandleGrab(&ev);                         break;
                      case VisibilityNotify:  
                      case Expose:            HandleExpose(&ev);                       break;
                      case FocusIn:           
                      case FocusOut:          HandleFocus(&ev.xfocus);                 break;
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
                        SubSublet *ws = SUBLET(subUtilFind(subtle->bar.sublets, event->wd));
                        if(ws)
                          {
                            subRubyCall(ws);
                            subSubletConfigure();
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

              subRubyCall(s);
              subArraySort(subtle->sublets, subSubletCompare);

              s = SUBLET(subtle->sublets->data[0]);
            }
          subSubletConfigure();
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
