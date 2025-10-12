/* This file is licensed under the "MIT License" Copyright (c) 2023 Halalaluyafail3. See the file LICENSE or go to the following for full license details: https://github.com/Halalaluyafail3/LuaPreprocessor/blob/master/LICENSE */
#ifndef NOLOCALE_H
	#define NOLOCALE_H
	#include<time.h>
	#include<stdio.h>
	#include<stdarg.h>
	#include<stdint.h>
	#include<stdbool.h>
	size_t CountFileHeader(const char*,size_t);
	int CharacterToDigit(char);
	int CharacterToHexadecimalDigit(char);
	int CharacterToOctalDigit(char);
	int CharacterToBinaryDigit(char);
	char DigitToCharacter(int);
	char HexadecimalDigitToCharacter(int);
	char OctalDigitToCharacter(int);
	char BinaryDigitToCharacter(int);
	char MakeUppercase(char);
	char MakeLowercase(char);
	bool IsDecimalDigit(char);
	bool IsHexadecimalDigit(char);
	bool IsOctalDigit(char);
	bool IsBinaryDigit(char);
	bool IsUppercase(char);
	bool IsLowercase(char);
	bool IsAlphabetic(char);
	bool IsAlphanumeric(char);
	bool IsPunctuation(char);
	bool IsGraphical(char);
	bool IsPrintable(char);
	bool IsControl(char);
	bool IsBlank(char);
	bool IsSpace(char);
	size_t Cstrftime(char*restrict,size_t,const char*restrict,const struct tm*restrict);
	double Catof(const char*);
	int Catoi(const char*);
	long Catol(const char*);
	long long Catoll(const char*);
	float Cstrtof(const char*restrict,char**restrict);
	double Cstrtod(const char*restrict,char**restrict);
	long double Cstrtold(const char*restrict,char**restrict);
	long Cstrtol(const char*restrict,char**restrict,int);
	long long Cstrtoll(const char*restrict,char**restrict,int);
	unsigned long Cstrtoul(const char*restrict,char**restrict,int);
	unsigned long long Cstrtoull(const char*restrict,char**restrict,int);
	intmax_t Cstrtoimax(const char*restrict,char**restrict,int);
	uintmax_t Cstrtoumax(const char*restrict,char**restrict,int);
	int Cfprintf(FILE*restrict,const char*restrict,...);
	int Cprintf(const char*restrict,...);
	int Csnprintf(char*restrict,size_t,const char*restrict,...);
	int Csprintf(char*restrict,const char*restrict,...);
	int Cvfprintf(FILE*restrict,const char*restrict,va_list);
	int Cvprintf(const char*restrict,va_list);
	int Cvsnprintf(char*restrict,size_t,const char*restrict,va_list);
	int Cvsprintf(char*restrict,const char*restrict,va_list);
	int Cfscanf(FILE*restrict,const char*restrict,...);
	int Cscanf(const char*restrict,...);
	int Csscanf(char*restrict,const char*restrict,...);
	int Cvfscanf(FILE*restrict,const char*restrict,va_list);
	int Cvscanf(const char*restrict,va_list);
	int Cvsscanf(char*restrict,const char*restrict,va_list);
#endif
