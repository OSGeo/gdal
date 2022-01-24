/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGPSbabelDriver class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_spawn.h"

#include "ogr_gpsbabel.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRGPSBabelDriverIdentify()                  */
/************************************************************************/

static bool OGRGPSBabelDriverIdentifyInternal(
    GDALOpenInfo* poOpenInfo,
    const char** ppszGSPBabelDriverName )
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GPSBABEL:") )
        return true;

    const char* pszGPSBabelDriverName = nullptr;
    if( poOpenInfo->fpL == nullptr )
            return false;

    if (memcmp(poOpenInfo->pabyHeader, "MsRcd", 5) == 0)
        pszGPSBabelDriverName = "mapsource";
    else if (memcmp(poOpenInfo->pabyHeader, "MsRcf", 5) == 0)
        pszGPSBabelDriverName = "gdb";
    else if (strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "<osm") != nullptr)
    {
        if( GDALGetDriverByName("OSM") != nullptr )
            return false;
        pszGPSBabelDriverName = "osm";
    }
    else if (strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "<TrainingCenterDatabase") != nullptr)
        pszGPSBabelDriverName = "gtrnctr";
    else if (strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "$GPGSA") != nullptr ||
                strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "$GPGGA") != nullptr)
        pszGPSBabelDriverName = "nmea";
    else if (STARTS_WITH_CI((const char*)poOpenInfo->pabyHeader, "OziExplorer"))
        pszGPSBabelDriverName = "ozi";
    else if (strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "Grid") &&
                strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "Datum") &&
                strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "Header"))
        pszGPSBabelDriverName = "garmin_txt";
    else if (poOpenInfo->pabyHeader[0] == 13 && poOpenInfo->pabyHeader[10] == 'M' && poOpenInfo->pabyHeader[11] == 'S' &&
                (poOpenInfo->pabyHeader[12] >= '0' && poOpenInfo->pabyHeader[12] <= '9') &&
                (poOpenInfo->pabyHeader[13] >= '0' && poOpenInfo->pabyHeader[13] <= '9') &&
                poOpenInfo->pabyHeader[12] * 10 + poOpenInfo->pabyHeader[13] >= 30 &&
                (poOpenInfo->pabyHeader[14] == 1 || poOpenInfo->pabyHeader[14] == 2) && poOpenInfo->pabyHeader[15] == 0 &&
                poOpenInfo->pabyHeader[16] == 0 && poOpenInfo->pabyHeader[17] == 0)
        pszGPSBabelDriverName = "mapsend";
    else if (strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "$PMGNWPL") != nullptr ||
                strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader), "$PMGNRTE") != nullptr)
        pszGPSBabelDriverName = "magellan";
    else if (poOpenInfo->pabyHeader[0] == 'A' &&
                poOpenInfo->pabyHeader[1] >= 'A' && poOpenInfo->pabyHeader[1] <= 'Z' &&
                poOpenInfo->pabyHeader[2] >= 'A' && poOpenInfo->pabyHeader[2] <= 'Z' &&
                poOpenInfo->pabyHeader[3] >= 'A' && poOpenInfo->pabyHeader[3] <= 'Z' &&
                EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "igc") )
        pszGPSBabelDriverName = "igc";

    static int bGPSBabelFound = -1;
    if( pszGPSBabelDriverName != nullptr && bGPSBabelFound < 0 )
    {
#ifndef WIN32
        VSIStatBufL sStat;
        bGPSBabelFound = VSIStatL("/usr/bin/gpsbabel", &sStat) == 0;
        if( !bGPSBabelFound )
#endif
        {
            const char* const apszArgs[] = { "gpsbabel", "-V", nullptr };
            CPLString osTmpFileName("/vsimem/gpsbabel_tmp.tmp");
            VSILFILE* tmpfp = VSIFOpenL(osTmpFileName, "wb");
            bGPSBabelFound = CPLSpawn(apszArgs, nullptr, tmpfp, FALSE) == 0;
            VSIFCloseL(tmpfp);
            VSIUnlink(osTmpFileName);
        }
    }

    if( bGPSBabelFound )
        *ppszGSPBabelDriverName = pszGPSBabelDriverName;
    return *ppszGSPBabelDriverName != nullptr;
}

static int OGRGPSBabelDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    const char* pszGPSBabelDriverName = nullptr;
    return OGRGPSBabelDriverIdentifyInternal( poOpenInfo,
                                              &pszGPSBabelDriverName );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGPSBabelDriverOpen( GDALOpenInfo* poOpenInfo )

{
    const char* pszGPSBabelDriverName = nullptr;
    if (poOpenInfo->eAccess == GA_Update ||
        !OGRGPSBabelDriverIdentifyInternal(poOpenInfo, &pszGPSBabelDriverName))
        return nullptr;

    OGRGPSBabelDataSource *poDS = new OGRGPSBabelDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, pszGPSBabelDriverName,
                     poOpenInfo->papszOpenOptions ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRGPSBabelDriverCreate( const char * pszName,
                                             int /* nBands */,
                                             int /* nXSize */,
                                             int /* nYSize */,
                                             GDALDataType /* eDT */,
                                             char **papszOptions )
{
    OGRGPSBabelWriteDataSource *poDS = new OGRGPSBabelWriteDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRGPSBabelDriverDelete( const char *pszFilename )

{
    if( VSIUnlink( pszFilename ) == 0 )
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                        RegisterOGRGPSBabel()                         */
/************************************************************************/

void RegisterOGRGPSBabel()
{
    if (! GDAL_CHECK_VERSION("OGR/GPSBabel driver"))
        return;

    if( GDALGetDriverByName( "GPSBabel" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GPSBabel" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "GPSBabel" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/gpsbabel.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "mps gdb osm tcx igc" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "GPSBABEL:" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='FILENAME' type='string' description='Filename to open'/>"
"  <Option name='GPSBABEL_DRIVER' type='string' description='Name of the GPSBabel to use'/>"
"</OpenOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='GPSBABEL_DRIVER' type='string' description='Name of the GPSBabel to use'/>"
"</CreationOptionList>");

    poDriver->pfnOpen = OGRGPSBabelDriverOpen;
    poDriver->pfnIdentify = OGRGPSBabelDriverIdentify;
    poDriver->pfnCreate = OGRGPSBabelDriverCreate;
    poDriver->pfnDelete = OGRGPSBabelDriverDelete;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
