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
#include "ogr_geojson.h"

#include "cpl_http.h"
#include "cpl_vsi_error.h"
#include "cpl_vsi_virtual.h"

#include <cmath>

/************************************************************************/
/*                  OGRJSONFGDataset::~OGRJSONFGDataset()               */
/************************************************************************/

OGRJSONFGDataset::~OGRJSONFGDataset()
{
    CPLFree(pszGeoData_);
    if (fpOut_)
    {
        FinishWriting();

        VSIFCloseL(fpOut_);
    }
}

/************************************************************************/
/*                           FinishWriting()                            */
/************************************************************************/

void OGRJSONFGDataset::FinishWriting()
{
    if (m_nPositionBeforeFCClosed == 0)
    {
        m_nPositionBeforeFCClosed = fpOut_->Tell();

        if (!EmitStartFeaturesIfNeededAndReturnIfFirstFeature())
            VSIFPrintfL(fpOut_, "\n");
        VSIFPrintfL(fpOut_, "]");

        // When we didn't know if there was a single layer, we omitted writing
        // the coordinate precision at ICreateLayer() time.
        // Now we can check if there was a single layer, or several layers with
        // same precision setting, and write it when possible.
        if (!bSingleOutputLayer_ && !apoLayers_.empty() &&
            apoLayers_.front()->GetLayerDefn()->GetGeomFieldCount() > 0)
        {
            const auto &oCoordPrec = apoLayers_.front()
                                         ->GetLayerDefn()
                                         ->GetGeomFieldDefn(0)
                                         ->GetCoordinatePrecision();
            bool bSameGeomCoordPrec =
                (oCoordPrec.dfXYResolution !=
                     OGRGeomCoordinatePrecision::UNKNOWN ||
                 oCoordPrec.dfZResolution !=
                     OGRGeomCoordinatePrecision::UNKNOWN);
            for (size_t i = 1; i < apoLayers_.size(); ++i)
            {
                if (apoLayers_[i]->GetLayerDefn()->GetGeomFieldCount() > 0)
                {
                    const auto &oOtherCoordPrec =
                        apoLayers_[i]
                            ->GetLayerDefn()
                            ->GetGeomFieldDefn(0)
                            ->GetCoordinatePrecision();
                    bSameGeomCoordPrec &= (oOtherCoordPrec.dfXYResolution ==
                                               oCoordPrec.dfXYResolution &&
                                           oOtherCoordPrec.dfZResolution ==
                                               oCoordPrec.dfZResolution);
                }
            }
            if (bSameGeomCoordPrec)
            {
                if (oCoordPrec.dfXYResolution !=
                    OGRGeomCoordinatePrecision::UNKNOWN)
                {
                    VSIFPrintfL(fpOut_,
                                ",\n\"xy_coordinate_resolution_place\":%g",
                                oCoordPrec.dfXYResolution);
                }
                if (oCoordPrec.dfZResolution !=
                    OGRGeomCoordinatePrecision::UNKNOWN)
                {
                    VSIFPrintfL(fpOut_,
                                ",\n\"z_coordinate_resolution_place\":%g",
                                oCoordPrec.dfZResolution);
                }

                OGRSpatialReference oSRSWGS84;
                oSRSWGS84.SetWellKnownGeogCS("WGS84");
                const auto oCoordPrecWGS84 = oCoordPrec.ConvertToOtherSRS(
                    apoLayers_.front()->GetSpatialRef(), &oSRSWGS84);

                if (oCoordPrecWGS84.dfXYResolution !=
                    OGRGeomCoordinatePrecision::UNKNOWN)
                {
                    VSIFPrintfL(fpOut_, ",\n\"xy_coordinate_resolution\":%g",
                                oCoordPrecWGS84.dfXYResolution);
                }
                if (oCoordPrecWGS84.dfZResolution !=
                    OGRGeomCoordinatePrecision::UNKNOWN)
                {
                    VSIFPrintfL(fpOut_, ",\n\"z_coordinate_resolution\":%g",
                                oCoordPrecWGS84.dfZResolution);
                }
            }
        }

        VSIFPrintfL(fpOut_, "\n}\n");
        fpOut_->Flush();
    }
}

/************************************************************************/
/*                         SyncToDiskInternal()                         */
/************************************************************************/

OGRErr OGRJSONFGDataset::SyncToDiskInternal()
{
    if (m_nPositionBeforeFCClosed == 0 && GetFpOutputIsSeekable())
    {
        FinishWriting();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                         BeforeCreateFeature()                        */
/************************************************************************/

void OGRJSONFGDataset::BeforeCreateFeature()
{
    if (m_nPositionBeforeFCClosed)
    {
        // If we had called SyncToDisk() previously, undo its effects
        fpOut_->Seek(m_nPositionBeforeFCClosed, SEEK_SET);
        m_nPositionBeforeFCClosed = 0;
    }

    if (!EmitStartFeaturesIfNeededAndReturnIfFirstFeature())
    {
        VSIFPrintfL(fpOut_, ",\n");
    }
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

bool OGRJSONFGDataset::Open(GDALOpenInfo *poOpenInfo,
                            GeoJSONSourceType nSrcType)
{
    const char *pszUnprefixed = poOpenInfo->pszFilename;
    if (STARTS_WITH_CI(pszUnprefixed, "JSONFG:"))
    {
        pszUnprefixed += strlen("JSONFG:");
    }

    std::string osDefaultLayerName;

    VSIVirtualHandleUniquePtr fp;
    if (nSrcType == eGeoJSONSourceService)
    {
        if (!ReadFromService(poOpenInfo, pszUnprefixed))
            return false;
        if (poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update from remote service not supported");
            return false;
        }
    }
    else if (nSrcType == eGeoJSONSourceText)
    {
        if (poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update from inline definition not supported");
            return false;
        }
        pszGeoData_ = CPLStrdup(pszUnprefixed);
    }
    else if (nSrcType == eGeoJSONSourceFile)
    {
        if (poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Update not supported");
            return false;
        }
        SetDescription(pszUnprefixed);
        osDefaultLayerName = CPLGetBasename(pszUnprefixed);
        eAccess = poOpenInfo->eAccess;

        // Ingests the first bytes of the file in pszGeoData_
        if (!EQUAL(pszUnprefixed, poOpenInfo->pszFilename))
        {
            GDALOpenInfo oOpenInfo(pszUnprefixed, GA_ReadOnly);
            if (oOpenInfo.fpL == nullptr || oOpenInfo.pabyHeader == nullptr)
                return false;
            pszGeoData_ =
                CPLStrdup(reinterpret_cast<const char *>(oOpenInfo.pabyHeader));
            fp.reset(oOpenInfo.fpL);
            oOpenInfo.fpL = nullptr;
        }
        else if (poOpenInfo->fpL == nullptr)
            return false;
        else
        {
            fp.reset(poOpenInfo->fpL);
            poOpenInfo->fpL = nullptr;
            pszGeoData_ = CPLStrdup(
                reinterpret_cast<const char *>(poOpenInfo->pabyHeader));
        }
    }
    else
    {
        return false;
    }

    if (osDefaultLayerName.empty())
        osDefaultLayerName = "features";

    const auto SetReaderOptions = [poOpenInfo](OGRJSONFGReader &oReader)
    {
        const char *pszGeometryElement = CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "GEOMETRY_ELEMENT", "AUTO");
        if (EQUAL(pszGeometryElement, "PLACE"))
            oReader.SetGeometryElement(OGRJSONFGReader::GeometryElement::PLACE);
        else if (EQUAL(pszGeometryElement, "GEOMETRY"))
            oReader.SetGeometryElement(
                OGRJSONFGReader::GeometryElement::GEOMETRY);
    };

    if (nSrcType == eGeoJSONSourceFile)
    {
        auto poReader = std::make_unique<OGRJSONFGReader>();
        SetReaderOptions(*(poReader.get()));

        // Try to use a streaming parser if the content of the file seems
        // to be FeatureCollection
        bool bUseStreamingInterface = false;
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
        if (bUseStreamingInterface)
        {
            bool bCanTryWithNonStreamingParserOut = true;
            if (poReader->AnalyzeWithStreamingParser(
                    this, fp.get(), osDefaultLayerName,
                    bCanTryWithNonStreamingParserOut))
            {
                if (!apoLayers_.empty())
                {
                    auto poLayer = cpl::down_cast<OGRJSONFGStreamedLayer *>(
                        apoLayers_[0].get());
                    poLayer->SetFile(std::move(fp));
                    auto poParser = std::make_unique<OGRJSONFGStreamingParser>(
                        *(poReader.get()), false);
                    poLayer->SetStreamingParser(std::move(poParser));
                }

                for (size_t i = 1; i < apoLayers_.size(); ++i)
                {
                    auto poLayer = cpl::down_cast<OGRJSONFGStreamedLayer *>(
                        apoLayers_[i].get());

                    auto fpNew = VSIVirtualHandleUniquePtr(
                        VSIFOpenL(pszUnprefixed, "rb"));
                    if (!fpNew)
                    {
                        CPLError(CE_Failure, CPLE_FileIO,
                                 "Cannot open %s again", pszUnprefixed);
                        return false;
                    }
                    poLayer->SetFile(std::move(fpNew));

                    auto poParser = std::make_unique<OGRJSONFGStreamingParser>(
                        *(poReader.get()), false);
                    poLayer->SetStreamingParser(std::move(poParser));
                }
                poReader_ = std::move(poReader);
                return true;
            }
            if (!bCanTryWithNonStreamingParserOut)
                return false;
        }

        // Fallback to in-memory ingestion
        CPLAssert(poOpenInfo->fpL == nullptr);
        poOpenInfo->fpL = fp.release();
        if (!ReadFromFile(poOpenInfo, pszUnprefixed))
            return false;
    }

    // In-memory ingestion of the file
    OGRJSONFGReader oReader;
    SetReaderOptions(oReader);
    const bool bRet = oReader.Load(this, pszGeoData_, osDefaultLayerName);
    CPLFree(pszGeoData_);
    pszGeoData_ = nullptr;
    return bRet;
}

/************************************************************************/
/*                  OGRJSONFGDataset::GetLayer()                        */
/************************************************************************/

OGRLayer *OGRJSONFGDataset::GetLayer(int i)
{
    if (i < 0 || i >= static_cast<int>(apoLayers_.size()))
        return nullptr;
    return apoLayers_[i].get();
}

/************************************************************************/
/*                  OGRJSONFGDataset::AddLayer()                        */
/************************************************************************/

OGRJSONFGMemLayer *
OGRJSONFGDataset::AddLayer(std::unique_ptr<OGRJSONFGMemLayer> &&poLayer)
{
    apoLayers_.emplace_back(std::move(poLayer));
    return static_cast<OGRJSONFGMemLayer *>(apoLayers_.back().get());
}

/************************************************************************/
/*                  OGRJSONFGDataset::AddLayer()                        */
/************************************************************************/

OGRJSONFGStreamedLayer *
OGRJSONFGDataset::AddLayer(std::unique_ptr<OGRJSONFGStreamedLayer> &&poLayer)
{
    apoLayers_.emplace_back(std::move(poLayer));
    return static_cast<OGRJSONFGStreamedLayer *>(apoLayers_.back().get());
}

/************************************************************************/
/*                           ReadFromFile()                             */
/************************************************************************/

bool OGRJSONFGDataset::ReadFromFile(GDALOpenInfo *poOpenInfo,
                                    const char *pszUnprefixed)
{
    GByte *pabyOut = nullptr;
    if (!EQUAL(poOpenInfo->pszFilename, pszUnprefixed))
    {
        GDALOpenInfo oOpenInfo(pszUnprefixed, GA_ReadOnly);
        if (oOpenInfo.fpL == nullptr || oOpenInfo.pabyHeader == nullptr)
            return false;
        VSIFSeekL(oOpenInfo.fpL, 0, SEEK_SET);
        if (!VSIIngestFile(oOpenInfo.fpL, pszUnprefixed, &pabyOut, nullptr, -1))
        {
            return false;
        }
    }
    else
    {
        if (poOpenInfo->fpL == nullptr)
            return false;
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
        if (!VSIIngestFile(poOpenInfo->fpL, poOpenInfo->pszFilename, &pabyOut,
                           nullptr, -1))
        {
            return false;
        }

        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
    }

    CPLFree(pszGeoData_);
    pszGeoData_ = reinterpret_cast<char *>(pabyOut);

    CPLAssert(nullptr != pszGeoData_);

    return true;
}

/************************************************************************/
/*                           ReadFromService()                          */
/************************************************************************/

bool OGRJSONFGDataset::ReadFromService(GDALOpenInfo *poOpenInfo,
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
        if (JSONFGIsObject(pszStoredContent))
        {
            pszGeoData_ = pszStoredContent;
            nGeoDataLen_ = strlen(pszGeoData_);

            SetDescription(pszSource);
            return true;
        }

        OGRGeoJSONDriverStoreContent(pszSource, pszStoredContent);
        return false;
    }

    /* -------------------------------------------------------------------- */
    /*      Fetch the result.                                               */
    /* -------------------------------------------------------------------- */
    char *papsOptions[] = {
        const_cast<char *>("HEADERS=Accept: text/plain, application/json"),
        nullptr};

    CPLHTTPResult *pResult = CPLHTTPFetch(pszSource, papsOptions);

    /* -------------------------------------------------------------------- */
    /*      Try to handle CURL/HTTP errors.                                 */
    /* -------------------------------------------------------------------- */
    if (nullptr == pResult || 0 == pResult->nDataLen ||
        0 != CPLGetLastErrorNo())
    {
        CPLHTTPDestroyResult(pResult);
        return false;
    }

    if (0 != pResult->nStatus)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Curl reports error: %d: %s",
                 pResult->nStatus, pResult->pszErrBuf);
        CPLHTTPDestroyResult(pResult);
        return false;
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

    SetDescription(pszSource);

    /* -------------------------------------------------------------------- */
    /*      Cleanup HTTP resources.                                         */
    /* -------------------------------------------------------------------- */
    CPLHTTPDestroyResult(pResult);

    CPLAssert(nullptr != pszGeoData_);

    /* -------------------------------------------------------------------- */
    /*      Cache the content if it is not handled by this driver, but      */
    /*      another related one.                                            */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszSource, poOpenInfo->pszFilename))
    {
        if (!JSONFGIsObject(pszGeoData_))
        {
            OGRGeoJSONDriverStoreContent(pszSource, pszGeoData_);
            pszGeoData_ = nullptr;
            nGeoDataLen_ = 0;
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

bool OGRJSONFGDataset::Create(const char *pszName, CSLConstList papszOptions)
{
    CPLAssert(nullptr == fpOut_);
    bSingleOutputLayer_ =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "SINGLE_LAYER", "NO"));

    bFpOutputIsSeekable_ = !(strcmp(pszName, "/vsistdout/") == 0 ||
                             STARTS_WITH(pszName, "/vsigzip/") ||
                             STARTS_WITH(pszName, "/vsizip/"));

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    /* -------------------------------------------------------------------- */
    /*     File overwrite not supported.                                    */
    /* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;
    if (0 == VSIStatL(pszName, &sStatBuf))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The JSONFG driver does not overwrite existing files.");
        return false;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    fpOut_ = VSIFOpenExL(pszName, "w", true);
    if (nullptr == fpOut_)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to create JSONFG dataset: %s: %s", pszName,
                 VSIGetLastErrorMsg());
        return false;
    }

    SetDescription(pszName);

    VSIFPrintfL(fpOut_, "{\n\"type\": \"FeatureCollection\",\n");
    VSIFPrintfL(fpOut_, "\"conformsTo\" : [\"[ogc-json-fg-1-0.1:core]\"],\n");

    return true;
}

/************************************************************************/
/*                        EmitStartFeaturesIfNeeded()                   */
/************************************************************************/

bool OGRJSONFGDataset::EmitStartFeaturesIfNeededAndReturnIfFirstFeature()
{
    if (!bHasEmittedFeatures_)
    {
        bHasEmittedFeatures_ = true;
        VSIFPrintfL(fpOut_, "\"features\" : [\n");
        return true;
    }
    return false;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRJSONFGDataset::ICreateLayer(const char *pszNameIn,
                               const OGRGeomFieldDefn *poSrcGeomFieldDefn,
                               CSLConstList papszOptions)
{
    if (nullptr == fpOut_)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "JSONFG driver doesn't support creating a layer "
                 "on a read-only datasource");
        return nullptr;
    }

    if (bSingleOutputLayer_ && !apoLayers_.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only one layer can be created since SINGLE_LAYER=YES "
                 "creation option has been used");
        return nullptr;
    }

    const auto eGType =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetType() : wkbNone;
    const auto poSRS =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetSpatialRef() : nullptr;

    std::string osCoordRefSys;
    std::unique_ptr<OGRCoordinateTransformation> poCTToWGS84;
    if (poSRS)
    {
        const auto GetCURIE =
            [](const char *pszAuthName, const char *pszAuthCode)
        {
            std::string osRet = "[";
            if (STARTS_WITH(pszAuthName, "IAU_"))
                osRet += "IAU";
            else
                osRet += pszAuthName;
            osRet += ':';
            osRet += pszAuthCode;
            osRet += ']';
            return osRet;
        };

        const auto GetCoordRefSys = [GetCURIE](const char *pszAuthName,
                                               const char *pszAuthCode,
                                               double dfCoordEpoch = 0)
        {
            if (dfCoordEpoch > 0)
            {
                json_object *poObj = json_object_new_object();
                json_object_object_add(poObj, "type",
                                       json_object_new_string("Reference"));
                json_object_object_add(
                    poObj, "href",
                    json_object_new_string(
                        GetCURIE(pszAuthName, pszAuthCode).c_str()));
                json_object_object_add(poObj, "epoch",
                                       json_object_new_double(dfCoordEpoch));
                return poObj;
            }
            else
            {
                return json_object_new_string(
                    GetCURIE(pszAuthName, pszAuthCode).c_str());
            }
        };

        const char *pszAuthName = poSRS->GetAuthorityName(nullptr);
        const char *pszAuthCode = poSRS->GetAuthorityCode(nullptr);
        const double dfCoordEpoch = poSRS->GetCoordinateEpoch();
        json_object *poObj = nullptr;
        if (pszAuthName && pszAuthCode)
        {
            poObj = GetCoordRefSys(pszAuthName, pszAuthCode, dfCoordEpoch);
        }
        else if (poSRS->IsCompound())
        {
            const char *pszAuthNameHoriz = poSRS->GetAuthorityName("HORIZCRS");
            const char *pszAuthCodeHoriz = poSRS->GetAuthorityCode("HORIZCRS");
            const char *pszAuthNameVert = poSRS->GetAuthorityName("VERTCRS");
            const char *pszAuthCodeVert = poSRS->GetAuthorityCode("VERTCRS");
            if (pszAuthNameHoriz && pszAuthCodeHoriz && pszAuthNameVert &&
                pszAuthCodeVert)
            {
                poObj = json_object_new_array();
                json_object_array_add(poObj, GetCoordRefSys(pszAuthNameHoriz,
                                                            pszAuthCodeHoriz,
                                                            dfCoordEpoch));
                json_object_array_add(
                    poObj, GetCoordRefSys(pszAuthNameVert, pszAuthCodeVert));
            }
        }

        if (poObj)
        {
            osCoordRefSys =
                json_object_to_json_string_ext(poObj, JSON_C_TO_STRING_SPACED);
            json_object_put(poObj);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Input CRS %s cannot be expressed as a reference (ie "
                     "well-known CRS by code). "
                     "Retry be reprojecting to a known CRS first",
                     poSRS->GetName());
            return nullptr;
        }

        if (!strstr(osCoordRefSys.c_str(), "[IAU:"))
        {
            OGRSpatialReference oSRSWGS84;
            oSRSWGS84.SetWellKnownGeogCS("WGS84");
            oSRSWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            poCTToWGS84.reset(
                OGRCreateCoordinateTransformation(poSRS, &oSRSWGS84));
        }
    }
    else if (eGType != wkbNone)
    {
        if (OGR_GT_HasZ(eGType))
            osCoordRefSys = "[OGC:CRS84h]";
        else
            osCoordRefSys = "[OGC:CRS84]";
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No SRS set on layer. Assuming it is long/lat on WGS84 "
                 "ellipsoid");
    }

    CPLStringList aosOptions(papszOptions);

    if (const char *pszCoordPrecisionGeom =
            CSLFetchNameValue(papszOptions, "COORDINATE_PRECISION_GEOMETRY"))
    {
        double dfXYResolutionGeometry =
            std::pow(10.0, -CPLAtof(pszCoordPrecisionGeom));
        double dfZResolutionGeometry = dfXYResolutionGeometry;
        aosOptions.SetNameValue("XY_COORD_PRECISION_GEOMETRY",
                                pszCoordPrecisionGeom);
        aosOptions.SetNameValue("Z_COORD_PRECISION_GEOMETRY",
                                pszCoordPrecisionGeom);
        if (IsSingleOutputLayer())
        {
            VSIFPrintfL(fpOut_, "\"xy_coordinate_resolution\": %g,\n",
                        dfXYResolutionGeometry);
            if (poSRS && poSRS->GetAxesCount() == 3)
            {
                VSIFPrintfL(fpOut_, "\"z_coordinate_resolution\": %g,\n",
                            dfZResolutionGeometry);
            }
        }
    }
    else if (poSrcGeomFieldDefn &&
             poSrcGeomFieldDefn->GetCoordinatePrecision().dfXYResolution ==
                 OGRGeomCoordinatePrecision::UNKNOWN &&
             CSLFetchNameValue(papszOptions, "SIGNIFICANT_FIGURES") == nullptr)
    {
        const int nXYPrecisionGeometry = 7;
        const int nZPrecisionGeometry = 3;
        aosOptions.SetNameValue("XY_COORD_PRECISION_GEOMETRY",
                                CPLSPrintf("%d", nXYPrecisionGeometry));
        aosOptions.SetNameValue("Z_COORD_PRECISION_GEOMETRY",
                                CPLSPrintf("%d", nZPrecisionGeometry));
    }

    double dfXYResolution = OGRGeomCoordinatePrecision::UNKNOWN;
    double dfZResolution = OGRGeomCoordinatePrecision::UNKNOWN;

    if (const char *pszCoordPrecisionPlace =
            CSLFetchNameValue(papszOptions, "COORDINATE_PRECISION_PLACE"))
    {
        dfXYResolution = std::pow(10.0, -CPLAtof(pszCoordPrecisionPlace));
        dfZResolution = dfXYResolution;
        if (IsSingleOutputLayer())
        {
            VSIFPrintfL(fpOut_, "\"xy_coordinate_resolution_place\": %g,\n",
                        dfXYResolution);
            if (poSRS && poSRS->GetAxesCount() == 3)
            {
                VSIFPrintfL(fpOut_, "\"z_coordinate_resolution_place\": %g,\n",
                            dfZResolution);
            }
        }
    }
    else if (poSrcGeomFieldDefn &&
             CSLFetchNameValue(papszOptions, "COORDINATE_PRECISION_PLACE") ==
                 nullptr &&
             CSLFetchNameValue(papszOptions, "SIGNIFICANT_FIGURES") == nullptr)
    {
        const auto &oCoordPrec = poSrcGeomFieldDefn->GetCoordinatePrecision();
        OGRSpatialReference oSRSWGS84;
        oSRSWGS84.SetWellKnownGeogCS("WGS84");
        const auto oCoordPrecWGS84 =
            oCoordPrec.ConvertToOtherSRS(poSRS, &oSRSWGS84);

        if (oCoordPrec.dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            dfXYResolution = oCoordPrec.dfXYResolution;
            aosOptions.SetNameValue(
                "XY_COORD_PRECISION_PLACE",
                CPLSPrintf("%d",
                           OGRGeomCoordinatePrecision::ResolutionToPrecision(
                               oCoordPrec.dfXYResolution)));
            if (IsSingleOutputLayer())
            {
                VSIFPrintfL(fpOut_, "\"xy_coordinate_resolution_place\": %g,\n",
                            oCoordPrec.dfXYResolution);
            }

            if (CSLFetchNameValue(papszOptions,
                                  "COORDINATE_PRECISION_GEOMETRY") == nullptr)
            {
                const double dfXYResolutionGeometry =
                    oCoordPrecWGS84.dfXYResolution;

                aosOptions.SetNameValue(
                    "XY_COORD_PRECISION_GEOMETRY",
                    CPLSPrintf(
                        "%d", OGRGeomCoordinatePrecision::ResolutionToPrecision(
                                  dfXYResolutionGeometry)));
                if (IsSingleOutputLayer())
                {
                    VSIFPrintfL(fpOut_, "\"xy_coordinate_resolution\": %g,\n",
                                dfXYResolutionGeometry);
                }
            }
        }

        if (oCoordPrec.dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN)
        {
            dfZResolution = oCoordPrec.dfZResolution;
            aosOptions.SetNameValue(
                "Z_COORD_PRECISION_PLACE",
                CPLSPrintf("%d",
                           OGRGeomCoordinatePrecision::ResolutionToPrecision(
                               dfZResolution)));
            if (IsSingleOutputLayer())
            {
                VSIFPrintfL(fpOut_, "\"z_coordinate_resolution_place\": %g,\n",
                            dfZResolution);
            }

            if (CSLFetchNameValue(papszOptions,
                                  "COORDINATE_PRECISION_GEOMETRY") == nullptr)
            {
                const double dfZResolutionGeometry =
                    oCoordPrecWGS84.dfZResolution;

                aosOptions.SetNameValue(
                    "Z_COORD_PRECISION_GEOMETRY",
                    CPLSPrintf(
                        "%d", OGRGeomCoordinatePrecision::ResolutionToPrecision(
                                  dfZResolutionGeometry)));
                if (IsSingleOutputLayer())
                {
                    VSIFPrintfL(fpOut_, "\"z_coordinate_resolution\": %g,\n",
                                dfZResolutionGeometry);
                }
            }
        }
    }

    auto poLayer = std::make_unique<OGRJSONFGWriteLayer>(
        pszNameIn, poSRS, std::move(poCTToWGS84), osCoordRefSys, eGType,
        aosOptions.List(), this);
    apoLayers_.emplace_back(std::move(poLayer));

    auto poLayerAdded = apoLayers_.back().get();
    if (eGType != wkbNone &&
        dfXYResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        auto poGeomFieldDefn =
            poLayerAdded->GetLayerDefn()->GetGeomFieldDefn(0);
        OGRGeomCoordinatePrecision oCoordPrec(
            poGeomFieldDefn->GetCoordinatePrecision());
        oCoordPrec.dfXYResolution = dfXYResolution;
        poGeomFieldDefn->SetCoordinatePrecision(oCoordPrec);
    }

    if (eGType != wkbNone &&
        dfZResolution != OGRGeomCoordinatePrecision::UNKNOWN)
    {
        auto poGeomFieldDefn =
            poLayerAdded->GetLayerDefn()->GetGeomFieldDefn(0);
        OGRGeomCoordinatePrecision oCoordPrec(
            poGeomFieldDefn->GetCoordinatePrecision());
        oCoordPrec.dfZResolution = dfZResolution;
        poGeomFieldDefn->SetCoordinatePrecision(oCoordPrec);
    }

    return poLayerAdded;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRJSONFGDataset::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return fpOut_ != nullptr &&
               (!bSingleOutputLayer_ || apoLayers_.empty());
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                      OGRJSONFGMustSwapXY()                           */
/************************************************************************/

bool OGRJSONFGMustSwapXY(const OGRSpatialReference *poSRS)
{
    return poSRS->GetDataAxisToSRSAxisMapping() == std::vector<int>{2, 1} ||
           poSRS->GetDataAxisToSRSAxisMapping() == std::vector<int>{2, 1, 3};
}
