/******************************************************************************
 * $Id$
 *
 * Project:  Microsoft Windows Bitmap
 * Purpose:  Read MS Windows Device Independent Bitmap (DIB) files
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
 * Revision 1.2  2002/12/04 18:37:49  dron
 * Support 32-bit, 24-bit True Color images and 8-bit pseudocolor ones.
 *
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

// Bitmap file consists of a BitmapFileHeader structure followed by
// a BitmapInfoHeader structure. An array of BMPColorEntry structures (also
// called a colour table) follows the bitmap information header structure. The
// colour table is followed by a second array of indexes into the colour table
// (the actual bitmap data).
//
// +---------------------+
// | BitmapFileHeader    |
// +---------------------+
// | BitmapInfoHeader    |
// +---------------------+
// | BMPColorEntry array |
// +---------------------+
// | Colour-index array  |
// +---------------------+

enum BMPComprMethod
{
    BMPC_RGB = 0,		// Uncompressed
    BMPC_RLE8 = 1,		// RLE for 8 bpp images
    BMPC_RLE4 = 2,		// RLE for 4 bpp images
    BMPC_BITFIELDS = 3,		// Bitmap is not compressed and the colour table
				// consists of three DWORD color masks that specify
				// the red, green, and blue components of each pixel.
				// This is valid when used with 16- and 32-bpp bitmaps.
    BMPC_JPEG,			// Indicates that the image is a JPEG image.
    BMPC_PNG			// Indicates that the image is a PNG image.
};

typedef struct
{
    GByte	iType[2];	// Signature "BM"
    GUInt32	iSize;		// Size in bytes of the bitmap file. Should always
				// be ignored while reading because of error
				// in Windows 3.0 SDK's description of this field

    GUInt16	iReserved1;	// Reserved, set as 0
    GUInt16	iReserved2;	// Reserved, set as 0
    GUInt32	iOffBits;	// Offset of the image from file start in bytes
} BitmapFileHeader;

typedef struct
{
    GUInt32	iSize;		// Size of BitmapInfoHeader structure in bytes.
				// Should be used to determine start of the
				// colour table
    GUInt32	iWidth;		// Image width
    GInt32	iHeight;	// Image height. If positive, image has bottom left
				// origin, if negative --- top left.
    GUInt16	iPlanes;	// Number of image planes (must be set to 1)
    GUInt16	iBitCount;	// Number of bits per pixel (1, 4, 8, 16, 24 or 32).
				// If 0 then the number of bits per pixel is
				// specified or is implied by the JPEG or PNG format.
    BMPComprMethod iCompression; // Compression method
    GUInt32	iSizeImage;	// Size of uncomressed image in bytes. May be 0
				// for BMPC_RGB bitmaps. If iCompression is BI_JPEG
				// or BI_PNG, iSizeImage indicates the size
				// of the JPEG or PNG image buffer. 
    GUInt32	iXPelsPerMeter;	// X resolution, pixels per meter (0 if not used)
    GUInt32	iYPelsPerMeter;	// Y resolution, pixels per meter (0 if not used)
    GUInt32	iClrUsed;	// Size of colour table. If 0, iBitCount should
				// be used to calculate this value (1<<iBitCount)
    GUInt32	iClrImportant;	// Number of important colours. If 0, all
				// colours are required
    // Fields above should be used for applications, compatible
    // with Windows NT 3.51 and earlier. Windows 98/Me, Windows 2000/XP
    // introduces additional fields:
    GUInt32	iRedMask;	// Colour mask that specifies the red component
				// of each pixel, valid only if iCompression
				// is set to BI_BITFIELDS.
    GUInt32	iGreenMask;	// The same for green component
    GUInt32	iBlueMask;	// The same for blue component
    GUInt32	iAlphaMask;	// Colour mask that specifies the alpha
				// component of each pixel.
    GUInt32	iCSType;	// Colour space of the DIB.
} BitmapInfoHeader;

// We will use playn byte array instead of this structure, but declaration
// provided for reference
typedef struct
{
    GByte	bBlue;
    GByte	bGreen;
    GByte	bRed;
    GByte	bReserved;	// Must be 0
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
    int			nColorTableSize;
    GByte		*pabyColorTable;
    GDALColorTable	*poColorTable;
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

    int		nScanSize;
    int		iBytesPerPixel;
    GByte	*iScan;
    
  public:

    		BMPRasterBand( BMPDataset *, int );
		~BMPRasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable  *GetColorTable();
};


/************************************************************************/
/*                           BMPRasterBand()                            */
/************************************************************************/

BMPRasterBand::BMPRasterBand( BMPDataset *poDS, int nBand )
{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_Byte;
    iBytesPerPixel = poDS->sInfoHeader.iBitCount / 8;

    // We will read one scanline per time. Scanlines in BMP aligned at 4-byte
    // boundary
    nBlockXSize = poDS->GetRasterXSize();
    nScanSize =
	((poDS->GetRasterXSize() * poDS->sInfoHeader.iBitCount + 31) & ~31) / 8;
    nBlockYSize = 1;
    
    CPLDebug( "BMP", "Band %d: set nBlockXSize=%d, nBlockYSize=%d, nScanSize=%d",
	      nBand, nBlockXSize, nBlockYSize, nScanSize );
    
    iScan = (GByte *)CPLMalloc( nScanSize * nBlockYSize );
}

/************************************************************************/
/*                           ~BMPRasterBand()                           */
/************************************************************************/

BMPRasterBand::~BMPRasterBand()
{
    CPLFree( iScan );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BMPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
				  void * pImage )
{
    BMPDataset	*poGDS = (BMPDataset *) poDS;
    long	iScanOffset;
    int		i, j;
    int		nBlockSize = nBlockXSize * nBlockYSize;

    if ( poGDS->sInfoHeader.iBitCount == 8  ||
	 poGDS->sInfoHeader.iBitCount == 24 ||
	 poGDS->sInfoHeader.iBitCount == 32 )
    {
	if ( poGDS->sInfoHeader.iHeight > 0 )
	{
	    iScanOffset = poGDS->sFileHeader.iSize -
		(nBlockYOff + 1) * nScanSize;
	}
	else
	{
	    iScanOffset = poGDS->sFileHeader.iOffBits +
			  nBlockYOff * nScanSize;
	}
	VSIFSeek( poGDS->fp, iScanOffset, SEEK_SET );
	VSIFRead( iScan, 1, nScanSize, poGDS->fp );
	
	for( i = 0, j = nBlockXOff; i < nBlockSize; i++ )
	{
	    // Colour triplets in BMP file organized in reverse order:
	    // blue, green, red
	    ((GByte *) pImage)[i] = iScan[j + iBytesPerPixel - nBand];
	    j += iBytesPerPixel;
	}
    }

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *BMPRasterBand::GetColorTable()
{
    BMPDataset   *poGDS = (BMPDataset *) poDS;

    return poGDS->poColorTable;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp BMPRasterBand::GetColorInterpretation()
{
    BMPDataset	    *poGDS = (BMPDataset *) poDS;

    if( poGDS->sInfoHeader.iBitCount == 24 ||
	poGDS->sInfoHeader.iBitCount == 32 ||
	poGDS->sInfoHeader.iBitCount == 16 )
    {
        if( nBand == 1 )
            return GCI_RedBand;
        else if( nBand == 2 )
            return GCI_GreenBand;
        else if( nBand == 3 )
            return GCI_BlueBand;
        else
            return GCI_Undefined;
    }
    else if( poGDS->sInfoHeader.iBitCount == 8 ||
	     poGDS->sInfoHeader.iBitCount == 4 )
    {
        return GCI_PaletteIndex;
    }
    else if( poGDS->sInfoHeader.iBitCount == 1 )
    {
	return GCI_GrayIndex;
    }
    else
        return GCI_Undefined;
}

/************************************************************************/
/*                           BMPDataset()				*/
/************************************************************************/

BMPDataset::BMPDataset()
{
    fp = NULL;
    pszProjection = CPLStrdup( "" );
    nBands = 0;
    pabyColorTable = NULL;
    poColorTable = NULL;
}

/************************************************************************/
/*                            ~BMPDataset()                             */
/************************************************************************/

BMPDataset::~BMPDataset()
{
    if ( pszProjection )
	CPLFree( pszProjection );
    if ( pabyColorTable )
	CPLFree( pabyColorTable );
    if ( poColorTable != NULL )
        delete poColorTable;
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
    VSIStatBuf	    sStat;

    poDS = new BMPDataset();

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    CPLStat(poOpenInfo->pszFilename, &sStat);
    
/* -------------------------------------------------------------------- */
/*      Read the BitmapFileHeader. We need iOffBits value only          */
/* -------------------------------------------------------------------- */
    VSIFSeek( poDS->fp, 10, SEEK_SET );
    VSIFRead( &poDS->sFileHeader.iOffBits, 1, 4, poDS->fp );
#ifdef CPL_MSB
    CPL_SWAP32PTR( &poDS->sFileHeader.iOffBits );
#endif
    poDS->sFileHeader.iSize = sStat.st_size;
    CPLDebug( "BMP", "File size %d bytes.", poDS->sFileHeader.iSize );
    CPLDebug( "BMP", "Image offset 0x%x bytes from file start.",
	      poDS->sFileHeader.iOffBits );

/* -------------------------------------------------------------------- */
/*      Read the BitmapInfoHeader.                                      */
/* -------------------------------------------------------------------- */
    VSIFSeek( poDS->fp, 14, SEEK_SET );
    VSIFRead( &poDS->sInfoHeader.iSize, 1, 4, poDS->fp );
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
    CPL_SWAP32PTR( &poDS->sInfoHeader.iSize );
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
    poDS->nRasterYSize = (poDS->sInfoHeader.iHeight > 0)?
	poDS->sInfoHeader.iHeight:-poDS->sInfoHeader.iHeight;
    switch ( poDS->sInfoHeader.iBitCount )
    {
	case 1:
	poDS->nBands = 1;
	break;
	case 4:
	case 8:
	{
	    int	    i;

	    poDS->nBands = 1;
	    // Allocate memory for colour table and read it
	    if ( poDS->sInfoHeader.iClrUsed )
		poDS->nColorTableSize = poDS->sInfoHeader.iClrUsed;
	    else
		poDS->nColorTableSize = 1 << poDS->sInfoHeader.iBitCount;
	    poDS->pabyColorTable = (GByte *)CPLMalloc( 4 * poDS->nColorTableSize );
	    VSIFSeek( poDS->fp, 14 + poDS->sInfoHeader.iSize, SEEK_SET );
	    VSIFRead( poDS->pabyColorTable, 4, poDS->nColorTableSize, poDS->fp );

	    GDALColorEntry oEntry;
	    poDS->poColorTable = new GDALColorTable();
	    for( i = 0; i < poDS->nColorTableSize; i++ )
	    {
                oEntry.c1 = poDS->pabyColorTable[i * 4 + 2];    // Red
                oEntry.c2 = poDS->pabyColorTable[i * 4 + 1];    // Green
                oEntry.c3 = poDS->pabyColorTable[i * 4];    // Blue
		oEntry.c4 = 255;
		
                poDS->poColorTable->SetColorEntry( i, &oEntry );
	    }
	}
	break;
	case 16:
	case 24:
	case 32:
	poDS->nBands = 3;
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

