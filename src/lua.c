
 /**
  * @package subtle
  *
  * @file Lua functions
  * @copyright Copyright (c) 2005-2008 Christoph Kappel <unexist@dorfelite.net>
  * @version $Id$
  * 
  * This program can be distributed under the terms of the GNU GPL.
  * See the file COPYING.
  **/

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdarg.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include "subtle.h"

static lua_State *subletstate = NULL;

/* Macros {{{ */
#define GET_GLOBAL(configstate) do { \
  lua_getglobal(configstate, table); \
  if(lua_istable(configstate, -1)) \
    { \
      lua_pushstring(configstate, field); \
      lua_gettable(configstate, -2); \
    } \
} while(0)
/* }}} */

/* StateNew {{{ */
static lua_State *
StateNew(void)
{
  lua_State *state = NULL;
  if(!(state = luaL_newstate()))
    {
      subUtilLogDebug("%s\n", (char *)lua_tostring(state, -1));
      subUtilLogError("Can't open lua state.\n");
    }
  luaL_openlibs(state);

  return(state);
} /* }}} */

/* GetNum {{{ */
static int
GetNum(lua_State *configstate,
  const char *table,
  const char *field,
  int fallback)
{
  GET_GLOBAL(configstate);
  if(!lua_isnumber(configstate, -1))
    {
      subUtilLogDebug("Expected double, got `%s' for `%s'.\n", lua_typename(configstate, -1), field);
      return(fallback);
    }
  return((int)lua_tonumber(configstate, -1));
} /* }}} */

/* GetString {{{ */
static char *
GetString(lua_State *configstate,
  const char *table,
  const char *field,
  char *fallback)
{
  GET_GLOBAL(configstate);
  if(!lua_isstring(configstate, -1))
    {
      subUtilLogDebug("Expected string, got `%s' for `%s'.\n", lua_typename(configstate, -1), field);
      return(fallback);
    }
  return((char *)lua_tostring(configstate, -1));
} /* }}} */

/* ParseColor {{{ */
static double
ParseColor(lua_State *configstate,
  const char *field,
  char *fallback)
{
  XColor color;
  char *name = GetString(configstate, "Colors", field, fallback);
  color.pixel = 0;

  if(!XParseColor(subtle->disp, DefaultColormap(subtle->disp, DefaultScreen(subtle->disp)), name, &color))
    {
      subUtilLogWarn("Can't load color '%s'.\n", name);
    }
  else if(!XAllocColor(subtle->disp, DefaultColormap(subtle->disp, DefaultScreen(subtle->disp)), &color))
    subUtilLogWarn("Can't alloc color '%s'.\n", name);
  return(color.pixel);
} /* }}} */

/* CountHook {{{ */
void
CountHook(lua_State *state,
  lua_Debug *ar)
{
  if(ar && ar->event == LUA_HOOKCOUNT)
    {
      lua_sethook(state, NULL, 0, 0);
      lua_pushstring(state, "Maximum instructions reached");
      lua_error(state);
    }
} /* }}} */

/* SetField {{{ */
static void
SetField(lua_State *state,
  const char *key,
  int type,
  ...)
{
  va_list ap;

  va_start(ap, type);
  lua_pushstring(state, key);
  switch(type)
    {
      case LUA_TSTRING:   lua_pushstring(state, va_arg(ap, char *));            break;
      case LUA_TFUNCTION: lua_pushcfunction(state, va_arg(ap, lua_CFunction));  break;
      case LUA_TNUMBER:   lua_pushnumber(state, va_arg(ap, double));            break;
      case LUA_TBOOLEAN:  lua_pushboolean(state, va_arg(ap, int));              break;
    }
  lua_settable(state, -3);
  va_end(ap);
} /* }}} */

/* SubletCreate {{{ */
static int
SubletCreate(int type, 
  lua_State *state)
{
  char *string = (char *)lua_tostring(state, 2), *watch = NULL;
  int ref, interval = 0;
  
  switch(type)
    {
      case SUB_SUBLET_TYPE_TEXT: break;

#ifdef HAVE_SYS_INOTIFY_H
      case SUB_SUBLET_TYPE_WATCH: watch = (char *)lua_tostring(state, 3);  break;
#endif /* HAVE_SYS_INOTIFY_H */

      default: interval = (int)lua_tonumber(state, 3); break;
    }

  if(string)
    {
      /* Check functions with namespace */
      if(index(string, ':'))
        {
          char *table = strtok(string, ":"), *func = strtok(NULL, ":");
  
          if(table && func)
            {
              lua_getglobal(state, table);
              lua_pushstring(state, func);
              lua_gettable(state, -2);
            }
          else return(0);
        }
      else lua_getglobal(state, string);

      /* Use references instead of function names */
      ref = luaL_ref(state, LUA_REGISTRYINDEX);
      if(ref) subSubletNew(type, string, ref, interval, watch);
    }

  return(1); /* Make compiler happy */
} /* }}} */

/* LuaAddText {{{ */
static int
LuaAddText(lua_State *state)
{
  return(SubletCreate(SUB_SUBLET_TYPE_TEXT, state));
} /* }}} */

/* LuaAddTeaser {{{ */
static int
LuaAddTeaser(lua_State *state)
{
  return(SubletCreate(SUB_SUBLET_TYPE_TEASER, state));
} /* }}} */

/* LuaAddMeter {{{ */
static int
LuaAddMeter(lua_State *state)
{
  return(SubletCreate(SUB_SUBLET_TYPE_METER, state));
} /* }}} */

/* LuaAddWatch {{{ */
static int
LuaAddWatch(lua_State *state)
{
#ifdef HAVE_SYS_INOTIFY_H
  return(SubletCreate(SUB_SUBLET_TYPE_WATCH, state));
#else
  subUtilLogWarn("add_watch: Inotify is supported on this machine\n");
#endif /* HAVE_SYS_INOTIFY_H */ 
} /* }}} */

/* LuaAddHelper {{{ */
static int
LuaAddHelper(lua_State *state)
{
  return(SubletCreate(SUB_SUBLET_TYPE_HELPER, state));
} /* }}} */

 /** subLuaLoadConfig {{{
  * @brief Load config file from path
  * @param[in] path  Path to the config file
  **/

void
subLuaLoadConfig(const char *path)
{
  int size;
  char buf[100], *face = NULL, *style = NULL;
  DIR *dir = NULL;
  XGCValues gvals;
  XSetWindowAttributes attrs;
  lua_State *configstate = StateNew();
  SubTag *ct = NULL;

  /* Check path */
  if(!path)
    {
      snprintf(buf, sizeof(buf), "%s/.%s", getenv("HOME"), PACKAGE_NAME);
      if((dir = opendir(buf))) 
        {
          snprintf(buf, sizeof(buf), "%s/.%s/config.lua", getenv("HOME"), PACKAGE_NAME);
          closedir(dir);
        }
      else snprintf(buf, sizeof(buf), "%s/config.lua", CONFIG_DIR);
    }
  else snprintf(buf, sizeof(buf), "%s/config.lua", path);

  lua_sethook(configstate, CountHook, LUA_MASKCOUNT, 1000); ///< Limit execution to 1000 calls

  subUtilLogDebug("config=%s\n", buf);
  if(luaL_loadfile(configstate, buf) || lua_pcall(configstate, 0, 0, 0))
    {
      subUtilLogWarn("%s\n", (char *)lua_tostring(configstate, -1));
      lua_close(configstate);
      subUtilLogError("Can't parse config file\n");
    }

  /* Parse and load the font */
  face  = GetString(configstate, "Font", "Face", "fixed");
  style  = GetString(configstate, "Font", "Style", "medium");
  size  = GetNum(configstate, "Font", "Size", 12);

  snprintf(buf, sizeof(buf), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", face, style, size);
  if(!(subtle->xfs = XLoadQueryFont(subtle->disp, buf)))
    {
      subUtilLogWarn("Can't load font `%s', using fixed instead.\n", face);
      subUtilLogDebug("Font: %s\n", buf);
      subtle->xfs = XLoadQueryFont(subtle->disp, "-*-fixed-medium-*-*-*-13-*-*-*-*-*-*-*");
      if(!subtle->xfs) subUtilLogError("Can't load font `fixed`.\n");
    }

  /* Font metrics */
  subtle->fx  = (subtle->xfs->min_bounds.width + subtle->xfs->max_bounds.width) / 2;
  subtle->fy  = subtle->xfs->max_bounds.ascent + subtle->xfs->max_bounds.descent;

  subtle->th  = subtle->xfs->ascent + subtle->xfs->descent + 2;
  subtle->bw  = GetNum(configstate, "Options", "Border",  2);

  /* Read colors from config */
  subtle->colors.font   = ParseColor(configstate, "Font",       "#000000");   
  subtle->colors.border = ParseColor(configstate, "Border",     "#bdbabd");
  subtle->colors.norm   = ParseColor(configstate, "Normal",     "#22aa99");
  subtle->colors.focus  = ParseColor(configstate, "Focus",      "#ffa500");    
  subtle->colors.cover  = ParseColor(configstate, "Shade",      "#FFE6E6");
  subtle->colors.bg     = ParseColor(configstate, "Background", "#336699");

  /* View windows */
  attrs.background_pixel = subtle->colors.norm;
  attrs.save_under       = False;
  attrs.event_mask       = ButtonPressMask|ExposureMask|VisibilityChangeMask;

  subtle->bar.win     = WINNEW(DefaultRootWindow(subtle->disp), 0, 0, DisplayWidth(subtle->disp, DefaultScreen(subtle->disp)), subtle->th, 0,
    CWBackPixel|CWSaveUnder|CWEventMask);
  subtle->bar.views   = XCreateSimpleWindow(subtle->disp, subtle->bar.win, 0, 0, 1, subtle->th, 0, 0, subtle->colors.norm);
  subtle->bar.sublets = XCreateSimpleWindow(subtle->disp, subtle->bar.win, 0, 0, 1, subtle->th, 0, 0, subtle->colors.norm);

  XSelectInput(subtle->disp, subtle->bar.views, ButtonPressMask); 

  XMapWindow(subtle->disp, subtle->bar.views);
  XMapWindow(subtle->disp, subtle->bar.sublets);
  XMapRaised(subtle->disp, subtle->bar.win);

  /* Change GCs */
  gvals.foreground = subtle->colors.border;
  gvals.line_width = subtle->bw;
  XChangeGC(subtle->disp, subtle->gcs.border, GCForeground|GCLineWidth, &gvals);

  gvals.foreground  = subtle->colors.font;
  gvals.font        = subtle->xfs->fid;
  XChangeGC(subtle->disp, subtle->gcs.font, GCForeground|GCFont, &gvals);

  /* Adjust root window */
  attrs.cursor           = subtle->cursors.arrow;
  attrs.background_pixel = subtle->colors.bg;
  attrs.event_mask       = SubstructureRedirectMask|SubstructureNotifyMask|PropertyChangeMask;
  XChangeWindowAttributes(subtle->disp, DefaultRootWindow(subtle->disp), CWCursor|CWBackPixel|CWEventMask, &attrs);
  XClearWindow(subtle->disp, DefaultRootWindow(subtle->disp));

  /* Keys */
  lua_getglobal(configstate, "Keys");
  if(lua_istable(configstate, -1)) 
    { 
      lua_pushnil(configstate);
      while(lua_next(configstate, -2))
        {
          SubKey *k = subKeyNew(lua_tostring(configstate, -2), lua_tostring(configstate, -1)); 
          subArrayPush(subtle->keys, (void *)k);
          lua_pop(configstate, 1);
        }
      subArraySort(subtle->keys, subKeyCompare);
    }
  else printf("No keys found\n");

  /* Default tag */
  ct = subTagNew("default", NULL);
  subArrayPush(subtle->tags, (void *)ct);

  /* Tags */
  lua_getglobal(configstate, "Tags");
  if(lua_istable(configstate, -1)) 
    { 
      lua_pushnil(configstate);
      while(lua_next(configstate, -2))
        {
          SubTag *t = subTagNew((char *)lua_tostring(configstate, -2), (char *)lua_tostring(configstate, -1)); 
          subArrayPush(subtle->tags, (void *)t);
          lua_pop(configstate, 1);
        }
      subTagPublish();
    }
  else printf("No tags found\n");

  /* Views */
  lua_getglobal(configstate, "Views");
  if(lua_istable(configstate, -1)) 
    { 
      lua_pushnil(configstate);
      while(lua_next(configstate, -2))
        {
          SubView *v = subViewNew((char *)lua_tostring(configstate, -2), (char *)lua_tostring(configstate, -1)); 
          subArrayPush(subtle->views, (void *)v);
          lua_pop(configstate, 1);
        }
    }
  else ///< Create default view
    {
      SubView *v = subViewNew("subtle", "default");
      subArrayPush(subtle->views, (void *)v);
      printf("No views found \n");
    }

  if(0 < subtle->views->ndata) 
    {
      SubView *v = VIEW(subtle->views->data[0]);
      v->tags |= (1L << 1); ///< Add default tag to first view
      subEwmhSetCardinals(v->frame, SUB_EWMH_SUBTLE_VIEW_TAGS, (long *)&v->tags, 1); 
      subViewJump(VIEW(subtle->views->data[0])); ///< Jump to first view
      subViewPublish();
    }

  lua_close(configstate);
} /* }}} */

 /** subLuaLoadSublets {{{
  * @brief Load sublets from path
  * @param[in] path  Path to the sublets
  **/

void
subLuaLoadSublets(const char *path)
{
  int i;
  DIR *dir = NULL;
  char buf[100];
  struct dirent *entry = NULL;

  subletstate = StateNew();

#ifdef HAVE_SYS_INOTIFY_H
  if((subtle->notify = inotify_init()) < 0)
    {
      subUtilLogWarn("Can't init inotify\n");
      subUtilLogDebug("Inotify: %s\n", strerror(errno));
    }
  else fcntl(subtle->notify, F_SETFL, O_NONBLOCK);
#endif /* HAVE_SYS_INOTIFY_H */

  /* Check path */
  if(!path)
    {
      snprintf(buf, sizeof(buf), "%s/.%s/sublets", getenv("HOME"), PACKAGE_NAME);
      if((dir = opendir(buf))) closedir(dir);
      else snprintf(buf, sizeof(buf), "%s", SUBLET_DIR);
    }
  else snprintf(buf, sizeof(buf), "%s", path);

  subUtilLogDebug("path=%s\n", buf);

  /* Push functions on the stack */
  lua_newtable(subletstate);
  SetField(subletstate, "version",    LUA_TSTRING,    PACKAGE_VERSION);
  SetField(subletstate, "add_text",   LUA_TFUNCTION,  LuaAddText);
  SetField(subletstate, "add_teaser", LUA_TFUNCTION,  LuaAddTeaser);
  SetField(subletstate, "add_meter",  LUA_TFUNCTION,  LuaAddMeter);
  SetField(subletstate, "add_watch",  LUA_TFUNCTION,  LuaAddWatch);
  SetField(subletstate, "add_helper", LUA_TFUNCTION,  LuaAddHelper);

  lua_setglobal(subletstate, PACKAGE_NAME);

  if((dir = opendir(buf)))
    {
      while((entry = readdir(dir)))
        {
          if(!fnmatch("*.lua", entry->d_name, FNM_PATHNAME))
            {
              char file[150];
              snprintf(file, sizeof(file), "%s/%s", buf, entry->d_name);
              luaL_loadfile(subletstate, file);
              lua_pcall(subletstate, 0, 0, 0);
            }
        }
      closedir(dir);
      
      /* Preserve load order */
      for(i = 0; i < subtle->sublets->ndata - 1; i++) 
        SUBLET(subtle->sublets->data[i])->next = SUBLET(subtle->sublets->data[i + 1]);
      subtle->sublet = SUBLET(subtle->sublets->data[0]);

      subArraySort(subtle->sublets, subSubletCompare);
      subSubletConfigure();
    }
  else subUtilLogWarn("Can't find any loadable sublets\n"); 
} /* }}} */

 /** subLuaCall {{{
  * @brief Carefully call a Lua script
  * @param s  A #SubClient
  **/

void
subLuaCall(SubSublet *s)
{
  assert(s);

  if(s->flags & SUB_SUBLET_TYPE_HELPER) return; ///< Helpers need no updates

  /* Setup environment */
  lua_sethook(subletstate, CountHook, LUA_MASKCOUNT, 1000); ///< Limit execution to 1000 calls
  lua_settop(subletstate, 0);
  lua_rawgeti(subletstate, LUA_REGISTRYINDEX, s->ref);

  if(lua_pcall(subletstate, 0, 1, 0))
    {
      /* Kill faulty sublets */
      subUtilLogWarn("Call of sublet #%d failed\n", s->ref);
      subUtilLogDebug("%s\n", (char *)lua_tostring(subletstate, -1));
      subArrayPop(subtle->sublets, (void *)s);
      return;
    }

  switch(lua_type(subletstate, -1))
    {
      case LUA_TSTRING:
        if(s->string) free(s->string);
        s->string = strdup((char *)lua_tostring(subletstate, -1));
        s->width  = strlen(s->string);
        break;
      case LUA_TNUMBER: 
        s->number = (int)lua_tonumber(subletstate, -1);  
        s->width  = 63; ///< Magic number
        break;
      case LUA_TNIL:     
        subUtilLogWarn("Sublet #%d returned no usuable value\n", s->ref);  

        if(s->flags & SUB_SUBLET_TYPE_METER) 
          {
            s->number = 0;
            s->width  = 63; ///< Magic number
          }
        else
          {
            if(s->string) free(s->string);
            s->string = strdup("subtle");
            s->width  = 6;
          }        
        break;
      default:
        subUtilLogDebug("Sublet #%d returned unkown type %s\n", s->ref, lua_typename(subletstate, -1));
        lua_pop(subletstate, -1);
    }
} /* }}} */

 /** subLuaKill {{{
  * @brief Close Lua state
  **/

void
subLuaKill(void)
{
  if(subletstate) lua_close(subletstate);

#ifdef HAVE_SYS_INOTIFY_H
  if(subtle->notify) close(subtle->notify);
#endif /* HAVE_SYS_INOTIFY_H */

  subUtilLogDebug("kill=lua\n");
} /* }}} */
