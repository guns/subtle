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
  GC          gc;
  Window      win;
  XFontStruct *xfs;
  VALUE       instance;
  SubText     *text;
} SubtlextWindow;
/* }}} */

/* WindowMark {{{ */
static void
WindowMark(SubtlextWindow *w)
{
  if(w) rb_gc_mark(w->instance);
} /* }}} */

/* WindowSweep {{{ */
static void
WindowSweep(SubtlextWindow *w)
{
  if(w)
    {
      XFreeGC(display, w->gc);
      XFreeFont(display, w->xfs);

      subSharedTextFree(w->text);

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
  SubtlextWindow *w = NULL;
  int data[4] = { 0, 0, 1, 1 };
  VALUE geom = Qnil;
  XSetWindowAttributes sattrs;
  XGCValues gvals;

  subSubtlextConnect(); ///< Implicit open connection

  /* Wrap data */
  w = (SubtlextWindow *)subSharedMemoryAlloc(1, sizeof(SubtlextWindow));
  w->instance = Data_Wrap_Struct(self, WindowMark, 
    WindowSweep, (void *)w);

  /* Parse options hash */
  if(T_HASH == rb_type(options))
    {
      int i;
      const char *syms[] = { "x", "y", "width", "height" };

      for(i = 0; 4 > i; i++)
        {
          VALUE sym = CHAR2SYM(syms[i]);

          if(Qtrue == rb_hash_lookup(options, sym))
            data[i] = rb_hash_aref(options, sym);
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  /* Create geometry */
  geom = subGeometryInstantiate(data[0], data[1], data[2], data[3]);

  /* Create window */
  sattrs.override_redirect = True;

  w->win = XCreateWindow(display, DefaultRootWindow(display), 
    data[0], data[1], data[2], data[3], 1, CopyFromParent, CopyFromParent, CopyFromParent,
    CWOverrideRedirect, &sattrs);

  /* Create window defaults */
  w->xfs           = XLoadQueryFont(display, "-*-fixed-*-*-*-*-10-*-*-*-*-*-*-*");
  gvals.font       = w->xfs->fid;
  gvals.foreground = WhitePixel(display, DefaultScreen(display));
  w->gc            = XCreateGC(display, w->win, GCFont|GCForeground, &gvals);

  /* Store data */
  rb_iv_set(w->instance, "@win",      LONG2NUM(w->win));
  rb_iv_set(w->instance, "@geometry", geom);
  
  /* Yield to block if given */
  if(rb_block_given_p())
    {
      rb_yield_values(1, w->instance);
      rb_obj_instance_eval(0, 0, w->instance);
    }

  XSync(display, False); ///< Sync with X

  return w->instance;
} /* }}} */

/* subWindowNameWriter {{{ */
/*
 * call-seq: name=(str) -> nil
 *
 * Set name of a window
 *
 *  win.name = "sublet"
 *  => nil
 */

VALUE
subWindowNameWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      Window win = None;
      XClassHint hint;
      XTextProperty text;
      char *name = NULL;

      /* Check object type */
      switch(rb_type(value))
        {
          case T_STRING:
            name = RSTRING_PTR(value);
            win  = NUM2LONG(rb_iv_get(self, "@win"));

            /* Set Window informations */
            hint.res_name  = name;
            hint.res_class = name;

            XSetClassHint(display, win, &hint);
            XStringListToTextProperty(&name, 1, &text);
            XSetWMName(display, win, &text);

            free(text.value);
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }
    }

  return Qnil;
} /* }}} */

/* subWindowFontWriter {{{ */
/*
 * call-seq: font=(str) -> nil
 *
 * Set font of a window
 *
 *  win.font = "-*-fixed-*-*-*-*-10-*-*-*-*-*-*-*"
 *  => nil
 */

VALUE
subWindowFontWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      char *font = NULL;
      XGCValues gvals;
      XFontStruct *xfs = NULL;

      switch(rb_type(value))
        {
          case T_STRING:
            font = RSTRING_PTR(value);

            /* Create window font */
            if(!(xfs = XLoadQueryFont(display, font)))
              rb_raise(rb_eArgError, "Unknown value type");

            /* Replace font */
            if(w->xfs) XFreeFont(display, w->xfs);

            w->xfs     = xfs;
            gvals.font = xfs->fid;
      
            XChangeGC(display, w->gc, GCFont, &gvals);
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }
    }

  return Qnil;
} /* }}} */

/* subWindowBackgroundWriter {{{ */
/*
 * call-seq: background=(color) -> nil
 *
 * Set background color of a window
 *
 *  win.background = "#000000"
 *  => nil
 */

VALUE
subWindowBackgroundWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      VALUE win = Qnil;
      unsigned long color = BlackPixel(display, DefaultScreen(display));

      /* Check object type */
      switch(rb_type(value))
        {
          case T_STRING:
            color = subSharedParseColor(display, RSTRING_PTR(value));
            win   = rb_iv_get(self, "@win");

            XSetWindowBackground(display, NUM2LONG(win), color);
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }
    }

  return Qnil;
} /* }}} */

/* subWindowBorderColorWriter {{{ */
/*
 * call-seq: border_color=(color) -> nil
 *
 * Set border color of a window
 *
 *  win.border_color = "#000000"
 *  => nil
 */

VALUE
subWindowBorderColorWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      VALUE win = Qnil;
      unsigned long color = WhitePixel(display, DefaultScreen(display));

      /* Check object type */
      switch(rb_type(value))
        {
          case T_STRING:
            color = subSharedParseColor(display, RSTRING_PTR(value));
            win   = rb_iv_get(self, "@win");

            XSetWindowBorder(display, NUM2LONG(win), color);
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }
    }

  return Qnil;
} /* }}} */

/* subWindowBorderSizeWriter {{{ */
/*
 * call-seq: border_size=(width) -> nil
 *
 * Set border size of a window
 *
 *  win.border_size = 3
 *  => nil
 */

VALUE
subWindowBorderSizeWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      VALUE win = Qnil;
      int width = 3;

      /* Check object type */
      if(FIXNUM_P(value))
        {
          width = FIX2INT(value);
          win   = rb_iv_get(self, "@win");

          XSetWindowBorder(display, NUM2LONG(win), width);
        }
      else
        rb_raise(rb_eArgError, "Unknown value type");
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
subWindowGeometryReader(VALUE self)
{
  return rb_iv_get(self, "@geometry");
} /* }}} */

/* subWindowTextWriter {{{ */
/*
 * call-seq: text(str, color) -> nil
 *
 * Add test to window
 *
 *  win.text("subtle", "#fff000")
 *  => nil
 */

VALUE
subWindowTextWriter(VALUE self,
  VALUE text,
  VALUE color)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      
    }

  return Qnil;
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
