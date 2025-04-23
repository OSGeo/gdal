/******************************************************************************
 *
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Implementation of OGRXODRDriverCore.
 * Author:   Michael Scholz, German Aerospace Center (DLR)
 *           Gülsen Bardak, German Aerospace Center (DLR)
 *
 ******************************************************************************
 * Copyright 2024 German Aerospace Center (DLR), Institute of Transportation Systems
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"
#include "ogrxodrdrivercore.h"

/************************************************************************/
/*                    OGRXODRDriverIdentify()                           */
/************************************************************************/

int OGRXODRDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    return poOpenInfo->fpL != nullptr &&
           poOpenInfo->IsExtensionEqualToCI("xodr") &&
           !STARTS_WITH(poOpenInfo->pszFilename, "/vsi");
}

/************************************************************************/
/*                  OGRXODRDriverSetCommonMetadata()                    */
/************************************************************************/

void OGRXODRDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(
        GDAL_DMD_LONGNAME,
        "OpenDRIVE - Open Dynamic Road Information for Vehicle Environment");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "xodr");
    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='EPSILON' type='float' description='Epsilon value for "
        "linear approximation of continuous OpenDRIVE geometries.' "
        "default='1.0'/>"
        "  <Option name='DISSOLVE_TIN' type='boolean' description='Whether to "
        "dissolve triangulated surfaces.' default= 'NO'/>"
        "</OpenOptionList>");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->pfnIdentify = OGRXODRDriverIdentify;
}

/************************************************************************/
/*                   DeclareDeferredOGRXODRPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRXODRPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    OGRXODRDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
