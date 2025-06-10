/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB Support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2019, TileDB, Inc
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <cassert>
#include <cmath>
#include <cinttypes>
#include <limits>

#include "gdal_priv_templates.hpp"
#include "tiledbheaders.h"

// XML element inside _gdal XML metadata to store the number of overview levels
constexpr const char *OVERVIEW_COUNT_KEY = "tiledb:OverviewCount";

/************************************************************************/
/* ==================================================================== */
/*                            TileDBRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class TileDBRasterBand final : public GDALPamRasterBand
{
    friend class TileDBRasterDataset;

  protected:
    TileDBRasterDataset *poGDS;
    bool bStats;
    CPLString osAttrName;
    double m_dfNoData = 0;
    bool m_bNoDataSet = false;

  public:
    TileDBRasterBand(TileDBRasterDataset *, int,
                     const std::string &osAttr = TILEDB_VALUES);
    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, GSpacing, GSpacing,
                             GDALRasterIOExtraArg *psExtraArg) override;
    virtual GDALColorInterp GetColorInterpretation() override;

    double GetNoDataValue(int *pbHasNoData) override;
    CPLErr SetNoDataValue(double dfNoData) override;

    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int nIdx) override;
};

static CPLErr option_to_index_type(const char *pszIndexingType,
                                   TILEDB_INTERLEAVE_MODE &eMode)
{
    CPLErr eErr = CE_None;

    if (pszIndexingType)
    {
        if EQUAL (pszIndexingType, "BAND")
            eMode = BAND;
        else if EQUAL (pszIndexingType, "ATTRIBUTES")
            eMode = ATTRIBUTES;
        else if EQUAL (pszIndexingType, "PIXEL")
            eMode = PIXEL;
        else
        {
            eErr = CE_Failure;
            CPLError(eErr, CPLE_AppDefined,
                     "Unable to identify TileDB index mode %s.",
                     pszIndexingType);
        }
    }
    else
    {
        eMode = BAND;
    }

    return eErr;
}

static const char *index_type_name(TILEDB_INTERLEAVE_MODE eMode)
{
    switch (eMode)
    {
        case PIXEL:
            return "PIXEL";
        case ATTRIBUTES:
            return "ATTRIBUTES";
        case BAND:
            return "BAND";
        default:
            return nullptr;
    }
}

/************************************************************************/
/*                             SetBuffer()                              */
/************************************************************************/

static CPLErr SetBuffer(tiledb::Query *poQuery, GDALDataType eType,
                        const CPLString &osAttrName, void *pImage, size_t nSize)
{
    switch (eType)
    {
        case GDT_Byte:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<unsigned char *>(pImage), nSize);
            break;
        case GDT_Int8:
            poQuery->set_data_buffer(osAttrName,
                                     reinterpret_cast<int8_t *>(pImage), nSize);
            break;
        case GDT_UInt16:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<unsigned short *>(pImage), nSize);
            break;
        case GDT_UInt32:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<unsigned int *>(pImage), nSize);
            break;
        case GDT_UInt64:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<uint64_t *>(pImage), nSize);
            break;
        case GDT_Int16:
            poQuery->set_data_buffer(osAttrName,
                                     reinterpret_cast<short *>(pImage), nSize);
            break;
        case GDT_Int32:
            poQuery->set_data_buffer(osAttrName,
                                     reinterpret_cast<int *>(pImage), nSize);
            break;
        case GDT_Int64:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<int64_t *>(pImage), nSize);
            break;
        case GDT_Float32:
            poQuery->set_data_buffer(osAttrName,
                                     reinterpret_cast<float *>(pImage), nSize);
            break;
        case GDT_Float64:
            poQuery->set_data_buffer(osAttrName,
                                     reinterpret_cast<double *>(pImage), nSize);
            break;
        case GDT_CInt16:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<short *>(pImage), nSize * 2);
            break;
        case GDT_CInt32:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<int *>(pImage), nSize * 2);
            break;
        case GDT_CFloat32:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<float *>(pImage), nSize * 2);
            break;
        case GDT_CFloat64:
            poQuery->set_data_buffer(
                osAttrName, reinterpret_cast<double *>(pImage), nSize * 2);
            break;
        default:
            return CE_Failure;
    }
    return CE_None;
}

/************************************************************************/
/*                          TileDBRasterBand()                          */
/************************************************************************/

TileDBRasterBand::TileDBRasterBand(TileDBRasterDataset *poDSIn, int nBandIn,
                                   const std::string &osAttr)
    : poGDS(poDSIn), bStats(poDSIn->bStats), osAttrName(osAttr)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = poGDS->eDataType;
    if (eDataType == GDT_Unknown)
    {
        try
        {
            auto attr = (poGDS->m_roArray ? poGDS->m_roArray : poGDS->m_array)
                            ->schema()
                            .attribute(osAttr);
            switch (attr.type())
            {
                case TILEDB_INT8:
                    eDataType = GDT_Int8;
                    break;
                case TILEDB_UINT8:
                    eDataType = GDT_Byte;
                    break;
                case TILEDB_INT16:
                    eDataType =
                        attr.cell_val_num() == 2 ? GDT_CInt16 : GDT_Int16;
                    break;
                case TILEDB_UINT16:
                    eDataType = GDT_UInt16;
                    break;
                case TILEDB_INT32:
                    eDataType =
                        attr.cell_val_num() == 2 ? GDT_CInt32 : GDT_Int32;
                    break;
                case TILEDB_UINT32:
                    eDataType = GDT_UInt32;
                    break;
                case TILEDB_INT64:
                    eDataType = GDT_Int64;
                    break;
                case TILEDB_UINT64:
                    eDataType = GDT_UInt64;
                    break;
                case TILEDB_FLOAT32:
                    eDataType =
                        attr.cell_val_num() == 2 ? GDT_CFloat32 : GDT_Float32;
                    break;
                case TILEDB_FLOAT64:
                    eDataType =
                        attr.cell_val_num() == 2 ? GDT_CFloat64 : GDT_Float64;
                    break;
                default:
                {
                    const char *pszTypeName = "";
                    tiledb_datatype_to_str(attr.type(), &pszTypeName);
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Unhandled TileDB data type: %s", pszTypeName);
                    break;
                }
            }
        }
        catch (const tiledb::TileDBError &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        }
    }
    eAccess = poGDS->eAccess;
    nRasterXSize = poGDS->nRasterXSize;
    nRasterYSize = poGDS->nRasterYSize;
    nBlockXSize = poGDS->nBlockXSize;
    nBlockYSize = poGDS->nBlockYSize;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr TileDBRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                   int nXSize, int nYSize, void *pData,
                                   int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType, GSpacing nPixelSpace,
                                   GSpacing nLineSpace,
                                   GDALRasterIOExtraArg *psExtraArg)
{
    if (!poGDS->m_bDeferredCreateHasRun)
        poGDS->DeferredCreate(/* bCreateArray = */ true);
    if (!poGDS->m_bDeferredCreateHasBeenSuccessful)
        return CE_Failure;
    if (!poGDS->m_array)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset has been closed");
        return CE_Failure;
    }

    if (poGDS->eIndexMode == ATTRIBUTES && eRWFlag == GF_Write)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Unable to write using band ordered IRasterIO when using "
                 "interleave 'ATTRIBUTES'.\n");
        return CE_Failure;
    }

    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));

    if (eBufType == eDataType && nXSize == nBufXSize && nYSize == nBufYSize &&
        nBufferDTSize > 0 && nPixelSpace == nBufferDTSize &&
        nLineSpace == nBufXSize * nPixelSpace)
    {
        const uint64_t nBandIdx = poGDS->nBandStart + nBand - 1;
        std::vector<uint64_t> oaSubarray = {
            nBandIdx,        nBandIdx,
            (uint64_t)nYOff, (uint64_t)nYOff + nYSize - 1,
            (uint64_t)nXOff, (uint64_t)nXOff + nXSize - 1};
        if (poGDS->eIndexMode == PIXEL)
            std::rotate(oaSubarray.begin(), oaSubarray.begin() + 2,
                        oaSubarray.end());

        try
        {
            tiledb::Context *ctx = poGDS->m_ctx.get();
            const auto &oArray = poGDS->GetArray(eRWFlag == GF_Write, ctx);

            auto poQuery = std::make_unique<tiledb::Query>(*ctx, oArray);
            tiledb::Subarray subarray(*ctx, oArray);
            if (poGDS->m_array->schema().domain().ndim() == 3)
            {
                subarray.set_subarray(oaSubarray);
            }
            else
            {
                subarray.set_subarray(std::vector<uint64_t>(
                    oaSubarray.cbegin() + 2, oaSubarray.cend()));
            }
            poQuery->set_subarray(subarray);

            const size_t nValues = static_cast<size_t>(nBufXSize) * nBufYSize;
            SetBuffer(poQuery.get(), eDataType, osAttrName, pData, nValues);

            // write additional co-registered values
            std::vector<std::unique_ptr<void, decltype(&VSIFree)>> aBlocks;

            if (poGDS->m_lpoAttributeDS.size() > 0)
            {
                const int nXSizeToRead = nXOff + nXSize > nRasterXSize
                                             ? nRasterXSize - nXOff
                                             : nXSize;
                const int nYSizeToRead = nYOff + nYSize > nRasterYSize
                                             ? nRasterYSize - nYOff
                                             : nYSize;
                for (auto const &poAttrDS : poGDS->m_lpoAttributeDS)
                {
                    GDALRasterBand *poAttrBand = poAttrDS->GetRasterBand(nBand);
                    GDALDataType eAttrType = poAttrBand->GetRasterDataType();
                    int nBytes = GDALGetDataTypeSizeBytes(eAttrType);
                    void *pAttrBlock = VSI_MALLOC_VERBOSE(nBytes * nValues);

                    if (pAttrBlock == nullptr)
                    {
                        CPLError(CE_Failure, CPLE_OutOfMemory,
                                 "Cannot allocate attribute buffer");
                        return CE_Failure;
                    }
                    aBlocks.emplace_back(pAttrBlock, &VSIFree);

                    poAttrBand->AdviseRead(nXOff, nYOff, nXSizeToRead,
                                           nYSizeToRead, nXSizeToRead,
                                           nYSizeToRead, eAttrType, nullptr);

                    CPLErr eErr = poAttrBand->RasterIO(
                        GF_Read, nXOff, nYOff, nXSizeToRead, nYSizeToRead,
                        pAttrBlock, nXSizeToRead, nYSizeToRead, eAttrType,
                        nPixelSpace, nLineSpace, psExtraArg);

                    if (eErr == CE_None)
                    {
                        CPLString osName =
                            CPLGetBasenameSafe(poAttrDS->GetDescription());

                        SetBuffer(poQuery.get(), eAttrType, osName, pAttrBlock,
                                  nValues);
                    }
                    else
                    {
                        return eErr;
                    }
                }
            }

            if (bStats)
                tiledb::Stats::enable();

            auto status = poQuery->submit();

            if (bStats)
            {
                tiledb::Stats::dump(stdout);
                tiledb::Stats::disable();
            }

            if (status == tiledb::Query::Status::FAILED)
                return CE_Failure;
            else
                return CE_None;
        }
        catch (const tiledb::TileDBError &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "TileDB: TileDBRasterBand::IRasterIO() failed: %s",
                     e.what());
            return CE_Failure;
        }
    }

    return GDALPamRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace, psExtraArg);
}

CPLErr TileDBRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                    void *pImage)
{
    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    return IRasterIO(GF_Read, nXOff, nYOff, nBlockXSize, nBlockYSize, pImage,
                     nBlockXSize, nBlockYSize, eDataType, nDTSize,
                     nDTSize * nBlockXSize, nullptr);
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr TileDBRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                     void *pImage)

{
    if (eAccess == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Unable to write block, dataset is opened read only.\n");
        return CE_Failure;
    }

    CPLAssert(poGDS != nullptr && nBlockXOff >= 0 && nBlockYOff >= 0 &&
              pImage != nullptr);

    int nStartX = nBlockXSize * nBlockXOff;
    int nStartY = nBlockYSize * nBlockYOff;

    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    return IRasterIO(GF_Write, nStartX, nStartY, nBlockXSize, nBlockYSize,
                     pImage, nBlockXSize, nBlockYSize, eDataType, nDTSize,
                     nDTSize * nBlockXSize, nullptr);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp TileDBRasterBand::GetColorInterpretation()

{
    if (poGDS->nBands == 1)
        return GCI_GrayIndex;

    if (nBand == 1)
        return GCI_RedBand;

    else if (nBand == 2)
        return GCI_GreenBand;

    else if (nBand == 3)
        return GCI_BlueBand;

    return GCI_AlphaBand;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double TileDBRasterBand::GetNoDataValue(int *pbHasNoData)
{
    if (pbHasNoData)
        *pbHasNoData = false;
    if (m_bNoDataSet)
    {
        if (pbHasNoData)
            *pbHasNoData = true;
        return m_dfNoData;
    }
    if (!poGDS->m_bDeferredCreateHasBeenSuccessful)
        return 0.0;
    if (!poGDS->m_array)
        return 0.0;
    double dfNoData = 0.0;
    try
    {
        const void *value = nullptr;
        uint64_t size = 0;
        // Caution: 2 below statements must not be combined in a single one,
        // as the lifetime of value is linked to the return value of
        // attribute()
        auto attr = (poGDS->m_roArray ? poGDS->m_roArray : poGDS->m_array)
                        ->schema()
                        .attribute(osAttrName);
        attr.get_fill_value(&value, &size);
        if (value &&
            size == static_cast<uint64_t>(GDALGetDataTypeSizeBytes(eDataType)))
        {
            switch (eDataType)
            {
                case GDT_Byte:
                    dfNoData = *static_cast<const uint8_t *>(value);
                    break;
                case GDT_Int8:
                    dfNoData = *static_cast<const int8_t *>(value);
                    break;
                case GDT_UInt16:
                    dfNoData = *static_cast<const uint16_t *>(value);
                    break;
                case GDT_Int16:
                case GDT_CInt16:
                    dfNoData = *static_cast<const int16_t *>(value);
                    break;
                case GDT_UInt32:
                    dfNoData = *static_cast<const uint32_t *>(value);
                    break;
                case GDT_Int32:
                case GDT_CInt32:
                    dfNoData = *static_cast<const int32_t *>(value);
                    break;
                case GDT_UInt64:
                    dfNoData = static_cast<double>(
                        *static_cast<const uint64_t *>(value));
                    break;
                case GDT_Int64:
                    dfNoData = static_cast<double>(
                        *static_cast<const int64_t *>(value));
                    break;
                case GDT_Float16:
                case GDT_CFloat16:
                    // tileDB does not support float16
                    CPLAssert(false);
                    break;
                case GDT_Float32:
                case GDT_CFloat32:
                    dfNoData = *static_cast<const float *>(value);
                    break;
                case GDT_Float64:
                case GDT_CFloat64:
                    dfNoData = *static_cast<const double *>(value);
                    break;
                case GDT_Unknown:
                case GDT_TypeCount:
                    break;
            }
            if (pbHasNoData)
                *pbHasNoData = true;
        }
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
    }
    return dfNoData;
}

/************************************************************************/
/*                           IsValidNoData()                            */
/************************************************************************/

template <class T> static bool IsValidNoData(double dfNoData)
{
    return GDALIsValueInRange<T>(dfNoData) &&
           dfNoData == static_cast<double>(static_cast<T>(dfNoData));
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr TileDBRasterBand::SetNoDataValue(double dfNoData)
{
    if (poGDS->m_bDeferredCreateHasBeenSuccessful)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "TileDBRasterBand::SetNoDataValue(): cannot be called after "
                 "pixel values have been set");
        return CE_Failure;
    }

    if (nBand != 1 &&
        dfNoData !=
            cpl::down_cast<TileDBRasterBand *>(poGDS->papoBands[0])->m_dfNoData)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "TileDBRasterBand::SetNoDataValue(): all bands should have "
                 "the same nodata value");
        return CE_Failure;
    }

    bool bIsValid = false;
    switch (eDataType)
    {
        case GDT_Byte:
            bIsValid = IsValidNoData<uint8_t>(dfNoData);
            break;
        case GDT_Int8:
            bIsValid = IsValidNoData<int8_t>(dfNoData);
            break;
        case GDT_UInt16:
            bIsValid = IsValidNoData<uint16_t>(dfNoData);
            break;
        case GDT_Int16:
        case GDT_CInt16:
            bIsValid = IsValidNoData<int16_t>(dfNoData);
            break;
        case GDT_UInt32:
            bIsValid = IsValidNoData<uint32_t>(dfNoData);
            break;
        case GDT_Int32:
        case GDT_CInt32:
            bIsValid = IsValidNoData<int32_t>(dfNoData);
            break;
        case GDT_UInt64:
            bIsValid = IsValidNoData<uint64_t>(dfNoData);
            break;
        case GDT_Int64:
            bIsValid = IsValidNoData<int64_t>(dfNoData);
            break;
        case GDT_Float16:
        case GDT_CFloat16:
            // tileDB does not support float16
            bIsValid = CPLIsNan(dfNoData) || IsValidNoData<float>(dfNoData);
            break;
        case GDT_Float32:
        case GDT_CFloat32:
            bIsValid = CPLIsNan(dfNoData) || IsValidNoData<float>(dfNoData);
            break;
        case GDT_Float64:
        case GDT_CFloat64:
            bIsValid = true;
            break;
        case GDT_Unknown:
        case GDT_TypeCount:
            break;
    }
    if (!bIsValid)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "TileDBRasterBand::SetNoDataValue(): nodata value cannot be "
                 "stored in band data type");
        return CE_Failure;
    }

    m_dfNoData = dfNoData;
    m_bNoDataSet = true;
    return CE_None;
}

/************************************************************************/
/*                           GetOverviewCount()                         */
/************************************************************************/

int TileDBRasterBand::GetOverviewCount()
{
    if (poGDS->m_nOverviewCountFromMetadata > 0)
    {
        poGDS->LoadOverviews();
        return static_cast<int>(poGDS->m_apoOverviewDS.size());
    }
    return GDALPamRasterBand::GetOverviewCount();
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand *TileDBRasterBand::GetOverview(int nIdx)
{
    if (poGDS->m_nOverviewCountFromMetadata > 0)
    {
        poGDS->LoadOverviews();
        if (nIdx >= 0 && nIdx < static_cast<int>(poGDS->m_apoOverviewDS.size()))
        {
            return poGDS->m_apoOverviewDS[nIdx]->GetRasterBand(nBand);
        }
        return nullptr;
    }
    return GDALPamRasterBand::GetOverview(nIdx);
}

/************************************************************************/
/* ==================================================================== */
/*                     TileDBRasterDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                     ~TileDBRasterDataset()                           */
/************************************************************************/

TileDBRasterDataset::~TileDBRasterDataset()

{
    TileDBRasterDataset::CloseDependentDatasets();
    TileDBRasterDataset::Close();
}

/************************************************************************/
/*                              GetArray()                              */
/************************************************************************/

tiledb::Array &TileDBRasterDataset::GetArray(bool bForWrite,
                                             tiledb::Context *&ctx)
{
    if (eAccess == GA_Update && !bForWrite)
    {
        if (!m_roArray)
        {
            m_roCtx = std::make_unique<tiledb::Context>(m_ctx->config());
            if (nTimestamp)
            {
                m_roArray = std::make_unique<tiledb::Array>(
                    *m_roCtx, m_osArrayURI, TILEDB_READ,
                    tiledb::TemporalPolicy(tiledb::TimeTravel, nTimestamp));
            }
            else
            {
                m_roArray = std::make_unique<tiledb::Array>(
                    *m_roCtx, m_osArrayURI, TILEDB_READ);
            }
        }

        ctx = m_roCtx.get();
        return *(m_roArray.get());
    }

    ctx = m_ctx.get();
    return *(m_array.get());
}

/************************************************************************/
/*                           IRasterio()                                */
/************************************************************************/

CPLErr TileDBRasterDataset::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
    GSpacing nLineSpace, GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg)

{
    if (!m_bDeferredCreateHasRun)
        DeferredCreate(/* bCreateArray = */ true);
    if (!m_bDeferredCreateHasBeenSuccessful)
        return CE_Failure;
    if (!m_array)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset has been closed");
        return CE_Failure;
    }

    // support special case of writing attributes for bands, all attributes have to be set at once
    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));

    if (eIndexMode == ATTRIBUTES && nBandCount == nBands &&
        eBufType == eDataType && nXSize == nBufXSize && nYSize == nBufYSize &&
        nBufferDTSize > 0 && nPixelSpace == nBufferDTSize &&
        nLineSpace == nBufXSize * nPixelSpace &&
        ((nBandSpace == 0 && nBandCount == 1) ||
         nBandSpace == nBufYSize * nLineSpace))
    {
        std::vector<uint64_t> oaSubarray = {
            (uint64_t)nYOff, (uint64_t)nYOff + nYSize - 1, (uint64_t)nXOff,
            (uint64_t)nXOff + nXSize - 1};

        try
        {
            tiledb::Context *ctx = m_ctx.get();
            const auto &oArray = GetArray(eRWFlag == GF_Write, ctx);

            auto poQuery = std::make_unique<tiledb::Query>(*ctx, oArray);
            tiledb::Subarray subarray(*ctx, oArray);
            subarray.set_subarray(oaSubarray);
            poQuery->set_subarray(subarray);

            for (int b = 0; b < nBandCount; b++)
            {
                TileDBRasterBand *poBand = cpl::down_cast<TileDBRasterBand *>(
                    GetRasterBand(panBandMap[b]));
                const size_t nRegionSize =
                    static_cast<size_t>(nBufXSize) * nBufYSize * nBufferDTSize;
                SetBuffer(poQuery.get(), eDataType, poBand->osAttrName,
                          static_cast<GByte *>(pData) + b * nRegionSize,
                          nRegionSize);
            }

            if (bStats)
                tiledb::Stats::enable();

            auto status = poQuery->submit();

            if (bStats)
            {
                tiledb::Stats::dump(stdout);
                tiledb::Stats::disable();
            }

            if (status == tiledb::Query::Status::FAILED)
                return CE_Failure;
            else
                return CE_None;
        }
        catch (const tiledb::TileDBError &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "TileDB: TileDBRasterDataset::IRasterIO() failed: %s",
                     e.what());
            return CE_Failure;
        }
    }

    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nBandCount, panBandMap, nPixelSpace,
                                     nLineSpace, nBandSpace, psExtraArg);
}

/************************************************************************/
/*                             AddDimensions()                          */
/************************************************************************/

CPLErr TileDBRasterDataset::AddDimensions(tiledb::Domain &domain,
                                          const char *pszAttrName,
                                          tiledb::Dimension &y,
                                          tiledb::Dimension &x,
                                          tiledb::Dimension *poBands)

{
    CPLErr eErr = CE_None;

    const bool bHasFillValue =
        nBands ? cpl::down_cast<TileDBRasterBand *>(papoBands[0])->m_bNoDataSet
               : false;
    const double dfFillValue =
        nBands ? cpl::down_cast<TileDBRasterBand *>(papoBands[0])->m_dfNoData
               : 0.0;

    switch (eIndexMode)
    {
        case ATTRIBUTES:
            domain.add_dimensions(y, x);
            eErr = CreateAttribute(eDataType, pszAttrName, nBands,
                                   bHasFillValue, dfFillValue);
            break;
        case PIXEL:
            assert(poBands);
            domain.add_dimensions(y, x, *poBands);
            eErr = CreateAttribute(eDataType, pszAttrName, 1, bHasFillValue,
                                   dfFillValue);
            break;
        default:  // BAND
            assert(poBands);
            domain.add_dimensions(*poBands, y, x);
            eErr = CreateAttribute(eDataType, pszAttrName, 1, bHasFillValue,
                                   dfFillValue);
            break;
    }

    return eErr;
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

CPLErr TileDBRasterDataset::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (TileDBRasterDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        if (!m_bDeferredCreateHasRun)
            DeferredCreate(/* bCreateArray = */ true);

        if (nPamFlags & GPF_DIRTY)
            TrySaveXML();

        try
        {
            if (m_array)
            {
                m_array->close();
                m_array.reset();
            }
        }
        catch (const tiledb::TileDBError &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        }
        try
        {
            if (m_roArray)
            {
                m_roArray->close();
                m_roArray.reset();
            }
        }
        catch (const tiledb::TileDBError &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        }

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

int TileDBRasterDataset::CloseDependentDatasets()
{
    int bDroppedRef = GDALPamDataset::CloseDependentDatasets();
    if (!m_apoOverviewDS.empty())
        bDroppedRef = TRUE;
    m_apoOverviewDS.clear();
    return bDroppedRef;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr TileDBRasterDataset::FlushCache(bool bAtClosing)

{
    return BlockBasedFlushCache(bAtClosing);
}

/************************************************************************/
/*                           TrySaveXML()                               */
/************************************************************************/

CPLErr TileDBRasterDataset::TrySaveXML()

{
    if (m_array == nullptr)
        return CE_None;

    CPLXMLNode *psTree = nullptr;
    try
    {
        nPamFlags &= ~GPF_DIRTY;

        if (psPam == nullptr || (nPamFlags & GPF_NOSAVE))
            return CE_None;

        /* --------------------------------------------------------------------
         */
        /*      Make sure we know the filename we want to store in. */
        /* --------------------------------------------------------------------
         */
        if (!BuildPamFilename())
            return CE_None;

        /* --------------------------------------------------------------------
         */
        /*      Build the XML representation of the auxiliary metadata. */
        /* --------------------------------------------------------------------
         */
        psTree = SerializeToXML(nullptr);

        if (psTree == nullptr)
        {
            /* If we have unset all metadata, we have to delete the PAM file */
            m_array->delete_metadata(GDAL_ATTRIBUTE_NAME);

            if (m_bDatasetInGroup)
            {
                tiledb::Group group(*m_ctx, GetDescription(), TILEDB_WRITE);
                group.delete_metadata(GDAL_ATTRIBUTE_NAME);
            }

            return CE_None;
        }

        if (m_poSubDatasetsTree)
        {
            CPLAddXMLChild(psTree,
                           CPLCloneXMLTree(m_poSubDatasetsTree->psChild));
        }

        /* --------------------------------------------------------------------
         */
        /*      If we are working with a subdataset, we need to integrate */
        /*      the subdataset tree within the whole existing pam tree, */
        /*      after removing any old version of the same subdataset. */
        /* --------------------------------------------------------------------
         */
        if (!psPam->osSubdatasetName.empty())
        {
            CPLXMLNode *psOldTree, *psSubTree;

            CPLErrorReset();
            {
                CPLErrorHandlerPusher oQuietError(CPLQuietErrorHandler);
                psOldTree = CPLParseXMLFile(psPam->pszPamFilename);
            }

            if (psOldTree == nullptr)
                psOldTree =
                    CPLCreateXMLNode(nullptr, CXT_Element, "PAMDataset");

            for (psSubTree = psOldTree->psChild; psSubTree != nullptr;
                 psSubTree = psSubTree->psNext)
            {
                if (psSubTree->eType != CXT_Element ||
                    !EQUAL(psSubTree->pszValue, "Subdataset"))
                    continue;

                if (!EQUAL(CPLGetXMLValue(psSubTree, "name", ""),
                           psPam->osSubdatasetName))
                    continue;

                break;
            }

            if (psSubTree == nullptr)
            {
                psSubTree =
                    CPLCreateXMLNode(psOldTree, CXT_Element, "Subdataset");
                CPLCreateXMLNode(
                    CPLCreateXMLNode(psSubTree, CXT_Attribute, "name"),
                    CXT_Text, psPam->osSubdatasetName);
            }

            CPLXMLNode *psOldPamDataset =
                CPLGetXMLNode(psSubTree, "PAMDataset");
            if (psOldPamDataset != nullptr)
            {
                CPLRemoveXMLChild(psSubTree, psOldPamDataset);
                CPLDestroyXMLNode(psOldPamDataset);
            }

            CPLAddXMLChild(psSubTree, psTree);
            psTree = psOldTree;
        }

        if (m_nOverviewCountFromMetadata)
        {
            CPLCreateXMLElementAndValue(
                psTree, OVERVIEW_COUNT_KEY,
                CPLSPrintf("%d", m_nOverviewCountFromMetadata));
        }

        /* --------------------------------------------------------------------
         */
        /*      Try saving the auxiliary metadata. */
        /* --------------------------------------------------------------------
         */
        CPLErrorHandlerPusher oQuietError(CPLQuietErrorHandler);
        char *pszTree = CPLSerializeXMLTree(psTree);

        std::unique_ptr<tiledb::Array> poArrayToClose;
        tiledb::Array *poArray = m_array.get();
        if (eAccess == GA_ReadOnly)
        {
            if (nTimestamp)
            {
                poArrayToClose = std::make_unique<tiledb::Array>(
                    *m_ctx, m_array->uri(), TILEDB_WRITE,
                    tiledb::TemporalPolicy(tiledb::TimeTravel, nTimestamp));
            }
            else
            {
                poArrayToClose = std::make_unique<tiledb::Array>(
                    *m_ctx, m_array->uri(), TILEDB_WRITE);
            }
            poArray = poArrayToClose.get();
        }

        poArray->put_metadata(DATASET_TYPE_ATTRIBUTE_NAME, TILEDB_STRING_UTF8,
                              static_cast<int>(strlen(RASTER_DATASET_TYPE)),
                              RASTER_DATASET_TYPE);
        poArray->put_metadata(GDAL_ATTRIBUTE_NAME, TILEDB_UINT8,
                              static_cast<int>(strlen(pszTree)), pszTree);

        if (poArrayToClose)
            poArrayToClose->close();

        if (m_bDatasetInGroup)
        {
            tiledb::Group group(*m_ctx, GetDescription(), TILEDB_WRITE);
            group.put_metadata(GDAL_ATTRIBUTE_NAME, TILEDB_STRING_UTF8,
                               static_cast<int>(strlen(pszTree)), pszTree);
            group.close();
        }

        CPLFree(pszTree);

        /* --------------------------------------------------------------------
         */
        /*      Cleanup */
        /* --------------------------------------------------------------------
         */
        if (psTree)
            CPLDestroyXMLNode(psTree);

        return CE_None;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        if (psTree)
            CPLDestroyXMLNode(psTree);
        return CE_Failure;
    }
}

/************************************************************************/
/*                           TryLoadXML()                               */
/************************************************************************/

CPLErr TileDBRasterDataset::TryLoadXML(CSLConstList papszSiblingFiles)

{
    return TryLoadCachedXML(papszSiblingFiles, true);
}

/************************************************************************/
/*                           TryLoadCachedXML()                               */
/************************************************************************/

CPLErr TileDBRasterDataset::TryLoadCachedXML(CSLConstList /*papszSiblingFiles*/,
                                             bool bReload)

{
    CPLXMLNode *psTree = nullptr;
    try
    {
        PamInitialize();
        tiledb::VFS vfs(*m_ctx, m_ctx->config());

        /* --------------------------------------------------------------------
         */
        /*      Clear dirty flag.  Generally when we get to this point is */
        /*      from a call at the end of the Open() method, and some calls */
        /*      may have already marked the PAM info as dirty (for instance */
        /*      setting metadata), but really everything to this point is */
        /*      reproducible, and so the PAM info should not really be */
        /*      thought of as dirty. */
        /* --------------------------------------------------------------------
         */
        nPamFlags &= ~GPF_DIRTY;

        /* --------------------------------------------------------------------
         */
        /*      Try reading the file. */
        /* --------------------------------------------------------------------
         */
        if (!BuildPamFilename())
            return CE_None;

        /* --------------------------------------------------------------------
         */
        /*      In case the PAM filename is a .aux.xml file next to the */
        /*      physical file and we have a siblings list, then we can skip */
        /*      stat'ing the filesystem. */
        /* --------------------------------------------------------------------
         */
        CPLErr eLastErr = CPLGetLastErrorType();
        int nLastErrNo = CPLGetLastErrorNo();
        CPLString osLastErrorMsg = CPLGetLastErrorMsg();

        CPLErrorReset();
        {
            CPLErrorHandlerPusher oQuietError(CPLQuietErrorHandler);

            if (bReload)
            {
                tiledb_datatype_t v_type =
                    TILEDB_UINT8;  // CPLSerializeXMLTree returns char*
                const void *v_r = nullptr;
                uint32_t v_num = 0;
                auto &oArray = ((eAccess == GA_Update) && (m_roArray))
                                   ? m_roArray
                                   : m_array;
                oArray->get_metadata(GDAL_ATTRIBUTE_NAME, &v_type, &v_num,
                                     &v_r);
                if (v_r && (v_type == TILEDB_UINT8 || v_type == TILEDB_CHAR ||
                            v_type == TILEDB_STRING_ASCII ||
                            v_type == TILEDB_STRING_UTF8))
                {
                    osMetaDoc =
                        CPLString(static_cast<const char *>(v_r), v_num);
                }
                psTree = CPLParseXMLString(osMetaDoc);
            }

            if (bReload && psTree == nullptr &&
                vfs.is_file(psPam->pszPamFilename))
            {
                auto nBytes = vfs.file_size(psPam->pszPamFilename);
                tiledb::VFS::filebuf fbuf(vfs);
                fbuf.open(psPam->pszPamFilename, std::ios::in);
                std::istream is(&fbuf);
                osMetaDoc.resize(nBytes);
                is.read((char *)osMetaDoc.data(), nBytes);
                fbuf.close();
                psTree = CPLParseXMLString(osMetaDoc);
            }

            if (!bReload)
            {
                psTree = CPLParseXMLString(osMetaDoc);
            }
        }
        CPLErrorReset();

        if (eLastErr != CE_None)
            CPLErrorSetState(eLastErr, nLastErrNo, osLastErrorMsg.c_str());

        /* --------------------------------------------------------------------
         */
        /*      If we are looking for a subdataset, search for its subtree not.
         */
        /* --------------------------------------------------------------------
         */
        if (psTree && !psPam->osSubdatasetName.empty())
        {
            CPLXMLNode *psSubTree = psTree->psChild;

            for (; psSubTree != nullptr; psSubTree = psSubTree->psNext)
            {
                if (psSubTree->eType != CXT_Element ||
                    !EQUAL(psSubTree->pszValue, "Subdataset"))
                    continue;

                if (!EQUAL(CPLGetXMLValue(psSubTree, "name", ""),
                           psPam->osSubdatasetName))
                    continue;

                psSubTree = CPLGetXMLNode(psSubTree, "PAMDataset");
                break;
            }

            if (psSubTree != nullptr)
                psSubTree = CPLCloneXMLTree(psSubTree);

            CPLDestroyXMLNode(psTree);
            psTree = psSubTree;
        }
        if (psTree == nullptr)
            return CE_Failure;

        m_nOverviewCountFromMetadata =
            atoi(CPLGetXMLValue(psTree, OVERVIEW_COUNT_KEY, "0"));
        if (m_nOverviewCountFromMetadata)
        {
            CPLDebugOnly("TileDB", "OverviewCount = %d",
                         m_nOverviewCountFromMetadata);
        }

        /* --------------------------------------------------------------------
         */
        /*      Initialize ourselves from this XML tree. */
        /* --------------------------------------------------------------------
         */

        CPLString osPath(CPLGetPathSafe(psPam->pszPamFilename));
        const CPLErr eErr = XMLInit(psTree, osPath);

        CPLDestroyXMLNode(psTree);

        if (eErr != CE_None)
            PamClear();

        return eErr;
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        if (psTree)
            CPLDestroyXMLNode(psTree);
        return CE_Failure;
    }
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char **TileDBRasterDataset::GetMetadata(const char *pszDomain)

{
    if (pszDomain != nullptr && EQUAL(pszDomain, "SUBDATASETS"))
    {
        if (m_aosSubdatasetMD.empty())
        {
            CSLConstList papszMD = GDALPamDataset::GetMetadata(pszDomain);
            for (const auto &[pszKey, pszValue] :
                 cpl::IterateNameValue(papszMD))
            {
                if (STARTS_WITH(pszKey, "SUBDATASET_") &&
                    EQUAL(pszKey + strlen(pszKey) - strlen("_NAME"), "_NAME") &&
                    !STARTS_WITH(pszValue, "TILEDB:"))
                {
                    m_aosSubdatasetMD.AddNameValue(
                        pszKey, CPLString().Printf("TILEDB:\"%s\":%s",
                                                   GetDescription(), pszValue));
                }
                else
                {
                    m_aosSubdatasetMD.AddNameValue(pszKey, pszValue);
                }
            }
        }
        return m_aosSubdatasetMD.List();
    }
    else
    {
        return GDALPamDataset::GetMetadata(pszDomain);
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *TileDBRasterDataset::Open(GDALOpenInfo *poOpenInfo,
                                       tiledb::Object::Type objectType)

{
    try
    {
        return OpenInternal(poOpenInfo, objectType);
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "TileDB: Open(%s) failed: %s",
                 poOpenInfo->pszFilename, e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                             OpenInternal()                           */
/************************************************************************/

GDALDataset *TileDBRasterDataset::OpenInternal(GDALOpenInfo *poOpenInfo,
                                               tiledb::Object::Type objectType)

{
    auto poDS = std::make_unique<TileDBRasterDataset>();

    poDS->m_bDeferredCreateHasRun = true;
    poDS->m_bDeferredCreateHasBeenSuccessful = true;

    const char *pszConfig =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_CONFIG");

    const char *pszTimestamp =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_TIMESTAMP");

    poDS->bStats =
        CSLFetchBoolean(poOpenInfo->papszOpenOptions, "STATS", FALSE);

    if (pszConfig != nullptr)
    {
        poDS->m_osConfigFilename = pszConfig;
        tiledb::Config cfg(pszConfig);
        poDS->m_ctx.reset(new tiledb::Context(cfg));
    }
    else
    {
        poDS->m_ctx.reset(new tiledb::Context());
    }
    if (pszTimestamp)
        poDS->nTimestamp = std::strtoull(pszTimestamp, nullptr, 10);

    CPLString osURI;
    CPLString osSubdataset;

    std::string osAttrNameTmp;
    const char *pszAttr =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_ATTRIBUTE");

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "TILEDB://"))
    {
        // form required read attributes and open file
        // Create a corresponding GDALDataset.
        CPLStringList apszName(
            CSLTokenizeString2(poOpenInfo->pszFilename, ":",
                               CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES));

        if (apszName.size() != 3)
        {
            return nullptr;
        }

        osURI = TileDBDataset::VSI_to_tiledb_uri(apszName[1]);
        osSubdataset = apszName[2];
        poDS->SetSubdatasetName(osSubdataset.c_str());
    }
    else
    {
        if (pszAttr != nullptr)
        {
            poDS->SetSubdatasetName(pszAttr);
        }

        osURI = TileDBDataset::VSI_to_tiledb_uri(poOpenInfo->pszFilename);
    }

    CPLString osAux = CPLGetBasenameSafe(osURI);
    osAux += ".tdb";

    // aux file is in array folder
    poDS->SetPhysicalFilename(
        CPLFormFilenameSafe(osURI, osAux, nullptr).c_str());
    // Initialize any PAM information.
    poDS->SetDescription(osURI);

    poDS->m_osArrayURI = osURI;
    if (objectType == tiledb::Object::Type::Group)
    {
        poDS->m_bDatasetInGroup = true;

        // Full resolution raster array
        poDS->m_osArrayURI = CPLFormFilenameSafe(osURI.c_str(), "l_0", nullptr);
    }

    const tiledb_query_type_t eMode =
        poOpenInfo->eAccess == GA_Update ? TILEDB_WRITE : TILEDB_READ;

    if (poDS->nTimestamp)
    {
        poDS->m_array = std::make_unique<tiledb::Array>(
            *poDS->m_ctx, poDS->m_osArrayURI, eMode,
            tiledb::TemporalPolicy(tiledb::TimeTravel, poDS->nTimestamp));
    }
    else
    {
        poDS->m_array = std::make_unique<tiledb::Array>(
            *poDS->m_ctx, poDS->m_osArrayURI, eMode);
    }

    poDS->eAccess = poOpenInfo->eAccess;

    // Force opening read-only dataset
    if (eMode == TILEDB_WRITE)
    {
        tiledb::Context *ctx = nullptr;
        poDS->GetArray(false, ctx);
    }

    // dependent on PAM metadata for information about array
    poDS->TryLoadXML();

    tiledb::ArraySchema schema = poDS->m_array->schema();

    char **papszStructMeta = poDS->GetMetadata("IMAGE_STRUCTURE");
    const char *pszXSize = CSLFetchNameValue(papszStructMeta, "X_SIZE");
    if (pszXSize)
    {
        poDS->nRasterXSize = atoi(pszXSize);
        if (poDS->nRasterXSize <= 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Width %i should be greater than zero.",
                     poDS->nRasterXSize);
            return nullptr;
        }
    }

    const char *pszYSize = CSLFetchNameValue(papszStructMeta, "Y_SIZE");
    if (pszYSize)
    {
        poDS->nRasterYSize = atoi(pszYSize);
        if (poDS->nRasterYSize <= 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Height %i should be greater than zero.",
                     poDS->nRasterYSize);
            return nullptr;
        }
    }

    const char *pszNBits = CSLFetchNameValue(papszStructMeta, "NBITS");
    if (pszNBits)
    {
        poDS->nBitsPerSample = atoi(pszNBits);
    }

    const char *pszDataType = CSLFetchNameValue(papszStructMeta, "DATA_TYPE");
    if (pszDataType)
    {
        // handle the case where arrays have been written with int type
        // (2.5.0)
        GDALDataType eDT = GDALGetDataTypeByName(pszDataType);
        if (eDT == GDT_Unknown)
        {
            int t = atoi(pszDataType);
            if ((t > 0) && (t < GDT_TypeCount))
                poDS->eDataType = static_cast<GDALDataType>(atoi(pszDataType));
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Unknown data type %s.",
                         pszDataType);
                return nullptr;
            }
        }
        else
        {
            poDS->eDataType = eDT;
        }
    }
    else
    {
        if (!pszAttr)
        {
            if (schema.attribute_num() == 1)
            {
                osAttrNameTmp = schema.attribute(0).name();
                pszAttr = osAttrNameTmp.c_str();
            }
        }
    }

    const char *pszIndexMode = CSLFetchNameValue(papszStructMeta, "INTERLEAVE");

    if (pszIndexMode)
        option_to_index_type(pszIndexMode, poDS->eIndexMode);

    std::vector<tiledb::Dimension> dims = schema.domain().dimensions();

    int iYDim = 0;
    int iXDim = 1;
    if ((dims.size() == 2) || (dims.size() == 3))
    {
        if (dims.size() == 3)
        {
            if ((pszAttr != nullptr) &&
                (schema.attributes().count(pszAttr) == 0))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "%s attribute is not found in TileDB schema.",
                         pszAttr);
                return nullptr;
            }

            if (poDS->eIndexMode == PIXEL)
                std::rotate(dims.begin(), dims.begin() + 2, dims.end());

            if (dims[0].type() != TILEDB_UINT64)
            {
                const char *pszTypeName = "";
                tiledb_datatype_to_str(dims[0].type(), &pszTypeName);
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported BAND dimension type: %s", pszTypeName);
                return nullptr;
            }
            poDS->nBandStart = dims[0].domain<uint64_t>().first;
            const uint64_t nBandEnd = dims[0].domain<uint64_t>().second;
            if (nBandEnd < poDS->nBandStart ||
                nBandEnd - poDS->nBandStart >
                    static_cast<uint64_t>(std::numeric_limits<int>::max() - 1))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Invalid bounds for BAND dimension.");
                return nullptr;
            }
            poDS->nBands = static_cast<int>(nBandEnd - poDS->nBandStart + 1);
            iYDim = 1;
            iXDim = 2;
        }
        else
        {
            const char *pszBands =
                poDS->GetMetadataItem("NUM_BANDS", "IMAGE_STRUCTURE");
            if (pszBands)
            {
                poDS->nBands = atoi(pszBands);
            }

            poDS->eIndexMode = ATTRIBUTES;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong number of dimensions %d: expected 2 or 3.",
                 static_cast<int>(dims.size()));
        return nullptr;
    }

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(poDS->nBands, /*bIsZeroAllowed=*/true))
    {
        return nullptr;
    }

    if (dims[iYDim].type() != TILEDB_UINT64)
    {
        const char *pszTypeName = "";
        tiledb_datatype_to_str(dims[0].type(), &pszTypeName);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported Y dimension type: %s", pszTypeName);
        return nullptr;
    }
    if (!pszYSize)
    {
        const uint64_t nStart = dims[iYDim].domain<uint64_t>().first;
        const uint64_t nEnd = dims[iYDim].domain<uint64_t>().second;
        if (nStart != 0 ||
            nEnd > static_cast<uint64_t>(std::numeric_limits<int>::max() - 1))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Invalid bounds for Y dimension.");
            return nullptr;
        }
        poDS->nRasterYSize = static_cast<int>(nEnd - nStart + 1);
    }
    const uint64_t nBlockYSize = dims[iYDim].tile_extent<uint64_t>();
    if (nBlockYSize > static_cast<uint64_t>(std::numeric_limits<int>::max()))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too large block Y size.");
        return nullptr;
    }
    poDS->nBlockYSize = static_cast<int>(nBlockYSize);

    if (dims[iXDim].type() != TILEDB_UINT64)
    {
        const char *pszTypeName = "";
        tiledb_datatype_to_str(dims[0].type(), &pszTypeName);
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported Y dimension type: %s", pszTypeName);
        return nullptr;
    }
    if (!pszXSize)
    {
        const uint64_t nStart = dims[iXDim].domain<uint64_t>().first;
        const uint64_t nEnd = dims[iXDim].domain<uint64_t>().second;
        if (nStart != 0 ||
            nEnd > static_cast<uint64_t>(std::numeric_limits<int>::max() - 1))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Invalid bounds for X dimension.");
            return nullptr;
        }
        poDS->nRasterXSize = static_cast<int>(nEnd - nStart + 1);
    }
    const uint64_t nBlockXSize = dims[iXDim].tile_extent<uint64_t>();
    if (nBlockXSize > static_cast<uint64_t>(std::numeric_limits<int>::max()))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too large block X size.");
        return nullptr;
    }
    poDS->nBlockXSize = static_cast<int>(nBlockXSize);

    poDS->nBlocksX = DIV_ROUND_UP(poDS->nRasterXSize, poDS->nBlockXSize);
    poDS->nBlocksY = DIV_ROUND_UP(poDS->nRasterYSize, poDS->nBlockYSize);

    if (dims.size() == 3)
    {
        // Create band information objects.
        for (int i = 1; i <= poDS->nBands; ++i)
        {
            if (pszAttr == nullptr)
                poDS->SetBand(i, new TileDBRasterBand(poDS.get(), i));
            else
                poDS->SetBand(
                    i, new TileDBRasterBand(poDS.get(), i, CPLString(pszAttr)));
        }
    }
    else  // subdatasets or only attributes
    {
        if ((poOpenInfo->eAccess == GA_Update) &&
            (poDS->GetMetadata("SUBDATASETS") != nullptr))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "The TileDB driver does not support update access "
                     "to subdatasets.");
            return nullptr;
        }

        if (!osSubdataset.empty())
        {
            // do we have the attribute in the schema
            if (schema.attributes().count(osSubdataset))
            {
                poDS->SetBand(
                    1, new TileDBRasterBand(poDS.get(), 1, osSubdataset));
            }
            else
            {
                if (schema.attributes().count(osSubdataset + "_1"))
                {
                    // Create band information objects.
                    for (int i = 1; i <= poDS->nBands; ++i)
                    {
                        CPLString osAttr = CPLString().Printf(
                            "%s_%d", osSubdataset.c_str(), i);
                        poDS->SetBand(
                            i, new TileDBRasterBand(poDS.get(), i, osAttr));
                    }
                }
                else
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "%s attribute is not found in TileDB schema.",
                             osSubdataset.c_str());
                    return nullptr;
                }
            }
        }
        else
        {
            char **papszMeta = poDS->GetMetadata("SUBDATASETS");
            if (papszMeta != nullptr)
            {
                if ((CSLCount(papszMeta) / 2) == 1)
                {
                    const char *pszSubDSName =
                        poDS->m_aosSubdatasetMD.FetchNameValueDef(
                            "SUBDATASET_1_NAME", "");
                    return GDALDataset::FromHandle(
                        GDALOpen(pszSubDSName, poOpenInfo->eAccess));
                }
            }
            else if (poDS->eIndexMode == ATTRIBUTES)
            {
                poDS->nBands = schema.attribute_num();

                // Create band information objects.
                for (int i = 1; i <= poDS->nBands; ++i)
                {
                    CPLString osAttr =
                        TILEDB_VALUES + CPLString().Printf("_%i", i);
                    poDS->SetBand(i,
                                  new TileDBRasterBand(poDS.get(), i, osAttr));
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "%s is missing required TileDB subdataset metadata.",
                         osURI.c_str());
                return nullptr;
            }
        }
    }

    // reload metadata now that bands are created to populate band metadata
    poDS->TryLoadCachedXML(nullptr, false);

    tiledb::VFS vfs(*poDS->m_ctx, poDS->m_ctx->config());

    if (!STARTS_WITH_CI(osURI, "TILEDB:") && vfs.is_dir(osURI))
        poDS->oOvManager.Initialize(poDS.get(), ":::VIRTUAL:::");
    else
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Overviews not supported for network writes.");

    return poDS.release();
}

/************************************************************************/
/*                              CreateAttribute()                       */
/************************************************************************/

template <class T, class NoDataT = T>
static tiledb::Attribute CreateAttribute(tiledb::Context &ctx,
                                         const std::string &osAttrName,
                                         tiledb::FilterList &filterList,
                                         bool bHasFillValue, double dfFillValue)
{
    auto attr = tiledb::Attribute::create<T>(ctx, osAttrName, filterList);
    if (bHasFillValue && GDALIsValueInRange<NoDataT>(dfFillValue))
    {
        const auto nVal = static_cast<NoDataT>(dfFillValue);
        if (dfFillValue == static_cast<double>(nVal))
        {
            if constexpr (sizeof(T) == sizeof(NoDataT))
            {
                attr.set_fill_value(&nVal, sizeof(nVal));
            }
            else
            {
                T aVal = {nVal, nVal};
                attr.set_fill_value(&aVal, sizeof(aVal));
            }
        }
    }
    return attr;
}

/************************************************************************/
/*                              CreateAttribute()                       */
/************************************************************************/

CPLErr TileDBRasterDataset::CreateAttribute(GDALDataType eType,
                                            const CPLString &osAttrName,
                                            const int nSubRasterCount,
                                            bool bHasFillValue,
                                            double dfFillValue)
{
    try
    {
        for (int i = 0; i < nSubRasterCount; ++i)
        {
            CPLString osName(osAttrName);
            // a few special cases
            // remove any leading slashes or
            // additional slashes as in the case of hdf5
            if STARTS_WITH (osName, "//")
            {
                osName = osName.substr(2);
            }

            osName.replaceAll("/", "_");
            CPLString osPrettyName = osName;

            if ((eIndexMode == ATTRIBUTES) ||
                ((m_bHasSubDatasets) && (nSubRasterCount > 1)))
            {
                osName = CPLString().Printf("%s_%d", osName.c_str(), i + 1);
            }

            switch (eType)
            {
                case GDT_Byte:
                {
                    m_schema->add_attribute(::CreateAttribute<unsigned char>(
                        *m_ctx, osName, *m_filterList, bHasFillValue,
                        dfFillValue));
                    nBitsPerSample = 8;
                    break;
                }
                case GDT_Int8:
                {
                    m_schema->add_attribute(
                        ::CreateAttribute<int8_t>(*m_ctx, osName, *m_filterList,
                                                  bHasFillValue, dfFillValue));
                    nBitsPerSample = 8;
                    break;
                }
                case GDT_UInt16:
                {
                    m_schema->add_attribute(::CreateAttribute<uint16_t>(
                        *m_ctx, osName, *m_filterList, bHasFillValue,
                        dfFillValue));
                    nBitsPerSample = 16;
                    break;
                }
                case GDT_UInt32:
                {
                    m_schema->add_attribute(::CreateAttribute<uint32_t>(
                        *m_ctx, osName, *m_filterList, bHasFillValue,
                        dfFillValue));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_UInt64:
                {
                    m_schema->add_attribute(::CreateAttribute<uint64_t>(
                        *m_ctx, osName, *m_filterList, bHasFillValue,
                        dfFillValue));
                    nBitsPerSample = 64;
                    break;
                }
                case GDT_Int16:
                {
                    m_schema->add_attribute(::CreateAttribute<int16_t>(
                        *m_ctx, osName, *m_filterList, bHasFillValue,
                        dfFillValue));
                    nBitsPerSample = 16;
                    break;
                }
                case GDT_Int32:
                {
                    m_schema->add_attribute(::CreateAttribute<int32_t>(
                        *m_ctx, osName, *m_filterList, bHasFillValue,
                        dfFillValue));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_Int64:
                {
                    m_schema->add_attribute(::CreateAttribute<int64_t>(
                        *m_ctx, osName, *m_filterList, bHasFillValue,
                        dfFillValue));
                    nBitsPerSample = 64;
                    break;
                }
                case GDT_Float32:
                {
                    m_schema->add_attribute(
                        ::CreateAttribute<float>(*m_ctx, osName, *m_filterList,
                                                 bHasFillValue, dfFillValue));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_Float64:
                {
                    m_schema->add_attribute(
                        ::CreateAttribute<double>(*m_ctx, osName, *m_filterList,
                                                  bHasFillValue, dfFillValue));
                    nBitsPerSample = 64;
                    break;
                }
                case GDT_CInt16:
                {
                    m_schema->add_attribute(
                        ::CreateAttribute<int16_t[2], int16_t>(
                            *m_ctx, osName, *m_filterList, bHasFillValue,
                            dfFillValue));
                    nBitsPerSample = 16;
                    break;
                }
                case GDT_CInt32:
                {
                    m_schema->add_attribute(
                        ::CreateAttribute<int32_t[2], int32_t>(
                            *m_ctx, osName, *m_filterList, bHasFillValue,
                            dfFillValue));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_CFloat32:
                {
                    m_schema->add_attribute(::CreateAttribute<float[2], float>(
                        *m_ctx, osName, *m_filterList, bHasFillValue,
                        dfFillValue));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_CFloat64:
                {
                    m_schema->add_attribute(
                        ::CreateAttribute<double[2], double>(
                            *m_ctx, osName, *m_filterList, bHasFillValue,
                            dfFillValue));
                    nBitsPerSample = 64;
                    break;
                }
                default:
                    return CE_Failure;
            }

            if ((m_bHasSubDatasets) && (i == 0))
            {
                CPLString osDim;
                switch (nSubRasterCount)
                {
                    case 2:
                        osDim.Printf("%dx%d", nRasterXSize, nRasterYSize);
                        break;
                    default:
                        osDim.Printf("%dx%dx%d", nSubRasterCount, nRasterXSize,
                                     nRasterYSize);
                        break;
                }

                const int nSubDataCount = 1 + m_aosSubdatasetMD.size() / 2;
                m_aosSubdatasetMD.SetNameValue(
                    CPLString().Printf("SUBDATASET_%d_NAME", nSubDataCount),
                    CPLString().Printf("%s", osPrettyName.c_str()));

                m_aosSubdatasetMD.SetNameValue(
                    CPLString().Printf("SUBDATASET_%d_DESC", nSubDataCount),
                    CPLString().Printf("[%s] %s (%s)", osDim.c_str(),
                                       osPrettyName.c_str(),
                                       GDALGetDataTypeName(eType)));

                // add to PAM metadata
                if (!m_poSubDatasetsTree)
                {
                    m_poSubDatasetsTree.reset(
                        CPLCreateXMLNode(nullptr, CXT_Element, "PAMDataset"));
                }

                CPLXMLNode *psSubNode = CPLCreateXMLNode(
                    m_poSubDatasetsTree.get(), CXT_Element, "Subdataset");
                CPLAddXMLAttributeAndValue(psSubNode, "name",
                                           osPrettyName.c_str());

                CPLXMLNode *psMetaNode = CPLCreateXMLNode(
                    CPLCreateXMLNode(psSubNode, CXT_Element, "PAMDataset"),
                    CXT_Element, "Metadata");
                CPLAddXMLAttributeAndValue(psMetaNode, "domain",
                                           "IMAGE_STRUCTURE");

                CPLAddXMLAttributeAndValue(
                    CPLCreateXMLElementAndValue(
                        psMetaNode, "MDI",
                        CPLString().Printf("%d", nRasterXSize)),
                    "KEY", "X_SIZE");

                CPLAddXMLAttributeAndValue(
                    CPLCreateXMLElementAndValue(
                        psMetaNode, "MDI",
                        CPLString().Printf("%d", nRasterYSize)),
                    "KEY", "Y_SIZE");

                CPLAddXMLAttributeAndValue(
                    CPLCreateXMLElementAndValue(
                        psMetaNode, "MDI",
                        CPLString().Printf("%s", GDALGetDataTypeName(eType))),
                    "KEY", "DATA_TYPE");

                if (m_lpoAttributeDS.size() > 0)
                {
                    CPLAddXMLAttributeAndValue(
                        CPLCreateXMLElementAndValue(
                            psMetaNode, "MDI",
                            CPLString().Printf("%d", nBands)),
                        "KEY", "NUM_BANDS");
                }
                else
                {
                    CPLAddXMLAttributeAndValue(
                        CPLCreateXMLElementAndValue(
                            psMetaNode, "MDI",
                            CPLString().Printf("%d", nSubRasterCount)),
                        "KEY", "NUM_BANDS");
                }

                CPLAddXMLAttributeAndValue(
                    CPLCreateXMLElementAndValue(
                        psMetaNode, "MDI",
                        CPLString().Printf("%d", nBitsPerSample)),
                    "KEY", "NBITS");
            }
        }
        return CE_None;
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return CE_Failure;
    }
}

/************************************************************************/
/*                              SetBlockSize()                          */
/************************************************************************/

void TileDBRasterDataset::SetBlockSize(GDALRasterBand *poBand,
                                       CPLStringList &aosOptions)

{
    int nX = 0;
    int nY = 0;
    poBand->GetBlockSize(&nX, &nY);

    if (!aosOptions.FetchNameValue("BLOCKXSIZE"))
    {
        aosOptions.SetNameValue("BLOCKXSIZE", CPLString().Printf("%d", nX));
    }

    if (!aosOptions.FetchNameValue("BLOCKYSIZE"))
    {
        aosOptions.SetNameValue("BLOCKYSIZE", CPLString().Printf("%d", nY));
    }
}

/************************************************************************/
/*                              CreateLL()                              */
/*                                                                      */
/*      Shared functionality between TileDBDataset::Create() and        */
/*      TileDBDataset::CreateCopy() for creating TileDB array based on  */
/*      a set of options and a configuration.                           */
/************************************************************************/

TileDBRasterDataset *TileDBRasterDataset::CreateLL(const char *pszFilename,
                                                   int nXSize, int nYSize,
                                                   int nBandsIn,
                                                   GDALDataType eType,
                                                   CSLConstList papszOptions)
{
    try
    {
        if ((nXSize <= 0 && nYSize <= 0))
        {
            return nullptr;
        }

        auto poDS = std::make_unique<TileDBRasterDataset>();
        poDS->nRasterXSize = nXSize;
        poDS->nRasterYSize = nYSize;
        poDS->eDataType = eType;
        poDS->nBands = nBandsIn;
        poDS->eAccess = GA_Update;

        if (poDS->nBands == 0)  // subdatasets
        {
            poDS->eIndexMode = ATTRIBUTES;
        }
        else
        {
            const char *pszIndexMode =
                CSLFetchNameValue(papszOptions, "INTERLEAVE");

            if (option_to_index_type(pszIndexMode, poDS->eIndexMode))
                return nullptr;
        }

        const char *pszConfig =
            CSLFetchNameValue(papszOptions, "TILEDB_CONFIG");
        if (pszConfig != nullptr)
        {
            poDS->m_osConfigFilename = pszConfig;
            tiledb::Config cfg(pszConfig);
            poDS->m_ctx.reset(new tiledb::Context(cfg));
        }
        else
        {
            poDS->m_ctx.reset(new tiledb::Context());
        }

        if (CPLTestBool(
                CSLFetchNameValueDef(papszOptions, "CREATE_GROUP", "YES")))
        {
            poDS->m_bDatasetInGroup = true;
            tiledb::create_group(*(poDS->m_ctx.get()), pszFilename);

            {
                tiledb::Group group(*(poDS->m_ctx.get()), pszFilename,
                                    TILEDB_WRITE);
                group.put_metadata(
                    DATASET_TYPE_ATTRIBUTE_NAME, TILEDB_STRING_UTF8,
                    static_cast<int>(strlen(RASTER_DATASET_TYPE)),
                    RASTER_DATASET_TYPE);

                group.close();
            }

            // Full resolution raster array
            poDS->m_osArrayURI =
                CPLFormFilenameSafe(pszFilename, "l_0", nullptr);
        }
        else
        {
            poDS->m_osArrayURI = pszFilename;
        }

        const char *pszCompression =
            CSLFetchNameValue(papszOptions, "COMPRESSION");
        const char *pszCompressionLevel =
            CSLFetchNameValue(papszOptions, "COMPRESSION_LEVEL");

        const char *pszBlockXSize =
            CSLFetchNameValue(papszOptions, "BLOCKXSIZE");
        poDS->nBlockXSize = (pszBlockXSize) ? atoi(pszBlockXSize) : 256;
        const char *pszBlockYSize =
            CSLFetchNameValue(papszOptions, "BLOCKYSIZE");
        poDS->nBlockYSize = (pszBlockYSize) ? atoi(pszBlockYSize) : 256;
        poDS->bStats = CSLFetchBoolean(papszOptions, "STATS", FALSE);

        const char *pszTimestamp =
            CSLFetchNameValue(papszOptions, "TILEDB_TIMESTAMP");
        if (pszTimestamp != nullptr)
            poDS->nTimestamp = std::strtoull(pszTimestamp, nullptr, 10);

        // set dimensions and attribute type for schema
        poDS->m_schema.reset(
            new tiledb::ArraySchema(*poDS->m_ctx, TILEDB_DENSE));
        poDS->m_schema->set_tile_order(TILEDB_ROW_MAJOR);
        poDS->m_schema->set_cell_order(TILEDB_ROW_MAJOR);

        poDS->m_filterList.reset(new tiledb::FilterList(*poDS->m_ctx));

        if (pszCompression != nullptr)
        {
            int nLevel = (pszCompressionLevel) ? atoi(pszCompressionLevel) : -1;
            if (TileDBDataset::AddFilter(*(poDS->m_ctx.get()),
                                         *(poDS->m_filterList.get()),
                                         pszCompression, nLevel) == CE_None)
            {
                poDS->SetMetadataItem("COMPRESSION", pszCompression,
                                      "IMAGE_STRUCTURE");
                poDS->m_schema->set_coords_filter_list(*poDS->m_filterList);
            }
        }

        CPLString osAux = CPLGetBasenameSafe(pszFilename);
        osAux += ".tdb";

        poDS->SetPhysicalFilename(
            CPLFormFilenameSafe(pszFilename, osAux.c_str(), nullptr).c_str());

        // Initialize PAM information.
        poDS->SetDescription(pszFilename);

        // Note the dimension bounds are inclusive and are expanded to the match
        // the block size
        poDS->nBlocksX = DIV_ROUND_UP(nXSize, poDS->nBlockXSize);
        poDS->nBlocksY = DIV_ROUND_UP(nYSize, poDS->nBlockYSize);

        // register additional attributes to the pixel value, these will be
        // be reported as subdatasets on future reads
        CSLConstList papszAttributes =
            CSLFetchNameValueMultiple(papszOptions, "TILEDB_ATTRIBUTE");
        for (const char *pszAttribute : cpl::Iterate(papszAttributes))
        {
            // modeling additional attributes as subdatasets
            poDS->m_bHasSubDatasets = true;
            // check each attribute is a GDAL source
            std::unique_ptr<GDALDataset> poAttrDS(
                GDALDataset::Open(pszAttribute, GA_ReadOnly));

            if (poAttrDS != nullptr)
            {
                // check each is co-registered
                // candidate band
                int nAttrBands = poAttrDS->GetRasterCount();
                if (nAttrBands > 0)
                {
                    GDALRasterBand *poAttrBand = poAttrDS->GetRasterBand(1);

                    if ((poAttrBand->GetXSize() == poDS->nRasterXSize) &&
                        (poAttrBand->GetYSize() == poDS->nRasterYSize) &&
                        (poDS->nBands == nAttrBands))
                    {
                        // could check geotransform, but it is sufficient
                        // that cartesian dimensions are equal
                        poDS->m_lpoAttributeDS.push_back(std::move(poAttrDS));
                    }
                    else
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Skipping %s as it has a different dimension\n",
                            pszAttribute);
                    }
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Skipping %s as it doesn't have any bands\n",
                             pszAttribute);
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Skipping %s, not recognized as a GDAL dataset\n",
                         pszAttribute);
            }
        }

        return poDS.release();
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "TileDB: %s", e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                          DeferredCreate()                            */
/*                                                                      */
/*  Create dimension, domains and attributes. and optionally the array  */
/************************************************************************/

bool TileDBRasterDataset::DeferredCreate(bool bCreateArray)
{
    CPLAssert(!m_bDeferredCreateHasRun);
    m_bDeferredCreateHasRun = true;
    m_bDeferredCreateHasBeenSuccessful = false;

    try
    {
        // this driver enforces that all subdatasets are the same size
        tiledb::Domain domain(*m_ctx);

        uint64_t w = (uint64_t)nBlocksX * nBlockXSize - 1;
        uint64_t h = (uint64_t)nBlocksY * nBlockYSize - 1;

        auto d1 = tiledb::Dimension::create<uint64_t>(*m_ctx, "X", {0, w},
                                                      uint64_t(nBlockXSize));
        auto d2 = tiledb::Dimension::create<uint64_t>(*m_ctx, "Y", {0, h},
                                                      uint64_t(nBlockYSize));

        {
            CPLErr eErr;
            // Only used for unit test purposes (to check ability of GDAL to read
            // an arbitrary array)
            const char *pszAttrName =
                CPLGetConfigOption("TILEDB_ATTRIBUTE", TILEDB_VALUES);
            if ((nBands == 0) || (eIndexMode == ATTRIBUTES))
            {
                eErr = AddDimensions(domain, pszAttrName, d2, d1, nullptr);
            }
            else
            {
                auto d3 = tiledb::Dimension::create<uint64_t>(
                    *m_ctx, "BANDS", {1, uint64_t(nBands)}, 1);
                eErr = AddDimensions(domain, pszAttrName, d2, d1, &d3);
            }
            if (eErr != CE_None)
                return false;
        }

        m_schema->set_domain(domain).set_order(
            {{TILEDB_ROW_MAJOR, TILEDB_ROW_MAJOR}});

        // register additional attributes to the pixel value, these will be
        // be reported as subdatasets on future reads
        for (const auto &poAttrDS : m_lpoAttributeDS)
        {
            const std::string osAttrName =
                CPLGetBasenameSafe(poAttrDS->GetDescription());
            GDALRasterBand *poAttrBand = poAttrDS->GetRasterBand(1);
            int bHasNoData = false;
            const double dfNoData = poAttrBand->GetNoDataValue(&bHasNoData);
            CreateAttribute(poAttrBand->GetRasterDataType(), osAttrName.c_str(),
                            1, CPL_TO_BOOL(bHasNoData), dfNoData);
        }

        if (bCreateArray)
        {
            CreateArray();
        }

        m_bDeferredCreateHasBeenSuccessful = true;
        return true;
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "TileDB: %s", e.what());
        return false;
    }
}

/************************************************************************/
/*                          CreateArray()                               */
/************************************************************************/

void TileDBRasterDataset::CreateArray()
{
    tiledb::Array::create(m_osArrayURI, *m_schema);

    if (m_bDatasetInGroup)
    {
        tiledb::Group group(*m_ctx, GetDescription(), TILEDB_WRITE);
        group.add_member(m_osArrayURI, false);
        group.close();
    }

    if (nTimestamp)
        m_array.reset(new tiledb::Array(
            *m_ctx, m_osArrayURI, TILEDB_WRITE,
            tiledb::TemporalPolicy(tiledb::TimeTravel, nTimestamp)));
    else
        m_array.reset(new tiledb::Array(*m_ctx, m_osArrayURI, TILEDB_WRITE));
}

/************************************************************************/
/*                              CopySubDatasets()                       */
/*                                                                      */
/*      Copy SubDatasets from src to a TileDBRasterDataset              */
/*                                                                      */
/************************************************************************/

CPLErr TileDBRasterDataset::CopySubDatasets(GDALDataset *poSrcDS,
                                            TileDBRasterDataset *poDstDS,
                                            GDALProgressFunc pfnProgress,
                                            void *pProgressData)

{
    try
    {
        std::vector<std::unique_ptr<GDALDataset>> apoDatasets;
        poDstDS->m_bHasSubDatasets = true;
        CSLConstList papszSrcSubDatasets = poSrcDS->GetMetadata("SUBDATASETS");
        if (!papszSrcSubDatasets)
            return CE_Failure;
        const char *pszSubDSName =
            CSLFetchNameValue(papszSrcSubDatasets, "SUBDATASET_1_NAME");
        if (!pszSubDSName)
            return CE_Failure;

        CPLStringList apszTokens(CSLTokenizeString2(
            pszSubDSName, ":", CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES));
        // FIXME? this is tailored for HDF5-like subdataset names
        // HDF5:foo.hdf5:attrname.
        if (apszTokens.size() != 3)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot guess attribute name in %s", pszSubDSName);
            return CE_Failure;
        }

        std::unique_ptr<GDALDataset> poSubDataset(
            GDALDataset::Open(pszSubDSName));
        if (poSubDataset.get() == nullptr ||
            poSubDataset->GetRasterCount() == 0)
        {
            return CE_Failure;
        }

        uint64_t nSubXSize = poSubDataset->GetRasterXSize();
        uint64_t nSubYSize = poSubDataset->GetRasterYSize();

        const char *pszAttrName = apszTokens[2];

        auto poFirstSubDSBand = poSubDataset->GetRasterBand(1);
        int bFirstSubDSBandHasNoData = FALSE;
        const double dfFirstSubDSBandNoData =
            poFirstSubDSBand->GetNoDataValue(&bFirstSubDSBandHasNoData);
        poDstDS->CreateAttribute(poFirstSubDSBand->GetRasterDataType(),
                                 pszAttrName, poSubDataset->GetRasterCount(),
                                 CPL_TO_BOOL(bFirstSubDSBandHasNoData),
                                 dfFirstSubDSBandNoData);
        apoDatasets.push_back(std::move(poSubDataset));

        for (const auto &[pszKey, pszValue] :
             cpl::IterateNameValue(papszSrcSubDatasets))
        {
            if (EQUAL(pszKey, "SUBDATASET_1_NAME") || !strstr(pszKey, "_NAME"))
            {
                continue;
            }
            pszSubDSName = pszValue;
            apszTokens = CSLTokenizeString2(
                pszSubDSName, ":", CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES);
            if (apszTokens.size() != 3)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot guess attribute name in %s", pszSubDSName);
                continue;
            }

            std::unique_ptr<GDALDataset> poSubDS(
                GDALDataset::Open(pszSubDSName));
            if ((poSubDS != nullptr) && poSubDS->GetRasterCount() > 0)
            {
                GDALRasterBand *poBand = poSubDS->GetRasterBand(1);
                int nBlockXSize, nBlockYSize;
                poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);

                int bHasNoData = FALSE;
                const double dfNoData = poBand->GetNoDataValue(&bHasNoData);

                if ((poSubDS->GetRasterXSize() != (int)nSubXSize) ||
                    (poSubDS->GetRasterYSize() != (int)nSubYSize) ||
                    (nBlockXSize != poDstDS->nBlockXSize) ||
                    (nBlockYSize != poDstDS->nBlockYSize))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Sub-datasets must have the same dimension,"
                             " and block sizes, skipping %s",
                             pszSubDSName);
                }
                else
                {
                    pszAttrName = apszTokens[2];
                    poDstDS->CreateAttribute(
                        poSubDS->GetRasterBand(1)->GetRasterDataType(),
                        pszAttrName, poSubDS->GetRasterCount(),
                        CPL_TO_BOOL(bHasNoData), dfNoData);
                    apoDatasets.push_back(std::move(poSubDS));
                }
            }
            else
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Sub-datasets must be not null and contain data in bands,"
                    "skipping %s\n",
                    pszSubDSName);
            }
        }

        poDstDS->SetMetadata(poDstDS->m_aosSubdatasetMD.List(), "SUBDATASETS");

        poDstDS->CreateArray();

        /* --------------------------------------------------------  */
        /*      Report preliminary (0) progress.                     */
        /* --------------------------------------------------------- */
        if (!pfnProgress(0.0, nullptr, pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt,
                     "User terminated CreateCopy()");
            return CE_Failure;
        }

        // copy over subdatasets by block
        tiledb::Query query(*poDstDS->m_ctx, *poDstDS->m_array);
        query.set_layout(TILEDB_GLOBAL_ORDER);
        const uint64_t nTotalBlocks =
            static_cast<uint64_t>(poDstDS->nBlocksX) * poDstDS->nBlocksY;
        uint64_t nBlockCounter = 0;

        // row-major
        for (int j = 0; j < poDstDS->nBlocksY; ++j)
        {
            for (int i = 0; i < poDstDS->nBlocksX; ++i)
            {
                std::vector<std::unique_ptr<void, decltype(&VSIFree)>> aBlocks;
                // have to write set all tiledb attributes on write
                int iAttr = 0;
                for (auto &poSubDS : apoDatasets)
                {
                    const GDALDataType eDT =
                        poSubDS->GetRasterBand(1)->GetRasterDataType();

                    for (int b = 1; b <= poSubDS->GetRasterCount(); ++b)
                    {
                        const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
                        const size_t nValues =
                            static_cast<size_t>(poDstDS->nBlockXSize) *
                            poDstDS->nBlockYSize;
                        void *pBlock = VSI_MALLOC_VERBOSE(nDTSize * nValues);
                        if (!pBlock)
                            return CE_Failure;
                        aBlocks.emplace_back(pBlock, &VSIFree);
                        GDALRasterBand *poBand = poSubDS->GetRasterBand(b);
                        if (poBand->ReadBlock(i, j, pBlock) == CE_None)
                        {
                            SetBuffer(
                                &query, eDT,
                                poDstDS->m_schema->attribute(iAttr).name(),
                                pBlock, nValues);
                        }
                        ++iAttr;
                    }
                }

                if (poDstDS->bStats)
                    tiledb::Stats::enable();

                auto status = query.submit();

                if (poDstDS->bStats)
                {
                    tiledb::Stats::dump(stdout);
                    tiledb::Stats::disable();
                }

                if (status == tiledb::Query::Status::FAILED)
                {
                    return CE_Failure;
                }

                ++nBlockCounter;
                if (!pfnProgress(static_cast<double>(nBlockCounter) /
                                     static_cast<double>(nTotalBlocks),
                                 nullptr, pProgressData))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt,
                             "User terminated CreateCopy()");
                    return CE_Failure;
                }
            }
        }

        query.finalize();

        return CE_None;
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return CE_Failure;
    }
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

TileDBRasterDataset *TileDBRasterDataset::Create(const char *pszFilename,
                                                 int nXSize, int nYSize,
                                                 int nBandsIn,
                                                 GDALDataType eType,
                                                 char **papszOptions)

{
    CPLString osArrayPath = TileDBDataset::VSI_to_tiledb_uri(pszFilename);

    std::unique_ptr<TileDBRasterDataset> poDS(TileDBRasterDataset::CreateLL(
        osArrayPath, nXSize, nYSize, nBandsIn, eType, papszOptions));

    if (!poDS)
        return nullptr;

    const char *pszAttrName =
        CPLGetConfigOption("TILEDB_ATTRIBUTE", TILEDB_VALUES);
    for (int i = 0; i < poDS->nBands; i++)
    {
        if (poDS->eIndexMode == ATTRIBUTES)
            poDS->SetBand(
                i + 1, new TileDBRasterBand(
                           poDS.get(), i + 1,
                           TILEDB_VALUES + CPLString().Printf("_%i", i + 1)));
        else
            poDS->SetBand(i + 1,
                          new TileDBRasterBand(poDS.get(), i + 1, pszAttrName));
    }

    // TILEDB_WRITE_IMAGE_STRUCTURE=NO only used for unit test purposes (to
    // check ability of GDAL to read an arbitrary array)
    if (CPLTestBool(CPLGetConfigOption("TILEDB_WRITE_IMAGE_STRUCTURE", "YES")))
    {
        CPLStringList aosImageStruct;
        aosImageStruct.SetNameValue(
            "NBITS", CPLString().Printf("%d", poDS->nBitsPerSample));
        aosImageStruct.SetNameValue(
            "DATA_TYPE",
            CPLString().Printf("%s", GDALGetDataTypeName(poDS->eDataType)));
        aosImageStruct.SetNameValue(
            "X_SIZE", CPLString().Printf("%d", poDS->nRasterXSize));
        aosImageStruct.SetNameValue(
            "Y_SIZE", CPLString().Printf("%d", poDS->nRasterYSize));
        aosImageStruct.SetNameValue("INTERLEAVE",
                                    index_type_name(poDS->eIndexMode));
        aosImageStruct.SetNameValue("DATASET_TYPE", RASTER_DATASET_TYPE);

        if (poDS->m_lpoAttributeDS.size() > 0)
        {
            int i = 0;
            for (auto const &poAttrDS : poDS->m_lpoAttributeDS)
            {
                aosImageStruct.SetNameValue(
                    CPLString().Printf("TILEDB_ATTRIBUTE_%i", ++i),
                    CPLGetBasenameSafe(poAttrDS->GetDescription()).c_str());
            }
        }
        poDS->SetMetadata(aosImageStruct.List(), "IMAGE_STRUCTURE");
    }

    return poDS.release();
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *TileDBRasterDataset::CreateCopy(const char *pszFilename,
                                             GDALDataset *poSrcDS, int bStrict,
                                             char **papszOptions,
                                             GDALProgressFunc pfnProgress,
                                             void *pProgressData)

{
    CPLStringList aosOptions(CSLDuplicate(papszOptions));
    CPLString osArrayPath = TileDBDataset::VSI_to_tiledb_uri(pszFilename);

    std::unique_ptr<TileDBRasterDataset> poDstDS;

    if (CSLFetchNameValue(papszOptions, "APPEND_SUBDATASET"))
    {
        // TileDB schemas are fixed
        CPLError(CE_Failure, CPLE_NotSupported,
                 "TileDB driver does not support "
                 "appending to an existing schema.");
        return nullptr;
    }

    char **papszSrcSubDatasets = poSrcDS->GetMetadata("SUBDATASETS");

    if (papszSrcSubDatasets == nullptr)
    {
        const int nBands = poSrcDS->GetRasterCount();

        if (nBands > 0)
        {
            GDALRasterBand *poBand = poSrcDS->GetRasterBand(1);
            GDALDataType eType = poBand->GetRasterDataType();

            for (int i = 2; i <= nBands; ++i)
            {
                if (eType != poSrcDS->GetRasterBand(i)->GetRasterDataType())
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "TileDB driver does not support "
                             "source dataset with different band data types.");
                    return nullptr;
                }
            }

            poDstDS.reset(TileDBRasterDataset::Create(
                osArrayPath, poSrcDS->GetRasterXSize(),
                poSrcDS->GetRasterYSize(), nBands, eType, papszOptions));

            if (!poDstDS)
            {
                return nullptr;
            }

            for (int i = 1; i <= nBands; ++i)
            {
                int bHasNoData = FALSE;
                const double dfNoData =
                    poSrcDS->GetRasterBand(i)->GetNoDataValue(&bHasNoData);
                if (bHasNoData)
                    poDstDS->GetRasterBand(i)->SetNoDataValue(dfNoData);
            }

            CPLErr eErr =
                GDALDatasetCopyWholeRaster(poSrcDS, poDstDS.get(), papszOptions,
                                           pfnProgress, pProgressData);

            if (eErr != CE_None)
            {
                CPLError(eErr, CPLE_AppDefined,
                         "Error copying raster to TileDB.");
                return nullptr;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "TileDB driver does not support "
                     "source dataset with zero bands.");
            return nullptr;
        }
    }
    else
    {
        if (bStrict)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "TileDB driver does not support copying "
                     "subdatasets in strict mode.");
            return nullptr;
        }

        if (CSLFetchNameValue(papszOptions, "BLOCKXSIZE") ||
            CSLFetchNameValue(papszOptions, "BLOCKYSIZE"))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Changing block size is not supported when copying "
                     "subdatasets.");
            return nullptr;
        }

        const int nSubDatasetCount = CSLCount(papszSrcSubDatasets) / 2;
        const int nMaxFiles =
            atoi(CPLGetConfigOption("GDAL_READDIR_LIMIT_ON_OPEN", "1000"));

        aosOptions.SetNameValue("CREATE_GROUP", "NO");

        if (nSubDatasetCount <= nMaxFiles)
        {
            const char *pszSource =
                CSLFetchNameValue(papszSrcSubDatasets, "SUBDATASET_1_NAME");
            if (pszSource)
            {
                std::unique_ptr<GDALDataset> poSubDataset(
                    GDALDataset::Open(pszSource));
                if (poSubDataset && poSubDataset->GetRasterCount() > 0)
                {
                    GDALRasterBand *poBand = poSubDataset->GetRasterBand(1);

                    TileDBRasterDataset::SetBlockSize(poBand, aosOptions);
                    poDstDS.reset(TileDBRasterDataset::CreateLL(
                        osArrayPath, poBand->GetXSize(), poBand->GetYSize(), 0,
                        poBand->GetRasterDataType(), aosOptions.List()));

                    if (poDstDS)
                    {
                        if (!poDstDS->DeferredCreate(
                                /* bCreateArray = */ false))
                            return nullptr;

                        if (TileDBRasterDataset::CopySubDatasets(
                                poSrcDS, poDstDS.get(), pfnProgress,
                                pProgressData) != CE_None)
                        {
                            poDstDS.reset();
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Unable to copy subdatasets.");
                        }
                    }
                }
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Please increase GDAL_READDIR_LIMIT_ON_OPEN variable.");
        }
    }

    // TODO Supporting mask bands is a possible future task
    if (poDstDS != nullptr)
    {
        int nCloneFlags = GCIF_PAM_DEFAULT & ~GCIF_MASK;
        poDstDS->CloneInfo(poSrcDS, nCloneFlags);

        if (poDstDS->FlushCache(false) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "FlushCache() failed");
            return nullptr;
        }

        return poDstDS.release();
    }
    return nullptr;
}

/************************************************************************/
/*                            LoadOverviews()                           */
/************************************************************************/

void TileDBRasterDataset::LoadOverviews()
{
    if (m_bLoadOverviewsDone)
        return;
    m_bLoadOverviewsDone = true;

    // NOTE: read overview_model.rst for a high level explanation of overviews
    // are stored.

    if (!m_bDatasetInGroup)
        return;

    CPLStringList aosOpenOptions;
    if (nTimestamp)
    {
        aosOpenOptions.SetNameValue("TILEDB_TIMESTAMP",
                                    CPLSPrintf("%" PRIu64, nTimestamp));
    }
    if (!m_osConfigFilename.empty())
    {
        aosOpenOptions.SetNameValue("TILEDB_CONFIG",
                                    m_osConfigFilename.c_str());
    }
    for (int i = 0; i < m_nOverviewCountFromMetadata; ++i)
    {
        const std::string osArrayName = CPLSPrintf("l_%d", 1 + i);
        const std::string osOvrDatasetName =
            CPLFormFilenameSafe(GetDescription(), osArrayName.c_str(), nullptr);

        GDALOpenInfo oOpenInfo(osOvrDatasetName.c_str(), eAccess);
        oOpenInfo.papszOpenOptions = aosOpenOptions.List();
        auto poOvrDS = std::unique_ptr<GDALDataset>(
            Open(&oOpenInfo, tiledb::Object::Type::Array));
        if (!poOvrDS)
            return;
        if (poOvrDS->GetRasterCount() != nBands)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Overview %s has not the same number of bands as full "
                     "resolution dataset",
                     osOvrDatasetName.c_str());
            return;
        }
        m_apoOverviewDS.emplace_back(std::move(poOvrDS));
    }
}

/************************************************************************/
/*                            IBuildOverviews()                         */
/************************************************************************/

CPLErr TileDBRasterDataset::IBuildOverviews(
    const char *pszResampling, int nOverviews, const int *panOverviewList,
    int nListBands, const int *panBandList, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions)
{
    // NOTE: read overview_model.rst for a high level explanation of overviews
    // are stored.

    if (eAccess == GA_ReadOnly)
    {
        if (!CPLTestBool(
                CPLGetConfigOption("TILEDB_GEOTIFF_OVERVIEWS", "FALSE")))
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "Cannot %s overviews in TileDB format in read-only mode",
                nOverviews ? "create" : "delete");
            return CE_Failure;
        }

        // GeoTIFF overviews. This used to be supported before GDAL 3.10
        // although likely not desirable.
        return GDALPamDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
            pfnProgress, pProgressData, papszOptions);
    }

    if (nBands == 0)
    {
        return CE_Failure;
    }

    // If we already have PAM overview (i.e. GeoTIFF based), go through PAM
    if (cpl::down_cast<GDALPamRasterBand *>(GetRasterBand(1))
            ->GDALPamRasterBand::GetOverviewCount() > 0)
    {
        return GDALPamDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
            pfnProgress, pProgressData, papszOptions);
    }

    if (!m_bDatasetInGroup)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "IBuildOverviews() only supported for datasets created "
                    "with CREATE_GROUP=YES");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Our overview support currently only works safely if all         */
    /*      bands are handled at the same time.                             */
    /* -------------------------------------------------------------------- */
    if (nListBands != nBands)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Generation of TileDB overviews currently only "
                    "supported when operating on all bands.  "
                    "Operation failed.");
        return CE_Failure;
    }

    // Force loading existing overviews
    if (m_nOverviewCountFromMetadata)
        GetRasterBand(1)->GetOverviewCount();
    m_bLoadOverviewsDone = true;

    /* -------------------------------------------------------------------- */
    /*      Deletes existing overviews if requested.                        */
    /* -------------------------------------------------------------------- */
    if (nOverviews == 0)
    {
        CPLErr eErr = CE_None;

        // Unlink arrays from he group
        try
        {
            tiledb::Group group(*m_ctx, GetDescription(), TILEDB_WRITE);
            for (auto &&poODS : m_apoOverviewDS)
            {
                group.remove_member(poODS->GetDescription());
            }
            group.close();
        }
        catch (const tiledb::TileDBError &e)
        {
            eErr = CE_Failure;
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        }

        tiledb::VFS vfs(*m_ctx, m_ctx->config());

        // Delete arrays
        for (auto &&poODS : m_apoOverviewDS)
        {
            try
            {
                CPL_IGNORE_RET_VAL(poODS->Close());
                tiledb::Array::delete_array(*m_ctx, poODS->GetDescription());
                if (vfs.is_dir(poODS->GetDescription()))
                {
                    vfs.remove_dir(poODS->GetDescription());
                }
            }
            catch (const tiledb::TileDBError &e)
            {
                eErr = CE_Failure;
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Array::delete_array(%s) failed: %s",
                         poODS->GetDescription(), e.what());
            }
            m_apoOverviewDSRemoved.emplace_back(std::move(poODS));
        }

        m_apoOverviewDS.clear();
        m_nOverviewCountFromMetadata = 0;
        MarkPamDirty();
        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Establish which of the overview levels we already have, and     */
    /*      which are new.                                                  */
    /* -------------------------------------------------------------------- */
    std::vector<bool> abRequireNewOverview(nOverviews, true);
    for (int i = 0; i < nOverviews; ++i)
    {
        const int nOXSize = DIV_ROUND_UP(GetRasterXSize(), panOverviewList[i]);
        const int nOYSize = DIV_ROUND_UP(GetRasterYSize(), panOverviewList[i]);

        for (const auto &poODS : m_apoOverviewDS)
        {
            const int nOvFactor =
                GDALComputeOvFactor(poODS->GetRasterXSize(), GetRasterXSize(),
                                    poODS->GetRasterYSize(), GetRasterYSize());

            // If we already have a 1x1 overview and this new one would result
            // in it too, then don't create it.
            if (poODS->GetRasterXSize() == 1 && poODS->GetRasterYSize() == 1 &&
                nOXSize == 1 && nOYSize == 1)
            {
                abRequireNewOverview[i] = false;
                break;
            }

            if (nOvFactor == panOverviewList[i] ||
                nOvFactor == GDALOvLevelAdjust2(panOverviewList[i],
                                                GetRasterXSize(),
                                                GetRasterYSize()))
            {
                abRequireNewOverview[i] = false;
                break;
            }
        }

        if (abRequireNewOverview[i])
        {
            CPLStringList aosCreationOptions;
            aosCreationOptions.SetNameValue("CREATE_GROUP", "NO");
            aosCreationOptions.SetNameValue(
                "NBITS", CPLString().Printf("%d", nBitsPerSample));
            aosCreationOptions.SetNameValue("INTERLEAVE",
                                            index_type_name(eIndexMode));
            if (nTimestamp)
            {
                aosCreationOptions.SetNameValue(
                    "TILEDB_TIMESTAMP", CPLSPrintf("%" PRIu64, nTimestamp));
            }
            if (!m_osConfigFilename.empty())
            {
                aosCreationOptions.SetNameValue("TILEDB_CONFIG",
                                                m_osConfigFilename.c_str());
            }

            const std::string osArrayName =
                CPLSPrintf("l_%d", 1 + int(m_apoOverviewDS.size()));
            const std::string osOvrDatasetName = CPLFormFilenameSafe(
                GetDescription(), osArrayName.c_str(), nullptr);

            auto poOvrDS = std::unique_ptr<TileDBRasterDataset>(
                Create(osOvrDatasetName.c_str(), nOXSize, nOYSize, nBands,
                       GetRasterBand(1)->GetRasterDataType(),
                       aosCreationOptions.List()));
            if (!poOvrDS)
                return CE_Failure;

            // Apply nodata from main dataset
            for (int j = 0; j < nBands; ++j)
            {
                int bHasNoData = FALSE;
                const double dfNoData =
                    GetRasterBand(j + 1)->GetNoDataValue(&bHasNoData);
                if (bHasNoData)
                    poOvrDS->GetRasterBand(j + 1)->SetNoDataValue(dfNoData);
            }

            // Apply georeferencing from main dataset
            poOvrDS->SetSpatialRef(GetSpatialRef());
            double adfGeoTransform[6];
            if (GetGeoTransform(adfGeoTransform) == CE_None)
            {
                adfGeoTransform[1] *=
                    static_cast<double>(nRasterXSize) / nOXSize;
                adfGeoTransform[2] *=
                    static_cast<double>(nRasterXSize) / nOXSize;
                adfGeoTransform[4] *=
                    static_cast<double>(nRasterYSize) / nOYSize;
                adfGeoTransform[5] *=
                    static_cast<double>(nRasterYSize) / nOYSize;
                poOvrDS->SetGeoTransform(adfGeoTransform);
            }

            poOvrDS->DeferredCreate(/* bCreateArray = */ true);

            try
            {
                tiledb::Group group(*m_ctx, GetDescription(), TILEDB_WRITE);
                group.add_member(osOvrDatasetName, false);
                group.close();
            }
            catch (const tiledb::TileDBError &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            }

            m_apoOverviewDS.emplace_back(std::move(poOvrDS));
        }
    }

    m_nOverviewCountFromMetadata = static_cast<int>(m_apoOverviewDS.size());
    MarkPamDirty();

    /* -------------------------------------------------------------------- */
    /*      Refresh/generate overviews that are listed.                     */
    /* -------------------------------------------------------------------- */
    std::vector<GDALRasterBand *> apoSrcBands;
    std::vector<std::vector<GDALRasterBand *>> aapoOverviewBands;
    CPLErr eErr = CE_None;
    const auto osNormalizedResampling =
        GDALGetNormalizedOvrResampling(pszResampling);
    for (int iBand = 0; eErr == CE_None && iBand < nBands; iBand++)
    {
        apoSrcBands.push_back(GetRasterBand(iBand + 1));
        std::vector<GDALRasterBand *> apoOverviewBands;

        std::vector<bool> abAlreadyUsedOverviewBand(m_apoOverviewDS.size(),
                                                    false);

        for (int i = 0; i < nOverviews; i++)
        {
            bool bFound = false;
            for (size_t j = 0; j < m_apoOverviewDS.size(); ++j)
            {
                if (!abAlreadyUsedOverviewBand[j])
                {
                    auto &poODS = m_apoOverviewDS[j];
                    int nOvFactor = GDALComputeOvFactor(
                        poODS->GetRasterXSize(), nRasterXSize,
                        poODS->GetRasterYSize(), nRasterYSize);

                    if (nOvFactor == panOverviewList[i] ||
                        nOvFactor == GDALOvLevelAdjust2(panOverviewList[i],
                                                        nRasterXSize,
                                                        nRasterYSize))
                    {
                        abAlreadyUsedOverviewBand[j] = true;
                        auto poOvrBand = poODS->GetRasterBand(iBand + 1);
                        if (!osNormalizedResampling.empty())
                        {
                            // Store resampling method in band metadata, as it
                            // can be used by the gdaladdo utilities to refresh
                            // existing overviews with the method previously
                            // used
                            poOvrBand->SetMetadataItem(
                                "RESAMPLING", osNormalizedResampling.c_str());
                        }
                        apoOverviewBands.push_back(poOvrBand);
                        bFound = true;
                        break;
                    }
                }
            }
            if (!bFound)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Could not find dataset corresponding to ov factor %d",
                         panOverviewList[i]);
                eErr = CE_Failure;
            }
        }
        if (iBand > 0)
        {
            CPLAssert(apoOverviewBands.size() == aapoOverviewBands[0].size());
        }
        aapoOverviewBands.emplace_back(std::move(apoOverviewBands));
    }

    if (eErr == CE_None)
    {
        eErr = GDALRegenerateOverviewsMultiBand(apoSrcBands, aapoOverviewBands,
                                                pszResampling, pfnProgress,
                                                pProgressData, papszOptions);
    }

    return eErr;
}
