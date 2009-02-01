
 /**
  * @package subtle
  *
  * @file Display functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
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
  const char stipple[] = {
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
    0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12
  };

  /* Connect to display and setup error handler */
  subtle->disp = XOpenDisplay(display);
  if(!subtle->disp) subSharedLogError("Can't open display `%s'.\n", (display) ? display : ":0.0");
  XSetErrorHandler(subSharedLogXError);

  /* Create gcs */
  gvals.function      = GXcopy;
  gvals.fill_style    = FillStippled;
  gvals.stipple       = XCreateBitmapFromData(subtle->disp, DefaultRootWindow(subtle->disp),
    stipple, 15, 16);
  subtle->gcs.stipple  = XCreateGC(subtle->disp, DefaultRootWindow(subtle->disp),
    GCFunction|GCFillStyle|GCStipple, &gvals);

  subtle->gcs.font = XCreateGC(subtle->disp, DefaultRootWindow(subtle->disp),
    GCFunction, &gvals);

  gvals.function       = GXinvert;
  gvals.subwindow_mode = IncludeInferiors;
  gvals.line_width     = 3;
  subtle->gcs.invert   = XCreateGC(subtle->disp, DefaultRootWindow(subtle->disp),
    GCFunction|GCSubwindowMode|GCLineWidth, &gvals);

  /* Create cursors */
  subtle->cursors.arrow  = XCreateFontCursor(subtle->disp, XC_left_ptr);
  subtle->cursors.move   = XCreateFontCursor(subtle->disp, XC_dotbox);
  subtle->cursors.resize = XCreateFontCursor(subtle->disp, XC_sizing);

  printf("Display (%s) is %dx%d\n", DisplayString(subtle->disp), DisplayWidth(subtle->disp,
    DefaultScreen(subtle->disp)), DisplayHeight(subtle->disp, DefaultScreen(subtle->disp)));

  XSync(subtle->disp, False);
} /* }}} */

 /** subDisplayScan {{{
  * @brief Scan root window for clients
  **/

void
subDisplayScan(void)
{
  unsigned int i, n = 0;
  Window dummy, *wins = NULL;
  XWindowAttributes attr;

  XQueryTree(subtle->disp, ROOT, &dummy, &dummy, &wins, &n);
  for(i = 0; i < n; i++)
    {
      SubClient *c = NULL;
      SubTray *t = NULL;

      XGetWindowAttributes(subtle->disp, wins[i], &attr);
      if(False == attr.override_redirect) ///< Skip own windows 
        {
          switch(attr.map_state)
            {
              case IsViewable: ///< Client
                if((c = subClientNew(wins[i])))
                  subArrayPush(subtle->clients, (void *)c);
                break;
              case IsUnmapped: ///< Tray
                if((t = subTrayNew(wins[i])))
                  subArrayPush(subtle->trays, (void *)t);
                break;
            }
        }
    }
  XFree(wins);

  subClientPublish();
  subTrayUpdate();
  subViewConfigure(subtle->cv);
  subViewRender();
} /* }}} */

 /** subDisplayPublish {{{
  * @brief Publish display
  **/

void
subDisplayPublish(void)
{
  Window root = DefaultRootWindow(subtle->disp);
  long data[4] = { 0, 0, 0, 0 }, pid = (long)getpid();

  /* EWMH: Window manager information */
  subEwmhSetWindows(root, SUB_EWMH_NET_SUPPORTING_WM_CHECK, &subtle->windows.bar, 1);
  subEwmhSetString(subtle->windows.bar, SUB_EWMH_NET_WM_NAME, PKG_NAME);
  subEwmhSetCardinals(subtle->windows.bar, SUB_EWMH_NET_WM_PID, &pid, 1);
  subEwmhSetCardinals(root, SUB_EWMH_NET_DESKTOP_VIEWPORT, (long *)&data, 2);
  subEwmhSetCardinals(root, SUB_EWMH_NET_SHOWING_DESKTOP, (long *)&data, 1);

  /* EWMH: Workarea size */
  data[2] = DisplayWidth(subtle->disp, DefaultScreen(subtle->disp)); 
  data[3] = DisplayHeight(subtle->disp, DefaultScreen(subtle->disp));
  subEwmhSetCardinals(root, SUB_EWMH_NET_WORKAREA, (long *)&data, 4);

  /* EWMH: Desktop sizes */
  data[0] = DisplayWidth(subtle->disp, DefaultScreen(subtle->disp));
  data[1] = DisplayHeight(subtle->disp, DefaultScreen(subtle->disp));
  subEwmhSetCardinals(root, SUB_EWMH_NET_DESKTOP_GEOMETRY, (long *)&data, 2);

  /* EWMH: Supported window states */
  data[0] = subEwmhGet(SUB_EWMH_NET_WM_STATE_HIDDEN);
  data[1] = subEwmhGet(SUB_EWMH_NET_WM_STATE_FULLSCREEN);
  data[2] = subEwmhGet(SUB_EWMH_NET_WM_STATE_ABOVE);
  data[3] = subEwmhGet(SUB_EWMH_NET_WM_STATE_STICKY);

  subEwmhSetCardinals(root, SUB_EWMH_NET_SUPPORTED, (long *)&data, LENGTH(data));

  /* EWMH: Client list and client list stacking */
  subEwmhSetWindows(root, SUB_EWMH_NET_CLIENT_LIST, NULL, 0);
  subEwmhSetWindows(root, SUB_EWMH_NET_CLIENT_LIST_STACKING, NULL, 0);
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
      XFreeCursor(subtle->disp, subtle->cursors.arrow);
      XFreeCursor(subtle->disp, subtle->cursors.move);
      XFreeCursor(subtle->disp, subtle->cursors.resize);

      /* Free GCs */
      XFreeGC(subtle->disp, subtle->gcs.stipple);
      XFreeGC(subtle->disp, subtle->gcs.font);
      XFreeGC(subtle->disp, subtle->gcs.invert);
      if(subtle->xfs) XFreeFont(subtle->disp, subtle->xfs);

      /* Destroy view windows */
      XDestroySubwindows(subtle->disp, subtle->windows.bar);
      XDestroyWindow(subtle->disp, subtle->windows.bar);

      XCloseDisplay(subtle->disp);
    }

  subSharedLogDebug("kill=display\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
