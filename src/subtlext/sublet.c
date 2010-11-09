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

  subSubtlextConnect(NULL); ///< Implicit open connection

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
  int id = 0;
  VALUE parsed = Qnil, sublet = Qnil;
  char *name = NULL, buf[50] = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  if(T_SYMBOL == rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), NULL)))
    {
      if(CHAR2SYM("all") == parsed)
        return subClientAll(Qnil);
    }

  /* Find sublet */
  if(-1 != (id = subSubtlextFind("SUBTLE_SUBLET_LIST", buf, &name)))
    {
      if(!NIL_P((sublet = subSubletInstantiate(name))))
        rb_iv_set(sublet, "@id", INT2FIX(id));

      free(name);
    }

  return sublet;
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

  subSubtlextConnect(NULL); ///< Implicit open connection

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

  if(RTEST(name) && -1 != ((id = subSubtlextFind("SUB_SUBLET_LIST",
      RSTRING_PTR(name), NULL))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Send data */
      data.l[0] = id;

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_SUBLET_UPDATE", data, 32, True);
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

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_SUBLET_DATA", data, 8, True);
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return value;
} /* }}} */

/* subSubletForegroundWriter {{{ */
/*
 * call-seq: foreground=(color) -> nil
 *
 * Set default foreground of sublet
 *
 *  sublet.foreground = "#ff0000"
 *  => nil
 */

VALUE
subSubletForegroundWriter(VALUE self,
  VALUE value)
{
  /* Check object type */
  if(T_STRING == rb_type(value))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2INT(rb_iv_get(self, "@id"));
      data.l[1] = subSharedParseColor(display, RSTRING_PTR(value));

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_SUBLET_FOREGROUND", data, 32, True);
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'",
    rb_obj_classname(value));

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
  /* Check object type */
  if(T_STRING == rb_type(value))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2INT(rb_iv_get(self, "@id"));
      data.l[1] = subSharedParseColor(display, RSTRING_PTR(value));

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_SUBLET_BACKGROUND", data, 32, True);
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'",
    rb_obj_classname(value));

  return value;
} /* }}} */

/* subSubletGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get geometry of a sublet
 *
 *  win.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subSubletGeometryReader(VALUE self)
{
  int id = 0, size = 0, wx = 0, wy = 0, px = 0, py = 0;
  unsigned int wwidth = 0, wheight = 0, wbw = 0, wdepth = 0;
  unsigned int pwidth = 0, pheight = 0, pbw = 0, pdepth = 0;
  Window *wins = NULL, win = None, parent = None, wroot = None, proot = None;

  /* Fetch data */
  id   = FIX2INT(rb_iv_get(self, "@id"));
  wins = subSubtlextList("SUBTLE_SUBLET_WINDOWS", &size);
  win  = wins[id];

  free(wins);

  /* Get parent and geometries */
  XGetGeometry(display, win, &wroot, &wx, &wy,
    &wwidth, &wheight, &wbw, &wdepth);

  XQueryTree(display, win, &wroot, &parent, &wins,
    (unsigned int *)&size); ///< Get parent

  XGetGeometry(display, parent, &proot, &px, &py,
    &pwidth, &pheight, &pbw, &pdepth);

  if(wins) XFree(wins);

  return subGeometryInstantiate(px + wx, py + wy, wwidth, wheight);
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
  VALUE id = rb_iv_get(self, "@id");

  subSubtlextConnect(NULL); ///< Implicit open connection

  if(RTEST(id))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2INT(id);

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_SUBLET_KILL", data, 32, True);
    }
  else rb_raise(rb_eStandardError, "Failed killing sublet");

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
