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
 * SPDX-License-Identifier: MIT
 **********************************************************************/

#include "mitab_ogr_driver.h"

/************************************************************************/
/*                  OGRTABDriverIdentify()                              */
/************************************************************************/

static int OGRTABDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    // Files not ending with .tab, .mif or .mid are not handled by this driver.
    if (!poOpenInfo->bStatOK)
        return FALSE;
    if (poOpenInfo->bIsDirectory)
        return -1;  // Unsure.
    if (poOpenInfo->fpL == nullptr)
        return FALSE;
    if (poOpenInfo->IsExtensionEqualToCI("MIF") ||
        poOpenInfo->IsExtensionEqualToCI("MID"))
    {
        return TRUE;
    }
    if (poOpenInfo->IsExtensionEqualToCI("TAB"))
    {
        for (int i = 0; i < poOpenInfo->nHeaderBytes; i++)
        {
            const char *pszLine =
                reinterpret_cast<const char *>(poOpenInfo->pabyHeader) + i;
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
    if (!STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input"))
    {
        return -1;
    }
#endif
    return FALSE;
}

/************************************************************************/
/*                  OGRTABDriver::Open()                                */
/************************************************************************/

static GDALDataset *OGRTABDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (OGRTABDriverIdentify(poOpenInfo) == FALSE)
    {
        return nullptr;
    }

    if (poOpenInfo->IsExtensionEqualToCI("MIF") ||
        poOpenInfo->IsExtensionEqualToCI("MID"))
    {
        if (poOpenInfo->eAccess == GA_Update)
            return nullptr;
    }

#ifdef DEBUG
    // For AFL, so that .cur_input is detected as the archive filename.
    if (poOpenInfo->fpL != nullptr &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/") &&
        EQUAL(CPLGetFilename(poOpenInfo->pszFilename), ".cur_input"))
    {
        GDALOpenInfo oOpenInfo(
            (CPLString("/vsitar/") + poOpenInfo->pszFilename).c_str(),
            poOpenInfo->nOpenFlags);
        oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
        return OGRTABDriverOpen(&oOpenInfo);
    }
#endif

    OGRTABDataSource *poDS = new OGRTABDataSource();
    if (!poDS->Open(poOpenInfo, TRUE))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *
OGRTABDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                   CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                   CPL_UNUSED GDALDataType eDT, char **papszOptions)
{
    // Try to create the data source.
    OGRTABDataSource *poDS = new OGRTABDataSource();
    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

static CPLErr OGRTABDriverDelete(const char *pszDataSource)

{
    GDALDataset *poDS = nullptr;
    {
        // Make sure that the file opened by GDALOpenInfo is closed
        // when the object goes out of scope
        GDALOpenInfo oOpenInfo(pszDataSource, GA_ReadOnly);
        poDS = OGRTABDriverOpen(&oOpenInfo);
    }
    if (poDS == nullptr)
        return CE_Failure;
    char **papszFileList = poDS->GetFileList();
    delete poDS;

    char **papszIter = papszFileList;
    while (papszIter && *papszIter)
    {
        VSIUnlink(*papszIter);
        papszIter++;
    }
    CSLDestroy(papszFileList);

    VSIStatBufL sStatBuf;
    if (VSIStatL(pszDataSource, &sStatBuf) == 0 && VSI_ISDIR(sStatBuf.st_mode))
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
    if (GDALGetDriverByName("MapInfo File") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MapInfo File");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_REORDER_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MapInfo File");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "tab mif mid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/mitab.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_NUMERIC_FIELD_WIDTH_INCLUDES_SIGN,
                              "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_NUMERIC_FIELD_WIDTH_INCLUDES_DECIMAL_SEPARATOR, "YES");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='BOUNDS' type='string' "
        "description='Custom bounds. Expect format is "
        "xmin,ymin,xmax,ymax'/>"
        "  <Option name='ENCODING' type='string' "
        "description='to override the encoding "
        "interpretation of the DAT/MID with any encoding "
        "supported by CPLRecode or to \"\" to avoid any "
        "recoding (Neutral charset)'/>"
        "  <Option name='DESCRIPTION' type='string' "
        "description='Friendly name of table. Only for tab "
        "format.'/>"  // See
        // https://support.pitneybowes.com/SearchArticles/VFP05_KnowledgeWithSidebarHowTo?id=kA180000000CtuHCAS&popup=false&lang=en_US
        "  <Option name='STRICT_FIELDS_NAME_LAUNDERING' type='boolean' "
        "default='YES' description='Field name consisting of alphanumeric "
        "only, maximum length 31'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='FORMAT' type='string-select' description='type of "
        "MapInfo format'>"
        "    <Value>MIF</Value>"
        "    <Value>TAB</Value>"
        "  </Option>"
        "  <Option name='SPATIAL_INDEX_MODE' type='string-select' "
        "description='type of spatial index' default='QUICK'>"
        "    <Value>QUICK</Value>"
        "    <Value>OPTIMIZED</Value>"
        "  </Option>"
        "  <Option name='BLOCKSIZE' type='int' description='.map block size' "
        "min='512' max='32256' default='512'/>"
        "  <Option name='ENCODING' type='string' description='to override the "
        "encoding interpretation of the DAT/MID with any encoding supported by "
        "CPLRecode or to \"\" to avoid any recoding (Neutral charset)'/>"
        "  <Option name='STRICT_FIELDS_NAME_LAUNDERING' type='boolean' "
        "default='YES' description='Field name consisting of alphanumeric "
        "only, maximum length 31'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date DateTime Time");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
                              "Name Type WidthPrecision");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_READ, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_WRITE, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS, "Features");

    poDriver->pfnOpen = OGRTABDriverOpen;
    poDriver->pfnIdentify = OGRTABDriverIdentify;
    poDriver->pfnCreate = OGRTABDriverCreate;
    poDriver->pfnDelete = OGRTABDriverDelete;
    poDriver->pfnUnloadDriver = OGRTABDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
