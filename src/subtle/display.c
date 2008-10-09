
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
  if(!subtle->disp) subUtilLogError("Can't open display `%s'.\n", (display) ? display : ":0.0");
  XSetErrorHandler(subUtilLogXError);

  /* Create gcs */
  gvals.function      = GXcopy;
  gvals.fill_style    = FillStippled;
  gvals.stipple       = XCreateBitmapFromData(subtle->disp, DefaultRootWindow(subtle->disp), stipple, 15, 16);
  subtle->gcs.border  = XCreateGC(subtle->disp, DefaultRootWindow(subtle->disp),
    GCFunction|GCFillStyle|GCStipple, &gvals);

  subtle->gcs.font      = XCreateGC(subtle->disp, DefaultRootWindow(subtle->disp), GCFunction, &gvals);

  gvals.function        = GXinvert;
  gvals.subwindow_mode  = IncludeInferiors;
  gvals.line_width      = 3;
  subtle->gcs.invert    = XCreateGC(subtle->disp, DefaultRootWindow(subtle->disp), 
    GCFunction|GCSubwindowMode|GCLineWidth, &gvals);

  /* Create cursors */
  subtle->cursors.square  = XCreateFontCursor(subtle->disp, XC_dotbox);
  subtle->cursors.move    = XCreateFontCursor(subtle->disp, XC_fleur);
  subtle->cursors.arrow   = XCreateFontCursor(subtle->disp, XC_left_ptr);
  subtle->cursors.horz    = XCreateFontCursor(subtle->disp, XC_sb_h_double_arrow);
  subtle->cursors.vert    = XCreateFontCursor(subtle->disp, XC_sb_v_double_arrow);
  subtle->cursors.resize  = XCreateFontCursor(subtle->disp, XC_sizing);

  /* Init lists */
  subtle->grabs    = subArrayNew();
  subtle->tags    = subArrayNew();
  subtle->views   = subArrayNew();
  subtle->clients = subArrayNew();
  subtle->sublets = subArrayNew();

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

  assert(subtle);

  XQueryTree(subtle->disp, DefaultRootWindow(subtle->disp), &dummy, &dummy, &wins, &n);
  for(i = 0; i < n; i++)
    {
      /* Skip own windows */
      if(wins[i] && wins[i] != subtle->bar.win && wins[i] != subtle->cv->frame)
        {
          XGetWindowAttributes(subtle->disp, wins[i], &attr);
          if(attr.map_state == IsViewable) 
            {
              /* Create new client */
              SubClient *c = subClientNew(wins[i]);
              subArrayPush(subtle->clients, c);
            }
        }
    }
  XFree(wins);

  subClientPublish();
  subViewConfigure(subtle->cv);
  subViewRender();
} /* }}} */

 /** subDisplayKill {{{
  * @brief Close connection
  **/

void
subDisplayFinish(void)
{
  assert(subtle);

  if(subtle->disp)
    {
      /* Free cursors */
      XFreeCursor(subtle->disp, subtle->cursors.square);
      XFreeCursor(subtle->disp, subtle->cursors.move);
      XFreeCursor(subtle->disp, subtle->cursors.arrow);
      XFreeCursor(subtle->disp, subtle->cursors.horz);
      XFreeCursor(subtle->disp, subtle->cursors.vert);
      XFreeCursor(subtle->disp, subtle->cursors.resize);

      /* Free GCs */
      XFreeGC(subtle->disp, subtle->gcs.border);
      XFreeGC(subtle->disp, subtle->gcs.font);
      XFreeGC(subtle->disp, subtle->gcs.invert);
      if(subtle->xfs) XFreeFont(subtle->disp, subtle->xfs);

      /* Destroy view windows */
      XDestroySubwindows(subtle->disp, subtle->bar.win);
      XDestroyWindow(subtle->disp, subtle->bar.win);

      XCloseDisplay(subtle->disp);
    }

  subUtilLogDebug("kill=display\n");
} /* }}} */
