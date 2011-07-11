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

/* subSubletSingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Sublet, Array or nil
 *           [value]     -> Subtlext::Sublet, Array or nil
 *
 * Find Sublet by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_SUBLET_LIST</code> property list.
 * [String] Regexp match against name of Sublets, returns a Sublet on single
 *          match or an Array on multiple matches.
 * [Symbol] Either <i>:all</i> for an array of all Sublets or any string for
 *          an <b>exact</b> match.
 *
 *  Subtlext::Sublet.find(1)
 *  => #<Subtlext::Sublet:xxx>
 *
 *  Subtlext::Sublet.find("subtle")
 *  => #<Subtlext::Sublet:xxx>
 *
 *  Subtlext::Sublet[".*"]
 *  => [#<Subtlext::Sublet:xxx>, #<Subtlext::Sublet:xxx>]
 *
 *  Subtlext::Sublet["subtle"]
 *  => nil
 *
 *  Subtlext::Sublet[:clock]
 *  => #<Subtlext::Sublet:xxx>

 */

VALUE
subSubletSingFind(VALUE self,
  VALUE value)
{
  int flags = 0;
  VALUE parsed = Qnil;
  char buf[50] = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  switch(rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), &flags)))
    {
      case T_SYMBOL:
        if(CHAR2SYM("all") == parsed)
          return subSubletSingAll(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Sublet"))))
          return parsed;
    }

  return subSubtlextFindObjects("SUBTLE_SUBLET_LIST", "Sublet", buf, flags);
} /* }}} */

/* subSubletSingAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get an array of all running Sublets.
 *
 *  Subtlext::Sublet.all
 *  => [#<Subtlext::Sublet:xxx>, #<Subtlext::Sublet:xxx>]
 *
 *  Subtlext::Sublet.all
 *  => []
 */

VALUE
subSubletSingAll(VALUE self)
{
  int i, nsublets = 0;
  char **sublets = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth   = rb_intern("new");
  klass  = rb_const_get(mod, rb_intern("Sublet"));
  array  = rb_ary_new();

  /* Check results */
  if((sublets = subSharedPropertyGetStrings(display,
      DefaultRootWindow(display), XInternAtom(display, "SUBTLE_SUBLET_LIST",
      False), &nsublets)))
    {
      for(i = 0; i < nsublets; i++)
        {
          VALUE s = rb_funcall(klass, meth, 1, rb_str_new2(sublets[i]));

          rb_iv_set(s, "@id", INT2FIX(i));
          rb_ary_push(array, s);
        }

      XFreeStringList(sublets);
    }

  return array;
} /* }}} */

/* Class */

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
 * Create new Sublet object locally <b>without</b> calling #save automatically.
 *
 *  sublet = Subtlext::Sublet.new("subtle")
 *  => #<Subtlext::Sublet:xxx> *
 */

VALUE
subSubletInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(name));

  /* Init object */
  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subSubletUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Force subtle to update the data of this Sublet.
 *
 *  sublet.update
 *  => nil
 */

VALUE
subSubletUpdate(VALUE self)
{
  int id = -1;
  VALUE name = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", name);

  /* Check results */
  if(-1 != ((id = subSubtlextFindString("SUBTLE_SUBLET_LIST",
      RSTRING_PTR(name), NULL, SUB_MATCH_EXACT))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Send message */
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
 * Get data of this Sublet.
 *
 *  puts sublet.data
 *  => "subtle"
 */

VALUE
subSubletDataReader(VALUE self)
{
  VALUE id = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  return id;
} /* }}} */

/* subSubletDataWriter {{{ */
/*
 * call-seq: data=(string) -> String
 *
 * Set data of this Sublet.
 *
 *  sublet.data = "subtle"
 *  => "subtle"
 */

VALUE
subSubletDataWriter(VALUE self,
  VALUE value)
{
  VALUE id = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Check object type */
  if(T_STRING == rb_type(value))
    {
      char *list = NULL;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Store data */
      list = strdup(RSTRING_PTR(value));
      subSharedPropertySetStrings(display, DefaultRootWindow(display),
        XInternAtom(display, "SUBTLE_DATA", False), &list, 1);
      free(list);

      data.l[0] = FIX2LONG(id);

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_SUBLET_DATA", data, 32, True);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(value));

  return value;
} /* }}} */

/* subSubletForegroundWriter {{{ */
/*
 * call-seq: foreground=(string) -> nil
 *           foreground=(array)  -> nil
 *           foreground=(hash)   -> nil
 *           foreground=(fixnum) -> nil
 *           foreground=(color)  -> nil
 *
 * Set the foreground color of this Sublet which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Color]  Copy color from a Color object
 *
 *  sublet.foreground = "#ff0000"
 *  => nil
 */

VALUE
subSubletForegroundWriter(VALUE self,
  VALUE value)
{
  VALUE id = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Check object type */
  if(T_STRING == rb_type(value))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2INT(id);
      data.l[1] = subSharedParseColor(display, RSTRING_PTR(value));

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_SUBLET_FOREGROUND", data, 32, True);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(value));

  return value;
} /* }}} */

/* subSubletBackgroundWriter {{{ */
/*
 * call-seq: background=(string) -> nil
 *           background=(array)  -> nil
 *           background=(hash)   -> nil
 *           background=(fixnum) -> nil
 *           background=(color)  -> nil
 *
 * Set the background color of this Sublet which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Color]  Copy color from a Color object
 *
 *  sublet.background = "#ff0000"
 *  => nil
 */

VALUE
subSubletBackgroundWriter(VALUE self,
  VALUE value)
{
  VALUE id = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Check object type */
  if(T_STRING == rb_type(value))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2INT(rb_iv_get(self, "@id"));
      data.l[1] = subSharedParseColor(display, RSTRING_PTR(value));

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_SUBLET_BACKGROUND", data, 32, True);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(value));

  return value;
} /* }}} */

/* subSubletVisibilityShow {{{ */
/*
 * call-seq: show -> nil
 *
 * Show sublet in the panels.
 *
 *  sublet.show
 *  => nil
 */

VALUE
subSubletVisibilityShow(VALUE self)
{
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  data.l[0] = FIX2LONG(id);
  data.l[1] = SUB_EWMH_VISIBLE;

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_SUBLET_FLAGS", data, 32, True);

  return Qnil;
} /* }}} */

/* subSubletVisibilityHide {{{ */
/*
 * call-seq: hide -> nil
 *
 * Hide sublet from the panels.
 *
 *  sublet.hide
 *  => nil
 */

VALUE
subSubletVisibilityHide(VALUE self)
{
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  data.l[0] = FIX2LONG(id);
  data.l[1] = SUB_EWMH_HIDDEN;

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_SUBLET_FLAGS", data, 32, True);

  return Qnil;
} /* }}} */

/* subSubletGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get Geometry of this Sublet
 *
 *  win.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subSubletGeometryReader(VALUE self)
{
  int nwins = 0, wx = 0, wy = 0, px = 0, py = 0;
  unsigned int wwidth = 0, wheight = 0, wbw = 0, wdepth = 0;
  unsigned int pwidth = 0, pheight = 0, pbw = 0, pdepth = 0;
  Window *wins = NULL, win = None, parent = None, wroot = None, proot = None;
  VALUE id = Qnil;

 /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Fetch data */
  if((wins = subSubtlextWindowList("SUBTLE_SUBLET_WINDOWS", &nwins)))
    {
      win  = wins[id];

      free(wins);
    }

  /* Get parent and geometries */
  XGetGeometry(display, win, &wroot, &wx, &wy,
    &wwidth, &wheight, &wbw, &wdepth);

  XQueryTree(display, win, &wroot, &parent, &wins,
    (unsigned int *)&nwins); ///< Get parent

  XGetGeometry(display, parent, &proot, &px, &py,
    &pwidth, &pheight, &pbw, &pdepth);

  if(wins) XFree(wins);

  return subGeometryInstantiate(px + wx, py + wy, wwidth, wheight);
} /* }}} */

/* subSubletToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Sublet object to string.
 *
 *  puts sublet
 *  => sublet
 */

VALUE
subSubletToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subSubletKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Remove this Sublet from subtle and <b>freeze</b> this object.
 *
 *  sublet.kill
 *  => nil
 */

VALUE
subSubletKill(VALUE self)
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
    "SUBTLE_SUBLET_KILL", data, 32, True);

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
