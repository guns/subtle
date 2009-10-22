
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

#ifdef DEBUG
int debug = 0;
#endif /* DEBUG */

/* Prototypes {{{ */
static VALUE SubtlextSubtleClientCurrent(VALUE self);
static VALUE SubtlextClientUpdate(VALUE self);
static VALUE SubtlextGravityUpdate(VALUE self);
static VALUE SubtlextScreenUpdate(VALUE self);
static VALUE SubtlextTagUpdate(VALUE self);
static VALUE SubtlextViewUpdate(VALUE self);
/* }}} */

/* Flags {{{ */
#define SUB_TYPE_CLIENT  0           ///< Client
#define SUB_TYPE_VIEW    1           ///< View
#define SUB_TYPE_TAG     2           ///< Tag
#define SUB_TYPE_SCREEN  3           ///< Screen

#define SUB_ACTION_TAG   0           ///< Tag
#define SUB_ACTION_UNTAG 1           ///< Untag
/* }}} */

/* SubtlextInstantiateClient {{{ */
static VALUE
SubtlextInstantiateClient(Window win)
{
  VALUE klass = Qnil, client = Qnil;

  /* Create new instance */
  klass  = rb_const_get(mod, rb_intern("Client"));
  client = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(win));

  SubtlextClientUpdate(client);

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
SubtlextInstantiateGravity(int id)
{
  VALUE klass = Qnil, gravity = Qnil;

  /* Create new instance */
  klass   = rb_const_get(mod, rb_intern("Gravity"));
  gravity = rb_funcall(klass, rb_intern("new"), 1, INT2FIX(id)); 

  SubtlextGravityUpdate(gravity);

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

  SubtlextScreenUpdate(screen);

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

  SubtlextTagUpdate(tag);

  return tag;
} /* }}} */

/* SubtlextInstantiateView {{{ */
static VALUE
SubtlextInstantiateView(char *name)
{
  VALUE klass = Qnil, view = Qnil;

  /* Create new instance */
  klass = rb_const_get(mod, rb_intern("View"));
  view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(name));

  SubtlextViewUpdate(view);

  return view;
} /* }}} */

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
  char *klass = NULL;
  VALUE object = Qnil;

  assert(SUB_TYPE_CLIENT == type || SUB_TYPE_TAG == type ||
    SUB_TYPE_VIEW == type || SUB_TYPE_SCREEN == type);

  /* Get object type */
  switch(type)
    {
      case SUB_TYPE_CLIENT: klass = "Client"; break;
      case SUB_TYPE_TAG:    klass = "Tag";    break;
      case SUB_TYPE_SCREEN: klass = "Screen"; break;
      case SUB_TYPE_VIEW:   klass = "View";   break;
    }

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM: /* {{{ */
        if(SUB_TYPE_SCREEN == type)
          object = SubtlextInstantiateScreen(FIX2INT(value));
        break; /* }}} */
      case T_STRING: /* {{{ */
        /* Instantiate objects */
        switch(type)
          {
            case SUB_TYPE_CLIENT:
                {
                  Window win = None;

                  if(-1 != subSharedClientFind(RSTRING_PTR(value), 
                      &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
                    object = SubtlextInstantiateClient(win);
                }
              break;
            case SUB_TYPE_TAG:
              object = SubtlextInstantiateTag(RSTRING_PTR(value));
              break;
            case SUB_TYPE_VIEW:
              object = SubtlextInstantiateView(RSTRING_PTR(value));
              break;
          }
        break; /* }}} */
      case T_OBJECT: /* {{{ */
          {
            /* Check object type */
            if(rb_obj_is_instance_of(value, 
                rb_const_get(mod, rb_intern(klass)))) 
              object = value;
          }
        break; /* }}} */
      default: /* {{{ */
        if(exception)
          {
            rb_raise(rb_eArgError, "Unknwon value type `%s'", rb_obj_classname(value));

            return Qnil;
          } /* }}} */
    }

  if(NIL_P(object) && exception)
    rb_raise(rb_eArgError, "Failed finding `%s'", klass);

  return object;
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

/* SubtlextGetScreens {{{ */
static VALUE
SubtlextGetScreens(void)
{
  int n = 0;
  VALUE method = Qnil, klass = Qnil, array = Qnil, screen = Qnil, geometry = Qnil; 

#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
  int xinerama_event = 0, xinerama_error = 0;
#endif /* HAVE_X11_EXTENSIONS_XINERAMA_H */

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
  int i, id = 0, size = 0, match = 0, score = 0, *gravity1 = NULL;
  Window *clients = NULL, *views = NULL, found = None;
  VALUE win = Qnil, client = Qnil;
  unsigned long *cv = NULL, *flags1 = NULL;

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
      gravity1 = (int *)subSharedPropertyGet(NUM2LONG(win), XA_CARDINAL,
        "SUBTLE_WINDOW_GRAVITY", NULL);

      /* Iterate once to find a client score-based */
      for(i = 0; 100 != match && i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL,
            "SUBTLE_WINDOW_TAGS", NULL);

          if(win != clients[i] && *flags1 & *flags2) ///< Check if there are common tags
            {
              int *gravity2 = (int *)subSharedPropertyGet(clients[i], XA_CARDINAL,
                "SUBTLE_WINDOW_GRAVITY", NULL);

              if(match < (score = subSharedMatch(type, *gravity1, *gravity2)))
                {
                  match = score;
                  found = clients[i];
                  id    = i;
                }

              free(gravity2);
            }

          free(flags2);
        }

      if(found) client = SubtlextInstantiateClient(found);

      free(flags1);
      free(gravity1);
      free(clients);
      free(cv);
    }

  return client;
} /* }}} */

/* SubtlextTag {{{ */
static VALUE
SubtlextTag(VALUE self,
  VALUE value,
  int type,
  int action)
{
  VALUE tag = Qnil;

  assert(SUB_TYPE_CLIENT == type || SUB_TYPE_TAG == type);

  /* Find tag */
  if(Qnil != (tag = SubtlextFind(SUB_TYPE_TAG, value, True)))
    {
      VALUE oid = Qnil, tid = Qnil;

      oid = rb_iv_get(self, "@id");
      tid = rb_iv_get(tag, "@id");

      if(Qnil != oid && Qnil != tid)
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };

          data.l[0] = FIX2LONG(oid);
          data.l[1] = FIX2LONG(tid);
          data.l[2] = SUB_TYPE_CLIENT == type ? 0 : 1; ///< Get type

          subSharedMessage(DefaultRootWindow(display),
            SUB_ACTION_TAG == action ? "SUBTLE_WINDOW_TAG" : "SUBTLE_WINDOW_UNTAG",
            data, True);
        }
    }

  return Qnil;
} /* }}} */

/* SubtlextTagHas {{{ */
static VALUE
SubtlextTagHas(VALUE self,
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

/* SubtlextClientUpdate {{{ */
/*
 * call-seq: update -> nil
 *
 * Update Client properties
 *
 *  client.update
 *  => nil
 */

VALUE
SubtlextClientUpdate(VALUE self)
{
  VALUE win = rb_iv_get(self, "@win");

  if(RTEST(win) && (T_FIXNUM == rb_type(win) || T_BIGNUM == rb_type(win)))
    {
      int id = 0;
      char buf[20] = { 0 };

      snprintf(buf, sizeof(buf), "%#lx", NUM2LONG(win));
      if(-1 != (id = subSharedClientFind(buf, NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS))))
        {
          int *flags = NULL;
          char *title = NULL, *wmname = NULL, *wmclass = NULL;

          flags = (int *)subSharedPropertyGet(NUM2LONG(win), XA_CARDINAL,
            "SUBTLE_WINDOW_FLAGS", NULL);

          XFetchName(display, NUM2LONG(win), &title);
          subSharedPropertyClass(NUM2LONG(win), &wmname, &wmclass);

          /* Set properties */
          rb_iv_set(self, "@id",        INT2FIX(id));
          rb_iv_set(self, "@flags",     INT2FIX(*flags));
          rb_iv_set(self, "@title",     rb_str_new2(title));
          rb_iv_set(self, "@name",      rb_str_new2(wmname));
          rb_iv_set(self, "@klass",     rb_str_new2(wmclass));

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

/* SubtlextClientFind {{{ */
/*
 * call-seq: find(hash) -> Subtlext::Client or nil
 *
 * Find a Client
 *
 *  Subtlext::Client.find(:name => "xterm")
 *  => #<Subtlext::Client:xxx>
 *
 *  Subtlext::Client.find(:class => "foobar")
 *  => nil
 */

static VALUE
SubtlextClientFind(VALUE self,
  VALUE value)
{
  int flags = 0;
  VALUE client = Qnil, match = Qnil;

  /* Check object type */
  switch(rb_type(value))
    {
      case T_HASH:
          {
            int i;
            VALUE meth_has_key = rb_intern("has_key?"), meth_fetch = rb_intern("fetch");
            struct properties 
            {
              VALUE sym;
              int   flags;
            } props[] = {
              { CHAR2SYM("title"), SUB_MATCH_TITLE },
              { CHAR2SYM("name"),  SUB_MATCH_NAME  },
              { CHAR2SYM("class"), SUB_MATCH_CLASS },
              { CHAR2SYM("all"),   (SUB_MATCH_TITLE|SUB_MATCH_NAME|SUB_MATCH_CLASS) }
            };

            for(i = 0; LENGTH(props) > i; i++)
              {
                if(Qtrue == rb_funcall(value, meth_has_key, 1, props[i].sym))
                  {
                    match = rb_funcall(value, meth_fetch, 1, props[i].sym);
                    flags = props[i].flags;
                    break;
                  }
              }
          }
        break;
      case T_STRING:
        match = value;
        flags = (SUB_MATCH_NAME|SUB_MATCH_CLASS);
    }

  /* Find client */
  if(T_STRING == rb_type(match) && 0 < flags)
    {
      Window win = None;

      if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

      if(-1 != subSharedClientFind(RSTRING_PTR(match), &win, flags))
        {
          client = SubtlextInstantiateClient(win);
        }
      else rb_raise(rb_eStandardError, "Failed finding client");
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return client;
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

/* SubtlextClientTagList {{{ */
/*
 * call-seq: tags -> Array
 *
 * Get Array of defined Tag
 *
 *  client.tags
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 * 
 *  client.tags
 *  => []
 */

static VALUE
SubtlextClientTagList(VALUE self)
{
  Window win = 0;
  int i, size = 0;
  char **tags = NULL;
  unsigned long *flags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;

  if((win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      method = rb_intern("new");
      klass  = rb_const_get(mod, rb_intern("Tag"));
      flags  = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_WINDOW_TAGS", NULL);
      tags   = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
      array  = rb_ary_new2(size);

      /* Build tag array */
      for(i = 0; i < size; i++)
        {
          if((int)*flags & (1L << (i + 1)))
            {
              VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));
              rb_iv_set(t, "@id", INT2FIX(i));
              rb_ary_push(array, t);
            }
        }

      XFreeStringList(tags);
      free(flags);
    }

  return array;
} /* }}} */

/* SubtlextClientTagHas {{{ */
/*
 * call-seq: has_tag?(name) -> true or false
 * 
 * Check if Client has Tag by given name or Tag object
 *
 *  client.has_tag?("subtle")
 *  => true
 * 
 *  client.has_tag?(subtle.find_tag("subtle"))
 *  => false
 */

static VALUE
SubtlextClientTagHas(VALUE self,
  VALUE value)
{
  return SubtlextTagHas(self, value);
} /* }}} */

/* SubtlextClientTagAdd {{{ */
/*
 * call-seq: tag(name) -> nil
 *
 * Tag Client with given name or Tag object
 *
 *  client.tag("subtle")
 *  => #<Subtlext::Tag:xxx>
 * 
 *  client.tag(Subtlext::Tag.new("subtle"))
 *  => #<Subtlext::Tag:xxx>
 */

static VALUE
SubtlextClientTagAdd(VALUE self,
  VALUE value)
{
  SubtlextTag(self, value, SUB_TYPE_CLIENT, SUB_ACTION_TAG);

  return Qnil;
} /* }}} */

/* SubtlextClientTagDel {{{ */
/*
 * call-seq: untag(name) -> nil
 *
 * Untag Client with given name or Tag object
 *
 *  client.untag("subtle")
 *  => nil
 * 
 *  client.untag(subtle.find_tag("subtle"))
 *  => nil
 */

static VALUE
SubtlextClientTagDel(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_TYPE_CLIENT, SUB_ACTION_UNTAG);
} /* }}} */

/* SubtlextClientStateFull {{{ */
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
SubtlextClientStateFull(VALUE self)
{
  return FIX2INT(rb_iv_get(self, "@flags")) & SUB_EWMH_FULL ? Qtrue : Qfalse;
} /* }}} */

/* SubtlextClientStateFloat {{{ */
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
SubtlextClientStateFloat(VALUE self)
{
  return FIX2INT(rb_iv_get(self, "@flags")) & SUB_EWMH_FLOAT ? Qtrue : Qfalse;
} /* }}} */

/* SubtlextClientStateStick {{{ */
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
SubtlextClientStateStick(VALUE self)
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
  VALUE win = rb_iv_get(self, "@win");
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  if(RTEST(win))
    {
      data.l[0] = NUM2LONG(win);
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed setting focus");

  return Qnil;
} /* }}} */

/* SubtlextClientFocusHas {{{ */
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
SubtlextClientFocusHas(VALUE self)
{
  VALUE ret = Qfalse, win = Qnil;
  unsigned long *focus = NULL;

  /* Fetch data */
  win   = rb_iv_get(self, "@win");
  focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL);

  if(RTEST(win) && focus)
    {
      if(*focus == NUM2LONG(win)) ret = Qtrue;
      free(focus);
    }

  return ret;
} /* }}} */

/* SubtlextClientAlive {{{ */
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
SubtlextClientAlive(VALUE self)
{
  VALUE ret = Qfalse, name = rb_iv_get(self, "@name");

  /* Just find the client */
  if(RTEST(name) && -1 != subSharedClientFind(RSTRING_PTR(name), 
      NULL, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))
    ret = Qtrue;

  return ret;
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

/* SubtlextClientGravity {{{ */
/*
 * call-seq: gravity -> Subtlext::Gravity
 *
 * Get Client Gravity
 *
 *  client.gravity
 *  => #<Subtlext::Gravity:xxx>
 */

static VALUE
SubtlextClientGravity(VALUE self)
{
  Window win = None;
  VALUE gravity = Qnil;
  
  /* Load on demand */
  if(NIL_P((gravity = rb_iv_get(self, "@gravity"))) && 
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int *id = NULL;

      /* Collect data */
      id      = (int *)subSharedPropertyGet(win, XA_CARDINAL,
        "SUBTLE_WINDOW_GRAVITY", NULL);
      gravity = SubtlextInstantiateGravity(*id);

      rb_iv_set(self, "@gravity", gravity);

      free(id);
    }

  return gravity;
} /* }}} */

/* SubtlextClientGravitySet {{{ */
/*
 * call-seq: gravity=(fixnum) -> nil
 *
 * Set client gravity
 *
 *  client.gravity = 0
 *  => nil
 */

static VALUE
SubtlextClientGravitySet(VALUE self,
  VALUE value)
{
  int gravity = -1, mode = -1;

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM: gravity = FIX2INT(value); break;
      case T_HASH:
        {
          VALUE sym_gravity = CHAR2SYM("gravity"), sym_mode = CHAR2SYM("mode"),
            meth_has_key = rb_intern("has_key?"), meth_fetch = rb_intern("fetch");

          if(Qtrue == rb_funcall(value, meth_has_key, 1, sym_gravity)) 
            gravity = FIX2INT(rb_funcall(value, meth_fetch, 1, sym_gravity));

          if(Qtrue == rb_funcall(value, meth_has_key, 1, sym_mode)) 
            mode = FIX2INT(rb_funcall(value, meth_fetch, 1, sym_mode));
        }
        break;
      case T_OBJECT:
        {
          VALUE klass = rb_const_get(mod, rb_intern("Gravity")); 
          
          if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
            gravity = FIX2INT(rb_iv_get(value, "@id"));
        }
    }
  
  /* Set gravity */
  if(-1 != gravity)
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      VALUE id = rb_iv_get(self, "@id");

      data.l[0] = NUM2LONG(id);
      data.l[1] = gravity;
      data.l[2] = -1 != mode ? mode : 0;

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_GRAVITY", data, True);

      rb_iv_set(self, "@gravity", INT2FIX(gravity));
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return Qnil;
} /* }}} */

/* SubtlextClientScreen {{{ */
/*
 * call-seq: screen -> Subtlext::Screen
 *
 * Get screen client is on as Screen object
 *
 *  client.screen
 *  => #<Subtlext::Screen:xxx>
 */

static VALUE
SubtlextClientScreen(VALUE self)
{
  Window win = None;
  VALUE screen = Qnil;
  
  /* Load on demand */
  if(NIL_P((screen = rb_iv_get(self, "@screen"))) &&
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int *id = NULL;
      
      /* Collect data */
      id      = (int *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_WINDOW_SCREEN", NULL);
      screen  = SubtlextInstantiateScreen(*id);

      rb_iv_set(self, "@screen", screen);

      free(id);
    }

  return screen;
} /* }}} */

/* SubtlextClientScreenSet {{{ */
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
SubtlextClientScreenSet(VALUE self,
  VALUE value)
{
  int screen = -1;

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM: screen = FIX2INT(value); break;
      case T_OBJECT:
        {
          VALUE klass = rb_const_get(mod, rb_intern("Screen")); 
          
          if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
            screen = FIX2INT(rb_iv_get(value, "@id"));
        }
    }

  /* Set screen */
  if(-1 != screen)
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      VALUE id = rb_iv_get(self, "@id");

      data.l[0] = NUM2LONG(id);
      data.l[1] = screen;

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_SCREEN", data, True);

      rb_iv_set(self, "@screen", INT2FIX(screen));
    }
  else rb_raise(rb_eArgError, "Failed setting value type `%s'", rb_obj_classname(value));

  return Qnil;
} /* }}} */

/* SubtlextClientGeometry {{{ */
/*
 * call-seq: geometry -> Subtlext::Geometry
 *
 * Get Client Geometry
 *
 *  client.geometry
 *  => #<Subtlext::Geometry:xxx>
 */

static VALUE
SubtlextClientGeometry(VALUE self)
{
  Window win = None;
  VALUE geometry = Qnil;

  /* Load on demand */
  if(NIL_P((geometry = rb_iv_get(self, "@geometry"))) &&
      (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      XWindowAttributes attrs;

      XGetWindowAttributes(display, win, &attrs);

      geometry = SubtlextInstantiateGeometry(attrs.x, attrs.y, attrs.width, attrs.height);
      rb_iv_set(self, "@geometry", geometry);
    }

  return geometry;
} /* }}} */

/* SubtlextClientGeometrySet {{{ */
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
SubtlextClientGeometrySet(int argc,
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

/* SubtlextClientOperatorPlus {{{ */
/*
 * call-seq: +(tag) -> nil
 *
 * Add existing Tag to Client
 *
 *  client + tag
 *  => nil
 *
 *  client + "subtle" 
 *  => nil
 */

static VALUE
SubtlextClientOperatorPlus(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_TAG, SUB_TYPE_CLIENT);
} /* }}} */

/* SubtlextClientOperatorMinus {{{ */
/*
 * call-seq: -(tag) -> nil
 *
 * Add exsiting Tag to client
 *
 *  client - tag
 *  => nil
 *
 *  client - "subtle" 
 *  => nil
 */

static VALUE
SubtlextClientOperatorMinus(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_TYPE_CLIENT, SUB_ACTION_UNTAG);
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
 * call-seq: to_str -> Array
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

/* SubtlextGeometryToHAsh {{{ */
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
 * call-seq: new(id) -> Subtlext::Gravity
 *
 * Create a new Gravity object
 *
 *  gravity = Subtlext::Gravity.new(1)
 *  => #<Subtlext::Gravity:xxx>
 */

static VALUE
SubtlextGravityInit(VALUE self,
  VALUE id)
{
  if(0 >= FIX2INT(id) && 9 < FIX2INT(id))
    rb_raise(rb_eArgError, "Invalid value type");

  rb_iv_set(self, "@id",         id);
  rb_iv_set(self, "@geometries", rb_ary_new());

  if(!display) SubtlextConnect(Qnil); ///< Implicit open connection

  return self;
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
  int size = 0;
  XRectangle *modes = NULL;
  VALUE id = rb_iv_get(self, "@id");

  if(FIXNUM_P(id) && 1 <= FIX2INT(id) && 9 >= FIX2INT(id) && 
      (modes = subSharedGravityList(FIX2INT(id), &size)))
    {
      int i;
      VALUE klass = Qnil, meth = Qnil, ary = Qnil, geom = Qnil;
      
      klass   = rb_const_get(mod, rb_intern("Geometry"));
      meth    = rb_intern("new");
      ary     = rb_iv_get(self, "@geometries");

      /* Add modes to gravity */
      for(i = 0; i < size; i++)
        {
          geom = rb_funcall(klass, meth, 4, INT2FIX(modes[i].x),  INT2FIX(modes[i].y),  
            INT2FIX(modes[i].width),  INT2FIX(modes[i].height));

          rb_ary_push(ary, geom);
        }

      free(modes);
    }
  else rb_raise(rb_eStandardError, "Failed finding gravity");

  return Qnil;
} /* }}} */

/* SubtlextGravityGeometryAdd {{{ */
/*
 * call-seq: add_geometry(x, y, width, height) -> nil
 *           add_geometry(array)               -> nil
 *           add_geometry(hash)                -> nil
 *           add_geometry(geometry)            -> nil
 *
 * Add a new Geometry to Gravity
 *
 *  gravity.add_geometry(0, 0, 50, 50)
 *  => #<Subtlext::Geometry:xxx>
 *
 *  gravity.add_geometry([ 0, 0, 50, 50 ])
 *  => #<Subtlext::Geometry:xxx>
 *
 *  gravity.add_geometry({ :x => 0, :y => 0, :width => 50, :height => 50 })
 *  => #<Subtlext::Geometry:xxx>
 */

static VALUE
SubtlextGravityGeometryAdd(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE x = Qnil, y = Qnil, width = Qnil, height = Qnil, geometry = Qnil;

  /* Load geometry object */
  rb_scan_args(argc, argv, "04", &x, &y, &width, &height);
  geometry = SubtlextInstantiateGeometry(FIX2INT(x), FIX2INT(y), FIX2INT(width), FIX2INT(height));

  /* Add geometry */
  if(RTEST(geometry))
    {
      VALUE ary = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      
      ary       = rb_iv_get(self, "@geometries");
      data.l[0] = FIX2INT(rb_iv_get(self,  "@id"));
      data.l[1] = FIX2INT(rb_iv_get(geometry,  "@x"));
      data.l[2] = FIX2INT(rb_iv_get(geometry,  "@y"));
      data.l[3] = FIX2INT(rb_iv_get(geometry,  "@width"));
      data.l[4] = FIX2INT(rb_iv_get(geometry,  "@height"));

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_GRAVITY_NEW", data, True);

      rb_ary_push(ary, geometry);
    }

  return Qnil;
} /* }}} */

/* SubtlextGravityGeometryDel {{{ */
/*
 * call-seq: del_gravity(id) -> nil
 *
 * Delete a Geometry from Gravity
 *
 *  gravity.del_gepmetry(2)
 *  => nil
 */

static VALUE
SubtlextGravityGeometryDel(VALUE self,
  VALUE id)
{
  if(FIXNUM_P(id))
    {
      int gid = 0, mid = 0;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      VALUE ary = Qnil, size = Qnil;
      
      gid  = FIX2INT(rb_iv_get(self, "@id")) - 1;
      mid  = FIX2INT(id);
      ary  = rb_iv_get(self, "@geometries");
      size = rb_funcall(ary, rb_intern("size"), 0, NULL);

      if(1 < size && 0 <= mid && size > mid)
        {
          data.l[0] = gid;
          data.l[1] = mid;

          subSharedMessage(DefaultRootWindow(display), "SUBTLE_GRAVITY_KILL", data, True);

          rb_funcall(ary, rb_intern("delete_at"), 1, id);
        }
      else rb_raise(rb_eRangeError, "Argument is in invalid range");
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
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
  static const char *gravities[] = { ///< This order is necessary
    "BottomLeft", "Bottom", "BottomRight",
    "Left", "Center", "Right", 
    "TopLeft", "Top", "TopRight"
  };

  return rb_str_new2(gravities[FIX2INT(rb_iv_get(self, "@id")) - 1]);
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

      if(subSharedScreenFind(FIX2INT(id), &geometry))
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

/* SubtlextSubtleRunning {{{ */
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
SubtlextSubtleRunning(VALUE self)
{
  return subSharedSubtleRunning() ? Qtrue : Qfalse;
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

/* SubtlextSubtleTagList {{{ */
/*
 * call-seq: tags -> Array
 *
 * Get Array of defined Tag
 *
 *  subtle.tags
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 *
 *  subtle.tags
 *  => []
 */

static VALUE
SubtlextSubtleTagList(VALUE self)
{
  int i, size = 0;
  char **tags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  method = rb_intern("new");
  klass  = rb_const_get(mod, rb_intern("Tag"));
  tags   = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
  array  = rb_ary_new2(size);

  for(i = 0; i < size; i++)
    {
      VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));

      rb_iv_set(t, "@id",  INT2FIX(i));
      rb_ary_push(array, t);
    }

  XFreeStringList(tags);

  return array;
} /* }}} */

/* SubtlextSubtleTagFind {{{ */
/*
 * call-seq: find_tag(name) -> Subtlext::Tag or nil
 *
 * Find Tag by given name
 *
 *  subtle.find_tag("subtle")
 *  => #<Subtlext::Tag:xxx>
 * 
 *  subtle.find_tag("subtle")
 *  => nil
 */

static VALUE
SubtlextSubtleTagFind(VALUE self,
  VALUE name)
{
  return SubtlextFind(SUB_TYPE_TAG, name, True);
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
  VALUE tag = Qnil;

  if(RTEST((tag = SubtlextFind(SUB_TYPE_TAG, value, True))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2LONG(rb_iv_get(tag, "@id"));

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_KILL", data, True);  
    }

  return Qnil;
} /* }}} */

/* SubtlextSubtleClientList {{{ */
/*
 * call-seq: clients -> Array
 *
 * Get Array of Client
 *
 *  subtle.clients
 *  => [#<Subtlext::Client:xxx>, #<Subtlext::Client:xxx>]
 *
 *  subtle.clients
 *  => []
 */

static VALUE
SubtlextSubtleClientList(VALUE self)
{
  int size = 0;
  Window *clients = NULL;
  VALUE array = Qnil;

  array  = rb_ary_new2(size);    

  if((clients = subSharedClientList(&size)))
    {
      int i;

      for(i = 0; i < size; i++)
        rb_ary_push(array, SubtlextInstantiateClient(clients[i]));

      free(clients);
    }

  return array;
} /* }}} */

/* SubtlextSubtleClientFind {{{ */
/*
 * call-seq: find_client(name) -> Subtlext::Client or nil
 *
 * Find Client by given name
 *
 *  subtle.find_client("subtle")
 *  => #<Subtlext::Client:xxx>
 * 
 *  subtle.find_client("subtle")
 *  => nil
 */

static VALUE
SubtlextSubtleClientFind(VALUE self,
  VALUE name)
{
  return SubtlextFind(SUB_TYPE_CLIENT, name, True);
} /* }}} */

/* SubtlextSubtleClientFocus {{{ */
/*
 * call-seq: focus_client(name) -> Subtlext::Client or nil
 *
 * Find Client by given name and set focus
 *
 *  subtle.focus_client("subtle")
 *  => #<Subtlext::Client:xxx>
 *
 *  subtle.focus_client("subtle")
 *  => nil
 */

static VALUE
SubtlextSubtleClientFocus(VALUE self,
  VALUE name)
{
  int id = -1;
  Window win = 0;
  VALUE client = Qnil;

  if(RTEST(name) && -1 != ((id = subSharedClientFind(RSTRING_PTR(name), 
      &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, True);

      client = SubtlextInstantiateClient(win);
    }
  else rb_raise(rb_eStandardError, "Failed setting focus");

  return client;
} /* }}} */

/* SubtlextSubtleClientFocusLeft {{{ */
/*
 * call-seq: focus_left -> Sublext::Client or nil
 *
 * Focus Client left from current Client
 *
 *  subtle.focus_left
 *  => #<Subtlext::Client:xxx>
 *
 *  subtle.focus_left
 *  => nil
 */

static VALUE
SubtlextSubtleClientFocusLeft(VALUE self)
{
  return SubtlextClientFocus(SubtlextClientMatchLeft(SubtlextSubtleClientCurrent(self)));
} /* }}} */

/* SubtlextSubtleClientFocusDown {{{ */
/*
 * call-seq: focus_down -> Sublext::Client or nil
 *
 * Focus Client below current Client
 *
 *  subtle.focus_down
 *  => #<Subtlext::Client:xxx>
 *
 *  subtle.focus_down
 *  => nil
 */

static VALUE
SubtlextSubtleClientFocusDown(VALUE self)
{
  return SubtlextClientFocus(SubtlextClientMatchDown(SubtlextSubtleClientCurrent(self)));
} /* }}} */

/* SubtlextSubtleClientFocusUp {{{ */
/*
 * call-seq: focus_up -> Sublext::Client or nil
 *
 * Focus Client above current Client
 *
 *  subtle.focus_up
 *  => #<Subtlext::Client:xxx>
 *
 *  subtle.focus_up
 *  => nil
 */

static VALUE
SubtlextSubtleClientFocusUp(VALUE self)
{
  return SubtlextClientFocus(SubtlextClientMatchUp(SubtlextSubtleClientCurrent(self)));
} /* }}} */

/* SubtlextSubtleClientFocusRight {{{ */
/*
 * call-seq: focus_right -> Sublext::Client or nil
 *
 * Focus Client right from current Client
 *
 *  subtle.focus_right
 *  => #<Subtlext::Client:xxx>
 *
 *  subtle.focus_right
 *  => nil
 */

static VALUE
SubtlextSubtleClientFocusRight(VALUE self)
{
  return SubtlextClientFocus(SubtlextClientMatchRight(SubtlextSubtleClientCurrent(self)));
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
      &win, (SUB_MATCH_NAME|SUB_MATCH_CLASS)))))
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

/* SubtlextSubtleClientCurrent {{{ */
/*
 * call-seq: current_client -> Subtlext::Client
 *
 * Get current active Client
 *
 *  subtle.current_client
 *  => #<Subtlext::Client:xxx>
 */


static VALUE
SubtlextSubtleClientCurrent(VALUE self)
{
  VALUE client = Qnil;
  unsigned long *focus = NULL;

  if((focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL)))
    {
      client = SubtlextInstantiateClient(*focus);

      free(focus);
    }
  else rb_raise(rb_eStandardError, "Failed getting current client");

  return client;
} /* }}} */

/* SubtlextSubtleGravityList {{{ */
/*
 * call-seq: gravities -> Array
 *
 * Get Array of Gravity
 *
 *  subtle.gravities
 *  => [#<Subtlext::Gravity:xxx>, #<Subtlext::Gravity:xxx>]
 *
 *  subtle.gravities
 *  => []
 */

static VALUE
SubtlextSubtleGravityList(VALUE self)
{
  int size = 0;
  char **gravities = NULL;

  VALUE array = rb_ary_new();

  if((gravities = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_GRAVITY_LIST", &size)))
    {
      int i, id = -1, gid = -1;
      VALUE klass_grav = Qnil, klass_geom = Qnil, meth = Qnil, gravity = Qnil, geom = Qnil, ary = Qnil;
      XRectangle mode = { 0 };
      
      klass_grav = rb_const_get(mod, rb_intern("Gravity"));
      klass_geom = rb_const_get(mod, rb_intern("Geometry"));
      meth       = rb_intern("new");

      for(i = 0; i < size; i++)
        {
          sscanf(gravities[i], "%d#%hdx%hd+%hd+%hd", &gid, &mode.x, &mode.y, &mode.width, &mode.height);

          if(id != gid)
            {
              id      = gid;
              gravity = rb_funcall(klass_grav, meth, 1, INT2FIX(gid + 1));
              ary     = rb_iv_get(gravity, "@geometries");

              rb_ary_push(array, gravity);
            }

          geom = rb_funcall(klass_geom, meth, 4, INT2FIX(mode.x), INT2FIX(mode.y),  
            INT2FIX(mode.width), INT2FIX(mode.height));

          rb_ary_push(ary, geom);
        }

      XFreeStringList(gravities);
    }
  else rb_raise(rb_eStandardError, "Failed finding gravities");

  return array;
} /* }}} */

/* SubtlextSubtleGravityFind {{{ */
/*
 * call-seq: find_gravity(id) -> Subtlext::Gravity or nil
 *
 * Find Gravity by given id
 *
 *  subtle.find_gravity(1)
 *  => #<Subtlext::Gravity:xxx>
 * 
 *  subtle.find_gravity(1)
 *  => nil
 */

static VALUE
SubtlextSubtleGravityFind(VALUE self,
  VALUE id)
{
  int size = 0;
  XRectangle *modes = NULL;
  VALUE gravity = Qnil;

  if(FIXNUM_P(id) && 1 <= FIX2INT(id) && 9 >= FIX2INT(id) && 
      (modes = subSharedGravityList(FIX2INT(id), &size)))
    {
      int i;
      VALUE klass_grav = Qnil, klass_geom = Qnil, meth = Qnil, ary = Qnil, geom = Qnil;
      
      klass_grav = rb_const_get(mod, rb_intern("Gravity"));
      klass_geom = rb_const_get(mod, rb_intern("Geometry"));
      meth       = rb_intern("new");
      gravity    = rb_funcall(klass_grav, meth, 1, id);
      ary        = rb_iv_get(gravity, "@geometries");

      /* Add modes to gravity */
      for(i = 0; i < size; i++)
        {
          geom = rb_funcall(klass_geom, meth, 4, INT2FIX(modes[i].x),  INT2FIX(modes[i].y),  
            INT2FIX(modes[i].width),  INT2FIX(modes[i].height));

          rb_ary_push(ary, geom);
        }

      free(modes);
    }
  else rb_raise(rb_eStandardError, "Failed finding gravity");

  return gravity;
} /* }}} */

/* SubtlextSubtleSubletDel {{{ */
/*
 * call-seq: del_sublet(name) -> nil
 *
 * Delete Sublet by given name
 *
 *  subtle.del_sublet("subtle")
 *  => nil
 */

static VALUE
SubtlextSubtleSubletDel(VALUE self,
  VALUE name)
{
  int id = -1;

  if(RTEST(name) && -1 != ((id = subSharedSubletFind(RSTRING_PTR(name)))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Send data */
      data.l[0] = id;

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_KILL", data, True);       
    }
  else rb_raise(rb_eStandardError, "Failed finding sublet");

  return Qnil;
} /* }}} */

/* SubtlextSubtleSubletList {{{ */
/*
 * call-seq: sublets -> Array
 *
 * Get Array of Sublet
 *
 *  subtle.sublets
 *  => [#<Subtlext::Sublet:xxx>, #<Subtlext::Sublet:xxx>]
 *
 *  subtle.sublets
 *  => []
 */

static VALUE
SubtlextSubtleSubletList(VALUE self)
{
  int i, size = 0;
  char **sublets = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  method  = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("Sublet"));
  sublets = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_SUBLET_LIST", &size);
  array   = rb_ary_new2(size);

  if(sublets)
    {
      for(i = 0; i < size; i++)
        {
          VALUE s = rb_funcall(klass, method, 1, rb_str_new2(sublets[i]));

          rb_iv_set(s, "@id", INT2FIX(i));
          rb_ary_push(array, s);
        }

      XFreeStringList(sublets);
    }

  return array;
} /* }}} */

/* SubtlextSubtleSubletFind {{{ */
/*
 * call-seq: find_sublet(name) -> Subtlext::Sublet or nil
 *
 * Find Sublet by given name
 *
 *  subtle.find_sublet("subtle")
 *  => #<Subtlext::Sublet:xxx>
 * 
 *  subtle.find_sublet("subtle")
 *  => nil
 */

static VALUE
SubtlextSubtleSubletFind(VALUE self,
  VALUE name)
{
  int id = -1;
  VALUE klass = Qnil, sublet = Qnil;

  if(RTEST(name) && -1 != ((id = subSharedSubletFind(RSTRING_PTR(name)))))
    {
      klass  = rb_const_get(mod, rb_intern("Sublet"));
      sublet = rb_funcall(klass, rb_intern("new"), 1, name);

      rb_iv_set(sublet, "@id", INT2FIX(id));

      return sublet;
    }
  else rb_raise(rb_eStandardError, "Failed finding sublet");

  return sublet;
} /* }}} */

/* SubtlextSubtleViewList {{{ */
/*
 * call-seq: views -> Array
 *
 * Get array of defined View
 *
 *  subtle.views
 *  => [#<Subtlext::View:xxx>, #<Subtlext::View:xxx>]
 */

static VALUE
SubtlextSubtleViewList(VALUE self)
{
  int i, size = 0;
  char **names = NULL;
  Window *views = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  /* Collect data */
  method = rb_intern("new");
  klass  = rb_const_get(mod, rb_intern("View"));
  names  = subSharedPropertyStrings(DefaultRootWindow(display), 
    "_NET_DESKTOP_NAMES", &size);
  views   = (Window *)subSharedPropertyGet(DefaultRootWindow(display), 
    XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);  
  array  = rb_ary_new2(size);

  if(names && views)
    {
      for(i = 0; i < size; i++)
        {
          VALUE v = rb_funcall(klass, method, 1, rb_str_new2(names[i]));

          rb_iv_set(v, "@id",  INT2FIX(i));
          rb_iv_set(v, "@win", LONG2NUM(views[i]));
          rb_ary_push(array, v);
        }

      XFreeStringList(names); 
      free(views);
    }

  return array;
} /* }}} */

/* SubtlextSubtleViewFind {{{ */
/*
 * call-seq: find_view(name) -> Subtlext::View or nil
 *
 * Find View by given name
 *
 *  subtle.find_view("subtle")
 *  => #<Subtlext::View:xxx>
 *
 *  subtle.find_view("subtle")
 *  => nil
 */

static VALUE
SubtlextSubtleViewFind(VALUE self,
  VALUE name)
{
  return SubtlextFind(SUB_TYPE_VIEW, name, True);
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
  VALUE view = Qnil;

  if(RTEST((view = SubtlextFind(SUB_TYPE_VIEW, value, True))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      data.l[0] = FIX2INT(rb_iv_get(view, "@id"));

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_KILL", data, True);  
    }

  return Qnil;
} /* }}} */

/* SubtlextSubtleViewCurrent {{{ */
/*
 * call-seq: current_view -> Subtlext::View
 *
 * Get current active View
 *
 *  subtle.current_view
 *  => #<Subtlext::View:xxx>
 */

static VALUE
SubtlextSubtleViewCurrent(VALUE self)
{
  int size = 0;
  char **names = NULL;
  unsigned long *cv = NULL;
  Window *views = NULL;
  VALUE klass = Qnil, view = Qnil;
  
  /* Create view instance */
  klass = rb_const_get(mod, rb_intern("View"));
  names = subSharedPropertyStrings(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
  cv    = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  views = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW, 
    "_NET_VIRTUAL_ROOTS", NULL);
  view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(names[*cv]));

  rb_iv_set(view, "@id",  INT2FIX(*cv));
  rb_iv_set(view, "@win", LONG2NUM(views[*cv]));

  XFreeStringList(names);
  free(views);
  free(cv);
      
  return view;
} /* }}} */

/* SubtlextSubtleScreenList {{{ */
/*
 * call-seq: screens -> Array
 * 
 * Get Array of Screen
 *
 *  subtle.clients
 *  => [#<Subtlext::Screen:xxx>, #<Subtlext::Screen:xxx>]
 *
 *  subtle.screens
 *  => []
 */

static VALUE
SubtlextSubtleScreenList(VALUE self)
{
  return SubtlextGetScreens();
} /* }}} */

/* SubtlextSubtleScreenCurrent {{{ */
/*
 * call-seq: current_screen -> Subtlext::Screen
 *
 * Get current active Screen
 *
 *  subtle.current_screen
 *  => #<Subtlext::Screen:xxx>
 */

static VALUE
SubtlextSubtleScreenCurrent(VALUE self)
{
  unsigned long *focus = NULL;
  VALUE screen = Qnil;

  /* Get current screen from current client or use the first */
  if((focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL)))
    {
      int *id = NULL;

      id     = (int *)subSharedPropertyGet(*focus, XA_CARDINAL, "SUBTLE_WINDOW_SCREEN", NULL);
      screen = SubtlextInstantiateScreen(INT2FIX(*id));

      free(focus);
      free(id);
    }

  return screen;
} /* }}} */

/* SubtlextSubtleScreenFind {{{ */
/*
 * call-seq: find_screen(name) -> Subtlext::Screen or nil
 *
 * Find Screen by given id
 *
 *  subtle.find_screen(0)
 *  => #<Subtlext::Screen:xxx>
 * 
 *  subtle.find_screen(0)
 *  => nil
 */

static VALUE
SubtlextSubtleScreenFind(VALUE self,
  VALUE id)
{
  return SubtlextFind(SUB_TYPE_SCREEN, id, True);
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

  if(RTEST(name) && -1 != ((id = subSharedSubletFind(RSTRING_PTR(name)))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Send data */
      data.l[0] = id;

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_UPDATE", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed finding sublet");

  return Qnil;
} /* }}} */

/* SubtlextSubletData {{{ */
/*
 * call-seq: data -> String
 *
 * Get data of Sublet
 *
 *  puts sublet.data
 *  => "subtle" 
 */

static VALUE
SubtlextSubletData(VALUE self)
{
  VALUE id = rb_iv_get(self, "@id");
      
  return RTEST(id) ? id : Qnil;
} /* }}} */

/* SubtlextSubletDataSet {{{ */
/*
 * call-seq: data= -> String
 *
 * Set data of sublet
 *
 *  sublet.data = "subtle" 
 *  => "subtle" 
 */

static VALUE
SubtlextSubletDataSet(VALUE self,
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
      if(-1 == (id = subSharedTagFind(RSTRING_PTR(name))))
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };

          snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
          subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, True);    

          id = subSharedTagFind(RSTRING_PTR(name));
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

/* SubtlextTagTagggings {{{ */
/*
 * call-seq: taggings -> Array
 *
 * Get Array of Client and View that are tagged with this
 *
 *  tag.taggings()
 *  => [#<Subtlext::View:xxx>, #<Client:xxx>]
 *
 *  tag.taggings()
 *  => []
 */

static VALUE
SubtlextTagTaggings(VALUE self)
{
  int i, id = 0, size_clients = 0, size_views = 0;
  char **names = NULL;
  Window *clients = NULL, *views = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  /* Collect data */
  id      = FIX2INT(rb_iv_get(self, "@id"));
  method  = rb_intern("new");
  array   = rb_ary_new2(size_clients);
  clients = subSharedClientList(&size_clients);
  names   = subSharedPropertyStrings(DefaultRootWindow(display), 
    "_NET_DESKTOP_NAMES", &size_views);
  views   = (Window *)subSharedPropertyGet(DefaultRootWindow(display), 
    XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);

  /* Clients */
  if(clients)
    {
      klass = rb_const_get(mod, rb_intern("Client"));

      for(i = 0; i < size_clients; i++)
        {
          unsigned long *flags = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
            "SUBTLE_WINDOW_TAGS", NULL);

          if((int)*flags & (1L << (id + 1))) ///< Check if tag id matches
            {
              VALUE c = Qnil;
              char *wmklass = NULL;
              
              /* Create client instance */
              subSharedPropertyClass(clients[i], NULL, &wmklass);
              c = rb_funcall(klass, method, 1, rb_str_new2(wmklass));

              rb_iv_set(c, "@id",  INT2FIX(i));
              rb_iv_set(c, "@win", LONG2NUM(clients[i]));
              rb_ary_push(array, c);

              free(wmklass);
            }

          free(flags);
        }
      free(clients);
    }

  /* Views */
  if(names && views)
    {
      klass = rb_const_get(mod, rb_intern("View"));

      for(i = 0; i < size_views; i++)
        {
          unsigned long *flags = (unsigned long *)subSharedPropertyGet(views[i], XA_CARDINAL, 
            "SUBTLE_WINDOW_TAGS", NULL);

          if((int)*flags & (1L << (id + 1))) ///< Check if tag id matches
            {
              VALUE v = rb_funcall(klass, method, 1, rb_str_new2(names[i]));

              rb_iv_set(v, "@id",  INT2FIX(i));
              rb_iv_set(v, "@win", LONG2NUM(views[i]));
              rb_ary_push(array, v);
            }

          free(flags);
        }

      XFreeStringList(names);
      free(views);
    }

  return array;
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

/* Views */

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
      if(-1 == (id = subSharedViewFind(RSTRING_PTR(name), NULL)))
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };

          snprintf(data.b, sizeof(data.b), "%s", RSTRING_PTR(name));
          subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data, True);    

          id = subSharedViewFind(RSTRING_PTR(name), NULL);
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

/* SubtlextViewClientList {{{ */
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
SubtlextViewClientList(VALUE self)
{
  int i, size = 0;
  Window *clients = NULL;
  VALUE win = Qnil, array = Qnil;
  unsigned long *flags1 = NULL;
  
  win     = rb_iv_get(self, "@win");
  array   = rb_ary_new2(size);
  clients = subSharedClientList(&size);
  flags1  = (unsigned long *)subSharedPropertyGet(NUM2LONG(win), XA_CARDINAL, 
    "SUBTLE_WINDOW_TAGS", NULL);

  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
            "SUBTLE_WINDOW_TAGS", NULL);

          if(*flags1 & *flags2) ///< Check if there are common tags
            rb_ary_push(array, SubtlextInstantiateClient(clients[i]));

          free(flags2);
        }
      free(clients);
    }

  return array;
} /* }}} */

/* SubtlextViewTagList {{{ */
/*
 * call-seq: tags -> Array
 *
 * Get Array of Tags of View
 *
 *  view.tags
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 *
 *  view.tags
 *  => []
 */

static VALUE
SubtlextViewTagList(VALUE self)
{
  Window win = 0;
  int i, size = 0;
  char **tags = NULL;
  unsigned long *flags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil, name = Qnil;

  name = rb_iv_get(self, "@name");
  if(RTEST(name) && -1 != subSharedViewFind(RSTRING_PTR(name), &win))
    {
      method = rb_intern("new");
      klass  = rb_const_get(mod, rb_intern("Tag"));
      flags  = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_WINDOW_TAGS", NULL);
      tags   = subSharedPropertyStrings(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
      array  = rb_ary_new2(size);

      /* Build tag array */
      for(i = 0; i < size; i++)
        {
          if((int)*flags & (1L << (i + 1)))
            {
              VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));

              rb_iv_set(t, "@id", INT2FIX(i));
              rb_ary_push(array, t);
            }
        }

      XFreeStringList(tags);

      return array;
    }
  
  return Qnil;
} /* }}} */

/* SubtlextViewTagHas {{{ */
/*
 * call-seq: has_tag?(name) -> true or false
 *
 * Check if a View has a certain Tag
 *
 *  view.has_tag?("subtle")
 *  => true
 * 
 *  view.has_tag?(subtle.find_tag("subtle"))
 *  => false
 */

static VALUE
SubtlextViewTagHas(VALUE self,
  VALUE value)
{
  return SubtlextTagHas(self, value);
} /* }}} */

/* SubtlextViewTagAdd {{{ */
/*
 * call-seq: has_tag?(name) -> true or false
 *
 * Tag View with with given name or Tag object
 *
 *  view.tag("subtle")
 *  => #<Subtlext::Tag:xxx>
 * 
 *  view.tag(Tag.new("subtle"))
 *  => #<Subtlext::Tag:xxx> * 
 */

static VALUE
SubtlextViewTagAdd(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_TYPE_VIEW, SUB_ACTION_TAG);
} /* }}} */

/* SubtlextViewTagDel {{{ */
/*
 * call-seq: untag(name) -> nil
 *
 * Untag View with with given name or Tag object
 *
 *  view.untag("subtle")
 *  => #<Subtlext::Tag:xxx>
 * 
 *  view.untag(Tag.new("subtle"))
 *  => #<Subtlext::Tag:xxx> * 
 */

static VALUE
SubtlextViewTagDel(VALUE self,
  VALUE value)
{  
  return SubtlextTag(self, value, SUB_TYPE_VIEW, SUB_ACTION_UNTAG);
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

/* SubtlextViewCurrent {{{ */
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
SubtlextViewCurrent(VALUE self)
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

/* SubtlextViewOperatorPlus {{{ */
/*
 * call-seq: +(tag) -> nil
 *
 * Add existing Tag to View
 *
 *  view + tag
 *  => nil
 *
 *  view + "subtle" 
 *  => nil
 */

static VALUE
SubtlextViewOperatorPlus(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_TAG, SUB_TYPE_VIEW);
} /* }}} */

/* SubtlextViewOperatorMinus {{{ */
/*
 * call-seq: -(tag) -> nil
 * 
 * Remove existing Tag from View
 *
 *  view - tag
 *  => nil
 *
 *  view - "subtle" 
 *  => nil
 */

static VALUE
SubtlextViewOperatorMinus(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_UNTAG, SUB_TYPE_VIEW);
} /* }}} */

/* Init_subtlext {{{ */
/*
 * Subtlext is the module of the extension
 */
void 
Init_subtlext(void)
{
  VALUE client = Qnil, geometry = Qnil, gravity = Qnil, screen = Qnil;
  VALUE subtle = Qnil, sublet = Qnil, tag = Qnil, view = Qnil;

  /* Module: sublext */
  mod = rb_define_module("Subtlext");

  /*
   * Document-class: Subtlext::Client
   *
   * Client class for interaction with clients
   */

  client = rb_define_class_under(mod, "Client", rb_cObject);

  /* Client id */
  rb_define_attr(client,   "id",       1, 0); 

  /* Window id */
  rb_define_attr(client,   "win",      1, 0);

  /* WM_CLASS */
  rb_define_attr(client,   "klass",    1, 0);

  /* WM_NAME */
  rb_define_attr(client,   "title",    1, 0);

  /* Geometry */
  rb_define_attr(client,   "geometry", 1, 0);

  /* WM_NAME */
  rb_define_attr(client,   "name",     1, 0);

  /* Bitfield of window states */
  rb_define_attr(client,   "flags",    1, 0);

  rb_define_singleton_method(client, "find", SubtlextClientFind, 1);

  rb_define_method(client, "initialize",   SubtlextClientInit,           1);
  rb_define_method(client, "update",       SubtlextClientUpdate,         0);
  rb_define_method(client, "views",        SubtlextClientViewList,       0);
  rb_define_method(client, "tags",         SubtlextClientTagList,        0);
  rb_define_method(client, "has_tag?",     SubtlextClientTagHas,         1);
  rb_define_method(client, "tag",          SubtlextClientTagAdd,         1);
  rb_define_method(client, "untag",        SubtlextClientTagDel,         1);
  rb_define_method(client, "toggle_full",  SubtlextClientToggleFull,     0);
  rb_define_method(client, "toggle_float", SubtlextClientToggleFloat,    0);
  rb_define_method(client, "toggle_stick", SubtlextClientToggleStick,    0);
  rb_define_method(client, "is_full?",     SubtlextClientStateFull,      0);
  rb_define_method(client, "is_float?",    SubtlextClientStateFloat,     0);
  rb_define_method(client, "is_stick?",    SubtlextClientStateStick,     0);
  rb_define_method(client, "raise",        SubtlextClientRestackRaise,   0);
  rb_define_method(client, "lower",        SubtlextClientRestackLower,   0);
  rb_define_method(client, "up",           SubtlextClientMatchUp,        0);
  rb_define_method(client, "left",         SubtlextClientMatchLeft,      0);
  rb_define_method(client, "right",        SubtlextClientMatchRight,     0);
  rb_define_method(client, "down",         SubtlextClientMatchDown,      0);
  rb_define_method(client, "focus",        SubtlextClientFocus,          0);
  rb_define_method(client, "has_focus?",   SubtlextClientFocusHas,       0);
  rb_define_method(client, "kill",         SubtlextClientKill,           0);
  rb_define_method(client, "to_str",       SubtlextClientToString,       0);
  rb_define_method(client, "gravity",      SubtlextClientGravity,        0);
  rb_define_method(client, "gravity=",     SubtlextClientGravitySet,     1);
  rb_define_method(client, "screen",       SubtlextClientScreen,         0);
  rb_define_method(client, "screen=",      SubtlextClientScreenSet,      1);  
  rb_define_method(client, "geometry",     SubtlextClientGeometry,       0);
  rb_define_method(client, "geometry=",    SubtlextClientGeometrySet,   -1);
  rb_define_method(client, "alive?",       SubtlextClientAlive,          0);
  rb_define_method(client, "+",            SubtlextClientOperatorPlus,   1);
  rb_define_method(client, "-",            SubtlextClientOperatorMinus,  1);
  rb_define_alias(client, "save", "update");
  rb_define_alias(client, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Geometry
   *
   * Geometry class for sizes
   */

  geometry = rb_define_class_under(mod, "Geometry", rb_cObject);

  /* X offset */
  rb_define_attr(geometry,   "x",      1, 1);

  /* Y offset */
  rb_define_attr(geometry,   "y",      1, 1);

  /* Geometry width */
  rb_define_attr(geometry,   "width",  1, 1);

  /* Geometry height */
  rb_define_attr(geometry,   "height", 1, 1);

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
  rb_define_attr(gravity,  "id",         1, 0);

  /* Available geometries */
  rb_define_attr(gravity,  "geometries", 1, 0);

  rb_define_method(gravity, "initialize",   SubtlextGravityInit,         1);
  rb_define_method(gravity, "add_geometry", SubtlextGravityGeometryAdd, -1);
  rb_define_method(gravity, "del_geometry", SubtlextGravityGeometryDel,  1);
  rb_define_method(gravity, "to_str",       SubtlextGravityToString,     0);
  rb_define_alias(gravity, "to_s", "to_str");  

  /*
   * Document-class: Subtlext::Screen
   *
   * Screen class for interaction with screens
   */

  screen = rb_define_class_under(mod, "Screen", rb_cObject);

  /* Screen id */
  rb_define_attr(screen,   "id",       1, 0);

  /* Geometry */
  rb_define_attr(screen,   "geometry", 1, 0);

  /* Screen height */
  rb_define_attr(screen,   "height",   1, 0);

  rb_define_method(screen, "initialize", SubtlextScreenInit,       1);
  rb_define_method(screen, "clients",    SubtlextScreenClientList, 0);
  rb_define_method(screen, "to_str",     SubtlextScreenToString,   0);
  rb_define_alias(screen, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Subtle
   *
   * Subtle class for interaction with the window manager
   */

  subtle = rb_define_class_under(mod, "Subtle", rb_cObject);
  rb_define_singleton_method(subtle, "new",  SubtlextSubtleNew,  -1);
  rb_define_singleton_method(subtle, "new2", SubtlextSubtleNew2, 1);

  rb_define_method(subtle, "version",        SubtlextSubtleVersion,          0);
  rb_define_method(subtle, "display",        SubtlextSubtleDisplay,          0);
  rb_define_method(subtle, "views",          SubtlextSubtleViewList,         0);
  rb_define_method(subtle, "tags",           SubtlextSubtleTagList,          0);
  rb_define_method(subtle, "screens",        SubtlextSubtleScreenList,       0);
  rb_define_method(subtle, "sublets",        SubtlextSubtleSubletList,       0);
  rb_define_method(subtle, "clients",        SubtlextSubtleClientList,       0);
  rb_define_method(subtle, "gravities",      SubtlextSubtleGravityList,      0);
  rb_define_method(subtle, "find_view",      SubtlextSubtleViewFind,         1);
  rb_define_method(subtle, "find_tag",       SubtlextSubtleTagFind,          1);
  rb_define_method(subtle, "find_client",    SubtlextSubtleClientFind,       1);
  rb_define_method(subtle, "find_gravity",   SubtlextSubtleGravityFind,      1);
  rb_define_method(subtle, "find_screen",    SubtlextSubtleScreenFind,       1);
  rb_define_method(subtle, "find_sublet",    SubtlextSubtleSubletFind,       1);
  rb_define_method(subtle, "focus_client",   SubtlextSubtleClientFocus,      1);
  rb_define_method(subtle, "focus_left",     SubtlextSubtleClientFocusLeft,  0);
  rb_define_method(subtle, "focus_down",     SubtlextSubtleClientFocusDown,  0);
  rb_define_method(subtle, "focus_up",       SubtlextSubtleClientFocusUp,    0);
  rb_define_method(subtle, "focus_right",    SubtlextSubtleClientFocusRight, 0);
  rb_define_method(subtle, "add_tag",        SubtlextSubtleTagAdd,           1);
  rb_define_method(subtle, "add_view",       SubtlextSubtleViewAdd,          1);
  rb_define_method(subtle, "del_client",     SubtlextSubtleClientDel,        1);
  rb_define_method(subtle, "del_sublet",     SubtlextSubtleSubletDel,        1);
  rb_define_method(subtle, "del_tag",        SubtlextSubtleTagDel,           1);
  rb_define_method(subtle, "del_view",       SubtlextSubtleViewDel,          1);
  rb_define_method(subtle, "current_view",   SubtlextSubtleViewCurrent,      0);
  rb_define_method(subtle, "current_client", SubtlextSubtleClientCurrent,    0);
  rb_define_method(subtle, "current_screen", SubtlextSubtleScreenCurrent,    0);
  rb_define_method(subtle, "running?",       SubtlextSubtleRunning,          0);
  rb_define_method(subtle, "reload",         SubtlextSubtleReload,           0);
  rb_define_method(subtle, "quit",           SubtlextSubtleQuit,             0);
  rb_define_method(subtle, "spawn",          SubtlextSubtleSpawn,            1);
  rb_define_method(subtle, "to_str",         SubtlextSubtleToString,         0);
  rb_define_alias(subtle, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Sublet
   *
   * Sublet class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);

  /* Name of the sublet */
  rb_define_attr(sublet,   "name", 1, 0);

  rb_define_method(sublet, "initialize", SubtlextSubletInit,     1);
  rb_define_method(sublet, "update",     SubtlextSubletUpdate,   0);
  rb_define_method(sublet, "data",       SubtlextSubletData,     0);
  rb_define_method(sublet, "data=",      SubtlextSubletDataSet,  1);  
  rb_define_method(sublet, "to_str",     SubtlextSubletToString, 0);
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

  rb_define_method(tag, "initialize", SubtlextTagInit,     1);
  rb_define_method(tag, "update",     SubtlextTagUpdate,   0);
  rb_define_method(tag, "taggings",   SubtlextTagTaggings, 0);
  rb_define_method(tag, "to_str",     SubtlextTagToString, 0);
  rb_define_alias(tag, "save", "update");
  rb_define_alias(tag, "to_s", "to_str");

  /*
   * Document-class: Subtlext::View
   *
   * View class for interaction with views
   */

  view = rb_define_class_under(mod, "View", rb_cObject);

  /* View id */
  rb_define_attr(view,   "id",   1, 0);

  /* Window id */
  rb_define_attr(view,   "win",  1, 0);

  /* Name of the view */
  rb_define_attr(view,   "name", 1, 0);

  rb_define_method(view, "initialize", SubtlextViewInit,          1);
  rb_define_method(view, "update",     SubtlextViewUpdate,        0);
  rb_define_method(view, "clients",    SubtlextViewClientList,    0);
  rb_define_method(view, "tags",       SubtlextViewTagList,       0);
  rb_define_method(view, "has_tag?",   SubtlextViewTagHas,        1);
  rb_define_method(view, "tag",        SubtlextViewTagAdd,        1);
  rb_define_method(view, "untag",      SubtlextViewTagDel,        1);
  rb_define_method(view, "jump",       SubtlextViewJump,          0);
  rb_define_method(view, "current?",   SubtlextViewCurrent,       0);
  rb_define_method(view, "to_str",     SubtlextViewToString,      0);
  rb_define_method(view, "+",          SubtlextViewOperatorPlus,  1);
  rb_define_method(view, "-",          SubtlextViewOperatorMinus, 1);
  rb_define_alias(view, "save", "update");
  rb_define_alias(view, "to_s", "to_str");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
