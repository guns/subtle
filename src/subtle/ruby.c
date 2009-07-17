
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

static VALUE shelter = Qnil, sublet = Qnil, subtlext = Qnil; ///< Globals

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

/* RubySubletMark {{{ */
static void
RubySubletMark(SubSublet *s)
{
  rb_gc_mark(s->recv);
} /* }}} */

/* RubySubletNew {{{ */
static VALUE
RubySubletNew(VALUE self)
{
  SubSublet *s = NULL;
  VALUE data = Qnil;
  char *name = (char *)rb_class2name(self);

  /* Create sublet */
  s = subSubletNew();
  data    = Data_Wrap_Struct(self, RubySubletMark, NULL, (void *)s);
  s->recv = data; 
  s->name = strdup(name);

  rb_obj_call_init(data, 0, NULL); ///< Call initialize
  if(0 == s->interval) s->interval = 60; ///< Sanitize

  subArrayPush(subtle->sublets, s);
  rb_ary_push(shelter, data); ///< Protect from GC

  return data;
} /* }}} */

/* RubySubletInherited {{{ */
static VALUE
RubySubletInherited(VALUE self,
  VALUE recv)
{
  sublet = recv; ///< Store inherited sublet

  return Qnil;
} /* }}} */

/* RubySubletColor {{{ */
static VALUE
RubySubletColor(VALUE self,
  VALUE color)
{
  char buf[12];

  snprintf(buf, sizeof(buf), "<>%s<>", STR2CSTR(color));

  return rb_str_new2(buf);
} /* }}} */

/* RubySubletInterval {{{ */
static VALUE
RubySubletInterval(VALUE self)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  return s ? INT2FIX(s->interval) : Qnil;
} /* }}} */

/* RubySubletIntervalSet {{{ */
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
          case T_FIXNUM: s->interval = FIX2INT(value); return Qtrue;
          default: subSharedLogWarn("Unknown value type\n");
        }
    }

  return Qfalse;
} /* }}} */

#ifdef HAVE_SYS_INOTIFY_H
/* RubySubletPath {{{ */
static VALUE
RubySubletPath(VALUE self)
{
  SubSublet *s = NULL;

  Data_Get_Struct(self, SubSublet, s);

  return s ? rb_str_new2(s->path) : Qnil;
} /* }}} */

/* RubySubletPathSet {{{ */
static VALUE
RubySubletPathSet(VALUE self,
  VALUE value)
{
  SubSublet *s = NULL;
  char *watch = NULL;

  Data_Get_Struct(self, SubSublet, s);
  if(s)
    {
      switch(rb_type(value)) ///< Check value type
        {
          case T_STRING: 
            watch     = STR2CSTR(value);
            s->flags |= SUB_SUBLET_INOTIFY;
            s->path   = strdup(watch);

            /* Create inotify watch */
            if(0 < (s->interval = inotify_add_watch(subtle->notify, watch, IN_MODIFY)))
              {
                XSaveContext(subtle->dpy, subtle->windows.sublets, s->interval, (void *)s);

                subSharedLogDebug("Inotify: Adding watch on %s\n", watch); 

                return Qtrue;
              }

            subSharedLogWarn("Failed adding watch on file `%s': %s\n", 
              watch, strerror(errno));
            break;
          default: subSharedLogWarn("Failed handling unknown value type\n");
        }
    }

  return Qfalse;
} /* }}} */
#endif /* HAVE_SYS_INOTIFY_H */

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
  
  return Qnil != value ? STR2CSTR(value) : fallback;
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
  VALUE value = RubyGetValue(hash, key, T_TRUE, True);
  
  return Qtrue == value ? True : False; ///< We skip the fallback stuff here
} /* }}} */

/* RubyGetRect {{{ */
static void
RubyGetRect(VALUE hash,
  char *key,
  XRectangle *r)
{
  int data[4] = { 0 };
  VALUE ary = Qnil;

  if(Qnil != (ary = RubyGetValue(hash, key, T_ARRAY, False)))
    {
      int i;
      VALUE value = Qnil, meth = rb_intern("at");

      for(i = 0; i < 4; i++) ///< Safely fetching array values
        {
          value   = rb_funcall(ary, meth, 1, INT2FIX(i)) ;
          data[i] = RTEST(value) ? FIX2INT(value) : 0;
        }
    }

  /* Assign data to rect */
  r->x      = data[0]; ///< Left
  r->y      = data[1]; ///< Right
  r->width  = data[2]; ///< Top
  r->height = data[3]; ///< Bottom
} /* }}} */

/* RubyConfigForeach {{{ */
static int
RubyConfigForeach(VALUE key,
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
    { CHAR2SYM("WindowFloat"),        SUB_GRAB_WINDOW_TOGGLE,  SUB_STATE_FLOAT          }, 
    { CHAR2SYM("WindowFull"),         SUB_GRAB_WINDOW_TOGGLE,  SUB_STATE_FULL           }, 
    { CHAR2SYM("WindowStick"),        SUB_GRAB_WINDOW_TOGGLE,  SUB_STATE_STICK          }, 
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
              data = DATA(strdup(STR2CSTR(value)));
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
                } 
              break;
            case T_DATA: ///< Proc   
              type = SUB_GRAB_PROC;
              data = DATA(value);

              rb_ary_push(shelter, value); ///< Protect from GC
              break;
            default:
              subSharedLogWarn("Failed parsing grab `%s'\n", STR2CSTR(key));
              return Qnil;
          }

        if(-1 != type && (entry = (void *)subGrabNew(STR2CSTR(key), type, data)))
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
              if((entry = (void *)subTagNew(STR2CSTR(key), STR2CSTR(value), 0, 0, 0)))
                subArrayPush(subtle->tags, entry);
              break;
            case T_HASH:
              {
                int flags = 0, gravity = 0, screen = 0;
                char *regex = NULL;

                /* Fetch values */
                regex   = RubyGetString(value, "regex",   NULL);
                gravity = RubyGetFixnum(value, "gravity", 0);
                screen  = RubyGetFixnum(value, "screen",  0);

                /* Sanity */
                if(1 <= gravity && 9 >= gravity) flags |= SUB_TAG_GRAVITY;
                if(1 <= screen && screen <= subtle->screens->ndata)
                  {
                    screen -= 1;
                    flags |= SUB_TAG_SCREEN;
                  }

                /* Set property flags */
                if(True == RubyGetBool(value, "full"))  flags |= SUB_TAG_FULL;
                if(True == RubyGetBool(value, "float")) flags |= SUB_TAG_FLOAT;
                if(True == RubyGetBool(value, "stick")) flags |= SUB_TAG_STICK;

                if((entry = (void *)subTagNew(STR2CSTR(key), regex, flags, gravity, screen)))
                  subArrayPush(subtle->tags, entry);
                break;
              }
            default:
              subSharedLogWarn("Failed parsing tag `%s'\n", SYM2CHAR(key));
              return Qnil;
          }

        break; /* }}} */
      case SUB_TYPE_VIEW: /* {{{ */
        if((entry = (void *)subViewNew(STR2CSTR(key), STR2CSTR(value))))
          subArrayPush(subtle->views, entry);
        break; /* }}} */
      default: subSharedLogDebug("Never to be reached?!\n");
    }

  return Qnil;
} /* }}} */

/* RubyParseColor {{{ */
static unsigned long
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

/* RubyParseText {{{ */
static void
RubyParseText(char *string,
  SubArray *ary,
  int *width)
{
  int i = 0;
  SubText *t = NULL;
  unsigned long color = subtle->colors.fg_bar;
  char *tok = strtok(string, SEPARATOR);
  *width = 0;

  while(tok)
    {
      if(!strncmp(tok, "#", 1)) ///< Color
        {
          color = RubyParseColor(tok);
        }
      else ///< Recycle or re-use item to save allocs
        {
          if(i < ary->ndata)
            {
              t = TEXT(ary->data[i++]);

              if(t->flags & SUB_DATA_STRING && t->data.string) free(t->data.string);
            }
          else if((t = TEXT(subSharedMemoryAlloc(1, sizeof(SubText)))))
            {
              i++;
              subArrayPush(ary, t);
            }

          t->flags        = SUB_TYPE_TEXT|SUB_DATA_STRING;
          t->data.string  = strdup(tok);
          t->width        = XTextWidth(subtle->xfs, tok, strlen(tok) - 1) + 6; ///< Font offset
          t->color        = color;
          *width         += t->width;
        }

      tok = strtok(NULL, SEPARATOR);
    }
} /* }}} */

/* RubySubletData {{{ */
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
static VALUE
RubySubletDataSet(VALUE self,
  VALUE value)
{
  VALUE ret = Qfalse;
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);

  if(s)
    {
      switch(rb_type(value)) ///< Check value type
        {
          case T_STRING: 
            RubyParseText(RSTRING_PTR(value), s->text, &s->width); 
            ret = Qtrue;
            break;
          default:
            rb_raise(rb_eArgError, "Unknown value type");
        }
    }

  return ret;
} /* }}} */

/* RubySubtleTagAdd {{{ */
static VALUE
RubySubtleTagAdd(VALUE self,
  VALUE value)
{
  VALUE tag = Qnil;

  if(T_STRING == rb_type(value))
    {
      int tid = -1;
      SubTag *t = NULL;
      VALUE mod = Qnil, klass = Qnil;

      /* Create new tag */
      t = subTagNew(STR2CSTR(value), NULL, 0, 0, 0);
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
      int vid = -1;
      SubView *v = NULL;
      VALUE mod = Qnil, klass = Qnil;
      
      /* Create new view */
      v = subViewNew(STR2CSTR(value), NULL);
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
  else rb_raise(rb_eArgError, "Unknown value type");

  return view;
} /* }}} */

/* RubyFilter {{{ */
static inline int
RubyFilter(const struct dirent *entry)
{
  return !fnmatch("*.rb", entry->d_name, FNM_PATHNAME);
} /* }}} */

/* RubyDispatcher {{{ */
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

/* RubyWrapLoadConfig {{{ */
static VALUE
RubyWrapLoadConfig(VALUE data)
{
  int i, size = 0, state = 0;
  char *family = NULL, *style = NULL, buf[100];
  VALUE config = Qnil, entry = Qnil;

  /* Check path */
  if(!subtle->paths.config)
    {
      FILE *fd = NULL;

      snprintf(buf, sizeof(buf), "%s/%s/%s", getenv("XDG_CONFIG_HOME"), PKG_NAME, PKG_CONFIG);

      /* Check if config file exists */
      if(!(fd = fopen(buf, "r")))
        snprintf(buf, sizeof(buf), "%s/%s", DIR_CONFIG, PKG_CONFIG);
      else fclose(fd);
    }
  else snprintf(buf, sizeof(buf), "%s", subtle->paths.config);
  subSharedLogDebug("config=%s\n", buf);

  rb_load_protect(rb_str_new2(buf), 0, &state); ///< Load config
  if(state) RubyPerror(True, True, "Failed reading config `%s'", buf);

  if(!subtle || !subtle->dpy) return Qnil; ///< Exit after config check

  /* Config: Options */
  config          = rb_const_get(rb_cObject, rb_intern("OPTIONS"));
  subtle->bw      = RubyGetFixnum(config, "border",  2);
  subtle->step    = RubyGetFixnum(config, "step",    5);
  subtle->gravity = RubyGetFixnum(config, "gravity", 5);
  subtle->swap    = RubyGetBool(config, "swap");
  subtle->panel   = RubyGetBool(config, "panel");
  subtle->stipple = RubyGetBool(config, "stipple");
  RubyGetRect(config, "padding", &subtle->strut);

  /* Config: Font */
  config = rb_const_get(rb_cObject, rb_intern("FONT"));
  family = RubyGetString(config, "family", "fixed");
  style  = RubyGetString(config, "style",  "medium");
  size   = RubyGetFixnum(config, "size",    10);

  /* Config: Colors */
  config                   = rb_const_get(rb_cObject, rb_intern("COLORS"));
  subtle->colors.fg_bar    = RubyParseColor(RubyGetString(config, "fg_bar",        "#e2e2e5"));
  subtle->colors.fg_views  = RubyParseColor(RubyGetString(config, "fg_views",      "#CF6171"));
  subtle->colors.fg_focus  = RubyParseColor(RubyGetString(config, "fg_focus",      "#000000"));
  subtle->colors.bg_bar    = RubyParseColor(RubyGetString(config, "bg_bar",        "#444444"));
  subtle->colors.bg_views  = RubyParseColor(RubyGetString(config, "bg_views",      "#3d3d3d"));
  subtle->colors.bg_focus  = RubyParseColor(RubyGetString(config, "bg_focus",      "#CF6171"));
  subtle->colors.bo_focus  = RubyParseColor(RubyGetString(config, "border_focus",  "#CF6171"));
  subtle->colors.bo_normal = RubyParseColor(RubyGetString(config, "border_normal", "#CF6171"));
  subtle->colors.bg        = RubyParseColor(RubyGetString(config, "background",    "#3d3d3d3"));

  /* Config: Font */
  if(subtle->xfs) ///< Free in case of reload
    {
      XFreeFont(subtle->dpy, subtle->xfs);
      subtle->xfs = NULL;
    }

  snprintf(buf, sizeof(buf), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", family, style, size);
  if(!(subtle->xfs = XLoadQueryFont(subtle->dpy, buf)))
    {
      subSharedLogWarn("Failed loading font `%s' - falling back to fixed\n", family);
      subSharedLogDebug("Font: %s\n", buf);

      subtle->xfs = XLoadQueryFont(subtle->dpy, "-*-fixed-medium-*-*-*-13-*-*-*-*-*-*-*");
      if(!subtle->xfs) subSharedLogError("Failed loading font `fixed`\n");
    }

  subtle->fy = subtle->xfs->max_bounds.ascent + subtle->xfs->max_bounds.descent;
  subtle->th = subtle->fy + 2;

  /* Config: Grabs */
  config = rb_const_get(rb_cObject, rb_intern("GRABS"));
  rb_hash_foreach(config, RubyConfigForeach, SUB_TYPE_GRAB);

  if(0 == subtle->grabs->ndata)
    {
      subSharedLogWarn("No grabs found\n");
    }
  else subArraySort(subtle->grabs, subGrabCompare);

  /* Config: Tags */
  config = rb_const_get(rb_cObject, rb_intern("TAGS"));
  rb_hash_foreach(config, RubyConfigForeach, SUB_TYPE_TAG);

  if(1 == subtle->tags->ndata) ///< Excluding default tag
    {
      subSharedLogWarn("No tags found\n");
    }
  else subTagPublish();

  /* Config: Views */
  config = rb_const_get(rb_cObject, rb_intern("VIEWS"));
  switch(rb_type(config)) ///< Allow hashes or arrays for ordering
    {
      case T_HASH: rb_hash_foreach(config, RubyConfigForeach, SUB_TYPE_VIEW); break;
      case T_ARRAY:
        for(i = 0; Qnil != (entry = rb_ary_entry(config, i)); ++i)
          {
            if(T_HASH == rb_type(entry))
              rb_hash_foreach(entry, RubyConfigForeach, SUB_TYPE_VIEW);
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
      int def = -1;
      SubView *v = NULL;

      /* Check for view with default tag */
      for(i = 0; i < subtle->views->ndata; i++)
        if((v = VIEW(subtle->views->data[i])) && v->tags & SUB_TAG_DEFAULT)
          {
            def = i;
            subSharedLogDebug("Default view: name=%s, id=%d\n", v->name, def);
            break;
          }

      if(-1 == def) ///< Add default tag to first view
        {
          v = VIEW(subtle->views->data[0]);
          v->tags |= SUB_TAG_DEFAULT;
          subEwmhSetCardinals(v->button, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1);
        }
    }

  subViewUpdate();
  subViewPublish();

  /* Config: Hooks */
  config = rb_const_get(rb_cObject, rb_intern("HOOKS"));
  rb_hash_foreach(config, RubyConfigForeach, SUB_TYPE_HOOK);

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

  signal(SIGALRM, RubySignal); ///< Limit execution time
  alarm(EXECTIME);

  if((int)rargs[0] & SUB_TYPE_SUBLET) /* {{{ */
    {
      ret = rb_funcall(rargs[1], rb_intern("run"), 0, NULL) ;
    } /* }}} */
  else if((int)rargs[0] & (SUB_TYPE_GRAB|SUB_TYPE_HOOK)) /* {{{ */
    {
      int id = 0, arity = 0;

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
                    inst  = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(c->name));

                    rb_iv_set(inst, "@id",      INT2FIX(id));
                    rb_iv_set(inst, "@win",     LONG2NUM(c->win));
                    rb_iv_set(inst, "@klass",   rb_str_new2(c->klass));
                    rb_iv_set(inst, "@x",       INT2FIX(c->geom.x));
                    rb_iv_set(inst, "@y",       INT2FIX(c->geom.y));
                    rb_iv_set(inst, "@width",   INT2FIX(c->geom.width));
                    rb_iv_set(inst, "@height",  INT2FIX(c->geom.height));
                    rb_iv_set(inst, "@gravity", INT2FIX(c->gravity));
                    rb_iv_set(inst, "@screen",  INT2FIX(c->screen + 1));
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

  alarm(0);

  return ret;
} /* }}} */

/* RubyWrapRemove {{{ */
static VALUE
RubyWrapRemove(VALUE name)
{
  VALUE object = rb_const_get(rb_mKernel, rb_intern("Object"));

  return rb_funcall(object, rb_intern("send"), 2, rb_str_new2("remove_const"), name);
} /* }}} */

 /** subRubyInit {{{
  * @brief Init ruby
  **/

void
subRubyInit(void)
{
  VALUE mod = Qnil, klass = Qnil;

  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

  rb_define_method(rb_mKernel, "method_missing", RubyDispatcher, -1); ///< Subtlext dispatcher

  /* Module: subtle */
  mod = rb_define_module("Subtle");

  /* Class: sublet */
  klass = rb_define_class_under(mod, "Sublet", rb_cObject);
  rb_define_singleton_method(klass, "new",       RubySubletNew,       0);
  rb_define_singleton_method(klass, "inherited", RubySubletInherited, 1);

  rb_define_method(klass, "color",     RubySubletColor,       1);
  rb_define_method(klass, "interval",  RubySubletInterval,    0);
  rb_define_method(klass, "interval=", RubySubletIntervalSet, 1);
  rb_define_method(klass, "data",      RubySubletData,        0);
  rb_define_method(klass, "data=",     RubySubletDataSet,     1);

#ifdef HAVE_SYS_INOTIFY_H
  rb_define_method(klass, "path",      RubySubletPath,    0);
  rb_define_method(klass, "path=",     RubySubletPathSet, 1);
#endif /* HAVE_SYS_INOTIFY */

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

  rb_protect(RubyWrapLoadConfig, Qnil, &state);

  if(state) RubyPerror(True, True, "Failed reading config");
} /* }}} */

 /** subRubyLoadSublet {{{
  * @brief Load sublets from path
  * @param[in]  path  Path of the sublets
  **/

void
subRubyLoadSublet(const char *file)
{
  int state = 0;
  char path[100];

  /* Check path */
  if(!subtle->paths.sublets)
    snprintf(path, sizeof(path), "%s/%s/sublets/%s", getenv("XDG_DATA_HOME"), PKG_NAME, file);
  else snprintf(path, sizeof(path), "%s/%s", subtle->paths.sublets, file);
  subSharedLogDebug("path=%s\n", path);

  /* Load sublet */
  rb_load_protect(rb_str_new2(path), 0, &state); ///< Load sublet

  if(0 == state && Qnil != sublet)
    {
      VALUE self = Qnil;
      char *name = (char *)rb_class2name((ID)sublet);

      /* Instantiate sublet */
      if((self = rb_funcall(sublet, rb_intern("new"), 0, NULL)))
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

          subRubyCall(SUB_TYPE_SUBLET, s->recv, NULL); ///< First run

          printf("Loaded sublet (%s)\n", name);
        }
      else subSharedLogWarn("Failed instantiating sublet `%s'\n", name);

      sublet = Qnil;
    }
  else RubyPerror(True, False, "Failed loading sublet `%s'", path);
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
  if(!subtle->paths.sublets)
    snprintf(buf, sizeof(buf), "%s/%s/sublets", getenv("XDG_DATA_HOME"), PKG_NAME);
  else snprintf(buf, sizeof(buf), "%s", subtle->paths.sublets);
  subSharedLogDebug("path=%s\n", buf);

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

  result = rb_protect(RubyWrapCall, (VALUE)&rargs, &state);

  if(state) 
    {
      RubyPerror(True, False, "Failed calling %s",
        type & SUB_TYPE_SUBLET ? "sublet" : (type & SUB_TYPE_GRAB ? "grab" : "hook"));
      result = Qnil;

      alarm(0); ///< Reset alarm
    }

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
