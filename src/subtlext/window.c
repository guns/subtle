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

/* Typedefs {{{ */
typedef struct subtlextwindow_t
{
  GC gc;
  XFontStruct *xfs;
  VALUE instance;
} subWindow;
/* }}} */

/* WindowMark {{{ */
static void
WindowMark(subWindow *w)
{
  if(w) rb_gc_mark(w->instance);
} /* }}} */

/* WindowSweep {{{ */
static void
WindowSweep(subWindow *w)
{
  if(w)
    {
      XFreeGC(display, w->gc);
      XFreeFont(display, w->xfs);
      free(w);
    }
  
} /* }}} */

/* subWindowNew {{{ */
/*
 * call-seq: new(name, geometry, bw, foreground, background, border) -> Subtlext::Window
 *
 * Create a new Window object
 *
 *  win = Subtlext::Window.new(:x => 5, :y => 5) do |w|
 *    s.background = "#ffffff"
 *  end
 */

VALUE
subWindowNew(VALUE self,
  VALUE options)
{
  VALUE win = Qnil;
  subWindow *w = NULL;

  subSubtlextConnect(); ///< Implicit open connection

  /* Create window */
  w = (subWindow *)subSharedMemoryAlloc(1, sizeof(subWindow));
  win = Data_Wrap_Struct(self, WindowMark, 
    WindowSweep, (void *)w);

  /* Parse options hash */
  if(T_HASH == rb_type(options))
    {
    }
  else rb_raise(rb_eArgError, "Unknown value type");
  
  /* Yield to block if given */
  if(rb_block_given_p()) 
    rb_yield_values(1, win);

  return win;
}

VALUE
subWindowNameWriter(VALUE self,
  VALUE name)
{
  char *str = NULL;
  Window win = None;
  XClassHint hint;
  XTextProperty text;
  
  /* Set Window informations */
  str = RSTRING_PTR(name);
  hint.res_name  = str;
  hint.res_class = str;

  win = NUM2LONG(rb_iv_get(self, "@win"));

  XSetClassHint(display, win, &hint);
  XStringListToTextProperty(&str, 1, &text);
  XSetWMName(display, win, &text);

  return Qnil;
}

#if 0
  char *name = NULL;
  int x = 0, y = 0, width = 50, height = 50, bw = 1;
  unsigned long fg = 0, bg = 0, bo = 0;
  Window win = None;
  XSetWindowAttributes sattrs;
  XGCValues gvals;
  VALUE args[6] = { Qnil }, klass = Qnil, geometry = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  rb_scan_args(argc, argv, "06", &args[0], &args[1], &args[2], 
    &args[3], &args[4], &args[5]);

  /* Collect data */
  name     = RSTRING_PTR(args[0]);
  klass    = rb_const_get(mod, rb_intern("Geometry"));
  geometry = rb_funcall(klass, rb_intern("new"), 1, args[1]);

  /* Parse arguments */
  if(T_FIXNUM == rb_type(args[2]))
    bw = FIX2INT(args[2]);

  if(T_STRING == rb_type(args[3]))
    fg = subSharedParseColor(RSTRING_PTR(args[3]));

  if(T_STRING == rb_type(args[4]))
    bg = subSharedParseColor(RSTRING_PTR(args[4]));

  if(T_STRING == rb_type(args[5]))
    bo = subSharedParseColor(RSTRING_PTR(args[5]));

  /* Create window font */
  w->xfs           = XLoadQueryFont(display, "-*-fixed-*-*-*-*-10-*-*-*-*-*-*-*");
  gvals.font       = w->xfs->fid;
  gvals.foreground = fg;

  /* Create window gc */
  w->gc       = XCreateGC(display, DefaultRootWindow(display), 
    GCFont|GCForeground, &gvals);
  w->instance = Data_Wrap_Struct(self, WindowMark, 
    WindowSweep, (void *)w);

  /* Parse geometry */
  if(!NIL_P(geometry))
    {
      x      = FIX2INT(rb_iv_get(geometry, "@x"));
      y      = FIX2INT(rb_iv_get(geometry, "@y"));
      width  = FIX2INT(rb_iv_get(geometry, "@width"));
      height = FIX2INT(rb_iv_get(geometry, "@height"));
    }

  /* Create window */
  sattrs.override_redirect = True;
  sattrs.background_pixel  = bg;
  sattrs.border_pixel      = bo;

  win = XCreateWindow(display, DefaultRootWindow(display), 
    x, y, width, height, 1, CopyFromParent, CopyFromParent, CopyFromParent,
    CWOverrideRedirect|CWBackPixel|CWBorderPixel, &sattrs);

  XSync(display, False); ///< Sync with X

  /* Store data */
  rb_iv_set(w->instance, "@win",      LONG2NUM(win));
  rb_iv_set(w->instance, "@geometry", geometry);

  free(text.value);

  return w->instance;
#endif  
/* }}} */

/* subWindowBackgroundWriter {{{ */
/*
 * call-seq: background=(color) -> nil
 *
 * Set Background color of a window
 *
 *  win.background = "#000000"
 *  => nil
 */

VALUE
subWindowBackgroundWriter(VALUE self,
  VALUE value)
{
  subWindow *w = NULL;

  Data_Get_Struct(self, subWindow, w);
  if(w)
    {
      VALUE win = Qnil;
      unsigned long color = BlackPixel(display, DefaultScreen(display));

      switch(rb_type(value))
        {
          case T_STRING:
            color = subSharedParseColor(RSTRING_PTR(value));
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }

      win = rb_iv_get(self, "@win");
      XSetWindowBackground(display, NUM2LONG(win), color);
    }

  return Qnil;
} /* }}} */

/* subWindowGeometryReader {{{ */
/*
 * call-seq: gemetry -> Subtlext::Geometry
 *
 * Get geometry of a window
 *
 *  win.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subWindowGeometryReader(VALUE self,
  VALUE value)
{
  return rb_iv_get(self, "@geometry");
} /* }}} */

/* subWindowShow {{{ */
/*
 * call-seq: show() -> nil
 *
 * Show a Window
 *
 *  win.show
 *  => nil
 */

VALUE
subWindowShow(VALUE self)
{
  VALUE win = rb_iv_get(self, "@win");

  if(RTEST(win)) 
    {
      XMapRaised(display, NUM2LONG(win));
      XSync(display, False); ///< Sync with X
    }

  return Qnil;
} /* }}} */

/* subWindowHide {{{ */
/*
 * call-seq: hide() -> nil
 *
 * Hide a Window
 *
 *  win.hide
 *  => nil
 */

VALUE
subWindowHide(VALUE self)
{
  VALUE win = rb_iv_get(self, "@win");

  if(RTEST(win)) 
    {
      XUnmapWindow(display, NUM2LONG(win));
      XSync(display, False); ///< Sync with X
    }

  return Qnil;
} /* }}} */

/* subWindowPuts {{{ */
/*
 * call-seq: puts(string, x, y) -> nil
 *
 * Puts string into window at x/y coordinates
 *
 *  win.puts("subtle", 5, 10)
 *  => nil
 */

VALUE
subWindowPuts(int argc,
  VALUE *argv,
  VALUE self)
{
  subWindow *w = NULL;

  Data_Get_Struct(self, subWindow, w);
  if(w)
    {
      int x = 0, y = 0;
      char *string = NULL;
      VALUE args[3] = { Qnil }, win = Qnil;

      rb_scan_args(argc, argv, "03", &args[0], &args[1], &args[2]);

      win    = rb_iv_get(self, "@win");
      string = RSTRING_PTR(args[0]);

      if(FIXNUM_P(args[1]) && FIXNUM_P(args[2]))
        {
          x = FIX2INT(args[1]);
          y = FIX2INT(args[2]);
        }

      XDrawString(display, NUM2LONG(win), w->gc, x, y, string, strlen(string));
      XSync(display, False); ///< Sync with X
    }

  return Qnil;
} /* }}} */

/* subWindowKill {{{ */
/*
 * call-seq: kill() -> nil
 *
 * Kill a Window
 *
 *  win.kill
 *  => nil
 */

VALUE
subWindowKill(VALUE self)
{
  VALUE win = rb_iv_get(self, "@win");

  if(RTEST(win)) 
    {
      XDestroyWindow(display, NUM2LONG(win));
      rb_iv_set(self, "@win", Qnil); 
    }

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
