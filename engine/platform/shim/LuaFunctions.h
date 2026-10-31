/* dsfplatform shim for LuaFunctions.h: LuaFunction() registrations compile to nothing. */
#ifndef LUA_FUNCTIONS_H
#define LUA_FUNCTIONS_H
#include "LuaManager.h"
#define LuaFunction( func, expr )
#endif
