/******************************************************************************
 * $Id$
 *
 * Project:  Microsoft Windows Bitmap
 * Purpose:  Read MS Windows Device Independent Bitmap (DIB) files
 *           and OS/2 Presentation Manager bitmaps v. 1.x and v. 2.x
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.16  2003/02/15 14:23:09  dron
 * Fix in GetGeoTransform().
 *
 * Revision 1.15  2003/02/03 18:40:15  dron
 * Type of input data checked in Create() method now.
 *
 * Revision 1.14  2002/12/15 15:24:41  dron
 * Typos fixed.
 *
 * Revision 1.13  2002/12/15 15:13:33  dron
 * BMP structures extended.
 *
 * Revision 1.12  2002/12/13 14:15:50  dron
 * IWriteBlock() fixed, CreateCopy() removed, added RLE4 decoding and OS/2 BMP
 * support.
 *
 * Revision 1.11  2002/12/11 22:09:13  warmerda
 * Re-enable createcopy.
 *
 * Revision 1.10  2002/12/11 22:08:20  warmerda
 * fixed multiband support for writing ... open wit wb+ in create
 *
 * Revision 1.9  2002/12/11 16:13:04  dron
 * Support for 16-bit RGB images (untested).
 *
 * Revision 1.8  2002/12/10 22:12:59  dron
 * RLE8 decoding added.
 *
 * Revision 1.7  2002/12/09 19:31:44  dron
 * Switched to CPL_LSBWORD32 macro.
 *
 * Revision 1.6  2002/12/07 15:20:23  dron
 * SetColorTable() added. Create() really works now.
 *
 * Revision 1.5  2002/12/06 20:50:13  warmerda
 * fixed type warning
 *
 * Revision 1.4  2002/12/06 18:37:05  dron
 * Create() method added, 1- and 4-bpp images readed now.
 *
 * Revision 1.3  2002/12/05 19:25:35  dron
 * Preliminary CreateCopy() function.
 *
 * Revision 1.2  2002/12/04 18:37:49  dron
 * Support 32-bit, 24-bit True Color images and 8-bit pseudocolor ones.
 *
 * Revision 1.1  2002/12/03 19:04:18  dron
 * Initial version.
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_BMP(void);
CPL_C_END

enum BMPType
{
    BMPT_WIN4,	    // BMP used in Windows 3.0/NT 3.51/95
    BMPT_WIN5,	    // BMP used in Windows NT 4.0/98/Me/2000/XP
    BMPT_OS21,	    // BMP used in OS/2 PM 1.x
    BMPT_OS22	    // BMP used in OS/2 PM 2.x
};

// Bitmap file consists of a BitmapFileHeader structure followed by
// a BitmapInfoHeader structure. An array of BMPColorEntry structures (also
// called a colour table) follows the bitmap information header structure. The
// colour table is followed by a second array of indexes into the colour table
// (the actual bitmap data). Data may be comressed, for 4-bpp and 8-bpp used
// RLE compression.
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
//
// All numbers stored in Intel order with least significant byte first.

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

enum BMPLCSType			// Type of logical color space.
{
    BMPLT_CALIBRATED_RGB = 0,	// This value indicates that endpoints and gamma
				// values are given in the appropriate fields.
    BMPLT_DEVICE_RGB = 1,
    BMPLT_DEVICE_CMYK = 2,
};

typedef struct
{
    GInt32	iCIEX;
    GInt32	iCIEY;
    GInt32	iCIEZ;
} BMPCIEXYZ;

typedef struct			// This structure contains the x, y, and z
{				// coordinates of the three colors that correspond
    BMPCIEXYZ	iCIERed;	// to the red, green, and blue endpoints for
    BMPCIEXYZ	iCIEGreen;	// a specified logical color space.
    BMPCIEXYZ	iCIEBlue;
} BMPCIEXYZTriple;

typedef struct
{
    GByte	bType[2];	// Signature "BM"
    GInt32	iSize;		// Size in bytes of the bitmap file. Should always
				// be ignored while reading because of error
				// in Windows 3.0 SDK's description of this field
    GInt16	iReserved1;	// Reserved, set as 0
    GInt16	iReserved2;	// Reserved, set as 0
    GInt32	iOffBits;	// Offset of the image from file start in bytes
} BitmapFileHeader;
const int	BFH_SIZE = 14;

typedef struct
{
    GInt32	iSize;		// Size of BitmapInfoHeader structure in bytes.
				// Should be used to determine start of the
				// colour table
    GInt32	iWidth;		// Image width
    GInt32	iHeight;	// Image height. If positive, image has bottom left
				// origin, if negative --- top left.
    GInt16	iPlanes;	// Number of image planes (must be set to 1)
    GInt16	iBitCount;	// Number of bits per pixel (1, 4, 8, 16, 24 or 32).
				// If 0 then the number of bits per pixel is
				// specified or is implied by the JPEG or PNG format.
    BMPComprMethod iCompression; // Compression method
    GInt32	iSizeImage;	// Size of uncomressed image in bytes. May be 0
				// for BMPC_RGB bitmaps. If iCompression is BI_JPEG
				// or BI_PNG, iSizeImage indicates the size
				// of the JPEG or PNG image buffer. 
    GInt32	iXPelsPerMeter;	// X resolution, pixels per meter (0 if not used)
    GInt32	iYPelsPerMeter;	// Y resolution, pixels per meter (0 if not used)
    GInt32	iClrUsed;	// Size of colour table. If 0, iBitCount should
				// be used to calculate this value (1<<iBitCount)
    GInt32	iClrImportant;	// Number of important colours. If 0, all
				// colours are required

    // Fields above should be used for bitmaps, compatible with Windows NT 3.51
    // and earlier. Windows 98/Me, Windows 2000/XP introduces additional fields:

    GInt32	iRedMask;	// Colour mask that specifies the red component
				// of each pixel, valid only if iCompression
				// is set to BI_BITFIELDS.
    GInt32	iGreenMask;	// The same for green component
    GInt32	iBlueMask;	// The same for blue component
    GInt32	iAlphaMask;	// Colour mask that specifies the alpha
				// component of each pixel.
    BMPLCSType	iCSType;	// Colour space of the DIB.
    BMPCIEXYZTriple sEndpoints; // This member is ignored unless the iCSType member
				// specifies BMPLT_CALIBRATED_RGB.
    GInt32	iGammaRed;	// Toned response curve for red. This member
				// is ignored unless color values are calibrated
				// RGB values and iCSType is set to
				// BMPLT_CALIBRATED_RGB. Specified in 16^16 format.
    GInt32	iGammaGreen;	// Toned response curve for green.
    GInt32	iGammaBlue;	// Toned response curve for blue.
} BitmapInfoHeader;
const int	BIH_WIN4SIZE = 40; // For BMPT_WIN4
const int	BIH_WIN5SIZE = 57; // For BMPT_WIN5
const int	BIH_OS21SIZE = 12; // For BMPT_OS21
const int	BIH_OS22SIZE = 64; // For BMPT_OS22

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
    friend class BMPComprRasterBand;

    BitmapFileHeader	sFileHeader;
    BitmapInfoHeader	sInfoHeader;
    int			nColorTableSize, nColorElems;
    GByte		*pabyColorTable;
    GDALColorTable	*poColorTable;
    double		adfGeoTransform[6];
    int			bGeoTransformValid;
    char		*pszProjection;

    const char		*pszFilename;
    FILE		*fp;

  public:
                BMPDataset();
		~BMPDataset();
    
    static GDALDataset	*Open( GDALOpenInfo * );
    static GDALDataset	*Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    virtual void	FlushCache( void );

    CPLErr		GetGeoTransform( double * padfTransform );
    virtual CPLErr	SetGeoTransform( double * );
    const char		*GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            BMPRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BMPRasterBand : public GDALRasterBand
{
    friend class BMPDataset;

  protected:

    unsigned int    nScanSize;
    unsigned int    iBytesPerPixel;
    GByte	    *pabyScan;
    
  public:

    		BMPRasterBand( BMPDataset *, int );
		~BMPRasterBand();
    
    virtual CPLErr	    IReadBlock( int, int, void * );
    virtual CPLErr	    IWriteBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable  *GetColorTable();
    CPLErr		    SetColorTable( GDALColorTable * );
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
    long	iScanOffset;
    int		i, j;
    int		nBlockSize = nBlockXSize * nBlockYSize;

    if( poGDS->eAccess == GA_Update )
    {
        memset( pImage, 0, nBlockSize );
        return CE_None;
    }

    if ( poGDS->sInfoHeader.iHeight > 0 )
	iScanOffset = poGDS->sFileHeader.iSize - (nBlockYOff + 1) * nScanSize;
    else
	iScanOffset = poGDS->sFileHeader.iOffBits + nBlockYOff * nScanSize;

    if ( VSIFSeek( poGDS->fp, iScanOffset, SEEK_SET ) < 0 )
    {
	CPLError( CE_Failure, CPLE_FileIO,
		  "Can't seek to offset %d in input file", iScanOffset);
	return CE_Failure;
    }
    if ( VSIFRead( pabyScan, 1, nScanSize, poGDS->fp ) < nScanSize )
    {
	CPLError( CE_Failure, CPLE_FileIO,
		  "Can't read from offset %d in input file", iScanOffset);
	return CE_Failure;
    }
    
    if ( poGDS->sInfoHeader.iBitCount == 8  ||
	 poGDS->sInfoHeader.iBitCount == 24 ||
	 poGDS->sInfoHeader.iBitCount == 32 )
    {
	for ( i = 0, j = 0; i < nBlockSize; i++ )
	{
	    // Colour triplets in BMP file organized in reverse order:
	    // blue, green, red
	    ((GByte *) pImage)[i] = pabyScan[j + iBytesPerPixel - nBand];
	    j += iBytesPerPixel;
	}
    }
    if ( poGDS->sInfoHeader.iBitCount == 16 )
    {
	for ( i = 0, j = 0; i < nBlockSize; i++ )
	{
	    switch ( nBand )
	    {
		case 1:
		((GByte *) pImage)[i] = pabyScan[i + 1] & 0x1F;
		break;
		case 2:
		((GByte *) pImage)[i] = ((pabyScan[i] & 0x03) << 3) |
					((pabyScan[i + 1] & 0xE0) >> 5);
		break;
		case 3:
		((GByte *) pImage)[i] = (pabyScan[i] & 0x7c) >> 2;
		break;
		default:
		break;
	    }
	}
    }
    else if ( poGDS->sInfoHeader.iBitCount == 4 )
    {
	for ( i = 0, j = 0; i < nBlockSize; i++ )
	{
	    // Most significant part of the byte represents leftmost pixel
	    if ( i & 0x01 )
		((GByte *) pImage)[i] = pabyScan[j++] & 0x0F;
	    else
		((GByte *) pImage)[i] = (pabyScan[j] & 0xF0) >> 4;
	}
    }
    else if ( poGDS->sInfoHeader.iBitCount == 1 )
    {
	for ( i = 0, j = 0; i < nBlockSize; i++ )
	{
	    switch ( i % 8 )
	    {
		case 0:
		((GByte *) pImage)[i] = (pabyScan[j] & 0x80) >> 7;
		break;
		case 1:
		((GByte *) pImage)[i] = (pabyScan[j] & 0x40) >> 6;
		break;
		case 2:
		((GByte *) pImage)[i] = (pabyScan[j] & 0x20) >> 5;
		break;
		case 3:
		((GByte *) pImage)[i] = (pabyScan[j] & 0x10) >> 4;
		break;
		case 4:
		((GByte *) pImage)[i] = (pabyScan[j] & 0x08) >> 3;
		break;
		case 5:
		((GByte *) pImage)[i] = (pabyScan[j] & 0x04) >> 2;
		break;
		case 6:
		((GByte *) pImage)[i] = (pabyScan[j] & 0x02) >> 1;
		break;
		case 7:
		((GByte *) pImage)[i] = pabyScan[j++] & 0x01;
		break;
		default:
		break;
	    }
	}
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr BMPRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
				   void * pImage )
{
    BMPDataset	*poGDS = (BMPDataset *)poDS;
    int		iInPixel, iOutPixel;
    long	iScanOffset;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    iScanOffset = poGDS->sFileHeader.iSize - (nBlockYOff + 1) * nScanSize;
    if ( VSIFSeek( poGDS->fp, iScanOffset, SEEK_SET ) < 0 )
    {
	CPLError( CE_Failure, CPLE_FileIO,
		  "Can't seek to offset %d in output file", iScanOffset );
	return CE_Failure;
    }

    if( poGDS->nBands != 1 )
    {
        memset( pabyScan, 0, nScanSize );
        VSIFRead( pabyScan, 1, nScanSize, poGDS->fp );
        VSIFSeek( poGDS->fp, iScanOffset, SEEK_SET );
    }
    
    for ( iInPixel = 0, iOutPixel = iBytesPerPixel - nBand;
	  iInPixel < nBlockXSize; iInPixel++, iOutPixel += poGDS->nBands )
    {
	pabyScan[iOutPixel] = ((GByte *) pImage)[iInPixel];
    }
    
    if ( VSIFWrite( pabyScan, 1, nScanSize, poGDS->fp ) < nScanSize )
    {
	CPLError( CE_Failure, CPLE_FileIO,
		  "Can't write block with X offset %d and Y offset %d",
		  nBlockXOff, nBlockYOff );
	return CE_Failure;
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
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr BMPRasterBand::SetColorTable( GDALColorTable *poColorTable )
{
    BMPDataset	*poGDS = (BMPDataset *) poDS;
    
    if ( poColorTable )
    {
	GDALColorEntry	oEntry;
	GInt32		iLong;
	int		i;

	poGDS->sInfoHeader.iClrUsed = poColorTable->GetColorEntryCount();
	if ( poGDS->sInfoHeader.iClrUsed < 1 ||
	     poGDS->sInfoHeader.iClrUsed > (1 << poGDS->sInfoHeader.iBitCount) )
	    return CE_Failure;
	
	VSIFSeek( poGDS->fp, BFH_SIZE + 32, SEEK_SET );
	
	iLong = CPL_LSBWORD32( poGDS->sInfoHeader.iClrUsed );
	VSIFWrite( &iLong, 4, 1, poGDS->fp );
	poGDS->pabyColorTable = (GByte *) CPLRealloc( poGDS->pabyColorTable,
			poGDS->nColorElems * poGDS->sInfoHeader.iClrUsed );
	if ( !poGDS->pabyColorTable )
	    return CE_Failure;
	
	for( i = 0; i < poGDS->sInfoHeader.iClrUsed; i++ )
	{
	    poColorTable->GetColorEntryAsRGB( i, &oEntry );
	    poGDS->pabyColorTable[i * poGDS->nColorElems + 3] = 0;
	    poGDS->pabyColorTable[i * poGDS->nColorElems + 2] = oEntry.c1; // Red
	    poGDS->pabyColorTable[i * poGDS->nColorElems + 1] = oEntry.c2; // Green
	    poGDS->pabyColorTable[i * poGDS->nColorElems] = oEntry.c3;	   // Blue
	}
	
	VSIFSeek( poGDS->fp, BFH_SIZE + poGDS->sInfoHeader.iSize, SEEK_SET );
	if ( VSIFWrite( poGDS->pabyColorTable, 1, 
		poGDS->nColorElems * poGDS->sInfoHeader.iClrUsed, poGDS->fp ) <
	     poGDS->nColorElems * (GUInt32) poGDS->sInfoHeader.iClrUsed )
	{
	    return CE_Failure;
	}
    }
    else
	return CE_Failure;

    return CE_None;
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
/* ==================================================================== */
/*                       BMPComprRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BMPComprRasterBand : public BMPRasterBand
{
    friend class BMPDataset;

    GByte	    *pabyComprBuf;
    GByte	    *pabyUncomprBuf;
    
  public:

    		BMPComprRasterBand( BMPDataset *, int );
		~BMPComprRasterBand();
    
    virtual CPLErr	    IReadBlock( int, int, void * );
//    virtual CPLErr	    IWriteBlock( int, int, void * );
};

/************************************************************************/
/*                           BMPComprRasterBand()                       */
/************************************************************************/

BMPComprRasterBand::BMPComprRasterBand( BMPDataset *poDS, int nBand )
    : BMPRasterBand( poDS, nBand )
{
    int	    i, j, k, iLength;
    long    iComprSize, iUncomprSize;
   
    iComprSize = poDS->sFileHeader.iSize - poDS->sFileHeader.iOffBits;
    iUncomprSize = poDS->GetRasterXSize() * poDS->GetRasterYSize();
    pabyComprBuf = (GByte *) CPLMalloc( iComprSize );
    pabyUncomprBuf = (GByte *) CPLMalloc( iUncomprSize );

    CPLDebug( "BMP", "RLE8 compression detected." );
    CPLDebug ( "BMP", "Size of compressed buffer %ld bytes,"
	       " size of uncompressed buffer %ld bytes.",
	       iComprSize, iUncomprSize );
 
    VSIFSeek( poDS->fp, poDS->sFileHeader.iOffBits, SEEK_SET );
    VSIFRead( pabyComprBuf, 1, iComprSize, poDS->fp );
    i = 0;
    j = 0;
    if ( poDS->sInfoHeader.iBitCount == 8 )	    // RLE8
    {
	while( j < iUncomprSize && i < iComprSize )
	{
	    if ( pabyComprBuf[i] )
	    {
		iLength = pabyComprBuf[i++];
		while( iLength > 0 && j < iUncomprSize && i < iComprSize )
		{
		    pabyUncomprBuf[j++] = pabyComprBuf[i];
		    iLength--;
		}
		i++;
	    }
	    else
	    {
		i++;
		if ( pabyComprBuf[i] == 0 )	    // Next scanline
		{
		    i++;
		}
		else if ( pabyComprBuf[i] == 1 )    // End of image
		{
		    break;
		}
		else if ( pabyComprBuf[i] == 2 )    // Move to...
		{
		    i++;
		    if ( i < iComprSize - 1 )
		    {
			j += pabyComprBuf[i++] +
			     pabyComprBuf[i++] * poDS->GetRasterXSize();
		    }
		    else
			break;
		}
		else				    // Absolute mode
		{
		    iLength = pabyComprBuf[i++];
		    for ( k = 0; k < iLength && j < iUncomprSize && i < iComprSize; k++ )
			pabyUncomprBuf[j++] = pabyComprBuf[i++];
		    if ( k & 0x01 )
			i++;
		}
	    }
	}
    }
    else					    // RLE4
    {
	while( j < iUncomprSize && i < iComprSize )
	{
	    if ( pabyComprBuf[i] )
	    {
		iLength = pabyComprBuf[i++];
		while( iLength > 0 && j < iUncomprSize && i < iComprSize )
		{
		    if ( iLength & 0x01 )
			pabyUncomprBuf[j++] = (pabyComprBuf[i] & 0xF0) >> 4;
		    else
			pabyUncomprBuf[j++] = pabyComprBuf[i] & 0x0F;
		    iLength--;
		}
		i++;
	    }
	    else
	    {
		i++;
		if ( pabyComprBuf[i] == 0 )	    // Next scanline
		{
		    i++;
		}
		else if ( pabyComprBuf[i] == 1 )    // End of image
		{
		    break;
		}
		else if ( pabyComprBuf[i] == 2 )    // Move to...
		{
		    i++;
		    if ( i < iComprSize - 1 )
		    {
			j += pabyComprBuf[i++] +
			     pabyComprBuf[i++] * poDS->GetRasterXSize();
		    }
		    else
			break;
		}
		else				    // Absolute mode
		{
		    iLength = pabyComprBuf[i++];
		    for ( k = 0; k < iLength && j < iUncomprSize && i < iComprSize; k++ )
		    {
			if ( k & 0x01 )
			    pabyUncomprBuf[j++] = pabyComprBuf[i++] & 0x0F;
			else
			    pabyUncomprBuf[j++] = (pabyComprBuf[i] & 0xF0) >> 4;
		    }
		    if ( k & 0x01 )
			i++;
		}
	    }
	}
    }
}

/************************************************************************/
/*                           ~BMPComprRasterBand()                      */
/************************************************************************/

BMPComprRasterBand::~BMPComprRasterBand()
{
    if ( pabyComprBuf )
	CPLFree( pabyComprBuf );
    if ( pabyUncomprBuf )
	CPLFree( pabyUncomprBuf );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BMPComprRasterBand::
    IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage )
{
    memcpy( pImage, pabyUncomprBuf +
	    (poDS->GetRasterYSize() - nBlockYOff - 1) * poDS->GetRasterXSize(),
	    nBlockXSize );

    return CE_None;
}

/************************************************************************/
/*                           BMPDataset()				*/
/************************************************************************/

BMPDataset::BMPDataset()
{
    pszFilename = NULL;
    fp = NULL;
    nBands = 0;
    pszProjection = CPLStrdup( "" );
    bGeoTransformValid = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    pabyColorTable = NULL;
    poColorTable = NULL;
    memset( &sFileHeader, 0, sizeof(sFileHeader) );
    memset( &sInfoHeader, 0, sizeof(sInfoHeader) );
}

/************************************************************************/
/*                            ~BMPDataset()                             */
/************************************************************************/

BMPDataset::~BMPDataset()
{
    FlushCache();

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

CPLErr BMPDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0]) * 6 );
    
    if( bGeoTransformValid )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr BMPDataset::SetGeoTransform( double * padfTransform )
{
    CPLErr		eErr = CE_None;
    
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );
    
    if ( pszFilename && bGeoTransformValid )
	if ( GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform )
	     == FALSE )
	{
	    CPLError( CE_Failure, CPLE_FileIO, "Can't write world file." );
	    eErr = CE_Failure;
	}
    
    return eErr;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BMPDataset::GetProjectionRef()
{
    if( pszProjection )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void BMPDataset::FlushCache()

{
    GDALDataset::FlushCache();
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BMPDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->fp == NULL )
        return NULL;

    if(	!EQUALN((const char *) poOpenInfo->pabyHeader, "BM", 2) )
        return NULL;

    VSIFClose( poOpenInfo->fp );
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    BMPDataset	    *poDS;
    VSIStatBuf	    sStat;

    poDS = new BMPDataset();

    if( poOpenInfo->eAccess == GA_ReadOnly )
	poDS->fp = VSIFOpen( poOpenInfo->pszFilename, "rb" );
    else
	poDS->fp = VSIFOpen( poOpenInfo->pszFilename, "r+b" );
    if ( !poDS->fp )
	return NULL;

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
    BMPType	    eBMPType;
    
    VSIFSeek( poDS->fp, BFH_SIZE, SEEK_SET );
    VSIFRead( &poDS->sInfoHeader.iSize, 1, 4, poDS->fp );
#ifdef CPL_MSB
    CPL_SWAP32PTR( &poDS->sInfoHeader.iSize );
#endif

    if ( poDS->sInfoHeader.iSize == BIH_WIN4SIZE )
	eBMPType = BMPT_WIN4;
    else if ( poDS->sInfoHeader.iSize == BIH_OS21SIZE )
	eBMPType = BMPT_OS21;
    else if ( poDS->sInfoHeader.iSize == BIH_OS22SIZE ||
	      poDS->sInfoHeader.iSize == 16 )
	eBMPType = BMPT_OS22;
    else
	eBMPType = BMPT_WIN5;

    if ( eBMPType == BMPT_WIN4 || eBMPType == BMPT_WIN5 || eBMPType == BMPT_OS22 )
    {
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
	poDS->nColorElems = 4;
    }
    if ( eBMPType == BMPT_OS22 )
    {
	poDS->nColorElems = 3; // FIXME: different info in different documents regarding this!
    }
    if ( eBMPType == BMPT_OS21 )
    {
	GInt16  iShort;

	VSIFRead( &iShort, 1, 2, poDS->fp );
	poDS->sInfoHeader.iWidth = CPL_LSBWORD16( iShort );
	VSIFRead( &iShort, 1, 2, poDS->fp );
	poDS->sInfoHeader.iHeight = CPL_LSBWORD16( iShort );
	VSIFRead( &iShort, 1, 2, poDS->fp );
	poDS->sInfoHeader.iPlanes = CPL_LSBWORD16( iShort );
	VSIFRead( &iShort, 1, 2, poDS->fp );
	poDS->sInfoHeader.iBitCount = CPL_LSBWORD16( iShort );
	poDS->nColorElems = 3;
    }

    if ( poDS->sInfoHeader.iBitCount != 1  &&
	 poDS->sInfoHeader.iBitCount != 4  &&
	 poDS->sInfoHeader.iBitCount != 8  &&
	 poDS->sInfoHeader.iBitCount != 16 &&
	 poDS->sInfoHeader.iBitCount != 24 &&
	 poDS->sInfoHeader.iBitCount != 32 )
    {
	delete poDS;
	return NULL;
    }

    CPLDebug( "BMP", "Windows Device Independent Bitmap parameters:\n"
	      " info header size: %d bytes\n"
	      " width: %d\n height: %d\n planes: %d\n bpp: %d\n"
	      " compression: %d\n image size: %d bytes\n X resolution: %d\n"
	      " Y resolution: %d\n colours used: %d\n colours important: %d",
	      poDS->sInfoHeader.iSize,
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
	    poDS->pabyColorTable =
		(GByte *)CPLMalloc( poDS->nColorElems * poDS->nColorTableSize );
	    VSIFSeek( poDS->fp, BFH_SIZE + poDS->sInfoHeader.iSize, SEEK_SET );
	    VSIFRead( poDS->pabyColorTable, poDS->nColorElems,
		      poDS->nColorTableSize, poDS->fp );

	    GDALColorEntry oEntry;
	    poDS->poColorTable = new GDALColorTable();
	    for( i = 0; i < poDS->nColorTableSize; i++ )
	    {
                oEntry.c1 = poDS->pabyColorTable[i * poDS->nColorElems + 2]; // Red
                oEntry.c2 = poDS->pabyColorTable[i * poDS->nColorElems + 1]; // Green
                oEntry.c3 = poDS->pabyColorTable[i * poDS->nColorElems];     // Blue
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
    
    if ( poDS->sInfoHeader.iCompression == BMPC_RGB )
    {
	for( iBand = 1; iBand <= poDS->nBands; iBand++ )
	    poDS->SetBand( iBand, new BMPRasterBand( poDS, iBand ) );
    }
    else if ( poDS->sInfoHeader.iCompression == BMPC_RLE8 )
    {
	for( iBand = 1; iBand <= poDS->nBands; iBand++ )
	    poDS->SetBand( iBand, new BMPComprRasterBand( poDS, iBand ) );
    }
    else
    {
	delete poDS;
	return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    poDS->bGeoTransformValid = 
        GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                           poDS->adfGeoTransform );

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *BMPDataset::Create( const char * pszFilename,
				 int nXSize, int nYSize, int nBands,
				 GDALDataType eType, char **papszOptions )

{
    if( eType != GDT_Byte )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create BMP dataset with an illegal\n"
              "data type (%s), only Byte supported by the format.\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

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
    BMPDataset	    *poDS;

    poDS = new BMPDataset();

    poDS->fp = VSIFOpen( pszFilename, "wb+" );
    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create file %s.\n", 
                  pszFilename );
        return NULL;
    }

    poDS->pszFilename = pszFilename;

/* -------------------------------------------------------------------- */
/*      Fill the BitmapInfoHeader                                       */
/* -------------------------------------------------------------------- */
    int nScanSize;
    
    poDS->sInfoHeader.iSize = 40;
    poDS->sInfoHeader.iWidth = nXSize;
    poDS->sInfoHeader.iHeight = nYSize;
    poDS->sInfoHeader.iPlanes = 1;
    poDS->sInfoHeader.iBitCount = ( nBands == 3 )?24:8;
    poDS->sInfoHeader.iCompression = BMPC_RGB;
    nScanSize = ((poDS->sInfoHeader.iWidth *
		  poDS->sInfoHeader.iBitCount + 31) & ~31) / 8;
    poDS->sInfoHeader.iSizeImage = nScanSize * poDS->sInfoHeader.iHeight;
    poDS->sInfoHeader.iXPelsPerMeter = 0;
    poDS->sInfoHeader.iYPelsPerMeter = 0;
    poDS->nColorElems = 4;

/* -------------------------------------------------------------------- */
/*      Do we need colour table?                                        */
/* -------------------------------------------------------------------- */
    int		i;
 
    if ( nBands == 1 )
    {
	poDS->sInfoHeader.iClrUsed = 1 << poDS->sInfoHeader.iBitCount;
	poDS->pabyColorTable = 
	    (GByte *) CPLMalloc( poDS->nColorElems * poDS->sInfoHeader.iClrUsed );
	for ( i = 0; i < poDS->sInfoHeader.iClrUsed; i++ )
	{
	    poDS->pabyColorTable[i * poDS->nColorElems] =
		poDS->pabyColorTable[i * poDS->nColorElems + 1] =
		poDS->pabyColorTable[i * poDS->nColorElems + 2] =
		poDS->pabyColorTable[i * poDS->nColorElems + 3] = (GByte) i;
	}
    }
    else
    {
	poDS->sInfoHeader.iClrUsed = 0;
    }
    poDS->sInfoHeader.iClrImportant = 0;

/* -------------------------------------------------------------------- */
/*      Fill the BitmapFileHeader                                       */
/* -------------------------------------------------------------------- */
    poDS->sFileHeader.bType[0] = 'B';
    poDS->sFileHeader.bType[1] = 'M';
    poDS->sFileHeader.iSize = BFH_SIZE + poDS->sInfoHeader.iSize +
		    poDS->sInfoHeader.iClrUsed * poDS->nColorElems +
		    poDS->sInfoHeader.iSizeImage;
    poDS->sFileHeader.iReserved1 = 0;
    poDS->sFileHeader.iReserved2 = 0;
    poDS->sFileHeader.iOffBits = BFH_SIZE + poDS->sInfoHeader.iSize +
		    poDS->sInfoHeader.iClrUsed * poDS->nColorElems;

/* -------------------------------------------------------------------- */
/*      Write all structures to the file                                */
/* -------------------------------------------------------------------- */
    VSIFWrite( &poDS->sFileHeader.bType, 1, 2, poDS->fp );
    
    GInt32	iLong;
    GInt16	iShort;
    
    iLong = CPL_LSBWORD32( poDS->sFileHeader.iSize );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iShort = CPL_LSBWORD16( poDS->sFileHeader.iReserved1 );
    VSIFWrite( &iShort, 2, 1, poDS->fp );
    iShort = CPL_LSBWORD16( poDS->sFileHeader.iReserved2 );
    VSIFWrite( &iShort, 2, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sFileHeader.iOffBits );
    VSIFWrite( &iLong, 4, 1, poDS->fp );

    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iSize );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iWidth );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iHeight );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iShort = CPL_LSBWORD16( poDS->sInfoHeader.iPlanes );
    VSIFWrite( &iShort, 2, 1, poDS->fp );
    iShort = CPL_LSBWORD16( poDS->sInfoHeader.iBitCount );
    VSIFWrite( &iShort, 2, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iCompression );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iSizeImage );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iXPelsPerMeter );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iYPelsPerMeter );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iClrUsed );
    VSIFWrite( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iClrImportant );
    VSIFWrite( &iLong, 4, 1, poDS->fp );

    if ( poDS->sInfoHeader.iClrUsed )
	VSIFWrite( poDS->pabyColorTable, 1,
		   poDS->nColorElems * poDS->sInfoHeader.iClrUsed, poDS->fp );

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->nBands = nBands;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i <= poDS->nBands; i++ )
    {
        poDS->SetBand( i, new BMPRasterBand( poDS, i ) );
    }

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                        */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "WORLDFILE", FALSE ) )
	poDS->bGeoTransformValid = TRUE;

    return (GDALDataset *) poDS;
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
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='WORLDFILE' type='boolean' description='Write out world file'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = BMPDataset::Open;
        poDriver->pfnCreate = BMPDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

