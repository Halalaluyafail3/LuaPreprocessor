#include<time.h>
#include<stdio.h>
#include<assert.h>
#include<locale.h>
#include<stdarg.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>
#include<inttypes.h>
#include"NoLocale.h"
#define CHECK_ASCII(Character,Value)static_assert((Character)==(Value),"ASCII is required")
CHECK_ASCII('\a',7);
CHECK_ASCII('\b',8);
CHECK_ASCII('\t',9);
CHECK_ASCII('\n',10);
CHECK_ASCII('\v',11);
CHECK_ASCII('\f',12);
CHECK_ASCII('\r',13);
CHECK_ASCII(' ',32);
CHECK_ASCII('!',33);
CHECK_ASCII('"',34);
CHECK_ASCII('#',35);
CHECK_ASCII('$',36);
CHECK_ASCII('%',37);
CHECK_ASCII('&',38);
CHECK_ASCII('\'',39);
CHECK_ASCII('(',40);
CHECK_ASCII(')',41);
CHECK_ASCII('*',42);
CHECK_ASCII('+',43);
CHECK_ASCII(',',44);
CHECK_ASCII('-',45);
CHECK_ASCII('.',46);
CHECK_ASCII('/',47);
CHECK_ASCII('0',48);
CHECK_ASCII('1',49);
CHECK_ASCII('2',50);
CHECK_ASCII('3',51);
CHECK_ASCII('4',52);
CHECK_ASCII('5',53);
CHECK_ASCII('6',54);
CHECK_ASCII('7',55);
CHECK_ASCII('8',56);
CHECK_ASCII('9',57);
CHECK_ASCII(':',58);
CHECK_ASCII(';',59);
CHECK_ASCII('<',60);
CHECK_ASCII('=',61);
CHECK_ASCII('>',62);
CHECK_ASCII('?',63);
CHECK_ASCII('@',64);
CHECK_ASCII('A',65);
CHECK_ASCII('B',66);
CHECK_ASCII('C',67);
CHECK_ASCII('D',68);
CHECK_ASCII('E',69);
CHECK_ASCII('F',70);
CHECK_ASCII('G',71);
CHECK_ASCII('H',72);
CHECK_ASCII('I',73);
CHECK_ASCII('J',74);
CHECK_ASCII('K',75);
CHECK_ASCII('L',76);
CHECK_ASCII('M',77);
CHECK_ASCII('N',78);
CHECK_ASCII('O',79);
CHECK_ASCII('P',80);
CHECK_ASCII('Q',81);
CHECK_ASCII('R',82);
CHECK_ASCII('S',83);
CHECK_ASCII('T',84);
CHECK_ASCII('U',85);
CHECK_ASCII('V',86);
CHECK_ASCII('W',87);
CHECK_ASCII('X',88);
CHECK_ASCII('Y',89);
CHECK_ASCII('Z',90);
CHECK_ASCII('[',91);
CHECK_ASCII('\\',92);
CHECK_ASCII(']',93);
CHECK_ASCII('^',94);
CHECK_ASCII('_',95);
CHECK_ASCII('`',96);
CHECK_ASCII('a',97);
CHECK_ASCII('b',98);
CHECK_ASCII('c',99);
CHECK_ASCII('d',100);
CHECK_ASCII('e',101);
CHECK_ASCII('f',102);
CHECK_ASCII('g',103);
CHECK_ASCII('h',104);
CHECK_ASCII('i',105);
CHECK_ASCII('j',106);
CHECK_ASCII('k',107);
CHECK_ASCII('l',108);
CHECK_ASCII('m',109);
CHECK_ASCII('n',110);
CHECK_ASCII('o',111);
CHECK_ASCII('p',112);
CHECK_ASCII('q',113);
CHECK_ASCII('r',114);
CHECK_ASCII('s',115);
CHECK_ASCII('t',116);
CHECK_ASCII('u',117);
CHECK_ASCII('v',118);
CHECK_ASCII('w',119);
CHECK_ASCII('x',120);
CHECK_ASCII('y',121);
CHECK_ASCII('z',122);
CHECK_ASCII('{',123);
CHECK_ASCII('|',124);
CHECK_ASCII('}',125);
CHECK_ASCII('~',126);
#undef CHECK_ASCII
int CharacterToDigit(char Character){
	return(unsigned)Character^'0';
}
int CharacterToHexadecimalDigit(char Character){
	return(Character&~(unsigned)'0')-(Character>'9')*('A'-10);
}
int CharacterToOctalDigit(char Character){
	return(unsigned)Character^'0';
}
int CharacterToBinaryDigit(char Character){
	return(unsigned)Character^'0';
}
char DigitToCharacter(int Digit){
	return(unsigned)Digit|'0';
}
char HexadecimalDigitToCharacter(int Digit){
	return((unsigned)Digit|'0')+(Digit>9)*('A'-'0'-10);
}
char OctalDigitToCharacter(int Digit){
	return(unsigned)Digit|'0';
}
char BinaryDigitToCharacter(int Digit){
	return(unsigned)Digit|'0';
}
char MakeUppercase(char Character){
	return Character^(unsigned)((unsigned)Character-'a'<26)<<5;
}
char MakeLowercase(char Character){
	return Character|(unsigned)((unsigned)Character-'A'<26)<<5;
}
bool IsDigit(char Character){
	return(unsigned)Character-'0'<10;
}
bool IsHexadecimalDigit(char Character){
	return(unsigned)((unsigned)Character-'0'<10)|(Character|32U)-'a'<6;
}
bool IsOctalDigit(char Character){
	return(unsigned)Character-'0'<8;
}
bool IsBinaryDigit(char Character){
	return(unsigned)Character-'0'<2;
}
bool IsUppercase(char Character){
	return(unsigned)Character-'A'<26;
}
bool IsLowercase(char Character){
	return(unsigned)Character-'a'<26;
}
bool IsAlphabetic(char Character){
	return(Character|32U)-'a'<26;
}
bool IsAlphanumeric(char Character){
	return(unsigned)((unsigned)Character-'0'<10)|(Character|32U)-'a'<26;
}
bool IsPunctuation(char Character){
	return(unsigned)((unsigned)Character-'!'<='/'-'!')|(unsigned)Character-':'<='@'-':'|(unsigned)Character-'['<='`'-'['|(unsigned)Character-'{'<='~'-'{';
}
bool IsGraphical(char Character){
	return(unsigned)Character-'!'<94;
}
bool IsPrintable(char Character){
	return(unsigned)Character-' '<95;
}
bool IsControl(char Character){
	return(unsigned)((unsigned)Character<' ')|Character==127;
}
bool IsBlank(char Character){
	return(unsigned)(Character==' ')|Character=='\t';
}
bool IsSpace(char Character){
	return(unsigned)(Character==' ')|(unsigned)Character-'\t'<5;
}
#define LOCALE_FAIL(Name)\
	fputs("Error setting locale information in the "#Name" function\n",stderr);\
	abort()
#ifdef LC_ALL_MASK
	#define BEFORE_CALL(Name)\
		locale_t Locale=newlocale(LC_ALL_MASK,"C",0);\
		if(!Locale){\
			LOCALE_FAIL(Name);\
		}\
		locale_t Saved=uselocale(Locale);\
		if(!Saved){\
			freelocale(Locale);\
			LOCALE_FAIL(Name);\
		}
	#define AFTER_CALL(Name)\
		if(!(Locale=uselocale(Saved))){\
			if(Saved!=LC_GLOBAL_LOCALE){\
				freelocale(Saved);\
			}\
			LOCALE_FAIL(Name);\
		}\
		freelocale(Locale)
#elif defined _ENABLE_PER_THREAD_LOCALE
	#define BEFORE_CALL(Name)\
		int PerThread=_configthreadlocale(0);\
		if(PerThread==-1||_configthreadlocale(_ENABLE_PER_THREAD_LOCALE)==-1){\
			LOCALE_FAIL(Name);\
		}\
		const char*Locale=setlocale(LC_ALL,0);\
		size_t Length=strlen(Locale);\
		char SavedLocale[1024];\
		if(Length>=sizeof(SavedLocale)){\
			LOCALE_FAIL(Name);\
		}\
		memcpy(SavedLocale,Locale,Length+1);\
		if(!setlocale(LC_ALL,"C")){\
			LOCALE_FAIL(Name);\
		}
	#define AFTER_CALL(Name)\
		if(!setlocale(LC_ALL,SavedLocale)||_configthreadlocale(PerThread)==-1){\
			LOCALE_FAIL(Name);\
		}
#else
	#define BEFORE_CALL(Name)\
		const char*Locale=setlocale(LC_ALL,0);\
		size_t Length=strlen(Locale);\
		char SavedLocale[1024];\
		if(Length>=sizeof(SavedLocale)){\
			LOCALE_FAIL(Name);\
		}\
		memcpy(SavedLocale,Locale,Length+1);\
		if(!setlocale(LC_ALL,"C")){\
			LOCALE_FAIL(Name);\
		}
	#define AFTER_CALL(Name)\
		if(!setlocale(LC_ALL,SavedLocale)){\
			LOCALE_FAIL(Name);\
		}
#endif
#define SAVE_LOCALE(Returned,Name,Prototype,Expression)\
	Returned C##Name Prototype{\
		BEFORE_CALL(C##Name);\
		Returned Result=Name Expression;\
		AFTER_CALL(C##Name);\
		return Result;\
	}
#define SAVE_LOCALE_VARIADIC(Returned,Name,Prototype,Last,Expression)\
	Returned C##Name Prototype{\
		BEFORE_CALL(C##Name);\
		va_list Arguments;\
		va_start(Arguments,Last);\
		Returned Result=v##Name Expression;\
		va_end(Arguments);\
		AFTER_CALL(C##Name);\
		return Result;\
	}
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
