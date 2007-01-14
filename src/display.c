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
	else
		{
			char error[255];
			XGetErrorText(display, ev->error_code, error, sizeof(error));
			subLogDebug("%s: win=%#lx, request=%d\n", error, ev->resourceid, ev->request_code);
		}
	return(0);
}

static double
ParseColor(const char *field,
	char *fallback)
{
	XColor color;
	char *name = subLuaGetString("colors", field, fallback);
	color.pixel = 0;

	if(!XParseColor(d->dpy, DefaultColormap(d->dpy, DefaultScreen(d->dpy)), name, &color))
		{
			subLogWarn("Can't load color '%s'.\n", name);
		}
	else if(!XAllocColor(d->dpy, DefaultColormap(d->dpy, DefaultScreen(d->dpy)), &color))
		subLogWarn("Can't alloc color '%s'.\n", name);
	return(color.pixel);
}

static void
BindKey(const char *key,
	const char *value)
{
	int n = 0;
	KeySym *keys = NULL;
	char *tok = strtok((char *) value, "+");

	while(tok)
		{
			keys 				= (KeySym *)realloc(keys, ++n * sizeof(KeySym));
			keys[n - 1] = XStringToKeysym(tok);
			tok					= strtok(NULL, "+");
		}
	XRebindKeysym(d->dpy, keys[n - 1], keys, n - 1, key, strlen(key));
	subLogDebug("Adding keychain: name=%s, keys=%d\n", key, n);
	free(keys);
}

 /**
	* Create the display.
	* @param display_string The display name as string
	* @return Returns either nonzero on success or otherwise zero
	**/

int
subDisplayNew(const char *display_string)
{
	int i, size;
	char font[50], *face = NULL, *style = NULL;
	XGCValues gvals;
	XWindowChanges wc;
	XSetWindowAttributes attrs;
	XColor fg, bg;
	Pixmap shape, mask;
	Colormap cmap;
	Window root;
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

	root	= DefaultRootWindow(d->dpy);
	cmap	= DefaultColormap(d->dpy, DefaultScreen(d->dpy));
	
	/* Parse and load the font */
	face	= subLuaGetString("font", "face", "fixed");
	style	= subLuaGetString("font", "style", "medium");
	size	= subLuaGetNum("font", "size", 12);

	snprintf(font, sizeof(font), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", face, style, size);
	if(!(d->xfs = XLoadQueryFont(d->dpy, font)))
		{
			subLogWarn("Can't load font `%s', using fixed instead.\n", face);
			subLogDebug("Font string was: %s\n", font);
			d->xfs = XLoadQueryFont(d->dpy, "-*-fixed-medium-*-*-*-12-*-*-*-*-*-*-*");
		}

	/* Font metrics */
	d->fx	= (d->xfs->min_bounds.width + d->xfs->max_bounds.width) / 2;
	d->fy	= d->xfs->max_bounds.ascent + d->xfs->max_bounds.descent;

	d->th	= d->xfs->ascent + d->xfs->descent + 2;
	d->bw	= subLuaGetNum("options", "border",	2);

	/* Read colors from config */
	d->colors.font		= ParseColor("font",				"#000000"); 	
	d->colors.border	= ParseColor("border",			"#bdbabd");
	d->colors.norm		= ParseColor("normal",			"#22aa99");
	d->colors.act			= ParseColor("active",			"#ffa500");		
	d->colors.bg			= ParseColor("background",	"#336699");

	/* Create gcs */
	gvals.function		= GXcopy;
	gvals.foreground	= d->colors.border;
	gvals.fill_style	= FillStippled;
	gvals.line_width	= d->bw;
	gvals.stipple			= XCreateBitmapFromData(d->dpy, root, stipple, 15, 16);
	d->gcs.border			= XCreateGC(d->dpy, root,
		GCFunction|GCForeground|GCFillStyle|GCStipple|GCLineWidth, &gvals);

	gvals.foreground	= d->colors.font;
	gvals.font				= d->xfs->fid;
	d->gcs.font				= XCreateGC(d->dpy, root, GCFunction|GCForeground|GCFont, &gvals);
					
	gvals.function				= GXinvert;
	gvals.subwindow_mode	= IncludeInferiors;
	gvals.line_width			= 2;
	d->gcs.invert 				= XCreateGC(d->dpy, root, GCFunction|GCSubwindowMode|GCLineWidth, &gvals);

	/* Create cursors */
	shape	= XCreatePixmapFromBitmapData(d->dpy, root, shape_bits, 16, 16, 1, 0, 1);
	mask	= XCreatePixmapFromBitmapData(d->dpy, root, mask_bits, 16, 16, 1, 0, 1);
	XParseColor(d->dpy, cmap, "black", &fg);
	XParseColor(d->dpy, cmap, "white", &bg);
	d->cursors.square	= XCreatePixmapCursor(d->dpy, shape, mask, &fg, &bg, 1, 1);
	d->cursors.arrow	= XCreateFontCursor(d->dpy, XC_left_ptr);
	d->cursors.left		= XCreateFontCursor(d->dpy, XC_left_side);
	d->cursors.right	= XCreateFontCursor(d->dpy, XC_right_side);
	d->cursors.bottom	= XCreateFontCursor(d->dpy, XC_bottom_side);
	d->cursors.resize	= XCreateFontCursor(d->dpy, XC_sizing);

	attrs.cursor						= d->cursors.arrow;
	attrs.background_pixel	= d->colors.bg;
	attrs.event_mask				= SubstructureRedirectMask|SubstructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(d->dpy, root, CWCursor|CWBackPixel|CWEventMask, &attrs);
	XClearWindow(d->dpy, root);

	subLuaForeach("keys", BindKey);

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
  XFreeCursor(d->dpy, d->cursors.square);
  XFreeCursor(d->dpy, d->cursors.arrow);
  XFreeCursor(d->dpy, d->cursors.left);
  XFreeCursor(d->dpy, d->cursors.right);
  XFreeCursor(d->dpy, d->cursors.bottom);
  XFreeCursor(d->dpy, d->cursors.resize);

	XFreeGC(d->dpy, d->gcs.border);
	XFreeGC(d->dpy, d->gcs.font);
	XFreeGC(d->dpy, d->gcs.invert);
	XFreeFont(d->dpy, d->xfs);
	free(d);
}
