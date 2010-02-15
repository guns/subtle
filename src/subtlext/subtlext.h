
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
VALUE subClientFind(VALUE self, VALUE name);                      ///< Find client
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
VALUE subClientMatchUp(VALUE self);                               ///< Get client above
VALUE subClientMatchLeft(VALUE self);                             ///< Get client left
VALUE subClientMatchRight(VALUE self);                            ///< Get client right
VALUE subClientMatchDown(VALUE self);                             ///< Get client down
VALUE subClientFocus(VALUE self);                                 ///< Focus client
VALUE subClientFocusAsk(VALUE self);                              ///< Has client focus
VALUE subClientAliveAsk(VALUE self);                              ///< Is client alive
VALUE subClientGravityReader(VALUE self);                         ///< Get client gravity
VALUE subClientGravityWriter(VALUE self, VALUE value);            ///< Set client gravity
VALUE subClientScreenReader(VALUE self);                          ///< Get client screen
VALUE subClientScreenWriter(VALUE self, VALUE value);             ///< Set client screen
VALUE subClientGeometryReader(VALUE self);                        ///< Get client geometry
VALUE subClientGeometryWriter(int argc, VALUE *argv,
  VALUE self);                                                    ///< Set client geometry
VALUE subClientToString(VALUE self);                              ///< Client to string
VALUE subClientKill(VALUE self);                                  ///< Kill client
/* }}} */

/* geometry.c {{{ */
VALUE subGeometryInstantiate(int x, int y, int width, 
  int height);                                                    ///< Instantiate geometry
VALUE subGeometryInit(int argc, VALUE *argv, VALUE self);         ///< Create new geometry
VALUE subGeometryToArray(VALUE self);                             ///< Geometry to array
VALUE subGeometryToHash(VALUE self);                              ///< Geometry to hash
VALUE subGeometryToString(VALUE self);                            ///< Geometry to string
/* }}} */

/* gravity.c {{{ */
VALUE subGravityInstantiate(char *name);                          ///< Instantiate gravity
VALUE subGravityInit(VALUE self, VALUE name);                     ///< Create new gravity
VALUE subGravityFind(VALUE self, VALUE value);                    ///< Find gravity
VALUE subGravityAll(VALUE self);                                  ///< Get all gravities
VALUE subGravityUpdate(VALUE self);                               ///< Update gravity
VALUE subGravityGeometry(VALUE self);                             ///< Get geometry gravity
VALUE subGravityToString(VALUE self);                             ///< Gravity to string
VALUE subGravityToSym(VALUE self);                                ///< Gravity to symbol
VALUE subGravityKill(VALUE self);                                 ///< Kill gravity
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
VALUE subSubletToString(VALUE self);                              ///< Sublet to string
VALUE subSubletKill(VALUE self);                                  ///< Kill sublet
/* }}} */

/* subtle.c {{{ */
VALUE subSubtleDisplay(VALUE self);                               ///< Get display
VALUE subSubtleRunningAsk(VALUE self);                            ///< Is subtle running
VALUE subSubtleSpawn(VALUE self, VALUE cmd);                      ///< Spawn command
VALUE subSubtleFocus(VALUE self, VALUE name);                     ///< Focus a client
VALUE subSubtleClientDel(VALUE self, VALUE name);                 ///< Kill client
VALUE subSubtleGravityDel(VALUE self, VALUE value);               ///< Kill gravity
VALUE subSubtleTagAdd(VALUE self, VALUE value);                   ///< Add a tag
VALUE subSubtleTagDel(VALUE self, VALUE value);                   ///< Kill a tag
VALUE subSubtleTrayDel(VALUE self, VALUE value);                  ///< Kill a tray
VALUE subSubtleSubletDel(VALUE self, VALUE value);                ///< Kill a sublet
VALUE subSubtleViewAdd(VALUE self, VALUE value);                  ///< Add a view
VALUE subSubtleViewDel(VALUE self, VALUE value);                  ///< Kill a view
VALUE subSubtleReload(VALUE self);                                ///< Reload subtle
VALUE subSubtleQuit(VALUE self);                                  ///< Quit subtle
/* }}} */

/* subtlext.c {{{ */
void subSubtlextConnect(void);                                    ///< Connect to display
VALUE subSubtlextFind(int type, VALUE value, int exception);      ///< Find object
VALUE subSubtlextKill(VALUE value, int type);                     ///< Kill display
VALUE subSubtlextTagAdd(VALUE self, VALUE value);                 ///< Add tag to object
VALUE subSubtlextTagDel(VALUE self, VALUE value);                 ///< Del tag from object
VALUE subSubtlextTagList(VALUE self);                             ///< List tags from object
VALUE subSubtlextTagAssoc(VALUE self, int type);                  ///< Get tags from object
VALUE subSubtlextTagAsk(VALUE self, VALUE value);                 ///< Has object tag
VALUE subSubtlextFocus(VALUE self);                               ///< Focus object
VALUE subSubtlextFocusAsk(VALUE self);                            ///< Has object focus
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
VALUE subTrayFocus(VALUE self);                                   ///< Focus tray
VALUE subTrayFocusAsk(VALUE self);                                ///< Has tray focus
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
VALUE subViewCurrentAsk(VALUE self);                              ///< Is view the current
VALUE subViewToString(VALUE self);                                ///< View to string
VALUE subViewKill(VALUE self);                                    ///< Kill view
/* }}} */

/* window.c {{{ */
VALUE subWindowNew(VALUE self, VALUE options);                    ///< Create window
VALUE subWindowNameWriter(VALUE self, VALUE name);                ///< Set window name
VALUE subWindowBackgroundWriter(VALUE self, VALUE value);         ///< Set background
VALUE subWindowGeometryReader(VALUE self, VALUE value);           ///< Get window geometry
VALUE subWindowShow(VALUE self);                                  ///< Show window
VALUE subWindowHide(VALUE self);                                  ///< Hide window
VALUE subWindowPuts(int argc, VALUE *argv, VALUE self);           ///< Put text on window
VALUE subWindowKill(VALUE self);                                  ///< Kill window
/* }}} */

#endif /* SUBTLEXT_H */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
