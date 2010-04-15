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

/* subSubletInstantiate {{{ */
VALUE
subSubletInstantiate(char *name)
{
  VALUE klass = Qnil, sublet = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Sublet"));
  sublet = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return sublet;
} /* }}} */

/* subSubletInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Sublet
 *
 * Create new Sublet object
 *
 *  sublet = Subtlext::Sublet.new("subtle")
 *  => #<Subtlext::Sublet:xxx> *
 */

VALUE
subSubletInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);

  subSubtlextConnect(); ///< Implicit open connection

  return self;
} /* }}} */

/* subSubletFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Sublet or nil
 *           [value]     -> Subtlext::Sublet or nil
 *
 * Find Sublet by a given value which can be of following type:
 *
 * [fixnum] Array id
 * [string] Match against name of Sublet
 * [symbol] :all for an array of all Sublet
 *
 *  Subtlext::Sublet.find("subtle")
 *  => #<Subtlext::Sublet:xxx>
 *
 *  Subtlext::Sublet[1]
 *  => #<Subtlext::Sublet:xxx>
 *
 *  Subtlext::Sublet["subtle"]
 *  => nil
 */

VALUE
subSubletFind(VALUE self,
  VALUE value)
{
  return subSubtlextFind(SUB_TYPE_SUBLET, value, True);
} /* }}} */

/* subSubletAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get Array of Sublet
 *
 *  Subtlext::Sublet.all
 *  => [#<Subtlext::Sublet:xxx>, #<Subtlext::Sublet:xxx>]
 *
 *  Subtlext::Sublet.all
 *  => []
 */

VALUE
subSubletAll(VALUE self)
{
  int i, size = 0;
  char **sublets = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("Sublet"));
  sublets = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "SUBTLE_SUBLET_LIST", False), &size);
  array   = rb_ary_new2(size);

  /* Populate array */
  if(sublets)
    {
      for(i = 0; i < size; i++)
        {
          VALUE s = rb_funcall(klass, meth, 1, rb_str_new2(sublets[i]));

          rb_iv_set(s, "@id", INT2FIX(i));
          rb_ary_push(array, s);
        }

      XFreeStringList(sublets);
    }

  return array;
} /* }}} */

/* subSubletUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Force Sublet to update it's data
 *
 *  sublet.update
 *  => nil
 */

VALUE
subSubletUpdate(VALUE self)
{
  int id = -1;
  VALUE name = rb_iv_get(self, "@name");

  if(RTEST(name) && -1 != ((id = subSharedSubletFind(RSTRING_PTR(name), NULL))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Send data */
      data.l[0] = id;

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_UPDATE", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed finding sublet");

  return Qnil;
} /* }}} */

/* subSubletDataReader {{{ */
/*
 * call-seq: data -> String
 *
 * Get data of Sublet
 *
 *  puts sublet.data
 *  => "subtle"
 */

VALUE
subSubletDataReader(VALUE self)
{
  VALUE id = rb_iv_get(self, "@id");

  return RTEST(id) ? id : Qnil;
} /* }}} */

/* subSubletDataWriter {{{ */
/*
 * call-seq: data=(string) -> String
 *
 * Set data of sublet
 *
 *  sublet.data = "subtle"
 *  => "subtle"
 */

VALUE
subSubletDataWriter(VALUE self,
  VALUE value)
{
  if(T_STRING == rb_type(value))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      VALUE id = rb_iv_get(self, "@id");

      snprintf(data.b, sizeof(data.b), "%c%s",
        (char)NUM2LONG(id), RSTRING_PTR(value));

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_DATA", data, True);
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return value;
} /* }}} */

/* subSubletBackgroundWriter {{{ */
/*
 * call-seq: background=(color) -> nil
 *
 * Set background of sublet
 *
 *  sublet.background = "#ff0000"
 *  => nil
 */

VALUE
subSubletBackgroundWriter(VALUE self,
  VALUE value)
{
  if(T_STRING == rb_type(value))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2INT(rb_iv_get(self, "@id"));
      data.l[1] = subSharedParseColor(display, RSTRING_PTR(value));

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_BACKGROUND", data, True);
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return value;
} /* }}} */

/* subSubletToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Sublet object to String
 *
 *  puts sublet
 *  => sublet
 */

VALUE
subSubletToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* subSubletKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a Sublet
 *
 *  sublet.kill
 *  => nil
 */

VALUE
subSubletKill(VALUE self)
{
  return subSubtlextKill(self, SUB_TYPE_SUBLET);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
