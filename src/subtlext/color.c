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
  VALUE color)
{
  XColor xcolor = { 0 };

  subSubtlextConnect(); ///< Implicit open connection

  /* Check arguments */
  switch(rb_type(color))
    {
      case T_STRING:
        xcolor.pixel = subSharedParseColor(display, RSTRING_PTR(color));
        break;
      case T_FIXNUM:
        xcolor.pixel = FIX2INT(color);
        break;
      case T_BIGNUM:
        xcolor.pixel = NUM2LONG(color);
        break;
      default:
        rb_raise(rb_eArgError, "Unknown value type");
        return Qnil;
    }

  /* Get color values */
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
