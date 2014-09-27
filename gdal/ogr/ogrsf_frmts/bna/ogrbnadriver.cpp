/******************************************************************************
 * $Id: ogrbnadriver.cpp
 *
 * Project:  BNA Translator
 * Purpose:  Implements OGRBNADriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_bna.h"
#include "cpl_conv.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRBNADriverOpen( GDALOpenInfo* poOpenInfo )

{
// -------------------------------------------------------------------- 
//      Does this appear to be a .bna file?
// --------------------------------------------------------------------
    if( poOpenInfo->fpL == NULL ||
        !(EQUAL( CPLGetExtension(poOpenInfo->pszFilename), "bna" )
           || ((EQUALN( poOpenInfo->pszFilename, "/vsigzip/", 9) ||
                EQUALN( poOpenInfo->pszFilename, "/vsizip/", 8)) &&
               (strstr( poOpenInfo->pszFilename, ".bna") ||
                strstr( poOpenInfo->pszFilename, ".BNA")))) )
    {
        return NULL;
    }

    OGRBNADataSource   *poDS = new OGRBNADataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRBNADriverCreate( const char * pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
    OGRBNADataSource   *poDS = new OGRBNADataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRBNADriverDelete( const char *pszFilename )

{
    if( VSIUnlink( pszFilename ) == 0 )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRBNA()                           */
/************************************************************************/

void RegisterOGRBNA()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "BNA" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "BNA" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Atlas BNA" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "bna" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_bna.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
#ifdef WIN32
"  <Option name='LINEFORMAT' type='string-select' description='end-of-line sequence' default='CRLF'>"
#else
"  <Option name='LINEFORMAT' type='string-select' description='end-of-line sequence' default='LF'>"
#endif
"    <Value>CRLF</Value>"
"    <Value>LF</Value>"
"  </Option>"
"  <Option name='MULTILINE' type='boolean' description='Whether coordinates should be put on the same line' default='NO'/>"
"  <Option name='NB_IDS' type='string-select' description='Number of identifiers per record' default='2'>"
"    <Value>2</Value>"
"    <Value>3</Value>"
"    <Value>4</Value>"
"    <Value>NB_SOURCE_FIELDS</Value>"
"  </Option>"
"  <Option name='ELLIPSES_AS_ELLIPSES' type='boolean' description='Whether ellipses and circles should be recognized and written as such, instead of polygons' default='YES'/>"
"  <Option name='NB_PAIRS_PER_LINE' type='int' description='Maximum number of coordinate pair per line in multiline format'/>"
"  <Option name='COORDINATE_PRECISION' type='int' description='Number of decimal for coordinates' default='10'/>"
"</CreationOptionList>");
        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST, "<LayerCreationOptionList/>");

        poDriver->pfnOpen = OGRBNADriverOpen;
        poDriver->pfnCreate = OGRBNADriverCreate;
        poDriver->pfnDelete = OGRBNADriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
