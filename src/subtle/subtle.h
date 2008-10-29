
 /**
  * @package subtle
  *
  * @file Header file
  * @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#ifndef SUBTLE_H
#define SUBTLE_H 1

/* Includes {{{ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <regex.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>

#include "config.h"

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif /* HAVE_SYS_INOTIFY */
/* }}} */

/* Macros {{{ */
#define FLAGS     int                                             ///< Flags
#define TAGS      int                                             ///< Tags

#define CLIENTID  1                                               ///< Client data id
#define VIEWID    2                                               ///< View data id
#define BUTTONID  1                                               ///< Button data id
#define MINW      50                                              ///< Client min. width
#define MINH      50                                              ///< Client min. height
#define SNAP      10                                              ///< Snapping threshold

#define TEXTW(s)  (strlen(s) * subtle->fx + 8)                    ///< Textwidth in pixel
#define WINW(c)   (c->rect.width - 2 * subtle->bw)                ///< Get real width
#define WINH(c)   (c->rect.height - 2 * subtle->bw)               ///< Get real height
#define ROOT      DefaultRootWindow(subtle->disp)                 ///< Root window

#define SCREENW \
  DisplayWidth(subtle->disp, DefaultScreen(subtle->disp))         ///< Get screen width
#define SCREENH \
  DisplayHeight(subtle->disp, DefaultScreen(subtle->disp))        ///< Get screen height
#define BETWEEN(val,min,max) \
  (min && val < min ? min : max && val > max ? max : val)         ///< Value min/max

/* Casts */
#define ARRAY(a)  ((SubArray *)a)                                 ///< Cast to SubArray
#define CLIENT(c) ((SubClient *)c)                                ///< Cast to SubClient
#define GRAB(g)   ((SubGrab *)g)                                  ///< Cast to SubGrab
#define LAYOUT(l) ((SubLayout *)l)                                ///< Cast to SubLayout
#define RECT(r)   ((XRectangle *)r)                               ///< Cast to XRectangle
#define SUBLET(s) ((SubSublet *)s)                                ///< Cast to SubSublet
#define SUBTLE(s) ((SubSubtle *)s)                                ///< Cast to SubSubtle
#define TAG(t)    ((SubTag *)t)                                   ///< Cast to SubTag
#define VIEW(v)   ((SubView *)v)                                  ///< Cast to SubView

/* ICCCM */
#define SUB_EWMH_WM_NAME                       0                  ///< Name of window
#define SUB_EWMH_WM_CLASS                      1                  ///< Class of window
#define SUB_EWMH_WM_STATE                      2                  ///< Window state
#define SUB_EWMH_WM_PROTOCOLS                  3                  ///< Supported protocols 
#define SUB_EWMH_WM_TAKE_FOCUS                 4                  ///< Send focus messages
#define SUB_EWMH_WM_DELETE_WINDOW              5                  ///< Send close messages
#define SUB_EWMH_WM_NORMAL_HINTS               6                  ///< Window normal hints
#define SUB_EWMH_WM_SIZE_HINTS                 7                  ///< Window size hints

/* EWMH */
#define SUB_EWMH_NET_SUPPORTED                 8                  ///< Supported states
#define SUB_EWMH_NET_CLIENT_LIST               9                  ///< List of clients
#define SUB_EWMH_NET_CLIENT_LIST_STACKING     10                  ///< List of clients
#define SUB_EWMH_NET_NUMBER_OF_DESKTOPS       11                  ///< Total number of views
#define SUB_EWMH_NET_DESKTOP_NAMES            12                  ///< Names of the views
#define SUB_EWMH_NET_DESKTOP_GEOMETRY         13                  ///< Desktop geometry
#define SUB_EWMH_NET_DESKTOP_VIEWPORT         14                  ///< Viewport of the view
#define SUB_EWMH_NET_CURRENT_DESKTOP          15                  ///< Number of current view
#define SUB_EWMH_NET_ACTIVE_WINDOW            16                  ///< Focus window
#define SUB_EWMH_NET_WORKAREA                 17                  ///< Workarea of the views
#define SUB_EWMH_NET_SUPPORTING_WM_CHECK      18                  ///< Check for compliant window manager
#define SUB_EWMH_NET_VIRTUAL_ROOTS            19                  ///< List of virtual destops
#define SUB_EWMH_NET_CLOSE_WINDOW             20

#define SUB_EWMH_NET_WM_NAME                  21
#define SUB_EWMH_NET_WM_PID                   22                  ///< PID of client
#define SUB_EWMH_NET_WM_DESKTOP               23                  ///< Desktop client is on
#define SUB_EWMH_NET_SHOWING_DESKTOP          24                  ///< Showing desktop mode 

#define SUB_EWMH_NET_WM_STATE                 25                  ///< Window state
#define SUB_EWMH_NET_WM_STATE_MODAL           26                  ///< Modal window
#define SUB_EWMH_NET_WM_STATE_HIDDEN          27                  ///< Hidden window
#define SUB_EWMH_NET_WM_STATE_FULLSCREEN      28                  ///< Fullscreen window

#define SUB_EWMH_NET_WM_WINDOW_TYPE           29
#define SUB_EWMH_NET_WM_WINDOW_TYPE_DESKTOP   30
#define SUB_EWMH_NET_WM_WINDOW_TYPE_NORMAL    31
#define SUB_EWMH_NET_WM_WINDOW_TYPE_DIALOG    32

#define SUB_EWMH_NET_WM_ALLOWED_ACTIONS       33
#define SUB_EWMH_NET_WM_ACTION_MOVE           34
#define SUB_EWMH_NET_WM_ACTION_RESIZE         35
#define SUB_EWMH_NET_WM_ACTION_FULLSCREEN     36
#define SUB_EWMH_NET_WM_ACTION_CHANGE_DESKTOP 37
#define SUB_EWMH_NET_WM_ACTION_CLOSE          38

/* Misc */
#define SUB_EWMH_UTF8                         39                  ///< String encoding

/* subtle */
#define SUB_EWMH_SUBTLE_CLIENT_TAG            40                  ///< subtle client tag
#define SUB_EWMH_SUBTLE_CLIENT_UNTAG          41                  ///< subtle client untag
#define SUB_EWMH_SUBTLE_CLIENT_TAGS           42                  ///< subtle client tags
#define SUB_EWMH_SUBTLE_TAG_NEW               43                  ///< subtle tag new
#define SUB_EWMH_SUBTLE_TAG_KILL              44                  ///< subtle tag kill
#define SUB_EWMH_SUBTLE_TAG_LIST              45                  ///< subtle tag list
#define SUB_EWMH_SUBTLE_VIEW_NEW              46                  ///< subtle view new
#define SUB_EWMH_SUBTLE_VIEW_KILL             47                  ///< subtle view kill
#define SUB_EWMH_SUBTLE_VIEW_LIST             48                  ///< subtle view list
#define SUB_EWMH_SUBTLE_VIEW_TAG              49                  ///< subtle view tag
#define SUB_EWMH_SUBTLE_VIEW_UNTAG            50                  ///< subtle view untag
#define SUB_EWMH_SUBTLE_VIEW_TAGS             51                  ///< subtle view tags
/* }}} */

/* Flags {{{ */
/* Data types */
#define SUB_TYPE_CLIENT        (1L << 1)                          ///< Client
#define SUB_TYPE_GRAB          (1L << 2)                          ///< Grab
#define SUB_TYPE_LAYOUT        (1L << 3)                          ///< Layout
#define SUB_TYPE_SUBLET        (1L << 4)                          ///< Sublet
#define SUB_TYPE_TAG           (1L << 5)                          ///< Tag
#define SUB_TYPE_VIEW          (1L << 6)                          ///< View

/* Tile modes */
#define SUB_TILE_VERT          (1L << 7)                          ///< Tile vert
#define SUB_TILE_HORZ          (1L << 8)                          ///< Tile horz
#define SUB_TILE_SWAP          (1L << 9)                          ///< Tile swap

/* Data types */
#define SUB_DATA_STRING        (1L << 10)                         ///< String data
#define SUB_DATA_FIXNUM        (1L << 11)                         ///< Fixnum data
#define SUB_DATA_NIL           (1L << 12)                         ///< Nil data

/* Client states */
#define SUB_STATE_FLOAT        (1L << 13)                         ///< Floating window
#define SUB_STATE_FULL         (1L << 14)                         ///< Fullscreen window
#define SUB_STATE_RESIZE       (1L << 15)                         ///< Resized window
#define SUB_STATE_DEAD         (1L << 16)                         ///< Dead window
#define SUB_STATE_TILED        (1L << 17)                         ///< Tiled client
#define SUB_STATE_URGENT       (1L << 18)                         ///< Urgent client

/* Client preferences */
#define SUB_PREF_INPUT         (1L << 20)                         ///< Active/passive focus-model
#define SUB_PREF_FOCUS         (1L << 21)                         ///< Send focus message
#define SUB_PREF_CLOSE         (1L << 22)                         ///< Send close message
#define SUB_PREF_HINTS         (1L << 23)                         ///< Size hints available

/* Drag states */
#define SUB_DRAG_START         (1L << 1)                          ///< Drag start
#define SUB_DRAG_ABOVE         (1L << 2)                          ///< Drag above
#define SUB_DRAG_BELOW         (1L << 3)                          ///< Drag below
#define SUB_DRAG_BEFORE        (1L << 4)                          ///< Drag before
#define SUB_DRAG_AFTER         (1L << 5)                          ///< Drag after
#define SUB_DRAG_LEFT          (1L << 6)                          ///< Drag left
#define SUB_DRAG_RIGHT         (1L << 7)                          ///< Drag right
#define SUB_DRAG_TOP           (1L << 8)                          ///< Drag top
#define SUB_DRAG_BOTTOM        (1L << 9)                          ///< Drag bottom
#define SUB_DRAG_MOVE          (1L << 10)                         ///< Drag move
#define SUB_DRAG_SWAP          (1L << 11)                         ///< Drag swap
#define SUB_DRAG_RESIZE        (1L << 12)                         ///< Drag resize

/* Grabs */
#define SUB_GRAB_KEY           (1L << 10)                         ///< Key grab
#define SUB_GRAB_MOUSE         (1L << 11)                         ///< Mouse grab  
#define SUB_GRAB_VIEW_JUMP     (1L << 12)                         ///< Jump to view
#define SUB_GRAB_EXEC          (1L << 13)                         ///< Exec an app
#define SUB_GRAB_WINDOW_MOVE   (1L << 14)                         ///< Resize window
#define SUB_GRAB_WINDOW_RESIZE (1L << 15)                         ///< Move window
#define SUB_GRAB_WINDOW_FLOAT  (1L << 16)                         ///< Toggle float
#define SUB_GRAB_WINDOW_FULL   (1L << 17)                         ///< Toggle full
#define SUB_GRAB_WINDOW_KILL   (1L << 18)                         ///< Kill window

/* Fixed tags */
#define SUB_TAG_DEFAULT        (1L << 1)                          ///< Default tag
#define SUB_TAG_FLOAT          (1L << 2)                          ///< Float tag
#define SUB_TAG_FULL           (1L << 3)                          ///< Full tag
/* }}} */

/* Typedefs {{{ */
typedef struct subarray_t /* {{{ */
{
  int    ndata;                                                   ///< Array data count
  void  **data;                                                   ///< Array data
} SubArray; /* }}} */

typedef struct subclient_t /* {{{ */
{
  FLAGS               flags;                                      ///< Client flags
  TAGS                tags;                                       ///< Client tags
  int                 size;                                       ///< Client size, tags
  char                *name;                                      ///< Client name
  Colormap            cmap;                                       ///< Client colormap
  Window              win;                                        ///< Client window
  XRectangle          rect;                                       ///< Client rect
  XSizeHints          *hints;                                     ///< Client size hints
} SubClient; /* }}} */

typedef struct subgrab_t /* {{{ */
{
  FLAGS        flags;                                             ///< Grab flags
  unsigned int code, mod;                                         ///< Grab coden modifier

  union
  {
    char       *string;                                           ///< Grab data string
    int        number;                                            ///< Grab data number
  };
} SubGrab; /* }}} */

typedef struct sublayout_t /* {{{ */
{
  FLAGS  flags;                                                   ///< Layout flags
  struct subclient_t *c1;                                         ///< Layout client1
  struct subclient_t *c2;                                         ///< Layout client2
} SubLayout; /* }}} */

typedef struct subsublet_t /* {{{ */
{
  FLAGS         flags;                                            ///< Sublet flags
  unsigned long recv;                                             ///< Sublet ruby receiver
  int           width;                                            ///< Sublet width
  time_t        time, interval;                                   ///< Sublet update time, interval time

  struct subsublet_t *next;                                       ///< Sublet next sibling

  union 
  {
    char *string;                                                 ///< Sublet data string
    int  fixnum;                                                  ///< Sublet data fixnum
  };
} SubSublet; /* }}} */

typedef struct subsubtle_t /* {{{ */
{
  int                th, bw, fx, fy, step;                        ///< Subtle tab height, border width, font metrics, step

  Display            *disp;                                       ///< Subtle Xorg display
  Window             focus;                                       ///< Subtle focus
  XFontStruct        *xfs;                                        ///< Subtle font

  struct subview_t   *cv;                                         ///< Subtle current view
  struct subsublet_t *sublet;                                     ///< Subtle first sublet
  struct subarray_t  *grabs;                                      ///< Subtle grabs
  struct subarray_t  *tags;                                       ///< Subtle tags
  struct subarray_t  *views;                                      ///< Subtle views
  struct subarray_t  *clients;                                    ///< Subtle clients
  struct subarray_t  *sublets;                                    ///< Subtle sublets

#ifdef DEBUG
  int                debug;                                       ///< Subtle debug
#endif /* DEBUG */

#ifdef HAVE_SYS_INOTIFY_H
  int                notify;                                      ///< Subtle inotify descriptor
#endif /* HAVE_SYS_INOTIFY_H */

  struct
  {
    Window           win, views, caption, sublets; 
  } bar;                                                          ///< Subtle bar windows

  struct
  {
    unsigned long    font, border, norm, focus, bg;                            
  } colors;                                                       ///< Subtle colors

  struct
  {
    GC               font, border, invert;                  
  } gcs;                                                          ///< Subtle graphic contexts

  struct
  {
    Cursor           arrow, move, resize;                                
  } cursors;                                                      ///< Subtle cursors
} SubSubtle; /* }}} */

typedef struct subtag_t /* {{{ */
{
  FLAGS    flags;                                                 ///< Tag flags
  char    *name;                                                  ///< Tag name
  regex_t *preg;                                                  ///< Tag regex
} SubTag; /* }}} */

typedef struct subview_t /* {{{ */
{
  FLAGS             flags;                                        ///< View flags
  TAGS              tags;
  int               width;                                        ///< View tags, button width, layout
  Window            frame, button;                                ///< View frame, button
  char              *name;                                        ///< View name
  struct subarray_t *layout;                                      ///< View layout
} SubView; /* }}} */

extern SubSubtle *subtle;
/* }}} */

/* array.c {{{ */
SubArray *subArrayNew(void);                                      ///< Create new array
void subArrayPush(SubArray *a, void *e);                          ///< Push element to array
void subArrayPop(SubArray *a, void *e);                           ///< Pop element from array
int subArrayIndex(SubArray *a, void *e);                           ///< Find array id of element
void subArraySplice(SubArray *a, int idx, int len);               ///< Splice array at idx with len
void subArraySort(SubArray *a,                                    ///< Sort array with given compare function 
  int(*compar)(const void *a, const void *b));
void subArrayKill(SubArray *a, int clean);                        ///< Kill array with all elements
/* }}} */

/* client.c {{{ */
SubClient *subClientNew(Window win);                              ///< Create new client
void subClientConfigure(SubClient *c);                            ///< Send configure request
void subClientRender(SubClient *c);                               ///< Render client
void subClientFocus(SubClient *c);                                ///< Focus client
void subClientDrag(SubClient *c, int mode);                       ///< Move/drag client
void subClientToggle(SubClient *c, int type);                     ///< Toggle client state
void subClientFetchName(SubClient *c);                            ///< Fetch client name
void subClientSetWMState(SubClient *c, long state);               ///< Set client WM state
long subClientGetWMState(SubClient *c);                           ///< Get client WM state
void subClientPublish(void);                                      ///< Publish all clients
void subClientKill(SubClient *c);                                 ///< Kill client
/* }}} */

/* display.c {{{ */
void subDisplayInit(const char *display);                         ///< Create new display
void subDisplayScan(void);                                        ///< Scan root window
void subDisplayPublish(void);                                     ///< Publish display
void subDisplayFinish(void);                                      ///< Delete display
/* }}} */

/* event.c {{{ */
void subEventLoop(void);                                          ///< Event loop
/* }}} */

/* ewmh.c  {{{ */
void subEwmhInit(void);                                           ///< Init atoms/hints
Atom subEwmhFind(int hint);                                       ///< Find atom
char *subEwmhGetProperty(Window win, 
  Atom type, int hint, unsigned long *size);                      ///< Get property
void subEwmhSetWindows(Window win, int hint, 
  Window *values, int size);                                      ///< Set window properties
void subEwmhSetCardinals(Window win, int hint,
  long *values, int size);                                        ///< Set cardinal properties
void subEwmhSetString(Window win, int hint, 
  char *value);                                                   ///< Set string property
void subEwmhSetStrings(Window win, int hint,                      ///< Set string properties
  char **values, int size);
/* }}} */

/* grab.c {{{ */
void subGrabInit(void);                                           ///< Init keymap
SubGrab *subGrabNew(const char *name,                             ///< Create grab
  const char *value);
KeySym subGrabGet(void);                                          ///< Get grab
SubGrab *subGrabFind(int code, unsigned int mod);                 ///< Find grab
void subGrabSet(Window win);                                      ///< Grab window
void subGrabUnset(Window win);                                    ///< Ungrab window
int subGrabCompare(const void *a, const void *b);                 ///< Compare grabs
void subGrabKill(SubGrab *g);                                     ///< Kill grab
/* }}} */

/* layout.c {{{ */
SubLayout *subLayoutNew(SubClient *c1, SubClient *c2, 
  int mode);                                                      ///< Create new layout
void subLayoutKill(SubLayout *l);                                 ///< Kill layout
/* }}} */

/* ruby.c {{{ */
void subRubyInit(void);                                           ///< Init Ruby stack 
void subRubyLoadConfig(const char *file);                         ///< Load config file
void subRubyLoadSublets(const char *path);                        ///< Load sublets
void subRubyRun(SubSublet *s);                                    ///< Run Ruby script
void subRubyFinish(void);                                         ///< Kill Ruby stack
/* }}} */

/* sublet.c {{{ */
SubSublet *subSubletNew(void);                                    ///< Create new sublet
void subSubletUpdate(void);                                       ///< Update sublet bar
void subSubletRender(void);                                       ///< Render sublet
int subSubletCompare(const void *a, const void *b);               ///< Compare two sublets
void subSubletKill(SubSublet *s);                                 ///< Kill sublet
/* }}} */

/* tag.c {{{ */
void subTagInit(void);                                            ///< Init tags
SubTag *subTagNew(char *name, char *regex);                       ///< Create tag
SubTag *subTagFind(char *name, int *id);                          ///< Find tag
void subTagPublish(void);                                         ///< Publish tags
void subTagKill(SubTag *t);                                       ///< Delete tag
/* }}} */

/* util.c {{{ */
#ifdef DEBUG
#define subUtilLogDebug(...)  subUtilLog(0, __FILE__, __LINE__, __VA_ARGS__);
#else
#define subUtilLogDebug(...)
#endif /* DEBUG */

#define subUtilLogError(...)  subUtilLog(1, __FILE__, __LINE__,  __VA_ARGS__);
#define subUtilLogWarn(...)    subUtilLog(2, __FILE__, __LINE__, __VA_ARGS__);

void subUtilLog(int type, const char *file,
  int line, const char *format, ...);                             ///< Print messages
int subUtilLogXError(Display *disp, XErrorEvent *ev);             ///< Print X error messages
void *subUtilAlloc(size_t n, size_t size);                        ///< Allocate memory
void *subUtilRealloc(void *mem, size_t size);                     ///< Reallocate memory
XPointer *subUtilFind(Window win, XContext id);                   ///< Find window data
time_t subUtilTime(void);                                         ///< Get the current time
regex_t * subUtilRegexNew(char *regex);                           ///< Create new regex
int subUtilRegexMatch(regex_t *preg, char *string);               ///< Check if string matches preg
void subUtilRegexKill(regex_t *preg);                             ///< Kill regex
/* }}} */

/* view.c {{{ */
SubView *subViewNew(char *name, char *tags);                      ///< Create new view
void subViewConfigure(SubView *v);                                ///< Configure view
void subViewArrange(SubView *v, SubClient *c1, 
  SubClient *c2, int mode);                                       ///< Arrange view
void subViewUpdate(void);                                         ///< Update views
void subViewRender(void);                                         ///< Render views
void subViewJump(SubView *v);                                     ///< Jump to view
SubView *subViewFind(char *name, int *id);                        ///< Find view
void subViewPublish(void);                                        ///< Publish views
void subViewSanitize(SubClient *c);                               ///< Sanitize views
void subViewKill(SubView *v);                                     ///< Kill view
/* }}} */

#endif /* SUBTLE_H */
