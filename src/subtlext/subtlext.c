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
#include "config.h"
#include "shared.h"

Display *display = NULL;
int debug = 0;
static int refcount = 0;

static void SubtleKill(void *data);

/* SubtleNew {{{ */
static VALUE
SubtleNew(int argc,
  VALUE *argv,
  VALUE self)
{
  char *dispname = NULL;
  VALUE disp = Qnil, data = Qnil;

  rb_scan_args(argc, argv, "01", &disp);

  /* Open connection to server */
  if(!display)
    {
      if(RTEST(disp)) dispname = STR2CSTR(disp);
      if(!(display = XOpenDisplay(dispname)))
        {
          subSharedLogError("Can't open display `%s'.\n", (dispname) ? dispname : ":0.0");
          return Qnil;
        }
      XSetErrorHandler(subSharedLogXError);
      subSharedLogDebug("Connection opened (%s)\n", dispname);
    }
  refcount++;

  /* Create instance */
  data = Data_Wrap_Struct(self, 0, SubtleKill, display);
  rb_obj_call_init(data, 0, NULL);

  return data;
} /* }}} */

/* SubtleRunning {{{ */
static VALUE
SubtleRunning(VALUE self)
{
  char *prop = NULL;
  VALUE ret = Qfalse;
  
  prop = subSharedPropertyGet(DefaultRootWindow(display), XInternAtom(display, "UTF8_STRING", False),
    "_NET_WM_NAME", NULL);
  if(prop) 
    {
      if(!strncmp(prop, PKG_NAME, strlen(prop))) ret = Qtrue;
      subSharedLogDebug("wmname=%s\n", prop);
      free(prop);
    }

  return ret;
} /* }}} */

/* SubtleKill {{{ */
static void
SubtleKill(void *data)
{
  if(0 == --refcount) 
    {
      XCloseDisplay(display);
      display = NULL;
      subSharedLogDebug("Connection closed\n");
    }
} /* }}} */

/* SubtleVersion {{{ */
VALUE 
SubtleVersion(VALUE self)
{
  return rb_str_new2(PKG_VERSION);
} /* }}} */

/* SubtleDisplay {{{ */
VALUE 
SubtleDisplay(VALUE self)
{
  return rb_str_new2(DisplayString(display));
} /* }}} */

/* SubtleTags {{{ */
static VALUE
SubtleTags(VALUE self)
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

/* SubtleTagFind {{{ */
static VALUE
SubtleTagFind(VALUE self,
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

/* SubtleClients {{{ */
static VALUE
SubtleClients(VALUE self)
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

/* SubtleClientFind {{{ */
static VALUE
SubtleClientFind(VALUE self,
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

/* SubtleViews {{{ */
static VALUE
SubtleViews(VALUE self)
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

/* SubtleViewFind {{{ */
static VALUE
SubtleViewFind(VALUE self,
  VALUE name)
{
  int id = 0, size = 0;
  Window win = 0;
  char **names = NULL;
  VALUE klass = Qnil, view = Qnil;

  if(RTEST(name) && (id == subSharedViewFind(STR2CSTR(name), &win)))
    {
      names = subSharedPropertyList(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
      klass = rb_const_get(rb_mKernel, rb_intern("View"));
      view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(names[id]));
      rb_iv_set(view, "@id", INT2FIX(id));
      free(names);
      
      return view;
    }

  return Qnil;
} /* }}} */

/* SubtleViewCurrent {{{ */
static VALUE
SubtleViewCurrent(VALUE self)
{
  int size = 0;
  char **names = NULL;
  unsigned long *cv = NULL;
  VALUE klass = Qnil, view = Qnil;
  
  names = subSharedPropertyList(DefaultRootWindow(display), "_NET_DESKTOP_NAMES", &size);
  klass = rb_const_get(rb_mKernel, rb_intern("View"));
  cv    = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);
  view  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(names[*cv]));

  rb_iv_set(view, "@id", INT2FIX(*cv));
  free(names);
  free(cv);
      
  return view;
} /* }}} */

/* ViewInit {{{ */
static VALUE
ViewInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@name", name);

  return self;
} /* }}} */

/* ViewJump {{{ */
static VALUE
ViewJump(VALUE self)
{
  VALUE id = rb_iv_get(self, "@id");
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  data.l[0] = FIX2INT(id);

  subSharedMessage(DefaultRootWindow(display), "_NET_CURRENT_DESKTOP", data);

  return Qnil;
} /* }}} */

/* ViewCurrent {{{ */
static VALUE
ViewCurrent(VALUE self)
{
  unsigned long *cv = NULL;
  VALUE id = Qnil, ret = Qfalse;;
  
  id = rb_iv_get(self, "@id");
  cv = (unsigned long *)subSharedPropertyGet(DefaultRootWindow(display), XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL);

  if(FIX2INT(id) == *cv) ret = Qtrue;

  free(cv);
      
  return ret;
} /* }}} */

/* ViewToString {{{ */
static VALUE
ViewToString(VALUE self)
{
  VALUE name = rb_ivar_get(self, rb_intern("@name"));

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* TagInit {{{ */
static VALUE
TagInit(VALUE self,
  VALUE name)
{
  rb_iv_set(self, "@name", name);

  return self;
} /* }}} */

/* TagToString {{{ */
static VALUE
TagToString(VALUE self)
{
  VALUE name = rb_ivar_get(self, rb_intern("@name"));

  return RTEST(name) ? name : Qnil;
} /* }}} */

/* ClientInit {{{ */
static VALUE
ClientInit(VALUE self,
  VALUE win)
{
  char *wmname = subSharedWindowWMName(NUM2LONG(win));

  rb_iv_set(self, "@win", win);
  rb_iv_set(self, "@name", rb_str_new2(wmname));

  free(wmname);

  return self;
} /* }}} */

/* ClientTags {{{ */
static VALUE
ClientTags(VALUE self)
{
  Window win = 0;
  int i, size = 0;
  char **tags = NULL;
  unsigned long *flags = NULL;
  VALUE array = Qnil, method = Qnil, klass = Qnil, name = Qnil;

  name = rb_ivar_get(self, rb_intern("@name"));
  if(RTEST(name) && -1 != subSharedClientFind(STR2CSTR(name), &win))
    {
      method = rb_intern("new");
      klass  = rb_const_get(rb_mKernel, rb_intern("Tag"));
      flags  = (unsigned long *)subSharedPropertyGet(win, XA_CARDINAL, "SUBTLE_CLIENT_TAGS", NULL);
      tags   = subSharedPropertyList(DefaultRootWindow(display), "SUBTLE_TAG_LIST", &size);
      array  = rb_ary_new2(size);

      for(i = 0; i < size; i++)
        {
          if((int)*flags & (1L << (i + 1)))
            {
              VALUE t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));
              rb_ary_push(array, t);
            }
        }

      free(flags);
      free(tags);

      return array;
    }
  
  return Qnil;
} /* }}} */

/* ClientToString {{{ */
static VALUE
ClientToString(VALUE self)
{
  VALUE name = rb_ivar_get(self, rb_intern("@name"));

  return RTEST(name) ? name : Qnil;
} /* }}} */

void 
Init_subtlext(void)
{
  VALUE klass = Qnil;

  /* Class: subtle */
	klass = rb_define_class("Subtle", rb_cObject);
  rb_define_singleton_method(klass, "new", SubtleNew, -1);
	rb_define_method(klass, "version", SubtleVersion, 0);
	rb_define_method(klass, "display", SubtleDisplay, 0);
	rb_define_method(klass, "views", SubtleViews, 0);
	rb_define_method(klass, "tags", SubtleTags, 0);
	rb_define_method(klass, "clients", SubtleClients, 0);
  rb_define_method(klass, "find_view", SubtleViewFind, 1);
  rb_define_method(klass, "find_tag", SubtleTagFind, 1);
  rb_define_method(klass, "find_client", SubtleClientFind, 1);
  rb_define_method(klass, "current_view", SubtleViewCurrent, 0);
	rb_define_method(klass, "running?", SubtleRunning, 0);

  /* Class: view */
  klass = rb_define_class("View", rb_cObject);
  rb_define_attr(klass, "id", 1, 1);
  rb_define_attr(klass, "name", 1, 1);
  rb_define_method(klass, "initialize", ViewInit, 1);
  rb_define_method(klass, "jump", ViewJump, 0);
  rb_define_method(klass, "current?", ViewCurrent, 0);
  rb_define_method(klass, "to_s", ViewToString, 0);

  /* Class: tag */
  klass = rb_define_class("Tag", rb_cObject);
  rb_define_attr(klass, "id", 1, 1);
  rb_define_attr(klass, "name", 1, 1);
  rb_define_method(klass, "initialize", TagInit, 1);
  rb_define_method(klass, "to_s", TagToString, 0);

  /* Class: client */
  klass = rb_define_class("Client", rb_cObject);
  rb_define_attr(klass, "win", 1, 1);
  rb_define_attr(klass, "name", 1, 1);
  rb_define_method(klass, "initialize", ClientInit, 1);
  rb_define_method(klass, "tags", ClientTags, 0);
  rb_define_method(klass, "to_s", ClientToString, 0);
}
