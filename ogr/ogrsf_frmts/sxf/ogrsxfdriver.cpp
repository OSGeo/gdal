/******************************************************************************
 *
 * Project:  SXF Translator
 * Purpose:  Definition of classes for OGR SXF driver.
 * Author:   Ben Ahmed Daho Ali, bidandou(at)yahoo(dot)fr
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2011, Ben Ahmed Daho Ali
 * Copyright (c) 2013, NextGIS
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_sxf.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSXFDriverOpen(GDALOpenInfo *poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      Determine what sort of object this is.                          */
    /* -------------------------------------------------------------------- */

    VSIStatBufL sStatBuf;
    if (!poOpenInfo->IsExtensionEqualToCI("sxf") ||
        VSIStatL(poOpenInfo->pszFilename, &sStatBuf) != 0 ||
        !VSI_ISREG(sStatBuf.st_mode))
        return nullptr;

    OGRSXFDataSource *poDS = new OGRSXFDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update,
                    poOpenInfo->papszOpenOptions))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int OGRSXFDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (!poOpenInfo->IsExtensionEqualToCI("sxf") || !poOpenInfo->bStatOK ||
        poOpenInfo->bIsDirectory)
    {
        return GDAL_IDENTIFY_FALSE;
    }

    if (poOpenInfo->nHeaderBytes < 4)
    {
        return GDAL_IDENTIFY_UNKNOWN;
    }

    if (0 != memcmp(poOpenInfo->pabyHeader, "SXF", 3))
    {
        return GDAL_IDENTIFY_FALSE;
    }

    return GDAL_IDENTIFY_TRUE;
}

/************************************************************************/
/*                          GRSXFDriverDelete()                         */
/************************************************************************/

static CPLErr OGRSXFDriverDelete(const char *pszName)
{
    // TODO: add more extensions if aplicable
    static const char *const apszExtensions[] = {"szf", "rsc", "SZF", "RSC",
                                                 nullptr};

    VSIStatBufL sStatBuf;
    if (VSIStatL(pszName, &sStatBuf) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s does not appear to be a valid sxf file.", pszName);

        return CE_Failure;
    }

    for (int iExt = 0; apszExtensions[iExt] != nullptr; iExt++)
    {
        const std::string osFile =
            CPLResetExtensionSafe(pszName, apszExtensions[iExt]);
        if (VSIStatL(osFile.c_str(), &sStatBuf) == 0)
            VSIUnlink(osFile.c_str());
    }

    return CE_None;
}

/************************************************************************/
/*                        RegisterOGRSXF()                       */
/************************************************************************/
void RegisterOGRSXF()
{
    if (GDALGetDriverByName("SXF") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("SXF");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Storage and eXchange Format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/sxf.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "sxf");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='SXF_LAYER_FULLNAME' type='string' description='Use "
        "long layer names' default='NO'/>"
        "  <Option name='SXF_RSC_FILENAME' type='string' description='RSC file "
        "name' default=''/>"
        "  <Option name='SXF_SET_VERTCS' type='string' description='Layers "
        "spatial reference will include vertical coordinate system description "
        "if exist' default='NO'/>"
        "</OpenOptionList>");

    poDriver->pfnOpen = OGRSXFDriverOpen;
    poDriver->pfnDelete = OGRSXFDriverDelete;
    poDriver->pfnIdentify = OGRSXFDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
