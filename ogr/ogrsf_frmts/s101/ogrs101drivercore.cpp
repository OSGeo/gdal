/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101Driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"
#include "gdalplugindriverproxy.h"

#include "ogrsf_frmts.h"

#include "ogrs101drivercore.h"

/************************************************************************/
/*                       OGRS101DriverIdentify()                        */
/************************************************************************/

int OGRS101DriverIdentify(GDALOpenInfo *poOpenInfo)
{
    constexpr int DDR_LEADER_SIZE = 24;
    if (poOpenInfo->nHeaderBytes < DDR_LEADER_SIZE)
        return false;
    const char *pachLeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    // Per S-100 10a-4.8.2.1 The DDR Leader
    constexpr char DIGIT_ZERO = '0';
    if (pachLeader[5] != '3' ||          // Interchange level
        pachLeader[6] != 'L' ||          // Leader identifier
        pachLeader[7] != 'E' ||          // In line code extension indicator
        pachLeader[8] != '1' ||          // Version number
        pachLeader[9] != ' ' ||          // Application indicator
        pachLeader[10] != DIGIT_ZERO ||  // Field control length
        pachLeader[11] != '9' ||         // Field control length (c'ted)
        pachLeader[17] != ' ' ||         // Extended character set indicator
        pachLeader[18] != '!' ||  // Extended character set indicator(c'ted)
        pachLeader[19] != ' ')    // Extended character set indicator(c'ted)
    {
        return false;
    }
    // Test for S-101 DSID field structure
    return strstr(pachLeader, "DSID") != nullptr &&
           strstr(pachLeader,
                  "RCNM!RCID!ENSP!ENED!PRSP!PRED!PROF!DSNM!DSTL!DSRD!DSLG!"
                  "DSAB!DSED\\\\*DSTC") != nullptr;
}

/************************************************************************/
/*                   OGRS101DriverSetCommonMetadata()                   */
/************************************************************************/

void OGRS101DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);

    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "IHO S-101 (ENC)");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "000");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/s101.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='STRICT' type='boolean' default='YES' "
        "description='Whether the driver should error out as soon as a "
        "non-conformity with respect to the S-101 standard is detected. "
        "In non-strict mode, warnings will be emitted but processing will "
        "continue if the non-conformity is not fatal.'/>"
        "  <Option name='UPDATES' type='string-select' "
        "description='Should update files be "
        "incorporated into the base data on the fly' default='APPLY'>"
        "    <Value>APPLY</Value>"
        "    <Value>IGNORE</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->pfnIdentify = OGRS101DriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                    DeclareDeferredOGRS101Plugin()                    */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRS101Plugin()
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
    OGRS101DriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
