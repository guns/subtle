 /**
  *
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
        rb_raise(rb_eArgError, "Unknown value type");
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

  subSubtlextConnect(); ///< Implicit open connection

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
 * Convert Color object to <b>rrrrggggbbbb</b> hex String
 *
 *  puts color.to_hex
 *  => "#ff0000"
 */

VALUE
subColorToHex(VALUE self)
{
  char buf[14] = { 0 };
  int red = 0, green = 0, blue = 0;

  /* Get rgb values */
  red   = FIX2INT(rb_iv_get(self, "@red"));
  green = FIX2INT(rb_iv_get(self, "@green"));
  blue  = FIX2INT(rb_iv_get(self, "@blue"));

  snprintf(buf, sizeof(buf), "#%04x%04x%04x", red, green, blue);

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
  VALUE pixel = rb_iv_get(self, "@pixel");

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

// vim:ts=2:bs=2:sw=2:et:fdm=marker
