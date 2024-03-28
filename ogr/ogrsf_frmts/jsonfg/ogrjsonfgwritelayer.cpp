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
#include "cpl_time.h"
#include "ogrgeojsonreader.h"  // OGRJSonParse()

#include <algorithm>

/************************************************************************/
/*                         OGRJSONFGWriteLayer()                        */
/************************************************************************/

OGRJSONFGWriteLayer::OGRJSONFGWriteLayer(
    const char *pszName, const OGRSpatialReference *poSRS,
    std::unique_ptr<OGRCoordinateTransformation> &&poCTToWGS84,
    const std::string &osCoordRefSys, OGRwkbGeometryType eGType,
    CSLConstList papszOptions, OGRJSONFGDataset *poDS)
    : poDS_(poDS), poFeatureDefn_(new OGRFeatureDefn(pszName)),
      poCTToWGS84_(std::move(poCTToWGS84)), osCoordRefSys_(osCoordRefSys)
{
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType(eGType);
    if (eGType != wkbNone && poSRS)
    {
        auto poSRSClone = poSRS->Clone();
        poFeatureDefn_->GetGeomFieldDefn(0)->SetSpatialRef(poSRSClone);
        poSRSClone->Release();
        m_bMustSwapForPlace = OGRJSONFGMustSwapXY(poSRS);
    }
    SetDescription(poFeatureDefn_->GetName());

    bIsWGS84CRS_ = osCoordRefSys_.find("[OGC:CRS84]") != std::string::npos ||
                   osCoordRefSys_.find("[OGC:CRS84h]") != std::string::npos ||
                   osCoordRefSys_.find("[EPSG:4326]") != std::string::npos ||
                   osCoordRefSys_.find("[EPSG:4979]") != std::string::npos;

    oWriteOptions_.nXYCoordPrecision = atoi(CSLFetchNameValueDef(
        papszOptions, "XY_COORD_PRECISION_GEOMETRY", "-1"));
    oWriteOptions_.nZCoordPrecision = atoi(
        CSLFetchNameValueDef(papszOptions, "Z_COORD_PRECISION_GEOMETRY", "-1"));
    oWriteOptions_.nSignificantFigures =
        atoi(CSLFetchNameValueDef(papszOptions, "SIGNIFICANT_FIGURES", "-1"));
    oWriteOptions_.SetRFC7946Settings();
    oWriteOptions_.SetIDOptions(papszOptions);

    oWriteOptionsPlace_.nXYCoordPrecision = atoi(
        CSLFetchNameValueDef(papszOptions, "XY_COORD_PRECISION_PLACE", "-1"));
    oWriteOptionsPlace_.nZCoordPrecision = atoi(
        CSLFetchNameValueDef(papszOptions, "Z_COORD_PRECISION_PLACE", "-1"));
    oWriteOptionsPlace_.nSignificantFigures =
        atoi(CSLFetchNameValueDef(papszOptions, "SIGNIFICANT_FIGURES", "-1"));

    bWriteFallbackGeometry_ = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_GEOMETRY", "TRUE"));

    VSILFILE *fp = poDS_->GetOutputFile();
    if (poDS_->IsSingleOutputLayer())
    {
        auto poFeatureType = json_object_new_string(pszName);
        VSIFPrintfL(fp, "\"featureType\" : %s,\n",
                    json_object_to_json_string_ext(poFeatureType,
                                                   JSON_C_TO_STRING_SPACED));
        json_object_put(poFeatureType);
        if (!osCoordRefSys.empty())
            VSIFPrintfL(fp, "\"coordRefSys\" : %s,\n", osCoordRefSys.c_str());
    }
}

/************************************************************************/
/*                        ~OGRJSONFGWriteLayer()                        */
/************************************************************************/

OGRJSONFGWriteLayer::~OGRJSONFGWriteLayer()
{
    poFeatureDefn_->Release();
}

/************************************************************************/
/*                           SyncToDisk()                               */
/************************************************************************/

OGRErr OGRJSONFGWriteLayer::SyncToDisk()
{
    return poDS_->SyncToDiskInternal();
}

/************************************************************************/
/*                       GetValueAsDateOrDateTime()                     */
/************************************************************************/

static const char *GetValueAsDateOrDateTime(const OGRField *psRawValue,
                                            OGRFieldType eType)
{
    if (eType == OFTDate)
    {
        return CPLSPrintf("%04d-%02d-%02d", psRawValue->Date.Year,
                          psRawValue->Date.Month, psRawValue->Date.Day);
    }
    else
    {
        struct tm brokenDown;
        memset(&brokenDown, 0, sizeof(brokenDown));
        brokenDown.tm_year = psRawValue->Date.Year - 1900;
        brokenDown.tm_mon = psRawValue->Date.Month - 1;
        brokenDown.tm_mday = psRawValue->Date.Day;
        brokenDown.tm_hour = psRawValue->Date.Hour;
        brokenDown.tm_min = psRawValue->Date.Minute;
        brokenDown.tm_sec = 0;
        if (psRawValue->Date.TZFlag > 0)
        {
            // Force to UTC
            GIntBig nVal = CPLYMDHMSToUnixTime(&brokenDown);
            nVal -= (psRawValue->Date.TZFlag - 100) * 15 * 60;
            CPLUnixTimeToYMDHMS(nVal, &brokenDown);
        }
        if (std::fabs(std::round(psRawValue->Date.Second) -
                      psRawValue->Date.Second) < 1e-3)
        {
            return CPLSPrintf(
                "%04d-%02d-%02dT%02d:%02d:%02dZ", brokenDown.tm_year + 1900,
                brokenDown.tm_mon + 1, brokenDown.tm_mday, brokenDown.tm_hour,
                brokenDown.tm_min,
                static_cast<int>(std::round(psRawValue->Date.Second)));
        }
        else
        {
            return CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%06.3fZ",
                              brokenDown.tm_year + 1900, brokenDown.tm_mon + 1,
                              brokenDown.tm_mday, brokenDown.tm_hour,
                              brokenDown.tm_min, psRawValue->Date.Second);
        }
    }
}

/************************************************************************/
/*                     OGRJSONFGWriteGeometry()                         */
/************************************************************************/

static json_object *
OGRJSONFGWriteGeometry(const OGRGeometry *poGeometry,
                       const OGRGeoJSONWriteOptions &oOptions)
{
    if (wkbFlatten(poGeometry->getGeometryType()) == wkbPolyhedralSurface)
    {
        const auto poPS = poGeometry->toPolyhedralSurface();
        json_object *poObj = json_object_new_object();
        json_object_object_add(poObj, "type",
                               json_object_new_string("Polyhedron"));
        json_object *poCoordinates = json_object_new_array();
        json_object_object_add(poObj, "coordinates", poCoordinates);
        json_object *poOuterShell = json_object_new_array();
        json_object_array_add(poCoordinates, poOuterShell);
        for (const auto *poPoly : *poPS)
        {
            json_object_array_add(poOuterShell,
                                  OGRGeoJSONWritePolygon(poPoly, oOptions));
        }
        return poObj;
    }
    else
    {
        return nullptr;
    }
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRJSONFGWriteLayer::ICreateFeature(OGRFeature *poFeature)
{
    VSILFILE *fp = poDS_->GetOutputFile();
    poDS_->BeforeCreateFeature();

    if (oWriteOptions_.bGenerateID && poFeature->GetFID() == OGRNullFID)
    {
        poFeature->SetFID(nOutCounter_);
    }

    json_object *poObj = json_object_new_object();

    json_object_object_add(poObj, "type", json_object_new_string("Feature"));

    /* -------------------------------------------------------------------- */
    /*      Write FID if available                                          */
    /* -------------------------------------------------------------------- */
    OGRGeoJSONWriteId(poFeature, poObj, /* bIdAlreadyWritten = */ false,
                      oWriteOptions_);

    if (!poDS_->IsSingleOutputLayer())
    {
        json_object_object_add(poObj, "featureType",
                               json_object_new_string(GetDescription()));
        if (!osCoordRefSys_.empty() && !bIsWGS84CRS_)
        {
            json_object *poCoordRefSys = nullptr;
            CPL_IGNORE_RET_VAL(
                OGRJSonParse(osCoordRefSys_.c_str(), &poCoordRefSys));
            json_object_object_add(poObj, "coordRefSys", poCoordRefSys);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Write feature attributes to "properties" object.                */
    /* -------------------------------------------------------------------- */
    json_object *poObjProps = OGRGeoJSONWriteAttributes(
        poFeature, /* bWriteIdIfFoundInAttributes = */ true, oWriteOptions_);

    /* -------------------------------------------------------------------- */
    /*      Deal with time properties.                                      */
    /* -------------------------------------------------------------------- */
    json_object *poTime = nullptr;
    int nFieldTimeIdx = poFeatureDefn_->GetFieldIndex("jsonfg_time");
    if (nFieldTimeIdx < 0)
        nFieldTimeIdx = poFeatureDefn_->GetFieldIndex("time");
    if (nFieldTimeIdx >= 0 && poFeature->IsFieldSetAndNotNull(nFieldTimeIdx))
    {
        const auto poFieldDefn = poFeatureDefn_->GetFieldDefn(nFieldTimeIdx);
        const auto eType = poFieldDefn->GetType();
        if (eType == OFTDate || eType == OFTDateTime)
        {
            json_object_object_del(poObjProps, poFieldDefn->GetNameRef());
            poTime = json_object_new_object();
            json_object_object_add(
                poTime, eType == OFTDate ? "date" : "timestamp",
                json_object_new_string(GetValueAsDateOrDateTime(
                    poFeature->GetRawFieldRef(nFieldTimeIdx), eType)));
        }
    }
    else
    {
        bool bHasStartOrStop = false;
        json_object *poTimeStart = nullptr;
        int nFieldTimeStartIdx =
            poFeatureDefn_->GetFieldIndex("jsonfg_time_start");
        if (nFieldTimeStartIdx < 0)
            nFieldTimeStartIdx = poFeatureDefn_->GetFieldIndex("time_start");
        if (nFieldTimeStartIdx >= 0 &&
            poFeature->IsFieldSetAndNotNull(nFieldTimeStartIdx))
        {
            const auto poFieldDefnStart =
                poFeatureDefn_->GetFieldDefn(nFieldTimeStartIdx);
            const auto eType = poFieldDefnStart->GetType();
            if (eType == OFTDate || eType == OFTDateTime)
            {
                json_object_object_del(poObjProps,
                                       poFieldDefnStart->GetNameRef());
                poTimeStart = json_object_new_string(GetValueAsDateOrDateTime(
                    poFeature->GetRawFieldRef(nFieldTimeStartIdx), eType));
                bHasStartOrStop = true;
            }
        }

        json_object *poTimeEnd = nullptr;
        int nFieldTimeEndIdx = poFeatureDefn_->GetFieldIndex("jsonfg_time_end");
        if (nFieldTimeEndIdx < 0)
            nFieldTimeEndIdx = poFeatureDefn_->GetFieldIndex("time_end");
        if (nFieldTimeEndIdx >= 0 &&
            poFeature->IsFieldSetAndNotNull(nFieldTimeEndIdx))
        {
            const auto poFieldDefnEnd =
                poFeatureDefn_->GetFieldDefn(nFieldTimeEndIdx);
            const auto eType = poFieldDefnEnd->GetType();
            if (eType == OFTDate || eType == OFTDateTime)
            {
                json_object_object_del(poObjProps,
                                       poFieldDefnEnd->GetNameRef());
                poTimeEnd = json_object_new_string(GetValueAsDateOrDateTime(
                    poFeature->GetRawFieldRef(nFieldTimeEndIdx), eType));
                bHasStartOrStop = true;
            }
        }

        if (bHasStartOrStop)
        {
            poTime = json_object_new_object();
            json_object *poInterval = json_object_new_array();
            json_object_object_add(poTime, "interval", poInterval);
            json_object_array_add(poInterval,
                                  poTimeStart ? poTimeStart
                                              : json_object_new_string(".."));
            json_object_array_add(poInterval,
                                  poTimeEnd ? poTimeEnd
                                            : json_object_new_string(".."));
        }
    }

    json_object_object_add(poObj, "properties", poObjProps);

    /* -------------------------------------------------------------------- */
    /*      Write place and/or geometry                                     */
    /* -------------------------------------------------------------------- */
    const OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if (!poGeom)
    {
        json_object_object_add(poObj, "geometry", nullptr);
        json_object_object_add(poObj, "place", nullptr);
    }
    else
    {
        if (wkbFlatten(poGeom->getGeometryType()) == wkbPolyhedralSurface)
        {
            json_object_object_add(poObj, "geometry", nullptr);
            if (m_bMustSwapForPlace)
            {
                auto poGeomClone =
                    std::unique_ptr<OGRGeometry>(poGeom->clone());
                poGeomClone->swapXY();
                json_object_object_add(
                    poObj, "place",
                    OGRJSONFGWriteGeometry(poGeomClone.get(),
                                           oWriteOptionsPlace_));
            }
            else
            {
                json_object_object_add(
                    poObj, "place",
                    OGRJSONFGWriteGeometry(poGeom, oWriteOptionsPlace_));
            }
        }
        else if (bIsWGS84CRS_)
        {
            json_object_object_add(
                poObj, "geometry",
                OGRGeoJSONWriteGeometry(poGeom, oWriteOptions_));
            json_object_object_add(poObj, "place", nullptr);
        }
        else
        {
            if (bWriteFallbackGeometry_ && poCTToWGS84_)
            {
                auto poGeomClone =
                    std::unique_ptr<OGRGeometry>(poGeom->clone());
                if (poGeomClone->transform(poCTToWGS84_.get()) == OGRERR_NONE)
                {
                    json_object_object_add(
                        poObj, "geometry",
                        OGRGeoJSONWriteGeometry(poGeomClone.get(),
                                                oWriteOptions_));
                }
                else
                {
                    json_object_object_add(poObj, "geometry", nullptr);
                }
            }
            else
            {
                json_object_object_add(poObj, "geometry", nullptr);
            }

            if (m_bMustSwapForPlace)
            {
                auto poGeomClone =
                    std::unique_ptr<OGRGeometry>(poGeom->clone());
                poGeomClone->swapXY();
                json_object_object_add(
                    poObj, "place",
                    OGRGeoJSONWriteGeometry(poGeomClone.get(),
                                            oWriteOptionsPlace_));
            }
            else
            {
                json_object_object_add(
                    poObj, "place",
                    OGRGeoJSONWriteGeometry(poGeom, oWriteOptionsPlace_));
            }
        }
    }

    json_object_object_add(poObj, "time", poTime);

    VSIFPrintfL(fp, "%s",
                json_object_to_json_string_ext(
                    poObj, JSON_C_TO_STRING_SPACED
#ifdef JSON_C_TO_STRING_NOSLASHESCAPE
                               | JSON_C_TO_STRING_NOSLASHESCAPE
#endif
                    ));

    json_object_put(poObj);

    ++nOutCounter_;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/

OGRErr OGRJSONFGWriteLayer::CreateField(const OGRFieldDefn *poField,
                                        int /* bApproxOK */)
{
    if (poFeatureDefn_->GetFieldIndexCaseSensitive(poField->GetNameRef()) >= 0)
    {
        CPLDebug("JSONFG", "Field '%s' already present in schema",
                 poField->GetNameRef());

        return OGRERR_NONE;
    }

    poFeatureDefn_->AddFieldDefn(poField);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRJSONFGWriteLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCCreateField))
        return TRUE;
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;
    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRJSONFGWriteLayer::GetDataset()
{
    return poDS_;
}
