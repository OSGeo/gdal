/******************************************************************************
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  NITFDataset and driver related implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "nitfdrivercore.h"

/************************************************************************/
/*                     NITFDriverIdentify()                             */
/************************************************************************/

int NITFDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;

    /* -------------------------------------------------------------------- */
    /*      Is this a dataset selector? If so, it is obviously NITF.        */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszFilename, "NITF_IM:"))
        return TRUE;

    /* -------------------------------------------------------------------- */
    /*      Avoid that on Windows, JPEG_SUBFILE:x,y,z,data/../tmp/foo.ntf   */
    /*      to be recognized by the NITF driver, because                    */
    /*      'JPEG_SUBFILE:x,y,z,data' is considered as a (valid) directory  */
    /*      and thus the whole filename is evaluated as tmp/foo.ntf         */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszFilename, "JPEG_SUBFILE:"))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      First we check to see if the file has the expected header       */
    /*      bytes.                                                          */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 4)
        return FALSE;

    if (!STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NITF") &&
        !STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NSIF") &&
        !STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NITF"))
        return FALSE;

    /* Check that it is not in fact a NITF A.TOC file, which is handled by the
     * RPFTOC driver */
    for (int i = 0; i < static_cast<int>(poOpenInfo->nHeaderBytes) -
                            static_cast<int>(strlen("A.TOC"));
         i++)
    {
        if (STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader + i, "A.TOC"))
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                     NITFDriverSetCommonMetadata()                    */
/************************************************************************/

void NITFDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(NITF_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "National Imagery Transmission Format");

    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/nitf.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "ntf");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 UInt32 Int32 Float32");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='VALIDATE' type='boolean' description='Whether "
        "validation of metadata should be done' default='NO' />"
        "  <Option name='FAIL_IF_VALIDATION_ERROR' type='boolean' "
        "description='Whether a validation error should cause dataset opening "
        "to fail' default='NO' />"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = NITFDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS, "RasterValues");
}

/************************************************************************/
/*                     RPFTOCDriverIdentify()                           */
/************************************************************************/

int RPFTOCDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;

    /* -------------------------------------------------------------------- */
    /*      Is this a sub-dataset selector? If so, it is obviously RPFTOC.  */
    /* -------------------------------------------------------------------- */

    if (STARTS_WITH_CI(pszFilename, "NITF_TOC_ENTRY:"))
        return TRUE;

    /* -------------------------------------------------------------------- */
    /*      First we check to see if the file has the expected header       */
    /*      bytes.                                                          */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 48)
        return FALSE;

    if (RPFTOCIsNonNITFFileTOC(poOpenInfo, pszFilename))
        return TRUE;

    if (!STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NITF") &&
        !STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NSIF") &&
        !STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NITF"))
        return FALSE;

    /* If it is a NITF A.TOC file, it must contain A.TOC in its header */
    for (int i = 0; i < static_cast<int>(poOpenInfo->nHeaderBytes) -
                            static_cast<int>(strlen("A.TOC"));
         i++)
    {
        if (STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader + i, "A.TOC"))
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                          IsNonNITFFileTOC()                          */
/************************************************************************/

/* Check whether the file is a TOC file without NITF header */
int RPFTOCIsNonNITFFileTOC(GDALOpenInfo *poOpenInfo, const char *pszFilename)
{
    const char pattern[] = {0,   0,   '0', ' ', ' ', ' ', ' ', ' ',
                            ' ', ' ', 'A', '.', 'T', 'O', 'C'};
    if (poOpenInfo)
    {
        if (poOpenInfo->nHeaderBytes < 48)
            return FALSE;
        return memcmp(pattern, poOpenInfo->pabyHeader, 15) == 0;
    }
    else
    {
        VSILFILE *fp = VSIFOpenL(pszFilename, "rb");
        if (fp == nullptr)
        {
            return FALSE;
        }

        char buffer[48];
        int ret = (VSIFReadL(buffer, 1, 48, fp) == 48) &&
                  memcmp(pattern, buffer, 15) == 0;
        CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
        return ret;
    }
}

/************************************************************************/
/*                     RPFTOCDriverSetCommonMetadata()                  */
/************************************************************************/

void RPFTOCDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(RPFTOC_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Raster Product Format TOC format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/rpftoc.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "toc");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->pfnIdentify = RPFTOCDriverIdentify;
}

/************************************************************************/
/*                     ECRGTOCDriverIdentify()                          */
/************************************************************************/

int ECRGTOCDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;

    /* -------------------------------------------------------------------- */
    /*      Is this a sub-dataset selector? If so, it is obviously ECRGTOC. */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszFilename, "ECRG_TOC_ENTRY:"))
        return TRUE;

    /* -------------------------------------------------------------------- */
    /*  First we check to see if the file has the expected header           */
    /*  bytes.                                                              */
    /* -------------------------------------------------------------------- */
    const char *pabyHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if (pabyHeader == nullptr)
        return FALSE;

    if (strstr(pabyHeader, "<Table_of_Contents") != nullptr &&
        strstr(pabyHeader, "<file_header ") != nullptr)
        return TRUE;

    if (strstr(pabyHeader, "<!DOCTYPE Table_of_Contents [") != nullptr)
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                     ECRGTOCDriverSetCommonMetadata()                 */
/************************************************************************/

void ECRGTOCDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(ECRGTOC_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ECRG TOC format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/ecrgtoc.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "xml");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->pfnIdentify = ECRGTOCDriverIdentify;
}

/************************************************************************/
/*                    DeclareDeferredNITFPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredNITFPlugin()
{
    if (GDALGetDriverByName(NITF_DRIVER_NAME) != nullptr)
    {
        return;
    }

    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        NITFDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }

    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        RPFTOCDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }

    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        ECRGTOCDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
}
#endif
