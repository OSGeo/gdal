/******************************************************************************
 *
 * Project:  MongoDB Translator
 * Purpose:  Implements OGRMongoDBDriver.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2014-2019, Even Rouault <even dot rouault at spatialys dot com>
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include "ogrmongodbv3drivercore.h"

/************************************************************************/
/*                   OGRMongoDBv3DriverIdentify()                       */
/************************************************************************/

int OGRMongoDBv3DriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "MongoDBv3:") ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, "mongodb+srv:") ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, "mongodb:");
}

/************************************************************************/
/*                 OGRMongoDBv3DriverSetCommonMetadata()                */
/************************************************************************/

void OGRMongoDBv3DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "MongoDB (using libmongocxx v3 client)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/mongodbv3.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "OGRSQL SQLITE MongoDB");

    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "MongoDBv3:");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='OVERWRITE' type='boolean' description='Whether to "
        "overwrite an existing collection with the layer name to be created' "
        "default='NO'/>"
        "  <Option name='GEOMETRY_NAME' type='string' description='Name of "
        "geometry column.' default='geometry'/>"
        "  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to "
        "create a spatial index' default='YES'/>"
        "  <Option name='FID' type='string' description='Field name, with "
        "integer values, to use as FID' default='ogc_fid'/>"
        "  <Option name='WRITE_OGR_METADATA' type='boolean' "
        "description='Whether to create a description of layer fields in the "
        "_ogr_metadata collection' default='YES'/>"
        "  <Option name='DOT_AS_NESTED_FIELD' type='boolean' "
        "description='Whether to consider dot character in field name as "
        "sub-document' default='YES'/>"
        "  <Option name='IGNORE_SOURCE_ID' type='boolean' description='Whether "
        "to ignore _id field in features passed to CreateFeature()' "
        "default='NO'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='URI' type='string' description='Connection URI' />"
        "  <Option name='HOST' type='string' description='Server hostname' />"
        "  <Option name='PORT' type='integer' description='Server port' />"
        "  <Option name='DBNAME' type='string' description='Database name' />"
        "  <Option name='USER' type='string' description='User name' />"
        "  <Option name='PASSWORD' type='string' description='User password' />"
        "  <Option name='SSL_PEM_KEY_FILE' type='string' description='SSL PEM "
        "certificate/key filename' />"
        "  <Option name='SSL_PEM_KEY_PASSWORD' type='string' description='SSL "
        "PEM key password' />"
        "  <Option name='SSL_CA_FILE' type='string' description='SSL "
        "Certification Authority filename' />"
        "  <Option name='SSL_CRL_FILE' type='string' description='SSL "
        "Certification Revocation List filename' />"
        "  <Option name='SSL_ALLOW_INVALID_CERTIFICATES' type='boolean' "
        "description='Whether to allow connections to servers with invalid "
        "certificates' default='NO'/>"
        "  <Option name='BATCH_SIZE' type='integer' description='Number of "
        "features to retrieve per batch'/>"
        "  <Option name='FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN' "
        "type='integer' description='Number of features to retrieve to "
        "establish feature definition. -1 = unlimited' default='100'/>"
        "  <Option name='JSON_FIELD' type='boolean' description='Whether to "
        "include a field with the full document as JSON' default='NO'/>"
        "  <Option name='FLATTEN_NESTED_ATTRIBUTES' type='boolean' "
        "description='Whether to recursively explore nested objects and "
        "produce flatten OGR attributes' default='YES'/>"
        "  <Option name='FID' type='string' description='Field name, with "
        "integer values, to use as FID' default='ogc_fid'/>"
        "  <Option name='USE_OGR_METADATA' type='boolean' description='Whether "
        "to use the _ogr_metadata collection to read layer metadata' "
        "default='YES'/>"
        "  <Option name='BULK_INSERT' type='boolean' description='Whether to "
        "use bulk insert for feature creation' default='YES'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date DateTime Time IntegerList "
        "Integer64List RealList StringList Binary");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");

    poDriver->pfnIdentify = OGRMongoDBv3DriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                 DeclareDeferredOGRMongoDBv3Plugin()                  */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRMongoDBv3Plugin()
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
    OGRMongoDBv3DriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
