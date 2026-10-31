/* dsfplatform shim for LuaBinding.h: class bindings (Luna<T>) compile to nothing.
 * The binding templates in the original files are never instantiated. */
#ifndef LUABINDING_H
#define LUABINDING_H
#include "LuaManager.h"

template<class T> class Luna
{
public:
	static void Register( lua_State *L ) { }
};

namespace LuaHelpers
{
	template<class T> void CreateTableFromArray( const vector<T> &aIn, lua_State *L );
}

#define ADD_METHOD( method_name )
#define LUA_REGISTER_CLASS( T )
#endif
