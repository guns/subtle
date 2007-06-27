
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* This program is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
	* (at your option) any later version.
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	* GNU General Public License for more details.
	*
	* You should have received a copy of the GNU General Public License along
	* with this program; if not, write to the Free Software Foundation, Inc.,
	* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
	**/

#include "subtle.h"

SubDisplay *d = NULL;

static int
HandleXError(Display *display,
	XErrorEvent *ev)
{
	if(ev->error_code == BadAccess && ev->resourceid == DefaultRootWindow(display))
		{
			subLogError("Seems there is another WM running. Exiting.\n");
		}
	else if(ev->request_code != 42) /* X_SetInputFocus */
		{
			char error[255];
			XGetErrorText(display, ev->error_code, error, sizeof(error));
			subLogDebug("%s: win=%#lx, request=%d\n", error, ev->resourceid, ev->request_code);
		}
	return(0);
}

 /**
	* Create the display 
	* @param display_string The display name as string
	* @return Returns either nonzero on success or otherwise zero
	**/

int
subDisplayNew(const char *display_string)
{
	XGCValues gvals;
	XWindowChanges wc;
	Pixmap shape, mask;
	static char stipple[] = {
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12		
	};

	/* Create display and setup error handler */
	if((d = (SubDisplay *)calloc(1, sizeof(SubDisplay))))
		{
			d->dpy = XOpenDisplay(display_string);
			if(!d->dpy) subLogError("Can't open display `%s'.\n", (display_string) ? display_string : ":0");
			XSetErrorHandler(HandleXError);
		}
	else subLogError("Can't alloc memory.\n");

	/* Create gcs */
	gvals.function		= GXcopy;
	gvals.fill_style	= FillStippled;
	gvals.stipple			= XCreateBitmapFromData(d->dpy, DefaultRootWindow(d->dpy), stipple, 15, 16);
	d->gcs.border			= XCreateGC(d->dpy, DefaultRootWindow(d->dpy),
		GCFunction|GCFillStyle|GCStipple, &gvals);

	d->gcs.font				= XCreateGC(d->dpy, DefaultRootWindow(d->dpy), GCFunction, &gvals);

	gvals.function				= GXinvert;
	gvals.subwindow_mode	= IncludeInferiors;
	gvals.line_width			= 3;
	d->gcs.invert 				= XCreateGC(d->dpy, DefaultRootWindow(d->dpy), 
		GCFunction|GCSubwindowMode|GCLineWidth, &gvals);

	/* Create cursors */
	d->cursors.square	= XCreateFontCursor(d->dpy, XC_dotbox);
	d->cursors.move		= XCreateFontCursor(d->dpy, XC_fleur);
	d->cursors.arrow	= XCreateFontCursor(d->dpy, XC_left_ptr);
	d->cursors.horz		= XCreateFontCursor(d->dpy, XC_sb_h_double_arrow);
	d->cursors.vert		= XCreateFontCursor(d->dpy, XC_sb_v_double_arrow);
	d->cursors.resize	= XCreateFontCursor(d->dpy, XC_sizing);

	printf("Display (%s) is %d x %d\n", DisplayString(d->dpy), DisplayWidth(d->dpy, 
		DefaultScreen(d->dpy)), DisplayHeight(d->dpy, DefaultScreen(d->dpy)));

	XSync(d->dpy, False);
}

 /**
	* Kill the display
	**/

void
subDisplayKill(void)
{
	if(d)
		{
		  XFreeCursor(d->dpy, d->cursors.square);
			XFreeCursor(d->dpy, d->cursors.move);
		  XFreeCursor(d->dpy, d->cursors.arrow);
		  XFreeCursor(d->dpy, d->cursors.horz);
		  XFreeCursor(d->dpy, d->cursors.vert);
		  XFreeCursor(d->dpy, d->cursors.resize);

			XFreeGC(d->dpy, d->gcs.border);
			XFreeGC(d->dpy, d->gcs.font);
			XFreeGC(d->dpy, d->gcs.invert);
			if(d->xfs) XFreeFont(d->dpy, d->xfs);
			free(d);
		}
}
