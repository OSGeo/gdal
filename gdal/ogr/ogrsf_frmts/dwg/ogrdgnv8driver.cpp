/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include "ogr_dgnv8.h"
#include "cpl_conv.h"
#include "ogrteigha.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRDGNV8DriverUnload()                       */
/************************************************************************/

static void OGRDGNV8DriverUnload( GDALDriver* )
{
    CPLDebug("DGNv8", "Driver cleanup");
    OGRTEIGHADeinitialize();
}

/************************************************************************/
/*                         OGRDGNV8DriverIdentify()                     */
/************************************************************************/

static int OGRDGNV8DriverIdentify( GDALOpenInfo* poOpenInfo )

{
    VSIStatBuf sStat;
    if(  poOpenInfo->fpL != nullptr &&
         poOpenInfo->nHeaderBytes >= 512 )
    {
        // Is it a DGN v7 cell library?
        if( poOpenInfo->pabyHeader[0] == 0x08
            && poOpenInfo->pabyHeader[1] == 0x05
            && poOpenInfo->pabyHeader[2] == 0x17
            && poOpenInfo->pabyHeader[3] == 0x00 )
        {
            return GDALGetDriverByName("DGN") == nullptr &&
                   VSIStat(poOpenInfo->pszFilename, &sStat) == 0;
        }

        // Is it a DGN v7 regular 2D or 3D file?
        if( (poOpenInfo->pabyHeader[0] == 0x08 ||
             poOpenInfo->pabyHeader[0] == 0xC8)
            && poOpenInfo->pabyHeader[1] == 0x09
            && poOpenInfo->pabyHeader[2] == 0xFE
            && poOpenInfo->pabyHeader[3] == 0x02 )
        {
            return GDALGetDriverByName("DGN") == nullptr &&
                   VSIStat(poOpenInfo->pszFilename, &sStat) == 0;
        }
    }

    return poOpenInfo->fpL != nullptr &&
           poOpenInfo->nHeaderBytes >= 512 &&
           EQUAL( CPLGetExtension(poOpenInfo->pszFilename), "DGN" ) &&
           memcmp(poOpenInfo->pabyHeader,
                  "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) == 0 &&
           VSIStat(poOpenInfo->pszFilename, &sStat) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDGNV8DriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRDGNV8DriverIdentify(poOpenInfo))
        return nullptr;

    if( !OGRTEIGHAInitialize() )
        return nullptr;

    OGRDGNV8DataSource *poDS = new OGRDGNV8DataSource(OGRDGNV8GetServices());
    if( !poDS->Open( poOpenInfo->pszFilename,
                     poOpenInfo->eAccess == GA_Update) )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

static GDALDataset *OGRDGNV8DriverCreate( const char * pszName,
                                        int /* nBands */,
                                        int /* nXSize */,
                                        int /* nYSize */,
                                        GDALDataType /* eDT */,
                                        char **papszOptions )
{
    if( !OGRTEIGHAInitialize() )
        return nullptr;

    OGRDGNV8DataSource *poDS = new OGRDGNV8DataSource(OGRDGNV8GetServices());
    if( !poDS->PreCreate( pszName, papszOptions ) )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         RegisterOGRDGNV8()                           */
/************************************************************************/

void RegisterOGRDGNV8()

{
    if( GDALGetDriverByName( "DGNV8" ) != nullptr )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "DGNV8" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Microstation DGNv8" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dgn" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/dgnv8.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_FEATURE_STYLES, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='SEED' type='string' "
    "description='Filename of seed file to use'/>"
"  <Option name='COPY_SEED_FILE_COLOR_TABLE' type='boolean' "
    "description='whether the color table should be copied from the "
    "seed file.' default='NO'/>"
"  <Option name='COPY_SEED_FILE_MODEL' type='boolean' "
    "description='whether the existing models (without their graphic "
    "contents) should be copied from the seed file.' default='YES'/>"
"  <Option name='COPY_SEED_FILE_MODEL_CONTROL_ELEMENTS' type='boolean' "
    "description='whether the existing control elements of models should be "
    "copied from the seed file.' default='YES'/>"
"  <Option name='APPLICATION' type='string' "
    "description='Set Application field in header'/>"
"  <Option name='TITLE' type='string' "
    "description='Set Title field in header'/>"
"  <Option name='SUBJECT' type='string' "
    "description='Set Subject field in header'/>"
"  <Option name='AUTHOR' type='string' "
    "description='Set Author field in header'/>"
"  <Option name='KEYWORDS' type='string' "
    "description='Set Keywords field in header'/>"
"  <Option name='TEMPLATE' type='string' "
    "description='Set Template field in header'/>"
"  <Option name='COMMENTS' type='string' "
    "description='Set Comments field in header'/>"
"  <Option name='LAST_SAVED_BY' type='string' "
    "description='Set LastSavedBy field in header'/>"
"  <Option name='REVISION_NUMBER' type='string' "
    "description='Set RevisionNumber field in header'/>"
"  <Option name='CATEGORY' type='string' "
    "description='Set Category field in header'/>"
"  <Option name='MANAGER' type='string' "
    "description='Set Manager field in header'/>"
"  <Option name='COMPANY' type='string' "
    "description='Set Company field in header'/>"
"</CreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='DESCRIPTION' type='string' "
    "description='Description of the layer/model'/>"
"  <Option name='DIM' type='int' "
    "description='Dimension (2 or 3) of the layer/model'/>"
"</LayerCreationOptionList>" );

    poDriver->pfnOpen = OGRDGNV8DriverOpen;
    poDriver->pfnIdentify = OGRDGNV8DriverIdentify;
    poDriver->pfnCreate = OGRDGNV8DriverCreate;
    poDriver->pfnUnloadDriver = OGRDGNV8DriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
