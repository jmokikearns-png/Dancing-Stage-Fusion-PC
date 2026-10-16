/* dsfplatform shim for LuaManager.h: no Lua in the platform layer.
 * Declarations only, so that the Lua binding blocks at the end of some original
 * files still parse; nothing here is ever defined or called. */
#ifndef LUA_MANAGER_H
#define LUA_MANAGER_H

struct lua_State;
#define LUA_GLOBALSINDEX (-10001)
void lua_pushstring( lua_State *L, const char *s );
void lua_settable( lua_State *L, int idx );

class LuaManager
{
public:
	template<class F> void Register( F ) { }
	template<class V> void SetGlobal( const CString &sName, V v ) { }
};
extern LuaManager *LUA;

#define REGISTER_WITH_LUA_FUNCTION( Fn )

#endif
