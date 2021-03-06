 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* Flags {{{ */
#define WINDOW_COMPLETION_FUNC (1L << 1)
#define WINDOW_INPUT_FUNC      (1L << 2)
#define WINDOW_FOREIGN_WIN     (1L << 3)
/* }}} */

/* Typedefs {{{ */
typedef struct subtlextwindowtext_t {
  int     x, y;
  SubText *text;
} SubtlextWindowText;

typedef struct subtlextwindow_t
{
  int                flags, ntext;
  unsigned long      fg, bg;
  Window             win;
  VALUE              instance;
  SubFont            *font;
  SubtlextWindowText *text;
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
      /* Destroy window */
      if(!(w->flags & WINDOW_FOREIGN_WIN))
        XDestroyWindow(display, w->win);

      /* Free text */
      if(w->text)
        {
          int i;

          for(i = 0; i < w->ntext; i++)
            subSharedTextFree(w->text[i].text);

          free(w->text);
        }
      if(w->font) subSharedFontKill(display, w->font);

      free(w);
    }
} /* }}} */

/* WindowCall {{{ */
static VALUE
WindowCall(VALUE data)
{
  VALUE *rargs = (VALUE *)data;

  return rb_funcall(rargs[0], rargs[1], rargs[2], rargs[3], rargs[4]);
} /* }}} */

/* WindowExpose {{{ */
static void
WindowExpose(SubtlextWindow *w)
{
  if(w)
    {
      int i;

      XClearWindow(display, w->win);

      /* Render all text */
      for(i = 0; i < w->ntext; i++)
        subSharedTextRender(display, DefaultGC(display, 0), w->font,
          w->win, w->text[i].x, w->text[i].y, w->fg, w->fg,
          w->bg, w->text[i].text);

     XSync(display, False); ///< Sync with X
  }
} /* }}} */

/* WindowDefine {{{ */
static VALUE
WindowDefine(VALUE self,
  int argc,
  int flag,
  VALUE sym)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  rb_need_block();

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && rb_block_given_p())
    {
      VALUE p = rb_block_proc();
      int arity = rb_proc_arity(p);

      /* Check arity */
      if(argc == arity)
        {
          VALUE sing = rb_singleton_class(w->instance);

          w->flags |= flag;

          rb_funcall(sing, rb_intern("define_method"), 2, sym, p);
        }
      else rb_raise(rb_eArgError, "Wrong number of arguments (%d for %d)",
        arity, argc);
    }

  return Qnil;
} /* }}} */

/* Singleton */

/* subWindowSingOnce {{{ */
/*
 * call-seq: once(geometry) -> Value
 *
 * Show window once as long as proc runs
 *
 *  Subtlext::Window.once(:x => 10, :y => 10, :widht => 100, :height => 100) do |w|
 *    "test"
 *  end
 *  => "test"
 **/

VALUE
subWindowSingOnce(VALUE self,
  VALUE geometry)
{
  VALUE win = Qnil, ret = Qnil;

  rb_need_block();

  /* Create new window */
  win = subWindowInstantiate(geometry);

  /* Yield block */
  ret = rb_yield_values(1, win);

  subWindowKill(win);

  return ret;
} /* }}} */

/* Class */

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
 * Allocate space for new Window object.
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
 * call-seq: new(geometry, &block) -> Subtlext::Window
 *
 * Initialize Window object.
 *
 *  win = Subtlext::Window.new(:x => 5, :y => 5) do |w|
 *    s.background = "#ffffff"
 *  end
 */

VALUE
subWindowInit(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      VALUE geometry = Qnil;

      subSubtlextConnect(NULL); ///< Implicit open connection

      /* Check object type */
      switch(rb_type(value))
        {
          case T_HASH:
          case T_ARRAY:
              {
                XSetWindowAttributes sattrs;
                XRectangle r = { 0 };

                /* Create geometry */
                geometry = subGeometryInstantiate(0, 0, 1, 1);
                geometry = subGeometryInit(1, &value, geometry);

                subGeometryToRect(geometry, &r);

                /* Create window */
                sattrs.override_redirect = True;

                w->win = XCreateWindow(display, DefaultRootWindow(display),
                  r.x, r.y, r.width, r.height, 1, CopyFromParent,
                  CopyFromParent, CopyFromParent, CWOverrideRedirect, &sattrs);
              }
            break;
          case T_FIXNUM:
          case T_BIGNUM:
              {
                int x = 0, y = 0;
                unsigned int width = 0, height = 0, bw = 0, depth = 0;
                Window root = None;

                /* Update values */
                w->win    = FIX2LONG(value);
                w->flags |= WINDOW_FOREIGN_WIN;

                /* Get window geometry */
                if(XGetGeometry(display, w->win, &root,
                    &x, &y, &width, &height, &bw, &depth))
                  geometry = subGeometryInstantiate(x, y, width, height);
                else rb_raise(rb_eArgError,
                  "Unable to get geometry of window `%#lx'", w->win);
              }
            break;
          default: rb_raise(rb_eArgError, "Unexpected value-type `%s'",
            rb_obj_classname(value));
        }

      /* Store data */
      rb_iv_set(w->instance, "@win",      LONG2NUM(w->win));
      rb_iv_set(w->instance, "@geometry", geometry);
      rb_iv_set(w->instance, "@hidden",   Qtrue);

      /* Yield to block if given */
      if(rb_block_given_p())
        rb_yield_values(1, w->instance);

      /* Set window font */
      if(!w->font) w->font = subSharedFontNew(display,
          "-*-fixed-*-*-*-*-10-*-*-*-*-*-*-*");

      XSync(display, False); ///< Sync with X
    }

  return Qnil;
} /* }}} */

/* subWindowSubwindow {{{
 *
 * call-seq: subwindow(geometry, &block) -> Subtlext::Window or nil
 *
 * Create a subwindow of Window with given Geometry.
 *
 *  win.subwindow(:x => 5, :y => 5) do |w|
 *    s.background = "#ffffff"
 *  end
 */

VALUE
subWindowSubwindow(VALUE self,
  VALUE geometry)
{
  VALUE ret = Qnil;
  SubtlextWindow *w1 = NULL;

  Data_Get_Struct(self, SubtlextWindow, w1);
  if(w1)
    {
      SubtlextWindow *w2 = NULL;

      subSubtlextConnect(NULL); ///< Implicit open connection

      ret = subWindowInstantiate(geometry);

      Data_Get_Struct(ret, SubtlextWindow, w2);
      if(w2)
        {
          /* Yield to block if given */
          if(rb_block_given_p())
            rb_yield_values(1, w2->instance);

          XReparentWindow(display, w2->win, w1->win, 0, 0);
        }
    }

  return ret;
} /* }}} */

/* subWindowNameWriter {{{ */
/*
 * call-seq: name=(str) -> nil
 *
 * Set the WM_NAME of a Window-
 *
 *  win.name = "sublet"
 *  => nil
 */

VALUE
subWindowNameWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      Window win = None;
      XClassHint hint;
      XTextProperty text;
      char *name = NULL;

      /* Check object type */
      if(T_STRING == rb_type(value))
        {
          name = RSTRING_PTR(value);
          win  = NUM2LONG(rb_iv_get(self, "@win"));

          /* Set Window informations */
          hint.res_name  = name;
          hint.res_class = "Subtlext";

          XSetClassHint(display, win, &hint);
          XStringListToTextProperty(&name, 1, &text);
          XSetWMName(display, win, &text);

          free(text.value);
        }
      else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  return Qnil;
} /* }}} */

/* subWindowFontWriter {{{ */
/*
 * call-seq: font=(string) -> nil
 *
 * Set the font that is used for text inside of a W.indow
 *
 *  win.font = "-*-fixed-*-*-*-*-10-*-*-*-*-*-*-*"
 *  => nil
 */

VALUE
subWindowFontWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      char *font = NULL;
      SubFont *f = NULL;

      /* Check object type */
      if(T_STRING == rb_type(value))
        {
          font = RSTRING_PTR(value);

          /* Create window font */
          if(!(f = subSharedFontNew(display, font)))
            rb_raise(rb_eStandardError, "Failed loading font");

          /* Replace font */
          if(w->font) subSharedFontKill(display, w->font);

          w->font = f;
        }
      else rb_raise(rb_eStandardError, "Failed creating font");
    }

  return Qnil;
} /* }}} */

/* subWindowFontYReader {{{ */
/*
 * call-seq: font_y -> Fixnum
 *
 * Get y offset of the selected Window font.
 *
 *  win.font_y
 *  => 10
 */

VALUE
subWindowFontYReader(VALUE self)
{
  VALUE ret = INT2FIX(0);
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && w->font) ret = INT2FIX(w->font->y);

  return ret;
} /* }}} */

/* subWindowFontHeightReader {{{ */
/*
 * call-seq: font_height -> Fixnum
 *
 * Get the height of selected Window font.
 *
 *  win.font_height
 *  => 10
 */

VALUE
subWindowFontHeightReader(VALUE self)
{
  VALUE ret = INT2FIX(0);
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && w->font) ret = INT2FIX(w->font->height);

  return ret;
} /* }}} */

/* subWindowForegroundWriter {{{ */
/*
 * call-seq: foreground=(string) -> nil
 *           foreground=(array)  -> nil
 *           foreground=(hash)   -> nil
 *           foreground=(fixnum) -> nil
 *           foreground=(color)  -> nil
 *
 * Set the foreground color of this Window which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Color]  Copy color from a Color object
 *
 *  win.foreground = "#000000"
 *  => nil
 */

VALUE
subWindowForegroundWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w) w->fg = subColorPixel(value, Qnil, Qnil, NULL);

  return Qnil;
} /* }}} */

/* subWindowBackgroundWriter {{{ */
/*
 * call-seq: background=(string) -> nil
 *           background=(array)  -> nil
 *           background=(hash)   -> nil
 *           background=(fixnum) -> nil
 *           background=(color)  -> nil
 *
 * Set the background color of this Window which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Color]  Copy color from a Color object

 *  win.background = "#000000"
 *  => nil
 */

VALUE
subWindowBackgroundWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      w->bg = subColorPixel(value, Qnil, Qnil, NULL);

      XSetWindowBackground(display, w->win, w->bg);
    }

  return Qnil;
} /* }}} */

/* subWindowBorderColorWriter {{{ */
/*
 * call-seq: border=(string) -> nil
 *           border=(array)  -> nil
 *           border=(hash)   -> nil
 *           border=(fixnum) -> nil
 *           border=(color)  -> nil
 *
 * Set the border color of this Window which can be of
 * following types:
 *
 * [String] Any color representation of Xlib is allowed
 * [Array]  Must be an array with values for red, green and blue
 * [Hash]   Must be a hash with values for red, green and blue
 * [Fixnum] Pixel representation of a color in Xlib
 * [Color]  Copy color from a Color object
 *
 *  win.border_color = "#000000"
 *  => nil
 */

VALUE
subWindowBorderColorWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XSetWindowBorder(display, w->win,
        subColorPixel(value, Qnil, Qnil, NULL));
      XFlush(display);
    }

  return Qnil;
} /* }}} */

/* subWindowBorderSizeWriter {{{ */
/*
 * call-seq: border_size=(fixnum) -> nil
 *
 * Set border size of this Window.
 *
 *  win.border_size = 3
 *  => nil
 */

VALUE
subWindowBorderSizeWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

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
      else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  return Qnil;
} /* }}} */

/* subWindowGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get the Geometry of this Window.
 *
 *  win.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subWindowGeometryReader(VALUE self)
{
  VALUE geom = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@geometry", geom);

  return geom;
} /* }}} */

/* subWindowGeometryWriter {{{ */
/*
 * call-seq: geometry=(value)    -> nil
 *           geometry=(array)    -> nil
 *           geometry=(hash)     -> nil
 *           geometry=(geometry) -> nil
 *
 * Set the geometry of this Window which can be of following
 * types:
 *
 * [Array]    Must be an array with values for x, y, width and height
 * [Hash]     Must be a hash with values for x, y, width and height
 * [Geometry] Copy geometry from a Geometry object
 *
 *  win.geometry = { :x => 0, :y => 0, :width => 50, :height => 50 }
 *  => nil
 */

VALUE
subWindowGeometryWriter(VALUE self,
  VALUE value)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XRectangle r = { 0 };
      VALUE geom = Qnil;

      /* Create geometry */
      geom = subGeometryInstantiate(0, 0, 1, 1);
      geom = subGeometryInit(1, &value, geom);

      rb_iv_set(self, "@geometry", geom);
      subGeometryToRect(geom, &r);
      XMoveResizeWindow(display, w->win, r.x, r.y, r.width, r.height);
    }

  return Qnil;
} /* }}} */

/* subWindowWrite {{{ */
/*
 * call-seq: write(x, y, string) -> nil
 *
 * Write a string onto this Window at given x/y coordinates, that
 * <b>won't</b> be visible unless the Window content is updated
 * with #redraw.
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

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      /* Check object type */
      if(T_FIXNUM == rb_type(x) && T_FIXNUM == rb_type(y) &&
          T_STRING == rb_type(text))
        {
          int i, xi = FIX2INT(x), yi = FIX2INT(y);
          SubtlextWindowText *wt = NULL;

          /* Find text at x/y position and re-use it */
          for(i = 0; i < w->ntext; i++)
            {
              if(w->text[i].x == xi && w->text[i].y == yi)
                {
                  wt = &w->text[i];
                  break;
                }
            }

          /* Create new text item */
          if(!wt)
            {
              w->text = (SubtlextWindowText *)subSharedMemoryRealloc(w->text,
                (w->ntext + 1) * sizeof(SubtlextWindowText));

              w->text[w->ntext].text = subSharedTextNew();

              wt    = &w->text[w->ntext];
              wt->x = xi;
              wt->y = yi;

              w->ntext++;
            }

          len = subSharedTextParse(display, w->font, wt->text,
            RSTRING_PTR(text));
        }
      else rb_raise(rb_eArgError, "Unknown value-types");
    }

  return INT2FIX(len);
} /* }}} */

/* subWindowRead {{{ */
/*
 * call-seq: read(x, y, width) -> String
 *
 * Read input from given x/y coordinates and width.
 *
 *  string = read(10, 10, 10)
 *  => "subtle"
 */

VALUE
subWindowRead(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE ret = Qnil;
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XEvent ev;
      int pos = 0, len = 0, loop = True, start = 0;
      int guess = -1, state = 0, window = 10;
      char buf[32] = { 0 }, text[1024] = { 0 }, last[32] = { 0 }, *textwin = NULL;
      unsigned long *focus = NULL;
      VALUE x = Qnil, y = Qnil, width = Qnil, rargs[5] = { Qnil }, result = Qnil;
      Atom selection = None;
      KeySym sym;

      rb_scan_args(argc, argv, "21", &x, &y, &width);

      /* Check object type */
      if(T_FIXNUM != rb_type(x) || T_FIXNUM != rb_type(y))
        {
          rb_raise(rb_eArgError, "Unknown value types");

          return Qnil;
        }

      if(FIXNUM_P(width)) window = FIX2INT(width);

      /* Grab and set focus */
      XGrabKeyboard(display, DefaultRootWindow(display), True,
        GrabModeAsync, GrabModeAsync, CurrentTime);
      XMapRaised(display, w->win);
      XSelectInput(display, w->win, KeyPressMask|ButtonPressMask);
      XSetInputFocus(display, w->win, RevertToPointerRoot, CurrentTime);
      selection = XInternAtom(display, "SUBTLEXT_SELECTION", False);
      XFlush(display);

      while(loop)
        {
          text[len] = '_';
          textwin   = text + (len > window ? len - window : 0); ///< Apply text window size

          WindowExpose(w);
          subSharedTextDraw(display, DefaultGC(display, 0), w->font,
            w->win, FIX2INT(x), FIX2INT(y), w->fg, w->bg,
            textwin, strlen(textwin));

          XFlush(display);
          XNextEvent(display, &ev);

          switch(ev.type)
            {
              case ButtonPress: /* {{{ */
                if(Button2 == ev.xbutton.button)
                  {
                    /* Check if there is a selection owner */
                    if(None != XGetSelectionOwner(display, XA_PRIMARY))
                      {
                        XConvertSelection(display, XA_PRIMARY, XA_STRING,
                          selection, w->win, CurrentTime); ///< Convert to atom

                        XFlush(display);
                      }
                  }
                break; /* }}} */
              case SelectionNotify: /* {{{ */
                  {
                    Window selowner = None;

                    /* Get selection owner */
                    if(None != (selowner = XGetSelectionOwner(display,
                        XA_PRIMARY)))
                      {
                        unsigned long size = 0;
                        char *data = NULL;

                        /* Get selection data */
                        if((data = subSharedPropertyGet(display,
                              ev.xselection.requestor, XA_STRING,
                              selection, &size)))
                          {
                            strncpy(text + len, data, sizeof(text) - len);
                            len += size;

                            XFree(data);
                          }
                      }
                  }
                break; /* }}} */
              case KeyPress: /* {{{ */
                pos = XLookupString(&ev.xkey, buf, sizeof(buf), &sym, NULL);

                switch(sym)
                  {
                    case XK_Return:
                    case XK_KP_Enter: /* {{{ */
                      loop   = False;
                      text[len] = 0; ///< Remove underscore
                      ret       = rb_str_new2(text);
                      break; /* }}} */
                    case XK_Escape: /* {{{ */
                      loop = False;
                      break; /* }}} */
                    case XK_BackSpace: /* {{{ */
                      if(0 < len) text[len--] = 0;
                      text[len] = 0;
                      break; /* }}} */
                    case XK_Tab: /* {{{ */
                      if(w->flags & WINDOW_COMPLETION_FUNC)
                        {
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
                          rargs[0] = w->instance;
                          rargs[1] = rb_intern("__completion");
                          rargs[2] = 2;
                          rargs[3] = rb_str_new2(last);
                          rargs[4] = INT2FIX(guess);

                          /* Carefully call completion proc */
                          result = rb_protect(WindowCall, (VALUE)&rargs, &state);
                          if(state)
                            {
                              subSubtlextBacktrace();

                              continue;
                            }

                          /* Check type */
                          if(T_STRING == rb_type(result))
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
                break; /* }}} */
              default: break;
            }

          /* Call input proc */
          if(loop && (SelectionNotify == ev.type || KeyPress == ev.type) &&
              w->flags & WINDOW_INPUT_FUNC)
            {
              /* Wrap up data */
              rargs[0] = w->instance;
              rargs[1] = rb_intern("__input");
              rargs[2] = 1;
              rargs[3] = rb_str_new2(text);

              /* Carefully call completion proc */
              rb_protect(WindowCall, (VALUE)&rargs, &state);
              if(state) subSubtlextBacktrace();
            }
        }

      XSelectInput(display, w->win, NoEventMask);
      XUngrabKeyboard(display, CurrentTime);

      /* Restore logical focus */
      if((focus = (unsigned long *)subSharedPropertyGet(display,
          DefaultRootWindow(display), XA_WINDOW,
          XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
        {
          XSetInputFocus(display, *focus, RevertToPointerRoot, CurrentTime);

          free(focus);
        }
    }

  return ret;
} /* }}} */

/* subWindowGrabKeys {{{ */
/*
 * call-seq: grab_keys(&block) -> nil
 *
 * Grab key events and pass them to the block.
 *
 *  grab_keys do |key|
 *    case key
 *      when :return then p "return"
 *    end
 *  end
 *  => nil
 */

VALUE
subWindowGrabKeys(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  rb_need_block();

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && rb_block_given_p())
    {
      XEvent ev;
      int loop = True, state = 0;
      char buf[32] = { 0 };
      unsigned long *focus = NULL;
      VALUE p = rb_block_proc(), result = Qnil, rargs[4] = { Qnil }, sym = Qnil;
      KeySym keysym;

      /* Grab and set focus */
      XGrabKeyboard(display, DefaultRootWindow(display), True,
        GrabModeAsync, GrabModeAsync, CurrentTime);
      XMapRaised(display, w->win);
      XSetInputFocus(display, w->win, RevertToPointerRoot, CurrentTime);
      XSelectInput(display, w->win, KeyPressMask);
      XFlush(display);

      while(loop)
        {
          XMaskEvent(display, KeyPressMask, &ev);
          switch(ev.type)
            {
              case KeyPress: /* {{{ */
                XLookupString(&ev.xkey, buf, sizeof(buf), &keysym, NULL);

                /* Skip modifier keys */
                if(IsModifierKey(keysym)) continue;

                /* Translate syms to something meaningful */
                switch(keysym)
                  {
                    /* Arrow keys */
                    case XK_KP_Left:
                    case XK_Left:      sym = CHAR2SYM("left");      break;
                    case XK_KP_Right:
                    case XK_Right:     sym = CHAR2SYM("right");     break;
                    case XK_KP_Up:
                    case XK_Up:        sym = CHAR2SYM("up");        break;
                    case XK_KP_Down:
                    case XK_Down:      sym = CHAR2SYM("down");      break;

                    /* Input */
                    case XK_KP_Enter:
                    case XK_Return:    sym = CHAR2SYM("return");    break;
                    case XK_Escape:    sym = CHAR2SYM("escape");    break;
                    case XK_BackSpace: sym = CHAR2SYM("backspace"); break;
                    case XK_Tab:       sym = CHAR2SYM("tab");       break;
                    case XK_space:     sym = CHAR2SYM("space");     break;
                    default:           sym = CHAR2SYM(buf);
                  }

                /* Wrap up data */
                rargs[0] = p;
                rargs[1] = rb_intern("call");
                rargs[2] = 1;
                rargs[3] = sym;

                /* Carefully call listen proc */
                result = rb_protect(WindowCall, (VALUE)&rargs, &state);
                if(state) subSubtlextBacktrace();

                /* End event loop? */
                if(Qtrue != result || state) loop = False;
                break; /* }}} */
              default: break;
            }
        }

      XSelectInput(display, w->win, NoEventMask);
      XUngrabKeyboard(display, CurrentTime);

      /* Restore logical focus */
      if((focus = (unsigned long *)subSharedPropertyGet(display,
          DefaultRootWindow(display), XA_WINDOW,
          XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
        {
          XSetInputFocus(display, *focus, RevertToPointerRoot, CurrentTime);

          free(focus);
        }
    }

  return Qnil;
} /* }}} */

/* subWindowGrabMouse {{{ */
/*
 * call-seq: grab_mouse(&block) -> nil
 *
 * Grab mouse events and pass them to the block.
 *
 *  grab_mouse do |x, y, button|
 *    p "x=#{x}, y=#{y}, button=#{button}"
 *  end
 *  => nil
 */

VALUE
subWindowGrabMouse(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  rb_need_block();
  Data_Get_Struct(self, SubtlextWindow, w);
  if(w && rb_block_given_p())
    {
      rb_raise(rb_eNotImpError, "Not implemented yet");
    }

  return Qnil;
} /* }}} */

/* subWindowClear {{{ */
/*
 * call-seq: clear -> nil
 *
 * Clear this Window and remove all stored text.
 *
 *  win.clear
 *  => nil
 */

VALUE
subWindowClear(int argc,
  VALUE *argv,
  VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      VALUE x = Qnil, y = Qnil, width = Qnil, height = Qnil;

      rb_scan_args(argc, argv, "04", &x, &y, &width, &height);

      /* Either clear area or whole window */
      if(FIXNUM_P(x) && FIXNUM_P(y) && FIXNUM_P(width) && FIXNUM_P(height))
        {
          XClearArea(display, w->win, FIX2INT(x), FIX2INT(y),
            FIX2INT(width), FIX2INT(height), False);
        }
      else XClearWindow(display, w->win);
    }

  return Qnil;
} /* }}} */

/* subWindowRedraw {{{ */
/*
 * call-seq: redraw -> nil
 *
 * Redraw Window content.
 *
 *  win.redraw
 *  => nil
 */

VALUE
subWindowRedraw(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w) WindowExpose(w);

  return Qnil;
} /* }}} */

/* subWindowCompletion {{{ */
/*
 * call-seq: completion(&block) -> nil
 *
 * Add completion block to this Window, that is called whenever the
 * tab key is pressed.
 *
 *  win.completion do |str, guess|
 *    str
 *  end
 */

VALUE
subWindowCompletion(VALUE self)
{
  return WindowDefine(self, 2, WINDOW_COMPLETION_FUNC,
    CHAR2SYM("__completion"));
} /* }}} */

/* subWindowInput {{{ */
/*
 * call-seq: input(&block) -> nil
 *
 * Add input block that to this Window, that is called whenever the user
 * makes any input in a #read call.
 *
 *  win.input do |str|
 *    str
 *  end
 */

VALUE
subWindowInput(VALUE self)
{
  return WindowDefine(self, 1, WINDOW_INPUT_FUNC, CHAR2SYM("__input"));
} /* }}} */

/* subWindowRaise {{{ */
/*
 * call-seq: raise -> nil
 *
 * Raise this Window to the top of the window stack, when the window manager
 * supports that. (subtle does)
 *
 *  win.raise
 *  => nil
 */

VALUE
subWindowRaise(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XRaiseWindow(display, w->win);
      WindowExpose(w);
    }

  return Qnil;
} /* }}} */

/* subWindowLower {{{ */
/*
 * call-seq: lower -> nil
 *
 * Lower this Window to the bottom of the window stack, when the window manager
 * supports that. (subtle does)
 *
 *  win.lower
 *  => nil
 */

VALUE
subWindowLower(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XLowerWindow(display, w->win);
      WindowExpose(w);
    }

  return Qnil;
} /* }}} */

/* subWindowShow {{{ */
/*
 * call-seq: show() -> nil
 *
 * Show this Window on screen.
 *
 *  win.show
 *  => nil
 */

VALUE
subWindowShow(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      rb_iv_set(self, "@hidden", Qfalse);

      XMapRaised(display, w->win);
      WindowExpose(w);
    }

  return Qnil;
} /* }}} */

/* subWindowHide {{{ */
/*
 * call-seq: hide() -> nil
 *
 * Hide this Window from screen.
 *
 *  win.hide
 *  => nil
 */

VALUE
subWindowHide(VALUE self)
{
  VALUE win = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  if(RTEST(win))
    {
      rb_iv_set(self, "@hidden", Qtrue);

      XUnmapWindow(display, NUM2LONG(win));
      XSync(display, False); ///< Sync with X
    }

  return Qnil;
} /* }}} */

/* subWindowAskHidden {{{ */
/*
 * call-seq: hidden -> true or false
 *
 * Whether Window is hidden.
 *
 *  win.hidden?
 *  => true
 */

VALUE
subWindowAskHidden(VALUE self)
{
  VALUE hidden  = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@hidden", hidden);

  return hidden;
} /* }}} */

/* subWindowKill {{{ */
/*
 * call-seq: kill() -> nil
 *
 * Destroy this Window and <b>freeze</b> this object.
 *
 *  win.kill
 *  => nil
 */

VALUE
subWindowKill(VALUE self)
{
  SubtlextWindow *w = NULL;

  /* Check ruby object */
  rb_check_frozen(self);

  Data_Get_Struct(self, SubtlextWindow, w);
  if(w)
    {
      XUnmapWindow(display, w->win);

      rb_obj_freeze(self); ///< Freeze object
    }

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
