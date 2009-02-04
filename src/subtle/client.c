
 /**
  * @package subtle
  *
  * @file Client functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <X11/Xatom.h>
#include "subtle.h"

/* ClientMask {{{ */
static void
ClientMask(int state,
  SubClient *c,
  XRectangle *r)
{
  XRectangle rect = { 0, 0, 0, 0 };
  
  /* Init rect */
  if(c)
    {
      rect        = c->rect;
      rect.width  = WINW(c);
      rect.height = WINH(c);
    }

  /* Draw rectangles */
  switch(state)
    {
      case SUB_DRAG_START: 
        XDrawRectangle(subtle->disp, ROOT, subtle->gcs.invert, r->x - 1, r->y - 1,
          r->width - 3, r->height - 3);
        return;
      case SUB_DRAG_TOP:
        rect.x = 5;                          rect.y = 5;
        rect.width -= 10;                    rect.height = rect.height * 0.35 - 10;
        break;
      case SUB_DRAG_BOTTOM:
        rect.x = 5;                          rect.y = rect.height * 0.65; 
        rect.width -= 10;                    rect.height = rect.height * 0.35 - 5; 
        break;
      case SUB_DRAG_LEFT:
        rect.x = 5;                          rect.y = 5; 
        rect.width = rect.width * 0.35 - 5;  rect.height -= 10; 
        break;
      case SUB_DRAG_RIGHT:
        rect.x = rect.width * 0.65;          rect.y = 5; 
        rect.width = rect.width * 0.35 - 5;  rect.height -= 10; 
        break;
      case SUB_DRAG_SWAP:
        rect.x = rect.width * 0.35;          rect.y = rect.height * 0.35; 
        rect.width = rect.width * 0.3;       rect.height *= 0.3; 
        break;
      default:
        return;
    }

  XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, rect.x, rect.y, 
    rect.width, rect.height);
} /* }}} */

/* ClientSnap {{{ */
static void
ClientSnap(SubClient *c,
  XRectangle *r)
{
  assert(c && r);

  if(SNAP > r->x) r->x = subtle->bw;
  else if(r->x > (SCREENW - WINW(c) - SNAP)) r->x = SCREENW - c->rect.width + subtle->bw;
  if(SNAP + subtle->th > r->y) r->y = subtle->bw + subtle->th;
  else if(r->y > (SCREENH - WINH(c) - SNAP)) r->y = SCREENH - c->rect.height + subtle->bw;
} /* }}} */

 /** subClientNew {{{
  * @brief Create new client
  * @param[in]  win  Main window of the new client
  * @return Returns a new #SubClient or \p NULL
  **/

SubClient *
subClientNew(Window win)
{
  int i, n = 0;
  long vid = 1337;
  long supplied = 0;
  Window trans = 0;
  XWMHints *hints = NULL;
  XWindowAttributes attrs;
  Atom *protos = NULL;
  SubClient *c = NULL;

  assert(win);
  
  /* Create client */
  c = CLIENT(subSharedMemoryAlloc(1, sizeof(SubClient)));
  c->flags = SUB_TYPE_CLIENT;
  c->win   = win;

  /* Dimensions */
  c->rect.x      = 0;
  c->rect.y      = subtle->th;
  c->rect.width  = SCREENW;
  c->rect.height = SCREENH - subtle->th;

  /* Fetch name */
  if(!XFetchName(subtle->disp, c->win, &c->name))
    {
      free(c);

      return NULL;
    }

  /* Update client */
  XAddToSaveSet(subtle->disp, c->win);
  XSelectInput(subtle->disp, c->win, StructureNotifyMask|PropertyChangeMask|
    EnterWindowMask|FocusChangeMask|KeyPressMask|ButtonPressMask);
  XSetWindowBorderWidth(subtle->disp, c->win, subtle->bw);
  XSaveContext(subtle->disp, c->win, CLIENTID, (void *)c);
  subEwmhSetWMState(c->win, WithdrawnState);

  /* Window attributes */
  XGetWindowAttributes(subtle->disp, c->win, &attrs);
  c->cmap = attrs.colormap;

  /* Size hints */
  if(!(c->hints = XAllocSizeHints()))
    subSharedLogError("Can't alloc memory. Exhausted?\n");

  XGetWMNormalHints(subtle->disp, c->win, c->hints, &supplied);
  if(0 < supplied) c->flags |= SUB_PREF_HINTS;
  else XFree(c->hints);

  /* Window manager hints */
  if((hints = XGetWMHints(subtle->disp, c->win)))
    {
      if(hints->input) c->flags |= SUB_PREF_INPUT;
      if(hints->flags & XUrgencyHint) c->tags |= SUB_TAG_STICK;
      XFree(hints);
    }
  
  /* Protocols */
  if(XGetWMProtocols(subtle->disp, c->win, &protos, &n))
    {
      for(i = 0; i < n; i++)
        {
          int id = subEwmhFind(protos[i]);

          switch(id)
            {
              case SUB_EWMH_WM_TAKE_FOCUS:    c->flags |= SUB_PREF_FOCUS; break;
              case SUB_EWMH_WM_DELETE_WINDOW: c->flags |= SUB_PREF_CLOSE; break;
#ifdef DEBUG              
              default: 
                {
                  char *name = XGetAtomName(subtle->disp, protos[i]);

                  subSharedLogDebug("Protocols: name=%s, type=%ld\n", name, protos[i]);
                  if(name) XFree(name);
                }
#endif /* DEBG */     
            }
        }
      XFree(protos);
    }

  /* Tags */
  if(c->name)
    for(i = 0; i < subtle->tags->ndata; i++)
      {
        SubTag *t = TAG(subtle->tags->data[i]);

        if(t->preg && subSharedRegexMatch(t->preg, c->name)) c->tags |= (1L << (i + 1));
      }

  /* Special tags */
  if(!(c->tags & ~(SUB_TAG_FLOAT|SUB_TAG_FULL|SUB_TAG_STICK)))
    c->tags |= SUB_TAG_DEFAULT; ///< Ensure that there's at least on tag
  else
    {
      if(c->tags & SUB_TAG_FLOAT) subClientToggle(c, SUB_STATE_FLOAT);
      if(c->tags & SUB_TAG_FULL)  subClientToggle(c, SUB_STATE_FULL);
      if(c->tags & SUB_TAG_STICK) subClientToggle(c, SUB_STATE_STICK|SUB_STATE_FLOAT);
    }

  /* Check for transient windows */
  XGetTransientForHint(subtle->disp, win, &trans); 
  if(trans)
    {
      SubClient *t = CLIENT(subSharedFind(trans, CLIENTID));
      if(t)
        {
          c->flags = t->flags;
          subClientToggle(c, SUB_STATE_STICK|SUB_STATE_FLOAT);
        }
    }

  /* EWMH: Tags and desktop */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

  printf("Adding client (%s)\n", c->name);
  subSharedLogDebug("new=client, name=%s, win=%#lx, supplied=%ld\n", c->name, win, supplied);

  return c;
} /* }}} */

 /** subClientConfigure {{{
  * @brief Send a configure request to client
  * @param[in]  c  A #SubClient
  **/

void
subClientConfigure(SubClient *c)
{
  XRectangle r = { 0 };
  XConfigureEvent ev;

  assert(c);

  /* Client size */
  r = c->rect;
  r.width  = WINW(c);
  r.height = WINH(c);

  if(c->flags & SUB_STATE_FULL) 
    {
      r.x      = 0;
      r.y      = 0;
      r.width  = SCREENW;
      r.height = SCREENH;
    }

  /* Tell client new geometry */
  ev.type              = ConfigureNotify;
  ev.event             = c->win;
  ev.window            = c->win;
  ev.x                 = r.x;
  ev.y                 = r.y;
  ev.width             = r.width;
  ev.height            = r.height;
  ev.above             = None;
  ev.override_redirect = 0;
  ev.border_width      = 0;

  XMoveResizeWindow(subtle->disp, c->win, r.x, r.y, r.width, r.height);
  XSendEvent(subtle->disp, c->win, False, StructureNotifyMask, (XEvent *)&ev);

  subSharedLogDebug("client=%#lx, state=%c, x=%03d, y=%03d, width=%03d, height=%03d\n",
    c->win, c->flags & SUB_STATE_FLOAT ? 'f' : c->flags & SUB_STATE_FULL ? 'u' : 'n',
    r.x, r.y, r.width, r.height);
} /* }}} */

 /** subClientRender {{{
  * @brief Render client and redraw titlebar and borders
  * @param[in]  c  A #SubClient
  **/

void
subClientRender(SubClient *c)
{
  XSetWindowAttributes attrs;

  assert(c);

  attrs.border_pixel = subtle->windows.focus == c->win ? subtle->colors.focus : subtle->colors.norm;
  XChangeWindowAttributes(subtle->disp, c->win, CWBorderPixel, &attrs);

  /* Caption */
  XResizeWindow(subtle->disp, subtle->windows.caption, TEXTW(c->name), subtle->th);
  XClearWindow(subtle->disp, subtle->windows.caption);
  XDrawString(subtle->disp, subtle->windows.caption, subtle->gcs.font, 3, subtle->fy - 1,
    c->name, strlen(c->name));
} /* }}} */

 /** subClientFocus {{{
  * @brief Set or unset focus to client
  * @param[in]  c  A #SubClient
  **/

void
subClientFocus(SubClient *c)
{
  assert(c);
     
  subSharedLogDebug("Focus: win=%#lx, input=%d, focus=%d\n", c->win,
    !!(c->flags & SUB_PREF_INPUT), !!(c->flags & SUB_PREF_FOCUS));

  /* Check if client wants to take focus by itself */
  if(c->flags & SUB_PREF_FOCUS)
    {
      subEwmhMessage(c->win, c->win, SUB_EWMH_WM_PROTOCOLS, 
        subEwmhGet(SUB_EWMH_WM_TAKE_FOCUS), CurrentTime, 0, 0, 0);
    }   
  else XSetInputFocus(subtle->disp, c->win, RevertToNone, CurrentTime);
} /* }}} */

 /** subClientDrag {{{
  * @brief Move and/or drag client
  * @param[in]  c     A #SubClient
  * @param[in]  mode  Drag/move mode
  **/

void
subClientDrag(SubClient *c,
  int mode)
{
  XEvent ev;
  Window win, unused;
  unsigned int mask = 0;
  int loop = True, wx = 0, wy = 0, rx = 0, ry = 0, state = 0, lstate = 0;
  XRectangle r;
  SubClient *c2 = NULL, *lc = NULL;
  short *dirx = NULL, *diry = NULL, minx = 10, miny = 10, maxx = SCREENW, maxy = SCREENH;

  assert(c);
 
  /* Get window position on root window */
  XQueryPointer(subtle->disp, c->win, &win, &win, &rx, &ry, &wx, &wy, &mask);
  r.x      = rx - wx;
  r.y      = ry - wy;
  r.width  = c->rect.width;
  r.height = c->rect.height; 

  /* Directions and steppings  {{{ */
  switch(mode)
    {
      case SUB_DRAG_MOVE:
        dirx = &r.x;
        diry = &r.y;
        break;
      case SUB_DRAG_RESIZE_LEFT:
      case SUB_DRAG_RESIZE_RIGHT:
        dirx = (short *)&r.width;
        diry = (short *)&r.height;

        /* Respect size hints */
        if(c->flags & SUB_PREF_HINTS)
          {
            if(c->hints->flags & PResizeInc) ///< Resize hints
              {
                subtle->step = c->hints->width_inc;
                subtle->step = c->hints->height_inc;
              }
            if(c->hints->flags & PMinSize) ///< Min. size
              {
                minx = MINMAX(c->hints->min_width, MINW, 0);
                miny = MINMAX(c->hints->min_height, MINH, 0);
              }
            if(c->hints->flags & PMaxSize) ///< Max. size
              {
                maxx = c->hints->max_width;
                maxy = c->hints->max_height;
              }
         }
    } /* }}} */

  if(XGrabPointer(subtle->disp, c->win, True, 
    ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None,
    mode & (SUB_DRAG_RESIZE_LEFT|SUB_DRAG_RESIZE_RIGHT) ? subtle->cursors.resize : 
    subtle->cursors.move, CurrentTime)) return;

  XGrabServer(subtle->disp);
  if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE_LEFT|SUB_DRAG_RESIZE_RIGHT)) 
    ClientMask(SUB_DRAG_START, c, &r);

  while(loop) ///< Event loop
    {
      XMaskEvent(subtle->disp, 
        PointerMotionMask|ButtonReleaseMask|KeyPressMask|EnterWindowMask, &ev);
      switch(ev.type)
        {
          case EnterNotify:   win = ev.xcrossing.window; break; ///< Find destination window
          case ButtonRelease: loop = False;              break;
          case KeyPress: /* {{{ */
            if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE_LEFT|SUB_DRAG_RESIZE_RIGHT))
              {
                KeySym sym = XKeycodeToKeysym(subtle->disp, ev.xkey.keycode, 0);
                ClientMask(SUB_DRAG_START, c, &r);

                switch(sym)
                  {
                    case XK_Left:   *dirx -= subtle->step; break;
                    case XK_Right:  *dirx += subtle->step; break;
                    case XK_Up:     *diry -= subtle->step; break;
                    case XK_Down:   *diry += subtle->step; break;
                    case XK_Return: loop = False;   break;
                  }

                *dirx = MINMAX(*dirx, minx, maxx);
                *diry = MINMAX(*diry, miny, maxy);
              
                ClientMask(SUB_DRAG_START, c, &r);
              }
            break; /* }}} */
          case MotionNotify: /* {{{ */
            if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE_LEFT|SUB_DRAG_RESIZE_RIGHT)) /* Move/Resize {{{ */
              {
                ClientMask(SUB_DRAG_START, c, &r);
          
                /* Calculate selection rect */
                switch(mode)
                  {
                    case SUB_DRAG_MOVE:
                      r.x = (rx - wx) - (rx - ev.xmotion.x_root);
                      r.y = (ry - wy) - (ry - ev.xmotion.y_root);

                      ClientSnap(c, &r);
                      break;
                    case SUB_DRAG_RESIZE_LEFT: 
                    case SUB_DRAG_RESIZE_RIGHT:
                      /* Stepping and snapping */
                      if(c->rect.width + ev.xmotion.x_root >= rx)
                        {
                          if(SUB_DRAG_RESIZE_LEFT == mode)
                            {
                              int rxmot = rx - ev.xmotion.x_root, rxstep = 0;

                              r.x     = MINMAX((rx - wx) - rxmot, 0, 
                                c->rect.x + c->rect.width - minx);
                              rxstep  = r.x % subtle->step;

                              r.width = MINMAX(c->rect.width + rxmot + rxstep, 
                                minx + rxstep, maxx);
                              r.x    -= rxstep;
                            }
                          else
                            {
                              r.width = MINMAX(c->rect.width + (ev.xmotion.x_root - rx),
                                minx, maxx);
                              r.width -= r.width % subtle->step;
                            }
                        }
                      if(c->rect.height + ev.xmotion.y_root >= ry)
                        {
                          r.height = MINMAX(c->rect.height + (ev.xmotion.y_root - ry),
                            miny, maxy);
                          r.height -= r.height % subtle->step;
                        }
                      break;
                  }  

                ClientMask(SUB_DRAG_START, c, &r);
              } /* }}} */
            else if(SUB_DRAG_SWAP == mode && win != c->win) /* Tile {{{ */
              {
                if(!c2 ) c2 = CLIENT(subSharedFind(win, CLIENTID));
                if(c2 && !(c2->flags & SUB_STATE_FLOAT))
                  {
                    XQueryPointer(subtle->disp, win, &unused, &unused,
                      &rx, &ry, &wx, &wy, &mask);
                    r.x = rx - wx;
                    r.y = ry - wy;

                    /* Change drag state */
                    if(wx > c2->rect.width * 0.35 && wx < c2->rect.width * 0.65)
                      {
                        if(SUB_DRAG_SWAP != state && wy > c2->rect.height * 0.35 &&
                          wy < c2->rect.height * 0.65)
                          state = SUB_DRAG_SWAP;
                      }
                      
                    if(SUB_DRAG_TOP != state && wy < c2->rect.height * 0.35)
                      state = SUB_DRAG_TOP;
                    else if(SUB_DRAG_BOTTOM != state && wy > c2->rect.height * 0.65)
                      state = SUB_DRAG_BOTTOM;

                    if(SUB_DRAG_LEFT != state && wx < c2->rect.width * 0.35)
                      state = SUB_DRAG_LEFT;
                    else if(SUB_DRAG_RIGHT != state && wx > c2->rect.width * 0.65)
                      state = SUB_DRAG_RIGHT;

                    if(lstate != state || lc != c2) 
                      {
                        if(SUB_DRAG_START != lstate) ClientMask(lstate, lc, &r);
                        ClientMask(state, c2, &r);

                        lc     = c2;
                        lstate = state;
                      }
                    }
                } /* }}} */
            break; /* }}} */
        }
    }

  ClientMask(state, c2, &r); ///< Erase mask

  if(c && c2 && win != c->win) ///< Arrange {{{
    {
      switch(state) ///< State translation
        {
          case SUB_DRAG_TOP:    state = SUB_TILE_TOP;    break;
          case SUB_DRAG_BOTTOM: state = SUB_TILE_BOTTOM; break;
          case SUB_DRAG_LEFT:   state = SUB_TILE_LEFT;   break;
          case SUB_DRAG_RIGHT:  state = SUB_TILE_RIGHT;  break;
          case SUB_DRAG_SWAP:   state = SUB_TILE_SWAP;   break;
        }
          
      subViewArrange(subtle->cv, c, c2, state);
      subViewConfigure(subtle->cv);
    } /* }}} */
  else ///< Move/Resize
    {
      if(c->flags & SUB_STATE_FLOAT) 
        {
          r.y     -= (subtle->th + subtle->bw); ///< Border and bar height
          r.x     -= subtle->bw;
          c->rect  = r;

          subClientConfigure(c);
        }
      else if(mode & (SUB_DRAG_RESIZE_LEFT|SUB_DRAG_RESIZE_RIGHT))
        {
          /* Get size ratios */
          c->size = r.width * 100 / SCREENW;
          c->size = MINMAX(c->size, 10, 90); ///< Min 10%, Max 90%

          if(!(c->flags & SUB_STATE_RESIZE)) c->flags |= SUB_STATE_RESIZE;
          subViewConfigure(subtle->cv);        
        }
    }
  
  XUngrabPointer(subtle->disp, CurrentTime);
  XUngrabServer(subtle->disp);
} /* }}} */

 /** subClientToggle {{{
  * @brief Toggle various states of client
  * @param[in]  c     A #SubClient
  * @param[in]  type  Toggle type
  **/

void
subClientToggle(SubClient *c,
  int type)
{
  assert(c);

  if(c->flags & type) 
    {
      c->flags &= ~type;

      if(type & SUB_STATE_FULL) /* {{{ */
        {
          XSetWindowBorderWidth(subtle->disp, c->win, subtle->bw);
          XReparentWindow(subtle->disp, c->win, subtle->cv->frame, 0, 0);
        } /* }}} */
    }
  else 
    {
      c->flags |= type;

      if(type & SUB_STATE_FLOAT) /* {{{ */
        {
          if(c->flags & SUB_PREF_HINTS)
            {
              if(c->hints->flags & (USSize|PSize)) ///< User/program size
                {
                  c->rect.width  = c->hints->width;
                  c->rect.height = c->hints->height;
                }
              else if(c->hints->flags & PBaseSize) ///< Base size
                {
                  c->rect.width  = c->hints->base_width;
                  c->rect.height = c->hints->base_height;
                }
              else if(c->hints->flags & PMinSize) ///< Min size
                {
                  c->rect.width  = c->hints->min_width;
                  c->rect.height = c->hints->min_height;
                }
              else ///< Fallback
                {
                  c->rect.width  = MINW;
                  c->rect.height = MINH;
                }

              /* Limit width/height to max. screen size*/
              c->rect.width  = MINMAX(c->rect.width, MINW, SCREENW) + 2 * subtle->bw;
              c->rect.height = MINMAX(c->rect.height, MINH, SCREENH - 
                (subtle->th + 2 * subtle->bw)) + 2 * subtle->bw;

              if(c->hints && c->hints->flags & (USPosition|PPosition)) ///< User/program pos
                {
                  c->rect.x = c->hints->x;
                  c->rect.y = c->hints->y;
                }
              else if(c->hints->flags & PAspect) ///< Aspect size
                {
                  c->rect.x = (c->hints->min_aspect.x - c->hints->max_aspect.x) / 2;
                  c->rect.y = (c->hints->min_aspect.y - c->hints->max_aspect.y) / 2;
                }
              else ///< Fallback
                {
                  c->rect.x = (SCREENW - c->rect.width) / 2;
                  c->rect.y = (SCREENH - c->rect.height) / 2;
                }
            }
          else ///< Fallback
            {
              c->rect.width  = MINW + 2 * subtle->bw;
              c->rect.height = MINH + 2 * subtle->bw;
              c->rect.x      = (SCREENW - c->rect.width) / 2;
              c->rect.y      = (SCREENH - c->rect.height) / 2;
            }        
        } /* }}} */
      if(type & SUB_STATE_FULL) /* {{{ */
        {
          XSetWindowBorderWidth(subtle->disp, c->win, 0);
          XReparentWindow(subtle->disp, c->win, ROOT, 0, 0);
        } /* }}} */

      subClientConfigure(c);
    }

  subClientFocus(c);
} /* }}} */

 /** subClientPublish {{{
  * @brief Publish clients
  **/

void
subClientPublish(void)
{
  if(0 < subtle->clients->ndata )
    {
      int i;
      Window *wins = (Window *)subSharedMemoryAlloc(subtle->clients->ndata, sizeof(Window));

      for(i = 0; i < subtle->clients->ndata; i++) 
        wins[i] = CLIENT(subtle->clients->data[i])->win;

      /* EWMH: Client list and client list stacking */
      subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST, wins, subtle->clients->ndata);
      subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST_STACKING, wins, 
        subtle->clients->ndata);

      /* Perrow / percol */
      subtle->perrow = (int *)subSharedMemoryRealloc(subtle->perrow, 
        subtle->clients->ndata * sizeof(int));
      subtle->percol = (int *)subSharedMemoryRealloc(subtle->percol, 
        subtle->clients->ndata * sizeof(int));

      subSharedLogDebug("publish=client, clients=%d\n", subtle->clients->ndata);

      free(wins);
    }
} /* }}} */

 /** subClientKill {{{
  * @brief Send interested clients the close signal and/or kill it
  * @param[in]  c      A #SubClient
  * @param[in]  close  Close window
  **/

void
subClientKill(SubClient *c,
  int close)
{
  assert(c);

  printf("Killing client (%s)\n", c->name);

  /* Ignore further events and delete context */
  XSelectInput(subtle->disp, c->win, NoEventMask);
  XDeleteContext(subtle->disp, c->win, CLIENTID);
  if(subtle->windows.focus == c->win) subtle->windows.focus = 0; ///< Unset focus

  /* Close window */
  if(close && !(c->flags & SUB_STATE_DEAD))
    {
      if(c->flags & SUB_PREF_CLOSE) ///< Honor window preferences
        {
          subEwmhMessage(c->win, c->win, SUB_EWMH_WM_PROTOCOLS, 
            subEwmhGet(SUB_EWMH_WM_DELETE_WINDOW), CurrentTime, 0, 0, 0);
        }
      else XKillClient(subtle->disp, c->win);
    }

  if(c->hints) XFree(c->hints);
  if(c->name) XFree(c->name);
  free(c);

  subSharedLogDebug("kill=client\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
