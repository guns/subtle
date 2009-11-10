
 /**
  * @package subtle
  *
  * @file Shared functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
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
#ifdef WM
  if((!subtle && !type) || (subtle && 
    !(subtle->flags & SUB_SUBTLE_DEBUG) && !type)) return;
#else  /* WM */
  if(!debug && !type) return;
#endif /* WM */
#endif /* DEBUG */

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  switch(type)
    {
#ifdef DEBUG
      case 0: fprintf(stderr, "<DEBUG> %s:%d: %s", file, line, buf);  break;
#endif /* DEBUG */
      case 1: fprintf(stderr, "<ERROR> %s", buf); raise(SIGINT);      break;
      case 2: fprintf(stdout, "<WARNING> %s", buf);                   break;
      case 3: fprintf(stderr, "%s", buf); raise(SIGINT);              break;
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
#ifdef WM
  if(!(subtle->flags & SUB_SUBTLE_DEBUG)) return 0;
#else /* WM */
  if(debug) return 0;
#endif /* WM */
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
  * @brief Match a window based on neighbourship
  * @param[in]     type      Type of neighbourship
  * @param[in]     gravity1  Gravity 1
  * @param[in]     gravity2  Gravity 2
  **/

int
subSharedMatch(int type,
  int gravity1,
  int gravity2)
{
  int score = 0;

  /* Matching is a bit annoying, doing calculations on the numpad */
  switch(gravity2 - gravity1)
    {
      case -1: score = 40; break;
      case  1: score = 40; break;

      case -2: if(SUB_WINDOW_LEFT == type)  score =  80; break;
      case  2: if(SUB_WINDOW_RIGHT == type) score =  80; break;

      case -3: if(SUB_WINDOW_DOWN == type)  score = 100; break;
      case  3: if(SUB_WINDOW_UP == type)    score = 100; break;

      case -5: score = 60; break;
      case  5: score = 60; break;

      case -6: if(SUB_WINDOW_DOWN == type)  score =  80; break;
      case  6: if(SUB_WINDOW_UP == type)    score =  80; break;

      default: score = 10;
    }

  return score;
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
#ifdef WM
  SubEwmh e,
#else /* WM */
  char *name,
#endif /* WM */
  unsigned long *size)
{
  unsigned long nitems = 0, bytes = 0;
  unsigned char *data = NULL;
  int format = 0;
  Atom rtype = None, prop = None;
  Display *disp = NULL;

#ifdef WM
  char *name = "subtle";
#endif /* WM */

  assert(win);

#ifdef WM
  disp = subtle->dpy; 
  prop = subEwmhGet(e);
#else /* WM */
  disp = display;
  prop = XInternAtom(display, name, False);
#endif /* WM */

  if(Success != XGetWindowProperty(disp, win, prop, 0L, 4096, 
    False, type, &rtype, &format, &nitems, &bytes, &data))
    {
      subSharedLogWarn("Failed getting property `%s'\n", name);

      return NULL;
    }

  if(type != rtype)
    {
      subSharedLogDebug("Property: %s => %ld != %ld'\n", name, type, rtype);
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
#ifdef WM
  SubEwmh e,
#else /* WM */
  char *name,
#endif /* WM */
  int *size)
{
  Atom atom;
  char **list = NULL;
  XTextProperty text;
  Display *disp = NULL;

  assert(win && size);

  /* Check UTF8 and XA_STRING */
#ifdef WM
  disp = subtle->dpy;
  atom = subEwmhGet(e);
#else /* WM */
  disp = display;
  atom = XInternAtom(display, name, False);
#endif /* WM */ 
 
  if((XGetTextProperty(disp, win, &text, atom) || 
    XGetTextProperty(disp, win, &text, XA_STRING)) && text.nitems)
    {
      XTextPropertyToStringList(&text, &list, size);

      XFree(text.value);
    }

  return list;
} /* }}} */

 /** subSharedPropertyClass {{{
  * @brief Get window class
  * @warning Must be free'd
  * @param[in]     win    Client window
  * @param[inout]  inst   Client instance name
  * @param[inout]  klass  Client class name
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
#ifdef WM
    SUB_EWMH_WM_CLASS,
#else /* WM */    
    "WM_CLASS",
#endif /* WM */    
    &size);

  /* Sanitize instance/class names */
  if(inst)  *inst  = strdup(0 < size ? klasses[0] : "subtle");
  if(klass) *klass = strdup(1 < size ? klasses[1] : "subtle");

  if(klasses) XFreeStringList(klasses);
} /* }}} */

 /** subSharedPropertyDelete {{{
  * @brief Get window property
  * @param[in]  win   Client window
  * @param[in]  name  Property name
  * @param[in]  e     A #SubEwmh
  * return Deletes the property
  **/

void 
subSharedPropertyDelete(Window win, 
#ifdef WM
  SubEwmh e)
#else /* WM */
  char *name)
#endif /* WM */
{
#ifdef WM
  XDeleteProperty(subtle->dpy, win, subEwmhGet(e));
#else /* WM */
  XDeleteProperty(display, win, XInternAtom(display, name, False));
#endif /* WM */
} /* }}} */

 /** subSharedSpawn {{{
  * @brief Spawn a command
  * @param[in]  cmd  Command string
  **/

void
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
} /* }}} */

#ifdef WM
 /** subSharedFind {{{
  * @brief Find data with the context manager
  * @param[in]  win  Window
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
  **/

void
subSharedFocus(void)
{
  int dummy;
  Window win;
  SubClient *c = NULL;

  /* Focus */
  XQueryPointer(subtle->dpy, ROOT, (Window *)&dummy, &win,
    &dummy, &dummy, &dummy, &dummy, (unsigned int *)&dummy);

  if((c = CLIENT(subSharedFind(win, CLIENTID)))) subClientFocus(c);
  else XSetInputFocus(subtle->dpy, ROOT, RevertToNone, CurrentTime);
} /* }}} */

 /** subSharedTextWidth {{{
  * @brief Get width of the smallest enclosing box
  * @param[in]     string  The string
  * @param[in]     len     Length of the string
  * @param[inout]  left    Left bearing
  * @param[inout]  right   Right bearing
  * @param[in]     center  Center text
  * @return Width of the box
  **/

int
subSharedTextWidth(const char *string,
  int len,
  int *left,
  int *right,
  int center)
{
  int direction = 0, ascent = 0, descent = 0, lbearing = 0, rbearing = 0;
  XCharStruct overall;

  assert(string);

  XTextExtents(subtle->xfs, string, len, &direction, &ascent, &descent, &overall);

  /* Get bearings */
  lbearing = overall.lbearing;
  rbearing = overall.width - overall.rbearing;

  if(left)  *left  = lbearing;
  if(right) *right = rbearing;

  return center ? overall.width - abs(lbearing - rbearing) : overall.width;
} /* }}} */

#else /* WM */

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
  Window *clients = NULL;
  unsigned long len = 0;

  assert(size);

  if((clients = (Window *)subSharedPropertyGet(DefaultRootWindow(display),
      XA_WINDOW, "_NET_CLIENT_LIST", &len)))
    {
      *size = len / sizeof(Window);
    }
  else
    {
      *size = 0;
      subSharedLogDebug("Failed getting client list\n");
    }

  return clients;
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
  Window *trays = NULL;
  unsigned long len = 0;

  assert(size);

  if((trays = (Window *)subSharedPropertyGet(DefaultRootWindow(display),
      XA_WINDOW, "SUBTLE_TRAY_LIST", &len)))
    {
      *size = len / sizeof(Window);
    }
  else
    {
      *size = 0;
      subSharedLogDebug("Failed getting tray list\n");
    }

  return trays;
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
  int id = -1, size = 0;
  Window *clients = NULL;

  assert(match);
  
  /* Find client id */
  if((clients = subSharedClientList(&size)))
    {
      int i;
      char *title = NULL, *inst = NULL, *klass = NULL, buf[20] = { 0 };
      Window selwin = None;
      regex_t *preg = subSharedRegexNew(match);

      if(!strncmp(match, "#", 1) && win)
        selwin = subSharedWindowSelect(); ///< Select window

      for(i = 0; -1 == id && i < size; i++)
        {
          XFetchName(display, clients[i], &title);
          subSharedPropertyClass(clients[i], &inst, &klass);
          snprintf(buf, sizeof(buf), "%#lx", clients[i]);

          /* Find client either by window id or by title/inst/class */
          if(clients[i] == selwin || subSharedRegexMatch(preg, buf) ||
              (flags & SUB_MATCH_TITLE && title && subSharedRegexMatch(preg, title)) ||
              (flags & SUB_MATCH_NAME  && inst  && subSharedRegexMatch(preg, inst))  ||
              (flags & SUB_MATCH_CLASS && klass && subSharedRegexMatch(preg, klass)))
            {
              subSharedLogDebug("Found: type=client, name=%s, win=%#lx, id=%d, flags\n", 
                match, clients[i], i, flags);

              if(win) *win = clients[i];
              if(name)
                {
                  *name = (char *)subSharedMemoryAlloc(strlen(inst) + 1, sizeof(char));
                  strncpy(*name, inst, strlen(inst));
                 }
              
              id = i;
            }

          XFree(title);
          free(inst);
          free(klass);
        }

      subSharedRegexKill(preg);
      free(clients);
    }
  else subSharedLogDebug("Failed finding client `%s'\n", match);

  return id;
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
  int ret = -1, size = 0;
  char **tags = NULL;
  regex_t *preg = NULL;

  assert(match);

  /* Find tag id */
  if((preg = subSharedRegexNew(match)) &&
      (tags = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size)))
    {
      int i;

      for(i = 0; i < size; i++)
        {
          if((isdigit(match[0]) && atoi(match) == i) || 
              (!isdigit(match[0]) && subSharedRegexMatch(preg, tags[i])))
            {
              subSharedLogDebug("Found: type=tag, match=%s, name=%s, id=%d\n", 
                match, tags[i], i);

              if(name) *name = strdup(tags[i]);

              ret = i;
              break;
            }
        }

      subSharedRegexKill(preg);
      XFreeStringList(tags);
    }
  else subSharedLogDebug("Failed finding tag `%s'\n", match);

  return ret;
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
  int id = -1, size = 0;
  Window *trays = NULL;

  assert(match);
  
  /* Find tray id */
  if((trays = subSharedTrayList(&size)))
    {
      int i;
      char *title = NULL, *inst = NULL, *klass = NULL, buf[20] = { 0 };
      Window selwin = None;
      regex_t *preg = subSharedRegexNew(match);

      if(!strncmp(match, "#", 1) && win)
        selwin = subSharedWindowSelect(); ///< Select window

      for(i = 0; -1 == id && i < size; i++)
        {
          XFetchName(display, trays[i], &title);
          subSharedPropertyClass(trays[i], &inst, &klass);
          snprintf(buf, sizeof(buf), "%#lx", trays[i]);

          /* Find client either by window id or by title/inst/class */
          if(trays[i] == selwin || subSharedRegexMatch(preg, buf) ||
              (flags & SUB_MATCH_TITLE && title && subSharedRegexMatch(preg, title)) ||
              (flags & SUB_MATCH_NAME  && inst  && subSharedRegexMatch(preg, inst))  ||
              (flags & SUB_MATCH_CLASS && klass && subSharedRegexMatch(preg, klass)))
            {
              subSharedLogDebug("Found: type=tray, name=%s, win=%#lx, id=%d, flags\n", 
                match, trays[i], i, flags);

              if(win) *win = trays[i];
              if(name)
                {
                  *name = (char *)subSharedMemoryAlloc(strlen(inst) + 1, sizeof(char));
                  strncpy(*name, inst, strlen(inst));
                 }
              
              id = i;
            }

          XFree(title);
          free(inst);
          free(klass);
        }

      subSharedRegexKill(preg);
      free(trays);
    }
  else subSharedLogDebug("Failed finding tray `%s'\n", match);

  return id;
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
  int ret = -1, size = 0;
  char **sublets = NULL;
  regex_t *preg = NULL;

  assert(match);

  /* Find sublet id */
  if((preg = subSharedRegexNew(match)) &&
      (sublets = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_SUBLET_LIST", &size)))
    {
      int i;

      for(i = 0; i < size; i++)
        if((isdigit(match[0]) && atoi(match) == i) || 
            (!isdigit(match[0]) && subSharedRegexMatch(preg, sublets[i])))
          {
            subSharedLogDebug("Found: type=sublet, match=%s, name=%s, id=%d\n", 
              match, sublets[i], i);

            if(name) *name = strdup(sublets[i]);

            ret = i;
            break;
          }

      subSharedRegexKill(preg);
      XFreeStringList(sublets);
    }
  else subSharedLogDebug("Failed finding sublet `%s'\n", match);

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
#endif /* WM */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
