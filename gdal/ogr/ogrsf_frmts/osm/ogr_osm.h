/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/OpenStreeMap driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_OSM_H_INCLUDED
#define OGR_OSM_H_INCLUDED

// replace O(log2(N)) complexity of FindNode() by O(1)
#define ENABLE_NODE_LOOKUP_BY_HASHING 1

#include "ogrsf_frmts.h"
#include "cpl_string.h"

#include <array>
#include <set>
#include <unordered_set>
#include <map>
#include <vector>

#include "osm_parser.h"

#include "ogrsqlitevfs.h"

class ConstCharComp
{
    public:
        bool operator()(const char* a, const char* b) const
        {
            return strcmp(a, b) < 0;
        }
};

class OGROSMComputedAttribute
{
    public:
        CPLString    osName;
        int          nIndex;
        OGRFieldType eType;
        CPLString    osSQL;
        sqlite3_stmt  *hStmt;
        std::vector<CPLString> aosAttrToBind;
        std::vector<int> anIndexToBind;
        bool         bHardcodedZOrder;

        OGROSMComputedAttribute() : nIndex(-1), eType(OFTString), hStmt(nullptr), bHardcodedZOrder(false) {}
        explicit OGROSMComputedAttribute(const char* pszName) :
                osName(pszName), nIndex(-1), eType(OFTString), hStmt(nullptr), bHardcodedZOrder(false) {}
};

/************************************************************************/
/*                           OGROSMLayer                                */
/************************************************************************/

class OGROSMDataSource;

class OGROSMLayer final: public OGRLayer
{
    friend class OGROSMDataSource;

    OGROSMDataSource    *poDS;
    int                  nIdxLayer;
    OGRFeatureDefn      *poFeatureDefn;
    OGRSpatialReference *poSRS;
    long                 nFeatureCount;

    std::vector<char*>   apszNames; /* Needed to keep a "reference" to the string inserted into oMapFieldNameToIndex */
    std::map<const char*, int, ConstCharComp> oMapFieldNameToIndex;

    std::vector<OGROSMComputedAttribute> oComputedAttributes;

    bool                 bResetReadingAllowed;

    int                  nFeatureArraySize;
    int                  nFeatureArrayMaxSize;
    int                  nFeatureArrayIndex;
    OGRFeature**         papoFeatures;

    bool                  bHasOSMId;
    int                   nIndexOSMId;
    int                   nIndexOSMWayId;
    bool                  bHasVersion;
    bool                  bHasTimestamp;
    bool                  bHasUID;
    bool                  bHasUser;
    bool                  bHasChangeset;
    bool                  bHasOtherTags;
    int                   nIndexOtherTags;
    bool                  bHasAllTags;
    int                   nIndexAllTags;

    bool                  bHasWarnedTooManyFeatures;

    char                 *pszAllTags;
    bool                  bHasWarnedAllTagsTruncated;

    bool                  bUserInterested;

    bool                  AddToArray( OGRFeature* poFeature,
                                      int bCheckFeatureThreshold );

    int                   AddInOtherOrAllTags(const char* pszK);

    char                  szLaunderedFieldName[256];
    const char*           GetLaunderedFieldName(const char* pszName);

    std::vector<char*>    apszInsignificantKeys;
    std::map<const char*, int, ConstCharComp> aoSetInsignificantKeys;

    std::vector<char*>    apszIgnoreKeys;
    std::map<const char*, int, ConstCharComp> aoSetIgnoreKeys;

    std::set<std::string> aoSetWarnKeys;

  public:
                        OGROSMLayer( OGROSMDataSource* poDS,
                                     int nIdxLayer,
                                     const char* pszName );
    virtual             ~OGROSMLayer();

    virtual OGRFeatureDefn *GetLayerDefn() override {return poFeatureDefn;}

    virtual void        ResetReading() override;
    virtual int         TestCapability( const char * ) override;

    virtual OGRFeature *GetNextFeature() override;

    OGRFeature*         MyGetNextFeature( OGROSMLayer** ppoNewCurLayer,
                                          GDALProgressFunc pfnProgress,
                                          void* pProgressData );

    virtual GIntBig     GetFeatureCount( int bForce ) override;

    virtual OGRErr      SetAttributeFilter( const char* pszAttrQuery ) override;

    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    const OGREnvelope*  GetSpatialFilterEnvelope();

    int                 AddFeature(OGRFeature* poFeature,
                                   int bAttrFilterAlreadyEvaluated,
                                   int* pbFilteredOut = nullptr,
                                   int bCheckFeatureThreshold = TRUE);
    void                ForceResetReading();

    void                AddField(const char* pszName, OGRFieldType eFieldType);
    int                 GetFieldIndex(const char* pszName);

    bool                HasOSMId() const { return bHasOSMId; }
    void                SetHasOSMId(bool bIn) { bHasOSMId = bIn; }

    bool                HasVersion() const { return bHasVersion; }
    void                SetHasVersion(bool bIn) { bHasVersion = bIn; }

    bool                HasTimestamp() const { return bHasTimestamp; }
    void                SetHasTimestamp(bool bIn) { bHasTimestamp = bIn; }

    bool                HasUID() const { return bHasUID; }
    void                SetHasUID(bool bIn) { bHasUID = bIn; }

    bool                HasUser() const { return bHasUser; }
    void                SetHasUser(bool bIn) { bHasUser = bIn; }

    bool                HasChangeset() const { return bHasChangeset; }
    void                SetHasChangeset(bool bIn) { bHasChangeset = bIn; }

    bool                HasOtherTags() const { return bHasOtherTags; }
    void                SetHasOtherTags(bool bIn) { bHasOtherTags = bIn; }

    bool                HasAllTags() const { return bHasAllTags; }
    void                SetHasAllTags(bool bIn) { bHasAllTags = bIn; }

    void                SetFieldsFromTags(OGRFeature* poFeature,
                                          GIntBig nID,
                                          bool bIsWayID,
                                          unsigned int nTags, OSMTag* pasTags,
                                          OSMInfo* psInfo);

    void                SetDeclareInterest(bool bIn) { bUserInterested = bIn; }
    bool                IsUserInterested() const { return bUserInterested; }

    int                 HasAttributeFilter() const { return m_poAttrQuery != nullptr; }
    int                 EvaluateAttributeFilter(OGRFeature* poFeature);

    void                AddInsignificantKey(const char* pszK);
    int                 IsSignificantKey(const char* pszK) const
        { return aoSetInsignificantKeys.find(pszK) == aoSetInsignificantKeys.end(); }

    void                AddIgnoreKey(const char* pszK);
    void                AddWarnKey(const char* pszK);

    void                AddComputedAttribute(const char* pszName,
                                             OGRFieldType eType,
                                             const char* pszSQL);
};

/************************************************************************/
/*                        OGROSMDataSource                              */
/************************************************************************/

typedef struct
{
    char* pszK;
    int nKeyIndex;
    int nOccurrences;
    std::vector<char*> asValues;
    std::map<const char*, int, ConstCharComp> anMapV; /* map that is the reverse of asValues */
} KeyDesc;

typedef struct
{
    short               nKeyIndex; /* index of OGROSMDataSource.asKeys */
    short               bVIsIndex; /* whether we should use nValueIndex or nOffsetInpabyNonRedundantValues */
    union
    {
        int                 nValueIndex; /* index of KeyDesc.asValues */
        int                 nOffsetInpabyNonRedundantValues; /* offset in OGROSMDataSource.pabyNonRedundantValues */
    } u;
} IndexedKVP;

typedef struct
{
    GIntBig             nOff;
    /* Note: only one of nth bucket pabyBitmap or panSectorSize must be free'd */
    union
    {
        GByte          *pabyBitmap;    /* array of BUCKET_BITMAP_SIZE bytes */
        GByte          *panSectorSize; /* array of BUCKET_SECTOR_SIZE_ARRAY_SIZE bytes. Each values means (size in bytes - 8 ) / 2, minus 8. 252 means uncompressed */
    } u;
} Bucket;

typedef struct
{
    int               nLon;
    int               nLat;
} LonLat;

typedef struct
{
    GIntBig             nWayID;
    GIntBig*            panNodeRefs; /* point to a sub-array of OGROSMDataSource.anReqIds */
    unsigned int        nRefs;
    unsigned int        nTags;
    IndexedKVP*         pasTags; /*  point to a sub-array of OGROSMDataSource.pasAccumulatedTags */
    OSMInfo             sInfo;
    OGRFeature         *poFeature;
    EMULATED_BOOL       bIsArea : 1;
    EMULATED_BOOL       bAttrFilterAlreadyEvaluated : 1;
} WayFeaturePair;

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
typedef struct
{
    int nInd;           /* values are indexes of panReqIds */
    int nNext;          /* values are indexes of psCollisionBuckets, or -1 to stop the chain */
} CollisionBucket;
#endif

class OGROSMDataSource final: public OGRDataSource
{
    friend class OGROSMLayer;

    int                 nLayers;
    OGROSMLayer**       papoLayers;
    char*               pszName;

    OGREnvelope         sExtent;
    bool                bExtentValid;

    // Starts off at -1 to indicate that we do not know.
    int                 bInterleavedReading;
    OGROSMLayer        *poCurrentLayer;

    OSMContext         *psParser;
    bool                bHasParsedFirstChunk;
    bool                bStopParsing;

    sqlite3_vfs*        pMyVFS;

    sqlite3            *hDB;
    sqlite3_stmt       *hInsertNodeStmt;
    sqlite3_stmt       *hInsertWayStmt;
    sqlite3_stmt       *hSelectNodeBetweenStmt;
    sqlite3_stmt      **pahSelectNodeStmt;
    sqlite3_stmt      **pahSelectWayStmt;
    sqlite3_stmt       *hInsertPolygonsStandaloneStmt;
    sqlite3_stmt       *hDeletePolygonsStandaloneStmt;
    sqlite3_stmt       *hSelectPolygonsStandaloneStmt;
    bool                bHasRowInPolygonsStandalone;

    sqlite3            *hDBForComputedAttributes;

    int                 nMaxSizeForInMemoryDBInMB;
    bool                bInMemoryTmpDB;
    bool                bMustUnlink;
    CPLString           osTmpDBName;

    int                 nNodesInTransaction;

    std::unordered_set<std::string> aoSetClosedWaysArePolygons;
    int                 nMinSizeKeysInSetClosedWaysArePolygons;
    int                 nMaxSizeKeysInSetClosedWaysArePolygons;

    std::vector<LonLat> m_asLonLatCache{};

    std::array<const char*, 7>  m_ignoredKeys;

    bool                bReportAllNodes;
    bool                bReportAllWays;

    bool                bFeatureAdded;

    bool                bInTransaction;

    bool                bIndexPoints;
    bool                bUsePointsIndex;
    bool                bIndexWays;
    bool                bUseWaysIndex;

    std::vector<bool>   abSavedDeclaredInterest;
    OGRLayer*           poResultSetLayer;
    bool                bIndexPointsBackup;
    bool                bUsePointsIndexBackup;
    bool                bIndexWaysBackup;
    bool                bUseWaysIndexBackup;

    bool                bIsFeatureCountEnabled;

    bool                bAttributeNameLaundering;

    std::vector<GByte>  m_abyWayBuffer{};

    int                 nWaysProcessed;
    int                 nRelationsProcessed;

    bool                bCustomIndexing;
    bool                bCompressNodes;

    unsigned int        nUnsortedReqIds;
    GIntBig            *panUnsortedReqIds;

    unsigned int        nReqIds;
    GIntBig            *panReqIds;

#ifdef ENABLE_NODE_LOOKUP_BY_HASHING
    bool                bEnableHashedIndex;
    /* values >= 0 are indexes of panReqIds. */
    /*        == -1 for unoccupied */
    /*        < -1 are expressed as -nIndexToCollisionBuckets-2 where nIndexToCollisionBuckets point to psCollisionBuckets */
    int                *panHashedIndexes;
    CollisionBucket    *psCollisionBuckets;
    bool                bHashedIndexValid;
#endif

    LonLat             *pasLonLatArray;

    IndexedKVP         *pasAccumulatedTags; /* points to content of pabyNonRedundantValues or aoMapIndexedKeys */
    int                 nAccumulatedTags;
    GByte              *pabyNonRedundantValues;
    int                 nNonRedundantValuesLen;
    WayFeaturePair     *pasWayFeaturePairs;
    int                 nWayFeaturePairs;

    int                          nNextKeyIndex;
    std::vector<KeyDesc*>         asKeys;
    std::map<const char*, KeyDesc*, ConstCharComp> aoMapIndexedKeys; /* map that is the reverse of asKeys */

    CPLString           osNodesFilename;
    bool                bInMemoryNodesFile;
    bool                bMustUnlinkNodesFile;
    GIntBig             nNodesFileSize;
    VSILFILE           *fpNodes;

    GIntBig             nPrevNodeId;
    int                 nBucketOld;
    int                 nOffInBucketReducedOld;
    GByte              *pabySector;
    std::map<int, Bucket> oMapBuckets;
    Bucket*             GetBucket(int nBucketId);

    bool                bNeedsToSaveWayInfo;

    static const GIntBig FILESIZE_NOT_INIT = -2;
    static const GIntBig FILESIZE_INVALID = -1;
    GIntBig             m_nFileSize;

    void                CompressWay (bool bIsArea, unsigned int nTags, IndexedKVP* pasTags,
                                     int nPoints, LonLat* pasLonLatPairs,
                                     OSMInfo* psInfo,
                                     std::vector<GByte> &abyCompressedWay);
    void                UncompressWay( int nBytes, const GByte* pabyCompressedWay,
                                       bool *pbIsArea,
                                       std::vector<LonLat>& asCoords,
                                       unsigned int* pnTags, OSMTag* pasTags,
                                       OSMInfo* psInfo );

    bool                ParseConf(char** papszOpenOptions);
    bool                CreateTempDB();
    bool                SetDBOptions();
    bool                SetCacheSize();
    bool                CreatePreparedStatements();
    void                CloseDB();

    bool                IndexPoint( OSMNode* psNode );
    bool                IndexPointSQLite( OSMNode* psNode );
    bool                FlushCurrentSector();
    bool                FlushCurrentSectorCompressedCase();
    bool                FlushCurrentSectorNonCompressedCase();
    bool                IndexPointCustom( OSMNode* psNode );

    void                IndexWay(GIntBig nWayID, bool bIsArea,
                                 unsigned int nTags, IndexedKVP* pasTags,
                                 LonLat* pasLonLatPairs, int nPairs,
                                 OSMInfo* psInfo);

    bool                StartTransactionCacheDB();
    bool                CommitTransactionCacheDB();

    int                 FindNode(GIntBig nID);
    void                ProcessWaysBatch();

    void                ProcessPolygonsStandalone();

    void                LookupNodes();
    void                LookupNodesSQLite();
    void                LookupNodesCustom();
    void                LookupNodesCustomCompressedCase();
    void                LookupNodesCustomNonCompressedCase();

    unsigned int        LookupWays( std::map< GIntBig, std::pair<int,void*> >& aoMapWays,
                                    OSMRelation* psRelation );

    OGRGeometry*        BuildMultiPolygon(OSMRelation* psRelation,
                                          unsigned int* pnTags,
                                          OSMTag* pasTags);
    OGRGeometry*        BuildGeometryCollection(OSMRelation* psRelation, int bMultiLineString);

    bool                TransferToDiskIfNecesserary();

    Bucket*             AllocBucket(int iBucket);

    void                AddComputedAttributes(int iCurLayer,
                                             const std::vector<OGROSMComputedAttribute>& oAttributes);
    bool                IsClosedWayTaggedAsPolygon( unsigned int nTags, const OSMTag* pasTags );

  public:
                        OGROSMDataSource();
                        virtual ~OGROSMDataSource();

    virtual const char *GetName() override { return pszName; }
    virtual int         GetLayerCount() override { return nLayers; }
    virtual OGRLayer   *GetLayer( int ) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    virtual void        ResetReading() override;
    virtual OGRFeature* GetNextFeature( OGRLayer** ppoBelongingLayer,
                                        double* pdfProgressPct,
                                       GDALProgressFunc pfnProgress,
                                        void* pProgressData ) override;

    int                 Open ( const char* pszFilename, char** papszOpenOptions );

    int                 MyResetReading();
    bool                ParseNextChunk(int nIdxLayer,
                                       GDALProgressFunc pfnProgress,
                                       void* pProgressData);
    OGRErr              GetExtent( OGREnvelope *psExtent );
    int                 IsInterleavedReading();

    void                NotifyNodes(unsigned int nNodes, OSMNode* pasNodes);
    void                NotifyWay (OSMWay* psWay);
    void                NotifyRelation (OSMRelation* psRelation);
    void                NotifyBounds (double dfXMin, double dfYMin,
                                      double dfXMax, double dfYMax);

    OGROSMLayer*        GetCurrentLayer() { return poCurrentLayer; }
    void                SetCurrentLayer(OGROSMLayer* poLyr) { poCurrentLayer = poLyr; }

    bool                IsFeatureCountEnabled() const { return bIsFeatureCountEnabled; }

    bool                DoesAttributeNameLaundering() const { return bAttributeNameLaundering; }
};

#endif /* ndef OGR_OSM_H_INCLUDED */
