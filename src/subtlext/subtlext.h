
 /**
  * @package subtlext
  *
  * @file Header file
  * @copyright Copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
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
VALUE subClientInstantiate(Window win);                           ///< Instantiate client
VALUE subClientInit(VALUE self, VALUE win);                       ///< Create client
VALUE subClientFind(VALUE self, VALUE value);                     ///< Find client
VALUE subClientCurrent(VALUE self);                               ///< Get current client
VALUE subClientAll(VALUE self);                                   ///< Get all clients
VALUE subClientUpdate(VALUE self);                                ///< Update client
VALUE subClientViewList(VALUE self);                              ///< Get views clients is on
VALUE subClientStateFullAsk(VALUE self);                          ///< Is client fullscreen
VALUE subClientStateFloatAsk(VALUE self);                         ///< Is client floating
VALUE subClientStateStickAsk(VALUE self);                         ///< Is client stick
VALUE subClientToggleFull(VALUE self);                            ///< Toggle fullscreen
VALUE subClientToggleFloat(VALUE self);                           ///< Toggle floating
VALUE subClientToggleStick(VALUE self);                           ///< Toggle stick
VALUE subClientRestackRaise(VALUE self);                          ///< Raise client
VALUE subClientRestackLower(VALUE self);                          ///< Lower client
VALUE subClientSelectUp(VALUE self);                              ///< Get client above
VALUE subClientSelectLeft(VALUE self);                            ///< Get client left
VALUE subClientSelectRight(VALUE self);                           ///< Get client right
VALUE subClientSelectDown(VALUE self);                            ///< Get client down
VALUE subClientAliveAsk(VALUE self);                              ///< Is client alive
VALUE subClientScreenReader(VALUE self);                          ///< Get client screen
VALUE subClientScreenWriter(VALUE self, VALUE value);             ///< Set client screen
VALUE subClientGravityReader(VALUE self);                         ///< Get client gravity
VALUE subClientGravityWriter(VALUE self, VALUE value);            ///< Set client gravity
VALUE subClientGeometryReader(VALUE self);                        ///< Get client geometry
VALUE subClientGeometryWriter(int argc, VALUE *argv,
  VALUE self);                                                    ///< Set client geometry
VALUE subClientShow(VALUE self);                                  ///< Show client
VALUE subClientHide(VALUE self);                                  ///< Hide client
VALUE subClientHiddenAsk(VALUE self);                             ///< Whether client is hidden
VALUE subClientResizeWriter(VALUE self, VALUE value);             ///< Set Client resize
VALUE subClientToString(VALUE self);                              ///< Client to string
VALUE subClientKill(VALUE self);                                  ///< Kill client
/* }}} */

/* color.c {{{ */
unsigned long subColorPixel(VALUE value);                         ///< Get pixel value
VALUE subColorInstantiate(unsigned long pixel);                   ///< Instantiate color
VALUE subColorInit(VALUE self, VALUE value);                      ///< Create new color
VALUE subColorToHex(VALUE self);                                  ///< Convert to hex string
VALUE subColorToString(VALUE self);                               ///< Convert to string
VALUE subColorOperatorPlus(VALUE self, VALUE value);              ///< Concat string
/* }}} */

/* geometry.c {{{ */
VALUE subGeometryInstantiate(int x, int y, int width,
  int height);                                                    ///< Instantiate geometry
void subGeometryToRect(VALUE self, XRectangle *r);                ///< Geometry to rect
VALUE subGeometryInit(int argc, VALUE *argv, VALUE self);         ///< Create new geometry
VALUE subGeometryToArray(VALUE self);                             ///< Geometry to array
VALUE subGeometryToHash(VALUE self);                              ///< Geometry to hash
VALUE subGeometryToString(VALUE self);                            ///< Geometry to string
/* }}} */

/* gravity.c {{{ */
int subGravityFindId(char *match, char **name,
  XRectangle *geometry);                                          ///< Find gravity id
VALUE subGravityInstantiate(char *name);                          ///< Instantiate gravity
VALUE subGravityInit(int argc, VALUE *argv, VALUE self);          ///< Create new gravity
VALUE subGravityFind(VALUE self, VALUE value);                    ///< Find gravity
VALUE subGravityAll(VALUE self);                                  ///< Get all gravities
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
VALUE subIconDraw(VALUE self, VALUE x, VALUE y);                  ///< Draw a pixel
VALUE subIconDrawRect(int argc, VALUE *argv, VALUE self);         ///< Draw a rect
VALUE subIconClear(VALUE self);                                   ///< Clear icon
VALUE subIconToString(VALUE self);                                ///< Convert to string
VALUE subIconOperatorPlus(VALUE self, VALUE value);               ///< Concat string
VALUE subIconOperatorMult(VALUE self, VALUE value);               ///< Concat string
/* }}} */

/* screen.c {{{ */
VALUE subScreenInstantiate(int id);                               ///< Instantiate screen
VALUE subScreenInit(VALUE self, VALUE id);                        ///< Create new screen
VALUE subScreenFind(VALUE self, VALUE id);                        ///< Find screen
VALUE subScreenAll(VALUE self);                                   ///< Get all screens
VALUE subScreenCurrent(VALUE self);                               ///< Get current screen
VALUE subScreenUpdate(VALUE self);                                ///< Update screen
VALUE subScreenClientList(VALUE self);                            ///< Get client list of screen
VALUE subScreenToString(VALUE self);                              ///< Screen to string
/* }}} */

/* sublet.c {{{ */
VALUE subSubletInstantiate(char *name);                           ///< Instantiate sublet
VALUE subSubletInit(VALUE self, VALUE name);                      ///< Create sublet
VALUE subSubletFind(VALUE self, VALUE value);                     ///< Find sublet
VALUE subSubletAll(VALUE self);                                   ///< Get all sublets
VALUE subSubletUpdate(VALUE self);                                ///< Update sublet
VALUE subSubletDataReader(VALUE self);                            ///< Get sublet data
VALUE subSubletDataWriter(VALUE self, VALUE value);               ///< Set sublet data
VALUE subSubletBackgroundWriter(VALUE self, VALUE value);         ///< Set sublet background
VALUE subSubletGeometryReader(VALUE self);                        ///< Get sublet geometry
VALUE subSubletToString(VALUE self);                              ///< Sublet to string
VALUE subSubletKill(VALUE self);                                  ///< Kill sublet
/* }}} */

/* subtle.c {{{ */
VALUE subSubtleDisplayReader(VALUE self);                         ///< Get display
VALUE subSubtleRunningAsk(VALUE self);                            ///< Is subtle running
VALUE subSubtleSelect(VALUE self);                                ///< Select window
VALUE subSubtleReloadConfig(VALUE self);                          ///< Reload config
VALUE subSubtleReloadSublets(VALUE self);                         ///< Reload sublets
VALUE subSubtleRestart(VALUE self);                               ///< Restart subtle
VALUE subSubtleQuit(VALUE self);                                  ///< Quit subtle
VALUE subSubtleColors(VALUE self);                                ///< Get colors
VALUE subSubtleSpawn(VALUE self, VALUE cmd);                      ///< Spawn command
/* }}} */

/* subtlext.c {{{ */
void subSubtlextConnect(void);                                    ///< Connect to display
void subSubtlextBacktrace(void);                                  ///< Print ruby backtrace
VALUE subSubtlextConcat(VALUE str1, VALUE str2);                  ///< Concat strings
VALUE subSubtlextParse(VALUE value, char *buf,
  int len, int *flags);                                           ///< Parse arguments
Window *subSubtlextList(char *prop, int *size);                   ///< Get window list
int subSubtlextFind(char *prop, char *match, char **name);        ///< Find object
int subSubtlextFindWindow(char *prop, char *match, char **name,
  Window *win, int flags);                                        ///< Find window
VALUE subSubtlextAssoc(VALUE self, int type);                     ///< Get tags from object
Window *subSubtlextWMCheck(void);                                 ///< Get WM check window
/* }}} */

/* tag.c {{{ */
VALUE subTagInstantiate(char *name);                              ///< Instantiate tag
VALUE subTagInit(VALUE self, VALUE name);                         ///< Create tag
VALUE subTagFind(VALUE self, VALUE value);                        ///< Find tag
VALUE subTagAll(VALUE self);                                      ///< Get all tags
VALUE subTagUpdate(VALUE self);                                   ///< Update tag
VALUE subTagClients(VALUE self);                                  ///< Get clients with tag
VALUE subTagViews(VALUE self);                                    ///< Get views with tag
VALUE subTagToString(VALUE self);                                 ///< Tag to string
VALUE subTagKill(VALUE self);                                     ///< Kill tag
/* }}} */

/* tray.c {{{ */
VALUE subTrayInstantiate(Window win);                             ///< Instantiate tray
VALUE subTrayInit(VALUE self, VALUE win);                         ///< Create tray
VALUE subTrayFind(VALUE self, VALUE name);                        ///< Find tray
VALUE subTrayAll(VALUE self);                                     ///< Get all trays
VALUE subTrayUpdate(VALUE self);                                  ///< Update tray
VALUE subTrayToString(VALUE self);                                ///< Tray to string
VALUE subTrayKill(VALUE self);                                    ///< Kill tray
/* }}} */

/* view.c {{{ */
VALUE subViewInstantiate(char *name);                             ///< Instantiate view
VALUE subViewInit(VALUE self, VALUE name);                        ///< Create view
VALUE subViewFind(VALUE self, VALUE name);                        ///< Find view
VALUE subViewCurrent(VALUE self);                                 ///< Get current view
VALUE subViewAll(VALUE self);                                     ///< Get all views
VALUE subViewUpdate(VALUE self);                                  ///< Update view
VALUE subViewClients(VALUE self);                                 ///< Get clients of view
VALUE subViewJump(VALUE self);                                    ///< Jump to view
VALUE subViewSelectNext(VALUE self);                              ///< Select next view
VALUE subViewSelectPrev(VALUE self);                              ///< Select next view
VALUE subViewCurrentAsk(VALUE self);                              ///< Is view the current
VALUE subViewToString(VALUE self);                                ///< View to string
VALUE subViewKill(VALUE self);                                    ///< Kill view
/* }}} */

/* window.c {{{ */
VALUE subWindowInstantiate(VALUE geometry);                       ///< Instantiate window
VALUE subWindowDispatcher(int argc, VALUE *argv, VALUE self);     ///< Window dispatcher
VALUE subWindowAlloc(VALUE self);                                 ///< Allocate window
VALUE subWindowInit(VALUE self, VALUE geometry);                  ///< Init window
VALUE subWindowNameWriter(VALUE self, VALUE value);               ///< Set name
VALUE subWindowFontWriter(VALUE self, VALUE value);               ///< Set font
VALUE subWindowForegroundWriter(VALUE self, VALUE value);         ///< Set foreground
VALUE subWindowBackgroundWriter(VALUE self, VALUE value);         ///< Set background
VALUE subWindowBorderColorWriter(VALUE self, VALUE value);        ///< Set border color
VALUE subWindowBorderSizeWriter(VALUE self, VALUE value);         ///< Set border size
VALUE subWindowGeometryReader(VALUE self);                        ///< Get geometry
VALUE subWindowGeometryWriter(VALUE self, VALUE value);           ///< Set geometry
VALUE subWindowWrite(VALUE self, VALUE x, VALUE y, VALUE text);   ///< Write text
VALUE subWindowRead(VALUE self, VALUE x, VALUE y);                ///< Read text
VALUE subWindowClear(int argc, VALUE *argv, VALUE self);          ///< Clear area or window
VALUE subWindowRedraw(VALUE self);                                ///< Redraw window
VALUE subWindowCompletion(VALUE self);                            ///< Add completion proc
VALUE subWindowInput(VALUE self);                                 ///< Add input proc
VALUE subWindowOnce(VALUE self, VALUE geometry);                  ///< Run window once
VALUE subWindowShow(VALUE self);                                  ///< Show window
VALUE subWindowHide(VALUE self);                                  ///< Hide window
VALUE subWindowHiddenAsk(VALUE self);                             ///< Whether window is hidden
VALUE subWindowKill(VALUE self);                                  ///< Kill window
/* }}} */

#endif /* SUBTLEXT_H */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
