/* This file is licensed under the "MIT License" Copyright (c) 2023 Halalaluyafail3. See the file LICENSE or go to the following for full license details: https://github.com/Halalaluyafail3/LuaPreprocessor/blob/main/LICENSE */
#include"lua.h"
#include"lualib.h"
void LoadLibraries(lua_State*L){/* this function is intended as a way of adding libraries to the preprocessor */
	luaL_openlibs(L);
}
