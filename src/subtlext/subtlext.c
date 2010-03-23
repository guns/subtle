
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

#include <unistd.h>
#include <locale.h>
#include "subtlext.h"

Display *display = NULL;
VALUE mod = Qnil;

static const char *klasses[] = { 
  "Client", "Gravity", "View", "Tag", "Tray", "Screen", "Sublet" 
};

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

/* SubtlextPidReader {{{ */
/*
 * call-seq: pid => Fixnum
 *
 * Get window pid
 *
 *  object.pid
 *  => 123
 **/

static VALUE
SubtlextPidReader(VALUE self)
{
  Window win = None;
  VALUE pid = Qnil;

  /* Load on demand */
  if(NIL_P((pid = rb_iv_get(self, "@pid"))) &&
     (win = NUM2LONG(rb_iv_get(self, "@win"))))
    {
      int *id = NULL;

      /* Get pid */
      if((id = (int *)subSharedPropertyGet(display, win, XA_CARDINAL,
          XInternAtom(display, "_NET_WM_PID", False), NULL)))
        {
          pid = INT2FIX(*id);

          rb_iv_set(self, "@pid", pid);

          free(id);
        }
    }

  return pid;
} /* }}} */

/* SubtlextTag {{{ */
static VALUE
SubtlextTag(VALUE self,
  VALUE value,
  char *action)
{
  VALUE tag = Qnil;

  /* Find tag */
  if(Qnil != (tag = subSubtlextFind(SUB_TYPE_TAG, value, True)))
    {
      VALUE oid = Qnil, tid = Qnil;

      oid = rb_iv_get(self, "@id");
      tid = rb_iv_get(tag, "@id");

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
  else rb_raise(rb_eStandardError, "Failed adding tag");

  return Qnil;
} /* }}} */

/* SubtlextTagAdd {{{ */
/*
 * call-seq: tag(value) -> Subtlext::Tag
 *           +(value)   -> Subtlext::Tag
 *
 * Add an existing tag to window
 *
 *  object.tag("subtle")
 *  => #<Subtlext::Tag:xxx>
 *
 *  object + "subtle"
 *  => #<Subtlext::Tag:xxx>
 */

static VALUE
SubtlextTagAdd(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, "SUBTLE_WINDOW_TAG");
} /* }}} */

/* SubtlextTagDel {{{ */
/*
 * call-seq: untag(value) -> Subtlext::Tag
 *           -(value)     -> Subtlext::Tag
 *
 * Remove an existing tag from window
 *
 *  object.untag("subtle")
 *  => #<Subtlext::Tag:xxx>
 *
 *  object - "subtle"
 *  => #<Subtlext::Tag:xxx>
 */

static VALUE
SubtlextTagDel(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, "SUBTLE_WINDOW_UNTAG");
} /* }}} */

/* SubtlextTagList {{{ */
/*
 * call-seq: tags -> Array
 *
 * Get list of tags for window
 *
 *  object.tags
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 */

static VALUE
SubtlextTagList(VALUE self)
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
      flags  = (unsigned long *)subSharedPropertyGet(display, win, XA_CARDINAL, 
        XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);
      tags   = subSharedPropertyStrings(display, DefaultRootWindow(display), 
        XInternAtom(display, "SUBTLE_TAG_LIST", False), &size);

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

/* SubtlextTagAsk {{{ */
VALUE
SubtlextTagAsk(VALUE self,
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

      if((tags = (unsigned long *)subSharedPropertyGet(display, win, XA_CARDINAL, 
          XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL)))
        {
          if(*tags & (1L << (id + 1)))
            ret = Qtrue;
          
          free(tags);
        }
    }

  return ret;
} /* }}} */

/* SubtlextClick {{{ */
/*
 * call-seq: click(button, x, y) -> nil
 *
 * Emulate a click on a window
 *
 *  object.click(2)
 *  => nil
 */

static VALUE 
SubtlextClick(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE button = Qnil, x = Qnil, y = Qnil;

  rb_scan_args(argc, argv, "03", &button, &x, &y);

  /* Check object type */
  if(FIXNUM_P(button))
    {
      Window win = None, subwin = None;
      XEvent event = { 0 };

      win = NUM2LONG(rb_iv_get(self, "@win"));

      /* Assemble button event */
      event.type                  = EnterNotify;
      event.xcrossing.window      = win;
      event.xcrossing.root        = DefaultRootWindow(display);
      event.xcrossing.subwindow   = win;
      event.xcrossing.same_screen = True;
      event.xcrossing.x           = FIXNUM_P(x) ? FIX2INT(x) : 5;
      event.xcrossing.y           = FIXNUM_P(y) ? FIX2INT(y) : 5;

      /* Translate window x/y to root x/y */
      XTranslateCoordinates(display, event.xcrossing.window, 
        event.xcrossing.root, event.xcrossing.x, event.xcrossing.y,
        &event.xcrossing.x_root, &event.xcrossing.y_root, &subwin);

      //XSetInputFocus(display, event.xany.window, RevertToPointerRoot, CurrentTime);
      XSendEvent(display, win, True, EnterWindowMask, &event);

      /* Send button press event */
      event.type           = ButtonPress;
      event.xbutton.button = FIX2INT(button);

      XSendEvent(display, win, True, ButtonPressMask, &event);
      XFlush(display);

      usleep(12000);

      /* Send button release event */
      event.type = ButtonRelease;

      XSendEvent(display, win, True, ButtonReleaseMask, &event);
      XFlush(display);
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* SubtlextFocus {{{ */
/*
 * call-seq: focus -> nil
 *
 * Set focus to window
 *
 *  object.focus
 *  => nil
 */

VALUE
SubtlextFocus(VALUE self)
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

/* SubtlextFocusAsk {{{ */
/*
 * call-seq: has_focus? -> true or false
 *
 * Check if window has focus
 *
 *  object.focus?
 *  => true
 *
 *  object.focus?
 *  => false
 */

static VALUE
SubtlextFocusAsk(VALUE self)
{
  VALUE ret = Qfalse, win = Qnil;
  unsigned long *focus = NULL;

  /* Fetch data */
  win   = rb_iv_get(self, "@win");
  focus = (unsigned long *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_WINDOW, XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL);

  /* Check if win has focus */
  if(RTEST(win) && focus)
    {
      if(*focus == NUM2LONG(win)) ret = Qtrue;
      free(focus);
    }

  return ret;
} /* }}} */

/* Exported */

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

      if(!setlocale(LC_CTYPE, "")) XSupportsLocale();

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

/* subSubtlextConcat {{{ */
VALUE
subSubtlextConcat(VALUE str1,
  VALUE str2)
{
  VALUE ret = Qnil;

  /* Check value */
  if(RTEST(str1) && RTEST(str2) && T_STRING == rb_type(str1))
    {
      VALUE string = str2;
      
      /* Convert argument to string */
      if(T_STRING != rb_type(str2) && rb_respond_to(str2, rb_intern("to_s")))
        string = rb_funcall(str2, rb_intern("to_s"), 0, NULL);

      /* Concat strings */
      if(T_STRING == rb_type(string))
        ret = rb_str_cat(str1, RSTRING_PTR(string), RSTRING_LEN(string));
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return ret;
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
              { CHAR2SYM("role"),     SUB_MATCH_ROLE     },
              { CHAR2SYM("pid"),      SUB_MATCH_PID      }
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
                case T_FIXNUM:
                  snprintf(buf, sizeof(buf), "%ld", FIX2INT(match));
              }
          }
        break; /* }}} */        
      case T_OBJECT: /* {{{ */
        if(rb_obj_is_instance_of(value, rb_const_get(mod, 
            rb_intern(klasses[type])))) 
            return value;
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
            rb_raise(rb_eArgError, "Unknwon value type `%s'", 
              rb_obj_classname(value));

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

                rb_iv_set(object, "@id",       INT2FIX(id)); 
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
  //if(!NIL_P(object)) rb_iv_set(object, "@id", INT2FIX(id)); ///< Set id

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

/* subSubtlextAssoc {{{ */
VALUE
subSubtlextAssoc(VALUE self,
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
        names = subSharedPropertyStrings(display, DefaultRootWindow(display), 
          XInternAtom(display, "_NET_DESKTOP_NAMES", False), &size);
        wins  = (Window *)subSharedPropertyGet(display, DefaultRootWindow(display), 
          XA_WINDOW, XInternAtom(display, "_NET_VIRTUAL_ROOTS", False), NULL);
        break;
    }

  /* Populate array */
  for(i = 0; i < size; i++)
    {
      unsigned long *flags = (unsigned long *)subSharedPropertyGet(display, 
        wins[i], XA_CARDINAL, 
        XInternAtom(display, "SUBTLE_WINDOW_TAGS", False), NULL);

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

/* Plugin */

/* Init_subtlext {{{ */
/*
 * Subtlext is the module of the extension
 */
void
Init_subtlext(void)
{
  VALUE client = Qnil, color = Qnil, geometry = Qnil, gravity = Qnil;
  VALUE icon = Qnil, screen = Qnil, subtle = Qnil, sublet = Qnil;
  VALUE tag = Qnil, tray = Qnil, view = Qnil, window = Qnil;

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

  /* Singleton methods */
  rb_define_singleton_method(client, "find",    subClientFind,    1);
  rb_define_singleton_method(client, "current", subClientCurrent, 0);
  rb_define_singleton_method(client, "all",     subClientAll,     0);

  /* General methods */
  rb_define_method(client, "tags",         SubtlextTagList,   0);
  rb_define_method(client, "has_tag?",     SubtlextTagAsk,    1);
  rb_define_method(client, "tag",          SubtlextTagAdd,    1);
  rb_define_method(client, "untag",        SubtlextTagDel,    1);
  rb_define_method(client, "click",        SubtlextClick,    -1);
  rb_define_method(client, "focus",        SubtlextFocus,     0);
  rb_define_method(client, "has_focus?",   SubtlextFocusAsk,  0);

  /* Class methods */
  rb_define_method(client, "initialize",   subClientInit,            1);
  rb_define_method(client, "update",       subClientUpdate,          0);
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
  rb_define_method(client, "to_str",       subClientToString,        0);
  rb_define_method(client, "gravity",      subClientGravityReader,   0);
  rb_define_method(client, "gravity=",     subClientGravityWriter,   1);
  rb_define_method(client, "screen",       subClientScreenReader,    0);
  rb_define_method(client, "screen=",      subClientScreenWriter,    1);  
  rb_define_method(client, "geometry",     subClientGeometryReader,  0);
  rb_define_method(client, "geometry=",    subClientGeometryWriter, -1);
  rb_define_method(client, "pid",          SubtlextPidReader,        0);
  rb_define_method(client, "alive?",       subClientAliveAsk,        0);
  rb_define_method(client, "kill",         subClientKill,            0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(client), "[]", "find");

  /* Aliases */
  rb_define_alias(client, "+",    "tag");
  rb_define_alias(client, "-",    "untag");
  rb_define_alias(client, "save", "update");
  rb_define_alias(client, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Color
   *
   * Color class for interaction with colors
   */

  color = rb_define_class_under(mod, "Color", rb_cObject);

  /* Pixel number */
  rb_define_attr(color, "pixel", 1, 0);

  /* Class methods */
  rb_define_method(color, "initialize", subColorInit,         1);
  rb_define_method(color, "to_str",     subColorToString,     0);
  rb_define_method(color, "+",          subColorOperatorPlus, 1);

  /* Aliases */
  rb_define_alias(color, "to_s", "to_str");

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

  /* Class methods */
  rb_define_method(geometry, "initialize", subGeometryInit,     -1);
  rb_define_method(geometry, "to_a",       subGeometryToArray,   0);
  rb_define_method(geometry, "to_hash",    subGeometryToHash,    0);
  rb_define_method(geometry, "to_str",     subGeometryToString,  0);

  /* Aliases */
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

  /* Singleton methods */
  rb_define_singleton_method(gravity, "find", subGravityFind, 1);
  rb_define_singleton_method(gravity, "all",  subGravityAll,  0);

  /* Class methods */
  rb_define_method(gravity, "initialize", subGravityInit,     1);
  rb_define_method(gravity, "update",     subGravityUpdate,   0);
  rb_define_method(gravity, "geometry",   subGravityGeometry, 0);
  rb_define_method(gravity, "to_str",     subGravityToString, 0);
  rb_define_method(gravity, "to_sym",     subGravityToSym,    0);
  rb_define_method(gravity, "kill",       subGravityKill,     0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(gravity), "[]", "find");

  /* Aliases */
  rb_define_alias(gravity, "save", "update");
  rb_define_alias(gravity, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Icon
   *
   * Icon class for interaction with icons
   */

  icon = rb_define_class_under(mod, "Icon", rb_cObject);

  /* Icon width */
  rb_define_attr(icon, "width", 1, 0);

  /* Icon height */
  rb_define_attr(icon, "height", 1, 0);

  /* Singleton methods */
  rb_define_singleton_method(icon, "new", subIconNew, -1);

  /* Class methods */
  rb_define_method(icon, "draw",   subIconDraw,         2);
  rb_define_method(icon, "clear",  subIconClear,        0);
  rb_define_method(icon, "to_str", subIconToString,     0);
  rb_define_method(icon, "+",      subIconOperatorPlus, 1);
  rb_define_method(icon, "*",      subIconOperatorMult, 1);

  /* Aliases */
  rb_define_alias(icon, "to_s", "to_str");

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

  /* Singleton methods */
  rb_define_singleton_method(screen, "find",    subScreenFind,    1);
  rb_define_singleton_method(screen, "current", subScreenCurrent, 0);
  rb_define_singleton_method(screen, "all",     subScreenAll,     0);

  /* Class methods */
  rb_define_method(screen, "initialize", subScreenInit,       1);
  rb_define_method(screen, "update",     subScreenUpdate,     0);
  rb_define_method(screen, "clients",    subScreenClientList, 0);
  rb_define_method(screen, "to_str",     subScreenToString,   0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(screen), "[]", "find");

  /* Aliases */
  rb_define_alias(screen, "save", "update");
  rb_define_alias(screen, "to_s", "to_str");

  /*
   * Document-class: Subtlext::Subtle
   *
   * Module for interaction with the window manager
   */

  subtle = rb_define_module_under(mod, "Subtle");

  /* Singleton methods */
  rb_define_singleton_method(subtle, "display",        subSubtleDisplayReader, 0);
  rb_define_singleton_method(subtle, "clients",        subClientAll,           0);
  rb_define_singleton_method(subtle, "gravities",      subGravityAll,          0);
  rb_define_singleton_method(subtle, "screens",        subScreenAll,           0);
  rb_define_singleton_method(subtle, "sublets",        subSubletAll,           0);
  rb_define_singleton_method(subtle, "tags",           subTagAll,              0);
  rb_define_singleton_method(subtle, "trays",          subTrayAll,             0);
  rb_define_singleton_method(subtle, "views",          subViewAll,             0);
  rb_define_singleton_method(subtle, "find_client",    subClientFind,          1);
  rb_define_singleton_method(subtle, "find_gravity",   subGravityFind,         1);
  rb_define_singleton_method(subtle, "find_screen",    subScreenFind,          1);
  rb_define_singleton_method(subtle, "find_sublet",    subSubletFind,          1);
  rb_define_singleton_method(subtle, "find_tag",       subTagFind,             1);
  rb_define_singleton_method(subtle, "find_tray",      subTrayFind,            1);
  rb_define_singleton_method(subtle, "find_view",      subViewFind,            1);
  rb_define_singleton_method(subtle, "focus",          subSubtleFocus,         1);
  rb_define_singleton_method(subtle, "add_tag",        subSubtleTagAdd,        1);
  rb_define_singleton_method(subtle, "add_view",       subSubtleViewAdd,       1);
  rb_define_singleton_method(subtle, "del_client",     subSubtleClientDel,     1);
  rb_define_singleton_method(subtle, "del_gravity",    subSubtleGravityDel,    1);
  rb_define_singleton_method(subtle, "del_sublet",     subSubtleSubletDel,     1);
  rb_define_singleton_method(subtle, "del_tag",        subSubtleTagDel,        1);
  rb_define_singleton_method(subtle, "del_tray",       subSubtleTrayDel,       1);
  rb_define_singleton_method(subtle, "del_view",       subSubtleViewDel,       1);
  rb_define_singleton_method(subtle, "current_view",   subViewCurrent,         0);
  rb_define_singleton_method(subtle, "current_client", subClientCurrent,       0);
  rb_define_singleton_method(subtle, "current_screen", subScreenCurrent,       0);
  rb_define_singleton_method(subtle, "running?",       subSubtleRunningAsk,    0);
  rb_define_singleton_method(subtle, "reload_config",  subSubtleReloadConfig,  0);
  rb_define_singleton_method(subtle, "reload_sublets", subSubtleReloadSublets, 0);
  rb_define_singleton_method(subtle, "quit",           subSubtleQuit,          0);
  rb_define_singleton_method(subtle, "spawn",          subSubtleSpawn,         1);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(subtle), "+", "add_tag");
  rb_define_alias(rb_singleton_class(subtle), "-", "del_tag");

  /*
   * Document-class: Subtlext::Sublet
   *
   * Class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);

  /* Name of the sublet */
  rb_define_attr(sublet, "name", 1, 0);

  /* Singleton methods */
  rb_define_singleton_method(sublet, "find", subSubletFind, 1);
  rb_define_singleton_method(sublet, "all",  subSubletAll,  0);

  /* Class methods */
  rb_define_method(sublet, "initialize",  subSubletInit,             1);
  rb_define_method(sublet, "update",      subSubletUpdate,           0);
  rb_define_method(sublet, "data",        subSubletDataReader,       0);
  rb_define_method(sublet, "data=",       subSubletDataWriter,       1);
  rb_define_method(sublet, "background=", subSubletBackgroundWriter, 1);
  rb_define_method(sublet, "to_str",      subSubletToString,         0);
  rb_define_method(sublet, "kill",        subSubletKill,             0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(sublet), "[]", "find");

  /* Aliases */
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

  /* Singleton methods */
  rb_define_singleton_method(tag, "find", subTagFind, 1);
  rb_define_singleton_method(tag, "all",  subTagAll,  0);

  /* Class methods */
  rb_define_method(tag, "initialize", subTagInit,     1);
  rb_define_method(tag, "update",     subTagUpdate,   0);
  rb_define_method(tag, "clients",    subTagClients,  0);
  rb_define_method(tag, "views",      subTagViews,    0);
  rb_define_method(tag, "to_str",     subTagToString, 0);
  rb_define_method(tag, "kill",       subTagKill,     0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(tag), "[]", "find");

  /* Aliases */
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

  /* Singleton methods */
  rb_define_singleton_method(tray, "find", subTrayFind, 1);
  rb_define_singleton_method(tray, "all",  subTrayAll,  0);

  /* General methods */
  rb_define_method(tray, "click",      SubtlextClick,    -1);
  rb_define_method(tray, "focus",      SubtlextFocus,     0);
  rb_define_method(tray, "has_focus?", SubtlextFocusAsk,  0);

  /* Class methods */
  rb_define_method(tray, "initialize", subTrayInit,       1);
  rb_define_method(tray, "update",     subTrayUpdate,     0);
  rb_define_method(tray, "pid",        SubtlextPidReader, 0);
  rb_define_method(tray, "to_str",     subTrayToString,   0);
  rb_define_method(tray, "kill",       subTrayKill,       0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(tray), "[]", "find");

  /* Aliases */
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

  /* Singleton methods */
  rb_define_singleton_method(view, "find",    subViewFind,    1);
  rb_define_singleton_method(view, "current", subViewCurrent, 0);
  rb_define_singleton_method(view, "all",     subViewAll,     0);

  /* General methods */
  rb_define_method(view, "tags",       SubtlextTagList,      0);
  rb_define_method(view, "has_tag?",   SubtlextTagAsk,       1);
  rb_define_method(view, "tag",        SubtlextTagAdd,       1);
  rb_define_method(view, "untag",      SubtlextTagDel,       1);

  /* Class methods */
  rb_define_method(view, "initialize", subViewInit,          1);
  rb_define_method(view, "update",     subViewUpdate,        0);
  rb_define_method(view, "clients",    subViewClients,       0);
  rb_define_method(view, "jump",       subViewJump,          0);
  rb_define_method(view, "current?",   subViewCurrentAsk,    0);
  rb_define_method(view, "to_str",     subViewToString,      0);
  rb_define_method(view, "kill",       subViewKill,          0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(view), "[]", "find");
  
  /* Aliases */
  rb_define_alias(view, "+",     "tag");
  rb_define_alias(view, "-",     "untag");
  rb_define_alias(view, "click", "jump");
  rb_define_alias(view, "save",  "update");
  rb_define_alias(view, "to_s",  "to_str");
 
  /*
   * Document-class: Subtlext::Window
   *
   * Class for interaction with windows
   */

  window = rb_define_class_under(mod, "Window", rb_cObject);

  /* Window id */
  rb_define_attr(window, "win", 1, 0);

  /* Singleton methods */
  rb_define_singleton_method(window, "new", subWindowNew, 1);

  /* General methods */
  rb_define_method(window, "click",        SubtlextClick, -1);
  rb_define_method(window, "focus",        SubtlextFocus,  0);

  /* Class methods */
  rb_define_method(window, "name",         subWindowNameWriter,        1);
  rb_define_method(window, "font",         subWindowFontWriter,        1);
  rb_define_method(window, "foreground",   subWindowForegroundWriter,  1);
  rb_define_method(window, "background",   subWindowBackgroundWriter,  1);
  rb_define_method(window, "border_color", subWindowBorderColorWriter, 1);
  rb_define_method(window, "border_size",  subWindowBorderSizeWriter,  1);
  rb_define_method(window, "text",         subWindowTextWriter,        1);
  rb_define_method(window, "input",        subWindowInput,             0);
  rb_define_method(window, "geometry",     subWindowGeometryReader,    0);
  rb_define_method(window, "show",         subWindowShow,              0);
  rb_define_method(window, "hide",         subWindowHide,              0);
  rb_define_method(window, "kill",         subWindowKill,              0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(window), "configure", "new");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
