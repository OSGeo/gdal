/******************************************************************************
 * $Id: ogravcbindriver.cpp 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  OGR
 * Purpose:  OGRAVCE00Driver implementation (Arc/Info E00ary Coverages)
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

CPL_CVSID("$Id: ogravcbindriver.cpp 10645 2007-01-18 02:22:39Z warmerdam $");

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRAVCE00DriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRAVCE00DataSource *poDSE00;

    if( poOpenInfo->eAccess == GA_Update )
        return NULL;
    if( !poOpenInfo->bStatOK )
        return NULL;
    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "E00") )
        return NULL;

    poDSE00 = new OGRAVCE00DataSource();

    if( poDSE00->Open( poOpenInfo->pszFilename, TRUE )
        && poDSE00->GetLayerCount() > 0 )
    {
        return poDSE00;
    }
    delete poDSE00;

    return NULL;
}

/************************************************************************/
/*                           RegisterOGRAVC()                           */
/************************************************************************/

void RegisterOGRAVCE00()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "AVCE00" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "AVCE00" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Arc/Info E00 (ASCII) Coverage" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "e00" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_avce00.html" );

        poDriver->pfnOpen = OGRAVCE00DriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
