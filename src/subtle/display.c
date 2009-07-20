
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
  SubScreen *s = NULL;
  unsigned long mask = 0;
  const char stipple[] = {
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12
  };

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
  XineramaScreenInfo *screens = NULL;
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

  subtle->gcs.font = XCreateGC(subtle->dpy, ROOT, GCFunction, &gvals);

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

  /* Create windows */
  sattrs.event_mask        = ButtonPressMask|ExposureMask|VisibilityChangeMask;
  sattrs.override_redirect = True;
  sattrs.background_pixel  = subtle->colors.bg_bar;
  mask                     = CWOverrideRedirect;

  subtle->windows.bar     = XCreateWindow(subtle->dpy, ROOT, 0, 0, SCREENW, 1, 0, CopyFromParent, 
    InputOutput, CopyFromParent, CWEventMask|CWOverrideRedirect|CWBackPixel, &sattrs); 
  subtle->windows.views   = XCreateSimpleWindow(subtle->dpy, subtle->windows.bar, 
    0, 0, 1, 1, 0, 0, sattrs.background_pixel);
  subtle->windows.caption = XCreateSimpleWindow(subtle->dpy, subtle->windows.bar, 
    0, 0, 1, 1, 0, 0, sattrs.background_pixel);
  subtle->windows.tray    = XCreateSimpleWindow(subtle->dpy, subtle->windows.bar, 
    0, 0, 1, 1, 0, 0, sattrs.background_pixel);    
  subtle->windows.sublets = XCreateSimpleWindow(subtle->dpy, subtle->windows.bar, 
    0, 0, 1, 1, 0, 0, sattrs.background_pixel);

  /* Set override redirect */
  XChangeWindowAttributes(subtle->dpy, subtle->windows.bar, mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.views, mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.caption, mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.tray, mask, &sattrs);
  XChangeWindowAttributes(subtle->dpy, subtle->windows.sublets, mask, &sattrs);

  /* Select input */
  XSelectInput(subtle->dpy, subtle->windows.views, ButtonPressMask); 
  XSelectInput(subtle->dpy, subtle->windows.tray, KeyPressMask|ButtonPressMask); 

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  /* Xinerama */
  if(XineramaQueryExtension(subtle->dpy, &xinerama_event, &xinerama_error) &&
    XineramaIsActive(subtle->dpy))
    {
      int i, n;

      if((screens = XineramaQueryScreens(subtle->dpy, &n)))
        {
          for(i = 0; i < n; i++)
            {
              if((s = subScreenNew(screens[i].x_org, screens[i].y_org,
                screens[i].width, screens[i].height)))
                subArrayPush(subtle->screens, (void *)s);
            }

          XFree(screens);
        }
    } 
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Create default screen */
  if(0 == subtle->screens->ndata)
    {
      if((s = subScreenNew(0, 0, SCREENW, SCREENH)))
        subArrayPush(subtle->screens, (void *)s);
    }

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
  SubScreen *s = NULL;
  XGCValues gvals;

  assert(subtle);

  s = SCREEN(subtle->screens->data[0]);

  /* Update GCs */
  gvals.foreground = subtle->colors.fg_bar;
  gvals.line_width = subtle->bw;
  XChangeGC(subtle->dpy, subtle->gcs.stipple, GCForeground|GCLineWidth, &gvals);

  gvals.foreground = subtle->colors.fg_bar;
  gvals.font       = subtle->xfs->fid;
  XChangeGC(subtle->dpy, subtle->gcs.font, GCForeground|GCFont, &gvals);

  /* Update windows */
  XSetWindowBackground(subtle->dpy,  subtle->windows.bar,     subtle->colors.bg_bar);
  XSetWindowBackground(subtle->dpy,  subtle->windows.caption, subtle->colors.bg_focus);
  XSetWindowBackground(subtle->dpy,  subtle->windows.views,   subtle->colors.bg_views);
  XSetWindowBackground(subtle->dpy,  subtle->windows.tray,    subtle->colors.bg_bar);
  XSetWindowBackground(subtle->dpy,  subtle->windows.sublets, subtle->colors.bg_bar);
  XSetWindowBackground(subtle->dpy,  ROOT,                    subtle->colors.bg);

  XClearWindow(subtle->dpy, subtle->windows.bar);
  XClearWindow(subtle->dpy, subtle->windows.caption);
  XClearWindow(subtle->dpy, subtle->windows.views);
  XClearWindow(subtle->dpy, subtle->windows.tray);
  XClearWindow(subtle->dpy, subtle->windows.sublets);
  XClearWindow(subtle->dpy, ROOT);

  /* Bar and panel */
  if(subtle->panel)
    XReparentWindow(subtle->dpy, subtle->windows.sublets, ROOT, 0, 0);

  XMoveResizeWindow(subtle->dpy, subtle->windows.bar, 0, 
    subtle->swap ? s->base.height - subtle->th : 0, s->base.width, subtle->th);

  /* Map windows */
  XMapWindow(subtle->dpy, subtle->windows.views);
  XMapRaised(subtle->dpy, subtle->windows.bar);

  subScreenUpdate(); ///< Update strut
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
  subSubletUpdate();

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
      XDestroySubwindows(subtle->dpy, subtle->windows.bar);
      XDestroyWindow(subtle->dpy, subtle->windows.bar);

      XInstallColormap(subtle->dpy, DefaultColormap(subtle->dpy, SCRN));
      XSetInputFocus(subtle->dpy, PointerRoot, RevertToPointerRoot, CurrentTime);

      XCloseDisplay(subtle->dpy);
    }

  subSharedLogDebug("kill=display\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
