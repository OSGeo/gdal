/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRAVCDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_avc.h"

/************************************************************************/
/*                          OGRAVCDataSource()                          */
/************************************************************************/

OGRAVCDataSource::OGRAVCDataSource() : poSRS(nullptr), pszCoverageName(nullptr)
{
}

/************************************************************************/
/*                         ~OGRAVCDataSource()                          */
/************************************************************************/

OGRAVCDataSource::~OGRAVCDataSource()

{
    if (poSRS)
        poSRS->Release();
    CPLFree(pszCoverageName);
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRAVCDataSource::DSGetSpatialRef()

{
    return poSRS;
}

/************************************************************************/
/*                          GetCoverageName()                           */
/************************************************************************/

const char *OGRAVCDataSource::GetCoverageName()

{
    if (pszCoverageName == nullptr)
        return "";

    return pszCoverageName;
}
