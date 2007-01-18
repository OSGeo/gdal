/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Generic data file location finder, with application hooking.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static CPL_THREADLOCAL int bFinderInitialized = FALSE;
static CPL_THREADLOCAL int nFileFinders = 0;
static CPL_THREADLOCAL CPLFileFinder *papfnFinders = NULL;
static CPL_THREADLOCAL char **papszFinderLocations = NULL;

/************************************************************************/
/*                           CPLFinderInit()                            */
/************************************************************************/

static void CPLFinderInit()

{
    if( !bFinderInitialized )
    {
        bFinderInitialized = TRUE;
        CPLPushFileFinder( CPLDefaultFindFile );

        CPLPushFinderLocation( "." );

        if( CPLGetConfigOption( "GDAL_DATA", NULL ) != NULL )
        {
            CPLPushFinderLocation( CPLGetConfigOption( "GDAL_DATA", NULL ) );
        }
        else
        {
#ifdef GDAL_PREFIX
  #ifdef MACOSX_FRAMEWORK
            CPLPushFinderLocation( GDAL_PREFIX "/Resources/gdal" );
  #else
            CPLPushFinderLocation( GDAL_PREFIX "/share/gdal" );
  #endif
#else
            CPLPushFinderLocation( "/usr/local/share/gdal" );
#endif
        }
    }
}

/************************************************************************/
/*                           CPLFinderClean()                           */
/************************************************************************/

void CPLFinderClean()

{
    while( papszFinderLocations != NULL )
        CPLPopFinderLocation();
    while( CPLPopFileFinder() != NULL ) {}

    bFinderInitialized = FALSE;
}

/************************************************************************/
/*                         CPLDefaultFileFind()                         */
/************************************************************************/

const char *CPLDefaultFindFile( const char *pszClass, 
                                const char *pszBasename )

{
    int         i, nLocations = CSLCount( papszFinderLocations );

    (void) pszClass;

    for( i = nLocations-1; i >= 0; i-- )
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
    CPLFileFinder pfnReturn;

//    CPLFinderInit();

    if( nFileFinders == 0 )
        return NULL;

    pfnReturn = papfnFinders[--nFileFinders];

    if( nFileFinders == 0)
    {
        CPLFree( papfnFinders );
        papfnFinders = NULL;
    }

    return pfnReturn;
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
/*                       CPLPopFinderLocation()                         */
/************************************************************************/

void CPLPopFinderLocation()

{
    int      nCount;

    if( papszFinderLocations == NULL )
        return;

    CPLFinderInit();

    nCount = CSLCount(papszFinderLocations);
    if( nCount == 0 )
        return;

    CPLFree( papszFinderLocations[nCount-1] );
    papszFinderLocations[nCount-1] = NULL;

    if( nCount == 1 )
    {
        CPLFree( papszFinderLocations );
        papszFinderLocations = NULL;
    }
}
