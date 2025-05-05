/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONDataSource class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_geojson.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_http.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_error.h"
#include "json.h"
// #include "json_object.h"
#include "gdal_utils.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogrlibjsonutils.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogrgeojsonwriter.h"
#include "ogrsf_frmts.h"
#include "ogr_schema_override.h"

// #include "symbol_renames.h"

/************************************************************************/
/*                           OGRGeoJSONDataSource()                     */
/************************************************************************/

OGRGeoJSONDataSource::OGRGeoJSONDataSource()
    : pszName_(nullptr), pszGeoData_(nullptr), nGeoDataLen_(0),
      papoLayers_(nullptr), papoLayersWriter_(nullptr), nLayers_(0),
      fpOut_(nullptr), flTransGeom_(OGRGeoJSONDataSource::eGeometryPreserve),
      flTransAttrs_(OGRGeoJSONDataSource::eAttributesPreserve),
      bOtherPages_(false), bFpOutputIsSeekable_(false), nBBOXInsertLocation_(0),
      bUpdatable_(false)
{
}

/************************************************************************/
/*                           ~OGRGeoJSONDataSource()                    */
/************************************************************************/

OGRGeoJSONDataSource::~OGRGeoJSONDataSource()
{
    OGRGeoJSONDataSource::Close();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr OGRGeoJSONDataSource::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (OGRGeoJSONDataSource::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        if (!OGRGeoJSONDataSource::Clear())
            eErr = CE_Failure;

        if (GDALDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                 DealWithOgrSchemaOpenOption()                       */
/************************************************************************/

bool OGRGeoJSONDataSource::DealWithOgrSchemaOpenOption(
    const GDALOpenInfo *poOpenInfo)
{

    std::string osFieldsSchemaOverrideParam =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "OGR_SCHEMA", "");

    if (!osFieldsSchemaOverrideParam.empty())
    {

        if (poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "OGR_SCHEMA open option is not supported in update mode.");
            return false;
        }

        OGRSchemaOverride osSchemaOverride;
        if (!osSchemaOverride.LoadFromJSON(osFieldsSchemaOverrideParam) ||
            !osSchemaOverride.IsValid())
        {
            return false;
        }

        const auto &oLayerOverrides = osSchemaOverride.GetLayerOverrides();
        for (const auto &oLayer : oLayerOverrides)
        {
            const auto &oLayerName = oLayer.first;
            const auto &oLayerFieldOverride = oLayer.second;
            const bool bIsFullOverride{oLayerFieldOverride.IsFullOverride()};
            auto oFieldOverrides = oLayerFieldOverride.GetFieldOverrides();
            std::vector<OGRFieldDefn *> aoFields;

            CPLDebug("GeoJSON", "Applying schema override for layer %s",
                     oLayerName.c_str());

            // Fail if the layer name does not exist
            auto poLayer = GetLayerByName(oLayerName.c_str());
            if (poLayer == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer %s not found in GeoJSON file",
                         oLayerName.c_str());
                return false;
            }

            // Patch field definitions
            auto poLayerDefn = poLayer->GetLayerDefn();
            for (int i = 0; i < poLayerDefn->GetFieldCount(); i++)
            {
                auto poFieldDefn = poLayerDefn->GetFieldDefn(i);
                auto oFieldOverride =
                    oFieldOverrides.find(poFieldDefn->GetNameRef());
                if (oFieldOverride != oFieldOverrides.cend())
                {
                    if (oFieldOverride->second.GetFieldType().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetType(
                                oFieldOverride->second.GetFieldType().value());
                    if (oFieldOverride->second.GetFieldWidth().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetWidth(
                                oFieldOverride->second.GetFieldWidth().value());
                    if (oFieldOverride->second.GetFieldPrecision().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetPrecision(
                                oFieldOverride->second.GetFieldPrecision()
                                    .value());
                    if (oFieldOverride->second.GetFieldSubType().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetSubType(
                                oFieldOverride->second.GetFieldSubType()
                                    .value());
                    if (oFieldOverride->second.GetFieldName().has_value())
                        whileUnsealing(poFieldDefn)
                            ->SetName(oFieldOverride->second.GetFieldName()
                                          .value()
                                          .c_str());

                    if (bIsFullOverride)
                    {
                        aoFields.push_back(poFieldDefn);
                    }
                    oFieldOverrides.erase(oFieldOverride);
                }
            }

            // Error if any field override is not found
            if (!oFieldOverrides.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %s not found in layer %s",
                         oFieldOverrides.cbegin()->first.c_str(),
                         oLayerName.c_str());
                return false;
            }

            // Remove fields not in the override
            if (bIsFullOverride)
            {
                for (int i = poLayerDefn->GetFieldCount() - 1; i >= 0; i--)
                {
                    auto poFieldDefn = poLayerDefn->GetFieldDefn(i);
                    if (std::find(aoFields.begin(), aoFields.end(),
                                  poFieldDefn) == aoFields.end())
                    {
                        whileUnsealing(poLayerDefn)->DeleteFieldDefn(i);
                    }
                }
            }
        }
    }
    return true;
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

int OGRGeoJSONDataSource::Open(GDALOpenInfo *poOpenInfo,
                               GeoJSONSourceType nSrcType,
                               const char *pszJSonFlavor)
{
    osJSonFlavor_ = pszJSonFlavor;

    const char *pszUnprefixed = poOpenInfo->pszFilename;
    if (STARTS_WITH_CI(pszUnprefixed, pszJSonFlavor) &&
        pszUnprefixed[strlen(pszJSonFlavor)] == ':')
    {
        pszUnprefixed += strlen(pszJSonFlavor) + 1;
    }

    if (eGeoJSONSourceService == nSrcType)
    {
        if (!ReadFromService(poOpenInfo, pszUnprefixed))
            return FALSE;
        if (poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update from remote service not supported");
            return FALSE;
        }
    }
    else if (eGeoJSONSourceText == nSrcType)
    {
        if (poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update from inline definition not supported");
            return FALSE;
        }
        pszGeoData_ = CPLStrdup(pszUnprefixed);
    }
    else if (eGeoJSONSourceFile == nSrcType)
    {
        if (poOpenInfo->eAccess == GA_Update &&
            !EQUAL(pszJSonFlavor, "GeoJSON"))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update of %s not supported", pszJSonFlavor);
            return FALSE;
        }
        pszName_ = CPLStrdup(pszUnprefixed);
        bUpdatable_ = (poOpenInfo->eAccess == GA_Update);

        if (!EQUAL(pszUnprefixed, poOpenInfo->pszFilename))
        {
            GDALOpenInfo oOpenInfo(pszUnprefixed, GA_ReadOnly);
            if (oOpenInfo.fpL == nullptr || oOpenInfo.pabyHeader == nullptr)
                return FALSE;
            pszGeoData_ =
                CPLStrdup(reinterpret_cast<const char *>(oOpenInfo.pabyHeader));
        }
        else if (poOpenInfo->fpL == nullptr)
            return FALSE;
        else
        {
            pszGeoData_ = CPLStrdup(
                reinterpret_cast<const char *>(poOpenInfo->pabyHeader));
        }
    }
    else
    {
        Clear();
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Construct OGR layer and feature objects from                    */
    /*      GeoJSON text tree.                                              */
    /* -------------------------------------------------------------------- */
    if (nullptr == pszGeoData_ ||
        STARTS_WITH(pszGeoData_, "{\"couchdb\":\"Welcome\"") ||
        STARTS_WITH(pszGeoData_, "{\"db_name\":\"") ||
        STARTS_WITH(pszGeoData_, "{\"total_rows\":") ||
        STARTS_WITH(pszGeoData_, "{\"rows\":["))
    {
        Clear();
        return FALSE;
    }

    SetDescription(poOpenInfo->pszFilename);
    LoadLayers(poOpenInfo, nSrcType, pszUnprefixed, pszJSonFlavor);

    if (!DealWithOgrSchemaOpenOption(poOpenInfo))
    {
        Clear();
        return FALSE;
    }

    if (nLayers_ == 0)
    {
        bool bEmitError = true;
        if (eGeoJSONSourceService == nSrcType)
        {
            const CPLString osTmpFilename =
                VSIMemGenerateHiddenFilename(CPLSPrintf(
                    "geojson_%s", CPLGetFilename(poOpenInfo->pszFilename)));
            VSIFCloseL(VSIFileFromMemBuffer(osTmpFilename, (GByte *)pszGeoData_,
                                            nGeoDataLen_, TRUE));
            pszGeoData_ = nullptr;
            if (GDALIdentifyDriver(osTmpFilename, nullptr))
                bEmitError = false;
            VSIUnlink(osTmpFilename);
        }
        Clear();

        if (bEmitError)
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "Failed to read %s data",
                     pszJSonFlavor);
        }
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRGeoJSONDataSource::GetLayerCount()
{
    return nLayers_;
}

/************************************************************************/
/*                           GetLayer()                                 */
/************************************************************************/

OGRLayer *OGRGeoJSONDataSource::GetLayer(int nLayer)
{
    if (0 <= nLayer && nLayer < nLayers_)
    {
        if (papoLayers_)
            return papoLayers_[nLayer];
        else
            return papoLayersWriter_[nLayer];
    }

    return nullptr;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRGeoJSONDataSource::ICreateLayer(const char *pszNameIn,
                                   const OGRGeomFieldDefn *poSrcGeomFieldDefn,
                                   CSLConstList papszOptions)
{
    if (nullptr == fpOut_)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSON driver doesn't support creating a layer "
                 "on a read-only datasource");
        return nullptr;
    }

    if (nLayers_ != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GeoJSON driver doesn't support creating more than one layer");
        return nullptr;
    }

    const auto eGType =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetType() : wkbNone;
    const auto poSRS =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetSpatialRef() : nullptr;

    const char *pszForeignMembersCollection =
        CSLFetchNameValue(papszOptions, "FOREIGN_MEMBERS_COLLECTION");
    if (pszForeignMembersCollection)
    {
        if (pszForeignMembersCollection[0] != '{' ||
            pszForeignMembersCollection[strlen(pszForeignMembersCollection) -
                                        1] != '}')
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Value of FOREIGN_MEMBERS_COLLECTION should start with { "
                     "and end with }");
            return nullptr;
        }
        json_object *poTmp = nullptr;
        if (!OGRJSonParse(pszForeignMembersCollection, &poTmp, false))
        {
            pszForeignMembersCollection = nullptr;
        }
        json_object_put(poTmp);
        if (!pszForeignMembersCollection)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Value of FOREIGN_MEMBERS_COLLECTION is invalid JSON");
            return nullptr;
        }
    }

    std::string osForeignMembersFeature =
        CSLFetchNameValueDef(papszOptions, "FOREIGN_MEMBERS_FEATURE", "");
    if (!osForeignMembersFeature.empty())
    {
        if (osForeignMembersFeature.front() != '{' ||
            osForeignMembersFeature.back() != '}')
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Value of FOREIGN_MEMBERS_FEATURE should start with { and "
                     "end with }");
            return nullptr;
        }
        json_object *poTmp = nullptr;
        if (!OGRJSonParse(osForeignMembersFeature.c_str(), &poTmp, false))
        {
            osForeignMembersFeature.clear();
        }
        json_object_put(poTmp);
        if (osForeignMembersFeature.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Value of FOREIGN_MEMBERS_FEATURE is invalid JSON");
            return nullptr;
        }
    }

    VSIFPrintfL(fpOut_, "{\n\"type\": \"FeatureCollection\",\n");

    if (pszForeignMembersCollection)
    {
        VSIFWriteL(pszForeignMembersCollection + 1, 1,
                   strlen(pszForeignMembersCollection) - 2, fpOut_);
        VSIFWriteL(",\n", 2, 1, fpOut_);
    }

    bool bWriteFC_BBOX =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "WRITE_BBOX", "FALSE"));

    const bool bRFC7946 =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "RFC7946", "FALSE"));

    const char *pszNativeData = CSLFetchNameValue(papszOptions, "NATIVE_DATA");
    const char *pszNativeMediaType =
        CSLFetchNameValue(papszOptions, "NATIVE_MEDIA_TYPE");
    bool bWriteCRSIfWGS84 = true;
    bool bFoundNameInNativeData = false;
    if (pszNativeData && pszNativeMediaType &&
        EQUAL(pszNativeMediaType, "application/vnd.geo+json"))
    {
        json_object *poObj = nullptr;
        if (OGRJSonParse(pszNativeData, &poObj) &&
            json_object_get_type(poObj) == json_type_object)
        {
            json_object_iter it;
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
            CPLString osNativeData;
            bWriteCRSIfWGS84 = false;
            json_object_object_foreachC(poObj, it)
            {
                if (strcmp(it.key, "type") == 0 ||
                    strcmp(it.key, "features") == 0)
                {
                    continue;
                }
                if (strcmp(it.key, "bbox") == 0)
                {
                    if (CSLFetchNameValue(papszOptions, "WRITE_BBOX") ==
                        nullptr)
                        bWriteFC_BBOX = true;
                    continue;
                }
                if (strcmp(it.key, "crs") == 0)
                {
                    if (!bRFC7946)
                        bWriteCRSIfWGS84 = true;
                    continue;
                }
                // See https://tools.ietf.org/html/rfc7946#section-7.1
                if (bRFC7946 && (strcmp(it.key, "coordinates") == 0 ||
                                 strcmp(it.key, "geometries") == 0 ||
                                 strcmp(it.key, "geometry") == 0 ||
                                 strcmp(it.key, "properties") == 0))
                {
                    continue;
                }

                if (strcmp(it.key, "name") == 0)
                {
                    bFoundNameInNativeData = true;
                    if (!CPLFetchBool(papszOptions, "WRITE_NAME", true) ||
                        CSLFetchNameValue(papszOptions, "@NAME") != nullptr)
                    {
                        continue;
                    }
                }

                // If a native description exists, ignore it if an explicit
                // DESCRIPTION option has been provided.
                if (strcmp(it.key, "description") == 0 &&
                    CSLFetchNameValue(papszOptions, "DESCRIPTION"))
                {
                    continue;
                }

                if (strcmp(it.key, "xy_coordinate_resolution") == 0 ||
                    strcmp(it.key, "z_coordinate_resolution") == 0)
                {
                    continue;
                }

                json_object *poKey = json_object_new_string(it.key);
                VSIFPrintfL(fpOut_, "%s: ", json_object_to_json_string(poKey));
                json_object_put(poKey);
                VSIFPrintfL(fpOut_, "%s,\n",
                            json_object_to_json_string(it.val));
            }
            json_object_put(poObj);
        }
    }

    // Used by ogr2ogr in -nln mode
    const char *pszAtName = CSLFetchNameValue(papszOptions, "@NAME");
    if (pszAtName && CPLFetchBool(papszOptions, "WRITE_NAME", true))
    {
        json_object *poName = json_object_new_string(pszAtName);
        VSIFPrintfL(fpOut_, "\"name\": %s,\n",
                    json_object_to_json_string(poName));
        json_object_put(poName);
    }
    else if (!bFoundNameInNativeData &&
             CPLFetchBool(papszOptions, "WRITE_NAME", true) &&
             !EQUAL(pszNameIn, OGRGeoJSONLayer::DefaultName) &&
             !EQUAL(pszNameIn, ""))
    {
        json_object *poName = json_object_new_string(pszNameIn);
        VSIFPrintfL(fpOut_, "\"name\": %s,\n",
                    json_object_to_json_string(poName));
        json_object_put(poName);
    }

    const char *pszDescription = CSLFetchNameValue(papszOptions, "DESCRIPTION");
    if (pszDescription)
    {
        json_object *poDesc = json_object_new_string(pszDescription);
        VSIFPrintfL(fpOut_, "\"description\": %s,\n",
                    json_object_to_json_string(poDesc));
        json_object_put(poDesc);
    }

    OGRCoordinateTransformation *poCT = nullptr;
    if (bRFC7946)
    {
        if (poSRS == nullptr)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "No SRS set on layer. Assuming it is long/lat on WGS84 "
                     "ellipsoid");
        }
        else if (poSRS->GetAxesCount() == 3)
        {
            OGRSpatialReference oSRS_EPSG_4979;
            oSRS_EPSG_4979.importFromEPSG(4979);
            oSRS_EPSG_4979.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (!poSRS->IsSame(&oSRS_EPSG_4979))
            {
                poCT =
                    OGRCreateCoordinateTransformation(poSRS, &oSRS_EPSG_4979);
                if (poCT == nullptr)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Failed to create coordinate transformation "
                             "between the "
                             "input coordinate system and WGS84.");

                    return nullptr;
                }
            }
        }
        else
        {
            OGRSpatialReference oSRSWGS84;
            oSRSWGS84.SetWellKnownGeogCS("WGS84");
            oSRSWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (!poSRS->IsSame(&oSRSWGS84))
            {
                poCT = OGRCreateCoordinateTransformation(poSRS, &oSRSWGS84);
                if (poCT == nullptr)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Failed to create coordinate transformation "
                             "between the "
                             "input coordinate system and WGS84.");

                    return nullptr;
                }
            }
        }
    }
    else if (poSRS)
    {
        char *pszOGCURN = poSRS->GetOGCURN();
        if (pszOGCURN != nullptr &&
            (bWriteCRSIfWGS84 ||
             !EQUAL(pszOGCURN, "urn:ogc:def:crs:EPSG::4326")))
        {
            json_object *poObjCRS = json_object_new_object();
            json_object_object_add(poObjCRS, "type",
                                   json_object_new_string("name"));
            json_object *poObjProperties = json_object_new_object();
            json_object_object_add(poObjCRS, "properties", poObjProperties);

            if (EQUAL(pszOGCURN, "urn:ogc:def:crs:EPSG::4326"))
            {
                json_object_object_add(
                    poObjProperties, "name",
                    json_object_new_string("urn:ogc:def:crs:OGC:1.3:CRS84"));
            }
            else
            {
                json_object_object_add(poObjProperties, "name",
                                       json_object_new_string(pszOGCURN));
            }

            const char *pszCRS = json_object_to_json_string(poObjCRS);
            VSIFPrintfL(fpOut_, "\"crs\": %s,\n", pszCRS);

            json_object_put(poObjCRS);
        }
        CPLFree(pszOGCURN);
    }

    CPLStringList aosOptions(papszOptions);

    double dfXYResolution = OGRGeomCoordinatePrecision::UNKNOWN;
    double dfZResolution = OGRGeomCoordinatePrecision::UNKNOWN;

    if (const char *pszCoordPrecision =
            CSLFetchNameValue(papszOptions, "COORDINATE_PRECISION"))
    {
        dfXYResolution = std::pow(10.0, -CPLAtof(pszCoordPrecision));
        dfZResolution = dfXYResolution;
        VSIFPrintfL(fpOut_, "\"xy_coordinate_resolution\": %g,\n",
                    dfXYResolution);
        if (poSRS && poSRS->GetAxesCount() == 3)
        {
            VSIFPrintfL(fpOut_, "\"z_coordinate_resolution\": %g,\n",
                        dfZResolution);
        }
    }
    else if (poSrcGeomFieldDefn)
    {
        const auto &oCoordPrec = poSrcGeomFieldDefn->GetCoordinatePrecision();
        OGRSpatialReference oSRSWGS84;
        oSRSWGS84.SetWellKnownGeogCS("WGS84");
        const auto oCoordPrecWGS84 =
            oCoordPrec.ConvertToOtherSRS(poSRS, &oSRSWGS84);

        if (oCoordPrec.dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            dfXYResolution = poSRS && bRFC7946 ? oCoordPrecWGS84.dfXYResolution
                                               : oCoordPrec.dfXYResolution;

            aosOptions.SetNameValue(
                "XY_COORD_PRECISION",
                CPLSPrintf("%d",
                           OGRGeomCoordinatePrecision::ResolutionToPrecision(
                               dfXYResolution)));
            VSIFPrintfL(fpOut_, "\"xy_coordinate_resolution\": %g,\n",
                        dfXYResolution);
        }
        if (oCoordPrec.dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            dfZResolution = poSRS && bRFC7946 ? oCoordPrecWGS84.dfZResolution
                                              : oCoordPrec.dfZResolution;

            aosOptions.SetNameValue(
                "Z_COORD_PRECISION",
                CPLSPrintf("%d",
                           OGRGeomCoordinatePrecision::ResolutionToPrecision(
                               dfZResolution)));
            VSIFPrintfL(fpOut_, "\"z_coordinate_resolution\": %g,\n",
                        dfZResolution);
        }
    }

    if (bFpOutputIsSeekable_ && bWriteFC_BBOX)
    {
        nBBOXInsertLocation_ = static_cast<int>(VSIFTellL(fpOut_));

        const std::string osSpaceForBBOX(SPACE_FOR_BBOX + 1, ' ');
        VSIFPrintfL(fpOut_, "%s\n", osSpaceForBBOX.c_str());
    }

    VSIFPrintfL(fpOut_, "\"features\": [\n");

    OGRGeoJSONWriteLayer *poLayer = new OGRGeoJSONWriteLayer(
        pszNameIn, eGType, aosOptions.List(), bWriteFC_BBOX, poCT, this);

    if (eGType != wkbNone &&
        dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        auto poGeomFieldDefn = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
        OGRGeomCoordinatePrecision oCoordPrec(
            poGeomFieldDefn->GetCoordinatePrecision());
        oCoordPrec.dfXYResolution = dfXYResolution;
        poGeomFieldDefn->SetCoordinatePrecision(oCoordPrec);
    }

    if (eGType != wkbNone &&
        dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        auto poGeomFieldDefn = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
        OGRGeomCoordinatePrecision oCoordPrec(
            poGeomFieldDefn->GetCoordinatePrecision());
        oCoordPrec.dfZResolution = dfZResolution;
        poGeomFieldDefn->SetCoordinatePrecision(oCoordPrec);
    }

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    CPLAssert(papoLayers_ == nullptr);
    papoLayersWriter_ = static_cast<OGRGeoJSONWriteLayer **>(CPLRealloc(
        papoLayers_, sizeof(OGRGeoJSONWriteLayer *) * (nLayers_ + 1)));

    papoLayersWriter_[nLayers_++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONDataSource::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return fpOut_ != nullptr && nLayers_ == 0;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return m_bSupportsMGeometries;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return m_bSupportsZGeometries;

    return FALSE;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

int OGRGeoJSONDataSource::Create(const char *pszName,
                                 char ** /* papszOptions */)
{
    CPLAssert(nullptr == fpOut_);

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    bFpOutputIsSeekable_ = !(strcmp(pszName, "/vsistdout/") == 0 ||
                             STARTS_WITH(pszName, "/vsigzip/") ||
                             STARTS_WITH(pszName, "/vsizip/"));

    /* -------------------------------------------------------------------- */
    /*     File overwrite not supported.                                    */
    /* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;
    if (0 == VSIStatL(pszName, &sStatBuf))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The GeoJSON driver does not overwrite existing files.");
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    fpOut_ = VSIFOpenExL(pszName, "w", true);
    if (nullptr == fpOut_)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to create GeoJSON datasource: %s: %s", pszName,
                 VSIGetLastErrorMsg());
        return FALSE;
    }

    pszName_ = CPLStrdup(pszName);

    return TRUE;
}

/************************************************************************/
/*                           SetGeometryTranslation()                   */
/************************************************************************/

void OGRGeoJSONDataSource::SetGeometryTranslation(GeometryTranslation type)
{
    flTransGeom_ = type;
}

/************************************************************************/
/*                           SetAttributesTranslation()                 */
/************************************************************************/

void OGRGeoJSONDataSource::SetAttributesTranslation(AttributesTranslation type)
{
    flTransAttrs_ = type;
}

/************************************************************************/
/*                  PRIVATE FUNCTIONS IMPLEMENTATION                    */
/************************************************************************/

bool OGRGeoJSONDataSource::Clear()
{
    for (int i = 0; i < nLayers_; i++)
    {
        if (papoLayers_ != nullptr)
            delete papoLayers_[i];
        else
            delete papoLayersWriter_[i];
    }

    CPLFree(papoLayers_);
    papoLayers_ = nullptr;
    CPLFree(papoLayersWriter_);
    papoLayersWriter_ = nullptr;
    nLayers_ = 0;

    CPLFree(pszName_);
    pszName_ = nullptr;

    CPLFree(pszGeoData_);
    pszGeoData_ = nullptr;
    nGeoDataLen_ = 0;

    bool bRet = true;
    if (fpOut_)
    {
        if (VSIFCloseL(fpOut_) != 0)
            bRet = false;
        fpOut_ = nullptr;
    }
    return bRet;
}

/************************************************************************/
/*                           ReadFromFile()                             */
/************************************************************************/

int OGRGeoJSONDataSource::ReadFromFile(GDALOpenInfo *poOpenInfo,
                                       const char *pszUnprefixed)
{
    GByte *pabyOut = nullptr;
    if (!EQUAL(poOpenInfo->pszFilename, pszUnprefixed))
    {
        GDALOpenInfo oOpenInfo(pszUnprefixed, GA_ReadOnly);
        if (oOpenInfo.fpL == nullptr || oOpenInfo.pabyHeader == nullptr)
            return FALSE;
        VSIFSeekL(oOpenInfo.fpL, 0, SEEK_SET);
        if (!VSIIngestFile(oOpenInfo.fpL, pszUnprefixed, &pabyOut, nullptr, -1))
        {
            return FALSE;
        }
    }
    else
    {
        if (poOpenInfo->fpL == nullptr)
            return FALSE;
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
        if (!VSIIngestFile(poOpenInfo->fpL, poOpenInfo->pszFilename, &pabyOut,
                           nullptr, -1))
        {
            return FALSE;
        }

        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
    }

    CPLFree(pszGeoData_);
    pszGeoData_ = reinterpret_cast<char *>(pabyOut);

    CPLAssert(nullptr != pszGeoData_);

    return TRUE;
}

/************************************************************************/
/*                           ReadFromService()                          */
/************************************************************************/

int OGRGeoJSONDataSource::ReadFromService(GDALOpenInfo *poOpenInfo,
                                          const char *pszSource)
{
    CPLAssert(nullptr == pszGeoData_);
    CPLAssert(nullptr != pszSource);

    CPLErrorReset();

    /* -------------------------------------------------------------------- */
    /*      Look if we already cached the content.                          */
    /* -------------------------------------------------------------------- */
    char *pszStoredContent = OGRGeoJSONDriverStealStoredContent(pszSource);
    if (pszStoredContent != nullptr)
    {
        if (!EQUAL(pszStoredContent, INVALID_CONTENT_FOR_JSON_LIKE) &&
            ((osJSonFlavor_ == "ESRIJSON" &&
              ESRIJSONIsObject(pszStoredContent, poOpenInfo)) ||
             (osJSonFlavor_ == "TopoJSON" &&
              TopoJSONIsObject(pszStoredContent, poOpenInfo))))
        {
            pszGeoData_ = pszStoredContent;
            nGeoDataLen_ = strlen(pszGeoData_);

            pszName_ = CPLStrdup(pszSource);
            return true;
        }

        OGRGeoJSONDriverStoreContent(pszSource, pszStoredContent);
        return false;
    }

    /* -------------------------------------------------------------------- */
    /*      Fetch the GeoJSON result.                                        */
    /* -------------------------------------------------------------------- */
    CPLHTTPResult *pResult = GeoJSONHTTPFetchWithContentTypeHeader(pszSource);
    if (!pResult)
    {
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Copy returned GeoJSON data to text buffer.                      */
    /* -------------------------------------------------------------------- */
    char *pszData = reinterpret_cast<char *>(pResult->pabyData);

    // Directly assign CPLHTTPResult::pabyData to pszGeoData_.
    pszGeoData_ = pszData;
    nGeoDataLen_ = pResult->nDataLen;
    pResult->pabyData = nullptr;
    pResult->nDataLen = 0;

    pszName_ = CPLStrdup(pszSource);

    /* -------------------------------------------------------------------- */
    /*      Cleanup HTTP resources.                                         */
    /* -------------------------------------------------------------------- */
    CPLHTTPDestroyResult(pResult);

    CPLAssert(nullptr != pszGeoData_);

    /* -------------------------------------------------------------------- */
    /*      Cache the content if it is not handled by this driver, but      */
    /*      another related one.                                            */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszSource, poOpenInfo->pszFilename) && osJSonFlavor_ == "GeoJSON")
    {
        if (!GeoJSONIsObject(pszGeoData_, poOpenInfo))
        {
            if (ESRIJSONIsObject(pszGeoData_, poOpenInfo) ||
                TopoJSONIsObject(pszGeoData_, poOpenInfo) ||
                GeoJSONSeqIsObject(pszGeoData_, poOpenInfo) ||
                JSONFGIsObject(pszGeoData_, poOpenInfo))
            {
                OGRGeoJSONDriverStoreContent(pszSource, pszGeoData_);
                pszGeoData_ = nullptr;
                nGeoDataLen_ = 0;
            }
            else
            {
                OGRGeoJSONDriverStoreContent(
                    pszSource, CPLStrdup(INVALID_CONTENT_FOR_JSON_LIKE));
            }
            return false;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                       RemoveJSonPStuff()                             */
/************************************************************************/

void OGRGeoJSONDataSource::RemoveJSonPStuff()
{
    const char *const apszPrefix[] = {"loadGeoJSON(", "jsonp("};
    for (size_t iP = 0; iP < CPL_ARRAYSIZE(apszPrefix); iP++)
    {
        if (strncmp(pszGeoData_, apszPrefix[iP], strlen(apszPrefix[iP])) == 0)
        {
            const size_t nDataLen = strlen(pszGeoData_);
            memmove(pszGeoData_, pszGeoData_ + strlen(apszPrefix[iP]),
                    nDataLen - strlen(apszPrefix[iP]));
            size_t i = nDataLen - strlen(apszPrefix[iP]);
            pszGeoData_[i] = '\0';
            while (i > 0 && pszGeoData_[i] != ')')
            {
                i--;
            }
            pszGeoData_[i] = '\0';
        }
    }
}

/************************************************************************/
/*                           LoadLayers()                               */
/************************************************************************/

void OGRGeoJSONDataSource::LoadLayers(GDALOpenInfo *poOpenInfo,
                                      GeoJSONSourceType nSrcType,
                                      const char *pszUnprefixed,
                                      const char *pszJSonFlavor)
{
    if (nullptr == pszGeoData_)
    {
        CPLError(CE_Failure, CPLE_ObjectNull, "%s data buffer empty",
                 pszJSonFlavor);
        return;
    }

    if (nSrcType != eGeoJSONSourceFile)
    {
        RemoveJSonPStuff();
    }

    /* -------------------------------------------------------------------- */
    /*      Is it ESRI Feature Service data ?                               */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszJSonFlavor, "ESRIJSON"))
    {
        OGRESRIJSONReader reader;
        if (nSrcType == eGeoJSONSourceFile)
        {
            if (!ReadFromFile(poOpenInfo, pszUnprefixed))
                return;
        }
        OGRErr err = reader.Parse(pszGeoData_);
        if (OGRERR_NONE == err)
        {
            json_object *poObj = reader.GetJSonObject();
            CheckExceededTransferLimit(poObj);
            reader.ReadLayers(this, nSrcType);
        }
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Is it TopoJSON data ?                                           */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszJSonFlavor, "TOPOJSON"))
    {
        OGRTopoJSONReader reader;
        if (nSrcType == eGeoJSONSourceFile)
        {
            if (!ReadFromFile(poOpenInfo, pszUnprefixed))
                return;
        }
        OGRErr err = reader.Parse(
            pszGeoData_,
            nSrcType == eGeoJSONSourceService &&
                !STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:"));
        if (OGRERR_NONE == err)
        {
            reader.ReadLayers(this);
        }
        return;
    }

    VSILFILE *fp = nullptr;
    if (nSrcType == eGeoJSONSourceFile &&
        !EQUAL(poOpenInfo->pszFilename, pszUnprefixed))
    {
        GDALOpenInfo oOpenInfo(pszUnprefixed, GA_ReadOnly);
        if (oOpenInfo.fpL == nullptr || oOpenInfo.pabyHeader == nullptr)
            return;
        CPL_IGNORE_RET_VAL(oOpenInfo.TryToIngest(6000));
        CPLFree(pszGeoData_);
        pszGeoData_ =
            CPLStrdup(reinterpret_cast<const char *>(oOpenInfo.pabyHeader));
        fp = oOpenInfo.fpL;
        oOpenInfo.fpL = nullptr;
    }

    if (!GeoJSONIsObject(pszGeoData_, poOpenInfo))
    {
        CPLDebug(pszJSonFlavor, "No valid %s data found in source '%s'",
                 pszJSonFlavor, pszName_);
        if (fp)
            VSIFCloseL(fp);
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Configure GeoJSON format translator.                            */
    /* -------------------------------------------------------------------- */
    OGRGeoJSONReader *poReader = new OGRGeoJSONReader();
    SetOptionsOnReader(poOpenInfo, poReader);

    /* -------------------------------------------------------------------- */
    /*      Parse GeoJSON and build valid OGRLayer instance.                */
    /* -------------------------------------------------------------------- */
    bool bUseStreamingInterface = false;
    const GIntBig nMaxBytesFirstPass = CPLAtoGIntBig(
        CPLGetConfigOption("OGR_GEOJSON_MAX_BYTES_FIRST_PASS", "0"));
    if ((fp != nullptr || poOpenInfo->fpL != nullptr) &&
        (!STARTS_WITH(pszUnprefixed, "/vsistdin/") ||
         (nMaxBytesFirstPass > 0 && nMaxBytesFirstPass <= 1000000)))
    {
        const char *pszStr = strstr(pszGeoData_, "\"features\"");
        if (pszStr)
        {
            pszStr += strlen("\"features\"");
            while (*pszStr && isspace(static_cast<unsigned char>(*pszStr)))
                pszStr++;
            if (*pszStr == ':')
            {
                pszStr++;
                while (*pszStr && isspace(static_cast<unsigned char>(*pszStr)))
                    pszStr++;
                if (*pszStr == '[')
                {
                    bUseStreamingInterface = true;
                }
            }
        }
    }

    if (bUseStreamingInterface)
    {
        bool bTryStandardReading = false;
        if (poReader->FirstPassReadLayer(this, fp ? fp : poOpenInfo->fpL,
                                         bTryStandardReading))
        {
            if (fp)
                fp = nullptr;
            else
                poOpenInfo->fpL = nullptr;
            CheckExceededTransferLimit(poReader->GetJSonObject());
        }
        else
        {
            delete poReader;
        }
        if (!bTryStandardReading)
        {
            if (fp)
                VSIFCloseL(fp);
            return;
        }

        poReader = new OGRGeoJSONReader();
        SetOptionsOnReader(poOpenInfo, poReader);
    }

    if (fp)
        VSIFCloseL(fp);
    if (nSrcType == eGeoJSONSourceFile)
    {
        if (!ReadFromFile(poOpenInfo, pszUnprefixed))
        {
            delete poReader;
            return;
        }
        RemoveJSonPStuff();
    }
    const OGRErr err = poReader->Parse(pszGeoData_);
    if (OGRERR_NONE == err)
    {
        CheckExceededTransferLimit(poReader->GetJSonObject());
    }

    poReader->ReadLayers(this);
    delete poReader;
}

/************************************************************************/
/*                          SetOptionsOnReader()                        */
/************************************************************************/

void OGRGeoJSONDataSource::SetOptionsOnReader(GDALOpenInfo *poOpenInfo,
                                              OGRGeoJSONReader *poReader)
{
    if (eGeometryAsCollection == flTransGeom_)
    {
        poReader->SetPreserveGeometryType(false);
        CPLDebug("GeoJSON", "Geometry as OGRGeometryCollection type.");
    }

    if (eAttributesSkip == flTransAttrs_)
    {
        poReader->SetSkipAttributes(true);
        CPLDebug("GeoJSON", "Skip all attributes.");
    }

    poReader->SetFlattenNestedAttributes(
        CPLFetchBool(poOpenInfo->papszOpenOptions, "FLATTEN_NESTED_ATTRIBUTES",
                     false),
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                             "NESTED_ATTRIBUTE_SEPARATOR", "_")[0]);

    const bool bDefaultNativeData = bUpdatable_;
    poReader->SetStoreNativeData(CPLFetchBool(
        poOpenInfo->papszOpenOptions, "NATIVE_DATA", bDefaultNativeData));

    poReader->SetArrayAsString(CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "ARRAY_AS_STRING",
        CPLGetConfigOption("OGR_GEOJSON_ARRAY_AS_STRING", "NO"))));

    poReader->SetDateAsString(CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "DATE_AS_STRING",
        CPLGetConfigOption("OGR_GEOJSON_DATE_AS_STRING", "NO"))));

    const char *pszForeignMembers = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "FOREIGN_MEMBERS", "AUTO");
    if (EQUAL(pszForeignMembers, "AUTO"))
    {
        poReader->SetForeignMemberProcessing(
            OGRGeoJSONBaseReader::ForeignMemberProcessing::AUTO);
    }
    else if (EQUAL(pszForeignMembers, "ALL"))
    {
        poReader->SetForeignMemberProcessing(
            OGRGeoJSONBaseReader::ForeignMemberProcessing::ALL);
    }
    else if (EQUAL(pszForeignMembers, "NONE"))
    {
        poReader->SetForeignMemberProcessing(
            OGRGeoJSONBaseReader::ForeignMemberProcessing::NONE);
    }
    else if (EQUAL(pszForeignMembers, "STAC"))
    {
        poReader->SetForeignMemberProcessing(
            OGRGeoJSONBaseReader::ForeignMemberProcessing::STAC);
    }
}

/************************************************************************/
/*                     CheckExceededTransferLimit()                     */
/************************************************************************/

void OGRGeoJSONDataSource::CheckExceededTransferLimit(json_object *poObj)
{
    for (int i = 0; i < 2; i++)
    {
        if (i == 1)
        {
            if (poObj && json_object_get_type(poObj) == json_type_object)
            {
                poObj = CPL_json_object_object_get(poObj, "properties");
            }
        }
        if (poObj && json_object_get_type(poObj) == json_type_object)
        {
            json_object *poExceededTransferLimit =
                CPL_json_object_object_get(poObj, "exceededTransferLimit");
            if (poExceededTransferLimit &&
                json_object_get_type(poExceededTransferLimit) ==
                    json_type_boolean)
            {
                bOtherPages_ = CPL_TO_BOOL(
                    json_object_get_boolean(poExceededTransferLimit));
                return;
            }
        }
    }
}

/************************************************************************/
/*                            AddLayer()                                */
/************************************************************************/

void OGRGeoJSONDataSource::AddLayer(OGRGeoJSONLayer *poLayer)
{
    CPLAssert(papoLayersWriter_ == nullptr);

    // Return layer in readable state.
    poLayer->ResetReading();

    papoLayers_ = static_cast<OGRGeoJSONLayer **>(
        CPLRealloc(papoLayers_, sizeof(OGRGeoJSONLayer *) * (nLayers_ + 1)));
    papoLayers_[nLayers_] = poLayer;
    nLayers_++;
}

/************************************************************************/
/*                            FlushCache()                              */
/************************************************************************/

CPLErr OGRGeoJSONDataSource::FlushCache(bool /*bAtClosing*/)
{
    if (papoLayersWriter_ != nullptr)
    {
        return papoLayersWriter_[0]->SyncToDisk() == OGRERR_NONE ? CE_None
                                                                 : CE_Failure;
    }

    CPLErr eErr = CE_None;
    for (int i = 0; i < nLayers_; i++)
    {
        if (papoLayers_[i]->HasBeenUpdated())
        {
            papoLayers_[i]->SetUpdated(false);

            bool bOK = false;

            // Disable all filters.
            OGRFeatureQuery *poAttrQueryBak = papoLayers_[i]->m_poAttrQuery;
            papoLayers_[i]->m_poAttrQuery = nullptr;
            OGRGeometry *poFilterGeomBak = papoLayers_[i]->m_poFilterGeom;
            papoLayers_[i]->m_poFilterGeom = nullptr;

            // If the source data only contained one single feature and
            // that's still the case, then do not use a FeatureCollection
            // on writing.
            bool bAlreadyDone = false;
            if (papoLayers_[i]->GetFeatureCount(TRUE) == 1 &&
                papoLayers_[i]->GetMetadata("NATIVE_DATA") == nullptr)
            {
                papoLayers_[i]->ResetReading();
                OGRFeature *poFeature = papoLayers_[i]->GetNextFeature();
                if (poFeature != nullptr)
                {
                    if (poFeature->GetNativeData() != nullptr)
                    {
                        bAlreadyDone = true;
                        OGRGeoJSONWriteOptions oOptions;
                        json_object *poObj =
                            OGRGeoJSONWriteFeature(poFeature, oOptions);
                        VSILFILE *fp = VSIFOpenL(pszName_, "wb");
                        if (fp != nullptr)
                        {
                            bOK = VSIFPrintfL(
                                      fp, "%s",
                                      json_object_to_json_string(poObj)) > 0;
                            VSIFCloseL(fp);
                        }
                        json_object_put(poObj);
                    }
                    delete poFeature;
                }
            }

            // Otherwise do layer translation.
            if (!bAlreadyDone)
            {
                char **papszOptions = CSLAddString(nullptr, "-f");
                papszOptions = CSLAddString(papszOptions, "GeoJSON");
                GDALVectorTranslateOptions *psOptions =
                    GDALVectorTranslateOptionsNew(papszOptions, nullptr);
                CSLDestroy(papszOptions);
                GDALDatasetH hSrcDS = this;
                CPLString osNewFilename(pszName_);
                osNewFilename += ".tmp";
                GDALDatasetH hOutDS = GDALVectorTranslate(
                    osNewFilename, nullptr, 1, &hSrcDS, psOptions, nullptr);
                GDALVectorTranslateOptionsFree(psOptions);

                if (hOutDS != nullptr)
                {
                    CPLErrorReset();
                    GDALClose(hOutDS);
                    bOK = (CPLGetLastErrorType() == CE_None);
                }
                if (bOK)
                {
                    const bool bOverwrite = CPLTestBool(
                        CPLGetConfigOption("OGR_GEOJSON_REWRITE_IN_PLACE",
#ifdef _WIN32
                                           "YES"
#else
                                           "NO"
#endif
                                           ));
                    if (bOverwrite)
                    {
                        VSILFILE *fpTarget = nullptr;
                        for (int attempt = 0; attempt < 10; attempt++)
                        {
                            fpTarget = VSIFOpenL(pszName_, "rb+");
                            if (fpTarget)
                                break;
                            CPLSleep(0.1);
                        }
                        if (!fpTarget)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Cannot rewrite %s", pszName_);
                        }
                        else
                        {
                            bool bCopyOK = CPL_TO_BOOL(
                                VSIOverwriteFile(fpTarget, osNewFilename));
                            if (VSIFCloseL(fpTarget) != 0)
                                bCopyOK = false;
                            if (bCopyOK)
                            {
                                VSIUnlink(osNewFilename);
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Cannot rewrite %s with content of %s",
                                         pszName_, osNewFilename.c_str());
                            }
                        }
                    }
                    else
                    {
                        CPLString osBackup(pszName_);
                        osBackup += ".bak";
                        if (VSIRename(pszName_, osBackup) < 0)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Cannot create backup copy");
                        }
                        else if (VSIRename(osNewFilename, pszName_) < 0)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Cannot rename %s to %s",
                                     osNewFilename.c_str(), pszName_);
                        }
                        else
                        {
                            VSIUnlink(osBackup);
                        }
                    }
                }
            }
            if (!bOK)
                eErr = CE_Failure;

            // Restore filters.
            papoLayers_[i]->m_poAttrQuery = poAttrQueryBak;
            papoLayers_[i]->m_poFilterGeom = poFilterGeomBak;
        }
    }
    return eErr;
}
