#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
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

static void
BindKey(const char *key,
	const char *value)
{
	int n = 0;
	KeySym *keys = NULL;
	char *tok = strtok((char *) value, "+");

	while(tok)
		{
			keys 				= (KeySym *)realloc(keys, ++n * sizeof(KeySym));
			keys[n - 1] = XStringToKeysym(tok);
			tok					= strtok(NULL, "+");
		}
	XRebindKeysym(d->dpy, keys[n - 1], keys, n - 1, key, strlen(key));
	subLogDebug("Adding keychain: name=%s\n", key);
	free(keys);
}

 /**
	* Load config file
	* @param path Path to the config file
	**/

void
subLuaLoadConfig(const char *path)
{
	int size;
	char font[50], *face = NULL, *style = NULL;
	DIR *dir = NULL;
	char full[100];
	XGCValues gvals;
	XSetWindowAttributes attrs;
	lua_State *configstate = NULL;

	configstate = StateNew();

	/* Check whether relative or absolute path */
	if(path)
		{
			if(!strncmp(path, "/", 1)) snprintf(full, sizeof(full), "%s", path);
			else
				{
					snprintf(full, sizeof(full), "%s/.%s", getenv("HOME"), PACKAGE_NAME);
					if((dir = opendir(full)))
						{
							strncat(full, path, sizeof(path));
							closedir(dir);
						}
					else snprintf(full, sizeof(full), "/etc/%s/%s", PACKAGE_NAME, path);
				}
		}
	else
		{
			snprintf(full, sizeof(full), "%s/.%s", getenv("HOME"), PACKAGE_NAME);
			if((dir = opendir(full))) 
				{
					snprintf(full, sizeof(full), "%s/.%s/config.lua", getenv("HOME"), PACKAGE_NAME);
					closedir(dir);
				}
			else snprintf(full, sizeof(full), "/etc/%s/config.lua", PACKAGE_NAME);
		}

	subLogDebug("Reading `%s'\n", full);
	if(luaL_loadfile(configstate, full) || lua_pcall(configstate, 0, 0, 0))
		{
			subLogDebug("%s\n", (char *)lua_tostring(configstate, -1));
			lua_close(configstate);
			subLogError("Can't load config file `%s'.\n", full);
		}

	/* Parse and load the font */
	face	= GetString(configstate, "font", "face", "fixed");
	style	= GetString(configstate, "font", "style", "medium");
	size	= GetNum(configstate, "font", "size", 12);

	snprintf(font, sizeof(font), "-*-%s-%s-*-*-*-%d-*-*-*-*-*-*-*", face, style, size);
	if(!(d->xfs = XLoadQueryFont(d->dpy, font)))
		{
			subLogWarn("Can't load font `%s', using fixed instead.\n", face);
			subLogDebug("Font: %s\n", font);
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
	d->colors.shade		= ParseColor(configstate, "shade",			"#FFE6E6");
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
					BindKey(lua_tostring(configstate, -2), lua_tostring(configstate, -1));
					lua_pop(configstate, 1);
				}
		}

	lua_close(configstate);
}

static int
l_add(lua_State *state)
{
	subSubletAdd((char *)lua_tostring(state, 1), (int)lua_tonumber(state, 2), (int)lua_tonumber(state, 3));
}

 /**
	* Load sublets
	* @param path Path to the sublets
	**/

void
subLuaLoadSublets(const char *path)
{
	DIR *dir = NULL;
	char full[100];
	struct dirent *rent = NULL;

	state = StateNew();

	if(path) snprintf(full, sizeof(full), "%s", full);
	else snprintf(full, sizeof(full), "%s/.%s/sublets", getenv("HOME"), PACKAGE_NAME);

	/* Put functions onto stack */
	lua_pushcfunction(state, l_add);
	lua_setglobal(state, "add");

	if((dir = opendir(full)))
		{
			while((rent = readdir(dir)))
				{
					if(!fnmatch("*.lua", rent->d_name, FNM_PATHNAME))
						{
							snprintf(full, sizeof(full), "%s/.%s/sublets/%s", getenv("HOME"), PACKAGE_NAME, rent->d_name);
							luaL_loadfile(state, full);
							lua_pcall(state, 0, 0, 0);
						}
				}
			closedir(dir);
		}
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
	* Call a Lua function
	* @param function Function to call
	* @param data Storage for the function return value
	* @return Returns either a nonzero on succces or otherwise zero.
	**/

int
subLuaCall(const char *function,
	char **data)
{
	lua_getglobal(state, function);
	if(lua_pcall(state, 0, 1, 0))
		{
			subLogWarn("Failed to call `%s'.\n", function);
			return(0);
		}
	switch(lua_type(state, -1))
		{
			case LUA_TSTRING:
			case LUA_TNUMBER:
				if(*data) free(*data);
				*data = strdup((char *)lua_tostring(state, -1));
				break;
			default:
				subLogWarn("Sublet %s has no (valid) return value.\n", function);
				subLogDebug("Unhandled return type `%s'.", lua_typename(state, -1));
		}
	return(1);
}
