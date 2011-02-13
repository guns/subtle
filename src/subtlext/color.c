 /**
  * @package subtlext
  *
  * @file Gravity functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtlext.h"

/* ColorEqual {{{ */
VALUE
ColorEqual(VALUE self,
  VALUE other,
  int check_type)
{
  int ret = False;
  VALUE pixel1 = Qnil, pixel2 = Qnil;

  /* Check ruby object */
  GET_ATTR(self,  "@pixel", pixel1);
  GET_ATTR(other, "@pixel", pixel2);

  /* Check ruby object types */
  if(check_type)
    {
      ret = (rb_obj_class(self) == rb_obj_class(other) && pixel1 == pixel2);
    }
  else ret = (pixel1 == pixel2);

  return ret ? Qtrue : Qfalse;
} /* }}} */

/* Exported */

/* subColorPixel {{{ */
unsigned long
subColorPixel(VALUE value)
{
  unsigned long pixel = 0;

  /* Check object type */
  switch(rb_type(value))
    {
      case T_STRING:
        pixel = subSharedParseColor(display, RSTRING_PTR(value));
        break;
      case T_FIXNUM:
        pixel = FIX2LONG(value);
        break;
      case T_BIGNUM:
        pixel = NUM2LONG(value);
        break;
      case T_OBJECT:
          {
            VALUE klass = rb_const_get(mod, rb_intern("Color"));

            /* Check object instance */
            if(rb_obj_is_instance_of(value, klass))
              pixel = NUM2LONG(rb_iv_get(value, "@pixel"));
          }
        break;
      default:
        rb_raise(rb_eArgError, "Unexpected value-type `%s'",
          rb_obj_classname(value));
    }

  return pixel;
} /* }}} */

/* subColorInstantiate {{{ */
VALUE
subColorInstantiate(unsigned long pixel)
{
  VALUE klass = Qnil, color = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Color"));
  color = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(pixel));

  return color;
} /* }}} */

/* subColorInit {{{ */
/*
 * call-seq: new(color) -> Subtlext::Color
 *
 * Create new Color object
 *
 *  color = Subtlext::Color.new("#336699")
 *  => #<Subtlext::Color:xxx>
 *
 *  color = Subtlext::Color.new(14253553)
 *  => #<Subtlext::Color:xxx>
 */

VALUE
subColorInit(VALUE self,
  VALUE value)
{
  XColor xcolor = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get color values */
  xcolor.pixel = subColorPixel(value);

  XQueryColor(display, DefaultColormap(display,
    DefaultScreen(display)), &xcolor);

  /* Set values */
  rb_iv_set(self, "@red",   INT2FIX(xcolor.red));
  rb_iv_set(self, "@green", INT2FIX(xcolor.green));
  rb_iv_set(self, "@blue",  INT2FIX(xcolor.blue));
  rb_iv_set(self, "@pixel", LONG2NUM(xcolor.pixel));

  return self;
} /* }}} */

/* subColorToHex {{{ */
/*
 * call-seq: to_hex -> String
 *
 * Convert Color object to <b>rrggbb</b> hex String
 *
 *  puts color.to_hex
 *  => "#ff0000"
 */

VALUE
subColorToHex(VALUE self)
{
  char buf[8] = { 0 };
  VALUE red = Qnil, green = Qnil, blue = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@red",   red);
  GET_ATTR(self, "@green", green);
  GET_ATTR(self, "@blue",  blue);

  snprintf(buf, sizeof(buf), "#%02x%02x%02x", (int)(FIX2INT(red) & 0xff),
    (int)(FIX2INT(green) & 0xff), (int)(FIX2INT(blue) & 0xff));

  return rb_str_new2(buf);
} /* }}} */

/* subColorToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Color object to String
 *
 *  puts color
 *  => "<>123456789<>"
 */

VALUE
subColorToString(VALUE self)
{
  char buf[20] = { 0 };
  VALUE pixel = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@pixel", pixel);

  snprintf(buf, sizeof(buf), "%s#%ld%s",
    SEPARATOR, NUM2LONG(pixel), SEPARATOR);

  return rb_str_new2(buf);
} /* }}} */

/* subColorOperatorPlus {{{ */
/*
 * call-seq: +(string) -> String
 *
 * Convert self to String and add String
 *
 *  color + "subtle"
 *  => "<>123456789<>subtle"
 */

VALUE
subColorOperatorPlus(VALUE self,
  VALUE value)
{
  return subSubtlextConcat(subColorToString(self), value);
} /* }}} */

/* subColorEqual {{{ */
/*
 * call-seq: ==(other) -> True or False
 *
 * Whether both objects have the same values (based on pixel)
 *
 *  object1 == object2
 *  => true
 */

VALUE
subColorEqual(VALUE self,
  VALUE other)
{
  return ColorEqual(self, other, False);
} /* }}} */

/* subColorEqualTyped {{{ */
/*
 * call-seq: eql?(other) -> True or False
 *
 * Whether both objects have the same values and types (based on pixel)
 *
 *  object1.eql? object2
 *  => true
 */

VALUE
subColorEqualTyped(VALUE self,
  VALUE other)
{
  return ColorEqual(self, other, True);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
