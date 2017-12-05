/******************************************************************************
 *
 * Project:  Horizontal Datum Formats
 * Purpose:  Implementation of NTv2 datum shift format used in Canada, France,
 *           Australia and elsewhere.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Financial Support: i-cubed (http://www.i-cubed.com)
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

// TODO(schwehr): There are a lot of magic numbers in this driver that should
// be changed to constants and documented.

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_srs_api.h"
#include "rawdataset.h"

#include <algorithm>

CPL_CVSID("$Id$")

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

the actual grid data is a raster with 4 float32 bands (lat offset, long
offset, lat error, long error).  The offset values are in arc seconds.
The grid is flipped in the x and y axis from our usual GDAL orientation.
That is, the first pixel is the south east corner with scanlines going
east to west, and rows from south to north.  As a GDAL dataset we represent
these both in the more conventional orientation.
 */

/************************************************************************/
/* ==================================================================== */
/*                              NTv2Dataset                             */
/* ==================================================================== */
/************************************************************************/

class NTv2Dataset : public RawDataset
{
  public:
    bool        m_bMustSwap;
    VSILFILE    *fpImage;  // image data file.

    int         nRecordLength;
    vsi_l_offset nGridOffset;

    double      adfGeoTransform[6];

    void        CaptureMetadataItem( char *pszItem );

    int         OpenGrid( char *pachGridHeader, vsi_l_offset nDataStart );

  public:
                NTv2Dataset();
    virtual ~NTv2Dataset();

    virtual CPLErr SetGeoTransform( double * padfTransform ) override;
    virtual CPLErr GetGeoTransform( double * padfTransform ) override;
    virtual const char *GetProjectionRef() override;
    virtual void   FlushCache(void) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                              NTv2Dataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             NTv2Dataset()                          */
/************************************************************************/

NTv2Dataset::NTv2Dataset() :
    m_bMustSwap(false),
    fpImage(NULL),
    nRecordLength(0),
    nGridOffset(0)
{
    adfGeoTransform[0] =  0.0;
    adfGeoTransform[1] =  0.0;  // TODO(schwehr): Should this be 1.0?
    adfGeoTransform[2] =  0.0;
    adfGeoTransform[3] =  0.0;
    adfGeoTransform[4] =  0.0;
    adfGeoTransform[5] =  0.0;  // TODO(schwehr): Should this be 1.0?
}

/************************************************************************/
/*                            ~NTv2Dataset()                          */
/************************************************************************/

NTv2Dataset::~NTv2Dataset()

{
    FlushCache();

    if( fpImage != NULL )
    {
        if( VSIFCloseL( fpImage ) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }
}

/************************************************************************/
/*                        SwapPtr32IfNecessary()                        */
/************************************************************************/

static void SwapPtr32IfNecessary( bool bMustSwap, void* ptr )
{
    if( bMustSwap )
    {
        CPL_SWAP32PTR( static_cast<GByte *>(ptr) );
    }
}

/************************************************************************/
/*                        SwapPtr64IfNecessary()                        */
/************************************************************************/

static void SwapPtr64IfNecessary( bool bMustSwap, void* ptr )
{
    if( bMustSwap )
    {
        CPL_SWAP64PTR( static_cast<GByte *>(ptr) );
    }
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void NTv2Dataset::FlushCache()

{
/* -------------------------------------------------------------------- */
/*      Nothing to do in readonly mode, or if nothing seems to have     */
/*      changed metadata wise.                                          */
/* -------------------------------------------------------------------- */
    if( eAccess != GA_Update || !(GetPamFlags() & GPF_DIRTY) )
    {
        RawDataset::FlushCache();
        return;
    }

/* -------------------------------------------------------------------- */
/*      Load grid and file headers.                                     */
/* -------------------------------------------------------------------- */
    const int nRecords = 11;
    const int nRecordSize = 16;
    char achFileHeader[nRecords*nRecordSize] = { '\0' };
    char achGridHeader[nRecords*nRecordSize] = { '\0' };

    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));
    CPL_IGNORE_RET_VAL(
        VSIFReadL( achFileHeader, nRecords, nRecordSize, fpImage ));

    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, nGridOffset, SEEK_SET ));
    CPL_IGNORE_RET_VAL(
        VSIFReadL( achGridHeader, nRecords, nRecordSize, fpImage ));

/* -------------------------------------------------------------------- */
/*      Update the grid, and file headers with any available            */
/*      metadata.  If all available metadata is recognised then mark    */
/*      things "clean" from a PAM point of view.                        */
/* -------------------------------------------------------------------- */
    char **papszMD = GetMetadata();
    bool bSomeLeftOver = false;

    for( int i = 0; papszMD != NULL && papszMD[i] != NULL; i++ )
    {
        const size_t nMinLen = 8;
        char *pszKey = NULL;
        const char *pszValue = CPLParseNameValue( papszMD[i], &pszKey );
        if( pszKey == NULL )
            continue;

        if( EQUAL(pszKey,"GS_TYPE") )
        {
            memcpy( achFileHeader + 3*16+8, "        ", 8 );
            memcpy( achFileHeader + 3*16+8,
                    pszValue,
                    std::min(nMinLen, strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"VERSION") )
        {
            memcpy( achFileHeader + 4*16+8, "        ", 8 );
            memcpy( achFileHeader + 4*16+8,
                    pszValue,
                    std::min(nMinLen, strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"SYSTEM_F") )
        {
            memcpy( achFileHeader + 5*16+8, "        ", 8 );
            memcpy( achFileHeader + 5*16+8,
                    pszValue,
                    std::min(nMinLen, strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"SYSTEM_T") )
        {
            memcpy( achFileHeader + 6*16+8, "        ", 8 );
            memcpy( achFileHeader + 6*16+8,
                    pszValue,
                    std::min(nMinLen, strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"MAJOR_F") )
        {
            double dfValue = CPLAtof(pszValue);
            SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
            memcpy( achFileHeader + 7*16+8, &dfValue, 8 );
        }
        else if( EQUAL(pszKey,"MINOR_F") )
        {
            double dfValue = CPLAtof(pszValue);
            SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
            memcpy( achFileHeader + 8*16+8, &dfValue, 8 );
        }
        else if( EQUAL(pszKey,"MAJOR_T") )
        {
            double dfValue = CPLAtof(pszValue);
            SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
            memcpy( achFileHeader + 9*16+8, &dfValue, 8 );
        }
        else if( EQUAL(pszKey,"MINOR_T") )
        {
            double dfValue = CPLAtof(pszValue);
            SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
            memcpy( achFileHeader + 10*16+8, &dfValue, 8 );
        }
        else if( EQUAL(pszKey,"SUB_NAME") )
        {
            memcpy( achGridHeader + 0*16+8, "        ", 8 );
            memcpy( achGridHeader + 0*16+8,
                    pszValue,
                    std::min(nMinLen, strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"PARENT") )
        {
            memcpy( achGridHeader + 1*16+8, "        ", 8 );
            memcpy( achGridHeader + 1*16+8,
                    pszValue,
                    std::min(nMinLen, strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"CREATED") )
        {
            memcpy( achGridHeader + 2*16+8, "        ", 8 );
            memcpy( achGridHeader + 2*16+8,
                    pszValue,
                    std::min(nMinLen, strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"UPDATED") )
        {
            memcpy( achGridHeader + 3*16+8, "        ", 8 );
            memcpy( achGridHeader + 3*16+8,
                    pszValue,
                    std::min(nMinLen, strlen(pszValue)) );
        }
        else
        {
            bSomeLeftOver = true;
        }

        CPLFree( pszKey );
    }

/* -------------------------------------------------------------------- */
/*      Load grid and file headers.                                     */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, 0, SEEK_SET ));
    CPL_IGNORE_RET_VAL(
        VSIFWriteL( achFileHeader, nRecords, nRecordSize, fpImage ));

    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, nGridOffset, SEEK_SET ));
    CPL_IGNORE_RET_VAL(
        VSIFWriteL( achGridHeader, nRecords, nRecordSize, fpImage ));

/* -------------------------------------------------------------------- */
/*      Clear flags if we got everything, then let pam and below do     */
/*      their flushing.                                                 */
/* -------------------------------------------------------------------- */
    if( !bSomeLeftOver )
        SetPamFlags( GetPamFlags() & (~GPF_DIRTY) );

    RawDataset::FlushCache();
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NTv2Dataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "NTv2:") )
        return TRUE;

    if( poOpenInfo->nHeaderBytes < 64 )
        return FALSE;

    if( !STARTS_WITH_CI(reinterpret_cast<char *>(poOpenInfo->pabyHeader) + 0,
                        "NUM_OREC") )
        return FALSE;

    if( !STARTS_WITH_CI(reinterpret_cast<char *>(poOpenInfo->pabyHeader) + 16,
                        "NUM_SREC") )
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
/*      Are we targeting a particular grid?                             */
/* -------------------------------------------------------------------- */
    CPLString osFilename;
    int iTargetGrid = -1;

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "NTv2:") )
    {
        const char *pszRest = poOpenInfo->pszFilename + 5;

        iTargetGrid = atoi(pszRest);
        while( *pszRest != '\0' && *pszRest != ':' )
            pszRest++;

        if( *pszRest == ':' )
            pszRest++;

        osFilename = pszRest;
    }
    else
    {
        osFilename = poOpenInfo->pszFilename;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NTv2Dataset *poDS = new NTv2Dataset();
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL( osFilename, "rb" );
    else
        poDS->fpImage = VSIFOpenL( osFilename, "rb+" );

    if( poDS->fpImage == NULL )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the file header.                                           */
/* -------------------------------------------------------------------- */
    char achHeader[11*16] = { 0 };
    if (VSIFSeekL( poDS->fpImage, 0, SEEK_SET ) != 0 ||
        VSIFReadL( achHeader, 11, 16, poDS->fpImage ) != 16 )
    {
        delete poDS;
        return NULL;
    }

    const bool bIsLE =
        achHeader[8] == 11 && achHeader[9] == 0 && achHeader[10] == 0 &&
        achHeader[11] == 0;
    const bool bIsBE =
        achHeader[8] == 0 && achHeader[9] == 0 && achHeader[10] == 0 &&
        achHeader[11] == 11;
    if( !bIsLE && !bIsBE )
    {
        delete poDS;
        return NULL;
    }
#ifdef CPL_LSB
    const bool bMustSwap = bIsBE;
#else
    const bool bMustSwap = bIsLE;
#endif
    poDS->m_bMustSwap = bMustSwap;

    SwapPtr32IfNecessary( bMustSwap, achHeader + 2*16 + 8 );
    GInt32 nSubFileCount = 0;
    memcpy( &nSubFileCount, achHeader + 2*16 + 8, 4 );
    if (nSubFileCount <= 0 || nSubFileCount >= 1024)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid value for NUM_FILE : %d", nSubFileCount );
        delete poDS;
        return NULL;
    }

    poDS->CaptureMetadataItem( achHeader + 3*16 );
    poDS->CaptureMetadataItem( achHeader + 4*16 );
    poDS->CaptureMetadataItem( achHeader + 5*16 );
    poDS->CaptureMetadataItem( achHeader + 6*16 );

    double dfValue = 0.0;
    memcpy( &dfValue, achHeader + 7*16 + 8, 8 );
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    CPLString osFValue;
    osFValue.Printf( "%.15g", dfValue );
    poDS->SetMetadataItem( "MAJOR_F", osFValue );

    memcpy( &dfValue, achHeader + 8*16 + 8, 8 );
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    osFValue.Printf( "%.15g", dfValue );
    poDS->SetMetadataItem( "MINOR_F", osFValue );

    memcpy( &dfValue, achHeader + 9*16 + 8, 8 );
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    osFValue.Printf( "%.15g", dfValue );
    poDS->SetMetadataItem( "MAJOR_T", osFValue );

    memcpy( &dfValue, achHeader + 10*16 + 8, 8 );
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    osFValue.Printf( "%.15g", dfValue );
    poDS->SetMetadataItem( "MINOR_T", osFValue );

/* ==================================================================== */
/*      Loop over grids.                                                */
/* ==================================================================== */
    vsi_l_offset nGridOffset = sizeof(achHeader);

    for( int iGrid = 0; iGrid < nSubFileCount; iGrid++ )
    {
        if (VSIFSeekL( poDS->fpImage, nGridOffset, SEEK_SET ) < 0 ||
            VSIFReadL( achHeader, 11, 16, poDS->fpImage ) != 16)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot read header for subfile %d", iGrid );
            delete poDS;
            return NULL;
        }

        for( int i = 4; i <= 9; i++ )
            SwapPtr64IfNecessary( bMustSwap, achHeader + i*16 + 8 );

        SwapPtr32IfNecessary( bMustSwap, achHeader + 10*16 + 8 );

        GUInt32 nGSCount = 0;
        memcpy( &nGSCount, achHeader + 10*16 + 8, 4 );

        CPLString osSubName;
        osSubName.assign( achHeader + 8, 8 );
        osSubName.Trim();

        // If this is our target grid, open it as a dataset.
        if( iTargetGrid == iGrid || (iTargetGrid == -1 && iGrid == 0) )
        {
            if( !poDS->OpenGrid( achHeader, nGridOffset ) )
            {
                delete poDS;
                return NULL;
            }
        }

        // If we are opening the file as a whole, list subdatasets.
        if( iTargetGrid == -1 )
        {
            CPLString osKey;
            CPLString osValue;
            osKey.Printf( "SUBDATASET_%d_NAME", iGrid );
            osValue.Printf( "NTv2:%d:%s", iGrid, osFilename.c_str() );
            poDS->SetMetadataItem( osKey, osValue, "SUBDATASETS" );

            osKey.Printf( "SUBDATASET_%d_DESC", iGrid );
            osValue.Printf( "%s", osSubName.c_str() );
            poDS->SetMetadataItem( osKey, osValue, "SUBDATASETS" );
        }

        nGridOffset += (11 + static_cast<vsi_l_offset>( nGSCount ) ) * 16;
    }

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
/*                              OpenGrid()                              */
/*                                                                      */
/*      Note that the caller will already have byte swapped needed      */
/*      portions of the header.                                         */
/************************************************************************/

int NTv2Dataset::OpenGrid( char *pachHeader, vsi_l_offset nGridOffsetIn )

{
    nGridOffset = nGridOffsetIn;

/* -------------------------------------------------------------------- */
/*      Read the grid header.                                           */
/* -------------------------------------------------------------------- */
    CaptureMetadataItem( pachHeader + 0*16 );
    CaptureMetadataItem( pachHeader + 1*16 );
    CaptureMetadataItem( pachHeader + 2*16 );
    CaptureMetadataItem( pachHeader + 3*16 );

    double s_lat, n_lat, e_long, w_long, lat_inc, long_inc;
    memcpy( &s_lat,  pachHeader + 4*16 + 8, 8 );
    memcpy( &n_lat,  pachHeader + 5*16 + 8, 8 );
    memcpy( &e_long, pachHeader + 6*16 + 8, 8 );
    memcpy( &w_long, pachHeader + 7*16 + 8, 8 );
    memcpy( &lat_inc, pachHeader + 8*16 + 8, 8 );
    memcpy( &long_inc, pachHeader + 9*16 + 8, 8 );

    e_long *= -1;
    w_long *= -1;

    if( long_inc == 0.0 || lat_inc == 0.0 )
        return FALSE;
    const double dfXSize = floor((e_long - w_long) / long_inc + 1.5);
    const double dfYSize = floor((n_lat - s_lat) / lat_inc + 1.5);
    if( !(dfXSize >= 0 && dfXSize < INT_MAX) ||
        !(dfYSize >= 0 && dfYSize < INT_MAX) )
        return FALSE;
    nRasterXSize = static_cast<int>( dfXSize );
    nRasterYSize = static_cast<int>( dfYSize );

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
        return FALSE;
    if( nRasterXSize > INT_MAX / 16 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/*                                                                      */
/*      We use unusual offsets to remap from bottom to top, to top      */
/*      to bottom orientation, and also to remap east to west, to       */
/*      west to east.                                                   */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < 4; iBand++ )
    {
        RawRasterBand *poBand =
            new RawRasterBand( this, iBand+1, fpImage,
                               nGridOffset + 4*iBand + 11*16
                               + (nRasterXSize-1) * 16
                               + static_cast<vsi_l_offset>(nRasterYSize-1) * 16 * nRasterXSize,
                               -16, -16 * nRasterXSize,
                               GDT_Float32, !m_bMustSwap, TRUE, FALSE );
        SetBand( iBand+1, poBand );
    }

    GetRasterBand(1)->SetDescription( "Latitude Offset (arc seconds)" );
    GetRasterBand(2)->SetDescription( "Longitude Offset (arc seconds)" );
    GetRasterBand(3)->SetDescription( "Latitude Error" );
    GetRasterBand(4)->SetDescription( "Longitude Error" );

/* -------------------------------------------------------------------- */
/*      Setup georeferencing.                                           */
/* -------------------------------------------------------------------- */
    adfGeoTransform[0] = (w_long - long_inc*0.5) / 3600.0;
    adfGeoTransform[1] = long_inc / 3600.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = (n_lat + lat_inc*0.5) / 3600.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = (-1 * lat_inc) / 3600.0;

    return TRUE;
}

/************************************************************************/
/*                        CaptureMetadataItem()                         */
/************************************************************************/

void NTv2Dataset::CaptureMetadataItem( char *pszItem )

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

CPLErr NTv2Dataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr NTv2Dataset::SetGeoTransform( double * padfTransform )

{
    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to update geotransform on readonly file." );
        return CE_Failure;
    }

    if( padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Rotated and sheared geotransforms not supported for NTv2.");
        return CE_Failure;
    }

    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );

/* -------------------------------------------------------------------- */
/*      Update grid header.                                             */
/* -------------------------------------------------------------------- */
    char achHeader[11*16] = { '\0' };

    // read grid header
    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, nGridOffset, SEEK_SET ));
    CPL_IGNORE_RET_VAL(VSIFReadL( achHeader, 11, 16, fpImage ));

    // S_LAT
    double dfValue =
        3600 * (adfGeoTransform[3] + (nRasterYSize-0.5) * adfGeoTransform[5]);
    SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
    memcpy( achHeader +  4*16 + 8, &dfValue, 8 );

    // N_LAT
    dfValue = 3600 * (adfGeoTransform[3] + 0.5 * adfGeoTransform[5]);
    SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
    memcpy( achHeader +  5*16 + 8, &dfValue, 8 );

    // E_LONG
    dfValue =
        -3600 * (adfGeoTransform[0] + (nRasterXSize-0.5)*adfGeoTransform[1]);
    SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
    memcpy( achHeader +  6*16 + 8, &dfValue, 8 );

    // W_LONG
    dfValue = -3600 * (adfGeoTransform[0] + 0.5 * adfGeoTransform[1]);
    SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
    memcpy( achHeader +  7*16 + 8, &dfValue, 8 );

    // LAT_INC
    dfValue = -3600 * adfGeoTransform[5];
    SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
    memcpy( achHeader +  8*16 + 8, &dfValue, 8 );

    // LONG_INC
    dfValue = 3600 * adfGeoTransform[1];
    SwapPtr64IfNecessary( m_bMustSwap, &dfValue );
    memcpy( achHeader +  9*16 + 8, &dfValue, 8 );

    // write grid header.
    CPL_IGNORE_RET_VAL(VSIFSeekL( fpImage, nGridOffset, SEEK_SET ));
    CPL_IGNORE_RET_VAL(VSIFWriteL( achHeader, 11, 16, fpImage ));

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
/*                               Create()                               */
/************************************************************************/

GDALDataset *NTv2Dataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize,
                                  int nBands,
                                  GDALDataType eType,
                                  char ** papszOptions )
{
    if( eType != GDT_Float32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                 "Attempt to create NTv2 file with unsupported data type '%s'.",
                  GDALGetDataTypeName( eType ) );
        return NULL;
    }
    if( nBands != 4 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create NTv2 file with unsupported "
                  "band number '%d'.",
                  nBands);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Are we extending an existing file?                              */
/* -------------------------------------------------------------------- */
    const bool bAppend = CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false);

/* -------------------------------------------------------------------- */
/*      Try to open or create file.                                     */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = NULL;
    if( bAppend )
        fp = VSIFOpenL( pszFilename, "rb+" );
    else
        fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to open/create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a file level header if we are creating new.              */
/* -------------------------------------------------------------------- */
    char achHeader[11*16] = { '\0' };
    const char *pszValue = NULL;
    GUInt32 nNumFile = 1;
    bool bMustSwap = false;
    bool bIsLE = false;

    if( !bAppend )
    {
        memset( achHeader, 0, sizeof(achHeader) );

        bIsLE = EQUAL(CSLFetchNameValueDef(papszOptions,"ENDIANNESS", "LE"), "LE");
#ifdef CPL_LSB
        bMustSwap = !bIsLE;
#else
        bMustSwap = bIsLE;
#endif

        memcpy( achHeader +  0*16, "NUM_OREC", 8 );
        int nNumOrec = 11;
        SwapPtr32IfNecessary( bMustSwap, &nNumOrec );
        memcpy( achHeader + 0*16 + 8, &nNumOrec, 4 );

        memcpy( achHeader +  1*16, "NUM_SREC", 8 );
        int nNumSrec = 11;
        SwapPtr32IfNecessary( bMustSwap, &nNumSrec );
        memcpy( achHeader + 1*16 + 8, &nNumSrec, 4 );

        memcpy( achHeader +  2*16, "NUM_FILE", 8 );
        SwapPtr32IfNecessary( bMustSwap, &nNumFile );
        memcpy( achHeader + 2*16 + 8, &nNumFile, 4 );
        SwapPtr32IfNecessary( bMustSwap, &nNumFile );

        const size_t nMinLen = 16;
        memcpy( achHeader +  3*16, "GS_TYPE         ", 16 );
        pszValue = CSLFetchNameValueDef( papszOptions, "GS_TYPE", "SECONDS");
        memcpy( achHeader +  3*16+8,
                pszValue,
                std::min(nMinLen, strlen(pszValue)) );

        memcpy( achHeader +  4*16, "VERSION         ", 16 );
        pszValue = CSLFetchNameValueDef( papszOptions, "VERSION", "" );
        memcpy( achHeader +  4*16+8, pszValue, std::min(nMinLen, strlen(pszValue)) );

        memcpy( achHeader +  5*16, "SYSTEM_F        ", 16 );
        pszValue = CSLFetchNameValueDef( papszOptions, "SYSTEM_F", "" );
        memcpy( achHeader +  5*16+8, pszValue, std::min(nMinLen, strlen(pszValue)) );

        memcpy( achHeader +  6*16, "SYSTEM_T        ", 16 );
        pszValue = CSLFetchNameValueDef( papszOptions, "SYSTEM_T", "" );
        memcpy( achHeader +  6*16+8, pszValue, std::min(nMinLen, strlen(pszValue)) );

        memcpy( achHeader +  7*16, "MAJOR_F ", 8);
        memcpy( achHeader +  8*16, "MINOR_F ", 8 );
        memcpy( achHeader +  9*16, "MAJOR_T ", 8 );
        memcpy( achHeader + 10*16, "MINOR_T ", 8 );

        CPL_IGNORE_RET_VAL(VSIFWriteL( achHeader, 1, sizeof(achHeader), fp ));
    }

/* -------------------------------------------------------------------- */
/*      Otherwise update the header with an increased subfile count,    */
/*      and advanced to the last record of the file.                    */
/* -------------------------------------------------------------------- */
    else
    {
        CPL_IGNORE_RET_VAL(VSIFSeekL( fp, 0, SEEK_SET ));
        CPL_IGNORE_RET_VAL(VSIFReadL( achHeader, 1, 16, fp ));

        bIsLE =
            achHeader[8] == 11 &&
            achHeader[9] == 0 &&
            achHeader[10] == 0 &&
            achHeader[11] == 0;
        const bool bIsBE =
            achHeader[8] == 0 &&
            achHeader[9] == 0 &&
            achHeader[10] == 0 &&
            achHeader[11] == 11;
        if( !bIsLE && !bIsBE )
        {
            VSIFCloseL(fp);
            return NULL;
        }
#ifdef CPL_LSB
        bMustSwap = bIsBE;
#else
        bMustSwap = bIsLE;
#endif

        CPL_IGNORE_RET_VAL(VSIFSeekL( fp, 2*16 + 8, SEEK_SET ));
        CPL_IGNORE_RET_VAL(VSIFReadL( &nNumFile, 1, 4, fp ));
        SwapPtr32IfNecessary( bMustSwap, &nNumFile );

        nNumFile++;

        SwapPtr32IfNecessary( bMustSwap, &nNumFile );
        CPL_IGNORE_RET_VAL(VSIFSeekL( fp, 2*16 + 8, SEEK_SET ));
        CPL_IGNORE_RET_VAL(VSIFWriteL( &nNumFile, 1, 4, fp ));
        SwapPtr32IfNecessary( bMustSwap, &nNumFile );

        CPL_IGNORE_RET_VAL(VSIFSeekL( fp, 0, SEEK_END ));
        const vsi_l_offset nEnd = VSIFTellL( fp );
        CPL_IGNORE_RET_VAL(VSIFSeekL( fp, nEnd-16, SEEK_SET ));
    }

/* -------------------------------------------------------------------- */
/*      Write the grid header.                                          */
/* -------------------------------------------------------------------- */
    memset( achHeader, 0, sizeof(achHeader) );

    const size_t nMinLen = 16;

    memcpy( achHeader +  0*16, "SUB_NAME        ", 16 );
    pszValue = CSLFetchNameValueDef( papszOptions, "SUB_NAME", "" );
    memcpy( achHeader +  0*16+8,
            pszValue,
            std::min(nMinLen, strlen(pszValue)) );

    memcpy( achHeader +  1*16, "PARENT          ", 16 );
    pszValue = CSLFetchNameValueDef( papszOptions, "PARENT", "NONE" );
    memcpy( achHeader +  1*16+8,
            pszValue,
            std::min(nMinLen, strlen(pszValue)) );

    memcpy( achHeader +  2*16, "CREATED         ", 16 );
    pszValue = CSLFetchNameValueDef( papszOptions, "CREATED", "" );
    memcpy( achHeader +  2*16+8,
            pszValue,
            std::min(nMinLen, strlen(pszValue)) );

    memcpy( achHeader +  3*16, "UPDATED         ", 16 );
    pszValue = CSLFetchNameValueDef( papszOptions, "UPDATED", "" );
    memcpy( achHeader + 3*16+8,
            pszValue,
            std::min(nMinLen, strlen(pszValue)) );

    double dfValue;

    memcpy( achHeader +  4*16, "S_LAT   ", 8 );
    dfValue = 0;
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    memcpy( achHeader +  4*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  5*16, "N_LAT   ", 8 );
    dfValue = nYSize-1;
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    memcpy( achHeader +  5*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  6*16, "E_LONG  ", 8 );
    dfValue = -1*(nXSize-1);
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    memcpy( achHeader +  6*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  7*16, "W_LONG  ", 8 );
    dfValue = 0;
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    memcpy( achHeader +  7*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  8*16, "LAT_INC ", 8 );
    dfValue = 1;
    SwapPtr64IfNecessary( bMustSwap, &dfValue );
    memcpy( achHeader +  8*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  9*16, "LONG_INC", 8 );
    memcpy( achHeader +  9*16 + 8, &dfValue, 8 );

    memcpy( achHeader + 10*16, "GS_COUNT", 8 );
    GUInt32 nGSCount = nXSize * nYSize;
    SwapPtr32IfNecessary( bMustSwap, &nGSCount );
    memcpy( achHeader + 10*16+8, &nGSCount, 4 );

    CPL_IGNORE_RET_VAL(VSIFWriteL( achHeader, 1, sizeof(achHeader), fp ));

/* -------------------------------------------------------------------- */
/*      Write zeroed grid data.                                         */
/* -------------------------------------------------------------------- */
    memset( achHeader, 0, 16 );

    // Use -1 (0x000080bf) as the default error value.
    memset( achHeader + ((bIsLE) ? 10 : 9), 0x80, 1 );
    memset( achHeader + ((bIsLE) ? 11 : 8), 0xbf, 1 );
    memset( achHeader + ((bIsLE) ? 14 : 13), 0x80, 1 );
    memset( achHeader + ((bIsLE) ? 15 : 12), 0xbf, 1 );

    for( int i = 0; i < nXSize * nYSize; i++ )
        CPL_IGNORE_RET_VAL(VSIFWriteL( achHeader, 1, 16, fp ));

/* -------------------------------------------------------------------- */
/*      Write the end record.                                           */
/* -------------------------------------------------------------------- */
    memcpy( achHeader, "END     ", 8 );
    memset( achHeader + 8, 0, 8 );
    CPL_IGNORE_RET_VAL(VSIFWriteL( achHeader, 1, 16, fp ));
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

    if( nNumFile == 1 )
      return reinterpret_cast<GDALDataset *>(
          GDALOpen( pszFilename, GA_Update ) );

    CPLString osSubDSName;
    osSubDSName.Printf( "NTv2:%d:%s", nNumFile-1, pszFilename );
    return reinterpret_cast<GDALDataset *>(
        GDALOpen( osSubDSName, GA_Update ) );
}

/************************************************************************/
/*                         GDALRegister_NTv2()                          */
/************************************************************************/

void GDALRegister_NTv2()

{
    if( GDALGetDriverByName( "NTv2" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "NTv2" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "NTv2 Datum Grid Shift" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gsb" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Float32" );

    poDriver->pfnOpen = NTv2Dataset::Open;
    poDriver->pfnIdentify = NTv2Dataset::Identify;
    poDriver->pfnCreate = NTv2Dataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
