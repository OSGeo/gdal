/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  GDALGeorefPamDataset with helper to read georeferencing and other
 *           metadata from JP2Boxes
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_JP2_ABSTRACT_DATASET_H_INCLUDED
#define GDAL_JP2_ABSTRACT_DATASET_H_INCLUDED

//! @cond Doxygen_Suppress
#include "gdalgeorefpamdataset.h"

class CPL_DLL GDALJP2AbstractDataset : public GDALGeorefPamDataset
{
    char *pszWldFilename = nullptr;

    GDALDataset *poMemDS = nullptr;
    char **papszMetadataFiles = nullptr;
    int m_nWORLDFILEIndex = -1;
    CPLStringList m_aosImageStructureMetadata{};

    CPL_DISALLOW_COPY_ASSIGN(GDALJP2AbstractDataset)

  protected:
    int CloseDependentDatasets() override;

    virtual VSILFILE *GetFileHandle()
    {
        return nullptr;
    }

  public:
    GDALJP2AbstractDataset();
    ~GDALJP2AbstractDataset() override;

    void LoadJP2Metadata(GDALOpenInfo *poOpenInfo,
                         const char *pszOverrideFilename = nullptr,
                         VSILFILE *fpBox = nullptr);
    void LoadVectorLayers(int bOpenRemoteResources = FALSE);

    char **GetFileList(void) override;

    char **GetMetadata(const char *pszDomain = "") override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;

    int GetLayerCount() override;
    OGRLayer *GetLayer(int i) override;
};

//! @endcond

#endif /* GDAL_JP2_ABSTRACT_DATASET_H_INCLUDED */
