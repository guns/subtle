
 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <ruby.h>
#include "shared.h"

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

#define CHAR2SYM(name) ID2SYM(rb_intern(name))
#define SYM2CHAR(sym)  rb_id2name(SYM2ID(sym))

Display *display = NULL;
static int refcount = 0;
VALUE mod = Qnil;

static const char *klasses[] = { 
  "Client", "Gravity", "View", "Tag", "Tray", "Screen", "Sublet" 
};

#ifdef DEBUG
int debug = 0;
#endif /* DEBUG */

/* Prototypes {{{ */
static VALUE SubtlextClientUpdate(VALUE self);
static VALUE SubtlextGravityUpdate(VALUE self);
static VALUE SubtlextScreenUpdate(VALUE self);
static VALUE SubtlextTrayUpdate(VALUE self);
static VALUE SubtlextViewUpdate(VALUE self);
static VALUE SubtlextClientCurrent(VALUE self);
static VALUE SubtlextScreenCurrent(VALUE self);
static VALUE SubtlextViewCurrent(VALUE self);
static VALUE SubtlextClientAll(VALUE self);
static VALUE SubtlextGravityAll(VALUE self);
static VALUE SubtlextTagAll(VALUE self);
static VALUE SubtlextScreenAll(VALUE self);
static VALUE SubtlextSubletAll(VALUE self);
static VALUE SubtlextViewAll(VALUE self);
/* }}} */

/* Flags {{{ */
#define SUB_TYPE_CLIENT  0           ///< Client
#define SUB_TYPE_GRAVITY 1           ///< Gravity
#define SUB_TYPE_VIEW    2           ///< View
#define SUB_TYPE_TAG     3           ///< Tag
#define SUB_TYPE_TRAY    4           ///< Tray
#define SUB_TYPE_SCREEN  5           ///< Screen
#define SUB_TYPE_SUBLET  6           ///< Sublet
/* }}} */

/* Instantiator */

/* SubtlextInstantiateClient {{{ */
static VALUE
SubtlextInstantiateClient(Window win)
{
  VALUE klass = Qnil, client = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Client"));
  client = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(win));

  return client;
} /* }}} */

/* SubtlextInstantiateGeometry {{{ */
static VALUE
SubtlextInstantiateGeometry(int x,
  int y,
  int width,
  int height)
{
  VALUE klass = Qnil, geometry = Qnil;

  /* Create new instance */
  klass    = rb_const_get(mod, rb_intern("Geometry"));
  geometry = rb_funcall(klass, rb_intern("new"), 4, 
    INT2FIX(x), INT2FIX(y), INT2FIX(width), INT2FIX(height));

  return geometry;
} /* }}} */

/* SubtlextInstantiateGravity {{{ */
static VALUE
SubtlextInstantiateGravity(char *name)
{
  VALUE klass = Qnil, gravity = Qnil;

  /* Create new instance */
  klass   = rb_const_get(mod, rb_intern("Gravity"));
  gravity = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name)); 

  return gravity;
} /* }}} */

/* SubtlextInstantiateScreen {{{ */
static VALUE
SubtlextInstantiateScreen(int id)
{
  VALUE klass = Qnil, screen = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Screen"));
  screen = rb_funcall(klass, rb_intern("new"), 1, INT2FIX(id));

  return screen;
} /* }}} */

/* SubtlextInstantiateSublet {{{ */
static VALUE
SubtlextInstantiateSublet(char *name)
{
  VALUE klass = Qnil, screen = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Sublet"));
  screen = rb_funcall(klass, rb_intern("new"), 1, RSTRING_PTR(name));

  return screen;
} /* }}} */

/* SubtlextInstantiateTag {{{ */
static VALUE
SubtlextInstantiateTag(char *name)
{
  VALUE klass = Qnil, tag = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("Tag"));
  tag   = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return tag;
} /* }}} */

/* SubtlextInstantiateTray {{{ */
static VALUE
SubtlextInstantiateTray(Window win)
{
  VALUE klass = Qnil, tray = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("Tray"));
  tray  = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(win));

  return tray;
} /* }}} */

/* SubtlextInstantiateView {{{ */
static VALUE
SubtlextInstantiateView(char *name)
{
  VALUE klass = Qnil, view = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("View"));
  view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  return view;
} /* }}} */

/* Misc */

/* SubtlextConnect {{{ */
static void
SubtlextConnect(VALUE disp)
{
  /* Open display */
  if(!display) ///< Establish new connection
    {
      char *name = NULL;

      if(RTEST(disp)) name = RSTRING_PTR(disp);
      if(!(display = XOpenDisplay(name)))
        {
          subSharedLogError("Failed opening display `%s'\n", (name) ? name : ":0.0");

          return;
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

      subSharedLogDebug("Connection opened (%s)\n", name);
    }

  refcount++;
} /* }}} */

/* SubtlextFind {{{ */
static VALUE
SubtlextFind(int type,
  VALUE value,
  int exception)
{
  int id = 0, flags = 0;
  Window win = None;
  char *name = NULL, buf[50] = { 0 };
  XRectangle geometry = { 0 };
  VALUE object = Qnil;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM: /* {{{ */
        id = FIX2INT(value);
        snprintf(buf, sizeof(buf), "%d", id);
        break; /* }}} */
      case T_HASH: /* {{{ */
        if(SUB_TYPE_CLIENT == type)
          {
            int i;
            VALUE meth_has_key = rb_intern("has_key?"), meth_fetch = rb_intern("fetch"), match = Qnil;

            struct properties 
            {
              VALUE sym;
              int   flags;
            } props[] = {
              { CHAR2SYM("title"),   SUB_MATCH_TITLE   },
              { CHAR2SYM("name"),    SUB_MATCH_NAME    },
              { CHAR2SYM("class"),   SUB_MATCH_CLASS   },
              { CHAR2SYM("gravity"), SUB_MATCH_GRAVITY }
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
                case SUB_TYPE_CLIENT: return SubtlextClientCurrent(Qnil); break;
                case SUB_TYPE_SCREEN: return SubtlextScreenCurrent(Qnil); break;
                case SUB_TYPE_VIEW:   return SubtlextViewCurrent(Qnil);   break;
              }
          }
        else if(CHAR2SYM("all") == value)
          {
            switch(type)
              {
                case SUB_TYPE_CLIENT:  return SubtlextClientAll(Qnil);  break;
                case SUB_TYPE_GRAVITY: return SubtlextGravityAll(Qnil); break;
                case SUB_TYPE_TAG:     return SubtlextTagAll(Qnil);     break;
                case SUB_TYPE_SCREEN:  return SubtlextScreenAll(Qnil);  break;
                case SUB_TYPE_SUBLET:  return SubtlextSubletAll(Qnil);  break;
                case SUB_TYPE_VIEW:    return SubtlextViewAll(Qnil);    break;
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
            if(Qnil != (object = SubtlextInstantiateClient(win)))
              {
                rb_iv_set(object, "@id", FIX2INT(id)); 
                
                SubtlextClientUpdate(object);
              }
          }
        break; /* }}} */
      case SUB_TYPE_GRAVITY: /* {{{ */
        if(-1 != (id = subSharedGravityFind(buf, &name, &geometry)))
          {
            if(Qnil != (object = SubtlextInstantiateGravity(name)))
              {
                VALUE geom = SubtlextInstantiateGeometry(geometry.x, geometry.y, 
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
            if(Qnil != (object = SubtlextInstantiateTag(name)))
              rb_iv_set(object, "@id", FIX2INT(id)); 
            
            free(name);
          }
        break; /* }}} */
      case SUB_TYPE_TRAY: /* {{{ */
        if(-1 != (id = subSharedTrayFind(buf, NULL, &win, flags)))
          {
            if(Qnil != (object = SubtlextInstantiateTray(win)))
              {
                rb_iv_set(object, "@id", INT2FIX(id)); 

                SubtlextTrayUpdate(object);
              }
          }
        break; /* }}} */        
      case SUB_TYPE_SCREEN: /* {{{ */
        if(-1 != (id = subSharedScreenFind(id, &geometry)))
          {
            if(Qnil != (object = SubtlextInstantiateScreen(id)))
              {
                VALUE geom = SubtlextInstantiateGeometry(geometry.x, geometry.y, 
                  geometry.width, geometry.height);

                rb_iv_set(object, "@geometry", geom);
              }
          }
        break; /* }}} */
      case SUB_TYPE_SUBLET: /* {{{ */
        if(-1 != (id = subSharedSubletFind(buf, &name)))
          {
            if(Qnil != (object = SubtlextInstantiateSublet(name)))
              rb_iv_set(object, "@id", INT2FIX(id)); 
            
            free(name);
          }
        break; /* }}} */
      case SUB_TYPE_VIEW: /* {{{ */
        if(-1 != (id = subSharedViewFind(buf, &name, &win)))
          {
            if(Qnil != (object = SubtlextInstantiateView(name)))
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

/* SubtlextKill {{{ */
static VALUE
SubtlextKill(VALUE value,
  int type)
{
  int id = -1;

  /* Check object type */
  switch(rb_type(value))
    {
      case T_OBJECT: id = FIX2INT(rb_iv_get(value, "@id")); break;
      case T_STRING:
        {
          VALUE object = Qnil;

          if(RTEST((object = SubtlextFind(type, value, False))))
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

/* SubtlextToggle {{{ */
static VALUE
SubtlextToggle(VALUE self,
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

/* SubtlextRestack {{{ */
static VALUE
SubtlextRestack(VALUE self,
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

/* SubtlextScreens {{{ */
static VALUE
SubtlextScreens(void)
{
  int n = 0;
  VALUE method = Qnil, klass = Qnil, array = Qnil, screen = Qnil, geometry = Qnil; 

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  method = rb_intern("new");
  klass  = rb_const_get(mod, rb_intern("Screen"));
  array  = rb_ary_new();

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  /* Xinerama */
  if(XineramaQueryExtension(display, &xinerama_event, &xinerama_error) &&
    XineramaIsActive(display))
    {
      int i;
      XineramaScreenInfo *screens = NULL;

      /* Query screens */
      if((screens = XineramaQueryScreens(display, &n)))
        {
          for(i = 0; i < n; i++)
            {
              screen   = rb_funcall(klass, method, 1, INT2FIX(i));
              geometry = SubtlextInstantiateGeometry(screens[i].x_org,
                screens[i].y_org, screens[i].width, screens[i].height);

              rb_iv_set(screen, "@geometry", geometry);
              rb_ary_push(array, screen);
            }

          XFree(screens);
        }
    } 
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

  /* Get default screen */
  if(0 == n)
    {
      screen   = rb_funcall(klass, method, 1, INT2FIX(0));
      geometry = SubtlextInstantiateGeometry(0, 0, DisplayWidth(display, DefaultScreen(display)),
        DisplayHeight(display, DefaultScreen(display)));

      rb_iv_set(screen, "@geometry", geometry);
      rb_ary_push(array, screen);    
    }

  return array;
} /* }}} */

/* SubtlextMatch {{{ */
static VALUE
SubtlextMatch(VALUE self,
  int type)
{
  int i, id = 0, size = 0, match = 0, score = 0;
  Window *clients = NULL, *views = NULL, found = None;
  VALUE win = Qnil, client = Qnil;
  unsigned long *cv = NULL, *flags1 = NULL;
  XRectangle geometry1 = { 0 }, geometry2 = { 0 };

  win     = rb_iv_get(self, "@win");
  clients = subSharedClientList(&size);
  views   = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW,
    "_NET_VIRTUAL_ROOTS", NULL);
  cv      = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

  if(clients && cv)
    {
      flags1   = (unsigned long *)subSharedPropertyGet(views[*cv], XA_CARDINAL,
        "SUBTLE_WINDOW_TAGS", NULL);
      subSharedPropertyGeometry(win, &geometry1);

      /* Iterate once to find a client score-based */
      for(i = 0; 100 != match && i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL,
            "SUBTLE_WINDOW_TAGS", NULL);

          if(win != clients[i] && *flags1 & *flags2) ///< Check if there are common tags
            {
              subSharedPropertyGeometry(win, &geometry2);

              if(match < (score = subSharedMatch(type, &geometry1, &geometry2)))
                {
                  match = score;
                  found = clients[i];
                  id    = i;
                }

            }

          free(flags2);
        }

      if(found) client = SubtlextInstantiateClient(found);

      free(flags1);
      free(clients);
      free(cv);
    }

  return client;
} /* }}} */

/* SubtlextTag {{{ */
static VALUE
SubtlextTag(VALUE self,
  VALUE value,
  char *action)
{
  VALUE tag = Qnil;

  /* Find tag */
  if(Qnil != (tag = SubtlextFind(SUB_TYPE_TAG, value, True)))
    {
      VALUE oid = Qnil, tid = Qnil;

      oid   = rb_iv_get(self, "@id");
      tid   = rb_iv_get(tag, "@id");

      if(Qnil != oid && Qnil != tid)
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };

          data.l[0] = FIX2LONG(oid);
          data.l[1] = FIX2LONG(tid);
          data.l[2] = rb_obj_is_instance_of(value, 
            rb_const_get(mod, rb_intern("Client"))) ? 0 : 1; ///< Client = 0, View = 1

          subSharedMessage(DefaultRootWindow(display), action, data, True);
        }
    }

  return Qnil;
} /* }}} */

/* Window */

/* SubtlextWindowTagAdd {{{ */
/*
 * call-seq: tag(name) -> Subtlext::Tag
 *           +(name)   -> Subtlext::Tag
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

static VALUE
SubtlextWindowTagAdd(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, "SUBTLE_WINDOW_TAG");
} /* }}} */

/* SubtlextWindowTagDel {{{ */
/*
 * call-seq: untag(name) -> Subtlext::Tag
 *           -(name)     -> Subtlext::Tag
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

static VALUE
SubtlextWindowTagDel(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, "SUBTLE_WINDOW_UNTAG");
} /* }}} */

/* SubtlextWindowTagList {{{ */
static VALUE
SubtlextWindowTagList(VALUE self)
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

/* SubtlextWindowTagAssoc {{{ */
static VALUE
SubtlextWindowTagAssoc(VALUE self,
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

                if(!NIL_P(object)) SubtlextClientUpdate(object); ///< Init client
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

/* SubtlextWindowTagAsk {{{ */
static VALUE
SubtlextWindowTagAsk(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil, ret = Qfalse;

  /* Find tag */
  if(Qnil != (tag = SubtlextFind(SUB_TYPE_TAG, value, False)))
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

/* SubtlextWindowFocus {{{ */
static VALUE
SubtlextWindowFocus(VALUE self)
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

/* SubtlextWindowFocusAsk {{{ */
static VALUE
SubtlextWindowFocusAsk(VALUE self)
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

/* Client */

/* SubtlextClientInit {{{ */
/*
 * call-seq: new(win) -> Subtlext::Client
 *
 * Create a new Client object
 *
 *  client = Subtlext::Client.new(123456789)
 *  => #<Subtlext::Client:xxx>
 */

static VALUE
SubtlextClientInit(VALUE self,
  VALUE win)
{
  if(!FIXNUM_P(win) && T_BIGNUM != rb_type(win))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",       Qnil);
  rb_iv_set(self, "@win",      win);
  rb_iv_set(self, "@name",     Qnil);
  rb_iv_set(self, "@klass",    Qnil);
  rb_iv_set(self, "@title",    Qnil);
  rb_iv_set(self, "@geometry", Qnil); 
  rb_iv_set(self, "@gravity",  Qnil);
  rb_iv_set(self, "@screen",   Qnil);
  rb_iv_set(self, "@flags",    Qnil);

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  return self;
} /* }}} */

/* SubtlextClientFind {{{ */
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

static VALUE
SubtlextClientFind(VALUE self,
  VALUE name)
{
  return SubtlextFind(SUB_TYPE_CLIENT, name, True);
} /* }}} */

/* SubtlextClientCurrent {{{ */
/*
 * call-seq: current -> Subtlext::Client
 *
 * Get current active Client
 *
 *  Subtlext::Client.current
 *  => #<Subtlext::Client:xxx>
 */


static VALUE
SubtlextClientCurrent(VALUE self)
{
  VALUE client = Qnil;
  unsigned long *focus = NULL;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  if((focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL)))
    {
      client = SubtlextInstantiateClient(*focus);

      if(!NIL_P(client)) SubtlextClientUpdate(client);

      free(focus);
    }
  else rb_raise(rb_eStandardError, "Failed getting current client");

  return client;
} /* }}} */

/* SubtlextClientAll {{{ */
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

static VALUE
SubtlextClientAll(VALUE self)
{
  int i, size = 0;
  Window *clients = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil, client = Qnil;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection
  
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
              SubtlextClientUpdate(client);
              rb_ary_push(array, client);
            }
        }

      free(clients);
    }

  return array;
} /* }}} */

/* SubtlextClientUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Client properties
 *
 *  client.update
 *  => nil
 */

static VALUE
SubtlextClientUpdate(VALUE self)
{
  VALUE win = rb_iv_get(self, "@win");

  if(RTEST(win) && (T_FIXNUM == rb_type(win) || T_BIGNUM == rb_type(win)))
    {
      int id = 0;
      char buf[20] = { 0 };

      snprintf(buf, sizeof(buf), "%#lx", NUM2LONG(win));
      if(-1 != (id = subSharedClientFind(buf, NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS))))
        {
          int *flags = NULL;
          char *title = NULL, *wmname = NULL, *wmclass = NULL;

          flags = (int *)subSharedPropertyGet(NUM2LONG(win), XA_CARDINAL,
            "SUBTLE_WINDOW_FLAGS", NULL);

          XFetchName(display, NUM2LONG(win), &title);
          subSharedPropertyClass(NUM2LONG(win), &wmname, &wmclass);

          /* Set properties */
          rb_iv_set(self, "@id",       INT2FIX(id));
          rb_iv_set(self, "@flags",    INT2FIX(*flags));
          rb_iv_set(self, "@title",    rb_str_new2(title));
          rb_iv_set(self, "@name",     rb_str_new2(wmname));
          rb_iv_set(self, "@klass",    rb_str_new2(wmclass));

          /* Set to nil for on demand loading */
          rb_iv_set(self, "@geometry", Qnil);
          rb_iv_set(self, "@gravity",  Qnil);
          rb_iv_set(self, "@screen",   Qnil);

          free(flags);
          free(title);
          free(wmname);
          free(wmclass);
        }
      else rb_raise(rb_eStandardError, "Failed finding client");  
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* SubtlextClientViewList {{{ */
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

static VALUE
SubtlextClientViewList(VALUE self)
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
  names   = subSharedPropertyStrings(DefaultRootWindow(display), 
    "_NET_DESKTOP_NAMES", &size);
  views   = (Window *)subSharedPropertyGet(DefaultRootWindow(display), 
    XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);
  flags1  = (unsigned long *)subSharedPropertyGet(NUM2LONG(win), XA_CARDINAL, 
    "SUBTLE_WINDOW_TAGS", NULL);

  if(names && views)
    {
      for(i = 0; i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(views[i], 
            XA_CARDINAL, "SUBTLE_WINDOW_TAGS", NULL);

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

/* SubtlextClientStateFullAsk {{{ */
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
SubtlextClientStateFullAsk(VALUE self)
{
  return FIX2INT(rb_iv_get(self, "@flags")) & SUB_EWMH_FULL ? Qtrue : Qfalse;
} /* }}} */

/* SubtlextClientStateFloatAsk {{{ */
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
SubtlextClientStateFloatAsk(VALUE self)
{
  return FIX2INT(rb_iv_get(self, "@flags")) & SUB_EWMH_FLOAT ? Qtrue : Qfalse;
} /* }}} */

/* SubtlextClientStateStickAsk {{{ */
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
SubtlextClientStateStickAsk(VALUE self)
{
  return FIX2INT(rb_iv_get(self, "@flags")) & SUB_EWMH_STICK ? Qtrue : Qfalse;
} /* }}} */

/* SubtlextClientToggleFull {{{ */
/*
 * call-seq: toggle_full -> nil
 *
 * Toggle Client fullscreen state
 *
 *  client.toggle_full
 *  => nil
 */

static VALUE
SubtlextClientToggleFull(VALUE self)
{
  return SubtlextToggle(self, "_NET_WM_STATE_FULLSCREEN", SUB_EWMH_FULL);
} /* }}} */

/* SubtlextClientToggleFloat {{{ */
/*
 * call-seq: toggle_float -> nil
 *
 * Toggle Client floating state
 *
 *  client.toggle_float
 *  => nil
 */

static VALUE
SubtlextClientToggleFloat(VALUE self)
{
  return SubtlextToggle(self, "_NET_WM_STATE_ABOVE", SUB_EWMH_FLOAT);
} /* }}} */

/* SubtlextClientToggleStick {{{ */
/*
 * call-seq: toggle_stick -> nil
 *
 * Toggle Client sticky state
 *
 *  client.toggle_stick
 *  => nil
 */

static VALUE
SubtlextClientToggleStick(VALUE self)
{
  return SubtlextToggle(self, "_NET_WM_STATE_STICKY", SUB_EWMH_STICK);
} /* }}} */

/* SubtlextClientRestackRaise {{{ */
/*
 * call-seq: raise -> nil
 *
 * Raise Client window
 *
 *  client.raise
 *  => nil
 */

static VALUE
SubtlextClientRestackRaise(VALUE self)
{
  return SubtlextRestack(self, Above);
} /* }}} */

/* SubtlextClientRestackLower {{{ */
/*
 * call-seq: lower -> nil
 *
 * Raise Client window
 *
 *  client.raise
 *  => nil
 */

static VALUE
SubtlextClientRestackLower(VALUE self)
{
  return SubtlextRestack(self, Below);
} /* }}} */

/* SubtlextClientMatchUp {{{ */
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

static VALUE
SubtlextClientMatchUp(VALUE self)
{
  return SubtlextMatch(self, SUB_WINDOW_UP);
} /* }}} */

/* SubtlextClientMatchLeft {{{ */
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

static VALUE
SubtlextClientMatchLeft(VALUE self)
{
  return SubtlextMatch(self, SUB_WINDOW_LEFT);
} /* }}} */

/* SubtlextClientMatchRight {{{ */
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

static VALUE
SubtlextClientMatchRight(VALUE self)
{
  return SubtlextMatch(self, SUB_WINDOW_RIGHT);
} /* }}} */

/* SubtlextClientMatchDown {{{ */
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

static VALUE
SubtlextClientMatchDown(VALUE self)
{
  return SubtlextMatch(self, SUB_WINDOW_DOWN);
} /* }}} */

/* SubtlextClientFocus {{{ */
/*
 * call-seq: focus -> nil
 *
 * Set focus to Client
 *
 *  client.focus
 *  => nil
 */

static VALUE
SubtlextClientFocus(VALUE self)
{
  return SubtlextWindowFocus(self);
} /* }}} */

/* SubtlextClientFocusAsk {{{ */
/*
 * call-seq: has_focus? -> true or false
 *
 * Check if Client has focus
 *
 *  client.focus?
 *  => true
 *
 *  client.focus?
 *  => false
 */

static VALUE
SubtlextClientFocusAsk(VALUE self)
{
  return SubtlextWindowFocusAsk(self);
} /* }}} */

/* SubtlextClientAliveAsk {{{ */
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

static VALUE
SubtlextClientAliveAsk(VALUE self)
{
  VALUE ret = Qfalse, name = rb_iv_get(self, "@name");

  /* Just find the client */
  if(RTEST(name) && -1 != subSharedClientFind(RSTRING_PTR(name), 
      NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    ret = Qtrue;

  return ret;
} /* }}} */

/* SubtlextClientGravityReader {{{ */
/* 
 * call-seq: gravity -> Subtlext::Gravity
 *
 * Get Client Gravity
 *
 *  client.gravity
 *  => #<Subtlext::Gravity:xxx>
 */

static VALUE
SubtlextClientGravityReader(VALUE self)
{
  Window win = None;
  VALUE gravity = Qnil;
  
  /* Load on demand */
  if(NIL_P((gravity = rb_iv_get(self, "@gravity"))) && 
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int *id = NULL;
      char buf[5] = { 0 };

      /* Collect data */
      id = (int *)subSharedPropertyGet(win, XA_CARDINAL,
        "SUBTLE_WINDOW_GRAVITY", NULL);

      snprintf(buf, sizeof(buf), "%d", *id);
      gravity = SubtlextInstantiateGravity(buf);

      if(!NIL_P(gravity)) SubtlextGravityUpdate(gravity);

      rb_iv_set(self, "@gravity", gravity);

      free(id);
    }

  return gravity;
} /* }}} */

/* SubtlextClientGravityWriter {{{ */
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

static VALUE
SubtlextClientGravityWriter(VALUE self,
  VALUE value)
{
  VALUE gravity = SubtlextFind(SUB_TYPE_GRAVITY, value, True);
  
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

/* SubtlextClientScreenReader {{{ */
/*
 * call-seq: screen -> Subtlext::Screen
 *
 * Get screen Client is on as Screen object
 *
 *  client.screen
 *  => #<Subtlext::Screen:xxx>
 */

static VALUE
SubtlextClientScreenReader(VALUE self)
{
  Window win = None;
  VALUE screen = Qnil;
  
  /* Load on demand */
  if(NIL_P((screen = rb_iv_get(self, "@screen"))) &&
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int *id = NULL;
      
      /* Collect data */
      id     = (int *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_WINDOW_SCREEN", NULL);
      screen = SubtlextInstantiateScreen(*id);

      if(!NIL_P(screen)) SubtlextScreenUpdate(screen);

      rb_iv_set(self, "@screen", screen);

      free(id);
    }

  return screen;
} /* }}} */

/* SubtlextClientScreenWriter {{{ */
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

static VALUE
SubtlextClientScreenWriter(VALUE self,
  VALUE value)
{
  VALUE screen = SubtlextFind(SUB_TYPE_SCREEN, value, True);

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

/* SubtlextClientGeometryReader {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get Client Geometry
 *
 *  client.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

static VALUE
SubtlextClientGeometryReader(VALUE self)
{
  Window win = None;
  VALUE geom = Qnil;

  /* Load on demand */
  if(NIL_P((geom = rb_iv_get(self, "@geometry"))) &&
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      XRectangle geometry = { 0 };

      subSharedPropertyGeometry(win, &geometry);

      geom = SubtlextInstantiateGeometry(geometry.x, geometry.y, 
        geometry.width, geometry.height);
      rb_iv_set(self, "@geometry", geom);
    }

  return geom;
} /* }}} */

/* SubtlextClientGeometryWriter {{{ */
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

static VALUE
SubtlextClientGeometryWriter(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE x = Qnil, y = Qnil, width = Qnil, height = Qnil, geometry = Qnil;

  /* Load geometry object */
  rb_scan_args(argc, argv, "04", &x, &y, &width, &height);
  geometry = SubtlextInstantiateGeometry(FIX2INT(x), FIX2INT(y), FIX2INT(width), FIX2INT(height));

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

/* SubtlextClientToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Client object to String
 *
 *  puts client
 *  => "subtle"
 */

static VALUE
SubtlextClientToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextClientKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Send a close signal to Client
 *
 *  client.kill
 *  => nil
 */

static VALUE
SubtlextClientKill(VALUE self)
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

/* Geometry */

/* SubtlextGeometryInit {{{ */
/*
 * call-seq: new(x, y, width, height) -> Subtlext::Geometry
 *           new(array)               -> Subtlext::Geometry
 *           new(hash)                -> Subtlext::Geometry
 *           new(geometry)            -> Subtlext::Geometry
 *
 * Create a new Geometry object
 *
 *  geom = Subtlext::Geometry.new(0, 0, 50, 50)
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new([ 0, 0, 50, 50 ])
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new({ :x => 0, :y => 0, :width => 50, :height => 50 })
 *  => #<Subtlext::Geometry:xxx>
 *
 *  geom = Subtlext::Geometry.new(Subtlext::Geometry.new(0, 0, 50, 50))
 *  => #<Subtlext::Geometry:xxx>
 */

static VALUE
SubtlextGeometryInit(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE value = Qnil, data[4] = { Qnil };

  rb_scan_args(argc, argv, "04", &data[0], &data[1], &data[2], &data[3]);
  value = data[0];

  /* Check object type */
  switch(rb_type(value))
    {
      case T_ARRAY:
        if(4 == FIX2INT(rb_funcall(value, rb_intern("size"), 0, NULL)))
          {
            int i;
            VALUE meth = rb_intern("at");

            for(i = 0; 4 > i; i++)
              data[i] = rb_funcall(value, meth, 1, INT2FIX(i));
          }
        break;
      case T_HASH:
          {
            int i;
            const char *syms[] = { "x", "y", "width", "height" };
            VALUE meth_has_key = rb_intern("has_key?"), meth_fetch = rb_intern("fetch");

            for(i = 0; 4 > i; i++)
              {
                VALUE sym = CHAR2SYM(syms[i]);

                if(Qtrue == rb_funcall(value, meth_has_key, 1, sym)) 
                  data[i] = rb_funcall(value, meth_fetch, 1, sym);
              }
          }
        break;
      case T_OBJECT:
          {
            VALUE klass = rb_const_get(mod, rb_intern("Geometry")); 
            
            /* A copy constructor would be more suitable for this.. */
            if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
              {
                data[0] = rb_iv_get(value, "@x");
                data[1] = rb_iv_get(value, "@y");
                data[2] = rb_iv_get(value, "@height");
                data[3] = rb_iv_get(value, "@width");
              }
          }        
    }
  /* Set values */
  if(FIXNUM_P(data[0]) && FIXNUM_P(data[1]) && FIXNUM_P(data[2]) && 
      FIXNUM_P(data[3]) && 0 < FIX2INT(data[2]) && 0 < FIX2INT(data[3]))
    {
      rb_iv_set(self, "@x",      data[0]);
      rb_iv_set(self, "@y",      data[1]);
      rb_iv_set(self, "@width",  data[2]); 
      rb_iv_set(self, "@height", data[3]); 
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return self;
} /* }}} */

/* SubtlextGeometryToArray {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Geometry object to Array
 *
 *  geom.to_a
 *  => [0, 0, 50, 50]
 */

static VALUE
SubtlextGeometryToArray(VALUE self)
{
  VALUE ary = rb_ary_new2(4);

  rb_ary_push(ary, rb_iv_get(self, "@x"));
  rb_ary_push(ary, rb_iv_get(self, "@y"));
  rb_ary_push(ary, rb_iv_get(self, "@width"));
  rb_ary_push(ary, rb_iv_get(self, "@height"));

  return ary;
} /* }}} */

/* SubtlextGeometryToHash {{{ */
/*
 * call-seq: to_hash -> Hash
 *
 * Convert Geometry object to Hash
 *
 *  geom.to_hash
 *  => { :x => 0, :y => 0, :width => 50, :height => 50 }
 */

static VALUE
SubtlextGeometryToHash(VALUE self)
{
  VALUE klass = Qnil, hash = Qnil, meth = Qnil;

  klass = rb_const_get(rb_mKernel, rb_intern("Hash")); 
  hash  = rb_funcall(klass, rb_intern("new"), 0, NULL);
  meth  = rb_intern("store");

  rb_funcall(hash, meth, 2, CHAR2SYM("x"),      rb_iv_get(self, "@x"));
  rb_funcall(hash, meth, 2, CHAR2SYM("y"),      rb_iv_get(self, "@y"));
  rb_funcall(hash, meth, 2, CHAR2SYM("width"),  rb_iv_get(self, "@width"));
  rb_funcall(hash, meth, 2, CHAR2SYM("height"), rb_iv_get(self, "@height"));

  return hash;
} /* }}} */

/* SubtlextGeometryToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Geometry object to String
 *
 *  puts geom
 *  => "0x0+50+50"
 */

static VALUE
SubtlextGeometryToString(VALUE self)
{
  char buf[256];
  int x = 0, y = 0, width = 0, height = 0;

  x      = FIX2INT(rb_iv_get(self, "@x"));
  y      = FIX2INT(rb_iv_get(self, "@y"));
  width  = FIX2INT(rb_iv_get(self, "@width"));
  height = FIX2INT(rb_iv_get(self, "@height"));

  snprintf(buf, sizeof(buf), "%dx%d+%d+%d", x, y, width, height);

  return rb_str_new2(buf);
} /* }}} */

/* Gravity */

/* SubtlextGravityInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Gravity
 *
 * Create a new Gravity object
 *
 *  gravity = Subtlext::Gravity.new("top")
 *  => #<Subtlext::Gravity:xxx>
 */

static VALUE
SubtlextGravityInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",       Qnil);
  rb_iv_set(self, "@name",     name);
  rb_iv_set(self, "@geometry", Qnil);

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  return self;
} /* }}} */

/* SubtlextGravityFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Gravity or nil
 *           [value]     -> Subtlext::Gravity or nil
 *
 * Find Gravity by a given value which can be of following type:
 *
 * [fixnum] Array id
 * [string] Match against name of Gravity
 * [symbol] Symbol of the Gravity or :all for an array of all Gravity
 *
 *  Subtlext::Gravity.find("center")
 *  => #<Subtlext::Gravity:xxx>
 *
 *  Subtlext::Gravity[:center]
 *  => #<Subtlext::Gravity:xxx>
 *
 *  Subtlext::Gravity["center"]
 *  => nil
 */

static VALUE
SubtlextGravityFind(VALUE self,
  VALUE value)
{
  return SubtlextFind(SUB_TYPE_GRAVITY, value, True);
} /* }}} */

/* SubtlextGravityAll {{{ */
/*
 * call-seq: gravities -> Array
 *
 * Get Array of all Gravity
 *
 *  Subtlext::Gravity.all
 *  => [#<Subtlext::Gravity:xxx>, #<Subtlext::Gravity:xxx>]
 *
 *  Subtlext::Gravity.all
 *  => []
 */

static VALUE
SubtlextGravityAll(VALUE self)
{
  int size = 0;
  char **gravities = NULL;

  VALUE array = rb_ary_new();

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  /* Get gravity list */
  if((gravities = subSharedPropertyStrings(DefaultRootWindow(display), 
      "SUBTLE_GRAVITY_LIST", &size)))
    {
      int i;
      XRectangle geometry = { 0 };
      char buf[30] = { 0 };
      VALUE klass_grav = Qnil, klass_geom = Qnil, meth = Qnil, gravity = Qnil, geom = Qnil;

      klass_grav = rb_const_get(mod, rb_intern("Gravity"));
      klass_geom = rb_const_get(mod, rb_intern("Geometry"));
      meth       = rb_intern("new");

      /* Create gravity list */
      for(i = 0; i < size; i++)
        {
          sscanf(gravities[i], "%hdx%hd+%hd+%hd#%s", &geometry.x, &geometry.y,
            &geometry.width, &geometry.height, buf);

          gravity = rb_funcall(klass_grav, meth, 1, rb_str_new2(buf));
          geom    = rb_funcall(klass_geom, meth, 4, INT2FIX(geometry.x), INT2FIX(geometry.y),
            INT2FIX(geometry.width), INT2FIX(geometry.height));

          rb_iv_set(gravity, "@id", INT2FIX(i));
          rb_iv_set(gravity, "@geometry", geom);

          rb_ary_push(array, gravity);
        }

      XFreeStringList(gravities);
    }
  else rb_raise(rb_eStandardError, "Failed getting gravity list");

  return array;
} /* }}} */

/* SubtlextGravityUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Gravity properties
 *
 *  gravity.update
 *  => nil
 */

static VALUE
SubtlextGravityUpdate(VALUE self)
{
  int id = -1;
  XRectangle geometry = { 0 };
  char *name = NULL;
  VALUE match = rb_iv_get(self, "@name");

  /* Update gravity */
  if(T_STRING == rb_type(match) && 
      -1 != (id = subSharedGravityFind(RSTRING_PTR(match), &name, &geometry)))
    {
      rb_iv_set(self, "@id",       INT2FIX(id));
      rb_iv_set(self, "@name",     rb_str_new2(name));
      rb_iv_set(self, "@geometry", SubtlextInstantiateGeometry(geometry.x, geometry.y,
        geometry.width, geometry.height));

      free(name);
    }
  else rb_raise(rb_eStandardError, "Failed finding gravity");

  return Qnil;
} /* }}} */

/* SubtlextGravityGeometry {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get Gravity Geometry
 *
 *  gravity.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

static VALUE
SubtlextGravityGeometry(VALUE self)
{
  VALUE geometry = Qnil, name = Qnil;

  /* Load on demand */
  if(NIL_P((geometry = rb_iv_get(self, "@geometry"))) &&
      T_STRING == rb_type((name = rb_iv_get(self, "@name"))))
    {
      XRectangle geom = { 0 };

      subSharedGravityFind(RSTRING_PTR(name), NULL, &geom);

      geometry = SubtlextInstantiateGeometry(geom.x, geom.y, geom.width, geom.height);
      rb_iv_set(self, "@geometry", geometry);
    }

  return geometry;
} /* }}} */

/* SubtlextGravityToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Gravity object to String
 *
 *  puts gravity
 *  => "TopLeft"
 */

static VALUE
SubtlextGravityToString(VALUE self)
{
  return rb_iv_get(self, "@name");
} /* }}} */

/* SubtlextGravityToSym {{{ */
/*
 * call-seq: to_sym -> Symbol
 *
 * Convert Gravity object to Symbol
 *
 *  puts gravity.to_sym
 *  => :center
 */

static VALUE
SubtlextGravityToSym(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return CHAR2SYM(RSTRING_PTR(name));
} /* }}} */

/* SubtlextGravityKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a Gravity
 *
 *  gravity.kill
 *  => nil
 */

static VALUE
SubtlextGravityKill(VALUE self)
{
  return SubtlextKill(self, SUB_TYPE_GRAVITY);
} /* }}} */

/* Screen */

/* SubtlextScreenInit {{{ */
/*
 * call-seq: new(id) -> Subtlext::Screen
 *
 * Create a new Screen object
 *
 *  screen = Subtlext::Screen.new(0)
 *  => #<Subtlext::Screen:xxx>
 */

static VALUE
SubtlextScreenInit(VALUE self,
  VALUE id)
{
  if(!FIXNUM_P(id) || 0 > FIX2INT(id))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",       id);
  rb_iv_set(self, "@geometry", Qnil); 

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  return self;
} /* }}} */

/* SubtlextScreenFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Screen or nil
 *           [value]     -> Subtlext::Screen or nil
 *
 * Find Screen by a given value which can be of following type:
 *
 * [fixnum] Array id
 *
 *  Subtlext::Screen.find("subtle")
 *  => #<Subtlext::Screen:xxx>
 *
 *  Subtlext::Screen[1]
 *  => #<Subtlext::Screen:xxx>
 *
 *  Subtlext::Screen["subtle"]
 *  => nil
 */

static VALUE
SubtlextScreenFind(VALUE self,
  VALUE id)
{
  return SubtlextFind(SUB_TYPE_SCREEN, id, True);
} /* }}} */

/* SubtlextScreenAll {{{ */
/*
 * call-seq: all -> Array
 * 
 * Get Array of all Screen
 *
 *  Subtlext::Screen.all
 *  => [#<Subtlext::Screen:xxx>, #<Subtlext::Screen:xxx>]
 *
 *  Subtlext::Screen.all
 *  => []
 */

static VALUE
SubtlextScreenAll(VALUE self)
{
  return SubtlextScreens();
} /* }}} */

/* SubtlextSubtleCurrent {{{ */
/*
 * call-seq: current -> Subtlext::Screen
 *
 * Get current active Screen
 *
 *  Subtlext::Screen.current
 *  => #<Subtlext::Screen:xxx>
 */

static VALUE
SubtlextScreenCurrent(VALUE self)
{
  unsigned long *focus = NULL;
  VALUE screen = Qnil;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  /* Get current screen from current client or use the first */
  if((focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL)))
    {
      int *id = NULL;

      id     = (int *)subSharedPropertyGet(*focus, XA_CARDINAL, "SUBTLE_WINDOW_SCREEN", NULL);
      screen = SubtlextInstantiateScreen(*id);

      if(!NIL_P(screen)) SubtlextScreenUpdate(screen);

      free(focus);
      free(id);
    }

  return screen;
} /* }}} */

/* SubtlextScreenUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Screen properties
 *
 *  screen.update
 *  => nil
 */

static VALUE
SubtlextScreenUpdate(VALUE self)
{
  VALUE id = rb_iv_get(self, "@id");

  if(RTEST(id) && FIXNUM_P(id))
    {
      XRectangle geometry = { 0 };

      if(-1 != subSharedScreenFind(FIX2INT(id), &geometry))
        {
          rb_iv_set(self, "@geometry", SubtlextInstantiateGeometry(geometry.x,
            geometry.y, geometry.width, geometry.height));
        }
      else rb_raise(rb_eStandardError, "Failed finding screen");
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* SubtlextScreenClientList {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get Client on Screen
 *
 *  screen.clients
 *  => [ #<Subtlext::Client:xxx>, #<Subtlext::Client:xxx> ]
 *
 *  screen.clients
 *  => []
 */

static VALUE
SubtlextScreenClientList(VALUE self)
{
  int i, id, size = 0;
  Window *clients = NULL;
  VALUE array = Qnil;
  
  id      = FIX2INT(rb_iv_get(self, "@id"));
  array   = rb_ary_new();
  clients = subSharedClientList(&size);

  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          int *screen = (int *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
            "SUBTLE_WINDOW_SCREEN", NULL);

          if(id - 1 == *screen) ///< Check if screen matches
            rb_ary_push(array, SubtlextInstantiateClient(clients[i]));

          free(screen);
        }
      free(clients);
    }

  return array;
} /* }}} */

/* SubtlextScreenToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Screen object to String
 *
 *  puts screen
 *  => "0x0+800+600"
 */

static VALUE
SubtlextScreenToString(VALUE self)
{
  char buf[256];
  int x = 0, y = 0, width = 0, height = 0;

  x      = FIX2INT(rb_iv_get(self, "@x"));
  y      = FIX2INT(rb_iv_get(self, "@y"));
  width  = FIX2INT(rb_iv_get(self, "@width"));
  height = FIX2INT(rb_iv_get(self, "@height"));

  snprintf(buf, sizeof(buf), "%dx%d+%d+%d", x, y, width, height);

  return rb_str_new2(buf);
} /* }}} */

/* Subtle */

/* SubtlextSubtleKill {{{ */
static void
SubtlextSubtleKill(void *data)
{
  if(0 == --refcount) 
    {
      XCloseDisplay(display);
      display = NULL;
      subSharedLogDebug("Connection closed\n");
    }
} /* }}} */

/* SubtlextSubtleNew {{{ */
/*
 * call-seq: new(display, debug) -> Subtlext::Subtle
 *
 * Create a new Subtle object and open connection to X server
 *
 *  subtle = Subtlext::Subtle.new(":0")
 *  => #<Subtlext::Subtle:xxx>
 *
 *  subtle = Subtlext::Subtle.new(":0", true)
 *  <DEBUG> src/subtlext/subtlext.c:xxx: Connection opened (:0)
 *  => #<Subtlext::Subtle:xxx>
 */

static VALUE
SubtlextSubtleNew(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE disp = Qnil, data = Qnil;

#ifdef DEBUG
  VALUE dbg = Qfalse;

  rb_scan_args(argc, argv, "02", &disp, &dbg);

  if(Qtrue == dbg) debug++;
#else
  rb_scan_args(argc, argv, "01", &disp);
#endif /* DEBUG */

  SubtlextConnect(disp);

  /* Create instance */
  data = Data_Wrap_Struct(self, 0, SubtlextSubtleKill, display);
  rb_obj_call_init(data, 0, NULL);

  return data;
} /* }}} */

/* SubtlextSubtleNew2 {{{ */
/*
 * call-seq: new2(display) -> Subtlext::Subtle
 *
 * Create a new Subtle object - internal used only
 *
 *  subtle = Subtlext::Subtle.new(display)
 *  => #<Subtlext::Subtle:xxx>
 */

static VALUE
SubtlextSubtleNew2(VALUE self,
  VALUE disp)
{
  VALUE data = Qnil;

  display = (Display *)disp;
  refcount++;

  /* Create instance */
  data = Data_Wrap_Struct(self, 0, NULL, display);
  rb_obj_call_init(data, 0, NULL);

  return data;
} /* }}} */

/* SubtlextSubtleVersion {{{ */
/*
 * call-seq: version -> String
 *
 * Get the version of Subtlext
 *
 *  puts subtle.version 
 *  => "0.8.xxx"
 */

VALUE 
SubtlextSubtleVersion(VALUE self)
{
  return rb_str_new2(PKG_VERSION);
} /* }}} */

/* SubtlextSubtleDisplay {{{ */
/*
 * call-seq: display -> String
 *
 * Get the display name
 *
 *  subtle.display
 *  => ":0"
 */

VALUE 
SubtlextSubtleDisplay(VALUE self)
{
  return rb_str_new2(DisplayString(display));
} /* }}} */

/* SubtlextSubtleRunningAsk {{{ */
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

static VALUE
SubtlextSubtleRunningAsk(VALUE self)
{
  return subSharedSubtleRunning() ? Qtrue : Qfalse;
} /* }}} */

/* SubtlextSubtleSpawn {{{ */
/*
 * call-seq: spawn(cmd) -> nil
 *
 * Spawn a command
 *
 *  spawn("xterm")
 *  => nil
 */

static VALUE
SubtlextSubtleSpawn(VALUE self,
  VALUE cmd)
{
  if(RTEST(cmd) && T_STRING == rb_type(cmd))
    subSharedSpawn(RSTRING_PTR(cmd));
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* SubtlextSubtleFocus {{{ */
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

static VALUE
SubtlextSubtleFocus(VALUE self,
  VALUE name)
{
  int id = -1;
  Window win = 0;
  VALUE client = Qnil;

  if(RTEST(name) && -1 != ((id = subSharedClientFind(RSTRING_PTR(name), 
      NULL, &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, True);

      client = SubtlextInstantiateClient(win);
    }
  else rb_raise(rb_eStandardError, "Failed setting focus");

  return client;
} /* }}} */

/* SubtlextSubtleClientDel {{{ */
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

static VALUE
SubtlextSubtleClientDel(VALUE self,
  VALUE name)
{
  int id = -1;
  Window win = 0;

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

/* SubtlextSubtleGravityDel {{{ */
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

static VALUE
SubtlextSubtleGravityDel(VALUE self,
  VALUE value)
{
  return SubtlextKill(value, SUB_TYPE_GRAVITY);
} /* }}} */

/* SubtlextSubtleTagAdd {{{ */
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

static VALUE
SubtlextSubtleTagAdd(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil;

  if(NIL_P((tag = SubtlextFind(SUB_TYPE_TAG, value, False))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(value));
      subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, True);    

      tag = SubtlextInstantiateTag(RSTRING_PTR(value));
    }

  return tag;
} /* }}} */

/* SubtlextSubtleTagDel {{{ */
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

static VALUE
SubtlextSubtleTagDel(VALUE self,
  VALUE value)
{
  return SubtlextKill(value, SUB_TYPE_TAG);
} /* }}} */

/* SubtlextSubtleTrayDel {{{ */
/*
 * call-seq: del_tray(name) -> nil
 *
 * Delete Tray by given name or Tray object
 *
 *  subtle.del_sublet("subtle")
 *  => nil
 */

static VALUE
SubtlextSubtleTrayDel(VALUE self,
  VALUE value)
{
  return SubtlextKill(value, SUB_TYPE_TRAY);
} /* }}} */

/* SubtlextSubtleSubletDel {{{ */
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

static VALUE
SubtlextSubtleSubletDel(VALUE self,
  VALUE value)
{
  return SubtlextKill(value, SUB_TYPE_SUBLET);
} /* }}} */

/* SubtlextSubtleViewAdd {{{ */
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

static VALUE
SubtlextSubtleViewAdd(VALUE self,
  VALUE value)
{
  VALUE view = Qnil;

  if(NIL_P((view = SubtlextFind(SUB_TYPE_VIEW, value, False))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(value));
      subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data, False);

      view = SubtlextInstantiateView(RSTRING_PTR(value));
    }

  return view;
} /* }}} */

/* SubtlextSubtleViewDel {{{ */
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

static VALUE
SubtlextSubtleViewDel(VALUE self,
  VALUE value)
{
  return SubtlextKill(value, SUB_TYPE_VIEW);
} /* }}} */

/* SubtlextSubtleToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Subtle object to string
 *
 *  puts subtle
 *  => "subtle (0.8.xxx) on :0.0 (800x600)"
 */

static VALUE
SubtlextSubtleToString(VALUE self)
{
  char buf[256];

  snprintf(buf, sizeof(buf), "%s (%s) on %s (%dx%d)", 
    PKG_NAME, PKG_VERSION, DisplayString(display), 
    DisplayWidth(display, DefaultScreen(display)),
    DisplayHeight(display, DefaultScreen(display)));

  return rb_str_new2(buf);
} /* }}} */

/* SubtlextSubtleReload {{{ */
/*
 * call-seq: reload -> nil
 *
 * Force Subtle to reload the config
 *
 *  subtle.reload
 *  => nil
 */

static VALUE
SubtlextSubtleReload(VALUE self)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_RELOAD", data, True);

  return Qnil;
} /* }}} */

/* SubtlextSubtleQuit {{{ */
/*
 * call-seq: quit -> nil
 *
 * Force Subtle to exit
 *
 *  subtle.reload
 *  => nil
 */

static VALUE
SubtlextSubtleQuit(VALUE self)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_QUIT", data, True);

  return Qnil;
} /* }}} */

/* Sublet */

/* SubtlextSubletInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Sublet
 *
 * Create new Sublet object
 *
 *  sublet = Subtlext::Sublet.new("subtle")
 *  => #<Subtlext::Sublet:xxx> * 
 */

static VALUE
SubtlextSubletInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  return self;
} /* }}} */

/* SubtlextSubletFind {{{ */
/*
 * call-seq: find(value) -> Subtlext::Sublet or nil
 *           [value]     -> Subtlext::Sublet or nil
 *
 * Find Sublet by a given value which can be of following type:
 *
 * [fixnum] Array id
 * [string] Match against name of Sublet
 * [symbol] :all for an array of all Sublet
 *
 *  Subtlext::Sublet.find("subtle")
 *  => #<Subtlext::Sublet:xxx>
 *
 *  Subtlext::Sublet[1]
 *  => #<Subtlext::Sublet:xxx>
 *
 *  Subtlext::Sublet["subtle"]
 *  => nil
 */

static VALUE
SubtlextSubletFind(VALUE self,
  VALUE value)
{
  return SubtlextFind(SUB_TYPE_SUBLET, value, True);

} /* }}} */

/* SubtlextSubletAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get Array of Sublet
 *
 *  Subtlext::Sublet.all
 *  => [#<Subtlext::Sublet:xxx>, #<Subtlext::Sublet:xxx>]
 *
 *  Subtlext::Sublet.all
 *  => []
 */

static VALUE
SubtlextSubletAll(VALUE self)
{
  int i, size = 0;
  char **sublets = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection
  
  /* Fetch data */
  meth    = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("Sublet"));
  sublets = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_SUBLET_LIST", &size);
  array   = rb_ary_new2(size);

  /* Populate array */
  if(sublets)
    {
      for(i = 0; i < size; i++)
        {
          VALUE s = rb_funcall(klass, meth, 1, rb_str_new2(sublets[i]));

          rb_iv_set(s, "@id", INT2FIX(i));
          rb_ary_push(array, s);
        }

      XFreeStringList(sublets);
    }

  return array;
} /* }}} */

/* SubtlextSubletUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Force Sublet to update it's data
 *
 *  sublet.update
 *  => nil
 */

static VALUE
SubtlextSubletUpdate(VALUE self)
{
  int id = -1;
  VALUE name = rb_iv_get(self, "@name");

  if(RTEST(name) && -1 != ((id = subSharedSubletFind(RSTRING_PTR(name), NULL))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Send data */
      data.l[0] = id;

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_UPDATE", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed finding sublet");

  return Qnil;
} /* }}} */

/* SubtlextSubletDataReader {{{ */
/*
 * call-seq: data -> String
 *
 * Get data of Sublet
 *
 *  puts sublet.data
 *  => "subtle" 
 */

static VALUE
SubtlextSubletDataReader(VALUE self)
{
  VALUE id = rb_iv_get(self, "@id");
      
  return RTEST(id) ? id : Qnil;
} /* }}} */

/* SubtlextSubletDataWriter {{{ */
/*
 * call-seq: data= -> String
 *
 * Set data of sublet
 *
 *  sublet.data = "subtle" 
 *  => "subtle" 
 */

static VALUE
SubtlextSubletDataWriter(VALUE self,
  VALUE value)
{
  if(T_STRING == rb_type(value))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      VALUE id = rb_iv_get(self, "@id");

      snprintf(data.b, sizeof(data.b), "%c%s",
        (char)NUM2LONG(id), RSTRING_PTR(value));

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_DATA", data, True);
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return value;
} /* }}} */

/* SubtlextSubletToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Sublet object to String
 *
 *  puts sublet
 *  => sublet
 */

static VALUE
SubtlextSubletToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextSubletKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a Sublet
 *
 *  sublet.kill
 *  => nil
 */

static VALUE
SubtlextSubletKill(VALUE self)
{
  return SubtlextKill(self, SUB_TYPE_SUBLET);
} /* }}} */

/* Tag */

/* SubtlextTagInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Tag
 *
 * Create new Tag object
 *
 *  tag = Subtlext::Tag.new("subtle")
 *  => #<Subtlext::Tag:xxx>
 */

static VALUE
SubtlextTagInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  return self;
} /* }}} */

/* SubtlextTagFind {{{ */
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

static VALUE
SubtlextTagFind(VALUE self,
  VALUE value)
{
  return SubtlextFind(SUB_TYPE_TAG, value, True);
} /* }}} */

/* SubtlextTagAll {{{ */
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

static VALUE
SubtlextTagAll(VALUE self)
{
  int i, size = 0;
  char **tags = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection
  
  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Tag"));
  tags  = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
  array = rb_ary_new2(size);

  /* Populate array */
  if(tags)
    {
      for(i = 0; i < size; i++)
        {
          VALUE t = rb_funcall(klass, meth, 1, rb_str_new2(tags[i]));

          rb_iv_set(t, "@id", INT2FIX(i));
          rb_ary_push(array, t);
        }

      XFreeStringList(tags);
    }

  return array;
} /* }}} */

/* SubtlextTagUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Tag properties
 *
 *  tag.update
 *  => nil
 */

static VALUE
SubtlextTagUpdate(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  if(RTEST(name) && T_STRING == rb_type(name))
    {
      int id = -1;

      /* Create tag if needed */
      if(-1 == (id = subSharedTagFind(RSTRING_PTR(name), NULL)))
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };

          snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
          subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, True);    

          id = subSharedTagFind(RSTRING_PTR(name), NULL);
        }

      /* Final check */
      if(-1 != id)
        {
          rb_iv_set(self, "@id", INT2FIX(id));
        }
      else rb_raise(rb_eStandardError, "Failed finding tag");  
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* SubtlextTagClients {{{ */
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

static VALUE
SubtlextTagClients(VALUE self)
{
  return SubtlextWindowTagAssoc(self, SUB_TYPE_CLIENT);
} /* }}} */

/* SubtlextTagViews {{{ */
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

static VALUE
SubtlextTagViews(VALUE self)
{
  return SubtlextWindowTagAssoc(self, SUB_TYPE_VIEW);
} /* }}} */

/* SubtlextTagToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Tag object to String
 *
 *  puts tag
 *  => "subtle" 
 */

static VALUE
SubtlextTagToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextTagKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a Tag
 *
 *  tag.kill
 *  => nil
 */

static VALUE
SubtlextTagKill(VALUE self)
{
  return SubtlextKill(self, SUB_TYPE_TAG);
} /* }}} */

/* Tray */

/* SubtlextTrayInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::Tray
 *
 * Create new Tray object
 *
 *  tag = Subtlext::Tray.new("subtle")
 *  => #<Subtlext::Tray:xxx>
 */

static VALUE
SubtlextTrayInit(VALUE self,
  VALUE win)
{
  if(!FIXNUM_P(win) && T_BIGNUM != rb_type(win))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",    Qnil);
  rb_iv_set(self, "@win",   win);
  rb_iv_set(self, "@name",  Qnil);
  rb_iv_set(self, "@klass", Qnil);
  rb_iv_set(self, "@title", Qnil);

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  return self;
} /* }}} */

/* SubtlextTrayFind {{{ */
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
 *  Subtlext::Tray.find("subtle")
 *  => #<Subtlext::Tray:xxx>
 *
 *  Subtlext::Tray[1]
 *  => #<Subtlext::Tray:xxx>
 *
 *  Subtlext::Tray["subtle"]
 *  => nil
 */

static VALUE
SubtlextTrayFind(VALUE self,
  VALUE name)
{
  return SubtlextFind(SUB_TYPE_TRAY, name, True);
} /* }}} */

/* SubtlextTrayAll {{{ */
/*
 * call-seq: all -> Array
 *
 * Get Array of all Tray
 *
 *  Subtlext::Tray.all
 *  => [#<Subtlext::Tray:xxx>, #<Subtlext::Tray:xxx>]
 *
 *  Subtlext::Tray.all
 *  => []
 */

static VALUE
SubtlextTrayAll(VALUE self)
{
  int i, size = 0;
  Window *trays = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection
  
  /* Fetch data */
  meth  = rb_intern("new");
  klass = rb_const_get(mod, rb_intern("Tray"));
  trays = subSharedTrayList(&size);
  array = rb_ary_new2(size);

  /* Populate array */
  if(trays)
    {
      for(i = 0; i < size; i++)
        {
          VALUE t = rb_funcall(klass, meth, 1, LONG2NUM(trays[i]));

          if(!NIL_P(t)) SubtlextTrayUpdate(t);

          rb_ary_push(array, t);
        }

      free(trays);
    }

  return array;
} /* }}} */

/* SubtlextTrayUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Tray properties
 *
 *  tray.update
 *  => nil
 */

static VALUE
SubtlextTrayUpdate(VALUE self)
{
  VALUE win = rb_iv_get(self, "@win");

  if(RTEST(win) && (T_FIXNUM == rb_type(win) || T_BIGNUM == rb_type(win)))
    {
      int id = 0;
      char buf[20] = { 0 };

      snprintf(buf, sizeof(buf), "%#lx", NUM2LONG(win));
      if(-1 != (id = subSharedTrayFind(buf, NULL, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS))))
        {
          char *title = NULL, *wmname = NULL, *wmclass = NULL;

          XFetchName(display, NUM2LONG(win), &title);
          subSharedPropertyClass(NUM2LONG(win), &wmname, &wmclass);

          /* Set properties */
          rb_iv_set(self, "@id",    INT2FIX(id));
          rb_iv_set(self, "@title", rb_str_new2(title));
          rb_iv_set(self, "@name",  rb_str_new2(wmname));
          rb_iv_set(self, "@klass", rb_str_new2(wmclass));

          free(title);
          free(wmname);
          free(wmclass);
        }
      else rb_raise(rb_eStandardError, "Failed finding tray");  
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* SubtlextTrayFocus {{{ */
/*
 * call-seq: focus -> nil
 *
 * Set focus to Tray
 *
 *  tray.focus
 *  => nil
 */

static VALUE
SubtlextTrayFocus(VALUE self)
{
  return SubtlextWindowFocus(self);
} /* }}} */

/* SubtlextTrayFocusAsk {{{ */
/*
 * call-seq: has_focus? -> true or false
 *
 * Check if Tray has focus
 *
 *  tray.has_focus?
 *  => true
 *
 *  tray.has_focus?
 *  => false
 */

static VALUE
SubtlextTrayFocusAsk(VALUE self)
{
  return SubtlextWindowFocusAsk(self);
} /* }}} */

/* SubtlextTrayToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Tray object to String
 *
 *  puts tray
 *  => "subtle" 
 */

static VALUE
SubtlextTrayToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextTrayKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a Tray
 *
 *  tray.kill
 *  => nil
 */

static VALUE
SubtlextTrayKill(VALUE self)
{
  return SubtlextKill(self, SUB_TYPE_TRAY);
} /* }}} */

/* View */

/* SubtlextViewInit {{{ */
/*
 * call-seq: new(name) -> Subtlext::View
 *
 * Create a new View object
 *
 *  view = Subtlext::View.new("subtle")
 *  => #<Subtlext::View:xxx>
 */

static VALUE
SubtlextViewInit(VALUE self,
  VALUE name)
{
  if(T_STRING != rb_type(name))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@win",  Qnil);
  rb_iv_set(self, "@name", name);

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  return self;
} /* }}} */

/* SubtlextViewFind {{{ */
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

static VALUE
SubtlextViewFind(VALUE self,
  VALUE name)
{
  return SubtlextFind(SUB_TYPE_VIEW, name, True);
} /* }}} */

/* SubtlextViewCurrent {{{ */
/*
 * call-seq: current -> Subtlext::View
 *
 * Get current active View
 *
 *  Subtlext::View.current
 *  => #<Subtlext::View:xxx>
 */

static VALUE
SubtlextViewCurrent(VALUE self)
{
  int size = 0;
  char **names = NULL;
  unsigned long *cv = NULL;
  Window *views = NULL;
  VALUE view = Qnil;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection
  
  /* Get current view */
  names = subSharedPropertyStrings(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
  cv    = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  views = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW, 
    "_NET_VIRTUAL_ROOTS", NULL);

  /* Create instance */
  view = SubtlextInstantiateView(names[*cv]);
  rb_iv_set(view, "@win", LONG2NUM(views[*cv]));

  if(!NIL_P(view)) SubtlextViewUpdate(view);

  XFreeStringList(names);
  free(views);
  free(cv);
      
  return view;
} /* }}} */

/* SubtlextViewAll {{{ */
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

static VALUE
SubtlextViewAll(VALUE self)
{
  int i, size = 0;
  char **names = NULL;
  Window *views = NULL;
  VALUE meth = Qnil, klass = Qnil, array = Qnil;

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection
  
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

/* SubtlextViewUpdate {{{ */
/*
 * call-seq: Update -> nil
 *
 * Update View properties
 *
 *  view.update
 *  => nil
 */

static VALUE
SubtlextViewUpdate(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  if(RTEST(name) && T_STRING == rb_type(name))
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

      /* Final check */
      if(-1 != id)
        {
          rb_iv_set(self, "@id", INT2FIX(id));
        }
      else rb_raise(rb_eStandardError, "Failed finding view");  
    }
  else rb_raise(rb_eStandardError, "Failed finding view");  

  return Qnil;
} /* }}} */

/* SubtlextViewClients {{{ */
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

static VALUE
SubtlextViewClients(VALUE self)
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

          if(*flags1 & *flags2) ///< Check if there are common tags
            {
              if(!NIL_P(client = rb_funcall(klass, meth, 1, LONG2NUM(clients[i]))))
                {
                  SubtlextClientUpdate(client);
                  rb_ary_push(array, client); 
                }
            }

          free(flags2);
        }

      free(clients);
    }

  return array;
} /* }}} */

/* SubtlextViewJump {{{ */
/*
 * call-seq: jump -> nil
 *
 * Jump to this View
 *
 *  view.jump
 *  => nil
 */

static VALUE
SubtlextViewJump(VALUE self)
{
  VALUE id = rb_iv_get(self, "@id");
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  data.l[0] = FIX2INT(id);

  subSharedMessage(DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", data, True);

  return Qnil;
} /* }}} */

/* SubtlextViewCurrentAsk {{{ */
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

static VALUE
SubtlextViewCurrentAsk(VALUE self)
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

/* SubtlextViewToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert View object to String
 *
 *  puts view
 *  => "subtle" 
 */

static VALUE
SubtlextViewToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextViewKill {{{ */
/*
 * call-seq: kill -> nil
 *
 * Kill a View
 *
 *  view.kill
 *  => nil
 */

static VALUE
SubtlextViewKill(VALUE self)
{
  return SubtlextKill(self, SUB_TYPE_VIEW);
} /* }}} */

/* Init_subtlext {{{ */
/*
 * Subtlext is the module of the extension
 */
void 
Init_subtlext(void)
{
  VALUE client = Qnil, geometry = Qnil, gravity = Qnil, screen = Qnil;
  VALUE subtle = Qnil, sublet = Qnil, tag = Qnil, tray = Qnil, view = Qnil;

  /* Module: sublext */
  mod = rb_define_module("Subtlext");

  /*
   * Document-class: Subtlext::Client
   *
   * Client class for interaction with clients
   */

  client = rb_define_class_under(mod, "Client", rb_cObject);

  /* Client id */
  rb_define_attr(client, "id",       1, 0); 

  /* Window id */
  rb_define_attr(client, "win",      1, 0);

  /* WM_CLASS */
  rb_define_attr(client, "klass",    1, 0);

  /* WM_NAME */
  rb_define_attr(client, "title",    1, 0);

  /* WM_NAME */
  rb_define_attr(client, "name",     1, 0);

  /* Bitfield of window states */
  rb_define_attr(client, "flags",    1, 0);

  rb_define_singleton_method(client, "find",    SubtlextClientFind,    1);
  rb_define_singleton_method(client, "[]",      SubtlextClientFind,    1);
  rb_define_singleton_method(client, "current", SubtlextClientCurrent, 0);
  rb_define_singleton_method(client, "all",     SubtlextClientAll,     0);

  rb_define_method(client, "initialize",   SubtlextClientInit,            1);
  rb_define_method(client, "update",       SubtlextClientUpdate,          0);

  rb_define_method(client, "tags",         SubtlextWindowTagList,         0);
  rb_define_method(client, "has_tag?",     SubtlextWindowTagAsk,          1);
  rb_define_method(client, "tag",          SubtlextWindowTagAdd,          1);
  rb_define_method(client, "untag",        SubtlextWindowTagDel,          1);
  rb_define_method(client, "+",            SubtlextWindowTagAdd,          1);
  rb_define_method(client, "-",            SubtlextWindowTagDel,          1);

  rb_define_method(client, "views",        SubtlextClientViewList,        0);
  rb_define_method(client, "toggle_full",  SubtlextClientToggleFull,      0);
  rb_define_method(client, "toggle_float", SubtlextClientToggleFloat,     0);
  rb_define_method(client, "toggle_stick", SubtlextClientToggleStick,     0);
  rb_define_method(client, "is_full?",     SubtlextClientStateFullAsk,    0);
  rb_define_method(client, "is_float?",    SubtlextClientStateFloatAsk,   0);
  rb_define_method(client, "is_stick?",    SubtlextClientStateStickAsk,   0);
  rb_define_method(client, "raise",        SubtlextClientRestackRaise,    0);
  rb_define_method(client, "lower",        SubtlextClientRestackLower,    0);
  rb_define_method(client, "up",           SubtlextClientMatchUp,         0);
  rb_define_method(client, "left",         SubtlextClientMatchLeft,       0);
  rb_define_method(client, "right",        SubtlextClientMatchRight,      0);
  rb_define_method(client, "down",         SubtlextClientMatchDown,       0);
  rb_define_method(client, "focus",        SubtlextClientFocus,           0);
  rb_define_method(client, "has_focus?",   SubtlextClientFocusAsk,        0);
  rb_define_method(client, "to_str",       SubtlextClientToString,        0);
  rb_define_method(client, "gravity",      SubtlextClientGravityReader,   0);
  rb_define_method(client, "gravity=",     SubtlextClientGravityWriter,   1);
  rb_define_method(client, "screen",       SubtlextClientScreenReader,    0);
  rb_define_method(client, "screen=",      SubtlextClientScreenWriter,    1);  
  rb_define_method(client, "geometry",     SubtlextClientGeometryReader,  0);
  rb_define_method(client, "geometry=",    SubtlextClientGeometryWriter, -1);
  rb_define_method(client, "alive?",       SubtlextClientAliveAsk,        0);
  rb_define_method(client, "kill",         SubtlextClientKill,            0);

  rb_define_alias(client, "save", "update");
  rb_define_alias(client, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Geometry
   *
   * Geometry class for sizes
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

  rb_define_method(geometry, "initialize", SubtlextGeometryInit,     -1);
  rb_define_method(geometry, "to_a",       SubtlextGeometryToArray,   0);
  rb_define_method(geometry, "to_hash",    SubtlextGeometryToHash,    0);
  rb_define_method(geometry, "to_str",     SubtlextGeometryToString,  0);

  rb_define_alias(geometry, "to_h", "to_hash");
  rb_define_alias(geometry, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Gravity
   *
   * Gravity class for Client placement
   */

  gravity = rb_define_class_under(mod, "Gravity", rb_cObject);

  /* Gravity id */
  rb_define_attr(gravity, "id",       1, 0);

  /* Name of the gravity */
  rb_define_attr(gravity, "name",     1, 0);

  /* Geometry */
  rb_define_attr(gravity, "geometry", 0, 0);

  rb_define_singleton_method(gravity, "find", SubtlextGravityFind, 1);
  rb_define_singleton_method(gravity, "[]",   SubtlextGravityFind, 1);
  rb_define_singleton_method(gravity, "all",  SubtlextGravityAll,  0);

  rb_define_method(gravity, "initialize", SubtlextGravityInit,     1);
  rb_define_method(gravity, "update",     SubtlextGravityUpdate,   0);
  rb_define_method(gravity, "geometry",   SubtlextGravityGeometry, 0);
  rb_define_method(gravity, "to_str",     SubtlextGravityToString, 0);
  rb_define_method(gravity, "to_sym",     SubtlextGravityToSym,    0);
  rb_define_method(gravity, "kill",       SubtlextGravityKill,     0);

  rb_define_alias(gravity, "save", "update");
  rb_define_alias(gravity, "to_s", "to_str");  

  /*
   * Document-class: Subtlext::Screen
   *
   * Screen class for interaction with screens
   */

  screen = rb_define_class_under(mod, "Screen", rb_cObject);

  /* Screen id */
  rb_define_attr(screen, "id",       1, 0);

  /* Geometry */
  rb_define_attr(screen, "geometry", 1, 0);

  rb_define_singleton_method(screen, "find",    SubtlextScreenFind,    1);
  rb_define_singleton_method(screen, "[]",      SubtlextScreenFind,    1);
  rb_define_singleton_method(screen, "current", SubtlextScreenCurrent, 0);
  rb_define_singleton_method(screen, "all",     SubtlextScreenAll,     0);

  rb_define_method(screen, "initialize", SubtlextScreenInit,       1);
  rb_define_method(screen, "update",     SubtlextScreenUpdate,     0);
  rb_define_method(screen, "clients",    SubtlextScreenClientList, 0);
  rb_define_method(screen, "to_str",     SubtlextScreenToString,   0);

  rb_define_alias(screen, "save", "update");
  rb_define_alias(screen, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Subtle
   *
   * Subtle class for interaction with the window manager
   */

  subtle = rb_define_class_under(mod, "Subtle", rb_cObject);
  rb_define_singleton_method(subtle, "new",  SubtlextSubtleNew,  -1);
  rb_define_singleton_method(subtle, "new2", SubtlextSubtleNew2,  1);

  rb_define_method(subtle, "version",        SubtlextSubtleVersion,     0);
  rb_define_method(subtle, "display",        SubtlextSubtleDisplay,     0);

  rb_define_method(subtle, "clients",        SubtlextClientAll,         0);
  rb_define_method(subtle, "gravities",      SubtlextGravityAll,        0);
  rb_define_method(subtle, "screens",        SubtlextScreenAll,         0);
  rb_define_method(subtle, "sublets",        SubtlextSubletAll,         0);
  rb_define_method(subtle, "tags",           SubtlextTagAll,            0);
  rb_define_method(subtle, "trays",          SubtlextTrayAll,           0);
  rb_define_method(subtle, "views",          SubtlextViewAll,           0);

  rb_define_method(subtle, "find_client",    SubtlextClientFind,        1);
  rb_define_method(subtle, "find_gravity",   SubtlextGravityFind,       1);
  rb_define_method(subtle, "find_screen",    SubtlextScreenFind,        1);
  rb_define_method(subtle, "find_sublet",    SubtlextSubletFind,        1);
  rb_define_method(subtle, "find_tag",       SubtlextTagFind,           1);
  rb_define_method(subtle, "find_tray",      SubtlextTrayFind,          1);
  rb_define_method(subtle, "find_view",      SubtlextViewFind,          1);

  rb_define_method(subtle, "focus",          SubtlextSubtleFocus,       1);
  rb_define_method(subtle, "add_tag",        SubtlextSubtleTagAdd,      1);
  rb_define_method(subtle, "add_view",       SubtlextSubtleViewAdd,     1);

  rb_define_method(subtle, "del_client",     SubtlextSubtleClientDel,   1);
  rb_define_method(subtle, "del_gravity",    SubtlextSubtleGravityDel,  1);
  rb_define_method(subtle, "del_sublet",     SubtlextSubtleSubletDel,   1);
  rb_define_method(subtle, "del_tag",        SubtlextSubtleTagDel,      1);
  rb_define_method(subtle, "del_tray",       SubtlextSubtleTrayDel,     1);
  rb_define_method(subtle, "del_view",       SubtlextSubtleViewDel,     1);

  rb_define_method(subtle, "current_view",   SubtlextViewCurrent,       0);
  rb_define_method(subtle, "current_client", SubtlextClientCurrent,     0);
  rb_define_method(subtle, "current_screen", SubtlextScreenCurrent,     0);

  rb_define_method(subtle, "running?",       SubtlextSubtleRunningAsk,  0);
  rb_define_method(subtle, "reload",         SubtlextSubtleReload,      0);
  rb_define_method(subtle, "quit",           SubtlextSubtleQuit,        0);
  rb_define_method(subtle, "spawn",          SubtlextSubtleSpawn,       1);
  rb_define_method(subtle, "to_str",         SubtlextSubtleToString,    0);

  rb_define_alias(subtle, "to_s", "to_str");
  rb_define_alias(subtle, "kill", "quit");

  /*
   * Document-class: Subtlext::Sublet
   *
   * Sublet class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);

  /* Name of the sublet */
  rb_define_attr(sublet, "name", 1, 0);

  rb_define_singleton_method(sublet, "find", SubtlextSubletFind, 1);
  rb_define_singleton_method(sublet, "[]",   SubtlextSubletFind, 1);
  rb_define_singleton_method(sublet, "all",  SubtlextSubletAll,  0);

  rb_define_method(sublet, "initialize", SubtlextSubletInit,       1);
  rb_define_method(sublet, "update",     SubtlextSubletUpdate,     0);
  rb_define_method(sublet, "data",       SubtlextSubletDataReader, 0);
  rb_define_method(sublet, "data=",      SubtlextSubletDataWriter, 1);  
  rb_define_method(sublet, "to_str",     SubtlextSubletToString,   0);
  rb_define_method(sublet, "kill",       SubtlextSubletKill,       0);

  rb_define_alias(sublet, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Tag
   *
   * Tag class for interaction with tags
   */

  tag = rb_define_class_under(mod, "Tag", rb_cObject);

  /* Tag id */
  rb_define_attr(tag,   "id",   1, 0);

  /* Name of the tag */
  rb_define_attr(tag,   "name", 1, 0);

  rb_define_singleton_method(tag, "find", SubtlextTagFind, 1);
  rb_define_singleton_method(tag, "[]",   SubtlextTagFind, 1);
  rb_define_singleton_method(tag, "all",  SubtlextTagAll,  0);

  rb_define_method(tag, "initialize", SubtlextTagInit,     1);
  rb_define_method(tag, "update",     SubtlextTagUpdate,   0);
  rb_define_method(tag, "clients",    SubtlextTagClients,  0);
  rb_define_method(tag, "views",      SubtlextTagViews,    0);
  rb_define_method(tag, "to_str",     SubtlextTagToString, 0);
  rb_define_method(tag, "kill",       SubtlextTagKill,     0);

  rb_define_alias(tag, "save", "update");
  rb_define_alias(tag, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Tray
   *
   * Tag class for interaction with trays
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

  rb_define_singleton_method(tray, "find", SubtlextTrayFind, 1);
  rb_define_singleton_method(tray, "[]",   SubtlextTrayFind, 1);
  rb_define_singleton_method(tray, "all",  SubtlextTrayAll,  0);

  rb_define_method(tray, "initialize", SubtlextTrayInit,     1);
  rb_define_method(tray, "update",     SubtlextTrayUpdate,   0);
  rb_define_method(tray, "focus",      SubtlextTrayFocus,    0);
  rb_define_method(tray, "has_focus?", SubtlextTrayFocusAsk, 0);
  rb_define_method(tray, "to_str",     SubtlextTrayToString, 0);
  rb_define_method(tray, "kill",       SubtlextTrayKill,     0);

  rb_define_alias(tray, "to_s", "to_str");

  /*
   * Document-class: Subtlext::View
   *
   * View class for interaction with views
   */

  view = rb_define_class_under(mod, "View", rb_cObject);

  /* View id */
  rb_define_attr(view, "id",   1, 0);

  /* Window id */
  rb_define_attr(view, "win",  1, 0);

  /* Name of the view */
  rb_define_attr(view, "name", 1, 0);

  rb_define_singleton_method(view, "find",    SubtlextViewFind,    1);
  rb_define_singleton_method(view, "[]",      SubtlextViewFind,    1);
  rb_define_singleton_method(view, "current", SubtlextViewCurrent, 0);
  rb_define_singleton_method(view, "all",     SubtlextViewAll,     0);

  rb_define_method(view, "initialize", SubtlextViewInit,          1);
  rb_define_method(view, "update",     SubtlextViewUpdate,        0);

  rb_define_method(view, "tags",       SubtlextWindowTagList,     0);
  rb_define_method(view, "has_tag?",   SubtlextWindowTagAsk,      1);
  rb_define_method(view, "tag",        SubtlextWindowTagAdd,      1);
  rb_define_method(view, "untag",      SubtlextWindowTagDel,      1);
  rb_define_method(view, "+",          SubtlextWindowTagAdd,      1);
  rb_define_method(view, "-",          SubtlextWindowTagDel,      1);

  rb_define_method(view, "clients",    SubtlextViewClients,       0);
  rb_define_method(view, "jump",       SubtlextViewJump,          0);
  rb_define_method(view, "current?",   SubtlextViewCurrentAsk,    0);
  rb_define_method(view, "to_str",     SubtlextViewToString,      0);
  rb_define_method(view, "kill",       SubtlextViewKill,          0);

  rb_define_alias(view, "save", "update");
  rb_define_alias(view, "to_s", "to_str");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
