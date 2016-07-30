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

#include <memory>
#include <vector>
#include <string>

#include "ogrsf_frmts.h"

#include "libopencad/opencad_api.h"
#include "libopencad/cadgeometry.h"

class OGRCADLayer : public OGRLayer
{
    OGRFeatureDefn  *poFeatureDefn;

    OGRSpatialReference * poSpatialRef;

    GIntBig         nNextFID;

    CADLayer        &poCADLayer;
public:
    OGRCADLayer( CADLayer &poCADLayer, OGRSpatialReference *poSR );
    ~OGRCADLayer();

    void            ResetReading();
    OGRFeature      *GetNextFeature();
    OGRFeature      *GetFeature( GIntBig nFID );
    GIntBig         GetFeatureCount( int /* bForce */ );


    OGRSpatialReference *GetSpatialRef() { return poSpatialRef; }
    OGRFeatureDefn  *GetLayerDefn() { return poFeatureDefn; }

    std::vector< std::string > asFeaturesAttributes;
    int             TestCapability( const char * ) { return( FALSE ); }
};

class GDALCADDataset : public GDALDataset
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

public:
    GDALCADDataset();
    virtual ~GDALCADDataset();

    int            Open( GDALOpenInfo* poOpenInfo, CADFileIO* pFileIO, 
                            long nSubRasterLayer = -1, long nSubRasterFID = -1 );  
    int            GetLayerCount() { return nLayers; }
    OGRLayer      *GetLayer( int );
    int            TestCapability( const char * );
    virtual char **GetFileList();
    virtual const char  *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs(); 
    virtual int CloseDependentDatasets();

protected:
    OGRSpatialReference *GetSpatialReference();
    const char* GetPrjFilePath();
    void FillTransform(CADImage* pImage, double dfUnits);
private:
    CPL_DISALLOW_COPY_ASSIGN(GDALCADDataset);
};

CPLString CADRecode( const std::string& sString, int CADEncoding );

#endif
