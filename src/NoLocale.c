/* This file is licensed under the "MIT License" Copyright (c) 2023 Halalaluyafail3. See the file LICENSE or go to the following for full license details: https://github.com/Halalaluyafail3/LuaPreprocessor/blob/main/LICENSE */
#ifndef LUA_PREPROCESSOR_AVOID_EXTENSIONS
	/* split the if into two to avoid using the reserved name when the macro is defined */
	#ifndef _POSIX_C_SOURCE
		#define _POSIX_C_SOURCE 200809L
	#endif
#endif
#include<time.h>
#include<stdio.h>
#include<locale.h>
#include<stdarg.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>
#include<inttypes.h>
#include"NoLocale.h"
#ifndef LUA_PREPROCESSOR_AVOID_EXTENSIONS
	#ifdef LC_ALL_MASK
		#define POSIX_LOCALE_AVAILABLE 1
	#endif
	#ifdef _ENABLE_PER_THREAD_LOCALE
		#define WINDOWS_PER_THREAD_LOCALE_AVAILABLE 1
	#endif
#endif
/* each of the characters checked below should always have the same meaning regardless of shift state, position in a multibyte character, and locale */
/* otherwise the locale would need to be considered and all character comparisons would need to be done with wchar_t */
/* for example, this won't work with GB18030 or ISO-2022-JP encoded files, but will work fine with UTF-8, EUC-JP, or any single byte codepage */
/* notably, it is assumed that the characters @, $, and ` should have the same value across all locales which is only required in C23 */
enum{/* it is intended that non-ASCII is supported, though not efficiently */
	ASCII_SET=2,
	NON_ASCII_SET=1,
	CHARACTER_SET=1+(
		'\a'==7&&'\b'==8&&'\t'==9&&'\n'==10&&'\v'==11&&'\f'==12&&'\r'==13&&' '==32&&
		'0'==48&&'1'==49&&'2'==50&&'3'==51&&'4'==52&&'5'==53&&'6'==54&&'7'==55&&'8'==56&&'9'==57&&
		'A'==65&&'B'==66&&'C'==67&&'D'==68&&'E'==69&&'F'==70&&'G'==71&&'H'==72&&'I'==73&&'J'==74&&'K'==75&&'L'==76&&'M'==77&&
		'N'==78&&'O'==79&&'P'==80&&'Q'==81&&'R'==82&&'S'==83&&'T'==84&&'U'==85&&'V'==86&&'W'==87&&'X'==88&&'Y'==89&&'Z'==90&&
		'a'==97&&'b'==98&&'c'==99&&'d'==100&&'e'==101&&'f'==102&&'g'==103&&'h'==104&&'i'==105&&'j'==106&&'k'==107&&'l'==108&&'m'==109&&
		'n'==110&&'o'==111&&'p'==112&&'q'==113&&'r'==114&&'s'==115&&'t'==116&&'u'==117&&'v'==118&&'w'==119&&'x'==120&&'y'==121&&'z'==122&&
		'!'==33&&'"'==34&&'#'==35&&'$'==36&&'%'==37&&'&'==38&&'\''==39&&'('==40&&')'==41&&'*'==42&&'+'==43&&','==44&&'-'==45&&'.'==46&&'/'==47&&
		':'==58&&';'==59&&'<'==60&&'='==61&&'>'==62&&'?'==63&&'@'==64&&'['==91&&'\\'==92&&']'==93&&'^'==94&&'_'==95&&'`'==96&&'{'==123&&'|'==124&&'}'==125&&'~'==126
	)
};
/* generic selection used to select the expression depending upon the character set; however, both expressions must be valid in either case due to how _Generic works */
#define IF_ASCII(IsASCII,IsntASCII)_Generic((int(*)[CHARACTER_SET])0,int(*)[ASCII_SET]:IsASCII,int(*)[NON_ASCII_SET]:IsntASCII)
size_t CountFileHeader(const char*Buffer,size_t Length){/* UTF-8 BOM */
	return IF_ASCII(Length<3?0:((unsigned)((unsigned char)Buffer[0]==239)&(unsigned char)Buffer[1]==187&(unsigned char)Buffer[2]==191)*3,0);
}
int CharacterToDigit(char Character){/* the digit conversion functions assume valid inputs */
	return IF_ASCII((unsigned)Character^'0',(unsigned char)Character-(unsigned char)'0');
}
int CharacterToHexadecimalDigit(char Character){/* the code for optimizing ASCII is awkward but intended to be maximally efficient */
	return IF_ASCII((Character&~(unsigned)'0')-(Character>'9')*('A'-10),
		(unsigned char)Character>=(unsigned char)'0'&&(unsigned char)Character<=(unsigned char)'9'?(unsigned char)Character-(unsigned char)'0':
		Character=='a'||Character=='A'?10:Character=='b'||Character=='B'?11:Character=='c'||Character=='C'?12:
		Character=='d'||Character=='D'?13:Character=='e'||Character=='E'?14:15
	);
}
int CharacterToOctalDigit(char Character){
	return IF_ASCII((unsigned)Character^'0',(unsigned char)Character-(unsigned char)'0');
}
int CharacterToBinaryDigit(char Character){
	return IF_ASCII((unsigned)Character^'0',(unsigned char)Character-(unsigned char)'0');
}
char DigitToCharacter(int Digit){
	return IF_ASCII((unsigned)Digit|'0',Digit+(unsigned char)'0');
}
char HexadecimalDigitToCharacter(int Digit){
	return IF_ASCII(((unsigned)Digit|'0')+(Digit>9)*('A'-'0'-10),
		Digit<10?Digit+(unsigned char)'0':
		Digit==10?'A':Digit==11?'B':Digit==12?'C':Digit==13?'D':Digit==14?'E':'F'
	);
}
char OctalDigitToCharacter(int Digit){
	return IF_ASCII((unsigned)Digit|'0',Digit+(unsigned char)'0');
}
char BinaryDigitToCharacter(int Digit){
	return IF_ASCII((unsigned)Digit|'0',Digit+(unsigned char)'0');
}
char MakeUppercase(char Character){
	return IF_ASCII(Character^(unsigned)((unsigned)Character-'a'<26)<<5,
		Character=='a'?'A':Character=='b'?'B':Character=='c'?'C':Character=='d'?'D':Character=='e'?'E':Character=='f'?'F':
		Character=='g'?'G':Character=='h'?'H':Character=='i'?'I':Character=='j'?'J':Character=='k'?'K':Character=='l'?'L':
		Character=='m'?'M':Character=='n'?'N':Character=='o'?'O':Character=='p'?'P':Character=='q'?'Q':Character=='r'?'R':
		Character=='s'?'S':Character=='t'?'T':Character=='u'?'U':Character=='v'?'V':Character=='w'?'W':Character=='x'?'X':
		Character=='y'?'Y':Character=='z'?'Z':Character
	);
}
char MakeLowercase(char Character){
	return IF_ASCII(Character|(unsigned)((unsigned)Character-'A'<26)<<5,
		Character=='A'?'a':Character=='B'?'b':Character=='C'?'c':Character=='D'?'d':Character=='E'?'e':Character=='F'?'f':
		Character=='G'?'g':Character=='H'?'h':Character=='I'?'i':Character=='J'?'j':Character=='K'?'k':Character=='L'?'l':
		Character=='M'?'m':Character=='N'?'n':Character=='O'?'o':Character=='P'?'p':Character=='Q'?'q':Character=='R'?'r':
		Character=='S'?'s':Character=='T'?'t':Character=='U'?'u':Character=='V'?'v':Character=='W'?'w':Character=='X'?'x':
		Character=='Y'?'y':Character=='Z'?'z':Character
	);
}
bool IsDecimalDigit(char Character){/* contiguous 0-9 is guaranteed */
	return IF_ASCII((unsigned)Character-'0'<10,(unsigned)(unsigned char)Character-(unsigned char)'0'<10);
}
bool IsHexadecimalDigit(char Character){
	return IF_ASCII((unsigned)((unsigned)Character-'0'<10)|(Character|32U)-'a'<6,
		IsDecimalDigit(Character)||Character=='a'||Character=='A'||Character=='b'||Character=='B'||
		Character=='c'||Character=='C'||Character=='d'||Character=='D'||Character=='e'||Character=='E'||Character=='f'||Character=='F'
	);
}
bool IsOctalDigit(char Character){
	return IF_ASCII((unsigned)Character-'0'<8,(unsigned)(unsigned char)Character-(unsigned char)'0'<8);
}
bool IsBinaryDigit(char Character){
	return IF_ASCII((unsigned)Character-'0'<2,(unsigned)(unsigned char)Character-(unsigned char)'0'<2);
}
bool IsUppercase(char Character){
	return IF_ASCII((unsigned)Character-'A'<26,
		Character=='A'||Character=='B'||Character=='C'||Character=='D'||Character=='E'||Character=='F'||Character=='G'||
		Character=='H'||Character=='I'||Character=='J'||Character=='K'||Character=='L'||Character=='M'||Character=='N'||
		Character=='O'||Character=='P'||Character=='Q'||Character=='R'||Character=='S'||Character=='T'||Character=='U'||
		Character=='V'||Character=='W'||Character=='X'||Character=='Y'||Character=='Z'
	);
}
bool IsLowercase(char Character){
	return IF_ASCII((unsigned)Character-'a'<26,
		Character=='a'||Character=='b'||Character=='c'||Character=='d'||Character=='e'||Character=='f'||Character=='g'||
		Character=='h'||Character=='i'||Character=='j'||Character=='k'||Character=='l'||Character=='m'||Character=='n'||
		Character=='o'||Character=='p'||Character=='q'||Character=='r'||Character=='s'||Character=='t'||Character=='u'||
		Character=='v'||Character=='w'||Character=='x'||Character=='y'||Character=='z'
	);
}
bool IsAlphabetic(char Character){
	return IF_ASCII((Character|32U)-'a'<26,IsUppercase(Character)||IsLowercase(Character));
}
bool IsAlphanumeric(char Character){
	return IF_ASCII((unsigned)((unsigned)Character-'0'<10)|(Character|32U)-'a'<26,IsDecimalDigit(Character)||IsUppercase(Character)||IsLowercase(Character));
}
bool IsPunctuation(char Character){
	return IF_ASCII(
		(unsigned)((unsigned)Character-'!'<='/'-'!')|(unsigned)Character-':'<='@'-':'|(unsigned)Character-'['<='`'-'['|(unsigned)Character-'{'<='~'-'{',
		Character=='!'||Character=='"'||Character=='#'||Character=='$'||Character=='%'||Character=='&'||Character=='\''||Character=='('||
		Character==')'||Character=='*'||Character=='+'||Character==','||Character=='-'||Character=='.'||Character=='/'||Character==':'||
		Character==';'||Character=='<'||Character=='='||Character=='>'||Character=='?'||Character=='@'||Character=='['||Character=='\\'||
		Character==']'||Character=='^'||Character=='_'||Character=='`'||Character=='{'||Character=='|'||Character=='}'||Character=='~'
	);
}
bool IsGraphical(char Character){
	return IF_ASCII((unsigned)Character-'!'<94,IsAlphanumeric(Character)||IsPunctuation(Character));
}
bool IsPrintable(char Character){
	return IF_ASCII((unsigned)Character-' '<95,IsGraphical(Character)||Character==' ');
}
bool IsControl(char Character){/* all non-standard characters are considered control characters (they're locale dependent and this can cause problems) */
	return IF_ASCII((unsigned)Character-' '>94,!IsPrintable(Character));
}
bool IsBlank(char Character){
	return(unsigned)(Character==' ')|Character=='\t';
}
bool IsSpace(char Character){
	return IF_ASCII((unsigned)(Character==' ')|(unsigned)Character-'\t'<5,Character==' '||Character=='\t'||Character=='\n'||Character=='\v'||Character=='\f'||Character=='\v');
}
#define LOCALE_FAIL(Name)\
	fputs("Error setting locale information in the C"#Name" function\n",stderr);\
	abort()
/* try to be thread safe, or fall back to standard setlocale */
#ifdef POSIX_LOCALE_AVAILABLE
	#define SAVE_LOCALE(Returned,Name,Prototype,Expression)\
		Returned C##Name Prototype{\
			locale_t Locale=newlocale(LC_ALL_MASK,"C",0);\
			if(!Locale){\
				LOCALE_FAIL(Name);\
			}\
			locale_t Saved=uselocale(Locale);\
			if(!Saved){\
				freelocale(Locale);\
				LOCALE_FAIL(Name);\
			}\
			Returned Result=Name Expression;\
			if(!(Locale=uselocale(Saved))){\
				if(Saved!=LC_GLOBAL_LOCALE){\
					freelocale(Saved);\
				}\
				LOCALE_FAIL(Name);\
			}\
			freelocale(Locale);\
			return Result;\
		}
	#define SAVE_LOCALE_VARIADIC(Returned,Name,Prototype,Last,Expression)\
		Returned C##Name Prototype{\
			locale_t Locale=newlocale(LC_ALL_MASK,"C",0);\
			if(!Locale){\
				LOCALE_FAIL(Name);\
			}\
			locale_t Saved=uselocale(Locale);\
			if(!Saved){\
				freelocale(Locale);\
				LOCALE_FAIL(Name);\
			}\
			va_list Arguments;\
			va_start(Arguments,Last);\
			Returned Result=v##Name Expression;\
			va_end(Arguments);\
			if(!(Locale=uselocale(Saved))){\
				if(Saved!=LC_GLOBAL_LOCALE){\
					freelocale(Saved);\
				}\
				LOCALE_FAIL(Name);\
			}\
			freelocale(Locale);\
			return Result;\
		}
#elif defined WINDOWS_PER_THREAD_LOCALE_AVAILABLE
	#define SAVE_LOCALE(Returned,Name,Prototype,Expression)\
		Returned C##Name Prototype{\
			int PerThread=_configthreadlocale(_ENABLE_PER_THREAD_LOCALE);\
			if(PerThread==-1){\
				LOCALE_FAIL(Name);\
			}\
			if(PerThread==_DISABLE_PER_THREAD_LOCALE){\
				if(!setlocale(LC_ALL,"C")){\
					LOCALE_FAIL(Name);\
				}\
				Returned Result=Name Expression;\
				if(_configthreadlocale(_DISABLE_PER_THREAD_LOCALE)==-1){\
					LOCALE_FAIL(Name);\
				}\
				return Result;\
			}\
			const char*Locale=setlocale(LC_ALL,0);\
			size_t Length=strlen(Locale)+1;\
			if(Length>1024){\
				char*SavedLocale=malloc(Length);\
				if(!SavedLocale){\
					LOCALE_FAIL(Name);\
				}\
				memcpy(SavedLocale,Locale,Length);\
				if(!setlocale(LC_ALL,"C")){\
					free(SavedLocale);\
					LOCALE_FAIL(Name);\
				}\
				Returned Result=Name Expression;\
				if(!setlocale(LC_ALL,SavedLocale)){\
					free(SavedLocale);\
					LOCALE_FAIL(Name);\
				}\
				free(SavedLocale);\
				return Result;\
			}\
			char SavedLocale[1024];\
			memcpy(SavedLocale,Locale,Length);\
			if(!setlocale(LC_ALL,"C")){\
				LOCALE_FAIL(Name);\
			}\
			Returned Result=Name Expression;\
			if(!setlocale(LC_ALL,SavedLocale)){\
				LOCALE_FAIL(Name);\
			}\
			return Result;\
		}
	#define SAVE_LOCALE_VARIADIC(Returned,Name,Prototype,Last,Expression)\
		Returned C##Name Prototype{\
			int PerThread=_configthreadlocale(_ENABLE_PER_THREAD_LOCALE);\
			if(PerThread==-1){\
				LOCALE_FAIL(Name);\
			}\
			if(PerThread==_DISABLE_PER_THREAD_LOCALE){\
				if(!setlocale(LC_ALL,"C")){\
					LOCALE_FAIL(Name);\
				}\
				va_list Arguments;\
				va_start(Arguments,Last);\
				Returned Result=v##Name Expression;\
				va_end(Arguments);\
				if(_configthreadlocale(_DISABLE_PER_THREAD_LOCALE)==-1){\
					LOCALE_FAIL(Name);\
				}\
				return Result;\
			}\
			const char*Locale=setlocale(LC_ALL,0);\
			size_t Length=strlen(Locale)+1;\
			if(Length>1024){\
				char*SavedLocale=malloc(Length);\
				if(!SavedLocale){\
					LOCALE_FAIL(Name);\
				}\
				memcpy(SavedLocale,Locale,Length);\
				if(!setlocale(LC_ALL,"C")){\
					free(SavedLocale);\
					LOCALE_FAIL(Name);\
				}\
				va_list Arguments;\
				va_start(Arguments,Last);\
				Returned Result=v##Name Expression;\
				va_end(Arguments);\
				if(!setlocale(LC_ALL,SavedLocale)){\
					free(SavedLocale);\
					LOCALE_FAIL(Name);\
				}\
				free(SavedLocale);\
				return Result;\
			}\
			char SavedLocale[1024];\
			memcpy(SavedLocale,Locale,Length);\
			if(!setlocale(LC_ALL,"C")){\
				LOCALE_FAIL(Name);\
			}\
			va_list Arguments;\
			va_start(Arguments,Last);\
			Returned Result=v##Name Expression;\
			va_end(Arguments);\
			if(!setlocale(LC_ALL,SavedLocale)){\
				LOCALE_FAIL(Name);\
			}\
			return Result;\
		}
#else
	#define SAVE_LOCALE(Returned,Name,Prototype,Expression)\
		Returned C##Name Prototype{\
			const char*Locale=setlocale(LC_ALL,0);\
			size_t Length=strlen(Locale)+1;\
			if(Length>1024){\
				char*SavedLocale=malloc(Length);\
				if(!SavedLocale){\
					LOCALE_FAIL(Name);\
				}\
				memcpy(SavedLocale,Locale,Length);\
				if(!setlocale(LC_ALL,"C")){\
					free(SavedLocale);\
					LOCALE_FAIL(Name);\
				}\
				Returned Result=Name Expression;\
				if(!setlocale(LC_ALL,SavedLocale)){\
					free(SavedLocale);\
					LOCALE_FAIL(Name);\
				}\
				free(SavedLocale);\
				return Result;\
			}\
			char SavedLocale[1024];\
			memcpy(SavedLocale,Locale,Length);\
			if(!setlocale(LC_ALL,"C")){\
				LOCALE_FAIL(Name);\
			}\
			Returned Result=Name Expression;\
			if(!setlocale(LC_ALL,SavedLocale)){\
				LOCALE_FAIL(Name);\
			}\
			return Result;\
		}
	#define SAVE_LOCALE_VARIADIC(Returned,Name,Prototype,Last,Expression)\
		Returned C##Name Prototype{\
			const char*Locale=setlocale(LC_ALL,0);\
			size_t Length=strlen(Locale)+1;\
			if(Length>1024){\
				char*SavedLocale=malloc(Length);\
				if(!SavedLocale){\
					LOCALE_FAIL(Name);\
				}\
				memcpy(SavedLocale,Locale,Length);\
				if(!setlocale(LC_ALL,"C")){\
					free(SavedLocale);\
					LOCALE_FAIL(Name);\
				}\
				va_list Arguments;\
				va_start(Arguments,Last);\
				Returned Result=v##Name Expression;\
				va_end(Arguments);\
				if(!setlocale(LC_ALL,SavedLocale)){\
					free(SavedLocale);\
					LOCALE_FAIL(Name);\
				}\
				free(SavedLocale);\
				return Result;\
			}\
			char SavedLocale[1024];\
			memcpy(SavedLocale,Locale,Length);\
			if(!setlocale(LC_ALL,"C")){\
				LOCALE_FAIL(Name);\
			}\
			va_list Arguments;\
			va_start(Arguments,Last);\
			Returned Result=v##Name Expression;\
			va_end(Arguments);\
			if(!setlocale(LC_ALL,SavedLocale)){\
				LOCALE_FAIL(Name);\
			}\
			return Result;\
		}
#endif
SAVE_LOCALE(size_t,strftime,(char*restrict String,size_t Max,const char*restrict Format,const struct tm*restrict Tm),(String,Max,Format,Tm))
SAVE_LOCALE(double,atof,(const char*String),(String))
SAVE_LOCALE(int,atoi,(const char*String),(String))
SAVE_LOCALE(long,atol,(const char*String),(String))
SAVE_LOCALE(long long,atoll,(const char*String),(String))
SAVE_LOCALE(float,strtof,(const char*restrict String,char**restrict EndPointer),(String,EndPointer))
SAVE_LOCALE(double,strtod,(const char*restrict String,char**restrict EndPointer),(String,EndPointer))
SAVE_LOCALE(long double,strtold,(const char*restrict String,char**restrict EndPointer),(String,EndPointer))
SAVE_LOCALE(long,strtol,(const char*restrict String,char**restrict EndPointer,int Radix),(String,EndPointer,Radix))
SAVE_LOCALE(long long,strtoll,(const char*restrict String,char**restrict EndPointer,int Radix),(String,EndPointer,Radix))
SAVE_LOCALE(unsigned long,strtoul,(const char*restrict String,char**restrict EndPointer,int Radix),(String,EndPointer,Radix))
SAVE_LOCALE(unsigned long long,strtoull,(const char*restrict String,char**restrict EndPointer,int Radix),(String,EndPointer,Radix))
SAVE_LOCALE(intmax_t,strtoimax,(const char*restrict String,char**restrict EndPointer,int Radix),(String,EndPointer,Radix))
SAVE_LOCALE(uintmax_t,strtoumax,(const char*restrict String,char**restrict EndPointer,int Radix),(String,EndPointer,Radix))
SAVE_LOCALE_VARIADIC(int,fprintf,(FILE*restrict File,const char*restrict Format,...),Format,(File,Format,Arguments))
SAVE_LOCALE_VARIADIC(int,printf,(const char*restrict Format,...),Format,(Format,Arguments))
SAVE_LOCALE_VARIADIC(int,snprintf,(char*restrict String,size_t Max,const char*restrict Format,...),Format,(String,Max,Format,Arguments))
SAVE_LOCALE_VARIADIC(int,sprintf,(char*restrict String,const char*restrict Format,...),Format,(String,Format,Arguments))
SAVE_LOCALE(int,vfprintf,(FILE*restrict File,const char*restrict Format,va_list Arguments),(File,Format,Arguments))
SAVE_LOCALE(int,vprintf,(const char*restrict Format,va_list Arguments),(Format,Arguments))
SAVE_LOCALE(int,vsnprintf,(char*restrict String,size_t Max,const char*restrict Format,va_list Arguments),(String,Max,Format,Arguments))
SAVE_LOCALE(int,vsprintf,(char*restrict String,const char*restrict Format,va_list Arguments),(String,Format,Arguments))
SAVE_LOCALE_VARIADIC(int,fscanf,(FILE*restrict File,const char*restrict Format,...),Format,(File,Format,Arguments))
SAVE_LOCALE_VARIADIC(int,scanf,(const char*restrict Format,...),Format,(Format,Arguments))
SAVE_LOCALE_VARIADIC(int,sscanf,(char*restrict String,const char*restrict Format,...),Format,(String,Format,Arguments))
SAVE_LOCALE(int,vfscanf,(FILE*restrict File,const char*restrict Format,va_list Arguments),(File,Format,Arguments))
SAVE_LOCALE(int,vscanf,(const char*restrict Format,va_list Arguments),(Format,Arguments))
SAVE_LOCALE(int,vsscanf,(char*restrict String,const char*restrict Format,va_list Arguments),(String,Format,Arguments))
