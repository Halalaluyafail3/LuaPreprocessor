#ifndef NUMERIC_H
	#define NUMERIC_H
	#include<stddef.h>
	#include<stdbool.h>
	#include"lua-5.4.4/src/lua.h"
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
