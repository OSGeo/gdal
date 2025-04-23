/******************************************************************************
 *
 * Project:  Feather Translator
 * Purpose:  Implements OGRFeatherDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_feather.h"

#include "../arrow_common/ograrrowdataset.hpp"

/************************************************************************/
/*                         OGRFeatherDataset()                          */
/************************************************************************/

OGRFeatherDataset::OGRFeatherDataset(
    const std::shared_ptr<arrow::MemoryPool> &poMemoryPool)
    : OGRArrowDataset(poMemoryPool)
{
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRFeatherDataset::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCZGeometries))
        return true;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return true;

    return false;
}
