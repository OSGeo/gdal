/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Basis Universal / KTX2 driver.
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
#include "ktx2drivercore.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

/************************************************************************/
/*                            KTX2Dataset                               */
/************************************************************************/

class KTX2Dataset final : public GDALPamDataset
{
    friend class KTX2RasterBand;

    basist::ktx2_transcoder m_transcoder{};
    basist::ktx2_transcoder &m_transcoderRef;
    bool m_bHasDecodeRun = false;
    void *m_pEncodedData = nullptr;
    void *m_pDecodedData = nullptr;
    uint32_t m_nLineStride = 0;
    uint32_t m_iLayer = 0;
    uint32_t m_iFace = 0;
    uint32_t m_iLevel = 0;
    std::vector<std::unique_ptr<KTX2Dataset>> m_apoOverviewsDS{};

    void *GetDecodedData(uint32_t &nLineStride);

    CPL_DISALLOW_COPY_ASSIGN(KTX2Dataset)

  public:
    ~KTX2Dataset() override;
    KTX2Dataset(uint32_t iLayer, uint32_t iFace, void *pEncodedData);
    KTX2Dataset(KTX2Dataset *poParent, uint32_t iLevel);

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

/************************************************************************/
/*                            KTX2RasterBand                            */
/************************************************************************/

class KTX2RasterBand final : public GDALPamRasterBand
{
  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage) override;

  public:
    KTX2RasterBand(KTX2Dataset *poDSIn, int nBandIn);

    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int nIdx) override;
};

/************************************************************************/
/*                           KTX2Dataset()                              */
/************************************************************************/

KTX2Dataset::KTX2Dataset(uint32_t iLayer, uint32_t iFace, void *pEncodedData)
    : m_transcoderRef(m_transcoder), m_pEncodedData(pEncodedData),
      m_iLayer(iLayer), m_iFace(iFace)
{
}

/************************************************************************/
/*                           KTX2Dataset()                              */
/************************************************************************/

KTX2Dataset::KTX2Dataset(KTX2Dataset *poParent, uint32_t iLevel)
    : m_transcoderRef(poParent->m_transcoderRef), m_iLayer(poParent->m_iLayer),
      m_iFace(poParent->m_iFace), m_iLevel(iLevel)
{
    basist::ktx2_image_level_info level_info;
    CPL_IGNORE_RET_VAL(m_transcoderRef.get_image_level_info(
        level_info, m_iLevel, m_iLayer, m_iFace));
    nRasterXSize = static_cast<int>(level_info.m_orig_width);
    nRasterYSize = static_cast<int>(level_info.m_orig_height);
}

/************************************************************************/
/*                           ~KTX2Dataset()                             */
/************************************************************************/

KTX2Dataset::~KTX2Dataset()
{
    VSIFree(m_pEncodedData);
    VSIFree(m_pDecodedData);
}

/************************************************************************/
/*                        GetDecodedData()                              */
/************************************************************************/

void *KTX2Dataset::GetDecodedData(uint32_t &nLineStride)
{
    if (m_bHasDecodeRun)
    {
        nLineStride = m_nLineStride;
        return m_pDecodedData;
    }
    m_bHasDecodeRun = true;

    GDALInitBasisUTranscoder();

    basist::ktx2_image_level_info level_info;
    if (!m_transcoderRef.get_image_level_info(level_info, m_iLevel, m_iLayer,
                                              m_iFace))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ktx2_transcoder::get_image_level_info() failed!");
        return nullptr;
    }

    if (!m_transcoderRef.start_transcoding())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ktx2_transcoder::start_transcoding() failed!");
        return nullptr;
    }

    m_pDecodedData = VSI_MALLOC3_VERBOSE(level_info.m_orig_width,
                                         level_info.m_orig_height, 4);
    if (m_pDecodedData == nullptr)
        return nullptr;

    constexpr basist::transcoder_texture_format transcoder_tex_fmt =
        basist::transcoder_texture_format::cTFRGBA32;
    if (!m_transcoderRef.transcode_image_level(
            m_iLevel, m_iLayer, m_iFace, m_pDecodedData,
            level_info.m_orig_width * level_info.m_orig_height * 4,
            transcoder_tex_fmt))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ktx2_transcoder::transcode_image_level() failed!");
        VSIFree(m_pDecodedData);
        m_pDecodedData = nullptr;
        return nullptr;
    }

    m_nLineStride = level_info.m_orig_width * 4;
    nLineStride = m_nLineStride;
    return m_pDecodedData;
}

/************************************************************************/
/*                           KTX2RasterBand()                           */
/************************************************************************/

KTX2RasterBand::KTX2RasterBand(KTX2Dataset *poDSIn, int nBandIn)
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

CPLErr KTX2RasterBand::IReadBlock(int /*nBlockXOff*/, int nBlockYOff,
                                  void *pImage)
{
    auto poGDS = cpl::down_cast<KTX2Dataset *>(poDS);
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

int KTX2RasterBand::GetOverviewCount()
{
    auto poGDS = cpl::down_cast<KTX2Dataset *>(poDS);
    return static_cast<int>(poGDS->m_apoOverviewsDS.size());
}

/************************************************************************/
/*                             GetOverview()                            */
/************************************************************************/

GDALRasterBand *KTX2RasterBand::GetOverview(int nIdx)
{
    if (nIdx < 0 || nIdx >= GetOverviewCount())
        return nullptr;
    auto poGDS = cpl::down_cast<KTX2Dataset *>(poDS);
    return poGDS->m_apoOverviewsDS[nIdx]->GetRasterBand(nBand);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *KTX2Dataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!KTX2DriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update)
        return nullptr;

    VSILFILE *fpL = nullptr;
    uint32_t nLayer = static_cast<uint32_t>(-1);
    uint32_t nFace = static_cast<uint32_t>(-1);
    if (STARTS_WITH(poOpenInfo->pszFilename, "KTX2:"))
    {
        const CPLStringList aosTokens(CSLTokenizeString2(
            poOpenInfo->pszFilename, ":", CSLT_HONOURSTRINGS));
        if (aosTokens.size() != 4)
            return nullptr;
        fpL = VSIFOpenL(aosTokens[1], "rb");
        if (fpL == nullptr)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", aosTokens[1]);
            return nullptr;
        }
        nLayer = static_cast<uint32_t>(atoi(aosTokens[2]));
        nFace = static_cast<uint32_t>(atoi(aosTokens[3]));
    }
    GIntBig nMaxSize = std::strtoull(
        CPLGetConfigOption("KTX2_MAX_FILE_SIZE", "0"), nullptr, 10);
    constexpr GIntBig KTX2_LIMIT = std::numeric_limits<uint32_t>::max();
    if (nMaxSize == 0 || nMaxSize > KTX2_LIMIT)
        nMaxSize = KTX2_LIMIT;
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

    auto poDS = std::make_unique<KTX2Dataset>(
        nLayer != static_cast<uint32_t>(-1) ? nLayer : 0,
        nFace != static_cast<uint32_t>(-1) ? nFace : 0, pabyRet);
    auto &transcoder = poDS->m_transcoder;
    const bool bInit = transcoder.init(pabyRet, nSize);
    if (!bInit)
    {
        if (nSize >= sizeof(basist::ktx2_header))
        {
#define DEBUG_u32(x)                                                           \
    CPLDebug("KTX2", #x " = %u",                                               \
             static_cast<uint32_t>(transcoder.get_header().m_##x))
            DEBUG_u32(vk_format);
            DEBUG_u32(type_size);
            DEBUG_u32(pixel_width);
            DEBUG_u32(pixel_height);
            DEBUG_u32(pixel_depth);
            DEBUG_u32(layer_count);
            DEBUG_u32(face_count);
            DEBUG_u32(level_count);
            DEBUG_u32(supercompression_scheme);
            DEBUG_u32(dfd_byte_offset);
            DEBUG_u32(dfd_byte_length);
        }
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ktx2_transcoder::init() failed! "
                 "File either uses an unsupported feature or is invalid");
        return nullptr;
    }

    const uint32_t nLayers =
        std::max(1U, transcoder.get_layers());  // get_layers() may return 0
    const uint32_t nFaces = transcoder.get_faces();
    CPLDebug("KTX2", "levels = %u, faces = %u, layers = %u",
             transcoder.get_levels(), nFaces, nLayers);

    switch (transcoder.get_format())
    {
        case basist::basis_tex_format::cETC1S:
            poDS->SetMetadataItem("COMPRESSION", "ETC1S", "IMAGE_STRUCTURE");
            break;
        case basist::basis_tex_format::cUASTC4x4:
            poDS->SetMetadataItem("COMPRESSION", "UASTC", "IMAGE_STRUCTURE");
            break;
    }

    if (nLayer == static_cast<uint32_t>(-1) && (nFaces >= 2 || nLayers >= 2))
    {
        CPLStringList aosSubdatasets;
        int nSubDS = 1;
        for (uint32_t iLayer = 0; iLayer < nLayers; ++iLayer)
        {
            for (uint32_t iFace = 0; iFace < nFaces; ++iFace)
            {
                aosSubdatasets.SetNameValue(
                    CPLSPrintf("SUBDATASET_%d_NAME", nSubDS),
                    CPLSPrintf("KTX2:\"%s\":%u:%u", poOpenInfo->pszFilename,
                               iLayer, iFace));
                aosSubdatasets.SetNameValue(
                    CPLSPrintf("SUBDATASET_%d_DESC", nSubDS),
                    CPLSPrintf("Layer %u, face %u of %s", iLayer, iFace,
                               poOpenInfo->pszFilename));
                nSubDS++;
            }
        }
        poDS->nRasterXSize = 0;
        poDS->nRasterYSize = 0;
        poDS->SetMetadata(aosSubdatasets.List(), "SUBDATASETS");

        poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);

        return poDS.release();
    }
    else if (nLayer != static_cast<uint32_t>(-1) && nLayer >= nLayers)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid layer number: %u",
                 nLayer);
        return nullptr;
    }
    else if (nFace != static_cast<uint32_t>(-1) && nFace >= nFaces)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid face number: %u", nFace);
        return nullptr;
    }

    poDS->nRasterXSize = transcoder.get_width();
    poDS->nRasterYSize = transcoder.get_height();

    const int l_nBands = 3 + (transcoder.get_has_alpha() ? 1 : 0);
    for (int i = 1; i <= l_nBands; ++i)
    {
        poDS->SetBand(i, new KTX2RasterBand(poDS.get(), i));
    }

    for (uint32_t level_index = 0; level_index < transcoder.get_levels();
         ++level_index)
    {
        basist::ktx2_image_level_info level_info;
        uint32_t layer_index = 0;
        uint32_t face_index = 0;
        if (transcoder.get_image_level_info(level_info, level_index,
                                            layer_index, face_index))
        {
            CPLDebug(
                "KTX2",
                "level %u: width=%u, orig_width=%u, height=%u, orig_height=%u",
                level_index, level_info.m_width, level_info.m_orig_width,
                level_info.m_height, level_info.m_orig_height);

            if (level_index > 0)
            {
                auto poOverviewDS =
                    std::make_unique<KTX2Dataset>(poDS.get(), level_index);
                for (int i = 1; i <= l_nBands; ++i)
                {
                    poOverviewDS->SetBand(
                        i, new KTX2RasterBand(poOverviewDS.get(), i));
                }
                poDS->m_apoOverviewsDS.emplace_back(std::move(poOverviewDS));
            }
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

GDALDataset *KTX2Dataset::CreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int /*bStrict*/,
                                     char **papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    if (!GDAL_KTX2_BASISU_CreateCopy(pszFilename, poSrcDS,
                                     true,  // bIsKTX2
                                     papszOptions, pfnProgress, pProgressData))
    {
        return nullptr;
    }
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                         GDALRegister_KTX2()                          */
/************************************************************************/

void GDALRegister_KTX2()
{
    if (GDALGetDriverByName(KTX2_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    KTX2DriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = KTX2Dataset::Open;
    poDriver->pfnCreateCopy = KTX2Dataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
