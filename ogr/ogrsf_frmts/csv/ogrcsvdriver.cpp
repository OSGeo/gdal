/******************************************************************************
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_csv.h"

#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

static CPLMutex *hMutex = nullptr;
static std::map<CPLString, GDALDataset *> *poMap = nullptr;

/************************************************************************/
/*                         OGRCSVDriverIdentify()                       */
/************************************************************************/

static int OGRCSVDriverIdentify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->fpL != nullptr )
    {
        const CPLString osBaseFilename =
            CPLGetFilename(poOpenInfo->pszFilename);
        const CPLString osExt =
            OGRCSVDataSource::GetRealExtension(poOpenInfo->pszFilename);

        if( EQUAL(osBaseFilename, "NfdcFacilities.xls") ||
            EQUAL(osBaseFilename, "NfdcRunways.xls") ||
            EQUAL(osBaseFilename, "NfdcRemarks.xls") ||
            EQUAL(osBaseFilename, "NfdcSchedules.xls") )
        {
            return TRUE;
        }
        else if( (STARTS_WITH_CI(osBaseFilename, "NationalFile_") ||
                  STARTS_WITH_CI(osBaseFilename, "POP_PLACES_") ||
                  STARTS_WITH_CI(osBaseFilename, "HIST_FEATURES_") ||
                  STARTS_WITH_CI(osBaseFilename, "US_CONCISE_") ||
                  STARTS_WITH_CI(osBaseFilename, "AllNames_") ||
                  STARTS_WITH_CI(osBaseFilename,
                                 "Feature_Description_History_") ||
                  STARTS_WITH_CI(osBaseFilename, "ANTARCTICA_") ||
                  STARTS_WITH_CI(osBaseFilename, "GOVT_UNITS_") ||
                  STARTS_WITH_CI(osBaseFilename, "NationalFedCodes_") ||
                  STARTS_WITH_CI(osBaseFilename, "AllStates_") ||
                  STARTS_WITH_CI(osBaseFilename, "AllStatesFedCodes_") ||
                  (osBaseFilename.size() > 2 &&
                   STARTS_WITH_CI(osBaseFilename + 2, "_Features_")) ||
                  (osBaseFilename.size() > 2 &&
                   STARTS_WITH_CI(osBaseFilename + 2, "_FedCodes_"))) &&
                 (EQUAL(osExt, "txt") || EQUAL(osExt, "zip")) )
        {
            return TRUE;
        }
        else if( EQUAL(osBaseFilename, "allCountries.txt") ||
                 EQUAL(osBaseFilename, "allCountries.zip") )
        {
            return TRUE;
        }
        else if( EQUAL(osExt, "csv") || EQUAL(osExt, "tsv") )
        {
            return TRUE;
        }
        else if( STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") &&
                 EQUAL(osExt, "zip") )
        {
            return -1;  // Unsure.
        }
        else
        {
            return FALSE;
        }
    }
    else if( STARTS_WITH_CI(poOpenInfo->pszFilename, "CSV:") )
    {
        return TRUE;
    }
    else if( poOpenInfo->bIsDirectory )
    {
        return -1;  // Unsure.
    }

    return FALSE;
}

/************************************************************************/
/*                        OGRCSVDriverRemoveFromMap()                   */
/************************************************************************/

void OGRCSVDriverRemoveFromMap(const char *pszName, GDALDataset *poDS)
{
    if( poMap == nullptr )
        return;
    CPLMutexHolderD(&hMutex);
    std::map<CPLString, GDALDataset *>::iterator oIter = poMap->find(pszName);
    if( oIter != poMap->end() )
    {
        GDALDataset *poOtherDS = oIter->second;
        if( poDS == poOtherDS )
            poMap->erase(oIter);
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRCSVDriverOpen( GDALOpenInfo *poOpenInfo )

{
    if( !OGRCSVDriverIdentify(poOpenInfo) )
        return nullptr;

    if( poMap != nullptr )
    {
        CPLMutexHolderD(&hMutex);
        std::map<CPLString, GDALDataset *>::iterator oIter =
            poMap->find(poOpenInfo->pszFilename);
        if( oIter != poMap->end() )
        {
            GDALDataset *poOtherDS = oIter->second;
            poOtherDS->FlushCache(false);
        }
    }

    OGRCSVDataSource *poDS = new OGRCSVDataSource();

    if( !poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update,
                    FALSE, poOpenInfo->papszOpenOptions) )
    {
        delete poDS;
        poDS = nullptr;
    }

    if( poOpenInfo->eAccess == GA_Update && poDS != nullptr )
    {
        CPLMutexHolderD(&hMutex);
        if( poMap == nullptr )
            poMap = new std::map<CPLString, GDALDataset *>();
        if( poMap->find(poOpenInfo->pszFilename) == poMap->end() )
        {
            (*poMap)[poOpenInfo->pszFilename] = poDS;
        }
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRCSVDriverCreate( const char *pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
    // First, ensure there isn't any such file yet.
    VSIStatBufL sStatBuf;

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    if( VSIStatL(pszName, &sStatBuf) == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "It seems a file system object called '%s' already exists.",
                 pszName);

        return nullptr;
    }

    // If the target is not a simple .csv then create it as a directory.
    CPLString osDirName;

    if( EQUAL(CPLGetExtension(pszName), "csv") )
    {
        osDirName = CPLGetPath(pszName);
        if( osDirName == "" )
            osDirName = ".";

        // HACK: CPLGetPath("/vsimem/foo.csv") = "/vsimem", but this is not
        // recognized afterwards as a valid directory name.
        if( osDirName == "/vsimem" )
            osDirName = "/vsimem/";
    }
    else
    {
        if( STARTS_WITH(pszName, "/vsizip/"))
        {
            // Do nothing.
        }
        else if( !EQUAL(pszName, "/vsistdout/") &&
                 VSIMkdir(pszName, 0755) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create directory %s:\n%s",
                     pszName, VSIStrerror(errno));
            return nullptr;
        }
        osDirName = pszName;
    }

    // Force it to open as a datasource.
    OGRCSVDataSource *poDS = new OGRCSVDataSource();

    if( EQUAL(CPLGetExtension(pszName), "csv") )
    {
        poDS->CreateForSingleFile(osDirName, pszName);
    }
    else if( !poDS->Open(osDirName, TRUE, TRUE) )
    {
        delete poDS;
        return nullptr;
    }

    const char *pszGeometry = CSLFetchNameValue(papszOptions, "GEOMETRY");
    if( pszGeometry != nullptr && EQUAL(pszGeometry, "AS_WKT") )
        poDS->EnableGeometryFields();

    return poDS;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

static CPLErr OGRCSVDriverDelete( const char *pszFilename )

{
    return CPLUnlinkTree(pszFilename) == 0 ? CE_None : CE_Failure;
}

/************************************************************************/
/*                           OGRCSVDriverUnload()                       */
/************************************************************************/

static void OGRCSVDriverUnload( GDALDriver * )
{
    if( hMutex != nullptr )
        CPLDestroyMutex(hMutex);
    hMutex = nullptr;
    delete poMap;
    poMap = nullptr;
}

/************************************************************************/
/*                           RegisterOGRCSV()                           */
/************************************************************************/

void RegisterOGRCSV()

{
    if( GDALGetDriverByName("CSV") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("CSV");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Comma Separated Value (.csv)");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "csv");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/csv.html");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='GEOMETRY' type='string-select' description='how to encode geometry fields'>"
"    <Value>AS_WKT</Value>"
"  </Option>"
"</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='SEPARATOR' type='string-select' description='field separator' default='COMMA'>"
"    <Value>COMMA</Value>"
"    <Value>SEMICOLON</Value>"
"    <Value>TAB</Value>"
"    <Value>SPACE</Value>"
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
"  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column. Only used if GEOMETRY=AS_WKT' default='WKT'/>"
"  <Option name='STRING_QUOTING' type='string-select' description='whether to double-quote strings. IF_AMBIGUOUS means that string values that look like numbers will be quoted (it also implies IF_NEEDED).' default='IF_AMBIGUOUS'>"
"    <Value>IF_NEEDED</Value>"
"    <Value>IF_AMBIGUOUS</Value>"
"    <Value>ALWAYS</Value>"
"  </Option>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
#if 0
"  <Option name='SEPARATOR' type='string-select' description='field separator' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>COMMA</Value>"
"    <Value>SEMICOLON</Value>"
"    <Value>TAB</Value>"
"    <Value>SPACE</Value>"
"  </Option>"
#endif
"  <Option name='MERGE_SEPARATOR' type='boolean' description='whether to merge consecutive separators' default='NO'/>"
"  <Option name='AUTODETECT_TYPE' type='boolean' description='whether to guess data type from first bytes of the file' default='NO'/>"
"  <Option name='KEEP_SOURCE_COLUMNS' type='boolean' description='whether to add original columns whose guessed data type is not String. Only used if AUTODETECT_TYPE=YES' default='NO'/>"
"  <Option name='AUTODETECT_WIDTH' type='string-select' description='whether to auto-detect width/precision. Only used if AUTODETECT_TYPE=YES' default='NO'>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"    <Value>STRING_ONLY</Value>"
"  </Option>"
"  <Option name='AUTODETECT_SIZE_LIMIT' type='int' description='number of bytes to inspect for auto-detection of data type. Only used if AUTODETECT_TYPE=YES' default='1000000'/>"
"  <Option name='QUOTED_FIELDS_AS_STRING' type='boolean' description='Only used if AUTODETECT_TYPE=YES. Whether to enforce quoted fields as string fields.' default='NO'/>"
"  <Option name='X_POSSIBLE_NAMES' type='string' description='Comma separated list of possible names for X/longitude coordinate of a point.'/>"
"  <Option name='Y_POSSIBLE_NAMES' type='string' description='Comma separated list of possible names for Y/latitude coordinate of a point.'/>"
"  <Option name='Z_POSSIBLE_NAMES' type='string' description='Comma separated list of possible names for Z/elevation coordinate of a point.'/>"
"  <Option name='GEOM_POSSIBLE_NAMES' type='string' description='Comma separated list of possible names for geometry columns.' default='WKT'/>"
"  <Option name='KEEP_GEOM_COLUMNS' type='boolean' description='whether to add original x/y/geometry columns as regular fields.' default='YES'/>"
"  <Option name='HEADERS' type='string-select' description='Whether the first line of the file contains column names or not' default='AUTO'>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"    <Value>AUTO</Value>"
"  </Option>"
"  <Option name='EMPTY_STRING_AS_NULL' type='boolean' description='Whether to consider empty strings as null fields on reading' default='NO'/>"
"</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime "
                              "Time IntegerList Integer64List RealList "
                              "StringList");
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean Int16 Float32" );

    poDriver->pfnOpen = OGRCSVDriverOpen;
    poDriver->pfnIdentify = OGRCSVDriverIdentify;
    poDriver->pfnCreate = OGRCSVDriverCreate;
    poDriver->pfnDelete = OGRCSVDriverDelete;
    poDriver->pfnUnloadDriver = OGRCSVDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
