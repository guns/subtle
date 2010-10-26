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

/* SubtleSend {{{ */
static VALUE
SubtleSend(char *message)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  subSubtlextConnect(); ///< Implicit open connection

  subSharedMessage(display, DefaultRootWindow(display), message, data, True);

  return Qnil;
} /* }}} */

/* subSubtleDisplayReader {{{ */
/*
 * call-seq: display -> String
 *
 * Get the display name
 *
 *  subtle.display
 *  => ":0"
 */

VALUE
subSubtleDisplayReader(VALUE self)
{
  subSubtlextConnect(); ///< Implicit open connection

  return rb_str_new2(DisplayString(display));
} /* }}} */

/* subSubtleRunningAsk {{{ */
/*
 * call-seq: running? -> true or false
 *
 * Check if subtle is running
 *
 *  subtle.running?
 *  => true
 *
 *  subtle.running?
 *  => false
 */

VALUE
subSubtleRunningAsk(VALUE self)
{
  char *prop = NULL;
  Window *check = NULL;
  VALUE running = Qfalse;

  subSubtlextConnect(); ///< Implicit open connection

  /* Get supporting window */
  if(display && (check = subSubtlextWMCheck()))
    {
      subSharedLogDebug("Support: win=%#lx\n", *check);

      /* Get property */
      if((prop = subSharedPropertyGet(display, *check, XInternAtom(display, "UTF8_STRING", False),
        XInternAtom(display, "_NET_WM_NAME", False), NULL)))
        {
          if(!strncmp(prop, PKG_NAME, strlen(prop))) running = Qtrue;
          subSharedLogDebug("Running: wmname=%s\n", prop);

          free(prop);
        }

      free(check);
    }

  return running;
} /* }}} */

/* subSubtleSelect {{{ */
/*
 * call-seq: select_window -> String
 *
 * Select window and get name
 *
 *  select_window
 *  => "urxvt"
 */

VALUE
subSubtleSelect(VALUE self)
{
  int i, format = 0, buttons = 0;
  unsigned int n;
  unsigned long nitems = 0, bytes = 0;
  unsigned char *data = NULL;
  XEvent event;
  Window win = None;
  Atom type = None, rtype = None;
  Window dummy = None, root = None, *wins = NULL;
  Cursor cursor = None;

  subSubtlextConnect(); ///< Implicit open connection

  root   = DefaultRootWindow(display);
  cursor = XCreateFontCursor(display, XC_cross);
  type   = XInternAtom(display, "WM_STATE", True);

  /* Grab pointer */
  if(XGrabPointer(display, root, False, ButtonPressMask|ButtonReleaseMask,
      GrabModeSync, GrabModeAsync, root, cursor, CurrentTime))
    {
      XFreeCursor(display, cursor);

      return Qnil;
    }

  /* Select a window */
  while(None == win || 0 != buttons)
    {
      XAllowEvents(display, SyncPointer, CurrentTime);
      XWindowEvent(display, root, ButtonPressMask|ButtonReleaseMask, &event);

      switch(event.type)
        {
          case ButtonPress:
            if(None == win)
              win = event.xbutton.subwindow ? event.xbutton.subwindow : root; ///< Sanitize
            buttons++;
            break;
          case ButtonRelease:
            if(0 < buttons) buttons--;
            break;
        }
      }

  /* Find children with WM_STATE atom */
  XQueryTree(display, win, &dummy, &dummy, &wins, &n);

  for(i = 0; i < n; i++)
    if(Success == XGetWindowProperty(display, wins[i], type, 0, 0, False,
        AnyPropertyType, &rtype, &format, &nitems, &bytes, &data))
      {
        if(data)
          {
            XFree(data);
            data = NULL;
          }

        if(type == rtype)
          {
            win = wins[i];

            break;
          }
      }

  XFree(wins);
  XFreeCursor(display, cursor);
  XUngrabPointer(display, CurrentTime);

  XSync(display, False); ///< Sync all changes

  return None != win ? LONG2NUM(win) : Qnil;
} /* }}} */

/* subSubtleReload {{{ */
/*
 * call-seq: reload -> nil
 *
 * Force Subtle to reload config and sublets
 *
 *  subtle.reload
 *  => nil
 */

VALUE
subSubtleReload(VALUE self)
{
  return SubtleSend("SUBTLE_RELOAD");
} /* }}} */

/* subSubtleRestart {{{ */
/*
 * call-seq: restart -> nil
 *
 * Force Subtle to restart
 *
 *  subtle.restart
 *  => nil
 */

VALUE
subSubtleRestart(VALUE self)
{
  return SubtleSend("SUBTLE_RESTART");
} /* }}} */

/* subSubtleQuit {{{ */
/*
 * call-seq: quit -> nil
 *
 * Force Subtle to exit
 *
 *  subtle.reload
 *  => nil
 */

VALUE
subSubtleQuit(VALUE self)
{
  return SubtleSend("SUBTLE_QUIT");
} /* }}} */

/* subSubtleColors {{{ */
/*
 * call-seq: colors -> Hash
 *
 * Get array of all Colors
 *
 *  Subtlext::Subtle.colors
 *  => { :fg_panel => #<Subtlext::Color:xxx> }
 */

VALUE
subSubtleColors(VALUE self)
{
  long unsigned int i, size = 0;
  unsigned long *colors = NULL;
  VALUE meth = Qnil, klass = Qnil, hash = Qnil;
  const char *names[] = {
    "fg_panel", "fg_views", "fg_sublets", "fg_focus", "fg_urgent",
    "bg_panel", "bg_views", "bg_sublets", "bg_focus", "bg_urgent",
    "border_focus", "border_normal", "border_panel",
    "background", "separator"
  };

  subSubtlextConnect(); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Color"));
  colors = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "SUBTLE_COLORS", False), &size);
  hash = rb_hash_new();

  if(colors)
    {
      for(i = 0; i < size; i++)
        {
          VALUE c = rb_funcall(klass, meth, 1, LONG2NUM(colors[i]));

          rb_hash_aset(hash, CHAR2SYM(names[i]), c);
        }

      free(colors);
    }

  return hash;
} /* }}} */

/* subSubtleSpawn {{{ */
/*
 * call-seq: spawn(cmd) -> Fixnum
 *
 * Spawn a command and return the pid
 *
 *  spawn("xterm")
 *  => 123
 */

VALUE
subSubtleSpawn(VALUE self,
  VALUE cmd)
{
  VALUE ret = Qnil;

  if(T_STRING == rb_type(cmd))
    {
      ret = INT2FIX((int)subSharedSpawn(RSTRING_PTR(cmd)));
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return ret;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
