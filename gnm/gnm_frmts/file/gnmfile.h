/******************************************************************************
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

#ifndef GNMFILE_H_INCLUDED
#define GNMFILE_H_INCLUDED

#include "gnm.h"

#define GNM_MD_DEFAULT_FILE_FORMAT "ESRI Shapefile"

class GNMFileNetwork : public GNMGenericNetwork
{
  public:
    GNMFileNetwork();
    ~GNMFileNetwork() override;
    CPLErr Open(GDALOpenInfo *poOpenInfo) override;
    CPLErr Delete() override;
    int CloseDependentDatasets() override;
    OGRErr DeleteLayer(int) override;
    virtual CPLErr Create(const char *pszFilename,
                          CSLConstList papszOptions) override;

  protected:
    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    virtual int CheckNetworkExist(const char *pszFilename,
                                  CSLConstList papszOptions) override;

  protected:
    virtual CPLErr CreateMetadataLayerFromFile(const char *pszFilename,
                                               int nVersion,
                                               CSLConstList papszOptions);
    CPLErr StoreNetworkSrs() override;
    CPLErr LoadNetworkSrs() override;
    CPLErr DeleteMetadataLayer() override;
    virtual CPLErr CreateGraphLayerFromFile(const char *pszFilename,
                                            CSLConstList papszOptions);
    CPLErr DeleteGraphLayer() override;
    virtual CPLErr CreateFeaturesLayerFromFile(const char *pszFilename,
                                               CSLConstList papszOptions);
    CPLErr DeleteFeaturesLayer() override;
    CPLErr DeleteNetworkLayers() override;
    CPLErr LoadNetworkLayer(const char *pszLayername) override;
    bool CheckStorageDriverSupport(const char *pszDriverName) override;

  protected:
    CPLErr FormPath(const char *pszFilename, CSLConstList papszOptions);

  protected:
    CPLString m_soNetworkFullName;
    GDALDataset *m_pMetadataDS;
    GDALDataset *m_pGraphDS;
    GDALDataset *m_pFeaturesDS;
    std::map<OGRLayer *, GDALDataset *> m_mpLayerDatasetMap;
};

#endif
