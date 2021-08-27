/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Personal Geodatabase driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_pgeo.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")


/************************************************************************/
/*                                OGRPGeoDriverOpen()                   */
/************************************************************************/

static GDALDataset * OGRPGeoDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "WALK:") )
        return nullptr;

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOMEDIA:") )
        return nullptr;

    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "PGEO:")
        && !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"mdb") )
        return nullptr;

    // Disabling the attempt to guess if a MDB file is a PGeo database
    // or not. The mention to GDB_GeomColumns might be quite far in
    // the/ file, which can cause misdetection.  See
    // http://trac.osgeo.org/gdal/ticket/4498 This was initially meant
    // to know if a MDB should be opened by the PGeo or the Geomedia
    // driver.
#if 0
    if( !STARTS_WITH_CI(pszFilename, "PGEO:") &&
        EQUAL(CPLGetExtension(pszFilename),"mdb") )
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (!fp)
            return NULL;
        GByte* pabyHeader = (GByte*) CPLMalloc(100000);
        VSIFReadL(pabyHeader, 100000, 1, fp);
        VSIFCloseL(fp);

        /* Look for GDB_GeomColumns table */
        const GByte pabyNeedle[] = {
            'G', 0, 'D', 0, 'B', 0, '_', 0, 'G', 0, 'e', 0, 'o', 0, 'm', 0,
            'C', 0, 'o', 0, 'l', 0, 'u', 0, 'm', 0, 'n', 0, 's' };
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
    OGRPGeoDataSource *poDS = new OGRPGeoDataSource();

    if( !poDS->Open( poOpenInfo ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRPGeo()                          */
/************************************************************************/

void RegisterOGRPGeo()

{
    if( GDALGetDriverByName( "PGeo" ) != nullptr )
        return;

    GDALDriver* poDriver = new GDALDriver;

    poDriver->SetDescription( "PGeo" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ESRI Personal GeoDatabase" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mdb" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/pgeo.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList>"
"  <Option name='LIST_ALL_TABLES' type='string-select' scope='vector' description='Whether all tables, including system and internal tables (such as GDB_* tables) should be listed' default='NO'>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"</OpenOptionList>");

    poDriver->pfnOpen = OGRPGeoDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
