/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Fetch a function pointer from a shared library / DLL.
 * Author:   Frank Warmerdam, warmerda@home.com
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

/* ==================================================================== */
/*                  Unix Implementation                                 */
/* ==================================================================== */
#if defined(HAVE_DLFCN_H)

#define GOT_GETSYMBOL

#include <dlfcn.h>

/************************************************************************/
/*                            CPLGetSymbol()                            */
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

void *CPLGetSymbol( const char * pszLibrary, const char * pszSymbolName )

{
    void	*pLibrary;
    void	*pSymbol;

    pLibrary = dlopen(pszLibrary, RTLD_LAZY);
    if( pLibrary == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", dlerror() );
        return NULL;
    }

    pSymbol = dlsym( pLibrary, pszSymbolName );

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
#ifdef _WIN32

#define GOT_GETSYMBOL

#include <windows.h>

/************************************************************************/
/*                            CPLGetSymbol()                            */
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

void *CPLGetSymbol( const char * pszLibrary, const char * pszSymbolName )

{
    void	*pLibrary;
    void	*pSymbol;

    pLibrary = LoadLibrary(pszLibrary);
    if( pLibrary == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't load requested DLL: %s", pszLibrary );
        return NULL;
    }

    pSymbol = GetProcAddress( (HINSTANCE) pLibrary, pszSymbolName );

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

void *CPLGetSymbol(const char *, const char *)

{
    return NULL;
}
#endif
