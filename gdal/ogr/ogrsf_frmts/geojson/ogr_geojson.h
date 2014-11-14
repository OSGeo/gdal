/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definitions of OGR OGRGeoJSON driver types.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#ifndef OGR_GEOJSON_H_INCLUDED
#define OGR_GEOJSON_H_INCLUDED

#include "cpl_port.h"
#include <ogrsf_frmts.h>

#include <cstdio>
#include <vector> // used by OGRGeoJSONLayer
#include "ogrgeojsonutils.h"

#define SPACE_FOR_BBOX  80

class OGRGeoJSONDataSource;

/************************************************************************/
/*                           OGRGeoJSONLayer                            */
/************************************************************************/

class OGRGeoJSONLayer : public OGRLayer
{
public:

    static const char* const DefaultName;
    static const char* const DefaultFIDColumn;
    static const OGRwkbGeometryType DefaultGeometryType;
 
    OGRGeoJSONLayer( const char* pszName,
                     OGRSpatialReference* poSRS,
                     OGRwkbGeometryType eGType,
                     OGRGeoJSONDataSource* poDS );
    ~OGRGeoJSONLayer();

    //
    // OGRLayer Interface
    //
    OGRFeatureDefn* GetLayerDefn();
    
    int GetFeatureCount( int bForce = TRUE );
    void ResetReading();
    OGRFeature* GetNextFeature();
    int TestCapability( const char* pszCap );
    const char* GetFIDColumn();
    void SetFIDColumn( const char* pszFIDColumn );
    
    //
    // OGRGeoJSONLayer Interface
    //
    void AddFeature( OGRFeature* poFeature );
    void DetectGeometryType();

private:

    typedef std::vector<OGRFeature*> FeaturesSeq;
    FeaturesSeq seqFeatures_;
    FeaturesSeq::iterator iterCurrent_;


    // CPL_UNUSED OGRGeoJSONDataSource* poDS_;
    OGRFeatureDefn* poFeatureDefn_;
    CPLString sFIDColumn_;
};

/************************************************************************/
/*                         OGRGeoJSONWriteLayer                         */
/************************************************************************/

class OGRGeoJSONWriteLayer : public OGRLayer
{
public:
    OGRGeoJSONWriteLayer( const char* pszName,
                     OGRwkbGeometryType eGType,
                     char** papszOptions,
                     OGRGeoJSONDataSource* poDS );
    ~OGRGeoJSONWriteLayer();

    //
    // OGRLayer Interface
    //
    OGRFeatureDefn* GetLayerDefn() { return poFeatureDefn_; }
    OGRSpatialReference* GetSpatialRef() { return NULL; }

    void ResetReading() { }
    OGRFeature* GetNextFeature() { return NULL; }
    OGRErr ICreateFeature( OGRFeature* poFeature );
    OGRErr CreateField(OGRFieldDefn* poField, int bApproxOK);
    int TestCapability( const char* pszCap );

private:

    OGRGeoJSONDataSource* poDS_;
    OGRFeatureDefn* poFeatureDefn_;
    int nOutCounter_;

    int bWriteBBOX;
    int bBBOX3D;
    OGREnvelope3D sEnvelopeLayer;

    int nCoordPrecision;
};

/************************************************************************/
/*                           OGRGeoJSONDataSource                       */
/************************************************************************/

class OGRGeoJSONDataSource : public OGRDataSource
{
public:

    OGRGeoJSONDataSource();
    ~OGRGeoJSONDataSource();

    //
    // OGRDataSource Interface
    //
    int Open( GDALOpenInfo* poOpenInfo,
               GeoJSONSourceType nSrcType );
    const char* GetName();
    int GetLayerCount();
    OGRLayer* GetLayer( int nLayer );
    OGRLayer* ICreateLayer( const char* pszName,
                           OGRSpatialReference* poSRS = NULL,
                           OGRwkbGeometryType eGType = wkbUnknown,
                           char** papszOptions = NULL );
    int TestCapability( const char* pszCap );
    
    void AddLayer( OGRGeoJSONLayer* poLayer );

    //
    // OGRGeoJSONDataSource Interface
    //
    int Create( const char* pszName, char** papszOptions );
    VSILFILE* GetOutputFile() const { return fpOut_; }

    enum GeometryTranslation
    {
        eGeometryPreserve,
        eGeometryAsCollection,
    };
    
    void SetGeometryTranslation( GeometryTranslation type );

    enum AttributesTranslation
    {
        eAtributesPreserve,
        eAtributesSkip
    };

    void SetAttributesTranslation( AttributesTranslation type );

    int  GetFpOutputIsSeekable() const { return bFpOutputIsSeekable_; }
    int  GetBBOXInsertLocation() const { return nBBOXInsertLocation_; }

private:

    //
    // Private data members
    //
    char* pszName_;
    char* pszGeoData_;
    OGRLayer** papoLayers_;
    int nLayers_;
    VSILFILE* fpOut_;
    
    //
    // Translation/Creation control flags
    // 
    GeometryTranslation flTransGeom_;
    AttributesTranslation flTransAttrs_;

    int bFpOutputIsSeekable_;
    int nBBOXInsertLocation_;

    //
    // Priavte utility functions
    //
    void Clear();
    int ReadFromFile( GDALOpenInfo* poOpenInfo );
    int ReadFromService( const char* pszSource );
    void LoadLayers();
};


#endif /* OGR_GEOJSON_H_INCLUDED */
