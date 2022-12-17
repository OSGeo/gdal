/******************************************************************************
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  GDALDataset/GDALRasterBand implementation on top of "nitflib".
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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

#ifdef JPEG_SUPPORTED

#include "cpl_port.h"
#include "gdal_pam.h"

CPL_C_START
#ifdef LIBJPEG_12_PATH
#include LIBJPEG_12_PATH
#else
#include "jpeglib.h"
#endif
CPL_C_END

#if defined(EXPECTED_JPEG_LIB_VERSION) && !defined(LIBJPEG_12_PATH)
#if EXPECTED_JPEG_LIB_VERSION != JPEG_LIB_VERSION
#error EXPECTED_JPEG_LIB_VERSION != JPEG_LIB_VERSION
#endif
#endif

/*
 * Do we want to do special processing suitable for when JSAMPLE is a
 * 16bit value?
 */
#if defined(JPEG_LIB_MK1)
#define JPEG_LIB_MK1_OR_12BIT 1
#elif BITS_IN_JSAMPLE == 12
#define JPEG_LIB_MK1_OR_12BIT 1
#endif

#if defined(JPEG_DUAL_MODE_8_12) && !defined(NITFWriteJPEGBlock)
int NITFWriteJPEGBlock_12(GDALDataset *poSrcDS, VSILFILE *fp, int nBlockXOff,
                          int nBlockYOff, int nBlockXSize, int nBlockYSize,
                          int bProgressive, int nQuality, const GByte *pabyAPP6,
                          int nRestartInterval, GDALProgressFunc pfnProgress,
                          void *pProgressData);
#endif

int NITFWriteJPEGBlock(GDALDataset *poSrcDS, VSILFILE *fp, int nBlockXOff,
                       int nBlockYOff, int nBlockXSize, int nBlockYSize,
                       int bProgressive, int nQuality, const GByte *pabyAPP6,
                       int nRestartInterval, GDALProgressFunc pfnProgress,
                       void *pProgressData);

void jpeg_vsiio_dest(j_compress_ptr cinfo, VSILFILE *outfile);

/************************************************************************/
/*                         NITFWriteJPEGBlock()                         */
/************************************************************************/

int NITFWriteJPEGBlock(GDALDataset *poSrcDS, VSILFILE *fp, int nBlockXOff,
                       int nBlockYOff, int nBlockXSize, int nBlockYSize,
                       int bProgressive, int nQuality, const GByte *pabyAPP6,
                       int nRestartInterval, GDALProgressFunc pfnProgress,
                       void *pProgressData)
{
    GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
#if defined(JPEG_DUAL_MODE_8_12) && !defined(NITFWriteJPEGBlock)
    if (eDT == GDT_UInt16)
    {
        return NITFWriteJPEGBlock_12(poSrcDS, fp, nBlockXOff, nBlockYOff,
                                     nBlockXSize, nBlockYSize, bProgressive,
                                     nQuality, pabyAPP6, nRestartInterval,
                                     pfnProgress, pProgressData);
    }
#endif

    int anBandList[3] = {1, 2, 3};

    /* -------------------------------------------------------------------- */
    /*      Initialize JPG access to the file.                              */
    /* -------------------------------------------------------------------- */
    struct jpeg_compress_struct sCInfo;
    struct jpeg_error_mgr sJErr;

    memset(&sCInfo, 0, sizeof(sCInfo));
    sCInfo.err = jpeg_std_error(&sJErr);
    jpeg_create_compress(&sCInfo);

    jpeg_vsiio_dest(&sCInfo, fp);

    sCInfo.image_width = nBlockXSize;
    sCInfo.image_height = nBlockYSize;

    const int nBands = poSrcDS->GetRasterCount();
    sCInfo.input_components = nBands;

    if (nBands == 1)
    {
        sCInfo.in_color_space = JCS_GRAYSCALE;
    }
    else
    {
        sCInfo.in_color_space = JCS_RGB;
    }

    jpeg_set_defaults(&sCInfo);

#if defined(JPEG_LIB_MK1_OR_12BIT)
    if (eDT == GDT_UInt16)
    {
        sCInfo.data_precision = 12;
    }
    else
    {
        sCInfo.data_precision = 8;
    }
#endif

    GDALDataType eWorkDT;
#ifdef JPEG_LIB_MK1
    sCInfo.bits_in_jsample = sCInfo.data_precision;
    eWorkDT = GDT_UInt16; /* Always force to 16 bit for JPEG_LIB_MK1 */
#else
    eWorkDT = eDT;
#endif

    sCInfo.write_JFIF_header = FALSE;

    /* Set the restart interval */
    if (nRestartInterval < 0)
    {
        /* nRestartInterval < 0 means that we will guess the value */
        /* so we set it at the maximum allowed by MIL-STD-188-198 */
        /* that is to say the number of MCU per row-block */
        nRestartInterval = nBlockXSize / 8;
    }

    if (nRestartInterval > 0)
        sCInfo.restart_interval = nRestartInterval;

    jpeg_set_quality(&sCInfo, nQuality, TRUE);

    if (bProgressive)
        jpeg_simple_progression(&sCInfo);

    jpeg_start_compress(&sCInfo, TRUE);

    /* -------------------------------------------------------------------- */
    /*    Emits APP6 NITF application segment (required by MIL-STD-188-198) */
    /* -------------------------------------------------------------------- */
    if (pabyAPP6)
    {
        /* 0xe6 = APP6 marker */
        jpeg_write_marker(&sCInfo, 0xe6, (const JOCTET *)pabyAPP6, 23);
    }

    /* -------------------------------------------------------------------- */
    /*      Loop over image, copying image data.                            */
    /* -------------------------------------------------------------------- */
    const int nWorkDTSize = GDALGetDataTypeSizeBytes(eWorkDT);

    GByte *pabyScanline = reinterpret_cast<GByte *>(
        CPLMalloc(nBands * nBlockXSize * nWorkDTSize));

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    const double nTotalPixels = static_cast<double>(nXSize * nYSize);

    int nBlockXSizeToRead = nBlockXSize;
    if (nBlockXSize * nBlockXOff + nBlockXSize > nXSize)
    {
        nBlockXSizeToRead = nXSize - nBlockXSize * nBlockXOff;
    }
    int nBlockYSizeToRead = nBlockYSize;
    if (nBlockYSize * nBlockYOff + nBlockYSize > nYSize)
    {
        nBlockYSizeToRead = nYSize - nBlockYSize * nBlockYOff;
    }

#if defined(JPEG_LIB_MK1_OR_12BIT)
    bool bClipWarn = false;
#endif

    CPLErr eErr = CE_None;
    for (int iLine = 0; iLine < nBlockYSize && eErr == CE_None; iLine++)
    {
        if (iLine < nBlockYSizeToRead)
        {
            eErr = poSrcDS->RasterIO(
                GF_Read, nBlockXSize * nBlockXOff,
                iLine + nBlockYSize * nBlockYOff, nBlockXSizeToRead, 1,
                pabyScanline, nBlockXSizeToRead, 1, eWorkDT, nBands, anBandList,
                nBands * nWorkDTSize, nBands * nBlockXSize * nWorkDTSize,
                nWorkDTSize, nullptr);

#if !defined(JPEG_LIB_MK1_OR_12BIT)
            /* Repeat the last pixel till the end of the line */
            /* to minimize discontinuity */
            if (nBlockXSizeToRead < nBlockXSize)
            {
                for (int iBand = 0; iBand < nBands; iBand++)
                {
                    GByte bVal =
                        pabyScanline[nBands * (nBlockXSizeToRead - 1) + iBand];
                    for (int iX = nBlockXSizeToRead; iX < nBlockXSize; iX++)
                    {
                        pabyScanline[nBands * iX + iBand] = bVal;
                    }
                }
            }
#endif
        }

#if defined(JPEG_LIB_MK1_OR_12BIT)
        // clamp 16bit values to 12bit.
        if (eDT == GDT_UInt16)
        {
            GUInt16 *panScanline = reinterpret_cast<GUInt16 *>(pabyScanline);

            for (int iPixel = 0; iPixel < nXSize * nBands; iPixel++)
            {
                if (panScanline[iPixel] > 4095)
                {
                    panScanline[iPixel] = 4095;
                    if (!bClipWarn)
                    {
                        bClipWarn = true;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "One or more pixels clipped to fit 12bit "
                                 "domain for jpeg output.");
                    }
                }
            }
        }
#endif

        JSAMPLE *ppSamples = reinterpret_cast<JSAMPLE *>(pabyScanline);

        if (eErr == CE_None)
            jpeg_write_scanlines(&sCInfo, &ppSamples, 1);

        double nCurPixels =
            static_cast<double>(nBlockYOff) * nBlockYSize * nXSize +
            static_cast<double>(nBlockXOff) * nBlockYSize * nBlockXSize +
            (iLine + 1) * nBlockXSizeToRead;
        if (eErr == CE_None &&
            !pfnProgress(nCurPixels / nTotalPixels, nullptr, pProgressData))
        {
            eErr = CE_Failure;
            CPLError(CE_Failure, CPLE_UserInterrupt,
                     "User terminated CreateCopy()");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup and close.                                              */
    /* -------------------------------------------------------------------- */
    CPLFree(pabyScanline);

    if (eErr == CE_None)
        jpeg_finish_compress(&sCInfo);
    jpeg_destroy_compress(&sCInfo);

    return eErr == CE_None;
}
#endif /* def JPEG_SUPPORTED */
