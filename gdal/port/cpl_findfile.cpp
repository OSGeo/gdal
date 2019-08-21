/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Generic data file location finder, with application hooking.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "cpl_conv.h"

#include <cstddef>

#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

typedef struct
{
    bool bFinderInitialized;
    int nFileFinders;
    CPLFileFinder *papfnFinders;
    char **papszFinderLocations;
} FindFileTLS;

/************************************************************************/
/*                      CPLFindFileDeinitTLS()                          */
/************************************************************************/

static void CPLPopFinderLocationInternal( FindFileTLS* pTLSData );
static CPLFileFinder CPLPopFileFinderInternal( FindFileTLS* pTLSData );

static void CPLFindFileFreeTLS( void* pData )
{
    FindFileTLS* pTLSData = reinterpret_cast<FindFileTLS *>( pData );
    if( pTLSData != nullptr && pTLSData->bFinderInitialized )
    {
        while( pTLSData->papszFinderLocations != nullptr )
            CPLPopFinderLocationInternal(pTLSData);
        while( CPLPopFileFinderInternal(pTLSData) != nullptr ) {}

        pTLSData->bFinderInitialized = false;
    }
    CPLFree(pTLSData);
}

/************************************************************************/
/*                       CPLGetFindFileTLS()                            */
/************************************************************************/

static FindFileTLS* CPLGetFindFileTLS()
{
    int bMemoryError = FALSE;
    FindFileTLS* pTLSData =
        reinterpret_cast<FindFileTLS *>(
            CPLGetTLSEx( CTLS_FINDFILE, &bMemoryError ) );
    if( bMemoryError )
        return nullptr;
    if( pTLSData == nullptr )
    {
        pTLSData = static_cast<FindFileTLS *>(
            VSI_CALLOC_VERBOSE(1, sizeof(FindFileTLS) ) );
        if( pTLSData == nullptr )
            return nullptr;
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
    if( pTLSData != nullptr && !pTLSData->bFinderInitialized )
    {
        pTLSData->bFinderInitialized = true;
        CPLPushFileFinder( CPLDefaultFindFile );

        CPLPushFinderLocation( "." );

        if( CPLGetConfigOption( "GDAL_DATA", nullptr ) != nullptr )
        {
            CPLPushFinderLocation( CPLGetConfigOption( "GDAL_DATA", nullptr ) );
        }
        else
        {
#ifdef INST_DATA
            CPLPushFinderLocation( INST_DATA );
#endif
#ifdef GDAL_PREFIX
  #ifdef MACOSX_FRAMEWORK
            CPLPushFinderLocation( GDAL_PREFIX "/Resources/gdal" );
  #else
            CPLPushFinderLocation( GDAL_PREFIX "/share/gdal" );
  #endif
#endif
        }
    }
    return pTLSData;
}

/************************************************************************/
/*                           CPLFinderClean()                           */
/************************************************************************/

/** CPLFinderClean */
void CPLFinderClean()

{
    FindFileTLS* pTLSData = CPLGetFindFileTLS();
    CPLFindFileFreeTLS(pTLSData);
    int bMemoryError = FALSE;
    CPLSetTLSWithFreeFuncEx( CTLS_FINDFILE, nullptr, nullptr, &bMemoryError );
    // TODO: if( bMemoryError ) {}
}

/************************************************************************/
/*                         CPLDefaultFindFile()                         */
/************************************************************************/

/** CPLDefaultFindFile */
const char *CPLDefaultFindFile( const char * /* pszClass */,
                                const char *pszBasename )

{
    FindFileTLS* pTLSData = CPLGetFindFileTLS();
    if( pTLSData == nullptr )
        return nullptr;
    const int nLocations = CSLCount( pTLSData->papszFinderLocations );

    for( int i = nLocations-1; i >= 0; i-- )
    {
        const char *pszResult =
            CPLFormFilename( pTLSData->papszFinderLocations[i], pszBasename,
                             nullptr );

        VSIStatBufL sStat;
        if( VSIStatL( pszResult, &sStat ) == 0 )
            return pszResult;
    }

    return nullptr;
}

/************************************************************************/
/*                            CPLFindFile()                             */
/************************************************************************/

/** CPLFindFile */
const char *CPLFindFile( const char *pszClass, const char *pszBasename )

{
    FindFileTLS* pTLSData = CPLFinderInit();
    if( pTLSData == nullptr )
        return nullptr;

    for( int i = pTLSData->nFileFinders-1; i >= 0; i-- )
    {
        const char * pszResult =
            (pTLSData->papfnFinders[i])( pszClass, pszBasename );
        if( pszResult != nullptr )
            return pszResult;
    }

    return nullptr;
}

/************************************************************************/
/*                         CPLPushFileFinder()                          */
/************************************************************************/

/** CPLPushFileFinder */
void CPLPushFileFinder( CPLFileFinder pfnFinder )

{
    FindFileTLS* pTLSData = CPLFinderInit();
    if( pTLSData == nullptr )
        return;

    pTLSData->papfnFinders = static_cast<CPLFileFinder *>(
        CPLRealloc(pTLSData->papfnFinders,
            sizeof(CPLFileFinder) * ++pTLSData->nFileFinders) );
    pTLSData->papfnFinders[pTLSData->nFileFinders-1] = pfnFinder;
}

/************************************************************************/
/*                          CPLPopFileFinder()                          */
/************************************************************************/

CPLFileFinder CPLPopFileFinderInternal( FindFileTLS* pTLSData )

{
    if( pTLSData == nullptr )
        return nullptr;
    if( pTLSData->nFileFinders == 0 )
        return nullptr;

    CPLFileFinder pfnReturn = pTLSData->papfnFinders[--pTLSData->nFileFinders];

    if( pTLSData->nFileFinders == 0)
    {
        CPLFree( pTLSData->papfnFinders );
        pTLSData->papfnFinders = nullptr;
    }

    return pfnReturn;
}

/** CPLPopFileFinder */
CPLFileFinder CPLPopFileFinder()

{
    return CPLPopFileFinderInternal(CPLFinderInit());
}

/************************************************************************/
/*                       CPLPushFinderLocation()                        */
/************************************************************************/

/** CPLPushFinderLocation */
void CPLPushFinderLocation( const char *pszLocation )

{
    FindFileTLS* pTLSData = CPLFinderInit();
    if( pTLSData == nullptr )
        return;
    // Check if location already is in list.
    if( CSLFindStringCaseSensitive(pTLSData->papszFinderLocations,
                                   pszLocation) > -1 )
        return;
    pTLSData->papszFinderLocations =
        CSLAddStringMayFail( pTLSData->papszFinderLocations, pszLocation );
}

/************************************************************************/
/*                       CPLPopFinderLocation()                         */
/************************************************************************/

static void CPLPopFinderLocationInternal( FindFileTLS* pTLSData )

{
    if( pTLSData == nullptr || pTLSData->papszFinderLocations == nullptr )
        return;

    const int nCount = CSLCount(pTLSData->papszFinderLocations);
    if( nCount == 0 )
        return;

    CPLFree( pTLSData->papszFinderLocations[nCount-1] );
    pTLSData->papszFinderLocations[nCount-1] = nullptr;

    if( nCount == 1 )
    {
        CPLFree( pTLSData->papszFinderLocations );
        pTLSData->papszFinderLocations = nullptr;
    }
}

/** CPLPopFinderLocation */
void CPLPopFinderLocation()
{
    CPLPopFinderLocationInternal(CPLFinderInit());
}
