/******************************************************************************
 *
 * Name:     gdal_defaultoverviews.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALDefaultOverviews class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALDEFAULTOVERVIEWS_H_INCLUDED
#define GDALDEFAULTOVERVIEWS_H_INCLUDED

#include "cpl_port.h"
#include "cpl_progress.h"
#include "cpl_string.h"

/* ******************************************************************** */
/*                         GDALDefaultOverviews                         */
/* ******************************************************************** */

//! @cond Doxygen_Suppress
class GDALDataset;
class GDALOpenInfo;
class GDALRasterBand;

class CPL_DLL GDALDefaultOverviews
{
    friend class GDALDataset;

    GDALDataset *poDS;
    GDALDataset *poODS;

    CPLString osOvrFilename{};

    bool bOvrIsAux;

    bool bCheckedForMask;
    bool bOwnMaskDS;
    GDALDataset *poMaskDS;

    // For "overview datasets" we record base level info so we can
    // find our way back to get overview masks.
    GDALDataset *poBaseDS;

    // Stuff for deferred initialize/overviewscans.
    bool bCheckedForOverviews;
    void OverviewScan();
    char *pszInitName;
    bool bInitNameIsOVR;
    char **papszInitSiblingFiles;

  public:
    GDALDefaultOverviews();
    ~GDALDefaultOverviews();

    void Initialize(GDALDataset *poDSIn, const char *pszName = nullptr,
                    CSLConstList papszSiblingFiles = nullptr,
                    bool bNameIsOVR = false);

    void Initialize(GDALDataset *poDSIn, GDALOpenInfo *poOpenInfo,
                    const char *pszName = nullptr,
                    bool bTransferSiblingFilesIfLoaded = true);

    void TransferSiblingFiles(char **papszSiblingFiles);

    int IsInitialized();

    int CloseDependentDatasets();

    // Overview Related

    int GetOverviewCount(int nBand);
    GDALRasterBand *GetOverview(int nBand, int iOverview);

    CPLErr BuildOverviews(const char *pszBasename, const char *pszResampling,
                          int nOverviews, const int *panOverviewList,
                          int nBands, const int *panBandList,
                          GDALProgressFunc pfnProgress, void *pProgressData,
                          CSLConstList papszOptions);

    CPLErr BuildOverviewsSubDataset(const char *pszPhysicalFile,
                                    const char *pszResampling, int nOverviews,
                                    const int *panOverviewList, int nBands,
                                    const int *panBandList,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData,
                                    CSLConstList papszOptions);

    CPLErr BuildOverviewsMask(const char *pszResampling, int nOverviews,
                              const int *panOverviewList,
                              GDALProgressFunc pfnProgress, void *pProgressData,
                              CSLConstList papszOptions);

    static bool CheckSrcOverviewsConsistencyWithBase(
        GDALDataset *poFullResDS,
        const std::vector<GDALDataset *> &apoSrcOvrDS);

    CPLErr AddOverviews(const char *pszBasename,
                        const std::vector<GDALDataset *> &apoSrcOvrDS,
                        GDALProgressFunc pfnProgress, void *pProgressData,
                        CSLConstList papszOptions);

    CPLErr CleanOverviews();

    // Mask Related

    CPLErr CreateMaskBand(int nFlags, int nBand = -1);
    GDALRasterBand *GetMaskBand(int nBand);
    int GetMaskFlags(int nBand);

    int HaveMaskFile(char **papszSiblings = nullptr,
                     const char *pszBasename = nullptr);

    CSLConstList GetSiblingFiles() const
    {
        return papszInitSiblingFiles;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALDefaultOverviews)

    CPLErr CreateOrOpenOverviewFile(const char *pszBasename,
                                    CSLConstList papszOptions);
};

//! @endcond

#endif
