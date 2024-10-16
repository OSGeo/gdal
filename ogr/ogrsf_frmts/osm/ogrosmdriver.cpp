/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROSMDriver class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_osm.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_port.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"

/* g++ -DHAVE_EXPAT -fPIC -g -Wall ogr/ogrsf_frmts/osm/ogrosmdriver.cpp
 * ogr/ogrsf_frmts/osm/ogrosmdatasource.cpp ogr/ogrsf_frmts/osm/ogrosmlayer.cpp
 * -Iport -Igcore -Iogr -Iogr/ogrsf_frmts/osm -Iogr/ogrsf_frmts/mitab
 * -Iogr/ogrsf_frmts -shared -o ogr_OSM.so -L. -lgdal */

/************************************************************************/
/*                      OGROSMDriverIdentify()                          */
/************************************************************************/

static int OGROSMDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes == 0)
        return GDAL_IDENTIFY_FALSE;

    if (strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
               "<osm") != nullptr)
    {
        return GDAL_IDENTIFY_TRUE;
    }

    const int nLimitI =
        poOpenInfo->nHeaderBytes - static_cast<int>(strlen("OSMHeader"));
    for (int i = 0; i < nLimitI; i++)
    {
        if (memcmp(poOpenInfo->pabyHeader + i, "OSMHeader",
                   strlen("OSMHeader")) == 0)
        {
            return GDAL_IDENTIFY_TRUE;
        }
    }

    return GDAL_IDENTIFY_FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGROSMDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update)
        return nullptr;
    if (OGROSMDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    OGROSMDataSource *poDS = new OGROSMDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                        RegisterOGROSM()                           */
/************************************************************************/

void RegisterOGROSM()
{
    if (!GDAL_CHECK_VERSION("OGR/OSM driver"))
        return;

    if (GDALGetDriverByName("OSM") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("OSM");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "OpenStreetMap XML and PBF");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "osm pbf");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/osm.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='CONFIG_FILE' type='string' description='Configuration "
        "filename.'/>"
        "  <Option name='USE_CUSTOM_INDEXING' type='boolean' "
        "description='Whether to enable custom indexing.' default='YES'/>"
        "  <Option name='COMPRESS_NODES' type='boolean' description='Whether "
        "to compress nodes in temporary DB.' default='NO'/>"
        "  <Option name='MAX_TMPFILE_SIZE' type='int' description='Maximum "
        "size in MB of in-memory temporary file. If it exceeds that value, it "
        "will go to disk' default='100'/>"
        "  <Option name='INTERLEAVED_READING' type='boolean' "
        "description='Whether to enable interleaved reading.' default='NO'/>"
        "  <Option name='TAGS_FORMAT' type='string-select' "
        "description='Format for all_tags/other_tags fields.' default='HSTORE'>"
        "    <Value>HSTORE</Value>"
        "    <Value>JSON</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->pfnOpen = OGROSMDriverOpen;
    poDriver->pfnIdentify = OGROSMDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
