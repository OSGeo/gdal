/******************************************************************************
 * $Id$
 *
 * Project:  JML Translator
 * Purpose:  Implements OGRJMLDataset class
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogr_jml.h"
#include "cpl_conv.h"
#include "cpl_string.h"

extern "C" void RegisterOGRJML();

// g++ -DHAVE_EXPAT -fPIC -shared -Wall -g -DDEBUG ogr/ogrsf_frmts/jml/*.cpp -o ogr_JML.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/jml -L. -lgdal

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRJMLDataset()                             */
/************************************************************************/

OGRJMLDataset::OGRJMLDataset()

{
    poLayer = NULL;

    fp = NULL;

    bWriteMode = FALSE;
}

/************************************************************************/
/*                         ~OGRJMLDataset()                             */
/************************************************************************/

OGRJMLDataset::~OGRJMLDataset()

{
    delete poLayer;

    if ( fp != NULL )
        VSIFCloseL( fp);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRJMLDataset::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bWriteMode && poLayer == NULL;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRJMLDataset::GetLayer( int iLayer )

{
    if( iLayer != 0 )
        return NULL;
    else
        return poLayer;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int OGRJMLDataset::Identify( GDALOpenInfo* poOpenInfo )
{
    return poOpenInfo->nHeaderBytes != 0 &&
           strstr((const char*)poOpenInfo->pabyHeader, "<JCSDataFile") != NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* OGRJMLDataset::Open( GDALOpenInfo* poOpenInfo )

{
    if( !Identify(poOpenInfo) || poOpenInfo->fpL == NULL ||
        poOpenInfo->eAccess == GA_Update )
        return NULL;

#ifndef HAVE_EXPAT
    CPLError(CE_Failure, CPLE_NotSupported,
             "OGR/JML driver has not been built with read support. Expat library required");
    return NULL;
#else
    OGRJMLDataset* poDS = new OGRJMLDataset();
    poDS->SetDescription( poOpenInfo->pszFilename );

    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;

    poDS->poLayer = new OGRJMLLayer( CPLGetBasename(poOpenInfo->pszFilename), poDS, poDS->fp);

    return poDS;
#endif
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset* OGRJMLDataset::Create( const char *pszFilename, 
                                CPL_UNUSED int nXSize,
                                CPL_UNUSED int nYSize,
                                CPL_UNUSED int nBands,
                                CPL_UNUSED GDALDataType eDT,
                                CPL_UNUSED char **papszOptions )
{
    if (strcmp(pszFilename, "/dev/stdout") == 0)
        pszFilename = "/vsistdout/";

/* -------------------------------------------------------------------- */
/*     Do not override exiting file.                                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszFilename, &sStatBuf ) == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "You have to delete %s before being able to create it with the JML driver",
                 pszFilename);
        return NULL;
    }

    OGRJMLDataset* poDS = new OGRJMLDataset();

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    poDS->bWriteMode = TRUE;
    poDS->SetDescription( pszFilename );

    poDS->fp = VSIFOpenL( pszFilename, "w" );
    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create JML file %s.", 
                  pszFilename );
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer * OGRJMLDataset::ICreateLayer( const char * pszLayerName,
                                             CPL_UNUSED OGRSpatialReference *poSRS,
                                             CPL_UNUSED OGRwkbGeometryType eType,
                                             char ** papszOptions )
{
    if (!bWriteMode || poLayer != NULL)
        return NULL;

    int bAddRGBField = CSLTestBoolean(
        CSLFetchNameValueDef(papszOptions, "CREATE_R_G_B_FIELD", "YES"));
    int bAddOGRStyleField = CSLTestBoolean(
        CSLFetchNameValueDef(papszOptions, "CREATE_OGR_STYLE_FIELD", "NO"));
    int bClassicGML = CSLTestBoolean(
        CSLFetchNameValueDef(papszOptions, "CLASSIC_GML", "NO"));
    poLayer = new OGRJMLWriterLayer( pszLayerName, this, fp,
                                          bAddRGBField, bAddOGRStyleField,
                                          bClassicGML);

    return poLayer;
}

/************************************************************************/
/*                         RegisterOGRJML()                             */
/************************************************************************/

extern "C"
{

void RegisterOGRJML()
{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "JML" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "JML" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "OpenJUMP JML" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jml" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_jml.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"   <Option name='CREATE_R_G_B_FIELD' type='boolean' description='Whether to create a R_G_B field' default='YES'/>"
"   <Option name='CREATE_OGR_STYLE_FIELD' type='boolean' description='Whether to create a OGR_STYLE field' default='NO'/>"
"</LayerCreationOptionList>" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList/>"
);

        poDriver->pfnOpen = OGRJMLDataset::Open;
        poDriver->pfnIdentify = OGRJMLDataset::Identify;
        poDriver->pfnCreate = OGRJMLDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

}
