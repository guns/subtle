
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

static VALUE shelter = Qnil, sublets = Qnil, subtlext = Qnil; ///< Globals

/* Typedef {{{ */
typedef struct rubygrabs_t
{
  char          *key;
  int           value;
  unsigned long extra;
} RubyGrabs;

typedef struct rubyhooks_t
{
  char          *key;
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

  lineno  = rb_gv_get("$.");
  lasterr = rb_gv_get("$!");
  message = rb_obj_as_string(lasterr);

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if(True == verbose && RTEST(message)) subSharedLogWarn("%s\n\n", RSTRING(message)->ptr);
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

  /* Create sublet */
  s = subSubletNew();
  data    = Data_Wrap_Struct(self, RubySubletMark, NULL, (void *)s);
  s->recv = data; 
  s->name = strdup(rb_class2name(self));

  rb_obj_call_init(data, 0, NULL); ///< Call initialize
  rb_ary_push(shelter, data); ///< Protect from GC
  if(0 == s->interval) s->interval = 60; ///< Sanitize

  subArrayPush(subtle->sublets, s);

  return data;
} /* }}} */

/* RubySubletInherited {{{ */
static VALUE
RubySubletInherited(VALUE self,
  VALUE recv)
{
  rb_ary_push(sublets, recv); ///< Save the sublet

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
  Data_Get_Struct(self, SubSublet, s);

  if(s)
    {
      switch(rb_type(value)) ///< Check value type
        {
          case T_STRING: 
            if(RTEST(value))
              {
                char *watch = STR2CSTR(value);

                /* Create inotify watch */
                if(0 < (s->interval = inotify_add_watch(subtle->notify, watch, IN_MODIFY)))
                  {
                    s->flags |= SUB_SUBLET_INOTIFY;
                    s->path   = strdup(watch);
                    XSaveContext(subtle->disp, subtle->windows.sublets, s->interval, (void *)s);

                    return Qtrue;
                  }

                subSharedLogWarn("Failed watching file `%s': %s\n", watch, strerror(errno));
              }
            break;
          default: subSharedLogWarn("Failed handling unknown value type\n");
        }
    }

  return Qfalse;
} /* }}} */
#endif /* HAVE_SYS_INOTIFY_H */

/* RubyGetString {{{ */
static char *
RubyGetString(VALUE hash,
  char *key,
  char *fallback)
{
  VALUE value = Qundef;
  
  if(Qundef == (value = rb_funcall_rescue(hash, rb_intern("fetch"), 
    1, rb_str_new2(key)))) RubyPerror(False, True, "Failed fetching config key `%s'", key);

  return T_STRING == rb_type(value) ? STR2CSTR(value) : fallback;
} /* }}} */

/* RubyGetFixnum {{{ */
static int
RubyGetFixnum(VALUE hash,
  char *key,
  int fallback)
{
  VALUE value = Qundef;
  
  if(Qundef == (value = rb_funcall_rescue(hash, rb_intern("fetch"), 
    1, rb_str_new2(key)))) RubyPerror(False, True, "Failed fetching config key `%s'", key);

  return T_FIXNUM == rb_type(value) ? FIX2INT(value) : fallback;
} /* }}} */

/* RubyGetRect {{{ */
static void
RubyGetRect(VALUE hash,
  char *key,
  XRectangle *strut)
{
  int i, data[4] = { 0 };
  VALUE ary = Qundef, value = Qundef;
  
  if(Qundef == (ary = rb_funcall_rescue(hash, rb_intern("fetch"), 
    1, rb_str_new2(key)))) RubyPerror(False, True, "Failed fetching config key `%s'", key);

  if(T_ARRAY == rb_type(ary)) 
    {
      for(i = 0; i < 4; i++) ///< Safely fetching array values
        {
          if(Qundef != (value = rb_funcall_rescue(ary, rb_intern("at"), 1, INT2FIX(i))) && 
            Qnil != value) data[i] = FIX2INT(value);
          else RubyPerror(False, True, "Failed fetching array index `%d'", i + 1);
        }
    }

  strut->x      = data[0]; ///< Left
  strut->y      = data[1]; ///< Right
  strut->width  = data[2]; ///< Top
  strut->height = data[3]; ///< Bottom
} /* }}} */

/* RubyConfigForeach {{{ */
static int
RubyConfigForeach(VALUE key,
  VALUE value,
  VALUE extra)
{
  int i, type = -1;
  void *entry = NULL;
  char *name = NULL;
  SubData data = { None };

  static const RubyGrabs grabs[] = /* {{{ */
  {
    { "SubtleReload",       SUB_GRAB_SUBTLE_RELOAD,  None                     }, ///< 0
    { "SubtleQuit",         SUB_GRAB_SUBTLE_QUIT,    None                     }, 
    { "WindowMove",         SUB_GRAB_WINDOW_MOVE,    None                     }, ///< 2
    { "WindowResize",       SUB_GRAB_WINDOW_RESIZE,  None                     }, 
    { "WindowFloat",        SUB_GRAB_WINDOW_TOGGLE,  SUB_STATE_FLOAT          }, 
    { "WindowFull",         SUB_GRAB_WINDOW_TOGGLE,  SUB_STATE_FULL           }, 
    { "WindowStick",        SUB_GRAB_WINDOW_TOGGLE,  SUB_STATE_STICK          }, 
    { "WindowRaise",        SUB_GRAB_WINDOW_STACK,   Above                    }, 
    { "WindowLower",        SUB_GRAB_WINDOW_STACK,   Below                    }, 
    { "WindowLeft",         SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_LEFT          }, 
    { "WindowDown",         SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_DOWN          }, 
    { "WindowUp",           SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_UP            }, 
    { "WindowRight",        SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_RIGHT         }, 
    { "WindowKill",         SUB_GRAB_WINDOW_KILL,    None                     }, ///< 13
    { "GravityTopLeft",     SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP_LEFT     }, 
    { "GravityTopRight",    SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP_RIGHT    }, 
    { "GravityTop",         SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP          }, 
    { "GravityLeft",        SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_LEFT         }, 
    { "GravityCenter",      SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_CENTER       }, 
    { "GravityRight",       SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_RIGHT        }, 
    { "GravityBottomLeft",  SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM_LEFT  }, 
    { "GravityBottomRight", SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM_RIGHT },
    { "GravityBottom",      SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM       } ///< 22
  }; /* }}} */

  RubyHooks hooks[] = /* {{{ */
  {
    { "HookJump",      &subtle->hooks.jump      }, ///< 0
    { "HookConfigure", &subtle->hooks.configure },
    { "HookCreate",    &subtle->hooks.create    },
    { "HookFocus",     &subtle->hooks.focus     },
    { "HookGravity",   &subtle->hooks.gravity   }  ///< 4
  }; /* }}} */

  /* Create various types */
  switch(extra)
    {
      case SUB_TYPE_GRAB: /* {{{ */
        switch(rb_type(value)) ///< Check value type
          {
            case T_STRING:
              name = STR2CSTR(value);

              if(!strncmp(name, "ViewJump", 8)) ///< ViewJump
                {
                  if((name = (char *)name + 8))
                    {
                      type = SUB_GRAB_JUMP;
                      data = DATA((unsigned long)(atol(name) - 1));
                    }
                }
              else if(!strncmp(name, "Subtle", 6)) ///< Subtle
                {
                  for(i = 0; i < 2; i++)
                    if(!strcmp((char *)name, grabs[i].key))
                      {
                        type = grabs[i].value;
                        data = DATA(grabs[i].extra);
                      }
                }
              else if(!strncmp(name, "Window", 6)) ///< Window
                {
                  for(i = 2; i <= 13; i++)
                    if(!strcmp((char *)name, grabs[i].key))
                      {
                        type = grabs[i].value;
                        data = DATA(grabs[i].extra);
                      }
                }    
              else if(!strncmp(name, "Gravity", 7)) ///< Gravity
                {
                  size_t len = strlen(name) - 4;

                  for(i = 14; i <= 22; i++)
                    if(!strncmp((char *)name, grabs[i].key, len))
                      {
                        char *kind = name + len; ///< Get suffix

                        type = grabs[i].value;
                        data = DATA(grabs[i].extra);

                        /* Check horz/vert */
                        if(!strcmp(kind, "Horz")) data.num |= SUB_GRAVITY_HORZ;
                        else if(!strcmp(kind, "Vert")) data.num |= SUB_GRAVITY_VERT;
                      }
                }
              else ///< Exec
                {
                  type = SUB_GRAB_EXEC;
                  data = DATA(strdup(name));
                }
              break;
            case T_DATA: ///< Proc   
              type = SUB_GRAB_PROC;
              data = DATA(value);

              rb_ary_push(shelter, value); ///< Protect from GC
              break;
            default:
              subSharedLogWarn("Failed executing block `%s'\n", STR2CSTR(key));
              return Qnil;
          }

        if((entry = (void *)subGrabNew(STR2CSTR(key), type, data)) && -1 != type)
          subArrayPush(subtle->grabs, entry);
        break; /* }}} */
      case SUB_TYPE_HOOK: /* {{{ */
        if(T_STRING == rb_type(key) && T_DATA == rb_type(value)) ///< Check value types
          {
            name = STR2CSTR(key);
            if(!strncmp(name, "Hook", 4))
              {
                for(i = 0; i < LENGTH(hooks); i++)
                  if(!strcmp((char *)name, hooks[i].key))
                    {
                      *hooks[i].hook = value; ///< Store proc
                      rb_ary_push(shelter, value); ///< Protect from GC
                      
                      subSharedLogDebug("hook=%s\n", hooks[i].key);
                    }
              }        
          }
        break; /* }}} */
      case SUB_TYPE_TAG: /* {{{ */
        if((entry = (void *)subTagNew(STR2CSTR(key), STR2CSTR(value))))
          subArrayPush(subtle->tags, entry);
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
  XColor color = { subtle->colors.norm }; ///< Default color

  /* Parse and store color */
  if(!XParseColor(subtle->disp, COLORMAP, name, &color))
    {
      subSharedLogWarn("Failed loading color `%s'\n", name);
    }
  else if(!XAllocColor(subtle->disp, COLORMAP, &color))
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
            subArrayPush(ary, t);

          t->flags        = SUB_TYPE_TEXT|SUB_DATA_STRING;
          t->data.string  = strdup(tok);
          t->width        = XTextWidth(subtle->xfs, tok, strlen(tok)) + 6; ///< Font offset
          t->color        = color;
          *width          += t->width;
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
      t = subTagNew(STR2CSTR(value), NULL);
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

/* RubyRequire {{{ */
static VALUE
RubyRequire(VALUE path)
{
  rb_require(STR2CSTR(path)); 

  return Qnil;
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

/* RubyInstantiate {{{ */
static VALUE
RubyInstantiate(VALUE idx,
  VALUE *obj)
{
  char *name = NULL;
  
  /* Instantiate sublets */
  name = (char *)rb_class2name((ID)idx);
  if(!rb_funcall(idx, rb_intern("new"), 0, NULL))
    {
      subSharedLogWarn("Failed loading sublet `%s'\n", name);
    }
  else printf("Loaded sublet (%s)\n", name);

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
  * @param[in]  file  Config file
  **/

void
subRubyLoadConfig(const char *file)
{
  int state = 0, size = 0;
  char *family = NULL, *style = NULL, buf[100];
  VALUE config = Qnil;
  FILE *fd = NULL;

  /* Check path */
  if(!file)
    {
      snprintf(buf, sizeof(buf), "%s/%s/%s",
        getenv("XDG_CONFIG_HOME"), PKG_NAME, PKG_CONFIG);
      if(!(fd = fopen(buf, "r")))
        {
          snprintf(buf, sizeof(buf), "%s/%s", DIR_CONFIG, PKG_CONFIG);
        }
      else fclose(fd);
    }
  else snprintf(buf, sizeof(buf), "%s", file);
  subSharedLogDebug("config=%s\n", buf);

  /* Safety first */
  rb_load_protect(rb_str_new2(buf), 0, &state);
  if(state) RubyPerror(True, True, "Failed reading config `%s'", buf);

  if(!subtle || !subtle->disp) return; ///< Exit after config check

  /* Config: Options */
  config = rb_const_get(rb_cObject, rb_intern("OPTIONS"));
  subtle->bw      = RubyGetFixnum(config, "border",  2);
  subtle->step    = RubyGetFixnum(config, "step",    5);
  subtle->bar     = RubyGetFixnum(config, "bar",     0);
  subtle->gravity = RubyGetFixnum(config, "gravity", 5);
  RubyGetRect(config, "padding", &subtle->strut);

  /* Config: Font */
  config = rb_const_get(rb_cObject, rb_intern("FONT"));
  family = RubyGetString(config, "family",  "fixed");
  style  = RubyGetString(config, "style",   "medium");
  size   = RubyGetFixnum(config, "size",    12);

  /* Config: Colors */
  config                = rb_const_get(rb_cObject, rb_intern("COLORS"));
  subtle->colors.fg_bar   = RubyParseColor(RubyGetString(config, "fg_bar",     "#e2e2e5"));
  subtle->colors.bg_bar   = RubyParseColor(RubyGetString(config, "bg_bar",     "#444444"));
  subtle->colors.fg_focus = RubyParseColor(RubyGetString(config, "fg_focus",   "#000000"));
  subtle->colors.bg_focus = RubyParseColor(RubyGetString(config, "bg_focus",   "#bdbabd"));
  subtle->colors.norm     = RubyParseColor(RubyGetString(config, "normal",     "#22aa99"));
  subtle->colors.bg       = RubyParseColor(RubyGetString(config, "background", "#336699"));

  /* Config: Font */
  if(subtle->xfs) ///< Free in case of reload
    {
      XFreeFont(subtle->disp, subtle->xfs);
      subtle->xfs = NULL;
    }

  snprintf(buf, sizeof(buf), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", family, style, size);
  if(!(subtle->xfs = XLoadQueryFont(subtle->disp, buf)))
    {
      subSharedLogWarn("Failed loading font `%s' - falling back to fixed\n", family);
      subSharedLogDebug("Font: %s\n", buf);

      subtle->xfs = XLoadQueryFont(subtle->disp, "-*-fixed-medium-*-*-*-13-*-*-*-*-*-*-*");
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

  if(SUB_TAG_CLEAR == subtle->tags->ndata)
    {
      subSharedLogWarn("No tags found\n");
    }
  else subTagPublish();

  /* Config: Views */
  config = rb_const_get(rb_cObject, rb_intern("VIEWS"));
  rb_hash_foreach(config, RubyConfigForeach, SUB_TYPE_VIEW);

  /* Views */
  if(0 == subtle->views->ndata) ///< Create default view
    {
      SubView *v = subViewNew("subtle", "default");

      subArrayPush(subtle->views, (void *)v);
      subSharedLogWarn("No views found\n");
    }
  else ///< Check default tag
    {
      int i, def = -1;
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
} /* }}} */

 /** subRubyLoadSublets {{{
  * @brief Load sublets from path
  * @param[in]  path  Path of the sublets
  **/

void
subRubyLoadSublets(const char *path)
{
  int i, num;
  DIR *dir = NULL;
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
  if(!path)
    {
      snprintf(buf, sizeof(buf), "%s/%s/sublets", getenv("XDG_DATA_HOME"), PKG_NAME);
      if(!(dir = opendir(buf))) 
        {
          subSharedLogWarn("No sublets found\n");

          subSubletUpdate();
          subSubletPublish();

          return;
        }
      closedir(dir);
    }
  else snprintf(buf, sizeof(buf), "%s", path);
  subSharedLogDebug("path=%s\n", buf);

  /* Scan directory */
  if(0 < ((num = scandir(buf, &entries, RubyFilter, alphasort))))
    {
      sublets = rb_ary_new();

      for(i = 0; i < num; i++)
        {
          int state = 0;
          char file[150];

          snprintf(file, sizeof(file), "%s/%s", buf, entries[i]->d_name);
          subSharedLogDebug("path=%s\n", file);

          /* Safety first */
          rb_load_protect(rb_str_new2(file), 0, &state); ///< Load sublet
          if(state) RubyPerror(True, False, "Failed loading sublet `%s'", entries[i]->d_name);

          free(entries[i]);
        }
      free(entries);

      rb_iterate(rb_each, sublets, RubyInstantiate, Qnil); ///< Instantiate sublets

      if(0 < subtle->sublets->ndata)
        {
          /* Preserve load order */
          for(i = 0; i < subtle->sublets->ndata; i++)
            {
              if(i < subtle->sublets->ndata - 1) ///< Range check
                SUBLET(subtle->sublets->data[i])->next = SUBLET(subtle->sublets->data[i + 1]);
              subRubyCall(SUB_TYPE_SUBLET, SUBLET(subtle->sublets->data[i])->recv, NULL); ///< First run
            }

          subtle->sublet = SUBLET(subtle->sublets->data[0]);
          subArraySort(subtle->sublets, subSubletCompare); ///< Initially sort
        }
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
  VALUE mod = Qnil, klass = Qnil;

  /* Load subtlext */
  rb_protect(RubyRequire, rb_str_new2("subtle/subtlext"), &state);
  if(state) RubyPerror(True, True, "Failed loading subtlext");

  /* Module: subtlext */
  mod = rb_const_get(rb_mKernel, rb_intern("Subtlext"));

  /* Class: subtle */
  klass    = rb_const_get(mod, rb_intern("Subtle"));
  subtlext = rb_funcall(klass, rb_intern("new2"), 1, (VALUE)subtle->disp);

  /* @todo Overwrite methods to bypass timing problems */
  rb_define_method(klass, "add_tag",  RubySubtleTagAdd,  1);
  rb_define_method(klass, "add_view", RubySubtleViewAdd, 1);

  rb_ary_push(shelter, subtlext); ///< Protect from GC

  printf("Loaded subtlext\n");
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
  int ret = 1;
  VALUE value = Qundef;

  signal(SIGALRM, RubySignal); ///< Limit execution time
  alarm(EXECTIME);

  if(type & SUB_TYPE_SUBLET) /* {{{ */
    {
      if(Qundef == (value = rb_funcall_rescue(recv, rb_intern("run"), 0, NULL))) 
        {
          RubyPerror(True, False, "Failed calling sublet");
          ret = - 1;
        }
    } /* }}} */
  else if(type & (SUB_TYPE_GRAB|SUB_TYPE_HOOK)) /* {{{ */
    {
      int id = 0, arity = 0;
      char *kind = type & SUB_TYPE_GRAB ? "grab" : "hook";

      /* Get arity of proc */
      if(Qundef == (value = rb_funcall_rescue(recv, rb_intern("arity"), 0, NULL))) 
        RubyPerror(True, True, "Failed running %s", kind);

      arity = FIX2INT(value);

      /* Check proc arity */
      switch(arity)
        {
          case 0:
          case -1: ///< No optional arguments 
            if(Qundef == (value = rb_funcall_rescue(recv, rb_intern("call"), 0, NULL))) 
              {
                RubyPerror(True, False, "Failed calling %s", kind);
                ret = -1;
              }
            break;
          case 1: ///< One argument
            if(extra)
              {
                SubClient *c = CLIENT(extra);
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
                    rb_iv_set(inst, "@gravity", INT2FIX(c->gravity));
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

                if(Qundef == (value = rb_funcall_rescue(recv, rb_intern("call"), 1, inst)))
                  {
                    RubyPerror(True, False, "Failed calling %s", kind);
                    ret = -1;
                  }
                else ret = Qfalse == value ? 0 : 1; ///< Check return value

                break;
              } ///< Fall through
          default:
            rb_raise(rb_eStandardError, "Failed calling proc with `%d' argument(s)", arity);
            ret = -1;
        }
      subSharedLogDebug("Proc: arity=%d\n", arity);      
    } /* }}} */

  alarm(0);

  return ret;
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
