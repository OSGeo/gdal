/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/OpenStreeMap driver.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#ifndef _OGR_OSM_H_INCLUDED
#define _OGR_OSM_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_string.h"

#include <set>
#include <map>
#include <vector>

#include "osm_parser.h"

#include "ogr_sqlite.h"

class ConstCharComp
{
    public:
        bool operator()(const char* a, const char* b) const
        {
            return strcmp(a, b) < 0;
        }
};

/************************************************************************/
/*                           OGROSMLayer                                */
/************************************************************************/

class OGROSMDataSource;

class OGROSMLayer : public OGRLayer
{
    friend class OGROSMDataSource;
    
    OGROSMDataSource    *poDS;
    OGRFeatureDefn      *poFeatureDefn;
    OGRSpatialReference *poSRS;
    long                 nFeatureCount;

    std::vector<char*>   apszNames;
    std::map<const char*, int, ConstCharComp> oMapFieldNameToIndex;

    int                  bResetReadingAllowed;
    
    int                  nFeatureArraySize;
    int                  nFeatureArrayMaxSize;
    int                  nFeatureArrayIndex;
    OGRFeature**         papoFeatures;

    int                   bHasOSMId;
    int                   nIndexOSMId;
    int                   nIndexOSMWayId;
    int                   bHasVersion;
    int                   bHasTimestamp;
    int                   bHasUID;
    int                   bHasUser;
    int                   bHasChangeset;
    int                   bHasOtherTags;

    int                   bHasWarnedTooManyFeatures;

    char                 *pszAllTags;
    int                   bHasWarnedAllTagsTruncated;

    int                   bUserInterested;

    int                  AddToArray(OGRFeature* poFeature);

    char                  szLaunderedFieldName[256];
    const char*           GetLaunderedFieldName(const char* pszName);

    std::vector<char*>    apszUnsignificantKeys;
    std::map<const char*, int, ConstCharComp> aoSetUnsignificantKeys;

    std::vector<char*>    apszIgnoreKeys;
    std::map<const char*, int, ConstCharComp> aoSetIgnoreKeys;

    std::set<std::string> aoSetWarnKeys;

  public:
                        OGROSMLayer( OGROSMDataSource* poDS,
                                     const char* pszName );
    virtual             ~OGROSMLayer();

    virtual OGRFeatureDefn *GetLayerDefn() {return poFeatureDefn;}
    virtual OGRSpatialReference * GetSpatialRef() { return poSRS; }
    
    virtual void        ResetReading();
    virtual int         TestCapability( const char * );
                                     
    virtual OGRFeature *GetNextFeature();
    virtual int         GetFeatureCount( int bForce );
    
    virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );

    const OGREnvelope*  GetSpatialFilterEnvelope();

    int                 AddFeature(OGRFeature* poFeature,
                                   int bAttrFilterAlreadyEvaluated,
                                   int* pbFilteredOut = NULL);
    void                ForceResetReading();

    void                AddField(const char* pszName, OGRFieldType eFieldType);
    int                 GetFieldIndex(const char* pszName);

    int                 HasOSMId() const { return bHasOSMId; }
    void                SetHasOSMId(int bIn) { bHasOSMId = bIn; }

    int                 HasVersion() const { return bHasVersion; }
    void                SetHasVersion(int bIn) { bHasVersion = bIn; }

    int                 HasTimestamp() const { return bHasTimestamp; }
    void                SetHasTimestamp(int bIn) { bHasTimestamp = bIn; }

    int                 HasUID() const { return bHasUID; }
    void                SetHasUID(int bIn) { bHasUID = bIn; }

    int                 HasUser() const { return bHasUser; }
    void                SetHasUser(int bIn) { bHasUser = bIn; }

    int                 HasChangeset() const { return bHasChangeset; }
    void                SetHasChangeset(int bIn) { bHasChangeset = bIn; }

    void                SetHasOtherTags(int bIn) { bHasOtherTags = bIn; }
    int                 HasOtherTags() const { return bHasOtherTags; }
    int                 AddInOtherTag(const char* pszK);

    void                SetFieldsFromTags(OGRFeature* poFeature,
                                          GIntBig nID,
                                          int bIsWayID,
                                          unsigned int nTags, OSMTag* pasTags,
                                          OSMInfo* psInfo);

    void                SetDeclareInterest(int bIn) { bUserInterested = bIn; }
    int                 IsUserInterested() const { return bUserInterested; }

    int                 HasAttributeFilter() const { return m_poAttrQuery != NULL; }
    int                 EvaluateAttributeFilter(OGRFeature* poFeature);

    void                AddUnsignificantKey(const char* pszK);
    int                 IsSignificantKey(const char* pszK) const
        { return aoSetUnsignificantKeys.find(pszK) == aoSetUnsignificantKeys.end(); }

    void                AddIgnoreKey(const char* pszK);
    void                AddWarnKey(const char* pszK);
};

/************************************************************************/
/*                        OGROSMDataSource                              */
/************************************************************************/

typedef struct
{
    char* pszK;
    int nKeyIndex;
    int nOccurences;
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
    union
    {
        GByte          *pabyBitmap;
        GByte          *panSectorSize; /* (size in bytes - 8 ) / 2, minus 8. 252 means uncompressed */
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
    int                 bIsArea : 1;
    int                 bAttrFilterAlreadyEvaluated : 1;
} WayFeaturePair;

class OGROSMDataSource : public OGRDataSource
{
    friend class OGROSMLayer;
    
    int                 nLayers;
    OGROSMLayer**       papoLayers;
    char*               pszName;

    OGREnvelope         sExtent;
    int                 bExtentValid;

    int                 bInterleavedReading;
    OGROSMLayer        *poCurrentLayer;

    OSMContext         *psParser;
    int                 bHasParsedFirstChunk;
    int                 bStopParsing;

#ifdef HAVE_SQLITE_VFS
    sqlite3_vfs*        pMyVFS;
#endif

    sqlite3            *hDB;
    sqlite3_stmt       *hInsertNodeStmt;
    sqlite3_stmt       *hInsertWayStmt;
    sqlite3_stmt       *hSelectNodeBetweenStmt;
    sqlite3_stmt      **pahSelectNodeStmt;
    sqlite3_stmt      **pahSelectWayStmt;
    sqlite3_stmt       *hInsertPolygonsStandaloneStmt;
    sqlite3_stmt       *hDeletePolygonsStandaloneStmt;
    sqlite3_stmt       *hSelectPolygonsStandaloneStmt;
    int                 bHasRowInPolygonsStandalone;

    int                 nMaxSizeForInMemoryDBInMB;
    int                 bInMemoryTmpDB;
    int                 bMustUnlink;
    CPLString           osTmpDBName;

    int                 nNodesInTransaction;

    std::set<std::string> aoSetClosedWaysArePolygons;

    LonLat             *pasLonLatCache;

    int                 bReportAllNodes;
    int                 bReportAllWays;

    int                 bFeatureAdded;

    int                 bInTransaction;

    int                 bIndexPoints;
    int                 bUsePointsIndex;
    int                 bIndexWays;
    int                 bUseWaysIndex;

    std::vector<int>      abSavedDeclaredInterest;
    OGRLayer*             poResultSetLayer;
    int                   bIndexPointsBackup;
    int                   bUsePointsIndexBackup;
    int                   bIndexWaysBackup;
    int                   bUseWaysIndexBackup;

    int                 bIsFeatureCountEnabled;

    int                 bAttributeNameLaundering;

    GByte              *pabyWayBuffer;

    int                 nWaysProcessed;
    int                 nRelationsProcessed;

    int                 bCustomIndexing;
    int                 bCompressNodes;

    unsigned int        nUnsortedReqIds;
    GIntBig            *panUnsortedReqIds;

    unsigned int        nReqIds;
    GIntBig            *panReqIds;
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
    int                 bInMemoryNodesFile;
    int                 bMustUnlinkNodesFile;
    GIntBig             nNodesFileSize;
    VSILFILE           *fpNodes;

    GIntBig             nPrevNodeId;
    int                 nBucketOld;
    int                 nOffInBucketReducedOld;
    GByte              *pabySector;
    Bucket             *papsBuckets;
    int                 nBuckets;

    int                 bNeedsToSaveWayInfo;

    int                 CompressWay (unsigned int nTags, IndexedKVP* pasTags,
                                     int nPoints, LonLat* pasLonLatPairs,
                                     OSMInfo* psInfo,
                                     GByte* pabyCompressedWay);
    int                 UncompressWay( int nBytes, GByte* pabyCompressedWay,
                                       LonLat* pasCoords,
                                       unsigned int* pnTags, OSMTag* pasTags,
                                       OSMInfo* psInfo );

    int                 ParseConf();
    int                 CreateTempDB();
    int                 SetDBOptions();
    int                 SetCacheSize();
    int                 CreatePreparedStatements();
    void                CloseDB();

    int                 IndexPoint(OSMNode* psNode);
    int                 IndexPointSQLite(OSMNode* psNode);
    int                 FlushCurrentSector();
    int                 FlushCurrentSectorCompressedCase();
    int                 FlushCurrentSectorNonCompressedCase();
    int                 IndexPointCustom(OSMNode* psNode);

    void                IndexWay(GIntBig nWayID,
                                 unsigned int nTags, IndexedKVP* pasTags,
                                 LonLat* pasLonLatPairs, int nPairs,
                                 OSMInfo* psInfo);

    int                 StartTransaction();
    int                 CommitTransaction();

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

    int                 TransferToDiskIfNecesserary();

    int                 AllocMoreBuckets(int nNewBucketIdx);

  public:
                        OGROSMDataSource();
                        ~OGROSMDataSource();

    virtual const char *GetName() { return pszName; }
    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer   *GetLayer( int );

    virtual int         TestCapability( const char * );

    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );


    int                 Open ( const char* pszFilename, int bUpdateIn );

    int                 ResetReading();
    int                 ParseNextChunk();
    OGRErr              GetExtent( OGREnvelope *psExtent );
    int                 IsInterleavedReading();

    void                NotifyNodes(unsigned int nNodes, OSMNode* pasNodes);
    void                NotifyWay (OSMWay* psWay);
    void                NotifyRelation (OSMRelation* psRelation);
    void                NotifyBounds (double dfXMin, double dfYMin,
                                      double dfXMax, double dfYMax);

    OGROSMLayer*        GetCurrentLayer() { return poCurrentLayer; }
    void                SetCurrentLayer(OGROSMLayer* poLyr) { poCurrentLayer = poLyr; }

    int                 IsFeatureCountEnabled() const { return bIsFeatureCountEnabled; }

    int                 DoesAttributeNameLaundering() const { return bAttributeNameLaundering; }
};

/************************************************************************/
/*                            OGROSMDriver                              */
/************************************************************************/

class OGROSMDriver : public OGRSFDriver
{
  public:
                ~OGROSMDriver();

    virtual const char    *GetName();
    virtual OGRDataSource *Open( const char *, int );
    virtual OGRDataSource *CreateDataSource( const char * pszName,
                                             char **papszOptions );

    virtual int            TestCapability( const char * );
};

#endif /* ndef _OGR_OSM_H_INCLUDED */

