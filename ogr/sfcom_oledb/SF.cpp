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

/************************************************************************/
/*                       CPLDefaultErrorHandler()                       */
/************************************************************************/

static void OGR_OLEDBErrorHandler( CPLErr eErrClass, int nError, 
                                   const char * pszErrorMsg )

{
    static int       bLogInit = FALSE;
    static FILE *    fpLog = stderr;

    if( !bLogInit )
    {
        const char *cpl_log = NULL;

        bLogInit = TRUE;

        if( getenv("CPL_LOG") != NULL )
            cpl_log = getenv("CPL_LOG");

        fpLog = stderr;
        if( cpl_log != NULL && EQUAL(cpl_log,"OFF") )
        {
            fpLog = NULL;
        }
        else if( cpl_log != NULL )
        {
            char      path[5000];
            int       i = 0;

            strcpy( path, cpl_log );

            while( (fpLog = fopen( path, "rt" )) != NULL ) 
            {
                fclose( fpLog );

                sprintf( path, "%s_%d", cpl_log, i++ );
            }

            fpLog = fopen( path, "wt" );
        }
    }

    if( fpLog == NULL )
        return;

    if( eErrClass == CE_Debug )
        fprintf( fpLog, "%s\n", pszErrorMsg );
    else if( eErrClass == CE_Warning )
        fprintf( fpLog, "Warning %d: %s\n", nError, pszErrorMsg );
    else
        fprintf( fpLog, "ERROR %d: %s\n", nError, pszErrorMsg );

    fflush( fpLog );
}

/////////////////////////////////////////////////////////////////////////////
// DLL Entry Point

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        CPLSetErrorHandler( OGR_OLEDBErrorHandler );
        CPLDebug( "OGR_OLEDB", "DllMain" );
        _Module.Init(ObjectMap, hInstance, &LIBID_SFLib);
        DisableThreadLibraryCalls(hInstance);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
        _Module.Term();

    return TRUE;    // ok
}

/////////////////////////////////////////////////////////////////////////////
// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
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

