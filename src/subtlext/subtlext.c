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
static VALUE subtle = Qnil;
static int refs = 0;

static void SubtleKill(void *data);

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
          printf("Can't open display `%s'.\n", (dispname) ? dispname : ":0.0");
          return(Qnil);
        }
      XSetErrorHandler(subSharedLogXError);
      printf("Connection open\n");
    }
  refs++;

  /* Create instance */
  data = Data_Wrap_Struct(self, 0, SubtleKill, display);
  rb_obj_call_init(data, 0, NULL);

  return(data);
}

static void
SubtleKill(void *data)
{
  if(0 == --refs) 
    {
      XCloseDisplay(display);
      display = NULL;
      printf("Connection closed\n");
    }
}

VALUE 
SubtleVersion(VALUE self)
{
  return(rb_str_new2(PKG_VERSION));
}

void 
Init_subtlext(void)
{
	subtle = rb_define_class("Subtle", rb_cObject);
  rb_define_singleton_method(subtle, "new", SubtleNew, -1);
	rb_define_method(subtle, "version", SubtleVersion, 0);
}
