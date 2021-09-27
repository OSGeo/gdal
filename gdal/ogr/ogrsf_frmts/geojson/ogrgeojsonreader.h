/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines GeoJSON reader within OGR OGRGeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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
#ifndef OGR_GEOJSONREADER_H_INCLUDED
#define OGR_GEOJSONREADER_H_INCLUDED

#include "cpl_json_header.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogrsf_frmts.h"

#include "ogrgeojsonutils.h"
#include "directedacyclicgraph.hpp"

#include <utility>
#include <map>
#include <set>
#include <vector>

/************************************************************************/
/*                         FORWARD DECLARATIONS                         */
/************************************************************************/

class OGRGeometry;
class OGRRawPoint;
class OGRPoint;
class OGRMultiPoint;
class OGRLineString;
class OGRMultiLineString;
class OGRLinearRing;
class OGRPolygon;
class OGRMultiPolygon;
class OGRGeometryCollection;
class OGRFeature;
class OGRGeoJSONLayer;
class OGRSpatialReference;

/************************************************************************/
/*                           GeoJSONObject                              */
/************************************************************************/

struct GeoJSONObject
{
    enum Type
    {
        eUnknown = wkbUnknown, // non-GeoJSON properties
        ePoint = wkbPoint,
        eLineString = wkbLineString,
        ePolygon = wkbPolygon,
        eMultiPoint = wkbMultiPoint,
        eMultiLineString = wkbMultiLineString,
        eMultiPolygon = wkbMultiPolygon,
        eGeometryCollection = wkbGeometryCollection,
        eFeature,
        eFeatureCollection
    };

    enum CoordinateDimension
    {
        eMinCoordinateDimension = 2,
        eMaxCoordinateDimension = 3
    };
};

/************************************************************************/
/*                        OGRGeoJSONBaseReader                          */
/************************************************************************/

class OGRGeoJSONBaseReader
{
  public:
      OGRGeoJSONBaseReader();

    void SetPreserveGeometryType( bool bPreserve );
    void SetSkipAttributes( bool bSkip );
    void SetFlattenNestedAttributes( bool bFlatten, char chSeparator );
    void SetStoreNativeData( bool bStoreNativeData );
    void SetArrayAsString( bool bArrayAsString );
    void SetDateAsString( bool bDateAsString );

    bool GenerateFeatureDefn( std::map<std::string, int>& oMapFieldNameToIdx,
                              std::vector<std::unique_ptr<OGRFieldDefn>>& apoFieldDefn,
                              gdal::DirectedAcyclicGraph<int, std::string>& dag,
                              OGRLayer* poLayer, json_object* poObj );
    void FinalizeLayerDefn( OGRLayer* poLayer, CPLString& osFIDColumn );

    OGRGeometry* ReadGeometry( json_object* poObj, OGRSpatialReference* poLayerSRS );
    OGRFeature* ReadFeature( OGRLayer* poLayer, json_object* poObj,
                             const char* pszSerializedObj );
  protected:
    bool bGeometryPreserve_ = true;
    bool bAttributesSkip_ = false;
    bool bFlattenNestedAttributes_ = false;
    char chNestedAttributeSeparator_ = 0;
    bool bStoreNativeData_ = false;
    bool bArrayAsString_ = false;
    bool bDateAsString_ = false;

  private:

    std::set<int> aoSetUndeterminedTypeFields_;

    // bFlatten... is a tri-state boolean with -1 being unset.
    int bFlattenGeocouchSpatiallistFormat = -1;

    bool bFoundGeocouchId = false;
    bool bFoundRev = false;
    bool bFoundTypeFeature = false;
    bool bIsGeocouchSpatiallistFormat = false;
    bool bFeatureLevelIdAsAttribute_ = false;
    bool bFeatureLevelIdAsFID_ = false;
    bool m_bNeedFID64 = false;

    bool m_bDetectLayerGeomType = true;
    bool m_bFirstGeometry = true;
    OGRwkbGeometryType m_eLayerGeomType = wkbUnknown;

    CPL_DISALLOW_COPY_ASSIGN(OGRGeoJSONBaseReader)
};

/************************************************************************/
/*                           OGRGeoJSONReader                           */
/************************************************************************/

class OGRGeoJSONDataSource;
class OGRGeoJSONReaderStreamingParser;

class OGRGeoJSONReader: public OGRGeoJSONBaseReader
{
  public:
    OGRGeoJSONReader();
    ~OGRGeoJSONReader();

    OGRErr Parse( const char* pszText );
    void ReadLayers( OGRGeoJSONDataSource* poDS );
    void ReadLayer( OGRGeoJSONDataSource* poDS,
                    const char* pszName,
                    json_object* poObj );
    bool FirstPassReadLayer( OGRGeoJSONDataSource* poDS, VSILFILE* fp,
                             bool& bTryStandardReading );

    json_object* GetJSonObject() { return poGJObject_; }

    void ResetReading();
    OGRFeature* GetNextFeature(OGRGeoJSONLayer* poLayer);
    OGRFeature* GetFeature(OGRGeoJSONLayer* poLayer, GIntBig nFID);
    bool IngestAll(OGRGeoJSONLayer* poLayer);

    VSILFILE* GetFP() { return fp_; }
    bool CanEasilyAppend() const { return bCanEasilyAppend_; }
    bool FCHasBBOX() const { return bFCHasBBOX_; }

  private:
    friend class OGRGeoJSONReaderStreamingParser;

    json_object* poGJObject_;
    OGRGeoJSONReaderStreamingParser* poStreamingParser_;
    bool bFirstSeg_;
    bool bJSonPLikeWrapper_;
    VSILFILE* fp_;
    bool bCanEasilyAppend_;
    bool bFCHasBBOX_;

    size_t nBufferSize_;
    GByte* pabyBuffer_;

    GIntBig nTotalFeatureCount_;
    GUIntBig nTotalOGRFeatureMemEstimate_;

    std::map<GIntBig, std::pair<vsi_l_offset, vsi_l_offset>> oMapFIDToOffsetSize_;
    //
    // Copy operations not supported.
    //
    CPL_DISALLOW_COPY_ASSIGN(OGRGeoJSONReader)

    //
    // Translation utilities.
    //
    bool GenerateLayerDefn( OGRGeoJSONLayer* poLayer, json_object* poGJObject );

    static bool AddFeature( OGRGeoJSONLayer* poLayer, OGRGeometry* poGeometry );
    static bool AddFeature( OGRGeoJSONLayer* poLayer, OGRFeature* poFeature );

    void ReadFeatureCollection( OGRGeoJSONLayer* poLayer, json_object* poObj );
    size_t SkipPrologEpilogAndUpdateJSonPLikeWrapper( size_t nRead );
};

void OGRGeoJSONReaderSetField( OGRLayer* poLayer,
                               OGRFeature* poFeature,
                               int nField,
                               const char* pszAttrPrefix,
                               json_object* poVal,
                               bool bFlattenNestedAttributes,
                               char chNestedAttributeSeparator );
void OGRGeoJSONReaderAddOrUpdateField(
    std::vector<int>& retIndices,
    std::map<std::string, int>& oMapFieldNameToIdx,
    std::vector<std::unique_ptr<OGRFieldDefn>>& apoFieldDefn,
    const char* pszKey,
    json_object* poVal,
    bool bFlattenNestedAttributes,
    char chNestedAttributeSeparator,
    bool bArrayAsString,
    bool bDateAsString,
    std::set<int>& aoSetUndeterminedTypeFields );

/************************************************************************/
/*                 GeoJSON Parsing Utilities                            */
/************************************************************************/

lh_entry* OGRGeoJSONFindMemberEntryByName( json_object* poObj,
                                         const char* pszName );
json_object* OGRGeoJSONFindMemberByName( json_object* poObj,
                                         const char* pszName );
GeoJSONObject::Type OGRGeoJSONGetType( json_object* poObj );

json_object* json_ex_get_object_by_path( json_object* poObj,
                                         const char* pszPath );

json_object CPL_DLL*  CPL_json_object_object_get( struct json_object* obj,
                                                  const char *key );

bool CPL_DLL OGRJSonParse( const char* pszText, json_object** ppoObj,
                           bool bVerboseError = true );

bool OGRGeoJSONUpdateLayerGeomType( OGRLayer* poLayer,
                                    bool& bFirstGeom,
                                    OGRwkbGeometryType eGeomType,
                                    OGRwkbGeometryType& eLayerGeomType );

/************************************************************************/
/*                 GeoJSON Geometry Translators                         */
/************************************************************************/

bool OGRGeoJSONReadRawPoint( json_object* poObj, OGRPoint& point );
OGRGeometry* OGRGeoJSONReadGeometry( json_object* poObj );
OGRPoint* OGRGeoJSONReadPoint( json_object* poObj );
OGRMultiPoint* OGRGeoJSONReadMultiPoint( json_object* poObj );
OGRLineString* OGRGeoJSONReadLineString( json_object* poObj, bool bRaw=false );
OGRMultiLineString* OGRGeoJSONReadMultiLineString( json_object* poObj );
OGRLinearRing* OGRGeoJSONReadLinearRing( json_object* poObj );
OGRPolygon* OGRGeoJSONReadPolygon( json_object* poObj , bool bRaw=false);
OGRMultiPolygon* OGRGeoJSONReadMultiPolygon( json_object* poObj );
OGRGeometryCollection* OGRGeoJSONReadGeometryCollection( json_object* poObj,
                                                         OGRSpatialReference* poSRS = nullptr );
OGRSpatialReference* OGRGeoJSONReadSpatialReference( json_object* poObj );

/************************************************************************/
/*                          OGRESRIJSONReader                           */
/************************************************************************/

class OGRESRIJSONReader
{
  public:
    OGRESRIJSONReader();
    ~OGRESRIJSONReader();

    OGRErr Parse( const char* pszText );
    void ReadLayers( OGRGeoJSONDataSource* poDS, GeoJSONSourceType eSourceType );

    json_object* GetJSonObject() { return poGJObject_; }

private:
    json_object* poGJObject_;
    OGRGeoJSONLayer* poLayer_;

    //
    // Copy operations not supported.
    //
    OGRESRIJSONReader( OGRESRIJSONReader const& );
    OGRESRIJSONReader& operator=( OGRESRIJSONReader const& );

    //
    // Translation utilities.
    //
    bool GenerateLayerDefn();
    bool ParseField( json_object* poObj );
    bool AddFeature( OGRFeature* poFeature );

    OGRFeature* ReadFeature( json_object* poObj );
    OGRGeoJSONLayer* ReadFeatureCollection( json_object* poObj );
};

OGRGeometry* OGRESRIJSONReadGeometry( json_object* poObj );
OGRSpatialReference* OGRESRIJSONReadSpatialReference( json_object* poObj );
OGRwkbGeometryType OGRESRIJSONGetGeometryType( json_object* poObj );
OGRPoint* OGRESRIJSONReadPoint( json_object* poObj);
OGRGeometry* OGRESRIJSONReadLineString( json_object* poObj);
OGRGeometry* OGRESRIJSONReadPolygon( json_object* poObj);
OGRMultiPoint* OGRESRIJSONReadMultiPoint( json_object* poObj);

/************************************************************************/
/*                          OGRTopoJSONReader                           */
/************************************************************************/

class OGRTopoJSONReader
{
  public:
    OGRTopoJSONReader();
    ~OGRTopoJSONReader();

    OGRErr Parse( const char* pszText, bool bLooseIdentification );
    void ReadLayers( OGRGeoJSONDataSource* poDS );

  private:
    json_object* poGJObject_;

    //
    // Copy operations not supported.
    //
    OGRTopoJSONReader( OGRTopoJSONReader const& );
    OGRTopoJSONReader& operator=( OGRTopoJSONReader const& );
};

#endif /* OGR_GEOJSONUTILS_H_INCLUDED */
