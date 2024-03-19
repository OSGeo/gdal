/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaDriver functions implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
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

#include "ogrhanadrivercore.h"

// clang-format off
const char* OGRHanaLayerCreationOptionsConstants::GetList()
{
    return
           "<LayerCreationOptionList>"
           "  <Option name='OVERWRITE' type='boolean' description='Specifies whether to overwrite an existing table with the layer name to be created' default='NO'/>"
           "  <Option name='LAUNDER' type='boolean' description='Specifies whether layer and field names will be laundered' default='YES'/>"
           "  <Option name='PRECISION' type='boolean' description='Specifies whether fields created should keep the width and precision' default='YES'/>"
           "  <Option name='DEFAULT_STRING_SIZE' type='int' description='Specifies default string column size' default='256'/>"
           "  <Option name='GEOMETRY_NAME' type='string' description='Specifies name of geometry column.' default='OGR_GEOMETRY'/>"
           "  <Option name='GEOMETRY_NULLABLE' type='boolean' description='Specifies whether the values of the geometry column can be NULL' default='YES'/>"
           "  <Option name='GEOMETRY_INDEX' type='string' description='Specifies which spatial index to use for the geometry column' default='DEFAULT'/>"
           "  <Option name='SRID' type='int' description='Forced SRID of the layer'/>"
           "  <Option name='FID' type='string' description='Specifies the name of the FID column to create' default='OGR_FID'/>"
           "  <Option name='FID64' type='boolean' description='Specifies whether to create the FID column with BIGINT type to handle 64bit wide ids' default='NO'/>"
           "  <Option name='COLUMN_TYPES' type='string' description='Specifies a comma-separated list of strings in the format field_name=hana_field_type that define column types.'/>"
           "  <Option name='BATCH_SIZE' type='int' description='Specifies the number of bytes to be written per one batch' default='4194304'/>"
           "</LayerCreationOptionList>";
}

// clang-format on

// clang-format off
const char* OGRHanaOpenOptionsConstants::GetList()
{
    return
           "<OpenOptionList>"
           "  <Option name='DRIVER' type='string' description='Name or a path to a driver.For example, DRIVER={HDBODBC} or DRIVER=/usr/sap/hdbclient/libodbcHDB.so' required='true'/>"
           "  <Option name='HOST' type='string' description='Server hostname' required='true'/>"
           "  <Option name='PORT' type='int' description='Port number' required='true'/>"
           "  <Option name='DATABASE' type='string' description='Specifies the name of the database to connect to' required='true'/>"
           "  <Option name='USER' type='string' description='Specifies the user name' required='true'/>"
           "  <Option name='PASSWORD' type='string' description='Specifies the user password' required='true'/>"
           "  <Option name='USER_STORE_KEY' type='string' description='Specifies whether a connection is made using a key defined in the SAP HANA user store (hdbuserstore)' required='false'/>"
           "  <Option name='SCHEMA' type='string' description='Specifies the schema used for tables listed in TABLES option' required='true'/>"
           "  <Option name='TABLES' type='string' description='Restricted set of tables to list (comma separated)'/>"
           "  <Option name='ENCRYPT' type='boolean' description='Enables or disables TLS/SSL encryption' default='NO'/>"
           "  <Option name='SSL_CRYPTO_PROVIDER' type='string' description='Cryptographic library provider used for SSL communication (commoncrypto| sapcrypto | openssl)'/>"
           "  <Option name='SSL_KEY_STORE' type='string' description='Path to the keystore file that contains the server&apos;s private key'/>"
           "  <Option name='SSL_TRUST_STORE' type='string' description='Path to trust store file that contains the server&apos;s public certificate(s) (OpenSSL only)'/>"
           "  <Option name='SSL_VALIDATE_CERTIFICATE' type='boolean' description='If set to true, the server&apos;s certificate is validated' default='YES'/>"
           "  <Option name='SSL_HOST_NAME_IN_CERTIFICATE' type='string' description='Host name used to verify server&apos;s identity'/>"
           "  <Option name='CONNECTION_TIMEOUT' type='int' description='Connection timeout measured in milliseconds. Setting this option to 0 disables the timeout'/>"
           "  <Option name='PACKET_SIZE' type='int' description='Sets the maximum size of a request packet sent from the client to the server, in bytes. The minimum is 1 MB.'/>"
           "  <Option name='SPLIT_BATCH_COMMANDS' type='boolean' description='Allows split and parallel execution of batch commands on partitioned tables' default='YES'/>"
           "  <Option name='DETECT_GEOMETRY_TYPE' type='boolean' description='Specifies whether to detect the type of geometry columns. Note, the detection may take a significant amount of time for large tables' default='YES'/>"
           "</OpenOptionList>";
}

// clang-format on

/************************************************************************/
/*                         OGRHanaDriverIdentify()                      */
/************************************************************************/

int OGRHanaDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, HANA_PREFIX);
}

/************************************************************************/
/*                  OGRHANADriverSetCommonMetadata()                     */
/************************************************************************/

void OGRHANADriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "SAP HANA");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/hana.html");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, HANA_PREFIX);
    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST,
                              OGRHanaOpenOptionsConstants::GetList());
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");
    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
                              OGRHanaLayerCreationOptionsConstants::GetList());
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime "
                              "Time IntegerList "
                              "Integer64List RealList StringList Binary");

    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision Nullable Default");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS,
                              "Name Type WidthPrecision Nullable Default");

    poDriver->SetMetadataItem(GDAL_DCAP_NOTNULL_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DEFAULT_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "NATIVE OGRSQL SQLITE");

    poDriver->pfnIdentify = OGRHanaDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                   DeclareDeferredOGRHANAPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRHANAPlugin()
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
    OGRHANADriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
