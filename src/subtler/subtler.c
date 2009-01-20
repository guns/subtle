
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

#include "shared.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

Display *display = NULL;
int debug = 0;

/* Typedefs {{{ */
typedef void(*SubCommand)(char *, char *);
/* }}} */

/* Macros {{{ */
#define SubtlerAssert(cond,...) if(!cond) subSharedLog(3, __FILE__, __LINE__, __VA_ARGS__);
/* }}} */

/* Flags {{{ */
#define SUB_GROUP_CLIENT 0    ///< Group client
#define SUB_GROUP_SUBLET 1    ///< Group sublet
#define SUB_GROUP_TAG    2    ///< Group tag
#define SUB_GROUP_VIEW   3    ///< Group view
#define SUB_GROUP_TOTAL  4    ///< Group total

#define SUB_ACTION_NEW   0    ///< Action new
#define SUB_ACTION_KILL  1    ///< Action kill
#define SUB_ACTION_LIST  2    ///< Action list
#define SUB_ACTION_FIND  3    ///< Action find
#define SUB_ACTION_FOCUS 4    ///< Action focus
#define SUB_ACTION_FULL  5    ///< Action full
#define SUB_ACTION_FLOAT 6    ///< Action float
#define SUB_ACTION_STICK 7    ///< Action stick
#define SUB_ACTION_JUMP  8    ///< Action jump
#define SUB_ACTION_TAG   9    ///< Action tag
#define SUB_ACTION_UNTAG 10   ///< Action untag
#define SUB_ACTION_TAGS  11   ///< Action tags
#define SUB_ACTION_TOTAL 12   ///< Action total
/* }}} */

/* SubtlerToggle {{{ */
static void
SubtlerToggle(char *name,
  char *type)
{
  Window win = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  if(-1 != subSharedClientFind(name, &win))  
    {
      data.l[1] = XInternAtom(display, type, False);

      subSharedMessage(win, "_NET_WM_STATE", data, True);
    }
  else subSharedLogWarn("Failed to find client\n");
} /* }}} */

/* SubtlerClientInfo {{{ */
static void
SubtlerClientInfo(Window win)
{
  Window unused;
  int x, y;
  unsigned int width, height, border;
  unsigned long *nv = NULL, *cv = NULL, *rv = NULL;
  char *wmname = NULL, *wmclass = NULL;

  assert(win);

  /* Gather data */
  wmname  = subSharedWindowWMName(win);
  wmclass = subSharedWindowWMClass(win);

  /* Get properties */
  nv = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);
  cv = (unsigned long*)subSharedPropertyGet(win, XA_CARDINAL, "_NET_WM_DESKTOP", NULL);
  rv = (unsigned long*)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

  if(wmname && wmclass && nv && cv && rv && 
    XGetGeometry(display, win, &unused, &x, &y, &width, &height, &border, &border))
    {
      printf("%#lx %c %ld %ux%u %s (%s)\n", win, (*cv == *rv ? '*' : '-'),
        (*cv > *nv ? -1 : *cv), width, height, wmname, wmclass);

      free(wmname);
      free(wmclass);
      free(nv);
      free(cv);
      free(rv);
    }
  else subSharedLogWarn("Failed to fetch client infos!\n");
} /* }}} */

/* SubtlerClientList {{{ */
static void
SubtlerClientList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  Window *clients = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((clients = subSharedClientList(&size)))
    {
      for(i = 0; i < size; i++) SubtlerClientInfo(clients[i]);
      free(clients);
    }
  else subSharedLogWarn("Failed to get client list!\n");
} /* }}} */

/* SubtlerClientFind {{{ */
static void
SubtlerClientFind(char *arg1,
  char *arg2)
{
  Window win;

  SubtlerAssert(arg1, "Usage: %sr -c -f PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, &win)) SubtlerClientInfo(win);
  else subSharedLogWarn("Failed to find client\n");
} /* }}} */

/* SubtlerClientFocus {{{ */
static void
SubtlerClientFocus(char *arg1,
  char *arg2)
{
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1, "Usage: %sr -c -F CLIENT\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, &win))
    {
      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, False);
    }
  else subSharedLogWarn("Failed to find client\n");
} /* }}} */

/* SubtlerClientToggleFull {{{ */
static void
SubtlerClientToggleFull(char *arg1,
  char *arg2)
{
  SubtlerAssert(arg1, "Usage: %sr -c -U CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_FULLSCREEN");
} /* }}} */

/* SubtlerClientToggleFloat {{{ */
static void
SubtlerClientToggleFloat(char *arg1,
  char *arg2)
{
  SubtlerAssert(arg1, "Usage: %sr -c -L CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_ABOVE");
} /* }}} */

/* SubtlerClientToggleStick {{{ */
static void
SubtlerClientToggleStick(char *arg1,
  char *arg2)
{
  SubtlerAssert(arg1, "Usage: %sr -c -S CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_STICKY");
} /* }}} */

/* SubtlerClientTag {{{ */
static void
SubtlerClientTag(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1 && arg2, "Usage: %sr -c PATTERN -T PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL);
  data.l[1] = subSharedTagFind(arg2);

  if(data.l[0] && data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_CLIENT_TAG", data, False);
  else subSharedLogWarn("Failed to tag client\n");
} /* }}} */

/* SubtlerClientUntag {{{ */
static void
SubtlerClientUntag(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1 && arg2, "Usage: %sr -c PATTERN -u PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL);
  data.l[1] = subSharedTagFind(arg2);

  if(data.l[0] && data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_CLIENT_UNTAG", data, False);
  else subSharedLogWarn("Failed to untag client\n");
} /* }}} */

/* SubtlerClientTags {{{ */
static void
SubtlerClientTags(char *arg1,
  char *arg2)
{
  Window win;

  SubtlerAssert(arg1, "Usage: %sr -c PATTERN -g\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, &win))
    {
      int i, size = 0;
      unsigned long *flags = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL,
        "SUBTLE_CLIENT_TAGS", NULL);
      char **tags = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

      for(i = 0; i < size; i++)
        if((int)*flags & (1L << (i + 1))) printf("%s\n", tags[i]);
      
      free(flags);
      free(tags);
    }
  else subSharedLogWarn("Failed to fetch client tags\n");
} /* }}} */

/* SubtlerClientKill {{{ */
static void
SubtlerClientKill(char *arg1,
  char *arg2)
{
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1, "Usage: %sr -c -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, &win))
    {
      data.l[0] = CurrentTime;

      subSharedMessage(win, "_NET_CLOSE_WINDOW", data, False);
    }
  else subSharedLogWarn("Failed to kill client\n");
} /* }}} */

/* SubtlerTagNew {{{ */
static void
SubtlerTagNew(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1, "Usage: %sr -t -n NAME\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), "%s", arg1);
  
  if(!subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, False))
    subSharedLogWarn("Failed to create tag\n");
} /* }}} */

/* SubtlerTagList {{{ */
static void
SubtlerTagList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  char **tags = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((tags = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size)))
    {
      for(i = 0; i < size; i++) printf("%s\n", tags[i]);

      free(tags);
    }
  else subSharedLogWarn("Failed to get tag list\n");
} /* }}} */

/* SubtlerTagFind {{{ */
static void
SubtlerTagFind(char *arg1,
  char *arg2)
{
  int i, tag = -1, size_clients = 0, size_views = 0;
  char **names = NULL;
  Window *views = NULL, *clients = NULL;

  subSharedLogDebug("%s\n", __func__);

  /* Collect data */
  tag     = subSharedTagFind(arg1);
  names   = subSharedPropertyList(DefaultRootWindow(display),
    "_NET_DESKTOP_NAMES", &size_views);
  views  = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW,
    "_NET_VIRTUAL_ROOTS", NULL);
  clients = subSharedClientList(&size_clients);

  /* Views */
  if(tag && names && views)
    {
      for(i = 0; i < size_views; i++)
        {
          unsigned long *flags = (unsigned long *)subSharedPropertyGet(views[i], XA_CARDINAL,
            "SUBTLE_VIEW_TAGS", NULL);

          if((int)*flags & (1L << (tag + 1))) 
            printf("view   - %s\n", names[i]);

          free(flags);
        }

      free(names);
      free(views);
    }
  else subSharedLogWarn("Failed to get view list\n");

  /* Clients */
  if(tag && clients)
    {
      for(i = 0; i < size_clients; i++)
        {
          char *wmname = NULL, *wmclass = NULL;
          unsigned long *flags   = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL,
            "SUBTLE_CLIENT_TAGS", NULL);

          wmname  = subSharedWindowWMName(clients[i]);
          wmclass = subSharedWindowWMClass(clients[i]);

          if((int)*flags & (1L << (tag + 1))) 
            printf("client - %s (%s)\n", wmname, wmclass);

          free(flags);
          free(wmname);
          free(wmclass);
        }

      free(clients);
    }
  else subSharedLogWarn("Failed to get clients list\n");
} /* }}} */

/* SubtlerTagKill {{{ */
static void
SubtlerTagKill(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1, "Usage: %sr -t -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if((data.l[0] = subSharedTagFind(arg1)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_KILL", data, False);
  else subSharedLogWarn("Failed to kill tag\n");
} /* }}} */

/* SubtlerSubletList {{{ */
static void
SubtlerSubletList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  char **sublets = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((sublets = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_SUBLET_LIST", &size)))
    {
      for(i = 0; i < size; i++)
        printf("%s\n", sublets[i]);

      free(sublets);
    }
  else subSharedLogWarn("Failed to get sublet list\n");
} /* }}} */

/* SubtlerSubletKill {{{ */
static void
SubtlerSubletKill(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1, "Usage: %sr -s -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if((data.l[0] = subSharedSubletFind(arg2)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_KILL", data, False);
  else subSharedLogWarn("Failed to kill sublet\n");
} /* }}} */

/* SubtlerViewNew {{{ */
static void
SubtlerViewNew(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1, "Usage: %sr -t -n PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), "%s", arg1);

  if(!subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data, False))
    subSharedLogWarn("Failed to create view\n");
} /* }}} */

/* SubtlerViewList {{{ */
static void
SubtlerViewList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  unsigned long *nv = NULL, *cv = NULL;
  char **names = NULL;
  Window *views = NULL;

  subSharedLogDebug("%s\n", __func__);

  /* Collect data */
  nv     = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);
  cv     = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  names  = subSharedPropertyList(DefaultRootWindow(display), 
    "_NET_DESKTOP_NAMES", &size);
  views = (Window *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);

  if(nv && cv && names && views)
    {
      for(i = 0; *nv && i < *nv; i++)
        printf("%#lx %c %s\n", views[i], (i == *cv ? '*' : '-'), names[i]);

      free(nv);
      free(cv);
      free(names);
      free(views);
    }
  else subSharedLogWarn("Failed to get view list\n");
} /* }}} */

/* SubtlerViewJump {{{ */
static void
SubtlerViewJump(char *arg1,
  char *arg2)
{
  int view = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1, "Usage: %sr -v -j PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Try to convert arg1 to long or to find view */
  if((view = atoi(arg1)) || (( view = subSharedViewFind(arg1, NULL))))
    {
      data.l[0] = view;

      subSharedMessage(DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", data, False);
    }
  else subSharedLogWarn("Failed to jump to view\n");
} /* }}} */

/* SubtlerViewTag {{{ */
static void
SubtlerViewTag(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1 && arg2, "Usage: %sr -v PATTERN -T PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedViewFind(arg1, NULL);
  data.l[1] = subSharedTagFind(arg2);

  if(data.l[0] && data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_TAG", data, False);
  else subSharedLogWarn("Failed to tag view\n");
} /* }}} */

/* SubtlerViewUntag {{{ */
static void
SubtlerViewUntag(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1 && arg2, "Usage: %sr -v PATTERN -u PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedViewFind(arg1, NULL);
  data.l[1] = subSharedTagFind(arg2);

  if(data.l[0] && data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_UNTAG", data, False);
  else subSharedLogWarn("Failed to untag view\n");
} /* }}} */

/* SubtlerViewTags {{{ */
static void
SubtlerViewTags(char *arg1,
  char *arg2)
{
  int i, size = 0;
  Window win;
  char **tags = NULL;
  unsigned long *flags = NULL;

  SubtlerAssert(arg1, "Usage: %sr -v PATTERN -g\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 == subSharedViewFind(arg1, &win))
    {
      flags = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, "SUBTLE_VIEW_TAGS", NULL);
      tags  = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

      for(i = 0; i < size; i++)
        if((int)*flags & (1L << (i + 1))) printf("%s\n", tags[i]);
      
      free(flags);
      free(tags);
    }
  else subSharedLogWarn("Failed to find view\n");
} /* }}} */

/* SubtlerViewKill {{{ */
static void
SubtlerViewKill(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  SubtlerAssert(arg1, "Usage: %sr -v -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if((data.l[0] = subSharedViewFind(arg1, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_KILL", data, False);
  else subSharedLogWarn("Failed to kill view\n");
} /* }}} */

/* SubtlerUsage {{{ */
static void
SubtlerUsage(int group)
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
  if(-1 == group || SUB_GROUP_CLIENT == group)
    {
      printf("\nOptions for clients:\n" \
             "  -l, --list              List all clients\n" \
             "  -f, --find=PATTERN      Find a client\n" \
             "  -F, --focus=PATTERN     Set focus to client\n" \
             "  -U, --full              Toggle full\n" \
             "  -L, --float             Toggle float\n" \
             "  -i, --stick             Toggle stick\n" \
             "  -T, --tag=PATTERN       Add tag to client\n" \
             "  -u, --untag=PATTERN     Remove tag from client\n" \
             "  -g, --tags              Show client tags\n" \
             "  -k, --kill=PATTERN      Kill a client\n");
    }
  if(-1 == group || SUB_GROUP_SUBLET == group)
    {
      printf("\nOptions for sublets:\n" \
             "  -l, --list              List all sublets\n" \
             "  -k, --kill=PATTERN      Kill a sublet\n");
    }    
  if(-1 == group || SUB_GROUP_TAG == group)
    {
      printf("\nOptions for tags:\n" \
             "  -n, --new=NAME          Create new tag\n" \
             "  -l, --list              List all tags\n" \
             "  -f, --find              Find all clients/views by tag\n" \
             "  -k, --kill=PATTERN      Kill a tag\n");
    }
  if(-1 == group || SUB_GROUP_VIEW == group)
    {
      printf("\nOptions for views:\n" \
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
         "  %sr -t -n subtle         Add new tag 'subtle'\n" \
         "  %sr -v subtle -T rocks   Tag view 'subtle' with tag 'rocks'\n" \
         "  %sr -c xterm -g          Show tags of client 'xterm'\n" \
         "  %sr -c -f #              Select client and show info\n" \
         "  %sr -t -f term           Show every client/view tagged with 'term'\n" \
         "\nPlease report bugs to <%s>\n",
         PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_BUGREPORT);
} /* }}} */

/* SubtlerVersion {{{ */
static void
SubtlerVersion(void)
{
  printf("%sr %s - Copyright (c) 2005-2008 Christoph Kappel\n" \
          "Released under the GNU General Public License\n" \
          "Compiled for X%dR%d\n", 
          PKG_NAME, PKG_VERSION, X_PROTOCOL, X_PROTOCOL_REVISION);
}
/* }}} */

/* SubtlerSignal {{{ */
static void
SubtlerSignal(int signum)
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

/* SubtlerPipe {{{ */
static char *
SubtlerPipe(char *string)
{
  char buf[256], *ret = NULL;

  assert(string);

  if(!strncmp(string, "-", 1)) 
    {
      /* Open pipe */
      if(!fgets(buf, sizeof(buf), stdin)) subSharedLogError("Can't read from pipe\n");
      ret = (char *)subSharedMemoryAlloc(strlen(buf), sizeof(char));
      strncpy(ret, buf, strlen(buf) - 1);
      subSharedLogDebug("Pipe: len=%d\n", strlen(buf));
    }
  else ret = strdup(string);
  
  return ret;
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
    { "sublet",     no_argument,        0,  's'  },
    { "tags",       no_argument,        0,  't'  },
    { "views",      no_argument,        0,  'v'  },

    /* Actions */
    { "new",        no_argument,        0,  'n'  },
    { "kill",       no_argument,        0,  'k'  },
    { "list",       no_argument,        0,  'l'  },
    { "find",       no_argument,        0,  'f'  },
    { "focus",      no_argument,        0,  'F'  },
    { "full",       no_argument,        0,  'U'  },
    { "float",      no_argument,        0,  'L'  },
    { "stick",      no_argument,        0,  'S'  },
    { "jump",       no_argument,        0,  'j'  },
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
  SubCommand cmds[SUB_GROUP_TOTAL][SUB_ACTION_TOTAL] = { 
    /* Client, Sublet, Tag, View <=> New, Kill, List, Find, Focus, Full, Float, Stick, 
       Jump, Tag, Untag, Tags */
    { NULL, SubtlerClientKill, SubtlerClientList, SubtlerClientFind, SubtlerClientFocus, 
      SubtlerClientToggleFull, SubtlerClientToggleFloat, SubtlerClientToggleStick, NULL,
      SubtlerClientTag, SubtlerClientUntag, SubtlerClientTags },
    { NULL, SubtlerSubletKill, SubtlerSubletList, NULL, NULL, NULL, NULL, NULL, NULL, 
      NULL, NULL, NULL },
    { SubtlerTagNew, SubtlerTagKill, SubtlerTagList, SubtlerTagFind, NULL, NULL, NULL, 
      NULL, NULL, NULL, NULL, NULL },
    { SubtlerViewNew, SubtlerViewKill, SubtlerViewList, NULL, NULL, NULL, NULL, NULL,
      SubtlerViewJump, SubtlerViewTag, SubtlerViewUntag, SubtlerViewTags }
  };

  /* Set up signal handler */
  act.sa_handler = SubtlerSignal;
  act.sa_flags   = 0;
  memset(&act.sa_mask, 0, sizeof(sigset_t)); ///< Avoid uninitialized values
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGSEGV, &act, NULL);

  while((c = getopt_long(argc, argv, "cstvnkfFULSjlTugDd:hV", long_options, NULL)) != -1)
    {
      switch(c)
        {
          case 'c': group = SUB_GROUP_CLIENT;   break;
          case 's': group = SUB_GROUP_SUBLET;   break;
          case 't': group = SUB_GROUP_TAG;      break;
          case 'v': group = SUB_GROUP_VIEW;     break;

          case 'n': action = SUB_ACTION_NEW;    break;
          case 'k': action = SUB_ACTION_KILL;   break;
          case 'l': action = SUB_ACTION_LIST;   break;
          case 'f': action = SUB_ACTION_FIND;   break;
          case 'F': action = SUB_ACTION_FOCUS;  break;
          case 'U': action = SUB_ACTION_FULL;   break;
          case 'L': action = SUB_ACTION_FLOAT;  break;
          case 'S': action = SUB_ACTION_STICK;  break;
          case 'j': action = SUB_ACTION_JUMP;   break;
          case 'T': action = SUB_ACTION_TAG;    break;
          case 'u': action = SUB_ACTION_UNTAG;  break;
          case 'g': action = SUB_ACTION_TAGS;   break;

          case 'd': dispname = optarg;          break;
          case 'h': SubtlerUsage(group);        return 0;
#ifdef DEBUG          
          case 'D': debug = 1;                  break;
#endif /* DEBUG */
          case 'V': SubtlerVersion();           return 0;
          case '?':
            printf("Try `%sr --help for more information\n", PKG_NAME);
            return -1;
        }
    }

  /* Check command */
  if(-1 == group || -1 == action)
    {
      SubtlerUsage(group);
      return 0;
    }
  
  /* Get arguments */
  if(argc > optind)     arg1 = SubtlerPipe(argv[optind]);
  if(argc > optind + 1) arg2 = SubtlerPipe(argv[optind + 1]);

  /* Open connection to server */
  if(!(display = XOpenDisplay(dispname)))
    {
      printf("Failed to open display `%s'.\n", (dispname) ? dispname : ":0.0");
      return -1;
    }
  XSetErrorHandler(subSharedLogXError);

  /* Check if subtle is running */
  if(True != subSharedSubtleRunning())
    {
      XCloseDisplay(display);
      display = NULL;
      
      subSharedLogError("%s is not running\n", PKG_NAME);

      return -1;
    }

  /* Select command */
  if(cmds[group][action]) cmds[group][action](arg1, arg2);
  else SubtlerUsage(group);

  XCloseDisplay(display);
  if(arg1) free(arg1);
  if(arg2) free(arg2);
  
  return 0;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
