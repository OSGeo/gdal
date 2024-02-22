/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Basis Universal / BASISU driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_pam.h"
#include "common.h"
#include "include_basisu_sdk.h"
#include "basisudrivercore.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

/************************************************************************/
/*                           BASISUDataset                              */
/************************************************************************/

class BASISUDataset final : public GDALPamDataset
{
    friend class BASISURasterBand;

    basist::basisu_transcoder m_transcoder{};
    basist::basisu_transcoder &m_transcoderRef;
    bool m_bHasDecodeRun = false;
    void *m_pEncodedData = nullptr;
    uint32_t m_nEncodedDataSize = 0;
    void *m_pDecodedData = nullptr;
    uint32_t m_nLineStride = 0;
    BASISUDataset *m_poParent = nullptr;
    uint32_t m_iImageIdx = 0;
    uint32_t m_iLevel = 0;
    std::vector<std::unique_ptr<BASISUDataset>> m_apoOverviewsDS{};

    void *GetDecodedData(uint32_t &nLineStride);

    CPL_DISALLOW_COPY_ASSIGN(BASISUDataset)

  public:
    ~BASISUDataset() override;
    BASISUDataset(uint32_t iImageIdx, void *pEncodedData,
                  uint32_t nEncodedDataSize);
    BASISUDataset(BASISUDataset *poParent, uint32_t iLevel);

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

/************************************************************************/
/*                          BASISURasterBand                            */
/************************************************************************/

class BASISURasterBand final : public GDALPamRasterBand
{
  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage) override;

  public:
    BASISURasterBand(BASISUDataset *poDSIn, int nBandIn);

    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int nIdx) override;
};

/************************************************************************/
/*                           BASISUDataset()                            */
/************************************************************************/

BASISUDataset::BASISUDataset(uint32_t iImageIdx, void *pEncodedData,
                             uint32_t nEncodedDataSize)
    : m_transcoderRef(m_transcoder), m_pEncodedData(pEncodedData),
      m_nEncodedDataSize(nEncodedDataSize), m_iImageIdx(iImageIdx)
{
}

/************************************************************************/
/*                           BASISUDataset()                            */
/************************************************************************/

BASISUDataset::BASISUDataset(BASISUDataset *poParent, uint32_t iLevel)
    : m_transcoderRef(poParent->m_transcoderRef), m_poParent(poParent),
      m_iImageIdx(poParent->m_iImageIdx), m_iLevel(iLevel)
{
    basist::basisu_image_level_info level_info;
    CPL_IGNORE_RET_VAL(m_transcoderRef.get_image_level_info(
        m_poParent->m_pEncodedData, m_poParent->m_nEncodedDataSize, level_info,
        m_iImageIdx, m_iLevel));
    nRasterXSize = static_cast<int>(level_info.m_orig_width);
    nRasterYSize = static_cast<int>(level_info.m_orig_height);
}

/************************************************************************/
/*                           ~BASISUDataset()                           */
/************************************************************************/

BASISUDataset::~BASISUDataset()
{
    VSIFree(m_pEncodedData);
    VSIFree(m_pDecodedData);
}

/************************************************************************/
/*                        GetDecodedData()                              */
/************************************************************************/

void *BASISUDataset::GetDecodedData(uint32_t &nLineStride)
{
    if (m_bHasDecodeRun)
    {
        nLineStride = m_nLineStride;
        return m_pDecodedData;
    }
    m_bHasDecodeRun = true;

    GDALInitBasisUTranscoder();

    basist::basisu_image_level_info level_info;
    const auto poRefDS = m_poParent ? m_poParent : this;
    CPL_IGNORE_RET_VAL(m_transcoderRef.get_image_level_info(
        poRefDS->m_pEncodedData, poRefDS->m_nEncodedDataSize, level_info,
        m_iImageIdx, m_iLevel));

    if (!m_transcoderRef.start_transcoding(poRefDS->m_pEncodedData,
                                           poRefDS->m_nEncodedDataSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "basisu_transcoder::start_transcoding() failed!");
        return nullptr;
    }

    m_pDecodedData = VSI_MALLOC3_VERBOSE(level_info.m_orig_width,
                                         level_info.m_orig_height, 4);
    if (m_pDecodedData == nullptr)
        return nullptr;

    constexpr basist::transcoder_texture_format transcoder_tex_fmt =
        basist::transcoder_texture_format::cTFRGBA32;
    if (!m_transcoderRef.transcode_image_level(
            poRefDS->m_pEncodedData, poRefDS->m_nEncodedDataSize, m_iImageIdx,
            m_iLevel, m_pDecodedData,
            level_info.m_orig_width * level_info.m_orig_height * 4,
            transcoder_tex_fmt))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "basisu_transcoder::transcode_image_level() failed!");
        VSIFree(m_pDecodedData);
        m_pDecodedData = nullptr;
        return nullptr;
    }

    m_nLineStride = level_info.m_orig_width * 4;
    nLineStride = m_nLineStride;
    return m_pDecodedData;
}

/************************************************************************/
/*                           BASISURasterBand()                         */
/************************************************************************/

BASISURasterBand::BASISURasterBand(BASISUDataset *poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    nRasterXSize = poDSIn->GetRasterXSize();
    nRasterYSize = poDSIn->GetRasterYSize();
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;
    eDataType = GDT_Byte;
    SetColorInterpretation(
        static_cast<GDALColorInterp>(GCI_RedBand + nBandIn - 1));
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BASISURasterBand::IReadBlock(int /*nBlockXOff*/, int nBlockYOff,
                                    void *pImage)
{
    auto poGDS = cpl::down_cast<BASISUDataset *>(poDS);
    uint32_t nLineStride = 0;
    void *decoded_data = poGDS->GetDecodedData(nLineStride);
    if (decoded_data == nullptr)
        return CE_Failure;

    GDALCopyWords(static_cast<GByte *>(decoded_data) +
                      nBlockYOff * nLineStride + nBand - 1,
                  GDT_Byte, 4, pImage, GDT_Byte, 1, nBlockXSize);
    return CE_None;
}

/************************************************************************/
/*                           GetOverviewCount()                         */
/************************************************************************/

int BASISURasterBand::GetOverviewCount()
{
    auto poGDS = cpl::down_cast<BASISUDataset *>(poDS);
    return static_cast<int>(poGDS->m_apoOverviewsDS.size());
}

/************************************************************************/
/*                             GetOverview()                            */
/************************************************************************/

GDALRasterBand *BASISURasterBand::GetOverview(int nIdx)
{
    if (nIdx < 0 || nIdx >= GetOverviewCount())
        return nullptr;
    auto poGDS = cpl::down_cast<BASISUDataset *>(poDS);
    return poGDS->m_apoOverviewsDS[nIdx]->GetRasterBand(nBand);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BASISUDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!BASISUDriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update)
        return nullptr;

    VSILFILE *fpL = nullptr;
    uint32_t nImageIdx = static_cast<uint32_t>(-1);
    if (STARTS_WITH(poOpenInfo->pszFilename, "BASISU:"))
    {
        const CPLStringList aosTokens(CSLTokenizeString2(
            poOpenInfo->pszFilename, ":", CSLT_HONOURSTRINGS));
        if (aosTokens.size() != 3)
            return nullptr;
        fpL = VSIFOpenL(aosTokens[1], "rb");
        if (fpL == nullptr)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", aosTokens[1]);
            return nullptr;
        }
        nImageIdx = static_cast<uint32_t>(atoi(aosTokens[2]));
    }
    GIntBig nMaxSize = std::strtoull(
        CPLGetConfigOption("BASISU_MAX_FILE_SIZE", "0"), nullptr, 10);
    constexpr GIntBig BASISU_LIMIT = std::numeric_limits<uint32_t>::max();
    if (nMaxSize == 0 || nMaxSize > BASISU_LIMIT)
        nMaxSize = BASISU_LIMIT;
    GByte *pabyRet = nullptr;
    vsi_l_offset nSizeLarge = 0;
    int nRet = VSIIngestFile(fpL ? fpL : poOpenInfo->fpL, nullptr, &pabyRet,
                             &nSizeLarge, nMaxSize);
    if (fpL != nullptr)
        VSIFCloseL(fpL);
    if (!nRet)
    {
        return nullptr;
    }
    const uint32_t nSize = static_cast<uint32_t>(nSizeLarge);

    auto poDS = std::make_unique<BASISUDataset>(
        nImageIdx != static_cast<uint32_t>(-1) ? nImageIdx : 0, pabyRet, nSize);
    auto &transcoder = poDS->m_transcoder;
    basist::basisu_file_info file_info;
    if (!transcoder.get_file_info(pabyRet, nSize, file_info))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "basisu_transcoder::get_file_info() failed! "
                 "File either uses an unsupported feature or is invalid");
        return nullptr;
    }
    if (nImageIdx == static_cast<uint32_t>(-1) && file_info.m_total_images > 1)
    {
        CPLStringList aosSubdatasets;
        for (uint32_t iImageIdx = 0; iImageIdx < file_info.m_total_images;
             ++iImageIdx)
        {
            aosSubdatasets.SetNameValue(
                CPLSPrintf("SUBDATASET_%d_NAME", iImageIdx + 1),
                CPLSPrintf("BASISU:\"%s\":%u", poOpenInfo->pszFilename,
                           iImageIdx));
            aosSubdatasets.SetNameValue(
                CPLSPrintf("SUBDATASET_%d_DESC", iImageIdx + 1),
                CPLSPrintf("Image %u of %s", iImageIdx,
                           poOpenInfo->pszFilename));
        }
        poDS->nRasterXSize = 0;
        poDS->nRasterYSize = 0;
        poDS->SetMetadata(aosSubdatasets.List(), "SUBDATASETS");

        poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);

        return poDS.release();
    }

    basist::basisu_image_info image_info;
    if (!transcoder.get_image_info(pabyRet, nSize, image_info,
                                   poDS->m_iImageIdx))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "basisu_transcoder::get_image_info() failed");
        return nullptr;
    }
    poDS->nRasterXSize = static_cast<int>(image_info.m_orig_width);
    poDS->nRasterYSize = static_cast<int>(image_info.m_orig_height);

    switch (file_info.m_tex_format)
    {
        case basist::basis_tex_format::cETC1S:
            poDS->SetMetadataItem("COMPRESSION", "ETC1S", "IMAGE_STRUCTURE");
            break;
        case basist::basis_tex_format::cUASTC4x4:
            poDS->SetMetadataItem("COMPRESSION", "UASTC", "IMAGE_STRUCTURE");
            break;
    }

    const int l_nBands = 3 + (image_info.m_alpha_flag ? 1 : 0);
    for (int i = 1; i <= l_nBands; ++i)
    {
        poDS->SetBand(i, new BASISURasterBand(poDS.get(), i));
    }

    const uint32_t nLevels = file_info.m_image_mipmap_levels[poDS->m_iImageIdx];
    for (uint32_t level_index = 1; level_index < nLevels; ++level_index)
    {
        basist::basisu_image_level_info level_info;
        if (transcoder.get_image_level_info(pabyRet, nSize, level_info,
                                            poDS->m_iImageIdx, level_index))
        {
            auto poOverviewDS =
                std::make_unique<BASISUDataset>(poDS.get(), level_index);
            for (int i = 1; i <= l_nBands; ++i)
            {
                poOverviewDS->SetBand(
                    i, new BASISURasterBand(poOverviewDS.get(), i));
            }
            poDS->m_apoOverviewsDS.emplace_back(std::move(poOverviewDS));
        }
    }

    poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML(poOpenInfo->GetSiblingFiles());

    return poDS.release();
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

GDALDataset *BASISUDataset::CreateCopy(const char *pszFilename,
                                       GDALDataset *poSrcDS, int /*bStrict*/,
                                       char **papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void *pProgressData)
{
    if (!GDAL_KTX2_BASISU_CreateCopy(pszFilename, poSrcDS,
                                     false,  // bIsKTX2
                                     papszOptions, pfnProgress, pProgressData))
    {
        return nullptr;
    }
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                        GDALRegister_BASISU()                         */
/************************************************************************/

void GDALRegister_BASISU()
{
    if (GDALGetDriverByName(BASISU_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    BASISUDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = BASISUDataset::Open;
    poDriver->pfnCreateCopy = BASISUDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
