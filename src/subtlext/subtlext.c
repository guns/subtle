
 /**
  * @package subtlext
  *
  * @file Main functions
  * @copyright (c) 2005-2010 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include "subtlext.h"

Display *display = NULL;
VALUE mod = Qnil;

static const char *klasses[] = { 
  "Client", "Gravity", "View", "Tag", "Tray", "Screen", "Sublet" 
};

#ifdef DEBUG
int debug = 0;
#endif /* DEBUG */

/* subTag {{{ */
VALUE
subTag(VALUE self,
  VALUE value,
  char *action)
{
  VALUE tag = Qnil;

  /* Find tag */
  if(Qnil != (tag = subSubtlextFind(SUB_TYPE_TAG, value, True)))
    {
      VALUE oid = Qnil, tid = Qnil;

      oid   = rb_iv_get(self, "@id");
      tid   = rb_iv_get(tag, "@id");

      if(Qnil != oid && Qnil != tid)
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };

          data.l[0] = FIX2LONG(oid);
          data.l[1] = FIX2LONG(tid);
          data.l[2] = rb_obj_is_instance_of(self, 
            rb_const_get(mod, rb_intern("Client"))) ? 0 : 1; ///< Client = 0, View = 1

          subSharedMessage(DefaultRootWindow(display), action, data, True);
        }
    }

  return Qnil;
} /* }}} */

/* SubtlextSweep {{{ */
static void
SubtlextSweep(void)
{
  if(display) 
    {
      subSharedLogDebug("Connection closed (%s)\n", DisplayString(display));

      XCloseDisplay(display);

      display = NULL;
    }
} /* }}} */

/* subSubtlextConnect {{{ */
void
subSubtlextConnect(void)
{
  /* Open display */
  if(!display)
    {
      if(!(display = XOpenDisplay(NULL)))
        {
          rb_raise(rb_eStandardError, "Failed opening display `%s'", 
            DisplayString(display));
        }

      XSetErrorHandler(subSharedLogXError);

      /* Check if subtle is running */
      if(True != subSharedSubtleRunning())
        {
          XCloseDisplay(display);
          display = NULL;
          
          rb_raise(rb_eStandardError, "Failed finding running %s", PKG_NAME);

          return;
        }

      /* Register sweeper */
      atexit(SubtlextSweep);

      subSharedLogDebug("Connection opened (%s)\n", DisplayString(display));
    }
} /* }}} */

/* subSubtlextFind {{{ */
VALUE
subSubtlextFind(int type,
  VALUE value,
  int exception)
{
  int id = 0, flags = 0;
  Window win = None;
  char *name = NULL, buf[50] = { 0 };
  XRectangle geometry = { 0 };
  VALUE object = Qnil;

  subSubtlextConnect(); ///< Implicit open connection

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM: /* {{{ */
        id = FIX2INT(value);
        snprintf(buf, sizeof(buf), "%d", id);
        break; /* }}} */
      case T_HASH: /* {{{ */
          {
            int i;
            VALUE meth_has_key = rb_intern("has_key?"), meth_fetch = rb_intern("fetch"), match = Qnil;

            struct properties 
            {
              VALUE sym;
              int   flags;
            } props[] = {
              /* Only clients use all flags */
              { CHAR2SYM("name"),     SUB_MATCH_NAME     },
              { CHAR2SYM("instance"), SUB_MATCH_INSTANCE },
              { CHAR2SYM("class"),    SUB_MATCH_CLASS    },
              { CHAR2SYM("gravity"),  SUB_MATCH_GRAVITY  },
              { CHAR2SYM("role"),     SUB_MATCH_ROLE     }
            };

            /* Check hash keys */
            for(i = 0; LENGTH(props) > i; i++)
              {
                if(Qtrue == rb_funcall(value, meth_has_key, 1, props[i].sym))
                  {
                    match = rb_funcall(value, meth_fetch, 1, props[i].sym);
                    flags = props[i].flags;
                    break;
                  }
              }

            /* Check object type */
            switch(rb_type(match))
              {
                case T_STRING:
                  snprintf(buf, sizeof(buf), "%s", RSTRING_PTR(match));
                  break;
                case T_SYMBOL:
                  snprintf(buf, sizeof(buf), "%s", SYM2CHAR(match));
                  break;
              }
          }
        break; /* }}} */        
      case T_OBJECT: /* {{{ */
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern(klasses[type])))) 
          object = value;
        break; /* }}} */
      case T_STRING: /* {{{ */
        if(SUB_TYPE_CLIENT == type || SUB_TYPE_TRAY == type) ///< Set default match flags
          flags = (SUB_MATCH_NAME|SUB_MATCH_CLASS);

        snprintf(buf, sizeof(buf), "%s", RSTRING_PTR(value));
        break; /* }}} */
      case T_SYMBOL: /* {{{ */
        if(CHAR2SYM("current") == value)
          {
            switch(type)
              {
                case SUB_TYPE_CLIENT: return subClientCurrent(Qnil); break;
                case SUB_TYPE_SCREEN: return subScreenCurrent(Qnil); break;
                case SUB_TYPE_VIEW:   return subViewCurrent(Qnil);   break;
              }
          }
        else if(CHAR2SYM("all") == value)
          {
            switch(type)
              {
                case SUB_TYPE_CLIENT:  return subClientAll(Qnil);  break;
                case SUB_TYPE_GRAVITY: return subGravityAll(Qnil); break;
                case SUB_TYPE_TAG:     return subTagAll(Qnil);     break;
                case SUB_TYPE_SCREEN:  return subScreenAll(Qnil);  break;
                case SUB_TYPE_SUBLET:  return subSubletAll(Qnil);  break;
                case SUB_TYPE_VIEW:    return subViewAll(Qnil);    break;
              }          
          }
        else if(SUB_TYPE_GRAVITY == type)
          snprintf(buf, sizeof(buf), "%s", SYM2CHAR(value));
        break; /* }}} */
      default: /* {{{ */
        if(exception)
          {
            rb_raise(rb_eArgError, "Unknwon value type `%s'", rb_obj_classname(value));

            return Qnil;
          } /* }}} */
    }

  /* Instantiate objects */
  switch(type)
    {
      case SUB_TYPE_CLIENT: /* {{{ */
        if(-1 != (id = subSharedClientFind(buf, NULL, &win, flags)))
          {
            if(Qnil != (object = subClientInstantiate(win)))
              {
                rb_iv_set(object, "@id", FIX2INT(id)); 
                
                subClientUpdate(object);
              }
          }
        break; /* }}} */
      case SUB_TYPE_GRAVITY: /* {{{ */
        if(-1 != (id = subSharedGravityFind(buf, &name, &geometry)))
          {
            if(Qnil != (object = subGravityInstantiate(name)))
              {
                VALUE geom = subGeometryInstantiate(geometry.x, geometry.y, 
                  geometry.width, geometry.height);

                rb_iv_set(object, "@id",       INT2FIX(id));
                rb_iv_set(object, "@geometry", geom);
              }
            
            free(name);
          }
        break; /* }}} */     
      case SUB_TYPE_TAG: /* {{{ */
        if(-1 != (id = subSharedTagFind(buf, &name)))
          {
            if(Qnil != (object = subTagInstantiate(name)))
              rb_iv_set(object, "@id", FIX2INT(id)); 
            
            free(name);
          }
        break; /* }}} */
      case SUB_TYPE_TRAY: /* {{{ */
        if(-1 != (id = subSharedTrayFind(buf, NULL, &win, flags)))
          {
            if(Qnil != (object = subTrayInstantiate(win)))
              {
                rb_iv_set(object, "@id", INT2FIX(id)); 

                subTrayUpdate(object);
              }
          }
        break; /* }}} */        
      case SUB_TYPE_SCREEN: /* {{{ */
        if(-1 != (id = subSharedScreenFind(id, &geometry)))
          {
            if(Qnil != (object = subScreenInstantiate(id)))
              {
                VALUE geom = subGeometryInstantiate(geometry.x, geometry.y, 
                  geometry.width, geometry.height);

                rb_iv_set(object, "@geometry", geom);
              }
          }
        break; /* }}} */
      case SUB_TYPE_SUBLET: /* {{{ */
        if(-1 != (id = subSharedSubletFind(buf, &name)))
          {
            if(Qnil != (object = subSubletInstantiate(name)))
              rb_iv_set(object, "@id", INT2FIX(id)); 
            
            free(name);
          }
        break; /* }}} */
      case SUB_TYPE_VIEW: /* {{{ */
        if(-1 != (id = subSharedViewFind(buf, &name, &win)))
          {
            if(Qnil != (object = subViewInstantiate(name)))
              {
                rb_iv_set(object, "@id",  INT2FIX(id)); 
                rb_iv_set(object, "@win", LONG2NUM(win)); 
              }

            free(name);
          }
        break; /* }}} */
    }

  if(NIL_P(object) && exception)
    rb_raise(rb_eArgError, "Failed finding `%s'", klasses[type]);

  if(!NIL_P(object)) rb_iv_set(object, "@id", INT2FIX(id)); ///< Set id

  return object;
} /* }}} */

/* subSubtlextKill {{{ */
VALUE
subSubtlextKill(VALUE value,
  int type)
{
  int id = -1;

  subSubtlextConnect(); ///< Implicit open connection

  /* Check object type */
  switch(rb_type(value))
    {
      case T_OBJECT: id = FIX2INT(rb_iv_get(value, "@id")); break;
      case T_STRING:
        {
          VALUE object = Qnil;

          if(RTEST((object = subSubtlextFind(type, value, False))))
            id = FIX2INT(rb_iv_get(object, "@id"));
        }
    }

  /* Send message */
  if(-1 != id)
    {
      int i = 0;
      char buf[20] = { 0 };
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = id;

      /* Create message type */
      snprintf(buf, sizeof(buf), "SUBTLE_%s_KILL", klasses[type]);
      for(i = 6; i < strlen(buf); i++) buf[i] = toupper(buf[i]);

      printf("%s\n", buf);

      subSharedMessage(DefaultRootWindow(display), buf, data, True);
    }
  else rb_raise(rb_eStandardError, "Failed killing %s", klasses[type]);

  return Qnil;
} /* }}} */

/* subSubtlextTagAdd {{{ */
/*
 * call-seq: tag(value) -> Subtlext::Tag
 *           +(value)   -> Subtlext::Tag
 *
 * Add a Tag to Client or View
 *
 *  client.tag("subtle")
 *  => #<Subtlext::Tag:xxx>
 *
 *  view.tag("subtle")
 *  => #<Subtlext::Tag:xxx>
 *
 *  client + "subtle"
 *  => #<Subtlext::Tag:xxx>
 */

VALUE
subSubtlextTagAdd(VALUE self,
  VALUE value)
{
  return subTag(self, value, "SUBTLE_WINDOW_TAG");
} /* }}} */

/* subSubtlextTagDel {{{ */
/*
 * call-seq: untag(value) -> Subtlext::Tag
 *           -(value)     -> Subtlext::Tag
 *
 * Remove a Tag from Client or View
 *
 *  client.untag("subtle")
 *  => #<Subtlext::Tag:xxx>
 *
 *  view.untag("subtle")
 *  => #<Subtlext::Tag:xxx>
 *
 *  client - "subtle"
 *  => #<Subtlext::Tag:xxx>
 */

VALUE
subSubtlextTagDel(VALUE self,
  VALUE value)
{
  return subTag(self, value, "SUBTLE_WINDOW_UNTAG");
} /* }}} */

/* subSubtlextTagList {{{ */
VALUE
subSubtlextTagList(VALUE self)
{
  Window win = None;
  VALUE array = rb_ary_new();

  /* Get win */
  if(None != (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int i, size = 0;
      char **tags = NULL;
      unsigned long *flags = NULL;
      VALUE method = Qnil, klass = Qnil, t = Qnil;
    
      method = rb_intern("new");
      klass  = rb_const_get(mod, rb_intern("Tag"));
      flags  = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_WINDOW_TAGS", NULL);
      tags   = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);

      /* Build tag array */
      for(i = 0; i < size; i++)
        {
          if((int)*flags & (1L << (i + 1)))
            {
              /* Create new tag */
              t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));
              rb_iv_set(t, "@id", INT2FIX(i));
              rb_ary_push(array, t);
            }
        }

      XFreeStringList(tags);
      free(flags);
    }

  return array;
} /* }}} */

/* subSubtlextTagAssoc {{{ */
VALUE
subSubtlextTagAssoc(VALUE self,
  int type)
{
  int i, id = 0, size = 0;
  char **names = NULL;
  Window *wins = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil, object = Qnil;
  
  /* Collect data */
  id      = FIX2INT(rb_iv_get(self, "@id"));
  method  = rb_intern("new");
  array   = rb_ary_new();

  /* Check object type */
  switch(type)
    {
      case SUB_TYPE_CLIENT:
        klass = rb_const_get(mod, rb_intern("Client"));
        wins  = subSharedClientList(&size);
        break;
      case SUB_TYPE_VIEW:
        klass = rb_const_get(mod, rb_intern("View"));
        names = subSharedPropertyStrings(DefaultRootWindow(display), 
          "_NET_DESKTOP_NAMES", &size);
        wins  = (Window *)subSharedPropertyGet(DefaultRootWindow(display), 
          XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);
        break;
    }

  /* Populate array */
  for(i = 0; i < size; i++)
    {
      unsigned long *flags = (unsigned long *)subSharedPropertyGet(wins[i], XA_CARDINAL, 
        "SUBTLE_WINDOW_TAGS", NULL);

      if((int)*flags & (1L << (id + 1))) ///< Check if tag id matches
        {
          /* Instantiate objects */
          switch(type)
            {
              case SUB_TYPE_CLIENT:
                object = rb_funcall(klass, method, 1, LONG2NUM(wins[i]));

                if(!NIL_P(object)) subClientUpdate(object); ///< Init client
                break;
              case SUB_TYPE_VIEW:
                object = rb_funcall(klass, method, 1, rb_str_new2(names[i]));

                rb_iv_set(object, "@win", LONG2NUM(wins[i]));
                break;
            }

          rb_iv_set(object, "@id", INT2FIX(i));
          rb_ary_push(array, object);
        }

      free(flags);
    }

  if(names) XFreeStringList(names);
  free(wins);

  return array;
} /* }}} */

/* subSubtlextTagAsk {{{ */
VALUE
subSubtlextTagAsk(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil, ret = Qfalse;

  /* Find tag */
  if(Qnil != (tag = subSubtlextFind(SUB_TYPE_TAG, value, False)))
    {
      int id = 0;
      Window win = 0;
      unsigned long *tags = NULL;

      win = NUM2LONG(rb_iv_get(self, "@win"));
      id  = FIX2INT(rb_iv_get(tag, "@id"));

      if((tags = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, 
          "SUBTLE_WINDOW_TAGS", NULL)))
        {
          if(*tags & (1L << (id + 1)))
            ret = Qtrue;
          
          free(tags);
        }
    }

  return ret;
} /* }}} */

/* subSubtlextFocus {{{ */
VALUE
subSubtlextFocus(VALUE self)
{
  VALUE win = Qnil;

  /* Get win */
  if(Qnil != (win = rb_iv_get(self, "@win")))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = NUM2LONG(win);

      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed setting focus");

  return Qnil;
} /* }}} */

/* subSubtlextFocusAsk {{{ */
VALUE
subSubtlextFocusAsk(VALUE self)
{
  VALUE ret = Qfalse, win = Qnil;
  unsigned long *focus = NULL;

  /* Fetch data */
  win   = rb_iv_get(self, "@win");
  focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL);

  /* Check if win has focus */
  if(RTEST(win) && focus)
    {
      if(*focus == NUM2LONG(win)) ret = Qtrue;
      free(focus);
    }

  return ret;
} /* }}} */

/* Plugin */

/* Init_subtlext {{{ */
/*
 * Subtlext is the module of the extension
 */
void
Init_subtlext(void)
{
  VALUE client = Qnil, geometry = Qnil, gravity = Qnil, screen = Qnil;
  VALUE subtle = Qnil, sublet = Qnil, tag = Qnil, tray = Qnil, view = Qnil;
  VALUE window = Qnil;

  /*
   * Document-class: Subtlext
   *
   * Subtlext is the toplevel module
   */

  mod = rb_define_module("Subtlext");

  /* Subtlext version */
  rb_define_const(mod, "VERSION", rb_str_new2(PKG_VERSION));

  /*
   * Document-class: Subtlext::Client
   *
   * Class for interaction with clients
   */

  client = rb_define_class_under(mod, "Client", rb_cObject);

  /* Client id */
  rb_define_attr(client, "id",       1, 0); 

  /* Window id */
  rb_define_attr(client, "win",      1, 0);

  /* WM_NAME */
  rb_define_attr(client, "name",     1, 0);

  /* Instance of WM_CLASS */
  rb_define_attr(client, "instance", 1, 0);

  /* Class of WM_CLASS */
  rb_define_attr(client, "klass",    1, 0);

    /* Window role */
  rb_define_attr(client, "role",     1, 0);

  /* Bitfield of window states */
  rb_define_attr(client, "flags",    1, 0);

  rb_define_singleton_method(client, "find",    subClientFind,    1);
  rb_define_singleton_method(client, "[]",      subClientFind,    1);
  rb_define_singleton_method(client, "current", subClientCurrent, 0);
  rb_define_singleton_method(client, "all",     subClientAll,     0);

  rb_define_method(client, "initialize",   subClientInit,            1);
  rb_define_method(client, "update",       subClientUpdate,          0);

  rb_define_method(client, "tags",         subSubtlextTagList,         0);
  rb_define_method(client, "has_tag?",     subSubtlextTagAsk,          1);
  rb_define_method(client, "tag",          subSubtlextTagAdd,          1);
  rb_define_method(client, "untag",        subSubtlextTagDel,          1);
  rb_define_method(client, "+",            subSubtlextTagAdd,          1);
  rb_define_method(client, "-",            subSubtlextTagDel,          1);

  rb_define_method(client, "views",        subClientViewList,        0);
  rb_define_method(client, "toggle_full",  subClientToggleFull,      0);
  rb_define_method(client, "toggle_float", subClientToggleFloat,     0);
  rb_define_method(client, "toggle_stick", subClientToggleStick,     0);
  rb_define_method(client, "is_full?",     subClientStateFullAsk,    0);
  rb_define_method(client, "is_float?",    subClientStateFloatAsk,   0);
  rb_define_method(client, "is_stick?",    subClientStateStickAsk,   0);
  rb_define_method(client, "raise",        subClientRestackRaise,    0);
  rb_define_method(client, "lower",        subClientRestackLower,    0);
  rb_define_method(client, "up",           subClientMatchUp,         0);
  rb_define_method(client, "left",         subClientMatchLeft,       0);
  rb_define_method(client, "right",        subClientMatchRight,      0);
  rb_define_method(client, "down",         subClientMatchDown,       0);
  rb_define_method(client, "focus",        subClientFocus,           0);
  rb_define_method(client, "has_focus?",   subClientFocusAsk,        0);
  rb_define_method(client, "to_str",       subClientToString,        0);
  rb_define_method(client, "gravity",      subClientGravityReader,   0);
  rb_define_method(client, "gravity=",     subClientGravityWriter,   1);
  rb_define_method(client, "screen",       subClientScreenReader,    0);
  rb_define_method(client, "screen=",      subClientScreenWriter,    1);  
  rb_define_method(client, "geometry",     subClientGeometryReader,  0);
  rb_define_method(client, "geometry=",    subClientGeometryWriter, -1);
  rb_define_method(client, "alive?",       subClientAliveAsk,        0);
  rb_define_method(client, "kill",         subClientKill,            0);

  rb_define_alias(client, "save", "update");
  rb_define_alias(client, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Geometry
   *
   * Class for various sizes
   */

  geometry = rb_define_class_under(mod, "Geometry", rb_cObject);

  /* X offset */
  rb_define_attr(geometry, "x",      1, 1);

  /* Y offset */
  rb_define_attr(geometry, "y",      1, 1);

  /* Geometry width */
  rb_define_attr(geometry, "width",  1, 1);

  /* Geometry height */
  rb_define_attr(geometry, "height", 1, 1);

  rb_define_method(geometry, "initialize", subGeometryInit,     -1);
  rb_define_method(geometry, "to_a",       subGeometryToArray,   0);
  rb_define_method(geometry, "to_hash",    subGeometryToHash,    0);
  rb_define_method(geometry, "to_str",     subGeometryToString,  0);

  rb_define_alias(geometry, "to_h", "to_hash");
  rb_define_alias(geometry, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Gravity
   *
   * Class for Client placement
   */

  gravity = rb_define_class_under(mod, "Gravity", rb_cObject);

  /* Gravity id */
  rb_define_attr(gravity, "id",       1, 0);

  /* Name of the gravity */
  rb_define_attr(gravity, "name",     1, 0);

  /* Geometry */
  rb_define_attr(gravity, "geometry", 0, 0);

  rb_define_singleton_method(gravity, "find", subGravityFind, 1);
  rb_define_singleton_method(gravity, "[]",   subGravityFind, 1);
  rb_define_singleton_method(gravity, "all",  subGravityAll,  0);

  rb_define_method(gravity, "initialize", subGravityInit,     1);
  rb_define_method(gravity, "update",     subGravityUpdate,   0);
  rb_define_method(gravity, "geometry",   subGravityGeometry, 0);
  rb_define_method(gravity, "to_str",     subGravityToString, 0);
  rb_define_method(gravity, "to_sym",     subGravityToSym,    0);
  rb_define_method(gravity, "kill",       subGravityKill,     0);

  rb_define_alias(gravity, "save", "update");
  rb_define_alias(gravity, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Screen
   *
   * Class for interaction with screens
   */

  screen = rb_define_class_under(mod, "Screen", rb_cObject);

  /* Screen id */
  rb_define_attr(screen, "id",       1, 0);

  /* Geometry */
  rb_define_attr(screen, "geometry", 1, 0);

  rb_define_singleton_method(screen, "find",    subScreenFind,    1);
  rb_define_singleton_method(screen, "[]",      subScreenFind,    1);
  rb_define_singleton_method(screen, "current", subScreenCurrent, 0);
  rb_define_singleton_method(screen, "all",     subScreenAll,     0);

  rb_define_method(screen, "initialize", subScreenInit,       1);
  rb_define_method(screen, "update",     subScreenUpdate,     0);
  rb_define_method(screen, "clients",    subScreenClientList, 0);
  rb_define_method(screen, "to_str",     subScreenToString,   0);

  rb_define_alias(screen, "save", "update");
  rb_define_alias(screen, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Subtle
   *
   * Module for interaction with the window manager
   */

  subtle = rb_define_module_under(mod, "Subtle");

  rb_define_singleton_method(subtle, "display",        subSubtleDisplay,     0);

  rb_define_singleton_method(subtle, "clients",        subClientAll,         0);
  rb_define_singleton_method(subtle, "gravities",      subGravityAll,        0);
  rb_define_singleton_method(subtle, "screens",        subScreenAll,         0);
  rb_define_singleton_method(subtle, "sublets",        subSubletAll,         0);
  rb_define_singleton_method(subtle, "tags",           subTagAll,            0);
  rb_define_singleton_method(subtle, "trays",          subTrayAll,           0);
  rb_define_singleton_method(subtle, "views",          subViewAll,           0);

  rb_define_singleton_method(subtle, "find_client",    subClientFind,        1);
  rb_define_singleton_method(subtle, "find_gravity",   subGravityFind,       1);
  rb_define_singleton_method(subtle, "find_screen",    subScreenFind,        1);
  rb_define_singleton_method(subtle, "find_sublet",    subSubletFind,        1);
  rb_define_singleton_method(subtle, "find_tag",       subTagFind,           1);
  rb_define_singleton_method(subtle, "find_tray",      subTrayFind,          1);
  rb_define_singleton_method(subtle, "find_view",      subViewFind,          1);

  rb_define_singleton_method(subtle, "focus",          subSubtleFocus,       1);
  rb_define_singleton_method(subtle, "add_tag",        subSubtleTagAdd,      1);
  rb_define_singleton_method(subtle, "add_view",       subSubtleViewAdd,     1);

  rb_define_singleton_method(subtle, "del_client",     subSubtleClientDel,   1);
  rb_define_singleton_method(subtle, "del_gravity",    subSubtleGravityDel,  1);
  rb_define_singleton_method(subtle, "del_sublet",     subSubtleSubletDel,   1);
  rb_define_singleton_method(subtle, "del_tag",        subSubtleTagDel,      1);
  rb_define_singleton_method(subtle, "del_tray",       subSubtleTrayDel,     1);
  rb_define_singleton_method(subtle, "del_view",       subSubtleViewDel,     1);

  rb_define_singleton_method(subtle, "current_view",   subViewCurrent,       0);
  rb_define_singleton_method(subtle, "current_client", subClientCurrent,     0);
  rb_define_singleton_method(subtle, "current_screen", subScreenCurrent,     0);

  rb_define_singleton_method(subtle, "running?",       subSubtleRunningAsk,  0);
  rb_define_singleton_method(subtle, "reload",         subSubtleReload,      0);
  rb_define_singleton_method(subtle, "quit",           subSubtleQuit,        0);
  rb_define_singleton_method(subtle, "spawn",          subSubtleSpawn,       1);

  /*
   * Document-class: Subtlext::Sublet
   *
   * Class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);

  /* Name of the sublet */
  rb_define_attr(sublet, "name", 1, 0);

  rb_define_singleton_method(sublet, "find", subSubletFind, 1);
  rb_define_singleton_method(sublet, "[]",   subSubletFind, 1);
  rb_define_singleton_method(sublet, "all",  subSubletAll,  0);

  rb_define_method(sublet, "initialize",  subSubletInit,             1);
  rb_define_method(sublet, "update",      subSubletUpdate,           0);
  rb_define_method(sublet, "data",        subSubletDataReader,       0);
  rb_define_method(sublet, "data=",       subSubletDataWriter,       1);
  rb_define_method(sublet, "background=", subSubletBackgroundWriter, 1);
  rb_define_method(sublet, "to_str",      subSubletToString,         0);
  rb_define_method(sublet, "kill",        subSubletKill,             0);

  rb_define_alias(sublet, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Tag
   *
   * Class for interaction with tags
   */

  tag = rb_define_class_under(mod, "Tag", rb_cObject);

  /* Tag id */
  rb_define_attr(tag,   "id",   1, 0);

  /* Name of the tag */
  rb_define_attr(tag,   "name", 1, 0);

  rb_define_singleton_method(tag, "find", subTagFind, 1);
  rb_define_singleton_method(tag, "[]",   subTagFind, 1);
  rb_define_singleton_method(tag, "all",  subTagAll,  0);

  rb_define_method(tag, "initialize", subTagInit,     1);
  rb_define_method(tag, "update",     subTagUpdate,   0);
  rb_define_method(tag, "clients",    subTagClients,  0);
  rb_define_method(tag, "views",      subTagViews,    0);
  rb_define_method(tag, "to_str",     subTagToString, 0);
  rb_define_method(tag, "kill",       subTagKill,     0);

  rb_define_alias(tag, "save", "update");
  rb_define_alias(tag, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Tray
   *
   * Class for interaction with trays
   */

  tray = rb_define_class_under(mod, "Tray", rb_cObject);

  /* Tray id */
  rb_define_attr(tray, "id",    1, 0);

  /* Window id */
  rb_define_attr(tray, "win",   1, 0);

  /* WM_CLASS */
  rb_define_attr(tag,  "klass", 1, 0);

  /* WM_NAME */
  rb_define_attr(tag,  "title", 1, 0);

  /* WM_NAME */
  rb_define_attr(tag,  "name",  1, 0);

  rb_define_singleton_method(tray, "find", subTrayFind, 1);
  rb_define_singleton_method(tray, "[]",   subTrayFind, 1);
  rb_define_singleton_method(tray, "all",  subTrayAll,  0);

  rb_define_method(tray, "initialize", subTrayInit,     1);
  rb_define_method(tray, "update",     subTrayUpdate,   0);
  rb_define_method(tray, "focus",      subTrayFocus,    0);
  rb_define_method(tray, "has_focus?", subTrayFocusAsk, 0);
  rb_define_method(tray, "to_str",     subTrayToString, 0);
  rb_define_method(tray, "kill",       subTrayKill,     0);

  rb_define_alias(tray, "to_s", "to_str");

  /*
   * Document-class: Subtlext::View
   *
   * Class for interaction with views
   */

  view = rb_define_class_under(mod, "View", rb_cObject);

  /* View id */
  rb_define_attr(view, "id",   1, 0);

  /* Window id */
  rb_define_attr(view, "win",  1, 0);

  /* Name of the view */
  rb_define_attr(view, "name", 1, 0);

  rb_define_singleton_method(view, "find",    subViewFind,    1);
  rb_define_singleton_method(view, "[]",      subViewFind,    1);
  rb_define_singleton_method(view, "current", subViewCurrent, 0);
  rb_define_singleton_method(view, "all",     subViewAll,     0);

  rb_define_method(view, "initialize", subViewInit,          1);
  rb_define_method(view, "update",     subViewUpdate,        0);

  rb_define_method(view, "tags",       subSubtlextTagList,     0);
  rb_define_method(view, "has_tag?",   subSubtlextTagAsk,      1);
  rb_define_method(view, "tag",        subSubtlextTagAdd,      1);
  rb_define_method(view, "untag",      subSubtlextTagDel,      1);
  rb_define_method(view, "+",          subSubtlextTagAdd,      1);
  rb_define_method(view, "-",          subSubtlextTagDel,      1);

  rb_define_method(view, "clients",    subViewClients,       0);
  rb_define_method(view, "jump",       subViewJump,          0);
  rb_define_method(view, "current?",   subViewCurrentAsk,    0);
  rb_define_method(view, "to_str",     subViewToString,      0);
  rb_define_method(view, "kill",       subViewKill,          0);

  rb_define_alias(view, "save", "update");
  rb_define_alias(view, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Window
   *
   * Class for interaction with windows
   */

  window = rb_define_class_under(mod, "Window", rb_cObject);

  /* Window id */
  rb_define_attr(window, "win", 1, 0);

  /* WM_NAME */
  rb_define_attr(window, "name", 1, 0);

  rb_define_singleton_method(window, "new", subWindowNew, -1);

  rb_define_method(window, "show",        subWindowShow,              0);
  rb_define_method(window, "hide",        subWindowHide,              0);
  rb_define_method(window, "kill",        subWindowKill,              0);
  rb_define_method(window, "background=", subWindowBackgroundWriter,  1);
  rb_define_method(window, "geometry",    subWindowGeometryReader,    0);
  rb_define_method(window, "puts",        subWindowPuts,             -1);

} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
