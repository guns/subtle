
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
ClientMask(SubClient *c)
{
  XDrawRectangle(subtle->dpy, ROOT, subtle->gcs.invert, c->geom.x - 1, c->geom.y - 1,
    c->geom.width + 1, c->geom.height + 1);
} /* }}} */

/* ClientSnap {{{ */
static void
ClientSnap(SubClient *c)
{
  assert(c);

  if(subtle->screen->base.x + subtle->snap > c->geom.x) 
    c->geom.x = subtle->screen->base.x + subtle->bw;
  else if(c->geom.x > (subtle->screen->base.x + subtle->screen->base.width - WINW(c) - subtle->snap)) 
    c->geom.x = subtle->screen->base.x + subtle->screen->base.width - c->geom.width - subtle->bw;

  if(subtle->screen->base.y + subtle->snap + (0 < subtle->screen->base.y ? 0 : subtle->th) > c->geom.y)
    c->geom.y = subtle->screen->base.y + subtle->bw + (0 < subtle->screen->base.y ? 0 : subtle->th);
  else if(c->geom.y > (subtle->screen->base.y + subtle->screen->base.height - WINH(c) - subtle->snap)) 
    c->geom.y = subtle->screen->base.y + subtle->screen->base.height - c->geom.height - subtle->bw;
} /* }}} */

 /** subClientNew {{{
  * @brief Create new client
  * @param[in]  win  Client window
  * @return Returns a new #SubClient or \p NULL
  **/

SubClient *
subClientNew(Window win)
{
  int i, n, grav;
  long vid = 0;
  Window trans = 0;
  XWMHints *hints = NULL;
  XWindowAttributes attrs;
  XSetWindowAttributes sattrs;
  Atom *protos = NULL;
  SubClient *c = NULL, *k = NULL;

  assert(win);

  /* Create client */
  c = CLIENT(subSharedMemoryAlloc(1, sizeof(SubClient)));
  c->gravities = (int *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(int));
  c->screens   = (int *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(int));
  c->flags     = SUB_TYPE_CLIENT;
  c->gravity   = -1; ///< Force update
  c->win       = win;

 /* Default gravity */
  if(0 == subtle->gravity && (k = CLIENT(subSharedFind(subtle->windows.focus, CLIENTID))))
    grav = k->gravity; ///< Copy gravity
  else grav = subtle->gravity; ///< Set default

  /* Init gravities and screens */
  for(i = 0; i < subtle->views->ndata; i++)
    {
      c->gravities[i] = grav;
      c->screens[i]   = 0;
    }

   /* Fetch name and class */
  XFetchName(subtle->dpy, c->win, &c->caption);
  subSharedPropertyClass(c->win, &c->name, &c->klass);

  /* Fallback for stupid clients like Skype */
  if(!c->caption || !strcmp("", c->caption))
    {
      if(c->name) c->caption = strdup(c->name);
      else if(c->klass) c->caption = strdup(c->klass);
      else c->caption = strdup("subtle");
    }
 
  /* X related properties */
  sattrs.border_pixel = subtle->colors.bo_normal;
  XChangeWindowAttributes(subtle->dpy, c->win, CWBorderPixel, &sattrs);
  XSelectInput(subtle->dpy, c->win, EVENTMASK);
  XSetWindowBorderWidth(subtle->dpy, c->win, subtle->bw);
  XAddToSaveSet(subtle->dpy, c->win);
  XSaveContext(subtle->dpy, c->win, CLIENTID, (void *)c);

  /* Window attributes */
  XGetWindowAttributes(subtle->dpy, c->win, &attrs);
  c->cmap        = attrs.colormap;
  c->geom.x      = attrs.x;
  c->geom.y      = attrs.y;
  c->geom.width  = MAX(MINW, attrs.width);
  c->geom.height = MAX(MINH, attrs.height);
  c->base        = c->geom; ///< Backup size

  /* Update client */
  subEwmhSetWMState(c->win, WithdrawnState);
  subClientSetHints(c);
  subClientSetStrut(c);
  subClientSetSize(c);
  subClientSetTags(c);

  /* Window manager protocols */
  if(XGetWMProtocols(subtle->dpy, c->win, &protos, &n))
    {
      for(i = 0; i < n; i++)
        {
          switch(subEwmhFind(protos[i]))
            {
              case SUB_EWMH_WM_TAKE_FOCUS:    c->flags |= SUB_CLIENT_FOCUS; break;
              case SUB_EWMH_WM_DELETE_WINDOW: c->flags |= SUB_CLIENT_CLOSE; break;
              default: break;
            }
         }

      XFree(protos);
    }

  /* Window manager hints */
  if((hints = XGetWMHints(subtle->dpy, c->win)))
    {
      if(hints->flags & XUrgencyHint)              c->tags  |= SUB_CLIENT_URGENT;
      if(hints->flags & InputHint && hints->input) c->flags |= SUB_CLIENT_INPUT;

      XFree(hints);
    }

  /* Check for transient windows */
  if(XGetTransientForHint(subtle->dpy, c->win, &trans))
    {
      c->flags |= SUB_CLIENT_URGENT;

      if((k = CLIENT(subSharedFind(trans, CLIENTID))))
        c->tags |= k->tags; ///< Copy tags
     }

  /* Urgent windows */
  if(c->flags & SUB_CLIENT_URGENT)
    {
      subClientToggle(c, (~c->flags & (SUB_MODE_FLOAT|SUB_MODE_STICK)));
      subClientWarp(c);
    }

  /* EWMH: Gravity, screen and desktop */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_GRAVITY, (long *)&c->gravity, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_SCREEN, (long *)&c->screen, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

  if(subtle->hooks.create) ///< Create hook
    subRubyCall(SUB_TYPE_HOOK, subtle->hooks.create, c);

  subSharedLogDebug("new=client, name=%s, klass=%s, win=%#lx\n", c->name, c->klass, win);

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
  DEAD(c);

  r = c->geom;

  if(c->flags & SUB_MODE_FLOAT)
    {
      r.y += subtle->th;
    }
  else ///< Border
    {
      r.width  -= 2 * subtle->bw;
      r.height -= 2 * subtle->bw;
    }

  if(c->flags & SUB_MODE_FULL) ///< Get fullscreen size of screen
    r = SCREEN(subtle->screens->data[c->screen])->base;

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

  XMoveResizeWindow(subtle->dpy, c->win, r.x, r.y, r.width, r.height);
  XSendEvent(subtle->dpy, c->win, False, StructureNotifyMask, (XEvent *)&ev);

  subSharedLogDebug("configure=client, win=%#lx, name=%s, state=%c, x=%03d, y=%03d, width=%03d, height=%03d\n",
    c->win, c->klass, c->flags & SUB_MODE_FLOAT ? 'f' : c->flags & SUB_MODE_FULL ? 'u' : 'n',
    c->geom.x, c->geom.y, c->geom.width, c->geom.height);
} /* }}} */

 /** subClientRender {{{
  * @brief Render client and redraw titlebar and borders
  * @param[in]  c  A #SubClient
  **/

void
subClientRender(SubClient *c)
{
  char buf[255];
  XSetWindowAttributes sattrs;
  XGCValues gvals;

  DEAD(c);
  assert(c);

  /* Set window border */
  sattrs.border_pixel = subtle->windows.focus == c->win ? subtle->colors.bo_focus : 
    subtle->colors.bo_normal;
  XChangeWindowAttributes(subtle->dpy, c->win, CWBorderPixel, &sattrs);

  /* Caption */
  if(c->flags & (SUB_MODE_STICK|SUB_MODE_FLOAT))
    snprintf(buf, sizeof(buf), "%c%s", c->flags & SUB_MODE_STICK ? '*' : '^', c->caption);
  else snprintf(buf, sizeof(buf), "%s", c->caption);

  /* Caption title */
  gvals.foreground = subtle->colors.fg_focus;
  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);

  XClearWindow(subtle->dpy, subtle->panels.caption.win);

  XDrawString(subtle->dpy, subtle->panels.caption.win, subtle->gcs.font, 3, subtle->fy - 1,
    buf, strlen(buf));

} /* }}} */

 /** subClientFocus {{{
  * @brief Set focus to client
  * @param[in]  c  A #SubClient
  **/

void
subClientFocus(SubClient *c)
{
  DEAD(c);
  assert(c);

  /* Hook: Focus */
  if(subtle->hooks.focus && 
    0 == subRubyCall(SUB_TYPE_HOOK, subtle->hooks.focus, (void *)c))
    {
      subSharedLogDebug("Hook: name=focus, client=%#lx, state=ignored\n", c->win);

      return;
    }

  subClientSetCaption(c);

  /* Check client input focus type */
  if(!(c->flags & SUB_CLIENT_INPUT) && c->flags & SUB_CLIENT_FOCUS)
    {
      subEwmhMessage(c->win, c->win, SUB_EWMH_WM_PROTOCOLS, 
        subEwmhGet(SUB_EWMH_WM_TAKE_FOCUS), CurrentTime, 0, 0, 0);
    }   
  else XSetInputFocus(subtle->dpy, c->win, RevertToNone, CurrentTime);

  subSharedLogDebug("Focus: win=%#lx, input=%d, focus=%d\n", c->win,
    !!(c->flags & SUB_CLIENT_INPUT), !!(c->flags & SUB_CLIENT_FOCUS));
} /* }}} */

 /** subClientWarp {{{
  * @brief Warp pointer to window center
  * @param[in]  c  A #SubClient
  **/

void
subClientWarp(SubClient *c)
{
  DEAD(c);
  assert(c);

  XRaiseWindow(subtle->dpy, c->win);
  XWarpPointer(subtle->dpy, None, ROOT, 0, 0, 0, 0,
    c->geom.x + c->geom.width / 2, c->geom.y + c->geom.height / 2);
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
  int loop = True, left = False, wx = 0, wy = 0, ww = 0, wh = 0, rx = 0, ry = 0;
  short *dirx = NULL, *diry = NULL;
  Cursor cursor;
  XWindowAttributes attr;

  DEAD(c);
  assert(c);
 
  /* Init {{{ */
  XQueryPointer(subtle->dpy, c->win, &win, &win, &rx, &ry, &wx, &wy, &mask);
  XGetWindowAttributes(subtle->dpy, c->win, &attr);
  c->geom.x = rx - wx;
  c->geom.y = ry - wy;
  ww        = attr.width;
  wh        = attr.height;

  switch(mode)
    {
      case SUB_DRAG_MOVE:
        dirx   = &c->geom.x;
        diry   = &c->geom.y;
        cursor = subtle->cursors.move;
        break;
      case SUB_DRAG_RESIZE:
        dirx   = (short *)&c->geom.width;
        diry   = (short *)&c->geom.height;
        cursor = subtle->cursors.resize;
        left   = wx < c->geom.width / 2; ///< Resize from left
        break;
    } /* }}} */

  if(XGrabPointer(subtle->dpy, c->win, True, GRABMASK, GrabModeAsync, GrabModeAsync, None,
    cursor, CurrentTime)) return;

  XGrabServer(subtle->dpy);
  if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE)) ClientMask(c);

  while(loop) ///< Event loop
    {
      XMaskEvent(subtle->dpy, DRAGMASK, &ev);
      switch(ev.type)
        {
          case EnterNotify:   win = ev.xcrossing.window; break; ///< Find destination window
          case ButtonRelease: loop = False;              break;
          case FocusIn:
          case FocusOut:                                 break; ///< Ignore focus changes
          case KeyPress: /* {{{ */
            if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE))
              {
                KeySym sym = XKeycodeToKeysym(subtle->dpy, ev.xkey.keycode, 0);
                ClientMask(c);

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
              
                subClientSetSize(c);
                ClientMask(c);
              }
            break; /* }}} */
          case MotionNotify: /* {{{ */
            if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE))
              {
                ClientMask(c);
          
                /* Calculate selection rect */
                switch(mode)
                  {
                    case SUB_DRAG_MOVE:
                      c->geom.x = (rx - wx) - (rx - ev.xmotion.x_root);
                      c->geom.y = (ry - wy) - (ry - ev.xmotion.y_root);

                      ClientSnap(c); ///< Snap to border
                      break;
                    case SUB_DRAG_RESIZE: 
                      if(left) 
                        {
                          c->geom.width  = ww + (rx - ev.xmotion.x_root);
                          c->geom.width -= (c->geom.width % c->incw);
                          c->geom.x      = (rx - wx) + ww - c->geom.width;
                        }
                      else c->geom.width  = ww - (rx - ev.xmotion.x_root);

                      c->geom.height = wh - (ry - ev.xmotion.y_root);

                      subClientSetSize(c);
                      break;
                  }  

                ClientMask(c);
              }
            break; /* }}} */
        }
    }

  ClientMask(c); ///< Erase mask

  if(c->flags & SUB_MODE_FLOAT) ///< Resize client
    {
      c->geom.y -= (subtle->th + subtle->bw); ///< Border and bar height
      c->geom.x -= subtle->bw;

      subClientConfigure(c);
    }
  
  XUngrabPointer(subtle->dpy, CurrentTime);
  XUngrabServer(subtle->dpy);
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

      /* Shift if necessary */
      for(j = vid; -1 < vid && j < subtle->views->ndata - 1; j++)
        {
          c->gravities[j] = c->gravities[j + 1];
          c->screens[j]   = c->screens[j + 1];
        }

      /* Resize arrays */
      c->gravities = (int *)subSharedMemoryRealloc((void *)c->gravities,
        subtle->views->ndata * sizeof(int));
      c->screens = (int *)subSharedMemoryRealloc((void *)c->screens,
        subtle->views->ndata * sizeof(int));

      if(-1 == vid) ///< Initialise
        {
          c->gravities[subtle->views->ndata - 1] = subtle->gravity;
          c->screens[subtle->views->ndata - 1]   = 0;
        }
    }
} /* }}} */

 /** subClientTag {{{
  * @brief Set tag properties to client
  * @param[in]  c      A #SubClient
  * @param[in]  tagid  Tag id
  * @return Return changed flags
  **/

int
subClientTag(SubClient *c,
  int tag)
{
  int i, flags = 0;

  assert(c);
  if(!c || c->flags & SUB_CLIENT_DEAD) return flags;

  /* Update flags and tags */
  if(0 <= tag && subtle->tags->ndata > tag)
    {
      SubTag *t = TAG(subtle->tags->data[tag]);

      flags   |= (t->flags & (SUB_MODE_FULL|SUB_MODE_FLOAT|SUB_MODE_STICK));
      c->tags |= (1L << (tag + 1));

      /* Set size */
      if(t->flags & SUB_MODE_SIZE)
        {
          flags   |= SUB_MODE_FLOAT;
          c->base  = t->size;
        }

      /* Set gravity and screens for matching views */
      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          /* Match only views with this tag */
          if(v->tags & (1L << (tag + 1)))
            {
              if(t->flags & SUB_MODE_GRAVITY) c->gravities[i] = t->gravity;
              if(t->flags & SUB_MODE_SCREEN)  c->screens[i]   = t->screen;
            }
        }
    }

  return flags;
} /* }}} */

 /** subClientSetTags {{{
  * @brief Set client tags
  * @param[in]  c  A #SubClient
  **/

void
subClientSetTags(SubClient *c)
{
  int i, flags = 0, visible = 0;

  DEAD(c);
  assert(c);

  c->tags = 0; ///< Reset tags

  /* Check matching tags */
  for(i = 0; (c->name || c->klass) && i < subtle->tags->ndata; i++)
    {
      SubTag *t = TAG(subtle->tags->data[i]);

      if(t->preg && ((c->name && subSharedRegexMatch(t->preg, c->name)) || 
        (c->klass && subSharedRegexMatch(t->preg, c->klass))))
          flags |= subClientTag(c, i);
    }

  /* Check if client is visible on at least one screen */
  for(i = 0; i < subtle->views->ndata; i++)
    if(VIEW(subtle->views->data[i])->tags & c->tags) 
      {
        visible++;
        break;
      }

  if(0 == visible) flags |= subClientTag(c, 0); ///< Set default tag

  /* EWMH: Tags */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&c->tags, 1);

  subClientToggle(c, ~c->flags & flags); ///< Toggle flags
} /* }}} */

  /** subClientSetGravity {{{ 
   * @brief Set client gravity for current view
   * @param[in]  c        A #SubClient
   * @param[in]  gravity  The gravity number
   * @param[in]  force    Force update
   **/

void
subClientSetGravity(SubClient *c,
  int gravity,
  int force)
{
  DEAD(c);
  assert(c);

  if(force || c->gravity != gravity) ///< Check if update is required
    {
      int grav = 0, mode = 0;
      SubScreen *s = NULL;
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

      /* Hook: Gravity */
      if(subtle->hooks.gravity && 
        0 == subRubyCall(SUB_TYPE_HOOK, subtle->hooks.gravity, (void *)c))
        {
          subSharedLogDebug("Hook: name=gravity, client=%#lx, state=ignored\n", c->win);

          return;
        }

      grav = gravity & ~SUB_GRAVITY_MODES; ///< Strip modes

      /* Compute slot */
      s = SCREEN(subtle->screens->data[c->screens[subtle->vid]]);
      slot.height = s->geom.height / props[grav].cells_y;
      slot.width  = s->geom.width / props[grav].cells_x;
      slot.x      = s->geom.x + props[grav].grav_right * slot.width;
      slot.y      = s->geom.y + props[grav].grav_down * slot.height;

      /* Toggle between modes */
      if(gravity & SUB_GRAVITY_MODES || (c->geom.x == slot.x && c->geom.width == slot.width))
        {
          int height33 = s->geom.height / 3;
          int height66 = s->geom.height - height33;
          int comph    = abs(s->geom.height - 3 * height33); ///< Int rounding fix

          if(2 == props[grav].cells_y)
            {
              int y = s->geom.y + props[grav].grav_down * height33;

              if(gravity & SUB_GRAVITY_MODE66 || (!(gravity & SUB_GRAVITY_MODES) &&
                c->geom.height == slot.height && c->geom.y == slot.y)) ///< 33%
                {
                  slot.y      = y;
                  slot.height = height66;
                  mode        = SUB_GRAVITY_MODE66;
                }
              else if(gravity & SUB_GRAVITY_MODE33 || (!(gravity & SUB_GRAVITY_MODES) &&
                c->geom.height == height66 && c->geom.y == y)) ///< 66%
                {
                  slot.y      = s->geom.y + props[grav].grav_down * height66;
                  slot.height = height33;
                  mode        = SUB_GRAVITY_MODE33;

                }
            }
          else if(gravity & SUB_GRAVITY_MODE33 || (!(gravity & SUB_GRAVITY_MODES) &&
            c->geom.height == slot.height && c->geom.y == slot.y))
            {
              slot.y      = s->geom.y + height33;
              slot.height = height33 + comph;
              mode        = SUB_GRAVITY_MODE33;
            }
        }  

      /* Update client rect */
      c->geom                   = slot;
      c->gravity                = grav | mode;
      c->gravities[subtle->vid] = grav | mode;

      /* EWMH: Gravity */
      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_GRAVITY, (long *)&grav, 1);
    }
} /* }}} */

 /** subClientSetScreen {{{ 
  * @brief Set client screen for current view
  * @param[in]  c       A #SubClient
  * @param[in]  screen  The screen number
  * @param[in]  force   Force update
  **/

void
subClientSetScreen(SubClient *c,
  int screen,
  int force)
{
  assert(c);
  DEAD(c);

  if(force || c->screen != screen) ///< Check if update is required
    {
      c->screens[subtle->vid] = screen;
      c->screen               = screen;
      c->gravity              = -1; ///< Force update

      subClientSetSize(c); ///< Update size for screen

      /* EWMH: Screen */
      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_SCREEN, (long *)&c->screen, 1);
    }
} /* }}} */

  /** subClientSetSize {{{ 
   * @brief Set client size according to client hints
   * @param[in]  c  A #SubClient
   **/

void
subClientSetSize(SubClient *c)
{
  SubScreen *s = NULL;

  DEAD(c);
  assert(c);

  s = SCREEN(subtle->screens->data[c->screen]);

  /* Limit base width */
  if(c->base.width < c->minw) c->base.width = c->minw;
  if(c->base.width > c->maxw) c->base.width = c->maxw;
  if(c->base.x + c->base.width > s->base.x + s->base.width) 
    c->base.width = s->base.width - (s->base.x - c->base.x);

  /* Limit base height */
  if(c->base.height < c->minh) c->base.height = c->minh;
  if(c->base.height > c->maxh) c->base.height = c->maxh;
  if(c->base.y + c->base.height > s->base.y + s->base.height - subtle->bw)
    c->base.height = s->base.height - (s->base.y - c->base.y);

  /* Limit width */
  if(c->geom.width < c->minw) c->geom.width = c->minw;
  if(c->geom.width > c->maxw) c->geom.width = c->maxw;
  if(c->geom.x + c->geom.width > s->base.x + s->base.width) 
    c->geom.width = s->base.width - (s->base.x - c->geom.x);

  /* Limit height */
  if(c->geom.height < c->minh) c->geom.height = c->minh;
  if(c->geom.height > c->maxh) c->geom.height = c->maxh;
  if(c->geom.y + c->geom.height > s->base.y + s->base.height - subtle->bw)
    c->geom.height = s->base.height - (s->base.y - c->geom.y);

  /* Check incs */
  c->geom.width  -= c->geom.width % c->incw; 
  c->geom.height -= c->geom.height % c->inch;

  /* Check aspect ratios */
  if(c->minr && c->geom.height * c->minr > c->geom.width)
    c->geom.width = (int)(c->geom.height * c->minr);

  if(c->maxr && c->geom.height * c->maxr < c->geom.width)
    c->geom.width = (int)(c->geom.height * c->maxr); 
} /* }}} */

  /** subClientSetHints {{{
   * @brief Set client hints
   * @param[in]  c  A #SubClient
   **/

void
subClientSetHints(SubClient *c)
{
  long supplied = 0;
  XSizeHints *size = NULL;
  SubScreen *s = NULL;

  DEAD(c);
  assert(c);

  if(!(size = XAllocSizeHints()))
    subSharedLogError("Can't alloc memory. Exhausted?\n");

  s = SCREEN(subtle->screens->data[c->screen]);

  /* Default values {{{ */
  c->minw   = MINW;
  c->minh   = MINH;
  c->maxw   = s->base.width - 2 * subtle->bw;
  c->maxh   = s->base.height - subtle->th - 2 * subtle->bw;
  c->minr   = 0.0f;
  c->maxr   = 0.0f;
  c->incw   = 1;
  c->inch   = 1; /* }}} */

  /* Size hints */
  if(XGetWMNormalHints(subtle->dpy, c->win, size, &supplied))
    {
      if(size->flags & PMinSize) ///< Program min size
        {
          if(size->min_width)  c->minw = MIN(MINW, size->min_width);
          if(size->min_height) c->minh = MIN(MINH, size->min_height);
        }

      if(size->flags & PMaxSize) ///< Program max size
        {
          if(size->max_width)
            c->maxw = size->max_width > s->base.width ? 
              s->base.width : size->max_width;
          if(size->max_height)
            c->maxh = size->max_height > s->base.height - subtle->th ? 
              s->base.height - subtle->th : size->max_height;
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
    }

  XFree(size);

  subSharedLogDebug("Normal hints: x=%d, y=%d, width=%d, height=%d, minw=%d, minh=%d, " \
    "maxw=%d, maxh=%d, minr=%f, maxr=%f\n",
    c->geom.x, c->geom.y, c->geom.width, c->geom.height, c->minw, c->minh, c->maxw,
    c->maxh, c->minr, c->maxr);
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

  DEAD(c);
  assert(c);

  /* Get strut property */
  if((strut = (long *)subSharedPropertyGet(c->win, XA_CARDINAL, 
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

 /** subClientSetCaption {{{
  * @brief Set caption width
  * @param[in]  c  A #SubClient
  **/

void
subClientSetCaption(SubClient *c)
{
  /* Update panel width */
  subtle->panels.caption.width = XTextWidth(subtle->xfs, c->caption, 
    strlen(c->caption) + (c->flags & (SUB_MODE_STICK|SUB_MODE_FLOAT) ? 1 : 0)) + 6;
  XResizeWindow(subtle->dpy, subtle->panels.caption.win, 
    subtle->panels.caption.width, subtle->th);
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
  int flags = 0;

  DEAD(c);
  assert(c);

  if(c->flags & type) ///< Unset flags
    {
      c->flags &= ~type;

      if(type & SUB_MODE_FULL)
        XSetWindowBorderWidth(subtle->dpy, c->win, subtle->bw);

      if(type & SUB_MODE_FLOAT) 
        {
          c->base    = c->geom;
          c->gravity = -1; ///< Updating gravity
        }

      if(type & SUB_MODE_STICK) subViewConfigure(subtle->view);
    }
  else ///< Set flags
    {
      c->flags |= type;

      if(type & SUB_MODE_FLOAT)
        {
          SubScreen *s = SCREEN(subtle->screens->data[c->screen]);

          c->geom = c->base;

          subClientSetSize(c); ///< Sanitize

          if(s->base.x >= c->geom.x || s->base.y >= c->geom.y) ///< Center
            {
              c->geom.x = s->base.x + (s->base.width - c->geom.width) / 2;
              c->geom.y = s->base.y + ((s->base.height - subtle->th) - c->geom.height) / 2;
            }
        }

      if(type & SUB_MODE_FULL)
        XSetWindowBorderWidth(subtle->dpy, c->win, 0);
    }

  subClientConfigure(c);

  /* Translate flags */
  if(c->flags & SUB_MODE_FULL)  flags |= SUB_EWMH_FULL;
  if(c->flags & SUB_MODE_FLOAT) flags |= SUB_EWMH_FLOAT;
  if(c->flags & SUB_MODE_STICK) flags |= SUB_EWMH_STICK;

  /* EWMH: Flags */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_FLAGS, (long *)&flags, 1);

  if(VISIBLE(subtle->view, c)) ///< Check visibility first
    {
      subClientFocus(c);
      subPanelUpdate();
      subPanelRender();
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
  XSelectInput(subtle->dpy, c->win, NoEventMask);
  XDeleteContext(subtle->dpy, c->win, CLIENTID);
  XUnmapWindow(subtle->dpy, c->win);

  /* Focus */
  if(subtle->windows.focus == c->win)
    {
      subtle->windows.focus        = 0;
      subtle->panels.caption.width = 0;
      subPanelUpdate();
      subPanelRender();
    }

  /* Close window */
  if(close && !(c->flags & SUB_CLIENT_DEAD))
    {
      if(c->flags & SUB_CLIENT_CLOSE) ///< Honor window preferences
        {
          subEwmhMessage(c->win, c->win, SUB_EWMH_WM_PROTOCOLS,
            subEwmhGet(SUB_EWMH_WM_DELETE_WINDOW), CurrentTime, 0, 0, 0);
        }
      else XKillClient(subtle->dpy, c->win);
    }

  subSharedFocus(); ///< Focus

  if(c->gravities) free(c->gravities);
  if(c->screens)   free(c->screens);
  if(c->caption)   XFree(c->caption);
  if(c->name)      XFree(c->name);
  if(c->klass)     XFree(c->klass);
  free(c);

  subSharedLogDebug("kill=client\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
