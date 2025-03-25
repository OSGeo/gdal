/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONReader class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2008-2017, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogrgeojsongeometry.h"
#include "ogr_geojson.h"
#include "ogrlibjsonutils.h"
#include "ogrjsoncollectionstreamingparser.h"
#include "ogr_api.h"

#include <cmath>
#include <limits>
#include <set>
#include <functional>

/************************************************************************/
/*                      OGRGeoJSONReaderStreamingParser                 */
/************************************************************************/

class OGRGeoJSONReaderStreamingParser final
    : public OGRJSONCollectionStreamingParser
{
    OGRGeoJSONReader &m_oReader;
    OGRGeoJSONLayer *m_poLayer = nullptr;

    std::vector<OGRFeature *> m_apoFeatures{};
    size_t m_nCurFeatureIdx = 0;
    bool m_bOriginalIdModifiedEmitted = false;
    std::set<GIntBig> m_oSetUsedFIDs{};

    std::map<std::string, int> m_oMapFieldNameToIdx{};
    std::vector<std::unique_ptr<OGRFieldDefn>> m_apoFieldDefn{};
    gdal::DirectedAcyclicGraph<int, std::string> m_dag{};

    void AnalyzeFeature();

    CPL_DISALLOW_COPY_ASSIGN(OGRGeoJSONReaderStreamingParser)

  protected:
    void GotFeature(json_object *poObj, bool bFirstPass,
                    const std::string &osJson) override;
    void TooComplex() override;

  public:
    OGRGeoJSONReaderStreamingParser(OGRGeoJSONReader &oReader,
                                    OGRGeoJSONLayer *poLayer, bool bFirstPass,
                                    bool bStoreNativeData);
    ~OGRGeoJSONReaderStreamingParser();

    void FinalizeLayerDefn();

    OGRFeature *GetNextFeature();

    inline bool GetOriginalIdModifiedEmitted() const
    {
        return m_bOriginalIdModifiedEmitted;
    }

    inline void SetOriginalIdModifiedEmitted(bool b)
    {
        m_bOriginalIdModifiedEmitted = b;
    }
};

/************************************************************************/
/*                        OGRGeoJSONBaseReader()                        */
/************************************************************************/

OGRGeoJSONBaseReader::OGRGeoJSONBaseReader() = default;

/************************************************************************/
/*                           SetPreserveGeometryType                    */
/************************************************************************/

void OGRGeoJSONBaseReader::SetPreserveGeometryType(bool bPreserve)
{
    bGeometryPreserve_ = bPreserve;
}

/************************************************************************/
/*                           SetSkipAttributes                          */
/************************************************************************/

void OGRGeoJSONBaseReader::SetSkipAttributes(bool bSkip)
{
    bAttributesSkip_ = bSkip;
}

/************************************************************************/
/*                         SetFlattenNestedAttributes                   */
/************************************************************************/

void OGRGeoJSONBaseReader::SetFlattenNestedAttributes(bool bFlatten,
                                                      char chSeparator)
{
    bFlattenNestedAttributes_ = bFlatten;
    chNestedAttributeSeparator_ = chSeparator;
}

/************************************************************************/
/*                           SetStoreNativeData                         */
/************************************************************************/

void OGRGeoJSONBaseReader::SetStoreNativeData(bool bStoreNativeData)
{
    bStoreNativeData_ = bStoreNativeData;
}

/************************************************************************/
/*                           SetArrayAsString                           */
/************************************************************************/

void OGRGeoJSONBaseReader::SetArrayAsString(bool bArrayAsString)
{
    bArrayAsString_ = bArrayAsString;
}

/************************************************************************/
/*                           SetDateAsString                           */
/************************************************************************/

void OGRGeoJSONBaseReader::SetDateAsString(bool bDateAsString)
{
    bDateAsString_ = bDateAsString;
}

/************************************************************************/
/*                           OGRGeoJSONReader                           */
/************************************************************************/

OGRGeoJSONReader::OGRGeoJSONReader()
    : poGJObject_(nullptr), poStreamingParser_(nullptr), bFirstSeg_(false),
      bJSonPLikeWrapper_(false), fp_(nullptr), bCanEasilyAppend_(false),
      bFCHasBBOX_(false), nBufferSize_(0), pabyBuffer_(nullptr),
      nTotalFeatureCount_(0), nTotalOGRFeatureMemEstimate_(0)
{
}

/************************************************************************/
/*                          ~OGRGeoJSONReader                           */
/************************************************************************/

OGRGeoJSONReader::~OGRGeoJSONReader()
{
    if (nullptr != poGJObject_)
    {
        json_object_put(poGJObject_);
    }
    if (fp_ != nullptr)
    {
        VSIFCloseL(fp_);
    }
    delete poStreamingParser_;
    CPLFree(pabyBuffer_);

    poGJObject_ = nullptr;
}

/************************************************************************/
/*                           Parse                                      */
/************************************************************************/

OGRErr OGRGeoJSONReader::Parse(const char *pszText)
{
    if (nullptr != pszText)
    {
        // Skip UTF-8 BOM (#5630).
        const GByte *pabyData = (const GByte *)pszText;
        if (pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF)
        {
            CPLDebug("GeoJSON", "Skip UTF-8 BOM");
            pszText += 3;
        }

        if (poGJObject_ != nullptr)
        {
            json_object_put(poGJObject_);
            poGJObject_ = nullptr;
        }

        // JSON tree is shared for while lifetime of the reader object
        // and will be released in the destructor.
        if (!OGRJSonParse(pszText, &poGJObject_))
            return OGRERR_CORRUPT_DATA;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReadLayers                                 */
/************************************************************************/

void OGRGeoJSONReader::ReadLayers(OGRGeoJSONDataSource *poDS)
{
    if (nullptr == poGJObject_)
    {
        CPLDebug("GeoJSON",
                 "Missing parsed GeoJSON data. Forgot to call Parse()?");
        return;
    }

    ReadLayer(poDS, nullptr, poGJObject_);
}

/************************************************************************/
/*           OGRGeoJSONReaderStreamingParserGetMaxObjectSize()          */
/************************************************************************/

static size_t OGRGeoJSONReaderStreamingParserGetMaxObjectSize()
{
    const double dfTmp =
        CPLAtof(CPLGetConfigOption("OGR_GEOJSON_MAX_OBJ_SIZE", "200"));
    return dfTmp > 0 ? static_cast<size_t>(dfTmp * 1024 * 1024) : 0;
}

/************************************************************************/
/*                     OGRGeoJSONReaderStreamingParser()                */
/************************************************************************/

OGRGeoJSONReaderStreamingParser::OGRGeoJSONReaderStreamingParser(
    OGRGeoJSONReader &oReader, OGRGeoJSONLayer *poLayer, bool bFirstPass,
    bool bStoreNativeData)
    : OGRJSONCollectionStreamingParser(
          bFirstPass, bStoreNativeData,
          OGRGeoJSONReaderStreamingParserGetMaxObjectSize()),
      m_oReader(oReader), m_poLayer(poLayer)
{
}

/************************************************************************/
/*                   ~OGRGeoJSONReaderStreamingParser()                 */
/************************************************************************/

OGRGeoJSONReaderStreamingParser::~OGRGeoJSONReaderStreamingParser()
{
    for (size_t i = 0; i < m_apoFeatures.size(); i++)
        delete m_apoFeatures[i];
}

/************************************************************************/
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature *OGRGeoJSONReaderStreamingParser::GetNextFeature()
{
    if (m_nCurFeatureIdx < m_apoFeatures.size())
    {
        OGRFeature *poFeat = m_apoFeatures[m_nCurFeatureIdx];
        m_apoFeatures[m_nCurFeatureIdx] = nullptr;
        m_nCurFeatureIdx++;
        return poFeat;
    }
    m_nCurFeatureIdx = 0;
    m_apoFeatures.clear();
    return nullptr;
}

/************************************************************************/
/*                          GotFeature()                                */
/************************************************************************/

void OGRGeoJSONReaderStreamingParser::GotFeature(json_object *poObj,
                                                 bool bFirstPass,
                                                 const std::string &osJson)
{
    if (bFirstPass)
    {
        if (!m_oReader.GenerateFeatureDefn(m_oMapFieldNameToIdx, m_apoFieldDefn,
                                           m_dag, m_poLayer, poObj))
        {
        }
        m_poLayer->IncFeatureCount();
    }
    else
    {
        OGRFeature *poFeat =
            m_oReader.ReadFeature(m_poLayer, poObj, osJson.c_str());
        if (poFeat)
        {
            GIntBig nFID = poFeat->GetFID();
            if (nFID == OGRNullFID)
            {
                nFID = static_cast<GIntBig>(m_oSetUsedFIDs.size());
                while (cpl::contains(m_oSetUsedFIDs, nFID))
                {
                    ++nFID;
                }
            }
            else if (cpl::contains(m_oSetUsedFIDs, nFID))
            {
                if (!m_bOriginalIdModifiedEmitted)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Several features with id = " CPL_FRMT_GIB " have "
                             "been found. Altering it to be unique. "
                             "This warning will not be emitted anymore for "
                             "this layer",
                             nFID);
                    m_bOriginalIdModifiedEmitted = true;
                }
                nFID = static_cast<GIntBig>(m_oSetUsedFIDs.size());
                while (cpl::contains(m_oSetUsedFIDs, nFID))
                {
                    ++nFID;
                }
            }
            m_oSetUsedFIDs.insert(nFID);
            poFeat->SetFID(nFID);

            m_apoFeatures.push_back(poFeat);
        }
    }
}

/************************************************************************/
/*                         FinalizeLayerDefn()                          */
/************************************************************************/

void OGRGeoJSONReaderStreamingParser::FinalizeLayerDefn()
{
    OGRFeatureDefn *poDefn = m_poLayer->GetLayerDefn();
    auto oTemporaryUnsealer(poDefn->GetTemporaryUnsealer());
    const auto sortedFields = m_dag.getTopologicalOrdering();
    CPLAssert(sortedFields.size() == m_apoFieldDefn.size());
    for (int idx : sortedFields)
    {
        poDefn->AddFieldDefn(m_apoFieldDefn[idx].get());
    }
    m_dag = gdal::DirectedAcyclicGraph<int, std::string>();
    m_oMapFieldNameToIdx.clear();
    m_apoFieldDefn.clear();
}

/************************************************************************/
/*                            TooComplex()                              */
/************************************************************************/

void OGRGeoJSONReaderStreamingParser::TooComplex()
{
    if (!ExceptionOccurred())
        EmitException("GeoJSON object too complex/large. You may define the "
                      "OGR_GEOJSON_MAX_OBJ_SIZE configuration option to "
                      "a value in megabytes to allow "
                      "for larger features, or 0 to remove any size limit.");
}

/************************************************************************/
/*                       SetCoordinatePrecision()                       */
/************************************************************************/

static void SetCoordinatePrecision(json_object *poRootObj,
                                   OGRGeoJSONLayer *poLayer)
{
    if (poLayer->GetLayerDefn()->GetGeomType() != wkbNone)
    {
        OGRGeoJSONWriteOptions options;

        json_object *poXYRes =
            CPL_json_object_object_get(poRootObj, "xy_coordinate_resolution");
        if (poXYRes && (json_object_get_type(poXYRes) == json_type_double ||
                        json_object_get_type(poXYRes) == json_type_int))
        {
            auto poGeomFieldDefn = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
            OGRGeomCoordinatePrecision oCoordPrec(
                poGeomFieldDefn->GetCoordinatePrecision());
            oCoordPrec.dfXYResolution = json_object_get_double(poXYRes);
            whileUnsealing(poGeomFieldDefn)->SetCoordinatePrecision(oCoordPrec);

            options.nXYCoordPrecision =
                OGRGeomCoordinatePrecision::ResolutionToPrecision(
                    oCoordPrec.dfXYResolution);
        }

        json_object *poZRes =
            CPL_json_object_object_get(poRootObj, "z_coordinate_resolution");
        if (poZRes && (json_object_get_type(poZRes) == json_type_double ||
                       json_object_get_type(poZRes) == json_type_int))
        {
            auto poGeomFieldDefn = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
            OGRGeomCoordinatePrecision oCoordPrec(
                poGeomFieldDefn->GetCoordinatePrecision());
            oCoordPrec.dfZResolution = json_object_get_double(poZRes);
            whileUnsealing(poGeomFieldDefn)->SetCoordinatePrecision(oCoordPrec);

            options.nZCoordPrecision =
                OGRGeomCoordinatePrecision::ResolutionToPrecision(
                    oCoordPrec.dfZResolution);
        }

        poLayer->SetWriteOptions(options);
    }
}

/************************************************************************/
/*                       FirstPassReadLayer()                           */
/************************************************************************/

bool OGRGeoJSONReader::FirstPassReadLayer(OGRGeoJSONDataSource *poDS,
                                          VSILFILE *fp,
                                          bool &bTryStandardReading)
{
    bTryStandardReading = false;
    VSIFSeekL(fp, 0, SEEK_SET);
    bFirstSeg_ = true;

    std::string osName = poDS->GetDescription();
    if (STARTS_WITH_CI(osName.c_str(), "GeoJSON:"))
        osName = osName.substr(strlen("GeoJSON:"));
    osName = CPLGetBasenameSafe(osName.c_str());
    osName = OGRGeoJSONLayer::GetValidLayerName(osName.c_str());

    OGRGeoJSONLayer *poLayer =
        new OGRGeoJSONLayer(osName.c_str(), nullptr,
                            OGRGeoJSONLayer::DefaultGeometryType, poDS, this);
    OGRGeoJSONReaderStreamingParser oParser(*this, poLayer, true,
                                            bStoreNativeData_);

    vsi_l_offset nFileSize = 0;
    if (STARTS_WITH(poDS->GetDescription(), "/vsimem/") ||
        !STARTS_WITH(poDS->GetDescription(), "/vsi"))
    {
        VSIStatBufL sStatBuf;
        if (VSIStatL(poDS->GetDescription(), &sStatBuf) == 0)
        {
            nFileSize = sStatBuf.st_size;
        }
    }

    nBufferSize_ = 4096 * 10;
    pabyBuffer_ = static_cast<GByte *>(CPLMalloc(nBufferSize_));
    int nIter = 0;
    bool bThresholdReached = false;
    const GIntBig nMaxBytesFirstPass = CPLAtoGIntBig(
        CPLGetConfigOption("OGR_GEOJSON_MAX_BYTES_FIRST_PASS", "0"));
    const GIntBig nLimitFeaturesFirstPass = CPLAtoGIntBig(
        CPLGetConfigOption("OGR_GEOJSON_MAX_FEATURES_FIRST_PASS", "0"));
    while (true)
    {
        nIter++;

        if (nMaxBytesFirstPass > 0 &&
            static_cast<GIntBig>(nIter) * static_cast<GIntBig>(nBufferSize_) >=
                nMaxBytesFirstPass)
        {
            CPLDebug("GeoJSON", "First pass: early exit since above "
                                "OGR_GEOJSON_MAX_BYTES_FIRST_PASS");
            bThresholdReached = true;
            break;
        }

        size_t nRead = VSIFReadL(pabyBuffer_, 1, nBufferSize_, fp);
        const bool bFinished = nRead < nBufferSize_;
        size_t nSkip = 0;
        if (bFirstSeg_)
        {
            bFirstSeg_ = false;
            nSkip = SkipPrologEpilogAndUpdateJSonPLikeWrapper(nRead);
        }
        if (bFinished && bJSonPLikeWrapper_ && nRead > nSkip)
            nRead--;
        if (!oParser.Parse(reinterpret_cast<const char *>(pabyBuffer_ + nSkip),
                           nRead - nSkip, bFinished) ||
            oParser.ExceptionOccurred())
        {
            // to avoid killing ourselves during layer deletion
            poLayer->UnsetReader();
            delete poLayer;
            return false;
        }
        if (bFinished || (nIter % 100) == 0)
        {
            if (nFileSize == 0)
            {
                if (bFinished)
                {
                    CPLDebug("GeoJSON", "First pass: 100.00 %%");
                }
                else
                {
                    CPLDebug("GeoJSON",
                             "First pass: " CPL_FRMT_GUIB " bytes read",
                             static_cast<GUIntBig>(nIter) *
                                     static_cast<GUIntBig>(nBufferSize_) +
                                 nRead);
                }
            }
            else
            {
                CPLDebug("GeoJSON", "First pass: %.2f %%",
                         100.0 * VSIFTellL(fp) / nFileSize);
            }
        }
        if (nLimitFeaturesFirstPass > 0 &&
            poLayer->GetFeatureCount(FALSE) >= nLimitFeaturesFirstPass)
        {
            CPLDebug("GeoJSON", "First pass: early exit since above "
                                "OGR_GEOJSON_MAX_FEATURES_FIRST_PASS");
            bThresholdReached = true;
            break;
        }
        if (oParser.IsTypeKnown() && !oParser.IsFeatureCollection())
            break;
        if (bFinished)
            break;
    }

    if (bThresholdReached)
    {
        poLayer->InvalidateFeatureCount();
    }
    else if (!oParser.IsTypeKnown() || !oParser.IsFeatureCollection())
    {
        // to avoid killing ourselves during layer deletion
        poLayer->UnsetReader();
        delete poLayer;
        const vsi_l_offset nRAM =
            static_cast<vsi_l_offset>(CPLGetUsablePhysicalRAM());
        if (nFileSize == 0 || nRAM == 0 || nRAM > nFileSize * 20)
        {
            // Only try full ingestion if we have 20x more RAM than the file
            // size
            bTryStandardReading = true;
        }
        return false;
    }

    oParser.FinalizeLayerDefn();

    CPLString osFIDColumn;
    FinalizeLayerDefn(poLayer, osFIDColumn);
    if (!osFIDColumn.empty())
        poLayer->SetFIDColumn(osFIDColumn);

    bCanEasilyAppend_ = oParser.CanEasilyAppend();
    nTotalFeatureCount_ = poLayer->GetFeatureCount(FALSE);
    nTotalOGRFeatureMemEstimate_ = oParser.GetTotalOGRFeatureMemEstimate();

    json_object *poRootObj = oParser.StealRootObject();
    if (poRootObj)
    {
        bFCHasBBOX_ = CPL_json_object_object_get(poRootObj, "bbox") != nullptr;

        // CPLDebug("GeoJSON", "%s", json_object_get_string(poRootObj));

        json_object *poName = CPL_json_object_object_get(poRootObj, "name");
        if (poName && json_object_get_type(poName) == json_type_string)
        {
            const char *pszValue = json_object_get_string(poName);
            whileUnsealing(poLayer->GetLayerDefn())->SetName(pszValue);
            poLayer->SetDescription(pszValue);
        }

        json_object *poDescription =
            CPL_json_object_object_get(poRootObj, "description");
        if (poDescription &&
            json_object_get_type(poDescription) == json_type_string)
        {
            const char *pszValue = json_object_get_string(poDescription);
            poLayer->SetMetadataItem("DESCRIPTION", pszValue);
        }

        OGRSpatialReference *poSRS = OGRGeoJSONReadSpatialReference(poRootObj);
        const auto eGeomType = poLayer->GetLayerDefn()->GetGeomType();
        if (eGeomType != wkbNone && poSRS == nullptr)
        {
            // If there is none defined, we use 4326 / 4979.
            poSRS = new OGRSpatialReference();
            if (OGR_GT_HasZ(eGeomType))
                poSRS->importFromEPSG(4979);
            else
                poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
        CPLErrorReset();

        if (eGeomType != wkbNone && poSRS != nullptr)
        {
            auto poGeomFieldDefn = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
            whileUnsealing(poGeomFieldDefn)->SetSpatialRef(poSRS);
        }
        if (poSRS)
            poSRS->Release();

        SetCoordinatePrecision(poRootObj, poLayer);

        if (bStoreNativeData_)
        {
            CPLString osNativeData("NATIVE_DATA=");
            osNativeData += json_object_get_string(poRootObj);

            char *apszMetadata[3] = {
                const_cast<char *>(osNativeData.c_str()),
                const_cast<char *>(
                    "NATIVE_MEDIA_TYPE=application/vnd.geo+json"),
                nullptr};

            poLayer->SetMetadata(apszMetadata, "NATIVE_DATA");
        }

        poGJObject_ = poRootObj;
    }

    fp_ = fp;
    poDS->AddLayer(poLayer);
    return true;
}

/************************************************************************/
/*               SkipPrologEpilogAndUpdateJSonPLikeWrapper()            */
/************************************************************************/

size_t OGRGeoJSONReader::SkipPrologEpilogAndUpdateJSonPLikeWrapper(size_t nRead)
{
    size_t nSkip = 0;
    if (nRead >= 3 && pabyBuffer_[0] == 0xEF && pabyBuffer_[1] == 0xBB &&
        pabyBuffer_[2] == 0xBF)
    {
        CPLDebug("GeoJSON", "Skip UTF-8 BOM");
        nSkip += 3;
    }

    const char *const apszPrefix[] = {"loadGeoJSON(", "jsonp("};
    for (size_t i = 0; i < CPL_ARRAYSIZE(apszPrefix); i++)
    {
        if (nRead >= nSkip + strlen(apszPrefix[i]) &&
            memcmp(pabyBuffer_ + nSkip, apszPrefix[i], strlen(apszPrefix[i])) ==
                0)
        {
            nSkip += strlen(apszPrefix[i]);
            bJSonPLikeWrapper_ = true;
            break;
        }
    }

    return nSkip;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoJSONReader::ResetReading()
{
    CPLAssert(fp_);
    if (poStreamingParser_)
        bOriginalIdModifiedEmitted_ =
            poStreamingParser_->GetOriginalIdModifiedEmitted();
    delete poStreamingParser_;
    poStreamingParser_ = nullptr;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoJSONReader::GetNextFeature(OGRGeoJSONLayer *poLayer)
{
    CPLAssert(fp_);
    if (poStreamingParser_ == nullptr)
    {
        poStreamingParser_ = new OGRGeoJSONReaderStreamingParser(
            *this, poLayer, false, bStoreNativeData_);
        poStreamingParser_->SetOriginalIdModifiedEmitted(
            bOriginalIdModifiedEmitted_);
        VSIFSeekL(fp_, 0, SEEK_SET);
        bFirstSeg_ = true;
        bJSonPLikeWrapper_ = false;
    }

    OGRFeature *poFeat = poStreamingParser_->GetNextFeature();
    if (poFeat)
        return poFeat;

    while (true)
    {
        size_t nRead = VSIFReadL(pabyBuffer_, 1, nBufferSize_, fp_);
        const bool bFinished = nRead < nBufferSize_;
        size_t nSkip = 0;
        if (bFirstSeg_)
        {
            bFirstSeg_ = false;
            nSkip = SkipPrologEpilogAndUpdateJSonPLikeWrapper(nRead);
        }
        if (bFinished && bJSonPLikeWrapper_ && nRead > nSkip)
            nRead--;
        if (!poStreamingParser_->Parse(
                reinterpret_cast<const char *>(pabyBuffer_ + nSkip),
                nRead - nSkip, bFinished) ||
            poStreamingParser_->ExceptionOccurred())
        {
            break;
        }

        poFeat = poStreamingParser_->GetNextFeature();
        if (poFeat)
            return poFeat;

        if (bFinished)
            break;
    }

    return nullptr;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRGeoJSONReader::GetFeature(OGRGeoJSONLayer *poLayer, GIntBig nFID)
{
    CPLAssert(fp_);

    if (oMapFIDToOffsetSize_.empty())
    {
        CPLDebug("GeoJSON",
                 "Establishing index to features for first GetFeature() call");

        if (poStreamingParser_)
            bOriginalIdModifiedEmitted_ =
                poStreamingParser_->GetOriginalIdModifiedEmitted();
        delete poStreamingParser_;
        poStreamingParser_ = nullptr;

        OGRGeoJSONReaderStreamingParser oParser(*this, poLayer, false,
                                                bStoreNativeData_);
        oParser.SetOriginalIdModifiedEmitted(bOriginalIdModifiedEmitted_);
        VSIFSeekL(fp_, 0, SEEK_SET);
        bFirstSeg_ = true;
        bJSonPLikeWrapper_ = false;
        vsi_l_offset nCurOffset = 0;
        vsi_l_offset nFeatureOffset = 0;
        while (true)
        {
            size_t nRead = VSIFReadL(pabyBuffer_, 1, nBufferSize_, fp_);
            const bool bFinished = nRead < nBufferSize_;
            size_t nSkip = 0;
            if (bFirstSeg_)
            {
                bFirstSeg_ = false;
                nSkip = SkipPrologEpilogAndUpdateJSonPLikeWrapper(nRead);
            }
            if (bFinished && bJSonPLikeWrapper_ && nRead > nSkip)
                nRead--;
            auto pszPtr = reinterpret_cast<const char *>(pabyBuffer_ + nSkip);
            for (size_t i = 0; i < nRead - nSkip; i++)
            {
                oParser.ResetFeatureDetectionState();
                if (!oParser.Parse(pszPtr + i, 1,
                                   bFinished && (i + 1 == nRead - nSkip)) ||
                    oParser.ExceptionOccurred())
                {
                    return nullptr;
                }
                if (oParser.IsStartFeature())
                {
                    nFeatureOffset = nCurOffset + i;
                }
                else if (oParser.IsEndFeature())
                {
                    vsi_l_offset nFeatureSize =
                        (nCurOffset + i) - nFeatureOffset + 1;
                    auto poFeat = oParser.GetNextFeature();
                    if (poFeat)
                    {
                        const GIntBig nThisFID = poFeat->GetFID();
                        if (!cpl::contains(oMapFIDToOffsetSize_, nThisFID))
                        {
                            oMapFIDToOffsetSize_[nThisFID] =
                                std::pair<vsi_l_offset, vsi_l_offset>(
                                    nFeatureOffset, nFeatureSize);
                        }
                        delete poFeat;
                    }
                }
            }

            if (bFinished)
                break;
            nCurOffset += nRead;
        }

        bOriginalIdModifiedEmitted_ = oParser.GetOriginalIdModifiedEmitted();
    }

    const auto oIter = oMapFIDToOffsetSize_.find(nFID);
    if (oIter == oMapFIDToOffsetSize_.end())
    {
        return nullptr;
    }

    VSIFSeekL(fp_, oIter->second.first, SEEK_SET);
    if (oIter->second.second > 1000 * 1000 * 1000)
    {
        return nullptr;
    }
    size_t nSize = static_cast<size_t>(oIter->second.second);
    char *pszBuffer = static_cast<char *>(VSIMalloc(nSize + 1));
    if (!pszBuffer)
    {
        return nullptr;
    }
    if (VSIFReadL(pszBuffer, 1, nSize, fp_) != nSize)
    {
        VSIFree(pszBuffer);
        return nullptr;
    }
    pszBuffer[nSize] = 0;
    json_object *poObj = nullptr;
    if (!OGRJSonParse(pszBuffer, &poObj))
    {
        VSIFree(pszBuffer);
        return nullptr;
    }

    OGRFeature *poFeat = ReadFeature(poLayer, poObj, pszBuffer);
    json_object_put(poObj);
    VSIFree(pszBuffer);
    if (!poFeat)
    {
        return nullptr;
    }
    poFeat->SetFID(nFID);
    return poFeat;
}

/************************************************************************/
/*                           IngestAll()                                */
/************************************************************************/

bool OGRGeoJSONReader::IngestAll(OGRGeoJSONLayer *poLayer)
{
    const vsi_l_offset nRAM =
        static_cast<vsi_l_offset>(CPLGetUsablePhysicalRAM()) / 3 * 4;
    if (nRAM && nTotalOGRFeatureMemEstimate_ > nRAM)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Not enough memory to ingest all the layer: " CPL_FRMT_GUIB
                 " available, " CPL_FRMT_GUIB " needed",
                 nRAM, nTotalOGRFeatureMemEstimate_);
        return false;
    }

    CPLDebug("GeoJSON",
             "Total memory estimated for ingestion: " CPL_FRMT_GUIB " bytes",
             nTotalOGRFeatureMemEstimate_);

    ResetReading();
    GIntBig nCounter = 0;
    while (true)
    {
        OGRFeature *poFeature = GetNextFeature(poLayer);
        if (poFeature == nullptr)
            break;
        poLayer->AddFeature(poFeature);
        delete poFeature;
        nCounter++;
        if (((nCounter % 10000) == 0 || nCounter == nTotalFeatureCount_) &&
            nTotalFeatureCount_ > 0)
        {
            CPLDebug("GeoJSON", "Ingestion at %.02f %%",
                     100.0 * nCounter / nTotalFeatureCount_);
        }
    }
    return true;
}

/************************************************************************/
/*                           ReadLayer                                  */
/************************************************************************/

void OGRGeoJSONReader::ReadLayer(OGRGeoJSONDataSource *poDS,
                                 const char *pszName, json_object *poObj)
{
    GeoJSONObject::Type objType = OGRGeoJSONGetType(poObj);
    if (objType == GeoJSONObject::eUnknown)
    {
        // Check if the object contains key:value pairs where value
        // is a standard GeoJSON object. In which case, use key as the layer
        // name.
        if (json_type_object == json_object_get_type(poObj))
        {
            json_object_iter it;
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
            json_object_object_foreachC(poObj, it)
            {
                objType = OGRGeoJSONGetType(it.val);
                if (objType != GeoJSONObject::eUnknown)
                    ReadLayer(poDS, it.key, it.val);
            }
        }

        // CPLError(CE_Failure, CPLE_AppDefined,
        //          "Unrecognized GeoJSON structure.");

        return;
    }

    CPLErrorReset();

    // Figure out layer name
    std::string osName;
    if (pszName)
    {
        osName = pszName;
    }
    else
    {
        if (GeoJSONObject::eFeatureCollection == objType)
        {
            json_object *poName = CPL_json_object_object_get(poObj, "name");
            if (poName != nullptr &&
                json_object_get_type(poName) == json_type_string)
            {
                pszName = json_object_get_string(poName);
            }
        }
        if (pszName)
        {
            osName = pszName;
        }
        else
        {
            const char *pszDesc = poDS->GetDescription();
            if (strchr(pszDesc, '?') == nullptr &&
                strchr(pszDesc, '{') == nullptr)
            {
                osName = CPLGetBasenameSafe(pszDesc);
            }
        }
    }
    osName = OGRGeoJSONLayer::GetValidLayerName(osName.c_str());

    OGRGeoJSONLayer *poLayer = new OGRGeoJSONLayer(
        osName.c_str(), nullptr, OGRGeoJSONLayer::DefaultGeometryType, poDS,
        nullptr);

    OGRSpatialReference *poSRS = OGRGeoJSONReadSpatialReference(poObj);
    bool bDefaultSRS = false;
    if (poSRS == nullptr)
    {
        // If there is none defined, we use 4326 / 4979.
        poSRS = new OGRSpatialReference();
        bDefaultSRS = true;
    }
    {
        auto poGeomFieldDefn = poLayer->GetLayerDefn()->GetGeomFieldDefn(0);
        whileUnsealing(poGeomFieldDefn)->SetSpatialRef(poSRS);
    }

    if (!GenerateLayerDefn(poLayer, poObj))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer schema generation failed.");

        delete poLayer;
        poSRS->Release();
        return;
    }

    if (GeoJSONObject::eFeatureCollection == objType)
    {
        json_object *poDescription =
            CPL_json_object_object_get(poObj, "description");
        if (poDescription != nullptr &&
            json_object_get_type(poDescription) == json_type_string)
        {
            poLayer->SetMetadataItem("DESCRIPTION",
                                     json_object_get_string(poDescription));
        }

        SetCoordinatePrecision(poObj, poLayer);
    }

    /* -------------------------------------------------------------------- */
    /*      Translate single geometry-only Feature object.                  */
    /* -------------------------------------------------------------------- */

    if (GeoJSONObject::ePoint == objType ||
        GeoJSONObject::eMultiPoint == objType ||
        GeoJSONObject::eLineString == objType ||
        GeoJSONObject::eMultiLineString == objType ||
        GeoJSONObject::ePolygon == objType ||
        GeoJSONObject::eMultiPolygon == objType ||
        GeoJSONObject::eGeometryCollection == objType)
    {
        OGRGeometry *poGeometry = ReadGeometry(poObj, poLayer->GetSpatialRef());
        if (!AddFeature(poLayer, poGeometry))
        {
            CPLDebug("GeoJSON", "Translation of single geometry failed.");
            delete poLayer;
            poSRS->Release();
            return;
        }
    }
    /* -------------------------------------------------------------------- */
    /*      Translate single but complete Feature object.                   */
    /* -------------------------------------------------------------------- */
    else if (GeoJSONObject::eFeature == objType)
    {
        OGRFeature *poFeature = ReadFeature(poLayer, poObj, nullptr);
        AddFeature(poLayer, poFeature);
    }
    /* -------------------------------------------------------------------- */
    /*      Translate multi-feature FeatureCollection object.               */
    /* -------------------------------------------------------------------- */
    else if (GeoJSONObject::eFeatureCollection == objType)
    {
        ReadFeatureCollection(poLayer, poObj);

        if (CPLGetLastErrorType() != CE_Warning)
            CPLErrorReset();
    }

    poLayer->DetectGeometryType();

    if (bDefaultSRS && poLayer->GetGeomType() != wkbNone)
    {
        if (OGR_GT_HasZ(poLayer->GetGeomType()))
            poSRS->importFromEPSG(4979);
        else
            poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    poSRS->Release();

    poDS->AddLayer(poLayer);
}

/************************************************************************/
/*                         GenerateLayerDefn()                          */
/************************************************************************/

bool OGRGeoJSONReader::GenerateLayerDefn(OGRGeoJSONLayer *poLayer,
                                         json_object *poGJObject)
{
    CPLAssert(nullptr != poGJObject);
    CPLAssert(nullptr != poLayer->GetLayerDefn());
    CPLAssert(0 == poLayer->GetLayerDefn()->GetFieldCount());

    if (bAttributesSkip_)
        return true;

    /* -------------------------------------------------------------------- */
    /*      Scan all features and generate layer definition.                */
    /* -------------------------------------------------------------------- */
    bool bSuccess = true;

    std::map<std::string, int> oMapFieldNameToIdx;
    std::vector<std::unique_ptr<OGRFieldDefn>> apoFieldDefn;
    gdal::DirectedAcyclicGraph<int, std::string> dag;

    GeoJSONObject::Type objType = OGRGeoJSONGetType(poGJObject);
    if (GeoJSONObject::eFeature == objType)
    {
        bSuccess = GenerateFeatureDefn(oMapFieldNameToIdx, apoFieldDefn, dag,
                                       poLayer, poGJObject);
    }
    else if (GeoJSONObject::eFeatureCollection == objType)
    {
        json_object *poObjFeatures =
            OGRGeoJSONFindMemberByName(poGJObject, "features");
        if (nullptr != poObjFeatures &&
            json_type_array == json_object_get_type(poObjFeatures))
        {
            const auto nFeatures = json_object_array_length(poObjFeatures);
            for (auto i = decltype(nFeatures){0}; i < nFeatures; ++i)
            {
                json_object *poObjFeature =
                    json_object_array_get_idx(poObjFeatures, i);
                if (!GenerateFeatureDefn(oMapFieldNameToIdx, apoFieldDefn, dag,
                                         poLayer, poObjFeature))
                {
                    CPLDebug("GeoJSON", "Create feature schema failure.");
                    bSuccess = false;
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid FeatureCollection object. "
                     "Missing \'features\' member.");
            bSuccess = false;
        }
    }

    // Note: the current strategy will not produce stable output, depending
    // on the order of features, if there are conflicting order / cycles.
    // See https://github.com/OSGeo/gdal/pull/4552 for a number of potential
    // resolutions if that has to be solved in the future.
    OGRFeatureDefn *poDefn = poLayer->GetLayerDefn();
    const auto sortedFields = dag.getTopologicalOrdering();
    CPLAssert(sortedFields.size() == apoFieldDefn.size());
    {
        auto oTemporaryUnsealer(poDefn->GetTemporaryUnsealer());
        for (int idx : sortedFields)
        {
            poDefn->AddFieldDefn(apoFieldDefn[idx].get());
        }
    }

    CPLString osFIDColumn;
    FinalizeLayerDefn(poLayer, osFIDColumn);
    if (!osFIDColumn.empty())
        poLayer->SetFIDColumn(osFIDColumn);

    return bSuccess;
}

/************************************************************************/
/*                          FinalizeLayerDefn()                         */
/************************************************************************/

void OGRGeoJSONBaseReader::FinalizeLayerDefn(OGRLayer *poLayer,
                                             CPLString &osFIDColumn)
{
    /* -------------------------------------------------------------------- */
    /*      Validate and add FID column if necessary.                       */
    /* -------------------------------------------------------------------- */
    osFIDColumn.clear();
    OGRFeatureDefn *poLayerDefn = poLayer->GetLayerDefn();
    CPLAssert(nullptr != poLayerDefn);

    whileUnsealing(poLayerDefn)->SetGeomType(m_eLayerGeomType);

    if (m_bNeedFID64)
    {
        poLayer->SetMetadataItem(OLMD_FID64, "YES");
    }

    if (!bFeatureLevelIdAsFID_)
    {
        const int idx = poLayerDefn->GetFieldIndexCaseSensitive("id");
        if (idx >= 0)
        {
            OGRFieldDefn *poFDefn = poLayerDefn->GetFieldDefn(idx);
            if (poFDefn->GetType() == OFTInteger ||
                poFDefn->GetType() == OFTInteger64)
            {
                osFIDColumn = poLayerDefn->GetFieldDefn(idx)->GetNameRef();
            }
        }
    }
}

/************************************************************************/
/*                     OGRGeoJSONReaderAddOrUpdateField()               */
/************************************************************************/

void OGRGeoJSONReaderAddOrUpdateField(
    std::vector<int> &retIndices,
    std::map<std::string, int> &oMapFieldNameToIdx,
    std::vector<std::unique_ptr<OGRFieldDefn>> &apoFieldDefn,
    const char *pszKey, json_object *poVal, bool bFlattenNestedAttributes,
    char chNestedAttributeSeparator, bool bArrayAsString, bool bDateAsString,
    std::set<int> &aoSetUndeterminedTypeFields)
{
    const auto jType = json_object_get_type(poVal);
    if (bFlattenNestedAttributes && poVal != nullptr &&
        jType == json_type_object)
    {
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poVal, it)
        {
            char szSeparator[2] = {chNestedAttributeSeparator, '\0'};

            CPLString osAttrName(
                CPLSPrintf("%s%s%s", pszKey, szSeparator, it.key));
            if (it.val != nullptr &&
                json_object_get_type(it.val) == json_type_object)
            {
                OGRGeoJSONReaderAddOrUpdateField(
                    retIndices, oMapFieldNameToIdx, apoFieldDefn, osAttrName,
                    it.val, true, chNestedAttributeSeparator, bArrayAsString,
                    bDateAsString, aoSetUndeterminedTypeFields);
            }
            else
            {
                OGRGeoJSONReaderAddOrUpdateField(
                    retIndices, oMapFieldNameToIdx, apoFieldDefn, osAttrName,
                    it.val, false, 0, bArrayAsString, bDateAsString,
                    aoSetUndeterminedTypeFields);
            }
        }
        return;
    }

    const auto oMapFieldNameToIdxIter = oMapFieldNameToIdx.find(pszKey);
    if (oMapFieldNameToIdxIter == oMapFieldNameToIdx.end())
    {
        OGRFieldSubType eSubType;
        const OGRFieldType eType =
            GeoJSONPropertyToFieldType(poVal, eSubType, bArrayAsString);
        auto poFieldDefn = std::make_unique<OGRFieldDefn>(pszKey, eType);
        poFieldDefn->SetSubType(eSubType);
        if (eSubType == OFSTBoolean)
            poFieldDefn->SetWidth(1);
        if (poFieldDefn->GetType() == OFTString && !bDateAsString)
        {
            int nTZFlag = 0;
            poFieldDefn->SetType(
                GeoJSONStringPropertyToFieldType(poVal, nTZFlag));
            poFieldDefn->SetTZFlag(nTZFlag);
        }
        apoFieldDefn.emplace_back(std::move(poFieldDefn));
        const int nIndex = static_cast<int>(apoFieldDefn.size()) - 1;
        retIndices.emplace_back(nIndex);
        oMapFieldNameToIdx[pszKey] = nIndex;
        if (poVal == nullptr)
            aoSetUndeterminedTypeFields.insert(nIndex);
    }
    else if (poVal)
    {
        const int nIndex = oMapFieldNameToIdxIter->second;
        retIndices.emplace_back(nIndex);
        // If there is a null value: do not update field definition.
        OGRFieldDefn *poFDefn = apoFieldDefn[nIndex].get();
        const OGRFieldType eType = poFDefn->GetType();
        const OGRFieldSubType eSubType = poFDefn->GetSubType();
        OGRFieldSubType eNewSubType;
        OGRFieldType eNewType =
            GeoJSONPropertyToFieldType(poVal, eNewSubType, bArrayAsString);
        const bool bNewIsEmptyArray =
            (jType == json_type_array && json_object_array_length(poVal) == 0);
        if (cpl::contains(aoSetUndeterminedTypeFields, nIndex))
        {
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(eNewType);
            if (poFDefn->GetType() == OFTString && !bDateAsString)
            {
                int nTZFlag = 0;
                poFDefn->SetType(
                    GeoJSONStringPropertyToFieldType(poVal, nTZFlag));
                poFDefn->SetTZFlag(nTZFlag);
            }
            poFDefn->SetSubType(eNewSubType);
            aoSetUndeterminedTypeFields.erase(nIndex);
        }
        else if (eType == OFTInteger)
        {
            if (eNewType == OFTInteger && eSubType == OFSTBoolean &&
                eNewSubType != OFSTBoolean)
            {
                poFDefn->SetSubType(OFSTNone);
            }
            else if (eNewType == OFTInteger64 || eNewType == OFTReal ||
                     eNewType == OFTInteger64List || eNewType == OFTRealList ||
                     eNewType == OFTStringList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if (eNewType == OFTIntegerList)
            {
                if (eSubType == OFSTBoolean && eNewSubType != OFSTBoolean)
                {
                    poFDefn->SetSubType(OFSTNone);
                }
                poFDefn->SetType(eNewType);
            }
            else if (eNewType != OFTInteger)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
                poFDefn->SetSubType(OFSTJSON);
            }
        }
        else if (eType == OFTInteger64)
        {
            if (eNewType == OFTReal)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if (eNewType == OFTIntegerList || eNewType == OFTInteger64List)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTInteger64List);
            }
            else if (eNewType == OFTRealList || eNewType == OFTStringList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if (eNewType != OFTInteger && eNewType != OFTInteger64)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
                poFDefn->SetSubType(OFSTJSON);
            }
        }
        else if (eType == OFTReal)
        {
            if (eNewType == OFTIntegerList || eNewType == OFTInteger64List ||
                eNewType == OFTRealList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTRealList);
            }
            else if (eNewType == OFTStringList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTStringList);
            }
            else if (eNewType != OFTInteger && eNewType != OFTInteger64 &&
                     eNewType != OFTReal)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
                poFDefn->SetSubType(OFSTJSON);
            }
        }
        else if (eType == OFTString)
        {
            if (eSubType == OFSTNone)
            {
                if (eNewType == OFTStringList)
                {
                    poFDefn->SetType(OFTStringList);
                }
                else if (eNewType != OFTString)
                {
                    poFDefn->SetSubType(OFSTJSON);
                }
            }
        }
        else if (eType == OFTIntegerList)
        {
            if (eNewType == OFTString)
            {
                if (!bNewIsEmptyArray)
                {
                    poFDefn->SetSubType(OFSTNone);
                    poFDefn->SetType(eNewType);
                    poFDefn->SetSubType(OFSTJSON);
                }
            }
            else if (eNewType == OFTInteger64List || eNewType == OFTRealList ||
                     eNewType == OFTStringList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if (eNewType == OFTInteger64)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTInteger64List);
            }
            else if (eNewType == OFTReal)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTRealList);
            }
            else if (eNewType == OFTInteger || eNewType == OFTIntegerList)
            {
                if (eSubType == OFSTBoolean && eNewSubType != OFSTBoolean)
                {
                    poFDefn->SetSubType(OFSTNone);
                }
            }
            else
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
                poFDefn->SetSubType(OFSTJSON);
            }
        }
        else if (eType == OFTInteger64List)
        {
            if (eNewType == OFTString)
            {
                if (!bNewIsEmptyArray)
                {
                    poFDefn->SetSubType(OFSTNone);
                    poFDefn->SetType(eNewType);
                    poFDefn->SetSubType(OFSTJSON);
                }
            }
            else if (eNewType == OFTInteger64List || eNewType == OFTRealList ||
                     eNewType == OFTStringList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if (eNewType == OFTReal)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTRealList);
            }
            else if (eNewType != OFTInteger && eNewType != OFTInteger64 &&
                     eNewType != OFTIntegerList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
                poFDefn->SetSubType(OFSTJSON);
            }
        }
        else if (eType == OFTRealList)
        {
            if (eNewType == OFTString)
            {
                if (!bNewIsEmptyArray)
                {
                    poFDefn->SetSubType(OFSTNone);
                    poFDefn->SetType(eNewType);
                    poFDefn->SetSubType(OFSTJSON);
                }
            }
            else if (eNewType == OFTStringList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if (eNewType != OFTInteger && eNewType != OFTInteger64 &&
                     eNewType != OFTReal && eNewType != OFTIntegerList &&
                     eNewType != OFTInteger64List && eNewType != OFTRealList)
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
                poFDefn->SetSubType(OFSTJSON);
            }
        }
        else if (eType == OFTStringList)
        {
            if (eNewType == OFTString && eNewSubType == OFSTJSON)
            {
                if (!bNewIsEmptyArray)
                {
                    poFDefn->SetSubType(OFSTNone);
                    poFDefn->SetType(eNewType);
                    poFDefn->SetSubType(OFSTJSON);
                }
            }
        }
        else if (eType == OFTDate || eType == OFTTime || eType == OFTDateTime)
        {
            if (eNewType == OFTString && !bDateAsString &&
                eNewSubType == OFSTNone)
            {
                int nTZFlag = 0;
                eNewType = GeoJSONStringPropertyToFieldType(poVal, nTZFlag);
                if (poFDefn->GetTZFlag() > OGR_TZFLAG_UNKNOWN &&
                    nTZFlag != poFDefn->GetTZFlag())
                {
                    if (nTZFlag == OGR_TZFLAG_UNKNOWN)
                        poFDefn->SetTZFlag(OGR_TZFLAG_UNKNOWN);
                    else
                        poFDefn->SetTZFlag(OGR_TZFLAG_MIXED_TZ);
                }
            }
            if (eType != eNewType)
            {
                poFDefn->SetSubType(OFSTNone);
                if (eNewType == OFTString)
                {
                    poFDefn->SetType(eNewType);
                    poFDefn->SetSubType(eNewSubType);
                }
                else if (eType == OFTDate && eNewType == OFTDateTime)
                {
                    poFDefn->SetType(OFTDateTime);
                }
                else if (!(eType == OFTDateTime && eNewType == OFTDate))
                {
                    poFDefn->SetType(OFTString);
                    poFDefn->SetSubType(OFSTJSON);
                }
            }
        }

        poFDefn->SetWidth(poFDefn->GetSubType() == OFSTBoolean ? 1 : 0);
    }
    else
    {
        const int nIndex = oMapFieldNameToIdxIter->second;
        retIndices.emplace_back(nIndex);
    }
}

/************************************************************************/
/*             OGRGeoJSONGenerateFeatureDefnDealWithID()                */
/************************************************************************/

void OGRGeoJSONGenerateFeatureDefnDealWithID(
    json_object *poObj, json_object *poObjProps, int &nPrevFieldIdx,
    std::map<std::string, int> &oMapFieldNameToIdx,
    std::vector<std::unique_ptr<OGRFieldDefn>> &apoFieldDefn,
    gdal::DirectedAcyclicGraph<int, std::string> &dag,
    bool &bFeatureLevelIdAsFID, bool &bFeatureLevelIdAsAttribute,
    bool &bNeedFID64)
{
    json_object *poObjId = OGRGeoJSONFindMemberByName(poObj, "id");
    if (poObjId)
    {
        const auto iterIdxId = oMapFieldNameToIdx.find("id");
        if (iterIdxId == oMapFieldNameToIdx.end())
        {
            if (json_object_get_type(poObjId) == json_type_int)
            {
                // If the value is negative, we cannot use it as the FID
                // as OGRMemLayer doesn't support negative FID. And we would
                // have an ambiguity with -1 that can mean OGRNullFID
                // so in that case create a regular attribute and let OGR
                // attribute sequential OGR FIDs.
                if (json_object_get_int64(poObjId) < 0)
                {
                    bFeatureLevelIdAsFID = false;
                }
                else
                {
                    bFeatureLevelIdAsFID = true;
                }
            }
            if (!bFeatureLevelIdAsFID)
            {
                // If there's a top-level id of type string or negative int,
                // and no properties.id, then declare a id field.
                bool bHasRegularIdProp = false;
                if (nullptr != poObjProps &&
                    json_object_get_type(poObjProps) == json_type_object)
                {
                    bHasRegularIdProp =
                        CPL_json_object_object_get(poObjProps, "id") != nullptr;
                }
                if (!bHasRegularIdProp)
                {
                    OGRFieldType eType = OFTString;
                    if (json_object_get_type(poObjId) == json_type_int)
                    {
                        if (CPL_INT64_FITS_ON_INT32(
                                json_object_get_int64(poObjId)))
                            eType = OFTInteger;
                        else
                            eType = OFTInteger64;
                    }
                    apoFieldDefn.emplace_back(
                        std::make_unique<OGRFieldDefn>("id", eType));
                    const int nIdx = static_cast<int>(apoFieldDefn.size()) - 1;
                    oMapFieldNameToIdx["id"] = nIdx;
                    nPrevFieldIdx = nIdx;
                    dag.addNode(nIdx, "id");
                    bFeatureLevelIdAsAttribute = true;
                }
            }
        }
        else
        {
            const int nIdx = iterIdxId->second;
            nPrevFieldIdx = nIdx;
            if (bFeatureLevelIdAsAttribute &&
                json_object_get_type(poObjId) == json_type_int)
            {
                if (apoFieldDefn[nIdx]->GetType() == OFTInteger)
                {
                    if (!CPL_INT64_FITS_ON_INT32(
                            json_object_get_int64(poObjId)))
                        apoFieldDefn[nIdx]->SetType(OFTInteger64);
                }
            }
            else if (bFeatureLevelIdAsAttribute)
            {
                apoFieldDefn[nIdx]->SetType(OFTString);
            }
        }
    }

    if (!bNeedFID64)
    {
        json_object *poId = CPL_json_object_object_get(poObj, "id");
        if (poId == nullptr)
        {
            if (poObjProps &&
                json_object_get_type(poObjProps) == json_type_object)
            {
                poId = CPL_json_object_object_get(poObjProps, "id");
            }
        }
        if (poId != nullptr && json_object_get_type(poId) == json_type_int)
        {
            GIntBig nFID = json_object_get_int64(poId);
            if (!CPL_INT64_FITS_ON_INT32(nFID))
            {
                bNeedFID64 = true;
            }
        }
    }
}

/************************************************************************/
/*                        GenerateFeatureDefn()                         */
/************************************************************************/
bool OGRGeoJSONBaseReader::GenerateFeatureDefn(
    std::map<std::string, int> &oMapFieldNameToIdx,
    std::vector<std::unique_ptr<OGRFieldDefn>> &apoFieldDefn,
    gdal::DirectedAcyclicGraph<int, std::string> &dag, OGRLayer *poLayer,
    json_object *poObj)
{
    /* -------------------------------------------------------------------- */
    /*      Read collection of properties.                                  */
    /* -------------------------------------------------------------------- */
    lh_entry *poObjPropsEntry =
        OGRGeoJSONFindMemberEntryByName(poObj, "properties");
    json_object *poObjProps =
        const_cast<json_object *>(static_cast<const json_object *>(
            poObjPropsEntry ? poObjPropsEntry->v : nullptr));

    std::vector<int> anCurFieldIndices;
    int nPrevFieldIdx = -1;

    OGRGeoJSONGenerateFeatureDefnDealWithID(
        poObj, poObjProps, nPrevFieldIdx, oMapFieldNameToIdx, apoFieldDefn, dag,
        bFeatureLevelIdAsFID_, bFeatureLevelIdAsAttribute_, m_bNeedFID64);

    json_object *poGeomObj = CPL_json_object_object_get(poObj, "geometry");
    if (poGeomObj && json_object_get_type(poGeomObj) == json_type_object)
    {
        const auto eType = OGRGeoJSONGetOGRGeometryType(poGeomObj);

        OGRGeoJSONUpdateLayerGeomType(m_bFirstGeometry, eType,
                                      m_eLayerGeomType);

        if (eType != wkbNone && eType != wkbUnknown)
        {
            // This is maybe too optimistic: it assumes that the geometry
            // coordinates array is in the correct format
            m_bExtentRead |= OGRGeoJSONGetExtent3D(poGeomObj, &m_oEnvelope3D);
        }
    }

    bool bSuccess = false;

    if (nullptr != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object)
    {
        if (bIsGeocouchSpatiallistFormat)
        {
            poObjProps = CPL_json_object_object_get(poObjProps, "properties");
            if (nullptr == poObjProps ||
                json_object_get_type(poObjProps) != json_type_object)
            {
                return true;
            }
        }

        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poObjProps, it)
        {
            if (!bIsGeocouchSpatiallistFormat &&
                !cpl::contains(oMapFieldNameToIdx, it.key))
            {
                // Detect the special kind of GeoJSON output by a spatiallist of
                // GeoCouch such as:
                // http://gd.iriscouch.com/cphosm/_design/geo/_rewrite/data?bbox=12.53%2C55.73%2C12.54%2C55.73
                if (strcmp(it.key, "_id") == 0)
                {
                    bFoundGeocouchId = true;
                }
                else if (bFoundGeocouchId && strcmp(it.key, "_rev") == 0)
                {
                    bFoundRev = true;
                }
                else if (bFoundRev && strcmp(it.key, "type") == 0 &&
                         it.val != nullptr &&
                         json_object_get_type(it.val) == json_type_string &&
                         strcmp(json_object_get_string(it.val), "Feature") == 0)
                {
                    bFoundTypeFeature = true;
                }
                else if (bFoundTypeFeature &&
                         strcmp(it.key, "properties") == 0 &&
                         it.val != nullptr &&
                         json_object_get_type(it.val) == json_type_object)
                {
                    if (bFlattenGeocouchSpatiallistFormat < 0)
                        bFlattenGeocouchSpatiallistFormat =
                            CPLTestBool(CPLGetConfigOption(
                                "GEOJSON_FLATTEN_GEOCOUCH", "TRUE"));
                    if (bFlattenGeocouchSpatiallistFormat)
                    {
                        const auto typeIter = oMapFieldNameToIdx.find("type");
                        if (typeIter != oMapFieldNameToIdx.end())
                        {
                            const int nIdx = typeIter->second;
                            apoFieldDefn.erase(apoFieldDefn.begin() + nIdx);
                            oMapFieldNameToIdx.erase(typeIter);
                            dag.removeNode(nIdx);
                        }

                        bIsGeocouchSpatiallistFormat = true;
                        return GenerateFeatureDefn(oMapFieldNameToIdx,
                                                   apoFieldDefn, dag, poLayer,
                                                   poObj);
                    }
                }
            }

            anCurFieldIndices.clear();
            OGRGeoJSONReaderAddOrUpdateField(
                anCurFieldIndices, oMapFieldNameToIdx, apoFieldDefn, it.key,
                it.val, bFlattenNestedAttributes_, chNestedAttributeSeparator_,
                bArrayAsString_, bDateAsString_, aoSetUndeterminedTypeFields_);
            for (int idx : anCurFieldIndices)
            {
                dag.addNode(idx, apoFieldDefn[idx]->GetNameRef());
                if (nPrevFieldIdx != -1)
                {
                    dag.addEdge(nPrevFieldIdx, idx);
                }
                nPrevFieldIdx = idx;
            }
        }

        // Whether/how we should deal with foreign members
        if (eForeignMemberProcessing_ == ForeignMemberProcessing::AUTO)
        {
            if (CPL_json_object_object_get(poObj, "stac_version"))
                eForeignMemberProcessing_ = ForeignMemberProcessing::STAC;
            else
                eForeignMemberProcessing_ = ForeignMemberProcessing::NONE;
        }
        if (eForeignMemberProcessing_ != ForeignMemberProcessing::NONE)
        {
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
            json_object_object_foreachC(poObj, it)
            {
                if (eForeignMemberProcessing_ ==
                        ForeignMemberProcessing::STAC &&
                    strcmp(it.key, "assets") == 0 &&
                    json_object_get_type(it.val) == json_type_object)
                {
                    json_object_iter it2;
                    it2.key = nullptr;
                    it2.val = nullptr;
                    it2.entry = nullptr;
                    json_object_object_foreachC(it.val, it2)
                    {
                        if (json_object_get_type(it2.val) == json_type_object)
                        {
                            json_object_iter it3;
                            it3.key = nullptr;
                            it3.val = nullptr;
                            it3.entry = nullptr;
                            json_object_object_foreachC(it2.val, it3)
                            {
                                anCurFieldIndices.clear();
                                OGRGeoJSONReaderAddOrUpdateField(
                                    anCurFieldIndices, oMapFieldNameToIdx,
                                    apoFieldDefn,
                                    std::string("assets.")
                                        .append(it2.key)
                                        .append(".")
                                        .append(it3.key)
                                        .c_str(),
                                    it3.val, bFlattenNestedAttributes_,
                                    chNestedAttributeSeparator_,
                                    bArrayAsString_, bDateAsString_,
                                    aoSetUndeterminedTypeFields_);
                                for (int idx : anCurFieldIndices)
                                {
                                    dag.addNode(
                                        idx, apoFieldDefn[idx]->GetNameRef());
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
                else if (strcmp(it.key, "type") != 0 &&
                         strcmp(it.key, "id") != 0 &&
                         strcmp(it.key, "geometry") != 0 &&
                         strcmp(it.key, "bbox") != 0 &&
                         strcmp(it.key, "properties") != 0)
                {
                    anCurFieldIndices.clear();
                    OGRGeoJSONReaderAddOrUpdateField(
                        anCurFieldIndices, oMapFieldNameToIdx, apoFieldDefn,
                        it.key, it.val, bFlattenNestedAttributes_,
                        chNestedAttributeSeparator_, bArrayAsString_,
                        bDateAsString_, aoSetUndeterminedTypeFields_);
                    for (int idx : anCurFieldIndices)
                    {
                        dag.addNode(idx, apoFieldDefn[idx]->GetNameRef());
                        if (nPrevFieldIdx != -1)
                        {
                            dag.addEdge(nPrevFieldIdx, idx);
                        }
                        nPrevFieldIdx = idx;
                    }
                }
            }
        }

        bSuccess = true;  // SUCCESS
    }
    else if (nullptr != poObjPropsEntry &&
             (poObjProps == nullptr ||
              (json_object_get_type(poObjProps) == json_type_array &&
               json_object_array_length(poObjProps) == 0)))
    {
        // Ignore "properties": null and "properties": []
        bSuccess = true;
    }
    else if (poObj != nullptr &&
             json_object_get_type(poObj) == json_type_object)
    {
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poObj, it)
        {
            if (strcmp(it.key, "type") != 0 &&
                strcmp(it.key, "geometry") != 0 &&
                strcmp(it.key, "centroid") != 0 &&
                strcmp(it.key, "bbox") != 0 && strcmp(it.key, "center") != 0)
            {
                if (!cpl::contains(oMapFieldNameToIdx, it.key))
                {
                    anCurFieldIndices.clear();
                    OGRGeoJSONReaderAddOrUpdateField(
                        anCurFieldIndices, oMapFieldNameToIdx, apoFieldDefn,
                        it.key, it.val, bFlattenNestedAttributes_,
                        chNestedAttributeSeparator_, bArrayAsString_,
                        bDateAsString_, aoSetUndeterminedTypeFields_);
                    for (int idx : anCurFieldIndices)
                    {
                        dag.addNode(idx, apoFieldDefn[idx]->GetNameRef());
                        if (nPrevFieldIdx != -1)
                        {
                            dag.addEdge(nPrevFieldIdx, idx);
                        }
                        nPrevFieldIdx = idx;
                    }
                }
            }
        }

        bSuccess = true;  // SUCCESS
        // CPLError(CE_Failure, CPLE_AppDefined,
        //          "Invalid Feature object. "
        //          "Missing \'properties\' member." );
    }

    return bSuccess;
}

/************************************************************************/
/*                   OGRGeoJSONUpdateLayerGeomType()                    */
/************************************************************************/

bool OGRGeoJSONUpdateLayerGeomType(bool &bFirstGeom,
                                   OGRwkbGeometryType eGeomType,
                                   OGRwkbGeometryType &eLayerGeomType)
{
    if (bFirstGeom)
    {
        eLayerGeomType = eGeomType;
        bFirstGeom = false;
    }
    else if (OGR_GT_HasZ(eGeomType) && !OGR_GT_HasZ(eLayerGeomType) &&
             wkbFlatten(eGeomType) == wkbFlatten(eLayerGeomType))
    {
        eLayerGeomType = eGeomType;
    }
    else if (!OGR_GT_HasZ(eGeomType) && OGR_GT_HasZ(eLayerGeomType) &&
             wkbFlatten(eGeomType) == wkbFlatten(eLayerGeomType))
    {
        // ok
    }
    else if (eGeomType != eLayerGeomType && eLayerGeomType != wkbUnknown)
    {
        CPLDebug("GeoJSON", "Detected layer of mixed-geometry type features.");
        eLayerGeomType = wkbUnknown;
        return false;
    }
    return true;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRGeoJSONReader::AddFeature(OGRGeoJSONLayer *poLayer,
                                  OGRGeometry *poGeometry)
{
    bool bAdded = false;

    // TODO: Should we check if geometry is of type of wkbGeometryCollection?

    if (nullptr != poGeometry)
    {
        OGRFeature *poFeature = new OGRFeature(poLayer->GetLayerDefn());
        poFeature->SetGeometryDirectly(poGeometry);

        bAdded = AddFeature(poLayer, poFeature);
    }

    return bAdded;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRGeoJSONReader::AddFeature(OGRGeoJSONLayer *poLayer,
                                  OGRFeature *poFeature)
{
    if (poFeature == nullptr)
        return false;

    poLayer->AddFeature(poFeature);
    delete poFeature;

    return true;
}

/************************************************************************/
/*                           ReadGeometry                               */
/************************************************************************/

OGRGeometry *OGRGeoJSONBaseReader::ReadGeometry(json_object *poObj,
                                                OGRSpatialReference *poLayerSRS)
{
    OGRGeometry *poGeometry = OGRGeoJSONReadGeometry(poObj, poLayerSRS);

    /* -------------------------------------------------------------------- */
    /*      Wrap geometry with GeometryCollection as a common denominator.  */
    /*      Sometimes a GeoJSON text may consist of objects of different    */
    /*      geometry types. Users may request wrapping all geometries with  */
    /*      OGRGeometryCollection type by using option                      */
    /*      GEOMETRY_AS_COLLECTION=NO|YES (NO is default).                  */
    /* -------------------------------------------------------------------- */
    if (nullptr != poGeometry)
    {
        if (!bGeometryPreserve_ &&
            wkbGeometryCollection != poGeometry->getGeometryType())
        {
            OGRGeometryCollection *poMetaGeometry = new OGRGeometryCollection();
            poMetaGeometry->addGeometryDirectly(poGeometry);
            return poMetaGeometry;
        }
    }

    return poGeometry;
}

/************************************************************************/
/*                OGRGeoJSONReaderSetFieldNestedAttribute()             */
/************************************************************************/

static void OGRGeoJSONReaderSetFieldNestedAttribute(OGRLayer *poLayer,
                                                    OGRFeature *poFeature,
                                                    const char *pszAttrPrefix,
                                                    char chSeparator,
                                                    json_object *poVal)
{
    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    json_object_object_foreachC(poVal, it)
    {
        const char szSeparator[2] = {chSeparator, '\0'};
        const CPLString osAttrName(
            CPLSPrintf("%s%s%s", pszAttrPrefix, szSeparator, it.key));
        if (it.val != nullptr &&
            json_object_get_type(it.val) == json_type_object)
        {
            OGRGeoJSONReaderSetFieldNestedAttribute(
                poLayer, poFeature, osAttrName, chSeparator, it.val);
        }
        else
        {
            const int nField =
                poFeature->GetDefnRef()->GetFieldIndexCaseSensitive(osAttrName);
            OGRGeoJSONReaderSetField(poLayer, poFeature, nField, osAttrName,
                                     it.val, false, 0);
        }
    }
}

/************************************************************************/
/*                   OGRGeoJSONReaderSetField()                         */
/************************************************************************/

void OGRGeoJSONReaderSetField(OGRLayer *poLayer, OGRFeature *poFeature,
                              int nField, const char *pszAttrPrefix,
                              json_object *poVal, bool bFlattenNestedAttributes,
                              char chNestedAttributeSeparator)
{
    if (bFlattenNestedAttributes && poVal != nullptr &&
        json_object_get_type(poVal) == json_type_object)
    {
        OGRGeoJSONReaderSetFieldNestedAttribute(
            poLayer, poFeature, pszAttrPrefix, chNestedAttributeSeparator,
            poVal);
        return;
    }
    if (nField < 0)
        return;

    OGRFieldDefn *poFieldDefn = poFeature->GetFieldDefnRef(nField);
    CPLAssert(nullptr != poFieldDefn);
    OGRFieldType eType = poFieldDefn->GetType();

    if (poVal == nullptr)
    {
        poFeature->SetFieldNull(nField);
    }
    else if (OFTInteger == eType)
    {
        poFeature->SetField(nField, json_object_get_int(poVal));

        // Check if FID available and set correct value.
        if (EQUAL(poFieldDefn->GetNameRef(), poLayer->GetFIDColumn()))
            poFeature->SetFID(json_object_get_int(poVal));
    }
    else if (OFTInteger64 == eType)
    {
        poFeature->SetField(nField, (GIntBig)json_object_get_int64(poVal));

        // Check if FID available and set correct value.
        if (EQUAL(poFieldDefn->GetNameRef(), poLayer->GetFIDColumn()))
            poFeature->SetFID(
                static_cast<GIntBig>(json_object_get_int64(poVal)));
    }
    else if (OFTReal == eType)
    {
        poFeature->SetField(nField, json_object_get_double(poVal));
    }
    else if (OFTIntegerList == eType)
    {
        const enum json_type eJSonType(json_object_get_type(poVal));
        if (eJSonType == json_type_array)
        {
            const auto nLength = json_object_array_length(poVal);
            int *panVal = static_cast<int *>(CPLMalloc(sizeof(int) * nLength));
            for (auto i = decltype(nLength){0}; i < nLength; i++)
            {
                json_object *poRow = json_object_array_get_idx(poVal, i);
                panVal[i] = json_object_get_int(poRow);
            }
            poFeature->SetField(nField, static_cast<int>(nLength), panVal);
            CPLFree(panVal);
        }
        else if (eJSonType == json_type_boolean || eJSonType == json_type_int)
        {
            poFeature->SetField(nField, json_object_get_int(poVal));
        }
    }
    else if (OFTInteger64List == eType)
    {
        const enum json_type eJSonType(json_object_get_type(poVal));
        if (eJSonType == json_type_array)
        {
            const auto nLength = json_object_array_length(poVal);
            GIntBig *panVal =
                static_cast<GIntBig *>(CPLMalloc(sizeof(GIntBig) * nLength));
            for (auto i = decltype(nLength){0}; i < nLength; i++)
            {
                json_object *poRow = json_object_array_get_idx(poVal, i);
                panVal[i] = static_cast<GIntBig>(json_object_get_int64(poRow));
            }
            poFeature->SetField(nField, static_cast<int>(nLength), panVal);
            CPLFree(panVal);
        }
        else if (eJSonType == json_type_boolean || eJSonType == json_type_int)
        {
            poFeature->SetField(
                nField, static_cast<GIntBig>(json_object_get_int64(poVal)));
        }
    }
    else if (OFTRealList == eType)
    {
        const enum json_type eJSonType(json_object_get_type(poVal));
        if (eJSonType == json_type_array)
        {
            const auto nLength = json_object_array_length(poVal);
            double *padfVal =
                static_cast<double *>(CPLMalloc(sizeof(double) * nLength));
            for (auto i = decltype(nLength){0}; i < nLength; i++)
            {
                json_object *poRow = json_object_array_get_idx(poVal, i);
                padfVal[i] = json_object_get_double(poRow);
            }
            poFeature->SetField(nField, static_cast<int>(nLength), padfVal);
            CPLFree(padfVal);
        }
        else if (eJSonType == json_type_boolean || eJSonType == json_type_int ||
                 eJSonType == json_type_double)
        {
            poFeature->SetField(nField, json_object_get_double(poVal));
        }
    }
    else if (OFTStringList == eType)
    {
        const enum json_type eJSonType(json_object_get_type(poVal));
        if (eJSonType == json_type_array)
        {
            auto nLength = json_object_array_length(poVal);
            char **papszVal =
                (char **)CPLMalloc(sizeof(char *) * (nLength + 1));
            decltype(nLength) i = 0;  // Used after for.
            for (; i < nLength; i++)
            {
                json_object *poRow = json_object_array_get_idx(poVal, i);
                const char *pszVal = json_object_get_string(poRow);
                if (pszVal == nullptr)
                    break;
                papszVal[i] = CPLStrdup(pszVal);
            }
            papszVal[i] = nullptr;
            poFeature->SetField(nField, papszVal);
            CSLDestroy(papszVal);
        }
        else
        {
            poFeature->SetField(nField, json_object_get_string(poVal));
        }
    }
    else
    {
        poFeature->SetField(nField, json_object_get_string(poVal));
    }
}

/************************************************************************/
/*                           ReadFeature()                              */
/************************************************************************/

OGRFeature *OGRGeoJSONBaseReader::ReadFeature(OGRLayer *poLayer,
                                              json_object *poObj,
                                              const char *pszSerializedObj)
{
    CPLAssert(nullptr != poObj);

    OGRFeatureDefn *poFDefn = poLayer->GetLayerDefn();
    OGRFeature *poFeature = new OGRFeature(poFDefn);

    if (bStoreNativeData_)
    {
        poFeature->SetNativeData(pszSerializedObj
                                     ? pszSerializedObj
                                     : json_object_to_json_string(poObj));
        poFeature->SetNativeMediaType("application/vnd.geo+json");
    }

    /* -------------------------------------------------------------------- */
    /*      Translate GeoJSON "properties" object to feature attributes.    */
    /* -------------------------------------------------------------------- */
    CPLAssert(nullptr != poFeature);

    json_object *poObjProps = OGRGeoJSONFindMemberByName(poObj, "properties");
    if (!bAttributesSkip_ && nullptr != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object)
    {
        if (bIsGeocouchSpatiallistFormat)
        {
            json_object *poId = CPL_json_object_object_get(poObjProps, "_id");
            if (poId != nullptr &&
                json_object_get_type(poId) == json_type_string)
                poFeature->SetField("_id", json_object_get_string(poId));

            json_object *poRev = CPL_json_object_object_get(poObjProps, "_rev");
            if (poRev != nullptr &&
                json_object_get_type(poRev) == json_type_string)
            {
                poFeature->SetField("_rev", json_object_get_string(poRev));
            }

            poObjProps = CPL_json_object_object_get(poObjProps, "properties");
            if (nullptr == poObjProps ||
                json_object_get_type(poObjProps) != json_type_object)
            {
                return poFeature;
            }
        }

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
                CPLDebug("GeoJSON", "Cannot find field %s", it.key);
            }
            else
            {
                OGRGeoJSONReaderSetField(poLayer, poFeature, nField, it.key,
                                         it.val, bFlattenNestedAttributes_,
                                         chNestedAttributeSeparator_);
            }
        }
    }

    // Whether/how we should deal with foreign members
    if (!bAttributesSkip_ &&
        eForeignMemberProcessing_ != ForeignMemberProcessing::NONE)
    {
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poObj, it)
        {
            if (eForeignMemberProcessing_ == ForeignMemberProcessing::STAC &&
                strcmp(it.key, "assets") == 0 &&
                json_object_get_type(it.val) == json_type_object)
            {
                json_object_iter it2;
                it2.key = nullptr;
                it2.val = nullptr;
                it2.entry = nullptr;
                json_object_object_foreachC(it.val, it2)
                {
                    if (json_object_get_type(it2.val) == json_type_object)
                    {
                        json_object_iter it3;
                        it3.key = nullptr;
                        it3.val = nullptr;
                        it3.entry = nullptr;
                        json_object_object_foreachC(it2.val, it3)
                        {
                            const std::string osFieldName =
                                std::string("assets.")
                                    .append(it2.key)
                                    .append(".")
                                    .append(it3.key)
                                    .c_str();
                            const int nField =
                                poFDefn->GetFieldIndexCaseSensitive(
                                    osFieldName.c_str());
                            if (nField < 0 && !(bFlattenNestedAttributes_ &&
                                                it3.val != nullptr &&
                                                json_object_get_type(it3.val) ==
                                                    json_type_object))
                            {
                                CPLDebug("GeoJSON", "Cannot find field %s",
                                         osFieldName.c_str());
                            }
                            else
                            {
                                OGRGeoJSONReaderSetField(
                                    poLayer, poFeature, nField,
                                    osFieldName.c_str(), it3.val,
                                    bFlattenNestedAttributes_,
                                    chNestedAttributeSeparator_);
                            }
                        }
                    }
                }
            }
            else if (strcmp(it.key, "type") != 0 && strcmp(it.key, "id") != 0 &&
                     strcmp(it.key, "geometry") != 0 &&
                     strcmp(it.key, "bbox") != 0 &&
                     strcmp(it.key, "properties") != 0)
            {
                const int nField = poFDefn->GetFieldIndexCaseSensitive(it.key);
                if (nField < 0 &&
                    !(bFlattenNestedAttributes_ && it.val != nullptr &&
                      json_object_get_type(it.val) == json_type_object))
                {
                    CPLDebug("GeoJSON", "Cannot find field %s", it.key);
                }
                else
                {
                    OGRGeoJSONReaderSetField(poLayer, poFeature, nField, it.key,
                                             it.val, bFlattenNestedAttributes_,
                                             chNestedAttributeSeparator_);
                }
            }
        }
    }

    if (!bAttributesSkip_ && nullptr == poObjProps)
    {
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poObj, it)
        {
            const int nFldIndex = poFDefn->GetFieldIndexCaseSensitive(it.key);
            if (nFldIndex >= 0)
            {
                if (it.val)
                    poFeature->SetField(nFldIndex,
                                        json_object_get_string(it.val));
                else
                    poFeature->SetFieldNull(nFldIndex);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to use feature-level ID if available                        */
    /*      and of integral type. Otherwise, leave unset (-1) then index    */
    /*      in features sequence will be used as FID.                       */
    /* -------------------------------------------------------------------- */
    json_object *poObjId = OGRGeoJSONFindMemberByName(poObj, "id");
    if (nullptr != poObjId && bFeatureLevelIdAsFID_)
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
    /*      Translate geometry sub-object of GeoJSON Feature.               */
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
            // Done.  They had 'geometry':null.
            else
                return poFeature;
        }
    }

    if (nullptr != poObjGeom)
    {
        // NOTE: If geometry can not be parsed or read correctly
        //       then NULL geometry is assigned to a feature and
        //       geometry type for layer is classified as wkbUnknown.
        OGRGeometry *poGeometry =
            ReadGeometry(poObjGeom, poLayer->GetSpatialRef());
        if (nullptr != poGeometry)
        {
            poFeature->SetGeometryDirectly(poGeometry);
        }
    }
    else
    {
        static bool bWarned = false;
        if (!bWarned)
        {
            bWarned = true;
            CPLDebug(
                "GeoJSON",
                "Non conformant Feature object. Missing \'geometry\' member.");
        }
    }

    return poFeature;
}

/************************************************************************/
/*                           Extent getters                             */
/************************************************************************/

bool OGRGeoJSONBaseReader::ExtentRead() const
{
    return m_bExtentRead;
}

OGREnvelope3D OGRGeoJSONBaseReader::GetExtent3D() const
{
    return m_oEnvelope3D;
}

/************************************************************************/
/*                           ReadFeatureCollection()                    */
/************************************************************************/

void OGRGeoJSONReader::ReadFeatureCollection(OGRGeoJSONLayer *poLayer,
                                             json_object *poObj)
{
    json_object *poObjFeatures = OGRGeoJSONFindMemberByName(poObj, "features");
    if (nullptr == poObjFeatures)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid FeatureCollection object. "
                 "Missing \'features\' member.");
        return;
    }

    if (json_type_array == json_object_get_type(poObjFeatures))
    {
        const auto nFeatures = json_object_array_length(poObjFeatures);
        for (auto i = decltype(nFeatures){0}; i < nFeatures; ++i)
        {
            json_object *poObjFeature =
                json_object_array_get_idx(poObjFeatures, i);
            OGRFeature *poFeature = ReadFeature(poLayer, poObjFeature, nullptr);
            AddFeature(poLayer, poFeature);
        }
    }

    // Collect top objects except 'type' and the 'features' array.
    if (bStoreNativeData_)
    {
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        CPLString osNativeData;
        json_object_object_foreachC(poObj, it)
        {
            if (strcmp(it.key, "type") == 0 || strcmp(it.key, "features") == 0)
            {
                continue;
            }
            if (osNativeData.empty())
                osNativeData = "{ ";
            else
                osNativeData += ", ";
            json_object *poKey = json_object_new_string(it.key);
            osNativeData += json_object_to_json_string(poKey);
            json_object_put(poKey);
            osNativeData += ": ";
            osNativeData += json_object_to_json_string(it.val);
        }
        if (osNativeData.empty())
        {
            osNativeData = "{ ";
        }
        osNativeData += " }";

        osNativeData = "NATIVE_DATA=" + osNativeData;

        char *apszMetadata[3] = {
            const_cast<char *>(osNativeData.c_str()),
            const_cast<char *>("NATIVE_MEDIA_TYPE=application/vnd.geo+json"),
            nullptr};

        poLayer->SetMetadata(apszMetadata, "NATIVE_DATA");
    }
}

/************************************************************************/
/*                          OGRGeoJSONGetExtent3D()                     */
/************************************************************************/

bool OGRGeoJSONGetExtent3D(json_object *poObj, OGREnvelope3D *poEnvelope)
{
    if (!poEnvelope || !poObj)
    {
        return false;
    }

    // poObjCoords can be an array of arrays, this lambda function will
    // recursively parse the array
    std::function<bool(json_object *, OGREnvelope3D *)> fParseCoords;
    fParseCoords = [&fParseCoords](json_object *poObjCoordsIn,
                                   OGREnvelope3D *poEnvelopeIn) -> bool
    {
        if (json_type_array == json_object_get_type(poObjCoordsIn))
        {
            const auto nItems = json_object_array_length(poObjCoordsIn);

            double dXVal = std::numeric_limits<double>::quiet_NaN();
            double dYVal = std::numeric_limits<double>::quiet_NaN();
            double dZVal = std::numeric_limits<double>::quiet_NaN();

            for (auto i = decltype(nItems){0}; i < nItems; ++i)
            {

                // Get the i element
                json_object *poObjCoordsElement =
                    json_object_array_get_idx(poObjCoordsIn, i);

                const json_type eType{json_object_get_type(poObjCoordsElement)};

                // if it is an array, recurse
                if (json_type_array == eType)
                {
                    if (!fParseCoords(poObjCoordsElement, poEnvelopeIn))
                    {
                        return false;
                    }
                }
                else if (json_type_double == eType || json_type_int == eType)
                {
                    switch (i)
                    {
                        case 0:
                        {
                            dXVal = json_object_get_double(poObjCoordsElement);
                            break;
                        }
                        case 1:
                        {
                            dYVal = json_object_get_double(poObjCoordsElement);
                            break;
                        }
                        case 2:
                        {
                            dZVal = json_object_get_double(poObjCoordsElement);
                            break;
                        }
                        default:
                            return false;
                    }
                }
                else
                {
                    return false;
                }
            }

            if (!std::isnan(dXVal) && !std::isnan(dYVal))
            {
                if (std::isnan(dZVal))
                {
                    static_cast<OGREnvelope *>(poEnvelopeIn)
                        ->Merge(dXVal, dYVal);
                }
                else
                {
                    poEnvelopeIn->Merge(dXVal, dYVal, dZVal);
                }
            }

            return true;
        }
        else
        {
            return false;
        }
    };

    // This function looks for "coordinates" and for "geometries" to handle
    // geometry collections.  It will recurse on itself to handle nested geometry.
    std::function<bool(json_object *, OGREnvelope3D *)> fParseGeometry;
    fParseGeometry = [&fParseGeometry,
                      &fParseCoords](json_object *poObjIn,
                                     OGREnvelope3D *poEnvelopeIn) -> bool
    {
        // Get the "coordinates" array from the JSON object
        json_object *poObjCoords =
            OGRGeoJSONFindMemberByName(poObjIn, "coordinates");

        // Return if found and not an array
        if (poObjCoords && json_object_get_type(poObjCoords) != json_type_array)
        {
            return false;
        }
        else if (poObjCoords)
        {
            return fParseCoords(poObjCoords, poEnvelopeIn);
        }

        // Try "geometries"
        if (!poObjCoords)
        {
            poObjCoords = OGRGeoJSONFindMemberByName(poObjIn, "geometries");
        }

        // Return if not found or not an array
        if (!poObjCoords ||
            json_object_get_type(poObjCoords) != json_type_array)
        {
            return false;
        }
        else
        {
            // Loop thgrough the geometries
            const auto nItems = json_object_array_length(poObjCoords);
            for (auto i = decltype(nItems){0}; i < nItems; ++i)
            {
                json_object *poObjGeometry =
                    json_object_array_get_idx(poObjCoords, i);

                // Recurse
                if (!fParseGeometry(poObjGeometry, poEnvelopeIn))
                {
                    return false;
                }
            }
            return true;
        }
    };

    return fParseGeometry(poObj, poEnvelope);
}
