/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                       OGRDXFDriverIdentify()                         */
/************************************************************************/

static int OGRDXFDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return poOpenInfo->fpL != NULL &&
           EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"dxf");
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDXFDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRDXFDriverIdentify(poOpenInfo) )
        return NULL;

    OGRDXFDataSource   *poDS = new OGRDXFDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRDXFDriverCreate( const char * pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
    OGRDXFWriterDS *poDS = new OGRDXFWriterDS();

    if( poDS->Open( pszName, papszOptions ) )
        return poDS;
    else
    {
        delete poDS;
        return NULL;
    }
}

/************************************************************************/
/*                           RegisterOGRDXF()                           */
/************************************************************************/

void RegisterOGRDXF()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "DXF" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "DXF" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "AutoCAD DXF" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dxf" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_dxf.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='HEADER' type='string' description='Template header file' default='header.dxf'/>"
"  <Option name='TRAILER' type='string' description='Template trailer file' default='trailer.dxf'/>"
"  <Option name='FIRST_ENTITY' type='int' description='Identifier of first entity'/>"
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
                                            "<LayerCreationOptionList/>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRDXFDriverOpen;
        poDriver->pfnIdentify = OGRDXFDriverIdentify;
        poDriver->pfnCreate = OGRDXFDriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
