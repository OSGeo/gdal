/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_openfilegdb.h"

#include <cstddef>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"

CPL_CVSID("$Id$")

// g++ -O2 -Wall -Wextra -g -shared -fPIC ogr/ogrsf_frmts/openfilegdb/*.cpp
// -o ogr_OpenFileGDB.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts
// -Iogr/ogrsf_frmts/mem -Iogr/ogrsf_frmts/openfilegdb -L. -lgdal

extern "C" void RegisterOGROpenFileGDB();

#define ENDS_WITH(str, strLen, end) \
    (strLen >= strlen(end) && EQUAL(str + strLen - strlen(end), end))

/************************************************************************/
/*                         OGROpenFileGDBDriverIdentify()               */
/************************************************************************/

static GDALIdentifyEnum OGROpenFileGDBDriverIdentifyInternal( GDALOpenInfo* poOpenInfo,
                                                 const char*& pszFilename )
{
    // FUSIL is a fuzzer
#ifdef FOR_FUSIL
    CPLString osOrigFilename(pszFilename);
#endif

    // First check if we have to do any work.
    size_t nLen = strlen(pszFilename);
    if( ENDS_WITH(pszFilename, nLen, ".gdb") ||
        ENDS_WITH(pszFilename, nLen, ".gdb/") )
    {
        // Check that the filename is really a directory, to avoid confusion
        // with Garmin MapSource - gdb format which can be a problem when the
        // driver is loaded as a plugin, and loaded before the GPSBabel driver
        // (http://trac.osgeo.org/osgeo4w/ticket/245)
        if( STARTS_WITH(pszFilename, "/vsicurl/https://github.com/") ||
            !poOpenInfo->bStatOK ||
            !poOpenInfo->bIsDirectory )
        {
            // In case we do not manage to list the directory, try to stat one
            // file.
            VSIStatBufL stat;
            if( !(STARTS_WITH(pszFilename, "/vsicurl/") &&
                  VSIStatL( CPLFormFilename(
                      pszFilename, "a00000001", "gdbtable"), &stat ) == 0) )
            {
                return GDAL_IDENTIFY_FALSE;
            }
        }
        return GDAL_IDENTIFY_TRUE;
    }
    /* We also accept zipped GDB */
    else if( ENDS_WITH(pszFilename, nLen, ".gdb.zip") ||
             ENDS_WITH(pszFilename, nLen, ".gdb.tar") ||
                /* Canvec GBs */
             (ENDS_WITH(pszFilename, nLen, ".zip") &&
              (strstr(pszFilename, "_gdb") != nullptr ||
               strstr(pszFilename, "_GDB") != nullptr)) )
    {
        return GDAL_IDENTIFY_TRUE;
    }
    /* We also accept tables themselves */
    else if( ENDS_WITH(pszFilename, nLen, ".gdbtable") )
    {
        return GDAL_IDENTIFY_TRUE;
    }
#ifdef FOR_FUSIL
    /* To be able to test fuzzer on any auxiliary files used (indexes, etc.) */
    else if( strlen(CPLGetBasename(pszFilename)) == 9 &&
             CPLGetBasename(pszFilename)[0] == 'a' )
    {
        pszFilename = CPLFormFilename(CPLGetPath(pszFilename),
                                      CPLGetBasename(pszFilename),
                                      "gdbtable");
        return GDAL_IDENTIFY_TRUE;
    }
    else if( strlen(CPLGetBasename(CPLGetBasename(pszFilename))) == 9 &&
             CPLGetBasename(CPLGetBasename(pszFilename))[0] == 'a' )
    {
        pszFilename =
            CPLFormFilename( CPLGetPath(pszFilename),
                             CPLGetBasename(CPLGetBasename(pszFilename)),
                             "gdbtable");
        return GDAL_IDENTIFY_TRUE;
    }
#endif

#ifdef DEBUG
    /* For AFL, so that .cur_input is detected as the archive filename */
    else if( EQUAL(CPLGetFilename(pszFilename), ".cur_input") )
    {
        // This file may be recognized or not by this driver,
        // but there were not enough elements to judge.
        return GDAL_IDENTIFY_UNKNOWN;
    }
#endif

    else if( EQUAL(pszFilename, ".") )
    {
        GDALIdentifyEnum eRet = GDAL_IDENTIFY_FALSE;
        char* pszCurrentDir = CPLGetCurrentDir();
        if( pszCurrentDir )
        {
            const char* pszTmp = pszCurrentDir;
            eRet = OGROpenFileGDBDriverIdentifyInternal(poOpenInfo, pszTmp);
            CPLFree(pszCurrentDir);
        }
        return eRet;
    }

    else
    {
        return GDAL_IDENTIFY_FALSE;
    }
}

static int OGROpenFileGDBDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    const char* pszFilename = poOpenInfo->pszFilename;
    return OGROpenFileGDBDriverIdentifyInternal( poOpenInfo, pszFilename );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset* OGROpenFileGDBDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update )
        return nullptr;

    const char* pszFilename = poOpenInfo->pszFilename;
#ifdef FOR_FUSIL
    CPLString osOrigFilename(pszFilename);
#endif
    if( OGROpenFileGDBDriverIdentifyInternal( poOpenInfo, pszFilename ) == GDAL_IDENTIFY_FALSE )
        return nullptr;

#ifdef FOR_FUSIL
    const char* pszSrcDir = CPLGetConfigOption("FUSIL_SRC_DIR", NULL);
    if( pszSrcDir != NULL && VSIStatL( osOrigFilename, &stat ) == 0 &&
        VSI_ISREG(stat.st_mode) )
    {
        /* Copy all files from FUSIL_SRC_DIR to directory of pszFilename */
        /* except pszFilename itself */
        CPLString osSave(pszFilename);
        char** papszFiles = VSIReadDir(pszSrcDir);
        for(int i=0; papszFiles[i] != NULL; i++)
        {
            if( strcmp(papszFiles[i], CPLGetFilename(osOrigFilename)) != 0 )
            {
                CPLCopyFile(
                    CPLFormFilename(CPLGetPath(osOrigFilename), papszFiles[i],
                                    NULL),
                    CPLFormFilename(pszSrcDir, papszFiles[i], NULL) );
            }
        }
        CSLDestroy(papszFiles);
        pszFilename = CPLFormFilename("", osSave.c_str(), NULL);
    }
#endif

#ifdef DEBUG
    /* For AFL, so that .cur_input is detected as the archive filename */
    if( poOpenInfo->fpL != nullptr &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input") )
    {
        GDALOpenInfo oOpenInfo(
            (CPLString("/vsitar/") + poOpenInfo->pszFilename).c_str(),
            poOpenInfo->nOpenFlags );
        oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
        return OGROpenFileGDBDriverOpen(&oOpenInfo);
    }
#endif

    OGROpenFileGDBDataSource* poDS = new OGROpenFileGDBDataSource();
    if( poDS->Open( poOpenInfo ) )
    {
        return poDS;
    }

    delete poDS;
    return nullptr;
}

/***********************************************************************/
/*                       RegisterOGROpenFileGDB()                      */
/***********************************************************************/

void RegisterOGROpenFileGDB()

{
    if( !GDAL_CHECK_VERSION("OGR OpenFileGDB") )
        return;

    if( GDALGetDriverByName( "OpenFileGDB" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "OpenFileGDB" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ESRI FileGDB" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gdb" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/openfilegdb.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_FIELD_DOMAINS, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList>"
"  <Option name='LIST_ALL_TABLES' type='string-select' scope='vector' description='Whether all tables, including system and internal tables (such as GDB_* tables) should be listed' default='NO'>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"</OpenOptionList>");

    poDriver->pfnOpen = OGROpenFileGDBDriverOpen;
    poDriver->pfnIdentify = OGROpenFileGDBDriverIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
