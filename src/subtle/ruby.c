
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
typedef struct rubytable_t
{
  char *key;
  int value;
  unsigned long extra;
} RubyTable;
/* }}} */

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
  VALUE value = rb_funcall(hash, rb_intern("fetch"), 1, rb_str_new2(key));
 
  if(T_STRING == rb_type(value)) return STR2CSTR(value);

  return fallback;
} /* }}} */

/* RubyGetFixnum {{{ */
static int
RubyGetFixnum(VALUE hash,
  char *key,
  int fallback)
{
  VALUE value = rb_funcall(hash, rb_intern("fetch"), 1, rb_str_new2(key));

  if(T_FIXNUM == rb_type(value)) return FIX2INT(value);

  return fallback;
} /* }}} */

/* RubyGetArray {{{ */
static int
RubyGetArray(VALUE hash,
  char *key,
  int idx,
  int fallback)
{
  VALUE ary = rb_funcall(hash, rb_intern("fetch"), 1, rb_str_new2(key));

  if(T_ARRAY == rb_type(ary)) 
    if(idx <= FIX2INT(rb_funcall(ary, rb_intern("length"), 0, Qnil)) - 1)
      {
        return FIX2INT(rb_funcall(ary, rb_intern("at"), 1, INT2FIX(idx)));
      }

  return fallback;
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

  static const RubyTable table[] = /* {{{ */
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
    { "WindowUp",           SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_UP            }, 
    { "WindowLeft",         SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_LEFT          }, 
    { "WindowRight",        SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_RIGHT         }, 
    { "WindowDown",         SUB_GRAB_WINDOW_SELECT,  SUB_WINDOW_DOWN          }, 
    { "WindowKill",         SUB_GRAB_WINDOW_KILL,    None                     }, ///< 13
    { "GravityTopLeft",     SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP_LEFT     }, 
    { "GravityTopRight",    SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP_RIGHT    }, 
    { "GravityTop",         SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_TOP          }, 
    { "GravityLeft",        SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_LEFT         }, 
    { "GravityCenter",      SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_CENTER       }, 
    { "GravityRight",       SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_RIGHT        }, 
    { "GravityBottomLeft",  SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM_LEFT  }, 
    { "GravityBottomRight", SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM_RIGHT },
    { "GravityBottom",      SUB_GRAB_WINDOW_GRAVITY, SUB_GRAVITY_BOTTOM       }   ///< 22

  }; /* }}} */  

  /* Create various types */
  switch(extra)
    {
      case SUB_TYPE_GRAB:
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
              else if(!strncmp(name, "Subtle", 6)) ///< ViewJump
                {
                  for(i = 0; i < 2; i++)
                    if(!strcmp((char *)name, table[i].key))
                      {
                        type = table[i].value;
                        data = DATA(table[i].extra);
                      }
                }
              else if(!strncmp(name, "Window", 6)) ///< Window
                {
                  for(i = 2; i <= 13; i++)
                    if(!strcmp((char *)name, table[i].key))
                      {
                        type = table[i].value;
                        data = DATA(table[i].extra);
                      }
                }    
              else if(!strncmp(name, "Gravity", 7)) ///< Gravity
                {
                  size_t len = strlen(name) - 4;

                  for(i = 14; i <= 22; i++)
                    if(!strncmp((char *)name, table[i].key, len))
                      {
                        char *kind = name + len; ///< Get suffix

                        type = table[i].value;
                        data = DATA(table[i].extra);

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
        break;
      case SUB_TYPE_TAG:
        if((entry = (void *)subTagNew(STR2CSTR(key), STR2CSTR(value))))
          subArrayPush(subtle->tags, entry);
        break;
      case SUB_TYPE_VIEW:
        if((entry = (void *)subViewNew(STR2CSTR(key), STR2CSTR(value))))
          subArrayPush(subtle->views, entry);
        break;
      default: subSharedLogDebug("Never to be reached?!\n");
    }

  return Qnil;
} /* }}} */

/* RubyParseColor {{{ */
static unsigned long
RubyParseColor(char *name)
{
  XColor color = { subtle->colors.font }; ///< Default color

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
  unsigned long color = subtle->colors.font;
  char *tok = strtok(string, SEPARTOR);
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

      tok = strtok(NULL, SEPARTOR);
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

/* RubyPerror {{{ */
static VALUE
RubyPerror(const char *msg,
  int fatal)
{
  VALUE lasterr = Qnil, message = Qnil;

  /* Get last error message */
  lasterr = rb_gv_get("$!");
  message = rb_obj_as_string(lasterr);

  if(RTEST(message))
    {
      if(True == fatal)
        {
          subSharedLogError("%s:\n %s\n", msg, RSTRING(message)->ptr);
        }
      else subSharedLogWarn("%s: %s\n", msg, RSTRING(message)->ptr);
    }

  return Qnil;
} /* }}} */

/* RubyFilter {{{ */
static inline int
RubyFilter(const struct dirent *entry)
{
  return !fnmatch("*.rb", entry->d_name, FNM_PATHNAME);
} /* }}} */

/* RubyProtect {{{ */
static VALUE
RubyProtect(VALUE script)
{
  SubSublet *s = NULL;
  VALUE result = Qnil;

  assert(script);

  s = SUBLET(script);

  if(s->flags & SUB_TYPE_SUBLET)
    result = rb_funcall(SUBLET(s)->recv, rb_intern("run"), 0, NULL);
  else if(s->flags & SUB_TYPE_GRAB)
    {
      int id = 0, arity = 0;
      VALUE mod = Qnil, klass = Qnil, client = Qnil;

      /* Catch arity errors */
      switch((arity = FIX2INT(rb_funcall(GRAB(s)->data.num, rb_intern("arity"), 0, NULL))))
        {
          case 0:
          case -1: ///< No optional arguments 
            result = rb_funcall(GRAB(s)->data.num, rb_intern("call"), 0, NULL);
            break;
          case 1:
            if(subtle->cc)
              {
                if(Qnil == subtlext) subRubyLoadSubtlext(); ///< Load on demand

                /* Create client instance */
                id     = subArrayIndex(subtle->clients, (void *)subtle->cc);
                mod    = rb_const_get(rb_mKernel, rb_intern("Subtlext"));
                klass  = rb_const_get(mod, rb_intern("Client"));
                client = rb_funcall(klass, rb_intern("new"), 1, rb_str_new2(subtle->cc->name));

                rb_iv_set(client, "@id",      INT2FIX(id));
                rb_iv_set(client, "@win",     LONG2NUM(subtle->cc->win));
                rb_iv_set(client, "@gravity", INT2FIX(subtle->cc->gravity));

                result = rb_funcall(GRAB(s)->data.num, rb_intern("call"), 1, client);
                break;
              } ///< Fall through
          default:
            rb_raise(rb_eStandardError, "Failed calling proc with `%d' argument(s)", arity);
        }
      subSharedLogDebug("Proc: arity=%d\n", arity);
    }

  return result;
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

  if(Qnil == subtlext) subRubyLoadSubtlext(); ///< Load on demand

  if(rb_respond_to(subtlext, rb_to_id(missing))) ///< Dispatch method calls
    return rb_funcall2(subtlext, rb_to_id(missing), --argc, ++argv);
  
  rb_raise(rb_eStandardError, "Failed finding method `%s'", name);
  return Qnil;  
} /* }}} */

/* RubyEach {{{ */
static VALUE
RubyEach(VALUE idx,
  VALUE *obj)
{
  char *name = NULL;
  
  /* Instantiate sublets */
  name = (char *)rb_class2name((ID)idx);
  if(!rb_funcall(idx, rb_intern("new"), 0, NULL))
    {
      subSharedLogWarn("Failed loading sublet `%s'\n", name);
    }
  else printf("Loading sublet (%s)\n", name);

  return Qnil;
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
  if(state) RubyPerror("Failed reading config", True);

  if(!subtle || !subtle->disp) return; ///< Exit after config check

  /* Config: Options */
  config = rb_const_get(rb_cObject, rb_intern("OPTIONS"));
  subtle->bw           = RubyGetFixnum(config, "border", 2);
  subtle->step         = RubyGetFixnum(config, "step", 5);
  subtle->bar          = RubyGetFixnum(config, "bar", 0);
  subtle->strut.x      = RubyGetArray(config, "padding", 0, 0);
  subtle->strut.y      = RubyGetArray(config, "padding", 1, 0);
  subtle->strut.width  = RubyGetArray(config, "padding", 2, 0);
  subtle->strut.height = RubyGetArray(config, "padding", 3, 0);

  /* Config: Font */
  config = rb_const_get(rb_cObject, rb_intern("FONT"));
  family = RubyGetString(config, "family",  "fixed");
  style  = RubyGetString(config, "style", "medium");
  size   = RubyGetFixnum(config, "size",  12);

  /* Config: Colors */
  config                = rb_const_get(rb_cObject, rb_intern("COLORS"));
  subtle->colors.border = RubyParseColor(RubyGetString(config, "border",     "#bdbabd"));
  subtle->colors.norm   = RubyParseColor(RubyGetString(config, "normal",     "#22aa99"));
  subtle->colors.focus  = RubyParseColor(RubyGetString(config, "focus",      "#ffa500"));
  subtle->colors.bg     = RubyParseColor(RubyGetString(config, "background", "#336699"));
  subtle->colors.font   = RubyParseColor(RubyGetString(config, "font",       "#000000"));

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
            subSharedLogDebug("Default view: id=%d\n", i);
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
          if(state) RubyPerror("Failed loading sublet", False);

          free(entries[i]);
        }
      free(entries);

      rb_iterate(rb_each, sublets, RubyEach, Qnil); ///< Instantiate sublets

      if(0 < subtle->sublets->ndata)
        {
          /* Preserve load order */
          for(i = 0; i < subtle->sublets->ndata - 1; i++)
            {
              SUBLET(subtle->sublets->data[i])->next = SUBLET(subtle->sublets->data[i + 1]);
              subRubyRun(SUBLET(subtle->sublets->data[i])); ///< First run
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

  printf("Loading subtlext\n");

  /* Load subtlext */
  rb_protect(RubyRequire, rb_str_new2("subtle/subtlext"), &state);
  if(state) RubyPerror("Failed loading subtlext", True);

  /* Module: subtlext */
  mod = rb_const_get(rb_mKernel, rb_intern("Subtlext"));

  /* Class: subtle */
  klass    = rb_const_get(mod, rb_intern("Subtle"));
  subtlext = rb_funcall(klass, rb_intern("new2"), 1, (VALUE)subtle->disp);

  /* @todo Overwrite methods to bypass timing problems */
  rb_define_method(klass, "add_tag",  RubySubtleTagAdd,  1);
  rb_define_method(klass, "add_view", RubySubtleViewAdd, 1);

  rb_ary_push(shelter, subtlext); ///< Protect from GC
} /* }}} */

 /** subRubyRun {{{
  * @brief Safely run ruby script
  * @param[in]  script  A #SubSublet
  **/

void
subRubyRun(void *script)
{
  int error = 0;

  assert(script);

  /* Safety first */
  rb_protect(RubyProtect, (VALUE)script, &error); ///< Load sublet
  if(error)
    {
      SubSublet *s = SUBLET(script);

      if(s->flags & SUB_TYPE_SUBLET) ///< Sublet
        {
          RubyPerror("Sublet", False);
          subArrayRemove(subtle->sublets, script);
          subSubletKill(s, True);
        }
      else RubyPerror("Grab", False); ///< Grab
    }
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
