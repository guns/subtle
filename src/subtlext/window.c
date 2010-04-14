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
  Window        win;
  SubFont       *font;
  SubText       *text;
  VALUE         instance;
  unsigned long fg, bg;
  int           width;
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
      subSharedTextFree(w->text);
      subSharedFontKill(display, w->font);

      free(w);
    }
} /* }}} */

/* subWindowAlloc {{{ */
/*
 * call-seq: new(geometry) -> Subtlext::Window
 *
 * Allocate new Window object
 **/

VALUE
subWindowAlloc(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Create window */
  w = (SubtlextWindow *)subSharedMemoryAlloc(1, sizeof(SubtlextWindow));
  w->instance = Data_Wrap_Struct(self, WindowMark, 
    WindowSweep, (void *)w);

  return w->instance;
} /* }}} */

/* subWindowInit {{{ 
 *
 * call-seq: new(geometry) -> Subtlext::Window
 *
 * Initialize Window object
 *
 *  win = Subtlext::Window.new(:x => 5, :y => 5) do |w|
 *    s.background = "#ffffff"
 *  end
 */

VALUE
subWindowNew(VALUE self,
  VALUE geometry)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)  
    {
      int data[4] = { 0, 0, 1, 1 };
      VALUE geom = Qnil;
      XSetWindowAttributes sattrs;

      subSubtlextConnect(); ///< Implicit open connection

      /* Parse geometry hash */
      if(T_HASH == rb_type(geometry))
        {
          int i;
          const char *syms[] = { "x", "y", "width", "height" };

          for(i = 0; 4 > i; i++)
            {
              VALUE sym = CHAR2SYM(syms[i]);

              if(RTEST(rb_hash_lookup(geometry, sym)))
                data[i] = FIX2INT(rb_hash_aref(geometry, sym));
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
      w->font          = subSharedFontNew(display, "-*-fixed-*-*-*-*-10-*-*-*-*-*-*-*");
      w->text          = subSharedTextNew();
      w->bg            = BlackPixel(display, DefaultScreen(display));
      
      /* Store data */
      rb_iv_set(w->instance, "@win",      LONG2NUM(w->win));
      rb_iv_set(w->instance, "@geometry", geom);
      
      /* Yield to block if given */
      if(rb_block_given_p())
        {
          //rb_yield_values(1, w->instance);
          rb_obj_instance_eval(0, 0, w->instance);
        }

      XSync(display, False); ///< Sync with X
    }

  return Qnil;
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
            hint.res_class = "Subtlext";

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
      SubFont *f = NULL;

      switch(rb_type(value))
        {
          case T_STRING:
            font = RSTRING_PTR(value);

            /* Create window font */
            if(!(f = subSharedFontNew(display, font)))
              rb_raise(rb_eArgError, "Unknown value type");

            /* Replace font */
            if(w->font) subSharedFontKill(display, w->font);

            w->font = f;
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }
    }

  return Qnil;
} /* }}} */

/* subWindowForegroundWriter {{{ */
/*
 * call-seq: foreground=(color) -> nil
 *
 * Set foreground color of a window
 *
 *  win.foreground = "#000000"
 *  => nil
 */

VALUE
subWindowForegroundWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      /* Check object type */
      switch(rb_type(value))
        {
          case T_STRING:
            w->fg = subSharedParseColor(display, RSTRING_PTR(value));
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
      /* Check object type */
      switch(rb_type(value))
        {
          case T_STRING:
            w->bg = subSharedParseColor(display, RSTRING_PTR(value));

            XSetWindowBackground(display, w->win, w->bg);
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
      unsigned long color = WhitePixel(display, DefaultScreen(display));

      /* Check object type */
      switch(rb_type(value))
        {
          case T_STRING:
            color = subSharedParseColor(display, RSTRING_PTR(value));

            XSetWindowBorder(display, w->win, color);
            XFlush(display);
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
      int width = 3;

      /* Check object type */
      if(FIXNUM_P(value))
        {
          width = FIX2INT(value);

          XSetWindowBorderWidth(display, w->win, width);
          XFlush(display);
        }
      else rb_raise(rb_eArgError, "Unknown value type");
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
  VALUE text)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w) w->width = subSharedTextParse(display, w->font, w->text,
    RSTRING_PTR(text));

  return Qnil;
} /* }}} */

/* subWindowInput {{{ */
/*
 * call-seq: input -> String
 *
 * Get input
 *
 *  inp = input
 *  => "subtle"
 */

VALUE
subWindowInput(VALUE self)
{
  SubtlextWindow *w = NULL;
  VALUE ret = Qnil;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w) 
    {
      XEvent ev;
      char buf[32] = { 0 }, text[4096] = { 0 };
      int num = 0, len = 0, running = True;
      KeySym sym;

      XGrabKeyboard(display, DefaultRootWindow(display), True, 
        GrabModeAsync, GrabModeAsync, CurrentTime);
      XMapRaised(display, w->win);
      XSelectInput(display, w->win, KeyPressMask);
      XSetInputFocus(display, w->win, RevertToPointerRoot, CurrentTime);
          XFlush(display);

      while(running)
        {
          XClearWindow(display, w->win);

          subSharedTextRender(display, DefaultGC(display, 0), w->font,
            w->win, 3, w->font->y, w->fg, w->bg, w->text);
          
          subSharedTextDraw(display, DefaultGC(display, 0), w->font,
            w->win, w->width + 5, w->font->y, w->fg, w->bg, text);

          XFlush(display);
          XNextEvent(display, &ev);

          switch(ev.type)
            {
              case KeyPress: 
                num = XLookupString(&ev.xkey, buf, sizeof(buf), &sym, NULL);

                switch(sym)
                  {
                    case XK_Return:
                    case XK_KP_Enter:
                      running = False;
                      break;
                    case XK_Escape:
                      running = False;
                      text[0] = 0;
                      break;
                    case XK_BackSpace:
                      if(0 < len) text[--len] = 0;
                      break;
                    default:
                      buf[num] = 0;
                      strncpy(text + len, buf, sizeof(text) - len);
                      len += num;
                      break;
                  }
                break;
              default: break;
            }
        }

      XUngrabKeyboard(display, CurrentTime);

      ret = rb_str_new2(text);
    }

  return ret;
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
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w) 
    {
      XMapRaised(display, w->win);
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
