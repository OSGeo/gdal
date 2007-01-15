/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of DLL Exports.
 * Author:   Ken Shih, kshih@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Les Technologies SoftMap Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.9  2002/05/08 20:27:48  warmerda
 * added support for caching OGRDataSources
 *
 * Revision 1.8  2002/01/13 01:41:21  warmerda
 * included date in startup debug
 *
 * Revision 1.7  2001/11/09 19:05:34  warmerda
 * added debuggin
 *
 * Revision 1.6  2001/11/01 16:46:15  warmerda
 * use default CPLLoggingErrorHandler now
 *
 * Revision 1.5  2001/10/22 21:28:25  warmerda
 * updated logging rules
 *
 * Revision 1.4  2001/05/28 19:34:41  warmerda
 * override error handler
 *
 * Revision 1.3  2000/01/31 16:26:21  warmerda
 * added header
 *
 */

#include "stdafx.h"
#include "resource.h"
#include <initguid.h>
#include "SF.h"

#include "SF_i.c"
#include "SFSess.h"
#include "SFDS.h"

CComModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
OBJECT_ENTRY(CLSID_SF, CSFSource)
END_OBJECT_MAP()

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        CPLSetErrorHandler( CPLLoggingErrorHandler );
        CPLDebug( "OGR_OLEDB", "DllMain: " __DATE__ );
        _Module.Init(ObjectMap, hInstance, &LIBID_SFLib);
        DisableThreadLibraryCalls(hInstance);
        CPLDebug( "OGR_OLEDB", "DllMain complete." );
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        CPLDebug( "OGR_OLEDB", "DllMain() - DLL_PROCESS_DETACH" );
        
        SFDSCacheCleanup();

        _Module.Term();
    }

    return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
    CPLDebug( "OGR_OLEDB", "DllCanUnloadNow() - lockcount = %d", 
              _Module.GetLockCount() );

    return (_Module.GetLockCount()==0) ? S_OK : S_FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// Returns a class factory to create an object of the requested type

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return _Module.GetClassObject(rclsid, riid, ppv);
}

/////////////////////////////////////////////////////////////////////////////
// DllRegisterServer - Adds entries to the system registry

STDAPI DllRegisterServer(void)
{
    // registers object, typelib and all interfaces in typelib
    return _Module.RegisterServer(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// DllUnregisterServer - Removes entries from the system registry

STDAPI DllUnregisterServer(void)
{
    return _Module.UnregisterServer(TRUE);
}

