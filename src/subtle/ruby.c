
 /**
  * @package subtle
  *
  * @file Ruby functions
  * @copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
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

static VALUE sublets; ///< Sublet list

/* RubySubletNew {{{ */
static VALUE
RubySubletNew(VALUE self)
{
  SubSublet *s = NULL;
  VALUE data = Qnil;

  /* Create sublet */
  s = subSubletNew();
  data    = Data_Wrap_Struct(self, 0, NULL, (void *)s); ///< We omit the finalizer
  s->recv = data; 
  s->name = strdup(rb_class2name(self));

  rb_obj_call_init(data, 0, NULL); ///< Call initialize
  if(0 == s->interval) s->interval = 60; ///< Sanitize

  subArrayPush(subtle->sublets, s);
  subRubyRun(s); ///< Init data

  return data;
} /* }}} */

/* RubySubletInherited {{{ */
static VALUE
RubySubletInherited(VALUE self,
  VALUE recv)
{
  const char *name = rb_class2name((ID)recv);

  rb_ary_push(sublets, recv); ///< Add derived class name to sublet list

  printf("Loading sublet (%s)\n", name);

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
      switch(rb_type(value))
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

                subSharedLogWarn("Failed to watch file `%s': %s\n", watch, strerror(errno));
              }
            break;
          default: subSharedLogWarn("Unknown value type\n");
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
      if(s->flags & SUB_DATA_FIXNUM)      return INT2FIX(s->fixnum);
      else if(s->flags & SUB_DATA_STRING) return rb_str_new2(s->string);
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
      s->flags &= ~(SUB_DATA_FIXNUM|SUB_DATA_STRING|SUB_DATA_NIL); ///< Clear flags

      switch(rb_type(value))
        {
          case T_FIXNUM: 
            s->flags  |= SUB_DATA_FIXNUM;
            s->fixnum  = FIX2INT(value);
            s->width   = 63; ///< Magic number
            return Qtrue;
          case T_STRING: 
            if(s->string) free(s->string);
            s->flags  |= SUB_DATA_STRING;
            s->string  = strdup(RSTRING_PTR(value));
            s->width   = RSTRING_LEN(value) * subtle->fx + 6;
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
      subSharedLogWarn("Failed to load color '%s'.\n", key);
    }
  else if(!XAllocColor(subtle->disp, cmap, &color))
    subSharedLogWarn("Failed to alloc color '%s'.\n", key);

  return color.pixel;
} /* }}} */

/* RubyProtectedRun {{{ */
static VALUE
RubyProtectedRun(VALUE recv)
{
  rb_funcall(recv, rb_intern("run"), 0, NULL);

  return Qnil;
} /* }}} */

/* RubyArrayIterate {{{ */
static VALUE
RubyArrayIterate(VALUE elem,
  VALUE *obj)
{
  VALUE status = 0;
  
  /* Create new class instance */
  if(!(status = rb_funcall(elem, rb_intern("new"), 0, NULL)))
    subSharedLogWarn("Failed running sublet\n");

  return Qnil;
} /* }}} */

/* RubyConfigIterate {{{ */
static int
RubyConfigIterate(VALUE key,
  VALUE value,
  VALUE extra)
{
  void *entry = NULL;

  /* Create various types */
  switch(extra)
    {
      case SUB_TYPE_GRAB:
        entry = (void *)subGrabNew(STR2CSTR(key), STR2CSTR(value));
        if(entry) subArrayPush(subtle->grabs, entry);
        break;

      case SUB_TYPE_TAG:
        entry = (void *)subTagNew(STR2CSTR(key), STR2CSTR(value));
        if(entry) subArrayPush(subtle->tags, entry);
        break;

      case SUB_TYPE_VIEW:
        entry = (void *)subViewNew(STR2CSTR(key), STR2CSTR(value));
        if(entry) subArrayPush(subtle->views, entry);
        break;

      default: subSharedLogDebug("Never to be reached?\n");
    }

  return Qnil;
} /* }}} */

/* RubyConfigParse {{{ */
static VALUE
RubyConfigParse(VALUE path)
{
  int size;
  char *family = NULL, *style = NULL, font[100];
  XGCValues gvals;
  XSetWindowAttributes attrs;
  VALUE config = 0;

  rb_require(STR2CSTR(path));

  /* Config: Options */
  config = rb_const_get(rb_cObject, rb_intern("OPTIONS"));
  subtle->bw   = RubyGetFixnum(config, "border", 2);
  subtle->step = RubyGetFixnum(config, "step", 5);

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

  /* Load font */
  snprintf(font, sizeof(font), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", family, style, size);
  if(!(subtle->xfs = XLoadQueryFont(subtle->disp, font)))
    {
      subSharedLogWarn("Can't load font `%s', using fixed instead.\n", family);
      subSharedLogDebug("Font: %s\n", font);
      subtle->xfs = XLoadQueryFont(subtle->disp, "-*-fixed-medium-*-*-*-13-*-*-*-*-*-*-*");
      if(!subtle->xfs) subSharedLogError("Can't load font `fixed`.\n");
    }

  /* Font metrics */
  subtle->fx = (subtle->xfs->min_bounds.width + subtle->xfs->max_bounds.width) / 2;
  subtle->fy = subtle->xfs->max_bounds.ascent + subtle->xfs->max_bounds.descent;
  subtle->th = subtle->xfs->ascent + subtle->xfs->descent + 2;

  /* View windows */
  attrs.background_pixel = subtle->colors.norm;
  attrs.save_under       = False;
  attrs.event_mask       = ButtonPressMask|ExposureMask|VisibilityChangeMask;

  subtle->windows.bar     = XCreateWindow(subtle->disp, DefaultRootWindow(subtle->disp),
    0, 0, SCREENW, subtle->th, 0, CopyFromParent, InputOutput, CopyFromParent, 
    CWBackPixel|CWSaveUnder|CWEventMask, &attrs); 
  subtle->windows.caption = XCreateSimpleWindow(subtle->disp, subtle->windows.bar, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.focus);
  subtle->windows.views   = XCreateSimpleWindow(subtle->disp, subtle->windows.bar, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.norm);
  subtle->windows.tray    = XCreateSimpleWindow(subtle->disp, subtle->windows.bar, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.norm);    
  subtle->windows.sublets = XCreateSimpleWindow(subtle->disp, subtle->windows.bar, 0, 0, 1,
    subtle->th, 0, 0, subtle->colors.norm);

  /* Map windows */
  XMapWindow(subtle->disp, subtle->windows.views);
  XMapWindow(subtle->disp, subtle->windows.sublets);
  XMapWindow(subtle->disp, subtle->windows.tray);
  XMapWindow(subtle->disp, subtle->windows.bar);

  /* Select input */
  XSelectInput(subtle->disp, subtle->windows.views, ButtonPressMask); 
  XSelectInput(subtle->disp, subtle->windows.tray, KeyPressMask|ButtonPressMask); 
  subTraySelect(); ///< Get tray selection

  /* Update GCs */
  gvals.foreground = subtle->colors.border;
  gvals.line_width = subtle->bw;
  XChangeGC(subtle->disp, subtle->gcs.stipple, GCForeground|GCLineWidth, &gvals);

  gvals.foreground = subtle->colors.font;
  gvals.font       = subtle->xfs->fid;
  XChangeGC(subtle->disp, subtle->gcs.font, GCForeground|GCFont, &gvals);

  /* Update root window */
  attrs.cursor           = subtle->cursors.arrow;
  attrs.background_pixel = subtle->colors.bg;
  attrs.event_mask       = SubstructureRedirectMask|SubstructureNotifyMask|PropertyChangeMask;
  XChangeWindowAttributes(subtle->disp, ROOT, CWCursor|CWBackPixel|CWEventMask, &attrs);
  XClearWindow(subtle->disp, ROOT);

  /* Config: Keys */
  config = rb_const_get(rb_cObject, rb_intern("GRABS"));
  rb_hash_foreach(config, RubyConfigIterate, SUB_TYPE_GRAB);

  /* Config: Tags */
  config = rb_const_get(rb_cObject, rb_intern("TAGS"));
  rb_hash_foreach(config, RubyConfigIterate, SUB_TYPE_TAG);

  /* Config: Views */
  config = rb_const_get(rb_cObject, rb_intern("VIEWS"));
  rb_hash_foreach(config, RubyConfigIterate, SUB_TYPE_VIEW);

  return Qnil;
} /* }}} */

/* RubyFilter {{{ */
static inline int
RubyFilter(const struct dirent *entry)
{
  return !fnmatch("*.rb", entry->d_name, FNM_PATHNAME);
} /* }}} */

 /** subRubyInit {{{
  * @brief Init ruby
  **/

void
subRubyInit(void)
{
  VALUE klass = Qnil;

  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

  /* Class: sublet */
  klass = rb_define_class("Sublet", rb_cObject);
  rb_define_singleton_method(klass, "new",       RubySubletNew, 0);
  rb_define_singleton_method(klass, "inherited", RubySubletInherited, 1);
  rb_define_method(klass, "interval",  RubySubletInterval, 0);
  rb_define_method(klass, "interval=", RubySubletIntervalSet, 1);
  rb_define_method(klass, "data",      RubySubletData, 0);
  rb_define_method(klass, "data=",     RubySubletDataSet, 1);

#ifdef HAVE_SYS_INOTIFY_H
  rb_define_method(klass, "path",      RubySubletPath, 0);
  rb_define_method(klass, "path=",     RubySubletPathSet, 1);
#endif /* HAVE_SYS_INOTIFY */  
} /* }}} */

 /** subRubyLoadConfig {{{
  * @brief Load config file
  * @param[in]  file  Config file
  **/

void
subRubyLoadConfig(const char *file)
{
  int status;
  char config[100];
  FILE *fd = NULL;

  /* Check path */
  if(!file)
    {
      snprintf(config, sizeof(config), "%s/.%s/%s", getenv("HOME"), PKG_NAME, PKG_CONFIG);
      if(!(fd = fopen(config, "r"))) 
        {
          snprintf(config, sizeof(config), "%s/%s", DIR_CONFIG, PKG_CONFIG);
        }
      else fclose(fd);
    }
  else snprintf(config, sizeof(config), "%s", file);
  subSharedLogDebug("config=%s\n", config);

  /* Safety first */
  rb_protect(RubyConfigParse, rb_str_new2(config), &status);
  if(Qundef == status) subSharedLogWarn("Failed reading/parsing config\n");

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
      subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS, (long *)&v->tags, 1);
    }

  subViewUpdate();
  subViewJump(VIEW(subtle->views->data[0])); ///< Jump to first view
  subViewPublish();

  rb_gc_start();
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

  sublets = rb_ary_new();

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
      snprintf(buf, sizeof(buf), "%s/.%s/sublets", getenv("HOME"), PKG_NAME);
      if((dir = opendir(buf))) closedir(dir);
      else snprintf(buf, sizeof(buf), "%s", DIR_SUBLET);
    }
  else snprintf(buf, sizeof(buf), "%s", path);
  subSharedLogDebug("path=%s\n", buf);

  /* Scan directory */
  num = scandir(buf, &entries, RubyFilter, alphasort);
  if(0 < num)
    {
      for(i = 0; i < num; i++)
        {
          char file[150];

          snprintf(file, sizeof(file), "%s/%s", buf, entries[i]->d_name);
          subSharedLogDebug("sublet=%s\n", file);
          rb_require(file); ///< Load subclass of Sublet

          free(entries[i]);
        }
      free(entries);

      rb_iterate(rb_each, sublets, RubyArrayIterate, Qnil); ///< Instantiate sublets

      if(0 < subtle->sublets->ndata)
        {
          /* Preserve load order */
          for(i = 0; i < subtle->sublets->ndata - 1; i++) 
            SUBLET(subtle->sublets->data[i])->next = SUBLET(subtle->sublets->data[i + 1]);
          subtle->sublet = SUBLET(subtle->sublets->data[0]);
          
          /* Sort and configure */
          subArraySort(subtle->sublets, subSubletCompare);
          subSubletUpdate();
          subSubletPublish();
        }
    }
  else subSharedLogWarn("No sublets found\n");
} /* }}} */

 /** subRubyRun {{{
  * @brief Safely run ruby script
  * @param[in]  s  A #SubSublet
  **/

void
subRubyRun(SubSublet *s)
{
  int status;

  assert(s);

  rb_protect(RubyProtectedRun, s->recv, &status);
  if(Qundef == status)
    {
      VALUE lasterr = Qnil, message = Qnil;
      
      /* Get last error message */
      lasterr = rb_gv_get("$!");
      message = rb_obj_as_string(lasterr);
      if(RTEST(message)) subSharedLogDebug("Ruby: %s\n", RSTRING(message)->ptr);

      subSharedLogWarn("Failed running sublet\n");
      subArrayPop(subtle->sublets, (void *)s);
      subSubletKill(s);
    }
} /* }}} */

 /** subRubyFinish {{{
  * @brief Finish ruby stack
  **/

void
subRubyFinish(void)
{
  ruby_finalize();
  rb_exit(0);

#ifdef HAVE_SYS_INOTIFY_H
  if(subtle->notify) close(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */

  subSharedLogDebug("kill=ruby\n");
} /* }}} */

// vim:ts=2:bs=2:sw=2:et:fdm=marker
