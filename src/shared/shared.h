
 /**
  * @package subtle
  *
  * @file Header file
  * @copyright Copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
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

#ifdef WM
#include "subtle.h"
#endif /* WM */

#include "config.h"
/* }}} */

/* Flags {{{ */
#define SUB_GRAVITY_UNKNOWN       0L         ///< Gravity unknown
#define SUB_GRAVITY_BOTTOM_LEFT   1L         ///< Gravity bottom left
#define SUB_GRAVITY_BOTTOM        2L         ///< Gravity bottom
#define SUB_GRAVITY_BOTTOM_RIGHT  3L         ///< Gravity bottom right
#define SUB_GRAVITY_LEFT          4L         ///< Gravity left
#define SUB_GRAVITY_CENTER        5L         ///< Gravity center
#define SUB_GRAVITY_RIGHT         6L         ///< Gravity right
#define SUB_GRAVITY_TOP_LEFT      7L         ///< Gravity top left
#define SUB_GRAVITY_TOP           8L         ///< Gravity top
#define SUB_GRAVITY_TOP_RIGHT     9L         ///< Gravity top right

#define SUB_WINDOW_LEFT           0L         ///< Window left
#define SUB_WINDOW_DOWN           1L         ///< Window down
#define SUB_WINDOW_UP             2L         ///< Window above
#define SUB_WINDOW_RIGHT          3L         ///< Window right

#define SUB_EWMH_FULL             (1L << 1)  ///< EWMH full flag
#define SUB_EWMH_FLOAT            (1L << 2)  ///< EWMH float flag
#define SUB_EWMH_STICK            (1L << 3)  ///< EWMH stick flag
/* }}} */

/* Typedefs {{{ */
#ifndef WM
typedef union submessagedata_t {
  char  b[20];
  short s[10];
  long  l[5];
} SubMessageData;

extern Display *display;

#ifdef DEBUG
extern int debug;
#endif /* DEBUG */

#endif /* WM */
/* }}} */

/* Log {{{ */
#ifdef DEBUG
#define subSharedLogDebug(...)  subSharedLog(0, __FILE__, __LINE__, __VA_ARGS__);
#else /* DEBUG */
#define subSharedLogDebug(...)
#endif /* DEBUG */

#define subSharedLogError(...)  subSharedLog(1, __FILE__, __LINE__,  __VA_ARGS__);
#define subSharedLogWarn(...)   subSharedLog(2, __FILE__, __LINE__, __VA_ARGS__);

void subSharedLog(int type, const char *file, 
  int line, const char *format, ...);                             ///< Print messages
int subSharedLogXError(Display *display, XErrorEvent *ev);        ///< Print X error messages
/* }}} */

/* Alloc {{{ */
void *subSharedMemoryAlloc(size_t n, size_t size);                ///< Allocate memory
void *subSharedMemoryRealloc(void *mem, size_t size);             ///< Reallocate memory
/* }}} */

/* Regex {{{ */
regex_t *subSharedRegexNew(char *regex);                          ///< Create new regex
int subSharedRegexMatch(regex_t *preg, char *string);             ///< Check if string matches preg
void subSharedRegexKill(regex_t *preg);                           ///< Kill regex
/* }}} */

/* Match {{{ */
int subSharedMatch(int type, int gravity1, int gravity2);         ///< Match window
/* }}} */

/* Property {{{ */
#if WM
char *subSharedPropertyGet(Window win, Atom type,
  SubEwmh e, unsigned long *size);                                ///< Get window property
char **subSharedPropertyStrings(Window win,
  SubEwmh e, int *size);                                          ///< Get window property list
void subSharedPropertyDelete(Window win, SubEwmh e);              ///< Delete window property
#else
char *subSharedPropertyGet(Window win, Atom type,
  char *name, unsigned long *size);                               ///< Get window property
char **subSharedPropertyStrings(Window win,
  char *name, int *size);                                         ///< Get window property list
void subSharedPropertyDelete(Window win, char *name);             ///< Delete window property
#endif /* WM */

void subSharedPropertyClass(Window win, char **inst,
  char **klass);                                                  ///< Get WM class
/* }}} */

#ifdef WM
/* Subtle {{{ */
XPointer * subSharedFind(Window win, XContext id);                ///< Find data with context manager
time_t subSharedTime(void);                                       ///< Get current time 
void subSharedFocus(void);                                        ///< Get pointer window and focus it
/* }}} */
#else /* WM */
/* Message {{{ */
int subSharedMessage(Window win, char *type,
  SubMessageData data, int sync);                                 ///< Send client message
/* }}} */

/* Window {{{ */
Window *subSharedWindowWMCheck(void);                             ///< Get WM check window
Window subSharedWindowSelect(void);                               ///< Select a window
/* }}} */

/* Client {{{ */
Window *subSharedClientList(int *size);                           ///< Get client list
int subSharedClientFind(char *name, Window *win);                 ///< Find client id
/* }}} */

/* Tag {{{ */
int subSharedTagFind(char *name);                                 ///< Find tag id
/* }}} */

/* View {{{ */
int subSharedViewFind(char *name, Window *win);                   ///< Find view id              
/* }}} */

/* Sublet {{{ */
int subSharedSubletFind(char *name);                              ///< Find sublet id
/* }}} */

/* Subtle {{{ */
int subSharedSubtleRunning(void);                                 ///< Check if subtle is running
/* }}} */
#endif /* WM */

#endif /* SHARED_H */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
