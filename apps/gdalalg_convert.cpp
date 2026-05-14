/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//! @cond Doxygen_Suppress

#include "cpl_error.h"
#include "gdalalg_convert.h"
#include "gdal_priv.h"

/************************************************************************/
/*                         GDALConvertAlgorithm                         */
/************************************************************************/

GDALConvertAlgorithm::GDALConvertAlgorithm()
    : GDALDispatcherAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    // only for the help message
    AddProgressArg();
    AddOutputFormatArg(&m_format);
    AddInputDatasetArg(&m_inputDataset);
    AddOutputDatasetArg(&m_outputDataset);

    m_longDescription = "For all options, run 'gdal raster convert --help' "
                        "or 'gdal vector convert --help'";
}

GDALConvertAlgorithm::~GDALConvertAlgorithm() = default;

//! @endcond
