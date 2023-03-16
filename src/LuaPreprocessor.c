#include<float.h>
#include<stdio.h>
#include<assert.h>
#include<limits.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include<tgmath.h>
#include<stdbool.h>
#include"Numeric.h"
#include"NoLocale.h"
#include"Libraries.h"
#include"lua-5.4.4/src/lua.h"
#include"lua-5.4.4/src/lauxlib.h"
static_assert(_Generic(+(lua_Integer)0,lua_Integer:1,default:0),"Expected Lua integers to not require integer promotions");
static_assert(_Generic(+(size_t)0,size_t:1,default:0),"Expected size_t to not require integer promotions");
static_assert((int)UINT_MAX==-1,"Expected conversions to signed integers to wrap");
#if LUA_MAXINTEGER>=SIZE_MAX
	#define LSIZE_MAX SIZE_MAX
#else
	#define LSIZE_MAX ((size_t)LUA_MAXINTEGER)
#endif
/* size of buffers for writing numbers into strings (including nul char and a leading space) */
#define INTEGER_BUFFER_SIZE (sizeof(lua_Unsigned)>SIZE_MAX/CHAR_BIT?SIZE_MAX:sizeof(lua_Unsigned)*CHAR_BIT/4+4+!!(sizeof(lua_Unsigned)*CHAR_BIT%4))
#define MAX_EXPONENT (-(uintmax_t)l_floatatt(MIN_EXP)+4>UINTMAX_MAX-l_floatatt(MANT_DIG)?UINTMAX_MAX:-(uintmax_t)l_floatatt(MIN_EXP)+4+l_floatatt(MANT_DIG)>(l_floatatt(MAX_EXP)>UINTMAX_MAX-4?UINTMAX_MAX:(uintmax_t)l_floatatt(MAX_EXP)+4)?-(uintmax_t)l_floatatt(MIN_EXP)+4+l_floatatt(MANT_DIG):l_floatatt(MAX_EXP)>UINTMAX_MAX-4?UINTMAX_MAX:(uintmax_t)l_floatatt(MAX_EXP)+4)
#define FLOAT_BUFFER_SIZE (l_floatatt(MANT_DIG)/4+!!(l_floatatt(MANT_DIG)%4)+10+(MAX_EXPONENT>99)+(MAX_EXPONENT>999)+(MAX_EXPONENT>9999)+(MAX_EXPONENT>99999)+MAX_EXPONENT/1000000)
#define NUMBER_BUFFER_SIZE (INTEGER_BUFFER_SIZE>FLOAT_BUFFER_SIZE?INTEGER_BUFFER_SIZE:FLOAT_BUFFER_SIZE)
/* these functions exist to be called in protected mode by Lua to handle errors */
static int LuaPushString(lua_State*L){
	lua_pushlstring(L,lua_touserdata(L,1),lua_tointeger(L,2));
	return 1;
}
static int LuaGetTable(lua_State*L){
	lua_gettable(L,-2);
	return 1;
}
/* <=TOKEN_SHORT_NAME can be used to tell if the token is a short name/string */
/* >=TOKEN_LONG_NAME can be used to tell if the token is a long name/string */
/* TOKEN_INVALID is zero, so if(T) can check if T is valid */
enum{
	TOKEN_SHORT_STRING=-2,/* a string that can fit into the token's buffer */
	TOKEN_SHORT_NAME,/* a name that can fit into the token's buffer */
	TOKEN_INVALID,/* used for the special start and end tokens */
	TOKEN_INTEGER,/* an integer constant */
	TOKEN_FLOAT,/* a floating point constant */
	TOKEN_SYMBOL,/* a symbol */
	TOKEN_LONG_NAME,/* a name that can't fit into the token's buffer and has an external allocated buffer */
	TOKEN_LONG_STRING/* a string that can't fit into the token's buffer and has an external allocated buffer */
};
static const char*const*const TokenNames=(const char*const[]){"string","name",0,"integer","float","symbol","name","string"}+2;
/* length shall be less than LSIZE_MAX */
static const size_t*const TokenNameLengths=(const size_t[]){6,4,0,7,5,6,4,6}+2;
/* <=SYMBOL_OPEN_BRACE can be used to tell if the token is a opening bracket */
/* >=SYMBOL_CLOSE_BRACE can be used to tell if the token is a closing bracket */
enum{/* some extra symbols are defined here that aren't used in Lua, even if they have no special meaning here */
	SYMBOL_OPEN_PARENTHESIS,/* ( */
	SYMBOL_OPEN_BRACKET,/* [ */
	SYMBOL_OPEN_BRACE,/* { */
	SYMBOL_ADD,/* + */
	SYMBOL_SUBTRACT,/* - */
	SYMBOL_MULTIPLY,/* * */
	SYMBOL_DIVIDE,/* / */
	SYMBOL_FLOOR_DIVIDE,/* // */
	SYMBOL_MODULO,/* % */
	SYMBOL_POWER,/* ^ */
	SYMBOL_EQUALS,/* == */
	SYMBOL_UNEQUALS,/* ~= */
	SYMBOL_LESSER,/* < */
	SYMBOL_GREATER,/* > */
	SYMBOL_LESSER_EQUAL,/* <= */
	SYMBOL_GREATER_EQUAL,/* >= */
	SYMBOL_AND,/* & */
	SYMBOL_EXCLUSIVE_OR,/* ~ */
	SYMBOL_OR,/* | */
	SYMBOL_LEFT_SHIFT,/* << */
	SYMBOL_RIGHT_SHIFT,/* >> */
	SYMBOL_LENGTH,/* # */
	SYMBOL_CONCATENATE,/* .. */
	SYMBOL_ASSIGN,/* = */
	SYMBOL_DOT,/* . */
	SYMBOL_COMMA,/* , */
	SYMBOL_COLON,/* : */
	SYMBOL_SEMICOLON,/* ; */
	SYMBOL_LABEL,/* :: */
	SYMBOL_VARIABLE_ARGUMENTS,/* ... */
	SYMBOL_AT,/* @ */
	SYMBOL_BANG,/* ! */
	SYMBOL_BACKTICK,/* ` */
	SYMBOL_QUESTION,/* ? */
	SYMBOL_DOLLAR,/* $ */
	SYMBOL_CLOSE_BRACE,/* } */
	SYMBOL_CLOSE_BRACKET,/* ] */
	SYMBOL_CLOSE_PARENTHESIS/* ) */
};
static const char*SymbolTokens[]={
	"(","[","{",
	"+","-","*","/","//","%","^",
	"==","~=","<",">","<=",">=",
	"&","~","|","<<",">>",
	"#","..","=",
	".",",",":",";","::","...",
	"@","!","`","?","$",
	"}","]",")"
};
/* length shall be less than LSIZE_MAX */
static const size_t SymbolTokenLengths[]={
	1,1,1,
	1,1,1,1,2,1,1,
	2,2,1,1,2,2,
	1,1,1,2,2,
	1,2,1,
	1,1,1,1,2,3,
	1,1,1,1,1,
	1,1,1
};
/* the size must be known so the buffer for short strings/names can be as large as possible */
#define TOKEN_EXPECTED_SIZE 64
typedef union Token Token;
/* this arrangement is used to avoid as much padding as possible */
union Token{
	struct{
		Token*Next;
		Token*Previous;
		signed char Type;
	};
	#define TOKEN_HEADER(Name)Token*Next_##Name;Token*Previous_##Name;signed char Type_##Name
	struct{
		TOKEN_HEADER(Short);
		unsigned char Length;
		char Buffer[TOKEN_EXPECTED_SIZE-offsetof(struct{TOKEN_HEADER(A);unsigned char B;char C;},C)];
	}Short;
	struct{
		TOKEN_HEADER(Long);
		size_t Length;
		size_t Capacity;/* Capacity<=LSIZE_MAX */
		char*Buffer;
	}Long;
	struct{
		TOKEN_HEADER(Integer);
		lua_Integer Integer;
	};
	struct{
		TOKEN_HEADER(Float);
		lua_Number Float;/* !signbit(Float)&&!isnan(Float) */
	};
	struct{
		TOKEN_HEADER(Symbol);
		signed char Type;
		lua_Integer NotNowAmount;
	}Symbol;
	#undef TOKEN_HEADER
};
static_assert(sizeof(Token)==TOKEN_EXPECTED_SIZE,"Token is of incorrect size");
#undef TOKEN_EXPECTED_SIZE
static_assert(sizeof((Token){0}.Short.Buffer)<=UCHAR_MAX,"Short buffer is too large");
/* these checks just simplify the code so that basic things don't need to be checked */
/* all keywords and built in macro names fit in short names */
/* small numbers can be added to a short string's length without overflowing */
/* doubling the length of a short string will result in a reasonable increase in size */
/* small numbers can be subtracted from a long string's length without underflowing */
static_assert(sizeof((Token){0}.Short.Buffer)<LSIZE_MAX-8,"Short buffer is way too large (too few possible long strings)");
static_assert(sizeof((Token){0}.Short.Buffer)>=8,"Short buffer is way too small (should be at least 8 bytes)");
enum{
	ERROR_STATIC,/* static error message (Message is a string literal, Capacity is undefined) */
	ERROR_ALLOCATED,/* allocated error message (Message is allocated, Capacity is only defined if this type is used) */
	ERROR_MEMORY/* error allocating error message (same representation as static) */
};
typedef struct ErrorMessage ErrorMessage;
struct ErrorMessage{
	char*Message;/* not null if an error is present */
	size_t Length;/* should only be accessed if an error is present */
	size_t Capacity;/* Capacity<=LSIZE_MAX, Capacity>=16 (reduce reallocating many times at small sizes) */
	signed char Type;/* ERROR_STATIC if no error is present */
};
static inline void StaticError(ErrorMessage*Error,const char*Message,size_t Length){
	Error->Message=(char*)Message;
	Error->Length=Length;
}
#define STATIC_ERROR(Error,Message_)StaticError(Error,Message_,sizeof(Message_)-1)
static inline void MemoryError(ErrorMessage*Error){
	Error->Message="Error allocating error message";
	Error->Length=30;
	Error->Type=ERROR_MEMORY;
}
/*
the type used to represent the preprocessor, tokens are stored in a list between Start end End
the list must be valid at any time so that the PreprocessorState can be easily freed at any arbitrary time, even in error
once an error occurs, it should never be removed (never recoverable)

at all times during preprocessing, the value at index 1 on the Lua stack will be the preprocessor state
*/
typedef struct PreprocessorState PreprocessorState;
struct PreprocessorState{
	Token Start;
	Token End;
	Token*CursorStart;/* points to the first token behind the accessible tokens, or null if Lua code cannot interact with the token list */
	Token*Cursor;/* points to the token currently being referenced (TOKEN_INVALID indicates nothing is being referenced), or is undefined if CursorStart is null */
	ErrorMessage Error;/* when an error is present, CursorStart and Cursor are both undefined */
};
typedef struct TokenList TokenList;
struct TokenList{
	Token*First;
	Token*Last;
};
static inline bool IsSymbolCloseBracket(Token*Checking){
	return Checking->Symbol.Type>=SYMBOL_CLOSE_BRACE;
}
static inline Token*CreateToken(void){
	return malloc(sizeof(Token));
}
static inline void FreeContentsDirect(Token*Freeing){
	free(Freeing->Long.Buffer);
}
static inline void FreeContents(Token*Freeing){
	if(Freeing->Type>=TOKEN_LONG_NAME){
		FreeContentsDirect(Freeing);
	}
}
static inline void FreeTokenDirect(Token*Freeing){
	free(Freeing);
}
static inline void FreeToken(Token*Freeing){
	FreeContents(Freeing);
	FreeTokenDirect(Freeing);
}
static TokenList MakeTokenList(const char*Buffer,size_t Length,ErrorMessage*Error){
	Token Start;/* special token at the start so something is there */
	Start.Next=0;
	TokenList Result;
	Result.Last=&Start;
	size_t Index=0;
	lua_Integer NotNowAmount=0;
	if(!Length){
		goto Done;
	}
	#define PARSE_FAIL(Message)\
		STATIC_ERROR(Error,Message);\
		goto Fail
	#define PARSE_FAIL_DIRECT(Message)\
		STATIC_ERROR(Error,Message);\
		goto FailDirect
	#define PARSE_FAIL_CONTENTS(Message)\
		STATIC_ERROR(Error,Message);\
		goto FailContents
	#define CREATE_NEW_TOKEN(Name)\
		Token*Name=CreateToken();\
		if(!Name){\
			PARSE_FAIL("Error allocating tokens");\
		}\
		(Result.Last=(Name->Previous=Result.Last)->Next=Name)->Next=0
	#define CREATE_NON_SYMBOL(Name)\
		CREATE_NEW_TOKEN(Name);\
		if(NotNowAmount){\
			PARSE_FAIL_DIRECT("Only symbols may have notnows");\
		}
	#define CREATE_SYMBOL(Name)\
		CREATE_NEW_TOKEN(Name);\
		Name->Type=TOKEN_SYMBOL;\
		Name->Symbol.NotNowAmount=NotNowAmount;\
		NotNowAmount=0
	for(;;){
		Loop:;
		switch(Buffer[Index]){
			case'\\':{
				if(NotNowAmount==LUA_MAXINTEGER){
					PARSE_FAIL("Too many notnows");
				}
				++NotNowAmount;
				if(++Index==Length){
					PARSE_FAIL("Only symbols may have notnows");
				}
				break;
			}
			#define SINGLE_CHARACTER_SYMBOL(Character,Type_)\
				case Character:{\
					CREATE_SYMBOL(Symbol);\
					Symbol->Symbol.Type=Type_;\
					if(++Index==Length){\
						goto Done;\
					}\
					break;\
				}
			SINGLE_CHARACTER_SYMBOL('(',SYMBOL_OPEN_PARENTHESIS);
			SINGLE_CHARACTER_SYMBOL('{',SYMBOL_OPEN_BRACE);
			SINGLE_CHARACTER_SYMBOL('+',SYMBOL_ADD);
			SINGLE_CHARACTER_SYMBOL('*',SYMBOL_MULTIPLY);
			SINGLE_CHARACTER_SYMBOL('%',SYMBOL_MODULO);
			SINGLE_CHARACTER_SYMBOL('^',SYMBOL_POWER);
			SINGLE_CHARACTER_SYMBOL('&',SYMBOL_AND);
			SINGLE_CHARACTER_SYMBOL('|',SYMBOL_OR);
			SINGLE_CHARACTER_SYMBOL('#',SYMBOL_LENGTH);
			SINGLE_CHARACTER_SYMBOL(',',SYMBOL_COMMA);
			SINGLE_CHARACTER_SYMBOL(';',SYMBOL_SEMICOLON);
			SINGLE_CHARACTER_SYMBOL('@',SYMBOL_AT);
			SINGLE_CHARACTER_SYMBOL('!',SYMBOL_BANG);
			SINGLE_CHARACTER_SYMBOL('`',SYMBOL_BACKTICK);
			SINGLE_CHARACTER_SYMBOL('?',SYMBOL_QUESTION);
			SINGLE_CHARACTER_SYMBOL('$',SYMBOL_DOLLAR);
			SINGLE_CHARACTER_SYMBOL('}',SYMBOL_CLOSE_BRACE);
			SINGLE_CHARACTER_SYMBOL(']',SYMBOL_CLOSE_BRACKET);
			SINGLE_CHARACTER_SYMBOL(')',SYMBOL_CLOSE_PARENTHESIS);
			#undef SINGLE_CHARACTER_SYMBOL
			#define DOUBLE_CHARACTER_SYMBOL(Character,OptionalCharacter,Type_,TypeOptional)\
				case Character:{\
					CREATE_SYMBOL(Symbol);\
					if(++Index==Length){\
						Symbol->Symbol.Type=Type_;\
						goto Done;\
					}\
					if(Buffer[Index]==(OptionalCharacter)){\
						Symbol->Symbol.Type=TypeOptional;\
						if(++Index==Length){\
							goto Done;\
						}\
					}else{\
						Symbol->Symbol.Type=Type_;\
					}\
					break;\
				}
			DOUBLE_CHARACTER_SYMBOL('/','/',SYMBOL_DIVIDE,SYMBOL_FLOOR_DIVIDE);
			DOUBLE_CHARACTER_SYMBOL('=','=',SYMBOL_ASSIGN,SYMBOL_EQUALS);
			DOUBLE_CHARACTER_SYMBOL('~','=',SYMBOL_EXCLUSIVE_OR,SYMBOL_UNEQUALS);
			DOUBLE_CHARACTER_SYMBOL(':',':',SYMBOL_COLON,SYMBOL_LABEL);
			#undef DOUBLE_CHARACTER_SYMBOL
			#define DOUBLE_CHARACTER_SYMBOL_TWO(Character,OptionalCharacter1,OptionalCharacter2,Type_,TypeOptional1,TypeOptional2)\
				case Character:{\
					CREATE_SYMBOL(Symbol);\
					if(++Index==Length){\
						Symbol->Symbol.Type=Type_;\
						goto Done;\
					}\
					if(Buffer[Index]==(OptionalCharacter1)){\
						Symbol->Symbol.Type=TypeOptional1;\
						if(++Index==Length){\
							goto Done;\
						}\
					}else if(Buffer[Index]==(OptionalCharacter2)){\
						Symbol->Symbol.Type=TypeOptional2;\
						if(++Index==Length){\
							goto Done;\
						}\
					}else{\
						Symbol->Symbol.Type=Type_;\
					}\
					break;\
				}
			DOUBLE_CHARACTER_SYMBOL_TWO('<','<','=',SYMBOL_LESSER,SYMBOL_LEFT_SHIFT,SYMBOL_LESSER_EQUAL);
			DOUBLE_CHARACTER_SYMBOL_TWO('>','>','=',SYMBOL_GREATER,SYMBOL_RIGHT_SHIFT,SYMBOL_GREATER_EQUAL);
			#undef DOUBLE_CHARACTER_SYMBOL_TWO
			case'-':{
				if(++Index==Length){
					CREATE_SYMBOL(Symbol);
					Symbol->Symbol.Type=SYMBOL_SUBTRACT;
					goto Done;
				}
				if(Buffer[Index]!='-'){
					CREATE_SYMBOL(Symbol);
					Symbol->Symbol.Type=SYMBOL_SUBTRACT;
				}else if(++Index==Length){
					goto Done;
				}else if(Buffer[Index]=='['){
					size_t Level=0;
					for(;;){
						if(++Index==Length){
							goto Done;
						}
						if(Buffer[Index]=='['){
							break;
						}
						if(Buffer[Index]!='='){
							goto Short;
						}
						++Level;
					}
					for(;;){
						if(++Index==Length){
							PARSE_FAIL("Unfinished long comment");
						}
						if(Buffer[Index]==']'){
							size_t ExitLevel=0;
							for(;;){
								if(++Index==Length){
									PARSE_FAIL("Unfinished long comment");
								}
								if(Buffer[Index]==']'){
									if(Level==ExitLevel){
										if(++Index==Length){
											goto Done;
										}
										goto Loop;
									}
									ExitLevel=0;
								}else if(Buffer[Index]=='='){
									++ExitLevel;
								}else{
									break;
								}
							}
						}
					}
				}else{
					Short:;
					for(;;){
						if(Buffer[Index]=='\n'||Buffer[Index]=='\r'){
							if(++Index==Length){
								goto Done;
							}/* if the end of line sequence is \n\r or \r\n then the second byte will be skipped later */
							break;
						}
						if(++Index==Length){
							goto Done;
						}
					}
				}
				break;
			}
			#define CREATE_NUMBER\
				CREATE_NON_SYMBOL(Number);\
				if(Output.IsInteger){\
					Number->Type=TOKEN_INTEGER;\
					Number->Integer=Output.Integer;\
				}else{\
					Number->Type=TOKEN_FLOAT;\
					Number->Float=Output.Float;\
				}
			#define PARSE_NUMBER\
				FloatOrInteger Output;\
				size_t NumberLength=StringToFloatOrInteger(Buffer+Index,Length-Index,&Output);\
				if((Index+=NumberLength)==Length){\
					CREATE_NUMBER;\
					goto Done;\
				}\
				if(IsAlphanumeric(Buffer[Index])||Buffer[Index]=='_'||Buffer[Index]=='.'){/* a number must not be followed by one of these, e.g. 123abc is invalid */\
					PARSE_FAIL("Invalid number");\
				}\
				CREATE_NUMBER
			case'.':{
				if(++Index==Length){
					CREATE_SYMBOL(Symbol);
					Symbol->Symbol.Type=SYMBOL_DOT;
					goto Done;
				}
				if(Buffer[Index]=='.'){
					CREATE_SYMBOL(Symbol);
					if(++Index==Length){
						Symbol->Symbol.Type=SYMBOL_CONCATENATE;
						goto Done;
					}
					if(Buffer[Index]=='.'){
						Symbol->Symbol.Type=SYMBOL_VARIABLE_ARGUMENTS;
						if(++Index==Length){
							goto Done;
						}
					}else{
						Symbol->Symbol.Type=SYMBOL_CONCATENATE;
					}
				}else if(IsDigit(Buffer[Index])){
					--Index;
					PARSE_NUMBER;
				}else{
					CREATE_SYMBOL(Symbol);
					Symbol->Symbol.Type=SYMBOL_DOT;
				}
				break;
			}
			case'[':{
				if(++Index==Length){
					CREATE_SYMBOL(Symbol);
					Symbol->Symbol.Type=SYMBOL_OPEN_BRACKET;
					goto Done;
				}
				size_t Level;
				if(Buffer[Index]=='['){
					Level=0;
				}else if(Buffer[Index]=='='){
					Level=1;
					for(;;){
						if(++Index==Length){
							PARSE_FAIL("Unfinished long string");
						}
						if(Buffer[Index]=='['){
							break;
						}
						if(Buffer[Index]!='='){
							PARSE_FAIL("Invalid long string delimiter");
						}
						++Level;
					}
				}else{
					CREATE_SYMBOL(Symbol);
					Symbol->Symbol.Type=SYMBOL_OPEN_BRACKET;
					break;
				}
				if(++Index==Length){
					PARSE_FAIL("Unfinished long string");
				}
				if(Buffer[Index]=='\n'?++Index==Length||Buffer[Index]=='\r'&&++Index==Length:Buffer[Index]=='\r'&&(++Index==Length||Buffer[Index]=='\n'&&++Index==Length)){
					PARSE_FAIL("Unfinished long string");
				}
				size_t StringLength=0;
				for(size_t StringIndex=Index;;){
					if(Buffer[Index]==']'){
						for(size_t ExitLevel=0;;){
							if(++Index==Length){
								PARSE_FAIL("Unfinished long string");
							}
							if(Buffer[Index]==']'){
								if(Level==ExitLevel){
									#define WRITE_CONTENTS(...)\
										for(size_t BufferIndex=0;BufferIndex!=StringLength;++BufferIndex){\
											if(Buffer[StringIndex]=='\n'){\
												if(Buffer[++StringIndex]=='\r'){\
													++StringIndex;\
												}\
												BufferIndex[__VA_ARGS__]='\n';\
											}else if(Buffer[StringIndex]=='\r'){\
												if(Buffer[++StringIndex]=='\n'){\
													++StringIndex;\
												}\
												BufferIndex[__VA_ARGS__]='\n';\
											}else{\
												BufferIndex[__VA_ARGS__]=Buffer[StringIndex++];\
											}\
										}
									if(StringLength<=sizeof(Start.Short.Buffer)){
										CREATE_NON_SYMBOL(String);
										String->Type=TOKEN_SHORT_STRING;
										String->Short.Length=StringLength;
										WRITE_CONTENTS(String->Short.Buffer);
									}else{
										CREATE_NON_SYMBOL(String);
										char*StringBuffer=malloc(StringLength);
										if(!StringBuffer){
											PARSE_FAIL_DIRECT("Error allocating long string");
										}
										String->Type=TOKEN_LONG_STRING;
										String->Long.Buffer=StringBuffer;
										String->Long.Capacity=String->Long.Length=StringLength;
										WRITE_CONTENTS(StringBuffer);
									}
									#undef WRITE_CONTENTS
									if(++Index==Length){
										goto Done;
									}
									goto Loop;
								}else{
									if(ExitLevel>LSIZE_MAX||StringLength>=LSIZE_MAX-ExitLevel){
										PARSE_FAIL("Error allocating long string");
									}
									StringLength+=ExitLevel+1;
									ExitLevel=0;
								}
							}else if(Buffer[Index]=='='){
								++ExitLevel;
							}else{
								break;
							}
						}
					}
					if(StringLength==LSIZE_MAX){
						PARSE_FAIL("Error allocating long string");
					}
					++StringLength;
					if(Buffer[Index]=='\n'){
						if(++Index==Length||Buffer[Index]=='\r'&&++Index==Length){
							PARSE_FAIL("Unfinished long string");
						}
					}else if(Buffer[Index]=='\r'){
						if(++Index==Length||Buffer[Index]=='\n'&&++Index==Length){
							PARSE_FAIL("Unfinished long string");
						}
					}else if(++Index==Length){
						PARSE_FAIL("Unfinished long string");
					}
				}
			}
			case'\'':case'"':{
				char EndingCharacter=Buffer[Index];
				if(++Index==Length){
					PARSE_FAIL("Unfinished short string");
				}
				CREATE_NON_SYMBOL(String);
				String->Type=TOKEN_SHORT_STRING;
				String->Short.Length=0;
				#define MAKE_LONG_STRING\
					size_t Capacity=sizeof(String->Short.Buffer)>LSIZE_MAX>>1?LSIZE_MAX:sizeof(String->Short.Buffer)<<1;\
					char*StringBuffer=malloc(Capacity);\
					if(!StringBuffer){\
						PARSE_FAIL_DIRECT("Error allocating short string");\
					}\
					memcpy(StringBuffer,String->Short.Buffer,String->Short.Length);\
					String->Long.Length=String->Short.Length;\
					String->Long.Capacity=Capacity;\
					String->Long.Buffer=StringBuffer;\
					String->Type=TOKEN_LONG_STRING
				/* unicode encoding up to 6 bytes, as Lua allows */
				#define UNICODE_ADD2(Name)\
					String->Name.Buffer[String->Name.Length++]=0XC0|CodePoint>>6;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint&0X3F
				#define UNICODE_ADD3(Name)\
					String->Name.Buffer[String->Name.Length++]=0XE0|CodePoint>>12;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>6&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint&0X3F
				#define UNICODE_ADD4(Name)\
					String->Name.Buffer[String->Name.Length++]=0XF0|CodePoint>>18;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>12&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>6&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint&0X3F
				#define UNICODE_ADD5(Name)\
					String->Name.Buffer[String->Name.Length++]=0XF8|CodePoint>>24;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>18&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>12&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>6&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint&0X3F
				#define UNICODE_ADD6(Name)\
					String->Name.Buffer[String->Name.Length++]=0XFC|CodePoint>>30;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>24&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>18&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>12&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint>>6&0X3F;\
					String->Name.Buffer[String->Name.Length++]=0X80|CodePoint&0X3F
				#define SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,Character,Escape)\
					case Character:{\
						ADD_CHARACTER(Escape);\
						if(++Index==Length){\
							Fail("Unfinished short string");\
						}\
						break;\
					}
				#define PARSE(Fail,Name)\
					switch(Buffer[Index]){\
						case'\\':{\
							switch(Buffer[Index]){\
								case'\n':{\
									if(++Index==Length||Buffer[Index]=='\r'&&++Index==Length){\
										Fail("Unfinished short string");\
									}\
									ADD_CHARACTER('\n');\
									break;\
								}\
								case'\r':{\
									if(++Index==Length||Buffer[Index]=='\n'&&++Index==Length){\
										Fail("Unfinished short string");\
									}\
									ADD_CHARACTER('\n');\
									break;\
								}\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'\\','\\');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'\'','\'');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'"','"');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'a','\a');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'b','\b');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'f','\f');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'n','\n');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'r','\r');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'s',' ');/* \s escape sequnce (not valid in Lua) */\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'t','\t');\
								SINGLE_CHARACTER_ESCAPE_SEQUENCE(Fail,'v','\v');\
								case'x':{\
									if(Index>=Length-2||!IsHexadecimalDigit(Buffer[++Index])){\
										Fail("Expected two hexadecimal digits after \\x");\
									}\
									unsigned Character=(unsigned)CharacterToHexadecimalDigit(Buffer[Index])<<4;\
									if(!IsHexadecimalDigit(Buffer[++Index])){\
										Fail("Expected two hexadecimal digits after \\x");\
									}\
									Character|=CharacterToHexadecimalDigit(Buffer[Index]);\
									if(++Index==Length){\
										Fail("Unfinished short string");\
									}\
									ADD_CHARACTER(Character);\
									break;\
								}\
								case'u':{\
									if(++Index==Length||Buffer[Index]!='{'){\
										Fail("Expected an opening brace after \\u");\
									}\
									unsigned long CodePoint=0;\
									for(;;){\
										if(++Index==Length){\
											Fail("Unfinished \\u escape sequence");\
										}\
										if(IsHexadecimalDigit(Buffer[Index])){\
											if(CodePoint>0X7FFFFFF){\
												Fail("\\u escape sequence is too large");\
											}\
											CodePoint=CodePoint<<4|CharacterToHexadecimalDigit(Buffer[Index]);\
										}else if(Buffer[Index]=='}'){\
											break;\
										}else{\
											Fail("Invalid character in \\u escape sequence");\
										}\
									}\
									if(CodePoint<=0X7F){\
										ADD_CHARACTER(CodePoint);\
									}else if(CodePoint<=0X7FF){\
										ALLOCATE(2);\
										UNICODE_ADD2(Name);\
									}else if(CodePoint<=0XFFFF){\
										ALLOCATE(3);\
										UNICODE_ADD3(Name);\
									}else if(CodePoint<=0X1FFFFF){\
										ALLOCATE(4);\
										UNICODE_ADD4(Name);\
									}else if(CodePoint<=0X3FFFFFF){\
										ALLOCATE(5);\
										UNICODE_ADD5(Name);\
									}else{\
										ALLOCATE(6);\
										UNICODE_ADD6(Name);\
									}\
									if(++Index==Length){\
										Fail("Unfinished short string");\
									}\
									break;\
								}\
								case'z':{\
									for(;;){\
										if(++Index==Length){\
											Fail("Unfinished short string");\
										}\
										if(!IsSpace(Buffer[Index])){\
											break;\
										}\
									}\
									break;\
								}\
								default:{\
									if(!IsDigit(Buffer[Index])){\
										Fail("Invalid escape sequence");\
									}\
									int Character=CharacterToDigit(Buffer[Index]);\
									if(++Index==Length){\
										Fail("Unfinished short string");\
									}\
									if(IsDigit(Buffer[Index])){\
										Character=Character*10+CharacterToDigit(Buffer[Index]);\
										if(++Index==Length){\
											Fail("Unfinished short string");\
										}\
										if(IsDigit(Buffer[Index])){\
											int LastDigit=CharacterToDigit(Buffer[Index]);\
											if(Character>25||Character==25&&LastDigit>5){\
												Fail("Decimal escape sequence is too large");\
											}\
											if(++Index==Length){\
												Fail("Unfinished short string");\
											}\
											Character=Character*10+LastDigit;\
										}\
									}\
									ADD_CHARACTER(Character);\
								}\
							}\
							break;\
						}\
						case'\n':{/* string literals may span multiple lines (not valid in Lua) */\
							if(++Index==Length||Buffer[Index]=='\r'&&++Index==Length){\
								Fail("Unfinished short string");\
							}\
							ADD_CHARACTER('\n');\
							break;\
						}\
						case'\r':{\
							if(++Index==Length||Buffer[Index]=='\n'&&++Index==Length){\
								Fail("Unfinished short string");\
							}\
							ADD_CHARACTER('\n');\
							break;\
						}\
						default:{\
							if(Buffer[Index]==EndingCharacter){\
								if(++Index==Length){\
									goto Done;\
								}\
								goto Loop;\
							}\
							ADD_CHARACTER(Buffer[Index]);\
							if(++Index==Length){\
								Fail("Unfinished short string");\
							}\
						}\
					}
				for(;;){
					#define ADD_CHARACTER(...)\
						if(String->Short.Length==sizeof(String->Short.Buffer)){\
							MAKE_LONG_STRING;\
							String->Long.Buffer[String->Long.Length++]=__VA_ARGS__;\
							goto Long;\
						}\
						String->Short.Buffer[String->Short.Length++]=__VA_ARGS__
					#define ALLOCATE(...)\
						if(String->Short.Length>sizeof(String->Short.Buffer)-(__VA_ARGS__)){\
							MAKE_LONG_STRING;\
							UNICODE_ADD##__VA_ARGS__(Long);\
							goto Long;\
						}
					PARSE(PARSE_FAIL_DIRECT,Short);
					#undef ADD_CHARACTER
					#undef ALLOCATE
				}
				#undef MAKE_LONG_STRING
				Long:;
				if(++Index==Length){
					PARSE_FAIL_CONTENTS("Unfinished short string");
				}
				for(;;){
					#define ADD_CHARACTER(...)\
						if(String->Long.Length==String->Long.Capacity){\
							size_t NewCapacity;\
							if(String->Long.Capacity>LSIZE_MAX>>1){\
								if(String->Long.Capacity==LSIZE_MAX){\
									PARSE_FAIL_CONTENTS("Error allocating short string");\
								}\
								NewCapacity=LSIZE_MAX;\
							}else{\
								NewCapacity=String->Long.Capacity<<1;\
							}\
							char*NewBuffer=realloc(String->Long.Buffer,NewCapacity);\
							if(!NewBuffer){\
								PARSE_FAIL_CONTENTS("Error allocating short string");\
							}\
							String->Long.Capacity=NewCapacity;\
							String->Long.Buffer=NewBuffer;\
						}\
						String->Long.Buffer[String->Long.Length++]=__VA_ARGS__
					#define ALLOCATE(...)\
						size_t Amount=__VA_ARGS__;\
						if(String->Long.Length>String->Long.Capacity-Amount){\
							size_t NewCapacity;\
							if(String->Long.Capacity>LSIZE_MAX>>1){\
								if(String->Long.Capacity>LSIZE_MAX-Amount){\
									PARSE_FAIL_CONTENTS("Error allocating short string");\
								}\
								NewCapacity=LSIZE_MAX;\
							}else{\
								NewCapacity=String->Long.Capacity<<1;\
							}\
							char*NewBuffer=realloc(String->Long.Buffer,NewCapacity);\
							if(!NewBuffer){\
								PARSE_FAIL_CONTENTS("Error allocating short string");\
							}\
							String->Long.Capacity=NewCapacity;\
							String->Long.Buffer=NewBuffer;\
						}
					PARSE(PARSE_FAIL_CONTENTS,Long);
					#undef ADD_CHARACTER
					#undef ALLOCATE
				}
				#undef UNICODE_ADD2
				#undef UNICODE_ADD3
				#undef UNICODE_ADD4
				#undef UNICODE_ADD5
				#undef UNICODE_ADD6
				#undef SINGLE_CHARACTER_ESCAPE_SEQUENCE
				#undef PARSE
			}
			default:{
				if(IsAlphabetic(Buffer[Index])||Buffer[Index]=='_'){
					size_t NameIndex=Index;
					#define CREATE_NAME\
						size_t NameLength=Index-NameIndex;\
						if(NameLength<=sizeof(Start.Short.Buffer)){\
							CREATE_NON_SYMBOL(Name);\
							Name->Type=TOKEN_SHORT_NAME;\
							memcpy(Name->Short.Buffer,Buffer+NameIndex,Name->Short.Length=NameLength);\
						}else if(NameLength>LSIZE_MAX){\
							PARSE_FAIL("Error allocating name");\
						}else{\
							CREATE_NON_SYMBOL(Name);\
							char*NameBuffer=malloc(NameLength);\
							if(!NameBuffer){\
								PARSE_FAIL_DIRECT("Error allocating name");\
							}\
							Name->Type=TOKEN_LONG_NAME;\
							memcpy(Name->Long.Buffer=NameBuffer,Buffer+NameIndex,Name->Long.Length=Name->Long.Capacity=NameLength);\
						}
					for(;;){
						if(++Index==Length){
							CREATE_NAME;
							goto Done;
						}
						if(!IsAlphanumeric(Buffer[Index])&&Buffer[Index]!='_'){
							break;
						}
					}
					CREATE_NAME;
					#undef CREATE_NAME
					break;
				}
				if(IsDigit(Buffer[Index])){
					PARSE_NUMBER;
					break;
				}
				if(IsSpace(Buffer[Index])){
					if(++Index==Length){
						goto Done;
					}
					break;
				}
				STATIC_ERROR(Error,"Invalid character");
			}
			#undef CREATE_NUMBER
			#undef PARSE_NUMBER
			Fail:;
			if(Result.First=Start.Next){
				while(Result.First!=Result.Last){
					Token*Next=Result.First->Next;
					FreeToken(Result.First);
					if(Next==Result.Last){
						break;
					}
					Result.First=Next->Next;
					FreeToken(Next);
				}
				FreeToken(Result.First);
				Result.First=Result.Last=0;
				return Result;
			}
			Result.Last=0;
			return Result;
			FailContents:;
			FreeContentsDirect(Result.Last);
			FailDirect:;
			Result.First=Start.Next;
			while(Result.First!=Result.Last){
				Token*Next=Result.First->Next;
				FreeToken(Result.First);
				if(Next==Result.Last){
					break;
				}
				Result.First=Next->Next;
				FreeToken(Next);
			}
			FreeTokenDirect(Result.Last);
			Result.First=Result.Last=0;
			return Result;
		}
	}
	#undef CREATE_NEW_TOKEN
	#undef CREATE_NON_SYMBOL
	#undef CREATE_SYMBOL
	Done:;
	if(NotNowAmount){
		PARSE_FAIL("Invalid notnows");
	}
	#undef PARSE_FAIL
	#undef PARSE_FAIL_DIRECT
	#undef PARSE_FAIL_CONTENTS
	if(!(Result.First=Start.Next)){
		Result.Last=0;
	}
	return Result;
}
/*
the type of a predefined macro:
Dollar is the first token in the macro expasion (the $ token)
MacroName is the second token in the macro expansion (the macro name)
State and L are the preprocessor and Lua states currently in use

a macro must return the first result of the expansion, even in error

the macro may only manipulate the Dollar token and all tokens which come after it, as well as the Next pointer of the token before Dollar, even in error
*/
typedef struct NamedPredefinedMacro NamedPredefinedMacro;
struct NamedPredefinedMacro{
	const char*Buffer;
	size_t Length;/* Length<=LSIZE_MAX */
	Token*(*Function)(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L);
};
static char PredefinedMacroMetatable[1];/* the address of this is used as a key in the Lua registry to store the metatable of a predefined macro */
static char PreprocessorStateMetatable[1];/* the address of this is used as a key in the Lua registry to store the metatable of a preprocessor state */
static Token*HandleDollar(Token*Dollar,PreprocessorState*State,lua_State*L){
	char*NameBuffer;
	size_t NameLength;
	Token*Name=Dollar->Next;
	if(!lua_checkstack(L,6)){/* all predefined macros can rely on having 6 stack space */
		STATIC_ERROR(&State->Error,"C stack overflow when finding macro");
		return Dollar;
	}
	lua_getiuservalue(L,1,1);
	lua_pushcfunction(L,LuaGetTable);
	lua_pushvalue(L,-2);
	lua_pushcfunction(L,LuaPushString);
	for(;;){
		if(Name->Type<=TOKEN_SHORT_NAME){
			lua_pushlightuserdata(L,NameBuffer=Name->Short.Buffer);
			lua_pushinteger(L,NameLength=Name->Short.Length);
			break;
		}
		if(Name->Type>=TOKEN_LONG_NAME){
			lua_pushlightuserdata(L,NameBuffer=Name->Long.Buffer);
			lua_pushinteger(L,NameLength=Name->Long.Length);
			break;
		}
		if(Name->Type!=TOKEN_SYMBOL||Name->Symbol.Type!=SYMBOL_DOLLAR||Name->Symbol.NotNowAmount){
			STATIC_ERROR(&State->Error,"Expected a name or string after the dollar");
			lua_pop(L,4);
			return Dollar;
		}
		Name=HandleDollar(Name,State,L);
		if(State->Error.Message){
			return Dollar;
		}
	}
	if(lua_pcall(L,2,1,0)){
		STATIC_ERROR(&State->Error,"Error allocating string when finding macro");
		lua_pop(L,4);
		return Dollar;
	}
	if(lua_pcall(L,2,1,0)){
		STATIC_ERROR(&State->Error,"Error when finding macro");
		lua_pop(L,2);
		return Dollar;
	}
	#define ADD_ERROR_INFO(Buffer,Length_,...)\
		size_t BufferLength=Length_;\
		if(State->Error.Type==ERROR_STATIC){\
			if(BufferLength>LSIZE_MAX-21){\
				MemoryError(&State->Error);\
			}\
			size_t Length=BufferLength+21;\
			if(State->Error.Length>LSIZE_MAX-Length){\
				MemoryError(&State->Error);\
				__VA_ARGS__;\
			}\
			size_t MessageLength=State->Error.Length+Length;\
			size_t Capacity=MessageLength>LSIZE_MAX/2?LSIZE_MAX:MessageLength*2;\
			char*Message=malloc(Capacity);\
			if(!Message){\
				MemoryError(&State->Error);\
				__VA_ARGS__;\
			}\
			memcpy(Message,State->Error.Message,State->Error.Length);\
			State->Error.Message=Message;\
			memcpy(Message+=State->Error.Length,"\nwhile running macro ",21);\
			memcpy(Message+21,Buffer,BufferLength);\
			State->Error.Length=MessageLength;\
			State->Error.Capacity=Capacity;\
			State->Error.Type=ERROR_ALLOCATED;\
		}else if(State->Error.Type==ERROR_ALLOCATED){\
			if(BufferLength>LSIZE_MAX-21){\
				MemoryError(&State->Error);\
			}\
			size_t Length=BufferLength+21;\
			if(State->Error.Capacity-State->Error.Length<Length){\
				if(State->Error.Capacity>LSIZE_MAX-Length){\
					MemoryError(&State->Error);\
					__VA_ARGS__;\
				}\
				size_t NewCapacity=State->Error.Capacity+Length;\
				char*NewMessage=realloc(State->Error.Message,NewCapacity=NewCapacity>LSIZE_MAX/2?LSIZE_MAX:NewCapacity*2);\
				if(!NewMessage){\
					MemoryError(&State->Error);\
					__VA_ARGS__;\
				}\
				State->Error.Message=NewMessage;\
				State->Error.Capacity=NewCapacity;\
			}\
			char*Extra=State->Error.Message+State->Error.Length;\
			memcpy(Extra,"\nwhile running macro ",21);\
			memcpy(Extra+21,Buffer,BufferLength);\
			State->Error.Length+=Length;\
		}\
		__VA_ARGS__
	lua_Integer Depth=0;/* this is required to tell whether $foo.bar was invoking macro foo or invoking macro bar in namespace foo */
	if(!lua_isfunction(L,-1)){
		if(lua_type(L,-1)==LUA_TUSERDATA){
			lua_copy(L,-1,-2);
			lua_pop(L,1);
			lua_getmetatable(L,-1);
			lua_rawgetp(L,LUA_REGISTRYINDEX,PredefinedMacroMetatable);
			if(!lua_rawequal(L,-1,-2)){
				STATIC_ERROR(&State->Error,"Expected the macro to be a function");
				lua_pop(L,3);
				return Dollar;
			}
			NamedPredefinedMacro Macro=*(NamedPredefinedMacro*)lua_touserdata(L,-3);
			if(Macro.Length!=NameLength||memcmp(Macro.Buffer,NameBuffer,NameLength)){/* the predefined macros can assume that the name is correct, to avoid checking if the macro name is a long string or name */
				STATIC_ERROR(&State->Error,"Invalid name for predefined macro");
				lua_pop(L,3);
				return Dollar;
			}
			lua_pop(L,3);
			Token*Result=Macro.Function(Dollar,Name,State,L);
			if(State->Error.Message){
				ADD_ERROR_INFO(Macro.Buffer,Macro.Length,return Result);
			}
			return Result;
		}
		for(;;){
			if(!lua_istable(L,-1)){
				STATIC_ERROR(&State->Error,"Expected the macro to be a function");
				lua_pop(L,2);
				return Dollar;
			}
			if(Depth==LUA_MAXINTEGER){
				lua_pop(L,2);
				STATIC_ERROR(&State->Error,"Maximum macro namespace depth exceeded");
				return Dollar;
			}
			++Depth;
			lua_copy(L,-1,-2);
			lua_pop(L,1);
			Name=Name->Next;
			for(;;){
				if(Name->Type!=TOKEN_SYMBOL||Name->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected a period to delimit names after the dollar");
					lua_pop(L,1);
					return Dollar;
				}
				if(Name->Symbol.Type==SYMBOL_DOT){
					break;
				}
				if(Name->Symbol.Type!=SYMBOL_DOLLAR){
					STATIC_ERROR(&State->Error,"Expected a period to delimit names after the dollar");
					lua_pop(L,1);
					return Dollar;
				}
				Name=HandleDollar(Name,State,L);
				if(State->Error.Message){
					lua_pop(L,1);
					return Dollar;
				}
			}
			Name=Name->Next;
			lua_pushcfunction(L,LuaGetTable);
			lua_pushvalue(L,-2);
			lua_pushcfunction(L,LuaPushString);
			for(;;){
				if(Name->Type<=TOKEN_SHORT_NAME){
					lua_pushlightuserdata(L,NameBuffer=Name->Short.Buffer);
					lua_pushinteger(L,NameLength=Name->Short.Length);
					break;
				}
				if(Name->Type>=TOKEN_LONG_NAME){
					lua_pushlightuserdata(L,NameBuffer=Name->Long.Buffer);
					lua_pushinteger(L,NameLength=Name->Long.Length);
					break;
				}
				if(Name->Type!=TOKEN_SYMBOL||Name->Symbol.Type!=SYMBOL_DOLLAR||Name->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected a name or string after the period after the dollar");
					lua_pop(L,4);
					return Dollar;
				}
				Name=HandleDollar(Name,State,L);
				if(State->Error.Message){
					lua_pop(L,4);
					return Dollar;
				}
			}
			if(lua_pcall(L,2,1,0)){
				STATIC_ERROR(&State->Error,"Error allocating string when finding macro");
				lua_pop(L,4);
				return Dollar;
			}
			if(lua_pcall(L,2,1,0)){
				STATIC_ERROR(&State->Error,"Error when finding macro");
				lua_pop(L,2);
				return Dollar;
			}
			if(lua_isfunction(L,-1)){
				break;
			}
		}
	}
	lua_pushcfunction(L,LuaPushString);
	lua_pushlightuserdata(L,NameBuffer);
	lua_pushinteger(L,NameLength);
	if(lua_pcall(L,2,1,0)){
		MemoryError(&State->Error);
		lua_pop(L,3);
		return Dollar;
	}
	lua_copy(L,-1,-3);
	lua_pop(L,1);
	lua_pushvalue(L,1);
	lua_pushinteger(L,Depth);
	Token*Previous=State->CursorStart=(State->Cursor=Dollar)->Previous;
	if(lua_pcall(L,2,0,0)){
		size_t StringLength;
		const char*String=lua_tolstring(L,-2,&StringLength);
		if(State->Error.Message){
			ADD_ERROR_INFO(String,StringLength,lua_pop(L,2);return Previous->Next);
		}
		if(lua_type(L,-1)!=LUA_TSTRING){
			if(StringLength>LSIZE_MAX-26){
				MemoryError(&State->Error);
				lua_pop(L,2);
				return Previous->Next;
			}
			size_t Length=StringLength+26;
			char*Message=malloc(Length);
			if(!Message){
				MemoryError(&State->Error);
				lua_pop(L,2);
				return Previous->Next;
			}
			memcpy(State->Error.Message=Message,"Error while running macro ",26);
			memcpy(Message+26,String,StringLength);
			State->Error.Capacity=State->Error.Length=Length;
			State->Error.Type=ERROR_ALLOCATED;
			lua_pop(L,2);
			return Previous->Next;
		}
		size_t ErrorLength;
		const char*Error=lua_tolstring(L,-1,&ErrorLength);
		if(StringLength>LSIZE_MAX-21){
			MemoryError(&State->Error);
			lua_pop(L,2);
			return Previous->Next;
		}
		size_t Length=StringLength+21;
		if(Length>LSIZE_MAX-ErrorLength){
			MemoryError(&State->Error);
			lua_pop(L,2);
			return Previous->Next;
		}
		char*Message=malloc(Length+=ErrorLength);
		if(!Message){
			MemoryError(&State->Error);
			lua_pop(L,2);
			return Previous->Next;
		}
		memcpy(State->Error.Message=Message,Error,ErrorLength);
		memcpy(Message+=ErrorLength,"\nwhile running macro ",21);
		memcpy(Message+21,String,StringLength);
		State->Error.Capacity=State->Error.Length=Length;
		State->End.Type=ERROR_ALLOCATED;
		lua_pop(L,2);
		return Previous->Next;
	}
	if(State->Error.Message){
		size_t StringLength;
		const char*String=lua_tolstring(L,-1,&StringLength);
		ADD_ERROR_INFO(String,StringLength,lua_pop(L,1);return Previous->Next);
	}
	#undef ADD_ERROR_INFO
	lua_pop(L,1);
	State->CursorStart=0;
	return Previous->Next;
}
static PreprocessorState*PushPreprocessorState(lua_State*L){
	lua_newuserdatauv(L,sizeof(PreprocessorState),1);
	lua_rawgetp(L,LUA_REGISTRYINDEX,PreprocessorStateMetatable);
	lua_setmetatable(L,-2);
	PreprocessorState*State=lua_touserdata(L,-1);
	State->End.Type=State->Start.Type=TOKEN_INVALID;
	(State->Start.Next=&State->End)->Next=(State->End.Previous=&State->Start)->Previous=0;
	State->Error.Message=0;
	State->Error.Type=ERROR_STATIC;
	return State;
}
static PreprocessorState*GetPreprocessorStateRaw(lua_State*L,int Index){
	if(lua_type(L,Index)!=LUA_TUSERDATA||!lua_getmetatable(L,Index)){
		luaL_argerror(L,Index,"expected cursor");
	}
	lua_rawgetp(L,LUA_REGISTRYINDEX,PreprocessorStateMetatable);
	if(!lua_rawequal(L,-1,-2)){
		luaL_argerror(L,Index,"expected cursor");
	}
	lua_pop(L,2);
	return lua_touserdata(L,Index);
}
static PreprocessorState*GetPreprocessorState(lua_State*L,int Index){
	PreprocessorState*State=GetPreprocessorStateRaw(L,Index);
	if(State->Error.Message||!State->CursorStart){
		luaL_argerror(L,Index,"invalid use of cursor");
	}
	return State;
}
static int MacroGetContent(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	switch(Cursor->Type){
		case TOKEN_SHORT_STRING:case TOKEN_SHORT_NAME:{
			lua_pushlstring(L,Cursor->Short.Buffer,Cursor->Short.Length);
			break;
		}
		case TOKEN_INVALID:{
			luaL_argerror(L,1,"invalid use of cursor");
		}
		case TOKEN_INTEGER:{
			lua_pushinteger(L,Cursor->Integer);
			break;
		}
		case TOKEN_FLOAT:{
			lua_pushnumber(L,Cursor->Float);
			break;
		}
		case TOKEN_SYMBOL:{
			lua_pushlstring(L,SymbolTokens[Cursor->Symbol.Type],SymbolTokenLengths[Cursor->Symbol.Type]);
			break;
		}
		case TOKEN_LONG_NAME:case TOKEN_LONG_STRING:{
			lua_pushlstring(L,Cursor->Long.Buffer,Cursor->Long.Length);
		}
	}
	return 1;
}
static int MacroSetContent(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	switch(Cursor->Type){
		#define HANDLE_CASES(ShortType,LongType,...)\
			case ShortType:{\
				size_t Length;\
				const char*Buffer=luaL_checklstring(L,2,&Length);\
				__VA_ARGS__;\
				if(Length>sizeof(Cursor->Short.Buffer)){\
					char*NewBuffer=malloc(Length);\
					if(!NewBuffer){\
						luaL_error(L,"not enough memory");\
					}\
					Cursor->Type=LongType;\
					memcpy(Cursor->Long.Buffer=NewBuffer,Buffer,Cursor->Long.Capacity=Cursor->Long.Length=Length);\
				}else{\
					memcpy(Cursor->Short.Buffer,Buffer,Cursor->Short.Length=Length);\
				}\
				break;\
			}\
			case LongType:{\
				size_t Length;\
				const char*Buffer=luaL_checklstring(L,2,&Length);\
				__VA_ARGS__;\
				if(Length>sizeof(Cursor->Short.Buffer)){\
					if(Length>Cursor->Long.Capacity){\
						size_t Capacity=Length>LSIZE_MAX>>1?LSIZE_MAX:Length<<1;\
						char*NewBuffer=realloc(Cursor->Long.Buffer,Capacity);\
						if(!NewBuffer){\
							luaL_error(L,"not enough memory");\
						}\
						Cursor->Long.Capacity=Capacity;\
						memcpy(Cursor->Long.Buffer=NewBuffer,Buffer,Cursor->Long.Length=Length);\
					}else{\
						memcpy(Cursor->Long.Buffer,Buffer,Cursor->Long.Length=Length);\
					}\
				}else{\
					free(Cursor->Long.Buffer);\
					Cursor->Type=ShortType;\
					memcpy(Cursor->Short.Buffer,Buffer,Cursor->Short.Length=Length);\
				}\
				break;\
			}
		#define CHECK_NAME\
			if(!IsAlphabetic(*Buffer)&&*Buffer!='_'){\
				luaL_argerror(L,2,"invalid name");\
			}\
			for(size_t Index=1;Index!=Length;++Index){\
				if(!IsAlphanumeric(Buffer[Index])&&Buffer[Index]!='_'){\
					luaL_argerror(L,2,"invalid name");\
				}\
			}
		HANDLE_CASES(TOKEN_SHORT_NAME,TOKEN_LONG_NAME,CHECK_NAME);
		#undef CHECK_NAME
		HANDLE_CASES(TOKEN_SHORT_STRING,TOKEN_LONG_STRING);
		#undef HANDLE_CASES
		case TOKEN_INVALID:{
			luaL_argerror(L,1,"invalid use of cursor");
		}
		case TOKEN_INTEGER:{
			Cursor->Integer=luaL_checkinteger(L,2);
			break;
		}
		case TOKEN_FLOAT:{
			lua_Number Float=luaL_checknumber(L,2);
			if(isnan(Float)){
				luaL_argerror(L,2,"invalid number");
			}
			if(signbit(Float)){
				if(Float){
					luaL_argerror(L,2,"invalid number");
				}
				Cursor->Float=0;
			}else{
				Cursor->Float=Float;
			}
			break;
		}
		case TOKEN_SYMBOL:{
			size_t Length;
			const char*Buffer=luaL_checklstring(L,2,&Length);
			if(Length==1){
				switch(*Buffer){
					#define SINGLE_CHARACTER_CASE(Character,Type_)\
						case Character:{\
							Cursor->Symbol.Type=Type_;\
							break;\
						}
					SINGLE_CHARACTER_CASE('(',SYMBOL_OPEN_PARENTHESIS);
					SINGLE_CHARACTER_CASE('[',SYMBOL_OPEN_BRACKET);
					SINGLE_CHARACTER_CASE('{',SYMBOL_OPEN_BRACE);
					SINGLE_CHARACTER_CASE('+',SYMBOL_ADD);
					SINGLE_CHARACTER_CASE('-',SYMBOL_SUBTRACT);
					SINGLE_CHARACTER_CASE('*',SYMBOL_MULTIPLY);
					SINGLE_CHARACTER_CASE('/',SYMBOL_DIVIDE);
					SINGLE_CHARACTER_CASE('%',SYMBOL_MODULO);
					SINGLE_CHARACTER_CASE('^',SYMBOL_POWER);
					SINGLE_CHARACTER_CASE('<',SYMBOL_LESSER);
					SINGLE_CHARACTER_CASE('>',SYMBOL_GREATER);
					SINGLE_CHARACTER_CASE('&',SYMBOL_AND);
					SINGLE_CHARACTER_CASE('~',SYMBOL_EXCLUSIVE_OR);
					SINGLE_CHARACTER_CASE('|',SYMBOL_OR);
					SINGLE_CHARACTER_CASE('#',SYMBOL_LENGTH);
					SINGLE_CHARACTER_CASE('=',SYMBOL_ASSIGN);
					SINGLE_CHARACTER_CASE('.',SYMBOL_DOT);
					SINGLE_CHARACTER_CASE(',',SYMBOL_COMMA);
					SINGLE_CHARACTER_CASE(':',SYMBOL_COLON);
					SINGLE_CHARACTER_CASE(';',SYMBOL_SEMICOLON);
					SINGLE_CHARACTER_CASE('@',SYMBOL_AT);
					SINGLE_CHARACTER_CASE('!',SYMBOL_BANG);
					SINGLE_CHARACTER_CASE('`',SYMBOL_BACKTICK);
					SINGLE_CHARACTER_CASE('?',SYMBOL_QUESTION);
					SINGLE_CHARACTER_CASE('$',SYMBOL_DOLLAR);
					SINGLE_CHARACTER_CASE('}',SYMBOL_CLOSE_BRACE);
					SINGLE_CHARACTER_CASE(']',SYMBOL_CLOSE_BRACKET);
					SINGLE_CHARACTER_CASE(')',SYMBOL_CLOSE_PARENTHESIS);
					#undef SINGLE_CHARACTER_CASE
					default:{
						luaL_argerror(L,1,"invalid symbol");
					}
				}
			}else if(Length==2){
				switch(*Buffer){
					#define DOUBLE_CHARACTER_CASE(Character,ExtraCharacter,Type_)\
						case Character:{\
							if(Buffer[1]!=(ExtraCharacter)){\
								luaL_argerror(L,1,"invalid symbol");\
							}\
							Cursor->Symbol.Type=Type_;\
							break;\
						}
					DOUBLE_CHARACTER_CASE('/','/',SYMBOL_FLOOR_DIVIDE);
					DOUBLE_CHARACTER_CASE('=','=',SYMBOL_EQUALS);
					DOUBLE_CHARACTER_CASE('~','=',SYMBOL_UNEQUALS);
					DOUBLE_CHARACTER_CASE('.','.',SYMBOL_CONCATENATE);
					DOUBLE_CHARACTER_CASE(':',':',SYMBOL_LABEL);
					#define DOUBLE_CHARACTER_CASE_TWO(Character,ExtraCharacter1,ExtraCharacter2,Type1,Type2)\
						case Character:{\
							if(Buffer[1]==(ExtraCharacter1)){\
								Cursor->Symbol.Type=Type1;\
								break;\
							}\
							if(Buffer[1]==(ExtraCharacter2)){\
								Cursor->Symbol.Type=Type2;\
								break;\
							}\
							luaL_argerror(L,1,"invalid symbol");\
						}
					DOUBLE_CHARACTER_CASE_TWO('<','=','<',SYMBOL_LESSER_EQUAL,SYMBOL_LEFT_SHIFT);
					DOUBLE_CHARACTER_CASE_TWO('>','=','>',SYMBOL_GREATER_EQUAL,SYMBOL_RIGHT_SHIFT);
					#undef DOUBLE_CHARACTER_CASE
					#undef DOUBLE_CHARACTER_CASE_TWO
					default:{
						luaL_argerror(L,1,"invalid symbol");
					}
				}
			}else if(Length==3&&*Buffer=='.'&&Buffer[1]=='.'&&Buffer[2]=='.'){
				Cursor->Symbol.Type=SYMBOL_VARIABLE_ARGUMENTS;
			}else{
				luaL_argerror(L,1,"invalid symbol");
			}
		}
	}
	return 0;
}
static int MacroGetType(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	lua_pushlstring(L,TokenNames[Cursor->Type],TokenNameLengths[Cursor->Type]);
	return 1;
}
static int MacroSetType(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	size_t Length;
	const char*Buffer=luaL_checklstring(L,2,&Length);
	switch(*Buffer){
		case'n':{
			if(Length!=4||Buffer[1]!='a'||Buffer[2]!='m'||Buffer[3]!='e'){
				luaL_argerror(L,2,"invalid type");
			}
			FreeContents(Cursor);
			Cursor->Type=TOKEN_SHORT_NAME;
			memcpy(Cursor->Short.Buffer,"nil",Cursor->Short.Length=3);
			break;
		}
		case'i':{
			if(Length!=7||Buffer[1]!='n'||Buffer[2]!='t'||Buffer[3]!='e'||Buffer[4]!='g'||Buffer[5]!='e'||Buffer[6]!='r'){
				luaL_argerror(L,2,"invalid type");
			}
			FreeContents(Cursor);
			Cursor->Type=TOKEN_INTEGER;
			Cursor->Integer=0;
			break;
		}
		case'f':{
			if(Length!=5||Buffer[1]!='l'||Buffer[2]!='o'||Buffer[3]!='a'||Buffer[4]!='t'){
				luaL_argerror(L,2,"invalid type");
			}
			FreeContents(Cursor);
			Cursor->Type=TOKEN_FLOAT;
			Cursor->Float=0;
			break;
		}
		case's':{
			if(Length!=6){
				luaL_argerror(L,2,"invalid type");
			}
			if(Buffer[1]=='t'){
				if(Buffer[2]!='r'||Buffer[3]!='i'||Buffer[4]!='n'||Buffer[5]!='g'){
					luaL_argerror(L,2,"invalid type");
				}
				FreeContents(Cursor);
				Cursor->Type=TOKEN_SHORT_STRING;
				Cursor->Short.Length=0;
			}else if(Buffer[1]!='y'||Buffer[2]!='m'||Buffer[3]!='b'||Buffer[4]!='o'||Buffer[5]!='l'){
				luaL_argerror(L,2,"invalid type");
			}else{
				FreeContents(Cursor);
				Cursor->Type=TOKEN_SYMBOL;
				Cursor->Symbol.Type=SYMBOL_DOLLAR;
				Cursor->Symbol.NotNowAmount=0;
			}
			break;
		}
		default:{
			luaL_argerror(L,2,"invalid type");
		}
	}
	return 0;
}
static int MacroGetNotNowAmount(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(Cursor->Type==TOKEN_SYMBOL){
		lua_pushinteger(L,Cursor->Symbol.NotNowAmount);
	}else if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}else{
		lua_pushinteger(L,0);
	}
	return 1;
}
static int MacroSetNotNowAmount(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(Cursor->Type==TOKEN_SYMBOL){
		lua_Integer NotNowAmount=luaL_checkinteger(L,2);
		if(NotNowAmount<0){
			luaL_argerror(L,2,"out of bounds");
		}
		Cursor->Symbol.NotNowAmount=NotNowAmount;
	}else if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}else if(luaL_checkinteger(L,2)){
		luaL_argerror(L,2,"notnow for non-symbol must be zero");
	}
	return 0;
}
static int MacroIsValid(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	lua_pushboolean(L,State->Cursor->Type);
	return 1;
}
static int MacroMakeInvalid(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	State->Cursor=&State->End;
	return 0;
}
static int MacroIsAdvancingValid(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	lua_pushboolean(L,Cursor->Next->Type);
	return 1;
}
static int MacroIsRetreatingValid(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	lua_pushboolean(L,Cursor->Previous!=State->CursorStart);
	return 1;
}
static int MacroGoToStart(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	State->Cursor=State->CursorStart->Next;
	return 0;
}
static int MacroGoToEnd(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->End.Previous;
	State->Cursor=Cursor==State->CursorStart?&State->End:Cursor;
	return 0;
}
static int MacroAdvance(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	State->Cursor=Cursor->Next;
	return 0;
}
static int MacroRetreat(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Previous=Cursor->Previous;
	State->Cursor=Previous!=State->CursorStart?Previous:&State->End;
	return 0;
}
static int MacroRemoveAndAdvance(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Next=Cursor->Next;
	Token*Previous=Cursor->Previous;
	State->Cursor=(Next->Previous=Previous)->Next=Next;
	FreeToken(Cursor);
	return 0;
}
static int MacroRemoveAndRetreat(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Next=Cursor->Next;
	Token*Previous=Cursor->Previous;
	State->Cursor=((Previous->Next=Next)->Previous=Previous)!=State->CursorStart?Previous:&State->End;
	FreeToken(Cursor);
	return 0;
}
static int MacroInsertAtStart(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Result=CreateToken();
	if(!Result){
		luaL_error(L,"not enough memory");
	}
	Result->Type=TOKEN_INTEGER;
	Result->Integer=0;
	Token*Previous=State->CursorStart;
	State->Cursor=(Result->Previous=Previous)->Next=(Result->Next=Previous->Next)->Previous=Result;
	return 0;
}
static int MacroInsertAtEnd(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Result=CreateToken();
	if(!Result){
		luaL_error(L,"not enough memory");
	}
	Result->Type=TOKEN_INTEGER;
	Result->Integer=0;
	(State->End.Previous=State->Cursor=(Result->Previous=State->End.Previous)->Next=Result)->Next=&State->End;
	return 0;
}
static int MacroInsertAhead(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Result=CreateToken();
	if(!Result){
		luaL_error(L,"not enough memory");
	}
	Result->Type=TOKEN_INTEGER;
	Result->Integer=0;
	State->Cursor=(Result->Previous=Cursor)->Next=(Result->Next=Cursor->Next)->Previous=Result;
	return 0;
}
static int MacroInsertBehind(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Result=CreateToken();
	if(!Result){
		luaL_error(L,"not enough memory");
	}
	Result->Type=TOKEN_INTEGER;
	Result->Integer=0;
	State->Cursor=(Result->Next=Cursor)->Previous=(Result->Previous=Cursor->Previous)->Next=Result;
	return 0;
}
static int MacroInsertAtStartAndStay(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Result=CreateToken();
	if(!Result){
		luaL_error(L,"not enough memory");
	}
	Result->Type=TOKEN_INTEGER;
	Result->Integer=0;
	Token*Previous=State->CursorStart;
	(Result->Previous=Previous)->Next=(Result->Next=Previous->Next)->Previous=Result;
	return 0;
}
static int MacroInsertAtEndAndStay(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Result=CreateToken();
	if(!Result){
		luaL_error(L,"not enough memory");
	}
	Result->Type=TOKEN_INTEGER;
	Result->Integer=0;
	(State->End.Previous=(Result->Previous=State->End.Previous)->Next=Result)->Next=&State->End;
	return 0;
}
static int MacroInsertAheadAndStay(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Result=CreateToken();
	if(!Result){
		luaL_error(L,"not enough memory");
	}
	Result->Type=TOKEN_INTEGER;
	Result->Integer=0;
	(Result->Previous=Cursor)->Next=(Result->Next=Cursor->Next)->Previous=Result;
	return 0;
}
static int MacroInsertBehindAndStay(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Result=CreateToken();
	if(!Result){
		luaL_error(L,"not enough memory");
	}
	Result->Type=TOKEN_INTEGER;
	Result->Integer=0;
	(Result->Next=Cursor)->Previous=(Result->Previous=Cursor->Previous)->Next=Result;
	return 0;
}
static int MacroStealToStartAndAdvance(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Moving=GetPreprocessorState(L,2);
	Token*Cursor=Moving->Cursor;
	if(State==Moving||!Cursor->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*Previous=State->CursorStart;
	Token*OldNext=Cursor->Next;
	Moving->Cursor=(OldNext->Previous=Cursor->Previous)->Next=OldNext;
	State->Cursor=(Cursor->Previous=Previous)->Next=(Cursor->Next=Previous->Next)->Previous=Cursor;
	return 0;
}
static int MacroStealToEndAndAdvance(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Moving=GetPreprocessorState(L,2);
	Token*Cursor=Moving->Cursor;
	if(State==Moving||!Cursor->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*OldNext=Cursor->Next;
	Moving->Cursor=(OldNext->Previous=Cursor->Previous)->Next=OldNext;
	(State->End.Previous=State->Cursor=(Cursor->Previous=State->End.Previous)->Next=Cursor)->Next=&State->End;
	return 0;
}
static int MacroStealAheadAndAdvance(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Moving=GetPreprocessorState(L,2);
	Token*Cursor=Moving->Cursor;
	if(State==Moving||!Cursor->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*Previous=State->Cursor;
	if(!Previous->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*OldNext=Cursor->Next;
	Moving->Cursor=(OldNext->Previous=Cursor->Previous)->Next=OldNext;
	State->Cursor=(Cursor->Previous=Previous)->Next=(Cursor->Next=Previous->Next)->Previous=Cursor;
	return 0;
}
static int MacroStealBehindAndAdvance(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Moving=GetPreprocessorState(L,2);
	Token*Cursor=Moving->Cursor;
	if(State==Moving||!Cursor->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*Next=State->Cursor;
	if(!Next->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*OldNext=Cursor->Next;
	Moving->Cursor=(OldNext->Previous=Cursor->Previous)->Next=OldNext;
	State->Cursor=(Cursor->Next=Next)->Previous=(Cursor->Previous=Next->Previous)->Next=Cursor;
	return 0;
}
static int MacroStealToStartAndRetreat(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Moving=GetPreprocessorState(L,2);
	Token*Cursor=Moving->Cursor;
	if(State==Moving||!Cursor->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*Previous=State->CursorStart;
	Token*OldPrevious=Cursor->Previous;
	Moving->Cursor=((OldPrevious->Next=Cursor->Next)->Previous=OldPrevious)!=Moving->CursorStart?OldPrevious:&Moving->End;
	State->Cursor=(Cursor->Previous=Previous)->Next=(Cursor->Next=Previous->Next)->Previous=Cursor;
	return 0;
}
static int MacroStealToEndAndRetreat(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Moving=GetPreprocessorState(L,2);
	Token*Cursor=Moving->Cursor;
	if(State==Moving||!Cursor->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*OldPrevious=Cursor->Previous;
	Moving->Cursor=((OldPrevious->Next=Cursor->Next)->Previous=OldPrevious)!=Moving->CursorStart?OldPrevious:&Moving->End;
	(State->End.Previous=State->Cursor=(Cursor->Previous=State->End.Previous)->Next=Cursor)->Next=&State->End;
	return 0;
}
static int MacroStealAheadAndRetreat(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Moving=GetPreprocessorState(L,2);
	Token*Cursor=Moving->Cursor;
	if(State==Moving||!Cursor->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*Previous=State->Cursor;
	if(!Previous->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*OldPrevious=Cursor->Previous;
	Moving->Cursor=((OldPrevious->Next=Cursor->Next)->Previous=OldPrevious)!=Moving->CursorStart?OldPrevious:&Moving->End;
	State->Cursor=(Cursor->Previous=Previous)->Next=(Cursor->Next=Previous->Next)->Previous=Cursor;
	return 0;
}
static int MacroStealBehindAndRetreat(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Moving=GetPreprocessorState(L,2);
	Token*Cursor=Moving->Cursor;
	if(State==Moving||!Cursor->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*Next=State->Cursor;
	if(!Next->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*OldPrevious=Cursor->Previous;
	Moving->Cursor=((OldPrevious->Next=Cursor->Next)->Previous=OldPrevious)!=Moving->CursorStart?OldPrevious:&Moving->End;
	State->Cursor=(Cursor->Next=Next)->Previous=(Cursor->Previous=Next->Previous)->Next=Cursor;
	return 0;
}
static int MacroHandleDollar(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(Cursor->Type!=TOKEN_SYMBOL||Cursor->Symbol.Type!=SYMBOL_DOLLAR||Cursor->Symbol.NotNowAmount){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*CursorStart=State->CursorStart;
	State->CursorStart=0;
	State->Cursor=HandleDollar(Cursor,State,L);
	State->CursorStart=CursorStart;
	if(State->Error.Message){
		lua_pushlstring(L,State->Error.Message,State->Error.Length);
		lua_error(L);
	}
	return 0;
}
static int MacroHandleDollarsAndNotNows(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	for(Token*Cursor=State->Cursor;;){
		if(Cursor->Type!=TOKEN_SYMBOL){
			lua_pushboolean(L,0);
			return 1;
		}
		if(Cursor->Symbol.NotNowAmount){
			--Cursor->Symbol.NotNowAmount;
			lua_pushboolean(L,1);
			return 1;
		}
		if(Cursor->Symbol.Type!=SYMBOL_DOLLAR){
			lua_pushboolean(L,0);
			return 1;
		}
		Token*CursorStart=State->CursorStart;
		State->CursorStart=0;
		State->Cursor=Cursor=HandleDollar(Cursor,State,L);
		State->CursorStart=CursorStart;
		if(State->Error.Message){
			lua_pushlstring(L,State->Error.Message,State->Error.Length);
			lua_error(L);
		}
	}
}
static int MacroCopy(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*CopyingState=GetPreprocessorState(L,2);
	if(State==CopyingState){
		return 0;
	}
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Copying=CopyingState->Cursor;
	switch(Copying->Type){
		case TOKEN_SHORT_STRING:case TOKEN_SHORT_NAME:{
			FreeContents(Cursor);
			Cursor->Type=Copying->Type;
			memcpy(Cursor->Short.Buffer,Copying->Short.Buffer,Cursor->Short.Length=Copying->Short.Length);
			break;
		}
		case TOKEN_LONG_NAME:case TOKEN_LONG_STRING:{
			char*Buffer;
			size_t Capacity;
			if(Cursor->Type>=TOKEN_LONG_NAME){
				if(Cursor->Long.Capacity>=Copying->Long.Length){
					Cursor->Type=Copying->Type;
					memcpy(Cursor->Long.Buffer,Copying->Long.Buffer,Cursor->Long.Length=Copying->Long.Length);
					break;
				}
				Buffer=realloc(Cursor->Long.Buffer,Capacity=Copying->Long.Length>LSIZE_MAX>>1?LSIZE_MAX:Copying->Long.Length<<1);
			}else{
				Buffer=malloc(Capacity=Copying->Long.Length);
			}
			if(!Buffer){
				luaL_error(L,"not enough memory");
			}
			Cursor->Type=Copying->Type;
			Cursor->Long.Capacity=Capacity;
			memcpy(Cursor->Long.Buffer=Buffer,Copying->Long.Buffer,Cursor->Long.Length=Copying->Long.Length);
			break;
		}
		case TOKEN_INVALID:{
			luaL_argerror(L,2,"invalid use of cursor");
		}
		case TOKEN_INTEGER:{
			FreeContents(Cursor);
			Cursor->Type=TOKEN_INTEGER;
			Cursor->Integer=Copying->Integer;
			break;
		}
		case TOKEN_FLOAT:{
			FreeContents(Cursor);
			Cursor->Type=TOKEN_FLOAT;
			Cursor->Float=Copying->Float;
			break;
		}
		case TOKEN_SYMBOL:{
			FreeContents(Cursor);
			Cursor->Type=TOKEN_SYMBOL;
			Cursor->Symbol.Type=Copying->Symbol.Type;
			Cursor->Symbol.NotNowAmount=Copying->Symbol.NotNowAmount;
		}
	}
	return 0;
}
static int MacroShiftToStart(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Next=Cursor->Next;
	(Next->Previous=Cursor->Previous)->Next=Next;
	Next=State->CursorStart;
	(Cursor->Previous=Next)->Next=(Cursor->Next=Next->Next)->Previous=Cursor;
	return 0;
}
static int MacroShiftToStartAndAdvance(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Next=Cursor->Next;
	State->Cursor=(Next->Previous=Cursor->Previous)->Next=Next;
	Next=State->CursorStart;
	(Cursor->Previous=Next)->Next=(Cursor->Next=Next->Next)->Previous=Cursor;
	return 0;
}
static int MacroShiftToStartAndRetreat(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Previous=Cursor->Previous;
	State->Cursor=((Previous->Next=Cursor->Next)->Previous=Previous)!=State->CursorStart?Previous:&State->End;
	Previous=State->CursorStart;
	(Cursor->Previous=Previous)->Next=(Cursor->Next=Previous->Next)->Previous=Cursor;
	return 0;
}
static int MacroShiftToEnd(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Next=Cursor->Next;
	(Next->Previous=Cursor->Previous)->Next=Next;
	(State->End.Previous=(Cursor->Previous=State->End.Previous)->Next=Cursor)->Next=&State->End;
	return 0;
}
static int MacroShiftToEndAndAdvance(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Next=Cursor->Next;
	State->Cursor=(Next->Previous=Cursor->Previous)->Next=Next;
	(State->End.Previous=(Cursor->Previous=State->End.Previous)->Next=Cursor)->Next=&State->End;
	return 0;
}
static int MacroShiftToEndAndRetreat(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Previous=Cursor->Previous;
	State->Cursor=((Previous->Next=Cursor->Next)->Previous=Previous)!=State->CursorStart?Previous:&State->End;
	(State->End.Previous=(Cursor->Previous=State->End.Previous)->Next=Cursor)->Next=&State->End;
	return 0;
}
static int MacroSwapWithStart(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Previous=State->CursorStart;
	Token*Swapping=Previous->Next;
	Token*Next=Swapping->Next;
	if(Next==Cursor){
		(State->Cursor=Cursor->Next=(Swapping->Next=Cursor->Next)->Previous=Swapping)->Previous=(Cursor->Previous=Swapping->Previous)->Next=Cursor;
	}else{
		State->Cursor=(Swapping->Next=Cursor->Next)->Previous=(Swapping->Previous=Cursor->Previous)->Next=Swapping;
		(Cursor->Next=Next)->Previous=(Cursor->Previous=Previous)->Next=Cursor;
	}
	return 0;
}
static int MacroSwapWithEnd(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Swapping=State->End.Previous;
	Token*Previous=Swapping->Previous;
	if(Previous==Cursor){
		(State->Cursor=Cursor->Previous=(Swapping->Previous=Cursor->Previous)->Next=Swapping)->Next=(Cursor->Next=&State->End)->Previous=Cursor;
	}else{
		State->Cursor=(Swapping->Next=Cursor->Next)->Previous=(Swapping->Previous=Cursor->Previous)->Next=Swapping;
		(Cursor->Next=&State->End)->Previous=(Cursor->Previous=Previous)->Next=Cursor;
	}
	return 0;
}
static int MacroSwapAhead(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Next=Cursor->Next;
	if(!Next->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	(State->Cursor=Cursor->Previous=(Next->Previous=Cursor->Previous)->Next=Next)->Next=(Cursor->Next=Next->Next)->Previous=Cursor;
	return 0;
}
static int MacroSwapBehind(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Previous=Cursor->Previous;
	if(Previous==State->CursorStart){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	(State->Cursor=Cursor->Next=(Previous->Next=Cursor->Next)->Previous=Previous)->Previous=(Cursor->Previous=Previous->Previous)->Next=Cursor;
	return 0;
}
static int MacroSwapBetween(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	PreprocessorState*Swapping=GetPreprocessorState(L,2);
	Token*Cursor=State->Cursor;
	if(!Cursor->Type){
		luaL_argerror(L,1,"invalid use of cursor");
	}
	Token*Other=Swapping->Cursor;
	if(!Other->Type){
		luaL_argerror(L,2,"invalid use of cursor");
	}
	Token*Next=Other->Next;
	Token*Previous=Other->Previous;
	State->Cursor=(Other->Previous=Cursor->Previous)->Next=(Other->Next=Cursor->Next)->Previous=Other;
	Swapping->Cursor=(Cursor->Previous=Previous)->Next=(Cursor->Next=Next)->Previous=Cursor;
	return 0;
}
static int MacroGetMacros(lua_State*L){
	GetPreprocessorState(L,1);
	lua_getiuservalue(L,1,1);
	return 1;
}
static int MacroSetMacros(lua_State*L){
	GetPreprocessorState(L,1);
	luaL_checktype(L,2,LUA_TTABLE);
	lua_settop(L,2);
	lua_setiuservalue(L,1,1);
	return 0;
}
static int MacroGetError(lua_State*L){
	PreprocessorState*State=GetPreprocessorStateRaw(L,1);
	if(State->Error.Message){
		lua_pushlstring(L,State->Error.Message,State->Error.Length);
	}else{
		lua_pushnil(L);
	}
	return 1;
}
static int MacroSetError(lua_State*L){
	PreprocessorState*State=GetPreprocessorStateRaw(L,1);
	size_t Length;
	const char*String=luaL_checklstring(L,2,&Length);
	switch(State->Error.Type){
		#define SET_ERROR(...)\
			if(!Message){\
				__VA_ARGS__;\
				MemoryError(&State->Error);\
				luaL_error(L,"not enough memory");\
			}\
			memcpy(State->Error.Message=Message,String,State->Error.Length=Length);\
			State->Error.Capacity=Capacity
		case ERROR_STATIC:{
			size_t Capacity=Length<16?16:Length;
			char*Message=malloc(Capacity);
			SET_ERROR();
			State->Error.Type=ERROR_ALLOCATED;
			break;
		}
		case ERROR_ALLOCATED:{
			if(Length<=State->Error.Capacity){
				memcpy(State->Error.Message,String,State->Error.Length=Length);
				break;
			}
			size_t Capacity=Length>LSIZE_MAX>>1?LSIZE_MAX:Length<<1;
			char*Message=realloc(State->Error.Message,Capacity);
			SET_ERROR(free(State->Error.Message));
			break;
		}
		#undef SET_ERROR
		case ERROR_MEMORY:{
			luaL_error(L,"not enough memory");
		}
	}
	return 0;
}
static int MacroClear(lua_State*L){
	PreprocessorState*State=GetPreprocessorState(L,1);
	for(Token*Freeing=State->CursorStart->Next;;){
		Token*Next=Freeing->Next;
		if(!Next){
			State->Cursor=(Freeing->Previous=State->CursorStart)->Next=Freeing;
			return 0;
		}
		FreeToken(Freeing);
		Freeing=Next->Next;
		if(!Freeing){
			State->Cursor=(Next->Previous=State->CursorStart)->Next=Next;
			return 0;
		}
		FreeToken(Next);
	}
}
static int LuaCreateTokenList(lua_State*L){
	luaL_checktype(L,1,LUA_TTABLE);
	PreprocessorState*State=PushPreprocessorState(L);
	State->CursorStart=State->Cursor=&State->Start;
	lua_pushvalue(L,1);
	lua_setiuservalue(L,-2,1);
	return 1;
}
static int LuaTokenListGC(lua_State*L){
	PreprocessorState*State=lua_touserdata(L,1);
	if(State->Error.Type==ERROR_ALLOCATED){
		free(State->Error.Message);
	}
	for(Token*Freeing=State->Start.Next;;){
		Token*Next=Freeing->Next;
		if(!Next){
			return 0;
		}
		FreeToken(Freeing);
		Freeing=Next->Next;
		if(!Freeing){
			return 0;
		}
		FreeToken(Next);
	}
}
static Token*PredefinedNow(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	for(Token*AfterMacroName=MacroName->Next;;){
		if(AfterMacroName->Type==TOKEN_SYMBOL){
			if(AfterMacroName->Symbol.Type<=SYMBOL_OPEN_BRACE){
				if(AfterMacroName->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected the opening bracket after $now to not be notnow");
					return Dollar;
				}
				Token*Nowing=AfterMacroName->Next;
				for(size_t BracketsAmount=0;;){
					if(Nowing->Type==TOKEN_SYMBOL){
						if(Nowing->Symbol.NotNowAmount){
							--Nowing->Symbol.NotNowAmount;
						}else if(Nowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
							if(BracketsAmount==SIZE_MAX){
								STATIC_ERROR(&State->Error,"Bracket overflow after $now");
								return Dollar;
							}
							++BracketsAmount;
						}else if(Nowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
							if(!BracketsAmount){
								FreeTokenDirect(MacroName);
								MacroName=Nowing->Next;
								(MacroName->Previous=Nowing->Previous)->Next=MacroName;
								FreeTokenDirect(Nowing);
								MacroName=AfterMacroName->Next;
								FreeTokenDirect(AfterMacroName);
								(MacroName->Previous=Dollar->Previous)->Next=MacroName;
								FreeTokenDirect(Dollar);
								return MacroName;
							}
							--BracketsAmount;
						}else if(Nowing->Symbol.Type==SYMBOL_DOLLAR){
							Nowing=HandleDollar(Nowing,State,L);
							if(State->Error.Message){
								return Dollar;
							}
							continue;
						}
					}else if(!Nowing->Type){
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $now");
						return Dollar;
					}
					Nowing=Nowing->Next;
				}
			}else if(AfterMacroName->Symbol.Type!=SYMBOL_DOLLAR||AfterMacroName->Symbol.NotNowAmount){
				STATIC_ERROR(&State->Error,"Expected a number or opening bracket after $now");
				return Dollar;
			}
		}else if(AfterMacroName->Type==TOKEN_INTEGER){
			NowInteger:;
			if(AfterMacroName->Integer<0){
				STATIC_ERROR(&State->Error,"The number provided to $now N must be nonnegative");
				return Dollar;
			}
			STATIC_ERROR(&State->Error,"$now N is unimplemented");
			return Dollar;
		}else if(AfterMacroName->Type==TOKEN_FLOAT){
			if(!FloatFitsInInteger(AfterMacroName->Float)){
				STATIC_ERROR(&State->Error,"Number provided to $now N doesn't fit into an int");
				return Dollar;
			}
			AfterMacroName->Type=TOKEN_INTEGER;
			AfterMacroName->Integer=AfterMacroName->Float;
			goto NowInteger;
		}else{
			STATIC_ERROR(&State->Error,"Expected a number or opening bracket after $now");
			return Dollar;
		}
		AfterMacroName=HandleDollar(AfterMacroName,State,L);
		if(State->Error.Message){
			return Dollar;
		}
	}
}
static Token*PredefinedNotNow(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	for(Token*AfterMacroName=MacroName->Next;;){
		if(AfterMacroName->Type==TOKEN_SYMBOL){
			if(AfterMacroName->Symbol.Type==SYMBOL_SEMICOLON){
				if(AfterMacroName->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected the semicolon after $notnow to not be notnow");
					return Dollar;
				}
				((Dollar->Next=AfterMacroName->Next)->Previous=Dollar)->Symbol.NotNowAmount=1;
				FreeTokenDirect(AfterMacroName);
				FreeTokenDirect(MacroName);
				return Dollar;
			}else if(AfterMacroName->Symbol.Type==SYMBOL_COLON){
				if(AfterMacroName->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected the colon after $notnow to not be notnow");
					return Dollar;
				}
				Token*NotNowing=AfterMacroName->Next;
				for(;;){
					if(NotNowing->Type!=TOKEN_SYMBOL){
						STATIC_ERROR(&State->Error,"Expected a symbol after $notnow:");
						return Dollar;
					}
					if(NotNowing->Symbol.NotNowAmount){
						break;
					}
					if(NotNowing->Symbol.Type!=SYMBOL_DOLLAR){
						NotNowing->Symbol.NotNowAmount=1;
						break;
					}
					NotNowing=HandleDollar(NotNowing,State,L);
					if(State->Error.Message){
						return Dollar;
					}
				}
				(NotNowing->Previous=Dollar->Previous)->Next=NotNowing;
				FreeTokenDirect(Dollar);
				FreeTokenDirect(MacroName);
				FreeTokenDirect(AfterMacroName);
				return NotNowing;
			}else if(AfterMacroName->Symbol.Type<=SYMBOL_OPEN_BRACE){
				if(AfterMacroName->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected the opening bracket after $notnow to not be notnow");
					return Dollar;
				}
				Token*NotNowing=AfterMacroName->Next;
				for(size_t BracketsAmount=0;;){
					if(NotNowing->Type==TOKEN_SYMBOL){
						if(!NotNowing->Symbol.NotNowAmount){
							if(NotNowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
								if(BracketsAmount==SIZE_MAX){
									STATIC_ERROR(&State->Error,"Bracket overflow after $notnow");
									return Dollar;
								}
								++BracketsAmount;
							}else if(NotNowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
								if(!BracketsAmount){
									FreeTokenDirect(MacroName);
									MacroName=NotNowing->Next;
									(MacroName->Previous=NotNowing->Previous)->Next=MacroName;
									FreeTokenDirect(NotNowing);
									MacroName=AfterMacroName->Next;
									FreeTokenDirect(AfterMacroName);
									(MacroName->Previous=Dollar->Previous)->Next=MacroName;
									FreeTokenDirect(Dollar);
									return MacroName;
								}
								--BracketsAmount;
							}
							NotNowing->Symbol.NotNowAmount=1;
						}
					}else if(!NotNowing->Type){
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow");
						return Dollar;
					}
					NotNowing=NotNowing->Next;
				}
			}else if(AfterMacroName->Symbol.Type==SYMBOL_LABEL){
				if(AfterMacroName->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected the double colon after $notnow to not be notnow");
					return Dollar;
				}
				Token*OpenBracket=AfterMacroName->Next;
				for(;;){
					if(OpenBracket->Type!=TOKEN_SYMBOL){
						STATIC_ERROR(&State->Error,"Expected an opening bracket after $notnow::");
						return Dollar;
					}
					if(OpenBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
						break;
					}
					if(OpenBracket->Symbol.Type!=SYMBOL_DOLLAR||OpenBracket->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected an opening bracket after $notnow::");
						return Dollar;
					}
					OpenBracket=HandleDollar(OpenBracket,State,L);
					if(State->Error.Message){
						return Dollar;
					}
				}
				if(OpenBracket->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected the opening bracket after $notnow:: to not be notnow");
					return Dollar;
				}
				Token*NotNowing=OpenBracket->Next;
				for(size_t BracketsAmount=0;;){
					if(NotNowing->Type==TOKEN_SYMBOL){
						if(!NotNowing->Symbol.NotNowAmount){
							if(NotNowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
								if(BracketsAmount==SIZE_MAX){
									STATIC_ERROR(&State->Error,"Bracket overflow after $notnow::");
									return Dollar;
								}
								++BracketsAmount;
							}else if(NotNowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
								if(!BracketsAmount){
									FreeTokenDirect(MacroName);
									FreeTokenDirect(AfterMacroName);
									MacroName=NotNowing->Next;
									(MacroName->Previous=NotNowing->Previous)->Next=MacroName;
									FreeTokenDirect(NotNowing);
									MacroName=OpenBracket->Next;
									FreeTokenDirect(OpenBracket);
									(MacroName->Previous=Dollar->Previous)->Next=MacroName;
									FreeTokenDirect(Dollar);
									return MacroName;
								}
								--BracketsAmount;
							}else if(NotNowing->Symbol.Type==SYMBOL_DOLLAR){
								NotNowing=HandleDollar(NotNowing,State,L);
								if(State->Error.Message){
									return Dollar;
								}
								continue;
							}
							NotNowing->Symbol.NotNowAmount=1;
						}
					}else if(!NotNowing->Type){
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow::");
						return Dollar;
					}
					NotNowing=NotNowing->Next;
				}
			}else if(AfterMacroName->Symbol.Type==SYMBOL_QUESTION){
				if(AfterMacroName->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected the question mark after $notnow to not be notnow");
					return Dollar;
				}
				Token*AfterQuestion=AfterMacroName->Next;
				for(;;){
					if(AfterQuestion->Type!=TOKEN_SYMBOL){
						STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after $notnow?");
						return Dollar;
					}
					if(AfterQuestion->Symbol.Type<=SYMBOL_OPEN_BRACE){
						break;
					}
					if(AfterQuestion->Symbol.Type==SYMBOL_LABEL){
						if(AfterQuestion->Symbol.NotNowAmount){
							STATIC_ERROR(&State->Error,"Expected the double colon after $notnow? to not be notnow");
							return Dollar;
						}
						Token*OpenBracket=AfterQuestion->Next;
						for(;;){
							if(OpenBracket->Type!=TOKEN_SYMBOL){
								STATIC_ERROR(&State->Error,"Expected an opening bracket after $notnow?::");
								return Dollar;
							}
							if(OpenBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
								break;
							}
							if(OpenBracket->Symbol.Type!=SYMBOL_DOLLAR||AfterQuestion->Symbol.NotNowAmount){
								STATIC_ERROR(&State->Error,"Expected an opening bracket after $notnow?::");
								return Dollar;
							}
							OpenBracket=HandleDollar(OpenBracket,State,L);
							if(State->Error.Message){
								return Dollar;
							}
						}
						if(OpenBracket->Symbol.NotNowAmount){
							STATIC_ERROR(&State->Error,"Expected the opening bracket after $notnow?:: to not be notnow");
							return Dollar;
						}
						Token*NotNowing=OpenBracket->Next;
						size_t BracketsAmount=0;
						for(;;){
							if(NotNowing->Type!=TOKEN_SYMBOL){
								if(!NotNowing->Type){
									STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow?::");
									return Dollar;
								}
								break;
							}
							if(NotNowing->Symbol.NotNowAmount){
								--NotNowing->Symbol.NotNowAmount;
								break;
							}
							if(NotNowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
								BracketsAmount=1;
								break;
							}
							if(NotNowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
								FreeTokenDirect(MacroName);
								FreeTokenDirect(AfterMacroName);
								FreeTokenDirect(AfterQuestion);
								FreeTokenDirect(OpenBracket);
								AfterMacroName=Dollar->Previous;
								(AfterMacroName->Next=MacroName=NotNowing->Next)->Previous=AfterMacroName;
								FreeTokenDirect(Dollar);
								FreeTokenDirect(NotNowing);
								return MacroName;
							}
							if(NotNowing->Symbol.Type!=SYMBOL_DOLLAR){
								break;
							}
							NotNowing=HandleDollar(NotNowing,State,L);
							if(State->Error.Message){
								return Dollar;
							}
						}
						Token*CloseBracket=NotNowing->Next;
						for(;;){
							if(CloseBracket->Type==TOKEN_SYMBOL){
								if(CloseBracket->Symbol.NotNowAmount){
									--CloseBracket->Symbol.NotNowAmount;
								}else if(CloseBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
									if(BracketsAmount==SIZE_MAX){
										STATIC_ERROR(&State->Error,"Bracket overflow after $notnow?::");
										return Dollar;
									}
									++BracketsAmount;
								}else if(CloseBracket->Symbol.Type>=SYMBOL_CLOSE_BRACE){
									if(!BracketsAmount){
										break;
									}
									--BracketsAmount;
								}else if(CloseBracket->Symbol.Type==SYMBOL_DOLLAR){
									CloseBracket=HandleDollar(CloseBracket,State,L);
									if(State->Error.Message){
										return Dollar;
									}
									continue;
								}
							}else if(!CloseBracket->Type){
								STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow?::");
								return Dollar;
							}
							CloseBracket=CloseBracket->Next;
						}
						Token*Last=CloseBracket->Previous;
						(OpenBracket->Next=CloseBracket)->Previous=OpenBracket;
						(NotNowing->Previous=State->End.Previous)->Next=NotNowing;
						(State->End.Previous=Last)->Next=&State->End;
						for(;;){
							if(NotNowing->Type!=TOKEN_SYMBOL||NotNowing->Symbol.NotNowAmount){
								break;
							}
							if(NotNowing->Symbol.Type!=SYMBOL_DOLLAR){
								NotNowing->Symbol.NotNowAmount=1;
								break;
							}
							NotNowing=HandleDollar(NotNowing,State,L);
							if(State->Error.Message){
								return Dollar;
							}
							if(!NotNowing->Type){
								FreeTokenDirect(MacroName);
								FreeTokenDirect(AfterMacroName);
								FreeTokenDirect(AfterQuestion);
								FreeTokenDirect(OpenBracket);
								AfterMacroName=Dollar->Previous;
								(AfterMacroName->Next=MacroName=CloseBracket->Next)->Previous=AfterMacroName;
								FreeTokenDirect(Dollar);
								FreeTokenDirect(CloseBracket);
								return MacroName;
							}
						}
						Last=NotNowing->Next;
						for(;;){
							if(Last->Type!=TOKEN_SYMBOL){
								if(!Last->Type){
									break;
								}
								Last=Last->Next;
							}else if(Last->Symbol.NotNowAmount){
								Last=Last->Next;
							}else if(Last->Symbol.Type!=SYMBOL_DOLLAR){
								Last->Symbol.NotNowAmount=1;
								Last=Last->Next;
							}else{
								Last=HandleDollar(Last,State,L);
								if(State->Error.Message){
									return Dollar;
								}
							}
						}
						FreeTokenDirect(MacroName);
						FreeTokenDirect(AfterMacroName);
						FreeTokenDirect(AfterQuestion);
						FreeTokenDirect(OpenBracket);
						MacroName=Dollar->Previous;
						AfterMacroName=CloseBracket->Next;
						FreeTokenDirect(Dollar);
						FreeTokenDirect(CloseBracket);
						Last=State->End.Previous;
						if(AfterMacroName==NotNowing){
							return(NotNowing->Previous=MacroName)->Next=NotNowing;
						}
						(State->End.Previous=NotNowing->Previous)->Next=&State->End;
						(MacroName->Next=NotNowing)->Previous=MacroName;
						(AfterMacroName->Previous=Last)->Next=AfterMacroName;
						return NotNowing;
					}
					if(AfterQuestion->Symbol.Type!=SYMBOL_DOLLAR||AfterQuestion->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected an opening bracket after or double colon $notnow?");
						return Dollar;
					}
					AfterQuestion=HandleDollar(AfterQuestion,State,L);
					if(State->Error.Message){
						return Dollar;
					}
				}
				if(AfterQuestion->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected the opening bracket after $notnow? to not be notnow");
					return Dollar;
				}
				Token*NotNowing=AfterQuestion->Next;
				size_t BracketsAmount=0;
				if(NotNowing->Type==TOKEN_SYMBOL){
					if(NotNowing->Symbol.NotNowAmount){
						--NotNowing->Symbol.NotNowAmount;
					}else if(NotNowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
						BracketsAmount=1;
					}else if(NotNowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
						FreeTokenDirect(MacroName);
						FreeTokenDirect(AfterMacroName);
						FreeTokenDirect(AfterQuestion);
						AfterMacroName=Dollar->Previous;
						(AfterMacroName->Next=MacroName=NotNowing->Next)->Previous=AfterMacroName;
						FreeTokenDirect(Dollar);
						FreeTokenDirect(NotNowing);
						return MacroName;
					}
				}else if(!NotNowing->Type){
					STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow?");
					return Dollar;
				}
				Token*CloseBracket=NotNowing->Next;
				for(;;){
					if(CloseBracket->Type==TOKEN_SYMBOL){
						if(CloseBracket->Symbol.NotNowAmount){
							--CloseBracket->Symbol.NotNowAmount;
						}else if(CloseBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
							if(BracketsAmount==SIZE_MAX){
								STATIC_ERROR(&State->Error,"Bracket overflow after $notnow?");
								return Dollar;
							}
							++BracketsAmount;
						}else if(CloseBracket->Symbol.Type>=SYMBOL_CLOSE_BRACE){
							if(!BracketsAmount){
								break;
							}
							--BracketsAmount;
						}
					}else if(!CloseBracket->Type){
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow?");
						return Dollar;
					}
					CloseBracket=CloseBracket->Next;
				}
				Token*Last=CloseBracket->Previous;
				(AfterQuestion->Next=CloseBracket)->Previous=AfterQuestion;
				(NotNowing->Previous=State->End.Previous)->Next=NotNowing;
				(State->End.Previous=Last)->Next=&State->End;
				for(;;){
					if(NotNowing->Type!=TOKEN_SYMBOL||NotNowing->Symbol.NotNowAmount){
						break;
					}
					if(NotNowing->Symbol.Type!=SYMBOL_DOLLAR){
						NotNowing->Symbol.NotNowAmount=1;
						break;
					}
					NotNowing=HandleDollar(NotNowing,State,L);
					if(State->Error.Message){
						return Dollar;
					}
					if(!NotNowing->Type){
						FreeTokenDirect(MacroName);
						FreeTokenDirect(AfterMacroName);
						FreeTokenDirect(AfterQuestion);
						AfterMacroName=Dollar->Previous;
						(AfterMacroName->Next=MacroName=CloseBracket->Next)->Previous=AfterMacroName;
						FreeTokenDirect(Dollar);
						FreeTokenDirect(CloseBracket);
						return MacroName;
					}
				}
				Last=NotNowing->Next;
				for(;;){
					if(Last->Type!=TOKEN_SYMBOL){
						if(!Last->Type){
							break;
						}
						Last=Last->Next;
					}else if(Last->Symbol.NotNowAmount){
						Last=Last->Next;
					}else if(Last->Symbol.Type!=SYMBOL_DOLLAR){
						Last->Symbol.NotNowAmount=1;
						Last=Last->Next;
					}else{
						Last=HandleDollar(Last,State,L);
						if(State->Error.Message){
							return Dollar;
						}
					}
				}
				FreeTokenDirect(MacroName);
				FreeTokenDirect(AfterMacroName);
				FreeTokenDirect(AfterQuestion);
				MacroName=Dollar->Previous;
				AfterMacroName=CloseBracket->Next;
				FreeTokenDirect(Dollar);
				FreeTokenDirect(CloseBracket);
				Last=State->End.Previous;
				if(AfterMacroName==NotNowing){
					return(NotNowing->Previous=MacroName)->Next=NotNowing;
				}
				(State->End.Previous=NotNowing->Previous)->Next=&State->End;
				(MacroName->Next=NotNowing)->Previous=MacroName;
				(AfterMacroName->Previous=Last)->Next=AfterMacroName;
				return NotNowing;
			}else if(AfterMacroName->Symbol.Type!=SYMBOL_DOLLAR||AfterMacroName->Symbol.NotNowAmount){
				STATIC_ERROR(&State->Error,"Expected a number, semicolon, colon, double colon, question mark, or opening bracket after $notnow");
				return Dollar;
			}
		}else if(AfterMacroName->Type==TOKEN_INTEGER){
			NotNowInteger:;
			if(AfterMacroName->Integer<0){
				STATIC_ERROR(&State->Error,"The number provided to $notnow N must be nonnegative");
				return Dollar;
			}
			for(Token*AfterInteger=AfterMacroName->Next;;){
				if(AfterInteger->Type!=TOKEN_SYMBOL){
					STATIC_ERROR(&State->Error,"Expected a semicolon, colon, double colon, question mark, or opening bracket after $notnow N");
					return Dollar;
				}
				if(AfterInteger->Symbol.Type==SYMBOL_SEMICOLON){
					if(AfterInteger->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected the semicolon after $notnow N to not be notnow");
						return Dollar;
					}
					((Dollar->Next=AfterInteger->Next)->Previous=Dollar)->Symbol.NotNowAmount=AfterMacroName->Integer;
					FreeTokenDirect(MacroName);
					FreeTokenDirect(AfterMacroName);
					FreeTokenDirect(AfterInteger);
					return Dollar;
				}else if(AfterInteger->Symbol.Type==SYMBOL_COLON){
					if(AfterInteger->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected the colon after $notnow N to not be notnow");
						return Dollar;
					}
					Token*NotNowing=AfterInteger->Next;
					for(;;){
						if(NotNowing->Type!=TOKEN_SYMBOL){
							STATIC_ERROR(&State->Error,"Expected a symbol after $notnow N:");
							return Dollar;
						}
						if(NotNowing->Symbol.NotNowAmount){
							if(NotNowing->Symbol.NotNowAmount-1>LUA_MAXINTEGER-AfterMacroName->Integer){
								STATIC_ERROR(&State->Error,"Overflow in notnows after $notnow N:");
								return Dollar;
							}
							NotNowing->Symbol.NotNowAmount+=AfterMacroName->Integer-1;
							break;
						}
						if(NotNowing->Symbol.Type!=SYMBOL_DOLLAR){
							NotNowing->Symbol.NotNowAmount+=AfterMacroName->Integer;
							break;
						}
						NotNowing=HandleDollar(NotNowing,State,L);
						if(State->Error.Message){
							return Dollar;
						}
					}
					(NotNowing->Previous=Dollar->Previous)->Next=NotNowing;
					FreeTokenDirect(Dollar);
					FreeTokenDirect(MacroName);
					FreeTokenDirect(AfterMacroName);
					FreeTokenDirect(AfterInteger);
					return NotNowing;
				}else if(AfterInteger->Symbol.Type<=SYMBOL_OPEN_BRACE){
					if(AfterInteger->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected the opening bracket after $notnow N to not be notnow");
						return Dollar;
					}
					Token*NotNowing=AfterInteger->Next;
					for(size_t BracketsAmount=0;;){
						if(NotNowing->Type==TOKEN_SYMBOL){
							if(!NotNowing->Symbol.NotNowAmount){
								if(NotNowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
									if(BracketsAmount==SIZE_MAX){
										STATIC_ERROR(&State->Error,"Bracket overflow after $notnow N");
										return Dollar;
									}
									++BracketsAmount;
								}else if(NotNowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
									if(!BracketsAmount){
										FreeTokenDirect(MacroName);
										FreeTokenDirect(AfterMacroName);
										MacroName=NotNowing->Next;
										(MacroName->Previous=NotNowing->Previous)->Next=MacroName;
										FreeTokenDirect(NotNowing);
										MacroName=AfterInteger->Next;
										FreeTokenDirect(AfterInteger);
										(MacroName->Previous=Dollar->Previous)->Next=MacroName;
										FreeTokenDirect(Dollar);
										return MacroName;
									}
									--BracketsAmount;
								}
								NotNowing->Symbol.NotNowAmount=AfterMacroName->Integer;
							}else if(NotNowing->Symbol.NotNowAmount-1>LUA_MAXINTEGER-AfterMacroName->Integer){
								STATIC_ERROR(&State->Error,"Overflow in notnows after $notnow N");
								return Dollar;
							}else{
								NotNowing->Symbol.NotNowAmount+=AfterMacroName->Integer-1;
							}
						}else if(!NotNowing->Type){
							STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow N");
							return Dollar;
						}
						NotNowing=NotNowing->Next;
					}
				}else if(AfterInteger->Symbol.Type==SYMBOL_LABEL){
					if(AfterInteger->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected the double colon after $notnow N to not be notnow");
						return Dollar;
					}
					Token*OpenBracket=AfterInteger->Next;
					for(;;){
						if(OpenBracket->Type!=TOKEN_SYMBOL){
							STATIC_ERROR(&State->Error,"Expected an opening bracket after $notnow N::");
							return Dollar;
						}
						if(OpenBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
							break;
						}
						if(OpenBracket->Symbol.Type!=SYMBOL_DOLLAR||OpenBracket->Symbol.NotNowAmount){
							STATIC_ERROR(&State->Error,"Expected an opening bracket after $notnow N::");
							return Dollar;
						}
						OpenBracket=HandleDollar(OpenBracket,State,L);
						if(State->Error.Message){
							return Dollar;
						}
					}
					if(OpenBracket->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected the opening bracket after $notnow N:: to not be notnow");
						return Dollar;
					}
					Token*NotNowing=OpenBracket->Next;
					for(size_t BracketsAmount=0;;){
						if(NotNowing->Type==TOKEN_SYMBOL){
							if(!NotNowing->Symbol.NotNowAmount){
								if(NotNowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
									if(BracketsAmount==SIZE_MAX){
										STATIC_ERROR(&State->Error,"Bracket overflow after $notnow N::");
										return Dollar;
									}
									++BracketsAmount;
								}else if(NotNowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
									if(!BracketsAmount){
										FreeTokenDirect(MacroName);
										FreeTokenDirect(AfterMacroName);
										FreeTokenDirect(AfterInteger);
										MacroName=NotNowing->Next;
										(MacroName->Previous=NotNowing->Previous)->Next=MacroName;
										FreeTokenDirect(NotNowing);
										MacroName=OpenBracket->Next;
										FreeTokenDirect(OpenBracket);
										(MacroName->Previous=Dollar->Previous)->Next=MacroName;
										FreeTokenDirect(Dollar);
										return MacroName;
									}
									--BracketsAmount;
								}else if(NotNowing->Symbol.Type==SYMBOL_DOLLAR){
									NotNowing=HandleDollar(NotNowing,State,L);
									if(State->Error.Message){
										return Dollar;
									}
									continue;
								}
								NotNowing->Symbol.NotNowAmount=AfterMacroName->Integer;
							}else if(NotNowing->Symbol.NotNowAmount-1>LUA_MAXINTEGER-AfterMacroName->Integer){
								STATIC_ERROR(&State->Error,"Overflow in notnows after $notnow N::");
								return Dollar;
							}else{
								NotNowing->Symbol.NotNowAmount+=AfterMacroName->Integer-1;
							}
						}else if(!NotNowing->Type){
							STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow N::");
							return Dollar;
						}
						NotNowing=NotNowing->Next;
					}
				}else if(AfterInteger->Symbol.Type==SYMBOL_QUESTION){
					if(AfterInteger->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected the question mark after $notnow N to not be notnow");
						return Dollar;
					}
					Token*AfterQuestion=AfterInteger->Next;
					for(;;){
						if(AfterQuestion->Type!=TOKEN_SYMBOL){
							STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after $notnow N?");
							return Dollar;
						}
						if(AfterQuestion->Symbol.Type<=SYMBOL_OPEN_BRACE){
							break;
						}
						if(AfterQuestion->Symbol.Type==SYMBOL_LABEL){
							if(AfterQuestion->Symbol.NotNowAmount){
								STATIC_ERROR(&State->Error,"Expected the double colon after $notnow N? to not be notnow");
								return Dollar;
							}
							Token*OpenBracket=AfterQuestion->Next;
							for(;;){
								if(OpenBracket->Type!=TOKEN_SYMBOL){
									STATIC_ERROR(&State->Error,"Expected an opening bracket after $notnow N?::");
									return Dollar;
								}
								if(OpenBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
									break;
								}
								if(OpenBracket->Symbol.Type!=SYMBOL_DOLLAR||AfterQuestion->Symbol.NotNowAmount){
									STATIC_ERROR(&State->Error,"Expected an opening bracket after $notnow N?::");
									return Dollar;
								}
								OpenBracket=HandleDollar(OpenBracket,State,L);
								if(State->Error.Message){
									return Dollar;
								}
							}
							if(OpenBracket->Symbol.NotNowAmount){
								STATIC_ERROR(&State->Error,"Expected the opening bracket after $notnow N?:: to not be notnow");
								return Dollar;
							}
							Token*NotNowing=OpenBracket->Next;
							size_t BracketsAmount=0;
							for(;;){
								if(NotNowing->Type!=TOKEN_SYMBOL){
									if(!NotNowing->Type){
										STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow N?::");
										return Dollar;
									}
									break;
								}
								if(NotNowing->Symbol.NotNowAmount){
									--NotNowing->Symbol.NotNowAmount;
									break;
								}
								if(NotNowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
									BracketsAmount=1;
									break;
								}
								if(NotNowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
									FreeTokenDirect(MacroName);
									FreeTokenDirect(AfterMacroName);
									FreeTokenDirect(AfterInteger);
									FreeTokenDirect(AfterQuestion);
									FreeTokenDirect(OpenBracket);
									AfterMacroName=Dollar->Previous;
									(AfterMacroName->Next=MacroName=NotNowing->Next)->Previous=AfterMacroName;
									FreeTokenDirect(Dollar);
									FreeTokenDirect(NotNowing);
									return MacroName;
								}
								if(NotNowing->Symbol.Type!=SYMBOL_DOLLAR){
									break;
								}
								NotNowing=HandleDollar(NotNowing,State,L);
								if(State->Error.Message){
									return Dollar;
								}
							}
							Token*CloseBracket=NotNowing->Next;
							for(;;){
								if(CloseBracket->Type==TOKEN_SYMBOL){
									if(CloseBracket->Symbol.NotNowAmount){
										--CloseBracket->Symbol.NotNowAmount;
									}else if(CloseBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
										if(BracketsAmount==SIZE_MAX){
											STATIC_ERROR(&State->Error,"Bracket overflow after $notnow N?::");
											return Dollar;
										}
										++BracketsAmount;
									}else if(CloseBracket->Symbol.Type>=SYMBOL_CLOSE_BRACE){
										if(!BracketsAmount){
											break;
										}
										--BracketsAmount;
									}else if(CloseBracket->Symbol.Type==SYMBOL_DOLLAR){
										CloseBracket=HandleDollar(CloseBracket,State,L);
										if(State->Error.Message){
											return Dollar;
										}
										continue;
									}
								}else if(!CloseBracket->Type){
									STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow N?::");
									return Dollar;
								}
								CloseBracket=CloseBracket->Next;
							}
							Token*Last=CloseBracket->Previous;
							(OpenBracket->Next=CloseBracket)->Previous=OpenBracket;
							(NotNowing->Previous=State->End.Previous)->Next=NotNowing;
							(State->End.Previous=Last)->Next=&State->End;
							for(;;){
								if(NotNowing->Type!=TOKEN_SYMBOL){
									break;
								}
								if(NotNowing->Symbol.NotNowAmount){
									if(NotNowing->Symbol.NotNowAmount-1>LUA_MAXINTEGER-AfterMacroName->Integer){
										STATIC_ERROR(&State->Error,"Overflow in notnows after $notnow N?");
										return Dollar;
									}
									NotNowing->Symbol.NotNowAmount+=AfterMacroName->Integer-1;
									break;
								}
								if(NotNowing->Symbol.Type!=SYMBOL_DOLLAR){
									NotNowing->Symbol.NotNowAmount=AfterMacroName->Integer;
									break;
								}
								NotNowing=HandleDollar(NotNowing,State,L);
								if(State->Error.Message){
									return Dollar;
								}
								if(!NotNowing->Type){
									FreeTokenDirect(MacroName);
									FreeTokenDirect(AfterMacroName);
									FreeTokenDirect(AfterInteger);
									FreeTokenDirect(AfterQuestion);
									FreeTokenDirect(OpenBracket);
									AfterMacroName=Dollar->Previous;
									(AfterMacroName->Next=MacroName=CloseBracket->Next)->Previous=AfterMacroName;
									FreeTokenDirect(Dollar);
									FreeTokenDirect(CloseBracket);
									return MacroName;
								}
							}
							Last=NotNowing->Next;
							for(;;){
								if(Last->Type!=TOKEN_SYMBOL){
									if(!Last->Type){
										break;
									}
									Last=Last->Next;
								}else if(Last->Symbol.NotNowAmount){
									if(Last->Symbol.NotNowAmount-1>LUA_MAXINTEGER-AfterMacroName->Integer){
										STATIC_ERROR(&State->Error,"Overflow in notnows after $notnow N?::");
										return Dollar;
									}
									Last->Symbol.NotNowAmount+=AfterMacroName->Integer-1;
									Last=Last->Next;
								}else if(Last->Symbol.Type!=SYMBOL_DOLLAR){
									Last->Symbol.NotNowAmount=AfterMacroName->Integer;
									Last=Last->Next;
								}else{
									Last=HandleDollar(Last,State,L);
									if(State->Error.Message){
										return Dollar;
									}
								}
							}
							FreeTokenDirect(MacroName);
							FreeTokenDirect(AfterMacroName);
							FreeTokenDirect(AfterInteger);
							FreeTokenDirect(AfterQuestion);
							FreeTokenDirect(OpenBracket);
							MacroName=Dollar->Previous;
							AfterMacroName=CloseBracket->Next;
							FreeTokenDirect(Dollar);
							FreeTokenDirect(CloseBracket);
							Last=State->End.Previous;
							if(AfterMacroName==NotNowing){
								return(NotNowing->Previous=MacroName)->Next=NotNowing;
							}
							(State->End.Previous=NotNowing->Previous)->Next=&State->End;
							(MacroName->Next=NotNowing)->Previous=MacroName;
							(AfterMacroName->Previous=Last)->Next=AfterMacroName;
							return NotNowing;
						}
						if(AfterQuestion->Symbol.Type!=SYMBOL_DOLLAR||AfterQuestion->Symbol.NotNowAmount){
							STATIC_ERROR(&State->Error,"Expected an opening bracket after or double colon $notnow N?");
							return Dollar;
						}
						AfterQuestion=HandleDollar(AfterQuestion,State,L);
						if(State->Error.Message){
							return Dollar;
						}
					}
					if(AfterQuestion->Symbol.NotNowAmount){
						STATIC_ERROR(&State->Error,"Expected the opening bracket after $notnow N? to not be notnow");
						return Dollar;
					}
					Token*NotNowing=AfterQuestion->Next;
					size_t BracketsAmount=0;
					if(NotNowing->Type==TOKEN_SYMBOL){
						if(NotNowing->Symbol.NotNowAmount){
							--NotNowing->Symbol.NotNowAmount;
						}else if(NotNowing->Symbol.Type<=SYMBOL_OPEN_BRACE){
							BracketsAmount=1;
						}else if(NotNowing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
							FreeTokenDirect(MacroName);
							FreeTokenDirect(AfterMacroName);
							FreeTokenDirect(AfterInteger);
							FreeTokenDirect(AfterQuestion);
							AfterMacroName=Dollar->Previous;
							(AfterMacroName->Next=MacroName=NotNowing->Next)->Previous=AfterMacroName;
							FreeTokenDirect(Dollar);
							FreeTokenDirect(NotNowing);
							return MacroName;
						}
					}else if(!NotNowing->Type){
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow N?");
						return Dollar;
					}
					Token*CloseBracket=NotNowing->Next;
					for(;;){
						if(CloseBracket->Type==TOKEN_SYMBOL){
							if(CloseBracket->Symbol.NotNowAmount){
								--CloseBracket->Symbol.NotNowAmount;
							}else if(CloseBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
								if(BracketsAmount==SIZE_MAX){
									STATIC_ERROR(&State->Error,"Bracket overflow after $notnow N?");
									return Dollar;
								}
								++BracketsAmount;
							}else if(CloseBracket->Symbol.Type>=SYMBOL_CLOSE_BRACE){
								if(!BracketsAmount){
									break;
								}
								--BracketsAmount;
							}
						}else if(!CloseBracket->Type){
							STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $notnow N?");
							return Dollar;
						}
						CloseBracket=CloseBracket->Next;
					}
					Token*Last=CloseBracket->Previous;
					(AfterQuestion->Next=CloseBracket)->Previous=AfterQuestion;
					(NotNowing->Previous=State->End.Previous)->Next=NotNowing;
					(State->End.Previous=Last)->Next=&State->End;
					for(;;){
						if(NotNowing->Type!=TOKEN_SYMBOL){
							break;
						}
						if(NotNowing->Symbol.NotNowAmount){
							if(NotNowing->Symbol.NotNowAmount-1>LUA_MAXINTEGER-AfterMacroName->Integer){
								STATIC_ERROR(&State->Error,"Overflow in notnows after $notnow N?");
								return Dollar;
							}
							NotNowing->Symbol.NotNowAmount+=AfterMacroName->Integer-1;
							break;
						}
						if(NotNowing->Symbol.Type!=SYMBOL_DOLLAR){
							NotNowing->Symbol.NotNowAmount=AfterMacroName->Integer;
							break;
						}
						NotNowing=HandleDollar(NotNowing,State,L);
						if(State->Error.Message){
							return Dollar;
						}
						if(!NotNowing->Type){
							FreeTokenDirect(MacroName);
							FreeTokenDirect(AfterMacroName);
							FreeTokenDirect(AfterInteger);
							FreeTokenDirect(AfterQuestion);
							AfterMacroName=Dollar->Previous;
							(AfterMacroName->Next=MacroName=CloseBracket->Next)->Previous=AfterMacroName;
							FreeTokenDirect(Dollar);
							FreeTokenDirect(CloseBracket);
							return MacroName;
						}
					}
					Last=NotNowing->Next;
					for(;;){
						if(Last->Type!=TOKEN_SYMBOL){
							if(!Last->Type){
								break;
							}
							Last=Last->Next;
						}else if(Last->Symbol.NotNowAmount){
							if(Last->Symbol.NotNowAmount-1>LUA_MAXINTEGER-AfterMacroName->Integer){
								STATIC_ERROR(&State->Error,"Overflow in notnows after $notnow N?");
								return Dollar;
							}
							Last->Symbol.NotNowAmount+=AfterMacroName->Integer-1;
							Last=Last->Next;
						}else if(Last->Symbol.Type!=SYMBOL_DOLLAR){
							Last->Symbol.NotNowAmount=AfterMacroName->Integer;
							Last=Last->Next;
						}else{
							Last=HandleDollar(Last,State,L);
							if(State->Error.Message){
								return Dollar;
							}
						}
					}
					FreeTokenDirect(MacroName);
					FreeTokenDirect(AfterMacroName);
					FreeTokenDirect(AfterInteger);
					FreeTokenDirect(AfterQuestion);
					MacroName=Dollar->Previous;
					AfterMacroName=CloseBracket->Next;
					FreeTokenDirect(Dollar);
					FreeTokenDirect(CloseBracket);
					Last=State->End.Previous;
					if(AfterMacroName==NotNowing){
						return(NotNowing->Previous=MacroName)->Next=NotNowing;
					}
					(State->End.Previous=NotNowing->Previous)->Next=&State->End;
					(MacroName->Next=NotNowing)->Previous=MacroName;
					(AfterMacroName->Previous=Last)->Next=AfterMacroName;
					return NotNowing;
				}else if(AfterInteger->Symbol.Type!=SYMBOL_DOLLAR||AfterInteger->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected a semicolon, colon, double colon, question mark, or opening bracket after $notnow N");
					return Dollar;
				}
				AfterInteger=HandleDollar(AfterInteger,State,L);
				if(State->Error.Message){
					return Dollar;
				}
			}
		}else if(AfterMacroName->Type==TOKEN_FLOAT){/* allow floats that are exactly representable as integers similar to the functions in the Lua standard library */
			if(!FloatFitsInInteger(AfterMacroName->Float)){
				STATIC_ERROR(&State->Error,"Number provided to $notnow N doesn't fit in an int");
				return Dollar;
			}
			AfterMacroName->Type=TOKEN_INTEGER;
			AfterMacroName->Integer=AfterMacroName->Float;
			goto NotNowInteger;
		}else{
			STATIC_ERROR(&State->Error,"Expected a number, semicolon, colon, double colon, question mark, or opening bracket after $notnow");
			return Dollar;
		}
		AfterMacroName=HandleDollar(AfterMacroName,State,L);
		if(State->Error.Message){
			return Dollar;
		}
	}
}
static Token*PredefinedToTokens(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	Token*AfterMacroName=MacroName->Next;
	const char*Buffer;
	size_t Length;
	for(;;){
		if(AfterMacroName->Type==TOKEN_SHORT_STRING){
			Buffer=AfterMacroName->Short.Buffer;
			Length=AfterMacroName->Short.Length;
			break;
		}
		if(AfterMacroName->Type==TOKEN_LONG_STRING){
			Buffer=AfterMacroName->Long.Buffer;
			Length=AfterMacroName->Long.Length;
			break;
		}
		if(AfterMacroName->Type!=TOKEN_SYMBOL||AfterMacroName->Symbol.Type!=SYMBOL_DOLLAR||AfterMacroName->Symbol.NotNowAmount){
			STATIC_ERROR(&State->Error,"Expected a string after $totokens");
			return Dollar;
		}
		AfterMacroName=HandleDollar(AfterMacroName,State,L);
		if(State->Error.Message){
			return Dollar;
		}
	}
	TokenList Output=MakeTokenList(Buffer,Length,&State->Error);
	if(State->Error.Message){
		return Dollar;
	}
	if(Output.First){
		(Output.First->Previous=Dollar->Previous)->Next=Output.First;
		FreeTokenDirect(Dollar);
		(Output.Last->Next=AfterMacroName->Next)->Previous=Output.Last;
		FreeToken(AfterMacroName);
		FreeTokenDirect(MacroName);
		return Output.First;
	}else{
		FreeTokenDirect(MacroName);
		MacroName=AfterMacroName->Next;
		(MacroName->Previous=Dollar->Previous)->Next=MacroName;
		FreeTokenDirect(Dollar);
		FreeToken(AfterMacroName);
		return MacroName;
	}
}
static Token*PredefinedToString(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	Token*OpenBracket=MacroName->Next;
	for(;;){
		if(OpenBracket->Type!=TOKEN_SYMBOL){
			STATIC_ERROR(&State->Error,"Expected an opening bracket after $tostring");
			return Dollar;
		}
		if(OpenBracket->Symbol.Type<=SYMBOL_OPEN_BRACE){
			if(OpenBracket->Symbol.NotNowAmount){
				STATIC_ERROR(&State->Error,"Expected the opening bracket after $tostring to not be notnow");
				return Dollar;
			}
			break;
		}
		if(OpenBracket->Symbol.Type!=SYMBOL_DOLLAR||OpenBracket->Symbol.NotNowAmount){
			STATIC_ERROR(&State->Error,"Expected an opening bracket after $tostring");
			return Dollar;
		}
		OpenBracket=HandleDollar(OpenBracket,State,L);
		if(State->Error.Message){
			return Dollar;
		}
	}
	Dollar->Type=TOKEN_SHORT_STRING;
	Dollar->Short.Length=0;
	Token*Previous=OpenBracket;
	Token*Converting=OpenBracket->Next;
	size_t BracketsAmount=0;
	#define HANDLE_STRING_CONVERSION(Name)\
		size_t Length=Converting->Name.Length+2;\
		for(size_t Index=0;Index!=Converting->Name.Length;++Index){\
			switch(Converting->Name.Buffer[Index]){\
				case'\a':case'\b':case'\f':case'\n':case'\r':case'\t':case'\v':case'\"':case'\\':{\
					if(Length==LSIZE_MAX){\
						STATIC_ERROR(&State->Error,"Error while allocating result of $tostring");\
						return Dollar;\
					}\
					++Length;\
					break;\
				}\
				default:{\
					if(IsControl(Converting->Name.Buffer[Index])&&(unsigned char)Converting->Name.Buffer[Index]<=0XFF){\
						if(Length>LSIZE_MAX-3){\
							STATIC_ERROR(&State->Error,"Error while allocating result of $tostring");\
							return Dollar;\
						}\
						Length+=3;\
					}\
				}\
			}\
		}\
		CHECK_LENGTH(Length);\
		ADD_CHARACTER('"');\
		for(size_t Index=0;Index!=Converting->Name.Length;++Index){\
			switch(Converting->Name.Buffer[Index]){\
				case'\a':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('a');\
					break;\
				}\
				case'\b':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('b');\
					break;\
				}\
				case'\f':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('f');\
					break;\
				}\
				case'\n':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('n');\
					break;\
				}\
				case'\r':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('r');\
					break;\
				}\
				case'\t':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('t');\
					break;\
				}\
				case'\v':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('v');\
					break;\
				}\
				case'\"':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('"');\
					break;\
				}\
				case'\\':{\
					ADD_CHARACTER('\\');\
					ADD_CHARACTER('\\');\
					break;\
				}\
				default:{\
					if(IsControl(Converting->Name.Buffer[Index])&&(unsigned char)Converting->Name.Buffer[Index]<=0XFF){\
						ADD_CHARACTER('\\');\
						ADD_CHARACTER('x');\
						ADD_CHARACTER(HexadecimalDigitToCharacter((unsigned)(unsigned char)Converting->Name.Buffer[Index]>>4));\
						ADD_CHARACTER(HexadecimalDigitToCharacter((unsigned char)Converting->Name.Buffer[Index]&0XFU));\
					}else{\
						ADD_CHARACTER(Converting->Name.Buffer[Index]);\
					}\
				}\
			}\
		}\
		ADD_CHARACTER('"');
	#define HANDLE_CONVERSION\
		switch(Converting->Type){\
			case TOKEN_SHORT_NAME:{\
				bool NeedsSpace=Previous->Type==TOKEN_SHORT_NAME||Previous->Type==TOKEN_LONG_NAME||Previous->Type==TOKEN_INTEGER||Previous->Type==TOKEN_FLOAT;\
				CHECK_LENGTH((size_t)Converting->Short.Length+NeedsSpace);\
				if(NeedsSpace){\
					ADD_CHARACTER(' ');\
				}\
				ADD_CHARACTERS(Converting->Short.Buffer,Converting->Short.Length);\
				break;\
			}\
			case TOKEN_LONG_NAME:{\
				bool NeedsSpace=Previous->Type==TOKEN_SHORT_NAME||Previous->Type==TOKEN_LONG_NAME||Previous->Type==TOKEN_INTEGER||Previous->Type==TOKEN_FLOAT;\
				if(NeedsSpace&&Converting->Long.Length==LSIZE_MAX){\
					STATIC_ERROR(&State->Error,"Error while allocating result of $tostring");\
					return Dollar;\
				}\
				CHECK_LENGTH(Converting->Long.Length+NeedsSpace);\
				if(NeedsSpace){\
					ADD_CHARACTER(' ');\
				}\
				ADD_CHARACTERS(Converting->Long.Buffer,Converting->Long.Length);\
				break;\
			}\
			case TOKEN_SHORT_STRING:{\
				HANDLE_STRING_CONVERSION(Short);\
				break;\
			}\
			case TOKEN_LONG_STRING:{\
				if(Converting->Long.Length>LSIZE_MAX-2){\
					STATIC_ERROR(&State->Error,"Error while the allocating result of $tostring");\
					return Dollar;\
				}\
				HANDLE_STRING_CONVERSION(Long);\
				break;\
			}\
			case TOKEN_INVALID:{\
				STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets after $tostring");\
				return Dollar;\
			}\
			case TOKEN_INTEGER:{\
				char String[INTEGER_BUFFER_SIZE];\
				int Length=Csnprintf(String,sizeof(String)," %#"LUA_INTEGER_FRMLEN"X"+(Previous->Type!=TOKEN_SHORT_NAME&&Previous->Type!=TOKEN_LONG_NAME&&Previous->Type!=TOKEN_INTEGER&&Previous->Type!=TOKEN_FLOAT&&(Previous->Type!=TOKEN_SYMBOL||Previous->Symbol.Type!=SYMBOL_DOT)),(lua_Unsigned)Converting->Integer);\
				if(Length<0||Length>=sizeof(String)){\
					STATIC_ERROR(&State->Error,"Error while formatting number in $tostring");\
					return Dollar;\
				}\
				CHECK_LENGTH(Length);\
				ADD_CHARACTERS(String,Length);\
				break;\
			}\
			case TOKEN_FLOAT:{\
				bool SkipSpace=Previous->Type!=TOKEN_SHORT_NAME&&Previous->Type!=TOKEN_LONG_NAME&&Previous->Type!=TOKEN_INTEGER&&Previous->Type!=TOKEN_FLOAT&&(Previous->Type!=TOKEN_SYMBOL||Previous->Symbol.Type!=SYMBOL_DOT);\
				char String[FLOAT_BUFFER_SIZE];\
				int Length=isfinite(Converting->Float)?Csnprintf(String,sizeof(String)," %"LUA_NUMBER_FRMLEN"A"+SkipSpace,Converting->Float):Csnprintf(String,sizeof(String)," 0X%jXP+%ju"+SkipSpace,(uintmax_t)FLT_RADIX,(uintmax_t)l_floatatt(MAX_EXP));\
				if(Length<0||Length>=sizeof(String)){\
					STATIC_ERROR(&State->Error,"Error while formatting number in $tostring");\
					return Dollar;\
				}\
				CHECK_LENGTH(Length);\
				ADD_CHARACTERS(String,Length);\
				break;\
			}\
			case TOKEN_SYMBOL:{\
				bool NeedsSpace;\
				switch(Converting->Symbol.Type){\
					case SYMBOL_OPEN_BRACKET:{\
						NeedsSpace=Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_OPEN_BRACKET;\
						break;\
					}\
					case SYMBOL_SUBTRACT:{\
						NeedsSpace=Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_SUBTRACT;\
						break;\
					}\
					case SYMBOL_DIVIDE:case SYMBOL_FLOOR_DIVIDE:{\
						NeedsSpace=Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_DIVIDE;\
						break;\
					}\
					case SYMBOL_LESSER:case SYMBOL_LESSER_EQUAL:case SYMBOL_LEFT_SHIFT:{\
						NeedsSpace=Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_LESSER;\
						break;\
					}\
					case SYMBOL_GREATER:case SYMBOL_GREATER_EQUAL:case SYMBOL_RIGHT_SHIFT:{\
						NeedsSpace=Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_GREATER;\
						break;\
					}\
					case SYMBOL_ASSIGN:case SYMBOL_EQUALS:{\
						NeedsSpace=Previous->Type==TOKEN_SYMBOL&&(Previous->Symbol.Type==SYMBOL_ASSIGN||Previous->Symbol.Type==SYMBOL_EXCLUSIVE_OR||Previous->Symbol.Type==SYMBOL_LESSER||Previous->Symbol.Type==SYMBOL_GREATER);\
						break;\
					}\
					case SYMBOL_COLON:case SYMBOL_LABEL:{\
						NeedsSpace=Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_COLON;\
						break;\
					}\
					case SYMBOL_DOT:case SYMBOL_CONCATENATE:case SYMBOL_VARIABLE_ARGUMENTS:{\
						NeedsSpace=Previous->Type==TOKEN_INTEGER||Previous->Type==TOKEN_FLOAT||Previous->Type==TOKEN_SYMBOL&&(Previous->Symbol.Type==SYMBOL_DOT||Previous->Symbol.Type==SYMBOL_CONCATENATE);\
						break;\
					}\
					default:{\
						NeedsSpace=0;\
					}\
				}\
				if(!Converting->Symbol.NotNowAmount){\
					if(Converting->Symbol.Type<=SYMBOL_OPEN_BRACE){\
						if(BracketsAmount==SIZE_MAX){\
							STATIC_ERROR(&State->Error,"$tostring bracket overflow");\
							return Dollar;\
						}\
						++BracketsAmount;\
					}else if(Converting->Symbol.Type>=SYMBOL_CLOSE_BRACE){\
						if(!BracketsAmount){\
							(Dollar->Next=Converting->Next)->Previous=Dollar;\
							FreeTokenDirect(MacroName);\
							MacroName=OpenBracket->Next;\
							FreeTokenDirect(OpenBracket);\
							while(MacroName!=Converting){\
								OpenBracket=MacroName->Next;\
								FreeToken(MacroName);\
								if(OpenBracket==Converting){\
									break;\
								}\
								MacroName=OpenBracket->Next;\
								FreeToken(OpenBracket);\
							}\
							FreeTokenDirect(Converting);\
							return Dollar;\
						}\
						--BracketsAmount;\
					}else if(Converting->Symbol.Type==SYMBOL_DOLLAR){\
						Converting=HandleDollar(Converting,State,L);\
						if(State->Error.Message){\
							return Dollar;\
						}\
						continue;\
					}\
				}else if(Converting->Symbol.NotNowAmount!=1){/* could add backslashes for the notnows, but that isn't valid in Lua */\
					STATIC_ERROR(&State->Error,"$tostring does not accept multiple notnows");\
					return Dollar;\
				}\
				CHECK_LENGTH(SymbolTokenLengths[Converting->Symbol.Type]+NeedsSpace);\
				if(NeedsSpace){\
					ADD_CHARACTER(' ');\
				}\
				ADD_CHARACTERS(SymbolTokens[Converting->Symbol.Type],SymbolTokenLengths[Converting->Symbol.Type]);\
			}\
		}
	for(;;){
		#define CHECK_LENGTH(...)\
			if((__VA_ARGS__)>sizeof(Dollar->Short.Buffer)-Dollar->Short.Length){\
				goto Long;\
			}
		#define ADD_CHARACTER(...)\
			Dollar->Short.Buffer[Dollar->Short.Length++]=__VA_ARGS__
		#define ADD_CHARACTERS(Buffer_,Length_)\
			size_t BufferLength=Length_;\
			memcpy(Dollar->Short.Buffer+Dollar->Short.Length,Buffer_,BufferLength);\
			Dollar->Short.Length+=BufferLength
		HANDLE_CONVERSION;
		#undef CHECK_LENGTH
		#undef ADD_CHARACTER
		#undef ADD_CHARACTERS
		Previous=Converting;
		Converting=Converting->Next;
	}
	Long:;
	size_t Capacity=sizeof(Dollar->Short.Buffer)>LSIZE_MAX>>1?LSIZE_MAX:sizeof(Dollar->Short.Buffer)<<1;
	char*Buffer=malloc(Capacity);
	if(!Buffer){
		STATIC_ERROR(&State->Error,"Error allocating string in $tostring");
		return Dollar;
	}
	memcpy(Buffer,Dollar->Short.Buffer,Dollar->Short.Length);
	Dollar->Long.Length=Dollar->Short.Length;
	Dollar->Long.Buffer=Buffer;
	Dollar->Long.Capacity=Capacity;
	Dollar->Type=TOKEN_LONG_STRING;
	for(;;){
		#define CHECK_LENGTH(...)\
			size_t AddingLength=__VA_ARGS__;\
			if(Dollar->Long.Capacity-Dollar->Long.Length<AddingLength){\
				if(Dollar->Long.Length>LSIZE_MAX-AddingLength){\
					STATIC_ERROR(&State->Error,"Error while allocating the result of $tostring");\
					return Dollar;\
				}\
				char*NewBuffer=realloc(Dollar->Long.Buffer,Dollar->Long.Capacity=Dollar->Long.Length+AddingLength>LSIZE_MAX>>1?LSIZE_MAX:Dollar->Long.Length+AddingLength<<1);\
				if(!NewBuffer){\
					STATIC_ERROR(&State->Error,"Error while allocating the result of $tostring");\
					return Dollar;\
				}\
				Dollar->Long.Buffer=NewBuffer;\
			}
		#define ADD_CHARACTER(...)\
			Dollar->Long.Buffer[Dollar->Long.Length++]=__VA_ARGS__
		#define ADD_CHARACTERS(Buffer_,Length_)\
			size_t BufferLength=Length_;\
			memcpy(Dollar->Long.Buffer+Dollar->Long.Length,Buffer_,BufferLength);\
			Dollar->Long.Length+=BufferLength
		HANDLE_CONVERSION;
		#undef CHECK_LENGTH
		#undef ADD_CHARACTER
		#undef ADD_CHARACTERS
		Previous=Converting;
		Converting=Converting->Next;
	}
	#undef HANDLE_STRING_CONVERSION
	#undef HANDLE_CONVERSION
}
static Token*PredefinedConcatenate(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	for(Token*Concatenating=MacroName->Next;;){
		#define CONCATENATE_PARSE(LongType,ShortType)\
			Concatenating=Concatenating->Next;\
			for(;;){\
				if(Concatenating->Type==TOKEN_SYMBOL){\
					if(Concatenating->Symbol.Type==SYMBOL_SEMICOLON){\
						if(Concatenating->Symbol.NotNowAmount){\
							STATIC_ERROR(&State->Error,"Expected the semicolon that finishes $concat to not be notnow");\
							return Dollar;\
						}\
						break;\
					}\
					if(Concatenating->Symbol.Type==SYMBOL_DOLLAR&&!Concatenating->Symbol.NotNowAmount){\
						Concatenating=HandleDollar(Concatenating,State,L);\
						if(State->Error.Message){\
							return Dollar;\
						}\
						continue;\
					}\
					STATIC_ERROR(&State->Error,"Expected a semicolon to finish $concat");\
					return Dollar;\
				}\
				if(Concatenating->Type==(ShortType)){\
					if(Length>LSIZE_MAX-Concatenating->Short.Length){\
						STATIC_ERROR(&State->Error,"Error allocating the result of $concat");\
						return Dollar;\
					}\
					Length+=Concatenating->Short.Length;\
				}else if(Concatenating->Type==(LongType)){\
					if(Concatenating->Long.Capacity>Longest->Long.Capacity){\
						Offset=Length;\
						Longest=Concatenating;\
					}\
					if(Length>LSIZE_MAX-Concatenating->Long.Length){\
						STATIC_ERROR(&State->Error,"Error allocating the result of $concat");\
						return Dollar;\
					}\
					Length+=Concatenating->Long.Length;\
				}else{\
					STATIC_ERROR(&State->Error,"Expected a semicolon to finish $concat");\
					return Dollar;\
				}\
				Concatenating=Concatenating->Next;\
			}
		#define CONCATENATE_LONG(LongType,ShortType)\
			char*Buffer;\
			if(Length>Longest->Long.Capacity){\
				size_t Capacity=Length>LSIZE_MAX>>1?LSIZE_MAX:Length<<1;\
				if(!(Buffer=realloc(Longest->Long.Buffer,Capacity))){\
					STATIC_ERROR(&State->Error,"Error allocating the result of $concat");\
					return Dollar;\
				}\
				Dollar->Type=LongType;\
				Dollar->Long.Capacity=Capacity;\
				Dollar->Long.Length=Length;\
				Dollar->Long.Buffer=Buffer;\
			}else{\
				Dollar->Type=LongType;\
				Dollar->Long.Capacity=Longest->Long.Capacity;\
				Dollar->Long.Length=Length;\
				Buffer=Dollar->Long.Buffer=Longest->Long.Buffer;\
			}\
			char*BufferOffset=Buffer+Offset;\
			for(size_t Index=Longest->Long.Length-1;Index;--Index){\
				BufferOffset[Index]=Buffer[Index];\
			}\
			Concatenating=MacroName->Next;\
			FreeTokenDirect(MacroName);\
			while(Concatenating!=Longest){\
				if(Concatenating->Type==(ShortType)){\
					memcpy(Buffer,Concatenating->Short.Buffer,Concatenating->Short.Length);\
					Buffer+=Concatenating->Short.Length;\
				}else{\
					memcpy(Buffer,Concatenating->Long.Buffer,Concatenating->Long.Length);\
					Buffer+=Concatenating->Long.Length;\
					FreeContentsDirect(Concatenating);\
				}\
				MacroName=Concatenating->Next;\
				FreeTokenDirect(Concatenating);\
				if(MacroName==Longest){\
					break;\
				}\
				if(MacroName->Type==(ShortType)){\
					memcpy(Buffer,MacroName->Short.Buffer,MacroName->Short.Length);\
					Buffer+=MacroName->Short.Length;\
				}else{\
					memcpy(Buffer,MacroName->Long.Buffer,MacroName->Long.Length);\
					Buffer+=MacroName->Long.Length;\
					FreeContentsDirect(MacroName);\
				}\
				Concatenating=MacroName->Next;\
				FreeTokenDirect(MacroName);\
			}\
			Buffer+=Longest->Long.Length;\
			Concatenating=Longest->Next;\
			FreeTokenDirect(Longest);\
			while(Concatenating->Type!=TOKEN_SYMBOL){\
				if(Concatenating->Type==(ShortType)){\
					memcpy(Buffer,Concatenating->Short.Buffer,Concatenating->Short.Length);\
					Buffer+=Concatenating->Short.Length;\
				}else{\
					memcpy(Buffer,Concatenating->Long.Buffer,Concatenating->Long.Length);\
					Buffer+=Concatenating->Long.Length;\
					FreeContentsDirect(Concatenating);\
				}\
				MacroName=Concatenating->Next;\
				FreeTokenDirect(Concatenating);\
				if(MacroName->Type==TOKEN_SYMBOL){\
					if(Concatenating=Dollar->Next=MacroName->Next){\
						Concatenating->Previous=Dollar;\
					}\
					FreeTokenDirect(MacroName);\
					return Dollar;\
				}\
				if(MacroName->Type==(ShortType)){\
					memcpy(Buffer,MacroName->Short.Buffer,MacroName->Short.Length);\
					Buffer+=MacroName->Short.Length;\
				}else{\
					memcpy(Buffer,MacroName->Long.Buffer,MacroName->Long.Length);\
					Buffer+=MacroName->Long.Length;\
					FreeContentsDirect(MacroName);\
				}\
				Concatenating=MacroName->Next;\
				FreeTokenDirect(MacroName);\
			}\
			(Dollar->Next=Concatenating->Next)->Previous=Dollar;\
			FreeTokenDirect(Concatenating);\
			return Dollar
		#define CONCATENATE_SHORT(LongType,ShortType)\
			char*Buffer;\
			if(Length>sizeof(Longest->Short.Buffer)){\
				if(!(Buffer=malloc(Length))){\
					STATIC_ERROR(&State->Error,"Error allocating the result of $concat");\
					return Dollar;\
				}\
				Dollar->Type=LongType;\
				Dollar->Long.Capacity=Dollar->Long.Length=Length;\
				Dollar->Long.Buffer=Buffer;\
			}else{\
				Dollar->Type=ShortType;\
				Dollar->Short.Length=Length;\
				Buffer=Dollar->Short.Buffer;\
			}\
			Concatenating=MacroName->Next;\
			FreeTokenDirect(MacroName);\
			MacroName=Concatenating->Next;\
			memcpy(Buffer,Concatenating->Short.Buffer,Concatenating->Short.Length);\
			Buffer+=Concatenating->Short.Length;\
			FreeTokenDirect(Concatenating);\
			while(MacroName->Type!=TOKEN_SYMBOL){\
				memcpy(Buffer,MacroName->Short.Buffer,MacroName->Short.Length);\
				Buffer+=MacroName->Short.Length;\
				Concatenating=MacroName->Next;\
				FreeTokenDirect(MacroName);\
				if(Concatenating->Type==TOKEN_SYMBOL){\
					(Dollar->Next=Concatenating->Next)->Previous=Dollar;\
					FreeTokenDirect(Concatenating);\
					return Dollar;\
				}\
				memcpy(Buffer,Concatenating->Short.Buffer,Concatenating->Short.Length);\
				Buffer+=Concatenating->Short.Length;\
				MacroName=Concatenating->Next;\
				FreeTokenDirect(Concatenating);\
			}\
			(Dollar->Next=MacroName->Next)->Previous=Dollar;\
			FreeTokenDirect(MacroName);\
			return Dollar
		#define CONCATENATE_FULL(LongType,ShortType)\
			case ShortType:{\
				size_t Length=Concatenating->Short.Length;\
				size_t Offset;\
				Token EmptyString;/* long string with 0 capacity so there is something to test against */\
				EmptyString.Long.Capacity=0;\
				Token*Longest=&EmptyString;\
				CONCATENATE_PARSE(LongType,ShortType);\
				if(Longest!=&EmptyString){\
					CONCATENATE_LONG(LongType,ShortType);\
				}else{\
					CONCATENATE_SHORT(LongType,ShortType);\
				}\
			}\
			case LongType:{\
				size_t Length=Concatenating->Long.Length;\
				size_t Offset=0;\
				Token*Longest=Concatenating;\
				CONCATENATE_PARSE(LongType,ShortType);\
				CONCATENATE_LONG(LongType,ShortType);\
			}
		switch(Concatenating->Type){
			CONCATENATE_FULL(TOKEN_LONG_NAME,TOKEN_SHORT_NAME);
			CONCATENATE_FULL(TOKEN_LONG_STRING,TOKEN_SHORT_STRING);
		}
		#undef CONCATENATE_PARSE
		#undef CONCATENATE_LONG
		#undef CONCATENATE_SHORT
		#undef CONCATENATE_FULL
		if(Concatenating->Type!=TOKEN_SYMBOL||Concatenating->Symbol.Type!=SYMBOL_DOLLAR||Concatenating->Symbol.NotNowAmount){
			STATIC_ERROR(&State->Error,"Expected a name or string after $concat");
			return Dollar;
		}
		Concatenating=HandleDollar(Concatenating,State,L);
		if(State->Error.Message){
			return Dollar;
		}
	}
}
static Token*PredefinedIf(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	Token*Parsing=MacroName->Next;
	Token*First=0;
	Token*Last=0;
	size_t BracketsAmount=0;
	bool FoundBranch=0;
	#define EVALUATE_CONDITION(AfterErrorMessage)\
		for(;;){\
			if(Parsing->Type!=TOKEN_SYMBOL){\
				STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after "AfterErrorMessage);\
				return Dollar;\
			}\
			if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the opening bracket after "AfterErrorMessage" to not be notnow");\
					return Dollar;\
				}\
				Parsing=Parsing->Next;\
				break;\
			}\
			if(Parsing->Symbol.Type==SYMBOL_LABEL){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the double colon after "AfterErrorMessage" to not be notnow");\
					return Dollar;\
				}\
				Parsing=Parsing->Next;\
				for(;;){\
					if(Parsing->Type!=TOKEN_SYMBOL){\
						STATIC_ERROR(&State->Error,"Expected an opening bracket after the double colon after "AfterErrorMessage);\
						return Dollar;\
					}\
					if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
						if(Parsing->Symbol.NotNowAmount){\
							STATIC_ERROR(&State->Error,"Expected the opening bracket after the double colon after "AfterErrorMessage" to not be notnow");\
							return Dollar;\
						}\
						Parsing=Parsing->Next;\
						break;\
					}\
					if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
						STATIC_ERROR(&State->Error,"Expected an opening bracket after the double colon after "AfterErrorMessage);\
						return Dollar;\
					}\
					Parsing=HandleDollar(Parsing,State,L);\
					if(State->Error.Message){\
						return Dollar;\
					}\
				}\
				break;\
			}\
			if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
				STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after "AfterErrorMessage);\
				return Dollar;\
			}\
			Parsing=HandleDollar(Parsing,State,L);\
			if(State->Error.Message){\
				return Dollar;\
			}\
		}\
		for(;;){\
			if(Parsing->Type<=TOKEN_SHORT_NAME){\
				if(Parsing->Short.Length==4){\
					if(memcmp(Parsing->Short.Buffer,"true",4)){\
						STATIC_ERROR(&State->Error,"Expected a boolean after the "AfterErrorMessage" condition opening bracket");\
						return Dollar;\
					}\
					FoundBranch=1;\
				}else if(Parsing->Short.Length!=5||memcmp(Parsing->Short.Buffer,"false",5)){\
					STATIC_ERROR(&State->Error,"Expected a boolean after the "AfterErrorMessage" condition opening bracket");\
					return Dollar;\
				}\
				Parsing=Parsing->Next;\
				break;\
			}\
			if(Parsing->Type!=TOKEN_SYMBOL||Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
				STATIC_ERROR(&State->Error,"Expected a boolean after the "AfterErrorMessage" condition opening bracket");\
				return Dollar;\
			}\
			Parsing=HandleDollar(Parsing,State,L);\
			if(State->Error.Message){\
				return Dollar;\
			}\
		}\
		for(;;){\
			if(Parsing->Type!=TOKEN_SYMBOL){\
				STATIC_ERROR(&State->Error,"Expected a closing bracket after the "AfterErrorMessage" condition");\
				return Dollar;\
			}\
			if(Parsing->Symbol.Type>=SYMBOL_CLOSE_BRACE){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the closing bracket after the "AfterErrorMessage" condition to not be notnow");\
					return Dollar;\
				}\
				Parsing=Parsing->Next;\
				break;\
			}\
			if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
				STATIC_ERROR(&State->Error,"Expected a closing bracket after the "AfterErrorMessage" condition");\
				return Dollar;\
			}\
			Parsing=HandleDollar(Parsing,State,L);\
			if(State->Error.Message){\
				return Dollar;\
			}\
		}
	#define SKIP_CONDITION(AfterErrorMessage)\
		for(;;){\
			if(Parsing->Type!=TOKEN_SYMBOL){\
				STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after "AfterErrorMessage);\
				return Dollar;\
			}\
			if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the opening bracket after "AfterErrorMessage" to not be notnow");\
					return Dollar;\
				}\
				for(;;){\
					if((Parsing=Parsing->Next)->Type==TOKEN_SYMBOL){\
						if(!Parsing->Symbol.NotNowAmount){\
							if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
								if(BracketsAmount==SIZE_MAX){\
									STATIC_ERROR(&State->Error,"Bracket overflow in the "AfterErrorMessage" condition");\
									return Dollar;\
								}\
								++BracketsAmount;\
							}else if(Parsing->Symbol.Type>=SYMBOL_CLOSE_BRACE){\
								if(!BracketsAmount){\
									Parsing=Parsing->Next;\
									break;\
								}\
								--BracketsAmount;\
							}\
						}\
					}else if(!Parsing->Type){\
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets in the "AfterErrorMessage" condition");\
						return Dollar;\
					}\
				}\
				break;\
				}\
			if(Parsing->Symbol.Type==SYMBOL_LABEL){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the double colon after "AfterErrorMessage" to not be notnow");\
					return Dollar;\
				}\
				Parsing=Parsing->Next;\
				for(;;){\
					if(Parsing->Type!=TOKEN_SYMBOL){\
						STATIC_ERROR(&State->Error,"Expected an opening bracket after the double colon after "AfterErrorMessage);\
						return Dollar;\
					}\
					if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
						if(Parsing->Symbol.NotNowAmount){\
							STATIC_ERROR(&State->Error,"Expected the opening bracket after the double colon after "AfterErrorMessage" to not be notnow");\
							return Dollar;\
						}\
						Parsing=Parsing->Next;\
						break;\
					}\
					if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
						STATIC_ERROR(&State->Error,"Expected an opening bracket after the double colon after "AfterErrorMessage);\
						return Dollar;\
					}\
					Parsing=HandleDollar(Parsing,State,L);\
					if(State->Error.Message){\
						return Dollar;\
					}\
				}\
				for(;;){\
					if(Parsing->Type==TOKEN_SYMBOL){\
						if(!Parsing->Symbol.NotNowAmount){\
							if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
								if(BracketsAmount==SIZE_MAX){\
									STATIC_ERROR(&State->Error,"Bracket overflow in the "AfterErrorMessage" condition");\
									return Dollar;\
								}\
								++BracketsAmount;\
							}else if(Parsing->Symbol.Type>=SYMBOL_CLOSE_BRACE){\
								if(!BracketsAmount){\
									Parsing=Parsing->Next;\
									break;\
								}\
								--BracketsAmount;\
							}else if(Parsing->Symbol.Type==SYMBOL_DOLLAR){\
								Parsing=HandleDollar(Parsing,State,L);\
								if(State->Error.Message){\
									return Dollar;\
								}\
								continue;\
							}\
						}\
					}else if(!Parsing->Type){\
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets in the "AfterErrorMessage" condition");\
						return Dollar;\
					}\
					Parsing=Parsing->Next;\
				}\
				break;\
			}\
			if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
				STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after "AfterErrorMessage);\
				return Dollar;\
			}\
			Parsing=HandleDollar(Parsing,State,L);\
			if(State->Error.Message){\
				return Dollar;\
			}\
		}
	#define EVALUATE_BRANCH(BranchName,AfterErrorMessage)\
		for(;;){\
			if(Parsing->Type!=TOKEN_SYMBOL){\
				STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after "AfterErrorMessage);\
				return Dollar;\
			}\
			if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the opening bracket after "AfterErrorMessage" to not be notnow");\
					return Dollar;\
				}\
				Parsing=Parsing->Next;\
				break;\
			}\
			if(Parsing->Symbol.Type==SYMBOL_LABEL){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the double colon after "AfterErrorMessage" to not be notnow");\
					return Dollar;\
				}\
				Parsing=Parsing->Next;\
				for(;;){\
					if(Parsing->Type!=TOKEN_SYMBOL){\
						STATIC_ERROR(&State->Error,"Expected an opening bracket after the double colon after "AfterErrorMessage);\
						return Dollar;\
					}\
					if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
						if(Parsing->Symbol.NotNowAmount){\
							STATIC_ERROR(&State->Error,"Expected the opening bracket after the double colon after "AfterErrorMessage" to not be notnow");\
							return Dollar;\
						}\
						Parsing=Parsing->Next;\
						break;\
					}\
					if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
						STATIC_ERROR(&State->Error,"Expected an opening bracket after the double colon after "AfterErrorMessage);\
						return Dollar;\
					}\
					Parsing=HandleDollar(Parsing,State,L);\
					if(State->Error.Message){\
						return Dollar;\
					}\
				}\
				break;\
			}\
			if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
				STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after "AfterErrorMessage);\
				return Dollar;\
			}\
			Parsing=HandleDollar(Parsing,State,L);\
			if(State->Error.Message){\
				return Dollar;\
			}\
		}\
		First=Parsing;\
		for(;;){\
			if(Parsing->Type==TOKEN_SYMBOL){\
				if(Parsing->Symbol.NotNowAmount){\
					--Parsing->Symbol.NotNowAmount;\
				}else if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
					if(BracketsAmount==SIZE_MAX){\
						STATIC_ERROR(&State->Error,"Bracket overflow in the "BranchName" branch");\
						return Dollar;\
					}\
					++BracketsAmount;\
				}else if(Parsing->Symbol.Type>=SYMBOL_CLOSE_BRACE){\
					if(!BracketsAmount){\
						Parsing=(Last=Parsing)->Next;\
						break;\
					}\
					--BracketsAmount;\
				}else if(Parsing->Symbol.Type==SYMBOL_DOLLAR){\
					Parsing=HandleDollar(Parsing,State,L);\
					if(State->Error.Message){\
						return Dollar;\
					}\
					continue;\
				}\
			}else if(!Parsing->Type){\
				STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets in the "BranchName" branch");\
				return Dollar;\
			}\
			Parsing=Parsing->Next;\
		}
	#define SKIP_BRANCH(BranchName,AfterErrorMessage)\
		for(;;){\
			if(Parsing->Type!=TOKEN_SYMBOL){\
				STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after "AfterErrorMessage);\
				return Dollar;\
			}\
			if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the opening bracket after "AfterErrorMessage" to not be notnow");\
					return Dollar;\
				}\
				for(;;){\
					if((Parsing=Parsing->Next)->Type==TOKEN_SYMBOL){\
						if(!Parsing->Symbol.NotNowAmount){\
							if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
								if(BracketsAmount==SIZE_MAX){\
									STATIC_ERROR(&State->Error,"Bracket overflow in the "BranchName" branch");\
									return Dollar;\
								}\
								++BracketsAmount;\
							}else if(Parsing->Symbol.Type>=SYMBOL_CLOSE_BRACE){\
								if(!BracketsAmount){\
									Parsing=Parsing->Next;\
									break;\
								}\
								--BracketsAmount;\
							}\
						}\
					}else if(!Parsing->Type){\
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets in the "BranchName" branch");\
						return Dollar;\
					}\
				}\
				break;\
			}\
			if(Parsing->Symbol.Type==SYMBOL_LABEL){\
				if(Parsing->Symbol.NotNowAmount){\
					STATIC_ERROR(&State->Error,"Expected the double colon after "AfterErrorMessage" to not be notnow");\
					return Dollar;\
				}\
				Parsing=Parsing->Next;\
				for(;;){\
					if(Parsing->Type!=TOKEN_SYMBOL){\
						STATIC_ERROR(&State->Error,"Expected an opening bracket after the double colon after "AfterErrorMessage);\
						return Dollar;\
					}\
					if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
						if(Parsing->Symbol.NotNowAmount){\
							STATIC_ERROR(&State->Error,"Expected the opening bracket after the double colon after "AfterErrorMessage" to not be notnow");\
							return Dollar;\
						}\
						Parsing=Parsing->Next;\
						break;\
					}\
					if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
						STATIC_ERROR(&State->Error,"Expected an opening bracket after the double colon after "AfterErrorMessage);\
						return Dollar;\
					}\
					Parsing=HandleDollar(Parsing,State,L);\
					if(State->Error.Message){\
						return Dollar;\
					}\
				}\
				for(;;){\
					if(Parsing->Type==TOKEN_SYMBOL){\
						if(!Parsing->Symbol.NotNowAmount){\
							if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){\
								if(BracketsAmount==SIZE_MAX){\
									STATIC_ERROR(&State->Error,"Bracket overflow in the "BranchName" branch");\
									return Dollar;\
								}\
								++BracketsAmount;\
							}else if(Parsing->Symbol.Type>=SYMBOL_CLOSE_BRACE){\
								if(!BracketsAmount){\
									Parsing=Parsing->Next;\
									break;\
								}\
								--BracketsAmount;\
							}else if(Parsing->Symbol.Type==SYMBOL_DOLLAR){\
								Parsing=HandleDollar(Parsing,State,L);\
								if(State->Error.Message){\
									return Dollar;\
								}\
								continue;\
							}\
						}\
					}else if(!Parsing->Type){\
						STATIC_ERROR(&State->Error,"Unexpected unbalanced brackets in the "BranchName" branch");\
						return Dollar;\
					}\
					Parsing=Parsing->Next;\
				}\
				break;\
			}\
			if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){\
				STATIC_ERROR(&State->Error,"Expected an opening bracket or double colon after "AfterErrorMessage);\
				return Dollar;\
			}\
			Parsing=HandleDollar(Parsing,State,L);\
			if(State->Error.Message){\
				return Dollar;\
			}\
		}
	EVALUATE_CONDITION("if");
	if(FoundBranch){
		EVALUATE_BRANCH("if","the if closing bracket");
		Skip:;
		for(;;){
			if(Parsing->Type==TOKEN_SHORT_NAME){
				if(Parsing->Short.Length==3){
					if(memcmp(Parsing->Short.Buffer,"end",3)){
						STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
						return Dollar;
					}
					break;
				}
				if(Parsing->Short.Length==4){
					if(memcmp(Parsing->Short.Buffer,"else",4)){
						STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
						return Dollar;
					}
					Parsing=Parsing->Next;
					SKIP_BRANCH("else","else");
				}else if(Parsing->Short.Length==6){
					if(memcmp(Parsing->Short.Buffer,"elseif",6)){
						STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
						return Dollar;
					}
					Parsing=Parsing->Next;
					SKIP_CONDITION("elseif");
					SKIP_BRANCH("elseif","the elseif closing bracket");
				}else{
					STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
					return Dollar;
				}
			}else if(Parsing->Type==TOKEN_SYMBOL&&Parsing->Symbol.Type==SYMBOL_DOLLAR&&!Parsing->Symbol.NotNowAmount){
				Parsing=HandleDollar(Parsing,State,L);
				if(State->Error.Message){
					return Dollar;
				}
			}else{
				STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
				return Dollar;
			}
		}
	}else{
		SKIP_BRANCH("if","the if closing bracket");
		for(;;){
			if(Parsing->Type==TOKEN_SHORT_NAME){
				if(Parsing->Short.Length==3){
					if(memcmp(Parsing->Short.Buffer,"end",3)){
						STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
						return Dollar;
					}
					break;
				}
				if(Parsing->Short.Length==4){
					if(memcmp(Parsing->Short.Buffer,"else",4)){
						STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
						return Dollar;
					}
					Parsing=Parsing->Next;
					EVALUATE_BRANCH("else","else");
					goto Skip;
				}
				if(Parsing->Short.Length==6){
					if(memcmp(Parsing->Short.Buffer,"elseif",6)){
						STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
						return Dollar;
					}
					Parsing=Parsing->Next;
					EVALUATE_CONDITION("elseif");
					if(FoundBranch){
						EVALUATE_BRANCH("elseif","the elseif closing bracket");
						goto Skip;
					}
					SKIP_BRANCH("elseif","the elseif closing bracket");
				}else{
					STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
					return Dollar;
				}
			}else if(Parsing->Type==TOKEN_SYMBOL&&Parsing->Symbol.Type==SYMBOL_DOLLAR&&!Parsing->Symbol.NotNowAmount){
				Parsing=HandleDollar(Parsing,State,L);
				if(State->Error.Message){
					return Dollar;
				}
			}else{
				STATIC_ERROR(&State->Error,"Expected an 'end' to finish $if");
				return Dollar;
			}
		}
	}
	#undef EVALUATE_CONDITION
	#undef SKIP_CONDITION
	#undef EVALUATE_BRANCH
	#undef SKIP_BRANCH
	if(First!=Last){
		Token*Previous=Dollar->Previous;
		(Previous->Next=First)->Previous=Previous;
		FreeTokenDirect(Dollar);
		Dollar=MacroName->Next;
		FreeTokenDirect(MacroName);
		MacroName=Dollar->Next;
		FreeTokenDirect(Dollar);
		Dollar=MacroName->Next;
		FreeTokenDirect(MacroName);
		MacroName=Dollar->Next;
		FreeTokenDirect(Dollar);
		Dollar=MacroName->Next;
		FreeTokenDirect(MacroName);
		while(Dollar!=First){
			Previous=Dollar->Next;
			FreeToken(Dollar);
			if(Previous==First){
				break;
			}
			Dollar=Previous->Next;
			FreeToken(Previous);
		}
		Previous=Last->Previous;
		(Previous->Next=Parsing->Next)->Previous=Previous;
		Previous=Last->Next;
		FreeToken(Last);
		while(Previous!=Parsing){
			Last=Previous->Next;
			FreeToken(Previous);
			if(Last==Parsing){
				break;
			}
			Previous=Last->Next;
			FreeToken(Last);
		}
		FreeToken(Parsing);
		return First;
	}else{
		Last=Parsing->Next;
		(Last->Previous=Dollar->Previous)->Next=Last;
		FreeTokenDirect(Dollar);
		Dollar=MacroName->Next;
		FreeTokenDirect(MacroName);
		MacroName=Dollar->Next;
		FreeTokenDirect(Dollar);
		Dollar=MacroName->Next;
		FreeTokenDirect(MacroName);
		MacroName=Dollar->Next;
		FreeTokenDirect(Dollar);
		Dollar=MacroName->Next;
		FreeTokenDirect(MacroName);
		MacroName=Dollar->Next;
		FreeToken(Dollar);
		while(MacroName!=Parsing){
			Dollar=MacroName->Next;
			FreeToken(MacroName);
			if(Dollar==Parsing){
				break;
			}
			MacroName=Dollar->Next;
			FreeToken(Dollar);
		}
		FreeToken(Parsing);
		return Last;
	}
}
static Token*PredefinedDefined(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	char*NameBuffer;
	size_t NameLength;
	Token*Name=MacroName->Next;
	lua_getiuservalue(L,1,1);
	lua_pushcfunction(L,LuaGetTable);
	lua_pushvalue(L,-2);
	lua_pushcfunction(L,LuaPushString);
	for(;;){
		if(Name->Type<=TOKEN_SHORT_NAME){
			lua_pushlightuserdata(L,NameBuffer=Name->Short.Buffer);
			lua_pushinteger(L,NameLength=Name->Short.Length);
			break;
		}
		if(Name->Type>=TOKEN_LONG_NAME){
			lua_pushlightuserdata(L,NameBuffer=Name->Long.Buffer);
			lua_pushinteger(L,NameLength=Name->Long.Length);
			break;
		}
		if(Name->Type!=TOKEN_SYMBOL||Name->Symbol.Type!=SYMBOL_DOLLAR||Name->Symbol.NotNowAmount){
			STATIC_ERROR(&State->Error,"Expected a name or string after $defined");
			lua_pop(L,4);
			return Dollar;
		}
		Name=HandleDollar(Name,State,L);
		if(State->Error.Message){
			return Dollar;
		}
	}
	if(lua_pcall(L,2,1,0)){
		STATIC_ERROR(&State->Error,"Error allocating the string in $defined");
		lua_pop(L,4);
		return Dollar;
	}
	if(lua_pcall(L,2,1,0)){
		STATIC_ERROR(&State->Error,"Error when finding macro in $defined");
		lua_pop(L,2);
		return Dollar;
	}
	bool IsDefined;
	if(lua_istable(L,-1)){
		for(;;){
			lua_copy(L,-1,-2);
			lua_pop(L,1);
			Name=Name->Next;
			for(;;){
				if(Name->Type!=TOKEN_SYMBOL||Name->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected a period to delimit names after $defined");
					lua_pop(L,1);
					return Dollar;
				}
				if(Name->Symbol.Type==SYMBOL_DOT){
					break;
				}
				if(Name->Symbol.Type!=SYMBOL_DOLLAR){
					STATIC_ERROR(&State->Error,"Expected a period to delimit names after $defined");
					lua_pop(L,1);
					return Dollar;
				}
				Name=HandleDollar(Name,State,L);
				if(State->Error.Message){
					lua_pop(L,1);
					return Dollar;
				}
			}
			Name=Name->Next;
			lua_pushcfunction(L,LuaGetTable);
			lua_pushvalue(L,-2);
			lua_pushcfunction(L,LuaPushString);
			for(;;){
				if(Name->Type<=TOKEN_SHORT_NAME){
					lua_pushlightuserdata(L,Name->Short.Buffer);
					lua_pushinteger(L,Name->Short.Length);
					break;
				}
				if(Name->Type>=TOKEN_LONG_NAME){
					lua_pushlightuserdata(L,Name->Long.Buffer);
					lua_pushinteger(L,Name->Long.Length);
					break;
				}
				if(Name->Type!=TOKEN_SYMBOL||Name->Symbol.Type!=SYMBOL_DOLLAR||Name->Symbol.NotNowAmount){
					STATIC_ERROR(&State->Error,"Expected a name or string after the period after $defined");
					lua_pop(L,4);
					return Dollar;
				}
				Name=HandleDollar(Name,State,L);
				if(State->Error.Message){
					lua_pop(L,4);
					return Dollar;
				}
			}
			if(lua_pcall(L,2,1,0)){
				STATIC_ERROR(&State->Error,"Error allocating string in $defined");
				lua_pop(L,4);
				return Dollar;
			}
			if(lua_pcall(L,2,1,0)){
				STATIC_ERROR(&State->Error,"Error when finding macro in $defined");
				lua_pop(L,2);
				return Dollar;
			}
			if(!lua_istable(L,-1)){
				Dollar->Type=TOKEN_SHORT_NAME;
				if(lua_isfunction(L,-1)){
					memcpy(Dollar->Short.Buffer,"true",Dollar->Short.Length=4);
				}else{
					memcpy(Dollar->Short.Buffer,"false",Dollar->Short.Length=5);
				}
				Token*Next=Name->Next;
				(Dollar->Next=Next)->Previous=Dollar;
				Next=MacroName->Next;
				FreeTokenDirect(MacroName);
				MacroName=Next->Next;
				FreeToken(Next);
				Next=MacroName->Next;
				FreeTokenDirect(MacroName);
				while(Next!=Name){
					MacroName=Next->Next;
					FreeToken(Next);
					Next=MacroName->Next;
					FreeTokenDirect(MacroName);
				}
				FreeToken(Name);
				return Dollar;
			}
		}
	}
	if(lua_type(L,-1)==LUA_TUSERDATA){
		lua_remove(L,-2);
		lua_getmetatable(L,-1);
		lua_rawgetp(L,LUA_REGISTRYINDEX,PredefinedMacroMetatable);
		if(lua_rawequal(L,-1,-2)){
			NamedPredefinedMacro*Macro=lua_touserdata(L,-3);
			IsDefined=Macro->Length==NameLength&&!memcmp(Macro->Buffer,NameBuffer,NameLength);
		}else{
			IsDefined=0;
		}
		lua_pop(L,3);
	}else{
		IsDefined=lua_isfunction(L,-1);
		lua_pop(L,2);
	}
	Dollar->Type=TOKEN_SHORT_NAME;
	if(IsDefined){
		memcpy(Dollar->Short.Buffer,"true",Dollar->Short.Length=4);
	}else{
		memcpy(Dollar->Short.Buffer,"false",Dollar->Short.Length=5);
	}
	FreeTokenDirect(MacroName);
	MacroName=Name->Next;
	FreeToken(Name);
	(Dollar->Next=MacroName)->Previous=Dollar;
	return Dollar;
}
enum LuaReaderHandling{
	READER_NONE,/* nothing extra to do */
	READER_SHORT_STRING,/* unfinished short string to read */
	READER_LONG_STRING,/* unfinished long string to read */
	READER_EXIT,/* exit */
	READER_SPACE,/* add a space */
	READER_RETURN/* add "return " */
};
typedef enum LuaReaderHandling LuaReaderHandling;
typedef struct LuaReaderState LuaReaderState;
struct LuaReaderState{
	PreprocessorState*State;
	Token*Parsing;
	size_t BracketsAmount;
	size_t StringIndex;
	signed char Handling;
	bool IsExpression;
	char Buffer[NUMBER_BUFFER_SIZE>128?NUMBER_BUFFER_SIZE:128];
};
static const char*LuaReader(lua_State*L,void*VoidReading,size_t*Size){
	LuaReaderState*Reading=VoidReading;
	switch(Reading->Handling){
		#define STRING_BASIC_ESCAPE(Escape,Character)\
			case Escape:{\
				if(Index>sizeof(Reading->Buffer)-2){\
					*Size=Index;\
					return Reading->Buffer;\
				}\
				++Reading->StringIndex;\
				Reading->Buffer[Index++]='\\';\
				Reading->Buffer[Index++]=Character;\
				continue;\
			}
		#define STRING_HANDLING(Name)\
			size_t Index=0;\
			while(Reading->StringIndex!=Reading->Parsing->Name.Length){\
				char Character=Reading->Parsing->Name.Buffer[Reading->StringIndex];\
				switch(Character){\
					STRING_BASIC_ESCAPE('\a','a');\
					STRING_BASIC_ESCAPE('\b','b');\
					STRING_BASIC_ESCAPE('\n','n');\
					STRING_BASIC_ESCAPE('\r','r');\
					STRING_BASIC_ESCAPE('\t','t');\
					STRING_BASIC_ESCAPE('\v','v');\
					STRING_BASIC_ESCAPE('"','"');\
					STRING_BASIC_ESCAPE('\\','\\');\
				}\
				if(IsControl(Character)&&(unsigned char)Character<=0XFF){\
					if(Index>sizeof(Reading->Buffer)-4){\
						*Size=Index;\
						return Reading->Buffer;\
					}\
					++Reading->StringIndex;\
					Reading->Buffer[Index++]='\\';\
					Reading->Buffer[Index++]='x';\
					Reading->Buffer[Index++]=HexadecimalDigitToCharacter((unsigned)(unsigned char)Character>>4);\
					Reading->Buffer[Index++]=HexadecimalDigitToCharacter((unsigned char)Character&0XFU);\
				}else{\
					if(Index==sizeof(Reading->Buffer)){\
						*Size=sizeof(Reading->Buffer);\
						return Reading->Buffer;\
					}\
					++Reading->StringIndex;\
					Reading->Buffer[Index++]=Character;\
				}\
			}\
			if(Index!=sizeof(Reading->Buffer)){\
				Reading->Handling=READER_NONE;\
				Reading->Parsing=Reading->Parsing->Next;\
				Reading->Buffer[Index++]='"';\
			}\
			*Size=Index;\
			return Reading->Buffer
		case READER_SHORT_STRING:{
			STRING_HANDLING(Short);
		}
		case READER_LONG_STRING:{
			STRING_HANDLING(Long);
		}
		#undef STRING_BASIC_ESCAPE
		#undef STRING_HANDLING
		case READER_EXIT:{
			*Size=0;
			return 0;
		}
		case READER_SPACE:{
			Reading->Handling=READER_NONE;
			*Size=1;
			return" ";
		}
		case READER_RETURN:{
			Reading->Handling=READER_NONE;
			*Size=7;
			return"return ";
		}
	}
	for(;;){
		switch(Reading->Parsing->Type){
			case TOKEN_SHORT_NAME:{
				const char*Buffer=Reading->Parsing->Short.Buffer;
				*Size=Reading->Parsing->Short.Length;
				Reading->Parsing=Reading->Parsing->Next;
				Reading->Handling=READER_SPACE;
				return Buffer;
			}
			case TOKEN_LONG_NAME:{
				const char*Buffer=Reading->Parsing->Long.Buffer;
				*Size=Reading->Parsing->Long.Length;
				Reading->Parsing=Reading->Parsing->Next;
				Reading->Handling=READER_SPACE;
				return Buffer;
			}
			case TOKEN_SHORT_STRING:{
				Reading->Handling=READER_SHORT_STRING;
				Reading->StringIndex=0;
				*Size=1;
				return"\"";
			}
			case TOKEN_LONG_STRING:{
				Reading->Handling=READER_LONG_STRING;
				Reading->StringIndex=0;
				*Size=1;
				return"\"";
			}
			case TOKEN_INVALID:{
				STATIC_ERROR(&Reading->State->Error,"Unexpected unbalanced brackets after $lua");
				*Size=0;
				return 0;
			}
			case TOKEN_INTEGER:{
				int Length=Csnprintf(Reading->Buffer,sizeof(Reading->Buffer),"%#"LUA_INTEGER_FRMLEN"X",(lua_Unsigned)Reading->Parsing->Integer);
				if(Length<0||Length>=sizeof(Reading->Buffer)){
					STATIC_ERROR(&Reading->State->Error,"Error while formatting number in $lua");
					*Size=0;
					return 0;
				}
				Reading->Parsing=Reading->Parsing->Next;
				Reading->Handling=READER_SPACE;
				*Size=Length;
				return Reading->Buffer;
			}
			case TOKEN_FLOAT:{
				int Length=isfinite(Reading->Parsing->Float)?Csnprintf(Reading->Buffer,sizeof(Reading->Buffer),"%"LUA_NUMBER_FRMLEN"A",Reading->Parsing->Float):Csnprintf(Reading->Buffer,sizeof(Reading->Buffer),"0X%jXP+%ju",(uintmax_t)FLT_RADIX,(uintmax_t)l_floatatt(MAX_EXP));
				if(Length<0||Length>=sizeof(Reading->Buffer)){
					STATIC_ERROR(&Reading->State->Error,"Error while formatting number in $lua");
					*Size=0;
					return 0;
				}
				Reading->Parsing=Reading->Parsing->Next;
				Reading->Handling=READER_SPACE;
				*Size=Length;
				return Reading->Buffer;
			}
			case TOKEN_SYMBOL:{
				if(Reading->Parsing->Symbol.NotNowAmount){
					if(Reading->Parsing->Symbol.NotNowAmount!=1){
						STATIC_ERROR(&Reading->State->Error,"Expected the symbols provided to $lua to not be notnow");
						*Size=0;
						return 0;
					}
					signed char ResultType=Reading->Parsing->Symbol.Type;
					Reading->Parsing=Reading->Parsing->Next;
					*Size=SymbolTokenLengths[ResultType];
					Reading->Handling=READER_SPACE;
					return SymbolTokens[ResultType];
				}
				if(Reading->Parsing->Symbol.Type!=SYMBOL_DOLLAR){
					if(Reading->Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){
						if(Reading->BracketsAmount==SIZE_MAX){
							STATIC_ERROR(&Reading->State->Error,"Bracket overflow after $lua");
							*Size=0;
							return 0;
						}
						++Reading->BracketsAmount;
					}else if(Reading->Parsing->Symbol.Type>=SYMBOL_CLOSE_BRACE){
						if(!Reading->BracketsAmount){
							if(Reading->IsExpression){
								Reading->Handling=READER_EXIT;
								*Size=1;
								return";";
							}
							*Size=0;
							return 0;
						}
						--Reading->BracketsAmount;
					}
					signed char ResultType=Reading->Parsing->Symbol.Type;
					Reading->Parsing=Reading->Parsing->Next;
					*Size=SymbolTokenLengths[ResultType];
					Reading->Handling=READER_SPACE;
					return SymbolTokens[ResultType];
				}
				Reading->Parsing=HandleDollar(Reading->Parsing,Reading->State,L);
				if(Reading->State->Error.Message){
					*Size=0;
					return 0;
				}
			}
		}
	}
}
static Token*PredefinedLua(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	int Top=lua_gettop(L);
	Token*Parsing=MacroName->Next;
	for(;;){
		if(Parsing->Type!=TOKEN_SYMBOL){
			STATIC_ERROR(&State->Error,"Expected an opening bracket after $lua");
			return Dollar;
		}
		if(Parsing->Symbol.Type<=SYMBOL_OPEN_BRACE){
			if(Parsing->Symbol.NotNowAmount){
				STATIC_ERROR(&State->Error,"Expected the opening bracket after $lua to not be notnow");
				return Dollar;
			}
			break;
		}
		if(Parsing->Symbol.Type!=SYMBOL_DOLLAR||Parsing->Symbol.NotNowAmount){
			STATIC_ERROR(&State->Error,"Expected an opening bracket after $lua");
			return Dollar;
		}
		Parsing=HandleDollar(Parsing,State,L);
		if(State->Error.Message){
			return Dollar;
		}
	}
	LuaReaderState Reading;
	Reading.State=State;
	Reading.Parsing=Parsing->Next;
	Reading.BracketsAmount=0;
	Reading.Handling=READER_RETURN;
	Reading.IsExpression=1;
	#define DYNAMIC_ERROR(Message_)\
		if(!State->Error.Message){\
			if(lua_type(L,-1)==LUA_TSTRING){\
				size_t Length;\
				const char*String=lua_tolstring(L,-1,&Length);\
				size_t Capacity=Length<16?16:Length;\
				char*Buffer=malloc(Capacity);\
				if(Buffer){\
					memcpy(State->Error.Message=Buffer,String,State->Error.Length=Length);\
					State->Error.Capacity=Capacity;\
					State->Error.Type=ERROR_ALLOCATED;\
				}else{\
					MemoryError(&State->Error);\
				}\
			}else{\
				STATIC_ERROR(&State->Error,Message_);\
			}\
		}\
		lua_settop(L,Top);\
		return Dollar
	if(lua_load(L,LuaReader,&Reading,"@$lua directive","t")){
		lua_settop(L,Top);
		if(State->Error.Message){
			return Dollar;
		}
		Reading.Parsing=Parsing->Next;
		Reading.BracketsAmount=0;
		Reading.Handling=READER_NONE;
		Reading.IsExpression=0;
		if(lua_load(L,LuaReader,&Reading,"@$lua directive","t")){
			DYNAMIC_ERROR("Expected the program provided to $lua to be valid");
		}
	}
	if(State->Error.Message){
		lua_settop(L,Top);
		return Dollar;
	}
	lua_pushvalue(L,1);
	State->Cursor=(State->CursorStart=Reading.Parsing)->Next;
	if(lua_pcall(L,1,LUA_MULTRET,0)){
		DYNAMIC_ERROR("Unexpected error in the $lua program");
	}
	#undef DYNAMIC_ERROR
	if(State->Error.Message){
		return Dollar;
	}
	State->CursorStart=0;
	if(lua_gettop(L)==Top){
		FreeTokenDirect(MacroName);
		MacroName=Parsing->Next;
		FreeTokenDirect(Parsing);
		while(MacroName!=Reading.Parsing){
			Parsing=MacroName->Next;
			FreeToken(MacroName);
			if(Parsing==Reading.Parsing){
				break;
			}
			MacroName=Parsing->Next;
			FreeToken(Parsing);
		}
		MacroName=Dollar->Previous;
		FreeTokenDirect(Dollar);
		Parsing=Reading.Parsing->Next;
		FreeTokenDirect(Reading.Parsing);
		return(Parsing->Previous=MacroName)->Next=Parsing;
	}
	lua_settop(L,Top+1);
	switch(lua_type(L,-1)){
		case LUA_TNIL:{
			Dollar->Type=TOKEN_SHORT_NAME;
			memcpy(Dollar->Short.Buffer,"nil",Dollar->Short.Length=3);
			goto Done;
		}
		case LUA_TBOOLEAN:{
			Dollar->Type=TOKEN_SHORT_NAME;
			if(lua_toboolean(L,-1)){
				memcpy(Dollar->Short.Buffer,"true",Dollar->Short.Length=4);
			}else{
				memcpy(Dollar->Short.Buffer,"false",Dollar->Short.Length=5);
			}
			goto Done;
		}
		case LUA_TNUMBER:{
			if(lua_isinteger(L,-1)){
				Dollar->Type=TOKEN_INTEGER;
				Dollar->Integer=lua_tointeger(L,-1);
			}else{
				lua_Number Float=lua_tonumber(L,-1);
				if(isnan(Float)){
					lua_settop(L,Top);
					STATIC_ERROR(&State->Error,"Expected the value returned by the $lua program to not be NAN");
					return Dollar;
				}
				if(signbit(Float)){
					Dollar->Symbol.Type=SYMBOL_OPEN_PARENTHESIS;
					MacroName->Type=TOKEN_SYMBOL;
					MacroName->Symbol.Type=SYMBOL_SUBTRACT;
					MacroName->Symbol.NotNowAmount=0;
					Parsing->Type=TOKEN_FLOAT;
					Parsing->Float=-Float;
					Parsing=Parsing->Next;
					FreeContents(Parsing);
					Parsing->Type=TOKEN_SYMBOL;
					Parsing->Symbol.Type=SYMBOL_CLOSE_PARENTHESIS;
					Parsing->Symbol.NotNowAmount=0;
					MacroName=Parsing->Next;
					(Parsing->Next=Reading.Parsing->Next)->Previous=Parsing;
					while(MacroName!=Reading.Parsing){
						Parsing=MacroName->Next;
						FreeToken(MacroName);
						if(Parsing==Reading.Parsing){
							break;
						}
						MacroName=Parsing->Next;
						FreeToken(Parsing);
					}
					FreeTokenDirect(Reading.Parsing);
					break;
				}
				Dollar->Type=TOKEN_FLOAT;
				Dollar->Float=Float;
			}
			goto Done;
		}
		case LUA_TSTRING:{
			size_t Length;
			const char*String=lua_tolstring(L,-1,&Length);
			if(Length<=sizeof(Dollar->Short.Buffer)){
				Dollar->Type=TOKEN_SHORT_STRING;
				memcpy(Dollar->Short.Buffer,String,Dollar->Short.Length=Length);
			}else{
				char*Buffer=malloc(Length);
				if(!Buffer){
					lua_settop(L,Top);
					STATIC_ERROR(&State->Error,"Error while allocating the result for $lua");
					return Dollar;
				}
				Dollar->Type=TOKEN_LONG_STRING;
				memcpy(Dollar->Long.Buffer=Buffer,String,Dollar->Long.Capacity=Dollar->Long.Length=Length);
			}
		}
		Done:;
		(Dollar->Next=Reading.Parsing->Next)->Previous=Dollar;
		FreeTokenDirect(MacroName);
		MacroName=Parsing->Next;
		FreeTokenDirect(Parsing);
		Parsing=MacroName->Next;
		FreeToken(MacroName);
		while(Parsing!=Reading.Parsing){
			MacroName=Parsing->Next;
			FreeToken(Parsing);
			if(MacroName==Reading.Parsing){
				break;
			}
			Parsing=MacroName->Next;
			FreeToken(MacroName);
		}
		FreeTokenDirect(Reading.Parsing);
		break;
		case LUA_TTABLE:{
			TokenList Output={0};
			for(lua_Integer Index=1;;){
				lua_pushcfunction(L,LuaGetTable);
				lua_pushvalue(L,-2);
				lua_pushinteger(L,Index);
				if(lua_pcall(L,2,1,0)){
					lua_settop(L,Top);
					STATIC_ERROR(&State->Error,"Error reading values from table returned by the $lua program");
					return Dollar;
				}
				if(lua_type(L,-1)!=LUA_TSTRING){
					if(lua_isnil(L,-1)){
						break;
					}
					lua_settop(L,Top);
					STATIC_ERROR(&State->Error,"Unexpected value in table returned by the $lua program");
					return Dollar;
				}
				size_t Length;
				const char*String=lua_tolstring(L,-1,&Length);
				TokenList Extension=MakeTokenList(String,Length,&State->Error);
				if(State->Error.Message){
					lua_settop(L,Top);
					while(Output.First){
						Token*Next=Output.First->Next;
						FreeToken(Output.First);
						if(!Next){
							break;
						}
						Output.First=Next->Next;
						FreeToken(Next);
					}
					return Dollar;
				}
				if(Extension.First){
					if(Output.Last){
						(Extension.First->Previous=Output.Last)->Next=Extension.First;
						Output.Last=Extension.Last;
					}else{
						Output=Extension;
					}
				}
				if(Index==LUA_MAXINTEGER){
					break;
				}
				lua_pop(L,1);
				++Index;
			}
			lua_settop(L,Top);
			if(!Output.First){
				Output.First=Dollar->Previous;
				(Output.Last=Output.First->Next=Reading.Parsing->Next)->Previous=Output.First;
				FreeTokenDirect(Dollar);
				FreeTokenDirect(MacroName);
				MacroName=Parsing->Next;
				FreeTokenDirect(Parsing);
				Parsing=MacroName->Next;
				FreeToken(MacroName);
				while(Parsing!=Reading.Parsing){
					MacroName=Parsing->Next;
					FreeToken(Parsing);
					if(MacroName==Reading.Parsing){
						break;
					}
					Parsing=MacroName->Next;
					FreeToken(MacroName);
				}
				FreeTokenDirect(Reading.Parsing);
				return Output.Last;
			}
			(Output.First->Previous=Dollar->Previous)->Next=Output.First;
			FreeTokenDirect(Dollar);
			(Output.Last->Next=Reading.Parsing->Next)->Previous=Output.Last;
			FreeTokenDirect(MacroName);
			MacroName=Parsing->Next;
			FreeTokenDirect(Parsing);
			Parsing=MacroName->Next;
			FreeToken(MacroName);
			while(Parsing!=Reading.Parsing){
				MacroName=Parsing->Next;
				FreeToken(Parsing);
				if(MacroName==Reading.Parsing){
					break;
				}
				Parsing=MacroName->Next;
				FreeToken(MacroName);
			}
			FreeTokenDirect(Reading.Parsing);
			return Output.First;
		}
		default:{
			STATIC_ERROR(&State->Error,"Unexpected return value from the $lua program");
			break;
		}
	}
	lua_settop(L,Top);
	return Dollar;
}
static Token*PredefinedNone(Token*Dollar,Token*MacroName,PreprocessorState*State,lua_State*L){
	Token*Next=MacroName->Next;
	FreeTokenDirect(MacroName);
	MacroName=Dollar->Previous;
	FreeTokenDirect(Dollar);
	return(Next->Previous=MacroName)->Next=Next;
}
static int LuaInvalidIndex(lua_State*L){
	return luaL_error(L,"invalid index");
}
static int LuaMain(lua_State*L){
	lua_gc(L,LUA_GCGEN,0,0);
	LoadLibraries(L);
	lua_createtable(L,0,2);
	typedef struct NamedMacroMethod NamedMacroMethod;
	struct NamedMacroMethod{
		const char*Buffer;
		size_t Length;/* Length<=LSIZE_MAX */
		lua_CFunction Function;
	};
	static const NamedMacroMethod MacroMethodsList[]={
		"get_content",11,MacroGetContent,
		"set_content",11,MacroSetContent,
		"get_type",8,MacroGetType,
		"set_type",8,MacroSetType,
		"get_not_now_amount",18,MacroGetNotNowAmount,
		"set_not_now_amount",18,MacroSetNotNowAmount,
		"is_valid",8,MacroIsValid,
		"make_invalid",12,MacroMakeInvalid,
		"is_advancing_valid",18,MacroIsAdvancingValid,
		"is_retreating_valid",19,MacroIsRetreatingValid,
		"go_to_start",11,MacroGoToStart,
		"go_to_end",9,MacroGoToEnd,
		"advance",7,MacroAdvance,
		"retreat",7,MacroRetreat,
		"remove_and_advance",18,MacroRemoveAndAdvance,
		"remove_and_retreat",18,MacroRemoveAndRetreat,
		"insert_at_start",15,MacroInsertAtStart,
		"insert_at_end",13,MacroInsertAtEnd,
		"insert_ahead",12,MacroInsertAhead,
		"insert_behind",13,MacroInsertBehind,
		"insert_at_start_and_stay",24,MacroInsertAtStartAndStay,
		"insert_at_end_and_stay",22,MacroInsertAtEndAndStay,
		"insert_ahead_and_stay",21,MacroInsertAheadAndStay,
		"insert_behind_and_stay",12,MacroInsertBehindAndStay,
		"steal_to_start_and_advance",26,MacroStealToStartAndAdvance,
		"steal_to_end_and_advance",24,MacroStealToEndAndAdvance,
		"steal_ahead_and_advance",23,MacroStealAheadAndAdvance,
		"steal_behind_and_advance",24,MacroStealBehindAndAdvance,
		"steal_to_start_and_retreat",26,MacroStealToStartAndRetreat,
		"steal_to_end_and_retreat",24,MacroStealToEndAndRetreat,
		"steal_ahead_and_retreat",23,MacroStealAheadAndRetreat,
		"steal_behind_and_retreat",24,MacroStealBehindAndRetreat,
		"handle_dollar",13,MacroHandleDollar,
		"handle_dollars_and_not_nows",27,MacroHandleDollarsAndNotNows,
		"copy",4,MacroCopy,
		"shift_to_start",14,MacroShiftToStart,
		"shift_to_start_and_advance",26,MacroShiftToStartAndAdvance,
		"shift_to_start_and_retreat",26,MacroShiftToStartAndRetreat,
		"shift_to_end",12,MacroShiftToEnd,
		"shift_to_end_and_advance",24,MacroShiftToEndAndAdvance,
		"shift_to_end_and_retreat",24,MacroShiftToEndAndRetreat,
		"swap_with_start",15,MacroSwapWithStart,
		"swap_with_end",13,MacroSwapWithEnd,
		"swap_ahead",10,MacroSwapAhead,
		"swap_behind",11,MacroSwapBehind,
		"swap_between",12,MacroSwapBetween,
		"get_macros",10,MacroGetMacros,
		"set_macros",10,MacroSetMacros,
		"get_error",9,MacroGetError,
		"set_error",9,MacroSetError,
		"clear",5,MacroClear
	};
	lua_createtable(L,0,sizeof(MacroMethodsList)/sizeof(*MacroMethodsList));
	lua_createtable(L,0,1);
	lua_pushcfunction(L,LuaInvalidIndex);
	lua_setfield(L,-2,"__index");
	lua_setmetatable(L,-2);
	for(size_t Index=0;Index!=sizeof(MacroMethodsList)/sizeof(*MacroMethodsList);++Index){
		lua_pushlstring(L,MacroMethodsList[Index].Buffer,MacroMethodsList[Index].Length);
		lua_pushcfunction(L,MacroMethodsList[Index].Function);
		lua_rawset(L,-3);
	}
	lua_setfield(L,-2,"__index");
	lua_pushliteral(L,"locked");
	lua_setfield(L,-2,"__metatable");
	lua_pushliteral(L,"Tokens");
	lua_setfield(L,-2,"__name");
	lua_pushcfunction(L,LuaTokenListGC);
	lua_setfield(L,-2,"__gc");
	lua_rawsetp(L,LUA_REGISTRYINDEX,PreprocessorStateMetatable);
	lua_register(L,"tokens",LuaCreateTokenList);
	PushPreprocessorState(L)->CursorStart=0;
	static const NamedPredefinedMacro PredefinedMacrosList[]={
		"now",3,PredefinedNow,
		"notnow",6,PredefinedNotNow,
		"totokens",8,PredefinedToTokens,
		"tostring",8,PredefinedToString,
		"concat",6,PredefinedConcatenate,
		"if",2,PredefinedIf,
		"defined",7,PredefinedDefined,
		"lua",3,PredefinedLua,
		"none",4,PredefinedNone
	};
	lua_createtable(L,sizeof(PredefinedMacrosList)/sizeof(*PredefinedMacrosList),0);
	lua_createtable(L,0,1);
	lua_pushliteral(L,"locked");
	lua_setfield(L,-2,"__metatable");
	for(size_t Index=0;Index!=sizeof(PredefinedMacrosList)/sizeof(*PredefinedMacrosList);++Index){
		NamedPredefinedMacro Macro=PredefinedMacrosList[Index];
		lua_pushlstring(L,Macro.Buffer,Macro.Length);
		*(NamedPredefinedMacro*)lua_newuserdatauv(L,sizeof(Macro),0)=Macro;
		lua_pushvalue(L,-3);
		lua_setmetatable(L,-2);
		lua_rawset(L,-4);
	}
	lua_rawsetp(L,LUA_REGISTRYINDEX,PredefinedMacroMetatable);
	lua_setiuservalue(L,-2,1);
	return 1;
}
static bool PrintTokens(Token*Printing,FILE*Output){
	for(Token*Previous=Printing->Previous;;){
		#define PUT_CHARACTER(...)\
			if(fputc(__VA_ARGS__,Output)==EOF&&ferror(Output)){\
				perror("Error writing to output");\
				return 0;\
			}
		#define PRINT(...)\
			if(Cfprintf(Output,__VA_ARGS__)<0){\
				perror("Error writing to output");\
				return 0;\
			}
		#define WRITE(Buffer,Amount_)\
			size_t Amount=Amount_;\
			if(fwrite(Buffer,1,Amount,Output)!=Amount){\
				perror("Error writing to output");\
				return 0;\
			}
		switch(Printing->Type){
			case TOKEN_SHORT_NAME:{
				if(Previous->Type==TOKEN_SHORT_NAME||Previous->Type==TOKEN_LONG_NAME||Previous->Type==TOKEN_INTEGER||Previous->Type==TOKEN_FLOAT){
					PUT_CHARACTER(' ');
				}
				WRITE(Printing->Short.Buffer,Printing->Short.Length);
				break;
			}
			case TOKEN_LONG_NAME:{
				if(Previous->Type==TOKEN_SHORT_NAME||Previous->Type==TOKEN_LONG_NAME||Previous->Type==TOKEN_INTEGER||Previous->Type==TOKEN_FLOAT){
					PUT_CHARACTER(' ');
				}
				WRITE(Printing->Long.Buffer,Printing->Long.Length);
				break;
			}
			#define PRINT_STRING(Name)\
				PUT_CHARACTER('"');\
				for(size_t Index=0;Index!=Printing->Name.Length;++Index){\
					switch(Printing->Name.Buffer[Index]){\
						case'\a':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('a');\
							break;\
						}\
						case'\b':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('b');\
							break;\
						}\
						case'\f':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('f');\
							break;\
						}\
						case'\n':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('n');\
							break;\
						}\
						case'\r':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('r');\
							break;\
						}\
						case'\t':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('t');\
							break;\
						}\
						case'\v':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('v');\
							break;\
						}\
						case'\"':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('"');\
							break;\
						}\
						case'\\':{\
							PUT_CHARACTER('\\');\
							PUT_CHARACTER('\\');\
							break;\
						}\
						default:{\
							if(IsControl(Printing->Name.Buffer[Index])&&(unsigned char)Printing->Name.Buffer[Index]<=0XFF){\
								PUT_CHARACTER('\\');\
								PUT_CHARACTER('x');\
								PUT_CHARACTER(HexadecimalDigitToCharacter((unsigned)(unsigned char)Printing->Name.Buffer[Index]>>4));\
								PUT_CHARACTER(HexadecimalDigitToCharacter((unsigned char)Printing->Name.Buffer[Index]&0XFU));\
							}else{\
								PUT_CHARACTER(Printing->Name.Buffer[Index]);\
							}\
						}\
					}\
				}\
				PUT_CHARACTER('"')
			case TOKEN_SHORT_STRING:{
				PRINT_STRING(Short);
				break;
			}
			case TOKEN_LONG_STRING:{
				PRINT_STRING(Long);
				break;
			}
			#undef PRINT_STRING
			case TOKEN_INVALID:{
				if(fputc('\n',Output)==EOF&&ferror(Output)){
					perror("Error writing to output");
					return 0;
				}
				return 1;
			}
			case TOKEN_INTEGER:{
				PRINT(" %#"LUA_INTEGER_FRMLEN"X"+(Previous->Type!=TOKEN_SHORT_NAME&&Previous->Type!=TOKEN_LONG_NAME&&Previous->Type!=TOKEN_INTEGER&&Previous->Type!=TOKEN_FLOAT&&(Previous->Type!=TOKEN_SYMBOL||Previous->Symbol.Type!=SYMBOL_DOT)),(lua_Unsigned)Printing->Integer);
				break;
			}
			case TOKEN_FLOAT:{
				bool SkipSpace=Previous->Type!=TOKEN_SHORT_NAME&&Previous->Type!=TOKEN_LONG_NAME&&Previous->Type!=TOKEN_INTEGER&&Previous->Type!=TOKEN_FLOAT&&(Previous->Type!=TOKEN_SYMBOL||Previous->Symbol.Type!=SYMBOL_DOT);
				if(isfinite(Printing->Float)){
					PRINT(" %"LUA_NUMBER_FRMLEN"A"+SkipSpace,Printing->Float);
				}else{
					PRINT(" 0X%jXP+%ju"+SkipSpace,(uintmax_t)FLT_RADIX,(uintmax_t)l_floatatt(MAX_EXP));
				}
				break;
			}
			case TOKEN_SYMBOL:{
				if(Printing->Symbol.NotNowAmount){
					fputc('\n',Output);
					fputs("Symbol '",stderr);
					fwrite(SymbolTokens[Printing->Symbol.Type],1,SymbolTokenLengths[Printing->Symbol.Type],stderr);
					fputs("' with extra notnow found\n",stderr);
					return 0;
				}
				switch(Printing->Symbol.Type){
					case SYMBOL_OPEN_BRACKET:{
						if(Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_OPEN_BRACKET){
							PUT_CHARACTER(' ');
						}
						break;
					}
					case SYMBOL_SUBTRACT:{
						if(Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_SUBTRACT){
							PUT_CHARACTER(' ');
						}
						break;
					}
					case SYMBOL_DIVIDE:case SYMBOL_FLOOR_DIVIDE:{
						if(Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_DIVIDE){
							PUT_CHARACTER(' ');
						}
						break;
					}
					case SYMBOL_LESSER:case SYMBOL_LESSER_EQUAL:case SYMBOL_LEFT_SHIFT:{
						if(Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_LESSER){
							PUT_CHARACTER(' ');
						}
						break;
					}
					case SYMBOL_GREATER:case SYMBOL_GREATER_EQUAL:case SYMBOL_RIGHT_SHIFT:{
						if(Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_GREATER){
							PUT_CHARACTER(' ');
						}
						break;
					}
					case SYMBOL_ASSIGN:case SYMBOL_EQUALS:{
						if(Previous->Type==TOKEN_SYMBOL&&(Previous->Symbol.Type==SYMBOL_ASSIGN||Previous->Symbol.Type==SYMBOL_EXCLUSIVE_OR||Previous->Symbol.Type==SYMBOL_LESSER||Previous->Symbol.Type==SYMBOL_GREATER)){
							PUT_CHARACTER(' ');
						}
						break;
					}
					case SYMBOL_COLON:case SYMBOL_LABEL:{
						if(Previous->Type==TOKEN_SYMBOL&&Previous->Symbol.Type==SYMBOL_COLON){
							PUT_CHARACTER(' ');
						}
						break;
					}
					case SYMBOL_DOT:case SYMBOL_CONCATENATE:case SYMBOL_VARIABLE_ARGUMENTS:{
						if(Previous->Type==TOKEN_INTEGER||Previous->Type==TOKEN_FLOAT||Previous->Type==TOKEN_SYMBOL&&(Previous->Symbol.Type==SYMBOL_DOT||Previous->Symbol.Type==SYMBOL_CONCATENATE)){
							PUT_CHARACTER(' ');
						}
					}
				}
				WRITE(SymbolTokens[Printing->Symbol.Type],SymbolTokenLengths[Printing->Symbol.Type]);
			}
		}
		#undef PUT_CHARACTER
		#undef PRINT
		#undef WRITE
		Previous=Printing;
		Printing=Printing->Next;
	}
}
static Token*HandleDollars(Token*First,PreprocessorState*State,lua_State*L){
	for(;;){
		if(First->Type!=TOKEN_SYMBOL){
			if(!First->Type){
				return First;
			}
			break;
		}
		if(First->Symbol.NotNowAmount){
			--First->Symbol.NotNowAmount;
			break;
		}
		if(First->Symbol.Type!=SYMBOL_DOLLAR){
			break;
		}
		First=HandleDollar(First,State,L);
		if(State->Error.Message){
			return First;
		}
	}
	for(Token*Parsing=First->Next;Parsing->Type;){
		if(Parsing->Type!=TOKEN_SYMBOL){
			Parsing=Parsing->Next;
		}else if(Parsing->Symbol.NotNowAmount){
			--Parsing->Symbol.NotNowAmount;
			Parsing=Parsing->Next;
		}else if(Parsing->Symbol.Type!=SYMBOL_DOLLAR){
			Parsing=Parsing->Next;
		}else{
			Parsing=HandleDollar(Parsing,State,L);
			if(State->Error.Message){
				return First;
			}
		}
	}
	return First;
}
int main(int ArgumentsLength,char**Arguments){
	if(ArgumentsLength<2){
		fputs("usage: ",stdout);
		fputs(ArgumentsLength&&**Arguments?*Arguments:"unknown",stdout);
		fputs(
			" input [output]\n"
			"Available input options:\n"
			"  -e in  use the argument 'in' as input\n"
			"  -- f   use the file 'f' as input\n"
			"  -      use the standard input as input\n"
			"  f      use the file 'f' as input, if it doesn't begin with '-'\n"
			"Available output options:\n"
			"  f      use the file 'f' as output\n"
			"         or use the standout output if no output is provided\n",
			stdout
		);
		return EXIT_SUCCESS;
	}
	lua_State*L=luaL_newstate();
	if(!L){
		fputs("Error loading lua state\n",stderr);
		return EXIT_FAILURE;
	}
	lua_pushcfunction(L,LuaMain);
	if(lua_pcall(L,0,1,0)){
		fputs("Error loading lua state\n",stderr);
		lua_close(L);
		return EXIT_FAILURE;
	}
	PreprocessorState*State=lua_touserdata(L,1);
	char*Output;
	TokenList List;
	#define CHECK_ARGUMENTS_AMOUNT(Amount)\
		if(ArgumentsLength>Amount){\
			fputs("Too many options provided\n",stderr);\
			lua_close(L);\
			return EXIT_FAILURE;\
		}
	#define READ_ERROR(...)\
		perror("Error while reading input file");\
		__VA_ARGS__;\
		lua_close(L);\
		return EXIT_FAILURE
	#define READ_FILE(Input,...)\
		char*Buffer=malloc(64);\
		if(!Buffer){\
			READ_ERROR(__VA_ARGS__);\
		}\
		size_t Length=fread(Buffer,1,64,Input);\
		if(Length==64){\
			for(;;){\
				size_t Extra;\
				size_t NewLength;\
				if(Length>SIZE_MAX>>1){\
					if(Length==SIZE_MAX){\
						if(fgetc(Input)!=EOF||!feof(Input)||ferror(Input)){\
							READ_ERROR(free(Buffer);__VA_ARGS__);\
						}\
						break;\
					}\
					Extra=(NewLength=SIZE_MAX)-Length;\
				}else{\
					Extra=Length;\
					NewLength=Length<<1;\
				}\
				char*NewBuffer=realloc(Buffer,NewLength);\
				if(!NewBuffer){\
					READ_ERROR(free(Buffer);__VA_ARGS__);\
				}\
				size_t Read=fread(NewBuffer+Length,1,Extra,Input);\
				if(Read!=Extra){\
					if(ferror(Input)){\
						READ_ERROR(free(NewBuffer);__VA_ARGS__);\
					}\
					Length+=Read;\
					Buffer=NewBuffer;\
					break;\
				}\
				Length=NewLength;\
				Buffer=NewBuffer;\
			}\
		}else if(ferror(Input)){\
			READ_ERROR(free(Buffer);__VA_ARGS__);\
		}\
		List=MakeTokenList(Buffer,Length,&State->Error);\
		free(Buffer);\
		__VA_ARGS__
	char*FirstArgument=Arguments[1];
	if(*FirstArgument=='-'){
		switch(FirstArgument[1]){
			#define INVALID_OPTION\
				fputs("Invalid option '",stderr);\
				fputs(FirstArgument,stderr);\
				fputs("'\n",stderr);\
				lua_close(L);\
				return EXIT_FAILURE
			case'e':{
				if(FirstArgument[2]){
					INVALID_OPTION;
				}
				if(ArgumentsLength<3){
					fputs("Expected input after the '-e' option\n",stderr);
					lua_close(L);
					return EXIT_FAILURE;
				}
				CHECK_ARGUMENTS_AMOUNT(4);
				List=MakeTokenList(Arguments[2],strlen(Arguments[2]),&State->Error);
				Output=Arguments[3];
				break;
			}
			case'-':{
				if(FirstArgument[2]){
					INVALID_OPTION;
				}
				if(ArgumentsLength<3){
					fputs("Expected a file name after the '--' option\n",stderr);
					lua_close(L);
					return EXIT_FAILURE;
				}
				CHECK_ARGUMENTS_AMOUNT(4);
				FILE*Input=fopen(Arguments[2],"r");
				if(!Input){
					perror("Error opening input file");
					lua_close(L);
					return EXIT_FAILURE;
				}
				READ_FILE(Input,fclose(Input));
				Output=Arguments[3];
				break;
			}
			case 0:{
				CHECK_ARGUMENTS_AMOUNT(3);
				READ_FILE(stdin);
				Output=Arguments[2];
				break;
			}
			default:{
				INVALID_OPTION;
			}
			#undef INVALID_OPTION
		}
	}else{
		CHECK_ARGUMENTS_AMOUNT(3);
		FILE*Input=fopen(FirstArgument,"r");
		if(!Input){
			perror("Error opening input file");
			lua_close(L);
			return EXIT_FAILURE;
		}
		READ_FILE(Input,fclose(Input));
		Output=Arguments[2];
	}
	#undef CHECK_ARGUMENTS_AMOUNT
	#undef READ_ERROR
	#undef READ_FILE
	if(State->Error.Message){
		fputs("Error when converting to tokens:\n",stderr);
		fwrite(State->Error.Message,1,State->Error.Length,stderr);
		fputc('\n',stderr);
		lua_close(L);
		return EXIT_FAILURE;
	}
	if(List.First){
		(State->Start.Next=List.First)->Previous=&State->Start;
		(State->End.Previous=List.Last)->Next=&State->End;
		List.First=HandleDollars(List.First,State,L);
		if(State->Error.Message){
			fputs("Error during evaluation:\n",stderr);
			fwrite(State->Error.Message,1,State->Error.Length,stderr);
			fputc('\n',stderr);
			lua_close(L);
			return EXIT_FAILURE;
		}
		if(List.First){
			if(Output){
				FILE*File=fopen(Output,"w");
				if(!File){
					perror("Error opening output file");
					lua_close(L);
					return EXIT_FAILURE;
				}
				if(!PrintTokens(List.First,File)){
					if(fclose(File)){
						perror("Error closing output file");
						lua_close(L);
						return EXIT_FAILURE;
					}
					if(remove(Output)){
						perror("Error removing output file");
						lua_close(L);
						return EXIT_FAILURE;
					}
					fputs("Output file successfully removed\n",stderr);
					lua_close(L);
					return EXIT_FAILURE;
				}
				if(fclose(File)){
					perror("Error closing output file");
					lua_close(L);
					return EXIT_FAILURE;
				}
			}else{
				clearerr(stdout);
				if(!PrintTokens(List.First,stdout)){
					lua_close(L);
					return EXIT_FAILURE;
				}
			}
		}
	}
	lua_close(L);
	return EXIT_SUCCESS;
}
