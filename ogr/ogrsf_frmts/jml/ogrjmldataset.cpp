/******************************************************************************
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_jml.h"
#include "ogrsf_frmts.h"

// g++ -DHAVE_EXPAT -fPIC -shared -Wall -g -DDEBUG ogr/ogrsf_frmts/jml/*.cpp -o ogr_JML.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/jml -L. -lgdal

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRJMLDataset()                             */
/************************************************************************/

OGRJMLDataset::OGRJMLDataset() :
    poLayer(nullptr),
    fp(nullptr),
    bWriteMode(false)
{}

/************************************************************************/
/*                         ~OGRJMLDataset()                             */
/************************************************************************/

OGRJMLDataset::~OGRJMLDataset()

{
    delete poLayer;

    if ( fp != nullptr )
        VSIFCloseL( fp);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRJMLDataset::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bWriteMode && poLayer == nullptr;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRJMLDataset::GetLayer( int iLayer )

{
    if( iLayer != 0 )
        return nullptr;

    return poLayer;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int OGRJMLDataset::Identify( GDALOpenInfo* poOpenInfo )
{
    return poOpenInfo->nHeaderBytes != 0 &&
      strstr(reinterpret_cast<char*>(poOpenInfo->pabyHeader),
             "<JCSDataFile") != nullptr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* OGRJMLDataset::Open( GDALOpenInfo* poOpenInfo )

{
    if( !Identify(poOpenInfo) || poOpenInfo->fpL == nullptr ||
        poOpenInfo->eAccess == GA_Update )
        return nullptr;

#ifndef HAVE_EXPAT
    CPLError( CE_Failure, CPLE_NotSupported,
              "OGR/JML driver has not been built with read support. "
              "Expat library required" );
    return nullptr;
#else
    OGRJMLDataset* poDS = new OGRJMLDataset();
    poDS->SetDescription( poOpenInfo->pszFilename );

    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    poDS->poLayer = new OGRJMLLayer(
        CPLGetBasename(poOpenInfo->pszFilename), poDS, poDS->fp );

    return poDS;
#endif
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset* OGRJMLDataset::Create( const char *pszFilename,
                                    int /* nXSize */,
                                    int /* nYSize */,
                                    int /* nBands */,
                                    GDALDataType /* eDT */,
                                    char ** /* papszOptions */ )
{
    if (strcmp(pszFilename, "/dev/stdout") == 0)
        pszFilename = "/vsistdout/";

/* -------------------------------------------------------------------- */
/*     Do not override exiting file.                                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszFilename, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "You have to delete %s before being able to create it "
                  "with the JML driver",
                  pszFilename);
        return nullptr;
    }

    OGRJMLDataset* poDS = new OGRJMLDataset();

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    poDS->bWriteMode = true;
    poDS->SetDescription( pszFilename );

    poDS->fp = VSIFOpenL( pszFilename, "w" );
    if( poDS->fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create JML file %s.",
                  pszFilename );
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer * OGRJMLDataset::ICreateLayer( const char * pszLayerName,
                                        OGRSpatialReference * poSRS,
                                        OGRwkbGeometryType /* eType */,
                                        char ** papszOptions )
{
    if (!bWriteMode || poLayer != nullptr)
        return nullptr;

    bool bAddRGBField = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "CREATE_R_G_B_FIELD", "YES"));
    bool bAddOGRStyleField = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "CREATE_OGR_STYLE_FIELD", "NO"));
    bool bClassicGML = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "CLASSIC_GML", "NO"));
    auto poSRSClone = poSRS;
    if( poSRSClone )
    {
        poSRSClone = poSRSClone->Clone();
        poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    poLayer = new OGRJMLWriterLayer( pszLayerName, poSRSClone, this, fp,
                                     bAddRGBField, bAddOGRStyleField,
                                     bClassicGML);
    if( poSRSClone )
        poSRSClone->Release();

    return poLayer;
}

/************************************************************************/
/*                         RegisterOGRJML()                             */
/************************************************************************/

void RegisterOGRJML()
{
    if( GDALGetDriverByName( "JML" ) != nullptr )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "JML" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "OpenJUMP JML" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jml" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/jml.html" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_FEATURE_STYLES, "YES" );

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"   <Option name='CREATE_R_G_B_FIELD' type='boolean' description='Whether to create a R_G_B field' default='YES'/>"
"   <Option name='CREATE_OGR_STYLE_FIELD' type='boolean' description='Whether to create a OGR_STYLE field' default='NO'/>"
"</LayerCreationOptionList>" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                               "<CreationOptionList/>" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime" );

    poDriver->pfnOpen = OGRJMLDataset::Open;
    poDriver->pfnIdentify = OGRJMLDataset::Identify;
    poDriver->pfnCreate = OGRJMLDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
