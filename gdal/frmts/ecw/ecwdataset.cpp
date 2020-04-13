/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  ECW (ERDAS Wavelet Compression Format) Driver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

// ncsjpcbuffer.h needs the min and max macros.
#undef NOMINMAX

#include "cpl_minixml.h"
#include "gdal_ecw.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "ogr_api.h"
#include "ogr_geometry.h"

#include "../mem/memdataset.h"

CPL_CVSID("$Id$")

#undef NOISY_DEBUG

#ifdef FRMT_ecw

constexpr unsigned char jpc_header[] = {0xff,0x4f};
constexpr unsigned char jp2_header[] =
    {0x00,0x00,0x00,0x0c,0x6a,0x50,0x20,0x20,0x0d,0x0a,0x87,0x0a};

static CPLMutex *hECWDatasetMutex = nullptr;
static int    bNCSInitialized = FALSE;

void ECWInitialize( void );

extern "C" int CPL_DLL GDALIsInGlobalDestructor(void);

#define BLOCK_SIZE 256

GDALDataset* ECWDatasetOpenJPEG2000(GDALOpenInfo* poOpenInfo);

/************************************************************************/
/*                           ECWReportError()                           */
/************************************************************************/

void ECWReportError(CNCSError& oErr, const char* pszMsg)
{
#if ECWSDK_VERSION<50
    char* pszErrorMessage = oErr.GetErrorMessage();
    CPLError( CE_Failure, CPLE_AppDefined,
              "%s%s", pszMsg, pszErrorMessage );
    NCSFree(pszErrorMessage);
#else
    CPLError( CE_Failure, CPLE_AppDefined,
             "%s%s", pszMsg, NCSGetLastErrorText(oErr) );
#endif
}

/************************************************************************/
/*                           ECWRasterBand()                            */
/************************************************************************/

ECWRasterBand::ECWRasterBand( ECWDataset *poDSIn, int nBandIn, int iOverviewIn,
                              char** papszOpenOptions )

{
    this->poDS = poDSIn;
    poGDS = poDSIn;

    this->iOverview = iOverviewIn;
    this->nBand = nBandIn;
    eDataType = poDSIn->eRasterDataType;

    nRasterXSize = poDS->GetRasterXSize() / ( 1 << (iOverview+1));
    nRasterYSize = poDS->GetRasterYSize() / ( 1 << (iOverview+1));

    nBlockXSize = BLOCK_SIZE;
    nBlockYSize = BLOCK_SIZE;

/* -------------------------------------------------------------------- */
/*      Work out band color interpretation.                             */
/* -------------------------------------------------------------------- */
    if( poDSIn->psFileInfo->eColorSpace == NCSCS_NONE )
        eBandInterp = GCI_Undefined;
    else if( poDSIn->psFileInfo->eColorSpace == NCSCS_GREYSCALE )
    {
        eBandInterp = GCI_GrayIndex;
        //we could also have alpha band.
        if ( strcmp(poDSIn->psFileInfo->pBands[nBand-1].szDesc, NCS_BANDDESC_AllOpacity) == 0 ||
             strcmp(poDSIn->psFileInfo->pBands[nBand-1].szDesc, NCS_BANDDESC_GreyscaleOpacity) ==0 ){
            eBandInterp = GCI_AlphaBand;
        }
    }else if (poDSIn->psFileInfo->eColorSpace == NCSCS_MULTIBAND ){
        eBandInterp = ECWGetColorInterpretationByName(poDSIn->psFileInfo->pBands[nBand-1].szDesc);
    }else if (poDSIn->psFileInfo->eColorSpace == NCSCS_sRGB){
        eBandInterp = ECWGetColorInterpretationByName(poDSIn->psFileInfo->pBands[nBand-1].szDesc);
        if( eBandInterp == GCI_Undefined )
        {
            if( nBand == 1 )
                eBandInterp = GCI_RedBand;
            else if( nBand == 2 )
                eBandInterp = GCI_GreenBand;
            else if( nBand == 3 )
                eBandInterp = GCI_BlueBand;
            else if (nBand == 4 )
            {
                if (strcmp(poDSIn->psFileInfo->pBands[nBand-1].szDesc, NCS_BANDDESC_AllOpacity) == 0)
                    eBandInterp = GCI_AlphaBand;
                else
                    eBandInterp = GCI_Undefined;
            }
            else
            {
                eBandInterp = GCI_Undefined;
            }
        }
    }
    else if( poDSIn->psFileInfo->eColorSpace == NCSCS_YCbCr )
    {
        if( CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB","YES") ))
        {
            if( nBand == 1 )
                eBandInterp = GCI_RedBand;
            else if( nBand == 2 )
                eBandInterp = GCI_GreenBand;
            else if( nBand == 3 )
                eBandInterp = GCI_BlueBand;
            else
                eBandInterp = GCI_Undefined;
        }
        else
        {
            if( nBand == 1 )
                eBandInterp = GCI_YCbCr_YBand;
            else if( nBand == 2 )
                eBandInterp = GCI_YCbCr_CbBand;
            else if( nBand == 3 )
                eBandInterp = GCI_YCbCr_CrBand;
            else
                eBandInterp = GCI_Undefined;
        }
    }
    else
        eBandInterp = GCI_Undefined;

/* -------------------------------------------------------------------- */
/*      If this is the base level, create a set of overviews.           */
/* -------------------------------------------------------------------- */
    if( iOverview == -1 )
    {
        int i;
        for( i = 0;
             nRasterXSize / (1 << (i+1)) > 128
                 && nRasterYSize / (1 << (i+1)) > 128;
             i++ )
        {
            apoOverviews.push_back( new ECWRasterBand( poDSIn, nBandIn, i, papszOpenOptions ) );
        }
    }

    bPromoteTo8Bit =
        poDSIn->psFileInfo->nBands == 4 && nBand == 4 &&
        poDSIn->psFileInfo->pBands[0].nBits == 8 &&
        poDSIn->psFileInfo->pBands[1].nBits == 8 &&
        poDSIn->psFileInfo->pBands[2].nBits == 8 &&
        poDSIn->psFileInfo->pBands[3].nBits == 1 &&
        eBandInterp == GCI_AlphaBand &&
        CPLFetchBool(papszOpenOptions, "1BIT_ALPHA_PROMOTION",
            CPLTestBool(
                CPLGetConfigOption("GDAL_ECW_PROMOTE_1BIT_ALPHA_AS_8BIT",
                                   "YES")));
    if( bPromoteTo8Bit )
        CPLDebug("ECW", "Fourth (alpha) band is promoted from 1 bit to 8 bit");

    if( (poDSIn->psFileInfo->pBands[nBand-1].nBits % 8) != 0 && !bPromoteTo8Bit )
        GDALPamRasterBand::SetMetadataItem("NBITS",
                        CPLString().Printf("%d",poDSIn->psFileInfo->pBands[nBand-1].nBits),
                        "IMAGE_STRUCTURE" );

    GDALPamRasterBand::SetDescription(poDSIn->psFileInfo->pBands[nBand-1].szDesc);
}

/************************************************************************/
/*                          ~ECWRasterBand()                           */
/************************************************************************/

ECWRasterBand::~ECWRasterBand()

{
    GDALRasterBand::FlushCache();

    while( !apoOverviews.empty() )
    {
        delete apoOverviews.back();
        apoOverviews.pop_back();
    }
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *ECWRasterBand::GetOverview( int iOverviewIn )

{
    if( iOverviewIn >= 0 && iOverviewIn < (int) apoOverviews.size() )
        return apoOverviews[iOverviewIn];
    else
        return nullptr;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp ECWRasterBand::GetColorInterpretation()

{
    return eBandInterp;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/*                                                                      */
/*      This would normally just be used by folks using the ECW code    */
/*      to read JP2 streams in other formats (such as NITF) and         */
/*      providing their own color interpretation regardless of what     */
/*      ECW might think the stream itself says.                         */
/************************************************************************/

CPLErr ECWRasterBand::SetColorInterpretation( GDALColorInterp eNewInterp )

{
    eBandInterp = eNewInterp;

    return CE_None;
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr ECWRasterBand::AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                  int nBufXSize, int nBufYSize,
                                  GDALDataType eDT,
                                  char **papszOptions )
{
    const int nResFactor = 1 << (iOverview+1);

    return poGDS->AdviseRead( nXOff * nResFactor,
                              nYOff * nResFactor,
                              nXSize * nResFactor,
                              nYSize * nResFactor,
                              nBufXSize, nBufYSize, eDT,
                              1, &nBand, papszOptions );
}

//statistics support:
#if ECWSDK_VERSION >= 50

/************************************************************************/
/*                       GetDefaultHistogram()                          */
/************************************************************************/

CPLErr ECWRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
    int *pnBuckets, GUIntBig ** ppanHistogram,
    int bForce,
    GDALProgressFunc f, void *pProgressData)
{
    int bForceCoalesced = bForce;
    // If file version is smaller than 3, there will be no statistics in the file. But if it is version 3 or higher we don't want underlying implementation to compute histogram
    // so we set bForceCoalesced to FALSE.
    if (poGDS->psFileInfo->nFormatVersion >= 3){
        bForceCoalesced = FALSE;
    }
    // We check if we have PAM histogram. If we have them we return them. This will allow to override statistics stored in the file.
    CPLErr pamError =  GDALPamRasterBand::GetDefaultHistogram(pdfMin, pdfMax, pnBuckets, ppanHistogram, bForceCoalesced, f, pProgressData);
    if ( pamError == CE_None || poGDS->psFileInfo->nFormatVersion<3 || eBandInterp == GCI_AlphaBand){
        return pamError;
    }

    NCS::CError error = poGDS->StatisticsEnsureInitialized();
    if (!error.Success()){
        CPLError( CE_Warning, CPLE_AppDefined,
            "ECWRDataset::StatisticsEnsureInitialized failed in ECWRasterBand::GetDefaultHistogram. " );
        return CE_Failure;
    }
    GetBandIndexAndCountForStatistics(nStatsBandIndex, nStatsBandCount);
    bool bHistogramFromFile = false;
    if ( poGDS->pStatistics != nullptr ){
        NCSBandStats& bandStats = poGDS->pStatistics->BandsStats[nStatsBandIndex];
        if ( bandStats.Histogram != nullptr && bandStats.nHistBucketCount > 0 ){
            *pnBuckets = bandStats.nHistBucketCount;
            *ppanHistogram = static_cast<GUIntBig *>(VSIMalloc(bandStats.nHistBucketCount * sizeof(GUIntBig)));
            for (size_t i = 0; i < bandStats.nHistBucketCount; i++){
                (*ppanHistogram)[i] = static_cast<GUIntBig>(bandStats.Histogram[i]);
            }
            //JTO: this is not perfect as You can't tell who wrote the histogram !!!
            //It will offset it unnecessarily for files with hists not modified by GDAL.
            const double dfHalfBucket = (bandStats.fMaxHist -  bandStats.fMinHist) / (2 * (*pnBuckets - 1));
            if ( pdfMin != nullptr ){
                *pdfMin = bandStats.fMinHist - dfHalfBucket;
            }
            if ( pdfMax != nullptr ){
                *pdfMax = bandStats.fMaxHist + dfHalfBucket;
            }
            bHistogramFromFile = true;
        }
    }

    if (!bHistogramFromFile ){
        if (bForce == TRUE){
            //compute. Save.
            pamError = GDALPamRasterBand::GetDefaultHistogram(pdfMin, pdfMax, pnBuckets, ppanHistogram, TRUE, f,pProgressData);
            if (pamError == CE_None){
                const CPLErr error2 = SetDefaultHistogram(*pdfMin, *pdfMax, *pnBuckets, *ppanHistogram);
                if (error2 != CE_None){
                    //Histogram is there but we failed to save it back to file.
                    CPLError (CE_Warning, CPLE_AppDefined,
                        "SetDefaultHistogram failed in ECWRasterBand::GetDefaultHistogram. Histogram might not be saved in .ecw file." );
                }
                return CE_None;
            }
            return pamError;
        }
        // No histogram, no forced computation.
        return CE_Warning;
    }
    // Statistics were already there and were used.
    return CE_None;
}

/************************************************************************/
/*                       SetDefaultHistogram()                          */
/************************************************************************/

CPLErr ECWRasterBand::SetDefaultHistogram( double dfMin, double dfMax,
                                           int nBuckets,
                                           GUIntBig *panHistogram )
{
    //Only version 3 supports saving statistics.
    if (poGDS->psFileInfo->nFormatVersion < 3 || eBandInterp == GCI_AlphaBand){
        return GDALPamRasterBand::SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);
    }

    //determine if there are statistics in PAM file.
    double dummy;
    int dummy_i;
    GUIntBig *dummy_histogram = nullptr;
    bool hasPAMDefaultHistogram =
        GDALPamRasterBand::GetDefaultHistogram(
            &dummy, &dummy, &dummy_i, &dummy_histogram,
            FALSE, nullptr, nullptr) == CE_None;
    if( hasPAMDefaultHistogram ) {
        VSIFree(dummy_histogram);
    }

    // ECW SDK ignores statistics for opacity bands. So we need to compute
    // number of bands without opacity.
    GetBandIndexAndCountForStatistics(nStatsBandIndex, nStatsBandCount);
    UINT32 bucketCounts[256];
    std::fill_n(bucketCounts, nStatsBandCount, 0);
    bucketCounts[nStatsBandIndex] = nBuckets;

    NCS::CError error = poGDS->StatisticsEnsureInitialized();
    if (!error.Success()){
        CPLError( CE_Warning, CPLE_AppDefined,
            "ECWRDataset::StatisticsEnsureInitialized failed in ECWRasterBand::SetDefaultHistogram. Default histogram will be written to PAM. " );
        return GDALPamRasterBand::SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);
    }

    NCSFileStatistics *pStatistics = poGDS->pStatistics;

    if (pStatistics == nullptr){
        error = NCSEcwInitStatistics(&pStatistics, nStatsBandCount, bucketCounts);
        poGDS->bStatisticsDirty = TRUE;
        poGDS->pStatistics = pStatistics;
        if (!error.Success()){
            CPLError( CE_Warning, CPLE_AppDefined,
                        "NCSEcwInitStatistics failed in ECWRasterBand::SetDefaultHistogram." );
            return GDALPamRasterBand::SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);
        }
        //no error statistics properly initialized but there were no statistics previously.
    }else{
        //is there a room for our band already?
        //This should account for following cases:
        //1. Existing histogram (for this or different band) has smaller bucket count.
        //2. There is no existing histogram but statistics are set for one or more bands (pStatistics->nHistBucketCounts is zero).
        if ((int)pStatistics->BandsStats[nStatsBandIndex].nHistBucketCount != nBuckets){
            //no. There is no room. We need more!
            NCSFileStatistics *pNewStatistics = nullptr;
            for (size_t i=0;i<pStatistics->nNumberOfBands;i++){
                bucketCounts[i] = pStatistics->BandsStats[i].nHistBucketCount;
            }
            bucketCounts[nStatsBandIndex] = nBuckets;
            if (nBuckets < static_cast<int>(pStatistics->BandsStats[nStatsBandIndex].nHistBucketCount)){
                pStatistics->BandsStats[nStatsBandIndex].nHistBucketCount = nBuckets;
            }
            error = NCSEcwInitStatistics(&pNewStatistics, nStatsBandCount, bucketCounts);
            if (!error.Success()){
                CPLError( CE_Warning, CPLE_AppDefined,
                          "NCSEcwInitStatistics failed in "
                          "ECWRasterBand::SetDefaultHistogram (reallocate)." );
                return GDALPamRasterBand::SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);
            }
            //we need to copy existing statistics.
            error = NCSEcwCopyStatistics(&pNewStatistics, pStatistics);
            if (!error.Success()){
                CPLError( CE_Warning, CPLE_AppDefined,
                    "NCSEcwCopyStatistics failed in ECWRasterBand::SetDefaultHistogram." );
                NCSEcwFreeStatistics(pNewStatistics);
                return GDALPamRasterBand::SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);
            }
            pNewStatistics->nNumberOfBands = nStatsBandCount;
            NCSEcwFreeStatistics(pStatistics);
            pStatistics = pNewStatistics;
            poGDS->pStatistics = pStatistics;
            poGDS->bStatisticsDirty = TRUE;
        }
    }

    //at this point we have allocated statistics structure.
    double dfHalfBucket = (dfMax - dfMin) / (2 * nBuckets);
    pStatistics->BandsStats[nStatsBandIndex].fMinHist = static_cast<IEEE4>(dfMin + dfHalfBucket);
    pStatistics->BandsStats[nStatsBandIndex].fMaxHist = static_cast<IEEE4>(dfMax - dfHalfBucket);
    for (int i=0;i<nBuckets;i++){
        pStatistics->BandsStats[nStatsBandIndex].Histogram[i] = static_cast<UINT64>(panHistogram[i]);
    }

    if (hasPAMDefaultHistogram){
        CPLError( CE_Debug, CPLE_AppDefined,
                    "PAM default histogram will be overwritten." );
        return GDALPamRasterBand::SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);
    }
    return CE_None;
}

/************************************************************************/
/*                   GetBandIndexAndCountForStatistics()                */
/************************************************************************/

void ECWRasterBand::GetBandIndexAndCountForStatistics(int &bandIndex, int &bandCount) const
{
    bandIndex = nBand-1;
    bandCount = poGDS->nBands;
    for (int i=0;i<poGDS->nBands;i++){
        if (poDS->GetRasterBand(i+1)->GetColorInterpretation() == GCI_AlphaBand){
            bandCount--;
            if ( i < nBand-1 ){
                bandIndex--;
            }
        }
    }
}

/************************************************************************/
/*                           GetMinimum()                               */
/************************************************************************/

double ECWRasterBand::GetMinimum(int* pbSuccess)
{
    if( poGDS->psFileInfo->nFormatVersion >= 3 )
    {
        NCS::CError error = poGDS->StatisticsEnsureInitialized();
        if ( error.Success() )
        {
            GetBandIndexAndCountForStatistics(nStatsBandIndex, nStatsBandCount);
            if ( poGDS->pStatistics != nullptr )
            {
                NCSBandStats& bandStats = poGDS->pStatistics->BandsStats[nStatsBandIndex];
                if (!std::isnan(bandStats.fMinVal))
                {
                    if( pbSuccess )
                        *pbSuccess = TRUE;
                    return bandStats.fMinVal;
                }
            }
        }
    }
    return GDALPamRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                           GetMaximum()                               */
/************************************************************************/

double ECWRasterBand::GetMaximum(int* pbSuccess)
{
    if( poGDS->psFileInfo->nFormatVersion >= 3 )
    {
        NCS::CError error = poGDS->StatisticsEnsureInitialized();
        if ( error.Success() )
        {
            GetBandIndexAndCountForStatistics(nStatsBandIndex, nStatsBandCount);
            if ( poGDS->pStatistics != nullptr )
            {
                NCSBandStats& bandStats = poGDS->pStatistics->BandsStats[nStatsBandIndex];
                if (!std::isnan(bandStats.fMaxVal))
                {
                    if( pbSuccess )
                        *pbSuccess = TRUE;
                    return bandStats.fMaxVal;
                }
            }
        }
    }
    return GDALPamRasterBand::GetMaximum(pbSuccess);
}
/************************************************************************/
/*                          GetStatistics()                             */
/************************************************************************/

CPLErr ECWRasterBand::GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax,
                                  double *pdfMean, double *padfStdDev )
{
    int bForceCoalesced = bForce;
    // If file version is smaller than 3, there will be no statistics in the file. But if it is version 3 or higher we don't want underlying implementation to compute histogram
    // so we set bForceCoalesced to FALSE.
    if (poGDS->psFileInfo->nFormatVersion >= 3) {
        bForceCoalesced = FALSE;
    }
    // We check if we have PAM histogram. If we have them we return them. This will allow to override statistics stored in the file.
    CPLErr pamError =  GDALPamRasterBand::GetStatistics(bApproxOK, bForceCoalesced, pdfMin, pdfMax, pdfMean, padfStdDev);
    if ( pamError == CE_None || poGDS->psFileInfo->nFormatVersion<3 || eBandInterp == GCI_AlphaBand){
        return pamError;
    }

    NCS::CError error = poGDS->StatisticsEnsureInitialized();
    if (!error.Success()){
        CPLError( CE_Failure, CPLE_AppDefined,
                        "ECWRDataset::StatisticsEnsureInitialized failed in ECWRasterBand::GetStatistic. " );
        return CE_Failure;
    }
    GetBandIndexAndCountForStatistics(nStatsBandIndex, nStatsBandCount);
    bool bStatisticsFromFile = false;

    if ( poGDS->pStatistics != nullptr )
    {
        bStatisticsFromFile = true;
        NCSBandStats& bandStats = poGDS->pStatistics->BandsStats[nStatsBandIndex];
        if ( pdfMin != nullptr && !std::isnan(bandStats.fMinVal)) {
            *pdfMin = bandStats.fMinVal;
        }else{
            bStatisticsFromFile = false;
        }
        if ( pdfMax != nullptr && !std::isnan(bandStats.fMaxVal)) {
            *pdfMax = bandStats.fMaxVal;
        }else{
            bStatisticsFromFile = false;
        }
        if ( pdfMean != nullptr && !std::isnan(bandStats.fMeanVal)) {
            *pdfMean = bandStats.fMeanVal;
        }else{
            bStatisticsFromFile = false;
        }
        if ( padfStdDev != nullptr && !std::isnan(bandStats.fStandardDev)) {
            *padfStdDev  = bandStats.fStandardDev;
        }else{
            bStatisticsFromFile = false;
        }
        if (bStatisticsFromFile) return CE_None;
    }
    //no required statistics.
    if (!bStatisticsFromFile && bForce == TRUE){
        double dfMin, dfMax, dfMean,dfStdDev;
        pamError = GDALPamRasterBand::GetStatistics(bApproxOK, TRUE,
            &dfMin,
            &dfMax,
            &dfMean,
            &dfStdDev);
        if (pdfMin!=nullptr) {
            *pdfMin = dfMin;
        }
        if (pdfMax !=nullptr){
            *pdfMax = dfMax;
        }
        if (pdfMean !=nullptr){
            *pdfMean = dfMean;
        }
        if (padfStdDev!=nullptr){
            *padfStdDev = dfStdDev;
        }
        if ( pamError == CE_None){
            const CPLErr err = SetStatistics(dfMin,dfMax,dfMean,dfStdDev);
            if (err !=CE_None){
                CPLError (CE_Warning, CPLE_AppDefined,
                    "SetStatistics failed in ECWRasterBand::GetDefaultHistogram. Statistics might not be saved in .ecw file." );
            }
            return CE_None;
        }
        //whatever happened we return.
        return pamError;
    }
    //no statistics and we are not forced to return.
    return CE_Warning;
}

/************************************************************************/
/*                          SetStatistics()                             */
/************************************************************************/

CPLErr ECWRasterBand::SetStatistics( double dfMin, double dfMax,
                                  double dfMean, double dfStdDev ){
    if (poGDS->psFileInfo->nFormatVersion < 3 || eBandInterp == GCI_AlphaBand){
        return GDALPamRasterBand::SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
    }
    double dummy;
    bool hasPAMStatistics = GDALPamRasterBand::GetStatistics(TRUE, FALSE, &dummy, &dummy, &dummy, &dummy) == CE_None;

    NCS::CError error = poGDS->StatisticsEnsureInitialized();
    if (!error.Success()){
            CPLError( CE_Warning, CPLE_AppDefined,
                        "ECWRDataset::StatisticsEnsureInitialized failed in ECWRasterBand::SetStatistic. Statistics will be written to PAM. " );
            return GDALPamRasterBand::SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
    }
    GetBandIndexAndCountForStatistics(nStatsBandIndex, nStatsBandCount);
    if (poGDS->pStatistics == nullptr){
        error = NCSEcwInitStatistics(&poGDS->pStatistics, nStatsBandCount, nullptr);
        if (!error.Success()){
            CPLError( CE_Warning, CPLE_AppDefined,
                        "NCSEcwInitStatistics failed in ECWRasterBand::SetStatistic. Statistics will be written to PAM." );
            return GDALPamRasterBand::SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
        }
    }

    poGDS->pStatistics->BandsStats[nStatsBandIndex].fMinVal = static_cast<IEEE4>(dfMin);
    poGDS->pStatistics->BandsStats[nStatsBandIndex].fMaxVal = static_cast<IEEE4>(dfMax);
    poGDS->pStatistics->BandsStats[nStatsBandIndex].fMeanVal = static_cast<IEEE4>(dfMean);
    poGDS->pStatistics->BandsStats[nStatsBandIndex].fStandardDev = static_cast<IEEE4>(dfStdDev);
    poGDS->bStatisticsDirty = TRUE;
    //if we have PAM statistics we need to save them as well. Better option would be to remove them from PAM file but I don't know how to do that without messing in PAM internals.
    if ( hasPAMStatistics ){
        CPLError( CE_Debug, CPLE_AppDefined,
                        "PAM statistics will be overwritten." );
        return  GDALPamRasterBand::SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
    }

    return CE_None;
}
#endif

//#if !defined(SDK_CAN_DO_SUPERSAMPLING)
/************************************************************************/
/*                          OldIRasterIO()                              */
/************************************************************************/

/* This implementation of IRasterIO(), derived from the one of GDAL 1.9 */
/* and older versions, is meant at making over-sampling */
/* work with ECW SDK 3.3. Newer versions of the SDK can do super-sampling in their */
/* SetView() call. */

CPLErr ECWRasterBand::OldIRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    int          iBand;
    GByte        *pabyWorkBuffer = nullptr;
    const int nResFactor = 1 << (iOverview+1);

    nXOff *= nResFactor;
    nYOff *= nResFactor;
    nXSize *= nResFactor;
    nYSize *= nResFactor;

/* -------------------------------------------------------------------- */
/*      Try to do it based on existing "advised" access.                */
/* -------------------------------------------------------------------- */
    int nRet = poGDS->TryWinRasterIO( eRWFlag,
                               nXOff, nYOff,
                               nXSize, nYSize,
                               static_cast<GByte *>(pData), nBufXSize, nBufYSize,
                               eBufType, 1, &nBand,
                               nPixelSpace, nLineSpace, 0 , psExtraArg);
    if( nRet == TRUE )
        return CE_None;
    if( nRet < 0 )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      The ECW SDK doesn't supersample, so adjust for this case.       */
/* -------------------------------------------------------------------- */

    int nNewXSize = nBufXSize;
    int nNewYSize = nBufYSize;

    if ( nXSize < nBufXSize )
        nNewXSize = nXSize;

    if ( nYSize < nBufYSize )
        nNewYSize = nYSize;

/* -------------------------------------------------------------------- */
/*      Can we perform direct loads, or must we load into a working     */
/*      buffer, and transform?                                          */
/* -------------------------------------------------------------------- */
    const int     nRawPixelSize = GDALGetDataTypeSize(poGDS->eRasterDataType) / 8;

    int bDirect = nPixelSpace == 1 && eBufType == GDT_Byte
        && nNewXSize == nBufXSize && nNewYSize == nBufYSize;
    if( !bDirect )
        pabyWorkBuffer = static_cast<GByte *>(CPLMalloc(nNewXSize * nRawPixelSize));

/* -------------------------------------------------------------------- */
/*      Establish access at the desired resolution.                     */
/* -------------------------------------------------------------------- */
    poGDS->CleanupWindow();

    iBand = nBand-1;
    poGDS->nBandIndexToPromoteTo8Bit = ( bPromoteTo8Bit ) ? 0 : -1;
    // TODO: Fix writable strings issue.
    CNCSError oErr = poGDS->poFileView->SetView( 1, reinterpret_cast<unsigned int *>(&iBand),
                                                 nXOff, nYOff,
                                                 nXOff + nXSize - 1,
                                                 nYOff + nYSize - 1,
                                                 nNewXSize, nNewYSize );
    if( oErr.GetErrorNumber() != NCS_SUCCESS )
    {
        CPLFree( pabyWorkBuffer );
        ECWReportError(oErr);

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read back one scanline at a time, till request is satisfied.    */
/*      Supersampling is not supported by the ECW API, so we will do    */
/*      it ourselves.                                                   */
/* -------------------------------------------------------------------- */
    double  dfSrcYInc = static_cast<double>(nNewYSize) / nBufYSize;
    double  dfSrcXInc = static_cast<double>(nNewXSize) / nBufXSize;
    int         iSrcLine, iDstLine;
    CPLErr eErr = CE_None;

    for( iSrcLine = 0, iDstLine = 0; iDstLine < nBufYSize; iDstLine++ )
    {
        NCSEcwReadStatus eRStatus;
        GPtrDiff_t       iDstLineOff = iDstLine * (GPtrDiff_t)nLineSpace;
        unsigned char   *pabySrcBuf;

        if( bDirect )
            pabySrcBuf = ((GByte *)pData) + iDstLineOff;
        else
            pabySrcBuf = pabyWorkBuffer;

        if ( nNewYSize == nBufYSize || iSrcLine == (int)(iDstLine * dfSrcYInc) )
        {
            eRStatus = poGDS->poFileView->ReadLineBIL(
                poGDS->eNCSRequestDataType, 1, (void **) &pabySrcBuf );

            if( eRStatus != NCSECW_READ_OK )
            {
                CPLDebug( "ECW", "ReadLineBIL status=%d", (int) eRStatus );
                CPLError( CE_Failure, CPLE_AppDefined,
                         "NCScbmReadViewLineBIL failed." );
                eErr = CE_Failure;
                break;
            }

            if( bPromoteTo8Bit )
            {
                for ( int iX = 0; iX < nNewXSize; iX++ )
                {
                    pabySrcBuf[iX] *= 255;
                }
            }

            if( !bDirect )
            {
                if ( nNewXSize == nBufXSize )
                {
                    GDALCopyWords( pabyWorkBuffer, poGDS->eRasterDataType,
                                nRawPixelSize,
                                ((GByte *)pData) + iDstLine * nLineSpace,
                                eBufType, (int)nPixelSpace, nBufXSize );
                }
                else
                {
                    int iPixel;

                    for ( iPixel = 0; iPixel < nBufXSize; iPixel++ )
                    {
                        GDALCopyWords( pabyWorkBuffer
                                    + nRawPixelSize*((int)(iPixel*dfSrcXInc)),
                                    poGDS->eRasterDataType, nRawPixelSize,
                                    (GByte *)pData + iDstLineOff
                                    + iPixel * nPixelSpace,
                                                    eBufType, (int)nPixelSpace, 1 );
                    }
                }
            }

            iSrcLine++;
        }
        else
        {
            // Just copy the previous line in this case
            GDALCopyWords( (GByte *)pData + (iDstLineOff - nLineSpace),
                            eBufType, (int)nPixelSpace,
                            (GByte *)pData + iDstLineOff,
                            eBufType, (int)nPixelSpace, nBufXSize );
        }

        if( psExtraArg->pfnProgress != nullptr &&
                    !psExtraArg->pfnProgress(1.0 * (iDstLine + 1) / nBufYSize, "",
                                             psExtraArg->pProgressData) )
        {
            eErr = CE_Failure;
            break;
        }
    }

    CPLFree( pabyWorkBuffer );

    return eErr;
}
//#endif !defined(SDK_CAN_DO_SUPERSAMPLING)

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr ECWRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )
{
    if( eRWFlag == GF_Write )
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Default line and pixel spacing if needed.                       */
    /* -------------------------------------------------------------------- */
    if ( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize( eBufType ) / 8;

    if ( nLineSpace == 0 )
        nLineSpace = nPixelSpace * nBufXSize;

    CPLDebug( "ECWRasterBand",
            "RasterIO(nBand=%d,iOverview=%d,nXOff=%d,nYOff=%d,nXSize=%d,nYSize=%d -> %dx%d)",
            nBand, iOverview, nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );

#if !defined(SDK_CAN_DO_SUPERSAMPLING)
    if( poGDS->bUseOldBandRasterIOImplementation )
    {
        return OldIRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                            pData, nBufXSize, nBufYSize,
                            eBufType,
                            nPixelSpace, nLineSpace, psExtraArg );
    }

#endif

    int nResFactor = 1 << (iOverview+1);

    return poGDS->IRasterIO(eRWFlag,
                            nXOff * nResFactor,
                            nYOff * nResFactor,
                            (nXSize == nRasterXSize) ? poGDS->nRasterXSize : nXSize * nResFactor,
                            (nYSize == nRasterYSize) ? poGDS->nRasterYSize : nYSize * nResFactor,
                            pData, nBufXSize, nBufYSize,
                            eBufType, 1, &nBand,
                            nPixelSpace, nLineSpace, nLineSpace*nBufYSize, psExtraArg);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ECWRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff, void * pImage )

{
    int nXOff = nBlockXOff * nBlockXSize,
        nYOff = nBlockYOff * nBlockYSize,
        nXSize = nBlockXSize,
        nYSize = nBlockYSize;

    if( nXOff + nXSize > nRasterXSize )
        nXSize = nRasterXSize - nXOff;
    if( nYOff + nYSize > nRasterYSize )
        nYSize = nRasterYSize - nYOff;

    int nPixelSpace = GDALGetDataTypeSize(eDataType) / 8;
    int nLineSpace = nPixelSpace * nBlockXSize;

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    return IRasterIO( GF_Read,
                      nXOff, nYOff, nXSize, nYSize,
                      pImage, nXSize, nYSize,
                      eDataType, nPixelSpace, nLineSpace, &sExtraArg );
}

/************************************************************************/
/* ==================================================================== */
/*                            ECWDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ECWDataset()                              */
/************************************************************************/

ECWDataset::ECWDataset(int bIsJPEG2000In)

{
    this->bIsJPEG2000 = bIsJPEG2000In;
    bUsingCustomStream = FALSE;
    poFileView = nullptr;
    bWinActive = FALSE;
    panWinBandList = nullptr;
    eRasterDataType = GDT_Byte;
    papszGMLMetadata = nullptr;

    bHdrDirty = FALSE;
    bGeoTransformChanged = FALSE;
    bProjectionChanged = FALSE;
    bProjCodeChanged = FALSE;
    bDatumCodeChanged = FALSE;
    bUnitsCodeChanged = FALSE;

    bUseOldBandRasterIOImplementation = FALSE;
#if ECWSDK_VERSION>=50

    pStatistics = nullptr;
    bStatisticsDirty = FALSE;
    bStatisticsInitialized = FALSE;
    bFileMetaDataDirty = FALSE;

#endif

    sCachedMultiBandIO.bEnabled = FALSE;
    sCachedMultiBandIO.nBandsTried = 0;
    sCachedMultiBandIO.nXOff = 0;
    sCachedMultiBandIO.nYOff = 0;
    sCachedMultiBandIO.nXSize = 0;
    sCachedMultiBandIO.nYSize = 0;
    sCachedMultiBandIO.nBufXSize = 0;
    sCachedMultiBandIO.nBufYSize = 0;
    sCachedMultiBandIO.eBufType = GDT_Unknown;
    sCachedMultiBandIO.pabyData = nullptr;

    bPreventCopyingSomeMetadata = FALSE;

    nBandIndexToPromoteTo8Bit = -1;

    poDriver = (GDALDriver*) GDALGetDriverByName( bIsJPEG2000 ? "JP2ECW" : "ECW" );

    psFileInfo =  nullptr;
    eNCSRequestDataType = NCSCT_UINT8;
    nWinXOff = 0;
    nWinYOff = 0;
    nWinXSize = 0;
    nWinYSize = 0;
    nWinBufXSize = 0;
    nWinBufYSize = 0;
    nWinBandCount = 0;
    nWinBufLoaded = FALSE;
    papCurLineBuf = nullptr;

    m_nAdviseReadXOff = -1;
    m_nAdviseReadYOff = -1;
    m_nAdviseReadXSize = -1;
    m_nAdviseReadYSize = -1;
    m_nAdviseReadBufXSize = -1;
    m_nAdviseReadBufYSize = -1;
    m_nAdviseReadBandCount = -1;
    m_panAdviseReadBandList = nullptr;
}

/************************************************************************/
/*                           ~ECWDataset()                              */
/************************************************************************/

ECWDataset::~ECWDataset()

{
    GDALPamDataset::FlushCache();
    CleanupWindow();

#if ECWSDK_VERSION>=50
    NCSFileMetaData* pFileMetaDataCopy = nullptr;
    if( bFileMetaDataDirty )
    {
        NCSCopyMetaData(&pFileMetaDataCopy, psFileInfo->pFileMetaData);
    }
#endif

/* -------------------------------------------------------------------- */
/*      Release / dereference iostream.                                 */
/* -------------------------------------------------------------------- */
    // The underlying iostream of the CNCSJP2FileView (poFileView) object may
    // also be the underlying iostream of other CNCSJP2FileView (poFileView)
    // objects.  Consequently, when we delete the CNCSJP2FileView (poFileView)
    // object, we must decrement the nFileViewCount attribute of the underlying
    // VSIIOStream object, and only delete the VSIIOStream object when
    // nFileViewCount is equal to zero.

    CPLMutexHolder oHolder( &hECWDatasetMutex );

    // bInGDALGlobalDestructor is set to TRUE by gdaldllmain.cpp/GDALDestroy() so as
    // to avoid an issue with the ECW SDK 3.3 where the destructor of CNCSJP2File::CNCSJP2FileVector CNCSJP2File::sm_Files;
    // static resource allocated in NCJP2File.cpp can be called before GDALDestroy(), causing
    // ECW SDK resources ( CNCSJP2File files ) to be closed before we get here.
    //
    // We also have an issue with ECW SDK 5.0 and ECW files on Linux when
    // running a multi-threaded test under Java if there's still an ECW dataset
    // not explicitly closed at process termination.
    /*  #0  0x00007fffb26e7a80 in NCSAtomicAdd64 () from /home/even/ecwjp2_sdk/redistributable/x64/libNCSEcw.so
        #1  0x00007fffb2aa7684 in NCS::SDK::CBuffer2D::Free() () from /home/even/ecwjp2_sdk/redistributable/x64/libNCSEcw.so
        #2  0x00007fffb2aa7727 in NCS::SDK::CBuffer2D::~CBuffer2D() () from /home/even/ecwjp2_sdk/redistributable/x64/libNCSEcw.so
        #3  0x00007fffb29aa7be in NCS::ECW::CReader::~CReader() () from /home/even/ecwjp2_sdk/redistributable/x64/libNCSEcw.so
        #4  0x00007fffb29aa819 in NCS::ECW::CReader::~CReader() () from /home/even/ecwjp2_sdk/redistributable/x64/libNCSEcw.so
        #5  0x00007fffb291fd3a in NCS::CView::Close(bool) () from /home/even/ecwjp2_sdk/redistributable/x64/libNCSEcw.so
        #6  0x00007fffb2927529 in NCS::CView::~CView() () from /home/even/ecwjp2_sdk/redistributable/x64/libNCSEcw.so
        #7  0x00007fffb29277f9 in NCS::CView::~CView() () from /home/even/ecwjp2_sdk/redistributable/x64/libNCSEcw.so
        #8  0x00007fffb71a9a53 in ECWDataset::~ECWDataset (this=0x7fff942cce10, __in_chrg=<optimized out>) at ecwdataset.cpp:1003
        #9  0x00007fffb71a9cca in ECWDataset::~ECWDataset (this=0x7fff942cce10, __in_chrg=<optimized out>) at ecwdataset.cpp:1039
        #10 0x00007fffb7551f98 in GDALDriverManager::~GDALDriverManager (this=0x7ffff01981a0, __in_chrg=<optimized out>) at gdaldrivermanager.cpp:196
        #11 0x00007fffb7552140 in GDALDriverManager::~GDALDriverManager (this=0x7ffff01981a0, __in_chrg=<optimized out>) at gdaldrivermanager.cpp:288
        #12 0x00007fffb7552e18 in GDALDestroyDriverManager () at gdaldrivermanager.cpp:824
        #13 0x00007fffb7551c61 in GDALDestroy () at gdaldllmain.cpp:80
        #14 0x00007ffff7de990e in _dl_fini () at dl-fini.c:254
    */
    // Not reproducible with similar test in C++, but this might be
    // just a matter of luck related to the order in which the
    // libraries are unloaded, so just don't try to delete poFileView
    // from the GDAL destructor.
    if( poFileView != nullptr && !GDALIsInGlobalDestructor() )
    {
#if ECWSDK_VERSION >= 55
        delete poFileView;
#else
        VSIIOStream *poUnderlyingIOStream = (VSIIOStream *)nullptr;

        if( bUsingCustomStream )
        {
            poUnderlyingIOStream = ((VSIIOStream *)(poFileView->GetStream()));
        }
        delete poFileView;

        if( bUsingCustomStream )
        {
            if( --poUnderlyingIOStream->nFileViewCount == 0 )
                delete poUnderlyingIOStream;
        }
#endif
        poFileView = nullptr;
    }

    /* WriteHeader() must be called after closing the file handle to work */
    /* on Windows */
    if( bHdrDirty )
        WriteHeader();
#if ECWSDK_VERSION>=50
    if (bStatisticsDirty){
        StatisticsWrite();
    }
    CleanupStatistics();

    if( bFileMetaDataDirty )
    {
        WriteFileMetaData(pFileMetaDataCopy);
        NCSFreeMetaData(pFileMetaDataCopy);
    }
#endif

    CSLDestroy( papszGMLMetadata );

    CPLFree(sCachedMultiBandIO.pabyData);

    CPLFree(m_panAdviseReadBandList);
}

#if ECWSDK_VERSION>=50

/************************************************************************/
/*                    StatisticsEnsureInitialized()                     */
/************************************************************************/

NCS::CError ECWDataset::StatisticsEnsureInitialized(){
    if (bStatisticsInitialized == TRUE){
        return NCS_SUCCESS;
    }

    NCS::CError error = poFileView->GetClientStatistics(&pStatistics);
    if (error.Success()){
        bStatisticsInitialized = TRUE;
    }
    return error;
}

/************************************************************************/
/*                          StatisticsWrite()                           */
/************************************************************************/

NCS::CError ECWDataset::StatisticsWrite()
{
    CPLDebug("ECW", "In StatisticsWrite()");
    NCSFileView* view = NCSEcwEditOpen( GetDescription() );
    NCS::CError error;
    if ( view != nullptr ){
        error = NCSEcwEditSetStatistics(view, pStatistics);
        if (error.Success()){
            error = NCSEcwEditFlushAll(view);
            if (error.Success()){
                error = NCSEcwEditClose(view);
            }
        }
    }

    bStatisticsDirty = FALSE;

    return error;
}

/************************************************************************/
/*                          CleanupStatistics()                         */
/************************************************************************/

void ECWDataset::CleanupStatistics(){
    if (bStatisticsInitialized == TRUE && pStatistics !=nullptr){
        NCSEcwFreeStatistics(pStatistics);
    }
}

#endif // #if ECWSDK_VERSION>=50

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ECWDataset::SetGeoTransform( double * padfGeoTransform )
{
    if ( bIsJPEG2000 || eAccess == GA_ReadOnly )
        return GDALPamDataset::SetGeoTransform(padfGeoTransform);

    if ( !bGeoTransformValid ||
        adfGeoTransform[0] != padfGeoTransform[0] ||
        adfGeoTransform[1] != padfGeoTransform[1] ||
        adfGeoTransform[2] != padfGeoTransform[2] ||
        adfGeoTransform[3] != padfGeoTransform[3] ||
        adfGeoTransform[4] != padfGeoTransform[4] ||
        adfGeoTransform[5] != padfGeoTransform[5] )
    {
        memcpy(adfGeoTransform, padfGeoTransform, 6 * sizeof(double));
        bGeoTransformValid = TRUE;
        bHdrDirty = TRUE;
        bGeoTransformChanged = TRUE;
    }

    return CE_None;
}

/************************************************************************/
/*                            SetProjection()                           */
/************************************************************************/

CPLErr ECWDataset::_SetProjection( const char* pszProjectionIn )
{
    if ( bIsJPEG2000 || eAccess == GA_ReadOnly )
        return GDALPamDataset::_SetProjection(pszProjectionIn);

    if ( !( (pszProjection == nullptr && pszProjectionIn == nullptr) ||
            (pszProjection != nullptr && pszProjectionIn != nullptr &&
             strcmp(pszProjection, pszProjectionIn) == 0) ) )
    {
        CPLFree(pszProjection);
        pszProjection = pszProjectionIn ? CPLStrdup(pszProjectionIn) : nullptr;
        bHdrDirty = TRUE;
        bProjectionChanged = TRUE;
    }

    return CE_None;
}

/************************************************************************/
/*                            SetMetadataItem()                         */
/************************************************************************/

CPLErr ECWDataset::SetMetadataItem( const char * pszName,
                                    const char * pszValue,
                                    const char * pszDomain )
{
    if ( !bIsJPEG2000 && eAccess == GA_Update &&
         (pszDomain == nullptr || EQUAL(pszDomain, "") ||
          (pszDomain != nullptr && EQUAL(pszDomain, "ECW"))) &&
         pszName != nullptr &&
         (strcmp(pszName, "PROJ") == 0 || strcmp( pszName, "DATUM") == 0 ||
          strcmp( pszName, "UNITS") == 0 ) )
    {
        CPLString osNewVal = pszValue ? pszValue : "";
        if (osNewVal.size() > 31)
            osNewVal.resize(31);
        if (strcmp(pszName, "PROJ") == 0)
        {
            bProjCodeChanged = (osNewVal != m_osProjCode);
            m_osProjCode = osNewVal;
            bHdrDirty |= bProjCodeChanged;
        }
        else if (strcmp( pszName, "DATUM") == 0)
        {
            bDatumCodeChanged |= (osNewVal != m_osDatumCode)? TRUE:FALSE ;
            m_osDatumCode = osNewVal;
            bHdrDirty |= bDatumCodeChanged;
        }
        else
        {
            bUnitsCodeChanged |= (osNewVal != m_osUnitsCode)?TRUE:FALSE;
            m_osUnitsCode = osNewVal;
            bHdrDirty |= bUnitsCodeChanged;
        }
        return CE_None;
    }
#if ECWSDK_VERSION >=50
    else if ( psFileInfo != nullptr &&
              psFileInfo->nFormatVersion >= 3 &&
              eAccess == GA_Update &&
              (pszDomain == nullptr || EQUAL(pszDomain, "")) &&
              pszName != nullptr &&
              STARTS_WITH(pszName, "FILE_METADATA_") )
    {
        bFileMetaDataDirty = TRUE;

        if( psFileInfo->pFileMetaData == nullptr )
            NCSInitMetaData(&(psFileInfo->pFileMetaData));

        if( strcmp(pszName, "FILE_METADATA_CLASSIFICATION") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sClassification);
            psFileInfo->pFileMetaData->sClassification = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_ACQUISITION_DATE") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sAcquisitionDate);
            psFileInfo->pFileMetaData->sAcquisitionDate = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_ACQUISITION_SENSOR_NAME") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sAcquisitionSensorName);
            psFileInfo->pFileMetaData->sAcquisitionSensorName = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_COMPRESSION_SOFTWARE") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sCompressionSoftware);
            psFileInfo->pFileMetaData->sCompressionSoftware = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_AUTHOR") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sAuthor);
            psFileInfo->pFileMetaData->sAuthor = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_COPYRIGHT") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sCopyright);
            psFileInfo->pFileMetaData->sCopyright = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_COMPANY") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sCompany);
            psFileInfo->pFileMetaData->sCompany = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_EMAIL") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sEmail);
            psFileInfo->pFileMetaData->sEmail = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_ADDRESS") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sAddress);
            psFileInfo->pFileMetaData->sAddress = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else if( strcmp(pszName, "FILE_METADATA_TELEPHONE") == 0 )
        {
            NCSFree(psFileInfo->pFileMetaData->sTelephone);
            psFileInfo->pFileMetaData->sTelephone = pszValue ? NCSStrDupT(NCS::CString(pszValue).c_str()) : nullptr;
            return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
        }
        else
        {
            return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
        }
    }
#endif
    else
        return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                              SetMetadata()                           */
/************************************************************************/

CPLErr ECWDataset::SetMetadata( char ** papszMetadata,
                                const char * pszDomain )
{
    /* The bPreventCopyingSomeMetadata is set by ECWCreateCopy() */
    /* just before calling poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT ); */
    if( bPreventCopyingSomeMetadata && (pszDomain == nullptr || EQUAL(pszDomain, "")) )
    {
        char** papszMetadataDup = nullptr;
        char** papszIter = papszMetadata;
        while( *papszIter )
        {
            char* pszKey = nullptr;
            CPLParseNameValue(*papszIter, &pszKey);
            /* Remove a few metadata item from the source that we don't want in */
            /* the target metadata */
            if( pszKey != nullptr && (
                    EQUAL(pszKey, "VERSION") ||
                    EQUAL(pszKey, "COMPRESSION_RATE_TARGET") ||
                    EQUAL(pszKey, "COMPRESSION_RATE_ACTUAL") ||
                    EQUAL(pszKey, "CLOCKWISE_ROTATION_DEG") ||
                    EQUAL(pszKey, "COLORSPACE") ||
                    EQUAL(pszKey, "COMPRESSION_DATE") ||
                    STARTS_WITH_CI(pszKey, "FILE_METADATA_") ) )
            {
                /* do nothing */
            }
            else
            {
                papszMetadataDup = CSLAddString(papszMetadataDup, *papszIter);
            }
            CPLFree(pszKey);
            papszIter ++;
        }

       bPreventCopyingSomeMetadata = FALSE;
       CPLErr eErr = SetMetadata(papszMetadataDup, pszDomain);
       bPreventCopyingSomeMetadata = TRUE;
       CSLDestroy(papszMetadataDup);
       return eErr;
    }

    if ( ((pszDomain == nullptr || EQUAL(pszDomain, "") || EQUAL(pszDomain, "ECW")) &&
          (CSLFetchNameValue(papszMetadata, "PROJ") != nullptr ||
           CSLFetchNameValue(papszMetadata, "DATUM") != nullptr ||
           CSLFetchNameValue(papszMetadata, "UNITS") != nullptr))
#if ECWSDK_VERSION >=50
       || (psFileInfo != nullptr &&
           psFileInfo->nFormatVersion >= 3 &&
           eAccess == GA_Update &&
           (pszDomain == nullptr || EQUAL(pszDomain, "")) &&
           (CSLFetchNameValue(papszMetadata, "FILE_METADATA_CLASSIFICATION") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_ACQUISITION_DATE") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_ACQUISITION_SENSOR_NAME") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_COMPRESSION_SOFTWARE") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_AUTHOR") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_COPYRIGHT") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_COMPANY") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_EMAIL") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_ADDRESS") != nullptr ||
            CSLFetchNameValue(papszMetadata, "FILE_METADATA_TELEPHONE") != nullptr))
#endif
        )
    {
        CPLStringList osNewMetadata;
        char** papszIter = papszMetadata;
        while(papszIter && *papszIter)
        {
            if (STARTS_WITH(*papszIter, "PROJ=") ||
                STARTS_WITH(*papszIter, "DATUM=") ||
                STARTS_WITH(*papszIter, "UNITS=") ||
                (STARTS_WITH(*papszIter, "FILE_METADATA_") && strchr(*papszIter, '=') != nullptr) )
            {
                char* pszKey = nullptr;
                const char* pszValue = CPLParseNameValue(*papszIter, &pszKey );
                SetMetadataItem(pszKey, pszValue, pszDomain);
                CPLFree(pszKey);
            }
            else
                osNewMetadata.AddString(*papszIter);
            papszIter ++;
        }
        if (!osNewMetadata.empty())
            return GDALPamDataset::SetMetadata(osNewMetadata.List(), pszDomain);
        else
            return CE_None;
    }
    else
        return GDALPamDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                             WriteHeader()                            */
/************************************************************************/

void ECWDataset::WriteHeader()
{
    if (!bHdrDirty)
        return;

    CPLAssert(eAccess == GA_Update);
    CPLAssert(!bIsJPEG2000);

    bHdrDirty = FALSE;

    NCSEcwEditInfo *psEditInfo = nullptr;
    NCSError eErr;

    /* Load original header info */
#if ECWSDK_VERSION<50
    eErr = NCSEcwEditReadInfo((char*) GetDescription(), &psEditInfo);
#else
    eErr = NCSEcwEditReadInfo(  NCS::CString::Utf8Decode(GetDescription()).c_str(), &psEditInfo);
#endif
    if (eErr != NCS_SUCCESS)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "NCSEcwEditReadInfo() failed");
        return;
    }

    /* To avoid potential cross-heap issues, we keep the original */
    /* strings, and restore them before freeing the structure */
    char* pszOriginalCode = psEditInfo->szDatum;
    char* pszOriginalProj = psEditInfo->szProjection;

    /* Alter the structure with user modified information */
    char szProjCode[32], szDatumCode[32], szUnits[32];
    if (bProjectionChanged)
    {
        if (ECWTranslateFromWKT( pszProjection, szProjCode, sizeof(szProjCode),
                                 szDatumCode, sizeof(szDatumCode), szUnits ) )
        {
            psEditInfo->szDatum = szDatumCode;
            psEditInfo->szProjection = szProjCode;
            psEditInfo->eCellSizeUnits = ECWTranslateToCellSizeUnits(szUnits);
            CPLDebug("ECW", "Rewrite DATUM : %s", psEditInfo->szDatum);
            CPLDebug("ECW", "Rewrite PROJ : %s", psEditInfo->szProjection);
            CPLDebug("ECW", "Rewrite UNITS : %s",
                     ECWTranslateFromCellSizeUnits(psEditInfo->eCellSizeUnits));
        }
    }

    if (bDatumCodeChanged)
    {
        psEditInfo->szDatum = (char*) ((m_osDatumCode.size()) ? m_osDatumCode.c_str() : "RAW");
        CPLDebug("ECW", "Rewrite DATUM : %s", psEditInfo->szDatum);
    }
    if (bProjCodeChanged)
    {
        psEditInfo->szProjection = (char*) ((m_osProjCode.size()) ? m_osProjCode.c_str() : "RAW");
        CPLDebug("ECW", "Rewrite PROJ : %s", psEditInfo->szProjection);
    }
    if (bUnitsCodeChanged)
    {
        psEditInfo->eCellSizeUnits = ECWTranslateToCellSizeUnits(m_osUnitsCode.c_str());
        CPLDebug("ECW", "Rewrite UNITS : %s",
                 ECWTranslateFromCellSizeUnits(psEditInfo->eCellSizeUnits));
    }

    if (bGeoTransformChanged)
    {
        psEditInfo->fOriginX = adfGeoTransform[0];
        psEditInfo->fCellIncrementX = adfGeoTransform[1];
        psEditInfo->fOriginY = adfGeoTransform[3];
        psEditInfo->fCellIncrementY = adfGeoTransform[5];
        CPLDebug("ECW", "Rewrite Geotransform");
    }

    /* Write modified header info */
#if ECWSDK_VERSION<50
    eErr = NCSEcwEditWriteInfo((char*) GetDescription(), psEditInfo, nullptr, nullptr, nullptr);
#else
    eErr = NCSEcwEditWriteInfo( NCS::CString::Utf8Decode(GetDescription()).c_str(), psEditInfo, nullptr, nullptr, nullptr);
#endif
    if (eErr != NCS_SUCCESS)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "NCSEcwEditWriteInfo() failed");
    }

    /* Restore original pointers before free'ing */
    psEditInfo->szDatum = pszOriginalCode;
    psEditInfo->szProjection = pszOriginalProj;

    NCSEcwEditFreeInfo(psEditInfo);
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr ECWDataset::AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize,
                               CPL_UNUSED GDALDataType eDT,
                               int nBandCount, int *panBandList,
                               CPL_UNUSED char **papszOptions )
{
    CPLDebug( "ECW",
              "ECWDataset::AdviseRead(%d,%d,%d,%d->%d,%d)",
              nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );

#if !defined(SDK_CAN_DO_SUPERSAMPLING)
    if( nBufXSize > nXSize || nBufYSize > nYSize )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Supersampling not directly supported by ECW toolkit,\n"
                  "ignoring AdviseRead() request." );
        return CE_Warning;
    }
#endif

/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */

    CPLErr eErr;
    int bStopProcessing = FALSE;
    eErr = ValidateRasterIOOrAdviseReadParameters( "AdviseRead()", &bStopProcessing,
                                                    nXOff, nYOff, nXSize, nYSize,
                                                    nBufXSize, nBufYSize,
                                                    nBandCount, panBandList);
    if( eErr != CE_None || bStopProcessing )
        return eErr;

    if( nBandCount > 100 )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                     "AdviseRead(): Too many bands : %d", nBandCount);
        return CE_Failure;
    }

    if( nBufXSize != nXSize || nBufYSize != nYSize )
    {
        // This early exit is because experimentally we found that
        // performance of requesting at 50% is much slower with
        // AdviseRead()...
        // At least on JPEG2000 images with SDK 3.3
        CPLDebug("ECW", "Ignoring AdviseRead() for non full resolution request");
        return CE_None;
    }

    // We don't setup the reading window right away, in case the actual read
    // pattern wouldn't be compatible of it. Which might be the case for
    // example if AdviseRead() requests a full image, but we don't read by
    // chunks of the full width of one or several lines 
    m_nAdviseReadXOff = nXOff;
    m_nAdviseReadYOff = nYOff;
    m_nAdviseReadXSize = nXSize;
    m_nAdviseReadYSize = nYSize;
    m_nAdviseReadBufXSize = nBufXSize;
    m_nAdviseReadBufYSize = nBufYSize;
    m_nAdviseReadBandCount = nBandCount;
    CPLFree(m_panAdviseReadBandList);
    if( panBandList )
    {
        m_panAdviseReadBandList =
            static_cast<int*>(CPLMalloc(sizeof(int) * nBandCount));
        memcpy(m_panAdviseReadBandList, panBandList, sizeof(int) * nBandCount);
    }
    else
    {
        m_panAdviseReadBandList = nullptr;
    }

    return CE_None;
}

/************************************************************************/
/*                        RunDeferredAdviseRead()                        */
/************************************************************************/

CPLErr ECWDataset::RunDeferredAdviseRead()
{
    CPLAssert(m_nAdviseReadXOff >= 0);

    const int nXOff = m_nAdviseReadXOff;
    const int nYOff = m_nAdviseReadYOff;
    const int nXSize = m_nAdviseReadXSize;
    const int nYSize = m_nAdviseReadYSize;
    const int nBufXSize = m_nAdviseReadBufXSize;
    const int nBufYSize = m_nAdviseReadBufYSize;
    const int nBandCount = m_nAdviseReadBandCount;
    int* panBandList = m_panAdviseReadBandList;

    m_nAdviseReadXOff = -1;
    m_nAdviseReadYOff = -1;
    m_nAdviseReadXSize = -1;
    m_nAdviseReadYSize = -1;
    m_nAdviseReadBufXSize = -1;
    m_nAdviseReadBufYSize = -1;
    m_nAdviseReadBandCount = -1;
    m_panAdviseReadBandList = nullptr;

/* -------------------------------------------------------------------- */
/*      Adjust band numbers to be zero based.                           */
/* -------------------------------------------------------------------- */
    int* panAdjustedBandList = (int *)
        CPLMalloc(sizeof(int) * nBandCount );
    nBandIndexToPromoteTo8Bit = -1;
    for( int ii= 0; ii < nBandCount; ii++ )
    {
        panAdjustedBandList[ii] = (panBandList != nullptr) ? panBandList[ii] - 1 : ii;
        if( ((ECWRasterBand*)GetRasterBand(panAdjustedBandList[ii] + 1))->bPromoteTo8Bit )
            nBandIndexToPromoteTo8Bit = ii;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup old window cache information.                           */
/* -------------------------------------------------------------------- */
    CleanupWindow();

/* -------------------------------------------------------------------- */
/*      Set the new requested window.                                   */
/* -------------------------------------------------------------------- */
    CNCSError oErr = poFileView->SetView( nBandCount, (UINT32 *) panAdjustedBandList,
                                nXOff, nYOff,
                                nXOff + nXSize-1, nYOff + nYSize-1,
                                nBufXSize, nBufYSize );

    CPLFree( panAdjustedBandList );
    if( oErr.GetErrorNumber() != NCS_SUCCESS )
    {
        ECWReportError(oErr);

        bWinActive = FALSE;
        CPLFree( panBandList );
        return CE_Failure;
    }

    bWinActive = TRUE;

/* -------------------------------------------------------------------- */
/*      Record selected window.                                         */
/* -------------------------------------------------------------------- */
    nWinXOff = nXOff;
    nWinYOff = nYOff;
    nWinXSize = nXSize;
    nWinYSize = nYSize;
    nWinBufXSize = nBufXSize;
    nWinBufYSize = nBufYSize;

    panWinBandList = (int *) CPLMalloc(sizeof(int)*nBandCount);
    if( panBandList != nullptr )
        memcpy( panWinBandList, panBandList, sizeof(int)* nBandCount);
    else
    {
        for( int ii= 0; ii < nBandCount; ii++ )
        {
            panWinBandList[ii] = ii + 1;
        }
    }
    nWinBandCount = nBandCount;

    nWinBufLoaded = -1;

/* -------------------------------------------------------------------- */
/*      Allocate current scanline buffer.                               */
/* -------------------------------------------------------------------- */
    papCurLineBuf = (void **) CPLMalloc(sizeof(void*) * nWinBandCount );
    for( int iBand = 0; iBand < nWinBandCount; iBand++ )
        papCurLineBuf[iBand] =
            CPLMalloc(nBufXSize * (GDALGetDataTypeSize(eRasterDataType)/8) );

    CPLFree( panBandList );

    return CE_None;
}

/************************************************************************/
/*                           TryWinRasterIO()                           */
/*                                                                      */
/*      Try to satisfy the given request based on the currently         */
/*      defined window.  Return TRUE on success or FALSE on             */
/*      failure.  On failure, the caller should satisfy the request     */
/*      another way (not report an error).                              */
/************************************************************************/

int ECWDataset::TryWinRasterIO( CPL_UNUSED GDALRWFlag eFlag,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                GByte *pabyData, int nBufXSize, int nBufYSize,
                                GDALDataType eDT,
                                int nBandCount, int *panBandList,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GSpacing nBandSpace,
                                GDALRasterIOExtraArg* psExtraArg )
{
    int iBand, i;

/* -------------------------------------------------------------------- */
/*      Provide default buffer organization.                            */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize( eDT ) / 8;
    if( nLineSpace == 0 )
        nLineSpace = nPixelSpace * nBufXSize;
    if( nBandSpace == 0 )
        nBandSpace = nLineSpace * nBufYSize;

/* -------------------------------------------------------------------- */
/*      Do some simple tests to see if the current window can           */
/*      satisfy our requirement.                                        */
/* -------------------------------------------------------------------- */
#ifdef NOISY_DEBUG
    CPLDebug( "ECW", "TryWinRasterIO(%d,%d,%d,%d,%d,%d)",
              nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );
#endif

    if( !bWinActive )
    {
        if( nXOff == m_nAdviseReadXOff && nXSize == m_nAdviseReadXSize &&
            nBufXSize == m_nAdviseReadBufXSize )
        {
            if( RunDeferredAdviseRead() != CE_None )
                return FALSE;
        }
        if( !bWinActive )
        {
            return FALSE;
        }
    }

    if( nXOff != nWinXOff || nXSize != nWinXSize )
        return FALSE;

    if( nBufXSize != nWinBufXSize )
        return FALSE;

    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        for( i = 0; i < nWinBandCount; i++ )
        {
            if( panWinBandList[i] == panBandList[iBand] )
                break;
        }

        if( i == nWinBandCount )
            return FALSE;
    }

    if( nYOff < nWinYOff || nYOff + nYSize > nWinYOff + nWinYSize )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Now we try more subtle tests.                                   */
/* -------------------------------------------------------------------- */
    {
        static int nDebugCount = 0;

        if( nDebugCount < 30 )
            CPLDebug( "ECW",
                      "TryWinRasterIO(%d,%d,%d,%d -> %dx%d) - doing advised read.",
                      nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );

        if( nDebugCount == 29 )
            CPLDebug( "ECW", "No more TryWinRasterIO messages will be reported" );

        nDebugCount++;
    }

/* -------------------------------------------------------------------- */
/*      Actually load data one buffer line at a time.                   */
/* -------------------------------------------------------------------- */
    int iBufLine;

    for( iBufLine = 0; iBufLine < nBufYSize; iBufLine++ )
    {
        double fFileLine = ((iBufLine+0.5) / nBufYSize) * nYSize + nYOff;
        int iWinLine =
            (int) (((fFileLine - nWinYOff) / nWinYSize) * nWinBufYSize);

        if( iWinLine == nWinBufLoaded + 1 )
            LoadNextLine();

        if( iWinLine != nWinBufLoaded )
            return FALSE;

/* -------------------------------------------------------------------- */
/*      Copy out all our target bands.                                  */
/* -------------------------------------------------------------------- */
        int iWinBand;
        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            for( iWinBand = 0; iWinBand < nWinBandCount; iWinBand++ )
            {
                if( panWinBandList[iWinBand] == panBandList[iBand] )
                    break;
            }

            GDALCopyWords( papCurLineBuf[iWinBand], eRasterDataType,
                           GDALGetDataTypeSize( eRasterDataType ) / 8,
                           pabyData + nBandSpace * iBand
                           + iBufLine * nLineSpace, eDT, (int)nPixelSpace,
                           nBufXSize );
        }

        if( psExtraArg->pfnProgress != nullptr &&
                    !psExtraArg->pfnProgress(1.0 * (iBufLine + 1) / nBufYSize, "",
                                             psExtraArg->pProgressData) )
        {
            return -1;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                            LoadNextLine()                            */
/************************************************************************/

CPLErr ECWDataset::LoadNextLine()

{
    if( !bWinActive )
        return CE_Failure;

    if( nWinBufLoaded == nWinBufYSize-1 )
    {
        CleanupWindow();
        return CE_Failure;
    }

    NCSEcwReadStatus  eRStatus;
    eRStatus = poFileView->ReadLineBIL( eNCSRequestDataType,
                                        (UINT16) nWinBandCount,
                                        papCurLineBuf );
    if( eRStatus != NCSECW_READ_OK )
        return CE_Failure;

    if( nBandIndexToPromoteTo8Bit >= 0 )
    {
        for(int iX = 0; iX < nWinBufXSize; iX ++ )
        {
            ((GByte*)papCurLineBuf[nBandIndexToPromoteTo8Bit])[iX] *= 255;
        }
    }

    nWinBufLoaded++;

    return CE_None;
}

/************************************************************************/
/*                           CleanupWindow()                            */
/************************************************************************/

void ECWDataset::CleanupWindow()

{
    if( !bWinActive )
        return;

    bWinActive = FALSE;
    CPLFree( panWinBandList );
    panWinBandList = nullptr;

    for( int iBand = 0; iBand < nWinBandCount; iBand++ )
        CPLFree( papCurLineBuf[iBand] );
    CPLFree( papCurLineBuf );
    papCurLineBuf = nullptr;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr ECWDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg)

{
    if( eRWFlag == GF_Write )
        return CE_Failure;

    if( nBandCount > 100 )
        return CE_Failure;

    if( bUseOldBandRasterIOImplementation )
        /* Sanity check. Should not happen */
        return CE_Failure;
    int nDataTypeSize = (GDALGetDataTypeSize(eRasterDataType) / 8);

    if ( nPixelSpace == 0 ){
        nPixelSpace = nDataTypeSize;
    }

    if (nLineSpace == 0 ) {
        nLineSpace = nPixelSpace*nBufXSize;
    }
    if ( nBandSpace == 0 ){
        nBandSpace = static_cast<GSpacing>(nDataTypeSize)*nBufXSize*nBufYSize;
    }

    // Use GDAL upsampling if non nearest
    if( (nBufXSize > nXSize || nBufYSize > nYSize) &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        int nBufDataTypeSize = (GDALGetDataTypeSize(eBufType) / 8);
        GByte* pabyTemp = (GByte*)VSI_MALLOC3_VERBOSE(nXSize, nYSize, nBufDataTypeSize * nBandCount);
        if( pabyTemp == nullptr )
        {
            return CE_Failure;
        }

        CPLErr eErr = IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                pabyTemp, nXSize, nYSize,
                                eBufType, nBandCount, panBandMap,
                                nBufDataTypeSize,
                                (GIntBig)nBufDataTypeSize* nXSize,
                                (GIntBig)nBufDataTypeSize*nXSize*nYSize,
                                psExtraArg);

        if( eErr == CE_None )
        {
            /* Create a MEM dataset that wraps the input buffer */
            GDALDataset* poMEMDS = MEMDataset::Create("", nXSize, nYSize, 0,
                                                      eBufType, nullptr);
            char szBuffer[64];
            int nRet;

            for( int i = 0; i < nBandCount; i++ )
            {
                nRet = CPLPrintPointer(szBuffer, pabyTemp + i * nBufDataTypeSize, sizeof(szBuffer));
                szBuffer[nRet] = 0;
                char** papszOptions = CSLSetNameValue(nullptr, "DATAPOINTER", szBuffer);

                papszOptions = CSLSetNameValue(papszOptions, "PIXELOFFSET",
                    CPLSPrintf(CPL_FRMT_GIB, (GIntBig)nBufDataTypeSize * nBandCount));

                papszOptions = CSLSetNameValue(papszOptions, "LINEOFFSET",
                    CPLSPrintf(CPL_FRMT_GIB, (GIntBig)nBufDataTypeSize * nBandCount * nXSize));

                poMEMDS->AddBand(eBufType, papszOptions);
                CSLDestroy(papszOptions);

                const char* pszNBITS = GetRasterBand(i+1)->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
                if( pszNBITS )
                    poMEMDS->GetRasterBand(i+1)->SetMetadataItem("NBITS", pszNBITS, "IMAGE_STRUCTURE");
            }

            GDALRasterIOExtraArg sExtraArgTmp;
            INIT_RASTERIO_EXTRA_ARG(sExtraArgTmp);
            sExtraArgTmp.eResampleAlg = psExtraArg->eResampleAlg;

            CPL_IGNORE_RET_VAL(poMEMDS->RasterIO(GF_Read, 0, 0, nXSize, nYSize,
                                pData, nBufXSize, nBufYSize,
                                eBufType,
                                nBandCount, nullptr,
                                nPixelSpace, nLineSpace, nBandSpace,
                                &sExtraArgTmp));

            GDALClose(poMEMDS);
        }

        VSIFree(pabyTemp);

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      ECW SDK 3.3 has a bug with the ECW format when we query the     */
/*      number of bands of the dataset, but not in the "natural order". */
/*      It ignores the content of panBandMap. (#4234)                   */
/* -------------------------------------------------------------------- */
#if ECWSDK_VERSION < 40
    if( !bIsJPEG2000 && nBandCount == nBands )
    {
        int i;
        int bDoBandIRasterIO = FALSE;
        for( i = 0; i < nBandCount; i++ )
        {
            if( panBandMap[i] != i + 1 )
            {
                bDoBandIRasterIO = TRUE;
            }
        }
        if( bDoBandIRasterIO )
        {
            return GDALDataset::IRasterIO(
                                    eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
        }
    }
#endif

/* -------------------------------------------------------------------- */
/*      Check if we can directly return the data in case we have cached */
/*      it from a previous call in a multi-band reading pattern.        */
/* -------------------------------------------------------------------- */
    if( nBandCount == 1 && *panBandMap > 1 && *panBandMap <= nBands &&
        sCachedMultiBandIO.nXOff == nXOff &&
        sCachedMultiBandIO.nYOff == nYOff &&
        sCachedMultiBandIO.nXSize == nXSize &&
        sCachedMultiBandIO.nYSize == nYSize &&
        sCachedMultiBandIO.nBufXSize == nBufXSize &&
        sCachedMultiBandIO.nBufYSize == nBufYSize &&
        sCachedMultiBandIO.eBufType == eBufType )
    {
        sCachedMultiBandIO.nBandsTried ++;

        if( sCachedMultiBandIO.bEnabled &&
            sCachedMultiBandIO.pabyData != nullptr )
        {
            int j;
            int nBufTypeSize = GDALGetDataTypeSize(eBufType) / 8;
            for(j = 0; j < nBufYSize; j++)
            {
                GDALCopyWords(sCachedMultiBandIO.pabyData +
                                    (*panBandMap - 1) * nBufXSize * nBufYSize * nBufTypeSize +
                                    j * nBufXSize * nBufTypeSize,
                            eBufType, nBufTypeSize,
                            ((GByte*)pData) + j * nLineSpace, eBufType, (int)nPixelSpace,
                            nBufXSize);
            }
            return CE_None;
        }

        if( !(sCachedMultiBandIO.bEnabled) &&
            sCachedMultiBandIO.nBandsTried == nBands &&
            CPLTestBool(CPLGetConfigOption("ECW_CLEVER", "YES")) )
        {
            sCachedMultiBandIO.bEnabled = TRUE;
            CPLDebug("ECW", "Detecting successive band reading pattern (for next time)");
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to do it based on existing "advised" access.                */
/* -------------------------------------------------------------------- */
    int nRet = TryWinRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                        (GByte *) pData, nBufXSize, nBufYSize,
                        eBufType, nBandCount, panBandMap,
                        nPixelSpace, nLineSpace, nBandSpace, psExtraArg );
    if( nRet == TRUE )
        return CE_None;
    else if( nRet < 0 )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      If we are requesting a single line at 1:1, we do a multi-band   */
/*      AdviseRead() and then TryWinRasterIO() again.                   */
/*                                                                      */
/*      Except for reading a 1x1 window when reading a scanline might   */
/*      be longer.                                                      */
/* -------------------------------------------------------------------- */
    if( nXSize == 1 && nYSize == 1 && nBufXSize == 1 && nBufYSize == 1 )
    {
        /* do nothing */
    }

#if !defined(SDK_CAN_DO_SUPERSAMPLING)
/* -------------------------------------------------------------------- */
/*      If we are supersampling we need to fall into the general        */
/*      purpose logic.                                                  */
/* -------------------------------------------------------------------- */
    else if( nXSize < nBufXSize || nYSize < nBufYSize )
    {
        bUseOldBandRasterIOImplementation = TRUE;
        CPLErr eErr =
            GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
        bUseOldBandRasterIOImplementation = FALSE;
        return eErr;
    }
#endif

    else if( nBufYSize == 1 )
    {
        // This is tricky, because it expects the rest of the image
        // with this buffer width to be read. The preferred way to
        // achieve this behaviour would be to call AdviseRead before
        // call IRasterIO.  The logic could be improved to detect
        // successive pattern of single line reading before doing an
        // AdviseRead.
        CPLErr eErr;

        eErr = AdviseRead( nXOff, nYOff, nXSize, GetRasterYSize() - nYOff,
                           nBufXSize, (nRasterYSize - nYOff) / nYSize, eBufType,
                           nBandCount, panBandMap, nullptr );
        if( eErr == CE_None
            && TryWinRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                               (GByte *) pData, nBufXSize, nBufYSize,
                               eBufType, nBandCount, panBandMap,
                               nPixelSpace, nLineSpace, nBandSpace, psExtraArg ) )
            return CE_None;
    }

    CPLDebug( "ECW",
              "RasterIO(%d,%d,%d,%d -> %dx%d) - doing interleaved read.",
              nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize );

/* -------------------------------------------------------------------- */
/*      Setup view.                                                     */
/* -------------------------------------------------------------------- */
    UINT32 anBandIndices[100];
    int    i;
    NCSError     eNCSErr;
    CNCSError    oErr(GetCNCSError(NCS_SUCCESS));

    for( i = 0; i < nBandCount; i++ )
        anBandIndices[i] = panBandMap[i] - 1;

    CleanupWindow();

/* -------------------------------------------------------------------- */
/*      Cache data in the context of a multi-band reading pattern.      */
/* -------------------------------------------------------------------- */
    if( nBandCount == 1 && *panBandMap == 1 && (nBands == 3 || nBands == 4) )
    {
        if( sCachedMultiBandIO.bEnabled && sCachedMultiBandIO.nBandsTried != nBands )
        {
            sCachedMultiBandIO.bEnabled = FALSE;
            CPLDebug("ECW", "Disabling successive band reading pattern");
        }

        sCachedMultiBandIO.nXOff = nXOff;
        sCachedMultiBandIO.nYOff = nYOff;
        sCachedMultiBandIO.nXSize = nXSize;
        sCachedMultiBandIO.nYSize = nYSize;
        sCachedMultiBandIO.nBufXSize = nBufXSize;
        sCachedMultiBandIO.nBufYSize = nBufYSize;
        sCachedMultiBandIO.eBufType = eBufType;
        sCachedMultiBandIO.nBandsTried = 1;

        int nBufTypeSize = GDALGetDataTypeSize(eBufType) / 8;

        if( sCachedMultiBandIO.bEnabled )
        {
            GByte* pNew = (GByte*)VSIRealloc(
                sCachedMultiBandIO.pabyData,
                    nBufXSize * nBufYSize * nBands * nBufTypeSize);
            if( pNew == nullptr )
                CPLFree(sCachedMultiBandIO.pabyData);
            sCachedMultiBandIO.pabyData = pNew;
        }

        if( sCachedMultiBandIO.bEnabled &&
            sCachedMultiBandIO.pabyData != nullptr )
        {
            nBandIndexToPromoteTo8Bit = -1;
            for( i = 0; i < nBands; i++ )
            {
                if( ((ECWRasterBand*)GetRasterBand(i+1))->bPromoteTo8Bit )
                    nBandIndexToPromoteTo8Bit = i;
                anBandIndices[i] = i;
            }

            oErr = poFileView->SetView( nBands, anBandIndices,
                                        nXOff, nYOff,
                                        nXOff + nXSize - 1,
                                        nYOff + nYSize - 1,
                                        nBufXSize, nBufYSize );
            eNCSErr = oErr.GetErrorNumber();

            if( eNCSErr != NCS_SUCCESS )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "%s", NCSGetErrorText(eNCSErr) );

                return CE_Failure;
            }

            CPLErr eErr = ReadBands(sCachedMultiBandIO.pabyData,
                                    nBufXSize, nBufYSize, eBufType,
                                    nBands,
                                    nBufTypeSize,
                                    nBufXSize * nBufTypeSize,
                                    nBufXSize * nBufYSize * nBufTypeSize,
                                    psExtraArg);
            if( eErr != CE_None )
                return eErr;

            int j;
            for(j = 0; j < nBufYSize; j++)
            {
                GDALCopyWords(sCachedMultiBandIO.pabyData +
                                    j * nBufXSize * nBufTypeSize,
                              eBufType, nBufTypeSize,
                              ((GByte*)pData) + j * nLineSpace, eBufType, (int)nPixelSpace,
                              nBufXSize);
            }
            return CE_None;
        }
    }

    nBandIndexToPromoteTo8Bit = -1;
    for( i = 0; i < nBandCount; i++ )
    {
        if( ((ECWRasterBand*)GetRasterBand(anBandIndices[i]+1))->bPromoteTo8Bit )
            nBandIndexToPromoteTo8Bit = i;
    }
    oErr = poFileView->SetView( nBandCount, anBandIndices,
                                nXOff, nYOff,
                                nXOff + nXSize - 1,
                                nYOff + nYSize - 1,
                                nBufXSize, nBufYSize );
    eNCSErr = oErr.GetErrorNumber();

    if( eNCSErr != NCS_SUCCESS )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s", NCSGetErrorText(eNCSErr) );

        return CE_Failure;
    }

    return ReadBands(pData, nBufXSize, nBufYSize, eBufType,
                     nBandCount, nPixelSpace, nLineSpace, nBandSpace,
                     psExtraArg);
}

/************************************************************************/
/*                        ReadBandsDirectly()                           */
/************************************************************************/

CPLErr ECWDataset::ReadBandsDirectly(void * pData, int nBufXSize, int nBufYSize,
                                     CPL_UNUSED GDALDataType eBufType,
                                     int nBandCount,
                                     CPL_UNUSED GSpacing nPixelSpace,
                                     GSpacing nLineSpace,
                                     GSpacing nBandSpace,
                                     GDALRasterIOExtraArg* psExtraArg)
{
    CPLDebug( "ECW",
              "ReadBandsDirectly(-> %dx%d) - reading lines directly.",
              nBufXSize, nBufYSize);

    UINT8 **pBIL = (UINT8**)NCSMalloc(nBandCount * sizeof(UINT8*), FALSE);

    for(int nB = 0; nB < nBandCount; nB++)
    {
        pBIL[nB] = ((UINT8*)pData) + (nBandSpace*nB);//for any bit depth
    }

    CPLErr eErr = CE_None;
    for(int nR = 0; nR < nBufYSize; nR++)
    {
        if (poFileView->ReadLineBIL(eNCSRequestDataType,(UINT16) nBandCount, (void**)pBIL) != NCSECW_READ_OK)
        {
            eErr = CE_Failure;
            break;
        }
        for(int nB = 0; nB < nBandCount; nB++)
        {
            if( nB == nBandIndexToPromoteTo8Bit )
            {
                for(int iX = 0; iX < nBufXSize; iX ++ )
                {
                    pBIL[nB][iX] *= 255;
                }
            }
            pBIL[nB] += nLineSpace;
        }

        if( psExtraArg->pfnProgress != nullptr &&
                    !psExtraArg->pfnProgress(1.0 * (nR + 1) / nBufYSize, "",
                                             psExtraArg->pProgressData) )
        {
            eErr = CE_Failure;
            break;
        }
    }
    if(pBIL)
    {
        NCSFree(pBIL);
    }
    return eErr;
}

/************************************************************************/
/*                            ReadBands()                               */
/************************************************************************/

CPLErr ECWDataset::ReadBands(void * pData, int nBufXSize, int nBufYSize,
                             GDALDataType eBufType,
                             int nBandCount,
                             GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
                             GDALRasterIOExtraArg* psExtraArg)
{
    int i;
/* -------------------------------------------------------------------- */
/*      Setup working scanline, and the pointers into it.               */
/* -------------------------------------------------------------------- */
    int nDataTypeSize = (GDALGetDataTypeSize(eRasterDataType) / 8);
    bool bDirect = (eBufType == eRasterDataType) && nDataTypeSize == nPixelSpace &&
        nLineSpace == (nPixelSpace*nBufXSize) && nBandSpace == (static_cast<GSpacing>(nDataTypeSize)*nBufXSize*nBufYSize) ;
    if (bDirect)
    {
        return ReadBandsDirectly(pData, nBufXSize, nBufYSize,eBufType,
            nBandCount, nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
    }
     CPLDebug( "ECW",
              "ReadBands(-> %dx%d) - reading lines using GDALCopyWords.",
              nBufXSize, nBufYSize);
    CPLErr eErr = CE_None;
    GByte *pabyBILScanline = (GByte *) CPLMalloc(nBufXSize * nDataTypeSize *
                                                nBandCount);
    GByte **papabyBIL = (GByte **) CPLMalloc(nBandCount * sizeof(void*));

    for( i = 0; i < nBandCount; i++ )
        papabyBIL[i] = pabyBILScanline + i * nBufXSize * nDataTypeSize;

/* -------------------------------------------------------------------- */
/*      Read back all the data for the requested view.                  */
/* -------------------------------------------------------------------- */
    for( int iScanline = 0; iScanline < nBufYSize; iScanline++ )
    {
        NCSEcwReadStatus  eRStatus;

        eRStatus = poFileView->ReadLineBIL( eNCSRequestDataType,
                                            (UINT16) nBandCount,
                                            (void **) papabyBIL );
        if( eRStatus != NCSECW_READ_OK )
        {
            eErr = CE_Failure;
            CPLError( CE_Failure, CPLE_AppDefined,
                    "NCScbmReadViewLineBIL failed." );
            break;
        }

        for( i = 0; i < nBandCount; i++ )
        {
            if( i == nBandIndexToPromoteTo8Bit )
            {
                for(int iX = 0; iX < nBufXSize; iX ++ )
                {
                    papabyBIL[i][iX] *= 255;
                }
            }

            GDALCopyWords(
                pabyBILScanline + i * nDataTypeSize * nBufXSize,
                eRasterDataType, nDataTypeSize,
                ((GByte *) pData) + nLineSpace * iScanline + nBandSpace * i,
                eBufType, (int)nPixelSpace,
                nBufXSize );
        }

        if( psExtraArg->pfnProgress != nullptr &&
                    !psExtraArg->pfnProgress(1.0 * (iScanline + 1) / nBufYSize, "",
                                             psExtraArg->pProgressData) )
        {
            eErr = CE_Failure;
            break;
        }
    }

    CPLFree( pabyBILScanline );
    CPLFree( papabyBIL );

    return eErr;
}

/************************************************************************/
/*                        IdentifyJPEG2000()                            */
/*                                                                      */
/*          Identify method that only supports JPEG2000 files.          */
/************************************************************************/

int ECWDataset::IdentifyJPEG2000( GDALOpenInfo * poOpenInfo )

{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "J2K_SUBFILE:") )
        return TRUE;

    else if( poOpenInfo->nHeaderBytes >= 16
        && (memcmp( poOpenInfo->pabyHeader, jpc_header,
                    sizeof(jpc_header) ) == 0
            || memcmp( poOpenInfo->pabyHeader, jp2_header,
                    sizeof(jp2_header) ) == 0) )
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                            OpenJPEG2000()                            */
/*                                                                      */
/*          Open method that only supports JPEG2000 files.              */
/************************************************************************/

GDALDataset *ECWDataset::OpenJPEG2000( GDALOpenInfo * poOpenInfo )

{
    if (!IdentifyJPEG2000(poOpenInfo))
        return nullptr;

    return Open( poOpenInfo, TRUE );
}

/************************************************************************/
/*                           IdentifyECW()                              */
/*                                                                      */
/*      Identify method that only supports ECW files.                   */
/************************************************************************/

int ECWDataset::IdentifyECW( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      This has to either be a file on disk ending in .ecw or a        */
/*      ecwp: protocol url.                                             */
/* -------------------------------------------------------------------- */
    if( (!EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"ecw")
         || poOpenInfo->nHeaderBytes == 0)
        && !STARTS_WITH_CI(poOpenInfo->pszFilename, "ecwp:")
        && !STARTS_WITH_CI(poOpenInfo->pszFilename,"ecwps:") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                              OpenECW()                               */
/*                                                                      */
/*      Open method that only supports ECW files.                       */
/************************************************************************/

GDALDataset *ECWDataset::OpenECW( GDALOpenInfo * poOpenInfo )

{
    if (!IdentifyECW(poOpenInfo))
        return nullptr;

    return Open( poOpenInfo, FALSE );
}

/************************************************************************/
/*                            OpenFileView()                            */
/************************************************************************/

CNCSJP2FileView *ECWDataset::OpenFileView( const char *pszDatasetName,
                                           bool bProgressive,
                                           int &bUsingCustomStream,
                                           CPL_UNUSED bool bWrite )
{
/* -------------------------------------------------------------------- */
/*      First we try to open it as a normal CNCSFile, letting the       */
/*      ECW SDK manage the IO itself.   This will only work for real    */
/*      files, and ecwp: or ecwps: sources.                             */
/* -------------------------------------------------------------------- */
    CNCSJP2FileView *poFileView = nullptr;
    NCSError         eErr;
    CNCSError        oErr(GetCNCSError(NCS_SUCCESS));

    bUsingCustomStream = FALSE;
    poFileView = new CNCSFile();
    //we always open in read only mode. This should be improved in the future.
    try
    {
#ifdef WIN32
        if( CPLTestBool( CPLGetConfigOption( "GDAL_FILENAME_IS_UTF8", "YES" ) ) )
        {
            wchar_t *pwszDatasetName = CPLRecodeToWChar( pszDatasetName, CPL_ENC_UTF8, CPL_ENC_UCS2 );
            oErr = poFileView->Open( pwszDatasetName, bProgressive, false );
            CPLFree( pwszDatasetName );
        }
        else
#endif
        {
            oErr = poFileView->Open( (char *) pszDatasetName, bProgressive, false );
        }
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unexpected exception occurred in ECW SDK");
        delete poFileView;
        return nullptr;
    }
    eErr = oErr.GetErrorNumber();

/* -------------------------------------------------------------------- */
/*      If that did not work, trying opening as a virtual file.         */
/* -------------------------------------------------------------------- */
    if( eErr != NCS_SUCCESS )
    {
        CPLDebug( "ECW",
                  "NCScbmOpenFileView(%s): eErr=%d, will try VSIL stream.",
                  pszDatasetName, (int) eErr );

        delete poFileView;

        VSILFILE *fpVSIL = VSIFOpenL( pszDatasetName, "rb" );
        if( fpVSIL == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open %s.", pszDatasetName );
            return nullptr;
        }

        if( hECWDatasetMutex == nullptr )
        {
            hECWDatasetMutex = CPLCreateMutex();
        }
        else if( !CPLAcquireMutex( hECWDatasetMutex, 60.0 ) )
        {
            CPLDebug( "ECW", "Failed to acquire mutex in 60s." );
        }
        else
        {
            CPLDebug( "ECW", "Got mutex." );
        }
        
        poFileView = new CNCSJP2FileView();

#if ECWSDK_VERSION >= 55
        NCS::CString streamName(pszDatasetName);
        auto vsiIoStream = NCS::CView::FindSteamByStreamNameFromOpenDatasets(streamName);
        if (!vsiIoStream) {
            vsiIoStream = std::make_shared<VSIIOStream>();
            std::static_pointer_cast<VSIIOStream>(vsiIoStream)->Access(fpVSIL, FALSE, TRUE, pszDatasetName, 0, -1);
        }
        oErr = poFileView->Open(vsiIoStream, bProgressive);
#else
        auto vsiIoStream = new VSIIOStream();
        vsiIoStream->Access(fpVSIL, FALSE, TRUE, pszDatasetName, 0, -1);
        oErr = poFileView->Open(vsiIoStream, bProgressive);

        // The CNCSJP2FileView (poFileView) object may not use the iostream
        // (poIOStream) passed to the CNCSJP2FileView::Open() method if an
        // iostream is already available to the ECW JPEG 2000 SDK for a given
        // file.  Consequently, if the iostream passed to
        // CNCSJP2FileView::Open() does not become the underlying iostream
        // of the CNCSJP2FileView object, then it should be deleted.
        //
        // In addition, the underlying iostream of the CNCSJP2FileView object
        // should not be deleted until all CNCSJP2FileView objects using the
        // underlying iostream are deleted. Consequently, each time a
        // CNCSJP2FileView object is created, the nFileViewCount attribute
        // of the underlying VSIIOStream object must be incremented for use
        // in the ECWDataset destructor.

        VSIIOStream * poUnderlyingIOStream =
            ((VSIIOStream *)(poFileView->GetStream()));

        if ( poUnderlyingIOStream )
            poUnderlyingIOStream->nFileViewCount++;

        if ( vsiIoStream != poUnderlyingIOStream )
        {
            delete vsiIoStream;
        }
        else
        {
            bUsingCustomStream = TRUE;
        }
#endif

        CPLReleaseMutex( hECWDatasetMutex );

        if( oErr.GetErrorNumber() != NCS_SUCCESS )
        {
            delete poFileView;
            ECWReportError(oErr);

            return nullptr;
        }
    }

    return poFileView;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ECWDataset::Open( GDALOpenInfo * poOpenInfo, int bIsJPEG2000 )

{
    CNCSJP2FileView *poFileView = nullptr;
    int              i;
    int              bUsingCustomStream = FALSE;
    CPLString        osFilename = poOpenInfo->pszFilename;

    ECWInitialize();

    /* Note: J2K_SUBFILE is somehow an obsolete concept that predates /vsisubfile/ */
    /* syntax and was used mainly(only?) by the NITF driver before its switch */
    /* to /vsisubfile */

/* -------------------------------------------------------------------- */
/*      If we get a J2K_SUBFILE style name, convert it into the         */
/*      corresponding /vsisubfile/ path.                                */
/*                                                                      */
/*      From: J2K_SUBFILE:offset,size,filename                           */
/*      To: /vsisubfile/offset_size,filename                            */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(osFilename, "J2K_SUBFILE:"))
    {
        char** papszTokens = CSLTokenizeString2(osFilename.c_str()+12, ",", 0);
        if (CSLCount(papszTokens) >= 3)
        {
            osFilename.Printf( "/vsisubfile/%s_%s,%s",
                               papszTokens[0], papszTokens[1], papszTokens[2]);
        }
        else
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to parse J2K_SUBFILE specification." );
            CSLDestroy(papszTokens);
            return nullptr;
        }
        CSLDestroy(papszTokens);
    }

/* -------------------------------------------------------------------- */
/*      Open the client interface.                                      */
/* -------------------------------------------------------------------- */
    poFileView = OpenFileView( osFilename.c_str(), false, bUsingCustomStream, poOpenInfo->eAccess == GA_Update );
    if( poFileView == nullptr )
    {
#if ECWSDK_VERSION < 50
        /* Detect what is apparently the ECW v3 file format signature */
        if( EQUAL(CPLGetExtension(osFilename), "ECW") &&
            poOpenInfo->nHeaderBytes > 0x30 &&
            STARTS_WITH_CI((const char*)(poOpenInfo->pabyHeader + 0x20), "ecw ECW3") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot open %s which looks like a ECW format v3 file, that requires ECW SDK 5.0 or later",
                     osFilename.c_str());
        }
#endif
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ECWDataset  *poDS = new ECWDataset(bIsJPEG2000);
    poDS->poFileView = poFileView;
    poDS->eAccess = poOpenInfo->eAccess;

    // Disable .aux.xml writing for subfiles and such.  Unfortunately
    // this will also disable it in some cases where it might be
    // applicable.
    if( bUsingCustomStream )
        poDS->nPamFlags |= GPF_DISABLED;

    poDS->bUsingCustomStream = bUsingCustomStream;

/* -------------------------------------------------------------------- */
/*      Fetch general file information.                                 */
/* -------------------------------------------------------------------- */
    poDS->psFileInfo = poFileView->GetFileInfo();

    CPLDebug( "ECW", "FileInfo: SizeXY=%d,%d Bands=%d\n"
              "       OriginXY=%g,%g  CellIncrementXY=%g,%g\n"
              "       ColorSpace=%d, eCellType=%d\n",
              poDS->psFileInfo->nSizeX,
              poDS->psFileInfo->nSizeY,
              poDS->psFileInfo->nBands,
              poDS->psFileInfo->fOriginX,
              poDS->psFileInfo->fOriginY,
              poDS->psFileInfo->fCellIncrementX,
              poDS->psFileInfo->fCellIncrementY,
              (int) poDS->psFileInfo->eColorSpace,
              (int) poDS->psFileInfo->eCellType );

/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->psFileInfo->nSizeX;
    poDS->nRasterYSize = poDS->psFileInfo->nSizeY;

/* -------------------------------------------------------------------- */
/*      Establish the GDAL data type that corresponds.  A few NCS       */
/*      data types have no direct corresponding value in GDAL so we     */
/*      will coerce to something sufficiently similar.                  */
/* -------------------------------------------------------------------- */
    poDS->eNCSRequestDataType = poDS->psFileInfo->eCellType;
    switch( poDS->psFileInfo->eCellType )
    {
        case NCSCT_UINT8:
            poDS->eRasterDataType = GDT_Byte;
            break;

        case NCSCT_UINT16:
            poDS->eRasterDataType = GDT_UInt16;
            break;

        case NCSCT_UINT32:
        case NCSCT_UINT64:
            poDS->eRasterDataType = GDT_UInt32;
            poDS->eNCSRequestDataType = NCSCT_UINT32;
            break;

        case NCSCT_INT8:
        case NCSCT_INT16:
            poDS->eRasterDataType = GDT_Int16;
            poDS->eNCSRequestDataType = NCSCT_INT16;
            break;

        case NCSCT_INT32:
        case NCSCT_INT64:
            poDS->eRasterDataType = GDT_Int32;
            poDS->eNCSRequestDataType = NCSCT_INT32;
            break;

        case NCSCT_IEEE4:
            poDS->eRasterDataType = GDT_Float32;
            break;

        case NCSCT_IEEE8:
            poDS->eRasterDataType = GDT_Float64;
            break;

        default:
            CPLDebug("ECW", "Unhandled case : eCellType = %d",
                     (int)poDS->psFileInfo->eCellType );
            break;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i=0; i < poDS->psFileInfo->nBands; i++ )
        poDS->SetBand( i+1, new ECWRasterBand( poDS, i+1, -1, poOpenInfo->papszOpenOptions ) );

/* -------------------------------------------------------------------- */
/*      Look for supporting coordinate system information.              */
/* -------------------------------------------------------------------- */
    if( bIsJPEG2000 )
    {
        poDS->LoadJP2Metadata(poOpenInfo, osFilename);
    }
    else
    {
        poDS->ECW2WKTProjection();

    /* -------------------------------------------------------------------- */
    /*      Check for world file.                                           */
    /* -------------------------------------------------------------------- */
        if( !poDS->bGeoTransformValid )
        {
            poDS->bGeoTransformValid |=
                GDALReadWorldFile2( osFilename, nullptr,
                                    poDS->adfGeoTransform,
                                    poOpenInfo->GetSiblingFiles(), nullptr )
                || GDALReadWorldFile2( osFilename, ".wld",
                                    poDS->adfGeoTransform,
                                    poOpenInfo->GetSiblingFiles(), nullptr );
        }
    }

    poDS->SetMetadataItem("COMPRESSION_RATE_TARGET", CPLString().Printf("%d", poDS->psFileInfo->nCompressionRate));
    poDS->SetMetadataItem("COLORSPACE", ECWGetColorSpaceName(poDS->psFileInfo->eColorSpace));
#if ECWSDK_VERSION>=50
    if( !bIsJPEG2000 )
         poDS->SetMetadataItem("VERSION", CPLString().Printf("%d", poDS->psFileInfo->nFormatVersion));
#if ECWSDK_VERSION>=51
    // output jp2 header info
    if( bIsJPEG2000 && poDS->poFileView ) {
        // comments
        char *csComments = nullptr;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:COMMENTS", &csComments);
        if (csComments) {
            poDS->SetMetadataItem("ALL_COMMENTS", CPLString().Printf("%s", csComments));
            NCSFree(csComments);
        }

        // Profile
        UINT32 nProfile = 2;
        UINT32 nRsiz = 0;
        poDS->poFileView->GetParameter((char*)"JP2:COMPLIANCE:PROFILE:TYPE", &nRsiz);
        if (nRsiz == 0)
            nProfile = 2; // Profile 2 (no restrictions)
        else if (nRsiz == 1)
            nProfile = 0; // Profile 0
        else if (nRsiz == 2)
            nProfile = 1; // Profile 1, NITF_BIIF_NPJE, NITF_BIIF_EPJE
        poDS->SetMetadataItem("PROFILE", CPLString().Printf("%d", nProfile), JPEG2000_DOMAIN_NAME);

        // number of tiles on X axis
        UINT32 nTileNrX = 1;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:TILENR:X", &nTileNrX);
        poDS->SetMetadataItem("TILES_X", CPLString().Printf("%d", nTileNrX), JPEG2000_DOMAIN_NAME);

        // number of tiles on X axis
        UINT32 nTileNrY = 1;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:TILENR:Y", &nTileNrY);
        poDS->SetMetadataItem("TILES_Y", CPLString().Printf("%d", nTileNrY), JPEG2000_DOMAIN_NAME);

        // Tile Width
        UINT32 nTileSizeX = 0;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:TILESIZE:X", &nTileSizeX);
        poDS->SetMetadataItem("TILE_WIDTH", CPLString().Printf("%d", nTileSizeX), JPEG2000_DOMAIN_NAME);

        // Tile Height
        UINT32 nTileSizeY = 0;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:TILESIZE:Y", &nTileSizeY);
        poDS->SetMetadataItem("TILE_HEIGHT", CPLString().Printf("%d", nTileSizeY), JPEG2000_DOMAIN_NAME);

        // Precinct Sizes on X axis
        char *csPreSizeX = nullptr;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:PRECINCTSIZE:X", &csPreSizeX);
        if (csPreSizeX) {
                poDS->SetMetadataItem("PRECINCT_SIZE_X", csPreSizeX, JPEG2000_DOMAIN_NAME);
            NCSFree(csPreSizeX);
        }

        // Precinct Sizes on Y axis
        char *csPreSizeY = nullptr;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:PRECINCTSIZE:Y", &csPreSizeY);
        if (csPreSizeY) {
            poDS->SetMetadataItem("PRECINCT_SIZE_Y", csPreSizeY, JPEG2000_DOMAIN_NAME);
            NCSFree(csPreSizeY);
        }

        // Code Block Size on X axis
        UINT32 nCodeBlockSizeX = 0;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:CODEBLOCK:X", &nCodeBlockSizeX);
        poDS->SetMetadataItem("CODE_BLOCK_SIZE_X", CPLString().Printf("%d", nCodeBlockSizeX), JPEG2000_DOMAIN_NAME);

        // Code Block Size on Y axis
        UINT32 nCodeBlockSizeY = 0;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:CODEBLOCK:Y", &nCodeBlockSizeY);
        poDS->SetMetadataItem("CODE_BLOCK_SIZE_Y", CPLString().Printf("%d", nCodeBlockSizeY), JPEG2000_DOMAIN_NAME);

        // Bitdepth
        char *csBitdepth = nullptr;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:BITDEPTH", &csBitdepth);
        if (csBitdepth) {
            poDS->SetMetadataItem("PRECISION", csBitdepth, JPEG2000_DOMAIN_NAME);
            NCSFree(csBitdepth);
        }

        // Resolution Levels
        UINT32 nLevels = 0;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:RESOLUTION:LEVELS", &nLevels);
        poDS->SetMetadataItem("RESOLUTION_LEVELS", CPLString().Printf("%d", nLevels), JPEG2000_DOMAIN_NAME);

        // Qualaity Layers
        UINT32 nLayers = 0;
        poDS->poFileView->GetParameter((char*)"JP2:DECOMPRESS:LAYERS", &nLayers);
        poDS->SetMetadataItem("QUALITY_LAYERS", CPLString().Printf("%d", nLayers), JPEG2000_DOMAIN_NAME);

        // Progression Order
        char *csOrder = nullptr;
        poDS->poFileView->GetParameter((char*)"JPC:DECOMPRESS:PROGRESSION:ORDER", &csOrder);
        if (csOrder) {
            poDS->SetMetadataItem("PROGRESSION_ORDER", csOrder, JPEG2000_DOMAIN_NAME);
            NCSFree(csOrder);
        }

        // DWT Filter
        const char *csFilter = nullptr;
        UINT32 nFilter;
        poDS->poFileView->GetParameter((char*)"JP2:TRANSFORMATION:TYPE", &nFilter);
        if (nFilter)
            csFilter = "5x3";
        else
            csFilter = "9x7";
        poDS->SetMetadataItem("TRANSFORMATION_TYPE", csFilter, JPEG2000_DOMAIN_NAME);

        // SOP used?
        bool bSOP = 0;
        poDS->poFileView->GetParameter((char*)"JP2:DECOMPRESS:SOP:EXISTS", &bSOP);
        poDS->SetMetadataItem("USE_SOP", (bSOP) ? "TRUE" : "FALSE", JPEG2000_DOMAIN_NAME);

        // EPH used?
        bool bEPH = 0;
        poDS->poFileView->GetParameter((char*)"JP2:DECOMPRESS:EPH:EXISTS", &bEPH);
        poDS->SetMetadataItem("USE_EPH", (bEPH) ? "TRUE" : "FALSE", JPEG2000_DOMAIN_NAME);

        // GML JP2 data contained?
        bool bGML = 0;
        poDS->poFileView->GetParameter((char*)"JP2:GML:JP2:BOX:EXISTS", &bGML);
        poDS->SetMetadataItem("GML_JP2_DATA", (bGML) ? "TRUE" : "FALSE", JPEG2000_DOMAIN_NAME);
    }
    #endif //ECWSDK_VERSION>=51
    if ( !bIsJPEG2000 && poDS->psFileInfo->nFormatVersion >=3 ){
        poDS->SetMetadataItem("COMPRESSION_RATE_ACTUAL", CPLString().Printf("%f", poDS->psFileInfo->fActualCompressionRate));
        poDS->SetMetadataItem("CLOCKWISE_ROTATION_DEG", CPLString().Printf("%f", poDS->psFileInfo->fCWRotationDegrees));
        poDS->SetMetadataItem("COMPRESSION_DATE", poDS->psFileInfo->sCompressionDate);
        //Get file metadata.
        poDS->ReadFileMetaDataFromFile();
    }
#else
    poDS->SetMetadataItem("VERSION", CPLString().Printf("%d",bIsJPEG2000?1:2));
#endif

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( osFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Vector layers                                                   */
/* -------------------------------------------------------------------- */
    if( bIsJPEG2000 && poOpenInfo->nOpenFlags & GDAL_OF_VECTOR )
    {
        poDS->LoadVectorLayers(
            CPLFetchBool(poOpenInfo->papszOpenOptions, "OPEN_REMOTE_GML",
                         false));

        // If file opened in vector-only mode and there's no vector,
        // return
        if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
            poDS->GetLayerCount() == 0 )
        {
            delete poDS;
            return nullptr;
        }
    }

    return poDS;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **ECWDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "ECW", "GML", nullptr);
}

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char *ECWDataset::GetMetadataItem( const char * pszName,
                                         const char * pszDomain )
{
    if (!bIsJPEG2000 && pszDomain != nullptr && EQUAL(pszDomain, "ECW") && pszName != nullptr)
    {
        if (EQUAL(pszName, "PROJ"))
            return m_osProjCode.size() ? m_osProjCode.c_str() : "RAW";
        if (EQUAL(pszName, "DATUM"))
            return m_osDatumCode.size() ? m_osDatumCode.c_str() : "RAW";
        if (EQUAL(pszName, "UNITS"))
            return m_osUnitsCode.size() ? m_osUnitsCode.c_str() : "METERS";
    }
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **ECWDataset::GetMetadata( const char *pszDomain )

{
    if( !bIsJPEG2000 && pszDomain != nullptr && EQUAL(pszDomain, "ECW") )
    {
        oECWMetadataList.Clear();
        oECWMetadataList.AddString(CPLSPrintf("%s=%s", "PROJ", GetMetadataItem("PROJ", "ECW")));
        oECWMetadataList.AddString(CPLSPrintf("%s=%s", "DATUM", GetMetadataItem("DATUM", "ECW")));
        oECWMetadataList.AddString(CPLSPrintf("%s=%s", "UNITS", GetMetadataItem("UNITS", "ECW")));
        return oECWMetadataList.List();
    }
    else if( pszDomain == nullptr || !EQUAL(pszDomain,"GML") )
        return GDALPamDataset::GetMetadata( pszDomain );
    else
        return papszGMLMetadata;
}

/************************************************************************/
/*                   ReadFileMetaDataFromFile()                         */
/*                                                                      */
/* Gets relevant information from NCSFileMetadata and populates         */
/* GDAL metadata.                                                       */
/*                                                                      */
/************************************************************************/
#if ECWSDK_VERSION >= 50
void ECWDataset::ReadFileMetaDataFromFile()
{
    if (psFileInfo->pFileMetaData == nullptr) return;

    if (psFileInfo->pFileMetaData->sClassification != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_CLASSIFICATION", NCS::CString(psFileInfo->pFileMetaData->sClassification));
    if (psFileInfo->pFileMetaData->sAcquisitionDate != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_ACQUISITION_DATE", NCS::CString(psFileInfo->pFileMetaData->sAcquisitionDate));
    if (psFileInfo->pFileMetaData->sAcquisitionSensorName != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_ACQUISITION_SENSOR_NAME", NCS::CString(psFileInfo->pFileMetaData->sAcquisitionSensorName));
    if (psFileInfo->pFileMetaData->sCompressionSoftware != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_COMPRESSION_SOFTWARE", NCS::CString(psFileInfo->pFileMetaData->sCompressionSoftware));
    if (psFileInfo->pFileMetaData->sAuthor != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_AUTHOR", NCS::CString(psFileInfo->pFileMetaData->sAuthor));
    if (psFileInfo->pFileMetaData->sCopyright != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_COPYRIGHT", NCS::CString(psFileInfo->pFileMetaData->sCopyright));
    if (psFileInfo->pFileMetaData->sCompany != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_COMPANY", NCS::CString(psFileInfo->pFileMetaData->sCompany));
    if (psFileInfo->pFileMetaData->sEmail != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_EMAIL", NCS::CString(psFileInfo->pFileMetaData->sEmail));
    if (psFileInfo->pFileMetaData->sAddress != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_ADDRESS", NCS::CString(psFileInfo->pFileMetaData->sAddress));
    if (psFileInfo->pFileMetaData->sTelephone != nullptr )
        GDALDataset::SetMetadataItem("FILE_METADATA_TELEPHONE", NCS::CString(psFileInfo->pFileMetaData->sTelephone));
}

/************************************************************************/
/*                       WriteFileMetaData()                            */
/************************************************************************/

void ECWDataset::WriteFileMetaData(NCSFileMetaData* pFileMetaDataCopy)
{
    if (!bFileMetaDataDirty )
        return;

    CPLAssert(eAccess == GA_Update);
    CPLAssert(!bIsJPEG2000);

    bFileMetaDataDirty = FALSE;

    NCSFileView *psFileView = nullptr;
    NCSError eErr;

    psFileView = NCSEditOpen( GetDescription() );
    if (psFileView == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "NCSEditOpen() failed");
        return;
    }

    eErr = NCSEditSetFileMetaData(psFileView, pFileMetaDataCopy);
    if( eErr != NCS_SUCCESS )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "NCSEditSetFileMetaData() failed : %s",
                 NCSGetLastErrorText(eErr));
    }

    NCSEditFlushAll(psFileView);
    NCSEditClose(psFileView);
}

#endif
/************************************************************************/
/*                         ECW2WKTProjection()                          */
/*                                                                      */
/*      Set the dataset pszProjection string in OGC WKT format by       */
/*      looking up the ECW (GDT) coordinate system info in              */
/*      ecw_cs.wkt support data file.                                   */
/*                                                                      */
/*      This code is likely still broken in some circumstances.  For    */
/*      instance, I haven't been careful about changing the linear      */
/*      projection parameters (false easting/northing) if the units     */
/*      is feet.  Lots of cases missing here, and in ecw_cs.wkt.        */
/************************************************************************/

void ECWDataset::ECW2WKTProjection()

{
    if( psFileInfo == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Capture Geotransform.                                           */
/*                                                                      */
/*      We will try to ignore the provided file information if it is    */
/*      origin (0,0) and pixel size (1,1).  I think sometimes I have    */
/*      also seen pixel increments of 0 on invalid datasets.            */
/* -------------------------------------------------------------------- */
    if( psFileInfo->fOriginX != 0.0
        || psFileInfo->fOriginY != 0.0
        || (psFileInfo->fCellIncrementX != 0.0
            && psFileInfo->fCellIncrementX != 1.0)
        || (psFileInfo->fCellIncrementY != 0.0
            && psFileInfo->fCellIncrementY != 1.0) )
    {
        bGeoTransformValid = TRUE;

        adfGeoTransform[0] = psFileInfo->fOriginX;
        adfGeoTransform[1] = psFileInfo->fCellIncrementX;
        adfGeoTransform[2] = 0.0;

        adfGeoTransform[3] = psFileInfo->fOriginY;
        adfGeoTransform[4] = 0.0;

        /* By default, set Y-resolution negative assuming images always */
        /* have "Upward" orientation (Y coordinates increase "Upward"). */
        /* Setting ECW_ALWAYS_UPWARD=FALSE option relexes that policy   */
        /* and makes the driver rely on the actual Y-resolution         */
        /* value (sign) of an image. This allows to correctly process   */
        /* rare images with "Downward" orientation, where Y coordinates */
        /* increase "Downward" and Y-resolution is positive.            */
        if( CPLTestBool( CPLGetConfigOption("ECW_ALWAYS_UPWARD","TRUE") ) )
            adfGeoTransform[5] = -fabs(psFileInfo->fCellIncrementY);
        else
            adfGeoTransform[5] = psFileInfo->fCellIncrementY;
    }

/* -------------------------------------------------------------------- */
/*      do we have projection and datum?                                */
/* -------------------------------------------------------------------- */
    CPLString osUnits = ECWTranslateFromCellSizeUnits(psFileInfo->eCellSizeUnits);

    CPLDebug( "ECW", "projection=%s, datum=%s, units=%s",
              psFileInfo->szProjection, psFileInfo->szDatum,
              osUnits.c_str());

    if( EQUAL(psFileInfo->szProjection,"RAW") )
        return;

/* -------------------------------------------------------------------- */
/*      Set projection if we have it.                                   */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    /* For backward-compatible with previous behaviour. Should we only */
    /* restrict to those 2 values ? */
    if (psFileInfo->eCellSizeUnits != ECW_CELL_UNITS_METERS &&
        psFileInfo->eCellSizeUnits != ECW_CELL_UNITS_FEET)
        osUnits = ECWTranslateFromCellSizeUnits(ECW_CELL_UNITS_METERS);

    m_osDatumCode = psFileInfo->szDatum;
    m_osProjCode = psFileInfo->szProjection;
    m_osUnitsCode = osUnits;
    if( oSRS.importFromERM( psFileInfo->szProjection,
                            psFileInfo->szDatum,
                            osUnits ) == OGRERR_NONE )
    {
        oSRS.exportToWkt( &pszProjection );
    }

    CPLErrorReset(); /* see #4187 */
}

/************************************************************************/
/*                        ECWTranslateFromWKT()                         */
/************************************************************************/

int ECWTranslateFromWKT( const char *pszWKT,
                         char *pszProjection,
                         int nProjectionLen,
                         char *pszDatum,
                         int nDatumLen,
                         char *pszUnits)

{
    OGRSpatialReference oSRS;

    strcpy( pszProjection, "RAW" );
    strcpy( pszDatum, "RAW" );
    strcpy( pszUnits, "METERS" );

    if( pszWKT == nullptr || strlen(pszWKT) == 0 )
        return FALSE;

    oSRS.importFromWkt( pszWKT );

    if( oSRS.IsLocal() )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Do we have an overall EPSG number for this coordinate system?   */
/* -------------------------------------------------------------------- */
    const char *pszAuthorityCode = nullptr;
    const char *pszAuthorityName = nullptr;
    UINT32 nEPSGCode = 0;

    if( oSRS.IsProjected() )
    {
        pszAuthorityCode =  oSRS.GetAuthorityCode( "PROJCS" );
        pszAuthorityName =  oSRS.GetAuthorityName( "PROJCS" );
    }
    else if( oSRS.IsGeographic() )
    {
        pszAuthorityCode =  oSRS.GetAuthorityCode( "GEOGCS" );
        pszAuthorityName =  oSRS.GetAuthorityName( "GEOGCS" );
    }

    if( pszAuthorityName != nullptr && EQUAL(pszAuthorityName,"EPSG")
        && pszAuthorityCode != nullptr && atoi(pszAuthorityCode) > 0 )
        nEPSGCode = (UINT32) atoi(pszAuthorityCode);

    if( nEPSGCode != 0 )
    {
        char *pszEPSGProj = nullptr, *pszEPSGDatum = nullptr;
        CNCSError oErr =
            CNCSJP2FileView::GetProjectionAndDatum( atoi(pszAuthorityCode),
                                                 &pszEPSGProj, &pszEPSGDatum );

        CPLDebug( "ECW", "GetGDTProjDat(%d) = %s/%s",
                  atoi(pszAuthorityCode),
                  pszEPSGProj ? pszEPSGProj : "(null)",
                  pszEPSGDatum ? pszEPSGDatum : "(null)");

        if( oErr.GetErrorNumber() == NCS_SUCCESS
            && pszEPSGProj != nullptr && pszEPSGDatum != nullptr )
        {
            strncpy( pszProjection, pszEPSGProj, nProjectionLen );
            strncpy( pszDatum, pszEPSGDatum, nDatumLen );
            pszProjection[nProjectionLen - 1] = 0;
            pszDatum[nDatumLen - 1] = 0;
            NCSFree( pszEPSGProj );
            NCSFree( pszEPSGDatum );
            return TRUE;
        }

        NCSFree( pszEPSGProj );
        NCSFree( pszEPSGDatum );
    }

/* -------------------------------------------------------------------- */
/*      Fallback to translating based on the ecw_cs.wkt file, and       */
/*      various jiffy rules.                                            */
/* -------------------------------------------------------------------- */

    return oSRS.exportToERM( pszProjection, pszDatum, pszUnits ) == OGRERR_NONE;
}

/************************************************************************/
/*                    ECWTranslateToCellSizeUnits()                     */
/************************************************************************/

CellSizeUnits ECWTranslateToCellSizeUnits(const char* pszUnits)
{
    if (EQUAL(pszUnits, "METERS"))
        return ECW_CELL_UNITS_METERS;
    else if (EQUAL(pszUnits, "DEGREES"))
        return ECW_CELL_UNITS_DEGREES;
    else if (EQUAL(pszUnits, "FEET"))
        return ECW_CELL_UNITS_FEET;
    else if (EQUAL(pszUnits, "UNKNOWN"))
        return ECW_CELL_UNITS_UNKNOWN;
    else if (EQUAL(pszUnits, "INVALID"))
        return ECW_CELL_UNITS_INVALID;
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Unrecognized value for UNITS : %s", pszUnits);
        return ECW_CELL_UNITS_INVALID;
    }
}

/************************************************************************/
/*                   ECWGetColorInterpretationByName()                  */
/************************************************************************/

GDALColorInterp ECWGetColorInterpretationByName(const char *pszName)
{
    if (EQUAL(pszName, NCS_BANDDESC_AllOpacity))
        return GCI_AlphaBand;
    else if (EQUAL(pszName, NCS_BANDDESC_Blue))
        return GCI_BlueBand;
    else if (EQUAL(pszName, NCS_BANDDESC_Green))
        return GCI_GreenBand;
    else if (EQUAL(pszName, NCS_BANDDESC_Red))
        return GCI_RedBand;
    else if (EQUAL(pszName, NCS_BANDDESC_Greyscale))
        return GCI_GrayIndex;
    else if (EQUAL(pszName, NCS_BANDDESC_GreyscaleOpacity))
        return GCI_AlphaBand;
    return GCI_Undefined;
}

/************************************************************************/
/*                    ECWGetColorInterpretationName()                   */
/************************************************************************/

const char* ECWGetColorInterpretationName(GDALColorInterp eColorInterpretation, int nBandNumber)
{
    const char *pszResult = nullptr;
    switch (eColorInterpretation){
    case GCI_AlphaBand:
        pszResult = NCS_BANDDESC_AllOpacity;
        break;
    case GCI_GrayIndex:
        pszResult = NCS_BANDDESC_Greyscale;
        break;
    case GCI_RedBand:
    case GCI_GreenBand:
    case GCI_BlueBand:
        pszResult = GDALGetColorInterpretationName(eColorInterpretation);
        break;
    case GCI_Undefined:
        if (nBandNumber == 0 ) {
            pszResult = "Red";
        }else if (nBandNumber == 1) {
            pszResult = "Green";
        }else if (nBandNumber == 2) {
            pszResult = "Blue";
        }
        else
        {
            pszResult = CPLSPrintf(NCS_BANDDESC_Band,nBandNumber + 1);
        }
        break;
    default:
        pszResult = CPLSPrintf(NCS_BANDDESC_Band,nBandNumber + 1);
        break;
    }
    return pszResult;
}

/************************************************************************/
/*                         ECWGetColorSpaceName()                       */
/************************************************************************/

const char* ECWGetColorSpaceName(NCSFileColorSpace colorSpace)
{
    switch (colorSpace)
    {
        case NCSCS_NONE:
            return "NONE"; break;
        case NCSCS_GREYSCALE:
            return "GREYSCALE"; break;
        case NCSCS_YUV:
            return "YUV"; break;
        case NCSCS_MULTIBAND:
            return "MULTIBAND"; break;
        case NCSCS_sRGB:
            return "RGB"; break;
        case NCSCS_YCbCr:
            return "YCbCr"; break;
        default:
            return "unrecognized";
    }
}
/************************************************************************/
/*                     ECWTranslateFromCellSizeUnits()                  */
/************************************************************************/

const char* ECWTranslateFromCellSizeUnits(CellSizeUnits eUnits)
{
    if (eUnits == ECW_CELL_UNITS_METERS)
        return "METERS";
    else if (eUnits == ECW_CELL_UNITS_DEGREES)
        return "DEGREES";
    else if (eUnits == ECW_CELL_UNITS_FEET)
        return "FEET";
    else if (eUnits == ECW_CELL_UNITS_UNKNOWN)
        return "UNKNOWN";
    else
        return "INVALID";
}

#endif /* def FRMT_ecw */

/************************************************************************/
/*                           ECWInitialize()                            */
/*                                                                      */
/*      Initialize NCS library.  We try to defer this as late as        */
/*      possible since de-initializing it seems to be expensive/slow    */
/*      on some system.                                                 */
/************************************************************************/

void ECWInitialize()

{
    CPLMutexHolder oHolder( &hECWDatasetMutex );

    if( bNCSInitialized )
        return;

#ifndef WIN32
    NCSecwInit();
#endif
    bNCSInitialized = TRUE;

/* -------------------------------------------------------------------- */
/*      This will disable automatic conversion of YCbCr to RGB by       */
/*      the toolkit.                                                    */
/* -------------------------------------------------------------------- */
    if( !CPLTestBool( CPLGetConfigOption("CONVERT_YCBCR_TO_RGB","YES") ) )
        NCSecwSetConfig(NCSCFG_JP2_MANAGE_ICC, FALSE);
#if ECWSDK_VERSION>= 50
    NCSecwSetConfig(NCSCFG_ECWP_CLIENT_HTTP_USER_AGENT,
                    "ECW GDAL Driver/" NCS_ECWJP2_FULL_VERSION_STRING_DOT_DEL);
#endif
/* -------------------------------------------------------------------- */
/*      Initialize cache memory limit.  Default is apparently 1/4 RAM.  */
/* -------------------------------------------------------------------- */
    const char *pszEcwCacheSize =
        CPLGetConfigOption("GDAL_ECW_CACHE_MAXMEM",nullptr);
    if( pszEcwCacheSize == nullptr )
        pszEcwCacheSize = CPLGetConfigOption("ECW_CACHE_MAXMEM",nullptr);

    if( pszEcwCacheSize != nullptr )
        NCSecwSetConfig(NCSCFG_CACHE_MAXMEM, (UINT32) atoi(pszEcwCacheSize) );

    /* -------------------------------------------------------------------- */
    /*      Version 3.x and 4.x of the ECWJP2 SDK did not resolve datum and */
    /*      projection to EPSG code using internal mapping.                 */
    /*      Version 5.x do so we provide means to achieve old               */
    /*      behaviour.                                                      */
    /* -------------------------------------------------------------------- */
    #if ECWSDK_VERSION >= 50
    if( CPLTestBool( CPLGetConfigOption("ECW_DO_NOT_RESOLVE_DATUM_PROJECTION","NO") ) == TRUE)
        NCSecwSetConfig(NCSCFG_PROJECTION_FORMAT, NCS_PROJECTION_ERMAPPER_FORMAT);
    #endif
/* -------------------------------------------------------------------- */
/*      Allow configuration of a local cache based on configuration     */
/*      options.  Setting the location turns things on.                 */
/* -------------------------------------------------------------------- */
    const char *pszOpt = nullptr;
    CPL_IGNORE_RET_VAL(pszOpt);

#if ECWSDK_VERSION >= 40
    pszOpt = CPLGetConfigOption( "ECWP_CACHE_SIZE_MB", nullptr );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_ECWP_CACHE_SIZE_MB, (INT32) atoi( pszOpt ) );

    pszOpt = CPLGetConfigOption( "ECWP_CACHE_LOCATION", nullptr );
    if( pszOpt )
    {
        NCSecwSetConfig( NCSCFG_ECWP_CACHE_LOCATION, pszOpt );
        NCSecwSetConfig( NCSCFG_ECWP_CACHE_ENABLED, (BOOLEAN) TRUE );
    }
#endif

/* -------------------------------------------------------------------- */
/*      Various other configuration items.                              */
/* -------------------------------------------------------------------- */
    pszOpt = CPLGetConfigOption( "ECWP_BLOCKING_TIME_MS", nullptr );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_BLOCKING_TIME_MS,
                         (NCSTimeStampMs) atoi(pszOpt) );

    // I believe 10s means we wait for complete data back from
    // ECWP almost all the time which is good for our blocking model.
    pszOpt = CPLGetConfigOption( "ECWP_REFRESH_TIME_MS", "10000" );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_REFRESH_TIME_MS,
                         (NCSTimeStampMs) atoi(pszOpt) );

    pszOpt = CPLGetConfigOption( "ECW_TEXTURE_DITHER", nullptr );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_TEXTURE_DITHER,
                         (BOOLEAN) CPLTestBool( pszOpt ) );

    pszOpt = CPLGetConfigOption( "ECW_FORCE_FILE_REOPEN", nullptr );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_FORCE_FILE_REOPEN,
                         (BOOLEAN) CPLTestBool( pszOpt ) );

    pszOpt = CPLGetConfigOption( "ECW_CACHE_MAXOPEN", nullptr );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_CACHE_MAXOPEN, (UINT32) atoi(pszOpt) );

#if ECWSDK_VERSION >= 40
    pszOpt = CPLGetConfigOption( "ECW_AUTOGEN_J2I", nullptr );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_JP2_AUTOGEN_J2I,
                         (BOOLEAN) CPLTestBool( pszOpt ) );

    pszOpt = CPLGetConfigOption( "ECW_OPTIMIZE_USE_NEAREST_NEIGHBOUR", nullptr );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_OPTIMIZE_USE_NEAREST_NEIGHBOUR,
                         (BOOLEAN) CPLTestBool( pszOpt ) );

    pszOpt = CPLGetConfigOption( "ECW_RESILIENT_DECODING", nullptr );
    if( pszOpt )
        NCSecwSetConfig( NCSCFG_RESILIENT_DECODING,
                         (BOOLEAN) CPLTestBool( pszOpt ) );
#endif
}

/************************************************************************/
/*                         GDALDeregister_ECW()                         */
/************************************************************************/

static void GDALDeregister_ECW( GDALDriver * )

{
    /* For unknown reason, this cleanup can take up to 3 seconds (see #3134) for SDK 3.3. */
    /* Not worth it */
#if ECWSDK_VERSION >= 50
#ifndef WIN32
    if( bNCSInitialized )
    {
        bNCSInitialized = FALSE;
        if( !GDALIsInGlobalDestructor() )
        {
            NCSecwShutdown();
        }
    }
#endif
#endif

    if( hECWDatasetMutex != nullptr )
    {
        CPLDestroyMutex( hECWDatasetMutex );
        hECWDatasetMutex = nullptr;
    }
}

/************************************************************************/
/*                          GDALRegister_ECW()                          */
/************************************************************************/

/* Needed for v4.3 and v5.0 */
#if !defined(NCS_ECWSDK_VERSION_STRING) && defined(NCS_ECWJP2_VERSION_STRING)
#define NCS_ECWSDK_VERSION_STRING NCS_ECWJP2_VERSION_STRING
#endif

void GDALRegister_ECW()

{
#ifdef FRMT_ecw
    if( !GDAL_CHECK_VERSION( "ECW driver" ) )
        return;

    if( GDALGetDriverByName( "ECW" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ECW" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );

    CPLString osLongName = "ERDAS Compressed Wavelets (SDK ";

#ifdef NCS_ECWSDK_VERSION_STRING
    osLongName += NCS_ECWSDK_VERSION_STRING;
#else
    osLongName += "3.x";
#endif
    osLongName += ")";

    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, osLongName );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/ecw.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ecw" );

    poDriver->pfnIdentify = ECWDataset::IdentifyECW;
    poDriver->pfnOpen = ECWDataset::OpenECW;
    poDriver->pfnUnloadDriver = GDALDeregister_ECW;
#ifdef HAVE_COMPRESS
    // The create method does not work with SDK 3.3 ( crash in
    // CNCSJP2FileView::WriteLineBIL() due to m_pFile being nullptr ).
#if ECWSDK_VERSION >= 50
    poDriver->pfnCreate = ECWCreateECW;
#endif
    poDriver->pfnCreateCopy = ECWCreateCopyECW;
#if ECWSDK_VERSION >= 50
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte UInt16" );
#else
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,  "Byte" );
#endif
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='TARGET' type='float' description='Compression Percentage' />"
"   <Option name='PROJ' type='string' description='ECW Projection Name'/>"
"   <Option name='DATUM' type='string' description='ECW Datum Name' />"

#if ECWSDK_VERSION < 40
"   <Option name='LARGE_OK' type='boolean' description='Enable compressing 500+MB files'/>"
#else
"   <Option name='ECW_ENCODE_KEY' type='string' description='OEM Compress Key from ERDAS.'/>"
"   <Option name='ECW_ENCODE_COMPANY' type='string' description='OEM Company Name.'/>"
#endif

#if ECWSDK_VERSION >= 50
"   <Option name='ECW_FORMAT_VERSION' type='integer' description='ECW format version (2 or 3).' default='2'/>"
#endif

"</CreationOptionList>" );
#else
    // In read-only mode, we support VirtualIO. This is not the case
    // for ECWCreateCopyECW().
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
#endif

    GetGDALDriverManager()->RegisterDriver( poDriver );
#endif /* def FRMT_ecw */
}

/************************************************************************/
/*                      GDALRegister_ECW_JP2ECW()                       */
/*                                                                      */
/*      This function exists so that when built as a plugin, there      */
/*      is a function that will register both drivers.                  */
/************************************************************************/

void GDALRegister_ECW_JP2ECW()

{
    GDALRegister_ECW();
    GDALRegister_JP2ECW();
}

/************************************************************************/
/*                     ECWDatasetOpenJPEG2000()                         */
/************************************************************************/
GDALDataset* ECWDatasetOpenJPEG2000(GDALOpenInfo* poOpenInfo)
{
    return ECWDataset::OpenJPEG2000(poOpenInfo);
}

/************************************************************************/
/*                        GDALRegister_JP2ECW()                         */
/************************************************************************/
void GDALRegister_JP2ECW()

{
#ifdef FRMT_ecw
    if( !GDAL_CHECK_VERSION( "JP2ECW driver" ) )
        return;

    if( GDALGetDriverByName( "JP2ECW" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "JP2ECW" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );

    CPLString osLongName = "ERDAS JPEG2000 (SDK ";

#ifdef NCS_ECWSDK_VERSION_STRING
    osLongName += NCS_ECWSDK_VERSION_STRING;
#else
    osLongName += "3.x";
#endif
    osLongName += ")";

    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, osLongName );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/jp2ecw.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "jp2" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = ECWDataset::IdentifyJPEG2000;
    poDriver->pfnOpen = ECWDataset::OpenJPEG2000;

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' description='Whether a 1-bit alpha channel should be promoted to 8-bit' default='YES'/>"
"   <Option name='OPEN_REMOTE_GML' type='boolean' description='Whether to load remote vector layers referenced by a link in a GMLJP2 v2 box' default='NO'/>"
"   <Option name='GEOREF_SOURCES' type='string' description='Comma separated list made with values INTERNAL/GMLJP2/GEOJP2/WORLDFILE/PAM/NONE that describe the priority order for georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
"</OpenOptionList>" );

#ifdef HAVE_COMPRESS
    poDriver->pfnCreate = ECWCreateJPEG2000;
    poDriver->pfnCreateCopy = ECWCreateCopyJPEG2000;
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 "
                               "Float32 "
#if ECWSDK_VERSION >= 40
    // Crashes for sure with 3.3. Didn't try other versions
                               "Float64"
#endif
                              );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='TARGET' type='float' description='Compression Percentage' />"
"   <Option name='PROJ' type='string' description='ECW Projection Name'/>"
"   <Option name='DATUM' type='string' description='ECW Datum Name' />"
"   <Option name='UNITS' type='string-select' description='ECW Projection Units'>"
"       <Value>METERS</Value>"
"       <Value>FEET</Value>"
"   </Option>"

#if ECWSDK_VERSION < 40
"   <Option name='LARGE_OK' type='boolean' description='Enable compressing 500+MB files'/>"
#else
"   <Option name='ECW_ENCODE_KEY' type='string' description='OEM Compress Key from ERDAS.'/>"
"   <Option name='ECW_ENCODE_COMPANY' type='string' description='OEM Company Name.'/>"
#endif

"   <Option name='GeoJP2' type='boolean' description='defaults to ON'/>"
"   <Option name='GMLJP2' type='boolean' description='defaults to ON'/>"
"   <Option name='GMLJP2V2_DEF' type='string' description='Definition file to describe how a GMLJP2 v2 box should be generated. If set to YES, a minimal instance will be created'/>"
"   <Option name='PROFILE' type='string-select'>"
"       <Value>BASELINE_0</Value>"
"       <Value>BASELINE_1</Value>"
"       <Value>BASELINE_2</Value>"
"       <Value>NPJE</Value>"
"       <Value>EPJE</Value>"
"   </Option>"
"   <Option name='PROGRESSION' type='string-select'>"
"       <Value>LRCP</Value>"
"       <Value>RLCP</Value>"
"       <Value>RPCL</Value>"
"   </Option>"
"   <Option name='CODESTREAM_ONLY' type='boolean' description='No JP2 wrapper'/>"
"   <Option name='NBITS' type='int' description='Bits (precision) for sub-byte files (1-7), sub-uint16 (9-15)'/>"
"   <Option name='LEVELS' type='int'/>"
"   <Option name='LAYERS' type='int'/>"
"   <Option name='PRECINCT_WIDTH' type='int'/>"
"   <Option name='PRECINCT_HEIGHT' type='int'/>"
"   <Option name='TILE_WIDTH' type='int'/>"
"   <Option name='TILE_HEIGHT' type='int'/>"
"   <Option name='INCLUDE_SOP' type='boolean'/>"
"   <Option name='INCLUDE_EPH' type='boolean'/>"
"   <Option name='DECOMPRESS_LAYERS' type='int'/>"
"   <Option name='DECOMPRESS_RECONSTRUCTION_PARAMETER' type='float'/>"
"   <Option name='WRITE_METADATA' type='boolean' description='Whether metadata should be written, in a dedicated JP2 XML box' default='NO'/>"
"   <Option name='MAIN_MD_DOMAIN_ONLY' type='boolean' description='(Only if WRITE_METADATA=YES) Whether only metadata from the main domain should be written' default='NO'/>"
"</CreationOptionList>" );
#endif

    GetGDALDriverManager()->RegisterDriver( poDriver );
#endif /* def FRMT_ecw */
}
