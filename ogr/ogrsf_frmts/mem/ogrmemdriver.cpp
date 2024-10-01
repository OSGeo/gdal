/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMemDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_mem.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"
#include "ogr_core.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRMemDriverOpen(GDALOpenInfo *)
{
    return nullptr;
}

/************************************************************************/
/*                       OGRMemDriverCreate()                           */
/************************************************************************/

static GDALDataset *OGRMemDriverCreate(const char *pszName, int /* nXSize */,
                                       int /* nYSize */, int /* nBandCount */,
                                       GDALDataType, char **papszOptions)

{
    return new OGRMemDataSource(pszName, papszOptions);
}

/************************************************************************/
/*                           RegisterOGRMem()                           */
/************************************************************************/

void RegisterOGRMEM()

{
    if (GDALGetDriverByName("Memory") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("Memory");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Memory");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_REORDER_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date DateTime Time IntegerList "
        "Integer64List RealList StringList Binary");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision Nullable Default Unique "
                              "Comment AlternativeName Domain");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
                              "Name Type WidthPrecision Nullable Default "
                              "Unique Domain AlternativeName Comment");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='ADVERTIZE_UTF8' type='boolean' description='Whether "
        "the layer will contain UTF-8 strings' default='NO'/>"
        "  <Option name='FID' type='string' description="
        "'Name of the FID column to create' default='' />"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_COORDINATE_EPOCH, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_FIELD_DOMAINS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DOMAIN_TYPES,
                              "Coded Range Glob");

    poDriver->SetMetadataItem(GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS,
                              "Name Type Nullable SRS CoordinateEpoch");

    poDriver->pfnOpen = OGRMemDriverOpen;
    poDriver->pfnCreate = OGRMemDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
