
 /**
  * @package subtle
  *
  * @file Ruby functions
  * @copyright (c) 2005-2009 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <ruby.h>
#include "subtle.h"

#define CHAR2SYM(name) ID2SYM(rb_intern(name))
#define SYM2CHAR(sym)  rb_id2name(SYM2ID(sym))

static VALUE shelter = Qnil, inherited = Qnil, subtlext = Qnil; ///< Globals

/* Typedef {{{ */
typedef struct rubygrabs_t
{
  VALUE         sym;
  int           value;
  unsigned long extra;
} RubyGrabs;

typedef struct rubyhooks_t
{
  VALUE         sym;
  unsigned long *hook;
} RubyHooks;

typedef struct rubypanels_t
{
  VALUE    sym;
  SubPanel *panel;
} RubyPanels;
/* }}} */

/* RubyPerror {{{ */
static VALUE
RubyPerror(int verbose, 
  int fatal,
  const char *format,
  ...)
{
  va_list ap;
  char buf[255];
  VALUE lineno = Qnil, lasterr = Qnil, message = Qnil;

  /* Fetch infos */
  lineno  = rb_gv_get("$.");
  lasterr = rb_gv_get("$!");
  message = rb_obj_as_string(lasterr);

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if(True == verbose && RTEST(message)) subSharedLogWarn("%s\n\n", RSTRING_PTR(message));
  if(True == fatal) subSharedLogError("%s at line %d\n", buf, FIX2INT(lineno));
  subSharedLogWarn("%s at line %d\n", buf, FIX2INT(lineno));

  return Qnil;
} /* }}} */

/* RubyFilter {{{ */
static inline int
RubyFilter(const struct dirent *entry)
{
  return !fnmatch("*.rb", entry->d_name, FNM_PATHNAME);
} /* }}} */

/* RubyDispatcher {{{ */
/*
 * Dispatcher for Subtlext commands - internal use only
 */

static VALUE  
RubyDispatcher(int argc, 
  VALUE *argv, 
  VALUE self)
{  
  char *name = NULL;
  VALUE missing = Qnil, args = Qnil;     

  rb_scan_args(argc, argv, "1*", &missing, &args);  
  name = (char *)rb_id2name(SYM2ID(missing));  

  subSharedLogDebug("Missing: method=%s\n", name);

  if(Qnil == subtlext) subRubyLoadSubtlext(); ///< Load subtlext on demand

  if(rb_respond_to(subtlext, rb_to_id(missing))) ///< Dispatch method calls
    return rb_funcall2(subtlext, rb_to_id(missing), --argc, ++argv);
  
  rb_raise(rb_eStandardError, "Failed finding method `%s'", name);
  return Qnil;  
} /* }}} */

/* RubySignal {{{ */
static void
RubySignal(int signum)
{
  if(SIGALRM == signum) ///< Catch SIGALRM
    {
      rb_raise(rb_eInterrupt, "Execution time (%ds) expired", EXECTIME);
    }
} /* }}} */

/* RubyConcat {{{ */
static VALUE
RubyConcat(VALUE str1,
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

/* RubyGetValue {{{ */
static VALUE
RubyGetValue(VALUE hash,
  char *key,
  int type,
  int optional)
{
  VALUE value = Qundef, sym = CHAR2SYM(key);

  assert(key);

  /* Check for key */
  if(RTEST(hash) && Qtrue == rb_funcall(hash, rb_intern("has_key?"), 1, sym))
    value = rb_funcall(hash, rb_intern("fetch"), 1, sym);
  else if(!optional) subSharedLogWarn("Failed reading key `%s'\n", key);

  return rb_type(value) == type ? value : Qnil;
} /* }}} */

/* RubyGetString {{{ */
static char *
RubyGetString(VALUE hash,
  char *key,
  char *fallback)
{
  VALUE value = RubyGetValue(hash, key, T_STRING, NULL == fallback);
  
  return Qnil != value ? RSTRING_PTR(value) : fallback;
} /* }}} */

/* RubyGetFixnum {{{ */
static int
RubyGetFixnum(VALUE hash,
  char *key,
  int fallback)
{
  VALUE value = RubyGetValue(hash, key, T_FIXNUM, 0 == fallback);
  
  return Qnil != value ? FIX2INT(value) : fallback;
} /* }}} */

/* RubyGetBool {{{ */
static int
RubyGetBool(VALUE hash,
  char *key)
{
  VALUE value = Qnil, sym = CHAR2SYM(key);

  /* Do it manually for trinary state */
  if(RTEST(hash) && Qtrue == rb_funcall(hash, rb_intern("has_key?"), 1, sym))
    value = rb_funcall(hash, rb_intern("fetch"), 1, sym);
  
  return Qtrue == value ? True : (Qfalse == value ? False : -1);
} /* }}} */

/* RubyGetRect {{{ */
static int
RubyGetRect(VALUE hash,
  char *key,
  XRectangle *r)
{
  VALUE ary = Qnil;
  int data[4] = { 0 }, ret = False;

  if(Qnil != (ary = RubyGetValue(hash, key, T_ARRAY, True)))
    {
      int i;
      VALUE value = Qnil, meth = rb_intern("at");

      for(i = 0; i < 4; i++) ///< Safely fetching array values
        {
          value   = rb_funcall(ary, meth, 1, INT2FIX(i)) ;
          data[i] = RTEST(value) ? FIX2INT(value) : 0;
        }
      
      ret = True;
    }

  /* Assign data to rect */
  r->x      = data[0]; ///< Left
  r->y      = data[1]; ///< Right
  r->width  = data[2]; ///< Top
  r->height = data[3]; ///< Bottom

  return ret;
} /* }}} */

/* RubyParseForeach {{{ */
static int
RubyParseForeach(VALUE key,
  VALUE value,
  VALUE extra)
{
  int i, type = -1;
  void *entry = NULL;
  SubData data = { None };

  RubyGrabs grabs[] = /* {{{ */
  {
    { CHAR2SYM("SubtleReload"),       SUB_GRAB_SUBTLE_RELOAD,  None                     },
    { CHAR2SYM("SubtleQuit"),         SUB_GRAB_SUBTLE_QUIT,    None                     }, 
    { CHAR2SYM("WindowMove"),         SUB_GRAB_WINDOW_MOVE,    None                     },
    { CHAR2SYM("WindowResize"),       SUB_GRAB_WINDOW_RESIZE,  None                     }, 
    { CHAR2SYM("WindowFloat"),        SUB_GRAB_WINDOW_TOGGLE,  SUB_MODE_FLOAT          }, 
    { CHAR2SYM("WindowFull"),         SUB_GRAB_WINDOW_TOGGLE,  SUB_MODE_FULL           }, 
    { CHAR2SYM("WindowStick"),        SUB_GRAB_WINDOW_TOGGLE,  SUB_MODE_STICK          }, 
    { CHAR2SYM("WindowRaise"),        SUB_GRAB_WINDOW_STACK,   Above                    }, 
    { CHAR2SYM("WindowLower"),        SUB_GRAB_WINDOW_STACK,   Below                    }, 
    { CHAR2SYM("WindowLeft"),         SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_LEFT          }, 
    { CHAR2SYM("WindowDown"),         SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_DOWN          }, 
    { CHAR2SYM("WindowUp"),           SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_UP            }, 
    { CHAR2SYM("WindowRight"),        SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_RIGHT         }, 
    { CHAR2SYM("WindowKill"),         SUB_GRAB_WINDOW_KILL,    None                     },
    { CHAR2SYM("GravityTopLeft"),     SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP_LEFT     }, 
    { CHAR2SYM("GravityTopRight"),    SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP_RIGHT    }, 
    { CHAR2SYM("GravityTop"),         SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP          }, 
    { CHAR2SYM("GravityLeft"),        SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_LEFT         }, 
    { CHAR2SYM("GravityCenter"),      SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_CENTER       }, 
    { CHAR2SYM("GravityRight"),       SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_RIGHT        }, 
    { CHAR2SYM("GravityBottomLeft"),  SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM_LEFT  }, 
    { CHAR2SYM("GravityBottomRight"), SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM_RIGHT },
    { CHAR2SYM("GravityBottom"),      SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM       }
  }; /* }}} */

  RubyHooks hooks[] = /* {{{ */
  {
    { CHAR2SYM("HookJump"),      &subtle->hooks.jump      },
    { CHAR2SYM("HookConfigure"), &subtle->hooks.configure },
    { CHAR2SYM("HookCreate"),    &subtle->hooks.create    },
    { CHAR2SYM("HookFocus"),     &subtle->hooks.focus     },
    { CHAR2SYM("HookGravity"),   &subtle->hooks.gravity   }
  }; /* }}} */

  /* Create various types */
  switch(extra)
    {
      case SUB_TYPE_GRAB: /* {{{ */
        switch(rb_type(value)) ///< Check value type
          {
            case T_STRING: ///< String
              type = SUB_GRAB_EXEC;
              data = DATA(strdup(RSTRING_PTR(value)));
              break;
            case T_SYMBOL: ///< Symbol
              for(i = 0; i < LENGTH(grabs); i++)
                {
                  if(value == grabs[i].sym)
                    {
                      type = grabs[i].value;
                      data = DATA(grabs[i].extra);
                      break;
                    }
                }

              if(-1 == type) ///< Symbols with parameters
                {
                  const char *name = SYM2CHAR(value);

                  if(!strncmp(name, "ViewJump", 8)) ///< View jump
                    {
                      if((name = (char *)name + 8))
                        {
                          type = SUB_GRAB_VIEW_JUMP;
                          data = DATA((unsigned long)(atol(name) - 1));
                        }
                    }
                  else if(!strncmp(name, "ScreenJump", 10)) ///< Screen jump
                    {
                      if((name = (char *)name + 10))
                        {
                          type = SUB_GRAB_SCREEN_JUMP;
                          data = DATA((unsigned long)(atol(name) - 1));
                        }
                    }
                  else if(!strncmp(name, "WindowScreen", 12)) ///< Window screen
                    {
                      if((name = (char *)name + 12))
                        {
                          type = SUB_GRAB_WINDOW_SCREEN;
                          data = DATA((unsigned long)(atol(name) - 1));
                        }
                    }
                } 
              break;
            case T_DATA: ///< Proc   
              type = SUB_GRAB_PROC;
              data = DATA(value);

              rb_ary_push(shelter, value); ///< Protect from GC
              break;
            default:
              subSharedLogWarn("Failed parsing grab `%s'\n", RSTRING_PTR(key));
              return Qnil;
          }

        if(-1 != type && (entry = (void *)subGrabNew(RSTRING_PTR(key), type, data)))
          subArrayPush(subtle->grabs, entry);
        break; /* }}} */
      case SUB_TYPE_HOOK: /* {{{ */
        if(T_SYMBOL == rb_type(key) && T_DATA == rb_type(value)) ///< Check value types
          {
            for(i = 0; i < LENGTH(hooks); i++)
              if(key == hooks[i].sym)
                {
                  *hooks[i].hook = value; ///< Store proc
                  rb_ary_push(shelter, value); ///< Protect from GC
                  
                  subSharedLogDebug("hook=%s\n", SYM2CHAR(hooks[i].sym));
                  break;
                }
          }
        break; /* }}} */
      case SUB_TYPE_TAG: /* {{{ */
        switch(rb_type(value)) ///< Check value type
          {
            case T_STRING:
              if((entry = (void *)subTagNew(RSTRING_PTR(key), RSTRING_PTR(value))))
                subArrayPush(subtle->tags, entry);
              break;
            case T_HASH:
              {
                int mode;
                SubTag *t = NULL;
                char *regex = RubyGetString(value, "regex", NULL);

                if((t = subTagNew(RSTRING_PTR(key), regex)))
                  {
                    /* Fetch values */
                    t->gravity = RubyGetFixnum(value, "gravity", 0);
                    t->screen  = RubyGetFixnum(value, "screen",  0);

                    /* Sanity */
                    if(1 <= t->gravity && 9 >= t->gravity) t->flags |= SUB_MODE_GRAVITY;
                    if(0 <= t->screen && t->screen < subtle->screens->ndata)
                      t->flags  |= SUB_MODE_SCREEN;
                    if(RubyGetRect(value, "size", &t->size)) 
                      t->flags |= SUB_MODE_SIZE;

                    /* Set property flags */
                    if(True == (mode = RubyGetBool(value, "full")))
                      t->flags |= SUB_MODE_FULL;
                    else if(False == mode) t->flags |= SUB_MODE_UNFULL;

                    if(True == (mode = RubyGetBool(value, "float")))  
                      t->flags |= SUB_MODE_FLOAT;
                    else if(False == mode) t->flags |= SUB_MODE_UNFLOAT;

                    if(True == (mode = RubyGetBool(value, "stick"))) 
                      t->flags |= SUB_MODE_STICK;
                    else if(False == mode) t->flags |= SUB_MODE_UNSTICK;

                    if(True == (mode = RubyGetBool(value, "urgent"))) 
                      t->flags |= SUB_MODE_URGENT;
                    else if(False == mode) t->flags |= SUB_MODE_UNURGENT;

                    subArrayPush(subtle->tags, (void *)t);
                  }
                break;
              }
            default:
              subSharedLogWarn("Failed parsing tag `%s'\n", SYM2CHAR(key));
              return Qnil;
          }

        break; /* }}} */
      case SUB_TYPE_VIEW: /* {{{ */
        if((entry = (void *)subViewNew(RSTRING_PTR(key), RSTRING_PTR(value))))
          subArrayPush(subtle->views, entry);
        break; /* }}} */
      default: subSharedLogDebug("Never to be reached?!\n");
    }

  return Qnil;
} /* }}} */

/* RubyParsePanel {{{ */
int
RubyParsePanel(VALUE hash,
  char *key,
  SubPanel **p)
{
  Window panel = subtle->windows.panel1;
  int i, j, flags = 0, enabled = False;
  VALUE entry = Qnil, value = RubyGetValue(hash, key, T_ARRAY, False); 
  VALUE spacer = CHAR2SYM("spacer");
 
  RubyPanels panels[] = {
    { CHAR2SYM("views"),   &subtle->panels.views   },
    { CHAR2SYM("caption"), &subtle->panels.caption },
    { CHAR2SYM("sublets"), &subtle->panels.sublets },
    { CHAR2SYM("tray"),    &subtle->panels.tray    }
  };

  if(!strncmp(key, "bottom", 6))
    {
      flags |= SUB_PANEL_BOTTOM;
      panel  = subtle->windows.panel2;
    }
 
  /* Parse panels */
  if(T_ARRAY == rb_type(value))
    {
      for(i = 0; Qnil != (entry = rb_ary_entry(value, i)); ++i)
        {
          if(entry == spacer)
            {
              flags |= SUB_PANEL_SPACER1;
            }
          else
            for(j = 0; j < LENGTH(panels); j++)
              {
                if(entry == panels[j].sym)
                  {
                    /* Append to list */
                    if(!subtle->panel)
                      {
                        subtle->panel = panels[j].panel;
                        *p            = panels[j].panel;
                      }
                    else (*p)->next  = panels[j].panel;

                    /* Update panel */
                    *p          = panels[j].panel;
                    (*p)->flags = SUB_TYPE_PANEL|flags;

                    XReparentWindow(subtle->dpy, (*p)->win, panel, 0, 0);

                    flags   = 0;
                    enabled = True; /// Enable this panel
                  }
              }
        }
    }

  /* Add end spacer */
  if(*p && flags & SUB_PANEL_SPACER1) (*p)->flags |= SUB_PANEL_SPACER2;

  return enabled;
} /* }}} */

/* RubyParseColor {{{ */
unsigned long
RubyParseColor(char *name)
{
  XColor color = { 0 }; ///< Default color

  /* Parse and store color */
  if(!XParseColor(subtle->dpy, COLORMAP, name, &color))
    {
      subSharedLogWarn("Failed loading color `%s'\n", name);
    }
  else if(!XAllocColor(subtle->dpy, COLORMAP, &color))
    subSharedLogWarn("Failed allocating color `%s'\n", name);

  return color.pixel;
} /* }}} */

/* RubySubletMark {{{ */
static void
RubySubletMark(SubSublet *s)
{
  if(s) rb_gc_mark(s->recv);
} /* }}} */

/* RubyWrapInit {{{ */
static VALUE
RubyWrapInit(VALUE data)
{
  rb_obj_call_init(data, 0, NULL); ///< Call initialize

  return Qnil;
} /* }}} */

/* RubySubtleTagAdd {{{ */
static VALUE
RubySubtleTagAdd(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil;

  if(T_STRING == rb_type(value))
    {
      SubTag *t = NULL;

      /* Create new tag */
      if((t = subTagNew(RSTRING_PTR(value), NULL)))
        {
          int tid = -1;
          VALUE mod = Qnil, klass = Qnil;

          subArrayPush(subtle->tags, (void *)t);
          subTagPublish();

          /* Create new instance */
          tid   = subArrayIndex(subtle->tags, (void *)t);
          mod   = rb_const_get(rb_mKernel, rb_intern("Subtlext"));
          klass = rb_const_get(mod, rb_intern("Tag"));
          tag   = rb_funcall(klass, rb_intern("new"), 1, value);

          rb_iv_set(tag, "@id",   INT2FIX(tid));
          rb_iv_set(tag, "@name", rb_str_new2(t->name));
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return tag;
} /* }}} */

/* RubySubtleViewAdd {{{ */
static VALUE
RubySubtleViewAdd(VALUE self,
  VALUE value)
{
  VALUE view = Qnil;

  if(T_STRING == rb_type(value))
    {
      SubView *v = NULL;
      
      /* Create new view */
      if((v = subViewNew(RSTRING_PTR(value), NULL)))
        {
          int vid = -1;
          VALUE mod = Qnil, klass = Qnil;

          subArrayPush(subtle->views, (void *)v);
          subClientUpdate(-1); ///< Grow
          subViewUpdate();
          subViewPublish();
          subViewRender();    

          /* Create new instance */
          vid   = subArrayIndex(subtle->views, (void *)v);
          mod   = rb_const_get(rb_mKernel, rb_intern("Subtlext"));
          klass = rb_const_get(mod, rb_intern("View"));
          view  = rb_funcall(klass, rb_intern("new"), 1, value);

          rb_iv_set(view, "@id",   INT2FIX(vid));
          rb_iv_set(view, "@name", rb_str_new2(v->name));
          rb_iv_set(view, "@win",  LONG2NUM(v->button));      
        }
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return view;
} /* }}} */

/* RubySubletNew {{{ */
/*
 * call-seq: new -> Subtle::Sublet
 *
 * Create new Sublet object
 *
 *  tag = Subtle::Subtle.new
 *  => #<Subtle::Sublet:xxx>
 */

static VALUE
RubySubletNew(VALUE self)
{
  int state = 0;
  SubSublet *s = NULL;
  VALUE data = Qnil;
  char *name = (char *)rb_class2name(self);

  /* Create sublet */
  s = subSubletNew();
  data    = Data_Wrap_Struct(self, RubySubletMark, NULL, (void *)s);
  s->recv = data; 
  s->name = strdup(name);

  /* Call initialize */
  rb_protect(RubyWrapInit, data, &state);
  if(state) 
    {
      RubyPerror(True, False, "Failed initializing sublet `%s'", name);

      subSubletKill(s, False);

      return Qnil;
    }

  if(0 == s->interval) s->interval = 60; ///< Sanitize

  subArrayPush(subtle->sublets, s);
  rb_ary_push(shelter, data); ///< Protect from GC

  /* Check click handler */
  if(rb_respond_to(s->recv, rb_intern("click"))) s->flags |= SUB_SUBLET_CLICK;

  return data;
} /* }}} */

/* RubySubletInherited {{{ */
/*
 * call-seq: inherited(recv) -> nil
 *
 * Callback for inheritance - internal use only
 *
 *  tag = Subtle::Subtle.new
 *  => #<Subtle::Sublet:xxx>
 */

static VALUE
RubySubletInherited(VALUE self,
  VALUE recv)
{
  inherited = recv; ///< Store inherited sublet

  return Qnil;
} /* }}} */

/* RubySubletInterval {{{ */
/*
 * call-seq: interval -> Fixnum
 *
 * Get interval time of Sublet
 *
 *  puts sublet.interval
 *  => 60
 */

static VALUE
RubySubletInterval(VALUE self)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  return s ? INT2FIX(s->interval) : Qnil;
} /* }}} */

/* RubySubletIntervalSet {{{ */
/*
 * call-seq: interval=(fixnum) -> nil
 *
 * Set interval time of Sublet
 *
 *  sublet.interval = 60
 *  => nil
 */

static VALUE
RubySubletIntervalSet(VALUE self,
  VALUE value)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);
  if(s)
    {
      switch(rb_type(value))
        {
          case T_FIXNUM: 
            s->interval = FIX2INT(value); 
            s->time     = subSharedTime() + s->interval;

            if(0 < s->interval) s->flags |= SUB_SUBLET_INTERVAL;
            else s->flags &= ~SUB_SUBLET_INTERVAL;
            break;
          default: rb_raise(rb_eArgError, "Unknown value type");
        }
    }

  return Qnil;
} /* }}} */

/* RubySubletData {{{ */
/*
 * call-seq: data -> String or nil
 *
 * Get data of Sublet
 *
 *  puts sublet.data
 *  => "subtle"
 */

static VALUE
RubySubletData(VALUE self)
{
  int i;
  VALUE string = Qnil;
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);
  
  /* Concat string */
  if(s && 0 < s->text->ndata) 
    {
      for(i = 0; i < s->text->ndata; i++)
        {
          SubText *t = TEXT(s->text->data[i]);

          if(Qnil == string) rb_str_new2(t->data.string);
          else rb_str_cat(string, t->data.string, strlen(t->data.string));
        }
    }

  return string;
} /* }}} */

/* RubySubletDataSet {{{ */
/*
 * call-seq: data=(string) -> nil
 *
 * Set data of Sublet
 *
 *  sublet.data = "subtle"
 *  => nil
 */

static VALUE
RubySubletDataSet(VALUE self,
  VALUE value)
{
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);

  if(s && RTEST(value) && T_STRING == rb_type(value)) ///< Check value type
    {
      subSubletSetData(s, RSTRING_PTR(value)); 
    }
  else rb_raise(rb_eArgError, "Unknown value type");

  return Qnil;
} /* }}} */

/* RubySubletWatch {{{ */
/*
 * call-seq: watch(source) -> true or false
 *
 * Add watch file via inotify or socket
 *
 *  watch "/path/to/file"
 *  => true
 *
 *  @socket = TCPSocket("localhost", 6600)
 *  watch @socket
 */

static VALUE
RubySubletWatch(VALUE self,
  VALUE value)
{
  int flags = 0;
  VALUE ret = Qfalse;
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);

  if(s && !(s->flags & (SUB_SUBLET_SOCKET|SUB_SUBLET_INOTIFY)) && RTEST(value))
    {
      if(rb_respond_to(value, rb_intern("fileno"))) ///< Probably a socket
        {
          int fd = FIX2INT(rb_funcall(value, rb_intern("fileno"), 0, NULL)); ///< Get socket file number

          s->flags |= SUB_SUBLET_SOCKET;
          s->watch  = fd;

          XSaveContext(subtle->dpy, subtle->panels.sublets.win, s->watch, (void *)s);
          subEventWatchAdd(s->watch);

          /* Set nonblocking */
          if(-1 == (flags = fcntl(fd, F_GETFL, 0))) flags = 0;
          fcntl(fd, F_SETFL, flags | O_NONBLOCK);
 
          ret = Qtrue;
        }
#ifdef HAVE_SYS_INOTIFY_H
      else if(T_STRING == rb_type(value)) /// Inotify file
        {
          char *watch = RSTRING_PTR(value);

          /* Create inotify watch */
          if(0 < (s->watch = inotify_add_watch(subtle->notify, watch, IN_MODIFY)))
            {
              s->flags |= SUB_SUBLET_INOTIFY;

              XSaveContext(subtle->dpy, subtle->panels.sublets.win, s->watch, (void *)s);
              subSharedLogDebug("Inotify: Adding watch on %s\n", watch); 

              ret = Qtrue;
            }
          else subSharedLogWarn("Failed adding watch on file `%s': %s\n", 
            watch, strerror(errno));
        }
#endif /* HAVE_SYS_INOTIFY_H */
      else subSharedLogWarn("Failed handling unknown value type\n");
    }

  return ret;
} /* }}} */

/* RubySubletUnwatch {{{ */
/*
 * call-seq: unwatch -> true or false
 *
 * Remove watch from Sublet
 *
 *  unwatch
 *  => true
 */

static VALUE
RubySubletUnwatch(VALUE self)
{
  VALUE ret = Qfalse;
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);
  if(s)
    {
      if(s->flags & SUB_SUBLET_SOCKET) ///< Probably a socket
        {
          XDeleteContext(subtle->dpy, subtle->panels.sublets.win, s->watch);
          subEventWatchDel(s->watch);

          s->flags &= ~SUB_SUBLET_SOCKET;
          s->watch  = 0;

          ret = Qtrue;
        }
#ifdef HAVE_SYS_INOTIFY_H
      else if(s->flags & SUB_SUBLET_INOTIFY) /// Inotify file
        {
          XDeleteContext(subtle->dpy, subtle->panels.sublets.win, s->watch);
          inotify_rm_watch(subtle->notify, s->watch);

          s->flags &= ~SUB_SUBLET_INOTIFY;
          s->watch  = 0;

          ret = Qtrue;
        }
#endif /* HAVE_SYS_INOTIFY_H */
    }

  return ret;
} /* }}} */

/* RubyIconInit {{{ */
/*
 * call-seq: new(path) -> Subtle::Icon
 *
 * Create new Icon object
 *
 *  tag = Subtle::Icon.new("/path/to/icon")
 *  => #<Subtle::Icon:xxx>
 */

static VALUE
RubyIconInit(VALUE self,
  VALUE path)
{
  int id = -1;
  SubPixmap *p = NULL;

  /* Creating pixmap */
  if(-1 == (id = subPixmapFind(RSTRING_PTR(path))) &&
    (p = subPixmapNew(RSTRING_PTR(path))))
    {
      id = subtle->pixmaps->ndata;
      subArrayPush(subtle->pixmaps, (void *)p);
    }

  rb_iv_set(self, "@id", INT2FIX(id));

  return self;
} /* }}} */

/* RubyIconToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Icon object to String
 *
 *  puts icon
 *  => "<>!4<>" 
 */

static VALUE
RubyIconToString(VALUE self)
{
  char buf[12];
  int id = FIX2INT(rb_iv_get(self, "@id"));

  snprintf(buf, sizeof(buf), "%s!%d%s",
    SEPARATOR, id, SEPARATOR);

  return rb_str_new2(buf);
} /* }}} */

/* RubyIconOperatorPlus {{{ */
/*
 * call-seq: +(string) -> String
 *
 * Convert self to String and add String
 *
 *  icon + "subtle"
 *  => "<>!4<>subtle"
 */

static VALUE
RubyIconOperatorPlus(VALUE self,
  VALUE value)
{
  return RubyConcat(RubyIconToString(self), value);
} /* }}} */

/* RubyColorInit {{{ */
/*
 * call-seq: new(color) -> Subtle::Color
 *
 * Create new Color object
 *
 *  tag = Subtle::Color.new("#336699")
 *  => #<Subtle::Color:xxx>
 */

static VALUE
RubyColorInit(VALUE self,
  VALUE color)
{
  /* Check arguments */ 
  if(RTEST(color) && T_STRING == rb_type(color))
    {
      unsigned long pixel = RubyParseColor(RSTRING_PTR(color));

      rb_iv_set(self, "@pixel", INT2FIX(pixel));
    }
  else
    {
      rb_raise(rb_eArgError, "Unknown value type");
      return Qnil;
    }

  return self;
} /* }}} */

/* RubyColorToString {{{ */
/*
 * call-seq: to_str -> String
 *
 * Convert Color object to String
 *
 *  puts color
 *  => "<>123456789<>" 
 */

static VALUE
RubyColorToString(VALUE self)
{
  char buf[20];
  VALUE pixel = rb_iv_get(self, "@pixel");

  snprintf(buf, sizeof(buf), "%s#%ld%s",
    SEPARATOR, NUM2LONG(pixel), SEPARATOR);

  return rb_str_new2(buf);
} /* }}} */

/* RubyColorOperatorPlus {{{ */
/*
 * call-seq: +(string) -> String
 *
 * Convert self to String and add String
 *
 *  color + "subtle"
 *  => "<>123456789<>subtle"
 */

static VALUE
RubyColorOperatorPlus(VALUE self,
  VALUE value)
{
  return RubyConcat(RubyColorToString(self), value);
} /* }}} */

/* RubyWrapLoadConfig {{{ */
static VALUE
RubyWrapLoadConfig(VALUE data)
{
  int i, state = 0;
  char *font = NULL, buf[100], path[50];
  VALUE config = Qnil, entry = Qnil;
  SubPanel *p = NULL;

  /* Check config paths */
  if(subtle->paths.config) 
    snprintf(path, sizeof(path), "%s", subtle->paths.config);
  else
    {
      FILE *fd = NULL;
      char *tok = NULL, *bufp = buf, *home = getenv("XDG_CONFIG_HOME"), 
        *dirs = getenv("XDG_CONFIG_DIRS");

      /* Combine paths */
      snprintf(path, sizeof(path), "%s/.config", getenv("HOME"));
      snprintf(buf, sizeof(buf), "%s:%s", home ? home : path, dirs ? dirs : "/etc/xdg");

      /* Search config in XDG path */
      while((tok = strsep(&bufp, ":")))
        {
          snprintf(path, sizeof(path), "%s/%s/%s", tok, PKG_NAME, PKG_CONFIG);

          if((fd = fopen(path, "r"))) ///< Check if config file exists
            {
              fclose(fd);
              break;
            }

          subSharedLogDebug("Checked config=%s\n", path);
        }
    }
  printf("Using config `%s'\n", path);
  rb_load_protect(rb_str_new2(path), 0, &state); ///< Load config

  if(state) RubyPerror(True, True, "Failed finding config in `%s'", buf);

  if(!subtle || !subtle->dpy) return Qnil; ///< Exit after config check

  /* Config: Options */
  config          = rb_const_get(rb_cObject, rb_intern("OPTIONS"));
  subtle->bw      = RubyGetFixnum(config, "border",  2);
  subtle->step    = RubyGetFixnum(config, "step",    5);
  subtle->snap    = RubyGetFixnum(config, "snap",    10);
  subtle->gravity = RubyGetFixnum(config, "gravity", 5);
  RubyGetRect(config, "padding", &subtle->strut);
  if(True == RubyGetBool(config, "urgent")) subtle->flags |= SUB_SUBTLE_URGENT;

  /* Config: Font */
  font = RubyGetString(config, "font", FONT);
  if(subtle->xfs) ///< Free in case of reload
    {
      XFreeFont(subtle->dpy, subtle->xfs);
      subtle->xfs = NULL;
    }

  if(!(subtle->xfs = XLoadQueryFont(subtle->dpy, font)))
    {
      subSharedLogWarn("Failed loading font `%s'\n", font);

      subtle->xfs = XLoadQueryFont(subtle->dpy, FONT);
      if(!subtle->xfs) subSharedLogError("Failed loading font `%s`\n", FONT);
    }

  /* Calculate font size and panel height */
  subtle->th = subtle->xfs->max_bounds.ascent + subtle->xfs->max_bounds.descent + 2;
  subtle->fy = (subtle->th - 2 + subtle->xfs->max_bounds.ascent) / 2;

  /* Config: Colors */
  config                    = rb_const_get(rb_cObject, rb_intern("COLORS"));
  subtle->colors.fg_panel   = RubyParseColor(RubyGetString(config, "fg_panel",      "#e2e2e5"));
  subtle->colors.fg_views   = RubyParseColor(RubyGetString(config, "fg_views",      "#CF6171"));
  subtle->colors.fg_sublets = RubyParseColor(RubyGetString(config, "fg_sublets",    "#CF6171"));
  subtle->colors.fg_focus   = RubyParseColor(RubyGetString(config, "fg_focus",      "#000000"));
  subtle->colors.bg_panel   = RubyParseColor(RubyGetString(config, "bg_panel",      "#444444"));
  subtle->colors.bg_views   = RubyParseColor(RubyGetString(config, "bg_views",      "#3d3d3d"));
  subtle->colors.bg_sublets = RubyParseColor(RubyGetString(config, "bg_sublets",    "#CF6171"));
  subtle->colors.bg_focus   = RubyParseColor(RubyGetString(config, "bg_focus",      "#CF6171"));
  subtle->colors.bo_focus   = RubyParseColor(RubyGetString(config, "border_focus",  "#CF6171"));
  subtle->colors.bo_normal  = RubyParseColor(RubyGetString(config, "border_normal", "#CF6171"));
  subtle->colors.bg         = RubyParseColor(RubyGetString(config, "background",    "#3d3d3d3"));

  /* Config: Panels */
  config = rb_const_get(rb_cObject, rb_intern("PANEL"));
  if(RubyParsePanel(config, "top", &p))      subtle->flags |= SUB_SUBTLE_PANEL1;
  if(RubyParsePanel(config, "bottom", &p))   subtle->flags |= SUB_SUBTLE_PANEL2;
  
  /* Separator */
  subtle->separator.string = strdup(RubyGetString(config, "separator", "|"));
  subtle->separator.width  = subSharedTextWidth(subtle->separator.string, 
    strlen(subtle->separator.string), NULL, NULL, True) + 6;

  /* Config: Grabs */
  config = rb_const_get(rb_cObject, rb_intern("GRABS"));
  rb_hash_foreach(config, RubyParseForeach, SUB_TYPE_GRAB);

  if(0 == subtle->grabs->ndata)
    {
      subSharedLogWarn("No grabs found\n");
    }
  else subArraySort(subtle->grabs, subGrabCompare);

  /* Config: Tags */
  config = rb_const_get(rb_cObject, rb_intern("TAGS"));

  /*Check default tag */
  if(T_HASH != rb_type(config) || 
    Qtrue != rb_funcall(config, rb_intern("has_key?"), 1, rb_str_new2("default")))
    {
      SubTag *t = subTagNew("default", NULL);

      subArrayPush(subtle->tags, (void *)t);
    }
  else ///< Fetch default tag
    {
      VALUE key = Qnil, value = Qnil;

      key   = rb_str_new2("default");
      value = rb_funcall(config, rb_intern("delete"), 1, key);
      
      RubyParseForeach(key, value, SUB_TYPE_TAG);
    }

  if(T_HASH == rb_type(config)) ///< Parse tags hash
    rb_hash_foreach(config, RubyParseForeach, SUB_TYPE_TAG);

  subTagPublish();

  if(1 == subtle->tags->ndata) subSharedLogWarn("No tags found\n");

  /* Config: Views */
  config = rb_const_get(rb_cObject, rb_intern("VIEWS"));
  switch(rb_type(config)) ///< Allow hashes or arrays for ordering
    {
      case T_HASH: rb_hash_foreach(config, RubyParseForeach, SUB_TYPE_VIEW); break;
      case T_ARRAY:
        for(i = 0; Qnil != (entry = rb_ary_entry(config, i)); ++i)
          {
            if(T_HASH == rb_type(entry))
              rb_hash_foreach(entry, RubyParseForeach, SUB_TYPE_VIEW);
            else subSharedLogWarn("Failed parsing view entry %d\n", i);
          }
        break;
      default: break;
    }

  /* Views */
  if(0 == subtle->views->ndata) ///< Create default view
    {
      SubView *v = subViewNew("subtle", "default");

      subArrayPush(subtle->views, (void *)v);
      subSharedLogWarn("No views found\n");
    }
  else ///< Check default tag
    {
      SubView *v = NULL;

      /* Check for view with default tag */
      for(i = subtle->views->ndata - 1; i >= 0; i--)
        if((v = VIEW(subtle->views->data[i])) && v->tags & SUB_TAG_DEFAULT)
          {
            subSharedLogDebug("Default view: name=%s\n", v->name);
            break;
          }

      v->tags |= SUB_TAG_DEFAULT; ///< Set default tag

      /* EWMH: Tags */
      subEwmhSetCardinals(v->button, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1);
    }

  subViewUpdate();
  subViewPublish();

  /* Config: Hooks */
  config = rb_const_get(rb_cObject, rb_intern("HOOKS"));
  rb_hash_foreach(config, RubyParseForeach, SUB_TYPE_HOOK);

  return Qnil;
} /* }}} */

/* RubyWrapLoadSubtlext {{{ */
static VALUE
RubyWrapLoadSubtlext(VALUE data)
{
  VALUE mod = Qnil, klass = Qnil;

  /* Load and init subtlext */
  rb_require("subtle/subtlext"); ///< Load subtlext
  mod = rb_const_get(rb_mKernel, rb_intern("Subtlext"));

  /* Class: subtle */
  klass    = rb_const_get(mod, rb_intern("Subtle"));
  subtlext = rb_funcall(klass, rb_intern("new2"), 1, (VALUE)subtle->dpy);

  /* @todo Overwrite methods to bypass timing problems */
  rb_define_method(klass, "add_tag",  RubySubtleTagAdd,  1);
  rb_define_method(klass, "add_view", RubySubtleViewAdd, 1);

  rb_ary_push(shelter, subtlext); ///< Protect from GC

  printf("Loaded subtlext\n");

  return Qnil;
}/* }}} */

/* RubyWrapCall {{{ */
static VALUE
RubyWrapCall(VALUE data)
{
  VALUE ret = Qfalse, *rargs = (VALUE *)data;

  if((int)rargs[0] & SUB_CALL_SUBLET_RUN) /* {{{ */
    {
      ret = rb_funcall(rargs[1], rb_intern("run"), 0, NULL);
    } /* }}} */
  else if((int)rargs[0] & SUB_CALL_SUBLET_CLICK) /* {{{ */
    {
      Window win = 0;
      int x = 0, y = 0, arity = 0;
      XButtonEvent *ev = (XButtonEvent *)rargs[2];
      VALUE meth = Qnil, cb = rb_intern("click");

      /* Get arity of callback */
      meth  = rb_funcall(rargs[1], rb_intern("method"), 1, CHAR2SYM("click"));
      arity = FIX2INT(rb_funcall(meth, rb_intern("arity"), 0, NULL));

      XTranslateCoordinates(subtle->dpy, ev->window, ev->subwindow, 
        ev->x, ev->y, &x, &y, &win);

      ret = rb_funcall(rargs[1], cb, arity, INT2FIX(ev->button), INT2FIX(x), INT2FIX(y));

      subSharedLogDebug("Proc: arity=%d\n", arity);      
    } /* }}} */
  else if((int)rargs[0] & (SUB_CALL_GRAB|SUB_CALL_HOOK)) /* {{{ */
    {
      int id = 0, arity = 0, flags = 0;

      /* Check proc arity */
      switch((arity = FIX2INT(rb_funcall(rargs[1], rb_intern("arity"), 0, NULL))))
        {
          case 0:
          case -1: ///< No optional arguments 
            ret = rb_funcall(rargs[1], rb_intern("call"), 0, NULL);
            break;
          case 1: ///< One argument
            if(rargs[2])
              {
                SubClient *c = CLIENT(rargs[2]);
                VALUE mod = Qnil, klass = Qnil, inst = Qnil;

                if(Qnil == subtlext) subRubyLoadSubtlext(); ///< Load subtlext on demand
                mod = rb_const_get(rb_mKernel, rb_intern("Subtlext"));

                if(c->flags & SUB_TYPE_CLIENT)
                  {
                    /* Create client instance */
                    id    = subArrayIndex(subtle->clients, (void *)c);
                    klass = rb_const_get(mod, rb_intern("Client"));
                    inst  = rb_funcall(klass, rb_intern("new"), 1, LONG2NUM(c->win));

                    /* Translate flags */
                    if(c->flags & SUB_MODE_FULL)  flags |= SUB_EWMH_FULL;
                    if(c->flags & SUB_MODE_FLOAT) flags |= SUB_EWMH_FLOAT;
                    if(c->flags & SUB_MODE_STICK) flags |= SUB_EWMH_STICK;

                    /* Set properties */
                    rb_iv_set(inst, "@id",      INT2FIX(id));
                    rb_iv_set(inst, "@name",    rb_str_new2(c->name));
                    rb_iv_set(inst, "@klass",   rb_str_new2(c->klass));
                    rb_iv_set(inst, "@x",       INT2FIX(c->geom.x));
                    rb_iv_set(inst, "@y",       INT2FIX(c->geom.y));
                    rb_iv_set(inst, "@width",   INT2FIX(c->geom.width));
                    rb_iv_set(inst, "@height",  INT2FIX(c->geom.height));
                    rb_iv_set(inst, "@gravity", INT2FIX(c->gravity));
                    rb_iv_set(inst, "@screen",  INT2FIX(c->screen + 1));
                    rb_iv_set(inst, "@flags",   INT2FIX(flags));
                  }
                else if(c->flags & SUB_TYPE_VIEW)
                  {
                    SubView *v = VIEW(c);

                    /* Create view instance */
                    id    = subArrayIndex(subtle->views, (void *)v);
                    klass = rb_const_get(mod, rb_intern("View"));
                    inst  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(v->name));

                    rb_iv_set(inst, "@id",  INT2FIX(id));
                    rb_iv_set(inst, "@win", LONG2NUM(v->button));
                  }

                ret = rb_funcall(rargs[1], rb_intern("call"), 1, inst);
                break;
              } ///< Fall through
          default:
            rb_raise(rb_eStandardError, "Failed calling proc with `%d' argument(s)", arity);
            ret = Qfalse;
        }
      subSharedLogDebug("Proc: arity=%d\n", arity);      
    } /* }}} */

  return ret;
} /* }}} */

/* RubyWrapRemove {{{ */
static VALUE
RubyWrapRemove(VALUE name)
{
  VALUE object = rb_const_get(rb_mKernel, rb_intern("Object"));

  return rb_funcall(object, rb_intern("send"), 2, rb_str_new2("remove_const"), name);
} /* }}} */

/* RubyWrapRelease {{{ */
static VALUE
RubyWrapRelease(VALUE value)
{
  if(Qtrue == rb_funcall(shelter, rb_intern("include?"), 1, value))
    rb_funcall(shelter, rb_intern("delete"), 1, value);

  return Qnil;
} /* }}} */

 /** subRubyInit {{{
  * @brief Init ruby
  **/

void
subRubyInit(void)
{
  VALUE mod = Qnil, sublet = Qnil, icon = Qnil, color = Qnil;

  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

  rb_define_method(rb_mKernel, "method_missing", RubyDispatcher, -1); ///< Subtlext dispatcher

  /*
   * Document-class: Subtle 
   *
   * Subtle is the module for internal use in subtle for sublets.
   */

  mod = rb_define_module("Subtle");

  /*
   * Document-class: Subtle::Sublet
   *
   * Sublet class for interaction with sublets
   */

  sublet = rb_define_class_under(mod, "Sublet", rb_cObject);
  rb_define_singleton_method(sublet, "new",       RubySubletNew,       0);
  rb_define_singleton_method(sublet, "inherited", RubySubletInherited, 1);
  rb_define_method(sublet, "interval",  RubySubletInterval,    0);
  rb_define_method(sublet, "interval=", RubySubletIntervalSet, 1);
  rb_define_method(sublet, "data",      RubySubletData,        0);
  rb_define_method(sublet, "data=",     RubySubletDataSet,     1);
  rb_define_method(sublet, "watch",     RubySubletWatch,       1);
  rb_define_method(sublet, "unwatch",   RubySubletUnwatch,     0);

  /*
   * Document-class: Subtle::Icon
   *
   * Icon class for interaction with icons
   */

  icon = rb_define_class_under(mod, "Icon", rb_cObject);

  /* Icon id */
  rb_define_attr(icon, "id", 1, 0);

  rb_define_method(icon, "initialize", RubyIconInit,         1);
  rb_define_method(icon, "to_str",     RubyIconToString,     0);
  rb_define_method(icon, "+",          RubyIconOperatorPlus, 1);
  rb_define_alias(icon, "to_s", "to_str");

  /*
   * Document-class: Subtle::Color
   *
   * Color class for interaction with colors
   */

  color = rb_define_class_under(mod, "Color", rb_cObject);

  /* Pixel number */
  rb_define_attr(color, "pixel", 1, 0);

  rb_define_method(color, "initialize", RubyColorInit,         1);
  rb_define_method(color, "to_str",     RubyColorToString,     0);
  rb_define_method(color, "+",          RubyColorOperatorPlus, 1);
  rb_define_alias(color, "to_s", "to_str");

  /* Bypassing garbage collection */
  shelter = rb_ary_new();
  rb_gc_register_address(&shelter);
} /* }}} */

 /** subRubyLoadConfig {{{
  * @brief Load config file
  * @param[in]  path  Path to config file
  **/

void
subRubyLoadConfig(void)
{
  int state = 0;

  /* Carefully load config */
  rb_protect(RubyWrapLoadConfig, Qnil, &state);
  if(state) RubyPerror(True, True, "Failed reading config");
} /* }}} */

 /** subRubyReloadConfig {{{
  * @brief Reload config file
  * @param[in]  path  Path to config file
  **/

void
subRubyReloadConfig(void)
{
  int i, state = 0;

  /* Reset before reloading */
  subtle->flags  &= (SUB_SUBTLE_DEBUG|SUB_SUBTLE_EWMH|SUB_SUBTLE_RUN);
  subtle->panel   = NULL;

  /* Clear x cache */
  subtle->panels.tray.x    = 0;
  subtle->panels.views.x   = 0;
  subtle->panels.caption.x = 0;
  subtle->panels.sublets.x = 0;

  /* Clear arrays */
  subArrayClear(subtle->grabs, True);
  subArrayClear(subtle->tags,  True);
  subArrayClear(subtle->views, True);

  /* Release hooks */
  if(subtle->hooks.jump)      subRubyRelease(subtle->hooks.jump);
  if(subtle->hooks.configure) subRubyRelease(subtle->hooks.configure);
  if(subtle->hooks.create)    subRubyRelease(subtle->hooks.create);
  if(subtle->hooks.focus)     subRubyRelease(subtle->hooks.focus);
  if(subtle->hooks.gravity)   subRubyRelease(subtle->hooks.gravity);

  if(subtle->separator.string) free(subtle->separator.string);

  /* Remove constants */
  subRubyRemove("OPTIONS");
  subRubyRemove("PANEL");
  subRubyRemove("COLORS");
  subRubyRemove("GRABS");
  subRubyRemove("TAGS");
  subRubyRemove("VIEWS");
  subRubyRemove("HOOKS");

   /* Carefully load config */
  rb_protect(RubyWrapLoadConfig, Qnil, &state);
  if(state) RubyPerror(True, True, "Failed reading config");

  subDisplayConfigure();

 /* Update client tags */
  for(i = 0; i < subtle->clients->ndata; i++)
    subClientSetTags(CLIENT(subtle->clients->data[i]));

  subViewJump(subtle->views->data[0]);
  subPanelRender();

  printf("Reloaded config\n");
} /* }}} */

 /** subRubyLoadSublet {{{
  * @brief Load sublets from path
  * @param[in]  path  Path of the sublets
  **/

void
subRubyLoadSublet(const char *file)
{
  int state = 0;
  char buf[100];

  /* Check path */
  if(subtle->paths.sublets)
    snprintf(buf, sizeof(buf), "%s/%s", subtle->paths.sublets, file);
  else
    {
      char *data = getenv("XDG_DATA_HOME"), path[50];

      /* Combine paths */
      snprintf(path, sizeof(path), "%s/.local/share", getenv("HOME"));
      snprintf(buf, sizeof(buf), "%s/%s/sublets/%s", 
        data ? data : path, PKG_NAME, file);
    }
  subSharedLogDebug("sublet=%s\n", buf);

  /* Carefully load sublet */
  rb_load_protect(rb_str_new2(buf), 0, &state); ///< Load sublet
  if(0 == state && Qnil != inherited)
    {
      VALUE self = Qnil;
      char *name = (char *)rb_class2name((ID)inherited);

      /* Instantiate sublet */
      if(Qnil != (self = rb_funcall(inherited, rb_intern("new"), 0, NULL)))
        {
          SubSublet *s = NULL;
          Data_Get_Struct(self, SubSublet, s);

          /* Append sublet to linked list */
          if(!subtle->sublet) subtle->sublet = s;
          else
            {
              SubSublet *iter = subtle->sublet;

              while(iter && iter->next) iter = iter->next;

              iter->next = s;
            }

          subRubyCall(SUB_CALL_SUBLET_RUN, s->recv, NULL); ///< First run

          printf("Loaded sublet (%s)\n", name);
        }
      else subSharedLogWarn("Failed instantiating sublet `%s'\n", name);

      inherited = Qnil;
    }
  else RubyPerror(True, False, "Failed loading sublet `%s'", buf);
} /* }}} */

 /** subRubyLoadSublets {{{
  * @brief Load sublets from path
  **/

void
subRubyLoadSublets(void)
{
  int i, num;
  char buf[100];
  struct dirent **entries = NULL;

#ifdef HAVE_SYS_INOTIFY_H
  if(0 > (subtle->notify = inotify_init()))
    {
      subSharedLogWarn("Failed initing inotify\n");
      subSharedLogDebug("Inotify: %s\n", strerror(errno));
    }
  else fcntl(subtle->notify, F_SETFL, O_NONBLOCK);
#endif /* HAVE_SYS_INOTIFY_H */

  /* Check path */
  if(subtle->paths.sublets)
    snprintf(buf, sizeof(buf), "%s", subtle->paths.sublets);
  else
    {
      char *data = getenv("XDG_DATA_HOME"), path[50];

      /* Combine paths */
      snprintf(path, sizeof(path), "%s/.local/share", getenv("HOME"));
      snprintf(buf, sizeof(buf), "%s/%s/sublets", data ? data : path, PKG_NAME);
    }
  printf("Loading sublets from `%s'\n", buf);

  /* Scan directory */
  if(0 < ((num = scandir(buf, &entries, RubyFilter, alphasort))))
    {
      for(i = 0; i < num; i++)
        {
          subRubyLoadSublet(entries[i]->d_name);

          free(entries[i]);
        }
      free(entries);

      subArraySort(subtle->sublets, subSubletCompare); ///< Initially sort
    }
  else subSharedLogWarn("No sublets found\n");

  subSubletUpdate();
  subSubletPublish();
} /* }}} */

 /** subRubyLoadSubtlext {{{
  * @brief Load subtlext
  **/

void
subRubyLoadSubtlext(void)
{
  int state = 0;

  /* Carefully load sublext */
  rb_protect(RubyWrapLoadSubtlext, Qnil, &state);
  if(state) RubyPerror(True, True, "Failed loading subtlext");
} /* }}} */

 /** subRubyCall {{{
  * @brief Safely call ruby script
  * @param[in]  type   Script type
  * @param[in]  recv   Script receiver
  * @param[in]  extra  Extra argument
  * @retval   0  Called script returned false
  * @retval   1  Called script returned true
  * @retval  -1  Calling script failed
  **/

int
subRubyCall(int type,
  unsigned long recv,
  void *extra)
{
  int state = 0;
  VALUE result = Qnil, rargs[3] = { 0 };

  /* Wrap up data */
  rargs[0] = (VALUE)type;
  rargs[1] = recv;
  rargs[2] = (VALUE)extra;

  signal(SIGALRM, RubySignal); ///< Limit execution time
  alarm(EXECTIME);

  /* Carefully call */
  result = rb_protect(RubyWrapCall, (VALUE)&rargs, &state);
  if(state) 
    {
      RubyPerror(True, False, "Failed calling %s",
        type & (SUB_CALL_SUBLET_RUN|SUB_CALL_SUBLET_CLICK) ? 
        "sublet" : (type & SUB_CALL_GRAB ? "grab" : "hook"));

      result = Qnil;
    }

  alarm(0);

  return Qtrue == result ? 1 : (Qfalse == result ? 0 : -1);
} /* }}} */

 /** subRubyRemove {{{
  * @brief Remove constant
  * @param[in]  name  Name of constant
  **/

int
subRubyRemove(char *name)
{
  int state = 0;

  rb_protect(RubyWrapRemove, rb_str_new2(name), &state);
 
  return state;
} /* }}} */

 /** subRubyRelease {{{
  * @brief Release value from shelter
  * @param[in]  value  The released value
  **/

int
subRubyRelease(unsigned long value)
{
  int state = 0;

  rb_protect(RubyWrapRelease, value, &state);
 
  return state;
} /* }}} */

 /** subRubyFinish {{{
  * @brief Finish ruby stack
  **/

void
subRubyFinish(void)
{
  ruby_finalize();

#ifdef HAVE_SYS_INOTIFY_H
  if(subtle && subtle->notify) close(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */

  subSharedLogDebug("kill=ruby\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
