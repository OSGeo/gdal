/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROSMDataSource class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gpb.h"
#include "ogr_osm.h"

#include <cassert>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_port.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogrlayerdecorator.h"
#include "ogrsf_frmts.h"
#include "ogrsqliteexecutesql.h"
#include "osm_parser.h"
#include "ogr_swq.h"
#include "sqlite3.h"

#ifdef EMBED_RESOURCE_FILES
#include "embedded_resources.h"
#endif

#undef SQLITE_STATIC
#define SQLITE_STATIC (static_cast<sqlite3_destructor_type>(nullptr))

constexpr int LIMIT_IDS_PER_REQUEST = 200;

constexpr int IDX_LYR_POINTS = 0;
constexpr int IDX_LYR_LINES = 1;
constexpr int IDX_LYR_MULTILINESTRINGS = 2;
constexpr int IDX_LYR_MULTIPOLYGONS = 3;
constexpr int IDX_LYR_OTHER_RELATIONS = 4;

static int DBL_TO_INT(double x)
{
    return static_cast<int>(floor(x * 1.0e7 + 0.5));
}

static double INT_TO_DBL(int x)
{
    return x / 1.0e7;
}

constexpr unsigned int MAX_COUNT_FOR_TAGS_IN_WAY = 255;  // Must fit on 1 byte.

constexpr int NODE_PER_BUCKET = 65536;

static bool VALID_ID_FOR_CUSTOM_INDEXING(GIntBig _id)
{
    return _id >= 0 && _id / NODE_PER_BUCKET < INT_MAX;
}

// Minimum size of data written on disk, in *uncompressed* case.
constexpr int SECTOR_SIZE = 512;
// Which represents, 64 nodes
// constexpr int NODE_PER_SECTOR = SECTOR_SIZE / (2 * 4);
constexpr int NODE_PER_SECTOR = 64;
constexpr int NODE_PER_SECTOR_SHIFT = 6;

// Per bucket, we keep track of the absence/presence of sectors
// only, to reduce memory usage.
// #define BUCKET_BITMAP_SIZE  NODE_PER_BUCKET / (8 * NODE_PER_SECTOR)
constexpr int BUCKET_BITMAP_SIZE = 128;

// #define BUCKET_SECTOR_SIZE_ARRAY_SIZE  NODE_PER_BUCKET / NODE_PER_SECTOR
// Per bucket, we keep track of the real size of the sector. Each sector
// size is encoded in a single byte, whose value is:
// (sector_size in bytes - 8 ) / 2, minus 8. 252 means uncompressed
constexpr int BUCKET_SECTOR_SIZE_ARRAY_SIZE = 1024;

// Must be a multiple of both BUCKET_BITMAP_SIZE and
// BUCKET_SECTOR_SIZE_ARRAY_SIZE
constexpr int knPAGE_SIZE = 4096;

// compressSize should not be greater than 512, so COMPRESS_SIZE_TO_BYTE() fits
// on a byte.
static GByte COMPRESS_SIZE_TO_BYTE(size_t nCompressSize)
{
    return static_cast<GByte>((nCompressSize - 8) / 2);
}

template <typename T> static T ROUND_COMPRESS_SIZE(T nCompressSize)
{
    return ((nCompressSize + 1) / 2) * 2;
}

static int COMPRESS_SIZE_FROM_BYTE(GByte byte_on_size)
{
    return static_cast<int>(byte_on_size) * 2 + 8;
}

// Max number of features that are accumulated in pasWayFeaturePairs.
constexpr int MAX_DELAYED_FEATURES = 75000;
// Max number of tags that are accumulated in pasAccumulatedTags.
constexpr int MAX_ACCUMULATED_TAGS = MAX_DELAYED_FEATURES * 5;
// Max size of the string with tag values that are accumulated in
// pabyNonRedundantValues.
constexpr int MAX_NON_REDUNDANT_VALUES = MAX_DELAYED_FEATURES * 10;
// Max size of the string with tag values that are accumulated in
// pabyNonRedundantKeys.
constexpr int MAX_NON_REDUNDANT_KEYS = MAX_DELAYED_FEATURES * 10;
// Max number of features that are accumulated in panUnsortedReqIds
constexpr int MAX_ACCUMULATED_NODES = 1000000;

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
// Size of panHashedIndexes array. Must be in the list at
// http://planetmath.org/goodhashtableprimes , and greater than
// MAX_ACCUMULATED_NODES.
constexpr int HASHED_INDEXES_ARRAY_SIZE = 3145739;
// #define HASHED_INDEXES_ARRAY_SIZE   1572869
constexpr int COLLISION_BUCKET_ARRAY_SIZE = (MAX_ACCUMULATED_NODES / 100) * 40;

// hash function = identity
#define HASH_ID_FUNC(x) (static_cast<std::uintptr_t>(x))
#endif  // ENABLE_NODE_LOOKUP_BY_HASHING

// #define FAKE_LOOKUP_NODES

// #define DEBUG_MEM_USAGE
#ifdef DEBUG_MEM_USAGE
size_t GetMaxTotalAllocs();
#endif

static void WriteVarSInt64(GIntBig nSVal, GByte **ppabyData);

class DSToBeOpened
{
  public:
    GIntBig nPID{};
    CPLString osDSName{};
    CPLString osInterestLayers{};
};

static CPLMutex *hMutex = nullptr;
static std::vector<DSToBeOpened> oListDSToBeOpened;

/************************************************************************/
/*                    AddInterestLayersForDSName()                      */
/************************************************************************/

static void AddInterestLayersForDSName(const CPLString &osDSName,
                                       const CPLString &osInterestLayers)
{
    CPLMutexHolder oMutexHolder(&hMutex);
    DSToBeOpened oDSToBeOpened;
    oDSToBeOpened.nPID = CPLGetPID();
    oDSToBeOpened.osDSName = osDSName;
    oDSToBeOpened.osInterestLayers = osInterestLayers;
    oListDSToBeOpened.push_back(oDSToBeOpened);
}

/************************************************************************/
/*                    GetInterestLayersForDSName()                      */
/************************************************************************/

static CPLString GetInterestLayersForDSName(const CPLString &osDSName)
{
    CPLMutexHolder oMutexHolder(&hMutex);
    GIntBig nPID = CPLGetPID();
    for (auto oIter = oListDSToBeOpened.begin();
         oIter < oListDSToBeOpened.end(); ++oIter)
    {
        const auto &ds = *oIter;
        if (ds.nPID == nPID && ds.osDSName == osDSName)
        {
            CPLString osInterestLayers = ds.osInterestLayers;
            oListDSToBeOpened.erase(oIter);
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
    m_apsKeys.push_back(nullptr);  // guard to avoid index 0 to be used

    MAX_INDEXED_KEYS = static_cast<unsigned>(
        atoi(CPLGetConfigOption("OSM_MAX_INDEXED_KEYS", "32768")));
    MAX_INDEXED_VALUES_PER_KEY = static_cast<unsigned>(
        atoi(CPLGetConfigOption("OSM_MAX_INDEXED_VALUES_PER_KEY", "1024")));
}

/************************************************************************/
/*                          ~OGROSMDataSource()                         */
/************************************************************************/

OGROSMDataSource::~OGROSMDataSource()

{
    m_apoLayers.clear();

    if (m_psParser != nullptr)
        CPLDebug("OSM", "Number of bytes read in file : " CPL_FRMT_GUIB,
                 OSM_GetBytesRead(m_psParser));
    OSM_Close(m_psParser);

    if (m_hDB != nullptr)
        CloseDB();

    if (m_hDBForComputedAttributes != nullptr)
        sqlite3_close(m_hDBForComputedAttributes);

    if (m_pMyVFS)
    {
        sqlite3_vfs_unregister(m_pMyVFS);
        CPLFree(m_pMyVFS->pAppData);
        CPLFree(m_pMyVFS);
    }

    if (!m_osTmpDBName.empty() && m_bMustUnlink)
    {
        const char *pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
        if (!EQUAL(pszVal, "NOT_EVEN_AT_END"))
            VSIUnlink(m_osTmpDBName);
    }

    CPLFree(m_panReqIds);
#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    CPLFree(m_panHashedIndexes);
    CPLFree(m_psCollisionBuckets);
#endif
    CPLFree(m_pasLonLatArray);
    CPLFree(m_panUnsortedReqIds);

    CPLFree(m_pasAccumulatedTags);
    CPLFree(pabyNonRedundantKeys);
    CPLFree(pabyNonRedundantValues);

#ifdef OSM_DEBUG
    FILE *f = fopen("keys.txt", "wt");
    for (int i = 1; i < startic_cast<int>(asKeys.size()); i++)
    {
        KeyDesc *psKD = asKeys[i];
        if (psKD)
        {
            fprintf(f, "%08d idx=%d %s\n", psKD->nOccurrences, psKD->nKeyIndex,
                    psKD->pszK);
        }
    }
    fclose(f);
#endif

    for (int i = 1; i < static_cast<int>(m_apsKeys.size()); i++)
    {
        KeyDesc *psKD = m_apsKeys[i];
        if (psKD)
        {
            CPLFree(psKD->pszK);
            for (int j = 0; j < static_cast<int>(psKD->apszValues.size()); j++)
                CPLFree(psKD->apszValues[j]);
            delete psKD;
        }
    }

    if (m_fpNodes)
        VSIFCloseL(m_fpNodes);
    if (!m_osNodesFilename.empty() && m_bMustUnlinkNodesFile)
    {
        const char *pszVal = CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
        if (!EQUAL(pszVal, "NOT_EVEN_AT_END"))
            VSIUnlink(m_osNodesFilename);
    }

    CPLFree(m_pabySector);
    for (auto &oIter : m_oMapBuckets)
    {
        if (m_bCompressNodes)
        {
            int nRem =
                oIter.first % (knPAGE_SIZE / BUCKET_SECTOR_SIZE_ARRAY_SIZE);
            if (nRem == 0)
                CPLFree(oIter.second.u.panSectorSize);
        }
        else
        {
            int nRem = oIter.first % (knPAGE_SIZE / BUCKET_BITMAP_SIZE);
            if (nRem == 0)
                CPLFree(oIter.second.u.pabyBitmap);
        }
    }
}

/************************************************************************/
/*                             CloseDB()                               */
/************************************************************************/

void OGROSMDataSource::CloseDB()
{
    if (m_hInsertNodeStmt != nullptr)
        sqlite3_finalize(m_hInsertNodeStmt);
    m_hInsertNodeStmt = nullptr;

    if (m_hInsertWayStmt != nullptr)
        sqlite3_finalize(m_hInsertWayStmt);
    m_hInsertWayStmt = nullptr;

    if (m_hInsertPolygonsStandaloneStmt != nullptr)
        sqlite3_finalize(m_hInsertPolygonsStandaloneStmt);
    m_hInsertPolygonsStandaloneStmt = nullptr;

    if (m_hDeletePolygonsStandaloneStmt != nullptr)
        sqlite3_finalize(m_hDeletePolygonsStandaloneStmt);
    m_hDeletePolygonsStandaloneStmt = nullptr;

    if (m_hSelectPolygonsStandaloneStmt != nullptr)
        sqlite3_finalize(m_hSelectPolygonsStandaloneStmt);
    m_hSelectPolygonsStandaloneStmt = nullptr;

    if (m_pahSelectNodeStmt != nullptr)
    {
        for (int i = 0; i < LIMIT_IDS_PER_REQUEST; i++)
        {
            if (m_pahSelectNodeStmt[i] != nullptr)
                sqlite3_finalize(m_pahSelectNodeStmt[i]);
        }
        CPLFree(m_pahSelectNodeStmt);
        m_pahSelectNodeStmt = nullptr;
    }

    if (m_pahSelectWayStmt != nullptr)
    {
        for (int i = 0; i < LIMIT_IDS_PER_REQUEST; i++)
        {
            if (m_pahSelectWayStmt[i] != nullptr)
                sqlite3_finalize(m_pahSelectWayStmt[i]);
        }
        CPLFree(m_pahSelectWayStmt);
        m_pahSelectWayStmt = nullptr;
    }

    if (m_bInTransaction)
        CommitTransactionCacheDB();

    sqlite3_close(m_hDB);
    m_hDB = nullptr;
}

/************************************************************************/
/*                             IndexPoint()                             */
/************************************************************************/

constexpr GByte abyBitsCount[] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4,
    2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5,
    3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

bool OGROSMDataSource::IndexPoint(const OSMNode *psNode)
{
    if (!m_bIndexPoints)
        return true;

    if (m_bCustomIndexing)
        return IndexPointCustom(psNode);

    return IndexPointSQLite(psNode);
}

/************************************************************************/
/*                          IndexPointSQLite()                          */
/************************************************************************/

bool OGROSMDataSource::IndexPointSQLite(const OSMNode *psNode)
{
    sqlite3_bind_int64(m_hInsertNodeStmt, 1, psNode->nID);

    LonLat sLonLat;
    sLonLat.nLon = DBL_TO_INT(psNode->dfLon);
    sLonLat.nLat = DBL_TO_INT(psNode->dfLat);

    sqlite3_bind_blob(m_hInsertNodeStmt, 2, &sLonLat, sizeof(sLonLat),
                      SQLITE_STATIC);

    const int rc = sqlite3_step(m_hInsertNodeStmt);
    sqlite3_reset(m_hInsertNodeStmt);
    if (!(rc == SQLITE_OK || rc == SQLITE_DONE))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed inserting node " CPL_FRMT_GIB ": %s", psNode->nID,
                 sqlite3_errmsg(m_hDB));
        return false;
    }

    return true;
}

/************************************************************************/
/*                           FlushCurrentSector()                       */
/************************************************************************/

bool OGROSMDataSource::FlushCurrentSector()
{
#ifndef FAKE_LOOKUP_NODES
    if (m_bCompressNodes)
        return FlushCurrentSectorCompressedCase();

    return FlushCurrentSectorNonCompressedCase();
#else
    return true;
#endif
}

/************************************************************************/
/*                            AllocBucket()                             */
/************************************************************************/

Bucket *OGROSMDataSource::AllocBucket(int iBucket)
{
    if (m_bCompressNodes)
    {
        const int nRem =
            iBucket % (knPAGE_SIZE / BUCKET_SECTOR_SIZE_ARRAY_SIZE);
        Bucket *psPrevBucket = GetBucket(iBucket - nRem);
        if (psPrevBucket->u.panSectorSize == nullptr)
            psPrevBucket->u.panSectorSize =
                static_cast<GByte *>(VSI_CALLOC_VERBOSE(1, knPAGE_SIZE));
        GByte *panSectorSize = psPrevBucket->u.panSectorSize;
        Bucket *psBucket = GetBucket(iBucket);
        if (panSectorSize != nullptr)
        {
            psBucket->u.panSectorSize =
                panSectorSize + nRem * BUCKET_SECTOR_SIZE_ARRAY_SIZE;
            return psBucket;
        }
        psBucket->u.panSectorSize = nullptr;
    }
    else
    {
        const int nRem = iBucket % (knPAGE_SIZE / BUCKET_BITMAP_SIZE);
        Bucket *psPrevBucket = GetBucket(iBucket - nRem);
        if (psPrevBucket->u.pabyBitmap == nullptr)
            psPrevBucket->u.pabyBitmap =
                reinterpret_cast<GByte *>(VSI_CALLOC_VERBOSE(1, knPAGE_SIZE));
        GByte *pabyBitmap = psPrevBucket->u.pabyBitmap;
        Bucket *psBucket = GetBucket(iBucket);
        if (pabyBitmap != nullptr)
        {
            psBucket->u.pabyBitmap = pabyBitmap + nRem * BUCKET_BITMAP_SIZE;
            return psBucket;
        }
        psBucket->u.pabyBitmap = nullptr;
    }

    // Out of memory.
    CPLError(CE_Failure, CPLE_AppDefined,
             "AllocBucket() failed. Use OSM_USE_CUSTOM_INDEXING=NO");
    m_bStopParsing = true;
    return nullptr;
}

/************************************************************************/
/*                             GetBucket()                              */
/************************************************************************/

Bucket *OGROSMDataSource::GetBucket(int nBucketId)
{
    auto oIter = m_oMapBuckets.find(nBucketId);
    if (oIter == m_oMapBuckets.end())
    {
        Bucket *psBucket = &m_oMapBuckets[nBucketId];
        psBucket->nOff = -1;
        if (m_bCompressNodes)
            psBucket->u.panSectorSize = nullptr;
        else
            psBucket->u.pabyBitmap = nullptr;
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
    GByte *pabyOut = abyOutBuffer;
    LonLat *pasLonLatIn = reinterpret_cast<LonLat *>(m_pabySector);
    int nLastLon = 0;
    int nLastLat = 0;
    bool bLastValid = false;

    CPLAssert((NODE_PER_SECTOR % 8) == 0);
    memset(abyOutBuffer, 0, NODE_PER_SECTOR / 8);
    pabyOut += NODE_PER_SECTOR / 8;
    for (int i = 0; i < NODE_PER_SECTOR; i++)
    {
        if (pasLonLatIn[i].nLon || pasLonLatIn[i].nLat)
        {
            abyOutBuffer[i >> 3] |= (1 << (i % 8));
            if (bLastValid)
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
    GByte *pabyToWrite = nullptr;
    if (nCompressSize >= static_cast<size_t>(SECTOR_SIZE))
    {
        nCompressSize = SECTOR_SIZE;
        pabyToWrite = m_pabySector;
    }
    else
        pabyToWrite = abyOutBuffer;

    if (VSIFWriteL(pabyToWrite, 1, nCompressSize, m_fpNodes) == nCompressSize)
    {
        memset(m_pabySector, 0, SECTOR_SIZE);
        m_nNodesFileSize += nCompressSize;

        Bucket *psBucket = GetBucket(m_nBucketOld);
        if (psBucket->u.panSectorSize == nullptr)
        {
            psBucket = AllocBucket(m_nBucketOld);
            if (psBucket == nullptr)
                return false;
        }
        CPLAssert(psBucket->u.panSectorSize != nullptr);
        psBucket->u.panSectorSize[m_nOffInBucketReducedOld] =
            COMPRESS_SIZE_TO_BYTE(nCompressSize);

        return true;
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "Cannot write in temporary node file %s : %s",
             m_osNodesFilename.c_str(), VSIStrerror(errno));

    return false;
}

/************************************************************************/
/*                   FlushCurrentSectorNonCompressedCase()              */
/************************************************************************/

bool OGROSMDataSource::FlushCurrentSectorNonCompressedCase()
{
    if (VSIFWriteL(m_pabySector, 1, static_cast<size_t>(SECTOR_SIZE),
                   m_fpNodes) == static_cast<size_t>(SECTOR_SIZE))
    {
        memset(m_pabySector, 0, SECTOR_SIZE);
        m_nNodesFileSize += SECTOR_SIZE;
        return true;
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "Cannot write in temporary node file %s : %s",
             m_osNodesFilename.c_str(), VSIStrerror(errno));

    return false;
}

/************************************************************************/
/*                          IndexPointCustom()                          */
/************************************************************************/

bool OGROSMDataSource::IndexPointCustom(const OSMNode *psNode)
{
    if (psNode->nID <= m_nPrevNodeId)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Non increasing node id. Use OSM_USE_CUSTOM_INDEXING=NO");
        m_bStopParsing = true;
        return false;
    }
    if (!VALID_ID_FOR_CUSTOM_INDEXING(psNode->nID))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported node id value (" CPL_FRMT_GIB
                 "). Use OSM_USE_CUSTOM_INDEXING=NO",
                 psNode->nID);
        m_bStopParsing = true;
        return false;
    }

    const int nBucket = static_cast<int>(psNode->nID / NODE_PER_BUCKET);
    const int nOffInBucket = static_cast<int>(psNode->nID % NODE_PER_BUCKET);
    const int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
    const int nOffInBucketReducedRemainder =
        nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

    Bucket *psBucket = GetBucket(nBucket);

    if (!m_bCompressNodes)
    {
        const int nBitmapIndex = nOffInBucketReduced / 8;
        const int nBitmapRemainder = nOffInBucketReduced % 8;
        if (psBucket->u.pabyBitmap == nullptr)
        {
            psBucket = AllocBucket(nBucket);
            if (psBucket == nullptr)
                return false;
        }
        CPLAssert(psBucket->u.pabyBitmap != nullptr);
        psBucket->u.pabyBitmap[nBitmapIndex] |= (1 << nBitmapRemainder);
    }

    if (nBucket != m_nBucketOld)
    {
        CPLAssert(nBucket > m_nBucketOld);
        if (m_nBucketOld >= 0)
        {
            if (!FlushCurrentSector())
            {
                m_bStopParsing = true;
                return false;
            }
        }
        m_nBucketOld = nBucket;
        m_nOffInBucketReducedOld = nOffInBucketReduced;
        CPLAssert(psBucket->nOff == -1);
        psBucket->nOff = VSIFTellL(m_fpNodes);
    }
    else if (nOffInBucketReduced != m_nOffInBucketReducedOld)
    {
        CPLAssert(nOffInBucketReduced > m_nOffInBucketReducedOld);
        if (!FlushCurrentSector())
        {
            m_bStopParsing = true;
            return false;
        }
        m_nOffInBucketReducedOld = nOffInBucketReduced;
    }

    LonLat *psLonLat = reinterpret_cast<LonLat *>(
        m_pabySector + sizeof(LonLat) * nOffInBucketReducedRemainder);
    psLonLat->nLon = DBL_TO_INT(psNode->dfLon);
    psLonLat->nLat = DBL_TO_INT(psNode->dfLat);

    m_nPrevNodeId = psNode->nID;

    return true;
}

/************************************************************************/
/*                             NotifyNodes()                            */
/************************************************************************/

void OGROSMDataSource::NotifyNodes(unsigned int nNodes, const OSMNode *pasNodes)
{
    const OGREnvelope *psEnvelope =
        m_apoLayers[IDX_LYR_POINTS]->GetSpatialFilterEnvelope();

    for (unsigned int i = 0; i < nNodes; i++)
    {
        /* If the point doesn't fit into the envelope of the spatial filter */
        /* then skip it */
        if (psEnvelope != nullptr && !(pasNodes[i].dfLon >= psEnvelope->MinX &&
                                       pasNodes[i].dfLon <= psEnvelope->MaxX &&
                                       pasNodes[i].dfLat >= psEnvelope->MinY &&
                                       pasNodes[i].dfLat <= psEnvelope->MaxY))
            continue;

        if (!IndexPoint(&pasNodes[i]))
            break;

        if (!m_apoLayers[IDX_LYR_POINTS]->IsUserInterested())
            continue;

        bool bInterestingTag = m_bReportAllNodes;
        const OSMTag *pasTags = pasNodes[i].pasTags;

        if (!m_bReportAllNodes)
        {
            for (unsigned int j = 0; j < pasNodes[i].nTags; j++)
            {
                const char *pszK = pasTags[j].pszK;
                if (m_apoLayers[IDX_LYR_POINTS]->IsSignificantKey(pszK))
                {
                    bInterestingTag = true;
                    break;
                }
            }
        }

        if (bInterestingTag)
        {
            auto poFeature = std::make_unique<OGRFeature>(
                m_apoLayers[IDX_LYR_POINTS]->GetLayerDefn());

            poFeature->SetGeometryDirectly(
                new OGRPoint(pasNodes[i].dfLon, pasNodes[i].dfLat));

            m_apoLayers[IDX_LYR_POINTS]->SetFieldsFromTags(
                poFeature.get(), pasNodes[i].nID, false, pasNodes[i].nTags,
                pasTags, &pasNodes[i].sInfo);

            bool bFilteredOut = false;
            if (!m_apoLayers[IDX_LYR_POINTS]->AddFeature(std::move(poFeature),
                                                         false, &bFilteredOut,
                                                         !m_bFeatureAdded))
            {
                m_bStopParsing = true;
                break;
            }
            else if (!bFilteredOut)
                m_bFeatureAdded = true;
        }
    }
}

static void OGROSMNotifyNodes(unsigned int nNodes, OSMNode *pasNodes,
                              OSMContext * /* psOSMContext */, void *user_data)
{
    static_cast<OGROSMDataSource *>(user_data)->NotifyNodes(nNodes, pasNodes);
}

/************************************************************************/
/*                            LookupNodes()                             */
/************************************************************************/

// #define DEBUG_COLLISIONS 1

void OGROSMDataSource::LookupNodes()
{
    if (m_bCustomIndexing)
        LookupNodesCustom();
    else
        LookupNodesSQLite();

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    if (m_nReqIds > 1 && m_bEnableHashedIndex)
    {
        memset(m_panHashedIndexes, 0xFF,
               HASHED_INDEXES_ARRAY_SIZE * sizeof(int));
        m_bHashedIndexValid = true;
#ifdef DEBUG_COLLISIONS
        int nCollisions = 0;
#endif
        int iNextFreeBucket = 0;
        for (unsigned int i = 0; i < m_nReqIds; i++)
        {
            int nIndInHashArray = static_cast<int>(
                HASH_ID_FUNC(m_panReqIds[i]) % HASHED_INDEXES_ARRAY_SIZE);
            int nIdx = m_panHashedIndexes[nIndInHashArray];
            if (nIdx == -1)
            {
                m_panHashedIndexes[nIndInHashArray] = i;
            }
            else
            {
#ifdef DEBUG_COLLISIONS
                nCollisions++;
#endif
                int iBucket = 0;
                if (nIdx >= 0)
                {
                    if (iNextFreeBucket == COLLISION_BUCKET_ARRAY_SIZE)
                    {
                        CPLDebug(
                            "OSM",
                            "Too many collisions. Disabling hashed indexing");
                        m_bHashedIndexValid = false;
                        m_bEnableHashedIndex = false;
                        break;
                    }
                    iBucket = iNextFreeBucket;
                    m_psCollisionBuckets[iNextFreeBucket].nInd = nIdx;
                    m_psCollisionBuckets[iNextFreeBucket].nNext = -1;
                    m_panHashedIndexes[nIndInHashArray] = -iNextFreeBucket - 2;
                    iNextFreeBucket++;
                }
                else
                {
                    iBucket = -nIdx - 2;
                }
                if (iNextFreeBucket == COLLISION_BUCKET_ARRAY_SIZE)
                {
                    CPLDebug("OSM",
                             "Too many collisions. Disabling hashed indexing");
                    m_bHashedIndexValid = false;
                    m_bEnableHashedIndex = false;
                    break;
                }
                while (true)
                {
                    int iNext = m_psCollisionBuckets[iBucket].nNext;
                    if (iNext < 0)
                    {
                        m_psCollisionBuckets[iBucket].nNext = iNextFreeBucket;
                        m_psCollisionBuckets[iNextFreeBucket].nInd = i;
                        m_psCollisionBuckets[iNextFreeBucket].nNext = -1;
                        iNextFreeBucket++;
                        break;
                    }
                    iBucket = iNext;
                }
            }
        }
#ifdef DEBUG_COLLISIONS
        /* Collision rate in practice is around 12% on France, Germany, ... */
        /* Maximum seen ~ 15.9% on a planet file but often much smaller. */
        CPLDebug("OSM",
                 "nCollisions = %d/%d (%.1f %%), iNextFreeBucket = %d/%d",
                 nCollisions, nReqIds, nCollisions * 100.0 / nReqIds,
                 iNextFreeBucket, COLLISION_BUCKET_ARRAY_SIZE);
#endif
    }
    else
        m_bHashedIndexValid = false;
#endif  // ENABLE_NODE_LOOKUP_BY_HASHING
}

/************************************************************************/
/*                           LookupNodesSQLite()                        */
/************************************************************************/

void OGROSMDataSource::LookupNodesSQLite()
{
    CPLAssert(m_nUnsortedReqIds <=
              static_cast<unsigned int>(MAX_ACCUMULATED_NODES));

    m_nReqIds = 0;
    for (unsigned int i = 0; i < m_nUnsortedReqIds; i++)
    {
        GIntBig id = m_panUnsortedReqIds[i];
        m_panReqIds[m_nReqIds++] = id;
    }

    std::sort(m_panReqIds, m_panReqIds + m_nReqIds);

    /* Remove duplicates */
    unsigned int j = 0;
    for (unsigned int i = 0; i < m_nReqIds; i++)
    {
        if (!(i > 0 && m_panReqIds[i] == m_panReqIds[i - 1]))
            m_panReqIds[j++] = m_panReqIds[i];
    }
    m_nReqIds = j;

    unsigned int iCur = 0;
    j = 0;
    while (iCur < m_nReqIds)
    {
        unsigned int nToQuery = m_nReqIds - iCur;
        if (nToQuery > static_cast<unsigned int>(LIMIT_IDS_PER_REQUEST))
            nToQuery = static_cast<unsigned int>(LIMIT_IDS_PER_REQUEST);

        assert(nToQuery > 0);
        sqlite3_stmt *hStmt = m_pahSelectNodeStmt[nToQuery - 1];
        for (unsigned int i = iCur; i < iCur + nToQuery; i++)
        {
            sqlite3_bind_int64(hStmt, i - iCur + 1, m_panReqIds[i]);
        }
        iCur += nToQuery;

        while (sqlite3_step(hStmt) == SQLITE_ROW)
        {
            const GIntBig id = sqlite3_column_int64(hStmt, 0);
            const LonLat *psLonLat =
                reinterpret_cast<const LonLat *>(sqlite3_column_blob(hStmt, 1));

            m_panReqIds[j] = id;
            m_pasLonLatArray[j].nLon = psLonLat->nLon;
            m_pasLonLatArray[j].nLat = psLonLat->nLat;
            j++;
        }

        sqlite3_reset(hStmt);
    }
    m_nReqIds = j;
}

/************************************************************************/
/*                           DecompressSector()                         */
/************************************************************************/

static bool DecompressSector(const GByte *pabyIn, int nSectorSize,
                             GByte *pabyOut)
{
    const GByte *pabyPtr = pabyIn;
    LonLat *pasLonLatOut = reinterpret_cast<LonLat *>(pabyOut);
    int nLastLon = 0;
    int nLastLat = 0;
    bool bLastValid = false;

    pabyPtr += NODE_PER_SECTOR / 8;
    for (int i = 0; i < NODE_PER_SECTOR; i++)
    {
        if (pabyIn[i >> 3] & (1 << (i % 8)))
        {
            if (bLastValid)
            {
                pasLonLatOut[i].nLon =
                    static_cast<int>(nLastLon + ReadVarSInt64(&pabyPtr));
                pasLonLatOut[i].nLat =
                    static_cast<int>(nLastLat + ReadVarSInt64(&pabyPtr));
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

    int nRead = static_cast<int>(pabyPtr - pabyIn);
    nRead = ROUND_COMPRESS_SIZE(nRead);
    return nRead == nSectorSize;
}

/************************************************************************/
/*                           LookupNodesCustom()                        */
/************************************************************************/

void OGROSMDataSource::LookupNodesCustom()
{
    m_nReqIds = 0;

    if (m_nBucketOld >= 0)
    {
        if (!FlushCurrentSector())
        {
            m_bStopParsing = true;
            return;
        }

        m_nBucketOld = -1;
    }

    CPLAssert(m_nUnsortedReqIds <=
              static_cast<unsigned int>(MAX_ACCUMULATED_NODES));

    for (unsigned int i = 0; i < m_nUnsortedReqIds; i++)
    {
        GIntBig id = m_panUnsortedReqIds[i];

        if (!VALID_ID_FOR_CUSTOM_INDEXING(id))
            continue;

        int nBucket = static_cast<int>(id / NODE_PER_BUCKET);
        int nOffInBucket = static_cast<int>(id % NODE_PER_BUCKET);
        int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;

        const auto oIter = m_oMapBuckets.find(nBucket);
        if (oIter == m_oMapBuckets.end())
            continue;
        const Bucket *psBucket = &(oIter->second);

        if (m_bCompressNodes)
        {
            if (psBucket->u.panSectorSize == nullptr ||
                !(psBucket->u.panSectorSize[nOffInBucketReduced]))
                continue;
        }
        else
        {
            int nBitmapIndex = nOffInBucketReduced / 8;
            int nBitmapRemainder = nOffInBucketReduced % 8;
            if (psBucket->u.pabyBitmap == nullptr ||
                !(psBucket->u.pabyBitmap[nBitmapIndex] &
                  (1 << nBitmapRemainder)))
                continue;
        }

        m_panReqIds[m_nReqIds++] = id;
    }

    std::sort(m_panReqIds, m_panReqIds + m_nReqIds);

    /* Remove duplicates */
    unsigned int j = 0;  // Used after for.
    for (unsigned int i = 0; i < m_nReqIds; i++)
    {
        if (!(i > 0 && m_panReqIds[i] == m_panReqIds[i - 1]))
            m_panReqIds[j++] = m_panReqIds[i];
    }
    m_nReqIds = j;

#ifdef FAKE_LOOKUP_NODES
    for (unsigned int i = 0; i < nReqIds; i++)
    {
        pasLonLatArray[i].nLon = 0;
        pasLonLatArray[i].nLat = 0;
    }
#else
    if (m_bCompressNodes)
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
    constexpr int SECURITY_MARGIN = 8 + 8 + 2 * NODE_PER_SECTOR;
    GByte abyRawSector[SECTOR_SIZE + SECURITY_MARGIN];
    memset(abyRawSector + SECTOR_SIZE, 0, SECURITY_MARGIN);

    int l_nBucketOld = -1;
    int l_nOffInBucketReducedOld = -1;
    int k = 0;
    int nOffFromBucketStart = 0;

    unsigned int j = 0;  // Used after for.
    for (unsigned int i = 0; i < m_nReqIds; i++)
    {
        const GIntBig id = m_panReqIds[i];
        const int nBucket = static_cast<int>(id / NODE_PER_BUCKET);
        const int nOffInBucket = static_cast<int>(id % NODE_PER_BUCKET);
        const int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
        const int nOffInBucketReducedRemainder =
            nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

        if (nBucket != l_nBucketOld)
        {
            l_nOffInBucketReducedOld = -1;
            k = 0;
            nOffFromBucketStart = 0;
        }

        if (nOffInBucketReduced != l_nOffInBucketReducedOld)
        {
            const auto oIter = m_oMapBuckets.find(nBucket);
            if (oIter == m_oMapBuckets.end())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            const Bucket *psBucket = &(oIter->second);
            if (psBucket->u.panSectorSize == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            const int nSectorSize = COMPRESS_SIZE_FROM_BYTE(
                psBucket->u.panSectorSize[nOffInBucketReduced]);

            /* If we stay in the same bucket, we can reuse the previously */
            /* computed offset, instead of starting from bucket start */
            for (; k < nOffInBucketReduced; k++)
            {
                if (psBucket->u.panSectorSize[k])
                    nOffFromBucketStart +=
                        COMPRESS_SIZE_FROM_BYTE(psBucket->u.panSectorSize[k]);
            }

            VSIFSeekL(m_fpNodes, psBucket->nOff + nOffFromBucketStart,
                      SEEK_SET);
            if (nSectorSize == SECTOR_SIZE)
            {
                if (VSIFReadL(m_pabySector, 1, static_cast<size_t>(SECTOR_SIZE),
                              m_fpNodes) != static_cast<size_t>(SECTOR_SIZE))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot read node " CPL_FRMT_GIB, id);
                    continue;
                    // FIXME ?
                }
            }
            else
            {
                if (static_cast<int>(VSIFReadL(abyRawSector, 1, nSectorSize,
                                               m_fpNodes)) != nSectorSize)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot read sector for node " CPL_FRMT_GIB, id);
                    continue;
                    // FIXME ?
                }
                abyRawSector[nSectorSize] = 0;

                if (!DecompressSector(abyRawSector, nSectorSize, m_pabySector))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error while uncompressing sector for "
                             "node " CPL_FRMT_GIB,
                             id);
                    continue;
                    // FIXME ?
                }
            }

            l_nBucketOld = nBucket;
            l_nOffInBucketReducedOld = nOffInBucketReduced;
        }

        m_panReqIds[j] = id;
        memcpy(m_pasLonLatArray + j,
               m_pabySector + nOffInBucketReducedRemainder * sizeof(LonLat),
               sizeof(LonLat));

        if (m_pasLonLatArray[j].nLon || m_pasLonLatArray[j].nLat)
            j++;
    }
    m_nReqIds = j;
}

/************************************************************************/
/*                    LookupNodesCustomNonCompressedCase()              */
/************************************************************************/

void OGROSMDataSource::LookupNodesCustomNonCompressedCase()
{
    unsigned int j = 0;  // Used after for.

    int l_nBucketOld = -1;
    const Bucket *psBucket = nullptr;
    // To be glibc friendly, we will do reads aligned on 4096 byte offsets
    const int knDISK_SECTOR_SIZE = 4096;
    CPL_STATIC_ASSERT((knDISK_SECTOR_SIZE % SECTOR_SIZE) == 0);
    GByte abyDiskSector[knDISK_SECTOR_SIZE];
    // Offset in the nodes files for which abyDiskSector was read
    GIntBig nOldOffset = -knDISK_SECTOR_SIZE - 1;
    // Number of valid bytes in abyDiskSector
    size_t nValidBytes = 0;
    int k = 0;
    int nSectorBase = 0;
    for (unsigned int i = 0; i < m_nReqIds; i++)
    {
        const GIntBig id = m_panReqIds[i];
        const int nBucket = static_cast<int>(id / NODE_PER_BUCKET);
        const int nOffInBucket = static_cast<int>(id % NODE_PER_BUCKET);
        const int nOffInBucketReduced = nOffInBucket >> NODE_PER_SECTOR_SHIFT;
        const int nOffInBucketReducedRemainder =
            nOffInBucket & ((1 << NODE_PER_SECTOR_SHIFT) - 1);

        const int nBitmapIndex = nOffInBucketReduced / 8;
        const int nBitmapRemainder = nOffInBucketReduced % 8;

        if (psBucket == nullptr || nBucket != l_nBucketOld)
        {
            const auto oIter = m_oMapBuckets.find(nBucket);
            if (oIter == m_oMapBuckets.end())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            psBucket = &(oIter->second);
            if (psBucket->u.pabyBitmap == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot read node " CPL_FRMT_GIB, id);
                continue;
                // FIXME ?
            }
            l_nBucketOld = nBucket;
            nOldOffset = -knDISK_SECTOR_SIZE - 1;
            k = 0;
            nSectorBase = 0;
        }

        /* If we stay in the same bucket, we can reuse the previously */
        /* computed offset, instead of starting from bucket start */
        for (; k < nBitmapIndex; k++)
        {
            assert(psBucket->u.pabyBitmap);
            // psBucket->u.pabyBitmap cannot be NULL
            // coverity[var_deref_op]
            nSectorBase += abyBitsCount[psBucket->u.pabyBitmap[k]];
        }
        int nSector = nSectorBase;
        if (nBitmapRemainder)
        {
            assert(psBucket->u.pabyBitmap);
            nSector += abyBitsCount[psBucket->u.pabyBitmap[nBitmapIndex] &
                                    ((1 << nBitmapRemainder) - 1)];
        }

        const GIntBig nNewOffset = psBucket->nOff + nSector * SECTOR_SIZE;
        if (nNewOffset - nOldOffset >= knDISK_SECTOR_SIZE)
        {
            // Align on 4096 boundary to be glibc caching friendly
            const GIntBig nAlignedNewPos =
                nNewOffset & ~(static_cast<GIntBig>(knDISK_SECTOR_SIZE) - 1);
            VSIFSeekL(m_fpNodes, nAlignedNewPos, SEEK_SET);
            nValidBytes =
                VSIFReadL(abyDiskSector, 1, knDISK_SECTOR_SIZE, m_fpNodes);
            nOldOffset = nAlignedNewPos;
        }

        const size_t nOffsetInDiskSector =
            static_cast<size_t>(nNewOffset - nOldOffset) +
            nOffInBucketReducedRemainder * sizeof(LonLat);
        if (nValidBytes < sizeof(LonLat) ||
            nOffsetInDiskSector > nValidBytes - sizeof(LonLat))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read node " CPL_FRMT_GIB, id);
            continue;
        }
        memcpy(&m_pasLonLatArray[j], abyDiskSector + nOffsetInDiskSector,
               sizeof(LonLat));

        m_panReqIds[j] = id;
        if (m_pasLonLatArray[j].nLon || m_pasLonLatArray[j].nLat)
            j++;
    }
    m_nReqIds = j;
}

/************************************************************************/
/*                            WriteVarInt()                             */
/************************************************************************/

static void WriteVarInt(unsigned int nVal, std::vector<GByte> &abyData)
{
    while (true)
    {
        if ((nVal & (~0x7fU)) == 0)
        {
            abyData.push_back(static_cast<GByte>(nVal));
            return;
        }

        abyData.push_back(0x80 | static_cast<GByte>(nVal & 0x7f));
        nVal >>= 7;
    }
}

/************************************************************************/
/*                           WriteVarInt64()                            */
/************************************************************************/

static void WriteVarInt64(GUIntBig nVal, std::vector<GByte> &abyData)
{
    while (true)
    {
        if ((static_cast<uint32_t>(nVal) & (~0x7fU)) == 0)
        {
            abyData.push_back(static_cast<GByte>(nVal));
            return;
        }

        abyData.push_back(0x80 | static_cast<GByte>(nVal & 0x7f));
        nVal >>= 7;
    }
}

/************************************************************************/
/*                           WriteVarSInt64()                           */
/************************************************************************/

static void WriteVarSInt64(GIntBig nSVal, std::vector<GByte> &abyData)
{
    GIntBig nVal = nSVal >= 0 ? nSVal << 1 : ((-1 - nSVal) << 1) + 1;

    while (true)
    {
        if ((nVal & (~0x7f)) == 0)
        {
            abyData.push_back(static_cast<GByte>(nVal));
            return;
        }

        abyData.push_back(0x80 | static_cast<GByte>(nVal & 0x7f));
        nVal >>= 7;
    }
}

/************************************************************************/
/*                           WriteVarSInt64()                           */
/************************************************************************/

static void WriteVarSInt64(GIntBig nSVal, GByte **ppabyData)
{
    GIntBig nVal = nSVal >= 0 ? nSVal << 1 : ((-1 - nSVal) << 1) + 1;

    GByte *pabyData = *ppabyData;
    while (true)
    {
        if ((nVal & (~0x7f)) == 0)
        {
            *pabyData = static_cast<GByte>(nVal);
            *ppabyData = pabyData + 1;
            return;
        }

        *pabyData = 0x80 | static_cast<GByte>(nVal & 0x7f);
        nVal >>= 7;
        pabyData++;
    }
}

/************************************************************************/
/*                             CompressWay()                            */
/************************************************************************/

void OGROSMDataSource::CompressWay(bool bIsArea, unsigned int nTags,
                                   const IndexedKVP *pasTags, int nPoints,
                                   const LonLat *pasLonLatPairs,
                                   const OSMInfo *psInfo,
                                   std::vector<GByte> &abyCompressedWay)
{
    abyCompressedWay.clear();
    abyCompressedWay.push_back((bIsArea) ? 1 : 0);
    CPLAssert(nTags <= MAX_COUNT_FOR_TAGS_IN_WAY);
    abyCompressedWay.push_back(static_cast<GByte>(nTags));

    for (unsigned int iTag = 0; iTag < nTags; iTag++)
    {
        if (pasTags[iTag].bKIsIndex)
        {
            WriteVarInt(pasTags[iTag].uKey.nKeyIndex, abyCompressedWay);
        }
        else
        {
            const char *pszK =
                reinterpret_cast<const char *>(pabyNonRedundantKeys) +
                pasTags[iTag].uKey.nOffsetInpabyNonRedundantKeys;

            abyCompressedWay.push_back(0);

            abyCompressedWay.insert(
                abyCompressedWay.end(), reinterpret_cast<const GByte *>(pszK),
                reinterpret_cast<const GByte *>(pszK) + strlen(pszK) + 1);
        }

        if (pasTags[iTag].bVIsIndex)
        {
            WriteVarInt(pasTags[iTag].uVal.nValueIndex, abyCompressedWay);
        }
        else
        {
            const char *pszV =
                reinterpret_cast<const char *>(pabyNonRedundantValues) +
                pasTags[iTag].uVal.nOffsetInpabyNonRedundantValues;

            if (pasTags[iTag].bKIsIndex)
                abyCompressedWay.push_back(0);

            abyCompressedWay.insert(
                abyCompressedWay.end(), reinterpret_cast<const GByte *>(pszV),
                reinterpret_cast<const GByte *>(pszV) + strlen(pszV) + 1);
        }
    }

    if (m_bNeedsToSaveWayInfo)
    {
        if (psInfo != nullptr)
        {
            abyCompressedWay.push_back(1);
            WriteVarInt64(psInfo->ts.nTimeStamp, abyCompressedWay);
            WriteVarInt64(psInfo->nChangeset, abyCompressedWay);
            WriteVarInt(psInfo->nVersion, abyCompressedWay);
            WriteVarInt(psInfo->nUID, abyCompressedWay);
            // FIXME : do something with pszUserSID
        }
        else
        {
            abyCompressedWay.push_back(0);
        }
    }

    abyCompressedWay.insert(
        abyCompressedWay.end(),
        reinterpret_cast<const GByte *>(&(pasLonLatPairs[0])),
        reinterpret_cast<const GByte *>(&(pasLonLatPairs[0])) + sizeof(LonLat));
    for (int i = 1; i < nPoints; i++)
    {
        GIntBig nDiff64 = static_cast<GIntBig>(pasLonLatPairs[i].nLon) -
                          static_cast<GIntBig>(pasLonLatPairs[i - 1].nLon);
        WriteVarSInt64(nDiff64, abyCompressedWay);

        nDiff64 = pasLonLatPairs[i].nLat - pasLonLatPairs[i - 1].nLat;
        WriteVarSInt64(nDiff64, abyCompressedWay);
    }
}

/************************************************************************/
/*                             UncompressWay()                          */
/************************************************************************/

void OGROSMDataSource::UncompressWay(int nBytes, const GByte *pabyCompressedWay,
                                     bool *pbIsArea,
                                     std::vector<LonLat> &asCoords,
                                     unsigned int *pnTags, OSMTag *pasTags,
                                     OSMInfo *psInfo)
{
    asCoords.clear();
    const GByte *pabyPtr = pabyCompressedWay;
    if (pbIsArea)
        *pbIsArea = (*pabyPtr == 1) ? true : false;
    pabyPtr++;
    unsigned int nTags = *pabyPtr;
    pabyPtr++;

    if (pnTags)
        *pnTags = nTags;

    // TODO: Some additional safety checks.
    for (unsigned int iTag = 0; iTag < nTags; iTag++)
    {
        const int nK = ReadVarInt32(&pabyPtr);
        const GByte *pszK = nullptr;
        if (nK == 0)
        {
            pszK = pabyPtr;
            while (*pabyPtr != '\0')
                pabyPtr++;
            pabyPtr++;
        }

        const int nV = nK == 0 ? 0 : ReadVarInt32(&pabyPtr);
        const GByte *pszV = nullptr;
        if (nV == 0)
        {
            pszV = pabyPtr;
            while (*pabyPtr != '\0')
                pabyPtr++;
            pabyPtr++;
        }

        if (pasTags)
        {
            CPLAssert(nK >= 0 && static_cast<unsigned>(nK) < m_apsKeys.size());
            pasTags[iTag].pszK =
                nK ? m_apsKeys[nK]->pszK : reinterpret_cast<const char *>(pszK);

            CPLAssert(nK == 0 ||
                      (nV >= 0 && static_cast<unsigned>(nV) <
                                      m_apsKeys[nK]->apszValues.size()));
            pasTags[iTag].pszV = nV ? m_apsKeys[nK]->apszValues[nV]
                                    : reinterpret_cast<const char *>(pszV);
        }
    }

    if (m_bNeedsToSaveWayInfo)
    {
        if (*pabyPtr)
        {
            pabyPtr++;

            OSMInfo sInfo;
            if (psInfo == nullptr)
                psInfo = &sInfo;

            psInfo->ts.nTimeStamp = ReadVarInt64(&pabyPtr);
            psInfo->nChangeset = ReadVarInt64(&pabyPtr);
            psInfo->nVersion = ReadVarInt32(&pabyPtr);
            psInfo->nUID = ReadVarInt32(&pabyPtr);

            psInfo->bTimeStampIsStr = false;
            psInfo->pszUserSID = "";  // FIXME
        }
        else
            pabyPtr++;
    }

    LonLat lonLat;
    memcpy(&lonLat.nLon, pabyPtr, sizeof(int));
    memcpy(&lonLat.nLat, pabyPtr + sizeof(int), sizeof(int));
    asCoords.emplace_back(lonLat);
    pabyPtr += 2 * sizeof(int);
    do
    {
        lonLat.nLon = static_cast<int>(lonLat.nLon + ReadVarSInt64(&pabyPtr));
        lonLat.nLat = static_cast<int>(lonLat.nLat + ReadVarSInt64(&pabyPtr));
        asCoords.emplace_back(lonLat);
    } while (pabyPtr < pabyCompressedWay + nBytes);
}

/************************************************************************/
/*                              IndexWay()                              */
/************************************************************************/

void OGROSMDataSource::IndexWay(GIntBig nWayID, bool bIsArea,
                                unsigned int nTags, const IndexedKVP *pasTags,
                                const LonLat *pasLonLatPairs, int nPairs,
                                const OSMInfo *psInfo)
{
    if (!m_bIndexWays)
        return;

    sqlite3_bind_int64(m_hInsertWayStmt, 1, nWayID);

    const unsigned nTagsClamped = std::min(nTags, MAX_COUNT_FOR_TAGS_IN_WAY);
    if (nTagsClamped < nTags)
    {
        CPLDebug("OSM",
                 "Too many tags for way " CPL_FRMT_GIB ": %u. "
                 "Clamping to %u",
                 nWayID, nTags, nTagsClamped);
    }
    CompressWay(bIsArea, nTagsClamped, pasTags, nPairs, pasLonLatPairs, psInfo,
                m_abyWayBuffer);
    sqlite3_bind_blob(m_hInsertWayStmt, 2, m_abyWayBuffer.data(),
                      static_cast<int>(m_abyWayBuffer.size()), SQLITE_STATIC);

    int rc = sqlite3_step(m_hInsertWayStmt);
    sqlite3_reset(m_hInsertWayStmt);
    if (!(rc == SQLITE_OK || rc == SQLITE_DONE))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed inserting way " CPL_FRMT_GIB ": %s", nWayID,
                 sqlite3_errmsg(m_hDB));
    }
}

/************************************************************************/
/*                              FindNode()                              */
/************************************************************************/

int OGROSMDataSource::FindNode(GIntBig nID)
{
    if (m_nReqIds == 0)
        return -1;
    int iFirst = 0;
    int iLast = m_nReqIds - 1;
    while (iFirst < iLast)
    {
        int iMid = (iFirst + iLast) / 2;
        if (nID > m_panReqIds[iMid])
            iFirst = iMid + 1;
        else
            iLast = iMid;
    }
    if (iFirst == iLast && nID == m_panReqIds[iFirst])
        return iFirst;
    return -1;
}

/************************************************************************/
/*                         ProcessWaysBatch()                           */
/************************************************************************/

void OGROSMDataSource::ProcessWaysBatch()
{
    if (m_asWayFeaturePairs.empty())
        return;

    // printf("nodes = %d, features = %d\n", nUnsortedReqIds, int(m_asWayFeaturePairs.size()));
    LookupNodes();

    for (WayFeaturePair &sWayFeaturePairs : m_asWayFeaturePairs)
    {
        const bool bIsArea = sWayFeaturePairs.bIsArea;
        m_asLonLatCache.clear();

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
        if (m_bHashedIndexValid)
        {
            for (unsigned int i = 0; i < sWayFeaturePairs.nRefs; i++)
            {
                int nIndInHashArray = static_cast<int>(
                    HASH_ID_FUNC(sWayFeaturePairs.panNodeRefs[i]) %
                    HASHED_INDEXES_ARRAY_SIZE);
                int nIdx = m_panHashedIndexes[nIndInHashArray];
                if (nIdx < -1)
                {
                    int iBucket = -nIdx - 2;
                    while (true)
                    {
                        nIdx = m_psCollisionBuckets[iBucket].nInd;
                        if (m_panReqIds[nIdx] ==
                            sWayFeaturePairs.panNodeRefs[i])
                            break;
                        iBucket = m_psCollisionBuckets[iBucket].nNext;
                        if (iBucket < 0)
                        {
                            nIdx = -1;
                            break;
                        }
                    }
                }
                else if (nIdx >= 0 &&
                         m_panReqIds[nIdx] != sWayFeaturePairs.panNodeRefs[i])
                    nIdx = -1;

                if (nIdx >= 0)
                {
                    m_asLonLatCache.push_back(m_pasLonLatArray[nIdx]);
                }
            }
        }
        else
#endif  // ENABLE_NODE_LOOKUP_BY_HASHING
        {
            int nIdx = -1;
            for (unsigned int i = 0; i < sWayFeaturePairs.nRefs; i++)
            {
                if (nIdx >= 0 && sWayFeaturePairs.panNodeRefs[i] ==
                                     sWayFeaturePairs.panNodeRefs[i - 1] + 1)
                {
                    if (static_cast<unsigned>(nIdx + 1) < m_nReqIds &&
                        m_panReqIds[nIdx + 1] ==
                            sWayFeaturePairs.panNodeRefs[i])
                        nIdx++;
                    else
                        nIdx = -1;
                }
                else
                    nIdx = FindNode(sWayFeaturePairs.panNodeRefs[i]);
                if (nIdx >= 0)
                {
                    m_asLonLatCache.push_back(m_pasLonLatArray[nIdx]);
                }
            }
        }

        if (!m_asLonLatCache.empty() && bIsArea)
        {
            m_asLonLatCache.push_back(m_asLonLatCache[0]);
        }

        if (m_asLonLatCache.size() < 2)
        {
            CPLDebug("OSM",
                     "Way " CPL_FRMT_GIB
                     " with %d nodes that could be found. Discarding it",
                     sWayFeaturePairs.nWayID,
                     static_cast<int>(m_asLonLatCache.size()));
            sWayFeaturePairs.poFeature.reset();
            sWayFeaturePairs.bIsArea = false;
            continue;
        }

        if (bIsArea && m_apoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested())
        {
            IndexWay(sWayFeaturePairs.nWayID, /*bIsArea = */ true,
                     sWayFeaturePairs.nTags, sWayFeaturePairs.pasTags,
                     m_asLonLatCache.data(),
                     static_cast<int>(m_asLonLatCache.size()),
                     &sWayFeaturePairs.sInfo);
        }
        else
            IndexWay(sWayFeaturePairs.nWayID, bIsArea, 0, nullptr,
                     m_asLonLatCache.data(),
                     static_cast<int>(m_asLonLatCache.size()), nullptr);

        if (sWayFeaturePairs.poFeature == nullptr)
        {
            continue;
        }

        OGRLineString *poLS = new OGRLineString();
        OGRGeometry *poGeom = poLS;

        const int nPoints = static_cast<int>(m_asLonLatCache.size());
        poLS->setNumPoints(nPoints, /*bZeroizeNewContent=*/false);
        for (int i = 0; i < nPoints; i++)
        {
            poLS->setPoint(i, INT_TO_DBL(m_asLonLatCache[i].nLon),
                           INT_TO_DBL(m_asLonLatCache[i].nLat));
        }

        sWayFeaturePairs.poFeature->SetGeometryDirectly(poGeom);

        if (m_asLonLatCache.size() != sWayFeaturePairs.nRefs)
            CPLDebug("OSM",
                     "For way " CPL_FRMT_GIB
                     ", got only %d nodes instead of %d",
                     sWayFeaturePairs.nWayID, nPoints, sWayFeaturePairs.nRefs);

        bool bFilteredOut = false;
        if (!m_apoLayers[IDX_LYR_LINES]->AddFeature(
                std::move(sWayFeaturePairs.poFeature),
                sWayFeaturePairs.bAttrFilterAlreadyEvaluated, &bFilteredOut,
                !m_bFeatureAdded))
            m_bStopParsing = true;
        else if (!bFilteredOut)
            m_bFeatureAdded = true;
    }

    if (m_apoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested())
    {
        for (WayFeaturePair &sWayFeaturePairs : m_asWayFeaturePairs)
        {
            if (sWayFeaturePairs.bIsArea &&
                (sWayFeaturePairs.nTags || m_bReportAllWays))
            {
                sqlite3_bind_int64(m_hInsertPolygonsStandaloneStmt, 1,
                                   sWayFeaturePairs.nWayID);

                int rc = sqlite3_step(m_hInsertPolygonsStandaloneStmt);
                sqlite3_reset(m_hInsertPolygonsStandaloneStmt);
                if (!(rc == SQLITE_OK || rc == SQLITE_DONE))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Failed inserting into "
                             "polygons_standalone " CPL_FRMT_GIB ": %s",
                             sWayFeaturePairs.nWayID, sqlite3_errmsg(m_hDB));
                }
            }
        }
    }

    m_asWayFeaturePairs.clear();
    m_nUnsortedReqIds = 0;

    m_nAccumulatedTags = 0;
    nNonRedundantKeysLen = 0;
    nNonRedundantValuesLen = 0;
}

/************************************************************************/
/*                      IsClosedWayTaggedAsPolygon()                    */
/************************************************************************/

bool OGROSMDataSource::IsClosedWayTaggedAsPolygon(unsigned int nTags,
                                                  const OSMTag *pasTags)
{
    bool bIsArea = false;
    const int nSizeArea = 4;
    const int nStrnlenK =
        std::max(nSizeArea, m_nMaxSizeKeysInSetClosedWaysArePolygons) + 1;
    std::string oTmpStr;
    oTmpStr.reserve(m_nMaxSizeKeysInSetClosedWaysArePolygons);
    for (unsigned int i = 0; i < nTags; i++)
    {
        const char *pszK = pasTags[i].pszK;
        const int nKLen = static_cast<int>(CPLStrnlen(pszK, nStrnlenK));
        if (nKLen > m_nMaxSizeKeysInSetClosedWaysArePolygons)
            continue;

        if (nKLen == nSizeArea && strcmp(pszK, "area") == 0)
        {
            const char *pszV = pasTags[i].pszV;
            if (strcmp(pszV, "yes") == 0)
            {
                bIsArea = true;
                // final true. We can't have several area tags...
                break;
            }
            else if (strcmp(pszV, "no") == 0)
            {
                bIsArea = false;
                break;
            }
        }
        if (bIsArea)
            continue;

        if (nKLen >= m_nMinSizeKeysInSetClosedWaysArePolygons)
        {
            oTmpStr.assign(pszK, nKLen);
            if (aoSetClosedWaysArePolygons.find(oTmpStr) !=
                aoSetClosedWaysArePolygons.end())
            {
                bIsArea = true;
                continue;
            }
        }

        const char *pszV = pasTags[i].pszV;
        const int nVLen = static_cast<int>(CPLStrnlen(pszV, nStrnlenK));
        if (nKLen + 1 + nVLen >= m_nMinSizeKeysInSetClosedWaysArePolygons &&
            nKLen + 1 + nVLen <= m_nMaxSizeKeysInSetClosedWaysArePolygons)
        {
            oTmpStr.assign(pszK, nKLen);
            oTmpStr.append(1, '=');
            oTmpStr.append(pszV, nVLen);
            if (aoSetClosedWaysArePolygons.find(oTmpStr) !=
                aoSetClosedWaysArePolygons.end())
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

void OGROSMDataSource::NotifyWay(const OSMWay *psWay)
{
    m_nWaysProcessed++;
    if (m_nWaysProcessed % 10000 == 0)
    {
        CPLDebug("OSM", "Ways processed : %d", m_nWaysProcessed);
#ifdef DEBUG_MEM_USAGE
        CPLDebug("OSM", "GetMaxTotalAllocs() = " CPL_FRMT_GUIB,
                 static_cast<GUIntBig>(GetMaxTotalAllocs()));
#endif
    }

    if (!m_bUsePointsIndex)
        return;

    // printf("way %d : %d nodes\n", (int)psWay->nID, (int)psWay->nRefs);

    if (psWay->nRefs < 2)
    {
        CPLDebug("OSM", "Way " CPL_FRMT_GIB " with %d nodes. Discarding it",
                 psWay->nID, psWay->nRefs);
        return;
    }

    /* Is a closed way a polygon ? */
    bool bIsArea = false;
    if (psWay->panNodeRefs[0] == psWay->panNodeRefs[psWay->nRefs - 1])
    {
        bIsArea = IsClosedWayTaggedAsPolygon(psWay->nTags, psWay->pasTags);
    }

    bool bInterestingTag = m_bReportAllWays;
    if (!bIsArea && !m_bReportAllWays)
    {
        for (unsigned int i = 0; i < psWay->nTags; i++)
        {
            const char *pszK = psWay->pasTags[i].pszK;
            if (m_apoLayers[IDX_LYR_LINES]->IsSignificantKey(pszK))
            {
                bInterestingTag = true;
                break;
            }
        }
    }

    std::unique_ptr<OGRFeature> poFeature;
    bool bAttrFilterAlreadyEvaluated = false;
    if (!bIsArea && m_apoLayers[IDX_LYR_LINES]->IsUserInterested() &&
        bInterestingTag)
    {
        poFeature = std::make_unique<OGRFeature>(
            m_apoLayers[IDX_LYR_LINES]->GetLayerDefn());

        m_apoLayers[IDX_LYR_LINES]->SetFieldsFromTags(
            poFeature.get(), psWay->nID, false, psWay->nTags, psWay->pasTags,
            &psWay->sInfo);

        // Optimization: if we have an attribute filter, that does not require
        // geometry, and if we don't need to index ways, then we can just
        // evaluate the attribute filter without the geometry.
        if (m_apoLayers[IDX_LYR_LINES]->HasAttributeFilter() &&
            !m_apoLayers[IDX_LYR_LINES]
                 ->AttributeFilterEvaluationNeedsGeometry() &&
            !m_bIndexWays)
        {
            if (!m_apoLayers[IDX_LYR_LINES]->EvaluateAttributeFilter(
                    poFeature.get()))
            {
                return;
            }
            bAttrFilterAlreadyEvaluated = true;
        }
    }
    else if (!m_bIndexWays)
    {
        return;
    }

    if (m_nUnsortedReqIds + psWay->nRefs >
            static_cast<unsigned int>(MAX_ACCUMULATED_NODES) ||
        m_asWayFeaturePairs.size() ==
            static_cast<size_t>(MAX_DELAYED_FEATURES) ||
        m_nAccumulatedTags + psWay->nTags >
            static_cast<unsigned int>(MAX_ACCUMULATED_TAGS) ||
        nNonRedundantKeysLen + 1024 > MAX_NON_REDUNDANT_KEYS ||
        nNonRedundantValuesLen + 1024 > MAX_NON_REDUNDANT_VALUES)
    {
        ProcessWaysBatch();
    }

    m_asWayFeaturePairs.push_back(WayFeaturePair());
    WayFeaturePair &sWayFeaturePairs = m_asWayFeaturePairs.back();

    sWayFeaturePairs.nWayID = psWay->nID;
    sWayFeaturePairs.nRefs = psWay->nRefs - (bIsArea ? 1 : 0);
    sWayFeaturePairs.panNodeRefs = m_panUnsortedReqIds + m_nUnsortedReqIds;
    sWayFeaturePairs.poFeature = std::move(poFeature);
    sWayFeaturePairs.bIsArea = bIsArea;
    sWayFeaturePairs.bAttrFilterAlreadyEvaluated = bAttrFilterAlreadyEvaluated;

    if (bIsArea && m_apoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested())
    {
        unsigned int nTagCount = 0;

        if (m_bNeedsToSaveWayInfo)
        {
            if (!psWay->sInfo.bTimeStampIsStr)
                sWayFeaturePairs.sInfo.ts.nTimeStamp =
                    psWay->sInfo.ts.nTimeStamp;
            else
            {
                OGRField sField;
                if (OGRParseXMLDateTime(psWay->sInfo.ts.pszTimeStamp, &sField))
                {
                    struct tm brokendown;
                    memset(&brokendown, 0, sizeof(brokendown));
                    brokendown.tm_year = sField.Date.Year - 1900;
                    brokendown.tm_mon = sField.Date.Month - 1;
                    brokendown.tm_mday = sField.Date.Day;
                    brokendown.tm_hour = sField.Date.Hour;
                    brokendown.tm_min = sField.Date.Minute;
                    brokendown.tm_sec =
                        static_cast<int>(sField.Date.Second + .5);
                    sWayFeaturePairs.sInfo.ts.nTimeStamp =
                        CPLYMDHMSToUnixTime(&brokendown);
                }
                else
                    sWayFeaturePairs.sInfo.ts.nTimeStamp = 0;
            }
            sWayFeaturePairs.sInfo.nChangeset = psWay->sInfo.nChangeset;
            sWayFeaturePairs.sInfo.nVersion = psWay->sInfo.nVersion;
            sWayFeaturePairs.sInfo.nUID = psWay->sInfo.nUID;
            sWayFeaturePairs.sInfo.bTimeStampIsStr = false;
            sWayFeaturePairs.sInfo.pszUserSID = "";  // FIXME
        }
        else
        {
            sWayFeaturePairs.sInfo.ts.nTimeStamp = 0;
            sWayFeaturePairs.sInfo.nChangeset = 0;
            sWayFeaturePairs.sInfo.nVersion = 0;
            sWayFeaturePairs.sInfo.nUID = 0;
            sWayFeaturePairs.sInfo.bTimeStampIsStr = false;
            sWayFeaturePairs.sInfo.pszUserSID = "";
        }

        sWayFeaturePairs.pasTags = m_pasAccumulatedTags + m_nAccumulatedTags;

        for (unsigned int iTag = 0; iTag < psWay->nTags; iTag++)
        {
            const char *pszK = psWay->pasTags[iTag].pszK;
            const char *pszV = psWay->pasTags[iTag].pszV;

            if (std::any_of(begin(m_ignoredKeys), end(m_ignoredKeys),
                            [pszK](const char *pszIgnoredKey)
                            { return strcmp(pszK, pszIgnoredKey) == 0; }))
            {
                continue;
            }

            auto oIterK = m_aoMapIndexedKeys.find(pszK);
            KeyDesc *psKD = nullptr;
            if (oIterK == m_aoMapIndexedKeys.end())
            {
                if (m_apsKeys.size() >= 1 + MAX_INDEXED_KEYS)
                {
                    if (m_apsKeys.size() == 1 + MAX_INDEXED_KEYS)
                    {
                        CPLDebug("OSM", "More than %d different keys found",
                                 MAX_INDEXED_KEYS);
                        // To avoid next warnings.
                        m_apsKeys.push_back(nullptr);
                    }

                    const int nLenK = static_cast<int>(strlen(pszK)) + 1;
                    if (nNonRedundantKeysLen + nLenK > MAX_NON_REDUNDANT_KEYS)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Too many/too long keys found");
                        continue;
                    }
                    memcpy(pabyNonRedundantKeys + nNonRedundantKeysLen, pszK,
                           nLenK);
                    m_pasAccumulatedTags[m_nAccumulatedTags].bKIsIndex = FALSE;
                    m_pasAccumulatedTags[m_nAccumulatedTags]
                        .uKey.nOffsetInpabyNonRedundantKeys =
                        nNonRedundantKeysLen;
                    nNonRedundantKeysLen += nLenK;
                }
                else
                {
                    psKD = new KeyDesc();
                    psKD->pszK = CPLStrdup(pszK);
                    psKD->nKeyIndex = static_cast<int>(m_apsKeys.size());
                    psKD->nOccurrences = 0;
                    psKD->apszValues.push_back(CPLStrdup(
                        ""));  // guard value to avoid index 0 to be used
                    m_aoMapIndexedKeys[psKD->pszK] = psKD;
                    m_apsKeys.push_back(psKD);
                }
            }
            else
            {
                psKD = oIterK->second;
            }

            if (psKD)
            {
                psKD->nOccurrences++;
                m_pasAccumulatedTags[m_nAccumulatedTags].bKIsIndex = TRUE;
                m_pasAccumulatedTags[m_nAccumulatedTags].uKey.nKeyIndex =
                    psKD->nKeyIndex;
            }

            if (psKD != nullptr &&
                psKD->apszValues.size() < 1 + MAX_INDEXED_VALUES_PER_KEY)
            {
                int nValueIndex = 0;
                auto oIterV = psKD->anMapV.find(pszV);
                if (oIterV == psKD->anMapV.end())
                {
                    char *pszVDup = CPLStrdup(pszV);
                    nValueIndex = static_cast<int>(psKD->apszValues.size());
                    psKD->anMapV[pszVDup] = nValueIndex;
                    psKD->apszValues.push_back(pszVDup);
                }
                else
                    nValueIndex = oIterV->second;

                m_pasAccumulatedTags[m_nAccumulatedTags].bVIsIndex = TRUE;
                m_pasAccumulatedTags[m_nAccumulatedTags].uVal.nValueIndex =
                    nValueIndex;
            }
            else
            {
                const int nLenV = static_cast<int>(strlen(pszV)) + 1;

                if (psKD != nullptr &&
                    psKD->apszValues.size() == 1 + MAX_INDEXED_VALUES_PER_KEY)
                {
                    CPLDebug("OSM", "More than %d different values for tag %s",
                             MAX_INDEXED_VALUES_PER_KEY, pszK);
                    // To avoid next warnings.
                    psKD->apszValues.push_back(CPLStrdup(""));
                }

                if (nNonRedundantValuesLen + nLenV > MAX_NON_REDUNDANT_VALUES)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Too many/too long values found");
                    continue;
                }
                memcpy(pabyNonRedundantValues + nNonRedundantValuesLen, pszV,
                       nLenV);
                m_pasAccumulatedTags[m_nAccumulatedTags].bVIsIndex = FALSE;
                m_pasAccumulatedTags[m_nAccumulatedTags]
                    .uVal.nOffsetInpabyNonRedundantValues =
                    nNonRedundantValuesLen;
                nNonRedundantValuesLen += nLenV;
            }
            m_nAccumulatedTags++;

            nTagCount++;
            if (nTagCount == MAX_COUNT_FOR_TAGS_IN_WAY)
                break;
        }

        sWayFeaturePairs.nTags = nTagCount;
    }
    else
    {
        sWayFeaturePairs.sInfo.ts.nTimeStamp = 0;
        sWayFeaturePairs.sInfo.nChangeset = 0;
        sWayFeaturePairs.sInfo.nVersion = 0;
        sWayFeaturePairs.sInfo.nUID = 0;
        sWayFeaturePairs.sInfo.bTimeStampIsStr = false;
        sWayFeaturePairs.sInfo.pszUserSID = "";

        sWayFeaturePairs.nTags = 0;
        sWayFeaturePairs.pasTags = nullptr;
    }

    memcpy(m_panUnsortedReqIds + m_nUnsortedReqIds, psWay->panNodeRefs,
           sizeof(GIntBig) * (psWay->nRefs - (bIsArea ? 1 : 0)));
    m_nUnsortedReqIds += (psWay->nRefs - (bIsArea ? 1 : 0));
}

static void OGROSMNotifyWay(OSMWay *psWay, OSMContext * /* psOSMContext */,
                            void *user_data)
{
    static_cast<OGROSMDataSource *>(user_data)->NotifyWay(psWay);
}

/************************************************************************/
/*                            LookupWays()                              */
/************************************************************************/

unsigned int OGROSMDataSource::LookupWays(
    std::map<GIntBig, std::pair<int, void *>> &aoMapWays,
    const OSMRelation *psRelation)
{
    unsigned int nFound = 0;
    unsigned int iCur = 0;

    while (iCur < psRelation->nMembers)
    {
        unsigned int nToQuery = 0;
        unsigned int i = iCur;  // Used after for.
        for (; i < psRelation->nMembers; i++)
        {
            if (psRelation->pasMembers[i].eType == MEMBER_WAY &&
                strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0)
            {
                nToQuery++;
                if (nToQuery ==
                    static_cast<unsigned int>(LIMIT_IDS_PER_REQUEST))
                {
                    break;
                }
            }
        }

        if (nToQuery == 0)
            break;

        unsigned int iLastI = (i == psRelation->nMembers) ? i : i + 1;

        sqlite3_stmt *hStmt = m_pahSelectWayStmt[nToQuery - 1];
        unsigned int nBindIndex = 1;
        for (i = iCur; i < iLastI; i++)
        {
            if (psRelation->pasMembers[i].eType == MEMBER_WAY &&
                strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0)
            {
                sqlite3_bind_int64(hStmt, nBindIndex,
                                   psRelation->pasMembers[i].nID);
                nBindIndex++;
            }
        }
        iCur = iLastI;

        while (sqlite3_step(hStmt) == SQLITE_ROW)
        {
            GIntBig id = sqlite3_column_int64(hStmt, 0);
            if (aoMapWays.find(id) == aoMapWays.end())
            {
                int nBlobSize = sqlite3_column_bytes(hStmt, 1);
                const void *blob = sqlite3_column_blob(hStmt, 1);
                void *blob_dup = CPLMalloc(nBlobSize);
                memcpy(blob_dup, blob, nBlobSize);
                aoMapWays[id] = std::pair(nBlobSize, blob_dup);
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

OGRGeometry *OGROSMDataSource::BuildMultiPolygon(const OSMRelation *psRelation,
                                                 unsigned int *pnTags,
                                                 OSMTag *pasTags)
{
    std::map<GIntBig, std::pair<int, void *>> aoMapWays;
    LookupWays(aoMapWays, psRelation);

    bool bMissing = false;

    for (unsigned int i = 0; i < psRelation->nMembers; i++)
    {
        if (psRelation->pasMembers[i].eType == MEMBER_WAY &&
            strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0)
        {
            if (aoMapWays.find(psRelation->pasMembers[i].nID) ==
                aoMapWays.end())
            {
                CPLDebug("OSM",
                         "Relation " CPL_FRMT_GIB
                         " has missing ways. Ignoring it",
                         psRelation->nID);
                bMissing = true;
                break;
            }
        }
    }

    if (bMissing)
    {
        // cppcheck-suppress constVariableReference
        for (auto &oIter : aoMapWays)
            CPLFree(oIter.second.second);

        return nullptr;
    }

    OGRMultiLineString oMLS;
    std::vector<OGRGeometry *> apoPolygons(psRelation->nMembers);
    int nPolys = 0;

    if (pnTags != nullptr)
        *pnTags = 0;

    for (unsigned int i = 0; i < psRelation->nMembers; i++)
    {
        if (psRelation->pasMembers[i].eType == MEMBER_WAY &&
            strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0)
        {
            const auto &oGeom = aoMapWays[psRelation->pasMembers[i].nID];

            if (pnTags != nullptr && *pnTags == 0 &&
                strcmp(psRelation->pasMembers[i].pszRole, "outer") == 0)
            {
                // This backup in m_abyWayBuffer is crucial for safe memory
                // usage, as pasTags[].pszV will point to it !
                m_abyWayBuffer.clear();
                m_abyWayBuffer.insert(m_abyWayBuffer.end(),
                                      static_cast<const GByte *>(oGeom.second),
                                      static_cast<const GByte *>(oGeom.second) +
                                          oGeom.first);

                UncompressWay(oGeom.first, m_abyWayBuffer.data(), nullptr,
                              m_asLonLatCache, pnTags, pasTags, nullptr);
            }
            else
            {
                UncompressWay(oGeom.first,
                              static_cast<const GByte *>(oGeom.second), nullptr,
                              m_asLonLatCache, nullptr, nullptr, nullptr);
            }

            OGRLineString *poLS = nullptr;

            if (!m_asLonLatCache.empty() &&
                m_asLonLatCache.front().nLon == m_asLonLatCache.back().nLon &&
                m_asLonLatCache.front().nLat == m_asLonLatCache.back().nLat)
            {
                OGRPolygon *poPoly = new OGRPolygon();
                OGRLinearRing *poRing = new OGRLinearRing();
                poPoly->addRingDirectly(poRing);
                apoPolygons[nPolys++] = poPoly;
                poLS = poRing;

                if (strcmp(psRelation->pasMembers[i].pszRole, "outer") == 0)
                {
                    sqlite3_bind_int64(m_hDeletePolygonsStandaloneStmt, 1,
                                       psRelation->pasMembers[i].nID);
                    CPL_IGNORE_RET_VAL(
                        sqlite3_step(m_hDeletePolygonsStandaloneStmt));
                    sqlite3_reset(m_hDeletePolygonsStandaloneStmt);
                }
            }
            else
            {
                poLS = new OGRLineString();
                oMLS.addGeometryDirectly(poLS);
            }

            const int nPoints = static_cast<int>(m_asLonLatCache.size());
            poLS->setNumPoints(nPoints, /*bZeroizeNewContent=*/false);
            for (int j = 0; j < nPoints; j++)
            {
                poLS->setPoint(j, INT_TO_DBL(m_asLonLatCache[j].nLon),
                               INT_TO_DBL(m_asLonLatCache[j].nLat));
            }
        }
    }

    if (oMLS.getNumGeometries() > 0)
    {
        auto poPolyFromEdges = std::unique_ptr<OGRGeometry>(
            OGRGeometry::FromHandle(OGRBuildPolygonFromEdges(
                OGRGeometry::ToHandle(&oMLS), TRUE, FALSE, 0, nullptr)));
        if (poPolyFromEdges && poPolyFromEdges->getGeometryType() == wkbPolygon)
        {
            const OGRPolygon *poSuperPoly = poPolyFromEdges->toPolygon();
            for (const OGRLinearRing *poRing : *poSuperPoly)
            {
                if (poRing != nullptr && poRing->getNumPoints() >= 4 &&
                    poRing->getX(0) ==
                        poRing->getX(poRing->getNumPoints() - 1) &&
                    poRing->getY(0) == poRing->getY(poRing->getNumPoints() - 1))
                {
                    OGRPolygon *poPoly = new OGRPolygon();
                    poPoly->addRing(poRing);
                    apoPolygons[nPolys++] = poPoly;
                }
            }
        }
    }

    std::unique_ptr<OGRGeometry> poRet;

    if (nPolys > 0)
    {
        int bIsValidGeometry = FALSE;
        const char *apszOptions[2] = {"METHOD=DEFAULT", nullptr};
        auto poGeom =
            std::unique_ptr<OGRGeometry>(OGRGeometryFactory::organizePolygons(
                apoPolygons.data(), nPolys, &bIsValidGeometry, apszOptions));

        if (poGeom && poGeom->getGeometryType() == wkbPolygon)
        {
            auto poMulti = std::make_unique<OGRMultiPolygon>();
            poMulti->addGeometryDirectly(poGeom.release());
            poGeom = std::move(poMulti);
        }

        if (poGeom && poGeom->getGeometryType() == wkbMultiPolygon)
        {
            poRet = std::move(poGeom);
        }
        else
        {
            CPLDebug("OSM",
                     "Relation " CPL_FRMT_GIB
                     ": Geometry has incompatible type : %s",
                     psRelation->nID,
                     poGeom ? OGR_G_GetGeometryName(
                                  OGRGeometry::ToHandle(poGeom.get()))
                            : "null");
        }
    }

    // cppcheck-suppress constVariableReference
    for (auto &oIter : aoMapWays)
        CPLFree(oIter.second.second);

    return poRet.release();
}

/************************************************************************/
/*                          BuildGeometryCollection()                   */
/************************************************************************/

OGRGeometry *
OGROSMDataSource::BuildGeometryCollection(const OSMRelation *psRelation,
                                          bool bMultiLineString)
{
    std::map<GIntBig, std::pair<int, void *>> aoMapWays;
    LookupWays(aoMapWays, psRelation);

    std::unique_ptr<OGRGeometryCollection> poColl =
        bMultiLineString ? std::make_unique<OGRMultiLineString>()
                         : std::make_unique<OGRGeometryCollection>();

    for (unsigned int i = 0; i < psRelation->nMembers; i++)
    {
        if (psRelation->pasMembers[i].eType == MEMBER_NODE && !bMultiLineString)
        {
            m_nUnsortedReqIds = 1;
            m_panUnsortedReqIds[0] = psRelation->pasMembers[i].nID;
            LookupNodes();
            if (m_nReqIds == 1)
            {
                poColl->addGeometryDirectly(
                    new OGRPoint(INT_TO_DBL(m_pasLonLatArray[0].nLon),
                                 INT_TO_DBL(m_pasLonLatArray[0].nLat)));
            }
        }
        else if (psRelation->pasMembers[i].eType == MEMBER_WAY &&
                 strcmp(psRelation->pasMembers[i].pszRole, "subarea") != 0 &&
                 aoMapWays.find(psRelation->pasMembers[i].nID) !=
                     aoMapWays.end())
        {
            const auto &oGeom = aoMapWays[psRelation->pasMembers[i].nID];

            bool bIsArea = false;
            UncompressWay(oGeom.first, reinterpret_cast<GByte *>(oGeom.second),
                          &bIsArea, m_asLonLatCache, nullptr, nullptr, nullptr);
            OGRLineString *poLS = nullptr;
            if (bIsArea && !bMultiLineString)
            {
                OGRLinearRing *poLR = new OGRLinearRing();
                OGRPolygon *poPoly = new OGRPolygon();
                poPoly->addRingDirectly(poLR);
                poColl->addGeometryDirectly(poPoly);
                poLS = poLR;
            }
            else
            {
                poLS = new OGRLineString();
                poColl->addGeometryDirectly(poLS);
            }

            const int nPoints = static_cast<int>(m_asLonLatCache.size());
            poLS->setNumPoints(nPoints, /*bZeroizeNewContent=*/false);
            for (int j = 0; j < nPoints; j++)
            {
                poLS->setPoint(j, INT_TO_DBL(m_asLonLatCache[j].nLon),
                               INT_TO_DBL(m_asLonLatCache[j].nLat));
            }
        }
    }

    if (poColl->getNumGeometries() == 0)
    {
        poColl.reset();
    }

    // cppcheck-suppress constVariableReference
    for (auto &oIter : aoMapWays)
        CPLFree(oIter.second.second);

    return poColl.release();
}

/************************************************************************/
/*                            NotifyRelation()                          */
/************************************************************************/

void OGROSMDataSource::NotifyRelation(const OSMRelation *psRelation)
{
    if (!m_asWayFeaturePairs.empty())
        ProcessWaysBatch();

    m_nRelationsProcessed++;
    if ((m_nRelationsProcessed % 10000) == 0)
    {
        CPLDebug("OSM", "Relations processed : %d", m_nRelationsProcessed);
#ifdef DEBUG_MEM_USAGE
        CPLDebug("OSM", "GetMaxTotalAllocs() = " CPL_FRMT_GUIB,
                 static_cast<GUIntBig>(GetMaxTotalAllocs()));
#endif
    }

    if (!m_bUseWaysIndex)
        return;

    bool bMultiPolygon = false;
    bool bMultiLineString = false;
    bool bInterestingTagFound = false;
    const char *pszTypeV = nullptr;
    for (unsigned int i = 0; i < psRelation->nTags; i++)
    {
        const char *pszK = psRelation->pasTags[i].pszK;
        if (strcmp(pszK, "type") == 0)
        {
            const char *pszV = psRelation->pasTags[i].pszV;
            pszTypeV = pszV;
            if (strcmp(pszV, "multipolygon") == 0 ||
                strcmp(pszV, "boundary") == 0)
            {
                bMultiPolygon = true;
            }
            else if (strcmp(pszV, "multilinestring") == 0 ||
                     strcmp(pszV, "route") == 0)
            {
                bMultiLineString = true;
            }
        }
        else if (strcmp(pszK, "created_by") != 0)
            bInterestingTagFound = true;
    }

    // Optimization: If we have an attribute filter, that does not require
    // geometry, then we can just evaluate the attribute filter without the
    // geometry.
    const int iCurLayer = bMultiPolygon      ? IDX_LYR_MULTIPOLYGONS
                          : bMultiLineString ? IDX_LYR_MULTILINESTRINGS
                                             : IDX_LYR_OTHER_RELATIONS;
    if (!m_apoLayers[iCurLayer]->IsUserInterested())
        return;

    std::unique_ptr<OGRFeature> poFeature;

    if (!(bMultiPolygon && !bInterestingTagFound) &&
        // We cannot do early filtering for multipolygon that has no
        // interesting tag, since we may fetch attributes from ways.
        m_apoLayers[iCurLayer]->HasAttributeFilter() &&
        !m_apoLayers[iCurLayer]->AttributeFilterEvaluationNeedsGeometry())
    {
        poFeature = std::make_unique<OGRFeature>(
            m_apoLayers[iCurLayer]->GetLayerDefn());

        m_apoLayers[iCurLayer]->SetFieldsFromTags(
            poFeature.get(), psRelation->nID, false, psRelation->nTags,
            psRelation->pasTags, &psRelation->sInfo);

        if (!m_apoLayers[iCurLayer]->EvaluateAttributeFilter(poFeature.get()))
        {
            return;
        }
    }

    OGRGeometry *poGeom = nullptr;

    unsigned int nExtraTags = 0;
    OSMTag pasExtraTags[1 + MAX_COUNT_FOR_TAGS_IN_WAY];

    if (bMultiPolygon)
    {
        if (!bInterestingTagFound)
        {
            poGeom = BuildMultiPolygon(psRelation, &nExtraTags, pasExtraTags);
            CPLAssert(nExtraTags <= MAX_COUNT_FOR_TAGS_IN_WAY);
            pasExtraTags[nExtraTags].pszK = "type";
            pasExtraTags[nExtraTags].pszV = pszTypeV;
            nExtraTags++;
        }
        else
            poGeom = BuildMultiPolygon(psRelation, nullptr, nullptr);
    }
    else
        poGeom = BuildGeometryCollection(psRelation, bMultiLineString);

    if (poGeom != nullptr)
    {
        bool bAttrFilterAlreadyEvaluated = true;
        if (poFeature == nullptr)
        {
            poFeature = std::make_unique<OGRFeature>(
                m_apoLayers[iCurLayer]->GetLayerDefn());

            m_apoLayers[iCurLayer]->SetFieldsFromTags(
                poFeature.get(), psRelation->nID, false,
                nExtraTags ? nExtraTags : psRelation->nTags,
                nExtraTags ? pasExtraTags : psRelation->pasTags,
                &psRelation->sInfo);

            bAttrFilterAlreadyEvaluated = false;
        }

        poFeature->SetGeometryDirectly(poGeom);

        bool bFilteredOut = FALSE;
        if (!m_apoLayers[iCurLayer]->AddFeature(
                std::move(poFeature), bAttrFilterAlreadyEvaluated,
                &bFilteredOut, !m_bFeatureAdded))
            m_bStopParsing = true;
        else if (!bFilteredOut)
            m_bFeatureAdded = true;
    }
}

static void OGROSMNotifyRelation(OSMRelation *psRelation,
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

    if (!m_bHasRowInPolygonsStandalone)
        m_bHasRowInPolygonsStandalone =
            sqlite3_step(m_hSelectPolygonsStandaloneStmt) == SQLITE_ROW;

    bool bFirst = true;

    while (m_bHasRowInPolygonsStandalone &&
           m_apoLayers[IDX_LYR_MULTIPOLYGONS]->m_apoFeatures.size() < 10000)
    {
        if (bFirst)
        {
            CPLDebug("OSM", "Remaining standalone polygons");
            bFirst = false;
        }

        GIntBig id = sqlite3_column_int64(m_hSelectPolygonsStandaloneStmt, 0);

        sqlite3_bind_int64(m_pahSelectWayStmt[0], 1, id);
        if (sqlite3_step(m_pahSelectWayStmt[0]) == SQLITE_ROW)
        {
            int nBlobSize = sqlite3_column_bytes(m_pahSelectWayStmt[0], 1);
            const void *blob = sqlite3_column_blob(m_pahSelectWayStmt[0], 1);

            // coverity[tainted_data]
            UncompressWay(nBlobSize, static_cast<const GByte *>(blob), nullptr,
                          m_asLonLatCache, &nTags, pasTags, &sInfo);
            CPLAssert(nTags <= MAX_COUNT_FOR_TAGS_IN_WAY);

            OGRMultiPolygon *poMulti = new OGRMultiPolygon();
            OGRPolygon *poPoly = new OGRPolygon();
            OGRLinearRing *poRing = new OGRLinearRing();
            poMulti->addGeometryDirectly(poPoly);
            poPoly->addRingDirectly(poRing);
            OGRLineString *poLS = poRing;

            poLS->setNumPoints(static_cast<int>(m_asLonLatCache.size()),
                               /*bZeroizeNewContent=*/false);
            for (int j = 0; j < static_cast<int>(m_asLonLatCache.size()); j++)
            {
                poLS->setPoint(j, INT_TO_DBL(m_asLonLatCache[j].nLon),
                               INT_TO_DBL(m_asLonLatCache[j].nLat));
            }

            auto poFeature = std::make_unique<OGRFeature>(
                m_apoLayers[IDX_LYR_MULTIPOLYGONS]->GetLayerDefn());

            m_apoLayers[IDX_LYR_MULTIPOLYGONS]->SetFieldsFromTags(
                poFeature.get(), id, true, nTags, pasTags, &sInfo);

            poFeature->SetGeometryDirectly(poMulti);

            bool bFilteredOut = false;
            if (!m_apoLayers[IDX_LYR_MULTIPOLYGONS]->AddFeature(
                    std::move(poFeature), FALSE, &bFilteredOut,
                    !m_bFeatureAdded))
            {
                m_bStopParsing = true;
                break;
            }
            else if (!bFilteredOut)
            {
                m_bFeatureAdded = true;
            }
        }
        else
        {
            CPLAssert(false);
        }

        sqlite3_reset(m_pahSelectWayStmt[0]);

        m_bHasRowInPolygonsStandalone =
            sqlite3_step(m_hSelectPolygonsStandaloneStmt) == SQLITE_ROW;
    }
}

/************************************************************************/
/*                             NotifyBounds()                           */
/************************************************************************/

void OGROSMDataSource::NotifyBounds(double dfXMin, double dfYMin, double dfXMax,
                                    double dfYMax)
{
    m_sExtent.MinX = dfXMin;
    m_sExtent.MinY = dfYMin;
    m_sExtent.MaxX = dfXMax;
    m_sExtent.MaxY = dfYMax;
    m_bExtentValid = true;

    CPLDebug("OSM", "Got bounds : minx=%f, miny=%f, maxx=%f, maxy=%f", dfXMin,
             dfYMin, dfXMax, dfYMax);
}

static void OGROSMNotifyBounds(double dfXMin, double dfYMin, double dfXMax,
                               double dfYMax, OSMContext * /* psCtxt */,
                               void *user_data)
{
    static_cast<OGROSMDataSource *>(user_data)->NotifyBounds(dfXMin, dfYMin,
                                                             dfXMax, dfYMax);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROSMDataSource::Open(const char *pszFilename,
                           CSLConstList papszOpenOptionsIn)

{
    m_psParser = OSM_Open(pszFilename, OGROSMNotifyNodes, OGROSMNotifyWay,
                          OGROSMNotifyRelation, OGROSMNotifyBounds, this);
    if (m_psParser == nullptr)
        return FALSE;

    if (CPLFetchBool(papszOpenOptionsIn, "INTERLEAVED_READING", false))
        m_bInterleavedReading = TRUE;

    /* The following 4 config options are only useful for debugging */
    m_bIndexPoints = CPLTestBool(CPLGetConfigOption("OSM_INDEX_POINTS", "YES"));
    m_bUsePointsIndex =
        CPLTestBool(CPLGetConfigOption("OSM_USE_POINTS_INDEX", "YES"));
    m_bIndexWays = CPLTestBool(CPLGetConfigOption("OSM_INDEX_WAYS", "YES"));
    m_bUseWaysIndex =
        CPLTestBool(CPLGetConfigOption("OSM_USE_WAYS_INDEX", "YES"));

    m_bCustomIndexing = CPLTestBool(CSLFetchNameValueDef(
        papszOpenOptionsIn, "USE_CUSTOM_INDEXING",
        CPLGetConfigOption("OSM_USE_CUSTOM_INDEXING", "YES")));
    if (!m_bCustomIndexing)
        CPLDebug("OSM", "Using SQLite indexing for points");
    m_bCompressNodes = CPLTestBool(
        CSLFetchNameValueDef(papszOpenOptionsIn, "COMPRESS_NODES",
                             CPLGetConfigOption("OSM_COMPRESS_NODES", "NO")));
    if (m_bCompressNodes)
        CPLDebug("OSM", "Using compression for nodes DB");

    // Do not change the below order without updating the IDX_LYR_ constants!
    m_apoLayers.emplace_back(
        std::make_unique<OGROSMLayer>(this, IDX_LYR_POINTS, "points"));
    m_apoLayers.back()->GetLayerDefn()->SetGeomType(wkbPoint);

    m_apoLayers.emplace_back(
        std::make_unique<OGROSMLayer>(this, IDX_LYR_LINES, "lines"));
    m_apoLayers.back()->GetLayerDefn()->SetGeomType(wkbLineString);

    m_apoLayers.emplace_back(std::make_unique<OGROSMLayer>(
        this, IDX_LYR_MULTILINESTRINGS, "multilinestrings"));
    m_apoLayers.back()->GetLayerDefn()->SetGeomType(wkbMultiLineString);

    m_apoLayers.emplace_back(std::make_unique<OGROSMLayer>(
        this, IDX_LYR_MULTIPOLYGONS, "multipolygons"));
    m_apoLayers.back()->GetLayerDefn()->SetGeomType(wkbMultiPolygon);

    m_apoLayers.emplace_back(std::make_unique<OGROSMLayer>(
        this, IDX_LYR_OTHER_RELATIONS, "other_relations"));
    m_apoLayers.back()->GetLayerDefn()->SetGeomType(wkbGeometryCollection);

    if (!ParseConf(papszOpenOptionsIn))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not parse configuration file for OSM import");
        return FALSE;
    }

    const char *pszTagsFormat =
        CSLFetchNameValue(papszOpenOptionsIn, "TAGS_FORMAT");
    if (pszTagsFormat)
    {
        if (EQUAL(pszTagsFormat, "JSON"))
            m_bTagsAsHSTORE = false;
        else if (EQUAL(pszTagsFormat, "HSTORE"))
            m_bTagsAsHSTORE = true;
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Invalid value for TAGS_FORMAT open option: %s",
                     pszTagsFormat);
        }
    }

    const auto eTagsSubType = m_bTagsAsHSTORE ? OFSTNone : OFSTJSON;
    for (auto &&poLayer : m_apoLayers)
    {
        if (poLayer->HasAllTags())
        {
            poLayer->AddField("all_tags", OFTString, eTagsSubType);
            if (poLayer->HasOtherTags())
            {
                poLayer->SetHasOtherTags(false);
            }
        }
        else if (poLayer->HasOtherTags())
            poLayer->AddField("other_tags", OFTString, eTagsSubType);
    }

    m_bNeedsToSaveWayInfo =
        (m_apoLayers[IDX_LYR_MULTIPOLYGONS]->HasTimestamp() ||
         m_apoLayers[IDX_LYR_MULTIPOLYGONS]->HasChangeset() ||
         m_apoLayers[IDX_LYR_MULTIPOLYGONS]->HasVersion() ||
         m_apoLayers[IDX_LYR_MULTIPOLYGONS]->HasUID() ||
         m_apoLayers[IDX_LYR_MULTIPOLYGONS]->HasUser());

    m_panReqIds = static_cast<GIntBig *>(
        VSI_MALLOC_VERBOSE(MAX_ACCUMULATED_NODES * sizeof(GIntBig)));
#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    m_panHashedIndexes = static_cast<int *>(
        VSI_MALLOC_VERBOSE(HASHED_INDEXES_ARRAY_SIZE * sizeof(int)));
    m_psCollisionBuckets = static_cast<CollisionBucket *>(VSI_MALLOC_VERBOSE(
        COLLISION_BUCKET_ARRAY_SIZE * sizeof(CollisionBucket)));
#endif
    m_pasLonLatArray = static_cast<LonLat *>(
        VSI_MALLOC_VERBOSE(MAX_ACCUMULATED_NODES * sizeof(LonLat)));
    m_panUnsortedReqIds = static_cast<GIntBig *>(
        VSI_MALLOC_VERBOSE(MAX_ACCUMULATED_NODES * sizeof(GIntBig)));
    try
    {
        m_asWayFeaturePairs.reserve(MAX_DELAYED_FEATURES);
    }
    catch (const std::exception &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "OGROSMDataSource::Open(): out of memory");
        return FALSE;
    }
    m_pasAccumulatedTags = static_cast<IndexedKVP *>(
        VSI_MALLOC_VERBOSE(MAX_ACCUMULATED_TAGS * sizeof(IndexedKVP)));
    pabyNonRedundantValues =
        static_cast<GByte *>(VSI_MALLOC_VERBOSE(MAX_NON_REDUNDANT_VALUES));
    pabyNonRedundantKeys =
        static_cast<GByte *>(VSI_MALLOC_VERBOSE(MAX_NON_REDUNDANT_KEYS));
    if (m_panReqIds == nullptr || m_pasLonLatArray == nullptr ||
        m_panUnsortedReqIds == nullptr || m_pasAccumulatedTags == nullptr ||
        pabyNonRedundantValues == nullptr || pabyNonRedundantKeys == nullptr)
    {
        return FALSE;
    }

    m_nMaxSizeForInMemoryDBInMB = atoi(CSLFetchNameValueDef(
        papszOpenOptionsIn, "MAX_TMPFILE_SIZE",
        CPLGetConfigOption("OSM_MAX_TMPFILE_SIZE", "100")));
    if (m_nMaxSizeForInMemoryDBInMB == 0)
        m_nMaxSizeForInMemoryDBInMB = 1;
    GIntBig nSize =
        static_cast<GIntBig>(m_nMaxSizeForInMemoryDBInMB) * 1024 * 1024;
    if (nSize < 0 ||
        static_cast<GUIntBig>(nSize) > std::numeric_limits<size_t>::max() / 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for OSM_MAX_TMPFILE_SIZE. Using 100 instead.");
        m_nMaxSizeForInMemoryDBInMB = 100;
        nSize = static_cast<GIntBig>(m_nMaxSizeForInMemoryDBInMB) * 1024 * 1024;
    }

    if (m_bCustomIndexing)
    {
        m_pabySector = static_cast<GByte *>(VSI_CALLOC_VERBOSE(1, SECTOR_SIZE));

        if (m_pabySector == nullptr)
        {
            return FALSE;
        }

        m_bInMemoryNodesFile = true;
        m_osNodesFilename = VSIMemGenerateHiddenFilename("osm_temp_nodes");
        m_fpNodes = VSIFOpenL(m_osNodesFilename, "wb+");
        if (m_fpNodes == nullptr)
        {
            return FALSE;
        }

        CPLPushErrorHandler(CPLQuietErrorHandler);
        const bool bSuccess =
            VSIFTruncateL(m_fpNodes,
                          static_cast<vsi_l_offset>(nSize * 3 / 4)) == 0;
        CPLPopErrorHandler();

        if (bSuccess)
        {
            VSIFTruncateL(m_fpNodes, 0);
        }
        else
        {
            CPLDebug("OSM", "Not enough memory for in-memory file. "
                            "Using disk temporary file instead.");

            VSIFCloseL(m_fpNodes);
            m_fpNodes = nullptr;
            VSIUnlink(m_osNodesFilename);

            m_bInMemoryNodesFile = false;
            m_osNodesFilename = CPLGenerateTempFilenameSafe("osm_tmp_nodes");

            m_fpNodes = VSIFOpenL(m_osNodesFilename, "wb+");
            if (m_fpNodes == nullptr)
            {
                return FALSE;
            }

            /* On Unix filesystems, you can remove a file even if it */
            /* opened */
            const char *pszVal =
                CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if (EQUAL(pszVal, "YES"))
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                m_bMustUnlinkNodesFile = VSIUnlink(m_osNodesFilename) != 0;
                CPLPopErrorHandler();
            }

            return FALSE;
        }
    }

    const bool bRet = CreateTempDB();
    if (bRet)
    {
        CPLString osInterestLayers =
            GetInterestLayersForDSName(GetDescription());
        if (!osInterestLayers.empty())
        {
            ReleaseResultSet(ExecuteSQL(osInterestLayers, nullptr, nullptr));
        }
    }
    return bRet;
}

/************************************************************************/
/*                             CreateTempDB()                           */
/************************************************************************/

bool OGROSMDataSource::CreateTempDB()
{
    char *pszErrMsg = nullptr;

    int rc = 0;
    bool bIsExisting = false;
    bool bSuccess = false;

    const char *pszExistingTmpFile =
        CPLGetConfigOption("OSM_EXISTING_TMPFILE", nullptr);
    if (pszExistingTmpFile != nullptr)
    {
        bSuccess = true;
        bIsExisting = true;
        rc = sqlite3_open_v2(pszExistingTmpFile, &m_hDB,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX,
                             nullptr);
    }
    else
    {
        m_osTmpDBName = VSIMemGenerateHiddenFilename("osm_temp.sqlite");

        // On 32 bit, the virtual memory space is scarce, so we need to
        // reserve it right now. Will not hurt on 64 bit either.
        VSILFILE *fp = VSIFOpenL(m_osTmpDBName, "wb");
        if (fp)
        {
            vsi_l_offset nSize =
                static_cast<vsi_l_offset>(m_nMaxSizeForInMemoryDBInMB) * 1024 *
                1024;
            if (m_bCustomIndexing && m_bInMemoryNodesFile)
                nSize = nSize / 4;

            CPLPushErrorHandler(CPLQuietErrorHandler);
            bSuccess = VSIFTruncateL(fp, nSize) == 0;
            CPLPopErrorHandler();

            if (bSuccess)
                bSuccess = VSIFTruncateL(fp, 0) == 0;

            VSIFCloseL(fp);

            if (!bSuccess)
            {
                CPLDebug("OSM", "Not enough memory for in-memory file. "
                                "Using disk temporary file instead.");
                VSIUnlink(m_osTmpDBName);
            }
        }

        if (bSuccess)
        {
            m_bInMemoryTmpDB = true;
            m_pMyVFS = OGRSQLiteCreateVFS(nullptr, this);
            sqlite3_vfs_register(m_pMyVFS, 0);
            rc = sqlite3_open_v2(m_osTmpDBName.c_str(), &m_hDB,
                                 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                     SQLITE_OPEN_NOMUTEX,
                                 m_pMyVFS->zName);
        }
    }

    if (!bSuccess)
    {
        m_osTmpDBName = CPLGenerateTempFilenameSafe("osm_tmp");
        rc = sqlite3_open(m_osTmpDBName.c_str(), &m_hDB);

        /* On Unix filesystems, you can remove a file even if it */
        /* opened */
        if (rc == SQLITE_OK)
        {
            const char *pszVal =
                CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if (EQUAL(pszVal, "YES"))
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                m_bMustUnlink = VSIUnlink(m_osTmpDBName) != 0;
                CPLPopErrorHandler();
            }
        }
    }

    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "sqlite3_open(%s) failed: %s",
                 m_osTmpDBName.c_str(), sqlite3_errmsg(m_hDB));
        return false;
    }

    if (!SetDBOptions())
    {
        return false;
    }

    if (!bIsExisting)
    {
        rc = sqlite3_exec(
            m_hDB, "CREATE TABLE nodes (id INTEGER PRIMARY KEY, coords BLOB)",
            nullptr, nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create table nodes : %s", pszErrMsg);
            sqlite3_free(pszErrMsg);
            return false;
        }

        rc = sqlite3_exec(
            m_hDB, "CREATE TABLE ways (id INTEGER PRIMARY KEY, data BLOB)",
            nullptr, nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create table ways : %s", pszErrMsg);
            sqlite3_free(pszErrMsg);
            return false;
        }

        rc = sqlite3_exec(
            m_hDB, "CREATE TABLE polygons_standalone (id INTEGER PRIMARY KEY)",
            nullptr, nullptr, &pszErrMsg);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create table polygons_standalone : %s",
                     pszErrMsg);
            sqlite3_free(pszErrMsg);
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
    char *pszErrMsg = nullptr;
    int rc = sqlite3_exec(m_hDB, "PRAGMA synchronous = OFF", nullptr, nullptr,
                          &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to run PRAGMA synchronous : %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return false;
    }

    rc = sqlite3_exec(m_hDB, "PRAGMA journal_mode = OFF", nullptr, nullptr,
                      &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to run PRAGMA journal_mode : %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return false;
    }

    rc = sqlite3_exec(m_hDB, "PRAGMA temp_store = MEMORY", nullptr, nullptr,
                      &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to run PRAGMA temp_store : %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return false;
    }

    SetCacheSize();

    if (!StartTransactionCacheDB())
        return false;

    return true;
}

/************************************************************************/
/*                              SetCacheSize()                          */
/************************************************************************/

void OGROSMDataSource::SetCacheSize()
{
    const char *pszSqliteCacheMB =
        CPLGetConfigOption("OSM_SQLITE_CACHE", nullptr);

    if (pszSqliteCacheMB == nullptr)
        return;

    char *pszErrMsg = nullptr;
    char **papszResult = nullptr;
    int nRowCount = 0;
    int nColCount = 0;
    int iSqlitePageSize = -1;
    const GIntBig iSqliteCacheBytes =
        static_cast<GIntBig>(atoi(pszSqliteCacheMB)) * 1024 * 1024;

    /* querying the current PageSize */
    int rc = sqlite3_get_table(m_hDB, "PRAGMA page_size", &papszResult,
                               &nRowCount, &nColCount, &pszErrMsg);
    if (rc == SQLITE_OK)
    {
        for (int iRow = 1; iRow <= nRowCount; iRow++)
        {
            iSqlitePageSize = atoi(papszResult[(iRow * nColCount) + 0]);
        }
        sqlite3_free_table(papszResult);
    }
    if (iSqlitePageSize < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to run PRAGMA page_size : %s",
                 pszErrMsg ? pszErrMsg : sqlite3_errmsg(m_hDB));
        sqlite3_free(pszErrMsg);
        return;
    }
    if (iSqlitePageSize == 0)
        return;

    /* computing the CacheSize as #Pages */
    const int iSqliteCachePages =
        static_cast<int>(iSqliteCacheBytes / iSqlitePageSize);
    if (iSqliteCachePages <= 0)
        return;

    rc = sqlite3_exec(m_hDB,
                      CPLSPrintf("PRAGMA cache_size = %d", iSqliteCachePages),
                      nullptr, nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unrecognized value for PRAGMA cache_size : %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
    }
}

/************************************************************************/
/*                        CreatePreparedStatements()                    */
/************************************************************************/

bool OGROSMDataSource::CreatePreparedStatements()
{
    int rc =
        sqlite3_prepare_v2(m_hDB, "INSERT INTO nodes (id, coords) VALUES (?,?)",
                           -1, &m_hInsertNodeStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(m_hDB));
        return false;
    }

    m_pahSelectNodeStmt = static_cast<sqlite3_stmt **>(
        CPLCalloc(sizeof(sqlite3_stmt *), LIMIT_IDS_PER_REQUEST));

    char szTmp[LIMIT_IDS_PER_REQUEST * 2 + 128];
    strcpy(szTmp, "SELECT id, coords FROM nodes WHERE id IN (");
    int nLen = static_cast<int>(strlen(szTmp));
    for (int i = 0; i < LIMIT_IDS_PER_REQUEST; i++)
    {
        if (i == 0)
        {
            strcpy(szTmp + nLen, "?) ORDER BY id ASC");
            nLen += 2;
        }
        else
        {
            strcpy(szTmp + nLen - 1, ",?) ORDER BY id ASC");
            nLen += 2;
        }
        rc = sqlite3_prepare_v2(m_hDB, szTmp, -1, &m_pahSelectNodeStmt[i],
                                nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "sqlite3_prepare_v2() failed :  %s",
                     sqlite3_errmsg(m_hDB));
            return false;
        }
    }

    rc = sqlite3_prepare_v2(m_hDB, "INSERT INTO ways (id, data) VALUES (?,?)",
                            -1, &m_hInsertWayStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(m_hDB));
        return false;
    }

    m_pahSelectWayStmt = static_cast<sqlite3_stmt **>(
        CPLCalloc(sizeof(sqlite3_stmt *), LIMIT_IDS_PER_REQUEST));

    strcpy(szTmp, "SELECT id, data FROM ways WHERE id IN (");
    nLen = static_cast<int>(strlen(szTmp));
    for (int i = 0; i < LIMIT_IDS_PER_REQUEST; i++)
    {
        if (i == 0)
        {
            strcpy(szTmp + nLen, "?)");
            nLen += 2;
        }
        else
        {
            strcpy(szTmp + nLen - 1, ",?)");
            nLen += 2;
        }
        rc = sqlite3_prepare_v2(m_hDB, szTmp, -1, &m_pahSelectWayStmt[i],
                                nullptr);
        if (rc != SQLITE_OK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "sqlite3_prepare_v2() failed :  %s",
                     sqlite3_errmsg(m_hDB));
            return false;
        }
    }

    rc = sqlite3_prepare_v2(m_hDB,
                            "INSERT INTO polygons_standalone (id) VALUES (?)",
                            -1, &m_hInsertPolygonsStandaloneStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(m_hDB));
        return false;
    }

    rc = sqlite3_prepare_v2(m_hDB,
                            "DELETE FROM polygons_standalone WHERE id = ?", -1,
                            &m_hDeletePolygonsStandaloneStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(m_hDB));
        return false;
    }

    rc = sqlite3_prepare_v2(m_hDB,
                            "SELECT id FROM polygons_standalone ORDER BY id",
                            -1, &m_hSelectPolygonsStandaloneStmt, nullptr);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "sqlite3_prepare_v2() failed :  %s", sqlite3_errmsg(m_hDB));
        return false;
    }

    return true;
}

/************************************************************************/
/*                      StartTransactionCacheDB()                       */
/************************************************************************/

bool OGROSMDataSource::StartTransactionCacheDB()
{
    if (m_bInTransaction)
        return false;

    char *pszErrMsg = nullptr;
    int rc = sqlite3_exec(m_hDB, "BEGIN", nullptr, nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to start transaction : %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return false;
    }

    m_bInTransaction = true;

    return true;
}

/************************************************************************/
/*                        CommitTransactionCacheDB()                    */
/************************************************************************/

bool OGROSMDataSource::CommitTransactionCacheDB()
{
    if (!m_bInTransaction)
        return false;

    m_bInTransaction = false;

    char *pszErrMsg = nullptr;
    int rc = sqlite3_exec(m_hDB, "COMMIT", nullptr, nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to commit transaction : %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return false;
    }

    return true;
}

/************************************************************************/
/*                     AddComputedAttributes()                          */
/************************************************************************/

void OGROSMDataSource::AddComputedAttributes(
    int iCurLayer, const std::vector<OGROSMComputedAttribute> &oAttributes)
{
    for (const auto &oAttribute : oAttributes)
    {
        if (!oAttribute.osSQL.empty())
        {
            m_apoLayers[iCurLayer]->AddComputedAttribute(
                oAttribute.osName, oAttribute.eType, oAttribute.osSQL);
        }
    }
}

/************************************************************************/
/*                           ParseConf()                                */
/************************************************************************/

bool OGROSMDataSource::ParseConf(CSLConstList papszOpenOptionsIn)
{
    VSILFILE *fpConf = nullptr;

    const char *pszFilename =
        CSLFetchNameValueDef(papszOpenOptionsIn, "CONFIG_FILE",
                             CPLGetConfigOption("OSM_CONFIG_FILE", nullptr));
    if (pszFilename == nullptr)
    {
#if !defined(USE_ONLY_EMBEDDED_RESOURCE_FILES)
        pszFilename = CPLFindFile("gdal", "osmconf.ini");
#endif
#ifdef EMBED_RESOURCE_FILES
        if (!pszFilename || EQUAL(pszFilename, "osmconf.ini"))
        {
            static const bool bOnce [[maybe_unused]] = []()
            {
                CPLDebug("OSM", "Using embedded osmconf.ini");
                return true;
            }();
            fpConf = VSIFileFromMemBuffer(
                nullptr,
                const_cast<GByte *>(
                    reinterpret_cast<const GByte *>(OSMGetOSMConfIni())),
                static_cast<int>(strlen(OSMGetOSMConfIni())),
                /* bTakeOwnership = */ false);
        }
#else
        if (!pszFilename)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot find osmconf.ini configuration file");
            return false;
        }
#endif
    }

    if (pszFilename)
        m_osConfigFile = pszFilename;

#if defined(EMBED_RESOURCE_FILES)
    if (!fpConf)
#endif
    {
        fpConf = VSIFOpenL(pszFilename, "rb");
        if (fpConf == nullptr)
            return false;
    }

    const char *pszLine = nullptr;
    int iCurLayer = -1;
    std::vector<OGROSMComputedAttribute> oAttributes;

    while ((pszLine = CPLReadLine2L(fpConf, -1, nullptr)) != nullptr)
    {
        if (pszLine[0] == '#')
            continue;
        if (pszLine[0] == '[' && pszLine[strlen(pszLine) - 1] == ']')
        {
            if (iCurLayer >= 0)
                AddComputedAttributes(iCurLayer, oAttributes);
            oAttributes.resize(0);

            iCurLayer = -1;
            pszLine++;
            const_cast<char *>(pszLine)[strlen(pszLine) - 1] =
                '\0'; /* Evil but OK */

            if (strcmp(pszLine, "general") == 0)
            {
                continue;
            }

            int i = 0;
            for (auto &&poLayer : m_apoLayers)
            {
                if (strcmp(pszLine, poLayer->GetName()) == 0)
                {
                    iCurLayer = i;
                    break;
                }
                ++i;
            }
            if (iCurLayer < 0)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer '%s' mentioned in %s is unknown to the driver",
                         pszLine, pszFilename);
            }
            continue;
        }

        if (STARTS_WITH(pszLine, "closed_ways_are_polygons="))
        {
            char **papszTokens2 = CSLTokenizeString2(
                pszLine + strlen("closed_ways_are_polygons="), ",", 0);
            m_nMinSizeKeysInSetClosedWaysArePolygons = INT_MAX;
            m_nMaxSizeKeysInSetClosedWaysArePolygons = 0;
            for (int i = 0; papszTokens2[i] != nullptr; i++)
            {
                const int nTokenSize =
                    static_cast<int>(strlen(papszTokens2[i]));
                aoSetClosedWaysArePolygons.insert(papszTokens2[i]);
                m_nMinSizeKeysInSetClosedWaysArePolygons = std::min(
                    m_nMinSizeKeysInSetClosedWaysArePolygons, nTokenSize);
                m_nMaxSizeKeysInSetClosedWaysArePolygons = std::max(
                    m_nMaxSizeKeysInSetClosedWaysArePolygons, nTokenSize);
            }
            CSLDestroy(papszTokens2);
        }

        else if (STARTS_WITH(pszLine, "report_all_tags="))
        {
            if (strcmp(pszLine + strlen("report_all_tags="), "yes") == 0)
            {
                std::fill(begin(m_ignoredKeys), end(m_ignoredKeys), "");
            }
        }

        else if (STARTS_WITH(pszLine, "report_all_nodes="))
        {
            if (strcmp(pszLine + strlen("report_all_nodes="), "no") == 0)
            {
                m_bReportAllNodes = false;
            }
            else if (strcmp(pszLine + strlen("report_all_nodes="), "yes") == 0)
            {
                m_bReportAllNodes = true;
            }
        }

        else if (STARTS_WITH(pszLine, "report_all_ways="))
        {
            if (strcmp(pszLine + strlen("report_all_ways="), "no") == 0)
            {
                m_bReportAllWays = false;
            }
            else if (strcmp(pszLine + strlen("report_all_ways="), "yes") == 0)
            {
                m_bReportAllWays = true;
            }
        }

        else if (STARTS_WITH(pszLine, "attribute_name_laundering="))
        {
            if (strcmp(pszLine + strlen("attribute_name_laundering="), "no") ==
                0)
            {
                m_bAttributeNameLaundering = false;
            }
            else if (strcmp(pszLine + strlen("attribute_name_laundering="),
                            "yes") == 0)
            {
                m_bAttributeNameLaundering = true;
            }
        }

        else if (STARTS_WITH(pszLine, "tags_format="))
        {
            if (EQUAL(pszLine + strlen("tags_format="), "json"))
            {
                m_bTagsAsHSTORE = false;
            }
            else if (EQUAL(pszLine + strlen("tags_format="), "hstore"))
            {
                m_bTagsAsHSTORE = true;
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Unsupported value for tags_format: %s",
                         pszLine + strlen("tags_format="));
            }
        }

        else if (iCurLayer >= 0)
        {
            char **papszTokens = CSLTokenizeString2(pszLine, "=", 0);
            if (CSLCount(papszTokens) == 2 &&
                strcmp(papszTokens[0], "other_tags") == 0)
            {
                if (strcmp(papszTokens[1], "no") == 0)
                    m_apoLayers[iCurLayer]->SetHasOtherTags(false);
                else if (strcmp(papszTokens[1], "yes") == 0)
                    m_apoLayers[iCurLayer]->SetHasOtherTags(true);
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "all_tags") == 0)
            {
                if (strcmp(papszTokens[1], "no") == 0)
                    m_apoLayers[iCurLayer]->SetHasAllTags(false);
                else if (strcmp(papszTokens[1], "yes") == 0)
                    m_apoLayers[iCurLayer]->SetHasAllTags(true);
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "osm_id") == 0)
            {
                if (strcmp(papszTokens[1], "no") == 0)
                    m_apoLayers[iCurLayer]->SetHasOSMId(false);
                else if (strcmp(papszTokens[1], "yes") == 0)
                {
                    m_apoLayers[iCurLayer]->SetHasOSMId(true);
                    m_apoLayers[iCurLayer]->AddField("osm_id", OFTString);

                    if (iCurLayer == IDX_LYR_MULTIPOLYGONS)
                        m_apoLayers[iCurLayer]->AddField("osm_way_id",
                                                         OFTString);
                }
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "osm_version") == 0)
            {
                if (strcmp(papszTokens[1], "no") == 0)
                    m_apoLayers[iCurLayer]->SetHasVersion(false);
                else if (strcmp(papszTokens[1], "yes") == 0)
                {
                    m_apoLayers[iCurLayer]->SetHasVersion(true);
                    m_apoLayers[iCurLayer]->AddField("osm_version", OFTInteger);
                }
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "osm_timestamp") == 0)
            {
                if (strcmp(papszTokens[1], "no") == 0)
                    m_apoLayers[iCurLayer]->SetHasTimestamp(false);
                else if (strcmp(papszTokens[1], "yes") == 0)
                {
                    m_apoLayers[iCurLayer]->SetHasTimestamp(true);
                    m_apoLayers[iCurLayer]->AddField("osm_timestamp",
                                                     OFTDateTime);
                }
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "osm_uid") == 0)
            {
                if (strcmp(papszTokens[1], "no") == 0)
                    m_apoLayers[iCurLayer]->SetHasUID(false);
                else if (strcmp(papszTokens[1], "yes") == 0)
                {
                    m_apoLayers[iCurLayer]->SetHasUID(true);
                    m_apoLayers[iCurLayer]->AddField("osm_uid", OFTInteger);
                }
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "osm_user") == 0)
            {
                if (strcmp(papszTokens[1], "no") == 0)
                    m_apoLayers[iCurLayer]->SetHasUser(false);
                else if (strcmp(papszTokens[1], "yes") == 0)
                {
                    m_apoLayers[iCurLayer]->SetHasUser(true);
                    m_apoLayers[iCurLayer]->AddField("osm_user", OFTString);
                }
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "osm_changeset") == 0)
            {
                if (strcmp(papszTokens[1], "no") == 0)
                    m_apoLayers[iCurLayer]->SetHasChangeset(false);
                else if (strcmp(papszTokens[1], "yes") == 0)
                {
                    m_apoLayers[iCurLayer]->SetHasChangeset(true);
                    m_apoLayers[iCurLayer]->AddField("osm_changeset",
                                                     OFTInteger);
                }
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "attributes") == 0)
            {
                char **papszTokens2 =
                    CSLTokenizeString2(papszTokens[1], ",", 0);
                for (int i = 0; papszTokens2[i] != nullptr; i++)
                {
                    m_apoLayers[iCurLayer]->AddField(papszTokens2[i],
                                                     OFTString);
                    for (const char *&pszIgnoredKey : m_ignoredKeys)
                    {
                        if (strcmp(papszTokens2[i], pszIgnoredKey) == 0)
                            pszIgnoredKey = "";
                    }
                }
                CSLDestroy(papszTokens2);
            }
            else if (CSLCount(papszTokens) == 2 &&
                     (strcmp(papszTokens[0], "unsignificant") == 0 ||
                      strcmp(papszTokens[0], "insignificant") == 0))
            {
                char **papszTokens2 =
                    CSLTokenizeString2(papszTokens[1], ",", 0);
                for (int i = 0; papszTokens2[i] != nullptr; i++)
                {
                    m_apoLayers[iCurLayer]->AddInsignificantKey(
                        papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "ignore") == 0)
            {
                char **papszTokens2 =
                    CSLTokenizeString2(papszTokens[1], ",", 0);
                for (int i = 0; papszTokens2[i] != nullptr; i++)
                {
                    m_apoLayers[iCurLayer]->AddIgnoreKey(papszTokens2[i]);
                    m_apoLayers[iCurLayer]->AddWarnKey(papszTokens2[i]);
                }
                CSLDestroy(papszTokens2);
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strcmp(papszTokens[0], "computed_attributes") == 0)
            {
                char **papszTokens2 =
                    CSLTokenizeString2(papszTokens[1], ",", 0);
                oAttributes.resize(0);
                for (int i = 0; papszTokens2[i] != nullptr; i++)
                {
                    oAttributes.push_back(
                        OGROSMComputedAttribute(papszTokens2[i]));
                }
                CSLDestroy(papszTokens2);
            }
            else if (CSLCount(papszTokens) == 2 &&
                     strlen(papszTokens[0]) >= 5 &&
                     strcmp(papszTokens[0] + strlen(papszTokens[0]) - 5,
                            "_type") == 0)
            {
                CPLString osName(papszTokens[0]);
                osName.resize(strlen(papszTokens[0]) - 5);
                const char *pszType = papszTokens[1];
                bool bFound = false;
                OGRFieldType eType = OFTString;
                if (EQUAL(pszType, "Integer"))
                    eType = OFTInteger;
                else if (EQUAL(pszType, "Integer64"))
                    eType = OFTInteger64;
                else if (EQUAL(pszType, "Real"))
                    eType = OFTReal;
                else if (EQUAL(pszType, "String"))
                    eType = OFTString;
                else if (EQUAL(pszType, "DateTime"))
                    eType = OFTDateTime;
                else
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unhandled type (%s) for attribute %s", pszType,
                             osName.c_str());
                for (size_t i = 0; i < oAttributes.size(); i++)
                {
                    if (oAttributes[i].osName == osName)
                    {
                        bFound = true;
                        oAttributes[i].eType = eType;
                        break;
                    }
                }
                if (!bFound)
                {
                    const int idx =
                        m_apoLayers[iCurLayer]->GetLayerDefn()->GetFieldIndex(
                            osName);
                    if (idx >= 0)
                    {
                        m_apoLayers[iCurLayer]
                            ->GetLayerDefn()
                            ->GetFieldDefn(idx)
                            ->SetType(eType);
                        bFound = true;
                    }
                }
                if (!bFound)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Undeclared attribute : %s", osName.c_str());
                }
            }
            else if (CSLCount(papszTokens) >= 2 &&
                     strlen(papszTokens[0]) >= 4 &&
                     strcmp(papszTokens[0] + strlen(papszTokens[0]) - 4,
                            "_sql") == 0)
            {
                CPLString osName(papszTokens[0]);
                osName.resize(strlen(papszTokens[0]) - 4);
                size_t i = 0;  // Used after for.
                for (; i < oAttributes.size(); i++)
                {
                    if (oAttributes[i].osName == osName)
                    {
                        const char *pszSQL = strchr(pszLine, '=') + 1;
                        while (*pszSQL == ' ')
                            pszSQL++;
                        bool bInQuotes = false;
                        if (*pszSQL == '"')
                        {
                            bInQuotes = true;
                            pszSQL++;
                        }
                        oAttributes[i].osSQL = pszSQL;
                        if (bInQuotes && oAttributes[i].osSQL.size() > 1 &&
                            oAttributes[i].osSQL.back() == '"')
                            oAttributes[i].osSQL.resize(
                                oAttributes[i].osSQL.size() - 1);
                        break;
                    }
                }
                if (i == oAttributes.size())
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Undeclared attribute : %s", osName.c_str());
                }
            }
            CSLDestroy(papszTokens);
        }
    }

    if (iCurLayer >= 0)
        AddComputedAttributes(iCurLayer, oAttributes);

    VSIFCloseL(fpConf);

    return true;
}

/************************************************************************/
/*                          MyResetReading()                            */
/************************************************************************/

int OGROSMDataSource::MyResetReading()
{
    if (m_hDB == nullptr)
        return FALSE;
    if (m_bCustomIndexing && m_fpNodes == nullptr)
        return FALSE;

    OSM_ResetReading(m_psParser);

    char *pszErrMsg = nullptr;
    int rc =
        sqlite3_exec(m_hDB, "DELETE FROM nodes", nullptr, nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to DELETE FROM nodes : %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return FALSE;
    }

    rc = sqlite3_exec(m_hDB, "DELETE FROM ways", nullptr, nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unable to DELETE FROM ways : %s",
                 pszErrMsg);
        sqlite3_free(pszErrMsg);
        return FALSE;
    }

    rc = sqlite3_exec(m_hDB, "DELETE FROM polygons_standalone", nullptr,
                      nullptr, &pszErrMsg);
    if (rc != SQLITE_OK)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to DELETE FROM polygons_standalone : %s", pszErrMsg);
        sqlite3_free(pszErrMsg);
        return FALSE;
    }
    m_bHasRowInPolygonsStandalone = false;

    if (m_hSelectPolygonsStandaloneStmt != nullptr)
        sqlite3_reset(m_hSelectPolygonsStandaloneStmt);

    {
        m_asWayFeaturePairs.clear();
        m_nUnsortedReqIds = 0;
        m_nReqIds = 0;
        m_nAccumulatedTags = 0;
        nNonRedundantKeysLen = 0;
        nNonRedundantValuesLen = 0;

        for (KeyDesc *psKD : m_apsKeys)
        {
            if (psKD)
            {
                CPLFree(psKD->pszK);
                for (auto *pszValue : psKD->apszValues)
                    CPLFree(pszValue);
                delete psKD;
            }
        }
        m_apsKeys.resize(1);  // keep guard to avoid index 0 to be used
        m_aoMapIndexedKeys.clear();
    }

    if (m_bCustomIndexing)
    {
        m_nPrevNodeId = -1;
        m_nBucketOld = -1;
        m_nOffInBucketReducedOld = -1;

        VSIFSeekL(m_fpNodes, 0, SEEK_SET);
        VSIFTruncateL(m_fpNodes, 0);
        m_nNodesFileSize = 0;

        memset(m_pabySector, 0, SECTOR_SIZE);

        for (auto &oIter : m_oMapBuckets)
        {
            Bucket &sBucket = oIter.second;
            sBucket.nOff = -1;
            if (m_bCompressNodes)
            {
                if (sBucket.u.panSectorSize)
                    memset(sBucket.u.panSectorSize, 0,
                           BUCKET_SECTOR_SIZE_ARRAY_SIZE);
            }
            else
            {
                if (sBucket.u.pabyBitmap)
                    memset(sBucket.u.pabyBitmap, 0, BUCKET_BITMAP_SIZE);
            }
        }
    }

    for (auto &&poLayer : m_apoLayers)
    {
        poLayer->ForceResetReading();
    }

    m_bStopParsing = false;
    m_poCurrentLayer = nullptr;

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

OGRFeature *OGROSMDataSource::GetNextFeature(OGRLayer **ppoBelongingLayer,
                                             double *pdfProgressPct,
                                             GDALProgressFunc pfnProgress,
                                             void *pProgressData)
{
    m_bInterleavedReading = TRUE;

    if (m_poCurrentLayer == nullptr)
    {
        m_poCurrentLayer = m_apoLayers[0].get();
    }
    if (pdfProgressPct != nullptr || pfnProgress != nullptr)
    {
        if (m_nFileSize == FILESIZE_NOT_INIT)
        {
            VSIStatBufL sStat;
            if (VSIStatL(GetDescription(), &sStat) == 0)
            {
                m_nFileSize = static_cast<GIntBig>(sStat.st_size);
            }
            else
            {
                m_nFileSize = FILESIZE_INVALID;
            }
        }
    }

    while (true)
    {
        OGROSMLayer *poNewCurLayer = nullptr;
        CPLAssert(m_poCurrentLayer != nullptr);
        OGRFeature *poFeature = m_poCurrentLayer->MyGetNextFeature(
            &poNewCurLayer, pfnProgress, pProgressData);
        m_poCurrentLayer = poNewCurLayer;
        if (poFeature == nullptr)
        {
            if (m_poCurrentLayer != nullptr)
                continue;
            if (ppoBelongingLayer != nullptr)
                *ppoBelongingLayer = nullptr;
            if (pdfProgressPct != nullptr)
                *pdfProgressPct = 1.0;
            return nullptr;
        }
        if (ppoBelongingLayer != nullptr)
            *ppoBelongingLayer = m_poCurrentLayer;
        if (pdfProgressPct != nullptr)
        {
            if (m_nFileSize != FILESIZE_INVALID)
            {
                *pdfProgressPct =
                    1.0 * OSM_GetBytesRead(m_psParser) / m_nFileSize;
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

bool OGROSMDataSource::ParseNextChunk(int nIdxLayer,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    if (m_bStopParsing)
        return false;

    m_bHasParsedFirstChunk = true;
    m_bFeatureAdded = false;
    while (true)
    {
#ifdef DEBUG_MEM_USAGE
        static int counter = 0;
        counter++;
        if ((counter % 1000) == 0)
            CPLDebug("OSM", "GetMaxTotalAllocs() = " CPL_FRMT_GUIB,
                     static_cast<GUIntBig>(GetMaxTotalAllocs()));
#endif

        OSMRetCode eRet = OSM_ProcessBlock(m_psParser);
        if (pfnProgress != nullptr)
        {
            double dfPct = -1.0;
            if (m_nFileSize != FILESIZE_INVALID)
            {
                dfPct = 1.0 * OSM_GetBytesRead(m_psParser) / m_nFileSize;
            }
            if (!pfnProgress(dfPct, "", pProgressData))
            {
                m_bStopParsing = true;
                for (auto &&poLayer : m_apoLayers)
                {
                    poLayer->ForceResetReading();
                }
                return false;
            }
        }

        if (eRet == OSM_EOF || eRet == OSM_ERROR)
        {
            if (eRet == OSM_EOF)
            {
                if (!m_asWayFeaturePairs.empty())
                    ProcessWaysBatch();

                ProcessPolygonsStandalone();

                if (!m_bHasRowInPolygonsStandalone)
                    m_bStopParsing = true;

                if (!m_bInterleavedReading && !m_bFeatureAdded &&
                    m_bHasRowInPolygonsStandalone &&
                    nIdxLayer != IDX_LYR_MULTIPOLYGONS)
                {
                    return false;
                }

                return m_bFeatureAdded || m_bHasRowInPolygonsStandalone;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "An error occurred during the parsing of data "
                         "around byte " CPL_FRMT_GUIB,
                         OSM_GetBytesRead(m_psParser));

                m_bStopParsing = true;
                return false;
            }
        }
        else
        {
            if (m_bInMemoryTmpDB)
            {
                if (!TransferToDiskIfNecesserary())
                    return false;
            }

            if (m_bFeatureAdded)
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
    if (m_bInMemoryNodesFile)
    {
        if (m_nNodesFileSize / 1024 / 1024 >
            3 * m_nMaxSizeForInMemoryDBInMB / 4)
        {
            m_bInMemoryNodesFile = false;

            VSIFCloseL(m_fpNodes);
            m_fpNodes = nullptr;

            const std::string osNewTmpDBName(
                CPLGenerateTempFilenameSafe("osm_tmp_nodes"));

            CPLDebug("OSM",
                     "%s too big for RAM. Transferring it onto disk in %s",
                     m_osNodesFilename.c_str(), osNewTmpDBName.c_str());

            if (CPLCopyFile(osNewTmpDBName.c_str(), m_osNodesFilename) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot copy %s to %s",
                         m_osNodesFilename.c_str(), osNewTmpDBName.c_str());
                VSIUnlink(osNewTmpDBName.c_str());
                m_bStopParsing = true;
                return false;
            }

            VSIUnlink(m_osNodesFilename);

            if (m_bInMemoryTmpDB)
            {
                /* Try to grow the sqlite in memory-db to the full space now */
                /* it has been freed. */
                VSILFILE *fp = VSIFOpenL(m_osTmpDBName, "rb+");
                if (fp)
                {
                    VSIFSeekL(fp, 0, SEEK_END);
                    vsi_l_offset nCurSize = VSIFTellL(fp);
                    vsi_l_offset nNewSize =
                        static_cast<vsi_l_offset>(m_nMaxSizeForInMemoryDBInMB) *
                        1024 * 1024;
                    CPLPushErrorHandler(CPLQuietErrorHandler);
                    const bool bSuccess =
                        VSIFSeekL(fp, nNewSize, SEEK_SET) == 0;
                    CPLPopErrorHandler();

                    if (bSuccess)
                        VSIFTruncateL(fp, nCurSize);

                    VSIFCloseL(fp);
                }
            }

            m_osNodesFilename = osNewTmpDBName;

            m_fpNodes = VSIFOpenL(m_osNodesFilename, "rb+");
            if (m_fpNodes == nullptr)
            {
                m_bStopParsing = true;
                return false;
            }

            VSIFSeekL(m_fpNodes, 0, SEEK_END);

            /* On Unix filesystems, you can remove a file even if it */
            /* opened */
            const char *pszVal =
                CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if (EQUAL(pszVal, "YES"))
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                m_bMustUnlinkNodesFile = VSIUnlink(m_osNodesFilename) != 0;
                CPLPopErrorHandler();
            }
        }
    }

    if (m_bInMemoryTmpDB)
    {
        VSIStatBufL sStat;

        int nLimitMB = m_nMaxSizeForInMemoryDBInMB;
        if (m_bCustomIndexing && m_bInMemoryNodesFile)
            nLimitMB = nLimitMB * 1 / 4;

        if (VSIStatL(m_osTmpDBName, &sStat) == 0 &&
            sStat.st_size / 1024 / 1024 > nLimitMB)
        {
            m_bInMemoryTmpDB = false;

            CloseDB();

            const std::string osNewTmpDBName(
                CPLGenerateTempFilenameSafe("osm_tmp"));

            CPLDebug("OSM",
                     "%s too big for RAM. Transferring it onto disk in %s",
                     m_osTmpDBName.c_str(), osNewTmpDBName.c_str());

            if (CPLCopyFile(osNewTmpDBName.c_str(), m_osTmpDBName) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot copy %s to %s",
                         m_osTmpDBName.c_str(), osNewTmpDBName.c_str());
                VSIUnlink(osNewTmpDBName.c_str());
                m_bStopParsing = true;
                return false;
            }

            VSIUnlink(m_osTmpDBName);

            m_osTmpDBName = osNewTmpDBName;

            const int rc = sqlite3_open_v2(
                m_osTmpDBName.c_str(), &m_hDB,
                SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, nullptr);
            if (rc != SQLITE_OK)
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                         "sqlite3_open(%s) failed: %s", m_osTmpDBName.c_str(),
                         sqlite3_errmsg(m_hDB));
                m_bStopParsing = true;
                CloseDB();
                return false;
            }

            /* On Unix filesystems, you can remove a file even if it */
            /* opened */
            const char *pszVal =
                CPLGetConfigOption("OSM_UNLINK_TMPFILE", "YES");
            if (EQUAL(pszVal, "YES"))
            {
                CPLPushErrorHandler(CPLQuietErrorHandler);
                m_bMustUnlink = VSIUnlink(m_osTmpDBName) != 0;
                CPLPopErrorHandler();
            }

            if (!SetDBOptions() || !CreatePreparedStatements())
            {
                m_bStopParsing = true;
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

int OGROSMDataSource::TestCapability(const char *pszCap)
{
    return EQUAL(pszCap, ODsCRandomLayerRead);
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGROSMDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || static_cast<size_t>(iLayer) >= m_apoLayers.size())
        return nullptr;

    return m_apoLayers[iLayer].get();
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGROSMDataSource::GetExtent(OGREnvelope *psExtent)
{
    if (!m_bHasParsedFirstChunk)
    {
        m_bHasParsedFirstChunk = true;
        OSM_ProcessBlock(m_psParser);
    }

    if (m_bExtentValid)
    {
        *psExtent = m_sExtent;
        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                   OGROSMSingleFeatureLayer                           */
/************************************************************************/

class OGROSMSingleFeatureLayer final : public OGRLayer
{
  private:
    int nVal;
    char *pszVal;
    OGRFeatureDefn *poFeatureDefn;
    int iNextShapeId;

    OGROSMSingleFeatureLayer(const OGROSMSingleFeatureLayer &) = delete;
    OGROSMSingleFeatureLayer &
    operator=(const OGROSMSingleFeatureLayer &) = delete;

  public:
    OGROSMSingleFeatureLayer(const char *pszLayerName, int nVal);
    OGROSMSingleFeatureLayer(const char *pszLayerName, const char *pszVal);
    virtual ~OGROSMSingleFeatureLayer();

    virtual void ResetReading() override
    {
        iNextShapeId = 0;
    }

    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual int TestCapability(const char *) override
    {
        return FALSE;
    }
};

/************************************************************************/
/*                    OGROSMSingleFeatureLayer()                        */
/************************************************************************/

OGROSMSingleFeatureLayer::OGROSMSingleFeatureLayer(const char *pszLayerName,
                                                   int nValIn)
    : nVal(nValIn), pszVal(nullptr),
      poFeatureDefn(new OGRFeatureDefn("SELECT")), iNextShapeId(0)
{
    poFeatureDefn->Reference();
    OGRFieldDefn oField(pszLayerName, OFTInteger);
    poFeatureDefn->AddFieldDefn(&oField);
}

/************************************************************************/
/*                    OGROSMSingleFeatureLayer()                        */
/************************************************************************/

OGROSMSingleFeatureLayer::OGROSMSingleFeatureLayer(const char *pszLayerName,
                                                   const char *pszValIn)
    : nVal(0), pszVal(CPLStrdup(pszValIn)),
      poFeatureDefn(new OGRFeatureDefn("SELECT")), iNextShapeId(0)
{
    poFeatureDefn->Reference();
    OGRFieldDefn oField(pszLayerName, OFTString);
    poFeatureDefn->AddFieldDefn(&oField);
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

OGRFeature *OGROSMSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return nullptr;

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    else
        poFeature->SetField(0, nVal);
    poFeature->SetFID(iNextShapeId++);
    return poFeature;
}

/************************************************************************/
/*                      OGROSMResultLayerDecorator                      */
/************************************************************************/

class OGROSMResultLayerDecorator final : public OGRLayerDecorator
{
    std::string osDSName;
    std::string osInterestLayers;

  public:
    OGROSMResultLayerDecorator(OGRLayer *poLayer, const std::string &osDSNameIn,
                               const std::string &osInterestLayersIn)
        : OGRLayerDecorator(poLayer, TRUE), osDSName(osDSNameIn),
          osInterestLayers(osInterestLayersIn)
    {
    }

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override
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

OGRLayer *OGROSMDataSource::ExecuteSQL(const char *pszSQLCommand,
                                       OGRGeometry *poSpatialFilter,
                                       const char *pszDialect)

{
    /* -------------------------------------------------------------------- */
    /*      Special GetBytesRead() command                                  */
    /* -------------------------------------------------------------------- */
    if (strcmp(pszSQLCommand, "GetBytesRead()") == 0)
    {
        char szVal[64] = {};
        snprintf(szVal, sizeof(szVal), CPL_FRMT_GUIB,
                 OSM_GetBytesRead(m_psParser));
        return new OGROSMSingleFeatureLayer("GetBytesRead", szVal);
    }

    /* -------------------------------------------------------------------- */
    /*      Special SHOW config_file_path command                           */
    /* -------------------------------------------------------------------- */
    if (strcmp(pszSQLCommand, "SHOW config_file_path") == 0)
    {
        return new OGROSMSingleFeatureLayer("config_file_path",
                                            m_osConfigFile.c_str());
    }

    if (m_poResultSetLayer != nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "A SQL result layer is still in use. Please delete it first");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Special SET interest_layers = command                           */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH(pszSQLCommand, "SET interest_layers ="))
    {
        char **papszTokens =
            CSLTokenizeString2(pszSQLCommand + 21, ",",
                               CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
        for (auto &&poLayer : m_apoLayers)
        {
            poLayer->SetDeclareInterest(FALSE);
        }

        for (int i = 0; papszTokens[i] != nullptr; i++)
        {
            OGROSMLayer *poLayer =
                dynamic_cast<OGROSMLayer *>(GetLayerByName(papszTokens[i]));
            if (poLayer != nullptr)
            {
                poLayer->SetDeclareInterest(TRUE);
            }
        }

        if (m_apoLayers[IDX_LYR_POINTS]->IsUserInterested() &&
            !m_apoLayers[IDX_LYR_LINES]->IsUserInterested() &&
            !m_apoLayers[IDX_LYR_MULTILINESTRINGS]->IsUserInterested() &&
            !m_apoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() &&
            !m_apoLayers[IDX_LYR_OTHER_RELATIONS]->IsUserInterested())
        {
            if (CPLGetConfigOption("OSM_INDEX_POINTS", nullptr) == nullptr)
            {
                CPLDebug("OSM", "Disabling indexing of nodes");
                m_bIndexPoints = false;
            }
            if (CPLGetConfigOption("OSM_USE_POINTS_INDEX", nullptr) == nullptr)
            {
                m_bUsePointsIndex = false;
            }
            if (CPLGetConfigOption("OSM_INDEX_WAYS", nullptr) == nullptr)
            {
                CPLDebug("OSM", "Disabling indexing of ways");
                m_bIndexWays = false;
            }
            if (CPLGetConfigOption("OSM_USE_WAYS_INDEX", nullptr) == nullptr)
            {
                m_bUseWaysIndex = false;
            }
        }
        else if (m_apoLayers[IDX_LYR_LINES]->IsUserInterested() &&
                 !m_apoLayers[IDX_LYR_MULTILINESTRINGS]->IsUserInterested() &&
                 !m_apoLayers[IDX_LYR_MULTIPOLYGONS]->IsUserInterested() &&
                 !m_apoLayers[IDX_LYR_OTHER_RELATIONS]->IsUserInterested())
        {
            if (CPLGetConfigOption("OSM_INDEX_WAYS", nullptr) == nullptr)
            {
                CPLDebug("OSM", "Disabling indexing of ways");
                m_bIndexWays = false;
            }
            if (CPLGetConfigOption("OSM_USE_WAYS_INDEX", nullptr) == nullptr)
            {
                m_bUseWaysIndex = false;
            }
        }

        CSLDestroy(papszTokens);

        return nullptr;
    }

    while (*pszSQLCommand == ' ')
        pszSQLCommand++;

    /* Try to analyse the SQL command to get the interest table */
    if (STARTS_WITH_CI(pszSQLCommand, "SELECT"))
    {
        bool bLayerAlreadyAdded = false;
        CPLString osInterestLayers = "SET interest_layers =";

        if (pszDialect != nullptr && EQUAL(pszDialect, "SQLITE"))
        {
            const auto oSetLayers = OGRSQLiteGetReferencedLayers(pszSQLCommand);
            for (const LayerDesc &oLayerDesc : oSetLayers)
            {
                if (oLayerDesc.osDSName.empty())
                {
                    if (bLayerAlreadyAdded)
                        osInterestLayers += ",";
                    bLayerAlreadyAdded = true;
                    osInterestLayers += oLayerDesc.osLayerName;
                }
            }
        }
        else
        {
            swq_select sSelectInfo;

            CPLPushErrorHandler(CPLQuietErrorHandler);
            CPLErr eErr = sSelectInfo.preparse(pszSQLCommand);
            CPLPopErrorHandler();

            if (eErr == CE_None)
            {
                swq_select *pCurSelect = &sSelectInfo;
                while (pCurSelect != nullptr)
                {
                    for (int iTable = 0; iTable < pCurSelect->table_count;
                         iTable++)
                    {
                        swq_table_def *psTableDef =
                            pCurSelect->table_defs + iTable;
                        if (psTableDef->data_source == nullptr)
                        {
                            if (bLayerAlreadyAdded)
                                osInterestLayers += ",";
                            bLayerAlreadyAdded = true;
                            osInterestLayers += psTableDef->table_name;
                        }
                    }
                    pCurSelect = pCurSelect->poOtherSelect;
                }
            }
        }

        if (bLayerAlreadyAdded)
        {
            /* Backup current optimization parameters */
            m_abSavedDeclaredInterest.resize(0);
            for (auto &&poLayer : m_apoLayers)
            {
                m_abSavedDeclaredInterest.push_back(
                    poLayer->IsUserInterested());
            }
            m_bIndexPointsBackup = m_bIndexPoints;
            m_bUsePointsIndexBackup = m_bUsePointsIndex;
            m_bIndexWaysBackup = m_bIndexWays;
            m_bUseWaysIndexBackup = m_bUseWaysIndex;

            /* Update optimization parameters */
            delete ExecuteSQL(osInterestLayers, nullptr, nullptr);

            MyResetReading();

            /* Run the request */
            m_poResultSetLayer = GDALDataset::ExecuteSQL(
                pszSQLCommand, poSpatialFilter, pszDialect);

            /* If the user explicitly run a COUNT() request, then do it ! */
            if (m_poResultSetLayer)
            {
                if (pszDialect != nullptr && EQUAL(pszDialect, "SQLITE"))
                {
                    m_poResultSetLayer = new OGROSMResultLayerDecorator(
                        m_poResultSetLayer, GetDescription(), osInterestLayers);
                }
                m_bIsFeatureCountEnabled = true;
            }

            return m_poResultSetLayer;
        }
    }

    return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGROSMDataSource::ReleaseResultSet(OGRLayer *poLayer)

{
    if (poLayer != nullptr && poLayer == m_poResultSetLayer)
    {
        m_poResultSetLayer = nullptr;

        m_bIsFeatureCountEnabled = false;

        /* Restore backup'ed optimization parameters */
        int i = 0;
        for (auto &&poIterLayer : m_apoLayers)
        {
            poIterLayer->SetDeclareInterest(m_abSavedDeclaredInterest[i]);
            ++i;
        }
        if (m_bIndexPointsBackup && !m_bIndexPoints)
            CPLDebug("OSM", "Re-enabling indexing of nodes");
        m_bIndexPoints = m_bIndexPointsBackup;
        m_bUsePointsIndex = m_bUsePointsIndexBackup;
        if (m_bIndexWaysBackup && !m_bIndexWays)
            CPLDebug("OSM", "Re-enabling indexing of ways");
        m_bIndexWays = m_bIndexWaysBackup;
        m_bUseWaysIndex = m_bUseWaysIndexBackup;
        m_abSavedDeclaredInterest.clear();
    }

    delete poLayer;
}

/************************************************************************/
/*                         IsInterleavedReading()                       */
/************************************************************************/

int OGROSMDataSource::IsInterleavedReading()
{
    if (m_bInterleavedReading < 0)
    {
        m_bInterleavedReading =
            CPLTestBool(CPLGetConfigOption("OGR_INTERLEAVED_READING", "NO"));
        CPLDebug("OSM", "OGR_INTERLEAVED_READING = %d", m_bInterleavedReading);
    }
    return m_bInterleavedReading;
}
