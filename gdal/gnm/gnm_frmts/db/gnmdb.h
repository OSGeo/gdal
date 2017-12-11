/******************************************************************************
 * $Id$
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM db based generic driver.
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

class GNMDatabaseNetwork : public GNMGenericNetwork
{
public:
    GNMDatabaseNetwork();
    virtual ~GNMDatabaseNetwork();
    virtual CPLErr Open( GDALOpenInfo* poOpenInfo ) override;
    virtual OGRErr      DeleteLayer(int) override;
    virtual CPLErr Create( const char* pszFilename, char** papszOptions ) override;
protected:
    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                   OGRSpatialReference *poSpatialRef = nullptr,
                                   OGRwkbGeometryType eGType = wkbUnknown,
                                   char ** papszOptions = nullptr ) override;
    virtual int CheckNetworkExist( const char* pszFilename, char** papszOptions ) override;
protected:
    virtual CPLErr DeleteMetadataLayer() override;
    virtual CPLErr DeleteGraphLayer() override;
    virtual CPLErr DeleteFeaturesLayer() override;
    virtual CPLErr DeleteNetworkLayers() override;
    virtual CPLErr LoadNetworkLayer(const char* pszLayername) override;
    virtual bool CheckStorageDriverSupport(const char* pszDriverName) override;
protected:
    CPLErr FormName(const char* pszFilename, char** papszOptions);
    CPLErr DeleteLayerByName(const char* pszLayerName);
protected:
    GDALDataset* m_poDS;
    CPLString m_soNetworkFullName;
};
