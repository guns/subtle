
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

#include "subtle.h"

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
        XDrawRectangle(subtle->disp, DefaultRootWindow(subtle->disp), subtle->gcs.invert, r->x + 1, r->y + 1, 
          r->width - 3, (c->flags & SUB_STATE_SHADE) ? subtle->th - subtle->bw : r->height - subtle->bw);
        break;
      case SUB_DRAG_TOP:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, 5, 5, c->rect.width - 10, 
          c->rect.height * 0.5 - 5);
        break;
      case SUB_DRAG_BOTTOM:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, 5, c->rect.height * 0.5, 
          c->rect.width - 10, c->rect.height * 0.5 - 5);
        break;
      case SUB_DRAG_LEFT:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, 5, 5, c->rect.width * 0.5 - 5, 
          c->rect.height - 10);
        break;
      case SUB_DRAG_RIGHT:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, c->rect.width * 0.5, 5, 
          c->rect.width * 0.5 - 5, c->rect.height - 10);
        break;
      case SUB_DRAG_SWAP:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, c->rect.width * 0.35, 
          c->rect.height * 0.35, c->rect.width * 0.3, c->rect.height * 0.3);
        break;
      case SUB_DRAG_BEFORE:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, 5, 5, c->rect.width * 0.1 - 5, 
          c->rect.height - 10);
        break;
      case SUB_DRAG_AFTER:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, c->rect.width * 0.9, 5, 
          c->rect.width * 0.1 - 5, c->rect.height - 10);
        break;
      case SUB_DRAG_ABOVE:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, 5, 5, c->rect.width - 10, 
          c->rect.height * 0.1 - 5);
        break;
      case SUB_DRAG_BELOW:
        XFillRectangle(subtle->disp, c->frame, subtle->gcs.invert, 5, c->rect.height * 0.9, 
          c->rect.width - 10, c->rect.height * 0.1 - 5);
        break;
    }
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
  Window unnused;
  char *wmclass = NULL;
  long vid = 1337;
  XWMHints *hints = NULL;
  XWindowAttributes attr;
  XSetWindowAttributes attrs;
  Atom *protos = NULL;
  SubClient *c = NULL;

  assert(win);
  
  c  = CLIENT(subUtilAlloc(1, sizeof(SubClient)));
  c->flags  = SUB_TYPE_CLIENT;
  c->win    = win;

  /* Dimensions */
  c->rect.x       = 0;
  c->rect.y       = subtle->th;
  c->rect.width   = DisplayWidth(subtle->disp, DefaultScreen(subtle->disp));
  c->rect.height  = DisplayHeight(subtle->disp, DefaultScreen(subtle->disp)) - subtle->th;

  /* Create windows */
  attrs.border_pixel      = subtle->colors.border;
  attrs.background_pixel  = subtle->colors.norm;
  attrs.background_pixmap = ParentRelative;
  attrs.override_redirect = True;
  attrs.event_mask        = SubstructureRedirectMask|SubstructureNotifyMask|ExposureMask|VisibilityChangeMask|
    EnterWindowMask|FocusChangeMask|KeyPressMask|ButtonPressMask;

  c->frame      = WINNEW(DefaultRootWindow(subtle->disp), 0, subtle->th, c->rect.width, c->rect.height, 0, 
    CWOverrideRedirect|CWBackPixmap|CWEventMask);
  c->title      = WINNEW(c->frame, 0, 0, c->rect.width, subtle->th, 0, CWBackPixel);
  c->caption    = WINNEW(c->frame, 0, 1, 1, subtle->th - 2, 0, CWBackPixel);
  attrs.cursor  = subtle->cursors.horz;
  c->left       = WINNEW(c->frame, 0, subtle->th, subtle->bw, c->rect.height - subtle->th, 0, 
    CWBackPixel|CWCursor);
  c->right      = WINNEW(c->frame, c->rect.width - subtle->bw, subtle->th, subtle->bw, 
    c->rect.height - subtle->th, 0, CWBackPixel|CWCursor);
  attrs.cursor  = subtle->cursors.vert;
  c->bottom     = WINNEW(c->frame, 0, c->rect.height - subtle->bw, c->rect.width, subtle->bw, 0, 
    CWBackPixel|CWCursor);

  /* Update client */
  XSelectInput(subtle->disp, c->win, PropertyChangeMask|StructureNotifyMask);
  XSetWindowBorderWidth(subtle->disp, c->win, 0);
  XReparentWindow(subtle->disp, c->win, c->frame, subtle->bw, subtle->th);
  XAddToSaveSet(subtle->disp, c->win);
  XSaveContext(subtle->disp, c->frame, 1, (void *)c);

  /* Window attributes */
  XGetWindowAttributes(subtle->disp, c->win, &attr);
  c->cmap  = attr.colormap;
  subClientFetchName(c);

  /* Window manager hints */
  hints = XGetWMHints(subtle->disp, win);
  if(hints)
    {
      if(hints->flags & StateHint) subClientSetWMState(c, hints->initial_state);
      else subClientSetWMState(c, NormalState);
      if(hints->input) c->flags |= SUB_PREF_INPUT;
      if(hints->flags & XUrgencyHint) c->flags |= SUB_STATE_TRANS;
      XFree(hints);
    }
  
  /* Protocols */
  if(XGetWMProtocols(subtle->disp, c->win, &protos, &n))
    {
      for(i = 0; i < n; i++)
        {
          if(protos[i] == subEwmhFind(SUB_EWMH_WM_TAKE_FOCUS))          c->flags |= SUB_PREF_FOCUS;
          else if(protos[i] == subEwmhFind(SUB_EWMH_WM_DELETE_WINDOW))  c->flags |= SUB_PREF_CLOSE;
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

  /* Ensure that there is at least one tag */
  if(!c->tags) 
    {
      int id;
      subTagFind("default", &id); ///< Just id of the tag
      c->tags |= (1L << (id + 1));
    }  
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_CLIENT_TAGS, (long *)&c->tags, 1);

  /* EWMH: Desktop */
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

  /* Check for dialog windows etc. */
  XGetTransientForHint(subtle->disp, win, &unnused);
  if(unnused && !(c->flags & SUB_STATE_TRANS)) c->flags |= SUB_STATE_TRANS;

  printf("Adding client (%s)\n", c->name);
  subUtilLogDebug("new=client, name=%s, win=%#lx\n", c->name, win);

  return(c);
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

  /* Resize client windows */
  XMoveResizeWindow(subtle->disp, c->frame, c->rect.x, c->rect.y, c->rect.width, (c->flags & SUB_STATE_SHADE) ? subtle->th : c->rect.height);
  if(c->flags & SUB_STATE_FULL) XMoveResizeWindow(subtle->disp, c->win, 0, 0, c->rect.width, c->rect.height);
  else if(!(c->flags & SUB_STATE_SHADE))
    {
      XMoveResizeWindow(subtle->disp, c->win, subtle->bw, subtle->th, c->rect.width - 2 * subtle->bw, c->rect.height - subtle->th - subtle->bw);
      XMoveResizeWindow(subtle->disp, c->right, c->rect.width - subtle->bw, subtle->th, subtle->bw, c->rect.height - subtle->th);
      XMoveResizeWindow(subtle->disp, c->bottom, 0, c->rect.height - subtle->bw, c->rect.width, subtle->bw);
    }
  else XMoveResizeWindow(subtle->disp, c->title, 0, 0, c->rect.width, subtle->th);

  /* Tell client new geometry */
  if(!(c->flags & SUB_STATE_SHADE))
    {
      ev.type               = ConfigureNotify;
      ev.event              = c->win;
      ev.window             = c->win;
      ev.x                  = c->rect.x;
      ev.y                  = c->rect.y;
      ev.width              = (c->flags & SUB_STATE_FULL) ? c->rect.width  : WINWIDTH(c);
      ev.height             = (c->flags & SUB_STATE_FULL) ? c->rect.height : WINHEIGHT(c);
      ev.above              = None;
      ev.border_width       = 0;
      ev.override_redirect  = 0;

      XSendEvent(subtle->disp, c->win, False, StructureNotifyMask, (XEvent *)&ev);
    }
} /* }}} */

 /** subClientRender {{{
  * @brief Render client and redraw titlebar and borders
  * @param[in]  c  A #SubClient
  **/

void
subClientRender(SubClient *c)
{
  unsigned long col = 0;

  assert(c);

  col = subtle->focus == c->frame ? subtle->colors.focus : 
    (c->flags & SUB_STATE_SHADE ? subtle->colors.cover : subtle->colors.norm);

  /* Update color */
  XSetWindowBackground(subtle->disp, c->title,    col);
  XSetWindowBackground(subtle->disp, c->left,     col);
  XSetWindowBackground(subtle->disp, c->right,    col);
  XSetWindowBackground(subtle->disp, c->bottom,   col);
  XSetWindowBackground(subtle->disp, c->caption,  col);

  /* Clear windows */
  XClearWindow(subtle->disp, c->title);
  XClearWindow(subtle->disp, c->left);
  XClearWindow(subtle->disp, c->right);
  XClearWindow(subtle->disp, c->bottom);
  XClearWindow(subtle->disp, c->caption);

  /* Titlebar */
  XFillRectangle(subtle->disp, c->title, subtle->gcs.border, subtle->th + 1, 2, 
    c->rect.width - subtle->th - 4, subtle->th - 4);  
  XDrawString(subtle->disp, c->caption, subtle->gcs.font, 5, subtle->fy - 1, c->name, strlen(c->name));
} /* }}} */

 /** subClientFocus {{{
  * @brief Set or unset focus to client
  * @param[in]  c  A #SubClient
  **/

void
subClientFocus(SubClient *c)
{
  assert(c);

  if(c->flags & SUB_PREF_FOCUS && !(c->flags & SUB_STATE_SHADE))
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
  else
    {
      /* Remove focus from client if exists */
      if(subtle->focus && subtle->focus != c->frame)
        {
          Window win = subtle->focus;
          SubClient *f = CLIENT(subUtilFind(win, 1));
          if(f && f->flags & SUB_TYPE_CLIENT)
            {
              if(!(f->flags & SUB_STATE_DEAD)) 
                {
                  subKeyUngrab(f->frame);
                  subClientRender(f);
                }
            }
          else subKeyUngrab(win);
        } 
      XSetInputFocus(subtle->disp, c->win, RevertToNone, CurrentTime);
      
      subKeyGrab(c->frame);
      subClientRender(c);
      subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_ACTIVE_WINDOW, &c->win, 1);
    }
} /* }}} */

 /** subClientMap {{{
  * @brief Map client with all subwindows to screen
  * @param[in]  c  A #SubClient
  **/

void
subClientMap(SubClient *c)
{
  assert(c);

  XMapSubwindows(subtle->disp, c->frame);
  XMapWindow(subtle->disp, c->frame);
} /* }}} */

 /** subClientUnmap {{{
  * @brief Unmap client with all subClientUnmap from screen
  * @param[in]  c  A #SubClient
  **/

void
subClientUnmap(SubClient *c)
{
  assert(c);

  XUnmapSubwindows(subtle->disp, c->frame);
  XUnmapWindow(subtle->disp, c->frame);
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
  Cursor cursor;
  Window win, unused;
  unsigned int mask;
  int wx = 0, wy = 0, rx = 0, ry = 0, state = SUB_DRAG_START, lstate = SUB_DRAG_START;
  XRectangle r;
  SubClient *c2 = NULL, *lc = NULL;

  assert(c);
  
  /* Get window position on root window */
  XQueryPointer(subtle->disp, c->frame, &win, &win, &rx, &ry, &wx, &wy, &mask);
  r.x        = rx - wx;
  r.y        = ry - wy;
  r.width   = c->rect.width;
  r.height  = c->rect.height;

  /* Select cursor */
  switch(mode)
    {
      case SUB_DRAG_LEFT:    
      case SUB_DRAG_RIGHT:  cursor = subtle->cursors.horz;    break;
      case SUB_DRAG_BOTTOM: cursor = subtle->cursors.vert;    break;
      case SUB_DRAG_MOVE:   cursor = subtle->cursors.move;    break;
      default:              cursor = subtle->cursors.square;  break;
    }

  if(XGrabPointer(subtle->disp, c->frame, True, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, 
    GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime)) return;

  XGrabServer(subtle->disp);
  if(SUB_DRAG_MOVE >= mode) ClientMask(SUB_DRAG_START, c, &r);

  for(;;)
    {
      XMaskEvent(subtle->disp, PointerMotionMask|ButtonReleaseMask|EnterWindowMask, &ev);
      switch(ev.type)
        {
          /* Button release doesn't return our destination window */
          case EnterNotify: win = ev.xcrossing.window; break;
          case MotionNotify: /* {{{ */
            if(SUB_DRAG_MOVE >= mode) 
              {
                ClientMask(SUB_DRAG_START, c, &r);
          
                /* Calculate dimensions of the selection rect */
                switch(mode)
                  {
                    case SUB_DRAG_LEFT:   
                      r.x     = (rx - wx) - (rx - ev.xmotion.x_root);  
                      r.width = c->rect.width + ((rx - wx ) - ev.xmotion.x_root);  
                      break;
                    case SUB_DRAG_RIGHT:  r.width  = c->rect.width + (ev.xmotion.x_root - rx);  break;
                    case SUB_DRAG_BOTTOM: r.height = c->rect.height + (ev.xmotion.y_root - ry); break;
                    case SUB_DRAG_MOVE:
                      r.x = (rx - wx) - (rx - ev.xmotion.x_root);
                      r.y = (ry - wy) - (ry - ev.xmotion.y_root);
                      break;
                  }  
                ClientMask(SUB_DRAG_START, c, &r);
              }
            else if(win != c->frame && SUB_DRAG_SWAP == mode)
              {
                if(!c2 || c2->frame != win) c2 = CLIENT(subUtilFind(win, 1));
                if(c2)
                  {
                    XQueryPointer(subtle->disp, win, &unused, &unused, &rx, &ry, &wx, &wy, &mask);
                    r.x = rx - wx;
                    r.y = ry - wy;

                    /* Change drag state */
                    if(wx > c2->rect.width * 0.35 && wx < c2->rect.width * 0.65)
                      {
                        if(state != SUB_DRAG_TOP && wy > c2->rect.height * 0.1 && wy < c2->rect.height * 0.35)
                          state = SUB_DRAG_TOP;
                        else if(state != SUB_DRAG_BOTTOM && wy > c2->rect.height * 0.65 && wy < c2->rect.height * 0.9)
                          state = SUB_DRAG_BOTTOM;
                        else if(state != SUB_DRAG_SWAP && wy > c2->rect.height * 0.35 && wy < c2->rect.height * 0.65)
                          state = SUB_DRAG_SWAP;
                      }
                    if(state != SUB_DRAG_ABOVE && wy < c2->rect.height * 0.1) state = SUB_DRAG_ABOVE;
                    else if(state != SUB_DRAG_BELOW && wy > c2->rect.height * 0.9) state = SUB_DRAG_BELOW;
                    if(wy > c2->rect.height * 0.1 && wy < c2->rect.height * 0.9)
                      {
                        if(state != SUB_DRAG_LEFT && wx > c2->rect.width * 0.1 && wx < c2->rect.width * 0.35)
                          state = SUB_DRAG_LEFT;
                        else if(state != SUB_DRAG_RIGHT && wx > c2->rect.width * 0.65 && wx < c2->rect.width * 0.9)
                          state = SUB_DRAG_RIGHT;
                        else if(state != SUB_DRAG_BEFORE && wx < c2->rect.width * 0.1)
                          state = SUB_DRAG_BEFORE;
                        else if(state != SUB_DRAG_AFTER && wx > c2->rect.width * 0.9)
                          state = SUB_DRAG_AFTER;
                      }

                    if(lstate != state || lc != c2) 
                      {
                        if(lstate != SUB_DRAG_START) ClientMask(lstate, lc, &r);
                        ClientMask(state, c2, &r);

                        lc     = c2;
                        lstate = state;
                      }
                    }
                }
            break; /* }}} */
          case ButtonRelease: /* {{{ */
            if(win != c->frame && c && c2) ///< Except same window
              {
                ClientMask(state, c2, &r); ///< Erase mask
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
                XDrawRectangle(subtle->disp, DefaultRootWindow(subtle->disp), subtle->gcs.invert, 
                  r.x + 1, r.y + 1, r.width - 3, (c->flags & SUB_STATE_SHADE) ? subtle->th - 3 : r.height - 3);

                if(c->flags & SUB_STATE_FLOAT) subClientConfigure(c);
                else if(SUB_DRAG_BOTTOM >= mode) 
                  {
                    /* Get size ratios */
                    if(SUB_DRAG_RIGHT == mode || SUB_DRAG_LEFT == mode) c->size = r.width * 100 / WINWIDTH(c);
                    else c->size = r.height * 100 / WINHEIGHT(c);

                    if(90 > c->size) c->size = 90;      ///< Max. 90%
                    else if(10 > c->size) c->size = 10; ///< Min. 10%

                    if(!(c->flags & SUB_STATE_RESIZE)) c->flags |= SUB_STATE_RESIZE;
                    subViewConfigure(subtle->cv);                    
                  }
              }

            XUngrabServer(subtle->disp);
            XUngrabPointer(subtle->disp, CurrentTime);

            return; /* }}} */
        }
    }
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
  XEvent event;

  assert(c);

  if(c->flags & type)
    {
      c->flags &= ~type;
      switch(type)
        {
          case SUB_STATE_SHADE:
            subClientSetWMState(c, NormalState);

            /* Map most of the windows */
            XMapWindow(subtle->disp, c->win);
            XMapWindow(subtle->disp, c->left);
            XMapWindow(subtle->disp, c->right);
            XMapWindow(subtle->disp, c->bottom);

            /* Resize and redraw */
            XMoveResizeWindow(subtle->disp, c->frame, c->rect.x, c->rect.y, c->rect.width, c->rect.height);
            subClientRender(c);
            break;
          case SUB_STATE_FLOAT: 
            //XReparentWindow(subtle->disp, c->frame, c->tile->frame, c->rect.x, c->rect.y);  
            break;
#if 0            
          case SUB_STATE_FULL:
            /* Map most of the windows */
            XMapWindow(subtle->disp, c->title);
            XMapWindow(subtle->disp, c->left);
            XMapWindow(subtle->disp, c->right);
            XMapWindow(subtle->disp, c->bottom);
            XMapWindow(subtle->disp, c->caption);

            subClientConfigure(c);
            subArrayPush(c->tile->clients, (void *)c);
            subTileConfigure(c->tile);
            break;  
#endif            
        }
    }
  else 
    {
      //long supplied = 0;
      //XSizeHints *hints = NULL;
      c->flags |= type;

      switch(type)
        {
          case SUB_STATE_SHADE:
            subClientSetWMState(c, WithdrawnState);

            /* Unmap most of the windows */
            XUnmapWindow(subtle->disp, c->win);
            XUnmapWindow(subtle->disp, c->left);
            XUnmapWindow(subtle->disp, c->right);
            XUnmapWindow(subtle->disp, c->bottom);

            /* Resize and redraw */
            XMoveResizeWindow(subtle->disp, c->frame, c->rect.x, c->rect.y, c->rect.width, subtle->th);
            subClientRender(c);
            break;
#if 0            
          case SUB_STATE_FLOAT:
            /* Respect the user/program preferences */
            hints = XAllocSizeHints();
            if(!hints) subUtilLogError("Can't alloc memory. Exhausted?\n");
            if(XGetWMNormalHints(subtle->disp, c->win, hints, &supplied))
              {
                if(hints->flags & (USPosition|PPosition))
                  {
                    c->rect.x = hints->x + 2 * subtle->bw;
                    c->rect.y = hints->y + subtle->th + subtle->bw;
                  }
                else if(hints->flags & PAspect)
                  {
                    c->rect.x = (hints->min_aspect.x - hints->max_aspect.x) / 2;
                    c->rect.y = (hints->min_aspect.y - hints->max_aspect.y) / 2;
                  }
                if(hints->flags & (USSize|PSize))
                  {
                    c->rect.width  = hints->width + 2 * subtle->bw;
                    c->rect.height  = hints->height + subtle->th + subtle->bw;
                  }
                else if(hints->flags & PBaseSize)
                  {
                    c->rect.width  = hints->base_width + 2 * subtle->bw;
                    c->rect.height  = hints->base_height + subtle->th + subtle->bw;
                  }
                else
                  {
                    /* Fallback for clients breaking the ICCCM (mostly Gtk+ stuff) */
                    if(hints->base_width > 0 && hints->base_width <= DisplayWidth(subtle->disp, DefaultScreen(subtle->disp)) &&
                      hints->base_height > 0 && hints->base_height <= DisplayHeight(subtle->disp, DefaultScreen(subtle->disp))) 
                      {
                        c->rect.width  = hints->base_width + 2 * subtle->bw;
                        c->rect.height  = hints->base_width + subtle->th + subtle->bw;
                        c->rect.x      = (DisplayWidth(subtle->disp, DefaultScreen(subtle->disp)) - c->rect.width) / 2;
                        c->rect.y      = (DisplayHeight(subtle->disp, DefaultScreen(subtle->disp)) - c->rect.height) / 2;
                      }
                  }
              }

            subClientConfigure(c);
            XReparentWindow(subtle->disp, c->frame, DefaultRootWindow(subtle->disp), c->rect.x, c->rect.y);
            XRaiseWindow(subtle->disp, c->frame);

            XFree(hints);
            break;
          case SUB_STATE_FULL:
            /* Unmap some windows */
            XUnmapWindow(subtle->disp, c->title);
            XUnmapWindow(subtle->disp, c->left);
            XUnmapWindow(subtle->disp, c->right);
            XUnmapWindow(subtle->disp, c->bottom);                
            XUnmapWindow(subtle->disp, c->caption);

            /* Resize to display resolution */
            c->rect.x      = 0;
            c->rect.y      = 0;
            c->rect.width  = DisplayWidth(subtle->disp, DefaultScreen(subtle->disp));
            c->rect.height  = DisplayHeight(subtle->disp, DefaultScreen(subtle->disp));

            XReparentWindow(subtle->disp, c->frame, DefaultRootWindow(subtle->disp), 0, 0);
            subClientConfigure(c);
#endif            
        }
    }
  XUngrabServer(subtle->disp);
  while(XCheckTypedEvent(subtle->disp, UnmapNotify, &event));
  if(type != SUB_STATE_FULL) subViewConfigure(subtle->cv);
} /* }}} */

  /** subClientFetchName {{{
   * @brief Fetch client name and store it
   * @param[in]  c  A #SubClient
   **/

void
subClientFetchName(SubClient *c)
{
  int width;

  assert(c);

  if(c->name) XFree(c->name);
  XFetchName(subtle->disp, c->win, &c->name);
  if(!c->name) c->name = strdup("subtle");

  /* Check max length of the caption */
  width = (strlen(c->name) + 1) * subtle->fx + 3;
  if(width > c->rect.width - subtle->th - 4) width = c->rect.width - subtle->th - 14;
  XMoveResizeWindow(subtle->disp, c->caption, 0, 0, width, subtle->th);

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
  return(state);
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
      subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_CLIENT_LIST, wins, subtle->clients->ndata);
      subEwmhSetWindows(DefaultRootWindow(subtle->disp), SUB_EWMH_NET_CLIENT_LIST_STACKING, wins, subtle->clients->ndata);

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
  XSelectInput(subtle->disp, c->frame, NoEventMask);
  XDeleteContext(subtle->disp, c->frame, 1);

  if(subtle->focus == c->frame) subtle->focus = 0; ///< Unset focus
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
  XDestroySubwindows(subtle->disp, c->frame);
  XDestroyWindow(subtle->disp, c->frame);
  subViewSanitize(c);
  subArrayPop(subtle->clients, (void *)c);

  if(c->name) XFree(c->name);
  free(c);

  subUtilLogDebug("kill=client\n");
} /* }}} */
