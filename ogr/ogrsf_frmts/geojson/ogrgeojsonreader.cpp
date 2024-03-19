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

#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogr_geojson.h"
#include "ogrjsoncollectionstreamingparser.h"
#include "ogr_api.h"

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
                while (m_oSetUsedFIDs.find(nFID) != m_oSetUsedFIDs.end())
                {
                    ++nFID;
                }
            }
            else if (m_oSetUsedFIDs.find(nFID) != m_oSetUsedFIDs.end())
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
                while (m_oSetUsedFIDs.find(nFID) != m_oSetUsedFIDs.end())
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

    const char *pszName = poDS->GetDescription();
    if (STARTS_WITH_CI(pszName, "GeoJSON:"))
        pszName += strlen("GeoJSON:");
    pszName = CPLGetBasename(pszName);

    OGRGeoJSONLayer *poLayer = new OGRGeoJSONLayer(
        pszName, nullptr, OGRGeoJSONLayer::DefaultGeometryType, poDS, this);
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
        if (bFinished && bJSonPLikeWrapper_ && nRead - nSkip > 0)
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
        if (bFinished && bJSonPLikeWrapper_ && nRead - nSkip > 0)
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
            if (bFinished && bJSonPLikeWrapper_ && nRead - nSkip > 0)
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
                        if (oMapFIDToOffsetSize_.find(nThisFID) ==
                            oMapFIDToOffsetSize_.end())
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

    auto oIter = oMapFIDToOffsetSize_.find(nFID);
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
    if (pszName == nullptr)
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
        if (pszName == nullptr)
        {
            const char *pszDesc = poDS->GetDescription();
            if (strchr(pszDesc, '?') == nullptr &&
                strchr(pszDesc, '{') == nullptr)
            {
                pszName = CPLGetBasename(pszDesc);
            }
        }
        if (pszName == nullptr)
            pszName = OGRGeoJSONLayer::DefaultName;
    }

    OGRGeoJSONLayer *poLayer = new OGRGeoJSONLayer(
        pszName, nullptr, OGRGeoJSONLayer::DefaultGeometryType, poDS, nullptr);

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
    }

    if (CPLGetLastErrorType() != CE_Warning)
        CPLErrorReset();

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
/*                    OGRGeoJSONReadSpatialReference                    */
/************************************************************************/

OGRSpatialReference *OGRGeoJSONReadSpatialReference(json_object *poObj)
{

    /* -------------------------------------------------------------------- */
    /*      Read spatial reference definition.                              */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS = nullptr;

    json_object *poObjSrs = OGRGeoJSONFindMemberByName(poObj, "crs");
    if (nullptr != poObjSrs)
    {
        json_object *poObjSrsType =
            OGRGeoJSONFindMemberByName(poObjSrs, "type");
        if (poObjSrsType == nullptr)
            return nullptr;

        const char *pszSrsType = json_object_get_string(poObjSrsType);

        // TODO: Add URL and URN types support.
        if (STARTS_WITH_CI(pszSrsType, "NAME"))
        {
            json_object *poObjSrsProps =
                OGRGeoJSONFindMemberByName(poObjSrs, "properties");
            if (poObjSrsProps == nullptr)
                return nullptr;

            json_object *poNameURL =
                OGRGeoJSONFindMemberByName(poObjSrsProps, "name");
            if (poNameURL == nullptr)
                return nullptr;

            const char *pszName = json_object_get_string(poNameURL);

            // Mostly to emulate GDAL 2.x behavior
            // See https://github.com/OSGeo/gdal/issues/2035
            if (EQUAL(pszName, "urn:ogc:def:crs:OGC:1.3:CRS84"))
                pszName = "EPSG:4326";

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE !=
                poSRS->SetFromUserInput(
                    pszName,
                    OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()))
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }

        else if (STARTS_WITH_CI(pszSrsType, "EPSG"))
        {
            json_object *poObjSrsProps =
                OGRGeoJSONFindMemberByName(poObjSrs, "properties");
            if (poObjSrsProps == nullptr)
                return nullptr;

            json_object *poObjCode =
                OGRGeoJSONFindMemberByName(poObjSrsProps, "code");
            if (poObjCode == nullptr)
                return nullptr;

            int nEPSG = json_object_get_int(poObjCode);

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE != poSRS->importFromEPSG(nEPSG))
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }

        else if (STARTS_WITH_CI(pszSrsType, "URL") ||
                 STARTS_WITH_CI(pszSrsType, "LINK"))
        {
            json_object *poObjSrsProps =
                OGRGeoJSONFindMemberByName(poObjSrs, "properties");
            if (poObjSrsProps == nullptr)
                return nullptr;

            json_object *poObjURL =
                OGRGeoJSONFindMemberByName(poObjSrsProps, "url");

            if (nullptr == poObjURL)
            {
                poObjURL = OGRGeoJSONFindMemberByName(poObjSrsProps, "href");
            }
            if (poObjURL == nullptr)
                return nullptr;

            const char *pszURL = json_object_get_string(poObjURL);

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE != poSRS->importFromUrl(pszURL))
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }

        else if (EQUAL(pszSrsType, "OGC"))
        {
            json_object *poObjSrsProps =
                OGRGeoJSONFindMemberByName(poObjSrs, "properties");
            if (poObjSrsProps == nullptr)
                return nullptr;

            json_object *poObjURN =
                OGRGeoJSONFindMemberByName(poObjSrsProps, "urn");
            if (poObjURN == nullptr)
                return nullptr;

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (OGRERR_NONE !=
                poSRS->importFromURN(json_object_get_string(poObjURN)))
            {
                delete poSRS;
                poSRS = nullptr;
            }
        }
    }

    // Strip AXIS, since geojson has (easting, northing) / (longitude, latitude)
    // order.  According to http://www.geojson.org/geojson-spec.html#id2 :
    // "Point coordinates are in x, y order (easting, northing for projected
    // coordinates, longitude, latitude for geographic coordinates)".
    if (poSRS != nullptr)
    {
        OGR_SRSNode *poGEOGCS = poSRS->GetAttrNode("GEOGCS");
        if (poGEOGCS != nullptr)
            poGEOGCS->StripNodes("AXIS");
    }

    return poSRS;
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

    auto oMapFieldNameToIdxIter = oMapFieldNameToIdx.find(pszKey);
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
        if (aoSetUndeterminedTypeFields.find(nIndex) !=
            aoSetUndeterminedTypeFields.end())
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
        auto iterIdxId = oMapFieldNameToIdx.find("id");
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
                oMapFieldNameToIdx.find(it.key) == oMapFieldNameToIdx.end())
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
                        auto typeIter = oMapFieldNameToIdx.find("type");
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
                if (oMapFieldNameToIdx.find(it.key) == oMapFieldNameToIdx.end())
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
    else if (eGeomType != eLayerGeomType)
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
/*                           OGRGeoJSONFindMemberByName                 */
/************************************************************************/

lh_entry *OGRGeoJSONFindMemberEntryByName(json_object *poObj,
                                          const char *pszName)
{
    if (nullptr == pszName || nullptr == poObj)
        return nullptr;

    if (nullptr != json_object_get_object(poObj))
    {
        lh_entry *entry = json_object_get_object(poObj)->head;
        while (entry != nullptr)
        {
            if (EQUAL(static_cast<const char *>(entry->k), pszName))
                return entry;
            entry = entry->next;
        }
    }

    return nullptr;
}

json_object *OGRGeoJSONFindMemberByName(json_object *poObj, const char *pszName)
{
    lh_entry *entry = OGRGeoJSONFindMemberEntryByName(poObj, pszName);
    if (nullptr == entry)
        return nullptr;
    return (json_object *)entry->v;
}

/************************************************************************/
/*                           OGRGeoJSONGetType                          */
/************************************************************************/

GeoJSONObject::Type OGRGeoJSONGetType(json_object *poObj)
{
    if (nullptr == poObj)
        return GeoJSONObject::eUnknown;

    json_object *poObjType = OGRGeoJSONFindMemberByName(poObj, "type");
    if (nullptr == poObjType)
        return GeoJSONObject::eUnknown;

    const char *name = json_object_get_string(poObjType);
    if (EQUAL(name, "Point"))
        return GeoJSONObject::ePoint;
    else if (EQUAL(name, "LineString"))
        return GeoJSONObject::eLineString;
    else if (EQUAL(name, "Polygon"))
        return GeoJSONObject::ePolygon;
    else if (EQUAL(name, "MultiPoint"))
        return GeoJSONObject::eMultiPoint;
    else if (EQUAL(name, "MultiLineString"))
        return GeoJSONObject::eMultiLineString;
    else if (EQUAL(name, "MultiPolygon"))
        return GeoJSONObject::eMultiPolygon;
    else if (EQUAL(name, "GeometryCollection"))
        return GeoJSONObject::eGeometryCollection;
    else if (EQUAL(name, "Feature"))
        return GeoJSONObject::eFeature;
    else if (EQUAL(name, "FeatureCollection"))
        return GeoJSONObject::eFeatureCollection;
    else
        return GeoJSONObject::eUnknown;
}

/************************************************************************/
/*                   OGRGeoJSONGetOGRGeometryType()                     */
/************************************************************************/

OGRwkbGeometryType OGRGeoJSONGetOGRGeometryType(json_object *poObj)
{
    if (nullptr == poObj)
        return wkbUnknown;

    json_object *poObjType = CPL_json_object_object_get(poObj, "type");
    if (nullptr == poObjType)
        return wkbUnknown;

    OGRwkbGeometryType eType = wkbUnknown;
    const char *name = json_object_get_string(poObjType);
    if (EQUAL(name, "Point"))
        eType = wkbPoint;
    else if (EQUAL(name, "LineString"))
        eType = wkbLineString;
    else if (EQUAL(name, "Polygon"))
        eType = wkbPolygon;
    else if (EQUAL(name, "MultiPoint"))
        eType = wkbMultiPoint;
    else if (EQUAL(name, "MultiLineString"))
        eType = wkbMultiLineString;
    else if (EQUAL(name, "MultiPolygon"))
        eType = wkbMultiPolygon;
    else if (EQUAL(name, "GeometryCollection"))
        eType = wkbGeometryCollection;
    else
        return wkbUnknown;

    json_object *poCoordinates;
    if (eType == wkbGeometryCollection)
    {
        json_object *poGeometries =
            CPL_json_object_object_get(poObj, "geometries");
        if (poGeometries &&
            json_object_get_type(poGeometries) == json_type_array &&
            json_object_array_length(poGeometries) > 0)
        {
            if (OGR_GT_HasZ(OGRGeoJSONGetOGRGeometryType(
                    json_object_array_get_idx(poGeometries, 0))))
                eType = OGR_GT_SetZ(eType);
        }
    }
    else
    {
        poCoordinates = CPL_json_object_object_get(poObj, "coordinates");
        if (poCoordinates &&
            json_object_get_type(poCoordinates) == json_type_array &&
            json_object_array_length(poCoordinates) > 0)
        {
            while (true)
            {
                auto poChild = json_object_array_get_idx(poCoordinates, 0);
                if (!(poChild &&
                      json_object_get_type(poChild) == json_type_array &&
                      json_object_array_length(poChild) > 0))
                {
                    if (json_object_array_length(poCoordinates) == 3)
                        eType = OGR_GT_SetZ(eType);
                    break;
                }
                poCoordinates = poChild;
            }
        }
    }

    return eType;
}

/************************************************************************/
/*                           OGRGeoJSONReadGeometry                     */
/************************************************************************/

OGRGeometry *OGRGeoJSONReadGeometry(json_object *poObj,
                                    OGRSpatialReference *poParentSRS)
{

    OGRGeometry *poGeometry = nullptr;
    OGRSpatialReference *poSRS = nullptr;
    lh_entry *entry = OGRGeoJSONFindMemberEntryByName(poObj, "crs");
    if (entry != nullptr)
    {
        json_object *poObjSrs = (json_object *)entry->v;
        if (poObjSrs != nullptr)
        {
            poSRS = OGRGeoJSONReadSpatialReference(poObj);
        }
    }

    OGRSpatialReference *poSRSToAssign = nullptr;
    if (entry != nullptr)
    {
        poSRSToAssign = poSRS;
    }
    else if (poParentSRS)
    {
        poSRSToAssign = poParentSRS;
    }
    else
    {
        // Assign WGS84 if no CRS defined on geometry.
        poSRSToAssign = OGRSpatialReference::GetWGS84SRS();
    }

    GeoJSONObject::Type objType = OGRGeoJSONGetType(poObj);
    if (GeoJSONObject::ePoint == objType)
        poGeometry = OGRGeoJSONReadPoint(poObj);
    else if (GeoJSONObject::eMultiPoint == objType)
        poGeometry = OGRGeoJSONReadMultiPoint(poObj);
    else if (GeoJSONObject::eLineString == objType)
        poGeometry = OGRGeoJSONReadLineString(poObj);
    else if (GeoJSONObject::eMultiLineString == objType)
        poGeometry = OGRGeoJSONReadMultiLineString(poObj);
    else if (GeoJSONObject::ePolygon == objType)
        poGeometry = OGRGeoJSONReadPolygon(poObj);
    else if (GeoJSONObject::eMultiPolygon == objType)
        poGeometry = OGRGeoJSONReadMultiPolygon(poObj);
    else if (GeoJSONObject::eGeometryCollection == objType)
        poGeometry = OGRGeoJSONReadGeometryCollection(poObj, poSRSToAssign);
    else
    {
        CPLDebug("GeoJSON", "Unsupported geometry type detected. "
                            "Feature gets NULL geometry assigned.");
    }

    if (poGeometry && GeoJSONObject::eGeometryCollection != objType)
        poGeometry->assignSpatialReference(poSRSToAssign);

    if (poSRS)
        poSRS->Release();

    return poGeometry;
}

/************************************************************************/
/*                        OGRGeoJSONGetCoordinate()                     */
/************************************************************************/

static double OGRGeoJSONGetCoordinate(json_object *poObj,
                                      const char *pszCoordName, int nIndex,
                                      bool &bValid)
{
    json_object *poObjCoord = json_object_array_get_idx(poObj, nIndex);
    if (nullptr == poObjCoord)
    {
        CPLDebug("GeoJSON", "Point: got null object for %s.", pszCoordName);
        bValid = false;
        return 0.0;
    }

    const int iType = json_object_get_type(poObjCoord);
    if (json_type_double != iType && json_type_int != iType)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid '%s' coordinate. "
                 "Type is not double or integer for \'%s\'.",
                 pszCoordName, json_object_to_json_string(poObjCoord));
        bValid = false;
        return 0.0;
    }

    return json_object_get_double(poObjCoord);
}

/************************************************************************/
/*                           OGRGeoJSONReadRawPoint                     */
/************************************************************************/

bool OGRGeoJSONReadRawPoint(json_object *poObj, OGRPoint &point)
{
    CPLAssert(nullptr != poObj);

    if (json_type_array == json_object_get_type(poObj))
    {
        const auto nSize = json_object_array_length(poObj);

        if (nSize < GeoJSONObject::eMinCoordinateDimension)
        {
            CPLDebug("GeoJSON", "Invalid coord dimension. "
                                "At least 2 dimensions must be present.");
            return false;
        }

        bool bValid = true;
        const double dfX = OGRGeoJSONGetCoordinate(poObj, "x", 0, bValid);
        const double dfY = OGRGeoJSONGetCoordinate(poObj, "y", 1, bValid);
        point.setX(dfX);
        point.setY(dfY);

        // Read Z coordinate.
        if (nSize >= GeoJSONObject::eMaxCoordinateDimension)
        {
            // Don't *expect* mixed-dimension geometries, although the
            // spec doesn't explicitly forbid this.
            const double dfZ = OGRGeoJSONGetCoordinate(poObj, "z", 2, bValid);
            point.setZ(dfZ);
        }
        else
        {
            point.flattenTo2D();
        }
        return bValid;
    }

    return false;
}

/************************************************************************/
/*                           OGRGeoJSONReadPoint                        */
/************************************************************************/

OGRPoint *OGRGeoJSONReadPoint(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjCoords = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if (nullptr == poObjCoords)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid Point object. Missing \'coordinates\' member.");
        return nullptr;
    }

    OGRPoint *poPoint = new OGRPoint();
    if (!OGRGeoJSONReadRawPoint(poObjCoords, *poPoint))
    {
        CPLDebug("GeoJSON", "Point: raw point parsing failure.");
        delete poPoint;
        return nullptr;
    }

    return poPoint;
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiPoint                   */
/************************************************************************/

OGRMultiPoint *OGRGeoJSONReadMultiPoint(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjPoints = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if (nullptr == poObjPoints)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MultiPoint object. "
                 "Missing \'coordinates\' member.");
        return nullptr;
    }

    OGRMultiPoint *poMultiPoint = nullptr;
    if (json_type_array == json_object_get_type(poObjPoints))
    {
        const auto nPoints = json_object_array_length(poObjPoints);

        poMultiPoint = new OGRMultiPoint();

        for (auto i = decltype(nPoints){0}; i < nPoints; ++i)
        {
            json_object *poObjCoords =
                json_object_array_get_idx(poObjPoints, i);

            OGRPoint pt;
            if (poObjCoords != nullptr &&
                !OGRGeoJSONReadRawPoint(poObjCoords, pt))
            {
                delete poMultiPoint;
                CPLDebug("GeoJSON", "LineString: raw point parsing failure.");
                return nullptr;
            }
            poMultiPoint->addGeometry(&pt);
        }
    }

    return poMultiPoint;
}

/************************************************************************/
/*                           OGRGeoJSONReadLineString                   */
/************************************************************************/

OGRLineString *OGRGeoJSONReadLineString(json_object *poObj, bool bRaw)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjPoints = nullptr;

    if (!bRaw)
    {
        poObjPoints = OGRGeoJSONFindMemberByName(poObj, "coordinates");
        if (nullptr == poObjPoints)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid LineString object. "
                     "Missing \'coordinates\' member.");
            return nullptr;
        }
    }
    else
    {
        poObjPoints = poObj;
    }

    OGRLineString *poLine = nullptr;

    if (json_type_array == json_object_get_type(poObjPoints))
    {
        const auto nPoints = json_object_array_length(poObjPoints);

        poLine = new OGRLineString();
        poLine->setNumPoints(static_cast<int>(nPoints));

        for (auto i = decltype(nPoints){0}; i < nPoints; ++i)
        {
            json_object *poObjCoords =
                json_object_array_get_idx(poObjPoints, i);
            if (poObjCoords == nullptr)
            {
                delete poLine;
                CPLDebug("GeoJSON", "LineString: got null object.");
                return nullptr;
            }

            OGRPoint pt;
            if (!OGRGeoJSONReadRawPoint(poObjCoords, pt))
            {
                delete poLine;
                CPLDebug("GeoJSON", "LineString: raw point parsing failure.");
                return nullptr;
            }
            if (pt.getCoordinateDimension() == 2)
            {
                poLine->setPoint(static_cast<int>(i), pt.getX(), pt.getY());
            }
            else
            {
                poLine->setPoint(static_cast<int>(i), pt.getX(), pt.getY(),
                                 pt.getZ());
            }
        }
    }

    return poLine;
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiLineString              */
/************************************************************************/

OGRMultiLineString *OGRGeoJSONReadMultiLineString(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjLines = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if (nullptr == poObjLines)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MultiLineString object. "
                 "Missing \'coordinates\' member.");
        return nullptr;
    }

    OGRMultiLineString *poMultiLine = nullptr;

    if (json_type_array == json_object_get_type(poObjLines))
    {
        const auto nLines = json_object_array_length(poObjLines);

        poMultiLine = new OGRMultiLineString();

        for (auto i = decltype(nLines){0}; i < nLines; ++i)
        {
            json_object *poObjLine = json_object_array_get_idx(poObjLines, i);

            OGRLineString *poLine;
            if (poObjLine != nullptr)
                poLine = OGRGeoJSONReadLineString(poObjLine, true);
            else
                poLine = new OGRLineString();

            if (nullptr != poLine)
            {
                poMultiLine->addGeometryDirectly(poLine);
            }
        }
    }

    return poMultiLine;
}

/************************************************************************/
/*                           OGRGeoJSONReadLinearRing                   */
/************************************************************************/

OGRLinearRing *OGRGeoJSONReadLinearRing(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    OGRLinearRing *poRing = nullptr;

    if (json_type_array == json_object_get_type(poObj))
    {
        const auto nPoints = json_object_array_length(poObj);

        poRing = new OGRLinearRing();
        poRing->setNumPoints(static_cast<int>(nPoints));

        for (auto i = decltype(nPoints){0}; i < nPoints; ++i)
        {
            json_object *poObjCoords = json_object_array_get_idx(poObj, i);
            if (poObjCoords == nullptr)
            {
                delete poRing;
                CPLDebug("GeoJSON", "LinearRing: got null object.");
                return nullptr;
            }

            OGRPoint pt;
            if (!OGRGeoJSONReadRawPoint(poObjCoords, pt))
            {
                delete poRing;
                CPLDebug("GeoJSON", "LinearRing: raw point parsing failure.");
                return nullptr;
            }

            if (2 == pt.getCoordinateDimension())
                poRing->setPoint(static_cast<int>(i), pt.getX(), pt.getY());
            else
                poRing->setPoint(static_cast<int>(i), pt.getX(), pt.getY(),
                                 pt.getZ());
        }
    }

    return poRing;
}

/************************************************************************/
/*                           OGRGeoJSONReadPolygon                      */
/************************************************************************/

OGRPolygon *OGRGeoJSONReadPolygon(json_object *poObj, bool bRaw)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjRings = nullptr;

    if (!bRaw)
    {
        poObjRings = OGRGeoJSONFindMemberByName(poObj, "coordinates");
        if (nullptr == poObjRings)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid Polygon object. "
                     "Missing \'coordinates\' member.");
            return nullptr;
        }
    }
    else
    {
        poObjRings = poObj;
    }

    OGRPolygon *poPolygon = nullptr;

    if (json_type_array == json_object_get_type(poObjRings))
    {
        const auto nRings = json_object_array_length(poObjRings);
        if (nRings > 0)
        {
            json_object *poObjPoints = json_object_array_get_idx(poObjRings, 0);
            if (poObjPoints == nullptr)
            {
                poPolygon = new OGRPolygon();
            }
            else
            {
                OGRLinearRing *poRing = OGRGeoJSONReadLinearRing(poObjPoints);
                if (nullptr != poRing)
                {
                    poPolygon = new OGRPolygon();
                    poPolygon->addRingDirectly(poRing);
                }
            }

            for (auto i = decltype(nRings){1};
                 i < nRings && nullptr != poPolygon; ++i)
            {
                poObjPoints = json_object_array_get_idx(poObjRings, i);
                if (poObjPoints != nullptr)
                {
                    OGRLinearRing *poRing =
                        OGRGeoJSONReadLinearRing(poObjPoints);
                    if (nullptr != poRing)
                    {
                        poPolygon->addRingDirectly(poRing);
                    }
                }
            }
        }
        else
        {
            poPolygon = new OGRPolygon();
        }
    }

    return poPolygon;
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiPolygon                 */
/************************************************************************/

OGRMultiPolygon *OGRGeoJSONReadMultiPolygon(json_object *poObj)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjPolys = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if (nullptr == poObjPolys)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid MultiPolygon object. "
                 "Missing \'coordinates\' member.");
        return nullptr;
    }

    OGRMultiPolygon *poMultiPoly = nullptr;

    if (json_type_array == json_object_get_type(poObjPolys))
    {
        const auto nPolys = json_object_array_length(poObjPolys);

        poMultiPoly = new OGRMultiPolygon();

        for (auto i = decltype(nPolys){0}; i < nPolys; ++i)
        {
            json_object *poObjPoly = json_object_array_get_idx(poObjPolys, i);
            if (poObjPoly == nullptr)
            {
                poMultiPoly->addGeometryDirectly(new OGRPolygon());
            }
            else
            {
                OGRPolygon *poPoly = OGRGeoJSONReadPolygon(poObjPoly, true);
                if (nullptr != poPoly)
                {
                    poMultiPoly->addGeometryDirectly(poPoly);
                }
            }
        }
    }

    return poMultiPoly;
}

/************************************************************************/
/*                           OGRGeoJSONReadGeometryCollection           */
/************************************************************************/

OGRGeometryCollection *
OGRGeoJSONReadGeometryCollection(json_object *poObj, OGRSpatialReference *poSRS)
{
    CPLAssert(nullptr != poObj);

    json_object *poObjGeoms = OGRGeoJSONFindMemberByName(poObj, "geometries");
    if (nullptr == poObjGeoms)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid GeometryCollection object. "
                 "Missing \'geometries\' member.");
        return nullptr;
    }

    OGRGeometryCollection *poCollection = nullptr;

    if (json_type_array == json_object_get_type(poObjGeoms))
    {
        poCollection = new OGRGeometryCollection();
        poCollection->assignSpatialReference(poSRS);

        const auto nGeoms = json_object_array_length(poObjGeoms);
        for (auto i = decltype(nGeoms){0}; i < nGeoms; ++i)
        {
            json_object *poObjGeom = json_object_array_get_idx(poObjGeoms, i);
            if (poObjGeom == nullptr)
            {
                CPLDebug("GeoJSON", "Skipping null sub-geometry");
                continue;
            }

            OGRGeometry *poGeometry = OGRGeoJSONReadGeometry(poObjGeom, poSRS);
            if (nullptr != poGeometry)
            {
                poCollection->addGeometryDirectly(poGeometry);
            }
        }
    }

    return poCollection;
}

/************************************************************************/
/*                       OGR_G_CreateGeometryFromJson                   */
/************************************************************************/

/** Create a OGR geometry from a GeoJSON geometry object */
OGRGeometryH OGR_G_CreateGeometryFromJson(const char *pszJson)
{
    if (nullptr == pszJson)
    {
        // Translation failed.
        return nullptr;
    }

    json_object *poObj = nullptr;
    if (!OGRJSonParse(pszJson, &poObj))
        return nullptr;

    OGRGeometry *poGeometry = OGRGeoJSONReadGeometry(poObj);

    // Release JSON tree.
    json_object_put(poObj);

    return (OGRGeometryH)poGeometry;
}

/************************************************************************/
/*                       json_ex_get_object_by_path()                   */
/************************************************************************/

json_object *json_ex_get_object_by_path(json_object *poObj, const char *pszPath)
{
    if (poObj == nullptr || json_object_get_type(poObj) != json_type_object ||
        pszPath == nullptr || *pszPath == '\0')
    {
        return nullptr;
    }
    char **papszTokens = CSLTokenizeString2(pszPath, ".", 0);
    for (int i = 0; papszTokens[i] != nullptr; i++)
    {
        poObj = CPL_json_object_object_get(poObj, papszTokens[i]);
        if (poObj == nullptr)
            break;
        if (papszTokens[i + 1] != nullptr)
        {
            if (json_object_get_type(poObj) != json_type_object)
            {
                poObj = nullptr;
                break;
            }
        }
    }
    CSLDestroy(papszTokens);
    return poObj;
}

/************************************************************************/
/*                             OGRJSonParse()                           */
/************************************************************************/

bool OGRJSonParse(const char *pszText, json_object **ppoObj, bool bVerboseError)
{
    if (ppoObj == nullptr)
        return false;
    json_tokener *jstok = json_tokener_new();
    const int nLen = pszText == nullptr ? 0 : static_cast<int>(strlen(pszText));
    *ppoObj = json_tokener_parse_ex(jstok, pszText, nLen);
    if (jstok->err != json_tokener_success)
    {
        if (bVerboseError)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JSON parsing error: %s (at offset %d)",
                     json_tokener_error_desc(jstok->err), jstok->char_offset);
        }

        json_tokener_free(jstok);
        *ppoObj = nullptr;
        return false;
    }
    json_tokener_free(jstok);
    return true;
}

/************************************************************************/
/*                    CPL_json_object_object_get()                      */
/************************************************************************/

// This is the same as json_object_object_get() except it will not raise
// deprecation warning.

json_object *CPL_json_object_object_get(struct json_object *obj,
                                        const char *key)
{
    json_object *poRet = nullptr;
    json_object_object_get_ex(obj, key, &poRet);
    return poRet;
}

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
