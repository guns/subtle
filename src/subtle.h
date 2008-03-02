
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
	*
	* $Header$
	**/

#ifndef SUBTLE_H
#define SUBTLE_H 1

/* Includes {{{ */
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

#include "config.h"

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif /* HAVE_SYS_INOTIFY */
/* }}} */

/* Macros {{{ */
#define SUBWINNEW(parent,x,y,width,height,border,mask) \
	XCreateWindow(d->disp, parent, x, y, width, height, border, CopyFromParent, \
		InputOutput, CopyFromParent, mask, &attrs);					// Shortcut

#define SUBWINWIDTH(w)	(w->width - 2 * d->bw)					// Get real window width
#define SUBWINHEIGHT(w)	(w->height - d->th - d->bw)			// Get real window height
/* }}} */

/* Flags {{{ */
#define SUB_TYPE_CLIENT				(1L << 1)									// Client
#define SUB_TYPE_TILE					(1L << 2)									// Tile

#define SUB_TYPE_VERT					(1L << 3)									// Vert tile
#define SUB_TYPE_HORZ					(1L << 4)									// Horiz tile

#define SUB_STATE_SHADE				(1L << 5)									// Shaded window
#define SUB_STATE_FLOAT				(1L << 6)									// Floated window
#define SUB_STATE_FULL				(1L << 7)									// Fullscreen window
#define SUB_STATE_RESIZE			(1L << 8)									// Resized window
#define SUB_STATE_STACK				(1L << 9)									// Stacked tiling window
#define SUB_STATE_TRANS				(1L << 10)								// Transient window
#define SUB_STATE_DEAD				(1L << 11)								// Dead window

#define SUB_PREF_INPUT				(1L << 12)								// Active/passive focus-model
#define SUB_PREF_FOCUS				(1L << 13)								// Send focus message
#define SUB_PREF_CLOSE				(1L << 14)								// Send close message

#define SUB_DRAG_START				(1L << 1)									// Drag start
#define SUB_DRAG_ABOVE				(1L << 2)									// Drag above
#define SUB_DRAG_BELOW				(1L << 3)									// Drag below
#define SUB_DRAG_BEFORE				(1L << 4)									// Drag before
#define SUB_DRAG_AFTER				(1L << 5)									// Drag after
#define SUB_DRAG_MOVE					(1L << 6)									// Drag move
#define SUB_DRAG_SWAP					(1L << 7)									// Drag swap
#define SUB_DRAG_TOP					(1L << 8)									// Drag split horiz top
#define SUB_DRAG_BOTTOM				(1L << 9)									// Drag split horiz bottom
#define SUB_DRAG_LEFT					(1L << 10)								// Drag split vert left
#define SUB_DRAG_RIGHT				(1L << 11)								// Drag split vert right

#define SUB_KEY_FOCUS_ABOVE		(1L << 1)									// Focus above window
#define SUB_KEY_FOCUS_BELOW		(1L << 2)									// Focus below window
#define SUB_KEY_FOCUS_NEXT		(1L << 3)									// Focus next window
#define SUB_KEY_FOCUS_PREV		(1L << 4)									// Focus prev window
#define SUB_KEY_FOCUS_ANY			(1L << 5)									// Focus any window

#define SUB_KEY_DELETE_WIN		(1L << 6)									// Delete win
#define SUB_KEY_TOGGLE_SHADE	(1L << 7)									// Toggle collapse
#define SUB_KEY_TOGGLE_RAISE	(1L << 8)									// Toggle raise
#define SUB_KEY_TOGGLE_FULL		(1L << 9)									// Toggle fullscreen
#define SUB_KEY_TOGGLE_PILE		(1L << 10)								// Toggle pile

#define SUB_KEY_TOGGLE_LAYOUT	(1L << 11)								// Toggle tile layout
#define SUB_KEY_DESKTOP_NEXT	(1L << 12)								// Switch to next desktop
#define SUB_KEY_DESKTOP_PREV	(1L << 13)								// Switch to previous desktop
#define SUB_KEY_DESKTOP_MOVE	(1L << 14)								// Move window to desktop
#define SUB_KEY_EXEC					(1L << 15)								// Exec an app
/* }}} */

/* array.c {{{ */
typedef struct subarray
{
	void	**data;																					// Array data
	int		ndata;																					// Array data number
} SubArray;

SubArray *subArrayNew(void);														// Create new array
void subArrayPush(SubArray *a, void *elem);							// Push element to array
void subArrayPop(SubArray *a, void *elem);							// Pop element from array
void subArraySwap(SubArray *a, void *elem1, 
	void *elem2);																					// Swap element a and b
void subArrayKill(SubArray *a);													// Kill array
/* }}} */

/* client.c {{{ */
typedef struct subclient
{
	int							x, y, width, height, flags;						// Common properties and flags
	struct subtile 	*tile;																// Parent tile
	int							n, size;															// ?
	char						*name;																// Client name
	Colormap				cmap;																	// Colormap	
	Window					frame, caption, title, win;						// Decoration windows
	Window					left, right, bottom;									// Border windows
} SubClient;

SubClient *subClientNew(Window win);										// Create new client
void subClientDelete(SubClient *c);											// Delete client
void subClientRender(SubClient *c);											// Render client
void subClientFocus(SubClient *c);											// Focus client
void subClientResize(SubClient *c);											// Resize client
void subClientMap(SubClient *c);	
void subClientUnmap(SubClient *c);
void subClientConfigure(SubClient *c);									// Send configure request
void subClientDrag(SubClient *c, int mode);							// Move/drag client
void subClientToggle(SubClient *c, int type);						// Toggle client state
void subClientFetchName(SubClient *c);									// Fetch client name
void subClientSetWMState(SubClient *c, long state);			// Set client WM state
long subClientGetWMState(SubClient *c);									// Get client WM state
/* }}} */

/* tile.c {{{ */
typedef struct subtile
{
	int								x, y, width, height, flags;					// Common properties and flags
	struct subtile		*tile;															// Parent tile
	struct subclient	*pile;															// Pile top
	struct subarray		*children;													// Children
} SubTile;

SubTile *subTileNew(int mode);													// Create new tile
void subTilePush(SubTile *t, void *c);									// Push children to tile
void subTilePop(SubTile *t, void *c);										// Pop children from tile
void subTileKill(SubTile *t);														// Kill tile
void subTileConfigure(SubTile *t);											// Configure tile
/* }}} */

/* rule.c {{{ */
typedef struct subrule
{
	int							size;																	// Rule size
	regex_t					*regex;																// Rule regex
	struct subtile	*tile;																// Rule tile
} SubRule;

SubRule *subRuleNew(char *tags, int size);							// Create rule
void subRuleDelete(SubRule *r);													// Delete rule

/* }}} */

/* view.c {{{ */
typedef struct subview
{
	int							width, xid;														// View button width, view xid
	Window					frame, button;												// View frame, button
	char 						*name;																// View name
	struct subtile	*tile;																// View tile
	struct subarray *rules;																// View rules
} SubView;

SubView *subViewFind(int xid);													// Find view
SubView *subViewNew(char *name);												// Create new view
void subViewDelete(SubView *v);													// Delete view
void subViewDestroy(SubView *v);												// Destroy view
void subViewMerge(Window win);													// Merge window
void subViewRender(void);																// Render views
void subViewConfigure(void);														// Configure views
void subViewSwitch(SubView *v);													// Switch view
void subViewKill(void);																	// Kill views

/* }}} */

/* sublet.c {{{ */
#define SUB_SUBLET_TYPE_TEXT		(1L << 1)								// Text sublet
#define SUB_SUBLET_TYPE_TEASER	(1L << 2)								// Teaser sublet
#define SUB_SUBLET_TYPE_METER		(1L << 3)								// Meter sublet
#define SUB_SUBLET_TYPE_WATCH		(1L << 4)								// Watch sublet
#define SUB_SUBLET_TYPE_HELPER	(1L << 5)								// Helper sublet

#define SUB_SUBLET_FAIL_FIRST		(1L << 6)								// Fail first time
#define SUB_SUBLET_FAIL_SECOND	(1L << 7)								// Fail second time
#define SUB_SUBLET_FAIL_THIRD		(1L << 8)								// Fail third time

typedef struct subsublet
{
	int			flags, ref;																		// Flags, Lua object reference, width
	time_t	time, interval;																// Last update time, interval time

	struct subsublet *next;																// Next sibling

	union 
	{
		char *string;																				// String data
		int number;																					// Number data
	};
} SubSublet;

void subSubletNew(int type, char *name, int ref, 				// Create new sublet
	time_t interval, char *watch);
void subSubletDelete(SubSublet *s);											// Delete sublet
void subSubletDestroy(SubSublet *s);										// Destroy sublet
void subSubletRender(void);															// Render sublet
void subSubletConfigure(void);													// Configure sublet bar
void subSubletMerge(int pos);														// Merge sublet
SubSublet *subSubletNext(void);													// Get next sublet
void subSubletKill(void);																// Delete sublets
/* }}} */

/* display.c {{{ */
typedef struct subdisplay
{
	Display						*disp;															// Connection to X server
	int								th, bw, fx, fy;											// Tab height, border widthm font metrics
	XFontStruct				*xfs;																// Font

	struct subclient	*focus;															// Focus window
	struct subview		*cv;																// Current view
	
	struct subarray		*views;															// View list
	struct subarray		*clients;														// Client list
	struct subarray		*sublets;														// Sublet list

#ifdef HAVE_SYS_INOTIFY_H
	int								notify;															// Inotify descriptor
#endif

	struct
	{
		Window					win, views, sublets;								// Bar windows
	} bar;

	struct
	{
		unsigned long		font, border, norm, focus, 
										cover, bg;													// Colors
	} colors;
	struct
	{
		GC							font, border, invert;								// Graphic contexts (GCs)
	} gcs;
	struct
	{
		Cursor					square, move, arrow, horz, vert, 
										resize;															// Cursors
	} cursors;
} SubDisplay;

extern SubDisplay *d;

void subDisplayNew(const char *display_string);					// Create new display
void subDisplayKill(void);															// Delete display
void subDisplayScan(void);															// Scan root window
/* }}} */

/* key.c {{{ */
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

void subKeyInit(void);																	// Init the keys
SubKey *subKeyFind(int keycode, unsigned int mod);			// Find a key
void subKeyNew(const char *key,
	const char *value);																		// Create new key
void subKeyGrab(SubClient *c);													// Grab keys for a window
void subKeyUngrab(SubClient *c);												// Ungrab keys for a window
void subKeyKill(void);																	// Delete all keys
/* }}} */

/* lua.c {{{ */
void subLuaLoadConfig(const char *path);								// Load config file
void subLuaLoadSublets(const char *path);								// Load sublets
void subLuaKill(void);																	// Kill Lua state
void subLuaCall(SubSublet *s);													// Call Lua script
/* }}} */

/* event.c {{{ */
time_t subEventGetTime(void);														// Get the current time
int subEventLoop(void);																	// Event loop
/* }}} */

/* util.c {{{ */
#ifdef DEBUG
void subUtilLogSetDebug(void);
#define subUtilLogDebug(...)	subUtilLog(0, __FILE__, __LINE__, __VA_ARGS__);
#else
#define subUtilLogDebug(...)
#endif /* DEBUG */

#define subUtilLogError(...)	subUtilLog(1, __FILE__, __LINE__, __VA_ARGS__);
#define subUtilLogWarn(...)		subUtilLog(2, __FILE__, __LINE__, __VA_ARGS__);

void subUtilLog(int type, const char *file,
	int line, const char *format, ...);										// Print messages
void *subUtilRealloc(void *mem, size_t size);						// Reallocate memory
void *subUtilAlloc(size_t n, size_t size);							// Allocate memory
XPointer *subUtilFind(Window win, XContext id);					// Find window data
/* }}} */

/* ewmh.c  {{{ */
enum SubEwmhHints
{
	/* ICCCM */
	SUB_EWMH_WM_NAME,																			// Name of window
	SUB_EWMH_WM_CLASS,																		// Class of window
	SUB_EWMH_WM_STATE,																		// Window state
	SUB_EWMH_WM_PROTOCOLS,																// Supported protocols 
	SUB_EWMH_WM_TAKE_FOCUS,																// Send focus messages
	SUB_EWMH_WM_DELETE_WINDOW,														// Send close messages
	SUB_EWMH_WM_NORMAL_HINTS,															// Window normal hints
	SUB_EWMH_WM_SIZE_HINTS,																// Window size hints

	/* EWMH */
	SUB_EWMH_NET_SUPPORTED,																// Supported states
	SUB_EWMH_NET_CLIENT_LIST,															// List of clients
	SUB_EWMH_NET_CLIENT_LIST_STACKING,										// List of clients
	SUB_EWMH_NET_NUMBER_OF_DESKTOPS,											// Total number of views
	SUB_EWMH_NET_DESKTOP_NAMES,														// Names of the views
	SUB_EWMH_NET_DESKTOP_GEOMETRY,												// Desktop geometry
	SUB_EWMH_NET_DESKTOP_VIEWPORT,												// Viewport of the view
	SUB_EWMH_NET_CURRENT_DESKTOP,													// Number of current view
	SUB_EWMH_NET_ACTIVE_WINDOW,														// Focus window
	SUB_EWMH_NET_WORKAREA,																// Workarea of the views
	SUB_EWMH_NET_SUPPORTING_WM_CHECK,											// Check for compliant window manager
	SUB_EWMH_NET_VIRTUAL_ROOTS,														// List of virtual destops
	SUB_EWMH_NET_CLOSE_WINDOW,

	SUB_EWMH_NET_WM_PID,																	// PID of client
	SUB_EWMH_NET_WM_DESKTOP,															// Desktop client is on

	SUB_EWMH_NET_WM_STATE,																// Window state
	SUB_EWMH_NET_WM_STATE_MODAL,													// Modal window
	SUB_EWMH_NET_WM_STATE_SHADED,													// Shaded window
	SUB_EWMH_NET_WM_STATE_HIDDEN,													// Hidden window
	SUB_EWMH_NET_WM_STATE_FULLSCREEN,											// Fullscreen window

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

char * subEwmhGetProperty(Window win, 
	Atom type, int hint, unsigned long *size);						// Get property
void subEwmhSetWindows(Window win, int hint, 
	Window *values, int size);														// Set window properties
void subEwmhSetCardinals(Window win, int hint,
	long *values, int size);															// Set cardinal properties
void subEwmhSetString(Window win, int hint, 
	char *value);																					// Set string property
void subEwmhSetStrings(Window win, int hint,						// Set string properties
	char **values, int size);
/* }}} */

#endif /* SUBTLE_H */
