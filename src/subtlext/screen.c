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
  unsigned long size = 0;
  VALUE method = Qnil, klass = Qnil, array = Qnil, screen = Qnil, geometry = Qnil;
  long *workareas = NULL;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  method = rb_intern("new");
  klass  = rb_const_get(mod, rb_intern("Screen"));
  array  = rb_ary_new();

  /* Get workarea list */
  if((workareas = (long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_CARDINAL,
      XInternAtom(display, "_NET_WORKAREA", False), &size)))
    {
      int i;

      for(i = 0; i < size / 4; i++)
        {
          /* Create new screen */
          screen   = rb_funcall(klass, method, 1, INT2FIX(i));
          geometry = subGeometryInstantiate(workareas[i * 4 + 0],
            workareas[i * 4 + 1], workareas[i * 4 + 2], workareas[i * 4 + 3]);

          rb_iv_set(screen, "@geometry", geometry);
          rb_ary_push(array, screen);
        }

      free(workareas);
    }
  else printf("Failed getting workarea list\n");

  return array;
} /* }}} */

/* Singleton */

/* subScreenSingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Screen or nil
 *           [value]     -> Subtlext::Screen or nil
 *
 * Find Screen by a given value which can be of following type:
 *
 * [fixnum] Array id
 *
 *  Subtlext::Screen.find(1)
 *  => #<Subtlext::Screen:xxx>
 *
 *  Subtlext::Screen[1]
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenSingFind(VALUE self,
  VALUE value)
{
  VALUE screen = Qnil;

  /* Check object type */
  if(FIXNUM_P(value))
    {
      VALUE screens = ScreenList();

      screen = rb_ary_entry(screens, FIX2INT(value));
    }
  else rb_raise(rb_eArgError, "Unexpected value type `%s'",
    rb_obj_classname(value));

  return screen;
} /* }}} */

/* subScreenSingAll {{{ */
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
subScreenSingAll(VALUE self)
{
  return ScreenList();
} /* }}} */

/* subScreenSingCurrent {{{ */
/*
 * call-seq: current -> Subtlext::Screen
 *
 * Get current active Screen
 *
 *  Subtlext::Screen.current
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subScreenSingCurrent(VALUE self)
{
  int id = 0;
  unsigned long *focus = NULL;
  VALUE screen = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

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

/* Class */

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
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(id));

  rb_iv_set(self, "@id",       id);
  rb_iv_set(self, "@geometry", Qnil);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
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
  VALUE id = Qnil;

  /* Check object type */
  if(FIXNUM_P(id = rb_iv_get(self, "@id")))
    {
      VALUE screens = Qnil, screen = Qnil;

      screens = ScreenList();

      if(RTEST(screen = rb_ary_entry(screens, FIX2INT(id))))
        {
          VALUE geometry = rb_iv_get(screen, "@geometry");

          rb_iv_set(self, "@geometry", geometry);
        }
      else rb_raise(rb_eStandardError, "Failed finding screen");
    }
  else rb_raise(rb_eStandardError, "Failed updating screen");

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
  VALUE klass = Qnil, meth = Qnil, array = Qnil, client = Qnil;

  id      = FIX2INT(rb_iv_get(self, "@id"));
  klass   = rb_const_get(mod, rb_intern("Client"));
  meth    = rb_intern("new");
  array   = rb_ary_new();
  clients = subSubtlextList("_NET_CLIENT_LIST", &size);

  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          int *screen = (int *)subSharedPropertyGet(display, clients[i],XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_SCREEN", False), NULL);

          /* Check if screen matches */
          if(id == *screen)
            {
              if(!NIL_P(client = rb_funcall(klass, meth, 1, INT2FIX(i))))
                {
                  subClientUpdate(client);
                  rb_ary_push(array, client);
                }
            }

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
  VALUE geometry = rb_iv_get(self, "@geometry");

  return RTEST(geometry) ? subGeometryToString(geometry) : Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
