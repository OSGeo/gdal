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
#include "ogr_geojson.h"
#include "ogrgeojsonreader.h"
#include "ogr_swq.h"
#include <map>
#include <set>
#include <vector>

class OGRPLScenesDataV1Layer;
class OGRPLScenesDataV1Dataset final: public GDALDataset
{
        bool            m_bLayerListInitialized;
        bool            m_bMustCleanPersistent;
        CPLString       m_osBaseURL;
        CPLString       m_osAPIKey;
        CPLString       m_osNextItemTypesPageURL;
        CPLString       m_osFilter;

        int                   m_nLayers;
        OGRPLScenesDataV1Layer  **m_papoLayers;

        bool            m_bFollowLinks;

        char              **GetBaseHTTPOptions();
        OGRLayer           *ParseItemType(json_object* poItemType);
        bool                ParseItemTypes(json_object* poObj,
                                              CPLString& osNext);
        void                EstablishLayerList();
        GDALDataset       *OpenRasterScene(GDALOpenInfo* poOpenInfo,
                                           CPLString osScene,
                                           char** papszOptions);
        CPLString           InsertAPIKeyInURL(CPLString osURL);

    public:
                            OGRPLScenesDataV1Dataset();
                           virtual ~OGRPLScenesDataV1Dataset();

        virtual int         GetLayerCount() override;
        virtual OGRLayer   *GetLayer(int idx) override;
        virtual OGRLayer   *GetLayerByName(const char* pszName) override;

        json_object        *RunRequest(const char* pszURL,
                                       int bQuiet404Error = FALSE,
                                       const char* pszHTTPVerb = "GET",
                                       bool bExpectJSonReturn = true,
                                       const char* pszPostContent = nullptr);

        bool                DoesFollowLinks() const { return m_bFollowLinks; }
        const CPLString&    GetFilter() const { return m_osFilter; }
        const CPLString&    GetBaseURL() const { return m_osBaseURL; }

        static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
};

class OGRPLScenesDataV1FeatureDefn final: public OGRFeatureDefn
{
            OGRPLScenesDataV1Layer* m_poLayer;

    public:
        OGRPLScenesDataV1FeatureDefn(OGRPLScenesDataV1Layer* poLayer,
                                 const char* pszName):
                            OGRFeatureDefn(pszName), m_poLayer(poLayer) {}
       ~OGRPLScenesDataV1FeatureDefn() {}

       virtual int GetFieldCount() const override;

       void DropRefToLayer() { m_poLayer = nullptr; }
};

class OGRPLScenesDataV1Layer final: public OGRLayer
{
            friend class OGRPLScenesDataV1Dataset;
            friend class OGRPLScenesDataV1FeatureDefn;

            OGRPLScenesDataV1Dataset* m_poDS;
            bool                  m_bFeatureDefnEstablished;
            OGRPLScenesDataV1FeatureDefn* m_poFeatureDefn;
            OGRSpatialReference*  m_poSRS;
            GIntBig               m_nTotalFeatures;
            std::map<CPLString, int> m_oMapPrefixedJSonFieldNameToFieldIdx;
            std::map<int,CPLString>  m_oMapFieldIdxToQueryableJSonFieldName;

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

            json_object*          m_poAttributeFilter;
            bool                  m_bFilterMustBeClientSideEvaluated;

            std::set<CPLString>   m_oSetAssets;
            std::set<CPLString>   m_oSetUnregisteredAssets;
            std::set<CPLString>   m_oSetUnregisteredFields;

            OGRFeature           *GetNextRawFeature();
            bool                  SetFieldFromPrefixedJSonFieldName(
                                        OGRFeature* poFeature,
                                        const CPLString& osPrefixedJSonFieldName,
                                        json_object* poVal );
            void                  EstablishLayerDefn();
            void                  RegisterField(OGRFieldDefn* poFieldDefn,
                                                const char* pszQueryableJSonName,
                                                const char* pszPrefixedJSonName);
            bool                  GetNextPage();
            json_object*          BuildFilter(swq_expr_node* poNode);
            bool                  IsSimpleComparison(const swq_expr_node* poNode);

    public:
                            OGRPLScenesDataV1Layer(OGRPLScenesDataV1Dataset* poDS,
                                                   const char* pszName);
                           virtual ~OGRPLScenesDataV1Layer();

        virtual void            ResetReading() override;
        virtual OGRFeature     *GetNextFeature() override;
        virtual int             TestCapability(const char*) override;
        virtual OGRFeatureDefn *GetLayerDefn() override;
        virtual GIntBig         GetFeatureCount(int bForce = FALSE) override;

        virtual char      **GetMetadata( const char * pszDomain = "" ) override;
        virtual const char *GetMetadataItem( const char * pszName, const char* pszDomain = "" ) override;

        virtual void        SetSpatialFilter( OGRGeometry *poGeom ) override;
        virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

        virtual OGRErr      SetAttributeFilter( const char * ) override;

        virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) override;
        virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
};

#endif /* ndef OGR_PLSCENES_H_INCLUDED */
