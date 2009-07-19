
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

  for(i = id; i < subtle->tags->ndata - 1; i++)
    {
      tag = (1L << (i + 1));

      if(c->tags & (1L << (i + 2))) ///< Next bit
        c->tags |= tag;
      else
        c->tags &= ~tag;
    }

  /* EWMH: Tags */
  subEwmhSetCardinals(c->flags & SUB_TYPE_VIEW ? VIEW(c)->button : c->win, 
    SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&c->tags, 1);
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

        subSharedLogWarn("Failed executing command `%s'\n", cmd); ///< Never to be reached
        exit(1);
      case -1: subSharedLogWarn("Failed forking `%s'\n", cmd);
    }
} /* }}} */

/* EventFindSublet {{{ */
static SubSublet *
EventFindSublet(int id)
{
  int i = 0;
  SubSublet *iter = subtle->sublet;

  while(iter)
    {
      if(i++ == id) break;
      iter = iter->next;
    }

  return iter;
} /* }}} */

/* EventConfigure {{{ */
static void
EventConfigure(XConfigureRequestEvent *ev)
{
  XWindowChanges wc;

  wc.x          = ev->x;
  wc.y          = ev->y;
  wc.width      = ev->width;
  wc.height     = ev->height;
  wc.sibling    = ev->above;
  wc.stack_mode = ev->detail;

  XConfigureWindow(subtle->dpy, ev->window, ev->value_mask, &wc); 
} /* }}} */

/* EventMapRequest {{{ */
static void
EventMapRequest(XMapRequestEvent *ev)
{
  SubClient *c = NULL;

  if(!(c = CLIENT(subSharedFind(ev->window, CLIENTID))))
    {
      if((c = subClientNew(ev->window))) ///< Create new client
        {
          subArrayPush(subtle->clients, (void *)c);
          subClientPublish();

          if(VISIBLE(subtle->cv, c)) ///< Check visibility first
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
      subArrayRemove(subtle->clients, (void *)c);
      subClientPublish();
      if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
      subClientKill(c, False);
    }
  else if((t = TRAY(subSharedFind(ev->window, TRAYID))))
    {
      subEwmhSetWMState(t->win, WithdrawnState);
      subArrayRemove(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
      subSubletUpdate();
    }    
} /* }}} */

/* EventDestroy {{{ */
static void
EventDestroy(XDestroyWindowEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  if((c = CLIENT(subSharedFind(ev->event, CLIENTID)))) ///< Client
    {
      c->flags |= SUB_STATE_DEAD;

      subArrayRemove(subtle->clients, (void *)c);
      subClientPublish();
      if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
      subClientKill(c, True); 
    }
  else if((t = TRAY(subSharedFind(ev->event, TRAYID)))) ///< Tray
    {
      subArrayRemove(subtle->trays, (void *)t);
      subTrayKill(t);
      subTrayUpdate();
      subSubletUpdate();
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

      SubSublet *s = NULL;
      SubTag *t = NULL;
      SubView *v = NULL;

      switch(id)
        {
          case SUB_EWMH_NET_CURRENT_DESKTOP: /* {{{ */
            if(0 <= ev->data.l[0] && ev->data.l[0] < subtle->views->ndata)
              {
                v = VIEW(subtle->views->data[ev->data.l[0]]);

                if(subtle->cv != v) subViewJump(v);
              }
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

                subClientWarp(c);
                subClientFocus(c);
              }
            break; /* }}} */
          case SUB_EWMH_NET_RESTACK_WINDOW: /* {{{ */
            if((c = CLIENT(subSharedFind(ev->data.l[1], CLIENTID))))
              {        
                switch(ev->data.l[2])
                  {
                    case Above: XRaiseWindow(subtle->dpy, c->win); break;
                    case Below: XLowerWindow(subtle->dpy, c->win); break;
                    default:
                      subSharedLogDebug("Restack: Ignored restack event (%d)\n", ev->data.l[2]);
                  }
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
                    
                      subEwmhSetCardinals(v->button, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1);
                      if(subtle->cv == v) subViewConfigure(v);
                    } 
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_GRAVITY: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))))
              {
                int vid = -1 != ev->data.l[1] ? ev->data.l[1] :
                  subArrayIndex(subtle->views, (void *)subtle->cv);

                c->gravity        = -1; ///< Force update
                c->gravities[vid] = ev->data.l[2];

                if(VISIBLE(subtle->cv, c)) 
                  {
                    if(1 <= ev->data.l[2] && 9 >= ev->data.l[2]) ///< Check values
                      {
                        subClientSetGravity(c, ev->data.l[2]);

                        vid               = subArrayIndex(subtle->views, (void *)subtle->cv);
                        c->gravities[vid] = c->gravity;

                        subClientWarp(c);
                        XRaiseWindow(subtle->dpy, c->win);                
                      }
                  }
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_WINDOW_SCREEN: /* {{{ */
            if((c = CLIENT(subArrayGet(subtle->clients, (int)ev->data.l[0]))))
              {
                if(1 <= ev->data.l[1] && subtle->screens->ndata >= ev->data.l[1]) ///< Check values
                  {
                    c->screen  = ev->data.l[1] - 1;
                    c->gravity = -1; ///< Force update

                    subClientSetSize(c);

                    /* EWMH: Screen */
                    subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_SCREEN, (long *)&c->screen, 1);

                    if(VISIBLE(subtle->cv, c)) 
                      subViewConfigure(subtle->cv);
                  }
              }
            break; /* }}} */            
          case SUB_EWMH_SUBTLE_TAG_NEW: /* {{{ */
            t = subTagNew(ev->data.b, NULL, 0, 0, 0); 
            subArrayPush(subtle->tags, (void *)t);
            subTagPublish();
            break; /* }}} */
          case SUB_EWMH_SUBTLE_TAG_KILL: /* {{{ */
            if((t = TAG(subArrayGet(subtle->tags, (int)ev->data.l[0]))))
              {
                int i, reconf = subtle->cv->tags & (1L << ((int)ev->data.l[0] + 1));

                for(i = 0; i < subtle->views->ndata; i++) ///< Views
                  EventUntag(CLIENT(subtle->views->data[i]), (int)ev->data.l[0]);

                for(i = 0; i < subtle->clients->ndata; i++) ///< Clients
                  EventUntag(CLIENT(subtle->clients->data[i]), (int)ev->data.l[0]);
              
                subArrayRemove(subtle->tags, (void *)t);
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
                subClientUpdate(-1); ///< Grow
                subViewUpdate();
                subViewPublish();
                subViewRender();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_VIEW_KILL: /* {{{ */
            if((v = VIEW(subArrayGet(subtle->views, (int)ev->data.l[0]))))
              {
                int vid = subArrayIndex(subtle->views, (void *)v);

                subArrayRemove(subtle->views, (void *)v);
                subClientUpdate(vid); ///< Shrink
                subViewKill(v);
                subViewUpdate();
                subViewPublish();

                if(subtle->cv == v) subViewJump(VIEW(subtle->views->data[0])); 
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_NEW: /* {{{ */
            if(ev->data.b)
              {
                subRubyLoadSublet(ev->data.b);
                subArraySort(subtle->sublets, subSubletCompare);
                subSubletUpdate();
                subSubletPublish();
                subTrayUpdate();
                subSubletRender(); 
              }
            break; /* }}} */            
          case SUB_EWMH_SUBTLE_SUBLET_UPDATE: /* {{{ */
            if((s = EventFindSublet((int)ev->data.l[0])))
              {
                subRubyCall(SUB_TYPE_SUBLET, s->recv, NULL);
                subSubletUpdate();
                subSubletRender();                
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_SUBLET_KILL: /* {{{ */
            if((s = EventFindSublet((int)ev->data.l[0])))
              {
                subArrayRemove(subtle->sublets, (void *)s);
                subArraySort(subtle->sublets, subSubletCompare);
                subSubletKill(s, True);
                subSubletUpdate();
                subSubletPublish();
                subTrayUpdate();
              }
            break; /* }}} */
          case SUB_EWMH_SUBTLE_RELOAD: /* {{{ */
            raise(SIGHUP);
            break; /* }}} */
          case SUB_EWMH_SUBTLE_QUIT: /* {{{ */
            raise(SIGTERM);
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
                      subSubletUpdate();
                      subSubletRender();
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
  else if((c = CLIENT(subSharedFind(ev->window, CLIENTID))));
    {
      switch(subEwmhFind(ev->message_type))
        {
          /* Actions: 0 = remove / 1 = add / 2 = toggle - we always toggle */
          case SUB_EWMH_NET_WM_STATE: /* {{{ */
            switch(subEwmhFind(ev->data.l[1])) ///< Only the first property
              {
                case SUB_EWMH_NET_WM_STATE_FULLSCRN:
                  subClientToggle(c, SUB_STATE_FULL);
                  break;
                case SUB_EWMH_NET_WM_STATE_ABOVE:
                  subClientToggle(c, SUB_STATE_FLOAT);
                  break;
                case SUB_EWMH_NET_WM_STATE_STICKY:
                  subClientToggle(c, SUB_STATE_STICK);
                  break;
                default: break;
              }

            if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
            break; /* }}} */
          case SUB_EWMH_NET_CLOSE_WINDOW: /* {{{ */
            subArrayRemove(subtle->clients, (void *)c);
            subClientPublish();
            if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
            subClientKill(c, True);
            subViewUpdate();
            break; /* }}} */
          case SUB_EWMH_NET_MOVERESIZE_WINDOW: /* {{{ */
            if(!(c->flags & SUB_STATE_FLOAT)) subClientToggle(c, SUB_STATE_FLOAT);

            c->geom.x      = ev->data.l[1];
            c->geom.y      = ev->data.l[2];
            c->geom.width  = ev->data.l[3];
            c->geom.height = ev->data.l[4];

            subClientSetSize(c);
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
  SubClient *c = (SubClient *)subSharedFind(ev->window, 1);
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
        if((c = CLIENT(subSharedFind(ev->window, CLIENTID)))) 
          {
            char *caption = NULL;

            if(XFetchName(subtle->dpy, c->win, &caption))
              {
                if(c->caption) free(c->caption);
                c->caption = caption;
                c->width   = XTextWidth(subtle->xfs, c->caption, strlen(c->caption)) + 6; ///< Font offset

                if(subtle->windows.focus == c->win) 
                  {
                    subClientRender(c);
                    subViewRender();
                  }
              }
          }
        break; /* }}} */
      case SUB_EWMH_WM_NORMAL_HINTS: /* {{{ */
        if((c = CLIENT(subSharedFind(ev->window, CLIENTID)))) 
          {
            subClientSetHints(c);
          }
        else if((t = TRAY(subSharedFind(ev->window, TRAYID))))
          {
            subTrayConfigure(t);
            subTrayUpdate();
            subViewRender();
          }          
        break; /* }}} */
      case SUB_EWMH_NET_WM_STRUT: /* {{{ */
         if((c = CLIENT(subSharedFind(ev->window, CLIENTID)))) 
          {
            subClientSetStrut(c);
            subScreenUpdate();
            subSharedLogDebug("Hints: Updated strut hints\n");
          }
        break; /* }}} */
      case SUB_EWMH_XEMBED_INFO: /* {{{ */
        if((t = TRAY(subSharedFind(ev->window, TRAYID))))
          {
            subTraySetState(t);
            subTrayUpdate();
            subViewRender();
          }
        break; /* }}} */
#ifdef DEBUG
      default:
        {
          char *name = XGetAtomName(subtle->dpy, ev->atom);

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

  if(ROOT == ev->window)
    {
      subGrabSet(ROOT);
    }
  else if((c = CLIENT(subSharedFind(ev->window, CLIENTID))))
    {
      if(!(c->flags & SUB_STATE_DEAD))
        subClientFocus(c);
    }
  else if((t = TRAY(subSharedFind(ev->window, TRAYID)))) 
    subTrayFocus(t);

  /* Remove any other event of the same type and window */
  while(XCheckTypedWindowEvent(subtle->dpy, ev->window, ev->type, &event));    
} /* }}} */

/* EventSelection {{{ */
void
EventSelection(XSelectionClearEvent *ev)
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
  KeySym sym;

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

        if(ev->xbutton.window != subtle->windows.focus) ///< Update focus
          XSetInputFocus(subtle->dpy, ev->xbutton.window, RevertToNone, CurrentTime);

        code  = XK_Pointer_Button1 + ev->xbutton.button;
        state = ev->xbutton.state;
        break;
      case KeyPress:
        sym   = XKeycodeToKeysym(subtle->dpy, ev->xkey.keycode, 0);
        code  = ev->xkey.keycode;
        state = ev->xkey.state;

        break;
    }

  /* Find grab */
  if((g = subGrabFind(code, sym, state)))
    {
      Window win = 0;
      SubClient *c = NULL;
      FLAGS flag = 0;
      int vid = 0;

      win  = ev->xbutton.window == ROOT ? ev->xbutton.subwindow : ev->xbutton.window;
      flag = g->flags & ~(SUB_TYPE_GRAB|SUB_GRAB_KEY|SUB_GRAB_MOUSE); ///< Clear mask

      switch(flag)
        {
          case SUB_GRAB_EXEC: /* {{{ */
            if(g->data.string) EventExec(g->data.string);
            break; /* }}} */
          case SUB_GRAB_PROC: /* {{{ */
            subRubyCall(SUB_TYPE_GRAB, g->data.num, CLIENT(subSharedFind(win, CLIENTID)));
            break; /* }}} */
          case SUB_GRAB_VIEW_JUMP: /* {{{ */
            if(g->data.num < subtle->views->ndata)
              {
                SubView *v = VIEW(subtle->views->data[g->data.num]);

                if(subtle->cv != v) subViewJump(v);
              }
            break; /* }}} */
          case SUB_GRAB_SCREEN_JUMP: /* {{{ */
            if(g->data.num < subtle->screens->ndata)
              subScreenJump(SCREEN(subtle->screens->data[g->data.num]));
            break; /* }}} */
          case SUB_GRAB_SUBTLE_RELOAD: /* {{{ */
            raise(SIGHUP);
            break; /* }}} */
          case SUB_GRAB_SUBTLE_QUIT: /* {{{ */
            raise(SIGTERM);
            break; /* }}} */
          case SUB_GRAB_WINDOW_MOVE:
          case SUB_GRAB_WINDOW_RESIZE: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))) && !(c->flags & SUB_STATE_FULL))
              {
                if(!(c->flags & SUB_STATE_FLOAT)) subClientToggle(c, SUB_STATE_FLOAT);

                if(SUB_GRAB_WINDOW_MOVE == flag)        flag = SUB_DRAG_MOVE;
                else if(SUB_GRAB_WINDOW_RESIZE == flag) flag = SUB_DRAG_RESIZE;

                subClientDrag(c, flag);
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_TOGGLE: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))))
              {
                subClientToggle(c, g->data.num);
                if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_STACK: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))) && VISIBLE(subtle->cv, c)) 
              {
                if(Above == g->data.num)      XRaiseWindow(subtle->dpy, c->win);
                else if(Below == g->data.num) XLowerWindow(subtle->dpy, c->win);
              }
            break; /* }}} */            
          case SUB_GRAB_WINDOW_SELECT: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))))
              {
                int i, match = 0, score = 0;
                Window found = None;

                /* Iterate once to find a client score-based */
                for(i = 0; 100 != match && i < subtle->clients->ndata; i++)
                  {
                    SubClient *iter = CLIENT(subtle->clients->data[i]);

                    if(c != iter && VISIBLE(subtle->cv, iter))
                      if(match < (score = subSharedMatch(g->data.num, c->gravity, iter->gravity)))
                        {
                          match = score;
                          found = iter->win;
                        }
                  }
                  printf("match=%d\n", match);

                if(found && (c = CLIENT(subSharedFind(found, CLIENTID))))
                  {
                    subClientWarp(c);
                    subClientFocus(c);
                    subSharedLogDebug("Match: win=%#lx, score=%d, iterations=%d\n", found, match, i);
                  }
              }
            else ///< Select the first if no client has focus
              {
                int i;

                for(i = 0; i < subtle->clients->ndata; i++)
                  {
                    SubClient *iter = CLIENT(subtle->clients->data[i]);

                    if(VISIBLE(subtle->cv, iter)) 
                      {
                        subClientFocus(iter);
                        subClientWarp(iter);
                        break;
                      }
                  }
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_GRAVITY: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))))
              {
                if(c->flags & SUB_STATE_FLOAT) subClientToggle(c, SUB_STATE_FLOAT);
                if(c->flags & SUB_STATE_FULL)  subClientToggle(c, SUB_STATE_FULL);

                subClientSetGravity(c, g->data.num);

                vid               = subArrayIndex(subtle->views, (void *)subtle->cv);
                c->gravities[vid] = c->gravity;

                subClientWarp(c);
                XRaiseWindow(subtle->dpy, c->win);
              }
            break; /* }}} */
          case SUB_GRAB_WINDOW_KILL: /* {{{ */
            if((c = CLIENT(subSharedFind(win, CLIENTID))))
              { 
                subArrayRemove(subtle->clients, (void *)c);
                subClientPublish();
                if(VISIBLE(subtle->cv, c)) subViewConfigure(subtle->cv);
                subClientKill(c, True);
              }
            break; /* }}} */
          default:
            subSharedLogWarn("Failed finding grab!\n");
        }

      subSharedLogDebug("Grab: code=%03d, mod=%03d\n", g->code, g->mod);
    }
} /* }}} */

/* EventExpose {{{ */
static void
EventExpose(XEvent *ev)
{
  XEvent event;
  SubClient *c = CLIENT(subSharedFind(subtle->windows.focus, CLIENTID));

  if(c) subClientRender(c); ///< Render caption
  subSubletRender();
  subViewRender();

  /* Remove any other event of the same type and window */
  while(XCheckTypedWindowEvent(subtle->dpy, ev->xany.window, ev->type, &event));
} /* }}} */

/* EventFocus {{{ */
static void
EventFocus(XFocusChangeEvent *ev)
{
  SubClient *c = NULL;
  SubTray *t = NULL;

  /* Check if we are interested in this event */
  if((NotifyNormal != ev->mode || NotifyInferior == ev->detail) &&
    !(NotifyWhileGrabbed == ev->mode && 
    (NotifyNonlinear == ev->detail || NotifyAncestor == ev->detail)))
    {
      subSharedLogDebug("Focus ignore: type=%s, mode=%d, detail=%d, send_event=%d\n", 
        FocusIn == ev->type ? "in" : "out", ev->mode, ev->detail, ev->send_event);
      return;
    }

  /* Handle other focus event */
  if(ROOT == ev->window)
    {
      if(FocusIn == ev->type) 
        {
          subtle->windows.focus = ROOT;

          subGrabSet(ROOT);
        }
      else if(FocusOut == ev->type)
        {
          subtle->windows.focus  = 0;

          subGrabUnset(ROOT);
        }
    }
  else if((c = CLIENT(subSharedFind(ev->window, CLIENTID)))) ///< Clients
    { 
      if(FocusIn == ev->type && VISIBLE(subtle->cv, c)) ///< FocusIn event
        {
          subtle->windows.focus = c->win;

          XMapWindow(subtle->dpy, subtle->windows.caption);

          subGrabSet(c->win);
          subClientRender(c);

          /* EWMH: Active window */
          subEwmhSetWindows(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW, &c->win, 1);
        }
      else if(FocusOut == ev->type && !(c->flags & SUB_STATE_FULL)) ///< FocusOut event
        {
          if(!(c->flags & SUB_STATE_DEAD)) ///< Don't revive
            {
              subtle->windows.focus  = 0;
              
              subGrabUnset(c->win);
              subClientRender(c);
            }
            
          XUnmapWindow(subtle->dpy, subtle->windows.caption); 
        }
    }
  else if((t = TRAY(subSharedFind(ev->window, TRAYID)))) ///< Tray
    {
      int opcodes[3] = { XEMBED_WINDOW_DEACTIVATE, XEMBED_FOCUS_OUT, 0 };

      if(FocusIn == ev->type) 
        {
          opcodes[0] = XEMBED_WINDOW_ACTIVATE;
          opcodes[1] = XEMBED_FOCUS_IN;
          opcodes[2] = XEMBED_FOCUS_CURRENT;

          /* EWMH: Active window */
          subEwmhSetWindows(ROOT, SUB_EWMH_NET_ACTIVE_WINDOW, &t->win, 1);
        }

      subEwmhMessage(ev->window, ev->window, SUB_EWMH_XEMBED, CurrentTime, 
        opcodes[0], 0, 0, 0);
      subEwmhMessage(ev->window, ev->window, SUB_EWMH_XEMBED, CurrentTime, 
        opcodes[1], opcodes[2], 0, 0);          
    }
} /* }}} */

 /** subEventLoop {{{ 
  * @brief Event all X events
  **/

void
subEventLoop(void)
{
  int nfds;
  XEvent ev;
  time_t now;
  fd_set rfds;
  struct timeval tv;

#ifdef HAVE_SYS_INOTIFY_H
  char buf[BUFLEN];
#endif /* HAVE_SYS_INOTIFY_H */

  tv.tv_sec  = 0; 
  tv.tv_usec = 0;
  nfds       = ConnectionNumber(subtle->dpy) + 1;

  subSubletRender(); ///< Render first time

  FD_ZERO(&rfds);
  FD_SET(ConnectionNumber(subtle->dpy), &rfds);

#ifdef HAVE_SYS_INOTIFY_H
  FD_SET(subtle->notify, &rfds); ///< Add inotify socket to set
  nfds = nfds < subtle->notify + 1 ? subtle->notify + 1: nfds; ///< Find biggest fd number
#endif /* HAVE_SYS_INOTIFY_H */

  XSync(subtle->dpy, False); ///< Sync before waiting for data

  while(1)
    {
      now = subSharedTime();
      if(select(nfds, &rfds, NULL, NULL, &tv)) ///< Data ready on any connection
        {
          if(FD_ISSET(ConnectionNumber(subtle->dpy), &rfds)) ///< X connection {{{
            {
              while(XPending(subtle->dpy)) ///< X events
                {
                  XNextEvent(subtle->dpy, &ev);
                  switch(ev.type)
                    {
                      case ConfigureRequest:  EventConfigure(&ev.xconfigurerequest); break;
                      case MapRequest:        EventMapRequest(&ev.xmaprequest);      break;
                      case MapNotify:         EventMap(&ev.xmap);                    break;
                      case UnmapNotify:       EventUnmap(&ev.xunmap);                break;
                      case DestroyNotify:     EventDestroy(&ev.xdestroywindow);      break;
                      case ClientMessage:     EventMessage(&ev.xclient);             break;
                      case ColormapNotify:    EventColormap(&ev.xcolormap);          break;
                      case PropertyNotify:    EventProperty(&ev.xproperty);          break;
                      case EnterNotify:       EventCrossing(&ev.xcrossing);          break;
                      case SelectionClear:    EventSelection(&ev.xselectionclear);   break;
                      case ButtonPress:
                      case KeyPress:          EventGrab(&ev);                        break;
                      case VisibilityNotify:  
                      case Expose:            EventExpose(&ev);                      break;
                      case FocusIn:           
                      case FocusOut:          EventFocus(&ev.xfocus);                break;
                    }
                }            
            } /* }}} */
#ifdef HAVE_SYS_INOTIFY_H
          if(FD_ISSET(subtle->notify, &rfds)) ///< Inotify {{{
            {
              if(0 < read(subtle->notify, buf, BUFLEN)) ///< Inotify events
                {
                  struct inotify_event *event = (struct inotify_event *)&buf[0];

                  if(event && IN_IGNORED != event->mask) ///< Skip unwatch events
                    {
                      SubSublet *s = NULL;

                      if((s = SUBLET(subSharedFind(subtle->windows.sublets, event->wd))))
                        {
                          subRubyCall(SUB_TYPE_SUBLET, s->recv, NULL);
                          subSubletUpdate();
                          subSubletRender();
                          subViewRender();
                        }
                    }
                }
            } /* }}} */
#endif /* HAVE_SYS_INOTIFY_H */
        }
      else ///< Timeout waiting for data or error {{{
        {
          if(0 < subtle->sublets->ndata)
            {
              SubSublet *s = SUBLET(subtle->sublets->data[0]);

              if(!(s->flags & SUB_SUBLET_INOTIFY))
                {
                  while(s && s->time <= now)
                    {
                      subRubyCall(SUB_TYPE_SUBLET, s->recv, NULL);

                      s->time  = now + s->interval; ///< Adjust seconds
                      s->time -= s->time % s->interval;

                      subArraySort(subtle->sublets, subSubletCompare);
                    }
                }

              subViewRender();
              subSubletUpdate();
              subSubletRender();

              XFlush(subtle->dpy);
            }
        } /* }}} */

      /* Set timeout */
      if(0 < subtle->sublets->ndata && 
        !(SUBLET(subtle->sublets->data[0])->flags & SUB_SUBLET_INOTIFY))
        tv.tv_sec = SUBLET(subtle->sublets->data[0])->time - now;
      else tv.tv_sec = 60; 

      if(0 > tv.tv_sec) tv.tv_sec = 0; ///< Sanitize
      tv.tv_usec = 0;

      FD_ZERO(&rfds);
      FD_SET(ConnectionNumber(subtle->dpy), &rfds);

#ifdef HAVE_SYS_INOTIFY_H
      FD_SET(subtle->notify, &rfds); ///< Add inotify socket to set
#endif /* HAVE_SYS_INOTIFY_H */
    }
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
