 /**
  * @package subtlext
  *
  * @file Gravity functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtlext.h"

/* subGravityInstantiate {{{ */
VALUE
subGravityInstantiate(char *name)
{
  VALUE klass = Qnil, gravity = Qnil;

  /* Create new instance */
  klass   = rb_const_get(mod, rb_intern("Gravity"));
  gravity = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return gravity;
} /* }}} */

/* subGravityInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Gravity
 *
 * Create a new Gravity object
 *
 *  gravity = Subtlext::Gravity.new("top")
 *  => #<Subtlext::Gravity:xxx>
 */

VALUE
subGravityInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",       Qnil);
  rb_iv_set(self, "@name",     name);
  rb_iv_set(self, "@geometry", Qnil);

  subSubtlextConnect(); ///< Implicit open connection

  return self;
} /* }}} */

/* subGravityFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Gravity or nil
 *           [value]     -> Subtlext::Gravity or nil
 *
 * Find Gravity by a given value which can be of following type:
 *
 * [fixnum] Array id
 * [string] Match against name of Gravity
 * [symbol] Symbol of the Gravity or :all for an array of all Gravity
 *
 *  Subtlext::Gravity.find("center")
 *  => #<Subtlext::Gravity:xxx>
 *
 *  Subtlext::Gravity[:center]
 *  => #<Subtlext::Gravity:xxx>
 *
 *  Subtlext::Gravity["center"]
 *  => nil
 */

VALUE
subGravityFind(VALUE self,
  VALUE value)
{
  return subSubtlextFind(SUB_TYPE_GRAVITY, value, True);
} /* }}} */

/* subGravityAll {{{ */
/*
 * call-seq: gravities -> Array
 *
 * Get Array of all Gravity
 *
 *  Subtlext::Gravity.all
 *  => [#<Subtlext::Gravity:xxx>, #<Subtlext::Gravity:xxx>]
 *
 *  Subtlext::Gravity.all
 *  => []
 */

VALUE
subGravityAll(VALUE self)
{
  int size = 0;
  char **gravities = NULL;

  VALUE array = rb_ary_new();

  subSubtlextConnect(); ///< Implicit open connection

  /* Get gravity list */
  if((gravities = subSharedPropertyStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_GRAVITY_LIST", False), &size)))
    {
      int i;
      XRectangle geometry = { 0 };
      char buf[30] = { 0 };
      VALUE klass_grav = Qnil, klass_geom = Qnil, meth = Qnil, gravity = Qnil, geom = Qnil;

      klass_grav = rb_const_get(mod, rb_intern("Gravity"));
      klass_geom = rb_const_get(mod, rb_intern("Geometry"));
      meth       = rb_intern("new");

      /* Create gravity list */
      for(i = 0; i < size; i++)
        {
          sscanf(gravities[i], "%hdx%hd+%hd+%hd#%s", &geometry.x, &geometry.y,
            &geometry.width, &geometry.height, buf);

          gravity = rb_funcall(klass_grav, meth, 1, rb_str_new2(buf));
          geom    = rb_funcall(klass_geom, meth, 4, INT2FIX(geometry.x), INT2FIX(geometry.y),
            INT2FIX(geometry.width), INT2FIX(geometry.height));

          rb_iv_set(gravity, "@id", INT2FIX(i));
          rb_iv_set(gravity, "@geometry", geom);

          rb_ary_push(array, gravity);
        }

      XFreeStringList(gravities);
    }
  else rb_raise(rb_eStandardError, "Failed getting gravity list");

  return array;
} /* }}} */

/* subGravityUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Gravity properties
 *
 *  gravity.update
 *  => nil
 */

VALUE
subGravityUpdate(VALUE self)
{
  int id = -1;
  XRectangle geometry = { 0 };
  char *name = NULL;
  VALUE match = rb_iv_get(self, "@name");

  /* Update gravity */
  if(T_STRING == rb_type(match) &&
      -1 != (id = subSharedGravityFind(RSTRING_PTR(match), &name, &geometry)))
    {
      rb_iv_set(self, "@id",       INT2FIX(id));
      rb_iv_set(self, "@name",     rb_str_new2(name));
      rb_iv_set(self, "@geometry", subGeometryInstantiate(geometry.x, geometry.y,
        geometry.width, geometry.height));

      free(name);
    }
  else rb_raise(rb_eStandardError, "Failed finding gravity");

  return Qnil;
} /* }}} */

/* subGravityGeometry {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get Gravity Geometry
 *
 *  gravity.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGravityGeometry(VALUE self)
{
  VALUE geometry = Qnil, name = Qnil;

  /* Load on demand */
  if(NIL_P((geometry = rb_iv_get(self, "@geometry"))) &&
      T_STRING == rb_type((name = rb_iv_get(self, "@name"))))
    {
      XRectangle geom = { 0 };

      subSharedGravityFind(RSTRING_PTR(name), NULL, &geom);

      geometry = subGeometryInstantiate(geom.x, geom.y, geom.width, geom.height);
      rb_iv_set(self, "@geometry", geometry);
    }

  return geometry;
} /* }}} */

/* subGravityToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Gravity object to String
 *
 *  puts gravity
 *  => "TopLeft"
 */

VALUE
subGravityToString(VALUE self)
{
  return rb_iv_get(self, "@name");
} /* }}} */

/* subGravityToSym {{{ */
/*
 * call-seq: to_sym -> Symbol
 *
 * Convert Gravity object to Symbol
 *
 *  puts gravity.to_sym
 *  => :center
 */

VALUE
subGravityToSym(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return CHAR2SYM(RSTRING_PTR(name));
} /* }}} */

/* subGravityKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a Gravity
 *
 *  gravity.kill
 *  => nil
 */

VALUE
subGravityKill(VALUE self)
{
  return subSubtlextKill(self, SUB_TYPE_GRAVITY);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
