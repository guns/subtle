
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

static VALUE shelter = Qnil, sublets = Qnil, subtlext = Qnil; ///< Shelter for sweeped objects

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
                    s->flags |= SUB_DATA_INOTIFY;
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

/* RubySubletData {{{ */
static VALUE
RubySubletData(VALUE self)
{
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);

  if(s)
    {
      if(s->flags & SUB_DATA_NUM)      return INT2FIX(s->data.num);
      else if(s->flags & SUB_DATA_STRING) return rb_str_new2(s->data.string);
    }

  return Qnil;
} /* }}} */

/* RubySubletDataSet {{{ */
static VALUE
RubySubletDataSet(VALUE self,
  VALUE value)
{
  SubSublet *s = NULL;
  Data_Get_Struct(self, SubSublet, s);

  if(s)
    {
      s->flags &= ~(SUB_DATA_NUM|SUB_DATA_STRING|SUB_DATA_NIL); ///< Clear flags

      switch(rb_type(value))
        {
          case T_FIXNUM: 
            s->flags    |= SUB_DATA_NUM;
            s->data.num  = FIX2INT(value);
            s->width     = 63; ///< Magic number
            return Qtrue;
          case T_STRING: 
            if(s->data.string) free(s->data.string);
            s->flags       |= SUB_DATA_STRING;
            s->data.string  = strdup(RSTRING_PTR(value));
            s->width        = RSTRING_LEN(value) * subtle->fx + 6;
            return Qtrue;
          default:
            s->flags |= SUB_DATA_NIL;
            subSharedLogWarn("Unknown value type\n");
        }
    }

  return Qfalse;
} /* }}} */

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

/* RubyParseColor {{{ */
static unsigned long
RubyParseColor(VALUE hash,
  char *key,
  char *fallback)
{ 
  XColor color = { 0 };
  Colormap cmap = DefaultColormap(subtle->disp, DefaultScreen(subtle->disp));
  char *name = RubyGetString(hash, key, fallback);
  
  /* Parse and allow color */
  if(!XParseColor(subtle->disp, cmap, name, &color))
    {
      subSharedLogWarn("Failed loading color `%s'\n", key);
    }
  else if(!XAllocColor(subtle->disp, cmap, &color))
    subSharedLogWarn("Failed allocating color `%s'\n", key);

  return color.pixel;
} /* }}} */

/* RubyConfigForeach {{{ */
static int
RubyConfigForeach(VALUE key,
  VALUE value,
  VALUE extra)
{
  int type = -1;
  void *entry = NULL;
  SubData data;

  /* Create various types */
  switch(extra)
    {
      case SUB_TYPE_GRAB:
        switch(rb_type(value)) ///< Check value type
          {
            case T_STRING: ///< Exec
              data = DATA(strdup(STR2CSTR(value)));
              type = SUB_GRAB_EXEC;
              break;
            case T_FIXNUM: ///< Specific
              type = FIX2INT(value);

              if(RNGVIEW <= type && RNGVIEW + 100 >= type) ///< Jump
                {
                  data = DATA((unsigned long)(type - RNGVIEW - 1)); ///< Update for index
                  type = SUB_GRAB_JUMP;
                }
              else if(RNGGRAV <= type && RNGGRAV + 100 >= type) ///< Gravity
                {
                  data = DATA((unsigned long)(type - RNGGRAV));
                  type = SUB_GRAB_GRAVITY;
                }
              break;
            case T_DATA: ///< Proc   
              data = DATA(value);
              type = SUB_GRAB_PROC;

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

/* RubyConfigParse {{{ */
static VALUE
RubyConfigParse(VALUE path)
{
  int size;
  char *family = NULL, *style = NULL, font[100];
  VALUE config = 0;

  rb_require(STR2CSTR(path));

  if(!subtle->disp) return Qnil;

  /* Config: Options */
  config = rb_const_get(rb_cObject, rb_intern("OPTIONS"));
  subtle->bw           = RubyGetFixnum(config, "border", 2);
  subtle->step         = RubyGetFixnum(config, "step", 5);
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
  subtle->colors.border = RubyParseColor(config, "border",     "#bdbabd");
  subtle->colors.norm   = RubyParseColor(config, "normal",     "#22aa99");
  subtle->colors.focus  = RubyParseColor(config, "focus",      "#ffa500");
  subtle->colors.bg     = RubyParseColor(config, "background", "#336699");
  subtle->colors.font   = RubyParseColor(config, "font",       "#000000");

  /* Config: Font */
  if(subtle->xfs) 
    {
      XFreeFont(subtle->disp, subtle->xfs);
      subtle->xfs = NULL;
    }

  snprintf(font, sizeof(font), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", family, style, size);
  if(!(subtle->xfs = XLoadQueryFont(subtle->disp, font)))
    {
      subSharedLogWarn("Failed loading font `%s' - falling back to fixed\n", family);
      subSharedLogDebug("Font: %s\n", font);
      subtle->xfs = XLoadQueryFont(subtle->disp, "-*-fixed-medium-*-*-*-13-*-*-*-*-*-*-*");
      if(!subtle->xfs) subSharedLogError("Failed loading font `fixed`\n");
    }

  subtle->fx = (subtle->xfs->min_bounds.width + subtle->xfs->max_bounds.width) / 2;
  subtle->fy = subtle->xfs->max_bounds.ascent + subtle->xfs->max_bounds.descent;
  subtle->th = subtle->xfs->ascent + subtle->xfs->descent + 2;

  /* Config: Grabs */
  config = rb_const_get(rb_cObject, rb_intern("GRABS"));
  rb_hash_foreach(config, RubyConfigForeach, SUB_TYPE_GRAB);

  /* Config: Tags */
  config = rb_const_get(rb_cObject, rb_intern("TAGS"));
  rb_hash_foreach(config, RubyConfigForeach, SUB_TYPE_TAG);

  /* Config: Views */
  config = rb_const_get(rb_cObject, rb_intern("VIEWS"));
  rb_hash_foreach(config, RubyConfigForeach, SUB_TYPE_VIEW);

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
          subSharedLogError("%s: %s\n", msg, RSTRING(message)->ptr);
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

/* RubyConst {{{ */
static VALUE  
RubyConst(int argc, 
  VALUE *argv, 
  VALUE self)
{  
  char *name = NULL, *suffix = NULL;
  VALUE missing = Qnil, args = Qnil;     

  rb_scan_args(argc, argv, "1*", &missing, &args);  
  name = (char *)rb_id2name(SYM2ID(missing));  

  subSharedLogDebug("Missing: const=%s\n", name);

  /* Parse missing constants */  
  if(!strncmp(name, "Jump", 4)) ///< Jump
    {
      if((suffix = (char *)name + 4))
        return INT2FIX(RNGVIEW + atoi(suffix));
    }
  else if(!strncmp(name, "Gravity", 7)) ///< Gravity
    {
      if((suffix = (char *)name + 7))
        {
          VALUE mod = Qnil, submod = Qnil, value = Qnil;

          mod    = rb_const_get(rb_mKernel, rb_intern("Subtlext"));
          submod = rb_const_get(mod, rb_intern("Gravity"));
          
          if(Qnil != ((value  = rb_const_get(submod, rb_intern(suffix)))))
            return INT2FIX(RNGGRAV + FIX2INT(value));
        }
    }

  rb_raise(rb_eStandardError, "Failed finding constant `%s'", name);
  return Qnil;  
} /* }}} */

/* RubyMethod {{{ */
static VALUE  
RubyMethod(int argc, 
  VALUE *argv, 
  VALUE self)
{  
  char *name = NULL;
  VALUE missing = Qnil, args = Qnil;     

  rb_scan_args(argc, argv, "1*", &missing, &args);  
  name = (char *)rb_id2name(SYM2ID(missing));  

  subSharedLogDebug("Missing: method=%s\n", name);

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
  int error = 0;
  VALUE mod = Qnil, submod = Qnil, klass = Qnil;

  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

  rb_define_method(rb_mKernel, "method_missing", RubyMethod, -1); ///< Subtlext wrapper

  /* Module: subtle */
  mod = rb_define_module("Subtle");

  /* Module: grab */
  submod = rb_define_module_under(mod, "Grab");
  rb_define_singleton_method(submod, "const_missing", RubyConst, -1);

  rb_define_const(submod, "WindowMove",   INT2FIX(SUB_GRAB_WINDOW_MOVE));
  rb_define_const(submod, "WindowResize", INT2FIX(SUB_GRAB_WINDOW_RESIZE));
  rb_define_const(submod, "WindowFloat",  INT2FIX(SUB_GRAB_WINDOW_FLOAT));
  rb_define_const(submod, "WindowFull",   INT2FIX(SUB_GRAB_WINDOW_FULL));
  rb_define_const(submod, "WindowStick",  INT2FIX(SUB_GRAB_WINDOW_STICK));
  rb_define_const(submod, "WindowRaise",  INT2FIX(SUB_GRAB_WINDOW_RAISE));
  rb_define_const(submod, "WindowLower",  INT2FIX(SUB_GRAB_WINDOW_LOWER));
  rb_define_const(submod, "WindowUp",     INT2FIX(SUB_GRAB_WINDOW_UP));
  rb_define_const(submod, "WindowLeft",   INT2FIX(SUB_GRAB_WINDOW_LEFT));
  rb_define_const(submod, "WindowRight",  INT2FIX(SUB_GRAB_WINDOW_RIGHT));
  rb_define_const(submod, "WindowDown",   INT2FIX(SUB_GRAB_WINDOW_DOWN));
  rb_define_const(submod, "WindowKill",   INT2FIX(SUB_GRAB_WINDOW_KILL));

  /* Load subtlext */
  rb_protect(RubyRequire, rb_str_new2("subtle/subtlext"), &error);
  if(error) RubyPerror("Failed loading sublext", True);

  /* Class: sublet */
  klass = rb_define_class_under(mod, "Sublet", rb_cObject);
  rb_define_singleton_method(klass, "new",       RubySubletNew,       0);
  rb_define_singleton_method(klass, "inherited", RubySubletInherited, 1);

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
  int error = 0;
  char config[100];
  FILE *fd = NULL;

  /* Check path */
  if(!file)
    {
      snprintf(config, sizeof(config), "%s/%s/%s", 
        getenv("XDG_CONFIG_HOME"), PKG_NAME, PKG_CONFIG);
      if(!(fd = fopen(config, "r"))) 
        {
          snprintf(config, sizeof(config), "%s/%s", DIR_CONFIG, PKG_CONFIG);
        }
      else fclose(fd);
    }
  else snprintf(config, sizeof(config), "%s", file);
  subSharedLogDebug("config=%s\n", config);

  /* Safety first */
  rb_protect(RubyConfigParse, rb_str_new2(config), &error);
  if(error) RubyPerror("Failed reading config", True);

  if(!subtle->disp) return;

  /* Grabs */
  if(0 == subtle->grabs->ndata) 
    {
      subSharedLogWarn("No grabs found\n");
    }
  else subArraySort(subtle->grabs, subGrabCompare);

  /* Tags */
  if(4 == subtle->tags->ndata) 
    {
      subSharedLogWarn("No tags found\n");
    }
  else subTagPublish();

  /* Views */
  if(0 == subtle->views->ndata)
    {
      SubView *v = subViewNew("subtle", "default");

      subArrayPush(subtle->views, (void *)v);
      subSharedLogWarn("No views found\n");
    }
  else
    {
      SubView *v = VIEW(subtle->views->data[0]);

      v->tags |= (1L << 1); ///< Add default tag to first view
      subEwmhSetCardinals(v->button, SUB_EWMH_SUBTLE_WINDOW_TAGS, (long *)&v->tags, 1);
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
          int error = 0;
          char file[150];

          snprintf(file, sizeof(file), "%s/%s", buf, entries[i]->d_name);
          subSharedLogDebug("path=%s\n", file);

          /* Safety first */
          rb_protect(RubyRequire, rb_str_new2(file), &error); ///< Load sublet
          if(error) RubyPerror("Failed loading sublet", False);

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

 /** subRubyLoadSublext {{{
  * @brief Load sublext
  **/

void
subRubyLoadSublext(void)
{
  VALUE mod = Qnil, klass = Qnil;

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
  if(subtle->notify) close(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */

  subSharedLogDebug("kill=ruby\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
