/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  OGRAVCBinDriver implementation (Arc/Info Binary Coverages)
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

#include "ogr_avc.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRAVCBinDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update )
        return nullptr;
    if( !poOpenInfo->bStatOK )
        return nullptr;
    if( poOpenInfo->fpL != nullptr )
    {
        char** papszSiblingFiles = poOpenInfo->GetSiblingFiles();
        if( papszSiblingFiles != nullptr )
        {
            bool bFoundCandidateFile = false;
            for( int i = 0; papszSiblingFiles[i] != nullptr; i++ )
            {
                if( EQUAL(CPLGetExtension(papszSiblingFiles[i]), "ADF") )
                {
                    bFoundCandidateFile = true;
                    break;
                }
            }
            if( !bFoundCandidateFile )
                return nullptr;
        }
    }

    OGRAVCBinDataSource *poDS = new OGRAVCBinDataSource();

    if( poDS->Open( poOpenInfo->pszFilename, TRUE )
        && poDS->GetLayerCount() > 0 )
    {
        return poDS;
    }
    delete poDS;

    return nullptr;
}

/************************************************************************/
/*                           RegisterOGRAVC()                           */
/************************************************************************/

void RegisterOGRAVCBin()

{
    if( GDALGetDriverByName( "AVCBin" ) != nullptr )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "AVCBin" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Arc/Info Binary Coverage" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/avcbin.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->pfnOpen = OGRAVCBinDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
