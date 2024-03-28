/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGC Features and Geometries JSON (JSON-FG)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
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

#include "ogr_jsonfg.h"

#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogr_geojson.h"

#include "cpl_vsi_virtual.h"

#include <json.h>  // JSON-C

/************************************************************************/
/*                  OGRJSONFGReader::~OGRJSONFGReader()                 */
/************************************************************************/

OGRJSONFGReader::~OGRJSONFGReader()
{
    if (poObject_)
        json_object_put(poObject_);
}

/************************************************************************/
/*                  OGRJSONFGReader::Load()                             */
/************************************************************************/

bool OGRJSONFGReader::Load(OGRJSONFGDataset *poDS, const char *pszText,
                           const std::string &osDefaultLayerName)
{
    if (!OGRJSonParse(pszText, &poObject_))
        return false;

    poDS_ = poDS;
    osDefaultLayerName_ = osDefaultLayerName;

    if (!GenerateLayerDefns())
        return false;

    const GeoJSONObject::Type objType = OGRGeoJSONGetType(poObject_);
    if (objType == GeoJSONObject::eFeature)
    {
        OGRJSONFGMemLayer *poLayer = nullptr;
        auto poFeat = ReadFeature(poObject_, nullptr, &poLayer, nullptr);
        if (poFeat)
        {
            poLayer->AddFeature(std::move(poFeat));
            return true;
        }
        return false;
    }
    else if (objType == GeoJSONObject::eFeatureCollection)
    {
        json_object *poObjFeatures =
            OGRGeoJSONFindMemberByName(poObject_, "features");
        if (nullptr != poObjFeatures &&
            json_type_array == json_object_get_type(poObjFeatures))
        {
            const auto nFeatures = json_object_array_length(poObjFeatures);
            for (auto i = decltype(nFeatures){0}; i < nFeatures; ++i)
            {
                json_object *poObjFeature =
                    json_object_array_get_idx(poObjFeatures, i);
                OGRJSONFGMemLayer *poLayer = nullptr;
                auto poFeat =
                    ReadFeature(poObjFeature, nullptr, &poLayer, nullptr);
                if (!poFeat)
                    return false;
                poLayer->AddFeature(std::move(poFeat));
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                    OGRJSONFGReadCoordRefSys()                        */
/************************************************************************/

static std::unique_ptr<OGRSpatialReference>
OGRJSONFGReadCoordRefSys(json_object *poCoordRefSys, bool bCanRecurse = true)
{
    const auto eType = json_object_get_type(poCoordRefSys);
    if (eType == json_type_string)
    {
        const char *pszStr = json_object_get_string(poCoordRefSys);
        if (pszStr[0] == '[' && pszStr[strlen(pszStr) - 1] == ']')
        {
            // Safe CURIE, e.g. "[EPSG:4326]"
            const char *pszColon = strchr(pszStr + 1, ':');
            if (!pszColon)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid coordRefSys string: %s", pszStr);
                return nullptr;
            }
            std::string osURL("http://www.opengis.net/def/crs/");
            osURL.append(pszStr + 1, pszColon - (pszStr + 1));
            osURL += "/0/";
            osURL.append(pszColon + 1,
                         (pszStr + strlen(pszStr) - 1) - (pszColon + 1));
            auto poSRS = std::make_unique<OGRSpatialReference>();
            if (poSRS->importFromCRSURL(osURL.c_str()) != OGRERR_NONE)
            {
                return nullptr;
            }
            return poSRS;
        }
        else if (STARTS_WITH(pszStr, "http://www.opengis.net/def/crs/"))
        {
            // OGC URI, e.g. "http://www.opengis.net/def/crs/EPSG/0/4326"
            auto poSRS = std::make_unique<OGRSpatialReference>();
            if (poSRS->importFromCRSURL(pszStr) != OGRERR_NONE)
            {
                return nullptr;
            }
            return poSRS;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid coordRefSys string: %s", pszStr);
            return nullptr;
        }
    }
    else if (eType == json_type_object)
    {
        /* Things like
              {
                "type": "Reference",
                "href": "http://www.opengis.net/def/crs/EPSG/0/4258",
                "epoch": 2016.47
              }
        */

        json_object *poType = CPL_json_object_object_get(poCoordRefSys, "type");
        if (!poType)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing type member in coordRefSys object");
            return nullptr;
        }
        if (json_object_get_type(poType) != json_type_string)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Type member of coordRefSys object is not a string");
            return nullptr;
        }
        const char *pszType = json_object_get_string(poType);
        if (strcmp(pszType, "Reference") != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Only type=\"Reference\" handled in coordRefSys object");
            return nullptr;
        }

        json_object *poHRef = CPL_json_object_object_get(poCoordRefSys, "href");
        if (!poHRef)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing href member in coordRefSys object");
            return nullptr;
        }

        auto poSRS = OGRJSONFGReadCoordRefSys(poHRef);
        if (!poSRS)
            return nullptr;

        json_object *poEpoch =
            CPL_json_object_object_get(poCoordRefSys, "epoch");
        if (poEpoch)
        {
            const auto epochType = json_object_get_type(poEpoch);
            if (epochType != json_type_int && epochType != json_type_double)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Wrong value type for epoch member in coordRefSys object");
                return nullptr;
            }

            poSRS->SetCoordinateEpoch(json_object_get_double(poEpoch));
        }

        return poSRS;
    }
    else if (eType == json_type_array && bCanRecurse)
    {
        if (json_object_array_length(poCoordRefSys) != 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Expected 2 items in coordRefSys array");
            return nullptr;
        }
        auto poSRS1 = OGRJSONFGReadCoordRefSys(
            json_object_array_get_idx(poCoordRefSys, 0),
            /* bCanRecurse = */ false);
        if (!poSRS1)
            return nullptr;
        auto poSRS2 = OGRJSONFGReadCoordRefSys(
            json_object_array_get_idx(poCoordRefSys, 1),
            /* bCanRecurse = */ false);
        if (!poSRS2)
            return nullptr;
        auto poSRS = std::make_unique<OGRSpatialReference>();

        std::string osName;
        const char *pszName1 = poSRS1->GetName();
        osName = pszName1 ? pszName1 : "unnamed";
        osName += " + ";
        const char *pszName2 = poSRS2->GetName();
        osName += pszName2 ? pszName2 : "unnamed";

        if (poSRS->SetCompoundCS(osName.c_str(), poSRS1.get(), poSRS2.get()) !=
            OGRERR_NONE)
            return nullptr;
        const double dfEpoch = poSRS1->GetCoordinateEpoch();
        if (dfEpoch > 0)
            poSRS->SetCoordinateEpoch(dfEpoch);
        return poSRS;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid coordRefSys object");
    }
    return nullptr;
}

/************************************************************************/
/*              OGRJSONFGReader::AnalyzeWithStreamingParser()           */
/************************************************************************/

bool OGRJSONFGReader::AnalyzeWithStreamingParser(
    OGRJSONFGDataset *poDS, VSILFILE *fp, const std::string &osDefaultLayerName,
    bool &bCanTryWithNonStreamingParserOut)
{
    poDS_ = poDS;
    osDefaultLayerName_ = osDefaultLayerName;

    bCanTryWithNonStreamingParserOut = false;
    OGRJSONFGStreamingParser oParser(*this, /*bFirstPass = */ true);

    std::vector<GByte> abyBuffer;
    abyBuffer.resize(4096 * 10);
    while (true)
    {
        size_t nRead = VSIFReadL(abyBuffer.data(), 1, abyBuffer.size(), fp);
        const bool bFinished = nRead < abyBuffer.size();
        if (!oParser.Parse(reinterpret_cast<const char *>(abyBuffer.data()),
                           nRead, bFinished) ||
            oParser.ExceptionOccurred())
        {
            return false;
        }
        if (oParser.IsTypeKnown() && !oParser.IsFeatureCollection())
        {
            break;
        }
        if (bFinished)
            break;
    }

    if (!oParser.IsTypeKnown() || !oParser.IsFeatureCollection())
    {
        fp->Seek(0, SEEK_END);
        const vsi_l_offset nFileSize = fp->Tell();
        const vsi_l_offset nRAM =
            static_cast<vsi_l_offset>(CPLGetUsablePhysicalRAM());
        if (nRAM == 0 || nRAM > nFileSize * 20)
        {
            // Only try full ingestion if we have 20x more RAM than the file
            // size
            bCanTryWithNonStreamingParserOut = true;
        }
        return false;
    }

    poObject_ = oParser.StealRootObject();

    return FinalizeGenerateLayerDefns(true);
}

/************************************************************************/
/*                OGRJSONFGReader::GenerateLayerDefns()                 */
/************************************************************************/

bool OGRJSONFGReader::GenerateLayerDefns()
{
    const GeoJSONObject::Type objType = OGRGeoJSONGetType(poObject_);
    if (objType == GeoJSONObject::eFeature)
    {
        if (!GenerateLayerDefnFromFeature(poObject_))
            return false;
    }
    else if (objType == GeoJSONObject::eFeatureCollection)
    {
        json_object *poObjFeatures =
            OGRGeoJSONFindMemberByName(poObject_, "features");
        if (nullptr != poObjFeatures &&
            json_type_array == json_object_get_type(poObjFeatures))
        {
            const auto nFeatures = json_object_array_length(poObjFeatures);
            for (auto i = decltype(nFeatures){0}; i < nFeatures; ++i)
            {
                json_object *poObjFeature =
                    json_object_array_get_idx(poObjFeatures, i);
                if (!GenerateLayerDefnFromFeature(poObjFeature))
                {
                    return false;
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid FeatureCollection object. "
                     "Missing \'features\' member.");
            return false;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing or unhandled root type object");
        return false;
    }

    return FinalizeGenerateLayerDefns(false);
}

/************************************************************************/
/*             OGRJSONFGReader::FinalizeGenerateLayerDefns()            */
/************************************************************************/

bool OGRJSONFGReader::FinalizeGenerateLayerDefns(bool bStreamedLayer)
{
    json_object *poName = CPL_json_object_object_get(poObject_, "featureType");
    if (poName && json_object_get_type(poName) == json_type_string)
    {
        // Remap from hard-coded default layer name to the one of featureType
        auto oIter = oMapBuildContext_.find(osDefaultLayerName_);
        osDefaultLayerName_ = json_object_get_string(poName);
        if (oIter != oMapBuildContext_.end())
        {
            auto oBuildContext = std::move(oIter->second);
            oMapBuildContext_.erase(oIter);
            oMapBuildContext_[osDefaultLayerName_] = std::move(oBuildContext);
        }
    }
    else if (poName && json_object_get_type(poName) == json_type_array)
    {
        static bool bWarningMsgEmitted = false;
        if (!bWarningMsgEmitted)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "featureType value as an array is not supported.");
            bWarningMsgEmitted = true;
        }
    }

    json_object *poCoordRefSys = nullptr;
    std::unique_ptr<OGRSpatialReference> poSRSTopLevel;
    bool bInvalidCRS = false;
    bool bSwapPlacesXYTopLevel = false;
    if (json_object_object_get_ex(poObject_, "coordRefSys", &poCoordRefSys) &&
        eGeometryElement_ != GeometryElement::GEOMETRY)
    {
        poSRSTopLevel = OGRJSONFGReadCoordRefSys(poCoordRefSys);
        if (poSRSTopLevel)
        {
            poSRSTopLevel->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            bSwapPlacesXYTopLevel = OGRJSONFGMustSwapXY(poSRSTopLevel.get());
        }
        else
        {
            bInvalidCRS = true;
        }
    }

    // Finalize layer definition building and create OGRLayer objects
    for (auto &oBuildContextIter : oMapBuildContext_)
    {
        const char *pszLayerName = oBuildContextIter.first.c_str();
        auto &oBuildContext = oBuildContextIter.second;

        FinalizeBuildContext(oBuildContext, pszLayerName, bStreamedLayer,
                             bInvalidCRS, bSwapPlacesXYTopLevel,
                             poSRSTopLevel.get());
    }

    return true;
}

/************************************************************************/
/*                OGRJSONFGReader::FinalizeBuildContext()               */
/************************************************************************/

void OGRJSONFGReader::FinalizeBuildContext(LayerDefnBuildContext &oBuildContext,
                                           const char *pszLayerName,
                                           bool bStreamedLayer,
                                           bool bInvalidCRS,
                                           bool bSwapPlacesXYTopLevel,
                                           OGRSpatialReference *poSRSTopLevel)
{
    std::unique_ptr<OGRSpatialReference> poSRSWGS84(
        OGRSpatialReference::GetWGS84SRS()->Clone());
    poSRSWGS84->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRSpatialReference *poSRSLayer = nullptr;
    if (oBuildContext.poCRSAtFeatureLevel)
    {
        poSRSLayer = oBuildContext.poCRSAtFeatureLevel.get();
        oBuildContext.bSwapPlacesXY = OGRJSONFGMustSwapXY(poSRSLayer);
    }
    else if (poSRSTopLevel)
    {
        poSRSLayer = poSRSTopLevel;
        oBuildContext.bSwapPlacesXY = bSwapPlacesXYTopLevel;
    }
    if (!bInvalidCRS)
    {
        if (!poSRSLayer && !oBuildContext.bHasCoordRefSysAtFeatureLevel)
        {
            // No coordRefSys member found anywhere ? Fallback to WGS 84
            poSRSLayer = poSRSWGS84.get();
        }

        if (poSRSLayer && poSRSLayer->IsSame(poSRSWGS84.get()))
        {
            oBuildContext.bLayerCRSIsWGS84 = true;
        }
        else if (poSRSLayer)
        {
            const char *pszAuthName = poSRSLayer->GetAuthorityName(nullptr);
            if (!(pszAuthName && STARTS_WITH(pszAuthName, "IAU")))
            {
                oBuildContext.poCTWGS84ToLayerCRS.reset(
                    OGRCreateCoordinateTransformation(poSRSWGS84.get(),
                                                      poSRSLayer));
            }
        }
    }

    std::unique_ptr<OGRJSONFGMemLayer> poMemLayer;
    std::unique_ptr<OGRJSONFGStreamedLayer> poStreamedLayer;
    OGRLayer *poLayer;
    if (bStreamedLayer)
    {
        poStreamedLayer = std::make_unique<OGRJSONFGStreamedLayer>(
            poDS_, pszLayerName, poSRSLayer, oBuildContext.eLayerGeomType);
        poLayer = poStreamedLayer.get();
    }
    else
    {
        poMemLayer = std::make_unique<OGRJSONFGMemLayer>(
            poDS_, pszLayerName, poSRSLayer, oBuildContext.eLayerGeomType);
        poLayer = poMemLayer.get();
    }

    // Note: the current strategy will not produce stable output, depending
    // on the order of features, if there are conflicting order / cycles.
    // See https://github.com/OSGeo/gdal/pull/4552 for a number of potential
    // resolutions if that has to be solved in the future.
    OGRFeatureDefn *poLayerDefn = poLayer->GetLayerDefn();
    auto oTemporaryUnsealer(poLayerDefn->GetTemporaryUnsealer());

    if (poLayer->GetLayerDefn()->GetGeomType() != wkbNone)
    {
        OGRGeoJSONWriteOptions options;

        json_object *poXYRes = CPL_json_object_object_get(
            poObject_, "xy_coordinate_resolution_place");
        if (poXYRes && (json_object_get_type(poXYRes) == json_type_double ||
                        json_object_get_type(poXYRes) == json_type_int))
        {
            auto poGeomFieldDefn = poLayerDefn->GetGeomFieldDefn(0);
            OGRGeomCoordinatePrecision oCoordPrec(
                poGeomFieldDefn->GetCoordinatePrecision());
            oCoordPrec.dfXYResolution = json_object_get_double(poXYRes);
            poGeomFieldDefn->SetCoordinatePrecision(oCoordPrec);
        }

        json_object *poZRes = CPL_json_object_object_get(
            poObject_, "z_coordinate_resolution_place");
        if (poZRes && (json_object_get_type(poZRes) == json_type_double ||
                       json_object_get_type(poZRes) == json_type_int))
        {
            auto poGeomFieldDefn = poLayerDefn->GetGeomFieldDefn(0);
            OGRGeomCoordinatePrecision oCoordPrec(
                poGeomFieldDefn->GetCoordinatePrecision());
            oCoordPrec.dfZResolution = json_object_get_double(poZRes);
            poGeomFieldDefn->SetCoordinatePrecision(oCoordPrec);
        }
    }

    std::set<std::string> oSetFieldNames;
    for (const auto &poFieldDefn : oBuildContext.apoFieldDefn)
        oSetFieldNames.insert(poFieldDefn->GetNameRef());

    auto AddTimeField =
        [poLayerDefn, &oSetFieldNames](const char *pszName, OGRFieldType eType)
    {
        if (oSetFieldNames.find(pszName) == oSetFieldNames.end())
        {
            OGRFieldDefn oFieldDefn(pszName, eType);
            poLayerDefn->AddFieldDefn(&oFieldDefn);
        }
        else
        {
            OGRFieldDefn oFieldDefn((std::string("jsonfg_") + pszName).c_str(),
                                    eType);
            poLayerDefn->AddFieldDefn(&oFieldDefn);
        }
        return poLayerDefn->GetFieldCount() - 1;
    };

    if (oBuildContext.bHasTimeTimestamp)
    {
        oBuildContext.nIdxFieldTime = AddTimeField("time", OFTDateTime);
    }
    else if (oBuildContext.bHasTimeDate)
    {
        oBuildContext.nIdxFieldTime = AddTimeField("time", OFTDate);
    }

    if (oBuildContext.bHasTimeIntervalStartDate ||
        oBuildContext.bHasTimeIntervalStartTimestamp ||
        oBuildContext.bHasTimeIntervalEndDate ||
        oBuildContext.bHasTimeIntervalEndTimestamp)
    {
        // Mix of Date/DateTime for start/end is not supposed to happen,
        // but be tolerant to that
        if (oBuildContext.bHasTimeIntervalStartTimestamp)
        {
            oBuildContext.nIdxFieldTimeStart =
                AddTimeField("time_start", OFTDateTime);
        }
        else if (oBuildContext.bHasTimeIntervalStartDate)
        {
            oBuildContext.nIdxFieldTimeStart =
                AddTimeField("time_start", OFTDate);
        }
        else if (oBuildContext.bHasTimeIntervalEndTimestamp)
        {
            oBuildContext.nIdxFieldTimeStart =
                AddTimeField("time_start", OFTDateTime);
        }
        else /* if( oBuildContext.bHasTimeIntervalEndDate ) */
        {
            oBuildContext.nIdxFieldTimeStart =
                AddTimeField("time_start", OFTDate);
        }

        if (oBuildContext.bHasTimeIntervalEndTimestamp)
        {
            oBuildContext.nIdxFieldTimeEnd =
                AddTimeField("time_end", OFTDateTime);
        }
        else if (oBuildContext.bHasTimeIntervalEndDate)
        {
            oBuildContext.nIdxFieldTimeEnd = AddTimeField("time_end", OFTDate);
        }
        else if (oBuildContext.bHasTimeIntervalStartTimestamp)
        {
            oBuildContext.nIdxFieldTimeEnd =
                AddTimeField("time_end", OFTDateTime);
        }
        else /* if( oBuildContext.bHasTimeIntervalStartDate ) */
        {
            oBuildContext.nIdxFieldTimeEnd = AddTimeField("time_end", OFTDate);
        }
    }

    const auto sortedFields = oBuildContext.dag.getTopologicalOrdering();
    CPLAssert(sortedFields.size() == oBuildContext.apoFieldDefn.size());
    for (int idx : sortedFields)
    {
        poLayerDefn->AddFieldDefn(oBuildContext.apoFieldDefn[idx].get());
    }

    if (!oBuildContext.bFeatureLevelIdAsFID)
    {
        const int idx = poLayerDefn->GetFieldIndexCaseSensitive("id");
        if (idx >= 0)
        {
            OGRFieldDefn *poFDefn = poLayerDefn->GetFieldDefn(idx);
            if (poFDefn->GetType() == OFTInteger ||
                poFDefn->GetType() == OFTInteger64)
            {
                if (poStreamedLayer)
                {
                    poStreamedLayer->SetFIDColumn(
                        poLayerDefn->GetFieldDefn(idx)->GetNameRef());
                }
                else
                {
                    poMemLayer->SetFIDColumn(
                        poLayerDefn->GetFieldDefn(idx)->GetNameRef());
                }
            }
        }
    }

    if (oBuildContext.bNeedFID64)
        poLayer->SetMetadataItem(OLMD_FID64, "YES");

    if (poStreamedLayer)
    {
        poStreamedLayer->SetFeatureCount(oBuildContext.nFeatureCount);
        oBuildContext.poStreamedLayer =
            poDS_->AddLayer(std::move(poStreamedLayer));
    }
    else
    {
        oBuildContext.poMemLayer = poDS_->AddLayer(std::move(poMemLayer));
    }
}

/************************************************************************/
/*            OGRJSONFGReader::GetLayerNameForFeature()                 */
/************************************************************************/

const char *OGRJSONFGReader::GetLayerNameForFeature(json_object *poObj) const
{
    const char *pszName = osDefaultLayerName_.c_str();
    json_object *poName = CPL_json_object_object_get(poObj, "featureType");
    // The spec allows an array of strings, but we don't support that
    if (poName != nullptr && json_object_get_type(poName) == json_type_string)
    {
        pszName = json_object_get_string(poName);
    }
    return pszName;
}

/************************************************************************/
/*                   OGRJSONFGCreateNonGeoJSONGeometry()                */
/************************************************************************/

static std::unique_ptr<OGRGeometry>
OGRJSONFGCreateNonGeoJSONGeometry(json_object *poObj, bool bWarn)
{
    json_object *poObjType = CPL_json_object_object_get(poObj, "type");
    const char *pszType = json_object_get_string(poObjType);
    if (!pszType)
        return nullptr;

    if (strcmp(pszType, "Polyhedron") == 0)
    {
        auto poCoordinates = CPL_json_object_object_get(poObj, "coordinates");
        if (!poCoordinates ||
            json_object_get_type(poCoordinates) != json_type_array)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing or invalid coordinates in Polyhedron");
            return nullptr;
        }
        if (json_object_array_length(poCoordinates) != 1)
        {
            if (bWarn)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Polyhedron with inner shells not supported");
            }
            return nullptr;
        }
        auto poJOuterShell = json_object_array_get_idx(poCoordinates, 0);
        auto poGeom = std::make_unique<OGRPolyhedralSurface>();
        const auto nPolys = json_object_array_length(poJOuterShell);
        for (auto i = decltype(nPolys){0}; i < nPolys; ++i)
        {
            auto poJPoly = json_object_array_get_idx(poJOuterShell, i);
            if (!poJPoly)
                return nullptr;
            auto poPoly = OGRGeoJSONReadPolygon(poJPoly, /*bRaw = */ true);
            if (!poPoly)
                return nullptr;
            if (poGeom->addGeometryDirectly(poPoly) != OGRERR_NONE)
                return nullptr;
        }

        return poGeom;
    }
    else if (strcmp(pszType, "Prism") == 0)
    {
        auto poBase = CPL_json_object_object_get(poObj, "base");
        if (!poBase || json_object_get_type(poBase) != json_type_object)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing or invalid base in Prism");
            return nullptr;
        }

        json_object *poLower = CPL_json_object_object_get(poObj, "lower");
        const double dfLower = poLower ? json_object_get_double(poLower) : 0.0;
        json_object *poUpper = CPL_json_object_object_get(poObj, "upper");
        const double dfUpper = poUpper ? json_object_get_double(poUpper) : 0.0;

        auto poBaseGeom =
            std::unique_ptr<OGRGeometry>(OGRGeoJSONReadGeometry(poBase));
        if (!poBaseGeom)
            return nullptr;
        if (poBaseGeom->getGeometryType() == wkbPoint)
        {
            const auto poPoint = poBaseGeom.get()->toPoint();
            auto poGeom = std::make_unique<OGRLineString>();
            poGeom->addPoint(poPoint->getX(), poPoint->getY(), dfLower);
            poGeom->addPoint(poPoint->getX(), poPoint->getY(), dfUpper);
            return poGeom;
        }
        else if (poBaseGeom->getGeometryType() == wkbLineString)
        {
            const auto poLS = poBaseGeom.get()->toLineString();
            auto poGeom = std::make_unique<OGRMultiPolygon>();
            for (int i = 0; i < poLS->getNumPoints() - 1; ++i)
            {
                auto poPoly = new OGRPolygon();
                auto poRing = new OGRLinearRing();
                poRing->addPoint(poLS->getX(i), poLS->getY(i), dfLower);
                poRing->addPoint(poLS->getX(i + 1), poLS->getY(i + 1), dfLower);
                poRing->addPoint(poLS->getX(i + 1), poLS->getY(i + 1), dfUpper);
                poRing->addPoint(poLS->getX(i), poLS->getY(i), dfUpper);
                poRing->addPoint(poLS->getX(i), poLS->getY(i), dfLower);
                poPoly->addRingDirectly(poRing);
                poGeom->addGeometryDirectly(poPoly);
            }
            return poGeom;
        }
        else if (poBaseGeom->getGeometryType() == wkbPolygon)
        {
            const auto poBasePoly = poBaseGeom.get()->toPolygon();
            if (poBasePoly->getNumInteriorRings() > 0)
            {
                if (bWarn)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Polygon with holes is not supported as the base "
                             "for Prism");
                }
                return nullptr;
            }
            const auto poLS = poBasePoly->getExteriorRing();
            if (poLS == nullptr)
            {
                return nullptr;
            }
            auto poGeom = std::make_unique<OGRPolyhedralSurface>();
            // Build lower face
            {
                auto poPoly = new OGRPolygon();
                auto poRing = new OGRLinearRing();
                for (int i = 0; i < poLS->getNumPoints(); ++i)
                {
                    poRing->addPoint(poLS->getX(i), poLS->getY(i), dfLower);
                }
                poPoly->addRingDirectly(poRing);
                poGeom->addGeometryDirectly(poPoly);
            }
            // Build side faces
            for (int i = 0; i < poLS->getNumPoints() - 1; ++i)
            {
                auto poPoly = new OGRPolygon();
                auto poRing = new OGRLinearRing();
                poRing->addPoint(poLS->getX(i), poLS->getY(i), dfLower);
                poRing->addPoint(poLS->getX(i + 1), poLS->getY(i + 1), dfLower);
                poRing->addPoint(poLS->getX(i + 1), poLS->getY(i + 1), dfUpper);
                poRing->addPoint(poLS->getX(i), poLS->getY(i), dfUpper);
                poRing->addPoint(poLS->getX(i), poLS->getY(i), dfLower);
                poPoly->addRingDirectly(poRing);
                poGeom->addGeometryDirectly(poPoly);
            }
            // Build upper face
            {
                auto poPoly = new OGRPolygon();
                auto poRing = new OGRLinearRing();
                for (int i = 0; i < poLS->getNumPoints(); ++i)
                {
                    poRing->addPoint(poLS->getX(i), poLS->getY(i), dfUpper);
                }
                poPoly->addRingDirectly(poRing);
                poGeom->addGeometryDirectly(poPoly);
            }
            return poGeom;
        }
        else
        {
            if (bWarn)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unsupported base geometry type for Prism");
            }
            return nullptr;
        }
    }
    else
    {
        if (bWarn)
        {
            CPLError(CE_Warning, CPLE_AppDefined, "Unhandled place.type = %s",
                     pszType);
        }
        return nullptr;
    }
}

/************************************************************************/
/*            OGRJSONFGReader::GenerateLayerDefnFromFeature()           */
/************************************************************************/

bool OGRJSONFGReader::GenerateLayerDefnFromFeature(json_object *poObj)
{
    const GeoJSONObject::Type objType = OGRGeoJSONGetType(poObj);
    if (objType != GeoJSONObject::eFeature)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Did not get a Feature");
        return false;
    }

    const char *psLayerName = GetLayerNameForFeature(poObj);

    auto oBuildContextIter = oMapBuildContext_.find(psLayerName);
    if (oBuildContextIter == oMapBuildContext_.end())
    {
        LayerDefnBuildContext oContext;
        oMapBuildContext_[psLayerName] = std::move(oContext);
        oBuildContextIter = oMapBuildContext_.find(psLayerName);
    }
    LayerDefnBuildContext *poContext = &(oBuildContextIter->second);

    ++poContext->nFeatureCount;

    json_object *poCoordRefSys = nullptr;
    json_object *poPlace = nullptr;
    if (eGeometryElement_ != GeometryElement::GEOMETRY)
    {
        poPlace = CPL_json_object_object_get(poObj, "place");
        if (poPlace && json_object_get_type(poPlace) == json_type_object)
        {
            poCoordRefSys = CPL_json_object_object_get(poPlace, "coordRefSys");
        }
        if (!poCoordRefSys)
            poCoordRefSys = CPL_json_object_object_get(poObj, "coordRefSys");

        if (poCoordRefSys)
        {
            std::string osVal = json_object_to_json_string(poCoordRefSys);
            if (!poContext->bHasCoordRefSysAtFeatureLevel)
            {
                poContext->bHasCoordRefSysAtFeatureLevel = true;
                poContext->osCoordRefSysAtFeatureLevel = std::move(osVal);
                poContext->poCRSAtFeatureLevel =
                    OGRJSONFGReadCoordRefSys(poCoordRefSys);
                if (poContext->poCRSAtFeatureLevel)
                {
                    poContext->poCRSAtFeatureLevel->SetAxisMappingStrategy(
                        OAMS_TRADITIONAL_GIS_ORDER);
                }
            }
            else if (poContext->osCoordRefSysAtFeatureLevel != osVal)
            {
                poContext->osCoordRefSysAtFeatureLevel.clear();
                poContext->poCRSAtFeatureLevel.reset();
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Deal with place / geometry                                      */
    /* -------------------------------------------------------------------- */

    if (poContext->bDetectLayerGeomType)
    {
        bool bFallbackToGeometry =
            (eGeometryElement_ != GeometryElement::PLACE);
        if (poPlace && json_object_get_type(poPlace) == json_type_object)
        {
            const auto eType = OGRGeoJSONGetOGRGeometryType(poPlace);
            if (eType == wkbUnknown)
            {
                auto poGeom =
                    OGRJSONFGCreateNonGeoJSONGeometry(poPlace, /*bWarn=*/true);
                if (poGeom)
                {
                    bFallbackToGeometry = false;
                    poContext->bDetectLayerGeomType =
                        OGRGeoJSONUpdateLayerGeomType(
                            poContext->bFirstGeometry,
                            poGeom->getGeometryType(),
                            poContext->eLayerGeomType);
                }
            }
            else
            {
                bFallbackToGeometry = false;
                poContext->bDetectLayerGeomType = OGRGeoJSONUpdateLayerGeomType(
                    poContext->bFirstGeometry, eType,
                    poContext->eLayerGeomType);
            }
        }

        if (bFallbackToGeometry)
        {
            json_object *poGeomObj =
                CPL_json_object_object_get(poObj, "geometry");
            if (poGeomObj &&
                json_object_get_type(poGeomObj) == json_type_object)
            {
                const auto eType = OGRGeoJSONGetOGRGeometryType(poGeomObj);
                poContext->bDetectLayerGeomType = OGRGeoJSONUpdateLayerGeomType(
                    poContext->bFirstGeometry, eType,
                    poContext->eLayerGeomType);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Deal with time                                                  */
    /* -------------------------------------------------------------------- */
    json_object *poTime = CPL_json_object_object_get(poObj, "time");
    if (poTime)
    {
        json_object *poDate = CPL_json_object_object_get(poTime, "date");
        if (poDate && json_object_get_type(poDate) == json_type_string)
            poContext->bHasTimeDate = true;

        json_object *poTimestamp =
            CPL_json_object_object_get(poTime, "timestamp");
        if (poTimestamp &&
            json_object_get_type(poTimestamp) == json_type_string)
            poContext->bHasTimeTimestamp = true;

        json_object *poInterval =
            CPL_json_object_object_get(poTime, "interval");
        if (poInterval && json_object_get_type(poInterval) == json_type_array &&
            json_object_array_length(poInterval) == 2)
        {
            json_object *poStart = json_object_array_get_idx(poInterval, 0);
            if (poStart && json_object_get_type(poStart) == json_type_string)
            {
                const char *pszStart = json_object_get_string(poStart);
                if (strchr(pszStart, 'Z'))
                    poContext->bHasTimeIntervalStartTimestamp = true;
                else if (strcmp(pszStart, "..") != 0)
                    poContext->bHasTimeIntervalStartDate = true;
            }

            json_object *poEnd = json_object_array_get_idx(poInterval, 1);
            if (poEnd && json_object_get_type(poEnd) == json_type_string)
            {
                const char *pszEnd = json_object_get_string(poEnd);
                if (strchr(pszEnd, 'Z'))
                    poContext->bHasTimeIntervalEndTimestamp = true;
                else if (strcmp(pszEnd, "..") != 0)
                    poContext->bHasTimeIntervalEndDate = true;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Read collection of properties.                                  */
    /* -------------------------------------------------------------------- */
    json_object *poObjProps = CPL_json_object_object_get(poObj, "properties");

    int nPrevFieldIdx = -1;

    // First deal with id, either at top level or in properties["id"]
    OGRGeoJSONGenerateFeatureDefnDealWithID(
        poObj, poObjProps, nPrevFieldIdx, poContext->oMapFieldNameToIdx,
        poContext->apoFieldDefn, poContext->dag,
        poContext->bFeatureLevelIdAsFID, poContext->bFeatureLevelIdAsAttribute,
        poContext->bNeedFID64);

    if (nullptr != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object)
    {
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        std::vector<int> anCurFieldIndices;
        json_object_object_foreachC(poObjProps, it)
        {
            anCurFieldIndices.clear();
            OGRGeoJSONReaderAddOrUpdateField(
                anCurFieldIndices, poContext->oMapFieldNameToIdx,
                poContext->apoFieldDefn, it.key, it.val,
                bFlattenNestedAttributes_, chNestedAttributeSeparator_,
                bArrayAsString_, bDateAsString_,
                poContext->aoSetUndeterminedTypeFields);
            for (int idx : anCurFieldIndices)
            {
                poContext->dag.addNode(
                    idx, poContext->apoFieldDefn[idx]->GetNameRef());
                if (nPrevFieldIdx != -1)
                {
                    poContext->dag.addEdge(nPrevFieldIdx, idx);
                }
                nPrevFieldIdx = idx;
            }
        }
    }

    return true;
}

/************************************************************************/
/*                  OGRJSONFGReader::ReadFeature()                      */
/************************************************************************/

std::unique_ptr<OGRFeature>
OGRJSONFGReader::ReadFeature(json_object *poObj, const char *pszRequestedLayer,
                             OGRJSONFGMemLayer **pOutMemLayer,
                             OGRJSONFGStreamedLayer **pOutStreamedLayer)
{
    const char *pszLayerName = GetLayerNameForFeature(poObj);
    if (pszRequestedLayer && strcmp(pszLayerName, pszRequestedLayer) != 0)
        return nullptr;

    auto oBuildContextIter = oMapBuildContext_.find(pszLayerName);
    CPLAssert(oBuildContextIter != oMapBuildContext_.end());
    auto &oBuildContext = oBuildContextIter->second;
    OGRLayer *poLayer =
        oBuildContext.poStreamedLayer
            ? static_cast<OGRLayer *>(oBuildContext.poStreamedLayer)
            : static_cast<OGRLayer *>(oBuildContext.poMemLayer);

    if (pOutMemLayer)
        *pOutMemLayer = oBuildContext.poMemLayer;
    else if (pOutStreamedLayer)
        *pOutStreamedLayer = oBuildContext.poStreamedLayer;

    OGRFeatureDefn *poFDefn = poLayer->GetLayerDefn();
    auto poFeature = std::make_unique<OGRFeature>(poFDefn);

    /* -------------------------------------------------------------------- */
    /*      Translate GeoJSON "properties" object to feature attributes.    */
    /* -------------------------------------------------------------------- */

    json_object *poObjProps = CPL_json_object_object_get(poObj, "properties");
    if (nullptr != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object)
    {
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poObjProps, it)
        {
            const int nField = poFDefn->GetFieldIndexCaseSensitive(it.key);
            if (nField < 0 &&
                !(bFlattenNestedAttributes_ && it.val != nullptr &&
                  json_object_get_type(it.val) == json_type_object))
            {
                CPLDebug("JSONFG", "Cannot find field %s", it.key);
            }
            else
            {
                OGRGeoJSONReaderSetField(
                    poLayer, poFeature.get(), nField, it.key, it.val,
                    bFlattenNestedAttributes_, chNestedAttributeSeparator_);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to use feature-level ID if available                        */
    /*      and of integral type. Otherwise, leave unset (-1) then index    */
    /*      in features sequence will be used as FID.                       */
    /* -------------------------------------------------------------------- */
    json_object *poObjId = CPL_json_object_object_get(poObj, "id");
    if (nullptr != poObjId && oBuildContext.bFeatureLevelIdAsFID)
    {
        poFeature->SetFID(static_cast<GIntBig>(json_object_get_int64(poObjId)));
    }

    /* -------------------------------------------------------------------- */
    /*      Handle the case where the special id is in a regular field.     */
    /* -------------------------------------------------------------------- */
    else if (nullptr != poObjId)
    {
        const int nIdx = poFDefn->GetFieldIndexCaseSensitive("id");
        if (nIdx >= 0 && !poFeature->IsFieldSet(nIdx))
        {
            poFeature->SetField(nIdx, json_object_get_string(poObjId));
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Deal with time                                                  */
    /* -------------------------------------------------------------------- */
    json_object *poTime = CPL_json_object_object_get(poObj, "time");
    if (poTime)
    {
        json_object *poDate = CPL_json_object_object_get(poTime, "date");
        if (poDate && json_object_get_type(poDate) == json_type_string)
        {
            poFeature->SetField(oBuildContext.nIdxFieldTime,
                                json_object_get_string(poDate));
        }

        json_object *poTimestamp =
            CPL_json_object_object_get(poTime, "timestamp");
        if (poTimestamp &&
            json_object_get_type(poTimestamp) == json_type_string)
        {
            poFeature->SetField(oBuildContext.nIdxFieldTime,
                                json_object_get_string(poTimestamp));
        }

        json_object *poInterval =
            CPL_json_object_object_get(poTime, "interval");
        if (poInterval && json_object_get_type(poInterval) == json_type_array &&
            json_object_array_length(poInterval) == 2)
        {
            json_object *poStart = json_object_array_get_idx(poInterval, 0);
            if (poStart && json_object_get_type(poStart) == json_type_string)
            {
                const char *pszStart = json_object_get_string(poStart);
                if (strcmp(pszStart, "..") != 0)
                    poFeature->SetField(oBuildContext.nIdxFieldTimeStart,
                                        pszStart);
            }

            json_object *poEnd = json_object_array_get_idx(poInterval, 1);
            if (poEnd && json_object_get_type(poEnd) == json_type_string)
            {
                const char *pszEnd = json_object_get_string(poEnd);
                if (strcmp(pszEnd, "..") != 0)
                    poFeature->SetField(oBuildContext.nIdxFieldTimeEnd, pszEnd);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Translate "place" (and fallback to "geometry") sub-object       */
    /* -------------------------------------------------------------------- */
    json_object *poPlace = nullptr;
    bool bFallbackToGeometry = (eGeometryElement_ != GeometryElement::PLACE);

    if (eGeometryElement_ != GeometryElement::GEOMETRY)
    {
        poPlace = CPL_json_object_object_get(poObj, "place");
    }
    if (poPlace && json_object_get_type(poPlace) == json_type_object)
    {
        json_object *poCoordRefSys = nullptr;
        if (!oBuildContext.poCRSAtFeatureLevel)
        {
            poCoordRefSys = CPL_json_object_object_get(poPlace, "coordRefSys");
            if (!poCoordRefSys)
            {
                poCoordRefSys =
                    CPL_json_object_object_get(poObj, "coordRefSys");
            }
        }

        std::unique_ptr<OGRGeometry> poGeometry;
        json_object *poObjType = CPL_json_object_object_get(poPlace, "type");
        const char *pszType = json_object_get_string(poObjType);
        if (pszType && (strcmp(pszType, "Polyhedron") == 0 ||
                        strcmp(pszType, "Prism") == 0))
        {
            poGeometry =
                OGRJSONFGCreateNonGeoJSONGeometry(poPlace, /* bWarn=*/false);
        }
        else
        {
            poGeometry.reset(OGRGeoJSONReadGeometry(poPlace, nullptr));
        }
        if (poGeometry)
            bFallbackToGeometry = false;

        auto poLayerSRS = poLayer->GetSpatialRef();
        if (!poGeometry)
        {
            // nothing to do
        }
        else if (poCoordRefSys)
        {
            auto poFeatureCRS = OGRJSONFGReadCoordRefSys(poCoordRefSys);
            if (poFeatureCRS)
            {
                poFeatureCRS->SetAxisMappingStrategy(
                    OAMS_TRADITIONAL_GIS_ORDER);
                const bool bFeatureCRSNeedSwapXY =
                    OGRJSONFGMustSwapXY(poFeatureCRS.get());
                if (poLayerSRS)
                {
                    // Both feature and layer-level CRS. Reproject if needed
                    if (!poFeatureCRS->IsSame(poLayerSRS))
                    {
                        auto poCT =
                            std::unique_ptr<OGRCoordinateTransformation>(
                                OGRCreateCoordinateTransformation(
                                    poFeatureCRS.get(), poLayerSRS));
                        if (poCT)
                        {
                            if (bFeatureCRSNeedSwapXY)
                                poGeometry->swapXY();
                            if (poGeometry->transform(poCT.get()) ==
                                OGRERR_NONE)
                            {
                                poGeometry->assignSpatialReference(poLayerSRS);
                                poFeature->SetGeometryDirectly(
                                    poGeometry.release());
                            }
                        }
                    }
                    else
                    {
                        poGeometry->assignSpatialReference(poLayerSRS);
                        if (oBuildContext.bSwapPlacesXY)
                            poGeometry->swapXY();
                        poFeature->SetGeometryDirectly(poGeometry.release());
                    }
                }
                else
                {
                    // No layer-level CRS
                    auto poFeatureCRSBorrowed = poFeatureCRS.release();
                    poGeometry->assignSpatialReference(poFeatureCRSBorrowed);
                    poFeatureCRSBorrowed->Release();
                    if (bFeatureCRSNeedSwapXY)
                        poGeometry->swapXY();
                    poFeature->SetGeometryDirectly(poGeometry.release());
                }
            }
        }
        else
        {
            poGeometry->assignSpatialReference(poLayerSRS);
            if (oBuildContext.bSwapPlacesXY)
                poGeometry->swapXY();
            poFeature->SetGeometryDirectly(poGeometry.release());
        }
    }

    if (bFallbackToGeometry &&
        (oBuildContext.poCTWGS84ToLayerCRS || oBuildContext.bLayerCRSIsWGS84))
    {
        json_object *poGeomObj = CPL_json_object_object_get(poObj, "geometry");
        if (nullptr != poGeomObj)
        {
            auto poGeometry = std::unique_ptr<OGRGeometry>(
                OGRGeoJSONReadGeometry(poGeomObj, nullptr));
            if (poGeometry)
            {
                if (oBuildContext.poCTWGS84ToLayerCRS)
                {
                    if (poGeometry->transform(
                            oBuildContext.poCTWGS84ToLayerCRS.get()) ==
                        OGRERR_NONE)
                    {
                        poGeometry->assignSpatialReference(
                            poLayer->GetSpatialRef());
                        poFeature->SetGeometryDirectly(poGeometry.release());
                    }
                }
                else /* if (oBuildContext.bLayerCRSIsWGS84) */
                {
                    poGeometry->assignSpatialReference(
                        poLayer->GetSpatialRef());
                    poFeature->SetGeometryDirectly(poGeometry.release());
                }
            }
        }
    }

    return poFeature;
}
