/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogropenfilegdbdrivercore.h"

#define ENDS_WITH(str, strLen, end)                                            \
    (strLen >= strlen(end) && EQUAL(str + strLen - strlen(end), end))

/************************************************************************/
/*                         OGROpenFileGDBDriverIdentify()               */
/************************************************************************/

GDALIdentifyEnum OGROpenFileGDBDriverIdentify(GDALOpenInfo *poOpenInfo,
                                              const char *&pszFilename)
{
    if (STARTS_WITH(pszFilename, "OpenFileGDB:"))
        return GDAL_IDENTIFY_TRUE;

        // FUSIL is a fuzzer
#ifdef FOR_FUSIL
    CPLString osOrigFilename(pszFilename);
#endif

    // First check if we have to do any work.
    size_t nLen = strlen(pszFilename);
    if (ENDS_WITH(pszFilename, nLen, ".gdb") ||
        ENDS_WITH(pszFilename, nLen, ".gdb/"))
    {
        // Check that the filename is really a directory, to avoid confusion
        // with Garmin MapSource - gdb format which can be a problem when the
        // driver is loaded as a plugin, and loaded before the GPSBabel driver
        // (http://trac.osgeo.org/osgeo4w/ticket/245)
        if (STARTS_WITH(pszFilename, "/vsicurl/https://github.com/") ||
            !poOpenInfo->bStatOK || !poOpenInfo->bIsDirectory)
        {
            // In case we do not manage to list the directory, try to stat one
            // file.
            VSIStatBufL stat;
            if (!(STARTS_WITH(pszFilename, "/vsicurl/") &&
                  VSIStatL(
                      CPLFormFilename(pszFilename, "a00000001", "gdbtable"),
                      &stat) == 0))
            {
                return GDAL_IDENTIFY_FALSE;
            }
        }
        return GDAL_IDENTIFY_TRUE;
    }
    /* We also accept zipped GDB */
    else if (ENDS_WITH(pszFilename, nLen, ".gdb.zip") ||
             ENDS_WITH(pszFilename, nLen, ".gdb.tar") ||
             /* Canvec GBs */
             (ENDS_WITH(pszFilename, nLen, ".zip") &&
              (strstr(pszFilename, "_gdb") != nullptr ||
               strstr(pszFilename, "_GDB") != nullptr)))
    {
        return GDAL_IDENTIFY_TRUE;
    }
    /* We also accept tables themselves */
    else if (ENDS_WITH(pszFilename, nLen, ".gdbtable"))
    {
        return GDAL_IDENTIFY_TRUE;
    }
#ifdef FOR_FUSIL
    /* To be able to test fuzzer on any auxiliary files used (indexes, etc.) */
    else if (strlen(CPLGetBasename(pszFilename)) == 9 &&
             CPLGetBasename(pszFilename)[0] == 'a')
    {
        pszFilename = CPLFormFilename(CPLGetPath(pszFilename),
                                      CPLGetBasename(pszFilename), "gdbtable");
        return GDAL_IDENTIFY_TRUE;
    }
    else if (strlen(CPLGetBasename(CPLGetBasename(pszFilename))) == 9 &&
             CPLGetBasename(CPLGetBasename(pszFilename))[0] == 'a')
    {
        pszFilename = CPLFormFilename(
            CPLGetPath(pszFilename),
            CPLGetBasename(CPLGetBasename(pszFilename)), "gdbtable");
        return GDAL_IDENTIFY_TRUE;
    }
#endif

#ifdef DEBUG
    /* For AFL, so that .cur_input is detected as the archive filename */
    else if (EQUAL(CPLGetFilename(pszFilename), ".cur_input"))
    {
        // This file may be recognized or not by this driver,
        // but there were not enough elements to judge.
        return GDAL_IDENTIFY_UNKNOWN;
    }
#endif

    else if (EQUAL(pszFilename, "."))
    {
        GDALIdentifyEnum eRet = GDAL_IDENTIFY_FALSE;
        char *pszCurrentDir = CPLGetCurrentDir();
        if (pszCurrentDir)
        {
            const char *pszTmp = pszCurrentDir;
            eRet = OGROpenFileGDBDriverIdentify(poOpenInfo, pszTmp);
            CPLFree(pszCurrentDir);
        }
        return eRet;
    }

    else
    {
        return GDAL_IDENTIFY_FALSE;
    }
}

static int OGROpenFileGDBDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    const char *pszFilename = poOpenInfo->pszFilename;
    return OGROpenFileGDBDriverIdentify(poOpenInfo, pszFilename);
}

/************************************************************************/
/*                 OGROpenFileGDBDriverSetCommonMetadata()              */
/************************************************************************/

void OGROpenFileGDBDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ESRI FileGDB");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "gdb");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/openfilegdb.html");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_GEOMETRY_FLAGS,
                              "EquatesMultiAndSingleLineStringDuringWrite "
                              "EquatesMultiAndSinglePolygonDuringWrite");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Real String Date DateTime Binary "
                              "Integer64 Date Time");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Int16 Float32");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "Nullable Default "
                              "AlternativeName Domain");
    poDriver->SetMetadataItem(
        GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
        "Name Type Nullable Default Domain AlternativeName");
    // see https://support.esri.com/en/technical-article/000010906
    poDriver->SetMetadataItem(
        GDAL_DMD_ILLEGAL_FIELD_NAMES,
        "ADD ALTER AND BETWEEN BY COLUMN CREATE DELETE DROP EXISTS FOR FROM "
        "GROUP IN INSERT INTO IS LIKE NOT NULL OR ORDER SELECT SET TABLE "
        "UPDATE VALUES WHERE");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DEFAULT_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FIELD_DOMAINS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_RENAME_LAYERS, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_RELATIONSHIPS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_RELATIONSHIP, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_RELATIONSHIP, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_UPDATE_RELATIONSHIP, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_RELATIONSHIP_FLAGS,
                              "OneToOne OneToMany ManyToMany Composite "
                              "Association ForwardPathLabel BackwardPathLabel");
    poDriver->SetMetadataItem(GDAL_DMD_RELATIONSHIP_RELATED_TABLE_TYPES,
                              "features media");

    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DOMAIN_TYPES,
                              "Coded Range");

    poDriver->SetMetadataItem(GDAL_DMD_ALTER_GEOM_FIELD_DEFN_FLAGS, "Name SRS");
    poDriver->SetMetadataItem(GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='LIST_ALL_TABLES' type='string-select' scope='vector' "
        "description='Whether all tables, including system and internal tables "
        "(such as GDB_* tables) should be listed' default='NO'>"
        "    <Value>YES</Value>"
        "    <Value>NO</Value>"
        "  </Option>"
        "  <Option name='NODATA_OR_MASK' type='string' scope='raster' "
        "description='AUTO, MASK, NONE or numeric nodata value'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='TARGET_ARCGIS_VERSION' type='string-select' "
        "default='ALL'>"
        "       <Value>ALL</Value>"
        "       <Value>ARCGIS_PRO_3_2_OR_LATER</Value>"
        "  </Option>"
        "  <Option name='FEATURE_DATASET' type='string' "
        "description='FeatureDataset folder into which to put the new layer'/>"
        "  <Option name='LAYER_ALIAS' type='string' description='Alias of "
        "layer name'/>"
        "  <Option name='GEOMETRY_NAME' type='string' description='Name of "
        "geometry column' default='SHAPE'/>"
        "  <Option name='GEOMETRY_NULLABLE' type='boolean' "
        "description='Whether the values of the geometry column can be NULL' "
        "default='YES'/>"
        "  <Option name='FID' type='string' description='Name of OID column' "
        "default='OBJECTID'/>"
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
        "  <Option name='COLUMN_TYPES' type='string' description='A list of "
        "strings of format field_name=fgdb_field_type (separated by comma) to "
        "force the FileGDB column type of fields to be created'/>"
        "  <Option name='DOCUMENTATION' type='string' description='XML "
        "documentation'/>"
        "  <Option name='CONFIGURATION_KEYWORD' type='string-select' "
        "description='Customize how data is stored. By default text in UTF-8 "
        "and data up to 1TB' default='DEFAULTS'>"
        "    <Value>DEFAULTS</Value>"
        "    <Value>MAX_FILE_SIZE_4GB</Value>"
        "    <Value>MAX_FILE_SIZE_256TB</Value>"
        "    <Value>TEXT_UTF16</Value>"
        "  </Option>"
        "  <Option name='TIME_IN_UTC' type='boolean' description='Whether "
        "datetime fields should be considered to be in UTC' default='NO'/>"
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

    poDriver->pfnIdentify = OGROpenFileGDBDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                 DeclareDeferredOGROpenFileGDBPlugin()                */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGROpenFileGDBPlugin()
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
    OGROpenFileGDBDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
