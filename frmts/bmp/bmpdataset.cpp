/******************************************************************************
 * $Id$
 *
 * Project:  Microsoft Windows Bitmap
 * Purpose:  Read MS Windows Device Independent Bitmap (dib) files
 * Author:   Andrey Kiselev, dron@at1895.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@at1895.spb.edu>
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
 * Revision 1.1  2002/12/03 19:04:18  dron
 * Initial version.
 *
 *
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_BMP(void);
CPL_C_END

enum BMPComprMethod
{
    BMPC_RGB,			// Without compression
    BMPC_RLE4,			// RLE for 4 bpp images
    BMPC_RLE8,			// RLE for 8 bpp images
    BMPC_BITFIELDS		// ???
};

typedef struct
{
    GUInt16	iType;		// Signature "BM"
    GUInt32	iSize;		// iSize should always be ignored while
				// reading because of error in Windows
				// 3.0 SDK's description of this field

    GUInt16	iReserved1;	// Reserved, set as 0
    GUInt16	iReserved2;	// Reserved, set as 0
    GUInt32	iOffBits;	// Offset of the image from file start in bytes
} BitmapFileHeader;

typedef struct
{
    GUInt32	iSize;		// Size of BitmapInfoHeader structure in bytes
    GUInt32	iWidth;		// Image width
    GUInt32	iHeight;	// Image height
    GUInt16	iPlanes;	// Number of image planes (set as 1)
    GUInt16	iBitCount;	// Number of bits per pixel (1, 4, 8, 16, 24 or 32)
    BMPComprMethod iCompression; // Compression method
    GUInt32	iSizeImage;	// Size of uncomressed image in bytes (or 0 sometimes)
    GUInt32	iXPelsPerMeter;	// X resolution, pixels per meter (0 if not used)
    GUInt32	iYPelsPerMeter;	// Y resolution, pixels per meter (0 if not used)
    GUInt32	iClrUsed;	// Size of colour table. If 0, iBitCount
				// should be used to calculate this value (2^iBitCount)
    GUInt32	iClrImportant;	// Number of important colours. If 0, all
				// colours are important
} BitmapInfoHeader;

typedef struct
{
    GByte	bBlue;
    GByte	bGreen;
    GByte	bRed;
    GByte	bReserved;
} BMPColorEntry;

/************************************************************************/
/* ==================================================================== */
/*				BMPDataset				*/
/* ==================================================================== */
/************************************************************************/

class BMPDataset : public GDALDataset
{
    friend class BMPRasterBand;

    BitmapFileHeader	sFileHeader;
    BitmapInfoHeader	sInfoHeader;
    FILE		*fp;
    char		*pszProjection;

  public:
                BMPDataset();
		~BMPDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

//    CPLErr 	GetGeoTransform( double * padfTransform );
//    const char *GetProjectionRef();

};

/************************************************************************/
/* ==================================================================== */
/*                            BMPRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BMPRasterBand : public GDALRasterBand
{
    friend class BMPDataset;

  public:

    		BMPRasterBand( BMPDataset *, int );
    
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           BMPRasterBand()                            */
/************************************************************************/

BMPRasterBand::BMPRasterBand( BMPDataset *poDS, int nBand )
{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_Byte;
    
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BMPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
				  void * pImage )
{
    BMPDataset *poGDS = (BMPDataset *) poDS;
    
    CPLDebug( "BMP", "nBlockXOff=%d, nBlockYOff=%d", nBlockXOff, nBlockYOff );
    VSIFSeek( poGDS->fp,
	      poGDS->sFileHeader.iOffBits +
	      nBlockYOff * nBlockXSize * poGDS->nBands +
	      nBlockXOff * nBlockYSize,
	      SEEK_SET );
    VSIFRead( pImage, 1, nBlockXSize * nBlockYSize, poGDS->fp );
    
    return CE_None;
}

/************************************************************************/
/*                           BMPDataset()				*/
/************************************************************************/

BMPDataset::BMPDataset()
{
    fp = NULL;
    pszProjection = CPLStrdup( "" );
    nBands = 0;
}

/************************************************************************/
/*                            ~BMPDataset()                             */
/************************************************************************/

BMPDataset::~BMPDataset()
{
    if ( pszProjection )
	CPLFree( pszProjection );
    if( fp != NULL )
        VSIFClose( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

/*CPLErr L1BDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}*/

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

/*const char *L1BDataset::GetProjectionRef()
{
    if( bProjDetermined )
        return pszProjection;
    else
        return "";
}*/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BMPDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->fp == NULL )
        return NULL;

    if(	!EQUALN((const char *) poOpenInfo->pabyHeader, "BM", 2) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    BMPDataset	    *poDS;

    poDS = new BMPDataset();

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the BitmapFileHeader. We need iOffBits value only          */
/* -------------------------------------------------------------------- */
    VSIFSeek( poDS->fp, 10, SEEK_SET );
    VSIFRead( &poDS->sFileHeader.iOffBits, 1, 4, poDS->fp );
#ifdef CPL_MSB
    CPL_SWAP32PTR( &poDS->sFileHeader.iOffBits );
#endif
    CPLDebug( "BMP", "Image offset 0x%x bytes from file start.",
	      poDS->sFileHeader.iOffBits );

/* -------------------------------------------------------------------- */
/*      Read the BitmapInfoHeader.                                      */
/* -------------------------------------------------------------------- */
    VSIFSeek( poDS->fp, 18, SEEK_SET );
    VSIFRead( &poDS->sInfoHeader.iWidth, 1, 4, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iHeight, 1, 4, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iPlanes, 1, 2, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iBitCount, 1, 2, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iCompression, 1, 4, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iSizeImage, 1, 4, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iXPelsPerMeter, 1, 4, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iYPelsPerMeter, 1, 4, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iClrUsed, 1, 4, poDS->fp );
    VSIFRead( &poDS->sInfoHeader.iClrImportant, 1, 4, poDS->fp );

#ifdef CPL_MSB
    CPL_SWAP32PTR( &poDS->sInfoHeader.iWidth );
    CPL_SWAP32PTR( &poDS->sInfoHeader.iHeight );
    CPL_SWAP16PTR( &poDS->sInfoHeader.iPlanes );
    CPL_SWAP16PTR( &poDS->sInfoHeader.iBitCount );
    CPL_SWAP32PTR( &poDS->sInfoHeader.iCompression );
    CPL_SWAP32PTR( &poDS->sInfoHeader.iSizeImage );
    CPL_SWAP32PTR( &poDS->sInfoHeader.iXPelsPerMeter );
    CPL_SWAP32PTR( &poDS->sInfoHeader.iYPelsPerMeter );
    CPL_SWAP32PTR( &poDS->sInfoHeader.iClrUsed );
    CPL_SWAP32PTR( &poDS->sInfoHeader.iClrImportant );
#endif
    
    CPLDebug( "BMP", "Windows Device Independent Bitmap parameters:\n"
	      " width: %d\n height: %d\n planes: %d\n bpp: %d\n compression: %d\n"
	      " image size: %d\n X resolution: %d\n Y resolution: %d\n"
	      " colours used: %d\n colours important: %d",
	      poDS->sInfoHeader.iWidth, poDS->sInfoHeader.iHeight,
	      poDS->sInfoHeader.iPlanes, poDS->sInfoHeader.iBitCount,
	      poDS->sInfoHeader.iCompression, poDS->sInfoHeader.iSizeImage,
	      poDS->sInfoHeader.iXPelsPerMeter, poDS->sInfoHeader.iYPelsPerMeter,
	      poDS->sInfoHeader.iClrUsed, poDS->sInfoHeader.iClrImportant );
    
    poDS->nRasterXSize = poDS->sInfoHeader.iWidth;
    poDS->nRasterYSize = poDS->sInfoHeader.iHeight;
    switch ( poDS->sInfoHeader.iBitCount )
    {
	case 1:
	case 4:
	case 8:
	poDS->nBands = 1;
	break;
	//case 16: FIXME
	case 24:
	poDS->nBands = 3;
	break;
	case 32:
	poDS->nBands = 4;
	break;
	default:
	delete poDS;
	return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		    iBand;
    
    for( iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new BMPRasterBand( poDS, iBand ) );
    }

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_BMP()				*/
/************************************************************************/

void GDALRegister_BMP()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "BMP" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "BMP" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "MS Windows Device Independent Bitmap" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_bmp.html" );

        poDriver->pfnOpen = BMPDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

