
 /**
  * @package subtle
  *
  * @file Client functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <X11/Xatom.h>
#include "subtle.h"

/* Typedef {{{ */
typedef struct clientgravity_t
{
  int grav_right, grav_down, cells_x, cells_y;
} ClientGravity;
/* }}} */

/* ClientMask {{{ */
static void
ClientMask(SubClient *c,
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

  XDrawRectangle(subtle->disp, ROOT, subtle->gcs.invert, r->x - 1, r->y - 1,
    r->width - 3, r->height - 3);
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

  if(SNAP + subtle->th > r->y) r->y = subtle->bw + subtle->th;
  else if(r->y > (SCREENH - WINH(c) - SNAP)) 
    r->y = SCREENH - c->rect.height + subtle->bw;
} /* }}} */

 /** subClientNew {{{
  * @brief Create new client
  * @param[in]  win  Client window
  * @return Returns a new #SubClient or \p NULL
  **/

SubClient *
subClientNew(Window win)
{
  int i, n;
  long vid = 1337;
  Window trans = 0;
  XWMHints *hints = NULL;
  XWindowAttributes attrs;
  Atom *protos = NULL;
  SubClient *c = NULL, *k = NULL;

  assert(win);

  /* Create client */
  c = CLIENT(subSharedMemoryAlloc(1, sizeof(SubClient)));
  c->flags = SUB_TYPE_CLIENT;
  c->tags  = SUB_TAG_DEFAULT;
  c->klass = subEwmhGetProperty(win, XA_STRING, SUB_EWMH_WM_CLASS, NULL);
  c->win   = win;

  /* Fetch name */
  if(!XFetchName(subtle->disp, c->win, &c->name))
    {
      /* Fallback for clients without WM_NAME like skype.. */
      if(c->klass) c->name = strdup(c->klass);
      else
        {
          c->name  = strdup("subtle");
          c->klass = NULL;
        }
    }
  c->width = XTextWidth(subtle->xfs, c->name, strlen(c->name)) + 6; ///< Font offset

  /* Update client */
  subEwmhSetWMState(c->win, WithdrawnState);
  XSelectInput(subtle->disp, c->win, EVENTMASK);
  XSetWindowBorderWidth(subtle->disp, c->win, subtle->bw);
  XAddToSaveSet(subtle->disp, c->win);
  XSaveContext(subtle->disp, c->win, CLIENTID, (void *)c);
  subClientSetSize(c);
  subClientSetTags(c);
  subClientSetStrut(c);

  /* Window attributes */
  XGetWindowAttributes(subtle->disp, c->win, &attrs);
  c->cmap        = attrs.colormap;
  c->rect.x      = attrs.x;
  c->rect.y      = attrs.y;
  c->rect.width  = attrs.width;
  c->rect.height = attrs.height;

  /* Protocols */
  if(XGetWMProtocols(subtle->disp, c->win, &protos, &n))
    {
      for(i = 0; i < n; i++)
        {
          switch(subEwmhFind(protos[i]))
            {
              case SUB_EWMH_WM_TAKE_FOCUS:    c->flags |= SUB_PREF_FOCUS; break;
              case SUB_EWMH_WM_DELETE_WINDOW: c->flags |= SUB_PREF_CLOSE; break;
              default: break;
            }
         }

      XFree(protos);
    }

  /* Window manager hints */
  if((hints = XGetWMHints(subtle->disp, c->win)))
    {
      if(hints->flags & XUrgencyHint)              c->tags  |= SUB_TAG_STICK;
      if(hints->flags & InputHint && hints->input) c->flags |= SUB_PREF_INPUT;
      if(hints->flags & WindowGroupHint)           
        {
          c->flags |= SUB_PREF_GROUP;
          c->group  = hints->window_group;
        }

      XFree(hints);
    }

  /* Check for transient windows */
  if(XGetTransientForHint(subtle->disp, c->win, &trans))
    if((k = CLIENT(subSharedFind(trans, CLIENTID))))
      {
        c->tags = k->tags; ///< Copy tags
        subClientToggle(c, SUB_STATE_STICK|SUB_STATE_FLOAT);

        if(subtle->windows.focus != c->win) ///< Move pointer to transient
          subClientWarp(c);
      }

  /* Properties */
  if(c->tags & SUB_TAG_FLOAT) subClientToggle(c, SUB_STATE_FLOAT);
  if(c->tags & SUB_TAG_FULL)  subClientToggle(c, SUB_STATE_FULL);
  if(c->tags & SUB_TAG_STICK) subClientToggle(c, SUB_STATE_STICK);

  /* Gravity */
  if(c->tags & SUB_TAG_TOP)
    {
      if(c->tags & SUB_TAG_LEFT)       c->gravity = SUB_GRAVITY_TOP_LEFT;
      else if(c->tags & SUB_TAG_RIGHT) c->gravity = SUB_GRAVITY_TOP_RIGHT;
      else                             c->gravity = SUB_GRAVITY_TOP;
    }
  else if(c->tags & SUB_TAG_BOTTOM)
    {
      if(c->tags & SUB_TAG_LEFT)       c->gravity = SUB_GRAVITY_BOTTOM_LEFT;
      else if(c->tags & SUB_TAG_RIGHT) c->gravity = SUB_GRAVITY_BOTTOM_RIGHT;
      else                             c->gravity = SUB_GRAVITY_BOTTOM;
    }
  else if(c->tags & SUB_TAG_LEFT)      c->gravity = SUB_GRAVITY_LEFT;
  else if(c->tags & SUB_TAG_RIGHT)     c->gravity = SUB_GRAVITY_RIGHT;
  else if(0 < subtle->gravity)         c->gravity = subtle->gravity;
  else 
    {
      if((k = CLIENT(subSharedFind(subtle->windows.focus, CLIENTID))))
        c->gravity = k->gravity;
      else c->gravity = SUB_GRAVITY_CENTER;
    }

  c->gravities = (int *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(int));

  for(i = 0; i < subtle->views->ndata; i++)
    c->gravities[i] = c->gravity;
  c->gravity = -1; ///< Force update

  /* EWMH: Tags, gravity and desktop */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&c->tags, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_GRAVITY, (long *)&c->gravity, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

  if(subtle->hooks.create) ///< Create hook
    subRubyCall(SUB_TYPE_HOOK, subtle->hooks.create, c);

  subSharedLogDebug("new=client, name=%s, win=%#lx\n", c->name, win);

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

  r = c->rect;

  if(c->flags & SUB_STATE_FLOAT)
    {
      r.y += subtle->th;
    }
  else ///< Border
    {
      r.width  -= 2 * subtle->bw;
      r.height -= 2 * subtle->bw;
    }

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
    c->rect.x, c->rect.y, c->rect.width, c->rect.height);
} /* }}} */

 /** subClientRender {{{
  * @brief Render client and redraw titlebar and borders
  * @param[in]  c  A #SubClient
  **/

void
subClientRender(SubClient *c)
{
  int pos = 0;
  char buf[255];
  XSetWindowAttributes attrs;

  assert(c);

  attrs.border_pixel = subtle->windows.focus == c->win ? subtle->colors.focus : subtle->colors.norm;
  XChangeWindowAttributes(subtle->disp, c->win, CWBorderPixel, &attrs);

  /* Caption */
  if(c->flags & (SUB_STATE_STICK|SUB_STATE_FLOAT))
    {
      snprintf(buf, sizeof(buf), "%c%s", c->flags & SUB_STATE_STICK ? '*' : '^', c->name);
      pos = (subtle->xfs->min_bounds.width + subtle->xfs->max_bounds.width) / 2; ///< Width of char
    }
  else snprintf(buf, sizeof(buf), "%s", c->name);

  XResizeWindow(subtle->disp, subtle->windows.caption, c->width + pos, subtle->th);
  XClearWindow(subtle->disp, subtle->windows.caption);

  XDrawString(subtle->disp, subtle->windows.caption, subtle->gcs.font, 3, subtle->fy - 1,
    buf, strlen(buf));
} /* }}} */

 /** subClientFocus {{{
  * @brief Set focus to client
  * @param[in]  c  A #SubClient
  **/

void
subClientFocus(SubClient *c)
{
  assert(c);


  /* Check client input focus type */
  if(!(c->flags & SUB_PREF_INPUT) && c->flags & SUB_PREF_FOCUS)
    {
      subEwmhMessage(c->win, c->win, SUB_EWMH_WM_PROTOCOLS, 
        subEwmhGet(SUB_EWMH_WM_TAKE_FOCUS), CurrentTime, 0, 0, 0);
    }   
  else XSetInputFocus(subtle->disp, c->win, RevertToNone, CurrentTime);

  if(subtle->hooks.focus) ///< Focus hook
    subRubyCall(SUB_TYPE_HOOK, subtle->hooks.focus, (void *)c);

  subSharedLogDebug("Focus: win=%#lx, input=%d, focus=%d\n", c->win,
    !!(c->flags & SUB_PREF_INPUT), !!(c->flags & SUB_PREF_FOCUS));
} /* }}} */

 /** subClientWarp {{{
  * @brief Warp pointer to window center
  * @param[in]  c  A #SubClient
  **/

void
subClientWarp(SubClient *c)
{
  assert(c);

  XWarpPointer(subtle->disp, None, ROOT, 0, 0, 0, 0,
    c->rect.x + c->rect.width / 2, c->rect.y + c->rect.height / 2);
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
  Window win;
  unsigned int mask = 0;
  int loop = True, left = False, wx = 0, wy = 0, rx = 0, ry = 0;
  short *dirx = NULL, *diry = NULL;
  XRectangle r;
  Cursor cursor;
  SubClient *c2 = NULL;

  assert(c);
 
  /* Init {{{ */
  XQueryPointer(subtle->disp, c->win, &win, &win, &rx, &ry, &wx, &wy, &mask);
  r.x      = rx - wx;
  r.y      = ry - wy;
  r.width  = c->rect.width;
  r.height = c->rect.height; 

  switch(mode)
    {
      case SUB_DRAG_MOVE:
        dirx   = &r.x;
        diry   = &r.y;
        cursor = subtle->cursors.move;
        break;
      case SUB_DRAG_RESIZE:
        dirx   = (short *)&r.width;
        diry   = (short *)&r.height;
        cursor = subtle->cursors.resize;
        left   = wx < c->rect.width / 2; ///< Resize from left
        break;
    } /* }}} */

  if(XGrabPointer(subtle->disp, c->win, True, GRABMASK, GrabModeAsync, GrabModeAsync, None,
    cursor, CurrentTime)) return;

  XGrabServer(subtle->disp);
  if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE)) ClientMask(c, &r);

  while(loop) ///< Event loop
    {
      XMaskEvent(subtle->disp, DRAGMASK, &ev);
      switch(ev.type)
        {
          case EnterNotify:   win = ev.xcrossing.window; break; ///< Find destination window
          case ButtonRelease: loop = False;              break;
          case FocusIn:
          case FocusOut:                                 break; ///< Ignore focus changes
          case KeyPress: /* {{{ */
            if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE))
              {
                KeySym sym = XKeycodeToKeysym(subtle->disp, ev.xkey.keycode, 0);
                ClientMask(c, &r);

                switch(sym)
                  {
                    case XK_Left:   *dirx -= subtle->step; break;
                    case XK_Right:  *dirx += subtle->step; break;
                    case XK_Up:     *diry -= subtle->step; break;
                    case XK_Down:   *diry += subtle->step; break;
                    case XK_Return: loop = False;   break;
                  }

                *dirx = MINMAX(*dirx, c->minw, c->maxw);
                *diry = MINMAX(*diry, c->minh, c->maxh);
              
                ClientMask(c, &r);
              }
            break; /* }}} */
          case MotionNotify: /* {{{ */
            if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE))
              {
                ClientMask(c, &r);
          
                /* Calculate selection rect */
                switch(mode)
                  {
                    case SUB_DRAG_MOVE:
                      r.x = (rx - wx) - (rx - ev.xmotion.x_root);
                      r.y = (ry - wy) - (ry - ev.xmotion.y_root);

                      ClientSnap(c, &r); ///< Snap to border
                      break;
                    case SUB_DRAG_RESIZE: 
                      if(c->rect.width + ev.xmotion.x_root >= rx)
                        {
                          int width = left ? c->rect.width + (rx - ev.xmotion.x_root) : 
                            c->rect.width - (rx - ev.xmotion.x_root);

                          if(width >= c->minw && width <= c->maxw && 
                            c->rect.x + width < SCREENW) ///< Limit size
                            {
                              r.width = width - (width % c->incw); 
                              if(left) r.x = c->rect.x + c->rect.width - r.width;
                            }
                        }

                      if(c->rect.height + ev.xmotion.y_root >= ry)
                        {
                          int height = c->rect.height - (ry - ev.xmotion.y_root);

                          if(height >= c->minh && height <= c->maxh &&
                            c->rect.y + height < SCREENH - subtle->bw) ///< Limit size
                            r.height = height - (height % c->inch);
                        }

                      /* Check aspect ratios */
                      if(c->minr && r.height * c->minr > r.width)
                        r.width = (int)(r.height * c->minr);

                      if(c->maxr && r.height * c->maxr < r.width)
                        r.width = (int)(r.height * c->maxr);
                        
                      break;
                  }  

                ClientMask(c, &r);
              }
            break; /* }}} */
        }
    }

  ClientMask(c2, &r); ///< Erase mask

  if(c->flags & SUB_STATE_FLOAT) ///< Resize client
    {
      r.y     -= (subtle->th + subtle->bw); ///< Border and bar height
      r.x     -= subtle->bw;
      c->rect  = r;

      subClientConfigure(c);
    }
  
  XUngrabPointer(subtle->disp, CurrentTime);
  XUngrabServer(subtle->disp);
} /* }}} */

  /** subClientUpdate {{{ 
   * @brief Update gravity array
   * @param[in]  vid  View index
   **/

void
subClientUpdate(int vid)
{
  int i, j;

  for(i = 0; i < subtle->clients->ndata; i++)
    {
      SubClient *c = CLIENT(subtle->clients->data[i]);

      /* Shift gravities if necessary */
      for(j = vid; -1 < vid && j < subtle->views->ndata - 1; j++)
        c->gravities[j] = c->gravities[j + 1];

      c->gravities = (int *)subSharedMemoryRealloc((void *)c->gravities, 
        subtle->views->ndata * sizeof(int));

      if(-1 == vid) c->gravities[subtle->views->ndata - 1] = SUB_GRAVITY_CENTER; ///< Initialise 
    }
} /* }}} */

  /** subClientSetGravity {{{ 
   * @brief Set client gravity
   * @param[in]  c     A #SubClient
   * @param[in]  type  A #SubGravity
   **/

void
subClientSetGravity(SubClient *c,
  int type)
{
  int grav = 0, mode = 0;
  XRectangle slot = { 0 };
  static const ClientGravity props[] = /* {{{ */
  {
    { 0, 1, 1, 1 }, ///< Gravity unknown
    { 0, 1, 2, 2 }, ///< Gravity bottom left
    { 0, 1, 1, 2 }, ///< Gravity bottom
    { 1, 1, 2, 2 }, ///< Gravity bottom right
    { 0, 0, 2, 1 }, ///< Gravity left
    { 0, 0, 1, 1 }, ///< Gravity center
    { 1, 0, 2, 1 }, ///< Gravity right
    { 0, 0, 2, 2 }, ///< Gravity top left
    { 0, 0, 1, 2 }, ///< Gravity top
    { 1, 0, 2, 2 }, ///< Gravity top right
  }; /* }}} */

  assert(c);

  /* Modes */
  grav = type & ~SUB_GRAVITY_ALL;

  /* Compute slot */
  slot.height = subtle->screen.height / props[grav].cells_y;
  slot.width  = subtle->screen.width / props[grav].cells_x;
  slot.x      = subtle->screen.x + props[grav].grav_right * slot.width;
  slot.y      = subtle->screen.y + props[grav].grav_down * slot.height;

  /* Toggle between modes */
  if(type & SUB_GRAVITY_HORZ &&
    (type & SUB_GRAVITY_MODES || (c->rect.y == slot.y && c->rect.height == slot.height)))
    {
      int width33 = subtle->screen.width / 3;
      int width66 = subtle->screen.width - width33;
      int compw   = abs(subtle->screen.width - 3 * width33); ///< Int rounding fix

      if(2 == props[grav].cells_x)
        {
          int x = subtle->screen.x + props[grav].grav_right * width33;

          if(type & SUB_GRAVITY_MODE66 ||
            (!(type & SUB_GRAVITY_MODES) && c->rect.width == slot.width && c->rect.x == slot.x)) ///< 33%
            {
              slot.x     = x;
              slot.width = width66;
              mode       = SUB_GRAVITY_HORZ|SUB_GRAVITY_MODE66;
            }
          else if(type & SUB_GRAVITY_MODE33 ||
            (!(type & SUB_GRAVITY_MODES) && c->rect.width == width66 && c->rect.x == x)) ///< 66%
            {
              slot.x     = subtle->screen.x + props[grav].grav_right * width66;
              slot.width = width33;
              mode       = SUB_GRAVITY_HORZ|SUB_GRAVITY_MODE33;
            }
        }
      else if(type & SUB_GRAVITY_MODE33 ||
        (!(type & SUB_GRAVITY_MODES) && c->rect.width == slot.width && c->rect.x == slot.x))
        {
          slot.x     = subtle->screen.x + width33;
          slot.width = width33 + compw;
          mode       = SUB_GRAVITY_HORZ|SUB_GRAVITY_MODE33;
        }
    }  
  else if(type & SUB_GRAVITY_VERT && 
    (type & SUB_GRAVITY_MODES || (c->rect.x == slot.x && c->rect.width == slot.width)))
    {
      int height33 = subtle->screen.height / 3;
      int height66 = subtle->screen.height - height33;
      int comph    = abs(subtle->screen.height - 3 * height33); ///< Int rounding fix

      if(2 == props[grav].cells_y)
        {
          int y = subtle->screen.y + props[grav].grav_down * height33;

          if(type & SUB_GRAVITY_MODE66 || 
            (!(type & SUB_GRAVITY_MODES) && c->rect.height == slot.height && c->rect.y == slot.y)) ///< 33%
            {
              slot.y      = y;
              slot.height = height66;
              mode        = SUB_GRAVITY_VERT|SUB_GRAVITY_MODE66;
            }
          else if(type & SUB_GRAVITY_MODE33 ||
            (!(type & SUB_GRAVITY_MODES) && c->rect.height == height66 && c->rect.y == y)) ///< 66%
            {
              slot.y      = subtle->screen.y + props[grav].grav_down * height66;
              slot.height = height33;
              mode        = SUB_GRAVITY_VERT|SUB_GRAVITY_MODE33;
            }
        }
      else if(type & SUB_GRAVITY_MODE33 ||
        (!(type & SUB_GRAVITY_MODES) && c->rect.height == slot.height && c->rect.y == slot.y))
        {
          slot.y      = subtle->screen.y + height33;
          slot.height = height33 + comph;
          mode        = SUB_GRAVITY_VERT|SUB_GRAVITY_MODE33;
        }
    }

  /* Update client rect */
  c->rect    = slot;
  c->gravity = grav | mode;

  /* EWMH: Gravity */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_GRAVITY, (long *)&c->gravity, 1);

  if(subtle->hooks.gravity) ///< Gravity hook
    subRubyCall(SUB_TYPE_HOOK, subtle->hooks.gravity, (void *)c);
} /* }}} */

  /** subClientSetSize {{{ 
   * @brief Set client size
   * @param[in]  c  A #SubClient
   **/

void
subClientSetSize(SubClient *c)
{
  long supplied = 0;
  XSizeHints *size = NULL;

  assert(c);

  if(!(size = XAllocSizeHints()))
    subSharedLogError("Can't alloc memory. Exhausted?\n");

  /* Default values {{{ */
  c->flags &= ~SUB_PREF_POS;
  c->flags &= ~SUB_PREF_SIZE;
  c->minw   = MINW;
  c->minh   = MINH;
  c->maxw   = SCREENW;
  c->maxh   = SCREENH - subtle->th;
  c->basew  = 0;
  c->baseh  = 0;
  c->minr   = 0.0f;
  c->maxr   = 0.0f;
  c->incw   = 1;
  c->inch   = 1; /* }}} */

  /* Size hints */
  if(XGetWMNormalHints(subtle->disp, c->win, size, &supplied))
    {
      if(size->flags & PMinSize) ///< Program min size
        {
          if(size->min_width)  c->minw = size->min_width;
          if(size->min_height) c->minh = size->min_height;
        }

      if(size->flags & PMaxSize) ///< Program max size
        {
          if(size->max_width)  c->maxw = MIN(size->max_width, SCREENW);
          if(size->max_height) c->maxh = MIN(size->max_height, SCREENH - subtle->th);
        }

      if(size->flags & PBaseSize) ///< Program max size
        {
          if(size->base_width)  c->basew = MIN(size->base_width, SCREENW);
          if(size->base_height) c->baseh = MIN(size->base_height, SCREENH - subtle->th);
        }

      if(size->flags & PAspect) ///< Aspect
        {
          if(size->min_aspect.y) c->minr = (float)size->min_aspect.x / size->min_aspect.y;
          if(size->max_aspect.y) c->maxr = (float)size->max_aspect.x / size->max_aspect.y;
        }

      if(size->flags & PResizeInc) ///< Resize inc
        {
          if(size->width_inc)  c->incw = size->width_inc;
          if(size->height_inc) c->inch = size->height_inc;
        }

      XFree(size);
    }

  subSharedLogDebug("Size: x=%d, y=%d, width=%d, height=%d, minw=%d, minh=%d, " \
    "maxw=%d, maxh=%d, basew=%d, baseh=%d, minr=%f, maxr=%f\n",
    c->rect.x, c->rect.y, c->rect.width, c->rect.height,
    c->minw, c->minh, c->maxw, c->maxh, c->basew, c->basew, c->minr, c->maxr);
} /* }}} */

 /** subClientSetTags {{{
  * @brief Set client tags
  * @param[in]  c  A #SubClient
  **/

void
subClientSetTags(SubClient *c)
{
  int i;

  for(i = 0; (c->name || c->klass) && i < subtle->tags->ndata; i++)
    {
      SubTag *t = TAG(subtle->tags->data[i]);

      if(t->preg && ((c->name && subSharedRegexMatch(t->preg, c->name)) ||
        (c->klass && subSharedRegexMatch(t->preg, c->klass))))
          c->tags |= (1L << (i + 1));
    }
  if(1 < (c->tags >> SUB_TAG_CLEAR)) c->tags &= ~SUB_TAG_DEFAULT;  
} /* }}} */

  /** subClientSetStrut {{{ 
   * @brief Set client strut
   * @param[in]  c  A #SubClient
   **/

void
subClientSetStrut(SubClient *c)
{
  unsigned long size = 0;
  long *strut = NULL;

  assert(c);

  /* Get strut property */
  if((strut = (long *)subEwmhGetProperty(c->win, XA_CARDINAL, 
    SUB_EWMH_NET_WM_STRUT, &size)))
    {
      if(4 == size) ///< Only complete struts
        {
          subtle->strut.x      = MAX(subtle->strut.x,      strut[0]); ///< Left
          subtle->strut.y      = MAX(subtle->strut.y,      strut[1]); ///< Right
          subtle->strut.width  = MAX(subtle->strut.width,  strut[2]); ///< Top
          subtle->strut.height = MAX(subtle->strut.height, strut[3]); ///< Bottom

          subSharedLogDebug("Strut: x=%ld, y=%d, width=%d, height=%d\n", subtle->strut.x,
            subtle->strut.y, subtle->strut.width, subtle->strut.height);
        }

      XFree(strut);
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
  assert(c);

  if(c->flags & type)
    {
      c->flags  &= ~type;

      if(type & SUB_STATE_FULL)
        XSetWindowBorderWidth(subtle->disp, c->win, subtle->bw);

      if(type & SUB_STATE_FLOAT) c->gravity = -1; ///< Updating gravity
      if(type & SUB_STATE_STICK) subViewConfigure(subtle->cv);
    }
  else
    {
      c->flags |= type;

      if(type & SUB_STATE_FLOAT)
        {
          /* Ignore values if they are garbage */
          if(c->minw > c->rect.width || c->minh > c->rect.width ||
            c->maxw < c->rect.width || c->maxh < c->rect.height)
            {
              c->rect.width  = c->minw;
              c->rect.height = c->minh;
            }

          if(0 < c->rect.x || 0 < c->rect.y ||
            c->rect.x + c->rect.width > SCREENW ||
            c->rect.y + c->rect.height > SCREENH - subtle->th) ///< Center
            {
              c->rect.x = (SCREENW - c->rect.width) / 2;
              c->rect.y = ((SCREENH - subtle->th) - c->rect.height) / 2;
            }
        }

      if(type & SUB_STATE_FULL)
        XSetWindowBorderWidth(subtle->disp, c->win, 0);
    }

  subClientConfigure(c);

  if(VISIBLE(subtle->cv, c)) ///< Check visibility first
    {
      subClientFocus(c);
      subClientRender(c);
    }
} /* }}} */

 /** subClientPublish {{{
  * @brief Publish clients
  **/

void
subClientPublish(void)
{
  int i;
  Window *wins = (Window *)subSharedMemoryAlloc(subtle->clients->ndata, sizeof(Window));

  for(i = 0; i < subtle->clients->ndata; i++)
    wins[i] = CLIENT(subtle->clients->data[i])->win;

  /* EWMH: Client list and client list stacking */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST, wins, subtle->clients->ndata);
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST_STACKING, wins,
    subtle->clients->ndata);

  subSharedLogDebug("publish=client, clients=%d\n", subtle->clients->ndata);

  free(wins);
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

  /* Ignore further events and delete context */
  XSelectInput(subtle->disp, c->win, NoEventMask);
  XDeleteContext(subtle->disp, c->win, CLIENTID);

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

  subSharedFocus(); ///< Focus

  if(c->gravities) free(c->gravities);
  if(c->klass) XFree(c->klass);
  if(c->name) XFree(c->name);
  free(c);

  subSharedLogDebug("kill=client\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
