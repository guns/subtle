
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* See the COPYING file for the license in the latest tarball.
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
}

#define GET_GLOBAL(configstate) do { \
	lua_getglobal(configstate, table); \
	if(lua_istable(configstate, -1)) \
		{ \
			lua_pushstring(configstate, field); \
			lua_gettable(configstate, -2); \
		} \
} while(0)

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
}

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
}

static double
ParseColor(lua_State *configstate,
	const char *field,
	char *fallback)
{
	XColor color;
	char *name = GetString(configstate, "Colors", field, fallback);
	color.pixel = 0;

	if(!XParseColor(d->disp, DefaultColormap(d->disp, DefaultScreen(d->disp)), name, &color))
		{
			subUtilLogWarn("Can't load color '%s'.\n", name);
		}
	else if(!XAllocColor(d->disp, DefaultColormap(d->disp, DefaultScreen(d->disp)), &color))
		subUtilLogWarn("Can't alloc color '%s'.\n", name);
	return(color.pixel);
}

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
}

 /**
	* Load config file
	* @param path Path to the config file
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

	lua_sethook(configstate, CountHook, LUA_MASKCOUNT, 1000);

	subUtilLogDebug("Reading `%s'\n", buf);
	if(luaL_loadfile(configstate, buf) || lua_pcall(configstate, 0, 0, 0))
		{
			subUtilLogWarn("%s\n", (char *)lua_tostring(configstate, -1));
			lua_close(configstate);
			subUtilLogError("Can't parse config file\n");
		}

	/* Parse and load the font */
	face	= GetString(configstate, "Font", "Face", "fixed");
	style	= GetString(configstate, "Font", "Style", "medium");
	size	= GetNum(configstate, "Font", "Size", 12);

	snprintf(buf, sizeof(buf), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", face, style, size);
	if(!(d->xfs = XLoadQueryFont(d->disp, buf)))
		{
			subUtilLogWarn("Can't load font `%s', using fixed instead.\n", face);
			subUtilLogDebug("Font: %s\n", buf);
			d->xfs = XLoadQueryFont(d->disp, "-*-fixed-medium-*-*-*-12-*-*-*-*-*-*-*");
			if(!d->xfs) subUtilLogError("Can't load font `fixed`.\n");
		}

	/* Font metrics */
	d->fx	= (d->xfs->min_bounds.width + d->xfs->max_bounds.width) / 2;
	d->fy	= d->xfs->max_bounds.ascent + d->xfs->max_bounds.descent;

	d->th	= d->xfs->ascent + d->xfs->descent + 2;
	d->bw	= GetNum(configstate, "Options", "Border",	2);

	/* Read colors from config */
	d->colors.font		= ParseColor(configstate, "Font",				"#000000"); 	
	d->colors.border	= ParseColor(configstate, "Border",			"#bdbabd");
	d->colors.norm		= ParseColor(configstate, "Normal",			"#22aa99");
	d->colors.focus		= ParseColor(configstate, "Focus",			"#ffa500");		
	d->colors.cover		= ParseColor(configstate, "Collapse",		"#FFE6E6");
	d->colors.bg			= ParseColor(configstate, "Background",	"#336699");

	/* Change GCs */
	gvals.foreground	= d->colors.border;
	gvals.line_width	= d->bw;
	XChangeGC(d->disp, d->gcs.border, GCForeground|GCLineWidth, &gvals);

	gvals.foreground	= d->colors.font;
	gvals.font				= d->xfs->fid;
	XChangeGC(d->disp, d->gcs.font, GCForeground|GCFont, &gvals);

	/* Adjust root window */
	attrs.cursor						= d->cursors.arrow;
	attrs.background_pixel	= d->colors.bg;
	attrs.event_mask				= SubstructureRedirectMask|SubstructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(d->disp, DefaultRootWindow(d->disp), CWCursor|CWBackPixel|CWEventMask, &attrs);
	XClearWindow(d->disp, DefaultRootWindow(d->disp));

	/* Keys */
	lua_getglobal(configstate, "Keys");
	if(lua_istable(configstate, -1)) 
		{ 
			lua_pushnil(configstate);
			while(lua_next(configstate, -2))
				{
					subKeyNew(lua_tostring(configstate, -2), lua_tostring(configstate, -1)); 
					lua_pop(configstate, 1);
				}
		}
	
	/* Rules */
	d->cv = subViewNew("root", NULL);

	lua_getglobal(configstate, "Rules");
	if(lua_istable(configstate, -1)) 
		{ 
			lua_pushnil(configstate);
			while(lua_next(configstate, -2))
				{
					if(lua_istable(configstate, -1))
						{ 
							char *tags = NULL, *view = (char *)lua_tostring(configstate, -2);

							lua_pushnil(configstate);
							while(lua_next(configstate, -2))
								{
									if(!tags) tags = strdup((char *)lua_tostring(configstate, -1));
									else
										{
											char *tag = (char *)lua_tostring(configstate, -1);

											tags = (char *)subUtilRealloc(tags, sizeof(char) * (strlen(tags) + strlen(tag) + 2));
											sprintf(tags, "%s|%s", tags, tag);
										}
									lua_pop(configstate, 1);
								}
							if(tags) subViewNew(view, tags);;
							free(tags);
						}
					lua_pop(configstate, 1);
				}
		}
	lua_close(configstate);
}

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
			case LUA_TSTRING:   lua_pushstring(state, va_arg(ap, char *));						break;
			case LUA_TFUNCTION: lua_pushcfunction(state, va_arg(ap, lua_CFunction));  break;
			case LUA_TNUMBER:   lua_pushnumber(state, va_arg(ap, double));						break;
			case LUA_TBOOLEAN:  lua_pushboolean(state, va_arg(ap, int));							break;
		}
	lua_settable(state, -3);
	va_end(ap);
}

static int
PrepareSublet(int type, 
	lua_State *state)
{
	char *string = (char *)lua_tostring(state, 2), *watch = NULL;
	int ref, interval = 0;
	
	switch(type)
		{
			case SUB_SUBLET_TYPE_TEXT: break;

#ifdef HAVE_SYS_INOTIFY_H
			case SUB_SUBLET_TYPE_WATCH: watch = (char *)lua_tostring(state, 3);	break;
#endif /* HAVE_SYS_INOTIFY_H */

			default: interval = (int)lua_tonumber(state, 3); break;
		}

	if(string)
		{
			/* Check functions with a namespace */
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

			ref = luaL_ref(state, LUA_REGISTRYINDEX);
			if(ref) subSubletNew(type, string, ref, interval, watch);
		}

	return(1); /* Make the compiler happy */
}

/* Sublet functions */
static int
LuaAddText(lua_State *state)
{
	return(PrepareSublet(SUB_SUBLET_TYPE_TEXT, state));
}

static int
LuaAddTeaser(lua_State *state)
{
	return(PrepareSublet(SUB_SUBLET_TYPE_TEASER, state));
}

static int
LuaAddMeter(lua_State *state)
{
	return(PrepareSublet(SUB_SUBLET_TYPE_METER, state));
}

static int
LuaAddWatch(lua_State *state)
{
#ifdef HAVE_SYS_INOTIFY_H
	return(PrepareSublet(SUB_SUBLET_TYPE_WATCH, state));
#else
	subUtilLogWarn("add_watch: Inotify is supported on this machine\n");
#endif /* HAVE_SYS_INOTIFY_H */
}

 /**
	* Load sublets
	* @param path Path to the sublets
	**/

void
subLuaLoadSublets(const char *path)
{
	DIR *dir = NULL;
	char buf[100];
	struct dirent *entry = NULL;

	subletstate = StateNew();

#ifdef HAVE_SYS_INOTIFY_H
	if((d->notify = inotify_init()) < 0)
		{
			subUtilLogWarn("Can't init inotify\n");
			subUtilLogDebug("Inotify: %s\n", strerror(errno));
		}
	else fcntl(d->notify, F_SETFL, O_NONBLOCK);
#endif /* HAVE_SYS_INOTIFY_H */

	/* Check path */
	if(!path)
		{
			snprintf(buf, sizeof(buf), "%s/.%s/sublets", getenv("HOME"), PACKAGE_NAME);
			if((dir = opendir(buf))) closedir(dir);
			else snprintf(buf, sizeof(buf), "%s", SUBLET_DIR);
		}
	else snprintf(buf, sizeof(buf), "%s", path);

	/* Bar windows */
	d->bar.win			= XCreateSimpleWindow(d->disp, DefaultRootWindow(d->disp), 0, 0, 
		DisplayWidth(d->disp, DefaultScreen(d->disp)), d->th, 0, 0, d->colors.norm);
	d->bar.views		= XCreateSimpleWindow(d->disp, d->bar.win, 0, 0, 1, d->th, 0, 0, d->colors.norm);
	d->bar.sublets	= XCreateSimpleWindow(d->disp, d->bar.win, 0, 0, 1, d->th, 0, 0, d->colors.norm);

	XSetWindowBackground(d->disp, d->bar.win, d->colors.norm);
	XSetWindowBackground(d->disp, d->bar.views, d->colors.norm);
	XSetWindowBackground(d->disp, d->bar.sublets, d->colors.norm);

	XSelectInput(d->disp, d->bar.views, ButtonPressMask); 

	XMapWindow(d->disp, d->bar.views);
	XMapWindow(d->disp, d->bar.sublets);

	/* Push functions on the stack */
	lua_newtable(subletstate);
	SetField(subletstate, "version",		LUA_TSTRING,		PACKAGE_VERSION);
	SetField(subletstate, "add_text",		LUA_TFUNCTION,	LuaAddText);
	SetField(subletstate, "add_teaser",	LUA_TFUNCTION,	LuaAddTeaser);
	SetField(subletstate, "add_meter",	LUA_TFUNCTION,	LuaAddMeter);
	SetField(subletstate,	"add_watch",	LUA_TFUNCTION,	LuaAddWatch);

	lua_setglobal(subletstate, PACKAGE_NAME);

	if((dir = opendir(buf)))
		{
			while((entry = readdir(dir)))
				{
					if(!fnmatch("*.lua", entry->d_name, FNM_PATHNAME))
						{
							snprintf(buf, sizeof(buf), "%s/.%s/sublets/%s", getenv("HOME"), PACKAGE_NAME, entry->d_name);
							luaL_loadfile(subletstate, buf);
							lua_pcall(subletstate, 0, 0, 0);
						}
				}
			closedir(dir);
				
			subViewConfigure();
			subSubletConfigure();
		}
	else subUtilLogWarn("Can't find any sublets to load\n");
}

 /**
	* Close Lua state
	**/

void
subLuaKill(void)
{
	if(subletstate) lua_close(subletstate);

#ifdef HAVE_SYS_INOTIFY_H
	if(d->notify) close(d->notify);
#endif /* HAVE_SYS_INOTIFY_H */

}

 /**
	* Call a Lua script
	* @param w A #SubWin
	**/

void
subLuaCall(SubSublet *s)
{
	if(s)
		{
			lua_sethook(subletstate, CountHook, LUA_MASKCOUNT, 1000);

			lua_settop(subletstate, 0);
			lua_rawgeti(subletstate, LUA_REGISTRYINDEX, s->ref);

			if(lua_pcall(subletstate, 0, 1, 0))
				{
					/* Fail check */
					if(s->flags & SUB_SUBLET_FAIL_SECOND) s->flags |= SUB_SUBLET_FAIL_THIRD;
					else if(s->flags & SUB_SUBLET_FAIL_FIRST) s->flags |= SUB_SUBLET_FAIL_SECOND;
					else s->flags |= SUB_SUBLET_FAIL_FIRST;

					subUtilLogWarn("Error (%d/3) in sublet #%d:\n", 
						(s->flags & SUB_SUBLET_FAIL_THIRD) ? 3 : (s->flags & SUB_SUBLET_FAIL_SECOND ? 2 : 1), s->ref);
					subUtilLogWarn("%s\n", (char *)lua_tostring(subletstate, -1));

					if(s->flags & SUB_SUBLET_FAIL_THIRD) subSubletDelete(s);
					else
						{
							if(s->flags & SUB_SUBLET_TYPE_METER) s->number = 0;
							else
								{
									if(s->string) free(s->string);
									s->string = strdup("subtle");
								}
						}
					return;
				}

			switch(lua_type(subletstate, -1))
				{
					case LUA_TSTRING:
						if(s->string) free(s->string);
						s->string = strdup((char *)lua_tostring(subletstate, -1));
						break;
					case LUA_TNUMBER: s->number = (int)lua_tonumber(subletstate, -1);	break;
					case LUA_TNIL: 		
						subUtilLogWarn("Sublet #%d returns no usuable value\n", s->ref);	

						if(s->flags & SUB_SUBLET_TYPE_METER) s->number = 0;
						else
							{
								if(s->string) free(s->string);
								s->string = strdup("subtle");
							}
						break;
					default:
						subUtilLogDebug("Sublet #%d returned unkown type %s\n", s->ref, lua_typename(subletstate, -1));
						lua_pop(subletstate, -1);
				}
		}

}
