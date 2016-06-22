/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
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
    
    GIntBig         nNextFID;

    CADLayer        &poCADLayer;
public:
    OGRCADLayer( CADLayer &poCADLayer );
    ~OGRCADLayer();
    
    void            ResetReading();
    OGRFeature      *GetNextFeature();
    OGRFeature      *GetFeature( GIntBig nFID );
    GIntBig         GetFeatureCount( int bForce );
    
    
    OGRFeatureDefn  *GetLayerDefn() { return poFeatureDefn; }
    
    int             TestCapability( const char * ) { return( FALSE ); }
};

class OGRCADDataSource : public GDALDataset
{
    std::unique_ptr<CADFile>    spoCADFile;
    
    OGRCADLayer                 **papoLayers;
    size_t                      nLayers;
    
public:
    OGRCADDataSource();
    ~OGRCADDataSource();
    
    int             Open( const char * pszFilename, int bUpdate );
    
    int             GetLayerCount() { return nLayers; }
    OGRLayer        *GetLayer( int );
    
    int             TestCapability( const char * ) { return( FALSE ); }
};

#endif