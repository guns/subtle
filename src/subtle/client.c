
 /**
  * @package subtle
  *
  * @file Client functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <X11/Xatom.h>
#include "subtle.h"

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
  SubScreen *s = NULL;

  assert(c);

  s = SCREEN(subtle->screens->data[c->screen]);

  /* Snap to screen edges */
  if(s->geom.x + subtle->snap > c->geom.x)
    c->geom.x = s->geom.x + subtle->bw;
  else if(c->geom.x > (s->geom.x + s->geom.width + subtle->bw - c->geom.width - subtle->snap))
    c->geom.x = s->geom.x + s->geom.width - c->geom.width - subtle->bw;

  if(s->geom.y + subtle->snap > c->geom.y)
    c->geom.y = s->geom.y + subtle->bw;
  else if(c->geom.y > (s->geom.y + s->geom.height + subtle->bw - c->geom.height - subtle->snap))
    c->geom.y = s->geom.y + s->geom.height - c->geom.height - subtle->bw;
} /* }}} */

/* ClientGravity {{{ */
int
ClientGravity(void)
{
  int grav = 5;
  SubClient *c = NULL;

  /* Default gravity */
  if(0 == subtle->gravity)
    {
      if((c = CLIENT(subSubtleFind(subtle->windows.focus, CLIENTID))))
        grav = c->gravity; ///< Copy gravity
    }
  else grav = subtle->gravity; ///< Set default

  return grav;
} /* }}} */

 /** subClientNew {{{
  * @brief Create new client
  * @param[in]  win  Client window
  * @return Returns a new #SubClient or \p NULL
  **/

SubClient *
subClientNew(Window win)
{
  int i, grav = 0, flags = 0;
  long vid = 0;
  XWindowAttributes attrs;
  XSetWindowAttributes sattrs;
  SubClient *c = NULL;

  assert(win);

  /* Create new client */
  c = CLIENT(subSharedMemoryAlloc(1, sizeof(SubClient)));
  c->gravities = (int *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(int));
  c->screens   = (int *)subSharedMemoryAlloc(subtle->views->ndata, sizeof(int));
  c->flags     = SUB_TYPE_CLIENT;
  c->gravity   = -1; ///< Force update
  c->win       = win;

  /* Window attributes */
  XGetWindowAttributes(subtle->dpy, c->win, &attrs);
  c->cmap        = attrs.colormap;
  c->geom.x      = attrs.x;
  c->geom.y      = attrs.y;
  c->geom.width  = MAX(MINW, attrs.width);
  c->geom.height = MAX(MINH, attrs.height);

  /* Init gravities and screens */
  grav = ClientGravity();
  for(i = 0; i < subtle->views->ndata; i++)
    {
      c->gravities[i] = grav;
      c->screens[i]   = 0;
    }

   /* Fetch name, instance and class */
  subSharedPropertyClass(subtle->dpy, c->win, &c->instance, &c->klass);
  subSharedPropertyName(subtle->dpy, c->win, &c->name, c->klass);

  /* X properties */
  sattrs.border_pixel = subtle->colors.bo_normal;
  sattrs.event_mask   = EVENTMASK;
  XChangeWindowAttributes(subtle->dpy, c->win, CWBorderPixel|CWEventMask, &sattrs);
  XSetWindowBorderWidth(subtle->dpy, c->win, subtle->bw);

  XAddToSaveSet(subtle->dpy, c->win);
  XSaveContext(subtle->dpy, c->win, CLIENTID, (void *)c);

  /* Update client */
  subEwmhSetWMState(c->win, WithdrawnState);
  subClientSetType(c, &flags);
  subClientSetSizeHints(c, &flags);
  subClientSetProtocols(c);
  subClientSetStrut(c);
  subClientSetTags(c);
  subClientSetWMHints(c, &flags);
  subClientSetState(c, &flags);
  subClientSetTransient(c, &flags);

  /* Update according to flags */
  subClientToggle(c, (~c->flags & flags)); ///< Toggle flags
  if(c->flags & SUB_CLIENT_CENTER) subClientCenter(c);

  /* EWMH: Gravity, screen and desktop */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_GRAVITY, (long *)&c->gravity, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_SCREEN, (long *)&c->screen, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

  subSharedLogDebug("new=client, name=%s, instance=%s, klass=%s, win=%#lx\n",
    c->name, c->instance, c->klass, win);

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

  if(c->flags & SUB_MODE_FULL) ///< Get fullscreen size of screen
    r = SCREEN(subtle->screens->data[-1 != c->screen ? c->screen : 0])->base;

  /* Tell client new geometry */
  ev.type              = ConfigureNotify;
  ev.event             = c->win;
  ev.window            = c->win;
  ev.x                 = r.x;
  ev.y                 = r.y;
  ev.width             = r.width;
  ev.height            = r.height;
  ev.border_width      = subtle->bw;
  ev.above             = None;
  ev.override_redirect = False;

  XMoveResizeWindow(subtle->dpy, c->win, r.x, r.y, r.width, r.height);
  XSendEvent(subtle->dpy, c->win, False, StructureNotifyMask, (XEvent *)&ev);

  subSharedLogDebug("Configure: type=client, win=%#lx, name=%s, state=%c, x=%03d, y=%03d, width=%03d, height=%03d\n",
    c->win, c->klass, c->flags & SUB_MODE_FLOAT ? 'f' : c->flags & SUB_MODE_FULL ? 'u' : 'n',
    c->geom.x, c->geom.y, c->geom.width, c->geom.height);

  /* Hook: Create */
  subHookCall(SUB_HOOK_CLIENT_CONFIGURE, (void *)c);
} /* }}} */

 /** subClientRender {{{
  * @brief Render client and redraw titlebar and borders
  * @param[in]  c  A #SubClient
  **/

void
subClientRender(SubClient *c)
{
  int x = 0;
  char buf[50] = { 0 };
  long fg = 0, bg = 0;

  DEAD(c);
  assert(c);

  /* Title modes */
  if(c->flags & SUB_MODE_FLOAT)
    {
      snprintf(buf + x, sizeof(buf), "%c", '^');
      x++;
    }
  if(c->flags & SUB_MODE_STICK)
    {
      snprintf(buf + x, sizeof(buf), "%c", '*');
      x++;
    }

  snprintf(buf + x, sizeof(buf), "%s", c->name);

  /* Select color pair */
  if(c->flags & SUB_MODE_URGENT)
    {
      fg = subtle->colors.fg_urgent;
      bg = subtle->colors.bg_urgent;
    }
  else
    {
      fg = subtle->colors.fg_focus;
      bg = subtle->colors.bg_focus;
    }

  /* Set window background and border */
  XSetWindowBackground(subtle->dpy, subtle->windows.title.win, bg);
  XClearWindow(subtle->dpy, subtle->windows.title.win);

  if(!(c->flags & SUB_CLIENT_IMMOBILE)) ///< Exclude immobile windows
    {
      XSetWindowBorder(subtle->dpy, c->win, subtle->windows.focus == c->win ?
        subtle->colors.bo_focus : subtle->colors.bo_normal);
    }

  subSharedTextDraw(subtle->dpy, subtle->gcs.font, subtle->font,
    subtle->windows.title.win, 3, subtle->font->y, fg, bg, buf);
} /* }}} */

 /** subClientCompare {{{
  * @brief Compare two stacking level of clients
  * @param[in]  a  A #SubClient
  * @param[in]  b  A #SubClient
  * @return Returns the result of the comparison of both clients
  * @retval  -1  a is smaller
  * @retval  0   a and b are equal
  * @retval  1   a is greater
  **/

int
subClientCompare(const void *a,
  const void *b)
{
  int ret = 0;
  SubClient *c1 = *(SubClient **)a, *c2 = *(SubClient **)b;

  assert(a && b);

  /* Check flags */
  if((c1->flags | c2->flags) & SUB_MODE_FULL)
    {
      if(c1->flags & SUB_MODE_FULL) ret = 1;
      if(c2->flags & SUB_MODE_FULL) ret = -1;
    }
  else if((c1->flags | c2->flags) & SUB_CLIENT_IMMOBILE)
    {
      if(c1->flags & SUB_CLIENT_IMMOBILE) ret = -1;
      if(c2->flags & SUB_CLIENT_IMMOBILE) ret = 1;
    }

  return ret;
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

  /* Check client input focus type */
  if(!(c->flags & SUB_CLIENT_INPUT) && c->flags & SUB_CLIENT_FOCUS)
    {
      subEwmhMessage(c->win, c->win, SUB_EWMH_WM_PROTOCOLS,
        subEwmhGet(SUB_EWMH_WM_TAKE_FOCUS), CurrentTime, 0, 0, 0);
    }
  else XSetInputFocus(subtle->dpy, c->win, RevertToNone, CurrentTime);

  subtle->sid = c->screen; ///< Store screen

  subSharedLogDebug("Focus: type=client, win=%#lx, input=%d, focus=%d\n", c->win,
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
  unsigned int dummy = 0;
  int loop = True, left = False, wx = 0, wy = 0, ww = 0, wh = 0, rx = 0, ry = 0;
  short *dirx = NULL, *diry = NULL;
  Cursor cursor;

  DEAD(c);
  assert(c);

  /* Init {{{ */
  XQueryPointer(subtle->dpy, c->win, &win, &win, &rx, &ry, &wx, &wy, &dummy);
  c->geom.x = rx - wx;
  c->geom.y = ry - wy;
  ww        = c->geom.width;
  wh        = c->geom.height;

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

  /* Prevent resizing of fixed size clients before grabbing */
  if(mode == SUB_DRAG_RESIZE && c->flags & SUB_CLIENT_FIXED) return;

  if(XGrabPointer(subtle->dpy, c->win, True, GRABMASK, GrabModeAsync,
    GrabModeAsync, None, cursor, CurrentTime)) return;

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

                subClientResize(c);
                ClientMask(c);
              }
            break; /* }}} */
          case MotionNotify: /* {{{ */
            if(mode & (SUB_DRAG_MOVE|SUB_DRAG_RESIZE))
              {
                int check = 0;
                ClientMask(c);

                /* Calculate selection rect */
                switch(mode)
                  {
                    case SUB_DRAG_MOVE:
                      c->geom.x = (rx - wx) - (rx - ev.xmotion.x_root);
                      c->geom.y = (ry - wy) - (ry - ev.xmotion.y_root);

                      ClientSnap(c); ///< Snap border
                      break;
                    case SUB_DRAG_RESIZE:
                      if(left) ///< Drag left
                        {
                          if(0 < (rx - wx) - (rx - ev.xmotion.x_root)) ///< Check edge
                            {
                              /* Calculate width and x */
                              check         = ww + (rx - ev.xmotion.x_root);
                              c->geom.width = check > c->minw ? check : c->minw;
                              c->geom.width -= (c->geom.width % c->incw);
                              c->geom.x     = (rx - wx) + ww - c->geom.width;
                            }
                        }
                      else c->geom.width = ww - (rx - ev.xmotion.x_root); ///< Drag right

                      c->geom.height = wh - (ry - ev.xmotion.y_root);
                      subClientResize(c);

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
      /* Subtract borer */
      c->geom.x -= subtle->bw;
      c->geom.y -= subtle->bw;

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
          c->gravities[subtle->views->ndata - 1] = ClientGravity();
          c->screens[subtle->views->ndata - 1]   = 0;
        }
    }
} /* }}} */

  /** subClientResize {{{
   * @brief Set client size according to client hints
   * @param[in]  c    A #SubClient
   **/

void
subClientResize(SubClient *c)
{
  DEAD(c);
  assert(c);

  /* Check sizes */
  if(!(c->flags & SUB_MODE_NONRESIZE) &&
      (subtle->flags & SUB_SUBTLE_RESIZE ||
      c->flags & (SUB_MODE_FLOAT|SUB_MODE_RESIZE)))
    {
      /* Limit width */
      if(c->geom.width < c->minw)  c->geom.width  = c->minw;
      if(c->geom.width > c->maxw)  c->geom.width  = c->maxw;

      /* Limit height */
      if(c->geom.height < c->minh) c->geom.height = c->minh;
      if(c->geom.height > c->maxh) c->geom.height = c->maxh;

      /* Check incs */
      c->geom.width  -= WIDTH(c) % c->incw;
      c->geom.height -= HEIGHT(c) % c->inch;

      /* Check aspect ratios */
      if(c->minr && c->geom.height * c->minr > c->geom.width)
        c->geom.width = (int)(c->geom.height * c->minr);

      if(c->maxr && c->geom.height * c->maxr < c->geom.width)
        c->geom.width = (int)(c->geom.height * c->maxr);
    }

  /* Fit sizes */
  if(!(c->flags & SUB_CLIENT_IMMOBILE))
    {
      SubScreen *s = SCREEN(subtle->screens->data[c->screen]);

      subScreenFit(s, &c->geom);
    }
} /* }}} */

  /** subClientCenter {{{
   * @brief Center client
   * @param[in]  c  A #SubClient
   **/

void
subClientCenter(SubClient *c)
{
  DEAD(c);
  assert(c);

  if(!(c->flags & SUB_CLIENT_IMMOBILE))
    {
      SubScreen *s = SCREEN(subtle->screens->data[c->screen]);

      c->geom.x = s->geom.x +
        (s->geom.width - c->geom.width - 2 * subtle->bw) / 2;
      c->geom.y = s->geom.y +
        (s->geom.height - c->geom.height - 2 * subtle->bw) / 2;
    }
} /* }}} */

 /** subClientTag {{{
  * @brief Set tag properties to client
  * @param[in]  c    A #SubClient
  * @param[in]  tag  Tag id
  * @return Return changed flags
  **/

int
subClientTag(SubClient *c,
  int tag)
{
  int flags = 0;

  assert(c);

  /* Update flags and tags */
  if(c && !(c->flags & SUB_CLIENT_DEAD))
    {
      if(0 <= tag && subtle->tags->ndata > tag)
        {
          int i;
          SubTag *t = TAG(subtle->tags->data[tag]);

          /* Collect flags and tags */
          flags    |= (t->flags & MODES_ALL);
          c->flags |= (t->flags & MODES_NONE);
          c->tags  |= (1L << (tag + 1));

          /* Set size and enable float */
          if(t->flags & SUB_TAG_GEOMETRY && !(c->flags & SUB_MODE_NONFLOAT))
            {
              flags    |= (SUB_MODE_FLOAT|SUB_MODE_NONRESIZE); ///< Disable size checks
              c->flags &= ~SUB_CLIENT_CENTER; ///< Disable center
              c->geom   = t->geometry;
            }

          /* Set gravity and screens for matching views */
          for(i = 0; i < subtle->views->ndata; i++)
            {
              SubView *v = VIEW(subtle->views->data[i]);

              /* Match only views with this tag */
              if(v->tags & (1L << (tag + 1)) || t->flags & SUB_MODE_STICK)
                {
                  if(t->flags & SUB_TAG_GRAVITY) c->gravities[i] = t->gravity;
                  if(t->flags & SUB_TAG_SCREEN)  c->screens[i] = t->screen;
                }
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
  char *role = NULL;

  DEAD(c);
  assert(c);

  c->tags = 0; ///< Reset tags

  /* Get window role */
  role = subSharedPropertyGet(subtle->dpy, c->win, XA_STRING,
    subEwmhGet(SUB_EWMH_WM_WINDOW_ROLE), NULL);

  /* Check matching tags */
  for(i = 0; (c->name || c->klass) && i < subtle->tags->ndata; i++)
    {
      SubTag *t = TAG(subtle->tags->data[i]);

      /* Check if tag matches client */
      if(t->preg && ((t->flags & SUB_TAG_MATCH_NAME && c->name &&
          subSharedRegexMatch(t->preg, c->name))||
          (t->flags & SUB_TAG_MATCH_INSTANCE && c->instance &&
          subSharedRegexMatch(t->preg, c->instance)) ||
          (t->flags & SUB_TAG_MATCH_CLASS && c->klass &&
          subSharedRegexMatch(t->preg, c->klass)) ||
          (t->flags & SUB_TAG_MATCH_ROLE && role &&
          subSharedRegexMatch(t->preg, role))))
        flags |= subClientTag(c, i);
    }

  if(role) free(role);

  subViewDynamic(); ///< Dynamic views

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
   * @param[in]  screen   The screen number
   * @param[in]  force    Force update
   **/

void
subClientSetGravity(SubClient *c,
  int gravity,
  int screen,
  int force)
{
  DEAD(c);
  assert(c);

  if(force || c->gravity != gravity || c->screen != screen) ///< Check if update is required
    {
      SubScreen *s = SCREEN(subtle->screens->data[-1 != screen ?
        screen : c->screen]);

      if(c->flags & SUB_MODE_FLOAT)
        {
          if(-1 != screen)
            {
              SubScreen *s1 = NULL, *s2 = NULL;

              s1 = SCREEN(subtle->screens->data[-1 != c->screen ? c->screen : 0]);
              s2 = SCREEN(subtle->screens->data[screen]);

              /* Update screen offsets */
              c->geom.x = c->geom.x - s1->geom.x + s2->geom.x;
              c->geom.y = c->geom.y - s1->geom.y + s2->geom.y;
            }
        }
      else if(!(c->flags & SUB_CLIENT_IMMOBILE))
        {
          SubGravity *g = GRAVITY(subtle->gravities->data[-1 != gravity ?
            gravity : c->gravity]);

          /* Calculate slot */
          c->geom.width  = (s->geom.width * g->geom.width / 100);
          c->geom.height = (s->geom.height * g->geom.height / 100);
          c->geom.x      = s->geom.x + ((s->geom.width - c->geom.width) *
            g->geom.x / 100);
          c->geom.y      = s->geom.y + ((s->geom.height - c->geom.height) *
            g->geom.y / 100);

          /* Substract border width */
          c->geom.width  -= 2 * subtle->bw;
          c->geom.height -= 2 * subtle->bw;
        }

      /* Update client */
      if(-1 != screen)  c->screen  = c->screens[subtle->vid]   = screen;
      if(-1 != gravity) c->gravity = c->gravities[subtle->vid] = gravity;

      subClientResize(c);

      /* EWMH: Gravity and screen */
      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_GRAVITY,
        (long *)&c->gravity, 1);
      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_SCREEN,
        (long *)&c->screen, 1);

      /* Hook: Gravity */
      subHookCall(SUB_HOOK_CLIENT_GRAVITY, (void *)c);
    }
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
  if((strut = (long *)subSharedPropertyGet(subtle->dpy, c->win, XA_CARDINAL,
      subEwmhGet(SUB_EWMH_NET_WM_STRUT), &size)))
    {
      if(4 * sizeof(long) == size) ///< Only complete struts
        {
          subtle->strut.x      = MAX(subtle->strut.x,      strut[0]); ///< Left
          subtle->strut.y      = MAX(subtle->strut.y,      strut[1]); ///< Right
          subtle->strut.width  = MAX(subtle->strut.width,  strut[2]); ///< Top
          subtle->strut.height = MAX(subtle->strut.height, strut[3]); ///< Bottom

          /* Update screen and clients */
          subScreenConfigure();
          subViewConfigure(CURVIEW, True);

          subSharedLogDebug("Strut: x=%ld, y=%d, width=%d, height=%d\n", subtle->strut.x,
            subtle->strut.y, subtle->strut.width, subtle->strut.height);
        }

      XFree(strut);
    }
} /* }}} */

 /** subClientSetName {{{
  * @brief Set title width
  * @param[in]  c  A #SubClient
  **/

void
subClientSetName(SubClient *c)
{
  int len = 0;

  assert(c);
  DEAD(c);

  /* Check for immobile clients */
  if(!(c->flags & SUB_CLIENT_IMMOBILE))
    {
      len = strlen(c->name);

      /* Title modes */
      if(c->flags & SUB_MODE_FLOAT) len++;
      if(c->flags & SUB_MODE_STICK) len++;

      /* Update panel width */
      subtle->windows.title.width = subSharedTextWidth(subtle->font, c->name,
        50 >= len ? len : 50, NULL, NULL, True) + 6 + 2 * subtle->pbw; ///< Font offset and panel border
      XResizeWindow(subtle->dpy, subtle->windows.title.win,
        subtle->windows.title.width, subtle->th - 2 * subtle->pbw);
    }
  else subtle->windows.title.width = 0;
} /* }}} */

 /** subClientSetProtocols {{{
  * @brief Set client protocols
  * @param[in]  c  A #SubClient
  **/

void
subClientSetProtocols(SubClient *c)
{
  int i, n = 0;
  Atom *protos = NULL;

  assert(c);

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
} /* }}} */

  /** subClientSetSizeHints {{{
   * @brief Set client size hints
   * @param[in]  c  A #SubClient
   **/

void
subClientSetSizeHints(SubClient *c,
  int *flags)
{
  long supplied = 0;
  XSizeHints *size = NULL;
  SubScreen *s = NULL;

  DEAD(c);
  assert(c);

  if(!(size = XAllocSizeHints()))
    {
      subSharedLogError("Can't alloc memory. Exhausted?\n");
      abort();
    }

  s = SCREEN(subtle->screens->data[0]); ///< Assume first screen

  /* Default values {{{ */
  c->minw   = MINW;
  c->minh   = MINH;
  c->maxw   = s->base.width - 2 * subtle->bw;
  c->maxh   = s->base.height - subtle->th - 2 * subtle->bw;
  c->minr   = 0.0f;
  c->maxr   = 0.0f;
  c->incw   = 1;
  c->inch   = 1; /* }}} */

  /* Size hints - no idea why it's called normal hints */
  if(XGetWMNormalHints(subtle->dpy, c->win, size, &supplied))
    {
      if(size->flags & PMinSize) ///< Program min size
        {
          /* Limit min size to screen size if larger */
          if(size->min_width)
            c->minw = c->minw > s->geom.width ? s->geom.width :
              MIN(MINW, size->min_width);
          if(size->min_height)
            c->minh = c->minh > s->geom.height ? s->geom.height :
              MIN(MINH, size->min_height);
        }

      if(size->flags & PMaxSize) ///< Program max size
        {
          /* Limit max size to screen size if larger */
          if(size->max_width)
            c->maxw = size->max_width > s->geom.width ?
              s->geom.width : size->max_width;
          if(size->max_height)
            c->maxh = size->max_height > s->geom.height - subtle->th ?
              s->geom.height - subtle->th : size->max_height;
        }

      /* Floating on equal min and max sizes */
      if(size->flags & PMinSize && size->flags & PMaxSize)
        {
          if(size->min_width == size->max_width &&
              size->min_height == size->max_height &&
              !(c->flags & SUB_CLIENT_IMMOBILE))
            {
              *flags   |= SUB_MODE_FLOAT;
              c->flags |= (SUB_CLIENT_FIXED|SUB_CLIENT_CENTER);
            }
        }

      if(size->flags & PAspect) ///< Aspect
        {
          if(size->min_aspect.y)
            c->minr = (float)size->min_aspect.x / size->min_aspect.y;
          if(size->max_aspect.y)
            c->maxr = (float)size->max_aspect.x / size->max_aspect.y;
        }

      if(size->flags & PResizeInc) ///< Resize inc
        {
          if(size->width_inc)  c->incw = size->width_inc;
          if(size->height_inc) c->inch = size->height_inc;
        }

      /* Check for specific position */
      if(!(c->flags & SUB_MODE_NONRESIZE) &&
          (subtle->flags & SUB_SUBTLE_RESIZE ||
          c->flags & (SUB_MODE_FLOAT|SUB_MODE_RESIZE)))
        {
          if(size->flags & (USSize|PSize)) ///< User/program size
            {
              c->geom.width  = size->width;
              c->geom.height = size->height;
            }

          if(size->flags & (USPosition|PPosition)) ///< User/program position
            {
              c->geom.x = size->x;
              c->geom.y = size->y;
            }
        }
    }

  XFree(size);

  subSharedLogDebug("Normal hints: x=%d, y=%d, width=%d, height=%d, minw=%d, minh=%d, " \
    "maxw=%d, maxh=%d, minr=%f, maxr=%f\n",
    c->geom.x, c->geom.y, c->geom.width, c->geom.height, c->minw,
    c->minh, c->maxw, c->maxh, c->minr, c->maxr);
} /* }}} */

  /** subClientSetWMHints {{{
   * @brief Set client WM hints
   * @param[in]  c      A #SubClient
   * @param[in]  flags  Mode flags
   **/

void
subClientSetWMHints(SubClient *c,
  int *flags)
{
  XWMHints *hints = NULL;

  assert(c && flags);

  /* Window manager hints */
  if((hints = XGetWMHints(subtle->dpy, c->win)))
    {
      /* Handle urgency */
      if(!(c->flags & SUB_MODE_NONURGENT))
        {
          /* Set urgency or remove urgency after losing focus */
          if(hints->flags & XUrgencyHint)     *flags |= SUB_MODE_URGENT;
          else if(c->flags & SUB_MODE_URGENT) *flags |= SUB_MODE_URGENT_FOCUS;
        }

      if(hints->flags & InputHint && hints->input)
        c->flags |= SUB_CLIENT_INPUT;

      XFree(hints);
    }
} /* }}} */

  /** subClientSetState {{{
   * @brief Set client WM state
   * @param[in]  c      A #SubClient
   * @param[in]  flags  Mode flags
   **/

void
subClientSetState(SubClient *c,
  int *flags)
{
  int i;
  unsigned long size = 0;
  Atom *states = NULL;

  assert(c);

  /* Window state */
  if((states = (Atom *)subSharedPropertyGet(subtle->dpy, c->win, XA_ATOM,
      subEwmhGet(SUB_EWMH_NET_WM_STATE), &size)))
    {
      for(i = 0; i < size / sizeof(Atom); i++)
        {
          switch(subEwmhFind(states[i]))
            {
              case SUB_EWMH_NET_WM_STATE_FULLSCREEN: *flags |= SUB_MODE_FULL;   break;
              case SUB_EWMH_NET_WM_STATE_ABOVE:      *flags |= SUB_MODE_FLOAT;  break;
              case SUB_EWMH_NET_WM_STATE_STICKY:     *flags |= SUB_MODE_STICK;  break;
              case SUB_EWMH_NET_WM_STATE_MODAL:      *flags |= SUB_MODE_URGENT; break;
              default: break;
            }
        }

      XFree(states);
    }
} /* }}} */

 /** subClientSetTransient {{{
  * @brief Set client transient
  * @param[in]  c      A #SubClient
  * @param[in]  flags  Mode flags
  **/

void
subClientSetTransient(SubClient *c,
  int *flags)
{
  int i;
  Window trans = 0;

  assert(c && flags);

  /* Check for transient windows */
  if(XGetTransientForHint(subtle->dpy, c->win, &trans))
    {
      SubClient *k = NULL;

      /* Check if transient windows should be urgent */
      *flags |= subtle->flags & SUB_SUBTLE_URGENT ?
        SUB_MODE_FLOAT|SUB_MODE_URGENT : SUB_MODE_FLOAT;

      if((k = CLIENT(subSubtleFind(trans, CLIENTID))))
        {
          /* Copy stick flag, tags and screens */
          c->flags  |= (k->flags & SUB_MODE_STICK);
          c->tags   |= k->tags;
          c->screen |= k->screen;

          for(i = 0; i < subtle->views->ndata; i++)
            c->screens[i] = k->screens[i];
        }
     }
} /* }}} */

 /** subClientSetType {{{
  * @brief Set client type
  * @param[in]  c      A #SubClient
  * @param[in]  flags  Mode flags
  **/

void
subClientSetType(SubClient *c,
  int *flags)
{
  int i;
  unsigned long size = 0;
  Atom *types = NULL;

  assert(c);

  /* Window type */
  if((types = (Atom *)subSharedPropertyGet(subtle->dpy, c->win, XA_ATOM,
      subEwmhGet(SUB_EWMH_NET_WM_WINDOW_TYPE), &size)))
    {
      int id = 0;

      for(i = 0; i < size / sizeof(Atom); i++)
        {
          switch((id = subEwmhFind(types[i])))
            {
              case SUB_EWMH_NET_WM_WINDOW_TYPE_DIALOG:
                *flags   |= SUB_MODE_FLOAT;
                c->flags |= SUB_CLIENT_CENTER;
                break;
              case SUB_EWMH_NET_WM_WINDOW_TYPE_DOCK:
              case SUB_EWMH_NET_WM_WINDOW_TYPE_DESKTOP:
                *flags   |= SUB_MODE_STICK;
                c->flags |= SUB_CLIENT_IMMOBILE;

                /* Special treatment */
                if(SUB_EWMH_NET_WM_WINDOW_TYPE_DESKTOP == id)
                  {
                    SubScreen *s = SCREEN(subtle->screens->data[c->screen]);

                    c->geom = s->base;

                    /* Add panel heights */
                    if(subtle->flags & SUB_SUBTLE_PANEL1)
                      {
                        c->geom.y      += subtle->th;
                        c->geom.height -= subtle->th;
                      }

                    if(subtle->flags & SUB_SUBTLE_PANEL2)
                      c->geom.height -= subtle->th;

                    XLowerWindow(subtle->dpy, c->win);
                  }
                else XRaiseWindow(subtle->dpy, c->win);

                XSetWindowBorderWidth(subtle->dpy, c->win, 0);
                break;
              default: break;
            }
        }

      XFree(types);
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
  int flags = 0;

  DEAD(c);
  assert(c);

  /* Remove flags */
  if(type & SUB_MODE_FULL   && c->flags & SUB_MODE_NONFULL)
    type &= ~SUB_MODE_FULL;
  if(type & SUB_MODE_FLOAT  && c->flags & SUB_MODE_NONFLOAT)
    type &= ~SUB_MODE_FLOAT;
  if(type & SUB_MODE_STICK  && c->flags & SUB_MODE_NONSTICK)
    type &= ~SUB_MODE_STICK;
  if(type & SUB_MODE_URGENT && c->flags & SUB_MODE_NONURGENT)
    type &= ~SUB_MODE_URGENT;

  if(c->flags & type) ///< Unset flags
    {
      c->flags &= ~type;

      if(type & SUB_MODE_FULL)
        XSetWindowBorderWidth(subtle->dpy, c->win, subtle->bw);

      if(type & SUB_MODE_FLOAT)
        c->gravity = -1; ///< Updating gravity

      if(type & SUB_MODE_STICK) subViewConfigure(CURVIEW, False);
    }
  else ///< Set flags
    {
      /* Need to be done before setting flag */
      if(type & SUB_MODE_STICK)
        {
          int i;

          /* Set gravity and screen for other views */
          for(i = 0; i < subtle->views->ndata; i++)
            {
              SubView *v = VIEW(subtle->views->data[i]);

              if(!(VISIBLE(v, c)))
                {
                  if(-1 != c->gravity) c->gravities[i] = c->gravity;
                  //if(-1 != c->screen)  c->screens[i]   = c->screen;
                }
            }
        }

      c->flags |= type;

      if(type & SUB_MODE_FLOAT) subClientResize(c); ///< Sanitize
      if(type & SUB_MODE_FULL)
        {
          XSetWindowBorderWidth(subtle->dpy, c->win, 0);
          subClientResize(c); ///< Sanitize
        }

      if(type & SUB_MODE_URGENT && VISIBLE(CURVIEW, c))
        c->flags |= SUB_CLIENT_WARP;
    }

  subClientConfigure(c);

  /* Sort for keeping stacking order */
  if(type & SUB_MODE_FULL) subArraySort(subtle->clients, subClientCompare);

  /* Translate flags */
  if(c->flags & SUB_MODE_FULL)  flags |= SUB_EWMH_FULL;
  if(c->flags & SUB_MODE_FLOAT) flags |= SUB_EWMH_FLOAT;
  if(c->flags & SUB_MODE_STICK) flags |= SUB_EWMH_STICK;

  /* EWMH: Flags */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_FLAGS, (long *)&flags, 1);
} /* }}} */

 /** subClientPublish {{{
  * @brief Publish clients
  **/

void
subClientPublish(void)
{
  int i;
  Window *wins = (Window *)subSharedMemoryAlloc(subtle->clients->ndata,
    sizeof(Window));

  for(i = 0; i < subtle->clients->ndata; i++)
    wins[i] = CLIENT(subtle->clients->data[i])->win;

  /* EWMH: Client list and client list stacking */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST, wins,
    subtle->clients->ndata);
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST_STACKING, wins,
    subtle->clients->ndata);

  XSync(subtle->dpy, False); ///< Sync all changes

  subSharedLogDebug("publish=client, clients=%d\n", subtle->clients->ndata);

  free(wins);
} /* }}} */

 /** subClientKill {{{
  * @brief Send interested clients the  signal and/or kill it
  * @param[in]  c        A #SubClient
  * @param[in]  destroy  Destroy window
  **/

void
subClientKill(SubClient *c,
  int destroy)
{
  assert(c);

    /* Hook: Create */
  subHookCall(SUB_HOOK_CLIENT_KILL, (void *)c);

  /* Focus */
  if(subtle->windows.focus == c->win)
    {
      subtle->windows.focus       = 0;
      subtle->windows.title.width = 0;
      subPanelUpdate();
      subPanelRender();
    }

  /* Urgent */
  if(c->flags & SUB_MODE_URGENT)
    {
      int i;

      /* Dehighlight views */
      for(i = 0; i < subtle->views->ndata; i++)
        {
          SubView *v = VIEW(subtle->views->data[i]);

          if(VISIBLE(v, c)) v->flags &= ~SUB_MODE_URGENT;
        }

      subViewRender();
    }

  /* Ignore further events and delete context */
  XSelectInput(subtle->dpy, c->win, NoEventMask);
  XDeleteContext(subtle->dpy, c->win, CLIENTID);
  XUnmapWindow(subtle->dpy, c->win);

  subSubtleFocus(True);

  /* Destroy window */
  if(destroy && !(c->flags & SUB_CLIENT_DEAD))
    {
      if(c->flags & SUB_CLIENT_CLOSE) ///< Honor window preferences
        {
          subEwmhMessage(c->win, c->win, SUB_EWMH_WM_PROTOCOLS,
            subEwmhGet(SUB_EWMH_WM_DELETE_WINDOW), CurrentTime, 0, 0, 0);
        }
      else XKillClient(subtle->dpy, c->win);
    }

  if(c->gravities) free(c->gravities);
  if(c->screens)   free(c->screens);
  if(c->name)      free(c->name);
  if(c->instance)  free(c->instance);
  if(c->klass)     free(c->klass);
  free(c);

  subSharedLogDebug("kill=client\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
