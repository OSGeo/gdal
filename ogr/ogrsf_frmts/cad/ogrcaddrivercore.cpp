/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, polimax@mail.ru
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016, NextGIS
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/

#include "ogrsf_frmts.h"

#include "ogrcaddrivercore.h"

/************************************************************************/
/*                    OGRCADDriverIdentify()                            */
/************************************************************************/

static int OGRCADDriverIdentify(GDALOpenInfo *poOpenInfo)

{
#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
    if (!poOpenInfo->IsExtensionEqualToCI("DWG"))
        return FALSE;
#endif
    return poOpenInfo->nHeaderBytes >= 6 && poOpenInfo->pabyHeader[0] == 'A' &&
           poOpenInfo->pabyHeader[1] == 'C';
}

/************************************************************************/
/*                  OGRCADDriverSetCommonMetadata()                  */
/************************************************************************/

void OGRCADDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "AutoCAD Driver");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dwg");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/cad.html");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='MODE' type='string' description='Open mode. "
        "READ_ALL - read all data (slow), READ_FAST - read main data "
        "(fast), READ_FASTEST - read less data' default='READ_FAST'/>"
        "  <Option name='ADD_UNSUPPORTED_GEOMETRIES_DATA' type='string' "
        "description='Add unsupported geometries data (color, attributes) "
        "to the layer (YES/NO). They will have no geometrical "
        "representation.' default='NO'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_READ, "YES");

    poDriver->pfnIdentify = OGRCADDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRCADPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRCADPlugin()
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
    OGRCADDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
