#ifndef SUBTLE_H
#define SUBTLE_H 1

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <regex.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <X11/cursorfont.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif /* HAVE_SYS_INOTIFY */

/* Macros */
#define SUBWINNEW(parent,x,y,width,height,border) \
	XCreateWindow(d->disp, parent, x, y, width, height, border, CopyFromParent, \
		InputOutput, CopyFromParent, mask, &attrs);				// Shortcut

#define SUBWINWIDTH(w)	(w->width - 2 * d->bw)				// Get real window width
#define SUBWINHEIGHT(w)	(w->height - d->th - d->bw)		// Get real window height

/* win.c */
#define SUB_WIN_TYPE_VIEW			(1L << 1)						// View window
#define SUB_WIN_TYPE_TILE			(1L << 2)						// Tile window
#define SUB_WIN_TYPE_CLIENT		(1L << 3)						// Client window

#define SUB_WIN_STATE_SHADE		(1L << 4)						// Shaded window
#define SUB_WIN_STATE_FLOAT		(1L << 5)						// Floated window
#define SUB_WIN_STATE_FULL		(1L << 6)						// Fullscreen window
#define SUB_WIN_STATE_RESIZE	(1L << 7)						// Resized window
#define SUB_WIN_STATE_STACK		(1L << 8)						// Piled tiling window
#define SUB_WIN_STATE_TRANS		(1L << 9)						// Transient window

#define SUB_WIN_PREF_INPUT		(1L << 10)					// Active/passive focus-model
#define SUB_WIN_PREF_FOCUS		(1L << 11)					// Send focus message
#define SUB_WIN_PREF_CLOSE		(1L << 12)					// Send close message

#define SUB_WIN_TILE_VERT			(1L << 13)					// Vert-tile
#define SUB_WIN_TILE_HORZ			(1L << 14)					// Horiz-tile>

/* Forward declarations */
struct subtile;
struct subclient;

typedef struct subwin
{
	int			flags, views;																// Window flags and views
	int			x, y, width, height, resized;								// Window properties
	Window	frame;

	struct	subwin *parent;															// Parent window
	struct	subwin *next;																// Next sibling
	struct	subwin *prev;																// Prev sibling

	union
	{
		struct subtile		*tile;													// Tile data
		struct subclient	*client;												// Client data
	};
} SubWin;

SubWin *subWinNew(void);															// Create new window
void subWinDelete(SubWin *w);													// Delete window
void subWinDestroy(SubWin *w);												// Destroy window
void subWinRender(SubWin *w);													// Render wrapper
void subWinCut(SubWin *w);														// Cut window
void subWinPrepend(SubWin *w1, SubWin *w2);						// Prepend window
void subWinAppend(SubWin *w1, SubWin *w2);						// Append window
void subWinReplace(SubWin *w1, SubWin *w2);						// Replace two windows
void subWinSwap(SubWin *w1, SubWin *w2);							// Swap two windows
void subWinFocus(SubWin *w);													// Focus wrapper
void subWinResize(SubWin *w);													// Resize window
void subWinReparent(SubWin *p, SubWin *w);						// Reparent window
void subWinMap(SubWin *w);														// Map window
void subWinUnmap(SubWin *w);													// Unmap window

/* tile.c */
typedef struct subtile
{
	SubWin *pile, *first, *last;												// Pile top, first/last child
} SubTile;

SubWin *subTileNew(short mode);												// Create new tile
void subTileDelete(SubWin *t);												// Delete tile
void subTileDestroy(SubWin *t);												// Destroy tile
void subTileAdd(SubWin *t, SubWin *w);								// Add a window to tile
void subTileConfigure(SubWin *t);											// Configure tile

/* client.c */
#define SUB_CLIENT_DRAG_LEFT					(1L << 1)				// Drag start from left
#define SUB_CLIENT_DRAG_RIGHT					(1L << 2)				// Drag start from right
#define SUB_CLIENT_DRAG_BOTTOM				(1L << 3)				// Drag start from bottom
#define SUB_CLIENT_DRAG_MOVE					(1L << 4)				// Drag move start from titlebar
#define SUB_CLIENT_DRAG_SWAP					(1L << 5)				// Drag swap start from titlebar

#define SUB_CLIENT_DRAG_STATE_START		(1L << 1)				// Drag state start
#define SUB_CLIENT_DRAG_STATE_ABOVE		(1L << 2)				// Drag state above
#define SUB_CLIENT_DRAG_STATE_BELOW		(1L << 3)				// Drag state below
#define SUB_CLIENT_DRAG_STATE_BEFORE	(1L << 4)				// Drag state before
#define SUB_CLIENT_DRAG_STATE_AFTER		(1L << 5)				// Drag state after
#define SUB_CLIENT_DRAG_STATE_SWAP		(1L << 6)				// Drag state swap
#define SUB_CLIENT_DRAG_STATE_TOP			(1L << 7)				// Drag state split horiz top
#define SUB_CLIENT_DRAG_STATE_BOTTOM	(1L << 8)				// Drag state split horiz bottom
#define SUB_CLIENT_DRAG_STATE_LEFT		(1L << 9)				// Drag state split vert left
#define SUB_CLIENT_DRAG_STATE_RIGHT		(1L << 10)			// Drag state split vert right

typedef struct subclient
{
	int				n;																				// Client number
	char			*name;																		// Client name
	Colormap	cmap;																			// Client colormap	
	Window		caption, title, win;											// Subwindows
	Window		left, right, bottom;											// Border windows	
} SubClient;

SubWin *subClientNew(Window win);											// Create new client
void subClientDelete(SubWin *w);											// Delete client
void subClientDestroy(SubWin *w);											// Destroy client
void subClientRender(SubWin *w);											// Render the window
void subClientConfigure(SubWin *w);										// Send configure request
void subClientDrag(short mode, SubWin *w);						// Move/Resize a window
void subClientToggle(short type, SubWin *w);					// Toggle various states
void subClientFetchName(SubWin *w);										// Fetch client name
void subClientSetWMState(SubWin *w, long state);			// Set client WM state
long subClientGetWMState(SubWin *w);									// Get client WM state

/* view.c */
typedef struct subview
{
	int			width, xid;																	// View button width, view xid
	char 		*name;																			// View name
	Window	button;																			// View button

	regex_t	*regex;																			// View regex
	SubWin	*w;																					// View window

	struct subview *next;																// Next sibling
	struct subview *prev;																// Prev sibling
} SubView;

SubView *subViewFind(int xid);												// Find view
SubView *subViewNew(char *name, char *tags);					// Create new view
void subViewDelete(SubView *v);												// Delete view
void subViewDestroy(SubView *v);											// Destroy view
void subViewKill(void);																// Kill views
void subViewSift(Window win);													// Sift window
void subViewRender(void);															// Render views
void subViewConfigure(void);													// Configure views
void subViewSwitch(SubView *v);												// Switch view

/* sublet.c */
#define SUB_SUBLET_TYPE_TEXT		(1L << 1)							// Text sublet
#define SUB_SUBLET_TYPE_TEASER	(1L << 2)							// Teaser sublet
#define SUB_SUBLET_TYPE_METER		(1L << 3)							// Meter sublet
#define SUB_SUBLET_TYPE_WATCH		(1L << 4)							// Watch sublet

#define SUB_SUBLET_FAIL_FIRST		(1L << 5)							// Fail first time
#define SUB_SUBLET_FAIL_SECOND	(1L << 6)							// Fail second time
#define SUB_SUBLET_FAIL_THIRD		(1L << 7)							// Fail third time

typedef struct subsublet
{
	int			flags, ref;																	// Flags, Lua object reference, width
	time_t	time, interval;															// Last update time, interval time

	struct subsublet *next;															// Next sibling

	union 
	{
		char *string;																			// String data
		int number;																				// Number data
	};
} SubSublet;

void subSubletNew(int type, char *name, int ref, 			// Create new sublet
	time_t interval, char *watch);
void subSubletDelete(SubSublet *s);										// Delete sublet
void subSubletDestroy(SubSublet *s);									// Destroy sublet
void subSubletRender(void);														// Render sublet
void subSubletConfigure(void);												// Configure sublet bar
void subSubletSift(int pos);													// Sift sublet
SubSublet *subSubletNext(void);												// Get next sublet
void subSubletKill(void);															// Delete sublets

/* display.c */
typedef struct subdisplay
{
	Display						*disp;														// Connection to X server
	int								th, bw, fx, fy;										// Tab height, border widthm font metrics
	XFontStruct				*xfs;															// Font

	SubWin						*focus;														// Focus window
	SubView						*rv, *cv;													// Root view, current view

#ifdef HAVE_SYS_INOTIFY_H
	int								notify;														// Inotify descriptor
#endif

	struct
	{
		Window					win, views, sublets;							// Bar windows
	} bar;

	struct
	{
		unsigned long		font, border, norm, focus, 
										cover, bg;												// Colors
	} colors;
	struct
	{
		GC							font, border, invert;							// Graphic contexts (GCs)
	} gcs;
	struct
	{
		Cursor					square, move, arrow, horz, vert, 
										resize;														// Cursors
	} cursors;
} SubDisplay;

extern SubDisplay *d;

void subDisplayNew(const char *display_string);				// Create new display
void subDisplayKill(void);														// Delete display
void subDisplayScan(void);														// Scan root window

/* key.c */
#define SUB_KEY_ACTION_FOCUS_ABOVE			(1L << 1)			// Focus above window
#define SUB_KEY_ACTION_FOCUS_BELOW			(1L << 2)			// Focus below window
#define SUB_KEY_ACTION_FOCUS_NEXT				(1L << 3)			// Focus next window
#define SUB_KEY_ACTION_FOCUS_PREV				(1L << 4)			// Focus prev window
#define SUB_KEY_ACTION_FOCUS_ANY				(1L << 5)			// Focus any window

#define SUB_KEY_ACTION_DELETE_WIN				(1L << 6)			// Delete win
#define SUB_KEY_ACTION_TOGGLE_COLLAPSE	(1L << 7)			// Toggle collapse
#define SUB_KEY_ACTION_TOGGLE_RAISE			(1L << 8)			// Toggle raise
#define SUB_KEY_ACTION_TOGGLE_FULL			(1L << 9)			// Toggle fullscreen
#define SUB_KEY_ACTION_TOGGLE_PILE			(1L << 10)		// Toggle pile

#define SUB_KEY_ACTION_TOGGLE_LAYOUT		(1L << 11)		// Toggle tile layout
#define SUB_KEY_ACTION_DESKTOP_NEXT			(1L << 12)		// Switch to next desktop
#define SUB_KEY_ACTION_DESKTOP_PREV			(1L << 13)		// Switch to previous desktop
#define SUB_KEY_ACTION_DESKTOP_MOVE			(1L << 14)		// Move window to desktop
#define SUB_KEY_ACTION_EXEC							(1L << 15)		// Exec an app

typedef struct subkey
{
	int flags, code;
	unsigned int mod;

	union
	{
		char *string;
		int number;
	};
} SubKey;

void subKeyInit(void);																// Init the keys
SubKey *subKeyFind(int keycode, unsigned int mod);		// Find a key
void subKeyNew(const char *key,
	const char *value);																	// Create new key
void subKeyGrab(SubWin *w);														// Grab keys for a window
void subKeyUngrab(SubWin *w);													// Ungrab keys for a window
void subKeyKill(void);																// Delete all keys

/* lua.c */
void subLuaLoadConfig(const char *path);							// Load config file
void subLuaLoadSublets(const char *path);							// Load sublets
void subLuaKill(void);																// Kill Lua state
void subLuaCall(SubSublet *s);												// Call Lua script

/* event.c */
time_t subEventGetTime(void);													// Get the current time
int subEventLoop(void);																// Event loop

/* util.c */
#ifdef DEBUG
void subUtilLogToggle(void);
#define subUtilLogDebug(...)	subUtilLog(0, __FILE__, __LINE__, __VA_ARGS__);
#else
#define subUtilLogDebug(...)
#endif /* DEBUG */

#define subUtilLogError(...)	subUtilLog(1, __FILE__, __LINE__, __VA_ARGS__);
#define subUtilLogWarn(...)		subUtilLog(2, __FILE__, __LINE__, __VA_ARGS__);

void subUtilLog(short type, const char *file, 				// Print messages
	short line, const char *format, ...);
void *subUtilRealloc(void *mem, size_t size);					// Reallocate memory
void *subUtilAlloc(size_t n, size_t size);						// Allocate memory
XPointer *subUtilFind(Window win, XContext id);				// Find window data

/* ewmh.c */
enum SubEwmhHints
{
	/* ICCCM */
	SUB_EWMH_WM_NAME,																		// Name of window
	SUB_EWMH_WM_CLASS,																	// Class of window
	SUB_EWMH_WM_STATE,																	// Window state
	SUB_EWMH_WM_PROTOCOLS,															// Supported protocols 
	SUB_EWMH_WM_TAKE_FOCUS,															// Send focus messages
	SUB_EWMH_WM_DELETE_WINDOW,													// Send close messages
	SUB_EWMH_WM_NORMAL_HINTS,														// Window normal hints
	SUB_EWMH_WM_SIZE_HINTS,															// Window size hints

	/* EWMH */
	SUB_EWMH_NET_SUPPORTED,															// Supported states
	SUB_EWMH_NET_CLIENT_LIST,														// List of clients
	SUB_EWMH_NET_CLIENT_LIST_STACKING,									// List of clients
	SUB_EWMH_NET_NUMBER_OF_DESKTOPS,										// Total number of views
	SUB_EWMH_NET_DESKTOP_NAMES,													// Names of the views
	SUB_EWMH_NET_DESKTOP_GEOMETRY,											// Desktop geometry
	SUB_EWMH_NET_DESKTOP_VIEWPORT,											// Viewport of the view
	SUB_EWMH_NET_CURRENT_DESKTOP,												// Number of current view
	SUB_EWMH_NET_ACTIVE_WINDOW,													// Focus window
	SUB_EWMH_NET_WORKAREA,															// Workarea of the views
	SUB_EWMH_NET_SUPPORTING_WM_CHECK,										// Check for compliant window manager
	SUB_EWMH_NET_VIRTUAL_ROOTS,													// List of virtual destops
	SUB_EWMH_NET_CLOSE_WINDOW,

	SUB_EWMH_NET_WM_PID,																// PID of client
	SUB_EWMH_NET_WM_DESKTOP,														// Desktop client is on

	SUB_EWMH_NET_WM_STATE,															// Window state
	SUB_EWMH_NET_WM_STATE_MODAL,												// Modal window
	SUB_EWMH_NET_WM_STATE_SHADED,												// Shaded window
	SUB_EWMH_NET_WM_STATE_HIDDEN,												// Hidden window
	SUB_EWMH_NET_WM_STATE_FULLSCREEN,										// Fullscreen window

	SUB_EWMH_NET_WM_WINDOW_TYPE,
	SUB_EWMH_NET_WM_WINDOW_TYPE_DESKTOP,
	SUB_EWMH_NET_WM_WINDOW_TYPE_NORMAL,
	SUB_EWMH_NET_WM_WINDOW_TYPE_DIALOG,

	SUB_EWMH_NET_WM_ALLOWED_ACTIONS,
	SUB_EWMH_NET_WM_ACTION_MOVE,
	SUB_EWMH_NET_WM_ACTION_RESIZE,
	SUB_EWMH_NET_WM_ACTION_SHADE,
	SUB_EWMH_NET_WM_ACTION_FULLSCREEN,
	SUB_EWMH_NET_WM_ACTION_CHANGE_DESKTOP,
	SUB_EWMH_NET_WM_ACTION_CLOSE,

	/* Misc */
	SUB_EWMH_UTF8
};

void subEwmhInit(void);																	// Init atoms/hints
Atom subEwmhFind(int hint);															// Find atom

char * subEwmhGetProperty(Window win, Atom type,				// Get property
	int hint, unsigned long *size);
void subEwmhSetWindows(Window win, int hint, 
	Window *values, int size);														// Set window properties
void subEwmhSetCardinals(Window win, int hint,
	long *values, int size);															// Set cardinal properties
void subEwmhSetString(Window win, int hint, 
	char *value);																					// Set string property
void subEwmhSetStrings(Window win, int hint,						// Set string properties
	char **values, int size);

#endif /* SUBTLE_H */
