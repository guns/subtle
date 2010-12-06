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

/* Singleton */

/* subTraySingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Client or nil
 *           [value]     -> Subtlext::Client or nil
 *
 * Find Client by a given value which can be of following type:
 *
 * [fixnum] Array id
 * [string] Match against WM_NAME or WM_CLASS
 * [hash]   With one of following keys: :title, :name, :class, :gravity
 * [symbol] Either :current for current Client or :all for an array
 *
 *  Subtlext::Tray.find("subtle")
 *  => #<Subtlext::Tray:xxx>
 *
 *  Subtlext::Tray[1]
 *  => #<Subtlext::Tray:xxx>
 *
 *  Subtlext::Tray["subtle"]
 *  => nil
 */

VALUE
subTraySingFind(VALUE self,
  VALUE value)
{
  int id = 0, flags = 0;
  Window win = None;
  VALUE parsed = Qnil, tray = Qnil;
  char *name = NULL, buf[50] = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  if(T_SYMBOL == rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), &flags)))
    {
      if(CHAR2SYM("all") == parsed)
        return subTraySingAll(Qnil);
    }

  /* Find tray */
  if(-1 != (id = subSubtlextFindWindow("SUBTLE_TRAY_LIST",
      buf, NULL, &win, flags)))
    {
      if(!NIL_P((tray = subTrayInstantiate(win))))
        {
          rb_iv_set(tray, "@id", INT2FIX(id));

          subTrayUpdate(tray);
        }

      free(name);
    }

  return tray;
} /* }}} */

/* subTraySingAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get Array of all Tray
 *
 *  Subtlext::Tray.all
 *  => [#<Subtlext::Tray:xxx>, #<Subtlext::Tray:xxx>]
 *
 *  Subtlext::Tray.all
 *  => []
 */

VALUE
subTraySingAll(VALUE self)
{
  int i, ntrays = 0;
  Window *trays = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Tray"));
  array = rb_ary_new();

  /* Check results */
  if((trays = subSubtlextList("SUBTLE_TRAY_LIST", &ntrays)))
    {
      for(i = 0; i < ntrays; i++)
        {
          VALUE t = rb_funcall(klass, meth, 1, LONG2NUM(trays[i]));

          if(!NIL_P(t)) subTrayUpdate(t);

          rb_ary_push(array, t);
        }

      free(trays);
    }

  return array;
} /* }}} */

/* Class */

/* subTrayInstantiate {{{ */
VALUE
subTrayInstantiate(Window win)
{
  VALUE klass = Qnil, tray = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("Tray"));
  tray  = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(win));

  return tray;
} /* }}} */

/* subTrayInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Tray
 *
 * Create new Tray object
 *
 *  tag = Subtlext::Tray.new("subtle")
 *  => #<Subtlext::Tray:xxx>
 */

VALUE
subTrayInit(VALUE self,
  VALUE win)
{
  if(!FIXNUM_P(win) && T_BIGNUM != rb_type(win))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(win));

  /* Init object */
  rb_iv_set(self, "@id",    Qnil);
  rb_iv_set(self, "@win",   win);
  rb_iv_set(self, "@name",  Qnil);
  rb_iv_set(self, "@klass", Qnil);
  rb_iv_set(self, "@title", Qnil);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subTrayUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Tray properties
 *
 *  tray.update
 *  => nil
 */

VALUE
subTrayUpdate(VALUE self)
{
  int id = 0;
  char buf[20] = { 0 };
  VALUE win = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Find tray */
  snprintf(buf, sizeof(buf), "%#lx", NUM2LONG(win));
  if(-1 != (id = subSubtlextFindWindow("SUBTLE_TRAY_LIST", buf, NULL,
      NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS))))
    {
      char *title = NULL, *wmname = NULL, *wmclass = NULL;

      XFetchName(display, NUM2LONG(win), &title);
      subSharedPropertyClass(display, NUM2LONG(win), &wmname, &wmclass);

      /* Set properties */
      rb_iv_set(self, "@id",    INT2FIX(id));
      rb_iv_set(self, "@title", rb_str_new2(title));
      rb_iv_set(self, "@name",  rb_str_new2(wmname));
      rb_iv_set(self, "@klass", rb_str_new2(wmclass));

      free(title);
      free(wmname);
      free(wmclass);
    }

  return Qnil;
} /* }}} */

/* subTrayToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Tray object to String
 *
 *  puts tray
 *  => "subtle"
 */

VALUE
subTrayToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subTrayKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a Tray
 *
 *  tray.kill
 *  => nil
 */

VALUE
subTrayKill(VALUE self)
{
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = FIX2INT(id);

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_TRAY_KILL", data, 32, True);

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
