/******************************************************************************
 * $Id$
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM file based generic driver.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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

#include "gnm.h"

#define GNM_MD_DEFAULT_FILE_FORMAT  "ESRI Shapefile"

class GNMFileNetwork : public GNMGenericNetwork
{
public:
    GNMFileNetwork();
    virtual ~GNMFileNetwork();
    virtual CPLErr Open( GDALOpenInfo* poOpenInfo ) override;
    virtual CPLErr Delete() override;
    virtual int CloseDependentDatasets() override;
    virtual OGRErr      DeleteLayer(int) override;
    virtual CPLErr Create( const char* pszFilename, char** papszOptions ) override;
protected:
    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                   OGRSpatialReference *poSpatialRef = nullptr,
                                   OGRwkbGeometryType eGType = wkbUnknown,
                                   char ** papszOptions = nullptr ) override;
    virtual int CheckNetworkExist( const char* pszFilename, char** papszOptions ) override;
protected:
    virtual CPLErr CreateMetadataLayerFromFile( const char* pszFilename, int nVersion,
                                        char** papszOptions );
    virtual CPLErr StoreNetworkSrs() override;
    virtual CPLErr LoadNetworkSrs() override;
    virtual CPLErr DeleteMetadataLayer() override;
    virtual CPLErr CreateGraphLayerFromFile( const char* pszFilename,
                                     char** papszOptions );
    virtual CPLErr DeleteGraphLayer() override;
    virtual CPLErr CreateFeaturesLayerFromFile( const char* pszFilename,
                                        char** papszOptions );
    virtual CPLErr DeleteFeaturesLayer() override;
    virtual CPLErr DeleteNetworkLayers() override;
    virtual CPLErr LoadNetworkLayer(const char* pszLayername) override;
    virtual bool CheckStorageDriverSupport(const char* pszDriverName) override;
protected:
    CPLErr FormPath(const char* pszFilename, char** papszOptions);
protected:
    CPLString m_soNetworkFullName;
    GDALDataset* m_pMetadataDS;
    GDALDataset* m_pGraphDS;
    GDALDataset* m_pFeaturesDS;
    std::map<OGRLayer*, GDALDataset*> m_mpLayerDatasetMap;
};
