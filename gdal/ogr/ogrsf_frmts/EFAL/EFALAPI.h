/*****************************************************************************
*       Copyright (c) 1989-2015, Pitney Bowes Software
*       All  rights reserved.
*       Confidential Property of Pitney Bowes Software
*
* $Workfile:  $
* $Revision:  $
* $Modtime:  $
*
* $Log:  $
*
* Description: EFAL
*
*****************************************************************************/

#ifndef EFALAPI_H
#define EFALAPI_H
#ifndef EFAL_STRUCTS_ONLY

#if defined(_MSC_VER)
#pragma once
#endif
#include <MIDefs.h>
#include "MIDefs.h"

#if ELLIS_OS_IS_WINOS
#include <windows.h>

#define dynlib HMODULE
#define dynlib_open(path) LoadLibraryA(path)
#define dynlib_sym(handle,symbol) GetProcAddress(handle,symbol)
#define dynlib_close(handle) FreeLibrary(handle)

#else

#include <dlfcn.h>

#define HMODULE void*
#define LoadLibrary(path) dlopen(path, RTLD_LAZY | RTLD_LOCAL)
#define GetProcAddress(handle,symbol) dlsym(handle,symbol)
#define FreeLibrary(handle) dlclose(handle)

#define dynlib void*
#define dynlib_open(path) dlopen(path, RTLD_LAZY | RTLD_LOCAL)
#define dynlib_sym(handle,symbol) dlsym(handle,symbol)
#define dynlib_close(handle) dlclose(handle)
#define dynlib_error(handle) dlerror()
#define __cdecl 

#if !defined(stricmp)
#define stricmp strcasecmp 
#endif
#if !defined(strnicmp)
#define strnicmp strncasecmp 
#endif
#if !defined(wcscpy_s)
#define wcscpy_s(dest,destsz,src) wcscpy(dest,src)
#endif
#endif


#if ELLIS_OS_ISUNIX
#define EFALFUNCTION
#define EFALCLASS   class 
#elif defined MICOMPONENT_STATIC
#define EFALFUNCTION
#define EFALCLASS   class 
#elif defined __EFALDLL__
#define EFALFUNCTION __declspec(dllexport)
#define EFALCLASS   class __declspec(dllexport)
#else
#define EFALFUNCTION __declspec(dllimport)
#define EFALCLASS   class  __declspec(dllimport)
#endif // MICOMPONENT_STATIC

typedef MI_UINT64 EFALHANDLE;
// Callback type for getting the custom EFAL string resources from client application.
typedef const wchar_t* (ResourceStringCallback)(const wchar_t* resourceStringName);
#endif

/* ***********************************************************
* Date & Time structs
* ***********************************************************
*/
typedef struct EFALDATE
{
	int year;
	int month;
	int day;
}EFALDATE;

typedef struct EFALTIME
{
	int hour;
	int minute;
	int second;
	int millisecond;
}EFALTIME;

typedef struct EFALDATETIME
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int millisecond;
}EFALDATETIME;

#endif

