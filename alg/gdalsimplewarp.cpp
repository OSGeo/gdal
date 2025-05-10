/******************************************************************************
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Simple (source in memory) warp algorithm.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging, Fort Collin, CO
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_alg.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

/************************************************************************/
/*                      GDALSimpleWarpRemapping()                       */
/*                                                                      */
/*      This function implements any raster remapping requested in      */
/*      the options list.  The remappings are applied to the source     */
/*      data before warping.  Two kinds are support ... REMAP           */
/*      commands which remap selected pixel values for any band and     */
/*      REMAP_MULTI which only remap pixels matching the input in       */
/*      all bands at once (i.e. to remap an RGB value to another).      */
/************************************************************************/

static void GDALSimpleWarpRemapping(int nBandCount, GByte **papabySrcData,
                                    int nSrcXSize, int nSrcYSize,
                                    char **papszWarpOptions)

{

    /* ==================================================================== */
    /*      Process any and all single value REMAP commands.                */
    /* ==================================================================== */
    char **papszRemaps = CSLFetchNameValueMultiple(papszWarpOptions, "REMAP");

    const int nRemaps = CSLCount(papszRemaps);
    for (int iRemap = 0; iRemap < nRemaps; iRemap++)
    {

        /* --------------------------------------------------------------------
         */
        /*      What are the pixel values to map from and to? */
        /* --------------------------------------------------------------------
         */
        char **papszTokens = CSLTokenizeString(papszRemaps[iRemap]);

        if (CSLCount(papszTokens) != 2)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Ill formed REMAP `%s' ignored in "
                     "GDALSimpleWarpRemapping()",
                     papszRemaps[iRemap]);
            CSLDestroy(papszTokens);
            continue;
        }

        const int nFromValue = atoi(papszTokens[0]);
        const int nToValue = atoi(papszTokens[1]);
        // TODO(schwehr): Why is it ok to narrow ints to byte without checking?
        const GByte byToValue = static_cast<GByte>(nToValue);

        CSLDestroy(papszTokens);

        /* --------------------------------------------------------------------
         */
        /*      Pass over each band searching for matches. */
        /* --------------------------------------------------------------------
         */
        for (int iBand = 0; iBand < nBandCount; iBand++)
        {
            GByte *pabyData = papabySrcData[iBand];
            int nPixelCount = nSrcXSize * nSrcYSize;

            while (nPixelCount != 0)
            {
                if (*pabyData == nFromValue)
                    *pabyData = byToValue;

                pabyData++;
                nPixelCount--;
            }
        }
    }

    CSLDestroy(papszRemaps);

    /* ==================================================================== */
    /*      Process any and all REMAP_MULTI commands.                       */
    /* ==================================================================== */
    papszRemaps = CSLFetchNameValueMultiple(papszWarpOptions, "REMAP_MULTI");

    const int nRemapsMulti = CSLCount(papszRemaps);
    for (int iRemap = 0; iRemap < nRemapsMulti; iRemap++)
    {
        /* --------------------------------------------------------------------
         */
        /*      What are the pixel values to map from and to? */
        /* --------------------------------------------------------------------
         */
        char **papszTokens = CSLTokenizeString(papszRemaps[iRemap]);

        const int nTokens = CSLCount(papszTokens);
        if (nTokens % 2 == 1 || nTokens == 0 || nTokens > nBandCount * 2)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Ill formed REMAP_MULTI `%s' ignored in "
                     "GDALSimpleWarpRemapping()",
                     papszRemaps[iRemap]);
            CSLDestroy(papszTokens);
            continue;
        }

        const int nMapBandCount = nTokens / 2;

        int *panFromValue =
            static_cast<int *>(CPLMalloc(sizeof(int) * nMapBandCount));
        int *panToValue =
            static_cast<int *>(CPLMalloc(sizeof(int) * nMapBandCount));

        for (int iBand = 0; iBand < nMapBandCount; iBand++)
        {
            panFromValue[iBand] = atoi(papszTokens[iBand]);
            panToValue[iBand] = atoi(papszTokens[iBand + nMapBandCount]);
        }

        CSLDestroy(papszTokens);

        /* --------------------------------------------------------------------
         */
        /*      Search for matching values to replace. */
        /* --------------------------------------------------------------------
         */
        const int nPixelCount = nSrcXSize * nSrcYSize;

        for (int iPixel = 0; iPixel < nPixelCount; iPixel++)
        {
            bool bMatch = true;

            // Always check band 0.
            for (int iBand = 0; bMatch && iBand < std::max(1, nMapBandCount);
                 iBand++)
            {
                if (papabySrcData[iBand][iPixel] != panFromValue[iBand])
                    bMatch = false;
            }

            if (!bMatch)
                continue;

            for (int iBand = 0; iBand < nMapBandCount; iBand++)
                papabySrcData[iBand][iPixel] =
                    static_cast<GByte>(panToValue[iBand]);
        }

        CPLFree(panFromValue);
        CPLFree(panToValue);
    }

    CSLDestroy(papszRemaps);
}

/************************************************************************/
/*                        GDALSimpleImageWarp()                         */
/************************************************************************/

/**
 * Perform simple image warp.
 *
 * Copies an image from a source dataset to a destination dataset applying
 * an application defined transformation.   This algorithm is called simple
 * because it lacks many options such as resampling kernels (other than
 * nearest neighbour), support for data types other than 8bit, and the
 * ability to warp images without holding the entire source and destination
 * image in memory.
 *
 * The following option(s) may be passed in papszWarpOptions.
 * <ul>
 * <li> "INIT=v[,v...]": This option indicates that the output dataset should
 * be initialized to the indicated value in any area valid data is not written.
 * Distinct values may be listed for each band separated by columns.
 * </ul>
 *
 * For more advanced warping capabilities, consider using GDALWarp().
 *
 * @param hSrcDS the source image dataset.
 * @param hDstDS the destination image dataset.
 * @param nBandCount the number of bands to be warped.  If zero, all bands
 * will be processed.
 * @param panBandList the list of bands to translate.
 * @param pfnTransform the transformation function to call.  See
 * GDALTransformerFunc().
 * @param pTransformArg the callback handle to pass to pfnTransform.
 * @param pfnProgress the function used to report progress.  See
 * GDALProgressFunc().
 * @param pProgressArg the callback handle to pass to pfnProgress.
 * @param papszWarpOptions additional options controlling the warp.
 *
 * @return TRUE if the operation completes, or FALSE if an error occurs.
 * @see GDALWarp()
 */

int CPL_STDCALL GDALSimpleImageWarp(GDALDatasetH hSrcDS, GDALDatasetH hDstDS,
                                    int nBandCount, int *panBandList,
                                    GDALTransformerFunc pfnTransform,
                                    void *pTransformArg,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressArg, char **papszWarpOptions)

{
    VALIDATE_POINTER1(hSrcDS, "GDALSimpleImageWarp", 0);
    VALIDATE_POINTER1(hDstDS, "GDALSimpleImageWarp", 0);

    /* -------------------------------------------------------------------- */
    /*      If no bands provided assume we should process all bands.        */
    /* -------------------------------------------------------------------- */
    if (nBandCount == 0)
    {
        nBandCount = GDALGetRasterCount(hSrcDS);
        if (nBandCount == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No raster band in source dataset");
            return FALSE;
        }

        panBandList = static_cast<int *>(CPLCalloc(sizeof(int), nBandCount));

        for (int iBand = 0; iBand < nBandCount; iBand++)
            panBandList[iBand] = iBand + 1;

        const int nResult = GDALSimpleImageWarp(
            hSrcDS, hDstDS, nBandCount, panBandList, pfnTransform,
            pTransformArg, pfnProgress, pProgressArg, papszWarpOptions);
        CPLFree(panBandList);
        return nResult;
    }

    /* -------------------------------------------------------------------- */
    /*      Post initial progress.                                          */
    /* -------------------------------------------------------------------- */
    if (pfnProgress)
    {
        if (!pfnProgress(0.0, "", pProgressArg))
            return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Load the source image band(s).                                  */
    /* -------------------------------------------------------------------- */
    const int nSrcXSize = GDALGetRasterXSize(hSrcDS);
    const int nSrcYSize = GDALGetRasterYSize(hSrcDS);
    std::vector<std::unique_ptr<GByte, VSIFreeReleaser>> apabySrcDataUniquePtr(
        nBandCount);

    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        apabySrcDataUniquePtr[iBand].reset(
            static_cast<GByte *>(VSI_MALLOC2_VERBOSE(nSrcXSize, nSrcYSize)));
        if (apabySrcDataUniquePtr[iBand] == nullptr)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "GDALSimpleImageWarp out of memory.");
            return FALSE;
        }

        if (GDALRasterIO(GDALGetRasterBand(hSrcDS, panBandList[iBand]), GF_Read,
                         0, 0, nSrcXSize, nSrcYSize,
                         apabySrcDataUniquePtr[iBand].get(), nSrcXSize,
                         nSrcYSize, GDT_Byte, 0, 0) != CE_None)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "GDALSimpleImageWarp GDALRasterIO failure %s",
                     CPLGetLastErrorMsg());
            return FALSE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Check for remap request(s).                                     */
    /* -------------------------------------------------------------------- */
    std::vector<GByte *> apabySrcData;
    for (auto &ptr : apabySrcDataUniquePtr)
        apabySrcData.push_back(ptr.get());
    GDALSimpleWarpRemapping(nBandCount, apabySrcData.data(), nSrcXSize,
                            nSrcYSize, papszWarpOptions);

    /* -------------------------------------------------------------------- */
    /*      Allocate scanline buffers for output image.                     */
    /* -------------------------------------------------------------------- */
    const int nDstXSize = GDALGetRasterXSize(hDstDS);
    const int nDstYSize = GDALGetRasterYSize(hDstDS);
    std::vector<std::unique_ptr<GByte, VSIFreeReleaser>> apabyDstLine(
        nBandCount);

    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        apabyDstLine[iBand].reset(
            static_cast<GByte *>(VSI_MALLOC_VERBOSE(nDstXSize)));
        if (!apabyDstLine[iBand])
            return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate x,y,z coordinate arrays for transformation ... one     */
    /*      scanlines worth of positions.                                   */
    /* -------------------------------------------------------------------- */
    std::unique_ptr<double, VSIFreeReleaser> padfX(
        static_cast<double *>(VSI_MALLOC2_VERBOSE(sizeof(double), nDstXSize)));
    std::unique_ptr<double, VSIFreeReleaser> padfY(
        static_cast<double *>(VSI_MALLOC2_VERBOSE(sizeof(double), nDstXSize)));
    std::unique_ptr<double, VSIFreeReleaser> padfZ(
        static_cast<double *>(VSI_MALLOC2_VERBOSE(sizeof(double), nDstXSize)));
    std::unique_ptr<int, VSIFreeReleaser> pabSuccess(
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nDstXSize)));
    if (!padfX || !padfY || !padfZ || !pabSuccess)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Establish the value we will use to initialize the bands.  We    */
    /*      default to -1 indicating the initial value should be read       */
    /*      and preserved from the source file, but allow this to be        */
    /*      overridden by passed                                            */
    /*      option(s).                                                      */
    /* -------------------------------------------------------------------- */
    std::vector<int> anBandInit(nBandCount);
    if (CSLFetchNameValue(papszWarpOptions, "INIT"))
    {
        const CPLStringList aosTokens(CSLTokenizeStringComplex(
            CSLFetchNameValue(papszWarpOptions, "INIT"), " ,", FALSE, FALSE));

        const int nTokenCount = aosTokens.size();

        for (int iBand = 0; iBand < nBandCount; iBand++)
        {
            if (nTokenCount == 0)
                anBandInit[iBand] = 0;
            else
                anBandInit[iBand] =
                    atoi(aosTokens[std::min(iBand, nTokenCount - 1)]);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Loop over all the scanlines in the output image.                */
    /* -------------------------------------------------------------------- */
    for (int iDstY = 0; iDstY < nDstYSize; iDstY++)
    {
        // Clear output buffer to "transparent" value.  Should not we
        // really be reading from the destination file to support overlay?
        for (int iBand = 0; iBand < nBandCount; iBand++)
        {
            if (anBandInit[iBand] == -1)
            {
                if (GDALRasterIO(GDALGetRasterBand(hDstDS, iBand + 1), GF_Read,
                                 0, iDstY, nDstXSize, 1,
                                 apabyDstLine[iBand].get(), nDstXSize, 1,
                                 GDT_Byte, 0, 0) != CE_None)
                {
                    return FALSE;
                }
            }
            else
            {
                memset(apabyDstLine[iBand].get(), anBandInit[iBand], nDstXSize);
            }
        }

        // Set point to transform.
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            (padfX.get())[iDstX] = iDstX + 0.5;
            (padfY.get())[iDstX] = iDstY + 0.5;
            (padfZ.get())[iDstX] = 0.0;
        }

        // Transform the points from destination pixel/line coordinates
        // to source pixel/line coordinates.
        pfnTransform(pTransformArg, TRUE, nDstXSize, padfX.get(), padfY.get(),
                     padfZ.get(), pabSuccess.get());

        // Loop over the output scanline.
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            if (!(pabSuccess.get())[iDstX])
                continue;

            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int.
            if ((padfX.get())[iDstX] < 0.0 || (padfY.get())[iDstX] < 0.0)
                continue;

            const int iSrcX = static_cast<int>((padfX.get())[iDstX]);
            const int iSrcY = static_cast<int>((padfY.get())[iDstX]);

            if (iSrcX >= nSrcXSize || iSrcY >= nSrcYSize)
                continue;

            const int iSrcOffset = iSrcX + iSrcY * nSrcXSize;

            for (int iBand = 0; iBand < nBandCount; iBand++)
                (apabyDstLine[iBand].get())[iDstX] =
                    apabySrcData[iBand][iSrcOffset];
        }

        // Write scanline to disk.
        for (int iBand = 0; iBand < nBandCount; iBand++)
        {
            if (GDALRasterIO(GDALGetRasterBand(hDstDS, iBand + 1), GF_Write, 0,
                             iDstY, nDstXSize, 1, apabyDstLine[iBand].get(),
                             nDstXSize, 1, GDT_Byte, 0, 0) != CE_None)
            {
                return FALSE;
            }
        }

        if (pfnProgress != nullptr)
        {
            if (!pfnProgress((iDstY + 1) / static_cast<double>(nDstYSize), "",
                             pProgressArg))
            {
                CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                return FALSE;
            }
        }
    }

    return TRUE;
}
