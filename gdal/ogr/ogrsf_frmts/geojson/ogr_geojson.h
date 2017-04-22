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
#include "../mem/ogr_mem.h"

#include <cstdio>
#include <vector>  // Used by OGRGeoJSONLayer.
#include "ogrgeojsonutils.h"
#include "ogrgeojsonwriter.h"

class OGRGeoJSONDataSource;

/************************************************************************/
/*                           OGRGeoJSONLayer                            */
/************************************************************************/

class OGRGeoJSONLayer : public OGRMemLayer
{
    friend class OGRGeoJSONDataSource;

  public:
    static const char* const DefaultName;
    static const OGRwkbGeometryType DefaultGeometryType;

    OGRGeoJSONLayer( const char* pszName,
                     OGRSpatialReference* poSRS,
                     OGRwkbGeometryType eGType,
                     OGRGeoJSONDataSource* poDS );
    virtual ~OGRGeoJSONLayer();

    //
    // OGRLayer Interface
    //
    virtual const char* GetFIDColumn() override;
    virtual int         TestCapability( const char * pszCap ) override;

    virtual OGRErr      SyncToDisk() override;
    //
    // OGRGeoJSONLayer Interface
    //
    void SetFIDColumn( const char* pszFIDColumn );
    void AddFeature( OGRFeature* poFeature );
    void DetectGeometryType();

  private:
    OGRGeoJSONDataSource* poDS_;
    CPLString sFIDColumn_;
    bool bUpdated_;
    bool bOriginalIdModified_;
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
                          bool bWriteFC_BBOXIn,
                          OGRCoordinateTransformation* poCT,
                          OGRGeoJSONDataSource* poDS );
    ~OGRGeoJSONWriteLayer();

    //
    // OGRLayer Interface
    //
    OGRFeatureDefn* GetLayerDefn() override { return poFeatureDefn_; }
    OGRSpatialReference* GetSpatialRef() override { return NULL; }

    void ResetReading() override { }
    OGRFeature* GetNextFeature() override { return NULL; }
    OGRErr ICreateFeature( OGRFeature* poFeature ) override;
    OGRErr CreateField( OGRFieldDefn* poField, int bApproxOK ) override;
    int TestCapability( const char* pszCap ) override;

  private:
    OGRGeoJSONDataSource* poDS_;
    OGRFeatureDefn* poFeatureDefn_;
    int nOutCounter_;

    bool bWriteBBOX;
    bool bBBOX3D;
    bool bWriteFC_BBOX;
    OGREnvelope3D sEnvelopeLayer;

    int nCoordPrecision_;
    int nSignificantFigures_;

    bool bRFC7946_;
    OGRCoordinateTransformation* poCT_;
    OGRGeoJSONWriteOptions oWriteOptions_;
};

/************************************************************************/
/*                           OGRGeoJSONDataSource                       */
/************************************************************************/

class OGRGeoJSONDataSource : public OGRDataSource
{
  public:
    OGRGeoJSONDataSource();
    virtual ~OGRGeoJSONDataSource();

    //
    // OGRDataSource Interface
    //
    int Open( GDALOpenInfo* poOpenInfo,
               GeoJSONSourceType nSrcType );
    const char* GetName() override;
    int GetLayerCount() override;
    OGRLayer* GetLayer( int nLayer ) override;
    OGRLayer* ICreateLayer( const char* pszName,
                            OGRSpatialReference* poSRS = NULL,
                            OGRwkbGeometryType eGType = wkbUnknown,
                            char** papszOptions = NULL ) override;
    int TestCapability( const char* pszCap ) override;

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
        eAttributesPreserve,
        eAttributesSkip
    };

    void SetAttributesTranslation( AttributesTranslation type );

    int  GetFpOutputIsSeekable() const { return bFpOutputIsSeekable_; }
    int  GetBBOXInsertLocation() const { return nBBOXInsertLocation_; }
    int  HasOtherPages() const { return bOtherPages_; }
    bool IsUpdatable() const { return bUpdatable_; }

    virtual void        FlushCache() override;

    static const size_t SPACE_FOR_BBOX = 130;

  private:
    //
    // Private data members
    //
    char* pszName_;
    char* pszGeoData_;
    vsi_l_offset nGeoDataLen_;
    OGRGeoJSONLayer** papoLayers_;
    OGRGeoJSONWriteLayer** papoLayersWriter_;
    int nLayers_;
    VSILFILE* fpOut_;

    //
    // Translation/Creation control flags
    //
    GeometryTranslation flTransGeom_;
    AttributesTranslation flTransAttrs_;
    bool bOtherPages_;  // ESRI Feature Service specific.

    bool bFpOutputIsSeekable_;
    int nBBOXInsertLocation_;

    bool bUpdatable_;

    //
    // Private utility functions
    //
    void Clear();
    int ReadFromFile( GDALOpenInfo* poOpenInfo );
    int ReadFromService( const char* pszSource );
    void LoadLayers(char** papszOpenOptions);
};

#endif  // OGR_GEOJSON_H_INCLUDED
