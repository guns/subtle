
 /**
	* @package subtle
	*
	* @file Display functions
	* @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
	* @version $Id$
	*
	* This program can be distributed under the terms of the GNU GPL.
	* See the file COPYING.
	**/

#include "subtle.h"

SubDisplay *d = NULL;

 /** subDisplayNew {{{
	* @Open connection to X server and create display
	* @param[in] display_string	The display name as string
	* @return Returns a #SubDisplay or \p NULL
	**/

SubDisplay *
subDisplayNew(const char *display_string)
{
	XGCValues gvals;
	const char stipple[] = {
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12		
	};

	/* Create display and setup error handler */
	d = (SubDisplay *)subUtilAlloc(1, sizeof(SubDisplay));
	d->disp = XOpenDisplay(display_string);
	if(!d->disp) subUtilLogError("Can't open display `%s'.\n", (display_string) ? display_string : ":0.0");
	XSetErrorHandler(subUtilLogXError);

	/* Create gcs */
	gvals.function		= GXcopy;
	gvals.fill_style	= FillStippled;
	gvals.stipple			= XCreateBitmapFromData(d->disp, DefaultRootWindow(d->disp), stipple, 15, 16);
	d->gcs.border			= XCreateGC(d->disp, DefaultRootWindow(d->disp),
		GCFunction|GCFillStyle|GCStipple, &gvals);

	d->gcs.font				= XCreateGC(d->disp, DefaultRootWindow(d->disp), GCFunction, &gvals);

	gvals.function				= GXinvert;
	gvals.subwindow_mode	= IncludeInferiors;
	gvals.line_width			= 3;
	d->gcs.invert 				= XCreateGC(d->disp, DefaultRootWindow(d->disp), 
		GCFunction|GCSubwindowMode|GCLineWidth, &gvals);

	/* Create cursors */
	d->cursors.square	= XCreateFontCursor(d->disp, XC_dotbox);
	d->cursors.move		= XCreateFontCursor(d->disp, XC_fleur);
	d->cursors.arrow	= XCreateFontCursor(d->disp, XC_left_ptr);
	d->cursors.horz		= XCreateFontCursor(d->disp, XC_sb_h_double_arrow);
	d->cursors.vert		= XCreateFontCursor(d->disp, XC_sb_v_double_arrow);
	d->cursors.resize	= XCreateFontCursor(d->disp, XC_sizing);

	/* Init lists */
	d->keys			= subArrayNew();
	d->tags			= subArrayNew();
	d->views		= subArrayNew();
	d->clients	= subArrayNew();
	d->sublets	= subArrayNew();

	printf("Display (%s) is %dx%d\n", DisplayString(d->disp), DisplayWidth(d->disp, 
		DefaultScreen(d->disp)), DisplayHeight(d->disp, DefaultScreen(d->disp)));

	XSync(d->disp, False);

	return(d);
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

	assert(d);

	XQueryTree(d->disp, DefaultRootWindow(d->disp), &dummy, &dummy, &wins, &n);
	for(i = 0; i < n; i++)
		{
			/* Skip own windows */
			if(wins[i] && wins[i] != d->bar.win && wins[i] != d->cv->frame)
				{
					XGetWindowAttributes(d->disp, wins[i], &attr);
					if(attr.map_state == IsViewable) subClientNew(wins[i]);
				}
		}
	XFree(wins);

	subClientPublish();
	subViewUpdate();
	subViewConfigure(d->cv);
	subViewRender();
} /* }}} */

 /** subDisplayKill {{{
	* @brief Close connection and kill display
	**/

void
subDisplayKill(void)
{
	assert(d);

	if(d->disp)
		{
			/* Free cursors */
			XFreeCursor(d->disp, d->cursors.square);
			XFreeCursor(d->disp, d->cursors.move);
			XFreeCursor(d->disp, d->cursors.arrow);
			XFreeCursor(d->disp, d->cursors.horz);
			XFreeCursor(d->disp, d->cursors.vert);
			XFreeCursor(d->disp, d->cursors.resize);

			/* Free GCs */
			XFreeGC(d->disp, d->gcs.border);
			XFreeGC(d->disp, d->gcs.font);
			XFreeGC(d->disp, d->gcs.invert);
			if(d->xfs) XFreeFont(d->disp, d->xfs);

			/* Destroy view windows */
			XDestroySubwindows(d->disp, d->bar.win);
			XDestroyWindow(d->disp, d->bar.win);

			XCloseDisplay(d->disp);
		}
	free(d);

	subUtilLogDebug("kill=display\n");
} /* }}} */
