/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101Dataset
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_s101.h"
#include "ogrs101drivercore.h"
#include "ogrs101featurecatalog.h"

/************************************************************************/
/*                            UnloadDriver()                            */
/************************************************************************/

/* static */ void OGRS101Dataset::UnloadDriver(GDALDriver *)
{
    OGRS101FeatureCatalog::CleanupSingletonFeatureCatalog();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

/* static */ GDALDataset *OGRS101Dataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!OGRS101DriverIdentify(poOpenInfo))
        return nullptr;

    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("S101");
        return nullptr;
    }

    auto poDS = std::make_unique<OGRS101Dataset>();
    {
        auto poReader = std::make_unique<OGRS101Reader>();
        if (!poReader->Load(poOpenInfo))
            return nullptr;
        poDS->m_poReader = std::move(poReader);
    }
    auto &poReader = poDS->m_poReader;
    poDS->SetMetadata(poReader->GetMetadata().List());

    if (poReader->IsCancelled())
        return poDS.release();

    {
        auto poFDefn = poReader->StealInformationTypeFeatureDefn();
        if (poFDefn)
        {
            poDS->m_apoLayers.push_back(
                std::make_unique<OGRS101LayerInformationType>(
                    *poDS, poReader->GetInformationTypeRecords(),
                    std::move(poFDefn)));
        }
    }

    for (auto &[nCRSId, poFDefn] : poReader->StealPointFeatureDefns())
    {
        const auto &oMap = poReader->GetMapCRSIdToRecordIdxForPoints();
        const auto oIterIndices = oMap.find(nCRSId);
        CPLAssert(oIterIndices != oMap.end());
        const auto &anRecordIndices = oIterIndices->second;
        poDS->m_apoLayers.push_back(std::make_unique<OGRS101LayerPoint>(
            *poDS, poReader->GetPointRecords(), anRecordIndices,
            std::move(poFDefn)));
    }

    for (auto &[nCRSId, poFDefn] : poReader->StealMultiPointFeatureDefns())
    {
        const auto &oMap = poReader->GetMapCRSIdToRecordIdxForMultiPoints();
        const auto oIterIndices = oMap.find(nCRSId);
        CPLAssert(oIterIndices != oMap.end());
        const auto &anRecordIndices = oIterIndices->second;
        poDS->m_apoLayers.push_back(std::make_unique<OGRS101LayerMultiPoint>(
            *poDS, poReader->GetMultiPointRecords(), anRecordIndices,
            std::move(poFDefn)));
    }

    {
        auto poFDefn = poReader->StealCurveFeatureDefn();
        if (poFDefn)
        {
            poDS->m_apoLayers.push_back(std::make_unique<OGRS101LayerCurve>(
                *poDS, poReader->GetCurveRecords(), std::move(poFDefn)));
        }
    }

    {
        auto poFDefn = poReader->StealCompositeCurveFeatureDefn();
        if (poFDefn)
        {
            poDS->m_apoLayers.push_back(
                std::make_unique<OGRS101LayerCompositeCurve>(
                    *poDS, poReader->GetCompositeCurveRecords(),
                    std::move(poFDefn)));
        }
    }

    {
        auto poFDefn = poReader->StealSurfaceFeatureDefn();
        if (poFDefn)
        {
            poDS->m_apoLayers.push_back(std::make_unique<OGRS101LayerSurface>(
                *poDS, poReader->GetSurfaceRecords(), std::move(poFDefn)));
        }
    }

    for (auto &[featureTypeKey, oLayerDef] :
         poReader->StealFeatureTypeLayerDefs())
    {
        auto poLayer = std::make_unique<OGRS101LayerFeatureType>(
            *poDS, poReader->GetFeatureTypeRecords(), oLayerDef.anRecordIndices,
            std::move(oLayerDef.poFeatureDefn));
        if (!oLayerDef.osName.empty())
            poLayer->SetMetadataItem("NAME", oLayerDef.osName.c_str());
        if (!oLayerDef.osDefinition.empty())
            poLayer->SetMetadataItem("DEFINITION",
                                     oLayerDef.osDefinition.c_str());
        if (!oLayerDef.osAlias.empty())
            poLayer->SetMetadataItem("ALIAS", oLayerDef.osAlias.c_str());
        poDS->m_apoLayers.push_back(std::move(poLayer));
    }

    poDS->m_oMapFieldDomains = std::move(poReader->StealFieldDomains());

    return poDS.release();
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRS101Dataset::GetLayerCount() const
{
    return static_cast<int>(m_apoLayers.size());
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRS101Dataset::GetLayer(int i) const
{
    if (i < 0 || i >= GetLayerCount())
        return nullptr;
    return m_apoLayers[i].get();
}

/************************************************************************/
/*                    OGRS101Layer::TestCapability()                    */
/************************************************************************/

int OGRS101Dataset::TestCapability(const char *pszCap) const
{
    if (EQUAL(pszCap, ODsCZGeometries))
    {
        for (const auto &poLayer : m_apoLayers)
        {
            if (poLayer->TestCapability(OLCZGeometries))
                return true;
        }
        return false;
    }

    // Totally custom, for autotest purposes
    if (EQUAL(pszCap, "HasFeatureCatalog"))
        return m_poReader->GetFeatureCatalog() != nullptr;

    return GDALDataset::TestCapability(pszCap);
}
