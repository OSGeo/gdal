/******************************************************************************
 *
 * Project:  Erdas Imagine Driver
 * Purpose:  Entry point for building overviews, used by non-imagine formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "hfa_p.h"

#include <cstddef>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"

CPLErr HFAAuxBuildOverviews(const char *pszOvrFilename, GDALDataset *poParentDS,
                            GDALDataset **ppoODS, int nBands,
                            const int *panBandList, int nNewOverviews,
                            const int *panNewOverviewList,
                            const char *pszResampling,
                            GDALProgressFunc pfnProgress, void *pProgressData,
                            CSLConstList papszOptions)

{
    // If the .aux file doesn't exist yet then create it now.
    if (*ppoODS == nullptr)
    {
        GDALDataType eDT = GDT_Unknown;
        // Determine the band datatype, and verify that all bands are the same.
        for (int iBand = 0; iBand < nBands; iBand++)
        {
            GDALRasterBand *poBand =
                poParentDS->GetRasterBand(panBandList[iBand]);

            if (iBand == 0)
            {
                eDT = poBand->GetRasterDataType();
            }
            else
            {
                if (eDT != poBand->GetRasterDataType())
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "HFAAuxBuildOverviews() doesn't support a "
                             "mixture of band data types.");
                    return CE_Failure;
                }
            }
        }

        // Create the HFA (.aux) file.  We create it with
        // COMPRESSED=YES so that no space will be allocated for the
        // base band.
        GDALDriver *poHFADriver =
            static_cast<GDALDriver *>(GDALGetDriverByName("HFA"));
        if (poHFADriver == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "HFA driver is unavailable.");
            return CE_Failure;
        }

        CPLString osDepFileOpt = "DEPENDENT_FILE=";
        osDepFileOpt += CPLGetFilename(poParentDS->GetDescription());

        const char *const apszOptions[4] = {"COMPRESSED=YES", "AUX=YES",
                                            osDepFileOpt.c_str(), nullptr};

        *ppoODS =
            poHFADriver->Create(pszOvrFilename, poParentDS->GetRasterXSize(),
                                poParentDS->GetRasterYSize(),
                                poParentDS->GetRasterCount(), eDT, apszOptions);

        if (*ppoODS == nullptr)
            return CE_Failure;
    }

    // Create the layers.  We depend on the normal buildoverviews
    // support for HFA to do this.  But we disable the internal
    // computation of the imagery for these layers.
    //
    // We avoid regenerating the new layers here, because if we did
    // it would use the base layer from the .aux file as the source
    // data, and that is fake (all invalid tiles).
    CPLStringList aosOptions(papszOptions);
    aosOptions.SetNameValue("REGENERATE", "NO");

    CPLErr eErr = (*ppoODS)->BuildOverviews(
        pszResampling, nNewOverviews, panNewOverviewList, nBands, panBandList,
        pfnProgress, pProgressData, aosOptions.List());

    return eErr;
}
