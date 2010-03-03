
 /**
  * @package subtle
  *
  * @file Shared functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <stdarg.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include "shared.h"

 /** subSharedLog {{{
  * @brief Print messages depending on type
  * @param[in]  type    Message type
  * @param[in]  file    File name
  * @param[in]  line    Line number
  * @param[in]  format  Message format
  * @param[in]  ...     Variadic arguments
  **/

void
subSharedLog(int type,
  const char *file,
  int line,
  const char *format,
  ...)
{
  va_list ap;
  char buf[255];

#ifdef DEBUG
#ifdef SUBTLE
  if((!subtle && !type) || (subtle && 
    !(subtle->flags & SUB_SUBTLE_DEBUG) && !type)) return;
#else  /* SUBTLE */
  if(!debug && !type) return;
#endif /* SUBTLE */
#endif /* DEBUG */

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  switch(type)
    {
#ifdef DEBUG
      case 0: fprintf(stderr, "<DEBUG> %s:%d: %s", file, line, buf); break;
#endif /* DEBUG */
      case 1: fprintf(stderr, "<ERROR> %s", buf);                    break;
      case 2: fprintf(stdout, "<WARNING> %s", buf);                  break;
      case 3: fprintf(stderr, "%s", buf);                            break;
    }
} /* }}} */

 /** subSharedLogXError {{{
  * @brief Print X error messages
  * @params[in]  display  Display
  * @params[in]  ev       A #XErrorEvent
  * @return Returns zero
  * @retval  0  Default return value
  **/

int
subSharedLogXError(Display *disp,
  XErrorEvent *ev)
{
#ifdef DEBUG
#ifdef SUBTLE
  if(!(subtle->flags & SUB_SUBTLE_DEBUG)) return 0;
#else /* SUBTLE */
  if(debug) return 0;
#endif /* SUBTLE */
#endif /* DEBUG */

  if(42 != ev->request_code) /* X_SetInputFocus */
    {
      char error[255];
      XGetErrorText(disp, ev->error_code, error, sizeof(error));
      subSharedLogDebug("%s: win=%#lx, request=%d\n", error, ev->resourceid, ev->request_code);
    }

  return 0; 
} /* }}} */

 /** subSharedMemoryAlloc {{{
  * @brief Alloc memory and check result
  * @param[in]  n     Number of elements
  * @param[in]  size  Size of the memory block
  * @return Returns new memory block or \p NULL
  **/

void *
subSharedMemoryAlloc(size_t n,
  size_t size)
{
  void *mem = calloc(n, size);
  if(!mem) subSharedLogError("Failed allocating memory\n");
  return mem;
} /* }}} */

 /** subSharedMemoryRealloc {{{
  * @brief Realloc memory and check result
  * @param[in]  mem   Memory block
  * @param[in]  size  Size of the memory block
  * @return Returns new memory block or \p NULL
  **/

void *
subSharedMemoryRealloc(void *mem,
  size_t size)
{
  mem = realloc(mem, size);
  if(!mem) subSharedLogDebug("Memory has been freed. Expected?\n");
  return mem;
} /* }}} */

 /** subSharedRegexNew {{{ 
  * @brief Create new regex
  * @param[in]  regex  Regex 
  * @return Returns a #regex_t or \p NULL
  **/

regex_t *
subSharedRegexNew(char *regex)
{
  int errcode;
  regex_t *preg = NULL;

  assert(regex);
  
  preg = (regex_t *)subSharedMemoryAlloc(1, sizeof(regex_t));

  /* Thread safe error handling */
  if((errcode = regcomp(preg, regex, REG_EXTENDED|REG_NOSUB|REG_ICASE)))
    {
      size_t errsize = regerror(errcode, preg, NULL, 0);
      char *errbuf = (char *)subSharedMemoryAlloc(1, errsize);

      regerror(errcode, preg, errbuf, errsize);

      subSharedLogDebug("Can't compile preg `%s': %s\n", regex, errbuf);

      free(errbuf);
      subSharedRegexKill(preg);

      return NULL;
    }
  return preg;
} /* }}} */

 /** subSharedRegexMatch {{{
  * @brief Check if string match preg
  * @param[in]  preg      A #regex_t
  * @param[in]  string    String
  * @retval  1  If string matches preg
  * @retval  0  If string doesn't match
  **/

int
subSharedRegexMatch(regex_t *preg,
  char *string)
{
  assert(preg);

  return !regexec(preg, string, 0, NULL, 0);
} /* }}} */

 /** subSharedRegexKill {{{
  * @brief Kill preg
  * @param[in]  preg  #regex_t
  **/

void
subSharedRegexKill(regex_t *preg)
{
  assert(preg);

  regfree(preg);
  free(preg);
} /* }}} */

 /** subSharedMatch {{{
  * @brief Match a window based on position
  * @param[in]  type       Type of matching
  * @param[in]  geometry1  Geometry 1
  * @param[in]  geometry2  Geometry 2
  **/

int
subSharedMatch(int type,
  XRectangle *geometry1,
  XRectangle *geometry2)
{
  int dx = 0, dy = 0;

  /* Euclidean distance */
  dx = abs(geometry1->x - geometry2->x);
  dy = abs(geometry1->y - geometry2->y);

  if(0 == dx) dx = 100;
  if(0 == dy) dy = 100;

  /* Add weighting */
  switch(type)
    {
     case SUB_WINDOW_LEFT:
        if(geometry2->x < geometry1->x) dx /= 8;
        dy *= 2;
        break;
      case SUB_WINDOW_RIGHT:
        if(geometry2->x > geometry1->x) dx /= 8;
        dy *= 2;
        break;
      case SUB_WINDOW_UP:
        if(geometry2->y < geometry1->y) dy /= 8;
        dx *= 2;
        break;
      case SUB_WINDOW_DOWN:
        if(geometry2->y > geometry1->y) dy /= 8;
        dx *= 2;
        break;
    }

  return dx + dy;
} /* }}} */

 /** subSharedPropertyGet {{{
  * @brief Get window property
  * @param[in]  win   Client window
  * @param[in]  type  Property type 
  * @param[in]  name  Property name
  * @param[in]  e     A #SubEwmh
  * @param[in]  size  Size of the property
  * return Returns the property
  **/

char *
subSharedPropertyGet(Window win,
  Atom type,
#ifdef SUBTLE
  SubEwmh e,
#else /* SUBTLE */
  char *name,
#endif /* SUBTLE */
  unsigned long *size)
{
  int format = 0;
  unsigned long nitems = 0, bytes = 0;
  unsigned char *data = NULL;
  Atom rtype = None, prop = None;
  Display *disp = NULL;

#ifdef SUBTLE
  char name[5] = { 0 };
#endif /* SUBTLE */

  assert(win);

#ifdef SUBTLE
  disp = subtle->dpy; 
  prop = subEwmhGet(e);

  snprintf(name, sizeof(name), "%d", e);
#else /* SUBTLE */
  disp = display;
  prop = XInternAtom(display, name, False);
#endif /* SUBTLE */

  if(Success != XGetWindowProperty(disp, win, prop, 0L, 4096, 
      False, type, &rtype, &format, &nitems, &bytes, &data))
    {
      subSharedLogDebug("Failed getting property `%s'\n", name);

      return NULL;
    }

  /* Check result */
  if(type != rtype)
    {
      subSharedLogDebug("Property: name=%s, type=%ld, rtype=%ld'\n", name, type, rtype);
      XFree(data);

      return NULL;
    }

#ifdef __LP64__
  if(size) *size = (unsigned long)(format / 4) * nitems;
#else /* LP64 */
  if(size) *size = (unsigned long)(format / 8) * nitems;
#endif /* LP64 */

  return (char *)data;
} /* }}} */

 /** subSharedPropertyStrings {{{
  * @brief Get property list
  * @param[in]     win   Client window
  * @param[in]     name  Property name
  * @param[in]     e     A #SubEwmh
  * @param[inout]  size  Size of the property list
  * return Returns the property list
  **/

char **
subSharedPropertyStrings(Window win,
#ifdef SUBTLE
  SubEwmh e,
#else /* SUBTLE */
  char *name,
#endif /* SUBTLE */
  int *size)
{
  Atom atom;
  char **list = NULL;
  XTextProperty prop;
  Display *disp = NULL;

  assert(win && size);

  /* Check UTF8 and XA_STRING */
#ifdef SUBTLE
  disp = subtle->dpy;
  atom = subEwmhGet(e);
#else /* SUBTLE */
  disp = display;
  atom = XInternAtom(display, name, False);
#endif /* SUBTLE */ 
 
  if((XGetTextProperty(disp, win, &prop, atom) || 
    XGetTextProperty(disp, win, &prop, XA_STRING)) && prop.nitems)
    {
      XmbTextPropertyToTextList(disp, &prop, &list, size);

      XFree(prop.value);
    }

  return list;
} /* }}} */

 /** subSharedPropertyName {{{
  * @brief Get window title
  * @warning Must be free'd
  * @param[in]     win       A #Window
  * @param[inout]  name      Window WM_NAME
  * @param[in]     fallback  Fallback name
  **/

void
subSharedPropertyName(Window win,
  char **name,
  char *fallback)
{
  Display *disp = NULL;
  char **list = NULL;
  XTextProperty prop;
  Atom atom;

#ifdef SUBTLE
  disp = subtle->dpy;
  atom = subEwmhGet(SUB_EWMH_NET_WM_NAME);
#else /* SUBTLE */
  disp = display;
  atom = XInternAtom(display, "_NET_WM_NAME", False);
#endif /* SUBTLE */
  
  /* Get text property */
  XGetTextProperty(disp, win, &prop, atom);
  if(!prop.nitems)
    {
      XGetTextProperty(disp, win, &prop, XA_WM_NAME);
      if(!prop.nitems)
        {
          *name = strdup(fallback);

          return;
        }
    }

  /* Handle encoding */
  if(XA_STRING == prop.encoding)
    *name = strdup((char *)prop.value);
  else
    {
      int size = 0;

      if(Success == XmbTextPropertyToTextList(disp, &prop, &list, &size) && 0 < size)
        {
          *name = strdup(*list);

          XFreeStringList(list);
        }
    }

  /* Fallback */
  if(!*name) *name = strdup(fallback);
} /* }}} */

 /** subSharedPropertyClass {{{
  * @brief Get window class
  * @warning Must be free'd
  * @param[in]     win    A #Window
  * @param[inout]  inst   Window instance name
  * @param[inout]  klass  Window class name
  **/

void
subSharedPropertyClass(Window win,
  char **inst,
  char **klass)
{
  int size = 0;
  char **klasses = NULL;

  assert(win);

  klasses = subSharedPropertyStrings(win, 
#ifdef SUBTLE
    SUB_EWMH_WM_CLASS,
#else /* SUBTLE */    
    "WM_CLASS",
#endif /* SUBTLE */    
    &size);

  /* Sanitize instance/class names */
  if(inst)  *inst  = strdup(0 < size ? klasses[0] : "subtle");
  if(klass) *klass = strdup(1 < size ? klasses[1] : "subtle");

  if(klasses) XFreeStringList(klasses);
} /* }}} */

 /** subSharedPropertyGeometry {{{
  * @brief Get window geometry
  * @param[in]     win       A #Window
  * @param[inout]  geometry  A #XRectangle
  **/

void
subSharedPropertyGeometry(Window win,
  XRectangle *geometry)
{
  XWindowAttributes attrs;

  assert(win && geometry);

#ifdef SUBTLE
  XGetWindowAttributes(subtle->dpy, win, &attrs);
#else /* SUBTLE */
  XGetWindowAttributes(display, win, &attrs);
#endif /* SUBTLE */

  /* Copy values */
  geometry->x      = attrs.x;
  geometry->y      = attrs.y;
  geometry->width  = attrs.width;
  geometry->height = attrs.height;
} /* }}} */

 /** subSharedPropertyDelete {{{
  * @brief Deletes the property
  * @param[in]  win   A #Window
  * @param[in]  name  Property name
  * @param[in]  e     A #SubEwmh
  **/

void 
subSharedPropertyDelete(Window win, 
#ifdef SUBTLE
  SubEwmh e)
#else /* SUBTLE */
  char *name)
#endif /* SUBTLE */
{
  assert(win);

#ifdef SUBTLE
  XDeleteProperty(subtle->dpy, win, subEwmhGet(e));
#else /* SUBTLE */
  XDeleteProperty(display, win, XInternAtom(display, name, False));
#endif /* SUBTLE */
} /* }}} */

 /** subSharedParseColor {{{
  * @brief Parse and load color
  * @param[in]  name  Color string
  * @return Color pixel value
  **/

unsigned long
subSharedParseColor(char *name)
{
  XColor color = { 0 }; ///< Default color
  Display *disp = NULL;

  assert(name);

#ifdef SUBTLE
  disp = subtle->dpy; 
#else /* SUBTLE */
  disp = display;
#endif /* SUBTLE */

  /* Parse and store color */
  if(!XParseColor(disp, DefaultColormap(disp, DefaultScreen(disp)), 
      name, &color))
    {
      subSharedLogWarn("Failed loading color `%s'\n", name);
    }
  else if(!XAllocColor(disp, DefaultColormap(disp, DefaultScreen(disp)), 
      &color))
    subSharedLogWarn("Failed allocating color `%s'\n", name);

  return color.pixel;
} /* }}} */

 /** subSharedSpawn {{{
  * @brief Spawn a command
  * @param[in]  cmd  Command string
  **/

pid_t
subSharedSpawn(char *cmd)
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

  return pid;
} /* }}} */

#ifdef SUBTLE
 /** subSharedFind {{{
  * @brief Find data with the context manager
  * @param[in]  win  A #Window
  * @param[in]  id   Context id
  * @return Returns found data pointer or \p NULL
  **/

XPointer *
subSharedFind(Window win,
  XContext id)
{
  XPointer *data = NULL;

  return XCNOENT != XFindContext(subtle->dpy, win, id, (XPointer *)&data) ? data : NULL;
} /* }}} */

 /** subSharedTime {{{
  * @brief Get the current time in seconds 
  * @return Returns time in seconds
  **/

time_t
subSharedTime(void)
{
  struct timeval tv;

  gettimeofday(&tv, 0);

  return tv.tv_sec;
} /* }}} */

 /** subSharedFocus {{{
  * @brief Get pointer window and focus it
  * @param[in]  focus  Focus next client
  * @return New focus window
  **/

Window
subSharedFocus(int focus)
{
  int dummy;
  Window win;
  SubClient *c = NULL;

  /* Focus */
  XQueryPointer(subtle->dpy, ROOT, (Window *)&dummy, &win,
    &dummy, &dummy, &dummy, &dummy, (unsigned int *)&dummy);
  
  /* Find next client */
  if((c = CLIENT(subSharedFind(win, CLIENTID)))) 
    {
      subClientFocus(c);

      return c->win;
    }
  else if(focus)
    {
      int i;

      for(i = 0; i < subtle->clients->ndata; i++)
        {
          c = CLIENT(subtle->clients->data[i]);

          /* Check screen and visibility */
          if(c->screen == subtle->sid && VISIBLE(CURVIEW, c))
            {
              subClientWarp(c);
              subClientFocus(c);

              return c->win;
            }
        }
    }

  XSetInputFocus(subtle->dpy, ROOT, RevertToParent, CurrentTime); ///< Fallback

  return ROOT;
} /* }}} */

 /** subSharedTextWidth {{{
  * @brief Get width of the smallest enclosing box
  * @param[in]     text    The text
  * @param[in]     len     Length of the string
  * @param[inout]  left    Left bearing
  * @param[inout]  right   Right bearing
  * @param[in]     center  Center text
  * @return Width of the box
  **/

int
subSharedTextWidth(const char *text,
  int len,
  int *left,
  int *right,
  int center)
{
  int width = 0, lbearing = 0, rbearing = 0;

  assert(text);

#ifdef HAVE_X11_XFT_XFT_H
  if(subtle->flags & SUB_SUBTLE_XFT) ///< XFT
    {
      XGlyphInfo extents;

      XftTextExtentsUtf8(subtle->dpy, subtle->font.xft, (XftChar8*)text,
        len, &extents);

      lbearing = extents.x;
      rbearing = extents.xOff - extents.width;
      width    = extents.width;
    }
  else ///< XFS
#endif /* HAVE_X11_XFT_XFT_H */
    {
      XRectangle overall_ink = { 0 }, overall_logical = { 0 };

      XmbTextExtents(subtle->font.xfs, text, len, 
        &overall_ink, &overall_logical);

      width    = overall_logical.width;
      lbearing = overall_logical.x;
      rbearing = rbearing;
    }

  if(left)  *left  = lbearing;
  if(right) *right = rbearing;

  return center ? width - abs(lbearing - rbearing) : width;
} /* }}} */

 /** subSharedTextDraw {{{
  * @brief Draw text
  * @param[in]  win   Length of the string
  * @param[in]  x     Left bearing
  * @param[in]  y     Right bearing
  * @param[in]  fg    Center text
  * @param[in]  bg    Center text
  * @param[in]  text  The string
  * @return Width of the box
  **/

void subSharedTextDraw(Window win,
  int x,
  int y,
  long fg,
  long bg,
  const char *text)
{
  assert(text);

  /* Clear window */
  if(0 <= bg)
    {
      XSetWindowBackground(subtle->dpy, win, bg);
      XClearWindow(subtle->dpy, win);
    }

  /* Draw text */
#ifdef HAVE_X11_XFT_XFT_H
  if(subtle->flags & SUB_SUBTLE_XFT) ///< XFT
    {
      XftColor color = { 0 };
      XColor xcolor = { 0 };

      /* Get color */
      xcolor.pixel = fg;

      XQueryColor(subtle->dpy, COLORMAP, &xcolor);

      color.pixel       = xcolor.pixel;
      color.color.red   = xcolor.red;
      color.color.green = xcolor.green;
      color.color.blue  = xcolor.blue;
      color.color.alpha = 0xffff;

      XftDrawChange(subtle->font.draw, win);                                                                               
      XftDrawStringUtf8(subtle->font.draw, &color, subtle->font.xft,
        x, y, (XftChar8 *)text, strlen(text));
    }
  else ///< XFS
#endif /* HAVE_X11_XFT_XFT_H */
    {
      XGCValues gvals;

      gvals.foreground = fg;
      
      XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);
      XmbDrawString(subtle->dpy, win, subtle->font.xfs, subtle->gcs.font, 
        x, y, text, strlen(text));
    }
} /* }}} */

#else /* SUBTLE */

/* SharedList {{{ */
Window *
SharedList(char *prop,
  int *size)
{
  Window *wins = NULL;
  unsigned long len = 0;

  assert(prop && size);

  if((wins = (Window *)subSharedPropertyGet(DefaultRootWindow(display),
      XA_WINDOW, prop, &len)))
    {
      *size = len / sizeof(Window);
    }
  else
    {
      *size = 0;
      subSharedLogDebug("Failed getting window list\n");
    }

  return wins;
} /* }}} */

/* SharedFind {{{ */
int
SharedFind(char *prop,
  char *match,
  char **name)
{
  int ret = -1, size = 0;
  char **strings = NULL;
  regex_t *preg = NULL;

  assert(prop && match);

  /* Find sublet id */
  if((preg = subSharedRegexNew(match)) &&
      (strings = subSharedPropertyStrings(DefaultRootWindow(display), prop, &size)))
    {
      int i;

      for(i = 0; i < size; i++)
        if((isdigit(match[0]) && atoi(match) == i) || 
            (!isdigit(match[0]) && subSharedRegexMatch(preg, strings[i])))
          {
            subSharedLogDebug("Found: prop=%s, match=%s, name=%s, id=%d\n", 
              prop, match, strings[i], i);

            if(name) *name = strdup(strings[i]);

            ret = i;
            break;
          }

      subSharedRegexKill(preg);
      XFreeStringList(strings);
    }
  else subSharedLogDebug("Failed finding string `%s'\n", match);

  return ret;
} /* }}} */

/* SharedFindWindow {{{ */
int
SharedFindWindow(char *prop,
  char *match,
  char **name,
  Window *win,
  int flags)
{
  int id = -1, size = 0, gravity1 = 0, gravity2 = 0;
  Window *wins = NULL;

  assert(prop && match);
  
  /* Find window id */
  if((wins = SharedList(prop, &size)))
    {
      int i;
      char *wmname = NULL, *instance = NULL, *klass = NULL, *role = NULL, buf[20] = { 0 };
      Window selwin = None;
      regex_t *preg = subSharedRegexNew(match);

      /* Select window */
      if(!strncmp(match, "#", 1) && win)
        selwin = subSharedWindowSelect();

      /* Find id of gravity */
      if(flags & SUB_MATCH_GRAVITY)
        gravity1 = subSharedGravityFind(match, NULL, NULL); 

      for(i = 0; -1 == id && i < size; i++)
        {
          XFetchName(display, wins[i], &wmname);
          subSharedPropertyClass(wins[i], &instance, &klass);
          snprintf(buf, sizeof(buf), "%#lx", wins[i]);

          /* Get window gravity */
          if(flags & SUB_MATCH_GRAVITY)
            {
              int *gravity = (int *)subSharedPropertyGet(wins[i], XA_CARDINAL,
                "SUBTLE_WINDOW_GRAVITY", NULL);

              gravity2 = *gravity;

              subSharedLogDebug("Gravity: match=%s, gravity1=%d, gravity2=%d\n", 
                match, gravity1, gravity2);

              free(gravity);
            }

          /* Get window role */
          role = subSharedPropertyGet(wins[i], XA_STRING, "WM_WINDOW_ROLE", NULL);

          /* Find window either by window id, by title/inst/class or by gravity */
          if(wins[i] == selwin || subSharedRegexMatch(preg, buf) ||
              (flags & SUB_MATCH_NAME     && wmname     && subSharedRegexMatch(preg, wmname))   ||
              (flags & SUB_MATCH_INSTANCE && instance   && subSharedRegexMatch(preg, instance)) ||
              (flags & SUB_MATCH_CLASS    && klass      && subSharedRegexMatch(preg, klass))    ||
              (flags & SUB_MATCH_ROLE     && role       && subSharedRegexMatch(preg, role))     ||
              (flags & SUB_MATCH_GRAVITY  && gravity1 == gravity2))
            {
              subSharedLogDebug("Found: prop=%s, name=%s, win=%#lx, id=%d, flags\n", 
                prop, match, wins[i], i, flags);

              if(win) *win = wins[i];
              if(name)
                {
                  *name = (char *)subSharedMemoryAlloc(strlen(instance) + 1, sizeof(char));
                  strncpy(*name, instance, strlen(instance));
                 }
              
              id = i;
            }
          
          if(role) free(role);
          free(wmname);
          free(instance);
          free(klass);
        }

      subSharedRegexKill(preg);
      free(wins);
    }
  else subSharedLogDebug("Failed finding window `%s'\n", match);

  return id;
} /* }}} */

 /** subSharedMessage {{{
  * @brief Send client message to window
  * @param[in]  win    Client window
  * @param[in]  type   Message type 
  * @param[in]  data   A #SubMessageData
  * @param[in]  xsync  Sync connection
  * @returns
  **/

int
subSharedMessage(Window win,
  char *type,
  SubMessageData data,
  int xsync)
{
  int status = 0;
  XEvent ev;
  long mask = SubstructureRedirectMask|SubstructureNotifyMask;

  assert(win);

  /* Assemble event */
  ev.xclient.type         = ClientMessage;
  ev.xclient.serial       = 0;
  ev.xclient.send_event   = True;
  ev.xclient.message_type = XInternAtom(display, type, False);
  ev.xclient.window       = win;
  ev.xclient.format       = 32;

  /* Data */
  ev.xclient.data.l[0] = data.l[0];
  ev.xclient.data.l[1] = data.l[1];
  ev.xclient.data.l[2] = data.l[2];
  ev.xclient.data.l[3] = data.l[3];
  ev.xclient.data.l[4] = data.l[4];

  if(!display || !((status = XSendEvent(display, DefaultRootWindow(display), False, mask, &ev))))
    subSharedLogWarn("Failed sending client message `%s'\n", type);

  if(True == xsync) XSync(display, False);

  return status;
} /* }}} */

 /** subSharedWindowWMCheck {{{
  * @brief Get WM check window
  **/

Window *
subSharedWindowWMCheck(void)
{
  return (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW,
    "_NET_SUPPORTING_WM_CHECK", NULL);
} /* }}} */

 /** subSharedWindowSelect {{{
  * @brief Selects a window
  * @return Returns the selected window
  * @retval  None  No window selected
  **/

Window
subSharedWindowSelect(void)
{
  int i, format = 0, buttons = 0;
  unsigned int n;
  unsigned long nitems = 0, bytes = 0;
  unsigned char *data = NULL;
  XEvent event;
  Atom type = None, rtype = None;
  Window win = None, dummy = None, root = None, *wins = NULL;
  Cursor cursor = None;

  root   = DefaultRootWindow(display);
  cursor = XCreateFontCursor(display, XC_cross);
  type   = XInternAtom(display, "WM_STATE", True);

  if(XGrabPointer(display, root, False, ButtonPressMask|ButtonReleaseMask, 
      GrabModeSync, GrabModeAsync, root, cursor, CurrentTime))
    {
      XFreeCursor(display, cursor);

      return None;
    }

  /* Select a window */
  while(None == win || 0 != buttons)
    {
      XAllowEvents(display, SyncPointer, CurrentTime);
      XWindowEvent(display, root, ButtonPressMask|ButtonReleaseMask, &event);

      switch(event.type)
        {
          case ButtonPress:
            if(None == win) 
              win = event.xbutton.subwindow ? event.xbutton.subwindow : root; ///< Sanitize
            buttons++;
            break;
          case ButtonRelease: if(0 < buttons) buttons--; break;
        }
      }

  /* Find children with WM_STATE atom */
  XQueryTree(display, win, &dummy, &dummy, &wins, &n);
  for(i = 0; i < n; i++)
    if(Success == XGetWindowProperty(display, wins[i], type, 0, 0, False, 
        AnyPropertyType, &rtype, &format, &nitems, &bytes, &data))
      {
        if(data) 
          {
            XFree(data);
            data = NULL;
          }
        if(type == rtype) 
          {
            win = wins[i];
            break;
          }
      }

  XFree(wins);
  XFreeCursor(display, cursor);
  XUngrabPointer(display, CurrentTime);

  return win;
} /* }}} */

 /** subSharedClientList {{{
  * @brief Get client list
  * @param[inout]  size  Size of the window list
  * @return Returns the window list
  * @retval  NULL  No clients found
  **/

Window *
subSharedClientList(int *size)
{
  return SharedList("_NET_CLIENT_LIST", size);
} /* }}} */

 /** subSharedTrayList {{{
  * @brief Get tray list
  * @param[inout]  size  Size of the window list
  * @return Returns the window list
  * @retval  NULL  No trays found
  **/

Window *
subSharedTrayList(int *size)
{
  return SharedList("SUBTLE_TRAY_LIST", size);
} /* }}} */

 /** subSharedClientFind {{{
  * @brief Find client id
  * @param[in]     match  Match string
  * @param[inout]  name   Name of the client
  * @param[inout]  win    Client window
  * @param[in]     flags  Matching flags
  * @return Returns the client window list id
  * @retval  -1  Client not found
  **/

int
subSharedClientFind(char *match,
  char **name,
  Window *win,
  int flags)
{
  return SharedFindWindow("_NET_CLIENT_LIST", match, name, win, flags);
} /* }}} */

 /** subSharedGravityFind {{{
  * @brief Get gravity data
  * @param[in]     match     Name or id of the gravity
  * @param[inout]  name      Name of the gravity
  * @param[inout]  geometry  Gravity geometry
  * @return Returns the id of the gravity 
  * @retval  -1  No gravity found
  **/

int
subSharedGravityFind(char *match,
  char **name,
  XRectangle *geometry)
{
  int ret = -1, size = 0;
  char **gravities = NULL;
  regex_t *preg = NULL;

  assert(match);

  /* Find gravity id */
  if((preg = subSharedRegexNew(match)) &&
      (gravities = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_GRAVITY_LIST", &size)))
    {
      int i;
      XRectangle geom = { 0 };
      char buf[30] = { 0 };

      for(i = 0; i < size; i++)
        {
          sscanf(gravities[i], "%hdx%hd+%hd+%hd#%s", &geom.x, &geom.y,
            &geom.width, &geom.height, buf);

          /* Check id and name */
          if((isdigit(match[0]) && atoi(match) == i) || 
              (!isdigit(match[0]) && subSharedRegexMatch(preg, buf)))
            {
              subSharedLogDebug("Found: type=gravity, name=%s, id=%d\n", buf, i);

              if(geometry) *geometry = geom;
              if(name)
                {
                  *name = (char *)subSharedMemoryAlloc(strlen(buf) + 1, sizeof(char));
                  strncpy(*name, buf, strlen(buf));
                }

              ret = i;
              break;
            }
          }

      subSharedRegexKill(preg);
      XFreeStringList(gravities);
    }
  else subSharedLogDebug("Failed finding gravity `%s'\n", name);

  return ret;
} /* }}} */

 /** subSharedScreenFind {{{
  * @brief Find screen id
  * @param[in]   id        Screen id
  * @param[out]  geometry  Geometry of the screen
  * @return Returns the screen list id
  * @retval  -1  Screen not found
  **/

int
subSharedScreenFind(int id,
  XRectangle *geometry)
{
  int ret = -1;

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;

  /* Xinerama */
  if(XineramaQueryExtension(display, &xinerama_event, &xinerama_error) &&
      XineramaIsActive(display))
    {
      int n = 0;
      XineramaScreenInfo *screens = NULL;

      /* Query screens */
      if((screens = XineramaQueryScreens(display, &n)))
        {
          if(0 <= id && n > id) ///< Find selected screen
            {
              ret = id;

              if(geometry)
                {
                  geometry->x      = screens[id].x_org;
                  geometry->y      = screens[id].y_org;
                  geometry->width  = screens[id].width;
                  geometry->height = screens[id].height;
                }
            }

          XFree(screens);
        }
    } 
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Probably default screen */
  if(-1 == ret && 0 == id)
    {
      ret = id;

      if(geometry)
        {
          geometry->x      = 0;
          geometry->y      = 0;
          geometry->width  = DisplayWidth(display, DefaultScreen(display));
          geometry->height = DisplayHeight(display, DefaultScreen(display));
        }
    }

  return ret;
} /* }}} */

 /** subSharedTagFind {{{
  * @brief Find tag id
  * @param[in]     match  Tag name or id
  * @param[inout]  name   Name of the found tag
  * @return Returns the tag list id
  * @retval  -1  Tag not found
  **/

int
subSharedTagFind(char *match,
  char **name)
{
  return SharedFind("SUBTLE_TAG_LIST", match, name);
} /* }}} */

 /** subSharedTrayFind {{{
  * @brief Find tray id
  * @param[in]     match  Tray name or id
  * @param[inout]  name   Name of the found tray
  * @param[inout]  win    Tray window
  * @param[in]     flags  Matching flags
  * @return Returns the tray list id
  * @retval  -1  Tag not found
  **/

int
subSharedTrayFind(char *match,
  char **name,
  Window *win,
  int flags)
{
  return SharedFindWindow("SUBTLE_TRAY_LIST", match, name, win, flags);
} /* }}} */

 /** subSharedSubletFind {{{
  * @brief Find sublet
  * @param[in]     match  Sublet name or id
  * @param[inout]  name   Name of the found sublet
  * @return Returns the sublet id
  * @retval  -1   Sublet not found
  **/

int
subSharedSubletFind(char *match,
  char **name)
{
  return SharedFind("SUBTLE_SUBLET_LIST", match, name);
} /* }}} */

 /** subSharedViewFind {{{
  * @brief Find view id
  * @param[in]     match  View name or window id
  * @param[inout]  name   Name of the found view
  * @param[out]    win    View window
  * @return Returns the view list id
  * @retval  -1  View not found
  **/

int
subSharedViewFind(char *match,
  char **name,
  Window *win)
{
  int ret = -1, size = 0;
  Window *views = NULL;
  char **names = NULL;
  regex_t *preg = NULL;

  assert(match);

  /* Find view id */
  if((views = (Window *)subSharedPropertyGet(DefaultRootWindow(display), 
      XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL)) &&
      (names = subSharedPropertyStrings(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size)) &&
      (preg = subSharedRegexNew(match)))
    {
      int i;
      char buf[10] = { 0 };

      for(i = 0; i < size; i++)
        {
          snprintf(buf, sizeof(buf), "%#lx", views[i]);

          /* Find view either by name or by window id */
          if((isdigit(match[0]) && atoi(match) == i) ||
              (!isdigit(match[0]) && subSharedRegexMatch(preg, names[i])) ||
              subSharedRegexMatch(preg, buf))
            {
              subSharedLogDebug("Found: type=view, match=%s, name=%s win=%#lx, id=%d\n",
                match, names[i], views[i], i);

              if(win) *win   = views[i];
              if(name) *name = strdup(names[i]);

              ret = i;
              break;
            }
        }

      subSharedRegexKill(preg);
      XFreeStringList(names);
      free(views);
    }
  else subSharedLogDebug("Failed finding view `%s'\n", match);

  return ret;
} /* }}} */

 /** subSharedSubtleRunning {{{
  * @brief Check if subtle is running
  * @return Returns if subtle is running
  * @retval  1  subtle is running
  * @retval  0  subtle is not running
  **/

int
subSharedSubtleRunning(void)
{
  char *prop = NULL;
  Window *check = NULL;
  int ret = False;

  /* Get supporting window */
  if(display && (check = subSharedWindowWMCheck()))
    {
      subSharedLogDebug("Support: win=%#lx\n", *check);

      /* Get property */
      if((prop = subSharedPropertyGet(*check, XInternAtom(display, "UTF8_STRING", False),
        "_NET_WM_NAME", NULL)))
        {
          if(!strncmp(prop, PKG_NAME, strlen(prop))) ret = True;
          subSharedLogDebug("Running: wmname=%s\n", prop);

          free(prop);
        }

      free(check);
    }

  return ret;
} /* }}} */
#endif /* SUBTLE */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
