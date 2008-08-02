
 /**
  * @package subtle
  *
  * @file subtle remote client
  * @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <regex.h>
#include <getopt.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/cursorfont.h>

#include "config.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

static Display *disp = NULL;

/* Typedefs {{{ */
typedef union messagedata_t {
  char  b[20];
  short s[10];
  long  l[5];
} MessageData;

typedef void(*Command)(char *, char *);
/* }}} */

/* Prototypes {{{ */
void RegexKill(regex_t *preg);
/* }}} */

/* Macros  {{{ */
/* Flags */
#define GROUP_CLIENT 0    ///< Group client
#define GROUP_TAG    1    ///< Group tag
#define GROUP_VIEW   2    ///< Group view

#define ACTION_NEW   0    ///< Action new
#define ACTION_KILL  1    ///< Action kill
#define ACTION_LIST  2    ///< Action list
#define ACTION_FIND  3    ///< Action find
#define ACTION_FOCUS 4    ///< Action focus
#define ACTION_JUMP  5    ///< Action jump
#define ACTION_SHADE 6    ///< Action shade
#define ACTION_TAG   7    ///< Action tag
#define ACTION_UNTAG 8    ///< Action untag
#define ACTION_TAGS  9    ///< Action tags

/* Log */
#ifdef DEBUG
static int debug = 0;
#define Debug(...) Log(0, __FILE__, __LINE__, __VA_ARGS__);
#else
#define Debug(...)
#endif /* DEBUG */

#define Error(...) Log(1, __FILE__, __LINE__, __VA_ARGS__);
#define Warn(...) Log(2, __FILE__, __LINE__, __VA_ARGS__);
#define Assert(cond,...) if(!cond) Log(3, __FILE__, __LINE__, __VA_ARGS__);
/* }}} */

/* Log {{{ */
void
Log(int type,
  const char *file,
  int line,
  const char *format,
  ...)
{
  va_list ap;
  char buf[255];

#ifdef DEBUG
  if(!debug && !type) return;
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

/* XError {{{ */
int
XError(Display *display,
  XErrorEvent *ev)
{
#ifdef DEBUG
  if(debug) return(0);
#endif /* DEBUG */  

  if(ev->request_code != 42) /* X_SetInputFocus */
    {
      char error[255];
      XGetErrorText(display, ev->error_code, error, sizeof(error));
      Debug("%s: win=%#lx, request=%d\n", error, ev->resourceid, ev->request_code);
    }
  return(0); 
} /* }}} */

/* Alloc {{{ */
static void *
Alloc(size_t n,
  size_t size)
{
  void *mem = calloc(n, size);
  if(!mem) Error("Can't alloc memory. Exhausted?\n");
  return(mem);
}
/* }}} */

/* RegexNew {{{ */
static regex_t *
RegexNew(char *regex)
{
  int errcode;
  regex_t *preg = NULL;

  assert(regex);
  
  preg = (regex_t *)Alloc(1, sizeof(regex_t));

  /* Thread safe error handling */
  if((errcode = regcomp(preg, regex, REG_EXTENDED|REG_NOSUB|REG_ICASE)))
    {
      size_t errsize = regerror(errcode, preg, NULL, 0);
      char *errbuf = (char *)Alloc(1, errsize);

      regerror(errcode, preg, errbuf, errsize);

      Warn("Can't compile preg `%s'\n", regex);
      Debug("%s\n", errbuf);

      free(errbuf);
      RegexKill(preg);

      return(NULL);
    }
  return(preg);
} /* }}} */

/* RegexMatch {{{ */
static int
RegexMatch(regex_t *preg,
  char *string)
{
  assert(preg);

  return(!regexec(preg, string, 0, NULL, 0));
} /* }}} */

/* RegexKill {{{ */
void
RegexKill(regex_t *preg)
{
  assert(preg);

  regfree(preg);
  free(preg);
} /* }}} */

/* Message {{{ */
static void
Message(Window win,
  char *type,
  MessageData data)
{
  int i;
  XEvent ev;
  long mask = SubstructureRedirectMask|SubstructureNotifyMask;

  assert(win && type);

  /* Assemble event */
  ev.xclient.type         = ClientMessage;
  ev.xclient.serial       = 0;
  ev.xclient.send_event   = True;
  ev.xclient.message_type = XInternAtom(disp, type, False);
  ev.xclient.window       = win;
  ev.xclient.format       = 32;

  for(i = 0; i < 5; i++) ev.xclient.data.l[i] = data.l[i]; ///< Copy data

  if(!XSendEvent(disp, DefaultRootWindow(disp), False, mask, &ev)) 
    Warn("Can't send client message %s\n", type);
}
/* }}} */

/* PropertyGet {{{ */
static char *
PropertyGet(Window win,
  Atom type,
  char *name,
  unsigned long *size)
{
  unsigned long nitems, bytes;
  unsigned char *data = NULL;
  int format;
  Atom rettype, prop = XInternAtom(disp, name, False);

  assert(win && name);

  if(XGetWindowProperty(disp, win, prop, 0L, 4096, False, type, &rettype, 
    &format, &nitems, &bytes, &data) != Success)
    {
      Error("Failed to get property (%s)\n", name);
    }
  if(type != rettype)
    {
      XFree(data);
      Error("Invalid type for property (%s)\n", name);
    }
  if(size) *size = (unsigned long)(format / 8) * nitems;

  return((char *)data);
} /* }}} */

/* PropertyList {{{ */
static char **
PropertyList(char *name,
  int *size)
{
  int i, id = 0;
  char *string = NULL, **names = NULL;
  unsigned long len;

  assert(name && size);

  /* Get data */
  string = (char *)PropertyGet(DefaultRootWindow(disp), XInternAtom(disp, "UTF8_STRING", False), name, &len);

  /* @todo Convert string to names list */
  if(string)
    {
      /* Count \0 in string */
      for(i = 0; i < len; i++) if(string[i] == '\0') (*size)++;

      names  = (char **)Alloc(*size, sizeof(char *));
      names[id++] = string;

      for(i = 0; i < len; i++)
        {
          if(string[i] == '\0') 
            {
              if(id >= *size) break;
              names[id++] = string + i + 1;
            }
        }
    }
  else Error("Failed to get propery (%s)\n", name);

  return(names);
} /* }}} */

/* Select {{{ */
static Window
Select(void)
{
  int i, format, buttons = 0;
  unsigned int n;
  unsigned long *view = NULL, nitems, after;
  unsigned char *data = NULL;
  XEvent event;
  Atom type = None;
  Window win = None, frame = None, *frames = NULL, dummy, *wins = NULL;
  Cursor cursor = XCreateFontCursor(disp, XC_dotbox);

  /* Get view frame */
  view    = (unsigned long *)PropertyGet(DefaultRootWindow(disp), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  frames  = (Window *)PropertyGet(DefaultRootWindow(disp), XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);
  frame    = frames[*view];
  free(view);
  free(frames);

  if(XGrabPointer(disp, frame, False, ButtonPressMask|ButtonReleaseMask, 
    GrabModeSync, GrabModeAsync, frame, cursor, CurrentTime)) return(None);

  /* Select a window */
  while(win == None || buttons != 0)
    {
      XAllowEvents(disp, SyncPointer, CurrentTime);
      XWindowEvent(disp, frame, ButtonPressMask|ButtonReleaseMask, &event);

      switch(event.type)
        {
          case ButtonPress:
            if(win == None) win = event.xbutton.subwindow ? event.xbutton.subwindow : frame; ///< Sanitize
            buttons++;
            break;
          case ButtonRelease: if(0 < buttons) buttons--; break;
        }
      }

  /* Find children with WM_STATE atom */
  XQueryTree(disp, win, &dummy, &dummy, &wins, &n);
  for(i = 0; i < n; i++)
    {
      data = NULL;
      XGetWindowProperty(disp, wins[i], XInternAtom(disp, "WM_STATE", True), 0, 0, 
        False, AnyPropertyType, &type, &format, &nitems, &after, &data);

      if(data) XFree(data);
      if(type) 
        {
          win = wins[i];
          break;
        }
    }
  XFree(wins);

  XUngrabPointer(disp, CurrentTime);

  return(win);
} /* }}} */

/* ClientName {{{ */
static char *
ClientName(Window win)
{
  assert(win);
  
  char *name = PropertyGet(win, XA_STRING, "WM_NAME", NULL);
  return(name ? name : NULL);
}
/* }}} */

/* ClientClass {{{ */
static char *
ClientClass(Window win)
{
  assert(win);
  
  char *class = PropertyGet(win, XA_STRING, "WM_CLASS", NULL);
  return(class ? class : NULL);
}
/* }}} */

/* ClientList {{{ */
static Window *
ClientList(int *size)
{
  unsigned long len;

  assert(size);

  Window *clients = (Window *)PropertyGet(DefaultRootWindow(disp), XA_WINDOW, "_NET_CLIENT_LIST", &len);
  if(clients)
    {
      *size = len / sizeof(Window);
      return(clients);
    }
  else
    {
      *size = 0;
      Error("Failed to get client list\n");
      return(NULL);
    }
} /* }}} */

/* ClientFind {{{ */
static int
ClientFind(char *name,
  Window *win)
{
  int size = 0;
  Window *clients = NULL, selwin = None;

  assert(name);

  if(!strncmp(name, "#", 1) && win) selwin = Select(); ///< Select window
  
  clients = ClientList(&size);
  if(clients)
    {
      int i;
      char *cname = NULL, buf[10];
      regex_t *preg = RegexNew(name);

      for(i = 0; i < size; i++)
        {
          cname = ClientName(clients[i]);
          snprintf(buf, sizeof(buf), "%#lx", clients[i]);

          /* Find client either by window, name or by window id */
          if(clients[i] == selwin || RegexMatch(preg, cname) || RegexMatch(preg, buf))
            {
              Debug("Found: type=client, name=%s, win=%#lx, n=%d\n", name, clients[i], i);

              if(win) *win = clients[i];

              RegexKill(preg);
              free(clients);
              free(cname);

              return(i);
            }
          free(cname);
        }
      RegexKill(preg);
    }
  free(clients);

  Error("Can't fint client `%s'\n", name);

  return(-1); ///< Never reached
} /* }}} */

/* ClientInfo {{{ */
static void
ClientInfo(Window win)
{
  Window unused;
  int x, y;
  unsigned int width, height, border;
  unsigned long *nv = NULL, *cv = NULL, *rv = NULL;
  char *name = NULL, *cc = NULL;

  assert(win);

  name = ClientName(win);
  cc   = ClientClass(win);

  XGetGeometry(disp, win, &unused, &x, &y, &width, &height, &border, &border);

  nv = (unsigned long *)PropertyGet(DefaultRootWindow(disp), XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);
  cv = (unsigned long*)PropertyGet(win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL);
  rv = (unsigned long*)PropertyGet(DefaultRootWindow(disp), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

  printf("%#lx %c %ld %ux%u %s (%s)\n", win, (*cv == *rv ? '*' : '-'), 
    (*cv > *nv ? -1 : *cv), width, height, name, cc);

  if(name) free(name);
  if(cc) free(cc);
  if(nv) free(nv);
  if(cv) free(cv);
  if(rv) free(rv);
}
/* }}} */

/* TagFind {{{ */
static int
TagFind(char *name)
{
  int i, size = 0, tag = 0;
  char **tags = NULL;
  regex_t *preg = NULL;

  assert(name);

  preg = RegexNew(name);
  tags = PropertyList("SUBTLE_TAG_LIST", &size);

  /* Find tag id */
  for(i = 0; i < size; i++)
    if(RegexMatch(preg, tags[i]))
      {
        tag = i + 1;

        Debug("Found: type=tag, name=%s, n=%d\n", name, i);

        RegexKill(preg);
        free(tags);

        return(i);
      }

  RegexKill(preg);
  free(tags);

  Error("Cannot find tag `%s'.\n", name);

  return(-1); ///< Never reached
} /* }}} */

/* ViewFind {{{ */
static int
ViewFind(char *name,
  Window *win)
{
  int size = 0;
  Window *frames = NULL;
  char **names = NULL;

  assert(name);

  frames = (Window *)PropertyGet(DefaultRootWindow(disp), XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);
  names  = PropertyList("_NET_DESKTOP_NAMES", &size);  

  if(names)
    {
      int i;
      char buf[10];
      regex_t *preg = RegexNew(name);

      for(i = 0; i < size; i++)
        {
          snprintf(buf, sizeof(buf), "%#lx", frames[i]);

          /* Find client either by name or by window id */
          if(RegexMatch(preg, names[i]) || RegexMatch(preg, buf)) 
            {
              Debug("Found: type=view, name=%s win=%#lx, n=%d\n", name, frames[i], i);

              if(win) *win = frames[i];

              RegexKill(preg);
              free(frames);
              free(names);

              return(i);
            }
        }
      RegexKill(preg);
    }

  free(frames);
  free(names);

  Error("Can't fint view `%s'.\n", name);

  return(-1); ///< Never reached
} /* }}} */

/* ActionClientList {{{ */
static void
ActionClientList(char *arg1,
  char *arg2)
{
  int size = 0;
  Window *clients = NULL;

  Debug("%s\n", __func__);

  clients = ClientList(&size);
  if(clients)
    {
      int i;

      for(i = 0; i < size; i++) ClientInfo(clients[i]);
      free(clients);
    }
} /* }}} */

/* ActionClientFind {{{ */
static void
ActionClientFind(char *arg1,
  char *arg2)
{
  Window win;

  Assert(arg1, "Usage: %sr -c -f PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ClientFind(arg1, &win);
  ClientInfo(win);
} /* }}} */

/* ActionClientFocus {{{ */
static void
ActionClientFocus(char *arg1,
  char *arg2)
{
  Window win;
  unsigned long *cv = NULL, *rv = NULL;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -c -F CLIENT\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ClientFind(arg1, &win);
  cv = (unsigned long*)PropertyGet(win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL);
  rv = (unsigned long*)PropertyGet(DefaultRootWindow(disp), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

  if(*cv && *rv && *cv != *rv) 
    {
      Debug("Switching: active=%d, view=%d\n", *rv, *cv);
      data.l[0] = *cv;
      Message(DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", data);
    }
  else 
    {
      data.l[0] = win;
      Message(DefaultRootWindow(disp), "_NET_ACTIVE_WINDOW", data);
    }
      
  free(cv);
  free(rv);
} /* }}} */

/* ActionClientShade {{{ */
static void
ActionClientShade(char *arg1,
  char *arg2)
{
  Window win;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -c -s PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ClientFind(arg1, &win);
  data.l[0] = win;

  Message(DefaultRootWindow(disp), "_NET_WM_ACTION_SHADE", data);
} /* }}} */

/* ActionClientTag {{{ */
static void
ActionClientTag(char *arg1,
  char *arg2)
{
  int tag = 0;
  Window win;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1 && arg2, "Usage: %sr -c PATTERN -T PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ClientFind(arg1, &win);
  tag = TagFind(arg2);

  data.l[0] = win;
  data.l[1] = tag + 1;

  Message(DefaultRootWindow(disp), "SUBTLE_CLIENT_TAG", data);
} /* }}} */

/* ActionClientUntag {{{ */
static void
ActionClientUntag(char *arg1,
  char *arg2)
{
  int tag = 0;
  Window win;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1 && arg2, "Usage: %sr -c PATTERN -u PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ClientFind(arg1, &win);
  tag = TagFind(arg2);

  data.l[0] = win;
  data.l[1] = tag + 1;

  Message(DefaultRootWindow(disp), "SUBTLE_CLIENT_UNTAG", data);
} /* }}} */

/* ActionClientTags {{{ */
static void
ActionClientTags(char *arg1,
  char *arg2)
{
  int i, size = 0;
  Window win;
  char **tags = NULL;
  unsigned long *flags = NULL;

  Assert(arg1, "Usage: %sr -c PATTERN -g\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ClientFind(arg1, &win);
  flags = (unsigned long *)PropertyGet(win, XA_CARDINAL, "SUBTLE_CLIENT_TAGS", NULL);
  tags  = PropertyList("SUBTLE_TAG_LIST", &size);

  for(i = 0; i < size; i++)
    if((int)*flags & (1L << (i + 1))) printf("%s\n", tags[i]);
  
  free(flags);
  free(tags);
} /* }}} */

/* ActionClientKill {{{ */
static void
ActionClientKill(char *arg1,
  char *arg2)
{
  Window win;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -c -k PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ClientFind(arg1, &win);
  data.l[0] = win;

  Message(win, "_NET_CLOSE_WINDOW", data);
} /* }}} */

/* ActionTagNew {{{ */
static void
ActionTagNew(char *arg1,
  char *arg2)
{
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -t -n NAME\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), arg1);

  Message(DefaultRootWindow(disp), "SUBTLE_TAG_NEW", data);
} /* }}} */

/* ActionTagList {{{ */
static void
ActionTagList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  char **tags = NULL;

  Debug("%s\n", __func__);

  tags = PropertyList("SUBTLE_TAG_LIST", &size);

  for(i = 0; i < size; i++)
    printf("%s\n", tags[i]);

  free(tags);
} /* }}} */

/* ActionTagFind {{{ */
static void
ActionTagFind(char *arg1,
  char *arg2)
{
  int i, tag = -1, size_clients = 0, size_views = 0;
  char **views = NULL;
  Window *frames = NULL, *clients = NULL;
  unsigned long *flags = NULL;

  Debug("%s\n", __func__);

  /* Collect data */
  tag      = TagFind(arg1);
  clients = ClientList(&size_clients);
  views    = PropertyList("_NET_DESKTOP_NAMES", &size_views);
  frames  = (Window *)PropertyGet(DefaultRootWindow(disp), XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);

  /* Views */
  for(i = 0; i < size_views; i++)
    {
      flags = (unsigned long *)PropertyGet(frames[i], XA_CARDINAL, "SUBTLE_VIEW_TAGS", NULL);

      if((int)*flags & (1L << (tag + 1))) printf("view   - %s\n", views[i]);

      free(flags);
    }

  /* Clients */
  for(i = 0; i < size_clients; i++)
    {
      char *name = NULL, *class = NULL;

      flags = (unsigned long *)PropertyGet(clients[i], XA_CARDINAL, "SUBTLE_CLIENT_TAGS", NULL);
      name  = ClientName(clients[i]);
      class  = ClientClass(clients[i]);

      if((int)*flags & (1L << (tag + 1))) printf("client - %s (%s)\n", name, class);

      free(flags);
      free(name);
    }

  free(clients);
  free(frames);
  free(views);
} /* }}} */

/* ActionTagKill {{{ */
static void
ActionTagKill(char *arg1,
  char *arg2)
{
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -t -k PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), arg1);

  Message(DefaultRootWindow(disp), "SUBTLE_TAG_KILL", data);  
} /* }}} */

/* ActionViewNew {{{ */
static void
ActionViewNew(char *arg1,
  char *arg2)
{
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -t -n PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), arg1);

  Message(DefaultRootWindow(disp), "SUBTLE_VIEW_NEW", data);
} /* }}} */

/* ActionViewList {{{ */
static void
ActionViewList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  unsigned long *nv = NULL, *cv = NULL;
  char **views = NULL;
  Window *frames = NULL;

  Debug("%s\n", __func__);

  /* Collect data */
  nv     = (unsigned long *)PropertyGet(DefaultRootWindow(disp), XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);
  cv     = (unsigned long *)PropertyGet(DefaultRootWindow(disp), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  frames = (Window *)PropertyGet(DefaultRootWindow(disp), XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);
  views  = PropertyList("_NET_DESKTOP_NAMES", &size);

  for(i = 0; *nv && i < *nv; i++)
    printf("%#lx %c %s\n", frames[i], (i == *cv ? '*' : '-'), views[i]);

  free(nv);
  free(cv);
  free(frames);
  free(views);
} /* }}} */

/* ActionViewJump {{{ */
static void
ActionViewJump(char *arg1,
  char *arg2)
{
  int view = 0;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -v -j PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  /* Try to convert arg1 to long or to find view */
  if(!(view = atoi(arg1)))
    view = ViewFind(arg1, NULL);

  data.l[0] = view;

  Message(DefaultRootWindow(disp), "_NET_CURRENT_DESKTOP", data);
} /* }}} */

/* ActionViewTag {{{ */
static void
ActionViewTag(char *arg1,
  char *arg2)
{
  Window win;
  int tag = 0;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1 && arg2, "Usage: %sr -v PATTERN -T PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ViewFind(arg1, &win);
  tag = TagFind(arg2);

  data.l[0] = win;
  data.l[1] = tag + 1;

  Message(DefaultRootWindow(disp), "SUBTLE_VIEW_TAG", data);
} /* }}} */

/* ActionViewUntag {{{ */
static void
ActionViewUntag(char *arg1,
  char *arg2)
{
  Window win;
  int tag = 0;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1 && arg2, "Usage: %sr -v PATTERN -u PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ViewFind(arg1, &win);
  tag = TagFind(arg2);

  data.l[0] = win;
  data.l[1] = tag + 1;

  Message(DefaultRootWindow(disp), "SUBTLE_VIEW_UNTAG", data);
} /* }}} */

/* ActionViewTags {{{ */
static void
ActionViewTags(char *arg1,
  char *arg2)
{
  int i, size = 0;
  Window win;
  char **tags = NULL;
  unsigned long *flags = NULL;

  Assert(arg1, "Usage: %sr -v PATTERN -g\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  ViewFind(arg1, &win);
  flags = (unsigned long *)PropertyGet(win, XA_CARDINAL, "SUBTLE_VIEW_TAGS", NULL);
  tags  = PropertyList("SUBTLE_TAG_LIST", &size);

  for(i = 0; i < size; i++)
    if((int)*flags & (1L << (i + 1))) printf("%s\n", tags[i]);
  
  free(flags);
  free(tags);
} /* }}} */

/* ActionViewKill {{{ */
static void
ActionViewKill(char *arg1,
  char *arg2)
{
  int view;
  MessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -v -k PATTERN\n", PACKAGE_NAME);
  Debug("%s\n", __func__);

  view = ViewFind(arg1, NULL);

  data.l[0] = view;

  Message(DefaultRootWindow(disp), "SUBTLE_VIEW_KILL", data);  
} /* }}} */

/* Usage {{{ */
static void
Usage(int group)
{
  printf("Usage: %sr [OPTIONS] [GROUP] [ACTION]\n", PACKAGE_NAME);

  if(-1 == group)
    {
      printf("\nOptions:\n" \
             "  -d, --display=DISPLAY \t Connect to DISPLAY (default: $DISPLAY)\n" \
             "  -D, --debug           \t Print debugging messages\n" \
             "  -h, --help            \t Show this help and exit\n" \
             "  -V, --version         \t Show version info and exit\n" \
             "\nGroups:\n" \
             "  -c, --clients         \t Use clients group\n" \
             "  -t, --tags            \t Use tags group\n" \
             "  -v, --views           \t Use views group\n");
    }
  if(-1 == group || 0 == group)
    {
      printf("\nActions for clients:\n" \
             "  -l, --list            \t List all clients\n" \
             "  -f, --find=PATTERN    \t Find a client\n" \
             "  -F, --focus=PATTERN   \t Set focus to client\n" \
             "  -s, --shade=PATTERN   \t Shade client\n" \
             "  -T, --tag=PATTERN     \t Add tag to client\n" \
             "  -u, --untag=PATTERN   \t Remove tag from client\n" \
             "  -g, --tags            \t Show client tags\n" \
             "  -k, --kill=PATTERN    \t Kill a client\n");
    }
  if(-1 == group || 1 == group)
    {
      printf("\nActions for tags:\n" \
             "  -n, --new=NAME        \t Create new tag\n" \
             "  -l, --list            \t List all tags\n" \
             "  -f, --find            \t Find all clients/views by tag\n" \
             "  -k, --kill=PATTERN    \t Kill a tag\n");
    }
  if(-1 == group || 2 == group)
    {
      printf("\nActions for views:\n" \
             "  -n, --new=NAME        \t Create new view\n" \
             "  -l, --list            \t List all views\n" \
             "  -T, --tag=PATTERN     \t Add tag to view\n" \
             "  -u, --untag=PATTERN   \t Remove tag from view\n" \
             "  -g, --tags            \t Show view tags\n" \
             "  -k, --kill=VIEW       \t Kill a view\n");
    }
  
  printf("\nPattern:\n" \
         "  Matching clients, tags and views works either via plain, regex\n" \
         "  (see regex(7)) or window id. If a pattern matches more than once\n" \
         "  ONLY the first match will be used.\n\n" \
         "  Generally PATTERN can be '-' to read from stdin or '#' to interatively\n" \
         "  select a client window\n");

  printf("\nFormat:\n" \
         "  Client list: <window id> [-*] <view> <geometry> <name> <class>\n" \
         "  Tag    list: <name>\n" \
         "  View   list: <window id> [-*] <name>\n");
  
  printf("\nExamples:\n" \
         "  %sr -c -l             \t List all clients\n" \
         "  %sr -t -a subtle      \t Add new tag 'subtle'\n" \
         "  %sr -v subtle -T rocks\t Tag view 'subtle' with tag 'rocks'\n" \
         "  %sr -c xterm -g       \t Show tags of client 'xterm'\n" \
         "  %sr -c -f #           \t Select client and show info\n" \
         "  %sr -t -f term        \t Show every client/view tagged with 'term'\n" \
         "\nPlease report bugs to <%s>\n",
         PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME, PACKAGE_BUGREPORT);
} /* }}} */

/* Version {{{ */
static void
Version(void)
{
  printf("%sr %s - Copyright (c) 2005-2008 Christoph Kappel\n" \
          "Released under the GNU General Public License\n" \
          "Compiled for X%d\n", PACKAGE_NAME, PACKAGE_VERSION, X_PROTOCOL);
}
/* }}} */

/* Signal {{{ */
static void
Signal(int signum)
{
#ifdef HAVE_EXECINFO_H
  void *array[10];
  size_t size;
#endif /* HAVE_EXECINFO_H */

  switch(signum)
    {
      case SIGTERM:
      case SIGINT: 
        exit(1);
      case SIGSEGV: 
#ifdef HAVE_EXECINFO_H
        size = backtrace(array, 10);

        printf("Last %zd stack frames:\n", size);
        backtrace_symbols_fd(array, size, 0);
#endif /* HAVE_EXECINFO_H */

        printf("Please report this bug to <%s>\n", PACKAGE_BUGREPORT);
        abort();
    }
} /* }}} */

/* Pipe {{{ */
static char *
Pipe(char *string)
{
  char buf[256], *ret = NULL;

  assert(string);

  if(!strncmp(string, "-", 1)) 
    {
      if(!fgets(buf, sizeof(buf), stdin)) Error("Can't read from pipe\n");
      ret = (char *)Alloc(strlen(buf), sizeof(char));
      strncpy(ret, buf, strlen(buf) - 1);
      Debug("Pipe: len=%d\n", strlen(buf));
    }
  else ret = strdup(string);
  
  return(ret);
} /* }}} */

/* main {{{ */
int
main(int argc,
  char *argv[])
{
  int c, group = -1, action = -1;
  char *display = NULL, *arg1 = NULL, *arg2 = NULL;
  struct sigaction act;
  static struct option long_options[] =
  {
    /* Groups */
    { "clients",    no_argument,        0,  'c'  },
    { "tags",       no_argument,        0,  't'  },
    { "views",      no_argument,        0,  'v'  },

    /* Actions */
    { "new",        no_argument,        0,  'n'  },  
    { "kill",       no_argument,        0,  'k'  },
    { "list",       no_argument,        0,  'l'  },
    { "find",       no_argument,        0,  'f'  },
    { "focus",      no_argument,        0,  'F'  },
    { "jump",       no_argument,        0,  'j'  },
    { "shade",      no_argument,        0,  's'  },
    { "tag",        no_argument,        0,  'T'  },
    { "untag",      no_argument,        0,  'u'  },
    { "tags",       no_argument,        0,  'g'  },

    /* Default */
#ifdef DEBUG
    { "debug",      no_argument,        0,  'D'  },
#endif /* DEBUG */
    { "display",    required_argument,  0,  'd'  },
    { "help",       no_argument,        0,  'h'  },
    { "version",    no_argument,        0,  'V'  },
    { 0, 0, 0, 0}
  };

  /* Command table */
  Command cmds[3][10] = { /* Client, Tag, View <=> New, Kill, List, Find, Focus, Jump, Shade, Tag, Untag, Tags */
    { NULL, ActionClientKill, ActionClientList, ActionClientFind, ActionClientFocus, NULL, ActionClientShade, ActionClientTag, ActionClientUntag, ActionClientTags },
    { ActionTagNew, ActionTagKill, ActionTagList, ActionTagFind, NULL, NULL, NULL, NULL, NULL, NULL },
    { ActionViewNew, ActionViewKill, ActionViewList, NULL, NULL, ActionViewJump, NULL, ActionViewTag, ActionViewUntag, ActionViewTags }
  };

  /* Set up signal handler */
  act.sa_handler  = Signal;
  act.sa_flags    = 0;
  memset(&act.sa_mask, 0, sizeof(sigset_t)); ///< Avoid uninitialized values
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGSEGV, &act, NULL);

  while((c = getopt_long(argc, argv, "ctvnkfFjlsTugDd:hV", long_options, NULL)) != -1)
    {
      switch(c)
        {
          case 'c': group   = GROUP_CLIENT;  break;
          case 't': group   = GROUP_TAG;     break;
          case 'v': group   = GROUP_VIEW;    break;

          case 'n': action  = ACTION_NEW;    break;
          case 'k': action  = ACTION_KILL;   break;
          case 'l': action  = ACTION_LIST;   break;
          case 'f': action  = ACTION_FIND;   break;
          case 'F': action  = ACTION_FOCUS;  break;
          case 'j': action  = ACTION_JUMP;   break;
          case 's': action  = ACTION_SHADE;  break;
          case 'T': action  = ACTION_TAG;    break;
          case 'u': action  = ACTION_UNTAG;  break;
          case 'g': action  = ACTION_TAGS;   break;

          case 'd': display = optarg;        break;
          case 'h': Usage(group);            return(0);
#ifdef DEBUG          
          case 'D': debug = 1;               break;
#endif /* DEBUG */
          case 'V': Version();               return(0);
          case '?':
            printf("Try `%sr --help for more information\n", PACKAGE_NAME);
            return(-1);
        }
    }

  /* Check command */
  if(-1 == group || -1 == action)
    {
      Usage(group);
      return(0);
    }
  
  /* Get arguments */
  if(argc > optind) arg1 = Pipe(argv[optind]);
  if(argc > optind + 1) arg2 = Pipe(argv[optind + 1]);

  /* Open connection to server */
  if(!(disp = XOpenDisplay(display)))
    {
      printf("Can't open display `%s'.\n", (display) ? display : ":0.0");
      return(-1);
    }
  XSetErrorHandler(XError);

  /* Check command */
  if(cmds[group][action]) cmds[group][action](arg1, arg2);
  else Usage(group);

  XCloseDisplay(disp);
  if(arg1) free(arg1);
  if(arg2) free(arg2);
  
  return(0);
} /* }}} */
