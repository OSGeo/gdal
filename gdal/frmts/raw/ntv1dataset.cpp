/******************************************************************************
 *
 * Project:  Horizontal Datum Formats
 * Purpose:  Implementation of NTv1 datum shift format used in Canada
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault, <even.rouault at spatialys.com>
 * Copyright (c) 2010, Frank Warmerdam
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_srs_api.h"
#include "rawdataset.h"

#include <algorithm>

CPL_CVSID("$Id$")



/**
 * The header for the file, and each grid consists of 12 16byte records.
 * The first half is an ASCII label, and the second half is the value
 * often in a little endian int or double.

the actual grid data is a raster with 2 float64 bands (lat offset, long
offset).  The offset values are in arc seconds.
The grid is flipped in the x and y axis from our usual GDAL orientation.
That is, the first pixel is the south east corner with scanlines going
east to west, and rows from south to north.  As a GDAL dataset we represent
these both in the more conventional orientation.
 */

/************************************************************************/
/* ==================================================================== */
/*                              NTv1Dataset                             */
/* ==================================================================== */
/************************************************************************/

class NTv1Dataset final: public RawDataset
{
  public:
    VSILFILE    *fpImage;  // image data file.

    double      adfGeoTransform[6];

    void        CaptureMetadataItem( char *pszItem );

    CPL_DISALLOW_COPY_ASSIGN(NTv1Dataset)

  public:
    NTv1Dataset();
    ~NTv1Dataset() override;

    CPLErr GetGeoTransform( double * padfTransform ) override;
    const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                              NTv1Dataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             NTv1Dataset()                            */
/************************************************************************/

NTv1Dataset::NTv1Dataset() :
    fpImage(nullptr)
{
    adfGeoTransform[0] =  0.0;
    adfGeoTransform[1] =  1.0;
    adfGeoTransform[2] =  0.0;
    adfGeoTransform[3] =  0.0;
    adfGeoTransform[4] =  0.0;
    adfGeoTransform[5] =  1.0;
}

/************************************************************************/
/*                            ~NTv1Dataset()                            */
/************************************************************************/

NTv1Dataset::~NTv1Dataset()

{
    NTv1Dataset::FlushCache();

    if( fpImage != nullptr )
    {
        CPL_IGNORE_RET_VAL( VSIFCloseL( fpImage ) );
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NTv1Dataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 192 )
        return FALSE;

    if( memcmp(poOpenInfo->pabyHeader, "HEADER  \0\0\0\x0c\0\0\0\0S LAT   ", 24) != 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NTv1Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr ||
        poOpenInfo->eAccess == GA_Update )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NTv1Dataset *poDS = new NTv1Dataset();
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Read the file header.                                           */
/* -------------------------------------------------------------------- */
    char achHeader[192];
    memcpy(achHeader, poOpenInfo->pabyHeader, 192);

    CPL_MSBPTR64(achHeader + 24);
    CPL_MSBPTR64(achHeader + 40);
    CPL_MSBPTR64(achHeader + 56);
    CPL_MSBPTR64(achHeader + 72);
    CPL_MSBPTR64(achHeader + 88);
    CPL_MSBPTR64(achHeader + 104);

    poDS->CaptureMetadataItem( achHeader + 128 );
    poDS->CaptureMetadataItem( achHeader + 144 );

    double s_lat, n_lat, e_long, w_long, lat_inc, long_inc;
    memcpy( &s_lat,  achHeader + 24, 8 );
    memcpy( &n_lat,  achHeader + 40, 8 );
    memcpy( &e_long, achHeader + 56, 8 );
    memcpy( &w_long, achHeader + 72, 8 );
    memcpy( &lat_inc, achHeader + 88, 8 );
    memcpy( &long_inc, achHeader + 104, 8 );

    e_long *= -1;
    w_long *= -1;

    if( long_inc == 0.0 || lat_inc == 0.0 )
    {
        delete poDS;
        return nullptr;
    }
    const double dfXSize = floor((e_long - w_long) / long_inc + 0.5 + 1);
    const double dfYSize = floor((n_lat - s_lat) / lat_inc + 0.5 + 1);
    if( !(dfXSize >= 0 && dfXSize < INT_MAX) ||
        !(dfYSize >= 0 && dfYSize < INT_MAX) )
    {
        delete poDS;
        return nullptr;
    }
    poDS->nRasterXSize = static_cast<int>( dfXSize );
    poDS->nRasterYSize = static_cast<int>( dfYSize );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return nullptr;
    }
    if( poDS->nRasterXSize > INT_MAX / 16 )
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/*                                                                      */
/*      We use unusual offsets to remap from bottom to top, to top      */
/*      to bottom orientation, and also to remap east to west, to       */
/*      west to east.                                                   */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < 2; iBand++ )
    {
        RawRasterBand *poBand =
            new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                               192 + 8*iBand
                                    + (poDS->nRasterXSize-1) * 16
                                    + static_cast<vsi_l_offset>(poDS->nRasterYSize-1) * 16 * poDS->nRasterXSize,
                               -16, -16 * poDS->nRasterXSize,
                               GDT_Float64, !CPL_IS_LSB,
                               RawRasterBand::OwnFP::NO );
        poDS->SetBand( iBand+1, poBand );
    }

    poDS->GetRasterBand(1)->SetDescription( "Latitude Offset (arc seconds)" );
    poDS->GetRasterBand(2)->SetDescription( "Longitude Offset (arc seconds)" );
    poDS->GetRasterBand(2)->SetMetadataItem("positive_value", "west");

/* -------------------------------------------------------------------- */
/*      Setup georeferencing.                                           */
/* -------------------------------------------------------------------- */
    poDS->adfGeoTransform[0] = w_long - long_inc*0.5;
    poDS->adfGeoTransform[1] = long_inc;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = n_lat + lat_inc*0.5;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1 * lat_inc;

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                        CaptureMetadataItem()                         */
/************************************************************************/

void NTv1Dataset::CaptureMetadataItem( char *pszItem )

{
    CPLString osKey;
    CPLString osValue;

    osKey.assign( pszItem, 8 );
    osValue.assign( pszItem+8, 8 );

    SetMetadataItem( osKey.Trim(), osValue.Trim() );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NTv1Dataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NTv1Dataset::_GetProjectionRef()

{
    return SRS_WKT_WGS84_LAT_LONG;
}

/************************************************************************/
/*                         GDALRegister_NTv1()                          */
/************************************************************************/

void GDALRegister_NTv1()

{
    if( GDALGetDriverByName( "NTv1" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "NTv1" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "NTv1 Datum Grid Shift" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dat" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = NTv1Dataset::Open;
    poDriver->pfnIdentify = NTv1Dataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
