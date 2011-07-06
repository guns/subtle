 /**
  * @package subtlext
  *
  * @file Gravity functions
  * @copyright (c) 2005-2011 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPLv2.
  * See the file COPYING for details.
  **/

#include "subtlext.h"

/* GravityToRect {{{ */
void
GravityToRect(VALUE self,
  XRectangle *r)
{
  VALUE geometry = rb_iv_get(self, "@geometry");

  subGeometryToRect(geometry, r); ///< Get values
} /* }}} */

/* GravityFind {{{ */
static VALUE
GravityFind(char *source,
  int flags)
{
  int ngravities = 0;
  char **gravities = NULL;
  VALUE ret = Qnil;

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Get gravity list */
  if((gravities = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_GRAVITY_LIST", False), &ngravities)))
    {
      int i, selid = -1;
      XRectangle geometry = { 0 };
      char buf[30] = { 0 };
      VALUE klass_grav = Qnil, klass_geom = Qnil, meth = Qnil;
      VALUE gravity = Qnil, geom = Qnil;
      regex_t *preg = NULL;

      /* Fetch data */
      klass_grav = rb_const_get(mod, rb_intern("Gravity"));
      klass_geom = rb_const_get(mod, rb_intern("Geometry"));
      meth       = rb_intern("new");

      /* Create if source is given */
      if(source)
        {
          if(isdigit(source[0])) selid = atoi(source);
          preg = subSharedRegexNew(source);
        }

      /* Create gravity list */
      for(i = 0; i < ngravities; i++)
        {
          sscanf(gravities[i], "%hdx%hd+%hd+%hd#%s", &geometry.x, &geometry.y,
            &geometry.width, &geometry.height, buf);

          /* Check if gravity matches */
          if(!source || (source && (selid == i || (-1 == selid &&
              ((flags & SUB_MATCH_EXACT && 0 == strcmp(source, buf)) ||
              (preg && !(flags & SUB_MATCH_EXACT) &&
                subSharedRegexMatch(preg, buf)))))))
            {
              /* Create new gravity */
              gravity = rb_funcall(klass_grav, meth, 1, rb_str_new2(buf));
              geom    = rb_funcall(klass_geom, meth, 4, INT2FIX(geometry.x),
                INT2FIX(geometry.y), INT2FIX(geometry.width),
                INT2FIX(geometry.height));

              rb_iv_set(gravity, "@id",       INT2FIX(i));
              rb_iv_set(gravity, "@geometry", geom);

              ret = subSubtlextOneOrMany(gravity, ret);
            }
        }

      if(preg)    subSharedRegexKill(preg);
      XFreeStringList(gravities);
    }
  else rb_raise(rb_eStandardError, "Failed getting gravity list");

  return NIL_P(ret) ? rb_ary_new() : ret;
} /* }}} */

/* GravityFindId {{{ */
static int
GravityFindId(char *match,
  char **name,
  XRectangle *geometry)
{
  int ret = -1, ngravities = 0;
  char **gravities = NULL;
  regex_t *preg = NULL;

  assert(match);

  /* Find gravity id */
  if((preg = subSharedRegexNew(match)) &&
      (gravities = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_GRAVITY_LIST", False), &ngravities)))
    {
      int i;
      XRectangle geom = { 0 };
      char buf[30] = { 0 };

      for(i = 0; i < ngravities; i++)
        {
          sscanf(gravities[i], "%hdx%hd+%hd+%hd#%s", &geom.x, &geom.y,
            &geom.width, &geom.height, buf);

          /* Check id and name */
          if((isdigit(match[0]) && atoi(match) == i) ||
              (!isdigit(match[0]) && subSharedRegexMatch(preg, buf)))
            {
              subSharedLogDebugSubtlext("Found: type=gravity, name=%s, id=%d\n", buf, i);

              if(geometry) *geometry = geom;
              if(name)
                {
                  *name = (char *)subSharedMemoryAlloc(strlen(buf) + 1, sizeof(char));
                  strncpy(*name, buf, strlen(buf));
                }

              ret = i;
              break;
           }
       }
    }
  else subSharedLogDebugSubtlext("Failed finding gravity `%s'\n", name);

  if(preg)       subSharedRegexKill(preg);
  if(gravities) XFreeStringList(gravities);

  return ret;
} /* }}} */

/* Singleton */

/* subGravitySingFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Gravity, Array or nil
 *           [value]     -> Subtlext::Gravity, Array or nil
 *
 * Find Gravity by a given <i>value</i> which can be of following type:
 *
 * [Fixnum] Array index of the <code>SUBTLE_GRAVITY_LIST</code> property list.
 * [String] Regexp match against name of Gravities, returns a Gravity on single
 *          match or an Array on multiple matches.
 * [Symbol] Either <i>:all</i> for an array of all Views or any string for an
 *          <b>exact</b> match.
 *
 *  Subtlext::Gravity.find(1)
 *  => #<Subtlext::Gravity:xxx>
 *
 *  Subtlext::Gravity.find("subtle")
 *  => #<Subtlext::Gravity:xxx>
 *
 *  Subtlext::Gravity[".*"]
 *  => [#<Subtlext::Gravity:xxx>, #<Subtlext::Gravity:xxx>]
 *
 *  Subtlext::Gravity["subtle"]
 *  => nil
 *
 *  Subtlext::Gravity[:center]
 *  => #<Subtlext::Gravity:xxx>

 */

VALUE
subGravitySingFind(VALUE self,
  VALUE value)
{
  int flags = 0;
  VALUE parsed = Qnil;
  char buf[50] = { 0 };

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Check object type */
  switch(rb_type(parsed = subSubtlextParse(
      value, buf, sizeof(buf), &flags)))
    {
      case T_SYMBOL:
        if(CHAR2SYM("all") == parsed)
          return subGravitySingAll(Qnil);
        break;
      case T_OBJECT:
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Gravity"))))
          return parsed;
    }

  return GravityFind(buf, flags);
} /* }}} */

/* subGravitySingAll {{{ */
/*
 * Get an array of all Gravites based on the <code>SUBTLE_GRAVITIY_LIST</code>
 * property list.
 *
 *  Subtlext::Gravity.all
 *  => [#<Subtlext::Gravity:xxx>, #<Subtlext::Gravity:xxx>]
 *
 *  Subtlext::Gravity.all
 *  => []
 */

VALUE
subGravitySingAll(VALUE self)
{
  return GravityFind(NULL, 0);
} /* }}} */

/* Class */

/* subGravityInstantiate {{{ */
VALUE
subGravityInstantiate(char *name)
{
  VALUE klass = Qnil, gravity = Qnil;

  /* Create new instance */
  klass   = rb_const_get(mod, rb_intern("Gravity"));
  gravity = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return gravity;
} /* }}} */

/* subGravityInit {{{ */
/*
 * call-seq: new(name, gravity) -> Subtlext::Gravity
 *
 * Create a new Gravity object locally <b>without</b> calling #save automatically.
 *
 * The Gravity <b>won't</b> be useable until #save is called.
 *
 *  gravity = Subtlext::Gravity.new("top")
 *  => #<Subtlext::Gravity:xxx>
 */

VALUE
subGravityInit(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE data[2] = { Qnil };

  rb_scan_args(argc, argv, "02", &data[0], &data[1]);

  if(T_STRING != rb_type(data[0]))
    rb_raise(rb_eArgError, "Invalid value type");

  /* Init object */
  rb_iv_set(self, "@id",       Qnil);
  rb_iv_set(self, "@name",     data[0]);
  rb_iv_set(self, "@geometry", data[1]);

  subSubtlextConnect(NULL); ///< Implicit open connection

  return self;
} /* }}} */

/* subGravityUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Gravity properties based on <b>required</b> Gravity index.
 *
 *  gravity.update
 *  => nil
 */

VALUE
subGravityUpdate(VALUE self)
{
  int id = -1;
  XRectangle geom = { 0 };
  char *name = NULL;
  VALUE match = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", match);

  /* Find gravity */
  if(-1 == (id = GravityFindId(RSTRING_PTR(match), &name, &geom)))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      VALUE geometry = rb_iv_get(self, "@geometry");

      if(NIL_P(geometry = rb_iv_get(self, "@geometry")))
        rb_raise(rb_eStandardError, "No geometry given");

      subGeometryToRect(geometry, &geom); ///< Get values

      /* Create new gravity */
      snprintf(data.b, sizeof(data.b), "%hdx%hd+%hd+%hd#%s",
        geom.x, geom.y, geom.width, geom.height, RSTRING_PTR(match));
      subSharedMessage(display, DefaultRootWindow(display),
        "SUBTLE_GRAVITY_NEW", data, 8, True);

      id = GravityFindId(RSTRING_PTR(match), NULL, NULL);
    }
  else ///< Update gravity
    {
      VALUE geometry = Qnil;

      geometry = subGeometryInstantiate(geom.x, geom.y,
        geom.width, geom.height);

      rb_iv_set(self, "@name",    rb_str_new2(name));
      rb_iv_set(self, "@gravity", geometry);

      free(name);
    }

  /* Guess gravity id */
  if(-1 == id)
    {
      int ngravities = 0;
      char **gravities = NULL;

      gravities = subSharedPropertyGetStrings(display, DefaultRootWindow(display),
        XInternAtom(display, "SUBTLE_GRAVITY_LIST", False), &ngravities);

      id = ngravities; ///< New id should be last

      XFreeStringList(gravities);
    }

  rb_iv_set(self, "@id", INT2FIX(id));

  return Qnil;
} /* }}} */

/* subGravityClients {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get an array of Clients that have this Gravity.
 *
 *  gravity.clients
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  tag.clients
 *  => []
 */

VALUE
subGravityClients(VALUE self)
{
  int i, nclients = 0;
  Window *clients = NULL;
  VALUE id = Qnil, klass = Qnil, meth = Qnil, array = Qnil, c = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  subSubtlextConnect(NULL); ///< Implicit open connection

  /* Fetch data */
  klass   = rb_const_get(mod, rb_intern("Client"));
  meth    = rb_intern("new");
  array   = rb_ary_new();
  clients = subSubtlextWindowList("_NET_CLIENT_LIST", &nclients);

  /* Check results */
  if(clients)
    {
      for(i = 0; i < nclients; i++)
        {
          unsigned long *gravity = NULL;

          /* Get window gravity */
          gravity = (unsigned long *)subSharedPropertyGet(display,
            clients[i], XA_CARDINAL, XInternAtom(display,
            "SUBTLE_CLIENT_GRAVITY", False), NULL);

          /* Check if there are common tags or window is stick */
          if(gravity && FIX2INT(id) == *gravity &&
              !NIL_P(c = rb_funcall(klass, meth, 1, INT2FIX(i))))
            {
              rb_iv_set(c, "@win", LONG2NUM(clients[i]));

              subClientUpdate(c);

              rb_ary_push(array, c);
            }

          if(gravity) free(gravity);
        }

      free(clients);
    }

  return array;
} /* }}} */

/* subGravityGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get the Gravity Geometry
 *
 *  gravity.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGravityGeometryReader(VALUE self)
{
  VALUE geometry = Qnil, name = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@name", name);

  /* Load on demand */
  if(NIL_P((geometry = rb_iv_get(self, "@geometry"))))
    {
      XRectangle geom = { 0 };

      GravityFindId(RSTRING_PTR(name), NULL, &geom);

      geometry = subGeometryInstantiate(geom.x, geom.y,
        geom.width, geom.height);
      rb_iv_set(self, "@geometry", geometry);
    }

  return geometry;
} /* }}} */

/* subGravityGeometryWriter {{{ */
/*
 * call-seq: geometry=(geometry) -> nil
 *
 * Set the Gravity Geometry
 *
 *  gravity.geometry=geometry
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGravityGeometryWriter(VALUE self,
  VALUE value)
{
  /* Check value type */
  if(T_OBJECT == rb_type(value))
    {
      VALUE klass = rb_const_get(mod, rb_intern("Geometry"));

      /* Check object instance */
      if(rb_obj_is_instance_of(value, klass))
        {
          rb_iv_set(self, "@geometry", value);
        }
      else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  return Qnil;
} /* }}} */

/* subGravityGeometryFor {{{ */
/*
 * call-seq: geometry_for(screen) -> nil
 *
 * Get the Gravity Geometry for given Screen in pixel values.
 *
 *  gravity.geometry_for(screen)
 *  => #<Subtlext::Geometry:xxx>
 */

VALUE
subGravityGeometryFor(VALUE self,
  VALUE value)
{
  VALUE ary = rb_ary_new2(4);

  /* Check value type */
  if(T_OBJECT == rb_type(value))
    {
      VALUE klass = rb_const_get(mod, rb_intern("Screen"));

      /* Check object instance */
      if(rb_obj_is_instance_of(value, klass))
        {
          XRectangle real = { 0 }, geom_grav = { 0 }, geom_screen = { 0 };

          GravityToRect(self,  &geom_grav);
          GravityToRect(value, &geom_screen);

          /* Calculate real values for screen */
          real.width  = geom_screen.width * geom_grav.width / 100;
          real.height = geom_screen.height * geom_grav.height / 100;
          real.x      = geom_screen.x +
            (geom_screen.width - real.width) * geom_grav.x / 100;
          real.y      = geom_screen.y +
            (geom_screen.height - real.height) * geom_grav.y / 100;

          /* Fill into result array */
          rb_ary_push(ary, INT2FIX(real.x));
          rb_ary_push(ary, INT2FIX(real.y));
          rb_ary_push(ary, INT2FIX(real.width));
          rb_ary_push(ary, INT2FIX(real.height));
        }
      else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
        rb_obj_classname(value));
    }

  return ary;
} /* }}} */

/* subGravityToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert this Gravity object to string.
 *
 *  puts gravity
 *  => "TopLeft"
 */

VALUE
subGravityToString(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return name;
} /* }}} */

/* subGravityToSym {{{ */
/*
 * call-seq: to_sym -> Symbol
 *
 * Convert this Gravity object to symbol.
 *
 *  puts gravity.to_sym
 *  => :center
 */

VALUE
subGravityToSym(VALUE self)
{
  VALUE name = Qnil;

  /* Check ruby object */
  GET_ATTR(self, "@name", name);

  return CHAR2SYM(RSTRING_PTR(name));
} /* }}} */

/* subGravityKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Remove this Gravity from subtle and <b>freeze</b> this object.
 *
 *  gravity.kill
 *  => nil
 */

VALUE
subGravityKill(VALUE self)
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
    "SUBTLE_GRAVITY_KILL", data, 32, True);

  rb_obj_freeze(self); ///< Freeze object

  return Qnil;
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
