/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr.h"

/************************************************************************/
/*                              Rename()                                */
/************************************************************************/

bool ZarrDimension::Rename(const std::string &osNewName)
{
    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }
    if (!IsXArrayDimension())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot rename an implicit dimension "
                 "(that is one listed in _ARRAY_DIMENSIONS attribute)");
        return false;
    }
    if (!ZarrGroupBase::IsValidObjectName(osNewName))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid dimension name");
        return false;
    }

    if (auto poParentGroup = m_poParentGroup.lock())
    {
        if (!poParentGroup->RenameDimension(m_osName, osNewName))
        {
            return false;
        }
    }

    BaseRename(osNewName);

    m_bModified = true;

    return true;
}
