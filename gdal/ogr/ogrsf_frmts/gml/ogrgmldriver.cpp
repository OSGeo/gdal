/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  OGRGMLDriver implementation
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gml.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gmlreaderp.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRGMLDriverIdentify()                       */
/************************************************************************/

static int OGRGMLDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->fpL == nullptr )
    {
        if( strstr(poOpenInfo->pszFilename, "xsd=") != nullptr )
            return -1; /* must be later checked */
        return FALSE;
    }
    /* Might be a OS-Mastermap gzipped GML, so let be nice and try to open */
    /* it transparently with /vsigzip/ */
    else
    if ( poOpenInfo->pabyHeader[0] == 0x1f && poOpenInfo->pabyHeader[1] == 0x8b &&
         EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "gz") &&
         !STARTS_WITH(poOpenInfo->pszFilename, "/vsigzip/") )
    {
        return -1; /* must be later checked */
    }
    else
    {
        const char* szPtr = (const char*)poOpenInfo->pabyHeader;

        if( ( (unsigned char)szPtr[0] == 0xEF )
            && ( (unsigned char)szPtr[1] == 0xBB )
            && ( (unsigned char)szPtr[2] == 0xBF) )
        {
            szPtr += 3;
        }
/* -------------------------------------------------------------------- */
/*      Here, we expect the opening chevrons of GML tree root element   */
/* -------------------------------------------------------------------- */
        if( szPtr[0] != '<' )
            return FALSE;

        if( !poOpenInfo->TryToIngest(4096) )
            return FALSE;

        return OGRGMLDataSource::CheckHeader((const char*)poOpenInfo->pabyHeader);
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGMLDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update )
        return nullptr;

    if( OGRGMLDriverIdentify( poOpenInfo ) == FALSE )
        return nullptr;

    OGRGMLDataSource *poDS = new OGRGMLDataSource();

    if( !poDS->Open(  poOpenInfo ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                             Create()                                 */
/************************************************************************/

static GDALDataset *OGRGMLDriverCreate( const char * pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
    OGRGMLDataSource    *poDS = new OGRGMLDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRGML()                           */
/************************************************************************/

void RegisterOGRGML()

{
    if( GDALGetDriverByName( "GML" ) != nullptr )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "GML" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Geography Markup Language (GML)" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gml" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "gml xml" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/gml.html" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='XSD' type='string' description='Name of the related application schema file (.xsd).'/>"
"  <Option name='GFS_TEMPLATE' type='string' description='Filename of a .gfs template file to apply.'/>"
"  <Option name='WRITE_GFS' type='string-select' description='Whether to write a .gfs file' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"  <Option name='FORCE_SRS_DETECTION' type='boolean' description='Force a full scan to detect the SRS of layers.' default='NO'/>"
"  <Option name='EMPTY_AS_NULL' type='boolean' description='Force empty fields to be reported as NULL. Set to NO so that not-nullable fields can be exposed' default='YES'/>"
"  <Option name='GML_ATTRIBUTES_TO_OGR_FIELDS' type='boolean' description='Whether GML attributes should be reported as OGR fields' default='NO'/>"
"  <Option name='INVERT_AXIS_ORDER_IF_LAT_LONG' type='boolean' description='Whether to present SRS and coordinate ordering in traditional GIS order' default='YES'/>"
"  <Option name='CONSIDER_EPSG_AS_URN' type='string-select' description='Whether to consider srsName like EPSG:XXXX as respecting EPSG axis order' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"  <Option name='SWAP_COORDINATES' type='string-select' "
    "description='Whether the order of geometry coordinates should be inverted.' "
    "default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"  <Option name='READ_MODE' type='string-select' description='Read mode' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>STANDARD</Value>"
"    <Value>SEQUENTIAL_LAYERS</Value>"
"    <Value>INTERLEAVED_LAYERS</Value>"
"  </Option>"
"  <Option name='EXPOSE_GML_ID' type='string-select' description='Whether to make feature gml:id as a gml_id attribute' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"  <Option name='EXPOSE_FID' type='string-select' description='Whether to make feature fid as a fid attribute' default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"  <Option name='DOWNLOAD_SCHEMA' type='boolean' description='Whether to download the remote application schema if needed (only for WFS currently)' default='YES'/>"
"  <Option name='REGISTRY' type='string' description='Filename of the registry with application schemas.'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='XSISCHEMAURI' type='string' description='URI to be inserted as the schema location.'/>"
"  <Option name='XSISCHEMA' type='string-select' description='where to write a .xsd application schema. INTERNAL should not normally be used' default='EXTERNAL'>"
"    <Value>EXTERNAL</Value>"
"    <Value>INTERNAL</Value>"
"    <Value>OFF</Value>"
"  </Option>"
"  <Option name='PREFIX' type='string' description='Prefix for the application target namespace.' default='ogr'/>"
"  <Option name='STRIP_PREFIX' type='boolean' description='Whether to avoid writing the prefix of the application target namespace in the GML file.' default='NO'/>"
"  <Option name='TARGET_NAMESPACE' type='string' description='Application target namespace.' default='http://ogr.maptools.org/'/>"
"  <Option name='FORMAT' type='string-select' description='Version of GML to use' default='GML3.2'>"
"    <Value>GML2</Value>"
"    <Value>GML3</Value>"
"    <Value>GML3.2</Value>"
"    <Value>GML3Deegree</Value>"
"  </Option>"
"  <Option name='GML_FEATURE_COLLECTION' type='boolean' description='Whether to use the gml:FeatureCollection. Only valid for FORMAT=GML3/GML3.2' default='NO'/>"
"  <Option name='GML3_LONGSRS' type='boolean' description='Whether to write SRS with \"urn:ogc:def:crs:EPSG::\" prefix with GML3* versions' default='YES'/>"
"  <Option name='SRSNAME_FORMAT' type='string-select' description='Format of srsName (for GML3* versions)' default='OGC_URL'>"
"    <Value>SHORT</Value>"
"    <Value>OGC_URN</Value>"
"    <Value>OGC_URL</Value>"
"  </Option>"
"  <Option name='WRITE_FEATURE_BOUNDED_BY' type='boolean' description='Whether to write &lt;gml:boundedBy&gt; element for each feature with GML3* versions' default='YES'/>"
"  <Option name='SPACE_INDENTATION' type='boolean' description='Whether to indent the output for readability' default='YES'/>"
"  <Option name='SRSDIMENSION_LOC' type='string-select' description='(only valid for FORMAT=GML3xx) Location where to put srsDimension attribute' default='POSLIST'>"
"    <Value>POSLIST</Value>"
"    <Value>GEOMETRY</Value>"
"    <Value>GEOMETRY,POSLIST</Value>"
"  </Option>"
"  <Option name='GML_ID' type='string' description='Value of feature collection gml:id (GML 3.2 only)' default='aFeatureCollection'/>"
"  <Option name='NAME' type='string' description='Content of GML name element'/>"
"  <Option name='DESCRIPTION' type='string' description='Content of GML description element'/>"
"</CreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST, "<LayerCreationOptionList/>");
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime IntegerList Integer64List RealList StringList" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean Int16 Float32" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_UNIQUE_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->pfnOpen = OGRGMLDriverOpen;
    poDriver->pfnIdentify = OGRGMLDriverIdentify;
    poDriver->pfnCreate = OGRGMLDriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
