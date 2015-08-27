/******************************************************************************
 * $Id$
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

#include "rawdataset.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

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
/*				NTv2Dataset				*/
/* ==================================================================== */
/************************************************************************/

class NTv2Dataset : public RawDataset
{
  public:
    VSILFILE	*fpImage;	// image data file.

    int         nRecordLength;
    vsi_l_offset nGridOffset;
    
    double      adfGeoTransform[6];

    void        CaptureMetadataItem( char *pszItem );

    int         OpenGrid( char *pachGridHeader, vsi_l_offset nDataStart );

  public:
    		NTv2Dataset();
    	        ~NTv2Dataset();
    
    virtual CPLErr SetGeoTransform( double * padfTransform );
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef();
    virtual void   FlushCache(void);

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*				NTv2Dataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             NTv2Dataset()                          */
/************************************************************************/

NTv2Dataset::NTv2Dataset() : fpImage(NULL), nRecordLength(0), nGridOffset(0) { }

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
    char achFileHeader[11*16];
    char achGridHeader[11*16];

    VSIFSeekL( fpImage, 0, SEEK_SET );
    VSIFReadL( achFileHeader, 11, 16, fpImage );

    VSIFSeekL( fpImage, nGridOffset, SEEK_SET );
    VSIFReadL( achGridHeader, 11, 16, fpImage );

/* -------------------------------------------------------------------- */
/*      Update the grid, and file headers with any available            */
/*      metadata.  If all available metadata is recognised then mark    */
/*      things "clean" from a PAM point of view.                        */
/* -------------------------------------------------------------------- */
    char **papszMD = GetMetadata();
    int i;
    int bSomeLeftOver = FALSE;

    for( i = 0; papszMD != NULL && papszMD[i] != NULL; i++ )
    {
        char *pszKey = NULL;
        const char *pszValue = CPLParseNameValue( papszMD[i], &pszKey );
        if( pszKey == NULL )
            continue;

        if( EQUAL(pszKey,"GS_TYPE") )
        {
            memcpy( achFileHeader + 3*16+8, "        ", 8 );
            memcpy( achFileHeader + 3*16+8, pszValue, MIN(8,strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"VERSION") )
        {
            memcpy( achFileHeader + 4*16+8, "        ", 8 );
            memcpy( achFileHeader + 4*16+8, pszValue, MIN(8,strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"SYSTEM_F") )
        {
            memcpy( achFileHeader + 5*16+8, "        ", 8 );
            memcpy( achFileHeader + 5*16+8, pszValue, MIN(8,strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"SYSTEM_T") )
        {
            memcpy( achFileHeader + 6*16+8, "        ", 8 );
            memcpy( achFileHeader + 6*16+8, pszValue, MIN(8,strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"MAJOR_F") )
        {
            double dfValue = CPLAtof(pszValue);
            CPL_LSBPTR64( &dfValue );
            memcpy( achFileHeader + 7*16+8, &dfValue, 8 );
        }
        else if( EQUAL(pszKey,"MINOR_F") )
        {
            double dfValue = CPLAtof(pszValue);
            CPL_LSBPTR64( &dfValue );
            memcpy( achFileHeader + 8*16+8, &dfValue, 8 );
        }
        else if( EQUAL(pszKey,"MAJOR_T") )
        {
            double dfValue = CPLAtof(pszValue);
            CPL_LSBPTR64( &dfValue );
            memcpy( achFileHeader + 9*16+8, &dfValue, 8 );
        }
        else if( EQUAL(pszKey,"MINOR_T") )
        {
            double dfValue = CPLAtof(pszValue);
            CPL_LSBPTR64( &dfValue );
            memcpy( achFileHeader + 10*16+8, &dfValue, 8 );
        }
        else if( EQUAL(pszKey,"SUB_NAME") )
        {
            memcpy( achGridHeader + 0*16+8, "        ", 8 );
            memcpy( achGridHeader + 0*16+8, pszValue, MIN(8,strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"PARENT") )
        {
            memcpy( achGridHeader + 1*16+8, "        ", 8 );
            memcpy( achGridHeader + 1*16+8, pszValue, MIN(8,strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"CREATED") )
        {
            memcpy( achGridHeader + 2*16+8, "        ", 8 );
            memcpy( achGridHeader + 2*16+8, pszValue, MIN(8,strlen(pszValue)) );
        }
        else if( EQUAL(pszKey,"UPDATED") )
        {
            memcpy( achGridHeader + 3*16+8, "        ", 8 );
            memcpy( achGridHeader + 3*16+8, pszValue, MIN(8,strlen(pszValue)) );
        }
        else
        {
            bSomeLeftOver = TRUE;
        }
        
        CPLFree( pszKey );
    }

/* -------------------------------------------------------------------- */
/*      Load grid and file headers.                                     */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fpImage, 0, SEEK_SET );
    VSIFWriteL( achFileHeader, 11, 16, fpImage );

    VSIFSeekL( fpImage, nGridOffset, SEEK_SET );
    VSIFWriteL( achGridHeader, 11, 16, fpImage );

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
    if( EQUALN(poOpenInfo->pszFilename,"NTv2:",5) )
        return TRUE;
    
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
/*      Are we targetting a particular grid?                            */
/* -------------------------------------------------------------------- */
    CPLString osFilename;
    int iTargetGrid = -1;

    if( EQUALN(poOpenInfo->pszFilename,"NTv2:",5) )
    {
        const char *pszRest = poOpenInfo->pszFilename+5;
        
        iTargetGrid = atoi(pszRest);
        while( *pszRest != '\0' && *pszRest != ':' )
            pszRest++;
     
        if( *pszRest == ':' )
            pszRest++;
        
        osFilename = pszRest;
    }
    else
        osFilename = poOpenInfo->pszFilename;
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NTv2Dataset 	*poDS;

    poDS = new NTv2Dataset();
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
    char  achHeader[11*16];
    GInt32 nSubFileCount;
    double dfValue;
    CPLString osFValue;

    VSIFSeekL( poDS->fpImage, 0, SEEK_SET );
    VSIFReadL( achHeader, 11, 16, poDS->fpImage );

    CPL_LSBPTR32( achHeader + 2*16 + 8 );
    memcpy( &nSubFileCount, achHeader + 2*16 + 8, 4 );
    if (nSubFileCount <= 0 || nSubFileCount >= 1024)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                  "Invalid value for NUM_FILE : %d", nSubFileCount);
        delete poDS;
        return NULL;
    }

    poDS->CaptureMetadataItem( achHeader + 3*16 );
    poDS->CaptureMetadataItem( achHeader + 4*16 );
    poDS->CaptureMetadataItem( achHeader + 5*16 );
    poDS->CaptureMetadataItem( achHeader + 6*16 );

    memcpy( &dfValue, achHeader + 7*16 + 8, 8 );
    CPL_LSBPTR64( &dfValue );
    osFValue.Printf( "%.15g", dfValue );
    poDS->SetMetadataItem( "MAJOR_F", osFValue );
    
    memcpy( &dfValue, achHeader + 8*16 + 8, 8 );
    CPL_LSBPTR64( &dfValue );
    osFValue.Printf( "%.15g", dfValue );
    poDS->SetMetadataItem( "MINOR_F", osFValue );
    
    memcpy( &dfValue, achHeader + 9*16 + 8, 8 );
    CPL_LSBPTR64( &dfValue );
    osFValue.Printf( "%.15g", dfValue );
    poDS->SetMetadataItem( "MAJOR_T", osFValue );
    
    memcpy( &dfValue, achHeader + 10*16 + 8, 8 );
    CPL_LSBPTR64( &dfValue );
    osFValue.Printf( "%.15g", dfValue );
    poDS->SetMetadataItem( "MINOR_T", osFValue );

/* ==================================================================== */
/*      Loop over grids.                                                */
/* ==================================================================== */
    int iGrid;
    vsi_l_offset nGridOffset = sizeof(achHeader);

    for( iGrid = 0; iGrid < nSubFileCount; iGrid++ )
    {
        CPLString  osSubName;
        int i;
        GUInt32 nGSCount;

        VSIFSeekL( poDS->fpImage, nGridOffset, SEEK_SET );
        if (VSIFReadL( achHeader, 11, 16, poDS->fpImage ) != 16)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read header for subfile %d", iGrid);
            delete poDS;
            return NULL;
        }

        for( i = 4; i <= 9; i++ )
            CPL_LSBPTR64( achHeader + i*16 + 8 );
        
        CPL_LSBPTR32( achHeader + 10*16 + 8 );
        
        memcpy( &nGSCount, achHeader + 10*16 + 8, 4 );

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
            CPLString osKey, osValue;

            osKey.Printf( "SUBDATASET_%d_NAME", iGrid );
            osValue.Printf( "NTv2:%d:%s", iGrid, osFilename.c_str() );
            poDS->SetMetadataItem( osKey, osValue, "SUBDATASETS" );

            osKey.Printf( "SUBDATASET_%d_DESC", iGrid );
            osValue.Printf( "%s", osSubName.c_str() );
            poDS->SetMetadataItem( osKey, osValue, "SUBDATASETS" );
        }

        nGridOffset += (11+(vsi_l_offset)nGSCount) * 16;
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

    return( poDS );
}

/************************************************************************/
/*                              OpenGrid()                              */
/*                                                                      */
/*      Note that the caller will already have byte swapped needed      */
/*      portions of the header.                                         */
/************************************************************************/

int NTv2Dataset::OpenGrid( char *pachHeader, vsi_l_offset nGridOffset )

{
    this->nGridOffset = nGridOffset;

/* -------------------------------------------------------------------- */
/*      Read the grid header.                                           */
/* -------------------------------------------------------------------- */
    double s_lat, n_lat, e_long, w_long, lat_inc, long_inc;

    CaptureMetadataItem( pachHeader + 0*16 );
    CaptureMetadataItem( pachHeader + 1*16 );
    CaptureMetadataItem( pachHeader + 2*16 );
    CaptureMetadataItem( pachHeader + 3*16 );

    memcpy( &s_lat,  pachHeader + 4*16 + 8, 8 );
    memcpy( &n_lat,  pachHeader + 5*16 + 8, 8 );
    memcpy( &e_long, pachHeader + 6*16 + 8, 8 );
    memcpy( &w_long, pachHeader + 7*16 + 8, 8 );
    memcpy( &lat_inc, pachHeader + 8*16 + 8, 8 );
    memcpy( &long_inc, pachHeader + 9*16 + 8, 8 );

    e_long *= -1;
    w_long *= -1;

    nRasterXSize = (int) floor((e_long - w_long) / long_inc + 1.5);
    nRasterYSize = (int) floor((n_lat - s_lat) / lat_inc + 1.5);

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/*                                                                      */
/*      We use unusual offsets to remap from bottom to top, to top      */
/*      to bottom orientation, and also to remap east to west, to       */
/*      west to east.                                                   */
/* -------------------------------------------------------------------- */
    int iBand;
    
    for( iBand = 0; iBand < 4; iBand++ )
    {
        RawRasterBand *poBand = 
            new RawRasterBand( this, iBand+1, fpImage, 
                               nGridOffset + 4*iBand + 11*16
                               + (nRasterXSize-1) * 16
                               + (nRasterYSize-1) * 16 * nRasterXSize,
                               -16, -16 * nRasterXSize,
                               GDT_Float32, CPL_IS_LSB, TRUE, FALSE );
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
    double dfValue;
    char   achHeader[11*16];

    // read grid header
    VSIFSeekL( fpImage, nGridOffset, SEEK_SET );
    VSIFReadL( achHeader, 11, 16, fpImage );

    // S_LAT
    dfValue = 3600 * (adfGeoTransform[3] + (nRasterYSize-0.5) * adfGeoTransform[5]);
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  4*16 + 8, &dfValue, 8 );

    // N_LAT
    dfValue = 3600 * (adfGeoTransform[3] + 0.5 * adfGeoTransform[5]);
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  5*16 + 8, &dfValue, 8 );

    // E_LONG
    dfValue = -3600 * (adfGeoTransform[0] + (nRasterXSize-0.5)*adfGeoTransform[1]);
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  6*16 + 8, &dfValue, 8 );

    // W_LONG
    dfValue = -3600 * (adfGeoTransform[0] + 0.5 * adfGeoTransform[1]);
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  7*16 + 8, &dfValue, 8 );

    // LAT_INC
    dfValue = -3600 * adfGeoTransform[5];
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  8*16 + 8, &dfValue, 8 );
    
    // LONG_INC
    dfValue = 3600 * adfGeoTransform[1];
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  9*16 + 8, &dfValue, 8 );
    
    // write grid header.
    VSIFSeekL( fpImage, nGridOffset, SEEK_SET );
    VSIFWriteL( achHeader, 11, 16, fpImage );

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
                                  CPL_UNUSED int nBands,
                                  GDALDataType eType,
                                  char ** papszOptions )
{
    if( eType != GDT_Float32 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create NTv2 file with unsupported data type '%s'.",
                 GDALGetDataTypeName( eType ) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Are we extending an existing file?                              */
/* -------------------------------------------------------------------- */
    VSILFILE	*fp;
    GUInt32   nNumFile = 1;

    int bAppend = CSLFetchBoolean(papszOptions,"APPEND_SUBDATASET",FALSE);
    
/* -------------------------------------------------------------------- */
/*      Try to open or create file.                                     */
/* -------------------------------------------------------------------- */
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
    char achHeader[11*16];
    const char *pszValue;

    if( !bAppend )
    {
        memset( achHeader, 0, sizeof(achHeader) );
        
        memcpy( achHeader +  0*16, "NUM_OREC", 8 );
        achHeader[ 0*16 + 8] = 0xb;

        memcpy( achHeader +  1*16, "NUM_SREC", 8 );
        achHeader[ 1*16 + 8] = 0xb;

        memcpy( achHeader +  2*16, "NUM_FILE", 8 );
        achHeader[ 2*16 + 8] = 0x1;

        memcpy( achHeader +  3*16, "GS_TYPE         ", 16 );
        pszValue = CSLFetchNameValueDef( papszOptions, "GS_TYPE", "SECONDS");
        memcpy( achHeader +  3*16+8, pszValue, MIN(16,strlen(pszValue)) );

        memcpy( achHeader +  4*16, "VERSION         ", 16 );
        pszValue = CSLFetchNameValueDef( papszOptions, "VERSION", "" );
        memcpy( achHeader +  4*16+8, pszValue, MIN(16,strlen(pszValue)) );

        memcpy( achHeader +  5*16, "SYSTEM_F        ", 16 );
        pszValue = CSLFetchNameValueDef( papszOptions, "SYSTEM_F", "" );
        memcpy( achHeader +  5*16+8, pszValue, MIN(16,strlen(pszValue)) );

        memcpy( achHeader +  6*16, "SYSTEM_T        ", 16 );
        pszValue = CSLFetchNameValueDef( papszOptions, "SYSTEM_T", "" );
        memcpy( achHeader +  6*16+8, pszValue, MIN(16,strlen(pszValue)) );

        memcpy( achHeader +  7*16, "MAJOR_F ", 8);
        memcpy( achHeader +  8*16, "MINOR_F ", 8 );
        memcpy( achHeader +  9*16, "MAJOR_T ", 8 );
        memcpy( achHeader + 10*16, "MINOR_T ", 8 );

        VSIFWriteL( achHeader, 1, sizeof(achHeader), fp );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise update the header with an increased subfile count,    */
/*      and advanced to the last record of the file.                    */
/* -------------------------------------------------------------------- */
    else
    {
        VSIFSeekL( fp, 2*16 + 8, SEEK_SET );
        VSIFReadL( &nNumFile, 1, 4, fp );
        CPL_LSBPTR32( &nNumFile );

        nNumFile++;
        
        CPL_LSBPTR32( &nNumFile );
        VSIFSeekL( fp, 2*16 + 8, SEEK_SET );
        VSIFWriteL( &nNumFile, 1, 4, fp );

        vsi_l_offset nEnd;

        VSIFSeekL( fp, 0, SEEK_END );
        nEnd = VSIFTellL( fp );
        VSIFSeekL( fp, nEnd-16, SEEK_SET );
    }

/* -------------------------------------------------------------------- */
/*      Write the grid header.                                          */
/* -------------------------------------------------------------------- */
    memset( achHeader, 0, sizeof(achHeader) );

    memcpy( achHeader +  0*16, "SUB_NAME        ", 16 );
    pszValue = CSLFetchNameValueDef( papszOptions, "SUB_NAME", "" );
    memcpy( achHeader +  0*16+8, pszValue, MIN(16,strlen(pszValue)) );
    
    memcpy( achHeader +  1*16, "PARENT          ", 16 );
    pszValue = CSLFetchNameValueDef( papszOptions, "PARENT", "NONE" );
    memcpy( achHeader +  1*16+8, pszValue, MIN(16,strlen(pszValue)) );
    
    memcpy( achHeader +  2*16, "CREATED         ", 16 );
    pszValue = CSLFetchNameValueDef( papszOptions, "CREATED", "" );
    memcpy( achHeader +  2*16+8, pszValue, MIN(16,strlen(pszValue)) );
    
    memcpy( achHeader +  3*16, "UPDATED         ", 16 );
    pszValue = CSLFetchNameValueDef( papszOptions, "UPDATED", "" );
    memcpy( achHeader +  3*16+8, pszValue, MIN(16,strlen(pszValue)) );

    double dfValue;

    memcpy( achHeader +  4*16, "S_LAT   ", 8 );
    dfValue = 0;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  4*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  5*16, "N_LAT   ", 8 );
    dfValue = nYSize-1;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  5*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  6*16, "E_LONG  ", 8 );
    dfValue = -1*(nXSize-1);
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  6*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  7*16, "W_LONG  ", 8 );
    dfValue = 0;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  7*16 + 8, &dfValue, 8 );

    memcpy( achHeader +  8*16, "LAT_INC ", 8 );
    dfValue = 1;
    CPL_LSBPTR64( &dfValue );
    memcpy( achHeader +  8*16 + 8, &dfValue, 8 );
    
    memcpy( achHeader +  9*16, "LONG_INC", 8 );
    memcpy( achHeader +  9*16 + 8, &dfValue, 8 );
    
    memcpy( achHeader + 10*16, "GS_COUNT", 8 );
    GUInt32 nGSCount = nXSize * nYSize;
    CPL_LSBPTR32( &nGSCount );
    memcpy( achHeader + 10*16+8, &nGSCount, 4 );
    
    VSIFWriteL( achHeader, 1, sizeof(achHeader), fp );

/* -------------------------------------------------------------------- */
/*      Write zeroed grid data.                                         */
/* -------------------------------------------------------------------- */
    int i;

    memset( achHeader, 0, 16 );

    // Use -1 (0x000080bf) as the default error value.
    memset( achHeader + 10, 0x80, 1 );
    memset( achHeader + 11, 0xbf, 1 );
    memset( achHeader + 14, 0x80, 1 );
    memset( achHeader + 15, 0xbf, 1 );

    for( i = 0; i < nXSize * nYSize; i++ )
        VSIFWriteL( achHeader, 1, 16, fp );
    
/* -------------------------------------------------------------------- */
/*      Write the end record.                                           */
/* -------------------------------------------------------------------- */
    memset( achHeader, 0, 16 );
    memcpy( achHeader, "END     ", 8 );
    VSIFWriteL( achHeader, 1, 16, fp );
    VSIFCloseL( fp );

    if( nNumFile == 1 )
        return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
    else
    {
        CPLString osSubDSName;
        osSubDSName.Printf( "NTv2:%d:%s", nNumFile-1, pszFilename );
        return (GDALDataset *) GDALOpen( osSubDSName, GA_Update );
    }
}

/************************************************************************/
/*                         GDALRegister_NTv2()                          */
/************************************************************************/

void GDALRegister_NTv2()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "NTv2" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "NTv2" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NTv2 Datum Grid Shift" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gsb" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Float32" );

        poDriver->pfnOpen = NTv2Dataset::Open;
        poDriver->pfnIdentify = NTv2Dataset::Identify;
        poDriver->pfnCreate = NTv2Dataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
