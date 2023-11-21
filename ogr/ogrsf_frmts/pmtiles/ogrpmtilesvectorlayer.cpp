/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Planet Labs
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

#include "ogr_pmtiles.h"

#include "mvtutils.h"

#include <algorithm>
#include <time.h>

/************************************************************************/
/*                        OGRPMTilesVectorLayer()                       */
/************************************************************************/

OGRPMTilesVectorLayer::OGRPMTilesVectorLayer(
    OGRPMTilesDataset *poDS, const char *pszLayerName,
    const CPLJSONObject &oFields, const CPLJSONArray &oAttributesFromTileStats,
    bool bJsonField, double dfMinX, double dfMinY, double dfMaxX, double dfMaxY,
    OGRwkbGeometryType eGeomType, int nZoomLevel,
    bool bZoomLevelFromSpatialFilter)
    : m_poDS(poDS), m_poFeatureDefn(new OGRFeatureDefn(pszLayerName)),
      m_bJsonField(bJsonField)
{
    SetDescription(pszLayerName);
    m_poFeatureDefn->SetGeomType(eGeomType);
    OGRSpatialReference *poSRS = new OGRSpatialReference();
    poSRS->importFromEPSG(3857);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poSRS->Release();
    m_poFeatureDefn->Reference();

    if (m_bJsonField)
    {
        OGRFieldDefn oFieldDefnId("mvt_id", OFTInteger64);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefnId);
    }
    else
    {
        OGRMVTInitFields(m_poFeatureDefn, oFields, oAttributesFromTileStats);
    }

    m_sExtent.MinX = dfMinX;
    m_sExtent.MinY = dfMinY;
    m_sExtent.MaxX = dfMaxX;
    m_sExtent.MaxY = dfMaxY;

    m_nZoomLevel = nZoomLevel;
    m_bZoomLevelAuto = bZoomLevelFromSpatialFilter;
    OGRPMTilesVectorLayer::SetSpatialFilter(nullptr);

    // If the metadata contains an empty fields object, this may be a sign
    // that it doesn't know the schema. In that case check if a tile has
    // attributes, and in that case create a json field.
    if (!m_bJsonField && oFields.IsValid() && oFields.GetChildren().empty())
    {
        m_bJsonField = true;
        auto poSrcFeature = GetNextSrcFeature();
        m_bJsonField = false;

        if (poSrcFeature)
        {
            // There is at least the mvt_id field
            if (poSrcFeature->GetFieldCount() > 1)
            {
                m_bJsonField = true;
            }
        }
        OGRPMTilesVectorLayer::ResetReading();
    }

    if (m_bJsonField)
    {
        OGRFieldDefn oFieldDefn("json", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
}

/************************************************************************/
/*                       ~OGRPMTilesVectorLayer()                       */
/************************************************************************/

OGRPMTilesVectorLayer::~OGRPMTilesVectorLayer()
{
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void OGRPMTilesVectorLayer::ResetReading()
{
    m_poTileDS.reset();
    m_poTileLayer = nullptr;
    m_poTileIterator.reset();
}

/************************************************************************/
/*                      GuessGeometryType()                             */
/************************************************************************/

/* static */
OGRwkbGeometryType OGRPMTilesVectorLayer::GuessGeometryType(
    OGRPMTilesDataset *poDS, const char *pszLayerName, int nZoomLevel)
{
    OGRPMTilesTileIterator oIterator(poDS, nZoomLevel);

    const char *const apszAllowedDrivers[] = {"MVT", nullptr};
    CPLStringList aosOpenOptions;
    aosOpenOptions.SetNameValue("METADATA_FILE",
                                poDS->GetMetadataFilename().c_str());
    std::string osTileData;
    bool bFirst = true;
    OGRwkbGeometryType eGeomType = wkbUnknown;
    time_t nStart;
    time(&nStart);
    while (true)
    {
        uint32_t nRunLength = 0;
        const auto sTile = oIterator.GetNextTile(&nRunLength);
        if (sTile.offset == 0)
        {
            break;
        }

        const auto *posStr = poDS->ReadTileData(sTile.offset, sTile.length);
        if (!posStr)
        {
            continue;
        }
        osTileData = *posStr;

        std::string osTmpFilename =
            CPLSPrintf("/vsimem/mvt_%p_%u_%u.pbf", poDS, sTile.x, sTile.y);
        VSIFCloseL(VSIFileFromMemBuffer(
            osTmpFilename.c_str(), reinterpret_cast<GByte *>(&osTileData[0]),
            osTileData.size(), false));

        auto poTileDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            ("MVT:" + osTmpFilename).c_str(), GDAL_OF_VECTOR | GDAL_OF_INTERNAL,
            apszAllowedDrivers, aosOpenOptions.List(), nullptr));
        if (poTileDS)
        {
            auto poTileLayer = poTileDS->GetLayerByName(pszLayerName);
            if (poTileLayer)
            {
                if (bFirst)
                {
                    eGeomType = poTileLayer->GetGeomType();
                    if (eGeomType != wkbUnknown)
                        bFirst = false;
                }
                else if (eGeomType != poTileLayer->GetGeomType())
                {
                    VSIUnlink(osTmpFilename.c_str());
                    return wkbUnknown;
                }
                if (nRunLength > 1)
                    oIterator.SkipRunLength();
            }
        }
        VSIUnlink(osTmpFilename.c_str());

        // Browse through tiles no longer than 1 sec
        time_t nNow;
        time(&nNow);
        if (nNow - nStart > 1)
            break;
    }

    return eGeomType;
}

/************************************************************************/
/*                    GetTotalFeatureCount()                            */
/************************************************************************/

GIntBig OGRPMTilesVectorLayer::GetTotalFeatureCount() const
{
    OGRPMTilesTileIterator oIterator(m_poDS, m_nZoomLevel);

    GIntBig nFeatureCount = 0;
    const char *const apszAllowedDrivers[] = {"MVT", nullptr};
    CPLStringList aosOpenOptions;
    aosOpenOptions.SetNameValue("METADATA_FILE",
                                m_poDS->GetMetadataFilename().c_str());
    std::string osTileData;
    while (true)
    {
        uint32_t nRunLength = 0;
        const auto sTile = oIterator.GetNextTile(&nRunLength);
        if (sTile.offset == 0)
        {
            break;
        }

        const auto *posStr = m_poDS->ReadTileData(sTile.offset, sTile.length);
        if (!posStr)
        {
            continue;
        }
        osTileData = *posStr;

        std::string osTmpFilename = CPLSPrintf(
            "/vsimem/mvt_%p_%u_%u_getfeaturecount.pbf", this, sTile.x, sTile.y);
        VSIFCloseL(VSIFileFromMemBuffer(
            osTmpFilename.c_str(), reinterpret_cast<GByte *>(&osTileData[0]),
            osTileData.size(), false));

        auto poTileDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            ("MVT:" + osTmpFilename).c_str(), GDAL_OF_VECTOR | GDAL_OF_INTERNAL,
            apszAllowedDrivers, aosOpenOptions.List(), nullptr));
        if (poTileDS)
        {
            auto poTileLayer = poTileDS->GetLayerByName(GetDescription());
            if (poTileLayer)
            {
                const GIntBig nTileFeatureCount =
                    poTileLayer->GetFeatureCount();
                nFeatureCount += nRunLength * nTileFeatureCount;
                if (nRunLength > 1)
                    oIterator.SkipRunLength();
            }
        }
        VSIUnlink(osTmpFilename.c_str());
    }

    return nFeatureCount;
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig OGRPMTilesVectorLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr)
    {
        if (m_nFeatureCount < 0)
        {
            m_nFeatureCount = GetTotalFeatureCount();
        }
        return m_nFeatureCount;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature *OGRPMTilesVectorLayer::GetFeature(GIntBig nFID)
{
    if (nFID < 0)
        return nullptr;
    const int nZ = m_nZoomLevel;
    const int nX = static_cast<int>(nFID & ((1 << nZ) - 1));
    const int nY = static_cast<int>((nFID >> nZ) & ((1 << nZ) - 1));
    const GIntBig nTileFID = nFID >> (2 * nZ);

    OGRPMTilesTileIterator oIterator(m_poDS, m_nZoomLevel, nX, nY, nX, nY);
    const auto sTile = oIterator.GetNextTile();
    if (sTile.offset == 0)
    {
        return nullptr;
    }
    CPLAssert(sTile.z == m_nZoomLevel);
    CPLAssert(sTile.x == static_cast<uint32_t>(nX));
    CPLAssert(sTile.y == static_cast<uint32_t>(nY));

    const auto *posStr = m_poDS->ReadTileData(sTile.offset, sTile.length);
    if (!posStr)
    {
        return nullptr;
    }
    std::string osTileData = *posStr;

    std::string osTmpFilename = CPLSPrintf(
        "/vsimem/mvt_%p_%u_%u_getfeature.pbf", this, sTile.x, sTile.y);
    VSIFCloseL(VSIFileFromMemBuffer(osTmpFilename.c_str(),
                                    reinterpret_cast<GByte *>(&osTileData[0]),
                                    osTileData.size(), false));

    const char *const apszAllowedDrivers[] = {"MVT", nullptr};
    CPLStringList aosOpenOptions;
    aosOpenOptions.SetNameValue("X", CPLSPrintf("%u", sTile.x));
    aosOpenOptions.SetNameValue("Y", CPLSPrintf("%u", sTile.y));
    aosOpenOptions.SetNameValue("Z", CPLSPrintf("%d", m_nZoomLevel));
    aosOpenOptions.SetNameValue(
        "METADATA_FILE",
        m_bJsonField ? "" : m_poDS->GetMetadataFilename().c_str());
    if (!m_poDS->GetClipOpenOption().empty())
    {
        aosOpenOptions.SetNameValue("CLIP",
                                    m_poDS->GetClipOpenOption().c_str());
    }
    auto poTileDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
        ("MVT:" + osTmpFilename).c_str(), GDAL_OF_VECTOR | GDAL_OF_INTERNAL,
        apszAllowedDrivers, aosOpenOptions.List(), nullptr));
    std::unique_ptr<OGRFeature> poFeature;
    if (poTileDS)
    {
        auto poTileLayer = poTileDS->GetLayerByName(GetDescription());
        if (poTileLayer)
        {
            auto poUnderlyingFeature =
                std::unique_ptr<OGRFeature>(poTileLayer->GetFeature(nTileFID));
            if (poUnderlyingFeature)
            {
                poFeature = CreateFeatureFrom(poUnderlyingFeature.get());
                poFeature->SetFID(nFID);
            }
        }
    }
    VSIUnlink(osTmpFilename.c_str());

    return poFeature.release();
}

/************************************************************************/
/*                        GetNextSrcFeature()                           */
/************************************************************************/

std::unique_ptr<OGRFeature> OGRPMTilesVectorLayer::GetNextSrcFeature()
{
    if (!m_poTileIterator)
    {
        int nMinTileX;
        int nMinTileY;
        int nMaxTileX;
        int nMaxTileY;
        ExtentToTileExtent(m_sExtent, nMinTileX, nMinTileY, nMaxTileX,
                           nMaxTileY);

        // Optimization: if the spatial filter is totally out of the extent,
        // exit early
        if (m_nFilterMaxX < nMinTileX || m_nFilterMaxY < nMinTileY ||
            m_nFilterMinX > nMaxTileX || m_nFilterMinY > nMaxTileY)
        {
            return nullptr;
        }

        m_poTileIterator = std::make_unique<OGRPMTilesTileIterator>(
            m_poDS, m_nZoomLevel, m_nFilterMinX, m_nFilterMinY, m_nFilterMaxX,
            m_nFilterMaxY);
    }

    OGRFeature *poTileFeat = nullptr;
    if (!m_poTileLayer ||
        (poTileFeat = m_poTileLayer->GetNextFeature()) == nullptr)
    {
        const char *const apszAllowedDrivers[] = {"MVT", nullptr};

        while (true)
        {
            const auto sTile = m_poTileIterator->GetNextTile();
            if (sTile.offset == 0)
            {
                return nullptr;
            }

            m_nX = sTile.x;
            m_nY = sTile.y;

            if (sTile.offset == m_nLastTileOffset)
            {
                // In case of run-length encoded tiles, we do not need to
                // re-read it from disk
            }
            else
            {
                m_nLastTileOffset = sTile.offset;
                CPLDebugOnly("PMTiles", "Opening tile X=%u, Y=%u, Z=%d",
                             sTile.x, sTile.y, m_nZoomLevel);

                const auto *posStr =
                    m_poDS->ReadTileData(sTile.offset, sTile.length);
                if (!posStr)
                {
                    return nullptr;
                }
                m_osTileData = *posStr;
            }

            m_poTileDS.reset();
            const std::string osTmpFilename =
                CPLSPrintf("/vsimem/mvt_%p_%u_%u.pbf", this, sTile.x, sTile.y);
            VSIFCloseL(VSIFileFromMemBuffer(
                osTmpFilename.c_str(),
                reinterpret_cast<GByte *>(&m_osTileData[0]),
                m_osTileData.size(), false));

            CPLStringList aosOpenOptions;
            aosOpenOptions.SetNameValue("X", CPLSPrintf("%u", sTile.x));
            aosOpenOptions.SetNameValue("Y", CPLSPrintf("%u", sTile.y));
            aosOpenOptions.SetNameValue("Z", CPLSPrintf("%d", m_nZoomLevel));
            aosOpenOptions.SetNameValue(
                "METADATA_FILE",
                m_bJsonField ? "" : m_poDS->GetMetadataFilename().c_str());
            if (!m_poDS->GetClipOpenOption().empty())
            {
                aosOpenOptions.SetNameValue(
                    "CLIP", m_poDS->GetClipOpenOption().c_str());
            }
            m_poTileDS.reset(GDALDataset::Open(
                ("MVT:" + osTmpFilename).c_str(),
                GDAL_OF_VECTOR | GDAL_OF_INTERNAL, apszAllowedDrivers,
                aosOpenOptions.List(), nullptr));
            if (m_poTileDS)
            {
                m_poTileDS->SetDescription(osTmpFilename.c_str());
                m_poTileDS->MarkSuppressOnClose();
                m_poTileLayer = m_poTileDS->GetLayerByName(GetDescription());
                if (m_poTileLayer)
                {
                    poTileFeat = m_poTileLayer->GetNextFeature();
                    if (poTileFeat)
                    {
                        break;
                    }
                }
                m_poTileDS.reset();
                m_poTileLayer = nullptr;
            }
            else
            {
                VSIUnlink(osTmpFilename.c_str());
            }
        }
    }

    return std::unique_ptr<OGRFeature>(poTileFeat);
}

/************************************************************************/
/*                         CreateFeatureFrom()                          */
/************************************************************************/

std::unique_ptr<OGRFeature>
OGRPMTilesVectorLayer::CreateFeatureFrom(OGRFeature *poSrcFeature)
{
    return std::unique_ptr<OGRFeature>(OGRMVTCreateFeatureFrom(
        poSrcFeature, m_poFeatureDefn, m_bJsonField, GetSpatialRef()));
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature *OGRPMTilesVectorLayer::GetNextRawFeature()
{
    auto poSrcFeat = GetNextSrcFeature();
    if (poSrcFeat == nullptr)
        return nullptr;

    const GIntBig nFIDBase =
        (static_cast<GIntBig>(m_nY) << m_nZoomLevel) | m_nX;
    auto poFeature = CreateFeatureFrom(poSrcFeat.get());
    poFeature->SetFID((poSrcFeat->GetFID() << (2 * m_nZoomLevel)) | nFIDBase);

    return poFeature.release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPMTilesVectorLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCStringsAsUTF8) ||
        EQUAL(pszCap, OLCFastSpatialFilter) || EQUAL(pszCap, OLCFastGetExtent))
    {
        return TRUE;
    }

    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_nFeatureCount >= 0 && !m_poFilterGeom && !m_poAttrQuery;

    return FALSE;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRPMTilesVectorLayer::GetExtent(OGREnvelope *psExtent, int)
{
    *psExtent = m_sExtent;
    return OGRERR_NONE;
}

/************************************************************************/
/*                         ExtentToTileExtent()                         */
/************************************************************************/

void OGRPMTilesVectorLayer::ExtentToTileExtent(const OGREnvelope &sEnvelope,
                                               int &nTileMinX, int &nTileMinY,
                                               int &nTileMaxX,
                                               int &nTileMaxY) const
{
    const double dfTileDim = 2 * MAX_GM / (1 << m_nZoomLevel);
    constexpr double EPS = 1e-5;
    nTileMinX = std::max(0, static_cast<int>(floor(
                                (sEnvelope.MinX + MAX_GM) / dfTileDim + EPS)));
    // PMTiles and MVT uses a Y=MAX_GM as the y=0 tile
    nTileMinY = std::max(0, static_cast<int>(floor(
                                (MAX_GM - sEnvelope.MaxY) / dfTileDim + EPS)));
    nTileMaxX = std::min(
        static_cast<int>(floor((sEnvelope.MaxX + MAX_GM) / dfTileDim + EPS)),
        (1 << m_nZoomLevel) - 1);
    nTileMaxY = std::min(
        static_cast<int>(floor((MAX_GM - sEnvelope.MinY) / dfTileDim + EPS)),
        (1 << m_nZoomLevel) - 1);
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRPMTilesVectorLayer::SetSpatialFilter(OGRGeometry *poGeomIn)
{
    OGRLayer::SetSpatialFilter(poGeomIn);

    if (m_poFilterGeom != nullptr && m_sFilterEnvelope.MinX <= -MAX_GM &&
        m_sFilterEnvelope.MinY <= -MAX_GM && m_sFilterEnvelope.MaxX >= MAX_GM &&
        m_sFilterEnvelope.MaxY >= MAX_GM)
    {
        if (m_bZoomLevelAuto)
        {
            m_nZoomLevel = m_poDS->GetMinZoomLevel();
        }
        m_nFilterMinX = 0;
        m_nFilterMinY = 0;
        m_nFilterMaxX = (1 << m_nZoomLevel) - 1;
        m_nFilterMaxY = (1 << m_nZoomLevel) - 1;
    }
    else if (m_poFilterGeom != nullptr &&
             m_sFilterEnvelope.MinX >= -10 * MAX_GM &&
             m_sFilterEnvelope.MinY >= -10 * MAX_GM &&
             m_sFilterEnvelope.MaxX <= 10 * MAX_GM &&
             m_sFilterEnvelope.MaxY <= 10 * MAX_GM)
    {
        if (m_bZoomLevelAuto)
        {
            double dfExtent =
                std::min(m_sFilterEnvelope.MaxX - m_sFilterEnvelope.MinX,
                         m_sFilterEnvelope.MaxY - m_sFilterEnvelope.MinY);
            m_nZoomLevel = std::max(
                m_poDS->GetMinZoomLevel(),
                std::min(static_cast<int>(0.5 + log(2 * MAX_GM / dfExtent) /
                                                    log(2.0)),
                         m_poDS->GetMaxZoomLevel()));
            CPLDebug("PMTiles", "Zoom level = %d", m_nZoomLevel);
        }
        ExtentToTileExtent(m_sFilterEnvelope, m_nFilterMinX, m_nFilterMinY,
                           m_nFilterMaxX, m_nFilterMaxY);
    }
    else
    {
        if (m_bZoomLevelAuto)
        {
            m_nZoomLevel = m_poDS->GetMaxZoomLevel();
        }
        m_nFilterMinX = 0;
        m_nFilterMinY = 0;
        m_nFilterMaxX = (1 << m_nZoomLevel) - 1;
        m_nFilterMaxY = (1 << m_nZoomLevel) - 1;
    }
}
