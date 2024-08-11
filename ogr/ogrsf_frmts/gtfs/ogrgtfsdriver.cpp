/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements GTFS driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <map>
#include <new>
#include <utility>

/***********************************************************************/
/*                         OGRGTFSDataset                              */
/***********************************************************************/

class OGRGTFSDataset final : public GDALDataset
{
    std::vector<std::unique_ptr<OGRLayer>> m_apoLayers{};

  public:
    OGRGTFSDataset() = default;

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int nIdx) override;

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
};

/***********************************************************************/
/*                              GetLayer()                             */
/***********************************************************************/

OGRLayer *OGRGTFSDataset::GetLayer(int nIdx)
{
    return nIdx >= 0 && nIdx < static_cast<int>(m_apoLayers.size())
               ? m_apoLayers[nIdx].get()
               : nullptr;
}

/***********************************************************************/
/*                           OGRGTFSLayer                              */
/***********************************************************************/

class OGRGTFSLayer final : public OGRLayer
{
    std::string m_osDirname{};
    std::unique_ptr<GDALDataset> m_poUnderlyingDS{};
    OGRLayer *m_poUnderlyingLayer = nullptr;  // owned by m_poUnderlyingDS
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    int m_nTripIdIdx = -1;
    int m_nLatIdx = -1;
    int m_nLonIdx = -1;
    bool m_bIsTrips = false;
    bool m_bPrepared = false;
    std::map<std::string, std::pair<double, double>> m_oMapStopIdToLonLat{};
    std::map<std::string, std::map<int, std::string>> m_oMapTripIdToStopIds{};

    void PrepareTripsData();

  public:
    OGRGTFSLayer(const std::string &osDirname, const char *pszName,
                 std::unique_ptr<GDALDataset> &&poUnderlyingDS);
    ~OGRGTFSLayer() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    int TestCapability(const char *) override;
    GIntBig GetFeatureCount(int bForce) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }
};

/***********************************************************************/
/*                           OGRGTFSLayer()                            */
/***********************************************************************/

OGRGTFSLayer::OGRGTFSLayer(const std::string &osDirname, const char *pszName,
                           std::unique_ptr<GDALDataset> &&poUnderlyingDS)
    : m_osDirname(osDirname), m_poUnderlyingDS(std::move(poUnderlyingDS))
{
    m_poFeatureDefn = new OGRFeatureDefn(pszName);
    SetDescription(pszName);
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();

    m_poUnderlyingLayer = m_poUnderlyingDS->GetLayer(0);

    auto poSrcLayerDefn = m_poUnderlyingLayer->GetLayerDefn();
    const int nFieldCount = poSrcLayerDefn->GetFieldCount();
    m_nTripIdIdx = poSrcLayerDefn->GetFieldIndex("trip_id");
    if (EQUAL(pszName, "stops"))
    {
        m_nLatIdx = poSrcLayerDefn->GetFieldIndex("stop_lat");
        m_nLonIdx = poSrcLayerDefn->GetFieldIndex("stop_lon");
    }
    else if (EQUAL(pszName, "shapes"))
    {
        m_nLatIdx = poSrcLayerDefn->GetFieldIndex("shape_pt_lat");
        m_nLonIdx = poSrcLayerDefn->GetFieldIndex("shape_pt_lon");
    }
    m_bIsTrips = EQUAL(pszName, "trips") && m_nTripIdIdx >= 0;

    if (m_nLatIdx >= 0 && m_nLonIdx >= 0)
        m_poFeatureDefn->SetGeomType(wkbPoint);
    else if (m_bIsTrips)
        m_poFeatureDefn->SetGeomType(wkbLineString);

    for (int i = 0; i < nFieldCount; ++i)
    {
        OGRFieldDefn oFieldDefn(poSrcLayerDefn->GetFieldDefn(i));
        const char *pszFieldName = oFieldDefn.GetNameRef();
        if (i == m_nLatIdx || i == m_nLonIdx ||
            EQUAL(pszFieldName, "shape_dist_traveled"))
        {
            oFieldDefn.SetType(OFTReal);
        }
        else if (EQUAL(pszFieldName, "shape_pt_sequence"))
        {
            oFieldDefn.SetType(OFTInteger);
        }
        else if (EQUAL(pszFieldName, "date") ||
                 EQUAL(pszFieldName, "start_date") ||
                 EQUAL(pszFieldName, "end_date"))
        {
            oFieldDefn.SetType(OFTDate);
        }
        else if (EQUAL(pszFieldName, "arrival_time") ||
                 EQUAL(pszFieldName, "departure_time"))
        {
            oFieldDefn.SetType(OFTTime);
        }
        else if (strstr(pszFieldName, "_type") ||
                 EQUAL(pszFieldName, "stop_sequence"))
        {
            oFieldDefn.SetType(OFTInteger);
        }
        else if (EQUAL(pszFieldName, "monday") ||
                 EQUAL(pszFieldName, "tuesday") ||
                 EQUAL(pszFieldName, "wednesday") ||
                 EQUAL(pszFieldName, "thursday") ||
                 EQUAL(pszFieldName, "friday") ||
                 EQUAL(pszFieldName, "saturday") ||
                 EQUAL(pszFieldName, "sunday"))
        {
            oFieldDefn.SetType(OFTInteger);
            oFieldDefn.SetSubType(OFSTBoolean);
        }
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
}

/***********************************************************************/
/*                          ~OGRGTFSLayer()                            */
/***********************************************************************/

OGRGTFSLayer::~OGRGTFSLayer()
{
    m_poFeatureDefn->Release();
}

/***********************************************************************/
/*                          ResetReading()                             */
/***********************************************************************/

void OGRGTFSLayer::ResetReading()
{
    m_poUnderlyingLayer->ResetReading();
}

/***********************************************************************/
/*                        PrepareTripsData()                           */
/***********************************************************************/

void OGRGTFSLayer::PrepareTripsData()
{
    m_bPrepared = true;
    try
    {
        {
            auto poStopsDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                (m_osDirname + "/stops.txt").c_str(), GDAL_OF_VECTOR));
            if (!poStopsDS)
                return;
            auto poStopsLyr = poStopsDS->GetLayer(0);
            if (!poStopsLyr)
                return;
            const auto poStopsLyrDefn = poStopsLyr->GetLayerDefn();
            const int nStopIdIdx = poStopsLyrDefn->GetFieldIndex("stop_id");
            const int nStopLatIdx = poStopsLyrDefn->GetFieldIndex("stop_lat");
            const int nStopLonIdx = poStopsLyrDefn->GetFieldIndex("stop_lon");
            if (nStopIdIdx < 0 || nStopLatIdx < 0 || nStopLonIdx < 0)
                return;
            for (auto &&poFeature : poStopsLyr)
            {
                const char *pszStopId = poFeature->GetFieldAsString(nStopIdIdx);
                if (pszStopId)
                {
                    m_oMapStopIdToLonLat[pszStopId] = std::make_pair(
                        poFeature->GetFieldAsDouble(nStopLonIdx),
                        poFeature->GetFieldAsDouble(nStopLatIdx));
                }
            }
        }

        auto poStopTimesDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            (m_osDirname + "/stop_times.txt").c_str(), GDAL_OF_VECTOR));
        if (!poStopTimesDS)
            return;
        auto poStopTimesLyr = poStopTimesDS->GetLayer(0);
        if (!poStopTimesLyr)
            return;
        const auto poStopTimesLyrDefn = poStopTimesLyr->GetLayerDefn();
        const int nStopIdIdx = poStopTimesLyrDefn->GetFieldIndex("stop_id");
        const int nTripIdIdx = poStopTimesLyrDefn->GetFieldIndex("trip_id");
        const int nStopSequenceIdx =
            poStopTimesLyrDefn->GetFieldIndex("stop_sequence");
        if (nStopIdIdx < 0 || nTripIdIdx < 0 || nStopSequenceIdx < 0)
            return;
        for (auto &&poFeature : poStopTimesLyr)
        {
            const char *pszStopId = poFeature->GetFieldAsString(nStopIdIdx);
            const char *pszTripId = poFeature->GetFieldAsString(nTripIdIdx);
            const int nStopSequence =
                poFeature->GetFieldAsInteger(nStopSequenceIdx);
            if (pszStopId && pszTripId)
            {
                m_oMapTripIdToStopIds[pszTripId][nStopSequence] = pszStopId;
            }
        }
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Not enough memory");
    }
}

/***********************************************************************/
/*                          GetNextFeature()                           */
/***********************************************************************/

OGRFeature *OGRGTFSLayer::GetNextFeature()
{
    if (m_bIsTrips && !m_bPrepared)
        PrepareTripsData();

    while (true)
    {
        auto poSrcFeature =
            std::unique_ptr<OGRFeature>(m_poUnderlyingLayer->GetNextFeature());
        if (poSrcFeature == nullptr)
            return nullptr;

        auto poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
        const int nFieldCount = poSrcFeature->GetFieldCount();
        poFeature->SetFID(poSrcFeature->GetFID());
        auto poSrcLayerDefn = m_poUnderlyingLayer->GetLayerDefn();
        for (int i = 0; i < nFieldCount; ++i)
        {
            const auto eType = m_poFeatureDefn->GetFieldDefn(i)->GetType();
            if (poSrcLayerDefn->GetFieldDefn(i)->GetType() == eType)
            {
                poFeature->SetField(i, poSrcFeature->GetRawFieldRef(i));
            }
            else if (eType == OFTDate)
            {
                const char *pszVal = poSrcFeature->GetFieldAsString(i);
                constexpr char ZERO_DIGIT = '0';
                if (pszVal && strlen(pszVal) == 8)
                {
                    const int nYear = (pszVal[0] - ZERO_DIGIT) * 1000 +
                                      (pszVal[1] - ZERO_DIGIT) * 100 +
                                      (pszVal[2] - ZERO_DIGIT) * 10 +
                                      (pszVal[3] - ZERO_DIGIT);
                    const int nMonth = (pszVal[4] - ZERO_DIGIT) * 10 +
                                       (pszVal[5] - ZERO_DIGIT);
                    const int nDay = (pszVal[6] - ZERO_DIGIT) * 10 +
                                     (pszVal[7] - ZERO_DIGIT);
                    poFeature->SetField(i, nYear, nMonth, nDay, 0, 0, 0, 0);
                }
            }
            else if (eType == OFTInteger)
            {
                poFeature->SetField(i, poSrcFeature->GetFieldAsInteger(i));
            }
            else
            {
                const char *pszVal = poSrcFeature->GetFieldAsString(i);
                poFeature->SetField(i, pszVal);
            }
        }
        if (m_nLatIdx >= 0 && m_nLonIdx >= 0)
        {
            poFeature->SetGeometryDirectly(
                new OGRPoint(poFeature->GetFieldAsDouble(m_nLonIdx),
                             poFeature->GetFieldAsDouble(m_nLatIdx)));
        }
        else if (m_bIsTrips)
        {
            const char *pszTripId = poFeature->GetFieldAsString(m_nTripIdIdx);
            if (pszTripId)
            {
                const auto oIter = m_oMapTripIdToStopIds.find(pszTripId);
                if (oIter != m_oMapTripIdToStopIds.end())
                {
                    OGRLineString *poLS = new OGRLineString();
                    for (const auto &kv : oIter->second)
                    {
                        const auto oIter2 =
                            m_oMapStopIdToLonLat.find(kv.second);
                        if (oIter2 != m_oMapStopIdToLonLat.end())
                            poLS->addPoint(oIter2->second.first,
                                           oIter2->second.second);
                    }
                    poFeature->SetGeometryDirectly(poLS);
                }
            }
        }
        if ((!m_poFilterGeom || FilterGeometry(poFeature->GetGeometryRef())) &&
            (!m_poAttrQuery || m_poAttrQuery->Evaluate(poFeature.get())))
        {
            return poFeature.release();
        }
    }
}

/***********************************************************************/
/*                          GetFeatureCount()                          */
/***********************************************************************/

GIntBig OGRGTFSLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom || m_poAttrQuery)
        return OGRLayer::GetFeatureCount(bForce);
    return m_poUnderlyingLayer->GetFeatureCount(bForce);
}

/***********************************************************************/
/*                          TestCapability()                           */
/***********************************************************************/

int OGRGTFSLayer::TestCapability(const char *pszCap)
{
    return EQUAL(pszCap, OLCStringsAsUTF8);
}

/***********************************************************************/
/*                         OGRGTFSShapesGeomLayer                      */
/***********************************************************************/

class OGRGTFSShapesGeomLayer final : public OGRLayer
{
    std::unique_ptr<GDALDataset> m_poUnderlyingDS{};
    OGRLayer *m_poUnderlyingLayer = nullptr;  // owned by m_poUnderlyingDS
    OGRFeatureDefn *m_poFeatureDefn = nullptr;
    bool m_bPrepared = false;
    std::vector<std::unique_ptr<OGRFeature>> m_apoFeatures{};
    size_t m_nIdx = 0;

    void Prepare();

  public:
    explicit OGRGTFSShapesGeomLayer(
        std::unique_ptr<GDALDataset> &&poUnderlyingDS);
    ~OGRGTFSShapesGeomLayer() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    int TestCapability(const char *) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int bForce) override;
};

/***********************************************************************/
/*                       OGRGTFSShapesGeomLayer()                      */
/***********************************************************************/

OGRGTFSShapesGeomLayer::OGRGTFSShapesGeomLayer(
    std::unique_ptr<GDALDataset> &&poUnderlyingDS)
    : m_poUnderlyingDS(std::move(poUnderlyingDS))
{
    m_poFeatureDefn = new OGRFeatureDefn("shapes_geom");
    SetDescription("shapes_geom");
    m_poFeatureDefn->SetGeomType(wkbLineString);
    m_poFeatureDefn->Reference();
    OGRFieldDefn oField("shape_id", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oField);

    m_poUnderlyingLayer = m_poUnderlyingDS->GetLayer(0);
}

/***********************************************************************/
/*                      ~OGRGTFSShapesGeomLayer()                      */
/***********************************************************************/

OGRGTFSShapesGeomLayer::~OGRGTFSShapesGeomLayer()
{
    m_poFeatureDefn->Release();
}

/***********************************************************************/
/*                          ResetReading()                             */
/***********************************************************************/

void OGRGTFSShapesGeomLayer::ResetReading()
{
    m_nIdx = 0;
}

/***********************************************************************/
/*                            Prepare()                                */
/***********************************************************************/

void OGRGTFSShapesGeomLayer::Prepare()
{
    m_bPrepared = true;
    const auto poSrcLayerDefn = m_poUnderlyingLayer->GetLayerDefn();
    const int nShapeIdIdx = poSrcLayerDefn->GetFieldIndex("shape_id");
    const int nLonIdx = poSrcLayerDefn->GetFieldIndex("shape_pt_lon");
    const int nLatIdx = poSrcLayerDefn->GetFieldIndex("shape_pt_lat");
    const int nSeqIdx = poSrcLayerDefn->GetFieldIndex("shape_pt_sequence");
    if (nShapeIdIdx < 0 || nLonIdx < 0 || nLatIdx < 0 || nSeqIdx < 0)
        return;
    std::map<std::string, std::map<int, std::pair<double, double>>> oMap;
    try
    {
        for (auto &&poFeature : m_poUnderlyingLayer)
        {
            const char *pszShapeId = poFeature->GetFieldAsString(nShapeIdIdx);
            if (pszShapeId)
            {
                const int nSeq = poFeature->GetFieldAsInteger(nSeqIdx);
                const double dfLon = poFeature->GetFieldAsDouble(nLonIdx);
                const double dfLat = poFeature->GetFieldAsDouble(nLatIdx);
                oMap[pszShapeId][nSeq] = std::make_pair(dfLon, dfLat);
            }
        }
        for (const auto &kv : oMap)
        {
            const auto &osShapeId = kv.first;
            const auto &oMapPoints = kv.second;
            auto poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn);
            poFeature->SetField(0, osShapeId.c_str());
            OGRLineString *poLS = new OGRLineString();
            for (const auto &kv2 : oMapPoints)
            {
                poLS->addPoint(kv2.second.first, kv2.second.second);
            }
            poFeature->SetGeometryDirectly(poLS);
            poFeature->SetFID(static_cast<GIntBig>(m_apoFeatures.size()));
            m_apoFeatures.emplace_back(std::move(poFeature));
        }
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Not enough memory");
    }
}

/***********************************************************************/
/*                          GetNextFeature()                           */
/***********************************************************************/

OGRFeature *OGRGTFSShapesGeomLayer::GetNextFeature()
{
    if (!m_bPrepared)
        Prepare();
    while (true)
    {
        if (m_nIdx >= m_apoFeatures.size())
            return nullptr;
        if ((m_poFilterGeom == nullptr ||
             FilterGeometry(m_apoFeatures[m_nIdx]->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr ||
             m_poAttrQuery->Evaluate(m_apoFeatures[m_nIdx].get())))
        {
            auto poRet = m_apoFeatures[m_nIdx]->Clone();
            m_nIdx++;
            return poRet;
        }
        m_nIdx++;
    }
}

/***********************************************************************/
/*                          TestCapability()                           */
/***********************************************************************/

int OGRGTFSShapesGeomLayer::TestCapability(const char *pszCap)
{
    return EQUAL(pszCap, OLCStringsAsUTF8);
}

/***********************************************************************/
/*                          GetFeatureCount()                          */
/***********************************************************************/

GIntBig OGRGTFSShapesGeomLayer::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery != nullptr || m_poFilterGeom != nullptr)
        return OGRLayer::GetFeatureCount(bForce);
    if (!m_bPrepared)
        Prepare();
    return static_cast<GIntBig>(m_apoFeatures.size());
}

/***********************************************************************/
/*                              Identify()                             */
/***********************************************************************/

static const char *const apszRequiredFiles[] = {"agency.txt", "routes.txt",
                                                "trips.txt",  "stop_times.txt",
                                                "stops.txt",  "calendar.txt"};

int OGRGTFSDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH(poOpenInfo->pszFilename, "GTFS:"))
        return TRUE;

    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "zip"))
        return FALSE;

    // Check first filename in ZIP

    constexpr int OFFSET_FILENAME_SIZE = 26;
    constexpr int OFFSET_FILENAME_VAL = 30;
    if (poOpenInfo->nHeaderBytes < OFFSET_FILENAME_VAL ||
        memcmp(poOpenInfo->pabyHeader, "PK\x03\x04", 4) != 0)
    {
        return FALSE;
    }

    for (const char *pszFilename : apszRequiredFiles)
    {
        const int nLen = static_cast<int>(strlen(pszFilename));
        if (CPL_LSBSINT16PTR(poOpenInfo->pabyHeader + OFFSET_FILENAME_SIZE) ==
                nLen &&
            poOpenInfo->nHeaderBytes > OFFSET_FILENAME_VAL + nLen &&
            memcmp(poOpenInfo->pabyHeader + OFFSET_FILENAME_VAL, pszFilename,
                   nLen) == 0)
        {
            return TRUE;
        }
    }

    static const char *const apszOptionalFiles[] = {
        "calendar_dates.txt", "fare_attributes.txt", "fare_rules.txt",
        "shapes.txt",         "frequencies.txt",     "transfers.txt",
        "feed_info.txt"};
    for (const char *pszFilename : apszOptionalFiles)
    {
        const int nLen = static_cast<int>(strlen(pszFilename));
        if (CPL_LSBSINT16PTR(poOpenInfo->pabyHeader + OFFSET_FILENAME_SIZE) ==
                nLen &&
            poOpenInfo->nHeaderBytes > OFFSET_FILENAME_VAL + nLen &&
            memcmp(poOpenInfo->pabyHeader + OFFSET_FILENAME_VAL, pszFilename,
                   nLen) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/***********************************************************************/
/*                              Open()                                 */
/***********************************************************************/

GDALDataset *OGRGTFSDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    const char *pszFilename = poOpenInfo->pszFilename;
    if (STARTS_WITH(pszFilename, "GTFS:"))
        pszFilename += strlen("GTFS:");

    std::string osBaseDir(pszFilename);
    if (!STARTS_WITH(pszFilename, "/vsizip/") &&
        EQUAL(CPLGetExtension(pszFilename), "zip"))
    {
        osBaseDir = "/vsizip/{";
        osBaseDir += pszFilename;
        osBaseDir += '}';
    }

    const std::string osCSVBaseDirPrefix(std::string("CSV:") + osBaseDir);

    auto poDS = std::make_unique<OGRGTFSDataset>();

    char **papszFilenames = VSIReadDir(osBaseDir.c_str());
    size_t nCountFound = 0;
    std::string osShapesFilename;
    for (CSLConstList papszIter = papszFilenames; papszIter && *papszIter;
         ++papszIter)
    {
        if (!EQUAL(CPLGetExtension(*papszIter), "txt"))
            continue;
        for (const char *pszFilenameInDir : apszRequiredFiles)
        {
            if (EQUAL(*papszIter, pszFilenameInDir))
            {
                nCountFound++;
                break;
            }
        }
        if (EQUAL(*papszIter, "shapes.txt"))
            osShapesFilename = *papszIter;

        auto poCSVDataset = std::unique_ptr<GDALDataset>(
            GDALDataset::Open((osCSVBaseDirPrefix + '/' + *papszIter).c_str(),
                              GDAL_OF_VERBOSE_ERROR | GDAL_OF_VECTOR));
        if (poCSVDataset)
        {
            auto poUnderlyingLayer = poCSVDataset->GetLayer(0);
            if (poUnderlyingLayer)
            {
                auto poSrcLayerDefn = poUnderlyingLayer->GetLayerDefn();
                if (poSrcLayerDefn->GetFieldIndex("field_1") < 0)
                {
                    poDS->m_apoLayers.emplace_back(
                        std::make_unique<OGRGTFSLayer>(
                            osCSVBaseDirPrefix, CPLGetBasename(*papszIter),
                            std::move(poCSVDataset)));
                }
            }
        }
    }
    CSLDestroy(papszFilenames);

    if (nCountFound != sizeof(apszRequiredFiles) / sizeof(apszRequiredFiles[0]))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GTFS: required .txt files missing");
        return nullptr;
    }

    if (!osShapesFilename.empty())
    {
        auto poCSVDataset = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            (osCSVBaseDirPrefix + '/' + osShapesFilename).c_str(),
            GDAL_OF_VERBOSE_ERROR | GDAL_OF_VECTOR));
        if (poCSVDataset)
        {
            auto poUnderlyingLayer = poCSVDataset->GetLayer(0);
            if (poUnderlyingLayer)
            {
                poDS->m_apoLayers.emplace_back(
                    std::make_unique<OGRGTFSShapesGeomLayer>(
                        std::move(poCSVDataset)));
            }
        }
    }

    return poDS.release();
}

/***********************************************************************/
/*                         RegisterOGRGTFS()                           */
/***********************************************************************/

void RegisterOGRGTFS()

{
    if (GDALGetDriverByName("GTFS") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("GTFS");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "General Transit Feed Specification");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/gtfs.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "zip");

    poDriver->pfnOpen = OGRGTFSDataset::Open;
    poDriver->pfnIdentify = OGRGTFSDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
