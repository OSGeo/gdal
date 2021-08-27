/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Personal Geodatabase driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_geomedia.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGeomediaDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "WALK:") )
        return nullptr;

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "PGEO:") )
        return nullptr;

    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOMEDIA:")
        && !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"mdb") )
        return nullptr;

    /* Disabling the attempt to guess if a MDB file is a Geomedia database */
    /* or not. See similar fix in PGeo driver for rationale. */
#if 0
    if( !STARTS_WITH_CI(pszFilename, "GEOMEDIA:") &&
        EQUAL(CPLGetExtension(pszFilename),"mdb") )
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (!fp)
            return NULL;
        GByte* pabyHeader = (GByte*) CPLMalloc(100000);
        VSIFReadL(pabyHeader, 100000, 1, fp);
        VSIFCloseL(fp);

        /* Look for GAliasTable table */
        const GByte pabyNeedle[] = { 'G', 0, 'A', 0, 'l', 0, 'i', 0, 'a', 0, 's', 0, 'T', 0, 'a', 0, 'b', 0, 'l', 0, 'e'};
        int bFound = FALSE;
        for(int i=0;i<100000 - (int)sizeof(pabyNeedle);i++)
        {
            if (memcmp(pabyHeader + i, pabyNeedle, sizeof(pabyNeedle)) == 0)
            {
                bFound = TRUE;
                break;
            }
        }
        CPLFree(pabyHeader);
        if (!bFound)
            return NULL;
    }
#endif

#ifndef WIN32
    // Try to register MDB Tools driver
    CPLODBCDriverInstaller::InstallMdbToolsDriver();
#endif /* ndef WIN32 */

    // Open data source
    OGRGeomediaDataSource *poDS = new OGRGeomediaDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        return nullptr;
    }

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("GEOMEDIA") )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}


/************************************************************************/
/*                           RegisterOGRODBC()                          */
/************************************************************************/

void RegisterOGRGeomedia()

{
    if( GDALGetDriverByName( "Geomedia" ) != nullptr )
        return;

    GDALDriver* poDriver = new GDALDriver;

    poDriver->SetDescription( "Geomedia" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Geomedia .mdb" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mdb" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/geomedia.html" );

    poDriver->pfnOpen = OGRGeomediaDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
