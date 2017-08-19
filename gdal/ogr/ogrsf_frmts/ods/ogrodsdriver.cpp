/******************************************************************************
 *
 * Project:  ODS Translator
 * Purpose:  Implements OGRODSDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_ods.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

using namespace OGRODS;

// g++ -DHAVE_EXPAT -g -Wall -fPIC ogr/ogrsf_frmts/ods/*.cpp -shared
// -o ogr_ODS.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts
// -Iogr/ogrsf_frmts/mem -Iogr/ogrsf_frmts/ods -L. -lgdal

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int OGRODSDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "ODS:") )
        return TRUE;

    if( EQUAL(CPLGetFilename(poOpenInfo->pszFilename), "content.xml"))
    {
        return poOpenInfo->nHeaderBytes != 0 &&
               strstr(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                      "<office:document-content") != NULL;
    }

    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "ODS") &&
        !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "ODS}"))
        return FALSE;

    if( STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") ||
        STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") )
        return poOpenInfo->eAccess == GA_ReadOnly;

    return poOpenInfo->nHeaderBytes > 2 &&
           memcmp(poOpenInfo->pabyHeader, "PK", 2) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRODSDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRODSDriverIdentify(poOpenInfo) )
        return NULL;

    const char* pszFilename = poOpenInfo->pszFilename;
    CPLString osExt(CPLGetExtension(pszFilename));
    CPLString osContentFilename(pszFilename);

    VSILFILE* fpContent = NULL;
    VSILFILE* fpSettings = NULL;

    CPLString osPrefixedFilename("/vsizip/");
    osPrefixedFilename += poOpenInfo->pszFilename;
    if( STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") ||
        STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") )
    {
        osPrefixedFilename = poOpenInfo->pszFilename;
    }

    if (EQUAL(osExt, "ODS") || EQUAL(osExt, "ODS}"))
    {
        osContentFilename.Printf("%s/content.xml", osPrefixedFilename.c_str());
    }
    else if (poOpenInfo->eAccess == GA_Update) /* We cannot update the xml file, only the .ods */
    {
        return NULL;
    }

    if (STARTS_WITH_CI(osContentFilename, "ODS:") ||
        EQUAL(CPLGetFilename(osContentFilename), "content.xml"))
    {
        if (STARTS_WITH_CI(osContentFilename, "ODS:"))
            osContentFilename = osContentFilename.substr(4);

        fpContent = VSIFOpenL(osContentFilename, "rb");
        if (fpContent == NULL)
            return NULL;

        char szBuffer[1024];
        int nRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fpContent);
        szBuffer[nRead] = 0;

        if (strstr(szBuffer, "<office:document-content") == NULL)
        {
            VSIFCloseL(fpContent);
            return NULL;
        }

        /* We could also check that there's a <office:spreadsheet>, but it might be further */
        /* in the XML due to styles, etc... */
    }
    else
    {
        return NULL;
    }

    if (EQUAL(osExt, "ODS") || EQUAL(osExt, "ODS)"))
    {
        fpSettings =
            VSIFOpenL(CPLSPrintf("%s/settings.xml", osPrefixedFilename.c_str()), "rb");
    }

    OGRODSDataSource *poDS = new OGRODSDataSource();

    if( !poDS->Open( pszFilename, fpContent, fpSettings, poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                         OGRODSDriverCreate()                         */
/************************************************************************/

static
GDALDataset *OGRODSDriverCreate( const char *pszName,
                                 int /* nXSize */,
                                 int /* nYSize */,
                                 int /* nBands */,
                                 GDALDataType /* eDT */,
                                 char **papszOptions )

{
    if (!EQUAL(CPLGetExtension(pszName), "ODS"))
    {
        CPLError( CE_Failure, CPLE_AppDefined, "File extension should be ODS" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create datasource.                                       */
/* -------------------------------------------------------------------- */
    OGRODSDataSource *poDS = new OGRODSDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRODS()                           */
/************************************************************************/

void RegisterOGRODS()

{
    if( GDALGetDriverByName( "ODS" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ODS" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                "Open Document/ LibreOffice / "
                               "OpenOffice Spreadsheet " );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ods" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_ods.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime "
                               "Time Binary" );

    poDriver->pfnIdentify = OGRODSDriverIdentify;
    poDriver->pfnOpen = OGRODSDriverOpen;
    poDriver->pfnCreate = OGRODSDriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
