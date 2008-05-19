
 /**
	* @package subtle
	*
	* @file Header file
	* @copyright Copyright (c) 2005-2008 Christoph Kappel
	* @version $Id$
	*
	* See the COPYING file for the license in the latest tarball.
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
#define WINNEW(parent,x,y,width,height,border,mask) \
	XCreateWindow(d->disp, parent, x, y, width, height, border, CopyFromParent, \
		InputOutput, CopyFromParent, mask, &attrs);					///< Shortcut

#define WINWIDTH(w)		(w->width - 2 * d->bw)						///< Get real window width
#define WINHEIGHT(w)	(w->height - d->th - d->bw)				///< Get real window height

#define ARRAY(a)	((SubArray *)a)												///< Cast to SubArray
#define VIEW(v)		((SubView *)v)												///< Cast to SubView
#define TILE(t)		((SubTile *)t)												///< Cast to SubTile
#define CLIENT(c)	((SubClient *)c)											///< Cast to SubClient
#define RULE(r)		((SubRule *)r)												///< Cast to SubRule
#define SUBLET(s)	((SubSublet *)s)											///< Cast to SubSublet
#define KEY(k)		((SubKey *)k)													///< Cast to SubKey
/* }}} */

/* Flags {{{ */
#define SUB_TYPE_CLIENT					(1L << 1)								///< Client
#define SUB_TYPE_TILE						(1L << 2)								///< Tile
#define SUB_TYPE_VERT						(1L << 3)								///< Vert tile
#define SUB_TYPE_HORZ						(1L << 4)								///< Horiz tile
#define SUB_TYPE_VIEW						(1L << 5)								///< View
#define SUB_TYPE_SUBLET					(1L << 6)								///< Sublet
#define SUB_TYPE_KEY						(1L << 7)								///< Key
#define SUB_TYPE_RULE						(1L << 8)								///< Rule

#define SUB_STATE_SHADE					(1L << 9)								///< Shaded window
#define SUB_STATE_FLOAT					(1L << 10)							///< Floated window
#define SUB_STATE_FULL					(1L << 11)							///< Fullscreen window
#define SUB_STATE_RESIZE				(1L << 12)							///< Resized window
#define SUB_STATE_STACK					(1L << 13)							///< Stacked tiling window
#define SUB_STATE_TRANS					(1L << 14)							///< Transient window
#define SUB_STATE_DEAD					(1L << 15)							///< Dead window
#define SUB_STATE_MULTI					(1L << 16)							///< Multi tagged

#define SUB_PREF_INPUT					(1L << 17)							///< Active/passive focus-model
#define SUB_PREF_FOCUS					(1L << 18)							///< Send focus message
#define SUB_PREF_CLOSE					(1L << 19)							///< Send close message

#define SUB_DRAG_START					(1L << 1)								///< Drag start
#define SUB_DRAG_ABOVE					(1L << 2)								///< Drag above
#define SUB_DRAG_BELOW					(1L << 3)								///< Drag below
#define SUB_DRAG_BEFORE					(1L << 4)								///< Drag before
#define SUB_DRAG_AFTER					(1L << 5)								///< Drag after
#define SUB_DRAG_LEFT						(1L << 6)								///< Drag left
#define SUB_DRAG_RIGHT					(1L << 7)								///< Drag right
#define SUB_DRAG_TOP						(1L << 8)								///< Drag top
#define SUB_DRAG_BOTTOM					(1L << 9)								///< Drag bottom
#define SUB_DRAG_MOVE						(1L << 10)							///< Drag move
#define SUB_DRAG_SWAP						(1L << 11)							///< Drag swap

#define SUB_KEY_FOCUS_ABOVE			(1L << 9)								///< Focus above window
#define SUB_KEY_FOCUS_BELOW			(1L << 10)							///< Focus below window
#define SUB_KEY_FOCUS_NEXT			(1L << 11)							///< Focus next window
#define SUB_KEY_FOCUS_PREV			(1L << 12)							///< Focus prev window
#define SUB_KEY_FOCUS_ANY				(1L << 13)							///< Focus any window
#define SUB_KEY_DELETE_WIN			(1L << 14)							///< Delete win
#define SUB_KEY_TOGGLE_SHADE		(1L << 15)							///< Toggle collapse
#define SUB_KEY_TOGGLE_RAISE		(1L << 16)							///< Toggle raise
#define SUB_KEY_TOGGLE_FULL			(1L << 17)							///< Toggle fullscreen
#define SUB_KEY_TOGGLE_PILE			(1L << 18)							///< Toggle pile
#define SUB_KEY_TOGGLE_LAYOUT		(1L << 19)							///< Toggle tile layout
#define SUB_KEY_DESKTOP_NEXT		(1L << 20)							///< Switch to next desktop
#define SUB_KEY_DESKTOP_PREV		(1L << 21)							///< Switch to previous desktop
#define SUB_KEY_DESKTOP_MOVE		(1L << 22)							///< Move window to desktop
#define SUB_KEY_EXEC						(1L << 23)							///< Exec an app

#define SUB_SUBLET_TYPE_TEXT		(1L << 9)								///< Text sublet
#define SUB_SUBLET_TYPE_TEASER	(1L << 10)							///< Teaser sublet
#define SUB_SUBLET_TYPE_METER		(1L << 11)							///< Meter sublet
#define SUB_SUBLET_TYPE_WATCH		(1L << 12)							///< Watch sublet
#define SUB_SUBLET_TYPE_HELPER	(1L << 13)							///< Helper sublet

#define SUB_SUBLET_FAIL_FIRST		(1L << 14)							///< Fail first time
#define SUB_SUBLET_FAIL_SECOND	(1L << 15)							///< Fail second time
#define SUB_SUBLET_FAIL_THIRD		(1L << 16)							///< Fail third time
/* }}} */

/* array.c {{{ */
typedef struct subarray
{
	int		ndata;																					///< Array data count
	void	**data;																					///< Array data
} SubArray;

SubArray *subArrayNew(void);														///< Create new array
void subArrayPush(SubArray *a, void *e);								///< Push element to array
void subArrayPop(SubArray *a, void *e);									///< Pop element from array
int subArrayFind(SubArray *a, void *e);									///< Find array id of element
void subArraySplice(SubArray *a, int idx, int len);			///< Splice array at idx with len
void subArraySort(SubArray *a,													///< Sort array with given compare function 
	int(*compar)(const void *a, const void *b));
void subArrayKill(SubArray *a, int clean);							///< Kill array with all elements
/* }}} */

/* client.c {{{ */
typedef struct subclient
{
	int								flags, x, y, width, height, size;		///< Client flags and common properties
	struct subtile 		*tile;															///< Client parent
	char							*name;															///< Client name
	Colormap					cmap;																///< Client colormap	
	Window						frame, caption, title, win;					///< Client decoration windows
	Window						left, right, bottom;								///< Client border windows
	struct subarray		*multi;															///< Client multi
} SubClient;

SubClient *subClientNew(Window win);										///< Create new client
void subClientConfigure(SubClient *c);									///< Send configure request
void subClientRender(SubClient *c);											///< Render client
void subClientFocus(SubClient *c);											///< Focus client
void subClientMap(SubClient *c);												///< Map client	
void subClientUnmap(SubClient *c);											///< Unmap client
void subClientDrag(SubClient *c, int mode);							///< Move/drag client
void subClientToggle(SubClient *c, int type);						///< Toggle client state
void subClientFetchName(SubClient *c);									///< Fetch client name
void subClientSetWMState(SubClient *c, long state);			///< Set client WM state
long subClientGetWMState(SubClient *c);									///< Get client WM state
void subClientKill(SubClient *c);												///< Kill client
/* }}} */

/* tile.c {{{ */
typedef struct subtile
{
	int								flags, x, y, width, height, size;		///< Tile flags and common properties
	struct subtile		*tile;															///< Tile parent
	struct subclient	*top;																///< Tile stack top
	struct subarray		*clients;														///< Tile clients
	void 							*superior;													///< Tile superior
} SubTile;

SubTile *subTileNew(int mode, void *sup);								///< Create new tile
void subTileConfigure(SubTile *t);											///< Configure tile
void subTileRemap(SubTile *t);													///< Remap tile
void subTileKill(SubTile *t, int clean);								///< Kill tile
/* }}} */

/* rule.c {{{ */
typedef struct subrule
{
	int							flags, size;													///< Rule flags, size
	regex_t					*regex;																///< Rule regex
	struct subtile	*tile;																///< Rule tile
} SubRule;

SubRule *subRuleNew(char *tags, int size);							///< Create rule
void subRuleKill(SubRule *r, int clean);								///< Delete rule

/* }}} */

/* view.c {{{ */
typedef struct subview
{
	int							flags, width;													///< View flags and button width
	Window					frame, button;												///< View frame, button
	char 						*name;																///< View name
	struct subtile	*tile;																///< View tile
	struct subarray *rules;																///< View rules
} SubView;

SubView *subViewNew(char *name);												///< Create new view
void subViewConfigure(void);														///< Configure views
void subViewRender(void);																///< Render views
void subViewMerge(Window win);													///< Merge window
void subViewJump(SubView *v);														///< Jump to view
void subViewKill(SubView *v);														///< Kill view
/* }}} */

/* sublet.c {{{ */
typedef struct subsublet
{
	int			flags, ref, width;														///< Sublet flags, object reference, width
	time_t	time, interval;																///< Sublet update time, interval time

	struct subsublet *next;																///< Sublet next sibling

	union 
	{
		char *string;																				///< Sublet data string
		int number;																					///< Sublet data number
	};
} SubSublet;

SubSublet *subSubletNew(int type, char *name, int ref, 	///< Create new sublet
	time_t interval, char *watch);
void subSubletConfigure(void);													///< Configure sublet bar
void subSubletRender(void);															///< Render sublet
int subSubletCompare(const void *a, const void *b);			///< Compare two sublets
void subSubletKill(SubSublet *s);												///< Kill sublet
/* }}} */

/* display.c {{{ */
typedef struct subdisplay
{
	Display						*disp;															///< Connection to X server
	int								th, bw, fx, fy;											///< Tab height, border widthm font metrics
	XFontStruct				*xfs;																///< Font

	struct subsublet	*sublet;														///< First sublet
	struct subclient	*focus;															///< Focus window
	struct subview		*cv;																///< Current view
	
	struct subarray		*views;															///< View list
	struct subarray		*clients;														///< Client list
	struct subarray		*sublets;														///< Sublet list
	struct subarray		*keys;															///< Key list

#ifdef HAVE_SYS_INOTIFY_H
	int								notify;															///< Inotify descriptor
#endif

	struct
	{
		Window					win, views, sublets;								///< Bar windows
	} bar;

	struct
	{
		unsigned long		font, border, norm, focus, 
										cover, bg;													///< Colors
	} colors;
	struct
	{
		GC							font, border, invert;								///< Graphic contexts
	} gcs;
	struct
	{
		Cursor					square, move, arrow, horz, vert, 
										resize;															///< Cursors
	} cursors;
} SubDisplay;

extern SubDisplay *d;

SubDisplay *subDisplayNew(const char *display_string);	///< Create new display
void subDisplayKill(void);															///< Delete display
void subDisplayScan(void);															///< Scan root window
/* }}} */

/* key.c {{{ */
typedef struct subkey
{
	int flags, code;																			///< Key flags, code
	unsigned int mod;																			///< Key modifier

	union
	{
		char *string;																				///< Key data string
		int number;																					///< Key data number
	};
} SubKey;

void subKeyInit(void);																	///< Init the keys
SubKey *subKeyNew(const char *key,											///< Create new key
	const char *value);
SubKey *subKeyFind(int code, unsigned int mod);					///< Find key
void subKeyGrab(Window win);														///< Grab keys for window
void subKeyUngrab(Window win);													///< Ungrab keys for window
int subKeyCompare(const void *a, const void *b);				///< Compare two keys
void subKeyKill(SubKey *k);															///< Kill key
/* }}} */

/* lua.c {{{ */
void subLuaLoadConfig(const char *path);								///< Load config file
void subLuaLoadSublets(const char *path);								///< Load sublets
void subLuaKill(void);																	///< Kill Lua state
void subLuaCall(SubSublet *s);													///< Call Lua script
/* }}} */

/* event.c {{{ */
void subEventLoop(void);																	///< Event loop
/* }}} */

/* util.c {{{ */
#ifdef DEBUG
void subUtilLogSetDebug(void);
#define subUtilLogDebug(...)	subUtilLog(0, __FILE__, __LINE__, __VA_ARGS__);
#else
#define subUtilLogDebug(...)
#endif /* DEBUG */

#define subUtilLogError(...)	subUtilLog(1, __FILE__, __LINE__,  __VA_ARGS__);
#define subUtilLogWarn(...)		subUtilLog(2, __FILE__, __LINE__, __VA_ARGS__);

void subUtilLog(int type, const char *file,
	int line, const char *format, ...);										///< Print messages
int subUtilLogXError(Display *disp, XErrorEvent *ev);		///< Print X error messages
void *subUtilAlloc(size_t n, size_t size);							///< Allocate memory
void *subUtilRealloc(void *mem, size_t size);						///< Reallocate memory
XPointer *subUtilFind(Window win, XContext id);					///< Find window data
time_t subUtilTime(void);																///< Get the current time
/* }}} */

/* ewmh.c  {{{ */
enum SubEwmhHints
{
	/* ICCCM */
	SUB_EWMH_WM_NAME,																			///< Name of window
	SUB_EWMH_WM_CLASS,																		///< Class of window
	SUB_EWMH_WM_STATE,																		///< Window state
	SUB_EWMH_WM_PROTOCOLS,																///< Supported protocols 
	SUB_EWMH_WM_TAKE_FOCUS,																///< Send focus messages
	SUB_EWMH_WM_DELETE_WINDOW,														///< Send close messages
	SUB_EWMH_WM_NORMAL_HINTS,															///< Window normal hints
	SUB_EWMH_WM_SIZE_HINTS,																///< Window size hints

	/* EWMH */
	SUB_EWMH_NET_SUPPORTED,																///< Supported states
	SUB_EWMH_NET_CLIENT_LIST,															///< List of clients
	SUB_EWMH_NET_CLIENT_LIST_STACKING,										///< List of clients
	SUB_EWMH_NET_NUMBER_OF_DESKTOPS,											///< Total number of views
	SUB_EWMH_NET_DESKTOP_NAMES,														///< Names of the views
	SUB_EWMH_NET_DESKTOP_GEOMETRY,												///< Desktop geometry
	SUB_EWMH_NET_DESKTOP_VIEWPORT,												///< Viewport of the view
	SUB_EWMH_NET_CURRENT_DESKTOP,													///< Number of current view
	SUB_EWMH_NET_ACTIVE_WINDOW,														///< Focus window
	SUB_EWMH_NET_WORKAREA,																///< Workarea of the views
	SUB_EWMH_NET_SUPPORTING_WM_CHECK,											///< Check for compliant window manager
	SUB_EWMH_NET_VIRTUAL_ROOTS,														///< List of virtual destops
	SUB_EWMH_NET_CLOSE_WINDOW,

	SUB_EWMH_NET_WM_PID,																	///< PID of client
	SUB_EWMH_NET_WM_DESKTOP,															///< Desktop client is on

	SUB_EWMH_NET_WM_STATE,																///< Window state
	SUB_EWMH_NET_WM_STATE_MODAL,													///< Modal window
	SUB_EWMH_NET_WM_STATE_SHADED,													///< Shaded window
	SUB_EWMH_NET_WM_STATE_HIDDEN,													///< Hidden window
	SUB_EWMH_NET_WM_STATE_FULLSCREEN,											///< Fullscreen window

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

void subEwmhInit(void);																	///< Init atoms/hints
Atom subEwmhFind(int hint);															///< Find atom

char * subEwmhGetProperty(Window win, 
	Atom type, int hint, unsigned long *size);						///< Get property
void subEwmhSetWindows(Window win, int hint, 
	Window *values, int size);														///< Set window properties
void subEwmhSetCardinals(Window win, int hint,
	long *values, int size);															///< Set cardinal properties
void subEwmhSetString(Window win, int hint, 
	char *value);																					///< Set string property
void subEwmhSetStrings(Window win, int hint,						///< Set string properties
	char **values, int size);
/* }}} */

#endif /* SUBTLE_H */
