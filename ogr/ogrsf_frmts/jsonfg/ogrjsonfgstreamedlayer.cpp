/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGC Features and Geometries JSON (JSON-FG)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_jsonfg.h"

/************************************************************************/
/*             OGRJSONFGStreamedLayer::OGRJSONFGStreamedLayer()         */
/************************************************************************/

OGRJSONFGStreamedLayer::OGRJSONFGStreamedLayer(GDALDataset *poDS,
                                               const char *pszName,
                                               OGRSpatialReference *poSRS,
                                               OGRwkbGeometryType eGType)
    : m_poDS(poDS), poFeatureDefn_(new OGRFeatureDefn(pszName))
{

    poFeatureDefn_->Reference();

    SetDescription(poFeatureDefn_->GetName());
    poFeatureDefn_->SetGeomType(eGType);

    if (eGType != wkbNone && poSRS != nullptr)
    {
        OGRSpatialReference *poSRSClone = poSRS->Clone();
        poFeatureDefn_->GetGeomFieldDefn(0)->SetSpatialRef(poSRSClone);
        poSRSClone->Release();
    }

    poFeatureDefn_->Seal(/* bSealFields = */ true);
}

/************************************************************************/
/*           OGRJSONFGStreamedLayer::~OGRJSONFGStreamedLayer()          */
/************************************************************************/

OGRJSONFGStreamedLayer::~OGRJSONFGStreamedLayer()
{
    poFeatureDefn_->Release();
}

/************************************************************************/
/*                            SetFile()                                 */
/************************************************************************/

void OGRJSONFGStreamedLayer::SetFile(VSIVirtualHandleUniquePtr &&poFile)
{
    poFile_ = std::move(poFile);
    poFile_->Seek(0, SEEK_SET);
}

/************************************************************************/
/*                        SetStreamingParser()                          */
/************************************************************************/

void OGRJSONFGStreamedLayer::SetStreamingParser(
    std::unique_ptr<OGRJSONFGStreamingParser> &&poStreamingParser)
{
    poStreamingParser_ = std::move(poStreamingParser);
    poStreamingParser_->SetRequestedLayer(GetName());
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

void OGRJSONFGStreamedLayer::ResetReading()
{
    CPLAssert(poFile_);
    CPLAssert(poStreamingParser_);
    poStreamingParser_ = poStreamingParser_->Clone();
    poFile_->Seek(0, SEEK_SET);
    oSetUsedFIDs_.clear();
}

/************************************************************************/
/*                             EnsureUniqueFID()                                 */
/************************************************************************/

OGRFeature *OGRJSONFGStreamedLayer::EnsureUniqueFID(OGRFeature *poFeat)
{
    GIntBig nFID = poFeat->GetFID();
    if (nFID == OGRNullFID)
    {
        nFID = static_cast<GIntBig>(oSetUsedFIDs_.size());
        while (oSetUsedFIDs_.find(nFID) != oSetUsedFIDs_.end())
        {
            ++nFID;
        }
    }
    else if (oSetUsedFIDs_.find(nFID) != oSetUsedFIDs_.end())
    {
        if (!bOriginalIdModified_)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Several features with id = " CPL_FRMT_GIB " have "
                     "been found. Altering it to be unique. "
                     "This warning will not be emitted anymore for "
                     "this layer",
                     nFID);
            bOriginalIdModified_ = true;
        }
        nFID = static_cast<GIntBig>(oSetUsedFIDs_.size());
        while (oSetUsedFIDs_.find(nFID) != oSetUsedFIDs_.end())
        {
            ++nFID;
        }
    }
    oSetUsedFIDs_.insert(nFID);
    poFeat->SetFID(nFID);
    return poFeat;
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature *OGRJSONFGStreamedLayer::GetNextRawFeature()
{
    CPLAssert(poFile_);
    CPLAssert(poStreamingParser_);

    auto poFeatAndLayer = poStreamingParser_->GetNextFeature();
    if (poFeatAndLayer.first)
    {
        return EnsureUniqueFID(poFeatAndLayer.first.release());
    }

    std::vector<GByte> abyBuffer;
    abyBuffer.resize(4096 * 10);
    while (true)
    {
        size_t nRead = poFile_->Read(abyBuffer.data(), 1, abyBuffer.size());
        const bool bFinished = nRead < abyBuffer.size();
        if (!poStreamingParser_->Parse(
                reinterpret_cast<const char *>(abyBuffer.data()), nRead,
                bFinished) ||
            poStreamingParser_->ExceptionOccurred())
        {
            break;
        }

        poFeatAndLayer = poStreamingParser_->GetNextFeature();
        if (poFeatAndLayer.first)
        {
            return EnsureUniqueFID(poFeatAndLayer.first.release());
        }
        if (bFinished)
            break;
    }

    return nullptr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRJSONFGStreamedLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return !m_poFilterGeom && !m_poAttrQuery && nFeatureCount_ >= 0;

    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;

    else if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

GIntBig OGRJSONFGStreamedLayer::GetFeatureCount(int bForce)
{
    if (!m_poFilterGeom && !m_poAttrQuery && nFeatureCount_ >= 0)
        return nFeatureCount_;
    return OGRLayer::GetFeatureCount(bForce);
}
