
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

/* SubtlextRunning {{{ */
static VALUE
SubtlextRunning(void)
{
  Window *wmcheck = NULL;
  VALUE ret = Qfalse;

  if((wmcheck = subSharedWindowWMCheck()))
    {
      char *prop = subSharedPropertyGet(*wmcheck, XInternAtom(display, "UTF8_STRING", False),
        "_NET_WM_NAME", NULL);
      if(prop) 
        {
          if(!strncmp(prop, PKG_NAME, strlen(prop))) ret = Qtrue;
          subSharedLogDebug("wmname=%s\n", prop);
          free(prop);
        }
      free(wmcheck);
    }

  return ret;
} /* }}} */

/* SubtlextTagFind {{{ */
static VALUE
SubtlextTagFind(VALUE value)
{
  int id = -1;
  VALUE klass = Qnil, tag = Qnil, name = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  klass = rb_const_get(rb_mKernel, rb_intern("Tag")); ///< Get Tag class

  /* Check object */
  switch(rb_type(value))
    {
      case T_STRING:
        if(-1 == (id = subSharedTagFind(STR2CSTR(value)))) ///< Check tag existance
          {
            /* Send tag to subtle */
            snprintf(data.b, sizeof(data.b), "%s", STR2CSTR(value));
            subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, True);
          }

        if(-1 != id || -1 != (id = subSharedTagFind(STR2CSTR(value)))) ///< Check tag existance
          {
            /* Create new tag */
            tag = rb_funcall(klass, rb_intern("new"), 1, value);
            rb_iv_set(tag, "@id", INT2FIX(id));
            rb_iv_set(tag, "@name", value);

            return tag;
          }
          printf("id=%d, name=%s\n", id, STR2CSTR(value));

          rb_raise(rb_eArgError, "Unknown value type");
          return Qnil;
        break;
      case T_OBJECT:
      case T_CLASS:
        if(rb_obj_is_instance_of(value, klass)) ///< Check object instance
          {
            if(-1 == (id = rb_iv_get(value, "@id")))
              {
                name = rb_iv_get(value, "@name");

                /* Send tag to subtle */
                snprintf(data.b, sizeof(data.b), "%s", STR2CSTR(name));
                subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, True);

                if(-1 == (id = subSharedTagFind(STR2CSTR(name)))) ///< Check tag existance
                  {
                    rb_raise(rb_eArgError, "Unknown value type");
                    return Qnil;
                  }                

                rb_iv_set(value, "@id", INT2FIX(id)); ///< Update tag id
              }

            return value;
          }
    }

  rb_raise(rb_eArgError, "Unknown value type");
  return Qnil;
} /* }}} */

/* Client {{{ */
/* SubtlextClientInit {{{ */
static VALUE
SubtlextClientInit(VALUE self,
  VALUE win)
{
  char *wmname = subSharedWindowWMName(NUM2LONG(win));

  rb_iv_set(self, "@win", win);
  rb_iv_set(self, "@name", rb_str_new2(wmname));

  free(wmname);

  return self;
} /* }}} */

/* SubtlextClientTags {{{ */
static VALUE
SubtlextClientTags(VALUE self)
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

/* SubtlextClientTag {{{ */
static VALUE
SubtlextClientTag(VALUE self,
  VALUE value)
{
  VALUE klass = Qnil, tag = Qnil, id = Qnil, win = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  klass = rb_const_get(rb_mKernel, rb_intern("Tag")); ///< Get Tag class
  tag   = SubtlextTagFind(value); ///< Find or create tag
  id    = rb_iv_get(tag,  "@id");
  win   = rb_iv_get(self, "@win");

  /* Send data */
  data.l[0] = FIX2LONG(id);
  data.l[1] = FIX2LONG(win);

  subSharedMessage(DefaultRootWindow(display), "SUBTLE_CLIENT_TAG", data, True);

  return Qnil;
} /* }}} */

/* SubtlextClientUntag {{{ */
static VALUE
SubtlextClientUntag(VALUE self,
  VALUE value)
{  
  return Qnil;
} /* }}} */

/* SubtlextClientToString {{{ */
static VALUE
SubtlextClientToString(VALUE self)
{
  VALUE name = rb_ivar_get(self, rb_intern("@name"));

  return RTEST(name) ? name : Qnil;
} /* }}} */
/* }}} */

/* Subtle {{{ */
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

  /* Open connection to server */
  if(!display)
    {
      if(RTEST(disp)) dispname = STR2CSTR(disp);
      if(!(display = XOpenDisplay(dispname)))
        {
          subSharedLogError("Can't open display `%s'\n", (dispname) ? dispname : ":0.0");

          return Qnil;
        }
      XSetErrorHandler(subSharedLogXError);

      /* Check if subtle is running */
      if(Qfalse == SubtlextRunning())
        {
          XCloseDisplay(display);
          display = NULL;
          
          rb_raise(rb_eRuntimeError, "%s is not running", PKG_NAME);

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
  return SubtlextRunning();
} /* }}} */

/* SubtlextSubtleTagList {{{ */
static VALUE
SubtlextSubtleTagList(VALUE self)
{
  int i, size = 0;
  char **tags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  method = rb_intern("new");
  klass  = rb_const_get(rb_mKernel, rb_intern("View"));
  tags   = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
  array  = rb_ary_new2(size);

  for(i = 0; i < size; i++)
    {
      VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));
      rb_ary_push(array, t);
    }

  return array;
} /* }}} */

/* SubtlextSubtleTagFind {{{ */
static VALUE
SubtlextSubtleTagFind(VALUE self,
  VALUE name)
{
  int id = 0, size = 0;
  char **names = NULL;
  VALUE klass  = Qnil, tag = Qnil;

  if(RTEST(name) && (id = subSharedTagFind(STR2CSTR(name))))
    {
      names = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
      klass = rb_const_get(rb_mKernel, rb_intern("Tag"));
      tag   = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(names[id]));
      rb_iv_set(tag, "@id", INT2FIX(id));
      free(names);

      return tag;
    }

  return Qnil;
} /* }}} */

/* SubtlextSubtleTagAdd {{{ */
static VALUE
SubtlextSubtleTagAdd(VALUE self,
  VALUE value)
{
  int id = 0, rtype = T_NIL;
  VALUE klass  = Qnil, tag = Qnil, name = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  klass = rb_const_get(rb_mKernel, rb_intern("Tag")); ///< Get Tag class
  rtype = rb_type(value);

  /* Check type */
  switch(rtype)
    {
      case T_STRING: name = value; break;
      case T_OBJECT:
      case T_CLASS:
        if(rb_obj_is_instance_of(name, klass))
          {
            name = rb_iv_get(value, "@name");
            break;
          }
      default: 
        rb_raise(rb_eArgError, "Unknown value type");
        return Qnil;
    }

  /* Send data */
  snprintf(data.b, sizeof(data.b), STR2CSTR(name));
  subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_NEW", data, True);

  /* Check tag */
  id = subSharedTagFind(STR2CSTR(name));
  if(-1 != id)
    {
      if(T_STRING == rtype)
        {
          /* Create new tag */
          tag = rb_funcall(klass, rb_intern("new"), 1, name);
          rb_iv_set(tag, "@id", INT2FIX(id));

          return tag;
        }

      return value;
    }

  return Qnil;
} /* }}} */

/* SubtlextSubtleTagDel {{{ */
static VALUE
SubtlextSubtleTagDel(VALUE self,
  VALUE value)
{
  VALUE name = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check type */
  switch(rb_type(value))
    {
      case T_STRING: name = value; break;
      case T_OBJECT:
      case T_CLASS:
        if(rb_obj_is_instance_of(name, 
          rb_const_get(rb_mKernel, rb_intern("Tag"))))
          {
            name = rb_iv_get(value, "@name");
            break;
          }
      default: 
        rb_raise(rb_eArgError, "Unknown value type");
        return Qnil;
    }

  /* Send data */
  snprintf(data.b, sizeof(data.b), STR2CSTR(name));
  subSharedMessage(DefaultRootWindow(display), "SUBTLE_TAG_KILL", data, True);

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
          VALUE c = rb_funcall(klass, method, 1, INT2NUM(clients[i]));
          rb_ary_push(array, c);
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
  Window win = 0;
  VALUE klass = Qnil, client = Qnil;

  if(RTEST(name) && -1 != subSharedClientFind(STR2CSTR(name), &win))
    {
      char *wmname = subSharedWindowWMName(win);

      klass  = rb_const_get(rb_mKernel, rb_intern("Client"));
      client = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(wmname));
      free(wmname);

      return client;
    }

  return Qnil;
} /* }}} */

/* SubtlextSubtleViewList {{{ */
static VALUE
SubtlextSubtleViewList(VALUE self)
{
  int i, size = 0;
  char **views = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil;
  
  method = rb_intern("new");
  klass  = rb_const_get(rb_mKernel, rb_intern("View"));
  views  = subSharedPropertyList(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
  array  = rb_ary_new2(size);

  for(i = 0; i < size; i++)
    {
      VALUE v = rb_funcall(klass, method, 1, rb_str_new2(views[i]));
      rb_iv_set(v, "@id", INT2FIX(i));
      rb_ary_push(array, v);
    }

  return array;
} /* }}} */

/* SubtlextSubtleViewFind {{{ */
static VALUE
SubtlextSubtleViewFind(VALUE self,
  VALUE name)
{
  int id = 0, size = 0;
  Window win = 0;
  char **names = NULL;
  VALUE klass = Qnil, view = Qnil;

  if(RTEST(name))
    {
      id = subSharedViewFind(STR2CSTR(name), &win);
      if(-1 != id)
        {
          names = subSharedPropertyList(DefaultRootWindow(display), 
            "_NET_DESKTOP_NAMES", &size);
          klass = rb_const_get(rb_mKernel, rb_intern("View"));
          view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(names[id]));
          rb_iv_set(view, "@id", INT2FIX(id));
          free(names);
          
          return view;
        }
    }

  return Qnil;
} /* }}} */

/* SubtlextSubtleViewAdd {{{ */
static VALUE
SubtlextSubtleViewAdd(VALUE self,
  VALUE value)
{
  Window win = 0;
  int id = 0, rtype = T_NIL;
  VALUE klass  = Qnil, view = Qnil, name = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  klass = rb_const_get(rb_mKernel, rb_intern("View")); ///< Get View class
  rtype = rb_type(value);

  /* Check type */
  switch(rtype)
    {
      case T_STRING: name = value; break;
      case T_OBJECT:
      case T_CLASS:
        if(rb_obj_is_instance_of(name, klass))
          {
            name = rb_iv_get(value, "@name");
            break;
          }
      default: 
        rb_raise(rb_eArgError, "Unknown value type");
        return Qnil;
    }

  /* Send data */
  snprintf(data.b, sizeof(data.b), STR2CSTR(name));
  subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_NEW", data, True);

  /* Check view */
  if(-1 != (id = subSharedViewFind(STR2CSTR(name), &win)))
    {
      if(T_STRING == rtype)
        {
printf("1\n");
          /* Create new view */
          view = rb_funcall(klass, rb_intern("new"), 1, name);
          rb_iv_set(view, "@id",  INT2FIX(id));
          rb_iv_set(view, "@win", NUM2LONG(win));
printf("2\n");

          return view;
        }

      return value;
    }

  return Qnil;
} /* }}} */

/* SubtlextSubtleViewDel {{{ */
static VALUE
SubtlextSubtleViewDel(VALUE self,
  VALUE value)
{
  VALUE name = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check type */
  switch(rb_type(value))
    {
      case T_STRING: name = value; break;
      case T_OBJECT:
      case T_CLASS:
        if(rb_obj_is_instance_of(name, 
          rb_const_get(rb_mKernel, rb_intern("View"))))
          {
            name = rb_iv_get(value, "@name");
            break;
          }
      default: 
        rb_raise(rb_eArgError, "Unknown value type");
        return Qnil;
    }

  /* Send data */
  snprintf(data.b, sizeof(data.b), STR2CSTR(name));
  subSharedMessage(DefaultRootWindow(display), "SUBTLE_VIEW_KILL", data, True);

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
/* }}} */

/* Tag {{{ */
/* SubtlextTagInit {{{ */
static VALUE
SubtlextTagInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@id",   INT2FIX(-1));
  rb_iv_set(self, "@name", name);

  return self;
} /* }}} */

/* SubtlextTagToString {{{ */
static VALUE
SubtlextTagToString(VALUE self)
{
  VALUE name = rb_ivar_get(self, rb_intern("@name"));

  return RTEST(name) ? name : Qnil;
} /* }}} */
/* }}} */

/* View {{{ */
/* SubtlextViewInit {{{ */
static VALUE
SubtlextViewInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@name", name);

  return self;
} /* }}} */

/* SubtlextViewTags {{{ */
static VALUE
SubtlextViewTags(VALUE self)
{
  Window win = 0;
  int i, size = 0;
  char **tags = NULL;
  unsigned long *flags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil, name = Qnil;

  name = rb_ivar_get(self, rb_intern("@name"));
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
  VALUE name = rb_ivar_get(self, rb_intern("@name"));

  return RTEST(name) ? name : Qnil;
} /* }}} */
/* }}} */

 /** Init_subtlext {{{
  * @brief Extension entry point
  **/

void 
Init_subtlext(void)
{
  VALUE klass = Qnil;

  /* Class: client {{{ */
  klass = rb_define_class("Client", rb_cObject);
  rb_define_attr(klass, "id", 1, 1);
  rb_define_attr(klass, "win", 1, 1);
  rb_define_attr(klass, "name", 1, 1);
  rb_define_method(klass, "initialize", SubtlextClientInit, 1);
  rb_define_method(klass, "tags",       SubtlextClientTags, 0);
  rb_define_method(klass, "tag",        SubtlextClientTag, 1);
  rb_define_method(klass, "untag",      SubtlextClientUntag, 1);
//  rb_define_method(klass, "focus",   ClientFocus, 0);
//  rb_define_method(klass, "focus?",  ClientFocus, 0);
  rb_define_method(klass, "to_s",       SubtlextClientToString, 0);
  /* }}} */

  /* Class: subtle {{{ */
  klass = rb_define_class("Subtle", rb_cObject);
  rb_define_singleton_method(klass, "new", SubtlextSubtleNew, -1);
  rb_define_method(klass, "version",      SubtlextSubtleVersion, 0);
  rb_define_method(klass, "display",      SubtlextSubtleDisplay, 0);
  rb_define_method(klass, "views",        SubtlextSubtleViewList, 0);
  rb_define_method(klass, "tags",         SubtlextSubtleTagList, 0);
  rb_define_method(klass, "clients",      SubtlextSubtleClientList, 0);
  rb_define_method(klass, "find_view",    SubtlextSubtleViewFind, 1);
  rb_define_method(klass, "find_tag",     SubtlextSubtleTagFind, 1);
  rb_define_method(klass, "find_client",  SubtlextSubtleClientFind, 1);
  rb_define_method(klass, "add_tag",      SubtlextSubtleTagAdd, 1);
  rb_define_method(klass, "del_tag",      SubtlextSubtleTagDel, 1);
  rb_define_method(klass, "add_view",     SubtlextSubtleViewAdd, 1);
  rb_define_method(klass, "del_view",     SubtlextSubtleViewDel, 1);
  rb_define_method(klass, "current_view", SubtlextSubtleViewCurrent, 0);
  rb_define_method(klass, "running?",     SubtlextSubtleRunning, 0);
  /* }}} */

  /* Class: tag {{{ */
  klass = rb_define_class("Tag", rb_cObject);
  rb_define_attr(klass, "id", 1, 1);
  rb_define_attr(klass, "name", 1, 1);
  rb_define_method(klass, "initialize", SubtlextTagInit, 1);
  rb_define_method(klass, "to_s",       SubtlextTagToString, 0);
  /* }}} */

  /* Class: view {{{ */
  klass = rb_define_class("View", rb_cObject);
  rb_define_attr(klass, "id", 1, 1);
  rb_define_attr(klass, "win", 1, 1);
  rb_define_attr(klass, "name", 1, 1);
  rb_define_method(klass, "initialize", SubtlextViewInit, 1);
  rb_define_method(klass, "tags",       SubtlextViewTags, 0);
//  rb_define_method(klass, "add_tag", ViewTagAdd, 1);
//  rb_define_method(klass, "del_tag", ViewTagDel, 1);
  rb_define_method(klass, "jump",       SubtlextViewJump, 0);
  rb_define_method(klass, "current?",   SubtlextViewCurrent, 0);
  rb_define_method(klass, "to_s",       SubtlextViewToString, 0);
  /* }}} */
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
