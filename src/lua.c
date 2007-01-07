#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "subtle.h"

static lua_State *state = NULL;

static int
l_add(lua_State *state)
{
	subSubletAdd((char *)lua_tostring(state, 1), (int)lua_tonumber(state, 2), (int)lua_tonumber(state, 3));
}

 /**
	* Open a Lua state.
	* @return Returns either nonzero on success otherwise zero.
	**/

int
subLuaNew(void)
{
	if(!(state = luaL_newstate()))
		{
			subLogError("Can't open lua state.\n");
			subLogDebug("%s\n", (char *)lua_tostring(state, -1));
			return(0);
		}
	luaL_openlibs(state);

	/* Put functions onto stack */
	lua_pushcfunction(state, l_add);
	lua_setglobal(state, "add");

	return(1);
}

 /**
	* Close a Lua state.
	**/
	
void
subLuaKill(void)
{
	lua_close(state);
}

 /**
	* Load a file into the Lua state.
	* @param path Path to the file
	* @param file File to load
	* @return Returns either nonzero on success or otherwise zero.
	**/

int
subLuaLoad(const char *path,
	const char *file)
{
	char full[100];

	if(path) snprintf(full, sizeof(full), "%s/%s", path, file);
	else snprintf(full, sizeof(full), "%s/.subtle/%s", getenv("HOME"), file);

	subLogDebug("Reading `%s'\n", full);
	if(luaL_loadfile(state, full))
		{
			subLogError("Can't read file `%s'.\n", full);
			subLogDebug("%s\n", (char *) lua_tostring(state, -1));
			return(0);
		}
	return(!lua_pcall(state, 0, 0, 0));
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
				subLogWarn("Docklet %s has no (valid) return value.\n", function);
				subLogDebug("Unhandled return type `%s'.", lua_typename(state, -1));
		}
	return(1);
}

 /**
	* Do foreach field in the table.
	* @param table Table name
	* @param func Table function
	**/

void
subLuaForeach(const char *table,
	SubLuaFunction function)
{
	lua_getglobal(state, table);
	if(!lua_istable(state, -1)) 
		{ 
			subLogDebug("`%s' is not a table.\n", table); 
			return; 
		} 
	lua_pushnil(state);
	while(lua_next(state, -2))
		{
			function(lua_tostring(state, -2), lua_tostring(state, -1));
			lua_pop(state, 1);
		}
}
	
#define GET_GLOBAL do { \
	lua_getglobal(state, table); \
	if(!lua_istable(state, -1)) \
		{ \
			subLogDebug("`%s' is not a table.\n", table); \
			return(fallback); \
		} \
	lua_pushstring(state, field); \
	lua_gettable(state, -2); \
} while(0)

 /**
	* Get an int from a table.
	* @param table Table name
	* @param field Table field
	* @return Returns either a found value or otherwise zero.
	**/

int
subLuaGetNum(const char *table,
	const char *field,
	int fallback)
{
	GET_GLOBAL;
	if(!lua_isnumber(state, -1))
		{
			subLogDebug("Expected double, got `%s' for `%s'.\n", lua_typename(state, -1), field);
			return(fallback);
		}
	return((int)lua_tonumber(state, -1));
}

 /**
	* Get a string from a table.
	* @param table Table name
	* @param field Table field
	* @return Returns either a found value or otherwise NULL.
	**/

char *
subLuaGetString(const char *table,
	const char *field,
	char *fallback)
{
	GET_GLOBAL;
	if(!lua_isstring(state, -1))
		{
			subLogDebug("Expected string, got `%s' for `%s'.\n", lua_typename(state, -1), field);
			return(fallback);
		}
	return((char *) lua_tostring(state, -1));
}
