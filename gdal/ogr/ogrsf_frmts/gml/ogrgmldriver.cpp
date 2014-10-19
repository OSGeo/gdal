/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRGMLDriverUnload()                          */
/************************************************************************/

static void OGRGMLDriverUnload(CPL_UNUSED GDALDriver* poDriver)
{
    if( GMLReader::hMutex != NULL )
        CPLDestroyMutex( GMLReader::hMutex );
    GMLReader::hMutex = NULL;
}

/************************************************************************/
/*                         OGRGMLDriverIdentify()                       */
/************************************************************************/

static int OGRGMLDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->fpL == NULL )
    {
        if( strstr(poOpenInfo->pszFilename, "xsd=") != NULL )
            return -1; /* must be later checked */
        return FALSE;
    }
    /* Might be a OS-Mastermap gzipped GML, so let be nice and try to open */
    /* it transparently with /vsigzip/ */
    else
    if ( poOpenInfo->pabyHeader[0] == 0x1f && poOpenInfo->pabyHeader[1] == 0x8b &&
         EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "gz") &&
         strncmp(poOpenInfo->pszFilename, "/vsigzip/", strlen("/vsigzip/")) != 0 )
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
    OGRGMLDataSource    *poDS;

    if( poOpenInfo->eAccess == GA_Update )
        return NULL;

    if( OGRGMLDriverIdentify( poOpenInfo ) == FALSE )
        return NULL;

    poDS = new OGRGMLDataSource();

    if( !poDS->Open(  poOpenInfo->pszFilename ) )
    {
        delete poDS;
        return NULL;
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
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRGML()                           */
/************************************************************************/

void RegisterOGRGML()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "GML" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GML" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Geography Markup Language (GML)" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gml" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "gml xml" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_gml.html" );

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
"  <Option name='FORMAT' type='string-select' description='Version of GML to use' default='GML2'>"
"    <Value>GML2</Value>"
"    <Value>GML3</Value>"
"    <Value>GML3.2</Value>"
"    <Value>GML3Deegree</Value>"
"  </Option>"
"  <Option name='GML3_LONGSRS' type='boolean' description='Whether to write SRS with \"urn:ogc:def:crs:EPSG::\" prefix with GML3* versions' default='YES'/>"
"  <Option name='WRITE_FEATURE_BOUNDED_BY' type='boolean' description='Whether to write <gml:boundedBy> element for each feature with GML3* versions' default='YES'/>"
"  <Option name='SPACE_INDENTATION' type='boolean' description='Whether to indentate the output for readability' default='YES'/>"
"  <Option name='SRSDIMENSION_LOC' type='string-select' description='(only valid for FORMAT=GML3xx) Location where to put srsDimension attribute' default='POSLIST'>"
"    <Value>POSLIST</Value>"
"    <Value>GEOMETRY</Value>"
"    <Value>GEOMETRY,POSLIST</Value>"
"  </Option>"
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST, "<LayerCreationOptionList/>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRGMLDriverOpen;
        poDriver->pfnIdentify = OGRGMLDriverIdentify;
        poDriver->pfnCreate = OGRGMLDriverCreate;
        poDriver->pfnUnloadDriver = OGRGMLDriverUnload;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
