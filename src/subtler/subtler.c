
 /**
  * @package subtle
  *
  * @file subtle remote client
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "shared.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

Display *display = NULL;
int debug = 0;

/* Typedefs {{{ */
typedef void(*SubtlerCommand)(char *, char *);
/* }}} */

/* Macros {{{ */
#define CHECK(cond,...) if(!cond) subSharedLog(3, __FILE__, __LINE__, __VA_ARGS__);
/* }}} */

/* Flags {{{ */
/* Groups */
#define SUB_GROUP_CLIENT   0   ///< Group client
#define SUB_GROUP_GRAVITY  1   ///< Group gravity
#define SUB_GROUP_SCREEN   2   ///< Group screen
#define SUB_GROUP_SUBLET   3   ///< Group sublet
#define SUB_GROUP_TAG      4   ///< Group tag
#define SUB_GROUP_VIEW     5   ///< Group view
#define SUB_GROUP_TOTAL    6   ///< Group total

/* Actions */
#define SUB_ACTION_ADD     0   ///< Action add
#define SUB_ACTION_KILL    1   ///< Action kill
#define SUB_ACTION_FIND    2   ///< Action find
#define SUB_ACTION_FOCUS   3   ///< Action focus
#define SUB_ACTION_FULL    4   ///< Action max
#define SUB_ACTION_FLOAT   5   ///< Action float
#define SUB_ACTION_STICK   6   ///< Action stick
#define SUB_ACTION_JUMP    7   ///< Action jump
#define SUB_ACTION_LIST    8   ///< Action list
#define SUB_ACTION_TAG     9   ///< Action tag
#define SUB_ACTION_UNTAG   10  ///< Action untag
#define SUB_ACTION_TAGS    11  ///< Action tags
#define SUB_ACTION_UPDATE  12  ///< Action update
#define SUB_ACTION_DATA    13  ///< Action data
#define SUB_ACTION_GRAVITY 14  ///< Action gravity
#define SUB_ACTION_SCREEN  15  ///< Action screen
#define SUB_ACTION_RAISE   16  ///< Action raise
#define SUB_ACTION_LOWER   17  ///< Action lower
#define SUB_ACTION_TOTAL   18  ///< Action total

/* Modifier */
#define SUB_MOD_LEFT       0   ///< Mod left
#define SUB_MOD_DOWN       1   ///< Mod down
#define SUB_MOD_UP         2   ///< Mod up
#define SUB_MOD_RIGHT      3   ///< Mod right
#define SUB_MOD_CURRENT    4   ///< Mod current
#define SUB_MOD_SELECT     5   ///< Mod select
#define SUB_MOD_RELOAD     6   ///< Mod reload
#define SUB_MOD_QUIT       7   ///< Mod quit
#define SUB_MOD_TOTAL      8   ///< Mod total
/* }}} */

/* SubtlerToggle {{{ */
static void
SubtlerToggle(char *name,
  char *type)
{
  Window win = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  if(-1 != subSharedClientFind(name, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))  
    {
      data.l[1] = XInternAtom(display, type, False);

      subSharedMessage(win, "_NET_WM_STATE", data, False);
    }
  else subSharedLogWarn("Failed toggling client\n");
} /* }}} */

/* SubtlerRestack {{{ */
static void
SubtlerRestack(char *name,
  int detail)
{
  Window win = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  if(-1 != subSharedClientFind(name, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))  
    {
      data.l[0] = 2; ///< Mimic a pager
      data.l[1] = win;
      data.l[2] = detail; 
      
      subSharedMessage(DefaultRootWindow(display), "_NET_RESTACK_WINDOW", data, False);
    }
  else subSharedLogWarn("Failed restacking client");  
} /* }}} */

/* SubtlerMatch {{{ */
static char *
SubtlerMatch(int type)
{
  char *ret = NULL;
  int i, size = 0, match = 0, score = 0, *gravity1 = NULL;
  Window found = None, *clients = NULL, *views = NULL;
  unsigned long *cv = NULL, *flags1 = NULL, *focus = NULL;

  if((focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
      XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL)))
    {
      clients = subSharedClientList(&size);
      views   = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW, 
        "_NET_VIRTUAL_ROOTS", NULL);
      cv      = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
        XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

      if(clients && cv)
        {
          flags1   = (unsigned long *)subSharedPropertyGet(views[*cv], XA_CARDINAL, 
            "SUBTLE_WINDOW_TAGS", NULL);
          gravity1 = (int *)subSharedPropertyGet(*focus, XA_CARDINAL, 
            "SUBTLE_WINDOW_GRAVITY", NULL);

          /* Iterate once to find a client score-based */
          for(i = 0; 100 != match && i < size; i++)
            {
              unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
                "SUBTLE_WINDOW_TAGS", NULL);

              if(*focus != clients[i] && *flags1 & *flags2) ///< Check if there are common tags
                {
                  int *gravity2 = (int *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
                    "SUBTLE_WINDOW_GRAVITY", NULL);

                  if(match < (score = subSharedMatch(type,  *gravity1, *gravity2)))
                    {
                      match = score;
                      found = clients[i];
                    }

                  free(gravity2);
                }

              free(flags2);
            }
          
          if(found) subSharedPropertyClass(found, NULL, &ret); ///< Get WM_CLASS of found win

          free(focus);
          free(flags1);
          free(gravity1);
          free(clients);
          free(cv);
        }
    }

  return ret;
} /* }}} */

/* SubtlerCurrentClient {{{ */
static char *
SubtlerCurrentClient(void)
{
  char *ret = NULL;
  unsigned long *focus = NULL;

  if((focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
      XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL)))
    {
      char buf[20];

      snprintf(buf, sizeof(buf), "%#lx", *focus);
      ret = strdup(buf);

      free(focus);
    }

  return ret;
} /* }}} */

/* SubtlerCurrentView {{{ */
static char *
SubtlerCurrentView(void)
{
  int size = 0;
  char **names = NULL, *ret = NULL;
  unsigned long *cv = NULL;
  
  names = subSharedPropertyStrings(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
  cv    = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

  ret = strdup(names[*cv]);

  XFreeStringList(names);
  free(cv);

  return ret;
} /* }}} */

/* SubtlerClientPrint {{{ */
void
SubtlerClientPrint(Window win,
  int rv,
  int nv)
{
  int x, y;
  Window unused;
  char *inst = NULL, *klass = NULL, *grav = NULL, buf[5] = { 0 };
  unsigned int width, height, border;
  unsigned long *cv = NULL, *gravity = NULL, *screen = NULL, *flags = NULL;

  /* Collect client data */
  subSharedPropertyClass(win, &inst, &klass);
  cv      = (unsigned long*)subSharedPropertyGet(win, XA_CARDINAL, 
    "_NET_WM_DESKTOP", NULL);
  gravity = (unsigned long*)subSharedPropertyGet(win, XA_CARDINAL, 
    "SUBTLE_WINDOW_GRAVITY", NULL);
  screen = (unsigned long*)subSharedPropertyGet(win, XA_CARDINAL, 
    "SUBTLE_WINDOW_SCREEN", NULL);
  flags = (unsigned long*)subSharedPropertyGet(win, XA_CARDINAL, 
    "SUBTLE_WINDOW_FLAGS", NULL);

  /* Get gravity */
  snprintf(buf, sizeof(buf), "%ld", *gravity);
  subSharedGravityFind(buf, &grav, NULL);

  XGetGeometry(display, win, &unused, &x, &y, &width, &height, &border, &border);

  printf("%#10lx %c %ld %4u x %-4u %10s %ld %c%c%c %s (%s)\n", win, (*cv == rv ? '*' : '-'),
    (*cv > nv ? -1 : *cv + 1), width, height, grav, *screen, 
    *flags & SUB_EWMH_FULL ? 'F' : '-', *flags & SUB_EWMH_FLOAT ? 'O' : '-', 
    *flags & SUB_EWMH_STICK ? 'S' : '-', inst, klass);

  free(inst);
  free(klass);
  free(cv);
  free(gravity);
  free(screen);
  free(flags);
  free(grav);
} /* }}} */

/* Client */

/* SubtlerClientFind {{{ */
static void
SubtlerClientFind(char *arg1,
  char *arg2)
{
  Window win;

  CHECK(arg1, "Usage: %sr -c -f PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    {
      unsigned long *nv = NULL, *rv = NULL;

      /* Collect data */
      nv = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
        XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);
      rv = (unsigned long*)subSharedPropertyGet(DefaultRootWindow(display),
        XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

      SubtlerClientPrint(win, *nv, *rv);

      free(nv);
      free(rv);
    }
  else subSharedLogWarn("Failed finding client\n");
} /* }}} */

/* SubtlerClientFocus {{{ */
static void
SubtlerClientFocus(char *arg1,
  char *arg2)
{
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -c -o CLIENT\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    {
      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, False);
    }
  else subSharedLogWarn("Failed finding client\n");
} /* }}} */

/* SubtlerClientToggleFull {{{ */
static void
SubtlerClientToggleFull(char *arg1,
  char *arg2)
{
  CHECK(arg1, "Usage: %sr -c -F CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_FULLSCREEN");
} /* }}} */

/* SubtlerClientToggleFloat {{{ */
static void
SubtlerClientToggleFloat(char *arg1,
  char *arg2)
{
  CHECK(arg1, "Usage: %sr -c -O CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_ABOVE");
} /* }}} */

/* SubtlerClientToggleStick {{{ */
static void
SubtlerClientToggleStick(char *arg1,
  char *arg2)
{
  CHECK(arg1, "Usage: %sr -c -S CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_STICKY");
} /* }}} */

/* SubtlerClientRestackRaise {{{ */
static void
SubtlerClientRestackRaise(char *arg1,
  char *arg2)
{
  CHECK(arg1, "Usage: %sr -c -E CLIENT\n", PKG_NAME);

  SubtlerRestack(arg1, Above);
} /* }}} */

/* SubtlerClientRestackLower {{{ */
static void
SubtlerClientRestackLower(char *arg1,
  char *arg2)
{
  CHECK(arg1, "Usage: %sr -c -R CLIENT\n", PKG_NAME);

  SubtlerRestack(arg1, Below);
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
      unsigned long *nv = NULL, *rv = NULL;

      /* Collect data */
      nv = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
        XA_CARDINAL, "_NET_NUMBER_OF_DESKTOPS", NULL);
      rv = (unsigned long*)subSharedPropertyGet(DefaultRootWindow(display),
        XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

      for(i = 0; i < size; i++) 
        SubtlerClientPrint(clients[i], *nv, *rv);

      free(clients);
      free(nv);
      free(rv);
    }
  else subSharedLogWarn("Failed getting client list!\n");
} /* }}} */

/* SubtlerClientTag {{{ */
static void
SubtlerClientTag(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1 && arg2, "Usage: %sr -c PATTERN -T PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS));
  data.l[1] = subSharedTagFind(arg2, NULL);

  if(-1 != data.l[0] && -1 != data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_TAG", data, False);
  else subSharedLogWarn("Failed tagging client\n");
} /* }}} */

/* SubtlerClientUntag {{{ */
static void
SubtlerClientUntag(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1 && arg2, "Usage: %sr -c PATTERN -U PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS));
  data.l[1] = subSharedTagFind(arg2, NULL);

  if(-1 != data.l[0] && -1 != data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_UNTAG", data, False);
  else subSharedLogWarn("Failed untagging client\n");
} /* }}} */

/* SubtlerClientGravity {{{ */
static void
SubtlerClientGravity(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1 && arg2, "Usage: %sr -c PATTERN -y NUMBER\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS));
  data.l[1] = subSharedGravityFind(arg2, NULL, NULL);

  if(-1 != data.l[0] && -1 != data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_GRAVITY", data, False);
  else subSharedLogWarn("Failed setting client gravity\n");
} /* }}} */

/* SubtlerClientScreen {{{ */
static void
SubtlerClientScreen(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1 && arg2, "Usage: %sr -c PATTERN -n NUMBER\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS));
  data.l[1] = atoi(arg2);

  if(-1 != data.l[0] && 0 <= data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_SCREEN", data, False);
  else subSharedLogWarn("Failed setting client screen\n");
} /* }}} */

/* SubtlerClientTags {{{ */
static void
SubtlerClientTags(char *arg1,
  char *arg2)
{
  Window win;

  CHECK(arg1, "Usage: %sr -c PATTERN -G\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    {
      int i, size = 0;
      unsigned long *flags = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL,
        "SUBTLE_WINDOW_TAGS", NULL);
      char **tags = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

      for(i = 0; i < size; i++)
        if((int)*flags & (1L << (i + 1))) printf("%s\n", tags[i]);
     
      XFreeStringList(tags);
      free(flags);
    }
  else subSharedLogWarn("Failed fetching client tags\n");
} /* }}} */

/* SubtlerClientKill {{{ */
static void
SubtlerClientKill(char *arg1,
  char *arg2)
{
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -c -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    {
      data.l[0] = CurrentTime;
      data.l[1] = 2;  ///< Claim to be a pager

      subSharedMessage(win, "_NET_CLOSE_WINDOW", data, False);
    }
  else subSharedLogWarn("Failed killing client\n");
} /* }}} */

/* Gravity */

/* SubtlerGravityNew {{{ */
static void
SubtlerGravityNew(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };
  XRectangle geometry = { 0 };

  CHECK(arg1 && arg2, "Usage: %sr -g -a NAME GEOMETRY\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Parse data */
  sscanf(arg2, "%hdx%hd+%hd+%hd", &geometry.x, &geometry.y,
    &geometry.width, &geometry.height);
  snprintf(data.b, sizeof(data.b), "%dx%d+%d+%d#%s", geometry.x, geometry.y, 
    geometry.width, geometry.height, arg1);

  if(!subSharedMessage(DefaultRootWindow(display), "SUBTLE_GRAVITY_NEW", data, False))
    subSharedLogWarn("Failed adding gravity\n");
} /* }}} */

/* SubtlerGravityFind {{{ */
static void
SubtlerGravityFind(char *arg1,
  char *arg2)
{
  XRectangle geometry = { 0 };
  char *name = NULL;

  CHECK(arg1, "Usage: %sr -g -f GRAVITY\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Find gravity */
  if(-1 != subSharedGravityFind(arg1, &name, &geometry))
    {
      printf("%s %3d x %-3d %3d + %-3d\n", name, 
        geometry.x, geometry.y, geometry.width, geometry.height);

      free(name);
    }
  else subSharedLogWarn("Failed finding gravity\n");
} /* }}} */

/* SubtlerGravityList {{{ */
static void
SubtlerGravityList(char *arg1,
  char *arg2)
{
  int size = 0;
  char **gravities = NULL;

  /* Get gravity list */
  if((gravities = subSharedPropertyStrings(DefaultRootWindow(display), 
      "SUBTLE_GRAVITY_LIST", &size)))
    {
      int i;
      XRectangle geometry = { 0 };
      char buf[30] = { 0 };

      for(i = 0; i < size; i++)
        {
          sscanf(gravities[i], "%hdx%hd+%hd+%hd#%s", &geometry.x, &geometry.y,
            &geometry.width, &geometry.height, buf);

          printf("%15s %3d x %-3d %3d + %-3d\n", buf, 
            geometry.x, geometry.y, geometry.width, geometry.height);
        }

      XFreeStringList(gravities);
    }
  else subSharedLogWarn("Failed getting gravity list\n");
} /* }}} */

/* SubtlerGravityKill {{{ */
static void
SubtlerGravityKill(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1 && arg2, "Usage: %sr -g -k GRAVITY\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedGravityFind(arg1, NULL, NULL)))  
      subSharedMessage(DefaultRootWindow(display), "SUBTLE_GRAVITY_KILL", data, False);
  else subSharedLogWarn("Failed killing gravity\n");
} /* }}} */

/* Screen */

/* SubtlerScreenFind {{{ */
static void
SubtlerScreenFind(char *arg1,
  char *arg2)
{
  int n = 0, find = 0;

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  CHECK(arg1, "Usage: %sr -s -f ID\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  find = atoi(arg1);

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  /* Xinerama */
  if(XineramaQueryExtension(display, &xinerama_event, &xinerama_error) &&
    XineramaIsActive(display))
    {
      int i;
      XineramaScreenInfo *screens = NULL;

      /* Query screens */
      if((screens = XineramaQueryScreens(display, &n)))
        {
          for(i = 0; i < n; i++)
            if(i == find)
              {
                printf("%d %4d x %-4d %4d + %-4d\n", i, screens[i].x_org, screens[i].y_org,
                  screens[i].width, screens[i].height);

                XFree(screens);

                return;
              }

          XFree(screens);
        }
    } 
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Get default screen */
  if(0 == n)
    printf("0 %4d x %-4d %4d + %-4d\n", 0, 0, DisplayWidth(display, DefaultScreen(display)), 
      DisplayHeight(display, DefaultScreen(display)));
  else subSharedLogWarn("Failed finding screen\n");
} /* }}} */

/* SubtlerScreenList {{{ */
static void
SubtlerScreenList(char *arg1,
  char *arg2)
{
  int n = 0;

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  subSharedLogDebug("%s\n", __func__);

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  /* Xinerama */
  if(XineramaQueryExtension(display, &xinerama_event, &xinerama_error) &&
    XineramaIsActive(display))
    {
      int i;
      XineramaScreenInfo *screens = NULL;

      /* Query screens */
      if((screens = XineramaQueryScreens(display, &n)))
        {
          for(i = 0; i < n; i++)
            printf("%d %4d x %-4d %4d + %-4d\n", i, screens[i].x_org, screens[i].y_org,
              screens[i].width, screens[i].height);

          XFree(screens);
        }
    } 
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Get default screen */
  if(0 == n)
    printf("0 %4d x %-4d %4d + %-4d\n", 0, 0, DisplayWidth(display, DefaultScreen(display)), 
      DisplayHeight(display, DefaultScreen(display)));  
} /* }}} */

/* Sublet */

/* SubtlerSubletNew {{{ */
static void
SubtlerSubletNew(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -b -a PATH\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), "%s", arg1);
  
  if(!subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_NEW", data, False))
    subSharedLogWarn("Failed creating sublet\n");
} /* }}} */

/* SubtlerSubletList {{{ */
static void
SubtlerSubletList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  char **sublets = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((sublets = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_SUBLET_LIST", &size)))
    {
      for(i = 0; i < size; i++)
        printf("%s\n", sublets[i]);

      XFreeStringList(sublets);
    }
  else subSharedLogWarn("Failed getting sublet list\n");
} /* }}} */

/* SubtlerSubletUpdate {{{ */
static void
SubtlerSubletUpdate(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -b -u PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedSubletFind(arg1, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_UPDATE", data, False);
  else subSharedLogWarn("Failed updating sublet\n");
} /* }}} */

/* SubtlerSubletData {{{ */
static void
SubtlerSubletData(char *arg1,
  char *arg2)
{
  int id = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -b PATTERN -A DATA\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (id = subSharedSubletFind(arg1, NULL)))
    {
      snprintf(data.b, sizeof(data.b), "%c%s", (char)id, arg2);

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_DATA", data, False);
    }
  else subSharedLogWarn("Failed setting sublet data\n");
} /* }}} */

/* SubtlerSubletKill {{{ */
static void
SubtlerSubletKill(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -b -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedSubletFind(arg1, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_KILL", data, False);
  else subSharedLogWarn("Failed killing sublet\n");
} /* }}} */

/* Tag */

/* SubtlerTagNew {{{ */
static void
SubtlerTagNew(char *arg1,
  char *arg2)
{
  CHECK(arg1, "Usage: %sr -t -a NAME\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 == subSharedTagFind(arg1, NULL))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", arg1);
      subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, False);
    }
} /* }}} */

/* SubtlerTagFind {{{ */
static void
SubtlerTagFind(char *arg1,
  char *arg2)
{
  int i, tag = -1, size_clients = 0, size_views = 0;
  char **names = NULL;
  Window *views = NULL, *clients = NULL;

  CHECK(arg1, "Usage: %sr -t -f NAME\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Collect data */
  tag     = subSharedTagFind(arg1, NULL);
  names   = subSharedPropertyStrings(DefaultRootWindow(display),
    "_NET_DESKTOP_NAMES", &size_views);
  views  = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW,
    "_NET_VIRTUAL_ROOTS", NULL);
  clients = subSharedClientList(&size_clients);

  /* Views */
  if(-1 != tag && names && views)
    {
      for(i = 0; i < size_views; i++)
        {
          unsigned long *flags = (unsigned long *)subSharedPropertyGet(views[i], 
            XA_CARDINAL, "SUBTLE_WINDOW_TAGS", NULL);

          if((int)*flags & (1L << (tag + 1))) 
            printf("%#lx %s [view]\n", views[i], names[i]);

          free(flags);
        }

      XFreeStringList(names);
      free(views);
    }
  else subSharedLogWarn("Failed getting view list\n");

  /* Clients */
  if(-1 != tag && clients)
    {
      for(i = 0; i < size_clients; i++)
        {
          char *name = NULL, *klass = NULL;
          unsigned long *flags   = (unsigned long *)subSharedPropertyGet(clients[i], 
            XA_CARDINAL, "SUBTLE_WINDOW_TAGS", NULL);

          subSharedPropertyClass(clients[i], &name, &klass);

          if((int)*flags & (1L << (tag + 1))) 
            printf("%#lx %s (%s) [client]\n", clients[i], name, klass);

          free(flags);
          free(name);
          free(klass);
        }

      free(clients);
    }
  else subSharedLogWarn("Failed getting client list\n");
} /* }}} */

/* SubtlerTagList {{{ */
static void
SubtlerTagList(char *arg1,
  char *arg2)
{
  int i, size = 0;
  char **tags = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((tags = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size)))
    {
      for(i = 0; i < size; i++) printf("%s\n", tags[i]);

      XFreeStringList(tags);
    }
  else subSharedLogWarn("Failed getting tag list\n");
} /* }}} */

/* SubtlerTagKill {{{ */
static void
SubtlerTagKill(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -t -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedTagFind(arg1, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_KILL", data, False);
  else subSharedLogWarn("Failed killing tag\n");
} /* }}} */

/* View */

/* SubtlerViewNew {{{ */
static void
SubtlerViewNew(char *arg1,
  char *arg2)
{
  CHECK(arg1, "Usage: %sr -t -a PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);
  
  if(-1 == subSharedViewFind(arg1, NULL, NULL))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", arg1);
      subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data, False);
    }
} /* }}} */

/* SubtlerViewFind {{{ */
static void
SubtlerViewFind(char *arg1,
  char *arg2)
{
  int id;
  Window win;
  char *name = NULL;

  CHECK(arg1, "Usage: %sr -v -f PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (id = subSharedViewFind(arg1, &name, &win)) )
    {
      unsigned long *cv = NULL, *rv = NULL;

      /* Collect data */
      cv = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
        XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
      rv = (unsigned long*)subSharedPropertyGet(DefaultRootWindow(display),
        XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);        

      printf("%#10lx %c %2d %s\n", win, (*cv == *rv ? '*' : '-'), id, name);

      free(name);
      free(cv);
      free(rv);
    }
  else subSharedLogWarn("Failed finding view\n");
} /* }}} */

/* SubtlerViewJump {{{ */
static void
SubtlerViewJump(char *arg1,
  char *arg2)
{
  int view = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -v -j PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Find view */
  if(-1 != (view = subSharedViewFind(arg1, NULL, NULL)))
    {
      data.l[0] = view;

      subSharedMessage(DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", data, False);
    }
  else subSharedLogWarn("Failed jumping to view\n");
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
  names  = subSharedPropertyStrings(DefaultRootWindow(display), 
    "_NET_DESKTOP_NAMES", &size);
  views = (Window *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);

  if(nv && cv && names && views)
    {
      for(i = 0; *nv && i < *nv; i++)
        printf("%#10lx %c %2d %s\n", views[i], (i == *cv ? '*' : '-'), i, names[i]);

      XFreeStringList(names);
      free(nv);
      free(cv);
      free(views);
    }
  else subSharedLogWarn("Failed getting view list\n");
} /* }}} */

/* SubtlerViewTag {{{ */
static void
SubtlerViewTag(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1 && arg2, "Usage: %sr -v PATTERN -T PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedViewFind(arg1, NULL, NULL);
  data.l[1] = subSharedTagFind(arg2, NULL);
  data.l[2] = 1;

  if(-1 != data.l[0] && -1 != data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_TAG", data, False);
  else subSharedLogWarn("Failed tagging view\n");
} /* }}} */

/* SubtlerViewUntag {{{ */
static void
SubtlerViewUntag(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1 && arg2, "Usage: %sr -v PATTERN -U PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedViewFind(arg1, NULL, NULL);
  data.l[1] = subSharedTagFind(arg2, NULL);
  data.l[2] = 1;

  if(-1 != data.l[0] && -1 != data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_UNTAG", data, False);
  else subSharedLogWarn("Failed untagging view\n");
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

  CHECK(arg1, "Usage: %sr -v PATTERN -G\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedViewFind(arg1, NULL, &win))
    {
      flags = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, "SUBTLE_WINDOW_TAGS", NULL);
      tags  = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

      for(i = 0; i < size; i++)
        if((int)*flags & (1L << (i + 1))) printf("%s\n", tags[i]);
      
      XFreeStringList(tags);
      free(flags);
    }
  else subSharedLogWarn("Failed finding view\n");
} /* }}} */

/* SubtlerViewKill {{{ */
static void
SubtlerViewKill(char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(arg1, "Usage: %sr -v -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if((data.l[0] = subSharedViewFind(arg1, NULL, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_KILL", data, False);
  else subSharedLogWarn("Failed killing view\n");
} /* }}} */

/* SubtlerUsage {{{ */
static void
SubtlerUsage(int group)
{
  printf("Usage: %sr [OPTIONS] [GROUP] [ACTION]\n", PKG_NAME);

  if(-1 == group)
    {
      printf("\nOptions:\n" \
             "  -d, --display=DISPLAY   Connect to DISPLAY (default: %s)\n" \
             "  -D, --debug             Print debugging messages\n" \
             "  -h, --help              Show this help and exit\n" \
             "  -V, --version           Show version info and exit\n" \
             "\nModifier:\n" \
             "  -r, --reload            Reload %s\n" \
             "  -q, --quit              Quit %s\n" \
             "  -H, --left              Select window left\n" \
             "  -J, --down              Select window below\n" \
             "  -K, --up                Select window above\n" \
             "  -L, --right             Select window right\n" \
             "  -C, --current           Select current active window/view\n" \
             "  -x, --select            Select a window via pointer\n" \
             "\nGroups:\n" \
             "  -c, --clients           Use clients group\n" \
             "  -g, --gravity           Use gravity group\n" \
             "  -s, --screen            Use screen group\n" \
             "  -b, --sublets           Use sublets group\n" \
             "  -t, --tags              Use tags group\n" \
             "  -v, --views             Use views group\n", 
             getenv("DISPLAY"), PKG_NAME, PKG_NAME);
    }

  if(-1 == group || SUB_GROUP_CLIENT == group)
    {
      printf("\nOptions for clients:\n" \
             "  -f, --find=PATTERN      Find client\n" \
             "  -o, --focus=PATTERN     Set focus to client\n" \
             "  -F, --full              Toggle full\n" \
             "  -O, --float             Toggle float\n" \
             "  -S, --stick             Toggle stick\n" \
             "  -l, --list              List all clients\n" \
             "  -T, --tag=PATTERN       Add tag to client\n" \
             "  -U, --untag=PATTERN     Remove tag from client\n" \
             "  -G, --tags              Show client tags\n" \
             "  -y, --gravity           Set client gravity\n" \
             "  -n, --screen            Set client screen\n" \
             "  -E, --raise             Raise client window\n" \
             "  -R, --lower             Lower client window\n" \
             "  -k, --kill=PATTERN      Kill client\n");
    }

  if(-1 == group || SUB_GROUP_GRAVITY == group)
    {
      printf("\nOptions for gravities:\n" \
             "  -a, --add NAME GEOMETRY Create new gravity mode\n" \
             "  -l, --list              List all gravities\n" \
             "  -f, --find=PATTERN      Find a gravity\n" \
             "  -k, --kill=PATTERN      Kill gravity mode\n");
    }      

  if(-1 == group || SUB_GROUP_SCREEN == group)
    {
      printf("\nOptions for screens:\n" \
             "  -l, --list              List all screens\n" \
             "  -f, --find=ID           Find a screen\n");
    }      

  if(-1 == group || SUB_GROUP_SUBLET == group)
    {
      printf("\nOptions for sublets:\n" \
             "  -a, --add=FILE          Create new sublet\n" \
             "  -l, --list              List all sublets\n" \
             "  -u, --update            Updates value of sublet\n" \
             "  -A, --data              Set data of sublet\n" \
             "  -k, --kill=PATTERN      Kill sublet\n");
    }    

  if(-1 == group || SUB_GROUP_TAG == group)
    {
      printf("\nOptions for tags:\n" \
             "  -a, --add=NAME          Create new tag\n" \
             "  -f, --find              Find all clients/views by tag\n" \
             "  -l, --list              List all tags\n" \
             "  -k, --kill=PATTERN      Kill tag\n");
    }

  if(-1 == group || SUB_GROUP_VIEW == group)
    {
      printf("\nOptions for views:\n" \
             "  -a, --add=NAME          Create new view\n" \
             "  -f, --find=PATTERN      Find a view\n" \
             "  -l, --list              List all views\n" \
             "  -T, --tag=PATTERN       Add tag to view\n" \
             "  -U, --untag=PATTERN     Remove tag from view\n" \
             "  -G, --tags              Show view tags\n" \
             "  -k, --kill=VIEW         Kill view\n");
    }
  
  printf("\nFormats:\n" \
         "  DISPLAY:  :<display number>\n" \
         "  ID:       <number>\n" \
         "  GEOMETRY: <x>x<y>+<width>+<height>\n" \
         "  PATTERN:\n" \
         "    Matching clients, gravities, tags and views works either via plaintext, \n" \
         "    regex (see regex(7)), id or window id. If a pattern matches more than once\n" \
         "    ONLY the first match will be used. If the PATTERN is '-' %sr will\n" \
         "    read from stdin.\n", PKG_NAME);

  printf("\nListings:\n" \
         "  Client listing:  <window id> [-*] <view id> <geometry> <gravity> <screen> <flags> <name> (<class>)\n" \
         "  Gravity listing: <gravity id> <geometry>\n" \
         "  Screen listing:  <screen id> <geometry>\n" \
         "  Tag listing:     <name>\n" \
         "  View listing:    <window id> [-*] <view id> <name>\n");

  printf("\nExamples:\n" \
         "  %sr -c -l                List all clients\n" \
         "  %sr -t -a subtle         Add new tag 'subtle'\n" \
         "  %sr -v subtle -T rocks   Tag view 'subtle' with tag 'rocks'\n" \
         "  %sr -c xterm -G          Show tags of client 'xterm'\n" \
         "  %sr -c -x -f             Select client and show info\n" \
         "  %sr -c -C -y 5           Set gravity 5 to current active client\n" \
         "  %sr -g -a 5 10x10+50+50  Add new gravity mode to gravity 4\n" \
         "  %sr -t -f term           Show every client/view tagged with 'term'\n" \
         "\nPlease report bugs to <%s>\n",
         PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_BUGREPORT);
} /* }}} */

/* SubtlerVersion {{{ */
static void
SubtlerVersion(void)
{
  printf("%sr %s - Copyright (c) 2005-2009 Christoph Kappel\n" \
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

/* SubtlerParse {{{ */
static char *
SubtlerParse(char *string)
{
  char buf[256], *ret = NULL;

  if(string)
    {
      if(!strncmp(string, "-", 1)) ///< Read pipe
        {
          if(!fgets(buf, sizeof(buf), stdin)) 
            subSharedLogError("Failed reading from pipe\n");

          ret = (char *)subSharedMemoryAlloc(strlen(buf), sizeof(char));
          strncpy(ret, buf, strlen(buf) - 1);

          subSharedLogDebug("Parse: len=%d\n", strlen(buf));
        }
      else ret = strdup(string); ///< Just copy string
    }

  return ret;
} /* }}} */

/* SubtlerReload {{{ */
static void
SubtlerReload(void)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_RELOAD", data, True);
} /* }}} */

/* SubtlerQuit {{{ */
static void
SubtlerQuit(void)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_QUIT", data, True);
} /* }}} */

/* main {{{ */
int
main(int argc,
  char *argv[])
{
  Window win = None;
  int c, mod = -1, group = -1, action = -1;
  char *dispname = NULL, *arg1 = NULL, *arg2 = NULL;
  struct sigaction act;
  const struct option long_options[] = /* {{{ */
  {
    /* Groups */
    { "clients", no_argument,       0, 'c' },
    { "screen",  no_argument,       0, 'e' },
    { "sublets", no_argument,       0, 's' },
    { "tags",    no_argument,       0, 't' },
    { "views",   no_argument,       0, 'v' },

    /* Actions */
    { "add",     no_argument,       0, 'a' },
    { "kill",    no_argument,       0, 'k' },
    { "find",    no_argument,       0, 'f' },
    { "focus",   no_argument,       0, 'o' },
    { "full",    no_argument,       0, 'F' },
    { "float",   no_argument,       0, 'O' },
    { "stick",   no_argument,       0, 'S' },
    { "jump",    no_argument,       0, 'j' },
    { "list",    no_argument,       0, 'l' },
    { "tag",     no_argument,       0, 'T' },
    { "untag",   no_argument,       0, 'U' },
    { "tags",    no_argument,       0, 'G' },
    { "update",  no_argument,       0, 'u' },
    { "data",    no_argument,       0, 'A' },
    { "gravity", no_argument,       0, 'g' },
    { "raise",   no_argument,       0, 'E' },
    { "lower",   no_argument,       0, 'R' },

    /* Modifier */
    { "reload",  no_argument,       0, 'r' },
    { "quit",    no_argument,       0, 'q' },
    { "left",    no_argument,       0, 'H' },
    { "down",    no_argument,       0, 'J' },
    { "up",      no_argument,       0, 'K' },
    { "right",   no_argument,       0, 'L' },
    { "current", no_argument,       0, 'C' },
    { "select",  no_argument,       0, 'x' },

    /* Other */
#ifdef DEBUG
    { "debug",   no_argument,       0, 'D' },
#endif /* DEBUG */
    { "display", required_argument, 0, 'd' },
    { "help",    no_argument,       0, 'h' },
    { "version", no_argument,       0, 'V' },
    { 0, 0, 0, 0}
  }; /* }}} */

  /* Command table {{{ */
  const SubtlerCommand cmds[SUB_GROUP_TOTAL][SUB_ACTION_TOTAL] = { 
    /* Client, Gravity, Screen, Sublet, Tag, View <=> Add, Kill, Find, Focus, Full, Float, 
       Stick, Jump, List, Tag, Untag, Tags, Update, Data, Gravity, Screen, Raise, Lower */
    { NULL, SubtlerClientKill, SubtlerClientFind,  SubtlerClientFocus, 
      SubtlerClientToggleFull, SubtlerClientToggleFloat, SubtlerClientToggleStick, NULL,
      SubtlerClientList, SubtlerClientTag, SubtlerClientUntag, SubtlerClientTags, NULL, 
      NULL, SubtlerClientGravity, SubtlerClientScreen, SubtlerClientRestackRaise, 
      SubtlerClientRestackLower },
    { SubtlerGravityNew, SubtlerGravityKill, SubtlerGravityFind, NULL, NULL, NULL, NULL, NULL, 
      SubtlerGravityList, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },    
    { NULL, NULL, SubtlerScreenFind, NULL, NULL, NULL, NULL, NULL, SubtlerScreenList, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { SubtlerSubletNew, SubtlerSubletKill, NULL, NULL, NULL, NULL, NULL, NULL, 
      SubtlerSubletList, NULL, NULL, NULL, SubtlerSubletUpdate, SubtlerSubletData,
      NULL, NULL, NULL, NULL },
    { SubtlerTagNew, SubtlerTagKill, SubtlerTagFind, NULL, NULL, NULL, 
      NULL, NULL, SubtlerTagList, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { SubtlerViewNew, SubtlerViewKill, SubtlerViewFind, NULL, NULL, NULL, NULL,
      SubtlerViewJump, SubtlerViewList, SubtlerViewTag, SubtlerViewUntag, SubtlerViewTags, 
      NULL, NULL, NULL, NULL, NULL, NULL }
  }; /* }}} */

  /* Set up signal handler */
  act.sa_handler = SubtlerSignal;
  act.sa_flags   = 0;
  memset(&act.sa_mask, 0, sizeof(sigset_t)); ///< Avoid uninitialized values
  sigaction(SIGSEGV, &act, NULL);

  while((c = getopt_long(argc, argv, "cgsbtvakfoFOSjlTUGuAynERrqHJKLDd:hCxV", long_options, NULL)) != -1)
    {
      switch(c)
        {
          /* Groups */
          case 'c': group = SUB_GROUP_CLIENT;    break;
          case 'g': group = SUB_GROUP_GRAVITY;   break;
          case 's': group = SUB_GROUP_SCREEN;    break;
          case 'b': group = SUB_GROUP_SUBLET;    break;
          case 't': group = SUB_GROUP_TAG;       break;
          case 'v': group = SUB_GROUP_VIEW;      break;

          /* Actions */
          case 'a': action = SUB_ACTION_ADD;     break;
          case 'k': action = SUB_ACTION_KILL;    break;
          case 'f': action = SUB_ACTION_FIND;    break;
          case 'o': action = SUB_ACTION_FOCUS;   break;
          case 'F': action = SUB_ACTION_FULL;    break;
          case 'O': action = SUB_ACTION_FLOAT;   break;
          case 'S': action = SUB_ACTION_STICK;   break;
          case 'j': action = SUB_ACTION_JUMP;    break;
          case 'l': action = SUB_ACTION_LIST;    break;
          case 'T': action = SUB_ACTION_TAG;     break;
          case 'U': action = SUB_ACTION_UNTAG;   break;
          case 'G': action = SUB_ACTION_TAGS;    break;
          case 'u': action = SUB_ACTION_UPDATE;  break;
          case 'A': action = SUB_ACTION_DATA;    break;
          case 'y': action = SUB_ACTION_GRAVITY; break;
          case 'n': action = SUB_ACTION_SCREEN;  break;
          case 'E': action = SUB_ACTION_RAISE;   break;
          case 'R': action = SUB_ACTION_LOWER;   break;

          /* Modifier */
          case 'r': mod = SUB_MOD_RELOAD;        break;
          case 'q': mod = SUB_MOD_QUIT;          break;
          case 'H': mod = SUB_MOD_LEFT;          break;
          case 'J': mod = SUB_MOD_DOWN;          break;
          case 'K': mod = SUB_MOD_UP;            break;
          case 'L': mod = SUB_MOD_RIGHT;         break;
          case 'C': mod = SUB_MOD_CURRENT;       break;
          case 'x': mod = SUB_MOD_SELECT;        break;

          /* Other */
          case 'd': dispname = optarg;           break;
          case 'h': SubtlerUsage(group);         return 0;
#ifdef DEBUG          
          case 'D': debug = 1;                   break;
#else /* DEBUG */
          case 'D': 
            printf("Please recompile %sr with `debug=yes'\n", PKG_NAME); 
            return 0;          
#endif /* DEBUG */
          case 'V': SubtlerVersion();            return 0;
          case '?':
            printf("Try `%sr --help' for more information\n", PKG_NAME);
            return -1;
        }
    }

  /* Open connection to server */
  if(!(display = XOpenDisplay(dispname)))
    {
      printf("Failed opening display `%s'.\n", (dispname) ? dispname : ":0.0");

      return -1;
    }
  XSetErrorHandler(subSharedLogXError);

  /* Check if subtle is running */
  if(True != subSharedSubtleRunning())
    {
      XCloseDisplay(display);
      display = NULL;
      
      subSharedLogError("Failed finding running %s\n", PKG_NAME);

      return -1;
    }
 
  /* Get arguments */
  switch(mod)
    {
      case SUB_MOD_RELOAD: SubtlerReload();                       return 0;
      case SUB_MOD_QUIT:   SubtlerQuit();                         return 0;

      /* Stupid - but easier */
      case SUB_MOD_LEFT:   arg1 = SubtlerMatch(SUB_WINDOW_LEFT);  break;
      case SUB_MOD_DOWN:   arg1 = SubtlerMatch(SUB_WINDOW_DOWN);  break;
      case SUB_MOD_UP:     arg1 = SubtlerMatch(SUB_WINDOW_UP);    break;
      case SUB_MOD_RIGHT:  arg1 = SubtlerMatch(SUB_WINDOW_RIGHT); break;

      case SUB_MOD_CURRENT:
        if(SUB_GROUP_VIEW == group) ///< Current of correct group
          {
            arg1 = SubtlerCurrentView();
          }
        else arg1 = SubtlerCurrentClient();
        break;
      case SUB_MOD_SELECT:
        if(None != (win = subSharedWindowSelect()))
          {
            char buf[20];

            /* Use window id for matching */
            snprintf(buf, sizeof(buf), "%#lx", win);
            arg1 = strdup(buf);
          }
        break;
      default:
        if(argc > optind)     arg1 = SubtlerParse(argv[optind]);
        if(argc > optind + 1) arg2 = SubtlerParse(argv[optind + 1]);
    }
  if(-1 != mod && argc > optind) ///< Get arg2
    arg2 = SubtlerParse(argv[optind]);

  /* Select command */
  if(-1 != group && -1 != action && cmds[group][action]) 
    {
      cmds[group][action](arg1, arg2);
    }
  else if((SUB_MOD_CURRENT == mod || (SUB_MOD_LEFT <= mod && SUB_MOD_RIGHT >= mod)) 
    && (SUB_GROUP_VIEW == group || SUB_GROUP_CLIENT == group))
    {
      printf("%s\n", arg1 ? arg1 : "None"); ///< Print selected client
    }
  else SubtlerUsage(group);

  XCloseDisplay(display);
  if(arg1) free(arg1);
  if(arg2) free(arg2);
  
  return 0;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
