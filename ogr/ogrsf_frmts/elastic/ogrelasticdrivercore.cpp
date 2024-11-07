/******************************************************************************
 *
 * Project:  Elasticsearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include "ogrelasticdrivercore.h"

/************************************************************************/
/*                  OGRElasticsearchDriverIdentify()                    */
/************************************************************************/

int OGRElasticsearchDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "ES:");
}

/************************************************************************/
/*                OGRElasticsearchDriverSetCommonMetadata()             */
/************************************************************************/

void OGRElasticsearchDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Elastic Search");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/elasticsearch.html");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "ES:");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS,
                              "OGRSQL SQLITE ES");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='INDEX_NAME' type='string' description='Name of the "
        "index to create (or reuse). By default the index name is the layer "
        "name.'/>"
        "  <Option name='INDEX_DEFINITION' type='string' description='Filename "
        "from which to read a user-defined index definition, or index "
        "definition as serialized JSon.'/>"
        "  <Option name='MAPPING_NAME' type='string' description='(ES &lt; 7) "
        "Name of the mapping type within the index.' "
        "default='FeatureCollection'/>"
        "  <Option name='MAPPING' type='string' description='Filename from "
        "which to read a user-defined mapping, or mapping as serialized "
        "JSon.'/>"
        "  <Option name='WRITE_MAPPING' type='string' description='Filename "
        "where to write the OGR generated mapping.'/>"
        "  <Option name='OVERWRITE' type='boolean' description='Whether to "
        "overwrite an existing type mapping with the layer name to be created' "
        "default='NO'/>"
        "  <Option name='OVERWRITE_INDEX' type='boolean' description='Whether "
        "to overwrite the whole index to which the layer belongs to' "
        "default='NO'/>"
        "  <Option name='GEOMETRY_NAME' type='string' description='Name of "
        "geometry column.' default='geometry'/>"
        "  <Option name='GEOM_MAPPING_TYPE' type='string-select' "
        "description='Mapping type for geometry fields' default='AUTO'>"
        "    <Value>AUTO</Value>"
        "    <Value>GEO_POINT</Value>"
        "    <Value>GEO_SHAPE</Value>"
        "  </Option>"
        "  <Option name='GEO_SHAPE_ENCODING' type='string-select' "
        "description='Encoding for geo_shape geometry fields' "
        "default='GeoJSON'>"
        "    <Value>GeoJSON</Value>"
        "    <Value>WKT</Value>"
        "  </Option>"
        "  <Option name='GEOM_PRECISION' type='string' description='Desired "
        "geometry precision. Number followed by unit. For example 1m'/>"
        "  <Option name='STORE_FIELDS' type='boolean' description='Whether "
        "fields should be stored in the index' default='NO'/>"
        "  <Option name='STORED_FIELDS' type='string' description='List of "
        "comma separated field names that should be stored in the index'/>"
        "  <Option name='NOT_ANALYZED_FIELDS' type='string' description='List "
        "of comma separated field names that should not be analyzed during "
        "indexing, or {ALL}'/>"
        "  <Option name='NOT_INDEXED_FIELDS' type='string' description='List "
        "of comma separated field names that should not be indexed'/>"
        "  <Option name='FIELDS_WITH_RAW_VALUE' type='string' "
        "description='List of comma separated field names (of type string) "
        "that should have an additional raw/not_analyzed field, or {ALL}'/>"
        "  <Option name='BULK_INSERT' type='boolean' description='Whether to "
        "use bulk insert for feature creation' default='YES'/>"
        "  <Option name='BULK_SIZE' type='integer' description='Size in bytes "
        "of the buffer for bulk upload' default='1000000'/>"
        "  <Option name='DOT_AS_NESTED_FIELD' type='boolean' "
        "description='Whether to consider dot character in field name as "
        "sub-document' default='YES'/>"
        "  <Option name='IGNORE_SOURCE_ID' type='boolean' description='Whether "
        "to ignore _id field in features passed to CreateFeature()' "
        "default='NO'/>"
        "  <Option name='FID' type='string' description='Field name, with "
        "integer values, to use as FID' default='ogc_fid'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='HOST' type='string' description='Server hostname' "
        "default='localhost'/>"
        "  <Option name='PORT' type='integer' description='Server port' "
        "default='9200'/>"
        "  <Option name='USERPWD' type='string' "
        "description='Basic authentication as username:password'/>"
        "  <Option name='LAYER' type='string' description='Index name or "
        "index_mapping to use for restricting layer listing'/>"
        "  <Option name='AGGREGATION' type='string' description='JSon "
        "serialized description of an aggregation request'/>"
        "  <Option name='BATCH_SIZE' type='integer' description='Number of "
        "features to retrieve per batch' default='100'/>"
        "  <Option name='FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN' "
        "type='integer' description='Number of features to retrieve to "
        "establish feature definition. -1 = unlimited' default='100'/>"
        "  <Option name='SINGLE_QUERY_TIMEOUT' type='float' "
        "description='Timeout in second for request such as GetFeatureCount() "
        "or GetExtent()'/>"
        "  <Option name='SINGLE_QUERY_TERMINATE_AFTER' type='integer' "
        "description='Maximum number of documents to collect for request such "
        "as GetFeatureCount() or GetExtent()'/>"
        "  <Option name='FEATURE_ITERATION_TIMEOUT' type='float' "
        "description='Timeout in second for feature iteration'/>"
        "  <Option name='FEATURE_ITERATION_TERMINATE_AFTER' type='integer' "
        "description='Maximum number of documents to collect for feature "
        "iteration'/>"
        "  <Option name='JSON_FIELD' type='boolean' description='Whether to "
        "include a field with the full document as JSON' default='NO'/>"
        "  <Option name='FLATTEN_NESTED_ATTRIBUTES' type='boolean' "
        "description='Whether to recursively explore nested objects and "
        "produce flatten OGR attributes' default='YES'/>"
        "  <Option name='BULK_INSERT' type='boolean' description='Whether to "
        "use bulk insert for feature creation' default='YES'/>"
        "  <Option name='BULK_SIZE' type='integer' description='Size in bytes "
        "of the buffer for bulk upload' default='1000000'/>"
        "  <Option name='FID' type='string' description='Field name, with "
        "integer values, to use as FID' default='ogc_fid'/>"
        "  <Option name='FORWARD_HTTP_HEADERS_FROM_ENV' type='string' "
        "description='Comma separated list of "
        "http_header_name=env_variable_name'/>"
        "  <Option name='ADD_SOURCE_INDEX_NAME' type='boolean' "
        "description='Whether to add the index name as a field for wildcard "
        "layers' default='NO'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime "
                              "Time IntegerList Integer64List RealList "
                              "StringList Binary");

    poDriver->pfnIdentify = OGRElasticsearchDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                  DeclareDeferredOGRElasticPlugin()                   */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRElasticPlugin()
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
    OGRElasticsearchDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
