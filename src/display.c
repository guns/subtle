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
	else if(ev->error_code != 42) /* X_SetInputFocus */
		{
			char error[255];
			XGetErrorText(display, ev->error_code, error, sizeof(error));
			subLogDebug("%s: win=%#lx, request=%d\n", error, ev->resourceid, ev->request_code);
		}
	return(0);
}

 /**
	* Create the display.
	* @param display_string The display name as string
	* @return Returns either nonzero on success or otherwise zero
	**/

int
subDisplayNew(const char *display_string)
{
	XGCValues gvals;
	XWindowChanges wc;
	XColor fg, bg;
	Pixmap shape, mask;
	static char stipple[] = {
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92, 0x24,
		0x49, 0x12, 0x24, 0x49, 0x92, 0x24, 0x49, 0x12		
	};
	static char shape_bits[]	= {
		0x00, 0x00, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0x0e, 0x70, 0x0e, 0x70,
		0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70, 0x0e, 0x70,
		0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0x00, 0x00		
	};
	static char mask_bits[]	= {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f, 0xf8,
		0x1f, 0xf8, 0x1f, 0xf8, 0x1f, 0xf8, 0x1f, 0xf8, 0x1f, 0xf8, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	/* Create display and setup error handler */
	if((d = (SubDisplay *)calloc(1, sizeof(SubDisplay))))
		{
			d->dpy = XOpenDisplay(display_string);
			if(!d->dpy)
				{
					subLogError("Can't open display `%s'.\n", display_string);
					return(0);
				}
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
	shape	= XCreatePixmapFromBitmapData(d->dpy, DefaultRootWindow(d->dpy), shape_bits, 16, 16, 1, 0, 1);
	mask	= XCreatePixmapFromBitmapData(d->dpy, DefaultRootWindow(d->dpy), mask_bits, 16, 16, 1, 0, 1);
	XParseColor(d->dpy, DefaultColormap(d->dpy, DefaultScreen(d->dpy)), "black", &fg);
	XParseColor(d->dpy, DefaultColormap(d->dpy, DefaultScreen(d->dpy)), "white", &bg);
	d->cursors.square	= XCreatePixmapCursor(d->dpy, shape, mask, &fg, &bg, 1, 1);
	d->cursors.move		= XCreateFontCursor(d->dpy, XC_fleur);
	d->cursors.arrow	= XCreateFontCursor(d->dpy, XC_left_ptr);
	d->cursors.left		= XCreateFontCursor(d->dpy, XC_left_side);
	d->cursors.right	= XCreateFontCursor(d->dpy, XC_right_side);
	d->cursors.bottom	= XCreateFontCursor(d->dpy, XC_bottom_side);
	d->cursors.resize	= XCreateFontCursor(d->dpy, XC_sizing);

	printf("Display (%s) is %d x %d\n", DisplayString(d->dpy), DisplayWidth(d->dpy, 
		DefaultScreen(d->dpy)), DisplayHeight(d->dpy, DefaultScreen(d->dpy)));
	XSync(d->dpy, False);
}

 /**
	* Kill the display.
	**/

void
subDisplayKill(void)
{
	if(d)
		{
		  XFreeCursor(d->dpy, d->cursors.square);
			XFreeCursor(d->dpy, d->cursors.move);
		  XFreeCursor(d->dpy, d->cursors.arrow);
		  XFreeCursor(d->dpy, d->cursors.left);
		  XFreeCursor(d->dpy, d->cursors.right);
		  XFreeCursor(d->dpy, d->cursors.bottom);
		  XFreeCursor(d->dpy, d->cursors.resize);

			XFreeGC(d->dpy, d->gcs.border);
			XFreeGC(d->dpy, d->gcs.font);
			XFreeGC(d->dpy, d->gcs.invert);
			if(d->xfs) XFreeFont(d->dpy, d->xfs);
			free(d);
		}
}
