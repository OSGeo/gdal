/******************************************************************************
 *
 * Project:  Microsoft Windows Bitmap
 * Purpose:  Read/write MS Windows Device Independent Bitmap (DIB) files
 *           and OS/2 Presentation Manager bitmaps v. 1.x and v. 2.x
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@remotesensing.org>
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_pam.h"

#include <limits>

CPL_CVSID("$Id$")

// Enable if you want to see lots of BMP debugging output.
// #define BMP_DEBUG

enum BMPType
{
    BMPT_WIN4,      // BMP used in Windows 3.0/NT 3.51/95
    BMPT_WIN5,      // BMP used in Windows NT 4.0/98/Me/2000/XP
    BMPT_OS21,      // BMP used in OS/2 PM 1.x
    BMPT_OS22       // BMP used in OS/2 PM 2.x
};

// Bitmap file consists of a BMPFileHeader structure followed by a
// BMPInfoHeader structure. An array of BMPColorEntry structures (also called
// a colour table) follows the bitmap information header structure. The colour
// table is followed by a second array of indexes into the colour table (the
// actual bitmap data). Data may be compressed, for 4-bpp and 8-bpp used RLE
// compression.
//
// +---------------------+
// | BMPFileHeader       |
// +---------------------+
// | BMPInfoHeader       |
// +---------------------+
// | BMPColorEntry array |
// +---------------------+
// | Colour-index array  |
// +---------------------+
//
// All numbers stored in Intel order with least significant byte first.

enum BMPComprMethod
{
    BMPC_RGB = 0L,              // Uncompressed
    BMPC_RLE8 = 1L,             // RLE for 8 bpp images
    BMPC_RLE4 = 2L,             // RLE for 4 bpp images
    BMPC_BITFIELDS = 3L,        // Bitmap is not compressed and the colour table
                                // consists of three DWORD color masks that specify
                                // the red, green, and blue components of each pixel.
                                // This is valid when used with 16- and 32-bpp bitmaps.
    BMPC_JPEG = 4L,             // Indicates that the image is a JPEG image.
    BMPC_PNG = 5L               // Indicates that the image is a PNG image.
};

enum BMPLCSType                 // Type of logical color space.
{
    BMPLT_CALIBRATED_RGB = 0,   // This value indicates that endpoints and gamma
                                // values are given in the appropriate fields.
    BMPLT_DEVICE_RGB = 1,
    BMPLT_DEVICE_CMYK = 2
};

typedef struct
{
    // cppcheck-suppress unusedStructMember
    GInt32      iCIEX;
    // cppcheck-suppress unusedStructMember
    GInt32      iCIEY;
    // cppcheck-suppress unusedStructMember
    GInt32      iCIEZ;
} BMPCIEXYZ;

typedef struct                  // This structure contains the x, y, and z
{                               // coordinates of the three colors that correspond
    BMPCIEXYZ   iCIERed;        // to the red, green, and blue endpoints for
    BMPCIEXYZ   iCIEGreen;      // a specified logical color space.
    BMPCIEXYZ   iCIEBlue;
} BMPCIEXYZTriple;

typedef struct
{
    GByte       bType[2];       // Signature "BM"
    GUInt32     iSize;          // Size in bytes of the bitmap file. Should always
                                // be ignored while reading because of error
                                // in Windows 3.0 SDK's description of this field
    GUInt16     iReserved1;     // Reserved, set as 0
    GUInt16     iReserved2;     // Reserved, set as 0
    GUInt32     iOffBits;       // Offset of the image from file start in bytes
} BMPFileHeader;

// File header size in bytes:
constexpr int BFH_SIZE = 14;

typedef struct
{
    GUInt32     iSize;          // Size of BMPInfoHeader structure in bytes.
                                // Should be used to determine start of the
                                // colour table
    GInt32      iWidth;         // Image width
    GInt32      iHeight;        // Image height. If positive, image has bottom left
                                // origin, if negative --- top left.
    GUInt16     iPlanes;        // Number of image planes (must be set to 1)
    GUInt16     iBitCount;      // Number of bits per pixel (1, 4, 8, 16, 24 or 32).
                                // If 0 then the number of bits per pixel is
                                // specified or is implied by the JPEG or PNG format.
    BMPComprMethod iCompression; // Compression method
    GUInt32     iSizeImage;     // Size of uncompressed image in bytes. May be 0
                                // for BMPC_RGB bitmaps. If iCompression is BI_JPEG
                                // or BI_PNG, iSizeImage indicates the size
                                // of the JPEG or PNG image buffer.
    GInt32      iXPelsPerMeter; // X resolution, pixels per meter (0 if not used)
    GInt32      iYPelsPerMeter; // Y resolution, pixels per meter (0 if not used)
    GUInt32     iClrUsed;       // Size of colour table. If 0, iBitCount should
                                // be used to calculate this value (1<<iBitCount)
    GUInt32     iClrImportant;  // Number of important colours. If 0, all
                                // colours are required

    // Fields above should be used for bitmaps, compatible with Windows NT 3.51
    // and earlier. Windows 98/Me, Windows 2000/XP introduces additional fields:

    GUInt32     iRedMask;       // Colour mask that specifies the red component
                                // of each pixel, valid only if iCompression
                                // is set to BI_BITFIELDS.
    GUInt32     iGreenMask;     // The same for green component
    GUInt32     iBlueMask;      // The same for blue component
    // cppcheck-suppress unusedStructMember
    GUInt32     iAlphaMask;     // Colour mask that specifies the alpha
                                // component of each pixel.
    // cppcheck-suppress unusedStructMember
    BMPLCSType  iCSType;        // Colour space of the DIB.
    BMPCIEXYZTriple sEndpoints; // This member is ignored unless the iCSType member
                                // specifies BMPLT_CALIBRATED_RGB.
    // cppcheck-suppress unusedStructMember
    GUInt32     iGammaRed;      // Toned response curve for red. This member
                                // is ignored unless color values are calibrated
                                // RGB values and iCSType is set to
                                // BMPLT_CALIBRATED_RGB. Specified in 16^16 format.
    // cppcheck-suppress unusedStructMember
    GUInt32     iGammaGreen;    // Toned response curve for green.
    // cppcheck-suppress unusedStructMember
    GUInt32     iGammaBlue;     // Toned response curve for blue.
} BMPInfoHeader;

// Info header size in bytes:
const unsigned int  BIH_WIN4SIZE = 40; // for BMPT_WIN4
#if 0  /* Unused */
const unsigned int  BIH_WIN5SIZE = 57; // for BMPT_WIN5
#endif
const unsigned int  BIH_OS21SIZE = 12; // for BMPT_OS21
const unsigned int  BIH_OS22SIZE = 64; // for BMPT_OS22

// We will use plain byte array instead of this structure, but declaration
// provided for reference
typedef struct
{
    // cppcheck-suppress unusedStructMember
    GByte       bBlue;
    // cppcheck-suppress unusedStructMember
    GByte       bGreen;
    // cppcheck-suppress unusedStructMember
    GByte       bRed;
    // cppcheck-suppress unusedStructMember
    GByte       bReserved;      // Must be 0
} BMPColorEntry;

/*****************************************************************/

static int countonbits( GUInt32 dw )
{
    int r = 0;
    for( int x = 0; x < 32; x++ )
    {
        if( (dw & (1U << x)) != 0 )
            r++;
    }
    return r;
}

static int findfirstonbit( GUInt32 n )
{
    for( int x = 0; x < 32; x++ )
    {
        if( (n & (1U << x)) != 0 )
            return x;
    }
    return -1;
}

/************************************************************************/
/* ==================================================================== */
/*                              BMPDataset                              */
/* ==================================================================== */
/************************************************************************/

class BMPDataset final: public GDALPamDataset
{
    friend class BMPRasterBand;
    friend class BMPComprRasterBand;

    BMPFileHeader       sFileHeader;
    BMPInfoHeader       sInfoHeader;
    int                 nColorElems;
    GByte               *pabyColorTable;
    GDALColorTable      *poColorTable;
    double              adfGeoTransform[6];
    int                 bGeoTransformValid;

    char                *pszFilename;
    VSILFILE            *fp;

  protected:
    CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                      void *, int, int, GDALDataType,
                      int, int *,
                      GSpacing nPixelSpace, GSpacing nLineSpace,
                      GSpacing nBandSpace,
                      GDALRasterIOExtraArg* psExtraArg ) override;

  public:
    BMPDataset();
    ~BMPDataset() override;

    static int Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParamList );

    CPLErr GetGeoTransform( double * padfTransform ) override;
    CPLErr SetGeoTransform( double * ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                            BMPRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BMPRasterBand CPL_NON_FINAL: public GDALPamRasterBand
{
    friend class BMPDataset;

  protected:

    GUInt32         nScanSize;
    unsigned int    iBytesPerPixel;
    GByte           *pabyScan;

  public:
    BMPRasterBand( BMPDataset *, int );
    ~BMPRasterBand() override;

    CPLErr IReadBlock( int, int, void * ) override;
    CPLErr IWriteBlock( int, int, void * ) override;
    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
    CPLErr SetColorTable( GDALColorTable * ) override;
};

/************************************************************************/
/*                           BMPRasterBand()                            */
/************************************************************************/

BMPRasterBand::BMPRasterBand( BMPDataset *poDSIn, int nBandIn ) :
    nScanSize(0),
    iBytesPerPixel(poDSIn->sInfoHeader.iBitCount / 8),
    pabyScan(nullptr)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_Byte;

    // We will read one scanline per time. Scanlines in BMP aligned at 4-byte
    // boundary
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    const auto knIntMax = std::numeric_limits<int>::max();
    if (nBlockXSize < (knIntMax - 31) / poDSIn->sInfoHeader.iBitCount)
    {
        nScanSize =
            ((poDS->GetRasterXSize() *
              poDSIn->sInfoHeader.iBitCount + 31) & ~31) / 8;
    }
    else
    {
        // pabyScan = NULL;
        return;
    }

#ifdef BMP_DEBUG
    CPLDebug( "BMP",
              "Band %d: set nBlockXSize=%d, nBlockYSize=%d, nScanSize=%d",
              nBand, nBlockXSize, nBlockYSize, nScanSize );
#endif

    pabyScan = static_cast<GByte *>(VSIMalloc( nScanSize ));
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

CPLErr BMPRasterBand::IReadBlock( int /* nBlockXOff */,
                                  int nBlockYOff,
                                  void * pImage )
{
    BMPDataset  *poGDS = (BMPDataset *) poDS;
    vsi_l_offset iScanOffset = 0;

    if ( poGDS->sInfoHeader.iHeight > 0 )
        iScanOffset = poGDS->sFileHeader.iOffBits +
            ( poGDS->GetRasterYSize() - nBlockYOff - 1 ) *
                static_cast<vsi_l_offset>(nScanSize);
    else
        iScanOffset = poGDS->sFileHeader.iOffBits +
            nBlockYOff * static_cast<vsi_l_offset>(nScanSize);

    if ( VSIFSeekL( poGDS->fp, iScanOffset, SEEK_SET ) < 0 )
    {
        // XXX: We will not report error here, because file just may be
        // in update state and data for this block will be available later.
        if( poGDS->eAccess == GA_Update )
        {
            memset( pImage, 0, nBlockXSize );
            return CE_None;
        }
        else
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Can't seek to offset " CPL_FRMT_GUIB " in input file to read data.",
                      iScanOffset );
            return CE_Failure;
        }
    }
    if ( VSIFReadL( pabyScan, 1, nScanSize, poGDS->fp ) < nScanSize )
    {
        // XXX
        if( poGDS->eAccess == GA_Update )
        {
            memset( pImage, 0, nBlockXSize );
            return CE_None;
        }
        else
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Can't read from offset " CPL_FRMT_GUIB " in input file.",
                      iScanOffset );
            return CE_Failure;
        }
    }

    if ( poGDS->sInfoHeader.iBitCount == 24 ||
         poGDS->sInfoHeader.iBitCount == 32 )
    {
        GByte *pabyTemp = pabyScan + 3 - nBand;

        for ( int i = 0; i < nBlockXSize; i++ )
        {
            // Colour triplets in BMP file organized in reverse order:
            // blue, green, red. When we have 32-bit BMP the forth byte
            // in quadruplet should be discarded as it has no meaning.
            // That is why we always use 3 byte count in the following
            // pabyTemp index.
            ((GByte *) pImage)[i] = *pabyTemp;
            pabyTemp += iBytesPerPixel;
        }
    }
    else if ( poGDS->sInfoHeader.iBitCount == 8 )
    {
        memcpy( pImage, pabyScan, nBlockXSize );
    }
    else if ( poGDS->sInfoHeader.iBitCount == 16 )
    {
        // rcg, oct 7/06: Byteswap if necessary, use int16
        // references to file pixels, expand samples to
        // 8-bit, support BMPC_BITFIELDS channel mask indicators,
        // and generalize band handling.

        GUInt16* pScan16 = (GUInt16*)pabyScan;
#ifdef CPL_MSB
        GDALSwapWords( pScan16, sizeof(GUInt16), nBlockXSize, 0);
#endif

        // todo: make these band members and precompute.
        int mask[3], shift[3], size[3];
        float fTo8bit[3];

        if(poGDS->sInfoHeader.iCompression == BMPC_RGB)
        {
            mask[0] = 0x7c00;
            mask[1] = 0x03e0;
            mask[2] = 0x001f;
        }
        else if(poGDS->sInfoHeader.iCompression == BMPC_BITFIELDS)
        {
            mask[0] = poGDS->sInfoHeader.iRedMask;
            mask[1] = poGDS->sInfoHeader.iGreenMask;
            mask[2] = poGDS->sInfoHeader.iBlueMask;
        }
        else
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unknown 16-bit compression %d.",
                      poGDS->sInfoHeader.iCompression);
            return CE_Failure;
        }

        for( int i = 0; i < 3; i++)
        {
            shift[i] = findfirstonbit(mask[i]);
            size[i]  = countonbits(mask[i]);
            if(size[i] > 14 || size[i] == 0)
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Bad 16-bit channel mask %8x.",
                          mask[i]);
                return CE_Failure;
            }
            fTo8bit[i] = 255.0f / ((1 << size[i])-1);
        }

        for( int i = 0; i < nBlockXSize; i++ )
        {
            ((GByte *) pImage)[i] = (GByte)
                (0.5f + fTo8bit[nBand-1] *
                    ((pScan16[i] & mask[nBand-1]) >> shift[nBand-1]));
#if 0
        // original code
            switch ( nBand )
            {
                case 1: // Red
                ((GByte *) pImage)[i] = pabyScan[i + 1] & 0x1F;
                break;

                case 2: // Green
                ((GByte *) pImage)[i] =
                    ((pabyScan[i] & 0x03) << 3) |
                    ((pabyScan[i + 1] & 0xE0) >> 5);
                break;

                case 3: // Blue
                ((GByte *) pImage)[i] = (pabyScan[i] & 0x7c) >> 2;
                break;
                default:
                break;
            }
#endif // 0
        }
    }
    else if ( poGDS->sInfoHeader.iBitCount == 4 )
    {
        GByte *pabyTemp = pabyScan;

        for ( int i = 0; i < nBlockXSize; i++ )
        {
            // Most significant part of the byte represents leftmost pixel
            if ( i & 0x01 )
                ((GByte *) pImage)[i] = *pabyTemp++ & 0x0F;
            else
                ((GByte *) pImage)[i] = (*pabyTemp & 0xF0) >> 4;
        }
    }
    else if ( poGDS->sInfoHeader.iBitCount == 1 )
    {
        GByte *pabyTemp = pabyScan;

        for( int i = 0; i < nBlockXSize; i++ )
        {
            switch ( i & 0x7 )
            {
                case 0:
                ((GByte *) pImage)[i] = (*pabyTemp & 0x80) >> 7;
                break;
                case 1:
                ((GByte *) pImage)[i] = (*pabyTemp & 0x40) >> 6;
                break;
                case 2:
                ((GByte *) pImage)[i] = (*pabyTemp & 0x20) >> 5;
                break;
                case 3:
                ((GByte *) pImage)[i] = (*pabyTemp & 0x10) >> 4;
                break;
                case 4:
                ((GByte *) pImage)[i] = (*pabyTemp & 0x08) >> 3;
                break;
                case 5:
                ((GByte *) pImage)[i] = (*pabyTemp & 0x04) >> 2;
                break;
                case 6:
                ((GByte *) pImage)[i] = (*pabyTemp & 0x02) >> 1;
                break;
                case 7:
                ((GByte *) pImage)[i] = *pabyTemp++ & 0x01;
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
    BMPDataset  *poGDS = (BMPDataset *)poDS;

    CPLAssert( poGDS != nullptr
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != nullptr );

    vsi_l_offset iScanOffset = poGDS->sFileHeader.iOffBits +
            ( poGDS->GetRasterYSize() - nBlockYOff - 1 ) *
                static_cast<vsi_l_offset>(nScanSize);
    if ( VSIFSeekL( poGDS->fp, iScanOffset, SEEK_SET ) < 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't seek to offset " CPL_FRMT_GUIB " in output file to write data.\n%s",
                  iScanOffset, VSIStrerror( errno ) );
        return CE_Failure;
    }

    if( poGDS->nBands != 1 )
    {
        memset( pabyScan, 0, nScanSize );
        VSIFReadL( pabyScan, 1, nScanSize, poGDS->fp );
        VSIFSeekL( poGDS->fp, iScanOffset, SEEK_SET );
    }

    for ( int iInPixel = 0, iOutPixel = iBytesPerPixel - nBand;
          iInPixel < nBlockXSize; iInPixel++, iOutPixel += poGDS->nBands )
    {
        pabyScan[iOutPixel] = ((GByte *) pImage)[iInPixel];
    }

    if ( VSIFWriteL( pabyScan, 1, nScanSize, poGDS->fp ) < nScanSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't write block with X offset %d and Y offset %d.\n%s",
                  nBlockXOff, nBlockYOff,
                  VSIStrerror( errno ) );
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
    BMPDataset  *poGDS = (BMPDataset *) poDS;

    if ( poColorTable )
    {
        poGDS->sInfoHeader.iClrUsed = poColorTable->GetColorEntryCount();
        if ( poGDS->sInfoHeader.iClrUsed < 1 ||
             poGDS->sInfoHeader.iClrUsed > (1U << poGDS->sInfoHeader.iBitCount) )
            return CE_Failure;

        VSIFSeekL( poGDS->fp, BFH_SIZE + 32, SEEK_SET );

        GUInt32 iULong = CPL_LSBWORD32( poGDS->sInfoHeader.iClrUsed );
        VSIFWriteL( &iULong, 4, 1, poGDS->fp );
        poGDS->pabyColorTable = (GByte *) CPLRealloc( poGDS->pabyColorTable,
                        poGDS->nColorElems * poGDS->sInfoHeader.iClrUsed );
        if ( !poGDS->pabyColorTable )
            return CE_Failure;

        for( unsigned int i = 0; i < poGDS->sInfoHeader.iClrUsed; i++ )
        {
            GDALColorEntry  oEntry;

            poColorTable->GetColorEntryAsRGB( i, &oEntry );
            poGDS->pabyColorTable[i * poGDS->nColorElems + 3] = 0;
            poGDS->pabyColorTable[i * poGDS->nColorElems + 2] =
                (GByte) oEntry.c1; // Red
            poGDS->pabyColorTable[i * poGDS->nColorElems + 1] =
                (GByte) oEntry.c2; // Green
            poGDS->pabyColorTable[i * poGDS->nColorElems] =
                (GByte) oEntry.c3;     // Blue
        }

        VSIFSeekL( poGDS->fp, BFH_SIZE + poGDS->sInfoHeader.iSize, SEEK_SET );
        if ( VSIFWriteL( poGDS->pabyColorTable, 1,
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
    BMPDataset      *poGDS = (BMPDataset *) poDS;

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
    else
    {
        return GCI_PaletteIndex;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                       BMPComprRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BMPComprRasterBand final: public BMPRasterBand
{
    friend class BMPDataset;

    GByte           *pabyComprBuf;
    GByte           *pabyUncomprBuf;

  public:
    BMPComprRasterBand( BMPDataset *, int );
    ~BMPComprRasterBand() override;

    CPLErr IReadBlock( int, int, void * ) override;
    // virtual CPLErr IWriteBlock( int, int, void * );
};

/************************************************************************/
/*                           BMPComprRasterBand()                       */
/************************************************************************/

BMPComprRasterBand::BMPComprRasterBand( BMPDataset *poDSIn, int nBandIn ) :
    BMPRasterBand( poDSIn, nBandIn ),
    pabyComprBuf(nullptr),
    pabyUncomprBuf(nullptr)
{
    /* TODO: it might be interesting to avoid uncompressing the whole data */
    /* in a single pass, especially if nXSize * nYSize is big */
    /* We could read incrementally one row at a time */
    const auto knIntMax = std::numeric_limits<int>::max();
    if (poDS->GetRasterXSize() > knIntMax / poDS->GetRasterYSize())
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too big dimensions : %d x %d",
                 poDS->GetRasterXSize(), poDS->GetRasterYSize());
        return;
    }

    if( poDSIn->sFileHeader.iSize <= poDSIn->sFileHeader.iOffBits ||
        poDSIn->sFileHeader.iSize - poDSIn->sFileHeader.iOffBits > knIntMax )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid header");
        return;
    }

    GUInt32 iComprSize = poDSIn->sFileHeader.iSize - poDSIn->sFileHeader.iOffBits;
    GUInt32 iUncomprSize = poDS->GetRasterXSize() * poDS->GetRasterYSize();

#ifdef DEBUG
    CPLDebug( "BMP", "RLE compression detected." );
    CPLDebug ( "BMP", "Size of compressed buffer %ld bytes,"
               " size of uncompressed buffer %ld bytes.",
               (long) iComprSize, (long) iUncomprSize );
#endif

    pabyComprBuf = (GByte *) VSIMalloc( iComprSize );
    pabyUncomprBuf = (GByte *) VSIMalloc( iUncomprSize );
    if (pabyComprBuf == nullptr ||
        pabyUncomprBuf == nullptr)
    {
        CPLFree(pabyComprBuf);
        pabyComprBuf = nullptr;
        CPLFree(pabyUncomprBuf);
        pabyUncomprBuf = nullptr;
        return;
    }

    if( VSIFSeekL( poDSIn->fp, poDSIn->sFileHeader.iOffBits, SEEK_SET ) != 0 ||
        VSIFReadL( pabyComprBuf, 1, iComprSize, poDSIn->fp ) < iComprSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't read from offset %ld in input file.",
                  (long) poDSIn->sFileHeader.iOffBits );
        CPLFree(pabyComprBuf);
        pabyComprBuf = nullptr;
        CPLFree(pabyUncomprBuf);
        pabyUncomprBuf = nullptr;
        return;
    }

    unsigned int i = 0;
    unsigned int j = 0;
    if ( poDSIn->sInfoHeader.iBitCount == 8 )         // RLE8
    {
        while( i < iComprSize )
        {
            if ( pabyComprBuf[i] )
            {
                unsigned int iLength = pabyComprBuf[i++];
                if( j == iUncomprSize )
                    break;
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
                if( i == iComprSize )
                    break;
                if ( pabyComprBuf[i] == 0 )         // Next scanline
                {
                    i++;
                }
                else if ( pabyComprBuf[i] == 1 )    // End of image
                {
                    break;
                }
                else if ( pabyComprBuf[i] == 2 )    // Move to...
                {
                    if( j == iUncomprSize )
                        break;
                    i++;
                    if ( i < iComprSize - 1 )
                    {
                        if( pabyComprBuf[i+1] >
                                knIntMax / poDS->GetRasterXSize() ||
                            static_cast<int>(pabyComprBuf[i+1]) *
                                poDS->GetRasterXSize() >
                                knIntMax -
                                static_cast<int>(j + pabyComprBuf[i]) )
                            break;
                        j += pabyComprBuf[i] +
                             pabyComprBuf[i+1] * poDS->GetRasterXSize();
                        i += 2;
                    }
                    else
                        break;
                }
                else                                // Absolute mode
                {
                    CPLAssert (i < iComprSize);
                    unsigned int iLength = pabyComprBuf[i++];
                    if( j == iUncomprSize )
                        break;
                    for ( unsigned k = 0; k < iLength && j < iUncomprSize && i < iComprSize; k++ )
                        pabyUncomprBuf[j++] = pabyComprBuf[i++];
                    if ( i & 0x01 )
                        i++;
                }
            }
        }
    }
    else                                            // RLE4
    {
        while( i < iComprSize )
        {
            if ( pabyComprBuf[i] )
            {
                unsigned int iLength = pabyComprBuf[i++];
                if( j == iUncomprSize )
                    break;
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
                if( i == iComprSize )
                    break;
                if ( pabyComprBuf[i] == 0 )         // Next scanline
                {
                    i++;
                }
                else if ( pabyComprBuf[i] == 1 )    // End of image
                {
                    break;
                }
                else if ( pabyComprBuf[i] == 2 )    // Move to...
                {
                    if( j == iUncomprSize )
                        break;
                    i++;
                    if ( i < iComprSize - 1 )
                    {
                        if( pabyComprBuf[i+1] >
                                knIntMax / poDS->GetRasterXSize() ||
                            static_cast<int>(pabyComprBuf[i+1]) *
                                poDS->GetRasterXSize() >
                                knIntMax -
                                static_cast<int>(j + pabyComprBuf[i]) )
                            break;
                        j += pabyComprBuf[i] +
                             pabyComprBuf[i+1] * poDS->GetRasterXSize();
                        i += 2;
                    }
                    else
                        break;
                }
                else                                // Absolute mode
                {
                    CPLAssert (i < iComprSize);
                    unsigned int iLength = pabyComprBuf[i++];
                    if( j == iUncomprSize )
                        break;
                    for ( unsigned k = 0; k < iLength && j < iUncomprSize && i < iComprSize; k++ )
                    {
                        if ( k & 0x01 )
                            pabyUncomprBuf[j++] = pabyComprBuf[i++] & 0x0F;
                        else
                            pabyUncomprBuf[j++] = (pabyComprBuf[i] & 0xF0) >> 4;
                    }
                    if ( i & 0x01 )
                        i++;
                }
            }
        }
    }
    /* Validate that we have read all compressed data (we tolerate missing */
    /* end of image marker) and that we have filled all uncompressed data */
    if( j < iUncomprSize || (i+1 != iComprSize && i+2 != iComprSize) )
    {
        CPLFree(pabyUncomprBuf);
        pabyUncomprBuf = nullptr;
    }
    // rcg, release compressed buffer here.
    CPLFree( pabyComprBuf );
    pabyComprBuf = nullptr;
}

/************************************************************************/
/*                           ~BMPComprRasterBand()                      */
/************************************************************************/

BMPComprRasterBand::~BMPComprRasterBand()
{
    CPLFree( pabyComprBuf );
    CPLFree( pabyUncomprBuf );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BMPComprRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                       int nBlockYOff,
                                       void * pImage )
{
    memcpy( pImage, pabyUncomprBuf +
            (poDS->GetRasterYSize() - nBlockYOff - 1) * poDS->GetRasterXSize(),
            nBlockXSize );

    return CE_None;
}

/************************************************************************/
/*                           BMPDataset()                               */
/************************************************************************/

BMPDataset::BMPDataset() :
    nColorElems(0),
    pabyColorTable(nullptr),
    poColorTable(nullptr),
    bGeoTransformValid(FALSE),
    pszFilename(nullptr),
    fp(nullptr)
{
    nBands = 0;

    memset( &sFileHeader, 0, sizeof(sFileHeader) );
    memset( &sInfoHeader, 0, sizeof(sInfoHeader) );

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~BMPDataset()                             */
/************************************************************************/

BMPDataset::~BMPDataset()
{
    FlushCache();

    CPLFree( pabyColorTable );
    if ( poColorTable )
        delete poColorTable;
    CPLFree( pszFilename );
    if( fp )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BMPDataset::GetGeoTransform( double * padfTransform )
{
    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0])*6 );
        return CE_None;
    }

    if( GDALPamDataset::GetGeoTransform( padfTransform ) == CE_None)
        return CE_None;

#ifdef notdef
    // See http://trac.osgeo.org/gdal/ticket/3578
    if (sInfoHeader.iXPelsPerMeter > 0 && sInfoHeader.iYPelsPerMeter > 0)
    {
        padfTransform[1] = sInfoHeader.iXPelsPerMeter;
        padfTransform[5] = -sInfoHeader.iYPelsPerMeter;
        padfTransform[0] = -0.5*padfTransform[1];
        padfTransform[3] = -0.5*padfTransform[5];
        return CE_None;
    }
#endif

    return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr BMPDataset::SetGeoTransform( double * padfTransform )
{
    if ( pszFilename && bGeoTransformValid )
    {
        memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

        CPLErr eErr = CE_None;
        if ( GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform )
             == FALSE )
        {
            CPLError( CE_Failure, CPLE_FileIO, "Can't write world file." );
            eErr = CE_Failure;
        }
        return eErr;
    }

    return GDALPamDataset::SetGeoTransform( padfTransform );
}

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      Multi-band raster io handler.  We will use  block based         */
/*      loading is used for multiband BMPs.  That is because they       */
/*      are effectively pixel interleaved, so processing all bands      */
/*      for a given block together avoid extra seeks.                   */
/************************************************************************/

CPLErr BMPDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg )

{
    if( nBandCount > 1 )
        return GDALDataset::BlockBasedRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace, psExtraArg );
    else
        return
            GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize, eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace, nBandSpace, psExtraArg );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BMPDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 2
        || poOpenInfo->pabyHeader[0] != 'B'
        || poOpenInfo->pabyHeader[1] != 'M' )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BMPDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr)
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    BMPDataset *poDS = new BMPDataset();
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    VSIStatBufL sStat;
    if (VSIStatL(poOpenInfo->pszFilename, &sStat) != 0)
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Read the BMPFileHeader. We need iOffBits value only             */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fp, 10, SEEK_SET );
    VSIFReadL( &poDS->sFileHeader.iOffBits, 1, 4, poDS->fp );
#ifdef CPL_MSB
    CPL_SWAP32PTR( &poDS->sFileHeader.iOffBits );
#endif
    poDS->sFileHeader.iSize = (GUInt32) sStat.st_size;

#ifdef BMP_DEBUG
    CPLDebug( "BMP", "File size %d bytes.", poDS->sFileHeader.iSize );
    CPLDebug( "BMP", "Image offset 0x%x bytes from file start.",
              poDS->sFileHeader.iOffBits );
#endif

/* -------------------------------------------------------------------- */
/*      Read the BMPInfoHeader.                                         */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fp, BFH_SIZE, SEEK_SET );
    VSIFReadL( &poDS->sInfoHeader.iSize, 1, 4, poDS->fp );
#ifdef CPL_MSB
    CPL_SWAP32PTR( &poDS->sInfoHeader.iSize );
#endif

    BMPType eBMPType;
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
        VSIFReadL( &poDS->sInfoHeader.iWidth, 1, 4, poDS->fp );
        VSIFReadL( &poDS->sInfoHeader.iHeight, 1, 4, poDS->fp );
        VSIFReadL( &poDS->sInfoHeader.iPlanes, 1, 2, poDS->fp );
        VSIFReadL( &poDS->sInfoHeader.iBitCount, 1, 2, poDS->fp );
        unsigned int iCompression;
        VSIFReadL( &iCompression, 1, 4, poDS->fp );
#ifdef CPL_MSB
        CPL_SWAP32PTR( &iCompression );
#endif
        if( iCompression > BMPC_PNG )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unsupported compression");
            delete poDS;
            return nullptr;
        }
        poDS->sInfoHeader.iCompression = static_cast<BMPComprMethod>(iCompression);
        VSIFReadL( &poDS->sInfoHeader.iSizeImage, 1, 4, poDS->fp );
        VSIFReadL( &poDS->sInfoHeader.iXPelsPerMeter, 1, 4, poDS->fp );
        VSIFReadL( &poDS->sInfoHeader.iYPelsPerMeter, 1, 4, poDS->fp );
        VSIFReadL( &poDS->sInfoHeader.iClrUsed, 1, 4, poDS->fp );
        VSIFReadL( &poDS->sInfoHeader.iClrImportant, 1, 4, poDS->fp );

        // rcg, read win4/5 fields. If we're reading a
        // legacy header that ends at iClrImportant, it turns
        // out that the three DWORD color table entries used
        // by the channel masks start here anyway.
        if(poDS->sInfoHeader.iCompression == BMPC_BITFIELDS)
        {
            VSIFReadL( &poDS->sInfoHeader.iRedMask, 1, 4, poDS->fp );
            VSIFReadL( &poDS->sInfoHeader.iGreenMask, 1, 4, poDS->fp );
            VSIFReadL( &poDS->sInfoHeader.iBlueMask, 1, 4, poDS->fp );
        }
#ifdef CPL_MSB
        CPL_SWAP32PTR( &poDS->sInfoHeader.iWidth );
        CPL_SWAP32PTR( &poDS->sInfoHeader.iHeight );
        CPL_SWAP16PTR( &poDS->sInfoHeader.iPlanes );
        CPL_SWAP16PTR( &poDS->sInfoHeader.iBitCount );
        CPL_SWAP32PTR( &poDS->sInfoHeader.iSizeImage );
        CPL_SWAP32PTR( &poDS->sInfoHeader.iXPelsPerMeter );
        CPL_SWAP32PTR( &poDS->sInfoHeader.iYPelsPerMeter );
        CPL_SWAP32PTR( &poDS->sInfoHeader.iClrUsed );
        CPL_SWAP32PTR( &poDS->sInfoHeader.iClrImportant );
        // rcg, swap win4/5 fields.
        CPL_SWAP32PTR( &poDS->sInfoHeader.iRedMask );
        CPL_SWAP32PTR( &poDS->sInfoHeader.iGreenMask );
        CPL_SWAP32PTR( &poDS->sInfoHeader.iBlueMask );
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

        VSIFReadL( &iShort, 1, 2, poDS->fp );
        poDS->sInfoHeader.iWidth = CPL_LSBWORD16( iShort );
        VSIFReadL( &iShort, 1, 2, poDS->fp );
        poDS->sInfoHeader.iHeight = CPL_LSBWORD16( iShort );
        VSIFReadL( &iShort, 1, 2, poDS->fp );
        poDS->sInfoHeader.iPlanes = CPL_LSBWORD16( iShort );
        VSIFReadL( &iShort, 1, 2, poDS->fp );
        poDS->sInfoHeader.iBitCount = CPL_LSBWORD16( iShort );
        poDS->sInfoHeader.iCompression = BMPC_RGB;
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
        return nullptr;
    }

#ifdef BMP_DEBUG
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
#endif

    if( poDS->sInfoHeader.iHeight == INT_MIN )
    {
        delete poDS;
        return nullptr;
    }

    poDS->nRasterXSize = poDS->sInfoHeader.iWidth;
    poDS->nRasterYSize = (poDS->sInfoHeader.iHeight > 0)?
        poDS->sInfoHeader.iHeight:-poDS->sInfoHeader.iHeight;

    if  (poDS->nRasterXSize <= 0 || poDS->nRasterYSize <= 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid dimensions : %d x %d",
                  poDS->nRasterXSize, poDS->nRasterYSize);
        delete poDS;
        return nullptr;
    }

    switch ( poDS->sInfoHeader.iBitCount )
    {
        case 1:
        case 4:
        case 8:
        {
            poDS->nBands = 1;
            int nColorTableSize;
            int nMaxColorTableSize = 1 << poDS->sInfoHeader.iBitCount;
            // Allocate memory for colour table and read it
            if ( poDS->sInfoHeader.iClrUsed )
            {
                if( poDS->sInfoHeader.iClrUsed > (GUInt32)nMaxColorTableSize )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Wrong value for iClrUsed: %u",
                             poDS->sInfoHeader.iClrUsed );
                    delete poDS;
                    return nullptr;
                }
                nColorTableSize = poDS->sInfoHeader.iClrUsed;
            }
            else
                nColorTableSize = nMaxColorTableSize;

            poDS->pabyColorTable =
                (GByte *)VSI_MALLOC2_VERBOSE( poDS->nColorElems, nColorTableSize );
            if (poDS->pabyColorTable == nullptr)
            {
                break;
            }

            if( VSIFSeekL( poDS->fp, BFH_SIZE + static_cast<vsi_l_offset>(poDS->sInfoHeader.iSize), SEEK_SET ) != 0 ||
                VSIFReadL( poDS->pabyColorTable, poDS->nColorElems,
                           nColorTableSize, poDS->fp ) != (size_t)nColorTableSize )
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot read color table");
                delete poDS;
                return nullptr;
            }

            GDALColorEntry oEntry;
            poDS->poColorTable = new GDALColorTable();
            for( int i = 0; i < nColorTableSize; i++ )
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
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    if ( poDS->sInfoHeader.iCompression == BMPC_RGB
    ||   poDS->sInfoHeader.iCompression == BMPC_BITFIELDS )
    {
        for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
        {
            BMPRasterBand* band = new BMPRasterBand( poDS, iBand );
            poDS->SetBand( iBand, band );
            if (band->pabyScan == nullptr)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "The BMP file is probably corrupted or too large. Image width = %d", poDS->nRasterXSize);
                delete poDS;
                return nullptr;
            }
        }
    }
    else if ( poDS->sInfoHeader.iCompression == BMPC_RLE8
              || poDS->sInfoHeader.iCompression == BMPC_RLE4 )
    {
        for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
        {
            BMPComprRasterBand* band = new BMPComprRasterBand( poDS, iBand );
            poDS->SetBand( iBand, band);
            if (band->pabyUncomprBuf == nullptr)
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "The BMP file is probably corrupted or too large. Image width = %d", poDS->nRasterXSize);
                delete poDS;
                return nullptr;
            }
        }
    }
    else
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Check for world file.                                           */
/* -------------------------------------------------------------------- */
    poDS->bGeoTransformValid =
        GDALReadWorldFile( poOpenInfo->pszFilename, nullptr,
                           poDS->adfGeoTransform );

    if( !poDS->bGeoTransformValid )
        poDS->bGeoTransformValid =
            GDALReadWorldFile( poOpenInfo->pszFilename, ".wld",
                               poDS->adfGeoTransform );

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

        return nullptr;
    }

    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "BMP driver doesn't support %d bands. Must be 1 or 3.\n",
                  nBands );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    BMPDataset *poDS = new BMPDataset();

    poDS->fp = VSIFOpenL( pszFilename, "wb+" );
    if( poDS->fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create file %s.\n",
                  pszFilename );
        delete poDS;
        return nullptr;
    }

    poDS->pszFilename = CPLStrdup(pszFilename);

/* -------------------------------------------------------------------- */
/*      Fill the BMPInfoHeader                                          */
/* -------------------------------------------------------------------- */
    poDS->sInfoHeader.iSize = 40;
    poDS->sInfoHeader.iWidth = nXSize;
    poDS->sInfoHeader.iHeight = nYSize;
    poDS->sInfoHeader.iPlanes = 1;
    poDS->sInfoHeader.iBitCount = ( nBands == 3 )?24:8;
    poDS->sInfoHeader.iCompression = BMPC_RGB;

    /* XXX: Avoid integer overflow. We can calculate size in one
     * step using
     *
     *   nScanSize = ((poDS->sInfoHeader.iWidth *
     *            poDS->sInfoHeader.iBitCount + 31) & ~31) / 8
     *
     * formula, but we should check for overflow conditions
     * during calculation.
     */
    GUInt32 nScanSize
        = (GUInt32)poDS->sInfoHeader.iWidth * poDS->sInfoHeader.iBitCount + 31;
    if ( !poDS->sInfoHeader.iWidth
         || !poDS->sInfoHeader.iBitCount
         || (nScanSize - 31) / poDS->sInfoHeader.iBitCount
                != (GUInt32)poDS->sInfoHeader.iWidth )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Wrong image parameters; "
                  "can't allocate space for scanline buffer" );
        delete poDS;

        return nullptr;
    }
    nScanSize = (nScanSize & ~31U) / 8;

    poDS->sInfoHeader.iSizeImage = nScanSize * poDS->sInfoHeader.iHeight;
    poDS->sInfoHeader.iXPelsPerMeter = 0;
    poDS->sInfoHeader.iYPelsPerMeter = 0;
    poDS->nColorElems = 4;

/* -------------------------------------------------------------------- */
/*      Do we need colour table?                                        */
/* -------------------------------------------------------------------- */
    if ( nBands == 1 )
    {
        poDS->sInfoHeader.iClrUsed = 1 << poDS->sInfoHeader.iBitCount;
        poDS->pabyColorTable =
            (GByte *) CPLMalloc( poDS->nColorElems * poDS->sInfoHeader.iClrUsed );
        for ( unsigned int i = 0; i < poDS->sInfoHeader.iClrUsed; i++ )
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
/*      Fill the BMPFileHeader                                          */
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
    if( VSIFWriteL( &poDS->sFileHeader.bType, 1, 2, poDS->fp ) != 2 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Write of first 2 bytes to BMP file %s failed.\n"
                  "Is file system full?",
                  pszFilename );
        delete poDS;

        return nullptr;
    }

    GInt32      iLong;
    GUInt32     iULong;
    GUInt16     iUShort;

    iULong = CPL_LSBWORD32( poDS->sFileHeader.iSize );
    VSIFWriteL( &iULong, 4, 1, poDS->fp );
    iUShort = CPL_LSBWORD16( poDS->sFileHeader.iReserved1 );
    VSIFWriteL( &iUShort, 2, 1, poDS->fp );
    iUShort = CPL_LSBWORD16( poDS->sFileHeader.iReserved2 );
    VSIFWriteL( &iUShort, 2, 1, poDS->fp );
    iULong = CPL_LSBWORD32( poDS->sFileHeader.iOffBits );
    VSIFWriteL( &iULong, 4, 1, poDS->fp );

    iULong = CPL_LSBWORD32( poDS->sInfoHeader.iSize );
    VSIFWriteL( &iULong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iWidth );
    VSIFWriteL( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iHeight );
    VSIFWriteL( &iLong, 4, 1, poDS->fp );
    iUShort = CPL_LSBWORD16( poDS->sInfoHeader.iPlanes );
    VSIFWriteL( &iUShort, 2, 1, poDS->fp );
    iUShort = CPL_LSBWORD16( poDS->sInfoHeader.iBitCount );
    VSIFWriteL( &iUShort, 2, 1, poDS->fp );
    iULong = CPL_LSBWORD32( poDS->sInfoHeader.iCompression );
    VSIFWriteL( &iULong, 4, 1, poDS->fp );
    iULong = CPL_LSBWORD32( poDS->sInfoHeader.iSizeImage );
    VSIFWriteL( &iULong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iXPelsPerMeter );
    VSIFWriteL( &iLong, 4, 1, poDS->fp );
    iLong = CPL_LSBWORD32( poDS->sInfoHeader.iYPelsPerMeter );
    VSIFWriteL( &iLong, 4, 1, poDS->fp );
    iULong = CPL_LSBWORD32( poDS->sInfoHeader.iClrUsed );
    VSIFWriteL( &iULong, 4, 1, poDS->fp );
    iULong = CPL_LSBWORD32( poDS->sInfoHeader.iClrImportant );
    VSIFWriteL( &iULong, 4, 1, poDS->fp );

    if ( poDS->sInfoHeader.iClrUsed )
    {
        if( VSIFWriteL( poDS->pabyColorTable, 1,
                        poDS->nColorElems * poDS->sInfoHeader.iClrUsed, poDS->fp )
            != poDS->nColorElems * poDS->sInfoHeader.iClrUsed )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Error writing color table.  Is disk full?" );
            delete poDS;

            return nullptr;
        }
    }

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->nBands = nBands;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
    {
        poDS->SetBand( iBand, new BMPRasterBand( poDS, iBand ) );
    }

/* -------------------------------------------------------------------- */
/*      Do we need a world file?                                        */
/* -------------------------------------------------------------------- */
    if( CPLFetchBool( papszOptions, "WORLDFILE", false ) )
        poDS->bGeoTransformValid = TRUE;

    return (GDALDataset *) poDS;
}

/************************************************************************/
/*                        GDALRegister_BMP()                            */
/************************************************************************/

void GDALRegister_BMP()

{
    if( GDALGetDriverByName( "BMP" ) != nullptr )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "BMP" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "MS Windows Device Independent Bitmap" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/bmp.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "bmp" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='WORLDFILE' type='boolean' description='Write out world file'/>"
"</CreationOptionList>" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = BMPDataset::Open;
    poDriver->pfnCreate = BMPDataset::Create;
    poDriver->pfnIdentify = BMPDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
