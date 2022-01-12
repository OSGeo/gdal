/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGmtDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gmt.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                    OGRGMTDriverIdentify()                            */
/************************************************************************/

static int OGRGMTDriverIdentify( GDALOpenInfo *poOpenInfo )

{
    const char* pszHeader = reinterpret_cast<const char*>(poOpenInfo->pabyHeader);
    if( poOpenInfo->nHeaderBytes && strstr(pszHeader, "@VGMT") != nullptr )
        return true;

    if( EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "GMT") )
        return true;

    return false;
}

/************************************************************************/
/*                           OGRGMTDriverOpen()                         */
/************************************************************************/

static GDALDataset *OGRGMTDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRGMTDriverIdentify(poOpenInfo) )
        return nullptr;

    OGRGmtDataSource *poDS = new OGRGmtDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, nullptr, nullptr,
                     poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         OGRGMTDriverCreate()                         */
/************************************************************************/


static GDALDataset *OGRGMTDriverCreate( const char *pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
    OGRGmtDataSource *poDS = new OGRGmtDataSource();

    if( poDS->Create( pszName, papszOptions ) )
        return poDS;

    delete poDS;
    return nullptr;
}


/************************************************************************/
/*                           RegisterOGRGMT()                           */
/************************************************************************/

void RegisterOGRGMT()

{
    if( GDALGetDriverByName("OGR_GMT") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("OGR_GMT");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GMT ASCII Vectors (.gmt)" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gmt" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/gmt.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRGMTDriverOpen;
    poDriver->pfnIdentify = OGRGMTDriverIdentify;
    poDriver->pfnCreate = OGRGMTDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
