
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
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <ctype.h>
#include <ruby.h>
#include "subtle.h"

static VALUE sublet, sublets, run, interval, data; ///< Common used symbols

/* SubletInit {{{ */
static VALUE
SubletInit(VALUE self,
  VALUE value)
{
  rb_ivar_set(self, data, rb_str_new2("n/a")); ///< Default value

  return(self); ///< Dummy
} /* }}} */

/* SubletInherited {{{ */
static VALUE
SubletInherited(VALUE self,
  VALUE recv)
{
  const char *name = rb_class2name((ID)recv);

  rb_ary_push(sublets, recv); ///< Push onto ruby-intern sublet list

  printf("Loading sublet %s\n", name);

  return(Qnil);                                           
} /* }}} */                                               

/* RubyGetString {{{ */
static char *
RubyGetString(VALUE hash,
  char *key,
  char *fallback)
{
  VALUE value = rb_funcall(hash, rb_intern("fetch"), 1, rb_str_new2(key));
 
  if(T_STRING == rb_type(value)) return(STR2CSTR(value));

  return(fallback);
} /* }}} */

/* RubyGetFixnum {{{ */
static int
RubyGetFixnum(VALUE hash,
  char *key,
  int fallback)
{
  VALUE value = rb_funcall(hash, rb_intern("fetch"), 1, rb_str_new2(key));

  if(T_FIXNUM == rb_type(value)) return(FIX2INT(value));

  return(fallback);
} /* }}} */

/* RubyParseColor {{{ */
static double
RubyParseColor(VALUE hash,
  char *key,
  char *fallback)
{ 
  XColor color = { 0, 0, 0, 0, '0', '0' };
  Colormap cmap = DefaultColormap(subtle->disp, DefaultScreen(subtle->disp));
  char *name = RubyGetString(hash, key, fallback);
  
  /* Parse and allow color */
  if(!XParseColor(subtle->disp, cmap, name, &color)) 
    {
      subUtilLogWarn("Can't load color '%s'.\n", key);
    }
  else if(!XAllocColor(subtle->disp, cmap, &color)) subUtilLogWarn("Can't alloc color '%s'.\n", key);

  return(color.pixel);
} /* }}} */

/* RubyCall {{{ */
VALUE
RubyCall(VALUE dummy)
{
  VALUE result = 0;
  SubSublet *s = SUBLET(dummy);

  s->flags &= ~(SUB_DATA_FIXNUM|SUB_DATA_STRING|SUB_DATA_NIL); ///< Remove
  rb_funcall(s->recv, run, 0, NULL);

  /* Fetch data */
  result = rb_ivar_get(s->recv, interval);
  if(T_FIXNUM == rb_type(result)) s->interval = FIX2INT(result);

  result = rb_ivar_get(s->recv, data);
  switch(rb_type(result))
    {
      case T_FIXNUM: 
        s->flags |= SUB_DATA_FIXNUM;
        s->fixnum = FIX2INT(result);
        s->width  = 63; ///< Magic number
      case T_STRING: 
        if(s->string) free(s->string);

        s->flags |= SUB_DATA_STRING;

        s->string = RSTRING_PTR(result);
        s->width  = RSTRING_LEN(result) * subtle->fx + 6;
        break;
      default:
        s->flags |= SUB_DATA_NIL;
        subUtilLogWarn("Unknown value type\n");
    }

  return(Qnil);
} /* }}} */

/* RubyHashIterate {{{ */
static int
RubyHashIterate(VALUE key,
  VALUE value,
  VALUE extra)
{
  void *entry = NULL;

  /* Create various types */
  switch(extra)
    {
      case SUB_TYPE_KEY:
        entry = (void *)subKeyNew(STR2CSTR(key), STR2CSTR(value));
        subArrayPush(subtle->keys, entry);
        break;

      case SUB_TYPE_TAG:
        entry = (void *)subTagNew(STR2CSTR(key), STR2CSTR(value));
        subArrayPush(subtle->tags, entry);
        break;

      case SUB_TYPE_VIEW:
        entry = (void *)subViewNew(STR2CSTR(key), STR2CSTR(value));
        subArrayPush(subtle->views, entry);
        break;

      default: 
        subUtilLogDebug("Never to be reached?\n");
    }

  return(Qnil);
} /* }}} */

/* RubyArrayIterate {{{ */
VALUE
RubyArrayIterate(VALUE elem,
  VALUE *obj)
{
  VALUE recv = 0, result = 0;
  SubSublet *s = NULL;
  
  /* Create new class instance */
  recv = rb_funcall(elem, rb_intern("new"), 0, NULL);
  if(recv)
    {
      /* Fetch data */
      result = rb_ivar_get(recv, interval);
      switch(rb_type(result))
        {
          case T_FIXNUM: s = subSubletNew(recv, FIX2INT(result), NULL); break;
          case T_STRING: s = subSubletNew(recv, 0, STR2CSTR(result));    break;
          default:
            subUtilLogWarn("Failed to initialize sublet");
            return(Qnil);
        }

      subRubyCall(s);
      subArrayPush(subtle->sublets, s);
    }

  return(Qnil);
} /* }}} */

/* RubyParseConfig {{{ */
static VALUE
RubyParseConfig(VALUE path)
{
  int size;
  char *face = NULL, *style = NULL, font[100]; 
  XGCValues gvals;
  XSetWindowAttributes attrs;
  VALUE yaml = 0, hash = 0, fetch = 0, config = 0, type = 0;

  /* Yaml */
  rb_require("yaml");
  yaml  = rb_const_get(rb_mKernel, rb_intern("YAML"));
  hash  = rb_funcall(yaml, rb_intern("load_file"), 1, path);
  fetch = rb_intern("fetch");

  /* Config: Options */
  config     = rb_funcall(hash, fetch, 1, rb_str_new2("Options"));
  subtle->bw = RubyGetFixnum(config, "Border",  2);

  /* Config: Font */
  config = rb_funcall(hash, fetch, 1, rb_str_new2("Font"));
  face   = RubyGetString(config, "Face",  "fixed");
  style  = RubyGetString(config, "Style", "medium");
  size   = RubyGetFixnum(config, "Size",  12);

  /* Config: Colors */
  config                = rb_funcall(hash, fetch, 1, rb_str_new2("Colors"));
  subtle->colors.font   = RubyParseColor(config, "Font",       "#000000");   
  subtle->colors.border = RubyParseColor(config, "Border",     "#bdbabd");
  subtle->colors.norm   = RubyParseColor(config, "Normal",     "#22aa99");
  subtle->colors.focus  = RubyParseColor(config, "Focus",      "#ffa500");    
  subtle->colors.cover  = RubyParseColor(config, "Shade",      "#FFE6E6");
  subtle->colors.bg     = RubyParseColor(config, "Background", "#336699");

  /* Load font */
  snprintf(font, sizeof(font), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", face, style, size);
  if(!(subtle->xfs = XLoadQueryFont(subtle->disp, font)))
    {
      subUtilLogWarn("Can't load font `%s', using fixed instead.\n", face);
      subUtilLogDebug("Font: %s\n", font);
      subtle->xfs = XLoadQueryFont(subtle->disp, "-*-fixed-medium-*-*-*-13-*-*-*-*-*-*-*");
      if(!subtle->xfs) subUtilLogError("Can't load font `fixed`.\n");
    }

   /* Font metrics */
  subtle->fx = (subtle->xfs->min_bounds.width + subtle->xfs->max_bounds.width) / 2;
  subtle->fy = subtle->xfs->max_bounds.ascent + subtle->xfs->max_bounds.descent;
  subtle->th = subtle->xfs->ascent + subtle->xfs->descent + 2;

  /* View windows */
  attrs.background_pixel = subtle->colors.norm;
  attrs.save_under       = False;
  attrs.event_mask       = ButtonPressMask|ExposureMask|VisibilityChangeMask;

  subtle->bar.win     = WINNEW(DefaultRootWindow(subtle->disp), 0, 0, 
    DisplayWidth(subtle->disp, DefaultScreen(subtle->disp)), subtle->th, 0, CWBackPixel|CWSaveUnder|CWEventMask);
  subtle->bar.views   = XCreateSimpleWindow(subtle->disp, subtle->bar.win, 0, 0, 1, subtle->th, 0, 0, subtle->colors.norm);
  subtle->bar.sublets = XCreateSimpleWindow(subtle->disp, subtle->bar.win, 0, 0, 1, subtle->th, 0, 0, subtle->colors.norm);

  XSelectInput(subtle->disp, subtle->bar.views, ButtonPressMask); 

  XMapWindow(subtle->disp, subtle->bar.views);
  XMapWindow(subtle->disp, subtle->bar.sublets);
  XMapWindow(subtle->disp, subtle->bar.win);

  /* Update GCs */
  gvals.foreground = subtle->colors.border;
  gvals.line_width = subtle->bw;
  XChangeGC(subtle->disp, subtle->gcs.border, GCForeground|GCLineWidth, &gvals);

  gvals.foreground  = subtle->colors.font;
  gvals.font        = subtle->xfs->fid;
  XChangeGC(subtle->disp, subtle->gcs.font, GCForeground|GCFont, &gvals);

  /* Update root window */
  attrs.cursor           = subtle->cursors.arrow;
  attrs.background_pixel = subtle->colors.bg;
  attrs.event_mask       = SubstructureRedirectMask|SubstructureNotifyMask|PropertyChangeMask;
  XChangeWindowAttributes(subtle->disp, DefaultRootWindow(subtle->disp), CWCursor|CWBackPixel|CWEventMask, &attrs);
  XClearWindow(subtle->disp, DefaultRootWindow(subtle->disp));

  /* Config: Keys */
  type   = SUB_TYPE_KEY;
  config = rb_funcall(hash, fetch, 1, rb_str_new2("Keys"));
  rb_hash_foreach(config, RubyHashIterate, type);

  /* Config: Tags */
  type   = SUB_TYPE_TAG;
  config = rb_funcall(hash, fetch, 1, rb_str_new2("Tags"));
  rb_hash_foreach(config, RubyHashIterate, type);

  /* Config: Views */
  type   = SUB_TYPE_VIEW;
  config = rb_funcall(hash, fetch, 1, rb_str_new2("Views"));
  rb_hash_foreach(config, RubyHashIterate, type);

  return(Qnil);
} /* }}} */

 /** subRubyInit {{{
  * @brief Start ruby
  **/

void
subRubyInit(void)
{
  /* Init ruby */
  RUBY_INIT_STACK;
  ruby_init();
  ruby_init_loadpath();
  ruby_script("subtle");

  /* Init sublet class */
  sublet = rb_define_class("Sublet", rb_cObject);
  rb_define_method(sublet, "initialize", SubletInit, 1);
  rb_define_singleton_method(sublet, "inherited", SubletInherited, 1);

  /* Define attrs */
  rb_define_attr(sublet, "interval", 1, 1);
  rb_define_attr(sublet, "data", 1, 1);

  /* Comon used symbols */
  run      = rb_intern("run");
  interval = rb_intern("@interval");
  data     = rb_intern("@data");
} /* }}} */

 /** subRubyLoadConfig {{{
  * @brief Load config file from path
  * @param[in]  path  Path of the config file
  **/

void
subRubyLoadConfig(const char *path)
{
  int status;
  char config[100];
  DIR *dir = NULL;
  SubTag *deftag = NULL;

  /* Check path */
  if(!path)
    {
      snprintf(config, sizeof(config), "%s/.%s", getenv("HOME"), PKG_NAME);
      if((dir = opendir(config))) 
        {
          snprintf(config, sizeof(config), "%s/.%s/subtle.yml", getenv("HOME"), PKG_NAME);
          closedir(dir);
        }
      else snprintf(config, sizeof(config), "%s/subtle.yml", DIR_CONFIG);
    }
  else snprintf(config, sizeof(config), "%s/subtle.yml", path);
  subUtilLogDebug("config=%s\n", config);

  /* Safety first */
  rb_protect(RubyParseConfig, rb_str_new2(config), &status);
  if(Qundef == status) subUtilLogError("Failed reading/parsing config\n");

  /* Keys */
  if(0 == subtle->keys->ndata) 
    {
      subUtilLogWarn("No keys found\n");
    }
  else subArraySort(subtle->keys, subKeyCompare);

  /* Tags */
  if(0 == subtle->tags->ndata) subUtilLogWarn("No tags found\n");
  deftag = subTagNew("default", NULL); ///< Default tag
  subArrayPush(subtle->tags, (void *)deftag);
  subTagPublish();

  /* Views */
  if(0 == subtle->views->ndata)
    {
      SubView *v = subViewNew("subtle", "default");
      subArrayPush(subtle->views, (void *)v);
      subUtilLogWarn("No views found\n");
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

  rb_gc_start(); ///< Start GC to get rid of temp stuff
} /* }}} */

 /** subRubyLoadSublets {{{
  * @brief Load sublets from path
  * @param[in]  path  Path of the sublets
  **/

void
subRubyLoadSublets(const char *path)
{
  int i;
  DIR *dir = NULL;
  char buf[100];
  struct dirent *entry = NULL;

  sublets = rb_ary_new();

#ifdef HAVE_SYS_INOTIFY_H
  if((subtle->notify = inotify_init()) < 0)
    {
      subUtilLogWarn("Failed initing inotify\n");
      subUtilLogDebug("Inotify: %s\n", strerror(errno));
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
  subUtilLogDebug("path=%s\n", buf);

  /* Read directory */
  if((dir = opendir(buf)))
    {
      while((entry = readdir(dir)))
        {
          if(!fnmatch("*.rb", entry->d_name, FNM_PATHNAME)) ///< Check file extension
            {
              char file[150];

              snprintf(file, sizeof(file), "%s/%s", buf, entry->d_name);
              rb_require(file);
            }
        }
      closedir(dir);

      rb_iterate(rb_each, sublets, RubyArrayIterate, Qnil); ///< Instantiate sublets

      if(0 < subtle->sublets->ndata)
        {
          /* Preserve load order */
          for(i = 0; i < subtle->sublets->ndata - 1; i++) 
            SUBLET(subtle->sublets->data[i])->next = SUBLET(subtle->sublets->data[i + 1]);
          subtle->sublet = SUBLET(subtle->sublets->data[0]);
          
          /* Sort and configure */
          subArraySort(subtle->sublets, subSubletCompare);
          subSubletConfigure();
        }
    }
  else subUtilLogWarn("No sublets found\n"); 
} /* }}} */

 /** subRubyCall {{{
  * @brief Call ruby script
  * @param[in]  s  Call a ruby script
  **/

void
subRubyCall(SubSublet *s)
{
  int status;

  assert(s);

  rb_protect(RubyCall, (VALUE)s, &status);
  if(Qundef == status)
    {
      subUtilLogWarn("Failed running sublet\n");
      subSubletKill(s);
    }
} /* }}} */

 /** subRubyFinish {{{
  * @brief Finish ruby stack
  **/

void
subRubyFinish(void)
{
  rb_exit(0);

#ifdef HAVE_SYS_INOTIFY_H
  if(subtle->notify) close(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */

  subUtilLogDebug("kill=ruby\n");
} /* }}} */
