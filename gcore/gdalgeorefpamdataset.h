/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALPamDataset with internal storage for georeferencing, with
 *           priority for PAM over internal georeferencing
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_GEOREF_PAM_DATASET_H_INCLUDED
#define GDAL_GEOREF_PAM_DATASET_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "gdal_pam.h"

class CPL_DLL GDALGeorefPamDataset : public GDALPamDataset
{
  protected:
    bool bGeoTransformValid;
    double adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};
    int nGCPCount;
    GDAL_GCP *pasGCPList;
    char **m_papszRPC;
    bool m_bPixelIsPoint;

    int m_nGeoTransformGeorefSrcIndex;
    int m_nGCPGeorefSrcIndex;
    int m_nProjectionGeorefSrcIndex;
    int m_nRPCGeorefSrcIndex;
    int m_nPixelIsPointGeorefSrcIndex;

    int GetPAMGeorefSrcIndex() const;
    mutable bool m_bGotPAMGeorefSrcIndex;
    mutable int m_nPAMGeorefSrcIndex;

    bool m_bPAMLoaded;
    char **m_papszMainMD;

    CPL_DISALLOW_COPY_ASSIGN(GDALGeorefPamDataset)

  public:
    GDALGeorefPamDataset();
    ~GDALGeorefPamDataset() override;

    CPLErr TryLoadXML(CSLConstList papszSiblingFiles = nullptr) override;

    CPLErr GetGeoTransform(double *) override;

    const OGRSpatialReference *GetSpatialRef() const override;

    int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;

    char **GetMetadata(const char *pszDomain = "") override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;
    CPLErr SetMetadata(char **papszMetadata,
                       const char *pszDomain = "") override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "") override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif /* GDAL_GEOREF_PAM_DATASET_H_INCLUDED */
