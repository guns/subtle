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

/* Singleton */

/* subTagSingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Tag or nil
 *           [value]     -> Subtlext::Tag or nil
 *
 * Find Tag by a given value which can be of following type:
 *
 * [fixnum] Array id
 * [string] Match against name of Tag
 * [symbol] :all for an array of all Tag
 *
 *  Subtlext::Tag.find("subtle")
 *  => #<Subtlext::Tag:xxx>
 *
 *  Subtlext::Tag[1]
 *  => #<Subtlext::Tag:xxx>
 *
 *  Subtlext::Tag["subtle"]
 *  => nil
 */

VALUE
subTagSingFind(VALUE self,
  VALUE value)
{
  int id = 0;
  VALUE parsed = Qnil, tag = Qnil;
  char *name = NULL, buf[50] = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  if(T_SYMBOL == rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), NULL)))
    {
      if(CHAR2SYM("visible") == parsed)
        return subTagSingVisible(Qnil);
      else if(CHAR2SYM("all") == parsed)
        return subTagSingAll(Qnil);
    }

  /* Find tag */
  if(-1 != (id = subSubtlextFind("SUBTLE_TAG_LIST", buf, &name)))
    {
      if(!NIL_P((tag = subTagInstantiate(name))))
        rb_iv_set(tag, "@id", INT2FIX(id));

      free(name);
    }

  return tag;
} /* }}} */

/* subTagSingVisible {{{ */
/*
 * call-seq: visible -> Array
 *
 * Get Array of all visible Tag
 *
 *  Subtlext::Tag.visible
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 *
 *  Subtlext::Tag.visible
 *  => []
 */

VALUE
subTagSingVisible(VALUE self)
{
  int i, ntags = 0;
  char **tags = NULL;
  unsigned long *visible = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, t = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth    = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("Tag"));
  array   = rb_ary_new();
  tags    = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "SUBTLE_TAG_LIST", False), &ntags);
  visible = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL, XInternAtom(display,
    "SUBTLE_VISIBLE_TAGS", False), NULL);

  /* Populate array */
  if(tags && visible)
    {
      for(i = 0; i < ntags; i++)
        {
          /* Create tag on match */
          if(*visible & (1L << (i + 1)) &&
              !NIL_P(t = rb_funcall(klass, meth, 1, rb_str_new2(tags[i]))))
            {
              rb_iv_set(t, "@id", INT2FIX(i));
              rb_ary_push(array, t);
            }
        }

    }

  if(tags)    XFreeStringList(tags);
  if(visible) free(visible);

  return array;
} /* }}} */

/* subTagSingAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get Array of all Tag
 *
 *  Subtlext::Tag.all
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 *
 *  Subtlext::Tag.all
 *  => []
 */

VALUE
subTagSingAll(VALUE self)
{
  int i, ntags = 0;
  char **tags = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Tag"));
  array = rb_ary_new();

  /* Check results */
  if((tags = subSharedPropertyStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_TAG_LIST", False), &ntags)))
    {
      for(i = 0; i < ntags; i++)
        {
          VALUE t = rb_funcall(klass, meth, 1, rb_str_new2(tags[i]));

          rb_iv_set(t, "@id", INT2FIX(i));
          rb_ary_push(array, t);
        }

      XFreeStringList(tags);
    }

  return array;
} /* }}} */

/* Class */

/* subTagInstantiate {{{ */
VALUE
subTagInstantiate(char *name)
{
  VALUE klass = Qnil, tag = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("Tag"));
  tag   = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return tag;
} /* }}} */

/* subTagInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Tag
 *
 * Create new Tag object
 *
 *  tag = Subtlext::Tag.new("subtle")
 *  => #<Subtlext::Tag:xxx>
 */

VALUE
subTagInit(VALUE self,
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

/* subTagUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Tag properties
 *
 *  tag.update
 *  => nil
 */

VALUE
subTagUpdate(VALUE self)
{
  int id = -1;
  VALUE name = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", name);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Create tag if needed */
  if(-1 == (id = subSubtlextFind("SUBTLE_TAG_LIST",
      RSTRING_PTR(name), NULL)))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_TAG_NEW", data, 8, True);

      id = subSubtlextFind("SUBTLE_TAG_LIST", RSTRING_PTR(name), NULL);
    }

  /* Guess tag id */
  if(-1 == id)
    {
      int ntags = 0;
      char **tags = NULL;

      tags = subSharedPropertyStrings(display, DefaultRootWindow(display),
        XInternAtom(display, "SUBTLE_TAG_LIST", False), &ntags);

      id = ntags; ///< New id should be last

      if(tags) XFreeStringList(tags);
    }

  rb_iv_set(self, "@id", INT2FIX(id));

  return Qnil;
} /* }}} */

/* subTagClients {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get Array of Client that are tagged with this
 *
 *  tag.clients
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  tag.clients
 *  => []
 */

VALUE
subTagClients(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  unsigned long *tags = NULL;
  VALUE id = Qnil, array = Qnil, klass = Qnil, meth = Qnil, c = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Fetch data */
  klass   = rb_const_get(mod, rb_intern("Client"));
  meth    = rb_intern("new");
  array   = rb_ary_new();
  clients = subSubtlextList("_NET_CLIENT_LIST", &nclients);

  /* Check results */
  if(clients)
    {
      for(i = 0; i < nclients; i++)
        {
          if((tags = (unsigned long *)subSharedPropertyGet(display,
              clients[i], XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_TAGS",
              False), NULL)))
            {
              /* Check if tag id matches */
              if(*tags & (1L << (FIX2INT(id) + 1)))
                {
                  /* Create new client */
                  if(!NIL_P(c = rb_funcall(klass, meth, 1, INT2FIX(i))))
                    {
                      rb_iv_set(c, "@win", LONG2NUM(clients[i]));

                      subClientUpdate(c);
                      rb_ary_push(array, c);
                    }
                }
            }
        }

      free(clients);
    }

  return array;
} /* }}} */

/* subTagViews {{{ */
/*
 * call-seq: views -> Array
 *
 * Get Array of View that are tagged with this
 *
 *  tag.views
 *  => [#<Subtlext::Views:xxx>, #<Subtlext::Views:xxx>]
 *
 *  tag.views
 *  => []
 */

VALUE
subTagViews(VALUE self)
{
  int i, nnames = 0;
  char **names = NULL;
  unsigned long *tags = NULL;
  VALUE id = Qnil, array = Qnil, klass = Qnil, meth = Qnil, v = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  klass  = rb_const_get(mod, rb_intern("View"));
  meth   = rb_intern("new");
  array  = rb_ary_new();
  names  = subSharedPropertyStrings(display, DefaultRootWindow(display),
    XInternAtom(display, "_NET_DESKTOP_NAMES", False), &nnames);
  tags   = (unsigned long *)subSharedPropertyGet(display,
    DefaultRootWindow(display), XA_CARDINAL,
    XInternAtom(display, "SUBTLE_VIEW_TAGS", False), NULL);

  /* Check results */
  if(names && tags)
    {
      for(i = 0; i < nnames; i++)
        {
          /* Check if tag id matches */
          if(tags[i] & (1L << (FIX2INT(id) + 1)))
            {
              /* Create new view */
              if(!NIL_P(v = rb_funcall(klass, meth, 1, rb_str_new2(names[i]))))
                {
                  rb_iv_set(v, "@id",  INT2FIX(i));
                  rb_ary_push(array, v);
                }
            }
        }
    }

  if(names) XFreeStringList(names);
  if(tags)  free(tags);

  return array;
} /* }}} */

/* subTagToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Tag object to String
 *
 *  puts tag
 *  => "subtle"
 */

VALUE
subTagToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subTagKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a Tag
 *
 *  tag.kill
 *  => nil
 */

VALUE
subTagKill(VALUE self)
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
    "SUBTLE_TAG_KILL", data, 32, True);

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
