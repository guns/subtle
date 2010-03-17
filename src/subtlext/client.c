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

      subSharedMessage(win, "_NET_WM_STATE", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed toggling client");

  return Qnil;
} /* }}} */

/* ClientMatch {{{ */
VALUE
ClientMatch(VALUE self,
  int type)
{
  int i, id = 0, size = 0, match = (1L << 30), score = 0;
  Window *clients = NULL, *views = NULL, found = None;
  VALUE win = Qnil, client = Qnil;
  unsigned long *cv = NULL, *flags1 = NULL;
  XRectangle geometry1 = { 0 }, geometry2 = { 0 };

  win     = rb_iv_get(self, "@win");
  clients = subSharedClientList(&size);
  views   = (Window *)subSharedPropertyGet(display, 
    DefaultRootWindow(display), XA_WINDOW,
    XInternAtom(display, "_NET_VIRTUAL_ROOTS", False), NULL);
  cv      = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, 
    XInternAtom(display, "_NET_CURRENT_DESKTOP", False), NULL);

  if(clients && cv)
    {
      flags1 = (unsigned long *)subSharedPropertyGet(display, 
        views[*cv], XA_CARDINAL,
        XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);
      subSharedPropertyGeometry(display, win, &geometry1);

      /* Iterate once to find a client score-based */
      for(i = 0; i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(display, 
            clients[i], XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

          if(win != clients[i] && *flags1 & *flags2) ///< Check if there are common tags
            {
              subSharedPropertyGeometry(display, win, &geometry2);

              if(match > (score = subSharedMatch(type, &geometry1, &geometry2)))
                {
                  match = score;
                  found = clients[i];
                  id    = i;
                }

            }

          free(flags2);
        }

      if(found) client = subClientInstantiate(found);

      free(flags1);
      free(clients);
      free(cv);
    }

  return client;
} /* }}} */

/* ClientRestack {{{ */
VALUE
ClientRestack(VALUE self,
  int detail)
{
  VALUE win = rb_iv_get(self, "@win");
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  if(RTEST(win))
    {
      data.l[0] = 2; ///< Claim to be a pager
      data.l[1] = NUM2LONG(win);
      data.l[2] = detail; 

      subSharedMessage(DefaultRootWindow(display), "_NET_RESTACK_WINDOW", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed restacking client");  

  return Qnil;
} /* }}} */

/* subClientInstantiate {{{ */
VALUE
subClientInstantiate(Window win)
{
  VALUE klass = Qnil, client = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Client"));
  client = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(win));

  return client;
} /* }}} */

/* subClientInit {{{ */
/*
 * call-seq: new(win) -> Subtlext::Client
 *
 * Create a new Client object
 *
 *  client = Subtlext::Client.new(123456789)
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientInit(VALUE self,
  VALUE win)
{
  if(!FIXNUM_P(win) && T_BIGNUM != rb_type(win))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",       Qnil);
  rb_iv_set(self, "@win",      win);
  rb_iv_set(self, "@name",     Qnil);
  rb_iv_set(self, "@instance", Qnil);
  rb_iv_set(self, "@klass",    Qnil);
  rb_iv_set(self, "@role",     Qnil);
  rb_iv_set(self, "@geometry", Qnil); 
  rb_iv_set(self, "@gravity",  Qnil);
  rb_iv_set(self, "@screen",   Qnil);
  rb_iv_set(self, "@flags",    Qnil);

  subSubtlextConnect(); ///< Implicit open connection

  return self;
} /* }}} */

/* subClientFind {{{ */
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
subClientFind(VALUE self,
  VALUE name)
{
  return subSubtlextFind(SUB_TYPE_CLIENT, name, True);
} /* }}} */

/* subClientCurrent {{{ */
/*
 * call-seq: current -> Subtlext::Client
 *
 * Get current active Client
 *
 *  Subtlext::Client.current
 *  => #<Subtlext::Client:xxx>
 */

VALUE
subClientCurrent(VALUE self)
{
  VALUE client = Qnil;
  unsigned long *focus = NULL;

  subSubtlextConnect(); ///< Implicit open connection

  if((focus = (unsigned long *)subSharedPropertyGet(display, 
      DefaultRootWindow(display), XA_WINDOW, 
      XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
    {
      client = subClientInstantiate(*focus);

      if(!NIL_P(client)) subClientUpdate(client);

      free(focus);
    }
  else rb_raise(rb_eStandardError, "Failed getting current client");

  return client;
} /* }}} */

/* subClientAll {{{ */
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
subClientAll(VALUE self)
{
  int i, size = 0;
  Window *clients = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, client = Qnil;

  subSubtlextConnect(); ///< Implicit open connection
  
  /* Fetch data */
  meth    = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("Client"));
  clients = subSharedClientList(&size);
  array   = rb_ary_new2(size);    

  /* Populate array */
  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          if(!NIL_P(client = rb_funcall(klass, meth, 1, LONG2NUM(clients[i]))))
            {
              subClientUpdate(client);
              rb_ary_push(array, client);
            }
        }

      free(clients);
    }

  return array;
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
  VALUE win = rb_iv_get(self, "@win");

  if(T_FIXNUM == rb_type(win) || T_BIGNUM == rb_type(win))
    {
      int id = 0;
      char buf[20] = { 0 };

      snprintf(buf, sizeof(buf), "%#lx", NUM2LONG(win));
      if(-1 != (id = subSharedClientFind(buf, NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS))))
        {
          int *flags = NULL;
          char *wmname = NULL, *wminstance = NULL, *wmclass = NULL, *role = NULL;

          subSharedPropertyClass(display, NUM2LONG(win), &wminstance, &wmclass);
          subSharedPropertyName(display, NUM2LONG(win), &wmname, wmclass);

          flags = (int *)subSharedPropertyGet(display, NUM2LONG(win), XA_CARDINAL,
            XInternAtom(display, "SUBTLE_WINDOW_FLAGS", False), NULL);
          role = subSharedPropertyGet(display, NUM2LONG(win), XA_STRING,
            XInternAtom(display, "WM_WINDOW_ROLE", False), NULL);

          /* Set properties */
          rb_iv_set(self, "@id",       INT2FIX(id));
          rb_iv_set(self, "@flags",    INT2FIX(*flags));
          rb_iv_set(self, "@name",     rb_str_new2(wmname));
          rb_iv_set(self, "@instance", rb_str_new2(wminstance));
          rb_iv_set(self, "@klass",    rb_str_new2(wmclass));
          rb_iv_set(self, "@role",     role ? rb_str_new2(role) : Qnil);

          /* Set to nil for on demand loading */
          rb_iv_set(self, "@geometry", Qnil);
          rb_iv_set(self, "@gravity",  Qnil);
          rb_iv_set(self, "@screen",   Qnil);

          free(flags);
          free(wmname);
          free(wminstance);
          free(wmclass);
          if(role) free(role);
        }
      else rb_raise(rb_eStandardError, "Failed finding client");  
    }
  else rb_raise(rb_eArgError, "Unknown value type");

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
  int i, size = 0;
  char **names = NULL;
  Window *views = NULL;
  VALUE win = Qnil, array = Qnil, method = Qnil, klass = Qnil;
  unsigned long *flags1 = NULL;
  
  win     = rb_iv_get(self, "@win");
  method  = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("View"));
  array   = rb_ary_new2(size);
  names   = subSharedPropertyStrings(display, DefaultRootWindow(display), 
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size);
  views   = (Window *)subSharedPropertyGet(display, DefaultRootWindow(display), 
    XA_WINDOW, XInternAtom(display, "_NET_VIRTUAL_ROOTS", False), NULL);
  flags1  = (unsigned long *)subSharedPropertyGet(display, NUM2LONG(win), 
    XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

  if(names && views)
    {
      for(i = 0; i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(display, 
            views[i], XA_CARDINAL, 
            XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

          if(*flags1 & *flags2) ///< Check if there are common tags
            {
              VALUE v = rb_funcall(klass, method, 1, rb_str_new2(names[i]));

              rb_iv_set(v, "@id",  INT2FIX(i));
              rb_iv_set(v, "@win", LONG2NUM(views[i]));
              rb_ary_push(array, v);
            }

          free(flags2);
        }

      XFreeStringList(names);
      free(views);
    }

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
 * Raise Client window
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
 * Raise Client window
 *
 *  client.raise
 *  => nil
 */

VALUE
subClientRestackLower(VALUE self)
{
  return ClientRestack(self, Below);
} /* }}} */

/* subClientMatchUp {{{ */
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
subClientMatchUp(VALUE self)
{
  return ClientMatch(self, SUB_WINDOW_UP);
} /* }}} */

/* subClientMatchLeft {{{ */
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
subClientMatchLeft(VALUE self)
{
  return ClientMatch(self, SUB_WINDOW_LEFT);
} /* }}} */

/* subClientMatchRight {{{ */
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
subClientMatchRight(VALUE self)
{
  return ClientMatch(self, SUB_WINDOW_RIGHT);
} /* }}} */

/* subClientMatchDown {{{ */
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
subClientMatchDown(VALUE self)
{
  return ClientMatch(self, SUB_WINDOW_DOWN);
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
  VALUE ret = Qfalse, name = rb_iv_get(self, "@name");

  /* Just find the client */
  if(RTEST(name) && -1 != subSharedClientFind(RSTRING_PTR(name), 
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
  Window win = None;
  VALUE gravity = Qnil;
  
  /* Load on demand */
  if(NIL_P((gravity = rb_iv_get(self, "@gravity"))) && 
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int *id = NULL;
      char buf[5] = { 0 };

      /* Get gravity */
      if((id = (int *)subSharedPropertyGet(display, win, XA_CARDINAL,
          XInternAtom(display, "SUBTLE_WINDOW_GRAVITY", False), NULL)))
        {
          snprintf(buf, sizeof(buf), "%d", *id);
          gravity = subGravityInstantiate(buf);

          if(!NIL_P(gravity)) subGravityUpdate(gravity);

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
  VALUE gravity = subSubtlextFind(SUB_TYPE_GRAVITY, value, True);
  
  /* Set gravity */
  if(Qnil != gravity)
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2LONG(rb_iv_get(self,    "@id"));
      data.l[1] = FIX2LONG(rb_iv_get(gravity, "@id"));

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_GRAVITY", data, True);

      rb_iv_set(self, "@gravity", gravity);
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return Qnil;
} /* }}} */

/* subClientScreenReader {{{ */
/*
 * call-seq: screen -> Subtlext::Screen
 *
 * Get screen Client is on as Screen object
 *
 *  client.screen
 *  => #<Subtlext::Screen:xxx>
 */

VALUE
subClientScreenReader(VALUE self)
{
  Window win = None;
  VALUE screen = Qnil;
  
  /* Load on demand */
  if(NIL_P((screen = rb_iv_get(self, "@screen"))) &&
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int *id = NULL;
      
      /* Collect data */
      id     = (int *)subSharedPropertyGet(display, win, XA_CARDINAL, 
        XInternAtom(display, "SUBTLE_WINDOW_SCREEN", False), NULL);
      screen = subScreenInstantiate(*id);

      if(!NIL_P(screen)) subScreenUpdate(screen);

      rb_iv_set(self, "@screen", screen);

      free(id);
    }

  return screen;
} /* }}} */

/* subClientScreenWriter {{{ */
/*
 * call-seq: screen=(screen) -> nil
 *
 * Set client screen
 *
 *  client.screen = 0
 *  => nil
 * 
 *  client.screen = subtle.find_screen(0)
 *  => nil
 */

VALUE
subClientScreenWriter(VALUE self,
  VALUE value)
{
  VALUE screen = subSubtlextFind(SUB_TYPE_SCREEN, value, True);

  /* Set screen */
  if(Qnil != screen)
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2LONG(rb_iv_get(self,   "@id"));
      data.l[1] = FIX2LONG(rb_iv_get(screen, "@id"));

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_SCREEN", data, True);

      rb_iv_set(self, "@screen", INT2FIX(screen));
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

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
  Window win = None;
  VALUE geom = Qnil;

  geom = rb_iv_get(self, "@geometry");
  win = NUM2LONG(rb_iv_get(self, "@win"));

  /* Load on demand */
  if(NIL_P((geom = rb_iv_get(self, "@geometry"))) &&
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      XRectangle geometry = { 0 };

      subSharedPropertyGeometry(display, win, &geometry);

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

      subSharedMessage(win, "_NET_MOVERESIZE_WINDOW", data, True);

      rb_iv_set(self, "@geometry", geometry);
    }

  return geometry;
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
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
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
  VALUE win = rb_iv_get(self, "@win");
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  if(RTEST(win))
    {
      data.l[0] = CurrentTime;
      data.l[1] = 2; ///< Claim to be a pager

      subSharedMessage(NUM2LONG(win), "_NET_CLOSE_WINDOW", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed killing client");

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
