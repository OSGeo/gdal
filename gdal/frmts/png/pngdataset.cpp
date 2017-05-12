/******************************************************************************
 *
 * Project:  PNG Driver
 * Purpose:  Implement GDAL PNG Support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 * ISSUES:
 *  o CollectMetadata() will only capture TEXT chunks before the image
 *    data as the code is currently structured.
 *  o Interlaced images are read entirely into memory for use.  This is
 *    bad for large images.
 *  o Image reading is always strictly sequential.  Reading backwards will
 *    cause the file to be rewound, and access started again from the
 *    beginning.
 *  o 16 bit alpha values are not scaled by to eight bit.
 *
 */

#include "pngdataset.h"

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "png.h"

#include <csetjmp>

#include <algorithm>

CPL_CVSID("$Id$");

// Note: Callers must provide blocks in increasing Y order.
// Disclaimer (E. Rouault): this code is not production ready at all. A lot of
// issues remain: uninitialized variables, unclosed files, lack of proper
// multiband handling, and an inability to read and write at the same time. Do
// not use it unless you're ready to fix it.

// Define SUPPORT_CREATE to enable use of the Create() call.
// #define SUPPORT_CREATE

#ifdef _MSC_VER
#  pragma warning(disable:4611)
#endif

static void
png_vsi_read_data(png_structp png_ptr, png_bytep data, png_size_t length);

static void
png_vsi_write_data(png_structp png_ptr, png_bytep data, png_size_t length);

static void png_vsi_flush(png_structp png_ptr);

static void png_gdal_error( png_structp png_ptr, const char *error_message );
static void png_gdal_warning( png_structp png_ptr, const char *error_message );


/************************************************************************/
/*                           PNGRasterBand()                            */
/************************************************************************/

PNGRasterBand::PNGRasterBand( PNGDataset *poDSIn, int nBandIn ) :
    bHaveNoData(FALSE),
    dfNoDataValue(-1)
{
    poDS = poDSIn;
    nBand = nBandIn;

    if( poDSIn->nBitDepth == 16 )
        eDataType = GDT_UInt16;
    else
        eDataType = GDT_Byte;

    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = 1;

#ifdef SUPPORT_CREATE
    reset_band_provision_flags();
#endif
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PNGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    PNGDataset *poGDS = reinterpret_cast<PNGDataset *>( poDS );
    int nPixelSize;

    CPLAssert( nBlockXOff == 0 );

    if( poGDS->nBitDepth == 16 )
        nPixelSize = 2;
    else
        nPixelSize = 1;

    const int nXSize = GetXSize();
    if (poGDS->fpImage == NULL)
    {
        memset( pImage, 0, nPixelSize * nXSize );
        return CE_None;
    }

    // Load the desired scanline into the working buffer.
    CPLErr eErr = poGDS->LoadScanline( nBlockYOff );
    if( eErr != CE_None )
        return eErr;

    const int nPixelOffset = poGDS->nBands * nPixelSize;

    GByte *pabyScanline = poGDS->pabyBuffer
        + (nBlockYOff - poGDS->nBufferStartLine) * nPixelOffset * nXSize
        + nPixelSize * (nBand - 1);

    // Transfer between the working buffer and the caller's buffer.
    if( nPixelSize == nPixelOffset )
        memcpy( pImage, pabyScanline, nPixelSize * nXSize );
    else if( nPixelSize == 1 )
    {
        for( int i = 0; i < nXSize; i++ )
            reinterpret_cast<GByte *>( pImage )[i] = pabyScanline[i*nPixelOffset];
    }
    else
    {
        CPLAssert( nPixelSize == 2 );
        for( int i = 0; i < nXSize; i++ )
        {
            reinterpret_cast<GUInt16 *>( pImage )[i] =
                *reinterpret_cast<GUInt16 *>( pabyScanline+i*nPixelOffset );
        }
    }

    // Forcibly load the other bands associated with this scanline.
    for(int iBand = 1; iBand < poGDS->GetRasterCount(); iBand++)
    {
        GDALRasterBlock *poBlock =
            poGDS->GetRasterBand(iBand+1)->GetLockedBlockRef(nBlockXOff,nBlockYOff);
        if( poBlock != NULL )
            poBlock->DropLock();
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp PNGRasterBand::GetColorInterpretation()

{
    PNGDataset *poGDS = reinterpret_cast<PNGDataset *>( poDS );

    if( poGDS->nColorType == PNG_COLOR_TYPE_GRAY )
        return GCI_GrayIndex;

    else if( poGDS->nColorType == PNG_COLOR_TYPE_GRAY_ALPHA )
    {
        if( nBand == 1 )
            return GCI_GrayIndex;
        else
            return GCI_AlphaBand;
    }

    else  if( poGDS->nColorType == PNG_COLOR_TYPE_PALETTE )
        return GCI_PaletteIndex;

    else  if( poGDS->nColorType == PNG_COLOR_TYPE_RGB
              || poGDS->nColorType == PNG_COLOR_TYPE_RGB_ALPHA )
    {
        if( nBand == 1 )
            return GCI_RedBand;
        else if( nBand == 2 )
            return GCI_GreenBand;
        else if( nBand == 3 )
            return GCI_BlueBand;
        else
            return GCI_AlphaBand;
    }
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *PNGRasterBand::GetColorTable()

{
    PNGDataset  *poGDS = reinterpret_cast<PNGDataset *>( poDS );

    if( nBand == 1 )
        return poGDS->poColorTable;

    return NULL;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr PNGRasterBand::SetNoDataValue( double dfNewValue )

{
   bHaveNoData = TRUE;
   dfNoDataValue = dfNewValue;

   return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double PNGRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( bHaveNoData )
    {
        if( pbSuccess != NULL )
            *pbSuccess = bHaveNoData;
        return dfNoDataValue;
    }

    return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/* ==================================================================== */
/*                             PNGDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             PNGDataset()                             */
/************************************************************************/

PNGDataset::PNGDataset() :
    fpImage(NULL),
    hPNG(NULL),
    psPNGInfo(NULL),
    nBitDepth(8),
    nColorType(0),
    bInterlaced(FALSE),
    nBufferStartLine(0),
    nBufferLines(0),
    nLastLineRead(-1),
    pabyBuffer(NULL),
    poColorTable(NULL),
    bGeoTransformValid(FALSE),
    bHasReadXMPMetadata(FALSE),
    bHasTriedLoadWorldFile(FALSE),
    bHasReadICCMetadata(FALSE)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    memset(&sSetJmpContext, 0, sizeof(sSetJmpContext));
}

/************************************************************************/
/*                            ~PNGDataset()                             */
/************************************************************************/

PNGDataset::~PNGDataset()

{
    FlushCache();

    if( hPNG != NULL )
        png_destroy_read_struct( &hPNG, &psPNGInfo, NULL );

    if( fpImage )
        VSIFCloseL( fpImage );

    if( poColorTable != NULL )
        delete poColorTable;
}

/************************************************************************/
/*                            IsFullBandMap()                           */
/************************************************************************/

static int IsFullBandMap(int *panBandMap, int nBands)
{
    for(int i=0;i<nBands;i++)
    {
        if( panBandMap[i] != i + 1 )
            return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr PNGDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg )

{
    // Coverity says that we cannot pass a nullptr to IRasterIO.
    if (panBandMap == NULL)
    {
      return CE_Failure;
    }

    if((eRWFlag == GF_Read) &&
       (nBandCount == nBands) &&
       (nXOff == 0) && (nYOff == 0) &&
       (nXSize == nBufXSize) && (nXSize == nRasterXSize) &&
       (nYSize == nBufYSize) && (nYSize == nRasterYSize) &&
       (eBufType == GDT_Byte) &&
       (eBufType == GetRasterBand(1)->GetRasterDataType()) &&
       (pData != NULL) &&
       (panBandMap != NULL) && IsFullBandMap(panBandMap, nBands))
    {
        // Pixel interleaved case.
        if( nBandSpace == 1 )
        {
            for(int y = 0; y < nYSize; ++y)
            {
                CPLErr tmpError = LoadScanline(y);
                if(tmpError != CE_None) return tmpError;
                const GByte* pabyScanline = pabyBuffer
                    + (y - nBufferStartLine) * nBands * nXSize;
                if( nPixelSpace == nBandSpace * nBandCount )
                {
                    memcpy(&(reinterpret_cast<GByte*>( pData )[(y*nLineSpace)]),
                           pabyScanline, nBandCount * nXSize);
                }
                else
                {
                    for(int x = 0; x < nXSize; ++x)
                    {
                        memcpy(&(reinterpret_cast<GByte*>(pData)[(y*nLineSpace) + (x*nPixelSpace)]),
                               (const GByte*)&(pabyScanline[x* nBandCount]), nBandCount);
                    }
                }
            }
        }
        else
        {
            for(int y = 0; y < nYSize; ++y)
            {
                CPLErr tmpError = LoadScanline(y);
                if(tmpError != CE_None) return tmpError;
                const GByte* pabyScanline = pabyBuffer
                    + (y - nBufferStartLine) * nBands * nXSize;
                GByte* pabyDest = reinterpret_cast<GByte *>( pData ) +
                                                            y*nLineSpace;
                if( nPixelSpace <= nBands && nBandSpace > nBands )
                {
                    // Cache friendly way for typical band interleaved case.
                    for(int iBand=0;iBand<nBands;iBand++)
                    {
                        GByte* pabyDest2 = pabyDest + iBand * nBandSpace;
                        const GByte* pabyScanline2 = pabyScanline + iBand;
                        GDALCopyWords( pabyScanline2, GDT_Byte, nBands,
                                       pabyDest2, GDT_Byte,
                                       static_cast<int>(nPixelSpace),
                                       nXSize );
                    }
                }
                else
                {
                    // Generic method
                    for(int x = 0; x < nXSize; ++x)
                    {
                        for(int iBand=0;iBand<nBands;iBand++)
                        {
                            pabyDest[(x*nPixelSpace) + iBand * nBandSpace] =
                                pabyScanline[x*nBands+iBand];
                        }
                    }
                }
            }
        }

        return CE_None;
    }

    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nBandCount, panBandMap,
                                     nPixelSpace, nLineSpace, nBandSpace,
                                     psExtraArg);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PNGDataset::GetGeoTransform( double * padfTransform )

{
    LoadWorldFile();

    if( bGeoTransformValid )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we can also flush out local TIFF strip      */
/*      cache if need be.                                               */
/************************************************************************/

void PNGDataset::FlushCache()

{
    GDALPamDataset::FlushCache();

    if( pabyBuffer != NULL )
    {
        CPLFree( pabyBuffer );
        pabyBuffer = NULL;
        nBufferStartLine = 0;
        nBufferLines = 0;
    }
}

/************************************************************************/
/*                              Restart()                               */
/*                                                                      */
/*      Restart reading from the beginning of the file.                 */
/************************************************************************/

void PNGDataset::Restart()

{
    png_destroy_read_struct( &hPNG, &psPNGInfo, NULL );

    hPNG = png_create_read_struct( PNG_LIBPNG_VER_STRING, this, NULL, NULL );

    png_set_error_fn( hPNG, &sSetJmpContext, png_gdal_error, png_gdal_warning );
    if( setjmp( sSetJmpContext ) != 0 )
        return;

    psPNGInfo = png_create_info_struct( hPNG );

    VSIFSeekL( fpImage, 0, SEEK_SET );
    png_set_read_fn( hPNG, fpImage, png_vsi_read_data );
    png_read_info( hPNG, psPNGInfo );

    if( nBitDepth < 8 )
        png_set_packing( hPNG );

    nLastLineRead = -1;
}

/************************************************************************/
/*                        safe_png_read_image()                         */
/************************************************************************/

static bool safe_png_read_image(png_structp hPNG,
                                png_bytep *png_rows,
                                jmp_buf     sSetJmpContext)
{
    if( setjmp( sSetJmpContext ) != 0 )
        return false;
    png_read_image( hPNG, png_rows );
    return true;
}

/************************************************************************/
/*                        LoadInterlacedChunk()                         */
/************************************************************************/

CPLErr PNGDataset::LoadInterlacedChunk( int iLine )

{
    int nPixelOffset;

    if( nBitDepth == 16 )
        nPixelOffset = 2 * GetRasterCount();
    else
        nPixelOffset = 1 * GetRasterCount();

    // What is the biggest chunk we can safely operate on?
    static const int MAX_PNG_CHUNK_BYTES = 100000000;

    int nMaxChunkLines =
        std::max(1, MAX_PNG_CHUNK_BYTES / (nPixelOffset * GetRasterXSize()));

    if( nMaxChunkLines > GetRasterYSize() )
        nMaxChunkLines = GetRasterYSize();

    // Allocate chunk buffer if we don't already have it from a previous
    // request.
    nBufferLines = nMaxChunkLines;
    if( nMaxChunkLines + iLine > GetRasterYSize() )
        nBufferStartLine = GetRasterYSize() - nMaxChunkLines;
    else
        nBufferStartLine = iLine;

    if( pabyBuffer == NULL )
    {
      pabyBuffer = reinterpret_cast<GByte *>(
          VSI_MALLOC_VERBOSE(nPixelOffset*GetRasterXSize()*nMaxChunkLines) );

        if( pabyBuffer == NULL )
        {
            return CE_Failure;
        }
#ifdef notdef
        if( nMaxChunkLines < GetRasterYSize() )
            CPLDebug( "PNG",
                      "Interlaced file being handled in %d line chunks.\n"
                      "Performance is likely to be quite poor.",
                      nMaxChunkLines );
#endif
    }

    // Do we need to restart reading? We do this if we aren't on the first
    // attempt to read the image.
    if( nLastLineRead != -1 )
    {
        Restart();
    }

    // Allocate and populate rows array. We create a row for each row in the
    // image but use our dummy line for rows not in the target window.
    png_bytep dummy_row = reinterpret_cast<png_bytep>(
        CPLMalloc(nPixelOffset*GetRasterXSize()) );
    png_bytep *png_rows
        = reinterpret_cast<png_bytep *>(
            CPLMalloc(sizeof(png_bytep) * GetRasterYSize()) );

    for( int i = 0; i < GetRasterYSize(); i++ )
    {
        if( i >= nBufferStartLine && i < nBufferStartLine + nBufferLines )
            png_rows[i] = pabyBuffer
                + (i-nBufferStartLine) * nPixelOffset * GetRasterXSize();
        else
            png_rows[i] = dummy_row;
    }

    bool bRet = safe_png_read_image( hPNG, png_rows, sSetJmpContext );

    CPLFree( png_rows );
    CPLFree( dummy_row );
    if( !bRet )
        return CE_Failure;

    nLastLineRead = nBufferStartLine + nBufferLines - 1;

    return CE_None;
}

/************************************************************************/
/*                        safe_png_read_rows()                          */
/************************************************************************/

static bool safe_png_read_rows(png_structp hPNG,
                                png_bytep  row,
                                jmp_buf    sSetJmpContext)
{
    if( setjmp( sSetJmpContext ) != 0 )
        return false;
    png_read_rows( hPNG, &row, NULL, 1 );
    return true;
}

/************************************************************************/
/*                            LoadScanline()                            */
/************************************************************************/

CPLErr PNGDataset::LoadScanline( int nLine )

{
    CPLAssert( nLine >= 0 && nLine < GetRasterYSize() );

    if( nLine >= nBufferStartLine && nLine < nBufferStartLine + nBufferLines)
        return CE_None;

    int nPixelOffset;
    if( nBitDepth == 16 )
        nPixelOffset = 2 * GetRasterCount();
    else
        nPixelOffset = 1 * GetRasterCount();

    // If the file is interlaced, we load the entire image into memory using the
    // high-level API.
    if( bInterlaced )
        return LoadInterlacedChunk( nLine );

    // Ensure we have space allocated for one scanline.
    if( pabyBuffer == NULL )
        pabyBuffer = reinterpret_cast<GByte *>(
            CPLMalloc(nPixelOffset * GetRasterXSize() ) );

    // Otherwise we just try to read the requested row. Do we need to rewind and
    // start over?
    if( nLine <= nLastLineRead )
    {
        Restart();
    }

    // Read till we get the desired row.
    png_bytep row = pabyBuffer;
    while( nLine > nLastLineRead )
    {
        if( !safe_png_read_rows( hPNG, row, sSetJmpContext ) )
            return CE_Failure;
        nLastLineRead++;
    }

    nBufferStartLine = nLine;
    nBufferLines = 1;

     // Do swap on LSB machines. 16-bit PNG data is stored in MSB format.
#ifdef CPL_LSB
    if( nBitDepth == 16 )
        GDALSwapWords( row, 2, GetRasterXSize() * GetRasterCount(), 2 );
#endif

    return CE_None;
}

/************************************************************************/
/*                          CollectMetadata()                           */
/*                                                                      */
/*      We normally do this after reading up to the image, but be       */
/*      forewarned: we can miss text chunks this way.                   */
/*                                                                      */
/*      We turn each PNG text chunk into one metadata item.  It         */
/*      might be nice to preserve language information though we        */
/*      don't try to now.                                               */
/************************************************************************/

void PNGDataset::CollectMetadata()

{
    if( nBitDepth < 8 )
    {
        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            GetRasterBand(iBand+1)->SetMetadataItem(
                "NBITS", CPLString().Printf( "%d", nBitDepth ),
                "IMAGE_STRUCTURE" );
        }
    }

    int nTextCount;
    png_textp text_ptr;
    if( png_get_text( hPNG, psPNGInfo, &text_ptr, &nTextCount ) == 0 )
        return;

    for( int iText = 0; iText < nTextCount; iText++ )
    {
        char *pszTag = CPLStrdup(text_ptr[iText].key);

        for( int i = 0; pszTag[i] != '\0'; i++ )
        {
            if( pszTag[i] == ' ' || pszTag[i] == '=' || pszTag[i] == ':' )
                pszTag[i] = '_';
        }

        GDALDataset::SetMetadataItem( pszTag, text_ptr[iText].text );
        CPLFree( pszTag );
    }
}

/************************************************************************/
/*                       CollectXMPMetadata()                           */
/************************************************************************/

// See §2.1.5 of
// http://wwwimages.adobe.com/www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart3.pdf.

void PNGDataset::CollectXMPMetadata()

{
    if (fpImage == NULL || bHasReadXMPMetadata)
        return;

    // Save current position to avoid disturbing PNG stream decoding.
    const vsi_l_offset nCurOffset = VSIFTellL(fpImage);

    vsi_l_offset nOffset = 8;
    VSIFSeekL( fpImage, nOffset, SEEK_SET );

    // Loop over chunks.
    while( true )
    {
        int nLength;

        if (VSIFReadL( &nLength, 4, 1, fpImage ) != 1)
            break;
        nOffset += 4;
        CPL_MSBPTR32(&nLength);
        if (nLength <= 0)
            break;

        char pszChunkType[5];
        if (VSIFReadL( pszChunkType, 4, 1, fpImage ) != 1)
            break;
        nOffset += 4;
        pszChunkType[4] = 0;

        if (strcmp(pszChunkType, "iTXt") == 0 && nLength > 22)
        {
            char* pszContent = reinterpret_cast<char *>(
                VSIMalloc(nLength + 1) );
            if (pszContent == NULL)
                break;
            if (VSIFReadL( pszContent, nLength, 1, fpImage) != 1)
            {
                VSIFree(pszContent);
                break;
            }
            nOffset += nLength;
            pszContent[nLength] = '\0';
            if (memcmp(pszContent, "XML:com.adobe.xmp\0\0\0\0\0", 22) == 0)
            {
                // Avoid setting the PAM dirty bit just for that.
                int nOldPamFlags = nPamFlags;

                char *apszMDList[2] = { pszContent + 22, NULL };
                SetMetadata(apszMDList, "xml:XMP");

                nPamFlags = nOldPamFlags;

                VSIFree(pszContent);

                break;
            }
            else
            {
                VSIFree(pszContent);
            }
        }
        else
        {
            nOffset += nLength;
            VSIFSeekL( fpImage, nOffset, SEEK_SET );
        }

        nOffset += 4;
        int nCRC;
        if (VSIFReadL( &nCRC, 4, 1, fpImage ) != 1)
            break;
    }

    VSIFSeekL( fpImage, nCurOffset, SEEK_SET );

    bHasReadXMPMetadata = TRUE;
}

/************************************************************************/
/*                           LoadICCProfile()                           */
/************************************************************************/

void PNGDataset::LoadICCProfile()
{
    if (hPNG == NULL || bHasReadICCMetadata)
        return;
    bHasReadICCMetadata = TRUE;

    png_charp pszProfileName;
    png_uint_32 nProfileLength;
#if (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR > 4) || PNG_LIBPNG_VER_MAJOR > 1
    png_bytep pProfileData;
#else
    png_charp pProfileData;
#endif
    int nCompressionType;

    // Avoid setting the PAM dirty bit just for that.
    int nOldPamFlags = nPamFlags;

    if (png_get_iCCP(hPNG, psPNGInfo, &pszProfileName,
       &nCompressionType, &pProfileData, &nProfileLength) != 0)
    {
        // Escape the profile.
        char *pszBase64Profile = CPLBase64Encode(
            static_cast<int>(nProfileLength), reinterpret_cast<const GByte *>( pProfileData ) );

        // Set ICC profile metadata.
        SetMetadataItem( "SOURCE_ICC_PROFILE", pszBase64Profile, "COLOR_PROFILE" );
        SetMetadataItem( "SOURCE_ICC_PROFILE_NAME", pszProfileName, "COLOR_PROFILE" );

        nPamFlags = nOldPamFlags;

        CPLFree(pszBase64Profile);

        return;
    }

    int nsRGBIntent;
    if (png_get_sRGB(hPNG, psPNGInfo, &nsRGBIntent) != 0)
    {
        SetMetadataItem( "SOURCE_ICC_PROFILE_NAME", "sRGB", "COLOR_PROFILE" );

        nPamFlags = nOldPamFlags;

        return;
    }

    double dfGamma;
    bool bGammaAvailable = false;
    if (png_get_valid(hPNG, psPNGInfo, PNG_INFO_gAMA))
    {
        bGammaAvailable = true;

        png_get_gAMA(hPNG,psPNGInfo, &dfGamma);

        SetMetadataItem( "PNG_GAMMA",
            CPLString().Printf( "%.9f", dfGamma ) , "COLOR_PROFILE" );
    }

    // Check that both cHRM and gAMA are available.
    if (bGammaAvailable && png_get_valid(hPNG, psPNGInfo, PNG_INFO_cHRM))
    {
        double dfaWhitepoint[2];
        double dfaCHR[6];

        png_get_cHRM(hPNG, psPNGInfo,
                    &dfaWhitepoint[0], &dfaWhitepoint[1],
                    &dfaCHR[0], &dfaCHR[1],
                    &dfaCHR[2], &dfaCHR[3],
                    &dfaCHR[4], &dfaCHR[5]);

        // Set all the colorimetric metadata.
        SetMetadataItem( "SOURCE_PRIMARIES_RED",
            CPLString().Printf( "%.9f, %.9f, 1.0", dfaCHR[0], dfaCHR[1] ) , "COLOR_PROFILE" );
        SetMetadataItem( "SOURCE_PRIMARIES_GREEN",
            CPLString().Printf( "%.9f, %.9f, 1.0", dfaCHR[2], dfaCHR[3] ) , "COLOR_PROFILE" );
        SetMetadataItem( "SOURCE_PRIMARIES_BLUE",
            CPLString().Printf( "%.9f, %.9f, 1.0", dfaCHR[4], dfaCHR[5] ) , "COLOR_PROFILE" );

        SetMetadataItem( "SOURCE_WHITEPOINT",
            CPLString().Printf( "%.9f, %.9f, 1.0", dfaWhitepoint[0], dfaWhitepoint[1] ) , "COLOR_PROFILE" );
    }

    nPamFlags = nOldPamFlags;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **PNGDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:XMP", "COLOR_PROFILE", NULL);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char  **PNGDataset::GetMetadata( const char * pszDomain )
{
    if (fpImage == NULL)
        return NULL;
    if (eAccess == GA_ReadOnly && !bHasReadXMPMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "xml:XMP"))
        CollectXMPMetadata();
    if (eAccess == GA_ReadOnly && !bHasReadICCMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "COLOR_PROFILE"))
        LoadICCProfile();
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                       GetMetadataItem()                              */
/************************************************************************/
const char *PNGDataset::GetMetadataItem( const char * pszName,
                                         const char * pszDomain )
{
    if (eAccess == GA_ReadOnly && !bHasReadICCMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "COLOR_PROFILE"))
        LoadICCProfile();
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int PNGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->fpL == NULL || poOpenInfo->nHeaderBytes < 4 )
        return FALSE;

    if( png_sig_cmp(poOpenInfo->pabyHeader, static_cast<png_size_t>( 0 ),
                    poOpenInfo->nHeaderBytes) != 0 )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PNGDataset::Open( GDALOpenInfo * poOpenInfo )

{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if( !Identify( poOpenInfo ) )
        return NULL;
#else
    if( poOpenInfo->fpL == NULL )
        return NULL;
#endif

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The PNG driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

    // Create a corresponding GDALDataset.
    PNGDataset *poDS = new PNGDataset();
    return OpenStage2( poOpenInfo, poDS );
}

GDALDataset *PNGDataset::OpenStage2( GDALOpenInfo * poOpenInfo, PNGDataset*& poDS )

{
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;
    poDS->eAccess = poOpenInfo->eAccess;

    poDS->hPNG = png_create_read_struct( PNG_LIBPNG_VER_STRING, poDS,
                                         NULL, NULL );
    if (poDS->hPNG == NULL)
    {
#if (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 2) || PNG_LIBPNG_VER_MAJOR > 1
        int version = static_cast<int>(png_access_version_number());
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The PNG driver failed to access libpng with version '%s',"
                  " library is actually version '%d'.\n",
                  PNG_LIBPNG_VER_STRING, version);
#else
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The PNG driver failed to in png_create_read_struct().\n"
                  "This may be due to version compatibility problems." );
#endif
        delete poDS;
        return NULL;
    }

    poDS->psPNGInfo = png_create_info_struct( poDS->hPNG );

    // Set up error handling.
    png_set_error_fn( poDS->hPNG, &poDS->sSetJmpContext, png_gdal_error, png_gdal_warning );

    if( setjmp( poDS->sSetJmpContext ) != 0 )
    {
        delete poDS;
        return NULL;
    }

    // Read pre-image data after ensuring the file is rewound.
    // We should likely do a setjmp() here.

    png_set_read_fn( poDS->hPNG, poDS->fpImage, png_vsi_read_data );
    png_read_info( poDS->hPNG, poDS->psPNGInfo );

    // Capture some information from the file that is of interest.
    poDS->nRasterXSize = static_cast<int>(png_get_image_width( poDS->hPNG, poDS->psPNGInfo));
    poDS->nRasterYSize = static_cast<int>(png_get_image_height( poDS->hPNG,poDS->psPNGInfo));

    poDS->nBands = png_get_channels( poDS->hPNG, poDS->psPNGInfo );
    poDS->nBitDepth = png_get_bit_depth( poDS->hPNG, poDS->psPNGInfo );
    poDS->bInterlaced = png_get_interlace_type( poDS->hPNG, poDS->psPNGInfo )
        != PNG_INTERLACE_NONE;

    poDS->nColorType = png_get_color_type( poDS->hPNG, poDS->psPNGInfo );

    if( poDS->nColorType == PNG_COLOR_TYPE_PALETTE
        && poDS->nBands > 1 )
    {
        CPLDebug( "GDAL", "PNG Driver got %d from png_get_channels(),\n"
                  "but this kind of image (paletted) can only have one band.\n"
                  "Correcting and continuing, but this may indicate a bug!",
                  poDS->nBands );
        poDS->nBands = 1;
    }

    // We want to treat 1-, 2-, and 4-bit images as eight bit. This call causes
    // libpng to unpack the image.
    if( poDS->nBitDepth < 8 )
        png_set_packing( poDS->hPNG );

    // Create band information objects.
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
        poDS->SetBand( iBand+1, new PNGRasterBand( poDS, iBand+1 ) );

    // Is there a palette?  Note: we should also read back and apply
    // transparency values if available.
    if( poDS->nColorType == PNG_COLOR_TYPE_PALETTE )
    {
        png_color *pasPNGPalette = NULL;
        int nColorCount = 0;

        if( png_get_PLTE( poDS->hPNG, poDS->psPNGInfo,
                          &pasPNGPalette, &nColorCount ) == 0 )
            nColorCount = 0;

        unsigned char *trans = NULL;
        png_color_16 *trans_values = NULL;
        int num_trans = 0;
        png_get_tRNS( poDS->hPNG, poDS->psPNGInfo,
                      &trans, &num_trans, &trans_values );

        poDS->poColorTable = new GDALColorTable();

        GDALColorEntry oEntry;
        int nNoDataIndex = -1;
        for( int iColor = nColorCount - 1; iColor >= 0; iColor-- )
        {
            oEntry.c1 = pasPNGPalette[iColor].red;
            oEntry.c2 = pasPNGPalette[iColor].green;
            oEntry.c3 = pasPNGPalette[iColor].blue;

            if( iColor < num_trans )
            {
                oEntry.c4 = trans[iColor];
                if( oEntry.c4 == 0 )
                {
                    if( nNoDataIndex == -1 )
                        nNoDataIndex = iColor;
                    else
                        nNoDataIndex = -2;
                }
            }
            else
                oEntry.c4 = 255;

            poDS->poColorTable->SetColorEntry( iColor, &oEntry );
        }

        // Special hack to use an index as the no data value, as long as it is
        // the only transparent color in the palette.
        if( nNoDataIndex > -1 )
        {
            poDS->GetRasterBand(1)->SetNoDataValue(nNoDataIndex);
        }
    }

    // Check for transparency values in greyscale images.
    if( poDS->nColorType == PNG_COLOR_TYPE_GRAY )
    {
        png_color_16 *trans_values = NULL;
        unsigned char *trans;
        int num_trans;

        if( png_get_tRNS( poDS->hPNG, poDS->psPNGInfo,
                          &trans, &num_trans, &trans_values ) != 0
            && trans_values != NULL )
        {
            poDS->GetRasterBand(1)->SetNoDataValue(trans_values->gray);
        }
    }

    // Check for nodata color for RGB images.
    if( poDS->nColorType == PNG_COLOR_TYPE_RGB )
    {
        png_color_16 *trans_values = NULL;
        unsigned char *trans;
        int num_trans;

        if( png_get_tRNS( poDS->hPNG, poDS->psPNGInfo,
                          &trans, &num_trans, &trans_values ) != 0
            && trans_values != NULL )
        {
            CPLString oNDValue;

            oNDValue.Printf( "%d %d %d",
                    trans_values->red,
                    trans_values->green,
                    trans_values->blue );
            poDS->SetMetadataItem( "NODATA_VALUES", oNDValue.c_str() );

            poDS->GetRasterBand(1)->SetNoDataValue(trans_values->red);
            poDS->GetRasterBand(2)->SetNoDataValue(trans_values->green);
            poDS->GetRasterBand(3)->SetNoDataValue(trans_values->blue);
        }
    }

    // Extract any text chunks as "metadata."
    poDS->CollectMetadata();

    // More metadata.
    if( poDS->nBands > 1 )
    {
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );
    }

    // Initialize any PAM information.
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML( poOpenInfo->GetSiblingFiles() );

    // Open overviews.
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename,
                                 poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                        LoadWorldFile()                               */
/************************************************************************/

void PNGDataset::LoadWorldFile()
{
    if (bHasTriedLoadWorldFile)
        return;
    bHasTriedLoadWorldFile = TRUE;

    char* pszWldFilename = NULL;
    bGeoTransformValid =
        GDALReadWorldFile2( GetDescription(), NULL,
                            adfGeoTransform, oOvManager.GetSiblingFiles(),
                            &pszWldFilename);

    if( !bGeoTransformValid )
        bGeoTransformValid =
            GDALReadWorldFile2( GetDescription(), ".wld",
                                adfGeoTransform, oOvManager.GetSiblingFiles(),
                                &pszWldFilename);

    if (pszWldFilename)
    {
        osWldFilename = pszWldFilename;
        CPLFree(pszWldFilename);
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **PNGDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    LoadWorldFile();

    if (!osWldFilename.empty() &&
        CSLFindString(papszFileList, osWldFilename) == -1)
    {
        papszFileList = CSLAddString( papszFileList, osWldFilename );
    }

    return papszFileList;
}

/************************************************************************/
/*                          WriteMetadataAsText()                       */
/************************************************************************/

#if defined(PNG_iTXt_SUPPORTED) || ((PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 4) || PNG_LIBPNG_VER_MAJOR > 1)
#define HAVE_ITXT_SUPPORT
#endif

#ifdef HAVE_ITXT_SUPPORT
static bool IsASCII(const char* pszStr)
{
    for(int i=0;pszStr[i]!='\0';i++)
    {
        if( reinterpret_cast<GByte *>(
            const_cast<char *>( pszStr ) )[i] >= 128 )
            return false;
    }
    return true;
}
#endif

void PNGDataset::WriteMetadataAsText(png_structp hPNG, png_infop psPNGInfo,
                                     const char* pszKey, const char* pszValue)
{
    png_text sText;
    memset(&sText, 0, sizeof(png_text));
    sText.compression = PNG_TEXT_COMPRESSION_NONE;
    sText.key = (png_charp) pszKey;
    sText.text = (png_charp) pszValue;
#ifdef HAVE_ITXT_SUPPORT
    // UTF-8 values should be written in iTXt, whereas TEXT should be LATIN-1.
    if( !IsASCII(pszValue) && CPLIsUTF8(pszValue, -1) )
        sText.compression = PNG_ITXT_COMPRESSION_NONE;
#endif
    png_set_text(hPNG, psPNGInfo, &sText, 1);
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
PNGDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
               int bStrict, char ** papszOptions,
               GDALProgressFunc pfnProgress, void * pProgressData )

{
    // Perform some rudimentary checks.
    const int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 && nBands != 2 && nBands != 3 && nBands != 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "PNG driver doesn't support %d bands.  Must be 1 (grey),\n"
                  "2 (grey+alpha), 3 (rgb) or 4 (rgba) bands.\n",
                  nBands );

        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte
        && poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt16 )
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "PNG driver doesn't support data type %s. "
                  "Only eight bit (Byte) and sixteen bit (UInt16) bands supported. %s\n",
                  GDALGetDataTypeName(
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()),
                  (bStrict) ? "" : "Defaulting to Byte" );

        if (bStrict)
            return NULL;
    }

    // Create the dataset.
    VSILFILE *fpImage = VSIFOpenL( pszFilename, "wb" );
    if( fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create png file %s.\n",
                  pszFilename );
        return NULL;
    }

    // Initialize PNG access to the file.
    jmp_buf     sSetJmpContext;

    png_structp hPNG = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, &sSetJmpContext, png_gdal_error, png_gdal_warning );
    png_infop  psPNGInfo = png_create_info_struct( hPNG );

    if( setjmp( sSetJmpContext ) != 0 )
    {
        VSIFCloseL( fpImage );
        png_destroy_write_struct( &hPNG, &psPNGInfo );
        return NULL;
    }

    // Set up some parameters.
    int  nColorType=0;

    if( nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorTable() == NULL )
        nColorType = PNG_COLOR_TYPE_GRAY;
    else if( nBands == 1 )
        nColorType = PNG_COLOR_TYPE_PALETTE;
    else if( nBands == 2 )
        nColorType = PNG_COLOR_TYPE_GRAY_ALPHA;
    else if( nBands == 3 )
        nColorType = PNG_COLOR_TYPE_RGB;
    else if( nBands == 4 )
        nColorType = PNG_COLOR_TYPE_RGB_ALPHA;

    int nBitDepth;
    GDALDataType eType;
    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt16 )
    {
        eType = GDT_Byte;
        nBitDepth = 8;
        if( nBands == 1 )
        {
            const char* pszNbits = poSrcDS->GetRasterBand(1)->GetMetadataItem(
                                                    "NBITS", "IMAGE_STRUCTURE");
            if( pszNbits != NULL )
            {
                nBitDepth = atoi(pszNbits);
                if( !(nBitDepth == 1 || nBitDepth == 2 || nBitDepth == 4) )
                    nBitDepth = 8;
            }
        }
    }
    else
    {
        eType = GDT_UInt16;
        nBitDepth = 16;
    }

    const char* pszNbits = CSLFetchNameValue(papszOptions, "NBITS");
    if( eType == GDT_Byte && pszNbits != NULL )
    {
        nBitDepth = atoi(pszNbits);
        if( !(nBitDepth == 1 || nBitDepth == 2 || nBitDepth == 4 || nBitDepth == 8) )
        {
            CPLError(CE_Warning, CPLE_NotSupported, "Invalid bit depth. Using 8");
            nBitDepth = 8;
        }
    }

    png_set_write_fn( hPNG, fpImage, png_vsi_write_data, png_vsi_flush );

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    png_set_IHDR( hPNG, psPNGInfo, nXSize, nYSize,
                  nBitDepth, nColorType, PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

    // Do we want to control the compression level?
    const char *pszLevel = CSLFetchNameValue( papszOptions, "ZLEVEL" );

    if( pszLevel )
    {
        const int nLevel = atoi(pszLevel);
        if( nLevel < 1 || nLevel > 9 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal ZLEVEL value '%s', should be 1-9.",
                      pszLevel );
            return NULL;
        }

        png_set_compression_level( hPNG, nLevel );
    }

    // Try to handle nodata values as a tRNS block (note that for paletted
    // images, we save the effect to apply as part of palette).
    png_color_16 sTRNSColor;

    // Gray nodata.
    if( nColorType == PNG_COLOR_TYPE_GRAY )
    {
       int bHaveNoData = FALSE;
       const double dfNoDataValue
           = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bHaveNoData );

       if ( bHaveNoData && dfNoDataValue >= 0 && dfNoDataValue < 65536 )
       {
          sTRNSColor.gray = (png_uint_16) dfNoDataValue;
          png_set_tRNS( hPNG, psPNGInfo, NULL, 0, &sTRNSColor );
       }
    }

    // RGB nodata.
    if( nColorType == PNG_COLOR_TYPE_RGB )
    {
       // First try to use the NODATA_VALUES metadata item.
       if ( poSrcDS->GetMetadataItem( "NODATA_VALUES" ) != NULL )
       {
           char **papszValues = CSLTokenizeString(
               poSrcDS->GetMetadataItem( "NODATA_VALUES" ) );

           if( CSLCount(papszValues) >= 3 )
           {
               sTRNSColor.red   = (png_uint_16) atoi(papszValues[0]);
               sTRNSColor.green = (png_uint_16) atoi(papszValues[1]);
               sTRNSColor.blue  = (png_uint_16) atoi(papszValues[2]);
               png_set_tRNS( hPNG, psPNGInfo, NULL, 0, &sTRNSColor );
           }

           CSLDestroy( papszValues );
       }
       // Otherwise, get the nodata value from the bands.
       else
       {
          int bHaveNoDataRed = FALSE;
          const double dfNoDataValueRed
              = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bHaveNoDataRed );

          int bHaveNoDataGreen = FALSE;
          const double dfNoDataValueGreen
              = poSrcDS->GetRasterBand(2)->GetNoDataValue( &bHaveNoDataGreen );

          int bHaveNoDataBlue = FALSE;
          const double dfNoDataValueBlue
              = poSrcDS->GetRasterBand(3)->GetNoDataValue( &bHaveNoDataBlue );

          if ( ( bHaveNoDataRed && dfNoDataValueRed >= 0 && dfNoDataValueRed < 65536 ) &&
               ( bHaveNoDataGreen && dfNoDataValueGreen >= 0 && dfNoDataValueGreen < 65536 ) &&
               ( bHaveNoDataBlue && dfNoDataValueBlue >= 0 && dfNoDataValueBlue < 65536 ) )
          {
             sTRNSColor.red   = static_cast<png_uint_16>( dfNoDataValueRed );
             sTRNSColor.green = static_cast<png_uint_16>( dfNoDataValueGreen );
             sTRNSColor.blue  = static_cast<png_uint_16>( dfNoDataValueBlue );
             png_set_tRNS( hPNG, psPNGInfo, NULL, 0, &sTRNSColor );
          }
       }
    }

    // Copy color profile data.
    const char *pszICCProfile = CSLFetchNameValue(papszOptions, "SOURCE_ICC_PROFILE");
    const char *pszICCProfileName = CSLFetchNameValue(papszOptions, "SOURCE_ICC_PROFILE_NAME");
    if (pszICCProfileName == NULL)
        pszICCProfileName = poSrcDS->GetMetadataItem( "SOURCE_ICC_PROFILE_NAME", "COLOR_PROFILE" );

    if (pszICCProfile == NULL)
        pszICCProfile = poSrcDS->GetMetadataItem( "SOURCE_ICC_PROFILE", "COLOR_PROFILE" );

    if ((pszICCProfileName != NULL) && EQUAL(pszICCProfileName, "sRGB"))
    {
        pszICCProfile = NULL;

        png_set_sRGB(hPNG, psPNGInfo, PNG_sRGB_INTENT_PERCEPTUAL);
    }

    if (pszICCProfile != NULL)
    {
        char *pEmbedBuffer = CPLStrdup(pszICCProfile);
        png_uint_32 nEmbedLen
            = CPLBase64DecodeInPlace(reinterpret_cast<GByte *>( pEmbedBuffer ) );
        const char* pszLocalICCProfileName = (pszICCProfileName!=NULL)?pszICCProfileName:"ICC Profile";

        png_set_iCCP(hPNG, psPNGInfo,
#if (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR > 4) || PNG_LIBPNG_VER_MAJOR > 1
            pszLocalICCProfileName,
#else
            (png_charp)pszLocalICCProfileName,
#endif
            0,
#if (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR > 4) || PNG_LIBPNG_VER_MAJOR > 1
            (png_const_bytep)pEmbedBuffer,
#else
            (png_charp)pEmbedBuffer,
#endif
            nEmbedLen);

        CPLFree(pEmbedBuffer);
    }
    else if ((pszICCProfileName == NULL) || !EQUAL(pszICCProfileName, "sRGB"))
    {
        // Output gamma, primaries and whitepoint.
        const char *pszGamma = CSLFetchNameValue(papszOptions, "PNG_GAMMA");
        if (pszGamma == NULL)
            pszGamma = poSrcDS->GetMetadataItem( "PNG_GAMMA", "COLOR_PROFILE" );

        if (pszGamma != NULL)
        {
            double dfGamma = CPLAtof(pszGamma);
            png_set_gAMA(hPNG, psPNGInfo, dfGamma);
        }

        const char *pszPrimariesRed = CSLFetchNameValue(papszOptions, "SOURCE_PRIMARIES_RED");
        if (pszPrimariesRed == NULL)
            pszPrimariesRed = poSrcDS->GetMetadataItem( "SOURCE_PRIMARIES_RED", "COLOR_PROFILE" );
        const char *pszPrimariesGreen = CSLFetchNameValue(papszOptions, "SOURCE_PRIMARIES_GREEN");
        if (pszPrimariesGreen == NULL)
            pszPrimariesGreen = poSrcDS->GetMetadataItem( "SOURCE_PRIMARIES_GREEN", "COLOR_PROFILE" );
        const char *pszPrimariesBlue = CSLFetchNameValue(papszOptions, "SOURCE_PRIMARIES_BLUE");
        if (pszPrimariesBlue == NULL)
            pszPrimariesBlue = poSrcDS->GetMetadataItem( "SOURCE_PRIMARIES_BLUE", "COLOR_PROFILE" );
        const char *pszWhitepoint = CSLFetchNameValue(papszOptions, "SOURCE_WHITEPOINT");
        if (pszWhitepoint == NULL)
            pszWhitepoint = poSrcDS->GetMetadataItem( "SOURCE_WHITEPOINT", "COLOR_PROFILE" );

        if ((pszPrimariesRed != NULL) && (pszPrimariesGreen != NULL) && (pszPrimariesBlue != NULL) &&
            (pszWhitepoint != NULL))
        {
            bool bOk = true;
            double faColour[8] = { 0.0 };
            char** apapszTokenList[4] = { NULL };

            apapszTokenList[0] = CSLTokenizeString2( pszWhitepoint, ",",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
            apapszTokenList[1] = CSLTokenizeString2( pszPrimariesRed, ",",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
            apapszTokenList[2] = CSLTokenizeString2( pszPrimariesGreen, ",",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
            apapszTokenList[3] = CSLTokenizeString2( pszPrimariesBlue, ",",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

            if ((CSLCount( apapszTokenList[0] ) == 3) &&
                (CSLCount( apapszTokenList[1] ) == 3) &&
                (CSLCount( apapszTokenList[2] ) == 3) &&
                (CSLCount( apapszTokenList[3] ) == 3))
            {
                for( int i = 0; i < 4; i++ )
                {
                    for( int j = 0; j < 3; j++ )
                    {
                        const double v = CPLAtof(apapszTokenList[i][j]);

                        if (j == 2)
                        {
                            /* Last term of xyY colour must be 1.0 */
                            if (v != 1.0)
                            {
                                bOk = false;
                                break;
                            }
                        }
                        else
                        {
                            faColour[i*2 + j] = v;
                        }
                    }
                    if (!bOk)
                        break;
                }

                if (bOk)
                {
                    png_set_cHRM(hPNG, psPNGInfo,
                        faColour[0], faColour[1],
                        faColour[2], faColour[3],
                        faColour[4], faColour[5],
                        faColour[6], faColour[7]);
                }
            }

            CSLDestroy( apapszTokenList[0] );
            CSLDestroy( apapszTokenList[1] );
            CSLDestroy( apapszTokenList[2] );
            CSLDestroy( apapszTokenList[3] );
        }
    }

    // Write the palette if there is one. Technically, it may be possible to
    // write 16-bit palettes for PNG, but for now, this is omitted.
    if( nColorType == PNG_COLOR_TYPE_PALETTE )
    {
        int bHaveNoData = FALSE;
        double dfNoDataValue
            = poSrcDS->GetRasterBand(1)->GetNoDataValue( &bHaveNoData );

        GDALColorTable *poCT = poSrcDS->GetRasterBand(1)->GetColorTable();

        int nEntryCount = poCT->GetColorEntryCount();
        int nMaxEntryCount = 1 << nBitDepth;
        if( nEntryCount > nMaxEntryCount )
            nEntryCount = nMaxEntryCount;

        png_color *pasPNGColors = reinterpret_cast<png_color *>(
            CPLMalloc( sizeof(png_color) * nEntryCount ) );

        GDALColorEntry sEntry;
        bool bFoundTrans = false;
        for( int iColor = 0; iColor < nEntryCount; iColor++ )
        {
            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            if( sEntry.c4 != 255 )
                bFoundTrans = true;

            pasPNGColors[iColor].red = static_cast<png_byte>( sEntry.c1 );
            pasPNGColors[iColor].green = static_cast<png_byte>( sEntry.c2 );
            pasPNGColors[iColor].blue = static_cast<png_byte>( sEntry.c3 );
        }

        png_set_PLTE( hPNG, psPNGInfo, pasPNGColors,
                      nEntryCount );

        CPLFree( pasPNGColors );

        // If we have transparent elements in the palette, we need to write a
        // transparency block.
        if( bFoundTrans || bHaveNoData )
        {
            unsigned char *pabyAlpha
                = reinterpret_cast<unsigned char *>(
                    CPLMalloc(nEntryCount) );

            for( int iColor = 0; iColor < nEntryCount; iColor++ )
            {
                poCT->GetColorEntryAsRGB( iColor, &sEntry );
                pabyAlpha[iColor] = static_cast<unsigned char>( sEntry.c4 );

                if( bHaveNoData && iColor == static_cast<int>( dfNoDataValue ) )
                    pabyAlpha[iColor] = 0;
            }

            png_set_tRNS( hPNG, psPNGInfo, pabyAlpha,
                          nEntryCount, NULL );

            CPLFree( pabyAlpha );
        }
    }

    // Add text info.
    // These are predefined keywords. See "4.2.7 tEXt Textual data" of
    // http://www.w3.org/TR/PNG-Chunks.html for more information.
    const char* apszKeywords[] = { "Title", "Author", "Description", "Copyright",
                                   "Creation Time", "Software", "Disclaimer",
                                   "Warning", "Source", "Comment", NULL };
    const bool bWriteMetadataAsText = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_METADATA_AS_TEXT", "FALSE"));
    for(int i=0;apszKeywords[i]!=NULL;i++)
    {
        const char* pszKey = apszKeywords[i];
        const char* pszValue = CSLFetchNameValue(papszOptions, pszKey);
        if( pszValue == NULL && bWriteMetadataAsText )
            pszValue = poSrcDS->GetMetadataItem(pszKey);
        if( pszValue != NULL )
        {
            WriteMetadataAsText(hPNG, psPNGInfo, pszKey, pszValue);
        }
    }
    if( bWriteMetadataAsText )
    {
        char** papszSrcMD = poSrcDS->GetMetadata();
        for( ; papszSrcMD && *papszSrcMD; papszSrcMD++ )
        {
            char* pszKey = NULL;
            const char* pszValue = CPLParseNameValue(*papszSrcMD, &pszKey );
            if( pszKey && pszValue )
            {
                if( CSLFindString(const_cast<char**>( apszKeywords ), pszKey) < 0 &&
                    !EQUAL(pszKey, "AREA_OR_POINT") && !EQUAL(pszKey, "NODATA_VALUES") )
                {
                    WriteMetadataAsText(hPNG, psPNGInfo, pszKey, pszValue);
                }
                CPLFree(pszKey);
            }
        }
    }

    // Write the PNG info.
    png_write_info( hPNG, psPNGInfo );

    if( nBitDepth < 8 )
        png_set_packing( hPNG );

    // Loop over the image, copying image data.
    CPLErr      eErr = CE_None;
    const int nWordSize = GDALGetDataTypeSize(eType) / 8;

    GByte *pabyScanline = reinterpret_cast<GByte *>(
        CPLMalloc( nBands * nXSize * nWordSize ) );

    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        png_bytep       row = pabyScanline;

        eErr = poSrcDS->RasterIO( GF_Read, 0, iLine, nXSize, 1,
                                  pabyScanline,
                                  nXSize, 1, eType,
                                  nBands, NULL,
                                  nBands * nWordSize,
                                  nBands * nXSize * nWordSize,
                                  nWordSize,
                                  NULL );

#ifdef CPL_LSB
        if( nBitDepth == 16 )
            GDALSwapWords( row, 2, nXSize * nBands, 2 );
#endif
        if( eErr == CE_None )
            png_write_rows( hPNG, &row, 1 );

        if( eErr == CE_None
            && !pfnProgress( (iLine+1) / static_cast<double>( nYSize ),
                             NULL, pProgressData ) )
        {
            eErr = CE_Failure;
            CPLError( CE_Failure, CPLE_UserInterrupt,
                      "User terminated CreateCopy()" );
        }
    }

    CPLFree( pabyScanline );

    png_write_end( hPNG, psPNGInfo );
    png_destroy_write_struct( &hPNG, &psPNGInfo );

    VSIFCloseL( fpImage );

    if( eErr != CE_None )
        return NULL;

    // Do we need a world file?
    if( CPLFetchBool( papszOptions, "WORLDFILE", false ) )
    {
      double adfGeoTransform[6];

      if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
        GDALWriteWorldFile( pszFilename, "wld", adfGeoTransform );
    }

    // Re-open dataset and copy any auxiliary PAM information.

    /* If writing to stdout, we can't reopen it, so return */
    /* a fake dataset to make the caller happy */
    if( CPLTestBool(CPLGetConfigOption("GDAL_OPEN_AFTER_COPY", "YES")) )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
        PNGDataset *poDS = reinterpret_cast<PNGDataset *>(
            PNGDataset::Open( &oOpenInfo ) );
        CPLPopErrorHandler();
        if( poDS )
        {
            int nFlags = GCIF_PAM_DEFAULT;
            if( bWriteMetadataAsText )
                nFlags &= ~GCIF_METADATA;
            poDS->CloneInfo( poSrcDS, nFlags );
            return poDS;
        }
        CPLErrorReset();
    }

    PNGDataset* poPNG_DS = new PNGDataset();
    poPNG_DS->nRasterXSize = nXSize;
    poPNG_DS->nRasterYSize = nYSize;
    poPNG_DS->nBitDepth = nBitDepth;
    for(int i=0;i<nBands;i++)
        poPNG_DS->SetBand( i+1, new PNGRasterBand( poPNG_DS, i+1) );
    return poPNG_DS;
}

/************************************************************************/
/*                         png_vsi_read_data()                          */
/*                                                                      */
/*      Read data callback through VSI.                                 */
/************************************************************************/
static void
png_vsi_read_data(png_structp png_ptr, png_bytep data, png_size_t length)

{
    // fread() returns 0 on error, so it is OK to store this in a png_size_t
    // instead of an int, which is what fread() actually returns.
    const png_size_t check
        = static_cast<png_size_t>(
            VSIFReadL(data, (png_size_t)1, length,
                      reinterpret_cast<VSILFILE *>( png_get_io_ptr(png_ptr) ) ) );

    if (check != length)
        png_error(png_ptr, "Read Error");
}

/************************************************************************/
/*                         png_vsi_write_data()                         */
/************************************************************************/

static void
png_vsi_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    const size_t check
        = VSIFWriteL(data, 1, length, reinterpret_cast<VSILFILE *>(
            png_get_io_ptr(png_ptr) ) );

    if (check != length)
      png_error(png_ptr, "Write Error");
}

/************************************************************************/
/*                           png_vsi_flush()                            */
/************************************************************************/
static void png_vsi_flush(png_structp png_ptr)
{
    VSIFFlushL( reinterpret_cast<VSILFILE *>( png_get_io_ptr(png_ptr) ) );
}

/************************************************************************/
/*                           png_gdal_error()                           */
/************************************************************************/

static void png_gdal_error( png_structp png_ptr, const char *error_message )
{
    CPLError( CE_Failure, CPLE_AppDefined,
              "libpng: %s", error_message );

    // Use longjmp instead of a C++ exception, because libpng is generally not
    // built as C++ and so will not honor unwind semantics.

    jmp_buf* psSetJmpContext = reinterpret_cast<jmp_buf *>(
        png_get_error_ptr( png_ptr ) );
    if (psSetJmpContext)
    {
        longjmp( *psSetJmpContext, 1 );
    }
}

/************************************************************************/
/*                          png_gdal_warning()                          */
/************************************************************************/

static void png_gdal_warning( CPL_UNUSED png_structp png_ptr,
                              const char *error_message )
{
    CPLError( CE_Warning, CPLE_AppDefined,
              "libpng: %s", error_message );
}

/************************************************************************/
/*                          GDALRegister_PNG()                          */
/************************************************************************/

void GDALRegister_PNG()

{
    if( GDALGetDriverByName( "PNG" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "PNG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Portable Network Graphics" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_various.html#PNG" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "png" );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/png" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='WORLDFILE' type='boolean' description='Create world file' default='FALSE'/>\n"
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='6'/>\n"
"   <Option name='SOURCE_ICC_PROFILE' type='string' description='ICC Profile'/>\n"
"   <Option name='SOURCE_ICC_PROFILE_NAME' type='string' description='ICC Profile name'/>\n"
"   <Option name='SOURCE_PRIMARIES_RED' type='string' description='x,y,1.0 (xyY) red chromaticity'/>\n"
"   <Option name='SOURCE_PRIMARIES_GREEN' type='string' description='x,y,1.0 (xyY) green chromaticity'/>\n"
"   <Option name='SOURCE_PRIMARIES_BLUE' type='string' description='x,y,1.0 (xyY) blue chromaticity'/>\n"
"   <Option name='SOURCE_WHITEPOINT' type='string' description='x,y,1.0 (xyY) whitepoint'/>\n"
"   <Option name='PNG_GAMMA' type='string' description='Gamma'/>\n"
"   <Option name='TITLE' type='string' description='Title'/>\n"
"   <Option name='DESCRIPTION' type='string' description='Description'/>\n"
"   <Option name='COPYRIGHT' type='string' description='Copyright'/>\n"
"   <Option name='COMMENT' type='string' description='Comment'/>\n"
"   <Option name='WRITE_METADATA_AS_TEXT' type='boolean' description='Whether to write source dataset metadata in TEXT chunks' default='FALSE'/>\n"
"   <Option name='NBITS' type='int' description='Force output bit depth: 1, 2 or 4'/>\n"
"</CreationOptionList>\n" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = PNGDataset::Open;
    poDriver->pfnCreateCopy = PNGDataset::CreateCopy;
    poDriver->pfnIdentify = PNGDataset::Identify;
#ifdef SUPPORT_CREATE
    poDriver->pfnCreate = PNGDataset::Create;
#endif

    GetGDALDriverManager()->RegisterDriver( poDriver );
}

#ifdef SUPPORT_CREATE
/************************************************************************/
/*                         IWriteBlock()                                */
/************************************************************************/

CPLErr PNGRasterBand::IWriteBlock(int x, int y, void* pvData)
{
    PNGDataset& ds = *reinterpret_cast<PNGDataset*>( poDS );

    // Write the block (or consolidate into multichannel block) and then write.

    const GDALDataType dt = GetRasterDataType();
    const size_t wordsize = ds.m_nBitDepth / 8;
    GDALCopyWords( pvData, dt, wordsize,
                   ds.m_pabyBuffer + (nBand-1) * wordsize,
                   dt, ds.nBands * wordsize,
                   nBlockXSize );

    // See if we have all the bands.
    m_bBandProvided[nBand - 1] = TRUE;
    for( size_t i = 0; i < static_cast<size_t>( ds.nBands ); i++ )
    {
        if(!m_bBandProvided[i])
            return CE_None;
    }

    // We received all the bands, so reset band flags and write pixels out.
    this->reset_band_provision_flags();

    // If it's the first block, write out the file header.
    if(x == 0 && y == 0)
    {
        CPLErr err = ds.write_png_header();
        if(err != CE_None)
            return err;
    }

#ifdef CPL_LSB
    if( ds.m_nBitDepth == 16 )
        GDALSwapWords( ds.m_pabyBuffer, 2, nBlockXSize * ds.nBands, 2 );
#endif
    png_write_rows( ds.m_hPNG, &ds.m_pabyBuffer, 1 );

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PNGDataset::SetGeoTransform( double * padfTransform )
{
    memcpy( m_adfGeoTransform, padfTransform, sizeof(double) * 6 );

    if ( m_pszFilename )
    {
        if ( GDALWriteWorldFile( m_pszFilename, "wld", m_adfGeoTransform )
             == FALSE )
        {
            CPLError( CE_Failure, CPLE_FileIO, "Can't write world file." );
            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr PNGRasterBand::SetColorTable(GDALColorTable* poCT)
{
    if( poCT == NULL )
        return CE_Failure;

    // We get called even for grayscale files, since some formats need a palette
    // even then. PNG doesn't, so if a gray palette is given, just ignore it.

    GDALColorEntry sEntry;
    for( size_t i = 0; i < static_cast<size_t>( poCT->GetColorEntryCount() ); i++ )
    {
        poCT->GetColorEntryAsRGB( i, &sEntry );
        if( sEntry.c1 != sEntry.c2 || sEntry.c1 != sEntry.c3)
        {
            CPLErr err = GDALPamRasterBand::SetColorTable(poCT);
            if(err != CE_None)
                return err;

            PNGDataset& ds = *reinterpret_cast<PNGDataset *>( poDS );
            ds.m_nColorType = PNG_COLOR_TYPE_PALETTE;
            break;
            // band::IWriteBlock will emit color table as part of the header
            // preceding the first block write.
        }
    }

    return CE_None;
}

/************************************************************************/
/*                  PNGDataset::write_png_header()                      */
/************************************************************************/

CPLErr PNGDataset::write_png_header()
{
    // Initialize PNG access to the file.
    m_hPNG = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, NULL,
        png_gdal_error, png_gdal_warning );

    m_psPNGInfo = png_create_info_struct( m_hPNG );

    png_set_write_fn( m_hPNG, m_fpImage, png_vsi_write_data, png_vsi_flush );

    png_set_IHDR( m_hPNG, m_psPNGInfo, nRasterXSize, nRasterYSize,
                  m_nBitDepth, m_nColorType, PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );

    png_set_compression_level(m_hPNG, Z_BEST_COMPRESSION);

    // png_set_swap_alpha(m_hPNG); // Use RGBA order, not ARGB.

    // Try to handle nodata values as a tRNS block (note that for paletted
    // images, we save the effect to apply as part of the palette).
    //m_bHaveNoData = FALSE;
    //m_dfNoDataValue = -1;
    png_color_16 sTRNSColor;

    int bHaveNoData = FALSE;
    double dfNoDataValue = -1;

    if( m_nColorType == PNG_COLOR_TYPE_GRAY )
    {
        dfNoDataValue = GetRasterBand(1)->GetNoDataValue( &bHaveNoData );

        if ( bHaveNoData && dfNoDataValue >= 0 && dfNoDataValue < 65536 )
        {
            sTRNSColor.gray = static_cast<png_uint_16>( dfNoDataValue );
            png_set_tRNS( m_hPNG, m_psPNGInfo, NULL, 0, &sTRNSColor );
        }
    }

    // RGB nodata.
    if( nColorType == PNG_COLOR_TYPE_RGB )
    {
        // First, try to use the NODATA_VALUES metadata item.
        if ( GetMetadataItem( "NODATA_VALUES" ) != NULL )
        {
            char **papszValues = CSLTokenizeString(
                GetMetadataItem( "NODATA_VALUES" ) );

            if( CSLCount(papszValues) >= 3 )
            {
                sTRNSColor.red   = static_cast<png_uint_16>( atoi(papszValues[0] ) );
                sTRNSColor.green = static_cast<png_uint_16>( atoi(papszValues[1] ) );
                sTRNSColor.blue  = static_cast<png_uint_16>( atoi(papszValues[2] ) );
                png_set_tRNS( m_hPNG, m_psPNGInfo, NULL, 0, &sTRNSColor );
            }

            CSLDestroy( papszValues );
        }
        // Otherwise, get the nodata value from the bands.
        else
        {
            int bHaveNoDataRed = FALSE;
            const double dfNoDataValueRed
                = GetRasterBand(1)->GetNoDataValue( &bHaveNoDataRed );

            int bHaveNoDataGreen = FALSE;
            const double dfNoDataValueGreen
                = GetRasterBand(2)->GetNoDataValue( &bHaveNoDataGreen );

            int bHaveNoDataBlue = FALSE;
            const double dfNoDataValueBlue
                = GetRasterBand(3)->GetNoDataValue( &bHaveNoDataBlue );

            if ( ( bHaveNoDataRed && dfNoDataValueRed >= 0 && dfNoDataValueRed < 65536 ) &&
                 ( bHaveNoDataGreen && dfNoDataValueGreen >= 0 && dfNoDataValueGreen < 65536 ) &&
                 ( bHaveNoDataBlue && dfNoDataValueBlue >= 0 && dfNoDataValueBlue < 65536 ) )
            {
                sTRNSColor.red   = static_cast<png_uint_16>( dfNoDataValueRed );
                sTRNSColor.green = static_cast<png_uint_16>( dfNoDataValueGreen );
                sTRNSColor.blue  = static_cast<png_uint_16>( dfNoDataValueBlue );
                png_set_tRNS( m_hPNG, m_psPNGInfo, NULL, 0, &sTRNSColor );
            }
        }
    }

    // Write the palette if there is one. Technically, it may be possible
    // to write 16-bit palettes for PNG, but for now, doing so is omitted.
    if( nColorType == PNG_COLOR_TYPE_PALETTE )
    {
        GDALColorTable *poCT = GetRasterBand(1)->GetColorTable();

        int bHaveNoData = FALSE;
        double dfNoDataValue = GetRasterBand(1)->GetNoDataValue( &bHaveNoData );

        m_pasPNGColors = reinterpret_cast<png_color *>(
            CPLMalloc( sizeof(png_color) * poCT->GetColorEntryCount() ) );

        GDALColorEntry sEntry;
        bool bFoundTrans = false;
        for( int iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
        {
            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            if( sEntry.c4 != 255 )
                bFoundTrans = true;

            m_pasPNGColors[iColor].red   = static_cast<png_byte>( sEntry.c1 );
            m_pasPNGColors[iColor].green = static_cast<png_byte>( sEntry.c2 );
            m_pasPNGColors[iColor].blue  = static_cast<png_byte>( sEntry.c3 );
        }

        png_set_PLTE( m_hPNG, m_psPNGInfo, m_pasPNGColors,
                      poCT->GetColorEntryCount() );

        // If we have transparent elements in the palette, we need to write a
        // transparency block.
        if( bFoundTrans || bHaveNoData )
        {
            m_pabyAlpha = reinterpret_cast<unsigned char *>(
                CPLMalloc(poCT->GetColorEntryCount() ) );

            for( int iColor = 0; iColor < poCT->GetColorEntryCount(); iColor++ )
            {
                poCT->GetColorEntryAsRGB( iColor, &sEntry );
                m_pabyAlpha[iColor] = static_cast<unsigned char>( sEntry.c4 );

                if( bHaveNoData && iColor == static_cast<int>( dfNoDataValue ) )
                    m_pabyAlpha[iColor] = 0;
            }

            png_set_tRNS( m_hPNG, m_psPNGInfo, m_pabyAlpha,
                          poCT->GetColorEntryCount(), NULL );
        }
    }

    png_write_info( m_hPNG, m_psPNGInfo );
    return CE_None;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *PNGDataset::Create
(
    const char* pszFilename,
    int nXSize, int nYSize,
    int nBands,
    GDALDataType eType,
    char **papszOptions
)
{
    if( eType != GDT_Byte && eType != GDT_UInt16)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create PNG dataset with an illegal\n"
                  "data type (%s), only Byte and UInt16 supported by the format.\n",
                  GDALGetDataTypeName(eType) );

        return NULL;
    }

    if( nBands < 1 || nBands > 4 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "PNG driver doesn't support %d bands. "
                  "Must be 1 (gray/indexed color),\n"
                  "2 (gray+alpha), 3 (rgb) or 4 (rgba) bands.\n",
                  nBands );

        return NULL;
    }

    // Bands are:
    // 1: Grayscale or indexed color.
    // 2: Gray plus alpha.
    // 3: RGB.
    // 4: RGB plus alpha.

    if(nXSize < 1 || nYSize < 1)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Specified pixel dimensions (% d x %d) are bad.\n",
                  nXSize, nYSize );
    }

    // Set up some parameters.
    PNGDataset* poDS = new PNGDataset();

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->nBands = nBands;

    switch(nBands)
    {
      case 1:
        poDS->m_nColorType = PNG_COLOR_TYPE_GRAY;
        break;  // If a non-gray palette is set, we'll change this.

      case 2:
        poDS->m_nColorType = PNG_COLOR_TYPE_GRAY_ALPHA;
        break;

      case 3:
        poDS->m_nColorType = PNG_COLOR_TYPE_RGB;
        break;

      case 4:
        poDS->m_nColorType = PNG_COLOR_TYPE_RGB_ALPHA;
        break;
    }

    poDS->m_nBitDepth = (eType == GDT_Byte ? 8 : 16);

    poDS->m_pabyBuffer = reinterpret_cast<GByte *>(
        CPLMalloc( nBands * nXSize * poDS->m_nBitDepth / 8 ) );

    // Create band information objects.
    for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
        poDS->SetBand( iBand, new PNGRasterBand( poDS, iBand ) );

    // Do we need a world file?
    if( CPLFetchBool( papszOptions, "WORLDFILE", false ) )
        poDS->m_bGeoTransformValid = TRUE;

    // Create the file.

    poDS->m_fpImage = VSIFOpenL( pszFilename, "wb" );
    if( poDS->m_fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create PNG file %s.\n",
                  pszFilename );
        delete poDS;
        return NULL;
    }

    poDS->m_pszFilename = CPLStrdup(pszFilename);

    return poDS;
}

#endif
