/******************************************************************************
 *
 * Project:  JPEG-XL Driver
 * Purpose:  Implement GDAL JPEG-XL Support based on libjxl
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

#include "cpl_error.h"
#include "gdalexif.h"
#include "gdaljp2metadata.h"
#include "gdaljp2abstractdataset.h"
#include "gdalorienteddataset.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>

#include "jxl_headers.h"

#include "jpegxldrivercore.h"

namespace
{
struct VSILFileReleaser
{
    void operator()(VSILFILE *fp)
    {
        if (fp)
            VSIFCloseL(fp);
    }
};
}  // namespace

/************************************************************************/
/*                        JPEGXLDataset                                 */
/************************************************************************/

class JPEGXLDataset final : public GDALJP2AbstractDataset
{
    friend class JPEGXLRasterBand;

    VSILFILE *m_fp = nullptr;
    JxlDecoderPtr m_decoder{};
#ifdef HAVE_JXL_THREADS
    JxlResizableParallelRunnerPtr m_parallelRunner{};
#endif
    bool m_bDecodingFailed = false;
    std::vector<GByte> m_abyImage{};
    std::vector<std::vector<GByte>> m_abyExtraChannels{};
    std::vector<GByte> m_abyInputData{};
    int m_nBits = 0;
    int m_nNonAlphaExtraChannels = 0;
#ifdef HAVE_JXL_BOX_API
    std::string m_osXMP{};
    char *m_apszXMP[2] = {nullptr, nullptr};
    std::vector<GByte> m_abyEXIFBox{};
    CPLStringList m_aosEXIFMetadata{};
    bool m_bHasJPEGReconstructionData = false;
    std::string m_osJPEGData{};
#endif

    bool Open(GDALOpenInfo *poOpenInfo);

    void GetDecodedImage(void *pabyOutputData, size_t nOutputDataSize);

  protected:
    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, int, int *, GSpacing, GSpacing, GSpacing,
                     GDALRasterIOExtraArg *psExtraArg) override;

  public:
    ~JPEGXLDataset();

    char **GetMetadataDomainList() override;
    char **GetMetadata(const char *pszDomain) override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override;

    CPLStringList GetCompressionFormats(int nXOff, int nYOff, int nXSize,
                                        int nYSize, int nBandCount,
                                        const int *panBandList) override;
    CPLErr ReadCompressedData(const char *pszFormat, int nXOff, int nYOff,
                              int nXSize, int nYSize, int nBandCount,
                              const int *panBandList, void **ppBuffer,
                              size_t *pnBufferSize,
                              char **ppszDetailedFormat) override;

    const std::vector<GByte> &GetDecodedImage();

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALPamDataset *OpenStaticPAM(GDALOpenInfo *poOpenInfo);
    static GDALDataset *OpenStatic(GDALOpenInfo *poOpenInfo);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

/************************************************************************/
/*                      JPEGXLRasterBand                                */
/************************************************************************/

class JPEGXLRasterBand final : public GDALPamRasterBand
{
  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override;

    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, GSpacing, GSpacing,
                     GDALRasterIOExtraArg *psExtraArg) override;

  public:
    JPEGXLRasterBand(JPEGXLDataset *poDSIn, int nBandIn,
                     GDALDataType eDataTypeIn, int nBitsPerSample,
                     GDALColorInterp eInterp);
};

/************************************************************************/
/*                         ~JPEGXLDataset()                             */
/************************************************************************/

JPEGXLDataset::~JPEGXLDataset()
{
    if (m_fp)
        VSIFCloseL(m_fp);
}

/************************************************************************/
/*                         JPEGXLRasterBand()                           */
/************************************************************************/

JPEGXLRasterBand::JPEGXLRasterBand(JPEGXLDataset *poDSIn, int nBandIn,
                                   GDALDataType eDataTypeIn, int nBitsPerSample,
                                   GDALColorInterp eInterp)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDataTypeIn;
    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
    SetColorInterpretation(eInterp);
    if ((eDataType == GDT_Byte && nBitsPerSample < 8) ||
        (eDataType == GDT_UInt16 && nBitsPerSample < 16))
    {
        SetMetadataItem("NBITS", CPLSPrintf("%d", nBitsPerSample),
                        "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPEGXLRasterBand::IReadBlock(int /*nBlockXOff*/, int nBlockYOff,
                                    void *pData)
{
    auto poGDS = cpl::down_cast<JPEGXLDataset *>(poDS);

    const auto &abyDecodedImage = poGDS->GetDecodedImage();
    if (abyDecodedImage.empty())
    {
        return CE_Failure;
    }

    const auto nDataSize = GDALGetDataTypeSizeBytes(eDataType);
    const int nNonExtraBands = poGDS->nBands - poGDS->m_nNonAlphaExtraChannels;
    if (nBand <= nNonExtraBands)
    {
        GDALCopyWords(abyDecodedImage.data() +
                          ((nBand - 1) + static_cast<size_t>(nBlockYOff) *
                                             nRasterXSize * nNonExtraBands) *
                              nDataSize,
                      eDataType, nDataSize * nNonExtraBands, pData, eDataType,
                      nDataSize, nRasterXSize);
    }
    else
    {
        const uint32_t nIndex = nBand - 1 - nNonExtraBands;
        memcpy(pData,
               poGDS->m_abyExtraChannels[nIndex].data() +
                   static_cast<size_t>(nBlockYOff) * nRasterXSize * nDataSize,
               nRasterXSize * nDataSize);
    }

    return CE_None;
}

/************************************************************************/
/*                         Identify()                                   */
/************************************************************************/

int JPEGXLDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->fpL == nullptr)
        return false;

    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "jxl"))
        return true;

    // See
    // https://github.com/libjxl/libjxl/blob/c98f133f3f5e456caaa2ba00bc920e923b713abc/lib/jxl/decode.cc#L107-L138

    // JPEG XL codestream
    if (poOpenInfo->nHeaderBytes >= 2 && poOpenInfo->pabyHeader[0] == 0xff &&
        poOpenInfo->pabyHeader[1] == 0x0a)
    {
        // Two bytes is not enough to reliably identify, so let's try to decode
        // basic info
        auto decoder = JxlDecoderMake(nullptr);
        if (!decoder)
            return false;
        JxlDecoderStatus status =
            JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO);
        if (status != JXL_DEC_SUCCESS)
        {
            return false;
        }

        status = JxlDecoderSetInput(decoder.get(), poOpenInfo->pabyHeader,
                                    poOpenInfo->nHeaderBytes);
        if (status != JXL_DEC_SUCCESS)
        {
            return false;
        }

        status = JxlDecoderProcessInput(decoder.get());
        if (status != JXL_DEC_BASIC_INFO)
        {
            return false;
        }

        return true;
    }

    return IsJPEGXLContainer(poOpenInfo);
}

/************************************************************************/
/*                             Open()                                   */
/************************************************************************/

bool JPEGXLDataset::Open(GDALOpenInfo *poOpenInfo)
{
    m_decoder = JxlDecoderMake(nullptr);
    if (!m_decoder)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlDecoderMake() failed");
        return false;
    }

#ifdef HAVE_JXL_THREADS
    m_parallelRunner = JxlResizableParallelRunnerMake(nullptr);
    if (!m_parallelRunner)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlResizableParallelRunnerMake() failed");
        return false;
    }

    if (JxlDecoderSetParallelRunner(m_decoder.get(), JxlResizableParallelRunner,
                                    m_parallelRunner.get()) != JXL_DEC_SUCCESS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlDecoderSetParallelRunner() failed");
        return false;
    }
#endif

    JxlDecoderStatus status =
        JxlDecoderSubscribeEvents(m_decoder.get(), JXL_DEC_BASIC_INFO |
#ifdef HAVE_JXL_BOX_API
                                                       JXL_DEC_BOX |
#endif
                                                       JXL_DEC_COLOR_ENCODING);
    if (status != JXL_DEC_SUCCESS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlDecoderSubscribeEvents() failed");
        return false;
    }

    JxlBasicInfo info;
    memset(&info, 0, sizeof(info));
    bool bGotInfo = false;

    // Steal file handle
    m_fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    VSIFSeekL(m_fp, 0, SEEK_SET);

    m_abyInputData.resize(1024 * 1024);

#ifdef HAVE_JXL_BOX_API
    JxlDecoderSetDecompressBoxes(m_decoder.get(), TRUE);
    std::vector<GByte> abyBoxBuffer(1024 * 1024);
    std::string osCurrentBox;
    std::vector<GByte> abyJumbBoxBuffer;
    const auto ProcessCurrentBox = [&]()
    {
        const size_t nRemainingBytesInBuffer =
            JxlDecoderReleaseBoxBuffer(m_decoder.get());
        CPLAssert(nRemainingBytesInBuffer < abyBoxBuffer.size());
        if (osCurrentBox == "xml " && m_osXMP.empty())
        {
            std::string osXML(reinterpret_cast<char *>(abyBoxBuffer.data()),
                              abyBoxBuffer.size() - nRemainingBytesInBuffer);
            if (osXML.compare(0, strlen("<?xpacket"), "<?xpacket") == 0)
            {
                m_osXMP = std::move(osXML);
            }
        }
        else if (osCurrentBox == "Exif" && m_aosEXIFMetadata.empty())
        {
            const size_t nSize = abyBoxBuffer.size() - nRemainingBytesInBuffer;
            // The first 4 bytes are at 0, before the TIFF EXIF file content
            if (nSize > 12 && abyBoxBuffer[0] == 0 && abyBoxBuffer[1] == 0 &&
                abyBoxBuffer[2] == 0 && abyBoxBuffer[3] == 0 &&
                (abyBoxBuffer[4] == 0x4d  // TIFF_BIGENDIAN
                 || abyBoxBuffer[4] == 0x49 /* TIFF_LITTLEENDIAN */))
            {
                m_abyEXIFBox.insert(m_abyEXIFBox.end(), abyBoxBuffer.data() + 4,
                                    abyBoxBuffer.data() + nSize);
#ifdef CPL_LSB
                const bool bSwab = abyBoxBuffer[4] == 0x4d;
#else
                const bool bSwab = abyBoxBuffer[4] == 0x49;
#endif
                constexpr int nTIFFHEADER = 0;
                uint32_t nTiffDirStart;
                memcpy(&nTiffDirStart, abyBoxBuffer.data() + 8,
                       sizeof(uint32_t));
                if (bSwab)
                {
                    CPL_LSBPTR32(&nTiffDirStart);
                }
                const std::string osTmpFilename =
                    CPLSPrintf("/vsimem/jxl/%p", this);
                VSILFILE *fpEXIF = VSIFileFromMemBuffer(
                    osTmpFilename.c_str(), abyBoxBuffer.data() + 4,
                    abyBoxBuffer.size() - 4, false);
                int nExifOffset = 0;
                int nInterOffset = 0;
                int nGPSOffset = 0;
                char **papszEXIFMetadata = nullptr;
                EXIFExtractMetadata(papszEXIFMetadata, fpEXIF, nTiffDirStart,
                                    bSwab, nTIFFHEADER, nExifOffset,
                                    nInterOffset, nGPSOffset);

                if (nExifOffset > 0)
                {
                    EXIFExtractMetadata(papszEXIFMetadata, fpEXIF, nExifOffset,
                                        bSwab, nTIFFHEADER, nExifOffset,
                                        nInterOffset, nGPSOffset);
                }
                if (nInterOffset > 0)
                {
                    EXIFExtractMetadata(papszEXIFMetadata, fpEXIF, nInterOffset,
                                        bSwab, nTIFFHEADER, nExifOffset,
                                        nInterOffset, nGPSOffset);
                }
                if (nGPSOffset > 0)
                {
                    EXIFExtractMetadata(papszEXIFMetadata, fpEXIF, nGPSOffset,
                                        bSwab, nTIFFHEADER, nExifOffset,
                                        nInterOffset, nGPSOffset);
                }
                VSIFCloseL(fpEXIF);
                m_aosEXIFMetadata.Assign(papszEXIFMetadata,
                                         /*takeOwnership=*/true);
            }
        }
        else if (osCurrentBox == "jumb")
        {
            abyJumbBoxBuffer = abyBoxBuffer;
        }
        osCurrentBox.clear();
    };

    // Process input to get boxes and basic info
    const uint64_t nMaxBoxBufferSize = std::strtoull(
        CPLGetConfigOption("GDAL_JPEGXL_MAX_BOX_BUFFER_SIZE", "100000000"),
        nullptr, 10);
#endif

    int l_nBands = 0;
    GDALDataType eDT = GDT_Unknown;

    while (true)
    {
        status = JxlDecoderProcessInput(m_decoder.get());

#ifdef HAVE_JXL_BOX_API
        if ((status == JXL_DEC_SUCCESS || status == JXL_DEC_BOX) &&
            !osCurrentBox.empty())
        {
            try
            {
                ProcessCurrentBox();
            }
            catch (const std::exception &)
            {
                CPLError(CE_Warning, CPLE_OutOfMemory,
                         "Not enough memory to read box '%s'",
                         osCurrentBox.c_str());
            }
        }
#endif

        if (status == JXL_DEC_SUCCESS)
        {
            break;
        }
        else if (status == JXL_DEC_NEED_MORE_INPUT)
        {
            JxlDecoderReleaseInput(m_decoder.get());

            const size_t nRead = VSIFReadL(m_abyInputData.data(), 1,
                                           m_abyInputData.size(), m_fp);
            if (nRead == 0)
            {
#ifdef HAVE_JXL_BOX_API
                JxlDecoderCloseInput(m_decoder.get());
#endif
                break;
            }
            if (JxlDecoderSetInput(m_decoder.get(), m_abyInputData.data(),
                                   nRead) != JXL_DEC_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderSetInput() failed");
                return false;
            }
#ifdef HAVE_JXL_BOX_API
            if (nRead < m_abyInputData.size())
            {
                JxlDecoderCloseInput(m_decoder.get());
            }
#endif
        }
        else if (status == JXL_DEC_BASIC_INFO)
        {
            bGotInfo = true;
            status = JxlDecoderGetBasicInfo(m_decoder.get(), &info);
            if (status != JXL_DEC_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderGetBasicInfo() failed");
                return false;
            }

            if (info.xsize > static_cast<uint32_t>(INT_MAX) ||
                info.ysize > static_cast<uint32_t>(INT_MAX))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too big raster");
                return false;
            }

            nRasterXSize = static_cast<int>(info.xsize);
            nRasterYSize = static_cast<int>(info.ysize);

            m_nBits = info.bits_per_sample;
            if (info.exponent_bits_per_sample == 0)
            {
                if (info.bits_per_sample <= 8)
                    eDT = GDT_Byte;
                else if (info.bits_per_sample <= 16)
                    eDT = GDT_UInt16;
            }
            else if (info.exponent_bits_per_sample == 8)
            {
                eDT = GDT_Float32;
            }
            if (eDT == GDT_Unknown)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Unhandled data type");
                return false;
            }

            l_nBands = static_cast<int>(info.num_color_channels) +
                       static_cast<int>(info.num_extra_channels);
            if (info.num_extra_channels == 1 &&
                (info.num_color_channels == 1 ||
                 info.num_color_channels == 3) &&
                info.alpha_bits != 0)
            {
                m_nNonAlphaExtraChannels = 0;
            }
            else
            {
                m_nNonAlphaExtraChannels =
                    static_cast<int>(info.num_extra_channels);
            }
        }
#ifdef HAVE_JXL_BOX_API
        else if (status == JXL_DEC_BOX)
        {
            osCurrentBox.clear();
            JxlBoxType type = {0};
            if (JxlDecoderGetBoxType(m_decoder.get(), type,
                                     /* decompressed = */ TRUE) !=
                JXL_DEC_SUCCESS)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "JxlDecoderGetBoxType() failed");
                continue;
            }
            char szType[5] = {0};
            memcpy(szType, type, sizeof(type));
            // CPLDebug("JPEGXL", "box: %s", szType);
            if (strcmp(szType, "xml ") == 0 || strcmp(szType, "Exif") == 0 ||
                strcmp(szType, "jumb") == 0)
            {
                uint64_t nRawSize = 0;
                JxlDecoderGetBoxSizeRaw(m_decoder.get(), &nRawSize);
                if (nRawSize > nMaxBoxBufferSize)
                {
                    CPLError(
                        CE_Warning, CPLE_OutOfMemory,
                        "Reading a '%s' box involves at least " CPL_FRMT_GUIB
                        " bytes, "
                        "but the current limitation of the "
                        "GDAL_JPEGXL_MAX_BOX_BUFFER_SIZE "
                        "configuration option is " CPL_FRMT_GUIB " bytes",
                        szType, static_cast<GUIntBig>(nRawSize),
                        static_cast<GUIntBig>(nMaxBoxBufferSize));
                    continue;
                }
                if (nRawSize > abyBoxBuffer.size())
                {
                    if (nRawSize > std::numeric_limits<size_t>::max() / 2)
                    {
                        CPLError(CE_Warning, CPLE_OutOfMemory,
                                 "Not enough memory to read box '%s'", szType);
                        continue;
                    }
                    try
                    {
                        abyBoxBuffer.clear();
                        abyBoxBuffer.resize(static_cast<size_t>(nRawSize));
                    }
                    catch (const std::exception &)
                    {
                        abyBoxBuffer.resize(1024 * 1024);
                        CPLError(CE_Warning, CPLE_OutOfMemory,
                                 "Not enough memory to read box '%s'", szType);
                        continue;
                    }
                }

                if (JxlDecoderSetBoxBuffer(m_decoder.get(), abyBoxBuffer.data(),
                                           abyBoxBuffer.size()) !=
                    JXL_DEC_SUCCESS)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "JxlDecoderSetBoxBuffer() failed");
                    continue;
                }
                osCurrentBox = szType;
            }
            else if (strcmp(szType, "jbrd") == 0)
            {
                m_bHasJPEGReconstructionData = true;
            }
        }
#endif
        else if (status == JXL_DEC_COLOR_ENCODING)
        {
#ifdef HAVE_JxlDecoderDefaultPixelFormat
            JxlPixelFormat format = {
                static_cast<uint32_t>(nBands),
                eDT == GDT_Byte     ? JXL_TYPE_UINT8
                : eDT == GDT_UInt16 ? JXL_TYPE_UINT16
                                    : JXL_TYPE_FLOAT,
                JXL_NATIVE_ENDIAN, 0 /* alignment */
            };
#endif

            bool bIsDefaultColorEncoding = false;
            JxlColorEncoding color_encoding;

            // Check if the color profile is the default one we set on creation.
            // If so, do not expose it as ICC color profile
            if (JXL_DEC_SUCCESS == JxlDecoderGetColorAsEncodedProfile(
                                       m_decoder.get(),
#ifdef HAVE_JxlDecoderDefaultPixelFormat
                                       &format,
#endif
                                       JXL_COLOR_PROFILE_TARGET_DATA,
                                       &color_encoding))
            {
                JxlColorEncoding default_color_encoding;
                JxlColorEncodingSetToSRGB(&default_color_encoding,
                                          info.num_color_channels ==
                                              1 /*is_gray*/);

                bIsDefaultColorEncoding =
                    color_encoding.color_space ==
                        default_color_encoding.color_space &&
                    color_encoding.white_point ==
                        default_color_encoding.white_point &&
                    color_encoding.white_point_xy[0] ==
                        default_color_encoding.white_point_xy[0] &&
                    color_encoding.white_point_xy[1] ==
                        default_color_encoding.white_point_xy[1] &&
                    (color_encoding.color_space == JXL_COLOR_SPACE_GRAY ||
                     color_encoding.color_space == JXL_COLOR_SPACE_XYB ||
                     (color_encoding.primaries ==
                          default_color_encoding.primaries &&
                      color_encoding.primaries_red_xy[0] ==
                          default_color_encoding.primaries_red_xy[0] &&
                      color_encoding.primaries_red_xy[1] ==
                          default_color_encoding.primaries_red_xy[1] &&
                      color_encoding.primaries_green_xy[0] ==
                          default_color_encoding.primaries_green_xy[0] &&
                      color_encoding.primaries_green_xy[1] ==
                          default_color_encoding.primaries_green_xy[1] &&
                      color_encoding.primaries_blue_xy[0] ==
                          default_color_encoding.primaries_blue_xy[0] &&
                      color_encoding.primaries_blue_xy[1] ==
                          default_color_encoding.primaries_blue_xy[1])) &&
                    color_encoding.transfer_function ==
                        default_color_encoding.transfer_function &&
                    color_encoding.gamma == default_color_encoding.gamma &&
                    color_encoding.rendering_intent ==
                        default_color_encoding.rendering_intent;
            }

            if (!bIsDefaultColorEncoding)
            {
                size_t icc_size = 0;
                if (JXL_DEC_SUCCESS ==
                    JxlDecoderGetICCProfileSize(m_decoder.get(),
#ifdef HAVE_JxlDecoderDefaultPixelFormat
                                                &format,
#endif
                                                JXL_COLOR_PROFILE_TARGET_DATA,
                                                &icc_size))
                {
                    std::vector<GByte> icc(icc_size);
                    if (JXL_DEC_SUCCESS == JxlDecoderGetColorAsICCProfile(
                                               m_decoder.get(),
#ifdef HAVE_JxlDecoderDefaultPixelFormat
                                               &format,
#endif
                                               JXL_COLOR_PROFILE_TARGET_DATA,
                                               icc.data(), icc_size))
                    {
                        // Escape the profile.
                        char *pszBase64Profile = CPLBase64Encode(
                            static_cast<int>(icc.size()), icc.data());

                        // Set ICC profile metadata.
                        SetMetadataItem("SOURCE_ICC_PROFILE", pszBase64Profile,
                                        "COLOR_PROFILE");

                        CPLFree(pszBase64Profile);
                    }
                }
            }
        }
#ifdef HAVE_JXL_BOX_API
        else if (status == JXL_DEC_BOX_NEED_MORE_OUTPUT)
        {
            // Grow abyBoxBuffer if it is too small
            const size_t nRemainingBytesInBuffer =
                JxlDecoderReleaseBoxBuffer(m_decoder.get());
            const size_t nBytesUsed =
                abyBoxBuffer.size() - nRemainingBytesInBuffer;
            if (abyBoxBuffer.size() > std::numeric_limits<size_t>::max() / 2)
            {
                CPLError(CE_Warning, CPLE_OutOfMemory,
                         "Not enough memory to read box '%s'",
                         osCurrentBox.c_str());
                osCurrentBox.clear();
                continue;
            }
            const size_t nNewBoxBufferSize = abyBoxBuffer.size() * 2;
            if (nNewBoxBufferSize > nMaxBoxBufferSize)
            {
                CPLError(CE_Warning, CPLE_OutOfMemory,
                         "Reading a '%s' box involves at least " CPL_FRMT_GUIB
                         " bytes, "
                         "but the current limitation of the "
                         "GDAL_JPEGXL_MAX_BOX_BUFFER_SIZE "
                         "configuration option is " CPL_FRMT_GUIB " bytes",
                         osCurrentBox.c_str(),
                         static_cast<GUIntBig>(nNewBoxBufferSize),
                         static_cast<GUIntBig>(nMaxBoxBufferSize));
                osCurrentBox.clear();
                continue;
            }
            try
            {
                abyBoxBuffer.resize(nNewBoxBufferSize);
            }
            catch (const std::exception &)
            {
                CPLError(CE_Warning, CPLE_OutOfMemory,
                         "Not enough memory to read box '%s'",
                         osCurrentBox.c_str());
                osCurrentBox.clear();
                continue;
            }
            if (JxlDecoderSetBoxBuffer(
                    m_decoder.get(), abyBoxBuffer.data() + nBytesUsed,
                    abyBoxBuffer.size() - nBytesUsed) != JXL_DEC_SUCCESS)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "JxlDecoderSetBoxBuffer() failed");
                osCurrentBox.clear();
                continue;
            }
        }
#endif
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined, "Unexpected event: %d",
                     status);
            break;
        }
    }

    JxlDecoderReleaseInput(m_decoder.get());

#ifdef HAVE_JXL_BOX_API
    // Load georeferencing from jumb box or from worldfile sidecar.
    if (!abyJumbBoxBuffer.empty())
    {
        VSILFILE *fpJUMB = VSIFileFromMemBuffer(
            nullptr, abyJumbBoxBuffer.data(), abyJumbBoxBuffer.size(), false);
        LoadJP2Metadata(poOpenInfo, nullptr, fpJUMB);
        VSIFCloseL(fpJUMB);
    }
#else
    if (IsJPEGXLContainer(poOpenInfo))
    {
        // A JPEGXL container can be explored with the JPEG2000 box reading
        // logic
        VSIFSeekL(m_fp, 12, SEEK_SET);
        poOpenInfo->fpL = m_fp;
        LoadJP2Metadata(poOpenInfo);
        poOpenInfo->fpL = nullptr;
    }
#endif
    else
    {
        // Only try to read worldfile
        VSILFILE *fpDummy = VSIFileFromMemBuffer(nullptr, nullptr, 0, false);
        LoadJP2Metadata(poOpenInfo, nullptr, fpDummy);
        VSIFCloseL(fpDummy);
    }

    if (!bGotInfo)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Did not get basic info");
        return false;
    }

    GDALDataset::SetMetadataItem("COMPRESSION_REVERSIBILITY",
                                 info.uses_original_profile
#ifdef HAVE_JXL_BOX_API
                                         && !m_bHasJPEGReconstructionData
#endif
                                     ? "LOSSLESS (possibly)"
                                     : "LOSSY",
                                 "IMAGE_STRUCTURE");
#ifdef HAVE_JXL_BOX_API
    if (m_bHasJPEGReconstructionData)
    {
        GDALDataset::SetMetadataItem("ORIGINAL_COMPRESSION", "JPEG",
                                     "IMAGE_STRUCTURE");
    }
#endif

#ifdef HAVE_JXL_THREADS
    const char *pszNumThreads =
        CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    uint32_t nMaxThreads = static_cast<uint32_t>(
        EQUAL(pszNumThreads, "ALL_CPUS") ? CPLGetNumCPUs()
                                         : atoi(pszNumThreads));
    if (nMaxThreads > 1024)
        nMaxThreads = 1024;  // to please Coverity

    const uint32_t nThreads = std::min(
        nMaxThreads,
        JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
    CPLDebug("JPEGXL", "Using %u threads", nThreads);
    JxlResizableParallelRunnerSetThreads(m_parallelRunner.get(), nThreads);
#endif

    // Instantiate bands
    const int nNonExtraBands = l_nBands - m_nNonAlphaExtraChannels;
    for (int i = 1; i <= l_nBands; i++)
    {
        GDALColorInterp eInterp = GCI_Undefined;
        if (info.num_color_channels == 1)
        {
            if (i == 1 && l_nBands <= 2)
                eInterp = GCI_GrayIndex;
            else if (i == 2 && info.num_extra_channels == 1 &&
                     info.alpha_bits != 0)
                eInterp = GCI_AlphaBand;
        }
        else if (info.num_color_channels == 3)
        {
            if (i <= 3)
                eInterp = static_cast<GDALColorInterp>(GCI_RedBand + (i - 1));
            else if (i == 4 && info.num_extra_channels == 1 &&
                     info.alpha_bits != 0)
                eInterp = GCI_AlphaBand;
        }
        std::string osBandName;

        if (i - 1 >= nNonExtraBands)
        {
            JxlExtraChannelInfo sExtraInfo;
            memset(&sExtraInfo, 0, sizeof(sExtraInfo));
            const size_t nIndex = static_cast<size_t>(i - 1 - nNonExtraBands);
            if (JxlDecoderGetExtraChannelInfo(m_decoder.get(), nIndex,
                                              &sExtraInfo) == JXL_DEC_SUCCESS)
            {
                switch (sExtraInfo.type)
                {
                    case JXL_CHANNEL_ALPHA:
                        eInterp = GCI_AlphaBand;
                        break;
                    case JXL_CHANNEL_DEPTH:
                        osBandName = "Depth channel";
                        break;
                    case JXL_CHANNEL_SPOT_COLOR:
                        osBandName = "Spot color channel";
                        break;
                    case JXL_CHANNEL_SELECTION_MASK:
                        osBandName = "Selection mask channel";
                        break;
                    case JXL_CHANNEL_BLACK:
                        osBandName = "Black channel";
                        break;
                    case JXL_CHANNEL_CFA:
                        osBandName = "CFA channel";
                        break;
                    case JXL_CHANNEL_THERMAL:
                        osBandName = "Thermal channel";
                        break;
                    case JXL_CHANNEL_RESERVED0:
                    case JXL_CHANNEL_RESERVED1:
                    case JXL_CHANNEL_RESERVED2:
                    case JXL_CHANNEL_RESERVED3:
                    case JXL_CHANNEL_RESERVED4:
                    case JXL_CHANNEL_RESERVED5:
                    case JXL_CHANNEL_RESERVED6:
                    case JXL_CHANNEL_RESERVED7:
                    case JXL_CHANNEL_UNKNOWN:
                    case JXL_CHANNEL_OPTIONAL:
                        break;
                }

                if (sExtraInfo.name_length > 0)
                {
                    std::string osName;
                    osName.resize(sExtraInfo.name_length);
                    if (JxlDecoderGetExtraChannelName(
                            m_decoder.get(), nIndex, &osName[0],
                            osName.size() + 1) == JXL_DEC_SUCCESS &&
                        osName != CPLSPrintf("Band %d", i))
                    {
                        osBandName = std::move(osName);
                    }
                }
            }
        }

        auto poBand = new JPEGXLRasterBand(
            this, i, eDT, static_cast<int>(info.bits_per_sample), eInterp);
        SetBand(i, poBand);
        if (!osBandName.empty())
            poBand->SetDescription(osBandName.c_str());
    }

    if (l_nBands > 1)
    {
        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }

    // Initialize any PAM information.
    SetDescription(poOpenInfo->pszFilename);
    TryLoadXML(poOpenInfo->GetSiblingFiles());
    oOvManager.Initialize(this, poOpenInfo->pszFilename,
                          poOpenInfo->GetSiblingFiles());

    nPamFlags &= ~GPF_DIRTY;

    return true;
}

/************************************************************************/
/*                        GetDecodedImage()                             */
/************************************************************************/

const std::vector<GByte> &JPEGXLDataset::GetDecodedImage()
{
    if (m_bDecodingFailed || !m_abyImage.empty())
        return m_abyImage;

    const auto eDT = GetRasterBand(1)->GetRasterDataType();
    const auto nDataSize = GDALGetDataTypeSizeBytes(eDT);
    assert(nDataSize > 0);
    const int nNonExtraBands = nBands - m_nNonAlphaExtraChannels;
    if (static_cast<size_t>(nRasterXSize) > std::numeric_limits<size_t>::max() /
                                                nRasterYSize / nDataSize /
                                                nNonExtraBands)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Image too big for architecture");
        m_bDecodingFailed = true;
        return m_abyImage;
    }

    try
    {
        m_abyImage.resize(static_cast<size_t>(nRasterXSize) * nRasterYSize *
                          nNonExtraBands * nDataSize);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate image buffer: %s", e.what());
        m_bDecodingFailed = true;
        return m_abyImage;
    }

    m_abyExtraChannels.resize(m_nNonAlphaExtraChannels);
    for (int i = 0; i < m_nNonAlphaExtraChannels; ++i)
    {
        try
        {
            m_abyExtraChannels[i].resize(static_cast<size_t>(nRasterXSize) *
                                         nRasterYSize * nDataSize);
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate image buffer: %s", e.what());
            m_bDecodingFailed = true;
            return m_abyImage;
        }
    }

    GetDecodedImage(m_abyImage.data(), m_abyImage.size());

    if (m_bDecodingFailed)
        m_abyImage.clear();

    return m_abyImage;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **JPEGXLDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE, "xml:XMP", "EXIF", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **JPEGXLDataset::GetMetadata(const char *pszDomain)
{
#ifdef HAVE_JXL_BOX_API
    if (pszDomain != nullptr && EQUAL(pszDomain, "xml:XMP") && !m_osXMP.empty())
    {
        m_apszXMP[0] = &m_osXMP[0];
        return m_apszXMP;
    }

    if (pszDomain != nullptr && EQUAL(pszDomain, "EXIF") &&
        !m_aosEXIFMetadata.empty())
    {
        return m_aosEXIFMetadata.List();
    }
#endif
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                       GetCompressionFormats()                        */
/************************************************************************/

CPLStringList JPEGXLDataset::GetCompressionFormats(int nXOff, int nYOff,
                                                   int nXSize, int nYSize,
                                                   int nBandCount,
                                                   const int *panBandList)
{
    CPLStringList aosRet;
    if (nXOff == 0 && nYOff == 0 && nXSize == nRasterXSize &&
        nYSize == nRasterYSize && IsAllBands(nBandCount, panBandList))
    {
        aosRet.AddString("JXL");
#ifdef HAVE_JXL_BOX_API
        if (m_bHasJPEGReconstructionData)
            aosRet.AddString("JPEG");
#endif
    }
    return aosRet;
}

/************************************************************************/
/*                       ReadCompressedData()                           */
/************************************************************************/

CPLErr JPEGXLDataset::ReadCompressedData(const char *pszFormat, int nXOff,
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

        if (EQUAL(aosTokens[0], "JXL"))
        {
            if (ppszDetailedFormat)
                *ppszDetailedFormat = VSIStrdup("JXL");
            VSIFSeekL(m_fp, 0, SEEK_END);
            const auto nFileSize = VSIFTellL(m_fp);
            if (nFileSize > std::numeric_limits<size_t>::max() / 2)
                return CE_Failure;
            auto nSize = static_cast<size_t>(nFileSize);
            if (ppBuffer)
            {
                if (pnBufferSize == nullptr)
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
                VSIFSeekL(m_fp, 0, SEEK_SET);
                if (VSIFReadL(*ppBuffer, nSize, 1, m_fp) != 1)
                {
                    if (bFreeOnError)
                    {
                        VSIFree(*ppBuffer);
                        *ppBuffer = nullptr;
                    }
                    return CE_Failure;
                }

                size_t nPos = 0;
                GByte *pabyData = static_cast<GByte *>(*ppBuffer);
                while (nSize - nPos >= 8)
                {
                    uint32_t nBoxSize;
                    memcpy(&nBoxSize, pabyData + nPos, 4);
                    CPL_MSBPTR32(&nBoxSize);
                    if (nBoxSize < 8 || nBoxSize > nSize - nPos)
                        break;
                    char szBoxName[5] = {0, 0, 0, 0, 0};
                    memcpy(szBoxName, pabyData + nPos + 4, 4);
                    if (memcmp(szBoxName, "Exif", 4) == 0 ||
                        memcmp(szBoxName, "xml ", 4) == 0 ||
                        memcmp(szBoxName, "jumb", 4) == 0)
                    {
                        CPLDebug("JPEGXL",
                                 "Remove existing %s box from "
                                 "source compressed data",
                                 szBoxName);
                        memmove(pabyData + nPos, pabyData + nPos + nBoxSize,
                                nSize - (nPos + nBoxSize));
                        nSize -= nBoxSize;
                    }
                    else
                    {
                        nPos += nBoxSize;
                    }
                }
            }
            if (pnBufferSize)
                *pnBufferSize = nSize;
            return CE_None;
        }

#ifdef HAVE_JXL_BOX_API
        if (m_bHasJPEGReconstructionData && EQUAL(aosTokens[0], "JPEG"))
        {
            auto decoder = JxlDecoderMake(nullptr);
            if (!decoder)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderMake() failed");
                return CE_Failure;
            }
            auto status = JxlDecoderSubscribeEvents(
                decoder.get(), JXL_DEC_BASIC_INFO |
                                   JXL_DEC_JPEG_RECONSTRUCTION |
                                   JXL_DEC_FULL_IMAGE);
            if (status != JXL_DEC_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderSubscribeEvents() failed");
                return CE_Failure;
            }

            VSIFSeekL(m_fp, 0, SEEK_SET);
            try
            {
                std::vector<GByte> jpeg_bytes;
                std::vector<GByte> jpeg_data_chunk(16 * 1024);

                bool bJPEGReconstruction = false;
                while (true)
                {
                    status = JxlDecoderProcessInput(decoder.get());
                    if (status == JXL_DEC_SUCCESS)
                    {
                        break;
                    }
                    else if (status == JXL_DEC_NEED_MORE_INPUT)
                    {
                        JxlDecoderReleaseInput(decoder.get());

                        const size_t nRead =
                            VSIFReadL(m_abyInputData.data(), 1,
                                      m_abyInputData.size(), m_fp);
                        if (nRead == 0)
                        {
                            break;
                        }
                        if (JxlDecoderSetInput(decoder.get(),
                                               m_abyInputData.data(),
                                               nRead) != JXL_DEC_SUCCESS)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "JxlDecoderSetInput() failed");
                            return CE_Failure;
                        }
                    }
                    else if (status == JXL_DEC_JPEG_RECONSTRUCTION)
                    {
                        bJPEGReconstruction = true;
                        // Decoding to JPEG.
                        if (JXL_DEC_SUCCESS !=
                            JxlDecoderSetJPEGBuffer(decoder.get(),
                                                    jpeg_data_chunk.data(),
                                                    jpeg_data_chunk.size()))
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Decoder failed to set JPEG Buffer\n");
                            return CE_Failure;
                        }
                    }
                    else if (status == JXL_DEC_JPEG_NEED_MORE_OUTPUT)
                    {
                        // Decoded a chunk to JPEG.
                        size_t used_jpeg_output =
                            jpeg_data_chunk.size() -
                            JxlDecoderReleaseJPEGBuffer(decoder.get());
                        jpeg_bytes.insert(
                            jpeg_bytes.end(), jpeg_data_chunk.data(),
                            jpeg_data_chunk.data() + used_jpeg_output);
                        if (used_jpeg_output == 0)
                        {
                            // Chunk is too small.
                            jpeg_data_chunk.resize(jpeg_data_chunk.size() * 2);
                        }
                        if (JXL_DEC_SUCCESS !=
                            JxlDecoderSetJPEGBuffer(decoder.get(),
                                                    jpeg_data_chunk.data(),
                                                    jpeg_data_chunk.size()))
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Decoder failed to set JPEG Buffer\n");
                            return CE_Failure;
                        }
                    }
                    else if (status == JXL_DEC_BASIC_INFO ||
                             status == JXL_DEC_FULL_IMAGE)
                    {
                        // do nothing
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Unexpected event: %d", status);
                        break;
                    }
                }
                if (bJPEGReconstruction)
                {
                    size_t used_jpeg_output =
                        jpeg_data_chunk.size() -
                        JxlDecoderReleaseJPEGBuffer(decoder.get());
                    jpeg_bytes.insert(jpeg_bytes.end(), jpeg_data_chunk.data(),
                                      jpeg_data_chunk.data() +
                                          used_jpeg_output);
                }

                JxlDecoderReleaseInput(decoder.get());

                if (!jpeg_bytes.empty() &&
                    jpeg_bytes.size() < static_cast<size_t>(INT_MAX))
                {
                    constexpr GByte EXIF_SIGNATURE[] = {'E', 'x',  'i',
                                                        'f', '\0', '\0'};
                    constexpr char APP1_XMP_SIGNATURE[] =
                        "http://ns.adobe.com/xap/1.0/";
                    size_t nChunkLoc = 2;
                    while (nChunkLoc + 4 <= jpeg_bytes.size())
                    {
                        if (jpeg_bytes[nChunkLoc + 0] == 0xFF &&
                            jpeg_bytes[nChunkLoc + 1] == 0xDA)
                        {
                            break;
                        }
                        if (jpeg_bytes[nChunkLoc + 0] != 0xFF)
                            break;
                        const int nChunkLength =
                            jpeg_bytes[nChunkLoc + 2] * 256 +
                            jpeg_bytes[nChunkLoc + 3];
                        if (nChunkLength < 2 ||
                            static_cast<size_t>(nChunkLength) >
                                jpeg_bytes.size() - (nChunkLoc + 2))
                            break;
                        if (jpeg_bytes[nChunkLoc + 0] == 0xFF &&
                            jpeg_bytes[nChunkLoc + 1] == 0xE1 &&
                            nChunkLoc + 4 + sizeof(EXIF_SIGNATURE) <=
                                jpeg_bytes.size() &&
                            memcmp(jpeg_bytes.data() + nChunkLoc + 4,
                                   EXIF_SIGNATURE, sizeof(EXIF_SIGNATURE)) == 0)
                        {
                            CPLDebug("JPEGXL", "Remove existing EXIF from "
                                               "source compressed data");
                            jpeg_bytes.erase(jpeg_bytes.begin() + nChunkLoc,
                                             jpeg_bytes.begin() + nChunkLoc +
                                                 2 + nChunkLength);
                            continue;
                        }
                        else if (jpeg_bytes[nChunkLoc + 0] == 0xFF &&
                                 jpeg_bytes[nChunkLoc + 1] == 0xE1 &&
                                 nChunkLoc + 4 + sizeof(APP1_XMP_SIGNATURE) <=
                                     jpeg_bytes.size() &&
                                 memcmp(jpeg_bytes.data() + nChunkLoc + 4,
                                        APP1_XMP_SIGNATURE,
                                        sizeof(APP1_XMP_SIGNATURE)) == 0)
                        {
                            CPLDebug("JPEGXL", "Remove existing XMP from "
                                               "source compressed data");
                            jpeg_bytes.erase(jpeg_bytes.begin() + nChunkLoc,
                                             jpeg_bytes.begin() + nChunkLoc +
                                                 2 + nChunkLength);
                            continue;
                        }
                        nChunkLoc += 2 + nChunkLength;
                    }

                    if (ppszDetailedFormat)
                    {
                        *ppszDetailedFormat =
                            VSIStrdup(GDALGetCompressionFormatForJPEG(
                                          jpeg_bytes.data(), jpeg_bytes.size())
                                          .c_str());
                    }

                    const auto nSize = jpeg_bytes.size();
                    if (ppBuffer)
                    {
                        if (*ppBuffer)
                        {
                            if (pnBufferSize == nullptr)
                                return CE_Failure;
                            if (*pnBufferSize < nSize)
                                return CE_Failure;
                        }
                        else
                        {
                            *ppBuffer = VSI_MALLOC_VERBOSE(nSize);
                            if (*ppBuffer == nullptr)
                                return CE_Failure;
                        }
                        memcpy(*ppBuffer, jpeg_bytes.data(), nSize);
                    }
                    if (pnBufferSize)
                        *pnBufferSize = nSize;

                    return CE_None;
                }
            }
            catch (const std::exception &)
            {
            }
        }
#endif
    }
    return CE_Failure;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *JPEGXLDataset::GetMetadataItem(const char *pszName,
                                           const char *pszDomain)
{
#ifdef HAVE_JXL_BOX_API
    if (pszDomain != nullptr && EQUAL(pszDomain, "EXIF") &&
        !m_aosEXIFMetadata.empty())
    {
        return m_aosEXIFMetadata.FetchNameValue(pszName);
    }
#endif

    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                        GetDecodedImage()                             */
/************************************************************************/

void JPEGXLDataset::GetDecodedImage(void *pabyOutputData,
                                    size_t nOutputDataSize)
{
    JxlDecoderRewind(m_decoder.get());
    VSIFSeekL(m_fp, 0, SEEK_SET);

    if (JxlDecoderSubscribeEvents(m_decoder.get(), JXL_DEC_FULL_IMAGE) !=
        JXL_DEC_SUCCESS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlDecoderSubscribeEvents() failed");
        return;
    }

    const auto eDT = GetRasterBand(1)->GetRasterDataType();
    while (true)
    {
        const JxlDecoderStatus status = JxlDecoderProcessInput(m_decoder.get());
        if (status == JXL_DEC_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Decoding error");
            m_bDecodingFailed = true;
            break;
        }
        else if (status == JXL_DEC_NEED_MORE_INPUT)
        {
            JxlDecoderReleaseInput(m_decoder.get());

            const size_t nRead = VSIFReadL(m_abyInputData.data(), 1,
                                           m_abyInputData.size(), m_fp);
            if (nRead == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Decoder expected more input, but no more available");
                m_bDecodingFailed = true;
                break;
            }
            if (JxlDecoderSetInput(m_decoder.get(), m_abyInputData.data(),
                                   nRead) != JXL_DEC_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderSetInput() failed");
                m_bDecodingFailed = true;
                break;
            }
        }
        else if (status == JXL_DEC_SUCCESS)
        {
            break;
        }
        else if (status == JXL_DEC_FULL_IMAGE)
        {
            // ok
        }
        else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER)
        {
            JxlPixelFormat format = {
                static_cast<uint32_t>(nBands - m_nNonAlphaExtraChannels),
                eDT == GDT_Byte     ? JXL_TYPE_UINT8
                : eDT == GDT_UInt16 ? JXL_TYPE_UINT16
                                    : JXL_TYPE_FLOAT,
                JXL_NATIVE_ENDIAN, 0 /* alignment */
            };

            size_t buffer_size;
            if (JxlDecoderImageOutBufferSize(m_decoder.get(), &format,
                                             &buffer_size) != JXL_DEC_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderImageOutBufferSize failed()");
                m_bDecodingFailed = true;
                break;
            }
            if (buffer_size != nOutputDataSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderImageOutBufferSize returned an unexpected "
                         "buffer_size");
                m_bDecodingFailed = true;
                break;
            }

            // It could be interesting to use JxlDecoderSetImageOutCallback()
            // to do progressive decoding, but at the time of writing, libjxl
            // seems to just call the callback when all the image is
            // decompressed
            if (JxlDecoderSetImageOutBuffer(m_decoder.get(), &format,
                                            pabyOutputData,
                                            nOutputDataSize) != JXL_DEC_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderSetImageOutBuffer failed()");
                m_bDecodingFailed = true;
                break;
            }

            format.num_channels = 1;
            for (int i = 0; i < m_nNonAlphaExtraChannels; ++i)
            {
                if (JxlDecoderExtraChannelBufferSize(m_decoder.get(), &format,
                                                     &buffer_size,
                                                     i) != JXL_DEC_SUCCESS)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "JxlDecoderExtraChannelBufferSize failed()");
                    m_bDecodingFailed = true;
                    break;
                }
                if (buffer_size != m_abyExtraChannels[i].size())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "JxlDecoderExtraChannelBufferSize returned an "
                             "unexpected buffer_size");
                    m_bDecodingFailed = true;
                    break;
                }
                if (JxlDecoderSetExtraChannelBuffer(
                        m_decoder.get(), &format, m_abyExtraChannels[i].data(),
                        m_abyExtraChannels[i].size(), i) != JXL_DEC_SUCCESS)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "JxlDecoderSetExtraChannelBuffer failed()");
                    m_bDecodingFailed = true;
                    break;
                }
            }
            if (m_bDecodingFailed)
            {
                break;
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unexpected decoder state: %d", status);
        }
    }

    // Rescale from 8-bits/16-bits
    if (m_nBits < GDALGetDataTypeSize(eDT))
    {
        const auto Rescale = [this, eDT](void *pBuffer, int nChannels)
        {
            const size_t nSamples =
                static_cast<size_t>(nRasterXSize) * nRasterYSize * nChannels;
            const int nMaxVal = (1 << m_nBits) - 1;
            if (eDT == GDT_Byte)
            {
                const int nHalfMaxWidth = 127;
                GByte *panData = static_cast<GByte *>(pBuffer);
                for (size_t i = 0; i < nSamples; ++i)
                {
                    panData[i] = static_cast<GByte>(
                        (panData[i] * nMaxVal + nHalfMaxWidth) / 255);
                }
            }
            else if (eDT == GDT_UInt16)
            {
                const int nHalfMaxWidth = 32767;
                uint16_t *panData = static_cast<uint16_t *>(pBuffer);
                for (size_t i = 0; i < nSamples; ++i)
                {
                    panData[i] = static_cast<uint16_t>(
                        (panData[i] * nMaxVal + nHalfMaxWidth) / 65535);
                }
            }
        };

        Rescale(pabyOutputData, nBands - m_nNonAlphaExtraChannels);
        for (int i = 0; i < m_nNonAlphaExtraChannels; ++i)
        {
            Rescale(m_abyExtraChannels[i].data(), 1);
        }
    }
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JPEGXLDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                int nXSize, int nYSize, void *pData,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eBufType, int nBandCount,
                                int *panBandMap, GSpacing nPixelSpace,
                                GSpacing nLineSpace, GSpacing nBandSpace,
                                GDALRasterIOExtraArg *psExtraArg)

{
    const auto AreSequentialBands = [](const int *panItems, int nItems)
    {
        for (int i = 0; i < nItems; i++)
        {
            if (panItems[i] != i + 1)
                return false;
        }
        return true;
    };

    if (eRWFlag == GF_Read && nXOff == 0 && nYOff == 0 &&
        nXSize == nRasterXSize && nYSize == nRasterYSize &&
        nBufXSize == nXSize && nBufYSize == nYSize)
    {
        // Get the full image in a pixel-interleaved way
        if (m_bDecodingFailed)
            return CE_Failure;

        CPLDebug("JPEGXL", "Using optimized IRasterIO() code path");

        const auto nBufTypeSize = GDALGetDataTypeSizeBytes(eBufType);
        const bool bIsPixelInterleaveBuffer =
            ((nBandSpace == 0 && nBandCount == 1) ||
             nBandSpace == nBufTypeSize) &&
            nPixelSpace == static_cast<GSpacing>(nBufTypeSize) * nBandCount &&
            nLineSpace == nPixelSpace * nRasterXSize;

        const auto eNativeDT = GetRasterBand(1)->GetRasterDataType();
        const auto nNativeDataSize = GDALGetDataTypeSizeBytes(eNativeDT);
        const bool bIsBandSequential =
            AreSequentialBands(panBandMap, nBandCount);
        if (eBufType == eNativeDT && bIsBandSequential &&
            nBandCount == nBands && m_nNonAlphaExtraChannels == 0 &&
            bIsPixelInterleaveBuffer)
        {
            // We can directly use the user output buffer
            GetDecodedImage(pData, static_cast<size_t>(nRasterXSize) *
                                       nRasterYSize * nBands * nNativeDataSize);
            return m_bDecodingFailed ? CE_Failure : CE_None;
        }

        const auto &abyDecodedImage = GetDecodedImage();
        if (abyDecodedImage.empty())
        {
            return CE_Failure;
        }
        const int nNonExtraBands = nBands - m_nNonAlphaExtraChannels;
        if (bIsPixelInterleaveBuffer && bIsBandSequential &&
            nBandCount == nNonExtraBands)
        {
            GDALCopyWords64(abyDecodedImage.data(), eNativeDT, nNativeDataSize,
                            pData, eBufType, nBufTypeSize,
                            static_cast<GPtrDiff_t>(nRasterXSize) *
                                nRasterYSize * nBandCount);
        }
        else
        {
            for (int iBand = 0; iBand < nBandCount; iBand++)
            {
                const int iSrcBand = panBandMap[iBand] - 1;
                if (iSrcBand < nNonExtraBands)
                {
                    for (int iY = 0; iY < nRasterYSize; iY++)
                    {
                        const GByte *pSrc = abyDecodedImage.data() +
                                            (static_cast<size_t>(iY) *
                                                 nRasterXSize * nNonExtraBands +
                                             iSrcBand) *
                                                nNativeDataSize;
                        GByte *pDst = static_cast<GByte *>(pData) +
                                      iY * nLineSpace + iBand * nBandSpace;
                        GDALCopyWords(pSrc, eNativeDT,
                                      nNativeDataSize * nNonExtraBands, pDst,
                                      eBufType, static_cast<int>(nPixelSpace),
                                      nRasterXSize);
                    }
                }
                else
                {
                    for (int iY = 0; iY < nRasterYSize; iY++)
                    {
                        const GByte *pSrc =
                            m_abyExtraChannels[iSrcBand - nNonExtraBands]
                                .data() +
                            static_cast<size_t>(iY) * nRasterXSize *
                                nNativeDataSize;
                        GByte *pDst = static_cast<GByte *>(pData) +
                                      iY * nLineSpace + iBand * nBandSpace;
                        GDALCopyWords(pSrc, eNativeDT, nNativeDataSize, pDst,
                                      eBufType, static_cast<int>(nPixelSpace),
                                      nRasterXSize);
                    }
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
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JPEGXLRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                   int nXSize, int nYSize, void *pData,
                                   int nBufXSize, int nBufYSize,
                                   GDALDataType eBufType, GSpacing nPixelSpace,
                                   GSpacing nLineSpace,
                                   GDALRasterIOExtraArg *psExtraArg)

{
    if (eRWFlag == GF_Read && nXOff == 0 && nYOff == 0 &&
        nXSize == nRasterXSize && nYSize == nRasterYSize &&
        nBufXSize == nXSize && nBufYSize == nYSize)
    {
        return cpl::down_cast<JPEGXLDataset *>(poDS)->IRasterIO(
            GF_Read, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, 1, &nBand, nPixelSpace, nLineSpace, 0, psExtraArg);
    }

    return GDALPamRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace, psExtraArg);
}

/************************************************************************/
/*                          OpenStaticPAM()                             */
/************************************************************************/

GDALPamDataset *JPEGXLDataset::OpenStaticPAM(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    auto poDS = std::make_unique<JPEGXLDataset>();
    if (!poDS->Open(poOpenInfo))
        return nullptr;

    return poDS.release();
}

/************************************************************************/
/*                          OpenStatic()                                */
/************************************************************************/

GDALDataset *JPEGXLDataset::OpenStatic(GDALOpenInfo *poOpenInfo)
{
    GDALDataset *poDS = OpenStaticPAM(poOpenInfo);

#ifdef HAVE_JXL_BOX_API
    if (poDS &&
        CPLFetchBool(poOpenInfo->papszOpenOptions, "APPLY_ORIENTATION", false))
    {
        const char *pszOrientation =
            poDS->GetMetadataItem("EXIF_Orientation", "EXIF");
        if (pszOrientation && !EQUAL(pszOrientation, "1"))
        {
            int nOrientation = atoi(pszOrientation);
            if (nOrientation >= 2 && nOrientation <= 8)
            {
                std::unique_ptr<GDALDataset> poOriDS(poDS);
                auto poOrientedDS = std::make_unique<GDALOrientedDataset>(
                    std::move(poOriDS),
                    static_cast<GDALOrientedDataset::Origin>(nOrientation));
                poDS = poOrientedDS.release();
            }
        }
    }
#endif

    return poDS;
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *JPEGXLDataset::CreateCopy(const char *pszFilename,
                                       GDALDataset *poSrcDS, int /*bStrict*/,
                                       char **papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void *pProgressData)

{
    if (poSrcDS->GetRasterXSize() <= 0 || poSrcDS->GetRasterYSize() <= 0 ||
        poSrcDS->GetRasterCount() == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid source dataset");
        return nullptr;
    }

    // Look for EXIF metadata first in the EXIF metadata domain, and fallback
    // to main domain.
    const bool bWriteExifMetadata =
        CPLFetchBool(papszOptions, "WRITE_EXIF_METADATA", true);
    char **papszEXIF = poSrcDS->GetMetadata("EXIF");
    bool bEXIFFromMainDomain = false;
    if (papszEXIF == nullptr && bWriteExifMetadata)
    {
        char **papszMetadata = poSrcDS->GetMetadata();
        for (CSLConstList papszIter = papszMetadata; papszIter && *papszIter;
             ++papszIter)
        {
            if (STARTS_WITH(*papszIter, "EXIF_"))
            {
                papszEXIF = papszMetadata;
                bEXIFFromMainDomain = true;
                break;
            }
        }
    }

    // Write "xml " box with xml:XMP metadata
    const bool bWriteXMP = CPLFetchBool(papszOptions, "WRITE_XMP", true);
    char **papszXMP = poSrcDS->GetMetadata("xml:XMP");

    const bool bWriteGeoJP2 = CPLFetchBool(papszOptions, "WRITE_GEOJP2", true);
    double adfGeoTransform[6];
    const bool bHasGeoTransform =
        poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None;
    const OGRSpatialReference *poSRS = poSrcDS->GetSpatialRef();
    const int nGCPCount = poSrcDS->GetGCPCount();
    char **papszRPCMD = poSrcDS->GetMetadata("RPC");
    std::unique_ptr<GDALJP2Box> poJUMBFBox;
    if (bWriteGeoJP2 &&
        (poSRS != nullptr || bHasGeoTransform || nGCPCount || papszRPCMD))
    {
        GDALJP2Metadata oJP2Metadata;
        if (poSRS)
            oJP2Metadata.SetSpatialRef(poSRS);
        if (bHasGeoTransform)
            oJP2Metadata.SetGeoTransform(adfGeoTransform);
        if (nGCPCount)
        {
            const OGRSpatialReference *poSRSGCP = poSrcDS->GetGCPSpatialRef();
            if (poSRSGCP)
                oJP2Metadata.SetSpatialRef(poSRSGCP);
            oJP2Metadata.SetGCPs(nGCPCount, poSrcDS->GetGCPs());
        }
        if (papszRPCMD)
            oJP2Metadata.SetRPCMD(papszRPCMD);

        const char *pszAreaOfPoint =
            poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        oJP2Metadata.bPixelIsPoint =
            pszAreaOfPoint && EQUAL(pszAreaOfPoint, GDALMD_AOP_POINT);

        std::unique_ptr<GDALJP2Box> poJP2GeoTIFF;
        poJP2GeoTIFF.reset(oJP2Metadata.CreateJP2GeoTIFF());
        if (poJP2GeoTIFF)
        {
            // Per JUMBF spec: UUID Content Type. The JUMBF box contains exactly
            // one UUID box
            const GByte abyUUIDTypeUUID[16] = {
                0x75, 0x75, 0x69, 0x64, 0x00, 0x11, 0x00, 0x10,
                0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
            std::unique_ptr<GDALJP2Box> poJUMBFDescrBox;
            poJUMBFDescrBox.reset(GDALJP2Box::CreateJUMBFDescriptionBox(
                abyUUIDTypeUUID, "GeoJP2 box"));
            const GDALJP2Box *poJP2GeoTIFFConst = poJP2GeoTIFF.get();
            poJUMBFBox.reset(GDALJP2Box::CreateJUMBFBox(poJUMBFDescrBox.get(),
                                                        1, &poJP2GeoTIFFConst));
        }
    }

    const char *pszLossLessCopy =
        CSLFetchNameValueDef(papszOptions, "LOSSLESS_COPY", "AUTO");
    if (EQUAL(pszLossLessCopy, "AUTO") || CPLTestBool(pszLossLessCopy))
    {
        void *pJPEGXLContent = nullptr;
        size_t nJPEGXLContent = 0;
        if (poSrcDS->ReadCompressedData(
                "JXL", 0, 0, poSrcDS->GetRasterXSize(),
                poSrcDS->GetRasterYSize(), poSrcDS->GetRasterCount(), nullptr,
                &pJPEGXLContent, &nJPEGXLContent, nullptr) == CE_None)
        {
            CPLDebug("JPEGXL", "Lossless copy from source dataset");
            GByte abySizeAndBoxName[8];
            std::vector<GByte> abyData;
            bool bFallbackToGeneral = false;
            try
            {
                abyData.assign(static_cast<const GByte *>(pJPEGXLContent),
                               static_cast<const GByte *>(pJPEGXLContent) +
                                   nJPEGXLContent);

                size_t nInsertPos = 0;
                if (abyData.size() >= 2 && abyData[0] == 0xff &&
                    abyData[1] == 0x0a)
                {
                    // If we get a "naked" codestream, insert it into a
                    // ISOBMFF-based container
                    constexpr const GByte abyJXLContainerSignatureAndFtypBox[] =
                        {0x00, 0x00, 0x00, 0x0C, 'J',  'X',  'L',  ' ',
                         0x0D, 0x0A, 0x87, 0x0A, 0x00, 0x00, 0x00, 0x14,
                         'f',  't',  'y',  'p',  'j',  'x',  'l',  ' ',
                         0,    0,    0,    0,    'j',  'x',  'l',  ' '};
                    uint32_t nBoxSize =
                        static_cast<uint32_t>(8 + abyData.size());
                    abyData.insert(
                        abyData.begin(), abyJXLContainerSignatureAndFtypBox,
                        abyJXLContainerSignatureAndFtypBox +
                            sizeof(abyJXLContainerSignatureAndFtypBox));
                    CPL_MSBPTR32(&nBoxSize);
                    memcpy(abySizeAndBoxName, &nBoxSize, 4);
                    memcpy(abySizeAndBoxName + 4, "jxlc", 4);
                    nInsertPos = sizeof(abyJXLContainerSignatureAndFtypBox);
                    abyData.insert(
                        abyData.begin() + nInsertPos, abySizeAndBoxName,
                        abySizeAndBoxName + sizeof(abySizeAndBoxName));
                }
                else
                {
                    size_t nPos = 0;
                    const GByte *pabyData = abyData.data();
                    while (nPos + 8 <= abyData.size())
                    {
                        uint32_t nBoxSize;
                        memcpy(&nBoxSize, pabyData + nPos, 4);
                        CPL_MSBPTR32(&nBoxSize);
                        if (nBoxSize < 8 || nBoxSize > abyData.size() - nPos)
                            break;
                        char szBoxName[5] = {0, 0, 0, 0, 0};
                        memcpy(szBoxName, pabyData + nPos + 4, 4);
                        if (memcmp(szBoxName, "jxlp", 4) == 0 ||
                            memcmp(szBoxName, "jxlc", 4) == 0)
                        {
                            nInsertPos = nPos;
                            break;
                        }
                        else
                        {
                            nPos += nBoxSize;
                        }
                    }
                }

                // Write "Exif" box with EXIF metadata
                if (papszEXIF && bWriteExifMetadata)
                {
                    if (nInsertPos)
                    {
                        GUInt32 nMarkerSize = 0;
                        GByte *pabyEXIF =
                            EXIFCreate(papszEXIF, nullptr, 0, 0, 0,  // overview
                                       &nMarkerSize);
                        CPLAssert(nMarkerSize > 6 &&
                                  memcmp(pabyEXIF, "Exif\0\0", 6) == 0);
                        // Add 4 leading bytes at 0
                        std::vector<GByte> abyEXIF(4 + nMarkerSize - 6);
                        memcpy(&abyEXIF[4], pabyEXIF + 6, nMarkerSize - 6);
                        CPLFree(pabyEXIF);

                        uint32_t nBoxSize =
                            static_cast<uint32_t>(8 + abyEXIF.size());
                        CPL_MSBPTR32(&nBoxSize);
                        memcpy(abySizeAndBoxName, &nBoxSize, 4);
                        memcpy(abySizeAndBoxName + 4, "Exif", 4);
                        abyData.insert(
                            abyData.begin() + nInsertPos, abySizeAndBoxName,
                            abySizeAndBoxName + sizeof(abySizeAndBoxName));
                        abyData.insert(abyData.begin() + nInsertPos + 8,
                                       abyEXIF.data(),
                                       abyEXIF.data() + abyEXIF.size());
                        nInsertPos += 8 + abyEXIF.size();
                    }
                    else
                    {
                        // shouldn't happen
                        CPLDebug("JPEGX", "Cannot add Exif box to codestream");
                        bFallbackToGeneral = true;
                    }
                }

                if (papszXMP && papszXMP[0] && bWriteXMP)
                {
                    if (nInsertPos)
                    {
                        const size_t nXMPLen = strlen(papszXMP[0]);
                        uint32_t nBoxSize = static_cast<uint32_t>(8 + nXMPLen);
                        CPL_MSBPTR32(&nBoxSize);
                        memcpy(abySizeAndBoxName, &nBoxSize, 4);
                        memcpy(abySizeAndBoxName + 4, "xml ", 4);
                        abyData.insert(
                            abyData.begin() + nInsertPos, abySizeAndBoxName,
                            abySizeAndBoxName + sizeof(abySizeAndBoxName));
                        abyData.insert(abyData.begin() + nInsertPos + 8,
                                       reinterpret_cast<GByte *>(papszXMP[0]),
                                       reinterpret_cast<GByte *>(papszXMP[0]) +
                                           nXMPLen);
                        nInsertPos += 8 + nXMPLen;
                    }
                    else
                    {
                        // shouldn't happen
                        CPLDebug("JPEGX", "Cannot add XMP box to codestream");
                        bFallbackToGeneral = true;
                    }
                }

                // Write GeoJP2 box in a JUMBF box from georeferencing information
                if (poJUMBFBox)
                {
                    if (nInsertPos)
                    {
                        const size_t nDataLen =
                            static_cast<size_t>(poJUMBFBox->GetBoxLength());
                        uint32_t nBoxSize = static_cast<uint32_t>(8 + nDataLen);
                        CPL_MSBPTR32(&nBoxSize);
                        memcpy(abySizeAndBoxName, &nBoxSize, 4);
                        memcpy(abySizeAndBoxName + 4, "jumb", 4);
                        abyData.insert(
                            abyData.begin() + nInsertPos, abySizeAndBoxName,
                            abySizeAndBoxName + sizeof(abySizeAndBoxName));
                        GByte *pabyBoxData = poJUMBFBox->GetWritableBoxData();
                        abyData.insert(abyData.begin() + nInsertPos + 8,
                                       pabyBoxData, pabyBoxData + nDataLen);
                        VSIFree(pabyBoxData);
                        nInsertPos += 8 + nDataLen;
                    }
                    else
                    {
                        // shouldn't happen
                        CPLDebug("JPEGX",
                                 "Cannot add JUMBF GeoJP2 box to codestream");
                        bFallbackToGeneral = true;
                    }
                }

                CPL_IGNORE_RET_VAL(nInsertPos);
            }
            catch (const std::exception &)
            {
                abyData.clear();
            }
            VSIFree(pJPEGXLContent);

            if (!bFallbackToGeneral && !abyData.empty())
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
                auto poDS = OpenStaticPAM(&oOpenInfo);
                if (poDS)
                {
                    // Do not create a .aux.xml file just for AREA_OR_POINT=Area
                    const char *pszAreaOfPoint =
                        poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
                    if (pszAreaOfPoint &&
                        EQUAL(pszAreaOfPoint, GDALMD_AOP_AREA))
                    {
                        poDS->SetMetadataItem(GDALMD_AREA_OR_POINT,
                                              GDALMD_AOP_AREA);
                        poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
                    }

                    // When copying from JPEG, expose the EXIF metadata in the main domain,
                    // so that PAM doesn't copy it.
                    if (bEXIFFromMainDomain)
                    {
                        for (CSLConstList papszIter = papszEXIF;
                             papszIter && *papszIter; ++papszIter)
                        {
                            if (STARTS_WITH(*papszIter, "EXIF_"))
                            {
                                char *pszKey = nullptr;
                                const char *pszValue =
                                    CPLParseNameValue(*papszIter, &pszKey);
                                if (pszKey && pszValue)
                                {
                                    poDS->SetMetadataItem(pszKey, pszValue);
                                }
                                CPLFree(pszKey);
                            }
                        }
                        poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
                    }

                    poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);
                }

                return poDS;
            }
        }
    }

    JxlPixelFormat format = {0, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    const auto eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    switch (eDT)
    {
        case GDT_Byte:
            format.data_type = JXL_TYPE_UINT8;
            break;
        case GDT_UInt16:
            format.data_type = JXL_TYPE_UINT16;
            break;
        case GDT_Float32:
            format.data_type = JXL_TYPE_FLOAT;
            break;
        default:
            CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type");
            return nullptr;
    }

    const char *pszLossLess = CSLFetchNameValue(papszOptions, "LOSSLESS");
    const char *pszDistance = CSLFetchNameValue(papszOptions, "DISTANCE");
    const char *pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");
    const char *pszAlphaDistance =
        CSLFetchNameValue(papszOptions, "ALPHA_DISTANCE");

    const bool bLossless = (pszLossLess == nullptr && pszDistance == nullptr &&
                            pszQuality == nullptr) ||
                           (pszLossLess != nullptr && CPLTestBool(pszLossLess));
    if (pszLossLess == nullptr &&
        (pszDistance != nullptr || pszQuality != nullptr))
    {
        CPLDebug("JPEGXL", "Using lossy mode");
    }
    if ((pszLossLess != nullptr && bLossless) && pszDistance != nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DISTANCE and LOSSLESS=YES are mutually exclusive");
        return nullptr;
    }
    if ((pszLossLess != nullptr && bLossless) && pszAlphaDistance != nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ALPHA_DISTANCE and LOSSLESS=YES are mutually exclusive");
        return nullptr;
    }
    if ((pszLossLess != nullptr && bLossless) && pszQuality != nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "QUALITY and LOSSLESS=YES are mutually exclusive");
        return nullptr;
    }
    if (pszDistance != nullptr && pszQuality != nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "QUALITY and DISTANCE are mutually exclusive");
        return nullptr;
    }

    float fDistance = 0.0f;
    float fAlphaDistance = -1.0;
    if (!bLossless)
    {
        fDistance =
            pszDistance ? static_cast<float>(CPLAtof(pszDistance)) : 1.0f;
        if (pszQuality != nullptr)
        {
            const double quality = CPLAtof(pszQuality);
            // Quality settings roughly match libjpeg qualities.
            // Formulas taken from cjxl.cc
            if (quality >= 100)
            {
                fDistance = 0;
            }
            else if (quality >= 30)
            {
                fDistance = static_cast<float>(0.1 + (100 - quality) * 0.09);
            }
            else
            {
                fDistance = static_cast<float>(
                    6.4 + pow(2.5, (30 - quality) / 5.0f) / 6.25f);
            }
        }
        if (fDistance >= 0.0f && fDistance < 0.1f)
            fDistance = 0.1f;

        if (pszAlphaDistance)
        {
            fAlphaDistance = static_cast<float>(CPLAtof(pszAlphaDistance));
            if (fAlphaDistance > 0.0f && fAlphaDistance < 0.1f)
                fAlphaDistance = 0.1f;
        }
    }

    const bool bAlphaDistanceSameAsMainChannel =
        (fAlphaDistance < 0.0f) ||
        ((bLossless && fAlphaDistance == 0.0f) ||
         (!bLossless && fAlphaDistance == fDistance));
#ifndef HAVE_JxlEncoderSetExtraChannelDistance
    if (!bAlphaDistanceSameAsMainChannel)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "ALPHA_DISTANCE ignored due to "
                 "JxlEncoderSetExtraChannelDistance() not being "
                 "available. Please upgrade libjxl to > 0.8.1");
    }
#endif

    auto encoder = JxlEncoderMake(nullptr);
    if (!encoder)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlEncoderMake() failed");
        return nullptr;
    }

    const char *pszNBits = CSLFetchNameValue(papszOptions, "NBITS");
    if (pszNBits == nullptr)
        pszNBits = poSrcDS->GetRasterBand(1)->GetMetadataItem(
            "NBITS", "IMAGE_STRUCTURE");
    const int nBits =
        ((eDT == GDT_Byte || eDT == GDT_UInt16) && pszNBits != nullptr)
            ? atoi(pszNBits)
            : GDALGetDataTypeSize(eDT);

    JxlBasicInfo basic_info;
    JxlEncoderInitBasicInfo(&basic_info);
    basic_info.xsize = poSrcDS->GetRasterXSize();
    basic_info.ysize = poSrcDS->GetRasterYSize();
    basic_info.bits_per_sample = nBits;
    basic_info.orientation = JXL_ORIENT_IDENTITY;
    if (format.data_type == JXL_TYPE_FLOAT)
    {
        basic_info.exponent_bits_per_sample = 8;
    }

    const int nSrcBands = poSrcDS->GetRasterCount();

    bool bHasInterleavedAlphaBand = false;
    if (nSrcBands == 1)
    {
        basic_info.num_color_channels = 1;
    }
    else if (nSrcBands == 2)
    {
        basic_info.num_color_channels = 1;
        basic_info.num_extra_channels = 1;
        if (poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
                GCI_AlphaBand &&
            bAlphaDistanceSameAsMainChannel)
        {
            bHasInterleavedAlphaBand = true;
            basic_info.alpha_bits = basic_info.bits_per_sample;
            basic_info.alpha_exponent_bits =
                basic_info.exponent_bits_per_sample;
        }
    }
    else /* if( nSrcBands >= 3 ) */
    {
        if (poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
                GCI_RedBand &&
            poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
                GCI_GreenBand &&
            poSrcDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand)
        {
            basic_info.num_color_channels = 3;
            basic_info.num_extra_channels = nSrcBands - 3;
            if (nSrcBands >= 4 &&
                poSrcDS->GetRasterBand(4)->GetColorInterpretation() ==
                    GCI_AlphaBand &&
                bAlphaDistanceSameAsMainChannel)
            {
                bHasInterleavedAlphaBand = true;
                basic_info.alpha_bits = basic_info.bits_per_sample;
                basic_info.alpha_exponent_bits =
                    basic_info.exponent_bits_per_sample;
            }
        }
        else
        {
            basic_info.num_color_channels = 1;
            basic_info.num_extra_channels = nSrcBands - 1;
        }
    }

    const int nBaseChannels = static_cast<int>(
        basic_info.num_color_channels + (bHasInterleavedAlphaBand ? 1 : 0));
    format.num_channels = nBaseChannels;

#ifndef HAVE_JxlEncoderInitExtraChannelInfo
    if (basic_info.num_extra_channels != (bHasInterleavedAlphaBand ? 1 : 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This version of libjxl does not support "
                 "creating non-alpha extra channels.");
        return nullptr;
    }
#endif

#ifdef HAVE_JXL_THREADS
    auto parallelRunner = JxlResizableParallelRunnerMake(nullptr);
    if (!parallelRunner)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlResizableParallelRunnerMake() failed");
        return nullptr;
    }

    const char *pszNumThreads = CSLFetchNameValue(papszOptions, "NUM_THREADS");
    if (pszNumThreads == nullptr)
        pszNumThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    uint32_t nMaxThreads = static_cast<uint32_t>(
        EQUAL(pszNumThreads, "ALL_CPUS") ? CPLGetNumCPUs()
                                         : atoi(pszNumThreads));
    if (nMaxThreads > 1024)
        nMaxThreads = 1024;  // to please Coverity

    const uint32_t nThreads =
        std::min(nMaxThreads, JxlResizableParallelRunnerSuggestThreads(
                                  basic_info.xsize, basic_info.ysize));
    CPLDebug("JPEGXL", "Using %u threads", nThreads);
    JxlResizableParallelRunnerSetThreads(parallelRunner.get(), nThreads);

    if (JxlEncoderSetParallelRunner(encoder.get(), JxlResizableParallelRunner,
                                    parallelRunner.get()) != JXL_ENC_SUCCESS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlEncoderSetParallelRunner() failed");
        return nullptr;
    }
#endif

#ifdef HAVE_JxlEncoderFrameSettingsCreate
    JxlEncoderFrameSettings *opts =
        JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
#else
    JxlEncoderOptions *opts = JxlEncoderOptionsCreate(encoder.get(), nullptr);
#endif
    if (opts == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlEncoderFrameSettingsCreate() failed");
        return nullptr;
    }

#ifdef HAVE_JxlEncoderSetCodestreamLevel
    if (poSrcDS->GetRasterXSize() > 262144 ||
        poSrcDS->GetRasterYSize() > 262144 ||
        poSrcDS->GetRasterXSize() > 268435456 / poSrcDS->GetRasterYSize())
    {
        JxlEncoderSetCodestreamLevel(encoder.get(), 10);
    }
#endif

    if (bLossless)
    {
#ifdef HAVE_JxlEncoderSetCodestreamLevel
        if (nBits > 12)
        {
            JxlEncoderSetCodestreamLevel(encoder.get(), 10);
        }
#endif

#ifdef HAVE_JxlEncoderSetFrameLossless
        JxlEncoderSetFrameLossless(opts, TRUE);
#else
        JxlEncoderOptionsSetLossless(opts, TRUE);
#endif
        basic_info.uses_original_profile = JXL_TRUE;
    }
    else
    {
#ifdef HAVE_JxlEncoderSetFrameDistance
        if (JxlEncoderSetFrameDistance(opts, fDistance) != JXL_ENC_SUCCESS)
#else
        if (JxlEncoderOptionsSetDistance(opts, fDistance) != JXL_ENC_SUCCESS)
#endif
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderSetFrameDistance() failed");
            return nullptr;
        }
    }

    const int nEffort = atoi(CSLFetchNameValueDef(papszOptions, "EFFORT", "5"));
#ifdef HAVE_JxlEncoderFrameSettingsSetOption
    if (JxlEncoderFrameSettingsSetOption(opts, JXL_ENC_FRAME_SETTING_EFFORT,
                                         nEffort) != JXL_ENC_SUCCESS)
#else
    if (JxlEncoderOptionsSetEffort(opts, nEffort) != JXL_ENC_SUCCESS)
#endif
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlEncoderFrameSettingsSetOption() failed");
        return nullptr;
    }

    std::vector<GByte> abyJPEG;
    void *pJPEGContent = nullptr;
    size_t nJPEGContent = 0;
    char *pszDetailedFormat = nullptr;
    // If the source dataset is a JPEG file or compatible of it, try to
    // losslessly add it
    if ((EQUAL(pszLossLessCopy, "AUTO") || CPLTestBool(pszLossLessCopy)) &&
        poSrcDS->ReadCompressedData(
            "JPEG", 0, 0, poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
            poSrcDS->GetRasterCount(), nullptr, &pJPEGContent, &nJPEGContent,
            &pszDetailedFormat) == CE_None)
    {
        CPLAssert(pszDetailedFormat != nullptr);
        const CPLStringList aosTokens(
            CSLTokenizeString2(pszDetailedFormat, ";", 0));
        VSIFree(pszDetailedFormat);
        const char *pszBitDepth = aosTokens.FetchNameValueDef("bit_depth", "");
        if (pJPEGContent && !EQUAL(pszBitDepth, "8"))
        {
            CPLDebug(
                "JPEGXL",
                "Unsupported bit_depth=%s for lossless transcoding from JPEG",
                pszBitDepth);
            VSIFree(pJPEGContent);
            pJPEGContent = nullptr;
        }
        const char *pszColorspace =
            aosTokens.FetchNameValueDef("colorspace", "");
        if (pJPEGContent && !EQUAL(pszColorspace, "unknown") &&
            !EQUAL(pszColorspace, "RGB") && !EQUAL(pszColorspace, "YCbCr"))
        {
            CPLDebug(
                "JPEGXL",
                "Unsupported colorspace=%s for lossless transcoding from JPEG",
                pszColorspace);
            VSIFree(pJPEGContent);
            pJPEGContent = nullptr;
        }
        const char *pszSOF = aosTokens.FetchNameValueDef("frame_type", "");
        if (pJPEGContent && !EQUAL(pszSOF, "SOF0_baseline") &&
            !EQUAL(pszSOF, "SOF1_extended_sequential") &&
            !EQUAL(pszSOF, "SOF2_progressive_huffman"))
        {
            CPLDebug(
                "JPEGXL",
                "Unsupported frame_type=%s for lossless transcoding from JPEG",
                pszSOF);
            VSIFree(pJPEGContent);
            pJPEGContent = nullptr;
        }
    }
    if (pJPEGContent)
    {
        try
        {
            abyJPEG.reserve(nJPEGContent);
            abyJPEG.insert(abyJPEG.end(), static_cast<GByte *>(pJPEGContent),
                           static_cast<GByte *>(pJPEGContent) + nJPEGContent);
            VSIFree(pJPEGContent);

            std::vector<GByte> abyJPEGMod;
            abyJPEGMod.reserve(abyJPEG.size());

            // Append Start Of Image marker (0xff 0xd8)
            abyJPEGMod.insert(abyJPEGMod.end(), abyJPEG.begin(),
                              abyJPEG.begin() + 2);

            // Rework JPEG data to remove APP (except APP0) and COM
            // markers as it confuses libjxl, when trying to
            // reconstruct a JPEG file
            size_t i = 2;
            while (i + 1 < abyJPEG.size())
            {
                if (abyJPEG[i] != 0xFF)
                {
                    // Not a valid tag (shouldn't happen)
                    abyJPEGMod.clear();
                    break;
                }

                // Stop when encountering a marker that is not a APP
                // or COM marker
                const bool bIsCOM = abyJPEG[i + 1] == 0xFE;
                if ((abyJPEG[i + 1] & 0xF0) != 0xE0 && !bIsCOM)
                {
                    // Append all markers until end
                    abyJPEGMod.insert(abyJPEGMod.end(), abyJPEG.begin() + i,
                                      abyJPEG.end());
                    break;
                }
                const bool bIsAPP0 = abyJPEG[i + 1] == 0xE0;

                // Skip marker ID
                i += 2;
                // Check we can read chunk length
                if (i + 1 >= abyJPEG.size())
                {
                    // Truncated JPEG file
                    abyJPEGMod.clear();
                    break;
                }
                const int nChunkLength = abyJPEG[i] * 256 + abyJPEG[i + 1];
                if ((bIsCOM || bIsAPP0) && i + nChunkLength <= abyJPEG.size())
                {
                    // Append COM or APP0 marker
                    abyJPEGMod.insert(abyJPEGMod.end(), abyJPEG.begin() + i - 2,
                                      abyJPEG.begin() + i + nChunkLength);
                }
                i += nChunkLength;
            }
            abyJPEG = std::move(abyJPEGMod);
        }
        catch (const std::exception &)
        {
        }
    }
    if (abyJPEG.empty() && !bLossless &&
        (!EQUAL(pszLossLessCopy, "AUTO") && CPLTestBool(pszLossLessCopy)))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "LOSSLESS_COPY=YES requested but not possible");
        return nullptr;
    }

    const char *pszICCProfile =
        CSLFetchNameValue(papszOptions, "SOURCE_ICC_PROFILE");
    if (pszICCProfile == nullptr)
    {
        pszICCProfile =
            poSrcDS->GetMetadataItem("SOURCE_ICC_PROFILE", "COLOR_PROFILE");
    }
    if (pszICCProfile && pszICCProfile[0] != '\0')
    {
        basic_info.uses_original_profile = JXL_TRUE;
    }

    if (abyJPEG.empty())
    {
        if (JXL_ENC_SUCCESS !=
            JxlEncoderSetBasicInfo(encoder.get(), &basic_info))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderSetBasicInfo() failed");
            return nullptr;
        }

        if (pszICCProfile && pszICCProfile[0] != '\0')
        {
            char *pEmbedBuffer = CPLStrdup(pszICCProfile);
            GInt32 nEmbedLen =
                CPLBase64DecodeInPlace(reinterpret_cast<GByte *>(pEmbedBuffer));
            if (JXL_ENC_SUCCESS !=
                JxlEncoderSetICCProfile(encoder.get(),
                                        reinterpret_cast<GByte *>(pEmbedBuffer),
                                        nEmbedLen))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetICCProfile() failed");
                CPLFree(pEmbedBuffer);
                return nullptr;
            }
            CPLFree(pEmbedBuffer);
        }
        else
        {
            JxlColorEncoding color_encoding;
            JxlColorEncodingSetToSRGB(&color_encoding,
                                      basic_info.num_color_channels ==
                                          1 /*is_gray*/);
            if (JXL_ENC_SUCCESS !=
                JxlEncoderSetColorEncoding(encoder.get(), &color_encoding))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetColorEncoding() failed");
                return nullptr;
            }
        }
    }

#ifdef HAVE_JxlEncoderInitExtraChannelInfo
    if (abyJPEG.empty() && basic_info.num_extra_channels > 0)
    {
        if (basic_info.num_extra_channels >= 5)
            JxlEncoderSetCodestreamLevel(encoder.get(), 10);

        for (int i = (bHasInterleavedAlphaBand ? 1 : 0);
             i < static_cast<int>(basic_info.num_extra_channels); ++i)
        {
            const int nBand =
                static_cast<int>(1 + basic_info.num_color_channels + i);
            const auto poBand = poSrcDS->GetRasterBand(nBand);
            JxlExtraChannelInfo extra_channel_info;
            const JxlExtraChannelType channelType =
                poBand->GetColorInterpretation() == GCI_AlphaBand
                    ? JXL_CHANNEL_ALPHA
                    : JXL_CHANNEL_OPTIONAL;
            JxlEncoderInitExtraChannelInfo(channelType, &extra_channel_info);
            extra_channel_info.bits_per_sample = basic_info.bits_per_sample;
            extra_channel_info.exponent_bits_per_sample =
                basic_info.exponent_bits_per_sample;

            const uint32_t nIndex = static_cast<uint32_t>(i);
            if (JXL_ENC_SUCCESS !=
                JxlEncoderSetExtraChannelInfo(encoder.get(), nIndex,
                                              &extra_channel_info))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetExtraChannelInfo() failed");
                return nullptr;
            }
            std::string osChannelName(CPLSPrintf("Band %d", nBand));
            const char *pszDescription = poBand->GetDescription();
            if (pszDescription && pszDescription[0] != '\0')
                osChannelName = pszDescription;
            if (JXL_ENC_SUCCESS !=
                JxlEncoderSetExtraChannelName(encoder.get(), nIndex,
                                              osChannelName.data(),
                                              osChannelName.size()))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetExtraChannelName() failed");
                return nullptr;
            }
#if HAVE_JxlEncoderSetExtraChannelDistance
            if (channelType == JXL_CHANNEL_ALPHA && fAlphaDistance >= 0.0f)
            {
                if (JXL_ENC_SUCCESS != JxlEncoderSetExtraChannelDistance(
                                           opts, nIndex, fAlphaDistance))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "JxlEncoderSetExtraChannelDistance failed");
                    return nullptr;
                }
            }
#endif
        }
    }
#endif

#ifdef HAVE_JXL_BOX_API
    const bool bCompressBox =
        CPLFetchBool(papszOptions, "COMPRESS_BOXES", false);

    if (papszXMP && papszXMP[0] && bWriteXMP)
    {
        JxlEncoderUseBoxes(encoder.get());

        const char *pszXMP = papszXMP[0];
        if (JxlEncoderAddBox(encoder.get(), "xml ",
                             reinterpret_cast<const uint8_t *>(pszXMP),
                             strlen(pszXMP), bCompressBox) != JXL_ENC_SUCCESS)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "JxlEncoderAddBox() failed");
            return nullptr;
        }
    }

    // Write "Exif" box with EXIF metadata
    if (papszEXIF && bWriteExifMetadata)
    {
        GUInt32 nMarkerSize = 0;
        GByte *pabyEXIF = EXIFCreate(papszEXIF, nullptr, 0, 0, 0,  // overview
                                     &nMarkerSize);
        CPLAssert(nMarkerSize > 6 && memcmp(pabyEXIF, "Exif\0\0", 6) == 0);
        // Add 4 leading bytes at 0
        std::vector<GByte> abyEXIF(4 + nMarkerSize - 6);
        memcpy(&abyEXIF[4], pabyEXIF + 6, nMarkerSize - 6);
        CPLFree(pabyEXIF);

        JxlEncoderUseBoxes(encoder.get());
        if (JxlEncoderAddBox(encoder.get(), "Exif", abyEXIF.data(),
                             abyEXIF.size(), bCompressBox) != JXL_ENC_SUCCESS)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "JxlEncoderAddBox() failed");
            return nullptr;
        }
    }

    // Write GeoJP2 box in a JUMBF box from georeferencing information
    if (poJUMBFBox)
    {
        GByte *pabyBoxData = poJUMBFBox->GetWritableBoxData();
        JxlEncoderUseBoxes(encoder.get());
        if (JxlEncoderAddBox(encoder.get(), "jumb", pabyBoxData,
                             static_cast<size_t>(poJUMBFBox->GetBoxLength()),
                             bCompressBox) != JXL_ENC_SUCCESS)
        {
            VSIFree(pabyBoxData);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderAddBox() failed for jumb");
            return nullptr;
        }
        VSIFree(pabyBoxData);
    }
#endif

    auto fp = std::unique_ptr<VSILFILE, VSILFileReleaser>(
        VSIFOpenL(pszFilename, "wb"));
    if (!fp)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s: %s", pszFilename,
                 VSIStrerror(errno));
        return nullptr;
    }

    int nPamMask = GCIF_PAM_DEFAULT;

    if (!abyJPEG.empty())
    {
#ifdef HAVE_JxlEncoderInitExtraChannelInfo
        const bool bHasMaskBand =
            basic_info.num_extra_channels == 0 &&
            poSrcDS->GetRasterBand(1)->GetMaskFlags() == GMF_PER_DATASET;
        if (bHasMaskBand)
        {
            nPamMask &= ~GCIF_MASK;

            basic_info.alpha_bits = basic_info.bits_per_sample;
            basic_info.num_extra_channels = 1;
            if (JXL_ENC_SUCCESS !=
                JxlEncoderSetBasicInfo(encoder.get(), &basic_info))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetBasicInfo() failed");
                return nullptr;
            }

            JxlExtraChannelInfo extra_channel_info;
            JxlEncoderInitExtraChannelInfo(JXL_CHANNEL_ALPHA,
                                           &extra_channel_info);
            extra_channel_info.bits_per_sample = basic_info.bits_per_sample;
            extra_channel_info.exponent_bits_per_sample =
                basic_info.exponent_bits_per_sample;

            if (JXL_ENC_SUCCESS != JxlEncoderSetExtraChannelInfo(
                                       encoder.get(), 0, &extra_channel_info))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetExtraChannelInfo() failed");
                return nullptr;
            }
        }
#endif

        CPLDebug("JPEGXL", "Adding JPEG frame");
        JxlEncoderStoreJPEGMetadata(encoder.get(), true);
        if (JxlEncoderAddJPEGFrame(opts, abyJPEG.data(), abyJPEG.size()) !=
            JXL_ENC_SUCCESS)
        {
            if (EQUAL(pszLossLessCopy, "AUTO"))
            {
                // could happen with a file with arithmetic encoding for example
                CPLDebug("JPEGXL",
                         "JxlEncoderAddJPEGFrame() framed. "
                         "Perhaps unsupported JPEG formulation for libjxl. "
                         "Retrying with normal code path");
                CPLStringList aosOptions(papszOptions);
                aosOptions.SetNameValue("LOSSLESS_COPY", "NO");
                CPLConfigOptionSetter oSetter("GDAL_ERROR_ON_LIBJPEG_WARNING",
                                              "YES", true);
                return CreateCopy(pszFilename, poSrcDS, FALSE,
                                  aosOptions.List(), pfnProgress,
                                  pProgressData);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderAddJPEGFrame() failed");
                return nullptr;
            }
        }

#ifdef HAVE_JxlEncoderInitExtraChannelInfo
        if (bHasMaskBand)
        {
            JxlColorEncoding color_encoding;
            JxlColorEncodingSetToSRGB(&color_encoding,
                                      basic_info.num_color_channels ==
                                          1 /*is_gray*/);
            // libjxl until commit
            // https://github.com/libjxl/libjxl/commits/c70c9d0bdc03f77d6bd8d9c3c56d4dac1b9b1652
            // needs JxlEncoderSetColorEncoding()
            // But post it (308b5f1eed81becac506569080e4490cc486660c,
            // "Use chunked frame adapter instead of image bundle in
            // EncodeFrame. (#2983)"), this errors out.
            CPL_IGNORE_RET_VAL(
                JxlEncoderSetColorEncoding(encoder.get(), &color_encoding));

            const auto nDataSize = GDALGetDataTypeSizeBytes(eDT);
            if (nDataSize <= 0 ||
                static_cast<size_t>(poSrcDS->GetRasterXSize()) >
                    std::numeric_limits<size_t>::max() /
                        poSrcDS->GetRasterYSize() / nDataSize)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Image too big for architecture");
                return nullptr;
            }
            const size_t nInputDataSize =
                static_cast<size_t>(poSrcDS->GetRasterXSize()) *
                poSrcDS->GetRasterYSize() * nDataSize;

            std::vector<GByte> abyInputData;
            try
            {
                abyInputData.resize(nInputDataSize);
            }
            catch (const std::exception &e)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate image buffer: %s", e.what());
                return nullptr;
            }

            format.num_channels = 1;
            if (poSrcDS->GetRasterBand(1)->GetMaskBand()->RasterIO(
                    GF_Read, 0, 0, poSrcDS->GetRasterXSize(),
                    poSrcDS->GetRasterYSize(), abyInputData.data(),
                    poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), eDT,
                    0, 0, nullptr) != CE_None)
            {
                return nullptr;
            }
            if (JxlEncoderSetExtraChannelBuffer(
                    opts, &format, abyInputData.data(),
                    static_cast<size_t>(poSrcDS->GetRasterXSize()) *
                        poSrcDS->GetRasterYSize() * nDataSize,
                    0) != JXL_ENC_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetExtraChannelBuffer() failed");
                return nullptr;
            }
        }
#endif
    }
    else
    {
        const auto nDataSize = GDALGetDataTypeSizeBytes(eDT);

        if (nDataSize <= 0 || static_cast<size_t>(poSrcDS->GetRasterXSize()) >
                                  std::numeric_limits<size_t>::max() /
                                      poSrcDS->GetRasterYSize() /
                                      nBaseChannels / nDataSize)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Image too big for architecture");
            return nullptr;
        }
        const size_t nInputDataSize =
            static_cast<size_t>(poSrcDS->GetRasterXSize()) *
            poSrcDS->GetRasterYSize() * nBaseChannels * nDataSize;

        std::vector<GByte> abyInputData;
        try
        {
            abyInputData.resize(nInputDataSize);
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate image buffer: %s", e.what());
            return nullptr;
        }

        if (poSrcDS->RasterIO(
                GF_Read, 0, 0, poSrcDS->GetRasterXSize(),
                poSrcDS->GetRasterYSize(), abyInputData.data(),
                poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), eDT,
                nBaseChannels, nullptr, nDataSize * nBaseChannels,
                nDataSize * nBaseChannels * poSrcDS->GetRasterXSize(),
                nDataSize, nullptr) != CE_None)
        {
            return nullptr;
        }

        const auto Rescale = [eDT, nBits, poSrcDS](void *pBuffer, int nChannels)
        {
            // Rescale to 8-bits/16-bits
            if ((eDT == GDT_Byte && nBits < 8) ||
                (eDT == GDT_UInt16 && nBits < 16))
            {
                const size_t nSamples =
                    static_cast<size_t>(poSrcDS->GetRasterXSize()) *
                    poSrcDS->GetRasterYSize() * nChannels;
                const int nMaxVal = (1 << nBits) - 1;
                const int nMavValHalf = nMaxVal / 2;
                if (eDT == GDT_Byte)
                {
                    uint8_t *panData = static_cast<uint8_t *>(pBuffer);
                    for (size_t i = 0; i < nSamples; ++i)
                    {
                        panData[i] = static_cast<GByte>(
                            (std::min(static_cast<int>(panData[i]), nMaxVal) *
                                 255 +
                             nMavValHalf) /
                            nMaxVal);
                    }
                }
                else if (eDT == GDT_UInt16)
                {
                    uint16_t *panData = static_cast<uint16_t *>(pBuffer);
                    for (size_t i = 0; i < nSamples; ++i)
                    {
                        panData[i] = static_cast<uint16_t>(
                            (std::min(static_cast<int>(panData[i]), nMaxVal) *
                                 65535 +
                             nMavValHalf) /
                            nMaxVal);
                    }
                }
            }
        };

        Rescale(abyInputData.data(), nBaseChannels);

        if (JxlEncoderAddImageFrame(opts, &format, abyInputData.data(),
                                    abyInputData.size()) != JXL_ENC_SUCCESS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderAddImageFrame() failed");
            return nullptr;
        }

#ifdef HAVE_JxlEncoderInitExtraChannelInfo
        format.num_channels = 1;
        for (int i = nBaseChannels; i < poSrcDS->GetRasterCount(); ++i)
        {
            if (poSrcDS->GetRasterBand(i + 1)->RasterIO(
                    GF_Read, 0, 0, poSrcDS->GetRasterXSize(),
                    poSrcDS->GetRasterYSize(), abyInputData.data(),
                    poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), eDT,
                    0, 0, nullptr) != CE_None)
            {
                return nullptr;
            }

            Rescale(abyInputData.data(), 1);

            if (JxlEncoderSetExtraChannelBuffer(
                    opts, &format, abyInputData.data(),
                    static_cast<size_t>(poSrcDS->GetRasterXSize()) *
                        poSrcDS->GetRasterYSize() * nDataSize,
                    i - nBaseChannels + (bHasInterleavedAlphaBand ? 1 : 0)) !=
                JXL_ENC_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetExtraChannelBuffer() failed");
                return nullptr;
            }
        }
#endif
    }

    JxlEncoderCloseInput(encoder.get());

    // Flush to file
    std::vector<GByte> abyOutputBuffer(4096 * 10);
    while (true)
    {
        size_t len = abyOutputBuffer.size();
        uint8_t *buf = abyOutputBuffer.data();
        JxlEncoderStatus process_result =
            JxlEncoderProcessOutput(encoder.get(), &buf, &len);
        if (process_result == JXL_ENC_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderProcessOutput() failed");
            return nullptr;
        }
        size_t nToWrite = abyOutputBuffer.size() - len;
        if (VSIFWriteL(abyOutputBuffer.data(), 1, nToWrite, fp.get()) !=
            nToWrite)
        {
            CPLError(CE_Failure, CPLE_FileIO, "VSIFWriteL() failed");
            return nullptr;
        }
        if (process_result != JXL_ENC_NEED_MORE_OUTPUT)
            break;
    }

    fp.reset();

    if (pfnProgress)
        pfnProgress(1.0, "", pProgressData);

    // Re-open file and clone missing info to PAM
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    auto poDS = OpenStaticPAM(&oOpenInfo);
    if (poDS)
    {
        // Do not create a .aux.xml file just for AREA_OR_POINT=Area
        const char *pszAreaOfPoint =
            poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        if (pszAreaOfPoint && EQUAL(pszAreaOfPoint, GDALMD_AOP_AREA))
        {
            poDS->SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA);
            poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
        }
#ifdef HAVE_JXL_BOX_API
        // When copying from JPEG, expose the EXIF metadata in the main domain,
        // so that PAM doesn't copy it.
        if (bEXIFFromMainDomain)
        {
            for (CSLConstList papszIter = papszEXIF; papszIter && *papszIter;
                 ++papszIter)
            {
                if (STARTS_WITH(*papszIter, "EXIF_"))
                {
                    char *pszKey = nullptr;
                    const char *pszValue =
                        CPLParseNameValue(*papszIter, &pszKey);
                    if (pszKey && pszValue)
                    {
                        poDS->SetMetadataItem(pszKey, pszValue);
                    }
                    CPLFree(pszKey);
                }
            }
            poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
        }
#endif
        poDS->CloneInfo(poSrcDS, nPamMask);
    }

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_JPEGXL()                         */
/************************************************************************/

void GDALRegister_JPEGXL()

{
    if (GDALGetDriverByName("JPEGXL") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    JPEGXLDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = JPEGXLDataset::OpenStatic;
    poDriver->pfnIdentify = JPEGXLDataset::Identify;
    poDriver->pfnCreateCopy = JPEGXLDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
