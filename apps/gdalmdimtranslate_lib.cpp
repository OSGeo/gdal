/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to convert a multidimensional raster
 * Author:   Even Rouault,<even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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
#include "commonutils.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"
#include "vrtdataset.h"
#include <algorithm>
#include <map>
#include <set>

CPL_CVSID("$Id$")

/************************************************************************/
/*                     GDALMultiDimTranslateOptions                     */
/************************************************************************/

struct GDALMultiDimTranslateOptions
{
    std::string osFormat{};
    CPLStringList aosCreateOptions{};
    std::vector<std::string> aosArraySpec{};
    std::vector<std::string> aosSubset{};
    std::vector<std::string> aosScaleFactor{};
    std::vector<std::string> aosGroup{};
    GDALProgressFunc pfnProgress = GDALDummyProgress;
    bool bStrict = false;
    void *pProgressData = nullptr;
    bool bUpdate = false;
};

/************************************************************************/
/*                        FindMinMaxIdxNumeric()                        */
/************************************************************************/

static void FindMinMaxIdxNumeric(const GDALMDArray* var,
                                 double* pdfTmp,
                                 const size_t nCount,
                                 const GUInt64 nStartIdx,
                                 const double dfMin,
                                 const double dfMax,
                                 const bool bSlice,
                                 bool& bFoundMinIdx,
                                 GUInt64& nMinIdx,
                                 bool& bFoundMaxIdx,
                                 GUInt64 &nMaxIdx,
                                 bool& bLastWasReversed,
                                 bool& bEmpty,
                                 const double EPS)
{
    if( nCount >= 2 )
    {
        bool bReversed = false;
        if( pdfTmp[0] > pdfTmp[nCount-1] )
        {
            bReversed = true;
            std::reverse(pdfTmp, pdfTmp + nCount);
        }
        if( nStartIdx > 0 && bLastWasReversed != bReversed )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Variable %s is non monotonic",
                        var->GetName().c_str());
            bEmpty = true;
            return;
        }
        bLastWasReversed = bReversed;

        if( !bFoundMinIdx )
        {
            if( bReversed && nStartIdx == 0 && dfMin > pdfTmp[nCount-1] )
            {
                bEmpty = true;
                return;
            }
            else if( !bReversed && dfMin < pdfTmp[0] - EPS )
            {
                if( bSlice )
                {
                    bEmpty = true;
                    return;
                }
                bFoundMinIdx = true;
                nMinIdx = nStartIdx;
            }
            else if( dfMin >= pdfTmp[0] - EPS &&
                        dfMin <= pdfTmp[nCount-1] + EPS )
            {
                for( size_t i = 0; i < nCount; i++ )
                {
                    if( dfMin <= pdfTmp[i] + EPS )
                    {
                        bFoundMinIdx = true;
                        nMinIdx = nStartIdx + (bReversed ? nCount - 1 - i : i);
                        break;
                    }
                }
                CPLAssert(bFoundMinIdx);
            }
        }
        if( !bFoundMaxIdx )
        {
            if( bReversed && nStartIdx == 0 && dfMax > pdfTmp[nCount-1] )
            {
                if( bSlice )
                {
                    bEmpty = true;
                    return;
                }
                bFoundMaxIdx = true;
                nMaxIdx = 0;
            }
            else if( !bReversed && dfMax < pdfTmp[0] - EPS )
            {
                if( nStartIdx == 0 )
                {
                    bEmpty = true;
                    return;
                }
                bFoundMaxIdx = true;
                nMaxIdx = nStartIdx - 1;
            }
            else if( dfMax > pdfTmp[0] - EPS &&
                        dfMax <= pdfTmp[nCount-1] + EPS )
            {
                for( size_t i = 1; i < nCount; i++ )
                {
                    if( dfMax <= pdfTmp[i] - EPS )
                    {
                        bFoundMaxIdx = true;
                        nMaxIdx = nStartIdx + (bReversed ? nCount - 1 - (i - 1) : i - 1);
                        break;
                    }
                }
                if( !bFoundMaxIdx )
                {
                    bFoundMaxIdx = true;
                    nMaxIdx = nStartIdx + (bReversed ? 0 : nCount - 1);
                }
            }
        }
    }
    else
    {
        if( !bFoundMinIdx )
        {
            if( dfMin <= pdfTmp[0] + EPS )
            {
                bFoundMinIdx = true;
                nMinIdx = nStartIdx;
            }
            else if( bLastWasReversed && nStartIdx > 0 )
            {
                bFoundMinIdx = true;
                nMinIdx = nStartIdx - 1;
            }
        }
        if( !bFoundMaxIdx )
        {
            if( dfMax >= pdfTmp[0] - EPS )
            {
                bFoundMaxIdx = true;
                nMaxIdx = nStartIdx;
            }
            else if( !bLastWasReversed && nStartIdx > 0 )
            {
                bFoundMaxIdx = true;
                nMaxIdx = nStartIdx - 1;
            }
        }
    }
}

/************************************************************************/
/*                        FindMinMaxIdxString()                         */
/************************************************************************/

static void FindMinMaxIdxString(const GDALMDArray* var,
                                const char** ppszTmp,
                                 const size_t nCount,
                                 const GUInt64 nStartIdx,
                                 const std::string& osMin,
                                 const std::string& osMax,
                                 const bool bSlice,
                                 bool& bFoundMinIdx,
                                 GUInt64& nMinIdx,
                                 bool& bFoundMaxIdx,
                                 GUInt64 &nMaxIdx,
                                 bool& bLastWasReversed,
                                 bool& bEmpty)
{
    bool bFoundNull = false;
    for( size_t i = 0; i < nCount; i++ )
    {
        if( ppszTmp[i] == nullptr )
        {
            bFoundNull = true;
            break;
        }
    }
    if( bFoundNull )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Variable %s contains null strings",
                    var->GetName().c_str());
        bEmpty = true;
        return;
    }
    if( nCount >= 2 )
    {
        bool bReversed = false;
        if( std::string(ppszTmp[0]) > std::string(ppszTmp[nCount-1]) )
        {
            bReversed = true;
            std::reverse(ppszTmp, ppszTmp + nCount);
        }
        if( nStartIdx > 0 && bLastWasReversed != bReversed )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Variable %s is non monotonic",
                        var->GetName().c_str());
            bEmpty = true;
            return;
        }
        bLastWasReversed = bReversed;

        if( !bFoundMinIdx )
        {
            if( bReversed && nStartIdx == 0 && osMin > std::string(ppszTmp[nCount-1]) )
            {
                bEmpty = true;
                return;
            }
            else if( !bReversed && osMin < std::string(ppszTmp[0]) )
            {
                if( bSlice )
                {
                    bEmpty = true;
                    return;
                }
                bFoundMinIdx = true;
                nMinIdx = nStartIdx;
            }
            else if( osMin >= std::string(ppszTmp[0]) &&
                        osMin <= std::string(ppszTmp[nCount-1]) )
            {
                for( size_t i = 0; i < nCount; i++ )
                {
                    if( osMin <= std::string(ppszTmp[i]) )
                    {
                        bFoundMinIdx = true;
                        nMinIdx = nStartIdx + (bReversed ? nCount - 1 - i : i);
                        break;
                    }
                }
                CPLAssert(bFoundMinIdx);
            }
        }
        if( !bFoundMaxIdx )
        {
            if( bReversed && nStartIdx == 0 && osMax > std::string(ppszTmp[nCount-1]) )
            {
                if( bSlice )
                {
                    bEmpty = true;
                    return;
                }
                bFoundMaxIdx = true;
                nMaxIdx = 0;
            }
            else if( !bReversed && osMax < std::string(ppszTmp[0]) )
            {
                if( nStartIdx == 0 )
                {
                    bEmpty = true;
                    return;
                }
                bFoundMaxIdx = true;
                nMaxIdx = nStartIdx - 1;
            }
            else if( osMax == std::string(ppszTmp[0]) )
            {
                bFoundMaxIdx = true;
                nMaxIdx = nStartIdx + (bReversed ? nCount - 1 : 0);
            }
            else if( osMax > std::string(ppszTmp[0]) &&
                        osMax <= std::string(ppszTmp[nCount-1]) )
            {
                for( size_t i = 1; i < nCount; i++ )
                {
                    if( osMax <= std::string(ppszTmp[i]) )
                    {
                        bFoundMaxIdx = true;
                        if( osMax == std::string(ppszTmp[i]) )
                            nMaxIdx = nStartIdx + (bReversed ? nCount - 1 - i : i);
                        else
                            nMaxIdx = nStartIdx + (bReversed ? nCount - 1 - (i - 1) : i - 1);
                        break;
                    }
                }
                CPLAssert(bFoundMaxIdx);
            }
        }
    }
    else
    {
        if( !bFoundMinIdx )
        {
            if( osMin <= std::string(ppszTmp[0]) )
            {
                bFoundMinIdx = true;
                nMinIdx = nStartIdx;
            }
            else if( bLastWasReversed && nStartIdx > 0 )
            {
                bFoundMinIdx = true;
                nMinIdx = nStartIdx - 1;
            }
        }
        if( !bFoundMaxIdx )
        {
            if( osMax >= std::string(ppszTmp[0]) )
            {
                bFoundMaxIdx = true;
                nMaxIdx = nStartIdx;
            }
            else if( !bLastWasReversed && nStartIdx > 0 )
            {
                bFoundMaxIdx = true;
                nMaxIdx = nStartIdx - 1;
            }
        }
    }
}

/************************************************************************/
/*                             GetDimensionDesc()                       */
/************************************************************************/

struct DimensionDesc
{
    GUInt64 nStartIdx = 0;
    GUInt64 nStep = 1;
    GUInt64 nSize = 0;
    GUInt64 nOriSize = 0;
    bool    bSlice = false;
};

struct DimensionRemapper
{
    std::map<std::string, DimensionDesc> oMap{};
};

static const DimensionDesc* GetDimensionDesc(DimensionRemapper& oDimRemapper,
                            const GDALMultiDimTranslateOptions *psOptions,
                            const std::shared_ptr<GDALDimension>& poDim)
{
    std::string osKey(poDim->GetFullName());
    osKey += CPLSPrintf("_" CPL_FRMT_GUIB, static_cast<GUIntBig>(poDim->GetSize()));
    auto oIter = oDimRemapper.oMap.find(osKey);
    if( oIter != oDimRemapper.oMap.end() &&
        oIter->second.nOriSize == poDim->GetSize() )
    {
        return &(oIter->second);
    }
    DimensionDesc desc;
    desc.nSize = poDim->GetSize();
    desc.nOriSize = desc.nSize;

    CPLString osRadix(poDim->GetName());
    osRadix += '(';
    for( const auto& subset: psOptions->aosSubset )
    {
        if( STARTS_WITH( subset.c_str(), osRadix.c_str() ) )
        {
            auto var = poDim->GetIndexingVariable();
            if( !var || var->GetDimensionCount() != 1 ||
                var->GetDimensions()[0]->GetSize() != poDim->GetSize() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Dimension %s has a subset specification, but lacks "
                         "a single dimension indexing variable",
                         poDim->GetName().c_str());
                return nullptr;
            }
            if( subset.back() != ')' )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing ')' in subset specification.");
                return nullptr;
            }
            CPLStringList aosTokens(CSLTokenizeString2(
                subset.substr(osRadix.size(),
                              subset.size() - 1 - osRadix.size()).c_str(),
                ",", CSLT_HONOURSTRINGS));
            if( aosTokens.size() == 1 )
            {
                desc.bSlice = true;
            }
            if( aosTokens.size() != 1 && aosTokens.size() != 2 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid number of valus in subset specification.");
                return nullptr;
            }

            const bool bIsNumeric =
                var->GetDataType().GetClass() == GEDTC_NUMERIC;
            const auto dt(bIsNumeric ?
                GDALExtendedDataType::Create(GDT_Float64) :
                GDALExtendedDataType::CreateString());

            double dfMin = 0;
            double dfMax = 0;
            std::string osMin;
            std::string osMax;
            if( bIsNumeric )
            {
                if( CPLGetValueType(aosTokens[0]) == CPL_VALUE_STRING ||
                    (aosTokens.size() == 2 &&
                     CPLGetValueType(aosTokens[1]) == CPL_VALUE_STRING) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Non numeric bound in subset specification.");
                    return nullptr;
                }
                dfMin = CPLAtof(aosTokens[0]);
                dfMax = dfMin;
                if( aosTokens.size() == 2 )
                    dfMax = CPLAtof(aosTokens[1]);
                if( dfMin > dfMax )
                    std::swap(dfMin, dfMax);
            }
            else
            {
                osMin = aosTokens[0];
                osMax = osMin;
                if( aosTokens.size() == 2 )
                    osMax = aosTokens[1];
                if( osMin > osMax )
                    std::swap(osMin, osMax);
            }

            const size_t nDTSize(dt.GetSize());
            const size_t nMaxChunkSize = static_cast<size_t>(
                std::min(static_cast<GUInt64>(10*1000*1000), poDim->GetSize()));
            std::vector<GByte> abyTmp(nDTSize * nMaxChunkSize);
            double* pdfTmp = reinterpret_cast<double*>(&abyTmp[0]);
            const char** ppszTmp = reinterpret_cast<const char**>(&abyTmp[0]);
            GUInt64 nStartIdx = 0;
            const double EPS = std::max( std::max(1e-10, fabs(dfMin) / 1e10),
                                         fabs(dfMax) / 1e10 );
            bool bFoundMinIdx = false;
            bool bFoundMaxIdx = false;
            GUInt64 nMinIdx = 0;
            GUInt64 nMaxIdx = 0;
            bool bLastWasReversed = false;
            bool bEmpty = false;
            while( true )
            {
                const size_t nCount = static_cast<size_t>(
                    std::min(static_cast<GUInt64>(nMaxChunkSize),
                             poDim->GetSize() - nStartIdx));
                if( nCount == 0 )
                    break;
                const GUInt64 anStartId[] = { nStartIdx };
                const size_t anCount[] = { nCount };
                if( !var->Read(anStartId, anCount, nullptr, nullptr,
                               dt, &abyTmp[0], nullptr, 0) )
                {
                    return nullptr;
                }
                if( bIsNumeric )
                {
                    FindMinMaxIdxNumeric(var.get(),
                                         pdfTmp, nCount, nStartIdx,
                                         dfMin, dfMax, desc.bSlice,
                                         bFoundMinIdx, nMinIdx,
                                         bFoundMaxIdx, nMaxIdx,
                                         bLastWasReversed,
                                         bEmpty,
                                         EPS);
                }
                else
                {
                    FindMinMaxIdxString(var.get(),
                                        ppszTmp, nCount, nStartIdx,
                                        osMin, osMax, desc.bSlice,
                                        bFoundMinIdx, nMinIdx,
                                        bFoundMaxIdx, nMaxIdx,
                                        bLastWasReversed,
                                        bEmpty);
                }
                if( dt.NeedsFreeDynamicMemory() )
                {
                    for( size_t i = 0; i < nCount; i++ )
                    {
                        dt.FreeDynamicMemory(&abyTmp[i * nDTSize]);
                    }
                }
                if( bEmpty ||
                    (bFoundMinIdx && bFoundMaxIdx) || nCount < nMaxChunkSize )
                {
                    break;
                }
                nStartIdx += nMaxChunkSize;
            }

            // cppcheck-suppress knownConditionTrueFalse
            if( !bLastWasReversed )
            {
                if( !bFoundMinIdx )
                    bEmpty = true;
                else if( !bFoundMaxIdx )
                    nMaxIdx = poDim->GetSize() - 1;
                else
                    bEmpty = nMaxIdx < nMinIdx;
            }
            else
            {
                if( !bFoundMaxIdx )
                    bEmpty = true;
                else if( !bFoundMinIdx )
                    nMinIdx = poDim->GetSize() - 1;
                else
                    bEmpty = nMinIdx < nMaxIdx;
            }
            if( bEmpty )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Subset specification results in an empty set");
                return nullptr;
            }

            // cppcheck-suppress knownConditionTrueFalse
            if( !bLastWasReversed )
            {
                CPLAssert(nMaxIdx >= nMinIdx);
                desc.nStartIdx = nMinIdx;
                desc.nSize = nMaxIdx - nMinIdx + 1;
            }
            else
            {
                CPLAssert(nMaxIdx <= nMinIdx);
                desc.nStartIdx = nMaxIdx;
                desc.nSize = nMinIdx - nMaxIdx + 1;
            }

            break;
        }
    }

    for( const auto& scaleFactor: psOptions->aosScaleFactor )
    {
        if( STARTS_WITH( scaleFactor.c_str(), osRadix.c_str() ) )
        {
            if( scaleFactor.back() != ')' )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing ')' in scalefactor specification.");
                return nullptr;
            }
            std::string osScaleFactor(
                scaleFactor.substr(osRadix.size(),
                                   scaleFactor.size() - 1 - osRadix.size()));
            int nScaleFactor = atoi(osScaleFactor.c_str());
            if( CPLGetValueType(osScaleFactor.c_str()) != CPL_VALUE_INTEGER ||
                nScaleFactor <= 0 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Only positive integer scale factor is supported");
                return nullptr;
            }
            desc.nSize /= nScaleFactor;
            if( desc.nSize == 0 )
                desc.nSize = 1;
            desc.nStep *= nScaleFactor;
            break;
        }
    }

    oDimRemapper.oMap[osKey] = desc;
    return &oDimRemapper.oMap[osKey];
}

/************************************************************************/
/*                           ParseArraySpec()                           */
/************************************************************************/

// foo
// name=foo,transpose=[1,0],view=[0],dstname=bar,ot=Float32
static bool ParseArraySpec(const std::string& arraySpec,
                           std::string& srcName,
                           std::string& dstName,
                           int& band,
                           std::vector<int>& anTransposedAxis,
                           std::string& viewExpr,
                           GDALExtendedDataType& outputType)
{
    if( !STARTS_WITH(arraySpec.c_str(), "name=") &&
        !STARTS_WITH(arraySpec.c_str(), "band=") )
    {
        srcName = arraySpec;
        dstName = arraySpec;
        auto pos = dstName.rfind('/');
        if( pos != std::string::npos )
            dstName = dstName.substr(pos+1);
        return true;
    }

    std::vector<std::string> tokens;
    std::string curToken;
    bool bInArray = false;
    for( size_t i = 0; i < arraySpec.size(); ++i )
    {
        if( !bInArray && arraySpec[i] == ',' )
        {
            tokens.emplace_back(std::move(curToken));
            curToken = std::string();
        }
        else
        {
            if( arraySpec[i] == '[' )
            {
                bInArray = true;
            }
            else if( arraySpec[i] == ']' )
            {
                bInArray = false;
            }
            curToken += arraySpec[i];
        }
    }
    if( !curToken.empty() )
    {
        tokens.emplace_back(std::move(curToken));
    }
    for( const auto& token: tokens )
    {
        if( STARTS_WITH(token.c_str(), "name=") )
        {
            srcName = token.substr(strlen("name="));
            if( dstName.empty() )
                dstName = srcName;
        }
        else if( STARTS_WITH(token.c_str(), "band=") )
        {
            band = atoi(token.substr(strlen("band=")).c_str());
            if( dstName.empty() )
                dstName = CPLSPrintf("Band%d", band);
        }
        else if( STARTS_WITH(token.c_str(), "dstname=") )
        {
            dstName = token.substr(strlen("dstname="));
        }
        else if( STARTS_WITH(token.c_str(), "transpose=") )
        {
            auto transposeExpr = token.substr(strlen("transpose="));
            if( transposeExpr.size() < 3 ||
                transposeExpr[0] != '[' || transposeExpr.back() != ']' )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid value for transpose");
                return false;
            }
            transposeExpr = transposeExpr.substr(1, transposeExpr.size()-2);
            CPLStringList aosAxis(
                CSLTokenizeString2(transposeExpr.c_str(), ",", 0));
            for( int i = 0; i < aosAxis.size(); ++i )
            {
                anTransposedAxis.push_back(atoi(aosAxis[i]));
            }
        }
        else if( STARTS_WITH(token.c_str(), "view=") )
        {
            viewExpr = token.substr(strlen("view="));
        }
        else if( STARTS_WITH(token.c_str(), "ot=") )
        {
            auto outputTypeStr = token.substr(strlen("ot="));
            if( outputTypeStr == "String" )
                outputType = GDALExtendedDataType::CreateString();
            else
            {
                auto eDT = GDALGetDataTypeByName(outputTypeStr.c_str());
                if( eDT == GDT_Unknown )
                    return false;
                outputType = GDALExtendedDataType::Create(eDT);
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected array specification part: %s", token.c_str());
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                           TranslateArray()                           */
/************************************************************************/

static bool TranslateArray(DimensionRemapper& oDimRemapper,
                           const std::string& arraySpec,
                           const std::shared_ptr<GDALGroup>& poSrcRootGroup,
                           const std::shared_ptr<GDALGroup>& poSrcGroup,
                           const std::shared_ptr<GDALGroup>& poDstRootGroup,
                           std::shared_ptr<GDALGroup>& poDstGroup,
                           GDALDataset* poSrcDS,
                           std::map<std::string, std::shared_ptr<GDALDimension>>& mapSrcToDstDims,
                           std::map<std::string, std::shared_ptr<GDALDimension>>& mapDstDimFullNames,
                           const GDALMultiDimTranslateOptions *psOptions)
{
    std::string srcArrayName;
    std::string dstArrayName;
    int band = -1;
    std::vector<int> anTransposedAxis;
    std::string viewExpr;
    GDALExtendedDataType outputType(GDALExtendedDataType::Create(GDT_Unknown));
    if( !ParseArraySpec(arraySpec,
                        srcArrayName,
                        dstArrayName,
                        band,
                        anTransposedAxis,
                        viewExpr,
                        outputType) )
    {
        return false;
    }

    std::shared_ptr<GDALMDArray> srcArray;
    if( poSrcRootGroup && poSrcGroup )
    {
        if( !srcArrayName.empty() && srcArrayName[0] == '/' )
            srcArray = poSrcRootGroup->OpenMDArrayFromFullname(srcArrayName);
        else
            srcArray = poSrcGroup->OpenMDArray(srcArrayName);
        if( !srcArray )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot find array %s",
                        srcArrayName.c_str());
            return false;
        }
    }
    else
    {
        auto poBand = poSrcDS->GetRasterBand(band);
        if( !poBand )
            return false;
        srcArray = poBand->AsMDArray();
    }

    auto tmpArray = srcArray;
    if( !anTransposedAxis.empty() )
    {
        tmpArray = tmpArray->Transpose(anTransposedAxis);
        if( !tmpArray )
            return false;
    }
    const auto& srcArrayDims(tmpArray->GetDimensions());
    std::map<std::shared_ptr<GDALDimension>, std::shared_ptr<GDALDimension>> oMapSubsetDimToSrcDim;

    std::vector<GDALMDArray::ViewSpec> viewSpecs;
    if( !viewExpr.empty() )
    {
        if( !psOptions->aosSubset.empty() ||
            !psOptions->aosScaleFactor.empty() )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "View specification not supported when used together "
                     "with subset and/or scalefactor options");
            return false;
        }
        tmpArray = tmpArray->GetView(viewExpr, true, viewSpecs);
        if( !tmpArray )
            return false;
    }
    else if( !psOptions->aosSubset.empty() ||
             !psOptions->aosScaleFactor.empty() )
    {
        bool bHasModifiedDim = false;
        viewExpr = '[';
        for( size_t i = 0; i < srcArrayDims.size(); ++i )
        {
            const auto& srcDim(srcArrayDims[i]);
            const auto poDimDesc = GetDimensionDesc(oDimRemapper, psOptions, srcDim);
            if( poDimDesc == nullptr )
                return false;
            if( i > 0 )
                viewExpr += ',';
            if( !poDimDesc->bSlice &&
                poDimDesc->nStartIdx == 0 &&
                poDimDesc->nStep == 1 &&
                poDimDesc->nSize == srcDim->GetSize() )
            {
                viewExpr += ":";
            }
            else
            {
                bHasModifiedDim = true;
                viewExpr += CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUInt64>(
                    poDimDesc->nStartIdx));
                if( !poDimDesc->bSlice )
                {
                    viewExpr += ':';
                    viewExpr += CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUInt64>(
                        poDimDesc->nStartIdx + poDimDesc->nSize * poDimDesc->nStep));
                    viewExpr += ':';
                    viewExpr += CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUInt64>(
                        poDimDesc->nStep));
                }
            }
        }
        viewExpr += ']';
        if( bHasModifiedDim )
        {
            auto tmpArrayNew = tmpArray->GetView(viewExpr, false, viewSpecs);
            if( !tmpArrayNew )
                return false;
            tmpArray = tmpArrayNew;
            size_t j = 0;
            const auto& tmpArrayDims(tmpArray->GetDimensions());
            for( size_t i = 0; i < srcArrayDims.size(); ++i )
            {
                const auto& srcDim(srcArrayDims[i]);
                const auto poDimDesc = GetDimensionDesc(oDimRemapper, psOptions, srcDim);
                if( poDimDesc == nullptr )
                    return false;
                if( poDimDesc->bSlice )
                    continue;
                CPLAssert( j < tmpArrayDims.size() );
                oMapSubsetDimToSrcDim[tmpArrayDims[j]] = srcDim;
                j++;
            }
        }
        else
        {
            viewExpr.clear();
        }
    }

    int idxSliceSpec = -1;
    for( size_t i = 0; i < viewSpecs.size(); ++i )
    {
        if( viewSpecs[i].m_osFieldName.empty() )
        {
            if( idxSliceSpec >= 0 )
            {
                idxSliceSpec = -1;
                break;
            }
            else
            {
                idxSliceSpec = static_cast<int>(i);
            }
        }
    }

    // Map source dimensions to target dimensions
    std::vector<std::shared_ptr<GDALDimension>> dstArrayDims;
    const auto& tmpArrayDims(tmpArray->GetDimensions());
    for( size_t i = 0; i < tmpArrayDims.size(); ++i )
    {
        const auto& srcDim(tmpArrayDims[i]);
        std::string srcDimFullName(srcDim->GetFullName());

        std::shared_ptr<GDALDimension> dstDim;
        {
            CPLErrorHandlerPusher oHandlerPusher(CPLQuietErrorHandler);
            CPLErrorStateBackuper oErrorStateBackuper;
            if( !srcDimFullName.empty() && srcDimFullName[0] == '/' )
            {
                dstDim = poDstRootGroup->OpenDimensionFromFullname(srcDimFullName);
            }
        }
        if( dstDim )
        {
            dstArrayDims.emplace_back(dstDim);
            continue;
        }

        auto oIter = mapSrcToDstDims.find(srcDimFullName);
        if( oIter != mapSrcToDstDims.end() )
        {
            dstArrayDims.emplace_back(oIter->second);
            continue;
        }
        auto oIterRealSrcDim = oMapSubsetDimToSrcDim.find(srcDim);
        if( oIterRealSrcDim != oMapSubsetDimToSrcDim.end() )
        {
            srcDimFullName = oIterRealSrcDim->second->GetFullName();
            oIter = mapSrcToDstDims.find(srcDimFullName);
            if( oIter != mapSrcToDstDims.end() )
            {
                dstArrayDims.emplace_back(oIter->second);
                continue;
            }
        }

        auto srcDimForGetDimensionDesc(srcDim);
        if( idxSliceSpec >= 0 )
        {
            const auto& viewSpec(viewSpecs[idxSliceSpec]);
            auto iParentDim = viewSpec.m_mapDimIdxToParentDimIdx[i];
            if( iParentDim != static_cast<size_t>(-1) )
                srcDimForGetDimensionDesc = srcArrayDims[iParentDim];
        }

        const auto poDimDesc = GetDimensionDesc(
            oDimRemapper, psOptions, srcDimForGetDimensionDesc);
        if( poDimDesc == nullptr )
            return false;

        std::string newDimNameFullName(srcDimFullName);
        std::string newDimName(srcDim->GetName());
        int nIncr = 2;
        std::string osDstGroupFullName(poDstGroup->GetFullName());
        if( osDstGroupFullName == "/" )
            osDstGroupFullName.clear();
        auto oIter2 = mapDstDimFullNames.find(osDstGroupFullName + '/' + srcDim->GetName());
        while( oIter2 != mapDstDimFullNames.end() &&
               oIter2->second->GetSize() != poDimDesc->nSize )
        {
            newDimName = srcDim->GetName() + CPLSPrintf("_%d", nIncr);
            newDimNameFullName =
                osDstGroupFullName + '/' + srcDim->GetName() + CPLSPrintf("_%d", nIncr);
            nIncr++;
            oIter2 = mapDstDimFullNames.find(newDimNameFullName);
        }
        if ( oIter2 != mapDstDimFullNames.end() &&
             oIter2->second->GetSize() == poDimDesc->nSize )
        {
            dstArrayDims.emplace_back(oIter2->second);
            continue;
        }

        dstDim = poDstGroup->CreateDimension(newDimName,
                                        srcDim->GetType(),
                                        srcDim->GetDirection(),
                                        poDimDesc->nSize);
        if( !dstDim )
            return false;
        if( !srcDimFullName.empty() && srcDimFullName[0] == '/' )
        {
            mapSrcToDstDims[srcDimFullName] = dstDim;
        }
        mapDstDimFullNames[dstDim->GetFullName()] = dstDim;
        dstArrayDims.emplace_back(dstDim);

        std::shared_ptr<GDALMDArray> srcIndexVar;
        GDALMDArray::Range range;
        range.m_nStartIdx = 0;
        range.m_nIncr = 1;
        std::string indexingVarSpec;
        if( idxSliceSpec >= 0 )
        {
            const auto& viewSpec(viewSpecs[idxSliceSpec]);
            auto iParentDim = viewSpec.m_mapDimIdxToParentDimIdx[i];
            if( iParentDim != static_cast<size_t>(-1) &&
                (srcIndexVar = srcArrayDims[iParentDim]->
                                GetIndexingVariable()) != nullptr &&
                srcIndexVar->GetDimensionCount() == 1 &&
                srcIndexVar->GetFullName() != srcArray->GetFullName() )
            {
                CPLAssert(iParentDim < viewSpec.m_parentRanges.size());
                range = viewSpec.m_parentRanges[iParentDim];
                indexingVarSpec = "name=" + srcIndexVar->GetFullName();
                indexingVarSpec += ",dstname=" + newDimName;
                if( psOptions->aosSubset.empty() &&
                    psOptions->aosScaleFactor.empty() )
                {
                    if( range.m_nStartIdx != 0 ||
                        range.m_nIncr != 1 ||
                        srcArrayDims[iParentDim]->GetSize() != srcDim->GetSize() )
                    {
                        indexingVarSpec +=",view=[";
                        if( range.m_nIncr > 0 || range.m_nStartIdx != srcDim->GetSize() - 1 )
                        {
                            indexingVarSpec += CPLSPrintf(CPL_FRMT_GUIB, range.m_nStartIdx);
                        }
                        indexingVarSpec += ':';
                        if( range.m_nIncr > 0 )
                        {
                            const auto nEndIdx = range.m_nStartIdx + range.m_nIncr * srcDim->GetSize();
                            indexingVarSpec += CPLSPrintf(CPL_FRMT_GUIB, nEndIdx);
                        }
                        else if( range.m_nStartIdx > -range.m_nIncr * srcDim->GetSize() )
                        {
                            const auto nEndIdx = range.m_nStartIdx + range.m_nIncr * srcDim->GetSize();
                            indexingVarSpec += CPLSPrintf(CPL_FRMT_GUIB, nEndIdx-1);
                        }
                        indexingVarSpec += ':';
                        indexingVarSpec += CPLSPrintf(CPL_FRMT_GIB, range.m_nIncr);
                        indexingVarSpec += ']';
                    }
                }
            }
        }
        else
        {
            srcIndexVar = srcDim->GetIndexingVariable();
            if( srcIndexVar )
            {
                indexingVarSpec = srcIndexVar->GetFullName();
            }
        }
        if( srcIndexVar &&
            srcIndexVar->GetFullName() != srcArray->GetFullName() )
        {
            if( poSrcRootGroup )
            {
                if( !TranslateArray(oDimRemapper,
                                indexingVarSpec,
                                poSrcRootGroup,
                                poSrcGroup,
                                poDstRootGroup,
                                poDstGroup, poSrcDS,
                                mapSrcToDstDims,
                                mapDstDimFullNames,
                                psOptions) )
                {
                    return false;
                }
            }
            else
            {
                double adfGT[6];
                if( poSrcDS->GetGeoTransform(adfGT) == CE_None &&
                    adfGT[2] == 0.0 && adfGT[4] == 0.0 )
                {
                    auto var = std::dynamic_pointer_cast<VRTMDArray>(
                        poDstGroup->CreateMDArray(
                            newDimName, { dstDim },
                            GDALExtendedDataType::Create(GDT_Float64) ));
                    if( var )
                    {
                        const double dfStart = srcIndexVar->GetName() == "X" ?
                            adfGT[0] + (range.m_nStartIdx + 0.5) * adfGT[1]:
                            adfGT[3] + (range.m_nStartIdx + 0.5) * adfGT[5];
                        const double dfIncr = (srcIndexVar->GetName() == "X" ?
                            adfGT[1] : adfGT[5]) * range.m_nIncr;
                        std::unique_ptr<VRTMDArraySourceRegularlySpaced> poSource(
                            new VRTMDArraySourceRegularlySpaced(dfStart, dfIncr));
                        var->AddSource(std::move(poSource));
                    }
                }
            }

            CPLErrorHandlerPusher oHandlerPusher(CPLQuietErrorHandler);
            CPLErrorStateBackuper oErrorStateBackuper;
            auto poDstIndexingVar(poDstGroup->OpenMDArray(newDimName));
            if( poDstIndexingVar )
                dstDim->SetIndexingVariable(poDstIndexingVar);
        }

    }
    if( outputType.GetClass() == GEDTC_NUMERIC &&
        outputType.GetNumericDataType() == GDT_Unknown )
    {
        outputType = GDALExtendedDataType(tmpArray->GetDataType());
    }
    auto dstArray = poDstGroup->CreateMDArray( dstArrayName,
                                                    dstArrayDims,
                                                    outputType );
    auto dstArrayVRT = std::dynamic_pointer_cast<VRTMDArray>(dstArray);
    if( !dstArrayVRT )
        return false;

    GUInt64 nCurCost = 0;
    dstArray->CopyFromAllExceptValues(srcArray.get(),
                                        false,
                                        nCurCost, 0,
                                        nullptr, nullptr);

    if( idxSliceSpec >= 0 )
    {
        std::set<size_t> oSetParentDimIdxNotInArray;
        for( size_t i = 0; i < srcArrayDims.size(); ++i )
        {
            oSetParentDimIdxNotInArray.insert(i);
        }
        const auto& viewSpec(viewSpecs[idxSliceSpec]);
        for( size_t i = 0; i < tmpArrayDims.size(); ++i )
        {
            auto iParentDim = viewSpec.m_mapDimIdxToParentDimIdx[i];
            if( iParentDim != static_cast<size_t>(-1) )
            {
                oSetParentDimIdxNotInArray.erase(iParentDim);
            }
        }
        for( const auto parentDimIdx: oSetParentDimIdxNotInArray )
        {
            const auto& srcDim(srcArrayDims[parentDimIdx]);
            const auto nStartIdx = viewSpec.m_parentRanges[parentDimIdx].m_nStartIdx;
            if( nStartIdx < static_cast<GUInt64>(INT_MAX) )
            {
                auto dstAttr = dstArray->CreateAttribute(
                    "DIM_" + srcDim->GetName() + "_INDEX",
                    {},
                    GDALExtendedDataType::Create(GDT_Int32));
                dstAttr->Write(static_cast<int>(nStartIdx));
            }
            else
            {
                auto dstAttr = dstArray->CreateAttribute(
                    "DIM_" + srcDim->GetName() + "_INDEX",
                    {},
                    GDALExtendedDataType::CreateString());
                dstAttr->Write(CPLSPrintf(CPL_FRMT_GUIB,
                                      static_cast<GUIntBig>(nStartIdx)));
            }

            auto srcIndexVar(srcDim->GetIndexingVariable());
            if( srcIndexVar && srcIndexVar->GetDimensionCount() == 1 )
            {
                const auto& dt(srcIndexVar->GetDataType());
                std::vector<GByte> abyTmp(dt.GetSize());
                const size_t nCount = 1;
                if( srcIndexVar->Read(&nStartIdx, &nCount, nullptr, nullptr,
                                      dt, &abyTmp[0], nullptr, 0) )
                {
                    {
                        auto dstAttr = dstArray->CreateAttribute(
                            "DIM_" + srcDim->GetName() + "_VALUE",
                            {}, dt);
                        dstAttr->Write(abyTmp.data(), abyTmp.size());
                        dt.FreeDynamicMemory(&abyTmp[0]);
                    }

                    const auto unit(srcIndexVar->GetUnit());
                    if( !unit.empty() )
                    {
                        auto dstAttr = dstArray->CreateAttribute(
                            "DIM_" + srcDim->GetName() + "_UNIT",
                            {}, GDALExtendedDataType::CreateString());
                        dstAttr->Write(unit.c_str());
                    }
                }
            }
        }
    }

    const auto dimCount(tmpArray->GetDimensionCount());
    std::vector<GUInt64> anSrcOffset(dimCount);
    std::vector<GUInt64> anCount(dimCount);
    for( size_t i = 0; i < dimCount; ++i )
    {
        anCount[i] = tmpArrayDims[i]->GetSize();
    }
    std::vector<GUInt64> anStep(dimCount, 1);
    std::vector<GUInt64> anDstOffset(dimCount);
    std::unique_ptr<VRTMDArraySourceFromArray> poSource(new
        VRTMDArraySourceFromArray(
            dstArrayVRT.get(),
            false,
            false,
            poSrcDS->GetDescription(),
            band < 0 ? srcArray->GetFullName() : std::string(),
            band >= 1 ? CPLSPrintf("%d", band) : std::string(),
            std::move(anTransposedAxis),
            viewExpr,
            std::move(anSrcOffset),
            std::move(anCount),
            std::move(anStep),
            std::move(anDstOffset)));
    dstArrayVRT->AddSource(std::move(poSource));

    return true;
}

/************************************************************************/
/*                               GetGroup()                             */
/************************************************************************/

static std::shared_ptr<GDALGroup> GetGroup(const std::shared_ptr<GDALGroup>& poRootGroup,
                                           const std::string& fullName)
{
    auto poCurGroup = poRootGroup;
    CPLStringList aosTokens(CSLTokenizeString2(
        fullName.c_str(), "/", 0));
    for( int i = 0; i < aosTokens.size(); i++ )
    {
        auto poCurGroupNew = poCurGroup->OpenGroup(aosTokens[i], nullptr);
        if( !poCurGroupNew )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find group %s", aosTokens[i]);
            return nullptr;
        }
        poCurGroup = poCurGroupNew;
    }
    return poCurGroup;
}

/************************************************************************/
/*                                CopyGroup()                           */
/************************************************************************/

static bool CopyGroup(DimensionRemapper& oDimRemapper,
                      const std::shared_ptr<GDALGroup>& poDstRootGroup,
                      std::shared_ptr<GDALGroup>& poDstGroup,
                      const std::shared_ptr<GDALGroup>& poSrcRootGroup,
                      const std::shared_ptr<GDALGroup>& poSrcGroup,
                      GDALDataset* poSrcDS,
                      std::map<std::string, std::shared_ptr<GDALDimension>>& mapSrcToDstDims,
                      std::map<std::string, std::shared_ptr<GDALDimension>>& mapDstDimFullNames,
                      const GDALMultiDimTranslateOptions *psOptions,
                      bool bRecursive)
{
    const auto srcDims = poSrcGroup->GetDimensions();
    std::map<std::string, std::string> mapSrcVariableNameToIndexedDimName;
    for( const auto& dim: srcDims )
    {
        const auto poDimDesc = GetDimensionDesc(oDimRemapper, psOptions, dim);
        if( poDimDesc == nullptr )
            return false;
        if( poDimDesc->bSlice )
            continue;
        auto dstDim = poDstGroup->CreateDimension(dim->GetName(),
                                                    dim->GetType(),
                                                    dim->GetDirection(),
                                                    poDimDesc->nSize);
        if( !dstDim )
            return false;
        mapSrcToDstDims[dim->GetFullName()] = dstDim;
        mapDstDimFullNames[dstDim->GetFullName()] = dstDim;
        auto poIndexingVarSrc(dim->GetIndexingVariable());
        if( poIndexingVarSrc )
        {
            mapSrcVariableNameToIndexedDimName[poIndexingVarSrc->GetName()] = dim->GetFullName();
        }
    }

    auto attrs = poSrcGroup->GetAttributes();
    for( const auto& attr: attrs )
    {
        auto dstAttr = poDstGroup->CreateAttribute(attr->GetName(),
                                    attr->GetDimensionsSize(),
                                    attr->GetDataType());
        if( !dstAttr )
        {
            if( !psOptions->bStrict )
                continue;
            return false;
        }
        auto raw(attr->ReadAsRaw());
        if( !dstAttr->Write(raw.data(), raw.size()) && !psOptions->bStrict )
            return false;
    }

    auto arrayNames = poSrcGroup->GetMDArrayNames();
    for( const auto& name: arrayNames )
    {
        if( !TranslateArray(oDimRemapper,
                            name,
                            poSrcRootGroup, poSrcGroup,
                            poDstRootGroup, poDstGroup,
                            poSrcDS,
                            mapSrcToDstDims,
                            mapDstDimFullNames,
                            psOptions) )
        {
            return false;
        }

        // If this array is the indexing variable of a dimension, link them
        // together.
        auto srcArray = poSrcGroup->OpenMDArray(name);
        CPLAssert(srcArray);
        auto dstArray = poDstGroup->OpenMDArray(name);
        CPLAssert(dstArray);
        auto oIterDimName = mapSrcVariableNameToIndexedDimName.find(srcArray->GetName());
        if( oIterDimName != mapSrcVariableNameToIndexedDimName.end() )
        {
            auto oCorrespondingDimIter = mapSrcToDstDims.find(oIterDimName->second);
            if( oCorrespondingDimIter != mapSrcToDstDims.end() )
            {
                CPLErrorHandlerPusher oHandlerPusher(CPLQuietErrorHandler);
                CPLErrorStateBackuper oErrorStateBackuper;
                oCorrespondingDimIter->second->SetIndexingVariable(dstArray);
            }
        }
    }

    if( bRecursive )
    {
        auto groupNames = poSrcGroup->GetGroupNames();
        for( const auto& name: groupNames )
        {
            auto srcSubGroup = poSrcGroup->OpenGroup(name);
            if( !srcSubGroup )
            {
                return false;
            }
            auto dstSubGroup = poDstGroup->CreateGroup(name);
            if( !dstSubGroup )
            {
                return false;
            }
            if( !CopyGroup(oDimRemapper,
                           poDstRootGroup, dstSubGroup,
                           poSrcRootGroup, srcSubGroup, poSrcDS,
                           mapSrcToDstDims,
                           mapDstDimFullNames,
                           psOptions, true) )
            {
                return false;
            }
        }
    }
    return true;
}

/************************************************************************/
/*                           ParseGroupSpec()                           */
/************************************************************************/

// foo
// name=foo,dstname=bar,recursive=no
static bool ParseGroupSpec(const std::string& groupSpec,
                           std::string& srcName,
                           std::string& dstName,
                           bool& bRecursive)
{
    bRecursive = true;
    if( !STARTS_WITH(groupSpec.c_str(), "name=") )
    {
        srcName = groupSpec;
        return true;
    }

    CPLStringList aosTokens(CSLTokenizeString2(groupSpec.c_str(), ",", 0));
    for( int i = 0; i < aosTokens.size(); i++ )
    {
        const std::string token(aosTokens[i]);
        if( STARTS_WITH(token.c_str(), "name=") )
        {
            srcName = token.substr(strlen("name="));
        }
        else if( STARTS_WITH(token.c_str(), "dstname=") )
        {
            dstName = token.substr(strlen("dstname="));
        }
        else if( token == "recursive=no" )
        {
            bRecursive = false;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected group specification part: %s", token.c_str());
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                           TranslateInternal()                        */
/************************************************************************/

static bool TranslateInternal(std::shared_ptr<GDALGroup>& poDstRootGroup,
                              GDALDataset* poSrcDS,
                              const GDALMultiDimTranslateOptions *psOptions)
{

    auto poSrcRootGroup = poSrcDS->GetRootGroup();
    if( poSrcRootGroup )
    {
        if( psOptions->aosGroup.empty() )
        {
            auto attrs = poSrcRootGroup->GetAttributes();
            for( const auto& attr: attrs )
            {
                if( attr->GetName() == "Conventions" )
                    continue;
                auto dstAttr = poDstRootGroup->CreateAttribute(attr->GetName(),
                                            attr->GetDimensionsSize(),
                                            attr->GetDataType());
                if( dstAttr )
                {
                    auto raw(attr->ReadAsRaw());
                    dstAttr->Write(raw.data(), raw.size());
                }
            }
        }
    }

    DimensionRemapper oDimRemapper;
    std::map<std::string, std::shared_ptr<GDALDimension>> mapSrcToDstDims;
    std::map<std::string, std::shared_ptr<GDALDimension>> mapDstDimFullNames;
    if( !psOptions->aosGroup.empty() )
    {
        if( poSrcRootGroup == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "No multidimensional source dataset: -group cannot be used");
            return false;
        }
        if( psOptions->aosGroup.size() == 1 )
        {
            std::string srcName;
            std::string dstName;
            bool bRecursive;
            if( !ParseGroupSpec(psOptions->aosGroup[0], srcName, dstName,
                                bRecursive) )
                return false;
            auto poSrcGroup = GetGroup(poSrcRootGroup, srcName);
            if( !poSrcGroup )
                return false;
            return CopyGroup(oDimRemapper,
                             poDstRootGroup,
                             poDstRootGroup,
                             poSrcRootGroup,
                             poSrcGroup,
                             poSrcDS,
                             mapSrcToDstDims,
                             mapDstDimFullNames,
                             psOptions,
                             bRecursive);
        }
        else
        {
            for( const auto& osGroupSpec: psOptions->aosGroup )
            {
                std::string srcName;
                std::string dstName;
                bool bRecursive;
                if( !ParseGroupSpec(osGroupSpec, srcName, dstName,
                                    bRecursive) )
                return false;
                auto poSrcGroup = GetGroup(poSrcRootGroup, srcName);
                if( !poSrcGroup )
                    return false;
                if( dstName.empty() )
                    dstName = poSrcGroup->GetName();
                auto dstSubGroup = poDstRootGroup->CreateGroup(dstName);
                if( !dstSubGroup ||
                    !CopyGroup(oDimRemapper,
                               poDstRootGroup, dstSubGroup,
                               poSrcRootGroup, poSrcGroup, poSrcDS,
                               mapSrcToDstDims,
                               mapDstDimFullNames,
                               psOptions, bRecursive) )
                {
                    return false;
                }
            }
        }
    }
    else if( !psOptions->aosArraySpec.empty() )
    {
        for( const auto& arraySpec: psOptions->aosArraySpec )
        {
            if( !TranslateArray(oDimRemapper,
                                arraySpec,
                                poSrcRootGroup,
                                poSrcRootGroup,
                                poDstRootGroup,
                                poDstRootGroup,
                                poSrcDS,
                                mapSrcToDstDims,
                                mapDstDimFullNames,
                                psOptions) )
            {
                return false;
            }
        }
    }
    else
    {
        if( poSrcRootGroup == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "No multidimensional source dataset");
            return false;
        }
        return CopyGroup(oDimRemapper,
                         poDstRootGroup,
                         poDstRootGroup,
                         poSrcRootGroup,
                         poSrcRootGroup,
                         poSrcDS,
                         mapSrcToDstDims,
                         mapDstDimFullNames,
                         psOptions,
                         true);
    }

    return true;
}


/************************************************************************/
/*                      CopyToNonMultiDimensionalDriver()               */
/************************************************************************/

static GDALDatasetH CopyToNonMultiDimensionalDriver(
    GDALDriver* poDriver, const char* pszDest,
    const std::shared_ptr<GDALGroup> &poRG,
    const GDALMultiDimTranslateOptions* psOptions)
{
    std::shared_ptr<GDALMDArray> srcArray;
    if( psOptions && !psOptions->aosArraySpec.empty() )
    {
        if( psOptions->aosArraySpec.size() != 1 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                        "For output to a non-multidimensional driver, only "
                        "one array should be specified");
            return nullptr;
        }
        std::string srcArrayName;
        std::string dstArrayName;
        int band = -1;
        std::vector<int> anTransposedAxis;
        std::string viewExpr;
        GDALExtendedDataType outputType(GDALExtendedDataType::Create(GDT_Unknown));
        ParseArraySpec(psOptions->aosArraySpec[0],
                            srcArrayName,
                            dstArrayName,
                            band,
                            anTransposedAxis,
                            viewExpr,
                            outputType);
        srcArray = poRG->OpenMDArray(dstArrayName);
    }
    else
    {
        auto srcArrayNames = poRG->GetMDArrayNames();
        for( const auto& srcArrayName: srcArrayNames )
        {
            auto tmpArray = poRG->OpenMDArray(srcArrayName);
            if( tmpArray )
            {
                const auto& dims(tmpArray->GetDimensions());
                if( !(dims.size() == 1 &&
                      dims[0]->GetIndexingVariable() &&
                      dims[0]->GetIndexingVariable()->GetName() == srcArrayName) )
                {
                    if( srcArray )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                    "Several arrays exist. Select one for "
                                    "output to non-multidimensional driver");
                        return nullptr;
                    }
                    srcArray = tmpArray;
                }
            }
        }
    }
    if( !srcArray )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find source array");
        return nullptr;
    }
    size_t iXDim = static_cast<size_t>(-1);
    size_t iYDim = static_cast<size_t>(-1);
    const auto& dims(srcArray->GetDimensions());
    for( size_t i = 0; i < dims.size(); ++i )
    {
        if( dims[i]->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X )
        {
            iXDim = i;
        }
        else if( dims[i]->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y )
        {
            iYDim = i;
        }
    }
    if( dims.size() == 1 )
    {
        iXDim = 0;
    }
    else if( dims.size() >= 2 &&
        (iXDim == static_cast<size_t>(-1) || iYDim == static_cast<size_t>(-1)) )
    {
        iXDim = dims.size() - 1;
        iYDim = dims.size() - 2;
    }
    std::unique_ptr<GDALDataset> poTmpSrcDS(srcArray->AsClassicDataset(iXDim, iYDim));
    if( !poTmpSrcDS )
        return nullptr;
    return GDALDataset::ToHandle(
        poDriver->CreateCopy(pszDest, poTmpSrcDS.get(), false,
            psOptions ? const_cast<char**>(psOptions->aosCreateOptions.List()) : nullptr,
            psOptions ? psOptions->pfnProgress : nullptr,
            psOptions ? psOptions->pProgressData : nullptr));
}

/************************************************************************/
/*                        GDALMultiDimTranslate()                       */
/************************************************************************/

/**
 * Converts raster data between different formats.
 *
 * This is the equivalent of the <a href="/programs/gdalmdimtranslate.html">gdalmdimtranslate</a> utility.
 *
 * GDALMultiDimTranslateOptions* must be allocated and freed with GDALMultiDimTranslateOptionsNew()
 * and GDALMultiDimTranslateOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL.
 * @param nSrcCount the number of input datasets.
 * @param pahSrcDS the list of input datasets.
 * @param psOptions the options struct returned by GDALMultiDimTranslateOptionsNew() or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 3.1
 */

GDALDatasetH GDALMultiDimTranslate( const char* pszDest,
                                    GDALDatasetH hDstDS,
                                    int nSrcCount, GDALDatasetH *pahSrcDS,
                                    const GDALMultiDimTranslateOptions *psOptions,
                                    int *pbUsageError )
{
    if( pbUsageError )
        *pbUsageError = false;
    if( nSrcCount != 1 || pahSrcDS[0] == nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only one source dataset is supported");
        if( pbUsageError )
            *pbUsageError = true;
        return nullptr;
    }

    if( hDstDS )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Update of existing file not supported yet");
        GDALClose(hDstDS);
        return nullptr;
    }

    CPLString osFormat(psOptions ? psOptions->osFormat : "");
    if( pszDest == nullptr /* && hDstDS == nullptr */ )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Both pszDest and hDstDS are NULL.");
        if( pbUsageError )
            *pbUsageError = true;
        return nullptr;
    }

    GDALDriver* poDriver = nullptr;

#ifdef this_is_dead_code_for_now
    const bool bCloseOutDSOnError = hDstDS == nullptr;
    if( pszDest == nullptr )
        pszDest = GDALGetDescription(hDstDS);
#endif

#ifdef this_is_dead_code_for_now
    if( hDstDS == nullptr )
#endif
    {
        if( osFormat.empty() )
        {
            if( EQUAL(CPLGetExtension(pszDest), "nc") )
                osFormat = "netCDF";
            else
                osFormat = GetOutputDriverForRaster(pszDest);
            if( osFormat.empty() )
            {
                return nullptr;
            }
        }
        poDriver = GDALDriver::FromHandle(GDALGetDriverByName( osFormat ));
        char** papszDriverMD = poDriver ? poDriver->GetMetadata(): nullptr;
        if( poDriver == nullptr
            || (!CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_RASTER, "FALSE")) &&
                !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_MULTIDIM_RASTER, "FALSE")))
            || (!CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE, "FALSE")) &&
                !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATECOPY, "FALSE")) &&
                !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE_MULTIDIMENSIONAL, "FALSE")) &&
                !CPLTestBool(CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATECOPY_MULTIDIMENSIONAL, "FALSE"))) )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Output driver `%s' not recognised or does not support "
                      "output file creation.", osFormat.c_str());
            return nullptr;
        }
    }

    GDALDataset* poSrcDS = GDALDataset::FromHandle(pahSrcDS[0]);

    std::unique_ptr<GDALDataset> poTmpDS;
    GDALDataset* poTmpSrcDS = poSrcDS;
    if( psOptions &&
        (!psOptions->aosArraySpec.empty() ||
         !psOptions->aosGroup.empty() ||
         !psOptions->aosSubset.empty() ||
         !psOptions->aosScaleFactor.empty()) )
    {
        auto poVRTDriver = GDALDriver::FromHandle(GDALGetDriverByName("VRT"));
        if( !poVRTDriver )
        {
#ifdef this_is_dead_code_for_now
            if( bCloseOutDSOnError )
#endif
            {
                GDALClose(hDstDS);
                hDstDS = nullptr;
            }
            return nullptr;
        }
        poTmpDS.reset(
            poVRTDriver->CreateMultiDimensional("", nullptr, nullptr));
        CPLAssert(poTmpDS);
        poTmpSrcDS = poTmpDS.get();

        auto poDstRootGroup = poTmpDS->GetRootGroup();
        CPLAssert(poDstRootGroup);

        if( !TranslateInternal(poDstRootGroup, poSrcDS, psOptions) )
        {
#ifdef this_is_dead_code_for_now
            if( bCloseOutDSOnError )
#endif
            {
                GDALClose(hDstDS);
                hDstDS = nullptr;
            }
            return nullptr;
        }
    }

    auto poRG(poTmpSrcDS->GetRootGroup());
    if( poRG &&
        poDriver->GetMetadataItem(GDAL_DCAP_CREATE_MULTIDIMENSIONAL) == nullptr &&
        poDriver->GetMetadataItem(GDAL_DCAP_CREATECOPY_MULTIDIMENSIONAL) == nullptr )
    {
#ifdef this_is_dead_code_for_now
        if( hDstDS )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Appending to non-multidimensional driver not supported.");
            GDALClose(hDstDS);
            hDstDS = nullptr;
            return nullptr;
        }
#endif
        hDstDS = CopyToNonMultiDimensionalDriver(
            poDriver, pszDest, poRG, psOptions);
    }
    else
    {
        hDstDS = GDALDataset::ToHandle(
            poDriver->CreateCopy(pszDest, poTmpSrcDS, false,
                    psOptions ? const_cast<char**>(psOptions->aosCreateOptions.List()) : nullptr,
                    psOptions ? psOptions->pfnProgress : nullptr,
                    psOptions ? psOptions->pProgressData : nullptr));
    }

    return hDstDS;
}

/************************************************************************/
/*                     GDALMultiDimTranslateOptionsNew()                */
/************************************************************************/

/**
 * Allocates a GDALMultiDimTranslateOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="/programs/gdalmdimtranslate.html">gdalmdimtranslate</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdalmultidimtranslate_bin.cpp use case) must be allocated with
 *                           GDALTranslateOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALMultiDimTranslateOptions struct. Must be freed with GDALMultiDimTranslateOptionsFree().
 *
 * @since GDAL 3.1
 */

GDALMultiDimTranslateOptions *GDALMultiDimTranslateOptionsNew(
    char** papszArgv, GDALMultiDimTranslateOptionsForBinary* psOptionsForBinary)
{
    GDALMultiDimTranslateOptions* psOptions = new GDALMultiDimTranslateOptions;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != nullptr && i < argc; i++ )
    {
        if( i < argc-1 && (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) )
        {
            ++i;
            psOptions->osFormat = papszArgv[i];
        }

        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = TRUE;
        }

        else if( EQUAL(papszArgv[i],"-strict")  )
        {
            psOptions->bStrict = true;
        }

        else if( i < argc-1 && EQUAL(papszArgv[i],"-array") )
        {
            ++i;
            psOptions->aosArraySpec.push_back(papszArgv[i]);
        }

        else if( i < argc-1 && EQUAL(papszArgv[i],"-group") )
        {
            ++i;
            psOptions->aosGroup.push_back(papszArgv[i]);
        }

        else if( i < argc-1 && EQUAL(papszArgv[i],"-subset") )
        {
            ++i;
            psOptions->aosSubset.push_back(papszArgv[i]);
        }

        else if( i < argc-1 && EQUAL(papszArgv[i],"-scaleaxes") )
        {
            ++i;
            CPLStringList aosScaleFactors(CSLTokenizeString2(papszArgv[i], ",", 0));
            for( int j = 0; j < aosScaleFactors.size(); j++ )
            {
                psOptions->aosScaleFactor.push_back(aosScaleFactors[j]);
            }
        }

        else if( i < argc-1 && EQUAL(papszArgv[i],"-co") )
        {
            ++i;
            psOptions->aosCreateOptions.AddString( papszArgv[i] );
        }

        else if( EQUAL(papszArgv[i], "-oo") && i+1 < argc )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->papszOpenOptions = CSLAddString(
                                                psOptionsForBinary->papszOpenOptions,
                                                papszArgv[++i] );
        }
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALMultiDimTranslateOptionsFree(psOptions);
            return nullptr;
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszSource == nullptr )
        {
            psOptionsForBinary->pszSource = CPLStrdup(papszArgv[i]);
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszDest == nullptr )
        {
            psOptionsForBinary->pszDest = CPLStrdup(papszArgv[i]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            GDALMultiDimTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }

    if( !psOptions->aosArraySpec.empty() && !psOptions->aosGroup.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-array and -group are mutually exclusive");
        GDALMultiDimTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->bUpdate = psOptions->bUpdate;
        if( !psOptions->osFormat.empty() )
            psOptionsForBinary->pszFormat = CPLStrdup(psOptions->osFormat.c_str());
    }

    return psOptions;
}

/************************************************************************/
/*                     GDALMultiDimTranslateOptionsFree()               */
/************************************************************************/

/**
 * Frees the GDALMultiDimTranslateOptions struct.
 *
 * @param psOptions the options struct for GDALMultiDimTranslate().
 *
 * @since GDAL 3.1
 */

void GDALMultiDimTranslateOptionsFree( GDALMultiDimTranslateOptions *psOptions )
{
    delete psOptions;
}

/************************************************************************/
/*               GDALMultiDimTranslateOptionsSetProgress()              */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALMultiDimTranslate().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 3.1
 */

void GDALMultiDimTranslateOptionsSetProgress( GDALMultiDimTranslateOptions *psOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
}
