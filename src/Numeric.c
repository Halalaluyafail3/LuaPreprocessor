/* This file is licensed under the "MIT License" Copyright (c) 2023 Halalaluyafail3. See the file LICENSE or go to the following for full license details: https://github.com/Halalaluyafail3/LuaPreprocessor/blob/main/LICENSE */
#include<float.h>
#include<stddef.h>
#include<tgmath.h>
#include<stdbool.h>
#include"Numeric.h"
#include"NoLocale.h"
#include"lua.h"
/* assume lua_Integer to lua_Number conversions never fail, specifically LUA_MININTEGER must not be outside the range of values representable by lua_Number */
/* this assumption is also made by Lua, which will do lua_Integer to lua_Number conversions without checking the range first */
bool IntegerFloatFitsInInteger(lua_Number IntegerFloat){/* integers, infinities, and NaNs are ok as arguments */
	return IntegerFloat>=LUA_MININTEGER&&IntegerFloat<-(lua_Number)LUA_MININTEGER;
}
bool FloatFitsInInteger(lua_Number Float){
	lua_Number Floored=floor(Float);
	return Float==Floored&&IntegerFloatFitsInInteger(Floored);
}
/* returns the amount of characters read, this will be zero in case of failure */
/* a string like "0E?" will result in 1, as it was able to parse it while ignoring the invalid exponent */
size_t StringToFloatOrInteger(const char*restrict String,size_t Length,FloatOrInteger*restrict Output){
	if(!Length){
		return 0;
	}
	char Reading=*String;
	size_t Index;
	bool IsPositive=Reading!='-';
	if(!IsPositive||Reading=='+'){
		if(Length==1){
			return 0;
		}
		Reading=String[Index=1];
	}else{
		Index=0;
	}
	size_t Start;
	lua_Number Exponent;
	size_t DigitsAmount;
	#define READ_TO_FIRST_DIGIT(...)\
		for(;;){\
			if(++ExponentIndex==Length){\
				Length=Index;\
				__VA_ARGS__;\
			}\
			if(IsDecimalDigit(Reading=String[ExponentIndex])){\
				break;\
			}\
			if(Reading!='_'){\
				Length=Index;\
				__VA_ARGS__;\
			}\
		}
	#define PARSE_EXPONENT(...)\
		size_t ExponentIndex=Index+1;\
		bool ExponentIsPositive;\
		for(;;){\
			if(ExponentIndex==Length){\
				Length=Index;\
				__VA_ARGS__;\
			}\
			if(IsDecimalDigit(Reading=String[ExponentIndex])){\
				ExponentIsPositive=1;\
				break;\
			}\
			if(Reading=='+'){\
				ExponentIsPositive=1;\
				READ_TO_FIRST_DIGIT(__VA_ARGS__);\
				break;\
			}\
			if(Reading=='-'){\
				ExponentIsPositive=0;\
				READ_TO_FIRST_DIGIT(__VA_ARGS__);\
				break;\
			}\
			if(Reading!='_'){\
				Length=Index;\
				__VA_ARGS__;\
			}\
			++ExponentIndex;\
		}\
		size_t ExponentStart=ExponentIndex;\
		while(++ExponentIndex!=Length){/* numbers are read right to left to keep as much precision as possible */\
			if(!IsDecimalDigit(Reading=String[ExponentIndex])&&Reading!='_'){\
				Length=ExponentIndex;\
				break;\
			}\
		}\
		Exponent=0;\
		size_t DigitExponent=0;\
		do{\
			if((Reading=String[--ExponentIndex])!='_'){/* Reading=='0' will use a different exponent to avoid zero*infinity */\
				Exponent+=CharacterToDigit(Reading)*pow((lua_Number)((Reading!='0')*9+1),(lua_Number)DigitExponent++);\
			}\
		}while(ExponentIndex!=ExponentStart);\
		Exponent*=ExponentIsPositive*2-1
	if(Reading=='0'){/* a leading zero could either be a decimal number of a prefix number such as 0XF */
		do{
			if(++Index==Length){
				Output->IsInteger=1;
				Output->Integer=0;
				return Length;
			}
		}while((Reading=String[Index])=='_');
		if(IsDecimalDigit(Reading)){
			goto Decimal;
		}
		Start=Index;
		if(Reading=='.'){
			goto Dot;
		}
		if((Reading=MakeUppercase(Reading))=='E'){/* the result is zero, though the exponent still needs to be read */
			for(;;){
				if(++Index==Length){
					Output->IsInteger=1;
					Output->Integer=0;
					return Start;
				}
				if(IsDecimalDigit(Reading=String[Index])){
					break;
				}
				if(Reading=='+'||Reading=='-'){
					for(;;){
						if(++Index==Length){
							Output->IsInteger=1;
							Output->Integer=0;
							return Start;
						}
						if(IsDecimalDigit(Reading=String[Index])){
							break;
						}
						if(Reading!='_'){
							Output->IsInteger=1;
							Output->Integer=0;
							return Start;
						}
					}
					break;
				}
				if(Reading!='_'){
					Output->IsInteger=1;
					Output->Integer=0;
					return Start;
				}
			}
			Output->IsInteger=0;
			Output->Float=(IsPositive*2-1)*(lua_Number)0;/* the sign needs to be considered even for zero with floats */
			for(;;){
				if(++Index==Length){
					return Length;
				}
				if(!IsDecimalDigit(Reading=String[Index])&&Reading!='_'){
					return Index;
				}
			}
		}
		#define PARSE_PREFIXED(Name,Radix,RadixLog2)\
			for(;;){\
				if(++Index==Length){\
					Output->IsInteger=1;\
					Output->Integer=0;\
					return Start;\
				}\
				if(Is##Name##Digit(Reading=String[Index])){\
					Start=Index;\
					break;\
				}\
				if(Reading=='.'){\
					for(;;){\
						if(++Index==Length){\
							Output->IsInteger=1;\
							Output->Integer=0;\
							return Start;\
						}\
						if(Is##Name##Digit(Reading=String[Index])){\
							DigitsAmount=1;\
							goto Name##Dot;\
						}\
						if(Reading!='_'){\
							Output->IsInteger=1;\
							Output->Integer=0;\
							return Start;\
						}\
					}\
				}\
				if(Reading!='_'){\
					Output->IsInteger=1;\
					Output->Integer=0;\
					return Start;\
				}\
			}\
			while(++Index!=Length){\
				if(!Is##Name##Digit(Reading=String[Index])&&Reading!='_'){\
					if(Reading=='.'){\
						DigitsAmount=0;\
						goto Name##Dot;\
					}\
					if(MakeUppercase(Reading)!='P'){\
						Length=Index;\
						break;\
					}\
					PARSE_EXPONENT(goto Name##Integer);\
					goto Name##Float;\
				}\
			}\
			Name##Integer:;\
			while((Reading=String[--Index])=='_');\
			Output->IsInteger=1;\
			Output->Integer=CharacterTo##Name##Digit(Reading);\
			for(lua_Unsigned Multiple=1;Index!=Start;){/* prefixed numbers wrap around */\
				if((Reading=String[--Index])!='_'){\
					Output->Integer+=CharacterTo##Name##Digit(Reading)*(Multiple*=(lua_Unsigned)(Radix));\
				}\
			}\
			Output->Integer*=((lua_Unsigned)IsPositive<<1)-1;/* unsigned negation to get wrap around */\
			return Length;\
			Name##Dot:;\
			while(++Index!=Length){\
				if(Is##Name##Digit(Reading=String[Index])){\
					++DigitsAmount;\
				}else if(Reading!='_'){\
					if(MakeUppercase(Reading)!='P'){\
						Length=Index;\
						break;\
					}\
					PARSE_EXPONENT(Exponent=-(lua_Number)DigitsAmount*(RadixLog2);goto Name##Float);\
					Exponent-=(lua_Number)DigitsAmount*(RadixLog2);\
					goto Name##Float;\
				}\
			}\
			Exponent=-(lua_Number)DigitsAmount*(RadixLog2);\
			Name##Float:;\
			Output->IsInteger=0;\
			Output->Float=0;\
			DigitsAmount=0;\
			do{\
				if(Is##Name##Digit(Reading=String[--Index])){\
					lua_Number Multiple=pow((lua_Number)((Reading!='0')+1),Exponent+(lua_Number)DigitsAmount++*(RadixLog2));\
					Output->Float+=Multiple?CharacterTo##Name##Digit(Reading)*Multiple:(lua_Number)CharacterTo##Name##Digit(Reading)/(Radix)*pow((lua_Number)2,Exponent+(lua_Number)DigitsAmount*(RadixLog2));\
				}/* if the result is zero, try again with the exponent increased and the result scaled down in case the result is representable even if the multiple isn't representable */\
			}while(Index!=Start);\
			Output->Float*=IsPositive*2-1;\
			return Length
		if(Reading=='X'){
			PARSE_PREFIXED(Hexadecimal,16,4);
		}
		if(Reading=='O'){
			PARSE_PREFIXED(Octal,8,3);
		}
		if(Reading=='B'){
			PARSE_PREFIXED(Binary,2,1);
		}
		#undef PARSE_PREFIXED
		Output->IsInteger=1;
		Output->Integer=0;
		return Index;
	}
	if(IsDecimalDigit(Reading)){
		Decimal:;
		for(Start=Index;;){
			if(++Index==Length){
				goto Integer;
			}
			if(!IsDecimalDigit(Reading=String[Index])&&Reading!='_'){
				if(Reading=='.'){
					break;
				}
				if(MakeUppercase(Reading)!='E'){
					Length=Index;
					goto Integer;
				}
				PARSE_EXPONENT(goto Integer);
				goto Float;
			}
		}
		Dot:;
		for(DigitsAmount=0;++Index!=Length;){
			if(IsDecimalDigit(Reading=String[Index])){
				++DigitsAmount;
			}else if(Reading!='_'){
				if(MakeUppercase(Reading)!='E'){
					Length=Index;
					break;
				}
				PARSE_EXPONENT(Exponent=-(lua_Number)DigitsAmount;goto Float);
				Exponent-=DigitsAmount;
				goto Float;
			}
		}
		Exponent=-(lua_Number)DigitsAmount;
		Float:;
		Output->IsInteger=0;
		Output->Float=0;
		DigitsAmount=0;
		do{
			if(IsDecimalDigit(Reading=String[--Index])){
				lua_Number Multiple=pow((lua_Number)((Reading!='0')*9+1),Exponent+DigitsAmount++);
				Output->Float+=Multiple?CharacterToDigit(Reading)*Multiple:(lua_Number)CharacterToDigit(Reading)/10*pow((lua_Number)10,Exponent+DigitsAmount);
			}
		}while(Index!=Start);
		Output->Float*=IsPositive*2-1;
		return Length;
		Integer:;
		for(DigitsAmount=1;(Reading=String[--Index])=='_';);
		if(IsPositive){/* decimal numbers will result in floats if not representable in integers, to check this positive and negative numbers need different cases */
			Output->Integer=CharacterToDigit(Reading);
			for(size_t Multiple=1;;){
				if(Index==Start){
					Output->IsInteger=1;
					return Length;
				}
				if((Reading=String[--Index])=='0'){/* if the multiple would overflow, it will only matter if a non-zero digit appears */
					Multiple=Multiple>LUA_MAXINTEGER/10?LUA_MAXINTEGER:Multiple*10;
					++DigitsAmount;
				}else if(Reading!='_'){
					int Digit=CharacterToDigit(Reading);
					if(Multiple>LUA_MAXINTEGER/10){
						lua_Integer Temporary=Output->Integer;
						Output->Float=Temporary+Digit*pow((lua_Number)10,DigitsAmount++);
						break;
					}
					Multiple*=10;
					if(Multiple>LUA_MAXINTEGER/Digit){
						lua_Integer Temporary=Output->Integer;
						Output->Float=Temporary+(lua_Number)Digit*Multiple;
						break;
					}
					lua_Integer DigitMultiple=Digit*Multiple;
					if(DigitMultiple>LUA_MAXINTEGER-Output->Integer){
						lua_Integer Temporary=Output->Integer;
						Output->Float=(lua_Number)Temporary+DigitMultiple;
						break;
					}
					Output->Integer+=DigitMultiple;
					++DigitsAmount;
				}
			}
			while(Index!=Start){
				if((Reading=String[--Index])!='_'){
					Output->Float+=CharacterToDigit(Reading)*pow((lua_Number)((Reading!='0')*9+1),DigitsAmount++);
				}
			}
		}else{
			Output->Integer=-CharacterToDigit(Reading);
			for(size_t Multiple=-1;;){
				if(Index==Start){
					Output->IsInteger=1;
					return Length;
				}
				if((Reading=String[--Index])=='0'){
					Multiple=Multiple<LUA_MININTEGER/10?LUA_MININTEGER:Multiple*10;
					++DigitsAmount;
				}else if(Reading!='_'){
					int Digit=CharacterToDigit(Reading);
					if(Multiple<LUA_MININTEGER/10){
						lua_Integer Temporary=Output->Integer;
						Output->Float=Temporary-Digit*pow((lua_Number)10,DigitsAmount++);
						break;
					}
					Multiple*=10;
					if(Multiple<LUA_MININTEGER/Digit){
						lua_Integer Temporary=Output->Integer;
						Output->Float=Temporary+(lua_Number)Digit*Multiple;
						break;
					}
					lua_Integer DigitMultiple=Digit*Multiple;
					if(DigitMultiple<LUA_MININTEGER-Output->Integer){
						lua_Integer Temporary=Output->Integer;
						Output->Float=(lua_Number)Temporary+DigitMultiple;
						break;
					}
					Output->Integer+=DigitMultiple;
					++DigitsAmount;
				}
			}
			while(Index!=Start){
				if((Reading=String[--Index])!='_'){
					Output->Float-=CharacterToDigit(Reading)*pow((lua_Number)((Reading!='0')*9+1),DigitsAmount++);
				}
			}
		}
		Output->IsInteger=0;
		return Length;
	}
	if(Reading=='.'){/* numbers starting with a period must be followed by a digit to avoid ambiguity with names */
		if((Start=Index+1)==Length||!IsDecimalDigit(String[Start])){
			return 0;
		}
		DigitsAmount=1;
		for(Index+=2;;){
			if(Index==Length){
				Exponent=0;
				break;
			}
			if(IsDecimalDigit(Reading=String[Index])){
				++DigitsAmount;
			}else if(Reading!='_'){
				if(MakeUppercase(Reading)!='E'){
					Exponent=0;
					Length=Index;
					break;
				}
				PARSE_EXPONENT(Exponent=0;goto Exit);
				break;
			}
			++Index;
		}
		Exit:;
		Output->IsInteger=0;
		Output->Float=0;
		do{
			if((Reading=String[--Index])!='_'){
				lua_Number Multiple=pow((lua_Number)((Reading!='0')*9+1),Exponent-DigitsAmount--);
				Output->Float+=Multiple?CharacterToDigit(Reading)*Multiple:(lua_Number)CharacterToDigit(Reading)/10*pow((lua_Number)10,Exponent-DigitsAmount);
			}
		}while(Index!=Start);
		Output->Float*=IsPositive*2-1;
		return Length;
	}
	#undef READ_TO_FIRST_DIGIT
	#undef PARSE_EXPONENT
	if((Reading=MakeUppercase(Reading))=='I'){
		if(Length<Index+3||MakeUppercase(String[Index+1])!='N'||MakeUppercase(String[Index+2])!='F'){
			return 0;
		}
		Output->IsInteger=0;
		#if LUA_FLOAT_TYPE==LUA_FLOAT_FLOAT
			Output->Float=(IsPositive*2-1)*HUGE_VALF;
		#elif LUA_FLOAT_TYPE==LUA_FLOAT_DOUBLE
			Output->Float=(IsPositive*2-1)*HUGE_VAL;
		#elif LUA_FLOAT_TYPE==LUA_FLOAT_LONGDOUBLE
			Output->Float=(IsPositive*2-1)*HUGE_VALL;
		#else
			#error"unkown float type"
		#endif
		return Index+(Length<Index+8||MakeUppercase(String[Index+3])!='I'||MakeUppercase(String[Index+4])!='N'||MakeUppercase(String[Index+5])!='I'||MakeUppercase(String[Index+6])!='T'||MakeUppercase(String[Index+7])!='Y'?3:8);
	}
	#ifdef NAN
		if(Reading=='N'){
			if(Length<Index+3||MakeUppercase(String[Index+1])!='A'||MakeUppercase(String[Index+2])!='N'){
				return 0;
			}
			Output->IsInteger=0;
			Output->Float=NAN;
			if(Length<Index+5||String[Index+3]!='('){
				return 3;
			}
			if((Reading=String[Index+4])==')'){
				return 5;
			}
			if(!IsAlphanumeric(Reading)&&Reading!='_'){
				return 3;
			}
			for(Index+=5;Index!=Length;){
				if((Reading=String[Index++])==')'){
					return Index;
				}
				if(!IsAlphanumeric(Reading)&&Reading!='_'){
					return 3;
				}
			}
			return 3;
		}
	#endif
	return 0;
}
