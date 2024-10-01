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
/*                OGRJSONFGMemLayer::OGRJSONFGMemLayer()                */
/************************************************************************/

OGRJSONFGMemLayer::OGRJSONFGMemLayer(GDALDataset *poDS, const char *pszName,
                                     OGRSpatialReference *poSRS,
                                     OGRwkbGeometryType eGType)
    : OGRMemLayer(pszName, poSRS, eGType), m_poDS(poDS)
{
    SetAdvertizeUTF8(true);
    SetUpdatable(false);
}

/************************************************************************/
/*                OGRJSONFGMemLayer::~OGRJSONFGMemLayer()               */
/************************************************************************/

OGRJSONFGMemLayer::~OGRJSONFGMemLayer() = default;

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

void OGRJSONFGMemLayer::AddFeature(std::unique_ptr<OGRFeature> poFeature)
{
    GIntBig nFID = poFeature->GetFID();

    // Detect potential FID duplicates and make sure they are eventually
    // unique.
    if (-1 == nFID)
    {
        nFID = GetFeatureCount(FALSE);
        OGRFeature *poTryFeature = nullptr;
        while ((poTryFeature = GetFeature(nFID)) != nullptr)
        {
            nFID++;
            delete poTryFeature;
        }
    }
    else
    {
        OGRFeature *poTryFeature = nullptr;
        if ((poTryFeature = GetFeature(nFID)) != nullptr)
        {
            if (!bOriginalIdModified_)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Several features with id = " CPL_FRMT_GIB " have been "
                    "found. Altering it to be unique. This warning will not "
                    "be emitted anymore for this layer",
                    nFID);
                bOriginalIdModified_ = true;
            }
            delete poTryFeature;
            nFID = GetFeatureCount(FALSE);
            while ((poTryFeature = GetFeature(nFID)) != nullptr)
            {
                nFID++;
                delete poTryFeature;
            }
        }
    }
    poFeature->SetFID(nFID);

    if (!CPL_INT64_FITS_ON_INT32(nFID))
        SetMetadataItem(OLMD_FID64, "YES");

    const bool bIsUpdatable = IsUpdatable();
    SetUpdatable(true);  // Temporary toggle on updatable flag.
    CPL_IGNORE_RET_VAL(OGRMemLayer::SetFeature(poFeature.get()));
    SetUpdatable(bIsUpdatable);
    SetUpdated(false);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRJSONFGMemLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCCurveGeometries))
        return FALSE;

    else if (EQUAL(pszCap, OLCMeasuredGeometries))
        return FALSE;

    return OGRMemLayer::TestCapability(pszCap);
}
