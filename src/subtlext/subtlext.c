
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

/* SubtlextStringify {{{ */
static void
SubtlextStringify(char *string)
{
  assert(string);

  /* Lowercase and replace strange characters */
  while('\0' != *string)
    {
      *string = toupper(*string);

      if(!isalnum(*string)) *string = '_';

      string++;
    }
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

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Load on demand */
  if(NIL_P((pid = rb_iv_get(self, "@pid"))))
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

/* SubtlextTagFind {{{ */
static int
SubtlextTagFind(VALUE value)
{
  int tags = 0;

  /* Check object type */
  switch(rb_type(value))
    {
      case T_STRING:
          {
            int id;

            /* Find tag and get id */
            if(-1 != (id = subSubtlextFind("SUBTLE_TAG_LIST",
                RSTRING_PTR(value), NULL)))
              tags |= (1L << (id + 1));
          }
        break;
      case T_OBJECT:
        /* Check instance type and fetch id */
        if(rb_obj_is_instance_of(value, rb_const_get(mod, rb_intern("Tag"))))
          {
            VALUE id = Qnil;

            if(FIXNUM_P((id = rb_iv_get(value, "@id"))))
              tags |= (1L << (FIX2INT(id) + 1));
          }
        break;
      default: break;
    }

  return tags;
} /* }}} */

/* SubtlextTagGet {{{ */
static int
SubtlextTagGet(VALUE self)
{
  int tags = 0;
  unsigned long *value_tags = NULL;
  VALUE id = Qnil;

  /* Check type */
  if(rb_obj_is_instance_of(self, rb_const_get(mod, rb_intern("Client")))) ///< Client
    {
      VALUE win = Qnil;

      if(RTEST((win = rb_iv_get(self, "@win")) && 0 != NUM2LONG(win)))
        {
          if((value_tags = (unsigned long *)subSharedPropertyGet(
              display, NUM2LONG(win), XA_CARDINAL, XInternAtom(display,
              "SUBTLE_WINDOW_TAGS", False), NULL)))
            {
              tags = *value_tags;

              free(value_tags);
            }
        }
    }
  else if(FIXNUM_P(id = rb_iv_get(self, "@id"))) ///< View
    {
      if((value_tags = (unsigned long *)subSharedPropertyGet(display,
          DefaultRootWindow(display), XA_CARDINAL,
          XInternAtom(display, "SUBTLE_VIEW_TAGS", False), NULL)))
        {
          tags = value_tags[FIX2INT(id)];

          free(value_tags);
        }
    }

  return tags;
} /* }}} */

/* SubtlextTag {{{ */
static VALUE
SubtlextTag(VALUE self,
  VALUE value,
  int action)
{
  int tags = 0;
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Check object type */
  switch(rb_type(value))
    {
      case T_STRING:
      case T_OBJECT:
        tags |= SubtlextTagFind(value);
        break;
      case T_ARRAY:
          {
            int i;
            VALUE entry = Qnil;

            /* Collect tag ids */
            for(i = 0; Qnil != (entry = rb_ary_entry(value, i)); ++i)
              tags |= SubtlextTagFind(entry);
          }
        break;
      default:
        rb_raise(rb_eArgError, "Unexpected value-type `%s'",
          rb_obj_classname(value));
    }

  /* Get and update tags */
  if(0 != action)
    {
      int cur_tags = SubtlextTagGet(self);

      /* Update masks */
      if(1 == action)       tags = cur_tags | tags;
      else if(-1 == action) tags = cur_tags & ~tags;
    }

  /* Send message */
  data.l[0] = FIX2LONG(id);
  data.l[1] = tags;
  data.l[2] = rb_obj_is_instance_of(self,
    rb_const_get(mod, rb_intern("Client"))) ? 0 : 1; ///< Client = 0, View = 1

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_WINDOW_TAGS", data, 32, True);

  return Qnil;
} /* }}} */

/* SubtlextTagWriter {{{ */
/*
 * call-seq: tags=(value) -> nil
 *
 * Set all tags of a window
 *
 *  object.tags=([ #<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx> ])
 *  => nil
 */

static VALUE
SubtlextTagWriter(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, 0);
} /* }}} */

/* SubtlextTagReader {{{ */
/*
 * call-seq: tags -> Array
 *
 * Get list of tags for window
 *
 *  object.tags
 *  => [#<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx>]
 */

static VALUE
SubtlextTagReader(VALUE self)
{
  char **tags = NULL;
  int i, ntags = 0, value_tags = 0;
  VALUE method = Qnil, klass = Qnil, t = Qnil;
  VALUE array = rb_ary_new();

  /* Check ruby object */
  rb_check_frozen(self);

  /* Fetch data */
  method     = rb_intern("new");
  klass      = rb_const_get(mod, rb_intern("Tag"));
  value_tags = SubtlextTagGet(self);

  /* Check results */
  if((tags = subSharedPropertyStrings(display, DefaultRootWindow(display),
      XInternAtom(display, "SUBTLE_TAG_LIST", False), &ntags)))
    {
      for(i = 0; i < ntags; i++)
        {
          if(value_tags & (1L << (i + 1)))
            {
              /* Create new tag */
              t = rb_funcall(klass, method, 1, rb_str_new2(tags[i]));
              rb_iv_set(t, "@id", INT2FIX(i));
              rb_ary_push(array, t);
            }
        }

      XFreeStringList(tags);
    }

  return array;
} /* }}} */

/* SubtlextTagAdd {{{ */
/*
 * call-seq: tag(value) -> nil
 *           +(value)   -> nil
 *
 * Add an existing tag to window
 *
 *  object.tag("subtle")
 *  => nil
 *
 *  object.tag([ #<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx> ])
 *  => nil
 *
 *  object + "subtle"
 *  => nil
 */

static VALUE
SubtlextTagAdd(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, 1);
} /* }}} */

/* SubtlextTagDel {{{ */
/*
 * call-seq: untag(value) -> nil
 *           -(value)     -> nil
 *
 * Remove an existing tag from window
 *
 *  object.untag("subtle")
 *  => nil
 *
 *  object.untag([ #<Subtlext::Tag:xxx>, #<Subtlext::Tag:xxx> ])
 *  => nil
 *
 *  object - "subtle"
 *  => nil
 */

static VALUE
SubtlextTagDel(VALUE self,
  VALUE value)
{
  return SubtlextTag(self, value, -1);
} /* }}} */

/* SubtlextTagAsk {{{ */
VALUE
SubtlextTagAsk(VALUE self,
  VALUE value)
{
  VALUE id = Qnil, tag = Qnil, ret = Qfalse;

  /* Check ruby object */
  rb_check_frozen(self);

  /* Find tag */
  if(Qnil != (tag = subTagSingFind(Qnil, value)))
    {
      int tags = SubtlextTagGet(self);

      GET_ATTR(tag, "@id", id);

      if(tags & (1L << (FIX2INT(id) + 1))) ret = Qtrue;
    }

  return ret;
} /* }}} */

/* SubtlextTagReload {{{ */
VALUE
SubtlextTagReload(VALUE self)
{
  VALUE id = Qnil;
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@id", id);

  /* Send message */
  data.l[0] = FIX2INT(id);

  subSharedMessage(display, DefaultRootWindow(display),
    "SUBTLE_WINDOW_RETAG", data, 32, True);

  return Qnil;
} /* }}} */

/* SubtlextSendButton {{{ */
/*
 * call-seq: send_click(button, x, y) -> nil
 *
 * Emulate a click on a window
 *
 *  object.send_button(2)
 *  => nil
 */

static VALUE
SubtlextSendButton(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE button = Qnil, x = Qnil, y = Qnil, win = Qnil;

  rb_scan_args(argc, argv, "03", &button, &x, &y);

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Check object type */
  if(FIXNUM_P(button))
    {
      Window subwin = None;
      XEvent event = { 0 };

      /* Assemble button event */
      event.type                  = EnterNotify;
      event.xcrossing.window      = NUM2LONG(win);
      event.xcrossing.root        = DefaultRootWindow(display);
      event.xcrossing.subwindow   = NUM2LONG(win);
      event.xcrossing.same_screen = True;
      event.xcrossing.x           = FIXNUM_P(x) ? FIX2INT(x) : 5;
      event.xcrossing.y           = FIXNUM_P(y) ? FIX2INT(y) : 5;

      /* Translate window x/y to root x/y */
      XTranslateCoordinates(display, event.xcrossing.window,
        event.xcrossing.root, event.xcrossing.x, event.xcrossing.y,
        &event.xcrossing.x_root, &event.xcrossing.y_root, &subwin);

      //XSetInputFocus(display, event.xany.window, RevertToPointerRoot, CurrentTime);
      XSendEvent(display, NUM2LONG(win), True, EnterWindowMask, &event);

      /* Send button press event */
      event.type           = ButtonPress;
      event.xbutton.button = FIX2INT(button);

      XSendEvent(display, NUM2LONG(win), True, ButtonPressMask, &event);
      XFlush(display);

      usleep(12000);

      /* Send button release event */
      event.type = ButtonRelease;

      XSendEvent(display, NUM2LONG(win), True, ButtonReleaseMask, &event);
      XFlush(display);
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* SubtlextSendKey {{{ */
/*
 * call-seq: send_key(key, x, y) -> nil
 *
 * Emulate a keypress on a window
 *
 *  object.send_key("d")
 *  => nil
 */

static VALUE
SubtlextSendKey(int argc,
  VALUE *argv,
  VALUE self)
{
  VALUE key = Qnil, x = Qnil, y = Qnil, win = Qnil;

  rb_scan_args(argc, argv, "03", &key, &x, &y);

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Check object type */
  if(T_STRING == rb_type(key))
    {
      int mouse = False;
      unsigned int code = 0, state = 0;
      Window subwin = None;
      KeySym sym = None;
      XEvent event = { 0 };

      /* Parse keys */
      if(NoSymbol == (sym = subSharedParseKey(display, RSTRING_PTR(key),
          &code, &state, &mouse)))
        {
          rb_raise(rb_eStandardError, "Unknown key");

          return Qnil;
        }

      /* Check mouse */
      if(True == mouse)
        {
          rb_raise(rb_eNotImpError,
            "Please use #send_button / #click for button events");

          return Qnil;
        }

      /* Assemble button event */
      event.type                  = EnterNotify;
      event.xcrossing.window      = NUM2LONG(win);
      event.xcrossing.root        = DefaultRootWindow(display);
      event.xcrossing.subwindow   = NUM2LONG(win);
      event.xcrossing.same_screen = True;
      event.xcrossing.x           = FIXNUM_P(x) ? FIX2INT(x) : 5;
      event.xcrossing.y           = FIXNUM_P(y) ? FIX2INT(y) : 5;

      /* Translate window x/y to root x/y */
      XTranslateCoordinates(display, event.xcrossing.window,
        event.xcrossing.root, event.xcrossing.x, event.xcrossing.y,
        &event.xcrossing.x_root, &event.xcrossing.y_root, &subwin);

      //XSetInputFocus(display, event.xany.window, RevertToPointerRoot, CurrentTime);
      XSendEvent(display, NUM2LONG(win), True, EnterWindowMask, &event);

      /* Send button press event */
      event.type         = KeyPress;
      event.xkey.state   = state;
      event.xkey.keycode = code;

      XSendEvent(display, NUM2LONG(win), True, KeyPressMask, &event);
      XFlush(display);

      usleep(12000);

      /* Send button release event */
      event.type = KeyRelease;

      XSendEvent(display, NUM2LONG(win), True, KeyReleaseMask, &event);
      XFlush(display);
    }
  else rb_raise(rb_eArgError, "Unexpected value-type `%s'",
    rb_obj_classname(key));

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
  SubMessageData data = { { 0, 0, 0, 0, 0 } };

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Send message */
  data.l[0] = NUM2LONG(win);

  subSharedMessage(display, DefaultRootWindow(display),
    "_NET_ACTIVE_WINDOW", data, 32, True);

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

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Fetch data */
  if((focus = (unsigned long *)subSharedPropertyGet(display, DefaultRootWindow(display),
      XA_WINDOW, XInternAtom(display, "_NET_ACTIVE_WINDOW", False), NULL)))
    {
      if(*focus == NUM2LONG(win)) ret = Qtrue;

      free(focus);
    }

  return ret;
} /* }}} */

/* SubtlextPropReader {{{ */
/*
 * call-seq: [value] -> String or Nil
 *
 * Get arbitrary persistent property
 *
 *  object["wm"]
 *  => "subtle"
 *
 *  object[:wm]
 *  => "subtle"
 */

VALUE
SubtlextPropReader(VALUE self,
  VALUE key)
{
  char *prop = NULL;
  VALUE ret = Qnil, win = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Check object type */
  switch(rb_type(key))
    {
      case T_STRING: prop = RSTRING_PTR(key);      break;
      case T_SYMBOL: prop = (char *)SYM2CHAR(key); break;
      default:
        rb_raise(rb_eArgError, "Unexpected key value type `%s'",
          rb_obj_classname(key));

        return Qnil;
    }

  /* Check results */
  if(prop)
    {
      char propname[255] = { 0 }, *name = NULL, *result = NULL;

      /* Sanitize property name */
      name = strdup(prop);
      SubtlextStringify(name);

      snprintf(propname, sizeof(propname), "SUBTLE_PROPERTY_%s", name);

      /* Get actual property */
      if((result = subSharedPropertyGet(display, NUM2LONG(win),
          XInternAtom(display, "UTF8_STRING", False),
          XInternAtom(display, propname, False), NULL)))
        {
          ret = rb_str_new2(result);

          free(result);
        }

      free(name);
    }

  return ret;
} /* }}} */

/* SubtlextPropWriter {{{ */
/*
 * call-seq: [key]= value -> Nil
 *
 * Set arbitrary persistent property
 *
 *  object["wm"] = "subtle"
 *  => nil
 */

VALUE
SubtlextPropWriter(VALUE self,
  VALUE key,
  VALUE value)
{
  char *prop = NULL;
  VALUE win = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self, "@win", win);

  /* Check object type */
  switch(rb_type(key))
    {
      case T_STRING: prop = RSTRING_PTR(key);      break;
      case T_SYMBOL: prop = (char *)SYM2CHAR(key); break;
      default:
        rb_raise(rb_eArgError, "Unexpected key value-type `%s'",
          rb_obj_classname(key));

        return Qnil;
    }

  /* Check object type */
  if(T_STRING == rb_type(value))
    {
      char propname[255] = { 0 }, *name = NULL;

      /* Sanitize property name */
      name = strdup(prop);
      SubtlextStringify(name);

      snprintf(propname, sizeof(propname), "SUBTLE_PROPERTY_%s", name);

      /* Update property */
      XChangeProperty(display, NUM2LONG(win),
        XInternAtom(display, propname, False),
        XInternAtom(display, "UTF8_STRING", False), 8, PropModeReplace,
        (unsigned char *)RSTRING_PTR(value), RSTRING_LEN(value));

      XSync(display, False); ///< Sync all changes

      free(name);
    }
  else rb_raise(rb_eArgError, "Unexpected value value-type `%s'",
    rb_obj_classname(value));

  return Qnil;
} /* }}} */

/* SubtlextEqualId {{{ */
/*
 * call-seq: ==(other) -> True or False
 *
 * Whether both objects are equal (based on id)
 *
 *  object1 == object2
 *  => true
 */

static VALUE
SubtlextEqualId(VALUE self,
  VALUE other)
{
  VALUE id1 = Qnil, id2 = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self,  "@id", id1);
  GET_ATTR(other, "@id", id2);

  return id1 == id2 ? Qtrue : Qfalse;
} /* }}} */

/* SubtlextEqualWindow {{{ */
/*
 * call-seq: ==(other) -> True or False
 *
 * Whether both objects are equal (based on win)
 *
 *  object1 == object2
 *  => true
 */

static VALUE
SubtlextEqualWindow(VALUE self,
  VALUE other)
{
  VALUE win1 = Qnil, win2 = Qnil;

  /* Check ruby object */
  rb_check_frozen(self);
  GET_ATTR(self,  "@win", win1);
  GET_ATTR(other, "@win", win2);

  return win1 == win2 ? Qtrue : Qfalse;
} /* }}} */

/* Exported */

/* subSubtlextConnect {{{ */
void
subSubtlextConnect(char *display_string)
{
  /* Open display */
  if(!display)
    {
      if(!(display = XOpenDisplay(display_string)))
        {
          rb_raise(rb_eStandardError, "Failed opening display `%s'",
            display_string);
        }

      XSetErrorHandler(subSharedLogXError);

      if(!setlocale(LC_CTYPE, "")) XSupportsLocale();

      /* Check if subtle is running */
      if(Qtrue != subSubtleSingRunningAsk(Qnil))
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

/* subSubtlextBacktrace {{{ */
void
subSubtlextBacktrace(void)
{
  VALUE lasterr = Qnil;

  /* Get last error */
  if(!NIL_P(lasterr = rb_gv_get("$!")))
    {
      int i;
      VALUE message = Qnil, klass = Qnil, backtrace = Qnil, entry = Qnil;

      /* Fetching backtrace data */
      message   = rb_obj_as_string(lasterr);
      klass     = rb_class_path(CLASS_OF(lasterr));
      backtrace = rb_funcall(lasterr, rb_intern("backtrace"), 0, NULL);

      /* Print error and backtrace */
      subSharedLogWarn("%s: %s\n", RSTRING_PTR(klass), RSTRING_PTR(message));
      for(i = 0; Qnil != (entry = rb_ary_entry(backtrace, i)); ++i)
        printf("\tfrom %s\n", RSTRING_PTR(entry));
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

/* subSubtlextParse {{{ */
VALUE
subSubtlextParse(VALUE value,
  char *buf,
  int len,
  int *flags)
{
  VALUE ret = Qtrue;

  /* Set defaults */
  if(flags) *flags = (SUB_MATCH_INSTANCE|SUB_MATCH_CLASS);

  /* Check for hash */
  if(T_HASH == rb_type(value) && flags) /* {{{ */
    {
      int i;
      VALUE match = Qnil;

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
          if(!NIL_P((match = rb_hash_lookup(value, props[i].sym))))
            {
              *flags = props[i].flags;
              break;
            }
        }

      value = match; ///< Just copy over
    } /* }}} */

  /* Check object type */
  switch(rb_type(value))
    {
      case T_FIXNUM: /* {{{ */
        snprintf(buf, len, "%d", (int)FIX2INT(value));
        break; /* }}} */
      case T_STRING: /* {{{ */
        snprintf(buf, len, "%s", RSTRING_PTR(value));
        break; /* }}} */
      case T_SYMBOL: /* {{{ */
        ret = value;
        snprintf(buf, len, "%s", SYM2CHAR(value));
        break; /* }}} */
      default: /* {{{ */
        rb_raise(rb_eArgError, "Unexpected value-type `%s'",
          rb_obj_classname(value));

        ret = Qfalse; /* }}} */
    }

  return ret;
} /* }}} */

/* subSubtlextList {{{ */

Window *
subSubtlextList(char *prop,
  int *size)
{
  Window *wins = NULL;
  unsigned long len = 0;

  assert(prop && size);

  if((wins = (Window *)subSharedPropertyGet(display, DefaultRootWindow(display),
      XA_WINDOW, XInternAtom(display, prop, False), &len)))
    {
      if(size) *size = len;
    }
  else
    {
      if(size) *size = 0;

      subSharedLogDebug("Failed getting window list\n");
    }

  return wins;
} /* }}} */

/* subSubtlextFind {{{ */
int
subSubtlextFind(char *prop,
  char *match,
  char **name)
{
  int ret = -1, size = 0;
  char **strings = NULL;
  regex_t *preg = NULL;

  assert(prop && match);

  /* Find id */
  if((preg = subSharedRegexNew(match)) &&
      (strings = subSharedPropertyStrings(display, DefaultRootWindow(display),
      XInternAtom(display, prop, False), &size)))
    {
      int i;

      for(i = 0; i < size; i++)
        {
          if((isdigit(match[0]) && atoi(match) == i) ||
              (!isdigit(match[0]) && subSharedRegexMatch(preg, strings[i])))
            {
              subSharedLogDebug("Found: prop=%s, match=%s, name=%s, id=%d\n",
                prop, match, strings[i], i);

              if(name) *name = strdup(strings[i]);

              ret = i;
              break;
            }
        }
    }
  else subSharedLogDebug("Failed finding string `%s'\n", match);

  if(preg)    subSharedRegexKill(preg);
  if(strings) XFreeStringList(strings);

  return ret;
} /* }}} */

/* subSubtlextFindWindow {{{ */
int
subSubtlextFindWindow(char *prop,
  char *match,
  char **name,
  Window *win,
  int flags)
{
  int id = -1, size = 0;
  Window *wins = NULL;

  assert(prop && match);

  /* Find window id */
  if((wins = subSubtlextList(prop, &size)))
    {
      int i, gravity1 = 0, gravity2 = -1, ngravities = 0;
      int pid1 = 0, pid2 = -1; ///<  Init differently
      long digit = -1;
      char *wmname = NULL, *instance = NULL, *klass = NULL;
      char *role = NULL, **gravities = NULL;
      Window selwin = None;
      regex_t *preg = subSharedRegexNew(match);

      if(isdigit(match[0])) digit = strtol(match, NULL, 0);

      /* Select window */
      if(0 == strcmp(match, "#") && win)
        selwin = subSubtleSingSelect(Qnil);

      /* Find id of gravity */
      if(flags & SUB_MATCH_GRAVITY)
        {
          gravities = subSharedPropertyStrings(display,
            DefaultRootWindow(display),
            XInternAtom(display, "SUBTLE_GRAVITY_LIST", False), &ngravities);

          gravity1 = subGravityFindId(match, NULL, NULL);
        }

      /* Get pid */
      if(flags & SUB_MATCH_PID)
        pid1 = atoi(match);

      for(i = 0; -1 == id && i < size; i++)
        {
          XFetchName(display, wins[i], &wmname);
          subSharedPropertyClass(display, wins[i], &instance, &klass);

          /* Get window gravity */
          if(flags & SUB_MATCH_GRAVITY)
            {
              int *gravity = NULL;

              if((gravity = (int *)subSharedPropertyGet(display, wins[i],
                  XA_CARDINAL, XInternAtom(display, "SUBTLE_WINDOW_GRAVITY",
                  False), NULL)))
                {
                  gravity2 = *gravity;

                  subSharedLogDebug("Gravity: match=%s, gravity1=%d, gravity2=%d\n",
                    match, gravity1, gravity2);

                  free(gravity);
                }
            }

          /* Get window role */
          if(flags & SUB_MATCH_ROLE)
            role = subSharedPropertyGet(display, wins[i], XA_STRING,
              XInternAtom(display, "WM_WINDOW_ROLE", False), NULL);

          /* Get window pid */
          if(flags & SUB_MATCH_PID)
            {
              int *pid = NULL;

              if((pid = (int *)subSharedPropertyGet(display, wins[i], XA_CARDINAL,
                  XInternAtom(display, "_NET_WM_PID", False), NULL)))
                {
                  pid2 = *pid;

                  free(pid);
                }
            }

          /* Find window either by window id, title, inst, class, gravity or pid */
          if(selwin == wins[i] || digit == wins[i] || digit == (long)i ||
              /* Compare WM_NAME */
              (flags & SUB_MATCH_NAME && wmname
                && subSharedRegexMatch(preg, wmname)) ||

              /* Compare instance part of WM_CLASS */
              (flags & SUB_MATCH_INSTANCE && instance
                && subSharedRegexMatch(preg, instance)) ||

              /* Compare class part of WM_CLASS */
              (flags & SUB_MATCH_CLASS && klass
                && subSharedRegexMatch(preg, klass)) ||

              /* Compare instance part of WM_CLASS */
              (flags & SUB_MATCH_ROLE && role
                && subSharedRegexMatch(preg, role)) ||

              /* Compare gravities */
              (flags & SUB_MATCH_GRAVITY && (gravity1 == gravity2 ||
                (0 <= gravity1 && gravity1 < ngravities &&
                0 <= gravity2 && gravity2 < ngravities &&
                !strcmp(gravities[gravity1], gravities[gravity2])))) ||

              /* Compare pids */
              (flags & SUB_MATCH_PID && pid1 == pid2))
            {
              subSharedLogDebug("Found: prop=%s, name=%s, win=%#lx, id=%d, flags\n",
                prop, match, wins[i], i, flags);

              if(win) *win = wins[i];
              if(name)
                {
                  *name = (char *)subSharedMemoryAlloc(strlen(instance) + 1, sizeof(char));
                  strncpy(*name, instance, strlen(instance));
                 }

              id = i;
            }

          if(role) free(role);
          free(wmname);
          free(instance);
          free(klass);
        }

      if(gravities) XFreeStringList(gravities);
      subSharedRegexKill(preg);
      free(wins);
    }
  else subSharedLogDebug("Failed finding window `%s'\n", match);

  return id;
} /* }}} */

 /** subSubtlextWMCheck {{{
  * @brief Get WM check window
  * @return A #Window
  **/

Window *
subSubtlextWMCheck(void)
{
  return (Window *)subSharedPropertyGet(display, DefaultRootWindow(display),
    XA_WINDOW, XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False), NULL);
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

  /* Window visibility */
  rb_define_attr(client, "hidden",   1, 0);

  /* Singleton methods */
  rb_define_singleton_method(client, "find",    subClientSingFind,    1);
  rb_define_singleton_method(client, "current", subClientSingCurrent, 0);
  rb_define_singleton_method(client, "visible", subClientSingVisible, 0);
  rb_define_singleton_method(client, "all",     subClientSingAll,     0);

  /* General methods */
  rb_define_method(client, "has_tag?",    SubtlextTagAsk,      1);
  rb_define_method(client, "tags",        SubtlextTagReader,   0);
  rb_define_method(client, "tags=",       SubtlextTagWriter,   1);
  rb_define_method(client, "tag",         SubtlextTagAdd,      1);
  rb_define_method(client, "untag",       SubtlextTagDel,      1);
  rb_define_method(client, "retag",       SubtlextTagReload,   0);
  rb_define_method(client, "send_button", SubtlextSendButton, -1);
  rb_define_method(client, "send_key",    SubtlextSendKey,    -1);
  rb_define_method(client, "focus",       SubtlextFocus,       0);
  rb_define_method(client, "has_focus?",  SubtlextFocusAsk,    0);
  rb_define_method(client, "[]",          SubtlextPropReader,  1);
  rb_define_method(client, "[]=",         SubtlextPropWriter,  2);
  rb_define_method(client, "==",          SubtlextEqualWindow, 1);

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
  rb_define_method(client, "up",           subClientSelectUp,        0);
  rb_define_method(client, "left",         subClientSelectLeft,      0);
  rb_define_method(client, "right",        subClientSelectRight,     0);
  rb_define_method(client, "down",         subClientSelectDown,      0);
  rb_define_method(client, "to_str",       subClientToString,        0);
  rb_define_method(client, "gravity",      subClientGravityReader,   0);
  rb_define_method(client, "gravity=",     subClientGravityWriter,   1);
  rb_define_method(client, "geometry",     subClientGeometryReader,  0);
  rb_define_method(client, "geometry=",    subClientGeometryWriter, -1);
  rb_define_method(client, "show",         subClientShow,            0);
  rb_define_method(client, "hide",         subClientHide,            0);
  rb_define_method(client, "hidden?",      subClientHiddenAsk,       0);
  rb_define_method(client, "resize=",      subClientResizeWriter,    1);
  rb_define_method(client, "pid",          SubtlextPidReader,        0);
  rb_define_method(client, "alive?",       subClientAliveAsk,        0);
  rb_define_method(client, "kill",         subClientKill,            0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(client), "[]", "find");

  /* Aliases */
  rb_define_alias(client, "+",     "tag");
  rb_define_alias(client, "-",     "untag");
  rb_define_alias(client, "save",  "update");
  rb_define_alias(client, "to_s",  "to_str");
  rb_define_alias(client, "click", "send_button");

  /*
   * Document-class: Subtlext::Color
   *
   * Color class for interaction with colors
   */

  color = rb_define_class_under(mod, "Color", rb_cObject);

  /* Red fraction */
  rb_define_attr(color, "red", 1, 0);

  /* Green fraction */
  rb_define_attr(color, "green", 1, 0);

  /* Blue fraction */
  rb_define_attr(color, "blue", 1, 0);

  /* Pixel number */
  rb_define_attr(color, "pixel", 1, 0);

  /* Class methods */
  rb_define_method(color, "initialize", subColorInit,         1);
  rb_define_method(color, "to_hex",     subColorToHex,        0);
  rb_define_method(color, "to_str",     subColorToString,     0);
  rb_define_method(color, "+",          subColorOperatorPlus, 1);
  rb_define_method(color, "==",         subColorEqual,        1);

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
  rb_define_method(geometry, "==",         subGeometryEqual,     1);

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
  rb_define_singleton_method(gravity, "find", subGravitySingFind, 1);
  rb_define_singleton_method(gravity, "all",  subGravitySingAll,  0);

  /* General methods */
  rb_define_method(gravity, "==", SubtlextEqualId, 1);

  /* Class methods */
  rb_define_method(gravity, "initialize",   subGravityInit,           -1);
  rb_define_method(gravity, "update",       subGravityUpdate,          0);
  rb_define_method(gravity, "clients",      subGravityClients,         0);
  rb_define_method(gravity, "geometry",     subGravityGeometryReader,  0);
  rb_define_method(gravity, "geometry=",    subGravityGeometryWriter,  1);
  rb_define_method(gravity, "geometry_for", subGravityGeometryFor,     1);
  rb_define_method(gravity, "to_str",       subGravityToString,        0);
  rb_define_method(gravity, "to_sym",       subGravityToSym,           0);
  rb_define_method(gravity, "kill",         subGravityKill,            0);

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
  rb_define_attr(icon, "width",  1, 0);

  /* Icon height */
  rb_define_attr(icon, "height", 1, 0);

  /* Allocate */
  rb_define_alloc_func(icon, subIconAlloc);

  /* Class methods */
  rb_define_method(icon, "initialize", subIconInit,         -1);
  rb_define_method(icon, "draw",       subIconDraw,          2);
  rb_define_method(icon, "draw_rect",  subIconDrawRect,     -1);
  rb_define_method(icon, "clear",      subIconClear,         0);
  rb_define_method(icon, "to_str",     subIconToString,      0);
  rb_define_method(icon, "+",          subIconOperatorPlus,  1);
  rb_define_method(icon, "*",          subIconOperatorMult,  1);

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
  rb_define_singleton_method(screen, "find",    subScreenSingFind,    1);
  rb_define_singleton_method(screen, "current", subScreenSingCurrent, 0);
  rb_define_singleton_method(screen, "all",     subScreenSingAll,     0);

  /* General methods */
  rb_define_method(screen, "==", SubtlextEqualId, 1);

  /* Class methods */
  rb_define_method(screen, "initialize", subScreenInit,       1);
  rb_define_method(screen, "update",     subScreenUpdate,     0);
  rb_define_method(screen, "view",       subScreenViewReader, 0);
  rb_define_method(screen, "view=",      subScreenViewWriter, 1);
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
  rb_define_singleton_method(subtle, "display",       subSubtleSingDisplayReader, 0);
  rb_define_singleton_method(subtle, "display=",      subSubtleSingDisplayWriter, 1);
  rb_define_singleton_method(subtle, "select_window", subSubtleSingSelect,        0);
  rb_define_singleton_method(subtle, "running?",      subSubtleSingRunningAsk,    0);
  rb_define_singleton_method(subtle, "reload",        subSubtleSingReload,        0);
  rb_define_singleton_method(subtle, "restart",       subSubtleSingRestart,       0);
  rb_define_singleton_method(subtle, "quit",          subSubtleSingQuit,          0);
  rb_define_singleton_method(subtle, "colors",        subSubtleSingColors,        0);
  rb_define_singleton_method(subtle, "font",          subSubtleSingFont,          0);
  rb_define_singleton_method(subtle, "spawn",         subSubtleSingSpawn,         1);

  /* Aliases */
  rb_define_alias(rb_singleton_class(subtle), "reload_config", "reload");

  /*
   * Document-class: Subtlext::Sublet
   *
   * Class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);

  /* Sublet id */
  rb_define_attr(screen, "id",   1, 0);

  /* Name of the sublet */
  rb_define_attr(sublet, "name", 1, 0);

  /* Singleton methods */
  rb_define_singleton_method(sublet, "find", subSubletSingFind, 1);
  rb_define_singleton_method(sublet, "all",  subSubletSingAll,  0);

  /* General methods */
  rb_define_method(sublet, "==", SubtlextEqualId, 1);

  /* Class methods */
  rb_define_method(sublet, "initialize",  subSubletInit,             1);
  rb_define_method(sublet, "update",      subSubletUpdate,           0);
  rb_define_method(sublet, "data",        subSubletDataReader,       0);
  rb_define_method(sublet, "data=",       subSubletDataWriter,       1);
  rb_define_method(sublet, "geometry",    subSubletGeometryReader,   0);
  rb_define_method(sublet, "foreground=", subSubletForegroundWriter, 1);
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
  rb_define_singleton_method(tag, "find",    subTagSingFind,    1);
  rb_define_singleton_method(tag, "visible", subTagSingVisible, 0);
  rb_define_singleton_method(tag, "all",     subTagSingAll,     0);

  /* General methods */
  rb_define_method(tag, "==", SubtlextEqualId, 1);

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
  rb_define_singleton_method(tray, "find", subTraySingFind, 1);
  rb_define_singleton_method(tray, "all",  subTraySingAll,  0);

  /* General methods */
  rb_define_method(tray, "send_button", SubtlextSendButton, -1);
  rb_define_method(tray, "send_key",    SubtlextSendKey,    -1);
  rb_define_method(tray, "focus",       SubtlextFocus,       0);
  rb_define_method(tray, "has_focus?",  SubtlextFocusAsk,    0);
  rb_define_method(tray, "[]",          SubtlextPropReader,  1);
  rb_define_method(tray, "[]=",         SubtlextPropWriter,  2);
  rb_define_method(tray, "==",          SubtlextEqualWindow, 1);

  /* Class methods */
  rb_define_method(tray, "initialize", subTrayInit,       1);
  rb_define_method(tray, "update",     subTrayUpdate,     0);
  rb_define_method(tray, "pid",        SubtlextPidReader, 0);
  rb_define_method(tray, "to_str",     subTrayToString,   0);
  rb_define_method(tray, "kill",       subTrayKill,       0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(tray), "[]", "find");

  /* Aliases */
  rb_define_alias(tray, "to_s",  "to_str");
  rb_define_alias(tray, "click", "send_button");

  /*
   * Document-class: Subtlext::View
   *
   * Class for interaction with views
   */

  view = rb_define_class_under(mod, "View", rb_cObject);

  /* View id */
  rb_define_attr(view, "id",   1, 0);

  /* Name of the view */
  rb_define_attr(view, "name", 1, 0);

  /* Singleton methods */
  rb_define_singleton_method(view, "find",    subViewSingFind,    1);
  rb_define_singleton_method(view, "current", subViewSingCurrent, 0);
  rb_define_singleton_method(view, "visible", subViewSingVisible, 0);
  rb_define_singleton_method(view, "all",     subViewSingAll,     0);

  /* General methods */
  rb_define_method(view, "has_tag?", SubtlextTagAsk,      1);
  rb_define_method(view, "tags",     SubtlextTagReader,   0);
  rb_define_method(view, "tags=",    SubtlextTagWriter,   1);
  rb_define_method(view, "tag",      SubtlextTagAdd,      1);
  rb_define_method(view, "untag",    SubtlextTagDel,      1);
  rb_define_method(view, "[]",       SubtlextPropReader,  1);
  rb_define_method(view, "[]=",      SubtlextPropWriter,  2);
  rb_define_method(view, "==",       SubtlextEqualId,     1);

  /* Class methods */
  rb_define_method(view, "initialize", subViewInit,          1);
  rb_define_method(view, "update",     subViewUpdate,        0);
  rb_define_method(view, "clients",    subViewClients,       0);
  rb_define_method(view, "jump",       subViewJump,          0);
  rb_define_method(view, "next",       subViewSelectNext,    0);
  rb_define_method(view, "prev",       subViewSelectPrev,    0);
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
  rb_define_attr(window, "win",    1, 0);

  /* Window visibility */
  rb_define_attr(window, "hidden", 1, 0);

  /* Allocate */
  rb_define_alloc_func(window, subWindowAlloc);

  /* Singleton methods */
  rb_define_singleton_method(window, "once", subWindowSingOnce, 1);

  /* General methods */
  rb_define_method(window, "send_button", SubtlextSendButton, -1);
  rb_define_method(window, "send_key",    SubtlextSendKey,    -1);
  rb_define_method(window, "focus",       SubtlextFocus,       0);
  rb_define_method(window, "[]",          SubtlextPropReader,  1);
  rb_define_method(window, "[]=",         SubtlextPropWriter,  2);
  rb_define_method(window, "==",          SubtlextEqualWindow, 1);

  /* Class methods */
  rb_define_method(window, "initialize",    subWindowInit,              1);
  rb_define_method(window, "name=",         subWindowNameWriter,        1);
  rb_define_method(window, "font=",         subWindowFontWriter,        1);
  rb_define_method(window, "foreground=",   subWindowForegroundWriter,  1);
  rb_define_method(window, "background=",   subWindowBackgroundWriter,  1);
  rb_define_method(window, "border_color=", subWindowBorderColorWriter, 1);
  rb_define_method(window, "border_size=",  subWindowBorderSizeWriter,  1);
  rb_define_method(window, "write",         subWindowWrite,             3);
  rb_define_method(window, "read",          subWindowRead,             -1);
  rb_define_method(window, "clear",         subWindowClear,            -1);
  rb_define_method(window, "redraw",        subWindowRedraw,            0);
  rb_define_method(window, "completion",    subWindowCompletion,        0);
  rb_define_method(window, "input",         subWindowInput,             0);
  rb_define_method(window, "geometry",      subWindowGeometryReader,    0);
  rb_define_method(window, "geometry=",     subWindowGeometryWriter,    1);
  rb_define_method(window, "show",          subWindowShow,              0);
  rb_define_method(window, "hide",          subWindowHide,              0);
  rb_define_method(window, "hidden?",       subWindowHiddenAsk,         0);
  rb_define_method(window, "kill",          subWindowKill,              0);

  /* Singleton aliases */
  rb_define_alias(rb_singleton_class(window), "configure", "new");

  /* Aliases */
  rb_define_alias(window, "click", "send_button");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
