/******************************************************************************
 * $Id$
 *
 * Project:  Memory Array Translator
 * Purpose:  Complete implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "memdataset.h"

CPL_CVSID("$Id$");

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

MEMRasterBand::MEMRasterBand( GDALDataset *poDSIn, int nBandIn,
                              GByte *pabyDataIn, GDALDataType eTypeIn,
                              GSpacing nPixelOffsetIn, GSpacing nLineOffsetIn,
                              int bAssumeOwnership, const char * pszPixelType) :
    GDALPamRasterBand(FALSE),
    pabyData(pabyDataIn),
    // Skip nPixelOffset and nLineOffset.
    bOwnData(bAssumeOwnership),
    bNoDataSet(FALSE),
    dfNoData(0.0),
    poColorTable(NULL),
    eColorInterp(GCI_Undefined),
    pszUnitType(NULL),
    papszCategoryNames(NULL),
    dfOffset(0.0),
    dfScale(1.0),
    psSavedHistograms(NULL)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDS->GetAccess();

    eDataType = eTypeIn;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    if( nPixelOffsetIn == 0 )
        nPixelOffsetIn = GDALGetDataTypeSizeBytes(eTypeIn);

    if( nLineOffsetIn == 0 )
        nLineOffsetIn = nPixelOffsetIn * static_cast<size_t>(nBlockXSize);

    nPixelOffset = nPixelOffsetIn;
    nLineOffset = nLineOffsetIn;
    bOwnData = bAssumeOwnership;

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

    if( poColorTable != NULL )
        delete poColorTable;

    CPLFree( pszUnitType );
    CSLDestroy( papszCategoryNames );

    if (psSavedHistograms != NULL)
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
                pabyData + nLineOffset*static_cast<size_t>(iLine + nYOff) +
                nXOff*nPixelOffset,
                eDataType,
                static_cast<int>(nPixelOffset),
                reinterpret_cast<GByte*>( pData ) +
                nLineSpaceBuf * static_cast<size_t>(iLine),
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
                nLineSpaceBuf*(size_t)iLine,
                eBufType,
                static_cast<int>(nPixelSpaceBuf),
                pabyData + nLineOffset*static_cast<size_t>(iLine + nYOff) +
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
        GByte* pabyData = NULL;
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

    GDALProgressFunc pfnProgressGlobal = psExtraArg->pfnProgress;
    void *pProgressDataGlobal = psExtraArg->pProgressData;

    CPLErr eErr = CE_None;
    for( int iBandIndex = 0;
         iBandIndex < nBandCount && eErr == CE_None;
         iBandIndex++ )
    {
        GDALRasterBand *poBand = GetRasterBand(panBandMap[iBandIndex]);

        if (poBand == NULL)
        {
            eErr = CE_Failure;
            break;
        }

        GByte *pabyBandData
            = reinterpret_cast<GByte *>(pData) + iBandIndex * nBandSpaceBuf;

        psExtraArg->pfnProgress = GDALScaledProgress;
        psExtraArg->pProgressData =
            GDALCreateScaledProgress( 1.0 * iBandIndex / nBandCount,
                                      1.0 * (iBandIndex + 1) / nBandCount,
                                      pfnProgressGlobal,
                                      pProgressDataGlobal );

        eErr = poBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                 reinterpret_cast<void *>( pabyBandData ),
                                 nBufXSize, nBufYSize,
                                 eBufType, nPixelSpaceBuf, nLineSpaceBuf,
                                 psExtraArg );

        GDALDestroyScaledProgress( psExtraArg->pProgressData );
    }

    psExtraArg->pfnProgress = pfnProgressGlobal;
    psExtraArg->pProgressData = pProgressDataGlobal;

    return eErr;
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
    if( poColorTable != NULL )
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
    return poColorTable;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr MEMRasterBand::SetColorTable( GDALColorTable *poCT )

{
    if( poColorTable != NULL )
        delete poColorTable;

    if( poCT == NULL )
        poColorTable = NULL;
    else
        poColorTable = poCT->Clone();

    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *MEMRasterBand::GetUnitType()

{
    if( pszUnitType == NULL )
        return "";

    return pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr MEMRasterBand::SetUnitType( const char *pszNewValue )

{
    CPLFree( pszUnitType );

    if( pszNewValue == NULL )
        pszUnitType = NULL;
    else
        pszUnitType = CPLStrdup(pszNewValue);

    return CE_None;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double MEMRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != NULL )
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
    if( pbSuccess != NULL )
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
    return papszCategoryNames;
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr MEMRasterBand::SetCategoryNames( char ** papszNewNames )

{
    CSLDestroy( papszCategoryNames );
    papszCategoryNames = CSLDuplicate( papszNewNames );

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
    if( psNode != NULL )
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
    if( psHistItem == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Insert our new default histogram at the front of the            */
/*      histogram list so that it will be the default histogram.        */
/* -------------------------------------------------------------------- */

    if( psSavedHistograms == NULL )
        psSavedHistograms = CPLCreateXMLNode( NULL, CXT_Element,
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
    if( psSavedHistograms != NULL )
    {
        for( CPLXMLNode *psXMLHist = psSavedHistograms->psChild;
             psXMLHist != NULL; psXMLHist = psXMLHist->psNext )
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
    pszProjection(NULL),
    nGCPCount(0),
    pasGCPs(NULL)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -1.0;
}

/************************************************************************/
/*                            ~MEMDataset()                            */
/************************************************************************/

MEMDataset::~MEMDataset()

{
    FlushCache();
    CPLFree( pszProjection );

    GDALDeinitGCPs( nGCPCount, pasGCPs );
    CPLFree( pasGCPs );
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

const char *MEMDataset::GetProjectionRef()

{
    if( pszProjection == NULL )
        return "";

    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr MEMDataset::SetProjection( const char *pszProjectionIn )

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

            if( RequestedRasterBand != NULL )
            {
                // return the internal band data pointer
                return(RequestedRasterBand->GetData());
            }
        }
    }

    return NULL;
}
/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int MEMDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *MEMDataset::GetGCPProjection()

{
    return osGCPProjection;
}

/************************************************************************/
/*                              GetGCPs()                               */
/************************************************************************/

const GDAL_GCP *MEMDataset::GetGCPs()

{
    return pasGCPs;
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr MEMDataset::SetGCPs( int nNewCount, const GDAL_GCP *pasNewGCPList,
                            const char *pszGCPProjection )

{
    GDALDeinitGCPs( nGCPCount, pasGCPs );
    CPLFree( pasGCPs );

    if( pszGCPProjection == NULL )
        osGCPProjection = "";
    else
        osGCPProjection = pszGCPProjection;

    nGCPCount = nNewCount;
    pasGCPs = GDALDuplicateGCPs( nGCPCount, pasNewGCPList );

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
    if( CSLFetchNameValue( papszOptions, "DATAPOINTER" ) == NULL )
    {
        const GSpacing nTmp = nPixelSize * GetRasterXSize();
        GByte *pData = NULL;
#if SIZEOF_VOIDP == 4
        if( nTmp > INT_MAX )
            pData = NULL;
        else
#endif
            pData = reinterpret_cast<GByte *>(
                VSI_CALLOC_VERBOSE((size_t)nTmp, GetRasterYSize() ) );

        if( pData == NULL )
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
    if( pszOption == NULL )
        nPixelOffset = nPixelSize;
    else
        nPixelOffset = CPLAtoGIntBig(pszOption);

    pszOption = CSLFetchNameValue(papszOptions, "LINEOFFSET");
    GSpacing nLineOffset;
    if( pszOption == NULL )
        nLineOffset = GetRasterXSize() * static_cast<size_t>( nPixelOffset );
    else
        nLineOffset = CPLAtoGIntBig(pszOption);

    SetBand( nBandId,
             new MEMRasterBand( this, nBandId, pData, eType,
                                nPixelOffset, nLineOffset, FALSE ) );

    return CE_None;
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
        || poOpenInfo->fpL != NULL )
        return NULL;

    char **papszOptions
        = CSLTokenizeStringComplex(poOpenInfo->pszFilename+6, ",",
                                   TRUE, FALSE );

/* -------------------------------------------------------------------- */
/*      Verify we have all required fields                              */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "PIXELS" ) == NULL
        || CSLFetchNameValue( papszOptions, "LINES" ) == NULL
        || CSLFetchNameValue( papszOptions, "DATAPOINTER" ) == NULL )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Missing required field (one of PIXELS, LINES or DATAPOINTER).  "
            "Unable to access in-memory array." );

        CSLDestroy( papszOptions );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the new MEMDataset object.                               */
/* -------------------------------------------------------------------- */
    MEMDataset *poDS = new MEMDataset();

    poDS->nRasterXSize = atoi(CSLFetchNameValue(papszOptions,"PIXELS"));
    poDS->nRasterYSize = atoi(CSLFetchNameValue(papszOptions,"LINES"));
    poDS->eAccess = GA_Update;

/* -------------------------------------------------------------------- */
/*      Extract other information.                                      */
/* -------------------------------------------------------------------- */
    const char *pszOption = CSLFetchNameValue(papszOptions,"BANDS");
    int nBands = 1;
    if( pszOption != NULL )
    {
        nBands = atoi(pszOption);
    }

    if( !GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBands, TRUE))
    {
        CSLDestroy( papszOptions );
        delete poDS;
        return NULL;
    }

    pszOption = CSLFetchNameValue(papszOptions,"DATATYPE");
    GDALDataType eType = GDT_Byte;
    if( pszOption != NULL )
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
                return NULL;
            }
        }
    }

    pszOption = CSLFetchNameValue(papszOptions, "PIXELOFFSET");
    GSpacing nPixelOffset;
    if( pszOption == NULL )
        nPixelOffset = GDALGetDataTypeSizeBytes(eType);
    else
        nPixelOffset = CPLScanUIntBig(pszOption,
                                      static_cast<int>(strlen(pszOption)));

    pszOption = CSLFetchNameValue(papszOptions, "LINEOFFSET");
    GSpacing nLineOffset = 0;
    if( pszOption == NULL )
        nLineOffset = poDS->nRasterXSize * static_cast<size_t>( nPixelOffset );
    else
        nLineOffset = CPLScanUIntBig(pszOption,
                                     static_cast<int>(strlen(pszOption)));

    pszOption = CSLFetchNameValue(papszOptions, "BANDOFFSET");
    GSpacing nBandOffset = 0;
    if( pszOption == NULL )
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
        return NULL;
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
        return NULL;
    }
#endif

    std::vector<GByte*> apbyBandData;
    bool bAllocOK = true;

    if( bPixelInterleaved )
    {
        apbyBandData.push_back(
            reinterpret_cast<GByte *>( VSI_CALLOC_VERBOSE( 1, nGlobalSize ) ) );

        if( apbyBandData[0] == NULL )
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
            if( apbyBandData[iBand] == NULL )
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
        return NULL;
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
        MEMRasterBand *poNewBand = NULL;

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
/*                     MEMDatasetIdentify()                             */
/************************************************************************/

static int MEMDatasetIdentify( GDALOpenInfo * poOpenInfo )
{
    return (STARTS_WITH(poOpenInfo->pszFilename, "MEM:::") &&
            poOpenInfo->fpL == NULL);
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
    if( GDALGetDriverByName( "MEM" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "MEM" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
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
    poDriver->pfnDelete = MEMDatasetDelete;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
