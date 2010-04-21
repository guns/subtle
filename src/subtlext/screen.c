 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtlext.h"

/* ScreenList {{{ */
VALUE
ScreenList(void)
{
  int n = 0;
  unsigned long len = 0;
  VALUE method = Qnil, klass = Qnil, array = Qnil, screen = Qnil, geometry = Qnil;
  XRectangle workarea = { 0 };
  long *workareas = NULL;

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  subSubtlextConnect(); ///< Implicit open connection

  /* Get workarea list */
  if((workareas = (long *)subSharedPropertyGet(display, DefaultRootWindow(display),
      XA_CARDINAL, XInternAtom(display, "_NET_WORKAREA", False), &len)))
    {
        workarea.x      = workareas[0];
        workarea.y      = workareas[1];
        workarea.width  = workareas[2];
        workarea.height = workareas[3];

        free(workareas);
    }
  else subSharedLogDebug("Failed getting workarea list\n");

  method = rb_intern("new");
  klass  = rb_const_get(mod, rb_intern("Screen"));
  array  = rb_ary_new();

  /* Create first screen */
  screen   = rb_funcall(klass, method, 1, INT2FIX(0));
  geometry = subGeometryInstantiate(workarea.x, workarea.y,
    workarea.width, workarea.height);

  rb_iv_set(screen, "@geometry", geometry);
  rb_ary_push(array, screen);

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  /* Xinerama */
  if(XineramaQueryExtension(display, &xinerama_event, &xinerama_error) &&
    XineramaIsActive(display))
    {
      int i;
      XineramaScreenInfo *screens = NULL;

      /* Query screens */
      if((screens = XineramaQueryScreens(display, &n)))
        {
          for(i = 1; i < n; i++) ///< Start with second
            {
              screen   = rb_funcall(klass, method, 1, INT2FIX(i));
              geometry = subGeometryInstantiate(screens[i].x_org,
                screens[i].y_org, screens[i].width, screens[i].height);

              rb_iv_set(screen, "@geometry", geometry);
              rb_ary_push(array, screen);
            }

          XFree(screens);
        }
    }
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  return array;
} /* }}} */

/* subScreenInstantiate {{{ */
VALUE
subScreenInstantiate(int id)
{
  VALUE klass = Qnil, screen = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Screen"));
  screen = rb_funcall(klass, rb_intern("new"), 1, INT2FIX(id));

  return screen;
} /* }}} */

/* subScreenInit {{{ */
/*
 * call-seq: new(id) -> Subtlext::Screen
 *
 * Create a new Screen object
 *
 *  screen = Subtlext::Screen.new(0)
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenInit(VALUE self,
  VALUE id)
{
  if(!FIXNUM_P(id) || 0 > FIX2INT(id))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",       id);
  rb_iv_set(self, "@geometry", Qnil);

  subSubtlextConnect(); ///< Implicit open connection

  return self;
} /* }}} */

/* subScreenFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Screen or nil
 *           [value]     -> Subtlext::Screen or nil
 *
 * Find Screen by a given value which can be of following type:
 *
 * [fixnum] Array id
 *
 *  Subtlext::Screen.find("subtle")
 *  => #<Subtlext::Screen:xxx>
 *
 *  Subtlext::Screen[1]
 *  => #<Subtlext::Screen:xxx>
 *
 *  Subtlext::Screen["subtle"]
 *  => nil
 */

VALUE
subScreenFind(VALUE self,
  VALUE id)
{
  return subSubtlextFind(SUB_TYPE_SCREEN, id, True);
} /* }}} */

/* subScreenAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get Array of all Screen
 *
 *  Subtlext::Screen.all
 *  => [#<Subtlext::Screen:xxx>, #<Subtlext::Screen:xxx>]
 *
 *  Subtlext::Screen.all
 *  => []
 */

VALUE
subScreenAll(VALUE self)
{
  return ScreenList();
} /* }}} */

/* subScreenCurrent {{{ */
/*
 * call-seq: current -> Subtlext::Screen
 *
 * Get current active Screen
 *
 *  Subtlext::Screen.current
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenCurrent(VALUE self)
{
  int id = 0;
  unsigned long *focus = NULL;
  VALUE screen = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  /* Get current screen from current client or use the first */
  if((focus = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_WINDOW,
      XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
    {
      int *screen_id = NULL;

      if((screen_id = (int *)subSharedPropertyGet(display, *focus, XA_CARDINAL,
          XInternAtom(display, "SUBTLE_WINDOW_SCREEN", False), NULL)))
        {
          id = *screen_id;

          free(screen_id);
        }

      free(focus);
    }

  /* Create screen */
  screen = subScreenInstantiate(id);

  subScreenUpdate(screen);

  return screen;
} /* }}} */

/* subScreenUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Screen properties
 *
 *  screen.update
 *  => nil
 */

VALUE
subScreenUpdate(VALUE self)
{
  VALUE id = rb_iv_get(self, "@id");

  if(RTEST(id) && FIXNUM_P(id))
    {
      XRectangle geometry = { 0 };

      if(-1 != subSharedScreenFind(FIX2INT(id), &geometry))
        {
          rb_iv_set(self, "@geometry", subGeometryInstantiate(geometry.x,
            geometry.y, geometry.width, geometry.height));
        }
      else rb_raise(rb_eStandardError, "Failed finding screen");
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* subScreenClientList {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get Client on Screen
 *
 *  screen.clients
 *  => [ #<Subtlext::Client:xxx>, #<Subtlext::Client:xxx> ]
 *
 *  screen.clients
 *  => []
 */

VALUE
subScreenClientList(VALUE self)
{
  int i, id, size = 0;
  Window *clients = NULL;
  VALUE array = Qnil;

  id      = FIX2INT(rb_iv_get(self, "@id"));
  array   = rb_ary_new();
  clients = subSharedClientList(&size);

  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          int *screen = (int *)subSharedPropertyGet(display, clients[i],XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_SCREEN", False), NULL);

          if(id - 1 == *screen) ///< Check if screen matches
            rb_ary_push(array, subClientInstantiate(clients[i]));

          free(screen);
        }
      free(clients);
    }

  return array;
} /* }}} */

/* subScreenToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Screen object to String
 *
 *  puts screen
 *  => "0x0+800+600"
 */

VALUE
subScreenToString(VALUE self)
{
  char buf[256] = { 0 };
  int x = 0, y = 0, width = 0, height = 0;

  x      = FIX2INT(rb_iv_get(self, "@x"));
  y      = FIX2INT(rb_iv_get(self, "@y"));
  width  = FIX2INT(rb_iv_get(self, "@width"));
  height = FIX2INT(rb_iv_get(self, "@height"));

  snprintf(buf, sizeof(buf), "%dx%d+%d+%d", x, y, width, height);

  return rb_str_new2(buf);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
