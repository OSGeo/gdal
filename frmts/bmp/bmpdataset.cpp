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
 * Revision 1.3  2002/12/05 19:25:35  dron
 * Preliminary CreateCopy() function.
 *
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
    GByte	bType[2];	// Signature "BM"
    GUInt32	iSize;		// Size in bytes of the bitmap file. Should always
				// be ignored while reading because of error
				// in Windows 3.0 SDK's description of this field
    GUInt16	iReserved1;	// Reserved, set as 0
    GUInt16	iReserved2;	// Reserved, set as 0
    GUInt32	iOffBits;	// Offset of the image from file start in bytes
} BitmapFileHeader;
const int	BFH_SIZE = 14;

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

// We will use plain byte array instead of this structure, but declaration
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
    GByte	*pabyScan;
    
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
    
    pabyScan = (GByte *) CPLMalloc( nScanSize * nBlockYSize );
}

/************************************************************************/
/*                           ~BMPRasterBand()                           */
/************************************************************************/

BMPRasterBand::~BMPRasterBand()
{
    CPLFree( pabyScan );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BMPRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
				  void * pImage )
{
    BMPDataset	*poGDS = (BMPDataset *) poDS;
    long	pabyScanOffset;
    int		i, j;
    int		nBlockSize = nBlockXSize * nBlockYSize;

    if ( poGDS->sInfoHeader.iBitCount == 8  ||
	 poGDS->sInfoHeader.iBitCount == 24 ||
	 poGDS->sInfoHeader.iBitCount == 32 )
    {
	if ( poGDS->sInfoHeader.iHeight > 0 )
	{
	    pabyScanOffset = poGDS->sFileHeader.iSize -
		(nBlockYOff + 1) * nScanSize;
	}
	else
	{
	    pabyScanOffset = poGDS->sFileHeader.iOffBits +
			  nBlockYOff * nScanSize;
	}
	VSIFSeek( poGDS->fp, pabyScanOffset, SEEK_SET );
	VSIFRead( pabyScan, 1, nScanSize, poGDS->fp );
	
	for( i = 0, j = nBlockXOff; i < nBlockSize; i++ )
	{
	    // Colour triplets in BMP file organized in reverse order:
	    // blue, green, red
	    ((GByte *) pImage)[i] = pabyScan[j + iBytesPerPixel - nBand];
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
    VSIFSeek( poDS->fp, BFH_SIZE, SEEK_SET );
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
	      " file size: %d\n width: %d\n height: %d\n planes: %d\n bpp: %d\n"
	      " compression: %d\n image size: %d\n X resolution: %d\n"
	      " Y resolution: %d\n colours used: %d\n colours important: %d",
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
	    VSIFSeek( poDS->fp, BFH_SIZE + poDS->sInfoHeader.iSize, SEEK_SET );
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
/*                      BMPImageCreateCopy()                            */
/************************************************************************/

static GDALDataset *
BMPImageCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                    int bStrict, char ** papszOptions, 
                    GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "BMP driver doesn't support %d bands. Must be 1 or 3.\n",
                  nBands );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    FILE        *fpImage;

    fpImage = VSIFOpen( pszFilename, "wt" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create file %s.\n", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create BitmapInfoHeader                                         */
/* -------------------------------------------------------------------- */
    BitmapInfoHeader	sInfoHeader;
    int			nScanSize;

    sInfoHeader.iSize = 40;
    sInfoHeader.iWidth = nXSize;
    sInfoHeader.iHeight = nYSize;
    sInfoHeader.iPlanes = 1;
    sInfoHeader.iBitCount = ( nBands == 3 )?24:8;
    sInfoHeader.iCompression = BMPC_RGB;
    nScanSize = ((sInfoHeader.iWidth * sInfoHeader.iBitCount + 31) & ~31) / 8;
    sInfoHeader.iSizeImage = nScanSize * sInfoHeader.iHeight;
    sInfoHeader.iXPelsPerMeter = 0;
    sInfoHeader.iYPelsPerMeter = 0;
    if (sInfoHeader.iBitCount < 24 ) 
        sInfoHeader.iClrUsed = ( 1 << sInfoHeader.iBitCount );
    else
	sInfoHeader.iClrUsed = 0;
    sInfoHeader.iClrImportant = 0;

/* -------------------------------------------------------------------- */
/*      Create BitmapFileHeader                                         */
/* -------------------------------------------------------------------- */
    BitmapFileHeader	sFileHeader;

    sFileHeader.bType[0] = 'B';
    sFileHeader.bType[1] = 'M';
    sFileHeader.iSize = BFH_SIZE + sInfoHeader.iSize +
	sInfoHeader.iClrUsed * 4 + sInfoHeader.iSizeImage;
    sFileHeader.iReserved1 = 0;
    sFileHeader.iReserved2 = 0;
    sFileHeader.iOffBits = BFH_SIZE + sInfoHeader.iSize +
	sInfoHeader.iClrUsed * 4;

/* -------------------------------------------------------------------- */
/*      Write all structures to the file                                */
/* -------------------------------------------------------------------- */
    // FIXME: swap in case of MSB
    VSIFWrite( &sFileHeader.bType, 1, 2, fpImage );
    VSIFWrite( &sFileHeader.iSize, 4, 1, fpImage );
    VSIFWrite( &sFileHeader.iReserved1, 2, 1, fpImage );
    VSIFWrite( &sFileHeader.iReserved2, 2, 1, fpImage );
    VSIFWrite( &sFileHeader.iOffBits, 4, 1, fpImage );

    VSIFWrite( &sInfoHeader.iSize, 4, 1, fpImage );
    VSIFWrite( &sInfoHeader.iWidth, 4, 1, fpImage );
    VSIFWrite( &sInfoHeader.iHeight, 4, 1, fpImage );
    VSIFWrite( &sInfoHeader.iPlanes, 2, 1, fpImage );
    VSIFWrite( &sInfoHeader.iBitCount, 2, 1, fpImage );
    VSIFWrite( &sInfoHeader.iCompression, 4, 1, fpImage );
    VSIFWrite( &sInfoHeader.iSizeImage, 4, 1, fpImage );
    VSIFWrite( &sInfoHeader.iXPelsPerMeter, 4, 1, fpImage );
    VSIFWrite( &sInfoHeader.iYPelsPerMeter, 4, 1, fpImage );
    VSIFWrite( &sInfoHeader.iClrUsed, 4, 1, fpImage );
    VSIFWrite( &sInfoHeader.iClrImportant, 4, 1, fpImage );
    
    if ( nBands == 1 )
    {
	// colour table...
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GByte	    *pabyOutput, *pabyInput;
    int		    iBand, iLine, iInPixel, iOutPixel;
    CPLErr	    eErr = CE_None;
    GDALRasterBand  *poBand;


    pabyInput = (GByte *) CPLMalloc( nXSize );
    pabyOutput = (GByte *) CPLMalloc( nScanSize );
    memset( pabyOutput, 0, nScanSize );
    
    for( iLine = nYSize - 1; eErr == CE_None && iLine >= 0; iLine-- )
    {
	for ( iBand = nBands; iBand > 0; iBand-- )
	{
	    poBand = poSrcDS->GetRasterBand( iBand );
	    eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1,
				     pabyInput, nXSize, 1, GDT_Byte,
				     sizeof(GByte), sizeof(GByte) * nXSize );
	    for ( iInPixel = 0, iOutPixel = iBand - 1;
		  iInPixel < nXSize; iInPixel++, iOutPixel+=nBands )
	    {
		pabyOutput[iOutPixel] = pabyInput[iInPixel];
	    }
	}
	if ( VSIFWrite( pabyOutput, 1, nScanSize, fpImage ) <= 0 )
	{
	    eErr = CE_Failure;
	    CPLError( CE_Failure, CPLE_FileIO,
		      "Can't write line %d", nYSize - iLine - 1 );
	}
	if( eErr == CE_None &&
	    !pfnProgress((nYSize - iLine) /
			 ((double) nYSize), NULL, pProgressData) )
	{
	    eErr = CE_Failure;
	    CPLError( CE_Failure, CPLE_UserInterrupt, 
		      "User terminated CreateCopy()" );
	}
    }

    CPLFree( pabyOutput );
    CPLFree( pabyInput );
    VSIFClose( fpImage );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
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
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );

        poDriver->pfnOpen = BMPDataset::Open;
        poDriver->pfnCreateCopy = BMPImageCreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

