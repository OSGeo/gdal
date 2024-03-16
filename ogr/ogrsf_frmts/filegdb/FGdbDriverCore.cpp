/******************************************************************************
 *
 * Project:  FileGDB Translator
 * Purpose:  Implements FileGDB OGR driver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
 * Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
 * Copyright (c) 2011-2023, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogrsf_frmts.h"
#include "FGdbDriverCore.h"

#define ENDS_WITH(str, strLen, end)                                            \
    (strLen >= strlen(end) && EQUAL(str + strLen - strlen(end), end))

/************************************************************************/
/*                 OGRFileGDBDriverIdentifyInternal()                   */
/************************************************************************/

GDALIdentifyEnum OGRFileGDBDriverIdentifyInternal(GDALOpenInfo *poOpenInfo,
                                                  const char *&pszFilename)
{
    // First check if we have to do any work.
    size_t nLen = strlen(pszFilename);
    if (ENDS_WITH(pszFilename, nLen, ".gdb") ||
        ENDS_WITH(pszFilename, nLen, ".gdb/"))
    {
        // Check that the filename is really a directory, to avoid confusion
        // with Garmin MapSource - gdb format which can be a problem when the
        // driver is loaded as a plugin, and loaded before the GPSBabel driver
        // (http://trac.osgeo.org/osgeo4w/ticket/245)
        if (STARTS_WITH(pszFilename, "/vsi") || !poOpenInfo->bStatOK ||
            !poOpenInfo->bIsDirectory)
        {
            return GDAL_IDENTIFY_FALSE;
        }
        return GDAL_IDENTIFY_TRUE;
    }
    else if (EQUAL(pszFilename, "."))
    {
        GDALIdentifyEnum eRet = GDAL_IDENTIFY_FALSE;
        char *pszCurrentDir = CPLGetCurrentDir();
        if (pszCurrentDir)
        {
            const char *pszTmp = pszCurrentDir;
            eRet = OGRFileGDBDriverIdentifyInternal(poOpenInfo, pszTmp);
            CPLFree(pszCurrentDir);
        }
        return eRet;
    }
    else
    {
        return GDAL_IDENTIFY_FALSE;
    }
}

/************************************************************************/
/*                      OGRFileGDBDriverIdentify()                      */
/************************************************************************/

static int OGRFileGDBDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    const char *pszFilename = poOpenInfo->pszFilename;
    return OGRFileGDBDriverIdentifyInternal(poOpenInfo, pszFilename);
}

/************************************************************************/
/*                OGRFileGDBDriverSetCommonMetadata()                   */
/************************************************************************/

void OGRFileGDBDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ESRI FileGDB");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gdb");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/filegdb.html");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='FEATURE_DATASET' type='string' "
        "description='FeatureDataset folder into to put the new layer'/>"
        "  <Option name='LAYER_ALIAS' type='string' description='Alias of "
        "layer name'/>"
        "  <Option name='GEOMETRY_NAME' type='string' description='Name of "
        "geometry column' default='SHAPE'/>"
        "  <Option name='GEOMETRY_NULLABLE' type='boolean' "
        "description='Whether the values of the geometry column can be NULL' "
        "default='YES'/>"
        "  <Option name='FID' type='string' description='Name of OID column' "
        "default='OBJECTID' deprecated_alias='OID_NAME'/>"
        "  <Option name='XYTOLERANCE' type='float' description='Snapping "
        "tolerance, used for advanced ArcGIS features like network and "
        "topology rules, on 2D coordinates, in the units of the CRS'/>"
        "  <Option name='ZTOLERANCE' type='float' description='Snapping "
        "tolerance, used for advanced ArcGIS features like network and "
        "topology rules, on Z coordinates, in the units of the CRS'/>"
        "  <Option name='MTOLERANCE' type='float' description='Snapping "
        "tolerance, used for advanced ArcGIS features like network and "
        "topology rules, on M coordinates'/>"
        "  <Option name='XORIGIN' type='float' description='X origin of the "
        "coordinate precision grid'/>"
        "  <Option name='YORIGIN' type='float' description='Y origin of the "
        "coordinate precision grid'/>"
        "  <Option name='ZORIGIN' type='float' description='Z origin of the "
        "coordinate precision grid'/>"
        "  <Option name='MORIGIN' type='float' description='M origin of the "
        "coordinate precision grid'/>"
        "  <Option name='XYSCALE' type='float' description='X,Y scale of the "
        "coordinate precision grid'/>"
        "  <Option name='ZSCALE' type='float' description='Z scale of the "
        "coordinate precision grid'/>"
        "  <Option name='MSCALE' type='float' description='M scale of the "
        "coordinate precision grid'/>"
        "  <Option name='XML_DEFINITION' type='string' description='XML "
        "definition to create the new table. The root node of such a XML "
        "definition must be a &lt;esri:DataElement&gt; element conformant to "
        "FileGDBAPI.xsd'/>"
        "  <Option name='CREATE_MULTIPATCH' type='boolean' "
        "description='Whether to write geometries of layers of type "
        "MultiPolygon as MultiPatch' default='NO'/>"
        "  <Option name='COLUMN_TYPES' type='string' description='A list of "
        "strings of format field_name=fgdb_field_type (separated by comma) to "
        "force the FileGDB column type of fields to be created'/>"
        "  <Option name='CONFIGURATION_KEYWORD' type='string-select' "
        "description='Customize how data is stored. By default text in UTF-8 "
        "and data up to 1TB'>"
        "    <Value>DEFAULTS</Value>"
        "    <Value>TEXT_UTF16</Value>"
        "    <Value>MAX_FILE_SIZE_4GB</Value>"
        "    <Value>MAX_FILE_SIZE_256TB</Value>"
        "    <Value>GEOMETRY_OUTOFLINE</Value>"
        "    <Value>BLOB_OUTOFLINE</Value>"
        "    <Value>GEOMETRY_AND_BLOB_OUTOFLINE</Value>"
        "  </Option>"
        "  <Option name='CREATE_SHAPE_AREA_AND_LENGTH_FIELDS' type='boolean' "
        "description='Whether to create special Shape_Length and Shape_Area "
        "fields' default='NO'/>"
        "</LayerCreationOptionList>");

    // Setting to another value than the default one doesn't really work
    // with the SDK
    // Option name='AREA_FIELD_NAME' type='string' description='Name of
    // the column that contains the geometry area' default='Shape_Area'
    // Option name='length_field_name' type='string' description='Name of
    // the column that contains the geometry length'
    // default='Shape_Length'

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Real String Date DateTime Binary");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Int16 Float32");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "Nullable Default "
                              "AlternativeName Domain");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DEFAULT_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FIELD_DOMAINS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_RELATIONSHIPS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_RENAME_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_GEOMETRY_FLAGS,
                              "EquatesMultiAndSingleLineStringDuringWrite "
                              "EquatesMultiAndSinglePolygonDuringWrite");
    // see https://support.esri.com/en/technical-article/000010906
    poDriver->SetMetadataItem(
        GDAL_DMD_ILLEGAL_FIELD_NAMES,
        "ADD ALTER AND BETWEEN BY COLUMN CREATE DELETE DROP EXISTS FOR FROM "
        "GROUP IN INSERT INTO IS LIKE NOT NULL OR ORDER SELECT SET TABLE "
        "UPDATE VALUES WHERE");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DOMAIN_TYPES,
                              "Coded Range");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "NATIVE OGRSQL SQLITE");
    poDriver->SetMetadataItem(GDAL_DMD_RELATIONSHIP_RELATED_TABLE_TYPES,
                              "features media");
    poDriver->SetMetadataItem(GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION, "YES");

    poDriver->pfnIdentify = OGRFileGDBDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                  DeclareDeferredOGRFileGDBPlugin()                   */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRFileGDBPlugin()
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
    OGRFileGDBDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
