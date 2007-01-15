/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features OLE DB Implementation
 * Purpose:  Utility functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#define _WIN32_WINNT 0x0400

#include "cpl_conv.h"
#include "sftraceback.h"
#include "sfutil.h"
#include "SF.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "oledbgis.h"
#include <time.h>

#ifdef CACHE_OGRDATASOURCE

#ifndef PRESERVATION_TIME
#  define PRESERVATION_TIME      60  /* 60 seconds */
#endif

#ifndef CLEANUP_TIME
#  define CLEANUP_TIME           20  /* 20 seconds */
#endif

typedef struct _OGRDataSourceInfo
{
    char               *pszDataSourceName;
    OGRDataSource      *poDS;
    int                nRefCount;
    time_t             nLastReleaseTime;
    struct _OGRDataSourceInfo *psNext;
} OGRDataSourceInfo;

static HANDLE            hOGRDSListMutex = NULL;
static OGRDataSourceInfo *psOGRDSList = NULL;

static DWORD WINAPI SFDSCacheCleaner( LPVOID );

/************************************************************************/
/*                      SFDSCacheOpenDataSource()                       */
/*                                                                      */
/*      Return an existing open OGRDataSource for the given name, or    */
/*      open a new one.                                                 */
/************************************************************************/

OGRDataSource *SFDSCacheOpenDataSource( const char *pszDataSourceName )

{
    OGRDataSource * poDS = NULL;

/* -------------------------------------------------------------------- */
/*      Initialize the OGRDSList mutex, if it doesn't exist,            */
/*      otherwise, just acquire it.                                     */
/* -------------------------------------------------------------------- */
    if( hOGRDSListMutex == NULL )
    {
        hOGRDSListMutex = CreateMutex( NULL, TRUE, NULL );
        CreateThread( NULL, 0, SFDSCacheCleaner, NULL, 0, NULL );
    }
    else
    {
        WaitForSingleObject( hOGRDSListMutex, INFINITE );
    }

/* -------------------------------------------------------------------- */
/*      Search the list to find a copy of this datasource that is       */
/*      available.  For now, we don't allow multiple use of the data    */
/*      source.                                                         */
/* -------------------------------------------------------------------- */
    OGRDataSourceInfo *psLink;

    for( psLink = psOGRDSList; psLink != NULL; psLink = psLink->psNext )
    {
        if( !EQUAL(psLink->pszDataSourceName,pszDataSourceName) )
            continue;
        
        if( psLink->nRefCount > 0 )
        {
            CPLDebug( "OGR_OLEDB", 
                      "Found an already-in-use copy of data source\n"
                      "`%s', skipping.", 
                      pszDataSourceName );
            continue;
        }

        psLink->nRefCount++;

        poDS = psLink->poDS;
        CPLDebug( "OGR_OLEDB", "Found an existing copy of `%s'.", 
                  pszDataSourceName );
        
        ReleaseMutex( hOGRDSListMutex );
        
        return poDS;
    }

/* -------------------------------------------------------------------- */
/*      We didn't find one, so we have to try and open the              */
/*      datasource now.                                                 */
/* -------------------------------------------------------------------- */
    SFRegisterOGRFormats();

    poDS = OGRSFDriverRegistrar::Open( pszDataSourceName, FALSE );

    if( poDS == NULL )
    {
        if( strlen(CPLGetLastErrorMsg()) > 0 )
            SFReportError(E_FAIL,IID_IDBInitialize,0,
                          "%s", CPLGetLastErrorMsg() );
        else
            SFReportError(E_FAIL,IID_IDBInitialize,0,
                          "Failed to open: %s",
                          pszDataSourceName);
    }

/* -------------------------------------------------------------------- */
/*      If we opened it successfully, add it to the list of open        */
/*      datasources.                                                    */
/* -------------------------------------------------------------------- */
    if( poDS != NULL )
    {
        CPLDebug( "OGR_OLEDB", 
                  "Opened a new instance of `%s' and added to cache.", 
                  pszDataSourceName );

        psLink = (OGRDataSourceInfo *) CPLCalloc(sizeof(OGRDataSourceInfo),1);
        psLink->poDS = poDS;
        psLink->pszDataSourceName = CPLStrdup( pszDataSourceName );
        psLink->nRefCount = 1;
        psLink->nLastReleaseTime = 0;
        psLink->psNext = psOGRDSList;
        
        psOGRDSList = psLink;
    }

    ReleaseMutex( hOGRDSListMutex );
    
    return poDS;
}

/************************************************************************/
/*                          SFDSCacheCleaner()                          */
/************************************************************************/

static DWORD WINAPI SFDSCacheCleaner( LPVOID )

{
    HANDLE hTimer;
    LARGE_INTEGER  liDueTime;

    liDueTime.QuadPart = CLEANUP_TIME * -10000000;

    hTimer = CreateWaitableTimer( NULL, TRUE, NULL );

    while( TRUE )
    {
        // Setup timer timeout.
        SetWaitableTimer( hTimer, &liDueTime, 0, NULL, NULL, 0 );

        // wait for timer to expire. 
        WaitForSingleObject( hTimer, INFINITE );

        // Wait for update access to list.
        WaitForSingleObject( hOGRDSListMutex, INFINITE );

        CPLDebug( "OGR_OLEDB", "SFDSCacheCleaner() making a pass." );

        // Scan list for old datasources that are not in use.
        OGRDataSourceInfo *psLink, *psLast = NULL;
        time_t             nCurTime;

        time( &nCurTime );

        for( psLink = psOGRDSList; psLink != NULL; )
        {
            if( psLink->nRefCount == 0 
                && psLink->nLastReleaseTime < nCurTime - PRESERVATION_TIME )
            {
                CPLDebug( "OGR_OLEDB", "SFDSCacheCleaner() closing %s.", 
                          psLink->pszDataSourceName );

                // Remove this link from the linked list. 
                if( psLast != NULL )
                    psLast->psNext = psLink->psNext;
                else
                    psOGRDSList = psLink->psNext;

                // Close the datasource.
                delete psLink->poDS;
                
                CPLFree( psLink->pszDataSourceName );
                CPLFree( psLink );

                if( psLast == NULL )    
                    psLink = psOGRDSList;
                else
                    psLink = psLast->psNext;
            }
            else
            {
                psLast = psLink;
                psLink = psLink->psNext;
            }
        }

        // Release list mutex.
        ReleaseMutex( hOGRDSListMutex );
    }    
}

/************************************************************************/
/*                          SFDSCacheCleanup()                          */
/*                                                                      */
/*      Close all remaining cached datasources.  None should be         */
/*      referenced, but close them anyways if they are.                 */
/************************************************************************/

void SFDSCacheCleanup()

{
    OGRDataSourceInfo *psLink, *psNext;

    CPLDebug( "OGR_OLEDB", "SFDSCacheCleanup() called." );

    // Wait for update access to list.
    WaitForSingleObject( hOGRDSListMutex, INFINITE );

    for( psLink = psOGRDSList; psLink != NULL; psLink = psNext )
    {
        psNext = psLink->psNext;

        if( psLink->nRefCount > 0 )
            CPLDebug( "OGR_OLEDB", 
                      "SFDSCacheCleanup() - %s still referenced!", 
                      psLink->pszDataSourceName );
    
        CPLDebug( "OGR_OLEDB", 
                  "SFDSCacheCleanup() - closing %s", 
                  psLink->pszDataSourceName );
        
        // Close the datasource.
        delete psLink->poDS;
        
        CPLFree( psLink->pszDataSourceName );
        CPLFree( psLink );
    }

    psOGRDSList = NULL;

    // Release list mutex.
    ReleaseMutex( hOGRDSListMutex );

    CPLDebug( "OGR_OLEDB", "SFDSCacheCleanup() done." );
}


/************************************************************************/
/*                     SFDSCacheReleaseDataSource()                     */
/************************************************************************/

void SFDSCacheReleaseDataSource( OGRDataSource *poDS )

{
    // Wait for update access to list.
    WaitForSingleObject( hOGRDSListMutex, INFINITE );

    // Traverse list, looking for match.
    OGRDataSourceInfo *psLink;
    time_t             nCurTime;
    
    time( &nCurTime );
    
    for( psLink = psOGRDSList; psLink != NULL; psLink = psLink->psNext )
    {
        if( psLink->poDS == poDS )
        {
            psLink->nRefCount--;
            CPLAssert( psLink->nRefCount == 0 );

            psLink->nLastReleaseTime = nCurTime;
        }
    }

    // Release list mutex.
    ReleaseMutex( hOGRDSListMutex );
}

#else /* undef CACHE_OGRDATASOURCE */

/************************************************************************/
/*                      SFDSCacheOpenDataSource()                       */
/*                                                                      */
/*      Return an existing open OGRDataSource for the given name, or    */
/*      open a new one.                                                 */
/************************************************************************/

OGRDataSource *SFDSCacheOpenDataSource( const char *pszDataSourceName )

{
    OGRDataSource * poDS = NULL;

    SFRegisterOGRFormats();

    poDS = OGRSFDriverRegistrar::Open( pszDataSourceName, FALSE );

    if( poDS == NULL )
    {
        if( strlen(CPLGetLastErrorMsg()) > 0 )
            SFReportError(E_FAIL,IID_IDBInitialize,0,
                          "%s", CPLGetLastErrorMsg() );
        else
            SFReportError(E_FAIL,IID_IDBInitialize,0,
                          "Failed to open: %s",
                          pszDataSourceName);
    }

    return poDS;
}

/************************************************************************/
/*                     SFDSCacheReleaseDataSource()                     */
/************************************************************************/

void SFDSCacheReleaseDataSource( OGRDataSource *poDS )

{
    delete poDS;
}
/************************************************************************/
/*                          SFDSCacheCleanup()                          */
/************************************************************************/

void SFDSCacheCleanup()

{
}

#endif /* undef CACHE_OGRDATASOURCE */

