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

  subSubtlextConnect(NULL); ///< Implicit open connection

  subSharedMessage(display, DefaultRootWindow(display),
    message, data, 32, True);

  return Qnil;
} /* }}} */

/* Singleton */

/* subSubtleSingDisplayReader {{{ */
/*
 * call-seq: display -> String
 *
 * Get the display name
 *
 *  subtle.display
 *  => ":0"
 */

VALUE
subSubtleSingDisplayReader(VALUE self)
{
  subSubtlextConnect(NULL); ///< Implicit open connection

  return rb_str_new2(DisplayString(display));
} /* }}} */

/* subSubtleSingDisplayWriter {{{ */
/*
 * call-seq: display=(string) -> nil
 *
 * Set the display name
 *
 *  subtle.display = ":0"
 *  => nil
 */

VALUE
subSubtleSingDisplayWriter(VALUE self,
  VALUE display_string)
{
  /* Explicit open connection */
  subSubtlextConnect(T_STRING == rb_type(display_string) ?
    RSTRING_PTR(display_string) : NULL);

  return Qnil;
} /* }}} */

/* subSubtleSingRunningAsk {{{ */
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
subSubtleSingRunningAsk(VALUE self)
{
  char *prop = NULL;
  Window *check = NULL;
  VALUE running = Qfalse;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get supporting window */
  if(display && (check = subSubtlextWMCheck()))
    {
      subSharedLogDebug("Support: win=%#lx\n", *check);

      /* Get property */
      if((prop = subSharedPropertyGet(display, *check, XInternAtom(display,
          "UTF8_STRING", False), XInternAtom(display, "_NET_WM_NAME", False),
          NULL)))
        {
          if(!strncmp(prop, PKG_NAME, strlen(prop))) running = Qtrue;
          subSharedLogDebug("Running: wmname=%s\n", prop);

          free(prop);
        }

      free(check);
    }

  return running;
} /* }}} */

/* subSubtleSingSelect {{{ */
/*
 * call-seq: select_window -> String
 *
 * Select window and get name
 *
 *  select_window
 *  => "urxvt"
 */

VALUE
subSubtleSingSelect(VALUE self)
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

  subSubtlextConnect(NULL); ///< Implicit open connection

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
    {
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
    }

  if(wins) XFree(wins);
  XFreeCursor(display, cursor);
  XUngrabPointer(display, CurrentTime);

  XSync(display, False); ///< Sync all changes

  return None != win ? LONG2NUM(win) : Qnil;
} /* }}} */

/* subSubtleSingReload {{{ */
/*
 * call-seq: reload -> nil
 *
 * Force Subtle to reload config and sublets
 *
 *  subtle.reload
 *  => nil
 */

VALUE
subSubtleSingReload(VALUE self)
{
  return SubtleSend("SUBTLE_RELOAD");
} /* }}} */

/* subSubtleSingRestart {{{ */
/*
 * call-seq: restart -> nil
 *
 * Force Subtle to restart
 *
 *  subtle.restart
 *  => nil
 */

VALUE
subSubtleSingRestart(VALUE self)
{
  return SubtleSend("SUBTLE_RESTART");
} /* }}} */

/* subSubtleSingQuit {{{ */
/*
 * call-seq: quit -> nil
 *
 * Force Subtle to exit
 *
 *  subtle.reload
 *  => nil
 */

VALUE
subSubtleSingQuit(VALUE self)
{
  return SubtleSend("SUBTLE_QUIT");
} /* }}} */

/* subSubtleSingColors {{{ */
/*
 * call-seq: colors -> Hash
 *
 * Get array of all Colors
 *
 *  Subtlext::Subtle.colors
 *  => { :fg_panel => #<Subtlext::Color:xxx> }
 */

VALUE
subSubtleSingColors(VALUE self)
{
  int i;
  unsigned long ncolors = 0, *colors = NULL;
  VALUE meth = Qnil, klass = Qnil, hash = Qnil;
  const char *names[] = {
    "focus_fg",    "focus_bg",    "focus_border",
    "urgent_fg",   "urgent_bg",   "urgent_border",
    "occupied_fg", "occupied_bg", "occupied_border",
    "views_fg",    "views_bg",    "views_border",
    "sublets_fg",  "sublets_bg",  "sublets_border",
    "panel_fg",    "panel_bg",
    "client_active", "client_inactive",
    "background", "separator"
  };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Color"));
  hash  = rb_hash_new();

  /* Check result */
  if((colors = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_CARDINAL,
      XInternAtom(display, "SUBTLE_COLORS", False), &ncolors)))
    {
      for(i = 0; i < ncolors; i++)
        {
          VALUE c = rb_funcall(klass, meth, 1, LONG2NUM(colors[i]));

          rb_hash_aset(hash, CHAR2SYM(names[i]), c);
        }

      free(colors);
    }

  return hash;
} /* }}} */

/* subSubtleSingFont {{{ */
/*
 * call-seq: Font -> String or nil
 *
 * Get font used in subtle
 *
 *  Subtlext::Subtle.font
 *  => "-*-*-medium-*-*-*-14-*-*-*-*-*-*-*"
 */

VALUE
subSubtleSingFont(VALUE self)
{
  char *prop = NULL;
  VALUE font = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get results */
  if((prop = subSharedPropertyGet(display, DefaultRootWindow(display),
      XInternAtom(display, "UTF8_STRING", False),
      XInternAtom(display, "SUBTLE_FONT", False),
      NULL)))
    {
      font = rb_str_new2(prop);

      free(prop);
    }

  return font;
} /* }}} */

/* subSubtleSingSpawn {{{ */
/*
 * call-seq: spawn(cmd) -> Fixnum
 *
 * Spawn a command and return the pid
 *
 *  spawn("xterm")
 *  => 123
 */

VALUE
subSubtleSingSpawn(VALUE self,
  VALUE cmd)
{
  VALUE ret = Qnil;

  /* Check object type */
  if(T_STRING == rb_type(cmd))
    {
      pid_t pid = 0;

      subSubtlextConnect(NULL); ///< Implicit open connection

      /* Create client */
      if(0 < (pid = subSharedSpawn(RSTRING_PTR(cmd))))
        {
          int nclients = 0;
          Window *clients = NULL;

          clients = subSubtlextList("_NET_CLIENT_LIST", &nclients);

          /* Create client */
          ret = subClientInstantiate(nclients);
          rb_iv_set(ret, "@pid", INT2FIX((int)pid));

          if(clients) free(clients);
        }
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(cmd));

  return ret;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
