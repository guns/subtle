
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
#define SUB_GRAVITY_UNKNOWN       0  ///< Gravity unknown
#define SUB_GRAVITY_BOTTOM_LEFT   1  ///< Gravity bottom left
#define SUB_GRAVITY_BOTTOM        2  ///< Gravity bottom
#define SUB_GRAVITY_BOTTOM_RIGHT  3  ///< Gravity bottom right
#define SUB_GRAVITY_LEFT          4  ///< Gravity left
#define SUB_GRAVITY_CENTER        5  ///< Gravity center
#define SUB_GRAVITY_RIGHT         6  ///< Gravity right
#define SUB_GRAVITY_TOP_LEFT      7  ///< Gravity top left
#define SUB_GRAVITY_TOP           8  ///< Gravity top
#define SUB_GRAVITY_TOP_RIGHT     9  ///< Gravity top right

#define SUB_WINDOW_UP             0  ///< Window above
#define SUB_WINDOW_LEFT           1  ///< Window left
#define SUB_WINDOW_RIGHT          2  ///< Window right
#define SUB_WINDOW_DOWN           3  ///< Window down
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
void subSharedMatch(int type, Window win, int gravity1, 
  int gravity2, int *match, Window *found);                       ///< Match window
/* }}} */

#ifdef WM
/* Subtle {{{ */
XPointer * subSharedFind(Window win, XContext id);                ///< Find data with context manager
time_t subSharedTime(void);                                       ///< Get current time 
/* }}} */
#endif /* WM */

#ifndef WM
/* Message {{{ */
int subSharedMessage(Window win, char *type,                      
  SubMessageData data, int sync);                                 ///< Send client message
/* }}} */

/* Property {{{ */
char *subSharedPropertyGet(Window win, Atom type, 
  char *name, unsigned long *size);                               ///< Get window property
char **subSharedPropertyList(Window win, char *name,
  int *size);                                                     ///< Get property list
void subSharedPropertyListFree(char **list, int size);            ///< Free property list
/* }}} */

/* Window {{{ */
Window *subSharedWindowWMCheck(void);                             ///< Get WM check window
char *subSharedWindowWMName(Window win);                          ///< Get WM name
char *subSharedWindowWMClass(Window win);                         ///< Get wM class
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
