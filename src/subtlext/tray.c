 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* Singleton */

/* subTraySingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Tray, Array or nil
 *           [value]     -> Subtlext::Tray, Array or nil
 *
 * Find Tray by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_TRAY_LIST</code> property list.
 * [String] Regexp match against both <code>WM_CLASS</code> values.
 * [Hash]   Instead of just match <code>WM_CLASS</code> match against
 *          following properties:
 *
 *          [:name]     Match against <code>WM_NAME</code>
 *          [:instance] Match against first value of <code>WM_NAME</code>
 *          [:class]    Match against second value of <code>WM_NAME</code>
 * [Symbol] Either <i>:all</i> for an array of all Trays or any string for
 *          an <b>exact</b> match.

 *  Subtlext::Tray.find(1)
 *  => #<Subtlext::Tray:xxx>
 *
 *  Subtlext::Tray.find("subtle")
 *  => #<Subtlext::Tray:xxx>
 *
 *  Subtlext::Tray[".*"]
 *  => [#<Subtlext::Tray:xxx>, #<Subtlext::Tray:xxx>]
 *
 *  Subtlext::Tray["subtle"]
 *  => nil
 *
 *  Subtlext::Tray[:terms]
 *  => #<Subtlext::Tray:xxx>
 *
 *  Subtlext::Tray[name: "subtle"]
 *  => #<Subtlext::Tray:xxx>
 */

VALUE
subTraySingFind(VALUE self,
  VALUE value)
{
  int i, flags = 0, size = 0;
  VALUE ret = Qnil, parsed = Qnil, tray = Qnil;
  char buf[50] = { 0 };
  Window *wins = NULL;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  switch(rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), &flags)))
    {
      case T_SYMBOL:
        if(CHAR2SYM("all") == parsed)
          return subTraySingAll(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Tray"))))
          return parsed;
    }

  /* Get tray list */
  if((wins = subSubtlextWindowList("SUBTLE_TRAY_LIST", &size)))
    {
      int selid = -1;
      VALUE meth = Qnil, klass = Qnil;
      regex_t *preg = subSharedRegexNew(buf);

      /* Special values */
      if(FIXNUM_P(value)) selid = (int)FIX2INT(value);

      /* Fetch data */
      meth  = rb_intern("new");
      klass = rb_const_get(mod, rb_intern("Tray"));

      /* Check each tray */
      for(i = 0; i < size; i++)
        {
          if(selid == i || (-1 == selid &&
              preg && subSubtlextWindowMatch(wins[i], preg, buf, NULL, flags)))
            {
              /* Create new tray */
              if(RTEST((tray = rb_funcall(klass, meth, 1, LONG2NUM(wins[i])))))
                {
                  rb_iv_set(tray, "@id", INT2FIX(i));

                  subTrayUpdate(tray);

                  ret = subSubtlextOneOrMany(tray, ret);
                }
            }
        }

      subSharedRegexKill(preg);
      free(wins);
    }

  return ret;
} /* }}} */

/* subTraySingAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get an array of all Trays based on <code>SUBTLE_TRAY_LIST</code>
 * property list.
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
  if((trays = subSubtlextWindowList("SUBTLE_TRAY_LIST", &ntrays)))
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
 * Create a new Tray object locally <b>without</b> calling #save automatically.
 *
 * The Tray <b>won't</b> be visible until #save is called.
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
 * Update Tray properties based on <b>required</b> Tray index and window id.
 *
 *  tray.update
 *  => nil
 */

VALUE
subTrayUpdate(VALUE self)
{
  int id = 0;
  Window win = None;

  /* Check ruby object */
  rb_check_frozen(self);
  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get tray values */
  id  = FIX2INT(rb_iv_get(self, "@id"));
  win = NUM2LONG(rb_iv_get(self, "@win"));

  /* Check values */
  if(0 <= id && 0 < win)
    {
      char *wmname = NULL, *wminstance = NULL, *wmclass = NULL;

      /* Get name, instance and class */
      subSharedPropertyClass(display, win, &wminstance, &wmclass);
      subSharedPropertyName(display, win, &wmname, wmclass);

      /* Set properties */
      rb_iv_set(self, "@name",     rb_str_new2(wmname));
      rb_iv_set(self, "@instance", rb_str_new2(wminstance));
      rb_iv_set(self, "@klass",    rb_str_new2(wmclass));

      free(wmname);
      free(wminstance);
      free(wmclass);
    }
  else rb_raise(rb_eStandardError, "Invalid tray");

  return Qnil;
} /* }}} */

/* subTrayToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Tray object to string.
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
 * Send a close signal to Client and <b>freeze</b> this object.
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
