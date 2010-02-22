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
  VALUE name)
{
  return subSubtlextFind(SUB_TYPE_VIEW, name, True);
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
  names = subSharedPropertyStrings(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
  cv    = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  views = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW, 
    "_NET_VIRTUAL_ROOTS", NULL);

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
  names = subSharedPropertyStrings(DefaultRootWindow(display), 
    "_NET_DESKTOP_NAMES", &size);
  views = (Window *)subSharedPropertyGet(DefaultRootWindow(display), 
    XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);  
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

  if(T_STRING == rb_type(name))
    {
      int id = -1;

      /* Create view if needed */
      if(-1 == (id = subSharedViewFind(RSTRING_PTR(name), NULL, NULL)))
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };

          snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
          subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data, True);    

          id = subSharedViewFind(RSTRING_PTR(name), NULL, NULL);
        }

      /* Guess view id */
      if(-1 == id)
        {
          int size = 0;
          char **names = NULL;

          names = subSharedPropertyStrings(DefaultRootWindow(display),
            "_NET_DESKTOP_NAMES", &size);

          id = size; ///< New id should be last

          XFreeStringList(names);
        }

      rb_iv_set(self, "@id", INT2FIX(id));
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
  unsigned long *flags1 = NULL;
  
  /* Fetch data */
  win     = rb_iv_get(self, "@win");
  klass   = rb_const_get(mod, rb_intern("Client"));
  meth    = rb_intern("new");
  array   = rb_ary_new2(size);
  clients = subSharedClientList(&size);
  flags1  = (unsigned long *)subSharedPropertyGet(NUM2LONG(win), XA_CARDINAL, 
    "SUBTLE_WINDOW_TAGS", NULL);

  /* Populate array */
  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
            "SUBTLE_WINDOW_TAGS", NULL);

          if(flags2 && *flags1 & *flags2) ///< Check if there are common tags
            {
              if(!NIL_P(client = rb_funcall(klass, meth, 1, LONG2NUM(clients[i]))))
                {
                  subClientUpdate(client);
                  rb_ary_push(array, client); 
                }
            }

          if(flags2) free(flags2);
        }

      free(clients);
    }

  free(flags1);

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

  data.l[0] = FIX2INT(id);

  subSharedMessage(DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", data, True);

  return Qnil;
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
  cv = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

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
  return subSubtlextKill(self, SUB_TYPE_VIEW);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
