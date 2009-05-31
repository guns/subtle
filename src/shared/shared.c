
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
#include <signal.h>
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
  if((!subtle && !type) || (subtle && !subtle->debug && !type)) return;
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
      case 1: fprintf(stderr, "<ERROR> %s", buf); raise(SIGTERM);     break;
      case 2: fprintf(stdout, "<WARNING> %s", buf);                   break;
      case 3: fprintf(stderr, "%s", buf); raise(SIGTERM);             break;
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
  if(subtle->debug) return 0;
  if(BadAccess == ev->error_code && DefaultRootWindow(disp) == ev->resourceid)
    {
      subSharedLogError("Failed taking over display. Another WM running - exiting.\n");
    }
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

#define MATCH(win,gravity1,gravity2,score,match,found) \
  if(gravity1 == gravity2 && score > *match) \
    { \
      *match = score; \
      *found = win; \
    } \

 /** subSharedMatch {{{
  * @brief Match a window based on neighbourship
  * @param[in]     type      Type of neighbourship
  * @param[in]     win       Window
  * @param[in]     gravity1  Gravity 1
  * @param[in]     gravity2  Gravity 2
  * @param[inout]  match     Match score
  * @param[inout]  found     Found window
  **/

void
subSharedMatch(int type,
  Window win,
  int gravity1,
  int gravity2,
  int *match,
  Window *found)
{
  switch(type)
    {
      case SUB_WINDOW_UP: /* {{{ */
        switch(gravity1)
          {
            case SUB_GRAVITY_BOTTOM:
              MATCH(win, gravity2, SUB_GRAVITY_TOP, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_LEFT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_RIGHT, 75, match, found);
              break;            
            case SUB_GRAVITY_BOTTOM_LEFT:
              MATCH(win, gravity2, SUB_GRAVITY_TOP, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_LEFT, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_RIGHT, 75, match, found);
              break;            
            case SUB_GRAVITY_BOTTOM_RIGHT:
              MATCH(win, gravity2, SUB_GRAVITY_TOP, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_LEFT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_RIGHT, 100, match, found);
              break;
          }
        break; /* }}} */
      case SUB_WINDOW_LEFT: /* {{{ */
        switch(gravity1)
          {
            case SUB_GRAVITY_TOP_RIGHT:
              MATCH(win, gravity2, SUB_GRAVITY_LEFT, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_LEFT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_LEFT, 50, match, found);
              break;
            case SUB_GRAVITY_BOTTOM_RIGHT:
              MATCH(win, gravity2, SUB_GRAVITY_LEFT, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_LEFT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_LEFT, 50, match, found);
              break;
            case SUB_GRAVITY_RIGHT:
              MATCH(win, gravity2, SUB_GRAVITY_LEFT, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_LEFT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_LEFT, 50, match, found);
              break;                                    
          }
        break; /* }}} */
      case SUB_WINDOW_RIGHT: /* {{{ */
        switch(gravity1)
          {
            case SUB_GRAVITY_TOP_LEFT:
              MATCH(win, gravity2, SUB_GRAVITY_RIGHT, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_RIGHT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_RIGHT, 50, match, found);
              break;
            case SUB_GRAVITY_BOTTOM_LEFT:
              MATCH(win, gravity2, SUB_GRAVITY_RIGHT, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_RIGHT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_RIGHT, 50, match, found);
              break;
            case SUB_GRAVITY_LEFT:
              MATCH(win, gravity2, SUB_GRAVITY_RIGHT, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_TOP_RIGHT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_RIGHT, 50, match, found);
              break;
          }
        break; /* }}} */
      case SUB_WINDOW_DOWN: /* {{{ */
        switch(gravity1)
          {
            case SUB_GRAVITY_TOP:
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_LEFT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_RIGHT, 75, match, found);
              break;            
            case SUB_GRAVITY_TOP_LEFT:
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_LEFT, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_RIGHT, 75, match, found);
              break;            
            case SUB_GRAVITY_TOP_RIGHT:
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM, 100, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_LEFT, 75, match, found);
              MATCH(win, gravity2, SUB_GRAVITY_BOTTOM_RIGHT, 100, match, found);
              break;
          }
        break; /* }}} */                        
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

  return XCNOENT != XFindContext(subtle->disp, win, id, (XPointer *)&data) ? data : NULL;
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

  XUnmapWindow(subtle->disp, subtle->windows.caption); 

   /* Focus */
  XQueryPointer(subtle->disp, ROOT, (Window *)&dummy, &win,
    &dummy, &dummy, &dummy, &dummy, (unsigned int *)&dummy);

  if((c = CLIENT(subSharedFind(win, CLIENTID)))) subClientFocus(c);
  else XSetInputFocus(subtle->disp, ROOT, RevertToNone, CurrentTime);
} /* }}} */
#endif /* WM */

#ifndef WM
 /** subSharedMessage {{{
  * @brief Send client message to window
  * @param[in]  win   Client window
  * @param[in]  type  Message type 
  * @param[in]  data  A #SubMessageData
  * @param[in]  sync  Sync connection
  * @returns
  **/

int
subSharedMessage(Window win,
  char *type,
  SubMessageData data,
  int sync)
{
  int status = 0;
  XEvent ev;
  long mask = SubstructureRedirectMask|SubstructureNotifyMask;

  assert(win && type);

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

  if(!((status = XSendEvent(display, DefaultRootWindow(display), False, mask, &ev))))
    subSharedLogWarn("Failed sending client message `%s'\n", type);

  if(True == sync) XSync(display, False);

  return status;
} /* }}} */

 /** subSharedPropertyGet {{{
  * @brief Get window property
  * @param[in]  win   Client window
  * @param[in]  type  Property type 
  * @param[in]  size  Size of the property
  * return Returns the property
  **/

char *
subSharedPropertyGet(Window win,
  Atom type,
  char *name,
  unsigned long *size)
{
  unsigned long nitems = 0, bytes = 0;
  unsigned char *data = NULL;
  int format = 0;
  Atom rtype = None, prop = None;

  assert(win && name);

  prop = XInternAtom(display, name, False);

  if(Success != XGetWindowProperty(display, win, prop, 0L, 4096, False, type, &rtype,
    &format, &nitems, &bytes, &data))
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
  if(size) *size = (unsigned long)(format / 8) * nitems;

  return (char *)data;
} /* }}} */

 /** subSharedPropertyList {{{
  * @brief Get property list
  * @param[in]    win   Client window
  * @param[in]    name  Property name
  * @param[inout]  size  Size of the property list
  * return Returns the property list
  **/

char **
subSharedPropertyList(Window win,
  char *name,
  int *size)
{
  int i, pos = 0, idx = 0;
  char *string = NULL, **names = NULL;
  unsigned long len;

  assert(name && size);

  /* Get data */
  if((string = (char *)subSharedPropertyGet(win, 
    XInternAtom(display, "UTF8_STRING", False), name, &len)))
    {
      /* Count \0 in string */
      for(i = 0; i < len; i++) 
        if(string[i] == '\0') (*size)++;

      names = (char **)subSharedMemoryAlloc(*size, sizeof(char *));

      for(i = 0; i < len; i++)
        {
          if(string[i] == '\0') 
            {
              if(idx >= *size) break;

              names[idx] = (char *)subSharedMemoryAlloc(i + 1 - pos, sizeof(char));
              strncpy(names[idx], string + pos, i + 1 - pos);

              idx++;
              pos = i + 1;
            }
        }

      free(string);

      return names;
    }
  
  subSharedLogDebug("Failed getting propery (%s)\n", name);

  return NULL;
} /* }}} */

 /** subSharedPropertyListFree {{{
  * @brief Free property list
  * @param[inout]  list  Property list
  * @param[in]     size  Size of the property list
  * return Returns the property list
  **/

void
subSharedPropertyListFree(char **list,
  int size)
{
  int i;

  assert(list);

  for(i = 0; i < size; i++)
    free(list[i]);

  free(list);
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

 /** subSharedWindowWMName {{{
  * @brief Get window name
  * @warning Must be free'd
  * @param[in]  win   Client window
  * return Returns the name of the window
  * @retval  NULL  Property not found
  **/

char *
subSharedWindowWMName(Window win)
{
  assert(win);
  
  char *wmname = subSharedPropertyGet(win, XA_STRING, "WM_NAME", NULL);
  return wmname ? wmname : NULL;
} /* }}} */

 /** subSharedWindowWMClass {{{
  * @brief Get window class
  * @warning Must be free'd
  * @param[in]  win   Client window
  * return Returns the class of the window
  * @retval  NULL  Property not found
  **/

char *
subSharedWindowWMClass(Window win)
{
  assert(win);
  
  char *wmclass = subSharedPropertyGet(win, XA_STRING, "WM_CLASS", NULL);
  return wmclass ? wmclass : NULL;
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
  unsigned long len;

  assert(size);

  clients = (Window *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_CLIENT_LIST", &len);
  if(clients)
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

 /** subSharedClientFind {{{
  * @brief Find client id
  * @param[in]   name  Client name
  * @param[out]  win   Client window
  * @return Returns the client window list id
  * @retval  -1   Client not found
  **/

int
subSharedClientFind(char *name,
  Window *win)
{
  int size = 0;
  Window *clients = NULL, selwin = None;

  assert(name);

  if(!strncmp(name, "#", 1) && win) selwin = subSharedWindowSelect(); ///< Select window
  
  if((clients = subSharedClientList(&size)))
    {
      int i;
      char *wmname = NULL, *wmclass = NULL, buf[10];
      regex_t *preg = subSharedRegexNew(name);

      for(i = 0; i < size; i++)
        {
          wmname  = subSharedWindowWMName(clients[i]);
          wmclass = subSharedWindowWMClass(clients[i]);
          snprintf(buf, sizeof(buf), "%#lx", clients[i]);

          /* Find client either by window id or by wmname */
          if(clients[i] == selwin || (wmname && subSharedRegexMatch(preg, wmname)) ||
            (wmclass && subSharedRegexMatch(preg, wmclass)) || subSharedRegexMatch(preg, buf))
            {
              subSharedLogDebug("Found: type=client, name=%s, win=%#lx, id=%d\n", name,
                clients[i], i);

              if(win) *win = clients[i];

              subSharedRegexKill(preg);
              free(clients);
              free(wmname);
              free(wmclass);

              return i;
            }
          free(wmname);
          free(wmclass);
        }
      subSharedRegexKill(preg);
    }
  free(clients);

  subSharedLogDebug("Failed finding client `%s'\n", name);

  return -1;
} /* }}} */

 /** subSharedTagFind {{{
  * @brief Find tag id
  * @param[in]   name  Tag name
  * @return Returns the tag list id
  * @retval  -1   Tag not found
  **/

int
subSharedTagFind(char *name)
{
  int i, size = 0;
  char **tags = NULL;
  regex_t *preg = NULL;

  assert(name);

  preg = subSharedRegexNew(name);
  tags = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

  /* Find tag id */
  for(i = 0; i < size; i++)
    if(subSharedRegexMatch(preg, tags[i]))
      {
        subSharedLogDebug("Found: type=tag, name=%s, id=%d\n", name, i);

        subSharedRegexKill(preg);
        subSharedPropertyListFree(tags, size);

        return i;
      }

  subSharedRegexKill(preg);
  subSharedPropertyListFree(tags, size);

  subSharedLogDebug("Failed finding tag `%s'\n", name);

  return -1;
} /* }}} */

 /** subSharedViewFind {{{
  * @brief Find view id
  * @param[in]   name  View name
  * @param[out]  win   View window
  * @return Returns the view list id
  * @retval  -1   View not found
  **/

int
subSharedViewFind(char *name,
  Window *win)
{
  int size = 0;
  Window *views = NULL, root = DefaultRootWindow(display);
  char **names = NULL;

  assert(name);

  views = (Window *)subSharedPropertyGet(root, XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);
  names = subSharedPropertyList(root, "_NET_DESKTOP_NAMES", &size);  

  if(names)
    {
      int i;
      char buf[10];
      regex_t *preg = subSharedRegexNew(name);

      for(i = 0; i < size; i++)
        {
          snprintf(buf, sizeof(buf), "%#lx", views[i]);

          /* Find view either by name or by window id */
          if(subSharedRegexMatch(preg, names[i]) || subSharedRegexMatch(preg, buf))
            {
              subSharedLogDebug("Found: type=view, name=%s win=%#lx, id=%d\n",
                name, views[i], i);

              if(win) *win = views[i];

              subSharedRegexKill(preg);
              subSharedPropertyListFree(names, size);
              free(views);

              return i;
            }
        }
      subSharedRegexKill(preg);
    }

  subSharedPropertyListFree(names, size);
  free(views);

  subSharedLogDebug("Failed finding view `%s'\n", name);

  return -1;
} /* }}} */

 /** subSharedSubletFind {{{
  * @brief Find sublet
  * @param[in]   name  Sublet name
  * @return Returns the sublet list id
  * @retval  -1   Sublet not found
  **/

int
subSharedSubletFind(char *name)
{
  int i, size = 0;
  char **sublets = NULL;
  regex_t *preg = NULL;

  assert(name);

  preg    = subSharedRegexNew(name);
  sublets = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_SUBLET_LIST", &size);

  /* Find sublet id */
  for(i = 0; i < size; i++)
    if(subSharedRegexMatch(preg, sublets[i]))
      {
        subSharedLogDebug("Found: type=sublet, name=%s, id=%d\n", name, i);

        subSharedRegexKill(preg);
        free(sublets);

        return i;
      }

  subSharedRegexKill(preg);
  subSharedPropertyListFree(sublets, size);

  subSharedLogDebug("Failed finding sublet `%s'\n", name);

  return -1;
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
  if((check = subSharedWindowWMCheck()))
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
