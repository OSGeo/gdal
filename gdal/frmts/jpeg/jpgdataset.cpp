/******************************************************************************
 *
 * Project:  JPEG JFIF Driver
 * Purpose:  Implement GDAL JPEG Support based on IJG libjpeg.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
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

#undef ENABLE_LIBJPEG_NO_RETURN

#include "cpl_port.h"
#include "jpgdataset.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <setjmp.h>

#include <algorithm>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdalexif.h"
CPL_C_START
#ifdef LIBJPEG_12_PATH
#  include LIBJPEG_12_PATH
#else
#  include "jpeglib.h"
#endif
CPL_C_END
#include "memdataset.h"
#include "vsidataio.h"

CPL_CVSID("$Id$")

static const int TIFF_VERSION = 42;

static const int TIFF_BIGENDIAN = 0x4d4d;
static const int TIFF_LITTLEENDIAN = 0x4949;

static const int JPEG_TIFF_IMAGEWIDTH      = 0x100;
static const int JPEG_TIFF_IMAGEHEIGHT     = 0x101;
static const int JPEG_TIFF_COMPRESSION     = 0x103;
static const int JPEG_EXIF_JPEGIFOFSET     = 0x201;
static const int JPEG_EXIF_JPEGIFBYTECOUNT = 0x202;

// Ok to use setjmp().
#ifdef _MSC_VER
#  pragma warning(disable:4611)
#endif



// Do we want to do special processing suitable for when JSAMPLE is a
// 16bit value?
#if defined(JPEG_LIB_MK1)
#  define JPEG_LIB_MK1_OR_12BIT 1
#elif BITS_IN_JSAMPLE == 12
#  define JPEG_LIB_MK1_OR_12BIT 1
#endif


#if !defined(JPGDataset)

/************************************************************************/
/*                       ReadEXIFMetadata()                             */
/************************************************************************/
void JPGDatasetCommon::ReadEXIFMetadata()
{
    if (bHasReadEXIFMetadata)
        return;

    CPLAssert(papszMetadata == NULL);

    // Save current position to avoid disturbing JPEG stream decoding.
    const vsi_l_offset nCurOffset = VSIFTellL(fpImage);

    if(EXIFInit(fpImage))
    {
        EXIFExtractMetadata(papszMetadata,
                            fpImage, nTiffDirStart,
                            bSwabflag, nTIFFHEADER,
                            nExifOffset, nInterOffset, nGPSOffset);

        if(nExifOffset  > 0){
            EXIFExtractMetadata(papszMetadata,
                                fpImage, nExifOffset,
                                bSwabflag, nTIFFHEADER,
                                nExifOffset, nInterOffset, nGPSOffset);
        }
        if(nInterOffset > 0) {
            EXIFExtractMetadata(papszMetadata,
                                fpImage, nInterOffset,
                                bSwabflag, nTIFFHEADER,
                                nExifOffset, nInterOffset, nGPSOffset);
        }
        if(nGPSOffset > 0) {
            EXIFExtractMetadata(papszMetadata,
                                fpImage, nGPSOffset,
                                bSwabflag, nTIFFHEADER,
                                nExifOffset, nInterOffset, nGPSOffset);
        }

        // Avoid setting the PAM dirty bit just for that.
        const int nOldPamFlags = nPamFlags;

        // Append metadata from PAM after EXIF metadata.
        papszMetadata = CSLMerge(papszMetadata, GDALPamDataset::GetMetadata());
        SetMetadata(papszMetadata);

        nPamFlags = nOldPamFlags;
    }

    VSIFSeekL(fpImage, nCurOffset, SEEK_SET);

    bHasReadEXIFMetadata = true;
}

/************************************************************************/
/*                        ReadXMPMetadata()                             */
/************************************************************************/

// See §2.1.3 of
// http://wwwimages.adobe.com/www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart3.pdf

void JPGDatasetCommon::ReadXMPMetadata()
{
    if (bHasReadXMPMetadata)
        return;

    // Save current position to avoid disturbing JPEG stream decoding.
    const vsi_l_offset nCurOffset = VSIFTellL(fpImage);

    // Search for APP1 chunk.
    // TODO(schwehr): What are these constants?
    GByte abyChunkHeader[2 + 2 + 29] = {};
    int nChunkLoc = 2;
    bool bFoundXMP = false;

    while(true)
    {
        if( VSIFSeekL(fpImage, nChunkLoc, SEEK_SET) != 0 )
            break;

        if( VSIFReadL(abyChunkHeader, sizeof(abyChunkHeader), 1, fpImage) != 1 )
            break;

        nChunkLoc += 2 + abyChunkHeader[2] * 256 + abyChunkHeader[3];
        // COM marker.
        if( abyChunkHeader[0] == 0xFF && abyChunkHeader[1] == 0xFE )
            continue;

        if( abyChunkHeader[0] != 0xFF
            || (abyChunkHeader[1] & 0xf0) != 0xe0 )
            break;  // Not an APP chunk.

        if( abyChunkHeader[1] == 0xe1 &&
            STARTS_WITH(reinterpret_cast<char *>(abyChunkHeader) + 4,
                        "http://ns.adobe.com/xap/1.0/") )
        {
            bFoundXMP = true;
            break;  // APP1 - XMP.
        }
    }

    if (bFoundXMP)
    {
        const int nXMPLength = abyChunkHeader[2] * 256 + abyChunkHeader[3];
        if (nXMPLength > 2 + 29)
        {
            char *pszXMP =
                static_cast<char *>(VSIMalloc(nXMPLength - 2 - 29 + 1));
            if (pszXMP)
            {
                if (VSIFReadL(pszXMP, nXMPLength - 2 - 29, 1, fpImage) == 1)
                {
                    pszXMP[nXMPLength - 2 - 29] = '\0';

                    // Avoid setting the PAM dirty bit just for that.
                    const int nOldPamFlags = nPamFlags;

                    char *apszMDList[2] = { pszXMP, NULL };
                    SetMetadata(apszMDList, "xml:XMP");

                    nPamFlags = nOldPamFlags;
                }
                VSIFree(pszXMP);
            }
        }
    }

    VSIFSeekL(fpImage, nCurOffset, SEEK_SET);

    bHasReadXMPMetadata = true;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **JPGDatasetCommon::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:XMP", "COLOR_PROFILE", NULL);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/
char **JPGDatasetCommon::GetMetadata( const char *pszDomain )
{
    if (fpImage == NULL)
        return NULL;
    if (eAccess == GA_ReadOnly && !bHasReadEXIFMetadata &&
        (pszDomain == NULL || EQUAL(pszDomain, "")))
        ReadEXIFMetadata();
    if (eAccess == GA_ReadOnly && !bHasReadXMPMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "xml:XMP"))
        ReadXMPMetadata();
    if (eAccess == GA_ReadOnly && !bHasReadICCMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "COLOR_PROFILE"))
        ReadICCProfile();
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                       GetMetadataItem()                              */
/************************************************************************/
const char *JPGDatasetCommon::GetMetadataItem( const char *pszName,
                                               const char *pszDomain )
{
    if (fpImage == NULL)
        return NULL;
    if (eAccess == GA_ReadOnly && !bHasReadEXIFMetadata &&
        (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        pszName != NULL &&
        (EQUAL(pszName, "COMMENT") || STARTS_WITH_CI(pszName, "EXIF_")))
        ReadEXIFMetadata();
    if (eAccess == GA_ReadOnly && !bHasReadICCMetadata &&
        pszDomain != NULL && EQUAL(pszDomain, "COLOR_PROFILE"))
        ReadICCProfile();
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                        ReadICCProfile()                              */
/*                                                                      */
/*                 Read ICC Profile from APP2 data                      */
/************************************************************************/
void JPGDatasetCommon::ReadICCProfile()
{
    if (bHasReadICCMetadata)
        return;
    bHasReadICCMetadata = true;

    const vsi_l_offset nCurOffset = VSIFTellL(fpImage);

    int nChunkCount = -1;
    int anChunkSize[256] = {};
    char *apChunk[256] = {};

    // Search for APP2 chunk.
    GByte abyChunkHeader[18] = {};
    int nChunkLoc = 2;
    bool bOk = true;

    while(true)
    {
        if( VSIFSeekL(fpImage, nChunkLoc, SEEK_SET) != 0 )
            break;

        if( VSIFReadL(abyChunkHeader, sizeof(abyChunkHeader), 1, fpImage) != 1 )
            break;

        if( abyChunkHeader[0] != 0xFF )
            break; // Not a valid tag

        if (abyChunkHeader[1] == 0xD9)
            break;  // End of image

        if ((abyChunkHeader[1] >= 0xD0) && (abyChunkHeader[1] <= 0xD8))
        {
            // Restart tags have no length
            nChunkLoc += 2;
            continue;
        }

        const int nChunkLength = abyChunkHeader[2] * 256 + abyChunkHeader[3];

        if( abyChunkHeader[1] == 0xe2 &&
            memcmp(reinterpret_cast<char *>(abyChunkHeader) + 4,
                   "ICC_PROFILE\0", 12) == 0 )
        {
            // Get length and segment ID
            // Header:
            // APP2 tag: 2 bytes
            // App Length: 2 bytes
            // ICC_PROFILE\0 tag: 12 bytes
            // Segment index: 1 bytes
            // Total segments: 1 bytes
            const int nICCChunkLength = nChunkLength - 16;
            if( nICCChunkLength < 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "nICCChunkLength unreasonable: %d", nICCChunkLength);
                bOk = false;
                break;
            }
            const int nICCChunkID = abyChunkHeader[16];
            const int nICCMaxChunkID = abyChunkHeader[17];

            if (nChunkCount == -1)
                nChunkCount = nICCMaxChunkID;

            // Check that all max segment counts are the same.
            if (nICCMaxChunkID != nChunkCount)
            {
                bOk = false;
                break;
            }

            // Check that no segment ID is larger than the total segment count.
            if( (nICCChunkID > nChunkCount) || (nICCChunkID == 0) ||
                (nChunkCount == 0) )
            {
                bOk = false;
                break;
            }

            // Check if ICC segment already loaded.
            if (apChunk[nICCChunkID-1] != NULL)
            {
                bOk = false;
                break;
            }

            // Load it.
            apChunk[nICCChunkID - 1] =
                static_cast<char *>(VSIMalloc(nICCChunkLength));
            if( apChunk[nICCChunkID - 1] == NULL )
            {
                bOk = false;
                break;
            }
            anChunkSize[nICCChunkID - 1] = nICCChunkLength;

            if( VSIFReadL(apChunk[nICCChunkID - 1], nICCChunkLength, 1,
                         fpImage) != 1 )
            {
                bOk = false;
                break;
            }
        }

        nChunkLoc += 2 + nChunkLength;
    }

    int nTotalSize = 0;

    // Get total size and verify that there are no missing segments.
    if (bOk)
    {
        for(int i = 0; i < nChunkCount; i++)
        {
            if (apChunk[i] == NULL)
            {
                // Missing segment - abort.
                bOk = false;
                break;
            }
            const int nSize = anChunkSize[i];
            if( nSize < 0 ||
                nTotalSize > std::numeric_limits<int>::max() - nSize )
            {
                CPLError(CE_Failure, CPLE_FileIO, "nTotalSize nonsensical");
                bOk = false;
                break;
            }
            nTotalSize += anChunkSize[i];
        }
    }

    // TODO(schwehr): Can we know what the maximum reasonable size is?
    if( nTotalSize > 2 << 28 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "nTotalSize unreasonable: %d", nTotalSize);
        bOk = false;
    }

    // Merge all segments together and set metadata.
    if (bOk && nChunkCount > 0)
    {
        char *pBuffer = static_cast<char *>(VSIMalloc(nTotalSize));
        if( pBuffer == NULL )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "ICCProfile too large.  nTotalSize: %d", nTotalSize);
        }
        else
        {
            char *pBufferPtr = pBuffer;
            for(int i = 0; i < nChunkCount; i++)
            {
                memcpy(pBufferPtr, apChunk[i], anChunkSize[i]);
                pBufferPtr += anChunkSize[i];
            }

            // Escape the profile.
            char *pszBase64Profile =
                CPLBase64Encode(nTotalSize, reinterpret_cast<GByte *>(pBuffer));

            // Avoid setting the PAM dirty bit just for that.
            const int nOldPamFlags = nPamFlags;

            // Set ICC profile metadata.
            SetMetadataItem("SOURCE_ICC_PROFILE", pszBase64Profile,
                            "COLOR_PROFILE");

            nPamFlags = nOldPamFlags;

            VSIFree(pBuffer);
            CPLFree(pszBase64Profile);
        }
    }

    for(int i = 0; i < nChunkCount; i++)
    {
        if (apChunk[i] != NULL)
            VSIFree(apChunk[i]);
    }

    VSIFSeekL(fpImage, nCurOffset, SEEK_SET);
}

/************************************************************************/
/*                        EXIFInit()                                    */
/*                                                                      */
/*           Create Metadata from Information file directory APP1       */
/************************************************************************/
bool JPGDatasetCommon::EXIFInit(VSILFILE *fp)
{
    if( nTiffDirStart == 0 )
        return false;
    if( nTiffDirStart > 0 )
        return true;
    nTiffDirStart = 0;

    // TODO(schwehr): Do a compile time endian check.
    int one = 1;
    bigendian = *reinterpret_cast<char *>(&one) == 0;

    // Search for APP1 chunk.
    GByte abyChunkHeader[10] = {};
    int nChunkLoc = 2;

    while(true)
    {
        if( VSIFSeekL(fp, nChunkLoc, SEEK_SET) != 0 )
            return false;

        if( VSIFReadL(abyChunkHeader, sizeof(abyChunkHeader), 1, fp) != 1 )
            return false;

        const int nChunkLength = abyChunkHeader[2] * 256 + abyChunkHeader[3];
        // COM marker
        if( abyChunkHeader[0] == 0xFF && abyChunkHeader[1] == 0xFE &&
            nChunkLength >= 2 )
        {
            char *pszComment =
                static_cast<char *>(CPLMalloc(nChunkLength - 2 + 1));
            if( nChunkLength > 2 &&
                VSIFSeekL(fp, nChunkLoc + 4, SEEK_SET) == 0 &&
                VSIFReadL(pszComment, nChunkLength - 2, 1, fp) == 1 )
            {
                pszComment[nChunkLength - 2] = 0;
                // Avoid setting the PAM dirty bit just for that.
                const int nOldPamFlags = nPamFlags;
                // Set ICC profile metadata.
                SetMetadataItem("COMMENT", pszComment);
                nPamFlags = nOldPamFlags;
            }
            CPLFree(pszComment);
        }
        else
        {
            if( abyChunkHeader[0] != 0xFF ||
                (abyChunkHeader[1] & 0xf0) != 0xe0 )
                break;  // Not an APP chunk.

            if( abyChunkHeader[1] == 0xe1 &&
                STARTS_WITH(reinterpret_cast<char *>(abyChunkHeader) + 4,
                            "Exif") )
            {
                nTIFFHEADER = nChunkLoc + 10;
            }
        }

        nChunkLoc += 2 + nChunkLength;
    }

    if( nTIFFHEADER < 0 )
        return false;

    // Read TIFF header.
    TIFFHeader hdr = { 0, 0, 0 };

    VSIFSeekL(fp, nTIFFHEADER, SEEK_SET);
    if( VSIFReadL(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr) )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to read %d byte from image header.",
                 static_cast<int>(sizeof(hdr)));
        return false;
    }

    if (hdr.tiff_magic != TIFF_BIGENDIAN && hdr.tiff_magic != TIFF_LITTLEENDIAN)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Not a TIFF file, bad magic number %u (%#x)",
                 hdr.tiff_magic, hdr.tiff_magic);
        return false;
    }

    if (hdr.tiff_magic == TIFF_BIGENDIAN)    bSwabflag = !bigendian;
    if (hdr.tiff_magic == TIFF_LITTLEENDIAN) bSwabflag = bigendian;

    if (bSwabflag) {
        CPL_SWAP16PTR(&hdr.tiff_version);
        CPL_SWAP32PTR(&hdr.tiff_diroff);
    }

    if (hdr.tiff_version != TIFF_VERSION)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Not a TIFF file, bad version number %u (%#x)",
                 hdr.tiff_version, hdr.tiff_version);
        return false;
    }
    nTiffDirStart = hdr.tiff_diroff;

    CPLDebug("JPEG", "Magic: %#x <%s-endian> Version: %#x\n",
             hdr.tiff_magic,
             hdr.tiff_magic == TIFF_BIGENDIAN ? "big" : "little",
             hdr.tiff_version );

    return true;
}

/************************************************************************/
/*                            JPGMaskBand()                             */
/************************************************************************/

JPGMaskBand::JPGMaskBand( JPGDataset *poDSIn )

{
    poDS = poDSIn;
    nBand = 0;

    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();

    eDataType = GDT_Byte;
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPGMaskBand::IReadBlock( int /* nBlockX */, int nBlockY, void *pImage )
{
    JPGDataset *poJDS = static_cast<JPGDataset *>(poDS);

    // Make sure the mask is loaded and decompressed.
    poJDS->DecompressMask();
    if( poJDS->pabyBitMask == NULL )
        return CE_Failure;

    // Set mask based on bitmask for this scanline.
    GUInt32 iBit =
        static_cast<GUInt32>(nBlockY) * static_cast<GUInt32>(nBlockXSize);

    GByte *const pbyImage = static_cast<GByte *>(pImage);
    if( poJDS->bMaskLSBOrder )
    {
        for( int iX = 0; iX < nBlockXSize; iX++ )
        {
            if( poJDS->pabyBitMask[iBit >> 3] & (0x1 << (iBit & 7)) )
                pbyImage[iX] = 255;
            else
                pbyImage[iX] = 0;
            iBit++;
        }
    }
    else
    {
        for( int iX = 0; iX < nBlockXSize; iX++ )
        {
            if( poJDS->pabyBitMask[iBit >> 3] & (0x1 << (7 - (iBit & 7))) )
                pbyImage[iX] = 255;
            else
                pbyImage[iX] = 0;
            iBit++;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           JPGRasterBand()                            */
/************************************************************************/

JPGRasterBand::JPGRasterBand( JPGDatasetCommon *poDSIn, int nBandIn ) :
    poGDS(poDSIn)
{
    poDS = poDSIn;

    nBand = nBandIn;
    if( poDSIn->GetDataPrecision() == 12 )
        eDataType = GDT_UInt16;
    else
        eDataType = GDT_Byte;

    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = 1;

    GDALMajorObject::SetMetadataItem("COMPRESSION", "JPEG", "IMAGE_STRUCTURE");
}

/************************************************************************/
/*                           JPGCreateBand()                            */
/************************************************************************/

GDALRasterBand *JPGCreateBand(JPGDatasetCommon *poDS, int nBand)
{
    return new JPGRasterBand(poDS, nBand);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void *pImage )

{
    CPLAssert(nBlockXOff == 0);

    const int nXSize = GetXSize();
    const int nWordSize = GDALGetDataTypeSizeBytes(eDataType);
    if (poGDS->fpImage == NULL)
    {
        memset(pImage, 0, nXSize * nWordSize);
        return CE_None;
    }

    // Load the desired scanline into the working buffer.
    CPLErr eErr = poGDS->LoadScanline(nBlockYOff);
    if( eErr != CE_None )
        return eErr;

    // Transfer between the working buffer the callers buffer.
    if( poGDS->GetRasterCount() == 1 )
    {
#ifdef JPEG_LIB_MK1
        GDALCopyWords(poGDS->pabyScanline, GDT_UInt16, 2,
                      pImage, eDataType, nWordSize,
                      nXSize);
#else
        memcpy(pImage, poGDS->pabyScanline, nXSize * nWordSize);
#endif
    }
    else
    {
#ifdef JPEG_LIB_MK1
        GDALCopyWords(poGDS->pabyScanline + (nBand - 1) * 2,
                      GDT_UInt16, 6,
                      pImage, eDataType, nWordSize,
                      nXSize);
#else
        if (poGDS->eGDALColorSpace == JCS_RGB &&
            poGDS->GetOutColorSpace() == JCS_CMYK &&
            eDataType == GDT_Byte)
        {
            GByte *const pbyImage = static_cast<GByte *>(pImage);
            if (nBand == 1)
            {
                for( int i=0; i < nXSize; i++ )
                {
                    const int C = poGDS->pabyScanline[i * 4 + 0];
                    const int K = poGDS->pabyScanline[i * 4 + 3];
                    pbyImage[i] = static_cast<GByte>((C * K) / 255);
                }
            }
            else if (nBand == 2)
            {
                for( int i = 0; i < nXSize; i++ )
                {
                    const int M = poGDS->pabyScanline[i * 4 + 1];
                    const int K = poGDS->pabyScanline[i * 4 + 3];
                    pbyImage[i] = static_cast<GByte>((M * K) / 255);
                }
            }
            else if (nBand == 3)
            {
                for( int i = 0; i < nXSize; i++ )
                {
                    const int Y = poGDS->pabyScanline[i * 4 + 2];
                    const int K = poGDS->pabyScanline[i * 4 + 3];
                    pbyImage[i] = static_cast<GByte>((Y * K) / 255);
                }
            }
        }
        else
        {
            GDALCopyWords(poGDS->pabyScanline + (nBand - 1) * nWordSize,
                          eDataType, nWordSize * poGDS->GetRasterCount(),
                          pImage, eDataType, nWordSize, nXSize);
        }
#endif
    }

    // Forcibly load the other bands associated with this scanline.
    if( nBand == 1 )
    {
        for( int iBand = 2; iBand <= poGDS->GetRasterCount() ; iBand++ )
        {
            GDALRasterBlock *const poBlock =
                poGDS->GetRasterBand(iBand)->
                    GetLockedBlockRef(nBlockXOff, nBlockYOff);
            if( poBlock != NULL )
                poBlock->DropLock();
        }
    }

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JPGRasterBand::GetColorInterpretation()

{
    if( poGDS->eGDALColorSpace == JCS_GRAYSCALE )
        return GCI_GrayIndex;

    else if( poGDS->eGDALColorSpace == JCS_RGB)
    {
        if ( nBand == 1 )
            return GCI_RedBand;
        else if( nBand == 2 )
            return GCI_GreenBand;
        else
            return GCI_BlueBand;
    }
    else if( poGDS->eGDALColorSpace == JCS_CMYK)
    {
        if ( nBand == 1 )
            return GCI_CyanBand;
        else if( nBand == 2 )
            return GCI_MagentaBand;
        else if ( nBand == 3 )
            return GCI_YellowBand;
        else
            return GCI_BlackBand;
    }
    else if( poGDS->eGDALColorSpace == JCS_YCbCr ||
             poGDS->eGDALColorSpace == JCS_YCCK)
    {
        if ( nBand == 1 )
            return GCI_YCbCr_YBand;
        else if( nBand == 2 )
            return GCI_YCbCr_CbBand;
        else if ( nBand == 3 )
            return GCI_YCbCr_CrBand;
        else
            return GCI_BlackBand;
    }

    CPLAssert(false);
    return GCI_Undefined;
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

GDALRasterBand *JPGRasterBand::GetMaskBand()

{
    if (poGDS->nScaleFactor > 1 )
        return GDALPamRasterBand::GetMaskBand();

    if (poGDS->fpImage == NULL)
        return NULL;

    if( !poGDS->bHasCheckedForMask)
    {
        if( CPLTestBool(CPLGetConfigOption("JPEG_READ_MASK", "YES")))
            poGDS->CheckForMask();
        poGDS->bHasCheckedForMask = true;
    }
    if( poGDS->pabyCMask )
    {
        if( poGDS->poMaskBand == NULL )
            poGDS->poMaskBand =
                new JPGMaskBand(static_cast<JPGDataset *>(poDS));

        return poGDS->poMaskBand;
    }

    return GDALPamRasterBand::GetMaskBand();
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

int JPGRasterBand::GetMaskFlags()

{
    if (poGDS->nScaleFactor > 1 )
        return GDALPamRasterBand::GetMaskFlags();

    if (poGDS->fpImage == NULL)
        return 0;

    GetMaskBand();
    if( poGDS->poMaskBand != NULL )
        return GMF_PER_DATASET;

    return GDALPamRasterBand::GetMaskFlags();
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *JPGRasterBand::GetOverview(int i)
{
    poGDS->InitInternalOverviews();

    if( poGDS->nInternalOverviewsCurrent == 0 )
        return GDALPamRasterBand::GetOverview(i);

    if( i < 0 || i >= poGDS->nInternalOverviewsCurrent )
        return NULL;

    return poGDS->papoInternalOverviews[i]->GetRasterBand(nBand);
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int JPGRasterBand::GetOverviewCount()
{
    poGDS->InitInternalOverviews();

    if( poGDS->nInternalOverviewsCurrent == 0 )
        return GDALPamRasterBand::GetOverviewCount();

    return poGDS->nInternalOverviewsCurrent;
}

/************************************************************************/
/* ==================================================================== */
/*                             JPGDataset                               */
/* ==================================================================== */
/************************************************************************/

JPGDatasetCommon::JPGDatasetCommon() :
    nScaleFactor(1),
    bHasInitInternalOverviews(false),
    nInternalOverviewsCurrent(0),
    nInternalOverviewsToFree(0),
    papoInternalOverviews(NULL),
    pszProjection(NULL),
    bGeoTransformValid(false),
    nGCPCount(0),
    pasGCPList(NULL),
    fpImage(NULL),
    nSubfileOffset(0),
    nLoadedScanline(-1),
    pabyScanline(NULL),
    bHasReadEXIFMetadata(false),
    bHasReadXMPMetadata(false),
    bHasReadICCMetadata(false),
    papszMetadata(NULL),
    papszSubDatasets(NULL),
    bigendian(true),
    nExifOffset(-1),
    nInterOffset(-1),
    nGPSOffset(-1),
    bSwabflag(false),
    nTiffDirStart(-1),
    nTIFFHEADER(-1),
    bHasDoneJpegCreateDecompress(false),
    bHasDoneJpegStartDecompress(false),
    bHasCheckedForMask(false),
    poMaskBand(NULL),
    pabyBitMask(NULL),
    bMaskLSBOrder(true),
    pabyCMask(NULL),
    nCMaskSize(0),
    eGDALColorSpace(JCS_UNKNOWN),
    bIsSubfile(false),
    bHasTriedLoadWorldFileOrTab(false)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                           ~JPGDataset()                              */
/************************************************************************/

JPGDatasetCommon::~JPGDatasetCommon()

{
    if( fpImage != NULL )
        VSIFCloseL(fpImage);

    if( pabyScanline != NULL )
        CPLFree(pabyScanline);
    if( papszMetadata != NULL )
        CSLDestroy(papszMetadata);

    if ( pszProjection )
        CPLFree(pszProjection);

    if ( nGCPCount > 0 )
    {
        GDALDeinitGCPs(nGCPCount, pasGCPList);
        CPLFree(pasGCPList);
    }

    CPLFree(pabyBitMask);
    CPLFree(pabyCMask);
    delete poMaskBand;

    CloseDependentDatasets();
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

int JPGDatasetCommon::CloseDependentDatasets()
{
    int bRet = GDALPamDataset::CloseDependentDatasets();
    if( nInternalOverviewsToFree )
    {
        bRet = TRUE;
        for(int i = 0; i < nInternalOverviewsToFree; i++)
            delete papoInternalOverviews[i];
        nInternalOverviewsToFree = 0;
    }
    CPLFree(papoInternalOverviews);
    papoInternalOverviews = NULL;

    return bRet;
}

/************************************************************************/
/*                          InitEXIFOverview()                          */
/************************************************************************/

GDALDataset* JPGDatasetCommon::InitEXIFOverview()
{
    if( !EXIFInit(fpImage) )
        return NULL;

    // Read number of entry in directory.
    GUInt16 nEntryCount = 0;
    if( nTiffDirStart > (INT_MAX - nTIFFHEADER) ||
        VSIFSeekL(fpImage, nTiffDirStart+nTIFFHEADER, SEEK_SET) != 0 ||
        VSIFReadL(&nEntryCount, 1,
                  sizeof(GUInt16), fpImage) != sizeof(GUInt16) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error reading EXIF Directory count at " CPL_FRMT_GUIB,
                 static_cast<vsi_l_offset>(nTiffDirStart) + nTIFFHEADER);
        return NULL;
    }

    if (bSwabflag)
        CPL_SWAP16PTR(&nEntryCount);

    // Some files are corrupt, a large entry count is a sign of this.
    if( nEntryCount > 125 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Ignoring EXIF directory with unlikely entry count (%d).",
                 nEntryCount);
        return NULL;
    }

    // Skip EXIF entries.
    VSIFSeekL(fpImage, nEntryCount * sizeof(GDALEXIFTIFFDirEntry), SEEK_CUR);

    // Read offset of next directory (IFD1).
    GUInt32 nNextDirOff = 0;
    if( VSIFReadL(&nNextDirOff, 1, sizeof(GUInt32), fpImage) !=
        sizeof(GUInt32) )
        return NULL;
    if( bSwabflag )
        CPL_SWAP32PTR(&nNextDirOff);
    if( nNextDirOff == 0 || nNextDirOff > UINT_MAX - nTIFFHEADER )
        return NULL;

    // Seek to IFD1.
    if( VSIFSeekL(fpImage, nTIFFHEADER + nNextDirOff, SEEK_SET) != 0 ||
        VSIFReadL(&nEntryCount, 1, sizeof(GUInt16), fpImage) != sizeof(GUInt16) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Error reading IFD1 Directory count at %d.",
                 nTIFFHEADER + nNextDirOff);
        return NULL;
    }

    if (bSwabflag)
        CPL_SWAP16PTR(&nEntryCount);
    if( nEntryCount > 125 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Ignoring IFD1 directory with unlikely entry count (%d).",
                 nEntryCount);
        return NULL;
    }
#if DEBUG_VERBOSE
    CPLDebug("JPEG", "IFD1 entry count = %d", nEntryCount);
#endif

    int nImageWidth = 0;
    int nImageHeight = 0;
    int nCompression = 6;
    GUInt32 nJpegIFOffset = 0;
    GUInt32 nJpegIFByteCount = 0;
    for( int i = 0; i < nEntryCount; i ++ )
    {
        GDALEXIFTIFFDirEntry sEntry;
        if( VSIFReadL(&sEntry, 1, sizeof(sEntry), fpImage) != sizeof(sEntry) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot read entry %d of IFD1", i);
            return NULL;
        }
        if (bSwabflag)
        {
            CPL_SWAP16PTR(&sEntry.tdir_tag);
            CPL_SWAP16PTR(&sEntry.tdir_type);
            CPL_SWAP32PTR(&sEntry.tdir_count);
            CPL_SWAP32PTR(&sEntry.tdir_offset);
        }

#ifdef DEBUG_VERBOSE
        CPLDebug("JPEG", "tag = %d (0x%4X), type = %d, count = %d, offset = %d",
                 sEntry.tdir_tag, sEntry.tdir_tag, sEntry.tdir_type,
                 sEntry.tdir_count, sEntry.tdir_offset);
#endif

        if( (sEntry.tdir_type == TIFF_SHORT || sEntry.tdir_type == TIFF_LONG) &&
            sEntry.tdir_count == 1 )
        {
            switch( sEntry.tdir_tag )
            {
            case JPEG_TIFF_IMAGEWIDTH:
                nImageWidth = sEntry.tdir_offset;
                break;
            case JPEG_TIFF_IMAGEHEIGHT:
                nImageHeight = sEntry.tdir_offset;
                break;
            case JPEG_TIFF_COMPRESSION:
                nCompression = sEntry.tdir_offset;
                break;
            case JPEG_EXIF_JPEGIFOFSET:
                nJpegIFOffset = sEntry.tdir_offset;
                break;
            case JPEG_EXIF_JPEGIFBYTECOUNT:
                nJpegIFByteCount = sEntry.tdir_offset;
                break;
            default:
                break;
            }
        }
    }
    if( nCompression != 6 ||
        nImageWidth >= nRasterXSize ||
        nImageHeight >= nRasterYSize ||
        nJpegIFOffset == 0 ||
        nJpegIFOffset > UINT_MAX - nTIFFHEADER ||
        static_cast<int>(nJpegIFByteCount) <= 0 )
    {
        return NULL;
    }

    const char* pszSubfile =
        CPLSPrintf("JPEG_SUBFILE:%u,%d,%s",
                   nTIFFHEADER + nJpegIFOffset,
                   nJpegIFByteCount,
                   GetDescription());
    JPGDatasetOpenArgs sArgs;
    sArgs.pszFilename = pszSubfile;
    sArgs.fpLin = NULL;
    sArgs.papszSiblingFiles = NULL;
    sArgs.nScaleFactor = 1;
    sArgs.bDoPAMInitialize = false;
    sArgs.bUseInternalOverviews = false;
    return JPGDataset::Open(&sArgs);
}

/************************************************************************/
/*                       InitInternalOverviews()                        */
/************************************************************************/

void JPGDatasetCommon::InitInternalOverviews()
{
    if( bHasInitInternalOverviews )
        return;
    bHasInitInternalOverviews = true;

    // Instantiate on-the-fly overviews (if no external ones).
    if( nScaleFactor == 1 && GetRasterBand(1)->GetOverviewCount() == 0 )
    {
        // EXIF overview.
        GDALDataset *poEXIFOverview = NULL;
        if( nRasterXSize > 512 || nRasterYSize > 512 )
        {
            const vsi_l_offset nCurOffset = VSIFTellL(fpImage);
            poEXIFOverview = InitEXIFOverview();
            if( poEXIFOverview != NULL )
            {
                if( poEXIFOverview->GetRasterCount() != nBands ||
                    poEXIFOverview->GetRasterXSize() >= nRasterXSize ||
                    poEXIFOverview->GetRasterYSize() >= nRasterYSize )
                {
                    GDALClose(poEXIFOverview);
                    poEXIFOverview = NULL;
                }
                else
                {
                    CPLDebug("JPEG", "EXIF overview (%d x %d) detected",
                             poEXIFOverview->GetRasterXSize(),
                             poEXIFOverview->GetRasterYSize());
                }
            }
            VSIFSeekL(fpImage, nCurOffset, SEEK_SET);
        }

        // libjpeg-6b only supports 2, 4 and 8 scale denominators.
        // TODO: Later versions support more.

        int nImplicitOverviews = 0;

        // For the needs of the implicit JPEG-in-TIFF overview mechanism.
        if( CPLTestBool(
               CPLGetConfigOption("JPEG_FORCE_INTERNAL_OVERVIEWS", "NO")) )
        {
            nImplicitOverviews = 3;
        }
        else
        {
            for( int i = 2; i >= 0; i--)
            {
                if( nRasterXSize >= (256 << i) || nRasterYSize >= (256 << i) )
                {
                    nImplicitOverviews = i + 1;
                    break;
                }
            }
        }

        if( nImplicitOverviews > 0 )
        {
            papoInternalOverviews = static_cast<GDALDataset **>(
                CPLMalloc((nImplicitOverviews + (poEXIFOverview ? 1 : 0)) *
                          sizeof(GDALDataset *)));
            for( int i = 0; i < nImplicitOverviews; i++ )
            {
                if( poEXIFOverview != NULL &&
                    poEXIFOverview->GetRasterXSize() >= nRasterXSize >> (i + 1) )
                {
                    break;
                }
                JPGDatasetOpenArgs sArgs;
                sArgs.pszFilename = GetDescription();
                sArgs.fpLin = NULL;
                sArgs.papszSiblingFiles = NULL;
                sArgs.nScaleFactor = 1 << (i + 1);
                sArgs.bDoPAMInitialize = false;
                sArgs.bUseInternalOverviews = false;
                GDALDataset *poImplicitOverview = JPGDataset::Open(&sArgs);
                if( poImplicitOverview == NULL )
                    break;
                papoInternalOverviews[nInternalOverviewsCurrent] =
                    poImplicitOverview;
                nInternalOverviewsCurrent++;
                nInternalOverviewsToFree++;
            }
            if( poEXIFOverview != NULL )
            {
                papoInternalOverviews[nInternalOverviewsCurrent] =
                    poEXIFOverview;
                nInternalOverviewsCurrent ++;
                nInternalOverviewsToFree ++;
            }
        }
        else if( poEXIFOverview )
        {
            papoInternalOverviews =
                static_cast<GDALDataset **>(CPLMalloc(sizeof(GDALDataset *)));
            papoInternalOverviews[0] = poEXIFOverview;
            nInternalOverviewsCurrent++;
            nInternalOverviewsToFree++;
        }
    }
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr JPGDatasetCommon::IBuildOverviews( const char *pszResampling,
                                          int nOverviewsListCount,
                                          int *panOverviewList,
                                          int nListBands, int *panBandList,
                                          GDALProgressFunc pfnProgress,
                                          void * pProgressData )
{
    bHasInitInternalOverviews = true;
    nInternalOverviewsCurrent = 0;

    return GDALPamDataset::IBuildOverviews(pszResampling,
                                           nOverviewsListCount,
                                           panOverviewList,
                                           nListBands, panBandList,
                                           pfnProgress, pProgressData);
}

/************************************************************************/
/*                           FlushCache()                               */
/************************************************************************/

void JPGDatasetCommon::FlushCache()

{
    GDALPamDataset::FlushCache();

    if (bHasDoneJpegStartDecompress)
    {
        Restart();
    }

    // For the needs of the implicit JPEG-in-TIFF overview mechanism.
    for(int i = 0; i < nInternalOverviewsCurrent; i++)
        papoInternalOverviews[i]->FlushCache();
}

#endif  // !defined(JPGDataset)

/************************************************************************/
/*                            JPGDataset()                              */
/************************************************************************/

JPGDataset::JPGDataset() : nQLevel(0)
{
    memset(&sDInfo, 0, sizeof(sDInfo));
    sDInfo.data_precision = 8;

    memset(&sJErr, 0, sizeof(sJErr));
    memset(&sJProgress, 0, sizeof(sJProgress));
}

/************************************************************************/
/*                           ~JPGDataset()                            */
/************************************************************************/

JPGDataset::~JPGDataset()

{
    GDALPamDataset::FlushCache();

    if (bHasDoneJpegStartDecompress)
    {
        jpeg_abort_decompress(&sDInfo);
    }
    if (bHasDoneJpegCreateDecompress)
    {
        jpeg_destroy_decompress(&sDInfo);
    }
}

/************************************************************************/
/*                      ErrorOutOnNonFatalError()                       */
/************************************************************************/

bool JPGDataset::ErrorOutOnNonFatalError()
{
    if( sErrorStruct.bNonFatalErrorEncountered )
    {
        sErrorStruct.bNonFatalErrorEncountered = false;
        return true;
    }
    return false;
}

/************************************************************************/
/*                            LoadScanline()                            */
/************************************************************************/

CPLErr JPGDataset::LoadScanline( int iLine )

{
    if( nLoadedScanline == iLine )
        return CE_None;

    // setup to trap a fatal error.
    if (setjmp(sErrorStruct.setjmp_buffer))
        return CE_Failure;

    if (!bHasDoneJpegStartDecompress)
    {

        /* In some cases, libjpeg needs to allocate a lot of memory */
        /* http://www.libjpeg-turbo.org/pmwiki/uploads/About/TwoIssueswiththeJPEGStandard.pdf */
        if( jpeg_has_multiple_scans(&(sDInfo)) )
        {
            /* In this case libjpeg will need to allocate memory or backing */
            /* store for all coefficients */
            /* See call to jinit_d_coef_controller() from master_selection() */
            /* in libjpeg */
            vsi_l_offset nRequiredMemory =
                static_cast<vsi_l_offset>(sDInfo.image_width) *
                sDInfo.image_height * sDInfo.num_components *
                ((sDInfo.data_precision+7)/8);
            /* BLOCK_SMOOTHING_SUPPORTED is generally defined, so we need */
            /* to replicate the logic of jinit_d_coef_controller() */
            if( sDInfo.progressive_mode )
                nRequiredMemory *= 3;

    #ifndef GDAL_LIBJPEG_LARGEST_MEM_ALLOC
    #define GDAL_LIBJPEG_LARGEST_MEM_ALLOC (100 * 1024 * 1024)
    #endif

            if( nRequiredMemory > GDAL_LIBJPEG_LARGEST_MEM_ALLOC &&
                CPLGetConfigOption("GDAL_ALLOW_LARGE_LIBJPEG_MEM_ALLOC", NULL) == NULL )
            {
                    CPLError(CE_Failure, CPLE_NotSupported,
                        "Reading this image would require libjpeg to allocate "
                        "at least " CPL_FRMT_GUIB " bytes. "
                        "This is disabled since above the " CPL_FRMT_GUIB " threshold. "
                        "You may override this restriction by defining the "
                        "GDAL_ALLOW_LARGE_LIBJPEG_MEM_ALLOC environment variable, "
                        "or recompile GDAL by defining the "
                        "GDAL_LIBJPEG_LARGEST_MEM_ALLOC macro to a value greater "
                        "than " CPL_FRMT_GUIB,
                        static_cast<GUIntBig>(nRequiredMemory),
                        static_cast<GUIntBig>(GDAL_LIBJPEG_LARGEST_MEM_ALLOC),
                        static_cast<GUIntBig>(GDAL_LIBJPEG_LARGEST_MEM_ALLOC));
                    return CE_Failure;
            }
        }

        sDInfo.progress = &sJProgress;
        sJProgress.progress_monitor = JPGDataset::ProgressMonitor;
        jpeg_start_decompress(&sDInfo);
        bHasDoneJpegStartDecompress = true;
    }

    if( pabyScanline == NULL )
    {
        int nJPEGBands = 0;
        switch(sDInfo.out_color_space)
        {
        case JCS_GRAYSCALE:
            nJPEGBands = 1;
            break;
        case JCS_RGB:
        case JCS_YCbCr:
            nJPEGBands = 3;
            break;
        case JCS_CMYK:
        case JCS_YCCK:
            nJPEGBands = 4;
            break;

        default:
            CPLAssert(false);
        }

        pabyScanline =
            static_cast<GByte *>(CPLMalloc(nJPEGBands * GetRasterXSize() * 2));
    }

    if( iLine < nLoadedScanline )
    {
        if( Restart() != CE_None )
            return CE_Failure;
    }

    while( nLoadedScanline < iLine )
    {
        JSAMPLE *ppSamples = reinterpret_cast<JSAMPLE *>(pabyScanline);
        jpeg_read_scanlines(&sDInfo, &ppSamples, 1);
        if( ErrorOutOnNonFatalError() )
            return CE_Failure;
        nLoadedScanline++;
    }

    return CE_None;
}

/************************************************************************/
/*                         LoadDefaultTables()                          */
/************************************************************************/

#if !defined(JPGDataset)

#define Q1table GDALJPEG_Q1table
#define Q2table GDALJPEG_Q2table
#define Q3table GDALJPEG_Q3table
#define Q4table GDALJPEG_Q4table
#define Q5table GDALJPEG_Q5table
#define AC_BITS GDALJPEG_AC_BITS
#define AC_HUFFVAL GDALJPEG_AC_HUFFVAL
#define DC_BITS GDALJPEG_DC_BITS
#define DC_HUFFVAL GDALJPEG_DC_HUFFVAL

static const GByte Q1table[64] =
{
    8,    72,  72,  72,  72,  72,  72,  72, // 0 - 7
    72,   72,  78,  74,  76,  74,  78,  89, // 8 - 15
    81,   84,  84,  81,  89, 106,  93,  94, // 16 - 23
    99,   94,  93, 106, 129, 111, 108, 116, // 24 - 31
    116, 108, 111, 129, 135, 128, 136, 145, // 32 - 39
    136, 128, 135, 155, 160, 177, 177, 160, // 40 - 47
    155, 193, 213, 228, 213, 193, 255, 255, // 48 - 55
    255, 255, 255, 255, 255, 255, 255, 255  // 56 - 63
};

static const GByte Q2table[64] =
{
    8, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 39, 37, 38, 37, 39, 45, 41, 42, 42, 41, 45, 53,
    47, 47, 50, 47, 47, 53, 65, 56, 54, 59, 59, 54, 56, 65, 68, 64, 69, 73,
    69, 64, 68, 78, 81, 89, 89, 81, 78, 98,108,115,108, 98,130,144,144,130,
    178,190,178,243,243,255
};

static const GByte Q3table[64] =
{
     8, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 11, 10, 11, 10, 11, 13, 11, 12, 12, 11, 13, 15,
    13, 13, 14, 13, 13, 15, 18, 16, 15, 16, 16, 15, 16, 18, 19, 18, 19, 21,
    19, 18, 19, 22, 23, 25, 25, 23, 22, 27, 30, 32, 30, 27, 36, 40, 40, 36,
    50, 53, 50, 68, 68, 91
};

static const GByte Q4table[64] =
{
    8, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 8, 7, 8, 7, 8, 9, 8, 8, 8, 8, 9, 11,
    9, 9, 10, 9, 9, 11, 13, 11, 11, 12, 12, 11, 11, 13, 14, 13, 14, 15,
    14, 13, 14, 16, 16, 18, 18, 16, 16, 20, 22, 23, 22, 20, 26, 29, 29, 26,
    36, 38, 36, 49, 49, 65
};

static const GByte Q5table[64] =
{
    4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 6, 5, 5, 6, 7, 6, 6, 6, 6, 6, 6, 7, 8, 7, 8, 8,
    8, 7, 8, 9, 9, 10, 10, 9, 9, 11, 12, 13, 12, 11, 14, 16, 16, 14,
    20, 21, 20, 27, 27, 36
};

static const GByte AC_BITS[16] =
{ 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125 };

static const GByte AC_HUFFVAL[256] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
    0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
    0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
    0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
    0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const GByte DC_BITS[16] =
    { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };

static const GByte DC_HUFFVAL[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B };

void JPGDataset::LoadDefaultTables( int n )
{
    if( nQLevel < 1 )
        return;

    // Load quantization table.
    JQUANT_TBL *quant_ptr = NULL;
    const GByte *pabyQTable = NULL;

    if( nQLevel == 1 )
        pabyQTable = Q1table;
    else if( nQLevel == 2 )
        pabyQTable = Q2table;
    else if( nQLevel == 3 )
        pabyQTable = Q3table;
    else if( nQLevel == 4 )
        pabyQTable = Q4table;
    else if( nQLevel == 5 )
        pabyQTable = Q5table;
    else
        return;

    if (sDInfo.quant_tbl_ptrs[n] == NULL)
        sDInfo.quant_tbl_ptrs[n] =
            jpeg_alloc_quant_table(reinterpret_cast<j_common_ptr>(&sDInfo));

    quant_ptr = sDInfo.quant_tbl_ptrs[n];  // quant_ptr is JQUANT_TBL.
    for (int i = 0; i < 64; i++) {
        // Qtable[] is desired quantization table, in natural array order.
        quant_ptr->quantval[i] = pabyQTable[i];
    }

    // Load AC huffman table.
    if (sDInfo.ac_huff_tbl_ptrs[n] == NULL)
        sDInfo.ac_huff_tbl_ptrs[n] =
            jpeg_alloc_huff_table(reinterpret_cast<j_common_ptr>(&sDInfo));

    // huff_ptr is JHUFF_TBL*.
    JHUFF_TBL *huff_ptr = sDInfo.ac_huff_tbl_ptrs[n];

    for (int i = 1; i <= 16; i++) {
        // counts[i] is number of Huffman codes of length i bits, i=1..16
        huff_ptr->bits[i] = AC_BITS[i-1];
    }

    for (int i = 0; i < 256; i++) {
        // symbols[] is the list of Huffman symbols, in code-length order.
        huff_ptr->huffval[i] = AC_HUFFVAL[i];
    }

    // Load DC huffman table.
    // TODO(schwehr): Revisit this "sideways" cast.
    if (sDInfo.dc_huff_tbl_ptrs[n] == NULL)
        sDInfo.dc_huff_tbl_ptrs[n] =
            jpeg_alloc_huff_table(reinterpret_cast<j_common_ptr>(&sDInfo));

    huff_ptr = sDInfo.dc_huff_tbl_ptrs[n];  // huff_ptr is JHUFF_TBL*

    for (int i = 1; i <= 16; i++) {
        // counts[i] is number of Huffman codes of length i bits, i=1..16
        huff_ptr->bits[i] = DC_BITS[i-1];
    }

    for (int i = 0; i < 256; i++) {
        // symbols[] is the list of Huffman symbols, in code-length order.
        huff_ptr->huffval[i] = DC_HUFFVAL[i];
    }
}
#endif  // !defined(JPGDataset)

/************************************************************************/
/*                       SetScaleNumAndDenom()                          */
/************************************************************************/

void JPGDataset::SetScaleNumAndDenom()
{
#if JPEG_LIB_VERSION > 62
    sDInfo.scale_num = 8 / nScaleFactor;
    sDInfo.scale_denom = 8;
#else
    sDInfo.scale_num = 1;
    sDInfo.scale_denom = nScaleFactor;
#endif
}

/************************************************************************/
/*                              Restart()                               */
/*                                                                      */
/*      Restart compressor at the beginning of the file.                */
/************************************************************************/

CPLErr JPGDataset::Restart()

{
    // Setup to trap a fatal error.
    if (setjmp(sErrorStruct.setjmp_buffer))
        return CE_Failure;

    J_COLOR_SPACE colorSpace = sDInfo.out_color_space;
    J_COLOR_SPACE jpegColorSpace = sDInfo.jpeg_color_space;

    jpeg_abort_decompress(&sDInfo);
    jpeg_destroy_decompress(&sDInfo);
    jpeg_create_decompress(&sDInfo);

#if !defined(JPGDataset)
    LoadDefaultTables(0);
    LoadDefaultTables(1);
    LoadDefaultTables(2);
    LoadDefaultTables(3);
#endif  // !defined(JPGDataset)

    // Restart IO.
    VSIFSeekL(fpImage, nSubfileOffset, SEEK_SET);

    jpeg_vsiio_src(&sDInfo, fpImage);
    jpeg_read_header(&sDInfo, TRUE);

    sDInfo.out_color_space = colorSpace;
    nLoadedScanline = -1;
    SetScaleNumAndDenom();

    // The following errors could happen when "recycling" an existing dataset
    // particularly when triggered by the implicit overviews of JPEG-in-TIFF
    // with a corrupted TIFF file.
    if( nRasterXSize !=
           static_cast<int>(sDInfo.image_width + nScaleFactor - 1) /
               nScaleFactor ||
        nRasterYSize !=
           static_cast<int>(sDInfo.image_height + nScaleFactor - 1) /
               nScaleFactor )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unexpected image dimension (%d x %d), "
                 "where as (%d x %d) was expected",
                 static_cast<int>(sDInfo.image_width + nScaleFactor - 1) /
                     nScaleFactor,
                 static_cast<int>(sDInfo.image_height + nScaleFactor - 1) /
                     nScaleFactor,
                 nRasterXSize, nRasterYSize);
        bHasDoneJpegStartDecompress = false;
    }
    else if( jpegColorSpace != sDInfo.jpeg_color_space )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unexpected jpeg color space : %d",
                 sDInfo.jpeg_color_space);
        bHasDoneJpegStartDecompress = false;
    }
    else
    {
        sDInfo.progress = &sJProgress;
        sJProgress.progress_monitor = JPGDataset::ProgressMonitor;
        jpeg_start_decompress(&sDInfo);
        bHasDoneJpegStartDecompress = true;
    }

    return CE_None;
}

#if !defined(JPGDataset)

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr JPGDatasetCommon::GetGeoTransform( double * padfTransform )

{
    CPLErr eErr = GDALPamDataset::GetGeoTransform(padfTransform);
    if( eErr != CE_Failure )
        return eErr;

    LoadWorldFileOrTab();

    if( bGeoTransformValid )
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);

        return CE_None;
    }

    return eErr;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int JPGDatasetCommon::GetGCPCount()

{
    const int nPAMGCPCount = GDALPamDataset::GetGCPCount();
    if( nPAMGCPCount != 0 )
        return nPAMGCPCount;

    LoadWorldFileOrTab();

    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *JPGDatasetCommon::GetGCPProjection()

{
    const int nPAMGCPCount = GDALPamDataset::GetGCPCount();
    if( nPAMGCPCount != 0 )
        return GDALPamDataset::GetGCPProjection();

    LoadWorldFileOrTab();

    if( pszProjection && nGCPCount > 0 )
        return pszProjection;

    return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *JPGDatasetCommon::GetGCPs()

{
    const int nPAMGCPCount = GDALPamDataset::GetGCPCount();
    if( nPAMGCPCount != 0 )
        return GDALPamDataset::GetGCPs();

    LoadWorldFileOrTab();

    return pasGCPList;
}

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      Checks for what might be the most common read case              */
/*      (reading an entire 8bit, RGB JPEG), and                         */
/*      optimizes for that case                                         */
/************************************************************************/

CPLErr JPGDatasetCommon::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff,
                                    int nXSize, int nYSize,
                                    void *pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    int nBandCount, int *panBandMap,
                                    GSpacing nPixelSpace, GSpacing nLineSpace,
                                    GSpacing nBandSpace,
                                    GDALRasterIOExtraArg *psExtraArg )

{
    // Coverity says that we cannot pass a nullptr to IRasterIO.
    if (panBandMap == NULL)
    {
      return CE_Failure;
    }

    if((eRWFlag == GF_Read) &&
       (nBandCount == 3) &&
       (nBands == 3) &&
       (nXOff == 0) && (nYOff == 0) &&
       (nXSize == nBufXSize) && (nXSize == nRasterXSize) &&
       (nYSize == nBufYSize) && (nYSize == nRasterYSize) &&
       (eBufType == GDT_Byte) && (GetDataPrecision() != 12) &&
       (pData != NULL) &&
       (panBandMap != NULL) &&
       (panBandMap[0] == 1) && (panBandMap[1] == 2) && (panBandMap[2] == 3) &&
       // These color spaces need to be transformed to RGB.
       GetOutColorSpace() != JCS_YCCK && GetOutColorSpace() != JCS_CMYK )
    {
        Restart();

        // Pixel interleaved case.
        if( nBandSpace == 1 )
        {
            for(int y = 0; y < nYSize; ++y)
            {
                CPLErr tmpError = LoadScanline(y);
                if(tmpError != CE_None)
                    return tmpError;

                if( nPixelSpace == 3 )
                {
                    memcpy(&(((GByte *)pData)[(y * nLineSpace)]), pabyScanline,
                           3 * nXSize);
                }
                else
                {
                    for(int x = 0; x < nXSize; ++x)
                    {
                        memcpy(&(((GByte *)pData)[(y * nLineSpace) +
                                                  (x * nPixelSpace)]),
                               (const GByte *)&(pabyScanline[x * 3]), 3);
                    }
                }
            }
        }
        else
        {
            for(int y = 0; y < nYSize; ++y)
            {
                CPLErr tmpError = LoadScanline(y);
                if(tmpError != CE_None)
                    return tmpError;
                for(int x = 0; x < nXSize; ++x)
                {
                    static_cast<GByte *>(
                        pData)[(y * nLineSpace) + (x * nPixelSpace)] =
                        pabyScanline[x * 3];
                    ((GByte *)pData)[(y * nLineSpace) + (x * nPixelSpace) +
                                     nBandSpace] = pabyScanline[x * 3 + 1];
                    ((GByte *)pData)[(y * nLineSpace) + (x * nPixelSpace) +
                                     2 * nBandSpace] = pabyScanline[x * 3 + 2];
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

#if JPEG_LIB_VERSION_MAJOR < 9
/************************************************************************/
/*                    JPEGDatasetIsJPEGLS()                             */
/************************************************************************/

static int JPEGDatasetIsJPEGLS( GDALOpenInfo * poOpenInfo )

{
    GByte *pabyHeader = poOpenInfo->pabyHeader;
    int nHeaderBytes = poOpenInfo->nHeaderBytes;

    if( nHeaderBytes < 10 )
        return FALSE;

    if( pabyHeader[0] != 0xff || pabyHeader[1] != 0xd8 )
        return FALSE;

    for (int nOffset = 2; nOffset + 4 < nHeaderBytes;)
    {
        if (pabyHeader[nOffset] != 0xFF)
            return FALSE;

        int nMarker = pabyHeader[nOffset + 1];
        if (nMarker == 0xF7)  // JPEG Extension 7, JPEG-LS.
            return TRUE;
        if (nMarker == 0xF8)  // JPEG Extension 8, JPEG-LS Extension.
            return TRUE;
        if (nMarker == 0xC3)  // Start of Frame 3.
            return TRUE;
        if (nMarker == 0xC7)  // Start of Frame 7.
            return TRUE;
        if (nMarker == 0xCB)  // Start of Frame 11.
            return TRUE;
        if (nMarker == 0xCF)  // Start of Frame 15.
            return TRUE;

        nOffset += 2 + pabyHeader[nOffset + 2] * 256 + pabyHeader[nOffset + 3];
    }

    return FALSE;
}
#endif

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int JPGDatasetCommon::Identify( GDALOpenInfo *poOpenInfo )

{
    // If it is a subfile, read the JPEG header.
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "JPEG_SUBFILE:") )
        return TRUE;

    // First we check to see if the file has the expected header bytes.
    const int nHeaderBytes = poOpenInfo->nHeaderBytes;

    if( nHeaderBytes < 10 )
        return FALSE;

    GByte * const pabyHeader = poOpenInfo->pabyHeader;
    if( pabyHeader[0] != 0xff ||
        pabyHeader[1] != 0xd8 ||
        pabyHeader[2] != 0xff )
        return FALSE;

#if JPEG_LIB_VERSION_MAJOR < 9
    if (JPEGDatasetIsJPEGLS(poOpenInfo))
    {
        return FALSE;
    }
#endif

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JPGDatasetCommon::Open( GDALOpenInfo *poOpenInfo )

{
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if( !Identify(poOpenInfo) )
        return NULL;
#endif

    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The JPEG driver does not support update access to existing"
                 " datasets.");
        return NULL;
    }

    VSILFILE *fpL = poOpenInfo->fpL;
    poOpenInfo->fpL = NULL;

    JPGDatasetOpenArgs sArgs;
    sArgs.pszFilename = poOpenInfo->pszFilename;
    sArgs.fpLin = fpL;
    sArgs.papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    sArgs.nScaleFactor = 1;
    sArgs.bDoPAMInitialize = true;
    sArgs.bUseInternalOverviews =
        CPLFetchBool(poOpenInfo->papszOpenOptions,
                     "USE_INTERNAL_OVERVIEWS", true);

    return JPGDataset::Open(&sArgs);
}

#endif // !defined(JPGDataset)

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *JPGDataset::Open( JPGDatasetOpenArgs *psArgs )

{
    JPGDataset *poDS = new JPGDataset();
    return OpenStage2(psArgs, poDS);
}

GDALDataset *JPGDataset::OpenStage2( JPGDatasetOpenArgs *psArgs,
                                     JPGDataset *&poDS )
{
    // Will detect mismatch between compile-time and run-time libjpeg versions.
    if (setjmp(poDS->sErrorStruct.setjmp_buffer))
    {
#if defined(JPEG_DUAL_MODE_8_12) && !defined(JPGDataset)
        if (poDS->sDInfo.data_precision == 12 && poDS->fpImage != NULL)
        {
            VSILFILE *fpImage = poDS->fpImage;
            poDS->fpImage = NULL;
            delete poDS;
            psArgs->fpLin = fpImage;
            return JPEGDataset12Open(psArgs);
        }
#endif
        delete poDS;
        return NULL;
    }

    const char *pszFilename = psArgs->pszFilename;
    VSILFILE *fpLin = psArgs->fpLin;
    char **papszSiblingFiles = psArgs->papszSiblingFiles;
    const int nScaleFactor = psArgs->nScaleFactor;
    const bool bDoPAMInitialize = psArgs->bDoPAMInitialize;
    const bool bUseInternalOverviews = psArgs->bUseInternalOverviews;

    // If it is a subfile, read the JPEG header.
    bool bIsSubfile = false;
    GUIntBig subfile_offset = 0;
    GUIntBig subfile_size = 0;
    const char *real_filename = pszFilename;
    int nQLevel = -1;

    if( STARTS_WITH_CI(pszFilename, "JPEG_SUBFILE:") )
    {
        bool bScan = false;

        if( STARTS_WITH_CI(pszFilename, "JPEG_SUBFILE:Q") )
        {
            char **papszTokens = CSLTokenizeString2(pszFilename + 14, ",", 0);
            if (CSLCount(papszTokens) >= 3)
            {
                nQLevel = atoi(papszTokens[0]);
                subfile_offset = CPLScanUIntBig(
                    papszTokens[1], static_cast<int>(strlen(papszTokens[1])));
                subfile_size = CPLScanUIntBig(
                    papszTokens[2], static_cast<int>(strlen(papszTokens[2])));
                bScan = true;
            }
            CSLDestroy(papszTokens);
        }
        else
        {
            char **papszTokens = CSLTokenizeString2(pszFilename + 13, ",", 0);
            if (CSLCount(papszTokens) >= 2)
            {
                subfile_offset = CPLScanUIntBig(
                    papszTokens[0], static_cast<int>(strlen(papszTokens[0])));
                subfile_size = CPLScanUIntBig(
                    papszTokens[1], static_cast<int>(strlen(papszTokens[1])));
                bScan = true;
            }
            CSLDestroy(papszTokens);
        }

        if( !bScan )
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Corrupt subfile definition: %s",
                     pszFilename);
            delete poDS;
            return NULL;
        }

        real_filename = strstr(pszFilename, ",");
        if( real_filename != NULL )
            real_filename = strstr(real_filename + 1, ",");
        if( real_filename != NULL && nQLevel != -1 )
            real_filename = strstr(real_filename + 1, ",");
        if( real_filename != NULL )
            real_filename++;
        else
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Could not find filename in subfile definition.");
            delete poDS;
            return NULL;
        }

        CPLDebug("JPG",
                 "real_filename %s, offset=" CPL_FRMT_GUIB ", size="
                 CPL_FRMT_GUIB "\n",
                 real_filename, subfile_offset, subfile_size);

        bIsSubfile = true;
    }

    // Open the file using the large file api if necessary.
    VSILFILE *fpImage = fpLin;

    if( fpImage == NULL )
    {
        fpImage = VSIFOpenL(real_filename, "rb");

        if( fpImage == NULL )
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "VSIFOpenL(%s) failed unexpectedly in jpgdataset.cpp",
                     real_filename);
            delete poDS;
            return NULL;
        }
    }
    else
    {
        fpImage = fpLin;
    }

    // Create a corresponding GDALDataset.
    poDS->nQLevel = nQLevel;
    poDS->fpImage = fpImage;

    // Move to the start of jpeg data.
    poDS->nSubfileOffset = subfile_offset;
    VSIFSeekL(poDS->fpImage, poDS->nSubfileOffset, SEEK_SET);

    poDS->eAccess = GA_ReadOnly;

    poDS->sDInfo.err = jpeg_std_error(&poDS->sJErr);
    poDS->sJErr.error_exit = JPGDataset::ErrorExit;
    poDS->sErrorStruct.p_previous_emit_message = poDS->sJErr.emit_message;
    poDS->sJErr.emit_message = JPGDataset::EmitMessage;
    poDS->sDInfo.client_data = &poDS->sErrorStruct;

    jpeg_create_decompress(&poDS->sDInfo);
    poDS->bHasDoneJpegCreateDecompress = true;

    // This is to address bug related in ticket #1795.
    if( CPLGetConfigOption("JPEGMEM", NULL) == NULL )
    {
        // If the user doesn't provide a value for JPEGMEM, we want to be sure
        // that at least 500 MB will be used before creating the temporary file.
        const long nMinMemory = 500 * 1024 * 1024;
        poDS->sDInfo.mem->max_memory_to_use =
            std::max(poDS->sDInfo.mem->max_memory_to_use, nMinMemory);
    }

#if !defined(JPGDataset)
    // Preload default NITF JPEG quantization tables.
    poDS->LoadDefaultTables(0);
    poDS->LoadDefaultTables(1);
    poDS->LoadDefaultTables(2);
    poDS->LoadDefaultTables(3);
#endif  // !defined(JPGDataset)

    // Read pre-image data after ensuring the file is rewound.
    VSIFSeekL(poDS->fpImage, poDS->nSubfileOffset, SEEK_SET);

    jpeg_vsiio_src(&poDS->sDInfo, poDS->fpImage);
    jpeg_read_header(&poDS->sDInfo, TRUE);

    if( poDS->sDInfo.data_precision != 8
        && poDS->sDInfo.data_precision != 12 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDAL JPEG Driver doesn't support files with precision of "
                 "other than 8 or 12 bits.");
        delete poDS;
        return NULL;
    }

    // Capture some information from the file that is of interest.

    poDS->nScaleFactor = nScaleFactor;
    poDS->SetScaleNumAndDenom();
    poDS->nRasterXSize =
        (poDS->sDInfo.image_width + nScaleFactor - 1) / nScaleFactor;
    poDS->nRasterYSize =
        (poDS->sDInfo.image_height + nScaleFactor - 1) / nScaleFactor;

    poDS->sDInfo.out_color_space = poDS->sDInfo.jpeg_color_space;
    poDS->eGDALColorSpace = poDS->sDInfo.jpeg_color_space;

    if( poDS->sDInfo.jpeg_color_space == JCS_GRAYSCALE )
    {
        poDS->nBands = 1;
    }
    else if( poDS->sDInfo.jpeg_color_space == JCS_RGB )
    {
        poDS->nBands = 3;
    }
    else if( poDS->sDInfo.jpeg_color_space == JCS_YCbCr )
    {
        poDS->nBands = 3;
        if (CPLTestBool(CPLGetConfigOption("GDAL_JPEG_TO_RGB", "YES")))
        {
            poDS->sDInfo.out_color_space = JCS_RGB;
            poDS->eGDALColorSpace = JCS_RGB;
            poDS->SetMetadataItem(
                "SOURCE_COLOR_SPACE", "YCbCr", "IMAGE_STRUCTURE" );
        }
    }
    else if( poDS->sDInfo.jpeg_color_space == JCS_CMYK )
    {
        if (poDS->sDInfo.data_precision == 8 &&
            CPLTestBool(CPLGetConfigOption("GDAL_JPEG_TO_RGB", "YES")))
        {
            poDS->eGDALColorSpace = JCS_RGB;
            poDS->nBands = 3;
            poDS->SetMetadataItem(
                "SOURCE_COLOR_SPACE", "CMYK", "IMAGE_STRUCTURE" );
        }
        else
        {
            poDS->nBands = 4;
        }
    }
    else if( poDS->sDInfo.jpeg_color_space == JCS_YCCK )
    {
        if (poDS->sDInfo.data_precision == 8 &&
            CPLTestBool(CPLGetConfigOption("GDAL_JPEG_TO_RGB", "YES")))
        {
            poDS->eGDALColorSpace = JCS_RGB;
            poDS->nBands = 3;
            poDS->SetMetadataItem(
                "SOURCE_COLOR_SPACE", "YCbCrK", "IMAGE_STRUCTURE" );

            // libjpeg does the translation from YCrCbK -> CMYK internally
            // and we'll do the translation to RGB in IReadBlock().
            poDS->sDInfo.out_color_space = JCS_CMYK;
        }
        else
        {
            poDS->nBands = 4;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unrecognized jpeg_color_space value of %d.\n",
                 poDS->sDInfo.jpeg_color_space);
        delete poDS;
        return NULL;
    }

    // Create band information objects.
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
        poDS->SetBand(iBand + 1, JPGCreateBand(poDS, iBand + 1));

    // More metadata.
    if(poDS->nBands > 1)
    {
        poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        poDS->SetMetadataItem("COMPRESSION", "JPEG", "IMAGE_STRUCTURE");
    }

    // Initialize any PAM information.
    poDS->SetDescription(pszFilename);

    if( nScaleFactor == 1 && bDoPAMInitialize )
    {
        if( !bIsSubfile )
            poDS->TryLoadXML(papszSiblingFiles);
        else
            poDS->nPamFlags |= GPF_NOSAVE;

        // Open (external) overviews.
        poDS->oOvManager.Initialize(poDS, real_filename, papszSiblingFiles);

        if( !bUseInternalOverviews )
            poDS->bHasInitInternalOverviews = true;

        // In the case of a file downloaded through the HTTP driver, this one
        // will unlink the temporary /vsimem file just after GDALOpen(), so
        // later VSIFOpenL() when reading internal overviews would fail.
        // Initialize them now.
        if( STARTS_WITH(real_filename, "/vsimem/http_") )
        {
            poDS->InitInternalOverviews();
        }
    }
    else
    {
        poDS->nPamFlags |= GPF_NOSAVE;
    }

    poDS->bIsSubfile = bIsSubfile;

    return poDS;
}

#if !defined(JPGDataset)

/************************************************************************/
/*                       LoadWorldFileOrTab()                           */
/************************************************************************/

void JPGDatasetCommon::LoadWorldFileOrTab()
{
    if (bIsSubfile)
        return;
    if (bHasTriedLoadWorldFileOrTab)
        return;
    bHasTriedLoadWorldFileOrTab = true;

    char *pszWldFilename = NULL;

    // TIROS3 JPEG files have a .wld extension, so don't look for .wld as
    // as worldfile.
    const bool bEndsWithWld =
        strlen(GetDescription()) > 4 &&
        EQUAL(GetDescription() + strlen(GetDescription()) - 4, ".wld");
    bGeoTransformValid =
        GDALReadWorldFile2(GetDescription(), NULL, adfGeoTransform,
                           oOvManager.GetSiblingFiles(), &pszWldFilename) ||
        GDALReadWorldFile2(GetDescription(), ".jpw", adfGeoTransform,
                           oOvManager.GetSiblingFiles(), &pszWldFilename) ||
        (!bEndsWithWld &&
         GDALReadWorldFile2(GetDescription(), ".wld", adfGeoTransform,
                            oOvManager.GetSiblingFiles(), &pszWldFilename));

    if( !bGeoTransformValid )
    {
        const bool bTabFileOK = CPL_TO_BOOL(GDALReadTabFile2(
            GetDescription(), adfGeoTransform, &pszProjection, &nGCPCount,
            &pasGCPList, oOvManager.GetSiblingFiles(), &pszWldFilename));

        if( bTabFileOK && nGCPCount == 0 )
            bGeoTransformValid = true;
    }

    if (pszWldFilename)
    {
        osWldFilename = pszWldFilename;
        CPLFree(pszWldFilename);
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **JPGDatasetCommon::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    LoadWorldFileOrTab();

    if( !osWldFilename.empty() &&
        CSLFindString(papszFileList, osWldFilename) == -1 )
    {
        papszFileList = CSLAddString(papszFileList, osWldFilename);
    }

    return papszFileList;
}

/************************************************************************/
/*                            CheckForMask()                            */
/************************************************************************/

void JPGDatasetCommon::CheckForMask()

{
    // Save current position to avoid disturbing JPEG stream decoding.
    const vsi_l_offset nCurOffset = VSIFTellL(fpImage);

    // Go to the end of the file, pull off four bytes, and see if
    // it is plausibly the size of the real image data.
    VSIFSeekL(fpImage, 0, SEEK_END);
    GIntBig nFileSize = VSIFTellL(fpImage);
    VSIFSeekL(fpImage, nFileSize - 4, SEEK_SET);

    GUInt32 nImageSize = 0;
    VSIFReadL(&nImageSize, 4, 1, fpImage);
    CPL_LSBPTR32(&nImageSize);

    GByte abyEOD[2] = { 0, 0 };

    if( nImageSize < nFileSize / 2 || nImageSize > nFileSize - 4 )
        goto end;

    // If that seems okay, seek back, and verify that just preceding
    // the bitmask is an apparent end-of-jpeg-data marker.
    VSIFSeekL(fpImage, nImageSize - 2, SEEK_SET);
    VSIFReadL(abyEOD, 2, 1, fpImage);
    if( abyEOD[0] != 0xff || abyEOD[1] != 0xd9 )
        goto end;

    // We seem to have a mask.  Read it in.
    nCMaskSize = static_cast<int>(nFileSize - nImageSize - 4);
    pabyCMask = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nCMaskSize));
    if (pabyCMask == NULL)
    {
        goto end;
    }
    VSIFReadL(pabyCMask, nCMaskSize, 1, fpImage);

    CPLDebug("JPEG", "Got %d byte compressed bitmask.", nCMaskSize);

    // TODO(schwehr): Refactor to not use goto.
end:
    VSIFSeekL(fpImage, nCurOffset, SEEK_SET);
}

/************************************************************************/
/*                           DecompressMask()                           */
/************************************************************************/

void JPGDatasetCommon::DecompressMask()

{
    if( pabyCMask == NULL || pabyBitMask != NULL )
        return;

    // Allocate 1bit buffer - may be slightly larger than needed.
    const int nBufSize = nRasterYSize * ((nRasterXSize + 7) / 8);
    pabyBitMask = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nBufSize));
    if (pabyBitMask == NULL)
    {
        CPLFree(pabyCMask);
        pabyCMask = NULL;
        return;
    }

    // Decompress.
    void *pOut =
        CPLZLibInflate(pabyCMask, nCMaskSize, pabyBitMask, nBufSize, NULL);

    // Cleanup if an error occurs.
    if( pOut == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failure decoding JPEG validity bitmask.");
        CPLFree(pabyCMask);
        pabyCMask = NULL;

        CPLFree(pabyBitMask);
        pabyBitMask = NULL;

        return;
    }

    const char *pszJPEGMaskBitOrder =
        CPLGetConfigOption("JPEG_MASK_BIT_ORDER", "AUTO");
    if( EQUAL(pszJPEGMaskBitOrder, "LSB") )
        bMaskLSBOrder = true;
    else if( EQUAL(pszJPEGMaskBitOrder, "MSB") )
        bMaskLSBOrder = false;
    else if( nRasterXSize > 8 && nRasterYSize > 1 )
    {
        // Test MSB ordering hypothesis in a very restrictive case where it is
        // *obviously* ordered as MSB ! (unless someone coded something
        // specifically to defeat the below logic)
        // The case considered here is dop_465_6100.jpg from #5102.
        // The mask is identical for each line, starting with 1's and ending
        // with 0's (or starting with 0's and ending with 1's), and no other
        // intermediate change.
        // We can detect the MSB ordering since the lsb bits at the end of the
        // first line will be set with the 1's of the beginning of the second
        // line.
        // We can only be sure of this heuristics if the change of value occurs
        // in the middle of a byte, or if the raster width is not a multiple of
        // 8.
        //
        // TODO(schwehr): Check logic in this section that was added in r26063.
        int nPrevValBit = 0;
        int nChangedValBit = 0;
        int iX = 0;  // Used after for.
        for( ; iX < nRasterXSize; iX++ )
        {
            const int nValBit =
                (pabyBitMask[iX >> 3] & (0x1 << (7 - (iX & 7)))) != 0;
            if( iX == 0 )
                nPrevValBit = nValBit;
            else if( nValBit != nPrevValBit )
            {
                nPrevValBit = nValBit;
                nChangedValBit++;
                if( nChangedValBit == 1 )
                {
                    const bool bValChangedOnByteBoundary = (iX % 8) == 0;
                    if( bValChangedOnByteBoundary && (nRasterXSize % 8) == 0 )
                        break;
                }
                else
                {
                    break;
                }
            }
            const int iNextLineX = iX + nRasterXSize;
            const int nNextLineValBit = (pabyBitMask[iNextLineX >> 3] &
                                         (0x1 << (7 - (iNextLineX & 7)))) != 0;
            if( nValBit != nNextLineValBit )
                break;
        }

        if( iX == nRasterXSize )
        {
            CPLDebug("JPEG",
                     "Bit ordering in mask is guessed to be msb (unusual)");
            bMaskLSBOrder = false;
        }
        else
        {
            bMaskLSBOrder = true;
        }
    }
    else
    {
        bMaskLSBOrder = true;
    }
}

#endif  // !defined(JPGDataset)

/************************************************************************/
/*                             ErrorExit()                              */
/************************************************************************/

void JPGDataset::ErrorExit(j_common_ptr cinfo)
{
    GDALJPEGErrorStruct *psErrorStruct =
        static_cast<GDALJPEGErrorStruct *>(cinfo->client_data);
    char buffer[JMSG_LENGTH_MAX] = {};

    // Create the message.
    (*cinfo->err->format_message)(cinfo, buffer);

    // Avoid error for a 12bit JPEG if reading from the 8bit JPEG driver and
    // we have JPEG_DUAL_MODE_8_12 support, as we'll try again with 12bit JPEG
    // driver.
#if defined(JPEG_DUAL_MODE_8_12) && !defined(JPGDataset)
    if (strstr(buffer, "Unsupported JPEG data precision 12") == NULL)
#endif
    CPLError(CE_Failure, CPLE_AppDefined, "libjpeg: %s", buffer);

    // Return control to the setjmp point.
    longjmp(psErrorStruct->setjmp_buffer, 1);
}

/************************************************************************/
/*                             EmitMessage()                            */
/************************************************************************/

void JPGDataset::EmitMessage(j_common_ptr cinfo, int msg_level)
{
    GDALJPEGErrorStruct *psErrorStruct =
        static_cast<GDALJPEGErrorStruct *>(cinfo->client_data);
    if( msg_level >= 0 )  // Trace message.
    {
        if( psErrorStruct->p_previous_emit_message != NULL )
            psErrorStruct->p_previous_emit_message(cinfo, msg_level);
    }
    else
    {
        // Warning : libjpeg will try to recover but the image will be likely
        // corrupted.

        struct jpeg_error_mgr *err = cinfo->err;

        // It's a warning message.  Since corrupt files may generate many
        // warnings, the policy implemented here is to show only the first
        // warning, unless trace_level >= 3.
        if (err->num_warnings == 0 || err->trace_level >= 3)
        {
            char buffer[JMSG_LENGTH_MAX] = {};

            // Create the message.
            (*cinfo->err->format_message) (cinfo, buffer);

            if( CPLTestBool(
                   CPLGetConfigOption("GDAL_ERROR_ON_LIBJPEG_WARNING", "NO")) )
            {
                psErrorStruct->bNonFatalErrorEncountered = true;
                CPLError(CE_Failure, CPLE_AppDefined, "libjpeg: %s", buffer);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "libjpeg: %s (this warning can be turned as an error "
                         "by setting GDAL_ERROR_ON_LIBJPEG_WARNING to TRUE)",
                         buffer);
            }
        }

        // Always count warnings in num_warnings.
        err->num_warnings++;
    }
}


/************************************************************************/
/*                          ProgressMonitor()                           */
/************************************************************************/

/* Avoid the risk of denial-of-service on crafted JPEGs with an insane */
/* number of scans. */
/* See http://www.libjpeg-turbo.org/pmwiki/uploads/About/TwoIssueswiththeJPEGStandard.pdf */
void JPGDataset::ProgressMonitor(j_common_ptr cinfo)
{
    if (cinfo->is_decompressor)
    {
        const int scan_no =
            reinterpret_cast<j_decompress_ptr>(cinfo)->input_scan_number;
        const int MAX_SCANS = atoi(
            CPLGetConfigOption("GDAL_JPEG_MAX_ALLOWED_SCAN_NUMBER", "100"));
        if (scan_no >= MAX_SCANS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Scan number %d exceeds maximum scans (%d)",
                     scan_no, MAX_SCANS);

            GDALJPEGErrorStruct *psErrorStruct =
                static_cast<GDALJPEGErrorStruct *>(cinfo->client_data);
            // Return control to the setjmp point.
            longjmp(psErrorStruct->setjmp_buffer, 1);
        }
    }
}

#if !defined(JPGDataset)

/************************************************************************/
/*                           JPGAddICCProfile()                         */
/*                                                                      */
/*      This function adds an ICC profile to a JPEG file.               */
/************************************************************************/

void JPGAddICCProfile( void *pInfo,
                       const char *pszICCProfile,
                       my_jpeg_write_m_header p_jpeg_write_m_header,
                       my_jpeg_write_m_byte p_jpeg_write_m_byte )
{
    if( pszICCProfile == NULL )
        return;

    // Write out each segment of the ICC profile.
    char *pEmbedBuffer = CPLStrdup(pszICCProfile);
    GInt32 nEmbedLen = CPLBase64DecodeInPlace((GByte *)pEmbedBuffer);
    char *pEmbedPtr = pEmbedBuffer;
    char const *const paHeader = "ICC_PROFILE";
    int nSegments = (nEmbedLen + 65518) / 65519;
    int nSegmentID = 1;

    while( nEmbedLen != 0)
    {
        // 65535 - 16 bytes for header = 65519
        const int nChunkLen = (nEmbedLen > 65519) ? 65519 : nEmbedLen;
        nEmbedLen -= nChunkLen;

        // Write marker and length.
        p_jpeg_write_m_header(
            pInfo, JPEG_APP0 + 2,
            static_cast<unsigned int>(nChunkLen + 14));

        // Write identifier.
        for( int i = 0; i < 12; i++ )
            p_jpeg_write_m_byte(pInfo, paHeader[i]);

        // Write ID and max ID.
        p_jpeg_write_m_byte(pInfo, nSegmentID);
        p_jpeg_write_m_byte(pInfo, nSegments);

        // Write ICC Profile.
        for(int i = 0; i < nChunkLen; i++)
            p_jpeg_write_m_byte(pInfo, pEmbedPtr[i]);

        nSegmentID++;

        pEmbedPtr += nChunkLen;
    }

    CPLFree(pEmbedBuffer);
}

/************************************************************************/
/*                           JPGAppendMask()                            */
/*                                                                      */
/*      This function appends a zlib compressed bitmask to a JPEG       */
/*      file (or really any file) pulled from an existing mask band.    */
/************************************************************************/

// MSVC does not know that memset() has initialized sStream.
#ifdef _MSC_VER
#  pragma warning(disable:4701)
#endif

CPLErr JPGAppendMask( const char *pszJPGFilename, GDALRasterBand *poMask,
                      GDALProgressFunc pfnProgress, void * pProgressData )

{
    const int nXSize = poMask->GetXSize();
    const int nYSize = poMask->GetYSize();
    const int nBitBufSize = nYSize * ((nXSize + 7) / 8);
    CPLErr eErr = CE_None;

    // Allocate uncompressed bit buffer.
    GByte *pabyBitBuf =
        static_cast<GByte *>(VSI_CALLOC_VERBOSE(1, nBitBufSize));

    GByte *pabyMaskLine = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));
    if (pabyBitBuf == NULL || pabyMaskLine == NULL)
    {
        eErr = CE_Failure;
    }

    // No reason to set it to MSB, unless for debugging purposes
    // to be able to generate a unusual LSB ordered mask (#5102).
    const char *pszJPEGMaskBitOrder =
        CPLGetConfigOption("JPEG_WRITE_MASK_BIT_ORDER", "LSB");
    const bool bMaskLSBOrder = EQUAL(pszJPEGMaskBitOrder, "LSB");

    // Set bit buffer from mask band, scanline by scanline.
    GUInt32 iBit = 0;
    for( int iY = 0; eErr == CE_None && iY < nYSize; iY++ )
    {
        eErr = poMask->RasterIO(GF_Read, 0, iY, nXSize, 1,
                                pabyMaskLine, nXSize, 1, GDT_Byte,
                                0, 0, NULL);
        if( eErr != CE_None )
            break;

        if( bMaskLSBOrder )
        {
            for( int iX = 0; iX < nXSize; iX++ )
            {
                if( pabyMaskLine[iX] != 0 )
                    pabyBitBuf[iBit >> 3] |= (0x1 << (iBit & 7));

                iBit++;
            }
        }
        else
        {
            for( int iX = 0; iX < nXSize; iX++ )
            {
                if( pabyMaskLine[iX] != 0 )
                    pabyBitBuf[iBit >> 3] |= (0x1 << (7 - (iBit & 7)));

                iBit++;
            }
        }

        if( eErr == CE_None &&
            !pfnProgress((iY + 1) / static_cast<double>(nYSize),
                         NULL, pProgressData) )
        {
            eErr = CE_Failure;
            CPLError(CE_Failure, CPLE_UserInterrupt,
                     "User terminated JPGAppendMask()");
        }
    }

    CPLFree(pabyMaskLine);

    // Compress.
    GByte *pabyCMask = NULL;

    if( eErr == CE_None )
    {
        pabyCMask = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nBitBufSize + 30));
        if (pabyCMask == NULL)
        {
            eErr = CE_Failure;
        }
    }

    size_t nTotalOut = 0;
    if ( eErr == CE_None )
    {
        if( CPLZLibDeflate(pabyBitBuf, nBitBufSize, 9,
                           pabyCMask, nBitBufSize + 30,
                           &nTotalOut) == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Deflate compression of jpeg bit mask failed.");
            eErr = CE_Failure;
        }
    }

    // Write to disk, along with image file size.
    if( eErr == CE_None )
    {
        VSILFILE *fpOut = VSIFOpenL(pszJPGFilename, "r+");
        if( fpOut == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to open jpeg to append bitmask.");
            eErr = CE_Failure;
        }
        else
        {
            VSIFSeekL(fpOut, 0, SEEK_END);

            GUInt32 nImageSize = static_cast<GUInt32>(VSIFTellL(fpOut));
            CPL_LSBPTR32(&nImageSize);

            if( VSIFWriteL(pabyCMask, 1, nTotalOut, fpOut) != nTotalOut )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failure writing compressed bitmask.\n%s",
                         VSIStrerror(errno));
                eErr = CE_Failure;
            }
            else
            {
                VSIFWriteL(&nImageSize, 4, 1, fpOut);
            }

            VSIFCloseL(fpOut);
        }
    }

    CPLFree(pabyBitBuf);
    CPLFree(pabyCMask);

    return eErr;
}

/************************************************************************/
/*                          JPGAddEXIFOverview()                        */
/************************************************************************/

void   JPGAddEXIFOverview( GDALDataType eWorkDT,
                           GDALDataset *poSrcDS, char **papszOptions,
                           void *cinfo,
                           my_jpeg_write_m_header p_jpeg_write_m_header,
                           my_jpeg_write_m_byte p_jpeg_write_m_byte,
                           GDALDataset *(pCreateCopy)(
                               const char *, GDALDataset *,
                               int, char **,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData ))
{
    const int nBands = poSrcDS->GetRasterCount();
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    bool bGenerateEXIFThumbnail =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "EXIF_THUMBNAIL", "NO"));
    const char *pszThumbnailWidth =
        CSLFetchNameValue(papszOptions, "THUMBNAIL_WIDTH");
    const char *pszThumbnailHeight =
        CSLFetchNameValue(papszOptions, "THUMBNAIL_HEIGHT");
    int nOvrWidth = 0;
    int nOvrHeight = 0;
    if( pszThumbnailWidth == NULL && pszThumbnailHeight == NULL )
    {
        if( nXSize >= nYSize )
        {
            nOvrWidth = 128;
        }
        else
        {
            nOvrHeight = 128;
        }
    }
    if( pszThumbnailWidth != NULL )
    {
        nOvrWidth = atoi(pszThumbnailWidth);
        if( nOvrWidth < 32 )
            nOvrWidth = 32;
        if( nOvrWidth > 1024 )
            nOvrWidth = 1024;
    }
    if( pszThumbnailHeight != NULL )
    {
        nOvrHeight = atoi(pszThumbnailHeight);
        if( nOvrHeight < 32 )
            nOvrHeight = 32;
        if( nOvrHeight > 1024 )
            nOvrHeight = 1024;
    }
    if( nOvrWidth == 0 )
    {
        nOvrWidth = static_cast<int>(
            static_cast<GIntBig>(nOvrHeight) * nXSize / nYSize);
        if( nOvrWidth == 0 ) nOvrWidth = 1;
    }
    else if( nOvrHeight == 0 )
    {
        nOvrHeight =
            static_cast<int>(static_cast<GIntBig>(nOvrWidth) * nYSize / nXSize);
        if( nOvrHeight == 0 )
            nOvrHeight = 1;
    }

    if( bGenerateEXIFThumbnail && nXSize > nOvrWidth && nYSize > nOvrHeight )
    {
        GDALDataset *poMemDS =
            MEMDataset::Create("", nOvrWidth, nOvrHeight, nBands,
                               eWorkDT, NULL);
        GDALRasterBand **papoSrcBands = static_cast<GDALRasterBand **>(
            CPLMalloc(nBands * sizeof(GDALRasterBand *)));
        GDALRasterBand ***papapoOverviewBands = static_cast<GDALRasterBand ***>(
            CPLMalloc(nBands * sizeof(GDALRasterBand **)));
        for(int i = 0; i < nBands; i++)
        {
            papoSrcBands[i] = poSrcDS->GetRasterBand(i + 1);
            papapoOverviewBands[i] = static_cast<GDALRasterBand **>(
                CPLMalloc(sizeof(GDALRasterBand *)));
            papapoOverviewBands[i][0] = poMemDS->GetRasterBand(i + 1);
        }
        CPLErr eErr = GDALRegenerateOverviewsMultiBand(nBands, papoSrcBands,
                                                       1, papapoOverviewBands,
                                                       "AVERAGE", NULL, NULL);
        CPLFree(papoSrcBands);
        for(int i = 0; i < nBands; i++)
        {
            CPLFree(papapoOverviewBands[i]);
        }
        CPLFree(papapoOverviewBands);

        if( eErr != CE_None )
        {
            GDALClose(poMemDS);
            return;
        }

        CPLString osTmpFile(CPLSPrintf("/vsimem/ovrjpg%p", poMemDS));
        GDALDataset *poOutDS =
            pCreateCopy(osTmpFile, poMemDS, 0, NULL, GDALDummyProgress, NULL);
        const bool bExifOverviewSuccess = poOutDS != NULL;
        delete poOutDS;
        poOutDS = NULL;
        GDALClose(poMemDS);
        vsi_l_offset nJPEGIfByteCount = 0;
        GByte *pabyOvr = NULL;
        if( bExifOverviewSuccess )
            pabyOvr = VSIGetMemFileBuffer(osTmpFile, &nJPEGIfByteCount, TRUE);
        VSIUnlink(osTmpFile);

        const unsigned int nMarkerSize =
            6 + 16 + 5 * 12 + 4 + static_cast<unsigned int>(nJPEGIfByteCount);
        if( pabyOvr == NULL )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Could not generate EXIF overview");
        }
        else if( nMarkerSize < 65536 )
        {
            p_jpeg_write_m_header(cinfo, JPEG_APP0 + 1, nMarkerSize);
            p_jpeg_write_m_byte(cinfo, 'E');  // EXIF signature.
            p_jpeg_write_m_byte(cinfo, 'x');
            p_jpeg_write_m_byte(cinfo, 'i');
            p_jpeg_write_m_byte(cinfo, 'f');
            p_jpeg_write_m_byte(cinfo, '\0');
            p_jpeg_write_m_byte(cinfo, '\0');

            // TIFF little-endian signature.
            p_jpeg_write_m_byte(cinfo, TIFF_LITTLEENDIAN & 0xff);
            p_jpeg_write_m_byte(cinfo, TIFF_LITTLEENDIAN >> 8);
            p_jpeg_write_m_byte(cinfo, TIFF_VERSION);
            p_jpeg_write_m_byte(cinfo, 0x00);

            p_jpeg_write_m_byte(cinfo, 8);  // Offset of IFD0.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 0);  // Number of entries of IFD0.
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 14); // Offset of IFD1.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 5);  // Number of entries of IFD1.
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, JPEG_TIFF_IMAGEWIDTH & 0xff);
            p_jpeg_write_m_byte(cinfo, (JPEG_TIFF_IMAGEWIDTH >> 8) & 0xff);

            p_jpeg_write_m_byte(cinfo, TIFF_LONG);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 1);  // 1 value.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, nOvrWidth & 0xff);
            p_jpeg_write_m_byte(cinfo, nOvrWidth >> 8);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, JPEG_TIFF_IMAGEHEIGHT & 0xff);
            p_jpeg_write_m_byte(cinfo, JPEG_TIFF_IMAGEHEIGHT >> 8);

            p_jpeg_write_m_byte(cinfo, TIFF_LONG);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 1);  // 1 value.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, nOvrHeight & 0xff);
            p_jpeg_write_m_byte(cinfo, nOvrHeight >> 8);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, JPEG_TIFF_COMPRESSION & 0xff);
            p_jpeg_write_m_byte(cinfo, JPEG_TIFF_COMPRESSION >> 8);

            p_jpeg_write_m_byte(cinfo, TIFF_SHORT);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 1); // 1 value.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 6); // JPEG compression.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, JPEG_EXIF_JPEGIFOFSET & 0xff);
            p_jpeg_write_m_byte(cinfo, JPEG_EXIF_JPEGIFOFSET >> 8);

            p_jpeg_write_m_byte(cinfo, TIFF_LONG);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 1); // 1 value.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            const unsigned int nJPEGIfOffset = 16 + 5 * 12 + 4;
            p_jpeg_write_m_byte(cinfo, nJPEGIfOffset & 0xff);
            p_jpeg_write_m_byte(cinfo, nJPEGIfOffset >> 8);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, JPEG_EXIF_JPEGIFBYTECOUNT & 0xff);
            p_jpeg_write_m_byte(cinfo, JPEG_EXIF_JPEGIFBYTECOUNT >> 8);

            p_jpeg_write_m_byte(cinfo, TIFF_LONG);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(cinfo, 1); // 1 value.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            p_jpeg_write_m_byte(
                cinfo, static_cast<GByte>(nJPEGIfByteCount & 0xff));
            p_jpeg_write_m_byte(
                cinfo, static_cast<GByte>(nJPEGIfByteCount >> 8));
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            // Offset of IFD2 == 0 ==> end of TIFF directory.
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);
            p_jpeg_write_m_byte(cinfo, 0);

            for( int i = 0; i < static_cast<int>(nJPEGIfByteCount); i++ )
                p_jpeg_write_m_byte(cinfo, pabyOvr[i]);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot write EXIF thumbnail. "
                     "The size of the EXIF segment exceeds 65536 bytes");
        }
        CPLFree(pabyOvr);
    }
}

#endif  // !defined(JPGDataset)

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *
JPGDataset::CreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                        int bStrict, char ** papszOptions,
                        GDALProgressFunc pfnProgress, void *pProgressData )

{
    if( !pfnProgress(0.0, NULL, pProgressData) )
        return NULL;

    // Some some rudimentary checks.
    const int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 && nBands != 3 && nBands != 4 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "JPEG driver doesn't support %d bands.  Must be 1 (grey), "
                 "3 (RGB) or 4 bands.\n", nBands);

        return NULL;
    }

    if (nBands == 1 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != NULL)
    {
        CPLError(bStrict ? CE_Failure : CE_Warning, CPLE_NotSupported,
                 "JPEG driver ignores color table. "
                 "The source raster band will be considered as grey level.\n"
                 "Consider using color table expansion "
                 "(-expand option in gdal_translate)");
        if (bStrict)
            return NULL;
    }

    VSILFILE *fpImage = NULL;
    GDALJPEGErrorStruct sErrorStruct;
    sErrorStruct.bNonFatalErrorEncountered = false;
    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

#if defined(JPEG_LIB_MK1_OR_12BIT) || defined(JPEG_DUAL_MODE_8_12)
    if( eDT != GDT_Byte && eDT != GDT_UInt16 )
    {
        CPLError(bStrict ? CE_Failure : CE_Warning, CPLE_NotSupported,
                 "JPEG driver doesn't support data type %s. "
                 "Only eight and twelve bit bands supported (Mk1 libjpeg).\n",
                 GDALGetDataTypeName(
                     poSrcDS->GetRasterBand(1)->GetRasterDataType()));

        if( bStrict )
            return NULL;
    }

    if( eDT == GDT_UInt16 || eDT == GDT_Int16 )
    {
#if defined(JPEG_DUAL_MODE_8_12) && !defined(JPGDataset)
        return JPEGDataset12CreateCopy(pszFilename, poSrcDS,
                                       bStrict, papszOptions,
                                       pfnProgress, pProgressData);
#else
        eDT = GDT_UInt16;
#endif  // defined(JPEG_DUAL_MODE_8_12) && !defined(JPGDataset)
    }
    else
    {
        eDT = GDT_Byte;
    }

#else  // !(defined(JPEG_LIB_MK1_OR_12BIT) || defined(JPEG_DUAL_MODE_8_12))
    if( eDT != GDT_Byte )
    {
        CPLError(bStrict ? CE_Failure : CE_Warning, CPLE_NotSupported,
                 "JPEG driver doesn't support data type %s. "
                 "Only eight bit byte bands supported.\n",
                 GDALGetDataTypeName(
                     poSrcDS->GetRasterBand(1)->GetRasterDataType()));

        if( bStrict )
            return NULL;
    }

    eDT = GDT_Byte;  // force to 8bit.
#endif  // !(defined(JPEG_LIB_MK1_OR_12BIT) || defined(JPEG_DUAL_MODE_8_12))

    // What options has the caller selected?
    int nQuality = 75;
    if( CSLFetchNameValue(papszOptions, "QUALITY") != NULL )
    {
        nQuality = atoi(CSLFetchNameValue(papszOptions, "QUALITY"));
        if( nQuality < 10 || nQuality > 100 )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "QUALITY=%s is not a legal value in the range 10-100.",
                     CSLFetchNameValue(papszOptions, "QUALITY"));
            return NULL;
        }
    }

    // Create the dataset.
    fpImage = VSIFOpenL(pszFilename, "wb");
    if( fpImage == NULL )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Unable to create jpeg file %s.\n",
                 pszFilename);
        return NULL;
    }

    struct jpeg_compress_struct sCInfo;
    struct jpeg_error_mgr sJErr;
    GByte *pabyScanline;

    // Does the source have a mask?  If so, we will append it to the
    // jpeg file after the imagery.
    const int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
    const bool bAppendMask =
        !(nMaskFlags & GMF_ALL_VALID) &&
        (nBands == 1 || (nMaskFlags & GMF_PER_DATASET)) &&
        CPLFetchBool(papszOptions, "INTERNAL_MASK", true);

    // Nasty trick to avoid variable clobbering issues with setjmp/longjmp.
    return CreateCopyStage2(pszFilename, poSrcDS, papszOptions,
                            pfnProgress, pProgressData,
                            fpImage, eDT, nQuality,
                            bAppendMask,
                            sErrorStruct, sCInfo, sJErr, pabyScanline);
}

GDALDataset *
JPGDataset::CreateCopyStage2( const char *pszFilename, GDALDataset *poSrcDS,
                              char **papszOptions,
                              GDALProgressFunc pfnProgress,
                              void * pProgressData,
                              VSILFILE *fpImage,
                              GDALDataType eDT,
                              int nQuality,
                              bool bAppendMask,
                              GDALJPEGErrorStruct &sErrorStruct,
                              struct jpeg_compress_struct &sCInfo,
                              struct jpeg_error_mgr &sJErr,
                              GByte *&pabyScanline)

{
    if (setjmp(sErrorStruct.setjmp_buffer))
    {
        if( fpImage )
            VSIFCloseL(fpImage);
        return NULL;
    }

    // Initialize JPG access to the file.
    sCInfo.err = jpeg_std_error(&sJErr);
    sJErr.error_exit = JPGDataset::ErrorExit;
    sErrorStruct.p_previous_emit_message = sJErr.emit_message;
    sJErr.emit_message = JPGDataset::EmitMessage;
    sCInfo.client_data = &sErrorStruct;

    jpeg_create_compress(&sCInfo);
    if (setjmp(sErrorStruct.setjmp_buffer))
    {
        if( fpImage )
            VSIFCloseL(fpImage);
        jpeg_destroy_compress(&sCInfo);
        return NULL;
    }

    jpeg_vsiio_dest(&sCInfo, fpImage);

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();
    sCInfo.image_width = nXSize;
    sCInfo.image_height = nYSize;
    sCInfo.input_components = nBands;

    if( nBands == 3 )
        sCInfo.in_color_space = JCS_RGB;
    else if( nBands == 1 )
        sCInfo.in_color_space = JCS_GRAYSCALE;
    else
        sCInfo.in_color_space = JCS_UNKNOWN;

    jpeg_set_defaults(&sCInfo);

    // This is to address bug related in ticket #1795.
    if (CPLGetConfigOption("JPEGMEM", NULL) == NULL)
    {
        // If the user doesn't provide a value for JPEGMEM, we want to be sure
        // that at least 500 MB will be used before creating the temporary file.
        const long nMinMemory = 500 * 1024 * 1024;
        sCInfo.mem->max_memory_to_use =
            std::max(sCInfo.mem->max_memory_to_use, nMinMemory);
    }

    if( eDT == GDT_UInt16 )
    {
        sCInfo.data_precision = 12;
    }
    else
    {
        sCInfo.data_precision = 8;
    }

    const char *pszVal = CSLFetchNameValue(papszOptions, "ARITHMETIC");
    if( pszVal )
        sCInfo.arith_code = CPLTestBool(pszVal);

    // Optimized Huffman coding. Supposedly slower according to libjpeg doc
    // but no longer significant with today computer standards.
    if( !sCInfo.arith_code )
        sCInfo.optimize_coding = TRUE;

#if JPEG_LIB_VERSION_MAJOR >= 8 && \
      (JPEG_LIB_VERSION_MAJOR > 8 || JPEG_LIB_VERSION_MINOR >= 3)
    pszVal = CSLFetchNameValue(papszOptions, "BLOCK");
    if( pszVal )
        sCInfo.block_size = atoi(pszVal);
#endif

#if JPEG_LIB_VERSION_MAJOR >= 9
    pszVal = CSLFetchNameValue(papszOptions, "COLOR_TRANSFORM");
    if( pszVal )
    {
        sCInfo.color_transform =
            EQUAL(pszVal, "RGB1") ? JCT_SUBTRACT_GREEN : JCT_NONE;
        jpeg_set_colorspace(&sCInfo, JCS_RGB);
    }
    else
#endif

    // Mostly for debugging purposes.
    if( nBands == 3 && CPLTestBool(CPLGetConfigOption("JPEG_WRITE_RGB", "NO")) )
    {
        jpeg_set_colorspace(&sCInfo, JCS_RGB);
    }

#ifdef JPEG_LIB_MK1
    sCInfo.bits_in_jsample = sCInfo.data_precision;
    // Always force to 16 bit for JPEG_LIB_MK1
    const GDALDataType eWorkDT = GDT_UInt16;
#else
    const GDALDataType eWorkDT = eDT;
#endif

    jpeg_set_quality(&sCInfo, nQuality, TRUE);

    const bool bProgressive = CPLFetchBool(papszOptions, "PROGRESSIVE", false);
    if( bProgressive )
        jpeg_simple_progression(&sCInfo);

    jpeg_start_compress(&sCInfo, TRUE);

    JPGAddEXIFOverview(eWorkDT, poSrcDS, papszOptions,
                       &sCInfo,
                       (my_jpeg_write_m_header)jpeg_write_m_header,
                       (my_jpeg_write_m_byte)jpeg_write_m_byte,
                       CreateCopy);

    // Add comment if available.
    const char *pszComment = CSLFetchNameValue(papszOptions, "COMMENT");
    if( pszComment )
        jpeg_write_marker(&sCInfo, JPEG_COM,
                          reinterpret_cast<const JOCTET *>(pszComment),
                          static_cast<unsigned int>(strlen(pszComment)));

    // Save ICC profile if available.
    const char *pszICCProfile =
        CSLFetchNameValue(papszOptions, "SOURCE_ICC_PROFILE");
    if (pszICCProfile == NULL)
        pszICCProfile =
            poSrcDS->GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE");

    if (pszICCProfile != NULL)
        JPGAddICCProfile(&sCInfo, pszICCProfile,
                         (my_jpeg_write_m_header)jpeg_write_m_header,
                         (my_jpeg_write_m_byte)jpeg_write_m_byte);

    // Loop over image, copying image data.
    const int nWorkDTSize = GDALGetDataTypeSizeBytes(eWorkDT);
    pabyScanline =
        static_cast<GByte *>(CPLMalloc(nBands * nXSize * nWorkDTSize));

    if (setjmp(sErrorStruct.setjmp_buffer))
    {
        VSIFCloseL(fpImage);
        CPLFree(pabyScanline);
        jpeg_destroy_compress(&sCInfo);
        return NULL;
    }

    CPLErr eErr = CE_None;
    bool bClipWarn = false;
    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        eErr = poSrcDS->RasterIO(
            GF_Read, 0, iLine, nXSize, 1, pabyScanline, nXSize, 1, eWorkDT,
            nBands, NULL, nBands * nWorkDTSize, nBands * nXSize * nWorkDTSize,
            nWorkDTSize, NULL);

        // Clamp 16bit values to 12bit.
        if( nWorkDTSize == 2 )
        {
            GUInt16 *panScanline = reinterpret_cast<GUInt16 *>(pabyScanline);

            for( int iPixel = 0; iPixel < nXSize * nBands; iPixel++ )
            {
                if( panScanline[iPixel] > 4095 )
                {
                    panScanline[iPixel] = 4095;
                    if( !bClipWarn )
                    {
                        bClipWarn = true;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "One or more pixels clipped to fit "
                                 "12bit domain for jpeg output.");
                    }
                }
            }
        }

        JSAMPLE *ppSamples = reinterpret_cast<JSAMPLE *>(pabyScanline);

        if( eErr == CE_None )
            jpeg_write_scanlines(&sCInfo, &ppSamples, 1);

        if( eErr == CE_None &&
            !pfnProgress(
                (iLine + 1) / ((bAppendMask ? 2 : 1) *
                               static_cast<double>(nYSize)),
                NULL, pProgressData) )
        {
            eErr = CE_Failure;
            CPLError(CE_Failure, CPLE_UserInterrupt,
                     "User terminated CreateCopy()");
        }
    }

    // Cleanup and close.
    if( eErr == CE_None )
        jpeg_finish_compress(&sCInfo);
    jpeg_destroy_compress(&sCInfo);

    // Free scanline and image after jpeg_finish_compress since this could
    // cause a longjmp to occur.
    CPLFree(pabyScanline);

    VSIFCloseL(fpImage);

    if( eErr != CE_None )
    {
        VSIUnlink(pszFilename);
        return NULL;
    }

    // Append masks to the jpeg file if necessary.
    int nCloneFlags = GCIF_PAM_DEFAULT;
    if( bAppendMask )
    {
        CPLDebug("JPEG", "Appending Mask Bitmap");

        void *pScaledData =
            GDALCreateScaledProgress(0.5, 1, pfnProgress, pProgressData);
        eErr = JPGAppendMask(
            pszFilename, poSrcDS->GetRasterBand(1)->GetMaskBand(),
            GDALScaledProgress, pScaledData );
        GDALDestroyScaledProgress(pScaledData);
        nCloneFlags &= (~GCIF_MASK);

        if( eErr != CE_None )
        {
            VSIUnlink(pszFilename);
            return NULL;
        }
    }

    // Do we need a world file?
    if( CPLFetchBool(papszOptions, "WORLDFILE", false) )
    {
        double adfGeoTransform[6] = {};

        poSrcDS->GetGeoTransform(adfGeoTransform);
        GDALWriteWorldFile(pszFilename, "wld", adfGeoTransform);
    }

    // Re-open dataset, and copy any auxiliary pam information.

    // If writing to stdout, we can't reopen it, so return
    // a fake dataset to make the caller happy.
    if( CPLTestBool(CPLGetConfigOption("GDAL_OPEN_AFTER_COPY", "YES")) )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);

        JPGDatasetOpenArgs sArgs;
        sArgs.pszFilename = pszFilename;
        sArgs.fpLin = NULL;
        sArgs.papszSiblingFiles = NULL;
        sArgs.nScaleFactor = 1;
        sArgs.bDoPAMInitialize = true;
        sArgs.bUseInternalOverviews = true;

        JPGDataset *poDS = dynamic_cast<JPGDataset *>(Open(&sArgs));
        CPLPopErrorHandler();
        if( poDS )
        {
            poDS->CloneInfo(poSrcDS, nCloneFlags);
            return poDS;
        }

        CPLErrorReset();
    }

    JPGDataset *poJPG_DS = new JPGDataset();
    poJPG_DS->nRasterXSize = nXSize;
    poJPG_DS->nRasterYSize = nYSize;
    for(int i = 0; i < nBands; i++)
        poJPG_DS->SetBand(i + 1, JPGCreateBand(poJPG_DS, i + 1));
    return poJPG_DS;
}

/************************************************************************/
/*                         GDALRegister_JPEG()                          */
/************************************************************************/

#if !defined(JPGDataset)

char **GDALJPGDriver::GetMetadata( const char *pszDomain )
{
    GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);
    return GDALDriver::GetMetadata(pszDomain);
}

static void GDALJPEGIsArithmeticCodingAvailableErrorExit(j_common_ptr cinfo)
{
    jmp_buf *p_setjmp_buffer = static_cast<jmp_buf *>(cinfo->client_data);
    // Return control to the setjmp point.
    longjmp(*p_setjmp_buffer, 1);
}

// Runtime check if arithmetic coding is available.
static bool GDALJPEGIsArithmeticCodingAvailable()
{
    struct jpeg_compress_struct sCInfo;
    struct jpeg_error_mgr sJErr;
    jmp_buf setjmp_buffer;
    if (setjmp(setjmp_buffer))
    {
        jpeg_destroy_compress(&sCInfo);
        return false;
    }
    sCInfo.err = jpeg_std_error(&sJErr);
    sJErr.error_exit = GDALJPEGIsArithmeticCodingAvailableErrorExit;
    sCInfo.client_data = &setjmp_buffer;
    jpeg_create_compress(&sCInfo);
    // Hopefully nothing will be written.
    jpeg_stdio_dest(&sCInfo, stderr);
    sCInfo.image_width = 1;
    sCInfo.image_height = 1;
    sCInfo.input_components = 1;
    sCInfo.in_color_space = JCS_UNKNOWN;
    jpeg_set_defaults(&sCInfo);
    sCInfo.arith_code = TRUE;
    jpeg_start_compress(&sCInfo, FALSE);
    jpeg_abort_compress(&sCInfo);
    jpeg_destroy_compress(&sCInfo);

    return true;
}

const char *GDALJPGDriver::GetMetadataItem( const char *pszName,
                                            const char *pszDomain )
{
    if( pszName != NULL && EQUAL(pszName, GDAL_DMD_CREATIONOPTIONLIST) &&
        (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        GDALDriver::GetMetadataItem(pszName, pszDomain) == NULL )
    {
        CPLString osCreationOptions =
"<CreationOptionList>\n"
"   <Option name='PROGRESSIVE' type='boolean' description='whether to generate a progressive JPEG' default='NO'/>\n"
"   <Option name='QUALITY' type='int' description='good=100, bad=0, default=75'/>\n"
"   <Option name='WORLDFILE' type='boolean' description='whether to generate a worldfile' default='NO'/>\n"
"   <Option name='INTERNAL_MASK' type='boolean' description='whether to generate a validity mask' default='YES'/>\n";
        if( GDALJPEGIsArithmeticCodingAvailable() )
            osCreationOptions +=
"   <Option name='ARITHMETIC' type='boolean' description='whether to use arithmetic encoding' default='NO'/>\n";
    osCreationOptions +=
#if JPEG_LIB_VERSION_MAJOR >= 8 && \
      (JPEG_LIB_VERSION_MAJOR > 8 || JPEG_LIB_VERSION_MINOR >= 3)
"   <Option name='BLOCK' type='int' description='between 1 and 16'/>\n"
#endif
#if JPEG_LIB_VERSION_MAJOR >= 9
"   <Option name='COLOR_TRANSFORM' type='string-select'>\n"
"       <Value>RGB</Value>"
"       <Value>RGB1</Value>"
"   </Option>"
#endif
"   <Option name='COMMENT' description='Comment' type='string'/>\n"
"   <Option name='SOURCE_ICC_PROFILE' description='ICC profile encoded in Base64' type='string'/>\n"
"   <Option name='EXIF_THUMBNAIL' type='boolean' description='whether to generate an EXIF thumbnail(overview). By default its max dimension will be 128' default='NO'/>\n"
"   <Option name='THUMBNAIL_WIDTH' type='int' description='Forced thumbnail width' min='32' max='512'/>\n"
"   <Option name='THUMBNAIL_HEIGHT' type='int' description='Forced thumbnail height' min='32' max='512'/>\n"
"</CreationOptionList>\n";
        SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST, osCreationOptions);
    }
    return GDALDriver::GetMetadataItem(pszName, pszDomain);
}

void GDALRegister_JPEG()

{
    if( GDALGetDriverByName("JPEG") != NULL )
        return;

    GDALDriver *poDriver = new GDALJPGDriver();

    poDriver->SetDescription("JPEG");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "JPEG JFIF");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_jpeg.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jpg");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "jpg jpeg");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jpeg");

#if defined(JPEG_LIB_MK1_OR_12BIT) || defined(JPEG_DUAL_MODE_8_12)
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte UInt16");
#else
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");
#endif
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>\n"
"   <Option name='USE_INTERNAL_OVERVIEWS' type='boolean' description='whether to use implicit internal overviews' default='YES'/>\n"
"</OpenOptionList>\n");

    poDriver->pfnIdentify = JPGDatasetCommon::Identify;
    poDriver->pfnOpen = JPGDatasetCommon::Open;
    poDriver->pfnCreateCopy = JPGDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
#endif
