 /**
  * @package subtlext
  *
  * @file Client functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtlext.h"

/* ClientToggle {{{ */
VALUE
ClientToggle(VALUE self,
  char *type,
  int flag)
{
  Window win = 0;

  rb_check_frozen(self);
  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Find window */
  if((win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int flags = 0;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      flags     = FIX2INT(rb_iv_get(self, "@flags"));
      data.l[1] = XInternAtom(display, type, False);

      /* Toggle flag */
      if(flags & flag) flags &= ~flag;
      else flags |= flag;

      rb_iv_set(self, "@flags", INT2FIX(flags));

      subSharedMessage(display, win, "_NET_WM_STATE", data, 32, True);
    }
  else rb_raise(rb_eStandardError, "Failed toggling client");

  return Qnil;
} /* }}} */

/* ClientSelect {{{ */
VALUE
ClientSelect(VALUE self,
  int type)
{
  int nclients = 0, i, match = (1L << 30), score = 0, found = 0;
  Window *clients = NULL;
  VALUE win = Qnil, client = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  if((clients = subSubtlextList("_NET_CLIENT_LIST", &nclients)))
    {
      XRectangle geometry1 = { 0 }, geometry2 = { 0 };
      int *gravity1 = (int *)subSharedPropertyGet(display, NUM2LONG(win),
        XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_GRAVITY", False),
        NULL);

      if(gravity1)
        {
          subSharedPropertyGeometry(display, win, &geometry1);

          /* Iterate once to find a client score-based */
          for(i = 0; i < nclients; i++)
            {
              int *gravity2 = (int *)subSharedPropertyGet(display, clients[i],
                XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_GRAVITY",
                  False), NULL);

              /* Check if there are common tags */
              if(gravity2 && win != clients[i] && *gravity1 != *gravity2)
                {
                  subSharedPropertyGeometry(display, clients[i], &geometry2);

                  if(match > (score = subSharedMatch(type,
                      &geometry1, &geometry2)))
                    {
                      match = score;
                      found = i;
                    }

                }

              if(gravity2) free(gravity2);
            }

          /* Create object from found window */
          if(found)
            {
              client = subClientInstantiate(found);

              subClientUpdate(client);
            }

          free(gravity1);
        }

      free(clients);
    }

  return client;
} /* }}} */

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

/* Singleton */

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
  int id = 0, flags = 0;
  Window win = None;
  VALUE parsed = Qnil, client = Qnil;
  char *name = NULL, buf[50] = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  if(T_SYMBOL == rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), &flags)))
    {
      if(CHAR2SYM("visible") == parsed)
        return subClientSingVisible(Qnil);
      else if(CHAR2SYM("all") == parsed)
        return subClientSingAll(Qnil);
      else if(CHAR2SYM("current") == parsed)
        return subClientSingCurrent(Qnil);
    }

  /* Find client */
  if(-1 != (id = subSubtlextFindWindow("_NET_CLIENT_LIST",
      buf, NULL, &win, flags)))
    {
      if(!NIL_P((client = subClientInstantiate(id))))
        subClientUpdate(client);

      free(name);
    }

  return client;
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

  if((focus = (unsigned long *)subSharedPropertyGet(display,
      DefaultRootWindow(display), XA_WINDOW,
      XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
    {
      client = subClientInstantiate(-1);

      /* Update client values */
      if(!NIL_P(client))
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
  clients = subSubtlextList("_NET_CLIENT_LIST", &nclients);
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
            "SUBTLE_WINDOW_TAGS", False), NULL);

          /* Create client on match */
          if(tags && *tags && *visible & *tags &&
              !NIL_P(client = rb_funcall(klass, meth, 1, INT2FIX(i))))
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
  clients = subSubtlextList("_NET_CLIENT_LIST", &nclients);

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
  rb_iv_set(self, "@hidden",   Qfalse);

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
  VALUE rid = Qnil, rwin = Qnil;

  rb_check_frozen(self);
  subSubtlextConnect(NULL); ///< Implicit open connection

  rid  = rb_iv_get(self, "@id");
  rwin = rb_iv_get(self, "@win");

  /* Find window */
  if((NIL_P(rwin) && FIXNUM_P(rid)) ||
      ((NIL_P(rid) || 0 > FIX2INT(rid)) && FIXNUM_P(rwin)))
    {
      char buf[20] = { 0 };

      if(FIXNUM_P(rwin)) snprintf(buf, sizeof(buf), "%ld", NUM2LONG(rwin));
      else snprintf(buf, sizeof(buf), "%d", FIX2INT(rid));

      id = subSubtlextFindWindow("_NET_CLIENT_LIST", buf,
        NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS));
    }
  else
    {
      id  = FIX2INT(rid);
      win = NUM2LONG(rwin);
    }

  /* Check values */
  if(0 <= id && 0 < win)
    {
      int *flags = NULL;
      char *wmname = NULL, *wminstance = NULL, *wmclass = NULL, *role = NULL;

      /* Get name, instance and class */
      subSharedPropertyClass(display, win, &wminstance, &wmclass);
      subSharedPropertyName(display, win, &wmname, wmclass);

      flags = (int *)subSharedPropertyGet(display, win, XA_CARDINAL,
        XInternAtom(display, "SUBTLE_WINDOW_FLAGS", False), NULL);
      role = subSharedPropertyGet(display, win, XA_STRING,
        XInternAtom(display, "WM_WINDOW_ROLE", False), NULL);

      /* Set properties */
      rb_iv_set(self, "@id",       INT2FIX(id));
      rb_iv_set(self, "@win",      LONG2NUM(win));
      rb_iv_set(self, "@flags",    flags ? INT2FIX(*flags) : INT2FIX(0));
      rb_iv_set(self, "@name",     rb_str_new2(wmname));
      rb_iv_set(self, "@instance", rb_str_new2(wminstance));
      rb_iv_set(self, "@klass",    rb_str_new2(wmclass));
      rb_iv_set(self, "@role",     role ? rb_str_new2(role) : Qnil);

      /* Set to nil for on demand loading */
      rb_iv_set(self, "@geometry", Qnil);
      rb_iv_set(self, "@gravity",  Qnil);
      rb_iv_set(self, "@screen",   Qnil);

      if(flags) free(flags);
      if(role) free(role);
      free(wmname);
      free(wminstance);
      free(wmclass);
    }
  else rb_raise(rb_eStandardError, "Failed updating client");

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
  names   = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  view_tags   = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VIEW_TAGS", False), NULL);
  client_tags = (unsigned long *)subSharedPropertyGet(display, NUM2LONG(win),
    XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);
  flags       = (unsigned long *)subSharedPropertyGet(display, NUM2LONG(win),
    XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_FLAGS", False), NULL);

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

/* subClientStateFullAsk {{{ */
/*
 * call-seq: is_full? -> true or false
 *
 * Check if a Client is fullscreen
 *
 *  client.is_full?
 *  => true
 *
 *  client.is_full?
 *  => false
 */

VALUE
subClientStateFullAsk(VALUE self)
{
  rb_check_frozen(self);

  return FIX2INT(rb_iv_get(self, "@flags")) & SUB_EWMH_FULL ? Qtrue : Qfalse;
} /* }}} */

/* subClientStateFloatAsk {{{ */
/*
 * call-seq: is_float? -> true or false
 *
 * Check if a Client is floating
 *
 *  client.is_float?
 *  => true
 *
 *  client.is_float?
 *  => false
 */

VALUE
subClientStateFloatAsk(VALUE self)
{
  rb_check_frozen(self);

  return FIX2INT(rb_iv_get(self, "@flags")) & SUB_EWMH_FLOAT ? Qtrue : Qfalse;
} /* }}} */

/* subClientStateStickAsk {{{ */
/*
 * call-seq: is_stick? -> true or false
 *
 * Check if a client is sticky
 *
 *  client.is_stick?
 *  => true
 *
 *  client.is_stick?
 *  => false
 */

VALUE
subClientStateStickAsk(VALUE self)
{
  rb_check_frozen(self);

  return FIX2INT(rb_iv_get(self, "@flags")) & SUB_EWMH_STICK ? Qtrue : Qfalse;
} /* }}} */

/* subClientToggleFull {{{ */
/*
 * call-seq: toggle_full -> nil
 *
 * Toggle Client fullscreen state
 *
 *  client.toggle_full
 *  => nil
 */

VALUE
subClientToggleFull(VALUE self)
{
  return ClientToggle(self, "_NET_WM_STATE_FULLSCREEN", SUB_EWMH_FULL);
} /* }}} */

/* subClientToggleFloat {{{ */
/*
 * call-seq: toggle_float -> nil
 *
 * Toggle Client floating state
 *
 *  client.toggle_float
 *  => nil
 */

VALUE
subClientToggleFloat(VALUE self)
{
  return ClientToggle(self, "_NET_WM_STATE_ABOVE", SUB_EWMH_FLOAT);
} /* }}} */

/* subClientToggleStick {{{ */
/*
 * call-seq: toggle_stick -> nil
 *
 * Toggle Client sticky state
 *
 *  client.toggle_stick
 *  => nil
 */

VALUE
subClientToggleStick(VALUE self)
{
  return ClientToggle(self, "_NET_WM_STATE_STICKY", SUB_EWMH_STICK);
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

/* subClientSelectUp {{{ */
/*
 * call-seq: up -> Subtlext::Client or nil
 *
 * Get Client above
 *
 *  client.up
 *  => #<Subtlext::Client:xxx>
 *
 *  client.up
 *  => nil
 */

VALUE
subClientSelectUp(VALUE self)
{
  return ClientSelect(self, SUB_WINDOW_UP);
} /* }}} */

/* subClientSelectLeft {{{ */
/*
 * call-seq: left -> Subtlext::Client or nil
 *
 * Get Client left
 *
 *  client.left
 *  => #<Subtlext::Client:xxx>
 *
 *  client.left
 *  => nil
 */

VALUE
subClientSelectLeft(VALUE self)
{
  return ClientSelect(self, SUB_WINDOW_LEFT);
} /* }}} */

/* subClientSelectRight {{{ */
/*
 * call-seq: right -> Subtlext::Client or nil
 *
 * Get Client right
 *
 *  client.right
 *  => #<Subtlext::Client:xxx>
 *
 *  client.right
 *  => nil
 */

VALUE
subClientSelectRight(VALUE self)
{
  return ClientSelect(self, SUB_WINDOW_RIGHT);
} /* }}} */

/* subClientSelectDown {{{ */
/*
 * call-seq: down -> Subtlext::Client or nil
 *
 * Get Client below
 *
 *  client.down
 *  => #<Subtlext::Client:xxx>
 *
 *  client.down
 *  => nil
 */

VALUE
subClientSelectDown(VALUE self)
{
  return ClientSelect(self, SUB_WINDOW_DOWN);
} /* }}} */

/* subClientAliveAsk {{{ */
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
subClientAliveAsk(VALUE self)
{
  VALUE ret = Qfalse, name = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", name);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Just find the client */
  if(-1 != subSubtlextFindWindow("_NET_CLIENT_LIST", RSTRING_PTR(name),
      NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    ret = Qtrue;

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
          XInternAtom(display, "SUBTLE_WINDOW_GRAVITY", False), NULL)))
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
 *
 * Set Client Gravity
 *
 *  client.gravity = 0
 *  => nil
 *
 *  client.gravity = :center
 *  => nil
 *
 *  client.gravity = Subtlext::Gravity[0]
 *  => nil
 *
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
  if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Gravity"))))
    gravity = value;
  else gravity = subGravitySingFind(Qnil, value);

  /* Set gravity */
  if(Qnil != gravity)
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2LONG(id);
      data.l[1] = FIX2LONG(rb_iv_get(gravity, "@id"));

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_WINDOW_GRAVITY", data, 32, True);

      rb_iv_set(self, "@gravity", gravity);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(value));

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
      Window win = None;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      win       = NUM2LONG(rb_iv_get(self, "@win"));
      data.l[1] = FIX2INT(rb_iv_get(geometry,  "@x"));
      data.l[2] = FIX2INT(rb_iv_get(geometry,  "@y"));
      data.l[3] = FIX2INT(rb_iv_get(geometry,  "@width"));
      data.l[4] = FIX2INT(rb_iv_get(geometry,  "@height"));

      subSharedMessage(display, win, "_NET_MOVERESIZE_WINDOW", data, 32, True);

      rb_iv_set(self, "@geometry", geometry);
    }

  return geometry;
} /* }}} */

/* subClientResizeWriter {{{ */
/*
 * call-seq: resize=(bool) -> nil
 *
 * Set Client resize
 *
 *  client.resize = :false
 *  => nil
 *
 */

VALUE
subClientResizeWriter(VALUE self,
  VALUE value)
{
  VALUE id = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check instance type */
  if(Qtrue == value || Qfalse == value)
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2LONG(id);
      data.l[1] = (Qtrue == value);

      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_WINDOW_RESIZE", data, 32, True);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(value));

  return Qnil;
} /* }}} */

/* subClientShow {{{ */
/*
 * call-seq: show() -> nil
 *
 * Show a Client
 *
 *  client.show
 *  => nil
 */

VALUE
subClientShow(VALUE self)
{
  VALUE win = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  rb_iv_set(self, "@hidden", Qfalse);

  XMapWindow(display, NUM2LONG(win));
  XSync(display, False); ///< Sync all changes

  return Qnil;
} /* }}} */

/* subClientHide {{{ */
/*
 * call-seq: hide() -> nil
 *
 * Hide a Client
 *
 *  client.hide
 *  => nil
 */

VALUE
subClientHide(VALUE self)
{
  VALUE win = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  rb_iv_set(self, "@hidden", Qtrue);

  XUnmapWindow(display, NUM2LONG(win));
  XSync(display, False); ///< Sync all changes

  return Qnil;
} /* }}} */

/* subClientHiddenAsk {{{ */
/*
 * call-seq: hidden -> true or false
 *
 * Whether Client is hidden
 *
 *  client.hidden?
 *  => true
 */

VALUE
subClientHiddenAsk(VALUE self)
{
  return rb_iv_get(self, "@hidden");
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
