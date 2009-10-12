
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
      if((c = CLIENT(subSharedFind(subtle->windows.focus, CLIENTID))))
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

  /* Create client */
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
  c->base        = c->geom; ///< Backup size

  /* Init gravities and screens */
  grav = ClientGravity();
  for(i = 0; i < subtle->views->ndata; i++)
    {
      c->gravities[i] = grav;
      c->screens[i]   = 0;
    }

   /* Fetch name and class */
  XFetchName(subtle->dpy, c->win, &c->title);
  subSharedPropertyClass(c->win, &c->name, &c->klass);

  /* Fallback for stupid clients like Skype */
  if(!c->title || !strcmp("", c->title))
    {
      if(c->name) c->title = strdup(c->name);
      else if(c->klass) c->title = strdup(c->klass);
      else c->title = strdup("subtle");
    }
 
  /* X related properties */
  sattrs.border_pixel = subtle->colors.bo_normal;
  XChangeWindowAttributes(subtle->dpy, c->win, CWBorderPixel, &sattrs);
  XSetWindowBorderWidth(subtle->dpy, c->win, subtle->bw);
  XSelectInput(subtle->dpy, c->win, EVENTMASK);
  XAddToSaveSet(subtle->dpy, c->win);
  XSaveContext(subtle->dpy, c->win, CLIENTID, (void *)c);
  subEwmhSetWMState(c->win, WithdrawnState);
  /* Update client */
  subClientSetNormalHints(c);
  subClientSetProtocols(c);
  subClientSetStrut(c);
  subClientSetTags(c);

  /* Set client flags */
  subClientSetHints(c, &flags);
  subClientSetState(c, &flags);
  subClientSetTransient(c, &flags);
  subClientToggle(c, (~c->flags & flags));
  if(c->flags & (SUB_MODE_URGENT|SUB_MODE_URGENT_ONCE)) subClientWarp(c);

  /* EWMH: Gravity, screen and desktop */
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_GRAVITY, (long *)&c->gravity, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_SCREEN, (long *)&c->screen, 1);
  subEwmhSetCardinals(c->win, SUB_EWMH_NET_WM_DESKTOP, &vid, 1);

  if(subtle->hooks.create) ///< Create hook
    subRubyCall(SUB_CALL_HOOK, subtle->hooks.create, c);

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

  if(!(c->flags & SUB_MODE_FLOAT))
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
  ev.border_width      = subtle->bw;
  ev.above             = None;
  ev.override_redirect = False;

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
  char buf[50];
  XSetWindowAttributes sattrs;
  XGCValues gvals;

  DEAD(c);
  assert(c);

  /* Set window border */
  sattrs.border_pixel = subtle->windows.focus == c->win ? subtle->colors.bo_focus : 
    subtle->colors.bo_normal;
  XChangeWindowAttributes(subtle->dpy, c->win, CWBorderPixel, &sattrs);

  /* Title mode */
  if(c->flags & (SUB_MODE_STICK|SUB_MODE_FLOAT))
    snprintf(buf, sizeof(buf), "%c%s", c->flags & SUB_MODE_STICK ? '*' : '^', c->title);
  else snprintf(buf, sizeof(buf), "%s", c->title);

  /* Title window */
  gvals.foreground = subtle->colors.fg_focus;
  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);
  XClearWindow(subtle->dpy, subtle->panels.title.win);

  XDrawString(subtle->dpy, subtle->panels.title.win, subtle->gcs.font, 3, subtle->fy,
    buf, strlen(buf));
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
  SubClient *c1 = *(SubClient **)a, *c2 = *(SubClient **)b;

  assert(a && b);

  /* Check flags */
  if((c1->flags & SUB_MODE_FULL) == (c2->flags & SUB_MODE_FULL)) return 0;
  else if(c1->flags & SUB_MODE_FULL) return 1;
  else return -1;
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
    0 == subRubyCall(SUB_CALL_HOOK, subtle->hooks.focus, (void *)c))
    {
      subSharedLogDebug("Hook: name=focus, client=%#lx, state=ignored\n", c->win);

      return;
    }

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
                int check = 0;
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
                      if(left) ///< Drag to left
                        {
                          if(0 < (rx - wx) - (rx - ev.xmotion.x_root)) ///< Check edge
                            {
                              check          = ww + (rx - ev.xmotion.x_root); ///< Avoid overflow
                              c->geom.width  = check > c->minw ? check : c->minw;
                              c->geom.width -= (c->geom.width % c->incw);
                              c->geom.x      = (rx - wx) + ww - c->geom.width;
                            }
                        }
                      else ///< Drag to right
                        {
                          check          = ww - (rx - ev.xmotion.x_root); ///< Avoid overflow
                          c->geom.width  = check > c->minw ? check : c->minw;
                        }

                      check = wh - (ry - ev.xmotion.y_root); ///< Avoid overflow
                      c->geom.height = check > c->minh ? check : c->minh;

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
      
      /* Collect flags and tags */
      flags    |= (t->flags & (SUB_MODE_FULL|SUB_MODE_FLOAT|SUB_MODE_STICK|SUB_MODE_URGENT));
      c->flags |= (t->flags & (SUB_MODE_UNFULL|SUB_MODE_UNFLOAT|SUB_MODE_UNSTICK|SUB_MODE_UNURGENT));
      c->tags  |= (1L << (tag + 1));

      /* Set size and enable float */
      if(t->flags & SUB_TAG_SIZE && !(c->flags & SUB_MODE_UNFLOAT))
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
              if(t->flags & SUB_TAG_GRAVITY) c->gravities[i] = t->gravity;
              if(t->flags & SUB_TAG_SCREEN)  c->screens[i]   = t->screen;
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

      /* Check if tag matches client */
      if(t->preg &&
        ((t->flags & SUB_TAG_MATCH_TITLE && c->title && subSharedRegexMatch(t->preg, c->title)) ||
        (t->flags & SUB_TAG_MATCH_NAME && c->name && subSharedRegexMatch(t->preg, c->name)) ||
        (t->flags & SUB_TAG_MATCH_CLASS && c->klass && subSharedRegexMatch(t->preg, c->klass))))
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
      int grav1 = 1, grav2 = 1, m = 0;
      XRectangle *mode = NULL;
      SubGravity *g = NULL;
      SubScreen *s = NULL;

      /* Hook: Gravity */
      if(subtle->hooks.gravity && 
          0 == subRubyCall(SUB_CALL_HOOK, subtle->hooks.gravity, (void *)c))
        {
          subSharedLogDebug("Hook: name=gravity, client=%#lx, state=ignored\n", c->win);

          return;
        }

      grav1 = GETGRAV(gravity);
      grav2 = GETGRAV(c->gravity);
      m     = GETMODE(c->gravity);

      /* Get gravity and mode */
      g = GRAVITY(subtle->gravities->data[grav1 - 1]);
      if(grav1 == grav2)
        {
          if(m < g->nmodes - 1) m++;
          else m = 0;
        }
      else m = 0;
      mode = &g->modes[m];

      /* Calculate slot */
      s = SCREEN(subtle->screens->data[c->screen]);
      c->geom.x      = s->geom.x + (s->geom.width * mode->x / 100);
      c->geom.y      = s->geom.y + (s->geom.height * mode->y / 100);
      c->geom.width  = s->geom.width * mode->width / 100;
      c->geom.height = s->geom.height * mode->height / 100;

      /* Update client */
      c->gravity  = c->gravities[subtle->vid] = GRAVMODE(grav1, m);
      subClientSetSize(c);

      /* EWMH: Gravity */
      subEwmhSetCardinals(c->win, SUB_EWMH_SUBTLE_WINDOW_GRAVITY, (long *)&c->gravity, 1);
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
  DEAD(c);
  assert(c);

  if(subtle->flags & SUB_SUBTLE_RESIZE || c->flags & SUB_MODE_FLOAT)
    {
      SubScreen *s = SCREEN(subtle->screens->data[c->screen]);

      /* Limit width */
      if(c->base.width < c->minw)  c->base.width  = c->minw;
      if(c->base.width > c->maxw)  c->base.width  = c->maxw;

      if(c->geom.width < c->minw)  c->geom.width  = c->minw;
      if(c->geom.width > c->maxw)  c->geom.width  = c->maxw;

      /* Limit height */
      if(c->base.height < c->minh) c->base.height = c->minh;
      if(c->base.height > c->maxh) c->base.height = c->maxh;

      if(c->geom.height < c->minh) c->geom.height = c->minh;
      if(c->geom.height > c->maxh) c->geom.height = c->maxh;

      /* Limit sizes */
      subScreenLimit(s, &c->geom, True);
      subScreenLimit(s, &c->base, True);

      /* Check aspect ratios */
      if(c->minr && c->geom.height * c->minr > c->geom.width)
        c->geom.width = (int)(c->geom.height * c->minr);

      if(c->maxr && c->geom.height * c->maxr < c->geom.width)
        c->geom.width = (int)(c->geom.height * c->maxr);

      /* Check incs */
      c->geom.width  -= c->geom.width % c->incw; 
      c->geom.height -= c->geom.height % c->inch;

      /* Center */
      if(c->geom.x == s->geom.x)
        c->geom.x = (s->geom.width - c->geom.width) / 2;
      if(c->geom.y == s->geom.y)
        c->geom.y = (s->geom.height - c->geom.height) / 2;
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
  if((strut = (long *)subSharedPropertyGet(c->win, XA_CARDINAL, 
      SUB_EWMH_NET_WM_STRUT, &size)))
    {
      if(4 * sizeof(long) == size) ///< Only complete struts
        {
          subtle->strut.x      = MAX(subtle->strut.x,      strut[0]); ///< Left
          subtle->strut.y      = MAX(subtle->strut.y,      strut[1]); ///< Right
          subtle->strut.width  = MAX(subtle->strut.width,  strut[2]); ///< Top
          subtle->strut.height = MAX(subtle->strut.height, strut[3]); ///< Bottom

          /* Update screen and clients */
          subScreenConfigure();
          subViewConfigure(subtle->view, True);

          subSharedLogDebug("Strut: x=%ld, y=%d, width=%d, height=%d\n", subtle->strut.x,
            subtle->strut.y, subtle->strut.width, subtle->strut.height);
        }

      XFree(strut);
    }
} /* }}} */

 /** subClientSetTitle {{{
  * @brief Set title width
  * @param[in]  c  A #SubClient
  **/

void
subClientSetTitle(SubClient *c)
{
  int len = 0;

  assert(c);
  DEAD(c);

  len = strlen(c->title) + (c->flags & (SUB_MODE_STICK|SUB_MODE_FLOAT) ? 1 : 0);

  /* Update panel width */
  subtle->panels.title.width = subSharedTextWidth(c->title, 50 >= len ? len : 50, 
    NULL, NULL, True) + 6;
  XResizeWindow(subtle->dpy, subtle->panels.title.win, 
    subtle->panels.title.width, subtle->th);
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

  /** subClientSetNormalHints {{{
   * @brief Set client normal hints
   * @param[in]  c  A #SubClient
   **/

void
subClientSetNormalHints(SubClient *c)
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
          /* Limit min size to screen size if larger */
          if(size->min_width)  
            c->minw = c->minw > s->geom.width ? s->geom.width : MIN(MINW, size->min_width);
          if(size->min_height) 
            c->minh = c->minh > s->geom.height ? s->geom.height : MIN(MINH, size->min_height);
        }

      if(size->flags & PMaxSize) ///< Program max size
        {
          /* Limit max size to screen size if larger */
          if(size->max_width)
            c->maxw = size->max_width > s->geom.width ?  s->geom.width : size->max_width;
          if(size->max_height)
            c->maxh = size->max_height > s->geom.height - subtle->th ? 
              s->geom.height - subtle->th : size->max_height;
        }

      if(size->flags & (USPosition|PPosition)) ///< User/program size
        {
          c->geom.x = c->base.x = size->x;
          c->geom.y = c->base.y = size->y;
        }

      if(size->flags & (USPosition|PPosition)) ///< User/program position
        {
          c->geom.x = c->base.x = size->x;
          c->geom.y = c->base.y = size->y;
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

  /** subClientSetHints {{{
   * @brief Set client WM hints
   * @param[in]  c      A #SubClient
   * @param[in]  flags  Mode flags
   **/

void
subClientSetHints(SubClient *c,
  int *flags)
{
  XWMHints *hints = NULL;

  assert(c && flags);

  /* Window manager hints */
  if((hints = XGetWMHints(subtle->dpy, c->win)))
    {
      /* Handle urgency */
      if(!(c->flags & SUB_MODE_UNURGENT))
        {
          if(hints->flags & XUrgencyHint)     *flags |= SUB_MODE_URGENT;
          else if(c->flags & SUB_MODE_URGENT) *flags |= SUB_MODE_URGENT_ONCE;
        }
        
      if(hints->flags & InputHint && hints->input) c->flags |= SUB_CLIENT_INPUT;

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
  if((states = (Atom *)subSharedPropertyGet(c->win, XA_ATOM, 
      SUB_EWMH_NET_WM_STATE, &size)))
    {
      for(i = 0; i < size / sizeof(Atom); i++)
        {
          switch(subEwmhFind(states[i]))
            {
              case SUB_EWMH_NET_WM_STATE_FULLSCREEN: *flags |= SUB_MODE_FULL;  break;
              case SUB_EWMH_NET_WM_STATE_ABOVE:      *flags |= SUB_MODE_FLOAT; break;
              case SUB_EWMH_NET_WM_STATE_STICKY:     *flags |= SUB_MODE_STICK; break;
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
  SubClient *k = NULL;

  assert(c && flags);

  /* Check for transient windows */
  if(XGetTransientForHint(subtle->dpy, c->win, &trans))
    {
      /* Check if transient windows should be urgent */
      *flags |= subtle->flags & SUB_SUBTLE_URGENT ? SUB_MODE_FLOAT|SUB_MODE_URGENT : SUB_MODE_FLOAT;

      if((k = CLIENT(subSharedFind(trans, CLIENTID))))
        {
          /* Copy tags and screens */
          c->tags   |= k->tags;
          c->screen |= k->screen;

           for(i = 0; i < subtle->views->ndata; i++)
             c->screens[i] = k->screens[i];
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
  int flags = 0;

  DEAD(c);
  assert(c);

  /* Remove flags */
  if(type & SUB_MODE_FULL && c->flags & SUB_MODE_UNFULL)     type &= ~SUB_MODE_FULL;
  if(type & SUB_MODE_FLOAT && c->flags & SUB_MODE_UNFLOAT)   type &= ~SUB_MODE_FLOAT;
  if(type & SUB_MODE_STICK && c->flags & SUB_MODE_UNSTICK)   type &= ~SUB_MODE_STICK;
  if(type & SUB_MODE_URGENT && c->flags & SUB_MODE_UNURGENT) type &= ~SUB_MODE_URGENT;

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

      if(type & SUB_MODE_STICK) subViewConfigure(subtle->view, False);
    }
  else ///< Set flags
    {
      c->flags |= type;

      if(type & SUB_MODE_FLOAT)
        {
          c->geom = c->base;

          subClientSetSize(c); ///< Sanitize
        }

      if(type & SUB_MODE_FULL)
        {
          XSetWindowBorderWidth(subtle->dpy, c->win, 0);
          subClientSetSize(c); ///< Sanitize
        }
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

  if(VISIBLE(subtle->view, c)) ///< Check visibility first
    {
      subClientFocus(c);
      subClientSetTitle(c);
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

  /* Focus */
  if(subtle->windows.focus == c->win)
    {
      subtle->windows.focus      = 0;
      subtle->panels.title.width = 0;
      subPanelUpdate();
      subPanelRender();
    }
    
  /* Ignore further events and delete context */
  XSelectInput(subtle->dpy, c->win, NoEventMask);
  XDeleteContext(subtle->dpy, c->win, CLIENTID);
  XUnmapWindow(subtle->dpy, c->win);

  subSharedFocus(); ///< Focus

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

  if(c->gravities) free(c->gravities);
  if(c->screens)   free(c->screens);
  if(c->title)     XFree(c->title);
  if(c->name)      XFree(c->name);
  if(c->klass)     XFree(c->klass);
  free(c);

  subSharedLogDebug("kill=client\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
