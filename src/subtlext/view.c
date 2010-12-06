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

/* ViewFind {{{ */
int
ViewFind(char *match,
  char **name)
{
  int ret = -1, i, nnames = 0;
  long digit = -1;
  char **names = NULL;
  regex_t *preg = NULL;

  assert(match);

  /* Fetch data */
  names = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  preg  = subSharedRegexNew(match);

  /* Check results */
  if(names && preg)
    {
      if(isdigit(match[0])) digit = strtol(match, NULL, 0);

      for(i = 0; i < nnames; i++)
        {
          /* Find view either by name or by window id */
          if(digit == (long)i || subSharedRegexMatch(preg, names[i]))
            {
              subSharedLogDebug("Found: type=view, match=%s, name=%s, id=%d\n",
                match, names[i], i);

              if(name) *name = strdup(names[i]);

              ret = i;
              break;
            }
        }
    }
  else subSharedLogDebug("Failed finding view `%s'\n", match);

  if(preg)  subSharedRegexKill(preg);
  if(names) XFreeStringList(names);

  return ret;
} /* }}} */

/* ViewSelect {{{ */
static VALUE
ViewSelect(VALUE self,
  int dir)
{
  int nnames = 0;
  char **names = NULL;
  VALUE id = Qnil, ret = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  if((names = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames)))
    {
      int vid = FIX2INT(id);

      /* Select view */
      if(SUB_VIEW_NEXT == dir && vid < (nnames - 1))
        {
          vid++;
        }
      else if(SUB_VIEW_PREV == dir && 0 < vid)
        {
          vid--;
        }

      /* Create view */
      ret = subViewInstantiate(names[vid]);
      subViewUpdate(ret);

      XFreeStringList(names);
    }

  return ret;
} /* }}} */

/* Singleton */

/* subViewSingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Client or nil
 *           [value]     -> Subtlext::Client or nil
 *
 * Find View by a given value which can be of following type:
 *
 * [fixnum] Array id
 * [string] Match against name of View
 * [symbol] Either :current for current View or :all for an array
 *
 *  Subtlext::View.find("subtle")
 *  => #<Subtlext::View:xxx>
 *
 *  Subtlext::View[1]
 *  => #<Subtlext::View:xxx>
 *
 *  Subtlext::View["subtle"]
 *  => nil
 */

VALUE
subViewSingFind(VALUE self,
  VALUE value)
{
  int id = 0;
  VALUE parsed = Qnil, view = Qnil;
  char *name = NULL, buf[50] = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  if(T_SYMBOL == rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), NULL)))
    {
      if(CHAR2SYM("visible") == parsed)
        return subViewSingVisible(Qnil);
      else if(CHAR2SYM("all") == parsed)
        return subViewSingAll(Qnil);
      else if(CHAR2SYM("current") == parsed)
        return subViewSingCurrent(Qnil);
    }

  /* Find view */
  if(-1 != (id = ViewFind(buf, &name)))
    {
      if(!NIL_P((view = subViewInstantiate(name))))
        rb_iv_set(view, "@id",  INT2FIX(id));

      free(name);
    }

  return view;
} /* }}} */

/* subViewSingCurrent {{{ */
/*
 * call-seq: current -> Subtlext::View
 *
 * Get current active View
 *
 *  Subtlext::View.current
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewSingCurrent(VALUE self)
{
  int nnames = 0;
  char **names = NULL;
  unsigned long *cur_view = NULL;
  VALUE view = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  names    = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  cur_view = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

  /* Check results */
  if(names && cur_view)
    {
      /* Create instance */
      view = subViewInstantiate(names[*cur_view]);
      rb_iv_set(view, "@id",  INT2FIX(*cur_view));
    }

  if(names)    XFreeStringList(names);
  if(cur_view) free(cur_view);

  return view;
} /* }}} */

/* subViewSingVisible {{{ */
/*
 * call-seq: visible -> Array
 *
 * Get array of all visible View
 *
 *  Subtlext::View.visible
 *  => [#<Subtlext::View:xxx>, #<Subtlext::View:xxx>]
 *
 *  Subtlext::View.visible
 *  => []
 */

VALUE
subViewSingVisible(VALUE self)
{
  int i, nnames = 0;
  char **names = NULL;
  unsigned long *visible = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, v = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("View"));
  array = rb_ary_new();
  names = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  visible = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VISIBLE_VIEWS", False), NULL);

  /* Check results */
  if(names && visible)
    {
      for(i = 0; i < nnames; i++)
        {
          /* Create view on match */
          if(*visible & (1L << (i + 1)) &&
              !NIL_P(v = rb_funcall(klass, meth, 1, rb_str_new2(names[i]))))
            {
              rb_iv_set(v, "@id", INT2FIX(i));
              rb_ary_push(array, v);
            }
        }

      XFreeStringList(names);
      free(visible);
    }

  return array;
} /* }}} */

/* subViewSingAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get array of all View
 *
 *  Subtlext::View.all
 *  => [#<Subtlext::View:xxx>, #<Subtlext::View:xxx>]
 *
 *  Subtlext::View.all
 *  => []
 */

VALUE
subViewSingAll(VALUE self)
{
  int i, nnames = 0;
  char **names = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, v = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  klass = rb_const_get(mod, rb_intern("View"));
  meth  = rb_intern("new");
  array = rb_ary_new();

  /* Check results */
  if((names = subSharedPropertyStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames)))
    {
      for(i = 0; i < nnames; i++)
        {
          if(!NIL_P(v = rb_funcall(klass, meth, 1, rb_str_new2(names[i]))))
            {
              rb_iv_set(v, "@id",  INT2FIX(i));
              rb_ary_push(array, v);
            }
        }

      XFreeStringList(names);
    }

  return array;
} /* }}} */

/* Class */

/* subViewInstantiate {{{ */
VALUE
subViewInstantiate(char *name)
{
  VALUE klass = Qnil, view = Qnil;

  assert(name);

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("View"));
  view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return view;
} /* }}} */

/* subViewInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::View
 *
 * Create a new View object
 *
 *  view = Subtlext::View.new("subtle")
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'",
      rb_obj_classname(name));

  /* Init object */
  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subViewUpdate {{{ */
/*
 * call-seq: Update -> nil
 *
 * Update View properties
 *
 *  view.update
 *  => nil
 */

VALUE
subViewUpdate(VALUE self)
{
  int id = -1;
  VALUE name = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", name);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Create view if needed */
  if(-1 == (id = ViewFind(RSTRING_PTR(name), NULL)))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_VIEW_NEW", data, 8, True);

      id = ViewFind(RSTRING_PTR(name), NULL);
    }

  /* Guess view id */
  if(-1 == id)
    {
      int nnames = 0;
      char **names = NULL;

      names = subSharedPropertyStrings(display, DefaultRootWindow(display),
        XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);

      id = nnames; ///< New id should be last

      if(names) XFreeStringList(names);
    }

  rb_iv_set(self, "@id", INT2FIX(id));

  return Qnil;
} /* }}} */

/* subViewClients {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get Array of Client on View
 *
 *  view.clients
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  view.clients
 *  => []
 */

VALUE
subViewClients(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  VALUE id = Qnil, klass = Qnil, meth = Qnil, array = Qnil, client = Qnil;
  unsigned long *view_tags = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  klass     = rb_const_get(mod, rb_intern("Client"));
  meth      = rb_intern("new");
  array     = rb_ary_new();
  clients   = subSubtlextList("_NET_CLIENT_LIST", &nclients);
  view_tags = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "SUBTLE_VIEW_TAGS", False), NULL);

  /* Check results */
  if(clients && view_tags)
    {
      for(i = 0; i < nclients; i++)
        {
          unsigned long *client_tags = NULL, *flags = NULL;

          /* Fetch window data */
          client_tags = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);
          flags       = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_FLAGS", False), NULL);

          /* Check if there are common tags or window is stick */
          if((client_tags && view_tags[FIX2INT(id)] & *client_tags) ||
              (flags && *flags & SUB_EWMH_STICK))
            {
              if(!NIL_P(client = rb_funcall(klass, meth, 1, INT2FIX(i))))
                {
                  subClientUpdate(client);
                  rb_ary_push(array, client);
                }
            }

          if(client_tags) free(client_tags);
          if(flags)       free(flags);
        }
    }

  if(clients)   free(clients);
  if(view_tags) free(view_tags);

  return array;
} /* }}} */

/* subViewJump {{{ */
/*
 * call-seq: jump -> nil
 *
 * Jump to this View
 *
 *  view.jump
 *  => nil
 */

VALUE
subViewJump(VALUE self)
{
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = FIX2INT(id);

  subSharedMessage(display, DefaultRootWindow(display),
    "_NET_CURRENT_DESKTOP", data, 32, True);

  return Qnil;
} /* }}} */

/* subViewSelectNext {{{ */
/*
 * call-seq: next -> Subtlext::View or nil
 *
 * Select next View
 *
 *  view.next
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewSelectNext(VALUE self)
{
  return ViewSelect(self, SUB_VIEW_NEXT);
} /* }}} */

/* subViewSelectPrev {{{ */
/*
 * call-seq: prev -> Subtlext::View or nil
 *
 * Select prev View
 *
 *  view.prev
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewSelectPrev(VALUE self)
{
  return ViewSelect(self, SUB_VIEW_PREV);
} /* }}} */

/* subViewCurrentAsk {{{ */
/*
 * call-seq: current? -> true or false
 *
 * Check if this View is the current active View
 *
 *  view.current?
 *  => true
 *
 *  view.current?
 *  => false
 */

VALUE
subViewCurrentAsk(VALUE self)
{
  VALUE id = Qnil, ret = Qfalse;;
  unsigned long *cur_view = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Check results */
  if((cur_view = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_CARDINAL,
      XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL)))
    {
      if(FIX2INT(id) == *cur_view) ret = Qtrue;

      free(cur_view);
    }

  return ret;
} /* }}} */

/* subViewToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert View object to String
 *
 *  puts view
 *  => "subtle"
 */

VALUE
subViewToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subViewKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a View
 *
 *  view.kill
 *  => nil
 */

VALUE
subViewKill(VALUE self)
{
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = FIX2INT(id);

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_VIEW_KILL", data, 32, True);

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
