/******************************************************************************
 * Project:  GDAL
 * Author:   Raul Alonso Reyes <raul dot alonsoreyes at satcen dot europa dot eu>
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 * Purpose:  JPEG-2000 driver based on Lurawave library, driver developed by SatCen
 *
 ******************************************************************************
 * Copyright (c) 2016, SatCen - European Union Satellite Centre
 * Copyright (c) 2016, Even Rouault
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

#include "jp2lurarasterband.h"
#include "jp2luradataset.h"

/************************************************************************/
/*                        JP2LuraRasterBand()                           */
/************************************************************************/

JP2LuraRasterBand::JP2LuraRasterBand(JP2LuraDataset *poDSIn, int nBandIn,
                                     GDALDataType eDataTypeIn,
                                     int nBits,
                                     int nBlockXSizeIn, int nBlockYSizeIn)

{
    eDataType = eDataTypeIn;
    nBlockXSize = nBlockXSizeIn;
    nBlockYSize = nBlockYSizeIn;
    poDS = poDSIn;
    nRasterXSize = poDSIn->nRasterXSize;
    nRasterYSize = poDSIn->nRasterYSize;
    nBand = nBandIn;

    if (nRasterXSize == nBlockXSize && nRasterYSize == nBlockYSize)
    {
    /* -------------------------------------------------------------------- */
    /*      Use a 2048x128 "virtual" block size unless the file is small.   */
    /* -------------------------------------------------------------------- */
        if (nRasterXSize >= 2048)
        {
            nBlockXSize = 2048;
        }
        else
        {
            nBlockXSize = nRasterXSize;
        }

        if (nRasterYSize >= 128)
        {
            nBlockYSize = 128;
        }
        else
        {
            nBlockYSize = nRasterYSize;
        }
    }

    if( (nBits % 8) != 0 )
    {
        GDALRasterBand::SetMetadataItem("NBITS",
                        CPLString().Printf("%d",nBits),
                        "IMAGE_STRUCTURE" );
    }
    GDALRasterBand::SetMetadataItem("COMPRESSION", "JPEG2000",
                    "IMAGE_STRUCTURE" );

    bForceCachedIO = FALSE;
}

/************************************************************************/
/*                      ~JP2OpenJPEGRasterBand()                        */
/************************************************************************/

JP2LuraRasterBand::~JP2LuraRasterBand()
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JP2LuraRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                     void * pImage)
{
    JP2LuraDataset *poGDS = reinterpret_cast<JP2LuraDataset *>(poDS);
#ifdef DEBUG_VERBOSE
    CPLDebug("JP2Lura", "IReadBlock(nBand=%d,nLevel=%d %d,%d)",
             nBand, poGDS->iLevel, nBlockXOff, nBlockYOff);
#endif
    int nXOff = nBlockXOff * nBlockXSize;
    int nYOff = nBlockYOff * nBlockYSize;
    int nXSize = nBlockXSize;
    int nYSize = nBlockYSize;
    if( nXOff + nXSize > nRasterXSize )
        nXSize = nRasterXSize - nXOff;
    if( nYOff + nYSize > nRasterYSize )
        nYSize = nRasterYSize - nYOff;
    GDALRasterIOExtraArg sExtraArgs;
    INIT_RASTERIO_EXTRA_ARG(sExtraArgs);
    const int nDTSizeBytes = GDALGetDataTypeSizeBytes(eDataType);
    CPLErr eErr = IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                            pImage, nXSize, nYSize,
                            eDataType,
                            nDTSizeBytes,
                            nDTSizeBytes * nXSize,
                            &sExtraArgs);

    // Unpack previously packed buffer if needed
    if( eErr == CE_None && nXSize < nBlockXSize )
    {
        GByte* pabyData = reinterpret_cast<GByte*>(pImage);
        for( int j = nYSize - 1; j >= 0; --j )
        {
            memmove( pabyData + j * nBlockXSize * nDTSizeBytes,
                     pabyData + j * nXSize * nDTSizeBytes,
                     nXSize * nDTSizeBytes );
        }
    }

    // Caching other bands while we have the cached buffer valid
    for( int iBand = 1;eErr == CE_None && iBand <= poGDS->nBands; iBand++ )
    {
        if( iBand == nBand )
            continue;
        JP2LuraRasterBand* poOtherBand =
            reinterpret_cast<JP2LuraRasterBand*>(poGDS->GetRasterBand(iBand));
        GDALRasterBlock* poBlock = poOtherBand->
                                TryGetLockedBlockRef(nBlockXOff,nBlockYOff);
        if (poBlock != nullptr)
        {
            poBlock->DropLock();
            continue;
        }

        poBlock = poOtherBand->
                            GetLockedBlockRef(nBlockXOff,nBlockYOff, TRUE);
        if (poBlock == nullptr)
        {
            continue;
        }

        GByte* pabyData = reinterpret_cast<GByte*>(poBlock->GetDataRef());
        eErr = poOtherBand->IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                        pabyData, nXSize, nYSize,
                        eDataType,
                        nDTSizeBytes,
                        nDTSizeBytes * nXSize,
                        &sExtraArgs);

        // Unpack previously packed buffer if needed
        if( eErr == CE_None && nXSize < nBlockXSize )
        {
            for( int j = nYSize - 1; j >= 0; --j )
            {
                memmove( pabyData + j * nBlockXSize * nDTSizeBytes,
                        pabyData + j * nXSize * nDTSizeBytes,
                        nXSize * nDTSizeBytes );
            }
        }

        poBlock->DropLock();
    }

    return eErr;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JP2LuraRasterBand::IRasterIO(GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void * pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    GSpacing nPixelSpace, GSpacing nLineSpace,
                                    GDALRasterIOExtraArg* psExtraArg)
{
    JP2LuraDataset *poGDS = reinterpret_cast<JP2LuraDataset *>(poDS);
    const int nBufTypeSize = GDALGetDataTypeSizeBytes(eBufType);

    if (eRWFlag != GF_Read)
        return CE_Failure;

#ifdef DEBUG_VERBOSE
    CPLDebug("JP2Lura", "RasterIO(nBand=%d,nLevel=%d %d,%d,%dx%d -> %dx%d)",
             nBand, poGDS->iLevel, nXOff, nYOff, nXSize, nYSize,
             nBufXSize, nBufYSize);
#endif
    if( eBufType != eDataType ||
        nPixelSpace != nBufTypeSize ||
        nLineSpace != nPixelSpace * nBufXSize )
    {
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace, psExtraArg);
    }

    // Use cached data
    if( poGDS->sOutputData.nXOff == nXOff &&
        poGDS->sOutputData.nYOff == nYOff &&
        poGDS->sOutputData.nXSize == nXSize &&
        poGDS->sOutputData.nYSize == nYSize &&
        poGDS->sOutputData.nBufXSize == nBufXSize &&
        poGDS->sOutputData.nBufYSize == nBufYSize &&
        poGDS->sOutputData.eBufType == eBufType )
    {
        if (poGDS->sOutputData.pDatacache[nBand - 1] != nullptr)
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("JP2Lura", "Using cached data");
#endif
            memcpy(pData, poGDS->sOutputData.pDatacache[nBand - 1],
                   static_cast<size_t>(nBufXSize)*nBufYSize*nBufTypeSize);
            return CE_None;
        }
    }

    /* ==================================================================== */
    /*      Do we have overviews that would be appropriate to satisfy       */
    /*      this request?                                                   */
    /* ==================================================================== */
    if ((nBufXSize < nXSize || nBufYSize < nYSize)
            && GetOverviewCount() > 0)
    {
        int         nOverview;
        GDALRasterIOExtraArg sExtraArg;

        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        nOverview = GDALBandGetBestOverviewLevel2(
                                        this, nXOff, nYOff, nXSize, nYSize,
                                        nBufXSize, nBufYSize, &sExtraArg);
        if (nOverview >= 0)
        {
            GDALRasterBand* poOverviewBand = GetOverview(nOverview);
            if (poOverviewBand == nullptr)
                    return CE_Failure;

            return poOverviewBand->RasterIO(
                                eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                pData, nBufXSize, nBufYSize, eBufType,
                                nPixelSpace, nLineSpace, &sExtraArg);
        }
    }

    if( nBufXSize != nXSize || nBufYSize != nYSize )
    {
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace, psExtraArg);
    }

    JP2_Error error = 0;
    const short factor = 1 << poGDS->iLevel;

    JP2_Rect comp_region;
    comp_region.ulLeft = nXOff;
    comp_region.ulRight = nXOff + nXSize;
    comp_region.ulTop = nYOff;
    comp_region.ulBottom = nYOff + nYSize;

    error = JP2_Decompress_SetProp(poGDS->sOutputData.handle,
                                   cJP2_Prop_Scale_Down, factor);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).",
                 JP2LuraDataset::GetErrorMessage(error));
        return CE_Failure;
    }

    poGDS->sOutputData.pimage = reinterpret_cast<unsigned char*>(pData);

    poGDS->sOutputData.nXOff = nXOff;
    poGDS->sOutputData.nYOff = nYOff;
    poGDS->sOutputData.nXSize = nXSize;
    poGDS->sOutputData.nYSize = nYSize;
    poGDS->sOutputData.nBufXSize = nBufXSize;
    poGDS->sOutputData.nBufYSize = nBufYSize;
    poGDS->sOutputData.eBufType = eBufType;

    poGDS->sOutputData.nBand = nBand;
    poGDS->sOutputData.nBands = poGDS->nBands;
    if( poGDS->sOutputData.pDatacache == nullptr )
    {
        poGDS->sOutputData.pDatacache = reinterpret_cast<unsigned char**>
                            (CPLCalloc( poGDS->nBands, sizeof(unsigned char*)));
    }

    for (int i = 0; i < poGDS->nBands; i++)
    {
        if (poGDS->sOutputData.pDatacache[i] != nullptr)
        {
            VSIFree(poGDS->sOutputData.pDatacache[i]);
            poGDS->sOutputData.pDatacache[i] = nullptr;
        }
        if (i == nBand-1)
            continue;
        poGDS->sOutputData.pDatacache[i] =
            reinterpret_cast<unsigned char*>(VSIMalloc(
                        static_cast<size_t>(nBufXSize)*nBufYSize*nBufTypeSize));
    }

    /*++++++++++++++++++++++++++++++++++++++++++++++*/
    /* Set the callback function and parameter      */
    /*++++++++++++++++++++++++++++++++++++++++++++++*/

    error = JP2_Decompress_SetProp(poGDS->sOutputData.handle,
                cJP2_Prop_Output_Parameter,
                reinterpret_cast<JP2_Property_Value>(&(poGDS->sOutputData)));
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).",
                 JP2LuraDataset::GetErrorMessage(error));
        return CE_Failure;
    }

    error = JP2_Decompress_SetProp(poGDS->sOutputData.handle,
                    cJP2_Prop_Output_Function,
                    reinterpret_cast<JP2_Property_Value>(
                                    GDALJP2Lura_Callback_Decompress_Write));
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error (%s).",
                 JP2LuraDataset::GetErrorMessage(error));
        return CE_Failure;
    }

    error = JP2_Decompress_Region(poGDS->sOutputData.handle, comp_region);
    if( error )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal library error during decompress region (%s).",
                 JP2LuraDataset::GetErrorMessage(error));
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int JP2LuraRasterBand::GetOverviewCount()
{
    JP2LuraDataset *poGDS = reinterpret_cast<JP2LuraDataset *>(poDS);
    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand* JP2LuraRasterBand::GetOverview(int iOvrLevel)
{
    JP2LuraDataset *poGDS = reinterpret_cast<JP2LuraDataset *>(poDS);
    if (iOvrLevel < 0 || iOvrLevel >= poGDS->nOverviewCount)
        return nullptr;

    return poGDS->papoOverviewDS[iOvrLevel]->GetRasterBand(nBand);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JP2LuraRasterBand::GetColorInterpretation()
{
    JP2LuraDataset *poGDS = reinterpret_cast<JP2LuraDataset *>(poDS);

    if( poGDS->poCT )
        return GCI_PaletteIndex;

    if( nBand == poGDS->nAlphaIndex + 1 )
        return GCI_AlphaBand;

    if (poGDS->nBands <= 2 && poGDS->eColorspace == cJP2_Colorspace_Gray)
        return GCI_GrayIndex;
    else if (poGDS->eColorspace == cJP2_Colorspace_RGBa)
    {
        if( nBand == poGDS->nRedIndex + 1 )
            return GCI_RedBand;
        if( nBand == poGDS->nGreenIndex + 1 )
            return GCI_GreenBand;
        if( nBand == poGDS->nBlueIndex + 1 )
            return GCI_BlueBand;
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                          GetColorTable()                             */
/************************************************************************/

GDALColorTable* JP2LuraRasterBand::GetColorTable()
{
    JP2LuraDataset *poGDS = reinterpret_cast<JP2LuraDataset *>(poDS);
    return poGDS->poCT;
}
