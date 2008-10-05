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

static VALUE subtle = Qnil, view = Qnil, tag = Qnil, name = Qnil; ///< Common used symbols
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
      if(Qnil != disp) dispname = STR2CSTR(disp);
      if(!(display = XOpenDisplay(dispname)))
        {
          subSharedLogError("Can't open display `%s'.\n", (dispname) ? dispname : ":0.0");
          return(Qnil);
        }
      XSetErrorHandler(subSharedLogXError);
      subSharedLogDebug("Connection opened (%s)\n", dispname);
    }
  refcount++;

  /* Create instance */
  data = Data_Wrap_Struct(self, 0, SubtleKill, display);
  rb_obj_call_init(data, 0, NULL);

  return(data);
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

  return(ret);
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
  return(rb_str_new2(PKG_VERSION));
} /* }}} */

/* SubtleDisplay {{{ */
VALUE 
SubtleDisplay(VALUE self)
{
  return(rb_str_new2(DisplayString(display)));
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
      rb_ary_push(array, v);
    }

  return(array);
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

  return(array);
} /* }}} */


/* ViewInit {{{ */
static VALUE
ViewInit(VALUE self,
  VALUE vname)
{
  rb_ivar_set(self, name, vname);

  return(self);
} /* }}} */

/* ViewToString {{{ */
static VALUE
ViewToString(VALUE self)
{
  VALUE result = rb_ivar_get(self, name);

  if(T_STRING != rb_type(result))
    {
      subSharedLogWarn("Unknown value type\n");
      return(Qnil);
    }

  return(result);
} /* }}} */

/* TagInit {{{ */
static VALUE
TagInit(VALUE self,
  VALUE vname)
{
  rb_ivar_set(self, name, vname);

  return(self);
} /* }}} */

/* TagToString {{{ */
static VALUE
TagToString(VALUE self)
{
  VALUE result = rb_ivar_get(self, name);

  if(T_STRING != rb_type(result))
    {
      subSharedLogWarn("Unknown value type\n");
      return(Qnil);
    }

  return(result);
} /* }}} */

void 
Init_subtlext(void)
{
  /* Class: subtle */
	subtle = rb_define_class("Subtle", rb_cObject);
  rb_define_singleton_method(subtle, "new", SubtleNew, -1);
	rb_define_method(subtle, "version", SubtleVersion, 0);
	rb_define_method(subtle, "display", SubtleDisplay, 0);
	rb_define_method(subtle, "views", SubtleViews, 0);
	rb_define_method(subtle, "tags", SubtleTags, 0);
	rb_define_method(subtle, "running?", SubtleRunning, 0);

  /* Class: view */
  view = rb_define_class("View", rb_cObject);
  rb_define_method(view, "initialize", ViewInit, 1);
  rb_define_method(view, "to_s", ViewToString, 0);

  /* Class: tag */
  tag = rb_define_class("Tag", rb_cObject);
  rb_define_method(tag, "initialize", TagInit, 1);
  rb_define_method(tag, "to_s", TagToString, 0);

  /* Attrs: view */
  rb_define_attr(view, "name", 1, 1);

  /* Attrs: tag */
  rb_define_attr(tag, "name", 1, 1);

  /* Common used symbols */
  name = rb_intern("@name");
}
