/******************************************************************************
 * $Id$
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  PLScenes driver interface
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Planet Labs
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

#ifndef OGR_PLSCENES_H_INCLUDED
#define OGR_PLSCENES_H_INCLUDED

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_srs_api.h"
#include "cpl_http.h"
#include <json.h>
#include "ogr_geojson.h"
#include "ogrgeojsonreader.h"
#include "swq.h"
#include <map>
#include <set>
#include <vector>

class OGRPLScenesLayer;
class OGRPLScenesDataset: public GDALDataset
{
        int             bMustCleanPersistent;
        CPLString       osBaseURL;
        CPLString       osAPIKey;

        int             nLayers;
        OGRPLScenesLayer  **papoLayers;
        std::map<OGRLayer*, OGRPLScenesLayer*> oMapResultSetToSourceLayer;

        char             **GetBaseHTTPOptions();
        GDALDataset       *OpenRasterScene(GDALOpenInfo* poOpenInfo,
                                           CPLString osScene,
                                           char** papszOptions);

    public:
                            OGRPLScenesDataset();
                           ~OGRPLScenesDataset();

        virtual int         GetLayerCount() { return nLayers; }
        virtual OGRLayer   *GetLayer(int idx);
        virtual OGRLayer   *GetLayerByName(const char* pszName);
        virtual OGRLayer   *ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect );
        virtual void        ReleaseResultSet( OGRLayer * poLayer );

        json_object        *RunRequest(const char* pszURL,
                                       int bQuiet404Error = FALSE);

        static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
};

class OGRPLScenesLayer: public OGRLayer
{
            friend class OGRPLScenesDataset;

            OGRPLScenesDataset* poDS;
            CPLString       osBaseURL;
            OGRFeatureDefn* poFeatureDefn;
            OGRSpatialReference* poSRS;
            int             bEOF;
            GIntBig         nNextFID;
            GIntBig         nFeatureCount;
            CPLString       osNextURL;
            CPLString       osRequestURL;
            CPLString       osQuery;

            OGRGeoJSONDataSource *poGeoJSONDS;
            OGRLayer             *poGeoJSONLayer;

            OGRGeometry    *poMainFilter;

            int             nPageSize;
            int             bStillInFirstPage;
            int             bAcquiredAscending;

            int             bFilterMustBeClientSideEvaluated;
            CPLString       osFilterURLPart;

            OGRFeature     *GetNextRawFeature();
            CPLString       BuildURL(int nFeatures);
            int             GetNextPage();
            CPLString       BuildFilter(swq_expr_node* poNode);

    public:
                            OGRPLScenesLayer(OGRPLScenesDataset* poDS,
                                             const char* pszName,
                                             const char* pszBaseURL,
                                             json_object* poObjCount10 = NULL);
                           ~OGRPLScenesLayer();

        virtual void            ResetReading();
        virtual GIntBig         GetFeatureCount(int bForce = FALSE);
        virtual OGRFeature     *GetNextFeature();
        virtual int             TestCapability(const char*);
        virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }

        virtual void        SetSpatialFilter( OGRGeometry *poGeom );
        virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom )
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

        virtual OGRErr      SetAttributeFilter( const char * );

        virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
        virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

        void                SetMainFilterRect(double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY);
        void                SetAcquiredOrderingFlag(int bAcquiredAscendingIn)
                                { bAcquiredAscending = bAcquiredAscendingIn; }
};

class OGRPLScenesV1Layer;
class OGRPLScenesV1Dataset: public GDALDataset
{
        bool            m_bLayerListInitialized;
        bool            m_bMustCleanPersistent;
        CPLString       m_osBaseURL;
        CPLString       m_osAPIKey;
        CPLString       m_osNextCatalogPageURL;
        CPLString       m_osFilter;

        int                   m_nLayers;
        OGRPLScenesV1Layer  **m_papoLayers;

        bool            m_bFollowLinks;

        char              **GetBaseHTTPOptions();
        OGRLayer           *ParseCatalog(json_object* poCatalog);
        bool                ParseCatalogsPage(json_object* poObj,
                                              CPLString& osNext);
        void                EstablishLayerList();
        GDALDataset       *OpenRasterScene(GDALOpenInfo* poOpenInfo,
                                           CPLString osScene,
                                           char** papszOptions);
        CPLString           InsertAPIKeyInURL(CPLString osURL);

    public:
                            OGRPLScenesV1Dataset();
                           ~OGRPLScenesV1Dataset();

        virtual int         GetLayerCount();
        virtual OGRLayer   *GetLayer(int idx);
        virtual OGRLayer   *GetLayerByName(const char* pszName);

        json_object        *RunRequest(const char* pszURL,
                                       int bQuiet404Error = FALSE,
                                       const char* pszHTTPVerb = "GET",
                                       bool bExpectJSonReturn = true,
                                       const char* pszPostContent = NULL);

        bool                DoesFollowLinks() const { return m_bFollowLinks; }
        const CPLString&    GetFilter() const { return m_osFilter; }
        const CPLString&    GetBaseURL() const { return m_osBaseURL; }

        static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
};

class OGRPLScenesV1FeatureDefn: public OGRFeatureDefn
{
            OGRPLScenesV1Layer* m_poLayer;

    public:
        OGRPLScenesV1FeatureDefn(OGRPLScenesV1Layer* poLayer,
                                 const char* pszName):
                            OGRFeatureDefn(pszName), m_poLayer(poLayer) {}
       ~OGRPLScenesV1FeatureDefn() {}

       virtual int GetFieldCount();

       void DropRefToLayer() { m_poLayer = NULL; }
};

struct OGRPLScenesV1LayerExprComparator;

class OGRPLScenesV1Layer: public OGRLayer
{
            friend class OGRPLScenesV1Dataset;
            friend class OGRPLScenesV1FeatureDefn;
            friend struct OGRPLScenesV1LayerExprComparator;

            OGRPLScenesV1Dataset* m_poDS;
            bool                  m_bFeatureDefnEstablished;
            OGRPLScenesV1FeatureDefn* m_poFeatureDefn;
            OGRSpatialReference*  m_poSRS;
            CPLString             m_osSpecURL;
            CPLString             m_osItemsURL;
            GIntBig               m_nTotalFeatures;
            std::vector<CPLString> m_aoAssetCategories;
            std::map<CPLString, int> m_oMapPrefixedJSonFieldNameToFieldIdx;
            std::map<int,CPLString>  m_oMapFieldIdxToQueriableJSonFieldName;
            std::set<CPLString>   m_oSetQueriable;

            GIntBig               m_nNextFID;
            bool                  m_bEOF;
            bool                  m_bStillInFirstPage;
            CPLString             m_osNextURL;
            CPLString             m_osRequestURL;
            int                   m_nPageSize;
            bool                  m_bInFeatureCountOrGetExtent;

            json_object          *m_poPageObj;
            json_object          *m_poFeatures;
            int                   m_nFeatureIdx;

            CPLString             m_osFilterURLPart;
            bool                  m_bFilterMustBeClientSideEvaluated;

            OGRFeature           *GetNextRawFeature();
            void                  SetFieldFromPrefixedJSonFieldName(
                                        OGRFeature* poFeature,
                                        const CPLString& osPrefixedJSonFieldName,
                                        json_object* poVal );
            void                  EstablishLayerDefn();
            void                  RegisterField(OGRFieldDefn* poFieldDefn,
                                                const char* pszQueriableJSonName,
                                                const char* pszPrefixedJSonName);
            static json_object*   ResolveRefIfNecessary(json_object* poObj,
                                                       json_object* poMain);
            void                  ParseProperties(json_object* poProperties,
                                                  json_object* poSpec,
                                                  CPLString& osPropertiesDesc,
                                                  const char* pszCategory);
            void                  ParseAssetProperties(
                                              json_object* poSpec,
                                              CPLString& osPropertiesDesc);
            void                  ProcessAssetFileProperties( json_object* poPropertiesAssetFile,
                                                     const CPLString& osAssetCategory,
                                                     CPLString& osPropertiesDesc );
            bool                  GetNextPage();
            CPLString             BuildRequestURL();
            CPLString             BuildFilter(swq_expr_node* poNode);
            bool                  IsSimpleComparison(const swq_expr_node* poNode);
            void                  FlattendAndOperands(swq_expr_node* poNode,
                                                      std::vector<swq_expr_node*>& oVector);
    public:
                            OGRPLScenesV1Layer(OGRPLScenesV1Dataset* poDS,
                                               const char* pszName,
                                               const char* pszSpecURL,
                                               const char* pszItemsURL,
                                               GIntBig nCount);
                           ~OGRPLScenesV1Layer();

        virtual void            ResetReading();
        virtual OGRFeature     *GetNextFeature();
        virtual int             TestCapability(const char*);
        virtual OGRFeatureDefn *GetLayerDefn();
        virtual GIntBig         GetFeatureCount(int bForce = FALSE);

        virtual char      **GetMetadata( const char * pszDomain = "" );
        virtual const char *GetMetadataItem( const char * pszName, const char* pszDomain = "" );

        virtual void        SetSpatialFilter( OGRGeometry *poGeom );
        virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom )
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

        virtual OGRErr      SetAttributeFilter( const char * );

        virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
        virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

};

#endif /* ndef OGR_PLSCENES_H_INCLUDED */
