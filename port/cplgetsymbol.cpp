/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Fetch a function pointer from a shared library / DLL.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.16  2006/03/21 20:11:54  fwarmerdam
 * fixup headers a bit
 *
 * Revision 1.15  2006/02/19 21:54:34  mloskot
 * [WINCE] Changes related to Windows CE port of CPL. Most changes are #ifdef wrappers.
 *
 * Revision 1.14  2004/11/11 20:40:38  fwarmerdam
 * Check for variant of symbol with a leading underscore on Mach-O/Apple
 * systems.  Patch by John Hayes / Lizardtech.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=668
 *
 * Revision 1.13  2003/08/26 01:08:04  warmerda
 * Cast return result of GetProcAddress() for use with MingW.
 *
 * Revision 1.12  2002/11/20 17:16:48  warmerda
 * Added debug report from dummy CPLGetSymbol().
 *
 * Revision 1.11  2001/07/18 04:00:49  warmerda
 * added CPL_CVSID
 *
 * Revision 1.10  2001/01/19 21:16:41  warmerda
 * expanded tabs
 *
 * Revision 1.9  2000/09/25 19:59:03  warmerda
 * look for WIN32 not _WIN32
 *
 * Revision 1.8  1999/05/20 02:54:38  warmerda
 * Added API documentation
 *
 * Revision 1.7  1999/04/23 13:56:36  warmerda
 * added stub implementation.  Don't check for __unix__
 *
 * Revision 1.6  1999/04/21 20:06:05  warmerda
 * Removed incorrect comment.
 *
 * Revision 1.5  1999/03/02 21:20:00  warmerda
 * test for dlfcn.h, not -ldl
 *
 * Revision 1.4  1999/03/02 21:08:11  warmerda
 * autoconf switch
 *
 * Revision 1.3  1999/01/28 18:35:44  warmerda
 * minor windows cleanup.
 *
 * Revision 1.2  1999/01/27 20:16:03  warmerda
 * Added windows implementation.
 *
 * Revision 1.1  1999/01/11 15:34:57  warmerda
 * New
 *
 */

#include "cpl_conv.h"

CPL_CVSID("$Id$");

/* ==================================================================== */
/*                  Unix Implementation                                 */
/* ==================================================================== */
#if defined(HAVE_DLFCN_H)

#define GOT_GETSYMBOL

#include <dlfcn.h>

/************************************************************************/
/*                            CPLGetSymbol()                            */
/************************************************************************/

/**
 * Fetch a function pointer from a shared library / DLL.
 *
 * This function is meant to abstract access to shared libraries and
 * DLLs and performs functions similar to dlopen()/dlsym() on Unix and
 * LoadLibrary() / GetProcAddress() on Windows.
 *
 * If no support for loading entry points from a shared library is available
 * this function will always return NULL.   Rules on when this function
 * issues a CPLError() or not are not currently well defined, and will have
 * to be resolved in the future.
 *
 * Currently CPLGetSymbol() doesn't try to:
 * <ul>
 *  <li> prevent the reference count on the library from going up
 *    for every request, or given any opportunity to unload      
 *    the library.                                            
 *  <li> Attempt to look for the library in non-standard         
 *    locations.                                              
 *  <li> Attempt to try variations on the symbol name, like      
 *    pre-prending or post-pending an underscore.
 * </ul>
 * 
 * Some of these issues may be worked on in the future.
 *
 * @param pszLibrary the name of the shared library or DLL containing
 * the function.  May contain path to file.  If not system supplies search
 * paths will be used.
 * @param pszSymbolName the name of the function to fetch a pointer to.
 * @return A pointer to the function if found, or NULL if the function isn't
 * found, or the shared library can't be loaded.
 */

void *CPLGetSymbol( const char * pszLibrary, const char * pszSymbolName )

{
    void        *pLibrary;
    void        *pSymbol;

    pLibrary = dlopen(pszLibrary, RTLD_LAZY);
    if( pLibrary == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", dlerror() );
        return NULL;
    }

    pSymbol = dlsym( pLibrary, pszSymbolName );

#if (defined(__APPLE__) && defined(__MACH__))
    /* On mach-o systems, C symbols have a leading underscore and depending
     * on how dlcompat is configured it may or may not add the leading
     * underscore.  So if dlsym() fails add an underscore and try again.
     */
    if( pSymbol == NULL )
    {
        char withUnder[strlen(pszSymbolName) + 2];
        withUnder[0] = '_'; withUnder[1] = 0;
        strcat(withUnder, pszSymbolName);
        pSymbol = dlsym( pLibrary, withUnder );
    }
#endif

    if( pSymbol == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", dlerror() );
        return NULL;
    }
    
    return( pSymbol );
}

#endif /* def __unix__ && defined(HAVE_DLFCN_H) */

/* ==================================================================== */
/*                 Windows Implementation                               */
/* ==================================================================== */
#ifdef WIN32

#define GOT_GETSYMBOL

#if defined(WIN32CE)
#  include "cpl_win32ce_api.h"
#else
#  include <windows.h>
#endif

/************************************************************************/
/*                            CPLGetSymbol()                            */
/************************************************************************/

void *CPLGetSymbol( const char * pszLibrary, const char * pszSymbolName )

{
    void        *pLibrary;
    void        *pSymbol;

    pLibrary = LoadLibrary(pszLibrary);
    if( pLibrary == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't load requested DLL: %s", pszLibrary );
        return NULL;
    }

    pSymbol = (void *) GetProcAddress( (HINSTANCE) pLibrary, pszSymbolName );

    if( pSymbol == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't find requested entry point: %s\n", pszSymbolName );
        return NULL;
    }
    
    return( pSymbol );
}

#endif /* def _WIN32 */


/* ==================================================================== */
/*      Dummy implementation.                                           */
/* ==================================================================== */

#ifndef GOT_GETSYMBOL

/************************************************************************/
/*                            CPLGetSymbol()                            */
/*                                                                      */
/*      Dummy implementation.                                           */
/************************************************************************/

void *CPLGetSymbol(const char *pszLibrary, const char *pszEntryPoint)

{
    CPLDebug( "CPL", 
              "CPLGetSymbol(%s,%s) called.  Failed as this is stub"
              " implementation.", pszLibrary, pszEntryPoint );
    return NULL;
}
#endif
