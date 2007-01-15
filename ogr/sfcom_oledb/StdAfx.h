// stdafx.h : include file for standard system include files,
//      or project specific include files that are used frequently,
//      but are changed infrequently

#if !defined(AFX_STDAFX_H__C9BD5069_0D6D_11D3_94FF_00104B238935__INCLUDED_)
#define AFX_STDAFX_H__C9BD5069_0D6D_11D3_94FF_00104B238935__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define STRICT
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#define _ATL_APARTMENT_THREADED

#include <atlbase.h>
#include "cpl_error.h"

//You may derive a class from CComModule and use it if you want to override
//something, but do not change the name of _Module
extern CComModule _Module;
#include <atlcom.h>
#include <atlctl.h>

#ifdef ATL_CPL_TRACE
#ifdef ATLTRACE2
#undef ATLTRACE2
#endif

#define ATLTRACE2    CPL_ATLTrace2
#endif

void CPL_ATLTrace2( DWORD category, UINT level, const char * format, ... );

#include <atldb.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__C9BD5069_0D6D_11D3_94FF_00104B238935__INCLUDED)
