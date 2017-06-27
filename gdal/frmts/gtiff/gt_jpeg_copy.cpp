/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Specialized copy of JPEG content into TIFF.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "gt_jpeg_copy.h"

#include "cpl_vsi.h"

#if defined(JPEG_DIRECT_COPY) || defined(HAVE_LIBJPEG)
#  include "vrt/vrtdataset.h"
#endif

#ifndef BIGTIFF_SUPPORT
#define tmsize_t tsize_t
#endif

#include <algorithm>

// Note: JPEG_DIRECT_COPY is not defined by default, because it is mainly
// useful for debugging purposes.

CPL_CVSID("$Id$")

#if defined(JPEG_DIRECT_COPY) || defined(HAVE_LIBJPEG)

/************************************************************************/
/*                      GetUnderlyingDataset()                          */
/************************************************************************/

static GDALDataset* GetUnderlyingDataset( GDALDataset* poSrcDS )
{
    // Test if we can directly copy original JPEG content if available.
    if( poSrcDS->GetDriver() != NULL &&
        poSrcDS->GetDriver() == GDALGetDriverByName("VRT") )
    {
        VRTDataset* poVRTDS = (VRTDataset* )poSrcDS;
        poSrcDS = poVRTDS->GetSingleSimpleSource();
    }

    return poSrcDS;
}

#endif // defined(JPEG_DIRECT_COPY) || defined(HAVE_LIBJPEG)

#ifdef JPEG_DIRECT_COPY

/************************************************************************/
/*                        IsBaselineDCTJPEG()                           */
/************************************************************************/

static bool IsBaselineDCTJPEG(VSILFILE* fp)
{
    GByte abyBuf[4] = { 0 };

    if( VSIFReadL(abyBuf, 1, 2, fp) != 2 ||
        abyBuf[0] != 0xff || abyBuf[1] != 0xd8 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Not a valid JPEG file" );
        return false;
    }

    int nOffset = 2;
    while( true )
    {
        VSIFSeekL(fp, nOffset, SEEK_SET);
        if( VSIFReadL(abyBuf, 1, 4, fp) != 4 ||
            abyBuf[0] != 0xFF )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Not a valid JPEG file" );
            return false;
        }

        const int nMarker = abyBuf[1];

        // Start of Frame 0 = Baseline DCT.
        if( nMarker == 0xC0 )
            return true;

        if( nMarker == 0xD9 )
            return false;

        if( nMarker == 0xF7 ||  // JPEG Extension 7, JPEG-LS
            nMarker == 0xF8 ||  // JPEG Extension 8, JPEG-LS Extension.
            // Other Start of Frames that we don't want to support.
            (nMarker >= 0xC1 && nMarker <= 0xCF) )
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Unsupported type of JPEG file for JPEG_DIRECT_COPY mode" );
            return false;
        }

        nOffset += 2 + abyBuf[2] * 256 + abyBuf[3];
    }
}

/************************************************************************/
/*                    GTIFF_CanDirectCopyFromJPEG()                     */
/************************************************************************/

int GTIFF_CanDirectCopyFromJPEG( GDALDataset* poSrcDS,
                                 char** &papszCreateOptions )
{
    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if( poSrcDS == NULL )
        return FALSE;
    if( poSrcDS->GetDriver() == NULL )
        return FALSE;
    if( !EQUAL(GDALGetDriverShortName(poSrcDS->GetDriver()), "JPEG") )
        return FALSE;

    const char* pszCompress = CSLFetchNameValue(papszCreateOptions, "COMPRESS");
    if(pszCompress != NULL && !EQUAL(pszCompress, "JPEG") )
        return FALSE;

    const char* pszSrcColorSpace =
        poSrcDS->GetMetadataItem("SOURCE_COLOR_SPACE", "IMAGE_STRUCTURE");
    if( pszSrcColorSpace != NULL &&
        (EQUAL(pszSrcColorSpace, "CMYK") || EQUAL(pszSrcColorSpace, "YCbCrK")) )
        return FALSE;

    bool bJPEGDirectCopy = false;

    VSILFILE* fpJPEG = VSIFOpenL(poSrcDS->GetDescription(), "rb");
    if( fpJPEG && IsBaselineDCTJPEG(fpJPEG) )
    {
        bJPEGDirectCopy = true;

        if(pszCompress == NULL )
            papszCreateOptions =
                CSLSetNameValue(papszCreateOptions, "COMPRESS", "JPEG");

        papszCreateOptions =
            CSLSetNameValue(papszCreateOptions, "BLOCKXSIZE", NULL);
        papszCreateOptions =
            CSLSetNameValue(papszCreateOptions, "BLOCKYSIZE",
                             CPLSPrintf("%d", poSrcDS->GetRasterYSize()));

        if( pszSrcColorSpace != NULL && EQUAL(pszSrcColorSpace, "YCbCr") )
            papszCreateOptions =
                CSLSetNameValue(papszCreateOptions, "PHOTOMETRIC", "YCBCR");
        else
            papszCreateOptions =
                CSLSetNameValue(papszCreateOptions, "PHOTOMETRIC", NULL);

        if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte )
            papszCreateOptions =
                CSLSetNameValue(papszCreateOptions, "NBITS", "12");
        else
            papszCreateOptions =
                CSLSetNameValue(papszCreateOptions, "NBITS", NULL);

        papszCreateOptions = CSLSetNameValue(papszCreateOptions, "TILED", NULL);
        papszCreateOptions =
            CSLSetNameValue(papszCreateOptions, "JPEG_QUALITY", NULL);
    }
    if( fpJPEG )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpJPEG));
    }

    return bJPEGDirectCopy;
}

/************************************************************************/
/*                     GTIFF_DirectCopyFromJPEG()                       */
/************************************************************************/

CPLErr GTIFF_DirectCopyFromJPEG( GDALDataset* poDS, GDALDataset* poSrcDS,
                                 GDALProgressFunc pfnProgress,
                                 void * pProgressData,
                                 int& bShouldFallbackToNormalCopyIfFail )
{
    bShouldFallbackToNormalCopyIfFail = TRUE;

    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if( poSrcDS == NULL )
        return CE_Failure;

    VSILFILE* fpJPEG = VSIFOpenL(poSrcDS->GetDescription(), "rb");
    if( fpJPEG == NULL )
        return CE_Failure;

    CPLErr eErr = CE_None;

    VSIFSeekL(fpJPEG, 0, SEEK_END);
    tmsize_t nSize = static_cast<tmsize_t>( VSIFTellL(fpJPEG) );
    VSIFSeekL(fpJPEG, 0, SEEK_SET);

    void* pabyJPEGData = VSIMalloc(nSize);
    if( pabyJPEGData == NULL )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpJPEG));
        return CE_Failure;
    }

    if( pabyJPEGData != NULL &&
        static_cast<tmsize_t>( VSIFReadL(pabyJPEGData, 1, nSize, fpJPEG) ) ==
        nSize )
    {
        bShouldFallbackToNormalCopyIfFail = FALSE;

        TIFF* hTIFF = (TIFF*) poDS->GetInternalHandle(NULL);
        if( TIFFWriteRawStrip(hTIFF, 0, pabyJPEGData, nSize) != nSize )
            eErr = CE_Failure;

        if( !pfnProgress( 1.0, NULL, pProgressData ) )
            eErr = CE_Failure;
    }
    else
    {
        eErr = CE_Failure;
    }

    VSIFree(pabyJPEGData);
    if( VSIFCloseL(fpJPEG) != 0 )
        eErr = CE_Failure;

    return eErr;
}

#endif // JPEG_DIRECT_COPY

#ifdef HAVE_LIBJPEG

#include "vsidataio.h"

#include <setjmp.h>

/*
 * We are using width_in_blocks which is supposed to be private to
 * libjpeg. Unfortunately, the libjpeg delivered with Cygwin has
 * renamed this member to width_in_data_units.  Since the header has
 * also renamed a define, use that unique define name in order to
 * detect the problem header and adjust to suit.
 */
#if defined(D_MAX_DATA_UNITS_IN_MCU)
#define width_in_blocks width_in_data_units
#endif

/************************************************************************/
/*                      GTIFF_CanCopyFromJPEG()                         */
/************************************************************************/

int GTIFF_CanCopyFromJPEG( GDALDataset* poSrcDS, char** &papszCreateOptions )
{
    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if( poSrcDS == NULL )
        return FALSE;
    if( poSrcDS->GetDriver() == NULL )
        return FALSE;
    if( !EQUAL(GDALGetDriverShortName(poSrcDS->GetDriver()), "JPEG") )
        return FALSE;

    const char* pszCompress = CSLFetchNameValue(papszCreateOptions, "COMPRESS");
    if( pszCompress == NULL || !EQUAL(pszCompress, "JPEG") )
        return FALSE;

    const int nBlockXSize =
        atoi(CSLFetchNameValueDef(papszCreateOptions, "BLOCKXSIZE", "0"));
    const int nBlockYSize =
        atoi(CSLFetchNameValueDef(papszCreateOptions, "BLOCKYSIZE", "0"));
    int nMCUSize = 8;
    const char* pszSrcColorSpace =
        poSrcDS->GetMetadataItem("SOURCE_COLOR_SPACE", "IMAGE_STRUCTURE");
    if( pszSrcColorSpace != NULL && EQUAL(pszSrcColorSpace, "YCbCr") )
        nMCUSize = 16;

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();

    const char* pszPhotometric =
        CSLFetchNameValue(papszCreateOptions, "PHOTOMETRIC");

    const bool bCompatiblePhotometric =
        pszPhotometric == NULL ||
        (nMCUSize == 16 && EQUAL(pszPhotometric, "YCbCr")) ||
        (nMCUSize == 8 && nBands == 4 &&
         poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_CyanBand &&
         poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
         GCI_MagentaBand &&
         poSrcDS->GetRasterBand(3)->GetColorInterpretation() ==
         GCI_YellowBand &&
         poSrcDS->GetRasterBand(4)->GetColorInterpretation() ==
         GCI_BlackBand) ||
        (nMCUSize == 8 && EQUAL(pszPhotometric, "RGB") && nBands == 3) ||
        (nMCUSize == 8 && EQUAL(pszPhotometric, "MINISBLACK") && nBands == 1);
    if( !bCompatiblePhotometric )
        return FALSE;

    if( nBands == 4 && pszPhotometric == NULL &&
         poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_CyanBand &&
         poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
         GCI_MagentaBand &&
         poSrcDS->GetRasterBand(3)->GetColorInterpretation() ==
         GCI_YellowBand &&
         poSrcDS->GetRasterBand(4)->GetColorInterpretation() == GCI_BlackBand )
    {
        papszCreateOptions =
            CSLSetNameValue(papszCreateOptions, "PHOTOMETRIC", "CMYK");
    }

    const char* pszInterleave =
        CSLFetchNameValue(papszCreateOptions, "INTERLEAVE");

    const bool bCompatibleInterleave =
        pszInterleave == NULL ||
        (nBands > 1 && EQUAL(pszInterleave, "PIXEL")) ||
        nBands == 1;
    if( !bCompatibleInterleave )
        return FALSE;

    if( (nBlockXSize == nXSize || (nBlockXSize % nMCUSize) == 0) &&
         (nBlockYSize == nYSize || (nBlockYSize % nMCUSize) == 0) &&
         poSrcDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte &&
         CSLFetchNameValue(papszCreateOptions, "NBITS") == NULL &&
         CSLFetchNameValue(papszCreateOptions, "JPEG_QUALITY") == NULL )
    {
        if( nMCUSize == 16 && pszPhotometric == NULL )
            papszCreateOptions =
                CSLSetNameValue(papszCreateOptions, "PHOTOMETRIC", "YCBCR");
        return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                      GTIFF_ErrorExitJPEG()                           */
/************************************************************************/

static void GTIFF_ErrorExitJPEG( j_common_ptr cinfo )
{
    jmp_buf *setjmp_buffer = (jmp_buf *) cinfo->client_data;
    char buffer[JMSG_LENGTH_MAX] = { '\0' };

    // Create the message.
    (*cinfo->err->format_message) (cinfo, buffer);

    CPLError( CE_Failure, CPLE_AppDefined,
              "libjpeg: %s", buffer );

    // Return control to the setjmp point.
    longjmp(*setjmp_buffer, 1);
}

/************************************************************************/
/*                      GTIFF_Set_TIFFTAG_JPEGTABLES()                  */
/************************************************************************/

static
void GTIFF_Set_TIFFTAG_JPEGTABLES( TIFF* hTIFF,
                                   jpeg_decompress_struct& sDInfo,
                                   jpeg_compress_struct& sCInfo )
{
    char szTmpFilename[128] = { '\0' };
    snprintf(szTmpFilename, sizeof(szTmpFilename),
             "/vsimem/tables_%p", &sDInfo);
    VSILFILE* fpTABLES = VSIFOpenL(szTmpFilename, "wb+");

    uint16 nPhotometric = 0;
    TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &nPhotometric );

    jpeg_vsiio_dest( &sCInfo, fpTABLES );

    // Avoid unnecessary tables to be emitted.
    if( nPhotometric != PHOTOMETRIC_YCBCR )
    {
        JQUANT_TBL* qtbl = sCInfo.quant_tbl_ptrs[1];
        if( qtbl != NULL )
            qtbl->sent_table = TRUE;
        JHUFF_TBL* htbl = sCInfo.dc_huff_tbl_ptrs[1];
        if( htbl != NULL )
            htbl->sent_table = TRUE;
        htbl = sCInfo.ac_huff_tbl_ptrs[1];
        if( htbl != NULL )
            htbl->sent_table = TRUE;
    }
    jpeg_write_tables( &sCInfo );

    CPL_IGNORE_RET_VAL(VSIFCloseL(fpTABLES));

    vsi_l_offset nSizeTables = 0;
    GByte* pabyJPEGTablesData =
        VSIGetMemFileBuffer(szTmpFilename, &nSizeTables, FALSE);
    TIFFSetField( hTIFF, TIFFTAG_JPEGTABLES,
                  static_cast<int>(nSizeTables),
                  pabyJPEGTablesData );

    VSIUnlink(szTmpFilename);
}

/************************************************************************/
/*             GTIFF_CopyFromJPEG_WriteAdditionalTags()                 */
/************************************************************************/

CPLErr GTIFF_CopyFromJPEG_WriteAdditionalTags( TIFF* hTIFF,
                                               GDALDataset* poSrcDS )
{
    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if( poSrcDS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Write TIFFTAG_JPEGTABLES                                        */
/* -------------------------------------------------------------------- */

    VSILFILE* fpJPEG = VSIFOpenL(poSrcDS->GetDescription(), "rb");
    if( fpJPEG == NULL )
        return CE_Failure;

    struct jpeg_error_mgr sJErr;
    struct jpeg_decompress_struct sDInfo;
    jmp_buf setjmp_buffer;
    if( setjmp(setjmp_buffer) )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpJPEG));
        return CE_Failure;
    }

    sDInfo.err = jpeg_std_error( &sJErr );
    sJErr.error_exit = GTIFF_ErrorExitJPEG;
    sDInfo.client_data = (void *) &setjmp_buffer;

    jpeg_create_decompress(&sDInfo);

    jpeg_vsiio_src( &sDInfo, fpJPEG );
    jpeg_read_header( &sDInfo, TRUE );

    struct jpeg_compress_struct sCInfo;

    sCInfo.err = jpeg_std_error( &sJErr );
    sJErr.error_exit = GTIFF_ErrorExitJPEG;
    sCInfo.client_data = (void *) &setjmp_buffer;

    jpeg_create_compress(&sCInfo);
    jpeg_copy_critical_parameters(&sDInfo, &sCInfo);
    GTIFF_Set_TIFFTAG_JPEGTABLES(hTIFF, sDInfo, sCInfo);
    jpeg_abort_compress(&sCInfo);
    jpeg_destroy_compress(&sCInfo);

/* -------------------------------------------------------------------- */
/*      Write TIFFTAG_REFERENCEBLACKWHITE if needed.                    */
/* -------------------------------------------------------------------- */

    uint16 nPhotometric = 0;
    if( !TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric) ) )
        nPhotometric = PHOTOMETRIC_MINISBLACK;

    uint16 nBitsPerSample = 0;
    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(nBitsPerSample)) )
        nBitsPerSample = 1;

    if( nPhotometric == PHOTOMETRIC_YCBCR )
    {
        /*
         * A ReferenceBlackWhite field *must* be present since the
         * default value is inappropriate for YCbCr.  Fill in the
         * proper value if application didn't set it.
         */
        float *ref = NULL;
        if( !TIFFGetField(hTIFF, TIFFTAG_REFERENCEBLACKWHITE, &ref) )
        {
            long top = 1L << nBitsPerSample;
            float refbw[6] = { 0.0 };
            refbw[1] = static_cast<float>(top - 1L);
            refbw[2] = static_cast<float>(top >> 1);
            refbw[3] = refbw[1];
            refbw[4] = refbw[2];
            refbw[5] = refbw[1];
            TIFFSetField( hTIFF, TIFFTAG_REFERENCEBLACKWHITE,
                          refbw );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write TIFFTAG_YCBCRSUBSAMPLING if needed.                       */
/* -------------------------------------------------------------------- */

    if( nPhotometric == PHOTOMETRIC_YCBCR && sDInfo.num_components == 3 )
    {
        if( (sDInfo.comp_info[0].h_samp_factor == 1 ||
             sDInfo.comp_info[0].h_samp_factor == 2) &&
            (sDInfo.comp_info[0].v_samp_factor == 1 ||
             sDInfo.comp_info[0].v_samp_factor == 2) &&
            sDInfo.comp_info[1].h_samp_factor == 1 &&
            sDInfo.comp_info[1].v_samp_factor == 1 &&
            sDInfo.comp_info[2].h_samp_factor == 1 &&
            sDInfo.comp_info[2].v_samp_factor == 1 )
        {
            TIFFSetField(hTIFF, TIFFTAG_YCBCRSUBSAMPLING,
                         sDInfo.comp_info[0].h_samp_factor,
                         sDInfo.comp_info[0].v_samp_factor);
        }
        else
        {
            CPLDebug(
                "GTiff",
                "Unusual sampling factors. "
                "TIFFTAG_YCBCRSUBSAMPLING not written." );
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */

    jpeg_abort_decompress( &sDInfo );
    jpeg_destroy_decompress( &sDInfo );

    if( VSIFCloseL(fpJPEG) != 0 )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                    GTIFF_CopyBlockFromJPEG()                         */
/************************************************************************/

typedef struct
{
    TIFF* hTIFF;
    jpeg_decompress_struct* psDInfo;
    int iX;
    int iY;
    int nXBlocks;
    int nXSize;
    int nYSize;
    int nBlockXSize;
    int nBlockYSize;
    int iMCU_sample_width;
    int iMCU_sample_height;
    jvirt_barray_ptr *pSrcCoeffs;
} GTIFF_CopyBlockFromJPEGArgs;

static CPLErr GTIFF_CopyBlockFromJPEG( GTIFF_CopyBlockFromJPEGArgs* psArgs )
{
    CPLString osTmpFilename(CPLSPrintf("/vsimem/%p", psArgs->psDInfo));
    VSILFILE* fpMEM = VSIFOpenL(osTmpFilename, "wb+");

/* -------------------------------------------------------------------- */
/*      Initialization of the compressor                                */
/* -------------------------------------------------------------------- */
    jmp_buf setjmp_buffer;
    if( setjmp(setjmp_buffer) )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpMEM));
        VSIUnlink(osTmpFilename);
        return CE_Failure;
    }

    TIFF* hTIFF = psArgs->hTIFF;
    jpeg_decompress_struct* psDInfo = psArgs->psDInfo;
    const int iX = psArgs->iX;
    const int iY = psArgs->iY;
    const int nXBlocks = psArgs->nXBlocks;
    const int nXSize = psArgs->nXSize;
    const int nYSize = psArgs->nYSize;
    const int nBlockXSize = psArgs->nBlockXSize;
    const int nBlockYSize = psArgs->nBlockYSize;
    const int iMCU_sample_width = psArgs->iMCU_sample_width;
    const int iMCU_sample_height = psArgs->iMCU_sample_height;
    jvirt_barray_ptr *pSrcCoeffs = psArgs->pSrcCoeffs;

    struct jpeg_error_mgr sJErr;
    struct jpeg_compress_struct sCInfo;
    sCInfo.err = jpeg_std_error( &sJErr );
    sJErr.error_exit = GTIFF_ErrorExitJPEG;
    sCInfo.client_data = (void *) &setjmp_buffer;

    // Initialize destination compression parameters from source values.
    jpeg_create_compress(&sCInfo);
    jpeg_copy_critical_parameters(psDInfo, &sCInfo);

    // Ensure libjpeg won't write any extraneous markers.
    sCInfo.write_JFIF_header = FALSE;
    sCInfo.write_Adobe_marker = FALSE;

/* -------------------------------------------------------------------- */
/*      Allocated destination coefficient array                         */
/* -------------------------------------------------------------------- */
    const bool bIsTiled = CPL_TO_BOOL(TIFFIsTiled(hTIFF));

    int nJPEGWidth = nBlockXSize;
    int nJPEGHeight = nBlockYSize;
    if( !bIsTiled )
    {
        nJPEGWidth = std::min(nBlockXSize, nXSize - iX * nBlockXSize);
        nJPEGHeight = std::min(nBlockYSize, nYSize - iY * nBlockYSize);
    }

    // Code partially derived from libjpeg transupp.c.

    // Correct the destination's image dimensions as necessary.
    #if JPEG_LIB_VERSION >= 70
    sCInfo.jpeg_width = nJPEGWidth;
    sCInfo.jpeg_height = nJPEGHeight;
    #else
    sCInfo.image_width = nJPEGWidth;
    sCInfo.image_height = nJPEGHeight;
    #endif

    // Save x/y offsets measured in iMCUs.
    const int x_crop_offset = (iX * nBlockXSize) / iMCU_sample_width;
    const int y_crop_offset = (iY * nBlockYSize) / iMCU_sample_height;

    jvirt_barray_ptr* pDstCoeffs = (jvirt_barray_ptr *)
        (*sCInfo.mem->alloc_small) ((j_common_ptr) &sCInfo, JPOOL_IMAGE,
                                    sizeof(jvirt_barray_ptr) * sCInfo.num_components);

    for( int ci = 0; ci < sCInfo.num_components; ci++ )
    {
        jpeg_component_info *compptr = sCInfo.comp_info + ci;
        int h_samp_factor, v_samp_factor;
        if( sCInfo.num_components == 1 )
        {
            // Force samp factors to 1x1 in this case.
            h_samp_factor = 1;
            v_samp_factor = 1;
        }
        else
        {
            h_samp_factor = compptr->h_samp_factor;
            v_samp_factor = compptr->v_samp_factor;
        }
        int width_in_iMCUs =
                (nJPEGWidth + iMCU_sample_width - 1) / iMCU_sample_width;
        int height_in_iMCUs =
                (nJPEGHeight + iMCU_sample_height - 1) / iMCU_sample_height;
        int nWidth_in_blocks = width_in_iMCUs * h_samp_factor;
        int nHeight_in_blocks = height_in_iMCUs * v_samp_factor;
        pDstCoeffs[ci] = (*sCInfo.mem->request_virt_barray)(
            (j_common_ptr) &sCInfo, JPOOL_IMAGE, FALSE,
            nWidth_in_blocks, nHeight_in_blocks,
            (JDIMENSION) v_samp_factor );
    }

    jpeg_vsiio_dest( &sCInfo, fpMEM );

    // Start compressor (note no image data is actually written here).
    jpeg_write_coefficients(&sCInfo, pDstCoeffs);

    jpeg_suppress_tables( &sCInfo, TRUE );

    // Must copy the right amount of data (the destination's image size)
    // starting at the given X and Y offsets in the source.
    for( int ci = 0; ci < sCInfo.num_components; ci++ )
    {
        jpeg_component_info *compptr = sCInfo.comp_info + ci;
        const int x_crop_blocks = x_crop_offset * compptr->h_samp_factor;
        const int y_crop_blocks = y_crop_offset * compptr->v_samp_factor;
        const JDIMENSION nSrcWidthInBlocks =
            psDInfo->comp_info[ci].width_in_blocks;
        const JDIMENSION nSrcHeightInBlocks =
            psDInfo->comp_info[ci].height_in_blocks;

        JDIMENSION nXBlocksToCopy = compptr->width_in_blocks;
        if( x_crop_blocks + compptr->width_in_blocks > nSrcWidthInBlocks )
            nXBlocksToCopy = nSrcWidthInBlocks - x_crop_blocks;

        for( JDIMENSION dst_blk_y = 0;
             dst_blk_y < compptr->height_in_blocks;
             dst_blk_y += compptr->v_samp_factor )
        {
            JBLOCKARRAY dst_buffer = (*psDInfo->mem->access_virt_barray)(
                (j_common_ptr) psDInfo, pDstCoeffs[ci],
                dst_blk_y,
                (JDIMENSION) compptr->v_samp_factor, TRUE );

            int offset_y = 0;
            if( bIsTiled &&
                dst_blk_y + y_crop_blocks + compptr->v_samp_factor >
                                                        nSrcHeightInBlocks )
            {
                const int nYBlocks =
                    (int)nSrcHeightInBlocks - (int)(dst_blk_y + y_crop_blocks);
                if( nYBlocks > 0 )
                {
                    JBLOCKARRAY src_buffer =
                        (*psDInfo->mem->access_virt_barray)(
                            (j_common_ptr) psDInfo, pSrcCoeffs[ci],
                            dst_blk_y + y_crop_blocks,
                            (JDIMENSION) 1, FALSE );
                    for( ; offset_y < nYBlocks; offset_y++ )
                    {
                        memcpy( dst_buffer[offset_y],
                                src_buffer[offset_y] + x_crop_blocks,
                                nXBlocksToCopy * (DCTSIZE2 * sizeof(JCOEF)));
                        if( nXBlocksToCopy < compptr->width_in_blocks )
                        {
                            memset(dst_buffer[offset_y] + nXBlocksToCopy, 0,
                                   (compptr->width_in_blocks - nXBlocksToCopy) *
                                   (DCTSIZE2 * sizeof(JCOEF)));
                        }
                    }
                }

                for( ; offset_y < compptr->v_samp_factor; offset_y++ )
                {
                    memset(
                        dst_buffer[offset_y], 0,
                        compptr->width_in_blocks * DCTSIZE2 * sizeof(JCOEF) );
                }
            }
            else
            {
                JBLOCKARRAY src_buffer = (*psDInfo->mem->access_virt_barray)(
                    (j_common_ptr) psDInfo, pSrcCoeffs[ci],
                    dst_blk_y + y_crop_blocks,
                    (JDIMENSION) compptr->v_samp_factor, FALSE );
                for( ; offset_y < compptr->v_samp_factor; offset_y++ )
                {
                    memcpy(dst_buffer[offset_y],
                           src_buffer[offset_y] + x_crop_blocks,
                           nXBlocksToCopy * (DCTSIZE2 * sizeof(JCOEF)));
                    if( nXBlocksToCopy < compptr->width_in_blocks )
                    {
                        memset(dst_buffer[offset_y] + nXBlocksToCopy, 0,
                               (compptr->width_in_blocks - nXBlocksToCopy) *
                               (DCTSIZE2 * sizeof(JCOEF)));
                    }
                }
            }
        }
    }

    jpeg_finish_compress(&sCInfo);
    jpeg_destroy_compress(&sCInfo);

    CPL_IGNORE_RET_VAL( VSIFCloseL(fpMEM) );

/* -------------------------------------------------------------------- */
/*      Write the JPEG content with libtiff raw API                     */
/* -------------------------------------------------------------------- */
    vsi_l_offset nSize = 0;
    GByte* pabyJPEGData = VSIGetMemFileBuffer(osTmpFilename, &nSize, FALSE);

    CPLErr eErr = CE_None;

    if( bIsTiled )
    {
        if( static_cast<vsi_l_offset>(
               TIFFWriteRawTile(
                   hTIFF, iX + iY * nXBlocks,
                   pabyJPEGData,
                   static_cast<tmsize_t>(nSize) ) ) != nSize )
            eErr = CE_Failure;
    }
    else
    {
        if( static_cast<vsi_l_offset>(
               TIFFWriteRawStrip(
                   hTIFF, iX + iY * nXBlocks,
                   pabyJPEGData, static_cast<tmsize_t>(nSize) ) ) != nSize )
            eErr = CE_Failure;
    }

    VSIUnlink(osTmpFilename);

    return eErr;
}

/************************************************************************/
/*                      GTIFF_CopyFromJPEG()                            */
/************************************************************************/

CPLErr GTIFF_CopyFromJPEG(GDALDataset* poDS, GDALDataset* poSrcDS,
                          GDALProgressFunc pfnProgress, void * pProgressData,
                          int& bShouldFallbackToNormalCopyIfFail)
{
    bShouldFallbackToNormalCopyIfFail = TRUE;

    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if( poSrcDS == NULL )
        return CE_Failure;

    VSILFILE* fpJPEG = VSIFOpenL(poSrcDS->GetDescription(), "rb");
    if( fpJPEG == NULL )
        return CE_Failure;

    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Initialization of the decompressor                              */
/* -------------------------------------------------------------------- */
    struct jpeg_error_mgr sJErr;
    struct jpeg_decompress_struct sDInfo;
    memset(&sDInfo, 0, sizeof(sDInfo));
    jmp_buf setjmp_buffer;
    if( setjmp(setjmp_buffer) )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpJPEG));
        jpeg_destroy_decompress(&sDInfo);
        return CE_Failure;
    }

    sDInfo.err = jpeg_std_error( &sJErr );
    sJErr.error_exit = GTIFF_ErrorExitJPEG;
    sDInfo.client_data = (void *) &setjmp_buffer;

    jpeg_create_decompress(&sDInfo);

    // This is to address bug related in ticket #1795.
    if( CPLGetConfigOption("JPEGMEM", NULL) == NULL )
    {
        // If the user doesn't provide a value for JPEGMEM, be sure that at
        // least 500 MB will be used before creating the temporary file.
        const long nMinMemory = 500 * 1024 * 1024;
        sDInfo.mem->max_memory_to_use =
            std::max(sDInfo.mem->max_memory_to_use, nMinMemory);
    }

    jpeg_vsiio_src( &sDInfo, fpJPEG );
    jpeg_read_header( &sDInfo, TRUE );

    jvirt_barray_ptr* pSrcCoeffs = jpeg_read_coefficients(&sDInfo);

/* -------------------------------------------------------------------- */
/*      Compute MCU dimensions                                          */
/* -------------------------------------------------------------------- */
    int iMCU_sample_width = 8;
    int iMCU_sample_height = 8;
    if( sDInfo.num_components != 1 )
    {
        iMCU_sample_width = sDInfo.max_h_samp_factor * 8;
        iMCU_sample_height = sDInfo.max_v_samp_factor * 8;
    }

/* -------------------------------------------------------------------- */
/*      Get raster and block dimensions                                 */
/* -------------------------------------------------------------------- */
    int nBlockXSize = 0;
    int nBlockYSize = 0;

    const int nXSize = poDS->GetRasterXSize();
    const int nYSize = poDS->GetRasterYSize();
    // nBands = poDS->GetRasterCount();

    // Don't use the GDAL block dimensions because of the split-band
    // mechanism that can expose a pseudo one-line-strip whereas the
    // real layout is a single big strip.

    TIFF* hTIFF = static_cast<TIFF*>( poDS->GetInternalHandle(NULL) );
    if( TIFFIsTiled(hTIFF) )
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(nBlockYSize) );
    }
    else
    {
        uint32 nRowsPerStrip = 0;
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                        &(nRowsPerStrip) ) )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "RowsPerStrip not defined ... assuming all one strip." );
            nRowsPerStrip = nYSize;  // Dummy value.
        }

        // If the rows per strip is larger than the file we will get
        // confused.  libtiff internally will treat the rowsperstrip as
        // the image height and it is best if we do too. (#4468)
        if( nRowsPerStrip > static_cast<uint32>(nYSize) )
            nRowsPerStrip = nYSize;

        nBlockXSize = nXSize;
        nBlockYSize = nRowsPerStrip;
    }

    const int nXBlocks = (nXSize + nBlockXSize - 1) / nBlockXSize;
    const int nYBlocks = (nYSize + nBlockYSize - 1) / nBlockYSize;

/* -------------------------------------------------------------------- */
/*      Copy blocks.                                                    */
/* -------------------------------------------------------------------- */

    bShouldFallbackToNormalCopyIfFail = FALSE;

    for( int iY = 0; iY < nYBlocks && eErr == CE_None; iY++ )
    {
        for( int iX = 0; iX < nXBlocks && eErr == CE_None; iX++ )
        {
            GTIFF_CopyBlockFromJPEGArgs sArgs;
            sArgs.hTIFF = hTIFF;
            sArgs.psDInfo = &sDInfo;
            sArgs.iX = iX;
            sArgs.iY = iY;
            sArgs.nXBlocks = nXBlocks;
            sArgs.nXSize = nXSize;
            sArgs.nYSize = nYSize;
            sArgs.nBlockXSize = nBlockXSize;
            sArgs.nBlockYSize = nBlockYSize;
            sArgs.iMCU_sample_width = iMCU_sample_width;
            sArgs.iMCU_sample_height = iMCU_sample_height;
            sArgs.pSrcCoeffs = pSrcCoeffs;

            eErr = GTIFF_CopyBlockFromJPEG( &sArgs );

            if( !pfnProgress((iY * nXBlocks + iX + 1) * 1.0 /
                                (nXBlocks * nYBlocks),
                             NULL, pProgressData ) )
                eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */

    jpeg_finish_decompress( &sDInfo );
    jpeg_destroy_decompress( &sDInfo );

    if( VSIFCloseL(fpJPEG) != 0 )
        eErr = CE_Failure;

    return eErr;
}

#endif  // HAVE_LIBJPEG
