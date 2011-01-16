
 /**
  * @package subtle
  *
  * @file Display functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <X11/cursorfont.h>
#include <unistd.h>
#include <locale.h>
#include "subtle.h"

/* DisplayClaim {{{ */
int
DisplayClaim(void)
{
  int success = True;
  char buf[10] = { 0 };
  Atom session = None;
  Window owner = None;

  /* Get session atom */
  snprintf(buf, sizeof(buf), "WM_S%d", DefaultScreen(subtle->dpy));
  session = XInternAtom(subtle->dpy, buf, False);

  if((owner = XGetSelectionOwner(subtle->dpy, session)))
    {
      if(!(subtle->flags & SUB_SUBTLE_REPLACE))
        {
          subSharedLogError("Found a running window manager\n");

          return False;
        }

      XSelectInput(subtle->dpy, owner, StructureNotifyMask);
      XSync(subtle->dpy, False);
    }

  /* Aquire session selection */
  XSetSelectionOwner(subtle->dpy, session,
    subtle->windows.support, CurrentTime);

  /* Wait for previous window manager to exit */
  if(XGetSelectionOwner(subtle->dpy, session) == subtle->windows.support)
    {
      if(owner)
        {
          int i;
          XEvent event;

          printf("Waiting for current window manager to exit\n");

          for(i = 0; i < WAITTIME; i++)
            {
              if(XCheckWindowEvent(subtle->dpy, owner,
                  StructureNotifyMask, &event) &&
                  DestroyNotify == event.type)
                return True;

              sleep(1);
            }

          subSharedLogError("Giving up waiting for window managert\n");
          success = False;
        }
    }
  else
    {
      subSharedLogWarn("Failed replacing current window manager\n");
      success = False;
    }

  return success;
} /* }}} */

/* Public */

 /** subDisplayInit {{{
  * @brief Open connection to X server and create display
  * @param[in]  display  The display name as string
  **/

void
subDisplayInit(const char *display)
{
  XGCValues gvals;
  XSetWindowAttributes sattrs;
  unsigned long mask = 0;
  const char stipple[] = {
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12
  };

#if defined HAVE_X11_EXTENSIONS_XINERAMA_H || defined HAVE_X11_EXTENSIONS_XRANDR_H
  int event = 0, junk = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H HAVE_X11_EXTENSIONS_XRANDR_H */

  assert(subtle);

  /* Set locale */
  if(!setlocale(LC_CTYPE, "")) XSupportsLocale();

  /* Connect to display and setup error handler */
  if(!(subtle->dpy = XOpenDisplay(display)))
    {
      subSharedLogError("Failed opening display `%s'\n",
        (display) ? display : ":0.0");

      subSubtleFinish();

      exit(-1);
    }

  /* Create supporting window */
  subtle->windows.support = XCreateSimpleWindow(subtle->dpy, ROOT,
    -100, -100, 1, 1, 0, 0, 0);

  sattrs.override_redirect = True;
  sattrs.event_mask        = PropertyChangeMask;
  XChangeWindowAttributes(subtle->dpy, subtle->windows.support,
    CWEventMask|CWOverrideRedirect, &sattrs);

  /* Claim and setup display */
  if(!DisplayClaim())
    {
      subSubtleFinish();

      exit(-1);
    }

  XSetErrorHandler(subSharedLogXError);
  setenv("DISPLAY", DisplayString(subtle->dpy), True); ///< Set display for clients

  /* Create GCs */
  gvals.function           = GXcopy;
  gvals.fill_style         = FillStippled;
  gvals.stipple            = XCreateBitmapFromData(subtle->dpy, ROOT, stipple, 15, 16);
  mask                     = GCFunction|GCFillStyle|GCStipple;
  subtle->gcs.stipple      = XCreateGC(subtle->dpy, ROOT, mask, &gvals);

  gvals.plane_mask         = AllPlanes;
  gvals.graphics_exposures = False;
  mask                     = GCFunction|GCPlaneMask|GCGraphicsExposures;
  subtle->gcs.font         = XCreateGC(subtle->dpy, ROOT, mask, &gvals);

  gvals.function           = GXinvert;
  gvals.subwindow_mode     = IncludeInferiors;
  gvals.line_width         = 3;
  mask                     = GCFunction|GCSubwindowMode|GCLineWidth;
  subtle->gcs.invert       = XCreateGC(subtle->dpy, ROOT, mask, &gvals);

  /* Create cursors */
  subtle->cursors.arrow  = XCreateFontCursor(subtle->dpy, XC_left_ptr);
  subtle->cursors.move   = XCreateFontCursor(subtle->dpy, XC_dotbox);
  subtle->cursors.resize = XCreateFontCursor(subtle->dpy, XC_sizing);

  /* Update root window */
  sattrs.cursor     = subtle->cursors.arrow;
  sattrs.event_mask = ROOTMASK;
  XChangeWindowAttributes(subtle->dpy, ROOT, CWCursor|CWEventMask, &sattrs);

  /* Create tray window */
  subtle->windows.tray.win = XCreateSimpleWindow(subtle->dpy,
    ROOT, 0, 0, 1, 1, 0, 0, subtle->colors.bg_focus);

  sattrs.override_redirect = True;
  sattrs.event_mask        = KeyPressMask|ButtonPressMask;
  XChangeWindowAttributes(subtle->dpy, subtle->windows.tray.win,
    CWOverrideRedirect|CWEventMask, &sattrs);

  /* Init screen width and height */
  subtle->width  = DisplayWidth(subtle->dpy, DefaultScreen(subtle->dpy));
  subtle->height = DisplayHeight(subtle->dpy, DefaultScreen(subtle->dpy));

  /* Check extensions */
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  if(!XineramaQueryExtension(subtle->dpy, &event, &junk))
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */
    subtle->flags &= ~SUB_SUBTLE_XINERAMA;

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
  if(!XRRQueryExtension(subtle->dpy, &event, &junk))
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */
    subtle->flags &= ~SUB_SUBTLE_XRANDR;

  XSync(subtle->dpy, False);

  printf("Display (%s) is %dx%d\n", DisplayString(subtle->dpy),
    subtle->width, subtle->height);

  subSharedLogDebugSubtle("init=display\n");
} /* }}} */

 /** subDisplayConfigure {{{
  * @brief Configure display
  **/

void
subDisplayConfigure(void)
{
  XGCValues gvals;

  assert(subtle);

  /* Update GCs */
  gvals.foreground = subtle->colors.stipple;
  gvals.line_width = subtle->bw;
  XChangeGC(subtle->dpy, subtle->gcs.stipple,
    GCForeground|GCLineWidth, &gvals);

  gvals.foreground = subtle->colors.separator;
  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);

  /* Update windows */
  XSetWindowBackground(subtle->dpy, subtle->windows.tray.win,
    subtle->colors.panel);

  /* Set background if set */
  if(-1 != subtle->colors.bg)
    XSetWindowBackground(subtle->dpy, ROOT, subtle->colors.bg);

  XClearWindow(subtle->dpy, subtle->windows.tray.win);
  XClearWindow(subtle->dpy, ROOT);

  /* Update struts and panels */
  subScreenResize();
  subScreenUpdate();

  XSync(subtle->dpy, False); ///< Sync all changes
} /* }}} */

 /** subDisplayScan {{{
  * @brief Scan root window for clients
  **/

void
subDisplayScan(void)
{
  unsigned int i, n = 0;
  Window dummy, *wins = NULL;

  assert(subtle);

  /* Scan for client windows */
  XQueryTree(subtle->dpy, ROOT, &dummy, &dummy, &wins, &n);
  for(i = 0; i < n; i++)
    {
      SubClient *c = NULL;
      XWindowAttributes attrs;

      XGetWindowAttributes(subtle->dpy, wins[i], &attrs);
      switch(attrs.map_state)
        {
          case IsViewable:
            if((c = subClientNew(wins[i])))
              subArrayPush(subtle->clients, (void *)c);
            break;
        }
    }
  XFree(wins);

  subClientPublish();
} /* }}} */

 /** subDisplayPublish {{{
  * @brief Update EWMH infos
  **/

void
subDisplayPublish(void)
{
  unsigned long *colors;

#define NCOLORS 24

  /* Create color array */
  colors = (unsigned long *)subSharedMemoryAlloc(NCOLORS,
    sizeof(unsigned long));

  colors[0]  = subtle->colors.fg_title;
  colors[1]  = subtle->colors.bg_title;
  colors[2]  = subtle->colors.bo_title;
  colors[3]  = subtle->colors.fg_focus;
  colors[4]  = subtle->colors.bg_focus;
  colors[5]  = subtle->colors.bo_focus;
  colors[6]  = subtle->colors.fg_urgent;
  colors[7]  = subtle->colors.bg_urgent;
  colors[8]  = subtle->colors.bo_urgent;
  colors[9]  = subtle->colors.fg_occupied;
  colors[10] = subtle->colors.bg_occupied;
  colors[11] = subtle->colors.bo_occupied;
  colors[12] = subtle->colors.fg_views;
  colors[13] = subtle->colors.bg_views;
  colors[14] = subtle->colors.bo_views;
  colors[15] = subtle->colors.fg_sublets;
  colors[16] = subtle->colors.bg_sublets;
  colors[17] = subtle->colors.bo_sublets;
  colors[18] = subtle->colors.bo_active;
  colors[19] = subtle->colors.bo_inactive;
  colors[20] = subtle->colors.panel;
  colors[21] = subtle->colors.bg;
  colors[22] = subtle->colors.stipple;
  colors[23] = subtle->colors.separator;

  /* EWMH: Colors */
  subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_COLORS, (long *)colors, NCOLORS);

  free(colors);

  XSync(subtle->dpy, False); ///< Sync all changes

  subSharedLogDebugSubtle("publish=colors, n=%d\n", NCOLORS);
} /* }}} */

 /** subDisplayFinish {{{
  * @brief Close connection
  **/

void
subDisplayFinish(void)
{
  assert(subtle);

  if(subtle->dpy)
    {
      XSync(subtle->dpy, False); ///< Sync all changes

      /* Free cursors */
      if(subtle->cursors.arrow)  XFreeCursor(subtle->dpy, subtle->cursors.arrow);
      if(subtle->cursors.move)   XFreeCursor(subtle->dpy, subtle->cursors.move);
      if(subtle->cursors.resize) XFreeCursor(subtle->dpy, subtle->cursors.resize);

      /* Free GCs */
      if(subtle->gcs.stipple) XFreeGC(subtle->dpy, subtle->gcs.stipple);
      if(subtle->gcs.font)    XFreeGC(subtle->dpy, subtle->gcs.font);
      if(subtle->gcs.invert)  XFreeGC(subtle->dpy, subtle->gcs.invert);

      /* Free font */
      if(subtle->font) subSharedFontKill(subtle->dpy, subtle->font);

      XDestroyWindow(subtle->dpy, subtle->windows.tray.win);
      XDestroyWindow(subtle->dpy, subtle->windows.support);

      XInstallColormap(subtle->dpy, DefaultColormap(subtle->dpy, SCRN));
      XSetInputFocus(subtle->dpy, ROOT, RevertToPointerRoot, CurrentTime);
      XCloseDisplay(subtle->dpy);
    }

  subSharedLogDebugSubtle("finish=display\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
