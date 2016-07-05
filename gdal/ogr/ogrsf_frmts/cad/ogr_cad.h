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
    OGRCADLayer( CADLayer &poCADLayer, std::string sESRISpatRef );
    ~OGRCADLayer();
    
    void            ResetReading();
    OGRFeature      *GetNextFeature();
    OGRFeature      *GetFeature( GIntBig nFID );
    GIntBig         GetFeatureCount( int /* bForce */ );
    
    
    OGRSpatialReference *GetSpatialRef() { return poSpatialRef; }
    OGRFeatureDefn  *GetLayerDefn() { return poFeatureDefn; }
    
    int             TestCapability( const char * ) { return( FALSE ); }
};

class OGRCADDataSource : public GDALDataset
{
    CADFile       *poCADFile;    
    OGRCADLayer  **papoLayers;
    int            nLayers;
    
public:
    OGRCADDataSource();
    ~OGRCADDataSource();
    
    int            Open( GDALOpenInfo* poOpenInfo, CADFileIO* pFileIO );    
    int            GetLayerCount() { return nLayers; }
    OGRLayer      *GetLayer( int );    
    int            TestCapability( const char * );        
};

#endif
