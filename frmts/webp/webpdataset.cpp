/******************************************************************************
 *
 * Project:  GDAL WEBP Driver
 * Purpose:  Implement GDAL WEBP Support based on libwebp
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include "webp_headers.h"
#include "webpdrivercore.h"

#include <limits>

/************************************************************************/
/* ==================================================================== */
/*                               WEBPDataset                            */
/* ==================================================================== */
/************************************************************************/

class WEBPRasterBand;

class WEBPDataset final : public GDALPamDataset
{
    friend class WEBPRasterBand;

    VSILFILE *fpImage;
    GByte *pabyUncompressed;
    int bHasBeenUncompressed;
    CPLErr eUncompressErrRet;
    CPLErr Uncompress();

    int bHasReadXMPMetadata;

  public:
    WEBPDataset();
    virtual ~WEBPDataset();

    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, int, int *, GSpacing nPixelSpace,
                             GSpacing nLineSpace, GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

    virtual char **GetMetadataDomainList() override;
    virtual char **GetMetadata(const char *pszDomain = "") override;

    CPLStringList GetCompressionFormats(int nXOff, int nYOff, int nXSize,
                                        int nYSize, int nBandCount,
                                        const int *panBandList) override;
    CPLErr ReadCompressedData(const char *pszFormat, int nXOff, int nYOff,
                              int nXSize, int nYSize, int nBandCount,
                              const int *panBandList, void **ppBuffer,
                              size_t *pnBufferSize,
                              char **ppszDetailedFormat) override;

    static GDALPamDataset *OpenPAM(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

/************************************************************************/
/* ==================================================================== */
/*                            WEBPRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class WEBPRasterBand final : public GDALPamRasterBand
{
    friend class WEBPDataset;

  public:
    WEBPRasterBand(WEBPDataset *, int);

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual GDALColorInterp GetColorInterpretation() override;
};

/************************************************************************/
/*                          WEBPRasterBand()                            */
/************************************************************************/

WEBPRasterBand::WEBPRasterBand(WEBPDataset *poDSIn, int)
{
    poDS = poDSIn;

    eDataType = GDT_Byte;

    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr WEBPRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
                                  void *pImage)
{
    WEBPDataset *poGDS = reinterpret_cast<WEBPDataset *>(poDS);

    if (poGDS->Uncompress() != CE_None)
        return CE_Failure;

    GByte *pabyUncompressed =
        &poGDS->pabyUncompressed[nBlockYOff * nRasterXSize * poGDS->nBands +
                                 nBand - 1];
    for (int i = 0; i < nRasterXSize; i++)
        reinterpret_cast<GByte *>(pImage)[i] =
            pabyUncompressed[poGDS->nBands * i];

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp WEBPRasterBand::GetColorInterpretation()

{
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
/*                             WEBPDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            WEBPDataset()                              */
/************************************************************************/

WEBPDataset::WEBPDataset()
    : fpImage(nullptr), pabyUncompressed(nullptr), bHasBeenUncompressed(FALSE),
      eUncompressErrRet(CE_None), bHasReadXMPMetadata(FALSE)
{
}

/************************************************************************/
/*                           ~WEBPDataset()                             */
/************************************************************************/

WEBPDataset::~WEBPDataset()

{
    FlushCache(true);
    if (fpImage)
        VSIFCloseL(fpImage);
    VSIFree(pabyUncompressed);
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **WEBPDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE, "xml:XMP", nullptr);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char **WEBPDataset::GetMetadata(const char *pszDomain)
{
    if ((pszDomain != nullptr && EQUAL(pszDomain, "xml:XMP")) &&
        !bHasReadXMPMetadata)
    {
        bHasReadXMPMetadata = TRUE;

        VSIFSeekL(fpImage, 12, SEEK_SET);

        bool bFirst = true;
        while (true)
        {
            char szHeader[5];
            GUInt32 nChunkSize;

            if (VSIFReadL(szHeader, 1, 4, fpImage) != 4 ||
                VSIFReadL(&nChunkSize, 1, 4, fpImage) != 4)
                break;

            szHeader[4] = '\0';
            CPL_LSBPTR32(&nChunkSize);

            if (bFirst)
            {
                if (strcmp(szHeader, "VP8X") != 0 || nChunkSize < 10)
                    break;

                int l_nFlags;
                if (VSIFReadL(&l_nFlags, 1, 4, fpImage) != 4)
                    break;
                CPL_LSBPTR32(&l_nFlags);
                if ((l_nFlags & 8) == 0)
                    break;

                VSIFSeekL(fpImage, nChunkSize - 4, SEEK_CUR);

                bFirst = false;
            }
            else if (strcmp(szHeader, "META") == 0)
            {
                if (nChunkSize > 1024 * 1024)
                    break;

                char *pszXMP =
                    reinterpret_cast<char *>(VSIMalloc(nChunkSize + 1));
                if (pszXMP == nullptr)
                    break;

                if (static_cast<GUInt32>(VSIFReadL(pszXMP, 1, nChunkSize,
                                                   fpImage)) != nChunkSize)
                {
                    VSIFree(pszXMP);
                    break;
                }
                pszXMP[nChunkSize] = '\0';

                /* Avoid setting the PAM dirty bit just for that */
                const int nOldPamFlags = nPamFlags;

                char *apszMDList[2] = {pszXMP, nullptr};
                SetMetadata(apszMDList, "xml:XMP");

                nPamFlags = nOldPamFlags;

                VSIFree(pszXMP);
                break;
            }
            else
                VSIFSeekL(fpImage, nChunkSize, SEEK_CUR);
        }
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                            Uncompress()                              */
/************************************************************************/

CPLErr WEBPDataset::Uncompress()
{
    if (bHasBeenUncompressed)
        return eUncompressErrRet;

    bHasBeenUncompressed = TRUE;
    eUncompressErrRet = CE_Failure;

    // To avoid excessive memory allocation attempts
    // Normally WebP images are no larger than 16383x16383*4 ~= 1 GB
    if (nRasterXSize > INT_MAX / (nRasterYSize * nBands))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Too large image");
        return CE_Failure;
    }

    pabyUncompressed = reinterpret_cast<GByte *>(
        VSIMalloc3(nRasterXSize, nRasterYSize, nBands));
    if (pabyUncompressed == nullptr)
        return CE_Failure;

    VSIFSeekL(fpImage, 0, SEEK_END);
    vsi_l_offset nSizeLarge = VSIFTellL(fpImage);
    if (nSizeLarge !=
        static_cast<vsi_l_offset>(static_cast<uint32_t>(nSizeLarge)))
        return CE_Failure;
    VSIFSeekL(fpImage, 0, SEEK_SET);
    uint32_t nSize = static_cast<uint32_t>(nSizeLarge);
    uint8_t *pabyCompressed = reinterpret_cast<uint8_t *>(VSIMalloc(nSize));
    if (pabyCompressed == nullptr)
        return CE_Failure;
    VSIFReadL(pabyCompressed, 1, nSize, fpImage);
    uint8_t *pRet;

    if (nBands == 4)
        pRet = WebPDecodeRGBAInto(pabyCompressed, static_cast<uint32_t>(nSize),
                                  static_cast<uint8_t *>(pabyUncompressed),
                                  static_cast<size_t>(nRasterXSize) *
                                      nRasterYSize * nBands,
                                  nRasterXSize * nBands);
    else
        pRet = WebPDecodeRGBInto(pabyCompressed, static_cast<uint32_t>(nSize),
                                 static_cast<uint8_t *>(pabyUncompressed),
                                 static_cast<size_t>(nRasterXSize) *
                                     nRasterYSize * nBands,
                                 nRasterXSize * nBands);

    VSIFree(pabyCompressed);
    if (pRet == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPDecodeRGBInto() failed");
        return CE_Failure;
    }
    eUncompressErrRet = CE_None;

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr WEBPDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                              int nXSize, int nYSize, void *pData,
                              int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, int nBandCount,
                              int *panBandMap, GSpacing nPixelSpace,
                              GSpacing nLineSpace, GSpacing nBandSpace,
                              GDALRasterIOExtraArg *psExtraArg)

{
    if ((eRWFlag == GF_Read) && (nBandCount == nBands) && (nXOff == 0) &&
        (nYOff == 0) && (nXSize == nBufXSize) && (nXSize == nRasterXSize) &&
        (nYSize == nBufYSize) && (nYSize == nRasterYSize) &&
        (eBufType == GDT_Byte) && (pData != nullptr) && (panBandMap[0] == 1) &&
        (panBandMap[1] == 2) && (panBandMap[2] == 3) &&
        (nBands == 3 || panBandMap[3] == 4))
    {
        if (Uncompress() != CE_None)
            return CE_Failure;
        if (nPixelSpace == nBands && nLineSpace == (nPixelSpace * nXSize) &&
            nBandSpace == 1)
        {
            memcpy(pData, pabyUncompressed,
                   static_cast<size_t>(nBands) * nXSize * nYSize);
        }
        else
        {
            for (int y = 0; y < nYSize; ++y)
            {
                GByte *pabyScanline = pabyUncompressed + y * nBands * nXSize;
                for (int x = 0; x < nXSize; ++x)
                {
                    for (int iBand = 0; iBand < nBands; iBand++)
                        reinterpret_cast<GByte *>(
                            pData)[(y * nLineSpace) + (x * nPixelSpace) +
                                   iBand * nBandSpace] =
                            pabyScanline[x * nBands + iBand];
                }
            }
        }

        return CE_None;
    }

    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nBandCount, panBandMap, nPixelSpace,
                                     nLineSpace, nBandSpace, psExtraArg);
}

/************************************************************************/
/*                       GetCompressionFormats()                        */
/************************************************************************/

CPLStringList WEBPDataset::GetCompressionFormats(int nXOff, int nYOff,
                                                 int nXSize, int nYSize,
                                                 int nBandCount,
                                                 const int *panBandList)
{
    CPLStringList aosRet;
    if (nXOff == 0 && nYOff == 0 && nXSize == nRasterXSize &&
        nYSize == nRasterYSize && IsAllBands(nBandCount, panBandList))
    {
        aosRet.AddString("WEBP");
    }
    return aosRet;
}

/************************************************************************/
/*                       ReadCompressedData()                           */
/************************************************************************/

CPLErr WEBPDataset::ReadCompressedData(const char *pszFormat, int nXOff,
                                       int nYOff, int nXSize, int nYSize,
                                       int nBandCount, const int *panBandList,
                                       void **ppBuffer, size_t *pnBufferSize,
                                       char **ppszDetailedFormat)
{
    if (nXOff == 0 && nYOff == 0 && nXSize == nRasterXSize &&
        nYSize == nRasterYSize && IsAllBands(nBandCount, panBandList))
    {
        const CPLStringList aosTokens(CSLTokenizeString2(pszFormat, ";", 0));
        if (aosTokens.size() != 1)
            return CE_Failure;

        if (EQUAL(aosTokens[0], "WEBP"))
        {
            if (ppszDetailedFormat)
                *ppszDetailedFormat = VSIStrdup("WEBP");
            VSIFSeekL(fpImage, 0, SEEK_END);
            const auto nFileSize = VSIFTellL(fpImage);
            if (nFileSize > std::numeric_limits<uint32_t>::max())
                return CE_Failure;
            auto nSize = static_cast<uint32_t>(nFileSize);
            if (ppBuffer)
            {
                if (!pnBufferSize)
                    return CE_Failure;
                bool bFreeOnError = false;
                if (*ppBuffer)
                {
                    if (*pnBufferSize < nSize)
                        return CE_Failure;
                }
                else
                {
                    *ppBuffer = VSI_MALLOC_VERBOSE(nSize);
                    if (*ppBuffer == nullptr)
                        return CE_Failure;
                    bFreeOnError = true;
                }
                VSIFSeekL(fpImage, 0, SEEK_SET);
                if (VSIFReadL(*ppBuffer, nSize, 1, fpImage) != 1)
                {
                    if (bFreeOnError)
                    {
                        VSIFree(*ppBuffer);
                        *ppBuffer = nullptr;
                    }
                    return CE_Failure;
                }

                // Remove META box
                if (nSize > 12 && memcmp(*ppBuffer, "RIFF", 4) == 0)
                {
                    size_t nPos = 12;
                    GByte *pabyData = static_cast<GByte *>(*ppBuffer);
                    while (nPos <= nSize - 8)
                    {
                        char szBoxName[5] = {0, 0, 0, 0, 0};
                        memcpy(szBoxName, pabyData + nPos, 4);
                        uint32_t nChunkSize;
                        memcpy(&nChunkSize, pabyData + nPos + 4, 4);
                        CPL_LSBPTR32(&nChunkSize);
                        if (nChunkSize % 2)  // Payload padding if needed
                            nChunkSize++;
                        if (nChunkSize > nSize - (nPos + 8))
                            break;
                        if (memcmp(szBoxName, "META", 4) == 0)
                        {
                            CPLDebug("WEBP",
                                     "Remove existing %s box from "
                                     "source compressed data",
                                     szBoxName);
                            if (nPos + 8 + nChunkSize < nSize)
                            {
                                memmove(pabyData + nPos,
                                        pabyData + nPos + 8 + nChunkSize,
                                        nSize - (nPos + 8 + nChunkSize));
                            }
                            nSize -= 8 + nChunkSize;
                        }
                        else
                        {
                            nPos += 8 + nChunkSize;
                        }
                    }

                    // Patch size of RIFF
                    uint32_t nSize32 = nSize - 8;
                    CPL_LSBPTR32(&nSize32);
                    memcpy(pabyData + 4, &nSize32, 4);
                }
            }
            if (pnBufferSize)
                *pnBufferSize = nSize;
            return CE_None;
        }
    }
    return CE_Failure;
}

/************************************************************************/
/*                          OpenPAM()                                   */
/************************************************************************/

GDALPamDataset *WEBPDataset::OpenPAM(GDALOpenInfo *poOpenInfo)

{
    if (!WEBPDriverIdentify(poOpenInfo) || poOpenInfo->fpL == nullptr)
        return nullptr;

    int nWidth, nHeight;
    if (!WebPGetInfo(reinterpret_cast<const uint8_t *>(poOpenInfo->pabyHeader),
                     static_cast<uint32_t>(poOpenInfo->nHeaderBytes), &nWidth,
                     &nHeight))
        return nullptr;

    int nBands = 3;

    auto poDS = std::make_unique<WEBPDataset>();

#if WEBP_DECODER_ABI_VERSION >= 0x0002
    WebPDecoderConfig config;
    if (!WebPInitDecoderConfig(&config))
        return nullptr;

    const bool bOK =
        WebPGetFeatures(poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes,
                        &config.input) == VP8_STATUS_OK;

    // Cf commit https://github.com/webmproject/libwebp/commit/86c0031eb2c24f78d4dcfc5dab752ebc9f511607#diff-859d219dccb3163cc11cd538effed461ff0145135070abfe70bd263f16408023
    // Added in webp 0.4.0
#if WEBP_DECODER_ABI_VERSION >= 0x0202
    poDS->GDALDataset::SetMetadataItem(
        "COMPRESSION_REVERSIBILITY",
        config.input.format == 2 ? "LOSSLESS" : "LOSSY", "IMAGE_STRUCTURE");
#endif

    if (config.input.has_alpha)
        nBands = 4;

    WebPFreeDecBuffer(&config.output);

    if (!bOK)
        return nullptr;

#endif

    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The WEBP driver does not support update access to existing"
                 " datasets.\n");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    for (int iBand = 0; iBand < nBands; iBand++)
        poDS->SetBand(iBand + 1, new WEBPRasterBand(poDS.get(), iBand + 1));

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);

    poDS->TryLoadXML(poOpenInfo->GetSiblingFiles());

    /* -------------------------------------------------------------------- */
    /*      Open overviews.                                                 */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename,
                                poOpenInfo->GetSiblingFiles());

    return poDS.release();
}

/************************************************************************/
/*                             Open()                                   */
/************************************************************************/

GDALDataset *WEBPDataset::Open(GDALOpenInfo *poOpenInfo)

{
    return OpenPAM(poOpenInfo);
}

/************************************************************************/
/*                              WebPUserData                            */
/************************************************************************/

typedef struct
{
    VSILFILE *fp;
    GDALProgressFunc pfnProgress;
    void *pProgressData;
} WebPUserData;

/************************************************************************/
/*                         WEBPDatasetWriter()                          */
/************************************************************************/

static int WEBPDatasetWriter(const uint8_t *data, size_t data_size,
                             const WebPPicture *const picture)
{
    WebPUserData *pUserData =
        reinterpret_cast<WebPUserData *>(picture->custom_ptr);
    return VSIFWriteL(data, 1, data_size, pUserData->fp) == data_size;
}

/************************************************************************/
/*                        WEBPDatasetProgressHook()                     */
/************************************************************************/

#if WEBP_ENCODER_ABI_VERSION >= 0x0100
static int WEBPDatasetProgressHook(int percent,
                                   const WebPPicture *const picture)
{
    WebPUserData *pUserData =
        reinterpret_cast<WebPUserData *>(picture->custom_ptr);
    return pUserData->pfnProgress(percent / 100.0, nullptr,
                                  pUserData->pProgressData);
}
#endif

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *WEBPDataset::CreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int bStrict,
                                     char **papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)

{
    const char *pszLossLessCopy =
        CSLFetchNameValueDef(papszOptions, "LOSSLESS_COPY", "AUTO");
    if (EQUAL(pszLossLessCopy, "AUTO") || CPLTestBool(pszLossLessCopy))
    {
        void *pWEBPContent = nullptr;
        size_t nWEBPContent = 0;
        if (poSrcDS->ReadCompressedData(
                "WEBP", 0, 0, poSrcDS->GetRasterXSize(),
                poSrcDS->GetRasterYSize(), poSrcDS->GetRasterCount(), nullptr,
                &pWEBPContent, &nWEBPContent, nullptr) == CE_None)
        {
            CPLDebug("WEBP", "Lossless copy from source dataset");
            std::vector<GByte> abyData;
            try
            {
                abyData.assign(static_cast<const GByte *>(pWEBPContent),
                               static_cast<const GByte *>(pWEBPContent) +
                                   nWEBPContent);

                char **papszXMP = poSrcDS->GetMetadata("xml:XMP");
                if (papszXMP && papszXMP[0])
                {
                    GByte abyChunkHeader[8];
                    memcpy(abyChunkHeader, "META", 4);
                    const size_t nXMPSize = strlen(papszXMP[0]);
                    uint32_t nChunkSize = static_cast<uint32_t>(nXMPSize);
                    CPL_LSBPTR32(&nChunkSize);
                    memcpy(abyChunkHeader + 4, &nChunkSize, 4);
                    abyData.insert(abyData.end(), abyChunkHeader,
                                   abyChunkHeader + sizeof(abyChunkHeader));
                    abyData.insert(
                        abyData.end(), reinterpret_cast<GByte *>(papszXMP[0]),
                        reinterpret_cast<GByte *>(papszXMP[0]) + nXMPSize);
                    if ((abyData.size() % 2) != 0)  // Payload padding if needed
                        abyData.push_back(0);

                    // Patch size of RIFF
                    uint32_t nSize32 =
                        static_cast<uint32_t>(abyData.size()) - 8;
                    CPL_LSBPTR32(&nSize32);
                    memcpy(abyData.data() + 4, &nSize32, 4);
                }
            }
            catch (const std::exception &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Exception occurred: %s",
                         e.what());
                abyData.clear();
            }
            VSIFree(pWEBPContent);

            if (!abyData.empty())
            {
                VSILFILE *fpImage = VSIFOpenL(pszFilename, "wb");
                if (fpImage == nullptr)
                {
                    CPLError(CE_Failure, CPLE_OpenFailed,
                             "Unable to create jpeg file %s.", pszFilename);

                    return nullptr;
                }
                if (VSIFWriteL(abyData.data(), 1, abyData.size(), fpImage) !=
                    abyData.size())
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Failure writing data: %s", VSIStrerror(errno));
                    VSIFCloseL(fpImage);
                    return nullptr;
                }
                if (VSIFCloseL(fpImage) != 0)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Failure writing data: %s", VSIStrerror(errno));
                    return nullptr;
                }

                pfnProgress(1.0, nullptr, pProgressData);

                // Re-open file and clone missing info to PAM
                GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
                auto poDS = OpenPAM(&oOpenInfo);
                if (poDS)
                {
                    poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);
                }

                return poDS;
            }
        }
    }

    const bool bLossless = CPLFetchBool(papszOptions, "LOSSLESS", false);
    if (!bLossless &&
        (!EQUAL(pszLossLessCopy, "AUTO") && CPLTestBool(pszLossLessCopy)))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "LOSSLESS_COPY=YES requested but not possible");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      WEBP library initialization                                     */
    /* -------------------------------------------------------------------- */

    WebPPicture sPicture;
    if (!WebPPictureInit(&sPicture))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureInit() failed");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Some some rudimentary checks                                    */
    /* -------------------------------------------------------------------- */

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    if (nXSize > 16383 || nYSize > 16383)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WEBP maximum image dimensions are 16383 x 16383.");

        return nullptr;
    }

    const int nBands = poSrcDS->GetRasterCount();
    if (nBands != 3
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
        && nBands != 4
#endif
    )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WEBP driver doesn't support %d bands. Must be 3 (RGB) "
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
                 "or 4 (RGBA) "
#endif
                 "bands.",
                 nBands);

        return nullptr;
    }

    const GDALDataType eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    if (eDT != GDT_Byte)
    {
        CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                 "WEBP driver doesn't support data type %s. "
                 "Only eight bit byte bands supported.",
                 GDALGetDataTypeName(
                     poSrcDS->GetRasterBand(1)->GetRasterDataType()));

        if (bStrict)
            return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      What options has the user selected?                             */
    /* -------------------------------------------------------------------- */
    float fQuality = 75.0f;
    const char *pszQUALITY = CSLFetchNameValue(papszOptions, "QUALITY");
    if (pszQUALITY != nullptr)
    {
        fQuality = static_cast<float>(CPLAtof(pszQUALITY));
        if (fQuality < 0.0f || fQuality > 100.0f)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "%s=%s is not a legal value.",
                     "QUALITY", pszQUALITY);
            return nullptr;
        }
    }

    WebPPreset nPreset = WEBP_PRESET_DEFAULT;
    const char *pszPRESET =
        CSLFetchNameValueDef(papszOptions, "PRESET", "DEFAULT");
    if (EQUAL(pszPRESET, "DEFAULT"))
        nPreset = WEBP_PRESET_DEFAULT;
    else if (EQUAL(pszPRESET, "PICTURE"))
        nPreset = WEBP_PRESET_PICTURE;
    else if (EQUAL(pszPRESET, "PHOTO"))
        nPreset = WEBP_PRESET_PHOTO;
    else if (EQUAL(pszPRESET, "PICTURE"))
        nPreset = WEBP_PRESET_PICTURE;
    else if (EQUAL(pszPRESET, "DRAWING"))
        nPreset = WEBP_PRESET_DRAWING;
    else if (EQUAL(pszPRESET, "ICON"))
        nPreset = WEBP_PRESET_ICON;
    else if (EQUAL(pszPRESET, "TEXT"))
        nPreset = WEBP_PRESET_TEXT;
    else
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "%s=%s is not a legal value.",
                 "PRESET", pszPRESET);
        return nullptr;
    }

    WebPConfig sConfig;
    if (!WebPConfigInitInternal(&sConfig, nPreset, fQuality,
                                WEBP_ENCODER_ABI_VERSION))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPConfigInit() failed");
        return nullptr;
    }

    // TODO: Get rid of this macro in a reasonable way.
#define FETCH_AND_SET_OPTION_INT(name, fieldname, minval, maxval)              \
    {                                                                          \
        const char *pszVal = CSLFetchNameValue(papszOptions, name);            \
        if (pszVal != nullptr)                                                 \
        {                                                                      \
            sConfig.fieldname = atoi(pszVal);                                  \
            if (sConfig.fieldname < minval || sConfig.fieldname > maxval)      \
            {                                                                  \
                CPLError(CE_Failure, CPLE_IllegalArg,                          \
                         "%s=%s is not a legal value.", name, pszVal);         \
                return nullptr;                                                \
            }                                                                  \
        }                                                                      \
    }

    FETCH_AND_SET_OPTION_INT("TARGETSIZE", target_size, 0, INT_MAX - 1);

    const char *pszPSNR = CSLFetchNameValue(papszOptions, "PSNR");
    if (pszPSNR)
    {
        sConfig.target_PSNR = static_cast<float>(CPLAtof(pszPSNR));
        if (sConfig.target_PSNR < 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "PSNR=%s is not a legal value.", pszPSNR);
            return nullptr;
        }
    }

    FETCH_AND_SET_OPTION_INT("METHOD", method, 0, 6);
    FETCH_AND_SET_OPTION_INT("SEGMENTS", segments, 1, 4);
    FETCH_AND_SET_OPTION_INT("SNS_STRENGTH", sns_strength, 0, 100);
    FETCH_AND_SET_OPTION_INT("FILTER_STRENGTH", filter_strength, 0, 100);
    FETCH_AND_SET_OPTION_INT("FILTER_SHARPNESS", filter_sharpness, 0, 7);
    FETCH_AND_SET_OPTION_INT("FILTER_TYPE", filter_type, 0, 1);
    FETCH_AND_SET_OPTION_INT("AUTOFILTER", autofilter, 0, 1);
    FETCH_AND_SET_OPTION_INT("PASS", pass, 1, 10);
    FETCH_AND_SET_OPTION_INT("PREPROCESSING", preprocessing, 0, 1);
    FETCH_AND_SET_OPTION_INT("PARTITIONS", partitions, 0, 3);
#if WEBP_ENCODER_ABI_VERSION >= 0x0002
    FETCH_AND_SET_OPTION_INT("PARTITION_LIMIT", partition_limit, 0, 100);
#endif
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
    sConfig.lossless = bLossless;
    if (sConfig.lossless)
        sPicture.use_argb = 1;
#endif
#if WEBP_ENCODER_ABI_VERSION >= 0x0209
    FETCH_AND_SET_OPTION_INT("EXACT", exact, 0, 1);
#endif

    if (!WebPValidateConfig(&sConfig))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPValidateConfig() failed");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate memory                                                 */
    /* -------------------------------------------------------------------- */
    GByte *pabyBuffer =
        reinterpret_cast<GByte *>(VSI_MALLOC3_VERBOSE(nBands, nXSize, nYSize));
    if (pabyBuffer == nullptr)
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the dataset.                                             */
    /* -------------------------------------------------------------------- */
    VSILFILE *fpImage = VSIFOpenL(pszFilename, "wb");
    if (fpImage == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Unable to create WEBP file %s.\n", pszFilename);
        VSIFree(pabyBuffer);
        return nullptr;
    }

    WebPUserData sUserData;
    sUserData.fp = fpImage;
    sUserData.pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    sUserData.pProgressData = pProgressData;

    /* -------------------------------------------------------------------- */
    /*      WEBP library settings                                           */
    /* -------------------------------------------------------------------- */

    sPicture.width = nXSize;
    sPicture.height = nYSize;
    sPicture.writer = WEBPDatasetWriter;
    sPicture.custom_ptr = &sUserData;
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
    sPicture.progress_hook = WEBPDatasetProgressHook;
#endif
    if (!WebPPictureAlloc(&sPicture))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureAlloc() failed");
        VSIFree(pabyBuffer);
        VSIFCloseL(fpImage);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Acquire source imagery.                                         */
    /* -------------------------------------------------------------------- */
    CPLErr eErr =
        poSrcDS->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pabyBuffer, nXSize,
                          nYSize, GDT_Byte, nBands, nullptr, nBands,
                          static_cast<GSpacing>(nBands) * nXSize, 1, nullptr);

/* -------------------------------------------------------------------- */
/*      Import and write to file                                        */
/* -------------------------------------------------------------------- */
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
    if (eErr == CE_None && nBands == 4)
    {
        if (!WebPPictureImportRGBA(&sPicture, pabyBuffer, nBands * nXSize))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "WebPPictureImportRGBA() failed");
            eErr = CE_Failure;
        }
    }
    else
#endif
        if (eErr == CE_None &&
            !WebPPictureImportRGB(&sPicture, pabyBuffer, nBands * nXSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "WebPPictureImportRGB() failed");
        eErr = CE_Failure;
    }

    if (eErr == CE_None && !WebPEncode(&sConfig, &sPicture))
    {
#if WEBP_ENCODER_ABI_VERSION >= 0x0100
        const char *pszErrorMsg = nullptr;
        switch (sPicture.error_code)
        {
            case VP8_ENC_ERROR_OUT_OF_MEMORY:
                pszErrorMsg = "Out of memory";
                break;
            case VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY:
                pszErrorMsg = "Out of memory while flushing bits";
                break;
            case VP8_ENC_ERROR_NULL_PARAMETER:
                pszErrorMsg = "A pointer parameter is NULL";
                break;
            case VP8_ENC_ERROR_INVALID_CONFIGURATION:
                pszErrorMsg = "Configuration is invalid";
                break;
            case VP8_ENC_ERROR_BAD_DIMENSION:
                pszErrorMsg = "Picture has invalid width/height";
                break;
            case VP8_ENC_ERROR_PARTITION0_OVERFLOW:
                pszErrorMsg = "Partition is bigger than 512k. Try using less "
                              "SEGMENTS, or increase PARTITION_LIMIT value";
                break;
            case VP8_ENC_ERROR_PARTITION_OVERFLOW:
                pszErrorMsg = "Partition is bigger than 16M";
                break;
            case VP8_ENC_ERROR_BAD_WRITE:
                pszErrorMsg = "Error while flushing bytes";
                break;
            case VP8_ENC_ERROR_FILE_TOO_BIG:
                pszErrorMsg = "File is bigger than 4G";
                break;
            case VP8_ENC_ERROR_USER_ABORT:
                pszErrorMsg = "User interrupted";
                break;
            default:
                CPLError(CE_Failure, CPLE_AppDefined,
                         "WebPEncode returned an unknown error code: %d",
                         sPicture.error_code);
                pszErrorMsg = "Unknown WebP error type.";
                break;
        }
        CPLError(CE_Failure, CPLE_AppDefined, "WebPEncode() failed : %s",
                 pszErrorMsg);
#else
        CPLError(CE_Failure, CPLE_AppDefined, "WebPEncode() failed");
#endif
        eErr = CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup and close.                                              */
    /* -------------------------------------------------------------------- */
    CPLFree(pabyBuffer);

    WebPPictureFree(&sPicture);

    VSIFCloseL(fpImage);

    if (pfnProgress)
        pfnProgress(1.0, "", pProgressData);

    if (eErr != CE_None)
    {
        VSIUnlink(pszFilename);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Re-open dataset, and copy any auxiliary pam information.        */
    /* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);

    /* If writing to stdout, we can't reopen it, so return */
    /* a fake dataset to make the caller happy */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    auto poDS = WEBPDataset::OpenPAM(&oOpenInfo);
    CPLPopErrorHandler();
    if (poDS)
    {
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);
        return poDS;
    }

    return nullptr;
}

/************************************************************************/
/*                         GDALRegister_WEBP()                          */
/************************************************************************/

void GDALRegister_WEBP()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    WEBPDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = WEBPDataset::Open;
    poDriver->pfnCreateCopy = WEBPDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
