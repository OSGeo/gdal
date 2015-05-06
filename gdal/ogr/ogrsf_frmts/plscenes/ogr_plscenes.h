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

#ifndef _OGR_PLSCENES_H_INCLUDED
#define _OGR_PLSCENES_H_INCLUDED

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_srs_api.h"
#include "cpl_http.h"
#include <json.h>
#include "ogr_geojson.h"
#include "ogrgeojsonreader.h"
#include "swq.h"
#include <map>

class OGRPLScenesLayer;
class OGRPLScenesDataset: public GDALDataset
{
        int             bMustCleanPersistant;
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

        static int          Identify(GDALOpenInfo* poOpenInfo);
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
        virtual OGRErr      SetAttributeFilter( const char * );
        
        virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce );
        
        void                SetMainFilterRect(double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY);
        void                SetAcquiredOrderingFlag(int bAcquiredAscendingIn)
                                { bAcquiredAscending = bAcquiredAscendingIn; }
};

#endif /* ndef _OGR_PLSCENES_H_INCLUDED */

