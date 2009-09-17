
 /**
  * @package subtle
  *
  * @file Display functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <X11/cursorfont.h>
#include "subtle.h"

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

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

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  assert(subtle);

  /* Connect to display and setup error handler */
  if(!(subtle->dpy = XOpenDisplay(display)))
    subSharedLogError("Failed opening display `%s'\n", (display) ? display : ":0.0");
  XSetErrorHandler(subSharedLogXError);

  /* Create GCs */
  gvals.function      = GXcopy;
  gvals.fill_style    = FillStippled;
  gvals.stipple       = XCreateBitmapFromData(subtle->dpy, ROOT, stipple, 15, 16);
  mask                = GCFunction|GCFillStyle|GCStipple;
  subtle->gcs.stipple = XCreateGC(subtle->dpy, ROOT, mask, &gvals);
 
  gvals.plane_mask         = AllPlanes;
  gvals.graphics_exposures = False;
  mask                     = GCFunction|GCPlaneMask|GCGraphicsExposures;
  subtle->gcs.font         = XCreateGC(subtle->dpy, ROOT, mask, &gvals);

  gvals.function       = GXinvert;
  gvals.subwindow_mode = IncludeInferiors;
  gvals.line_width     = 3;
  mask                 = GCFunction|GCSubwindowMode|GCLineWidth;
  subtle->gcs.invert   = XCreateGC(subtle->dpy, ROOT, mask, &gvals);

  /* Create cursors */
  subtle->cursors.arrow  = XCreateFontCursor(subtle->dpy, XC_left_ptr);
  subtle->cursors.move   = XCreateFontCursor(subtle->dpy, XC_dotbox);
  subtle->cursors.resize = XCreateFontCursor(subtle->dpy, XC_sizing);

  /* Update root window */
  sattrs.cursor     = subtle->cursors.arrow;
  sattrs.event_mask = ROOTMASK;
  XChangeWindowAttributes(subtle->dpy, ROOT, CWCursor|CWEventMask, &sattrs);

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  /* Xinerama */
  if(XineramaQueryExtension(subtle->dpy, &xinerama_event, &xinerama_error) &&
    XineramaIsActive(subtle->dpy))
    {
      int i, n = 0;
      XineramaScreenInfo *screens = NULL;
      SubScreen *s = NULL; 

      /* Query screens */
      if((screens = XineramaQueryScreens(subtle->dpy, &n)))
        {
          for(i = 0; i < n; i++)
            {
              if((s = subScreenNew(screens[i].x_org, screens[i].y_org,
                screens[i].width, screens[i].height)))
                subArrayPush(subtle->screens, (void *)s);
            }

          subtle->screen = SCREEN(subtle->screens->data[0]); ///< Select first screen

          XFree(screens);
        }
    } 
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Create default screen */
  if(0 == subtle->screens->ndata)
    {
      if((subtle->screen = subScreenNew(0, 0, SCREENW, SCREENH)))
        subArrayPush(subtle->screens, (void *)subtle->screen);
    }

  /* Create windows */
  sattrs.event_mask        = ButtonPressMask|ExposureMask;
  sattrs.override_redirect = True;
  sattrs.background_pixel  = subtle->colors.bg_panel;
  mask                     = CWEventMask|CWOverrideRedirect|CWBackPixel;

  subtle->windows.panel1     = XCreateWindow(subtle->dpy, ROOT, 0, 0, subtle->screen->base.width, 
    1, 0, CopyFromParent, InputOutput, CopyFromParent, mask, &sattrs);
  subtle->windows.panel2     = XCreateWindow(subtle->dpy, ROOT, 0, 
    subtle->screen->base.height - subtle->th, subtle->screen->base.width, 1, 0, CopyFromParent, 
    InputOutput, CopyFromParent, mask, &sattrs);
  subtle->panels.views.win   = XCreateSimpleWindow(subtle->dpy, subtle->windows.panel1, 
    0, 0, 1, 1, 0, 0, sattrs.background_pixel);
  subtle->panels.caption.win = XCreateSimpleWindow(subtle->dpy, subtle->windows.panel1, 
    0, 0, 1, 1, 0, 0, sattrs.background_pixel);
  subtle->panels.tray.win    = XCreateSimpleWindow(subtle->dpy, subtle->windows.panel1, 
    0, 0, 1, 1, 0, 0, subtle->colors.bg_focus);    
  subtle->panels.sublets.win = XCreateSimpleWindow(subtle->dpy, subtle->windows.panel1, 
    0, 0, 1, 1, 0, 0, sattrs.background_pixel);

  /* Set override redirect */
  mask = CWOverrideRedirect;
  XChangeWindowAttributes(subtle->dpy, subtle->windows.panel1,     mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.panel2,     mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->panels.views.win,   mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->panels.caption.win, mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->panels.tray.win,    mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->panels.sublets.win, mask, &sattrs);

  /* Select input */
  XSelectInput(subtle->dpy, subtle->panels.views.win, ButtonPressMask); 
  XSelectInput(subtle->dpy, subtle->panels.tray.win, KeyPressMask|ButtonPressMask); 
  XSelectInput(subtle->dpy, subtle->panels.sublets.win, ButtonPressMask); 

  XSync(subtle->dpy, False);

  printf("Display (%s) is %dx%d on %d screens\n", 
    DisplayString(subtle->dpy), SCREENW, SCREENH, subtle->screens->ndata);
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
  gvals.font       = subtle->xfs->fid;
  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground|GCFont, &gvals);

  /* Update windows */
  XSetWindowBackground(subtle->dpy,  subtle->windows.panel1,     subtle->colors.bg_panel);
  XSetWindowBackground(subtle->dpy,  subtle->windows.panel2,     subtle->colors.bg_panel);
  XSetWindowBackground(subtle->dpy,  subtle->panels.caption.win, subtle->colors.bg_focus);
  XSetWindowBackground(subtle->dpy,  subtle->panels.views.win,   subtle->colors.bg_views);
  XSetWindowBackground(subtle->dpy,  subtle->panels.tray.win,    subtle->colors.bg_panel);
  XSetWindowBackground(subtle->dpy,  subtle->panels.sublets.win, subtle->colors.bg_sublets);
  XSetWindowBackground(subtle->dpy,  ROOT,                       subtle->colors.bg);

  XClearWindow(subtle->dpy, subtle->windows.panel1);
  XClearWindow(subtle->dpy, subtle->windows.panel2);
  XClearWindow(subtle->dpy, subtle->panels.caption.win);
  XClearWindow(subtle->dpy, subtle->panels.views.win);
  XClearWindow(subtle->dpy, subtle->panels.tray.win);
  XClearWindow(subtle->dpy, subtle->panels.sublets.win);
  XClearWindow(subtle->dpy, ROOT);

  /* Panels */
  if(subtle->flags & SUB_SUBTLE_PANEL1)
    {
      XMoveResizeWindow(subtle->dpy, subtle->windows.panel1, 0, 0, 
        subtle->screen->base.width, subtle->th);
      XMapRaised(subtle->dpy, subtle->windows.panel1);
    }

  if(subtle->flags & SUB_SUBTLE_PANEL2)
    {
      XMoveResizeWindow(subtle->dpy, subtle->windows.panel2, 0, 
        subtle->screen->base.height - subtle->th, subtle->screen->base.width, 
        subtle->th);
      XMapRaised(subtle->dpy, subtle->windows.panel2);
    }

  subScreenUpdate(); ///< Initially update strut
  subPanelUpdate();  ///< Initially update panels
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
  subScreenUpdate();
  subTrayUpdate();

  /* Activate first view */
  subViewJump(VIEW(subtle->views->data[0]));
  subtle->windows.focus = ROOT;
  subGrabSet(ROOT);
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
      if(subtle->xfs)         XFreeFont(subtle->dpy, subtle->xfs);

      /* Destroy view windows */
      XDestroySubwindows(subtle->dpy, subtle->windows.panel1);
      XDestroyWindow(subtle->dpy, subtle->windows.panel1);
      XDestroyWindow(subtle->dpy, subtle->windows.panel2);

      XInstallColormap(subtle->dpy, DefaultColormap(subtle->dpy, SCRN));
      XSetInputFocus(subtle->dpy, ROOT, RevertToPointerRoot, CurrentTime);
      XCloseDisplay(subtle->dpy);
    }

  subSharedLogDebug("kill=display\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
