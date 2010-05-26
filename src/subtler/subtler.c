
 /**
  * @package subtle
  *
  * @file subtle remote client
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <locale.h>
#include "shared.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

Display *display = NULL;

/* Typedefs {{{ */
typedef void(*SubtlerCommand)(int, char *, char *);
/* }}} */

/* Macros {{{ */
#define CHECK(cond,...) \
  if(!(cond)) \
    { \
      subSharedLog(3, __FILE__, __LINE__, __VA_ARGS__); \
      exit(-1); \
    }
/* }}} */

/* Flags {{{ */
/* Groups */
#define SUB_GROUP_CLIENT       0   ///< Group client
#define SUB_GROUP_GRAVITY      1   ///< Group gravity
#define SUB_GROUP_SCREEN       2   ///< Group screen
#define SUB_GROUP_SUBLET       3   ///< Group sublet
#define SUB_GROUP_TAG          4   ///< Group tag
#define SUB_GROUP_TRAY         5   ///< Group tray
#define SUB_GROUP_VIEW         6   ///< Group view
#define SUB_GROUP_TOTAL        7   ///< Group total

/* Actions */
#define SUB_ACTION_ADD         0   ///< Action add
#define SUB_ACTION_KILL        1   ///< Action kill
#define SUB_ACTION_FIND        2   ///< Action find
#define SUB_ACTION_FOCUS       3   ///< Action focus
#define SUB_ACTION_FULL        4   ///< Action max
#define SUB_ACTION_FLOAT       5   ///< Action float
#define SUB_ACTION_STICK       6   ///< Action stick
#define SUB_ACTION_JUMP        7   ///< Action jump
#define SUB_ACTION_LIST        8   ///< Action list
#define SUB_ACTION_TAG         9   ///< Action tag
#define SUB_ACTION_UNTAG      10   ///< Action untag
#define SUB_ACTION_TAGS       11   ///< Action tags
#define SUB_ACTION_CLIENTS    12   ///< Action clients
#define SUB_ACTION_UPDATE     13   ///< Action update
#define SUB_ACTION_DATA       14   ///< Action data
#define SUB_ACTION_GRAVITY    15   ///< Action gravity
#define SUB_ACTION_SCREEN     16   ///< Action screen
#define SUB_ACTION_RAISE      17   ///< Action raise
#define SUB_ACTION_LOWER      18   ///< Action lower
#define SUB_ACTION_TOTAL      19   ///< Action total

/* Modifier */
#define SUB_MOD_CURRENT        1   ///< Mod current
#define SUB_MOD_SELECT         2   ///< Mod select
#define SUB_MOD_RELOAD_CONFIG  3   ///< Mod reload config
#define SUB_MOD_RELOAD_SUBLETS 4   ///< Mod reload sublets
#define SUB_MOD_RESTART        5   ///< Mod restart
#define SUB_MOD_QUIT           6   ///< Mod quit
#define SUB_MOD_TOTAL          7   ///< Mod total
/* }}} */

/* SubtlerToggle {{{ */
static void
SubtlerToggle(char *name,
  char *type)
{
  Window win = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  if(-1 != subSharedClientFind(name, NULL, &win,
      (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS)))
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

  if(-1 != subSharedClientFind(name, NULL, &win,
      (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS)))
    {
      data.l[0] = 2; ///< Mimic a pager
      data.l[1] = win;
      data.l[2] = detail;

      subSharedMessage(DefaultRootWindow(display), "_NET_RESTACK_WINDOW", data, False);
    }
  else subSharedLogWarn("Failed restacking client");
} /* }}} */

/* Client */

/* SubtlerCurrentClient {{{ */
static char *
SubtlerCurrentClient(void)
{
  char *ret = NULL;
  unsigned long *focus = NULL;

  if((focus = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_WINDOW,
      XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
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

  /* Collect view data */
  names = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size);
  cv    = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

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
  char *instance = NULL, *klass = NULL, *grav = NULL, buf[5] = { 0 };
  unsigned int width, height, border;
  unsigned long *cv = NULL, *gravity = NULL, *screen = NULL, *flags = NULL;

  /* Collect client data */
  subSharedPropertyClass(display, win, &instance, &klass);
  cv      = (unsigned long*)subSharedPropertyGet(display, win, XA_CARDINAL,
    XInternAtom(display, "_NET_WM_DESKTOP", False), NULL);
  gravity = (unsigned long*)subSharedPropertyGet(display, win, XA_CARDINAL,
    XInternAtom(display, "SUBTLE_WINDOW_GRAVITY", False), NULL);
  screen  = (unsigned long*)subSharedPropertyGet(display, win, XA_CARDINAL,
    XInternAtom(display, "SUBTLE_WINDOW_SCREEN", False), NULL);
  flags   = (unsigned long*)subSharedPropertyGet(display, win, XA_CARDINAL,
    XInternAtom(display, "SUBTLE_WINDOW_FLAGS", False), NULL);

  /* Get gravity */
  snprintf(buf, sizeof(buf), "%ld", *gravity);
  subSharedGravityFind(buf, &grav, NULL);

  XGetGeometry(display, win, &unused, &x, &y, &width, &height, &border, &border);

  printf("%#10lx %c %ld %4u x %4u + %4u + %-4u %12.12s %ld %c%c%c %s (%s)\n", win,
    (*cv == rv ? '*' : '-'), (*cv > nv ? -1 : *cv + 1), x, y, width, height,
    grav ? grav : "-", *screen,
    *flags & SUB_EWMH_FULL  ? 'F' : '-',
    *flags & SUB_EWMH_FLOAT ? 'O' : '-',
    *flags & SUB_EWMH_STICK ? 'S' : '-', instance, klass);

  free(instance);
  free(klass);
  free(cv);
  free(gravity);
  free(screen);
  free(flags);
  free(grav);
} /* }}} */

/* SubtlerClientFind {{{ */
static void
SubtlerClientFind(int argc,
  char *arg1,
  char *arg2)
{
  Window win;

  CHECK(1 == argc, "Usage: %sr -c -f PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, NULL, &win,
      (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS)))
    {
      unsigned long *nv = NULL, *rv = NULL;

      /* Collect data */
      nv = (unsigned long *)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False), NULL);
      rv = (unsigned long*)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

      SubtlerClientPrint(win, *nv, *rv);

      free(nv);
      free(rv);
    }
  else subSharedLogWarn("Failed finding client\n");
} /* }}} */

/* SubtlerClientFocus {{{ */
static void
SubtlerClientFocus(int argc,
  char *arg1,
  char *arg2)
{
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -c -o CLIENT\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, NULL, &win,
      (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS)))
    {
      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, False);
    }
  else subSharedLogWarn("Failed finding client\n");
} /* }}} */

/* SubtlerClientToggleFull {{{ */
static void
SubtlerClientToggleFull(int argc,
  char *arg1,
  char *arg2)
{
  CHECK(1 == argc, "Usage: %sr -c -F CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_FULLSCREEN");
} /* }}} */

/* SubtlerClientToggleFloat {{{ */
static void
SubtlerClientToggleFloat(int argc,
  char *arg1,
  char *arg2)
{
  CHECK(1 == argc, "Usage: %sr -c -O CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_ABOVE");
} /* }}} */

/* SubtlerClientToggleStick {{{ */
static void
SubtlerClientToggleStick(int argc,
  char *arg1,
  char *arg2)
{
  CHECK(1 == argc, "Usage: %sr -c -S CLIENT\n", PKG_NAME);

  SubtlerToggle(arg1, "_NET_WM_STATE_STICKY");
} /* }}} */

/* SubtlerClientRestackRaise {{{ */
static void
SubtlerClientRestackRaise(int argc,
  char *arg1,
  char *arg2)
{
  CHECK(1 == argc, "Usage: %sr -c -E CLIENT\n", PKG_NAME);

  SubtlerRestack(arg1, Above);
} /* }}} */

/* SubtlerClientRestackLower {{{ */
static void
SubtlerClientRestackLower(int argc,
  char *arg1,
  char *arg2)
{
  CHECK(1 == argc, "Usage: %sr -c -R CLIENT\n", PKG_NAME);

  SubtlerRestack(arg1, Below);
} /* }}} */

/* SubtlerClientList {{{ */
static void
SubtlerClientList(int argc,
  char *arg1,
  char *arg2)
{
  int i, size = 0;
  Window *clients = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((clients = subSharedClientList(&size)))
    {
      unsigned long *nv = NULL, *rv = NULL;

      /* Collect data */
      nv = (unsigned long *)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False), NULL);
      rv = (unsigned long*)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

      /* Print clients */
      for(i = 0; i < size; i++)
        SubtlerClientPrint(clients[i], *nv, *rv);

      free(clients);
      free(nv);
      free(rv);
    }
} /* }}} */

/* SubtlerClientTag {{{ */
static void
SubtlerClientTag(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(2 == argc, "Usage: %sr -c PATTERN -T PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL, NULL,
    (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS));
  data.l[1] = subSharedTagFind(arg2, NULL);

  if(-1 != data.l[0] && -1 != data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_TAG", data, False);
  else subSharedLogWarn("Failed tagging client\n");
} /* }}} */

/* SubtlerClientUntag {{{ */
static void
SubtlerClientUntag(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(2 == argc, "Usage: %sr -c PATTERN -U PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL, NULL,
    (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS));
  data.l[1] = subSharedTagFind(arg2, NULL);

  if(-1 != data.l[0] && -1 != data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_UNTAG", data, False);
  else subSharedLogWarn("Failed untagging client\n");
} /* }}} */

/* SubtlerClientGravity {{{ */
static void
SubtlerClientGravity(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(2 == argc, "Usage: %sr -c PATTERN -y NUMBER\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL, NULL,
    (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS));
  data.l[1] = subSharedGravityFind(arg2, NULL, NULL);

  if(-1 != data.l[0] && -1 != data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_GRAVITY", data, False);
  else subSharedLogWarn("Failed setting client gravity\n");
} /* }}} */

/* SubtlerClientScreen {{{ */
static void
SubtlerClientScreen(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(2 == argc, "Usage: %sr -c PATTERN -n NUMBER\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  data.l[0] = subSharedClientFind(arg1, NULL, NULL,
    (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS));
  data.l[1] = atoi(arg2);

  if(-1 != data.l[0] && 0 <= data.l[1])
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_SCREEN", data, False);
  else subSharedLogWarn("Failed setting client screen\n");
} /* }}} */

/* SubtlerClientTags {{{ */
static void
SubtlerClientTags(int argc,
  char *arg1,
  char *arg2)
{
  Window win;

  CHECK(1 == argc, "Usage: %sr -c PATTERN -G\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, NULL, &win,
      (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS)))
    {
      int i, size = 0;
      unsigned long *flags = (unsigned long *)subSharedPropertyGet(display,
        win, XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_TAGS", False),
        NULL);
      char **tags = subSharedPropertyStrings(display, DefaultRootWindow(display),
        XInternAtom(display, "SUBTLE_TAG_LIST", False), &size);

      for(i = 0; i < size; i++)
        if((int)*flags & (1L << (i + 1))) printf("%s\n", tags[i]);

      XFreeStringList(tags);
      free(flags);
    }
  else subSharedLogWarn("Failed fetching client tags\n");
} /* }}} */

/* SubtlerClientKill {{{ */
static void
SubtlerClientKill(int argc,
  char *arg1,
  char *arg2)
{
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -c -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedClientFind(arg1, NULL, &win,
      (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS)))
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
SubtlerGravityNew(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };
  XRectangle geometry = { 0 };

  CHECK(2 == argc, "Usage: %sr -g -a NAME GEOMETRY\n", PKG_NAME);
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
SubtlerGravityFind(int argc,
  char *arg1,
  char *arg2)
{
  XRectangle geometry = { 0 };
  char *name = NULL;

  CHECK(1 == argc, "Usage: %sr -g -f GRAVITY\n", PKG_NAME);
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
SubtlerGravityList(int argc,
  char *arg1,
  char *arg2)
{
  int size = 0;
  char **gravities = NULL;

  /* Get gravity list */
  if((gravities = subSharedPropertyStrings(display,
      DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_GRAVITY_LIST", False), &size)))
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
} /* }}} */

/* SubtlerGravityClients {{{ */
static void
SubtlerGravityClients(int argc,
  char *arg1,
  char *arg2)
{
  int i, id = 0, size = 0;
  Window *clients = NULL;
  unsigned long *gravity = NULL, *nv = NULL, *rv = NULL;

  CHECK(1 == argc, "Usage: %sr -g GRAVITY -I\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Find tag */
  if(-1 != (id = subSharedGravityFind(arg1, NULL, NULL)))
    {
      clients = subSharedClientList(&size);
      nv      = (unsigned long *)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False), NULL);
      rv      = (unsigned long*)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

      for(i = 0; i < size; i++)
        {
          gravity = (unsigned long*)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_GRAVITY", False), NULL);

          if(*gravity == id) ///< Check if gravity id matches
            SubtlerClientPrint(clients[i], *nv, *rv);

          free(gravity);
        }

      free(clients);
      free(nv);
      free(rv);
    }
  else subSharedLogWarn("Failed finding tag\n");
} /* }}} */

/* SubtlerGravityKill {{{ */
static void
SubtlerGravityKill(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(2 == argc, "Usage: %sr -g -k GRAVITY\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedGravityFind(arg1, NULL, NULL)))
      subSharedMessage(DefaultRootWindow(display), "SUBTLE_GRAVITY_KILL", data, False);
  else subSharedLogWarn("Failed killing gravity\n");
} /* }}} */

/* Screen */

/* SubtlerScreenFind {{{ */
static void
SubtlerScreenFind(int argc,
  char *arg1,
  char *arg2)
{
  int n = 0, find = 0;

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  CHECK(1 == argc, "Usage: %sr -e -f ID\n", PKG_NAME);
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
SubtlerScreenList(int argc,
  char *arg1,
  char *arg2)
{
  int n = 0;
  unsigned long len = 0;
  XRectangle workarea = { 0 };
  long *workareas = NULL;

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  subSharedLogDebug("%s\n", __func__);

  /* Get workarea list */
  if((workareas = (long *)subSharedPropertyGet(display, DefaultRootWindow(display),
      XA_CARDINAL, XInternAtom(display, "_NET_WORKAREA", False), &len)))
    {
        workarea.x      = workareas[0];
        workarea.y      = workareas[1];
        workarea.width  = workareas[2];
        workarea.height = workareas[3];

        free(workareas);
    }
  else subSharedLogWarn("Failed getting workarea list\n");

  /* First screen */
  printf("0 %4d x %-4d %4d + %-4d\n", workarea.x, workarea.y,
    workarea.width, workarea.height);

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
          for(i = 1; i < n; i++)
            printf("%d %4d x %-4d %4d + %-4d\n", i, screens[i].x_org, screens[i].y_org,
              screens[i].width, screens[i].height);

          XFree(screens);
        }
    }
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */
} /* }}} */

/* Sublet */

/* SubtlerSubletNew {{{ */
static void
SubtlerSubletNew(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -s -a PATH\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  snprintf(data.b, sizeof(data.b), "%s", arg1);

  if(!subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_NEW", data, False))
    subSharedLogWarn("Failed creating sublet\n");
} /* }}} */

/* SubtlerSubletList {{{ */
static void
SubtlerSubletList(int argc,
  char *arg1,
  char *arg2)
{
  int i, size = 0;
  char **sublets = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((sublets = subSharedPropertyStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_SUBLET_LIST", False), &size)))
    {
      for(i = 0; i < size; i++)
        printf("%s\n", sublets[i]);

      XFreeStringList(sublets);
    }
} /* }}} */

/* SubtlerSubletUpdate {{{ */
static void
SubtlerSubletUpdate(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -s -u PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedSubletFind(arg1, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_UPDATE", data, False);
  else subSharedLogWarn("Failed updating sublet\n");
} /* }}} */

/* SubtlerSubletData {{{ */
static void
SubtlerSubletData(int argc,
  char *arg1,
  char *arg2)
{
  int id = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -s PATTERN -A DATA\n", PKG_NAME);
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
SubtlerSubletKill(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -s -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedSubletFind(arg1, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_KILL", data, False);
  else subSharedLogWarn("Failed killing sublet\n");
} /* }}} */

/* Tag */

/* SubtlerTagNew {{{ */
static void
SubtlerTagNew(int argc,
  char *arg1,
  char *arg2)
{
  CHECK(1 == argc, "Usage: %sr -t -a NAME\n", PKG_NAME);
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
SubtlerTagFind(int argc,
  char *arg1,
  char *arg2)
{
  int i, tag = -1, size_clients = 0, size_views = 0;
  char **names = NULL;
  Window *views = NULL, *clients = NULL;

  CHECK(1 == argc, "Usage: %sr -t -f NAME\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Collect data */
  tag     = subSharedTagFind(arg1, NULL);
  names   = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size_views);
  views  = (Window *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_WINDOW, XInternAtom(display, "_NET_VIRTUAL_ROOTS", False), NULL);
  clients = subSharedClientList(&size_clients);

  /* Views */
  if(-1 != tag && names && views)
    {
      for(i = 0; i < size_views; i++)
        {
          unsigned long *flags = (unsigned long *)subSharedPropertyGet(display,
            views[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

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
          unsigned long *flags   = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

          subSharedPropertyClass(display, clients[i], &name, &klass);

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
SubtlerTagList(int argc,
  char *arg1,
  char *arg2)
{
  int i, size = 0;
  char **tags = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((tags = subSharedPropertyStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_TAG_LIST", False), &size)))
    {
      for(i = 0; i < size; i++) printf("%s\n", tags[i]);

      XFreeStringList(tags);
    }
} /* }}} */

/* SubtlerTagClients {{{ */
static void
SubtlerTagClients(int argc,
  char *arg1,
  char *arg2)
{
  int i, id = 0, size = 0;
  Window *clients = NULL;
  unsigned long *tags = NULL, *nv = NULL, *rv = NULL;

  CHECK(1 == argc, "Usage: %sr -t PATTERN -I\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Find tag */
  if(-1 != (id = subSharedTagFind(arg1, NULL)))
    {
      clients = subSharedClientList(&size);
      nv      = (unsigned long *)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False), NULL);
      rv      = (unsigned long*)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

      for(i = 0; i < size; i++)
        {
          tags = (unsigned long *)subSharedPropertyGet(display, clients[i],
            XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

          if(*tags & (1L << (id + 1))) ///< Check if tag id matches
            SubtlerClientPrint(clients[i], *nv, *rv);

          free(tags);
        }

      free(clients);
      free(nv);
      free(rv);
    }
  else subSharedLogWarn("Failed finding tag\n");
} /* }}} */

/* SubtlerTagKill {{{ */
static void
SubtlerTagKill(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -t -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedTagFind(arg1, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_KILL", data, False);
  else subSharedLogWarn("Failed killing tag\n");
} /* }}} */

/* Tray */

/* SubtlerTrayPrint {{{ */
void
SubtlerTrayPrint(Window win)
{
  char *inst = NULL, *klass = NULL;

  subSharedPropertyClass(display, win, &inst, &klass);

  printf("%#10lx %s (%s)\n", win, inst, klass);

  free(inst);
  free(klass);
} /* }}} */

/* SubtlerTagFind {{{ */
static void
SubtlerTrayFind(int argc,
  char *arg1,
  char *arg2)
{
  Window win;

  CHECK(1 == argc, "Usage: %sr -r -f PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedTrayFind(arg1, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    {
      SubtlerTrayPrint(win);
    }
  else subSharedLogWarn("Failed finding tray\n");
} /* }}} */

/* SubtlerTrayFocus {{{ */
static void
SubtlerTrayFocus(int argc,
  char *arg1,
  char *arg2)
{
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -r -o CLIENT\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedTrayFind(arg1, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    {
      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, False);
    }
  else subSharedLogWarn("Failed finding tray\n");
} /* }}} */

/* SubtlerTrayList {{{ */
static void
SubtlerTrayList(int argc,
  char *arg1,
  char *arg2)
{
  int i, size = 0;
  Window *trays = NULL;

  subSharedLogDebug("%s\n", __func__);

  if((trays = subSharedTrayList(&size)))
    {
      for(i = 0; i < size; i++)
        SubtlerTrayPrint(trays[i]);

      free(trays);
    }
} /* }}} */

/* SubtlerTrayKill {{{ */
static void
SubtlerTrayKill(int argc,
  char *arg1,
  char *arg2)
{
  Window win;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -r -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (data.l[0] = subSharedTrayFind(arg1, NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS))))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_TRAY_KILL", data, False);
  else subSharedLogWarn("Failed killing tray\n");
} /* }}} */

/* View */

/* SubtlerViewNew {{{ */
static void
SubtlerViewNew(int argc,
  char *arg1,
  char *arg2)
{
  CHECK(1 == argc, "Usage: %sr -t -a PATTERN\n", PKG_NAME);
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
SubtlerViewFind(int argc,
  char *arg1,
  char *arg2)
{
  int id;
  Window win;
  char *name = NULL;

  CHECK(1 == argc, "Usage: %sr -v -f PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != (id = subSharedViewFind(arg1, &name, &win)) )
    {
      unsigned long *cv = NULL, *rv = NULL;

      /* Collect data */
      cv = (unsigned long *)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);
      rv = (unsigned long*)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

      printf("%#10lx %c %2d %s\n", win, (*cv == *rv ? '*' : '-'), id, name);

      free(name);
      free(cv);
      free(rv);
    }
  else subSharedLogWarn("Failed finding view\n");
} /* }}} */

/* SubtlerViewJump {{{ */
static void
SubtlerViewJump(int argc,
  char *arg1,
  char *arg2)
{
  int view = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -v -j PATTERN\n", PKG_NAME);
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
SubtlerViewList(int argc,
  char *arg1,
  char *arg2)
{
  int i, size = 0;
  unsigned long *nv = NULL, *cv = NULL;
  char **names = NULL;
  Window *views = NULL;

  subSharedLogDebug("%s\n", __func__);

  /* Collect data */
  nv     = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False), NULL);
  cv     = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);
  names  = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size);
  views = (Window *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_WINDOW, XInternAtom(display, "_NET_VIRTUAL_ROOTS", False), NULL);

  if(nv && cv && names && views)
    {
      for(i = 0; *nv && i < *nv; i++)
        printf("%#10lx %c %2d %s\n", views[i], (i == *cv ? '*' : '-'),
          i, names[i]);

      XFreeStringList(names);
      free(nv);
      free(cv);
      free(views);
    }
  else subSharedLogWarn("Failed getting view list\n");
} /* }}} */

/* SubtlerViewTag {{{ */
static void
SubtlerViewTag(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(2 == argc, "Usage: %sr -v PATTERN -T PATTERN\n", PKG_NAME);
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
SubtlerViewUntag(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(2 == argc, "Usage: %sr -v PATTERN -U PATTERN\n", PKG_NAME);
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
SubtlerViewTags(int argc,
  char *arg1,
  char *arg2)
{
  int i, size = 0;
  Window win;
  char **tags = NULL;
  unsigned long *flags = NULL;

  CHECK(1 == argc, "Usage: %sr -v PATTERN -G\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if(-1 != subSharedViewFind(arg1, NULL, &win))
    {
      flags = (unsigned long *)subSharedPropertyGet(display, win,
        XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);
      tags  = subSharedPropertyStrings(display, DefaultRootWindow(display),
        XInternAtom(display, "SUBTLE_TAG_LIST", False), &size);

      for(i = 0; i < size; i++)
        if((int)*flags & (1L << (i + 1))) printf("%s\n", tags[i]);

      XFreeStringList(tags);
      free(flags);
    }
  else subSharedLogWarn("Failed finding view\n");
} /* }}} */

/* SubtlerViewClients {{{ */
static void
SubtlerViewClients(int argc,
  char *arg1,
  char *arg2)
{
  int i, size = 0;
  Window win;
  Window *clients = NULL;
  unsigned long *tags1 = NULL, *tags2 = NULL, *nv = NULL, *rv = NULL;

  CHECK(1 == argc, "Usage: %sr -v PATTERN -I\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  /* Find view */
  if(-1 != subSharedViewFind(arg1, NULL, &win))
    {
      /* Fetch data */
      tags1  = (unsigned long *)subSharedPropertyGet(display, win, XA_CARDINAL,
        XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);
      clients = subSharedClientList(&size);
      nv      = (unsigned long *)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False), NULL);
      rv      = (unsigned long*)subSharedPropertyGet(display,
        DefaultRootWindow(display), XA_CARDINAL,
        XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

      for(i = 0; i < size; i++)
        {
          tags2 = (unsigned long *)subSharedPropertyGet(display, clients[i],
            XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

          if(*tags1 & *tags2) ///< Check if there are common tags
            SubtlerClientPrint(clients[i], *nv, *rv);

          free(tags2);
        }

      free(tags1);
      free(clients);
      free(nv);
      free(rv);
    }
  else subSharedLogWarn("Failed finding view\n");
} /* }}} */

/* SubtlerViewKill {{{ */
static void
SubtlerViewKill(int argc,
  char *arg1,
  char *arg2)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  CHECK(1 == argc, "Usage: %sr -v -k PATTERN\n", PKG_NAME);
  subSharedLogDebug("%s\n", __func__);

  if((data.l[0] = subSharedViewFind(arg1, NULL, NULL)))
    subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_KILL", data, False);
  else subSharedLogWarn("Failed killing view\n");
} /* }}} */

/* Misc */

/* SubtlerUsage {{{ */
static void
SubtlerUsage(int group)
{
  printf("Usage: %sr [GENERIC|MODIFIER] [GROUP] [ACTION]\n", PKG_NAME);

  if(-1 == group)
    {
      printf("\nGeneric:\n" \
             "  -d, --display=DISPLAY   Connect to DISPLAY (default: %s)\n" \
             "  -D, --debug             Print debugging messages\n" \
             "  -h, --help              Show this help and exit\n" \
             "  -V, --version           Show version info and exit\n" \
             "\nModifier:\n" \
             "  -r, --reload-config     Reload config\n" \
             "  -R, --reload-sublets    Reload sublets\n" \
             "  -q, --restart           Restart subtle\n" \
             "  -Q, --quit              Quit %s\n" \
             "  -C, --current           Select current active window/view\n" \
             "  -X, --select            Select a window via pointer\n" \
             "\nGroups:\n" \
             "  -c, --client            Use client group\n" \
             "  -g, --gravity           Use gravity group\n" \
             "  -e, --screen            Use screen group\n" \
             "  -s, --sublet            Use sublet group\n" \
             "  -t, --tag               Use tag group\n" \
             "  -y, --tray              Use tray group\n" \
             "  -v, --view              Use views group\n",
             getenv("DISPLAY"), PKG_NAME);
    }

  if(-1 == group || SUB_GROUP_CLIENT == group)
    {
      printf("\nActions for clients:\n" \
             "  -f, --find=PATTERN      Find client\n" \
             "  -o, --focus=PATTERN     Set focus to client\n" \
             "  -F, --full              Toggle full\n" \
             "  -O, --float             Toggle float\n" \
             "  -S, --stick             Toggle stick\n" \
             "  -l, --list              List all clients\n" \
             "  -T, --tag=PATTERN       Add tag to client\n" \
             "  -U, --untag=PATTERN     Remove tag from client\n" \
             "  -G, --tags              Show client tags\n" \
             "  -Y, --gravity           Set client gravity\n" \
             "  -n, --screen            Set client screen\n" \
             "  -E, --raise             Raise client window\n" \
             "  -L, --lower             Lower client window\n" \
             "  -k, --kill=PATTERN      Kill client\n");
    }

  if(-1 == group || SUB_GROUP_GRAVITY == group)
    {
      printf("\nActions for gravities:\n" \
             "  -a, --add=NAME          Create new gravity\n" \
             "  -l, --list              List all gravities\n" \
             "  -f, --find=PATTERN      Find a gravity\n" \
             "  -k, --kill=PATTERN      Kill gravity mode\n");
    }

  if(-1 == group || SUB_GROUP_SCREEN == group)
    {
      printf("\nActions for screens:\n" \
             "  -l, --list              List all screens\n" \
             "  -f, --find=ID           Find a screen\n");
    }

  if(-1 == group || SUB_GROUP_SUBLET == group)
    {
      printf("\nActions for sublets:\n" \
             "  -a, --add=FILE          Create new sublet\n" \
             "  -l, --list              List all sublets\n" \
             "  -u, --update            Updates value of sublet\n" \
             "  -A, --data              Set data of sublet\n" \
             "  -k, --kill=PATTERN      Kill sublet\n");
    }

  if(-1 == group || SUB_GROUP_TAG == group)
    {
      printf("\nActions for tags:\n" \
             "  -a, --add=NAME          Create new tag\n" \
             "  -f, --find              Find all clients/views by tag\n" \
             "  -l, --list              List all tags\n" \
             "  -I, --clients           Show clients with tag\n" \
             "  -k, --kill=PATTERN      Kill tag\n");
    }

  if(-1 == group || SUB_GROUP_VIEW == group)
    {
      printf("\nActions for views:\n" \
             "  -a, --add=NAME          Create new view\n" \
             "  -f, --find=PATTERN      Find a view\n" \
             "  -l, --list              List all views\n" \
             "  -T, --tag=PATTERN       Add tag to view\n" \
             "  -U, --untag=PATTERN     Remove tag from view\n" \
             "  -G, --tags              Show view tags\n" \
             "  -I, --clients           Show clients on view\n" \
             "  -k, --kill=VIEW         Kill view\n");
    }

  printf("\nFormats:\n" \
         "  DISPLAY:  :<display number>\n" \
         "  ID:       <number>\n" \
         "  GEOMETRY: <x>x<y>+<width>+<height>\n" \
         "  PATTERN:\n" \
         "    Matching works either via plaintext, regex (see regex(7)), id or window id" \
         "    if applicable. If a pattern matches more than once ONLY the first match will" \
         "    be used." \
         "    If the PATTERN is '-' %sr will read from stdin.\n", PKG_NAME);

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
         "  %sr -t -f term           Show every client/view tagged with 'term'\n" \
         "\nPlease report bugs to <%s>\n",
         PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_NAME, PKG_BUGREPORT);
} /* }}} */

/* SubtlerVersion {{{ */
static void
SubtlerVersion(void)
{
  printf("%sr %s - Copyright (c) 2005-2010 Christoph Kappel\n" \
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
  char buf[256] = { 0 }, *ret = NULL;

  if(string)
    {
      if(!strncmp(string, "-", 1)) ///< Read pipe
        {
          if(!fgets(buf, sizeof(buf), stdin))
            {
              subSharedLogError("Failed reading from pipe\n");
              abort();
            }

          ret = (char *)subSharedMemoryAlloc(strlen(buf), sizeof(char));
          strncpy(ret, buf, strlen(buf) - 1);

          subSharedLogDebug("Parse: len=%d\n", strlen(buf));
        }
      else ret = strdup(string); ///< Just copy string
    }

  return ret;
} /* }}} */

/* main {{{ */
int
main(int argc,
  char *argv[])
{
  Window win = None;
  int c, mod = -1, group = -1, action = -1, args = 0;
  char *dispname = NULL, *arg1 = NULL, *arg2 = NULL;
  struct sigaction act;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };
  const struct option long_options[] = /* {{{ */
  {
    /* Groups */
    { "client",         no_argument,       0, 'c' },
    { "gravity",        no_argument,       0, 'g' },
    { "screen",         no_argument,       0, 'e' },
    { "sublet",         no_argument,       0, 's' },
    { "tag",            no_argument,       0, 't' },
    { "tray",           no_argument,       0, 'y' },
    { "view",           no_argument,       0, 'v' },

    /* Actions */
    { "add",            no_argument,       0, 'a' },
    { "kill",           no_argument,       0, 'k' },
    { "find",           no_argument,       0, 'f' },
    { "focus",          no_argument,       0, 'o' },
    { "full",           no_argument,       0, 'F' },
    { "float",          no_argument,       0, 'O' },
    { "stick",          no_argument,       0, 'S' },
    { "jump",           no_argument,       0, 'j' },
    { "list",           no_argument,       0, 'l' },
    { "tag",            no_argument,       0, 'T' },
    { "untag",          no_argument,       0, 'U' },
    { "tags",           no_argument,       0, 'G' },
    { "update",         no_argument,       0, 'u' },
    { "data",           no_argument,       0, 'A' },
    { "gravity",        no_argument,       0, 'Y' },
    { "raise",          no_argument,       0, 'E' },
    { "lower",          no_argument,       0, 'L' },

    /* Modifier */
    { "reload-config",  no_argument,       0, 'r' },
    { "reload-sublets", no_argument,       0, 'R' },
    { "quit",           no_argument,       0, 'Q' },
    { "current",        no_argument,       0, 'C' },
    { "select",         no_argument,       0, 'X' },

    /* Other */
#ifdef DEBUG
    { "debug",          no_argument,       0, 'D' },
#endif /* DEBUG */
    { "display",        required_argument, 0, 'd' },
    { "help",           no_argument,       0, 'h' },
    { "version",        no_argument,       0, 'V' },
    { 0, 0, 0, 0}
  }; /* }}} */

  /* Command table {{{ */
  const SubtlerCommand cmds[SUB_GROUP_TOTAL][SUB_ACTION_TOTAL] = {
    /* Client, Gravity, Screen, Sublet, Tag, Tray, View <=> Add, Kill, Find, Focus, Full, Float,
       Stick, Jump, List, Tag, Untag, Tags, Clients, Update, Data, Gravity, Screen, Raise, Lower */
    { NULL, SubtlerClientKill, SubtlerClientFind,  SubtlerClientFocus,
      SubtlerClientToggleFull, SubtlerClientToggleFloat, SubtlerClientToggleStick, NULL,
      SubtlerClientList, SubtlerClientTag, SubtlerClientUntag, SubtlerClientTags, NULL, NULL,
      NULL, SubtlerClientGravity, SubtlerClientScreen, SubtlerClientRestackRaise,
      SubtlerClientRestackLower },
    { SubtlerGravityNew, SubtlerGravityKill, SubtlerGravityFind, NULL, NULL, NULL, NULL, NULL,
      SubtlerGravityList, NULL, NULL, NULL, SubtlerGravityClients, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, SubtlerScreenFind, NULL, NULL, NULL, NULL, NULL, SubtlerScreenList, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { SubtlerSubletNew, SubtlerSubletKill, NULL, NULL, NULL, NULL, NULL, NULL,
      SubtlerSubletList, NULL, NULL, NULL, NULL, SubtlerSubletUpdate, SubtlerSubletData,
      NULL, NULL, NULL, NULL },
    { SubtlerTagNew, SubtlerTagKill, SubtlerTagFind, NULL, NULL, NULL, NULL, NULL, SubtlerTagList,
      NULL, NULL, NULL, SubtlerTagClients, NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, SubtlerTrayKill, SubtlerTrayFind, SubtlerTrayFocus, NULL, NULL, NULL, NULL,
      SubtlerTrayList, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
    { SubtlerViewNew, SubtlerViewKill, SubtlerViewFind, NULL, NULL, NULL, NULL,
      SubtlerViewJump, SubtlerViewList, SubtlerViewTag, SubtlerViewUntag, SubtlerViewTags,
      SubtlerViewClients, NULL, NULL, NULL, NULL, NULL, NULL }
  }; /* }}} */

  /* Set up signal handler */
  act.sa_handler = SubtlerSignal;
  act.sa_flags   = 0;
  memset(&act.sa_mask, 0, sizeof(sigset_t)); ///< Avoid uninitialized values
  sigaction(SIGSEGV, &act, NULL);

  /* Parse arguments {{{ */
  while((c = getopt_long(argc, argv, "cgestyvakfoFOSjlTUGIuAYnELrRqQCXd:hDV", long_options, NULL)) != -1)
    {
      switch(c)
        {
          /* Groups */
          case 'c': group = SUB_GROUP_CLIENT;    break;
          case 'g': group = SUB_GROUP_GRAVITY;   break;
          case 'e': group = SUB_GROUP_SCREEN;    break;
          case 's': group = SUB_GROUP_SUBLET;    break;
          case 't': group = SUB_GROUP_TAG;       break;
          case 'y': group = SUB_GROUP_TRAY;      break;
          case 'v': group = SUB_GROUP_VIEW;      break;

          /* Actions */
          case 'a': action = SUB_ACTION_ADD;      break;
          case 'k': action = SUB_ACTION_KILL;     break;
          case 'f': action = SUB_ACTION_FIND;     break;
          case 'o': action = SUB_ACTION_FOCUS;    break;
          case 'F': action = SUB_ACTION_FULL;     break;
          case 'O': action = SUB_ACTION_FLOAT;    break;
          case 'S': action = SUB_ACTION_STICK;    break;
          case 'j': action = SUB_ACTION_JUMP;     break;
          case 'l': action = SUB_ACTION_LIST;     break;
          case 'T': action = SUB_ACTION_TAG;      break;
          case 'U': action = SUB_ACTION_UNTAG;    break;
          case 'G': action = SUB_ACTION_TAGS;     break;
          case 'I': action = SUB_ACTION_CLIENTS;  break;
          case 'u': action = SUB_ACTION_UPDATE;   break;
          case 'A': action = SUB_ACTION_DATA;     break;
          case 'Y': action = SUB_ACTION_GRAVITY;  break;
          case 'n': action = SUB_ACTION_SCREEN;   break;
          case 'E': action = SUB_ACTION_RAISE;    break;
          case 'L': action = SUB_ACTION_LOWER;    break;

          /* Modifier */
          case 'r': mod = SUB_MOD_RELOAD_CONFIG;  break;
          case 'R': mod = SUB_MOD_RELOAD_SUBLETS; break;
          case 'q': mod = SUB_MOD_RESTART;        break;
          case 'Q': mod = SUB_MOD_QUIT;           break;
          case 'C': mod = SUB_MOD_CURRENT;        break;
          case 'X': mod = SUB_MOD_SELECT;         break;

          /* Other */
          case 'd': dispname = optarg;            break;
          case 'h': SubtlerUsage(group);          return 0;
#ifdef DEBUG
          case 'D': subSharedDebug();             break;
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
    } /* }}} */

  /* Open connection to server */
  if(!(display = XOpenDisplay(dispname)))
    {
      printf("Failed opening display `%s'.\n", (dispname) ? dispname : ":0.0");

      return -1;
    }
  XSetErrorHandler(subSharedLogXError);

  /* Set locale */
  if(!setlocale(LC_CTYPE, "")) XSupportsLocale();

  /* Check if subtle is running */
  if(True != subSharedSubtleRunning())
    {
      XCloseDisplay(display);
      display = NULL;

      subSharedLogError("Failed finding running %s\n", PKG_NAME);

      return -1;
    }

  /* Check mods */
  switch(mod)
    {
      case SUB_MOD_RELOAD_CONFIG:
        subSharedMessage(DefaultRootWindow(display),
          "SUBTLE_RELOAD_CONFIG", data, True);
        return 0;
      case SUB_MOD_RELOAD_SUBLETS:
        subSharedMessage(DefaultRootWindow(display),
          "SUBTLE_RELOAD_SUBLETS", data, True);
        return 0;
      case SUB_MOD_RESTART:
        subSharedMessage(DefaultRootWindow(display),
          "SUBTLE_RESTART", data, True);
        return 0;
      case SUB_MOD_QUIT:
        subSharedMessage(DefaultRootWindow(display),
          "SUBTLE_QUIT", data, True);
        return 0;
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
      if(arg1) args++;
      if(arg2) args++;

      cmds[group][action](args, arg1, arg2);
    }
  else if(SUB_MOD_CURRENT == mod && (SUB_GROUP_VIEW == group ||
      SUB_GROUP_CLIENT == group))
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
