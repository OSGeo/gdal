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

    int                  bResetReadingAllowed;
    
    int                  nFeatureArraySize;
    int                  nFeatureArrayMaxSize;
    int                  nFeatureArrayIndex;
    OGRFeature**         papoFeatures;

    int                   bHasOSMId;
    int                   bHasVersion;
    int                   bHasTimestamp;
    int                   bHasUID;
    int                   bHasUser;
    int                   bHasChangeset;
    int                   bHasOtherTags;

    int                   bHasWarned;

    int                   bUserInterested;

    int                AddToArray(OGRFeature* poFeature);

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

    int                 AddFeature(OGRFeature* poFeature, int* pbFilteredOut = NULL);
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
                                          unsigned int nTags, OSMTag* pasTags,
                                          OSMInfo* psInfo);

    void                SetDeclareInterest(int bIn) { bUserInterested = bIn; }
    int                 IsUserInterested() const { return bUserInterested; }

    std::set<std::string> aoSetUnsignificantKeys;
    std::set<std::string> aoSetIgnoreKeys;
    std::set<std::string> aoSetWarnKeys;
};

/************************************************************************/
/*                        OGROSMDataSource                              */
/************************************************************************/

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

    int                 nNodeSelectBetween, nNodeSelectIn;

    int                 nMaxSizeForInMemoryDBInMB;
    int                 bInMemoryTmpDB;
    int                 bMustUnlink;
    CPLString           osTmpDBName;

    int                 nNodesInTransaction;

    std::set<std::string> aoSetClosedWaysArePolygons;

    float*              pafLonLatCache;

    int                 bReportAllNodes;

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

    int                 ParseConf();
    int                 CreateTempDB();
    int                 SetDBOptions();
    int                 SetCacheSize();
    int                 CreatePreparedStatements();
    void                CloseDB();

    void                IndexPoint(OSMNode* psNode);
    void                IndexWay(OSMWay* psWay, OGRLineString* poLS);

    int                 StartTransaction();
    int                 CommitTransaction();

    unsigned int        LookupNodes( std::map< GIntBig, std::pair<float,float> >& aoMapNodes,
                                     OSMWay* psWay );
    unsigned int        LookupWays( std::map< GIntBig, std::pair<int,void*> >& aoMapWays,
                                    OSMRelation* psRelation );

    OGRGeometry*        BuildMultiPolygon(OSMRelation* psRelation);

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

