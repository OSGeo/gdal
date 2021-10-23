/******************************************************************************
 *
 * Project:  REC Translator
 * Purpose:  Implements EpiInfo .REC driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_rec.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRRECDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->fpL == nullptr ||
        !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "REC") )
    {
        return nullptr;
    }

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("REC") )
    {
        return nullptr;
    }

    OGRRECDataSource *poDS = new OGRRECDataSource();
    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    if( poDS != nullptr && poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "REC Driver doesn't support update." );
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRREC()                          */
/************************************************************************/

void RegisterOGRREC()

{
    if( GDALGetDriverByName( "REC" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "REC" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "rec" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "EPIInfo .REC " );
    poDriver->SetMetadataItem( GDAL_DCAP_NONSPATIAL, "YES" );

    poDriver->pfnOpen = OGRRECDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
