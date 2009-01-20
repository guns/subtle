
 /**
  * @package subtle
  *
  * @file subtle ruby extension
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  *
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <ruby.h>
#include "shared.h"

Display *display = NULL;
static int refcount = 0;

#ifdef DEBUG
int debug = 0;
#endif /* DEBUG */

/* Flags {{{ */
#define SUB_TYPE_TAG  0   ///< Tag
#define SUB_TYPE_VIEW 1   ///< View
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

  klass = rb_const_get(rb_mKernel, rb_intern(SUB_TYPE_TAG == type ? "Tag" : "View")); 

  /* Check object */
  switch(rb_type(value))
    {
      case T_STRING:
        if(SUB_TYPE_TAG == type) id = subSharedTagFind(STR2CSTR(value));
        else id = subSharedViewFind(STR2CSTR(value), &win);
        if(-1 == id && True == create)
          {
            /* Create and find again */
            snprintf(data.b, sizeof(data.b), "%s", STR2CSTR(value));
            subSharedMessage(DefaultRootWindow(display), 
              SUB_TYPE_TAG == type ? "SUBTLE_TAG_NEW" : "SUBTLE_VIEW_NEW", data, True);

            if(SUB_TYPE_TAG == type) id = subSharedTagFind(STR2CSTR(value));
            else id = subSharedViewFind(STR2CSTR(value), &win);

            return id;
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

        rb_raise(rb_eStandardError, "Failed to find object");
        return Qnil;
      case T_OBJECT:
      case T_CLASS:
        if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
          {
            if(-1 == (id = rb_iv_get(value, "@id")) && True == create)
              {
                name = rb_iv_get(value, "@name");

                /* Inform subtle */
                snprintf(data.b, sizeof(data.b), "%s", STR2CSTR(name));
                subSharedMessage(DefaultRootWindow(display), 
                  SUB_TYPE_TAG == type ? "SUBTLE_TAG_NEW" : "SUBTLE_VIEW_NEW", data, True);

                /* Check if object exists */
                if(SUB_TYPE_TAG == type) id = subSharedTagFind(STR2CSTR(value));
                else id = subSharedViewFind(STR2CSTR(value), &win);
                if(-1 == id)
                  {
                    rb_raise(rb_eStandardError, "Failed to update object");
                    return Qnil;
                  }                

                /* Update object */
                rb_iv_set(value, "@id", INT2FIX(id)); 
                if(SUB_TYPE_VIEW == type) rb_iv_set(obj, "@win", LONG2NUM(win));
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
  else rb_raise(rb_eStandardError, "Failed to toggle client");

  return Qnil;
} /* }}} */

/* SubtlextClientInit {{{ */
static VALUE
SubtlextClientInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@id",   Qnil);
  rb_iv_set(self, "@win",  Qnil);
  rb_iv_set(self, "@name", name);

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
  klass   = rb_const_get(rb_mKernel, rb_intern("View"));
  array   = rb_ary_new2(size);
  names   = subSharedPropertyList(DefaultRootWindow(display), 
    "_NET_DESKTOP_NAMES", &size);
  views   = (Window *)subSharedPropertyGet(DefaultRootWindow(display), 
    XA_WINDOW, "_NET_VIRTUAL_ROOTS", NULL);
  flags1  = (unsigned long *)subSharedPropertyGet(NUM2LONG(win), XA_CARDINAL, 
    "SUBTLE_CLIENT_TAGS", NULL);

  if(names && views)
    {
      for(i = 0; i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(views[i], 
            XA_CARDINAL, "SUBTLE_VIEW_TAGS", NULL);

          if(*flags1 & *flags2) ///< Check if there are common tags
            {
              VALUE v = rb_funcall(klass, method, 1, rb_str_new2(names[i]));

              rb_iv_set(v, "@id",  INT2FIX(i));
              rb_iv_set(v, "@win", LONG2NUM(views[i]));
              rb_ary_push(array, v);
            }

          free(flags2);
        }
      free(names);
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
      klass  = rb_const_get(rb_mKernel, rb_intern("Tag"));
      flags  = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_CLIENT_TAGS", NULL);
      tags   = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
      array  = rb_ary_new2(size);

      for(i = 0; i < size; i++)
        {
          if((int)*flags & (1L << (i + 1)))
            {
              VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));
              rb_iv_set(t, "@id", INT2FIX(i));
              rb_ary_push(array, t);
            }
        }

      free(flags);
      free(tags);

      return array;
    }
  
  return Qnil;
} /* }}} */

/* SubtlextClientTagAdd {{{ */
static VALUE
SubtlextClientTagAdd(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil;

  if(Qnil != (tag = SubtlextFind(SUB_TYPE_TAG, value, True)))
    {
      VALUE cid = Qnil, tid = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      cid = rb_iv_get(self, "@id");
      tid = rb_iv_get(tag,  "@id");

      /* Send data */
      data.l[0] = FIX2LONG(cid);
      data.l[1] = FIX2LONG(tid);

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_CLIENT_TAG", data, True);
    }

  return tag;
} /* }}} */

/* SubtlextClientTagDel {{{ */
static VALUE
SubtlextClientTagDel(VALUE self,
  VALUE value)
{  
  VALUE tag = Qnil;

  if(Qnil != (tag = SubtlextFind(SUB_TYPE_TAG, value, False)))
    {
      VALUE cid = Qnil, tid = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      cid = rb_iv_get(self, "@id");
      tid = rb_iv_get(tag,  "@id");

      /* Send data */
      data.l[0] = FIX2LONG(cid);
      data.l[1] = FIX2LONG(tid);

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_CLIENT_UNTAG", data, True);      
    }

  return Qnil;
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
  else rb_raise(rb_eStandardError, "Failed to set focus");

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

/* SubtlextClientToString {{{ */
static VALUE
SubtlextClientToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

static VALUE
SubtlextClientOperatorPlus(VALUE self,
  VALUE value)
{
  VALUE klass = Qnil;

  klass = rb_const_get(rb_mKernel, rb_intern("Tag"));

  /* Check object */
  switch(rb_type(value))
    {
      case T_OBJECT:
      case T_CLASS:
        if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
          {
            VALUE tid = Qnil, cid = Qnil;
            
            tid = rb_iv_get(value, "@id");
            cid = rb_iv_get(self, "@id");

            if(Qnil != tid && Qnil != cid)
              {
                SubMessageData data = { { 0, 0, 0, 0, 0 } };

                data.l[0] = FIX2LONG(cid);
                data.l[1] = FIX2LONG(tid);

                subSharedMessage(DefaultRootWindow(display), "SUBTLE_CLIENT_TAG", data, True);

                return Qtrue;
              }
          }
    }

  rb_raise(rb_eArgError, "Unknown value type");
  return Qfalse;
}

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
  char *dispname = NULL;
  VALUE disp = Qnil, data = Qnil;

#ifdef DEBUG
  VALUE dbg = Qfalse;

  rb_scan_args(argc, argv, "02", &disp, &dbg);

  if(Qtrue == dbg) debug++;
#else
  rb_scan_args(argc, argv, "01", &disp);
#endif /* DEBUG */ 

  /* Open display */
  if(!display)
    {
      if(RTEST(disp)) dispname = STR2CSTR(disp);
      if(!(display = XOpenDisplay(dispname)))
        {
          subSharedLogError("Failed to open display `%s'\n", (dispname) ? dispname : ":0.0");

          return Qnil;
        }
      XSetErrorHandler(subSharedLogXError);

      /* Check if subtle is running */
      if(True != subSharedSubtleRunning())
        {
          XCloseDisplay(display);
          display = NULL;
          
          rb_raise(rb_eStandardError, "Failed to find running %s", PKG_NAME);

          return Qnil;
        }

      subSharedLogDebug("Connection opened (%s)\n", dispname);
    }
  refcount++;

  /* Create instance */
  data = Data_Wrap_Struct(self, 0, SubtlextSubtleKill, display);
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

/* SubtlextSubtleTagList {{{ */
static VALUE
SubtlextSubtleTagList(VALUE self)
{
  int i, size = 0;
  char **tags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  method = rb_intern("new");
  klass  = rb_const_get(rb_mKernel, rb_intern("Tag"));
  tags   = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
  array  = rb_ary_new2(size);

  for(i = 0; i < size; i++)
    {
      VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));

      rb_iv_set(t, "@id",  INT2FIX(i));
      rb_ary_push(array, t);
    }

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

  return Qnil;
} /* }}} */

/* SubtlextSubtleClientList {{{ */
static VALUE
SubtlextSubtleClientList(VALUE self)
{
  int i, size = 0;
  Window *clients = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  method  = rb_intern("new");
  klass   = rb_const_get(rb_mKernel, rb_intern("Client"));
  clients = subSharedClientList(&size);
  array   = rb_ary_new2(size);

  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          char *wmname = subSharedWindowWMName(clients[i]);
          VALUE c = rb_funcall(klass, method, 1, rb_str_new2(wmname));

          rb_iv_set(c, "@id",  INT2FIX(i));
          rb_iv_set(c, "@win", LONG2NUM(clients[i]));
          rb_ary_push(array, c);

          free(wmname);
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
      char *wmname = subSharedWindowWMName(win);

      klass  = rb_const_get(rb_mKernel, rb_intern("Client"));
      client = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(wmname));

      rb_iv_set(client, "@id",  INT2FIX(id));
      rb_iv_set(client, "@win", LONG2NUM(win));
      free(wmname);

      return client;
    }

  rb_raise(rb_eStandardError, "Failed to find client");
  return Qnil;
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
      klass  = rb_const_get(rb_mKernel, rb_intern("Client"));
      client = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(wmname));

      rb_iv_set(client, "@id",  INT2FIX(id));
      rb_iv_set(client, "@win", LONG2NUM(win));
      free(wmname);

      return client;
    }

  rb_raise(rb_eStandardError, "Failed to set focus");
  return Qnil;
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

      subSharedMessage(win, "_NET_CLOSE_WINDOW", data, True);       
    }

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
  klass   = rb_const_get(rb_mKernel, rb_intern("Sublet"));
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
      free(sublets);
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
      klass  = rb_const_get(rb_mKernel, rb_intern("Sublet"));
      sublet = rb_funcall(klass, rb_intern("new"), 1, name);

      return sublet;
    }

  rb_raise(rb_eStandardError, "Failed to find sublet");
  return Qnil;
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
  klass  = rb_const_get(rb_mKernel, rb_intern("View"));
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

      free(names);
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
  VALUE klass = Qnil, view = Qnil;
  
  names = subSharedPropertyList(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
  klass = rb_const_get(rb_mKernel, rb_intern("View"));
  cv    = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display),
    XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(names[*cv]));

  rb_iv_set(view, "@id", INT2FIX(*cv));
  free(names);
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
      klass = rb_const_get(rb_mKernel, rb_intern("Client"));

      for(i = 0; i < size_clients; i++)
        {
          unsigned long *flags = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
            "SUBTLE_CLIENT_TAGS", NULL);

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
      klass = rb_const_get(rb_mKernel, rb_intern("View"));

      for(i = 0; i < size_views; i++)
        {
          unsigned long *flags = (unsigned long *)subSharedPropertyGet(views[i], XA_CARDINAL, 
            "SUBTLE_VIEW_TAGS", NULL);

          if((int)*flags & (1L << (id + 1))) ///< Check if tag id matches
            {
              VALUE v = rb_funcall(klass, method, 1, rb_str_new2(names[i]));

              rb_iv_set(v, "@id",  INT2FIX(i));
              rb_iv_set(v, "@win", LONG2NUM(views[i]));
              rb_ary_push(array, v);
            }

          free(flags);
        }
      free(names);
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
  klass   = rb_const_get(rb_mKernel, rb_intern("Client"));
  array   = rb_ary_new2(size);
  clients = subSharedClientList(&size);
  flags1  = (unsigned long *)subSharedPropertyGet(NUM2LONG(win), XA_CARDINAL, 
    "SUBTLE_VIEW_TAGS", NULL);

  if(clients)
    {
      for(i = 0; i < size; i++)
        {
          unsigned long *flags2 = (unsigned long *)subSharedPropertyGet(clients[i], XA_CARDINAL, 
            "SUBTLE_CLIENT_TAGS", NULL);

          if(*flags1 & *flags2) ///< Check if there are common tags
            {
              char *wmname = subSharedWindowWMName(clients[i]);
              VALUE c      = rb_funcall(klass, method, 1, rb_str_new2(wmname));

              rb_iv_set(c, "@id",  INT2FIX(i));
              rb_iv_set(c, "@win", LONG2NUM(clients[i]));
              rb_ary_push(array, c);

              free(wmname);
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
      klass  = rb_const_get(rb_mKernel, rb_intern("Tag"));
      flags  = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, 
        "SUBTLE_VIEW_TAGS", NULL);
      tags   = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
      array  = rb_ary_new2(size);

      for(i = 0; i < size; i++)
        {
          if((int)*flags & (1L << (i + 1)))
            {
              VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));
              rb_iv_set(t, "@id", INT2FIX(i));
              rb_ary_push(array, t);
            }
        }

      free(flags);
      free(tags);

      return array;
    }
  
  return Qnil;
} /* }}} */

/* SubtlextViewTagAdd {{{ */
static VALUE
SubtlextViewTagAdd(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil;

  if(Qnil != (tag = SubtlextFind(SUB_TYPE_TAG, value, True)))
    {
      VALUE vid = Qnil, tid = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      vid = rb_iv_get(self, "@id");
      tid = rb_iv_get(tag,  "@id");

      /* Send data */
      data.l[0] = FIX2LONG(vid);
      data.l[1] = FIX2LONG(tid);

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_TAG", data, True);
    }

  return tag;
} /* }}} */

/* SubtlextViewTagDel {{{ */
static VALUE
SubtlextViewTagDel(VALUE self,
  VALUE value)
{  
  VALUE tag = Qnil;

  if(Qnil != (tag = SubtlextFind(SUB_TYPE_TAG, value, False)))
    {
      VALUE vid = Qnil, tid = Qnil;
      SubMessageData data = { { 0, 0, 0, 0, 0 } };

      vid = rb_iv_get(self, "@id");
      tid = rb_iv_get(tag,  "@id");

      /* Send data */
      data.l[0] = FIX2LONG(vid);
      data.l[1] = FIX2LONG(tid);

      subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_UNTAG", data, True);      
    }

  return Qnil;
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

/* SubtlextViewToString {{{ */
static VALUE
SubtlextViewToString(VALUE self)
{
  VALUE name = rb_iv_get(self, "@name");

  return RTEST(name) ? name : Qnil;
} /* }}} */

 /** Init_subtlext {{{
  * @brief Extension entry point
  **/

void 
Init_subtlext(void)
{
  VALUE klass = Qnil;

  /* Class: client */
  klass = rb_define_class("Client", rb_cObject);
  rb_define_attr(klass,   "id", 1, 0);
  rb_define_attr(klass,   "win", 1, 0);
  rb_define_attr(klass,   "name", 1, 0);
  rb_define_method(klass, "initialize",    SubtlextClientInit, 1);
  rb_define_method(klass, "views",         SubtlextClientViewList, 0);
  rb_define_method(klass, "tags",          SubtlextClientTagList, 0);
  rb_define_method(klass, "tag",           SubtlextClientTagAdd, 1);
  rb_define_method(klass, "untag",         SubtlextClientTagDel, 1);
  rb_define_method(klass, "toggle_full",   SubtlextClientToggleFull, 0);
  rb_define_method(klass, "toggle_float",  SubtlextClientToggleFloat, 0);
  rb_define_method(klass, "toggle_stick",  SubtlextClientToggleStick, 0);
  rb_define_method(klass, "focus",         SubtlextClientFocus, 0);
  rb_define_method(klass, "focus?",        SubtlextClientFocusHas, 0);
  rb_define_method(klass, "to_s",          SubtlextClientToString, 0);
  rb_define_method(klass, "+",             SubtlextClientOperatorPlus, 1);

  /* Class: subtle */
  klass = rb_define_class("Subtle", rb_cObject);
  rb_define_singleton_method(klass, "new", SubtlextSubtleNew, -1);
  rb_define_method(klass, "version",       SubtlextSubtleVersion, 0);
  rb_define_method(klass, "display",       SubtlextSubtleDisplay, 0);
  rb_define_method(klass, "views",         SubtlextSubtleViewList, 0);
  rb_define_method(klass, "tags",          SubtlextSubtleTagList, 0);
  rb_define_method(klass, "sublets",       SubtlextSubtleSubletList, 0);
  rb_define_method(klass, "clients",       SubtlextSubtleClientList, 0);
  rb_define_method(klass, "find_view",     SubtlextSubtleViewFind, 1);
  rb_define_method(klass, "find_tag",      SubtlextSubtleTagFind, 1);
  rb_define_method(klass, "find_client",   SubtlextSubtleClientFind, 1);
  rb_define_method(klass, "find_sublet",   SubtlextSubtleSubletFind, 1);
  rb_define_method(klass, "focus_client",  SubtlextSubtleClientFocus, 1);
  rb_define_method(klass, "del_client",    SubtlextSubtleClientDel, 1);
  rb_define_method(klass, "add_tag",       SubtlextSubtleTagAdd, 1);
  rb_define_method(klass, "del_tag",       SubtlextSubtleTagDel, 1);
  rb_define_method(klass, "add_view",      SubtlextSubtleViewAdd, 1);
  rb_define_method(klass, "del_view",      SubtlextSubtleViewDel, 1);
  rb_define_method(klass, "current_view",  SubtlextSubtleViewCurrent, 0);
  rb_define_method(klass, "running?",      SubtlextSubtleRunning, 0);
  rb_define_method(klass, "to_s",          SubtlextSubtleToString, 0);

  /* Class: sublets */
  klass = rb_define_class("Sublet", rb_cObject);
  rb_define_attr(klass,   "name", 1, 0);
  rb_define_method(klass, "initialize",    SubtlextSubletInit, 1);
  rb_define_method(klass, "to_s",          SubtlextSubletToString, 0);

  /* Class: tag */
  klass = rb_define_class("Tag", rb_cObject);
  rb_define_attr(klass,   "id", 1, 0);
  rb_define_attr(klass,   "name", 1, 0);
  rb_define_method(klass, "initialize",    SubtlextTagInit, 1);
  rb_define_method(klass, "taggings",      SubtlextTagTaggings, 0);
  rb_define_method(klass, "to_s",          SubtlextTagToString, 0);

  /* Class: view */
  klass = rb_define_class("View", rb_cObject);
  rb_define_attr(klass,   "id", 1, 0);
  rb_define_attr(klass,   "win", 1, 0);
  rb_define_attr(klass,   "name", 1, 0);
  rb_define_method(klass, "initialize",    SubtlextViewInit, 1);
  rb_define_method(klass, "clients",       SubtlextViewClientList, 0);
  rb_define_method(klass, "tags",          SubtlextViewTagList, 0);
  rb_define_method(klass, "tag",           SubtlextViewTagAdd, 1);
  rb_define_method(klass, "untag",         SubtlextViewTagDel, 1);
  rb_define_method(klass, "jump",          SubtlextViewJump, 0);
  rb_define_method(klass, "current?",      SubtlextViewCurrent, 0);
  rb_define_method(klass, "to_s",          SubtlextViewToString, 0);
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
