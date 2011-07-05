 /**
  * @package subtlext
  *
  * @file Client functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* ClientRestack {{{ */
VALUE
ClientRestack(VALUE self,
  int detail)
{
  VALUE win = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = 2; ///< Claim to be a pager
  data.l[1] = NUM2LONG(win);
  data.l[2] = detail;

  subSharedMessage(display, DefaultRootWindow(display),
    "_NET_RESTACK_WINDOW", data, 32, True);

  return Qnil;
} /* }}} */

/* ClientFlagsGet {{{ */
static VALUE
ClientFlagsGet(VALUE self,
  int flag)
{
  VALUE flags = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@flags", flags)

  flags = rb_iv_get(self, "@flags");

  return (FIXNUM_P(flags) && FIX2INT(flags) & flag) ? Qtrue : Qfalse;
} /* }}} */

/* ClientFlagsGet {{{ */
static VALUE
ClientFlagsSet(VALUE self,
  int flag,
  int toggle)
{
  int iflags = flag;
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id",    id);

  /* Toggle flags */
  if(toggle)
    {
      VALUE flags = Qnil;

      GET_ATTR(self, "@flags", flags);

      iflags = FIX2INT(flags);
      if(iflags & flag) iflags &= ~flag;
      else iflags |= flag;
    }

  /* Assemble message */
  data.l[0] = FIX2LONG(id);
  data.l[1] = iflags;

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_CLIENT_FLAGS", data, 32, True);

  rb_iv_set(self, "@flags", INT2FIX(iflags));

  return Qnil;
} /* }}} */

/* ClientFlagsToggle {{{ */
VALUE
ClientFlagsToggle(VALUE self,
  char *type,
  int flag)
{
  int iflags = 0;
  VALUE win = Qnil, flags = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win",   win);
  GET_ATTR(self, "@flags", flags);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Update flags */
  iflags = FIX2INT(flags);
  if(iflags & flag) iflags &= ~flag;
  else iflags |= flag;

  rb_iv_set(self, "@flags", INT2FIX(iflags));

  /* Send message */
  data.l[0] = XInternAtom(display, "_NET_WM_STATE_TOGGLE", False);
  data.l[1] = XInternAtom(display, type, False);

  subSharedMessage(display, NUM2LONG(win), "_NET_WM_STATE", data, 32, True);

  return Qnil;
} /* }}} */

/* ClientGravity {{{ */
static int
ClientGravity(VALUE key,
  VALUE value,
  VALUE client)
{
  SubMessageData data = { .l = { -1, -1, -1, -1, -1 } };

  data.l[0] = FIX2INT(rb_iv_get(client, "@id"));

  /* Find gravity */
  if(RTEST(value))
    {
      VALUE gravity = subGravitySingFind(Qnil, value);

      data.l[1] = RTEST(gravity) ? FIX2INT(rb_iv_get(gravity, "@id")) : -1;
    }

  /* Find view id if any */
  if(RTEST(key))
    {
      VALUE view = subViewSingFind(Qnil, key);

      data.l[2] = RTEST(view) ? FIX2INT(rb_iv_get(view, "@id")) : -1;
    }

  /* Finally send */
  if(-1 != data.l[0] && -1 != data.l[1])
    {
      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_CLIENT_GRAVITY", data, 32, True);
    }

  return ST_CONTINUE;
} /* }}} */

/* ClientWindowId {{{ */
int
ClientWindowId(Window win)
{
  int ret = -1, size = 0;
  Window *wins = NULL;

  /* Fetch window list */
  if((wins = subSubtlextWindowList("_NET_CLIENT_LIST", &size)))
    {
      int i;

      /* Check if window is in window list */
      for(i = 0; i < size; i++)
        {
          if(wins[i] == win)
            {
              ret = i;
              break;
            }
        }

      free(wins);
    }
  else subSharedLogDebugSubtlext("Failed finding window `%#lx'\n", win);

  return ret;
} /* }}} */

/* Singleton */

/* subClientSingSelect {{{ */
/*
 * call-seq: select -> Subtlext::Client or nil
 *
 * Select a client
 *
 *  Subtlext::Client.select
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientSingSelect(VALUE self)
{
  VALUE win = subSubtleSingSelect(self);

  return None != NUM2LONG(win) ? subClientSingFind(self, win) : Qnil;
} /* }}} */

/* subClientSingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Client or nil
 *           [value]     -> Subtlext::Client or nil
 *
 * Find Client by a given value which can be of following type:
 *
 * [fixnum] Array id
 * [string] Match against WM_NAME or WM_CLASS
 * [hash]   With one of following keys: :title, :name, :class, :gravity
 * [symbol] Either :current for current Client or :all for an array
 *
 *  Subtlext::Client.find("subtle")
 *  => #<Subtlext::Client:xxx>
 *
 *  Subtlext::Client[:name => "subtle"]
 *  => #<Subtlext::Client:xxx>
 *
 *  Subtlext::Client["subtle"]
 *  => nil
 */

VALUE
subClientSingFind(VALUE self,
  VALUE value)
{
  int i, flags = 0, size = 0;
  VALUE ret = Qnil, parsed = Qnil, client = Qnil;
  char buf[50] = { 0 };
  Window *wins = NULL;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  switch(rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), &flags)))
    {
      case T_SYMBOL:
        if(CHAR2SYM("visible") == parsed)
          return subClientSingVisible(Qnil);
        else if(CHAR2SYM("all") == parsed)
          return subClientSingAll(Qnil);
        else if(CHAR2SYM("current") == parsed)
          return subClientSingCurrent(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Client"))))
          return parsed;
    }

  /* Get client list */
  if((wins = subSubtlextWindowList("_NET_CLIENT_LIST", &size)))
    {
      int selid = -1;
      Window selwin = None;
      VALUE meth = Qnil, klass = Qnil;
      regex_t *preg = NULL;

      /* Create regexp when required */
      if(!(flags & SUB_MATCH_EXACT)) preg = subSharedRegexNew(buf);

      /* Special values */
      if(FIXNUM_P(value)) selid  = (int)FIX2INT(value);
      if('#' == buf[0])   selwin = subSubtleSingSelect(Qnil);

      /* Fetch data */
      meth  = rb_intern("new");
      klass = rb_const_get(mod, rb_intern("Client"));

      /* Check each client */
      for(i = 0; i < size; i++)
        {
          if(selid == i || selwin == wins[i] || (-1 == selid &&
              subSubtlextWindowMatch(wins[i], preg, buf, NULL, flags)))
            {
              /* Create new client */
              if(RTEST((client = rb_funcall(klass, meth, 1, INT2FIX(i)))))
                {
                  rb_iv_set(client, "@win", LONG2NUM(wins[i]));

                  subClientUpdate(client);

                  ret = subSubtlextOneOrMany(client, ret);
                }
            }
        }

      if(preg) subSharedRegexKill(preg);
      free(wins);
    }

  return ret;
} /* }}} */

/* subClientSingCurrent {{{ */
/*
 * call-seq: current -> Subtlext::Client
 *
 * Get current active Client
 *
 *  Subtlext::Client.current
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientSingCurrent(VALUE self)
{
  VALUE client = Qnil;
  unsigned long *focus = NULL;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get current client */
  if((focus = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_WINDOW,
      XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
    {
      int id = ClientWindowId(*focus);

      /* Update client values */
      if(RTEST(client = subClientInstantiate(id)))
        {
          rb_iv_set(client, "@win", LONG2NUM(*focus));

          subClientUpdate(client);
        }

      free(focus);
    }
  else rb_raise(rb_eStandardError, "Failed getting current client");

  return client;
} /* }}} */

/* subClientSingVisible {{{ */
/*
 * call-seq: visible -> Array
 *
 * Get Array of all visible Client
 *
 *  Subtlext::Client.visible
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  Subtlext::Client.visible
 *  => []
 */

VALUE
subClientSingVisible(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  unsigned long *visible = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, client = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  array   = rb_ary_new();
  klass   = rb_const_get(mod, rb_intern("Client"));
  clients = subSubtlextWindowList("_NET_CLIENT_LIST", &nclients);
  visible = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VISIBLE_TAGS", False), NULL);

  /* Check results */
  if(clients && visible)
    {
      for(i = 0; i < nclients; i++)
        {
          unsigned long *tags = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL, XInternAtom(display,
            "SUBTLE_CLIENT_TAGS", False), NULL);

          /* Create client on match */
          if(tags && *tags && *visible & *tags &&
              RTEST(client = rb_funcall(klass, meth, 1, INT2FIX(i))))
            {
              rb_iv_set(client, "@win", LONG2NUM(clients[i]));

              subClientUpdate(client);
              rb_ary_push(array, client);
            }

          if(tags) free(tags);
        }
    }

  if(clients) free(clients);
  if(visible) free(visible);

  return array;
} /* }}} */

/* subClientSingAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get Array of all Client
 *
 *  Subtlext::Client.all
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  Subtlext::Client.all
 *  => []
 */

VALUE
subClientSingAll(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, client = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  array   = rb_ary_new();
  klass   = rb_const_get(mod, rb_intern("Client"));
  clients = subSubtlextWindowList("_NET_CLIENT_LIST", &nclients);

  /* Check results */
  if(clients)
    {
      for(i = 0; i < nclients; i++)
        {
          /* Create client */
          if(RTEST(client = rb_funcall(klass, meth, 1, INT2FIX(i))))
            {
              rb_iv_set(client, "@win", LONG2NUM(clients[i]));

              subClientUpdate(client);
              rb_ary_push(array, client);
            }
        }

      free(clients);
    }

  return array;
} /* }}} */

/* subClientSingRecent {{{ */
/*
 * call-seq: recent -> Array
 *
 * Get recent active clients
 *
 *  Subtlext::Client.recent
 *  => [ #<Subtlext::Client:xxx> ]
 */

VALUE
subClientSingRecent(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, client = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  array   = rb_ary_new();
  klass   = rb_const_get(mod, rb_intern("Client"));
  clients = subSubtlextWindowList("_NET_ACTIVE_WINDOW", &nclients);

  /* Check results */
  if(clients)
    {
      for(i = 0; i < nclients; i++)
        {
          /* Create client */
          if(!NIL_P(client = rb_funcall(klass, meth, 1, INT2FIX(i))))
            {
              rb_iv_set(client, "@win", LONG2NUM(clients[i]));

              subClientUpdate(client);
              rb_ary_push(array, client);
            }
        }

      free(clients);
    }

  return array;
} /* }}} */

/* Class */

/* subClientInstantiate {{{ */
VALUE
subClientInstantiate(int id)
{
  VALUE klass = Qnil, client = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Client"));
  client = rb_funcall(klass, rb_intern("new"), 1, INT2FIX(id));

  return client;
} /* }}} */

/* subClientInit {{{ */
/*
 * call-seq: new(win) -> Subtlext::Client
 *
 * Create a new Client object
 *
 *  client = Subtlext::Client.new(1)
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientInit(VALUE self,
  VALUE id)
{
  if(!FIXNUM_P(id))
    rb_raise(rb_eArgError, "Unexpected value-type `%s'", rb_obj_classname(id));

  /* Init object */
  rb_iv_set(self, "@id",       id);
  rb_iv_set(self, "@win",      Qnil);
  rb_iv_set(self, "@name",     Qnil);
  rb_iv_set(self, "@instance", Qnil);
  rb_iv_set(self, "@klass",    Qnil);
  rb_iv_set(self, "@role",     Qnil);
  rb_iv_set(self, "@geometry", Qnil);
  rb_iv_set(self, "@gravity",  Qnil);
  rb_iv_set(self, "@screen",   Qnil);
  rb_iv_set(self, "@flags",    Qnil);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subClientUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Client properties
 *
 *  client.update
 *  => nil
 */

VALUE
subClientUpdate(VALUE self)
{
  int id = 0;
  Window win = None;

  rb_check_frozen(self);
  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get client values */
  id  = FIX2INT(rb_iv_get(self, "@id"));
  win = NUM2LONG(rb_iv_get(self, "@win"));

  /* Check values */
  if(0 <= id && 0 < win)
    {
      int *flags = NULL;
      char *wmname = NULL, *wminstance = NULL, *wmclass = NULL, *role = NULL;

      /* Fetch name, instance and class */
      subSharedPropertyClass(display, win, &wminstance, &wmclass);
      subSharedPropertyName(display, win, &wmname, wmclass);

      /* Fetch window flags and role */
      flags = (int *)subSharedPropertyGet(display, win, XA_CARDINAL,
        XInternAtom(display, "SUBTLE_CLIENT_FLAGS", False), NULL);
      role = subSharedPropertyGet(display, win, XA_STRING,
        XInternAtom(display, "WM_WINDOW_ROLE", False), NULL);

      /* Set properties */
      rb_iv_set(self, "@flags",    flags ? INT2FIX(*flags) : INT2FIX(0));
      rb_iv_set(self, "@name",     rb_str_new2(wmname));
      rb_iv_set(self, "@instance", rb_str_new2(wminstance));
      rb_iv_set(self, "@klass",    rb_str_new2(wmclass));
      rb_iv_set(self, "@role",     role ? rb_str_new2(role) : Qnil);

      /* Set to nil for on demand loading */
      rb_iv_set(self, "@geometry", Qnil);
      rb_iv_set(self, "@gravity",  Qnil);

      if(flags) free(flags);
      if(role) free(role);
      free(wmname);
      free(wminstance);
      free(wmclass);
    }
  else rb_raise(rb_eStandardError, "Invalid client");

  return Qnil;
} /* }}} */

/* subClientViewList {{{ */
/*
 * call-seq: views -> Array
 *
 * Get Array of View Client is on
 *
 *  client.views
 *  => [#<Subtlext::View:xxx>, #<Subtlext::View:xxx>]
 *
 *  client.views
 *  => nil
 */

VALUE
subClientViewList(VALUE self)
{
  int i, nnames = 0;
  char **names = NULL;
  VALUE win = Qnil, array = Qnil, method = Qnil, klass = Qnil;
  unsigned long *view_tags = NULL, *client_tags = NULL, *flags = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  method  = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("View"));
  array   = rb_ary_new();
  names   = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  view_tags   = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VIEW_TAGS", False), NULL);
  client_tags = (unsigned long *)subSharedPropertyGet(display, NUM2LONG(win),
    XA_CARDINAL, XInternAtom(display, "SUBTLE_CLIENT_TAGS", False), NULL);
  flags       = (unsigned long *)subSharedPropertyGet(display, NUM2LONG(win),
    XA_CARDINAL, XInternAtom(display, "SUBTLE_CLIENT_FLAGS", False), NULL);

  /* Check results */
  if(names && view_tags && client_tags)
    {
      for(i = 0; i < nnames; i++)
        {
          /* Check if there are common tags or window is stick */
          if((view_tags[i] & *client_tags) ||
              (flags && *flags & SUB_EWMH_STICK))
            {
              VALUE v = rb_funcall(klass, method, 1, rb_str_new2(names[i]));

              rb_iv_set(v, "@id",  INT2FIX(i));
              rb_ary_push(array, v);
            }
        }

    }

  if(names)       XFreeStringList(names);
  if(view_tags)   free(view_tags);
  if(client_tags) free(client_tags);
  if(flags)       free(flags);

  return array;
} /* }}} */

/* subClientFlagsAskFull {{{ */
/*
 * call-seq: is_full? -> true or false
 *
 * Check if Client is fullscreen
 *
 *  client.is_full?
 *  => true
 *
 *  client.is_full?
 *  => false
 */

VALUE
subClientFlagsAskFull(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_FULL);
} /* }}} */

/* subClientFlagsAskFloat {{{ */
/*
 * call-seq: is_float? -> true or false
 *
 * Check if Client is floating
 *
 *  client.is_float?
 *  => true
 *
 *  client.is_float?
 *  => false
 */

VALUE
subClientFlagsAskFloat(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_FLOAT);
} /* }}} */

/* subClientFlagsAskStick {{{ */
/*
 * call-seq: is_stick? -> true or false
 *
 * Check if Client is sticky
 *
 *  client.is_stick?
 *  => true
 *
 *  client.is_stick?
 *  => false
 */

VALUE
subClientFlagsAskStick(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_STICK);
} /* }}} */

/* subClientFlagsAskResize {{{ */
/*
 * call-seq: is_resize? -> true or false
 *
 * Check if Client uses size hints
 *
 *  client.is_resize?
 *  => true
 *
 *  client.is_resize?
 *  => false
 */

VALUE
subClientFlagsAskResize(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_RESIZE);
} /* }}} */

/* subClientFlagsAskUrgent {{{ */
/*
 * call-seq: is_urgent? -> true or false
 *
 * Check if Client is urgent
 *
 *  client.is_urgent?
 *  => true
 *
 *  client.is_urgent?
 *  => false
 */

VALUE
subClientFlagsAskUrgent(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_URGENT);
} /* }}} */

/* subClientFlagsAskZaphod {{{ */
/*
 * call-seq: is_zaphod? -> true or false
 *
 * Check if Client is zaphod
 *
 *  client.is_zaphod?
 *  => true
 *
 *  client.is_zaphod?
 *  => false
 */

VALUE
subClientFlagsAskZaphod(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_ZAPHOD);
} /* }}} */

/* subClientFlagsAskFixed {{{ */
/*
 * call-seq: is_fixed? -> true or false
 *
 * Check if Client is fixed
 *
 *  client.is_fixed?
 *  => true
 *
 *  client.is_fixed?
 *  => false
 */

VALUE
subClientFlagsAskFixed(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_FIXED);
} /* }}} */

/* subClientFlagsAskBorderless {{{ */
/*
 * call-seq: is_borderless? -> true or false
 *
 * Check if Client is borderless
 *
 *  client.is_borderless?
 *  => true
 *
 *  client.is_borderless?
 *  => false
 */

VALUE
subClientFlagsAskBorderless(VALUE self)
{
  return ClientFlagsGet(self, SUB_EWMH_BORDERLESS);
} /* }}} */

/* subClientFlagsToggleFull {{{ */
/*
 * call-seq: toggle_full -> nil
 *
 * Toggle Client fullscreen state
 *
 *  client.toggle_full
 *  => nil
 */

VALUE
subClientFlagsToggleFull(VALUE self)
{
  return ClientFlagsToggle(self, "_NET_WM_STATE_FULLSCREEN", SUB_EWMH_FULL);
} /* }}} */

/* subClientFlagsToggleFloat {{{ */
/*
 * call-seq: toggle_float -> nil
 *
 * Toggle Client floating state
 *
 *  client.toggle_float
 *  => nil
 */

VALUE
subClientFlagsToggleFloat(VALUE self)
{
  return ClientFlagsToggle(self, "_NET_WM_STATE_ABOVE", SUB_EWMH_FLOAT);
} /* }}} */

/* subClientFlagsToggleStick {{{ */
/*
 * call-seq: toggle_stick -> nil
 *
 * Toggle Client sticky state
 *
 *  client.toggle_stick
 *  => nil
 */

VALUE
subClientFlagsToggleStick(VALUE self)
{
  return ClientFlagsToggle(self, "_NET_WM_STATE_STICKY", SUB_EWMH_STICK);
} /* }}} */

/* subClientFlagsToggleResize {{{ */
/*
 * call-seq: toggle_stick -> nil
 *
 * Toggle Client resize state
 *
 *  client.toggle_stick
 *  => nil
 */

VALUE
subClientFlagsToggleResize(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_RESIZE, True);
} /* }}} */

/* subClientFlagsToggleUrgent {{{ */
/*
 * call-seq: toggle_urgent -> nil
 *
 * Toggle Client urgent state
 *
 *  client.toggle_urgent
 *  => nil
 */

VALUE
subClientFlagsToggleUrgent(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_URGENT, True);
} /* }}} */

/* subClientFlagsToggleZaphod {{{ */
/*
 * call-seq: toggle_zaphod -> nil
 *
 * Toggle Client zaphod state
 *
 *  client.toggle_zaphod
 *  => nil
 */

VALUE
subClientFlagsToggleZaphod(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_ZAPHOD, True);
} /* }}} */

/* subClientFlagsToggleFixed {{{ */
/*
 * call-seq: toggle_fixed -> nil
 *
 * Toggle Client fixed state
 *
 *  client.toggle_fixed
 *  => nil
 */

VALUE
subClientFlagsToggleFixed(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_FIXED, True);
} /* }}} */

/* subClientFlagsToggleBorderless {{{ */
/*
 * call-seq: toggle_borderless -> nil
 *
 * Toggle Client borderless state
 *
 *  client.toggle_borderless
 *  => nil
 */

VALUE
subClientFlagsToggleBorderless(VALUE self)
{
  return ClientFlagsSet(self, SUB_EWMH_BORDERLESS, True);
} /* }}} */

/* subClientFlagsWriter {{{ */
/*
 * call-seq: flags=(array) -> nil
 *
 * Set multiple flags at once. Flags can be one or a combination of the
 * following:
 *
 * [*:full*]    Set fullscreen mode
 * [*:float*]   Set floating mode
 * [*:stick*]   Set sticky mode
 * [*:resize*]  Set resize mode
 *
 *  client.flags = [ :float, :stick ]
 *  => nil
 */

VALUE
subClientFlagsWriter(VALUE self,
  VALUE value)
{
  /* Check object type */
  if(T_ARRAY == rb_type(value))
    {
      int i, flags = 0;
      VALUE entry = Qnil;

      /* Translate flags */
      for(i = 0; Qnil != (entry = rb_ary_entry(value, i)); ++i)
        {
          if(CHAR2SYM("full") == entry)        flags |= SUB_EWMH_FULL;
          else if(CHAR2SYM("float") == entry)  flags |= SUB_EWMH_FLOAT;
          else if(CHAR2SYM("stick") == entry)  flags |= SUB_EWMH_STICK;
          else if(CHAR2SYM("resize") == entry) flags |= SUB_EWMH_RESIZE;
        }

      ClientFlagsSet(self, flags, False);
    }

  return Qnil;
} /* }}} */

/* subClientRestackRaise {{{ */
/*
 * call-seq: raise -> nil
 *
 * Move Client window to top of window stack
 *
 *  client.raise
 *  => nil
 */

VALUE
subClientRestackRaise(VALUE self)
{
  return ClientRestack(self, Above);
} /* }}} */

/* subClientRestackLower {{{ */
/*
 * call-seq: lower -> nil
 *
 * Move Client window to bottom of window stack
 *
 *  client.raise
 *  => nil
 */

VALUE
subClientRestackLower(VALUE self)
{
  return ClientRestack(self, Below);
} /* }}} */

/* subClientAskAlive {{{ */
/*
 * call-seq: alive? -> true or false
 *
 * Check if client is alive
 *
 *  client.alive?
 *  => true
 *
 *  client.alive?
 *  => false
 */

VALUE
subClientAskAlive(VALUE self)
{
  VALUE ret = Qfalse, win = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Just find the client */
  if(-1 != ClientWindowId(NUM2LONG(win)))
    ret = Qtrue;
  else rb_obj_freeze(self);

  return ret;
} /* }}} */

/* subClientGravityReader {{{ */
/*
 * call-seq: gravity -> Subtlext::Gravity
 *
 * Get Client Gravity
 *
 *  client.gravity
 *  => #<Subtlext::Gravity:xxx>
 */

VALUE
subClientGravityReader(VALUE self)
{
  VALUE win = Qnil, gravity = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Load on demand */
  if(NIL_P((gravity = rb_iv_get(self, "@gravity"))))
    {
      int *id = NULL;
      char buf[5] = { 0 };

      /* Get gravity */
      if((id = (int *)subSharedPropertyGet(display, NUM2LONG(win), XA_CARDINAL,
          XInternAtom(display, "SUBTLE_CLIENT_GRAVITY", False), NULL)))
        {
          /* Create gravity */
          snprintf(buf, sizeof(buf), "%d", *id);
          gravity = subGravityInstantiate(buf);

          subGravityUpdate(gravity);

          rb_iv_set(self, "@gravity", gravity);

          free(id);
        }
    }

  return gravity;
} /* }}} */

/* subClientGravityWriter {{{ */
/*
 * call-seq: gravity=(fixnum) -> nil
 *           gravity=(symbol) -> nil
 *           gravity=(object) -> nil
 *           gravity=(hash)   -> nil
 *
 * Set Client Gravity either for current or for specific view
 *
 *  # Set gravity for current view
 *  client.gravity = 0
 *  => nil
 *
 *  client.gravity = :center
 *  => nil
 *
 *  client.gravity = Subtlext::Gravity[0]
 *  => nil
 *
 *  # Set gravity for specific view
 *  client.gravity = { :terms => :center }
 *  => nil
 */

VALUE
subClientGravityWriter(VALUE self,
  VALUE value)
{
  VALUE id = Qnil, gravity = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check instance type */
  switch(rb_type(value))
    {
      case T_FIXNUM:
      case T_SYMBOL:
      case T_OBJECT:
        if(rb_obj_is_instance_of(value,
            rb_const_get(mod, rb_intern("Gravity"))) ||
            RTEST(gravity = subGravitySingFind(Qnil, value)))
          {
            ClientGravity(Qnil, gravity, self);

            rb_iv_set(self, "@gravity", gravity);
          }
        break;
      case T_HASH:
        rb_hash_foreach(value, ClientGravity, self);
        rb_iv_set(self, "@gravity", Qnil); ///< Reset to update on demand
        break;
      default: rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  return Qnil;
} /* }}} */

/* subClientGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get Client Geometry
 *
 *  client.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subClientGeometryReader(VALUE self)
{
  VALUE win = Qnil, geom = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Load on demand */
  if(NIL_P((geom = rb_iv_get(self, "@geometry"))))
    {
      XRectangle geometry = { 0 };

      subSharedPropertyGeometry(display, NUM2LONG(win), &geometry);

      geom = subGeometryInstantiate(geometry.x, geometry.y,
        geometry.width, geometry.height);

      rb_iv_set(self, "@geometry", geom);
    }

  return geom;
} /* }}} */

/* subClientGeometryWriter {{{ */
/*
 * call-seq: geometry=(x, y, width, height) -> Subtlext::Geometry
 *           geometry=(array)               -> Subtlext::Geometry
 *           geometry=(geometry)            -> Subtlext::Geometry
 *
 * Set Client geometry
 *
 *  client.geometry = 0, 0, 100, 100
 *  => #<Subtlext::Geometry:xxx>
 *
 *  client.geometry = [ 0, 0, 100, 100 ]
 *  => #<Subtlext::Geometry:xxx>
 *
 *  client.geometry = Subtlext::Geometry(0, 0, 100, 100)
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subClientGeometryWriter(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE klass = Qnil, geometry = Qnil;

  rb_check_frozen(self);
  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Delegate arguments */
  klass    = rb_const_get(mod, rb_intern("Geometry"));
  geometry = rb_funcall2(klass, rb_intern("new"), argc, argv);

  /* Update geometry */
  if(RTEST(geometry))
    {
      VALUE win = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      GET_ATTR(self, "@win", win);

      data.l[1] = FIX2INT(rb_iv_get(geometry,  "@x"));
      data.l[2] = FIX2INT(rb_iv_get(geometry,  "@y"));
      data.l[3] = FIX2INT(rb_iv_get(geometry,  "@width"));
      data.l[4] = FIX2INT(rb_iv_get(geometry,  "@height"));

      subSharedMessage(display, NUM2LONG(win),
        "_NET_MOVERESIZE_WINDOW", data, 32, True);

      rb_iv_set(self, "@geometry", geometry);
    }

  return geometry;
} /* }}} */

/* subClientScreenReader {{{ */
/*
 * call-seq: screen -> Subtlext::Screen
 *
 * Get Client Screen
 *
 *  client.screen
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subClientScreenReader(VALUE self)
{
  VALUE screen = Qnil, win = Qnil;
  int *sid = NULL;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Get screen */
  if((sid = (int *)subSharedPropertyGet(display, NUM2LONG(win), XA_CARDINAL,
      XInternAtom(display, "SUBTLE_CLIENT_SCREEN", False), NULL)))
    {
      screen = subScreenSingFind(self, INT2FIX(*sid));

      free(sid);
    }

  return screen;
} /* }}} */

/* subClientToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Client object to String
 *
 *  puts client
 *  => "subtle"
 */

VALUE
subClientToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subClientKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Send a close signal to Client
 *
 *  client.kill
 *  => nil
 */

VALUE
subClientKill(VALUE self)
{
  VALUE win = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Send message */
  data.l[0] = CurrentTime;
  data.l[1] = 2; ///< Claim to be a pager

  subSharedMessage(display, NUM2LONG(win),
    "_NET_CLOSE_WINDOW", data, 32, True);

  rb_obj_freeze(self);

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
