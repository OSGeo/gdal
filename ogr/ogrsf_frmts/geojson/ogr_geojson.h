/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definitions of OGR OGRGeoJSON driver types.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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

#include <ogrsf_frmts.h>
#include <cstdio>
#include <vector> // used by OGRGeoJSONLayer

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
    OGRSpatialReference* GetSpatialRef();
    
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
    void SetSpatialRef( OGRSpatialReference* poSRS );
    void DetectGeometryType();

private:

    typedef std::vector<OGRFeature*> FeaturesSeq;
    FeaturesSeq seqFeatures_;
    FeaturesSeq::iterator iterCurrent_;

    OGRGeoJSONDataSource* poDS_;
    OGRFeatureDefn* poFeatureDefn_;
    OGRSpatialReference* poSRS_;
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
    OGRErr CreateFeature( OGRFeature* poFeature );
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
    int Open( const char* pszSource );
    const char* GetName();
    int GetLayerCount();
    OGRLayer* GetLayer( int nLayer );
    OGRLayer* CreateLayer( const char* pszName,
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
    int ReadFromFile( const char* pszSource, VSILFILE* fpIn );
    int ReadFromService( const char* pszSource );
    void LoadLayers();
};


/************************************************************************/
/*                           OGRGeoJSONDriver                           */
/************************************************************************/

class OGRGeoJSONDriver : public OGRSFDriver
{
public:

    OGRGeoJSONDriver();
    ~OGRGeoJSONDriver();

    //
    // OGRSFDriver Interface
    //
    const char* GetName();
    OGRDataSource* Open( const char* pszName, int bUpdate );
    OGRDataSource* CreateDataSource( const char* pszName, char** papszOptions );
    OGRErr DeleteDataSource( const char* pszName );
    int TestCapability( const char* pszCap );

    //
    // OGRGeoJSONDriver Interface
    //
    // NOTE: New version of Open() based on Andrey's RFC 10.
    OGRDataSource* Open( const char* pszName, int bUpdate,
                         char** papszOptions );

};

#endif /* OGR_GEOJSON_H_INCLUDED */

