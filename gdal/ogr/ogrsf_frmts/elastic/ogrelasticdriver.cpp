/******************************************************************************
 * $Id$
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
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

#include "ogr_elastic.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                   OGRElasticSearchDriverIdentify()                   */
/************************************************************************/

static int OGRElasticSearchDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return EQUALN(poOpenInfo->pszFilename, "ES:", strlen("ES:"));
}

/************************************************************************/
/*                  OGRElasticSearchDriverOpen()                        */
/************************************************************************/

static GDALDataset* OGRElasticSearchDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRElasticSearchDriverIdentify(poOpenInfo) )
        return NULL;

    OGRElasticDataSource *poDS = new OGRElasticDataSource();
    if (!poDS->Open(poOpenInfo)) {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                     OGRElasticSearchDriverCreate()                   */
/************************************************************************/
static GDALDataset* OGRElasticSearchDriverCreate( const char * pszName,
                                                  CPL_UNUSED int nXSize,
                                                  CPL_UNUSED int nYSize,
                                                  CPL_UNUSED int nBands,
                                                  CPL_UNUSED GDALDataType eDT,
                                                  char ** papszOptions )
{
    OGRElasticDataSource *poDS = new OGRElasticDataSource();

    if (!poDS->Create(pszName, papszOptions)) {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          RegisterOGRElastic()                        */
/************************************************************************/

void RegisterOGRElastic() {
    if (!GDAL_CHECK_VERSION("OGR/Elastic Search driver"))
        return;
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "ElasticSearch" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "ElasticSearch" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                    "Elastic Search" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                    "drv_elasticsearch.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "ES:" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
    "<CreationOptionList/>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
    "<LayerCreationOptionList>"
    "  <Option name='INDEX_NAME' type='string' description='Name of the index to create (or reuse). By default the index name is the layer name.'/>"
    "  <Option name='MAPPING_NAME' type='string' description='Name of the mapping type within the index.' default='FeatureCollection'/>"
    "  <Option name='MAPPING' type='string' description='Filename from which to read a user-defined mapping, or mapping as serialized JSon.'/>"
    "  <Option name='WRITE_MAPPING' type='string' description='Filename where to write the OGR generated mapping.'/>"
    "  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing collection with the layer name to be created' default='NO'/>"
    "  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column.' default='geometry'/>"
    "  <Option name='GEOM_MAPPING_TYPE' type='string-select' description='Mapping type for geometry fields' default='AUTO'>"
    "    <Value>AUTO</Value>"
    "    <Value>GEO_POINT</Value>"
    "    <Value>GEO_SHAPE</Value>"
    "  </Option>"
    "  <Option name='GEOM_PRECISION' type='string' description='Desired geometry precision. Number followed by unit. For example 1m'/>"
    "  <Option name='BULK_INSERT' type='boolean' description='Whether to use bulk insert for feature creation' default='YES'/>"
    "  <Option name='BULK_SIZE' type='integer' description='Size in bytes of the buffer for bulk upload' default='1000000'/>"
    "  <Option name='DOT_AS_NESTED_FIELD' type='boolean' description='Whether to consider dot character in field name as sub-document' default='YES'/>"
    "  <Option name='IGNORE_SOURCE_ID' type='boolean' description='Whether to ignore _id field in features passed to CreateFeature()' default='NO'/>"
    "  <Option name='FID' type='string' description='Field name, with integer values, to use as FID' default='ogc_fid'/>"
    "</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='HOST' type='string' description='Server hostname' default='localhost'/>"
"  <Option name='PORT' type='integer' description='Server port' default='9200'/>"
"  <Option name='BATCH_SIZE' type='integer' description='Number of features to retrieve per batch' default='100'/>"
"  <Option name='FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN' type='integer' description='Number of features to retrieve to establish feature definition. -1 = unlimited' default='100'/>"
"  <Option name='JSON_FIELD' type='boolean' description='Whether to include a field with the full document as JSON' default='NO'/>"
"  <Option name='FLATTEN_NESTED_ATTRIBUTES' type='boolean' description='Whether to recursively explore nested objects and produce flatten OGR attributes' default='YES'/>"
"  <Option name='BULK_INSERT' type='boolean' description='Whether to use bulk insert for feature creation' default='YES'/>"
"  <Option name='BULK_SIZE' type='integer' description='Size in bytes of the buffer for bulk upload' default='1000000'/>"
"  <Option name='FID' type='string' description='Field name, with integer values, to use as FID' default='ogc_fid'/>"
"</OpenOptionList>");

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime Time IntegerList Integer64List RealList StringList Binary" );

        poDriver->pfnIdentify = OGRElasticSearchDriverIdentify;
        poDriver->pfnOpen = OGRElasticSearchDriverOpen;
        poDriver->pfnCreate = OGRElasticSearchDriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
