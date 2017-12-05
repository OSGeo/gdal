/******************************************************************************
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

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_time.h"
#include "ogr_api.h"
#include "ogr_osm.h"
#include "ogr_p.h"
#include "swq.h"

#define DO_NOT_DEFINE_SKIP_UNKNOWN_FIELD
#define DO_NOT_DEFINE_READ_VARUINT32
#define DO_NOT_DEFINE_SKIP_VARINT
#include "gpb.h"

#include "ogrlayerdecorator.h"
#include "ogrsqliteexecutesql.h"

#include <algorithm>

static const int LIMIT_IDS_PER_REQUEST = 200;

static const int MAX_NODES_PER_WAY = 2000;

static const int IDX_LYR_POINTS = 0;
static const int IDX_LYR_LINES = 1;
static const int IDX_LYR_MULTILINESTRINGS = 2;
static const int IDX_LYR_MULTIPOLYGONS = 3;
static const int IDX_LYR_OTHER_RELATIONS = 4;

static int DBL_TO_INT( double x )
{
  return static_cast<int>(floor(x * 1.0e7 + 0.5));
}
static double INT_TO_DBL( int x ) { return x / 1.0e7; }

static const int MAX_COUNT_FOR_TAGS_IN_WAY = 255;  // Must fit on 1 byte.
static const int MAX_SIZE_FOR_TAGS_IN_WAY = 1024;

// 5 bytes for encoding a int : really the worst case scenario!
static const int WAY_BUFFER_SIZE =
    1 /*is_area*/ + 1 + MAX_NODES_PER_WAY * 2 * 5 + MAX_SIZE_FOR_TAGS_IN_WAY;

static const int NODE_PER_BUCKET = 65536;

static bool VALID_ID_FOR_CUSTOM_INDEXING( GIntBig _id )
{
    return
        _id >= 0 &&
        _id / NODE_PER_BUCKET < INT_MAX;
}

// Minimum size of data written on disk, in *uncompressed* case.
static const int SECTOR_SIZE = 512;
// Which represents, 64 nodes
// static const int NODE_PER_SECTOR = SECTOR_SIZE / (2 * 4);
static const int NODE_PER_SECTOR = 64;
static const int NODE_PER_SECTOR_SHIFT = 6;

// Per bucket, we keep track of the absence/presence of sectors
// only, to reduce memory usage.
// #define BUCKET_BITMAP_SIZE  NODE_PER_BUCKET / (8 * NODE_PER_SECTOR)
static const int BUCKET_BITMAP_SIZE = 128;

// #define BUCKET_SECTOR_SIZE_ARRAY_SIZE  NODE_PER_BUCKET / NODE_PER_SECTOR
// Per bucket, we keep track of the real size of the sector. Each sector
// size is encoded in a single byte, whose value is:
// (sector_size in bytes - 8 ) / 2, minus 8. 252 means uncompressed
static const int BUCKET_SECTOR_SIZE_ARRAY_SIZE = 1024;

// Must be a multiple of both BUCKET_BITMAP_SIZE and
// BUCKET_SECTOR_SIZE_ARRAY_SIZE
static const int knPAGE_SIZE = 4096;

// compressSize should not be greater than 512, so COMPRESS_SIZE_TO_BYTE() fits
// on a byte.
static GByte COMPRESS_SIZE_TO_BYTE( size_t nCompressSize )
{
    return static_cast<GByte>((nCompressSize - 8) / 2);
}

template<typename T> static T ROUND_COMPRESS_SIZE( T nCompressSize )
{
    return ((nCompressSize + 1) / 2) * 2;
}
static int COMPRESS_SIZE_FROM_BYTE( GByte byte_on_size )
{
    return static_cast<int>(byte_on_size) * 2 + 8;
}

// Max number of features that are accumulated in pasWayFeaturePairs.
static const int MAX_DELAYED_FEATURES = 75000;
// Max number of tags that are accumulated in pasAccumulatedTags.
static const int MAX_ACCUMULATED_TAGS  = MAX_DELAYED_FEATURES * 5;
// Max size of the string with tag values that are accumulated in
// pabyNonRedundantValues.
static const int MAX_NON_REDUNDANT_VALUES = MAX_DELAYED_FEATURES * 10;
// Max number of features that are accumulated in panUnsortedReqIds
static const int MAX_ACCUMULATED_NODES = 1000000;

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
// Size of panHashedIndexes array. Must be in the list at
// http://planetmath.org/goodhashtableprimes , and greater than
// MAX_ACCUMULATED_NODES.
static const int HASHED_INDEXES_ARRAY_SIZE = 3145739;
// #define HASHED_INDEXES_ARRAY_SIZE   1572869
static const int COLLISION_BUCKET_ARRAY_SIZE =
    (MAX_ACCUMULATED_NODES / 100) * 40;

// hash function = identity
#define HASH_ID_FUNC(x)             ((GUIntBig)(x))
#endif // ENABLE_NODE_LOOKUP_BY_HASHING

// #define FAKE_LOOKUP_NODES

// #define DEBUG_MEM_USAGE
#ifdef DEBUG_MEM_USAGE
size_t GetMaxTotalAllocs();
#endif

static void WriteVarInt64(GUIntBig nSVal, GByte** ppabyData);
static void WriteVarSInt64(GIntBig nSVal, GByte** ppabyData);

CPL_CVSID("$Id$")

class DSToBeOpened
{
    public:
        GIntBig                 nPID;
        CPLString               osDSName;
        CPLString               osInterestLayers;
};

static CPLMutex                  *hMutex = NULL;
static std::vector<DSToBeOpened>  oListDSToBeOpened;

/************************************************************************/
/*                    AddInterestLayersForDSName()                      */
/************************************************************************/

static void AddInterestLayersForDSName( const CPLString& osDSName,
                                        const CPLString& osInterestLayers )
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

static CPLString GetInterestLayersForDSName( const CPLString& osDSName )
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

OGROSMDataSource::OGROSMDataSource() :
    nLayers(0),
    papoLayers(NULL),
    pszName(NULL),
    bExtentValid(false),
    bInterleavedReading(-1),
    poCurrentLayer(NULL),
    psParser(NULL),
    bHasParsedFirstChunk(false),
    bStopParsing(false),
    pMyVFS(NULL),
    hDB(NULL),
    hInsertNodeStmt(NULL),
    hInsertWayStmt(NULL),
    hSelectNodeBetweenStmt(NULL),
    pahSelectNodeStmt(NULL),
    pahSelectWayStmt(NULL),
    hInsertPolygonsStandaloneStmt(NULL),
    hDeletePolygonsStandaloneStmt(NULL),
    hSelectPolygonsStandaloneStmt(NULL),
    bHasRowInPolygonsStandalone(false),
    hDBForComputedAttributes(NULL),
    nMaxSizeForInMemoryDBInMB(0),
    bInMemoryTmpDB(false),
    bMustUnlink(true),
    nNodesInTransaction(0),
    nMinSizeKeysInSetClosedWaysArePolygons(0),
    nMaxSizeKeysInSetClosedWaysArePolygons(0),
    pasLonLatCache(NULL),
    bReportAllNodes(false),
    bReportAllWays(false),
    bFeatureAdded(false),
    bInTransaction(false),
    bIndexPoints(true),
    bUsePointsIndex(true),
    bIndexWays(true),
    bUseWaysIndex(true),
    poResultSetLayer(NULL),
    bIndexPointsBackup(false),
    bUsePointsIndexBackup(false),
    bIndexWaysBackup(false),
    bUseWaysIndexBackup(false),
    bIsFeatureCountEnabled(false),
    bAttributeNameLaundering(true),
    pabyWayBuffer(NULL),
    nWaysProcessed(0),
    nRelationsProcessed(0),
    bCustomIndexing(true),
    bCompressNodes(false),
    nUnsortedReqIds(0),
    panUnsortedReqIds(NULL),
    nReqIds(0),
    panReqIds(NULL),
#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    bEnableHashedIndex(true),
    panHashedIndexes(NULL),
    psCollisionBuckets(NULL),
    bHashedIndexValid(false),
#endif
    pasLonLatArray(NULL),
    pasAccumulatedTags(NULL),
    nAccumulatedTags(0),
    pabyNonRedundantValues(NULL),
    nNonRedundantValuesLen(0),
    pasWayFeaturePairs(NULL),
    nWayFeaturePairs(0),
    nNextKeyIndex(0),
    bInMemoryNodesFile(false),
    bMustUnlinkNodesFile(true),
    nNodesFileSize(0),
    fpNodes(NULL),
    nPrevNodeId(-INT_MAX),
    nBucketOld(-1),
    nOffInBucketReducedOld(-1),
    pabySector(NULL),
    bNeedsToSaveWayInfo(false),
    m_nFileSize(FILESIZE_NOT_INIT)
{}

/************************************************************************/
/*                          ~OGROSMDataSource()                         */
/************************************************************************/

OGROSMDataSource::~OGROSMDataSource()

{
    for( int i=0; i<nLayers; i++ )
        delete papoLayers[i];
    CPLFree(papoLayers);

    CPLFree(pszName);

    if( psParser != NULL )
        CPLDebug( "OSM",
                  "Number of bytes read in file : " CPL_FRMT_GUIB,
                  OSM_GetBytesRead(psParser) );
    OSM_Close(psParser);

    CPLFree(pasLonLatCache);
    CPLFree(pabyWayBuffer);

    if( hDB != NULL )
        CloseDB();

    if( hDBForComputedAttributes != NULL )
        sqlite3_close(hDBForComputedAttributes);

    if( pMyVFS )
    {
        sqlite3_vfs_unregister(pMyVFS);
        CPLFree(pMyVFS->pAppData);
        CPLFree(pMyVFS);
    }

    if( !osTmpDBName.empty() && bMustUnlink )
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

    for( int i = 0; i < nWayFeaturePairs; i++)
    {
        delete pasWayFeaturePairs[i].poFeature;
    }
    CPLFree(pasWayFeaturePairs);
    CPLFree(pasAccumulatedTags);
    CPLFree(pabyNonRedundantValues);

#ifdef OSM_DEBUG
    FILE* f = fopen("keys.txt", "wt");
    for( int i=0; i<startic_cast<int>(asKeys.size()); i++ )
    {
        KeyDesc* psKD = asKeys[i];
        fprintf(f, "%08d idx=%d %s\n",
                psKD->nOccurrences,
                psKD->nKeyIndex,
                psKD->pszK);
    }
    fclose(f);
#endif

    for( int i=0; i<static_cast<int>(asKeys.size()); i++ )
    {
        KeyDesc* psKD = asKeys[i];
        CPLFree(psKD->pszK);
        for( int j=0; j<static_cast<int>(psKD->asValues.size());j++)
            CPLFree(psKD->asValues[j]);
        delete psKD;
    }

    if( fpNodes )
        VSIFCloseL(fpNodes);
    if( !osNodesFilename.empty() && bMustUnlinkNodesFile )
    {
        const char* pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
        if( !EQUAL(pszVal, "NOT_EVEN_AT_END") )
            VSIUnlink(osNodesFilename);
    }

    CPLFree(pabySector);
    std::map<int, Bucket>::iterator oIter = oMapBuckets.begin();
    for( ; oIter != oMapBuckets.end(); ++oIter )
    {
        if( bCompressNodes )
        {
            int nRem = oIter->first % (knPAGE_SIZE / BUCKET_SECTOR_SIZE_ARRAY_SIZE);
            if( nRem == 0 )
                CPLFree(oIter->second.u.panSectorSize);
        }
        else
        {
            int nRem = oIter->first % (knPAGE_SIZE / BUCKET_BITMAP_SIZE);
            if( nRem == 0 )
                CPLFree(oIter->second.u.pabyBitmap);
        }
    }
}

/************************************************************************/
/*                             CloseDB()                               */
/************************************************************************/

void OGROSMDataSource::CloseDB()
{
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
        for( int i = 0; i < LIMIT_IDS_PER_REQUEST; i++ )
        {
            if( pahSelectNodeStmt[i] != NULL )
                sqlite3_finalize( pahSelectNodeStmt[i] );
        }
        CPLFree(pahSelectNodeStmt);
        pahSelectNodeStmt = NULL;
    }

    if( pahSelectWayStmt != NULL )
    {
        for( int i = 0; i < LIMIT_IDS_PER_REQUEST; i++ )
        {
            if( pahSelectWayStmt[i] != NULL )
                sqlite3_finalize( pahSelectWayStmt[i] );
        }
        CPLFree(pahSelectWayStmt);
        pahSelectWayStmt = NULL;
    }

    if( bInTransaction )
        CommitTransactionCacheDB();

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

bool OGROSMDataSource::IndexPoint( OSMNode* psNode )
{
    if( !bIndexPoints )
        return true;

    if( bCustomIndexing)
        return IndexPointCustom(psNode);

    return IndexPointSQLite(psNode);
}

/************************************************************************/
/*                          IndexPointSQLite()                          */
/************************************************************************/

bool OGROSMDataSource::IndexPointSQLite(OSMNode* psNode)
{
    sqlite3_bind_int64( hInsertNodeStmt, 1, psNode->nID );

    LonLat sLonLat;
    sLonLat.nLon = DBL_TO_INT(psNode->dfLon);
    sLonLat.nLat = DBL_TO_INT(psNode->dfLat);

    sqlite3_bind_blob( hInsertNodeStmt, 2, &sLonLat, sizeof(sLonLat),
                       SQLITE_STATIC );

    const int rc = sqlite3_step( hInsertNodeStmt );
    sqlite3_reset( hInsertNodeStmt );
    if( !(rc == SQLITE_OK || rc == SQLITE_DONE) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed inserting node " CPL_FRMT_GIB ": %s",
            psNode->nID, sqlite3_errmsg(hDB));
    }

    return true;
}

/************************************************************************/
/*                           FlushCurrentSector()                       */
/************************************************************************/

bool OGROSMDataSource::FlushCurrentSector()
{
#ifndef FAKE_LOOKUP_NODES
    if( bCompressNodes )
        return FlushCurrentSectorCompressedCase();

    return FlushCurrentSectorNonCompressedCase();
#else
    return true;
#endif
}

/************************************************************************/
/*                            AllocBucket()                             */
/************************************************************************/

Bucket* OGROSMDataSource::AllocBucket( int iBucket )
{
    if( bCompressNodes )
    {
        const int nRem = iBucket % (knPAGE_SIZE / BUCKET_SECTOR_SIZE_ARRAY_SIZE);
        Bucket* psPrevBucket = GetBucket(iBucket - nRem);
        if( psPrevBucket->u.panSectorSize == NULL )
            psPrevBucket->u.panSectorSize =
                static_cast<GByte*>(VSI_CALLOC_VERBOSE(1, knPAGE_SIZE));
        GByte* panSectorSize = psPrevBucket->u.panSectorSize;
        Bucket* psBucket = GetBucket( iBucket );
        if( panSectorSize != NULL )
        {
            psBucket->u.panSectorSize =
                panSectorSize +
                nRem * BUCKET_SECTOR_SIZE_ARRAY_SIZE;
            return psBucket;
        }
        psBucket->u.panSectorSize = NULL;
    }
    else
    {
        const int nRem = iBucket % (knPAGE_SIZE / BUCKET_BITMAP_SIZE);
        Bucket* psPrevBucket = GetBucket(iBucket - nRem);
        if( psPrevBucket->u.pabyBitmap == NULL )
            psPrevBucket->u.pabyBitmap =
                reinterpret_cast<GByte *>(VSI_CALLOC_VERBOSE(1, knPAGE_SIZE));
        GByte* pabyBitmap = psPrevBucket->u.pabyBitmap; 
        Bucket* psBucket = GetBucket( iBucket );
        if( pabyBitmap != NULL )
        {
            psBucket->u.pabyBitmap =
                pabyBitmap +
                nRem * BUCKET_BITMAP_SIZE;
            return psBucket;
        }
        psBucket->u.pabyBitmap = NULL;
    }

    // Out of memory.
    CPLError( CE_Failure, CPLE_AppDefined,
              "AllocBucket() failed. Use OSM_USE_CUSTOM_INDEXING=NO" );
    bStopParsing = true;
    return NULL;
}

/************************************************************************/
/*                             GetBucket()                              */
/************************************************************************/

Bucket* OGROSMDataSource::GetBucket(int nBucketId)
{
    std::map<int, Bucket>::iterator oIter = oMapBuckets.find(nBucketId);
    if( oIter == oMapBuckets.end() )
    {
        Bucket* psBucket = &oMapBuckets[nBucketId];
        psBucket->nOff = -1;
        if( bCompressNodes )
            psBucket->u.panSectorSize = NULL;
        else
            psBucket->u.pabyBitmap = NULL;
        return psBucket;
    }
    return &(oIter->second);
}

/************************************************************************/
/*                     FlushCurrentSectorCompressedCase()               */
/************************************************************************/

bool OGROSMDataSource::FlushCurrentSectorCompressedCase()
{
    GByte abyOutBuffer[2 * SECTOR_SIZE];
    GByte* pabyOut = abyOutBuffer;
    LonLat* pasLonLatIn = (LonLat*)pabySector;
    int nLastLon = 0;
    int nLastLat = 0;
    bool bLastValid = false;

    CPLAssert((NODE_PER_SECTOR % 8) == 0);
    memset(abyOutBuffer, 0, NODE_PER_SECTOR / 8);
    pabyOut += NODE_PER_SECTOR / 8;
    for( int i = 0; i < NODE_PER_SECTOR; i++)
    {
        if( pasLonLatIn[i].nLon || pasLonLatIn[i].nLat )
        {
            abyOutBuffer[i >> 3] |= (1 << (i % 8));
            if( bLastValid )
            {
                const GIntBig nDiff64Lon =
                  static_cast<GIntBig>(pasLonLatIn[i].nLon) -
                  static_cast<GIntBig>(nLastLon);
                const GIntBig nDiff64Lat = pasLonLatIn[i].nLat - nLastLat;
                WriteVarSInt64(nDiff64Lon, &pabyOut);
                WriteVarSInt64(nDiff64Lat, &pabyOut);
            }
            else
            {
                memcpy(pabyOut, &pasLonLatIn[i], sizeof(LonLat));
                pabyOut += sizeof(LonLat);
            }
            bLastValid = true;

            nLastLon = pasLonLatIn[i].nLon;
            nLastLat = pasLonLatIn[i].nLat;
        }
    }

    size_t nCompressSize = static_cast<size_t>(pabyOut - abyOutBuffer);
    CPLAssert(nCompressSize < sizeof(abyOutBuffer) - 1);
    abyOutBuffer[nCompressSize] = 0;

    nCompressSize = ROUND_COMPRESS_SIZE(nCompressSize);
    GByte* pabyToWrite = NULL;
    if( nCompressSize >= static_cast<size_t>(SECTOR_SIZE) )
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

        Bucket* psBucket = GetBucket(nBucketOld);
        if( psBucket->u.panSectorSize == NULL )
        {
            psBucket = AllocBucket(nBucketOld);
            if( psBucket == NULL )
                return false;
        }
        CPLAssert( psBucket->u.panSectorSize != NULL );
        psBucket->u.panSectorSize[nOffInBucketReducedOld] =
                                    COMPRESS_SIZE_TO_BYTE(nCompressSize);

        return true;
    }

    CPLError( CE_Failure, CPLE_AppDefined,
              "Cannot write in temporary node file %s : %s",
              osNodesFilename.c_str(), VSIStrerror(errno));

    return false;
}

/************************************************************************/
/*                   FlushCurrentSectorNonCompressedCase()              */
/************************************************************************/

bool OGROSMDataSource::FlushCurrentSectorNonCompressedCase()
{
    if( VSIFWriteL(pabySector, 1, static_cast<size_t>(SECTOR_SIZE),
                   fpNodes) == static_cast<size_t>(SECTOR_SIZE) )
    {
        memset(pabySector, 0, SECTOR_SIZE);
        nNodesFileSize += SECTOR_SIZE;
        return true;
    }

    CPLError( CE_Failure, CPLE_AppDefined,
              "Cannot write in temporary node file %s : %s",
              osNodesFilename.c_str(), VSIStrerror(errno));

    return false;
}

/************************************************************************/
/*                          IndexPointCustom()                          */
/************************************************************************/

bool OGROSMDataSource::IndexPointCustom(OSMNode* psNode)
{
    if( psNode->nID <= nPrevNodeId)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Non increasing node id. Use OSM_USE_CUSTOM_INDEXING=NO");
        bStopParsing = true;
        return false;
    }
    if( !VALID_ID_FOR_CUSTOM_INDEXING(psNode->nID) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported node id value (" CPL_FRMT_GIB
                  "). Use OSM_USE_CUSTOM_INDEXING=NO",
                  psNode->nID);
        bStopParsing = true;
        return false;
    }

    const int nBucket = static_cast<int>(psNode->nID / NODE_PER_BUCKET);
    const int nOffInBucket = static_cast<int>(psNode->nID % NODE_PER_BUCKET);
    const int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
    const int nOffInBucketReducedRemainer =
        nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

    Bucket* psBucket = GetBucket(nBucket);

    if( !bCompressNodes )
    {
        const int nBitmapIndex = nOffInBucketReduced / 8;
        const int nBitmapRemainer = nOffInBucketReduced % 8;
        if( psBucket->u.pabyBitmap == NULL )
        {
            psBucket = AllocBucket(nBucket);
            if( psBucket == NULL )
                return false;
        }
        CPLAssert( psBucket->u.pabyBitmap != NULL );
        psBucket->u.pabyBitmap[nBitmapIndex] |= (1 << nBitmapRemainer);
    }

    if( nBucket != nBucketOld )
    {
        CPLAssert(nBucket > nBucketOld);
        if( nBucketOld >= 0 )
        {
            if( !FlushCurrentSector() )
            {
                bStopParsing = true;
                return false;
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
            bStopParsing = true;
            return false;
        }
        nOffInBucketReducedOld = nOffInBucketReduced;
    }

    LonLat* psLonLat = reinterpret_cast<LonLat*>(
        pabySector + sizeof(LonLat) * nOffInBucketReducedRemainer);
    psLonLat->nLon = DBL_TO_INT(psNode->dfLon);
    psLonLat->nLat = DBL_TO_INT(psNode->dfLat);

    nPrevNodeId = psNode->nID;

    return true;
}

/************************************************************************/
/*                             NotifyNodes()                            */
/************************************************************************/

void OGROSMDataSource::NotifyNodes( unsigned int nNodes, OSMNode* pasNodes )
{
    const OGREnvelope* psEnvelope =
        papoLayers[IDX_LYR_POINTS]->GetSpatialFilterEnvelope();

    for( unsigned int i = 0; i < nNodes; i++ )
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

        bool bInterestingTag = bReportAllNodes;
        OSMTag* pasTags = pasNodes[i].pasTags;

        if( !bReportAllNodes )
        {
            for( unsigned int j = 0; j < pasNodes[i].nTags; j++)
            {
                const char* pszK = pasTags[j].pszK;
                if( papoLayers[IDX_LYR_POINTS]->IsSignificantKey(pszK) )
                {
                    bInterestingTag = true;
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
                poFeature, pasNodes[i].nID, false, pasNodes[i].nTags,
                pasTags, &pasNodes[i].sInfo );

            int bFilteredOut = FALSE;
            if( !papoLayers[IDX_LYR_POINTS]->AddFeature(poFeature, FALSE,
                                                        &bFilteredOut,
                                                        !bFeatureAdded) )
            {
                bStopParsing = true;
                break;
            }
            else if( !bFilteredOut )
                bFeatureAdded = true;
        }
    }
}

static void OGROSMNotifyNodes ( unsigned int nNodes,
                                OSMNode *pasNodes,
                                OSMContext * /* psOSMContext */,
                                void *user_data)
{
    static_cast<OGROSMDataSource *>(user_data)->NotifyNodes(nNodes, pasNodes);
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
        bHashedIndexValid = true;
#ifdef DEBUG_COLLISIONS
        int nCollisions = 0;
#endif
        int iNextFreeBucket = 0;
        for(unsigned int i = 0; i < nReqIds; i++)
        {
            int nIndInHashArray = static_cast<int>(HASH_ID_FUNC(panReqIds[i]) % HASHED_INDEXES_ARRAY_SIZE);
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
                int iBucket = 0;
                if( nIdx >= 0 )
                {
                    if(iNextFreeBucket == COLLISION_BUCKET_ARRAY_SIZE)
                    {
                        CPLDebug("OSM", "Too many collisions. Disabling hashed indexing");
                        bHashedIndexValid = false;
                        bEnableHashedIndex = false;
                        break;
                    }
                    iBucket = iNextFreeBucket;
                    psCollisionBuckets[iNextFreeBucket].nInd = nIdx;
                    psCollisionBuckets[iNextFreeBucket].nNext = -1;
                    panHashedIndexes[nIndInHashArray] = -iNextFreeBucket - 2;
                    iNextFreeBucket ++;
                }
                else
                {
                    iBucket = -nIdx - 2;
                }
                if(iNextFreeBucket == COLLISION_BUCKET_ARRAY_SIZE)
                {
                    CPLDebug("OSM", "Too many collisions. Disabling hashed indexing");
                    bHashedIndexValid = false;
                    bEnableHashedIndex = false;
                    break;
                }
                while( true )
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
        bHashedIndexValid = false;
#endif // ENABLE_NODE_LOOKUP_BY_HASHING
}

/************************************************************************/
/*                           LookupNodesSQLite()                        */
/************************************************************************/

void OGROSMDataSource::LookupNodesSQLite( )
{
    CPLAssert(
        nUnsortedReqIds <= static_cast<unsigned int>(MAX_ACCUMULATED_NODES));

    nReqIds = 0;
    for( unsigned int i = 0; i < nUnsortedReqIds; i++)
    {
        GIntBig id = panUnsortedReqIds[i];
        panReqIds[nReqIds++] = id;
    }

    std::sort(panReqIds, panReqIds + nReqIds);

    /* Remove duplicates */
    unsigned int j = 0;
    for( unsigned int i = 0; i < nReqIds; i++)
    {
        if( !(i > 0 && panReqIds[i] == panReqIds[i-1]) )
            panReqIds[j++] = panReqIds[i];
    }
    nReqIds = j;

    unsigned int iCur = 0;
    j = 0;
    while( iCur < nReqIds )
    {
        unsigned int nToQuery = nReqIds - iCur;
        if( nToQuery > static_cast<unsigned int>(LIMIT_IDS_PER_REQUEST) )
            nToQuery = static_cast<unsigned int>(LIMIT_IDS_PER_REQUEST);

        sqlite3_stmt* hStmt = pahSelectNodeStmt[nToQuery-1];
        for( unsigned int i=iCur;i<iCur + nToQuery;i++)
        {
             sqlite3_bind_int64( hStmt, i - iCur +1, panReqIds[i] );
        }
        iCur += nToQuery;

        while( sqlite3_step(hStmt) == SQLITE_ROW )
        {
            const GIntBig id = sqlite3_column_int64(hStmt, 0);
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
    GUIntBig nSVal64 = ReadVarUInt64(ppabyPtr);
    GIntBig nDiff64 = ((nSVal64 & 1) == 0) ?
        (GIntBig)(nSVal64 >> 1) : -(GIntBig)(nSVal64 >> 1)-1;
    return nDiff64;
}

/************************************************************************/
/*                           DecompressSector()                         */
/************************************************************************/

static bool DecompressSector( GByte* pabyIn, int nSectorSize, GByte* pabyOut )
{
    GByte* pabyPtr = pabyIn;
    LonLat* pasLonLatOut = (LonLat*) pabyOut;
    int nLastLon = 0;
    int nLastLat = 0;
    bool bLastValid = false;

    pabyPtr += NODE_PER_SECTOR / 8;
    for( int i = 0; i < NODE_PER_SECTOR; i++)
    {
        if( pabyIn[i >> 3] & (1 << (i % 8)) )
        {
            if( bLastValid )
            {
                pasLonLatOut[i].nLon = (int)(nLastLon + ReadVarSInt64(&pabyPtr));
                pasLonLatOut[i].nLat = (int)(nLastLat + ReadVarSInt64(&pabyPtr));
            }
            else
            {
                bLastValid = true;
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
    return nRead == nSectorSize;
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
            bStopParsing = true;
            return;
        }

        nBucketOld = -1;
    }

    CPLAssert(
        nUnsortedReqIds <= static_cast<unsigned int>(MAX_ACCUMULATED_NODES));

    for( unsigned int i = 0; i < nUnsortedReqIds; i++ )
    {
        GIntBig id = panUnsortedReqIds[i];

        if( !VALID_ID_FOR_CUSTOM_INDEXING(id) )
            continue;

        int nBucket = static_cast<int>(id / NODE_PER_BUCKET);
        int nOffInBucket = static_cast<int>(id % NODE_PER_BUCKET);
        int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;

        std::map<int, Bucket>::const_iterator oIter = oMapBuckets.find(nBucket);
        if( oIter == oMapBuckets.end() )
            continue;
        const Bucket* psBucket = &(oIter->second);

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
    unsigned int j = 0;  // Used after for.
    for( unsigned int i = 0; i < nReqIds; i++)
    {
        if( !(i > 0 && panReqIds[i] == panReqIds[i-1]) )
            panReqIds[j++] = panReqIds[i];
    }
    nReqIds = j;

#ifdef FAKE_LOOKUP_NODES
    for( unsigned int i = 0; i < nReqIds; i++)
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
    static const int SECURITY_MARGIN = 8 + 8 + 2 * NODE_PER_SECTOR;
    GByte abyRawSector[SECTOR_SIZE + SECURITY_MARGIN];
    memset(abyRawSector + SECTOR_SIZE, 0, SECURITY_MARGIN);

    int l_nBucketOld = -1;
    int l_nOffInBucketReducedOld = -1;
    int k = 0;
    int nOffFromBucketStart = 0;

    unsigned int j = 0;  // Used after for.
    for( unsigned int i = 0; i < nReqIds; i++ )
    {
        const GIntBig id = panReqIds[i];
        const int nBucket = static_cast<int>(id / NODE_PER_BUCKET);
        const int nOffInBucket = static_cast<int>(id % NODE_PER_BUCKET);
        const int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
        const int nOffInBucketReducedRemainer =
            nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

        if( nBucket != l_nBucketOld )
        {
            l_nOffInBucketReducedOld = -1;
            k = 0;
            nOffFromBucketStart = 0;
        }

        if( nOffInBucketReduced != l_nOffInBucketReducedOld )
        {
            std::map<int, Bucket>::const_iterator oIter = oMapBuckets.find(nBucket);
            if( oIter == oMapBuckets.end() )
            {
                CPLError(CE_Failure,  CPLE_AppDefined,
                        "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            const Bucket* psBucket = &(oIter->second);
            if( psBucket->u.panSectorSize == NULL )
            {
                CPLError(CE_Failure,  CPLE_AppDefined,
                        "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            const int nSectorSize =
                COMPRESS_SIZE_FROM_BYTE(
                    psBucket->u.panSectorSize[nOffInBucketReduced]);

            /* If we stay in the same bucket, we can reuse the previously */
            /* computed offset, instead of starting from bucket start */
            for( ; k < nOffInBucketReduced; k++ )
            {
                if( psBucket->u.panSectorSize[k] )
                    nOffFromBucketStart +=
                        COMPRESS_SIZE_FROM_BYTE(psBucket->u.panSectorSize[k]);
            }

            VSIFSeekL(fpNodes, psBucket->nOff + nOffFromBucketStart, SEEK_SET);
            if( nSectorSize == SECTOR_SIZE )
            {
                if( VSIFReadL(pabySector, 1,
                              static_cast<size_t>(SECTOR_SIZE),
                              fpNodes) != static_cast<size_t>(SECTOR_SIZE) )
                {
                    CPLError(CE_Failure,  CPLE_AppDefined,
                            "Cannot read node " CPL_FRMT_GIB, id);
                    continue;
                    // FIXME ?
                }
            }
            else
            {
                if( static_cast<int>(VSIFReadL(abyRawSector, 1, nSectorSize,
                                               fpNodes)) != nSectorSize )
                {
                    CPLError(CE_Failure,  CPLE_AppDefined,
                            "Cannot read sector for node " CPL_FRMT_GIB, id);
                    continue;
                    // FIXME ?
                }
                abyRawSector[nSectorSize] = 0;

                if( !DecompressSector(abyRawSector, nSectorSize, pabySector) )
                {
                    CPLError( CE_Failure,  CPLE_AppDefined,
                              "Error while uncompressing sector for node "
                              CPL_FRMT_GIB, id );
                    continue;
                    // FIXME ?
                }
            }

            l_nBucketOld = nBucket;
            l_nOffInBucketReducedOld = nOffInBucketReduced;
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
    unsigned int j = 0;  // Used after for.

    int l_nBucketOld = -1;
    const Bucket* psBucket = NULL;
    // To be glibc friendly, we will do reads aligned on 4096 byte offsets
    const int knDISK_SECTOR_SIZE = 4096;
    CPL_STATIC_ASSERT( (knDISK_SECTOR_SIZE % SECTOR_SIZE) == 0 );
    GByte abyDiskSector[knDISK_SECTOR_SIZE];
    // Offset in the nodes files for which abyDiskSector was read
    GIntBig nOldOffset = -knDISK_SECTOR_SIZE-1;
    // Number of valid bytes in abyDiskSector
    size_t nValidBytes = 0;
    int k = 0;
    int nSectorBase = 0;
    for( unsigned int i = 0; i < nReqIds; i++ )
    {
        const GIntBig id = panReqIds[i];
        const int nBucket = static_cast<int>(id / NODE_PER_BUCKET);
        const int nOffInBucket = static_cast<int>(id % NODE_PER_BUCKET);
        const int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
        const int nOffInBucketReducedRemainer =
            nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

        const int nBitmapIndex = nOffInBucketReduced / 8;
        const int nBitmapRemainer = nOffInBucketReduced % 8;

        if( psBucket == NULL || nBucket != l_nBucketOld )
        {
            std::map<int, Bucket>::const_iterator oIter = oMapBuckets.find(nBucket);
            if( oIter == oMapBuckets.end() )
            {
                CPLError(CE_Failure,  CPLE_AppDefined,
                        "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            psBucket = &(oIter->second);
            if( psBucket->u.pabyBitmap == NULL )
            {
                CPLError(CE_Failure,  CPLE_AppDefined,
                        "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            l_nBucketOld = nBucket;
            nOldOffset = -knDISK_SECTOR_SIZE-1;
            k = 0;
            nSectorBase = 0;
        }

        /* If we stay in the same bucket, we can reuse the previously */
        /* computed offset, instead of starting from bucket start */
        for( ; k < nBitmapIndex; k++ )
            // psBucket->u.pabyBitmap cannot be NULL
            // coverity[var_deref_op]
            nSectorBase += abyBitsCount[psBucket->u.pabyBitmap[k]];
        int nSector = nSectorBase;
        if( nBitmapRemainer )
            nSector +=
                abyBitsCount[psBucket->u.pabyBitmap[nBitmapIndex] &
                             ((1 << nBitmapRemainer) - 1)];

        const GIntBig nNewOffset = psBucket->nOff + nSector * SECTOR_SIZE;
        if( nNewOffset - nOldOffset >= knDISK_SECTOR_SIZE )
        {
            // Align on 4096 boundary to be glibc caching friendly
            const GIntBig nAlignedNewPos = nNewOffset &
                        ~(static_cast<GIntBig>(knDISK_SECTOR_SIZE)-1);
            VSIFSeekL(fpNodes, nAlignedNewPos, SEEK_SET);
            nValidBytes =
                    VSIFReadL(abyDiskSector, 1, knDISK_SECTOR_SIZE, fpNodes);
            nOldOffset = nAlignedNewPos;
        }

        const size_t nOffsetInDiskSector =
            static_cast<size_t>(nNewOffset - nOldOffset) +
            nOffInBucketReducedRemainer * sizeof(LonLat);
        if( nValidBytes < sizeof(LonLat) ||
            nOffsetInDiskSector > nValidBytes - sizeof(LonLat) )
        {
            CPLError(CE_Failure,  CPLE_AppDefined,
                    "Cannot read node " CPL_FRMT_GIB, id);
            continue;
        }
        memcpy( &pasLonLatArray[j],
                abyDiskSector + nOffsetInDiskSector,
                sizeof(LonLat) );

        panReqIds[j] = id;
        if( pasLonLatArray[j].nLon || pasLonLatArray[j].nLat )
            j++;
    }
    nReqIds = j;
}

/************************************************************************/
/*                            WriteVarInt()                             */
/************************************************************************/

static void WriteVarInt( unsigned int nVal, GByte** ppabyData )
{
    GByte* pabyData = *ppabyData;
    while( true )
    {
        if( (nVal & (~0x7fU)) == 0 )
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

static void WriteVarInt64( GUIntBig nVal, GByte** ppabyData )
{
    GByte* pabyData = *ppabyData;
    while( true )
    {
        if( (((GUInt32)nVal) & (~0x7fU)) == 0 )
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

static void WriteVarSInt64( GIntBig nSVal, GByte** ppabyData )
{
    GIntBig nVal = nSVal >= 0
        ? nSVal << 1
        : ((-1-nSVal) << 1) + 1;

    GByte* pabyData = *ppabyData;
    while( true )
    {
        if( (nVal & (~0x7f)) == 0 )
        {
            *pabyData = (GByte)nVal;
            *ppabyData = pabyData + 1;
            return;
        }

        *pabyData = 0x80 | (GByte)(nVal & 0x7f);
        nVal >>= 7;
        pabyData++;
    }
}

/************************************************************************/
/*                             CompressWay()                            */
/************************************************************************/

int OGROSMDataSource::CompressWay ( bool bIsArea, unsigned int nTags,
                                    IndexedKVP* pasTags,
                                    int nPoints, LonLat* pasLonLatPairs,
                                    OSMInfo* psInfo,
                                    GByte* pabyCompressedWay )
{
    GByte* pabyPtr = pabyCompressedWay;
    *pabyPtr = (bIsArea) ? 1 : 0;
    pabyPtr++;
    pabyPtr++; // skip tagCount

    int nTagCount = 0;
    CPLAssert(nTags < static_cast<unsigned int>(MAX_COUNT_FOR_TAGS_IN_WAY));
    for( unsigned int iTag = 0; iTag < nTags; iTag++ )
    {
        if( static_cast<int>(pabyPtr - pabyCompressedWay) + 2 >=
            MAX_SIZE_FOR_TAGS_IN_WAY )
        {
            break;
        }

        WriteVarInt(pasTags[iTag].nKeyIndex, &pabyPtr);

        // To fit in 2 bytes, the theoretical limit would be 127 * 128 + 127.
        if( pasTags[iTag].bVIsIndex )
        {
            if( static_cast<int>(pabyPtr - pabyCompressedWay) + 2 >=
                MAX_SIZE_FOR_TAGS_IN_WAY )
            {
                break;
            }

            WriteVarInt(pasTags[iTag].u.nValueIndex, &pabyPtr);
        }
        else
        {
            const char* pszV = (const char*)pabyNonRedundantValues +
                pasTags[iTag].u.nOffsetInpabyNonRedundantValues;

            int nLenV = static_cast<int>(strlen(pszV)) + 1;
            if( static_cast<int>(pabyPtr - pabyCompressedWay) +
                2 + nLenV >= MAX_SIZE_FOR_TAGS_IN_WAY )
            {
                break;
            }

            WriteVarInt(0, &pabyPtr);

            memcpy(pabyPtr, pszV, nLenV);
            pabyPtr += nLenV;
        }

        nTagCount ++;
    }

    pabyCompressedWay[1] = (GByte) nTagCount;

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
        GIntBig nDiff64 =
            (GIntBig)pasLonLatPairs[i].nLon - (GIntBig)pasLonLatPairs[i-1].nLon;
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
                                     bool* pbIsArea,
                                     LonLat* pasCoords,
                                     unsigned int* pnTags, OSMTag* pasTags,
                                     OSMInfo* psInfo )
{
    GByte* pabyPtr = pabyCompressedWay;
    if( pbIsArea )
        *pbIsArea = (*pabyPtr == 1) ? true : false;
    pabyPtr ++;
    unsigned int nTags = *pabyPtr;
    pabyPtr ++;

    if( pnTags )
        *pnTags = nTags;

    // TODO: Some additional safety checks.
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
            CPLAssert(nV == 0 ||
                      (nV > 0 && nV < (int)asKeys[nK]->asValues.size()));
            pasTags[iTag].pszV =
                nV ? asKeys[nK]->asValues[nV] : (const char*) pszV;
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

            psInfo->bTimeStampIsStr = false;
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
        pasCoords[nPoints].nLon = (int)(pasCoords[nPoints-1].nLon + ReadVarSInt64(&pabyPtr));
        pasCoords[nPoints].nLat = (int)(pasCoords[nPoints-1].nLat + ReadVarSInt64(&pabyPtr));

        nPoints ++;
    } while (pabyPtr < pabyCompressedWay + nBytes);

    return nPoints;
}

/************************************************************************/
/*                              IndexWay()                              */
/************************************************************************/

void OGROSMDataSource::IndexWay(GIntBig nWayID, bool bIsArea,
                                unsigned int nTags, IndexedKVP* pasTags,
                                LonLat* pasLonLatPairs, int nPairs,
                                OSMInfo* psInfo)
{
    if( !bIndexWays )
        return;

    sqlite3_bind_int64( hInsertWayStmt, 1, nWayID );

    int nBufferSize = CompressWay (bIsArea, nTags, pasTags, nPairs, pasLonLatPairs, psInfo, pabyWayBuffer);
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
    if( nReqIds == 0 )
        return -1;
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

    for( int iPair = 0; iPair < nWayFeaturePairs; iPair ++)
    {
        WayFeaturePair* psWayFeaturePairs = &pasWayFeaturePairs[iPair];

        const EMULATED_BOOL bIsArea = psWayFeaturePairs->bIsArea;

        unsigned int nFound = 0;

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
        if( bHashedIndexValid )
        {
            for( unsigned int i=0;i<psWayFeaturePairs->nRefs;i++)
            {
                int nIndInHashArray = static_cast<int>(
                    HASH_ID_FUNC(psWayFeaturePairs->panNodeRefs[i]) %
                        HASHED_INDEXES_ARRAY_SIZE);
                int nIdx = panHashedIndexes[nIndInHashArray];
                if( nIdx < -1 )
                {
                    int iBucket = -nIdx - 2;
                    while( true )
                    {
                        nIdx = psCollisionBuckets[iBucket].nInd;
                        if( panReqIds[nIdx] ==
                            psWayFeaturePairs->panNodeRefs[i] )
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

                if( nIdx >= 0 )
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
            for( unsigned int i=0;i<psWayFeaturePairs->nRefs;i++)
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
                if( nIdx >= 0 )
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
            psWayFeaturePairs->bIsArea = false;
            continue;
        }

        if( bIsArea && papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() )
        {
            IndexWay(psWayFeaturePairs->nWayID,
                     bIsArea != 0,
                     psWayFeaturePairs->nTags,
                     psWayFeaturePairs->pasTags,
                     pasLonLatCache, (int)nFound,
                     &psWayFeaturePairs->sInfo);
        }
        else
            IndexWay(psWayFeaturePairs->nWayID, bIsArea != 0, 0, NULL,
                     pasLonLatCache, (int)nFound, NULL);

        if( psWayFeaturePairs->poFeature == NULL )
        {
            continue;
        }

        OGRLineString* poLS = new OGRLineString();
        OGRGeometry* poGeom = poLS;

        poLS->setNumPoints((int)nFound);
        for( unsigned int i=0;i<nFound;i++)
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
            bStopParsing = true;
        else if( !bFilteredOut )
            bFeatureAdded = true;
    }

    if( papoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() )
    {
        for( int iPair = 0; iPair < nWayFeaturePairs; iPair ++)
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
/*                      IsClosedWayTaggedAsPolygon()                    */
/************************************************************************/

bool OGROSMDataSource::IsClosedWayTaggedAsPolygon( unsigned int nTags, const OSMTag* pasTags )
{
    bool bIsArea = false;
    const int nSizeArea = 4;
    const int nStrnlenK = std::max(nSizeArea,
                                   nMaxSizeKeysInSetClosedWaysArePolygons)+1;
    std::string oTmpStr;
    oTmpStr.reserve(nMaxSizeKeysInSetClosedWaysArePolygons);
    for( unsigned int i=0;i<nTags;i++)
    {
        const char* pszK = pasTags[i].pszK;
        const int nKLen = static_cast<int>(CPLStrnlen(pszK, nStrnlenK));
        if( nKLen > nMaxSizeKeysInSetClosedWaysArePolygons )
            continue;

        if( nKLen == nSizeArea && strcmp(pszK, "area") == 0 )
        {
            const char* pszV = pasTags[i].pszV;
            if( strcmp(pszV, "yes") == 0 )
            {
                bIsArea = true;
                // final true. We can't have several area tags...
                break;
            }
            else if( strcmp(pszV, "no") == 0 )
            {
                bIsArea = false;
                break;
            }
        }
        if( bIsArea )
            continue;

        if( nKLen >= nMinSizeKeysInSetClosedWaysArePolygons )
        {
            oTmpStr.assign(pszK, nKLen);
            if(  aoSetClosedWaysArePolygons.find(oTmpStr) !=
                    aoSetClosedWaysArePolygons.end() )
            {
                bIsArea = true;
                continue;
            }
        }

        const char* pszV = pasTags[i].pszV;
        const int nVLen = static_cast<int>(CPLStrnlen(pszV, nStrnlenK));
        if( nKLen + 1 + nVLen >= nMinSizeKeysInSetClosedWaysArePolygons &&
            nKLen + 1 + nVLen <= nMaxSizeKeysInSetClosedWaysArePolygons )
        {
            oTmpStr.assign(pszK, nKLen);
            oTmpStr.append(1, '=');
            oTmpStr.append(pszV, nVLen);
            if( aoSetClosedWaysArePolygons.find(oTmpStr) !=
                  aoSetClosedWaysArePolygons.end() )
            {
                bIsArea = true;
                continue;
            }
        }
    }
    return bIsArea;
}

/************************************************************************/
/*                              NotifyWay()                             */
/************************************************************************/

void OGROSMDataSource::NotifyWay( OSMWay* psWay )
{
    nWaysProcessed++;
    if( nWaysProcessed % 10000 == 0 )
    {
        CPLDebug("OSM", "Ways processed : %d", nWaysProcessed);
#ifdef DEBUG_MEM_USAGE
        CPLDebug("OSM", "GetMaxTotalAllocs() = " CPL_FRMT_GUIB,
                 static_cast<GUIntBig>(GetMaxTotalAllocs()));
#endif
    }

    if( !bUsePointsIndex )
        return;

    //printf("way %d : %d nodes\n", (int)psWay->nID, (int)psWay->nRefs);
    if( psWay->nRefs > static_cast<unsigned int>(MAX_NODES_PER_WAY) )
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
    bool bIsArea = false;
    if( psWay->panNodeRefs[0] == psWay->panNodeRefs[psWay->nRefs - 1] )
    {
        bIsArea = IsClosedWayTaggedAsPolygon(psWay->nTags, psWay->pasTags);
    }

    bool bInterestingTag = bReportAllWays;
    if( !bIsArea && !bReportAllWays )
    {
        for( unsigned int i=0;i<psWay->nTags;i++)
        {
            const char* pszK = psWay->pasTags[i].pszK;
            if( papoLayers[IDX_LYR_LINES]->IsSignificantKey(pszK) )
            {
                bInterestingTag = true;
                break;
            }
        }
    }

    OGRFeature* poFeature = NULL;
    bool bAttrFilterAlreadyEvaluated = false;
    if( !bIsArea && papoLayers[IDX_LYR_LINES]->IsUserInterested() &&
        bInterestingTag )
    {
        poFeature = new OGRFeature(papoLayers[IDX_LYR_LINES]->GetLayerDefn());

        papoLayers[IDX_LYR_LINES]->SetFieldsFromTags(
            poFeature, psWay->nID, false, psWay->nTags, psWay->pasTags,
            &psWay->sInfo );

        // Optimization: if we have an attribute filter, that does not require
        // geometry, and if we don't need to index ways, then we can just
        // evaluate the attribute filter without the geometry.
        if( papoLayers[IDX_LYR_LINES]->HasAttributeFilter() &&
            !papoLayers[IDX_LYR_LINES]->
                AttributeFilterEvaluationNeedsGeometry() &&
            !bIndexWays )
        {
            if( !papoLayers[IDX_LYR_LINES]->EvaluateAttributeFilter(poFeature) )
            {
                delete poFeature;
                return;
            }
            bAttrFilterAlreadyEvaluated = true;
        }
    }
    else if( !bIndexWays )
    {
        return;
    }

    if( nUnsortedReqIds + psWay->nRefs >
        static_cast<unsigned int>(MAX_ACCUMULATED_NODES) ||
        nWayFeaturePairs == MAX_DELAYED_FEATURES ||
        nAccumulatedTags + psWay->nTags >
        static_cast<unsigned int>(MAX_ACCUMULATED_TAGS) ||
        nNonRedundantValuesLen + 1024 > MAX_NON_REDUNDANT_VALUES )
    {
        ProcessWaysBatch();
    }

    WayFeaturePair* psWayFeaturePairs = &pasWayFeaturePairs[nWayFeaturePairs];

    psWayFeaturePairs->nWayID = psWay->nID;
    psWayFeaturePairs->nRefs = psWay->nRefs - (bIsArea ? 1 : 0);
    psWayFeaturePairs->panNodeRefs = panUnsortedReqIds + nUnsortedReqIds;
    psWayFeaturePairs->poFeature = poFeature;
    psWayFeaturePairs->bIsArea = bIsArea;
    psWayFeaturePairs->bAttrFilterAlreadyEvaluated =
        bAttrFilterAlreadyEvaluated;

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
                OGRField sField;
                if( OGRParseXMLDateTime(psWay->sInfo.ts.pszTimeStamp, &sField) )
                {
                    struct tm brokendown;
                    brokendown.tm_year = sField.Date.Year - 1900;
                    brokendown.tm_mon = sField.Date.Month - 1;
                    brokendown.tm_mday = sField.Date.Day;
                    brokendown.tm_hour = sField.Date.Hour;
                    brokendown.tm_min = sField.Date.Minute;
                    brokendown.tm_sec = (int)(sField.Date.Second + .5);
                    psWayFeaturePairs->sInfo.ts.nTimeStamp =
                        CPLYMDHMSToUnixTime(&brokendown);
                }
                else
                    psWayFeaturePairs->sInfo.ts.nTimeStamp = 0;
            }
            psWayFeaturePairs->sInfo.nChangeset = psWay->sInfo.nChangeset;
            psWayFeaturePairs->sInfo.nVersion = psWay->sInfo.nVersion;
            psWayFeaturePairs->sInfo.nUID = psWay->sInfo.nUID;
            psWayFeaturePairs->sInfo.bTimeStampIsStr = false;
            psWayFeaturePairs->sInfo.pszUserSID = ""; // FIXME
        }
        else
        {
            psWayFeaturePairs->sInfo.ts.nTimeStamp = 0;
            psWayFeaturePairs->sInfo.nChangeset = 0;
            psWayFeaturePairs->sInfo.nVersion = 0;
            psWayFeaturePairs->sInfo.nUID = 0;
            psWayFeaturePairs->sInfo.bTimeStampIsStr = false;
            psWayFeaturePairs->sInfo.pszUserSID = "";
        }

        psWayFeaturePairs->pasTags = pasAccumulatedTags + nAccumulatedTags;

        for(unsigned int iTag = 0; iTag < psWay->nTags; iTag++)
        {
            const char* pszK = psWay->pasTags[iTag].pszK;
            const char* pszV = psWay->pasTags[iTag].pszV;

            if( strcmp(pszK, "area") == 0 )
                continue;
            if( strcmp(pszK, "created_by") == 0 )
                continue;
            if( strcmp(pszK, "converted_by") == 0 )
                continue;
            if( strcmp(pszK, "note") == 0 )
                continue;
            if( strcmp(pszK, "todo") == 0 )
                continue;
            if( strcmp(pszK, "fixme") == 0 )
                continue;
            if( strcmp(pszK, "FIXME") == 0 )
                continue;

            std::map<const char*, KeyDesc*, ConstCharComp>::iterator oIterK =
                aoMapIndexedKeys.find(pszK);
            KeyDesc* psKD = NULL;
            if( oIterK == aoMapIndexedKeys.end() )
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
                psKD->nOccurrences = 0;
                psKD->asValues.push_back(CPLStrdup(""));
                aoMapIndexedKeys[psKD->pszK] = psKD;
                asKeys.push_back(psKD);
            }
            else
                psKD = oIterK->second;
            psKD->nOccurrences ++;

            pasAccumulatedTags[nAccumulatedTags].nKeyIndex = (short)psKD->nKeyIndex;

            /* to fit in 2 bytes, the theoretical limit would be 127 * 128 + 127 */
            if( psKD->asValues.size() < 1024 )
            {
                std::map<const char*, int, ConstCharComp>::iterator oIterV;
                oIterV = psKD->anMapV.find(pszV);
                int nValueIndex = 0;
                if( oIterV == psKD->anMapV.end() )
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
                const int nLenV = static_cast<int>(strlen(pszV)) + 1;

                if( psKD->asValues.size() == 1024 )
                {
                    CPLDebug( "OSM", "More than %d different values for tag %s",
                              1024, pszK);
                    // To avoid next warnings.
                    psKD->asValues.push_back(CPLStrdup(""));
                }

                CPLAssert( nNonRedundantValuesLen + nLenV <=
                           MAX_NON_REDUNDANT_VALUES );
                memcpy( pabyNonRedundantValues + nNonRedundantValuesLen, pszV,
                        nLenV);
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
        psWayFeaturePairs->sInfo.bTimeStampIsStr = false;
        psWayFeaturePairs->sInfo.pszUserSID = "";

        psWayFeaturePairs->nTags = 0;
        psWayFeaturePairs->pasTags = NULL;
    }

    nWayFeaturePairs++;

    memcpy( panUnsortedReqIds + nUnsortedReqIds,
            psWay->panNodeRefs, sizeof(GIntBig) * (psWay->nRefs - (bIsArea ? 1 : 0)));
    nUnsortedReqIds += (psWay->nRefs - (bIsArea ? 1 : 0));
}

static void OGROSMNotifyWay ( OSMWay *psWay,
                              OSMContext * /* psOSMContext */,
                              void *user_data)
{
    static_cast<OGROSMDataSource *>(user_data)->NotifyWay(psWay);
}

/************************************************************************/
/*                            LookupWays()                              */
/************************************************************************/

unsigned int OGROSMDataSource::LookupWays( std::map< GIntBig,
                                           std::pair<int,void*> >& aoMapWays,
                                           OSMRelation* psRelation )
{
    unsigned int nFound = 0;
    unsigned int iCur = 0;

    while( iCur < psRelation->nMembers )
    {
        unsigned int nToQuery = 0;
        unsigned int i = iCur;  // Used after for.
        for( ; i < psRelation->nMembers; i++ )
        {
            if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
                strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0 )
            {
                nToQuery ++;
                if( nToQuery ==
                    static_cast<unsigned int>(LIMIT_IDS_PER_REQUEST) )
                {
                    break;
                }
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

    bool bMissing = false;

    for( unsigned int i = 0; i < psRelation->nMembers; i++ )
    {
        if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
            strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0 )
        {
            if( aoMapWays.find( psRelation->pasMembers[i].nID ) == aoMapWays.end() )
            {
                CPLDebug("OSM", "Relation " CPL_FRMT_GIB " has missing ways. Ignoring it",
                        psRelation->nID);
                bMissing = true;
                break;
            }
        }
    }

    if( bMissing )
    {
        std::map< GIntBig, std::pair<int,void*> >::iterator oIter;
        for( oIter = aoMapWays.begin(); oIter != aoMapWays.end(); ++oIter )
            CPLFree(oIter->second.second);

        return NULL;
    }

    OGRMultiLineString* poMLS = new OGRMultiLineString();
    OGRGeometry** papoPolygons = static_cast<OGRGeometry**>( CPLMalloc(
        sizeof(OGRGeometry*) * psRelation->nMembers) );
    int nPolys = 0;

    if( pnTags != NULL )
        *pnTags = 0;

    for( unsigned int i = 0; i < psRelation->nMembers; i++ )
    {
        if( psRelation->pasMembers[i].eType == MEMBER_WAY &&
            strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0  )
        {
            const std::pair<int, void*>& oGeom = aoMapWays[ psRelation->pasMembers[i].nID ];

            LonLat* pasCoords = (LonLat*) pasLonLatCache;
            int nPoints = 0;

            if( pnTags != NULL && *pnTags == 0 &&
                strcmp(psRelation->pasMembers[i].pszRole, "outer") == 0 )
            {
                int nCompressedWaySize = oGeom.first;
                GByte* pabyCompressedWay = (GByte*) oGeom.second;

                memcpy(pabyWayBuffer, pabyCompressedWay, nCompressedWaySize);

                nPoints = UncompressWay (nCompressedWaySize, pabyWayBuffer,
                                         NULL, pasCoords,
                                         pnTags, pasTags, NULL );
            }
            else
            {
                nPoints = UncompressWay (oGeom.first, (GByte*) oGeom.second, NULL, pasCoords,
                                         NULL, NULL, NULL);
            }

            OGRLineString* poLS = NULL;

            if( pasCoords[0].nLon == pasCoords[nPoints - 1].nLon &&
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
                    CPL_IGNORE_RET_VAL(sqlite3_step( hDeletePolygonsStandaloneStmt ));
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
            for( unsigned int i = 0;
                 i < 1 + (unsigned int)poSuperPoly->getNumInteriorRings();
                 i++ )
            {
                OGRLinearRing* poRing =  (i == 0) ? poSuperPoly->getExteriorRing() :
                                                    poSuperPoly->getInteriorRing(i - 1);
                if( poRing != NULL && poRing->getNumPoints() >= 4 &&
                    poRing->getX(0) == poRing->getX(poRing->getNumPoints() -1) &&
                    poRing->getY(0) == poRing->getY(poRing->getNumPoints() -1) )
                {
                    OGRPolygon* poPoly = new OGRPolygon();
                    poPoly->addRing( poRing );
                    papoPolygons[nPolys ++] = poPoly;
                }
            }
        }

        OGR_G_DestroyGeometry(hPoly);
    }
    delete poMLS;

    OGRGeometry* poRet = NULL;

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
            CPLDebug( "OSM",
                      "Relation " CPL_FRMT_GIB
                      ": Geometry has incompatible type : %s",
                      psRelation->nID,
                      poGeom != NULL ?
                      OGR_G_GetGeometryName(
                          reinterpret_cast<OGRGeometryH>(poGeom)) : "null" );
            delete poGeom;
        }
    }

    CPLFree(papoPolygons);

    std::map< GIntBig, std::pair<int,void*> >::iterator oIter;
    for( oIter = aoMapWays.begin(); oIter != aoMapWays.end(); ++oIter )
        CPLFree(oIter->second.second);

    return poRet;
}

/************************************************************************/
/*                          BuildGeometryCollection()                   */
/************************************************************************/

OGRGeometry* OGROSMDataSource::BuildGeometryCollection(OSMRelation* psRelation,
                                                       int bMultiLineString)
{
    std::map< GIntBig, std::pair<int,void*> > aoMapWays;
    LookupWays( aoMapWays, psRelation );

    OGRGeometryCollection* poColl = ( bMultiLineString ) ?
        new OGRMultiLineString() : new OGRGeometryCollection();

    for( unsigned int i = 0; i < psRelation->nMembers; i ++ )
    {
        if( psRelation->pasMembers[i].eType ==
            MEMBER_NODE && !bMultiLineString )
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

            LonLat* pasCoords = reinterpret_cast<LonLat *>(pasLonLatCache);
            bool bIsArea = false;
            const int nPoints = UncompressWay(
                oGeom.first,
                reinterpret_cast<GByte *>(oGeom.second),
                &bIsArea, pasCoords, NULL, NULL, NULL );
            OGRLineString* poLS = NULL;
            if( bIsArea && !bMultiLineString )
            {
                OGRLinearRing* poLR = new OGRLinearRing();
                OGRPolygon* poPoly = new OGRPolygon();
                poPoly->addRingDirectly(poLR);
                poColl->addGeometryDirectly(poPoly);
                poLS = poLR;
            }
            else
            {
                poLS = new OGRLineString();
                poColl->addGeometryDirectly(poLS);
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

    if( poColl->getNumGeometries() == 0 )
    {
        delete poColl;
        poColl = NULL;
    }

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
    if( nWayFeaturePairs != 0 )
        ProcessWaysBatch();

    nRelationsProcessed++;
    if( (nRelationsProcessed % 10000) == 0 )
    {
        CPLDebug( "OSM", "Relations processed : %d", nRelationsProcessed );
#ifdef DEBUG_MEM_USAGE
        CPLDebug( "OSM",
                  "GetMaxTotalAllocs() = " CPL_FRMT_GUIB,
                  static_cast<GUIntBig>(GetMaxTotalAllocs()) );
#endif
    }

    if( !bUseWaysIndex )
        return;

    bool bMultiPolygon = false;
    bool bMultiLineString = false;
    bool bInterestingTagFound = false;
    const char* pszTypeV = NULL;
    for( unsigned int i = 0; i < psRelation->nTags; i ++ )
    {
        const char* pszK = psRelation->pasTags[i].pszK;
        if( strcmp(pszK, "type") == 0 )
        {
            const char* pszV = psRelation->pasTags[i].pszV;
            pszTypeV = pszV;
            if( strcmp(pszV, "multipolygon") == 0 ||
                strcmp(pszV, "boundary") == 0)
            {
                bMultiPolygon = true;
            }
            else if( strcmp(pszV, "multilinestring") == 0 ||
                     strcmp(pszV, "route") == 0 )
            {
                bMultiLineString = true;
            }
        }
        else if( strcmp(pszK, "created_by") != 0 )
            bInterestingTagFound = true;
    }

    // Optimization: If we have an attribute filter, that does not require
    // geometry, then we can just evaluate the attribute filter without the
    // geometry.
    const int iCurLayer =
        bMultiPolygon ?    IDX_LYR_MULTIPOLYGONS :
        bMultiLineString ? IDX_LYR_MULTILINESTRINGS :
        IDX_LYR_OTHER_RELATIONS;
    if( !papoLayers[iCurLayer]->IsUserInterested() )
        return;

    OGRFeature* poFeature = NULL;

    if( !(bMultiPolygon && !bInterestingTagFound) &&
        // We cannot do early filtering for multipolygon that has no
        // interesting tag, since we may fetch attributes from ways.
        papoLayers[iCurLayer]->HasAttributeFilter() &&
        !papoLayers[iCurLayer]->AttributeFilterEvaluationNeedsGeometry() )
    {
        poFeature = new OGRFeature(papoLayers[iCurLayer]->GetLayerDefn());

        papoLayers[iCurLayer]->SetFieldsFromTags( poFeature,
                                                  psRelation->nID,
                                                  false,
                                                  psRelation->nTags,
                                                  psRelation->pasTags,
                                                  &psRelation->sInfo);

        if( !papoLayers[iCurLayer]->EvaluateAttributeFilter(poFeature) )
        {
            delete poFeature;
            return;
        }
    }

    OGRGeometry* poGeom = NULL;

    unsigned int nExtraTags = 0;
    OSMTag pasExtraTags[1 + MAX_COUNT_FOR_TAGS_IN_WAY];

    if( bMultiPolygon )
    {
        if( !bInterestingTagFound )
        {
            poGeom = BuildMultiPolygon(psRelation, &nExtraTags, pasExtraTags);
            CPLAssert(nExtraTags <=
                      static_cast<unsigned int>(MAX_COUNT_FOR_TAGS_IN_WAY));
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
        bool bAttrFilterAlreadyEvaluated = true;
        if( poFeature == NULL )
        {
            poFeature = new OGRFeature(papoLayers[iCurLayer]->GetLayerDefn());

            papoLayers[iCurLayer]->SetFieldsFromTags(
                poFeature,
                psRelation->nID,
                false,
                nExtraTags ? nExtraTags : psRelation->nTags,
                nExtraTags ? pasExtraTags : psRelation->pasTags,
                &psRelation->sInfo);

            bAttrFilterAlreadyEvaluated = false;
        }

        poFeature->SetGeometryDirectly(poGeom);

        int bFilteredOut = FALSE;
        if( !papoLayers[iCurLayer]->AddFeature( poFeature,
                                                bAttrFilterAlreadyEvaluated,
                                                &bFilteredOut,
                                                !bFeatureAdded ) )
            bStopParsing = true;
        else if( !bFilteredOut )
            bFeatureAdded = true;
    }
    else
    {
        delete poFeature;
    }
}

static void OGROSMNotifyRelation ( OSMRelation *psRelation,
                                   OSMContext * /* psOSMContext */,
                                   void *user_data)
{
    static_cast<OGROSMDataSource *>(user_data)->NotifyRelation(psRelation);
}

/************************************************************************/
/*                      ProcessPolygonsStandalone()                     */
/************************************************************************/

void OGROSMDataSource::ProcessPolygonsStandalone()
{
    unsigned int nTags = 0;
    OSMTag pasTags[MAX_COUNT_FOR_TAGS_IN_WAY];
    OSMInfo sInfo;

    sInfo.ts.nTimeStamp = 0;
    sInfo.nChangeset = 0;
    sInfo.nVersion = 0;
    sInfo.nUID = 0;
    sInfo.bTimeStampIsStr = false;
    sInfo.pszUserSID = "";

    if( !bHasRowInPolygonsStandalone )
        bHasRowInPolygonsStandalone =
            sqlite3_step(hSelectPolygonsStandaloneStmt) == SQLITE_ROW;

    bool bFirst = true;

    while( bHasRowInPolygonsStandalone &&
           papoLayers[IDX_LYR_MULTIPOLYGONS]->nFeatureArraySize < 10000 )
    {
        if( bFirst )
        {
            CPLDebug( "OSM", "Remaining standalone polygons" );
            bFirst = false;
        }

        GIntBig id = sqlite3_column_int64(hSelectPolygonsStandaloneStmt, 0);

        sqlite3_bind_int64( pahSelectWayStmt[0], 1, id );
        if( sqlite3_step(pahSelectWayStmt[0]) == SQLITE_ROW )
        {
            int nBlobSize = sqlite3_column_bytes(pahSelectWayStmt[0], 1);
            const void* blob = sqlite3_column_blob(pahSelectWayStmt[0], 1);

            LonLat* pasCoords = reinterpret_cast<LonLat *>(pasLonLatCache);

            const int nPoints = UncompressWay(
                nBlobSize, reinterpret_cast<GByte *>(const_cast<void *>(blob)),
                NULL, pasCoords, &nTags, pasTags, &sInfo );
            CPLAssert(
                nTags <= static_cast<unsigned int>(MAX_COUNT_FOR_TAGS_IN_WAY));

            OGRMultiPolygon* poMulti = new OGRMultiPolygon();
            OGRPolygon* poPoly = new OGRPolygon();
            OGRLinearRing* poRing = new OGRLinearRing();
            poMulti->addGeometryDirectly(poPoly);
            poPoly->addRingDirectly(poRing);
            OGRLineString* poLS = poRing;

            poLS->setNumPoints(nPoints);
            for(int j=0;j<nPoints;j++)
            {
                poLS->setPoint( j,
                                INT_TO_DBL(pasCoords[j].nLon),
                                INT_TO_DBL(pasCoords[j].nLat) );
            }

            OGRFeature* poFeature =
                new OGRFeature(
                    papoLayers[IDX_LYR_MULTIPOLYGONS]->GetLayerDefn());

            papoLayers[IDX_LYR_MULTIPOLYGONS]->SetFieldsFromTags( poFeature,
                                                                  id,
                                                                  true,
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
                bStopParsing = true;
                break;
            }
            else if( !bFilteredOut )
            {
                bFeatureAdded = true;
            }
        }
        else
        {
            CPLAssert(false);
        }

        sqlite3_reset(pahSelectWayStmt[0]);

        bHasRowInPolygonsStandalone =
            sqlite3_step(hSelectPolygonsStandaloneStmt) == SQLITE_ROW;
    }
}

/************************************************************************/
/*                             NotifyBounds()                           */
/************************************************************************/

void OGROSMDataSource::NotifyBounds ( double dfXMin, double dfYMin,
                                      double dfXMax, double dfYMax )
{
    sExtent.MinX = dfXMin;
    sExtent.MinY = dfYMin;
    sExtent.MaxX = dfXMax;
    sExtent.MaxY = dfYMax;
    bExtentValid = true;

    CPLDebug( "OSM", "Got bounds : minx=%f, miny=%f, maxx=%f, maxy=%f",
              dfXMin, dfYMin, dfXMax, dfYMax );
}

static void OGROSMNotifyBounds( double dfXMin, double dfYMin,
                                double dfXMax, double dfYMax,
                                OSMContext* /* psCtxt */,
                                void* user_data )
{
    static_cast<OGROSMDataSource *>(user_data)->NotifyBounds( dfXMin, dfYMin,
                                                              dfXMax, dfYMax );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROSMDataSource::Open( const char * pszFilename,
                            char** papszOpenOptionsIn )

{
    pszName = CPLStrdup( pszFilename );

    psParser = OSM_Open( pszName,
                         OGROSMNotifyNodes,
                         OGROSMNotifyWay,
                         OGROSMNotifyRelation,
                         OGROSMNotifyBounds,
                         this );
    if( psParser == NULL )
        return FALSE;

    if( CPLFetchBool(papszOpenOptionsIn, "INTERLEAVED_READING", false) )
        bInterleavedReading = TRUE;

    /* The following 4 config options are only useful for debugging */
    bIndexPoints = CPLTestBool(CPLGetConfigOption("OSM_INDEX_POINTS", "YES"));
    bUsePointsIndex = CPLTestBool(
        CPLGetConfigOption("OSM_USE_POINTS_INDEX", "YES"));
    bIndexWays = CPLTestBool(CPLGetConfigOption("OSM_INDEX_WAYS", "YES"));
    bUseWaysIndex = CPLTestBool(
        CPLGetConfigOption("OSM_USE_WAYS_INDEX", "YES"));

    bCustomIndexing = CPLTestBool(CSLFetchNameValueDef(
            papszOpenOptionsIn, "USE_CUSTOM_INDEXING",
                        CPLGetConfigOption("OSM_USE_CUSTOM_INDEXING", "YES")));
    if( !bCustomIndexing )
        CPLDebug("OSM", "Using SQLite indexing for points");
    bCompressNodes = CPLTestBool(CSLFetchNameValueDef(
            papszOpenOptionsIn, "COMPRESS_NODES",
                        CPLGetConfigOption("OSM_COMPRESS_NODES", "NO")));
    if( bCompressNodes )
        CPLDebug("OSM", "Using compression for nodes DB");

    nLayers = 5;
    papoLayers = static_cast<OGROSMLayer **>(
        CPLMalloc(nLayers * sizeof(OGROSMLayer*)) );

    papoLayers[IDX_LYR_POINTS] =
        new OGROSMLayer(this, IDX_LYR_POINTS, "points");
    papoLayers[IDX_LYR_POINTS]->GetLayerDefn()->SetGeomType(wkbPoint);

    papoLayers[IDX_LYR_LINES] = new OGROSMLayer(this, IDX_LYR_LINES, "lines");
    papoLayers[IDX_LYR_LINES]->GetLayerDefn()->SetGeomType(wkbLineString);

    papoLayers[IDX_LYR_MULTILINESTRINGS] =
        new OGROSMLayer(this, IDX_LYR_MULTILINESTRINGS, "multilinestrings");
    papoLayers[IDX_LYR_MULTILINESTRINGS]->GetLayerDefn()->
        SetGeomType(wkbMultiLineString);

    papoLayers[IDX_LYR_MULTIPOLYGONS] =
        new OGROSMLayer(this, IDX_LYR_MULTIPOLYGONS, "multipolygons");
    papoLayers[IDX_LYR_MULTIPOLYGONS]->GetLayerDefn()->
        SetGeomType(wkbMultiPolygon);

    papoLayers[IDX_LYR_OTHER_RELATIONS] =
        new OGROSMLayer(this, IDX_LYR_OTHER_RELATIONS, "other_relations");
    papoLayers[IDX_LYR_OTHER_RELATIONS]->GetLayerDefn()->
        SetGeomType(wkbGeometryCollection);

    if( !ParseConf(papszOpenOptionsIn) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Could not parse configuration file for OSM import");
        return FALSE;
    }

    bNeedsToSaveWayInfo =
        ( papoLayers[IDX_LYR_MULTIPOLYGONS]->HasTimestamp() ||
          papoLayers[IDX_LYR_MULTIPOLYGONS]->HasChangeset() ||
          papoLayers[IDX_LYR_MULTIPOLYGONS]->HasVersion() ||
          papoLayers[IDX_LYR_MULTIPOLYGONS]->HasUID() ||
          papoLayers[IDX_LYR_MULTIPOLYGONS]->HasUser() );

    pasLonLatCache = static_cast<LonLat*>(
        VSI_MALLOC_VERBOSE(MAX_NODES_PER_WAY * sizeof(LonLat)));
    pabyWayBuffer = static_cast<GByte*>(VSI_MALLOC_VERBOSE(WAY_BUFFER_SIZE));

    panReqIds = static_cast<GIntBig*>(
        VSI_MALLOC_VERBOSE(MAX_ACCUMULATED_NODES * sizeof(GIntBig)));
#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    panHashedIndexes = static_cast<int*>(
        VSI_MALLOC_VERBOSE(HASHED_INDEXES_ARRAY_SIZE * sizeof(int)));
    psCollisionBuckets = static_cast<CollisionBucket*>(
        VSI_MALLOC_VERBOSE(COLLISION_BUCKET_ARRAY_SIZE *
                           sizeof(CollisionBucket)));
#endif
    pasLonLatArray = static_cast<LonLat*>(
        VSI_MALLOC_VERBOSE(MAX_ACCUMULATED_NODES * sizeof(LonLat)));
    panUnsortedReqIds = static_cast<GIntBig*>(
        VSI_MALLOC_VERBOSE(MAX_ACCUMULATED_NODES * sizeof(GIntBig)));
    pasWayFeaturePairs = static_cast<WayFeaturePair*>(
        VSI_MALLOC_VERBOSE(MAX_DELAYED_FEATURES * sizeof(WayFeaturePair)));
    pasAccumulatedTags = static_cast<IndexedKVP*>(
        VSI_MALLOC_VERBOSE(MAX_ACCUMULATED_TAGS * sizeof(IndexedKVP)) );
    pabyNonRedundantValues = static_cast<GByte*>(
        VSI_MALLOC_VERBOSE(MAX_NON_REDUNDANT_VALUES) );

    if( pasLonLatCache == NULL ||
        pabyWayBuffer == NULL ||
        panReqIds == NULL ||
        pasLonLatArray == NULL ||
        panUnsortedReqIds == NULL ||
        pasWayFeaturePairs == NULL ||
        pasAccumulatedTags == NULL ||
        pabyNonRedundantValues == NULL )
    {
        return FALSE;
    }

    nMaxSizeForInMemoryDBInMB = atoi(CSLFetchNameValueDef(papszOpenOptionsIn,
        "MAX_TMPFILE_SIZE", CPLGetConfigOption("OSM_MAX_TMPFILE_SIZE", "100")));
    GIntBig nSize =
        static_cast<GIntBig>(nMaxSizeForInMemoryDBInMB) * 1024 * 1024;
    if( nSize < 0 || (GIntBig)(size_t)nSize != nSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid value for OSM_MAX_TMPFILE_SIZE. Using 100 instead." );
        nMaxSizeForInMemoryDBInMB = 100;
        nSize = static_cast<GIntBig>(nMaxSizeForInMemoryDBInMB) * 1024 * 1024;
    }

    if( bCustomIndexing )
    {
        pabySector = static_cast<GByte *>(VSI_CALLOC_VERBOSE(1, SECTOR_SIZE));

        if( pabySector == NULL )
        {
            return FALSE;
        }

        bInMemoryNodesFile = true;
        osNodesFilename.Printf("/vsimem/osm_importer/osm_temp_nodes_%p", this);
        fpNodes = VSIFOpenL(osNodesFilename, "wb+");
        if( fpNodes == NULL )
        {
            return FALSE;
        }

        CPLPushErrorHandler(CPLQuietErrorHandler);
        bool bSuccess =
            VSIFSeekL(fpNodes, (vsi_l_offset) (nSize * 3 / 4), SEEK_SET) == 0;
        CPLPopErrorHandler();

        if( bSuccess )
        {
            VSIFSeekL(fpNodes, 0, SEEK_SET);
            VSIFTruncateL(fpNodes, 0);
        }
        else
        {
            CPLDebug( "OSM",
                      "Not enough memory for in-memory file. "
                      "Using disk temporary file instead." );

            VSIFCloseL(fpNodes);
            fpNodes = NULL;
            VSIUnlink(osNodesFilename);

            bInMemoryNodesFile = false;
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

    const bool bRet = CreateTempDB();
    if( bRet )
    {
        CPLString osInterestLayers = GetInterestLayersForDSName(GetName());
        if( !osInterestLayers.empty() )
        {
            delete ExecuteSQL( osInterestLayers, NULL, NULL );
        }
    }
    return bRet;
}

/************************************************************************/
/*                             CreateTempDB()                           */
/************************************************************************/

bool OGROSMDataSource::CreateTempDB()
{
    char* pszErrMsg = NULL;

    int rc = 0;
    bool bIsExisting = false;
    bool bSuccess = false;

    const char* pszExistingTmpFile = CPLGetConfigOption("OSM_EXISTING_TMPFILE", NULL);
    if( pszExistingTmpFile != NULL )
    {
        bSuccess = true;
        bIsExisting = true;
        rc = sqlite3_open_v2( pszExistingTmpFile, &hDB,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                              NULL );
    }
    else
    {
        osTmpDBName.Printf("/vsimem/osm_importer/osm_temp_%p.sqlite", this);

        // On 32 bit, the virtual memory space is scarce, so we need to
        // reserve it right now. Will not hurt on 64 bit either.
        VSILFILE* fp = VSIFOpenL(osTmpDBName, "wb");
        if( fp )
        {
            GIntBig nSize =
                static_cast<GIntBig>(nMaxSizeForInMemoryDBInMB) * 1024 * 1024;
            if( bCustomIndexing && bInMemoryNodesFile )
                nSize = nSize / 4;

            CPLPushErrorHandler(CPLQuietErrorHandler);
            bSuccess =
                VSIFSeekL(fp, static_cast<vsi_l_offset>(nSize), SEEK_SET) == 0;
            CPLPopErrorHandler();

            if( bSuccess )
                 bSuccess = VSIFTruncateL(fp, 0) == 0;

            VSIFCloseL(fp);

            if( !bSuccess )
            {
                CPLDebug( "OSM",
                          "Not enough memory for in-memory file. "
                          "Using disk temporary file instead.");
                VSIUnlink(osTmpDBName);
            }
        }

        if( bSuccess )
        {
            bInMemoryTmpDB = true;
            pMyVFS = OGRSQLiteCreateVFS(NULL, this);
            sqlite3_vfs_register(pMyVFS, 0);
            rc = sqlite3_open_v2(
                osTmpDBName.c_str(), &hDB,
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                SQLITE_OPEN_NOMUTEX,
                pMyVFS->zName );
        }
    }

    if( !bSuccess )
    {
        osTmpDBName = CPLGenerateTempFilename("osm_tmp");
        rc = sqlite3_open( osTmpDBName.c_str(), &hDB );

        /* On Unix filesystems, you can remove a file even if it */
        /* opened */
        if( rc == SQLITE_OK )
        {
            const char* pszVal =
                CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
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
        return false;
    }

    if( !SetDBOptions() )
    {
        return false;
    }

    if( !bIsExisting )
    {
        rc = sqlite3_exec(
            hDB,
            "CREATE TABLE nodes (id INTEGER PRIMARY KEY, coords BLOB)",
            NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to create table nodes : %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return false;
        }

        rc = sqlite3_exec(
            hDB,
            "CREATE TABLE ways (id INTEGER PRIMARY KEY, data BLOB)",
            NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to create table ways : %s", pszErrMsg );
            sqlite3_free( pszErrMsg );
            return false;
        }

        rc = sqlite3_exec(
            hDB,
            "CREATE TABLE polygons_standalone (id INTEGER PRIMARY KEY)",
            NULL, NULL, &pszErrMsg );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to create table polygons_standalone : %s",
                      pszErrMsg );
            sqlite3_free( pszErrMsg );
            return false;
        }
    }

    return CreatePreparedStatements();
}

/************************************************************************/
/*                            SetDBOptions()                            */
/************************************************************************/

bool OGROSMDataSource::SetDBOptions()
{
    char* pszErrMsg = NULL;
    int rc =
        sqlite3_exec( hDB, "PRAGMA synchronous = OFF", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA synchronous : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return false;
    }

    rc = sqlite3_exec(
        hDB, "PRAGMA journal_mode = OFF", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA journal_mode : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return false;
    }

    rc = sqlite3_exec(
        hDB, "PRAGMA temp_store = MEMORY", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Unable to run PRAGMA temp_store : %s",
                    pszErrMsg );
        sqlite3_free( pszErrMsg );
        return false;
    }

    if( !SetCacheSize() )
        return false;

    if( !StartTransactionCacheDB() )
        return false;

    return true;
}

/************************************************************************/
/*                              SetCacheSize()                          */
/************************************************************************/

bool OGROSMDataSource::SetCacheSize()
{
    const char* pszSqliteCacheMB = CPLGetConfigOption("OSM_SQLITE_CACHE", NULL);

    if( pszSqliteCacheMB == NULL )
        return true;

    char* pszErrMsg = NULL;
    char **papszResult = NULL;
    int nRowCount = 0;
    int nColCount = 0;
    int iSqlitePageSize = -1;
    const GIntBig iSqliteCacheBytes =
            static_cast<GIntBig>(atoi( pszSqliteCacheMB )) * 1024 * 1024;

    /* querying the current PageSize */
    int rc = sqlite3_get_table( hDB, "PRAGMA page_size",
                                &papszResult, &nRowCount, &nColCount,
                                &pszErrMsg );
    if( rc == SQLITE_OK )
    {
        for( int iRow = 1; iRow <= nRowCount; iRow++ )
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
        return true;
    }
    if( iSqlitePageSize == 0 )
        return true;

    /* computing the CacheSize as #Pages */
    const int iSqliteCachePages = static_cast<int>(
                                    iSqliteCacheBytes / iSqlitePageSize);
    if( iSqliteCachePages <= 0)
        return true;

    rc = sqlite3_exec( hDB, CPLSPrintf( "PRAGMA cache_size = %d",
                                        iSqliteCachePages ),
                       NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unrecognized value for PRAGMA cache_size : %s",
                  pszErrMsg );
        sqlite3_free( pszErrMsg );
    }

    return true;
}

/************************************************************************/
/*                        CreatePreparedStatements()                    */
/************************************************************************/

bool OGROSMDataSource::CreatePreparedStatements()
{
    int rc =
        sqlite3_prepare_v2( hDB,
                            "INSERT INTO nodes (id, coords) VALUES (?,?)", -1,
                            &hInsertNodeStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(hDB) );
        return false;
    }

    pahSelectNodeStmt = static_cast<sqlite3_stmt**>(
        CPLCalloc(sizeof(sqlite3_stmt*), LIMIT_IDS_PER_REQUEST) );

    char szTmp[LIMIT_IDS_PER_REQUEST*2 + 128];
    strcpy(szTmp, "SELECT id, coords FROM nodes WHERE id IN (");
    int nLen = static_cast<int>(strlen(szTmp));
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
        rc = sqlite3_prepare_v2( hDB, szTmp, -1, &pahSelectNodeStmt[i], NULL );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(hDB) );
            return false;
        }
    }

    rc = sqlite3_prepare_v2( hDB, "INSERT INTO ways (id, data) VALUES (?,?)", -1,
                          &hInsertWayStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(hDB) );
        return false;
    }

    pahSelectWayStmt = static_cast<sqlite3_stmt**>(
        CPLCalloc(sizeof(sqlite3_stmt*), LIMIT_IDS_PER_REQUEST) );

    strcpy(szTmp, "SELECT id, data FROM ways WHERE id IN (");
    nLen = static_cast<int>(strlen(szTmp));
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
        rc = sqlite3_prepare_v2( hDB, szTmp, -1, &pahSelectWayStmt[i], NULL );
        if( rc != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(hDB) );
            return false;
        }
    }

    rc = sqlite3_prepare_v2(
        hDB, "INSERT INTO polygons_standalone (id) VALUES (?)", -1,
        &hInsertPolygonsStandaloneStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(hDB) );
        return false;
    }

    rc = sqlite3_prepare_v2(
        hDB, "DELETE FROM polygons_standalone WHERE id = ?", -1,
        &hDeletePolygonsStandaloneStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(hDB) );
        return false;
    }

    rc = sqlite3_prepare_v2(
        hDB, "SELECT id FROM polygons_standalone ORDER BY id", -1,
        &hSelectPolygonsStandaloneStmt, NULL );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(hDB) );
        return false;
    }

    return true;
}

/************************************************************************/
/*                      StartTransactionCacheDB()                       */
/************************************************************************/

bool OGROSMDataSource::StartTransactionCacheDB()
{
    if( bInTransaction )
        return false;

    char* pszErrMsg = NULL;
    int rc = sqlite3_exec( hDB, "BEGIN", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to start transaction : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return false;
    }

    bInTransaction = true;

    return true;
}

/************************************************************************/
/*                        CommitTransactionCacheDB()                    */
/************************************************************************/

bool OGROSMDataSource::CommitTransactionCacheDB()
{
    if( !bInTransaction )
        return false;

    bInTransaction = false;

    char* pszErrMsg = NULL;
    int rc = sqlite3_exec( hDB, "COMMIT", NULL, NULL, &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to commit transaction : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return false;
    }

    return true;
}

/************************************************************************/
/*                     AddComputedAttributes()                          */
/************************************************************************/

void OGROSMDataSource::AddComputedAttributes(
    int iCurLayer,
    const std::vector<OGROSMComputedAttribute>& oAttributes)
{
    for(size_t i=0; i<oAttributes.size();i++)
    {
        if( !oAttributes[i].osSQL.empty() )
        {
            papoLayers[iCurLayer]->AddComputedAttribute(oAttributes[i].osName,
                                                        oAttributes[i].eType,
                                                        oAttributes[i].osSQL);
        }
    }
}

/************************************************************************/
/*                           ParseConf()                                */
/************************************************************************/

bool OGROSMDataSource::ParseConf( char** papszOpenOptionsIn )
{
    const char *pszFilename =
        CSLFetchNameValueDef(papszOpenOptionsIn, "CONFIG_FILE",
                             CPLGetConfigOption("OSM_CONFIG_FILE", NULL));
    if( pszFilename == NULL )
        pszFilename = CPLFindFile( "gdal", "osmconf.ini" );
    if( pszFilename == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Cannot find osmconf.ini configuration file");
        return false;
    }

    VSILFILE* fpConf = VSIFOpenL(pszFilename, "rb");
    if( fpConf == NULL )
        return false;

    const char* pszLine = NULL;
    int iCurLayer = -1;
    std::vector<OGROSMComputedAttribute> oAttributes;

    while((pszLine = CPLReadLine2L(fpConf, -1, NULL)) != NULL)
    {
        if(pszLine[0] == '#')
            continue;
        if(pszLine[0] == '[' && pszLine[strlen(pszLine)-1] == ']' )
        {
            if( iCurLayer >= 0 )
                AddComputedAttributes(iCurLayer, oAttributes);
            oAttributes.resize(0);

            iCurLayer = -1;
            pszLine ++;
            ((char*)pszLine)[strlen(pszLine)-1] = '\0'; /* Evil but OK */
            for(int i = 0; i < nLayers; i++)
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
                         "Layer '%s' mentioned in %s is unknown to the driver",
                         pszLine, pszFilename);
            }
            continue;
        }

        if( STARTS_WITH(pszLine, "closed_ways_are_polygons="))
        {
            char** papszTokens2 = CSLTokenizeString2(
                    pszLine + strlen("closed_ways_are_polygons="), ",", 0);
            nMinSizeKeysInSetClosedWaysArePolygons = INT_MAX;
            nMaxSizeKeysInSetClosedWaysArePolygons = 0;
            for(int i=0;papszTokens2[i] != NULL;i++)
            {
                const int nTokenSize = static_cast<int>(strlen(papszTokens2[i]));
                aoSetClosedWaysArePolygons.insert(papszTokens2[i]);
                nMinSizeKeysInSetClosedWaysArePolygons = std::min(
                    nMinSizeKeysInSetClosedWaysArePolygons, nTokenSize);
                nMaxSizeKeysInSetClosedWaysArePolygons = std::max(
                    nMinSizeKeysInSetClosedWaysArePolygons, nTokenSize);
            }
            CSLDestroy(papszTokens2);
        }

        else if(STARTS_WITH(pszLine, "report_all_nodes="))
        {
            if( strcmp(pszLine + strlen("report_all_nodes="), "no") == 0 )
            {
                bReportAllNodes = false;
            }
            else if( strcmp(pszLine + strlen("report_all_nodes="), "yes") == 0 )
            {
                bReportAllNodes = true;
            }
        }

        else if(STARTS_WITH(pszLine, "report_all_ways="))
        {
            if( strcmp(pszLine + strlen("report_all_ways="), "no") == 0 )
            {
                bReportAllWays = false;
            }
            else if( strcmp(pszLine + strlen("report_all_ways="), "yes") == 0 )
            {
                bReportAllWays = true;
            }
        }

        else if(STARTS_WITH(pszLine, "attribute_name_laundering="))
        {
            if( strcmp(pszLine + strlen("attribute_name_laundering="), "no") == 0 )
            {
                bAttributeNameLaundering = false;
            }
            else if( strcmp(pszLine + strlen("attribute_name_laundering="), "yes") == 0 )
            {
                bAttributeNameLaundering = true;
            }
        }

        else if( iCurLayer >= 0 )
        {
            char** papszTokens = CSLTokenizeString2(pszLine, "=", 0);
            if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "other_tags") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasOtherTags(false);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                    papoLayers[iCurLayer]->SetHasOtherTags(true);
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "all_tags") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasAllTags(false);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                    papoLayers[iCurLayer]->SetHasAllTags(true);
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_id") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasOSMId(false);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasOSMId(true);
                    papoLayers[iCurLayer]->AddField("osm_id", OFTString);

                    if( iCurLayer == IDX_LYR_MULTIPOLYGONS )
                        papoLayers[iCurLayer]->AddField("osm_way_id", OFTString);
                }
            }
             else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_version") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasVersion(false);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasVersion(true);
                    papoLayers[iCurLayer]->AddField("osm_version", OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_timestamp") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasTimestamp(false);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasTimestamp(true);
                    papoLayers[iCurLayer]->AddField("osm_timestamp", OFTDateTime);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_uid") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasUID(false);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasUID(true);
                    papoLayers[iCurLayer]->AddField("osm_uid", OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 && strcmp(papszTokens[0], "osm_user") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasUser(false);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasUser(true);
                    papoLayers[iCurLayer]->AddField("osm_user", OFTString);
                }
            }
            else if( CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "osm_changeset") == 0 )
            {
                if( strcmp(papszTokens[1], "no") == 0 )
                    papoLayers[iCurLayer]->SetHasChangeset(false);
                else if( strcmp(papszTokens[1], "yes") == 0 )
                {
                    papoLayers[iCurLayer]->SetHasChangeset(true);
                    papoLayers[iCurLayer]->AddField("osm_changeset",
                                                    OFTInteger);
                }
            }
            else if( CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "attributes") == 0 )
            {
                char** papszTokens2 =
                    CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->AddField(papszTokens2[i], OFTString);
                }
                CSLDestroy(papszTokens2);
            }
            else if( CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "unsignificant") == 0 )
            {
                char** papszTokens2 =
                    CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->AddUnsignificantKey(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            else if( CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "ignore") == 0 )
            {
                char** papszTokens2 =
                    CSLTokenizeString2(papszTokens[1], ",", 0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    papoLayers[iCurLayer]->AddIgnoreKey(papszTokens2[i]);
                    papoLayers[iCurLayer]->AddWarnKey(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            else if( CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "computed_attributes") == 0 )
            {
                char** papszTokens2 =
                    CSLTokenizeString2(papszTokens[1], ",", 0);
                oAttributes.resize(0);
                for(int i=0;papszTokens2[i] != NULL;i++)
                {
                    oAttributes.push_back(
                        OGROSMComputedAttribute(papszTokens2[i]));
                }
                CSLDestroy(papszTokens2);
            }
            else if( CSLCount(papszTokens) == 2 &&
                     strlen(papszTokens[0]) >= 5 &&
                     strcmp(papszTokens[0] + strlen(papszTokens[0]) - 5,
                            "_type") == 0 )
            {
                CPLString osName(papszTokens[0]);
                osName.resize(strlen(papszTokens[0]) - 5);
                const char* pszType = papszTokens[1];
                bool bFound = false;
                OGRFieldType eType = OFTString;
                if( EQUAL(pszType, "Integer") )
                    eType = OFTInteger;
                else if( EQUAL(pszType, "Integer64") )
                    eType = OFTInteger64;
                else if( EQUAL(pszType, "Real") )
                    eType = OFTReal;
                else if( EQUAL(pszType, "String") )
                    eType = OFTString;
                else if( EQUAL(pszType, "DateTime") )
                    eType = OFTDateTime;
                else
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "Unhandled type (%s) for attribute %s",
                                pszType, osName.c_str());
                for(size_t i = 0; i < oAttributes.size(); i++ )
                {
                    if( oAttributes[i].osName == osName )
                    {
                        bFound = true;
                        oAttributes[i].eType = eType;
                        break;
                    }
                }
                if( !bFound )
                {
                    const int idx =
                        papoLayers[iCurLayer]->
                            GetLayerDefn()->GetFieldIndex(osName);
                    if( idx >= 0 )
                    {
                        papoLayers[iCurLayer]->
                            GetLayerDefn()->GetFieldDefn(idx)->SetType(eType);
                        bFound = true;
                    }
                }
                if( !bFound )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Undeclared attribute : %s",
                             osName.c_str());
                }
            }
            else if( CSLCount(papszTokens) >= 2 &&
                     strlen(papszTokens[0]) >= 4 &&
                     strcmp(papszTokens[0] + strlen(papszTokens[0]) - 4,
                            "_sql") == 0 )
            {
                CPLString osName(papszTokens[0]);
                osName.resize(strlen(papszTokens[0]) - 4);
                size_t i = 0;  // Used after for.
                for( ; i < oAttributes.size(); i++ )
                {
                    if( oAttributes[i].osName == osName )
                    {
                        const char* pszSQL = strchr(pszLine, '=') + 1;
                        while( *pszSQL == ' ' )
                            pszSQL ++;
                        bool bInQuotes = false;
                        if( *pszSQL == '"' )
                        {
                            bInQuotes = true;
                            pszSQL ++;
                        }
                        oAttributes[i].osSQL = pszSQL;
                        if( bInQuotes && oAttributes[i].osSQL.size() > 1 &&
                            oAttributes[i].osSQL.back() == '"' )
                            oAttributes[i].osSQL.resize(oAttributes[i].osSQL.size()-1);
                        break;
                    }
                }
                if( i == oAttributes.size() )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Undeclared attribute : %s",
                              osName.c_str());
                }
            }
            CSLDestroy(papszTokens);
        }
    }

    if( iCurLayer >= 0 )
        AddComputedAttributes(iCurLayer, oAttributes);

    for(int i=0;i<nLayers;i++)
    {
        if( papoLayers[i]->HasAllTags() )
        {
            papoLayers[i]->AddField("all_tags", OFTString);
            if( papoLayers[i]->HasOtherTags() )
            {
                papoLayers[i]->SetHasOtherTags(false);
            }
        }
        else if( papoLayers[i]->HasOtherTags() )
            papoLayers[i]->AddField("other_tags", OFTString);
    }

    VSIFCloseL(fpConf);

    return true;
}

/************************************************************************/
/*                          MyResetReading()                            */
/************************************************************************/

int OGROSMDataSource::MyResetReading()
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

    rc = sqlite3_exec( hDB, "DELETE FROM polygons_standalone", NULL, NULL,
                       &pszErrMsg );
    if( rc != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to DELETE FROM polygons_standalone : %s", pszErrMsg );
        sqlite3_free( pszErrMsg );
        return FALSE;
    }
    bHasRowInPolygonsStandalone = false;

    if( hSelectPolygonsStandaloneStmt != NULL )
        sqlite3_reset( hSelectPolygonsStandaloneStmt );

    {
        for( int i = 0; i < nWayFeaturePairs; i++)
        {
            delete pasWayFeaturePairs[i].poFeature;
        }
        nWayFeaturePairs = 0;
        nUnsortedReqIds = 0;
        nReqIds = 0;
        nAccumulatedTags = 0;
        nNonRedundantValuesLen = 0;

        for( int i=0;i<static_cast<int>(asKeys.size()); i++ )
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

        std::map<int, Bucket>::iterator oIter = oMapBuckets.begin();
        for( ; oIter != oMapBuckets.end(); ++oIter )
        {
            Bucket* psBucket = &(oIter->second);
            psBucket->nOff = -1;
            if( bCompressNodes )
            {
                if( psBucket->u.panSectorSize )
                    memset(psBucket->u.panSectorSize, 0, BUCKET_SECTOR_SIZE_ARRAY_SIZE);
            }
            else
            {
                if( psBucket->u.pabyBitmap )
                    memset(psBucket->u.pabyBitmap, 0, BUCKET_BITMAP_SIZE);
            }
        }
    }

    for(int i=0;i<nLayers;i++)
    {
        papoLayers[i]->ForceResetReading();
    }

    bStopParsing = false;
    poCurrentLayer = NULL;

    return TRUE;
}

/************************************************************************/
/*                             ResetReading()                           */
/************************************************************************/

void OGROSMDataSource::ResetReading()
{
    MyResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGROSMDataSource::GetNextFeature( OGRLayer** ppoBelongingLayer,
                                              double* pdfProgressPct,
                                              GDALProgressFunc pfnProgress,
                                              void* pProgressData )
{
    bInterleavedReading = TRUE;

    if( poCurrentLayer == NULL )
    {
        poCurrentLayer = papoLayers[0];
    }
    if( pdfProgressPct != NULL || pfnProgress != NULL )
    {
        if( m_nFileSize == FILESIZE_NOT_INIT )
        {
            VSIStatBufL sStat;
            if( VSIStatL( pszName, &sStat ) == 0 )
            {
                m_nFileSize = static_cast<GIntBig>(sStat.st_size);
            }
            else
            {
                m_nFileSize = FILESIZE_INVALID;
            }
        }
    }

    while( true )
    {
        OGROSMLayer* poNewCurLayer = NULL;
        CPLAssert( poCurrentLayer != NULL );
        OGRFeature* poFeature = poCurrentLayer->MyGetNextFeature(&poNewCurLayer,
                                                                 pfnProgress,
                                                                 pProgressData);
        poCurrentLayer = poNewCurLayer;
        if( poFeature == NULL)
        {
            if( poCurrentLayer != NULL )
                continue;
            if( ppoBelongingLayer != NULL )
                *ppoBelongingLayer = NULL;
            if( pdfProgressPct != NULL )
                *pdfProgressPct = 1.0;
            return NULL;
        }
        if( ppoBelongingLayer != NULL )
            *ppoBelongingLayer = poCurrentLayer;
        if( pdfProgressPct != NULL )
        {
            if( m_nFileSize != FILESIZE_INVALID )
            {
                *pdfProgressPct = 1.0 * OSM_GetBytesRead(psParser) /
                                        m_nFileSize;
            }
            else
            {
                *pdfProgressPct = -1.0;
            }
        }

        return poFeature;
    }
}

/************************************************************************/
/*                           ParseNextChunk()                           */
/************************************************************************/

bool OGROSMDataSource::ParseNextChunk( int nIdxLayer,
                                       GDALProgressFunc pfnProgress,
                                       void* pProgressData )
{
    if( bStopParsing )
        return false;

    bHasParsedFirstChunk = true;
    bFeatureAdded = false;
    while( true )
    {
#ifdef DEBUG_MEM_USAGE
        static int counter = 0;
        counter ++;
        if( (counter % 1000) == 0 )
            CPLDebug("OSM", "GetMaxTotalAllocs() = " CPL_FRMT_GUIB,
                     static_cast<GUIntBig>(GetMaxTotalAllocs()));
#endif

        OSMRetCode eRet = OSM_ProcessBlock(psParser);
        if( pfnProgress != NULL )
        {
            double dfPct = -1.0;
            if( m_nFileSize != FILESIZE_INVALID )
            {
                dfPct = 1.0 * OSM_GetBytesRead(psParser) / m_nFileSize;
            }
            if( !pfnProgress( dfPct, "", pProgressData ) )
            {
                bStopParsing = true;
                for(int i=0;i<nLayers;i++)
                {
                    papoLayers[i]->ForceResetReading();
                }
                return false;
            }
        }

        if( eRet == OSM_EOF || eRet == OSM_ERROR )
        {
            if( eRet == OSM_EOF )
            {
                if( nWayFeaturePairs != 0 )
                    ProcessWaysBatch();

                ProcessPolygonsStandalone();

                if( !bHasRowInPolygonsStandalone )
                    bStopParsing = true;

                if( !bInterleavedReading && !bFeatureAdded &&
                    bHasRowInPolygonsStandalone &&
                    nIdxLayer != IDX_LYR_MULTIPOLYGONS )
                {
                    return false;
                }

                return bFeatureAdded || bHasRowInPolygonsStandalone;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "An error occurred during the parsing of data "
                         "around byte " CPL_FRMT_GUIB,
                         OSM_GetBytesRead(psParser));

                bStopParsing = true;
                return false;
            }
        }
        else
        {
            if( bInMemoryTmpDB )
            {
                if( !TransferToDiskIfNecesserary() )
                    return false;
            }

            if( bFeatureAdded )
                break;
        }
    }

    return true;
}

/************************************************************************/
/*                    TransferToDiskIfNecesserary()                     */
/************************************************************************/

bool OGROSMDataSource::TransferToDiskIfNecesserary()
{
    if( bInMemoryNodesFile )
    {
        if( nNodesFileSize / 1024 / 1024 > 3 * nMaxSizeForInMemoryDBInMB / 4 )
        {
            bInMemoryNodesFile = false;

            VSIFCloseL(fpNodes);
            fpNodes = NULL;

            CPLString osNewTmpDBName;
            osNewTmpDBName = CPLGenerateTempFilename("osm_tmp_nodes");

            CPLDebug("OSM", "%s too big for RAM. Transferring it onto disk in %s",
                     osNodesFilename.c_str(), osNewTmpDBName.c_str());

            if( CPLCopyFile( osNewTmpDBName, osNodesFilename ) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot copy %s to %s",
                         osNodesFilename.c_str(), osNewTmpDBName.c_str() );
                VSIUnlink(osNewTmpDBName);
                bStopParsing = true;
                return false;
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
                    GIntBig nNewSize =
                        static_cast<GIntBig>(nMaxSizeForInMemoryDBInMB) *
                        1024 * 1024;
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    const bool bSuccess =
                        VSIFSeekL(fp, (vsi_l_offset) nNewSize, SEEK_SET) == 0;
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
                bStopParsing = true;
                return false;
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
            bInMemoryTmpDB = false;

            CloseDB();

            CPLString osNewTmpDBName;

            osNewTmpDBName = CPLGenerateTempFilename("osm_tmp");

            CPLDebug("OSM", "%s too big for RAM. Transferring it onto disk in %s",
                     osTmpDBName.c_str(), osNewTmpDBName.c_str());

            if( CPLCopyFile( osNewTmpDBName, osTmpDBName ) != 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot copy %s to %s",
                         osTmpDBName.c_str(), osNewTmpDBName.c_str() );
                VSIUnlink(osNewTmpDBName);
                bStopParsing = true;
                return false;
            }

            VSIUnlink(osTmpDBName);

            osTmpDBName = osNewTmpDBName;

            const int rc =
                sqlite3_open_v2( osTmpDBName.c_str(), &hDB,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                                 NULL );
            if( rc != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                        "sqlite3_open(%s) failed: %s",
                        osTmpDBName.c_str(), sqlite3_errmsg( hDB ) );
                bStopParsing = true;
                CloseDB();
                return false;
            }

            /* On Unix filesystems, you can remove a file even if it */
            /* opened */
            const char* pszVal =
                CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if( EQUAL(pszVal, "YES") )
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                bMustUnlink = VSIUnlink( osTmpDBName ) != 0;
                CPLPopErrorHandler();
            }

            if( !SetDBOptions() || !CreatePreparedStatements() )
            {
                bStopParsing = true;
                CloseDB();
                return false;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROSMDataSource::TestCapability( const char * pszCap )
{
    return EQUAL(pszCap, ODsCRandomLayerRead);
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGROSMDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGROSMDataSource::GetExtent( OGREnvelope *psExtent )
{
    if( !bHasParsedFirstChunk )
    {
        bHasParsedFirstChunk = true;
        OSM_ProcessBlock(psParser);
    }

    if( bExtentValid )
    {
        *psExtent = sExtent;
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
                        virtual ~OGROSMSingleFeatureLayer();

    virtual void        ResetReading() override { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeatureDefn *GetLayerDefn() override { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                    OGROSMSingleFeatureLayer()                        */
/************************************************************************/

OGROSMSingleFeatureLayer::OGROSMSingleFeatureLayer( const char* pszLayerName,
                                                    int nValIn ) :
    nVal(nValIn),
    pszVal(NULL),
    poFeatureDefn(new OGRFeatureDefn( "SELECT" )),
    iNextShapeId(0)
{
    poFeatureDefn->Reference();
    OGRFieldDefn oField( pszLayerName, OFTInteger );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                    OGROSMSingleFeatureLayer()                        */
/************************************************************************/

OGROSMSingleFeatureLayer::OGROSMSingleFeatureLayer( const char* pszLayerName,
                                                    const char *pszValIn ) :
    nVal(0),
    pszVal(CPLStrdup(pszValIn)),
    poFeatureDefn(new OGRFeatureDefn( "SELECT" )),
    iNextShapeId(0)
{
    poFeatureDefn->Reference();
    OGRFieldDefn oField( pszLayerName, OFTString );
    poFeatureDefn->AddFieldDefn( &oField );
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
    if( iNextShapeId != 0 )
        return NULL;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    if( pszVal )
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
                                   CPLString osDSNameIn,
                                   CPLString osInterestLayersIn) :
                                        OGRLayerDecorator(poLayer, TRUE),
                                        osDSName(osDSNameIn),
                                        osInterestLayers(osInterestLayersIn) {}

        virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override
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
    if( strcmp(pszSQLCommand, "GetBytesRead()") == 0 )
    {
        char szVal[64] = {};
        snprintf( szVal, sizeof(szVal), CPL_FRMT_GUIB,
                  OSM_GetBytesRead(psParser) );
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
    if( STARTS_WITH(pszSQLCommand, "SET interest_layers =") )
    {
        char** papszTokens =
            CSLTokenizeString2(pszSQLCommand + 21, ",",
                               CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
        for( int i=0; i < nLayers; i++ )
        {
            papoLayers[i]->SetDeclareInterest(FALSE);
        }

        for( int i=0; papszTokens[i] != NULL; i++ )
        {
            OGROSMLayer* poLayer = reinterpret_cast<OGROSMLayer *>(
                GetLayerByName(papszTokens[i]) );
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
                bIndexPoints = false;
            }
            if( CPLGetConfigOption("OSM_USE_POINTS_INDEX", NULL) == NULL )
            {
                bUsePointsIndex = false;
            }
            if( CPLGetConfigOption("OSM_INDEX_WAYS", NULL) == NULL )
            {
                CPLDebug("OSM", "Disabling indexing of ways");
                bIndexWays = false;
            }
            if( CPLGetConfigOption("OSM_USE_WAYS_INDEX", NULL) == NULL )
            {
                bUseWaysIndex = false;
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
                bIndexWays = false;
            }
            if( CPLGetConfigOption("OSM_USE_WAYS_INDEX", NULL) == NULL )
            {
                bUseWaysIndex = false;
            }
        }

        CSLDestroy(papszTokens);

        return NULL;
    }

    while(*pszSQLCommand == ' ')
        pszSQLCommand ++;

    /* Try to analyse the SQL command to get the interest table */
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT") )
    {
        bool bLayerAlreadyAdded = false;
        CPLString osInterestLayers = "SET interest_layers =";

        if( pszDialect != NULL && EQUAL(pszDialect, "SQLITE") )
        {
            std::set<LayerDesc> oSetLayers =
                OGRSQLiteGetReferencedLayers(pszSQLCommand);
            std::set<LayerDesc>::iterator oIter = oSetLayers.begin();
            for(; oIter != oSetLayers.end(); ++oIter)
            {
                const LayerDesc& oLayerDesc = *oIter;
                if( oLayerDesc.osDSName.empty() )
                {
                    if( bLayerAlreadyAdded ) osInterestLayers += ",";
                    bLayerAlreadyAdded = true;
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

            if( eErr == CE_None )
            {
                swq_select* pCurSelect = &sSelectInfo;
                while(pCurSelect != NULL)
                {
                    for( int iTable = 0; iTable < pCurSelect->table_count;
                         iTable++ )
                    {
                        swq_table_def *psTableDef =
                            pCurSelect->table_defs + iTable;
                        if( psTableDef->data_source == NULL )
                        {
                            if( bLayerAlreadyAdded ) osInterestLayers += ",";
                            bLayerAlreadyAdded = true;
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
            delete ExecuteSQL(osInterestLayers, NULL, NULL);

            MyResetReading();

            /* Run the request */
            poResultSetLayer = OGRDataSource::ExecuteSQL( pszSQLCommand,
                                                          poSpatialFilter,
                                                          pszDialect );

            /* If the user explicitly run a COUNT() request, then do it ! */
            if( poResultSetLayer )
            {
                if( pszDialect != NULL && EQUAL(pszDialect, "SQLITE") )
                {
                    poResultSetLayer = new OGROSMResultLayerDecorator(
                                poResultSetLayer, GetName(), osInterestLayers);
                }
                bIsFeatureCountEnabled = true;
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

        bIsFeatureCountEnabled = false;

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
        bInterleavedReading = CPLTestBool(
                        CPLGetConfigOption("OGR_INTERLEAVED_READING", "NO"));
        CPLDebug("OSM", "OGR_INTERLEAVED_READING = %d", bInterleavedReading);
    }
    return bInterleavedReading;
}
