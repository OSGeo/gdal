/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTProcessedDataset processing functions
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_minixml.h"
#include "cpl_string.h"
#include "vrtdataset.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <vector>

/************************************************************************/
/*                               GetDstValue()                          */
/************************************************************************/

/** Return a destination value given an initial value, the destination no data
 * value and its replacement value
 */
static inline double GetDstValue(double dfVal, double dfDstNoData,
                                 double dfReplacementDstNodata,
                                 GDALDataType eIntendedDstDT,
                                 bool bDstIntendedDTIsInteger)
{
    if (bDstIntendedDTIsInteger && std::round(dfVal) == dfDstNoData)
    {
        return dfReplacementDstNodata;
    }
    else if (eIntendedDstDT == GDT_Float32 &&
             static_cast<float>(dfVal) == static_cast<float>(dfDstNoData))
    {
        return dfReplacementDstNodata;
    }
    else if (eIntendedDstDT == GDT_Float64 && dfVal == dfDstNoData)
    {
        return dfReplacementDstNodata;
    }
    else
    {
        return dfVal;
    }
}

/************************************************************************/
/*                    BandAffineCombinationData                         */
/************************************************************************/

namespace
{
/** Working structure for 'BandAffineCombination' builtin function. */
struct BandAffineCombinationData
{
    static constexpr const char *const EXPECTED_SIGNATURE =
        "BandAffineCombination";
    //! Signature (to make sure callback functions are called with the right argument)
    const std::string m_osSignature = EXPECTED_SIGNATURE;

    /** Replacement nodata value */
    std::vector<double> m_adfReplacementDstNodata{};

    /** Intended destination data type. */
    GDALDataType m_eIntendedDstDT = GDT_Float64;

    /** Affine transformation coefficients.
     * m_aadfCoefficients[i][0] is the constant term for the i(th) dst band
     * m_aadfCoefficients[i][j] is the weight of the j(th) src band for the
     * i(th) dst vand.
     * Said otherwise dst[i] = m_aadfCoefficients[i][0] +
     *      sum(m_aadfCoefficients[i][j + 1] * src[j] for j in 0...nSrcBands-1)
     */
    std::vector<std::vector<double>> m_aadfCoefficients{};

    //! Minimum clamping value.
    double m_dfClampMin = std::numeric_limits<double>::quiet_NaN();

    //! Maximum clamping value.
    double m_dfClampMax = std::numeric_limits<double>::quiet_NaN();
};
}  // namespace

/************************************************************************/
/*               SetOutputValuesForInNoDataAndOutNoData()               */
/************************************************************************/

static std::vector<double> SetOutputValuesForInNoDataAndOutNoData(
    int nInBands, double *padfInNoData, int *pnOutBands,
    double **ppadfOutNoData, bool bSrcNodataSpecified, double dfSrcNoData,
    bool bDstNodataSpecified, double dfDstNoData, bool bIsFinalStep)
{
    if (bSrcNodataSpecified)
    {
        std::vector<double> adfNoData(nInBands, dfSrcNoData);
        memcpy(padfInNoData, adfNoData.data(),
               adfNoData.size() * sizeof(double));
    }

    std::vector<double> adfDstNoData;
    if (bDstNodataSpecified)
    {
        adfDstNoData.resize(*pnOutBands, dfDstNoData);
    }
    else if (bIsFinalStep)
    {
        adfDstNoData =
            std::vector<double>(*ppadfOutNoData, *ppadfOutNoData + *pnOutBands);
    }
    else
    {
        adfDstNoData =
            std::vector<double>(padfInNoData, padfInNoData + nInBands);
        adfDstNoData.resize(*pnOutBands, *padfInNoData);
    }

    if (*ppadfOutNoData == nullptr)
    {
        *ppadfOutNoData =
            static_cast<double *>(CPLMalloc(*pnOutBands * sizeof(double)));
    }
    memcpy(*ppadfOutNoData, adfDstNoData.data(), *pnOutBands * sizeof(double));

    return adfDstNoData;
}

/************************************************************************/
/*                    BandAffineCombinationInit()                       */
/************************************************************************/

/** Init function for 'BandAffineCombination' builtin function. */
static CPLErr BandAffineCombinationInit(
    const char * /*pszFuncName*/, void * /*pUserData*/,
    CSLConstList papszFunctionArgs, int nInBands, GDALDataType eInDT,
    double *padfInNoData, int *pnOutBands, GDALDataType *peOutDT,
    double **ppadfOutNoData, const char * /* pszVRTPath */,
    VRTPDWorkingDataPtr *ppWorkingData)
{
    CPLAssert(eInDT == GDT_Float64);

    *peOutDT = eInDT;
    *ppWorkingData = nullptr;

    auto data = std::make_unique<BandAffineCombinationData>();

    std::map<int, std::vector<double>> oMapCoefficients{};
    double dfSrcNoData = std::numeric_limits<double>::quiet_NaN();
    bool bSrcNodataSpecified = false;
    double dfDstNoData = std::numeric_limits<double>::quiet_NaN();
    bool bDstNodataSpecified = false;
    double dfReplacementDstNodata = std::numeric_limits<double>::quiet_NaN();
    bool bReplacementDstNodataSpecified = false;

    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(papszFunctionArgs))
    {
        if (EQUAL(pszKey, "src_nodata"))
        {
            bSrcNodataSpecified = true;
            dfSrcNoData = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "dst_nodata"))
        {
            bDstNodataSpecified = true;
            dfDstNoData = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "replacement_nodata"))
        {
            bReplacementDstNodataSpecified = true;
            dfReplacementDstNodata = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "dst_intended_datatype"))
        {
            for (GDALDataType eDT = GDT_Byte; eDT < GDT_TypeCount;
                 eDT = static_cast<GDALDataType>(eDT + 1))
            {
                if (EQUAL(GDALGetDataTypeName(eDT), pszValue))
                {
                    data->m_eIntendedDstDT = eDT;
                    break;
                }
            }
        }
        else if (STARTS_WITH_CI(pszKey, "coefficients_"))
        {
            const int nTargetBand = atoi(pszKey + strlen("coefficients_"));
            if (nTargetBand <= 0 || nTargetBand > 65536)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            const CPLStringList aosTokens(CSLTokenizeString2(pszValue, ",", 0));
            if (aosTokens.size() != 1 + nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Argument %s has %d values, whereas %d are expected",
                         pszKey, aosTokens.size(), 1 + nInBands);
                return CE_Failure;
            }
            std::vector<double> adfValues;
            for (int i = 0; i < aosTokens.size(); ++i)
            {
                adfValues.push_back(CPLAtof(aosTokens[i]));
            }
            oMapCoefficients[nTargetBand - 1] = std::move(adfValues);
        }
        else if (EQUAL(pszKey, "min"))
        {
            data->m_dfClampMin = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "max"))
        {
            data->m_dfClampMax = CPLAtof(pszValue);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unrecognized argument name %s. Ignored", pszKey);
        }
    }

    const bool bIsFinalStep = *pnOutBands != 0;
    if (bIsFinalStep)
    {
        if (*pnOutBands != static_cast<int>(oMapCoefficients.size()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Final step expect %d bands, but only %d coefficient_XX "
                     "are provided",
                     *pnOutBands, static_cast<int>(oMapCoefficients.size()));
            return CE_Failure;
        }
    }
    else
    {
        *pnOutBands = static_cast<int>(oMapCoefficients.size());
    }

    const std::vector<double> adfDstNoData =
        SetOutputValuesForInNoDataAndOutNoData(
            nInBands, padfInNoData, pnOutBands, ppadfOutNoData,
            bSrcNodataSpecified, dfSrcNoData, bDstNodataSpecified, dfDstNoData,
            bIsFinalStep);

    if (bReplacementDstNodataSpecified)
    {
        data->m_adfReplacementDstNodata.resize(*pnOutBands,
                                               dfReplacementDstNodata);
    }
    else
    {
        for (double dfVal : adfDstNoData)
        {
            data->m_adfReplacementDstNodata.emplace_back(
                GDALGetNoDataReplacementValue(data->m_eIntendedDstDT, dfVal));
        }
    }

    // Check we have a set of coefficient for all output bands and
    // convert the map to a vector
    for (auto &oIter : oMapCoefficients)
    {
        const int iExpected = static_cast<int>(data->m_aadfCoefficients.size());
        if (oIter.first != iExpected)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Argument coefficients_%d is missing", iExpected + 1);
            return CE_Failure;
        }
        data->m_aadfCoefficients.emplace_back(std::move(oIter.second));
    }
    *ppWorkingData = data.release();
    return CE_None;
}

/************************************************************************/
/*                    BandAffineCombinationFree()                       */
/************************************************************************/

/** Free function for 'BandAffineCombination' builtin function. */
static void BandAffineCombinationFree(const char * /*pszFuncName*/,
                                      void * /*pUserData*/,
                                      VRTPDWorkingDataPtr pWorkingData)
{
    BandAffineCombinationData *data =
        static_cast<BandAffineCombinationData *>(pWorkingData);
    CPLAssert(data->m_osSignature ==
              BandAffineCombinationData::EXPECTED_SIGNATURE);
    CPL_IGNORE_RET_VAL(data->m_osSignature);
    delete data;
}

/************************************************************************/
/*                    BandAffineCombinationProcess()                    */
/************************************************************************/

/** Processing function for 'BandAffineCombination' builtin function. */
static CPLErr BandAffineCombinationProcess(
    const char * /*pszFuncName*/, void * /*pUserData*/,
    VRTPDWorkingDataPtr pWorkingData, CSLConstList /* papszFunctionArgs*/,
    int nBufXSize, int nBufYSize, const void *pInBuffer, size_t nInBufferSize,
    GDALDataType eInDT, int nInBands, const double *CPL_RESTRICT padfInNoData,
    void *pOutBuffer, size_t nOutBufferSize, GDALDataType eOutDT, int nOutBands,
    const double *CPL_RESTRICT padfOutNoData, double /*dfSrcXOff*/,
    double /*dfSrcYOff*/, double /*dfSrcXSize*/, double /*dfSrcYSize*/,
    const double /*adfSrcGT*/[], const char * /* pszVRTPath */,
    CSLConstList /*papszExtra*/)
{
    const size_t nElts = static_cast<size_t>(nBufXSize) * nBufYSize;

    CPL_IGNORE_RET_VAL(eInDT);
    CPLAssert(eInDT == GDT_Float64);
    CPL_IGNORE_RET_VAL(eOutDT);
    CPLAssert(eOutDT == GDT_Float64);
    CPL_IGNORE_RET_VAL(nInBufferSize);
    CPLAssert(nInBufferSize == nElts * nInBands * sizeof(double));
    CPL_IGNORE_RET_VAL(nOutBufferSize);
    CPLAssert(nOutBufferSize == nElts * nOutBands * sizeof(double));

    const BandAffineCombinationData *data =
        static_cast<BandAffineCombinationData *>(pWorkingData);
    CPLAssert(data->m_osSignature ==
              BandAffineCombinationData::EXPECTED_SIGNATURE);
    const double *CPL_RESTRICT padfSrc = static_cast<const double *>(pInBuffer);
    double *CPL_RESTRICT padfDst = static_cast<double *>(pOutBuffer);
    const bool bDstIntendedDTIsInteger =
        GDALDataTypeIsInteger(data->m_eIntendedDstDT);
    const double dfClampMin = data->m_dfClampMin;
    const double dfClampMax = data->m_dfClampMax;
    for (size_t i = 0; i < nElts; ++i)
    {
        for (int iDst = 0; iDst < nOutBands; ++iDst)
        {
            const auto &adfCoefficients = data->m_aadfCoefficients[iDst];
            double dfVal = adfCoefficients[0];
            bool bSetNoData = false;
            for (int iSrc = 0; iSrc < nInBands; ++iSrc)
            {
                // written this way to work with a NaN value
                if (!(padfSrc[iSrc] != padfInNoData[iSrc]))
                {
                    bSetNoData = true;
                    break;
                }
                dfVal += adfCoefficients[iSrc + 1] * padfSrc[iSrc];
            }
            if (bSetNoData)
            {
                *padfDst = padfOutNoData[iDst];
            }
            else
            {
                double dfDstVal = GetDstValue(
                    dfVal, padfOutNoData[iDst],
                    data->m_adfReplacementDstNodata[iDst],
                    data->m_eIntendedDstDT, bDstIntendedDTIsInteger);
                if (dfDstVal < dfClampMin)
                    dfDstVal = dfClampMin;
                if (dfDstVal > dfClampMax)
                    dfDstVal = dfClampMax;
                *padfDst = dfDstVal;
            }
            ++padfDst;
        }
        padfSrc += nInBands;
    }

    return CE_None;
}

/************************************************************************/
/*                                LUTData                               */
/************************************************************************/

namespace
{
/** Working structure for 'LUT' builtin function. */
struct LUTData
{
    static constexpr const char *const EXPECTED_SIGNATURE = "LUT";
    //! Signature (to make sure callback functions are called with the right argument)
    const std::string m_osSignature = EXPECTED_SIGNATURE;

    //! m_aadfLUTInputs[i][j] is the j(th) input value for that LUT of band i.
    std::vector<std::vector<double>> m_aadfLUTInputs{};

    //! m_aadfLUTOutputs[i][j] is the j(th) output value for that LUT of band i.
    std::vector<std::vector<double>> m_aadfLUTOutputs{};

    /************************************************************************/
    /*                              LookupValue()                           */
    /************************************************************************/

    double LookupValue(int iBand, double dfInput) const
    {
        const auto &adfInput = m_aadfLUTInputs[iBand];
        const auto &afdOutput = m_aadfLUTOutputs[iBand];

        // Find the index of the first element in the LUT input array that
        // is not smaller than the input value.
        int i = static_cast<int>(
            std::lower_bound(adfInput.data(), adfInput.data() + adfInput.size(),
                             dfInput) -
            adfInput.data());

        if (i == 0)
            return afdOutput[0];

        // If the index is beyond the end of the LUT input array, the input
        // value is larger than all the values in the array.
        if (i == static_cast<int>(adfInput.size()))
            return afdOutput.back();

        if (adfInput[i] == dfInput)
            return afdOutput[i];

        // Otherwise, interpolate.
        return afdOutput[i - 1] + (dfInput - adfInput[i - 1]) *
                                      ((afdOutput[i] - afdOutput[i - 1]) /
                                       (adfInput[i] - adfInput[i - 1]));
    }
};
}  // namespace

/************************************************************************/
/*                                LUTInit()                             */
/************************************************************************/

/** Init function for 'LUT' builtin function. */
static CPLErr LUTInit(const char * /*pszFuncName*/, void * /*pUserData*/,
                      CSLConstList papszFunctionArgs, int nInBands,
                      GDALDataType eInDT, double *padfInNoData, int *pnOutBands,
                      GDALDataType *peOutDT, double **ppadfOutNoData,
                      const char * /* pszVRTPath */,
                      VRTPDWorkingDataPtr *ppWorkingData)
{
    CPLAssert(eInDT == GDT_Float64);

    const bool bIsFinalStep = *pnOutBands != 0;
    *peOutDT = eInDT;
    *ppWorkingData = nullptr;

    if (!bIsFinalStep)
    {
        *pnOutBands = nInBands;
    }

    auto data = std::make_unique<LUTData>();

    double dfSrcNoData = std::numeric_limits<double>::quiet_NaN();
    bool bSrcNodataSpecified = false;
    double dfDstNoData = std::numeric_limits<double>::quiet_NaN();
    bool bDstNodataSpecified = false;

    std::map<int, std::pair<std::vector<double>, std::vector<double>>> oMap{};

    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(papszFunctionArgs))
    {
        if (EQUAL(pszKey, "src_nodata"))
        {
            bSrcNodataSpecified = true;
            dfSrcNoData = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "dst_nodata"))
        {
            bDstNodataSpecified = true;
            dfDstNoData = CPLAtof(pszValue);
        }
        else if (STARTS_WITH_CI(pszKey, "lut_"))
        {
            const int nBand = atoi(pszKey + strlen("lut_"));
            if (nBand <= 0 || nBand > nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            const CPLStringList aosTokens(CSLTokenizeString2(pszValue, ",", 0));
            std::vector<double> adfInputValues;
            std::vector<double> adfOutputValues;
            for (int i = 0; i < aosTokens.size(); ++i)
            {
                const CPLStringList aosTokens2(
                    CSLTokenizeString2(aosTokens[i], ":", 0));
                if (aosTokens2.size() != 2)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid value for argument '%s'", pszKey);
                    return CE_Failure;
                }
                adfInputValues.push_back(CPLAtof(aosTokens2[0]));
                adfOutputValues.push_back(CPLAtof(aosTokens2[1]));
            }
            oMap[nBand - 1] = std::pair(std::move(adfInputValues),
                                        std::move(adfOutputValues));
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unrecognized argument name %s. Ignored", pszKey);
        }
    }

    SetOutputValuesForInNoDataAndOutNoData(
        nInBands, padfInNoData, pnOutBands, ppadfOutNoData, bSrcNodataSpecified,
        dfSrcNoData, bDstNodataSpecified, dfDstNoData, bIsFinalStep);

    int iExpected = 0;
    // Check we have values for all bands and convert to vector
    for (auto &oIter : oMap)
    {
        if (oIter.first != iExpected)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Argument lut_%d is missing",
                     iExpected + 1);
            return CE_Failure;
        }
        ++iExpected;
        data->m_aadfLUTInputs.emplace_back(std::move(oIter.second.first));
        data->m_aadfLUTOutputs.emplace_back(std::move(oIter.second.second));
    }

    if (static_cast<int>(oMap.size()) < *pnOutBands)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing lut_XX element(s)");
        return CE_Failure;
    }

    *ppWorkingData = data.release();
    return CE_None;
}

/************************************************************************/
/*                                LUTFree()                             */
/************************************************************************/

/** Free function for 'LUT' builtin function. */
static void LUTFree(const char * /*pszFuncName*/, void * /*pUserData*/,
                    VRTPDWorkingDataPtr pWorkingData)
{
    LUTData *data = static_cast<LUTData *>(pWorkingData);
    CPLAssert(data->m_osSignature == LUTData::EXPECTED_SIGNATURE);
    CPL_IGNORE_RET_VAL(data->m_osSignature);
    delete data;
}

/************************************************************************/
/*                             LUTProcess()                             */
/************************************************************************/

/** Processing function for 'LUT' builtin function. */
static CPLErr
LUTProcess(const char * /*pszFuncName*/, void * /*pUserData*/,
           VRTPDWorkingDataPtr pWorkingData,
           CSLConstList /* papszFunctionArgs*/, int nBufXSize, int nBufYSize,
           const void *pInBuffer, size_t nInBufferSize, GDALDataType eInDT,
           int nInBands, const double *CPL_RESTRICT padfInNoData,
           void *pOutBuffer, size_t nOutBufferSize, GDALDataType eOutDT,
           int nOutBands, const double *CPL_RESTRICT padfOutNoData,
           double /*dfSrcXOff*/, double /*dfSrcYOff*/, double /*dfSrcXSize*/,
           double /*dfSrcYSize*/, const double /*adfSrcGT*/[],
           const char * /* pszVRTPath */, CSLConstList /*papszExtra*/)
{
    const size_t nElts = static_cast<size_t>(nBufXSize) * nBufYSize;

    CPL_IGNORE_RET_VAL(eInDT);
    CPLAssert(eInDT == GDT_Float64);
    CPL_IGNORE_RET_VAL(eOutDT);
    CPLAssert(eOutDT == GDT_Float64);
    CPL_IGNORE_RET_VAL(nInBufferSize);
    CPLAssert(nInBufferSize == nElts * nInBands * sizeof(double));
    CPL_IGNORE_RET_VAL(nOutBufferSize);
    CPLAssert(nOutBufferSize == nElts * nOutBands * sizeof(double));
    CPLAssert(nInBands == nOutBands);
    CPL_IGNORE_RET_VAL(nOutBands);

    const LUTData *data = static_cast<LUTData *>(pWorkingData);
    CPLAssert(data->m_osSignature == LUTData::EXPECTED_SIGNATURE);
    const double *CPL_RESTRICT padfSrc = static_cast<const double *>(pInBuffer);
    double *CPL_RESTRICT padfDst = static_cast<double *>(pOutBuffer);
    for (size_t i = 0; i < nElts; ++i)
    {
        for (int iBand = 0; iBand < nInBands; ++iBand)
        {
            // written this way to work with a NaN value
            if (!(*padfSrc != padfInNoData[iBand]))
                *padfDst = padfOutNoData[iBand];
            else
                *padfDst = data->LookupValue(iBand, *padfSrc);
            ++padfSrc;
            ++padfDst;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                        LocalScaleOffsetData                          */
/************************************************************************/

namespace
{
/** Working structure for 'LocalScaleOffset' builtin function. */
struct LocalScaleOffsetData
{
    static constexpr const char *const EXPECTED_SIGNATURE = "LocalScaleOffset";
    //! Signature (to make sure callback functions are called with the right argument)
    const std::string m_osSignature = EXPECTED_SIGNATURE;

    //! Nodata value for gain dataset(s)
    double m_dfGainNodata = std::numeric_limits<double>::quiet_NaN();

    //! Nodata value for offset dataset(s)
    double m_dfOffsetNodata = std::numeric_limits<double>::quiet_NaN();

    //! Minimum clamping value.
    double m_dfClampMin = std::numeric_limits<double>::quiet_NaN();

    //! Maximum clamping value.
    double m_dfClampMax = std::numeric_limits<double>::quiet_NaN();

    //! Map from gain/offset dataset name to datasets
    std::map<std::string, std::unique_ptr<GDALDataset>> m_oDatasetMap{};

    //! Vector of size nInBands that point to the raster band from which to read gains.
    std::vector<GDALRasterBand *> m_oGainBands{};

    //! Vector of size nInBands that point to the raster band from which to read offsets.
    std::vector<GDALRasterBand *> m_oOffsetBands{};

    //! Working buffer that contain gain values.
    std::vector<VRTProcessedDataset::NoInitByte> m_abyGainBuffer{};

    //! Working buffer that contain offset values.
    std::vector<VRTProcessedDataset::NoInitByte> m_abyOffsetBuffer{};
};
}  // namespace

/************************************************************************/
/*                           CheckAllBands()                            */
/************************************************************************/

/** Return true if the key of oMap is the sequence of all integers between
 * 0 and nExpectedBandCount-1.
 */
template <class T>
static bool CheckAllBands(const std::map<int, T> &oMap, int nExpectedBandCount)
{
    int iExpected = 0;
    for (const auto &kv : oMap)
    {
        if (kv.first != iExpected)
            return false;
        ++iExpected;
    }
    return iExpected == nExpectedBandCount;
}

/************************************************************************/
/*                        LocalScaleOffsetInit()                        */
/************************************************************************/

/** Init function for 'LocalScaleOffset' builtin function. */
static CPLErr
LocalScaleOffsetInit(const char * /*pszFuncName*/, void * /*pUserData*/,
                     CSLConstList papszFunctionArgs, int nInBands,
                     GDALDataType eInDT, double *padfInNoData, int *pnOutBands,
                     GDALDataType *peOutDT, double **ppadfOutNoData,
                     const char *pszVRTPath, VRTPDWorkingDataPtr *ppWorkingData)
{
    CPLAssert(eInDT == GDT_Float64);

    const bool bIsFinalStep = *pnOutBands != 0;
    *peOutDT = eInDT;
    *ppWorkingData = nullptr;

    if (!bIsFinalStep)
    {
        *pnOutBands = nInBands;
    }

    auto data = std::make_unique<LocalScaleOffsetData>();

    bool bNodataSpecified = false;
    double dfNoData = std::numeric_limits<double>::quiet_NaN();

    bool bGainNodataSpecified = false;
    bool bOffsetNodataSpecified = false;

    std::map<int, std::string> oGainDatasetNameMap;
    std::map<int, int> oGainDatasetBandMap;

    std::map<int, std::string> oOffsetDatasetNameMap;
    std::map<int, int> oOffsetDatasetBandMap;

    bool bRelativeToVRT = false;

    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(papszFunctionArgs))
    {
        if (EQUAL(pszKey, "relativeToVRT"))
        {
            bRelativeToVRT = CPLTestBool(pszValue);
        }
        else if (EQUAL(pszKey, "nodata"))
        {
            bNodataSpecified = true;
            dfNoData = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "gain_nodata"))
        {
            bGainNodataSpecified = true;
            data->m_dfGainNodata = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "offset_nodata"))
        {
            bOffsetNodataSpecified = true;
            data->m_dfOffsetNodata = CPLAtof(pszValue);
        }
        else if (STARTS_WITH_CI(pszKey, "gain_dataset_filename_"))
        {
            const int nBand = atoi(pszKey + strlen("gain_dataset_filename_"));
            if (nBand <= 0 || nBand > nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            oGainDatasetNameMap[nBand - 1] = pszValue;
        }
        else if (STARTS_WITH_CI(pszKey, "gain_dataset_band_"))
        {
            const int nBand = atoi(pszKey + strlen("gain_dataset_band_"));
            if (nBand <= 0 || nBand > nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            oGainDatasetBandMap[nBand - 1] = atoi(pszValue);
        }
        else if (STARTS_WITH_CI(pszKey, "offset_dataset_filename_"))
        {
            const int nBand = atoi(pszKey + strlen("offset_dataset_filename_"));
            if (nBand <= 0 || nBand > nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            oOffsetDatasetNameMap[nBand - 1] = pszValue;
        }
        else if (STARTS_WITH_CI(pszKey, "offset_dataset_band_"))
        {
            const int nBand = atoi(pszKey + strlen("offset_dataset_band_"));
            if (nBand <= 0 || nBand > nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            oOffsetDatasetBandMap[nBand - 1] = atoi(pszValue);
        }
        else if (EQUAL(pszKey, "min"))
        {
            data->m_dfClampMin = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "max"))
        {
            data->m_dfClampMax = CPLAtof(pszValue);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unrecognized argument name %s. Ignored", pszKey);
        }
    }

    if (!CheckAllBands(oGainDatasetNameMap, nInBands))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing gain_dataset_filename_XX element(s)");
        return CE_Failure;
    }
    if (!CheckAllBands(oGainDatasetBandMap, nInBands))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing gain_dataset_band_XX element(s)");
        return CE_Failure;
    }
    if (!CheckAllBands(oOffsetDatasetNameMap, nInBands))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing offset_dataset_filename_XX element(s)");
        return CE_Failure;
    }
    if (!CheckAllBands(oOffsetDatasetBandMap, nInBands))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing offset_dataset_band_XX element(s)");
        return CE_Failure;
    }

    data->m_oGainBands.resize(nInBands);
    data->m_oOffsetBands.resize(nInBands);

    constexpr int IDX_GAIN = 0;
    constexpr int IDX_OFFSET = 1;
    for (int i : {IDX_GAIN, IDX_OFFSET})
    {
        const auto &oMapNames =
            (i == IDX_GAIN) ? oGainDatasetNameMap : oOffsetDatasetNameMap;
        const auto &oMapBands =
            (i == IDX_GAIN) ? oGainDatasetBandMap : oOffsetDatasetBandMap;
        for (const auto &kv : oMapNames)
        {
            const int nInBandIdx = kv.first;
            const auto osFilename = VRTDataset::BuildSourceFilename(
                kv.second.c_str(), pszVRTPath, bRelativeToVRT);
            auto oIter = data->m_oDatasetMap.find(osFilename);
            if (oIter == data->m_oDatasetMap.end())
            {
                auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                    osFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                    nullptr, nullptr, nullptr));
                if (!poDS)
                    return CE_Failure;
                double adfAuxGT[6];
                if (poDS->GetGeoTransform(adfAuxGT) != CE_None)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s lacks a geotransform", osFilename.c_str());
                    return CE_Failure;
                }
                oIter = data->m_oDatasetMap
                            .insert(std::pair(osFilename, std::move(poDS)))
                            .first;
            }
            auto poDS = oIter->second.get();
            const auto oIterBand = oMapBands.find(nInBandIdx);
            CPLAssert(oIterBand != oMapBands.end());
            const int nAuxBand = oIterBand->second;
            if (nAuxBand <= 0 || nAuxBand > poDS->GetRasterCount())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band number (%d) for a %s dataset", nAuxBand,
                         (i == IDX_GAIN) ? "gain" : "offset");
                return CE_Failure;
            }
            auto poAuxBand = poDS->GetRasterBand(nAuxBand);
            int bAuxBandHasNoData = false;
            const double dfAuxNoData =
                poAuxBand->GetNoDataValue(&bAuxBandHasNoData);
            if (i == IDX_GAIN)
            {
                data->m_oGainBands[nInBandIdx] = poAuxBand;
                if (!bGainNodataSpecified && bAuxBandHasNoData)
                    data->m_dfGainNodata = dfAuxNoData;
            }
            else
            {
                data->m_oOffsetBands[nInBandIdx] = poAuxBand;
                if (!bOffsetNodataSpecified && bAuxBandHasNoData)
                    data->m_dfOffsetNodata = dfAuxNoData;
            }
        }
    }

    SetOutputValuesForInNoDataAndOutNoData(
        nInBands, padfInNoData, pnOutBands, ppadfOutNoData, bNodataSpecified,
        dfNoData, bNodataSpecified, dfNoData, bIsFinalStep);

    *ppWorkingData = data.release();
    return CE_None;
}

/************************************************************************/
/*                       LocalScaleOffsetFree()                         */
/************************************************************************/

/** Free function for 'LocalScaleOffset' builtin function. */
static void LocalScaleOffsetFree(const char * /*pszFuncName*/,
                                 void * /*pUserData*/,
                                 VRTPDWorkingDataPtr pWorkingData)
{
    LocalScaleOffsetData *data =
        static_cast<LocalScaleOffsetData *>(pWorkingData);
    CPLAssert(data->m_osSignature == LocalScaleOffsetData::EXPECTED_SIGNATURE);
    CPL_IGNORE_RET_VAL(data->m_osSignature);
    delete data;
}

/************************************************************************/
/*                          LoadAuxData()                               */
/************************************************************************/

// Load auxiliary corresponding offset, gain or trimming data.
static bool LoadAuxData(double dfULX, double dfULY, double dfLRX, double dfLRY,
                        size_t nElts, int nBufXSize, int nBufYSize,
                        const char *pszAuxType, GDALRasterBand *poAuxBand,
                        std::vector<VRTProcessedDataset::NoInitByte> &abyBuffer)
{
    double adfAuxGT[6];
    double adfAuxInvGT[6];

    // Compute pixel/line coordinates from the georeferenced extent
    CPL_IGNORE_RET_VAL(poAuxBand->GetDataset()->GetGeoTransform(
        adfAuxGT));  // return code already tested
    CPL_IGNORE_RET_VAL(GDALInvGeoTransform(adfAuxGT, adfAuxInvGT));
    const double dfULPixel =
        adfAuxInvGT[0] + adfAuxInvGT[1] * dfULX + adfAuxInvGT[2] * dfULY;
    const double dfULLine =
        adfAuxInvGT[3] + adfAuxInvGT[4] * dfULX + adfAuxInvGT[5] * dfULY;
    const double dfLRPixel =
        adfAuxInvGT[0] + adfAuxInvGT[1] * dfLRX + adfAuxInvGT[2] * dfLRY;
    const double dfLRLine =
        adfAuxInvGT[3] + adfAuxInvGT[4] * dfLRX + adfAuxInvGT[5] * dfLRY;
    if (dfULPixel >= dfLRPixel || dfULLine >= dfLRLine)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unexpected computed %s pixel/line", pszAuxType);
        return false;
    }
    if (dfULPixel < -1 || dfULLine < -1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unexpected computed %s upper left (pixel,line)=(%f,%f)",
                 pszAuxType, dfULPixel, dfULLine);
        return false;
    }
    if (dfLRPixel > poAuxBand->GetXSize() + 1 ||
        dfLRLine > poAuxBand->GetYSize() + 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unexpected computed %s lower right (pixel,line)=(%f,%f)",
                 pszAuxType, dfLRPixel, dfLRLine);
        return false;
    }

    const int nAuxXOff = std::max(0, static_cast<int>(std::round(dfULPixel)));
    const int nAuxYOff = std::max(0, static_cast<int>(std::round(dfULLine)));
    const int nAuxX2Off = std::min(poAuxBand->GetXSize(),
                                   static_cast<int>(std::round(dfLRPixel)));
    const int nAuxY2Off =
        std::min(poAuxBand->GetYSize(), static_cast<int>(std::round(dfLRLine)));

    try
    {
        abyBuffer.resize(nElts * sizeof(float));
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating working buffer");
        return false;
    }
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    sExtraArg.bFloatingPointWindowValidity = true;
    CPL_IGNORE_RET_VAL(sExtraArg.eResampleAlg);
    sExtraArg.eResampleAlg = GRIORA_Bilinear;
    sExtraArg.dfXOff = std::max(0.0, dfULPixel);
    sExtraArg.dfYOff = std::max(0.0, dfULLine);
    sExtraArg.dfXSize = std::min<double>(poAuxBand->GetXSize(), dfLRPixel) -
                        std::max(0.0, dfULPixel);
    sExtraArg.dfYSize = std::min<double>(poAuxBand->GetYSize(), dfLRLine) -
                        std::max(0.0, dfULLine);
    return (poAuxBand->RasterIO(
                GF_Read, nAuxXOff, nAuxYOff, std::max(1, nAuxX2Off - nAuxXOff),
                std::max(1, nAuxY2Off - nAuxYOff), abyBuffer.data(), nBufXSize,
                nBufYSize, GDT_Float32, 0, 0, &sExtraArg) == CE_None);
}

/************************************************************************/
/*                      LocalScaleOffsetProcess()                       */
/************************************************************************/

/** Processing function for 'LocalScaleOffset' builtin function. */
static CPLErr LocalScaleOffsetProcess(
    const char * /*pszFuncName*/, void * /*pUserData*/,
    VRTPDWorkingDataPtr pWorkingData, CSLConstList /* papszFunctionArgs*/,
    int nBufXSize, int nBufYSize, const void *pInBuffer, size_t nInBufferSize,
    GDALDataType eInDT, int nInBands, const double *CPL_RESTRICT padfInNoData,
    void *pOutBuffer, size_t nOutBufferSize, GDALDataType eOutDT, int nOutBands,
    const double *CPL_RESTRICT padfOutNoData, double dfSrcXOff,
    double dfSrcYOff, double dfSrcXSize, double dfSrcYSize,
    const double adfSrcGT[], const char * /* pszVRTPath */,
    CSLConstList /*papszExtra*/)
{
    const size_t nElts = static_cast<size_t>(nBufXSize) * nBufYSize;

    CPL_IGNORE_RET_VAL(eInDT);
    CPLAssert(eInDT == GDT_Float64);
    CPL_IGNORE_RET_VAL(eOutDT);
    CPLAssert(eOutDT == GDT_Float64);
    CPL_IGNORE_RET_VAL(nInBufferSize);
    CPLAssert(nInBufferSize == nElts * nInBands * sizeof(double));
    CPL_IGNORE_RET_VAL(nOutBufferSize);
    CPLAssert(nOutBufferSize == nElts * nOutBands * sizeof(double));
    CPLAssert(nInBands == nOutBands);
    CPL_IGNORE_RET_VAL(nOutBands);

    LocalScaleOffsetData *data =
        static_cast<LocalScaleOffsetData *>(pWorkingData);
    CPLAssert(data->m_osSignature == LocalScaleOffsetData::EXPECTED_SIGNATURE);
    const double *CPL_RESTRICT padfSrc = static_cast<const double *>(pInBuffer);
    double *CPL_RESTRICT padfDst = static_cast<double *>(pOutBuffer);

    // Compute georeferenced extent of input region
    const double dfULX =
        adfSrcGT[0] + adfSrcGT[1] * dfSrcXOff + adfSrcGT[2] * dfSrcYOff;
    const double dfULY =
        adfSrcGT[3] + adfSrcGT[4] * dfSrcXOff + adfSrcGT[5] * dfSrcYOff;
    const double dfLRX = adfSrcGT[0] + adfSrcGT[1] * (dfSrcXOff + dfSrcXSize) +
                         adfSrcGT[2] * (dfSrcYOff + dfSrcYSize);
    const double dfLRY = adfSrcGT[3] + adfSrcGT[4] * (dfSrcXOff + dfSrcXSize) +
                         adfSrcGT[5] * (dfSrcYOff + dfSrcYSize);

    auto &abyOffsetBuffer = data->m_abyGainBuffer;
    auto &abyGainBuffer = data->m_abyOffsetBuffer;

    for (int iBand = 0; iBand < nInBands; ++iBand)
    {
        if (!LoadAuxData(dfULX, dfULY, dfLRX, dfLRY, nElts, nBufXSize,
                         nBufYSize, "gain", data->m_oGainBands[iBand],
                         abyGainBuffer) ||
            !LoadAuxData(dfULX, dfULY, dfLRX, dfLRY, nElts, nBufXSize,
                         nBufYSize, "offset", data->m_oOffsetBands[iBand],
                         abyOffsetBuffer))
        {
            return CE_Failure;
        }

        const double *CPL_RESTRICT padfSrcThisBand = padfSrc + iBand;
        double *CPL_RESTRICT padfDstThisBand = padfDst + iBand;
        const float *pafGain =
            reinterpret_cast<const float *>(abyGainBuffer.data());
        const float *pafOffset =
            reinterpret_cast<const float *>(abyOffsetBuffer.data());
        const double dfSrcNodata = padfInNoData[iBand];
        const double dfDstNodata = padfOutNoData[iBand];
        const double dfGainNodata = data->m_dfGainNodata;
        const double dfOffsetNodata = data->m_dfOffsetNodata;
        const double dfClampMin = data->m_dfClampMin;
        const double dfClampMax = data->m_dfClampMax;
        for (size_t i = 0; i < nElts; ++i)
        {
            const double dfSrcVal = *padfSrcThisBand;
            // written this way to work with a NaN value
            if (!(dfSrcVal != dfSrcNodata))
            {
                *padfDstThisBand = dfDstNodata;
            }
            else
            {
                const double dfGain = pafGain[i];
                const double dfOffset = pafOffset[i];
                if (!(dfGain != dfGainNodata) || !(dfOffset != dfOffsetNodata))
                {
                    *padfDstThisBand = dfDstNodata;
                }
                else
                {
                    double dfUnscaled = dfSrcVal * dfGain - dfOffset;
                    if (dfUnscaled < dfClampMin)
                        dfUnscaled = dfClampMin;
                    if (dfUnscaled > dfClampMax)
                        dfUnscaled = dfClampMax;

                    *padfDstThisBand = dfUnscaled;
                }
            }
            padfSrcThisBand += nInBands;
            padfDstThisBand += nInBands;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           TrimmingData                               */
/************************************************************************/

namespace
{
/** Working structure for 'Trimming' builtin function. */
struct TrimmingData
{
    static constexpr const char *const EXPECTED_SIGNATURE = "Trimming";
    //! Signature (to make sure callback functions are called with the right argument)
    const std::string m_osSignature = EXPECTED_SIGNATURE;

    //! Nodata value for trimming dataset
    double m_dfTrimmingNodata = std::numeric_limits<double>::quiet_NaN();

    //! Maximum saturating RGB output value.
    double m_dfTopRGB = 0;

    //! Maximum threshold beyond which we give up saturation
    double m_dfToneCeil = 0;

    //! Margin to allow for dynamics in brighest areas (in [0,1] range)
    double m_dfTopMargin = 0;

    //! Index (zero-based) of input/output red band.
    int m_nRedBand = 1 - 1;

    //! Index (zero-based) of input/output green band.
    int m_nGreenBand = 2 - 1;

    //! Index (zero-based) of input/output blue band.
    int m_nBlueBand = 3 - 1;

    //! Trimming dataset
    std::unique_ptr<GDALDataset> m_poTrimmingDS{};

    //! Trimming raster band.
    GDALRasterBand *m_poTrimmingBand = nullptr;

    //! Working buffer that contain trimming values.
    std::vector<VRTProcessedDataset::NoInitByte> m_abyTrimmingBuffer{};
};
}  // namespace

/************************************************************************/
/*                           TrimmingInit()                             */
/************************************************************************/

/** Init function for 'Trimming' builtin function. */
static CPLErr TrimmingInit(const char * /*pszFuncName*/, void * /*pUserData*/,
                           CSLConstList papszFunctionArgs, int nInBands,
                           GDALDataType eInDT, double *padfInNoData,
                           int *pnOutBands, GDALDataType *peOutDT,
                           double **ppadfOutNoData, const char *pszVRTPath,
                           VRTPDWorkingDataPtr *ppWorkingData)
{
    CPLAssert(eInDT == GDT_Float64);

    const bool bIsFinalStep = *pnOutBands != 0;
    *peOutDT = eInDT;
    *ppWorkingData = nullptr;

    if (!bIsFinalStep)
    {
        *pnOutBands = nInBands;
    }

    auto data = std::make_unique<TrimmingData>();

    bool bNodataSpecified = false;
    double dfNoData = std::numeric_limits<double>::quiet_NaN();
    std::string osTrimmingFilename;
    bool bTrimmingNodataSpecified = false;
    bool bRelativeToVRT = false;

    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(papszFunctionArgs))
    {
        if (EQUAL(pszKey, "relativeToVRT"))
        {
            bRelativeToVRT = CPLTestBool(pszValue);
        }
        else if (EQUAL(pszKey, "nodata"))
        {
            bNodataSpecified = true;
            dfNoData = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "trimming_nodata"))
        {
            bTrimmingNodataSpecified = true;
            data->m_dfTrimmingNodata = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "trimming_dataset_filename"))
        {
            osTrimmingFilename = pszValue;
        }
        else if (EQUAL(pszKey, "red_band"))
        {
            const int nBand = atoi(pszValue) - 1;
            if (nBand < 0 || nBand >= nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            data->m_nRedBand = nBand;
        }
        else if (EQUAL(pszKey, "green_band"))
        {
            const int nBand = atoi(pszValue) - 1;
            if (nBand < 0 || nBand >= nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            data->m_nGreenBand = nBand;
        }
        else if (EQUAL(pszKey, "blue_band"))
        {
            const int nBand = atoi(pszValue) - 1;
            if (nBand < 0 || nBand >= nInBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid band in argument '%s'", pszKey);
                return CE_Failure;
            }
            data->m_nBlueBand = nBand;
        }
        else if (EQUAL(pszKey, "top_rgb"))
        {
            data->m_dfTopRGB = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "tone_ceil"))
        {
            data->m_dfToneCeil = CPLAtof(pszValue);
        }
        else if (EQUAL(pszKey, "top_margin"))
        {
            data->m_dfTopMargin = CPLAtof(pszValue);
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unrecognized argument name %s. Ignored", pszKey);
        }
    }

    if (data->m_nRedBand == data->m_nGreenBand ||
        data->m_nRedBand == data->m_nBlueBand ||
        data->m_nGreenBand == data->m_nBlueBand)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "red_band, green_band and blue_band must have distinct values");
        return CE_Failure;
    }

    const auto osFilename = VRTDataset::BuildSourceFilename(
        osTrimmingFilename.c_str(), pszVRTPath, bRelativeToVRT);
    data->m_poTrimmingDS.reset(GDALDataset::Open(
        osFilename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, nullptr,
        nullptr, nullptr));
    if (!data->m_poTrimmingDS)
        return CE_Failure;
    if (data->m_poTrimmingDS->GetRasterCount() != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Trimming dataset should have a single band");
        return CE_Failure;
    }
    data->m_poTrimmingBand = data->m_poTrimmingDS->GetRasterBand(1);

    double adfAuxGT[6];
    if (data->m_poTrimmingDS->GetGeoTransform(adfAuxGT) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s lacks a geotransform",
                 osFilename.c_str());
        return CE_Failure;
    }
    int bAuxBandHasNoData = false;
    const double dfAuxNoData =
        data->m_poTrimmingBand->GetNoDataValue(&bAuxBandHasNoData);
    if (!bTrimmingNodataSpecified && bAuxBandHasNoData)
        data->m_dfTrimmingNodata = dfAuxNoData;

    SetOutputValuesForInNoDataAndOutNoData(
        nInBands, padfInNoData, pnOutBands, ppadfOutNoData, bNodataSpecified,
        dfNoData, bNodataSpecified, dfNoData, bIsFinalStep);

    *ppWorkingData = data.release();
    return CE_None;
}

/************************************************************************/
/*                           TrimmingFree()                             */
/************************************************************************/

/** Free function for 'Trimming' builtin function. */
static void TrimmingFree(const char * /*pszFuncName*/, void * /*pUserData*/,
                         VRTPDWorkingDataPtr pWorkingData)
{
    TrimmingData *data = static_cast<TrimmingData *>(pWorkingData);
    CPLAssert(data->m_osSignature == TrimmingData::EXPECTED_SIGNATURE);
    CPL_IGNORE_RET_VAL(data->m_osSignature);
    delete data;
}

/************************************************************************/
/*                         TrimmingProcess()                            */
/************************************************************************/

/** Processing function for 'Trimming' builtin function. */
static CPLErr TrimmingProcess(
    const char * /*pszFuncName*/, void * /*pUserData*/,
    VRTPDWorkingDataPtr pWorkingData, CSLConstList /* papszFunctionArgs*/,
    int nBufXSize, int nBufYSize, const void *pInBuffer, size_t nInBufferSize,
    GDALDataType eInDT, int nInBands, const double *CPL_RESTRICT padfInNoData,
    void *pOutBuffer, size_t nOutBufferSize, GDALDataType eOutDT, int nOutBands,
    const double *CPL_RESTRICT padfOutNoData, double dfSrcXOff,
    double dfSrcYOff, double dfSrcXSize, double dfSrcYSize,
    const double adfSrcGT[], const char * /* pszVRTPath */,
    CSLConstList /*papszExtra*/)
{
    const size_t nElts = static_cast<size_t>(nBufXSize) * nBufYSize;

    CPL_IGNORE_RET_VAL(eInDT);
    CPLAssert(eInDT == GDT_Float64);
    CPL_IGNORE_RET_VAL(eOutDT);
    CPLAssert(eOutDT == GDT_Float64);
    CPL_IGNORE_RET_VAL(nInBufferSize);
    CPLAssert(nInBufferSize == nElts * nInBands * sizeof(double));
    CPL_IGNORE_RET_VAL(nOutBufferSize);
    CPLAssert(nOutBufferSize == nElts * nOutBands * sizeof(double));
    CPLAssert(nInBands == nOutBands);
    CPL_IGNORE_RET_VAL(nOutBands);

    TrimmingData *data = static_cast<TrimmingData *>(pWorkingData);
    CPLAssert(data->m_osSignature == TrimmingData::EXPECTED_SIGNATURE);
    const double *CPL_RESTRICT padfSrc = static_cast<const double *>(pInBuffer);
    double *CPL_RESTRICT padfDst = static_cast<double *>(pOutBuffer);

    // Compute georeferenced extent of input region
    const double dfULX =
        adfSrcGT[0] + adfSrcGT[1] * dfSrcXOff + adfSrcGT[2] * dfSrcYOff;
    const double dfULY =
        adfSrcGT[3] + adfSrcGT[4] * dfSrcXOff + adfSrcGT[5] * dfSrcYOff;
    const double dfLRX = adfSrcGT[0] + adfSrcGT[1] * (dfSrcXOff + dfSrcXSize) +
                         adfSrcGT[2] * (dfSrcYOff + dfSrcYSize);
    const double dfLRY = adfSrcGT[3] + adfSrcGT[4] * (dfSrcXOff + dfSrcXSize) +
                         adfSrcGT[5] * (dfSrcYOff + dfSrcYSize);

    if (!LoadAuxData(dfULX, dfULY, dfLRX, dfLRY, nElts, nBufXSize, nBufYSize,
                     "trimming", data->m_poTrimmingBand,
                     data->m_abyTrimmingBuffer))
    {
        return CE_Failure;
    }

    const float *pafTrimming =
        reinterpret_cast<const float *>(data->m_abyTrimmingBuffer.data());
    const int nRedBand = data->m_nRedBand;
    const int nGreenBand = data->m_nGreenBand;
    const int nBlueBand = data->m_nBlueBand;
    const double dfTopMargin = data->m_dfTopMargin;
    const double dfTopRGB = data->m_dfTopRGB;
    const double dfToneCeil = data->m_dfToneCeil;
#if !defined(trimming_non_optimized_version)
    const double dfInvToneCeil = 1.0 / dfToneCeil;
#endif
    const bool bRGBBandsAreFirst =
        std::max(std::max(nRedBand, nGreenBand), nBlueBand) <= 2;
    const double dfNoDataTrimming = data->m_dfTrimmingNodata;
    const double dfNoDataRed = padfInNoData[nRedBand];
    const double dfNoDataGreen = padfInNoData[nGreenBand];
    const double dfNoDataBlue = padfInNoData[nBlueBand];
    for (size_t i = 0; i < nElts; ++i)
    {
        // Extract local saturation value from trimming image
        const double dfLocalMaxRGB = pafTrimming[i];
        const double dfReducedRGB =
            std::min((1.0 - dfTopMargin) * dfTopRGB / dfLocalMaxRGB, 1.0);

        const double dfRed = padfSrc[nRedBand];
        const double dfGreen = padfSrc[nGreenBand];
        const double dfBlue = padfSrc[nBlueBand];
        bool bNoDataPixel = false;
        if ((dfLocalMaxRGB != dfNoDataTrimming) && (dfRed != dfNoDataRed) &&
            (dfGreen != dfNoDataGreen) && (dfBlue != dfNoDataBlue))
        {
            // RGB bands specific process
            const double dfMaxRGB = std::max(std::max(dfRed, dfGreen), dfBlue);
#if !defined(trimming_non_optimized_version)
            const double dfRedTimesToneRed = std::min(dfRed, dfToneCeil);
            const double dfGreenTimesToneGreen = std::min(dfGreen, dfToneCeil);
            const double dfBlueTimesToneBlue = std::min(dfBlue, dfToneCeil);
            const double dfInvToneMaxRGB =
                std::max(dfMaxRGB * dfInvToneCeil, 1.0);
            const double dfReducedRGBTimesInvToneMaxRGB =
                dfReducedRGB * dfInvToneMaxRGB;
            padfDst[nRedBand] = std::min(
                dfRedTimesToneRed * dfReducedRGBTimesInvToneMaxRGB, dfTopRGB);
            padfDst[nGreenBand] =
                std::min(dfGreenTimesToneGreen * dfReducedRGBTimesInvToneMaxRGB,
                         dfTopRGB);
            padfDst[nBlueBand] = std::min(
                dfBlueTimesToneBlue * dfReducedRGBTimesInvToneMaxRGB, dfTopRGB);
#else
            // Original formulas. Slightly less optimized than the above ones.
            const double dfToneMaxRGB = std::min(dfToneCeil / dfMaxRGB, 1.0);
            const double dfToneRed = std::min(dfToneCeil / dfRed, 1.0);
            const double dfToneGreen = std::min(dfToneCeil / dfGreen, 1.0);
            const double dfToneBlue = std::min(dfToneCeil / dfBlue, 1.0);
            padfDst[nRedBand] = std::min(
                dfReducedRGB * dfRed * dfToneRed / dfToneMaxRGB, dfTopRGB);
            padfDst[nGreenBand] = std::min(
                dfReducedRGB * dfGreen * dfToneGreen / dfToneMaxRGB, dfTopRGB);
            padfDst[nBlueBand] = std::min(
                dfReducedRGB * dfBlue * dfToneBlue / dfToneMaxRGB, dfTopRGB);
#endif

            // Other bands processing (NIR, ...): only apply RGB reduction factor
            if (bRGBBandsAreFirst)
            {
                // optimization
                for (int iBand = 3; iBand < nInBands; ++iBand)
                {
                    if (padfSrc[iBand] != padfInNoData[iBand])
                    {
                        padfDst[iBand] = dfReducedRGB * padfSrc[iBand];
                    }
                    else
                    {
                        bNoDataPixel = true;
                        break;
                    }
                }
            }
            else
            {
                for (int iBand = 0; iBand < nInBands; ++iBand)
                {
                    if (iBand != nRedBand && iBand != nGreenBand &&
                        iBand != nBlueBand)
                    {
                        if (padfSrc[iBand] != padfInNoData[iBand])
                        {
                            padfDst[iBand] = dfReducedRGB * padfSrc[iBand];
                        }
                        else
                        {
                            bNoDataPixel = true;
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            bNoDataPixel = true;
        }
        if (bNoDataPixel)
        {
            for (int iBand = 0; iBand < nInBands; ++iBand)
            {
                padfDst[iBand] = padfOutNoData[iBand];
            }
        }

        padfSrc += nInBands;
        padfDst += nInBands;
    }

    return CE_None;
}

/************************************************************************/
/*              GDALVRTRegisterDefaultProcessedDatasetFuncs()           */
/************************************************************************/

/** Register builtin functions that can be used in a VRTProcessedDataset.
 */
void GDALVRTRegisterDefaultProcessedDatasetFuncs()
{
    GDALVRTRegisterProcessedDatasetFunc(
        "BandAffineCombination", nullptr,
        "<ProcessedDatasetFunctionArgumentsList>"
        "   <Argument name='src_nodata' type='double' "
        "description='Override input nodata value'/>"
        "   <Argument name='dst_nodata' type='double' "
        "description='Override output nodata value'/>"
        "   <Argument name='replacement_nodata' "
        "description='value to substitute to a valid computed value that "
        "would be nodata' type='double'/>"
        "   <Argument name='dst_intended_datatype' type='string' "
        "description='Intented datatype of output (which might be "
        "different than the working data type)'/>"
        "   <Argument name='coefficients_{band}' "
        "description='Comma-separated coefficients for combining bands. "
        "First one is constant term' "
        "type='double_list' required='true'/>"
        "   <Argument name='min' description='clamp min value' type='double'/>"
        "   <Argument name='max' description='clamp max value' type='double'/>"
        "</ProcessedDatasetFunctionArgumentsList>",
        GDT_Float64, nullptr, 0, nullptr, 0, BandAffineCombinationInit,
        BandAffineCombinationFree, BandAffineCombinationProcess, nullptr);

    GDALVRTRegisterProcessedDatasetFunc(
        "LUT", nullptr,
        "<ProcessedDatasetFunctionArgumentsList>"
        "   <Argument name='src_nodata' type='double' "
        "description='Override input nodata value'/>"
        "   <Argument name='dst_nodata' type='double' "
        "description='Override output nodata value'/>"
        "   <Argument name='lut_{band}' "
        "description='List of the form [src value 1]:[dest value 1],"
        "[src value 2]:[dest value 2],...' "
        "type='string' required='true'/>"
        "</ProcessedDatasetFunctionArgumentsList>",
        GDT_Float64, nullptr, 0, nullptr, 0, LUTInit, LUTFree, LUTProcess,
        nullptr);

    GDALVRTRegisterProcessedDatasetFunc(
        "LocalScaleOffset", nullptr,
        "<ProcessedDatasetFunctionArgumentsList>"
        "   <Argument name='relativeToVRT' "
        "description='Whether gain and offset filenames are relative to "
        "the VRT' type='boolean' default='false'/>"
        "   <Argument name='gain_dataset_filename_{band}' "
        "description='Filename to the gain dataset' "
        "type='string' required='true'/>"
        "   <Argument name='gain_dataset_band_{band}' "
        "description='Band of the gain dataset' "
        "type='integer' required='true'/>"
        "   <Argument name='offset_dataset_filename_{band}' "
        "description='Filename to the offset dataset' "
        "type='string' required='true'/>"
        "   <Argument name='offset_dataset_band_{band}' "
        "description='Band of the offset dataset' "
        "type='integer' required='true'/>"
        "   <Argument name='min' description='clamp min value' type='double'/>"
        "   <Argument name='max' description='clamp max value' type='double'/>"
        "   <Argument name='nodata' type='double' "
        "description='Override dataset nodata value'/>"
        "   <Argument name='gain_nodata' type='double' "
        "description='Override gain dataset nodata value'/>"
        "   <Argument name='offset_nodata' type='double' "
        "description='Override offset dataset nodata value'/>"
        "</ProcessedDatasetFunctionArgumentsList>",
        GDT_Float64, nullptr, 0, nullptr, 0, LocalScaleOffsetInit,
        LocalScaleOffsetFree, LocalScaleOffsetProcess, nullptr);

    GDALVRTRegisterProcessedDatasetFunc(
        "Trimming", nullptr,
        "<ProcessedDatasetFunctionArgumentsList>"
        "   <Argument name='relativeToVRT' "
        "description='Whether trimming_dataset_filename is relative to the VRT'"
        " type='boolean' default='false'/>"
        "   <Argument name='trimming_dataset_filename' "
        "description='Filename to the trimming dataset' "
        "type='string' required='true'/>"
        "   <Argument name='red_band' type='integer' default='1'/>"
        "   <Argument name='green_band' type='integer' default='2'/>"
        "   <Argument name='blue_band' type='integer' default='3'/>"
        "   <Argument name='top_rgb' "
        "description='Maximum saturating RGB output value' "
        "type='double' required='true'/>"
        "   <Argument name='tone_ceil' "
        "description='Maximum threshold beyond which we give up saturation' "
        "type='double' required='true'/>"
        "   <Argument name='top_margin' "
        "description='Margin to allow for dynamics in brighest areas "
        "(between 0 and 1, should be close to 0)' "
        "type='double' required='true'/>"
        "   <Argument name='nodata' type='double' "
        "description='Override dataset nodata value'/>"
        "   <Argument name='trimming_nodata' type='double' "
        "description='Override trimming dataset nodata value'/>"
        "</ProcessedDatasetFunctionArgumentsList>",
        GDT_Float64, nullptr, 0, nullptr, 0, TrimmingInit, TrimmingFree,
        TrimmingProcess, nullptr);
}
