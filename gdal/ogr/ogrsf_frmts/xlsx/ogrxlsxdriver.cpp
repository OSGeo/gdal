/******************************************************************************
 *
 * Project:  XLSX Translator
 * Purpose:  Implements OGRXLSXDriver.
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

#include "ogr_xlsx.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

extern "C" void RegisterOGRXLSX();

using namespace OGRXLSX;

// g++ -DHAVE_EXPAT -g -Wall -fPIC ogr/ogrsf_frmts/xlsx/*.cpp -shared -o ogr_XLSX.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mem -Iogr/ogrsf_frmts/xlsx -L. -lgdal

static const char XLSX_MIMETYPE[] =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml";

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int OGRXLSXDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    if (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "XLSX") &&
        !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "XLSX}"))
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

static GDALDataset* OGRXLSXDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if (!OGRXLSXDriverIdentify(poOpenInfo) )
        return NULL;

    CPLString osPrefixedFilename("/vsizip/");
    osPrefixedFilename += poOpenInfo->pszFilename;
    if( STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") ||
        STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") )
    {
        osPrefixedFilename = poOpenInfo->pszFilename;
    }

    VSILFILE* fpContent = VSIFOpenL(CPLSPrintf("%s/[Content_Types].xml", osPrefixedFilename.c_str()), "rb");
    if (fpContent == NULL)
        return NULL;

    char szBuffer[2048];
    int nRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fpContent);
    szBuffer[nRead] = 0;

    VSIFCloseL(fpContent);

    if (strstr(szBuffer, XLSX_MIMETYPE) == NULL)
        return NULL;

    VSILFILE* fpWorkbook = VSIFOpenL(CPLSPrintf("%s/xl/workbook.xml", osPrefixedFilename.c_str()), "rb");
    if (fpWorkbook == NULL)
        return NULL;

    VSILFILE* fpWorkbookRels = VSIFOpenL(CPLSPrintf("%s/xl/_rels/workbook.xml.rels", osPrefixedFilename.c_str()), "rb");
    if (fpWorkbookRels == NULL)
    {
        VSIFCloseL(fpWorkbook);
        return NULL;
    }

    VSILFILE* fpSharedStrings = VSIFOpenL(CPLSPrintf("%s/xl/sharedStrings.xml", osPrefixedFilename.c_str()), "rb");
    VSILFILE* fpStyles = VSIFOpenL(CPLSPrintf("%s/xl/styles.xml", osPrefixedFilename.c_str()), "rb");

    OGRXLSXDataSource   *poDS = new OGRXLSXDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, osPrefixedFilename,
                     fpWorkbook, fpWorkbookRels, fpSharedStrings, fpStyles,
                     poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                       OGRXLSXDriverCreate()                          */
/************************************************************************/

static
GDALDataset *OGRXLSXDriverCreate( const char *pszName,
                                 int /* nXSize */,
                                 int /* nYSize */,
                                 int /* nBands */,
                                 GDALDataType /* eDT */,
                                 char **papszOptions )

{
    if (!EQUAL(CPLGetExtension(pszName), "XLSX"))
    {
        CPLError( CE_Failure, CPLE_AppDefined, "File extension should be XLSX" );
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
    OGRXLSXDataSource *poDS = new OGRXLSXDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRXLSX()                           */
/************************************************************************/

void RegisterOGRXLSX()

{
    if( GDALGetDriverByName( "XLSX" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "XLSX" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "MS Office Open XML spreadsheet" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "xlsx" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_xlsx.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime "
                               "Time" );

    poDriver->pfnIdentify = OGRXLSXDriverIdentify;
    poDriver->pfnOpen = OGRXLSXDriverOpen;
    poDriver->pfnCreate = OGRXLSXDriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
