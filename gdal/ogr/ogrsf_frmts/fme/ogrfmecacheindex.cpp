/******************************************************************************
 *
 * Project:  FMEObjects Translator
 * Purpose:  Implement the OGRFMECacheIndex class, a mechanism to manage a
 *           persistent index list of cached datasets.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002 Safe Software Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "fme2ogr.h"
#include "cpl_multiproc.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include <time.h>

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRFMECacheIndex()                          */
/************************************************************************/

OGRFMECacheIndex::OGRFMECacheIndex( const char * pszPathIn )

{
    psTree = NULL;
    pszPath = CPLStrdup( pszPathIn );
    hLock = NULL;
}

/************************************************************************/
/*                         ~OGRFMECacheIndex()                          */
/************************************************************************/

OGRFMECacheIndex::~OGRFMECacheIndex()

{
    if( psTree != NULL )
    {
        Unlock();
        CPLDestroyXMLNode( psTree );
        psTree = NULL;
    }
    CPLFree( pszPath );
}

/************************************************************************/
/*                                Lock()                                */
/************************************************************************/

int OGRFMECacheIndex::Lock()

{
    if( pszPath == NULL )
        return FALSE;

    hLock = CPLLockFile( pszPath, 5.0 );

    return hLock != NULL;
}

/************************************************************************/
/*                               Unlock()                               */
/************************************************************************/

int OGRFMECacheIndex::Unlock()

{
    if( pszPath == NULL || hLock == NULL )
        return FALSE;

    CPLUnlockFile( hLock );
    hLock = NULL;

    return TRUE;
}

/************************************************************************/
/*                                Load()                                */
/************************************************************************/

int OGRFMECacheIndex::Load()

{
/* -------------------------------------------------------------------- */
/*      Lock the cache index file if not already locked.                */
/* -------------------------------------------------------------------- */
    if( hLock == NULL && !Lock() )
        return FALSE;

    if( psTree != NULL )
    {
        CPLDestroyXMLNode( psTree );
        psTree = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the index file.  If we don't get it, we assume it is       */
/*      because it doesn't exist, and we create a "stub" tree in        */
/*      memory.                                                         */
/* -------------------------------------------------------------------- */
    FILE *fpIndex;
    int  nLength;
    char *pszIndexBuffer;

    fpIndex = VSIFOpen( GetPath(), "rb" );
    if( fpIndex == NULL )
    {
        psTree = CPLCreateXMLNode( NULL, CXT_Element, "OGRFMECacheIndex" );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Load the data from the file.                                    */
/* -------------------------------------------------------------------- */
    VSIFSeek( fpIndex, 0, SEEK_END );
    nLength = VSIFTell( fpIndex );
    VSIFSeek( fpIndex, 0, SEEK_SET );

    pszIndexBuffer = (char *) CPLMalloc(nLength+1);
    if( (int) VSIFRead( pszIndexBuffer, 1, nLength, fpIndex ) != nLength )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Read of %d byte index file failed.", nLength );
        return FALSE;
    }
    VSIFClose( fpIndex );

/* -------------------------------------------------------------------- */
/*      Parse the result into an inmemory XML tree.                     */
/* -------------------------------------------------------------------- */
    pszIndexBuffer[nLength] = '\0';
    psTree = CPLParseXMLString( pszIndexBuffer );

    CPLFree( pszIndexBuffer );

    return psTree != NULL;
}

/************************************************************************/
/*                                Save()                                */
/************************************************************************/

int OGRFMECacheIndex::Save()

{
    if( hLock == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Convert the XML tree into one big character buffer, and         */
/*      write it out.                                                   */
/* -------------------------------------------------------------------- */
    char *pszIndexBuffer = CPLSerializeXMLTree( psTree );

    if( pszIndexBuffer == NULL )
        return FALSE;

    FILE *fpIndex = VSIFOpen( GetPath(), "wb" );
    if( fpIndex == NULL )
        return FALSE;

    VSIFWrite( pszIndexBuffer, 1, strlen(pszIndexBuffer), fpIndex );
    CPLFree( pszIndexBuffer );

    VSIFClose( fpIndex );

    Unlock();

    return TRUE;
}

/************************************************************************/
/*                             FindMatch()                              */
/*                                                                      */
/*      Find a DataSource subtree that matches the passed in            */
/*      component values.                                               */
/************************************************************************/

CPLXMLNode *OGRFMECacheIndex::FindMatch( const char *pszDriver,
                                         const char *pszDataset,
                                         IFMEStringArray &oUserDirectives )

{
    CPLXMLNode *psCDS;

    if( psTree == NULL )
        return NULL;

    for( psCDS = psTree->psChild; psCDS != NULL; psCDS = psCDS->psNext )
    {
        if( !EQUAL(pszDriver,CPLGetXMLValue(psCDS,"Driver","")) )
            continue;

        if( !EQUAL(pszDataset,CPLGetXMLValue(psCDS,"DSName","")) )
            continue;

        CPLXMLNode *psDirective;
        int        bMatch = TRUE;
        int        iDir;

        psDirective = CPLGetXMLNode( psCDS, "UserDirectives.Directive" );
        for( iDir = 0;
             iDir < (int)oUserDirectives.entries() && bMatch;
             iDir++ )
        {
            if( psDirective == NULL || psDirective->psChild == NULL )
                bMatch = FALSE;
            else if( !EQUAL(psDirective->psChild->pszValue,
                            oUserDirectives(iDir)) )
                bMatch = FALSE;
            else
                psDirective = psDirective->psNext;
        }

        if( iDir < (int) oUserDirectives.entries() || !bMatch
            || (psDirective != NULL && psDirective->psNext != NULL) )
            continue;

        return psCDS;
    }

    return NULL;
}

/************************************************************************/
/*                               Touch()                                */
/*                                                                      */
/*      Update the LastUseTime on the passed datasource.                */
/************************************************************************/

void OGRFMECacheIndex::Touch( CPLXMLNode *psDSNode )

{
    if( psDSNode == NULL || !EQUAL(psDSNode->pszValue,"DataSource") )
        return;

/* -------------------------------------------------------------------- */
/*      Prepare the new time value to use.                              */
/* -------------------------------------------------------------------- */
    char      szNewTime[32];

    sprintf( szNewTime, "%lu", (unsigned long) time(NULL) );

/* -------------------------------------------------------------------- */
/*      Set or insert LastUseTime into dataset.                         */
/* -------------------------------------------------------------------- */
    CPLSetXMLValue( psDSNode, "LastUseTime", szNewTime );
}

/************************************************************************/
/*                             Reference()                              */
/************************************************************************/

void OGRFMECacheIndex::Reference( CPLXMLNode *psDSNode )

{
    if( psDSNode == NULL || !EQUAL(psDSNode->pszValue,"DataSource") )
        return;

    char szNewRefCount[32];

    sprintf( szNewRefCount, "%d",
             atoi(CPLGetXMLValue(psDSNode, "RefCount", "0")) + 1 );

    CPLSetXMLValue( psDSNode, "RefCount", szNewRefCount );

    Touch( psDSNode );
}

/************************************************************************/
/*                            Dereference()                             */
/************************************************************************/

void OGRFMECacheIndex::Dereference( CPLXMLNode *psDSNode )

{
    if( psDSNode == NULL
        || !EQUAL(psDSNode->pszValue,"DataSource")
        || CPLGetXMLNode(psDSNode,"RefCount") == NULL )
        return;

    char szNewRefCount[32];
    int  nRefCount = atoi(CPLGetXMLValue(psDSNode, "RefCount", "1"));
    if( nRefCount < 1 )
        nRefCount = 1;

    sprintf( szNewRefCount, "%d", nRefCount-1 );

    CPLSetXMLValue( psDSNode, "RefCount", szNewRefCount );

    Touch( psDSNode );
}

/************************************************************************/
/*                                Add()                                 */
/*                                                                      */
/*      Note that Add() takes over ownership of the passed tree.        */
/************************************************************************/

void OGRFMECacheIndex::Add( CPLXMLNode *psDSNode )

{
    if( psTree == NULL )
    {
        CPLAssert( false );
        return;
    }

    if( psDSNode == NULL || !EQUAL(psDSNode->pszValue,"DataSource") )
        return;

    psDSNode->psNext = psTree->psChild;
    psTree->psChild = psDSNode;


/* -------------------------------------------------------------------- */
/*      Prepare the creation time value to use.                         */
/* -------------------------------------------------------------------- */
    char      szNewTime[32];

    sprintf( szNewTime, "%lu", (unsigned long) time(NULL) );

/* -------------------------------------------------------------------- */
/*      Set or insert CreationTime into dataset.                        */
/* -------------------------------------------------------------------- */
    CPLSetXMLValue( psDSNode, "CreationTime", szNewTime );
}

/************************************************************************/
/*                          ExpireOldCaches()                           */
/*                                                                      */
/*      Make a pass over all the cache index entries.  Remove (and      */
/*      free the associated FME spatial caches) for any entries that    */
/*      haven't been touched for a long time.  Note that two            */
/*      different timeouts apply.  One is for layers with a RefCount    */
/*      of 0 and the other (longer time) is for those with a            */
/*      non-zero refcount.  Even if the RefCount is non-zero we         */
/*      assume this may because a program crashed during its run.       */
/************************************************************************/

int OGRFMECacheIndex::ExpireOldCaches( IFMESession *poSession )

{
    CPLXMLNode *psDSNode, *psLastDSNode = NULL;
    unsigned long nCurTime = time(NULL);
    int  bChangeMade = FALSE;

    if( psTree == NULL )
        return FALSE;

    for( psLastDSNode = NULL; true; psLastDSNode = psDSNode )
    {
        if( psLastDSNode != NULL )
            psDSNode = psLastDSNode->psNext;
        else
            psDSNode = psTree->psChild;
        if( psDSNode == NULL )
            break;

        if( !EQUAL(psDSNode->pszValue,"DataSource") )
            continue;

/* -------------------------------------------------------------------- */
/*      When was this datasource last accessed?                         */
/* -------------------------------------------------------------------- */
        unsigned long nLastUseTime = 0;

        sscanf( CPLGetXMLValue( psDSNode, "LastUseTime", "0" ),
                "%lu", &nLastUseTime );

/* -------------------------------------------------------------------- */
/*      When was this datasource created.                               */
/* -------------------------------------------------------------------- */
        unsigned long nCreationTime = 0;

        sscanf( CPLGetXMLValue( psDSNode, "CreationTime", "0" ),
                "%lu", &nCreationTime );

/* -------------------------------------------------------------------- */
/*      Do we want to delete this datasource according to our           */
/*      retention and ref timeout rules?                                */
/* -------------------------------------------------------------------- */
        int bCleanup = FALSE;

        // Do we want to cleanup this node?
        if( atoi(CPLGetXMLValue( psDSNode, "RefCount", "0" )) > 0
             && nLastUseTime + FMECACHE_REF_TIMEOUT < nCurTime )
            bCleanup = TRUE;

        if( atoi(CPLGetXMLValue( psDSNode, "RefCount", "0" )) < 1
            && nLastUseTime + FMECACHE_RETENTION < nCurTime )
            bCleanup = TRUE;

        if( atoi(CPLGetXMLValue( psDSNode, "RefCount", "0" )) < 1
            && nCreationTime + FMECACHE_MAX_RETENTION < nCurTime )
            bCleanup = TRUE;

        if( !bCleanup )
            continue;

        bChangeMade = TRUE;

        CPLDebug( "OGRFMECacheIndex",
                  "ExpireOldCaches() cleaning up data source %s - %ds since last use, %ds old.",
                  CPLGetXMLValue( psDSNode, "DSName", "<missing name>" ),
                  nCurTime - nLastUseTime,
                  nCurTime - nCreationTime );

/* -------------------------------------------------------------------- */
/*      Loop over all the layers, to delete the spatial caches on       */
/*      disk.                                                           */
/* -------------------------------------------------------------------- */
        CPLXMLNode *psLayerN;

        for( psLayerN = psDSNode->psChild;
             psLayerN != NULL;
             psLayerN = psLayerN->psNext )
        {
            IFMESpatialIndex   *poIndex;

            if( !EQUAL(psLayerN->pszValue,"OGRLayer") )
                continue;

            const char *pszBase;

            pszBase = CPLGetXMLValue( psLayerN, "SpatialCacheName", "" );
            if( EQUAL(pszBase,"") )
                continue;

            // open, and then delete the index on close.
            poIndex = poSession->createSpatialIndex( pszBase, "READ", NULL );

            if( poIndex == NULL )
                continue;

            if( poIndex->open() != 0 )
            {
                CPLDebug( "OGRFMECacheIndex", "Failed to open FME index %s.",
                          pszBase );

                poSession->destroySpatialIndex( poIndex );
                continue;
            }

            poIndex->close( FME_TRUE );
            poSession->destroySpatialIndex( poIndex );
        }

/* -------------------------------------------------------------------- */
/*      Remove the datasource from the tree.                            */
/* -------------------------------------------------------------------- */
        if( psLastDSNode == NULL )
            psTree->psChild = psDSNode->psNext;
        else
            psLastDSNode->psNext = psDSNode->psNext;

        psDSNode->psNext = NULL;
        CPLDestroyXMLNode( psDSNode );
        psDSNode = psLastDSNode;
    }

    return bChangeMade;
}
