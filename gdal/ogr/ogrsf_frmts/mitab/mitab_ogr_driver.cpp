/**********************************************************************
 *
 * Name:     mitab_ogr_driver.cpp
 * Project:  MapInfo Mid/Mif, Tab ogr support
 * Language: C++
 * Purpose:  Implementation of the MIDDATAFile class used to handle
 *           reading/writing of the MID/MIF files
 * Author:   Stephane Villeneuve, stephane.v@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Stephane Villeneuve
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "mitab_ogr_driver.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                  OGRTABDriverIdentify()                              */
/************************************************************************/

static int OGRTABDriverIdentify( GDALOpenInfo *poOpenInfo )

{
    // Files not ending with .tab, .mif or .mid are not handled by this driver.
    if( !poOpenInfo->bStatOK )
        return FALSE;
    if( poOpenInfo->bIsDirectory )
        return -1;  // Unsure.
    if( poOpenInfo->fpL == NULL )
        return FALSE;
    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MIF") ||
        EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MID") )
    {
        return TRUE;
    }
    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "TAB") )
    {
        for( int i = 0; i < poOpenInfo->nHeaderBytes; i++)
        {
            const char *pszLine = (const char *)poOpenInfo->pabyHeader + i;
            if (STARTS_WITH_CI(pszLine, "Fields"))
                return TRUE;
            else if (STARTS_WITH_CI(pszLine, "create view"))
                return TRUE;
            else if (STARTS_WITH_CI(pszLine, "\"\\IsSeamless\" = \"TRUE\""))
                return TRUE;
        }
    }
#ifdef DEBUG
    // For AFL, so that .cur_input is detected as the archive filename.
    if( !STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input") )
    {
        return -1;
    }
#endif
    return FALSE;
}

/************************************************************************/
/*                  OGRTABDriver::Open()                                */
/************************************************************************/

static GDALDataset *OGRTABDriverOpen( GDALOpenInfo *poOpenInfo )

{
    if( OGRTABDriverIdentify(poOpenInfo) == FALSE )
    {
        return NULL;
    }

    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MIF") ||
        EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "MID") )
    {
        if( poOpenInfo->eAccess == GA_Update )
            return NULL;
    }

#ifdef DEBUG
    // For AFL, so that .cur_input is detected as the archive filename.
    if( poOpenInfo->fpL != NULL &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input") )
    {
        GDALOpenInfo oOpenInfo(
            (CPLString("/vsitar/") + poOpenInfo->pszFilename).c_str(),
            poOpenInfo->nOpenFlags);
        oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
        return OGRTABDriverOpen(&oOpenInfo);
    }
#endif

    OGRTABDataSource *poDS = new OGRTABDataSource();
    if( !poDS->Open(poOpenInfo, TRUE) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRTABDriverCreate( const char *pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
    // Try to create the data source.
    OGRTABDataSource *poDS = new OGRTABDataSource();
    if( !poDS->Create(pszName, papszOptions) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

static CPLErr OGRTABDriverDelete( const char *pszDataSource )

{
    GDALDataset *poDS = NULL;
    {
        // Make sure that the file opened by GDALOpenInfo is closed
        // when the object goes out of scope
        GDALOpenInfo oOpenInfo(pszDataSource, GA_ReadOnly);
        poDS = OGRTABDriverOpen(&oOpenInfo);
    }
    if( poDS == NULL )
        return CE_Failure;
    char **papszFileList = poDS->GetFileList();
    delete poDS;

    char **papszIter = papszFileList;
    while( papszIter && *papszIter )
    {
        VSIUnlink(*papszIter);
        papszIter++;
    }
    CSLDestroy(papszFileList);

    VSIStatBufL sStatBuf;
    if( VSIStatL(pszDataSource, &sStatBuf) == 0 && VSI_ISDIR(sStatBuf.st_mode) )
    {
        VSIRmdir(pszDataSource);
    }

    return CE_None;
}

/************************************************************************/
/*                          OGRTABDriverUnload()                        */
/************************************************************************/

static void OGRTABDriverUnload(CPL_UNUSED GDALDriver *poDriver)
{
    MITABFreeCoordSysTable();
}

/************************************************************************/
/*              RegisterOGRTAB()                                        */
/************************************************************************/

void RegisterOGRTAB()

{
    if( GDALGetDriverByName("MapInfo File") != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MapInfo File");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MapInfo File");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "tab mif mid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drv_mitab.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='BOUNDS' type='string' description='Custom bounds. Expect format is xmin,ymin,xmax,ymax'/>"
"  <Option name='ENCODING' type='string' description='to override the encoding interpretation of the DAT/MID with any encoding supported by CPLRecode or to \"\" to avoid any recoding (Neutral charset)'/>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='FORMAT' type='string-select' description='type of MapInfo format'>"
"    <Value>MIF</Value>"
"    <Value>TAB</Value>"
"  </Option>"
"  <Option name='SPATIAL_INDEX_MODE' type='string-select' description='type of spatial index' default='QUICK'>"
"    <Value>QUICK</Value>"
"    <Value>OPTIMIZED</Value>"
"  </Option>"
"  <Option name='BLOCKSIZE' type='int' description='.map block size' min='512' max='32256' default='512'/>"
"  <Option name='ENCODING' type='string' description='to override the encoding interpretation of the DAT/MID with any encoding supported by CPLRecode or to \"\" to avoid any recoding (Neutral charset)'/>"
"</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Real String Date DateTime Time");

    poDriver->pfnOpen = OGRTABDriverOpen;
    poDriver->pfnIdentify = OGRTABDriverIdentify;
    poDriver->pfnCreate = OGRTABDriverCreate;
    poDriver->pfnDelete = OGRTABDriverDelete;
    poDriver->pfnUnloadDriver = OGRTABDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
