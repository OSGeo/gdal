/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/

#ifndef EFALAPI_H
#define EFALAPI_H
#ifndef EFAL_STRUCTS_ONLY

#if defined(_MSC_VER)
#pragma once
#endif
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

