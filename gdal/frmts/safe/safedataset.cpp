/******************************************************************************
 *
 * Project:  Sentinel SAFE products
 * Purpose:  Sentinel Products (manifest.safe) driver
 * Author:   Delfim Rego, delfimrego@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Delfim Rego <delfimrego@gmail.com>
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

#include "safedataset.h"

#include "cpl_time.h"

#ifdef USE_OMP
  #include <omp.h>
#endif

#ifdef USE_OMP
static int GetNumThreadsToUse()
{
    unsigned int nCores = std::thread::hardware_concurrency();
    return (nCores / 2 > 1) ? nCores / 2 : 1;
}
#endif

/************************************************************************/
/*                            SAFERasterBand                            */
/************************************************************************/

SAFERasterBand::SAFERasterBand( SAFEDataset *poDSIn,
                                GDALDataType eDataTypeIn,
                                const CPLString &osSwath,
                                const CPLString &osPolarization,
                                std::unique_ptr<GDALDataset>&& poBandFileIn ) :
    poBandFile(std::move(poBandFileIn))
{
    poDS = poDSIn;
    GDALRasterBand *poSrcBand = poBandFile->GetRasterBand( 1 );
    poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    eDataType = eDataTypeIn;

    if( !osSwath.empty() )
        SetMetadataItem("SWATH", osSwath.c_str());

    if( !osPolarization.empty() )
        SetMetadataItem("POLARIZATION", osPolarization.c_str());
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAFERasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
/* -------------------------------------------------------------------- */
/*      If the last strip is partial, we need to avoid                  */
/*      over-requesting.  We also need to initialize the extra part     */
/*      of the block to zero.                                           */
/* -------------------------------------------------------------------- */
    int nRequestYSize;
    if( (nBlockYOff + 1) * nBlockYSize > nRasterYSize )
    {
        nRequestYSize = nRasterYSize - nBlockYOff * nBlockYSize;
        memset( pImage, 0, (GDALGetDataTypeSize( eDataType ) / 8) *
            nBlockXSize * nBlockYSize );
    }
    else
    {
        nRequestYSize = nBlockYSize;
    }

/*-------------------------------------------------------------------- */
/*      If the input imagery is tiled, also need to avoid over-        */
/*      requesting in the X-direction.                                 */
/* ------------------------------------------------------------------- */
    int nRequestXSize;
    if( (nBlockXOff + 1) * nBlockXSize > nRasterXSize )
    {
        nRequestXSize = nRasterXSize - nBlockXOff * nBlockXSize;
        memset( pImage, 0, (GDALGetDataTypeSize( eDataType ) / 8) *
            nBlockXSize * nBlockYSize );
    }
    else
    {
        nRequestXSize = nBlockXSize;
    }
    if( eDataType == GDT_CInt16 && poBandFile->GetRasterCount() == 2 )
        return
            poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nRequestXSize, nRequestYSize,
                                  pImage, nRequestXSize, nRequestYSize,
                                  GDT_Int16,
                                  2, nullptr, 4, nBlockXSize * 4, 2, nullptr );

/* -------------------------------------------------------------------- */
/*      File has one sample marked as sample format void, a 32bits.     */
/* -------------------------------------------------------------------- */
    else if( eDataType == GDT_CInt16 && poBandFile->GetRasterCount() == 1 )
    {
        CPLErr eErr
            = poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nRequestXSize, nRequestYSize,
                                  pImage, nRequestXSize, nRequestYSize,
                                  GDT_CInt16,
                                  1, nullptr, 4, nBlockXSize * 4, 0, nullptr );

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      The 16bit case is straight forward.  The underlying file        */
/*      looks like a 16bit unsigned data too.                           */
/* -------------------------------------------------------------------- */
    else if( eDataType == GDT_UInt16 )
        return
            poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nRequestXSize, nRequestYSize,
                                  pImage, nRequestXSize, nRequestYSize,
                                  GDT_UInt16,
                                  1, nullptr, 2, nBlockXSize * 2, 0, nullptr );

    else if( eDataType == GDT_Byte )
        return
            poBandFile->RasterIO( GF_Read,
                                  nBlockXOff * nBlockXSize,
                                  nBlockYOff * nBlockYSize,
                                  nRequestXSize, nRequestYSize,
                                  pImage, nRequestXSize, nRequestYSize,
                                  GDT_Byte,
                                  1, nullptr, 1, nBlockXSize, 0, nullptr );

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                            SAFESLCRasterBand                         */
/************************************************************************/

SAFESLCRasterBand::SAFESLCRasterBand(SAFEDataset *poDSIn, GDALDataType eDataTypeIn,
                                     const CPLString &osSwath, const CPLString &osPolarization,
                                     std::unique_ptr<GDALDataset>&& poBandFileIn, BandType eBandType)
    : poBandFile(std::move(poBandFileIn))
{
    poDS = poDSIn;
    eDataType = eDataTypeIn;
    m_eInputDataType = eDataTypeIn;
    GDALRasterBand *poSrcBand = poBandFile->GetRasterBand(1);
    poSrcBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    m_eBandType = eBandType;

    if( !osSwath.empty() )
        SetMetadataItem("SWATH", osSwath.c_str());

    if( !osPolarization.empty() )
        SetMetadataItem("POLARIZATION", osPolarization.c_str());

    // For intensity band
    if( m_eBandType == INTENSITY )
        eDataType = GDT_Float32;
    else
    // For complex bands
        eDataType = GDT_CInt16;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAFESLCRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                     void *pImage)

{
    /* -------------------------------------------------------------------- */
    /*      If the last strip is partial, we need to avoid                  */
    /*      over-requesting.  We also need to initialize the extra part     */
    /*      of the block to zero.                                           */
    /* -------------------------------------------------------------------- */
    int nRequestYSize;
    if( (nBlockYOff + 1) * nBlockYSize > nRasterYSize )
    {
        nRequestYSize = nRasterYSize - nBlockYOff * nBlockYSize;
        memset(pImage, 0,
               static_cast<size_t>(GDALGetDataTypeSizeBytes(eDataType)) * nBlockXSize *
                   nBlockYSize);
    }
    else
    {
        nRequestYSize = nBlockYSize;
    }

    /*-------------------------------------------------------------------- */
    /*      If the input imagery is tiled, also need to avoid over-        */
    /*      requesting in the X-direction.                                 */
    /* ------------------------------------------------------------------- */
    int nRequestXSize;
    if( (nBlockXOff + 1) * nBlockXSize > nRasterXSize )
    {
        nRequestXSize = nRasterXSize - nBlockXOff * nBlockXSize;
        memset(pImage, 0,
               static_cast<size_t>(GDALGetDataTypeSizeBytes(eDataType)) * nBlockXSize *
                   nBlockYSize);
    }
    else
        nRequestXSize = nBlockXSize;

    if( m_eInputDataType == GDT_CInt16 && poBandFile->GetRasterCount() == 2 )
    {
        return poBandFile->RasterIO(
            GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
            nRequestXSize, nRequestYSize, pImage, nRequestXSize, nRequestYSize,
            GDT_Int16, 2, nullptr, 4, nBlockXSize * 4, 2, nullptr);
    }
    // File has one sample marked as sample format void, a 32bits.
    else if( m_eInputDataType == GDT_CInt16 &&
             poBandFile->GetRasterCount() == 1 )
    {
        if( m_eBandType == COMPLEX )
        {
            CPLErr eErr = poBandFile->RasterIO(
                GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                nRequestXSize, nRequestYSize, pImage, nRequestXSize,
                nRequestYSize, GDT_CInt16, 1, nullptr, 4, nBlockXSize * 4, 0,
                nullptr);
            if( eErr != CE_None )
            {
                return eErr;
            }
        }
        else if( m_eBandType == INTENSITY )
        {
            GInt16 *pnImageTmp = static_cast<GInt16 *>(
                VSI_MALLOC_VERBOSE(2 * nBlockXSize * nBlockYSize *
                        GDALGetDataTypeSizeBytes(GDT_Int16)));
            if( !pnImageTmp )
            {
                return CE_Failure;
            }

            CPLErr eErr = poBandFile->RasterIO(
                GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
                nRequestXSize, nRequestYSize, pnImageTmp, nRequestXSize,
                nRequestYSize, GDT_CInt16, 1, nullptr, 4, nBlockXSize * 4, 0,
                nullptr);
            if( eErr != CE_None )
            {
                CPLFree(pnImageTmp);
                return eErr;
            }

           float *pfBuffer = static_cast<float*>(pImage);
#ifdef USE_OMP
            omp_set_num_threads(GetNumThreadsToUse());
#pragma omp parallel
#endif
            for(int i = 0; i < nBlockYSize; i++)
            {
#ifdef USE_OMP
#pragma omp for nowait
#endif
                for(int j = 0; j < nBlockXSize; j++)
                {
                    int nPixOff = (2 * (i * nBlockXSize)) + (j * 2);
                    int nOutPixOff = (i * nBlockXSize) + j;
                    pfBuffer[nOutPixOff] = static_cast<float>(static_cast<double>(pnImageTmp[nPixOff] * pnImageTmp[nPixOff]) +
                                                              static_cast<double>(pnImageTmp[nPixOff + 1] * pnImageTmp[nPixOff + 1]));
                }
            }
            CPLFree(pnImageTmp);
        }
        return CE_None;
    }

    CPLAssert(false);
    return CE_Failure;
}

/************************************************************************/
/*                            SAFECalibRasterBand                       */
/************************************************************************/

SAFECalibratedRasterBand::SAFECalibratedRasterBand(SAFEDataset *poDSIn, GDALDataType eDataTypeIn,
                                                   const CPLString &osSwath, const CPLString &osPolarization,
                                                   std::unique_ptr<GDALDataset>&& poBandDatasetIn,
                                                   const char *pszCalibrationFilename,
                                                   CalibrationType eCalibrationType) :
    poBandDataset(std::move(poBandDatasetIn))
{
    poDS = poDSIn;
    GDALRasterBand *poSrcBand = poBandDataset->GetRasterBand(1);
    poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    eDataType = eDataTypeIn;

    if( !osSwath.empty() )
        SetMetadataItem("SWATH", osSwath.c_str());

    if( !osPolarization.empty() )
        SetMetadataItem("POLARIZATION", osPolarization.c_str());

    m_osCalibrationFilename = pszCalibrationFilename;
    m_eInputDataType = eDataTypeIn;
    eDataType = GDT_Float32;
    m_eCalibrationType = eCalibrationType;
}

/************************************************************************/
/*                            ReadLUT()                                 */
/************************************************************************/
/* Read the provided LUT in to m_ndTable                                */
/************************************************************************/
bool SAFECalibratedRasterBand::ReadLUT()
{
    const char * const papszCalibrationNodes[3] = {
                                             "=calibrationVector.sigmaNought",
                                             "=calibrationVector.betaNought",
                                             "=calibrationVector.gamma" };
    CPLString osCalibrationNodeName = papszCalibrationNodes[m_eCalibrationType];
    const char * pszEndSpace = " ";
    CPLString osStartTime, osEndTime;
    CPLXMLNode *poLUT = CPLParseXMLFile(m_osCalibrationFilename);
    if( !poLUT )
        return false;

    CPLString osParseLUT;
    CPLString osParsePixelLUT;
    CPLString osParseAzimuthLUT;
    CPLString osParseLineNoLUT;
    for( CPLXMLNode *psNode = poLUT; psNode != nullptr; )
    {
        if( psNode->psNext != nullptr )
            psNode = psNode->psNext;

        if( EQUAL( psNode->pszValue, "calibration" ) )
        {
            for( psNode = psNode->psChild; psNode != nullptr; )
            {
                if( EQUAL( psNode->pszValue, "adsHeader" ) )
                {
                    osStartTime = CPLGetXMLValue( psNode, "=adsHeader.startTime", " " );
                    osEndTime = CPLGetXMLValue( psNode, "=adsHeader.stopTime", " " );
                }

                if( psNode->psNext != nullptr )
                    psNode = psNode->psNext;

                if( EQUAL( psNode->pszValue, "calibrationVectorList" ) )
                {
                    for( psNode = psNode->psChild; psNode != nullptr; psNode = psNode->psNext )
                    {
                        if( EQUAL( psNode->pszValue, "calibrationVector" ) )
                        {
                            osParseAzimuthLUT += CPLGetXMLValue( psNode, "=calibrationVector.azimuthTime", " " );
                            osParseAzimuthLUT += pszEndSpace;
                            osParseLineNoLUT += CPLGetXMLValue( psNode, "=calibrationVector.line", " " );
                            osParseLineNoLUT += pszEndSpace;
                            osParsePixelLUT = CPLGetXMLValue( psNode, "pixel", " " );
                            m_nNumPixels = static_cast<int>(CPLAtof(CPLGetXMLValue(psNode, "pixel.count", " ")));
                            osParseLUT += CPLGetXMLValue(psNode, osCalibrationNodeName, " " );
                            osParseLUT += pszEndSpace;
                        }
                    }
                }
            }
        }
    }
    CPLDestroyXMLNode(poLUT);

    osParsePixelLUT += pszEndSpace;

    CPLStringList oStartTimeList(CSLTokenizeString2(osStartTime, " ", CSLT_HONOURSTRINGS));
    if( !oStartTimeList.size() )
        return false;
    m_oStartTimePoint = getTimePoint(oStartTimeList[0]);
    CPLStringList oEndTimeList(CSLTokenizeString2(osEndTime, " ", CSLT_HONOURSTRINGS));
    if( !oEndTimeList.size() )
        return false;
    m_oStopTimePoint = getTimePoint(oEndTimeList[0]);
    m_oAzimuthList.Assign(CSLTokenizeString2(osParseAzimuthLUT, " ", CSLT_HONOURSTRINGS));
    CPLStringList oLUTList(CSLTokenizeString2(osParseLUT, " ", CSLT_HONOURSTRINGS));
    CPLStringList oPixelList(CSLTokenizeString2(osParsePixelLUT, " ", CSLT_HONOURSTRINGS));
    CPLStringList oLineNoList(CSLTokenizeString2(osParseLineNoLUT, " ", CSLT_HONOURSTRINGS));

    m_anPixelLUT.resize(m_nNumPixels);
    for( int i = 0; i < m_nNumPixels; i++ )
        m_anPixelLUT[i] = static_cast<int>(CPLAtof(oPixelList[i]));

    int nTableSize = oLUTList.size();
    m_afTable.resize(nTableSize);
    for( int i = 0; i < nTableSize; i++ )
        m_afTable[i] = static_cast<float>(CPLAtof(oLUTList[i]));

    int nLineListSize = oLineNoList.size();
    m_anLineLUT.resize(nLineListSize);
    for( int i = 0; i < nLineListSize; i++ )
        m_anLineLUT[i] = static_cast<int>(CPLAtof(oLineNoList[i]));

    return true;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SAFECalibratedRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                            void * pImage)

{
    /* -------------------------------------------------------------------- */
    /*      If the last strip is partial, we need to avoid                  */
    /*      over-requesting.  We also need to initialize the extra part     */
    /*      of the block to zero.                                           */
    /* -------------------------------------------------------------------- */
    int nRequestYSize;
    if ( ( nBlockYOff + 1 ) * nBlockYSize > nRasterYSize )
    {
        nRequestYSize = nRasterYSize - nBlockYOff * nBlockYSize;
        memset( pImage, 0, static_cast<size_t>(GDALGetDataTypeSizeBytes(eDataType)) *
                nBlockXSize * nBlockYSize );
    }
    else
    {
        nRequestYSize = nBlockYSize;
    }

    // Check LUT values and fail before reading
    int nLineCalVecIdx = getCalibrationVectorIndex(nBlockYOff);
    const char *pszVec0Str = m_oAzimuthList[nLineCalVecIdx];
    const char *pszVec1Str = m_oAzimuthList[nLineCalVecIdx + 1];
    if( ((m_eInputDataType == GDT_CInt16) || (m_eInputDataType == GDT_Int16)) &&
        (!pszVec0Str || !pszVec1Str) )
        return CE_Failure;

    /*-------------------------------------------------------------------- */
    /*      If the input imagery is tiled, also need to avoid over-        */
    /*      requesting in the X-direction.                                 */
    /* ------------------------------------------------------------------- */
    int nRequestXSize;
    if( ( nBlockXOff + 1 ) * nBlockXSize > nRasterXSize )
    {
        nRequestXSize = nRasterXSize - nBlockXOff * nBlockXSize;
        memset( pImage, 0, static_cast<size_t>(GDALGetDataTypeSizeBytes(eDataType)) *
                nBlockXSize * nBlockYSize );
    }
    else
    {
        nRequestXSize = nBlockXSize;
    }

    TimePoint azTime = getazTime( m_oStartTimePoint,
                                      m_oStopTimePoint,
                                      nRasterYSize,
                                      nBlockYOff );
    TimePoint oVec0Time = getTimePoint( pszVec0Str );
    TimePoint oVec1Time = getTimePoint( pszVec1Str );
    double dfMuY = getTimeDiff(oVec0Time, azTime) / getTimeDiff(oVec0Time, oVec1Time);

    if( m_eInputDataType == GDT_CInt16 )
    {
        CPLErr eErr = CE_None;
        GInt16 *pnImageTmp
            = static_cast<GInt16 *>(
                                VSI_MALLOC_VERBOSE( 2 * nBlockXSize * nBlockYSize
                                * GDALGetDataTypeSizeBytes( GDT_Int16 ) ) );
        if( !pnImageTmp )
            return CE_Failure;

        if( poBandDataset->GetRasterCount() == 2 )
        {
            eErr = poBandDataset->RasterIO( GF_Read,
                                         nBlockXOff * nBlockXSize,
                                         nBlockYOff * nBlockYSize,
                                         nRequestXSize, nRequestYSize,
                                         pnImageTmp, nRequestXSize, nRequestYSize,
                                         GDT_Int16,
                                         2, nullptr, 4, nBlockXSize * 4, 2, nullptr );
        }
        /* -------------------------------------------------------------------- */
        /*      File has one sample marked as sample format void, a 32bits.     */
        /* -------------------------------------------------------------------- */
        else if( poBandDataset->GetRasterCount() == 1 )
        {
            eErr = poBandDataset->RasterIO( GF_Read,
                                         nBlockXOff * nBlockXSize,
                                         nBlockYOff * nBlockYSize,
                                         nRequestXSize, nRequestYSize,
                                         pnImageTmp, nRequestXSize, nRequestYSize,
                                         GDT_CInt16,
                                         1, nullptr, 4, nBlockXSize * 4, 0, nullptr );
        }

        //Interpolation of LUT Value
#ifdef USE_OMP
        omp_set_num_threads(GetNumThreadsToUse());
#pragma omp parallel
#endif
        for (int i = 0; i < nBlockYSize; i++)
        {
#ifdef USE_OMP
#pragma omp for nowait
#endif
            for (int j = 0; j < nBlockXSize; j++)
            {
                int nPixOff = (2 * (i * nBlockXSize)) + (j * 2);
                int nOutPixOff = (i * nBlockXSize) + j;
                int nPixelCalvecIdx = getPixelIndex(j);
                double dfMuX = ( double ) (j - m_anPixelLUT[nPixelCalvecIdx]) /
                               ( double ) (m_anPixelLUT[nPixelCalvecIdx + 1] -
                                m_anPixelLUT[nPixelCalvecIdx]);
                int lutIdx1 = ( nLineCalVecIdx * m_nNumPixels ) + nPixelCalvecIdx;
                int lutIdx2 = ( nLineCalVecIdx * m_nNumPixels ) + ( nPixelCalvecIdx + 1 );
                int lutIdx3 = ( ( nLineCalVecIdx + 1 ) * m_nNumPixels ) + nPixelCalvecIdx;
                int lutIdx4 = ( ( nLineCalVecIdx + 1 ) * m_nNumPixels ) + ( nPixelCalvecIdx + 1 );
                double dfLutValue = ( ( 1 - dfMuY ) * ( ( ( 1 - dfMuX ) * m_afTable[lutIdx1] )
                                    + ( dfMuX * m_afTable[lutIdx2] ) ) )
                                    + ( dfMuY * ( ( ( 1 - dfMuX ) * m_afTable[lutIdx3] )
                                    + ( dfMuX * m_afTable[lutIdx4] ) ) );
                double dfNum = static_cast<double>( ( pnImageTmp[nPixOff] * pnImageTmp[nPixOff] )
                                                  + (pnImageTmp[nPixOff + 1] * pnImageTmp[nPixOff + 1] ) );
                double dfCalibValue = dfNum / ( dfLutValue * dfLutValue );
                ( ( float * )pImage )[nOutPixOff] = ( float )dfCalibValue;
            }
        }
        CPLFree(pnImageTmp);
        return eErr;
    }
    else if ( m_eInputDataType == GDT_UInt16 )
    {
        CPLErr eErr = CE_None;
        GUInt16 *pnImageTmp = static_cast<GUInt16 *>(
             VSI_MALLOC_VERBOSE( nBlockXSize * nBlockYSize
                                 * GDALGetDataTypeSizeBytes( GDT_UInt16 ) ) );
        if( !pnImageTmp )
            return CE_Failure;
        eErr = poBandDataset->RasterIO( GF_Read,
                                     nBlockXOff * nBlockXSize,
                                     nBlockYOff * nBlockYSize,
                                     nRequestXSize, nRequestYSize,
                                     pnImageTmp, nRequestXSize, nRequestYSize,
                                     GDT_UInt16,
                                     1, nullptr, 2, nBlockXSize * 2, 0, nullptr );

#ifdef USE_OMP
        omp_set_num_threads(GetNumThreadsToUse());
#pragma omp parallel
#endif
        for (int i = 0; i < nBlockYSize; i++)
        {
#ifdef USE_OMP
#pragma omp for nowait
#endif
            for (int j = 0; j < nBlockXSize; j++)
            {
                int nPixOff = ( i * nBlockXSize ) + j;
                int nPixelCalvecIdx = getPixelIndex(j);
                double dfMuX = ( double ) ( j - m_anPixelLUT[nPixelCalvecIdx] ) /
                               ( double ) ( m_anPixelLUT[nPixelCalvecIdx + 1] -
                                 m_anPixelLUT[nPixelCalvecIdx] );
                int lutIdx1 = ( nLineCalVecIdx * m_nNumPixels ) + nPixelCalvecIdx;
                int lutIdx2 = ( nLineCalVecIdx * m_nNumPixels ) + ( nPixelCalvecIdx + 1 );
                int lutIdx3 = ( ( nLineCalVecIdx + 1 ) * m_nNumPixels ) + ( nPixelCalvecIdx );
                int lutIdx4 = ( ( nLineCalVecIdx + 1 ) * m_nNumPixels ) + ( nPixelCalvecIdx + 1 );
                double dfLutValue = ( ( 1 - dfMuY ) * ( ( ( 1 - dfMuX ) * m_afTable[lutIdx1] )
                                    + ( dfMuX * m_afTable[lutIdx2] ) ) )
                                    + ( dfMuY * ( ( ( 1 - dfMuX ) * m_afTable[lutIdx3] )
                                    + ( dfMuX * m_afTable[lutIdx4] ) ) );
                double dfCalibValue = ( double ) ( pnImageTmp[nPixOff] * pnImageTmp[nPixOff] )
                                        / (dfLutValue * dfLutValue);
                ( ( float * )pImage )[nPixOff] = ( float )dfCalibValue;
            }
        }
        CPLFree( pnImageTmp );
        return eErr;
    }
    else if( eDataType == GDT_Byte ) // Check if this is required.
        return poBandDataset->RasterIO(
            GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize,
            nRequestXSize, nRequestYSize, pImage, nRequestXSize, nRequestYSize,
            GDT_Byte, 1, nullptr, 1, nBlockXSize, 0, nullptr);

    CPLAssert( false );
    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                              SAFEDataset                              */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            ~SAFEDataset()                            */
/************************************************************************/

SAFEDataset::~SAFEDataset()

{
    SAFEDataset::FlushCache(true);

    CPLDestroyXMLNode( psManifest );
    CPLFree( pszProjection );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    SAFEDataset::CloseDependentDatasets();

    CSLDestroy( papszSubDatasets );
    CSLDestroy( papszExtraFiles );
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int SAFEDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if( nBands != 0 )
        bHasDroppedRef = TRUE;

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                      GetMetaDataObject()                             */
/************************************************************************/

CPLXMLNode * SAFEDataset::GetMetaDataObject(
    CPLXMLNode *psMetaDataObjects, const char *metadataObjectId)
{
/* -------------------------------------------------------------------- */
/*      Look for DataObject Element by ID.                              */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode *psMDO = psMetaDataObjects->psChild;
         psMDO != nullptr;
         psMDO = psMDO->psNext )
    {
        if( psMDO->eType != CXT_Element
            || !(EQUAL(psMDO->pszValue,"metadataObject")) ) {
            continue;
        }

        const char *pszElementID = CPLGetXMLValue( psMDO, "ID", "" );

        if (EQUAL(pszElementID, metadataObjectId)) {
            return psMDO;
        }
    }

    CPLError( CE_Warning, CPLE_AppDefined,
              "MetadataObject not found with ID=%s",
              metadataObjectId);

    return nullptr;
}

/************************************************************************/
/*                      GetDataObject()                                 */
/************************************************************************/

CPLXMLNode * SAFEDataset::GetDataObject(
    CPLXMLNode *psDataObjects, const char *dataObjectId)
{
/* -------------------------------------------------------------------- */
/*      Look for DataObject Element by ID.                              */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode *psDO = psDataObjects->psChild;
         psDO != nullptr;
         psDO = psDO->psNext )
    {
        if( psDO->eType != CXT_Element
            || !(EQUAL(psDO->pszValue,"dataObject")) ) {
            continue;
        }

        const char *pszElementID = CPLGetXMLValue( psDO, "ID", "" );

        if (EQUAL(pszElementID, dataObjectId)) {
            return psDO;
        }
    }

    CPLError( CE_Warning, CPLE_AppDefined,
              "DataObject not found with ID=%s",
              dataObjectId);

    return nullptr;
}

CPLXMLNode * SAFEDataset::GetDataObject(
    CPLXMLNode *psMetaDataObjects, CPLXMLNode *psDataObjects,
    const char *metadataObjectId)
{
/* -------------------------------------------------------------------- */
/*      Look for MetadataObject Element by ID.                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMDO = SAFEDataset::GetMetaDataObject(
            psMetaDataObjects, metadataObjectId);

    if (psMDO!=nullptr) {
        const char *dataObjectId = CPLGetXMLValue(
            psMDO, "dataObjectPointer.dataObjectID", "" );
        if( *dataObjectId != '\0' ) {
            return SAFEDataset::GetDataObject(psDataObjects, dataObjectId);
        }
    }

    CPLError( CE_Warning, CPLE_AppDefined,
              "DataObject not found with MetaID=%s",
              metadataObjectId);

    return nullptr;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **SAFEDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    papszFileList = CSLInsertStrings( papszFileList, -1, papszExtraFiles );

    return papszFileList;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int SAFEDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    /* Check for the case where we're trying to read the calibrated data: */
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL1_CALIB:")) {
        return TRUE;
    }

    /* Check for directory access when there is a manifest.safe file in the
       directory. */
    if( poOpenInfo->bIsDirectory )
    {
        VSIStatBufL sStat;
        CPLString osMDFilename =
            CPLFormCIFilename( poOpenInfo->pszFilename, "manifest.safe", nullptr );

        if( VSIStatL(osMDFilename, &sStat) == 0 && VSI_ISREG(sStat.st_mode) )
        {
            GDALOpenInfo oOpenInfo(osMDFilename, GA_ReadOnly, nullptr);
            return Identify(&oOpenInfo);
        }
        return FALSE;
    }

    /* otherwise, do our normal stuff */
    if( !EQUAL(CPLGetFilename(poOpenInfo->pszFilename), "manifest.safe") )
        return FALSE;

    if( poOpenInfo->nHeaderBytes < 100 )
        return FALSE;

    if( strstr((const char *) poOpenInfo->pabyHeader, "<xfdu:XFDU" ) == nullptr )
        return FALSE;

    // This driver doesn't handle Sentinel-2 data
    if( strstr((const char *) poOpenInfo->pabyHeader, "sentinel-2" ) != nullptr )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SAFEDataset::Open( GDALOpenInfo * poOpenInfo )

{

    // Is this a SENTINEL-1 manifest.safe definition?
    if ( !SAFEDataset::Identify( poOpenInfo ) ) {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*        Get subdataset information, if relevant                       */
/* -------------------------------------------------------------------- */
    CPLString osMDFilename;
    bool bIsSubDS = false;
    bool bIsSLC = false;
    //Subdataset 1st level selection (ex: for swath selection)
    CPLString osSelectedSubDS1;
    //Subdataset 2nd level selection (ex: for polarization selection)
    CPLString osSelectedSubDS2;
    CPLString osSelectedSubDS3;
    //0 for SIGMA , 1 for BETA, 2 for GAMMA and # for UNCALIB dataset
    SAFECalibratedRasterBand::CalibrationType eCalibrationType = SAFECalibratedRasterBand::SIGMA_NOUGHT;
    bool bCalibrated = false;
    //0 for amplitude, 1 for complex (2 band : I , Q) and 2 for INTENSITY
    typedef enum { UNKNOWN = -1, AMPLITUDE, COMPLEX, INTENSITY } RequestDataType;
    RequestDataType eRequestType = UNKNOWN;
    //Calibration Information selection
    CPLString osSubdatasetName;

    if(STARTS_WITH_CI(poOpenInfo->pszFilename, "SENTINEL1_CALIB:"))
    {
        bIsSubDS = true;
        osMDFilename = poOpenInfo->pszFilename + strlen("SENTINEL1_CALIB:");
        const char *pszSelectionCalib = strchr(osMDFilename.c_str(), ':');
        if( pszSelectionCalib == nullptr ||
            pszSelectionCalib == osMDFilename.c_str() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid syntax for SENTINEL1_CALIB:");
            return nullptr;
        }

        CPLString osCalibrationValue = osMDFilename;
        osCalibrationValue.resize(pszSelectionCalib - osMDFilename.c_str());
        osMDFilename = pszSelectionCalib + strlen(":");
        if(EQUAL(osCalibrationValue.c_str(), "UNCALIB"))
        {
            bCalibrated = false;
        }
        else if(EQUAL(osCalibrationValue.c_str(), "SIGMA0"))
        {
            bCalibrated = true;
            eCalibrationType = SAFECalibratedRasterBand::SIGMA_NOUGHT;
        }
        else if(EQUAL(osCalibrationValue.c_str(), "BETA0"))
        {
            bCalibrated = true;
            eCalibrationType = SAFECalibratedRasterBand::BETA_NOUGHT;
        }
        else if(EQUAL(osCalibrationValue.c_str(), "GAMMA"))
        {
            bCalibrated = true;
            eCalibrationType = SAFECalibratedRasterBand::GAMMA;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid syntax for SENTINEL1_CALIB:");
            return nullptr;
        }

        const auto nSelectionUnitPos = osMDFilename.rfind(':');
        if(nSelectionUnitPos == std::string::npos || nSelectionUnitPos == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid syntax for SENTINEL1_CALIB:");
            return nullptr;
        }

        CPLString osUnitValue = osMDFilename.substr(nSelectionUnitPos + 1);
        osMDFilename.resize(nSelectionUnitPos);
        if(EQUAL(osUnitValue.c_str(), "AMPLITUDE"))
            eRequestType = AMPLITUDE;
        else if(EQUAL(osUnitValue.c_str(), "COMPLEX"))
            eRequestType = COMPLEX;
        else if(EQUAL(osUnitValue.c_str(), "INTENSITY"))
            eRequestType = INTENSITY;
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid syntax for SENTINEL1_CALIB:");
            return nullptr;
        }

        const auto nSelection1Pos = osMDFilename.rfind(':');
        if(nSelection1Pos == std::string::npos || nSelection1Pos == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid syntax for SENTINEL1_CALIB:");
            return nullptr;
        }
        osSelectedSubDS1 = osMDFilename.substr(nSelection1Pos + 1);
        osMDFilename.resize(nSelection1Pos);

        const auto nSelection2Pos = osSelectedSubDS1.find('_');
        if(nSelection2Pos != std::string::npos && nSelection2Pos != 0)
        {
            osSelectedSubDS2 = osSelectedSubDS1.substr(nSelection2Pos+1);
            osSelectedSubDS1.resize(nSelection2Pos);
            const auto nSelection3Pos = osSelectedSubDS2.find('_');
            if(nSelection3Pos != std::string::npos && nSelection3Pos != 0)
            {
                osSelectedSubDS3 = osSelectedSubDS2.substr(nSelection3Pos+1);
                osSelectedSubDS2.resize(nSelection3Pos);
            }
        }

        // update directory check:
        VSIStatBufL sStat;
        if(VSIStatL(osMDFilename.c_str(), &sStat) == 0)
            poOpenInfo->bIsDirectory = VSI_ISDIR(sStat.st_mode);
        if( !bCalibrated )
            osSubdatasetName = "UNCALIB";
        else if( eCalibrationType == SAFECalibratedRasterBand::SIGMA_NOUGHT )
            osSubdatasetName = "SIGMA0";
        else if( eCalibrationType == SAFECalibratedRasterBand::BETA_NOUGHT )
            osSubdatasetName = "BETA0";
        else if( eCalibrationType == SAFECalibratedRasterBand::GAMMA )
            osSubdatasetName = "GAMMA";
        osSubdatasetName += ":";
        if( !osUnitValue.empty() )
        {
            osSubdatasetName += osUnitValue;
            osSubdatasetName += ":";
        }
        if( !osSelectedSubDS1.empty() )
        {
            osSubdatasetName += osSelectedSubDS1;
            osSubdatasetName += ":";
        }
        if( !osSelectedSubDS2.empty() )
        {
            osSubdatasetName += osSelectedSubDS2;
            osSubdatasetName += ":";
        }
        if( !osSelectedSubDS3.empty() )
        {
            osSubdatasetName += osSelectedSubDS3;
            osSubdatasetName += ":";
        }
        if( !osSubdatasetName.empty() )
        {
            if( osSubdatasetName.back() == ':' )
                osSubdatasetName.pop_back();
        }
    }
    else
    {
        osMDFilename = poOpenInfo->pszFilename;
    }

    if( poOpenInfo->bIsDirectory )
    {
        osMDFilename =
            CPLFormCIFilename(osMDFilename.c_str(), "manifest.safe", nullptr);
    }

/* -------------------------------------------------------------------- */
/*      Ingest the manifest.safe file.                                  */
/* -------------------------------------------------------------------- */

    CPLXMLNode *psManifest = CPLParseXMLFile(osMDFilename);
    if( psManifest == nullptr )
        return nullptr;

    CPLString osPath(CPLGetPath(osMDFilename));

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLDestroyXMLNode(psManifest);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The SAFE driver does not support update access to existing"
                 " datasets.\n");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Get contentUnit parent element.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psContentUnits = CPLGetXMLNode(
            psManifest,
            "=xfdu:XFDU.informationPackageMap.xfdu:contentUnit" );
    if( psContentUnits == nullptr )
    {
        CPLDestroyXMLNode(psManifest);
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to find <xfdu:XFDU><informationPackageMap>"
                 "<xfdu:contentUnit> in manifest file.");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Get Metadata Objects element.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMetaDataObjects
        = CPLGetXMLNode( psManifest, "=xfdu:XFDU.metadataSection" );
    if( psMetaDataObjects == nullptr )
    {
        CPLDestroyXMLNode(psManifest);
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to find <xfdu:XFDU><metadataSection>"
                 "in manifest file.");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Get Data Objects element.                                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDataObjects
        = CPLGetXMLNode( psManifest, "=xfdu:XFDU.dataObjectSection" );
    if( psDataObjects == nullptr )
    {
        CPLDestroyXMLNode(psManifest);
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to find <xfdu:XFDU><dataObjectSection> in document.");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    auto poDS = cpl::make_unique<SAFEDataset>();

    poDS->psManifest = psManifest;

/* -------------------------------------------------------------------- */
/*      Look for "Measurement Data Unit" contentUnit elements.          */
/* -------------------------------------------------------------------- */
    //Map with all measures aggregated by swath
    std::map<CPLString, std::set<CPLString> > oMapSwaths2Pols;
    std::vector<CPLString> oImageNumberSwPol;
    bool isWave = false;

    for( CPLXMLNode *psContentUnit = psContentUnits->psChild;
         psContentUnit != nullptr;
         psContentUnit = psContentUnit->psNext )
    {
        if( psContentUnit->eType != CXT_Element
            || !(EQUAL(psContentUnit->pszValue,"xfdu:contentUnit")) ) {
            continue;
        }

        const char *pszUnitType = CPLGetXMLValue( psContentUnit,
                "unitType", "" );

        const char *pszAnnotation  = nullptr;
        const char *pszCalibration = nullptr;
        const char *pszMeasurement = nullptr;

        if ( EQUAL(pszUnitType, "Measurement Data Unit") ) {
            /* Get dmdID and dataObjectID */
            const char *pszDmdID = CPLGetXMLValue(psContentUnit, "dmdID", "");
            const char *pszDataObjectID = CPLGetXMLValue(
                psContentUnit,
                "dataObjectPointer.dataObjectID", "" );
            if( *pszDataObjectID == '\0' || *pszDmdID == '\0' )
                continue;

            CPLXMLNode *psDataObject = SAFEDataset::GetDataObject(
                    psDataObjects, pszDataObjectID);

            const char *pszRepId = CPLGetXMLValue( psDataObject, "repID", "" );
            if ( !EQUAL(pszRepId, "s1Level1MeasurementSchema") )
                continue;

            pszMeasurement = CPLGetXMLValue(
                    psDataObject, "byteStream.fileLocation.href", "");
            if( *pszMeasurement == '\0' )
                continue;

            char** papszTokens = CSLTokenizeString2( pszDmdID, " ",
                CSLT_ALLOWEMPTYTOKENS | CSLT_STRIPLEADSPACES
                | CSLT_STRIPENDSPACES );

            for( int j = 0; j < CSLCount( papszTokens ); j++ )
            {
                const char* pszId = papszTokens[j];
                if( *pszId == '\0' )
                    continue;

                //Map the metadata ID to the object element
                CPLXMLNode *psDO = SAFEDataset::GetDataObject(
                        psMetaDataObjects, psDataObjects, pszId);
                if( psDO == nullptr )
                    continue;

                //check object type
                pszRepId = CPLGetXMLValue( psDO, "repID", "" );
                if( EQUAL(pszRepId, "s1Level1ProductSchema") )
                {
                    /* Get annotation filename */
                    pszAnnotation = CPLGetXMLValue(
                            psDO, "byteStream.fileLocation.href", "");
                    if( *pszAnnotation == '\0' )
                        continue;
                }
                else if( EQUAL(pszRepId, "s1Level1CalibrationSchema") )
                {
                    pszCalibration = CPLGetXMLValue(
                            psDO, "byteStream.fileLocation.href", "");
                    if( *pszCalibration == '\0' )
                        continue;
                }
                else
                {
                    continue;
                }
            }

            CSLDestroy(papszTokens);

            if( pszAnnotation == nullptr || pszCalibration == nullptr )
                continue;

            //open Annotation XML file
            const CPLString osAnnotationFilePath =
                CPLFormFilename(osPath, pszAnnotation, nullptr);
            const CPLString osCalibrationFilePath =
                CPLFormFilename(osPath, pszCalibration, nullptr);

            CPLXMLTreeCloser psAnnotation(CPLParseXMLFile(osAnnotationFilePath));
            if( psAnnotation.get() == nullptr )
                continue;
            CPLXMLTreeCloser psCalibration(CPLParseXMLFile(osCalibrationFilePath));
            if( psCalibration.get() == nullptr )
                continue;

            /* --------------------------------------------------------------------
             */
            /*      Get overall image information. */
            /* --------------------------------------------------------------------
             */
            const CPLString osProductType = CPLGetXMLValue(
                psAnnotation.get(), "=product.adsHeader.productType", "UNK");
            const CPLString osMissionId = CPLGetXMLValue(
                psAnnotation.get(), "=product.adsHeader.missionId", "UNK");
            const CPLString osPolarization = CPLGetXMLValue(
                psAnnotation.get(), "=product.adsHeader.polarisation", "UNK");
            const CPLString osMode = CPLGetXMLValue(
                psAnnotation.get(), "=product.adsHeader.mode", "UNK");
            const CPLString osSwath = CPLGetXMLValue(
                psAnnotation.get(), "=product.adsHeader.swath", "UNK");
            const CPLString osImageNumber = CPLGetXMLValue(
                psAnnotation.get(), "=product.adsHeader.imageNumber", "UNK");

            oMapSwaths2Pols[osSwath].insert(osPolarization);
            oImageNumberSwPol.push_back(osImageNumber + " " + osSwath
                                                + " " + osPolarization);
            if( EQUAL(osMode.c_str(),"WV") )
                isWave = true;

            if( EQUAL(osProductType.c_str(), "SLC") )
              bIsSLC = true;

            //if the dataunit is amplitude or complex and there is calibration applied it's not possible as calibrated datasets are intensity.
            if( eRequestType != INTENSITY && bCalibrated )
              continue;

            if( osSelectedSubDS1.empty() )
            {
                // If not subdataset was selected,
                // open the first one we can find.
                osSelectedSubDS1 = osSwath;
            }

            if( osSelectedSubDS3.empty() && isWave )
            {
                //If the selected mode is Wave mode (different file structure)
                //open the first vignette in the dataset.
                osSelectedSubDS3 = osImageNumber;
            }
            if( !EQUAL(osSelectedSubDS1.c_str(), osSwath.c_str()) ) {
                //do not mix swath, otherwise it does not work for SLC products
                continue;
            }

            if( !osSelectedSubDS2.empty()
                && (osSelectedSubDS2.find(osPolarization) == std::string::npos) )
            {
                // Add only selected polarizations.
                continue;
            }

            if( !osSelectedSubDS3.empty()
                && !EQUAL(osSelectedSubDS3.c_str(), osImageNumber.c_str()) )
            {
                //Add only selected image number (for Wave)
                continue;
            }

            //Changed the position of this code segment till nullptr
            poDS->nRasterXSize =
                atoi(CPLGetXMLValue( psAnnotation.get(),
                    "=product.imageAnnotation.imageInformation.numberOfSamples",
                    "-1" ));
            poDS->nRasterYSize =
                atoi(CPLGetXMLValue( psAnnotation.get(),
                    "=product.imageAnnotation.imageInformation.numberOfLines",
                    "-1" ));
            if( poDS->nRasterXSize <= 1 || poDS->nRasterYSize <= 1 ) {
                CPLError(
                    CE_Failure, CPLE_OpenFailed,
                    "Non-sane raster dimensions provided in manifest.safe. "
                    "If this is a valid SENTINEL-1 scene, please contact your "
                    "data provider for a corrected dataset.");
                return nullptr;
            }

            poDS->SetMetadataItem("PRODUCT_TYPE", osProductType.c_str());
            poDS->SetMetadataItem("MISSION_ID", osMissionId.c_str());
            poDS->SetMetadataItem("MODE", osMode.c_str());
            poDS->SetMetadataItem("SWATH", osSwath.c_str());

            /* -------------------------------------------------------------------- */
            /*      Get dataType (so we can recognize complex data), and the        */
            /*      bitsPerSample.                                                  */
            /* -------------------------------------------------------------------- */

            const char *pszDataType = CPLGetXMLValue(
                psAnnotation.get(),
                "=product.imageAnnotation.imageInformation.outputPixels", "");

            GDALDataType eDataType;
            if( EQUAL(pszDataType, "16 bit Signed Integer") )
                eDataType = GDT_CInt16;
            else if( EQUAL(pszDataType, "16 bit Unsigned Integer") )
                eDataType = GDT_UInt16;
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "dataType=%s: not a supported configuration.",
                         pszDataType);
                return nullptr;
            }

            /* Extract pixel spacing information */
            const char *pszPixelSpacing = CPLGetXMLValue(
                psAnnotation.get(),
                "=product.imageAnnotation.imageInformation.rangePixelSpacing",
                "UNK" );
            poDS->SetMetadataItem( "PIXEL_SPACING", pszPixelSpacing );

            const char *pszLineSpacing = CPLGetXMLValue(
                psAnnotation.get(),
                "=product.imageAnnotation.imageInformation.azimuthPixelSpacing",
                "UNK" );
            poDS->SetMetadataItem( "LINE_SPACING", pszLineSpacing );

/* -------------------------------------------------------------------- */
/*      Form full filename (path of manifest.safe + measurement file).  */
/* -------------------------------------------------------------------- */
            const std::string osFullFilename(
                CPLFormFilename( osPath, pszMeasurement, nullptr ));

/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
            auto poBandFile = std::unique_ptr<GDALDataset>(GDALDataset::Open(osFullFilename.c_str()));
            if( poBandFile == nullptr )
            {
                // NOP
            }
            else if( poBandFile->GetRasterCount() == 0 )
            {
                poBandFile.reset();
            }
            else
            {
                poDS->papszExtraFiles = CSLAddString( poDS->papszExtraFiles,
                                                      osAnnotationFilePath );
                poDS->papszExtraFiles = CSLAddString( poDS->papszExtraFiles,
                                                      osCalibrationFilePath );
                poDS->papszExtraFiles = CSLAddString( poDS->papszExtraFiles,
                                                      osFullFilename.c_str() );
                /* -------------------------------------------------------------------- */
                /*      Collect Annotation Processing Information                       */
                /* -------------------------------------------------------------------- */
                CPLXMLNode *psProcessingInfo =
                    CPLGetXMLNode(psAnnotation.get(),
                    "=product.imageAnnotation.processingInformation");

                if (psProcessingInfo != nullptr) {
                    OGRSpatialReference oLL, oPrj;

                    const char *pszEllipsoidName = CPLGetXMLValue(
                        psProcessingInfo, "ellipsoidName", "");
                    const double minor_axis = CPLAtof(CPLGetXMLValue(
                        psProcessingInfo, "ellipsoidSemiMinorAxis", "0.0"));
                    const double major_axis = CPLAtof(CPLGetXMLValue(
                        psProcessingInfo, "ellipsoidSemiMajorAxis", "0.0"));

                    if( EQUAL(pszEllipsoidName, "") || (minor_axis == 0.0) ||
                        (major_axis == 0.0) )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined, "Warning- incomplete"
                            " ellipsoid information.  Using wgs-84 parameters.\n");
                        oLL.SetWellKnownGeogCS("WGS84");
                        oPrj.SetWellKnownGeogCS("WGS84");
                    }
                    else if( EQUAL(pszEllipsoidName, "WGS84") )
                    {
                        oLL.SetWellKnownGeogCS("WGS84");
                        oPrj.SetWellKnownGeogCS("WGS84");
                    }
                    else
                    {
                        const double inv_flattening = major_axis / (major_axis - minor_axis);
                        oLL.SetGeogCS("", "", pszEllipsoidName, major_axis,
                            inv_flattening);
                        oPrj.SetGeogCS("", "", pszEllipsoidName, major_axis,
                            inv_flattening);
                    }

                    CPLFree(poDS->pszGCPProjection);
                    poDS->pszGCPProjection = nullptr;
                    oLL.exportToWkt(&(poDS->pszGCPProjection));
                }

                /* -------------------------------------------------------------------- */
                /*      Collect GCPs.                                                   */
                /* -------------------------------------------------------------------- */
                CPLXMLNode *psGeoGrid =
                    CPLGetXMLNode(psAnnotation.get(),
                    "=product.geolocationGrid.geolocationGridPointList");

                if( psGeoGrid != nullptr )
                {
                    if( poDS->nGCPCount > 0 )
                    {
                        GDALDeinitGCPs( poDS->nGCPCount, poDS->pasGCPList );
                        CPLFree( poDS->pasGCPList );
                    }

                    /* count GCPs */
                    poDS->nGCPCount = 0;

                    for( CPLXMLNode *psNode = psGeoGrid->psChild; psNode != nullptr;
                         psNode = psNode->psNext )
                    {
                        if( EQUAL(psNode->pszValue, "geolocationGridPoint") )
                            poDS->nGCPCount++;
                    }

                    if( poDS->nGCPCount > 0 )
                    {
                        poDS->pasGCPList = static_cast<GDAL_GCP *>(
                            CPLCalloc(sizeof(GDAL_GCP), poDS->nGCPCount));

                        poDS->nGCPCount = 0;

                        for (CPLXMLNode *psNode = psGeoGrid->psChild; psNode != nullptr;
                            psNode = psNode->psNext)
                        {
                            GDAL_GCP *psGCP = poDS->pasGCPList + poDS->nGCPCount;

                            if( !EQUAL(psNode->pszValue, "geolocationGridPoint") )
                                continue;

                            poDS->nGCPCount++;

                            char szID[32];
                            snprintf(szID, sizeof(szID), "%d", poDS->nGCPCount);
                            psGCP->pszId = CPLStrdup(szID);
                            psGCP->pszInfo = CPLStrdup("");
                            psGCP->dfGCPPixel = CPLAtof(CPLGetXMLValue(psNode, "pixel", "0"));
                            psGCP->dfGCPLine = CPLAtof(CPLGetXMLValue(psNode, "line", "0"));
                            psGCP->dfGCPX = CPLAtof(CPLGetXMLValue(psNode, "longitude", ""));
                            psGCP->dfGCPY = CPLAtof(CPLGetXMLValue(psNode, "latitude", ""));
                            psGCP->dfGCPZ = CPLAtof(CPLGetXMLValue(psNode, "height", ""));
                        }
                    }
                }

                // Create bands
                if( EQUAL(osProductType.c_str(), "SLC") )
                {
                    // only add bands if no subDS or uncalibrated subdataset
                    // with complex data. (Calibrated will always be intensity
                    // only)
                    if( !bCalibrated && (eRequestType == UNKNOWN ||
                                         eRequestType == COMPLEX) )
                    {
                        poBandFile->MarkAsShared();
                        SAFESLCRasterBand *poBand0 = new SAFESLCRasterBand(
                            poDS.get(), eDataType, osSwath, osPolarization,
                            std::move(poBandFile), SAFESLCRasterBand::COMPLEX);
                        poDS->SetBand(poDS->GetRasterCount() + 1, poBand0);
                    }
                    else if( eRequestType == INTENSITY )  // Intensity
                    {
                        SAFESLCRasterBand *poBand1 = new SAFESLCRasterBand(
                            poDS.get(), eDataType, osSwath, osPolarization,
                            std::move(poBandFile), SAFESLCRasterBand::INTENSITY);
                        poDS->SetBand(poDS->GetRasterCount() + 1, poBand1);
                    }
                }
                else if( !bCalibrated && (eRequestType == UNKNOWN ||
                                          eRequestType == AMPLITUDE) )
                {
                    SAFERasterBand *poBand = new SAFERasterBand(
                        poDS.get(), eDataType, osSwath, osPolarization, std::move(poBandFile));
                    poDS->SetBand(poDS->GetRasterCount() + 1, poBand);
                }
                else if( bCalibrated && (eRequestType == UNKNOWN ||
                                         eRequestType == COMPLEX) )
                {
                    auto poBand =
                        cpl::make_unique<SAFECalibratedRasterBand>(poDS.get(), eDataType, osSwath,
                                                     osPolarization, std::move(poBandFile),
                                                     osCalibrationFilePath,
                                                     eCalibrationType);
                    if( !poBand->ReadLUT() )
                    {
                        CPLError(CE_Failure, CPLE_OpenFailed,
                                 "Reading calibration LUT(s) failed: %s.",
                                 osCalibrationFilePath.c_str());
                        return nullptr;
                    }
                    poDS->SetBand(poDS->GetRasterCount() + 1, poBand.release());
                }
            }
        }
    }

    //loop through all Swath/pols to add subdatasets
    if( !bIsSubDS )
    {
        int iSubDS = 1;
        const CPLString aosCalibrationValues[4] = { "SIGMA0", "BETA0", "GAMMA", "UNCALIB" };
        const CPLString aosDataUnitValues[3] = { "AMPLITUDE", "COMPLEX", "INTENSITY" };
        if( !isWave )
        {
            for( auto iterSwath: oMapSwaths2Pols )
            {
                CPLString osSubDS1 = iterSwath.first;
                CPLString osSubDS2;

                for (const auto& pol: iterSwath.second)
                {
                    if( !osSubDS2.empty() )
                        osSubDS2 += "+";
                    osSubDS2 += pol;
                    //Create single band or multiband complex SubDataset
                    int i = 0;
                    for( i = 0; i < 3; i++ )
                    {
                      CPLString osCalibTemp = aosCalibrationValues[i];
                      SAFEDataset::AddSubDataset(
                          poDS.get(), iSubDS + i,
                          CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s_%s:%s",
                                     osCalibTemp.c_str(), osMDFilename.c_str(),
                                     osSubDS1.c_str(), pol.c_str(),
                                     aosDataUnitValues[2].c_str()),
                          CPLSPrintf("Single band with %s swath and %s "
                                     "polarization and %s calibration",
                                     osSubDS1.c_str(), pol.c_str(),
                                     osCalibTemp.c_str()));
                    }

                    if( bIsSLC )
                    {
                        CPLString osCalibTemp = aosCalibrationValues[i];
                        SAFEDataset::AddSubDataset(
                            poDS.get(), iSubDS + i,
                            CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s_%s:%s",
                                       osCalibTemp.c_str(),
                                       osMDFilename.c_str(), osSubDS1.c_str(),
                                       pol.c_str(),
                                       aosDataUnitValues[1].c_str()),
                            CPLSPrintf("Single band with %s swath and %s "
                                       "polarization and %s calibration",
                                       osSubDS1.c_str(), pol.c_str(),
                                       osCalibTemp.c_str()));
                        SAFEDataset::AddSubDataset(
                            poDS.get(), iSubDS + i + 1,
                            CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s_%s:%s",
                                       osCalibTemp.c_str(),
                                       osMDFilename.c_str(), osSubDS1.c_str(),
                                       pol.c_str(),
                                       aosDataUnitValues[2].c_str()),
                            CPLSPrintf("Single band with %s swath and %s "
                                       "polarization and %s calibration",
                                       osSubDS1.c_str(), pol.c_str(),
                                       osCalibTemp.c_str()));
                        iSubDS += 5;
                    }
                    else
                    {
                      CPLString osCalibTemp = aosCalibrationValues[i];

                      SAFEDataset::AddSubDataset(
                          poDS.get(), iSubDS + i,
                          CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s_%s:%s",
                                     osCalibTemp.c_str(), osMDFilename.c_str(),
                                     osSubDS1.c_str(), pol.c_str(),
                                     aosDataUnitValues[0].c_str()),
                          CPLSPrintf("Single band with %s swath and %s "
                                     "polarization and %s calibration",
                                     osSubDS1.c_str(), pol.c_str(),
                                     osCalibTemp.c_str()));
                      iSubDS += 4;
                    }
                }

                if( iterSwath.second.size() > 1 )
                {
                    // Create single band subdataset with all polarizations
                    int i = 0;
                    for( i = 0; i < 3; i++ )
                    {
                        CPLString osCalibTemp = aosCalibrationValues[i];

                        SAFEDataset::AddSubDataset(
                            poDS.get(), iSubDS + i,
                            CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s:%s",
                                       osCalibTemp.c_str(),
                                       osMDFilename.c_str(), osSubDS1.c_str(),
                                       aosDataUnitValues[2].c_str()),
                            CPLSPrintf(
                                "%s swath with all polarizations (%s) as "
                                "bands and %s calibration",
                                osSubDS1.c_str(), osSubDS2.c_str(),
                                osCalibTemp.c_str()));
                    }

                    if(bIsSLC)
                    {
                        CPLString osCalibTemp = aosCalibrationValues[i];
                        SAFEDataset::AddSubDataset(
                            poDS.get(), iSubDS + i,
                            CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s:%s",
                                       osCalibTemp.c_str(),
                                       osMDFilename.c_str(), osSubDS1.c_str(),
                                       aosDataUnitValues[1].c_str()),
                            CPLSPrintf(
                                "%s swath with all polarizations (%s) as "
                                "bands and %s calibration",
                                osSubDS1.c_str(), osSubDS2.c_str(),
                                osCalibTemp.c_str()));
                        SAFEDataset::AddSubDataset(
                            poDS.get(), iSubDS + i + 1,
                            CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s:%s",
                                       osCalibTemp.c_str(),
                                       osMDFilename.c_str(), osSubDS1.c_str(),
                                       aosDataUnitValues[2].c_str()),
                            CPLSPrintf(
                                "%s swath with all polarizations (%s) as "
                                "bands and %s calibration",
                                osSubDS1.c_str(), osSubDS2.c_str(),
                                osCalibTemp.c_str()));
                        iSubDS += 5;
                    }
                    else
                    {
                        CPLString osCalibTemp = aosCalibrationValues[i];
                        SAFEDataset::AddSubDataset(
                            poDS.get(), iSubDS + i,
                            CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s:%s",
                                       osCalibTemp.c_str(),
                                       osMDFilename.c_str(), osSubDS1.c_str(),
                                       aosDataUnitValues[0].c_str()),
                            CPLSPrintf(
                                "%s swath with all polarizations (%s) as "
                                "bands and %s calibration",
                                osSubDS1.c_str(), osSubDS2.c_str(),
                                osCalibTemp.c_str()));
                        iSubDS += 4;
                    }
                }
            }
        }
        else
        {
            for( const CPLString& osImgSwPol : oImageNumberSwPol )
            {
                CPLString osImgSwPolTmp = osImgSwPol;
                const char* pszImage = strchr(osImgSwPolTmp.c_str(),' ');
                CPLString osImage,osSwath,osPolarization;
                if( pszImage != nullptr )
                {
                    osImage = osImgSwPolTmp;
                    osImage.resize(pszImage - osImgSwPolTmp.c_str());
                    osImgSwPolTmp = pszImage + strlen(" ");
                    const char* pszSwath = strchr(osImgSwPolTmp.c_str(),' ');
                    if( pszSwath != nullptr )
                    {
                        osSwath = osImgSwPolTmp;
                        osSwath.resize(pszSwath - osImgSwPolTmp.c_str());
                        osPolarization = pszSwath + strlen(" ");
                        int i = 0;
                        for (i = 0; i < 3; i++){
                          CPLString osCalibTemp = aosCalibrationValues[i];

                          SAFEDataset::AddSubDataset(
                              poDS.get(), iSubDS + i,
                              CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s_%s_%s:%s",
                                         osCalibTemp.c_str(),
                                         osMDFilename.c_str(), osSwath.c_str(),
                                         osPolarization.c_str(),
                                         osImage.c_str(),
                                         aosDataUnitValues[2].c_str()),
                              CPLSPrintf("Single band with %s swath and %s "
                                         "polarization and %s calibration",
                                         osSwath.c_str(),
                                         osPolarization.c_str(),
                                         osCalibTemp.c_str()));
                        }

                        if( bIsSLC )
                        {
                          CPLString osCalibTemp = aosCalibrationValues[i];

                          SAFEDataset::AddSubDataset(
                              poDS.get(), iSubDS + i,
                              CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s_%s_%s:%s",
                                         osCalibTemp.c_str(),
                                         osMDFilename.c_str(), osSwath.c_str(),
                                         osPolarization.c_str(),
                                         osImage.c_str(),
                                         aosDataUnitValues[1].c_str()),
                              CPLSPrintf("Single band with %s swath and %s "
                                         "polarization and %s calibration",
                                         osSwath.c_str(),
                                         osPolarization.c_str(),
                                         osCalibTemp.c_str()));

                          SAFEDataset::AddSubDataset(
                              poDS.get(), iSubDS + i + 1,
                              CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s_%s_%s:%s",
                                         osCalibTemp.c_str(),
                                         osMDFilename.c_str(), osSwath.c_str(),
                                         osPolarization.c_str(),
                                         osImage.c_str(),
                                         aosDataUnitValues[2].c_str()),
                              CPLSPrintf("Single band with %s swath and %s "
                                         "polarization and %s calibration",
                                         osSwath.c_str(),
                                         osPolarization.c_str(),
                                         osCalibTemp.c_str()));

                          iSubDS += 5;
                        }
                        else
                        {
                          CPLString osCalibTemp = aosCalibrationValues[i];

                          SAFEDataset::AddSubDataset(
                              poDS.get(), iSubDS + i,
                              CPLSPrintf("SENTINEL1_CALIB:%s:%s:%s_%s_%s:%s",
                                         osCalibTemp.c_str(),
                                         osMDFilename.c_str(), osSwath.c_str(),
                                         osPolarization.c_str(),
                                         osImage.c_str(),
                                         aosDataUnitValues[0].c_str()),
                              CPLSPrintf("Single band with %s swath and %s "
                                         "polarization and %s calibration",
                                         osSwath.c_str(),
                                         osPolarization.c_str(),
                                         osCalibTemp.c_str()));
                          iSubDS += 4;
                        }
                    }
                }
            }
        }
    }
    if( poDS->GetRasterCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Measurement bands not found.");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Collect more metadata elements                                  */
    /* -------------------------------------------------------------------- */

    /* -------------------------------------------------------------------- */
    /*      Platform information                                            */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psPlatformAttrs =
        SAFEDataset::GetMetaDataObject(psMetaDataObjects, "platform");

    if( psPlatformAttrs != nullptr )
    {
        const char *pszItem =
            CPLGetXMLValue(psPlatformAttrs,
                           "metadataWrap.xmlData.safe:platform"
                           ".safe:familyName",
                           "");
        poDS->SetMetadataItem("SATELLITE_IDENTIFIER", pszItem);

        pszItem =
            CPLGetXMLValue(psPlatformAttrs,
                           "metadataWrap.xmlData.safe:platform"
                           ".safe:instrument.safe:familyName.abbreviation",
                           "");
        poDS->SetMetadataItem("SENSOR_IDENTIFIER", pszItem);

        pszItem = CPLGetXMLValue(psPlatformAttrs,
                                 "metadataWrap.xmlData.safe:platform"
                                 ".safe:instrument.safe:extension"
                                 ".s1sarl1:instrumentMode.s1sarl1:mode",
                                 "UNK");
        poDS->SetMetadataItem("BEAM_MODE", pszItem);

        pszItem = CPLGetXMLValue(psPlatformAttrs,
                                 "metadataWrap.xmlData.safe:platform"
                                 ".safe:instrument.safe:extension"
                                 ".s1sarl1:instrumentMode.s1sarl1:swath",
                                 "UNK");
        poDS->SetMetadataItem("BEAM_SWATH", pszItem);
    }

    /* -------------------------------------------------------------------- */
    /*      Acquisition Period information                                  */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psAcquisitionAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, "acquisitionPeriod");

    if( psAcquisitionAttrs != nullptr )
    {
        const char *pszItem = CPLGetXMLValue(
            psAcquisitionAttrs,
            "metadataWrap.xmlData.safe:acquisitionPeriod"
            ".safe:startTime", "UNK");
        poDS->SetMetadataItem("ACQUISITION_START_TIME", pszItem);
        pszItem = CPLGetXMLValue(
            psAcquisitionAttrs,
            "metadataWrap.xmlData.safe:acquisitionPeriod"
            ".safe:stopTime", "UNK");
        poDS->SetMetadataItem("ACQUISITION_STOP_TIME", pszItem);
    }

    /* -------------------------------------------------------------------- */
    /*      Processing information                                          */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psProcessingAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, "processing");

    if (psProcessingAttrs != nullptr) {
        const char *pszItem = CPLGetXMLValue(
            psProcessingAttrs,
            "metadataWrap.xmlData.safe:processing.safe:facility.name", "UNK");
        poDS->SetMetadataItem("FACILITY_IDENTIFIER", pszItem);
    }

    /* -------------------------------------------------------------------- */
    /*      Measurement Orbit Reference information                         */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psOrbitAttrs = SAFEDataset::GetMetaDataObject(
        psMetaDataObjects, "measurementOrbitReference");

    if (psOrbitAttrs != nullptr) {
        const char *pszItem = CPLGetXMLValue(psOrbitAttrs,
            "metadataWrap.xmlData.safe:orbitReference"
            ".safe:orbitNumber", "UNK");
        poDS->SetMetadataItem("ORBIT_NUMBER", pszItem);
        pszItem = CPLGetXMLValue(psOrbitAttrs,
            "metadataWrap.xmlData.safe:orbitReference"
            ".safe:extension.s1:orbitProperties.s1:pass", "UNK");
        poDS->SetMetadataItem("ORBIT_DIRECTION", pszItem);
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    const CPLString osDescription = osMDFilename;

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription(osDescription);

    poDS->SetPhysicalFilename(osMDFilename);
    if( !osSubdatasetName.empty() )
    {
        poDS->SetDescription(poOpenInfo->pszFilename);
        poDS->SetSubdatasetName(osSubdatasetName);
    }

    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), ":::VIRTUAL:::");

    return poDS.release();
}

/************************************************************************/
/*                            AddSubDataset()                           */
/************************************************************************/
void SAFEDataset::AddSubDataset(SAFEDataset *poDS, int iDSNum,
                                const CPLString &osName,
                                const CPLString &osDesc)
{
    poDS->papszSubDatasets = CSLAddNameValue(
        poDS->papszSubDatasets, CPLSPrintf("SUBDATASET_%d_NAME", iDSNum),
        osName.c_str());
    poDS->papszSubDatasets = CSLAddNameValue(
        poDS->papszSubDatasets, CPLSPrintf("SUBDATASET_%d_DESC", iDSNum),
        osDesc.c_str());
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int SAFEDataset::GetGCPCount()
{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *SAFEDataset::_GetGCPProjection()
{
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *SAFEDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *SAFEDataset::_GetProjectionRef()
{
    return pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SAFEDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 );

    if (bHaveGeoTransform)
        return CE_None;

    return CE_Failure;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **SAFEDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "SUBDATASETS", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **SAFEDataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "SUBDATASETS") )
        return papszSubDatasets;

    return GDALDataset::GetMetadata( pszDomain );
}


/************************************************************************/
/*                         GDALRegister_SAFE()                          */
/************************************************************************/

void GDALRegister_SAFE()
{
    if( GDALGetDriverByName("SAFE") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("SAFE");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Sentinel-1 SAR SAFE Product");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/safe.html");

    poDriver->pfnOpen = SAFEDataset::Open;
    poDriver->pfnIdentify = SAFEDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                  Azimuth time handling functions                     */
/************************************************************************/

SAFECalibratedRasterBand::TimePoint
SAFECalibratedRasterBand::getTimePoint(const char *pszTime)
{
    int nYear, nMonth, nDay, nHour, nMinute, nSeconds;
    long nMicroSeconds;
    sscanf(pszTime, "%d-%d-%dT%d:%d:%d.%ld", &nYear, &nMonth, &nDay, &nHour,
           &nMinute, &nSeconds, &nMicroSeconds);

    struct tm oTm;
    oTm.tm_sec = nSeconds;
    oTm.tm_min = nMinute;
    oTm.tm_hour = nHour;
    oTm.tm_mday = nDay;
    oTm.tm_mon = nMonth - 1;
    oTm.tm_year = nYear - 1900;
    oTm.tm_isdst = -1;

    std::time_t oTt = static_cast<std::time_t>(
        CPLYMDHMSToUnixTime(&oTm));

    TimePoint oTp = std::chrono::system_clock::from_time_t(oTt);
    TimePoint oTp1 = oTp + std::chrono::microseconds(nMicroSeconds);
    return oTp1;
}

double SAFECalibratedRasterBand::getTimeDiff(TimePoint oT1, TimePoint oT2)
{
    std::chrono::duration<double> oResult = (oT2 - oT1);
    return oResult.count();
}

SAFECalibratedRasterBand::TimePoint
SAFECalibratedRasterBand::getazTime(TimePoint oStart, TimePoint oStop,
                                    long nNumOfLines, int nOffset)
{
    double dfTemp = getTimeDiff(oStart, oStop);
    dfTemp /= (nNumOfLines - 1);
    unsigned long nTimeDiffMicro = static_cast<unsigned long>(dfTemp * 1000000);
    TimePoint oResult =
        oStart + (nOffset * std::chrono::microseconds(nTimeDiffMicro));
    return oResult;
}

/************************************************************************/
/*                Utility functions used in interpolation              */
/************************************************************************/

int SAFECalibratedRasterBand::getCalibrationVectorIndex(int nLineNo)
{
    for(size_t i = 1; i < m_anLineLUT.size(); i++)
    {
        if(nLineNo < m_anLineLUT[i])
            return static_cast<int>(i - 1);
    }
    return 0;
}

int SAFECalibratedRasterBand::getPixelIndex(int nPixelNo)
{
    for(int i = 1; i < m_nNumPixels; i++)
    {
        if(nPixelNo < m_anPixelLUT[i])
            return i - 1;
    }
    return 0;
}
