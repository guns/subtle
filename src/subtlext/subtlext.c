
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

Display *display = NULL;
static int refcount = 0;
VALUE mod = Qnil;

#ifdef DEBUG
int debug = 0;
#endif /* DEBUG */

/* Prototypes {{{ */
static VALUE SubtlextFixnumToGrav(VALUE self);
static VALUE SubtlextSubtleClientCurrent(VALUE self);
/* }}} */

/* Flags {{{ */
#define SUB_TYPE_CLIENT  0           ///< Client
#define SUB_TYPE_VIEW    1           ///< View
#define SUB_TYPE_TAG     2           ///< Tag

#define SUB_ACTION_TAG   0           ///< Tag
#define SUB_ACTION_UNTAG 1           ///< Untag
/* }}} */

/* SubtlextFind {{{ */
static VALUE
SubtlextFind(int type,
  VALUE value,
  int create)
{
  int id = -1;
  Window win = -1;
  VALUE klass = Qnil, obj = Qnil, name = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  assert(SUB_TYPE_TAG == type || SUB_TYPE_VIEW == type);

  klass = rb_const_get(mod, rb_intern(SUB_TYPE_TAG == type ? "Tag" : "View")); 

  /* Check object */
  switch(rb_type(value))
    {
      case T_STRING:
        if(SUB_TYPE_TAG == type) id = subSharedTagFind(STR2CSTR(value));
        else id = subSharedViewFind(STR2CSTR(value), &win);

        /* Create and find again */
        if(-1 == id && True == create)
          {
            snprintf(data.b, sizeof(data.b), "%s", STR2CSTR(value));
            subSharedMessage(DefaultRootWindow(display), 
              SUB_TYPE_TAG == type ? "SUBTLE_TAG_NEW" : "SUBTLE_VIEW_NEW", data, True);

            if(SUB_TYPE_TAG == type) id = subSharedTagFind(STR2CSTR(value));
            else id = subSharedViewFind(STR2CSTR(value), &win);
          }

        if(-1 != id)
          {
            /* Create new instance */
            obj = rb_funcall(klass, rb_intern("new"), 1, value);
            rb_iv_set(obj, "@id",   INT2FIX(id));
            rb_iv_set(obj, "@name", value);
            if(SUB_TYPE_VIEW == type) rb_iv_set(obj, "@win", LONG2NUM(win));

            return obj;
          }

        rb_raise(rb_eStandardError, "Failed finding %s", SUB_TYPE_TAG == type ? "tag" : "view");
        return Qnil;
      case T_OBJECT:
      case T_CLASS:
        if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
          {
            if(Qnil == rb_iv_get(value, "@id") && True == create)
              {
                name = rb_iv_get(value, "@name");

                /* Inform subtle */
                snprintf(data.b, sizeof(data.b), "%s", STR2CSTR(name));
                subSharedMessage(DefaultRootWindow(display), 
                  SUB_TYPE_TAG == type ? "SUBTLE_TAG_NEW" : "SUBTLE_VIEW_NEW", data, True);

                /* Check if object exists */
                if(SUB_TYPE_TAG == type) id = subSharedTagFind(STR2CSTR(name));
                else id = subSharedViewFind(STR2CSTR(name), &win);
                if(-1 == id)
                  {
                    rb_raise(rb_eStandardError, "Failed updating %s", 
                      SUB_TYPE_TAG == type ? "tag" : "view");
                    return Qnil;
                  }                

                /* Update object */
                rb_iv_set(value, "@id", INT2FIX(id)); 
                if(SUB_TYPE_VIEW == type) rb_iv_set(value, "@win", LONG2NUM(win));
              }

            return value;
          }
    }

  rb_raise(rb_eArgError, "Unknown value type");
  return Qnil;
} /* }}} */

/* SubtlextToggle {{{ */
static VALUE
SubtlextToggle(VALUE self,
  char *type)
{
  Window win = 0;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  if((win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      data.l[1] = XInternAtom(display, type, False);

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

/* SubtlextMatch {{{ */
static VALUE
SubtlextMatch(VALUE self, 
  int type)
{
  int i, id = 0, size = 0, match = 0, *gravity1 = NULL;
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

              subSharedMatch(type, clients[i], *gravity1, *gravity2, &match, &found);

              if(found == clients[i]) id = i;

              free(gravity2);
            }

          free(flags2);
        }
      
      if(found)
        {
          char *wmname = NULL;
          int *gravity = NULL;
          VALUE klass = Qnil;

          /* Create new instance */
          klass   = rb_const_get(mod, rb_intern("Client"));
          wmname  = subSharedWindowWMName(found);
          client  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(wmname));
          gravity = (int *)subSharedPropertyGet(found, XA_CARDINAL, 
            "SUBTLE_WINDOW_GRAVITY", NULL);

          rb_iv_set(client, "@id",      INT2FIX(id));
          rb_iv_set(client, "@win",     LONG2NUM(found));
          rb_iv_set(client, "@gravity", INT2FIX(*gravity));

          free(wmname);
          free(gravity);
        }

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
  int action,
  int type)
{
  VALUE tag = Qnil;

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
          data.l[2] = type;

          subSharedMessage(DefaultRootWindow(display), 
            SUB_ACTION_TAG == action ? "SUBTLE_WINDOW_TAG" : "SUBTLE_WINDOW_UNTAG", 
            data, True);

          return Qtrue;
        }
    }

  rb_raise(rb_eArgError, "Unknown value type");
  return Qfalse;
} /* }}} */

/* SubtlextClientInit {{{ */
static VALUE
SubtlextClientInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@id",      Qnil);
  rb_iv_set(self, "@win",     Qnil);
  rb_iv_set(self, "@name",    name);
  rb_iv_set(self, "@gravity", Qnil);

  return self;
} /* }}} */

/* SubtlextClientViewList {{{ */
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
  names   = subSharedPropertyList(DefaultRootWindow(display), 
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

      subSharedPropertyListFree(names, size);
      free(views);
    }

  return array;
} /* }}} */

/* SubtlextClientTagList {{{ */
static VALUE
SubtlextClientTagList(VALUE self)
{
  Window win = 0;
  int i, size = 0;
  char **tags = NULL;
  unsigned long *flags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil, name = Qnil;

  name = rb_iv_get(self, "@name");
  if(RTEST(name) && -1 != subSharedClientFind(STR2CSTR(name), &win))
    {
      method = rb_intern("new");
      klass  = rb_const_get(mod, rb_intern("Tag"));
      flags  = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_WINDOW_TAGS", NULL);
      tags   = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
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

      subSharedPropertyListFree(tags, size);
      free(flags);

      return array;
    }
  
  return Qnil;
} /* }}} */

/* SubtlextClientTagAdd {{{ */
static VALUE
SubtlextClientTagAdd(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_TAG, SUB_TYPE_CLIENT);
} /* }}} */

/* SubtlextClientTagDel {{{ */
static VALUE
SubtlextClientTagDel(VALUE self,
  VALUE value)
{  
  return SubtlextTag(self, value, SUB_ACTION_UNTAG, SUB_TYPE_CLIENT);
} /* }}} */

/* SubtlextClientToggleFull {{{ */
static VALUE
SubtlextClientToggleFull(VALUE self)
{
  return SubtlextToggle(self, "_NET_WM_STATE_FULLSCREEN");
} /* }}} */

/* SubtlextClientToggleFloat {{{ */
static VALUE
SubtlextClientToggleFloat(VALUE self)
{
  return SubtlextToggle(self, "_NET_WM_STATE_ABOVE");
} /* }}} */

/* SubtlextClientToggleStick {{{ */
static VALUE
SubtlextClientToggleStick(VALUE self)
{
  return SubtlextToggle(self, "_NET_WM_STATE_STICKY");
} /* }}} */

/* SubtlextClientRestackRaise {{{ */
static VALUE
SubtlextClientRestackRaise(VALUE self)
{
  return SubtlextRestack(self, Above);
} /* }}} */

/* SubtlextClientRestackLower {{{ */
static VALUE
SubtlextClientRestackLower(VALUE self)
{
  return SubtlextRestack(self, Below);
} /* }}} */

/* SubtlextClientMatchUp {{{ */
static VALUE
SubtlextClientMatchUp(VALUE self)
{
  return SubtlextMatch(self, SUB_WINDOW_UP);
} /* }}} */

/* SubtlextClientMatchLeft {{{ */
static VALUE
SubtlextClientMatchLeft(VALUE self)
{
  return SubtlextMatch(self, SUB_WINDOW_LEFT);
} /* }}} */

/* SubtlextClientMatchRight {{{ */
static VALUE
SubtlextClientMatchRight(VALUE self)
{
  return SubtlextMatch(self, SUB_WINDOW_RIGHT);
} /* }}} */

/* SubtlextClientMatchDown {{{ */
static VALUE
SubtlextClientMatchDown(VALUE self)
{
  return SubtlextMatch(self, SUB_WINDOW_DOWN);
} /* }}} */

/* SubtlextClientFocus {{{ */
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

/* SubtlextClientKill {{{ */
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

/* SubtlextClientToString {{{ */
static VALUE
SubtlextClientToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextClientGravity {{{ */
static VALUE
SubtlextClientGravity(VALUE self)
{
  VALUE gravity = rb_iv_get(self, "@gravity");

  return RTEST(gravity) ? gravity : Qnil;
} /* }}} */

/* SubtlextClientGravitySet {{{ */
static VALUE
SubtlextClientGravitySet(VALUE self,
  VALUE value)
{
  if(T_FIXNUM == rb_type(value))
    {
      int gravity = FIX2INT(value);

      if(0 <= gravity && 9 >= gravity)
        {
          SubMessageData data = { { 0, 0, 0, 0, 0 } };
          VALUE id = rb_iv_get(self, "@id");

          data.l[0] = NUM2LONG(id);
          data.l[1] = -1;
          data.l[2] = FIX2LONG(value);

          subSharedMessage(DefaultRootWindow(display), "SUBTLE_WINDOW_GRAVITY", data, True);  

          rb_iv_set(self, "@gravity", value);

          return value;
        }
      else 
        {
          rb_raise(rb_eStandardError, "Failed setting client gravity");
          return Qnil;
        }
    }
  
  rb_raise(rb_eArgError, "Failed setting value type `%d'", rb_type(value));
  return Qnil;
} /* }}} */

/* SubtlextClientOperatorPlus {{{ */
static VALUE
SubtlextClientOperatorPlus(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_TAG, SUB_TYPE_CLIENT);
} /* }}} */

/* SubtlextClientOperatorMinus {{{ */
static VALUE
SubtlextClientOperatorMinus(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_UNTAG, SUB_TYPE_CLIENT);
} /* }}} */

/* SubtlextFixnumToGrav {{{ */
static VALUE
SubtlextFixnumToGrav(VALUE self)
{
  static const char *gravities[] = { 
    "Unknown",
    "BottomLeft",
    "Bottom",
    "Bottom_Right",
    "Left",
    "Center",
    "Right",
    "TopLeft",
    "Top",
    "TopRight"
  };
      
  return rb_str_new2(gravities[FIX2INT(self)]);
} /* }}} */

/* SubtlextGravityToString {{{ */
static VALUE
SubtlextGravityToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

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
static VALUE
SubtlextSubtleNew(int argc,
  VALUE *argv,
  VALUE self)
{
  char *name = NULL;
  VALUE disp = Qnil, data = Qnil;

#ifdef DEBUG
  VALUE dbg = Qfalse;

  rb_scan_args(argc, argv, "02", &disp, &dbg);

  if(Qtrue == dbg) debug++;
#else
  rb_scan_args(argc, argv, "01", &disp);
#endif /* DEBUG */ 

  /* Open display */
  if(!display) ///< Establish new connection
    {
      if(RTEST(disp)) name = STR2CSTR(disp);
      if(!(display = XOpenDisplay(name)))
        {
          subSharedLogError("Failed opening display `%s'\n", (name) ? name : ":0.0");

          return Qnil;
        }
      XSetErrorHandler(subSharedLogXError);

      /* Check if subtle is running */
      if(True != subSharedSubtleRunning())
        {
          XCloseDisplay(display);
          display = NULL;
          
          rb_raise(rb_eStandardError, "Failed finding running %s", PKG_NAME);

          return Qnil;
        }

      subSharedLogDebug("Connection opened (%s)\n", name);
    }
  refcount++;

  /* Create instance */
  data = Data_Wrap_Struct(self, 0, SubtlextSubtleKill, display);
  rb_obj_call_init(data, 0, NULL);

  return data;
} /* }}} */

/* SubtlextSubtleNew2 {{{ */
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
VALUE 
SubtlextSubtleVersion(VALUE self)
{
  return rb_str_new2(PKG_VERSION);
} /* }}} */

/* SubtlextSubtleDisplay {{{ */
VALUE 
SubtlextSubtleDisplay(VALUE self)
{
  return rb_str_new2(DisplayString(display));
} /* }}} */

/* SubtlextSubtleRunning {{{ */
static VALUE
SubtlextSubtleRunning(VALUE self)
{
  return subSharedSubtleRunning() ? Qtrue : Qfalse;
} /* }}} */

/* SubtlextSubtleReload {{{ */
static VALUE
SubtlextSubtleReload(VALUE self)
{
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_RELOAD", data, True);

  return Qnil;
} /* }}} */

/* SubtlextSubtleTagList {{{ */
static VALUE
SubtlextSubtleTagList(VALUE self)
{
  int i, size = 0;
  char **tags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  method = rb_intern("new");
  klass  = rb_const_get(mod, rb_intern("Tag"));
  tags   = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
  array  = rb_ary_new2(size);

  for(i = 0; i < size; i++)
    {
      VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));

      rb_iv_set(t, "@id",  INT2FIX(i));
      rb_ary_push(array, t);
    }

  subSharedPropertyListFree(tags, size);

  return array;
} /* }}} */

/* SubtlextSubtleTagFind {{{ */
static VALUE
SubtlextSubtleTagFind(VALUE self,
  VALUE name)
{
  return SubtlextFind(SUB_TYPE_TAG, name, False);
} /* }}} */

/* SubtlextSubtleTagAdd {{{ */
static VALUE
SubtlextSubtleTagAdd(VALUE self,
  VALUE value)
{
  return SubtlextFind(SUB_TYPE_TAG, value, True); ///< Find or create tag
} /* }}} */

/* SubtlextSubtleTagDel {{{ */
static VALUE
SubtlextSubtleTagDel(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil;

  if(Qnil != (tag = SubtlextFind(SUB_TYPE_TAG, value, False)))
    {
      VALUE id = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      id = rb_iv_get(tag, "@id");

      /* Send data */
      data.l[0] = FIX2LONG(id);

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_KILL", data, True);  
    }
  else rb_raise(rb_eStandardError, "Failed finding tag");

  return Qnil;
} /* }}} */

/* SubtlextSubtleClientList {{{ */
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
      VALUE method = Qnil, klass = Qnil;
      
      method = rb_intern("new");
      klass  = rb_const_get(mod, rb_intern("Client"));

      for(i = 0; i < size; i++)
        {
          int *gravity = NULL;
          char *wmname = NULL;        
          VALUE client = Qnil;

          wmname  = subSharedWindowWMName(clients[i]);
          client  = rb_funcall(klass, method, 1, rb_str_new2(wmname));
          gravity = (int *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
            "SUBTLE_WINDOW_GRAVITY", NULL);

          rb_iv_set(client, "@id",      INT2FIX(i));
          rb_iv_set(client, "@win",     LONG2NUM(clients[i]));
          rb_iv_set(client, "@gravity", INT2FIX(*gravity));
          
          rb_ary_push(array, client);

          free(wmname);
          free(gravity);
        }
      free(clients);
    }

  return array;
} /* }}} */

/* SubtlextSubtleClientFind {{{ */
static VALUE
SubtlextSubtleClientFind(VALUE self,
  VALUE name)
{
  int id = -1;
  Window win = 0;
  VALUE klass = Qnil, client = Qnil;

  if(RTEST(name) && -1 != ((id = subSharedClientFind(STR2CSTR(name), &win))))
    {
      int *gravity = NULL;
      char *wmname = NULL;
      
      wmname  = subSharedWindowWMName(win);
      klass   = rb_const_get(mod, rb_intern("Client"));
      client  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(wmname));
      gravity = (int *)subSharedPropertyGet(win, XA_CARDINAL, "SUBTLE_WINDOW_GRAVITY", NULL);

      rb_iv_set(client, "@id",      INT2FIX(id));
      rb_iv_set(client, "@win",     LONG2NUM(win));
      rb_iv_set(client, "@gravity", INT2FIX(*gravity));

      free(wmname);
      free(gravity);
    }
  else rb_raise(rb_eStandardError, "Failed finding client");

  return client ;
} /* }}} */

/* SubtlextSubtleClientFocus {{{ */
static VALUE
SubtlextSubtleClientFocus(VALUE self,
  VALUE name)
{
  int id = -1;
  Window win = 0;
  VALUE klass = Qnil, client = Qnil;

  if(RTEST(name) && -1 != ((id = subSharedClientFind(STR2CSTR(name), &win))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };
      char *wmname = subSharedWindowWMName(win);

      data.l[0] = win;
      subSharedMessage(DefaultRootWindow(display), "_NET_ACTIVE_WINDOW", data, True);

      /* Create new client instance */
      klass  = rb_const_get(mod, rb_intern("Client"));
      client = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(wmname));

      rb_iv_set(client, "@id",  INT2FIX(id));
      rb_iv_set(client, "@win", LONG2NUM(win));
      free(wmname);
    }
  else rb_raise(rb_eStandardError, "Failed setting focus");

  return client;
} /* }}} */

/* SubtlextSubtleClientFocusLeft {{{ */
static VALUE
SubtlextSubtleClientFocusLeft(VALUE self)
{
  return SubtlextClientFocus(SubtlextClientMatchLeft(SubtlextSubtleClientCurrent(self)));
} /* }}} */

/* SubtlextSubtleClientFocusDown {{{ */
static VALUE
SubtlextSubtleClientFocusDown(VALUE self)
{
  return SubtlextClientFocus(SubtlextClientMatchDown(SubtlextSubtleClientCurrent(self)));
} /* }}} */

/* SubtlextSubtleClientFocusUp {{{ */
static VALUE
SubtlextSubtleClientFocusUp(VALUE self)
{
  return SubtlextClientFocus(SubtlextClientMatchUp(SubtlextSubtleClientCurrent(self)));
} /* }}} */

/* SubtlextSubtleClientFocusRight {{{ */
static VALUE
SubtlextSubtleClientFocusRight(VALUE self)
{
  return SubtlextClientFocus(SubtlextClientMatchRight(SubtlextSubtleClientCurrent(self)));
} /* }}} */

/* SubtlextSubtleClientDel {{{ */
static VALUE
SubtlextSubtleClientDel(VALUE self,
  VALUE name)
{
  int id = -1;
  Window win = 0;

  if(RTEST(name) && -1 != ((id = subSharedClientFind(STR2CSTR(name), &win))))
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
static VALUE
SubtlextSubtleClientCurrent(VALUE self)
{
  VALUE client = Qnil;
  unsigned long *focus = NULL;

  if((focus = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_WINDOW, "_NET_ACTIVE_WINDOW", NULL)))
    {
      int id = -1, *gravity = NULL;
      char *wmname = NULL;
      VALUE klass = Qnil;

      /* Create new client instance */
      wmname  = subSharedWindowWMName(*focus);
      klass   = rb_const_get(mod, rb_intern("Client"));
      client  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(wmname));
      id      = subSharedClientFind(wmname, NULL);
      gravity = (int *)subSharedPropertyGet(*focus, XA_CARDINAL, 
        "SUBTLE_WINDOW_GRAVITY", NULL);

      rb_iv_set(client, "@id",      INT2FIX(id));
      rb_iv_set(client, "@win",     LONG2NUM(*focus));
      rb_iv_set(client, "@gravity", INT2FIX(*gravity));

      free(focus);
      free(wmname);
      free(gravity);
    }
  else rb_raise(rb_eStandardError, "Failed getting current client");

  return client;
} /* }}} */

/* SubtlextSubtleSubletDel {{{ */
static VALUE
SubtlextSubtleSubletDel(VALUE self,
  VALUE name)
{
  int id = -1;

  if(RTEST(name) && -1 != ((id = subSharedSubletFind(STR2CSTR(name)))))
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
static VALUE
SubtlextSubtleSubletList(VALUE self)
{
  int i, size = 0;
  char **sublets = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  method  = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("Sublet"));
  sublets = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_SUBLET_LIST", &size);
  array   = rb_ary_new2(size);

  if(sublets)
    {
      for(i = 0; i < size; i++)
        {
          VALUE s = rb_funcall(klass, method, 1, rb_str_new2(sublets[i]));

          rb_iv_set(s, "@id", INT2FIX(i));
          rb_ary_push(array, s);
        }

      subSharedPropertyListFree(sublets, size);
    }

  return array;
} /* }}} */

/* SubtlextSubtleSubletFind {{{ */
static VALUE
SubtlextSubtleSubletFind(VALUE self,
  VALUE name)
{
  int id = -1;
  VALUE klass = Qnil, sublet = Qnil;

  if(RTEST(name) && -1 != ((id = subSharedSubletFind(STR2CSTR(name)))))
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
  names  = subSharedPropertyList(DefaultRootWindow(display), 
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

      subSharedPropertyListFree(names, size); 
      free(views);
    }

  return array;
} /* }}} */

/* SubtlextSubtleViewFind {{{ */
static VALUE
SubtlextSubtleViewFind(VALUE self,
  VALUE name)
{
  return SubtlextFind(SUB_TYPE_VIEW, name, False);
} /* }}} */

/* SubtlextSubtleViewAdd {{{ */
static VALUE
SubtlextSubtleViewAdd(VALUE self,
  VALUE value)
{
  return SubtlextFind(SUB_TYPE_VIEW, value, True); ///< Find or create view
} /* }}} */

/* SubtlextSubtleViewDel {{{ */
static VALUE
SubtlextSubtleViewDel(VALUE self,
  VALUE value)
{
  VALUE view = Qnil;

  if(Qnil != (view = SubtlextFind(SUB_TYPE_VIEW, value, False)))
    {
      VALUE id = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      id = rb_iv_get(view, "@id");

      /* Send data */
      data.l[0] = FIX2LONG(id);

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_KILL", data, True);  
    }

  return Qnil;
} /* }}} */

/* SubtlextSubtleViewCurrent {{{ */
static VALUE
SubtlextSubtleViewCurrent(VALUE self)
{
  int size = 0;
  char **names = NULL;
  unsigned long *cv = NULL;
  Window *views = NULL;
  VALUE klass = Qnil, view = Qnil;
  
  klass = rb_const_get(mod, rb_intern("View"));
  names = subSharedPropertyList(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
  cv    = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  views = (Window *)subSharedPropertyGet(DefaultRootWindow(display), XA_WINDOW, 
    "_NET_VIRTUAL_ROOTS", NULL);
  view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(names[*cv]));

  rb_iv_set(view, "@id",  INT2FIX(*cv));
  rb_iv_set(view, "@win", LONG2NUM(views[*cv]));

  subSharedPropertyListFree(names, size);
  free(views);
  free(cv);
      
  return view;
} /* }}} */

/* SubtlextSubtleToString {{{ */
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

/* SubtlextSubletInit {{{ */
static VALUE
SubtlextSubletInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);

  return self;
} /* }}} */

/* SubtlextSubletUpdate {{{ */
static VALUE
SubtlextSubletUpdate(VALUE self)
{
  int id = -1;
  VALUE name = rb_iv_get(self, "@name");

  if(RTEST(name) && -1 != ((id = subSharedSubletFind(STR2CSTR(name)))))
    {
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      /* Send data */
      data.l[0] = id;

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_SUBLET_UPDATE", data, True);
    }
  else rb_raise(rb_eStandardError, "Failed finding sublet");

  return Qnil;
} /* }}} */

/* SubtlextSubletToString {{{ */
static VALUE
SubtlextSubletToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextTagInit {{{ */
static VALUE
SubtlextTagInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@name", name);

  return self;
} /* }}} */

/* SubtlextTagSave {{{ */
static VALUE
SubtlextTagSave(VALUE self)
{
  return SubtlextFind(SUB_TYPE_TAG, self, True);
} /* }}} */

/* SubtlextTagTagggings {{{ */
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
  names   = subSharedPropertyList(DefaultRootWindow(display), 
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
              char *wmname = subSharedWindowWMName(clients[i]);
              VALUE c      = rb_funcall(klass, method, 1, rb_str_new2(wmname));

              rb_iv_set(c, "@id",  INT2FIX(i));
              rb_iv_set(c, "@win", LONG2NUM(clients[i]));
              rb_ary_push(array, c);

              free(wmname);
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

      subSharedPropertyListFree(names, size_views);
      free(views);
    }

  return array;
} /* }}} */

/* SubtlextTagToString {{{ */
static VALUE
SubtlextTagToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextViewInit {{{ */
static VALUE
SubtlextViewInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@win",  Qnil);
  rb_iv_set(self, "@name", name);

  return self;
} /* }}} */

/* SubtlextViewClientList {{{ */
static VALUE
SubtlextViewClientList(VALUE self)
{
  int i, size = 0;
  Window *clients = NULL;
  VALUE win = Qnil, array = Qnil, method = Qnil, klass = Qnil;
  unsigned long *flags1 = NULL;
  
  win     = rb_iv_get(self, "@win");
  method  = rb_intern("new");
  klass   = rb_const_get(mod, rb_intern("Client"));
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
            {
              VALUE client = Qnil;
              char *wmname = NULL;
              int *gravity = NULL;

              wmname  = subSharedWindowWMName(clients[i]);
              client  = rb_funcall(klass, method, 1, rb_str_new2(wmname));
              gravity = (int *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
                "SUBTLE_WINDOW_GRAVITY", NULL);

              rb_iv_set(client, "@id",      INT2FIX(i));
              rb_iv_set(client, "@win",     LONG2NUM(clients[i]));
              rb_iv_set(client, "@gravity", INT2FIX(*gravity));

              rb_ary_push(array, client);

              free(wmname);
              free(gravity);
            }

          free(flags2);
        }
      free(clients);
    }

  return array;
} /* }}} */

/* SubtlextViewTagList {{{ */
static VALUE
SubtlextViewTagList(VALUE self)
{
  Window win = 0;
  int i, size = 0;
  char **tags = NULL;
  unsigned long *flags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil, name = Qnil;

  name = rb_iv_get(self, "@name");
  if(RTEST(name) && -1 != subSharedViewFind(STR2CSTR(name), &win))
    {
      method = rb_intern("new");
      klass  = rb_const_get(mod, rb_intern("Tag"));
      flags  = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_WINDOW_TAGS", NULL);
      tags   = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
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

      subSharedPropertyListFree(tags, size);

      return array;
    }
  
  return Qnil;
} /* }}} */

/* SubtlextViewTagAdd {{{ */
static VALUE
SubtlextViewTagAdd(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_TAG, SUB_TYPE_VIEW);
} /* }}} */

/* SubtlextViewTagDel {{{ */
static VALUE
SubtlextViewTagDel(VALUE self,
  VALUE value)
{  
  return SubtlextTag(self, value, SUB_ACTION_UNTAG, SUB_TYPE_VIEW);
} /* }}} */

/* SubtlextViewJump {{{ */
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

/* SubtlextViewSave {{{ */
static VALUE
SubtlextViewSave(VALUE self)
{
  return SubtlextFind(SUB_TYPE_VIEW, self, True);
} /* }}} */

/* SubtlextViewToString {{{ */
static VALUE
SubtlextViewToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* SubtlextViewOperatorPlus {{{ */
static VALUE
SubtlextViewOperatorPlus(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_TAG, SUB_TYPE_VIEW);
} /* }}} */

/* SubtlextViewOperatorMinus {{{ */
static VALUE
SubtlextViewOperatorMinus(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, SUB_ACTION_UNTAG, SUB_TYPE_VIEW);
} /* }}} */

 /** Init_subtlext {{{
  * @brief Extension entry point
  **/

void 
Init_subtlext(void)
{
  VALUE klass = Qnil;

  /* Module: sublext */
  mod = rb_define_module("Subtlext");

  /* Class: client */
  klass = rb_define_class_under(mod, "Client", rb_cObject);
  rb_define_attr(klass,   "id",      1, 0);
  rb_define_attr(klass,   "win",     1, 0);
  rb_define_attr(klass,   "name",    1, 0);

  rb_define_method(klass, "initialize",   SubtlextClientInit,          1);
  rb_define_method(klass, "views",        SubtlextClientViewList,      0);
  rb_define_method(klass, "tags",         SubtlextClientTagList,       0);
  rb_define_method(klass, "tag",          SubtlextClientTagAdd,        1);
  rb_define_method(klass, "untag",        SubtlextClientTagDel,        1);
  rb_define_method(klass, "toggle_full",  SubtlextClientToggleFull,    0);
  rb_define_method(klass, "toggle_float", SubtlextClientToggleFloat,   0);
  rb_define_method(klass, "toggle_stick", SubtlextClientToggleStick,   0);
  rb_define_method(klass, "raise",        SubtlextClientRestackRaise,  0);
  rb_define_method(klass, "lower",        SubtlextClientRestackLower,  0);
  rb_define_method(klass, "up",           SubtlextClientMatchUp,       0);
  rb_define_method(klass, "left",         SubtlextClientMatchLeft,     0);
  rb_define_method(klass, "right",        SubtlextClientMatchRight,    0);
  rb_define_method(klass, "down",         SubtlextClientMatchDown,     0);
  rb_define_method(klass, "focus",        SubtlextClientFocus,         0);
  rb_define_method(klass, "focus?",       SubtlextClientFocusHas,      0);
  rb_define_method(klass, "kill",         SubtlextClientKill,          0);
  rb_define_method(klass, "to_s",         SubtlextClientToString,      0);
  rb_define_method(klass, "gravity",      SubtlextClientGravity,       0);
  rb_define_method(klass, "gravity=",     SubtlextClientGravitySet,    1);
  rb_define_method(klass, "+",            SubtlextClientOperatorPlus,  1);
  rb_define_method(klass, "-",            SubtlextClientOperatorMinus, 1);

  /* Class: gravity */
  klass = rb_define_class_under(mod, "Gravity", rb_cObject);
  rb_define_const(klass, "TopLeft",     INT2FIX(SUB_GRAVITY_TOP_LEFT));
  rb_define_const(klass, "Top",         INT2FIX(SUB_GRAVITY_TOP));
  rb_define_const(klass, "TopRight",    INT2FIX(SUB_GRAVITY_TOP_RIGHT));
  rb_define_const(klass, "Left",        INT2FIX(SUB_GRAVITY_LEFT));
  rb_define_const(klass, "Center",      INT2FIX(SUB_GRAVITY_CENTER));
  rb_define_const(klass, "Right",       INT2FIX(SUB_GRAVITY_RIGHT));
  rb_define_const(klass, "BottomLeft",  INT2FIX(SUB_GRAVITY_BOTTOM_LEFT));
  rb_define_const(klass, "Bottom",      INT2FIX(SUB_GRAVITY_BOTTOM));
  rb_define_const(klass, "BottomRight", INT2FIX(SUB_GRAVITY_BOTTOM_RIGHT));  

  rb_define_method(klass, "to_s", SubtlextGravityToString, 1);

  /* Class: fixnum */
  klass = rb_const_get(rb_mKernel, rb_intern("Fixnum"));
  rb_define_method(klass, "to_grav", SubtlextFixnumToGrav, 0);

  /* Class: subtle */
  klass = rb_define_class_under(mod, "Subtle", rb_cObject);
  rb_define_singleton_method(klass, "new", SubtlextSubtleNew,  -1);
  rb_define_singleton_method(klass, "new2", SubtlextSubtleNew2, 1);

  rb_define_method(klass, "version",        SubtlextSubtleVersion,          0);
  rb_define_method(klass, "display",        SubtlextSubtleDisplay,          0);
  rb_define_method(klass, "views",          SubtlextSubtleViewList,         0);
  rb_define_method(klass, "tags",           SubtlextSubtleTagList,          0);
  rb_define_method(klass, "sublets",        SubtlextSubtleSubletList,       0);
  rb_define_method(klass, "clients",        SubtlextSubtleClientList,       0);
  rb_define_method(klass, "find_view",      SubtlextSubtleViewFind,         1);
  rb_define_method(klass, "find_tag",       SubtlextSubtleTagFind,          1);
  rb_define_method(klass, "find_client",    SubtlextSubtleClientFind,       1);
  rb_define_method(klass, "find_sublet",    SubtlextSubtleSubletFind,       1);
  rb_define_method(klass, "focus_client",   SubtlextSubtleClientFocus,      1);
  rb_define_method(klass, "focus_left",     SubtlextSubtleClientFocusLeft,  0);
  rb_define_method(klass, "focus_down",     SubtlextSubtleClientFocusDown,  0);
  rb_define_method(klass, "focus_up",       SubtlextSubtleClientFocusUp,    0);
  rb_define_method(klass, "focus_right",    SubtlextSubtleClientFocusRight, 0);
  rb_define_method(klass, "add_tag",        SubtlextSubtleTagAdd,           1);
  rb_define_method(klass, "add_view",       SubtlextSubtleViewAdd,          1);
  rb_define_method(klass, "del_client",     SubtlextSubtleClientDel,        1);
  rb_define_method(klass, "del_sublet",     SubtlextSubtleSubletDel,        1);
  rb_define_method(klass, "del_tag",        SubtlextSubtleTagDel,           1);
  rb_define_method(klass, "del_view",       SubtlextSubtleViewDel,          1);
  rb_define_method(klass, "current_view",   SubtlextSubtleViewCurrent,      0);
  rb_define_method(klass, "current_client", SubtlextSubtleClientCurrent,    0);
  rb_define_method(klass, "running?",       SubtlextSubtleRunning,          0);
  rb_define_method(klass, "reload",         SubtlextSubtleReload,           0);
  rb_define_method(klass, "to_s",           SubtlextSubtleToString,         0);

  /* Class: sublet */
  klass = rb_define_class_under(mod, "Sublet", rb_cObject);
  rb_define_attr(klass,   "name", 1, 0);

  rb_define_method(klass, "initialize", SubtlextSubletInit,     1);
  rb_define_method(klass, "update",     SubtlextSubletUpdate,   0);
  rb_define_method(klass, "to_s",       SubtlextSubletToString, 0);

  /* Class: tag */
  klass = rb_define_class_under(mod, "Tag", rb_cObject);
  rb_define_attr(klass,   "id",   1, 0);
  rb_define_attr(klass,   "name", 1, 0);

  rb_define_method(klass, "initialize", SubtlextTagInit,     1);
  rb_define_method(klass, "taggings",   SubtlextTagTaggings, 0);
  rb_define_method(klass, "save",       SubtlextTagSave,     0);
  rb_define_method(klass, "to_s",       SubtlextTagToString, 0);

  /* Class: view */
  klass = rb_define_class_under(mod, "View", rb_cObject);
  rb_define_attr(klass,   "id",   1, 0);
  rb_define_attr(klass,   "win",  1, 0);
  rb_define_attr(klass,   "name", 1, 0);

  rb_define_method(klass, "initialize", SubtlextViewInit,          1);
  rb_define_method(klass, "clients",    SubtlextViewClientList,    0);
  rb_define_method(klass, "tags",       SubtlextViewTagList,       0);
  rb_define_method(klass, "tag",        SubtlextViewTagAdd,        1);
  rb_define_method(klass, "untag",      SubtlextViewTagDel,        1);
  rb_define_method(klass, "jump",       SubtlextViewJump,          0);
  rb_define_method(klass, "current?",   SubtlextViewCurrent,       0);
  rb_define_method(klass, "save",       SubtlextViewSave,          0);
  rb_define_method(klass, "to_s",       SubtlextViewToString,      0);
  rb_define_method(klass, "+",          SubtlextViewOperatorPlus,  1);
  rb_define_method(klass, "-",          SubtlextViewOperatorMinus, 1);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
