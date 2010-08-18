/******************************************************************************
 * $Id: landataset.cpp 17117 2009-05-25 19:26:01Z warmerdam $
 *
 * Project:  Horizontal Datum Formats
 * Purpose:  Implementation of NTv2 datum shift format used in Canada, France, 
 *           Australia and elsewhere.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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

#include "rawdataset.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id: landataset.cpp 17117 2009-05-25 19:26:01Z warmerdam $");

/** 
 * The header for the file, and each grid consists of 11 16byte records.
 * The first half is an ASCII label, and the second half is the value
 * often in a little endian int or float. 
 *
 * Example:

00000000  4e 55 4d 5f 4f 52 45 43  0b 00 00 00 00 00 00 00  |NUM_OREC........|
00000010  4e 55 4d 5f 53 52 45 43  0b 00 00 00 00 00 00 00  |NUM_SREC........|
00000020  4e 55 4d 5f 46 49 4c 45  01 00 00 00 00 00 00 00  |NUM_FILE........|
00000030  47 53 5f 54 59 50 45 20  53 45 43 4f 4e 44 53 20  |GS_TYPE SECONDS |
00000040  56 45 52 53 49 4f 4e 20  49 47 4e 30 37 5f 30 31  |VERSION IGN07_01|
00000050  53 59 53 54 45 4d 5f 46  4e 54 46 20 20 20 20 20  |SYSTEM_FNTF     |
00000060  53 59 53 54 45 4d 5f 54  52 47 46 39 33 20 20 20  |SYSTEM_TRGF93   |
00000070  4d 41 4a 4f 52 5f 46 20  cd cc cc 4c c2 54 58 41  |MAJOR_F ...L.TXA|
00000080  4d 49 4e 4f 52 5f 46 20  00 00 00 c0 88 3f 58 41  |MINOR_F .....?XA|
00000090  4d 41 4a 4f 52 5f 54 20  00 00 00 40 a6 54 58 41  |MAJOR_T ...@.TXA|
000000a0  4d 49 4e 4f 52 5f 54 20  27 e0 1a 14 c4 3f 58 41  |MINOR_T '....?XA|
000000b0  53 55 42 5f 4e 41 4d 45  46 52 41 4e 43 45 20 20  |SUB_NAMEFRANCE  |
000000c0  50 41 52 45 4e 54 20 20  4e 4f 4e 45 20 20 20 20  |PARENT  NONE    |
000000d0  43 52 45 41 54 45 44 20  33 31 2f 31 30 2f 30 37  |CREATED 31/10/07|
000000e0  55 50 44 41 54 45 44 20  20 20 20 20 20 20 20 20  |UPDATED         |
000000f0  53 5f 4c 41 54 20 20 20  00 00 00 00 80 04 02 41  |S_LAT   .......A|
00000100  4e 5f 4c 41 54 20 20 20  00 00 00 00 00 da 06 41  |N_LAT   .......A|
00000110  45 5f 4c 4f 4e 47 20 20  00 00 00 00 00 94 e1 c0  |E_LONG  ........|
00000120  57 5f 4c 4f 4e 47 20 20  00 00 00 00 00 56 d3 40  |W_LONG  .....V.@|
00000130  4c 41 54 5f 49 4e 43 20  00 00 00 00 00 80 76 40  |LAT_INC ......v@|
00000140  4c 4f 4e 47 5f 49 4e 43  00 00 00 00 00 80 76 40  |LONG_INC......v@|
00000150  47 53 5f 43 4f 55 4e 54  a4 43 00 00 00 00 00 00  |GS_COUNT.C......|
00000160  94 f7 c1 3e 70 ee a3 3f  2a c7 84 3d ff 42 af 3d  |...>p..?*..=.B.=|

 */

/************************************************************************/
/* ==================================================================== */
/*				NTv2Dataset				*/
/* ==================================================================== */
/************************************************************************/

class NTv2Dataset : public RawDataset
{
  public:
    FILE	*fpImage;	// image data file.

    int         nRecordLength;
    
    double      adfGeoTransform[6];

    void        CaptureMetadataItem( char *pszItem );

  public:
    		NTv2Dataset();
    	        ~NTv2Dataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*				NTv2Dataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             NTv2Dataset()                          */
/************************************************************************/

NTv2Dataset::NTv2Dataset()
{
    fpImage = NULL;
}

/************************************************************************/
/*                            ~NTv2Dataset()                          */
/************************************************************************/

NTv2Dataset::~NTv2Dataset()

{
    FlushCache();

    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NTv2Dataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 64 )
        return FALSE;

    if( !EQUALN((const char *)poOpenInfo->pabyHeader + 0, "NUM_OREC", 8 ) )
        return FALSE;

    if( !EQUALN((const char *)poOpenInfo->pabyHeader +16, "NUM_SREC", 8 ) )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NTv2Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NTv2Dataset 	*poDS;

    poDS = new NTv2Dataset();

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( poDS->fpImage == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the file header.                                           */
/* -------------------------------------------------------------------- */
    char  achHeader[11*16];
    GInt32 nSubFileCount;
    int i;

    VSIFSeekL( poDS->fpImage, 0, SEEK_SET );
    VSIFReadL( achHeader, 11, 16, poDS->fpImage );

    CPL_LSBPTR32( achHeader + 2*16 + 8 );
    memcpy( &nSubFileCount, achHeader + 2*16 + 8, 4 );

    poDS->CaptureMetadataItem( achHeader + 3*16 );
    poDS->CaptureMetadataItem( achHeader + 4*16 );
    poDS->CaptureMetadataItem( achHeader + 5*16 );
    poDS->CaptureMetadataItem( achHeader + 6*16 );

/* -------------------------------------------------------------------- */
/*      Read the grid header.                                           */
/* -------------------------------------------------------------------- */
    double s_lat, n_lat, e_long, w_long, lat_inc, long_inc;

    VSIFSeekL( poDS->fpImage, 11*16, SEEK_SET );
    VSIFReadL( achHeader, 11, 16, poDS->fpImage );

    poDS->CaptureMetadataItem( achHeader + 0*16 );
    poDS->CaptureMetadataItem( achHeader + 1*16 );
    poDS->CaptureMetadataItem( achHeader + 2*16 );
    poDS->CaptureMetadataItem( achHeader + 3*16 );

    for( i = 4; i <= 9; i++ )
        CPL_LSBPTR64( achHeader + i*16 + 8 );

    memcpy( &s_lat,  achHeader + 4*16 + 8, 8 );
    memcpy( &n_lat,  achHeader + 5*16 + 8, 8 );
    memcpy( &e_long, achHeader + 6*16 + 8, 8 );
    memcpy( &w_long, achHeader + 7*16 + 8, 8 );
    memcpy( &lat_inc, achHeader + 8*16 + 8, 8 );
    memcpy( &long_inc, achHeader + 9*16 + 8, 8 );

    e_long *= -1;
    w_long *= -1;

    poDS->nRasterXSize = (int) floor((e_long - w_long) / long_inc + 1.5);
    poDS->nRasterYSize = (int) floor((n_lat - s_lat) / lat_inc + 1.5);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 
        1, new RawRasterBand( poDS, 1, poDS->fpImage, 
                              VSIFTellL( poDS->fpImage ),
                              16, 16 * poDS->nRasterXSize,
                              GDT_Float32, CPL_IS_LSB, TRUE, FALSE ) );

    poDS->SetBand( 
        2, new RawRasterBand( poDS, 2, poDS->fpImage, 
                              VSIFTellL( poDS->fpImage ) + 4,
                              16, 16 * poDS->nRasterXSize,
                              GDT_Float32, CPL_IS_LSB, TRUE, FALSE ) );

/* -------------------------------------------------------------------- */
/*      Setup georeferencing.                                           */
/* -------------------------------------------------------------------- */
    poDS->adfGeoTransform[0] = (w_long - long_inc*0.5) / 3600.0;
    poDS->adfGeoTransform[1] = long_inc / 3600.0;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = (n_lat + lat_inc*0.5) / 3600.0;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = (-1 * lat_inc) / 3600.0;

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                        CaptureMetadataItem()                         */
/************************************************************************/

void NTv2Dataset::CaptureMetadataItem( char *pszItem )

{
    CPLString osKey, osValue;

    osKey.assign( pszItem, 8 );
    osValue.assign( pszItem+8, 8 );

    SetMetadataItem( osKey.Trim(), osValue.Trim() );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NTv2Dataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NTv2Dataset::GetProjectionRef()

{
    return SRS_WKT_WGS84;
}

/************************************************************************/
/*                        GDALRegister_NTv2()                         */
/************************************************************************/

void GDALRegister_NTv2()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "NTv2" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "NTv2" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NTv2 Datum Grid Shift" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = NTv2Dataset::Open;
        poDriver->pfnIdentify = NTv2Dataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

