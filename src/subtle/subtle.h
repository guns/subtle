
 /**
  * @package subtle
  *
  * @file Header file
  * @copyright Copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
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
#include "shared.h"

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif /* HAVE_SYS_INOTIFY */
/* }}} */

/* Macros {{{ */
#define FLAGS        unsigned int                                 ///< Flags
#define TAGS         unsigned int                                 ///< Tags

#define CLIENTID     1L                                           ///< Client data id
#define VIEWID       2L                                           ///< View data id
#define TRAYID       3L                                           ///< Tray data id
#define SUBLETID     4L                                           ///< Sublet data id

#define MINW         100L                                         ///< Client min width
#define MINH         100L                                         ///< Client min height
#define EXECTIME     1                                            ///< Max execution time
#define WAITTIME     10                                           ///< Max waiting time
#define DEFAULTTAG   (1L << 1)                                    ///< Default tag

#define WIDTH(c)     (c->geom.width + 2 * subtle->bw)             ///< Get real width
#define HEIGHT(c)    (c->geom.height + 2 * subtle->bw)            ///< Get real height
#define ZERO(n)      (0 < n ? n : 1)                              ///< Prevent zero
#define MIN(a,b)     (a >= b ? b : a)                             ///< Minimum
#define MAX(a,b)     (a >= b ? a : b)                             ///< Maximum

#define DEAD(c) \
  if(!c || c->flags & SUB_CLIENT_DEAD) return;                    ///< Check dead clients

#define MINMAX(val,min,max) \
  (min && val < min ? min : max && val > max ? max : val)         ///< Value min/max

#define ROOTMASK \
  (SubstructureRedirectMask|SubstructureNotifyMask|FocusChangeMask|PropertyChangeMask)
#define EVENTMASK \
  (StructureNotifyMask|PropertyChangeMask|EnterWindowMask|FocusChangeMask)
#define DRAGMASK \
  (PointerMotionMask|ButtonReleaseMask|KeyPressMask|EnterWindowMask|FocusChangeMask)
#define GRABMASK \
  (ButtonPressMask|ButtonReleaseMask|PointerMotionMask)

#define VISUAL \
  DefaultVisual(subtle->dpy, DefaultScreen(subtle->dpy))         ///< Default visual
#define COLORMAP \
  DefaultColormap(subtle->dpy, DefaultScreen(subtle->dpy))       ///< Default colormap
#define VISIBLE(v,c) \
  (v && c && (v->tags & c->tags || c->flags & SUB_MODE_STICK))   ///< Visible on view
#define SCREENW \
  DisplayWidth(subtle->dpy, DefaultScreen(subtle->dpy))          ///< Get screen width
#define SCREENH \
  DisplayHeight(subtle->dpy, DefaultScreen(subtle->dpy))         ///< Get screen height

#define CURVIEW VIEW(subtle->views->data[subtle->vid])           ///< Get current view
#define CURSCREEN SCREEN(subtle->screens->data[subtle->sid])     ///< Get current screen
#define DEFSCREEN SCREEN(subtle->screens->data[0])               ///< Get first screen

#define MODES_ALL \
  (SUB_MODE_FULL|SUB_MODE_FLOAT|SUB_MODE_STICK| \
  SUB_MODE_URGENT|SUB_MODE_RESIZE)                                ///< All mode flags

#define MODES_NONE \
  (SUB_MODE_NONFULL|SUB_MODE_NONFLOAT|SUB_MODE_NONSTICK| \
  SUB_MODE_NONURGENT|SUB_MODE_NONRESIZE)                          ///< All none mode flags

#define ROOT       DefaultRootWindow(subtle->dpy)                 ///< Root window
#define SCRN       DefaultScreen(subtle->dpy)                     ///< Default screen

#define SETRECT(r,a,b,c,d) \
  r.x      = a; \
  r.y      = b; \
  r.width  = c; \
  r.height = d;                                                   ///< Set rect values

/* Casts */
#define ARRAY(a)   ((SubArray *)a)                                ///< Cast to SubArray
#define CLIENT(c)  ((SubClient *)c)                               ///< Cast to SubClient
#define DATA(d)    ((SubData)d)                                   ///< Cast to SubData
#define GRAB(g)    ((SubGrab *)g)                                 ///< Cast to SubGrab
#define GRAVITY(g) ((SubGravity *)g)                              ///< Cast to SubGravity
#define HOOK(h)    ((SubHook *)h)                                 ///< Cast to SubHook
#define ICON(i)    ((SubIcon *)i)                                 ///< Cast to SubIcon
#define PANEL(p)   ((SubPanel *)p)                                ///< Cast to SubPanel
#define SCREEN(s)  ((SubScreen *)s)                               ///< Cast to SubScreen
#define SUBLET(s)  ((SubSublet *)s)                               ///< Cast to SubSublet
#define TEXT(t)    ((SubText *)t)                                 ///< Cast to SubText
#define TAG(t)     ((SubTag *)t)                                  ///< Cast to SubTag
#define TRAY(t)    ((SubTray *)t)                                 ///< Cast to SubTray
#define VIEW(v)    ((SubView *)v)                                 ///< Cast to SubView

/* XEmbed messages */
#define XEMBED_EMBEDDED_NOTIFY         0L                         ///< Start embedding
#define XEMBED_WINDOW_ACTIVATE         1L                         ///< Tray has focus
#define XEMBED_WINDOW_DEACTIVATE       2L                         ///< Tray has no focus
#define XEMBED_REQUEST_FOCUS           3L
#define XEMBED_FOCUS_IN                4L                         ///< Focus model
#define XEMBED_FOCUS_OUT               5L
#define XEMBED_FOCUS_NEXT              6L
#define XEMBED_FOCUS_PREV              7L
#define XEMBED_GRAB_KEY                8L
#define XEMBED_UNGRAB_KEY              9L
#define XEMBED_MODALITY_ON             10L
#define XEMBED_MODALITY_OFF            11L
#define XEMBED_REGISTER_ACCELERATOR    12L
#define XEMBED_UNREGISTER_ACCELERATOR  13L
#define XEMBED_ACTIVATE_ACCELERATOR    14L

/* Details for XEMBED_FOCUS_IN */
#define XEMBED_FOCUS_CURRENT           0L                         /// Focus default
#define XEMBED_FOCUS_FIRST             1L
#define XEMBED_FOCUS_LAST              2L

/* Flags for _XEMBED_INFO */
#define XEMBED_MAPPED                  (1L << 0)                  ///< Tray mapped
/* }}} */

/* Flags {{{ */
/* Data types */
#define SUB_TYPE_CLIENT               (1L << 1)                   ///< Client
#define SUB_TYPE_GRAB                 (1L << 2)                   ///< Grab
#define SUB_TYPE_GRAVITY              (1L << 3)                   ///< Gravity
#define SUB_TYPE_HOOK                 (1L << 4)                   ///< Hook
#define SUB_TYPE_PANEL                (1L << 5)                   ///< Panel
#define SUB_TYPE_SCREEN               (1L << 6)                   ///< Screen
#define SUB_TYPE_SUBLET               (1L << 7)                   ///< Sublet
#define SUB_TYPE_TAG                  (1L << 8)                   ///< Tag
#define SUB_TYPE_TRAY                 (1L << 9)                   ///< Tray
#define SUB_TYPE_VIEW                 (1L << 10)                  ///< View

/* Mode flags */
#define SUB_MODE_FULL                 (1L << 20)                  ///< Fullscreen mode
#define SUB_MODE_FLOAT                (1L << 21)                  ///< Float mode
#define SUB_MODE_STICK                (1L << 22)                  ///< Stick mode
#define SUB_MODE_URGENT               (1L << 23)                  ///< Urgent mode
#define SUB_MODE_URGENT_FOCUS         (1L << 24)                  ///< Urgent mode until focus
#define SUB_MODE_RESIZE               (1L << 25)                  ///< Resize mode
#define SUB_MODE_NONFULL              (1L << 26)                  ///< Disable full mode
#define SUB_MODE_NONFLOAT             (1L << 27)                  ///< Disable float mode
#define SUB_MODE_NONSTICK             (1L << 28)                  ///< Disable stick mode
#define SUB_MODE_NONURGENT            (1L << 29)                  ///< Disable urgent mode
#define SUB_MODE_NONRESIZE            (1L << 30)                  ///< Disable resize mode

/* Call flags */
#define SUB_CALL_HOOKS                (1L << 13)                  ///< Call hook
#define SUB_CALL_SUBLET_HOOKS         (1L << 14)                  ///< Call sublet hooks
#define SUB_CALL_SUBLET_CONFIGURE     (1L << 15)                  ///< Sublet watch hook
#define SUB_CALL_SUBLET_RUN           (1L << 16)                  ///< Sublet run hook
#define SUB_CALL_SUBLET_WATCH         (1L << 17)                  ///< Sublet watch hook
#define SUB_CALL_SUBLET_DOWN          (1L << 18)                  ///< Sublet mouse down hook
#define SUB_CALL_SUBLET_OVER          (1L << 19)                  ///< Sublet mouse over hook
#define SUB_CALL_SUBLET_OUT           (1L << 20)                  ///< Sublet mouse out hook

/* Hooks */
#define SUB_HOOK_START                (1L << 15)                  ///< Start hook (must start at 15)
#define SUB_HOOK_RELOAD               (1L << 16)                  ///< Start hook
#define SUB_HOOK_EXIT                 (1L << 17)                  ///< Exit hook
#define SUB_HOOK_CLIENT_CREATE        (1L << 18)                  ///< Client create hook
#define SUB_HOOK_CLIENT_CONFIGURE     (1L << 19)                  ///< Client configure hook
#define SUB_HOOK_CLIENT_FOCUS         (1L << 20)                  ///< Client focus hook
#define SUB_HOOK_CLIENT_KILL          (1L << 21)                  ///< Client kill hook
#define SUB_HOOK_TAG_CREATE           (1L << 22)                  ///< Tag create hook
#define SUB_HOOK_TAG_KILL             (1L << 23)                  ///< Tag kill hook
#define SUB_HOOK_VIEW_CREATE          (1L << 24)                  ///< View create hook
#define SUB_HOOK_VIEW_CONFIGURE       (1L << 25)                  ///< View configure hook
#define SUB_HOOK_VIEW_JUMP            (1L << 26)                  ///< View jump hook
#define SUB_HOOK_VIEW_KILL            (1L << 27)                  ///< View kill hook

/* Client flags */
#define SUB_CLIENT_FOCUS              (1L << 13)                  ///< Send focus message
#define SUB_CLIENT_INPUT              (1L << 14)                  ///< Active/passive focus-model
#define SUB_CLIENT_CLOSE              (1L << 15)                  ///< Send close message
#define SUB_CLIENT_DOCK               (1L << 16)                  ///< Dock window
#define SUB_CLIENT_DEAD               (1L << 17)                  ///< Dead window

/* Drag flags */
#define SUB_DRAG_START                (1L << 1)                   ///< Drag start
#define SUB_DRAG_MOVE                 (1L << 2)                   ///< Drag move
#define SUB_DRAG_SWAP                 (1L << 3)                   ///< Drag swap
#define SUB_DRAG_RESIZE               (1L << 4)                   ///< Drag resize

/* Grab flags */
#define SUB_GRAB_KEY                  (1L << 13)                  ///< Key grab
#define SUB_GRAB_MOUSE                (1L << 14)                  ///< Mouse grab  
#define SUB_GRAB_SPAWN                (1L << 15)                  ///< Spawn an app
#define SUB_GRAB_PROC                 (1L << 16)                  ///< Grab with proc
#define SUB_GRAB_VIEW_JUMP            (1L << 17)                  ///< Jump to view
#define SUB_GRAB_SCREEN_JUMP          (1L << 18)                  ///< Jump to screen
#define SUB_GRAB_SUBLETS_RELOAD       (1L << 19)                  ///< Reload subtle
#define SUB_GRAB_SUBTLE_RELOAD        (1L << 20)                  ///< Reload subtle
#define SUB_GRAB_SUBTLE_QUIT          (1L << 21)                  ///< Quit subtle
#define SUB_GRAB_WINDOW_MOVE          (1L << 22)                  ///< Resize window
#define SUB_GRAB_WINDOW_RESIZE        (1L << 23)                  ///< Move window
#define SUB_GRAB_WINDOW_TOGGLE        (1L << 24)                  ///< Toggle window
#define SUB_GRAB_WINDOW_STACK         (1L << 25)                  ///< Stack window
#define SUB_GRAB_WINDOW_SELECT        (1L << 26)                  ///< Select window
#define SUB_GRAB_WINDOW_GRAVITY       (1L << 27)                  ///< Set gravity of window
#define SUB_GRAB_WINDOW_SCREEN        (1L << 28)                  ///< Set screen of window
#define SUB_GRAB_WINDOW_KILL          (1L << 29)                  ///< Kill window

/* Panel flags */
#define SUB_PANEL_SPACER1             (1L << 13)                  ///< Panel spacer1
#define SUB_PANEL_SPACER2             (1L << 14)                  ///< Panel spacer2
#define SUB_PANEL_SEPARATOR1          (1L << 15)                  ///< Panel separator1
#define SUB_PANEL_SEPARATOR2          (1L << 16)                  ///< Panel separator2
#define SUB_PANEL_BOTTOM              (1L << 17)                  ///< Panel bottom
#define SUB_PANEL_SUBLETS             (1L << 18)                  ///< Panel sublets
#define SUB_PANEL_HIDDEN              (1L << 19)                  ///< Hidden panel

/* Sublet types */
#define SUB_SUBLET_INTERVAL           (1L << 20)
#define SUB_SUBLET_INOTIFY            (1L << 21)                  ///< Inotify sublet
#define SUB_SUBLET_SOCKET             (1L << 22)                  ///< Socket sublet
#define SUB_SUBLET_RUN                (1L << 23)                  ///< Sublet run function
#define SUB_SUBLET_DOWN               (1L << 24)                  ///< Sublet mouse down function
#define SUB_SUBLET_OVER               (1L << 25)                  ///< Sublet mouse over function
#define SUB_SUBLET_OUT                (1L << 26)                  ///< Sublet mouse out function
#define SUB_SUBLET_WATCH              (1L << 27)                  ///< Sublet watch function
#define SUB_SUBLET_PANEL              (1L << 28)                  ///< Sublet in panel

/* Subtle flags */
#define SUB_SUBTLE_DEBUG              (1L << 1)                   ///< Debug enabled
#define SUB_SUBTLE_STIPPLE            (1L << 2)                   ///< Stipple enabled
#define SUB_SUBTLE_PANEL1             (1L << 3)                   ///< Panel1 enabled
#define SUB_SUBTLE_PANEL2             (1L << 4)                   ///< Panel2 enabled
#define SUB_SUBTLE_URGENT             (1L << 5)                   ///< Urgent transients
#define SUB_SUBTLE_RESIZE             (1L << 6)                   ///< Respect size
#define SUB_SUBTLE_BACKGROUND         (1L << 7)                   ///< Set root background
#define SUB_SUBTLE_XINERAMA           (1L << 8)                   ///< Using Xinerama
#define SUB_SUBTLE_XRANDR             (1L << 9)                   ///< Using Xrandr
#define SUB_SUBTLE_EWMH               (1L << 10)                  ///< EWMH set
#define SUB_SUBTLE_RUN                (1L << 11)                  ///< Run event loop
#define SUB_SUBTLE_REPLACE            (1L << 12)                  ///< Replace previous wm

/* Tag flags */
#define SUB_TAG_GRAVITY               (1L << 13)                  ///< Gravity property
#define SUB_TAG_SCREEN                (1L << 14)                  ///< Screen property
#define SUB_TAG_GEOMETRY              (1L << 15)                  ///< Geometry property
#define SUB_TAG_MATCH                 (1L << 16)                  ///< Match property
#define SUB_TAG_MATCH_NAME            (1L << 17)                  ///< Match WM_NAME
#define SUB_TAG_MATCH_INSTANCE        (1L << 18)                  ///< Match instance of WM_CLASS
#define SUB_TAG_MATCH_CLASS           (1L << 19)                  ///< Match class of WM_CLASS
#define SUB_TAG_MATCH_ROLE            (1L << 20)                  ///< Match role of window
/* }}} */

/* Typedefs {{{ */
typedef struct subarray_t /* {{{ */
{
  int   ndata;                                                    ///< Array data count
  void **data;                                                    ///< Array data
} SubArray; /* }}} */

typedef struct subclient_t /* {{{ */
{
  FLAGS      flags;                                               ///< Client flags
  char       *name, *instance, *klass;                            ///< Client instance, klass

  TAGS       tags;                                                ///< Client tags
  Window     win;                                                 ///< Client window
  Colormap   cmap;                                                ///< Client colormap
  XRectangle geom, base;                                          ///< Client geom, base

  float      minr, maxr;                                          ///< Client ratios
  int        minw, minh, maxw, maxh, incw, inch;                  ///< Client sizes

  int        gravity, screen;                                     ///< Client informations
  int        *gravities, *screens;                                ///< Client per view
} SubClient; /* }}} */

typedef enum subewmh_t /* {{{ */
{
  /* ICCCM */
  SUB_EWMH_WM_NAME,                                               ///< Name of window
  SUB_EWMH_WM_CLASS,                                              ///< Class of window
  SUB_EWMH_WM_STATE,                                              ///< Window state
  SUB_EWMH_WM_PROTOCOLS,                                          ///< Supported protocols 
  SUB_EWMH_WM_TAKE_FOCUS,                                         ///< Send focus messages
  SUB_EWMH_WM_DELETE_WINDOW,                                      ///< Send close messages
  SUB_EWMH_WM_NORMAL_HINTS,                                       ///< Window normal hints
  SUB_EWMH_WM_SIZE_HINTS,                                         ///< Window size hints
  SUB_EWMH_WM_HINTS,                                              ///< Window hints
  SUB_EWMH_WM_WINDOW_ROLE,                                        ///< Window role

  /* EWMH */
  SUB_EWMH_NET_SUPPORTED,                                         ///< Supported states
  SUB_EWMH_NET_CLIENT_LIST,                                       ///< List of clients
  SUB_EWMH_NET_CLIENT_LIST_STACKING,                              ///< List of clients
  SUB_EWMH_NET_NUMBER_OF_DESKTOPS,                                ///< Total number of views
  SUB_EWMH_NET_DESKTOP_NAMES,                                     ///< Names of the views
  SUB_EWMH_NET_DESKTOP_GEOMETRY,                                  ///< Desktop geometry
  SUB_EWMH_NET_DESKTOP_VIEWPORT,                                  ///< Viewport of the view
  SUB_EWMH_NET_CURRENT_DESKTOP,                                   ///< Number of current view
  SUB_EWMH_NET_ACTIVE_WINDOW,                                     ///< Focus window
  SUB_EWMH_NET_WORKAREA,                                          ///< Workarea of the views
  SUB_EWMH_NET_SUPPORTING_WM_CHECK,                               ///< Check for compliant window manager
  SUB_EWMH_NET_VIRTUAL_ROOTS,                                     ///< List of virtual destops
  SUB_EWMH_NET_CLOSE_WINDOW,                                      ///< Close window             
  SUB_EWMH_NET_RESTACK_WINDOW,                                    ///< Change window stacking
  SUB_EWMH_NET_MOVERESIZE_WINDOW,                                 ///< Resize window
  SUB_EWMH_NET_SHOWING_DESKTOP,                                   ///< Showing desktop mode 

  SUB_EWMH_NET_WM_NAME,                                           ///< Name of client
  SUB_EWMH_NET_WM_PID,                                            ///< PID of client
  SUB_EWMH_NET_WM_DESKTOP,                                        ///< Desktop client is on
  SUB_EWMH_NET_WM_STRUT,                                          ///< Strut

  SUB_EWMH_NET_WM_WINDOW_TYPE,                                    ///< Window type
  SUB_EWMH_NET_WM_WINDOW_TYPE_DIALOG,                             ///< Dialog window
  SUB_EWMH_NET_WM_WINDOW_TYPE_DOCK,                               ///< Dock window

  SUB_EWMH_NET_WM_STATE,                                          ///< Window state
  SUB_EWMH_NET_WM_STATE_FULLSCREEN,                               ///< Fullscreen window
  SUB_EWMH_NET_WM_STATE_ABOVE,                                    ///< Floating window
  SUB_EWMH_NET_WM_STATE_STICKY,                                   ///< Sticky window

  /* Tray */
  SUB_EWMH_NET_SYSTEM_TRAY_OPCODE,                                ///< Tray messages
  SUB_EWMH_NET_SYSTEM_TRAY_MESSAGE_DATA,                          ///< Tray message data
  SUB_EWMH_NET_SYSTEM_TRAY_SELECTION,                             ///< Tray selection

  /* Misc */
  SUB_EWMH_UTF8,                                                  ///< String encoding
  SUB_EWMH_MANAGER,                                               ///< Selection manager

  /* XEmbed */
  SUB_EWMH_XEMBED,                                                ///< XEmbed
  SUB_EWMH_XEMBED_INFO,                                           ///< XEmbed info

  /* subtle */
  SUB_EWMH_SUBTLE_WINDOW_TAG,                                     ///< Subtle window tag
  SUB_EWMH_SUBTLE_WINDOW_UNTAG,                                   ///< Subtle window untag
  SUB_EWMH_SUBTLE_WINDOW_TAGS,                                    ///< Subtle window tags
  SUB_EWMH_SUBTLE_WINDOW_GRAVITY,                                 ///< Subtle window gravity
  SUB_EWMH_SUBTLE_WINDOW_SCREEN,                                  ///< Subtle window screen
  SUB_EWMH_SUBTLE_WINDOW_FLAGS,                                   ///< Subtle window flags
  SUB_EWMH_SUBTLE_GRAVITY_NEW,                                    ///< Subtle gravity new
  SUB_EWMH_SUBTLE_GRAVITY_LIST,                                   ///< Subtle gravity list
  SUB_EWMH_SUBTLE_GRAVITY_KILL,                                   ///< Subtle gravtiy kill
  SUB_EWMH_SUBTLE_TAG_NEW,                                        ///< Subtle tag new
  SUB_EWMH_SUBTLE_TAG_LIST,                                       ///< Subtle tag list
  SUB_EWMH_SUBTLE_TAG_KILL,                                       ///< Subtle tag kill
  SUB_EWMH_SUBTLE_TRAY_LIST,                                      ///< Subtle tray list
  SUB_EWMH_SUBTLE_TRAY_KILL,                                      ///< Subtle tray kill
  SUB_EWMH_SUBTLE_VIEW_NEW,                                       ///< Subtle view new
  SUB_EWMH_SUBTLE_VIEW_KILL,                                      ///< Subtle view kill
  SUB_EWMH_SUBTLE_SUBLET_NEW,                                     ///< Subtle sublet new
  SUB_EWMH_SUBTLE_SUBLET_UPDATE,                                  ///< Subtle sublet update
  SUB_EWMH_SUBTLE_SUBLET_DATA,                                    ///< Subtle sublet data
  SUB_EWMH_SUBTLE_SUBLET_BACKGROUND,                              ///< Subtle sublet background
  SUB_EWMH_SUBTLE_SUBLET_LIST,                                    ///< Subtle sublet list
  SUB_EWMH_SUBTLE_SUBLET_KILL,                                    ///< Subtle sublet kill
  SUB_EWMH_SUBTLE_RELOAD_CONFIG,                                  ///< Subtle reload config
  SUB_EWMH_SUBTLE_RELOAD_SUBLETS,                                 ///< Subtle reload sublets
  SUB_EWMH_SUBTLE_QUIT,                                           ///< Subtle quit

  SUB_EWMH_TOTAL
} SubEwmh; /* }}} */

typedef struct subgrab_t /* {{{ */
{
  FLAGS           flags;                                          ///< Grab flags

  KeySym          sym;                                            ///< Grab sym
  unsigned int    code, mod;                                      ///< Grab code, modifier

  union subdata_t data;                                           ///< Grab data
} SubGrab; /* }}} */

typedef struct subgravity_t /* {{{ */
{
  FLAGS      flags;                                               ///< Gravity flags

  int        quark;                                               ///< Gravity quark
  XRectangle geometry;                                            ///< Gravity geometry
} SubGravity; /* }}} */

typedef struct subhook_t /* {{{ */
{
  FLAGS         flags;                                            ///< Hook flags
  unsigned long proc;                                             ///< Hook proc
  void          *data;                                            ///< Hook data
} SubHook; /* }}} */

typedef struct subpanel_t /* {{{ */
{
  FLAGS             flags;                                        ///< Panel flags
  Window            win;                                          ///< Panel win
  int               x, width;                                     ///< Panel x, width

  struct subpanel_t *next;                                        ///< Panel next
} SubPanel; /* }}} */

typedef struct subscreen_t /* {{{ */
{
  FLAGS        flags;                                             ///< Screen flags

  XRectangle geom, base;                                          ///< Screen geom, base
} SubScreen; /* }}} */

typedef struct subsublet_t /* {{{ */
{
  FLAGS             flags;                                        ///< Sublet flags
  Window            win;                                          ///< Sublet window
  int               x, width, watch;                              ///< Sublet x, width, width
 
  char              *name;                                        ///< Sublet name
  unsigned long     instance, bg;                                 ///< Sublet ruby instance, background color
  time_t            time, interval;                               ///< Sublet update/interval time

  struct subtext_t  *text;                                        ///< Sublet text
} SubSublet; /* }}} */

typedef struct subsubtle_t /* {{{ */
{
  FLAGS                flags;                                     ///< Subtle flags

  int                  th, bw, step, snap, limit, gravity;        ///< Subtle properties
  int                  vid, sid;                                  ///< Subtle current view, screen

  Display              *dpy;                                      ///< Subtle Xorg display

  XRectangle           strut;                                     ///< Subtle strut

  struct subfont_t     *font;                                     ///< Subtle font

  struct subarray_t    *clients;                                  ///< Subtle clients
  struct subarray_t    *grabs;                                    ///< Subtle grabs
  struct subarray_t    *gravities;                                ///< Subtle gravities
  struct subarray_t    *hooks;                                    ///< Subtle hooks
  struct subarray_t    *panels;                                   ///< Subtle panels
  struct subarray_t    *screens;                                  ///< Subtle screens
  struct subarray_t    *sublets;                                  ///< Subtle sublets
  struct subarray_t    *tags;                                     ///< Subtle tags
  struct subarray_t    *trays;                                    ///< Subtle trays
  struct subarray_t    *views;                                    ///< Subtle views

#ifdef HAVE_SYS_INOTIFY_H
  int                  notify;                                    ///< Subtle inotify descriptor
#endif /* HAVE_SYS_INOTIFY_H */

#ifdef HAVE_X11_EXTENSIONS_XRANDR_H
  int                  xrandr;                                    ///< Subtle Xrandr events
#endif /* HAVE_X11_EXTENSIONS_XRANDR_H */

  struct
  {
    char             *string;                                    ///< Subtle sublet separator
    int               width;
  } separator;

  struct
  {
    char               *config, *sublets;                         ///< Subtle paths
  } paths;

  struct
  {
    Window             panel1, panel2, focus, support;
    struct subpanel_t  views, title, tray;
  } windows;                                                      ///< Subtle windows

  struct
  {
    unsigned long      fg_panel, fg_views, fg_sublets, fg_focus, fg_urgent,
                       bg_panel, bg_views, bg_sublets, bg_focus, bg_urgent,
                       bo_focus, bo_normal, bg;
  } colors;                                                       ///< Subtle colors

  struct
  {
    GC                 font, stipple, invert;
  } gcs;                                                          ///< Subtle graphic contexts

  struct
  {
    Cursor             arrow, move, resize;
  } cursors;                                                      ///< Subtle cursors
} SubSubtle; /* }}} */

typedef struct subtag_t /* {{{ */
{
  FLAGS      flags;                                               ///< Tag flags
  char       *name;                                               ///< Tag name
  regex_t    *preg;                                               ///< Tag regex
  int        gravity, screen;                                     ///< Tag gravity, screen
  XRectangle geometry;                                            ///< Tag geometry
} SubTag; /* }}} */

typedef struct subtray_t /* {{{ */
{
  FLAGS  flags;                                                   ///< Tray flags
  char   *name;                                                   ///< Tray name

  Window win;                                                     ///< Tray window
  int    width;                                                   ///< Tray width
} SubTray; /* }}} */

typedef struct subview_t /* {{{ */
{
  FLAGS    flags;                                                 ///< View flags
  char     *name;                                                 ///< View name
  TAGS     tags;                                                  ///< View tags

  Window   button;                                                ///< View button
  int      width;                                                 ///< View width
} SubView; /* }}} */

extern SubSubtle *subtle;
/* }}} */

/* array.c {{{ */
SubArray *subArrayNew(void);                                      ///< Create array
void subArrayPush(SubArray *a, void *elem);                       ///< Push element to array
void subArrayInsert(SubArray *a, int pos, void *elem);            ///< Insert element at pos
void subArrayRemove(SubArray *a, void *elem);                     ///< Remove element from array
void *subArrayGet(SubArray *a, int idx);                          ///< Get element
int subArrayIndex(SubArray *a, void *elem);                       ///< Find array id of element
void subArraySort(SubArray *a,                                    ///< Sort array with given compare function 
  int(*compar)(const void *a, const void *b));
void subArrayClear(SubArray *a, int clean);                       ///< Delete all elements
void subArrayKill(SubArray *a, int clean);                        ///< Kill array with all elements
/* }}} */

/* client.c {{{ */
SubClient *subClientNew(Window win);                              ///< Create client
void subClientConfigure(SubClient *c);                            ///< Send configure request
void subClientRender(SubClient *c);                               ///< Render client
int subClientCompare(const void *a, const void *b);               ///< Compare two clients
void subClientFocus(SubClient *c);                                ///< Focus client
void subClientWarp(SubClient *c);                                 ///< Warp to client
void subClientDrag(SubClient *c, int mode);                       ///< Move/drag client
void subClientUpdate(int vid);                                    ///< Update clients
int subClientTag(SubClient *c, int tag);                          ///< Tag client
void subClientSetTags(SubClient *c);                              ///< Update client tags
void subClientSetGravity(SubClient *c, int gravity, int force);   ///< Set client gravity
void subClientSetScreen(SubClient *c, int screen, int force);     ///< Set client screen
void subClientSetSize(SubClient *c);                              ///< Set client sizes
void subClientSetStrut(SubClient *c);                             ///< Set client strut
void subClientSetName(SubClient *c);                              ///< Set client WM_NAME
void subClientSetProtocols(SubClient *c);                         ///< Set client protocols
void subClientSetNormalHints(SubClient *c);                       ///< Set client normal hints
void subClientSetHints(SubClient *c, int *flags);                 ///< Set client WM hints
void subClientSetState(SubClient *c, int *flags);                 ///< Set client WM state
void subClientSetTransient(SubClient *c, int *flags);             ///< Set client transient
void subClientSetType(SubClient *c, int *flags);                  ///< Set client tyoe
void subClientToggle(SubClient *c, int type);                     ///< Toggle client state
void subClientPublish(void);                                      ///< Publish all clients
void subClientKill(SubClient *c, int destroy);                    ///< Kill client
/* }}} */

/* display.c {{{ */
void subDisplayInit(const char *display);                         ///< Create display
void subDisplayConfigure(void);                                   ///< Configure display
void subDisplayScan(void);                                        ///< Scan root window
void subDisplayFinish(void);                                      ///< Kill display
/* }}} */

/* event.c {{{ */
void subEventWatchAdd(int fd);                                    ///< Add watch fd
void subEventWatchDel(int fd);                                    ///< Del watch fd
void subEventLoop(void);                                          ///< Event loop
void subEventFinish(void);                                        ///< Finish events
/* }}} */

/* ewmh.c  {{{ */
void subEwmhInit(void);                                           ///< Init atoms/hints
Atom subEwmhGet(SubEwmh e);                                       ///< Get atom
SubEwmh subEwmhFind(Atom atom);                                   ///< Find atom id
long subEwmhGetWMState(Window win);                               ///< Get window WM state
long subEwmhGetXEmbedState(Window win);                           ///< Get window XEmbed state
void subEwmhSetWindows(Window win, SubEwmh e,
  Window *values, int size);                                      ///< Set window properties
void subEwmhSetCardinals(Window win, SubEwmh e,
  long *values, int size);                                        ///< Set cardinal properties
void subEwmhSetString(Window win, SubEwmh e,
  char *value);                                                   ///< Set string property
void subEwmhSetStrings(Window win, SubEwmh e,                     ///< Set string properties
  char **values, int size);
void subEwmhSetWMState(Window win, long state);                   ///< Set window WM state
int subEwmhMessage(Window dst, Window win, SubEwmh e,
  long data0, long data1, long data2, long data3,
  long data4);                                                    ///< Send message
void subEwmhFinish(void);                                         ///< Unset EWMH properties
/* }}} */

/* grab.c {{{ */
void subGrabInit(void);                                           ///< Init keymap
SubGrab *subGrabNew(const char *chain, int type,                  ///< Create grab
  SubData data);
KeySym subGrabGet(void);                                          ///< Get grab
SubGrab *subGrabFind(int code, unsigned int mod);                 ///< Find grab
void subGrabSet(Window win);                                      ///< Grab window
void subGrabUnset(Window win);                                    ///< Ungrab window
int subGrabCompare(const void *a, const void *b);                 ///< Compare grabs
void subGrabKill(SubGrab *g);                                     ///< Kill grab
/* }}} */

/* gravity.c {{{ */
SubGravity *subGravityNew(const char *name,
  XRectangle *geometry);                                          ///< Create gravity
int subGravityFind(const char *name, int quark);                  ///< Find gravity id
void subGravityPublish(void);                                     ///< Publish gravities
void subGravityKill(SubGravity *g);                               ///< Kill gravity
/* }}} */

/* hook.c {{{ */
SubHook *subHookNew(int type, unsigned long proc, void *data);    ///< Create hook
void subHookCall(int type, void *data);                           ///< Call hook
void subHookRemove(unsigned long proc, void *data);               ///< Remove hook
void subHookKill(SubHook *h);                                     ///< Kill hook
/* }}} */

/* panel.c {{{ */
void subPanelUpdate(void);                                        ///< Configure panels
void subPanelRender(void);                                        ///< Render panels
/* }}} */

/* ruby.c {{{ */
void subRubyInit(void);                                           ///< Init Ruby stack 
void subRubyLoadConfig(void);                                     ///< Load config file
void subRubyReloadConfig(void);                                   ///< Reload config file
void subRubyReloadSublets(void);                                  ///< Reload sublets
void subRubyLoadSublet(const char *file);                         ///< Load sublet
void subRubyLoadSublets(void);                                    ///< Load sublets
void subRubyLoadSubtlext(void);                                   ///< Load subtlext
void subRubyLoadPanels(void);                                     ///< Load panels
int subRubyCall(int type, unsigned long proc,
  void *data1, void *data2);                                      ///< Call Ruby script
int subRubyRemove(char *name);                                    ///< Remove constant
int subRubyRelease(unsigned long recv);                           ///< Release receiver
void subRubyFinish(void);                                         ///< Kill Ruby stack
/* }}} */

/* screen.c {{{ */
void subScreenInit(void);                                         ///< Init screens
SubScreen *subScreenNew(int x, int y, unsigned int width,
  unsigned int height);                                           ///< Create screen
void subScreenConfigure(void);                                    ///< Configure screens
void subScreenUpdate(void);                                       ///< Update screen sizes
void subScreenFit(SubScreen *s, XRectangle *r, int center);       ///< Fit rect size
void subScreenJump(SubScreen *s);                                 ///< Jump to screen
void subScreenKill(SubScreen *s);                                 ///< Kill screen
/* }}} */

/* sublet.c {{{ */
SubSublet *subSubletNew(void);                                    ///< Create sublet
void subSubletUpdate(void);                                       ///< Update sublet windows
void subSubletRender(SubSublet *s);                               ///< Render sublet
int subSubletCompare(const void *a, const void *b);               ///< Compare two sublets
void subSubletPublish(void);                                      ///< Publish sublets
void subSubletKill(SubSublet *s);                                 ///< Kill sublet
/* }}} */

/* subtle.c {{{ */
XPointer * subSubtleFind(Window win, XContext id);                ///< Find window
time_t subSubtleTime(void);                                       ///< Get current time
Window subSubtleFocus(int focus);                                 ///< Focus window
/* }}} */

/* tag.c {{{ */
SubTag *subTagNew(char *name, char *regex);                       ///< Create tag
void subTagPublish(void);                                         ///< Publish tags
void subTagKill(SubTag *t);                                       ///< Delete tag
/* }}} */

/* tray.c {{{ */
SubTray *subTrayNew(Window win);                                  ///< Create tray 
void subTrayConfigure(SubTray *t);                                ///< Configure tray
void subTrayFocus(SubTray * t);                                   ///< Focus tray
void subTrayUpdate(void);                                         ///< Update tray bar
void subTraySelect(void);                                         ///< Get selection
void subTrayFocus(SubTray *t);                                    ///< Focus tray
void subTraySetState(SubTray *t);                                 ///< Set state
void subTrayPublish(void);                                        ///< Publish trays
void subTrayKill(SubTray *t);                                     ///< Delete tray
/* }}} */

/* view.c {{{ */
SubView *subViewNew(char *name, char *tags);                      ///< Create view
void subViewConfigure(SubView *v, int align);                     ///< Configure view
void subViewUpdate(void);                                         ///< Update views
void subViewRender(void);                                         ///< Render views
void subViewJump(SubView *v, int focus);                          ///< Jump to view
void subViewPublish(void);                                        ///< Publish views
void subViewKill(SubView *v);                                     ///< Kill view
/* }}} */

#endif /* SUBTLE_H */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
