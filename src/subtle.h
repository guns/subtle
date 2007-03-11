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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

/* Masks */
#define ENTER_MASK			(EnterWindowMask|LeaveWindowMask)
#define BUTTON_MASK			(ButtonPressMask|ButtonReleaseMask)
#define STRUCT_MASK			(StructureNotifyMask|SubstructureNotifyMask)
#define KEY_MASK				(KeyPressMask|KeyReleaseMask)
#define FRAME_MASK			(ENTER_MASK|BUTTON_MASK|KEY_MASK|FocusChangeMask|VisibilityChangeMask|ExposureMask)

#define SUBPOINTERMASK	(ButtonPressMask|ButtonReleaseMask|PointerMotionMask)

/* Macros */
#define SUBWINNEW(parent,x,y,width,height,border) \
	XCreateWindow(d->dpy, parent, x, y, width, height, border, CopyFromParent, \
		InputOutput, CopyFromParent, mask, &attrs);

#define SUBWINWIDTH(w)	(w->width - 2 * d->bw)
#define SUBWINHEIGHT(w)	(w->height - d->th - d->bw)

#define SUBISSCREEN(w)	(w && w->prop & SUB_WIN_SCREEN)
#define SUBISTILE(w) 		(w && w->prop & (SUB_WIN_TILEH|SUB_WIN_TILEV))
#define SUBISCLIENT(w)	(w && w->prop & SUB_WIN_CLIENT)

/* win.c */
#define SUB_WIN_SCREEN			(1L << 1)									// Screen window
#define SUB_WIN_CLIENT			(1L << 2)									// Client window
#define SUB_WIN_TILEH				(1L << 3)									// Horizontal tiling window
#define SUB_WIN_TILEV				(1L << 4)									// Vertical tiling window
#define SUB_WIN_SUBLET			(1L << 5)									// Sublet window
#define SUB_WIN_TRANS				(1L << 6)									// Transient window
#define SUB_WIN_FLOAT				(1L << 7)									// Floating window
#define SUB_WIN_SHADED			(1L << 8)									// Shaded window
#define SUB_WIN_FIXED				(1L << 9)									// Fixed size window
#define SUB_WIN_INPUT				(1L << 10)								// Expect to get focus active/passiv
#define SUB_WIN_SEND_FOCUS	(1L << 11)								// Send focus messages
#define SUB_WIN_SEND_CLOSE	(1L << 12)								// Send close messages

#define SUB_WIN_DRAG_LEFT		(1L << 1)									// Drag start from left
#define SUB_WIN_DRAG_RIGHT	(1L << 2)									// Drag start from right
#define SUB_WIN_DRAG_BOTTOM	(1L << 3)									// Drag start from bottom
#define SUB_WIN_DRAG_MOVE		(1L << 4)									// Drag move start from titlebar
#define SUB_WIN_DRAG_SWAP		(1L << 5)									// Drag swap start from titlebar
#define SUB_WIN_DRAG_ICON		(1L << 6)									// Drag start from icon

struct subtile;
struct subclient;

typedef struct subwin
{
	int			x, y, width, height, prop, fixed;						// Window properties
	Window	icon, frame, title, win;										// Subwindows
	Window	left, right, bottom;												// Border windows
	struct subwin *parent;															// Parent window
	union
	{
		struct subtile *tile;
		struct subclient *client;
	};
} SubWin;

SubWin *subWinFind(Window win);												// Find a window
SubWin *subWinNew(Window win);												// Create a new window
void subWinDelete(SubWin *w);													// Delete a window
void subWinDrag(short mode, SubWin *w, 
	XButtonEvent *bev);																	// Move/Resize a window
void subWinToggle(short type, SubWin *w);							// Toggle shading or floating state of a window
void subWinResize(SubWin *w);													// Resize a window
void subWinRender(short mode, SubWin *w);							// Render a window
void subWinRestack(SubWin *w);												// Restack a window
void subWinMap(SubWin *w);														// Map a window
void subWinRaise(SubWin *w);													// Raise a window

/* tile.c */
typedef struct subtile
{
	unsigned int 		mw, mh;															// Tile min. width / height
	Window					btnew, btdel;												// Tile buttons
} SubTile;

#define subTileNewHoriz()	subTileNew(0)
#define subTileNewVert()	subTileNew(1)
SubWin *subTileNew(short mode);												// Create a new tile
void subTileAdd(SubWin *t, SubWin *w);								// Add a window to the tile
void subTileDelete(SubWin *w);												// Delete a tile
void subTileRender(short mode, SubWin *w);						// Render a tile
void subTileConfigure(SubWin *w);											// Configure a tile and it's children

/* client.c */
typedef struct subclient
{
	char			*name;																		// Client name
	Window		caption;																	// Client caption
	Colormap	cmap;																			// Client colormap	
} SubClient;

SubWin *subClientNew(Window win);											// Create a new client
void subClientDelete(SubWin *w);											// Delete a client
void subClientSetWMState(SubWin *w, long state);			// Set client WM state
long subClientGetWMState(SubWin *w);									// Get client WM state
void subClientSendConfigure(SubWin *w);								// Send configure request
void subClientSendDelete(SubWin *w);									// Send delete request
void subClientToggleShade(SubWin *w);									// Toggle shaded state
void subClientFetchName(SubWin *w);										// Fetch client name
void subClientRender(short mode, SubWin *w);					// Render the window

/* screen.c */
typedef struct subscreen
{
	int n;																							// Number of screens
	SubWin **wins;																			// Root windows
	SubWin *active;																			// Active screen
	Window statusbar;																		// Statusbar
} SubScreen;

extern SubScreen *screen;

void subScreenNew(void);															// Create a new screen
void subScreenAdd(void);															// Add a screen
void subScreenDelete(SubWin *w);											// Delete a screen
void subScreenKill(void);															// Kill all screens
void subScreenSwitch(int dir);												// Switch current screen

/* display.c */
typedef struct subdisplay
{
	Display						*dpy;															// Connection to X server
	Window						focus;														// Focus window
	unsigned int			th, bw;														// Tab height, border
	unsigned int			fx, fy;														// Font metrics
	XFontStruct				*xfs;															// Font
	struct
	{
		unsigned long		font, border, norm, focus, 
										shade, bg;												// Colors
	} colors;
	struct
	{
		GC							font, border, invert;							// Used graphic contexts (GCs)
	} gcs;
	struct
	{
		Cursor					square, move, arrow, left, right, 
										bottom, resize;										// Used cursors
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

/* sublet.c */
#define SUB_SUBLET_TEXT		(1L << 1)										// Text sublet
#define SUB_SUBLET_TEASER	(1L << 2)										// Teaser sublet
#define SUB_SUBLET_METER	(1L << 3)										// Text sublet

typedef union subsubletdata
{
	char *string;
	int number;
} SubSubletData;

typedef struct subsublet
{
	int ref, type, width;																// Lua object referece, Sublet type width
	unsigned int time, interval;												// Last update time, update interval
	Window win;																					// Sublet window
	SubSubletData data; 																// Sublet data
} SubSublet;

typedef struct subsubletlist
{
	int size;
	SubSublet **content;
} SubSubletList;

SubSublet *subSubletFind(Window win);									// Find a sublet window
SubSublet *subSubletGetRecent(void);									// Get the last recent sublet
void subSubletNew(void);															// Create a sublet
void subSubletAdd(int type, int ref, 									// Add a new sublet item 
	unsigned int interval, unsigned int width);
void subSubletRender(SubSublet *s);										// Render a sublet
void subSubletKill(void);															// Delete all sublet items

/* lua.c */
void subLuaLoadConfig(const char *path);							// Load config file
void subLuaLoadSublets(const char *path);							// Load sublets
void subLuaKill(void);																// Kill Lua state
void subLuaCall(int ref, SubSubletData *data);				// Call a Lua script

/* event.c */
int subEventGetTime(void);														// Get the current time
int subEventLoop(void);																// Event loop

/* ewmh.c */
enum SubEwmhHints
{
	/* ICCCM */
	SUB_EWMH_WM_STATE,																	// Window state
	SUB_EWMH_WM_CHANGE_STATE,
	SUB_EWMH_WM_PROTOCOLS,															// Supported protocols 
	SUB_EWMH_WM_COLORMAP_WINDOWS,
	SUB_EWMH_WM_TAKE_FOCUS,															// Send focus messages
	SUB_EWMH_WM_WINDOW_ROLE,
	SUB_EWMH_WM_DELETE_WINDOW,													// Send close messages

	/* EWMH */
	SUB_EWMH_NET_SUPPORTED,
	SUB_EWMH_NET_CLIENT_LIST,
	SUB_EWMH_NET_NUMBER_OF_DESKTOPS,										// Total number of desktops
	SUB_EWMH_NET_DESKTOP_GEOMETRY,											// Desktop geometry
	SUB_EWMH_NET_DESKTOP_VIEWPORT,											// Viewport of the desktop
	SUB_EWMH_NET_CURRENT_DESKTOP,												// Number of current desktop
	SUB_EWMH_NET_ACTIVE_WINDOW,
	SUB_EWMH_NET_WORKAREA,															// Workarea of the desktop
	SUB_EWMH_NET_SUPPORTING_WM_CHECK,										// Check for compliant window manager
	SUB_EWMH_NET_VIRTUAL_ROOTS,													// List of virtual destops
	SUB_EWMH_NET_CLOSE_WINDOW,

	SUB_EWMH_NET_WM_NAME,																// Name of a window
	SUB_EWMH_NET_WM_PID,																// PID of a client
	SUB_EWMH_NET_WM_DESKTOP,

	SUB_EWMH_NET_WM_STATE,
	SUB_EWMH_NET_WM_STATE_SHADED,
	SUB_EWMH_NET_WM_STATE_FULLSCREEN,

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

void subEwmhNew(void);																	// Register atoms/hints
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
