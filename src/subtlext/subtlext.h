
 /**
  * @package subtlext
  *
  * @file Header file
  * @copyright Copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#ifndef SUBTLEXT_H
#define SUBTLEXT_H 1

/* Includes {{{ */
#include <ruby.h>
#include "shared.h"
/* }}} */

/* Macros {{{ */
#define CHAR2SYM(name) ID2SYM(rb_intern(name))
#define SYM2CHAR(sym)  rb_id2name(SYM2ID(sym))

#define GET_ATTR(owner,name,value) \
  if(NIL_P(value = rb_iv_get(owner, name))) return Qnil;

#define ROOT DefaultRootWindow(display)
/* }}} */

/* Flags {{{ */
#define SUB_TYPE_CLIENT  0           ///< Client
#define SUB_TYPE_GRAVITY 1           ///< Gravity
#define SUB_TYPE_VIEW    2           ///< View
#define SUB_TYPE_TAG     3           ///< Tag
#define SUB_TYPE_TRAY    4           ///< Tray
#define SUB_TYPE_SCREEN  5           ///< Screen
#define SUB_TYPE_SUBLET  6           ///< Sublet
/* }}} */

/* Typedefs {{{ */
extern Display *display;
extern VALUE mod;

#ifdef DEBUG
extern int debug;
#endif /* DEBUG */
/* }}} */

/* client.c {{{ */
/* Singleton */
VALUE subClientSingSelect(VALUE self);                            ///< Select client
VALUE subClientSingFind(VALUE self, VALUE value);                 ///< Find client
VALUE subClientSingCurrent(VALUE self);                           ///< Get current client
VALUE subClientSingVisible(VALUE self);                           ///< Get all visible clients
VALUE subClientSingAll(VALUE self);                               ///< Get all clients
VALUE subClientSingRecent(VALUE self);                            ///< Get recent active clients

/* Class */
VALUE subClientInstantiate(Window win);                           ///< Instantiate client
VALUE subClientInit(VALUE self, VALUE win);                       ///< Create client
VALUE subClientUpdate(VALUE self);                                ///< Update client
VALUE subClientViewList(VALUE self);                              ///< Get views clients is on
VALUE subClientFlagsAskFull(VALUE self);                          ///< Is client fullscreen
VALUE subClientFlagsAskFloat(VALUE self);                         ///< Is client floating
VALUE subClientFlagsAskStick(VALUE self);                         ///< Is client stick
VALUE subClientFlagsAskResize(VALUE self);                        ///< Is client resize
VALUE subClientFlagsAskUrgent(VALUE self);                        ///< Is client urgent
VALUE subClientFlagsAskZaphod(VALUE self);                        ///< Is client zaphod
VALUE subClientFlagsAskFixed(VALUE self);                         ///< Is client fixed
VALUE subClientFlagsAskBorderless(VALUE self);                    ///< Is client borderless
VALUE subClientFlagsToggleFull(VALUE self);                       ///< Toggle fullscreen
VALUE subClientFlagsToggleFloat(VALUE self);                      ///< Toggle floating
VALUE subClientFlagsToggleStick(VALUE self);                      ///< Toggle stick
VALUE subClientFlagsToggleResize(VALUE self);                     ///< Toggle resize
VALUE subClientFlagsToggleUrgent(VALUE self);                     ///< Toggle urgent
VALUE subClientFlagsToggleZaphod(VALUE self);                     ///< Toggle zaphod
VALUE subClientFlagsToggleFixed(VALUE self);                      ///< Toggle fixed
VALUE subClientFlagsToggleBorderless(VALUE self);                 ///< Toggle borderless
VALUE subClientFlagsWriter(VALUE self, VALUE value);              ///< Set multiple flags
VALUE subClientRestackRaise(VALUE self);                          ///< Raise client
VALUE subClientRestackLower(VALUE self);                          ///< Lower client
VALUE subClientAskAlive(VALUE self);                              ///< Is client alive
VALUE subClientGravityReader(VALUE self);                         ///< Get client gravity
VALUE subClientGravityWriter(VALUE self, VALUE value);            ///< Set client gravity
VALUE subClientGeometryReader(VALUE self);                        ///< Get client geometry
VALUE subClientGeometryWriter(int argc, VALUE *argv,
  VALUE self);                                                    ///< Set client geometry
VALUE subClientScreenReader(VALUE self);                          ///< Get client screen
VALUE subClientResizeWriter(VALUE self, VALUE value);             ///< Set Client resize
VALUE subClientToString(VALUE self);                              ///< Client to string
VALUE subClientKill(VALUE self);                                  ///< Kill client
/* }}} */

/* color.c {{{ */
unsigned long subColorPixel(VALUE red, VALUE green,
  VALUE blue, XColor *xcolor);                                    ///< Get pixel value
VALUE subColorInstantiate(unsigned long pixel);                   ///< Instantiate color
VALUE subColorInit(int argc, VALUE *argv, VALUE self);            ///< Create new color
VALUE subColorToHex(VALUE self);                                  ///< Convert to hex string
VALUE subColorToArray(VALUE self);                                ///< Color to array
VALUE subColorToHash(VALUE self);                                 ///< Color to hash
VALUE subColorToString(VALUE self);                               ///< Convert to string
VALUE subColorOperatorPlus(VALUE self, VALUE value);              ///< Concat string
VALUE subColorEqual(VALUE self, VALUE other);                     ///< Whether objects are equal
VALUE subColorEqualTyped(VALUE self, VALUE other);                ///< Whether objects are equal typed
/* }}} */

/* geometry.c {{{ */
VALUE subGeometryInstantiate(int x, int y, int width,
  int height);                                                    ///< Instantiate geometry
void subGeometryToRect(VALUE self, XRectangle *r);                ///< Geometry to rect
VALUE subGeometryInit(int argc, VALUE *argv, VALUE self);         ///< Create new geometry
VALUE subGeometryToArray(VALUE self);                             ///< Geometry to array
VALUE subGeometryToHash(VALUE self);                              ///< Geometry to hash
VALUE subGeometryToString(VALUE self);                            ///< Geometry to string
VALUE subGeometryEqual(VALUE self, VALUE other);                  ///< Whether objects are equal
VALUE subGeometryEqualTyped(VALUE self, VALUE other);             ///< Whether objects are equal typed
/* }}} */

/* gravity.c {{{ */
/* Singleton */
VALUE subGravitySingFind(VALUE self, VALUE value);                ///< Find gravity
VALUE subGravitySingAll(VALUE self);                              ///< Get all gravities

/* Class */
VALUE subGravityInstantiate(char *name);                          ///< Instantiate gravity
VALUE subGravityInit(int argc, VALUE *argv, VALUE self);          ///< Create new gravity
VALUE subGravityUpdate(VALUE self);                               ///< Update gravity
VALUE subGravityClients(VALUE self);                              ///< List clients with gravity
VALUE subGravityGeometryReader(VALUE self);                       ///< Get geometry gravity
VALUE subGravityGeometryWriter(VALUE self, VALUE value);          ///< Set geometry gravity
VALUE subGravityGeometryFor(VALUE self, VALUE value);             ///< Get geometry gravity for screen
VALUE subGravityToString(VALUE self);                             ///< Gravity to string
VALUE subGravityToSym(VALUE self);                                ///< Gravity to symbol
VALUE subGravityKill(VALUE self);                                 ///< Kill gravity
/* }}} */

/* icon.c {{{ */
VALUE subIconAlloc(VALUE self);                                   ///< Allocate icon
VALUE subIconInit(int argc, VALUE *argv, VALUE self);             ///< Init icon
VALUE subIconDrawPoint(int argc, VALUE *argv, VALUE self);        ///< Draw a point
VALUE subIconDrawLine(int argc, VALUE *argv, VALUE self);         ///< Draw a line
VALUE subIconDrawRect(int argc, VALUE *argv, VALUE self);         ///< Draw a rect
VALUE subIconCopyArea(int argc, VALUE *argv, VALUE self);         ///< Copy icon area
VALUE subIconClear(int argc, VALUE *argv, VALUE self);            ///< Clear icon
VALUE subIconAskBitmap(VALUE self);                               ///< Whether icon is bitmap
VALUE subIconToString(VALUE self);                                ///< Convert to string
VALUE subIconOperatorPlus(VALUE self, VALUE value);               ///< Concat string
VALUE subIconOperatorMult(VALUE self, VALUE value);               ///< Concat string
VALUE subIconEqual(VALUE self, VALUE other);                      ///< Whether objects are equal
VALUE subIconEqualTyped(VALUE self, VALUE other);                 ///< Whether objects are equal typed
/* }}} */

/* screen.c {{{ */
/* Singleton */
VALUE subScreenSingFind(VALUE self, VALUE id);                    ///< Find screen
VALUE subScreenSingAll(VALUE self);                               ///< Get all screens
VALUE subScreenSingCurrent(VALUE self);                           ///< Get current screen

/* Class */
VALUE subScreenInstantiate(int id);                               ///< Instantiate screen
VALUE subScreenInit(VALUE self, VALUE id);                        ///< Create new screen
VALUE subScreenUpdate(VALUE self);                                ///< Update screen
VALUE subScreenJump(VALUE self);                                  ///< Jump to this screen
VALUE subScreenViewReader(VALUE self);                            ///< Get screen view
VALUE subScreenViewWriter(VALUE self, VALUE value);               ///< Set screen view
VALUE subScreenAskCurrent(VALUE self);                            ///< Whether screen is current
VALUE subScreenToString(VALUE self);                              ///< Screen to string
/* }}} */

/* sublet.c {{{ */
/* Singleton */
VALUE subSubletSingFind(VALUE self, VALUE value);                 ///< Find sublet
VALUE subSubletSingAll(VALUE self);                               ///< Get all sublets

/* Class */
VALUE subSubletInstantiate(char *name);                           ///< Instantiate sublet
VALUE subSubletInit(VALUE self, VALUE name);                      ///< Create sublet
VALUE subSubletUpdate(VALUE self);                                ///< Update sublet
VALUE subSubletDataReader(VALUE self);                            ///< Get sublet data
VALUE subSubletDataWriter(VALUE self, VALUE value);               ///< Set sublet data
VALUE subSubletVisibilityShow(VALUE self);                        ///< Show sublet
VALUE subSubletVisibilityHide(VALUE self);                        ///< Hide sublet
VALUE subSubletGeometryReader(VALUE self);                        ///< Get sublet geometry
VALUE subSubletToString(VALUE self);                              ///< Sublet to string
VALUE subSubletKill(VALUE self);                                  ///< Kill sublet
/* }}} */

/* subtle.c {{{ */
/* Singleton */
VALUE subSubtleSingDisplayReader(VALUE self);                     ///< Get display
VALUE subSubtleSingDisplayWriter(VALUE self, VALUE display);      ///< Set display
VALUE subSubtleSingAskRunning(VALUE self);                        ///< Is subtle running
VALUE subSubtleSingSelect(VALUE self);                            ///< Select window
VALUE subSubtleSingRender(VALUE self);                            ///< Render panels
VALUE subSubtleSingReload(VALUE self);                            ///< Reload config and sublets
VALUE subSubtleSingRestart(VALUE self);                           ///< Restart subtle
VALUE subSubtleSingQuit(VALUE self);                              ///< Quit subtle
VALUE subSubtleSingColors(VALUE self);                            ///< Get colors
VALUE subSubtleSingFont(VALUE self);                              ///< Get font
VALUE subSubtleSingSpawn(VALUE self, VALUE cmd);                  ///< Spawn command
/* }}} */

/* subtlext.c {{{ */
void subSubtlextConnect(char *display_string);                    ///< Connect to display
void subSubtlextBacktrace(void);                                  ///< Print ruby backtrace
VALUE subSubtlextConcat(VALUE str1, VALUE str2);                  ///< Concat strings
VALUE subSubtlextParse(VALUE value, char *buf,
  int len, int *flags);                                           ///< Parse arguments
VALUE subSubtlextOneOrMany(VALUE obj, VALUE recent);              ///< Return one or many
Window *subSubtlextWindowList(char *prop_name, int *size);        ///< Get window list
int subSubtlextWindowMatch(Window win, regex_t *preg,
  const char *source, char **name, int flags);                    ///< Match window
int subSubtlextFindString(char *prop_name, char *source,
  char **name, int flags);                                        ///< Find string id
VALUE subSubtlextFindObjects(char *prop_name, char *class_name,
  char *source, int flags);                                       ///< Find objects
/* }}} */

/* tag.c {{{ */
/* Singleton */
VALUE subTagSingFind(VALUE self, VALUE value);                    ///< Find tag
VALUE subTagSingVisible(VALUE self);                              ///< Get all visible tags
VALUE subTagSingAll(VALUE self);                                  ///< Get all tags

/* Class */
VALUE subTagInstantiate(char *name);                              ///< Instantiate tag
VALUE subTagInit(VALUE self, VALUE name);                         ///< Create tag
VALUE subTagUpdate(VALUE self);                                   ///< Update tag
VALUE subTagClients(VALUE self);                                  ///< Get clients with tag
VALUE subTagViews(VALUE self);                                    ///< Get views with tag
VALUE subTagToString(VALUE self);                                 ///< Tag to string
VALUE subTagKill(VALUE self);                                     ///< Kill tag
/* }}} */

/* tray.c {{{ */
/* Singleton */
VALUE subTraySingFind(VALUE self, VALUE name);                    ///< Find tray
VALUE subTraySingAll(VALUE self);                                 ///< Get all trays

/* Class */
VALUE subTrayInstantiate(Window win);                             ///< Instantiate tray
VALUE subTrayInit(VALUE self, VALUE win);                         ///< Create tray
VALUE subTrayUpdate(VALUE self);                                  ///< Update tray
VALUE subTrayToString(VALUE self);                                ///< Tray to string
VALUE subTrayKill(VALUE self);                                    ///< Kill tray
/* }}} */

/* view.c {{{ */
/* Singleton */
VALUE subViewSingFind(VALUE self, VALUE name);                    ///< Find view
VALUE subViewSingCurrent(VALUE self);                             ///< Get current view
VALUE subViewSingVisible(VALUE self);                             ///< Get all visible views
VALUE subViewSingAll(VALUE self);                                 ///< Get all views

/* Class */
VALUE subViewInstantiate(char *name);                             ///< Instantiate view
VALUE subViewInit(VALUE self, VALUE name);                        ///< Create view
VALUE subViewUpdate(VALUE self);                                  ///< Update view
VALUE subViewClients(VALUE self);                                 ///< Get clients of view
VALUE subViewJump(VALUE self);                                    ///< Jump to view
VALUE subViewSelectNext(VALUE self);                              ///< Select next view
VALUE subViewSelectPrev(VALUE self);                              ///< Select next view
VALUE subViewAskCurrent(VALUE self);                              ///< Whether view the current
VALUE subViewIcon(VALUE self);                                    ///< View icon if any
VALUE subViewToString(VALUE self);                                ///< View to string
VALUE subViewKill(VALUE self);                                    ///< Kill view
/* }}} */

/* window.c {{{ */
/* Singleton */
VALUE subWindowSingOnce(VALUE self, VALUE geometry);              ///< Run window once

/* Class */
VALUE subWindowInstantiate(VALUE geometry);                       ///< Instantiate window
VALUE subWindowDispatcher(int argc, VALUE *argv, VALUE self);     ///< Window dispatcher
VALUE subWindowAlloc(VALUE self);                                 ///< Allocate window
VALUE subWindowInit(VALUE self, VALUE geometry);                  ///< Init window
VALUE subWindowSubwindow(VALUE self, VALUE geometry);             ///< Create a subwindow
VALUE subWindowNameWriter(VALUE self, VALUE value);               ///< Set name
VALUE subWindowFontWriter(VALUE self, VALUE value);               ///< Set font
VALUE subWindowFontYReader(VALUE self);                           ///< Get y offset of font
VALUE subWindowFontHeightReader(VALUE self);                      ///< Get height of font
VALUE subWindowForegroundWriter(VALUE self, VALUE value);         ///< Set foreground
VALUE subWindowBackgroundWriter(VALUE self, VALUE value);         ///< Set background
VALUE subWindowBorderColorWriter(VALUE self, VALUE value);        ///< Set border color
VALUE subWindowBorderSizeWriter(VALUE self, VALUE value);         ///< Set border size
VALUE subWindowGeometryReader(VALUE self);                        ///< Get geometry
VALUE subWindowGeometryWriter(VALUE self, VALUE value);           ///< Set geometry
VALUE subWindowWrite(VALUE self, VALUE x, VALUE y, VALUE text);   ///< Write text
VALUE subWindowRead(int argc, VALUE *argv, VALUE self);           ///< Read text
VALUE subWindowGrabKeys(VALUE self);                              ///< Grab key events
VALUE subWindowGrabMouse(VALUE self);                             ///< Grab mouse events
VALUE subWindowClear(int argc, VALUE *argv, VALUE self);          ///< Clear area or window
VALUE subWindowRedraw(VALUE self);                                ///< Redraw window
VALUE subWindowCompletion(VALUE self);                            ///< Add completion proc
VALUE subWindowInput(VALUE self);                                 ///< Add input proc
VALUE subWindowRaise(VALUE self);                                 ///< Raise window
VALUE subWindowLower(VALUE self);                                 ///< Lower window
VALUE subWindowShow(VALUE self);                                  ///< Show window
VALUE subWindowHide(VALUE self);                                  ///< Hide window
VALUE subWindowAskHidden(VALUE self);                             ///< Whether window is hidden
VALUE subWindowKill(VALUE self);                                  ///< Kill window
/* }}} */

#endif /* SUBTLEXT_H */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
