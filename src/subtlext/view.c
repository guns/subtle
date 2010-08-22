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
  char **name,
  Window *win)
{
  int ret = -1;
  Window *views = NULL;

  assert(match);

  /* Find view id */
  if((views = (Window *)subSharedPropertyGet(display, DefaultRootWindow(display),
      XA_WINDOW, XInternAtom(display, "_NET_VIRTUAL_ROOTS", False), NULL)))
    {
      int i, size = 0;
      long digit = -1;
      char **names = NULL;
      regex_t *preg = NULL;

      names = subSharedPropertyStrings(display, DefaultRootWindow(display),
        XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size);
      preg  = subSharedRegexNew(match);

      if(isdigit(match[0])) digit = strtol(match, NULL, 0);

      for(i = 0; i < size; i++)
        {
          /* Find view either by name or by window id */
          if(digit == (long)i || digit == views[i] ||
              subSharedRegexMatch(preg, names[i]))
            {
              subSharedLogDebug("Found: type=view, match=%s, name=%s win=%#lx, id=%d\n",
                match, names[i], views[i], i);

              if(win) *win   = views[i];
              if(name) *name = strdup(names[i]);

              ret = i;
              break;
            }
        }

      subSharedRegexKill(preg);
      XFreeStringList(names);
      free(views);
    }
  else subSharedLogDebug("Failed finding view `%s'\n", match);

  return ret;
} /* }}} */

/* ViewSelect {{{ */
static VALUE
ViewSelect(VALUE self,
  int dir)
{
  int id, size = 0;
  char **names = NULL;
  VALUE ret = Qnil;

  rb_check_frozen(self);
  subSubtlextConnect(); ///< Implicit open connection

  /* Fetch data */
  id    = FIX2INT(rb_iv_get(self, "@id"));
  names = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size); 

  if(names)
    {
      /* Select view */
      if(SUB_VIEW_NEXT == dir && id < (size - 1))
        {
          id++;
        }
      else if(SUB_VIEW_PREV == dir && 0 < id)
        {
          id--;
        }
      else return Qnil;

      /* Create view */
      ret = subViewInstantiate(names[id]);
      subViewUpdate(ret);

      XFreeStringList(names);
    }

  return ret;
} /* }}} */

/* subViewInstantiate {{{ */
VALUE
subViewInstantiate(char *name)
{
  VALUE klass = Qnil, view = Qnil;

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
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@win",  Qnil);
  rb_iv_set(self, "@name", name);

  subSubtlextConnect(); ///< Implicit open connection

  return self;
} /* }}} */

/* subViewFind {{{ */
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
subViewFind(VALUE self,
  VALUE value)
{
  int id = 0;
  Window win = None;
  VALUE parsed = Qnil, view = Qnil;
  char *name = NULL, buf[50] = { 0 };

  subSubtlextConnect(); ///< Implicit open connection

  /* Check object type */
  if(T_SYMBOL == rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), NULL)))
    {
      if(CHAR2SYM("all") == parsed)
        return subViewAll(Qnil);
      else if(CHAR2SYM("current") == parsed)
        return subViewCurrent(Qnil);
    }

  /* Find view */
  if(-1 != (id = ViewFind(buf, &name, &win)))
    {
      if(!NIL_P((view = subViewInstantiate(name))))
        {
          rb_iv_set(view, "@id",  INT2FIX(id));
          rb_iv_set(view, "@win", LONG2NUM(win));
        }

      free(name);
    }

  return view;
} /* }}} */

/* subViewCurrent {{{ */
/*
 * call-seq: current -> Subtlext::View
 *
 * Get current active View
 *
 *  Subtlext::View.current
 *  => #<Subtlext::View:xxx>
 */

VALUE
subViewCurrent(VALUE self)
{
  int size = 0;
  char **names = NULL;
  unsigned long *cv = NULL;
  Window *views = NULL;
  VALUE view = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  /* Get current view */
  names = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size);
  cv    = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);
  views = (Window *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_WINDOW, XInternAtom(display, "_NET_VIRTUAL_ROOTS", False), NULL);

  /* Create instance */
  view = subViewInstantiate(names[*cv]);
  rb_iv_set(view, "@win", LONG2NUM(views[*cv]));

  if(!NIL_P(view)) subViewUpdate(view);

  XFreeStringList(names);
  free(views);
  free(cv);

  return view;
} /* }}} */

/* subViewAll {{{ */
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
subViewAll(VALUE self)
{
  int i, size = 0;
  char **names = NULL;
  Window *views = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("View"));
  names = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size);
  views = (Window *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_WINDOW, XInternAtom(display, "_NET_VIRTUAL_ROOTS", False), NULL);
  array = rb_ary_new2(size);

  if(names && views)
    {
      for(i = 0; i < size; i++)
        {
          VALUE v = rb_funcall(klass, meth, 1, rb_str_new2(names[i]));

          rb_iv_set(v, "@id",  INT2FIX(i));
          rb_iv_set(v, "@win", LONG2NUM(views[i]));
          rb_ary_push(array, v);
        }

      XFreeStringList(names);
      free(views);
    }

  return array;
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
  VALUE name = rb_iv_get(self, "@name");

  rb_check_frozen(self);
  subSubtlextConnect(); ///< Implicit open connection

  /* Check object type */
  if(T_STRING == rb_type(name))
    {
      int id = -1;
      Window win = None;

      /* Create view if needed */
      if(-1 == (id = ViewFind(RSTRING_PTR(name), NULL, &win)))
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };

          snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
          subSharedMessage(display, DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data, True);

          id = ViewFind(RSTRING_PTR(name), NULL, NULL);
        }

      /* Guess view id */
      if(-1 == id)
        {
          int size = 0;
          char **names = NULL;

          names = subSharedPropertyStrings(display, DefaultRootWindow(display),
            XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size);

          id = size; ///< New id should be last

          XFreeStringList(names);
        }

      rb_iv_set(self, "@id",  INT2FIX(id));
      rb_iv_set(self, "@win", LONG2NUM(win));
    }
  else rb_raise(rb_eArgError, "Unknown value type");

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
  int i, size = 0;
  Window *clients = NULL;
  VALUE win = Qnil, klass = Qnil, meth = Qnil, array = Qnil, client = Qnil;
  unsigned long *tags1 = NULL;

  rb_check_frozen(self);
  subSubtlextConnect(); ///< Implicit open connection

  /* Fetch data */
  win     = rb_iv_get(self, "@win");
  klass   = rb_const_get(mod, rb_intern("Client"));
  meth    = rb_intern("new");
  array   = rb_ary_new2(size);
  clients = subSubtlextList("_NET_CLIENT_LIST", &size);
  tags1   = (unsigned long *)subSharedPropertyGet(display, NUM2LONG(win), XA_CARDINAL,
    XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

  /* Populate array */
  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          unsigned long *tags2 = NULL, *flags = NULL;

          /* Get window flags */
          tags2 = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);
          flags = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_FLAGS", False), NULL);

          /* Check if there are common tags or window is stick */
          if((tags2 && *tags1 & *tags2) || *flags & SUB_EWMH_STICK)
            {
              if(!NIL_P(client = rb_funcall(klass, meth, 1, LONG2NUM(clients[i]))))
                {
                  subClientUpdate(client);
                  rb_ary_push(array, client);
                }
            }

          if(tags2) free(tags2);
          if(flags) free(flags);
        }

      free(clients);
    }

  free(tags1);

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
  VALUE id = rb_iv_get(self, "@id");
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  rb_check_frozen(self);
  subSubtlextConnect(); ///< Implicit open connection

  data.l[0] = FIX2INT(id);

  subSharedMessage(display, DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", data, True);

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
  unsigned long *cv = NULL;
  VALUE id = Qnil, ret = Qfalse;;

  id = rb_iv_get(self, "@id");
  cv = (unsigned long *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_CARDINAL, XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

  if(FIX2INT(id) == *cv) ret = Qtrue;
  free(cv);

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
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
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
  VALUE id = rb_iv_get(self, "@id");

  subSubtlextConnect(); ///< Implicit open connection

  if(RTEST(id))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2INT(id);

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_VIEW_KILL", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed killing view");

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
