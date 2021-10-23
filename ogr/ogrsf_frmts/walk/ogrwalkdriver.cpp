/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRWalkDriver class.
 * Author:   Xian Chen, chenxian at walkinfo.com.cn
 *
 ******************************************************************************
 * Copyright (c) 2013,  ZJU Walkinfo Technology Corp., Ltd.
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogrwalk.h"

CPL_CVSID("$Id$")


/************************************************************************/
/*                                OGRWalkDriverOpen()                   */
/************************************************************************/

static GDALDataset *OGRWalkDriverOpen( GDALOpenInfo* poOpenInfo )
{

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "PGEO:") )
        return nullptr;

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOMEDIA:") )
        return nullptr;

    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "WALK:")
        && !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MDB") )
        return nullptr;

#ifndef WIN32
    // Try to register MDB Tools driver
    CPLODBCDriverInstaller::InstallMdbToolsDriver();
#endif /* ndef WIN32 */

    OGRWalkDataSource  *poDS = new OGRWalkDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        return nullptr;
    }

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("WALK") )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}


/************************************************************************/
/*                          RegisterOGRWalk()                           */
/************************************************************************/

void RegisterOGRWalk()

{
    if( GDALGetDriverByName( "Walk" ) != nullptr )
        return;

    GDALDriver* poDriver = new GDALDriver;

    poDriver->SetDescription( "Walk" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );

    poDriver->pfnOpen = OGRWalkDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
