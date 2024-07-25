/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB multidimensional support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2023, TileDB, Inc
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

#include "tiledbmultidim.h"
#include "memmultidim.h"

/************************************************************************/
/*                TileDBAttribute::TileDBAttribute()                    */
/************************************************************************/

TileDBAttribute::TileDBAttribute(const std::string &osParentName,
                                 const std::string &osName)
    : GDALAbstractMDArray(osParentName, osName),
      GDALAttribute(osParentName, osName)
{
}

/************************************************************************/
/*                TileDBAttribute::Create()                             */
/************************************************************************/

/*static*/ std::shared_ptr<GDALAttribute>
TileDBAttribute::Create(const std::shared_ptr<TileDBAttributeHolder> &poParent,
                        const std::string &osName,
                        const std::vector<GUInt64> &anDimensions,
                        const GDALExtendedDataType &oDataType)
{
    if (anDimensions.size() > 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only 0 or 1-dimensional attribute are supported");
        return nullptr;
    }
    if (oDataType.GetClass() == GEDTC_STRING)
    {
        if (anDimensions.size() == 1 && anDimensions[0] != 1)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only single value string attribute are supported");
            return nullptr;
        }
    }
    else if (oDataType.GetClass() == GEDTC_COMPOUND)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Compound data type attribute are not supported");
        return nullptr;
    }

    auto poAttr = std::shared_ptr<TileDBAttribute>(
        new TileDBAttribute(poParent->IGetFullName(), osName));
    poAttr->m_poMemAttribute = MEMAttribute::Create(
        poParent->IGetFullName(), osName, anDimensions, oDataType);
    if (!poAttr->m_poMemAttribute)
        return nullptr;
    poAttr->m_poParent = poParent;
    return poAttr;
}

/************************************************************************/
/*                     TileDBAttribute::IRead()                         */
/************************************************************************/

bool TileDBAttribute::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                            const GInt64 *arrayStep,
                            const GPtrDiff_t *bufferStride,
                            const GDALExtendedDataType &bufferDataType,
                            void *pDstBuffer) const
{
    if (!CheckValidAndErrorOutIfNot())
        return false;
    auto poParent = m_poParent.lock();
    if (!poParent)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "TileDBAttribute::IRead() failed because owing array object"
                 "is no longer alive");
        return false;
    }
    if (GetDataType().GetClass() == GEDTC_STRING)
    {
        tiledb_datatype_t tiledb_dt;
        uint32_t nLen = 0;
        const void *ptr = nullptr;
        if (!poParent->GetMetadata(m_osName, &tiledb_dt, &nLen, &ptr))
            return false;
        if (tiledb_dt != TILEDB_STRING_UTF8 &&
            tiledb_dt != TILEDB_STRING_ASCII && tiledb_dt != TILEDB_UINT8)
            return false;
        std::string osStr;
        osStr.assign(static_cast<const char *>(ptr), nLen);
        if (!m_poMemAttribute->Write(osStr.c_str()))
            return false;
    }
    else
    {
        tiledb_datatype_t expected_tiledb_dt = TILEDB_ANY;
        if (!TileDBArray::GDALDataTypeToTileDB(
                GetDataType().GetNumericDataType(), expected_tiledb_dt))
            return false;
        tiledb_datatype_t tiledb_dt;
        uint32_t nLen = 0;
        const void *ptr = nullptr;
        if (!poParent->GetMetadata(m_osName, &tiledb_dt, &nLen, &ptr))
            return false;
        if (tiledb_dt != expected_tiledb_dt)
            return false;
        if (!m_poMemAttribute->Write(ptr, nLen * GetDataType().GetSize()))
            return false;
    }
    return m_poMemAttribute->Read(arrayStartIdx, count, arrayStep, bufferStride,
                                  bufferDataType, pDstBuffer);
}

/************************************************************************/
/*                     TileDBAttribute::IWrite()                        */
/************************************************************************/

bool TileDBAttribute::IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                             const GInt64 *arrayStep,
                             const GPtrDiff_t *bufferStride,
                             const GDALExtendedDataType &bufferDataType,
                             const void *pSrcBuffer)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;
    auto poParent = m_poParent.lock();
    if (!poParent)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "TileDBAttribute::IWrite() failed because owing array object"
                 "is no longer alive");
        return false;
    }
    if (!poParent->IIsWritable())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    if (!m_poMemAttribute->Write(arrayStartIdx, count, arrayStep, bufferStride,
                                 bufferDataType, pSrcBuffer))
    {
        return false;
    }

    if (GetDataType().GetClass() == GEDTC_STRING)
    {
        const char *pszStr = m_poMemAttribute->ReadAsString();
        if (pszStr)
        {
            const auto tiledb_dt = CPLIsASCII(pszStr, -1) ? TILEDB_STRING_ASCII
                                                          : TILEDB_STRING_UTF8;
            return poParent->PutMetadata(m_osName, tiledb_dt,
                                         static_cast<uint32_t>(strlen(pszStr)),
                                         pszStr);
        }
        return false;
    }

    tiledb_datatype_t tiledb_dt = TILEDB_ANY;
    if (!TileDBArray::GDALDataTypeToTileDB(GetDataType().GetNumericDataType(),
                                           tiledb_dt))
        return false;

    auto oRawResult = m_poMemAttribute->ReadAsRaw();
    if (!oRawResult.data())
        return false;
    const auto nValues =
        static_cast<uint32_t>(oRawResult.size() / GetDataType().GetSize());
    return poParent->PutMetadata(m_osName, tiledb_dt, nValues,
                                 oRawResult.data());
}
