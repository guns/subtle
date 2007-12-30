
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Header$
	**/

#include "subtle.h"

SubDisplay *d = NULL;

static int
HandleXError(Display *display,
	XErrorEvent *ev)
{
	if(ev->error_code == BadAccess && ev->resourceid == DefaultRootWindow(display))
		{
			subUtilLogError("Seems there is another WM running. Exiting.\n");
		}
	else if(ev->request_code != 42) /* X_SetInputFocus */
		{
			char error[255];
			XGetErrorText(display, ev->error_code, error, sizeof(error));
			subUtilLogDebug("%s: win=%#lx, request=%d\n", error, ev->resourceid, ev->request_code);
		}
	return(0);
}

 /**
	* Create the display 
	* @param display_string The display name as string
	* @return Returns either nonzero on success or otherwise zero
	**/

void
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
	XSetErrorHandler(HandleXError);

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

	printf("Display (%s) is %dx%d\n", DisplayString(d->disp), DisplayWidth(d->disp, 
		DefaultScreen(d->disp)), DisplayHeight(d->disp, DefaultScreen(d->disp)));

	XSync(d->disp, False);
}

 /**
	* Kill the display
	**/

void
subDisplayKill(void)
{
	assert(d);

	if(d->disp)
		{
			XFreeCursor(d->disp, d->cursors.square);
			XFreeCursor(d->disp, d->cursors.move);
			XFreeCursor(d->disp, d->cursors.arrow);
			XFreeCursor(d->disp, d->cursors.horz);
			XFreeCursor(d->disp, d->cursors.vert);
			XFreeCursor(d->disp, d->cursors.resize);

			XFreeGC(d->disp, d->gcs.border);
			XFreeGC(d->disp, d->gcs.font);
			XFreeGC(d->disp, d->gcs.invert);
			if(d->xfs) XFreeFont(d->disp, d->xfs);

			XCloseDisplay(d->disp);
		}
	free(d);
}

 /**
	* Scan root window for clients
	**/

void
subDisplayScan(void)
{
	unsigned int i, n;
	Window unused, *wins = NULL;
	XWindowAttributes attr;

	assert(d);

	XQueryTree(d->disp, DefaultRootWindow(d->disp), &unused, &unused, &wins, &n);
	for(i = 0; i < n; i++)
		{
			XGetWindowAttributes(d->disp, wins[i], &attr);
			if(wins[i] != d->bar.win && wins[i] != d->bar.views && 
				wins[i] != d->bar.sublets && wins[i] != d->cv->w->frame && 
				wins[i] != d->cv->button && attr.map_state == IsViewable) subViewMerge(wins[i]);
		}
	
	subTileConfigure(d->cv->w);
	XFree(wins);
}
