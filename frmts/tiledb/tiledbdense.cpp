/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB Support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2019, TileDB, Inc
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

#include <cassert>
#include <limits>

#include "tiledbheaders.h"

constexpr const char *RASTER_DATASET_TYPE = "raster";

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

  public:
    TileDBRasterBand(TileDBRasterDataset *, int,
                     const std::string &osAttr = TILEDB_VALUES);
    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, GSpacing, GSpacing,
                             GDALRasterIOExtraArg *psExtraArg) override;
    virtual GDALColorInterp GetColorInterpretation() override;
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
                        const CPLString &osAttrName, void *pImage, int nSize)
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
    if (poGDS->eIndexMode == ATTRIBUTES && eRWFlag == GF_Write)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Unable to write using band ordered IRasterIO when using "
                 "interleave 'ATTRIBUTES'.\n");
        return CE_Failure;
    }

    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));

    if (eBufType == eDataType && nXSize == nBufXSize && nYSize == nBufYSize &&
        nBufferDTSize > 0 && (nPixelSpace % nBufferDTSize) == 0 &&
        (nLineSpace % nBufferDTSize) == 0)
    {
        const uint64_t nBandIdx = poGDS->nBandStart + nBand - 1;
        std::vector<uint64_t> oaSubarray = {
            nBandIdx,        nBandIdx,
            (uint64_t)nYOff, (uint64_t)nYOff + nYSize - 1,
            (uint64_t)nXOff, (uint64_t)nXOff + nXSize - 1};
        if (poGDS->eIndexMode == PIXEL)
            std::rotate(oaSubarray.begin(), oaSubarray.begin() + 2,
                        oaSubarray.end());

        const bool bUseReadOnlyObjs =
            ((eRWFlag == GF_Read) && (eAccess == GA_Update) &&
             (poGDS->m_roArray));
        const auto &oCtxt = bUseReadOnlyObjs ? *poGDS->m_roCtx : *poGDS->m_ctx;
        const auto &oArray =
            bUseReadOnlyObjs ? *poGDS->m_roArray : *poGDS->m_array;

        auto poQuery = std::make_unique<tiledb::Query>(oCtxt, oArray);
        tiledb::Subarray subarray(oCtxt, oArray);
        if (poGDS->m_array->schema().domain().ndim() == 3)
        {
            subarray.set_subarray(oaSubarray);
        }
        else
        {
            subarray.set_subarray(std::vector<uint64_t>(oaSubarray.cbegin() + 2,
                                                        oaSubarray.cend()));
        }
        poQuery->set_subarray(subarray);

        SetBuffer(poQuery.get(), eDataType, osAttrName, pData, nXSize * nYSize);

        // write additional co-registered values
        std::vector<std::unique_ptr<void, decltype(&VSIFree)>> aBlocks;

        if (poGDS->lpoAttributeDS.size() > 0)
        {
            for (auto const &poAttrDS : poGDS->lpoAttributeDS)
            {
                GDALRasterBand *poAttrBand = poAttrDS->GetRasterBand(nBand);
                GDALDataType eAttrType = poAttrBand->GetRasterDataType();
                int nBytes = GDALGetDataTypeSizeBytes(eAttrType);
                size_t nValues = static_cast<size_t>(nBufXSize) * nBufYSize;
                void *pAttrBlock = VSIMalloc(nBytes * nValues);
                aBlocks.emplace_back(pAttrBlock, &VSIFree);

                if (pAttrBlock == nullptr)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Cannot allocate attribute buffer");
                    return CE_Failure;
                }

                poAttrBand->AdviseRead(nXOff, nYOff, nXSize, nYSize, nBufXSize,
                                       nBufYSize, eAttrType, nullptr);

                CPLErr eErr = poAttrBand->RasterIO(
                    GF_Read, nXOff, nYOff, nXSize, nYSize, pAttrBlock,
                    nBufXSize, nBufYSize, eAttrType, nPixelSpace, nLineSpace,
                    psExtraArg);

                if (eErr == CE_None)
                {
                    CPLString osName = CPLString().Printf(
                        "%s", CPLGetBasename(poAttrDS->GetDescription()));

                    SetBuffer(poQuery.get(), eAttrType, osName, pAttrBlock,
                              nBufXSize * nBufYSize);
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
/* ==================================================================== */
/*                     TileDBRasterDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                     ~TileDBRasterDataset()                           */
/************************************************************************/

TileDBRasterDataset::~TileDBRasterDataset()

{
    TileDBRasterDataset::FlushCache(true);

    try
    {
        if (m_array)
        {
            m_array->close();
        }
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
    }
    CPLDestroyXMLNode(psSubDatasetsTree);
    CSLDestroy(papszSubDatasets);
    CSLDestroy(papszAttributes);
}

/************************************************************************/
/*                           IRasterio()                                */
/************************************************************************/

CPLErr TileDBRasterDataset::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, int *panBandMap, GSpacing nPixelSpace, GSpacing nLineSpace,
    GSpacing nBandSpace, CPL_UNUSED GDALRasterIOExtraArg *psExtraArg)

{
    // support special case of writing attributes for bands, all attributes have to be set at once
    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));

    if (eIndexMode == ATTRIBUTES && nBandCount == nBands &&
        eBufType == eDataType && nXSize == nBufXSize && nYSize == nBufYSize &&
        nBufferDTSize > 0 && (nPixelSpace % nBufferDTSize) == 0 &&
        (nLineSpace % nBufferDTSize) == 0)
    {
        std::vector<uint64_t> oaSubarray = {
            (uint64_t)nYOff, (uint64_t)nYOff + nYSize - 1, (uint64_t)nXOff,
            (uint64_t)nXOff + nXSize - 1};

        const bool bUseReadOnlyObjs =
            ((eRWFlag == GF_Read) && (eAccess == GA_Update) && (m_roArray));
        const auto &oCtxt = bUseReadOnlyObjs ? *m_roCtx : *m_ctx;
        const auto &oArray = bUseReadOnlyObjs ? *m_roArray : *m_array;

        auto poQuery = std::make_unique<tiledb::Query>(oCtxt, oArray);
        tiledb::Subarray subarray(oCtxt, oArray);
        subarray.set_subarray(oaSubarray);
        poQuery->set_subarray(subarray);

        for (int b = 0; b < nBandCount; b++)
        {
            TileDBRasterBand *poBand =
                (TileDBRasterBand *)GetRasterBand(panBandMap[b]);
            int nRegionSize = nBufXSize * nBufYSize * nBufferDTSize;
            SetBuffer(poQuery.get(), eDataType, poBand->osAttrName,
                      ((GByte *)pData) + b * nRegionSize, nRegionSize);
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
    switch (eIndexMode)
    {
        case ATTRIBUTES:
            domain.add_dimensions(y, x);
            CreateAttribute(eDataType, pszAttrName, nBands);
            break;
        case PIXEL:
            assert(poBands);
            domain.add_dimensions(y, x, *poBands);
            CreateAttribute(eDataType, pszAttrName, 1);
            break;
        default:  // BAND
            assert(poBands);
            domain.add_dimensions(*poBands, y, x);
            CreateAttribute(eDataType, pszAttrName, 1);
            break;
    }

    return eErr;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr TileDBRasterDataset::FlushCache(bool bAtClosing)

{
    const CPLErr eErr = BlockBasedFlushCache(bAtClosing);

    if (nPamFlags & GPF_DIRTY)
        TrySaveXML();
    return eErr;
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
        tiledb::VFS vfs(*m_ctx, m_ctx->config());

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
            return CE_None;
        }

        if (psSubDatasetsTree != nullptr)
        {
            CPLAddXMLChild(psTree, CPLCloneXMLTree(psSubDatasetsTree->psChild));
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

        /* --------------------------------------------------------------------
         */
        /*      Try saving the auxiliary metadata. */
        /* --------------------------------------------------------------------
         */
        CPLErrorHandlerPusher oQuietError(CPLQuietErrorHandler);
        char *pszTree = CPLSerializeXMLTree(psTree);

        if (eAccess == GA_ReadOnly)
        {
            if (nTimestamp)
            {
                auto oMeta = std::unique_ptr<tiledb::Array>(new tiledb::Array(
                    *m_ctx, m_array->uri(), TILEDB_WRITE,
                    tiledb::TemporalPolicy(tiledb::TimeTravel, nTimestamp)));
                oMeta->put_metadata(GDAL_ATTRIBUTE_NAME, TILEDB_UINT8,
                                    static_cast<int>(strlen(pszTree)), pszTree);
                oMeta->close();
            }
            else
            {
                auto oMeta = std::unique_ptr<tiledb::Array>(
                    new tiledb::Array(*m_ctx, m_array->uri(), TILEDB_WRITE));
                oMeta->put_metadata(GDAL_ATTRIBUTE_NAME, TILEDB_UINT8,
                                    static_cast<int>(strlen(pszTree)), pszTree);
                oMeta->close();
            }
        }
        else
        {
            m_array->put_metadata("dataset_type", TILEDB_STRING_UTF8,
                                  static_cast<int>(strlen(RASTER_DATASET_TYPE)),
                                  RASTER_DATASET_TYPE);
            m_array->put_metadata(GDAL_ATTRIBUTE_NAME, TILEDB_UINT8,
                                  static_cast<int>(strlen(pszTree)), pszTree);
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

CPLErr TileDBRasterDataset::TryLoadXML(char **papszSiblingFiles)

{
    return TryLoadCachedXML(papszSiblingFiles, true);
}

/************************************************************************/
/*                           TryLoadCachedXML()                               */
/************************************************************************/

CPLErr TileDBRasterDataset::TryLoadCachedXML(char ** /*papszSiblingFiles*/,
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
                if ((eAccess == GA_Update) && (m_roArray))
                {
                    m_roArray->get_metadata(GDAL_ATTRIBUTE_NAME, &v_type,
                                            &v_num, &v_r);
                    if (v_r)
                    {
                        osMetaDoc =
                            CPLString(static_cast<const char *>(v_r), v_num);
                    }
                }
                else
                {
                    m_array->get_metadata(GDAL_ATTRIBUTE_NAME, &v_type, &v_num,
                                          &v_r);

                    if (v_r)
                    {
                        osMetaDoc =
                            CPLString(static_cast<const char *>(v_r), v_num);
                    }
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

        /* --------------------------------------------------------------------
         */
        /*      Initialize ourselves from this XML tree. */
        /* --------------------------------------------------------------------
         */

        CPLString osPath(CPLGetPath(psPam->pszPamFilename));
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
        char **papszMeta = CSLDuplicate(GDALPamDataset::GetMetadata(pszDomain));
        for (int i = 0; papszMeta && papszMeta[i]; i++)
        {
            if (STARTS_WITH(papszMeta[i], "SUBDATASET_") &&
                strstr(papszMeta[i], "_NAME="))
            {
                char *pszKey = nullptr;
                const char *pszAttr = CPLParseNameValue(papszMeta[i], &pszKey);
                if (pszAttr && !STARTS_WITH(pszAttr, "TILEDB:"))
                {
                    CPLString osAttr(pszAttr);
                    CPLFree(papszMeta[i]);
                    papszMeta[i] = CPLStrdup(
                        CPLString().Printf("%s=TILEDB:\"%s\":%s", pszKey,
                                           GetDescription(), osAttr.c_str()));
                }
                CPLFree(pszKey);
            }
        }
        m_osSubdatasetMD.Assign(papszMeta);
        return m_osSubdatasetMD.List();
    }
    else
    {
        return GDALPamDataset::GetMetadata(pszDomain);
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *TileDBRasterDataset::Open(GDALOpenInfo *poOpenInfo)

{
    auto poDS = std::make_unique<TileDBRasterDataset>();

    const char *pszConfig =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_CONFIG");

    const char *pszTimestamp =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_TIMESTAMP");

    poDS->bStats =
        CSLFetchBoolean(poOpenInfo->papszOpenOptions, "STATS", FALSE);

    if (pszConfig != nullptr)
    {
        tiledb::Config cfg(pszConfig);
        poDS->m_ctx.reset(new tiledb::Context(cfg));
    }
    else
    {
        poDS->m_ctx.reset(new tiledb::Context());
    }
    if (pszTimestamp)
        poDS->nTimestamp = std::strtoull(pszTimestamp, nullptr, 10);

    CPLString osArrayPath;
    CPLString osAux;
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

        osArrayPath = TileDBDataset::VSI_to_tiledb_uri(apszName[1]);
        osSubdataset = apszName[2];
        poDS->SetSubdatasetName(osSubdataset.c_str());
    }
    else
    {
        if (pszAttr != nullptr)
        {
            poDS->SetSubdatasetName(pszAttr);
        }

        osArrayPath = TileDBDataset::VSI_to_tiledb_uri(poOpenInfo->pszFilename);
    }

    const char *pszArrayName = CPLGetBasename(osArrayPath);
    osAux.Printf("%s.tdb", pszArrayName);

    // aux file is in array folder
    poDS->SetPhysicalFilename(CPLFormFilename(osArrayPath, osAux, nullptr));
    // Initialize any PAM information.
    poDS->SetDescription(osArrayPath);

    tiledb_query_type_t eMode = TILEDB_READ;
    if (poOpenInfo->eAccess == GA_Update)
    {
        eMode = TILEDB_WRITE;
        poDS->m_roCtx.reset(new tiledb::Context(poDS->m_ctx->config()));
        poDS->m_roArray.reset(
            new tiledb::Array(*poDS->m_roCtx, osArrayPath, TILEDB_READ));
    }

    if (poDS->nTimestamp)
    {
        if (eMode == TILEDB_READ)
        {
            poDS->m_array.reset(new tiledb::Array(
                *poDS->m_ctx, osArrayPath, TILEDB_READ,
                tiledb::TemporalPolicy(tiledb::TimeTravel, poDS->nTimestamp)));
        }
        else
        {
            poDS->m_array.reset(new tiledb::Array(
                *poDS->m_ctx, osArrayPath, TILEDB_WRITE,
                tiledb::TemporalPolicy(tiledb::TimeTravel, poDS->nTimestamp)));
        }
    }
    else
        poDS->m_array.reset(
            new tiledb::Array(*poDS->m_ctx, osArrayPath, eMode));

    poDS->eAccess = poOpenInfo->eAccess;

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
                    CPLString osDSName = CSLFetchNameValueDef(
                        poDS->papszSubDatasets, "SUBDATASET_1_NAME", "");
                    return (GDALDataset *)GDALOpen(osDSName,
                                                   poOpenInfo->eAccess);
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
                         osArrayPath.c_str());
                return nullptr;
            }
        }
    }

    // reload metadata now that bands are created to populate band metadata
    poDS->TryLoadCachedXML(nullptr, false);

    tiledb::VFS vfs(*poDS->m_ctx, poDS->m_ctx->config());

    if (!STARTS_WITH_CI(osArrayPath, "TILEDB:") && vfs.is_dir(osArrayPath))
        poDS->oOvManager.Initialize(poDS.get(), ":::VIRTUAL:::");
    else
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Overviews not supported for network writes.");

    return poDS.release();
}

/************************************************************************/
/*                              CreateAttribute()                       */
/************************************************************************/

CPLErr TileDBRasterDataset::CreateAttribute(GDALDataType eType,
                                            const CPLString &osAttrName,
                                            const int nSubRasterCount)
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
                ((bHasSubDatasets) && (nSubRasterCount > 1)))
            {
                osName = CPLString().Printf("%s_%d", osName.c_str(), i + 1);
            }

            switch (eType)
            {
                case GDT_Byte:
                {
                    m_schema->add_attribute(
                        tiledb::Attribute::create<unsigned char>(
                            *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 8;
                    break;
                }
                case GDT_Int8:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<int8_t>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 8;
                    break;
                }
                case GDT_UInt16:
                {
                    m_schema->add_attribute(
                        tiledb::Attribute::create<unsigned short>(
                            *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 16;
                    break;
                }
                case GDT_UInt32:
                {
                    m_schema->add_attribute(
                        tiledb::Attribute::create<unsigned int>(*m_ctx, osName,
                                                                *m_filterList));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_UInt64:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<uint64_t>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 64;
                    break;
                }
                case GDT_Int16:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<short>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 16;
                    break;
                }
                case GDT_Int32:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<int>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_Int64:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<int64_t>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 64;
                    break;
                }
                case GDT_Float32:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<float>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_Float64:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<double>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 64;
                    break;
                }
                case GDT_CInt16:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<short[2]>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 16;
                    break;
                }
                case GDT_CInt32:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<int[2]>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_CFloat32:
                {
                    m_schema->add_attribute(tiledb::Attribute::create<float[2]>(
                        *m_ctx, osName, *m_filterList));
                    nBitsPerSample = 32;
                    break;
                }
                case GDT_CFloat64:
                {
                    m_schema->add_attribute(
                        tiledb::Attribute::create<double[2]>(*m_ctx, osName,
                                                             *m_filterList));
                    nBitsPerSample = 64;
                    break;
                }
                default:
                    return CE_Failure;
            }

            if ((bHasSubDatasets) && (i == 0))
            {
                ++nSubDataCount;

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

                papszSubDatasets = CSLSetNameValue(
                    papszSubDatasets,
                    CPLString().Printf("SUBDATASET_%d_NAME", nSubDataCount),
                    CPLString().Printf("%s", osPrettyName.c_str()));

                papszSubDatasets = CSLSetNameValue(
                    papszSubDatasets,
                    CPLString().Printf("SUBDATASET_%d_DESC", nSubDataCount),
                    CPLString().Printf("[%s] %s (%s)", osDim.c_str(),
                                       osPrettyName.c_str(),
                                       GDALGetDataTypeName(eType)));

                // add to PAM metadata
                if (psSubDatasetsTree == nullptr)
                {
                    psSubDatasetsTree =
                        CPLCreateXMLNode(nullptr, CXT_Element, "PAMDataset");
                }

                CPLXMLNode *psSubNode = CPLCreateXMLNode(
                    psSubDatasetsTree, CXT_Element, "Subdataset");
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

                if (lpoAttributeDS.size() > 0)
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
                                       char **&papszOptions)

{
    int nX = 0;
    int nY = 0;
    poBand->GetBlockSize(&nX, &nY);

    if (CSLFetchNameValue(papszOptions, "BLOCKXSIZE") == nullptr)
    {
        papszOptions = CSLSetNameValue(papszOptions, "BLOCKXSIZE",
                                       CPLString().Printf("%d", nX));
    }

    if (CSLFetchNameValue(papszOptions, "BLOCKYSIZE") == nullptr)
    {
        papszOptions = CSLSetNameValue(papszOptions, "BLOCKYSIZE",
                                       CPLString().Printf("%d", nY));
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
                                                   char **papszOptions)
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
            tiledb::Config cfg(pszConfig);
            poDS->m_ctx.reset(new tiledb::Context(cfg));
        }
        else
        {
            poDS->m_ctx.reset(new tiledb::Context());
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

        CPLString osAux;
        const char *pszArrayName = CPLGetBasename(pszFilename);
        osAux.Printf("%s.tdb", pszArrayName);

        poDS->SetPhysicalFilename(
            CPLFormFilename(pszFilename, osAux.c_str(), nullptr));

        // Initialize PAM information.
        poDS->SetDescription(pszFilename);

        // this driver enforces that all subdatasets are the same size
        tiledb::Domain domain(*poDS->m_ctx);

        // Note the dimension bounds are inclusive and are expanded to the match
        // the block size
        poDS->nBlocksX = DIV_ROUND_UP(nXSize, poDS->nBlockXSize);
        poDS->nBlocksY = DIV_ROUND_UP(nYSize, poDS->nBlockYSize);

        uint64_t w = (uint64_t)poDS->nBlocksX * poDS->nBlockXSize - 1;
        uint64_t h = (uint64_t)poDS->nBlocksY * poDS->nBlockYSize - 1;

        auto d1 = tiledb::Dimension::create<uint64_t>(
            *poDS->m_ctx, "X", {0, w}, uint64_t(poDS->nBlockXSize));
        auto d2 = tiledb::Dimension::create<uint64_t>(
            *poDS->m_ctx, "Y", {0, h}, uint64_t(poDS->nBlockYSize));

        {
            // Only used for unit test purposes (to check ability of GDAL to read
            // an arbitrary array)
            const char *pszAttrName =
                CPLGetConfigOption("TILEDB_ATTRIBUTE", TILEDB_VALUES);
            if ((poDS->nBands == 0) || (poDS->eIndexMode == ATTRIBUTES))
            {
                poDS->AddDimensions(domain, pszAttrName, d2, d1, nullptr);
            }
            else
            {
                auto d3 = tiledb::Dimension::create<uint64_t>(
                    *poDS->m_ctx, "BANDS", {1, uint64_t(poDS->nBands)}, 1);
                poDS->AddDimensions(domain, pszAttrName, d2, d1, &d3);
            }
        }

        poDS->m_schema->set_domain(domain).set_order(
            {{TILEDB_ROW_MAJOR, TILEDB_ROW_MAJOR}});
        ;

        // register additional attributes to the pixel value, these will be
        // be reported as subdatasets on future reads
        poDS->papszAttributes =
            CSLFetchNameValueMultiple(papszOptions, "TILEDB_ATTRIBUTE");

        for (int i = 0; poDS->papszAttributes != nullptr &&
                        poDS->papszAttributes[i] != nullptr;
             i++)
        {
            // modeling additional attributes as subdatasets
            poDS->bHasSubDatasets = true;
            // check each attribute is a GDAL source
            std::unique_ptr<GDALDataset> poAttrDS(
                GDALDataset::Open(poDS->papszAttributes[i], GA_ReadOnly));

            if (poAttrDS != nullptr)
            {
                const char *pszAttrName =
                    CPLGetBasename(poAttrDS->GetDescription());
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
                        poDS->lpoAttributeDS.push_back(std::move(poAttrDS));
                        poDS->CreateAttribute(poAttrBand->GetRasterDataType(),
                                              pszAttrName, 1);
                    }
                    else
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Skipping %s as it has a different dimension\n",
                            poDS->papszAttributes[i]);
                    }
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Skipping %s as it doesn't have any bands\n",
                             poDS->papszAttributes[i]);
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Skipping %s, not recognized as a GDAL dataset\n",
                         poDS->papszAttributes[i]);
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
        poDstDS->bHasSubDatasets = true;
        char **papszSrcSubDatasets = poSrcDS->GetMetadata("SUBDATASETS");
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

        poDstDS->CreateAttribute(
            poSubDataset->GetRasterBand(1)->GetRasterDataType(), pszAttrName,
            poSubDataset->GetRasterCount());
        apoDatasets.push_back(std::move(poSubDataset));

        for (int i = 0; papszSrcSubDatasets[i] != nullptr; i++)
        {
            if (STARTS_WITH_CI(papszSrcSubDatasets[i], "SUBDATASET_1_NAME=") ||
                strstr(papszSrcSubDatasets[i], "_DESC="))
            {
                continue;
            }
            pszSubDSName = CPLParseNameValue(papszSrcSubDatasets[i], nullptr);
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

                if ((poSubDS->GetRasterXSize() != (int)nSubXSize) ||
                    (poSubDS->GetRasterYSize() != (int)nSubYSize) ||
                    (nBlockXSize != poDstDS->nBlockXSize) ||
                    (nBlockYSize != poDstDS->nBlockYSize))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Sub-datasets must have the same dimension,"
                             " and block sizes, skipping %s\n",
                             pszSubDSName);
                }
                else
                {
                    pszAttrName = apszTokens[2];
                    poDstDS->CreateAttribute(
                        poSubDS->GetRasterBand(1)->GetRasterDataType(),
                        pszAttrName, poSubDS->GetRasterCount());
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

        poDstDS->SetMetadata(poDstDS->papszSubDatasets, "SUBDATASETS");
        tiledb::Array::create(poDstDS->GetDescription(), *poDstDS->m_schema);

        if (poDstDS->nTimestamp)
        {
            poDstDS->m_array.reset(new tiledb::Array(
                *poDstDS->m_ctx, poDstDS->GetDescription(), TILEDB_WRITE,
                tiledb::TemporalPolicy(tiledb::TimeTravel,
                                       poDstDS->nTimestamp)));
        }
        else
            poDstDS->m_array.reset(new tiledb::Array(
                *poDstDS->m_ctx, poDstDS->GetDescription(), TILEDB_WRITE));

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
        int nTotalBlocks = poDstDS->nBlocksX * poDstDS->nBlocksY;

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
                    GDALDataType eDT =
                        poSubDS->GetRasterBand(1)->GetRasterDataType();

                    for (int b = 1; b <= poSubDS->GetRasterCount(); ++b)
                    {
                        int nBytes = GDALGetDataTypeSizeBytes(eDT);
                        int nValues = nBytes * poDstDS->nBlockXSize *
                                      poDstDS->nBlockYSize;
                        void *pBlock = VSIMalloc(nBytes * nValues);
                        aBlocks.emplace_back(pBlock, &VSIFree);
                        GDALRasterBand *poBand = poSubDS->GetRasterBand(b);
                        if (poBand->ReadBlock(i, j, pBlock) == CE_None)
                        {
                            SetBuffer(
                                &query, eDT,
                                poDstDS->m_schema->attribute(iAttr++).name(),
                                pBlock,
                                poDstDS->nBlockXSize * poDstDS->nBlockYSize);
                        }
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

                int nBlocks = ((j + 1) * poDstDS->nBlocksX);

                if (!pfnProgress(nBlocks / static_cast<double>(nTotalBlocks),
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

GDALDataset *TileDBRasterDataset::Create(const char *pszFilename, int nXSize,
                                         int nYSize, int nBandsIn,
                                         GDALDataType eType,
                                         char **papszOptions)

{
    CPLString osArrayPath = TileDBDataset::VSI_to_tiledb_uri(pszFilename);

    std::unique_ptr<TileDBRasterDataset> poDS(TileDBRasterDataset::CreateLL(
        osArrayPath, nXSize, nYSize, nBandsIn, eType, papszOptions));

    if (!poDS)
        return nullptr;

    tiledb::Array::create(osArrayPath, *poDS->m_schema);

    if (poDS->nTimestamp)
        poDS->m_array.reset(new tiledb::Array(
            *poDS->m_ctx, osArrayPath, TILEDB_WRITE,
            tiledb::TemporalPolicy(tiledb::TimeTravel, poDS->nTimestamp)));
    else
        poDS->m_array.reset(
            new tiledb::Array(*poDS->m_ctx, osArrayPath, TILEDB_WRITE));

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

    // Only used for unit test purposes (to check ability of GDAL to read
    // an arbitrary array)
    if (CPLTestBool(CPLGetConfigOption("TILEDB_WRITE_IMAGE_STRUCTURE", "YES")))
    {
        char **papszImageStruct = nullptr;
        papszImageStruct =
            CSLAddNameValue(papszImageStruct, "NBITS",
                            CPLString().Printf("%d", poDS->nBitsPerSample));
        papszImageStruct = CSLAddNameValue(
            papszImageStruct, "DATA_TYPE",
            CPLString().Printf("%s", GDALGetDataTypeName(poDS->eDataType)));
        papszImageStruct =
            CSLAddNameValue(papszImageStruct, "X_SIZE",
                            CPLString().Printf("%d", poDS->nRasterXSize));
        papszImageStruct =
            CSLAddNameValue(papszImageStruct, "Y_SIZE",
                            CPLString().Printf("%d", poDS->nRasterYSize));
        papszImageStruct = CSLAddNameValue(papszImageStruct, "INTERLEAVE",
                                           index_type_name(poDS->eIndexMode));
        papszImageStruct = CSLAddNameValue(papszImageStruct, "DATASET_TYPE",
                                           RASTER_DATASET_TYPE);

        if (poDS->lpoAttributeDS.size() > 0)
        {
            int i = 0;
            for (auto const &poAttrDS : poDS->lpoAttributeDS)
            {
                papszImageStruct = CSLAddNameValue(
                    papszImageStruct,
                    CPLString().Printf("TILEDB_ATTRIBUTE_%i", ++i),
                    CPLGetBasename(poAttrDS->GetDescription()));
            }
        }
        poDS->SetMetadata(papszImageStruct, "IMAGE_STRUCTURE");

        CSLDestroy(papszImageStruct);
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
    char **papszCopyOptions = CSLDuplicate(papszOptions);
    CPLString osArrayPath = TileDBDataset::VSI_to_tiledb_uri(pszFilename);

    std::unique_ptr<TileDBRasterDataset> poDstDS;

    if (CSLFetchNameValue(papszOptions, "APPEND_SUBDATASET"))
    {
        // TileDB schemas are fixed
        CPLError(CE_Failure, CPLE_NotSupported,
                 "TileDB driver does not support "
                 "appending to an existing schema.");
        CSLDestroy(papszCopyOptions);
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
                    CSLDestroy(papszCopyOptions);
                    return nullptr;
                }
            }

            poDstDS.reset(
                static_cast<TileDBRasterDataset *>(TileDBRasterDataset::Create(
                    osArrayPath, poSrcDS->GetRasterXSize(),
                    poSrcDS->GetRasterYSize(), nBands, eType, papszOptions)));

            if (!poDstDS)
            {
                CSLDestroy(papszCopyOptions);
                return nullptr;
            }

            CPLErr eErr =
                GDALDatasetCopyWholeRaster(poSrcDS, poDstDS.get(), papszOptions,
                                           pfnProgress, pProgressData);

            if (eErr != CE_None)
            {
                CPLError(eErr, CPLE_AppDefined,
                         "Error copying raster to TileDB.");
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "TileDB driver does not support "
                     "source dataset with zero bands.");
        }
    }
    else
    {
        if (bStrict)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "TileDB driver does not support copying "
                     "subdatasets in strict mode.");
            CSLDestroy(papszCopyOptions);
            return nullptr;
        }

        if (CSLFetchNameValue(papszOptions, "BLOCKXSIZE") ||
            CSLFetchNameValue(papszOptions, "BLOCKYSIZE"))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Changing block size is not supported when copying "
                     "subdatasets.");
            CSLDestroy(papszCopyOptions);
            return nullptr;
        }

        const int nSubDatasetCount = CSLCount(papszSrcSubDatasets) / 2;
        const int nMaxFiles =
            atoi(CPLGetConfigOption("GDAL_READDIR_LIMIT_ON_OPEN", "1000"));

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

                    TileDBRasterDataset::SetBlockSize(poBand, papszCopyOptions);
                    poDstDS.reset(TileDBRasterDataset::CreateLL(
                        osArrayPath, poBand->GetXSize(), poBand->GetYSize(), 0,
                        poBand->GetRasterDataType(), papszCopyOptions));

                    if (poDstDS && TileDBRasterDataset::CopySubDatasets(
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
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Please increase GDAL_READDIR_LIMIT_ON_OPEN variable.");
        }
    }

    CSLDestroy(papszCopyOptions);

    // TODO Supporting mask bands is a possible future task
    if (poDstDS != nullptr)
    {
        int nCloneFlags = GCIF_PAM_DEFAULT & ~GCIF_MASK;
        poDstDS->CloneInfo(poSrcDS, nCloneFlags);

        if (poDstDS->eIndexMode == ATTRIBUTES)
        {
            poDstDS->FlushCache(false);
        }

        poDstDS->m_array->close();
        poDstDS->eAccess = GA_ReadOnly;
        poDstDS->m_array->open(TILEDB_READ);

        return poDstDS.release();
    }
    return nullptr;
}
