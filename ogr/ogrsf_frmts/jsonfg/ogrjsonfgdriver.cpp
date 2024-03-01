/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGC Features and Geometries JSON (JSON-FG)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include "ogr_jsonfg.h"
#include "ogrgeojsonutils.h"

/************************************************************************/
/*                       OGRJSONFGDriverIdentify()                      */
/************************************************************************/

static int OGRJSONFGDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType = JSONFGDriverGetSourceType(poOpenInfo);
    if (nSrcType == eGeoJSONSourceUnknown)
        return FALSE;
    if (nSrcType == eGeoJSONSourceService &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "JSONFG:"))
    {
        return -1;
    }
    return TRUE;
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset *OGRJSONFGDriverOpen(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType = JSONFGDriverGetSourceType(poOpenInfo);
    if (nSrcType == eGeoJSONSourceUnknown)
        return nullptr;
    auto poDS = std::make_unique<OGRJSONFGDataset>();
    if (!poDS->Open(poOpenInfo, nSrcType))
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRJSONFGDriverCreate(const char *pszName, int /* nBands */,
                                          int /* nXSize */, int /* nYSize */,
                                          GDALDataType /* eDT */,
                                          char **papszOptions)
{
    auto poDS = std::make_unique<OGRJSONFGDataset>();
    if (!poDS->Create(pszName, papszOptions))
    {
        return nullptr;
    }
    return poDS.release();
}

/************************************************************************/
/*                           RegisterOGRJSONFG()                        */
/************************************************************************/

void RegisterOGRJSONFG()
{
    if (GDALGetDriverByName("JSONFG") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("JSONFG");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "OGC Features and Geometries JSON");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "json");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/jsonfg.html");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='GEOMETRY_ELEMENT' type='string-select' "
        "description='Which JSON element to use to create geometry from'>"
        "    <Value>AUTO</Value>"
        "    <Value>PLACE</Value>"
        "    <Value>GEOMETRY</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='SINGLE_LAYER' type='boolean' description='whether "
        "only one layer will be written' default='NO'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='COORDINATE_PRECISION_GEOMETRY' type='int' "
        "description='Number of decimal for coordinates in the geometry "
        "element'/>"
        "  <Option name='COORDINATE_PRECISION_PLACE' type='int' "
        "description='Number of decimal for coordinates in the place element'/>"
        "  <Option name='WRITE_GEOMETRY' type='boolean' "
        "description='Can be set to NO to avoid writing the geometry element "
        "when place is written' default='YES'/>"
        "  <Option name='SIGNIFICANT_FIGURES' type='int' description='Number "
        "of significant figures for floating-point values' default='17'/>"
        "  <Option name='ID_FIELD' type='string' description='Name of the "
        "source field that must be used as the id member of Feature features'/>"
        "  <Option name='ID_TYPE' type='string-select' description='Type of "
        "the id member of Feature features'>"
        "    <Value>AUTO</Value>"
        "    <Value>String</Value>"
        "    <Value>Integer</Value>"
        "  </Option>"
        "  <Option name='ID_GENERATE' type='boolean' "
        "description='Auto-generate feature ids' default='NO'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String IntegerList "
        "Integer64List RealList StringList Date DateTime");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(GDAL_DCAP_FLUSHCACHE_CONSISTENT_STATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION, "YES");

    poDriver->pfnOpen = OGRJSONFGDriverOpen;
    poDriver->pfnIdentify = OGRJSONFGDriverIdentify;
    poDriver->pfnCreate = OGRJSONFGDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
