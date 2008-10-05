
 /**
  * @package subtle
  *
  * @file subtle remote client
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "config.h"
#include "shared.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

Display *display = NULL;
int debug = 0;

/* Typedefs {{{ */
typedef void(*Command)(char *, char *);
/* }}} */

/* Macros {{{ */
#define Assert(cond,...) if(!cond) subSharedLog(3, __FILE__, __LINE__, __VA_ARGS__);
/* }}} */

/* Flags {{{ */
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
/* }}} */

/* ClientInfo {{{ */
static void
ClientInfo(Window win)
{
  Window unused;
  int x, y;
  unsigned int width, height, border;
  unsigned long *nv = NULL, *cv = NULL, *rv = NULL;
  char *wmname = NULL, *wmclass = NULL;

  assert(win);

  wmname  = subSharedWindowWMName(win);
  wmclass = subSharedWindowWMClass(win);

  XGetGeometry(display, win, &unused, &x, &y, &width, &height, &border, &border);

  nv = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display), XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);
  cv = (unsigned long*)subSharedPropertyGet(win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL);
  rv = (unsigned long*)subSharedPropertyGet(DefaultRootWindow(display), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

  printf("%#lx %c %ld %ux%u %s (%s)\n", win, (*cv == *rv ? '*' : '-'), 
    (*cv > *nv ? -1 : *cv), width, height, wmname, wmclass);

  if(wmname) free(wmname);
  if(wmclass) free(wmclass);
  if(nv) free(nv);
  if(cv) free(cv);
  if(rv) free(rv);
} /* }}} */

/* ActionClientList {{{ */
static void
ActionClientList(char *arg1,
  char *arg2)
{
  int size = 0;
  Window *clients = NULL;

  subSharedLogDebug("%s\n", __func__);

  clients = subSharedClientList(&size);
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

  Assert(arg1, "Usage: %sr -c -f PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedClientFind(arg1, &win);
  ClientInfo(win);
} /* }}} */

/* ActionClientFocus {{{ */
static void
ActionClientFocus(char *arg1,
  char *arg2)
{
  Window win;
  unsigned long *cv = NULL, *rv = NULL;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -c -F CLIENT\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedClientFind(arg1, &win);
  cv = (unsigned long*)subSharedPropertyGet(win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL);
  rv = (unsigned long*)subSharedPropertyGet(DefaultRootWindow(display), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

  if(*cv && *rv && *cv != *rv) 
    {
      subSharedLogDebug("Switching: active=%d, view=%d\n", *rv, *cv);
      data.l[0] = *cv;
      subSharedMessage(DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", data);
    }
  else 
    {
      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data);
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
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -c -s PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedClientFind(arg1, &win);
  data.l[0] = win;

  subSharedMessage(DefaultRootWindow(display), "_NET_WM_ACTION_SHADE", data);
} /* }}} */

/* ActionClientTag {{{ */
static void
ActionClientTag(char *arg1,
  char *arg2)
{
  int tag = 0;
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1 && arg2, "Usage: %sr -c PATTERN -T PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedClientFind(arg1, &win);
  tag = subSharedTagFind(arg2);

  data.l[0] = win;
  data.l[1] = tag + 1;

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_CLIENT_TAG", data);
} /* }}} */

/* ActionClientUntag {{{ */
static void
ActionClientUntag(char *arg1,
  char *arg2)
{
  int tag = 0;
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1 && arg2, "Usage: %sr -c PATTERN -u PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedClientFind(arg1, &win);
  tag = subSharedTagFind(arg2);

  data.l[0] = win;
  data.l[1] = tag + 1;

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_CLIENT_UNTAG", data);
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

  Assert(arg1, "Usage: %sr -c PATTERN -g\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedClientFind(arg1, &win);
  flags = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, "SUBTLE_CLIENT_TAGS", NULL);
  tags  = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

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
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -c -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedClientFind(arg1, &win);
  data.l[0] = win;

  subSharedMessage(win, "_NET_CLOSE_WINDOW", data);
} /* }}} */

/* ActionTagNew {{{ */
static void
ActionTagNew(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -t -n NAME\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), arg1);

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data);
} /* }}} */

/* ActionTagList {{{ */
static void
ActionTagList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  char **tags = NULL;

  subSharedLogDebug("%s\n", __func__);

  tags = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

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

  subSharedLogDebug("%s\n", __func__);

  /* Collect data */
  tag     = subSharedTagFind(arg1);
  clients = subSharedClientList(&size_clients);
  views   = subSharedPropertyList(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size_views);
  frames  = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);

  /* Views */
  for(i = 0; i < size_views; i++)
    {
      flags = (unsigned long *)subSharedPropertyGet(frames[i], XA_CARDINAL, "SUBTLE_VIEW_TAGS", NULL);

      if((int)*flags & (1L << (tag + 1))) printf("view   - %s\n", views[i]);

      free(flags);
    }

  /* Clients */
  for(i = 0; i < size_clients; i++)
    {
      char *wmname = NULL, *wmclass = NULL;

      flags   = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL, "SUBTLE_CLIENT_TAGS", NULL);
      wmname  = subSharedWindowWMName(clients[i]);
      wmclass = subSharedWindowWMClass(clients[i]);

      if((int)*flags & (1L << (tag + 1))) printf("client - %s (%s)\n", wmname, wmclass);

      free(flags);
      free(wmname);
      free(wmclass);
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
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -t -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), arg1);

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_KILL", data);  
} /* }}} */

/* ActionViewNew {{{ */
static void
ActionViewNew(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -t -n PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), arg1);

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data);
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

  subSharedLogDebug("%s\n", __func__);

  /* Collect data */
  nv     = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display), XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);
  cv     = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  frames = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);
  views  = subSharedPropertyList(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);

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
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -v -j PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Try to convert arg1 to long or to find view */
  if(!(view = atoi(arg1)))
    view = subSharedViewFind(arg1, NULL);

  data.l[0] = view;

  subSharedMessage(DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", data);
} /* }}} */

/* ActionViewTag {{{ */
static void
ActionViewTag(char *arg1,
  char *arg2)
{
  Window win;
  int tag = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1 && arg2, "Usage: %sr -v PATTERN -T PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedViewFind(arg1, &win);
  tag = subSharedTagFind(arg2);

  data.l[0] = win;
  data.l[1] = tag + 1;

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_TAG", data);
} /* }}} */

/* ActionViewUntag {{{ */
static void
ActionViewUntag(char *arg1,
  char *arg2)
{
  Window win;
  int tag = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1 && arg2, "Usage: %sr -v PATTERN -u PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedViewFind(arg1, &win);
  tag = subSharedTagFind(arg2);

  data.l[0] = win;
  data.l[1] = tag + 1;

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_UNTAG", data);
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

  Assert(arg1, "Usage: %sr -v PATTERN -g\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  subSharedViewFind(arg1, &win);
  flags = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, "SUBTLE_VIEW_TAGS", NULL);
  tags  = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

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
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  Assert(arg1, "Usage: %sr -v -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  view = subSharedViewFind(arg1, NULL);

  data.l[0] = view;

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_KILL", data);  
} /* }}} */

/* Usage {{{ */
static void
Usage(int group)
{
  printf("Usage: %sr [OPTIONS] [GROUP] [ACTION]\n", PKG_NAME);

  if(-1 == group)
    {
      printf("\nOptions:\n" \
             "  -d, --display=DISPLAY   Connect to DISPLAY (default: $DISPLAY)\n" \
             "  -D, --debug             Print debugging messages\n" \
             "  -h, --help              Show this help and exit\n" \
             "  -V, --version           Show version info and exit\n" \
             "\nGroups:\n" \
             "  -c, --clients           Use clients group\n" \
             "  -t, --tags              Use tags group\n" \
             "  -v, --views             Use views group\n");
    }
  if(-1 == group || 0 == group)
    {
      printf("\nActions for clients:\n" \
             "  -l, --list              List all clients\n" \
             "  -f, --find=PATTERN      Find a client\n" \
             "  -F, --focus=PATTERN     Set focus to client\n" \
             "  -s, --shade=PATTERN     Shade client\n" \
             "  -T, --tag=PATTERN       Add tag to client\n" \
             "  -u, --untag=PATTERN     Remove tag from client\n" \
             "  -g, --tags              Show client tags\n" \
             "  -k, --kill=PATTERN      Kill a client\n");
    }
  if(-1 == group || 1 == group)
    {
      printf("\nActions for tags:\n" \
             "  -n, --new=NAME          Create new tag\n" \
             "  -l, --list              List all tags\n" \
             "  -f, --find              Find all clients/views by tag\n" \
             "  -k, --kill=PATTERN      Kill a tag\n");
    }
  if(-1 == group || 2 == group)
    {
      printf("\nActions for views:\n" \
             "  -n, --new=NAME          Create new view\n" \
             "  -l, --list              List all views\n" \
             "  -T, --tag=PATTERN       Add tag to view\n" \
             "  -u, --untag=PATTERN     Remove tag from view\n" \
             "  -g, --tags              Show view tags\n" \
             "  -k, --kill=VIEW         Kill a view\n");
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
         "  %sr -c -l                List all clients\n" \
         "  %sr -t -a subtle         Add new tag 'subtle'\n" \
         "  %sr -v subtle -T rocks   Tag view 'subtle' with tag 'rocks'\n" \
         "  %sr -c xterm -g          Show tags of client 'xterm'\n" \
         "  %sr -c -f #              Select client and show info\n" \
         "  %sr -t -f term           Show every client/view tagged with 'term'\n" \
         "\nPlease report bugs to <%s>\n",
         PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_BUGREPORT);
} /* }}} */

/* Version {{{ */
static void
Version(void)
{
  printf("%sr %s - Copyright (c) 2005-2008 Christoph Kappel\n" \
          "Released under the GNU General Public License\n" \
          "Compiled for X%dR%d\n", 
          PKG_NAME, PKG_VERSION, X_PROTOCOL, X_PROTOCOL_REVISION);
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

        printf("Please report this bug to <%s>\n", PKG_BUGREPORT);
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
      if(!fgets(buf, sizeof(buf), stdin)) subSharedLogError("Can't read from pipe\n");
      ret = (char *)subSharedAlloc(strlen(buf), sizeof(char));
      strncpy(ret, buf, strlen(buf) - 1);
      subSharedLogDebug("Pipe: len=%d\n", strlen(buf));
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
  char *dispname = NULL, *arg1 = NULL, *arg2 = NULL;
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
    { NULL, ActionClientKill, ActionClientList, ActionClientFind, ActionClientFocus, NULL, 
      ActionClientShade, ActionClientTag, ActionClientUntag, ActionClientTags },
    { ActionTagNew, ActionTagKill, ActionTagList, ActionTagFind, NULL, NULL, NULL, NULL, NULL, NULL },
    { ActionViewNew, ActionViewKill, ActionViewList, NULL, NULL, ActionViewJump, NULL, ActionViewTag, 
      ActionViewUntag, ActionViewTags }
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

          case 'd': dispname = optarg;       break;
          case 'h': Usage(group);            return(0);
#ifdef DEBUG          
          case 'D': debug = 1;               break;
#endif /* DEBUG */
          case 'V': Version();               return(0);
          case '?':
            printf("Try `%sr --help for more information\n", PKG_NAME);
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
  if(!(display = XOpenDisplay(dispname)))
    {
      printf("Can't open display `%s'.\n", (dispname) ? dispname : ":0.0");
      return(-1);
    }
  XSetErrorHandler(subSharedLogXError);

  /* Check command */
  if(cmds[group][action]) cmds[group][action](arg1, arg2);
  else Usage(group);

  XCloseDisplay(display);
  if(arg1) free(arg1);
  if(arg2) free(arg2);
  
  return(0);
} /* }}} */
