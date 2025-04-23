/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_odbc.h"
#include "cpl_conv.h"
#include "ogrodbcdrivercore.h"

/************************************************************************/
/*                      OGRODBCDriverOpen()                             */
/************************************************************************/

static GDALDataset *OGRODBCDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (OGRODBCDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    OGRODBCDataSource *poDS = new OGRODBCDataSource();

    if (!poDS->Open(poOpenInfo))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRODBC()                            */
/************************************************************************/

void RegisterOGRODBC()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver;
    OGRODBCDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRODBCDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
