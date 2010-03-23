
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

#ifndef SHARED_H
#define SHARED_H 1

/* Includes {{{ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <regex.h>
#include <getopt.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/cursorfont.h>

#include "config.h"

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */
/* }}} */

/* Macros {{{ */
#define SEPARATOR "<>"                                            ///< Color separator
#define LENGTH(a) (sizeof(a) / sizeof(a[0]))                      ///< Array length
#define RINT(r)   printf("%s: x=%d, y=%d, width=%d, height=%d\n", \
  #r, r.x, r.y, r.width, r.height);                               ///< Print a XRectangle

#define DEFFONT   "-*-*-medium-*-*-*-14-*-*-*-*-*-*-*"            ///< Default font

/* Data flags */
#define SUB_DATA_STRING  (1L << 13)                               ///< String data
#define SUB_DATA_NUM     (1L << 14)                               ///< Num data
#define SUB_DATA_NIL     (1L << 15)                               ///< Nil data

#define FONT(f)   ((SubFont *)f)                                  ///< Cast to SubFont
#define TEXT(t)   ((SubText *)t)                                  ///< Cast to SubText
#define ITEM(i)   ((SubTextItem *)i)                              ///< Cast to SubTextItem
#define ICON(i)   ((SubIcon *)i)                                  ///< Cast to SubIcon
/* }}} */

/* Flags {{{ */
#define SUB_WINDOW_LEFT    0L                                     ///< Window left
#define SUB_WINDOW_DOWN    1L                                     ///< Window down
#define SUB_WINDOW_UP      2L                                     ///< Window above
#define SUB_WINDOW_RIGHT   3L                                     ///< Window right

#define SUB_EWMH_FULL      (1L << 1)                              ///< EWMH full flag
#define SUB_EWMH_FLOAT     (1L << 2)                              ///< EWMH float flag
#define SUB_EWMH_STICK     (1L << 3)                              ///< EWMH stick flag

#define SUB_MATCH_NAME     (1L << 1)                              ///< Match SUBTLE_NAME
#define SUB_MATCH_INSTANCE (1L << 2)                              ///< Match instance of WM_CLASS
#define SUB_MATCH_CLASS    (1L << 3)                              ///< Match class of WM_CLASS
#define SUB_MATCH_GRAVITY  (1L << 4)                              ///< Match gravity
#define SUB_MATCH_ROLE     (1L << 5)                              ///< Match window role
#define SUB_MATCH_PID      (1L << 6)                              ///< Match pid
/* }}} */

/* Typedefs {{{ */
#ifndef SUBTLER
typedef union subdata_t
{
  unsigned long num;
  char          *string;
} SubData;

typedef struct subtextitem_t
{
  int             icon, width, height;                            ///< Text width, height
  unsigned long   color;                                          ///< Text color

  union subdata_t data;                                           ///< Text data
} SubTextItem;

typedef struct subtext_t
{
  struct subtextitem_t **items;
  int                  nitems;
} SubText;

typedef struct subfont_t
{
  int y, height;
  XFontSet xfs;
} SubFont;

typedef struct subicon_t
{
  Pixmap       pixmap;
  unsigned int width, height;
  GC           gc;
} SubtIcon;
#endif /* SUBTLER */

#ifndef SUBTLE
typedef union submessagedata_t {
  char  b[20];
  short s[10];
  long  l[5];
} SubMessageData;

extern Display *display;

#endif /* SUBTLE */
/* }}} */

/* Log {{{ */
#ifdef DEBUG
#define subSharedLogDebug(...)  subSharedLog(0, __FILE__, __LINE__, __VA_ARGS__);
#else /* DEBUG */
#define subSharedLogDebug(...)
#endif /* DEBUG */

#define subSharedLogError(...)  subSharedLog(1, __FILE__, __LINE__,  __VA_ARGS__);
#define subSharedLogWarn(...)   subSharedLog(2, __FILE__, __LINE__, __VA_ARGS__);

void subSharedDebug(void);                                        ///< Enable debugging messages
void subSharedLog(int type, const char *file, 
  int line, const char *format, ...);                             ///< Print messages
int subSharedLogXError(Display *disp, XErrorEvent *ev);        ///< Print X error messages
/* }}} */

/* Memory {{{ */
void *subSharedMemoryAlloc(size_t n, size_t size);                ///< Allocate memory
void *subSharedMemoryRealloc(void *mem, size_t size);             ///< Reallocate memory
/* }}} */

/* Regex {{{ */
regex_t *subSharedRegexNew(char *regex);                          ///< Create new regex
int subSharedRegexMatch(regex_t *preg, char *string);             ///< Check if string matches preg
void subSharedRegexKill(regex_t *preg);                           ///< Kill regex
/* }}} */

/* Property {{{ */
char *subSharedPropertyGet(Display *disp, Window win, 
  Atom type, Atom prop, unsigned long *size);                     ///< Get window property
char **subSharedPropertyStrings(Display *disp, Window win,
  Atom prop, int *size);                                          ///< Get window property list
void subSharedPropertyName(Display *disp, Window win, 
  char **name, char *fallback);                                   ///< Get window name
void subSharedPropertyClass(Display *disp, Window win, 
  char **inst, char **klass);                                     ///< Get window class
void subSharedPropertyGeometry(Display *disp, Window win,
  XRectangle *geometry);                                          ///< Get window geometry
void subSharedPropertyDelete(Display *disp, Window win,
  Atom prop);                                                     ///< Delete window property
/* }}} */

#ifndef SUBTLER

/* Text {{{ */
SubText *subSharedTextNew(void);                                  ///< Create new text
int subSharedTextParse(Display *disp, SubFont *f,
  SubText *t, char *text);                                        ///< Parse text
void subSharedTextRender(Display *disp, GC gc, SubFont *f, 
  Window win, int x, int y, long fg, long bg, SubText *t);        ///< Render text
int subSharedTextWidth(SubFont *f, const char *text,
  int len, int *left, int *right, int center);                    ///< Get text width
void subSharedTextFree(SubText *t);                               ///< Free text

void subSharedTextDraw(Display *disp, GC gc, SubFont *f,
  Window win, int x, int y, long fg, long bg, const char *text);  ///< Draw text
/* }}} */

/* Font {{{ */
SubFont *subSharedFontNew(Display *disp, const char *name);       ///< Create font
void subSharedFontKill(Display *disp, SubFont *f);                ///< Kill font
/* }}} */

/* Misc {{{ */
unsigned long subSharedParseColor(Display *disp, char *name);     ///< Parse color
int subSharedMatch(int type, XRectangle *geometry1,
  XRectangle *geometry2);                                         ///< Match window
/* }}} */

#endif /* SUBTLER */

/* Misc {{{ */
pid_t subSharedSpawn(char *cmd);                                  ///< Spawn command
/* }}} */

#ifndef SUBTLE

/* Message {{{ */
int subSharedMessage(Window win, char *type,
  SubMessageData data, int xsync);                                ///< Send client message
/* }}} */

/* Window {{{ */
Window *subSharedWindowSUBTLECheck(void);                             ///< Get SUBTLE check window
Window subSharedWindowSelect(void);                               ///< Select a window
/* }}} */

/* Lists {{{ */
Window *subSharedClientList(int *size);                           ///< Get client list
Window *subSharedTrayList(int *size);                             ///< Get tray list
/* }}} */

/* Find {{{ */
int subSharedClientFind(char *match, char **name,
  Window *win, int flags);                                        ///< Find client id
int subSharedGravityFind(char *match, char **name,
  XRectangle *geometry);                                          ///< Find gravity id
int subSharedScreenFind(int id, XRectangle *geometry);            ///< Find screen id
int subSharedSubletFind(char *match, char **name);                ///< Find sublet id
int subSharedTagFind(char *match, char **name);                   ///< Find tag id
int subSharedTrayFind(char *match, char **name,
  Window *win, int flags);                                        ///< Find tray id
int subSharedViewFind(char *match, char **name,
  Window *win);                                                   ///< Find view id              
/* }}} */

/* Subtle {{{ */
int subSharedSubtleRunning(void);                                 ///< Check if subtle is running
/* }}} */

#endif /* SUBTLE */

#endif /* SHARED_H */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
