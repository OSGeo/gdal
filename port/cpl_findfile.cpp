/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Generic data file location finder, with application hooking.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.2  2001/01/19 21:16:41  warmerda
 * expanded tabs
 *
 * Revision 1.1  2000/08/29 21:06:25  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "cpl_string.h"

static int bFinderInitialized = FALSE;
static int nFileFinders = 0;
static CPLFileFinder *papfnFinders = NULL;
static char **papszFinderLocations = NULL;

/************************************************************************/
/*                           CPLFinderInit()                            */
/************************************************************************/

static void CPLFinderInit()

{
    if( !bFinderInitialized )
    {
        bFinderInitialized = TRUE;
        CPLPushFileFinder( CPLDefaultFindFile );
        CPLPushFinderLocation( "/usr/local/share" );
        CPLPushFinderLocation( "." );
    }
}

/************************************************************************/
/*                         CPLDefaultFileFind()                         */
/************************************************************************/

const char *CPLDefaultFindFile( const char *pszClass, 
                                const char *pszBasename )

{
    int         i, nLocations = CSLCount( papszFinderLocations );

    (void) pszClass;

    for( i = nLocations-1; nLocations >= 0; nLocations-- )
    {
        const char  *pszResult;
        VSIStatBuf  sStat;

        pszResult = CPLFormFilename( papszFinderLocations[i], pszBasename, 
                                     NULL );

        if( VSIStat( pszResult, &sStat ) == 0 )
            return pszResult;
    }
    
    return NULL;
}

/************************************************************************/
/*                            CPLFindFile()                             */
/************************************************************************/

const char *CPLFindFile( const char *pszClass, const char *pszBasename )

{
    int         i;

    CPLFinderInit();

    for( i = nFileFinders-1; i >= 0; i-- )
    {
        const char * pszResult;

        pszResult = (papfnFinders[i])( pszClass, pszBasename );
        if( pszResult != NULL )
            return pszResult;
    }

    return NULL;
}

/************************************************************************/
/*                         CPLPushFileFinder()                          */
/************************************************************************/

void CPLPushFileFinder( CPLFileFinder pfnFinder )

{
    CPLFinderInit();

    papfnFinders = (CPLFileFinder *) 
        CPLRealloc(papfnFinders,  sizeof(void*) * ++nFileFinders);
    papfnFinders[nFileFinders-1] = pfnFinder;
}

/************************************************************************/
/*                          CPLPopFileFinder()                          */
/************************************************************************/

CPLFileFinder CPLPopFileFinder()

{
    CPLFinderInit();

    if( nFileFinders == 0 )
        return NULL;

    return papfnFinders[--nFileFinders];
}

/************************************************************************/
/*                       CPLPushFinderLocation()                        */
/************************************************************************/

void CPLPushFinderLocation( const char *pszLocation )

{
    CPLFinderInit();

    papszFinderLocations  = CSLAddString( papszFinderLocations, 
                                          pszLocation );
}


/************************************************************************/
/*                       CPLPushFinderLocation()                        */
/************************************************************************/

void CPLPopFinderLocation()

{
    int      nCount;

    CPLFinderInit();

    nCount = CSLCount(papszFinderLocations);
    if( nCount == 0 )
        return;

    CPLFree( papszFinderLocations[nCount-1] );
    papszFinderLocations[nCount-1] = NULL;
}


