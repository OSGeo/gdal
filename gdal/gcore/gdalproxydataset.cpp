/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  A dataset and raster band classes that act as proxy for underlying
 *           GDALDataset* and GDALRasterBand*
 * Author:   Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_proxy.h"

CPL_CVSID("$Id$");

/* ******************************************************************** */
/*                        GDALProxyDataset                              */
/* ******************************************************************** */

#define D_PROXY_METHOD_WITH_RET(retType, retErrValue, methodName, argList, argParams) \
retType GDALProxyDataset::methodName argList \
{ \
    retType ret; \
    GDALDataset* poUnderlyingDataset = RefUnderlyingDataset(); \
    if (poUnderlyingDataset) \
    { \
        ret = poUnderlyingDataset->methodName argParams; \
        UnrefUnderlyingDataset(poUnderlyingDataset); \
    } \
    else \
    { \
        ret = retErrValue; \
    } \
    return ret; \
}


D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, IRasterIO,
                        ( GDALRWFlag eRWFlag,
                        int nXOff, int nYOff, int nXSize, int nYSize,
                        void * pData, int nBufXSize, int nBufYSize,
                        GDALDataType eBufType, 
                        int nBandCount, int *panBandMap,
                        GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
                        GDALRasterIOExtraArg* psExtraArg),
                        ( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                        pData, nBufXSize, nBufYSize,
                        eBufType, nBandCount, panBandMap,
                        nPixelSpace, nLineSpace, nBandSpace, psExtraArg ))


D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, IBuildOverviews,
                        ( const char *pszResampling, 
                          int nOverviews, int *panOverviewList, 
                          int nListBands, int *panBandList,
                          GDALProgressFunc pfnProgress, 
                          void * pProgressData ),
                        ( pszResampling, nOverviews, panOverviewList, 
                          nListBands, panBandList, pfnProgress, pProgressData ))

void  GDALProxyDataset::FlushCache()
{
    GDALDataset* poUnderlyingDataset = RefUnderlyingDataset();
    if (poUnderlyingDataset)
    {
        poUnderlyingDataset->FlushCache();
        UnrefUnderlyingDataset(poUnderlyingDataset);
    }
}

D_PROXY_METHOD_WITH_RET(char**, NULL, GetMetadataDomainList, (), ())
D_PROXY_METHOD_WITH_RET(char**, NULL, GetMetadata, (const char * pszDomain), (pszDomain))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetMetadata,
                        (char ** papszMetadata, const char * pszDomain),
                        (papszMetadata, pszDomain))
D_PROXY_METHOD_WITH_RET(const char*, NULL, GetMetadataItem,
                        (const char * pszName, const char * pszDomain),
                        (pszName, pszDomain))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetMetadataItem,
                        (const char * pszName, const char * pszValue, const char * pszDomain),
                        (pszName, pszValue, pszDomain))

D_PROXY_METHOD_WITH_RET(const char *, NULL, GetProjectionRef, (), ())
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetProjection, (const char* pszProjection), (pszProjection))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, GetGeoTransform, (double* padfGeoTransform), (padfGeoTransform))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetGeoTransform, (double* padfGeoTransform), (padfGeoTransform))

D_PROXY_METHOD_WITH_RET(void *, NULL, GetInternalHandle, ( const char * arg1), (arg1))
D_PROXY_METHOD_WITH_RET(GDALDriver *, NULL, GetDriver, (), ())
D_PROXY_METHOD_WITH_RET(char **, NULL, GetFileList, (), ())
D_PROXY_METHOD_WITH_RET(int, 0, GetGCPCount, (), ())
D_PROXY_METHOD_WITH_RET(const char *, NULL, GetGCPProjection, (), ())
D_PROXY_METHOD_WITH_RET(const GDAL_GCP *, NULL, GetGCPs, (), ())
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetGCPs,
                        (int nGCPCount, const GDAL_GCP *pasGCPList,
                         const char *pszGCPProjection),
                        (nGCPCount, pasGCPList, pszGCPProjection))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, AdviseRead,
                        ( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                GDALDataType eDT, 
                                int nBandCount, int *panBandList,
                                char **papszOptions ),
                        (nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, eDT, nBandCount, panBandList, papszOptions))
D_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, CreateMaskBand, ( int nFlags ), (nFlags))

/************************************************************************/
/*                    UnrefUnderlyingDataset()                        */
/************************************************************************/

void GDALProxyDataset::UnrefUnderlyingDataset(CPL_UNUSED GDALDataset* poUnderlyingDataset)
{
}

/* ******************************************************************** */
/*                        GDALProxyRasterBand                           */
/* ******************************************************************** */


#define RB_PROXY_METHOD_WITH_RET(retType, retErrValue, methodName, argList, argParams) \
retType GDALProxyRasterBand::methodName argList \
{ \
    retType ret; \
    GDALRasterBand* poSrcBand = RefUnderlyingRasterBand(); \
    if (poSrcBand) \
    { \
        ret = poSrcBand->methodName argParams; \
        UnrefUnderlyingRasterBand(poSrcBand); \
    } \
    else \
    { \
        ret = retErrValue; \
    } \
    return ret; \
}


#define RB_PROXY_METHOD_WITH_RET_WITH_INIT_BLOCK(retType, retErrValue, methodName, argList, argParams) \
retType GDALProxyRasterBand::methodName argList \
{ \
    retType ret; \
    GDALRasterBand* poSrcBand = RefUnderlyingRasterBand(); \
    if (poSrcBand) \
    { \
        if( !poSrcBand->InitBlockInfo() ) \
            ret = CE_Failure; \
        else \
            ret = poSrcBand->methodName argParams; \
        UnrefUnderlyingRasterBand(poSrcBand); \
    } \
    else \
    { \
        ret = retErrValue; \
    } \
    return ret; \
}

RB_PROXY_METHOD_WITH_RET_WITH_INIT_BLOCK(CPLErr, CE_Failure, IReadBlock,
                                ( int nXBlockOff, int nYBlockOff, void* pImage), 
                                (nXBlockOff, nYBlockOff, pImage) )
RB_PROXY_METHOD_WITH_RET_WITH_INIT_BLOCK(CPLErr, CE_Failure, IWriteBlock,
                                ( int nXBlockOff, int nYBlockOff, void* pImage), 
                                (nXBlockOff, nYBlockOff, pImage) )
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, IRasterIO,
                        ( GDALRWFlag eRWFlag,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void * pData, int nBufXSize, int nBufYSize,
                                GDALDataType eBufType,
                                GSpacing nPixelSpace,
                                GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg ), 
                        (eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                pData, nBufXSize, nBufYSize, eBufType,
                                nPixelSpace, nLineSpace, psExtraArg ) )

RB_PROXY_METHOD_WITH_RET(char**, NULL, GetMetadataDomainList, (), ())
RB_PROXY_METHOD_WITH_RET(char**, NULL, GetMetadata, (const char * pszDomain), (pszDomain))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetMetadata,
                        (char ** papszMetadata, const char * pszDomain),
                        (papszMetadata, pszDomain))
RB_PROXY_METHOD_WITH_RET(const char*, NULL, GetMetadataItem,
                        (const char * pszName, const char * pszDomain),
                        (pszName, pszDomain))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetMetadataItem,
                        (const char * pszName, const char * pszValue, const char * pszDomain),
                        (pszName, pszValue, pszDomain))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, FlushCache, (), ())
RB_PROXY_METHOD_WITH_RET(char**, NULL, GetCategoryNames, (), ())
RB_PROXY_METHOD_WITH_RET(double, 0, GetNoDataValue, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(double, 0, GetMinimum, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(double, 0, GetMaximum, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(double, 0, GetOffset, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(double, 0, GetScale, (int *pbSuccess), (pbSuccess))
RB_PROXY_METHOD_WITH_RET(const char*, NULL, GetUnitType, (), ())
RB_PROXY_METHOD_WITH_RET(GDALColorInterp, GCI_Undefined, GetColorInterpretation, (), ())
RB_PROXY_METHOD_WITH_RET(GDALColorTable*, NULL, GetColorTable, (), ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, Fill,
                        (double dfRealValue, double dfImaginaryValue),
                        (dfRealValue, dfImaginaryValue))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetCategoryNames, ( char ** arg ), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetNoDataValue, ( double arg ), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetColorTable, ( GDALColorTable *arg ), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetColorInterpretation,
                                    ( GDALColorInterp arg ), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetOffset, ( double arg ), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetScale, ( double arg ), (arg))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetUnitType, ( const char * arg ), (arg))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, GetStatistics,
                        ( int bApproxOK, int bForce,
                        double *pdfMin, double *pdfMax, 
                        double *pdfMean, double *padfStdDev ),
                        (bApproxOK, bForce, pdfMin, pdfMax, pdfMean, padfStdDev))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, ComputeStatistics,
                        ( int bApproxOK, 
                        double *pdfMin, double *pdfMax, 
                        double *pdfMean, double *pdfStdDev,
                        GDALProgressFunc pfn, void *pProgressData ),
                        ( bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev, pfn, pProgressData))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetStatistics,
                        ( double dfMin, double dfMax, 
                        double dfMean, double dfStdDev ),
                        (dfMin, dfMax, dfMean, dfStdDev))
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, ComputeRasterMinMax,
                        ( int arg1, double* arg2 ), (arg1, arg2))

RB_PROXY_METHOD_WITH_RET(int, 0, HasArbitraryOverviews, (), ())
RB_PROXY_METHOD_WITH_RET(int, 0,  GetOverviewCount, (), ())
RB_PROXY_METHOD_WITH_RET(GDALRasterBand*, NULL,  GetOverview, (int arg1), (arg1))
RB_PROXY_METHOD_WITH_RET(GDALRasterBand*, NULL,  GetRasterSampleOverview,
                        (int arg1), (arg1))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, BuildOverviews,
                        (const char * arg1, int arg2, int *arg3,
                        GDALProgressFunc arg4, void * arg5),
                        (arg1, arg2, arg3, arg4, arg5))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, AdviseRead,
                        ( int nXOff, int nYOff, int nXSize, int nYSize,
                        int nBufXSize, int nBufYSize, 
                        GDALDataType eDT, char **papszOptions ),
                        (nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, eDT, papszOptions))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, GetHistogram,
                        ( double dfMin, double dfMax,
                        int nBuckets, int * panHistogram,
                        int bIncludeOutOfRange, int bApproxOK,
                        GDALProgressFunc pfn, void *pProgressData ),
                        (dfMin, dfMax, nBuckets, panHistogram, bIncludeOutOfRange,
                        bApproxOK, pfn, pProgressData))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, GetDefaultHistogram,
                        (double *pdfMin, double *pdfMax,
                        int *pnBuckets, int ** ppanHistogram,
                        int bForce,
                        GDALProgressFunc pfn, void *pProgressData ),
                        (pdfMin, pdfMax, pnBuckets, ppanHistogram, bForce,
                        pfn, pProgressData))

RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetDefaultHistogram,
                        ( double dfMin, double dfMax,
                        int nBuckets, int * panHistogram ),
                        (dfMin, dfMax, nBuckets, panHistogram))

RB_PROXY_METHOD_WITH_RET(GDALRasterAttributeTable *, NULL,
                        GetDefaultRAT, (), ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, SetDefaultRAT,
                        ( const GDALRasterAttributeTable * arg1), (arg1))

RB_PROXY_METHOD_WITH_RET(GDALRasterBand*, NULL, GetMaskBand, (), ())
RB_PROXY_METHOD_WITH_RET(int, 0, GetMaskFlags, (), ())
RB_PROXY_METHOD_WITH_RET(CPLErr, CE_Failure, CreateMaskBand, ( int nFlags ), (nFlags))

RB_PROXY_METHOD_WITH_RET(CPLVirtualMem*, NULL, GetVirtualMemAuto,
                         ( GDALRWFlag eRWFlag, int *pnPixelSpace, GIntBig *pnLineSpace, char **papszOptions ),
                         (eRWFlag, pnPixelSpace, pnLineSpace, papszOptions) )

/************************************************************************/
/*                 UnrefUnderlyingRasterBand()                        */
/************************************************************************/

void GDALProxyRasterBand::UnrefUnderlyingRasterBand(CPL_UNUSED GDALRasterBand* poUnderlyingRasterBand)
{
}
