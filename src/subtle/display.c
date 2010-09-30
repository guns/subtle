
 /**
  * @package subtle
  *
  * @file Display functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
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
  XrmInitialize();

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

  /* Create windows */
  sattrs.event_mask        = ButtonPressMask|ExposureMask;
  sattrs.override_redirect = True;
  sattrs.background_pixel  = subtle->colors.bg_panel;
  mask                     = CWEventMask|CWOverrideRedirect|CWBackPixel;

  subtle->windows.panel1     = XCreateWindow(subtle->dpy, ROOT, 0, 1, 1, 1, 0,
    CopyFromParent, InputOutput, CopyFromParent, mask, &sattrs);
  subtle->windows.panel2     = XCreateWindow(subtle->dpy, ROOT, 0, 0, 1, 1, 0,
    CopyFromParent, InputOutput, CopyFromParent, mask, &sattrs);
  subtle->windows.views.win   = XCreateSimpleWindow(subtle->dpy,
    subtle->windows.panel1, 0, 0, 1, 1, 0, 0, sattrs.background_pixel);
  subtle->windows.title.win   = XCreateSimpleWindow(subtle->dpy,
    subtle->windows.panel1, 0, 0, 1, 1, 0, 0, sattrs.background_pixel);
  subtle->windows.tray.win    = XCreateSimpleWindow(subtle->dpy,
    subtle->windows.panel1, 0, 0, 1, 1, 0, 0, subtle->colors.bg_focus);

  /* Set override redirect */
  mask = CWOverrideRedirect;
  XChangeWindowAttributes(subtle->dpy, subtle->windows.panel1, mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.panel2, mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.views.win,
    mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.title.win,
    mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.tray.win,
    mask, &sattrs);

  /* Select input */
  XSelectInput(subtle->dpy, subtle->windows.views.win, ButtonPressMask);
  XSelectInput(subtle->dpy, subtle->windows.tray.win,
    KeyPressMask|ButtonPressMask);

  /* Check extensions */
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  if(XineramaQueryExtension(subtle->dpy, &event, &junk))
    subtle->flags |= SUB_SUBTLE_XINERAMA;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
  if(XRRQueryExtension(subtle->dpy, &event, &junk))
    subtle->flags |= SUB_SUBTLE_XRANDR;
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */

  XSync(subtle->dpy, False);

  printf("Display (%s) is %dx%d\n", DisplayString(subtle->dpy),
    SCREENW, SCREENH);
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
  gvals.foreground = subtle->colors.fg_panel;
  gvals.line_width = subtle->bw;
  XChangeGC(subtle->dpy, subtle->gcs.stipple, GCForeground|GCLineWidth, &gvals);

  gvals.foreground = subtle->colors.fg_panel;
  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground, &gvals);

  /* Update windows */
  XSetWindowBackground(subtle->dpy,  subtle->windows.panel1,
    subtle->colors.bg_panel);
  XSetWindowBackground(subtle->dpy,  subtle->windows.panel2,
    subtle->colors.bg_panel);
  XSetWindowBackground(subtle->dpy,  subtle->windows.title.win,
    subtle->colors.bg_focus);
  XSetWindowBackground(subtle->dpy,  subtle->windows.views.win,
    subtle->colors.bg_views);
  XSetWindowBackground(subtle->dpy,  subtle->windows.tray.win,
    subtle->colors.bg_panel);

  if(subtle->flags & SUB_SUBTLE_BACKGROUND) ///< Set background if desired
    XSetWindowBackground(subtle->dpy, ROOT, subtle->colors.bg);

  XClearWindow(subtle->dpy, subtle->windows.panel1);
  XClearWindow(subtle->dpy, subtle->windows.panel2);
  XClearWindow(subtle->dpy, subtle->windows.title.win);
  XClearWindow(subtle->dpy, subtle->windows.views.win);
  XClearWindow(subtle->dpy, subtle->windows.tray.win);
  XClearWindow(subtle->dpy, ROOT);

  /* Panels */
  if(subtle->flags & SUB_SUBTLE_PANEL1)
    {
      XMoveResizeWindow(subtle->dpy, subtle->windows.panel1, 0, 0,
        DEFSCREEN->base.width, subtle->th);
      XMapRaised(subtle->dpy, subtle->windows.panel1);
    }
  else XUnmapWindow(subtle->dpy, subtle->windows.panel1);

  if(subtle->flags & SUB_SUBTLE_PANEL2)
    {
      XMoveResizeWindow(subtle->dpy, subtle->windows.panel2, 0,
        DEFSCREEN->base.height - subtle->th, DEFSCREEN->base.width,
        subtle->th);
      XMapRaised(subtle->dpy, subtle->windows.panel2);
    }
  else XUnmapWindow(subtle->dpy, subtle->windows.panel2);

  /* Update struts and panels */
  subScreenConfigure();
  subPanelUpdate();
  subViewUpdate();
  subSubletUpdate();

  XSync(subtle->dpy, False); ///< Sync all changes
} /* }}} */

 /** subDisplayScan {{{
  * @brief Scan root window for clients
  **/

void
subDisplayScan(void)
{
  unsigned int i, n = 0, flags = 0;
  Window dummy, *wins = NULL;

  assert(subtle);

  /* Scan for client windows */
  XQueryTree(subtle->dpy, ROOT, &dummy, &dummy, &wins, &n);
  for(i = 0; i < n; i++)
    {
      SubClient *c = NULL;
      SubTray *t = NULL;
      XWindowAttributes attr;

      XGetWindowAttributes(subtle->dpy, wins[i], &attr);
      if(False == attr.override_redirect) ///< Skip some windows
        {
          switch(attr.map_state)
            {
              case IsViewable:
                if((flags = subEwmhGetXEmbedState(wins[i])) && ///< Tray
                    (t = subTrayNew(wins[i])))
                  {
                    subArrayPush(subtle->trays, (void *)t);
                  }
                else if((c = subClientNew(wins[i]))) ///< Clients
                  {
                    subArrayPush(subtle->clients, (void *)c);
                  }
                break;
            }
        }
    }
  XFree(wins);

  subClientPublish();
  subScreenConfigure();
  subTrayUpdate();

  /* Hook: Start */
  subHookCall(SUB_HOOK_START, NULL);

  /* Activate first view */
  subViewJump(VIEW(subtle->views->data[0]), False);
  subtle->windows.focus = ROOT;
  subGrabSet(ROOT, !(subtle->flags & SUB_SUBTLE_ESCAPE));
} /* }}} */

 /** subDisplayPublish {{{
  * @brief Update EWMH infos
  **/

void
subDisplayPublish(void)
{
  unsigned long *colors;

#define NCOLORS 14

  /* Create color array */
  colors = (unsigned long *)subSharedMemoryAlloc(NCOLORS, sizeof(unsigned long));

  colors[0]  = subtle->colors.fg_panel;
  colors[1]  = subtle->colors.fg_views;
  colors[2]  = subtle->colors.fg_sublets;
  colors[3]  = subtle->colors.fg_focus;
  colors[4]  = subtle->colors.fg_urgent;
  colors[5]  = subtle->colors.bg_panel;
  colors[6]  = subtle->colors.bg_views;
  colors[7]  = subtle->colors.bg_sublets;
  colors[8]  = subtle->colors.bg_focus;
  colors[9]  = subtle->colors.bg_urgent;
  colors[10] = subtle->colors.bo_focus;
  colors[11] = subtle->colors.bo_normal;
  colors[12] = subtle->colors.bo_panel;
  colors[13] = subtle->colors.bg;

  /* EWMH: Colors */
  subEwmhSetCardinals(ROOT, SUB_EWMH_SUBTLE_COLORS, (long *)colors, NCOLORS);

  subSharedLogDebug("publish=colors, n=%d\n", NCOLORS);

  XSync(subtle->dpy, False); ///< Sync all changes

  free(colors);
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

      /* Destroy windows */
      if(subtle->windows.panel1)
        {
          XDestroySubwindows(subtle->dpy, subtle->windows.panel1);
          XDestroyWindow(subtle->dpy, subtle->windows.panel1);
        }

      if(subtle->windows.panel2)
        {
          XDestroySubwindows(subtle->dpy, subtle->windows.panel2);
          XDestroyWindow(subtle->dpy, subtle->windows.panel2);
        }

      if(subtle->windows.panel1)
        XDestroyWindow(subtle->dpy, subtle->windows.support);

      XInstallColormap(subtle->dpy, DefaultColormap(subtle->dpy, SCRN));
      XSetInputFocus(subtle->dpy, ROOT, RevertToPointerRoot, CurrentTime);
      XCloseDisplay(subtle->dpy);
    }

  subSharedLogDebug("kill=display\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
