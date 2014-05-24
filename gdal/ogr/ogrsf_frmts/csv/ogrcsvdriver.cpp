/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_csv.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRCSVDriverIdentify()                       */
/************************************************************************/

static int OGRCSVDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->fpL != NULL )
    {
        CPLString osBaseFilename = CPLGetFilename(poOpenInfo->pszFilename);
        CPLString osExt = OGRCSVDataSource::GetRealExtension(poOpenInfo->pszFilename);

        if (EQUAL(osBaseFilename, "NfdcFacilities.xls") ||
            EQUAL(osBaseFilename, "NfdcRunways.xls") ||
            EQUAL(osBaseFilename, "NfdcRemarks.xls") ||
            EQUAL(osBaseFilename, "NfdcSchedules.xls"))
        {
            return TRUE;
        }
        else if ((EQUALN(osBaseFilename, "NationalFile_", 13) ||
              EQUALN(osBaseFilename, "POP_PLACES_", 11) ||
              EQUALN(osBaseFilename, "HIST_FEATURES_", 14) ||
              EQUALN(osBaseFilename, "US_CONCISE_", 11) ||
              EQUALN(osBaseFilename, "AllNames_", 9) ||
              EQUALN(osBaseFilename, "Feature_Description_History_", 28) ||
              EQUALN(osBaseFilename, "ANTARCTICA_", 11) ||
              EQUALN(osBaseFilename, "GOVT_UNITS_", 11) ||
              EQUALN(osBaseFilename, "NationalFedCodes_", 17) ||
              EQUALN(osBaseFilename, "AllStates_", 10) ||
              EQUALN(osBaseFilename, "AllStatesFedCodes_", 18) ||
              (strlen(osBaseFilename) > 2 && EQUALN(osBaseFilename+2, "_Features_", 10)) ||
              (strlen(osBaseFilename) > 2 && EQUALN(osBaseFilename+2, "_FedCodes_", 10))) &&
             (EQUAL(osExt, "txt") || EQUAL(osExt, "zip")) )
        {
            return TRUE;
        }
        else if (EQUAL(osBaseFilename, "allCountries.txt") ||
             EQUAL(osBaseFilename, "allCountries.zip"))
        {
            return TRUE;
        }
        else if (EQUAL(osExt,"csv") || EQUAL(osExt,"tsv"))
        {
            return TRUE;
        }
        else if (strncmp(poOpenInfo->pszFilename, "/vsizip/", 8) == 0 &&
                 EQUAL(osExt,"zip"))
        {
            return -1; /* unsure */
        }
        else
        {
            return FALSE;
        }
    }
    else if( EQUALN(poOpenInfo->pszFilename, "CSV:", 4) )
    {
        return TRUE;
    }
    else if ( poOpenInfo->bIsDirectory )
    {
        return -1; /* unsure */
    }
    else
        return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRCSVDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( OGRCSVDriverIdentify(poOpenInfo) == FALSE )
        return NULL;

    OGRCSVDataSource   *poDS = new OGRCSVDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update, FALSE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRCSVDriverCreate( const char * pszName,
                                    int nBands, int nXSize, int nYSize, GDALDataType eDT,
                                    char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      First, ensure there isn't any such file yet.                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    if( VSIStatL( pszName, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "It seems a file system object called '%s' already exists.",
                  pszName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      If the target is not a simple .csv then create it as a          */
/*      directory.                                                      */
/* -------------------------------------------------------------------- */
    CPLString osDirName;

    if( EQUAL(CPLGetExtension(pszName),"csv") )
    {
        osDirName = CPLGetPath(pszName);
        if( osDirName == "" )
            osDirName = ".";

        /* HACK: CPLGetPath("/vsimem/foo.csv") = "/vsimem", but this is not */
        /* recognized afterwards as a valid directory name */
        if( osDirName == "/vsimem" )
            osDirName = "/vsimem/";
    }
    else
    {
        if( strncmp(pszName, "/vsizip/", 8) == 0)
        {
            /* do nothing */
        }
        else if( !EQUAL(pszName, "/vsistdout/") &&
            VSIMkdir( pszName, 0755 ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to create directory %s:\n%s", 
                      pszName, VSIStrerror( errno ) );
            return NULL;
        }
        osDirName = pszName;
    }

/* -------------------------------------------------------------------- */
/*      Force it to open as a datasource.                               */
/* -------------------------------------------------------------------- */
    OGRCSVDataSource   *poDS = new OGRCSVDataSource();

    if( !poDS->Open( osDirName, TRUE, TRUE ) )
    {
        delete poDS;
        return NULL;
    }

    if( osDirName != pszName )
        poDS->SetDefaultCSVName( CPLGetFilename(pszName) );
    
    const char *pszGeometry = CSLFetchNameValue( papszOptions, "GEOMETRY");
    if (pszGeometry != NULL && EQUAL(pszGeometry, "AS_WKT"))
        poDS->EnableGeometryFields();

    return poDS;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

static CPLErr OGRCSVDriverDelete( const char *pszFilename )

{
    if( CPLUnlinkTree( pszFilename ) == 0 )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRCSV()                           */
/************************************************************************/

void RegisterOGRCSV()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "CSV" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "CSV" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Comma Separated Value (.csv)" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "csv" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_csv.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='GEOMETRY' type='string-select' description='how to encode geometry fields'>"
"    <Value>AS_WKT</Value>"
"  </Option>"
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='SEPARATOR' type='string-select' description='field separator' default='COMMA'>"
"    <Value>COMMA</Value>"
"    <Value>SEMICOLON</Value>"
"    <Value>TAB</Value>"
"  </Option>"
#ifdef WIN32
"  <Option name='LINEFORMAT' type='string-select' description='end-of-line sequence' default='CRLF'>"
#else
"  <Option name='LINEFORMAT' type='string-select' description='end-of-line sequence' default='LF'>"
#endif
"    <Value>CRLF</Value>"
"    <Value>LF</Value>"
"  </Option>"
"  <Option name='GEOMETRY' type='string-select' description='how to encode geometry fields'>"
"    <Value>AS_WKT</Value>"
"    <Value>AS_XYZ</Value>"
"    <Value>AS_XY</Value>"
"    <Value>AS_YX</Value>"
"  </Option>"
"  <Option name='CREATE_CSVT' type='boolean' description='whether to create a .csvt file' default='NO'/>"
"  <Option name='WRITE_BOM' type='boolean' description='whether to write a UTF-8 BOM prefix' default='NO'/>"
"</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRCSVDriverOpen;
        poDriver->pfnIdentify = OGRCSVDriverIdentify;
        poDriver->pfnCreate = OGRCSVDriverCreate;
        poDriver->pfnDelete = OGRCSVDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

