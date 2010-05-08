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
typedef struct subtlextwindowtext_t {
  int     x, y;
  SubText *text;
} SubtlextWindowText;

typedef struct subtlextwindow_t
{
  int                ntext;
  unsigned long      fg, bg;
  Window             win;
  VALUE              instance, completion;
  SubFont            *font;
  SubtlextWindowText *text;
} SubtlextWindow;
/* }}} */

/* WindowMark {{{ */
static void
WindowMark(SubtlextWindow *w)
{
  if(w)
    {
      rb_gc_mark(w->instance);

      if(RTEST(w->completion))
        rb_gc_mark(w->completion);
    }
} /* }}} */

/* WindowSweep {{{ */
static void
WindowSweep(SubtlextWindow *w)
{
  if(w)
    {
      int i;

      /* Free texts */
      for(i = 0; i < w->ntext; i++)
        subSharedTextFree(w->text[i].text);

      subSharedFontKill(display, w->font);

      free(w->text);
      free(w);
    }
} /* }}} */

/* WindowWrapCompletion {{{ */
static VALUE
WindowWrapCompletion(VALUE data)
{
  VALUE *rargs = (VALUE *)data;

  return rb_funcall(rargs[0], rb_intern("call"), 2, rargs[1], rargs[2]);
} /* }}} */

/* WindowExpose {{{ */
static void
WindowExpose(SubtlextWindow *w)
{
  int i;

  XClearWindow(display, w->win);

  for(i = 0; i < w->ntext; i++)
    subSharedTextRender(display, DefaultGC(display, 0), w->font,
      w->win, w->text[i].x, w->text[i].y, w->fg, w->bg, w->text[i].text);

 XSync(display, False); ///< Sync with X
} /* }}} */

/* subWindowInstantiate {{{ */
VALUE
subWindowInstantiate(VALUE geometry)
{
  VALUE klass = Qnil, win = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("Window"));
  win   = rb_funcall(klass, rb_intern("new"), 1, geometry);

  return win;
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
subWindowInit(VALUE self,
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

      /* Set window defaults */
      w->font       = subSharedFontNew(display, "-*-fixed-*-*-*-*-10-*-*-*-*-*-*-*");
      w->bg         = BlackPixel(display, DefaultScreen(display));
      w->completion = Qnil;

      /* Store data */
      rb_iv_set(w->instance, "@win",      LONG2NUM(w->win));
      rb_iv_set(w->instance, "@geometry", geom);

      /* Yield to block if given */
      if(rb_block_given_p())
        rb_yield_values(1, w->instance);

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

/* subWindowWrite {{{ */
/*
 * call-seq: write(x, y, text) -> nil
 *
 * Writes a string onto the window
 *
 *  win.write(10, 10, "subtle")
 *  => 15
 */

VALUE
subWindowWrite(VALUE self,
  VALUE x,
  VALUE y,
  VALUE text)
{
  int len = 0;
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      /* Check object type */
      if(T_FIXNUM == rb_type(x) && T_FIXNUM == rb_type(y) &&
          T_STRING == rb_type(text))
        {
          w->text = (SubtlextWindowText *)subSharedMemoryRealloc(w->text,
            (w->ntext + 1) * sizeof(SubtlextWindowText));

          /* Create new text item */
          w->text[w->ntext].text = subSharedTextNew();
          w->text[w->ntext].x    = FIX2INT(x);
          w->text[w->ntext].y    = FIX2INT(y);

          len = subSharedTextParse(display, w->font,
            w->text[w->ntext].text, RSTRING_PTR(text));

          w->ntext++;
        }
      else rb_raise(rb_eArgError, "Unknown value type");
    }

  return INT2FIX(len);
} /* }}} */

/* subWindowRead {{{ */
/*
 * call-seq: read(x, y) -> String
 *
 * Read input
 *
 *  string = read(10, 10)
 *  => "subtle"
 */

VALUE
subWindowRead(VALUE self,
  VALUE x,
  VALUE y)
{
  SubtlextWindow *w = NULL;
  VALUE ret = Qnil;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XEvent ev;
      int pos = 0, len = 0, running = True, start = 0, guess = -1;
      char buf[32] = { 0 }, text[1024] = { 0 }, last[32] = { 0 };
      KeySym sym;

      /* Check object type */
      if(T_FIXNUM != rb_type(x) || T_FIXNUM != rb_type(y))
        {
          rb_raise(rb_eArgError, "Unknown value type");

          return Qnil;
        }

      /* Grab server and focus */
      XGrabKeyboard(display, DefaultRootWindow(display), True,
        GrabModeAsync, GrabModeAsync, CurrentTime);
      XMapRaised(display, w->win);
      XSelectInput(display, w->win, KeyPressMask);
      XSetInputFocus(display, w->win, RevertToPointerRoot, CurrentTime);
          XFlush(display);

      while(running)
        {
          text[len] = '_';

          WindowExpose(w);

          subSharedTextDraw(display, DefaultGC(display, 0), w->font,
            w->win, FIX2INT(x), FIX2INT(y), w->fg, w->bg, text);

          XFlush(display);
          XNextEvent(display, &ev);

          switch(ev.type)
            {
              case KeyPress:
                pos = XLookupString(&ev.xkey, buf, sizeof(buf), &sym, NULL);

                switch(sym)
                  {
                    case XK_Return:
                    case XK_KP_Enter: /* {{{ */
                      running = False;
                      text[len] = 0; ///< Remove underscore
                      break; /* }}} */
                    case XK_Escape: /* {{{ */
                      running = False;
                      text[0] = 0;
                      break; /* }}} */
                    case XK_BackSpace: /* {{{ */
                      if(0 < len) 
                        {
                          text[len--] = 0;
                          text[len]   = 0;
                        }
                      break; /* }}} */
                    case XK_Tab: /* {{{ */
                      if(!(NIL_P(w->completion)))
                        {
                          int state = 0;
                          VALUE rargs[3] = { Qnil }, result = Qnil;

                          /* Select guess number */
                          if(0 == ++guess) 
                            {
                              int i;

                              /* Find start of current word */
                              for(i = len; 0 < i; i--)
                                if(' ' == text[i]) 
                                  {
                                    start = i + 1;
                                    break;
                                  }

                              strncpy(last, text + start, len - start); ///< Store match
                            }

                          /* Wrap up data */
                          rargs[0] = w->completion;
                          rargs[1] = rb_str_new2(last);
                          rargs[2] = INT2FIX(guess);

                          /* Carefully call completion proc */
                          result = rb_protect(WindowWrapCompletion, (VALUE)&rargs, &state);
                          if(state)
                            {
                              printf("Error in completion func\n");
                              rb_backtrace();

                              continue;
                            }

                          if(!(NIL_P(result)))
                            {
                              strncpy(text + start, RSTRING_PTR(result), sizeof(text) - len);

                              len = strlen(text);
                            }
                        }
                      break; /* }}} */
                    default: /* {{{ */
                      guess    = -1;
                      buf[pos] = 0;
                      strncpy(text + len, buf, sizeof(text) - len);
                      len += pos;
                      break; /* }}} */
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

/* subWindowClear {{{ */
/*
 * call-seq: clear -> nil
 *
 * Clear window
 *
 *  win.clear
 *  => nil
 */

VALUE
subWindowClear(VALUE self)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      int i;

      for(i = 0; i < w->ntext; i++)
        subSharedTextFree(w->text[i].text);

      w->ntext = 0;

      free(w->text);
    }

  return Qnil;
} /* }}} */

/* subWindowCompletion {{{ */
/*
 * call-seq: completion(&block) -> nil
 *
 * Add completion proc
 *
 *  win.completion do |str, guess|
 *    str
 *  end
 */

VALUE
subWindowCompletion(VALUE self)
{
  SubtlextWindow *w = NULL;

  rb_need_block();

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && rb_block_given_p())
    {
      VALUE p = rb_block_proc();
      int arity = rb_proc_arity(p);

      if(2 == arity) w->completion = p;
      else rb_raise(rb_eArgError, "Wrong number of arguments (%d for 2)", arity);
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
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XMapRaised(display, w->win);
      WindowExpose(w);
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
      /* Destroy window */
      XDestroyWindow(display, NUM2LONG(win));
      rb_iv_set(self, "@win", Qnil);
    }

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
