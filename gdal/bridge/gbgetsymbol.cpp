/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Bridge 
 * Purpose:  Fetch a function pointer from a shared library / DLL.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 * Adapted from cplgetsymbol.cpp.
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
 ****************************************************************************/

#include <stdio.h>
#include "gdalbridge.h"

/* ==================================================================== */
/*                  Unix Implementation                                 */
/* ==================================================================== */
#if defined(__unix__) || defined(unix)

#include <dlfcn.h>

/************************************************************************/
/*                            GBGetSymbol()                             */
/*                                                                      */
/*      Note that this function doesn't:                                */
/*       o prevent the reference count on the library from going up     */
/*         for every request, or given any opportunity to unload        */
/*         the library.                                                 */
/*       o Attempt to look for the library in non-standard              */
/*         locations.                                                   */
/*       o Attempt to try variations on the symbol name, like           */
/*         pre-prending or post-pending an underscore.                  */
/************************************************************************/

void *GBGetSymbol( const char * pszLibrary, const char * pszSymbolName )

{
    void	*pLibrary;
    void	*pSymbol;

    pLibrary = dlopen(pszLibrary, RTLD_LAZY);
    if( pLibrary == NULL )
    {
        return NULL;
    }

    pSymbol = dlsym( pLibrary, pszSymbolName );

    if( pSymbol == NULL )
    {
        fprintf( stderr, "GBGetSymbol(): %s\n", dlerror() );
        return NULL;
    }
    
    return( pSymbol );
}

#endif /* def __unix__ && defined(HAVE_DLFCN_H) */

/* ==================================================================== */
/*                 Windows Implementation                               */
/* ==================================================================== */
#ifdef _WIN32

#include <windows.h>

/************************************************************************/
/*                             GBGetSymbol()                            */
/*                                                                      */
/*      Note that this function doesn't:                                */
/*       o prevent the reference count on the library from going up     */
/*         for every request, or given any opportunity to unload        */
/*         the library.                                                 */
/*       o Attempt to look for the library in non-standard              */
/*         locations.                                                   */
/*       o Attempt to try variations on the symbol name, like           */
/*         pre-prending or post-pending an underscore.                  */
/************************************************************************/

void *GBGetSymbol( const char * pszLibrary, const char * pszSymbolName )

{
    void	*pLibrary;
    void	*pSymbol;

    pLibrary = LoadLibrary(pszLibrary);
    if( pLibrary == NULL )
    {
        return NULL;
    }

    pSymbol = GetProcAddress( (HINSTANCE) pLibrary, pszSymbolName );

    if( pSymbol == NULL )
    {
        fprintf( stderr,
                 "GBGetSymbol(): Can't find requested entry point: %s\n",
                 pszSymbolName );
        return NULL;
    }
    
    return( pSymbol );
}

#endif /* def _WIN32 */

