/******************************************************************************
 * $Id$
 *
 * Project:  eCognition
 * Purpose:  Implementation of Erdas .LAN / .GIS format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2004/05/26 17:45:16  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_LAN(void);
CPL_C_END

/**

Erdas Header format:

Offset   Size    Type      Description
------   ----    ----      -----------
0          6     char      magic cookie / version (ie. HEAD74). 
6          2    Int16      Pixel type, 0=8bit, 1=4bit, 2=16bit
8          2    Int16      Number of Bands. 
10         6     char      Unknown.
16         4    Int32      Width
20         4    Int32      Height
24         4    Int32      X Start (offset in original file?)
28         4    Int32      Y Start (offset in original file?)
32        56     char      Unknown.
88         2    Int16      0=LAT, 1=UTM, 2=StatePlane, 3- are projections?
90         2    Int16      Classes in coverage.
92        14     char      Unknown.
106        2    Int16      Area Unit (0=none, 1=Acre, 2=Hectare, 3=Other)
108        4  Float32      Pixel area. 
112        4  Float32      Upper Left corner X (center of pixel?)
116        4  Float32      Upper Left corner Y (center of pixel?)
120        4  Float32      Width of a pixel.
124        4  Float32      Height of a pixel.

All binary fields are in the same byte order but it may be big endian or
little endian depending on what platform the file was written on.  Usually
this can be checked against the number of bands though this test won't work
if there are more than 255 bands. 

**/

#define ERD_HEADER_SIZE  128

/************************************************************************/
/* ==================================================================== */
/*				LANDataset				*/
/* ==================================================================== */
/************************************************************************/

class LANDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.
    
    char	pachHeader[ERD_HEADER_SIZE];

    char        *pszProjection;
    
    double      adfGeoTransform[6];

  public:
    		LANDataset();
    	        ~LANDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                             LANDataset()                             */
/************************************************************************/

LANDataset::LANDataset()
{
    fpImage = NULL;
}

/************************************************************************/
/*                            ~LANDataset()                             */
/************************************************************************/

LANDataset::~LANDataset()

{
    if( fpImage != NULL )
        VSIFClose( fpImage );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LANDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the header (.pcb) file.       */
/*      Does this appear to be a pcb file?                              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < ERD_HEADER_SIZE || poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUALN((const char *)poOpenInfo->pabyHeader,"HEADER",6)
        && !EQUALN((const char *)poOpenInfo->pabyHeader,"HEAD74",6) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    LANDataset 	*poDS;

    poDS = new LANDataset();

/* -------------------------------------------------------------------- */
/*      Adopt the openinfo file pointer for use with this file.         */
/* -------------------------------------------------------------------- */
    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Do we need to byte swap the headers to local machine order?     */
/* -------------------------------------------------------------------- */
    int bBigEndian = poOpenInfo->pabyHeader[8] == 0;
    int bNeedSwap;

    memcpy( poDS->pachHeader, poOpenInfo->pabyHeader, ERD_HEADER_SIZE );

#ifdef CPL_LSB
    bNeedSwap = bBigEndian;
#else
    bNeedSwap = !bBigEndian;
#endif
        
    if( bNeedSwap )
    {
        CPL_SWAP16PTR( poDS->pachHeader + 6 );
        CPL_SWAP16PTR( poDS->pachHeader + 8 );

        CPL_SWAP32PTR( poDS->pachHeader + 16 );
        CPL_SWAP32PTR( poDS->pachHeader + 20 );
        CPL_SWAP32PTR( poDS->pachHeader + 24 );
        CPL_SWAP32PTR( poDS->pachHeader + 28 );

        CPL_SWAP16PTR( poDS->pachHeader + 88 );
        CPL_SWAP16PTR( poDS->pachHeader + 90 );

        CPL_SWAP16PTR( poDS->pachHeader + 106 );
        CPL_SWAP32PTR( poDS->pachHeader + 108 );
        CPL_SWAP32PTR( poDS->pachHeader + 112 );
        CPL_SWAP32PTR( poDS->pachHeader + 116 );
        CPL_SWAP32PTR( poDS->pachHeader + 120 );
        CPL_SWAP32PTR( poDS->pachHeader + 124 );
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    int  nBandCount, nPixelOffset;
    GDALDataType eDataType;

    poDS->nRasterXSize = *((GInt32 *) (poDS->pachHeader + 16));
    poDS->nRasterYSize = *((GInt32 *) (poDS->pachHeader + 20));

    if( *((GInt16 *) (poDS->pachHeader + 6)) == 0 )
    {
        eDataType = GDT_Byte;
        nPixelOffset = 1;
    }
    else if( *((GInt16 *) (poDS->pachHeader + 6)) == 1 ) /* 4bit! */
    {
        eDataType = GDT_Byte;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Currently the GDAL .LAN/.GIS driver does not support 4 bit files." );
        delete poDS;
        return NULL;
    }
    else if( *((GInt16 *) (poDS->pachHeader + 6)) == 2 )
    {
        nPixelOffset = 2;
        eDataType = GDT_Int16;
    }
    else
    {
    }

    nBandCount = *((GInt16 *) (poDS->pachHeader + 8));

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= nBandCount; iBand++ )
        poDS->SetBand( iBand, 
                       new RawRasterBand( poDS, iBand, poDS->fpImage, 
                                          ERD_HEADER_SIZE + (iBand-1) 
                                          * nPixelOffset * poDS->nRasterXSize,
                                          nPixelOffset, 
                                          poDS->nRasterXSize * nPixelOffset,
                                          eDataType, FALSE ));

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Try to interprete georeferencing.                               */
/* -------------------------------------------------------------------- */
    poDS->adfGeoTransform[0] = *((float *) (poDS->pachHeader + 112));
    poDS->adfGeoTransform[1] = *((float *) (poDS->pachHeader + 120));
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = *((float *) (poDS->pachHeader + 116));
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = - *((float *) (poDS->pachHeader + 124));

    // adjust for center of pixel vs. top left corner of pixel.
    poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
    poDS->adfGeoTransform[3] -= poDS->adfGeoTransform[5] * 0.5;

/* -------------------------------------------------------------------- */
/*      Look for a world file.                                          */
/* -------------------------------------------------------------------- */
#ifdef notdef
    poDS->bGeoTransformValid = 
        GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                           poDS->adfGeoTransform );
    if( !poDS->bGeoTransformValid )
	GDALReadWorldFile( poOpenInfo->pszFilename, ".lnw", 
                           poDS->adfGeoTransform );
#endif

    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr LANDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
    return CE_None;
}

/************************************************************************/
/*                          GDALRegister_LAN()                          */
/************************************************************************/

void GDALRegister_LAN()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "LAN" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "LAN" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Erdas .LAN/.GIS" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#LAN" );
        
        poDriver->pfnOpen = LANDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

