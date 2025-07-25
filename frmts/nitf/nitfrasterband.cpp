/******************************************************************************
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  NITFRasterBand (and related proxy band) implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006, 2020
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "nitfdataset.h"

#include <climits>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <algorithm>
#include <map>
#include <utility>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "nitflib.h"

/************************************************************************/
/*                       NITFMakeColorTable()                           */
/************************************************************************/

static GDALColorTable *NITFMakeColorTable(NITFImage *psImage,
                                          NITFBandInfo *psBandInfo)
{
    GDALColorTable *poColorTable = nullptr;

    if (psBandInfo->nSignificantLUTEntries > 0)
    {
        poColorTable = new GDALColorTable();

        for (int iColor = 0; iColor < psBandInfo->nSignificantLUTEntries;
             iColor++)
        {
            GDALColorEntry sEntry;
            sEntry.c1 = psBandInfo->pabyLUT[0 + iColor];
            sEntry.c2 = psBandInfo->pabyLUT[256 + iColor];
            sEntry.c3 = psBandInfo->pabyLUT[512 + iColor];
            sEntry.c4 = 255;

            poColorTable->SetColorEntry(iColor, &sEntry);
        }

        if (psImage->bNoDataSet)
        {
            GDALColorEntry sEntry = {0, 0, 0, 0};
            poColorTable->SetColorEntry(psImage->nNoDataValue, &sEntry);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      We create a color table for 1 bit data too...                   */
    /* -------------------------------------------------------------------- */
    if (poColorTable == nullptr && psImage->nBitsPerSample == 1)
    {
        poColorTable = new GDALColorTable();

        GDALColorEntry sEntry;
        sEntry.c1 = 0;
        sEntry.c2 = 0;
        sEntry.c3 = 0;
        sEntry.c4 = 255;
        poColorTable->SetColorEntry(0, &sEntry);

        sEntry.c1 = 255;
        sEntry.c2 = 255;
        sEntry.c3 = 255;
        sEntry.c4 = 255;
        poColorTable->SetColorEntry(1, &sEntry);
    }

    return poColorTable;
}

/************************************************************************/
/* ==================================================================== */
/*                        NITFProxyPamRasterBand                        */
/* ==================================================================== */
/************************************************************************/

NITFProxyPamRasterBand::~NITFProxyPamRasterBand()
{
    std::map<CPLString, char **>::iterator oIter = oMDMap.begin();
    while (oIter != oMDMap.end())
    {
        CSLDestroy(oIter->second);
        ++oIter;
    }
}

char **NITFProxyPamRasterBand::GetMetadata(const char *pszDomain)
{
    GDALRasterBand *_poSrcBand = RefUnderlyingRasterBand();
    if (_poSrcBand)
    {
        /* Let's merge metadata of PAM and the underlying band */
        /* PAM metadata should override underlying band metadata */
        char **papszMD = CSLDuplicate(_poSrcBand->GetMetadata(pszDomain));
        papszMD = CSLMerge(papszMD, GDALPamRasterBand::GetMetadata(pszDomain));

        if (pszDomain == nullptr)
            pszDomain = "";

        std::map<CPLString, char **>::iterator oIter = oMDMap.find(pszDomain);
        if (oIter != oMDMap.end())
            CSLDestroy(oIter->second);
        oMDMap[pszDomain] = papszMD;
        UnrefUnderlyingRasterBand(_poSrcBand);

        return papszMD;
    }

    return GDALPamRasterBand::GetMetadata(pszDomain);
}

const char *NITFProxyPamRasterBand::GetMetadataItem(const char *pszName,
                                                    const char *pszDomain)
{
    const char *pszRet = GDALPamRasterBand::GetMetadataItem(pszName, pszDomain);
    if (pszRet)
        return pszRet;

    GDALRasterBand *_poSrcBand = RefUnderlyingRasterBand();
    if (_poSrcBand)
    {
        if (!m_bEnablePixelTypeSignedByteWarning)
            _poSrcBand->EnablePixelTypeSignedByteWarning(false);
        pszRet = _poSrcBand->GetMetadataItem(pszName, pszDomain);
        _poSrcBand->EnablePixelTypeSignedByteWarning(true);
        UnrefUnderlyingRasterBand(_poSrcBand);
    }

    return pszRet;
}

CPLErr NITFProxyPamRasterBand::GetStatistics(int bApproxOK, int bForce,
                                             double *pdfMin, double *pdfMax,
                                             double *pdfMean, double *pdfStdDev)
{
    /* -------------------------------------------------------------------- */
    /*      Do we already have metadata items for the requested values?     */
    /* -------------------------------------------------------------------- */
    if ((pdfMin == nullptr ||
         GetMetadataItem("STATISTICS_MINIMUM") != nullptr) &&
        (pdfMax == nullptr ||
         GetMetadataItem("STATISTICS_MAXIMUM") != nullptr) &&
        (pdfMean == nullptr || GetMetadataItem("STATISTICS_MEAN") != nullptr) &&
        (pdfStdDev == nullptr ||
         GetMetadataItem("STATISTICS_STDDEV") != nullptr))
    {
        return GDALPamRasterBand::GetStatistics(bApproxOK, bForce, pdfMin,
                                                pdfMax, pdfMean, pdfStdDev);
    }

    GDALRasterBand *_poSrcBand = RefUnderlyingRasterBand();
    if (_poSrcBand)
    {
        CPLErr ret = _poSrcBand->GetStatistics(bApproxOK, bForce, pdfMin,
                                               pdfMax, pdfMean, pdfStdDev);
        if (ret == CE_None)
        {
            /* Report underlying statistics at PAM level */
            SetMetadataItem("STATISTICS_MINIMUM",
                            _poSrcBand->GetMetadataItem("STATISTICS_MINIMUM"));
            SetMetadataItem("STATISTICS_MAXIMUM",
                            _poSrcBand->GetMetadataItem("STATISTICS_MAXIMUM"));
            SetMetadataItem("STATISTICS_MEAN",
                            _poSrcBand->GetMetadataItem("STATISTICS_MEAN"));
            SetMetadataItem("STATISTICS_STDDEV",
                            _poSrcBand->GetMetadataItem("STATISTICS_STDDEV"));
        }
        UnrefUnderlyingRasterBand(_poSrcBand);
        return ret;
    }

    return CE_Failure;
}

CPLErr NITFProxyPamRasterBand::ComputeStatistics(
    int bApproxOK, double *pdfMin, double *pdfMax, double *pdfMean,
    double *pdfStdDev, GDALProgressFunc pfn, void *pProgressData)
{
    GDALRasterBand *_poSrcBand = RefUnderlyingRasterBand();
    if (_poSrcBand)
    {
        CPLErr ret = _poSrcBand->ComputeStatistics(
            bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev, pfn, pProgressData);
        if (ret == CE_None)
        {
            /* Report underlying statistics at PAM level */
            SetMetadataItem("STATISTICS_MINIMUM",
                            _poSrcBand->GetMetadataItem("STATISTICS_MINIMUM"));
            SetMetadataItem("STATISTICS_MAXIMUM",
                            _poSrcBand->GetMetadataItem("STATISTICS_MAXIMUM"));
            SetMetadataItem("STATISTICS_MEAN",
                            _poSrcBand->GetMetadataItem("STATISTICS_MEAN"));
            SetMetadataItem("STATISTICS_STDDEV",
                            _poSrcBand->GetMetadataItem("STATISTICS_STDDEV"));
        }
        UnrefUnderlyingRasterBand(_poSrcBand);
        return ret;
    }

    return CE_Failure;
}

#define RB_PROXY_METHOD_GET_DBL_WITH_SUCCESS(methodName)                       \
    double NITFProxyPamRasterBand::methodName(int *pbSuccess)                  \
    {                                                                          \
        int bSuccess = FALSE;                                                  \
        double dfRet = GDALPamRasterBand::methodName(&bSuccess);               \
        if (bSuccess)                                                          \
        {                                                                      \
            if (pbSuccess)                                                     \
                *pbSuccess = TRUE;                                             \
            return dfRet;                                                      \
        }                                                                      \
        GDALRasterBand *_poSrcBand = RefUnderlyingRasterBand();                \
        if (_poSrcBand)                                                        \
        {                                                                      \
            dfRet = _poSrcBand->methodName(pbSuccess);                         \
            UnrefUnderlyingRasterBand(_poSrcBand);                             \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            dfRet = 0;                                                         \
        }                                                                      \
        return dfRet;                                                          \
    }

RB_PROXY_METHOD_GET_DBL_WITH_SUCCESS(GetNoDataValue)
RB_PROXY_METHOD_GET_DBL_WITH_SUCCESS(GetMinimum)
RB_PROXY_METHOD_GET_DBL_WITH_SUCCESS(GetMaximum)

#define RB_PROXY_METHOD_WITH_RET_AND_CALL_OTHER_METHOD(                        \
    retType, retErrValue, methodName, underlyingMethodName, argList,           \
    argParams)                                                                 \
    retType NITFProxyPamRasterBand::methodName argList                         \
    {                                                                          \
        retType ret;                                                           \
        GDALRasterBand *_poSrcBand = RefUnderlyingRasterBand();                \
        if (_poSrcBand)                                                        \
        {                                                                      \
            ret = _poSrcBand->underlyingMethodName argParams;                  \
            UnrefUnderlyingRasterBand(_poSrcBand);                             \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            ret = retErrValue;                                                 \
        }                                                                      \
        return ret;                                                            \
    }

RB_PROXY_METHOD_WITH_RET_AND_CALL_OTHER_METHOD(CPLErr, CE_Failure, IReadBlock,
                                               ReadBlock,
                                               (int nXBlockOff, int nYBlockOff,
                                                void *pImage),
                                               (nXBlockOff, nYBlockOff, pImage))
RB_PROXY_METHOD_WITH_RET_AND_CALL_OTHER_METHOD(CPLErr, CE_Failure, IWriteBlock,
                                               WriteBlock,
                                               (int nXBlockOff, int nYBlockOff,
                                                void *pImage),
                                               (nXBlockOff, nYBlockOff, pImage))
RB_PROXY_METHOD_WITH_RET_AND_CALL_OTHER_METHOD(
    CPLErr, CE_Failure, IRasterIO, RasterIO,
    (GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
     void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
     GSpacing nPixelSpace, GSpacing nLineSpace,
     GDALRasterIOExtraArg *psExtraArg),
    (eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
     eBufType, nPixelSpace, nLineSpace, psExtraArg))

#define RB_PROXY_METHOD_WITH_RET(retType, retErrValue, methodName, argList,    \
                                 argParams)                                    \
    retType NITFProxyPamRasterBand::methodName argList                         \
    {                                                                          \
        retType ret;                                                           \
        GDALRasterBand *_poSrcBand = RefUnderlyingRasterBand();                \
        if (_poSrcBand)                                                        \
        {                                                                      \
            ret = _poSrcBand->methodName argParams;                            \
            UnrefUnderlyingRasterBand(_poSrcBand);                             \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            ret = retErrValue;                                                 \
        }                                                                      \
        return ret;                                                            \
    }

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, FlushCache, (bool bAtClosing),
                         (bAtClosing))

RB_PROXY_METHOD_WITH_RET(GDALColorInterp, GCI_Undefined, GetColorInterpretation,
                         (), ())
RB_PROXY_METHOD_WITH_RET(GDALColorTable *, nullptr, GetColorTable, (), ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, Fill,
                         (double dfRealValue, double dfImaginaryValue),
                         (dfRealValue, dfImaginaryValue))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, ComputeRasterMinMax,
                         (int arg1, double *arg2), (arg1, arg2))

RB_PROXY_METHOD_WITH_RET(int, 0, HasArbitraryOverviews, (), ())
RB_PROXY_METHOD_WITH_RET(int, 0, GetOverviewCount, (), ())
RB_PROXY_METHOD_WITH_RET(GDALRasterBand *, nullptr, GetOverview, (int arg1),
                         (arg1))
RB_PROXY_METHOD_WITH_RET(GDALRasterBand *, nullptr, GetRasterSampleOverview,
                         (GUIntBig arg1), (arg1))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, BuildOverviews,
                         (const char *arg1, int arg2, const int *arg3,
                          GDALProgressFunc arg4, void *arg5,
                          CSLConstList papszOptions),
                         (arg1, arg2, arg3, arg4, arg5, papszOptions))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, AdviseRead,
                         (int nXOff, int nYOff, int nXSize, int nYSize,
                          int nBufXSize, int nBufYSize, GDALDataType eDT,
                          char **papszOptions),
                         (nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                          eDT, papszOptions))

RB_PROXY_METHOD_WITH_RET(GDALRasterBand *, nullptr, GetMaskBand, (), ())
RB_PROXY_METHOD_WITH_RET(int, 0, GetMaskFlags, (), ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, CreateMaskBand, (int nFlagsIn),
                         (nFlagsIn))

/************************************************************************/
/*                 UnrefUnderlyingRasterBand()                        */
/************************************************************************/

void NITFProxyPamRasterBand::UnrefUnderlyingRasterBand(
    CPL_UNUSED GDALRasterBand *poUnderlyingRasterBand)
{
}

/************************************************************************/
/* ==================================================================== */
/*                            NITFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           NITFRasterBand()                           */
/************************************************************************/

NITFRasterBand::NITFRasterBand(NITFDataset *poDSIn, int nBandIn)
    : psImage(poDSIn->psImage)
{
    NITFBandInfo *psBandInfo = poDSIn->psImage->pasBandInfo + nBandIn - 1;

    poDS = poDSIn;
    nBand = nBandIn;
    eAccess = poDSIn->eAccess;

    /* -------------------------------------------------------------------- */
    /*      Translate data type(s).                                         */
    /* -------------------------------------------------------------------- */
    if (psImage->nBitsPerSample <= 8)
        eDataType = GDT_Byte;
    else if (psImage->nBitsPerSample == 16 && EQUAL(psImage->szPVType, "SI"))
        eDataType = GDT_Int16;
    else if (psImage->nBitsPerSample == 16)
        eDataType = GDT_UInt16;
    else if (psImage->nBitsPerSample == 12)
        eDataType = GDT_UInt16;
    else if (psImage->nBitsPerSample == 32 && EQUAL(psImage->szPVType, "SI"))
        eDataType = GDT_Int32;
    else if (psImage->nBitsPerSample == 32 && EQUAL(psImage->szPVType, "R"))
        eDataType = GDT_Float32;
    else if (psImage->nBitsPerSample == 32)
        eDataType = GDT_UInt32;
    else if (psImage->nBitsPerSample == 64 && EQUAL(psImage->szPVType, "R"))
        eDataType = GDT_Float64;
    else if (psImage->nBitsPerSample == 64 && EQUAL(psImage->szPVType, "C"))
        eDataType = GDT_CFloat32;
    /* ERO : note I'm not sure if CFloat64 can be transmitted as NBPP is only 2
     * characters */
    else
    {
        int bOpenUnderlyingDS =
            CPLTestBool(CPLGetConfigOption("NITF_OPEN_UNDERLYING_DS", "YES"));
        if (!bOpenUnderlyingDS && psImage->nBitsPerSample > 8 &&
            psImage->nBitsPerSample < 16)
        {
            if (EQUAL(psImage->szPVType, "SI"))
                eDataType = GDT_Int16;
            else
                eDataType = GDT_UInt16;
        }
        else
        {
            eDataType = GDT_Unknown;
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unsupported combination of PVTYPE(%s) and NBPP(%d).",
                     psImage->szPVType, psImage->nBitsPerSample);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Work out block size. If the image is all one big block we       */
    /*      handle via the scanline access API.                             */
    /* -------------------------------------------------------------------- */
    if (psImage->nBlocksPerRow == 1 && psImage->nBlocksPerColumn == 1 &&
        psImage->nBitsPerSample >= 8 && EQUAL(psImage->szIC, "NC"))
    {
        bScanlineAccess = TRUE;
        nBlockXSize = psImage->nBlockWidth;
        nBlockYSize = 1;
    }
    else
    {
        bScanlineAccess = FALSE;
        nBlockXSize = psImage->nBlockWidth;
        nBlockYSize = psImage->nBlockHeight;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have a color table?                                       */
    /* -------------------------------------------------------------------- */
    poColorTable = NITFMakeColorTable(psImage, psBandInfo);

    if (psImage->nABPP != 8 && psImage->nABPP != 16 && psImage->nABPP != 32 &&
        psImage->nABPP != 64)
    {
        SetMetadataItem("NBITS", CPLString().Printf("%d", psImage->nABPP),
                        "IMAGE_STRUCTURE");
    }

    if (psImage->nBitsPerSample == 3 || psImage->nBitsPerSample == 5 ||
        psImage->nBitsPerSample == 6 || psImage->nBitsPerSample == 7)
    {
        if (nBlockXSize > (INT_MAX - 7) / nBlockYSize)
        {
            eDataType = GDT_Unknown;
        }
        else
        {
            pUnpackData = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(((nBlockXSize * nBlockYSize + 7) / 8) * 8));
            if (pUnpackData == nullptr)
                eDataType = GDT_Unknown;
        }
    }
}

/************************************************************************/
/*                          ~NITFRasterBand()                           */
/************************************************************************/

NITFRasterBand::~NITFRasterBand()

{
    if (poColorTable != nullptr)
        delete poColorTable;

    VSIFree(pUnpackData);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr NITFRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    NITFDataset *poGDS = cpl::down_cast<NITFDataset *>(poDS);

    /* -------------------------------------------------------------------- */
    /*      Special case for JPEG blocks.                                   */
    /* -------------------------------------------------------------------- */
    if (EQUAL(psImage->szIC, "C3") || EQUAL(psImage->szIC, "M3"))
    {
        CPLErr eErr = poGDS->ReadJPEGBlock(nBlockXOff, nBlockYOff);
        const int nBlockBandSize = psImage->nBlockWidth *
                                   psImage->nBlockHeight *
                                   GDALGetDataTypeSizeBytes(eDataType);

        if (eErr != CE_None)
            return eErr;

        memcpy(pImage, poGDS->pabyJPEGBlock + (nBand - 1) * nBlockBandSize,
               nBlockBandSize);

        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the line/block                                             */
    /* -------------------------------------------------------------------- */
    int nBlockResult;

    if (bScanlineAccess)
    {
        nBlockResult = NITFReadImageLine(psImage, nBlockYOff, nBand, pImage);
    }
    else
    {
        nBlockResult =
            NITFReadImageBlock(psImage, nBlockXOff, nBlockYOff, nBand, pImage);
    }

    if (nBlockResult == BLKREAD_OK)
    {
        if (psImage->nBitsPerSample % 8)
            Unpack(reinterpret_cast<GByte *>(pImage));

        return CE_None;
    }

    if (nBlockResult == BLKREAD_FAIL)
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      If we got a null/missing block, try to fill it in with the      */
    /*      nodata value.  It seems this only really works properly for     */
    /*      8bit.                                                           */
    /* -------------------------------------------------------------------- */
    if (psImage->bNoDataSet)
        memset(pImage, psImage->nNoDataValue,
               static_cast<size_t>(psImage->nWordSize) * psImage->nBlockWidth *
                   psImage->nBlockHeight);
    else
        memset(pImage, 0,
               static_cast<size_t>(psImage->nWordSize) * psImage->nBlockWidth *
                   psImage->nBlockHeight);

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr NITFRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    /* -------------------------------------------------------------------- */
    /*      Write the line/block                                            */
    /* -------------------------------------------------------------------- */
    int nBlockResult;

    if (bScanlineAccess)
    {
        nBlockResult = NITFWriteImageLine(psImage, nBlockYOff, nBand, pImage);
    }
    else
    {
        nBlockResult =
            NITFWriteImageBlock(psImage, nBlockXOff, nBlockYOff, nBand, pImage);
    }

    if (nBlockResult == BLKREAD_OK)
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double NITFRasterBand::GetNoDataValue(int *pbSuccess)

{
    if (pbSuccess != nullptr)
        *pbSuccess = psImage->bNoDataSet;

    if (psImage->bNoDataSet)
        return psImage->nNoDataValue;

    return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp NITFRasterBand::GetColorInterpretation()

{
    NITFBandInfo *psBandInfo = psImage->pasBandInfo + nBand - 1;

    if (poColorTable != nullptr)
        return GCI_PaletteIndex;

    if (EQUAL(psBandInfo->szIREPBAND, "R"))
        return GCI_RedBand;
    if (EQUAL(psBandInfo->szIREPBAND, "G"))
        return GCI_GreenBand;
    if (EQUAL(psBandInfo->szIREPBAND, "B"))
        return GCI_BlueBand;
    if (EQUAL(psBandInfo->szIREPBAND, "M"))
        return GCI_GrayIndex;
    if (EQUAL(psBandInfo->szIREPBAND, "Y"))
        return GCI_YCbCr_YBand;
    if (EQUAL(psBandInfo->szIREPBAND, "Cb"))
        return GCI_YCbCr_CbBand;
    if (EQUAL(psBandInfo->szIREPBAND, "Cr"))
        return GCI_YCbCr_CrBand;

    return GCI_Undefined;
}

/************************************************************************/
/*                     NITFSetColorInterpretation()                     */
/************************************************************************/

CPLErr NITFSetColorInterpretation(NITFImage *psImage, int nBand,
                                  GDALColorInterp eInterp)

{
    const char *pszREP = nullptr;

    if (eInterp == GCI_RedBand)
        pszREP = "R";
    else if (eInterp == GCI_GreenBand)
        pszREP = "G";
    else if (eInterp == GCI_BlueBand)
        pszREP = "B";
    else if (eInterp == GCI_GrayIndex)
        pszREP = "M";
    else if (eInterp == GCI_YCbCr_YBand)
        pszREP = "Y";
    else if (eInterp == GCI_YCbCr_CbBand)
        pszREP = "Cb";
    else if (eInterp == GCI_YCbCr_CrBand)
        pszREP = "Cr";
    else if (eInterp == GCI_Undefined)
        return CE_None;

    if (pszREP == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Requested color interpretation (%s) not supported in NITF.",
                 GDALGetColorInterpretationName(eInterp));
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Where does this go in the file?                                 */
    /* -------------------------------------------------------------------- */
    NITFBandInfo *psBandInfo = psImage->pasBandInfo + nBand - 1;
    strcpy(psBandInfo->szIREPBAND, pszREP);
    GUIntBig nOffset = NITFIHFieldOffset(psImage, "IREPBAND");

    if (nOffset != 0)
        nOffset += (nBand - 1) * 13;

    /* -------------------------------------------------------------------- */
    /*      write it (space padded).                                        */
    /* -------------------------------------------------------------------- */
    char szPadded[4];
    strcpy(szPadded, pszREP);
    strcat(szPadded, " ");

    if (nOffset != 0)
    {
        if (VSIFSeekL(psImage->psFile->fp, nOffset, SEEK_SET) != 0 ||
            VSIFWriteL(reinterpret_cast<void *>(szPadded), 1, 2,
                       psImage->psFile->fp) != 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "IO failure writing new IREPBAND value to NITF file.");
            return CE_Failure;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr NITFRasterBand::SetColorInterpretation(GDALColorInterp eInterp)

{
    return NITFSetColorInterpretation(psImage, nBand, eInterp);
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *NITFRasterBand::GetColorTable()

{
    return poColorTable;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr NITFRasterBand::SetColorTable(GDALColorTable *poNewCT)

{
    NITFDataset *poGDS = cpl::down_cast<NITFDataset *>(poDS);
    if (poGDS->bInLoadXML)
        return GDALPamRasterBand::SetColorTable(poNewCT);

    if (poNewCT == nullptr)
        return CE_Failure;

    std::vector<GByte> abyNITFLUT(768);

    const int nCount = std::min(256, poNewCT->GetColorEntryCount());
    for (int i = 0; i < nCount; i++)
    {
        GDALColorEntry sEntry;

        poNewCT->GetColorEntryAsRGB(i, &sEntry);
        abyNITFLUT[i + 256 * 0] = static_cast<GByte>(sEntry.c1);
        abyNITFLUT[i + 256 * 1] = static_cast<GByte>(sEntry.c2);
        abyNITFLUT[i + 256 * 2] = static_cast<GByte>(sEntry.c3);
    }

    if (NITFWriteLUT(psImage, nBand, nCount, abyNITFLUT.data()))
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                           Unpack()                                   */
/************************************************************************/

void NITFRasterBand::Unpack(GByte *pData)
{
    const int n = nBlockXSize * nBlockYSize;

    GByte abyTempData[7] = {0, 0, 0, 0, 0, 0, 0};
    const GByte *pDataSrc = pData;
    if (n < psImage->nBitsPerSample && psImage->nBitsPerSample < 8)
    {
        memcpy(abyTempData, pData, n);
        pDataSrc = abyTempData;
    }

    switch (psImage->nBitsPerSample)
    {
        case 1:
        {
            // unpack 1-bit in-place in reverse
            // DANGER: Non-standard decrement of counter in the test section of
            // for.
            for (int i = n; --i >= 0;)
                pData[i] = (pData[i >> 3] & (0x80 >> (i & 7))) != 0;

            break;
        }
        case 2:
        {
            constexpr int s_Shift2[] = {6, 4, 2, 0};
            // unpack 2-bit in-place in reverse
            // DANGER: Non-standard decrement of counter in the test section of
            // for.
            for (int i = n; --i >= 0;)
                pData[i] =
                    (pData[i >> 2] >> static_cast<GByte>(s_Shift2[i & 3])) &
                    0x03;

            break;
        }
        case 4:
        {
            constexpr int s_Shift4[] = {4, 0};
            // unpack 4-bit in-place in reverse
            // DANGER: Non-standard decrement of counter in the test section of
            // for.
            for (int i = n; --i >= 0;)
                pData[i] =
                    (pData[i >> 1] >> static_cast<GByte>(s_Shift4[i & 1])) &
                    0x0f;

            break;
        }
        case 3:
        {
            // unpacks 8 pixels (3 bytes) at time
            int i = 0;
            int k = 0;
            for (; i + 7 < n; i += 8, k += 3)
            {
                pUnpackData[i + 0] = ((pDataSrc[k + 0] >> 5));
                pUnpackData[i + 1] = ((pDataSrc[k + 0] >> 2) & 0x07);
                pUnpackData[i + 2] =
                    ((pDataSrc[k + 0] << 1) & 0x07) | (pDataSrc[k + 1] >> 7);
                pUnpackData[i + 3] = ((pDataSrc[k + 1] >> 4) & 0x07);
                pUnpackData[i + 4] = ((pDataSrc[k + 1] >> 1) & 0x07);
                pUnpackData[i + 5] =
                    ((pDataSrc[k + 1] << 2) & 0x07) | (pDataSrc[k + 2] >> 6);
                pUnpackData[i + 6] = ((pDataSrc[k + 2] >> 3) & 0x07);
                pUnpackData[i + 7] = ((pDataSrc[k + 2]) & 0x7);
            }
            if (i < n)
            {
                pUnpackData[i + 0] = ((pDataSrc[k + 0] >> 5));
                if (i + 1 < n)
                    pUnpackData[i + 1] = ((pDataSrc[k + 0] >> 2) & 0x07);
                if (i + 2 < n)
                    pUnpackData[i + 2] = ((pDataSrc[k + 0] << 1) & 0x07) |
                                         (pDataSrc[k + 1] >> 7);
                if (i + 3 < n)
                    pUnpackData[i + 3] = ((pDataSrc[k + 1] >> 4) & 0x07);
                if (i + 4 < n)
                    pUnpackData[i + 4] = ((pDataSrc[k + 1] >> 1) & 0x07);
                if (i + 5 < n)
                    pUnpackData[i + 5] = ((pDataSrc[k + 1] << 2) & 0x07) |
                                         (pDataSrc[k + 2] >> 6);
                if (i + 6 < n)
                    pUnpackData[i + 6] = ((pDataSrc[k + 2] >> 3) & 0x07);
            }

            memcpy(pData, pUnpackData, n);
            break;
        }
        case 5:
        {
            // unpacks 8 pixels (5 bytes) at time
            int i = 0;
            int k = 0;
            for (; i + 7 < n; i += 8, k += 5)
            {
                pUnpackData[i + 0] = ((pDataSrc[k + 0] >> 3));
                pUnpackData[i + 1] =
                    ((pDataSrc[k + 0] << 2) & 0x1f) | (pDataSrc[k + 1] >> 6);
                pUnpackData[i + 2] = ((pDataSrc[k + 1] >> 1) & 0x1f);
                pUnpackData[i + 3] =
                    ((pDataSrc[k + 1] << 4) & 0x1f) | (pDataSrc[k + 2] >> 4);
                pUnpackData[i + 4] =
                    ((pDataSrc[k + 2] << 1) & 0x1f) | (pDataSrc[k + 3] >> 7);
                pUnpackData[i + 5] = ((pDataSrc[k + 3] >> 2) & 0x1f);
                pUnpackData[i + 6] =
                    ((pDataSrc[k + 3] << 3) & 0x1f) | (pDataSrc[k + 4] >> 5);
                pUnpackData[i + 7] = ((pDataSrc[k + 4]) & 0x1f);
            }
            if (i < n)
            {
                pUnpackData[i + 0] = ((pDataSrc[k + 0] >> 3));
                if (i + 1 < n)
                    pUnpackData[i + 1] = ((pDataSrc[k + 0] << 2) & 0x1f) |
                                         (pDataSrc[k + 1] >> 6);
                if (i + 2 < n)
                    pUnpackData[i + 2] = ((pDataSrc[k + 1] >> 1) & 0x1f);
                if (i + 3 < n)
                    pUnpackData[i + 3] = ((pDataSrc[k + 1] << 4) & 0x1f) |
                                         (pDataSrc[k + 2] >> 4);
                if (i + 4 < n)
                    pUnpackData[i + 4] = ((pDataSrc[k + 2] << 1) & 0x1f) |
                                         (pDataSrc[k + 3] >> 7);
                if (i + 5 < n)
                    pUnpackData[i + 5] = ((pDataSrc[k + 3] >> 2) & 0x1f);
                if (i + 6 < n)
                    pUnpackData[i + 6] = ((pDataSrc[k + 3] << 3) & 0x1f) |
                                         (pDataSrc[k + 4] >> 5);
            }

            memcpy(pData, pUnpackData, n);
            break;
        }
        case 6:
        {
            // unpacks 4 pixels (3 bytes) at time
            int i = 0;
            int k = 0;
            for (; i + 3 < n; i += 4, k += 3)
            {
                pUnpackData[i + 0] = ((pDataSrc[k + 0] >> 2));
                pUnpackData[i + 1] =
                    ((pDataSrc[k + 0] << 4) & 0x3f) | (pDataSrc[k + 1] >> 4);
                pUnpackData[i + 2] =
                    ((pDataSrc[k + 1] << 2) & 0x3f) | (pDataSrc[k + 2] >> 6);
                pUnpackData[i + 3] = ((pDataSrc[k + 2]) & 0x3f);
            }
            if (i < n)
            {
                pUnpackData[i + 0] = ((pDataSrc[k + 0] >> 2));
                if (i + 1 < n)
                    pUnpackData[i + 1] = ((pDataSrc[k + 0] << 4) & 0x3f) |
                                         (pDataSrc[k + 1] >> 4);
                if (i + 2 < n)
                    pUnpackData[i + 2] = ((pDataSrc[k + 1] << 2) & 0x3f) |
                                         (pDataSrc[k + 2] >> 6);
            }

            memcpy(pData, pUnpackData, n);
            break;
        }
        case 7:
        {
            // unpacks 8 pixels (7 bytes) at time
            int i = 0;
            int k = 0;
            for (; i + 7 < n; i += 8, k += 7)
            {
                pUnpackData[i + 0] = ((pDataSrc[k + 0] >> 1));
                pUnpackData[i + 1] =
                    ((pDataSrc[k + 0] << 6) & 0x7f) | (pDataSrc[k + 1] >> 2);
                pUnpackData[i + 2] =
                    ((pDataSrc[k + 1] << 5) & 0x7f) | (pDataSrc[k + 2] >> 3);
                pUnpackData[i + 3] =
                    ((pDataSrc[k + 2] << 4) & 0x7f) | (pDataSrc[k + 3] >> 4);
                pUnpackData[i + 4] =
                    ((pDataSrc[k + 3] << 3) & 0x7f) | (pDataSrc[k + 4] >> 5);
                pUnpackData[i + 5] =
                    ((pDataSrc[k + 4] << 2) & 0x7f) | (pDataSrc[k + 5] >> 6);
                pUnpackData[i + 6] =
                    ((pDataSrc[k + 5] << 1) & 0x7f) | (pDataSrc[k + 6] >> 7);
                pUnpackData[i + 7] = ((pDataSrc[k + 6]) & 0x7f);
            }
            if (i < n)
            {
                pUnpackData[i + 0] = ((pDataSrc[k + 0] >> 1));
                if (i + 1 < n)
                    pUnpackData[i + 1] = ((pDataSrc[k + 0] << 6) & 0x7f) |
                                         (pDataSrc[k + 1] >> 2);
                if (i + 2 < n)
                    pUnpackData[i + 2] = ((pDataSrc[k + 1] << 5) & 0x7f) |
                                         (pDataSrc[k + 2] >> 3);
                if (i + 3 < n)
                    pUnpackData[i + 3] = ((pDataSrc[k + 2] << 4) & 0x7f) |
                                         (pDataSrc[k + 3] >> 4);
                if (i + 4 < n)
                    pUnpackData[i + 4] = ((pDataSrc[k + 3] << 3) & 0x7f) |
                                         (pDataSrc[k + 4] >> 5);
                if (i + 5 < n)
                    pUnpackData[i + 5] = ((pDataSrc[k + 4] << 2) & 0x7f) |
                                         (pDataSrc[k + 5] >> 6);
                if (i + 6 < n)
                    pUnpackData[i + 6] = ((pDataSrc[k + 5] << 1) & 0x7f) |
                                         (pDataSrc[k + 6] >> 7);
            }

            memcpy(pData, pUnpackData, n);
            break;
        }
        case 12:
        {
            GByte *pabyImage = reinterpret_cast<GByte *>(pData);
            GUInt16 *panImage = reinterpret_cast<GUInt16 *>(pData);
            // DANGER: Non-standard decrement of counter in the test section of
            // for.
            for (int i = n; --i >= 0;)
            {
                const long iOffset = i * 3 / 2;
                if (i % 2 == 0)
                    panImage[i] = pabyImage[iOffset] +
                                  (pabyImage[iOffset + 1] & 0xf0) * 16;
                else
                    panImage[i] = (pabyImage[iOffset] & 0x0f) * 16 +
                                  (pabyImage[iOffset + 1] & 0xf0) / 16 +
                                  (pabyImage[iOffset + 1] & 0x0f) * 256;
            }

            break;
        }
    }
}

/************************************************************************/
/* ==================================================================== */
/*                       NITFWrapperRasterBand                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                      NITFWrapperRasterBand()                         */
/************************************************************************/

NITFWrapperRasterBand::NITFWrapperRasterBand(NITFDataset *poDSIn,
                                             GDALRasterBand *poBaseBandIn,
                                             int nBandIn)
    : poBaseBand(poBaseBandIn), eInterp(poBaseBandIn->GetColorInterpretation()),
      bIsJPEG(poBaseBandIn->GetDataset() != nullptr &&
              poBaseBandIn->GetDataset()->GetDriver() != nullptr &&
              EQUAL(poBaseBandIn->GetDataset()->GetDriver()->GetDescription(),
                    "JPEG"))
{
    poDS = poDSIn;
    nBand = nBandIn;
    poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    eDataType = poBaseBandIn->GetRasterDataType();
}

/************************************************************************/
/*                      ~NITFWrapperRasterBand()                        */
/************************************************************************/

NITFWrapperRasterBand::~NITFWrapperRasterBand()
{
    if (poColorTable != nullptr)
        delete poColorTable;
}

/************************************************************************/
/*                     RefUnderlyingRasterBand()                        */
/************************************************************************/

/* We don't need ref-counting. Just return the base band */
GDALRasterBand *NITFWrapperRasterBand::RefUnderlyingRasterBand()
{
    return poBaseBand;
}

/************************************************************************/
/*                            GetColorTable()                           */
/************************************************************************/

GDALColorTable *NITFWrapperRasterBand::GetColorTable()
{
    return poColorTable;
}

/************************************************************************/
/*                 SetColorTableFromNITFBandInfo()                      */
/************************************************************************/

void NITFWrapperRasterBand::SetColorTableFromNITFBandInfo()
{
    NITFDataset *poGDS = cpl::down_cast<NITFDataset *>(poDS);
    poColorTable = NITFMakeColorTable(poGDS->psImage,
                                      poGDS->psImage->pasBandInfo + nBand - 1);
}

/************************************************************************/
/*                        GetColorInterpretation()                      */
/************************************************************************/

GDALColorInterp NITFWrapperRasterBand::GetColorInterpretation()
{
    return eInterp;
}

/************************************************************************/
/*                        SetColorInterpretation()                      */
/************************************************************************/

CPLErr NITFWrapperRasterBand::SetColorInterpretation(GDALColorInterp eInterpIn)
{
    this->eInterp = eInterpIn;
    if (poBaseBand->GetDataset() != nullptr &&
        poBaseBand->GetDataset()->GetDriver() != nullptr &&
        EQUAL(poBaseBand->GetDataset()->GetDriver()->GetDescription(),
              "JP2ECW"))
        poBaseBand->SetColorInterpretation(eInterp);
    return CE_None;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int NITFWrapperRasterBand::GetOverviewCount()
{
    if (bIsJPEG)
    {
        if ((cpl::down_cast<NITFDataset *>(poDS))
                ->ExposeUnderlyingJPEGDatasetOverviews())
            return NITFProxyPamRasterBand::GetOverviewCount();

        return GDALPamRasterBand::GetOverviewCount();
    }

    return NITFProxyPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                             GetOverview()                            */
/************************************************************************/

GDALRasterBand *NITFWrapperRasterBand::GetOverview(int iOverview)
{
    if (bIsJPEG)
    {
        if ((cpl::down_cast<NITFDataset *>(poDS))
                ->ExposeUnderlyingJPEGDatasetOverviews())
            return NITFProxyPamRasterBand::GetOverview(iOverview);

        return GDALPamRasterBand::GetOverview(iOverview);
    }

    return NITFProxyPamRasterBand::GetOverview(iOverview);
}

/************************************************************************/
/*                      NITFComplexRasterBand()                         */
/************************************************************************/

NITFComplexRasterBand::NITFComplexRasterBand(NITFDataset *poDSIn,
                                             GDALRasterBand *poBandI,
                                             GDALRasterBand *poBandQ,
                                             int nIBand, int nQBand)
    : NITFRasterBand(poDSIn, nIBand)
{

    CPLAssert(poBandI->GetRasterDataType() == poBandQ->GetRasterDataType());
    underlyingDataType = poBandI->GetRasterDataType();

    //add the I and Q bands to an intermediate dataset
    poIntermediateDS = std::make_unique<NITFDataset>();
    poIntermediateDS->nRasterXSize = poDSIn->nRasterXSize;
    poIntermediateDS->nRasterYSize = poDSIn->nRasterYSize;
    poIntermediateDS->eAccess = poDSIn->eAccess;

    poIntermediateDS->SetBand(nIBand, poBandI);
    poIntermediateDS->SetBand(nQBand, poBandQ);

    anBandMap[0] = nIBand;
    anBandMap[1] = nQBand;

    //set the new datatype
    switch (underlyingDataType)
    {
        case GDT_Int16:
            eDataType = GDT_CInt16;
            break;
        case GDT_Int32:
            eDataType = GDT_CInt32;
            break;
        case GDT_Float32:
            eDataType = GDT_CFloat32;
            break;
        case GDT_Float64:
            eDataType = GDT_CFloat64;
            break;
        default:
            eDataType = GDT_Unknown;
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported complex datatype");
            break;
    }

    complexDataTypeSize = GDALGetDataTypeSizeBytes(eDataType);
    underlyingDataTypeSize = GDALGetDataTypeSizeBytes(underlyingDataType);
    CPLAssert(underlyingDataTypeSize * 2 == complexDataTypeSize);

    poBandI->GetBlockSize(&nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr NITFComplexRasterBand::IBlockIO(int nBlockXOff, int nBlockYOff,
                                       void *pImage, GDALRWFlag rwFlag)

{
    int nRequestYSize;
    int nRequestXSize;
    bool bMemset = false;

    /* -------------------------------------------------------------------- */
    /*      If the last strip is partial, we need to avoid                  */
    /*      over-requesting.  We also need to initialize the extra part     */
    /*      of the block to zero.                                           */
    /* -------------------------------------------------------------------- */
    if ((nBlockYOff + 1) * nBlockYSize > nRasterYSize)
    {
        nRequestYSize = nRasterYSize - nBlockYOff * nBlockYSize;
        if (rwFlag == GF_Read)
            bMemset = true;
    }
    else
    {
        nRequestYSize = nBlockYSize;
    }

    /*-------------------------------------------------------------------- */
    /*      If the input imagery is tiled, also need to avoid over-        */
    /*      requesting in the X-direction.                                 */
    /* ------------------------------------------------------------------- */
    if ((nBlockXOff + 1) * nBlockXSize > nRasterXSize)
    {
        nRequestXSize = nRasterXSize - nBlockXOff * nBlockXSize;
        if (rwFlag == GF_Read)
            bMemset = true;
    }
    else
    {
        nRequestXSize = nBlockXSize;
    }

    if (bMemset)
    {
        memset(pImage, 0,
               static_cast<size_t>(GDALGetDataTypeSizeBytes(eDataType)) *
                   nBlockXSize * nBlockYSize);
    }

    //read/write both bands with interleaved pixels
    return poIntermediateDS->RasterIO(
        rwFlag, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
        nRequestXSize, nRequestYSize, pImage, nRequestXSize, nRequestYSize,
        underlyingDataType, 2, &anBandMap[0], complexDataTypeSize,
        static_cast<GSpacing>(complexDataTypeSize) * nBlockXSize,
        underlyingDataTypeSize, nullptr);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr NITFComplexRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                         void *pImage)

{
    return IBlockIO(nBlockXOff, nBlockYOff, pImage, GF_Read);
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr NITFComplexRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                          void *pImage)

{
    return IBlockIO(nBlockXOff, nBlockYOff, pImage, GF_Write);
}
