#include"lua-5.4.4/src/lua.h"
#include"lua-5.4.4/src/lualib.h"
void LoadLibraries(lua_State*L){// this function is intended as a way of adding libraries to the preprocessor
	luaL_openlibs(L);
}
