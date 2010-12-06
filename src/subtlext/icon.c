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

#include <unistd.h>
#include "subtlext.h"

/* Typedef {{{ */
typedef struct subtlexticon_t
{
  GC           gc;
  Pixmap       pixmap;
  unsigned int width, height;
  VALUE        instance;
} SubtlextIcon;
/* }}} */

/* IconMark {{{ */
static void
IconMark(SubtlextIcon *i)
{
  if(i) rb_gc_mark(i->instance);
} /* }}} */

/* IconSweep {{{ */
static void
IconSweep(SubtlextIcon *i)
{
  if(i)
    {
      XFreePixmap(display, i->pixmap);
      if(0 != i->gc) XFreeGC(display, i->gc);

      free(i);
    }
} /* }}} */

/* subIconAlloc {{{ */
/*
 * call-seq: new(path)          -> Subtle::Icon
 *           new(width, height) -> Subtle::Icon
 *
 * Allocate new Icon object
 */

VALUE
subIconAlloc(VALUE self)
{
  SubtlextIcon *i = NULL;

  /* Create icon */
  i = (SubtlextIcon *)subSharedMemoryAlloc(1, sizeof(SubtlextIcon));
  i->instance = Data_Wrap_Struct(self, IconMark, IconSweep, (void *)i);

  return i->instance;
} /* }}} */

/* subIconInit {{{ */
/*
 * call-seq: initialize(path)          -> Subtle::Icon
 *           initialize(width, height) -> Subtle::Icon
 *
 * Initialize Icon object
 *
 *  icon = Subtlext::Icon.new("/path/to/icon")
 *  => #<Subtlext::Icon:xxx>
 *
 *  icon = Subtlext::Icon.new(8, 8)
 *  => #<Subtlext::Icon:xxx>
 */

VALUE
subIconInit(int argc,
  VALUE *argv,
  VALUE self)
{
  SubtlextIcon *i = NULL;

  Data_Get_Struct(self, SubtlextIcon, i);
  if(i)
    {
      VALUE arg1 = Qnil, arg2 = Qnil;

      rb_scan_args(argc, argv, "02", &arg1, &arg2);

      subSubtlextConnect(NULL); ///< Implicit open connection

      /* Find or create icon */
      if(T_STRING == rb_type(arg1)) ///< Icon path
        {
          int hotx = 0, hoty = 0;
          char buf[100] = { 0 }, *file = RSTRING_PTR(arg1);

          /* Find file */
          if(-1 != access(file, R_OK))
            snprintf(buf, sizeof(buf), "%s", file);
          else
            {
              char fallback[256] = { 0 }, *data = getenv("XDG_DATA_HOME");

              /* Combine paths */
              snprintf(fallback, sizeof(fallback), "%s/.local/share",
                getenv("HOME"));
              snprintf(buf, sizeof(buf), "%s/subtle/icons/%s",
                data ? data : fallback, file);

              if(-1 == access(buf, R_OK))
                rb_raise(rb_eStandardError, "Icon not found `%s'", file);
            }

          /* Reading icon file */
          if(BitmapSuccess != XReadBitmapFile(display, DefaultRootWindow(display),
              buf, &i->width, &i->height, &i->pixmap, &hotx, &hoty))
            {
              rb_raise(rb_eStandardError, "Icon not found `%s'", buf);

              return Qnil;
            }
        }
      else if(FIXNUM_P(arg1) && FIXNUM_P(arg2)) ///< Icon dimensions
        {
          /* Create empty pixmap */
          i->width  = FIX2INT(arg1);
          i->height = FIX2INT(arg2);
          i->pixmap = XCreatePixmap(display, DefaultRootWindow(display),
            i->width, i->height, 1);
        }
      else rb_raise(rb_eArgError, "Unexpected value-types");

      /* Update object */
      rb_iv_set(i->instance, "@width",  INT2FIX(i->width));
      rb_iv_set(i->instance, "@height", INT2FIX(i->height));

      XSync(display, False); ///< Sync all changes

      subSharedLogDebug("new=icon, width=%03d, height=%03d\n",
        i->width, i->height);
    }

  return Qnil;
} /* }}} */

/* subIconDraw {{{ */
/*
 * call-seq: draw(x, y) -> nil
 *
 * Draw a pixel on the icon
 *
 *  icon.draw(1, 1)
 *  => nil
 */

VALUE
subIconDraw(VALUE self,
  VALUE x,
  VALUE y)
{
  /* Check object type */
  if(FIXNUM_P(x) && FIXNUM_P(y))
    {
      SubtlextIcon *i = NULL;

      Data_Get_Struct(self, SubtlextIcon, i);
      if(i)
        {
          XGCValues gvals;

          if(0 == i->gc) ///< Create on demand
            i->gc = XCreateGC(display, i->pixmap, 0, NULL);

          /* Update GC */
          gvals.foreground = 1;
          gvals.background = 0;
          XChangeGC(display, i->gc, GCForeground|GCBackground, &gvals);

          XDrawPoint(display, i->pixmap, i->gc, FIX2INT(x), FIX2INT(y));

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return Qnil;
} /* }}} */

/* subIconDrawRect {{{ */
/*
 * call-seq: draw_rect(x, y, width, height, fill) -> nil
 *
 * Draw a rect on the icon
 *
 *  icon.draw_rect(1, 1, 10, 10, false)
 *  => nil
 */

VALUE
subIconDrawRect(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE data[5] = { Qnil };

  rb_scan_args(argc, argv, "05", &data[0], &data[1], &data[2], &data[3], &data[4]);

  if(FIXNUM_P(data[0]) && FIXNUM_P(data[1]) &&
      FIXNUM_P(data[2]) && FIXNUM_P(data[3]))
    {
      SubtlextIcon *i = NULL;

      Data_Get_Struct(self, SubtlextIcon, i);
      if(i)
        {
          XGCValues gvals;

          if(0 == i->gc) ///< Create on demand
            i->gc = XCreateGC(display, i->pixmap, 0, NULL);

          /* Update GC */
          gvals.foreground = 1;
          gvals.background = 0;
          XChangeGC(display, i->gc, GCForeground|GCBackground, &gvals);

          /* Draw rect */
          if(Qtrue == data[4])
            {
              XFillRectangle(display, i->pixmap, i->gc, FIX2INT(data[0]),
                FIX2INT(data[1]), FIX2INT(data[2]), FIX2INT(data[3]));
            }
          else XDrawRectangle(display, i->pixmap, i->gc, FIX2INT(data[0]),
            FIX2INT(data[1]), FIX2INT(data[2]), FIX2INT(data[3]));

          XFlush(display);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-types");

  return Qnil;
} /* }}} */

/* subIconClear {{{ */
/*
 * call-seq: clear -> nil
 *
 * Clear the icon
 *
 *  icon.clear
 *  => nil
 */

VALUE
subIconClear(VALUE self)
{
  SubtlextIcon *i = NULL;

  Data_Get_Struct(self, SubtlextIcon, i);
  if(i)
    {
      XGCValues gvals;

      if(0 == i->gc) ///< Create on demand
        i->gc = XCreateGC(display, i->pixmap, 0, NULL);

      /* Update GC */
      gvals.foreground = 0;
      gvals.background = 1;
      XChangeGC(display, i->gc, GCForeground|GCBackground, &gvals);

      XFillRectangle(display, i->pixmap, i->gc, 0, 0, i->width, i->height);

      XFlush(display);
    }

  return Qnil;
} /* }}} */

/* subIconToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Icon object to String
 *
 *  puts icon
 *  => "<>!4<>"
 */

VALUE
subIconToString(VALUE self)
{
  VALUE ret = Qnil;
  SubtlextIcon *i = NULL;

  Data_Get_Struct(self, SubtlextIcon, i);
  if(i)
    {
      char buf[20] = { 0 };

      snprintf(buf, sizeof(buf), "%s!%ld%s", SEPARATOR, i->pixmap, SEPARATOR);
      ret = rb_str_new2(buf);
    }

  return ret;
} /* }}} */

/* subIconOperatorPlus {{{ */
/*
* call-seq: +(string) -> String
*
* Convert self to String and add String
*
*  icon + "subtle"
*  => "<>!4<>subtle"
*/

VALUE
subIconOperatorPlus(VALUE self,
  VALUE value)
{
  return subSubtlextConcat(subIconToString(self), value);
} /* }}} */

/* subIconOperatorMult {{{ */
/*
* call-seq: *(value) -> String
*
* Convert self to String and multiply String
*
*  icon * 2
*  => "<>!4<><>!4<>"
*/

VALUE
subIconOperatorMult(VALUE self,
  VALUE value)
{
  VALUE ret = Qnil;

  /* Check id and name */
  if(FIXNUM_P(value))
    {
      /* Passthru to string class */
      ret = rb_funcall(subIconToString(self), rb_intern("*"), 1, value);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(value));

  return ret;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
