
 /**
	* subtle - window manager
	* Copyright (c) 2005-2007 Christoph Kappel
	*
	* This program is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
	* (at your option) any later version.
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	* GNU General Public License for more details.
	*
	* You should have received a copy of the GNU General Public License along
	* with this program; if not, write to the Free Software Foundation, Inc.,
	* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
	**/

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>

#include "subtle.h"

static lua_State *state = NULL;

static lua_State *
StateNew(void)
{
	lua_State *state = NULL;
	if(!(state = luaL_newstate()))
		{
			subLogDebug("%s\n", (char *)lua_tostring(state, -1));
			subLogError("Can't open lua state.\n");
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
			subLogDebug("Expected double, got `%s' for `%s'.\n", lua_typename(configstate, -1), field);
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
			subLogDebug("Expected string, got `%s' for `%s'.\n", lua_typename(configstate, -1), field);
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
	char *name = GetString(configstate, "colors", field, fallback);
	color.pixel = 0;

	if(!XParseColor(d->dpy, DefaultColormap(d->dpy, DefaultScreen(d->dpy)), name, &color))
		{
			subLogWarn("Can't load color '%s'.\n", name);
		}
	else if(!XAllocColor(d->dpy, DefaultColormap(d->dpy, DefaultScreen(d->dpy)), &color))
		subLogWarn("Can't alloc color '%s'.\n", name);
	return(color.pixel);
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

	subLogDebug("Reading `%s'\n", buf);
	if(luaL_loadfile(configstate, buf) || lua_pcall(configstate, 0, 0, 0))
		{
			subLogDebug("%s\n", (char *)lua_tostring(configstate, -1));
			lua_close(configstate);
			subLogError("Can't load config file `%s'.\n", buf);
		}

	/* Parse and load the font */
	face	= GetString(configstate, "font", "face", "fixed");
	style	= GetString(configstate, "font", "style", "medium");
	size	= GetNum(configstate, "font", "size", 12);

	snprintf(buf, sizeof(buf), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", face, style, size);
	if(!(d->xfs = XLoadQueryFont(d->dpy, buf)))
		{
			subLogWarn("Can't load font `%s', using fixed instead.\n", face);
			subLogDebug("Font: %s\n", buf);
			d->xfs = XLoadQueryFont(d->dpy, "-*-fixed-medium-*-*-*-12-*-*-*-*-*-*-*");
		}

	/* Font metrics */
	d->fx	= (d->xfs->min_bounds.width + d->xfs->max_bounds.width) / 2;
	d->fy	= d->xfs->max_bounds.ascent + d->xfs->max_bounds.descent;

	d->th	= d->xfs->ascent + d->xfs->descent + 2;
	d->bw	= GetNum(configstate, "options", "border",	2);

	/* Read colors from config */
	d->colors.font		= ParseColor(configstate, "font",				"#000000"); 	
	d->colors.border	= ParseColor(configstate, "border",			"#bdbabd");
	d->colors.norm		= ParseColor(configstate, "normal",			"#22aa99");
	d->colors.focus		= ParseColor(configstate, "focus",			"#ffa500");		
	d->colors.cover		= ParseColor(configstate, "shade",			"#FFE6E6");
	d->colors.bg			= ParseColor(configstate, "background",	"#336699");

	/* Change GCs */
	gvals.foreground	= d->colors.border;
	gvals.line_width	= d->bw;
	XChangeGC(d->dpy, d->gcs.border, GCForeground|GCLineWidth, &gvals);

	gvals.foreground	= d->colors.font;
	gvals.font				= d->xfs->fid;
	XChangeGC(d->dpy, d->gcs.font, GCForeground|GCFont, &gvals);

	/* Adjust root window */
	attrs.cursor						= d->cursors.arrow;
	attrs.background_pixel	= d->colors.bg;
	attrs.event_mask				= SubstructureRedirectMask|SubstructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(d->dpy, DefaultRootWindow(d->dpy), CWCursor|CWBackPixel|CWEventMask, &attrs);
	XClearWindow(d->dpy, DefaultRootWindow(d->dpy));

	/* Bind key chains */
	lua_getglobal(configstate, "keys");
	if(lua_istable(configstate, -1)) 
		{ 
			lua_pushnil(configstate);
			while(lua_next(configstate, -2))
				{
					subKeyParseChain(lua_tostring(configstate, -2), lua_tostring(configstate, -1));
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
	char *string = (char *)lua_tostring(state, 2);
	int ref, interval = (int)lua_tonumber(state, 3), width = (int)lua_tonumber(state, 4);

	if(string && interval && width)
		{
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
			if(ref) subSubletNew(type, ref, interval, width);

			printf("Loaded sublet %s (%d)\n", string, interval);
			subLogDebug("Sublet: name=%s, ref=%d, interval=%d, width=%d\n", string, ref, interval, width);
		}

	return(1); // Make the compiler happy
}

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

	state = StateNew();

	if(path) snprintf(buf, sizeof(buf), "%s", buf);
	else snprintf(buf, sizeof(buf), "%s/.%s/sublets", getenv("HOME"), PACKAGE_NAME);

	/* Put functions onto stack */
	lua_newtable(state);
	SetField(state, "version",		LUA_TSTRING,		PACKAGE_VERSION);
	SetField(state, "add_text",		LUA_TFUNCTION,	LuaAddText);
	SetField(state, "add_teaser",	LUA_TFUNCTION,	LuaAddTeaser);
	SetField(state, "add_meter",	LUA_TFUNCTION,	LuaAddMeter);
	lua_setglobal(state, PACKAGE_NAME);

	if((dir = opendir(buf)))
		{
			while((entry = readdir(dir)))
				{
					if(!fnmatch("*.lua", entry->d_name, FNM_PATHNAME))
						{
							snprintf(buf, sizeof(buf), "%s/.%s/sublets/%s", getenv("HOME"), PACKAGE_NAME, entry->d_name);
							luaL_loadfile(state, buf);
							lua_pcall(state, 0, 0, 0);
						}
				}
			closedir(dir);
		}
	subScreenConfigure();
}

 /**
	* Close Lua state
	**/

void
subLuaKill(void)
{
	if(state) lua_close(state);
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
			lua_settop(state, 0);
			lua_rawgeti(state, LUA_REGISTRYINDEX, s->ref);
			if(lua_pcall(state, 0, 1, 0))
				{
					if(s->flags & SUB_SUBLET_FAIL_THIRD)
						{
							subLogWarn("Unloaded sublet (#%d) after 3 failed attempts\n", s->ref);
							subSubletDelete(s);
							return;
						}				
					else if(s->flags & SUB_SUBLET_FAIL_SECOND) s->flags |= SUB_SUBLET_FAIL_THIRD;
					else if(s->flags & SUB_SUBLET_FAIL_FIRST) s->flags |= SUB_SUBLET_FAIL_SECOND;

					subLogWarn("Failed attempt #%d to call sublet (#%d).\n", 
						(s->flags & SUB_SUBLET_FAIL_SECOND) ? 2 : 1, s->ref);
				}

			switch(lua_type(state, -1))
				{
					case LUA_TNIL: 		subLogWarn("Sublet (#%d) does not return any usuable value\n", s->ref);	break;
					case LUA_TNUMBER: s->number = (int)lua_tonumber(state, -1);																break;
					case LUA_TSTRING:
						if(s->string) free(s->string);
						s->string = strdup((char *)lua_tostring(state, -1));
						break;
					default:
						subLogDebug("Sublet (#%d) returned unkown type %s\n", s->ref, lua_typename(state, -1));
						lua_pop(state, -1);
				}
		}

}
