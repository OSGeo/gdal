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
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#ifndef OGR_CAD_H_INCLUDED
#define OGR_CAD_H_INCLUDED

// gdal headers
#include "ogrsf_frmts.h"

// libopencad headers
#include "cadgeometry.h"
#include "opencad_api.h"

#include <set>

class OGRCADLayer final: public OGRLayer
{
    OGRFeatureDefn  *poFeatureDefn;
    OGRSpatialReference * poSpatialRef;
    GIntBig         nNextFID;
    CADLayer        &poCADLayer;
    int             nDWGEncoding;

public:
    OGRCADLayer( CADLayer &poCADLayer, OGRSpatialReference *poSR, int nEncoding );
    ~OGRCADLayer();

    void            ResetReading() override;
    OGRFeature      *GetNextFeature() override;
    OGRFeature      *GetFeature( GIntBig nFID ) override;
    GIntBig         GetFeatureCount( int /* bForce */ ) override;
    OGRSpatialReference *GetSpatialRef() override { return poSpatialRef; }
    OGRFeatureDefn  *GetLayerDefn() override { return poFeatureDefn; }
    std::set< CPLString > asFeaturesAttributes;
    int             TestCapability( const char * ) override { return( FALSE ); }
};

class GDALCADDataset final: public GDALDataset
{
    CPLString      osCADFilename;
    CADFile       *poCADFile;
    // vector
    OGRCADLayer  **papoLayers;
    int            nLayers;
    // raster
    CPLString      soWKT;
    double         adfGeoTransform[6];
    GDALDataset   *poRasterDS;
    OGRSpatialReference *poSpatialReference;

public:
    GDALCADDataset();
    virtual ~GDALCADDataset();

    int            Open( GDALOpenInfo* poOpenInfo, CADFileIO* pFileIO,
                            long nSubRasterLayer = -1, long nSubRasterFID = -1 );
    int            GetLayerCount() override { return nLayers; }
    OGRLayer      *GetLayer( int ) override;
    int            TestCapability( const char * ) override;
    virtual char **GetFileList() override;
    virtual const char  *_GetProjectionRef(void) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    virtual CPLErr GetGeoTransform( double * ) override;
    virtual int    GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual int CloseDependentDatasets() override;

protected:
    OGRSpatialReference *GetSpatialReference();
    const char* GetPrjFilePath();
    void FillTransform(CADImage* pImage, double dfUnits);
    int GetCadEncoding() const;
private:
    CPL_DISALLOW_COPY_ASSIGN(GDALCADDataset)
};

CPLString CADRecode( const CPLString& sString, int CADEncoding );

#endif
