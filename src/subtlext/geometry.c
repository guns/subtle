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
      case T_ARRAY:
        if(4 == FIX2INT(rb_funcall(value, rb_intern("size"), 0, NULL)))
          {
            int i;
            VALUE meth = rb_intern("at");

            for(i = 0; 4 > i; i++)
              data[i] = rb_funcall(value, meth, 1, INT2FIX(i));
          }
        break;
      case T_HASH:
          {
            int i;
            const char *syms[] = { "x", "y", "width", "height" };
            VALUE meth_has_key = rb_intern("has_key?"), meth_fetch = rb_intern("fetch");

            for(i = 0; 4 > i; i++)
              {
                VALUE sym = CHAR2SYM(syms[i]);

                if(Qtrue == rb_funcall(value, meth_has_key, 1, sym)) 
                  data[i] = rb_funcall(value, meth_fetch, 1, sym);
              }
          }
        break;
      case T_OBJECT:
          {
            VALUE klass = rb_const_get(mod, rb_intern("Geometry")); 
            
            /* A copy constructor would be more suitable for this.. */
            if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
              {
                data[0] = rb_iv_get(value, "@x");
                data[1] = rb_iv_get(value, "@y");
                data[2] = rb_iv_get(value, "@height");
                data[3] = rb_iv_get(value, "@width");
              }
          }        
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
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

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
  char buf[256];
  int x = 0, y = 0, width = 0, height = 0;

  x      = FIX2INT(rb_iv_get(self, "@x"));
  y      = FIX2INT(rb_iv_get(self, "@y"));
  width  = FIX2INT(rb_iv_get(self, "@width"));
  height = FIX2INT(rb_iv_get(self, "@height"));

  snprintf(buf, sizeof(buf), "%dx%d+%d+%d", x, y, width, height);

  return rb_str_new2(buf);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
