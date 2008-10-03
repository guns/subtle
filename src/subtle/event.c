
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

#include "subtle.h"

#ifdef HAVE_SYS_INOTIFY_H
#define BUFLEN (sizeof(struct inotify_event))
#endif

/* Exec {{{ */
static void
Exec(char *cmd)
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

/* GetParent {{{ */
static Window
GetParent(Window win)
{
  unsigned int nwins;
  Window parent, unused, *wins = NULL;

  /* Some events don't propagate but we need them to do so */
  XQueryTree(subtle->disp, win, &unused, &parent, &wins, &nwins);
  XFree(wins);

  return(parent);
} /* }}} */

/* HandleButtonPress {{{ */
static void
HandleButtonPress(XButtonEvent *ev)
{
  SubClient *c = NULL;
  static Time last_time = 0;

  if(ev->window == subtle->bar.views)
    {
      SubView *v = VIEW(subUtilFind(ev->subwindow, 1));
      if(subtle->cv != v) subViewJump(v); ///< Prevent jumping to current view
      return;
    }

  c = CLIENT(subUtilFind(ev->window, 1));
  if(c)
    {
      switch(ev->button)
        {
          case Button1:
            if(last_time > 0 && ev->time - last_time <= 300) ///< Double click
              {
                subUtilLogDebug("click=double, win=%#lx\n", ev->window);
                if(ev->subwindow == c->title) subClientToggle(c, SUB_STATE_SHADE);
                last_time = 0;
              }            
            else ///< Single click
              {
                subUtilLogDebug("click=single, win=%#lx\n", ev->window);
                if(c->flags & SUB_STATE_FLOAT) XRaiseWindow(subtle->disp, c->frame);

                if(ev->subwindow == c->left)        subClientDrag(c, SUB_DRAG_LEFT);
                else if(ev->subwindow == c->right)  subClientDrag(c, SUB_DRAG_RIGHT);
                else if(ev->subwindow == c->bottom) subClientDrag(c, SUB_DRAG_BOTTOM);
                else if(ev->subwindow == c->title)
                  { 
                    /* Either drag and move or drag an swap windows */
                    subClientDrag(c, (c->flags & SUB_STATE_FLOAT) ? SUB_DRAG_MOVE : SUB_DRAG_SWAP);
                    last_time = ev->time;
                  }
              }
            break;
          case Button2:
            if(ev->subwindow == c->title) subClientKill(c);
            break;
          case Button3: 
            if(ev->subwindow == c->title) subClientToggle(c, SUB_STATE_FLOAT);
            break;
/*          case Button4: 
          case Button5: 
            
            subViewJump(subtle->cv->next); break;
            if(subtle->cv->prev) subViewJump(subtle->cv->prev); break; */
        }
    }
} /* }}} */

/* HandleKeyPress {{{ */
static void
HandleKeyPress(XKeyEvent *ev)
{
  SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
printf("%s,%d: %s\n", __FILE__, __LINE__, __func__);

  if(c || ev->window == subtle->cv->frame)
    {
      SubKey *k = subKeyFind(ev->keycode, ev->state);
      if(k) 
        {
          if(k->flags & SUB_KEY_EXEC && k->string) Exec(k->string); 
          else if(k->flags & SUB_KEY_VIEW_JUMP) 
            {
              if(0 <= k->number && k->number < subtle->views->ndata)
                subViewJump(VIEW(subtle->views->data[k->number]));
            }
          else if(k->flags & SUB_KEY_VIEW_MNEMONIC)
            {
              KeySym sym = subKeyGet();
              SubView *v = VIEW(subUtilFind(subtle->bar.views, (int)sym));
              if(v) subViewJump(v);
            }

          subUtilLogDebug("KeyPress: code=%d, mod=%d\n", k->code, k->mod);
        }
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

  /* ICC events */
  if(DefaultRootWindow(subtle->disp) == ev->window && 32 == ev->format)
    {
      if(ev->message_type == subEwmhFind(SUB_EWMH_NET_CURRENT_DESKTOP)) /* {{{ */
        {
          if(0 <= ev->data.l[0] && ev->data.l[0] < subtle->views->ndata) ///< Bound checking
            subViewJump(VIEW(subtle->views->data[ev->data.l[0]]));
        } /* }}} */
      else if(ev->message_type == subEwmhFind(SUB_EWMH_NET_ACTIVE_WINDOW)) /* {{{ */
        {
          SubClient *focus = CLIENT(subUtilFind(GetParent(ev->data.l[0]), 1));
          if(focus) subClientFocus(focus);
        } /* }}} */
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_CLIENT_TAG)) /* {{{ */
        {
          Window win = GetParent(ev->data.l[0]);

          c = CLIENT(subUtilFind(win, 1));
          if(c)
            {
              c->tags |= (1L << ev->data.l[1]);
              subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);
            }
        } /* }}} */        
      else if(ev->message_type == subEwmhFind(SUB_EWMH_SUBTLE_CLIENT_UNTAG)) /* {{{ */
        {
          Window win = GetParent(ev->data.l[0]);

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
          SubTag *t = subTagFind(ev->data.b, NULL);

          if(t)
            {
              subArrayPop(subtle->tags, (void *)t);
              subTagPublish();
            }
        }  /* }}} */        
      return;
    }

  c = (SubClient *)subUtilFind(GetParent(ev->window), 1);
  if(c && 32 == ev->format)
    {
      if(ev->message_type == subEwmhFind(SUB_EWMH_NET_WM_STATE))
        {
          /* [0] => Remove = 0 / Add = 1 / Toggle = 2 -> we _always_ toggle */
          if(ev->data.l[1] == (long)subEwmhFind(SUB_EWMH_NET_WM_STATE_FULLSCREEN))
            {
              subClientToggle(c, SUB_STATE_FULL);
            }
          else if(ev->data.l[1] == (long)subEwmhFind(SUB_EWMH_NET_WM_STATE_SHADED))
            {
              subClientToggle(c, SUB_STATE_SHADE);
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
      SubClient *c = (SubClient *)subUtilFind(GetParent(ev->window), 1);
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
    
      if(subtle->focus != c->frame) subClientFocus(c);

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
HandleFocus(XFocusInEvent *ev)
{
  SubClient *c = (SubClient *)subUtilFind(ev->window, 1);
  if(c && c->flags & SUB_PREF_FOCUS)
    {
      /* Remove focus from client */
      if(subtle->focus != c->frame) 
        {
          Window win = subtle->focus;
          SubClient *f = CLIENT(subUtilFind(win, 1));
          if(f)
            {
              if(!(f->flags & SUB_STATE_DEAD)) 
                {
                  subKeyUngrab(f->frame);
                  subClientRender(f);
                }
            }
          else subKeyUngrab(win);

          subKeyGrab(c->frame);  
          subClientRender(c);
          subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_ACTIVE_WINDOW, &c->win, 1);
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
                      case ButtonPress:       HandleButtonPress(&ev.xbutton);          break;
                      case KeyPress:          HandleKeyPress(&ev.xkey);                break;
                      case ConfigureRequest:  HandleConfigure(&ev.xconfigurerequest);  break;
                      case MapRequest:        HandleMapRequest(&ev.xmaprequest);       break;
                      case DestroyNotify:     HandleDestroy(&ev.xdestroywindow);       break;
                      case ClientMessage:     HandleMessage(&ev.xclient);              break;
                      case ColormapNotify:    HandleColormap(&ev.xcolormap);           break;
                      case PropertyNotify:    HandleProperty(&ev.xproperty);           break;
                      case EnterNotify:       HandleCrossing(&ev.xcrossing);           break;
                      case VisibilityNotify:  
                      case Expose:            HandleExpose(&ev);                       break;
                      case FocusIn:           HandleFocus(&ev.xfocus);                 break;
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
