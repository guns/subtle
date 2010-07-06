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
  VALUE ret = Qnil;
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
          case ButtonRelease: if(0 < buttons) buttons--; break;
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
            char buf[20] = { 0 };

            snprintf(buf, sizeof(buf), "%#lx", wins[i]);

            ret = rb_str_new2(buf);
            break;
          }
      }

  XFree(wins);
  XFreeCursor(display, cursor);
  XUngrabPointer(display, CurrentTime);

  return ret;
} /* }}} */

/* subSubtleReloadConfig {{{ */
/*
 * call-seq: reload_config -> nil
 *
 * Force Subtle to reload the config
 *
 *  subtle.reload_config
 *  => nil
 */

VALUE
subSubtleReloadConfig(VALUE self)
{
  return SubtleSend("SUBTLE_RELOAD_CONFIG");
} /* }}} */

/* subSubtleReloadSublets {{{ */
/*
 * call-seq: reload_sublets -> nil
 *
 * Force Subtle to reload the sublets
 *
 *  subtle.reload_sublets
 *  => nil
 */

VALUE
subSubtleReloadSublets(VALUE self)
{
  return SubtleSend("SUBTLE_RELOAD_SUBLETS");
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

// vim:ts=2:bs=2:sw=2:et:fdm=marker
