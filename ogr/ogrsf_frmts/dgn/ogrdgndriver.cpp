/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRDGNDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dgn.h"
#include "cpl_conv.h"

/************************************************************************/
/*                       OGRDGNDriverIdentify()                         */
/************************************************************************/

static int OGRDGNDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->fpL != nullptr && poOpenInfo->nHeaderBytes >= 512 &&
        DGNTestOpen(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes))
    {
        return TRUE;
    }

    // Is this is a DGNv8 file ? If so, and if the DGNV8 driver is not
    // available, and we are called from GDALError(), emit an explicit
    // error.
    VSIStatBuf sStat;
    if ((poOpenInfo->nOpenFlags & GDAL_OF_FROM_GDALOPEN) != 0 &&
        poOpenInfo->papszAllowedDrivers == nullptr &&
        poOpenInfo->fpL != nullptr && poOpenInfo->nHeaderBytes >= 512 &&
        memcmp(poOpenInfo->pabyHeader, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) ==
            0 &&
        poOpenInfo->IsExtensionEqualToCI("DGN") &&
        VSIStat(poOpenInfo->pszFilename, &sStat) == 0 &&
        GDALGetDriverByName("DGNV8") == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "`%s' recognized as a DGNv8 dataset, but the DGNv8 driver is "
                 "not available in this GDAL build. Consult "
                 "https://gdal.org/drivers/vector/dgnv8.html",
                 poOpenInfo->pszFilename);
    }
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDGNDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRDGNDriverIdentify(poOpenInfo))
        return nullptr;

    OGRDGNDataSource *poDS = new OGRDGNDataSource();

    if (!poDS->Open(poOpenInfo) || poDS->GetLayerCount() == 0)
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRDGNDriverCreate(const char *, int /* nBands */,
                                       int /* nXSize */, int /* nYSize */,
                                       GDALDataType /* eDT */,
                                       char **papszOptions)
{
    OGRDGNDataSource *poDS = new OGRDGNDataSource();
    poDS->PreCreate(papszOptions);
    return poDS;
}

/************************************************************************/
/*                          RegisterOGRDGN()                            */
/************************************************************************/

void RegisterOGRDGN()

{
    if (GDALGetDriverByName("DGN") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("DGN");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Microstation DGN");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dgn");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/dgn.html");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='ENCODING' type='string' description="
        "'Encoding name, as supported by iconv'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='3D' type='boolean' description='whether 2D "
        "(seed_2d.dgn) or 3D (seed_3d.dgn) seed file should be used. This "
        "option is ignored if the SEED option is provided'/>"
        "  <Option name='SEED' type='string' description='Filename of seed "
        "file to use'/>"
        "  <Option name='COPY_WHOLE_SEED_FILE' type='boolean' "
        "description='whether the whole seed file should be copied. If not, "
        "only the first three elements (and potentially the color table) will "
        "be copied.' default='NO'/>"
        "  <Option name='COPY_SEED_FILE_COLOR_TABLE' type='boolean' "
        "description='whether the color table should be copied from the seed "
        "file.' default='NO'/>"
        "  <Option name='MASTER_UNIT_NAME' type='string' description='Override "
        "the master unit name from the seed file with the provided one or two "
        "character unit name.'/>"
        "  <Option name='SUB_UNIT_NAME' type='string' description='Override "
        "the master unit name from the seed file with the provided one or two "
        "character unit name.'/>"
        "  <Option name='MASTER_UNIT_NAME' type='string' description='Override "
        "the master unit name from the seed file with the provided one or two "
        "character unit name.'/>"
        "  <Option name='SUB_UNIT_NAME' type='string' description='Override "
        "the sub unit name from the seed file with the provided one or two "
        "character unit name.'/>"
        "  <Option name='SUB_UNITS_PER_MASTER_UNIT' type='int' "
        "description='Override the number of subunits per master unit. By "
        "default the seed file value is used.'/>"
        "  <Option name='UOR_PER_SUB_UNIT' type='int' description='Override "
        "the number of UORs (Units of Resolution) per sub unit. By default the "
        "seed file value is used.'/>"
        "  <Option name='ORIGIN' type='string' description='Value as x,y,z. "
        "Override the origin of the design plane. By default the origin from "
        "the seed file is used.'/>"
        "  <Option name='ENCODING' type='string' description="
        "'Encoding name, as supported by iconv'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
                              "<LayerCreationOptionList/>");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_READ, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_WRITE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");

    poDriver->pfnOpen = OGRDGNDriverOpen;
    poDriver->pfnIdentify = OGRDGNDriverIdentify;
    poDriver->pfnCreate = OGRDGNDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
