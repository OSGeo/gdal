/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROSMDataSource class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_osm.h"
#include "cpl_conv.h"
#include "cpl_time.h"
#include "ogr_p.h"
#include "ogr_api.h"
#include "swq.h"
#include "gpb.h"
#include "ogrlayerdecorator.h"
#include "ogrsqliteexecutesql.h"
#include "cpl_multiproc.h"

#include <algorithm>

#define LIMIT_IDS_PER_REQUEST 200

#define MAX_NODES_PER_WAY 2000

#define IDX_LYR_POINTS           0
#define IDX_LYR_LINES            1
#define IDX_LYR_MULTILINESTRINGS 2
#define IDX_LYR_MULTIPOLYGONS    3
#define IDX_LYR_OTHER_RELATIONS  4

#define DBL_TO_INT(x)            (int)floor((x) * 1e7 + 0.5)
#define INT_TO_DBL(x)            ((x) / 1e7)

#define MAX_COUNT_FOR_TAGS_IN_WAY   255  /* must fit on 1 byte */
#define MAX_SIZE_FOR_TAGS_IN_WAY    1024

/* 5 bytes for encoding a int : really the worst case scenario ! */
#define WAY_BUFFER_SIZE (1 + MAX_NODES_PER_WAY * 2 * 5 + MAX_SIZE_FOR_TAGS_IN_WAY)

#define NODE_PER_BUCKET     65536

 /* Initial Maximum count of buckets */
#define INIT_BUCKET_COUNT        65536

#define VALID_ID_FOR_CUSTOM_INDEXING(_id) ((_id) >= 0 && (_id / NODE_PER_BUCKET) < INT_MAX)

/* Minimum size of data written on disk, in *uncompressed* case */
#define SECTOR_SIZE         512
/* Which represents, 64 nodes */
/* #define NODE_PER_SECTOR     SECTOR_SIZE / (2 * 4) */
#define NODE_PER_SECTOR     64
#define NODE_PER_SECTOR_SHIFT   6

/* Per bucket, we keep track of the absence/presence of sectors */
/* only, to reduce memory usage */
/* #define BUCKET_BITMAP_SIZE  NODE_PER_BUCKET / (8 * NODE_PER_SECTOR) */
#define BUCKET_BITMAP_SIZE  128

/* #define BUCKET_SECTOR_SIZE_ARRAY_SIZE  NODE_PER_BUCKET / NODE_PER_SECTOR */
/* Per bucket, we keep track of the real size of the sector. Each sector */
/* size is encoded in a single byte, whose value is : */
/* (sector_size in bytes - 8 ) / 2, minus 8. 252 means uncompressed */
#define BUCKET_SECTOR_SIZE_ARRAY_SIZE   1024

/* Must be a multiple of both BUCKET_BITMAP_SIZE and BUCKET_SECTOR_SIZE_ARRAY_SIZE */
#define PAGE_SIZE   4096

/* compressSize should not be greater than 512, so COMPRESS_SIZE_TO_BYTE() fits on a byte */
#define COMPRESS_SIZE_TO_BYTE(nCompressSize)  (((nCompressSize) - 8) / 2)
#define ROUND_COMPRESS_SIZE(nCompressSize)    (((nCompressSize) + 1) / 2) * 2;
#define COMPRESS_SIZE_FROM_BYTE(byte_on_size) ((byte_on_size) * 2 + 8)

/* Max number of features that are accumulated in pasWayFeaturePairs */
#define MAX_DELAYED_FEATURES        75000
/* Max number of tags that are accumulated in pasAccumulatedTags */
#define MAX_ACCUMULATED_TAGS        MAX_DELAYED_FEATURES * 5
/* Max size of the string with tag values that are accumulated in pabyNonRedundantValues */
#define MAX_NON_REDUNDANT_VALUES    MAX_DELAYED_FEATURES * 10
/* Max number of features that are accumulated in panUnsortedReqIds */
#define MAX_ACCUMULATED_NODES       1000000

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
/* Size of panHashedIndexes array. Must be in the list at */
/* http://planetmath.org/goodhashtableprimes , and greater than MAX_ACCUMULATED_NODES */
#define HASHED_INDEXES_ARRAY_SIZE   3145739
//#define HASHED_INDEXES_ARRAY_SIZE   1572869
#define COLLISION_BUCKET_ARRAY_SIZE ((MAX_ACCUMULATED_NODES / 100) * 40)
/* hash function = identity ! */
#define HASH_ID_FUNC(x)             ((GUIntBig)(x))
#endif // ENABLE_NODE_LOOKUP_BY_HASHING

//#define FAKE_LOOKUP_NODES

//#define DEBUG_MEM_USAGE
#ifdef DEBUG_MEM_USAGE
size_t GetMaxTotalAllocs();
#endif

static void WriteVarInt64(GUIntBig nSVal, GByte** ppabyData);
static void WriteVarSInt64(GIntBig nSVal, GByte** ppabyData);

CPL_CVSID("$Id$");

class DSToBeOpened
{
    public:
        GIntBig                 nPID;
        CPLString               osDSName;
        CPLString               osInterestLayers;
};

static void                      *hMutex = NULL;
static std::vector<DSToBeOpened>  oListDSToBeOpened;

/************************************************************************/
/*                    AddInterestLayersForDSName()                      */
/************************************************************************/

static void AddInterestLayersForDSName(const CPLString& osDSName,
                                       const CPLString& osInterestLayers)
{
    CPLMutexHolder oMutexHolder(&hMutex);
    DSToBeOpened oDSToBeOpened;
    oDSToBeOpened.nPID = CPLGetPID();
    oDSToBeOpened.osDSName = osDSName;
    oDSToBeOpened.osInterestLayers = osInterestLayers;
    oListDSToBeOpened.push_back( oDSToBeOpened );
}

/************************************************************************/
/*                    GetInterestLayersForDSName()                      */
/************************************************************************/

static CPLString GetInterestLayersForDSName(const CPLString& osDSName)
{
    CPLMutexHolder oMutexHolder(&hMutex);
    GIntBig nPID = CPLGetPID();
    for(int i = 0; i < (int)oListDSToBeOpened.size(); i++)
    {
        if( oListDSToBeOpened[i].nPID == nPID &&
            oListDSToBeOpened[i].osDSName == osDSName )
        {
            CPLString osInterestLayers = oListDSToBeOpened[i].osInterestLayers;
            oListDSToBeOpened.erase(oListDSToBeOpened.begin()+i);
            return osInterestLayers;
        }
    }
    return "";
}

/************************************************************************/
/*                        OGROSMDataSource()                            */
/************************************************************************/

OGROSMDataSource::OGROSMDataSource()

{
    nLayers = 0;
    papoLayers = NULL;
    pszName = NULL;
    bInterleavedReading = -1;
    poCurrentLayer = NULL;
    psParser = NULL;
    bHasParsedFirstChunk = FALSE;
    bStopParsing = FALSE;
#ifdef HAVE_SQLITE_VFS
    pMyVFS = NULL;
#endif
    hDB = NULL;
    hInsertNodeStmt = NULL;
    hInsertWayStmt = NULL;
    hInsertPolygonsStandaloneStmt = NULL;
    hDeletePolygonsStandaloneStmt = NULL;
    hSelectPolygonsStandaloneStmt = NULL;
    bHasRowInPolygonsStandalone = FALSE;
    nNodesInTransaction = 0;
    bInTransaction = FALSE;
    pahSelectNodeStmt = NULL;
    pahSelectWayStmt = NULL;
    pasLonLatCache = NULL;
    bInMemoryTmpDB = FALSE;
    bMustUnlink = TRUE;
    nMaxSizeForInMemoryDBInMB = 0;
    bReportAllNodes = FALSE;
    bReportAllWays = FALSE;
    bFeatureAdded = FALSE;

    bIndexPoints = TRUE;
    bUsePointsIndex = TRUE;
    bIndexWays = TRUE;
    bUseWaysIndex = TRUE;

    poResultSetLayer = NULL;
    bIndexPointsBackup = FALSE;
    bUsePointsIndexBackup = FALSE;
    bIndexWaysBackup = FALSE;
    bUseWaysIndexBackup = FALSE;

    bIsFeatureCountEnabled = FALSE;

    bAttributeNameLaundering = TRUE;

    pabyWayBuffer = NULL;

    nWaysProcessed = 0;
    nRelationsProcessed = 0;

    bCustomIndexing = TRUE;
    bCompressNodes = FALSE;

    bInMemoryNodesFile = FALSE;
    bMustUnlinkNodesFile = TRUE;
    fpNodes = NULL;
    nNodesFileSize = 0;

    nPrevNodeId = -INT_MAX;
    nBucketOld = -1;
    nOffInBucketReducedOld = -1;
    pabySector = NULL;
    papsBuckets = NULL;
    nBuckets = 0;

    nReqIds = 0;
    panReqIds = NULL;
#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    bEnableHashedIndex = TRUE;
    panHashedIndexes = NULL;
    psCollisionBuckets = NULL;
    bHashedIndexValid = FALSE;
#endif
    pasLonLatArray = NULL;
    nUnsortedReqIds = 0;
    panUnsortedReqIds = NULL;
    nWayFeaturePairs = 0;
    pasWayFeaturePairs = NULL;
    nAccumulatedTags = 0;
    pasAccumulatedTags = NULL;
    nNonRedundantValuesLen = 0;
    pabyNonRedundantValues = NULL;
    nNextKeyIndex = 0;

    bNeedsToSaveWayInfo = FALSE;
}

/************************************************************************/
/*                          ~OGROSMDataSource()                         */
/************************************************************************/

OGROSMDataSource::~OGROSMDataSource()

{
    int i;
    for(i=0;i<nLayers;i++)
        delete papoLayers[i];
    CPLFree(papoLayers);

    CPLFree(pszName);

    if( psParser != NULL )
        CPLDebug("OSM", "Number of bytes read in file : " CPL_FRMT_GUIB, OSM_GetBytesRead(psParser));
    OSM_Close(psParser);

    CPLFree(pasLonLatCache);
    CPLFree(pabyWayBuffer);

    if( hDB != NULL )
        CloseDB();

#ifdef HAVE_SQLITE_VFS
    if (pMyVFS)
    {
        sqlite3_vfs_unregister(pMyVFS);
        CPLFree(pMyVFS->pAppData);
        CPLFree(pMyVFS);
    }
#endif

    if( osTmpDBName.size() && bMustUnlink )
    {
        const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
        if( !EQUAL(pszVal, "NOT_EVEN_AT_END") )
            VSIUnlink(osTmpDBName);
    }

    CPLFree(panReqIds);
#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    CPLFree(panHashedIndexes);
    CPLFree(psCollisionBuckets);
#endif
    CPLFree(pasLonLatArray);
    CPLFree(panUnsortedReqIds);

    for( i = 0; i < nWayFeaturePairs; i++)
    {
        delete pasWayFeaturePairs[i].poFeature;
    }
    CPLFree(pasWayFeaturePairs);
    CPLFree(pasAccumulatedTags);
    CPLFree(pabyNonRedundantValues);

#ifdef OSM_DEBUG
    FILE* f;
    f = fopen("keys.txt", "wt");
    for(i=0;i<(int)asKeys.size();i++)
    {
        KeyDesc* psKD = asKeys[i];
        fprintf(f, "%08d idx=%d %s\n",
                psKD->nOccurences,
                psKD->nKeyIndex,
                psKD->pszK);
    }
    fclose(f);
#endif

    for(i=0;i<(int)asKeys.size();i++)
    {
        KeyDesc* psKD = asKeys[i];
        CPLFree(psKD->pszK);
        for(int j=0;j<(int)psKD->asValues.size();j++)
            CPLFree(psKD->asValues[j]);
        delete psKD;
    }

    if( fpNodes )
        VSIFCloseL(fpNodes);
    if( osNodesFilename.size() && bMustUnlinkNodesFile )
    {
        const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
        if( !EQUAL(pszVal, "NOT_EVEN_AT_END") )
            VSIUnlink(osNodesFilename);
    }

    CPLFree(pabySector);
    if( papsBuckets )
    {
        for( i = 0; i < nBuckets; i++)
        {
            if( bCompressNodes )
            {
                int nRem = i % (PAGE_SIZE / BUCKET_SECTOR_SIZE_ARRAY_SIZE);
                if( nRem == 0 )
                    CPLFree(papsBuckets[i].u.panSectorSize);
            }
            else
            {
                int nRem = i % (PAGE_SIZE / BUCKET_BITMAP_SIZE);
                if( nRem == 0 )
                    CPLFree(papsBuckets[i].u.pabyBitmap);
            }
        }
        CPLFree(papsBuckets);
    }
}

/************************************************************************/
/*                             CloseDB()                               */
/************************************************************************/

void OGROSMDataSource::CloseDB()
{
    int i;

    if( hInsertNodeStmt != NULL )
        sqlite3_finalize( hInsertNodeStmt );
    hInsertNodeStmt = NULL;

    if( hInsertWayStmt != NULL )
        sqlite3_finalize( hInsertWayStmt );
    hInsertWayStmt = NULL;

    if( hInsertPolygonsStandaloneStmt != NULL )
        sqlite3_finalize( hInsertPolygonsStandaloneStmt );
    hInsertPolygonsStandaloneStmt = NULL;

    if( hDeletePolygonsStandaloneStmt != NULL )
        sqlite3_finalize( hDeletePolygonsStandaloneStmt );
    hDeletePolygonsStandaloneStmt = NULL;

    if( hSelectPolygonsStandaloneStmt != NULL )
        sqlite3_finalize( hSelectPolygonsStandaloneStmt );
    hSelectPolygonsStandaloneStmt = NULL;

    if( pahSelectNodeStmt != NULL )
    {
        for(i = 0; i < LIMIT_IDS_PER_REQUEST; i++)
        {
            if( pahSelectNodeStmt[i] != NULL )
                sqlite3_finalize( pahSelectNodeStmt[i] );
        }
        CPLFree(pahSelectNodeStmt);
        pahSelectNodeStmt = NULL;
    }

    if( pahSelectWayStmt != NULL )
    {
        for(i = 0; i < LIMIT_IDS_PER_REQUEST; i++)
        {
            if( pahSelectWayStmt[i] != NULL )
                sqlite3_finalize( pahSelectWayStmt[i] );
        }
        CPLFree(pahSelectWayStmt);
        pahSelectWayStmt = NULL;
    }

    if( bInTransaction )
        CommitTransaction();

    sqlite3_close(hDB);
    hDB = NULL;
}

/************************************************************************/
/*                             IndexPoint()                             */
/************************************************************************/

static const GByte abyBitsCount[] = {
0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
};

int OGROSMDataSource::IndexPoint(OSMNode* psNode)
{
    if( !bIndexPoints )
        return TRUE;

    if( bCustomIndexing)
        return IndexPointCustom(psNode);
    else
        return IndexPointSQLite(psNode);
}

/************************************************************************/
/*                          IndexPointSQLite()                          */
/************************************************************************/

int OGROSMDataSource::IndexPointSQLite(OSMNode* psNode)
{
    sqlite3_bind_int64( hInsertNodeStmt, 1, psNode->nID );

    LonLat sLonLat;
    sLonLat.nLon = DBL_TO_INT(psNode->dfLon);
    sLonLat.nLat = DBL_TO_INT(psNode->dfLat);

    sqlite3_bind_blob( hInsertNodeStmt, 2, &sLonLat, sizeof(sLonLat), SQLITE_STATIC );

    int rc = sqlite3_step( hInsertNodeStmt );
    sqlite3_reset( hInsertNodeStmt );
    if( !(rc == SQLITE_OK || rc == SQLITE_DONE) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed inserting node " CPL_FRMT_GIB ": %s",
            psNode->nID, sqlite3_errmsg(hDB));
    }

    return TRUE;
}

/************************************************************************/
/*                           FlushCurrentSector()                       */
/************************************************************************/

int OGROSMDataSource::FlushCurrentSector()
{
#ifndef FAKE_LOOKUP_NODES
    if( bCompressNodes )
        return FlushCurrentSectorCompressedCase();
    else
        return FlushCurrentSectorNonCompressedCase();
#else
    return TRUE;
#endif
}

/************************************************************************/
/*                            AllocBucket()                             */
/************************************************************************/

int OGROSMDataSource::AllocBucket(int iBucket)
{
    int bOOM = FALSE;
    if( bCompressNodes )
    {
        int nRem = iBucket % (PAGE_SIZE / BUCKET_SECTOR_SIZE_ARRAY_SIZE);
        if( papsBuckets[iBucket - nRem].u.panSectorSize == NULL )
            papsBuckets[iBucket - nRem].u.panSectorSize = (GByte*)VSICalloc(1, PAGE_SIZE);
        if( papsBuckets[iBucket - nRem].u.panSectorSize == NULL )
        {
            papsBuckets[iBucket].u.panSectorSize = NULL;
            bOOM = TRUE;
        }
        else
            papsBuckets[iBucket].u.panSectorSize = papsBuckets[iBucket - nRem].u.panSectorSize + nRem * BUCKET_SECTOR_SIZE_ARRAY_SIZE;
    }
    else
    {
        int nRem = iBucket % (PAGE_SIZE / BUCKET_BITMAP_SIZE);
        if( papsBuckets[iBucket - nRem].u.pabyBitmap == NULL )
            papsBuckets[iBucket - nRem].u.pabyBitmap = (GByte*)VSICalloc(1, PAGE_SIZE);
        if( papsBuckets[iBucket - nRem].u.pabyBitmap == NULL )
        {
            papsBuckets[iBucket].u.pabyBitmap = NULL;
            bOOM = TRUE;
        }
        else
            papsBuckets[iBucket].u.pabyBitmap = papsBuckets[iBucket - nRem].u.pabyBitmap + nRem * BUCKET_BITMAP_SIZE;
    }

    if( bOOM )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AllocBucket() failed. Use OSM_USE_CUSTOM_INDEXING=NO");
        bStopParsing = TRUE;
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                         AllocMoreBuckets()                           */
/************************************************************************/

int OGROSMDataSource::AllocMoreBuckets(int nNewBucketIdx, int bAllocBucket)
{
    CPLAssert(nNewBucketIdx >= nBuckets);

    int nNewBuckets = MAX(nBuckets + nBuckets / 2, nNewBucketIdx);

    size_t nNewSize = sizeof(Bucket) * nNewBuckets;
    if( (GUIntBig)nNewSize != sizeof(Bucket) * (GUIntBig)nNewBuckets )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AllocMoreBuckets() failed. Use OSM_USE_CUSTOM_INDEXING=NO");
        bStopParsing = TRUE;
        return FALSE;
    }

    Bucket* papsNewBuckets = (Bucket*) VSIRealloc(papsBuckets, nNewSize);
    if( papsNewBuckets == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AllocMoreBuckets() failed. Use OSM_USE_CUSTOM_INDEXING=NO");
        bStopParsing = TRUE;
        return FALSE;
    }
    papsBuckets = papsNewBuckets;

    int bOOM = FALSE;
    int i;
    for(i = nBuckets; i < nNewBuckets && !bOOM; i++)
    {
        papsBuckets[i].nOff = -1;
        if( bAllocBucket )
        {
            if( !AllocBucket(i) )
                bOOM = TRUE;
        }
        else
        {
            if( bCompressNodes )
                papsBuckets[i].u.panSectorSize = NULL;
            else
                papsBuckets[i].u.pabyBitmap = NULL;
        }
    }
    nBuckets = i;

    if( bOOM )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "AllocMoreBuckets() failed. Use OSM_USE_CUSTOM_INDEXING=NO");
        bStopParsing = TRUE;
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                     FlushCurrentSectorCompressedCase()               */
/************************************************************************/

int OGROSMDataSource::FlushCurrentSectorCompressedCase()
{
    GByte abyOutBuffer[2 * SECTOR_SIZE];
    GByte* pabyOut = abyOutBuffer;
    LonLat* pasLonLatIn = (LonLat*)pabySector;
    int nLastLon = 0, nLastLat = 0;
    int bLastValid = FALSE;
    int i;

    CPLAssert((NODE_PER_SECTOR % 8) == 0);
    memset(abyOutBuffer, 0, NODE_PER_SECTOR / 8);
    pabyOut += NODE_PER_SECTOR / 8;
    for(i = 0; i < NODE_PER_SECTOR; i++)
    {
        if( pasLonLatIn[i].nLon || pasLonLatIn[i].nLat )
        {
            abyOutBuffer[i >> 3] |= (1 << (i % 8));
            if( bLastValid )
            {
                GIntBig nDiff64Lon = (GIntBig)pasLonLatIn[i].nLon - (GIntBig)nLastLon;
                GIntBig nDiff64Lat = pasLonLatIn[i].nLat - nLastLat;
                WriteVarSInt64(nDiff64Lon, &pabyOut);
                WriteVarSInt64(nDiff64Lat, &pabyOut);
            }
            else
            {
                memcpy(pabyOut, &pasLonLatIn[i], sizeof(LonLat));
                pabyOut += sizeof(LonLat);
            }
            bLastValid = TRUE;

            nLastLon = pasLonLatIn[i].nLon;
            nLastLat = pasLonLatIn[i].nLat;
        }
    }

    size_t nCompressSize = (size_t)(pabyOut - abyOutBuffer);
    CPLAssert(nCompressSize < sizeof(abyOutBuffer) - 1);
    abyOutBuffer[nCompressSize] = 0;

    nCompressSize = ROUND_COMPRESS_SIZE(nCompressSize);
    GByte* pabyToWrite;
    if(nCompressSize >= SECTOR_SIZE)
    {
        nCompressSize = SECTOR_SIZE;
        pabyToWrite = pabySector;
    }
    else
        pabyToWrite = abyOutBuffer;

    if( VSIFWriteL(pabyToWrite, 1, nCompressSize, fpNodes) == nCompressSize )
    {
        memset(pabySector, 0, SECTOR_SIZE);
        nNodesFileSize += nCompressSize;

        if( nBucketOld >= nBuckets )
        {
            if( !AllocMoreBuckets(nBucketOld + 1) )
                return FALSE;
        }
        Bucket* psBucket = &papsBuckets[nBucketOld];
        if( psBucket->u.panSectorSize == NULL && !AllocBucket(nBucketOld) )
            return FALSE;
        psBucket->u.panSectorSize[nOffInBucketReducedOld] =
                                    COMPRESS_SIZE_TO_BYTE(nCompressSize);

        return TRUE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot write in temporary node file %s : %s",
                 osNodesFilename.c_str(), VSIStrerror(errno));
    }

    return FALSE;
}

/************************************************************************/
/*                   FlushCurrentSectorNonCompressedCase()              */
/************************************************************************/

int OGROSMDataSource::FlushCurrentSectorNonCompressedCase()
{
   if( VSIFWriteL(pabySector, 1, SECTOR_SIZE, fpNodes) == SECTOR_SIZE )
    {
        memset(pabySector, 0, SECTOR_SIZE);
        nNodesFileSize += SECTOR_SIZE;
        return TRUE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot write in temporary node file %s : %s",
                 osNodesFilename.c_str(), VSIStrerror(errno));
    }

    return FALSE;
}

/************************************************************************/
/*                          IndexPointCustom()                          */
/************************************************************************/

int OGROSMDataSource::IndexPointCustom(OSMNode* psNode)
{
    if( psNode->nID <= nPrevNodeId)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Non increasing node id. Use OSM_USE_CUSTOM_INDEXING=NO");
        bStopParsing = TRUE;
        return FALSE;
    }
    if( !VALID_ID_FOR_CUSTOM_INDEXING(psNode->nID) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported node id value (" CPL_FRMT_GIB "). Use OSM_USE_CUSTOM_INDEXING=NO",
                 psNode->nID);
        bStopParsing = TRUE;
        return FALSE;
    }

    int nBucket = (int)(psNode->nID / NODE_PER_BUCKET);
    int nOffInBucket = psNode->nID % NODE_PER_BUCKET;
    int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
    int nOffInBucketReducedRemainer = nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

    if( nBucket >= nBuckets )
    {
        if( !AllocMoreBuckets(nBucket + 1) )
            return FALSE;
    }
    Bucket* psBucket = &papsBuckets[nBucket];

    if( !bCompressNodes )
    {
        int nBitmapIndex = nOffInBucketReduced / 8;
        int nBitmapRemainer = nOffInBucketReduced % 8;
        if( psBucket->u.pabyBitmap == NULL && !AllocBucket(nBucket) )
            return FALSE;
        psBucket->u.pabyBitmap[nBitmapIndex] |= (1 << nBitmapRemainer);
    }

    if( nBucket != nBucketOld )
    {
        CPLAssert(nBucket > nBucketOld);
        if( nBucketOld >= 0 )
        {
            if( !FlushCurrentSector() )
            {
                bStopParsing = TRUE;
                return FALSE;
            }
        }
        nBucketOld = nBucket;
        nOffInBucketReducedOld = nOffInBucketReduced;
        CPLAssert(psBucket->nOff == -1);
        psBucket->nOff = VSIFTellL(fpNodes);
    }
    else if( nOffInBucketReduced != nOffInBucketReducedOld )
    {
        CPLAssert(nOffInBucketReduced > nOffInBucketReducedOld);
        if( !FlushCurrentSector() )
        {
            bStopParsing = TRUE;
            return FALSE;
        }
        nOffInBucketReducedOld = nOffInBucketReduced;
    }

    LonLat* psLonLat = (LonLat*)(pabySector + sizeof(LonLat) * nOffInBucketReducedRemainer);
    psLonLat->nLon = DBL_TO_INT(psNode->dfLon);
    psLonLat->nLat = DBL_TO_INT(psNode->dfLat);

    nPrevNodeId = psNode->nID;

    return TRUE;
}

/************************************************************************/
/*                             NotifyNodes()                            */
/************************************************************************/

void OGROSMDataSource::NotifyNodes(unsigned int nNodes, OSMNode* pasNodes)
{
    unsigned int i;

    const OGREnvelope* psEnvelope =
        papoLayers[IDX_LYR_POINTS]->GetSpatialFilterEnvelope();

    for(i = 0; i < nNodes; i++)
    {
        /* If the point doesn't fit into the envelope of the spatial filter */
        /* then skip it */
        if( psEnvelope != NULL &&
            !(pasNodes[i].dfLon >= psEnvelope->MinX &&
              pasNodes[i].dfLon <= psEnvelope->MaxX &&
              pasNodes[i].dfLat >= psEnvelope->MinY &&
              pasNodes[i].dfLat <= psEnvelope->MaxY) )
            continue;

        if( !IndexPoint(&pasNodes[i]) )
            break;

        if( !papoLayers[IDX_LYR_POINTS]->IsUserInterested() )
            continue;

        unsigned int j;
        int bInterestingTag = bReportAllNodes;
        OSMTag* pasTags = pasNodes[i].pasTags;

        if( !bReportAllNodes )
        {
            for(j = 0; j < pasNodes[i].nTags; j++)
            {
                const char* pszK = pasTags[j].pszK;
                if( papoLayers[IDX_LYR_POINTS]->IsSignificantKey(pszK) )
                {
                    bInterestingTag = TRUE;
                    break;
                }
            }
        }

        if( bInterestingTag )
        {
            OGRFeature* poFeature = new OGRFeature(
                        papoLayers[IDX_LYR_POINTS]->GetLayerDefn());

            poFeature->SetGeometryDirectly(
                new OGRPoint(pasNodes[i].dfLon, pasNodes[i].dfLat));

            papoLayers[IDX_LYR_POINTS]->SetFieldsFromTags(
                poFeature, pasNodes[i].nID, FALSE, pasNodes[i].nTags, pasTags, &pasNodes[i].sInfo );

            int bFilteredOut = FALSE;
            if( !papoLayers[IDX_LYR_POINTS]->AddFeature(poFeature, FALSE,
                                                        &bFilteredOut,
                                                        !bFeatureAdded) )
            {
                bStopParsing = TRUE;
                break;
            }
            else if (!bFilteredOut)
                bFeatureAdded = TRUE;
        }
    }
}

static void OGROSMNotifyNodes (unsigned int nNodes, OSMNode* pasNodes,
                               CPL_UNUSED OSMContext* psOSMContext, void* user_data)
{
    ((OGROSMDataSource*) user_data)->NotifyNodes(nNodes, pasNodes);
}

/************************************************************************/
/*                            LookupNodes()                             */
/************************************************************************/

//#define DEBUG_COLLISIONS 1

void OGROSMDataSource::LookupNodes( )
{
    if( bCustomIndexing )
        LookupNodesCustom();
    else
        LookupNodesSQLite();

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    if( nReqIds > 1 && bEnableHashedIndex )
    {
        memset(panHashedIndexes, 0xFF, HASHED_INDEXES_ARRAY_SIZE * sizeof(int));
        bHashedIndexValid = TRUE;
#ifdef DEBUG_COLLISIONS
        int nCollisions = 0;
#endif
        int iNextFreeBucket = 0;
        for(unsigned int i = 0; i < nReqIds; i++)
        {
            int nIndInHashArray = HASH_ID_FUNC(panReqIds[i]) % HASHED_INDEXES_ARRAY_SIZE;
            int nIdx = panHashedIndexes[nIndInHashArray];
            if( nIdx == -1 )
            {
                panHashedIndexes[nIndInHashArray] = i;
            }
            else
            {
#ifdef DEBUG_COLLISIONS
                nCollisions ++;
#endif
                int iBucket;
                if( nIdx >= 0 )
                {
                    if(iNextFreeBucket == COLLISION_BUCKET_ARRAY_SIZE)
                    {
                        CPLDebug("OSM", "Too many collisions. Disabling hashed indexing");
                        bHashedIndexValid = FALSE;
                        bEnableHashedIndex = FALSE;
                        break;
                    }
                    iBucket = iNextFreeBucket;
                    psCollisionBuckets[iNextFreeBucket].nInd = nIdx;
                    psCollisionBuckets[iNextFreeBucket].nNext = -1;
                    panHashedIndexes[nIndInHashArray] = -iNextFreeBucket - 2;
                    iNextFreeBucket ++;
                }
                else
                    iBucket = -nIdx - 2;
                if(iNextFreeBucket == COLLISION_BUCKET_ARRAY_SIZE)
                {
                    CPLDebug("OSM", "Too many collisions. Disabling hashed indexing");
                    bHashedIndexValid = FALSE;
                    bEnableHashedIndex = FALSE;
                    break;
                }
                while( TRUE )
                {
                    int iNext = psCollisionBuckets[iBucket].nNext;
                    if( iNext < 0 )
                    {
                        psCollisionBuckets[iBucket].nNext = iNextFreeBucket;
                        psCollisionBuckets[iNextFreeBucket].nInd = i;
                        psCollisionBuckets[iNextFreeBucket].nNext = -1;
                        iNextFreeBucket ++;
                        break;
                    }
                    iBucket = iNext;
                }
            }
        }
#ifdef DEBUG_COLLISIONS
        /* Collision rate in practice is around 12% on France, Germany, ... */
        /* Maximum seen ~ 15.9% on a planet file but often much smaller. */
        CPLDebug("OSM", "nCollisions = %d/%d (%.1f %%), iNextFreeBucket = %d/%d",
                 nCollisions, nReqIds, nCollisions * 100.0 / nReqIds,
                 iNextFreeBucket, COLLISION_BUCKET_ARRAY_SIZE);
#endif
    }
    else
        bHashedIndexValid = FALSE;
#endif // ENABLE_NODE_LOOKUP_BY_HASHING
}

/************************************************************************/
/*                           LookupNodesSQLite()                        */
/************************************************************************/

void OGROSMDataSource::LookupNodesSQLite( )
{
    unsigned int iCur;
    unsigned int i;

    CPLAssert(nUnsortedReqIds <= MAX_ACCUMULATED_NODES);

    nReqIds = 0;
    for(i = 0; i < nUnsortedReqIds; i++)
    {
        GIntBig id = panUnsortedReqIds[i];
        panReqIds[nReqIds++] = id;
    }

    std::sort(panReqIds, panReqIds + nReqIds);

    /* Remove duplicates */
    unsigned int j = 0;
    for(i = 0; i < nReqIds; i++)
    {
        if (!(i > 0 && panReqIds[i] == panReqIds[i-1]))
            panReqIds[j++] = panReqIds[i];
    }
    nReqIds = j;

    iCur = 0;
    j = 0;
    while( iCur < nReqIds )
    {
        unsigned int nToQuery = nReqIds - iCur;
        if( nToQuery > LIMIT_IDS_PER_REQUEST )
            nToQuery = LIMIT_IDS_PER_REQUEST;

        sqlite3_stmt* hStmt = pahSelectNodeStmt[nToQuery-1];
        for(i=iCur;i<iCur + nToQuery;i++)
        {
             sqlite3_bind_int64( hStmt, i - iCur +1, panReqIds[i] );
        }
        iCur += nToQuery;

        while( sqlite3_step(hStmt) == SQLITE_ROW )
        {
            GIntBig id = sqlite3_column_int64(hStmt, 0);
            LonLat* psLonLat = (LonLat*)sqlite3_column_blob(hStmt, 1);

            panReqIds[j] = id;
            pasLonLatArray[j].nLon = psLonLat->nLon;
            pasLonLatArray[j].nLat = psLonLat->nLat;
            j++;
        }

        sqlite3_reset(hStmt);
    }
    nReqIds = j;
}

/************************************************************************/
/*                            ReadVarSInt64()                           */
/************************************************************************/

static GIntBig ReadVarSInt64(GByte** ppabyPtr)
{
    GIntBig nSVal64 = ReadVarInt64(ppabyPtr);
    GIntBig nDiff64 = ((nSVal64 & 1) == 0) ? (((GUIntBig)nSVal64) >> 1) : -(((GUIntBig)nSVal64) >> 1)-1;
    return nDiff64;
}

/************************************************************************/
/*                           DecompressSector()                         */
/************************************************************************/

static int DecompressSector(GByte* pabyIn, int nSectorSize, GByte* pabyOut)
{
    GByte* pabyPtr = pabyIn;
    LonLat* pasLonLatOut = (LonLat*) pabyOut;
    int nLastLon = 0, nLastLat = 0;
    int bLastValid = FALSE;
    int i;

    pabyPtr += NODE_PER_SECTOR / 8;
    for(i = 0; i < NODE_PER_SECTOR; i++)
    {
        if( pabyIn[i >> 3] & (1 << (i % 8)) )
        {
            if( bLastValid )
            {
                pasLonLatOut[i].nLon = nLastLon + ReadVarSInt64(&pabyPtr);
                pasLonLatOut[i].nLat = nLastLat + ReadVarSInt64(&pabyPtr);
            }
            else
            {
                bLastValid = TRUE;
                memcpy(&(pasLonLatOut[i]), pabyPtr, sizeof(LonLat));
                pabyPtr += sizeof(LonLat);
            }

            nLastLon = pasLonLatOut[i].nLon;
            nLastLat = pasLonLatOut[i].nLat;
        }
        else
        {
            pasLonLatOut[i].nLon = 0;
            pasLonLatOut[i].nLat = 0;
        }
    }

    int nRead = (int)(pabyPtr - pabyIn);
    nRead = ROUND_COMPRESS_SIZE(nRead);
    return( nRead == nSectorSize );
}

/************************************************************************/
/*                           LookupNodesCustom()                        */
/************************************************************************/

void OGROSMDataSource::LookupNodesCustom( )
{
    nReqIds = 0;

    if( nBucketOld >= 0 )
    {
        if( !FlushCurrentSector() )
        {
            bStopParsing = TRUE;
            return;
        }

        nBucketOld = -1;
    }

    unsigned int i;

    CPLAssert(nUnsortedReqIds <= MAX_ACCUMULATED_NODES);

    for(i = 0; i < nUnsortedReqIds; i++)
    {
        GIntBig id = panUnsortedReqIds[i];

        if( !VALID_ID_FOR_CUSTOM_INDEXING(id) )
            continue;

        int nBucket = (int)(id / NODE_PER_BUCKET);
        int nOffInBucket = id % NODE_PER_BUCKET;
        int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;

        if( nBucket >= nBuckets )
            continue;
        Bucket* psBucket = &papsBuckets[nBucket];

        if( bCompressNodes )
        {
            if( psBucket->u.panSectorSize == NULL ||
                !(psBucket->u.panSectorSize[nOffInBucketReduced]) )
                continue;
        }
        else
        {
            int nBitmapIndex = nOffInBucketReduced / 8;
            int nBitmapRemainer = nOffInBucketReduced % 8;
            if( psBucket->u.pabyBitmap == NULL ||
                !(psBucket->u.pabyBitmap[nBitmapIndex] & (1 << nBitmapRemainer)) )
                continue;
        }

        panReqIds[nReqIds++] = id;
    }

    std::sort(panReqIds, panReqIds + nReqIds);

    /* Remove duplicates */
    unsigned int j = 0;
    for(i = 0; i < nReqIds; i++)
    {
        if (!(i > 0 && panReqIds[i] == panReqIds[i-1]))
            panReqIds[j++] = panReqIds[i];
    }
    nReqIds = j;

#ifdef FAKE_LOOKUP_NODES
    for(i = 0; i < nReqIds; i++)
    {
        pasLonLatArray[i].nLon = 0;
        pasLonLatArray[i].nLat = 0;
    }
#else
    if( bCompressNodes )
        LookupNodesCustomCompressedCase();
    else
        LookupNodesCustomNonCompressedCase();
#endif
}

/************************************************************************/
/*                      LookupNodesCustomCompressedCase()               */
/************************************************************************/

void OGROSMDataSource::LookupNodesCustomCompressedCase()
{
    unsigned int i;
    unsigned int j = 0;
#define SECURITY_MARGIN     (8 + 8 + 2 * NODE_PER_SECTOR)
    GByte abyRawSector[SECTOR_SIZE + SECURITY_MARGIN];
    memset(abyRawSector + SECTOR_SIZE, 0, SECURITY_MARGIN);

    int nBucketOld = -1;
    int nOffInBucketReducedOld = -1;
    int k = 0;
    int nOffFromBucketStart = 0;

    for(i = 0; i < nReqIds; i++)
    {
        GIntBig id = panReqIds[i];

        int nBucket = (int)(id / NODE_PER_BUCKET);
        int nOffInBucket = id % NODE_PER_BUCKET;
        int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
        int nOffInBucketReducedRemainer = nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

        if( nBucket != nBucketOld )
        {
            nOffInBucketReducedOld = -1;
            k = 0;
            nOffFromBucketStart = 0;
        }

        if ( nOffInBucketReduced != nOffInBucketReducedOld )
        {
            if( nBucket >= nBuckets )
            {
                CPLError(CE_Failure,  CPLE_AppDefined,
                        "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            Bucket* psBucket = &papsBuckets[nBucket];
            if( psBucket->u.panSectorSize == NULL )
            {
                CPLError(CE_Failure,  CPLE_AppDefined,
                        "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            int nSectorSize = COMPRESS_SIZE_FROM_BYTE(psBucket->u.panSectorSize[nOffInBucketReduced]);

            /* If we stay in the same bucket, we can reuse the previously */
            /* computed offset, instead of starting from bucket start */
            for(; k < nOffInBucketReduced; k++)
            {
                if( psBucket->u.panSectorSize[k] )
                    nOffFromBucketStart += COMPRESS_SIZE_FROM_BYTE(psBucket->u.panSectorSize[k]);
            }

            VSIFSeekL(fpNodes, psBucket->nOff + nOffFromBucketStart, SEEK_SET);
            if( nSectorSize == SECTOR_SIZE )
            {
                if( VSIFReadL(pabySector, 1, SECTOR_SIZE, fpNodes) != SECTOR_SIZE )
                {
                    CPLError(CE_Failure,  CPLE_AppDefined,
                            "Cannot read node " CPL_FRMT_GIB, id);
                    continue;
                    // FIXME ?
                }
            }
            else
            {
                if( (int)VSIFReadL(abyRawSector, 1, nSectorSize, fpNodes) != nSectorSize )
                {
                    CPLError(CE_Failure,  CPLE_AppDefined,
                            "Cannot read sector for node " CPL_FRMT_GIB, id);
                    continue;
                    // FIXME ?
                }
                abyRawSector[nSectorSize] = 0;

                if( !DecompressSector(abyRawSector, nSectorSize, pabySector) )
                {
                    CPLError(CE_Failure,  CPLE_AppDefined,
                            "Error while uncompressing sector for node " CPL_FRMT_GIB, id);
                    continue;
                    // FIXME ?
                }
            }

            nBucketOld = nBucket;
            nOffInBucketReducedOld = nOffInBucketReduced;
        }

        panReqIds[j] = id;
        memcpy(pasLonLatArray + j,
               pabySector + nOffInBucketReducedRemainer * sizeof(LonLat),
               sizeof(LonLat));

        if( pasLonLatArray[j].nLon || pasLonLatArray[j].nLat )
            j++;
    }
    nReqIds = j;
}

/************************************************************************/
/*                    LookupNodesCustomNonCompressedCase()              */
/************************************************************************/

void OGROSMDataSource::LookupNodesCustomNonCompressedCase()
{
    unsigned int i;
    unsigned int j = 0;

    for(i = 0; i < nReqIds; i++)
    {
        GIntBig id = panReqIds[i];

        int nBucket = (int)(id / NODE_PER_BUCKET);
        int nOffInBucket = id % NODE_PER_BUCKET;
        int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
        int nOffInBucketReducedRemainer = nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

        int nBitmapIndex = nOffInBucketReduced / 8;
        int nBitmapRemainer = nOffInBucketReduced % 8;

        if( nBucket >= nBuckets )
        {
            CPLError(CE_Failure,  CPLE_AppDefined,
                    "Cannot read node " CPL_FRMT_GIB, id);
            continue;
            // FIXME ?
        }
        Bucket* psBucket = &papsBuckets[nBucket];
        if( psBucket->u.pabyBitmap == NULL )
        {
            CPLError(CE_Failure,  CPLE_AppDefined,
                    "Cannot read node " CPL_FRMT_GIB, id);
            continue;
            // FIXME ?
        }

        int k;
        int nSector = 0;
        for(k = 0; k < nBitmapIndex; k++)
            nSector += abyBitsCount[psBucket->u.pabyBitmap[k]];
        if (nBitmapRemainer)
            nSector += abyBitsCount[psBucket->u.pabyBitmap[nBitmapIndex] & ((1 << nBitmapRemainer) - 1)];

        VSIFSeekL(fpNodes, psBucket->nOff + nSector * SECTOR_SIZE + nOffInBucketReducedRemainer * sizeof(LonLat), SEEK_SET);
        if( VSIFReadL(pasLonLatArray + j, 1, sizeof(LonLat), fpNodes) != sizeof(LonLat) )
        {
            CPLError(CE_Failure,  CPLE_AppDefined,
                     "Cannot read node " CPL_FRMT_GIB, id);
            // FIXME ?
        }
        else
        {
            panReqIds[j] = id;
            if( pasLonLatArray[j].nLon || pasLonLatArray[j].nLat )
                j++;
        }
    }
    nReqIds = j;
}

/************************************************************************/
/*                            WriteVarInt()                             */
/************************************************************************/

static void WriteVarInt(unsigned int nVal, GByte** ppabyData)
{
    GByte* pabyData = *ppabyData;
    while(TRUE)
    {
        if( (nVal & (~0x7f)) == 0 )
        {
            *pabyData = (GByte)nVal;
            *ppabyData = pabyData + 1;
            return;
        }

        *pabyData = 0x80 | (GByte)(nVal & 0x7f);
        nVal >>= 7;
        pabyData ++;
    }
}

/************************************************************************/
/*                           WriteVarInt64()                            */
/************************************************************************/

static void WriteVarInt64(GUIntBig nVal, GByte** ppabyData)
{
    GByte* pabyData = *ppabyData;
    while(TRUE)
    {
        if( (nVal & (~0x7f)) == 0 )
        {
            *pabyData = (GByte)nVal;
            *ppabyData = pabyData + 1;
            return;
        }

        *pabyData = 0x80 | (GByte)(nVal & 0x7f);
        nVal >>= 7;
        pabyData ++;
    }
}

/************************************************************************/
/*                           WriteVarSInt64()                           */
/************************************************************************/

static void WriteVarSInt64(GIntBig nSVal, GByte** ppabyData)
{
    GIntBig nVal;
    if( nSVal >= 0 )
        nVal = nSVal << 1;
    else
        nVal = ((-1-nSVal) << 1) + 1;

    GByte* pabyData = *ppabyData;
    while(TRUE)
    {
        if( (nVal & (~0x7f)) == 0 )
        {
            *pabyData = (GByte)nVal;
            *ppabyData = pabyData + 1;
            return;
        }

        *pabyData = 0x80 | (GByte)(nVal & 0x7f);
        nVal >>= 7;
        pabyData ++;
    }
}

/************************************************************************/
/*                             CompressWay()                            */
/************************************************************************/

int OGROSMDataSource::CompressWay ( unsigned int nTags, IndexedKVP* pasTags,
                                    int nPoints, LonLat* pasLonLatPairs,
                                    OSMInfo* psInfo,
                                    GByte* pabyCompressedWay )
{
    GByte* pabyPtr = pabyCompressedWay;
    pabyPtr ++;

    int nTagCount = 0;
    CPLAssert(nTags < MAX_COUNT_FOR_TAGS_IN_WAY);
    for(unsigned int iTag = 0; iTag < nTags; iTag++)
    {
        if ((int)(pabyPtr - pabyCompressedWay) + 2 >= MAX_SIZE_FOR_TAGS_IN_WAY)
        {
            break;
        }

        WriteVarInt(pasTags[iTag].nKeyIndex, &pabyPtr);

        /* to fit in 2 bytes, the theoretical limit would be 127 * 128 + 127 */
        if( pasTags[iTag].bVIsIndex )
        {
            if ((int)(pabyPtr - pabyCompressedWay) + 2 >= MAX_SIZE_FOR_TAGS_IN_WAY)
            {
                break;
            }

            WriteVarInt(pasTags[iTag].u.nValueIndex, &pabyPtr);
        }
        else
        {
            const char* pszV = (const char*)pabyNonRedundantValues +
                pasTags[iTag].u.nOffsetInpabyNonRedundantValues;

            int nLenV = strlen(pszV) + 1;
            if ((int)(pabyPtr - pabyCompressedWay) + 2 + nLenV >= MAX_SIZE_FOR_TAGS_IN_WAY)
            {
                break;
            }

            WriteVarInt(0, &pabyPtr);

            memcpy(pabyPtr, pszV, nLenV);
            pabyPtr += nLenV;
        }

        nTagCount ++;
    }

    pabyCompressedWay[0] = (GByte) nTagCount;

    if( bNeedsToSaveWayInfo )
    {
        if( psInfo != NULL )
        {
            *pabyPtr = 1;
            pabyPtr ++;

            WriteVarInt64(psInfo->ts.nTimeStamp, &pabyPtr);
            WriteVarInt64(psInfo->nChangeset, &pabyPtr);
            WriteVarInt(psInfo->nVersion, &pabyPtr);
            WriteVarInt(psInfo->nUID, &pabyPtr);
            // FIXME : do something with pszUserSID
        }
        else
        {
            *pabyPtr = 0;
            pabyPtr ++;
        }
    }

    memcpy(pabyPtr, &(pasLonLatPairs[0]), sizeof(LonLat));
    pabyPtr += sizeof(LonLat);
    for(int i=1;i<nPoints;i++)
    {
        GIntBig nDiff64;

        nDiff64 = (GIntBig)pasLonLatPairs[i].nLon - (GIntBig)pasLonLatPairs[i-1].nLon;
        WriteVarSInt64(nDiff64, &pabyPtr);

        nDiff64 = pasLonLatPairs[i].nLat - pasLonLatPairs[i-1].nLat;
        WriteVarSInt64(nDiff64, &pabyPtr);
    }
    int nBufferSize = (int)(pabyPtr - pabyCompressedWay);
    return nBufferSize;
}

/************************************************************************/
/*                             UncompressWay()                          */
/************************************************************************/

int OGROSMDataSource::UncompressWay( int nBytes, GByte* pabyCompressedWay,
                                     LonLat* pasCoords,
                                     unsigned int* pnTags, OSMTag* pasTags,
                                     OSMInfo* psInfo )
{
    GByte* pabyPtr = pabyCompressedWay;
    unsigned int nTags = *pabyPtr;
    pabyPtr ++;

    if (pnTags)
        *pnTags = nTags;

    /* TODO? : some additional safety checks */
    for(unsigned int iTag = 0; iTag < nTags; iTag++)
    {
        int nK = ReadVarInt32(&pabyPtr);
        int nV = ReadVarInt32(&pabyPtr);
        GByte* pszV = NULL;
        if( nV == 0 )
        {
            pszV = pabyPtr;
            while(*pabyPtr != '\0')
                pabyPtr ++;
            pabyPtr ++;
        }

        if( pasTags )
        {
            CPLAssert(nK >= 0 && nK < (int)asKeys.size());
            pasTags[iTag].pszK = asKeys[nK]->pszK;
            CPLAssert(nV == 0 || (nV > 0 && nV < (int)asKeys[nK]->asValues.size()));
            pasTags[iTag].pszV = nV ? asKeys[nK]->asValues[nV] : (const char*) pszV;
        }
    }

    if( bNeedsToSaveWayInfo )
    {
        if( *pabyPtr )
        {
            pabyPtr ++;

            OSMInfo sInfo;
            if( psInfo == NULL )
                psInfo = &sInfo;

            psInfo->ts.nTimeStamp = ReadVarInt64(&pabyPtr);
            psInfo->nChangeset = ReadVarInt64(&pabyPtr);
            psInfo->nVersion = ReadVarInt32(&pabyPtr);
            psInfo->nUID = ReadVarInt32(&pabyPtr);

            psInfo->bTimeStampIsStr = FALSE;
            psInfo->pszUserSID = ""; // FIXME
        }
        else
            pabyPtr ++;
    }

    memcpy(&pasCoords[0].nLon, pabyPtr, sizeof(int));
    memcpy(&pasCoords[0].nLat, pabyPtr + sizeof(int), sizeof(int));
    pabyPtr += 2 * sizeof(int);
    int nPoints = 1;
    do
    {
        pasCoords[nPoints].nLon = pasCoords[nPoints-1].nLon + ReadVarSInt64(&pabyPtr);
        pasCoords[nPoints].nLat = pasCoords[nPoints-1].nLat + ReadVarSInt64(&pabyPtr);

        nPoints ++;
    } while (pabyPtr < pabyCompressedWay + nBytes);

    return nPoints;
}

/************************************************************************/
/*                              IndexWay()                              */
/************************************************************************/

void OGROSMDataSource::IndexWay(GIntBig nWayID,
                                unsigned int nTags, IndexedKVP* pasTags,
                                LonLat* pasLonLatPairs, int nPairs,
                                OSMInfo* psInfo)
{
    if( !bIndexWays )
        return;

    sqlite3_bind_int64( hInsertWayStmt, 1, nWayID );

    int nBufferSize = CompressWay (nTags, pasTags, nPairs, pasLonLatPairs, psInfo, pabyWayBuffer);
    CPLAssert(nBufferSize <= WAY_BUFFER_SIZE);
    sqlite3_bind_blob( hInsertWayStmt, 2, pabyWayBuffer,
                       nBufferSize, SQLITE_STATIC );

    int rc = sqlite3_step( hInsertWayStmt );
    sqlite3_reset( hInsertWayStmt );
    if( !(rc == SQLITE_OK || rc == SQLITE_DONE) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Failed inserting way " CPL_FRMT_GIB ": %s",
                nWayID, sqlite3_errmsg(hDB));
    }
}

/************************************************************************/
/*                              FindNode()                              */
/************************************************************************/

int OGROSMDataSource::FindNode(GIntBig nID)
{
    int iFirst = 0;
    int iLast = nReqIds - 1;
    while(iFirst < iLast)
    {
        int iMid = (iFirst + iLast) / 2;
        if( nID > panReqIds[iMid])
            iFirst = iMid + 1;
        else
            iLast = iMid;
    }
    if( iFirst == iLast && nID == panReqIds[iFirst] )
        return iFirst;
    return -1;
}

/************************************************************************/
/*                         ProcessWaysBatch()                           */
/************************************************************************/

void OGROSMDataSource::ProcessWaysBatch()
{
    if( nWayFeaturePairs == 0 ) return;

    //printf("nodes = %d, features = %d\n", nUnsortedReqIds, nWayFeaturePairs);
    LookupNodes();

    int iPair;
    for(iPair = 0; iPair < nWayFeaturePairs; iPair ++)
    {
        WayFeaturePair* psWayFeaturePairs = &pasWayFeaturePairs[iPair];

        int bIsArea = psWayFeaturePairs->bIsArea;

        unsigned int nFound = 0;
        unsigned int i;

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
        if( bHashedIndexValid )
        {
            for(i=0;i<psWayFeaturePairs->nRefs;i++)
            {
                int nIndInHashArray =
                    HASH_ID_FUNC(psWayFeaturePairs->panNodeRefs[i]) %
                        HASHED_INDEXES_ARRAY_SIZE;
                int nIdx = panHashedIndexes[nIndInHashArray];
                if( nIdx < -1 )
                {
                    int iBucket = -nIdx - 2;
                    while( TRUE )
                    {
                        nIdx = psCollisionBuckets[iBucket].nInd;
                        if( panReqIds[nIdx] == psWayFeaturePairs->panNodeRefs[i] )
                            break;
                        iBucket = psCollisionBuckets[iBucket].nNext;
                        if( iBucket < 0 )
                        {
                            nIdx = -1;
                            break;
                        }
                    }
                }
                else if( nIdx >= 0 &&
                         panReqIds[nIdx] != psWayFeaturePairs->panNodeRefs[i] )
                    nIdx = -1;

                if (nIdx >= 0)
                {
                    pasLonLatCache[nFound].nLon = pasLonLatArray[nIdx].nLon;
                    pasLonLatCache[nFound].nLat = pasLonLatArray[nIdx].nLat;
                    nFound ++;
                }
            }
        }
        else
#endif // ENABLE_NODE_LOOKUP_BY_HASHING
        {
            int nIdx = -1;
            for(i=0;i<psWayFeaturePairs->nRefs;i++)
            {
                if( nIdx >= 0 && psWayFeaturePairs->panNodeRefs[i] ==
                                 psWayFeaturePairs->panNodeRefs[i-1] + 1 )
                {
                    if( nIdx+1 < (int)nReqIds && panReqIds[nIdx+1] ==
                                        psWayFeaturePairs->panNodeRefs[i] )
                        nIdx ++;
                    else
                        nIdx = -1;
                }
                else
                    nIdx = FindNode( psWayFeaturePairs->panNodeRefs[i] );
                if (nIdx >= 0)
                {
                    pasLonLatCache[nFound].nLon = pasLonLatArray[nIdx].nLon;
                    pasLonLatCache[nFound].nLat = pasLonLatArray[nIdx].nLat;
                    nFound ++;
                }
            }
        }

        if( nFound > 0 && bIsArea )
        {
            pasLonLatCache[nFound].nLon = pasLonLatCache[0].nLon;
            pasLonLatCache[nFound].nLat = pasLonLatCache[0].nLat;
            nFound ++;
        }

        if( nFound < 2 )
        {
            CPLDebug("OSM", "Way " CPL_FRMT_GIB " with %d nodes that could be found. Discarding it",
                    psWayFeaturePairs->nWayID, nFound);
            delete psWayFeaturePairs->poFeature;
            psWayFeaturePairs->poFeature = NULL;
            psWayFeaturePairs->bIsArea = FALSE;
            continue;
        }

        if( bIsArea && papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() )
        {
            IndexWay(psWayFeaturePairs->nWayID,
                     psWayFeaturePairs->nTags,
                     psWayFeaturePairs->pasTags,
                     pasLonLatCache, (int)nFound,
                     &psWayFeaturePairs->sInfo);
        }
        else
            IndexWay(psWayFeaturePairs->nWayID, 0, NULL,
                     pasLonLatCache, (int)nFound, NULL);

        if( psWayFeaturePairs->poFeature == NULL )
        {
            continue;
        }

        OGRLineString* poLS;
        OGRGeometry* poGeom;

        poLS = new OGRLineString();
        poGeom = poLS;

        poLS->setNumPoints((int)nFound);
        for(i=0;i<nFound;i++)
        {
            poLS->setPoint(i,
                        INT_TO_DBL(pasLonLatCache[i].nLon),
                        INT_TO_DBL(pasLonLatCache[i].nLat));
        }

        psWayFeaturePairs->poFeature->SetGeometryDirectly(poGeom);

        if( nFound != psWayFeaturePairs->nRefs )
            CPLDebug("OSM", "For way " CPL_FRMT_GIB ", got only %d nodes instead of %d",
                   psWayFeaturePairs->nWayID, nFound,
                   psWayFeaturePairs->nRefs);

        int bFilteredOut = FALSE;
        if( !papoLayers[IDX_LYR_LINES]->AddFeature(psWayFeaturePairs->poFeature,
                                                   psWayFeaturePairs->bAttrFilterAlreadyEvaluated,
                                                   &bFilteredOut,
                                                   !bFeatureAdded) )
            bStopParsing = TRUE;
        else if (!bFilteredOut)
            bFeatureAdded = TRUE;
    }

    if( papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() )
    {
        for(iPair = 0; iPair < nWayFeaturePairs; iPair ++)
        {
            WayFeaturePair* psWayFeaturePairs = &pasWayFeaturePairs[iPair];

            if( psWayFeaturePairs->bIsArea &&
                (psWayFeaturePairs->nTags || bReportAllWays) )
            {
                sqlite3_bind_int64( hInsertPolygonsStandaloneStmt , 1, psWayFeaturePairs->nWayID );

                int rc = sqlite3_step( hInsertPolygonsStandaloneStmt );
                sqlite3_reset( hInsertPolygonsStandaloneStmt );
                if( !(rc == SQLITE_OK || rc == SQLITE_DONE) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Failed inserting into polygons_standalone " CPL_FRMT_GIB ": %s",
                            psWayFeaturePairs->nWayID, sqlite3_errmsg(hDB));
                }
            }
        }
    }

    nWayFeaturePairs = 0;
    nUnsortedReqIds = 0;

    nAccumulatedTags = 0;
    nNonRedundantValuesLen = 0;
}

/************************************************************************/
/*                              NotifyWay()                             */
/************************************************************************/

void OGROSMDataSource::NotifyWay (OSMWay* psWay)
{
    unsigned int i;

    nWaysProcessed++;
    if( (nWaysProcessed % 10000) == 0 )
    {
        CPLDebug("OSM", "Ways processed : %d", nWaysProcessed);
#ifdef DEBUG_MEM_USAGE
        CPLDebug("OSM", "GetMaxTotalAllocs() = " CPL_FRMT_GUIB, (GUIntBig)GetMaxTotalAllocs());
#endif
    }

    if( !bUsePointsIndex )
        return;

    //printf("way %d : %d nodes\n", (int)psWay->nID, (int)psWay->nRefs);
    if( psWay->nRefs > MAX_NODES_PER_WAY )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Ways with more than %d nodes are not supported",
                 MAX_NODES_PER_WAY);
        return;
    }

    if( psWay->nRefs < 2 )
    {
        CPLDebug("OSM", "Way " CPL_FRMT_GIB " with %d nodes. Discarding it",
                 psWay->nID, psWay->nRefs);
        return;
    }

    /* Is a closed way a polygon ? */
    int bIsArea = FALSE;
    if( psWay->panNodeRefs[0] == psWay->panNodeRefs[psWay->nRefs - 1] )
    {
        for(i=0;i<psWay->nTags;i++)
        {
            const char* pszK = psWay->pasTags[i].pszK;
            if( strcmp(pszK, "area") == 0 )
            {
                if( strcmp(psWay->pasTags[i].pszV, "yes") == 0 )
                {
                    bIsArea = TRUE;
                }
                else if( strcmp(psWay->pasTags[i].pszV, "no") == 0 )
                {
                    bIsArea = FALSE;
                    break;
                }
            }
            else if( aoSetClosedWaysArePolygons.find(pszK) !=
                     aoSetClosedWaysArePolygons.end() )
            {
                bIsArea = TRUE;
            }
        }
    }

    OGRFeature* poFeature = NULL;

    int bInterestingTag = bReportAllWays;
    if( !bIsArea && !bReportAllWays )
    {
        for(i=0;i<psWay->nTags;i++)
        {
            const char* pszK = psWay->pasTags[i].pszK;
            if( papoLayers[IDX_LYR_LINES]->IsSignificantKey(pszK) )
            {
                bInterestingTag = TRUE;
                break;
            }
        }
    }

    int bAttrFilterAlreadyEvaluated = FALSE;
    if( !bIsArea && papoLayers[IDX_LYR_LINES]->IsUserInterested() && bInterestingTag )
    {
        poFeature = new OGRFeature(papoLayers[IDX_LYR_LINES]->GetLayerDefn());

        papoLayers[IDX_LYR_LINES]->SetFieldsFromTags(
            poFeature, psWay->nID, FALSE, psWay->nTags, psWay->pasTags, &psWay->sInfo );

        /* Optimization : if we have an attribute filter, that does not require geometry, */
        /* and if we don't need to index ways, then we can just evaluate the attribute */
        /* filter without the geometry */
        if( papoLayers[IDX_LYR_LINES]->HasAttributeFilter() &&
            !papoLayers[IDX_LYR_LINES]->AttributeFilterEvaluationNeedsGeometry() &&
            !bIndexWays )
        {
            if( !papoLayers[IDX_LYR_LINES]->EvaluateAttributeFilter(poFeature) )
            {
                delete poFeature;
                return;
            }
            bAttrFilterAlreadyEvaluated = TRUE;
        }
    }
    else if( !bIndexWays )
    {
        return;
    }

    if( nUnsortedReqIds + psWay->nRefs > MAX_ACCUMULATED_NODES ||
        nWayFeaturePairs == MAX_DELAYED_FEATURES ||
        nAccumulatedTags + psWay->nTags > MAX_ACCUMULATED_TAGS ||
        nNonRedundantValuesLen + 1024 > MAX_NON_REDUNDANT_VALUES )
    {
        ProcessWaysBatch();
    }

    WayFeaturePair* psWayFeaturePairs = &pasWayFeaturePairs[nWayFeaturePairs];

    psWayFeaturePairs->nWayID = psWay->nID;
    psWayFeaturePairs->nRefs = psWay->nRefs - bIsArea;
    psWayFeaturePairs->panNodeRefs = panUnsortedReqIds + nUnsortedReqIds;
    psWayFeaturePairs->poFeature = poFeature;
    psWayFeaturePairs->bIsArea = bIsArea;
    psWayFeaturePairs->bAttrFilterAlreadyEvaluated = bAttrFilterAlreadyEvaluated;

    if( bIsArea && papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() )
    {
        int nTagCount = 0;

        if( bNeedsToSaveWayInfo )
        {
            if( !psWay->sInfo.bTimeStampIsStr )
                psWayFeaturePairs->sInfo.ts.nTimeStamp =
                    psWay->sInfo.ts.nTimeStamp;
            else
            {
                int year, month, day, hour, minute, TZ;
                float second;
                if (OGRParseXMLDateTime(psWay->sInfo.ts.pszTimeStamp, &year, &month, &day,
                                        &hour, &minute, &second, &TZ))
                {
                    struct tm brokendown;
                    brokendown.tm_year = year - 1900;
                    brokendown.tm_mon = month - 1;
                    brokendown.tm_mday = day;
                    brokendown.tm_hour = hour;
                    brokendown.tm_min = minute;
                    brokendown.tm_sec = (int)(second + .5);
                    psWayFeaturePairs->sInfo.ts.nTimeStamp =
                        CPLYMDHMSToUnixTime(&brokendown);
                }
                else
                    psWayFeaturePairs->sInfo.ts.nTimeStamp = 0;
            }
            psWayFeaturePairs->sInfo.nChangeset = psWay->sInfo.nChangeset;
            psWayFeaturePairs->sInfo.nVersion = psWay->sInfo.nVersion;
            psWayFeaturePairs->sInfo.nUID = psWay->sInfo.nUID;
            psWayFeaturePairs->sInfo.bTimeStampIsStr = FALSE;
            psWayFeaturePairs->sInfo.pszUserSID = ""; // FIXME
        }
        else
        {
            psWayFeaturePairs->sInfo.ts.nTimeStamp = 0;
            psWayFeaturePairs->sInfo.nChangeset = 0;
            psWayFeaturePairs->sInfo.nVersion = 0;
            psWayFeaturePairs->sInfo.nUID = 0;
            psWayFeaturePairs->sInfo.bTimeStampIsStr = FALSE;
            psWayFeaturePairs->sInfo.pszUserSID = "";
        }

        psWayFeaturePairs->pasTags = pasAccumulatedTags + nAccumulatedTags;

        for(unsigned int iTag = 0; iTag < psWay->nTags; iTag++)
        {
            const char* pszK = psWay->pasTags[iTag].pszK;
            const char* pszV = psWay->pasTags[iTag].pszV;

            if (strcmp(pszK, "area") == 0)
                continue;
            if (strcmp(pszK, "created_by") == 0)
                continue;
            if (strcmp(pszK, "converted_by") == 0)
                continue;
            if (strcmp(pszK, "note") == 0)
                continue;
            if (strcmp(pszK, "todo") == 0)
                continue;
            if (strcmp(pszK, "fixme") == 0)
                continue;
            if (strcmp(pszK, "FIXME") == 0)
                continue;

            std::map<const char*, KeyDesc*, ConstCharComp>::iterator oIterK =
                aoMapIndexedKeys.find(pszK);
            KeyDesc* psKD;
            if (oIterK == aoMapIndexedKeys.end())
            {
                if( nNextKeyIndex >= 32768 ) /* somewhat arbitrary */
                {
                    if( nNextKeyIndex == 32768 )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Too many different keys in file");
                        nNextKeyIndex ++; /* to avoid next warnings */
                    }
                    continue;
                }
                psKD = new KeyDesc();
                psKD->pszK = CPLStrdup(pszK);
                psKD->nKeyIndex = nNextKeyIndex ++;
                //CPLDebug("OSM", "nNextKeyIndex=%d", nNextKeyIndex);
                psKD->nOccurences = 0;
                psKD->asValues.push_back(CPLStrdup(""));
                aoMapIndexedKeys[psKD->pszK] = psKD;
                asKeys.push_back(psKD);
            }
            else
                psKD = oIterK->second;
            psKD->nOccurences ++;

            pasAccumulatedTags[nAccumulatedTags].nKeyIndex = psKD->nKeyIndex;

            /* to fit in 2 bytes, the theoretical limit would be 127 * 128 + 127 */
            if( psKD->asValues.size() < 1024 )
            {
                std::map<const char*, int, ConstCharComp>::iterator oIterV;
                oIterV = psKD->anMapV.find(pszV);
                int nValueIndex;
                if (oIterV == psKD->anMapV.end())
                {
                    char* pszVDup = CPLStrdup(pszV);
                    nValueIndex = (int)psKD->asValues.size();
                    psKD->anMapV[pszVDup] = nValueIndex;
                    psKD->asValues.push_back(pszVDup);
                }
                else
                    nValueIndex = oIterV->second;

                pasAccumulatedTags[nAccumulatedTags].bVIsIndex = TRUE;
                pasAccumulatedTags[nAccumulatedTags].u.nValueIndex = nValueIndex;
            }
            else
            {
                int nLenV = strlen(pszV) + 1;

                if( psKD->asValues.size() == 1024 )
                {
                    CPLDebug("OSM", "More than %d different values for tag %s",
                            1024, pszK);
                    psKD->asValues.push_back(CPLStrdup(""));  /* to avoid next warnings */
                }

                CPLAssert( nNonRedundantValuesLen + nLenV <= MAX_NON_REDUNDANT_VALUES );
                memcpy(pabyNonRedundantValues + nNonRedundantValuesLen, pszV, nLenV);
                pasAccumulatedTags[nAccumulatedTags].bVIsIndex = FALSE;
                pasAccumulatedTags[nAccumulatedTags].u.nOffsetInpabyNonRedundantValues = nNonRedundantValuesLen;
                nNonRedundantValuesLen += nLenV;
            }
            nAccumulatedTags ++;

            nTagCount ++;
            if( nTagCount == MAX_COUNT_FOR_TAGS_IN_WAY )
                break;
        }

        psWayFeaturePairs->nTags = nTagCount;
    }
    else
    {
        psWayFeaturePairs->sInfo.ts.nTimeStamp = 0;
        psWayFeaturePairs->sInfo.nChangeset = 0;
        psWayFeaturePairs->sInfo.nVersion = 0;
        psWayFeaturePairs->sInfo.nUID = 0;
        psWayFeaturePairs->sInfo.bTimeStampIsStr = FALSE;
        psWayFeaturePairs->sInfo.pszUserSID = "";

        psWayFeaturePairs->nTags = 0;
        psWayFeaturePairs->pasTags = NULL;
    }

    nWayFeaturePairs++;

    memcpy(panUnsortedReqIds + nUnsortedReqIds, psWay->panNodeRefs, sizeof(GIntBig) * (psWay->nRefs - bIsArea));
    nUnsortedReqIds += (psWay->nRefs - bIsArea);
}

static void OGROSMNotifyWay (OSMWay* psWay, CPL_UNUSED OSMContext* psOSMContext, void* user_data)
{
    ((OGROSMDataSource*) user_data)->NotifyWay(psWay);
}

/************************************************************************/
/*                            LookupWays()                              */
/************************************************************************/

unsigned int OGROSMDataSource::LookupWays( std::map< GIntBig, std::pair<int,void*> >& aoMapWays,
                                           OSMRelation* psRelation )
{
    unsigned int nFound = 0;
    unsigned int iCur = 0;
    unsigned int i;

    while( iCur < psRelation->nMembers )
    {
        unsigned int nToQuery = 0;
        for(i=iCur;i<psRelation->nMembers;i++)
        {
            if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
                strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0 )
            {
                nToQuery ++;
                if( nToQuery == LIMIT_IDS_PER_REQUEST )
                    break;
            }
        }

        if( nToQuery == 0)
            break;

        unsigned int iLastI = (i == psRelation->nMembers) ? i : i + 1;

        sqlite3_stmt* hStmt = pahSelectWayStmt[nToQuery-1];
        unsigned int nBindIndex = 1;
        for(i=iCur;i<iLastI;i++)
        {
            if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
                strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0 )
            {
                sqlite3_bind_int64( hStmt, nBindIndex,
                                    psRelation->pasMembers[i].nID );
                nBindIndex ++;
            }
        }
        iCur = iLastI;

        while( sqlite3_step(hStmt) == SQLITE_ROW )
        {
            GIntBig id = sqlite3_column_int64(hStmt, 0);
            if( aoMapWays.find(id) == aoMapWays.end() )
            {
                int nBlobSize = sqlite3_column_bytes(hStmt, 1);
                const void* blob = sqlite3_column_blob(hStmt, 1);
                void* blob_dup = CPLMalloc(nBlobSize);
                memcpy(blob_dup, blob, nBlobSize);
                aoMapWays[id] = std::pair<int,void*>(nBlobSize, blob_dup);
            }
            nFound++;
        }

        sqlite3_reset(hStmt);
    }

    return nFound;
}

/************************************************************************/
/*                          BuildMultiPolygon()                         */
/************************************************************************/

OGRGeometry* OGROSMDataSource::BuildMultiPolygon(OSMRelation* psRelation,
                                                 unsigned int* pnTags,
                                                 OSMTag* pasTags)
{

    std::map< GIntBig, std::pair<int,void*> > aoMapWays;
    LookupWays( aoMapWays, psRelation );

    int bMissing = FALSE;
    unsigned int i;
    for(i = 0; i < psRelation->nMembers; i ++ )
    {
        if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
            strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0 )
        {
            if( aoMapWays.find( psRelation->pasMembers[i].nID ) == aoMapWays.end() )
            {
                CPLDebug("OSM", "Relation " CPL_FRMT_GIB " has missing ways. Ignoring it",
                        psRelation->nID);
                bMissing = TRUE;
                break;
            }
        }
    }

    OGRGeometry* poRet = NULL;
    OGRMultiLineString* poMLS = NULL;
    OGRGeometry** papoPolygons = NULL;
    int nPolys = 0;

    if( bMissing )
        goto cleanup;

    poMLS = new OGRMultiLineString();
    papoPolygons = (OGRGeometry**) CPLMalloc(
        sizeof(OGRGeometry*) *  psRelation->nMembers);
    nPolys = 0;

    if( pnTags != NULL )
        *pnTags = 0;

    for(i = 0; i < psRelation->nMembers; i ++ )
    {
        if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
            strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0  )
        {
            const std::pair<int, void*>& oGeom = aoMapWays[ psRelation->pasMembers[i].nID ];

            LonLat* pasCoords = (LonLat*) pasLonLatCache;
            int nPoints;

            if( pnTags != NULL && *pnTags == 0 &&
                strcmp(psRelation->pasMembers[i].pszRole, "outer") == 0 )
            {
                int nCompressedWaySize = oGeom.first;
                GByte* pabyCompressedWay = (GByte*) oGeom.second;

                memcpy(pabyWayBuffer, pabyCompressedWay, nCompressedWaySize);

                nPoints = UncompressWay (nCompressedWaySize, pabyWayBuffer,
                                         pasCoords,
                                         pnTags, pasTags, NULL );
            }
            else
            {
                nPoints = UncompressWay (oGeom.first, (GByte*) oGeom.second, pasCoords,
                                         NULL, NULL, NULL);
            }

            OGRLineString* poLS;

            if ( pasCoords[0].nLon == pasCoords[nPoints - 1].nLon &&
                 pasCoords[0].nLat == pasCoords[nPoints - 1].nLat )
            {
                OGRPolygon* poPoly = new OGRPolygon();
                OGRLinearRing* poRing = new OGRLinearRing();
                poPoly->addRingDirectly(poRing);
                papoPolygons[nPolys ++] = poPoly;
                poLS = poRing;

                if( strcmp(psRelation->pasMembers[i].pszRole, "outer") == 0 )
                {
                    sqlite3_bind_int64( hDeletePolygonsStandaloneStmt, 1, psRelation->pasMembers[i].nID );
                    sqlite3_step( hDeletePolygonsStandaloneStmt );
                    sqlite3_reset( hDeletePolygonsStandaloneStmt );
                }
            }
            else
            {
                poLS = new OGRLineString();
                poMLS->addGeometryDirectly(poLS);
            }

            poLS->setNumPoints(nPoints);
            for(int j=0;j<nPoints;j++)
            {
                poLS->setPoint( j,
                                INT_TO_DBL(pasCoords[j].nLon),
                                INT_TO_DBL(pasCoords[j].nLat) );
            }

        }
    }

    if( poMLS->getNumGeometries() > 0 )
    {
        OGRGeometryH hPoly = OGRBuildPolygonFromEdges( (OGRGeometryH) poMLS,
                                                        TRUE,
                                                        FALSE,
                                                        0,
                                                        NULL );
        if( hPoly != NULL && OGR_G_GetGeometryType(hPoly) == wkbPolygon )
        {
            OGRPolygon* poSuperPoly = (OGRPolygon* ) hPoly;
            for(i = 0; i < 1 + (unsigned int)poSuperPoly->getNumInteriorRings(); i++)
            {
                OGRPolygon* poPoly = new OGRPolygon();
                OGRLinearRing* poRing =  (i == 0) ? poSuperPoly->getExteriorRing() :
                                                    poSuperPoly->getInteriorRing(i - 1);
                if( poRing != NULL && poRing->getNumPoints() >= 4 &&
                    poRing->getX(0) == poRing->getX(poRing->getNumPoints() -1) &&
                    poRing->getY(0) == poRing->getY(poRing->getNumPoints() -1) )
                {
                    poPoly->addRing( poRing );
                    papoPolygons[nPolys ++] = poPoly;
                }
            }
        }

        OGR_G_DestroyGeometry(hPoly);
    }
    delete poMLS;

    if( nPolys > 0 )
    {
        int bIsValidGeometry = FALSE;
        const char* apszOptions[2] = { "METHOD=DEFAULT", NULL };
        OGRGeometry* poGeom = OGRGeometryFactory::organizePolygons(
            papoPolygons, nPolys, &bIsValidGeometry, apszOptions );

        if( poGeom != NULL && poGeom->getGeometryType() == wkbPolygon )
        {
            OGRMultiPolygon* poMulti = new OGRMultiPolygon();
            poMulti->addGeometryDirectly(poGeom);
            poGeom = poMulti;
        }

        if( poGeom != NULL && poGeom->getGeometryType() == wkbMultiPolygon )
        {
            poRet = poGeom;
        }
        else
        {
            CPLDebug("OSM", "Relation " CPL_FRMT_GIB ": Geometry has incompatible type : %s",
                     psRelation->nID,
                     poGeom != NULL ? OGR_G_GetGeometryName((OGRGeometryH)poGeom) : "null" );
            delete poGeom;
        }
    }

    CPLFree(papoPolygons);

cleanup:
    /* Cleanup */
    std::map< GIntBig, std::pair<int,void*> >::iterator oIter;
    for( oIter = aoMapWays.begin(); oIter != aoMapWays.end(); ++oIter )
        CPLFree(oIter->second.second);

    return poRet;
}

/************************************************************************/
/*                          BuildGeometryCollection()                   */
/************************************************************************/

OGRGeometry* OGROSMDataSource::BuildGeometryCollection(OSMRelation* psRelation, int bMultiLineString)
{
    std::map< GIntBig, std::pair<int,void*> > aoMapWays;
    LookupWays( aoMapWays, psRelation );

    unsigned int i;

    OGRGeometryCollection* poColl;
    if( bMultiLineString )
        poColl = new OGRMultiLineString();
    else
        poColl = new OGRGeometryCollection();

    for(i = 0; i < psRelation->nMembers; i ++ )
    {
        if( psRelation->pasMembers[i].eType == MEMBER_NODE && !bMultiLineString )
        {
            nUnsortedReqIds = 1;
            panUnsortedReqIds[0] = psRelation->pasMembers[i].nID;
            LookupNodes();
            if( nReqIds == 1 )
            {
                poColl->addGeometryDirectly(new OGRPoint(
                    INT_TO_DBL(pasLonLatArray[0].nLon),
                    INT_TO_DBL(pasLonLatArray[0].nLat)));
            }
        }
        else if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
                 strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0  &&
                 aoMapWays.find( psRelation->pasMembers[i].nID ) != aoMapWays.end() )
        {
            const std::pair<int, void*>& oGeom = aoMapWays[ psRelation->pasMembers[i].nID ];

            LonLat* pasCoords = (LonLat*) pasLonLatCache;
            int nPoints = UncompressWay (oGeom.first, (GByte*) oGeom.second, pasCoords, NULL, NULL, NULL);

            OGRLineString* poLS;

            poLS = new OGRLineString();
            poColl->addGeometryDirectly(poLS);

            poLS->setNumPoints(nPoints);
            for(int j=0;j<nPoints;j++)
            {
                poLS->setPoint( j,
                                INT_TO_DBL(pasCoords[j].nLon),
                                INT_TO_DBL(pasCoords[j].nLat) );
            }

        }
    }

    if( poColl->getNumGeometries() == 0 )
    {
        delete poColl;
        poColl = NULL;
    }

    /* Cleanup */
    std::map< GIntBig, std::pair<int,void*> >::iterator oIter;
    for( oIter = aoMapWays.begin(); oIter != aoMapWays.end(); ++oIter )
        CPLFree(oIter->second.second);

    return poColl;
}

/************************************************************************/
/*                            NotifyRelation()                          */
/************************************************************************/

void OGROSMDataSource::NotifyRelation (OSMRelation* psRelation)
{
    unsigned int i;

    if( nWayFeaturePairs != 0 )
        ProcessWaysBatch();

    nRelationsProcessed++;
    if( (nRelationsProcessed % 10000) == 0 )
    {
        CPLDebug("OSM", "Relations processed : %d", nRelationsProcessed);
#ifdef DEBUG_MEM_USAGE
        CPLDebug("OSM", "GetMaxTotalAllocs() = " CPL_FRMT_GUIB, (GUIntBig)GetMaxTotalAllocs());
#endif
    }

    if( !bUseWaysIndex )
        return;

    int bMultiPolygon = FALSE;
    int bMultiLineString = FALSE;
    int bInterestingTagFound = FALSE;
    const char* pszTypeV = NULL;
    for(i = 0; i < psRelation->nTags; i ++ )
    {
        const char* pszK = psRelation->pasTags[i].pszK;
        if( strcmp(pszK, "type") == 0 )
        {
            const char* pszV = psRelation->pasTags[i].pszV;
            pszTypeV = pszV;
            if( strcmp(pszV, "multipolygon") == 0 ||
                strcmp(pszV, "boundary") == 0)
            {
                bMultiPolygon = TRUE;
            }
            else if( strcmp(pszV, "multilinestring") == 0 ||
                     strcmp(pszV, "route") == 0 )
            {
                bMultiLineString = TRUE;
            }
        }
        else if ( strcmp(pszK, "created_by") != 0 )
            bInterestingTagFound = TRUE;
    }

    /* Optimization : if we have an attribute filter, that does not require geometry, */
    /* then we can just evaluate the attribute filter without the geometry */
    int iCurLayer = (bMultiPolygon) ?    IDX_LYR_MULTIPOLYGONS :
                    (bMultiLineString) ? IDX_LYR_MULTILINESTRINGS :
                                         IDX_LYR_OTHER_RELATIONS;
    if( !papoLayers[iCurLayer]->IsUserInterested() )
        return;

    OGRFeature* poFeature = NULL;

    if( !(bMultiPolygon && !bInterestingTagFound) && /* we cannot do early filtering for multipolygon that has no interesting tag, since we may fetch attributes from ways */
        papoLayers[iCurLayer]->HasAttributeFilter() &&
        !papoLayers[iCurLayer]->AttributeFilterEvaluationNeedsGeometry() )
    {
        poFeature = new OGRFeature(papoLayers[iCurLayer]->GetLayerDefn());

        papoLayers[iCurLayer]->SetFieldsFromTags( poFeature,
                                                  psRelation->nID,
                                                  FALSE,
                                                  psRelation->nTags,
                                                  psRelation->pasTags,
                                                  &psRelation->sInfo);

        if( !papoLayers[iCurLayer]->EvaluateAttributeFilter(poFeature) )
        {
            delete poFeature;
            return;
        }
    }

    OGRGeometry* poGeom;

    unsigned int nExtraTags = 0;
    OSMTag pasExtraTags[1 + MAX_COUNT_FOR_TAGS_IN_WAY];

    if( bMultiPolygon )
    {
        if( !bInterestingTagFound )
        {
            poGeom = BuildMultiPolygon(psRelation, &nExtraTags, pasExtraTags);
            CPLAssert(nExtraTags <= MAX_COUNT_FOR_TAGS_IN_WAY);
            pasExtraTags[nExtraTags].pszK = "type";
            pasExtraTags[nExtraTags].pszV = pszTypeV;
            nExtraTags ++;
        }
        else
            poGeom = BuildMultiPolygon(psRelation, NULL, NULL);
    }
    else
        poGeom = BuildGeometryCollection(psRelation, bMultiLineString);

    if( poGeom != NULL )
    {
        int bAttrFilterAlreadyEvaluated;
        if( poFeature == NULL )
        {
            poFeature = new OGRFeature(papoLayers[iCurLayer]->GetLayerDefn());

            papoLayers[iCurLayer]->SetFieldsFromTags( poFeature,
                                                      psRelation->nID,
                                                      FALSE,
                                                      nExtraTags ? nExtraTags : psRelation->nTags,
                                                      nExtraTags ? pasExtraTags : psRelation->pasTags,
                                                      &psRelation->sInfo);

            bAttrFilterAlreadyEvaluated = FALSE;
        }
        else
            bAttrFilterAlreadyEvaluated = TRUE;

        poFeature->SetGeometryDirectly(poGeom);

        int bFilteredOut = FALSE;
        if( !papoLayers[iCurLayer]->AddFeature( poFeature,
                                                bAttrFilterAlreadyEvaluated,
                                                &bFilteredOut,
                                                !bFeatureAdded ) )
            bStopParsing = TRUE;
        else if (!bFilteredOut)
            bFeatureAdded = TRUE;
    }
    else
        delete poFeature;
}

static void OGROSMNotifyRelation (OSMRelation* psRelation,
                                  CPL_UNUSED OSMContext* psOSMContext, void* user_data)
{
    ((OGROSMDataSource*) user_data)->NotifyRelation(psRelation);
}


/************************************************************************/
/*                      ProcessPolygonsStandalone()                     */
/************************************************************************/

void OGROSMDataSource::ProcessPolygonsStandalone()
{
    unsigned int nTags = 0;
    OSMTag pasTags[MAX_COUNT_FOR_TAGS_IN_WAY];
    OSMInfo sInfo;
    int bFirst = TRUE;

    sInfo.ts.nTimeStamp = 0;
    sInfo.nChangeset = 0;
    sInfo.nVersion = 0;
    sInfo.nUID = 0;
    sInfo.bTimeStampIsStr = FALSE;
    sInfo.pszUserSID = "";

    if( !bHasRowInPolygonsStandalone )
        bHasRowInPolygonsStandalone = (sqlite3_step(hSelectPolygonsStandaloneStmt) == SQLITE_ROW);

    while( bHasRowInPolygonsStandalone &&
           papoLayers[IDX_LYR_MULTIPOLYGONS]->nFeatureArraySize < 10000 )
    {
        if( bFirst )
        {
            CPLDebug("OSM", "Remaining standalone polygons");
            bFirst = FALSE;
        }

        GIntBig id = sqlite3_column_int64(hSelectPolygonsStandaloneStmt, 0);

        sqlite3_bind_int64( pahSelectWayStmt[0], 1, id );
        if( sqlite3_step(pahSelectWayStmt[0]) == SQLITE_ROW )
        {
            int nBlobSize = sqlite3_column_bytes(pahSelectWayStmt[0], 1);
            const void* blob = sqlite3_column_blob(pahSelectWayStmt[0], 1);

            LonLat* pasCoords = (LonLat*) pasLonLatCache;

            int nPoints = UncompressWay (nBlobSize, (GByte*) blob,
                                         pasCoords,
                                         &nTags, pasTags, &sInfo );
            CPLAssert(nTags <= MAX_COUNT_FOR_TAGS_IN_WAY);

            OGRLineString* poLS;

            OGRMultiPolygon* poMulti = new OGRMultiPolygon();
            OGRPolygon* poPoly = new OGRPolygon();
            OGRLinearRing* poRing = new OGRLinearRing();
            poMulti->addGeometryDirectly(poPoly);
            poPoly->addRingDirectly(poRing);
            poLS = poRing;

            poLS->setNumPoints(nPoints);
            for(int j=0;j<nPoints;j++)
            {
                poLS->setPoint( j,
                                INT_TO_DBL(pasCoords[j].nLon),
                                INT_TO_DBL(pasCoords[j].nLat) );
            }

            OGRFeature* poFeature = new OGRFeature(papoLayers[IDX_LYR_MULTIPOLYGONS]->GetLayerDefn());

            papoLayers[IDX_LYR_MULTIPOLYGONS]->SetFieldsFromTags( poFeature,
                                                                  id,
                                                                  TRUE,
                                                                  nTags,
                                                                  pasTags,
                                                                  &sInfo);

            poFeature->SetGeometryDirectly(poMulti);

            int bFilteredOut = FALSE;
            if( !papoLayers[IDX_LYR_MULTIPOLYGONS]->AddFeature( poFeature,
                                                    FALSE,
                                                    &bFilteredOut,
                                                    !bFeatureAdded ) )
            {
                bStopParsing = TRUE;
                break;
            }
            else if (!bFilteredOut)
                bFeatureAdded = TRUE;

        }
        else
            CPLAssert(FALSE);

        sqlite3_reset(pahSelectWayStmt[0]);

        bHasRowInPolygonsStandalone = (sqlite3_step(hSelectPolygonsStandaloneStmt) == SQLITE_ROW);
    }
}

/************************************************************************/
/*                             NotifyBounds()                           */
/************************************************************************/

void OGROSMDataSource::NotifyBounds (double dfXMin, double dfYMin,
                                     double dfXMax, double dfYMax)
{
    sExtent.MinX = dfXMin;
    sExtent.MinY = dfYMin;
    sExtent.MaxX = dfXMax;
    sExtent.MaxY = dfYMax;
    bExtentValid = TRUE;

    CPLDebug("OSM", "Got bounds : minx=%f, miny=%f, maxx=%f, maxy=%f",
             dfXMin, dfYMin, dfXMax, dfYMax);
}

static void OGROSMNotifyBounds( double dfXMin, double dfYMin,
                                double dfXMax, double dfYMax,
                                CPL_UNUSED OSMContext* psCtxt, void* user_data )
{
    ((OGROSMDataSource*) user_data)->NotifyBounds(dfXMin, dfYMin,
                                                  dfXMax, dfYMax);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROSMDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    const char* pszExt = CPLGetExtension(pszFilename);
    if( !EQUAL(pszExt, "pbf") &&
        !EQUAL(pszExt, "osm") &&
        !EQUALN(pszFilename, "/vsicurl_streaming/", strlen("/vsicurl_streaming/")) &&
        strcmp(pszFilename, "/vsistdin/") != 0 &&
        strcmp(pszFilename, "/dev/stdin/") != 0 )
        return FALSE;

    pszName = CPLStrdup( pszFilename );

    psParser = OSM_Open( pszName,
                         OGROSMNotifyNodes,
                         OGROSMNotifyWay,
                         OGROSMNotifyRelation,
                         OGROSMNotifyBounds,
                         this );
    if( psParser == NULL )
        return FALSE;

    if (bUpdateIn)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OGR/OSM driver does not support opening a file in update mode");
        return FALSE;
    }

    /* The following 4 config options are only usefull for debugging */
    bIndexPoints = CSLTestBoolean(CPLGetConfigOption("OSM_INDEX_POINTS", "YES"));
    bUsePointsIndex = CSLTestBoolean(CPLGetConfigOption("OSM_USE_POINTS_INDEX", "YES"));
    bIndexWays = CSLTestBoolean(CPLGetConfigOption("OSM_INDEX_WAYS", "YES"));
    bUseWaysIndex = CSLTestBoolean(CPLGetConfigOption("OSM_USE_WAYS_INDEX", "YES"));

    bCustomIndexing = CSLTestBoolean(CPLGetConfigOption("OSM_USE_CUSTOM_INDEXING", "YES"));
    if( !bCustomIndexing )
        CPLDebug("OSM", "Using SQLite indexing for points");
    bCompressNodes = CSLTestBoolean(CPLGetConfigOption("OSM_COMPRESS_NODES", "NO"));
    if( bCompressNodes )
        CPLDebug("OSM", "Using compression for nodes DB");

    nLayers = 5;
    papoLayers = (OGROSMLayer**) CPLMalloc(nLayers * sizeof(OGROSMLayer*));

    papoLayers[IDX_LYR_POINTS] = new OGROSMLayer(this, IDX_LYR_POINTS, "points");
    papoLayers[IDX_LYR_POINTS]->GetLayerDefn()->SetGeomType(wkbPoint);

    papoLayers[IDX_LYR_LINES] = new OGROSMLayer(this, IDX_LYR_LINES, "lines");
    papoLayers[IDX_LYR_LINES]->GetLayerDefn()->SetGeomType(wkbLineString);

    papoLayers[IDX_LYR_MULTILINESTRINGS] = new OGROSMLayer(this, IDX_LYR_MULTILINESTRINGS, "multilinestrings");
    papoLayers[IDX_LYR_MULTILINESTRINGS]->GetLayerDefn()->SetGeomType(wkbMultiLineString);

    papoLayers[IDX_LYR_MULTIPOLYGONS] = new OGROSMLayer(this, IDX_LYR_MULTIPOLYGONS, "multipolygons");
    papoLayers[IDX_LYR_MULTIPOLYGONS]->GetLayerDefn()->SetGeomType(wkbMultiPolygon);

    papoLayers[IDX_LYR_OTHER_RELATIONS] = new OGROSMLayer(this, IDX_LYR_OTHER_RELATIONS, "other_relations");
    papoLayers[IDX_LYR_OTHER_RELATIONS]->GetLayerDefn()->SetGeomType(wkbGeometryCollection);

    if( !ParseConf() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not parse configuration file for OSM import");
        return FALSE;
    }

    bNeedsToSaveWayInfo =
        ( papoLayers[IDX_LYR_MULTIPOLYGONS]->HasTimestamp() ||
          papoLayers[IDX_LYR_MULTIPOLYGONS]->HasChangeset() ||
          papoLayers[IDX_LYR_MULTIPOLYGONS]->HasVersion() ||
          papoLayers[IDX_LYR_MULTIPOLYGONS]->HasUID() ||
          papoLayers[IDX_LYR_MULTIPOLYGONS]->HasUser() );

    pasLonLatCache = (LonLat*)VSIMalloc(MAX_NODES_PER_WAY * sizeof(LonLat));
    pabyWayBuffer = (GByte*)VSIMalloc(WAY_BUFFER_SIZE);

    panReqIds = (GIntBig*)VSIMalloc(MAX_ACCUMULATED_NODES * sizeof(GIntBig));
#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    panHashedIndexes = (int*)VSIMalloc(HASHED_INDEXES_ARRAY_SIZE * sizeof(int));
    psCollisionBuckets = (CollisionBucket*)VSIMalloc(COLLISION_BUCKET_ARRAY_SIZE * sizeof(CollisionBucket));
#endif
    pasLonLatArray = (LonLat*)VSIMalloc(MAX_ACCUMULATED_NODES * sizeof(LonLat));
    panUnsortedReqIds = (GIntBig*)VSIMalloc(MAX_ACCUMULATED_NODES * sizeof(GIntBig));
    pasWayFeaturePairs = (WayFeaturePair*)VSIMalloc(MAX_DELAYED_FEATURES * sizeof(WayFeaturePair));
    pasAccumulatedTags = (IndexedKVP*) VSIMalloc(MAX_ACCUMULATED_TAGS * sizeof(IndexedKVP));
    pabyNonRedundantValues = (GByte*) VSIMalloc(MAX_NON_REDUNDANT_VALUES);

    if( pasLonLatCache == NULL ||
        pabyWayBuffer == NULL ||
        panReqIds == NULL ||
        pasLonLatArray == NULL ||
        panUnsortedReqIds == NULL ||
        pasWayFeaturePairs == NULL ||
        pasAccumulatedTags == NULL ||
        pabyNonRedundantValues == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out-of-memory when allocating one of the buffer used for the processing.");
        return FALSE;
    }

    nMaxSizeForInMemoryDBInMB = atoi(CPLGetConfigOption("OSM_MAX_TMPFILE_SIZE", "100"));
    GIntBig nSize = (GIntBig)nMaxSizeForInMemoryDBInMB * 1024 * 1024;
    if (nSize < 0 || (GIntBig)(size_t)nSize != nSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for OSM_MAX_TMPFILE_SIZE. Using 100 instead.");
        nMaxSizeForInMemoryDBInMB = 100;
        nSize = (GIntBig)nMaxSizeForInMemoryDBInMB * 1024 * 1024;
    }

    if( bCustomIndexing )
    {
        pabySector = (GByte*) VSICalloc(1, SECTOR_SIZE);

        if( pabySector == NULL || !AllocMoreBuckets(INIT_BUCKET_COUNT) )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                    "Out-of-memory when allocating one of the buffer used for the processing.");
            return FALSE;
        }

        bInMemoryNodesFile = TRUE;
        osNodesFilename.Printf("/vsimem/osm_importer/osm_temp_nodes_%p", this);
        fpNodes = VSIFOpenL(osNodesFilename, "wb+");
        if( fpNodes == NULL )
        {
            return FALSE;
        }

        CPLPushErrorHandler(CPLQuietErrorHandler);
        int bSuccess = VSIFSeekL(fpNodes, (vsi_l_offset) (nSize * 3 / 4), SEEK_SET) == 0;
        CPLPopErrorHandler();

        if( bSuccess )
        {
            VSIFSeekL(fpNodes, 0, SEEK_SET);
            VSIFTruncateL(fpNodes, 0);
        }
        else
        {
            CPLDebug("OSM", "Not enough memory for in-memory file. Using disk temporary file instead.");

            VSIFCloseL(fpNodes);
            fpNodes = NULL;
            VSIUnlink(osNodesFilename);

            bInMemoryNodesFile = FALSE;
            osNodesFilename = CPLGenerateTempFilename("osm_tmp_nodes");

            fpNodes = VSIFOpenL(osNodesFilename, "wb+");
            if( fpNodes == NULL )
            {
                return FALSE;
            }

            /* On Unix filesystems, you can remove a file even if it */
            /* opened */
            const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if( EQUAL(pszVal, "YES") )
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                bMustUnlinkNodesFile = VSIUnlink( osNodesFilename ) != 0;
                CPLPopErrorHandler();
            }

            return FALSE;
        }
    }

    int bRet = CreateTempDB();
    if( bRet )
    {
        CPLString osInterestLayers = GetInterestLayersForDSName(GetName());
        if( osInterestLayers.size() )
        {
            ExecuteSQL( osInterestLayers, NULL, NULL );
        }
    }
    return bRet;
}

/************************************************************************/
/*                             CreateTempDB()                           */
/************************************************************************/

int OGROSMDataSource::CreateTempDB()
{
    char* pszErrMsg = NULL;

    int rc = 0;
    int bIsExisting = FALSE;
    int bSuccess = FALSE;

#ifdef HAVE_SQLITE_VFS
    const char* pszExistingTmpFile = CPLGetConfigOption("OSM_EXISTING_TMPFILE", NULL);
    if ( pszExistingTmpFile != NULL )
    {
        bSuccess = TRUE;
        bIsExisting = TRUE;
        rc = sqlite3_open_v2( pszExistingTmpFile, &hDB,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                              NULL );
    }
    else
    {
        osTmpDBName.Printf("/vsimem/osm_importer/osm_temp_%p.sqlite", this);

        /* On 32 bit, the virtual memory space is scarce, so we need to reserve it right now */
        /* Won't hurt on 64 bit either. */
        VSILFILE* fp = VSIFOpenL(osTmpDBName, "wb");
        if( fp )
        {
            GIntBig nSize = (GIntBig)nMaxSizeForInMemoryDBInMB * 1024 * 1024;
            if( bCustomIndexing && bInMemoryNodesFile )
                nSize = nSize * 1 / 4;

            CPLPushErrorHandler(CPLQuietErrorHandler);
            bSuccess = VSIFSeekL(fp, (vsi_l_offset) nSize, SEEK_SET) == 0;
            CPLPopErrorHandler();

            if( bSuccess )
                 VSIFTruncateL(fp, 0);

            VSIFCloseL(fp);

            if( !bSuccess )
            {
                CPLDebug("OSM", "Not enough memory for in-memory file. Using disk temporary file instead.");
                VSIUnlink(osTmpDBName);
            }
        }

        if( bSuccess )
        {
            bInMemoryTmpDB = TRUE;
            pMyVFS = OGRSQLiteCreateVFS(NULL, this);
            sqlite3_vfs_register(pMyVFS, 0);
            rc = sqlite3_open_v2( osTmpDBName.c_str(), &hDB,
                                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                                pMyVFS->zName );
        }
    }
#endif

    if( !bSuccess )
    {
        osTmpDBName = CPLGenerateTempFilename("osm_tmp");
        rc = sqlite3_open( osTmpDBName.c_str(), &hDB );

        /* On Unix filesystems, you can remove a file even if it */
        /* opened */
        if( rc == SQLITE_OK )
        {
            const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if( EQUAL(pszVal, "YES") )
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                bMustUnlink = VSIUnlink( osTmpDBName ) != 0;
                CPLPopErrorHandler();
            }
        }
    }

    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "sqlite3_open(%s) failed: %s",
                  osTmpDBName.c_str(), sqlite3_errmsg( hDB ) );
        return FALSE;
    }

    if( !SetDBOptions() )
    {
        return FALSE;
    }

    if( !bIsExisting )
    {
        rc = sqlite3_exec( hDB,
                        "CREATE TABLE nodes (id INTEGER PRIMARY KEY, coords BLOB)",
                        NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to create table nodes : %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return FALSE;
        }

        rc = sqlite3_exec( hDB,
                        "CREATE TABLE ways (id INTEGER PRIMARY KEY, data BLOB)",
                        NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to create table ways : %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return FALSE;
        }

        rc = sqlite3_exec( hDB,
                        "CREATE TABLE polygons_standalone (id INTEGER PRIMARY KEY)",
                        NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to create table polygons_standalone : %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return FALSE;
        }
    }

    return CreatePreparedStatements();
}
/************************************************************************/
/*                            SetDBOptions()                            */
/************************************************************************/

int OGROSMDataSource::SetDBOptions()
{
    char* pszErrMsg = NULL;
    int rc;

    rc = sqlite3_exec( hDB, "PRAGMA synchronous = OFF", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA synchronous : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    rc = sqlite3_exec( hDB, "PRAGMA journal_mode = OFF", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA journal_mode : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    rc = sqlite3_exec( hDB, "PRAGMA temp_store = MEMORY", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA temp_store : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    if( !SetCacheSize() )
        return FALSE;

    if( !StartTransaction() )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                              SetCacheSize()                          */
/************************************************************************/

int OGROSMDataSource::SetCacheSize()
{
    int rc;
    const char* pszSqliteCacheMB = CPLGetConfigOption("OSM_SQLITE_CACHE", NULL);
    if (pszSqliteCacheMB != NULL)
    {
        char* pszErrMsg = NULL;
        char **papszResult;
        int nRowCount, nColCount;
        int iSqliteCachePages;
        int iSqlitePageSize = -1;
        int iSqliteCacheBytes = atoi( pszSqliteCacheMB ) * 1024 * 1024;

        /* querying the current PageSize */
        rc = sqlite3_get_table( hDB, "PRAGMA page_size",
                                &papszResult, &nRowCount, &nColCount,
                                &pszErrMsg );
        if( rc == SQLITE_OK )
        {
            int iRow;
            for (iRow = 1; iRow <= nRowCount; iRow++)
            {
                iSqlitePageSize = atoi( papszResult[(iRow * nColCount) + 0] );
            }
            sqlite3_free_table(papszResult);
        }
        if( iSqlitePageSize < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to run PRAGMA page_size : %s",
                      pszErrMsg ? pszErrMsg : sqlite3_errmsg(hDB) );
            sqlite3_free( pszErrMsg );
            return TRUE;
        }

        /* computing the CacheSize as #Pages */
        iSqliteCachePages = iSqliteCacheBytes / iSqlitePageSize;
        if( iSqliteCachePages <= 0)
            return TRUE;

        rc = sqlite3_exec( hDB, CPLSPrintf( "PRAGMA cache_size = %d",
                                            iSqliteCachePages ),
                           NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unrecognized value for PRAGMA cache_size : %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            rc = SQLITE_OK;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                        CreatePreparedStatements()                    */
/************************************************************************/

int OGROSMDataSource::CreatePreparedStatements()
{
    int rc;

    rc = sqlite3_prepare( hDB, "INSERT INTO nodes (id, coords) VALUES (?,?)", -1,
                          &hInsertNodeStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
        return FALSE;
    }

    pahSelectNodeStmt = (sqlite3_stmt**) CPLCalloc(sizeof(sqlite3_stmt*), LIMIT_IDS_PER_REQUEST);

    char szTmp[LIMIT_IDS_PER_REQUEST*2 + 128];
    strcpy(szTmp, "SELECT id, coords FROM nodes WHERE id IN (");
    int nLen = strlen(szTmp);
    for(int i=0;i<LIMIT_IDS_PER_REQUEST;i++)
    {
        if(i == 0)
        {
            strcpy(szTmp + nLen, "?) ORDER BY id ASC");
            nLen += 2;
        }
        else
        {
            strcpy(szTmp + nLen -1, ",?) ORDER BY id ASC");
            nLen += 2;
        }
        rc = sqlite3_prepare( hDB, szTmp, -1, &pahSelectNodeStmt[i], NULL );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
            return FALSE;
        }
    }

    rc = sqlite3_prepare( hDB, "INSERT INTO ways (id, data) VALUES (?,?)", -1,
                          &hInsertWayStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
        return FALSE;
    }

    pahSelectWayStmt = (sqlite3_stmt**) CPLCalloc(sizeof(sqlite3_stmt*), LIMIT_IDS_PER_REQUEST);

    strcpy(szTmp, "SELECT id, data FROM ways WHERE id IN (");
    nLen = strlen(szTmp);
    for(int i=0;i<LIMIT_IDS_PER_REQUEST;i++)
    {
        if(i == 0)
        {
            strcpy(szTmp + nLen, "?)");
            nLen += 2;
        }
        else
        {
            strcpy(szTmp + nLen -1, ",?)");
            nLen += 2;
        }
        rc = sqlite3_prepare( hDB, szTmp, -1, &pahSelectWayStmt[i], NULL );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
            return FALSE;
        }
    }

    rc = sqlite3_prepare( hDB, "INSERT INTO polygons_standalone (id) VALUES (?)", -1,
                          &hInsertPolygonsStandaloneStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
        return FALSE;
    }

    rc = sqlite3_prepare( hDB, "DELETE FROM polygons_standalone WHERE id = ?", -1,
                          &hDeletePolygonsStandaloneStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
        return FALSE;
    }

    rc = sqlite3_prepare( hDB, "SELECT id FROM polygons_standalone ORDER BY id", -1,
                          &hSelectPolygonsStandaloneStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "sqlite3_prepare() failed :  %s", sqlite3_errmsg(hDB) );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

int OGROSMDataSource::StartTransaction()
{
    if( bInTransaction )
        return FALSE;

    char* pszErrMsg = NULL;
    int rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to start transaction : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    bInTransaction = TRUE;

    return TRUE;
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

int OGROSMDataSource::CommitTransaction()
{
    if( !bInTransaction )
        return FALSE;

    bInTransaction = FALSE;

    char* pszErrMsg = NULL;
    int rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to commit transaction : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           ParseConf()                                */
/************************************************************************/

int OGROSMDataSource::ParseConf()
{
    const char *pszFilename = CPLGetConfigOption("OSM_CONFIG_FILE", NULL);
    if( pszFilename == NULL )
        pszFilename = CPLFindFile( "gdal", "osmconf.ini" );
    if( pszFilename == NULL )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Cannot find osmconf.ini configuration file");
        return FALSE;
    }

    VSILFILE* fpConf = VSIFOpenL(pszFilename, "rb");
    if( fpConf == NULL )
        return FALSE;

    const char* pszLine;
    int iCurLayer = -1;

    int i;

    while((pszLine = CPLReadLine2L(fpConf, -1, NULL)) != NULL)
    {
        if(pszLine[0] == '[' && pszLine[strlen(pszLine)-1] == ']' )
        {
            iCurLayer = -1;
            pszLine ++;
            ((char*)pszLine)[strlen(pszLine)-1] = '\0'; /* Evil but OK */
            for(i = 0; i < nLayers; i++)
            {
                if( strcmp(pszLine, papoLayers[i]->GetName()) == 0 )
                {
                    iCurLayer = i;
                    break;
                }
            }
            if( iCurLayer < 0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer '%s' mentionned in %s is unknown to the driver",
                         pszLine, pszFilename);
            }
            continue;
        }

        if( strncmp(pszLine, "closed_ways_are_polygons=",
                    strlen("closed_ways_are_polygons=")) == 0)
        {
            char** papszTokens = CSLTokenizeString2(pszLine, "=", 0);
            if( CSLCount(papszTokens) == 2)
            {
                char** papszTokens2 = CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    aoSetClosedWaysArePolygons.insert(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            CSLDestroy(papszTokens);
        }

        else if(strncmp(pszLine, "report_all_nodes=", strlen("report_all_nodes=")) == 0)
        {
            if( strcmp(pszLine + strlen("report_all_nodes="), "no") == 0 )
            {
                bReportAllNodes = FALSE;
            }
            else if( strcmp(pszLine + strlen("report_all_nodes="), "yes") == 0 )
            {
                bReportAllNodes = TRUE;
            }
        }

        else if(strncmp(pszLine, "report_all_ways=", strlen("report_all_ways=")) == 0)
        {
            if( strcmp(pszLine + strlen("report_all_ways="), "no") == 0 )
            {
                bReportAllWays = FALSE;
            }
            else if( strcmp(pszLine + strlen("report_all_ways="), "yes") == 0 )
            {
                bReportAllWays = TRUE;
            }
        }

        else if(strncmp(pszLine, "attribute_name_laundering=", strlen("attribute_name_laundering=")) == 0)
        {
            if( strcmp(pszLine + strlen("attribute_name_laundering="), "no") == 0 )
            {
                bAttributeNameLaundering = FALSE;
            }
            else if( strcmp(pszLine + strlen("attribute_name_laundering="), "yes") == 0 )
            {
                bAttributeNameLaundering = TRUE;
            }
        }

        else if( iCurLayer >= 0 )
        {
            char** papszTokens = CSLTokenizeString2(pszLine, "=", 0);
            if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "other_tags") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasOtherTags(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                    papoLayers[iCurLayer]->SetHasOtherTags(TRUE);
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "all_tags") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasAllTags(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                    papoLayers[iCurLayer]->SetHasAllTags(TRUE);
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_id") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasOSMId(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasOSMId(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_id", OFTString);

                    if( iCurLayer == IDX_LYR_MULTIPOLYGONS )
                        papoLayers[iCurLayer]->AddField("osm_way_id", OFTString);
                }
            }
             else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_version") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasVersion(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasVersion(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_version", OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_timestamp") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasTimestamp(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasTimestamp(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_timestamp", OFTDateTime);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_uid") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasUID(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasUID(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_uid", OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_user") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasUser(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasUser(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_user", OFTString);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_changeset") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasChangeset(FALSE);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasChangeset(TRUE);
                    papoLayers[iCurLayer]->AddField("osm_changeset", OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "attributes") == 0 )
            {
                char** papszTokens2 = CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->AddField(papszTokens2[i], OFTString);
                }
                CSLDestroy(papszTokens2);
            }
            else if ( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "unsignificant") == 0 )
            {
                char** papszTokens2 = CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->AddUnsignificantKey(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            else if ( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "ignore") == 0 )
            {
                char** papszTokens2 = CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->AddIgnoreKey(papszTokens2[i]);
                    papoLayers[iCurLayer]->AddWarnKey(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            CSLDestroy(papszTokens);
        }
    }

    for(i=0;i<nLayers;i++)
    {
        if( papoLayers[i]->HasAllTags() )
        {
            papoLayers[i]->AddField("all_tags", OFTString);
            if( papoLayers[i]->HasOtherTags() )
            {
                papoLayers[i]->SetHasOtherTags(FALSE);
            }
        }
        else if( papoLayers[i]->HasOtherTags() )
            papoLayers[i]->AddField("other_tags", OFTString);
    }

    VSIFCloseL(fpConf);

    return TRUE;
}

/************************************************************************/
/*                           ResetReading()                            */
/************************************************************************/

int OGROSMDataSource::ResetReading()
{
    if( hDB == NULL )
        return FALSE;
    if( bCustomIndexing && fpNodes == NULL )
        return FALSE;

    OSM_ResetReading(psParser);

    char* pszErrMsg = NULL;
    int rc = sqlite3_exec( hDB, "DELETE FROM nodes", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to DELETE FROM nodes : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    rc = sqlite3_exec( hDB, "DELETE FROM ways", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to DELETE FROM ways : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }

    rc = sqlite3_exec( hDB, "DELETE FROM polygons_standalone", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to DELETE FROM polygons_standalone : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }
    bHasRowInPolygonsStandalone = FALSE;

    {
        int i;
        for( i = 0; i < nWayFeaturePairs; i++)
        {
            delete pasWayFeaturePairs[i].poFeature;
        }
        nWayFeaturePairs = 0;
        nUnsortedReqIds = 0;
        nReqIds = 0;
        nAccumulatedTags = 0;
        nNonRedundantValuesLen = 0;

        for(i=0;i<(int)asKeys.size();i++)
        {
            KeyDesc* psKD = asKeys[i];
            CPLFree(psKD->pszK);
            for(int j=0;j<(int)psKD->asValues.size();j++)
                CPLFree(psKD->asValues[j]);
            delete psKD;
        }
        asKeys.resize(0);
        aoMapIndexedKeys.clear();
        nNextKeyIndex = 0;
    }

    if( bCustomIndexing )
    {
        nPrevNodeId = -1;
        nBucketOld = -1;
        nOffInBucketReducedOld = -1;

        VSIFSeekL(fpNodes, 0, SEEK_SET);
        VSIFTruncateL(fpNodes, 0);
        nNodesFileSize = 0;

        memset(pabySector, 0, SECTOR_SIZE);
        for(int i = 0; i < nBuckets; i++)
        {
            papsBuckets[i].nOff = -1;
            if( bCompressNodes )
            {
                if( papsBuckets[i].u.panSectorSize )
                    memset(papsBuckets[i].u.panSectorSize, 0, BUCKET_SECTOR_SIZE_ARRAY_SIZE);
            }
            else
            {
                if( papsBuckets[i].u.pabyBitmap )
                    memset(papsBuckets[i].u.pabyBitmap, 0, BUCKET_BITMAP_SIZE);
            }
        }
    }

    for(int i=0;i<nLayers;i++)
    {
        papoLayers[i]->ForceResetReading();
    }

    bStopParsing = FALSE;

    return TRUE;
}

/************************************************************************/
/*                           ParseNextChunk()                           */
/************************************************************************/

int OGROSMDataSource::ParseNextChunk(int nIdxLayer)
{
    if( bStopParsing )
        return FALSE;

    bHasParsedFirstChunk = TRUE;
    bFeatureAdded = FALSE;
    while( TRUE )
    {
#ifdef DEBUG_MEM_USAGE
        static int counter = 0;
        counter ++;
        if ((counter % 1000) == 0)
            CPLDebug("OSM", "GetMaxTotalAllocs() = " CPL_FRMT_GUIB, (GUIntBig)GetMaxTotalAllocs());
#endif

        OSMRetCode eRet = OSM_ProcessBlock(psParser);
        if( eRet == OSM_EOF || eRet == OSM_ERROR )
        {
            if( eRet == OSM_EOF )
            {
                if( nWayFeaturePairs != 0 )
                    ProcessWaysBatch();

                ProcessPolygonsStandalone();

                if( !bHasRowInPolygonsStandalone )
                    bStopParsing = TRUE;

                if( !bInterleavedReading && !bFeatureAdded &&
                    bHasRowInPolygonsStandalone &&
                    nIdxLayer != IDX_LYR_MULTIPOLYGONS )
                {
                    return FALSE;
                }

                return bFeatureAdded || bHasRowInPolygonsStandalone;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "An error occured during the parsing of data around byte " CPL_FRMT_GUIB,
                         OSM_GetBytesRead(psParser));

                bStopParsing = TRUE;
                return FALSE;
            }
        }
        else
        {
            if( bInMemoryTmpDB )
            {
                if( !TransferToDiskIfNecesserary() )
                    return FALSE;
            }

            if( bFeatureAdded )
                break;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                    TransferToDiskIfNecesserary()                     */
/************************************************************************/

int OGROSMDataSource::TransferToDiskIfNecesserary()
{
    if( bInMemoryNodesFile )
    {
        if( nNodesFileSize / 1024 / 1024 > 3 * nMaxSizeForInMemoryDBInMB / 4 )
        {
            bInMemoryNodesFile = FALSE;

            VSIFCloseL(fpNodes);
            fpNodes = NULL;

            CPLString osNewTmpDBName;
            osNewTmpDBName = CPLGenerateTempFilename("osm_tmp_nodes");

            CPLDebug("OSM", "%s too big for RAM. Transfering it onto disk in %s",
                     osNodesFilename.c_str(), osNewTmpDBName.c_str());

            if( CPLCopyFile( osNewTmpDBName, osNodesFilename ) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot copy %s to %s",
                         osNodesFilename.c_str(), osNewTmpDBName.c_str() );
                VSIUnlink(osNewTmpDBName);
                bStopParsing = TRUE;
                return FALSE;
            }

            VSIUnlink(osNodesFilename);

            if( bInMemoryTmpDB )
            {
                /* Try to grow the sqlite in memory-db to the full space now */
                /* it has been freed. */
                VSILFILE* fp = VSIFOpenL(osTmpDBName, "rb+");
                if( fp )
                {
                    VSIFSeekL(fp, 0, SEEK_END);
                    vsi_l_offset nCurSize = VSIFTellL(fp);
                    GIntBig nNewSize = ((GIntBig)nMaxSizeForInMemoryDBInMB) * 1024 * 1024;
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    int bSuccess = VSIFSeekL(fp, (vsi_l_offset) nNewSize, SEEK_SET) == 0;
                    CPLPopErrorHandler();

                    if( bSuccess )
                        VSIFTruncateL(fp, nCurSize);

                    VSIFCloseL(fp);
                }
            }

            osNodesFilename = osNewTmpDBName;

            fpNodes = VSIFOpenL(osNodesFilename, "rb+");
            if( fpNodes == NULL )
            {
                bStopParsing = TRUE;
                return FALSE;
            }

            VSIFSeekL(fpNodes, 0, SEEK_END);

            /* On Unix filesystems, you can remove a file even if it */
            /* opened */
            const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if( EQUAL(pszVal, "YES") )
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                bMustUnlinkNodesFile = VSIUnlink( osNodesFilename ) != 0;
                CPLPopErrorHandler();
            }
        }
    }

    if( bInMemoryTmpDB )
    {
        VSIStatBufL sStat;

        int nLimitMB = nMaxSizeForInMemoryDBInMB;
        if( bCustomIndexing && bInMemoryNodesFile )
            nLimitMB = nLimitMB * 1 / 4;

        if( VSIStatL( osTmpDBName, &sStat ) == 0 &&
            sStat.st_size / 1024 / 1024 > nLimitMB )
        {
            bInMemoryTmpDB = FALSE;

            CloseDB();

            CPLString osNewTmpDBName;
            int rc;

            osNewTmpDBName = CPLGenerateTempFilename("osm_tmp");

            CPLDebug("OSM", "%s too big for RAM. Transfering it onto disk in %s",
                     osTmpDBName.c_str(), osNewTmpDBName.c_str());

            if( CPLCopyFile( osNewTmpDBName, osTmpDBName ) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot copy %s to %s",
                         osTmpDBName.c_str(), osNewTmpDBName.c_str() );
                VSIUnlink(osNewTmpDBName);
                bStopParsing = TRUE;
                return FALSE;
            }

            VSIUnlink(osTmpDBName);

            osTmpDBName = osNewTmpDBName;

#ifdef HAVE_SQLITE_VFS
            rc = sqlite3_open_v2( osTmpDBName.c_str(), &hDB,
                                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                                  NULL );
#else
            rc = sqlite3_open( osTmpDBName.c_str(), &hDB );
#endif
            if( rc != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                        "sqlite3_open(%s) failed: %s",
                        osTmpDBName.c_str(), sqlite3_errmsg( hDB ) );
                bStopParsing = TRUE;
                CloseDB();
                return FALSE;
            }

            /* On Unix filesystems, you can remove a file even if it */
            /* opened */
            const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if( EQUAL(pszVal, "YES") )
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                bMustUnlink = VSIUnlink( osTmpDBName ) != 0;
                CPLPopErrorHandler();
            }

            if( !SetDBOptions() || !CreatePreparedStatements() )
            {
                bStopParsing = TRUE;
                CloseDB();
                return FALSE;
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROSMDataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGROSMDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGROSMDataSource::GetExtent( OGREnvelope *psExtent )
{
    if (!bHasParsedFirstChunk)
    {
        bHasParsedFirstChunk = TRUE;
        OSM_ProcessBlock(psParser);
    }

    if (bExtentValid)
    {
        memcpy(psExtent, &sExtent, sizeof(sExtent));
        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}


/************************************************************************/
/*                   OGROSMSingleFeatureLayer                           */
/************************************************************************/

class OGROSMSingleFeatureLayer : public OGRLayer
{
  private:
    int                 nVal;
    char               *pszVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGROSMSingleFeatureLayer( const char* pszLayerName,
                                                     int nVal );
                        OGROSMSingleFeatureLayer( const char* pszLayerName,
                                                     const char *pszVal );
                        ~OGROSMSingleFeatureLayer();

    virtual void        ResetReading() { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature();
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) { return FALSE; }
};


/************************************************************************/
/*                    OGROSMSingleFeatureLayer()                        */
/************************************************************************/

OGROSMSingleFeatureLayer::OGROSMSingleFeatureLayer(  const char* pszLayerName,
                                                     int nVal )
{
    poFeatureDefn = new OGRFeatureDefn( "SELECT" );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( pszLayerName, OFTInteger );
    poFeatureDefn->AddFieldDefn( &oField );

    iNextShapeId = 0;
    this->nVal = nVal;
    pszVal = NULL;
}

/************************************************************************/
/*                    OGROSMSingleFeatureLayer()                        */
/************************************************************************/

OGROSMSingleFeatureLayer::OGROSMSingleFeatureLayer(  const char* pszLayerName,
                                                     const char *pszVal )
{
    poFeatureDefn = new OGRFeatureDefn( "SELECT" );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( pszLayerName, OFTString );
    poFeatureDefn->AddFieldDefn( &oField );

    iNextShapeId = 0;
    nVal = 0;
    this->pszVal = CPLStrdup(pszVal);
}

/************************************************************************/
/*                    ~OGROSMSingleFeatureLayer()                       */
/************************************************************************/

OGROSMSingleFeatureLayer::~OGROSMSingleFeatureLayer()
{
    poFeatureDefn->Release();
    CPLFree(pszVal);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGROSMSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return NULL;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    else
        poFeature->SetField(0, nVal);
    poFeature->SetFID(iNextShapeId ++);
    return poFeature;
}

/************************************************************************/
/*                      OGROSMResultLayerDecorator                      */
/************************************************************************/

class OGROSMResultLayerDecorator : public OGRLayerDecorator
{
        CPLString               osDSName;
        CPLString               osInterestLayers;

    public:
        OGROSMResultLayerDecorator(OGRLayer* poLayer,
                                   CPLString osDSName,
                                   CPLString osInterestLayers) :
                                        OGRLayerDecorator(poLayer, TRUE),
                                        osDSName(osDSName),
                                        osInterestLayers(osInterestLayers) {}

        virtual int         GetFeatureCount( int bForce = TRUE )
        {
            /* When we run GetFeatureCount() with SQLite SQL dialect, */
            /* the OSM dataset will be re-opened. Make sure that it is */
            /* re-opened with the same interest layers */
            AddInterestLayersForDSName(osDSName, osInterestLayers);
            return OGRLayerDecorator::GetFeatureCount(bForce);
        }
};

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGROSMDataSource::ExecuteSQL( const char *pszSQLCommand,
                                         OGRGeometry *poSpatialFilter,
                                         const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Special GetBytesRead() command                                  */
/* -------------------------------------------------------------------- */
    if (strcmp(pszSQLCommand, "GetBytesRead()") == 0)
    {
        char szVal[64];
        sprintf(szVal, CPL_FRMT_GUIB, OSM_GetBytesRead(psParser));
        return new OGROSMSingleFeatureLayer( "GetBytesRead", szVal );
    }

    if( poResultSetLayer != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "A SQL result layer is still in use. Please delete it first");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special SET interest_layers = command                           */
/* -------------------------------------------------------------------- */
    if (strncmp(pszSQLCommand, "SET interest_layers =", 21) == 0)
    {
        char** papszTokens = CSLTokenizeString2(pszSQLCommand + 21, ",", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
        int i;

        for(i=0; i < nLayers; i++)
        {
            papoLayers[i]->SetDeclareInterest(FALSE);
        }

        for(i=0; papszTokens[i] != NULL; i++)
        {
            OGROSMLayer* poLayer = (OGROSMLayer*) GetLayerByName(papszTokens[i]);
            if( poLayer != NULL )
            {
                poLayer->SetDeclareInterest(TRUE);
            }
        }

        if( papoLayers[IDX_LYR_POINTS]->IsUserInterested() &&
            !papoLayers[IDX_LYR_LINES]->IsUserInterested() &&
            !papoLayers[IDX_LYR_MULTILINESTRINGS]->IsUserInterested() &&
            !papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() &&
            !papoLayers[IDX_LYR_OTHER_RELATIONS]->IsUserInterested())
        {
            if( CPLGetConfigOption("OSM_INDEX_POINTS", NULL) == NULL )
            {
                CPLDebug("OSM", "Disabling indexing of nodes");
                bIndexPoints = FALSE;
            }
            if( CPLGetConfigOption("OSM_USE_POINTS_INDEX", NULL) == NULL )
            {
                bUsePointsIndex = FALSE;
            }
            if( CPLGetConfigOption("OSM_INDEX_WAYS", NULL) == NULL )
            {
                CPLDebug("OSM", "Disabling indexing of ways");
                bIndexWays = FALSE;
            }
            if( CPLGetConfigOption("OSM_USE_WAYS_INDEX", NULL) == NULL )
            {
                bUseWaysIndex = FALSE;
            }
        }
        else if( papoLayers[IDX_LYR_LINES]->IsUserInterested() &&
                 !papoLayers[IDX_LYR_MULTILINESTRINGS]->IsUserInterested() &&
                 !papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() &&
                 !papoLayers[IDX_LYR_OTHER_RELATIONS]->IsUserInterested() )
        {
            if( CPLGetConfigOption("OSM_INDEX_WAYS", NULL) == NULL )
            {
                CPLDebug("OSM", "Disabling indexing of ways");
                bIndexWays = FALSE;
            }
            if( CPLGetConfigOption("OSM_USE_WAYS_INDEX", NULL) == NULL )
            {
                bUseWaysIndex = FALSE;
            }
        }

        CSLDestroy(papszTokens);

        return NULL;
    }

    while(*pszSQLCommand == ' ')
        pszSQLCommand ++;

    /* Try to analyse the SQL command to get the interest table */
    if( EQUALN(pszSQLCommand, "SELECT", 5) )
    {
        int bLayerAlreadyAdded = FALSE;
        CPLString osInterestLayers = "SET interest_layers =";

        if( pszDialect != NULL && EQUAL(pszDialect, "SQLITE") )
        {
            std::set<LayerDesc> oSetLayers = OGRSQLiteGetReferencedLayers(pszSQLCommand);
            std::set<LayerDesc>::iterator oIter = oSetLayers.begin();
            for(; oIter != oSetLayers.end(); ++oIter)
            {
                const LayerDesc& oLayerDesc = *oIter;
                if( oLayerDesc.osDSName.size() == 0 )
                {
                    if( bLayerAlreadyAdded ) osInterestLayers += ",";
                    bLayerAlreadyAdded = TRUE;
                    osInterestLayers += oLayerDesc.osLayerName;
                }
            }
        }
        else
        {
            swq_select sSelectInfo;

            CPLPushErrorHandler(CPLQuietErrorHandler);
            CPLErr eErr = sSelectInfo.preparse( pszSQLCommand );
            CPLPopErrorHandler();

            if( eErr == CPLE_None )
            {
                swq_select* pCurSelect = &sSelectInfo;
                while(pCurSelect != NULL)
                {
                    for( int iTable = 0; iTable < pCurSelect->table_count; iTable++ )
                    {
                        swq_table_def *psTableDef = pCurSelect->table_defs + iTable;
                        if( psTableDef->data_source == NULL )
                        {
                            if( bLayerAlreadyAdded ) osInterestLayers += ",";
                            bLayerAlreadyAdded = TRUE;
                            osInterestLayers += psTableDef->table_name;
                        }
                    }
                    pCurSelect = pCurSelect->poOtherSelect;
                }
            }
        }

        if( bLayerAlreadyAdded )
        {
            /* Backup current optimization parameters */
            abSavedDeclaredInterest.resize(0);
            for(int i=0; i < nLayers; i++)
            {
                abSavedDeclaredInterest.push_back(papoLayers[i]->IsUserInterested());
            }
            bIndexPointsBackup = bIndexPoints;
            bUsePointsIndexBackup = bUsePointsIndex;
            bIndexWaysBackup = bIndexWays;
            bUseWaysIndexBackup = bUseWaysIndex;

            /* Update optimization parameters */
            ExecuteSQL(osInterestLayers, NULL, NULL);

            ResetReading();

            /* Run the request */
            poResultSetLayer = OGRDataSource::ExecuteSQL( pszSQLCommand,
                                                            poSpatialFilter,
                                                            pszDialect );

            /* If the user explicitely run a COUNT() request, then do it ! */
            if( poResultSetLayer )
            {
                if( pszDialect != NULL && EQUAL(pszDialect, "SQLITE") )
                {
                    poResultSetLayer = new OGROSMResultLayerDecorator(
                                poResultSetLayer, GetName(), osInterestLayers);
                }
                bIsFeatureCountEnabled = TRUE;
            }

            return poResultSetLayer;
        }
    }

    return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                      poSpatialFilter,
                                      pszDialect );
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGROSMDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    if( poLayer != NULL && poLayer == poResultSetLayer )
    {
        poResultSetLayer = NULL;

        bIsFeatureCountEnabled = FALSE;

        /* Restore backup'ed optimization parameters */
        for(int i=0; i < nLayers; i++)
        {
            papoLayers[i]->SetDeclareInterest(abSavedDeclaredInterest[i]);
        }
        if( bIndexPointsBackup && !bIndexPoints )
            CPLDebug("OSM", "Re-enabling indexing of nodes");
        bIndexPoints = bIndexPointsBackup;
        bUsePointsIndex = bUsePointsIndexBackup;
        if( bIndexWaysBackup && !bIndexWays )
            CPLDebug("OSM", "Re-enabling indexing of ways");
        bIndexWays = bIndexWaysBackup;
        bUseWaysIndex = bUseWaysIndexBackup;
        abSavedDeclaredInterest.resize(0);
    }

    delete poLayer;
}

/************************************************************************/
/*                         IsInterleavedReading()                       */
/************************************************************************/

int OGROSMDataSource::IsInterleavedReading()
{
    if( bInterleavedReading < 0 )
    {
        bInterleavedReading = CSLTestBoolean(
                        CPLGetConfigOption("OGR_INTERLEAVED_READING", "NO"));
        CPLDebug("OSM", "OGR_INTERLEAVED_READING = %d", bInterleavedReading);
    }
    return bInterleavedReading;
}
