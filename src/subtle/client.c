
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

#define MINW 100
#define MINH 100
#define STEP 5
#define SNAP 10

/* ClientMask {{{ */
static void
ClientMask(int type,
  SubClient *c,
  XRectangle *r)
{
  /* @todo Put modifiers into a static table? */
  switch(type)
    {
      case SUB_DRAG_START: 
        XDrawRectangle(subtle->disp, DefaultRootWindow(subtle->disp), subtle->gcs.invert, 
          r->x - 1, r->y - 1, r->width - 3, r->height - 3);
        break;
      case SUB_DRAG_TOP:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, 5, 5, c->rect.width - 10, 
          c->rect.height * 0.5 - 5);
        break;
      case SUB_DRAG_BOTTOM:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, 5, c->rect.height * 0.5, 
          c->rect.width - 10, c->rect.height * 0.5 - 5);
        break;
      case SUB_DRAG_LEFT:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, 5, 5, c->rect.width * 0.5 - 5, 
          c->rect.height - 10);
        break;
      case SUB_DRAG_RIGHT:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, c->rect.width * 0.5, 5, 
          c->rect.width * 0.5 - 5, c->rect.height - 10);
        break;
      case SUB_DRAG_SWAP:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, c->rect.width * 0.35, 
          c->rect.height * 0.35, c->rect.width * 0.3, c->rect.height * 0.3);
        break;
      case SUB_DRAG_BEFORE:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, 5, 5, c->rect.width * 0.1 - 5, 
          c->rect.height - 10);
        break;
      case SUB_DRAG_AFTER:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, c->rect.width * 0.9, 5, 
          c->rect.width * 0.1 - 5, c->rect.height - 10);
        break;
      case SUB_DRAG_ABOVE:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, 5, 5, c->rect.width - 10, 
          c->rect.height * 0.1 - 5);
        break;
      case SUB_DRAG_BELOW:
        XFillRectangle(subtle->disp, c->win, subtle->gcs.invert, 5, c->rect.height * 0.9, 
          c->rect.width - 10, c->rect.height * 0.1 - 5);
        break;
    }
} /* }}} */

/* ClientSnap {{{ */
static void
ClientSnap(SubClient *c,
  XRectangle *r)
{
  assert(c && r);

  if(SNAP > r->x) r->x = subtle->bw;
  else if(r->x > (SCREENW - WINW(c) - SNAP)) 
    r->x = SCREENW - c->rect.width + subtle->bw;
  if(SNAP > r->y) r->y = subtle->bw;
  else if(r->y > (SCREENH - WINH(c) - SNAP)) 
    r->y = SCREENH - c->rect.height + subtle->bw;
} /* }}} */

 /** subClientNew {{{
  * @brief Create new client
  * @param[in]  win  Main window of the new client
  * @return Returns a #SubClient or \p NULL
  **/

SubClient *
subClientNew(Window win)
{
  int i, n = 0;
  long vid = 1337;
  char *wmclass = NULL;
  Window propwin = 0;
  XWMHints *hints = NULL;
  XWindowAttributes attrs;
  Atom *protos = NULL;
  SubClient *c = NULL;

  assert(win);
  
  c = CLIENT(subUtilAlloc(1, sizeof(SubClient)));
  c->flags = SUB_TYPE_CLIENT;
  c->win   = win;

  /* Dimensions */
  c->rect.x      = 0;
  c->rect.y      = subtle->th;
  c->rect.width  = SCREENW;
  c->rect.height = SCREENH - subtle->th;

  /* Update client */
  XAddToSaveSet(subtle->disp, c->win);
  XSelectInput(subtle->disp, c->win, SubstructureRedirectMask|SubstructureNotifyMask|
    ExposureMask|VisibilityChangeMask|EnterWindowMask|FocusChangeMask|
    KeyPressMask|ButtonPressMask|PropertyChangeMask);
  XSetWindowBorderWidth(subtle->disp, c->win, subtle->bw);
  XSaveContext(subtle->disp, c->win, CLIENTID, (void *)c);

  /* Window attributes */
  XGetWindowAttributes(subtle->disp, c->win, &attrs);
  c->cmap = attrs.colormap;
  subClientFetchName(c);

  /* Window manager hints */
  hints = XGetWMHints(subtle->disp, win);
  if(hints)
    {
      subClientSetWMState(c, NormalState);
      if(hints->input) c->flags |= SUB_PREF_INPUT;
      if(hints->flags & XUrgencyHint) c->flags |= (SUB_STATE_URGENT|SUB_STATE_FLOAT);
      XFree(hints);
    }
  
  /* Protocols */
  if(XGetWMProtocols(subtle->disp, c->win, &protos, &n))
    {
      for(i = 0; i < n; i++)
        {
          if(protos[i] == subEwmhFind(SUB_EWMH_WM_TAKE_FOCUS))   
            c->flags |= SUB_PREF_FOCUS;
          else if(protos[i] == subEwmhFind(SUB_EWMH_WM_DELETE_WINDOW)) 
            c->flags |= SUB_PREF_CLOSE;
        }
      XFree(protos);
    }

  /* Tags */
  wmclass = subEwmhGetProperty(win, XA_STRING, SUB_EWMH_WM_CLASS, NULL);
  if(wmclass)
    {
      for(i = 0; i < subtle->tags->ndata; i++)
        {
          SubTag *t = TAG(subtle->tags->data[i]);
          if(t->preg && subUtilRegexMatch(t->preg, wmclass)) c->tags |= (1L << (i + 1));
        }
      XFree(wmclass);  
    }

  if(!c->tags) c->tags |= SUB_TAG_DEFAULT; ///< Ensure that there's at least on tag
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);

  /* Special tags */
  XGetTransientForHint(subtle->disp, win, &propwin); ///< Check for dialogs
  if(propwin) c->tags |= SUB_TAG_FLOAT;

  /* EWMH: Desktop */
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

  printf("Adding client (%s)\n", c->name);
  subUtilLogDebug("new=client, name=%s, win=%#lx\n", c->name, win);

  return c;
} /* }}} */

 /** subClientConfigure {{{
  * @brief Send a configure request to client
  * @param[in]  c  A #SubClient
  **/

void
subClientConfigure(SubClient *c)
{
  XConfigureEvent ev;

  assert(c);

  /* Resize client window */
  if(c->flags & SUB_STATE_FULL) 
    {
      XMoveResizeWindow(subtle->disp, c->win, 0, 0, c->rect.width, c->rect.height);
    }
  else XMoveResizeWindow(subtle->disp, c->win, c->rect.x, c->rect.y, WINW(c), WINH(c));

  /* Tell client new geometry */
  ev.type              = ConfigureNotify;
  ev.event             = c->win;
  ev.window            = c->win;
  ev.x                 = c->rect.x;
  ev.y                 = c->rect.y;
  ev.width             = WINW(c);
  ev.height            = WINH(c);
  ev.above             = None;
  ev.border_width      = 0;
  ev.override_redirect = 0;

  subUtilLogDebug("client=%#lx, x=%03d, y=%03d, width=%03d, height=%03d\n", 
    c->win, c->rect.x, c->rect.y, WINW(c), WINH(c));

  XSendEvent(subtle->disp, c->win, False, StructureNotifyMask, (XEvent *)&ev);
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

  attrs.border_pixel = subtle->focus == c->win ? subtle->colors.focus : subtle->colors.norm;
  XChangeWindowAttributes(subtle->disp, c->win, CWBorderPixel, &attrs);

  /* Caption */
  XResizeWindow(subtle->disp, subtle->bar.caption, TEXTW(c->name), subtle->th);  
  XClearWindow(subtle->disp, subtle->bar.caption);
  XDrawString(subtle->disp, subtle->bar.caption, subtle->gcs.font, 3, subtle->fy - 1, 
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
  
  /* Check if client wants to take focus by itself */
  if(c->flags & SUB_PREF_FOCUS)
    {
      XEvent ev;
  
      ev.type                 = ClientMessage;
      ev.xclient.window       = c->win;
      ev.xclient.message_type = subEwmhFind(SUB_EWMH_WM_PROTOCOLS);
      ev.xclient.format       = 32;
      ev.xclient.data.l[0]    = subEwmhFind(SUB_EWMH_WM_TAKE_FOCUS);
      ev.xclient.data.l[1]    = CurrentTime;
      
      XSendEvent(subtle->disp, c->win, False, NoEventMask, &ev);
      
      subUtilLogDebug("Focus: win=%#lx, input=%d, send=%d\n", c->win,      
        !!(c->flags & SUB_PREF_INPUT), !!(c->flags & SUB_PREF_FOCUS));     
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
  unsigned int mask;
  int loop = True, wx = 0, wy = 0, rx = 0, ry = 0, 
    state = SUB_DRAG_START, lstate = SUB_DRAG_START;
  XRectangle r;
  SubClient *c2 = NULL, *lc = NULL;

  assert(c);

  /* Get window position on root window */
  XQueryPointer(subtle->disp, c->win, &win, &win, &rx, &ry, &wx, &wy, &mask);
  r.x      = rx - wx;
  r.y      = ry - wy;
  r.width  = c->rect.width;
  r.height = c->rect.height;

  if(XGrabPointer(subtle->disp, c->win, True, 
    ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, 
    SUB_DRAG_MOVE == mode ? subtle->cursors.move : subtle->cursors.resize, 
    CurrentTime)) return;

  XGrabServer(subtle->disp);
  if(SUB_DRAG_MOVE == mode)
    ClientMask(SUB_DRAG_START, c, &r);
  if(SUB_DRAG_RESIZE == mode) 
    {
      ClientMask(SUB_DRAG_START, c, &r);
      printf("lala\n");
      XWarpPointer(subtle->disp, 0, c->win, 0, 0, 0, 0, c->rect.width, c->rect.height);
    }

  while(loop) ///< Event loop
    {
      XMaskEvent(subtle->disp, 
        PointerMotionMask|ButtonReleaseMask|KeyPressMask|EnterWindowMask, &ev);
      switch(ev.type)
        {
          case EnterNotify:   win = ev.xcrossing.window; break; ///< Find destination window
          case ButtonRelease: loop = False;              break;
          case KeyPress: /* {{{ */
            if(SUB_DRAG_MOVE >= mode)
              {
                KeySym sym = XKeycodeToKeysym(subtle->disp, ev.xkey.keycode, 0);
                ClientMask(SUB_DRAG_START, c, &r);

                switch(sym)
                  {
                    case XK_Left:   r.x -= STEP;  break; 
                    case XK_Right:  r.x += STEP;  break;
                    case XK_Up:     r.y -= STEP;  break;
                    case XK_Down:   r.y += STEP;  break;
                    case XK_Return: loop = False; break;
                  }
                ClientMask(SUB_DRAG_START, c, &r);
              }
            break; /* }}} */
          case MotionNotify: /* {{{ */
            if(SUB_DRAG_MOVE == mode) 
              {
                ClientMask(SUB_DRAG_START, c, &r);
          
                /* Calculate dimensions of the selection rect */
                switch(mode)
                  {
                    case SUB_DRAG_MOVE:
                      r.x = (rx - wx) - (rx - ev.xmotion.x_root);
                      r.y = (ry - wy) - (ry - ev.xmotion.y_root);

                      ClientSnap(c, &r);
                      break;
                    case SUB_DRAG_RESIZE: 
                      r.width  = c->rect.width + (ev.xmotion.x_root - rx);
                      r.height = c->rect.height + (ev.xmotion.y_root - ry);
                      break;
                  }  
                ClientMask(SUB_DRAG_START, c, &r);
              }
            else if(win != c->win && SUB_DRAG_SWAP == mode)
              {
                if(!c2 ) c2 = CLIENT(subUtilFind(win, CLIENTID));
                if(c2)
                  {
                    XQueryPointer(subtle->disp, win, &unused, &unused, 
                      &rx, &ry, &wx, &wy, &mask);
                    r.x = rx - wx;
                    r.y = ry - wy;

                    /* Change drag state */
                    if(wx > c2->rect.width * 0.35 && wx < c2->rect.width * 0.65)
                      {
                        if(state != SUB_DRAG_TOP && wy > c2->rect.height * 0.1 && 
                          wy < c2->rect.height * 0.35)
                          state = SUB_DRAG_TOP;
                        else if(state != SUB_DRAG_BOTTOM && wy > c2->rect.height * 0.65 && 
                          wy < c2->rect.height * 0.9)
                          state = SUB_DRAG_BOTTOM;
                        else if(state != SUB_DRAG_SWAP && wy > c2->rect.height * 0.35 && 
                          wy < c2->rect.height * 0.65)
                          state = SUB_DRAG_SWAP;
                      }
                    if(state != SUB_DRAG_ABOVE && wy < c2->rect.height * 0.1) 
                      state = SUB_DRAG_ABOVE;
                    else if(state != SUB_DRAG_BELOW && wy > c2->rect.height * 0.9) 
                      state = SUB_DRAG_BELOW;
                    if(wy > c2->rect.height * 0.1 && wy < c2->rect.height * 0.9)
                      {
                        if(state != SUB_DRAG_LEFT && wx > c2->rect.width * 0.1 && 
                          wx < c2->rect.width * 0.35)
                          state = SUB_DRAG_LEFT;
                        else if(state != SUB_DRAG_RIGHT && wx > c2->rect.width * 0.65 && 
                          wx < c2->rect.width * 0.9)
                          state = SUB_DRAG_RIGHT;
                        else if(state != SUB_DRAG_BEFORE && wx < c2->rect.width * 0.1)
                          state = SUB_DRAG_BEFORE;
                        else if(state != SUB_DRAG_AFTER && wx > c2->rect.width * 0.9)
                          state = SUB_DRAG_AFTER;
                      }

                    if(lstate != state || lc != c2) 
                      {
                        if(SUB_DRAG_START != lstate) ClientMask(lstate, lc, &r);
                        ClientMask(state, c2, &r);

                        lc     = c2;
                        lstate = state;
                      }
                    }
                }
            break; /* }}} */
        }
    }

  ClientMask(state, c2, &r); ///< Erase mask

  if(win != c->win && c && c2) ///< Except same window
    {
      if(state & (SUB_DRAG_LEFT|SUB_DRAG_RIGHT)) 
        {
          subViewArrange(subtle->cv, c, c2, SUB_TILE_HORZ);
          subViewConfigure(subtle->cv);
        }
      else if(state & (SUB_DRAG_TOP|SUB_DRAG_BOTTOM)) 
        {
          subViewArrange(subtle->cv, c, c2, SUB_TILE_VERT);
          subViewConfigure(subtle->cv);
        }
      else if(SUB_DRAG_SWAP == state) 
        {
          subViewArrange(subtle->cv, c, c2, SUB_TILE_SWAP);
          subViewConfigure(subtle->cv);
        }
    }
  else ///< Move/Resize
    {
      if(c->flags & SUB_STATE_FLOAT || c->tags & SUB_TAG_FLOAT) 
        {
          r.y -= (subtle->th + subtle->bw);
          r.x -= subtle->bw;
          c->rect = r;
          subClientConfigure(c);
        }
      else if(SUB_DRAG_BOTTOM >= mode) 
        {
          /* Get size ratios */
          if(SUB_DRAG_RIGHT == mode || SUB_DRAG_LEFT == mode) 
            c->size = r.width * 100 / WINW(c);
          else c->size = r.height * 100 / WINH(c);

          if(90 > c->size) c->size = 90;      ///< Max. 90%
          else if(10 > c->size) c->size = 10; ///< Min. 10%

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

  if(c->flags & type) c->flags &= ~type;
  else 
    {
      long supplied = 0;
      XSizeHints *hints = NULL;
      int width = SCREENW, height = SCREENH;

      c->flags |= type;

      switch(type)
        {
          case SUB_STATE_FLOAT:
            hints = XAllocSizeHints();
            if(!hints) subUtilLogError("Can't alloc memory. Exhausted?\n");

            /* Respect the user/program preferences */
            if(XGetWMNormalHints(subtle->disp, c->win, hints, &supplied))
              {
                if(hints->flags & (USSize|PSize))
                  {
                    c->rect.width  = hints->width + 2 * subtle->bw;
                    c->rect.height = hints->height + 2 * subtle->bw;
                  }
                else if(hints->flags & PBaseSize)
                  {
                    c->rect.width  = hints->base_width + 2 * subtle->bw;
                    c->rect.height = hints->base_height + 2 * subtle->bw;
                  }

                /* Sanitize */
                if(MINW > c->rect.width)  c->rect.width  = MINW + 2 * subtle->bw;
                if(MINH > c->rect.height) c->rect.height = MINH + 2 * subtle->bw;

                if(hints->flags & (USPosition|PPosition))
                  {
                    c->rect.x = hints->x + 2 * subtle->bw;
                    c->rect.y = hints->y + 2 * subtle->bw;
                  }
                else if(hints->flags & PAspect)
                  {
                    c->rect.x = (hints->min_aspect.x - hints->max_aspect.x) / 2;
                    c->rect.y = (hints->min_aspect.y - hints->max_aspect.y) / 2;
                  }
                else
                  {
                    c->rect.x = (width - c->rect.width) / 2;
                    c->rect.y = (height - c->rect.height) / 2;
                  }
              }

            subClientConfigure(c);
            XRaiseWindow(subtle->disp, c->win);
            break;
          case SUB_STATE_FULL:
            /* Resize to display resolution */
            c->rect.x      = 0;
            c->rect.y      = 0;
            c->rect.width  = width;
            c->rect.height = height;

            subClientConfigure(c);
        }
    }
} /* }}} */

  /** subClientFetchName {{{
   * @brief Fetch client name and store it
   * @param[in]  c  A #SubClient
   **/

void
subClientFetchName(SubClient *c)
{
  assert(c);

  if(c->name) 
    {
      XFree(c->name);
      c->name = NULL;
    }

  XFetchName(subtle->disp, c->win, &c->name);
  if(!c->name) c->name = strdup("subtle");

  subClientRender(c);
} /* }}} */

 /** subClientSetWMState {{{
  * @brief Set WM state for client
  * @param[in]  c      A #SubClient
  * @param[in]  state  New state for the client
  **/

void
subClientSetWMState(SubClient *c,
  long state)
{
  CARD32 data[2];
  data[0] = state;
  data[1] = None; /* No icons */

  assert(c);

  XChangeProperty(subtle->disp, c->win, subEwmhFind(SUB_EWMH_WM_STATE), subEwmhFind(SUB_EWMH_WM_STATE),
    32, PropModeReplace, (unsigned char *)data, 2);
} /* }}} */

 /** subClientGetWMState {{{
  * @brief Get WM state from client
  * @param[in]  c  A #SubClient
  * @return Returns client WM state
  **/

long
subClientGetWMState(SubClient *c)
{
  Atom type;
  int format;
  unsigned long unused, bytes;
  long *data = NULL, state = WithdrawnState;

  assert(c);

  if(XGetWindowProperty(subtle->disp, c->win, subEwmhFind(SUB_EWMH_WM_STATE), 0L, 2L, False, 
      subEwmhFind(SUB_EWMH_WM_STATE), &type, &format, &bytes, &unused, (unsigned char **)&data) == Success && bytes)
    {
      state = *data;
      XFree(data);
    }
  return state;
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
      Window *wins = (Window *)subUtilAlloc(subtle->clients->ndata, sizeof(Window));

      for(i = 0; i < subtle->clients->ndata; i++) 
        wins[i] = CLIENT(subtle->clients->data[i])->win;

      /* EWMH: Client list and client list stacking */
      subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_CLIENT_LIST, 
        wins, subtle->clients->ndata);
      subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_CLIENT_LIST_STACKING, 
        wins, subtle->clients->ndata);

      subUtilLogDebug("publish=client, clients=%d\n", subtle->clients->ndata);

      free(wins);
    }
} /* }}} */

 /** subClientKill {{{
  * @brief Send interested clients the close signal and/or kill it
  * @param[in]  c  A #SubClient
  **/

void
subClientKill(SubClient *c)
{
  assert(c);

  printf("Killing client %s\n", c->name);

  /* Ignore further events and delete context */
  XSelectInput(subtle->disp, c->win, NoEventMask);
  XDeleteContext(subtle->disp, c->win, 1);

  if(subtle->focus == c->win) subtle->focus = 0; ///< Unset focus
  if(!(c->flags & SUB_STATE_DEAD))
    {
      subArrayPop(subtle->clients, (void *)c->win);

      /* EWMH: Update Client list and client list stacking */      
      subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_CLIENT_LIST,
        (Window *)subtle->clients->data, subtle->clients->ndata);
      subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_CLIENT_LIST_STACKING,
        (Window *)subtle->clients->data, subtle->clients->ndata);
    
      /* Honor window preferences */
      if(c->flags & SUB_PREF_CLOSE)
        {
          XEvent ev;

          ev.type                 = ClientMessage;
          ev.xclient.window       = c->win;
          ev.xclient.message_type = subEwmhFind(SUB_EWMH_WM_PROTOCOLS);
          ev.xclient.format       = 32;
          ev.xclient.data.l[0]    = subEwmhFind(SUB_EWMH_WM_DELETE_WINDOW);
          ev.xclient.data.l[1]    = CurrentTime;

          XSendEvent(subtle->disp, c->win, False, NoEventMask, &ev);
        }
      else XKillClient(subtle->disp, c->win);
    }

  subViewSanitize(c);
  subArrayPop(subtle->clients, (void *)c);

  if(c->name) XFree(c->name);
  free(c);

  subUtilLogDebug("kill=client\n");
} /* }}} */
