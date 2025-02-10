/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, polimax@mail.ru
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016, NextGIS
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/
#ifndef OGR_CAD_H_INCLUDED
#define OGR_CAD_H_INCLUDED

// gdal headers
#include "ogrsf_frmts.h"

// libopencad headers
#include "cadgeometry.h"
#include "opencad_api.h"

#include <set>

class OGRCADLayer final : public OGRLayer
{
    GDALDataset *m_poDS = nullptr;
    OGRFeatureDefn *poFeatureDefn;
    OGRSpatialReference *poSpatialRef;
    GIntBig nNextFID;
    CADLayer &poCADLayer;
    int nDWGEncoding;

  public:
    OGRCADLayer(GDALDataset *poDS, CADLayer &poCADLayer,
                OGRSpatialReference *poSR, int nEncoding);
    ~OGRCADLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFID) override;
    GIntBig GetFeatureCount(int /* bForce */) override;

    OGRSpatialReference *GetSpatialRef() override
    {
        return poSpatialRef;
    }

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    std::set<CPLString> asFeaturesAttributes;
    int TestCapability(const char *) override;

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
};

class GDALCADDataset final : public GDALDataset
{
    CPLString osCADFilename;
    CADFile *poCADFile;
    // vector
    OGRCADLayer **papoLayers;
    int nLayers;
    // raster
    double adfGeoTransform[6];
    GDALDataset *poRasterDS;
    mutable OGRSpatialReference *poSpatialReference;

  public:
    GDALCADDataset();
    virtual ~GDALCADDataset();

    int Open(GDALOpenInfo *poOpenInfo, CADFileIO *pFileIO,
             long nSubRasterLayer = -1, long nSubRasterFID = -1);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;
    int TestCapability(const char *) override;
    virtual char **GetFileList() override;
    const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform(double *) override;
    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual int CloseDependentDatasets() override;

  protected:
    const std::string GetPrjFilePath() const;
    void FillTransform(CADImage *pImage, double dfUnits);
    int GetCadEncoding() const;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALCADDataset)
};

CPLString CADRecode(const CPLString &sString, int CADEncoding);

#endif
