 /**
  * @package subtlext
  *
  * @file Geometry functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtlext.h"

/* subGeometryInstantiate {{{ */
VALUE
subGeometryInstantiate(int x,
  int y,
  int width,
  int height)
{
  VALUE klass = Qnil, geometry = Qnil;

  /* Create new instance */
  klass    = rb_const_get(mod, rb_intern("Geometry"));
  geometry = rb_funcall(klass, rb_intern("new"), 4,
    INT2FIX(x), INT2FIX(y), INT2FIX(width), INT2FIX(height));

  return geometry;
} /* }}} */

/* subGeometryToRect {{{ */
void
subGeometryToRect(VALUE self,
  XRectangle *r)
{
  r->x      = FIX2INT(rb_iv_get(self, "@x"));
  r->y      = FIX2INT(rb_iv_get(self, "@y"));
  r->width  = FIX2INT(rb_iv_get(self, "@width"));
  r->height = FIX2INT(rb_iv_get(self, "@height"));
} /* }}} */

/* subGeometryInit {{{ */
/*
 * call-seq: new(x, y, width, height) -> Subtlext::Geometry
 *           new(array)               -> Subtlext::Geometry
 *           new(hash)                -> Subtlext::Geometry
 *           new(geometry)            -> Subtlext::Geometry
 *
 * Create a new Geometry object
 *
 *  geom = Subtlext::Geometry.new(0, 0, 50, 50)
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new([ 0, 0, 50, 50 ])
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new({ :x => 0, :y => 0, :width => 50, :height => 50 })
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new(Subtlext::Geometry.new(0, 0, 50, 50))
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGeometryInit(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE value = Qnil, data[4] = { Qnil };

  rb_scan_args(argc, argv, "04", &data[0], &data[1], &data[2], &data[3]);
  value = data[0];

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM: break;
      case T_ARRAY:
        if(4 == FIX2INT(rb_funcall(value, rb_intern("size"), 0, NULL)))
          {
            int i;

            for(i = 0; 4 > i; i++)
              data[i] = rb_ary_entry(value, i);
          }
        break;
      case T_HASH:
          {
            int i;
            const char *syms[] = { "x", "y", "width", "height" };

            for(i = 0; 4 > i; i++)
              data[i] = rb_hash_lookup(value, CHAR2SYM(syms[i]));
          }
        break;
      case T_OBJECT:
          {
            VALUE klass = rb_const_get(mod, rb_intern("Geometry"));

            /* Check object instance */
            if(rb_obj_is_instance_of(value, klass))
              {
                data[0] = rb_iv_get(value, "@x");
                data[1] = rb_iv_get(value, "@y");
                data[2] = rb_iv_get(value, "@width");
                data[3] = rb_iv_get(value, "@height");
              }
          }
        break;
      default:
        rb_raise(rb_eArgError, "Unexpected value-type `%s'",
          rb_obj_classname(value));
    }

  /* Set values */
  if(FIXNUM_P(data[0]) && FIXNUM_P(data[1]) && FIXNUM_P(data[2]) &&
      FIXNUM_P(data[3]) && 0 < FIX2INT(data[2]) && 0 < FIX2INT(data[3]))
    {
      rb_iv_set(self, "@x",      data[0]);
      rb_iv_set(self, "@y",      data[1]);
      rb_iv_set(self, "@width",  data[2]);
      rb_iv_set(self, "@height", data[3]);
    }
  else rb_raise(rb_eStandardError, "Failed setting zero width or height");

  return self;
} /* }}} */

/* subGeometryToArray {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Geometry object to Array
 *
 *  geom.to_a
 *  => [0, 0, 50, 50]
 */

VALUE
subGeometryToArray(VALUE self)
{
  VALUE ary = rb_ary_new2(4);

  rb_ary_push(ary, rb_iv_get(self, "@x"));
  rb_ary_push(ary, rb_iv_get(self, "@y"));
  rb_ary_push(ary, rb_iv_get(self, "@width"));
  rb_ary_push(ary, rb_iv_get(self, "@height"));

  return ary;
} /* }}} */

/* subGeometryToHash {{{ */
/*
 * call-seq: to_hash -> Hash
 *
 * Convert Geometry object to Hash
 *
 *  geom.to_hash
 *  => { :x => 0, :y => 0, :width => 50, :height => 50 }
 */

VALUE
subGeometryToHash(VALUE self)
{
  VALUE klass = Qnil, hash = Qnil, meth = Qnil;

  klass = rb_const_get(rb_mKernel, rb_intern("Hash"));
  hash  = rb_funcall(klass, rb_intern("new"), 0, NULL);
  meth  = rb_intern("store");

  rb_funcall(hash, meth, 2, CHAR2SYM("x"),      rb_iv_get(self, "@x"));
  rb_funcall(hash, meth, 2, CHAR2SYM("y"),      rb_iv_get(self, "@y"));
  rb_funcall(hash, meth, 2, CHAR2SYM("width"),  rb_iv_get(self, "@width"));
  rb_funcall(hash, meth, 2, CHAR2SYM("height"), rb_iv_get(self, "@height"));

  return hash;
} /* }}} */

/* subGeometryToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Geometry object to String
 *
 *  puts geom
 *  => "0x0+50+50"
 */

VALUE
subGeometryToString(VALUE self)
{
  char buf[256] = { 0 };
  int x = 0, y = 0, width = 0, height = 0;

  /* Fetch data */
  x      = FIX2INT(rb_iv_get(self, "@x"));
  y      = FIX2INT(rb_iv_get(self, "@y"));
  width  = FIX2INT(rb_iv_get(self, "@width"));
  height = FIX2INT(rb_iv_get(self, "@height"));

  snprintf(buf, sizeof(buf), "%dx%d+%d+%d", x, y, width, height);

  return rb_str_new2(buf);
} /* }}} */

/* subGeometryEqual {{{ */
/*
 * call-seq: ==(other) -> True or False
 *
 * Whether both objects are equal (based on geometry)
 *
 *  object1 == object2
 *  => true
 */

VALUE
subGeometryEqual(VALUE self,
  VALUE other)
{
  XRectangle r1 = { 0 }, r2 = { 0 };

  subGeometryToRect(self, &r1);
  subGeometryToRect(other, &r2);

  return (r1.x == r2.x && r1.y == r2.y &&
    r1.width == r2.width && r1.height == r2.height) ? Qtrue : Qfalse;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
