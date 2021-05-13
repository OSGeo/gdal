 /******************************************************************************
 *
 * Project:  Memory Array Translator
 * Purpose:  Complete implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "memdataset.h"
#include "memmultidim.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"

CPL_CVSID("$Id$")

struct MEMDataset::Private
{
    std::shared_ptr<GDALGroup> m_poRootGroup{};
};

/************************************************************************/
/*                        MEMCreateRasterBand()                         */
/************************************************************************/

GDALRasterBandH MEMCreateRasterBand( GDALDataset *poDS, int nBand,
                                     GByte *pabyData, GDALDataType eType,
                                     int nPixelOffset, int nLineOffset,
                                     int bAssumeOwnership )

{
    return reinterpret_cast<GDALRasterBandH>(
        new MEMRasterBand( poDS, nBand, pabyData, eType, nPixelOffset,
                           nLineOffset, bAssumeOwnership ) );
}

/************************************************************************/
/*                       MEMCreateRasterBandEx()                        */
/************************************************************************/

GDALRasterBandH MEMCreateRasterBandEx( GDALDataset *poDS, int nBand,
                                       GByte *pabyData, GDALDataType eType,
                                       GSpacing nPixelOffset,
                                       GSpacing nLineOffset,
                                       int bAssumeOwnership )

{
    return reinterpret_cast<GDALRasterBandH>(
        new MEMRasterBand( poDS, nBand, pabyData, eType, nPixelOffset,
                           nLineOffset, bAssumeOwnership ) );
}

/************************************************************************/
/*                           MEMRasterBand()                            */
/************************************************************************/

MEMRasterBand::MEMRasterBand( GByte *pabyDataIn, GDALDataType eTypeIn,
                              int nXSizeIn, int nYSizeIn ) :
    GDALPamRasterBand(FALSE),
    pabyData(pabyDataIn),
    nPixelOffset(GDALGetDataTypeSizeBytes(eTypeIn)),
    nLineOffset(0),
    bOwnData(true),
    bNoDataSet(FALSE),
    dfNoData(0.0),
    eColorInterp(GCI_Undefined),
    dfOffset(0.0),
    dfScale(1.0),
    psSavedHistograms(nullptr)
{
    eAccess = GA_Update;
    eDataType = eTypeIn;
    nRasterXSize = nXSizeIn;
    nRasterYSize = nYSizeIn;
    nBlockXSize = nXSizeIn;
    nBlockYSize = 1;
    nLineOffset = nPixelOffset * static_cast<size_t>(nBlockXSize);
}

/************************************************************************/
/*                           MEMRasterBand()                            */
/************************************************************************/

MEMRasterBand::MEMRasterBand( GDALDataset *poDSIn, int nBandIn,
                              GByte *pabyDataIn, GDALDataType eTypeIn,
                              GSpacing nPixelOffsetIn, GSpacing nLineOffsetIn,
                              int bAssumeOwnership, const char * pszPixelType ) :
    GDALPamRasterBand(FALSE),
    pabyData(pabyDataIn),
    nPixelOffset(nPixelOffsetIn),
    nLineOffset(nLineOffsetIn),
    bOwnData(bAssumeOwnership),
    bNoDataSet(FALSE),
    dfNoData(0.0),
    eColorInterp(GCI_Undefined),
    dfOffset(0.0),
    dfScale(1.0),
    psSavedHistograms(nullptr)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDS->GetAccess();

    eDataType = eTypeIn;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    if( nPixelOffsetIn == 0 )
        nPixelOffset = GDALGetDataTypeSizeBytes(eTypeIn);

    if( nLineOffsetIn == 0 )
        nLineOffset = nPixelOffset * static_cast<size_t>(nBlockXSize);

    if( pszPixelType && EQUAL(pszPixelType,"SIGNEDBYTE") )
        SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );
}

/************************************************************************/
/*                           ~MEMRasterBand()                           */
/************************************************************************/

MEMRasterBand::~MEMRasterBand()

{
    if( bOwnData )
    {
        VSIFree( pabyData );
    }

    if (psSavedHistograms != nullptr)
        CPLDestroyXMLNode(psSavedHistograms);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MEMRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                  int nBlockYOff,
                                  void * pImage )
{
    CPLAssert( nBlockXOff == 0 );

    const int nWordSize = GDALGetDataTypeSize( eDataType ) / 8;

    if( nPixelOffset == nWordSize )
    {
        memcpy( pImage,
                pabyData + nLineOffset*(size_t)nBlockYOff,
                static_cast<size_t>(nPixelOffset) * nBlockXSize );
    }
    else
    {
        GByte * const pabyCur =
            pabyData + nLineOffset * static_cast<size_t>(nBlockYOff);

        for( int iPixel = 0; iPixel < nBlockXSize; iPixel++ )
        {
            memcpy( reinterpret_cast<GByte *>(pImage) + iPixel*nWordSize,
                    pabyCur + iPixel*nPixelOffset,
                    nWordSize );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr MEMRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff,
                                   int nBlockYOff,
                                   void * pImage )
{
    CPLAssert( nBlockXOff == 0 );
    const int nWordSize = GDALGetDataTypeSize( eDataType ) / 8;

    if( nPixelOffset == nWordSize )
    {
        memcpy( pabyData+nLineOffset*(size_t)nBlockYOff,
                pImage,
                static_cast<size_t>(nPixelOffset) * nBlockXSize );
    }
    else
    {
        GByte *pabyCur =
            pabyData + nLineOffset * static_cast<size_t>(nBlockYOff);

        for( int iPixel = 0; iPixel < nBlockXSize; iPixel++ )
        {
            memcpy( pabyCur + iPixel*nPixelOffset,
                    reinterpret_cast<GByte *>( pImage ) + iPixel*nWordSize,
                    nWordSize );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr MEMRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpaceBuf,
                                 GSpacing nLineSpaceBuf,
                                 GDALRasterIOExtraArg* psExtraArg )
{
    if( nXSize != nBufXSize || nYSize != nBufYSize )
    {
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize,
                                         eBufType,
                                         static_cast<int>(nPixelSpaceBuf),
                                         nLineSpaceBuf,
                                         psExtraArg);
    }

    // In case block based I/O has been done before.
    FlushCache();

    if( eRWFlag == GF_Read )
    {
        for( int iLine=0; iLine < nYSize; iLine++ )
        {
            GDALCopyWords(
                pabyData + nLineOffset*static_cast<GPtrDiff_t>(iLine + nYOff) +
                nXOff*nPixelOffset,
                eDataType,
                static_cast<int>(nPixelOffset),
                reinterpret_cast<GByte*>( pData ) +
                nLineSpaceBuf * static_cast<GPtrDiff_t>(iLine),
                eBufType,
                static_cast<int>(nPixelSpaceBuf),
                nXSize );
        }
    }
    else
    {
        for( int iLine = 0; iLine < nYSize; iLine++ )
        {
            GDALCopyWords(
                reinterpret_cast<GByte *>( pData ) +
                nLineSpaceBuf*static_cast<GPtrDiff_t>(iLine),
                eBufType,
                static_cast<int>(nPixelSpaceBuf),
                pabyData + nLineOffset*static_cast<GPtrDiff_t>(iLine + nYOff) +
                nXOff*nPixelOffset,
                eDataType,
                static_cast<int>(nPixelOffset),
                nXSize );
        }
    }
    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr MEMDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpaceBuf,
                              GSpacing nLineSpaceBuf,
                              GSpacing nBandSpaceBuf,
                              GDALRasterIOExtraArg* psExtraArg)
{
    const int eBufTypeSize = GDALGetDataTypeSize(eBufType) / 8;

    // Detect if we have a pixel-interleaved buffer and a pixel-interleaved
    // dataset.
    if( nXSize == nBufXSize && nYSize == nBufYSize &&
        nBandCount == nBands && nBands > 1 &&
        nBandSpaceBuf == eBufTypeSize &&
        nPixelSpaceBuf == nBandSpaceBuf * nBands )
    {
        GDALDataType eDT = GDT_Unknown;
        GByte* pabyData = nullptr;
        GSpacing nPixelOffset = 0;
        GSpacing nLineOffset = 0;
        int eDTSize = 0;
        int iBandIndex;
        for( iBandIndex = 0; iBandIndex < nBandCount; iBandIndex++ )
        {
            if( panBandMap[iBandIndex] != iBandIndex + 1 )
                break;

            MEMRasterBand *poBand = reinterpret_cast<MEMRasterBand *>(
                GetRasterBand(iBandIndex + 1) );
            if( iBandIndex == 0 )
            {
                eDT = poBand->GetRasterDataType();
                pabyData = poBand->pabyData;
                nPixelOffset = poBand->nPixelOffset;
                nLineOffset = poBand->nLineOffset;
                eDTSize = GDALGetDataTypeSize(eDT) / 8;
                if( nPixelOffset != static_cast<GSpacing>(nBands) * eDTSize )
                    break;
            }
            else if( poBand->GetRasterDataType() != eDT ||
                     nPixelOffset != poBand->nPixelOffset ||
                     nLineOffset != poBand->nLineOffset ||
                     poBand->pabyData != pabyData + iBandIndex * eDTSize )
            {
                break;
            }
        }
        if( iBandIndex == nBandCount )
        {
            FlushCache();
            if( eRWFlag == GF_Read )
            {
                for(int iLine=0;iLine<nYSize;iLine++)
                {
                    GDALCopyWords(
                        pabyData +
                        nLineOffset*static_cast<size_t>(iLine + nYOff) +
                        nXOff*nPixelOffset,
                        eDT,
                        eDTSize,
                        reinterpret_cast<GByte *>( pData ) +
                        nLineSpaceBuf * static_cast<size_t>(iLine),
                        eBufType,
                        eBufTypeSize,
                        nXSize * nBands );
                }
            }
            else
            {
                for(int iLine=0;iLine<nYSize;iLine++)
                {
                    GDALCopyWords(
                        reinterpret_cast<GByte *>( pData ) +
                        nLineSpaceBuf*(size_t)iLine,
                        eBufType,
                        eBufTypeSize,
                        pabyData +
                        nLineOffset * static_cast<size_t>(iLine + nYOff) +
                        nXOff*nPixelOffset,
                        eDT,
                        eDTSize,
                        nXSize * nBands);
                }
            }
            return CE_None;
        }
    }

    if( nBufXSize != nXSize || nBufYSize != nYSize )
        return GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                   pData, nBufXSize, nBufYSize,
                                   eBufType, nBandCount, panBandMap,
                                   nPixelSpaceBuf, nLineSpaceBuf, nBandSpaceBuf,
                                   psExtraArg );

    return GDALDataset::BandBasedRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                   pData, nBufXSize, nBufYSize,
                                   eBufType, nBandCount, panBandMap,
                                   nPixelSpaceBuf, nLineSpaceBuf, nBandSpaceBuf,
                                   psExtraArg );
}

/************************************************************************/
/*                            GetNoDataValue()                          */
/************************************************************************/
double MEMRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    if( bNoDataSet )
        return dfNoData;

    return 0.0;
}

/************************************************************************/
/*                            SetNoDataValue()                          */
/************************************************************************/
CPLErr MEMRasterBand::SetNoDataValue( double dfNewValue )
{
    dfNoData = dfNewValue;
    bNoDataSet = TRUE;

    return CE_None;
}

/************************************************************************/
/*                         DeleteNoDataValue()                          */
/************************************************************************/

CPLErr MEMRasterBand::DeleteNoDataValue()
{
    dfNoData = 0.0;
    bNoDataSet = FALSE;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp MEMRasterBand::GetColorInterpretation()

{
    if( m_poColorTable != nullptr )
        return GCI_PaletteIndex;

    return eColorInterp;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr MEMRasterBand::SetColorInterpretation( GDALColorInterp eGCI )

{
    eColorInterp = eGCI;

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *MEMRasterBand::GetColorTable()

{
    return m_poColorTable.get();
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr MEMRasterBand::SetColorTable( GDALColorTable *poCT )

{
    if( poCT == nullptr )
        m_poColorTable.reset();
    else
        m_poColorTable.reset(poCT->Clone());

    return CE_None;
}
/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable* MEMRasterBand::GetDefaultRAT()
{
    return m_poRAT.get();
}

/************************************************************************/
/*                            SetDefaultRAT()                           */
/************************************************************************/

CPLErr MEMRasterBand::SetDefaultRAT( const GDALRasterAttributeTable * poRAT )
{
    if( poRAT == nullptr )
        m_poRAT.reset();
    else
        m_poRAT.reset(poRAT->Clone());

    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *MEMRasterBand::GetUnitType()

{
    return m_osUnitType.c_str();
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr MEMRasterBand::SetUnitType( const char *pszNewValue )

{
    m_osUnitType = pszNewValue ? pszNewValue : "";

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double MEMRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;

    return dfOffset;
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

CPLErr MEMRasterBand::SetOffset( double dfNewOffset )

{
    dfOffset = dfNewOffset;
    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double MEMRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess != nullptr )
        *pbSuccess = TRUE;

    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr MEMRasterBand::SetScale( double dfNewScale )

{
    dfScale = dfNewScale;
    return CE_None;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **MEMRasterBand::GetCategoryNames()

{
    return m_aosCategoryNames.List();
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr MEMRasterBand::SetCategoryNames( char ** papszNewNames )

{
    m_aosCategoryNames = CSLDuplicate(papszNewNames);

    return CE_None;
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

CPLErr MEMRasterBand::SetDefaultHistogram( double dfMin, double dfMax,
                                           int nBuckets, GUIntBig *panHistogram)

{
/* -------------------------------------------------------------------- */
/*      Do we have a matching histogram we should replace?              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNode = PamFindMatchingHistogram( psSavedHistograms,
                                                   dfMin, dfMax, nBuckets,
                                                   TRUE, TRUE );
    if( psNode != nullptr )
    {
        /* blow this one away */
        CPLRemoveXMLChild( psSavedHistograms, psNode );
        CPLDestroyXMLNode( psNode );
    }

/* -------------------------------------------------------------------- */
/*      Translate into a histogram XML tree.                            */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psHistItem = PamHistogramToXMLTree( dfMin, dfMax, nBuckets,
                                                    panHistogram, TRUE, FALSE );
    if( psHistItem == nullptr )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Insert our new default histogram at the front of the            */
/*      histogram list so that it will be the default histogram.        */
/* -------------------------------------------------------------------- */

    if( psSavedHistograms == nullptr )
        psSavedHistograms = CPLCreateXMLNode( nullptr, CXT_Element,
                                              "Histograms" );

    psHistItem->psNext = psSavedHistograms->psChild;
    psSavedHistograms->psChild = psHistItem;

    return CE_None;
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr
MEMRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                    int *pnBuckets, GUIntBig **ppanHistogram,
                                    int bForce,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData )

{
    if( psSavedHistograms != nullptr )
    {
        for( CPLXMLNode *psXMLHist = psSavedHistograms->psChild;
             psXMLHist != nullptr;
             psXMLHist = psXMLHist->psNext )
        {
            if( psXMLHist->eType != CXT_Element
                || !EQUAL(psXMLHist->pszValue,"HistItem") )
                continue;

            int bApprox = FALSE;
            int bIncludeOutOfRange = FALSE;
            if( PamParseHistogram( psXMLHist, pdfMin, pdfMax, pnBuckets,
                                   ppanHistogram, &bIncludeOutOfRange,
                                   &bApprox ) )
                return CE_None;

            return CE_Failure;
        }
    }

    return GDALRasterBand::GetDefaultHistogram( pdfMin, pdfMax, pnBuckets,
                                                ppanHistogram, bForce,
                                                pfnProgress,pProgressData);
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int MEMRasterBand::GetOverviewCount()
{
    MEMDataset* poMemDS = dynamic_cast<MEMDataset*>(poDS);
    if( poMemDS == nullptr )
        return 0;
    return poMemDS->m_nOverviewDSCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand * MEMRasterBand::GetOverview( int i )

{
    MEMDataset* poMemDS = dynamic_cast<MEMDataset*>(poDS);
    if( poMemDS == nullptr )
        return nullptr;
    if( i < 0 || i >= poMemDS->m_nOverviewDSCount )
        return nullptr;
    return poMemDS->m_papoOverviewDS[i]->GetRasterBand(nBand);
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr MEMRasterBand::CreateMaskBand( int nFlagsIn )
{
    InvalidateMaskBand();

    MEMDataset* poMemDS = dynamic_cast<MEMDataset*>(poDS);
    if( (nFlagsIn & GMF_PER_DATASET) != 0 && nBand != 1 && poMemDS != nullptr )
    {
        MEMRasterBand* poFirstBand =
            reinterpret_cast<MEMRasterBand*>(poMemDS->GetRasterBand(1));
        if( poFirstBand != nullptr)
            return poFirstBand->CreateMaskBand( nFlagsIn );
    }

    GByte* pabyMaskData = static_cast<GByte*>(VSI_CALLOC_VERBOSE(nRasterXSize,
                                                                 nRasterYSize));
    if( pabyMaskData == nullptr )
        return CE_Failure;

    nMaskFlags = nFlagsIn;
    bOwnMask = true;
    poMask = new MEMRasterBand( pabyMaskData, GDT_Byte,
                                nRasterXSize, nRasterYSize );
    if( (nFlagsIn & GMF_PER_DATASET) != 0 && nBand == 1 && poMemDS != nullptr )
    {
        for( int i = 2; i <= poMemDS->GetRasterCount(); ++i )
        {
            MEMRasterBand* poOtherBand =
                reinterpret_cast<MEMRasterBand*>(poMemDS->GetRasterBand(i));
            poOtherBand->InvalidateMaskBand();
            poOtherBand->nMaskFlags = nFlagsIn;
            poOtherBand->bOwnMask = false;
            poOtherBand->poMask = poMask;
        }
    }
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*      MEMDataset                                                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            MEMDataset()                             */
/************************************************************************/

MEMDataset::MEMDataset() :
    GDALDataset(FALSE),
    bGeoTransformSet(FALSE),
    pszProjection(nullptr),
    m_nGCPCount(0),
    m_pasGCPs(nullptr),
    m_nOverviewDSCount(0),
    m_papoOverviewDS(nullptr),
    m_poPrivate(new Private())
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -1.0;
    DisableReadWriteMutex();
}

/************************************************************************/
/*                            ~MEMDataset()                            */
/************************************************************************/

MEMDataset::~MEMDataset()

{
    FlushCache();
    CPLFree( pszProjection );

    GDALDeinitGCPs( m_nGCPCount, m_pasGCPs );
    CPLFree( m_pasGCPs );

    for(int i=0;i<m_nOverviewDSCount;++i)
        delete m_papoOverviewDS[i];
    CPLFree(m_papoOverviewDS);
}

#if 0
/************************************************************************/
/*                          EnterReadWrite()                            */
/************************************************************************/

int MEMDataset::EnterReadWrite(CPL_UNUSED GDALRWFlag eRWFlag)
{
    return TRUE;
}

/************************************************************************/
/*                         LeaveReadWrite()                             */
/************************************************************************/

void MEMDataset::LeaveReadWrite()
{
}
#endif  // if 0

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *MEMDataset::_GetProjectionRef()

{
    if( pszProjection == nullptr )
        return "";

    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr MEMDataset::_SetProjection( const char *pszProjectionIn )

{
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszProjectionIn );

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MEMDataset::GetGeoTransform( double *padfGeoTransform )

{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );
    if( bGeoTransformSet )
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr MEMDataset::SetGeoTransform( double *padfGeoTransform )

{
    memcpy( adfGeoTransform, padfGeoTransform, sizeof(double) * 6 );
    bGeoTransformSet = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetInternalHandle()                         */
/************************************************************************/

void *MEMDataset::GetInternalHandle( const char * pszRequest )

{
    // check for MEMORYnnn string in pszRequest (nnnn can be up to 10
    // digits, or even omitted)
    if( STARTS_WITH_CI(pszRequest, "MEMORY"))
    {
        if(int BandNumber = static_cast<int>(CPLScanLong(&pszRequest[6], 10)))
        {
            MEMRasterBand *RequestedRasterBand =
                reinterpret_cast<MEMRasterBand *>( GetRasterBand(BandNumber) );

            // we're within a MEMDataset so the only thing a RasterBand
            // could be is a MEMRasterBand

            if( RequestedRasterBand != nullptr )
            {
                // return the internal band data pointer
                return RequestedRasterBand->GetData();
            }
        }
    }

    return nullptr;
}
/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int MEMDataset::GetGCPCount()

{
    return m_nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *MEMDataset::_GetGCPProjection()

{
    return osGCPProjection;
}

/************************************************************************/
/*                              GetGCPs()                               */
/************************************************************************/

const GDAL_GCP *MEMDataset::GetGCPs()

{
    return m_pasGCPs;
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr MEMDataset::_SetGCPs( int nNewCount, const GDAL_GCP *pasNewGCPList,
                            const char *pszGCPProjection )

{
    GDALDeinitGCPs( m_nGCPCount, m_pasGCPs );
    CPLFree( m_pasGCPs );

    if( pszGCPProjection == nullptr )
        osGCPProjection = "";
    else
        osGCPProjection = pszGCPProjection;

    m_nGCPCount = nNewCount;
    m_pasGCPs = GDALDuplicateGCPs( m_nGCPCount, pasNewGCPList );

    return CE_None;
}

/************************************************************************/
/*                              AddBand()                               */
/*                                                                      */
/*      Add a new band to the dataset, allowing creation options to     */
/*      specify the existing memory to use, otherwise create new        */
/*      memory.                                                         */
/************************************************************************/

CPLErr MEMDataset::AddBand( GDALDataType eType, char **papszOptions )

{
    const int nBandId = GetRasterCount() + 1;
    const GSpacing nPixelSize = GDALGetDataTypeSizeBytes(eType);

/* -------------------------------------------------------------------- */
/*      Do we need to allocate the memory ourselves?  This is the       */
/*      simple case.                                                    */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "DATAPOINTER" ) == nullptr )
    {
        const GSpacing nTmp = nPixelSize * GetRasterXSize();
        GByte* pData =
#if SIZEOF_VOIDP == 4
            ( nTmp > INT_MAX ) ? nullptr :
#endif
            reinterpret_cast<GByte *>(
                VSI_CALLOC_VERBOSE((size_t)nTmp, GetRasterYSize() ) );

        if( pData == nullptr )
        {
            return CE_Failure;
        }

        SetBand( nBandId,
                 new MEMRasterBand( this, nBandId, pData, eType, nPixelSize,
                                    nPixelSize * GetRasterXSize(), TRUE ) );

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Get layout of memory and other flags.                           */
/* -------------------------------------------------------------------- */
    const char *pszDataPointer = CSLFetchNameValue(papszOptions, "DATAPOINTER");
    GByte *pData = reinterpret_cast<GByte *>(
        CPLScanPointer( pszDataPointer,
                        static_cast<int>(strlen(pszDataPointer)) ) );

    const char *pszOption = CSLFetchNameValue(papszOptions, "PIXELOFFSET");
    GSpacing nPixelOffset;
    if( pszOption == nullptr )
        nPixelOffset = nPixelSize;
    else
        nPixelOffset = CPLAtoGIntBig(pszOption);

    pszOption = CSLFetchNameValue(papszOptions, "LINEOFFSET");
    GSpacing nLineOffset;
    if( pszOption == nullptr )
        nLineOffset = GetRasterXSize() * static_cast<size_t>( nPixelOffset );
    else
        nLineOffset = CPLAtoGIntBig(pszOption);

    SetBand( nBandId,
             new MEMRasterBand( this, nBandId, pData, eType,
                                nPixelOffset, nLineOffset, FALSE ) );

    return CE_None;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr MEMDataset::IBuildOverviews( const char *pszResampling,
                                     int nOverviews, int *panOverviewList,
                                     int nListBands, int *panBandList,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData )
{
    if( nBands == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Dataset has zero bands." );
        return CE_Failure;
    }

    if( nListBands != nBands )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in MEM only"
                  "supported when operating on all bands." );
        return CE_Failure;
    }

    if( nOverviews == 0 )
    {
        // Cleanup existing overviews
        for(int i=0;i<m_nOverviewDSCount;++i)
            delete m_papoOverviewDS[i];
        CPLFree(m_papoOverviewDS);
        m_nOverviewDSCount = 0;
        m_papoOverviewDS = nullptr;
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Force cascading. Help to get accurate results when masks are    */
/*      involved.                                                       */
/* -------------------------------------------------------------------- */
    if( nOverviews > 1 &&
        (STARTS_WITH_CI(pszResampling, "AVER") |
         STARTS_WITH_CI(pszResampling, "GAUSS") ||
         EQUAL(pszResampling, "CUBIC") ||
         EQUAL(pszResampling, "CUBICSPLINE") ||
         EQUAL(pszResampling, "LANCZOS") ||
         EQUAL(pszResampling, "BILINEAR")) )
    {
        double dfTotalPixels = 0;
        for( int i = 0; i < nOverviews; i++ )
        {
            dfTotalPixels +=
                static_cast<double>(nRasterXSize) * nRasterYSize /
                    (panOverviewList[i] * panOverviewList[i]);
        }

        double dfAccPixels = 0;
        for( int i = 0; i < nOverviews; i++ )
        {
            double dfPixels =
                static_cast<double>(nRasterXSize) * nRasterYSize /
                    (panOverviewList[i] * panOverviewList[i]);
            void* pScaledProgress = GDALCreateScaledProgress(
                    dfAccPixels / dfTotalPixels,
                    (dfAccPixels + dfPixels) / dfTotalPixels,
                    pfnProgress, pProgressData );
            CPLErr eErr = IBuildOverviews(
                                    pszResampling,
                                    1, &panOverviewList[i],
                                    nListBands, panBandList,
                                    GDALScaledProgress,
                                    pScaledProgress );
            GDALDestroyScaledProgress( pScaledProgress );
            dfAccPixels += dfPixels;
            if( eErr == CE_Failure )
                return eErr;
        }
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.                                                  */
/* -------------------------------------------------------------------- */
    GDALRasterBand *poBand = GetRasterBand( 1 );

    for( int i = 0; i < nOverviews; i++ )
    {
        bool bExisting = false;
        for( int j = 0; j < poBand->GetOverviewCount(); j++ )
        {
            GDALRasterBand * poOverview = poBand->GetOverview( j );
            if( poOverview == nullptr )
                continue;

            int nOvFactor =
                GDALComputeOvFactor(poOverview->GetXSize(),
                                    poBand->GetXSize(),
                                    poOverview->GetYSize(),
                                    poBand->GetYSize());

            if( nOvFactor == panOverviewList[i]
                || nOvFactor == GDALOvLevelAdjust2( panOverviewList[i],
                                                   poBand->GetXSize(),
                                                   poBand->GetYSize() ) )
            {
                bExisting = true;
                break;
            }
        }

        // Create new overview dataset if needed.
        if( !bExisting )
        {
            MEMDataset* poOvrDS = new MEMDataset();
            poOvrDS->eAccess = GA_Update;
            poOvrDS->nRasterXSize = (nRasterXSize + panOverviewList[i] - 1)
                                            / panOverviewList[i];
            poOvrDS->nRasterYSize = (nRasterYSize + panOverviewList[i] - 1)
                                            / panOverviewList[i];
            for( int iBand = 0; iBand < nBands; iBand ++ )
            {
                const GDALDataType eDT =
                            GetRasterBand(iBand+1)->GetRasterDataType();
                if( poOvrDS->AddBand( eDT, nullptr ) != CE_None )
                {
                    delete poOvrDS;
                    return CE_Failure;
                }
            }
            m_nOverviewDSCount ++;
            m_papoOverviewDS = (GDALDataset**) CPLRealloc(m_papoOverviewDS,
                                    sizeof(GDALDataset*) * m_nOverviewDSCount );
            m_papoOverviewDS[m_nOverviewDSCount-1] = poOvrDS;
        }
    }

/* -------------------------------------------------------------------- */
/*      Build band list.                                                */
/* -------------------------------------------------------------------- */
    GDALRasterBand **pahBands = static_cast<GDALRasterBand **>(
        CPLCalloc(sizeof(GDALRasterBand *), nBands) );
    for( int i = 0; i < nBands; i++ )
        pahBands[i] = GetRasterBand( panBandList[i] );

/* -------------------------------------------------------------------- */
/*      Refresh overviews that were listed.                             */
/* -------------------------------------------------------------------- */
    GDALRasterBand **papoOverviewBands = static_cast<GDALRasterBand **>(
        CPLCalloc(sizeof(void*), nOverviews) );
    GDALRasterBand **papoMaskOverviewBands = static_cast<GDALRasterBand **>(
        CPLCalloc(sizeof(void*), nOverviews) );

    CPLErr eErr = CE_None;
    for( int iBand = 0; iBand < nBands && eErr == CE_None; iBand++ )
    {
        poBand = GetRasterBand( panBandList[iBand] );

        int nNewOverviews = 0;
        for( int i = 0; i < nOverviews; i++ )
        {
            for( int j = 0; j < poBand->GetOverviewCount(); j++ )
            {
                GDALRasterBand * poOverview = poBand->GetOverview( j );

                int bHasNoData = FALSE;
                double noDataValue = poBand->GetNoDataValue(&bHasNoData);

                if( bHasNoData )
                  poOverview->SetNoDataValue(noDataValue);

                const int nOvFactor =
                    GDALComputeOvFactor(poOverview->GetXSize(),
                                        poBand->GetXSize(),
                                        poOverview->GetYSize(),
                                        poBand->GetYSize());

                if( nOvFactor == panOverviewList[i]
                    || nOvFactor == GDALOvLevelAdjust2(panOverviewList[i],
                                                       poBand->GetXSize(),
                                                       poBand->GetYSize() ) )
                {
                    papoOverviewBands[nNewOverviews++] = poOverview;
                    break;
                }
            }
        }

        // If the band has an explicit mask, we need to create overviews
        // for it
        MEMRasterBand* poMEMBand = reinterpret_cast<MEMRasterBand*>(poBand);
        const bool bMustGenerateMaskOvr =
                ( (poMEMBand->bOwnMask && poMEMBand->poMask != nullptr) ||
        // Or if it is a per-dataset mask, in which case just do it for the
        // first band
                  ((poMEMBand->nMaskFlags & GMF_PER_DATASET) != 0 && iBand == 0) );

        if( nNewOverviews > 0 && bMustGenerateMaskOvr )
        {
            for( int i = 0; i < nNewOverviews; i++ )
            {
                MEMRasterBand* poMEMOvrBand =
                    reinterpret_cast<MEMRasterBand*>(papoOverviewBands[i]);
                if( !(poMEMOvrBand->bOwnMask && poMEMOvrBand->poMask != nullptr) &&
                    (poMEMOvrBand->nMaskFlags & GMF_PER_DATASET) == 0 )
                {
                    poMEMOvrBand->CreateMaskBand( poMEMBand->nMaskFlags );
                }
                papoMaskOverviewBands[i] = poMEMOvrBand->GetMaskBand();
            }

            void* pScaledProgress = GDALCreateScaledProgress(
                    1.0 * iBand / nBands,
                    1.0 * (iBand+0.5) / nBands,
                    pfnProgress, pProgressData );

            MEMRasterBand* poMaskBand = reinterpret_cast<MEMRasterBand*>(
                                                        poBand->GetMaskBand());
            // Make the mask band to be its own mask, similarly to what is
            // done for alpha bands in GDALRegenerateOverviews() (#5640)
            poMaskBand->InvalidateMaskBand();
            poMaskBand->bOwnMask = false;
            poMaskBand->poMask = poMaskBand;
            poMaskBand->nMaskFlags = 0;
            eErr = GDALRegenerateOverviews(
                                        (GDALRasterBandH) poMaskBand,
                                        nNewOverviews,
                                        (GDALRasterBandH*)papoMaskOverviewBands,
                                        pszResampling,
                                        GDALScaledProgress, pScaledProgress );
            poMaskBand->InvalidateMaskBand();
            GDALDestroyScaledProgress( pScaledProgress );
        }

        // Generate overview of bands *AFTER* mask overviews
        if( nNewOverviews > 0 && eErr == CE_None  )
        {
            void* pScaledProgress = GDALCreateScaledProgress(
                    1.0 * (iBand+(bMustGenerateMaskOvr ? 0.5 : 1)) / nBands,
                    1.0 * (iBand+1)/ nBands,
                    pfnProgress, pProgressData );
            eErr = GDALRegenerateOverviews( (GDALRasterBandH) poBand,
                                            nNewOverviews,
                                            (GDALRasterBandH*)papoOverviewBands,
                                            pszResampling,
                                            GDALScaledProgress, pScaledProgress );
            GDALDestroyScaledProgress( pScaledProgress );
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( papoOverviewBands );
    CPLFree( papoMaskOverviewBands );
    CPLFree( pahBands );

    return eErr;
}

/************************************************************************/
/*                         CreateMaskBand()                             */
/************************************************************************/

CPLErr MEMDataset::CreateMaskBand( int nFlagsIn )
{
    GDALRasterBand* poFirstBand = GetRasterBand(1);
    if( poFirstBand == nullptr )
        return CE_Failure;
    return poFirstBand->CreateMaskBand( nFlagsIn | GMF_PER_DATASET );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MEMDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Do we have the special filename signature for MEM format        */
/*      description strings?                                            */
/* -------------------------------------------------------------------- */
    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "MEM:::")
        || poOpenInfo->fpL != nullptr )
        return nullptr;

    char **papszOptions
        = CSLTokenizeStringComplex(poOpenInfo->pszFilename+6, ",",
                                   TRUE, FALSE );

/* -------------------------------------------------------------------- */
/*      Verify we have all required fields                              */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "PIXELS" ) == nullptr
        || CSLFetchNameValue( papszOptions, "LINES" ) == nullptr
        || CSLFetchNameValue( papszOptions, "DATAPOINTER" ) == nullptr )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Missing required field (one of PIXELS, LINES or DATAPOINTER).  "
            "Unable to access in-memory array." );

        CSLDestroy( papszOptions );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create the new MEMDataset object.                               */
/* -------------------------------------------------------------------- */
    MEMDataset *poDS = new MEMDataset();

    poDS->nRasterXSize = atoi(CSLFetchNameValue(papszOptions,"PIXELS"));
    poDS->nRasterYSize = atoi(CSLFetchNameValue(papszOptions,"LINES"));
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Extract other information.                                      */
/* -------------------------------------------------------------------- */
    const char *pszOption = CSLFetchNameValue(papszOptions,"BANDS");
    int nBands = 1;
    if( pszOption != nullptr )
    {
        nBands = atoi(pszOption);
    }

    if( !GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBands, TRUE))
    {
        CSLDestroy( papszOptions );
        delete poDS;
        return nullptr;
    }

    pszOption = CSLFetchNameValue(papszOptions,"DATATYPE");
    GDALDataType eType = GDT_Byte;
    if( pszOption != nullptr )
    {
        if( atoi(pszOption) > 0 && atoi(pszOption) < GDT_TypeCount )
            eType = static_cast<GDALDataType>( atoi(pszOption) );
        else
        {
            eType = GDT_Unknown;
            for( int iType = 0; iType < GDT_TypeCount; iType++ )
            {
                if( EQUAL(GDALGetDataTypeName((GDALDataType) iType),
                          pszOption) )
                {
                    eType = static_cast<GDALDataType>( iType );
                    break;
                }
            }

            if( eType == GDT_Unknown )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "DATATYPE=%s not recognised.",
                          pszOption );
                CSLDestroy( papszOptions );
                delete poDS;
                return nullptr;
            }
        }
    }

    pszOption = CSLFetchNameValue(papszOptions, "PIXELOFFSET");
    GSpacing nPixelOffset;
    if( pszOption == nullptr )
        nPixelOffset = GDALGetDataTypeSizeBytes(eType);
    else
        nPixelOffset = CPLScanUIntBig(pszOption,
                                      static_cast<int>(strlen(pszOption)));

    pszOption = CSLFetchNameValue(papszOptions, "LINEOFFSET");
    GSpacing nLineOffset = 0;
    if( pszOption == nullptr )
        nLineOffset = poDS->nRasterXSize * static_cast<size_t>( nPixelOffset );
    else
        nLineOffset = CPLScanUIntBig(pszOption,
                                     static_cast<int>(strlen(pszOption)));

    pszOption = CSLFetchNameValue(papszOptions, "BANDOFFSET");
    GSpacing nBandOffset = 0;
    if( pszOption == nullptr )
        nBandOffset = nLineOffset * static_cast<size_t>( poDS->nRasterYSize );
    else
        nBandOffset = CPLScanUIntBig(pszOption,
                                     static_cast<int>(strlen(pszOption)));

    const char *pszDataPointer = CSLFetchNameValue(papszOptions,"DATAPOINTER");
    GByte *pabyData = reinterpret_cast<GByte *>(
        CPLScanPointer( pszDataPointer,
                        static_cast<int>(strlen(pszDataPointer)) ) );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        poDS->SetBand( iBand+1,
                       new MEMRasterBand( poDS, iBand+1,
                                          pabyData + iBand * nBandOffset,
                                          eType, nPixelOffset, nLineOffset,
                                          FALSE ) );
    }

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszOptions );
    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *MEMDataset::Create( const char * /* pszFilename */,
                                 int nXSize,
                                 int nYSize,
                                 int nBands,
                                 GDALDataType eType,
                                 char **papszOptions )
{

/* -------------------------------------------------------------------- */
/*      Do we want a pixel interleaved buffer?  I mostly care about     */
/*      this to test pixel interleaved IO in other contexts, but it     */
/*      could be useful to create a directly accessible buffer for      */
/*      some apps.                                                      */
/* -------------------------------------------------------------------- */
    bool bPixelInterleaved = false;
    const char *pszOption = CSLFetchNameValue( papszOptions, "INTERLEAVE" );
    if( pszOption && EQUAL(pszOption,"PIXEL") )
        bPixelInterleaved = true;

/* -------------------------------------------------------------------- */
/*      First allocate band data, verifying that we can get enough      */
/*      memory.                                                         */
/* -------------------------------------------------------------------- */
    const int nWordSize = GDALGetDataTypeSize(eType) / 8;
    if( nBands > 0 && nWordSize > 0 && (nBands > INT_MAX / nWordSize ||
        (GIntBig)nXSize * nYSize > GINTBIG_MAX / (nWordSize * nBands)) )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, "Multiplication overflow");
        return nullptr;
    }

    const GUIntBig nGlobalBigSize
        = static_cast<GUIntBig>(nWordSize) * nBands * nXSize * nYSize;
    const size_t nGlobalSize = static_cast<size_t>(nGlobalBigSize);
#if SIZEOF_VOIDP == 4
    if( static_cast<GUIntBig>(nGlobalSize) != nGlobalBigSize )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "Cannot allocate " CPL_FRMT_GUIB " bytes on this platform.",
                  nGlobalBigSize );
        return nullptr;
    }
#endif

    std::vector<GByte*> apbyBandData;
    bool bAllocOK = true;

    if( bPixelInterleaved )
    {
        apbyBandData.push_back(
            reinterpret_cast<GByte *>( VSI_CALLOC_VERBOSE( 1, nGlobalSize ) ) );

        if( apbyBandData[0] == nullptr )
            bAllocOK = FALSE;
        else
        {
            for( int iBand = 1; iBand < nBands; iBand++ )
                apbyBandData.push_back( apbyBandData[0] + iBand * nWordSize );
        }
    }
    else
    {
        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            apbyBandData.push_back(
                reinterpret_cast<GByte *>(
                    VSI_CALLOC_VERBOSE(
                        1,
                        static_cast<size_t>(nWordSize) * nXSize * nYSize ) ) );
            if( apbyBandData[iBand] == nullptr )
            {
                bAllocOK = FALSE;
                break;
            }
        }
    }

    if( !bAllocOK )
    {
        for( int iBand = 0;
             iBand < static_cast<int>( apbyBandData.size() );
             iBand++ )
        {
            if( apbyBandData[iBand] )
                VSIFree( apbyBandData[iBand] );
        }
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create the new GTiffDataset object.                             */
/* -------------------------------------------------------------------- */
    MEMDataset *poDS = new MEMDataset();

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;

    const char *pszPixelType = CSLFetchNameValue( papszOptions, "PIXELTYPE" );
    if( pszPixelType && EQUAL(pszPixelType, "SIGNEDBYTE") )
        poDS->SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );

    if( bPixelInterleaved )
        poDS->SetMetadataItem( "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE" );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        MEMRasterBand *poNewBand = nullptr;

        if( bPixelInterleaved )
            poNewBand = new MEMRasterBand( poDS, iBand+1, apbyBandData[iBand],
                                           eType, nWordSize * nBands, 0,
                                           iBand == 0 );
        else
            poNewBand = new MEMRasterBand( poDS, iBand+1, apbyBandData[iBand],
                                           eType, 0, 0, TRUE );

        poDS->SetBand( iBand+1, poNewBand );
    }

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    return poDS;
}

/************************************************************************/
/*                           GetMDArrayNames()                          */
/************************************************************************/

std::vector<std::string> MEMGroup::GetMDArrayNames(CSLConstList) const
{
    std::vector<std::string> names;
    for( const auto& iter: m_oMapMDArrays )
        names.push_back(iter.first);
    return names;
}

/************************************************************************/
/*                             OpenMDArray()                            */
/************************************************************************/

std::shared_ptr<GDALMDArray> MEMGroup::OpenMDArray(const std::string& osName,
                                                   CSLConstList) const
{
    auto oIter = m_oMapMDArrays.find(osName);
    if( oIter != m_oMapMDArrays.end() )
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                            GetGroupNames()                           */
/************************************************************************/

std::vector<std::string> MEMGroup::GetGroupNames(CSLConstList) const
{
    std::vector<std::string> names;
    for( const auto& iter: m_oMapGroups )
        names.push_back(iter.first);
    return names;
}

/************************************************************************/
/*                              OpenGroup()                             */
/************************************************************************/

std::shared_ptr<GDALGroup> MEMGroup::OpenGroup(const std::string& osName,
                                               CSLConstList) const
{
    auto oIter = m_oMapGroups.find(osName);
    if( oIter != m_oMapGroups.end() )
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                             CreateGroup()                            */
/************************************************************************/

std::shared_ptr<GDALGroup> MEMGroup::CreateGroup(const std::string& osName,
                                                 CSLConstList /*papszOptions*/)
{
    if( osName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty group name not supported");
        return nullptr;
    }
    if( m_oMapGroups.find(osName) != m_oMapGroups.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A group with same name already exists");
        return nullptr;
    }
    auto newGroup(std::make_shared<MEMGroup>(GetFullName(), osName.c_str()));
    m_oMapGroups[osName] = newGroup;
    return newGroup;
}

/************************************************************************/
/*                            CreateMDArray()                           */
/************************************************************************/

std::shared_ptr<GDALMDArray> MEMGroup::CreateMDArray(const std::string& osName,
                                                     const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                                     const GDALExtendedDataType& oType,
                                                     CSLConstList papszOptions)
{
    if( osName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty array name not supported");
        return nullptr;
    }
    if( m_oMapMDArrays.find(osName) != m_oMapMDArrays.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An array with same name already exists");
        return nullptr;
    }
    auto newArray(MEMMDArray::Create(GetFullName(), osName, aoDimensions, oType));

    // Used by NUMPYMultiDimensionalDataset
    const char *pszDataPointer = CSLFetchNameValue(papszOptions, "DATAPOINTER");
    GByte* pData = nullptr;
    std::vector<GPtrDiff_t> anStrides;
    if( pszDataPointer )
    {
        pData = reinterpret_cast<GByte *>(
            CPLScanPointer( pszDataPointer,
                            static_cast<int>(strlen(pszDataPointer)) ) );
        const char* pszStrides = CSLFetchNameValue(papszOptions, "STRIDES");
        if( pszStrides )
        {
            CPLStringList aosStrides(CSLTokenizeString2(pszStrides, ",", 0));
            if( static_cast<size_t>(aosStrides.size()) != aoDimensions.size() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid number of strides");
                return nullptr;
            }
            for( int i = 0; i < aosStrides.size(); i++ )
            {
                const auto nStride = CPLAtoGIntBig(aosStrides[i]);
                anStrides.push_back(static_cast<GPtrDiff_t>(nStride));
            }
        }
    }
    if( !newArray->Init(pData, anStrides) )
        return nullptr;
    m_oMapMDArrays[osName] = newArray;
    return newArray;
}

/************************************************************************/
/*                            GetAttribute()                            */
/************************************************************************/

std::shared_ptr<GDALAttribute> MEMGroup::GetAttribute(const std::string& osName) const
{
    auto oIter = m_oMapAttributes.find(osName);
    if( oIter != m_oMapAttributes.end() )
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                            GetAttributes()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> MEMGroup::GetAttributes(CSLConstList) const
{
    std::vector<std::shared_ptr<GDALAttribute>> oRes;
    for( const auto& oIter: m_oMapAttributes )
    {
        oRes.push_back(oIter.second);
    }
    return oRes;
}

/************************************************************************/
/*                            GetDimensions()                           */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>> MEMGroup::GetDimensions(CSLConstList) const
{
    std::vector<std::shared_ptr<GDALDimension>> oRes;
    for( const auto& oIter: m_oMapDimensions )
    {
        oRes.push_back(oIter.second);
    }
    return oRes;
}

/************************************************************************/
/*                           CreateAttribute()                          */
/************************************************************************/

std::shared_ptr<GDALAttribute> MEMGroup::CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList)
{
    if( osName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty attribute name not supported");
        return nullptr;
    }
    if( m_oMapAttributes.find(osName) != m_oMapAttributes.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An attribute with same name already exists");
        return nullptr;
    }
    auto newAttr(MEMAttribute::Create(
        (GetFullName() == "/" ? "/" : GetFullName() + "/") + "_GLOBAL_",
        osName, anDimensions, oDataType));
    if( !newAttr )
        return nullptr;
    m_oMapAttributes[osName] = newAttr;
    return newAttr;
}

/************************************************************************/
/*                          MEMAbstractMDArray()                        */
/************************************************************************/

MEMAbstractMDArray::MEMAbstractMDArray(const std::string& osParentName,
                                       const std::string& osName,
                       const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                       const GDALExtendedDataType& oType):
    GDALAbstractMDArray(osParentName, osName),
    m_aoDims(aoDimensions),
    m_oType(oType)
{
}

/************************************************************************/
/*                         ~MEMAbstractMDArray()                        */
/************************************************************************/

MEMAbstractMDArray::~MEMAbstractMDArray()
{
    if( m_bOwnArray )
    {
        if( m_oType.NeedsFreeDynamicMemory() )
        {
            GByte* pabyPtr = m_pabyArray;
            GByte* pabyEnd = m_pabyArray + m_nTotalSize;
            const auto nDTSize(m_oType.GetSize());
            while(pabyPtr < pabyEnd)
            {
                m_oType.FreeDynamicMemory(pabyPtr);
                pabyPtr += nDTSize;
            }
        }
        VSIFree(m_pabyArray);
    }
}

/************************************************************************/
/*                                  Init()                              */
/************************************************************************/

bool MEMAbstractMDArray::Init(GByte* pData,
                              const std::vector<GPtrDiff_t>& anStrides)
{
    GUInt64 nTotalSize = m_oType.GetSize();
    if( !m_aoDims.empty() )
    {
        if( anStrides.empty() )
        {
            m_anStrides.resize(m_aoDims.size());
        }
        else
        {
            CPLAssert( anStrides.size() == m_aoDims.size() );
            m_anStrides = anStrides;
        }

        // To compute strides we must proceed from the fastest varying dimension
        // (the last one), and then reverse the result
        for( size_t i = m_aoDims.size(); i != 0; )
        {
            --i;
            const auto& poDim = m_aoDims[i];
            auto nDimSize = poDim->GetSize();
            if( nDimSize != 0 && nTotalSize > std::numeric_limits<GUInt64>::max() / nDimSize )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Too big allocation");
                return false;
            }
            auto nNewSize = nTotalSize * nDimSize;
            if( anStrides.empty() )
                m_anStrides[i] = static_cast<size_t>(nTotalSize);
            nTotalSize = nNewSize;
        }
    }

    // We restrict the size of the allocation so that all elements can be
    // indexed by GPtrDiff_t
    if( nTotalSize > static_cast<size_t>(
            std::numeric_limits<GPtrDiff_t>::max()) )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Too big allocation");
        return false;
    }
    m_nTotalSize = static_cast<size_t>(nTotalSize);
    if( pData )
    {
        m_pabyArray = pData;
    }
    else
    {
        m_pabyArray = static_cast<GByte*>(VSI_CALLOC_VERBOSE(1, m_nTotalSize));
        m_bOwnArray = true;
    }
    return m_pabyArray != nullptr;
}

/************************************************************************/
/*                             FastCopy()                               */
/************************************************************************/

template<int N> inline static void FastCopy(size_t nIters,
                                            GByte* dstPtr,
                                            const GByte* srcPtr,
                                            GPtrDiff_t dst_inc_offset,
                                            GPtrDiff_t src_inc_offset)
{
    if( nIters >= 8 )
    {
#define COPY_ELT(i) memcpy(dstPtr + (i) * dst_inc_offset, srcPtr + (i) * src_inc_offset, N)
        while(true)
        {
            COPY_ELT(0);
            COPY_ELT(1);
            COPY_ELT(2);
            COPY_ELT(3);
            COPY_ELT(4);
            COPY_ELT(5);
            COPY_ELT(6);
            COPY_ELT(7);
            nIters -= 8;
            srcPtr += 8 * src_inc_offset;
            dstPtr += 8 * dst_inc_offset;
            if( nIters < 8 )
                break;
        }
        if( nIters == 0 )
            return;
    }
    while(true)
    {
        memcpy(dstPtr, srcPtr, N);
        if( (--nIters) == 0 )
            break;
        srcPtr += src_inc_offset;
        dstPtr += dst_inc_offset;
    }
}

/************************************************************************/
/*                             ReadWrite()                              */
/************************************************************************/

void MEMAbstractMDArray::ReadWrite(bool bIsWrite,
                                   const size_t* count,
                                   std::vector<StackReadWrite>& stack,
                                   const GDALExtendedDataType& srcType,
                                   const GDALExtendedDataType& dstType) const
{
    const auto nDims = m_aoDims.size();
    const auto nDimsMinus1 = nDims - 1;
    const bool bBothAreNumericDT =
        srcType.GetClass() == GEDTC_NUMERIC &&
        dstType.GetClass() == GEDTC_NUMERIC;
    const bool bSameNumericDT =
        bBothAreNumericDT &&
        srcType.GetNumericDataType() == dstType.GetNumericDataType();
    const auto nSameDTSize = bSameNumericDT ? srcType.GetSize() : 0;
    const bool bCanUseMemcpyLastDim =
        bSameNumericDT &&
            stack[nDimsMinus1].src_inc_offset == static_cast<GPtrDiff_t>(nSameDTSize) &&
            stack[nDimsMinus1].dst_inc_offset == static_cast<GPtrDiff_t>(nSameDTSize);
    const size_t nCopySizeLastDim =
        bCanUseMemcpyLastDim ? nSameDTSize * count[nDimsMinus1] : 0;
    const bool bNeedsFreeDynamicMemory = bIsWrite && dstType.NeedsFreeDynamicMemory();

    auto lambdaLastDim = [&](size_t idxPtr)
    {
        auto srcPtr = stack[idxPtr].src_ptr;
        auto dstPtr = stack[idxPtr].dst_ptr;
        if( nCopySizeLastDim )
        {
            memcpy(dstPtr, srcPtr, nCopySizeLastDim);
        }
        else
        {
            size_t nIters = count[nDimsMinus1];
            const auto dst_inc_offset = stack[nDimsMinus1].dst_inc_offset;
            const auto src_inc_offset = stack[nDimsMinus1].src_inc_offset;
            if( bSameNumericDT )
            {
                if( nSameDTSize == 1 )
                {
                    FastCopy<1>(nIters, dstPtr, srcPtr,
                                dst_inc_offset, src_inc_offset);
                    return;
                }
                if( nSameDTSize == 2 )
                {
                    FastCopy<2>(nIters, dstPtr, srcPtr,
                                dst_inc_offset, src_inc_offset);
                    return;
                }
                if( nSameDTSize == 4 )
                {
                    FastCopy<4>(nIters, dstPtr, srcPtr,
                                dst_inc_offset, src_inc_offset);
                    return;
                }
                if( nSameDTSize == 8 )
                {
                    FastCopy<8>(nIters, dstPtr, srcPtr,
                                dst_inc_offset, src_inc_offset);
                    return;
                }
                if( nSameDTSize == 16 )
                {
                    FastCopy<16>(nIters, dstPtr, srcPtr,
                                 dst_inc_offset, src_inc_offset);
                    return;
                }
                CPLAssert(false);
            }
            else if ( bBothAreNumericDT
#if SIZEOF_VOIDP == 8
                      && src_inc_offset <= std::numeric_limits<int>::max()
                      && dst_inc_offset <= std::numeric_limits<int>::max()
#endif
                 )
            {
                GDALCopyWords64( srcPtr, srcType.GetNumericDataType(),
                                 static_cast<int>(src_inc_offset),
                                 dstPtr, dstType.GetNumericDataType(),
                                 static_cast<int>(dst_inc_offset),
                                 static_cast<GPtrDiff_t>(nIters) );
                return;
            }

            while(true)
            {
                if( bNeedsFreeDynamicMemory )
                {
                    dstType.FreeDynamicMemory(dstPtr);
                }
                GDALExtendedDataType::CopyValue(srcPtr, srcType, dstPtr, dstType);
                if( (--nIters) == 0 )
                    break;
                srcPtr += src_inc_offset;
                dstPtr += dst_inc_offset;
            }
        }
    };

    if( nDims == 1 )
    {
        lambdaLastDim(0);
    }
    else if( nDims == 2 )
    {
        auto nIters = count[0];
        while(true)
        {
            lambdaLastDim(0);
            if( (--nIters) == 0 )
                break;
            stack[0].src_ptr += stack[0].src_inc_offset;
            stack[0].dst_ptr += stack[0].dst_inc_offset;
        }
    }
    else if( nDims == 3 )
    {
        stack[0].nIters = count[0];
        while(true)
        {
            stack[1].src_ptr = stack[0].src_ptr;
            stack[1].dst_ptr = stack[0].dst_ptr;
            auto nIters = count[1];
            while(true)
            {
                lambdaLastDim(1);
                if( (--nIters) == 0 )
                    break;
                stack[1].src_ptr += stack[1].src_inc_offset;
                stack[1].dst_ptr += stack[1].dst_inc_offset;
            }
            if( (--stack[0].nIters) == 0 )
                break;
            stack[0].src_ptr += stack[0].src_inc_offset;
            stack[0].dst_ptr += stack[0].dst_inc_offset;
        }
    }
    else
    {
        // Implementation valid for nDims >= 3

        size_t dimIdx = 0;
        // Non-recursive implementation. Hence the gotos
        // It might be possible to rewrite this without gotos, but I find they
        // make it clearer to understand the recursive nature of the code
lbl_next_depth:
        if( dimIdx == nDimsMinus1 - 1 )
        {
            auto nIters = count[dimIdx];
            while(true)
            {
                lambdaLastDim(dimIdx);
                if( (--nIters) == 0 )
                    break;
                stack[dimIdx].src_ptr += stack[dimIdx].src_inc_offset;
                stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
            }
            // If there was a test if( dimIdx > 0 ), that would be valid for nDims == 2
            goto lbl_return_to_caller;
        }
        else
        {
            stack[dimIdx].nIters = count[dimIdx];
            while(true)
            {
                dimIdx ++;
                stack[dimIdx].src_ptr = stack[dimIdx-1].src_ptr;
                stack[dimIdx].dst_ptr = stack[dimIdx-1].dst_ptr;
                goto lbl_next_depth;
lbl_return_to_caller:
                dimIdx --;
                if( (--stack[dimIdx].nIters) == 0 )
                    break;
                stack[dimIdx].src_ptr += stack[dimIdx].src_inc_offset;
                stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
            }
            if( dimIdx > 0 )
                goto lbl_return_to_caller;
        }
    }
}

/************************************************************************/
/*                                   IRead()                            */
/************************************************************************/

bool MEMAbstractMDArray::IRead(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               void* pDstBuffer) const
{
    const auto nDims = m_aoDims.size();
    if( nDims == 0 )
    {
        GDALExtendedDataType::CopyValue(m_pabyArray, m_oType, pDstBuffer, bufferDataType);
        return true;
    }
    std::vector<StackReadWrite> stack(nDims);
    const auto nBufferDTSize = bufferDataType.GetSize();
    GPtrDiff_t startSrcOffset = 0;
    for( size_t i = 0; i < nDims; i++ )
    {
        startSrcOffset += static_cast<GPtrDiff_t>(arrayStartIdx[i] * m_anStrides[i]);
        stack[i].src_inc_offset = static_cast<GPtrDiff_t>(arrayStep[i] * m_anStrides[i]);
        stack[i].dst_inc_offset = static_cast<GPtrDiff_t>(bufferStride[i] * nBufferDTSize);
    }
    stack[0].src_ptr = m_pabyArray + startSrcOffset;
    stack[0].dst_ptr = static_cast<GByte*>(pDstBuffer);

    ReadWrite(false, count, stack, m_oType, bufferDataType);
    return true;
}

/************************************************************************/
/*                                IWrite()                              */
/************************************************************************/

bool MEMAbstractMDArray::IWrite(const GUInt64* arrayStartIdx,
                               const size_t* count,
                               const GInt64* arrayStep,
                               const GPtrDiff_t* bufferStride,
                               const GDALExtendedDataType& bufferDataType,
                               const void* pSrcBuffer)
{
    const auto nDims = m_aoDims.size();
    if( nDims == 0 )
    {
        m_oType.FreeDynamicMemory(m_pabyArray);
        GDALExtendedDataType::CopyValue(pSrcBuffer, bufferDataType, m_pabyArray, m_oType);
        return true;
    }
    std::vector<StackReadWrite> stack(nDims);
    const auto nBufferDTSize = bufferDataType.GetSize();
    GPtrDiff_t startDstOffset = 0;
    for( size_t i = 0; i < nDims; i++ )
    {
        startDstOffset += static_cast<GPtrDiff_t>(arrayStartIdx[i] * m_anStrides[i]);
        stack[i].dst_inc_offset = static_cast<GPtrDiff_t>(arrayStep[i] * m_anStrides[i]);
        stack[i].src_inc_offset = static_cast<GPtrDiff_t>(bufferStride[i] * nBufferDTSize);
    }

    stack[0].dst_ptr = m_pabyArray + startDstOffset;
    stack[0].src_ptr = static_cast<const GByte*>(pSrcBuffer);

    ReadWrite(true, count, stack, bufferDataType, m_oType);
    return true;
}

/************************************************************************/
/*                               MEMMDArray()                           */
/************************************************************************/

MEMMDArray::MEMMDArray(const std::string& osParentName,
                       const std::string& osName,
               const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
               const GDALExtendedDataType& oType):
    GDALAbstractMDArray(osParentName, osName),
    MEMAbstractMDArray(osParentName, osName, aoDimensions, oType),
    GDALMDArray(osParentName, osName)
{
}

/************************************************************************/
/*                              ~MEMMDArray()                           */
/************************************************************************/

MEMMDArray::~MEMMDArray()
{
    if( m_pabyNoData )
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
        CPLFree(m_pabyNoData);
    }
}

/************************************************************************/
/*                          GetRawNoDataValue()                         */
/************************************************************************/

const void* MEMMDArray::GetRawNoDataValue() const
{
    return m_pabyNoData;
}

/************************************************************************/
/*                          SetRawNoDataValue()                         */
/************************************************************************/

bool MEMMDArray::SetRawNoDataValue(const void* pNoData)
{
    if( m_pabyNoData )
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
    }

    if( pNoData == nullptr )
    {
        CPLFree(m_pabyNoData);
        m_pabyNoData = nullptr;
    }
    else
    {
        const auto nSize = m_oType.GetSize();
        if( m_pabyNoData == nullptr )
        {
            m_pabyNoData = static_cast<GByte*>(CPLMalloc(nSize));
        }
        memset(m_pabyNoData, 0, nSize);
        GDALExtendedDataType::CopyValue( pNoData, m_oType, m_pabyNoData, m_oType );
    }
    return true;
}

/************************************************************************/
/*                            GetAttribute()                            */
/************************************************************************/

std::shared_ptr<GDALAttribute> MEMMDArray::GetAttribute(const std::string& osName) const
{
    auto oIter = m_oMapAttributes.find(osName);
    if( oIter != m_oMapAttributes.end() )
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                             GetAttributes()                          */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>> MEMMDArray::GetAttributes(CSLConstList) const
{
    std::vector<std::shared_ptr<GDALAttribute>> oRes;
    for( const auto& oIter: m_oMapAttributes )
    {
        oRes.push_back(oIter.second);
    }
    return oRes;
}

/************************************************************************/
/*                            CreateAttribute()                         */
/************************************************************************/

std::shared_ptr<GDALAttribute> MEMMDArray::CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList)
{
    if( osName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty attribute name not supported");
        return nullptr;
    }
    if( m_oMapAttributes.find(osName) != m_oMapAttributes.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An attribute with same name already exists");
        return nullptr;
    }
    auto newAttr(MEMAttribute::Create(GetFullName(), osName, anDimensions, oDataType));
    if( !newAttr )
        return nullptr;
    m_oMapAttributes[osName] = newAttr;
    return newAttr;
}

/************************************************************************/
/*                            BuildDimensions()                         */
/************************************************************************/

static std::vector<std::shared_ptr<GDALDimension>> BuildDimensions(
    const std::vector<GUInt64>& anDimensions)
{
    std::vector<std::shared_ptr<GDALDimension>> res;
    for( size_t i = 0; i < anDimensions.size(); i++ )
    {
        res.emplace_back(std::make_shared<MEMDimension>(
            std::string(), CPLSPrintf("dim%u", static_cast<unsigned>(i)),
            std::string(), std::string(), anDimensions[i]));
    }
    return res;
}

/************************************************************************/
/*                             MEMAttribute()                           */
/************************************************************************/

MEMAttribute::MEMAttribute(const std::string& osParentName,
                           const std::string& osName,
                           const std::vector<GUInt64>& anDimensions,
                           const GDALExtendedDataType& oType):
    GDALAbstractMDArray(osParentName, osName),
    MEMAbstractMDArray(osParentName, osName, BuildDimensions(anDimensions), oType),
    GDALAttribute(osParentName, osName)
{
}

/************************************************************************/
/*                        MEMAttribute::Create()                        */
/************************************************************************/

std::shared_ptr<MEMAttribute> MEMAttribute::Create(const std::string& osParentName,
                                                const std::string& osName,
                                                const std::vector<GUInt64>& anDimensions,
                                                const GDALExtendedDataType& oType)
{
    auto attr(std::shared_ptr<MEMAttribute>(
        new MEMAttribute(osParentName, osName, anDimensions, oType)));
    attr->SetSelf(attr);
    if( !attr->Init() )
        return nullptr;
    return attr;
}

/************************************************************************/
/*                             MEMDimension()                           */
/************************************************************************/

MEMDimension::MEMDimension(const std::string& osParentName,
                           const std::string& osName,
                           const std::string& osType,
                           const std::string& osDirection,
                           GUInt64 nSize):
    GDALDimension(osParentName, osName, osType, osDirection, nSize)
{
}

/************************************************************************/
/*                           SetIndexingVariable()                      */
/************************************************************************/

// cppcheck-suppress passedByValue
bool MEMDimension::SetIndexingVariable(std::shared_ptr<GDALMDArray> poIndexingVariable)
{
    m_poIndexingVariable = poIndexingVariable;
    return true;
}

/************************************************************************/
/*                             CreateDimension()                        */
/************************************************************************/

std::shared_ptr<GDALDimension> MEMGroup::CreateDimension(const std::string& osName,
                                                         const std::string& osType,
                                                         const std::string& osDirection,
                                                         GUInt64 nSize,
                                                         CSLConstList)
{
    if( osName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Empty dimension name not supported");
        return nullptr;
    }
    if( m_oMapDimensions.find(osName) != m_oMapDimensions.end() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A dimension with same name already exists");
        return nullptr;
    }
    auto newDim(std::make_shared<MEMDimension>(GetFullName(), osName, osType, osDirection, nSize));
    m_oMapDimensions[osName] = newDim;
    return newDim;
}

/************************************************************************/
/*                     CreateMultiDimensional()                         */
/************************************************************************/

GDALDataset * MEMDataset::CreateMultiDimensional( const char * pszFilename,
                                                  CSLConstList /*papszRootGroupOptions*/,
                                                  CSLConstList /*papszOptions*/ )
{
    auto poDS = new MEMDataset();

    poDS->SetDescription(pszFilename);
    poDS->m_poPrivate->m_poRootGroup.reset(new MEMGroup(std::string(), nullptr));

    return poDS;
}

/************************************************************************/
/*                          GetRootGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup> MEMDataset::GetRootGroup() const
{
    return m_poPrivate->m_poRootGroup;
}

/************************************************************************/
/*                     MEMDatasetIdentify()                             */
/************************************************************************/

static int MEMDatasetIdentify( GDALOpenInfo * poOpenInfo )
{
    return (STARTS_WITH(poOpenInfo->pszFilename, "MEM:::") &&
            poOpenInfo->fpL == nullptr);
}

/************************************************************************/
/*                       MEMDatasetDelete()                             */
/************************************************************************/

static CPLErr MEMDatasetDelete( const char* /* fileName */)
{
    /* Null implementation, so that people can Delete("MEM:::") */
    return CE_None;
}

/************************************************************************/
/*                          GDALRegister_MEM()                          */
/************************************************************************/

void GDALRegister_MEM()

{
    if( GDALGetDriverByName( "MEM" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "MEM" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIDIM_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "In Memory Raster" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32 Float32 Float64 "
                               "CInt16 CInt32 CFloat32 CFloat64" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='INTERLEAVE' type='string-select' default='BAND'>"
"       <Value>BAND</Value>"
"       <Value>PIXEL</Value>"
"   </Option>"
"</CreationOptionList>" );

    // Define GDAL_NO_OPEN_FOR_MEM_DRIVER macro to undefine Open() method for
    // MEM driver.  Otherwise, bad user input can trigger easily a GDAL crash
    // as random pointers can be passed as a string.  All code in GDAL tree
    // using the MEM driver use the Create() method only, so Open() is not
    // needed, except for esoteric uses.
#ifndef GDAL_NO_OPEN_FOR_MEM_DRIVER
    poDriver->pfnOpen = MEMDataset::Open;
    poDriver->pfnIdentify = MEMDatasetIdentify;
#endif
    poDriver->pfnCreate = MEMDataset::Create;
    poDriver->pfnCreateMultiDimensional = MEMDataset::CreateMultiDimensional;
    poDriver->pfnDelete = MEMDatasetDelete;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
