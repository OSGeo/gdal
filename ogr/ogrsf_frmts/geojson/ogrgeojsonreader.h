/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines GeoJSON reader within OGR OGRGeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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
/*                        OGRGeoJSONBaseReader                          */
/************************************************************************/

class OGRGeoJSONBaseReader
{
  public:
    OGRGeoJSONBaseReader();

    void SetPreserveGeometryType(bool bPreserve);
    void SetSkipAttributes(bool bSkip);
    void SetFlattenNestedAttributes(bool bFlatten, char chSeparator);
    void SetStoreNativeData(bool bStoreNativeData);
    void SetArrayAsString(bool bArrayAsString);
    void SetDateAsString(bool bDateAsString);

    enum class ForeignMemberProcessing
    {
        AUTO,
        ALL,
        NONE,
        STAC,
    };

    void
    SetForeignMemberProcessing(ForeignMemberProcessing eForeignMemberProcessing)
    {
        eForeignMemberProcessing_ = eForeignMemberProcessing;
    }

    bool GenerateFeatureDefn(
        std::map<std::string, int> &oMapFieldNameToIdx,
        std::vector<std::unique_ptr<OGRFieldDefn>> &apoFieldDefn,
        gdal::DirectedAcyclicGraph<int, std::string> &dag, OGRLayer *poLayer,
        json_object *poObj);
    void FinalizeLayerDefn(OGRLayer *poLayer, CPLString &osFIDColumn);

    OGRGeometry *ReadGeometry(json_object *poObj,
                              OGRSpatialReference *poLayerSRS);
    OGRFeature *ReadFeature(OGRLayer *poLayer, json_object *poObj,
                            const char *pszSerializedObj);

    bool ExtentRead() const;

    OGREnvelope3D GetExtent3D() const;

  protected:
    bool bGeometryPreserve_ = true;
    bool bAttributesSkip_ = false;
    bool bFlattenNestedAttributes_ = false;
    char chNestedAttributeSeparator_ = 0;
    bool bStoreNativeData_ = false;
    bool bArrayAsString_ = false;
    bool bDateAsString_ = false;
    ForeignMemberProcessing eForeignMemberProcessing_ =
        ForeignMemberProcessing::AUTO;

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

    bool m_bFirstGeometry = true;
    OGREnvelope3D m_oEnvelope3D;
    // Becomes true when extent has been read from data
    bool m_bExtentRead = false;
    OGRwkbGeometryType m_eLayerGeomType = wkbUnknown;

    CPL_DISALLOW_COPY_ASSIGN(OGRGeoJSONBaseReader)
};

/************************************************************************/
/*                           OGRGeoJSONReader                           */
/************************************************************************/

class OGRGeoJSONDataSource;
class OGRGeoJSONReaderStreamingParser;

class OGRGeoJSONReader : public OGRGeoJSONBaseReader
{
  public:
    OGRGeoJSONReader();
    ~OGRGeoJSONReader();

    OGRErr Parse(const char *pszText);
    void ReadLayers(OGRGeoJSONDataSource *poDS);
    void ReadLayer(OGRGeoJSONDataSource *poDS, const char *pszName,
                   json_object *poObj);
    bool FirstPassReadLayer(OGRGeoJSONDataSource *poDS, VSILFILE *fp,
                            bool &bTryStandardReading);

    json_object *GetJSonObject()
    {
        return poGJObject_;
    }

    void ResetReading();
    OGRFeature *GetNextFeature(OGRGeoJSONLayer *poLayer);
    OGRFeature *GetFeature(OGRGeoJSONLayer *poLayer, GIntBig nFID);
    bool IngestAll(OGRGeoJSONLayer *poLayer);

    VSILFILE *GetFP()
    {
        return fp_;
    }

    bool CanEasilyAppend() const
    {
        return bCanEasilyAppend_;
    }

    bool FCHasBBOX() const
    {
        return bFCHasBBOX_;
    }

  private:
    friend class OGRGeoJSONReaderStreamingParser;

    json_object *poGJObject_;
    OGRGeoJSONReaderStreamingParser *poStreamingParser_;
    bool bFirstSeg_;
    bool bJSonPLikeWrapper_;
    VSILFILE *fp_;
    bool bCanEasilyAppend_;
    bool bFCHasBBOX_;
    bool bOriginalIdModifiedEmitted_ = false;

    size_t nBufferSize_;
    GByte *pabyBuffer_;

    GIntBig nTotalFeatureCount_;
    GUIntBig nTotalOGRFeatureMemEstimate_;

    std::map<GIntBig, std::pair<vsi_l_offset, vsi_l_offset>>
        oMapFIDToOffsetSize_;
    //
    // Copy operations not supported.
    //
    CPL_DISALLOW_COPY_ASSIGN(OGRGeoJSONReader)

    //
    // Translation utilities.
    //
    bool GenerateLayerDefn(OGRGeoJSONLayer *poLayer, json_object *poGJObject);

    static bool AddFeature(OGRGeoJSONLayer *poLayer, OGRGeometry *poGeometry);
    static bool AddFeature(OGRGeoJSONLayer *poLayer, OGRFeature *poFeature);

    void ReadFeatureCollection(OGRGeoJSONLayer *poLayer, json_object *poObj);
    size_t SkipPrologEpilogAndUpdateJSonPLikeWrapper(size_t nRead);
};

void OGRGeoJSONGenerateFeatureDefnDealWithID(
    json_object *poObj, json_object *poObjProps, int &nPrevFieldIdx,
    std::map<std::string, int> &oMapFieldNameToIdx,
    std::vector<std::unique_ptr<OGRFieldDefn>> &apoFieldDefn,
    gdal::DirectedAcyclicGraph<int, std::string> &dag,
    bool &bFeatureLevelIdAsFID, bool &bFeatureLevelIdAsAttribute,
    bool &bNeedFID64);

void OGRGeoJSONReaderSetField(OGRLayer *poLayer, OGRFeature *poFeature,
                              int nField, const char *pszAttrPrefix,
                              json_object *poVal, bool bFlattenNestedAttributes,
                              char chNestedAttributeSeparator);
void OGRGeoJSONReaderAddOrUpdateField(
    std::vector<int> &retIndices,
    std::map<std::string, int> &oMapFieldNameToIdx,
    std::vector<std::unique_ptr<OGRFieldDefn>> &apoFieldDefn,
    const char *pszKey, json_object *poVal, bool bFlattenNestedAttributes,
    char chNestedAttributeSeparator, bool bArrayAsString, bool bDateAsString,
    std::set<int> &aoSetUndeterminedTypeFields);

/************************************************************************/
/*                 GeoJSON Parsing Utilities                            */
/************************************************************************/

bool OGRGeoJSONUpdateLayerGeomType(bool &bFirstGeom,
                                   OGRwkbGeometryType eGeomType,
                                   OGRwkbGeometryType &eLayerGeomType);

// Get the 3D extent from the geometry coordinates of a feature
bool OGRGeoJSONGetExtent3D(json_object *poObj, OGREnvelope3D *poEnvelope);

/************************************************************************/
/*                          OGRESRIJSONReader                           */
/************************************************************************/

class OGRESRIJSONReader
{
  public:
    OGRESRIJSONReader();
    ~OGRESRIJSONReader();

    OGRErr Parse(const char *pszText);
    void ReadLayers(OGRGeoJSONDataSource *poDS, GeoJSONSourceType eSourceType);

    json_object *GetJSonObject()
    {
        return poGJObject_;
    }

  private:
    json_object *poGJObject_;
    OGRGeoJSONLayer *poLayer_;

    //
    // Copy operations not supported.
    //
    OGRESRIJSONReader(OGRESRIJSONReader const &);
    OGRESRIJSONReader &operator=(OGRESRIJSONReader const &);

    //
    // Translation utilities.
    //
    bool GenerateLayerDefn();
    bool ParseField(json_object *poObj);
    bool AddFeature(OGRFeature *poFeature);

    OGRFeature *ReadFeature(json_object *poObj);
    OGRGeoJSONLayer *ReadFeatureCollection(json_object *poObj);
};

/************************************************************************/
/*                          OGRTopoJSONReader                           */
/************************************************************************/

class OGRTopoJSONReader
{
  public:
    OGRTopoJSONReader();
    ~OGRTopoJSONReader();

    OGRErr Parse(const char *pszText, bool bLooseIdentification);
    void ReadLayers(OGRGeoJSONDataSource *poDS);

  private:
    json_object *poGJObject_;

    //
    // Copy operations not supported.
    //
    OGRTopoJSONReader(OGRTopoJSONReader const &);
    OGRTopoJSONReader &operator=(OGRTopoJSONReader const &);
};

#endif /* OGR_GEOJSONUTILS_H_INCLUDED */
