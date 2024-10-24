/******************************************************************************
 *
 * Project:  DRDC Ottawa GEOINT
 * Purpose:  Radarsat Constellation Mission - XML Products (product.xml) driver
 * Author:   Roberto Caron, MDA
 *           on behalf of DRDC Ottawa
 *
 ******************************************************************************
 * Copyright (c) 2020, DRDC Ottawa
 *
 * Based on the RS2 Dataset Class
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "rcmdrivercore.h"

int RCMDatasetIdentify(GDALOpenInfo *poOpenInfo)
{
    /* Check for the case where we're trying to read the calibrated data: */
    CPLString calibrationFormat = FormatCalibration(nullptr, nullptr);

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, calibrationFormat))
    {
        return TRUE;
    }

    if (poOpenInfo->bIsDirectory)
    {
        /* Check for directory access when there is a product.xml file in the
        directory. */
        CPLString osMDFilename =
            CPLFormCIFilename(poOpenInfo->pszFilename, "product.xml", nullptr);

        VSIStatBufL sStat;
        if (VSIStatL(osMDFilename, &sStat) == 0)
        {
            CPLXMLNode *psProduct = CPLParseXMLFile(osMDFilename);
            if (psProduct == nullptr)
                return FALSE;

            CPLXMLNode *psProductAttributes =
                CPLGetXMLNode(psProduct, "=product");
            if (psProductAttributes == nullptr)
            {
                CPLDestroyXMLNode(psProduct);
                return FALSE;
            }

            /* Check the namespace only, should be rs2 */
            const char *szNamespace =
                CPLGetXMLValue(psProductAttributes, "xmlns", "");

            if (strstr(szNamespace, "rcm") == nullptr)
            {
                /* Invalid namespace */
                CPLDestroyXMLNode(psProduct);
                return FALSE;
            }

            CPLDestroyXMLNode(psProduct);
            return TRUE;
        }

        /* If not, check for directory extra 'metadata' access when there is a
        product.xml file in the directory. */

        CPLString osMDFilenameMetadata = CPLFormCIFilename(
            poOpenInfo->pszFilename, GetMetadataProduct(), nullptr);

        VSIStatBufL sStatMetadata;
        if (VSIStatL(osMDFilenameMetadata, &sStatMetadata) == 0)
        {
            CPLXMLNode *psProduct = CPLParseXMLFile(osMDFilenameMetadata);
            if (psProduct == nullptr)
                return FALSE;

            CPLXMLNode *psProductAttributes =
                CPLGetXMLNode(psProduct, "=product");
            if (psProductAttributes == nullptr)
            {
                CPLDestroyXMLNode(psProduct);
                return FALSE;
            }

            /* Check the namespace only, should be rs2 */
            const char *szNamespace =
                CPLGetXMLValue(psProductAttributes, "xmlns", "");

            if (strstr(szNamespace, "rcm") == nullptr)
            {
                /* Invalid namespace */
                CPLDestroyXMLNode(psProduct);
                return FALSE;
            }

            CPLDestroyXMLNode(psProduct);

            return TRUE;
        }

        return FALSE;
    }

    /* otherwise, do our normal stuff */
    if (strlen(poOpenInfo->pszFilename) < 11 ||
        !EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) - 11,
               "product.xml"))
        return FALSE;

    if (poOpenInfo->nHeaderBytes < 100)
        return FALSE;

    /* The RCM schema location is rcm_prod_product.xsd */
    if (strstr((const char *)poOpenInfo->pabyHeader, "/rcm") == nullptr ||
        strstr((const char *)poOpenInfo->pabyHeader, "<product") == nullptr)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                    RCMDriverSetCommonMetadata()                      */
/************************************************************************/

void RCMDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(RCM_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Radarsat Constellation Mission XML Product");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/rcm.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->pfnIdentify = RCMDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
}

/************************************************************************/
/*                     DeclareDeferredRCMPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredRCMPlugin()
{
    if (GDALGetDriverByName(RCM_DRIVER_NAME) != nullptr)
    {
        return;
    }

    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    RCMDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
