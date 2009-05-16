
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

#include <sys/types.h>
#include <unistd.h>
#include <X11/cursorfont.h>
#include "subtle.h"

 /** subDisplayInit {{{
  * @brief Open connection to X server and create display
  * @param[in]  display  The display name as string
  **/

void
subDisplayInit(const char *display)
{
  XGCValues gvals;
  XSetWindowAttributes attrs;
  const char stipple[] = {
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12
  };

  assert(subtle);

  /* Connect to display and setup error handler */
  if(!(subtle->disp = XOpenDisplay(display)))
    subSharedLogError("Failed opening display `%s'\n", (display) ? display : ":0.0");
  XSetErrorHandler(subSharedLogXError);

  /* Create GCs */
  gvals.function      = GXcopy;
  gvals.fill_style    = FillStippled;
  gvals.stipple       = XCreateBitmapFromData(subtle->disp, ROOT, stipple, 15, 16);
  subtle->gcs.stipple = XCreateGC(subtle->disp, ROOT,
    GCFunction|GCFillStyle|GCStipple, &gvals);

  subtle->gcs.font = XCreateGC(subtle->disp, ROOT, GCFunction, &gvals);

  gvals.function       = GXinvert;
  gvals.subwindow_mode = IncludeInferiors;
  gvals.line_width     = 3;
  subtle->gcs.invert   = XCreateGC(subtle->disp, ROOT, 
    GCFunction|GCSubwindowMode|GCLineWidth, &gvals);

  /* Create cursors */
  subtle->cursors.arrow  = XCreateFontCursor(subtle->disp, XC_left_ptr);
  subtle->cursors.move   = XCreateFontCursor(subtle->disp, XC_dotbox);
  subtle->cursors.resize = XCreateFontCursor(subtle->disp, XC_sizing);

  /* Update root window */
  attrs.cursor     = subtle->cursors.arrow;
  attrs.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|
    FocusChangeMask|PropertyChangeMask;
  XChangeWindowAttributes(subtle->disp, ROOT, CWCursor|CWEventMask, &attrs);

  /* Create windows */
  attrs.save_under        = False;
  attrs.event_mask        = ButtonPressMask|ExposureMask|VisibilityChangeMask;
  attrs.override_redirect = True;

  subtle->windows.bar     = XCreateWindow(subtle->disp, ROOT, 0, 0, SCREENW, 
    1, 0, CopyFromParent, InputOutput, CopyFromParent, 
    CWSaveUnder|CWEventMask|CWOverrideRedirect, &attrs); 
  subtle->windows.caption = XCreateSimpleWindow(subtle->disp, subtle->windows.bar, 
    0, 0, 1, 1, 0, 0, attrs.background_pixel);
  subtle->windows.views   = XCreateSimpleWindow(subtle->disp, subtle->windows.bar, 
    0, 0, 1, 1, 0, 0, attrs.background_pixel);
  subtle->windows.tray    = XCreateSimpleWindow(subtle->disp, subtle->windows.bar, 
    0, 0, 1, 1, 0, 0, attrs.background_pixel);    
  subtle->windows.sublets = XCreateSimpleWindow(subtle->disp, subtle->windows.bar, 
    0, 0, 1, 1, 0, 0, attrs.background_pixel);

  /* Select input */
  XSelectInput(subtle->disp, subtle->windows.views, ButtonPressMask); 
  XSelectInput(subtle->disp, subtle->windows.tray, KeyPressMask|ButtonPressMask); 

  XSync(subtle->disp, False);

  printf("Display (%s) is %dx%d\n", DisplayString(subtle->disp), SCREENW, SCREENH);
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
  gvals.foreground = subtle->colors.border;
  gvals.line_width = subtle->bw;
  XChangeGC(subtle->disp, subtle->gcs.stipple, GCForeground|GCLineWidth, &gvals);

  gvals.foreground = subtle->colors.font;
  gvals.font       = subtle->xfs->fid;
  XChangeGC(subtle->disp, subtle->gcs.font, GCForeground|GCFont, &gvals);

  /* Update windows */
  XSetWindowBackground(subtle->disp,  subtle->windows.bar,     subtle->colors.norm);
  XSetWindowBackground(subtle->disp,  subtle->windows.caption, subtle->colors.focus);
  XSetWindowBackground(subtle->disp,  subtle->windows.views,   subtle->colors.norm);
  XSetWindowBackground(subtle->disp,  subtle->windows.tray,    subtle->colors.norm);
  XSetWindowBackground(subtle->disp,  subtle->windows.sublets, subtle->colors.norm);
  XSetWindowBackground(subtle->disp,  ROOT,                    subtle->colors.bg);

  XClearWindow(subtle->disp, subtle->windows.bar);
  XClearWindow(subtle->disp, subtle->windows.caption);
  XClearWindow(subtle->disp, subtle->windows.views);
  XClearWindow(subtle->disp, subtle->windows.tray);
  XClearWindow(subtle->disp, subtle->windows.sublets);
  XClearWindow(subtle->disp, ROOT);

  XMoveResizeWindow(subtle->disp, subtle->windows.bar, 0, 0, SCREENW, subtle->th);

  /* Map windows */
  XMapWindow(subtle->disp, subtle->windows.views);
  XMapWindow(subtle->disp, subtle->windows.tray);
  XMapWindow(subtle->disp, subtle->windows.sublets);  
  XMapRaised(subtle->disp, subtle->windows.bar);
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

  XQueryTree(subtle->disp, ROOT, &dummy, &dummy, &wins, &n);
  for(i = 0; i < n; i++)
    {
      SubClient *c = NULL;
      XWindowAttributes attr;

      XGetWindowAttributes(subtle->disp, wins[i], &attr);
      if(False == attr.override_redirect) ///< Skip own windows 
        {
          switch(attr.map_state)
            {
              case IsViewable: ///< Client
                if((c = subClientNew(wins[i])))
                  subArrayPush(subtle->clients, (void *)c);
                break;
            }
        }
    }
  XFree(wins);

  subClientPublish();
  subTrayUpdate();

  /* Activate first view */
  subViewJump(VIEW(subtle->views->data[0]));
  subtle->windows.focus = ROOT;
  subGrabSet(ROOT);
} /* }}} */

 /** subDisplayPublish {{{
  * @brief Publish display
  **/

void
subDisplayPublish(void)
{
  long data[4] = { 0, 0, 0, 0 }, pid = (long)getpid();

  assert(subtle);

  /* EWMH: Window manager information */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_SUPPORTING_WM_CHECK, &subtle->windows.bar, 1);
  subEwmhSetString(subtle->windows.bar, SUB_EWMH_NET_WM_NAME, PKG_NAME);
  subEwmhSetString(subtle->windows.bar, SUB_EWMH_WM_CLASS, PKG_NAME);
  subEwmhSetCardinals(subtle->windows.bar, SUB_EWMH_NET_WM_PID, &pid, 1);
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_DESKTOP_VIEWPORT, (long *)&data, 2);
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_SHOWING_DESKTOP, (long *)&data, 1);

  /* EWMH: Workarea size */
  data[2] = DisplayWidth(subtle->disp, DefaultScreen(subtle->disp)); 
  data[3] = DisplayHeight(subtle->disp, DefaultScreen(subtle->disp));
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_WORKAREA, (long *)&data, 4);

  /* EWMH: Desktop sizes */
  data[0] = DisplayWidth(subtle->disp, DefaultScreen(subtle->disp));
  data[1] = DisplayHeight(subtle->disp, DefaultScreen(subtle->disp));
  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&data, 2);

  /* EWMH: Supported window states */
  data[0] = subEwmhGet(SUB_EWMH_NET_WM_STATE_HIDDEN);
  data[1] = subEwmhGet(SUB_EWMH_NET_WM_STATE_FULLSCREEN);
  data[2] = subEwmhGet(SUB_EWMH_NET_WM_STATE_ABOVE);
  data[3] = subEwmhGet(SUB_EWMH_NET_WM_STATE_STICKY);

  subEwmhSetCardinals(ROOT, SUB_EWMH_NET_SUPPORTED, (long *)&data, LENGTH(data));

  /* EWMH: Client list and client list stacking */
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST, NULL, 0);
  subEwmhSetWindows(ROOT, SUB_EWMH_NET_CLIENT_LIST_STACKING, NULL, 0);

  /* EWMH: Tray */
  subTraySelect();
}  /* }}} */

 /** subDisplayFinish {{{
  * @brief Close connection
  **/

void
subDisplayFinish(void)
{
  assert(subtle);

  if(subtle->disp)
    {
      /* Free cursors */
      if(subtle->cursors.arrow)  XFreeCursor(subtle->disp, subtle->cursors.arrow);
      if(subtle->cursors.move)   XFreeCursor(subtle->disp, subtle->cursors.move);
      if(subtle->cursors.resize) XFreeCursor(subtle->disp, subtle->cursors.resize);

      /* Free GCs */
      if(subtle->gcs.stipple) XFreeGC(subtle->disp, subtle->gcs.stipple);
      if(subtle->gcs.font)    XFreeGC(subtle->disp, subtle->gcs.font);
      if(subtle->gcs.invert)  XFreeGC(subtle->disp, subtle->gcs.invert);
      if(subtle->xfs)         XFreeFont(subtle->disp, subtle->xfs);

      /* Destroy view windows */
      XDestroySubwindows(subtle->disp, subtle->windows.bar);
      XDestroyWindow(subtle->disp, subtle->windows.bar);

      XInstallColormap(subtle->disp, DefaultColormap(subtle->disp, SCREEN));
      XSetInputFocus(subtle->disp, PointerRoot, RevertToPointerRoot, CurrentTime);

      XCloseDisplay(subtle->disp);
    }

  subSharedLogDebug("kill=display\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
