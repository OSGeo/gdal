/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRESRIJSONReader class (OGR ESRIJSON Driver)
 *           to read ESRI Feature Service REST data
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2013, Kyle Shannon <kyle at pobox dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogrlibjsonutils.h"

#include <limits.h>
#include <stddef.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_time.h"
#include "json.h"
// #include "json_object.h"
// #include "json_tokener.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogr_geojson.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogresrijsongeometry.h"

#include <map>
#include <utility>

// #include "symbol_renames.h"

/************************************************************************/
/*                          OGRESRIJSONReader()                         */
/************************************************************************/

OGRESRIJSONReader::OGRESRIJSONReader() : poGJObject_(nullptr), poLayer_(nullptr)
{
}

/************************************************************************/
/*                         ~OGRESRIJSONReader()                         */
/************************************************************************/

OGRESRIJSONReader::~OGRESRIJSONReader()
{
    if (nullptr != poGJObject_)
    {
        json_object_put(poGJObject_);
    }

    poGJObject_ = nullptr;
    poLayer_ = nullptr;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

OGRErr OGRESRIJSONReader::Parse(const char *pszText)
{
    json_object *jsobj = nullptr;
    if (nullptr != pszText && !OGRJSonParse(pszText, &jsobj, true))
    {
        return OGRERR_CORRUPT_DATA;
    }

    // JSON tree is shared for while lifetime of the reader object
    // and will be released in the destructor.
    poGJObject_ = jsobj;
    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReadLayers()                               */
/************************************************************************/

void OGRESRIJSONReader::ReadLayers(OGRGeoJSONDataSource *poDS,
                                   GeoJSONSourceType eSourceType)
{
    CPLAssert(nullptr == poLayer_);

    poDS->SetSupportsMGeometries(true);

    if (nullptr == poGJObject_)
    {
        CPLDebug("ESRIJSON",
                 "Missing parsed ESRIJSON data. Forgot to call Parse()?");
        return;
    }

    OGRSpatialReference *poSRS = OGRESRIJSONReadSpatialReference(poGJObject_);

    std::string osName = "ESRIJSON";
    if (eSourceType == eGeoJSONSourceFile)
    {
        osName = poDS->GetDescription();
        if (STARTS_WITH_CI(osName.c_str(), "ESRIJSON:"))
            osName = osName.substr(strlen("ESRIJSON:"));
        osName = CPLGetBasenameSafe(osName.c_str());
    }

    auto eGeomType = OGRESRIJSONGetGeometryType(poGJObject_);
    if (eGeomType == wkbNone)
    {
        if (poSRS)
        {
            eGeomType = wkbUnknown;
        }
        else
        {
            json_object *poObjFeatures =
                OGRGeoJSONFindMemberByName(poGJObject_, "features");
            if (poObjFeatures &&
                json_type_array == json_object_get_type(poObjFeatures))
            {
                const auto nFeatures = json_object_array_length(poObjFeatures);
                for (auto i = decltype(nFeatures){0}; i < nFeatures; ++i)
                {
                    json_object *poObjFeature =
                        json_object_array_get_idx(poObjFeatures, i);
                    if (poObjFeature != nullptr &&
                        json_object_get_type(poObjFeature) == json_type_object)
                    {
                        if (auto poObjGeometry = OGRGeoJSONFindMemberByName(
                                poObjFeature, "geometry"))
                        {
                            eGeomType = wkbUnknown;
                            poSRS =
                                OGRESRIJSONReadSpatialReference(poObjGeometry);
                            break;
                        }
                    }
                }
            }
        }
    }

    poLayer_ =
        new OGRGeoJSONLayer(osName.c_str(), poSRS, eGeomType, poDS, nullptr);
    poLayer_->SetSupportsMGeometries(true);
    if (poSRS != nullptr)
        poSRS->Release();

    if (!GenerateLayerDefn())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer schema generation failed.");

        delete poLayer_;
        return;
    }

    OGRGeoJSONLayer *poThisLayer = ReadFeatureCollection(poGJObject_);
    if (poThisLayer == nullptr)
    {
        delete poLayer_;
        return;
    }

    CPLErrorReset();

    poLayer_->DetectGeometryType();
    poDS->AddLayer(poLayer_);
}

/************************************************************************/
/*                        GenerateFeatureDefn()                         */
/************************************************************************/

bool OGRESRIJSONReader::GenerateLayerDefn()
{
    CPLAssert(nullptr != poGJObject_);

    bool bSuccess = true;

    OGRFeatureDefn *poDefn = poLayer_->GetLayerDefn();
    CPLAssert(nullptr != poDefn);
    CPLAssert(0 == poDefn->GetFieldCount());
    auto oTemporaryUnsealer(poDefn->GetTemporaryUnsealer());

    /* -------------------------------------------------------------------- */
    /*      Scan all features and generate layer definition.                */
    /* -------------------------------------------------------------------- */
    json_object *poFields = OGRGeoJSONFindMemberByName(poGJObject_, "fields");
    if (nullptr != poFields &&
        json_type_array == json_object_get_type(poFields))
    {
        const auto nFeatures = json_object_array_length(poFields);
        for (auto i = decltype(nFeatures){0}; i < nFeatures; ++i)
        {
            json_object *poField = json_object_array_get_idx(poFields, i);
            if (!ParseField(poField))
            {
                CPLDebug("GeoJSON", "Create feature schema failure.");
                bSuccess = false;
            }
        }
    }
    else if ((poFields = OGRGeoJSONFindMemberByName(
                  poGJObject_, "fieldAliases")) != nullptr &&
             json_object_get_type(poFields) == json_type_object)
    {
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poFields, it)
        {
            OGRFieldDefn fldDefn(it.key, OFTString);
            poDefn->AddFieldDefn(&fldDefn);
        }
    }
    else
    {
        // Guess the fields' schema from the content of the features' "attributes"
        // element
        json_object *poObjFeatures =
            OGRGeoJSONFindMemberByName(poGJObject_, "features");
        if (poObjFeatures &&
            json_type_array == json_object_get_type(poObjFeatures))
        {
            gdal::DirectedAcyclicGraph<int, std::string> dag;
            std::vector<std::unique_ptr<OGRFieldDefn>> apoFieldDefn{};
            std::map<std::string, int> oMapFieldNameToIdx{};
            std::vector<int> anCurFieldIndices;
            std::set<int> aoSetUndeterminedTypeFields;

            const auto nFeatures = json_object_array_length(poObjFeatures);
            for (auto i = decltype(nFeatures){0}; i < nFeatures; ++i)
            {
                json_object *poObjFeature =
                    json_object_array_get_idx(poObjFeatures, i);
                if (poObjFeature != nullptr &&
                    json_object_get_type(poObjFeature) == json_type_object)
                {
                    int nPrevFieldIdx = -1;

                    json_object *poObjProps =
                        OGRGeoJSONFindMemberByName(poObjFeature, "attributes");
                    if (nullptr != poObjProps &&
                        json_object_get_type(poObjProps) == json_type_object)
                    {
                        json_object_iter it;
                        it.key = nullptr;
                        it.val = nullptr;
                        it.entry = nullptr;
                        json_object_object_foreachC(poObjProps, it)
                        {
                            anCurFieldIndices.clear();
                            OGRGeoJSONReaderAddOrUpdateField(
                                anCurFieldIndices, oMapFieldNameToIdx,
                                apoFieldDefn, it.key, it.val,
                                /*bFlattenNestedAttributes = */ true,
                                /* chNestedAttributeSeparator = */ '.',
                                /* bArrayAsString =*/false,
                                /* bDateAsString = */ false,
                                aoSetUndeterminedTypeFields);
                            for (int idx : anCurFieldIndices)
                            {
                                dag.addNode(idx,
                                            apoFieldDefn[idx]->GetNameRef());
                                if (nPrevFieldIdx != -1)
                                {
                                    dag.addEdge(nPrevFieldIdx, idx);
                                }
                                nPrevFieldIdx = idx;
                            }
                        }
                    }
                }
            }

            const auto sortedFields = dag.getTopologicalOrdering();
            CPLAssert(sortedFields.size() == apoFieldDefn.size());
            for (int idx : sortedFields)
            {
                // cppcheck-suppress containerOutOfBounds
                poDefn->AddFieldDefn(apoFieldDefn[idx].get());
            }
        }
    }

    return bSuccess;
}

/************************************************************************/
/*                             ParseField()                             */
/************************************************************************/

static const std::map<std::string, std::pair<OGRFieldType, OGRFieldSubType>>
    goMapEsriTypeToOGR = {
        {"esriFieldTypeString", {OFTString, OFSTNone}},
        {"esriFieldTypeSingle", {OFTReal, OFSTFloat32}},
        {"esriFieldTypeDouble", {OFTReal, OFSTNone}},
        {"esriFieldTypeSmallInteger", {OFTInteger, OFSTInt16}},
        {"esriFieldTypeInteger", {OFTInteger, OFSTNone}},
        {"esriFieldTypeDate", {OFTDateTime, OFSTNone}},
        {"esriFieldTypeDateOnly", {OFTDate, OFSTNone}},
        {"esriFieldTypeTimeOnly", {OFTTime, OFSTNone}},
        {"esriFieldTypeBigInteger", {OFTInteger64, OFSTNone}},
        {"esriFieldTypeGUID", {OFTString, OFSTUUID}},
        {"esriFieldTypeGlobalID", {OFTString, OFSTUUID}},
};

bool OGRESRIJSONReader::ParseField(json_object *poObj)
{
    OGRFeatureDefn *poDefn = poLayer_->GetLayerDefn();
    CPLAssert(nullptr != poDefn);

    bool bSuccess = false;

    /* -------------------------------------------------------------------- */
    /*      Read collection of properties.                                  */
    /* -------------------------------------------------------------------- */
    json_object *poObjName = OGRGeoJSONFindMemberByName(poObj, "name");
    json_object *poObjType = OGRGeoJSONFindMemberByName(poObj, "type");
    if (nullptr != poObjName && nullptr != poObjType)
    {
        OGRFieldType eFieldType = OFTString;
        OGRFieldSubType eFieldSubType = OFSTNone;
        const char *pszObjName = json_object_get_string(poObjName);
        const char *pszObjType = json_object_get_string(poObjType);
        if (strcmp(pszObjType, "esriFieldTypeOID") == 0)
        {
            eFieldType = OFTInteger;
            poLayer_->SetFIDColumn(pszObjName);
        }
        else
        {
            const auto it = goMapEsriTypeToOGR.find(pszObjType);
            if (it != goMapEsriTypeToOGR.end())
            {
                eFieldType = it->second.first;
                eFieldSubType = it->second.second;
            }
            else
            {
                CPLDebug("ESRIJSON",
                         "Unhandled fields[\"%s\"].type = %s. "
                         "Processing it as a String",
                         pszObjName, pszObjType);
            }
        }
        OGRFieldDefn fldDefn(pszObjName, eFieldType);
        fldDefn.SetSubType(eFieldSubType);

        if (eFieldType != OFTDateTime)
        {
            json_object *const poObjLength =
                OGRGeoJSONFindMemberByName(poObj, "length");
            if (poObjLength != nullptr &&
                json_object_get_type(poObjLength) == json_type_int)
            {
                const int nWidth = json_object_get_int(poObjLength);
                // A dummy width of 2147483647 seems to indicate no known field with
                // which in the OGR world is better modelled as 0 field width.
                // (#6529)
                if (nWidth != INT_MAX)
                    fldDefn.SetWidth(nWidth);
            }
        }

        json_object *poObjAlias = OGRGeoJSONFindMemberByName(poObj, "alias");
        if (poObjAlias && json_object_get_type(poObjAlias) == json_type_string)
        {
            const char *pszAlias = json_object_get_string(poObjAlias);
            if (strcmp(pszObjName, pszAlias) != 0)
                fldDefn.SetAlternativeName(pszAlias);
        }

        poDefn->AddFieldDefn(&fldDefn);

        bSuccess = true;
    }
    return bSuccess;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRESRIJSONReader::AddFeature(OGRFeature *poFeature)
{
    if (nullptr == poFeature)
        return false;

    poLayer_->AddFeature(poFeature);
    delete poFeature;

    return true;
}

/************************************************************************/
/*                           EsriDateToOGRDate()                        */
/************************************************************************/

static void EsriDateToOGRDate(int64_t nVal, OGRField *psField)
{
    const auto nSeconds = nVal / 1000;
    const auto nMillisec = static_cast<int>(nVal % 1000);

    struct tm brokendowntime;
    CPLUnixTimeToYMDHMS(nSeconds, &brokendowntime);

    psField->Date.Year = static_cast<GInt16>(brokendowntime.tm_year + 1900);
    psField->Date.Month = static_cast<GByte>(brokendowntime.tm_mon + 1);
    psField->Date.Day = static_cast<GByte>(brokendowntime.tm_mday);
    psField->Date.Hour = static_cast<GByte>(brokendowntime.tm_hour);
    psField->Date.Minute = static_cast<GByte>(brokendowntime.tm_min);
    psField->Date.Second =
        static_cast<float>(brokendowntime.tm_sec + nMillisec / 1000.0);
    psField->Date.TZFlag = 100;
    psField->Date.Reserved = 0;
}

/************************************************************************/
/*                           ReadFeature()                              */
/************************************************************************/

OGRFeature *OGRESRIJSONReader::ReadFeature(json_object *poObj)
{
    CPLAssert(nullptr != poObj);
    CPLAssert(nullptr != poLayer_);

    OGRFeature *poFeature = new OGRFeature(poLayer_->GetLayerDefn());

    /* -------------------------------------------------------------------- */
    /*      Translate ESRIJSON "attributes" object to feature attributes.   */
    /* -------------------------------------------------------------------- */
    CPLAssert(nullptr != poFeature);

    json_object *poObjProps = OGRGeoJSONFindMemberByName(poObj, "attributes");
    if (nullptr != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object)
    {
        OGRFieldDefn *poFieldDefn = nullptr;
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poObjProps, it)
        {
            const int nField = poFeature->GetFieldIndex(it.key);
            if (nField >= 0)
            {
                poFieldDefn = poFeature->GetFieldDefnRef(nField);
                if (poFieldDefn && it.val != nullptr)
                {
                    if (EQUAL(it.key, poLayer_->GetFIDColumn()))
                        poFeature->SetFID(json_object_get_int(it.val));
                    switch (poLayer_->GetLayerDefn()
                                ->GetFieldDefn(nField)
                                ->GetType())
                    {
                        case OFTInteger:
                        {
                            poFeature->SetField(nField,
                                                json_object_get_int(it.val));
                            break;
                        }
                        case OFTReal:
                        {
                            poFeature->SetField(nField,
                                                json_object_get_double(it.val));
                            break;
                        }
                        case OFTDateTime:
                        {
                            const auto nVal = json_object_get_int64(it.val);
                            EsriDateToOGRDate(
                                nVal, poFeature->GetRawFieldRef(nField));
                            break;
                        }
                        default:
                        {
                            poFeature->SetField(nField,
                                                json_object_get_string(it.val));
                            break;
                        }
                    }
                }
            }
        }
    }

    const OGRwkbGeometryType eType = poLayer_->GetGeomType();
    if (eType == wkbNone)
        return poFeature;

    /* -------------------------------------------------------------------- */
    /*      Translate geometry sub-object of ESRIJSON Feature.               */
    /* -------------------------------------------------------------------- */
    json_object *poObjGeom = nullptr;
    json_object *poTmp = poObj;
    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    json_object_object_foreachC(poTmp, it)
    {
        if (EQUAL(it.key, "geometry"))
        {
            if (it.val != nullptr)
                poObjGeom = it.val;
            // We're done.  They had 'geometry':null.
            else
                return poFeature;
        }
    }

    if (nullptr != poObjGeom)
    {
        OGRGeometry *poGeometry = OGRESRIJSONReadGeometry(poObjGeom);
        if (nullptr != poGeometry)
        {
            poFeature->SetGeometryDirectly(poGeometry);
        }
    }

    return poFeature;
}

/************************************************************************/
/*                           ReadFeatureCollection()                    */
/************************************************************************/

OGRGeoJSONLayer *OGRESRIJSONReader::ReadFeatureCollection(json_object *poObj)
{
    CPLAssert(nullptr != poLayer_);

    json_object *poObjFeatures = OGRGeoJSONFindMemberByName(poObj, "features");
    if (nullptr == poObjFeatures)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid FeatureCollection object. "
                 "Missing \'features\' member.");
        return nullptr;
    }

    if (json_type_array == json_object_get_type(poObjFeatures))
    {
        const auto nFeatures = json_object_array_length(poObjFeatures);
        for (auto i = decltype(nFeatures){0}; i < nFeatures; ++i)
        {
            json_object *poObjFeature =
                json_object_array_get_idx(poObjFeatures, i);
            if (poObjFeature != nullptr &&
                json_object_get_type(poObjFeature) == json_type_object)
            {
                OGRFeature *poFeature =
                    OGRESRIJSONReader::ReadFeature(poObjFeature);
                AddFeature(poFeature);
            }
        }
    }

    // We're returning class member to follow the same pattern of
    // Read* functions call convention.
    CPLAssert(nullptr != poLayer_);
    return poLayer_;
}
