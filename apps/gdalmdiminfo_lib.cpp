/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to list info about a multidimensional
 *raster Author:   Even Rouault,<even.rouault at spatialys.com>
 *
 * ****************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include "cpl_enumerate.h"
#include "cpl_json.h"
#include "cpl_json_streaming_writer.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "gdalargumentparser.h"

#include <algorithm>
#include <limits>
#include <set>

static void DumpArray(const std::shared_ptr<GDALGroup> &rootGroup,
                      const std::shared_ptr<GDALMDArray> &array,
                      CPLJSonStreamingWriter &serializer,
                      const GDALMultiDimInfoOptions *psOptions,
                      std::set<std::string> &alreadyDumpedDimensions,
                      std::set<std::string> &alreadyDumpedArrays,
                      bool bOutputObjType, bool bOutputName,
                      bool bOutputOverviews);

/************************************************************************/
/*                       GDALMultiDimInfoOptions                        */
/************************************************************************/

struct GDALMultiDimInfoOptions
{
    bool bStdoutOutput = false;
    bool bSummary = false;
    bool bDetailed = false;
    bool bPretty = true;
    size_t nLimitValuesByDim = 0;
    CPLStringList aosArrayOptions{};
    std::string osArrayName{};
    bool bStats = false;
    std::string osFormat = "json";
};

/************************************************************************/
/*                           HasUniqueNames()                           */
/************************************************************************/

static bool HasUniqueNames(const std::vector<std::string> &oNames)
{
    std::set<std::string> oSetNames;
    for (const auto &subgroupName : oNames)
    {
        if (oSetNames.find(subgroupName) != oSetNames.end())
        {
            return false;
        }
        oSetNames.insert(subgroupName);
    }
    return true;
}

/************************************************************************/
/*                            DumpDataType()                            */
/************************************************************************/

static void DumpDataType(const GDALExtendedDataType &dt,
                         CPLJSonStreamingWriter &serializer)
{
    switch (dt.GetClass())
    {
        case GEDTC_STRING:
            serializer.Add("String");
            break;

        case GEDTC_NUMERIC:
        {
            auto poRAT = dt.GetRAT();
            if (poRAT)
            {
                auto objContext(serializer.MakeObjectContext());
                serializer.AddObjKey("name");
                serializer.Add(dt.GetName());
                serializer.AddObjKey("type");
                serializer.Add(GDALGetDataTypeName(dt.GetNumericDataType()));
                serializer.AddObjKey("attribute_table");
                auto arrayContext(serializer.MakeArrayContext());
                const int nRows = poRAT->GetRowCount();
                const int nCols = poRAT->GetColumnCount();
                for (int iRow = 0; iRow < nRows; ++iRow)
                {
                    auto obj2Context(serializer.MakeObjectContext());
                    for (int iCol = 0; iCol < nCols; ++iCol)
                    {
                        serializer.AddObjKey(poRAT->GetNameOfCol(iCol));
                        switch (poRAT->GetTypeOfCol(iCol))
                        {
                            case GFT_Integer:
                                serializer.Add(
                                    poRAT->GetValueAsInt(iRow, iCol));
                                break;
                            case GFT_Real:
                                serializer.Add(
                                    poRAT->GetValueAsDouble(iRow, iCol));
                                break;
                            case GFT_String:
                                serializer.Add(
                                    poRAT->GetValueAsString(iRow, iCol));
                                break;
                            case GFT_Boolean:
                                serializer.Add(
                                    poRAT->GetValueAsBoolean(iRow, iCol));
                                break;
                            case GFT_DateTime:
                            {
                                const auto sDateTime =
                                    poRAT->GetValueAsDateTime(iRow, iCol);
                                serializer.Add(
                                    GDALRasterAttributeTable::DateTimeToString(
                                        sDateTime));
                                break;
                            }
                            case GFT_WKBGeometry:
                            {
                                size_t nWKBSize = 0;
                                const GByte *pabyWKB =
                                    poRAT->GetValueAsWKBGeometry(iRow, iCol,
                                                                 nWKBSize);
                                std::string osWKT =
                                    GDALRasterAttributeTable::WKBGeometryToWKT(
                                        pabyWKB, nWKBSize);
                                if (osWKT.empty())
                                    serializer.AddNull();
                                else
                                    serializer.Add(osWKT);
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                serializer.Add(GDALGetDataTypeName(dt.GetNumericDataType()));
            }
            break;
        }

        case GEDTC_COMPOUND:
        {
            auto compoundContext(serializer.MakeObjectContext());
            serializer.AddObjKey("name");
            serializer.Add(dt.GetName());
            serializer.AddObjKey("size");
            serializer.Add(static_cast<unsigned>(dt.GetSize()));
            serializer.AddObjKey("components");
            const auto &components = dt.GetComponents();
            auto componentsContext(serializer.MakeArrayContext());
            for (const auto &comp : components)
            {
                auto compContext(serializer.MakeObjectContext());
                serializer.AddObjKey("name");
                serializer.Add(comp->GetName());
                serializer.AddObjKey("offset");
                serializer.Add(static_cast<unsigned>(comp->GetOffset()));
                serializer.AddObjKey("type");
                DumpDataType(comp->GetType(), serializer);
            }
            break;
        }
    }
}

/************************************************************************/
/*                             DumpValue()                              */
/************************************************************************/

template <typename T>
static void DumpValue(CPLJSonStreamingWriter &serializer, const void *bytes)
{
    T tmp;
    memcpy(&tmp, bytes, sizeof(T));
    serializer.Add(tmp);
}

/************************************************************************/
/*                          DumpComplexValue()                          */
/************************************************************************/

template <typename T>
static void DumpComplexValue(CPLJSonStreamingWriter &serializer,
                             const GByte *bytes)
{
    auto objectContext(serializer.MakeObjectContext());
    serializer.AddObjKey("real");
    DumpValue<T>(serializer, bytes);
    serializer.AddObjKey("imag");
    DumpValue<T>(serializer, bytes + sizeof(T));
}

/************************************************************************/
/*                             DumpValue()                              */
/************************************************************************/

static void DumpValue(CPLJSonStreamingWriter &serializer, const GByte *bytes,
                      const GDALDataType &eDT)
{
    switch (eDT)
    {
        case GDT_UInt8:
            DumpValue<GByte>(serializer, bytes);
            break;
        case GDT_Int8:
            DumpValue<GInt8>(serializer, bytes);
            break;
        case GDT_Int16:
            DumpValue<GInt16>(serializer, bytes);
            break;
        case GDT_UInt16:
            DumpValue<GUInt16>(serializer, bytes);
            break;
        case GDT_Int32:
            DumpValue<GInt32>(serializer, bytes);
            break;
        case GDT_UInt32:
            DumpValue<GUInt32>(serializer, bytes);
            break;
        case GDT_Int64:
            DumpValue<std::int64_t>(serializer, bytes);
            break;
        case GDT_UInt64:
            DumpValue<std::uint64_t>(serializer, bytes);
            break;
        case GDT_Float16:
            DumpValue<GFloat16>(serializer, bytes);
            break;
        case GDT_Float32:
            DumpValue<float>(serializer, bytes);
            break;
        case GDT_Float64:
            DumpValue<double>(serializer, bytes);
            break;
        case GDT_CInt16:
            DumpComplexValue<GInt16>(serializer, bytes);
            break;
        case GDT_CInt32:
            DumpComplexValue<GInt32>(serializer, bytes);
            break;
        case GDT_CFloat16:
            DumpComplexValue<GFloat16>(serializer, bytes);
            break;
        case GDT_CFloat32:
            DumpComplexValue<float>(serializer, bytes);
            break;
        case GDT_CFloat64:
            DumpComplexValue<double>(serializer, bytes);
            break;
        case GDT_Unknown:
        case GDT_TypeCount:
            CPLAssert(false);
            break;
    }
}

static void DumpValue(CPLJSonStreamingWriter &serializer, const GByte *values,
                      const GDALExtendedDataType &dt);

/************************************************************************/
/*                            DumpCompound()                            */
/************************************************************************/

static void DumpCompound(CPLJSonStreamingWriter &serializer,
                         const GByte *values, const GDALExtendedDataType &dt)
{
    CPLAssert(dt.GetClass() == GEDTC_COMPOUND);
    const auto &components = dt.GetComponents();
    auto objectContext(serializer.MakeObjectContext());
    for (const auto &comp : components)
    {
        serializer.AddObjKey(comp->GetName());
        DumpValue(serializer, values + comp->GetOffset(), comp->GetType());
    }
}

/************************************************************************/
/*                             DumpValue()                              */
/************************************************************************/

static void DumpValue(CPLJSonStreamingWriter &serializer, const GByte *values,
                      const GDALExtendedDataType &dt)
{
    switch (dt.GetClass())
    {
        case GEDTC_NUMERIC:
            DumpValue(serializer, values, dt.GetNumericDataType());
            break;
        case GEDTC_COMPOUND:
            DumpCompound(serializer, values, dt);
            break;
        case GEDTC_STRING:
        {
            const char *pszStr;
            // cppcheck-suppress pointerSize
            memcpy(&pszStr, values, sizeof(const char *));
            if (pszStr)
                serializer.Add(pszStr);
            else
                serializer.AddNull();
            break;
        }
    }
}

/************************************************************************/
/*                           SerializeJSON()                            */
/************************************************************************/

static void SerializeJSON(const CPLJSONObject &obj,
                          CPLJSonStreamingWriter &serializer)
{
    switch (obj.GetType())
    {
        case CPLJSONObject::Type::Unknown:
        {
            CPLAssert(false);
            break;
        }

        case CPLJSONObject::Type::Null:
        {
            serializer.AddNull();
            break;
        }

        case CPLJSONObject::Type::Object:
        {
            auto objectContext(serializer.MakeObjectContext());
            for (const auto &subobj : obj.GetChildren())
            {
                serializer.AddObjKey(subobj.GetName());
                SerializeJSON(subobj, serializer);
            }
            break;
        }

        case CPLJSONObject::Type::Array:
        {
            auto arrayContext(serializer.MakeArrayContext());
            const CPLJSONArray array = obj.ToArray();
            for (const auto &subobj : array)
            {
                SerializeJSON(subobj, serializer);
            }
            break;
        }

        case CPLJSONObject::Type::Boolean:
        {
            serializer.Add(obj.ToBool());
            break;
        }

        case CPLJSONObject::Type::String:
        {
            serializer.Add(obj.ToString());
            break;
        }

        case CPLJSONObject::Type::Integer:
        {
            serializer.Add(obj.ToInteger());
            break;
        }

        case CPLJSONObject::Type::Long:
        {
            serializer.Add(static_cast<int64_t>(obj.ToLong()));
            break;
        }

        case CPLJSONObject::Type::Double:
        {
            serializer.Add(obj.ToDouble());
            break;
        }
    }
}

/************************************************************************/
/*                           DumpAttrValue()                            */
/************************************************************************/

static void DumpAttrValue(const std::shared_ptr<GDALAttribute> &attr,
                          CPLJSonStreamingWriter &serializer)
{
    const auto &dt = attr->GetDataType();
    const size_t nEltCount(static_cast<size_t>(attr->GetTotalElementsCount()));
    switch (dt.GetClass())
    {
        case GEDTC_STRING:
        {
            if (nEltCount == 1)
            {
                const char *pszStr = attr->ReadAsString();
                if (pszStr)
                {
                    if (dt.GetSubType() == GEDTST_JSON)
                    {
                        CPLJSONDocument oDoc;
                        if (oDoc.LoadMemory(std::string(pszStr)))
                        {
                            SerializeJSON(oDoc.GetRoot(), serializer);
                        }
                        else
                        {
                            serializer.Add(pszStr);
                        }
                    }
                    else
                    {
                        serializer.Add(pszStr);
                    }
                }
                else
                {
                    serializer.AddNull();
                }
            }
            else
            {
                CPLStringList aosValues(attr->ReadAsStringArray());
                {
                    auto arrayContextValues(
                        serializer.MakeArrayContext(nEltCount < 10));
                    for (int i = 0; i < aosValues.size(); ++i)
                    {
                        serializer.Add(aosValues[i]);
                    }
                }
            }
            break;
        }

        case GEDTC_NUMERIC:
        {
            auto eDT = dt.GetNumericDataType();
            const auto rawValues(attr->ReadAsRaw());
            const GByte *bytePtr = rawValues.data();
            if (bytePtr)
            {
                const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
                if (nEltCount == 1)
                {
                    serializer.SetNewline(false);
                    DumpValue(serializer, rawValues.data(), eDT);
                    serializer.SetNewline(true);
                }
                else
                {
                    auto arrayContextValues(
                        serializer.MakeArrayContext(nEltCount < 10));
                    for (size_t i = 0; i < nEltCount; i++)
                    {
                        DumpValue(serializer, bytePtr, eDT);
                        bytePtr += nDTSize;
                    }
                }
            }
            else
            {
                serializer.AddNull();
            }
            break;
        }

        case GEDTC_COMPOUND:
        {
            auto rawValues(attr->ReadAsRaw());
            const GByte *bytePtr = rawValues.data();
            if (bytePtr)
            {
                if (nEltCount == 1)
                {
                    serializer.SetNewline(false);
                    DumpCompound(serializer, bytePtr, dt);
                    serializer.SetNewline(true);
                }
                else
                {
                    auto arrayContextValues(serializer.MakeArrayContext());
                    for (size_t i = 0; i < nEltCount; i++)
                    {
                        DumpCompound(serializer, bytePtr, dt);
                        bytePtr += dt.GetSize();
                    }
                }
            }
            else
            {
                serializer.AddNull();
            }
            break;
        }
    }
}

/************************************************************************/
/*                              DumpAttr()                              */
/************************************************************************/

static void DumpAttr(std::shared_ptr<GDALAttribute> attr,
                     CPLJSonStreamingWriter &serializer,
                     const GDALMultiDimInfoOptions *psOptions,
                     bool bOutputObjType, bool bOutputName)
{
    if (!bOutputObjType && !bOutputName && !psOptions->bDetailed)
    {
        DumpAttrValue(attr, serializer);
        return;
    }

    const auto &dt = attr->GetDataType();
    auto objectContext(serializer.MakeObjectContext());
    if (bOutputObjType)
    {
        serializer.AddObjKey("type");
        serializer.Add("attribute");
    }
    if (bOutputName)
    {
        serializer.AddObjKey("name");
        serializer.Add(attr->GetName());
    }

    if (psOptions->bDetailed)
    {
        serializer.AddObjKey("datatype");
        DumpDataType(dt, serializer);

        switch (dt.GetSubType())
        {
            case GEDTST_NONE:
                break;
            case GEDTST_JSON:
            {
                serializer.AddObjKey("subtype");
                serializer.Add("JSON");
                break;
            }
        }

        serializer.AddObjKey("value");
    }

    DumpAttrValue(attr, serializer);
}

/************************************************************************/
/*                             DumpAttrs()                              */
/************************************************************************/

static void DumpAttrs(const std::vector<std::shared_ptr<GDALAttribute>> &attrs,
                      CPLJSonStreamingWriter &serializer,
                      const GDALMultiDimInfoOptions *psOptions)
{
    std::vector<std::string> attributeNames;
    for (const auto &poAttr : attrs)
        attributeNames.emplace_back(poAttr->GetName());
    if (HasUniqueNames(attributeNames))
    {
        auto objectContext(serializer.MakeObjectContext());
        for (const auto &poAttr : attrs)
        {
            serializer.AddObjKey(poAttr->GetName());
            DumpAttr(poAttr, serializer, psOptions, false, false);
        }
    }
    else
    {
        auto arrayContext(serializer.MakeArrayContext());
        for (const auto &poAttr : attrs)
        {
            DumpAttr(poAttr, serializer, psOptions, false, true);
        }
    }
}

/************************************************************************/
/*                            DumpArrayRec()                            */
/************************************************************************/

static void DumpArrayRec(std::shared_ptr<GDALMDArray> array,
                         CPLJSonStreamingWriter &serializer, size_t nCurDim,
                         const std::vector<GUInt64> &dimSizes,
                         std::vector<GUInt64> &startIdx,
                         const GDALMultiDimInfoOptions *psOptions)
{
    do
    {
        auto arrayContext(serializer.MakeArrayContext());
        if (nCurDim + 1 == dimSizes.size())
        {
            const auto &dt(array->GetDataType());
            const auto nDTSize(dt.GetSize());
            const auto lambdaDumpValue =
                [&serializer, &dt, nDTSize](std::vector<GByte> &abyTmp,
                                            size_t nCount)
            {
                GByte *pabyPtr = &abyTmp[0];
                for (size_t i = 0; i < nCount; ++i)
                {
                    DumpValue(serializer, pabyPtr, dt);
                    dt.FreeDynamicMemory(pabyPtr);
                    pabyPtr += nDTSize;
                }
            };

            serializer.SetNewline(false);
            std::vector<size_t> count(dimSizes.size(), 1);
            if (psOptions->nLimitValuesByDim == 0 ||
                dimSizes.back() <= psOptions->nLimitValuesByDim)
            {
                const size_t nCount = static_cast<size_t>(dimSizes.back());
                if (nCount > 0)
                {
                    if (nCount != dimSizes.back() ||
                        nDTSize > std::numeric_limits<size_t>::max() / nCount)
                    {
                        serializer.Add("[too many values]");
                        break;
                    }
                    std::vector<GByte> abyTmp(nDTSize * nCount);
                    count.back() = nCount;
                    if (!array->Read(startIdx.data(), count.data(), nullptr,
                                     nullptr, dt, &abyTmp[0]))
                        break;
                    lambdaDumpValue(abyTmp, count.back());
                }
            }
            else
            {
                std::vector<GByte> abyTmp(
                    nDTSize * (psOptions->nLimitValuesByDim + 1) / 2);
                startIdx.back() = 0;
                size_t nStartCount = (psOptions->nLimitValuesByDim + 1) / 2;
                count.back() = nStartCount;
                if (!array->Read(startIdx.data(), count.data(), nullptr,
                                 nullptr, dt, &abyTmp[0]))
                    break;
                lambdaDumpValue(abyTmp, count.back());
                serializer.Add("[...]");

                count.back() = psOptions->nLimitValuesByDim / 2;
                if (count.back())
                {
                    startIdx.back() = dimSizes.back() - count.back();
                    if (!array->Read(startIdx.data(), count.data(), nullptr,
                                     nullptr, dt, &abyTmp[0]))
                        break;
                    lambdaDumpValue(abyTmp, count.back());
                }
            }
        }
        else
        {
            if (psOptions->nLimitValuesByDim == 0 ||
                dimSizes[nCurDim] <= psOptions->nLimitValuesByDim)
            {
                for (startIdx[nCurDim] = 0;
                     startIdx[nCurDim] < dimSizes[nCurDim]; ++startIdx[nCurDim])
                {
                    DumpArrayRec(array, serializer, nCurDim + 1, dimSizes,
                                 startIdx, psOptions);
                }
            }
            else
            {
                size_t nStartCount = (psOptions->nLimitValuesByDim + 1) / 2;
                for (startIdx[nCurDim] = 0; startIdx[nCurDim] < nStartCount;
                     ++startIdx[nCurDim])
                {
                    DumpArrayRec(array, serializer, nCurDim + 1, dimSizes,
                                 startIdx, psOptions);
                }
                serializer.Add("[...]");
                size_t nEndCount = psOptions->nLimitValuesByDim / 2;
                for (startIdx[nCurDim] = dimSizes[nCurDim] - nEndCount;
                     startIdx[nCurDim] < dimSizes[nCurDim]; ++startIdx[nCurDim])
                {
                    DumpArrayRec(array, serializer, nCurDim + 1, dimSizes,
                                 startIdx, psOptions);
                }
            }
        }
    } while (false);
    serializer.SetNewline(true);
}

/************************************************************************/
/*                           DumpDimensions()                           */
/************************************************************************/

static void
DumpDimensions(const std::shared_ptr<GDALGroup> &rootGroup,
               const std::vector<std::shared_ptr<GDALDimension>> &dims,
               CPLJSonStreamingWriter &serializer,
               const GDALMultiDimInfoOptions *psOptions,
               std::set<std::string> &alreadyDumpedDimensions)
{
    auto arrayContext(serializer.MakeArrayContext());
    for (const auto &dim : dims)
    {
        std::string osFullname(dim->GetFullName());
        if (alreadyDumpedDimensions.find(osFullname) !=
            alreadyDumpedDimensions.end())
        {
            serializer.Add(osFullname);
            continue;
        }

        auto dimObjectContext(serializer.MakeObjectContext());
        if (!osFullname.empty() && osFullname[0] == '/')
            alreadyDumpedDimensions.insert(osFullname);

        serializer.AddObjKey("name");
        serializer.Add(dim->GetName());

        serializer.AddObjKey("full_name");
        serializer.Add(osFullname);

        serializer.AddObjKey("size");
        serializer.Add(static_cast<std::uint64_t>(dim->GetSize()));

        const auto &type(dim->GetType());
        if (!type.empty())
        {
            serializer.AddObjKey("type");
            serializer.Add(type);
        }

        const auto &direction(dim->GetDirection());
        if (!direction.empty())
        {
            serializer.AddObjKey("direction");
            serializer.Add(direction);
        }

        auto poIndexingVariable(dim->GetIndexingVariable());
        if (poIndexingVariable)
        {
            serializer.AddObjKey("indexing_variable");
            bool isKnownFromRoot;
            {
                // For autotest/gdrivers/tiledb_multidim.py::test_tiledb_multidim_array_read_dim_label_and_spatial_ref
                CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                isKnownFromRoot =
                    rootGroup->OpenMDArrayFromFullname(
                        poIndexingVariable->GetFullName()) != nullptr;
            }
            if (isKnownFromRoot)
            {
                serializer.Add(poIndexingVariable->GetFullName());
            }
            else
            {
                std::set<std::string> alreadyDumpedDimensionsLocal(
                    alreadyDumpedDimensions);
                alreadyDumpedDimensionsLocal.insert(std::move(osFullname));
                std::set<std::string> alreadyDumpedArrays;

                auto indexingVariableContext(serializer.MakeObjectContext());
                serializer.AddObjKey(poIndexingVariable->GetName());
                DumpArray(rootGroup, poIndexingVariable, serializer, psOptions,
                          alreadyDumpedDimensionsLocal, alreadyDumpedArrays,
                          /* bOutputObjType = */ false,
                          /* bOutputName = */ false,
                          /* bOutputOverviews = */ false);
            }
        }
    }
}

/************************************************************************/
/*                         DumpStructuralInfo()                         */
/************************************************************************/

static void DumpStructuralInfo(CSLConstList papszStructuralInfo,
                               CPLJSonStreamingWriter &serializer)
{
    auto objectContext(serializer.MakeObjectContext());
    int i = 1;
    for (const auto &[pszKey, pszValue] : cpl::IterateNameValue(
             papszStructuralInfo, /* bReturnNullKeyIfNotNameValue = */ true))
    {
        if (pszKey)
        {
            serializer.AddObjKey(pszKey);
        }
        else
        {
            serializer.AddObjKey(CPLSPrintf("metadata_%d", i));
            ++i;
        }
        serializer.Add(pszValue);
    }
}

/************************************************************************/
/*                             DumpArray()                              */
/************************************************************************/

static void DumpArray(const std::shared_ptr<GDALGroup> &rootGroup,
                      const std::shared_ptr<GDALMDArray> &array,
                      CPLJSonStreamingWriter &serializer,
                      const GDALMultiDimInfoOptions *psOptions,
                      std::set<std::string> &alreadyDumpedDimensions,
                      std::set<std::string> &alreadyDumpedArrays,
                      bool bOutputObjType, bool bOutputName,
                      bool bOutputOverviews)
{
    // Protection against infinite recursion
    if (cpl::contains(alreadyDumpedArrays, array->GetFullName()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s already visited",
                 array->GetFullName().c_str());
        return;
    }
    alreadyDumpedArrays.insert(array->GetFullName());

    auto objectContext(serializer.MakeObjectContext());
    if (bOutputObjType)
    {
        serializer.AddObjKey("type");
        serializer.Add("array");
    }
    if (bOutputName)
    {
        serializer.AddObjKey("name");
        serializer.Add(array->GetName());
    }
    else
    {
        serializer.AddObjKey("full_name");
        serializer.Add(array->GetFullName());
    }

    if (psOptions->bSummary)
        return;

    serializer.AddObjKey("datatype");
    const auto &dt(array->GetDataType());
    DumpDataType(dt, serializer);

    auto dims = array->GetDimensions();
    if (!dims.empty())
    {
        serializer.AddObjKey("dimensions");
        DumpDimensions(rootGroup, dims, serializer, psOptions,
                       alreadyDumpedDimensions);

        serializer.AddObjKey("dimension_size");
        auto arrayContext(serializer.MakeArrayContext());
        for (const auto &poDim : dims)
        {
            serializer.Add(static_cast<uint64_t>(poDim->GetSize()));
        }
    }

    bool hasNonNullBlockSize = false;
    const auto blockSize = array->GetBlockSize();
    for (auto v : blockSize)
    {
        if (v != 0)
        {
            hasNonNullBlockSize = true;
            break;
        }
    }
    if (hasNonNullBlockSize)
    {
        serializer.AddObjKey("block_size");
        auto arrayContext(serializer.MakeArrayContext());
        for (auto v : blockSize)
        {
            serializer.Add(static_cast<uint64_t>(v));
        }
    }

    CPLStringList aosOptions;
    if (psOptions->bDetailed)
        aosOptions.SetNameValue("SHOW_ALL", "YES");
    auto attrs = array->GetAttributes(aosOptions.List());
    if (!attrs.empty())
    {
        serializer.AddObjKey("attributes");
        DumpAttrs(attrs, serializer, psOptions);
    }

    const auto &unit = array->GetUnit();
    if (!unit.empty())
    {
        serializer.AddObjKey("unit");
        serializer.Add(unit);
    }

    auto nodata = array->GetRawNoDataValue();
    if (nodata)
    {
        serializer.AddObjKey("nodata_value");
        DumpValue(serializer, static_cast<const GByte *>(nodata), dt);
    }

    bool bValid = false;
    double dfOffset = array->GetOffset(&bValid);
    if (bValid)
    {
        serializer.AddObjKey("offset");
        serializer.Add(dfOffset);
    }
    double dfScale = array->GetScale(&bValid);
    if (bValid)
    {
        serializer.AddObjKey("scale");
        serializer.Add(dfScale);
    }

    auto srs = array->GetSpatialRef();
    if (srs)
    {
        char *pszWKT = nullptr;
        CPLStringList wktOptions;
        wktOptions.SetNameValue("FORMAT", "WKT2_2018");
        if (srs->exportToWkt(&pszWKT, wktOptions.List()) == OGRERR_NONE)
        {
            serializer.AddObjKey("srs");
            {
                auto srsContext(serializer.MakeObjectContext());
                serializer.AddObjKey("wkt");
                serializer.Add(pszWKT);
                serializer.AddObjKey("data_axis_to_srs_axis_mapping");
                {
                    auto dataAxisContext(serializer.MakeArrayContext(true));
                    auto mapping = srs->GetDataAxisToSRSAxisMapping();
                    for (const auto &axisNumber : mapping)
                        serializer.Add(axisNumber);
                }
            }
        }
        CPLFree(pszWKT);
    }

    auto papszStructuralInfo = array->GetStructuralInfo();
    if (papszStructuralInfo)
    {
        serializer.AddObjKey("structural_info");
        DumpStructuralInfo(papszStructuralInfo, serializer);
    }

    if (psOptions->bDetailed)
    {
        serializer.AddObjKey("values");
        if (dims.empty())
        {
            std::vector<GByte> abyTmp(dt.GetSize());
            array->Read(nullptr, nullptr, nullptr, nullptr, dt, &abyTmp[0]);
            DumpValue(serializer, &abyTmp[0], dt);
        }
        else
        {
            std::vector<GUInt64> startIdx(dims.size());
            std::vector<GUInt64> dimSizes;
            for (const auto &dim : dims)
                dimSizes.emplace_back(dim->GetSize());
            DumpArrayRec(array, serializer, 0, dimSizes, startIdx, psOptions);
        }
    }

    if (psOptions->bStats)
    {
        double dfMin = 0.0;
        double dfMax = 0.0;
        double dfMean = 0.0;
        double dfStdDev = 0.0;
        GUInt64 nValidCount = 0;
        if (array->GetStatistics(false, true, &dfMin, &dfMax, &dfMean,
                                 &dfStdDev, &nValidCount, nullptr,
                                 nullptr) == CE_None)
        {
            serializer.AddObjKey("statistics");
            auto statContext(serializer.MakeObjectContext());
            if (nValidCount > 0)
            {
                serializer.AddObjKey("min");
                serializer.Add(dfMin);

                serializer.AddObjKey("max");
                serializer.Add(dfMax);

                serializer.AddObjKey("mean");
                serializer.Add(dfMean);

                serializer.AddObjKey("stddev");
                serializer.Add(dfStdDev);
            }

            serializer.AddObjKey("valid_sample_count");
            serializer.Add(static_cast<std::uint64_t>(nValidCount));
        }
    }

    if (bOutputOverviews)
    {
        const int nOverviews = array->GetOverviewCount();
        if (nOverviews > 0)
        {
            serializer.AddObjKey("overviews");
            auto overviewsContext(serializer.MakeArrayContext());
            for (int i = 0; i < nOverviews; ++i)
            {
                if (auto poOvrArray = array->GetOverview(i))
                {
                    bool bIsStandalone = false;
                    {
                        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                        bIsStandalone =
                            rootGroup->OpenMDArrayFromFullname(
                                poOvrArray->GetFullName()) == nullptr;
                    }
                    if (bIsStandalone)
                    {
                        DumpArray(rootGroup, poOvrArray, serializer, psOptions,
                                  alreadyDumpedDimensions, alreadyDumpedArrays,
                                  bOutputObjType, bOutputName,
                                  bOutputOverviews);
                    }
                    else
                    {
                        serializer.Add(poOvrArray->GetFullName());
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                             DumpArrays()                             */
/************************************************************************/

static void DumpArrays(const std::shared_ptr<GDALGroup> &rootGroup,
                       const std::shared_ptr<GDALGroup> &group,
                       const std::vector<std::string> &arrayNames,
                       CPLJSonStreamingWriter &serializer,
                       const GDALMultiDimInfoOptions *psOptions,
                       std::set<std::string> &alreadyDumpedDimensions,
                       std::set<std::string> &alreadyDumpedArrays)
{
    std::set<std::string> oSetNames;
    auto objectContext(serializer.MakeObjectContext());
    for (const auto &name : arrayNames)
    {
        if (oSetNames.find(name) != oSetNames.end())
            continue;  // should not happen on well behaved drivers
        oSetNames.insert(name);
        auto array = group->OpenMDArray(name);
        if (array)
        {
            serializer.AddObjKey(array->GetName());
            DumpArray(rootGroup, array, serializer, psOptions,
                      alreadyDumpedDimensions, alreadyDumpedArrays, false,
                      false, /* bOutputOverviews = */ true);
        }
    }
}

/************************************************************************/
/*                             DumpGroup()                              */
/************************************************************************/

static void DumpGroup(const std::shared_ptr<GDALGroup> &rootGroup,
                      const std::shared_ptr<GDALGroup> &group,
                      const char *pszDriverName,
                      CPLJSonStreamingWriter &serializer,
                      const GDALMultiDimInfoOptions *psOptions,
                      std::set<std::string> &alreadyDumpedDimensions,
                      std::set<std::string> &alreadyDumpedArrays,
                      bool bOutputObjType, bool bOutputName)
{
    auto objectContext(serializer.MakeObjectContext());
    if (bOutputObjType)
    {
        serializer.AddObjKey("type");
        serializer.Add("group");
    }
    if (pszDriverName)
    {
        serializer.AddObjKey("driver");
        serializer.Add(pszDriverName);
    }
    if (bOutputName)
    {
        serializer.AddObjKey("name");
        serializer.Add(group->GetName());

        // If the root group is not actually the root, print its full path
        if (pszDriverName != nullptr && group->GetName() != "/")
        {
            serializer.AddObjKey("full_name");
            serializer.Add(group->GetFullName());
        }
    }
    else if (psOptions->bSummary)
    {
        serializer.AddObjKey("full_name");
        serializer.Add(group->GetFullName());
    }

    CPLStringList aosOptionsGetAttr;
    if (psOptions->bDetailed)
        aosOptionsGetAttr.SetNameValue("SHOW_ALL", "YES");
    auto attrs = group->GetAttributes(aosOptionsGetAttr.List());
    if (!psOptions->bSummary && !attrs.empty())
    {
        serializer.AddObjKey("attributes");
        DumpAttrs(attrs, serializer, psOptions);
    }

    auto dims = group->GetDimensions();
    if (!psOptions->bSummary && !dims.empty())
    {
        serializer.AddObjKey("dimensions");
        DumpDimensions(rootGroup, dims, serializer, psOptions,
                       alreadyDumpedDimensions);
    }

    const auto &types = group->GetDataTypes();
    if (!psOptions->bSummary && !types.empty())
    {
        serializer.AddObjKey("datatypes");
        auto arrayContext(serializer.MakeArrayContext());
        for (const auto &dt : types)
        {
            DumpDataType(*(dt.get()), serializer);
        }
    }

    CPLStringList aosOptionsGetArray(psOptions->aosArrayOptions);
    if (psOptions->bDetailed)
        aosOptionsGetArray.SetNameValue("SHOW_ALL", "YES");
    auto arrayNames = group->GetMDArrayNames(aosOptionsGetArray.List());
    if (!arrayNames.empty())
    {
        serializer.AddObjKey("arrays");
        DumpArrays(rootGroup, group, arrayNames, serializer, psOptions,
                   alreadyDumpedDimensions, alreadyDumpedArrays);
    }

    auto papszStructuralInfo = group->GetStructuralInfo();
    if (!psOptions->bSummary && papszStructuralInfo)
    {
        serializer.AddObjKey("structural_info");
        DumpStructuralInfo(papszStructuralInfo, serializer);
    }

    auto subgroupNames = group->GetGroupNames();
    if (!subgroupNames.empty())
    {
        serializer.AddObjKey("groups");
        if (HasUniqueNames(subgroupNames))
        {
            auto groupContext(serializer.MakeObjectContext());
            for (const auto &subgroupName : subgroupNames)
            {
                auto subgroup = group->OpenGroup(subgroupName);
                if (subgroup)
                {
                    serializer.AddObjKey(subgroupName);
                    DumpGroup(rootGroup, subgroup, nullptr, serializer,
                              psOptions, alreadyDumpedDimensions,
                              alreadyDumpedArrays, false, false);
                }
            }
        }
        else
        {
            auto arrayContext(serializer.MakeArrayContext());
            for (const auto &subgroupName : subgroupNames)
            {
                auto subgroup = group->OpenGroup(subgroupName);
                if (subgroup)
                {
                    DumpGroup(rootGroup, subgroup, nullptr, serializer,
                              psOptions, alreadyDumpedDimensions,
                              alreadyDumpedArrays, false, true);
                }
            }
        }
    }
}

/************************************************************************/
/*                     GDALMultiDimTextOutputDumper                     */
/************************************************************************/

using TableType = std::vector<std::vector<std::string>>;

struct GDALMultiDimTextOutputDumper
{
    const GDALMultiDimInfoOptions &m_sOptions;
    std::string m_osOutput{};

    explicit GDALMultiDimTextOutputDumper(
        const GDALMultiDimInfoOptions *psOptions)
        : m_sOptions(*psOptions)
    {
    }

    void AddLine(const std::string &osLine = std::string())
    {
        if (m_sOptions.bStdoutOutput)
        {
            printf("%s\n", osLine.c_str());
        }
        else
        {
            m_osOutput += osLine;
            m_osOutput += '\n';
        }
    }

    void DumpTable(const TableType &lines, int nIndentSpaces = 2,
                   const std::vector<size_t> &anColSizeIn = {},
                   bool headerLineSeparator = true,
                   bool bLeftPadNumbers = true);

    void
    DumpAttributes(const std::vector<std::shared_ptr<GDALAttribute>> &apoAttrs,
                   int nIndentSpaces);

    void DumpStructuralInfo(CSLConstList papszStructuralInfo,
                            int nIndentSpaces);

    void DumpArray(const std::shared_ptr<GDALMDArray> &poArray);

    std::set<std::string>
    DumpDimensionsSummary(const std::shared_ptr<GDALGroup> &group);

    void DumpArraysSummary(
        const std::shared_ptr<GDALGroup> &group,
        const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays);

    void DrumpGroupsSummary(const std::shared_ptr<GDALGroup> &group);
};

/************************************************************************/
/*                             DumpTable()                              */
/************************************************************************/

void GDALMultiDimTextOutputDumper::DumpTable(
    const TableType &lines, int nIndentSpaces,
    const std::vector<size_t> &anColSizeIn, bool headerLineSeparator,
    bool bLeftPadNumbers)
{
    // Compute column max size if needed
    std::vector<size_t> anColSizeTmp;
    if (anColSizeIn.empty())
    {
        for (const auto &line : lines)
        {
            if (line.size() > anColSizeTmp.size())
                anColSizeTmp.resize(line.size());
            for (const auto [idxCol, col] : cpl::enumerate(line))
            {
                anColSizeTmp[idxCol] =
                    std::max(anColSizeTmp[idxCol], col.size());
            }
        }
    }
    const auto &anColSize = anColSizeIn.empty() ? anColSizeTmp : anColSizeIn;

    // Check which columns are numeric-only
    std::vector<bool> abIsNumbersOnly(anColSize.size(), true);
    for (const auto [idxLine, line] : cpl::enumerate(lines))
    {
        CPLAssert(abIsNumbersOnly.size() >= line.size());
        if (!headerLineSeparator || idxLine > 0)
        {
            for (const auto [idxCol, col] : cpl::enumerate(line))
            {
                abIsNumbersOnly[idxCol] =
                    abIsNumbersOnly[idxCol] &&
                    CPLGetValueType(col.c_str()) != CPL_VALUE_STRING;
            }
        }
    }

    // Now actually print table
    for (const auto [idxLine, line] : cpl::enumerate(lines))
    {
        CPLAssert(anColSize.size() >= line.size());
        std::string osFormattedLine(nIndentSpaces, ' ');
        for (const auto [idxCol, col] : cpl::enumerate(line))
        {
            if (idxCol > 0)
                osFormattedLine += "  ";
            if (idxLine == 0 && headerLineSeparator)
            {
                const size_t nLeftPadding =
                    (anColSize[idxCol] - col.size()) / 2;
                osFormattedLine += std::string(nLeftPadding, ' ');
                osFormattedLine += col;
                osFormattedLine += std::string(
                    anColSize[idxCol] - col.size() - nLeftPadding, ' ');
            }
            else if (!bLeftPadNumbers || !abIsNumbersOnly[idxCol])
            {
                osFormattedLine += col;
                osFormattedLine +=
                    std::string(anColSize[idxCol] - col.size(), ' ');
            }
            else
            {
                osFormattedLine +=
                    std::string(anColSize[idxCol] - col.size(), ' ');
                osFormattedLine += col;
            }
        }
        AddLine(osFormattedLine);

        if (idxLine == 0 && headerLineSeparator)
        {
            osFormattedLine = std::string(nIndentSpaces, ' ');
            for (size_t idxCol = 0; idxCol < line.size(); ++idxCol)
            {
                if (idxCol > 0)
                    osFormattedLine += "  ";
                osFormattedLine += std::string(anColSize[idxCol], '-');
            }
            AddLine(osFormattedLine);
        }
    }
}

/************************************************************************/
/*                            TypeToString()                            */
/************************************************************************/

static std::string TypeToString(const GDALExtendedDataType &dt,
                                bool emitCompoundName = true)
{
    std::string ret;
    switch (dt.GetClass())
    {
        case GEDTC_STRING:
            ret = "String";
            break;

        case GEDTC_NUMERIC:
            ret = GDALGetDataTypeName(dt.GetNumericDataType());
            break;

        case GEDTC_COMPOUND:
        {
            if (emitCompoundName && !dt.GetName().empty())
            {
                ret = dt.GetName();
                ret += ": ";
            }
            ret += '{';
            bool firstComp = true;
            const auto &components = dt.GetComponents();
            for (const auto &comp : components)
            {
                if (!firstComp)
                    ret += ", ";
                firstComp = false;
                ret += comp->GetName();
                ret += ": ";
                ret += TypeToString(comp->GetType(), false);
            }
            ret += '}';
            break;
        }
    }
    return ret;
}

/************************************************************************/
/*                           DumpAttributes()                           */
/************************************************************************/

void GDALMultiDimTextOutputDumper::DumpAttributes(
    const std::vector<std::shared_ptr<GDALAttribute>> &apoAttrs,
    int nIndentSpaces)
{
    if (!apoAttrs.empty())
    {
        AddLine();
        AddLine(std::string(nIndentSpaces, ' ').append("Attributes:"));
        TableType attrs;
        attrs.push_back({"Name", "Type", "Value"});

        for (const auto &poAttr : apoAttrs)
        {
            CPLJSonStreamingWriter serializer(nullptr, nullptr);
            serializer.SetPrettyFormatting(false);
            DumpAttrValue(poAttr, serializer);
            std::string osAttrVal = serializer.GetString();
            constexpr size_t MAX_COLS = 80;
            if (osAttrVal.size() <= MAX_COLS)
            {
                attrs.push_back({poAttr->GetName(),
                                 TypeToString(poAttr->GetDataType()),
                                 osAttrVal});
            }
            else
            {
                bool bFirstLineAttr = true;
                while (osAttrVal.size() > MAX_COLS)
                {
                    auto nLastBreak = osAttrVal.find_last_of(" ,\n", MAX_COLS);
                    if (nLastBreak == std::string::npos)
                        nLastBreak = osAttrVal.find_first_of(" ,\n");
                    if (nLastBreak != std::string::npos)
                    {
                        auto osVal = osAttrVal.substr(0, nLastBreak);
                        if (osAttrVal[nLastBreak] != ' ' &&
                            osAttrVal[nLastBreak] != '\n')
                            osVal += osAttrVal[nLastBreak];
                        attrs.push_back(
                            {bFirstLineAttr ? poAttr->GetName() : std::string(),
                             bFirstLineAttr
                                 ? TypeToString(poAttr->GetDataType())
                                 : std::string(),
                             osVal});
                        osAttrVal = osAttrVal.substr(nLastBreak + 1);
                    }
                    else
                    {
                        break;
                    }
                    bFirstLineAttr = false;
                }
                attrs.push_back(
                    {bFirstLineAttr ? poAttr->GetName() : std::string(),
                     bFirstLineAttr ? TypeToString(poAttr->GetDataType())
                                    : std::string(),
                     osAttrVal});
            }
        }
        DumpTable(attrs, nIndentSpaces + 2);
    }
}

/************************************************************************/
/*                         DimsToShapeString()                          */
/************************************************************************/

static std::string
DimsToShapeString(const std::vector<std::shared_ptr<GDALDimension>> &dims)
{
    std::string s("[");
    for (const auto &poDim : dims)
    {
        if (s.size() > 1)
            s += ", ";
        s += std::to_string(poDim->GetSize());
    }
    s += ']';
    return s;
}

/************************************************************************/
/*                            DimsToString()                            */
/************************************************************************/

static std::string
DimsToString(const std::vector<std::shared_ptr<GDALDimension>> &dims,
             const std::string &osSameDimGroup)
{
    std::string s("(");
    for (const auto &poDim : dims)
    {
        if (s.size() > 1)
            s += ", ";
        else if (!osSameDimGroup.empty())
        {
            s += osSameDimGroup;
            s += '/';
        }
        const std::string &osFullname = poDim->GetFullName();
        if (!osFullname.empty())
        {
            s += osSameDimGroup.empty()
                     ? osFullname
                     : osFullname.substr(osSameDimGroup.size() + 1);
        }
        else
        {
            s += "(unnamed)";
        }
    }
    s += ')';
    return s;
}

/************************************************************************/
/*                         DimsToSameDimGroup()                         */
/************************************************************************/

/** Return the prefix shared by all dimensions if there is one, or empty string
 * otherwise.
 */
static std::string
DimsToSameDimGroup(const std::vector<std::shared_ptr<GDALDimension>> &dims)
{
    std::string osSameDimGroup;
    for (const auto &poDim : dims)
    {
        std::string osDimGroup = poDim->GetFullName();
        const auto nPos = osDimGroup.rfind('/');
        if (nPos != std::string::npos)
        {
            osDimGroup.resize(nPos);
            if (osSameDimGroup.empty())
            {
                osSameDimGroup = osDimGroup;
            }
            else if (osSameDimGroup != osDimGroup)
            {
                return {};
            }
        }
        else
        {
            return {};
        }
    }
    return osSameDimGroup;
}

/************************************************************************/
/*                         BlockSizeToString()                          */
/************************************************************************/

template <class T>
static std::string BlockSizeToString(const std::vector<T> &anBlockSize,
                                     bool *pbAllZero = nullptr)
{
    std::string s("[");
    if (pbAllZero)
        *pbAllZero = true;
    for (const auto nSize : anBlockSize)
    {
        if (s.size() > 1)
            s += ", ";
        s += std::to_string(nSize);
        if (pbAllZero)
            *pbAllZero = *pbAllZero && nSize == 0;
    }
    s += ']';
    return s;
}

/************************************************************************/
/*                    WriteToStdoutWithSpaceIndent()                    */
/************************************************************************/

static void WriteToStdoutWithSpaceIndent(const char *pszText, void *pUserData)
{
    const int *pnIndent = static_cast<const int *>(pUserData);
    printf("%s", pszText);
    if (pszText[0] == '\n')
        printf("%s", std::string(*pnIndent, ' ').c_str());
}

/************************************************************************/
/*          GDALMultiDimTextOutputDumper::DumpStructuralInfo()          */
/************************************************************************/

void GDALMultiDimTextOutputDumper::DumpStructuralInfo(
    CSLConstList papszStructuralInfo, int nIndentSpaces)
{
    AddLine();
    AddLine(std::string(nIndentSpaces, ' ').append("Structural metadata:"));

    TableType info;

    int i = 1;
    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(papszStructuralInfo,
                               /* bReturnNullKeyIfNotNameValue = */ true))
    {
        std::vector<std::string> line;
        if (pszKey)
        {
            line.push_back(pszKey);
        }
        else
        {
            line.push_back(CPLSPrintf("metadata_%d", i));
            ++i;
        }
        line.push_back(pszValue);
        info.push_back(std::move(line));
    }
    DumpTable(info, nIndentSpaces + 2, {}, false);
}

/************************************************************************/
/*              GDALMultiDimTextOutputDumper::DumpArray()               */
/************************************************************************/

void GDALMultiDimTextOutputDumper::DumpArray(
    const std::shared_ptr<GDALMDArray> &poArray)
{
    AddLine();
    AddLine("  - " + poArray->GetFullName() + ":");

    constexpr int INDENT_LEVEL = 6;

    TableType props;
    const auto &dims = poArray->GetDimensions();
    props.push_back(
        {"Dimensions:", DimsToString(dims, DimsToSameDimGroup(dims))});
    props.push_back({"Shape:", DimsToShapeString(dims)});
    bool bAllZero = true;
    std::string osChunkSize =
        BlockSizeToString(poArray->GetBlockSize(), &bAllZero);
    if (!bAllZero)
        props.push_back({"Chunk size:", std::move(osChunkSize)});

    const auto &dt = poArray->GetDataType();
    props.push_back({"Type:", TypeToString(dt)});
    const std::string &osUnit = poArray->GetUnit();
    if (!osUnit.empty())
        props.push_back({"Unit:", osUnit});

    if (const auto nodata = poArray->GetRawNoDataValue())
    {
        CPLJSonStreamingWriter serializer(nullptr, nullptr);
        serializer.SetPrettyFormatting(false);
        DumpValue(serializer, static_cast<const GByte *>(nodata), dt);
        props.push_back({"Nodata value:", serializer.GetString()});
    }
    DumpTable(props, INDENT_LEVEL, {},
              /* headerLineSeparator =  */ false,
              /* bLeftPadNumbers = */ false);

    DumpAttributes(poArray->GetAttributes(), INDENT_LEVEL);

    if (const auto poSRS = poArray->GetSpatialRef())
    {
        AddLine();
        std::string osCRS;
        EmitTextDisplayOfCRS(poSRS.get(), "AUTO", "Coordinate Reference System",
                             [&osCRS](const std::string &s) { osCRS += s; });
        const CPLStringList aosLines(
            CSLTokenizeString2(osCRS.c_str(), "\n", 0));
        for (const char *pszLine : aosLines)
            AddLine(std::string(INDENT_LEVEL, ' ').append(pszLine));
    }

    if (CSLConstList papszStructuralInfo = poArray->GetStructuralInfo())
    {
        DumpStructuralInfo(papszStructuralInfo, INDENT_LEVEL);
    }

    if (m_sOptions.bStats)
    {
        double dfMin = 0.0;
        double dfMax = 0.0;
        double dfMean = 0.0;
        double dfStdDev = 0.0;
        GUInt64 nValidCount = 0;
        if (poArray->GetStatistics(false, true, &dfMin, &dfMax, &dfMean,
                                   &dfStdDev, &nValidCount, nullptr,
                                   nullptr) == CE_None)
        {
            AddLine();
            AddLine(std::string(INDENT_LEVEL, ' ').append("Statistics:"));

            TableType stats;
            if (nValidCount > 0)
            {
                stats.push_back({"min", std::to_string(dfMin)});
                stats.push_back({"max", std::to_string(dfMax)});
                stats.push_back({"mean", std::to_string(dfMean)});
                stats.push_back({"stddev", std::to_string(dfStdDev)});
            }
            stats.push_back(
                {"valid sample count", std::to_string(nValidCount)});

            DumpTable(stats, INDENT_LEVEL + 2, {}, false);
        }
    }

    if (m_sOptions.bDetailed)
    {
        if (dims.empty())
        {
            std::vector<GByte> abyTmp(dt.GetSize());
            poArray->Read(nullptr, nullptr, nullptr, nullptr, dt, &abyTmp[0]);
            CPLJSonStreamingWriter serializer(nullptr, nullptr);
            DumpValue(serializer, &abyTmp[0], dt);

            AddLine();
            AddLine(std::string(INDENT_LEVEL, ' ')
                        .append("Value: ")
                        .append(serializer.GetString()));
        }
        else
        {
            AddLine();
            AddLine(std::string(INDENT_LEVEL, ' ').append("Values:"));

            int nIndent = INDENT_LEVEL;
            CPLJSonStreamingWriter serializer(m_sOptions.bStdoutOutput
                                                  ? WriteToStdoutWithSpaceIndent
                                                  : nullptr,
                                              &nIndent);
            std::vector<GUInt64> startIdx(dims.size());
            std::vector<GUInt64> dimSizes;
            for (const auto &dim : dims)
                dimSizes.emplace_back(dim->GetSize());
            if (m_sOptions.bStdoutOutput)
                printf("%s", std::string(INDENT_LEVEL, ' ').c_str());
            DumpArrayRec(poArray, serializer, 0, dimSizes, startIdx,
                         &m_sOptions);
            if (!m_sOptions.bStdoutOutput)
            {
                AddLine(std::string(INDENT_LEVEL, ' ')
                            .append(CPLString(serializer.GetString())
                                        .replaceAll(
                                            '\n', std::string("\n").append(
                                                      std::string(INDENT_LEVEL,
                                                                  ' ')))));
            }
            else
            {
                AddLine();
            }
        }
    }
}

/************************************************************************/
/*        GDALMultiDimTextOutputDumper:: DumpDimensionsSummary()        */
/************************************************************************/

std::set<std::string> GDALMultiDimTextOutputDumper::DumpDimensionsSummary(
    const std::shared_ptr<GDALGroup> &group)
{
    std::set<std::string> oSetIndexingVariablePaths;

    const auto dims = group->GetDimensionsRecursive();
    if (!dims.empty())
    {
        AddLine();
        AddLine("Dimensions:");
        TableType lines;
        lines.push_back({"Name (path)", "Size", "Type", "Direction"});
        for (const auto &poDim : dims)
        {
            std::vector<std::string> line{
                poDim->GetFullName(), std::to_string(poDim->GetSize()),
                poDim->GetType(), poDim->GetDirection()};
            lines.push_back(std::move(line));
            const auto poIndexingVariable = poDim->GetIndexingVariable();
            if (poIndexingVariable)
            {
                const auto &osIndexingVarPath =
                    poIndexingVariable->GetFullName();
                if (!osIndexingVarPath.empty() && osIndexingVarPath[0] == '/')
                {
                    oSetIndexingVariablePaths.insert(osIndexingVarPath);
                }
            }
        }
        DumpTable(lines);
    }

    return oSetIndexingVariablePaths;
}

/************************************************************************/
/*          GDALMultiDimTextOutputDumper:: DumpArraysSummary()          */
/************************************************************************/

void GDALMultiDimTextOutputDumper::DumpArraysSummary(
    const std::shared_ptr<GDALGroup> &group,
    const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays)
{
    const std::set<std::string> oSetIndexingVariablePaths =
        DumpDimensionsSummary(group);

    TableType linesCoordinateArrays;
    TableType linesScalarArrays;
    std::map<std::string, TableType> linesDataArrays;
    for (const auto &poArray : apoArrays)
    {
        const auto &dims = poArray->GetDimensions();
        if (cpl::contains(oSetIndexingVariablePaths, poArray->GetFullName()))
        {
            if (linesCoordinateArrays.empty())
                linesCoordinateArrays.push_back(
                    {"Name (path)", "Dimension", "Type", "Unit"});
            std::string osDimension;
            for (const auto &poDim : dims)
            {
                if (osDimension.empty())
                    osDimension = '(';
                else
                    osDimension += ", ";
                osDimension += poDim->GetName();
            }
            if (!osDimension.empty())
                osDimension += ')';
            std::vector<std::string> line{
                poArray->GetFullName(), std::move(osDimension),
                TypeToString(poArray->GetDataType()), poArray->GetUnit()};
            linesCoordinateArrays.push_back(std::move(line));
        }
        else if (dims.empty())
        {
            if (linesScalarArrays.empty())
                linesScalarArrays.push_back({"Name (path)", "Type", "Unit"});
            std::vector<std::string> line{poArray->GetFullName(),
                                          TypeToString(poArray->GetDataType()),
                                          poArray->GetUnit()};
            linesScalarArrays.push_back(std::move(line));
        }
        else
        {
            const std::string osSameDimGroup = DimsToSameDimGroup(dims);

            bool bAllZero = true;
            std::string osChunkSize =
                BlockSizeToString(poArray->GetBlockSize(), &bAllZero);

            std::vector<std::string> line{
                poArray->GetFullName(), TypeToString(poArray->GetDataType()),
                poArray->GetUnit(), DimsToShapeString(dims),
                bAllZero ? std::string("(unknown)") : osChunkSize};
            linesDataArrays[DimsToString(dims, osSameDimGroup)].push_back(
                std::move(line));
        }
    }

    if (!linesCoordinateArrays.empty())
    {
        AddLine();
        AddLine("Coordinates (indexing variables):");
        DumpTable(linesCoordinateArrays);
    }

    if (!linesDataArrays.empty())
    {
        AddLine();
        AddLine("Data variables:");

        TableType headerLine;
        headerLine.push_back(
            {"Name (path)", "Type", "Unit", "Shape", "Chunk size"});

        std::vector<size_t> anColSizeDataVars;
        anColSizeDataVars.resize(headerLine[0].size());
        for (const auto [iCol, col] : cpl::enumerate(headerLine[0]))
            anColSizeDataVars[iCol] = col.size();
        for (const auto &[_, lines] : linesDataArrays)
        {
            for (const auto &line : lines)
            {
                for (const auto [iCol, col] : cpl::enumerate(line))
                {
                    anColSizeDataVars[iCol] =
                        std::max(anColSizeDataVars[iCol], col.size());
                }
            }
        }

        DumpTable(headerLine, 2, anColSizeDataVars);
        for (const auto &[path, lines] : linesDataArrays)
        {
            AddLine();
            AddLine(" " + path + ":");
            DumpTable(lines, 2, anColSizeDataVars, false);
        }
    }

    if (!linesScalarArrays.empty())
    {
        AddLine();
        AddLine("Scalar arrays:");
        DumpTable(linesScalarArrays);
    }
}

/************************************************************************/
/*          GDALMultiDimTextOutputDumper::DrumpGroupsSummary()          */
/************************************************************************/

void GDALMultiDimTextOutputDumper::DrumpGroupsSummary(
    const std::shared_ptr<GDALGroup> &group)
{
    const auto apoGroups = group->GetGroupsRecursive();
    if (!apoGroups.empty())
    {
        AddLine();
        AddLine("Groups:");
        std::vector<std::vector<std::string>> groupNames;
        for (auto &poGroup : apoGroups)
            groupNames.push_back(CPLStringList(
                CSLTokenizeString2(poGroup->GetFullName().c_str(), "/", 0)));
        std::sort(groupNames.begin(), groupNames.end());
        for (const auto &groupName : groupNames)
        {
            std::string osLine("  ");
            for (const auto &part : groupName)
            {
                osLine += '/';
                osLine += part;
            }
            AddLine(osLine);
        }
    }
}

/************************************************************************/
/*                             DumpAsText()                             */
/************************************************************************/

static char *DumpAsText(const std::shared_ptr<GDALGroup> &group,
                        const char *pszDriverName,
                        const GDALMultiDimInfoOptions *psOptions)
{
    GDALMultiDimTextOutputDumper dumper(psOptions);

    CPLStringList aosOptionsGetArray(psOptions->aosArrayOptions);
    if (psOptions->bDetailed)
        aosOptionsGetArray.SetNameValue("SHOW_ALL", "YES");

    const auto apoArrays =
        group->GetMDArraysRecursive(nullptr, aosOptionsGetArray.List());

    if (psOptions->osArrayName.empty())
    {
        dumper.AddLine(
            std::string("Driver: ")
                .append(pszDriverName ? pszDriverName : "(unknown)"));

        if (CSLConstList papszStructuralInfo = group->GetStructuralInfo())
        {
            dumper.DumpStructuralInfo(papszStructuralInfo, 0);
        }

        dumper.DumpArraysSummary(group, apoArrays);

        dumper.DrumpGroupsSummary(group);
    }

    if ((!psOptions->bSummary || !psOptions->osArrayName.empty()) &&
        !apoArrays.empty())
    {
        if (psOptions->osArrayName.empty())
        {
            dumper.DumpAttributes(group->GetAttributes(), 0);
            dumper.AddLine();
        }

        dumper.AddLine("Arrays:");
        for (const auto &poArray : apoArrays)
        {
            if (psOptions->osArrayName.empty() ||
                poArray->GetName() == psOptions->osArrayName ||
                poArray->GetFullName() == psOptions->osArrayName)
            {
                dumper.DumpArray(poArray);
            }
        }
    }

    if (psOptions->bStdoutOutput)
    {
        return VSIStrdup("ok");
    }
    else
    {
        return VSIStrdup(dumper.m_osOutput.c_str());
    }
}

/************************************************************************/
/*                           WriteToStdout()                            */
/************************************************************************/

static void WriteToStdout(const char *pszText, void *)
{
    printf("%s", pszText);
}

/************************************************************************/
/*                GDALMultiDimInfoAppOptionsGetParser()                 */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser> GDALMultiDimInfoAppOptionsGetParser(
    GDALMultiDimInfoOptions *psOptions,
    GDALMultiDimInfoOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdalmdiminfo", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(
        _("Lists various information about a GDAL multidimensional dataset."));

    argParser->add_epilog(_("For more details, consult "
                            "https://gdal.org/programs/gdalmdiminfo.html"));
    {
        auto &group = argParser->add_mutually_exclusive_group();

        group.add_argument("-summary")
            .flag()
            .store_into(psOptions->bSummary)
            .help(_("Report only group and array hierarchy, without detailed "
                    "information on attributes or dimensions."));

        group.add_argument("-detailed")
            .flag()
            .store_into(psOptions->bDetailed)
            .help(
                _("Most verbose output. Report attribute data types and array "
                  "values."));
    }

    argParser->add_inverted_logic_flag(
        "-nopretty", &psOptions->bPretty,
        _("Outputs on a single line without any indentation."));

    argParser->add_argument("-array")
        .metavar("<array_name>")
        .store_into(psOptions->osArrayName)
        .help(_("Name of the array, used to restrict the output to the "
                "specified array."));

    argParser->add_argument("-limit")
        .metavar("<number>")
        .scan<'i', int>()
        .store_into(psOptions->nLimitValuesByDim)
        .help(_("Number of values in each dimension that is used to limit the "
                "display of array values."));

    if (psOptionsForBinary)
    {
        argParser->add_open_options_argument(
            psOptionsForBinary->aosOpenOptions);

        argParser->add_input_format_argument(
            &psOptionsForBinary->aosAllowInputDrivers);

        argParser->add_argument("dataset_name")
            .metavar("<dataset_name>")
            .store_into(psOptionsForBinary->osFilename)
            .help("Input dataset.");
    }

    argParser->add_argument("-arrayoption")
        .metavar("<NAME>=<VALUE>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosArrayOptions.AddString(s.c_str()); })
        .help(_("Option passed to GDALGroup::GetMDArrayNames() to filter "
                "reported arrays."));

    argParser->add_argument("-stats")
        .flag()
        .store_into(psOptions->bStats)
        .help(_("Read and display image statistics."));

    argParser->add_argument("-format")
        .hidden()
        .store_into(psOptions->osFormat)
        .help(_("Output format."));

    // Only used by gdalmdiminfo binary to write output to stdout instead of in a string, in JSON mode
    argParser->add_argument("-stdout").flag().hidden().store_into(
        psOptions->bStdoutOutput);

    return argParser;
}

/************************************************************************/
/*                 GDALMultiDimInfoAppGetParserUsage()                  */
/************************************************************************/

std::string GDALMultiDimInfoAppGetParserUsage()
{
    try
    {
        GDALMultiDimInfoOptions sOptions;
        GDALMultiDimInfoOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALMultiDimInfoAppOptionsGetParser(&sOptions, &sOptionsForBinary);
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                          GDALMultiDimInfo()                          */
/************************************************************************/

/* clang-format off */
/**
 * Lists various information about a GDAL multidimensional dataset.
 *
 * This is the equivalent of the
 * <a href="/programs/gdalmdiminfo.html">gdalmdiminfo</a>utility.
 *
 * GDALMultiDimInfoOptions* must be allocated and freed with
 * GDALMultiDimInfoOptionsNew() and GDALMultiDimInfoOptionsFree() respectively.
 *
 * @param hDataset the dataset handle.
 * @param psOptionsIn the options structure returned by
 * GDALMultiDimInfoOptionsNew() or NULL.
 * @return string corresponding to the information about the raster dataset
 * (must be freed with CPLFree()), or NULL in case of error.
 *
 * @since GDAL 3.1
 */
/* clang-format on */

char *GDALMultiDimInfo(GDALDatasetH hDataset,
                       const GDALMultiDimInfoOptions *psOptionsIn)
{
    if (hDataset == nullptr)
        return nullptr;

    GDALMultiDimInfoOptions oOptionsDefault;
    const GDALMultiDimInfoOptions *psOptions =
        psOptionsIn ? psOptionsIn : &oOptionsDefault;
    CPLJSonStreamingWriter serializer(
        psOptions->bStdoutOutput ? WriteToStdout : nullptr, nullptr);
    serializer.SetPrettyFormatting(psOptions->bPretty);
    GDALDataset *poDS = GDALDataset::FromHandle(hDataset);
    auto group = poDS->GetRootGroup();
    if (!group)
        return nullptr;

    std::set<std::string> alreadyDumpedDimensions;
    std::set<std::string> alreadyDumpedArrays;

    try
    {
        const char *pszDriverName = nullptr;
        auto poDriver = poDS->GetDriver();
        if (poDriver)
            pszDriverName = poDriver->GetDescription();

        if (psOptions->osFormat == "text")
        {
            return DumpAsText(group, pszDriverName, psOptions);
        }
        else
        {
            if (psOptions->osArrayName.empty())
            {
                DumpGroup(group, group, pszDriverName, serializer, psOptions,
                          alreadyDumpedDimensions, alreadyDumpedArrays, true,
                          true);
            }
            else
            {
                auto curGroup = group;
                CPLStringList aosTokens(
                    CSLTokenizeString2(psOptions->osArrayName.c_str(), "/", 0));
                for (int i = 0; i < aosTokens.size() - 1; i++)
                {
                    auto curGroupNew = curGroup->OpenGroup(aosTokens[i]);
                    if (!curGroupNew)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot find group %s", aosTokens[i]);
                        return nullptr;
                    }
                    curGroup = std::move(curGroupNew);
                }
                const char *pszArrayName = aosTokens.back();
                auto array(curGroup->OpenMDArray(pszArrayName));
                if (!array)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot find array %s", pszArrayName);
                    return nullptr;
                }
                DumpArray(group, array, serializer, psOptions,
                          alreadyDumpedDimensions, alreadyDumpedArrays, true,
                          true, true);
            }

            if (psOptions->bStdoutOutput)
            {
                printf("\n");
                return VSIStrdup("ok");
            }
            else
            {
                return VSIStrdup(serializer.GetString().c_str());
            }
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                     GDALMultiDimInfoOptionsNew()                     */
/************************************************************************/

/**
 * Allocates a GDALMultiDimInfo struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdalmdiminfo.html">gdalmdiminfo</a> utility.
 * @param psOptionsForBinary should be nullptr, unless called from
 * gdalmultidiminfo_bin.cpp
 * @return pointer to the allocated GDALMultiDimInfoOptions struct. Must be
 * freed with GDALMultiDimInfoOptionsFree().
 *
 * @since GDAL 3.1
 */

GDALMultiDimInfoOptions *
GDALMultiDimInfoOptionsNew(char **papszArgv,
                           GDALMultiDimInfoOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALMultiDimInfoOptions>();

    /* -------------------------------------------------------------------- */
    /*      Parse arguments.                                                */
    /* -------------------------------------------------------------------- */

    CPLStringList aosArgv;

    if (papszArgv)
    {
        const int nArgc = CSLCount(papszArgv);
        for (int i = 0; i < nArgc; i++)
            aosArgv.AddString(papszArgv[i]);
    }

    try
    {
        auto argParser = GDALMultiDimInfoAppOptionsGetParser(
            psOptions.get(), psOptionsForBinary);
        argParser->parse_args_without_binary_name(aosArgv);
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return nullptr;
    }

    return psOptions.release();
}

/************************************************************************/
/*                    GDALMultiDimInfoOptionsFree()                     */
/************************************************************************/

/**
 * Frees the GDALMultiDimInfoOptions struct.
 *
 * @param psOptions the options struct for GDALMultiDimInfo().
 *
 * @since GDAL 3.1
 */

void GDALMultiDimInfoOptionsFree(GDALMultiDimInfoOptions *psOptions)
{
    delete psOptions;
}
