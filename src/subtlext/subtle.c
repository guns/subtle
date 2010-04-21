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

  subSharedMessage(DefaultRootWindow(display), message, data, True);

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
  subSubtlextConnect(); ///< Implicit open connection

  return subSharedSubtleRunning() ? Qtrue : Qfalse;
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

/* subSubtleFocus {{{ */
/*
 * call-seq: focus(name) -> Subtlext::Client or nil
 *
 * Find Client by given name and set focus
 *
 *  subtle.focus("subtle")
 *  => #<Subtlext::Client:xxx>
 *
 *  subtle.focus("subtle")
 *  => nil
 */

VALUE
subSubtleFocus(VALUE self,
  VALUE name)
{
  int id = -1;
  Window win = 0;
  VALUE client = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  if(RTEST(name) && -1 != ((id = subSharedClientFind(RSTRING_PTR(name),
      NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, True);

      client = subClientInstantiate(win);
    }
  else rb_raise(rb_eStandardError, "Failed setting focus");

  return client;
} /* }}} */

/* subSubtleClientDel {{{ */
/*
 * call-seq: del_client(name) -> nil
 *
 * Delete Client by given name or Client object
 *
 *  subtle.del_client("subtle")
 *  => nil
 *
 *  subtle.del_client(subtle.find_client("subtle"))
 *  => nil
 */

VALUE
subSubtleClientDel(VALUE self,
  VALUE name)
{
  int id = -1;
  Window win = 0;

  subSubtlextConnect(); ///< Implicit open connection

  if(RTEST(name) && -1 != ((id = subSharedClientFind(RSTRING_PTR(name),
      NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Send data */
      data.l[0] = CurrentTime;
      data.l[1] = 2; ///< Claim to be a pager

      subSharedMessage(win, "_NET_CLOSE_WINDOW", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed finding client");

  return Qnil;
} /* }}} */

/* subSubtleGravityDel {{{ */
/*
 * call-seq: del_gravity(name) -> nil
 *
 * Delete Gravity by given name or Gravity object
 *
 *  subtle.del_gravity("subtle")
 *  => nil
 *
 *  subtle.del_gravity(subtle.find_gravity("subtle"))
 *  => nil
 */

VALUE
subSubtleGravityDel(VALUE self,
  VALUE value)
{
  return subSubtlextKill(value, SUB_TYPE_GRAVITY);
} /* }}} */

/* subSubtleTagAdd {{{ */
/*
 * call-seq: add_tag(name) -> Subtlext::Tag
 *
 * Add Tag with given name or Tag object
 *
 *  subtle.add_tag("subtle")
 *  => #<Subtlext::Tag:xxx>
 *
 *  subtle.add_tag(Tag.new("subtle"))
 *  => #<Subtlext::Tag:xxx>
 */

VALUE
subSubtleTagAdd(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  if(NIL_P((tag = subSubtlextFind(SUB_TYPE_TAG, value, False))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(value));
      subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, True);

      tag = subTagInstantiate(RSTRING_PTR(value));
    }

  return tag;
} /* }}} */

/* subSubtleTagDel {{{ */
/*
 * call-seq: del_tag(name) -> nil
 *
 * Delete Tag by given name or Tag object
 *
 *  subtle.del_tag("subtle")
 *  => nil
 *
 *  subtle.del_tag(subtle.find_tag("subtle"))
 *  => nil
 */

VALUE
subSubtleTagDel(VALUE self,
  VALUE value)
{
  return subSubtlextKill(value, SUB_TYPE_TAG);
} /* }}} */

/* subSubtleTrayDel {{{ */
/*
 * call-seq: del_tray(name) -> nil
 *
 * Delete Tray by given name or Tray object
 *
 *  subtle.del_sublet("subtle")
 *  => nil
 */

VALUE
subSubtleTrayDel(VALUE self,
  VALUE value)
{
  return subSubtlextKill(value, SUB_TYPE_TRAY);
} /* }}} */

/* subSubtleSubletDel {{{ */
/*
 * call-seq: del_sublet(name) -> nil
 *
 * Delete Sublet by given name or Sublet object
 *
 *  subtle.del_sublet("subtle")
 *  => nil
 *
 *  subtle.del_sublet(subtle.find_sublet("subtle"))
 *  => nil
 */

VALUE
subSubtleSubletDel(VALUE self,
  VALUE value)
{
  return subSubtlextKill(value, SUB_TYPE_SUBLET);
} /* }}} */

/* subSubtleViewAdd {{{ */
/*
 * call-seq: add_view(name) -> Subtlext::View
 *
 * Add View with given name or View object
 *
 *  subtle.add_view("subtle")
 *  => #<Subtlext::View:xxx>
 *
 *  subtle.add_view(View.new("subtle"))
 *  => #<Subtlext::View:xxx>
 */

VALUE
subSubtleViewAdd(VALUE self,
  VALUE value)
{
  VALUE view = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  if(NIL_P((view = subSubtlextFind(SUB_TYPE_VIEW, value, False))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(value));
      subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data, False);

      view = subViewInstantiate(RSTRING_PTR(value));
    }

  return view;
} /* }}} */

/* subSubtleViewDel {{{ */
/*
 * call-seq: del_view(name) -> nil
 *
 * Delete View by given name or View object
 *
 *  subtle.del_view("subtle")
 *  => nil
 *
 *  subtle.del_view(subtle.find_view("subtle"))
 *  => nil
 */

VALUE
subSubtleViewDel(VALUE self,
  VALUE value)
{
  return subSubtlextKill(value, SUB_TYPE_VIEW);
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
