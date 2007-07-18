#ifndef SUBTLE_H
#define SUBTLE_H 1

/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

/* Macros */
#define SUBWINNEW(parent,x,y,width,height,border) \
	XCreateWindow(d->dpy, parent, x, y, width, height, border, CopyFromParent, \
		InputOutput, CopyFromParent, mask, &attrs);				// Shortcut

#define SUBWINWIDTH(w)	(w->width - 2 * d->bw)				// Get real window width
#define SUBWINHEIGHT(w)	(w->height - d->th - d->bw)		// Get real window height

/* win.c */
#define SUB_WIN_TYPE_SCREEN				(1L << 1)						// Screen window
#define SUB_WIN_TYPE_TILE					(1L << 2)						// Tile window
#define SUB_WIN_TYPE_CLIENT				(1L << 3)						// Client window

#define SUB_WIN_STATE_COLLAPSE		(1L << 4)						// Collapsed window
#define SUB_WIN_STATE_RAISE				(1L << 5)						// Raised window
#define SUB_WIN_STATE_FULL				(1L << 6)						// Fullscreen window
#define SUB_WIN_STATE_WEIGHT			(1L << 7)						// Weighted window
#define SUB_WIN_STATE_PILE				(1L << 8)						// Piled tiling window
#define SUB_WIN_STATE_TRANS				(1L << 9)						// Transient window

#define SUB_WIN_PREF_INPUT				(1L << 10)					// Active/passive focus-model
#define SUB_WIN_PREF_FOCUS				(1L << 11)					// Send focus message
#define SUB_WIN_PREF_CLOSE				(1L << 12)					// Send close message

#define SUB_WIN_TILE_VERT					(1L << 13)					// Vert-tile
#define SUB_WIN_TILE_HORZ					(1L << 14)					// Horiz-tile

#define SUB_WIN_DRAG_LEFT					(1L << 1)						// Drag start from left
#define SUB_WIN_DRAG_RIGHT				(1L << 2)						// Drag start from right
#define SUB_WIN_DRAG_BOTTOM				(1L << 3)						// Drag start from bottom
#define SUB_WIN_DRAG_MOVE					(1L << 4)						// Drag move start from titlebar
#define SUB_WIN_DRAG_SWAP					(1L << 5)						// Drag swap start from titlebar

#define SUB_WIN_DRAG_STATE_START	(1L << 1)						// Drag state start
#define SUB_WIN_DRAG_STATE_APPEND (1L << 2)						// Drag state append
#define SUB_WIN_DRAG_STATE_BELOW	(1L << 3)						// DRag state below
#define SUB_WIN_DRAG_STATE_SWAP		(1L << 4)						// Drag state swap

/* Forward declarations */
struct subtile;
struct subclient;

typedef struct subwin
{
	int			flags;																			// Window flags
	int			x, y, width, height, weight;								// Window properties
	Window	icon, frame, title, win;										// Subwindows
	Window	left, right, bottom;												// Border windows
	struct	subwin *parent;															// Parent window

	union
	{
		struct subtile		*tile;													// Tile data
		struct subclient	*client;												// Client data
	};
} SubWin;

SubWin *subWinFind(Window win);												// Find a window
SubWin *subWinNew(Window win);												// Create a new window
void subWinDelete(SubWin *w);													// Delete a window
void subWinRender(SubWin *w);													// Render a window
void subWinFocus(SubWin *w);													// Set focus
void subWinDrag(short mode, SubWin *w);								// Move/Resize a window
void subWinToggle(short type, SubWin *w);							// Toggle shading or floating state of a window
void subWinResize(SubWin *w);													// Resize a window
void subWinRestack(SubWin *w);												// Restack a window
void subWinMap(SubWin *w);														// Map a window
void subWinUnmap(SubWin *w);													// Unmap a window
void subWinRaise(SubWin *w);													// Raise a window

/* tile.c */
typedef struct subtile
{
	Window		btnew, btdel;															// Tile buttons
	SubWin		*peak;																		// Peak window
} SubTile;

SubWin *subTileNew(short mode);												// Create a new tile
void subTileDelete(SubWin *w);												// Delete a tile
void subTileRender(SubWin *w);												// Render a tile
void subTileAdd(SubWin *t, SubWin *w);								// Add a window to tile
void subTileConfigure(SubWin *w);											// Configure a tile

/* client.c */
typedef struct subclient
{
	char			*name;																		// Client name
	Window		caption;																	// Client caption
	Colormap	cmap;																			// Client colormap	
} SubClient;

SubWin *subClientNew(Window win);											// Create a new client
void subClientDelete(SubWin *w);											// Delete a client
void subClientRender(SubWin *w);											// Render the window
void subClientConfigure(SubWin *w);										// Send configure request
void subClientFetchName(SubWin *w);										// Fetch client name
void subClientSetWMState(SubWin *w, long state);			// Set client WM state
long subClientGetWMState(SubWin *w);									// Get client WM state

/* sublet.c */
#define SUB_SUBLET_TYPE_TEXT		(1L << 1)							// Text sublet
#define SUB_SUBLET_TYPE_TEASER	(1L << 2)							// Teaser sublet
#define SUB_SUBLET_TYPE_METER		(1L << 3)							// Text sublet

#define SUB_SUBLET_FAIL_FIRST		(1L << 4)							// Fail first time
#define SUB_SUBLET_FAIL_SECOND	(1L << 5)							// Fail second time
#define SUB_SUBLET_FAIL_THIRD		(1L << 6)							// Fail third time

typedef struct subsublet
{
	int			flags, ref, width;													// Flags, Lua object reference, width
	time_t	time, interval;															// Last update time, interval time
	Window	win;																				// Sublet window

	union 
	{
		char *string;
		int number;
	};
} SubSublet;

SubSublet *subSubletFind(Window win);									// Find a sublet
void subSubletInit(void);															// Init sublet list
void subSubletNew(int type, int ref, 									// Create a new sublet
	time_t interval, unsigned int width);
void subSubletDelete(SubSublet *s);										// Delete a sublet
void subSubletRender(SubSublet *s);										// Render a sublet
SubSublet *subSubletNext(void);												// Get the next sublet
void subSubletKill(void);															// Delete all sublets

/* screen.c */
#define SUB_SCREEN_NEXT		-3													// Next screen
#define SUB_SCREEN_PREV		-5													// Prev screen
#define SUB_SCREEN_ACTIVE -7													// Active screen

typedef struct subscreen
{
	int  width;																					// Screen button width
	char *name;																					// Screen names
	SubWin *w;																					// Root windows
	Window button;																			// Virtual screen buttons
} SubScreen;

SubScreen *subScreenFind(Window win);									// Find a screen
void subScreenInit(void);															// Init the screens
SubScreen *subScreenNew(void);												// Create a new screen
void subScreenDelete(SubWin *w);											// Delete a screen
void subScreenKill(void);															// Kill all screens
void subScreenRender(SubWin *w);											// Render the screen window
void subScreenAdd(Window win);												// Add a window to the bar
void subScreenConfigure(void);												// Configure the screen
SubScreen *subScreenGetPtr(int pos);									// Get screen pointer
int subScreenGetPos(SubScreen *s);										// Get screen position
void subScreenSwitch(int pos);												// Switch screens

/* display.c */
typedef struct subdisplay
{
	Display						*dpy;															// Connection to X server
	unsigned int			th, bw;														// Tab height, border width
	unsigned int			fx, fy;														// Font metrics
	XFontStruct				*xfs;															// Font

	SubWin						*focus;														// Focus window

	struct
	{
		Window					win, screens, sublets;						// Screen bar
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

int subDisplayNew(const char *display_string);				// Create a new display
void subDisplayKill(void);														// Delete a display

/* log.c */
#ifdef DEBUG
void subLogToggleDebug(void);
#define subLogDebug(...)	subLog(0, __FILE__, __LINE__, __VA_ARGS__);
#else
#define subLogDebug(...)
#endif /* DEBUG */

#define subLogError(...)	subLog(1, __FILE__, __LINE__, __VA_ARGS__);
#define subLogWarn(...)		subLog(2, __FILE__, __LINE__, __VA_ARGS__);

void subLog(short type, const char *file, 						// Print messages
	short line, const char *format, ...);

/* key.c */
#define SUB_KEY_ACTION_ADD_VTILE				(1L << 1)			// Add vert-tile
#define SUB_KEY_ACTION_ADD_HTILE				(1L << 2)			// Add horiz-tile
#define SUB_KEY_ACTION_DELETE_WIN				(1L << 3)			// Delete win
#define SUB_KEY_ACTION_TOGGLE_COLLAPSE	(1L << 4)			// Toggle collapse
#define SUB_KEY_ACTION_TOGGLE_RAISE			(1L << 5)			// Toggle raise
#define SUB_KEY_ACTION_TOGGLE_FULL			(1L << 6)			// Toggle fullscreen
#define SUB_KEY_ACTION_TOGGLE_WEIGHT		(1L << 7)			// Toggle weight
#define SUB_KEY_ACTION_TOGGLE_PILE			(1L << 8)			// Toggle pile
#define SUB_KEY_ACTION_DESKTOP_NEXT			(1L << 9)			// Switch to next desktop
#define SUB_KEY_ACTION_DESKTOP_PREV			(1L << 10)		// Switch to previous desktop
#define SUB_KEY_ACTION_DESKTOP_MOVE			(1L << 11)		// Move window to desktop
#define SUB_KEY_ACTION_EXEC							(1L << 12)		// Exec an app

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

void subKeyParseChain(const char *key,
	const char *value);																	// Parse key chain
SubKey *subKeyFind(int keycode, int mod);							// Find a key
void subKeyGrab(SubWin *w);														// Grab keys for a window
void subKeyUnrab(SubWin *w);													// Ungrab keys for a window

/* lua.c */
void subLuaLoadConfig(const char *path);							// Load config file
void subLuaLoadSublets(const char *path);							// Load sublets
void subLuaKill(void);																// Kill Lua state
void subLuaCall(SubSublet *s);												// Call a Lua script

/* event.c */
time_t subEventGetTime(void);													// Get the current time
int subEventLoop(void);																// Event loop

/* ewmh.c */
enum SubEwmhHints
{
	/* ICCCM */
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
	SUB_EWMH_NET_NUMBER_OF_DESKTOPS,										// Total number of desktops
	SUB_EWMH_NET_DESKTOP_GEOMETRY,											// Desktop geometry
	SUB_EWMH_NET_DESKTOP_VIEWPORT,											// Viewport of the desktop
	SUB_EWMH_NET_CURRENT_DESKTOP,												// Number of current desktop
	SUB_EWMH_NET_ACTIVE_WINDOW,													// Focus window
	SUB_EWMH_NET_WORKAREA,															// Workarea of the desktop
	SUB_EWMH_NET_SUPPORTING_WM_CHECK,										// Check for compliant window manager
	SUB_EWMH_NET_VIRTUAL_ROOTS,													// List of virtual destops
	SUB_EWMH_NET_CLOSE_WINDOW,

	SUB_EWMH_NET_WM_NAME,																// Name of a window
	SUB_EWMH_NET_WM_PID,																// PID of a client
	SUB_EWMH_NET_WM_DESKTOP,														// Desktop a client is on

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
	SUB_EWMH_NET_WM_ACTION_CLOSE
};

void subEwmhInit(void);																	// Init atoms/hints
Atom subEwmhGetAtom(int hint);													// Get an atom

int subEwmhSetWindow(Window win, int hint, 
	Window value);																				// Set window property
int subEwmhSetWindows(Window win, int hint, 
	Window *values, int size);														// Set window properties
int subEwmhSetString(Window win, int hint, 
	const char *value);																		// Set string property
int subEwmhGetString(Window win, int hint,
	char *value);																					// Get string property
int subEwmhSetCardinal(Window win, int hint,
	long value);																					// Set cardinal property
int subEwmhSetCardinals(Window win, int hint,
	long *values, int size);															// Set cardinal properties
int subEwmhGetCardinal(Window win, int hint,
	long *value);																					// Get cardinal property

void * subEwmhGetProperty(Window win, int hint,
	Atom type, int *num);																	// Get window properties
void subEwmhDeleteProperty(Window win, int hint);				// Delete window property

#endif /* SUBTLE_H */
