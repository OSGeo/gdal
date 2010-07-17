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
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

typedef struct
{
    int bFinderInitialized;
    int nFileFinders;
    CPLFileFinder *papfnFinders;
    char **papszFinderLocations;
} FindFileTLS;


/************************************************************************/
/*                      CPLFindFileDeinitTLS()                          */
/************************************************************************/

static void CPLPopFinderLocationInternal(FindFileTLS* pTLSData);
static CPLFileFinder CPLPopFileFinderInternal(FindFileTLS* pTLSData);

static void CPLFindFileFreeTLS(void* pData)
{
    FindFileTLS* pTLSData = (FindFileTLS*) pData;
    if( pTLSData->bFinderInitialized )
    {
        while( pTLSData->papszFinderLocations != NULL )
            CPLPopFinderLocationInternal(pTLSData);
        while( CPLPopFileFinderInternal(pTLSData) != NULL ) {}

        pTLSData->bFinderInitialized = FALSE;
    }
    CPLFree(pTLSData);
}

/************************************************************************/
/*                       CPLGetFindFileTLS()                            */
/************************************************************************/

static FindFileTLS* CPLGetFindFileTLS()
{
    FindFileTLS* pTLSData =
            (FindFileTLS *) CPLGetTLS( CTLS_FINDFILE );
    if (pTLSData == NULL)
    {
        pTLSData = (FindFileTLS*) CPLCalloc(1, sizeof(FindFileTLS));
        CPLSetTLSWithFreeFunc( CTLS_FINDFILE, pTLSData, CPLFindFileFreeTLS );
    }
    return pTLSData;
}

/************************************************************************/
/*                           CPLFinderInit()                            */
/************************************************************************/

static FindFileTLS* CPLFinderInit()

{
    FindFileTLS* pTLSData = CPLGetFindFileTLS();
    if( !pTLSData->bFinderInitialized )
    {
        pTLSData->bFinderInitialized = TRUE;
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
    return pTLSData;
}

/************************************************************************/
/*                           CPLFinderClean()                           */
/************************************************************************/

void CPLFinderClean()

{
    FindFileTLS* pTLSData = CPLGetFindFileTLS();
    CPLFindFileFreeTLS(pTLSData);
    CPLSetTLS( CTLS_FINDFILE, NULL, FALSE );
}

/************************************************************************/
/*                         CPLDefaultFileFind()                         */
/************************************************************************/

const char *CPLDefaultFindFile( const char *pszClass, 
                                const char *pszBasename )

{
    FindFileTLS* pTLSData = CPLGetFindFileTLS();
    int         i, nLocations = CSLCount( pTLSData->papszFinderLocations );

    (void) pszClass;

    for( i = nLocations-1; i >= 0; i-- )
    {
        const char  *pszResult;
        VSIStatBuf  sStat;

        pszResult = CPLFormFilename( pTLSData->papszFinderLocations[i], pszBasename, 
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

    FindFileTLS* pTLSData = CPLFinderInit();

    for( i = pTLSData->nFileFinders-1; i >= 0; i-- )
    {
        const char * pszResult;

        pszResult = (pTLSData->papfnFinders[i])( pszClass, pszBasename );
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
    FindFileTLS* pTLSData = CPLFinderInit();

    pTLSData->papfnFinders = (CPLFileFinder *) 
        CPLRealloc(pTLSData->papfnFinders,  sizeof(void*) * ++pTLSData->nFileFinders);
    pTLSData->papfnFinders[pTLSData->nFileFinders-1] = pfnFinder;
}

/************************************************************************/
/*                          CPLPopFileFinder()                          */
/************************************************************************/

CPLFileFinder CPLPopFileFinderInternal(FindFileTLS* pTLSData)

{
    CPLFileFinder pfnReturn;

    if( pTLSData->nFileFinders == 0 )
        return NULL;

    pfnReturn = pTLSData->papfnFinders[--pTLSData->nFileFinders];

    if( pTLSData->nFileFinders == 0)
    {
        CPLFree( pTLSData->papfnFinders );
        pTLSData->papfnFinders = NULL;
    }

    return pfnReturn;
}

CPLFileFinder CPLPopFileFinder()

{
    return CPLPopFileFinderInternal(CPLFinderInit());
}

/************************************************************************/
/*                       CPLPushFinderLocation()                        */
/************************************************************************/

void CPLPushFinderLocation( const char *pszLocation )

{
    FindFileTLS* pTLSData = CPLFinderInit();

    pTLSData->papszFinderLocations  = CSLAddString( pTLSData->papszFinderLocations, 
                                          pszLocation );
}


/************************************************************************/
/*                       CPLPopFinderLocation()                         */
/************************************************************************/

static void CPLPopFinderLocationInternal(FindFileTLS* pTLSData)

{
    int      nCount;

    if( pTLSData->papszFinderLocations == NULL )
        return;

    nCount = CSLCount(pTLSData->papszFinderLocations);
    if( nCount == 0 )
        return;

    CPLFree( pTLSData->papszFinderLocations[nCount-1] );
    pTLSData->papszFinderLocations[nCount-1] = NULL;

    if( nCount == 1 )
    {
        CPLFree( pTLSData->papszFinderLocations );
        pTLSData->papszFinderLocations = NULL;
    }
}

void CPLPopFinderLocation()
{
    CPLPopFinderLocationInternal(CPLFinderInit());
}
