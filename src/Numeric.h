/* This file is licensed under the "MIT License" Copyright (c) 2023 Halalaluyafail3. See the file LICENSE or go to the following for full license details: https://github.com/Halalaluyafail3/LuaPreprocessor/blob/master/LICENSE */
#ifndef NUMERIC_H
	#define NUMERIC_H
	#include<stddef.h>
	#include<stdbool.h>
	#include"lua.h"
	typedef struct FloatOrInteger FloatOrInteger;
	struct FloatOrInteger{
		union{
			lua_Integer Integer;
			lua_Number Float;
		};
		bool IsInteger;
	};
	bool IntegerFloatFitsInInteger(lua_Number);
	bool FloatFitsInInteger(lua_Number);
	size_t StringToFloatOrInteger(const char*restrict,size_t,FloatOrInteger*restrict);
#endif
