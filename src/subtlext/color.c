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

/* subColorInit {{{ */
/*
 * call-seq: new(color) -> Subtlext::Color
 *
 * Create new Color object
 *
 *  tag = Subtlext::Color.new("#336699")
 *  => #<Subtlext::Color:xxx>
 */

VALUE
subColorInit(VALUE self,
  VALUE color)
{
  /* Check arguments */
  if(T_STRING == rb_type(color))
    {
      unsigned long pixel = 0;

      subSubtlextConnect(); ///< Implicit open connection

      pixel = subSharedParseColor(display, RSTRING_PTR(color));

      rb_iv_set(self, "@pixel", INT2FIX(pixel));
    }
  else
    {
      rb_raise(rb_eArgError, "Unknown value type");
      return Qnil;
    }

  return self;
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
  char buf[20];
  VALUE pixel = rb_iv_get(self, "@pixel");

  snprintf(buf, sizeof(buf), "%s#%ld%s",
    SEPARATOR, NUM2LONG(pixel), SEPARATOR);

  return rb_str_new2(buf);
} /* }}} */

/* subColorOperatorPlus {{{ */
/*
 * call-seq: +(string) -> String
 *
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
