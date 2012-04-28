/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Specialized copy of JPEG content into TIFF.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault, <even dot rouault at mines dash paris dot org>
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

#include "cpl_vsi.h"
#include "gt_jpeg_copy.h"

/* Note: JPEG_DIRECT_COPY is not defined by default, because it is mainly */
/* usefull for debugging purposes */

CPL_CVSID("$Id$");

#if defined(JPEG_DIRECT_COPY) || defined(HAVE_LIBJPEG)

#include "vrt/vrtdataset.h"

/************************************************************************/
/*                      GetUnderlyingDataset()                          */
/************************************************************************/

static GDALDataset* GetUnderlyingDataset(GDALDataset* poSrcDS)
{
    /* Test if we can directly copy original JPEG content */
    /* if available */
    if (poSrcDS->GetDriver() != NULL &&
        poSrcDS->GetDriver() == GDALGetDriverByName("VRT"))
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

static int IsBaselineDCTJPEG(VSILFILE* fp)
{
    GByte abyBuf[4];

    if (VSIFReadL(abyBuf, 1, 2, fp) != 2 ||
        abyBuf[0] != 0xff || abyBuf[1] != 0xd8 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Not a valid JPEG file");
        return FALSE;
    }

    int nOffset = 2;
    while(TRUE)
    {
        VSIFSeekL(fp, nOffset, SEEK_SET);
        if (VSIFReadL(abyBuf, 1, 4, fp) != 4 ||
            abyBuf[0] != 0xFF)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "Not a valid JPEG file");
            return FALSE;
        }

        int nMarker = abyBuf[1];

        if (nMarker == 0xC0 /* Start of Frame 0 = Baseline DCT */)
            return TRUE;

        if (nMarker == 0xD9)
            return FALSE;

        if (nMarker == 0xF7 /* JPEG Extension 7, JPEG-LS */ ||
            nMarker == 0xF8 /* JPEG Extension 8, JPEG-LS Extension */ ||
            (nMarker >= 0xC1 && nMarker <= 0xCF) /* Other Start of Frames that we don't want to support */)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unsupported type of JPEG file for JPEG_DIRECT_COPY mode");
            return FALSE;
        }

        nOffset += 2 + abyBuf[2] * 256 + abyBuf[3];
    }
}

/************************************************************************/
/*                    GTIFF_CanDirectCopyFromJPEG()                     */
/************************************************************************/

int GTIFF_CanDirectCopyFromJPEG(GDALDataset* poSrcDS, char** &papszCreateOptions)
{
    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if (poSrcDS == NULL)
        return FALSE;
    if (poSrcDS->GetDriver() == NULL)
        return FALSE;
    if (!EQUAL(GDALGetDriverShortName(poSrcDS->GetDriver()), "JPEG"))
        return FALSE;

    const char* pszCompress = CSLFetchNameValue(papszCreateOptions, "COMPRESS");
    if (pszCompress != NULL && !EQUAL(pszCompress, "JPEG"))
        return FALSE;

    const char* pszSrcColorSpace = poSrcDS->GetMetadataItem("SOURCE_COLOR_SPACE", "IMAGE_STRUCTURE");
    if (pszSrcColorSpace != NULL &&
        (EQUAL(pszSrcColorSpace, "CMYK") || EQUAL(pszSrcColorSpace, "YCbCrK")))
        return FALSE;

    int bJPEGDirectCopy = FALSE;

    VSILFILE* fpJPEG = VSIFOpenL(poSrcDS->GetDescription(), "rb");
    if (fpJPEG && IsBaselineDCTJPEG(fpJPEG))
    {
        bJPEGDirectCopy = TRUE;

        if (pszCompress == NULL)
            papszCreateOptions = CSLSetNameValue(papszCreateOptions, "COMPRESS", "JPEG");

        papszCreateOptions = CSLSetNameValue(papszCreateOptions, "BLOCKXSIZE", NULL);
        papszCreateOptions = CSLSetNameValue(papszCreateOptions, "BLOCKYSIZE",
                                                CPLSPrintf("%d", poSrcDS->GetRasterYSize()));

        if (pszSrcColorSpace != NULL && EQUAL(pszSrcColorSpace, "YCbCr"))
            papszCreateOptions = CSLSetNameValue(papszCreateOptions, "PHOTOMETRIC", "YCBCR");
        else
            papszCreateOptions = CSLSetNameValue(papszCreateOptions, "PHOTOMETRIC", NULL);

        if (poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
            papszCreateOptions = CSLSetNameValue(papszCreateOptions, "NBITS", "12");
        else
            papszCreateOptions = CSLSetNameValue(papszCreateOptions, "NBITS", NULL);

        papszCreateOptions = CSLSetNameValue(papszCreateOptions, "TILED", NULL);
        papszCreateOptions = CSLSetNameValue(papszCreateOptions, "JPEG_QUALITY", NULL);
    }
    if (fpJPEG)
        VSIFCloseL(fpJPEG);

    return bJPEGDirectCopy;
}

/************************************************************************/
/*                     GTIFF_DirectCopyFromJPEG()                       */
/************************************************************************/

CPLErr GTIFF_DirectCopyFromJPEG(GDALDataset* poDS, GDALDataset* poSrcDS,
                                GDALProgressFunc pfnProgress, void * pProgressData,
                                int& bShouldFallbackToNormalCopyIfFail)
{
    bShouldFallbackToNormalCopyIfFail = TRUE;

    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if (poSrcDS == NULL)
        return CE_Failure;

    VSILFILE* fpJPEG = VSIFOpenL(poSrcDS->GetDescription(), "rb");
    if (fpJPEG == NULL)
        return CE_Failure;

    CPLErr eErr = CE_None;

    VSIFSeekL(fpJPEG, 0, SEEK_END);
    tmsize_t nSize = (tmsize_t) VSIFTellL(fpJPEG);
    VSIFSeekL(fpJPEG, 0, SEEK_SET);

    void* pabyJPEGData = VSIMalloc(nSize);
    if (pabyJPEGData == NULL)
    {
        VSIFCloseL(fpJPEG);
        return CE_Failure;
    }

    if (pabyJPEGData != NULL &&
        (tmsize_t)VSIFReadL(pabyJPEGData, 1, nSize, fpJPEG) == nSize)
    {
        bShouldFallbackToNormalCopyIfFail = FALSE;

        TIFF* hTIFF = (TIFF*) poDS->GetInternalHandle(NULL);
        if (TIFFWriteRawStrip(hTIFF, 0, pabyJPEGData, nSize) != nSize)
            eErr = CE_Failure;

        if( !pfnProgress( 1.0, NULL, pProgressData ) )
            eErr = CE_Failure;
    }
    else
    {
        eErr = CE_Failure;
    }

    VSIFree(pabyJPEGData);
    VSIFCloseL(fpJPEG);

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

int GTIFF_CanCopyFromJPEG(GDALDataset* poSrcDS, char** &papszCreateOptions)
{
    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if (poSrcDS == NULL)
        return FALSE;
    if (poSrcDS->GetDriver() == NULL)
        return FALSE;
    if (!EQUAL(GDALGetDriverShortName(poSrcDS->GetDriver()), "JPEG"))
        return FALSE;

    const char* pszCompress = CSLFetchNameValue(papszCreateOptions, "COMPRESS");
    if (pszCompress == NULL || !EQUAL(pszCompress, "JPEG"))
        return FALSE;

    int nBlockXSize = atoi(CSLFetchNameValueDef(papszCreateOptions, "BLOCKXSIZE", "0"));
    int nBlockYSize = atoi(CSLFetchNameValueDef(papszCreateOptions, "BLOCKYSIZE", "0"));
    int nMCUSize = 8;
    const char* pszSrcColorSpace =
        poSrcDS->GetMetadataItem("SOURCE_COLOR_SPACE", "IMAGE_STRUCTURE");
    if (pszSrcColorSpace != NULL && EQUAL(pszSrcColorSpace, "YCbCr"))
        nMCUSize = 16;
    else if (pszSrcColorSpace != NULL &&
        (EQUAL(pszSrcColorSpace, "CMYK") || EQUAL(pszSrcColorSpace, "YCbCrK")))
        return FALSE;

    int     nXSize = poSrcDS->GetRasterXSize();
    int     nYSize = poSrcDS->GetRasterYSize();
    int     nBands = poSrcDS->GetRasterCount();

    const char* pszPhotometric = CSLFetchNameValue(papszCreateOptions, "PHOTOMETRIC");
    int bCompatiblePhotometric = (
            pszPhotometric == NULL ||
            (nMCUSize == 16 && EQUAL(pszPhotometric, "YCbCr")) ||
            (nMCUSize == 8 && EQUAL(pszPhotometric, "RGB") && nBands == 3) ||
            (nMCUSize == 8 && EQUAL(pszPhotometric, "MINISBLACK") && nBands == 1) );
    if (!bCompatiblePhotometric)
        return FALSE;

    if ( (nBlockXSize == nXSize || (nBlockXSize % nMCUSize) == 0) &&
         (nBlockYSize == nYSize || (nBlockYSize % nMCUSize) == 0) &&
         poSrcDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte &&
         CSLFetchNameValue(papszCreateOptions, "NBITS") == NULL &&
         CSLFetchNameValue(papszCreateOptions, "JPEG_QUALITY") == NULL &&
         bCompatiblePhotometric )
    {
        if (nMCUSize == 16 && pszPhotometric == NULL)
            papszCreateOptions = CSLSetNameValue(papszCreateOptions, "PHOTOMETRIC", "YCBCR");
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/************************************************************************/
/*                      GTIFF_ErrorExitJPEG()                           */
/************************************************************************/

static void GTIFF_ErrorExitJPEG(j_common_ptr cinfo)
{
    jmp_buf *setjmp_buffer = (jmp_buf *) cinfo->client_data;
    char buffer[JMSG_LENGTH_MAX];

    /* Create the message */
    (*cinfo->err->format_message) (cinfo, buffer);

    CPLError( CE_Failure, CPLE_AppDefined,
              "libjpeg: %s", buffer );

    /* Return control to the setjmp point */
    longjmp(*setjmp_buffer, 1);
}

/************************************************************************/
/*                      GTIFF_Set_TIFFTAG_JPEGTABLES()                  */
/************************************************************************/

void GTIFF_Set_TIFFTAG_JPEGTABLES(TIFF* hTIFF,
                                  jpeg_decompress_struct& sDInfo,
                                  jpeg_compress_struct& sCInfo)
{
    char szTmpFilename[128];
    sprintf(szTmpFilename, "/vsimem/tables_%p", &sDInfo);
    VSILFILE* fpTABLES = VSIFOpenL(szTmpFilename, "wb+");

    jpeg_vsiio_dest( &sCInfo, fpTABLES );
    jpeg_write_tables( &sCInfo );

    VSIFCloseL(fpTABLES);

    vsi_l_offset nSizeTables = 0;
    GByte* pabyJPEGTablesData = VSIGetMemFileBuffer(szTmpFilename, &nSizeTables, FALSE);
    TIFFSetField(hTIFF, TIFFTAG_JPEGTABLES, (int)nSizeTables, pabyJPEGTablesData);

    VSIUnlink(szTmpFilename);
}

/************************************************************************/
/*             GTIFF_CopyFromJPEG_WriteAdditionalTags()                 */
/************************************************************************/

CPLErr GTIFF_CopyFromJPEG_WriteAdditionalTags(TIFF* hTIFF,
                                              GDALDataset* poSrcDS)
{
    poSrcDS = GetUnderlyingDataset(poSrcDS);
    if (poSrcDS == NULL)
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Write TIFFTAG_JPEGTABLES                                        */
/* -------------------------------------------------------------------- */

    VSILFILE* fpJPEG = VSIFOpenL(poSrcDS->GetDescription(), "rb");
    if (fpJPEG == NULL)
        return CE_Failure;

    struct jpeg_error_mgr sJErr;
    struct jpeg_decompress_struct sDInfo;
    jmp_buf setjmp_buffer;
    if (setjmp(setjmp_buffer))
    {
        VSIFCloseL(fpJPEG);
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

    uint16 nPhotometric;
    if( !TIFFGetField( hTIFF, TIFFTAG_PHOTOMETRIC, &(nPhotometric) ) )
        nPhotometric = PHOTOMETRIC_MINISBLACK;

    uint16 nBitsPerSample;
    if( !TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(nBitsPerSample)) )
        nBitsPerSample = 1;

    if ( nPhotometric == PHOTOMETRIC_YCBCR )
    {
        /*
         * A ReferenceBlackWhite field *must* be present since the
         * default value is inappropriate for YCbCr.  Fill in the
         * proper value if application didn't set it.
         */
        float *ref;
        if (!TIFFGetField(hTIFF, TIFFTAG_REFERENCEBLACKWHITE,
                    &ref))
        {
            float refbw[6];
            long top = 1L << nBitsPerSample;
            refbw[0] = 0;
            refbw[1] = (float)(top-1L);
            refbw[2] = (float)(top>>1);
            refbw[3] = refbw[1];
            refbw[4] = refbw[2];
            refbw[5] = refbw[1];
            TIFFSetField(hTIFF, TIFFTAG_REFERENCEBLACKWHITE,
                        refbw);
        }
    }

/* -------------------------------------------------------------------- */
/*      Write TIFFTAG_YCBCRSUBSAMPLING if needed.                       */
/* -------------------------------------------------------------------- */

    if ( nPhotometric == PHOTOMETRIC_YCBCR && sDInfo.num_components == 3 )
    {
        if ((sDInfo.comp_info[0].h_samp_factor == 1 || sDInfo.comp_info[0].h_samp_factor == 2) &&
            (sDInfo.comp_info[0].v_samp_factor == 1 || sDInfo.comp_info[0].v_samp_factor == 2) &&
            sDInfo.comp_info[1].h_samp_factor == 1 &&
            sDInfo.comp_info[1].v_samp_factor == 1 &&
            sDInfo.comp_info[2].h_samp_factor == 1 &&
            sDInfo.comp_info[2].v_samp_factor == 1)
        {
            TIFFSetField(hTIFF, TIFFTAG_YCBCRSUBSAMPLING,
                         sDInfo.comp_info[0].h_samp_factor,
                         sDInfo.comp_info[0].v_samp_factor);
        }
        else
        {
            CPLDebug("GTiff", "Unusual sampling factors. TIFFTAG_YCBCRSUBSAMPLING not written.");
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */

    jpeg_abort_decompress( &sDInfo );
    jpeg_destroy_decompress( &sDInfo );

    VSIFCloseL(fpJPEG);

    return CE_None;
}

/************************************************************************/
/*                    GTIFF_CopyBlockFromJPEG()                         */
/************************************************************************/

static CPLErr GTIFF_CopyBlockFromJPEG(TIFF* hTIFF,
                                      jpeg_decompress_struct& sDInfo,
                                      int iX, int iY,
                                      int nXBlocks, int nYBlocks,
                                      int nXSize, int nYSize,
                                      int nBlockXSize, int nBlockYSize,
                                      int iMCU_sample_width, int iMCU_sample_height,
                                      jvirt_barray_ptr *pSrcCoeffs)
{
    CPLString osTmpFilename(CPLSPrintf("/vsimem/%p", &sDInfo));
    VSILFILE* fpMEM = VSIFOpenL(osTmpFilename, "wb+");

/* -------------------------------------------------------------------- */
/*      Initialization of the compressor                                */
/* -------------------------------------------------------------------- */
    struct jpeg_error_mgr sJErr;
    struct jpeg_compress_struct sCInfo;
    jmp_buf setjmp_buffer;
    if (setjmp(setjmp_buffer))
    {
        VSIFCloseL(fpMEM);
        VSIUnlink(osTmpFilename);
        return CE_Failure;
    }

    sCInfo.err = jpeg_std_error( &sJErr );
    sJErr.error_exit = GTIFF_ErrorExitJPEG;
    sCInfo.client_data = (void *) &setjmp_buffer;

    /* Initialize destination compression parameters from source values */
    jpeg_create_compress(&sCInfo);
    jpeg_copy_critical_parameters(&sDInfo, &sCInfo);

    /* ensure libjpeg won't write any extraneous markers */
    sCInfo.write_JFIF_header = FALSE;
    sCInfo.write_Adobe_marker = FALSE;

/* -------------------------------------------------------------------- */
/*      Allocated destination coefficient array                         */
/* -------------------------------------------------------------------- */
    int bIsTiled = TIFFIsTiled(hTIFF);

    int nJPEGWidth, nJPEGHeight;
    if (bIsTiled)
    {
        nJPEGWidth = nBlockXSize;
        nJPEGHeight = nBlockYSize;
    }
    else
    {
        nJPEGWidth = MIN(nBlockXSize, nXSize - iX * nBlockXSize);
        nJPEGHeight = MIN(nBlockYSize, nYSize - iY * nBlockYSize);
    }

    /* Code partially derived from libjpeg transupp.c */

    /* Correct the destination's image dimensions as necessary */
    #if JPEG_LIB_VERSION >= 70
    sCInfo.jpeg_width = nJPEGWidth;
    sCInfo.jpeg_height = nJPEGHeight;
    #else
    sCInfo.image_width = nJPEGWidth;
    sCInfo.image_height = nJPEGHeight;
    #endif

    /* Save x/y offsets measured in iMCUs */
    int x_crop_offset = (iX * nBlockXSize) / iMCU_sample_width;
    int y_crop_offset = (iY * nBlockYSize) / iMCU_sample_height;

    jvirt_barray_ptr* pDstCoeffs = (jvirt_barray_ptr *)
        (*sCInfo.mem->alloc_small) ((j_common_ptr) &sCInfo, JPOOL_IMAGE,
                                    sizeof(jvirt_barray_ptr) * sCInfo.num_components);
    int ci;

    for (ci = 0; ci < sCInfo.num_components; ci++)
    {
        jpeg_component_info *compptr = sCInfo.comp_info + ci;
        int h_samp_factor, v_samp_factor;
        if (sCInfo.num_components == 1)
        {
            /* we're going to force samp factors to 1x1 in this case */
            h_samp_factor = v_samp_factor = 1;
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
        pDstCoeffs[ci] = (*sCInfo.mem->request_virt_barray)
                ((j_common_ptr) &sCInfo, JPOOL_IMAGE, FALSE,
                nWidth_in_blocks, nHeight_in_blocks, (JDIMENSION) v_samp_factor);
    }

    jpeg_vsiio_dest( &sCInfo, fpMEM );

    /* Start compressor (note no image data is actually written here) */
    jpeg_write_coefficients(&sCInfo, pDstCoeffs);

    jpeg_suppress_tables( &sCInfo, TRUE );

    /* We simply have to copy the right amount of data (the destination's
    * image size) starting at the given X and Y offsets in the source.
    */
    for (ci = 0; ci < sCInfo.num_components; ci++)
    {
        jpeg_component_info *compptr = sCInfo.comp_info + ci;
        int x_crop_blocks = x_crop_offset * compptr->h_samp_factor;
        int y_crop_blocks = y_crop_offset * compptr->v_samp_factor;
        JDIMENSION nSrcWidthInBlocks = sDInfo.comp_info[ci].width_in_blocks;
        JDIMENSION nSrcHeightInBlocks = sDInfo.comp_info[ci].height_in_blocks;

        JDIMENSION nXBlocksToCopy = compptr->width_in_blocks;
        if (x_crop_blocks + compptr->width_in_blocks > nSrcWidthInBlocks)
            nXBlocksToCopy = nSrcWidthInBlocks - x_crop_blocks;

        for (JDIMENSION dst_blk_y = 0;
                        dst_blk_y < compptr->height_in_blocks;
                        dst_blk_y += compptr->v_samp_factor)
        {
            JBLOCKARRAY dst_buffer = (*sDInfo.mem->access_virt_barray)
                            ((j_common_ptr) &sDInfo, pDstCoeffs[ci],
                                dst_blk_y,
                                (JDIMENSION) compptr->v_samp_factor, TRUE);

            int offset_y = 0;
            int nYBlocks = compptr->v_samp_factor;
            if( bIsTiled &&
                dst_blk_y + y_crop_blocks + compptr->v_samp_factor >
                                                        nSrcHeightInBlocks)
            {
                nYBlocks = nSrcHeightInBlocks - (dst_blk_y + y_crop_blocks);
                if (nYBlocks > 0)
                {
                    JBLOCKARRAY src_buffer = (*sDInfo.mem->access_virt_barray)
                                ((j_common_ptr) &sDInfo, pSrcCoeffs[ci],
                                dst_blk_y + y_crop_blocks,
                                    (JDIMENSION) 1, FALSE);
                    for (; offset_y < nYBlocks; offset_y++)
                    {
                        memcpy(dst_buffer[offset_y],
                               src_buffer[offset_y] + x_crop_blocks,
                               nXBlocksToCopy * (DCTSIZE2 * sizeof(JCOEF)));
                        if (nXBlocksToCopy < compptr->width_in_blocks)
                        {
                            memset(dst_buffer[offset_y]  + nXBlocksToCopy, 0,
                                   (compptr->width_in_blocks - nXBlocksToCopy) *
                                                    (DCTSIZE2 * sizeof(JCOEF)));
                        }
                    }
                }

                for (; offset_y < compptr->v_samp_factor; offset_y++)
                {
                    memset(dst_buffer[offset_y], 0,
                           compptr->width_in_blocks * (DCTSIZE2 * sizeof(JCOEF)));
                }
            }
            else
            {
                JBLOCKARRAY src_buffer = (*sDInfo.mem->access_virt_barray)
                                ((j_common_ptr) &sDInfo, pSrcCoeffs[ci],
                                dst_blk_y + y_crop_blocks,
                                (JDIMENSION) compptr->v_samp_factor, FALSE);
                for (; offset_y < compptr->v_samp_factor; offset_y++)
                {
                    memcpy(dst_buffer[offset_y],
                           src_buffer[offset_y] + x_crop_blocks,
                           nXBlocksToCopy * (DCTSIZE2 * sizeof(JCOEF)));
                    if (nXBlocksToCopy < compptr->width_in_blocks)
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

    VSIFCloseL(fpMEM);

/* -------------------------------------------------------------------- */
/*      Write the JPEG content with libtiff raw API                     */
/* -------------------------------------------------------------------- */
    vsi_l_offset nSize = 0;
    GByte* pabyJPEGData = VSIGetMemFileBuffer(osTmpFilename, &nSize, FALSE);

    CPLErr eErr = CE_None;

    if ( bIsTiled )
    {
        if ((vsi_l_offset)TIFFWriteRawTile(hTIFF, iX + iY * nXBlocks,
                                           pabyJPEGData, nSize) != nSize)
            eErr = CE_Failure;
    }
    else
    {
        if ((vsi_l_offset)TIFFWriteRawStrip(hTIFF, iX + iY * nXBlocks,
                                            pabyJPEGData, nSize) != nSize)
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
    if (poSrcDS == NULL)
        return CE_Failure;

    VSILFILE* fpJPEG = VSIFOpenL(poSrcDS->GetDescription(), "rb");
    if (fpJPEG == NULL)
        return CE_Failure;

    CPLErr eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Initialization of the decompressor                              */
/* -------------------------------------------------------------------- */
    struct jpeg_error_mgr sJErr;
    struct jpeg_decompress_struct sDInfo;
    jmp_buf setjmp_buffer;
    if (setjmp(setjmp_buffer))
    {
        VSIFCloseL(fpJPEG);
        return CE_Failure;
    }

    sDInfo.err = jpeg_std_error( &sJErr );
    sJErr.error_exit = GTIFF_ErrorExitJPEG;
    sDInfo.client_data = (void *) &setjmp_buffer;

    jpeg_create_decompress(&sDInfo);

    /* This is to address bug related in ticket #1795 */
    if (CPLGetConfigOption("JPEGMEM", NULL) == NULL)
    {
        /* If the user doesn't provide a value for JPEGMEM, we want to be sure */
        /* that at least 500 MB will be used before creating the temporary file */
        sDInfo.mem->max_memory_to_use =
                MAX(sDInfo.mem->max_memory_to_use, 500 * 1024 * 1024);
    }

    jpeg_vsiio_src( &sDInfo, fpJPEG );
    jpeg_read_header( &sDInfo, TRUE );

    jvirt_barray_ptr* pSrcCoeffs = jpeg_read_coefficients(&sDInfo);

/* -------------------------------------------------------------------- */
/*      Compute MCU dimensions                                          */
/* -------------------------------------------------------------------- */
    int iMCU_sample_width, iMCU_sample_height;
    if (sDInfo.num_components == 1)
    {
        iMCU_sample_width = 8;
        iMCU_sample_height = 8;
    }
    else
    {
        iMCU_sample_width = sDInfo.max_h_samp_factor * 8;
        iMCU_sample_height = sDInfo.max_v_samp_factor * 8;
    }

/* -------------------------------------------------------------------- */
/*      Get raster and block dimensions                                 */
/* -------------------------------------------------------------------- */
    int nXSize, nYSize, nBands;
    int nBlockXSize, nBlockYSize;

    nXSize = poDS->GetRasterXSize();
    nYSize = poDS->GetRasterYSize();
    nBands = poDS->GetRasterCount();

    /* We don't use the GDAL block dimensions because of the split-band */
    /* mechanism that can expose a pseudo one-line-strip whereas the */
    /* real layout is a single big strip */

    TIFF* hTIFF = (TIFF*) poDS->GetInternalHandle(NULL);
    if( TIFFIsTiled(hTIFF) )
    {
        TIFFGetField( hTIFF, TIFFTAG_TILEWIDTH, &(nBlockXSize) );
        TIFFGetField( hTIFF, TIFFTAG_TILELENGTH, &(nBlockYSize) );
    }
    else
    {
        uint32  nRowsPerStrip;
        if( !TIFFGetField( hTIFF, TIFFTAG_ROWSPERSTRIP,
                        &(nRowsPerStrip) ) )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                    "RowsPerStrip not defined ... assuming all one strip." );
            nRowsPerStrip = nYSize; /* dummy value */
        }

        // If the rows per strip is larger than the file we will get
        // confused.  libtiff internally will treat the rowsperstrip as
        // the image height and it is best if we do too. (#4468)
        if (nRowsPerStrip > (uint32)nYSize)
            nRowsPerStrip = nYSize;

        nBlockXSize = nXSize;
        nBlockYSize = nRowsPerStrip;
    }

    int nXBlocks = (nXSize + nBlockXSize - 1) / nBlockXSize;
    int nYBlocks = (nYSize + nBlockYSize - 1) / nBlockYSize;

/* -------------------------------------------------------------------- */
/*      Copy blocks.                                                    */
/* -------------------------------------------------------------------- */

    bShouldFallbackToNormalCopyIfFail = FALSE;

    for(int iY=0;iY<nYBlocks && eErr == CE_None;iY++)
    {
        for(int iX=0;iX<nXBlocks && eErr == CE_None;iX++)
        {
            eErr = GTIFF_CopyBlockFromJPEG( hTIFF,
                                            sDInfo,
                                            iX, iY,
                                            nXBlocks, nYBlocks,
                                            nXSize, nYSize,
                                            nBlockXSize, nBlockYSize,
                                            iMCU_sample_width,
                                            iMCU_sample_height,
                                            pSrcCoeffs );

            if (!pfnProgress((iY * nXBlocks + iX + 1) * 1.0 /
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

    VSIFCloseL(fpJPEG);

    return eErr;
}

#endif // HAVE_LIBJPEG
