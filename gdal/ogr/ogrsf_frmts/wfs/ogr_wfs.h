/******************************************************************************
 * $Id$
 *
 * Project:  WFS Translator
 * Purpose:  Definition of classes for OGR WFS driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_WFS_H_INCLUDED
#define OGR_WFS_H_INCLUDED

#include <vector>
#include <set>
#include <map>

#include "cpl_minixml.h"
#include "ogrsf_frmts.h"
#include "gmlreader.h"
#include "cpl_http.h"
#include "swq.h"

CPLXMLNode* WFSFindNode(CPLXMLNode* psXML, const char* pszRootName);
void OGRWFSRecursiveUnlink( const char *pszName );
CPLString WFS_TurnSQLFilterToOGCFilter( const swq_expr_node* poExpr,
                                        OGRDataSource* poDS,
                                        OGRFeatureDefn* poFDefn,
                                        int nVersion,
                                        int bPropertyIsNotEqualToSupported,
                                        int bUseFeatureId,
                                        int bGmlObjectIdNeedsGMLPrefix,
                                        const char* pszNSPrefix,
                                        int* pbOutNeedsNullCheck);
swq_custom_func_registrar* WFSGetCustomFuncRegistrar();

const char* FindSubStringInsensitive(const char* pszStr,
                                     const char* pszSubStr);

CPLString WFS_EscapeURL(const char* pszURL);
CPLString WFS_DecodeURL(const CPLString &osSrc);

// cppcheck-suppress copyCtorAndEqOperator
class OGRWFSSortDesc
{
    public:
        CPLString osColumn;
        bool      bAsc;

        OGRWFSSortDesc( const CPLString& osColumnIn, int bAscIn ) :
            osColumn(osColumnIn),
            bAsc(CPL_TO_BOOL(bAscIn)) {}
        OGRWFSSortDesc(const OGRWFSSortDesc& other) :
            osColumn(other.osColumn),
            bAsc(other.bAsc) {}
};

/************************************************************************/
/*                             OGRWFSLayer                              */
/************************************************************************/

class OGRWFSDataSource;

class OGRWFSLayer : public OGRLayer
{
    OGRWFSDataSource*   poDS;

    OGRFeatureDefn*     poFeatureDefn;
    bool                bGotApproximateLayerDefn;
    GMLFeatureClass*    poGMLFeatureClass;

    int                 bAxisOrderAlreadyInverted;
    OGRSpatialReference *poSRS;

    char*               pszBaseURL;
    char*               pszName;
    char*               pszNS;
    char*               pszNSVal;

    bool                bStreamingDS;
    GDALDataset        *poBaseDS;
    OGRLayer           *poBaseLayer;
    bool                bHasFetched;
    bool                bReloadNeeded;

    CPLString           osGeometryColumnName;
    OGRwkbGeometryType  eGeomType;
    GIntBig             nFeatures;
    bool                bCountFeaturesInGetNextFeature;

    int                 CanRunGetFeatureCountAndGetExtentTogether();

    CPLString           MakeGetFeatureURL(int nMaxFeatures, int bRequestHits);
    bool                MustRetryIfNonCompliantServer( const char* pszServerAnswer );
    GDALDataset*        FetchGetFeature(int nMaxFeatures);
    OGRFeatureDefn*     DescribeFeatureType();
    GIntBig             ExecuteGetFeatureResultTypeHits();

    double              dfMinX;
    double              dfMinY;
    double              dfMaxX;
    double              dfMaxY;
    bool                bHasExtents;

    OGRGeometry        *poFetchedFilterGeom;

    CPLString           osSQLWhere;
    CPLString           osWFSWhere;

    CPLString           osTargetNamespace;
    CPLString           GetDescribeFeatureTypeURL(int bWithNS);

    int                 nExpectedInserts;
    CPLString           osGlobalInsert;
    std::vector<CPLString> aosFIDList;

    bool                bInTransaction;

    CPLString           GetPostHeader();

    bool                bUseFeatureIdAtLayerLevel;

    bool                bPagingActive;
    int                 nPagingStartIndex;
    int                 nFeatureRead;
    int                 nFeatureCountRequested;

    OGRFeatureDefn*     BuildLayerDefnFromFeatureClass(GMLFeatureClass* poClass);

    char                *pszRequiredOutputFormat;

    std::vector<OGRWFSSortDesc> aoSortColumns;

  public:
                        OGRWFSLayer(OGRWFSDataSource* poDS,
                                    OGRSpatialReference* poSRS,
                                    int bAxisOrderAlreadyInverted,
                                    const char* pszBaseURL,
                                    const char* pszName,
                                    const char* pszNS,
                                    const char* pszNSVal);

                        virtual ~OGRWFSLayer();

    OGRWFSLayer*                Clone();

    const char                 *GetName() override { return pszName; }

    virtual void                ResetReading() override;
    virtual OGRFeature*         GetNextFeature() override;
    virtual OGRFeature*         GetFeature(GIntBig nFID) override;

    virtual OGRFeatureDefn *    GetLayerDefn() override;

    virtual int                 TestCapability( const char * ) override;

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;

    void                SetExtents(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;

    virtual OGRErr      StartTransaction() override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual OGRErr      SetIgnoredFields( const char **papszFields ) override;

    int                 HasLayerDefn() { return poFeatureDefn != NULL; }

    OGRFeatureDefn*     ParseSchema(CPLXMLNode* psSchema);
    OGRFeatureDefn*     BuildLayerDefn(OGRFeatureDefn* poSrcFDefn = NULL);

    OGRErr              DeleteFromFilter( CPLString osOGCFilter );

    const std::vector<CPLString>& GetLastInsertedFIDList() { return aosFIDList; }

    const char         *GetShortName();

    void                SetRequiredOutputFormat(const char* pszRequiredOutputFormatIn);

    const char         *GetRequiredOutputFormat() { return pszRequiredOutputFormat; }

    void                SetOrderBy(const std::vector<OGRWFSSortDesc>& aoSortColumnsIn);
    bool                HasGotApproximateLayerDefn() { GetLayerDefn(); return bGotApproximateLayerDefn; }

    const char*         GetNamespacePrefix() { return pszNS; }
    const char*         GetNamespaceName() { return pszNSVal; }
};

/************************************************************************/
/*                          OGRWFSJoinLayer                             */
/************************************************************************/

class OGRWFSJoinLayer : public OGRLayer
{
    OGRWFSDataSource   *poDS;
    OGRFeatureDefn     *poFeatureDefn;

    CPLString           osGlobalFilter;
    CPLString           osSortBy;
    int                 bDistinct;
    std::set<CPLString> aoSetMD5;

    std::vector<OGRWFSLayer*> apoLayers;

    GDALDataset        *poBaseDS;
    OGRLayer           *poBaseLayer;
    bool                bReloadNeeded;
    bool                bHasFetched;

    bool                bPagingActive;
    int                 nPagingStartIndex;
    int                 nFeatureRead;
    int                 nFeatureCountRequested;

    std::vector<CPLString> aoSrcFieldNames;
    std::vector<CPLString> aoSrcGeomFieldNames;

    CPLString           osFeatureTypes;

                        OGRWFSJoinLayer(OGRWFSDataSource* poDS,
                                        const swq_select* psSelectInfo,
                                        const CPLString& osGlobalFilter);
    CPLString           MakeGetFeatureURL(int bRequestHits = FALSE);
    GDALDataset*        FetchGetFeature();
    GIntBig             ExecuteGetFeatureResultTypeHits();

    public:

    static OGRWFSJoinLayer* Build(OGRWFSDataSource* poDS,
                                  const swq_select* psSelectInfo);
                       virtual ~OGRWFSJoinLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature*         GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override;

    virtual int                 TestCapability( const char * ) override;

    virtual GIntBig             GetFeatureCount( int bForce = TRUE ) override;

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    virtual OGRErr      SetAttributeFilter( const char * ) override;
};

/************************************************************************/
/*                           OGRWFSDataSource                           */
/************************************************************************/

class OGRWFSDataSource : public OGRDataSource
{
    char*               pszName;
    bool                bRewriteFile;
    CPLXMLNode*         psFileXML;

    OGRWFSLayer**       papoLayers;
    int                 nLayers;
    std::map<OGRLayer*, OGRLayer*> oMap;

    bool                bUpdate;

    bool                bGetFeatureSupportHits;
    CPLString           osVersion;
    bool                bNeedNAMESPACE;
    bool                bHasMinOperators;
    bool                bHasNullCheck;
    bool                bPropertyIsNotEqualToSupported;
    bool                bUseFeatureId;
    bool                bGmlObjectIdNeedsGMLPrefix;
    bool                bRequiresEnvelopeSpatialFilter;
    static bool                DetectRequiresEnvelopeSpatialFilter( CPLXMLNode* psRoot );

    bool                bTransactionSupport;
    char**              papszIdGenMethods;
    bool                DetectTransactionSupport( CPLXMLNode* psRoot );

    CPLString           osBaseURL;
    CPLString           osPostTransactionURL;

    CPLXMLNode*         LoadFromFile( const char * pszFilename );

    bool                bUseHttp10;

    char**              papszHttpOptions;

    bool                bPagingAllowed;
    int                 nPageSize;
    int                 nBaseStartIndex;
    bool                DetectSupportPagingWFS2(CPLXMLNode* psRoot);

    bool                bStandardJoinsWFS2;
    bool                DetectSupportStandardJoinsWFS2( CPLXMLNode* psRoot );

    bool                bLoadMultipleLayerDefn;
    std::set<CPLString> aoSetAlreadyTriedLayers;

    CPLString           osLayerMetadataCSV;
    CPLString           osLayerMetadataTmpFileName;
    OGRDataSource      *poLayerMetadataDS;
    OGRLayer           *poLayerMetadataLayer;

    CPLString           osGetCapabilities;
    const char         *apszGetCapabilities[2];
    GDALDataset        *poLayerGetCapabilitiesDS;
    OGRLayer           *poLayerGetCapabilitiesLayer;

    bool                bKeepLayerNamePrefix;

    bool                bEmptyAsNull;

    bool                bInvertAxisOrderIfLatLong;
    CPLString           osConsiderEPSGAsURN;
    bool                bExposeGMLId;

    CPLHTTPResult*      SendGetCapabilities(const char* pszBaseURL,
                                            CPLString& osTypeName);

    int                 GetLayerIndex(const char* pszName);

  public:
                        OGRWFSDataSource();
                        virtual ~OGRWFSDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate,
                              char** papszOpenOptions );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;
    virtual OGRLayer*           GetLayerByName(const char* pszLayerName) override;

    virtual int                 TestCapability( const char * ) override;

    virtual OGRLayer *          ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect ) override;
    virtual void                ReleaseResultSet( OGRLayer * poResultsSet ) override;

    bool                        UpdateMode() const { return bUpdate; }
    bool                        SupportTransactions() const
        { return bTransactionSupport; }
    void                        DisableSupportHits() { bGetFeatureSupportHits = false; }
    bool                        GetFeatureSupportHits() const
        { return bGetFeatureSupportHits; }
    const char                 *GetVersion() { return osVersion.c_str(); }

    bool                        IsOldDeegree( const char* pszErrorString );
    bool                        GetNeedNAMESPACE() const { return bNeedNAMESPACE; }
    bool                        HasMinOperators() const { return bHasMinOperators; }
    bool                        HasNullCheck() const { return bHasNullCheck; }
    bool                        UseFeatureId() const { return bUseFeatureId; }
    bool                        RequiresEnvelopeSpatialFilter() const
        { return bRequiresEnvelopeSpatialFilter; }
    void                        SetGmlObjectIdNeedsGMLPrefix() { bGmlObjectIdNeedsGMLPrefix = true; }
    int                         DoesGmlObjectIdNeedGMLPrefix() const
        { return bGmlObjectIdNeedsGMLPrefix; }

    void                        SetPropertyIsNotEqualToUnSupported() { bPropertyIsNotEqualToSupported = false; }
    bool                        PropertyIsNotEqualToSupported() const
        { return bPropertyIsNotEqualToSupported; }

    CPLString                   GetPostTransactionURL();

    void                        SaveLayerSchema(const char* pszLayerName, CPLXMLNode* psSchema);

    CPLHTTPResult*              HTTPFetch( const char* pszURL, char** papszOptions );

    bool                        IsPagingAllowed() const { return bPagingAllowed; }
    int                         GetPageSize() const { return nPageSize; }
    int                         GetBaseStartIndex() const { return nBaseStartIndex; }

    void                        LoadMultipleLayerDefn(const char* pszLayerName,
                                                      char* pszNS, char* pszNSVal);

    bool                        GetKeepLayerNamePrefix() const
        { return bKeepLayerNamePrefix; }
    const CPLString&            GetBaseURL() { return osBaseURL; }

    bool                        IsEmptyAsNull() const
        { return bEmptyAsNull; }
    bool                        InvertAxisOrderIfLatLong() const
        { return bInvertAxisOrderIfLatLong; }
    const CPLString&            GetConsiderEPSGAsURN() const { return osConsiderEPSGAsURN; }

    bool                        ExposeGMLId() const { return bExposeGMLId; }

    virtual char**              GetMetadataDomainList() override;
    virtual char**              GetMetadata( const char * pszDomain = "" ) override;
};

#endif /* ndef OGR_WFS_H_INCLUDED */
