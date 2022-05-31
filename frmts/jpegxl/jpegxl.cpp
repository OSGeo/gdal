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

#include <algorithm>
#include <cstdlib>
#include <limits>

#include "jxl_headers.h"

namespace
{
    struct VSILFileReleaser
    {
        void operator()(VSILFILE* fp) { if(fp) VSIFCloseL(fp); }
    };
}

/************************************************************************/
/*                        JPEGXLDataset                                 */
/************************************************************************/

class JPEGXLDataset final: public GDALJP2AbstractDataset
{
    friend class JPEGXLRasterBand;

    VSILFILE           *m_fp = nullptr;
    JxlDecoderPtr      m_decoder{};
#ifdef HAVE_JXL_THREADS
    JxlResizableParallelRunnerPtr m_parallelRunner{};
#endif
    bool                m_bDecodingFailed = false;
    std::vector<GByte>  m_abyImage{};
    std::vector<std::vector<GByte>> m_abyExtraChannels{};
    std::vector<GByte>  m_abyInputData{};
    int                 m_nBits = 0;
    int                 m_nNonAlphaExtraChannels = 0;
#ifdef HAVE_JXL_BOX_API
    std::string         m_osXMP{};
    char*               m_apszXMP[2] = {nullptr, nullptr};
    CPLStringList       m_aosEXIFMetadata{};
    bool                m_bHasJPEGReconstructionData = false;
#endif

    bool                Open(GDALOpenInfo* poOpenInfo);

    void                GetDecodedImage(void* pabyOuputData, size_t nOutputDataSize);

protected:

    CPLErr              IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, GSpacing, GSpacing, GSpacing,
                              GDALRasterIOExtraArg* psExtraArg ) override;

public:
                       ~JPEGXLDataset();

    char**              GetMetadataDomainList() override;
    char**              GetMetadata(const char* pszDomain) override;
    const char*         GetMetadataItem(const char* pszName, const char* pszDomain) override;

    const std::vector<GByte>& GetDecodedImage();

    static int          Identify(GDALOpenInfo* poOpenInfo);
    static GDALPamDataset* OpenStaticPAM(GDALOpenInfo* poOpenInfo);
    static GDALDataset* OpenStatic(GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress, void *pProgressData );
};

/************************************************************************/
/*                      JPEGXLRasterBand                                */
/************************************************************************/

class JPEGXLRasterBand final: public GDALPamRasterBand
{
protected:
    CPLErr              IReadBlock(int nBlockXOff, int nBlockYOff, void* pData) override;

    CPLErr              IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing, GSpacing,
                              GDALRasterIOExtraArg* psExtraArg ) override;

public:
                        JPEGXLRasterBand(JPEGXLDataset* poDSIn,
                                         int nBandIn,
                                         GDALDataType eDataTypeIn,
                                         int nBitsPerSample,
                                         GDALColorInterp eInterp);
};

/************************************************************************/
/*                         ~JPEGXLDataset()                             */
/************************************************************************/

JPEGXLDataset::~JPEGXLDataset()
{
    if( m_fp )
        VSIFCloseL(m_fp);
}

/************************************************************************/
/*                         JPEGXLRasterBand()                           */
/************************************************************************/

JPEGXLRasterBand::JPEGXLRasterBand(JPEGXLDataset* poDSIn,
                                   int nBandIn,
                                   GDALDataType eDataTypeIn,
                                   int nBitsPerSample,
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
    if( (eDataType == GDT_Byte && nBitsPerSample < 8) ||
        (eDataType == GDT_UInt16 && nBitsPerSample < 16) )
    {
        SetMetadataItem("NBITS",
                        CPLSPrintf("%d", nBitsPerSample),
                        "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPEGXLRasterBand::IReadBlock(int /*nBlockXOff*/, int nBlockYOff, void* pData)
{
    auto poGDS = cpl::down_cast<JPEGXLDataset*>(poDS);

    const auto& abyDecodedImage = poGDS->GetDecodedImage();
    if( abyDecodedImage.empty() )
    {
        return CE_Failure;
    }

    const auto nDataSize = GDALGetDataTypeSizeBytes(eDataType);
    const int nNonExtraBands = poGDS->nBands - poGDS->m_nNonAlphaExtraChannels;
    if( nBand <= nNonExtraBands )
    {
        GDALCopyWords( abyDecodedImage.data() +
                        ((nBand - 1) + static_cast<size_t>(nBlockYOff) * nRasterXSize * nNonExtraBands) * nDataSize,
                       eDataType,
                       nDataSize * nNonExtraBands,
                       pData,
                       eDataType,
                       nDataSize,
                       nRasterXSize );
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
/*                      IsJPEGXLContainer()                             */
/************************************************************************/

static bool IsJPEGXLContainer(GDALOpenInfo* poOpenInfo)
{
    constexpr const GByte abyJXLContainerSignature[] =
        {0x00, 0x00, 0x00, 0x0C, 'J', 'X', 'L', ' ', 0x0D, 0x0A, 0x87, 0x0A };
    return( poOpenInfo->nHeaderBytes >= static_cast<int>(sizeof(abyJXLContainerSignature)) &&
            memcmp(poOpenInfo->pabyHeader,
                   abyJXLContainerSignature,
                   sizeof(abyJXLContainerSignature)) == 0 );
}


/************************************************************************/
/*                         Identify()                                   */
/************************************************************************/

int JPEGXLDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( poOpenInfo->fpL == nullptr )
        return false;

    // See https://github.com/libjxl/libjxl/blob/c98f133f3f5e456caaa2ba00bc920e923b713abc/lib/jxl/decode.cc#L107-L138

    // JPEG XL codestream
    if( poOpenInfo->nHeaderBytes >= 2 &&
        poOpenInfo->pabyHeader[0] == 0xff &&
        poOpenInfo->pabyHeader[1] == 0x0a )
    {
        // Two bytes is not enough to reliably identify, so let's try to decode
        // basic info
        auto decoder = JxlDecoderMake(nullptr);
        if( !decoder )
            return false;
        JxlDecoderStatus status = JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO);
        if( status != JXL_DEC_SUCCESS )
        {
            return false;
        }

        status = JxlDecoderSetInput(decoder.get(), poOpenInfo->pabyHeader,
                                    poOpenInfo->nHeaderBytes);
        if( status != JXL_DEC_SUCCESS )
        {
            return false;
        }

        status = JxlDecoderProcessInput(decoder.get());
        if( status != JXL_DEC_BASIC_INFO )
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

bool JPEGXLDataset::Open(GDALOpenInfo* poOpenInfo)
{
    m_decoder = JxlDecoderMake(nullptr);
    if( !m_decoder )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlDecoderMake() failed");
        return false;
    }

#ifdef HAVE_JXL_THREADS
    m_parallelRunner = JxlResizableParallelRunnerMake(nullptr);
    if( !m_parallelRunner )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlResizableParallelRunnerMake() failed");
        return false;
    }

    if( JxlDecoderSetParallelRunner(m_decoder.get(), JxlResizableParallelRunner, m_parallelRunner.get()) !=
            JXL_DEC_SUCCESS )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlDecoderSetParallelRunner() failed");
        return false;
    }
#endif

    JxlDecoderStatus status = JxlDecoderSubscribeEvents(m_decoder.get(),
        JXL_DEC_BASIC_INFO |
#ifdef HAVE_JXL_BOX_API
        JXL_DEC_BOX |
#endif
        JXL_DEC_COLOR_ENCODING);
    if( status != JXL_DEC_SUCCESS )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlDecoderSubscribeEvents() failed");
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
        const size_t nRemainingBytesInBuffer = JxlDecoderReleaseBoxBuffer(m_decoder.get());
        CPLAssert(nRemainingBytesInBuffer < abyBoxBuffer.size());
        if( osCurrentBox == "xml " && m_osXMP.empty() )
        {
            std::string osXML(reinterpret_cast<char*>(abyBoxBuffer.data()),
                              abyBoxBuffer.size() - nRemainingBytesInBuffer);
            if( osXML.compare(0, strlen("<?xpacket"), "<?xpacket") == 0 )
            {
                m_osXMP = std::move(osXML);
            }
        }
        else if( osCurrentBox == "Exif" && m_aosEXIFMetadata.empty() )
        {
            const size_t nSize = abyBoxBuffer.size() - nRemainingBytesInBuffer;
            // The first 4 bytes are at 0, before the TIFF EXIF file content
            if( nSize > 12 && abyBoxBuffer[0] == 0 && abyBoxBuffer[1] == 0 &&
                abyBoxBuffer[2] == 0 && abyBoxBuffer[3] == 0 &&
                (abyBoxBuffer[4] == 0x4d // TIFF_BIGENDIAN
                 || abyBoxBuffer[4] == 0x49 /* TIFF_LITTLEENDIAN */) )
            {
#ifdef CPL_LSB
                const bool bSwab = abyBoxBuffer[4] == 0x4d;
#else
                const bool bSwab = abyBoxBuffer[4] == 0x49;
#endif
                constexpr int nTIFFHEADER = 0;
                uint32_t nTiffDirStart;
                memcpy(&nTiffDirStart, abyBoxBuffer.data() + 8, sizeof(uint32_t));
                if( bSwab )
                {
                    CPL_LSBPTR32(&nTiffDirStart);
                }
                const std::string osTmpFilename = CPLSPrintf("/vsimem/jxl/%p", this);
                VSILFILE* fpEXIF = VSIFileFromMemBuffer(osTmpFilename.c_str(),
                                                        abyBoxBuffer.data() + 4,
                                                        abyBoxBuffer.size() - 4, false);
                int    nExifOffset = 0;
                int    nInterOffset = 0;
                int    nGPSOffset = 0;
                char** papszEXIFMetadata = nullptr;
                EXIFExtractMetadata(papszEXIFMetadata,
                        fpEXIF, nTiffDirStart,
                        bSwab, nTIFFHEADER,
                        nExifOffset, nInterOffset, nGPSOffset);

                if(nExifOffset  > 0){
                    EXIFExtractMetadata(papszEXIFMetadata,
                                        fpEXIF, nExifOffset,
                                        bSwab, nTIFFHEADER,
                                        nExifOffset, nInterOffset, nGPSOffset);
                }
                if(nInterOffset > 0) {
                    EXIFExtractMetadata(papszEXIFMetadata,
                                        fpEXIF, nInterOffset,
                                        bSwab, nTIFFHEADER,
                                        nExifOffset, nInterOffset, nGPSOffset);
                }
                if(nGPSOffset > 0) {
                    EXIFExtractMetadata(papszEXIFMetadata,
                                        fpEXIF, nGPSOffset,
                                        bSwab, nTIFFHEADER,
                                        nExifOffset, nInterOffset, nGPSOffset);
                }
                VSIFCloseL(fpEXIF);
                m_aosEXIFMetadata.Assign(papszEXIFMetadata, /*takeOwnership=*/ true);
            }
        }
        else if( osCurrentBox == "jumb" )
        {
            abyJumbBoxBuffer = abyBoxBuffer;
        }
        osCurrentBox.clear();
    };

    // Process input to get boxes and basic info
    const uint64_t nMaxBoxBufferSize = std::strtoull(
        CPLGetConfigOption("GDAL_JPEGXL_MAX_BOX_BUFFER_SIZE", "100000000"), nullptr, 10);
#endif

    int l_nBands = 0;
    GDALDataType eDT = GDT_Unknown;

    while( true )
    {
        status = JxlDecoderProcessInput(m_decoder.get());

#ifdef HAVE_JXL_BOX_API
        if( (status == JXL_DEC_SUCCESS || status == JXL_DEC_BOX) && !osCurrentBox.empty() )
        {
            try
            {
                ProcessCurrentBox();
            }
            catch( const std::exception& )
            {
                CPLError(CE_Warning, CPLE_OutOfMemory,
                         "Not enough memory to read box '%s'",
                         osCurrentBox.c_str());
            }
        }
#endif

        if( status == JXL_DEC_SUCCESS )
        {
            break;
        }
        else if( status == JXL_DEC_NEED_MORE_INPUT )
        {
            JxlDecoderReleaseInput(m_decoder.get());

            const size_t nRead = VSIFReadL(m_abyInputData.data(), 1, m_abyInputData.size(), m_fp);
            if( nRead == 0 )
            {
                // For some reason, it can happen that JXL_DEC_NEED_MORE_INPUT
                // is called whereas we have reached end of file
                break;
            }
            if( JxlDecoderSetInput(m_decoder.get(), m_abyInputData.data(), nRead) !=
                    JXL_DEC_SUCCESS )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderSetInput() failed");
                return false;
            }
        }
        else if( status == JXL_DEC_BASIC_INFO )
        {
            bGotInfo = true;
            status = JxlDecoderGetBasicInfo(m_decoder.get(), &info);
            if( status != JXL_DEC_SUCCESS )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderGetBasicInfo() failed");
                return false;
            }

            if( info.xsize > static_cast<uint32_t>(INT_MAX) ||
                info.ysize > static_cast<uint32_t>(INT_MAX) )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too big raster");
                return false;
            }

            CPLDebug("JPEGXL", "uses_original_profile = %d",
                     info.uses_original_profile);

            nRasterXSize = static_cast<int>(info.xsize);
            nRasterYSize = static_cast<int>(info.ysize);

            m_nBits = info.bits_per_sample;
            if( info.exponent_bits_per_sample == 0 )
            {
                if( info.bits_per_sample <= 8 )
                    eDT = GDT_Byte;
                else if( info.bits_per_sample <= 16 )
                    eDT = GDT_UInt16;
            }
            else if( info.exponent_bits_per_sample == 8 )
            {
                eDT = GDT_Float32;
            }
            if( eDT == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Unhandled data type");
                return false;
            }

            l_nBands = static_cast<int>(info.num_color_channels) +
                       static_cast<int>(info.num_extra_channels);
            if( info.num_extra_channels == 1 &&
                (info.num_color_channels == 1 || info.num_color_channels == 3) &&
                info.alpha_bits != 0 )
            {
                m_nNonAlphaExtraChannels = 0;
            }
            else
            {
                m_nNonAlphaExtraChannels = static_cast<int>(info.num_extra_channels);
            }
        }
#ifdef HAVE_JXL_BOX_API
        else if( status == JXL_DEC_BOX )
        {
            osCurrentBox.clear();
            JxlBoxType type = { 0 };
            if( JxlDecoderGetBoxType(m_decoder.get(), type,
                                     /* decompressed = */ TRUE) != JXL_DEC_SUCCESS )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "JxlDecoderGetBoxType() failed");
                continue;
            }
            char szType[5] = { 0 };
            memcpy(szType, type, sizeof(type));
            // CPLDebug("JPEGXL", "box: %s", szType);
            if( strcmp(szType, "xml ") == 0 ||
                strcmp(szType, "Exif") == 0 ||
                strcmp(szType, "jumb") == 0 )
            {
                uint64_t nRawSize = 0;
                JxlDecoderGetBoxSizeRaw(m_decoder.get(), &nRawSize);
                if( nRawSize > nMaxBoxBufferSize )
                {
                    CPLError(CE_Warning, CPLE_OutOfMemory,
                             "Reading a '%s' box involves at least " CPL_FRMT_GUIB " bytes, "
                             "but the current limitation of the GDAL_JPEGXL_MAX_BOX_BUFFER_SIZE "
                             "configuration option is " CPL_FRMT_GUIB " bytes",
                             szType,
                             static_cast<GUIntBig>(nRawSize),
                             static_cast<GUIntBig>(nMaxBoxBufferSize));
                    continue;
                }
                if( nRawSize > abyBoxBuffer.size() )
                {
                    if( nRawSize > std::numeric_limits<size_t>::max() / 2 )
                    {
                        CPLError(CE_Warning, CPLE_OutOfMemory,
                                 "Not enough memory to read box '%s'",
                                 szType);
                        continue;
                    }
                    try
                    {
                        abyBoxBuffer.clear();
                        abyBoxBuffer.resize(static_cast<size_t>(nRawSize));
                    }
                    catch( const std::exception& )
                    {
                        abyBoxBuffer.resize(1024 * 1024);
                        CPLError(CE_Warning, CPLE_OutOfMemory,
                                 "Not enough memory to read box '%s'",
                                 szType);
                        continue;
                    }
                }

                if( JxlDecoderSetBoxBuffer(m_decoder.get(), abyBoxBuffer.data(),
                                           abyBoxBuffer.size()) != JXL_DEC_SUCCESS )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "JxlDecoderSetBoxBuffer() failed");
                    continue;
                }
                osCurrentBox = szType;
            }
            else if( strcmp(szType, "jbrd") == 0 )
            {
                m_bHasJPEGReconstructionData = true;
            }
        }
#endif
        else if( status == JXL_DEC_COLOR_ENCODING )
        {
            JxlPixelFormat format = {static_cast<uint32_t>(nBands),
                                     eDT == GDT_Byte ?   JXL_TYPE_UINT8:
                                     eDT == GDT_UInt16 ? JXL_TYPE_UINT16:
                                                         JXL_TYPE_FLOAT,
                                     JXL_NATIVE_ENDIAN,
                                     0 /* alignment */
            };

            bool bIsDefaultColorEncoding = false;
            JxlColorEncoding color_encoding;

            // Check if the color profile is the default one we set on creation.
            // If so, do not expose it as ICC color profile
            if( JXL_DEC_SUCCESS == JxlDecoderGetColorAsEncodedProfile(
                m_decoder.get(), &format,
                    JXL_COLOR_PROFILE_TARGET_DATA, &color_encoding) )
            {
                JxlColorEncoding default_color_encoding;
                JxlColorEncodingSetToSRGB(&default_color_encoding,
                    info.num_color_channels == 1 /*is_gray*/);

                bIsDefaultColorEncoding =
                    color_encoding.color_space == default_color_encoding.color_space &&
                    color_encoding.white_point == default_color_encoding.white_point &&
                    color_encoding.white_point_xy[0] == default_color_encoding.white_point_xy[0] &&
                    color_encoding.white_point_xy[1] == default_color_encoding.white_point_xy[1] &&
                    (color_encoding.color_space == JXL_COLOR_SPACE_GRAY ||
                     color_encoding.color_space == JXL_COLOR_SPACE_XYB ||
                     (color_encoding.primaries == default_color_encoding.primaries &&
                      color_encoding.primaries_red_xy[0] == default_color_encoding.primaries_red_xy[0] &&
                      color_encoding.primaries_red_xy[1] == default_color_encoding.primaries_red_xy[1] &&
                      color_encoding.primaries_green_xy[0] == default_color_encoding.primaries_green_xy[0] &&
                      color_encoding.primaries_green_xy[1] == default_color_encoding.primaries_green_xy[1] &&
                      color_encoding.primaries_blue_xy[0] == default_color_encoding.primaries_blue_xy[0] &&
                      color_encoding.primaries_blue_xy[1] == default_color_encoding.primaries_blue_xy[1])) &&
                    color_encoding.transfer_function == default_color_encoding.transfer_function &&
                    color_encoding.gamma == default_color_encoding.gamma &&
                    color_encoding.rendering_intent == default_color_encoding.rendering_intent;
            }

            if( !bIsDefaultColorEncoding )
            {
                size_t icc_size = 0;
                if( JXL_DEC_SUCCESS == JxlDecoderGetICCProfileSize(
                        m_decoder.get(), &format,
                        JXL_COLOR_PROFILE_TARGET_DATA, &icc_size) )
                {
                    std::vector<GByte> icc(icc_size);
                    if( JXL_DEC_SUCCESS == JxlDecoderGetColorAsICCProfile(
                            m_decoder.get(), &format,
                            JXL_COLOR_PROFILE_TARGET_DATA,
                            icc.data(), icc_size) )
                    {
                        // Escape the profile.
                        char *pszBase64Profile =
                            CPLBase64Encode(static_cast<int>(icc.size()), icc.data());

                        // Set ICC profile metadata.
                        SetMetadataItem("SOURCE_ICC_PROFILE", pszBase64Profile,
                                        "COLOR_PROFILE");

                        CPLFree(pszBase64Profile);
                    }
                }
            }
        }
#ifdef HAVE_JXL_BOX_API
        else if( status == JXL_DEC_BOX_NEED_MORE_OUTPUT )
        {
            // Grow abyBoxBuffer if it is too small
            const size_t nRemainingBytesInBuffer = JxlDecoderReleaseBoxBuffer(m_decoder.get());
            const size_t nBytesUsed = abyBoxBuffer.size() - nRemainingBytesInBuffer;
            if( abyBoxBuffer.size() > std::numeric_limits<size_t>::max() / 2 )
            {
                CPLError(CE_Warning, CPLE_OutOfMemory,
                         "Not enough memory to read box '%s'",
                         osCurrentBox.c_str());
                osCurrentBox.clear();
                continue;
            }
            const size_t nNewBoxBufferSize = abyBoxBuffer.size() * 2;
            if( nNewBoxBufferSize > nMaxBoxBufferSize )
            {
                CPLError(CE_Warning, CPLE_OutOfMemory,
                         "Reading a '%s' box involves at least " CPL_FRMT_GUIB " bytes, "
                         "but the current limitation of the GDAL_JPEGXL_MAX_BOX_BUFFER_SIZE "
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
            catch( const std::exception& )
            {
                CPLError(CE_Warning, CPLE_OutOfMemory,
                         "Not enough memory to read box '%s'",
                         osCurrentBox.c_str());
                osCurrentBox.clear();
                continue;
            }
            if( JxlDecoderSetBoxBuffer(m_decoder.get(),
                                       abyBoxBuffer.data() + nBytesUsed,
                                       abyBoxBuffer.size() - nBytesUsed) != JXL_DEC_SUCCESS )
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
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unexpected event: %d", status);
            break;
        }
    }

    JxlDecoderReleaseInput(m_decoder.get());

#ifdef HAVE_JXL_BOX_API
    // Load georeferencing from jumb box or from worldfile sidecar.
    if( !abyJumbBoxBuffer.empty() )
    {
        VSILFILE* fpJUMB = VSIFileFromMemBuffer(nullptr,
                                                abyJumbBoxBuffer.data(),
                                                abyJumbBoxBuffer.size(), false);
        LoadJP2Metadata(poOpenInfo, nullptr, fpJUMB);
        VSIFCloseL(fpJUMB);
    }
#else
    if( IsJPEGXLContainer(poOpenInfo) )
    {
        // A JPEGXL container can be explored with the JPEG2000 box reading logic
        VSIFSeekL(m_fp, 12, SEEK_SET);
        poOpenInfo->fpL = m_fp;
        LoadJP2Metadata(poOpenInfo);
        poOpenInfo->fpL = nullptr;
    }
#endif
    else
    {
        // Only try to read worldfile
        VSILFILE* fpDummy = VSIFileFromMemBuffer(nullptr, nullptr, 0, false);
        LoadJP2Metadata(poOpenInfo, nullptr, fpDummy);
        VSIFCloseL(fpDummy);
    }

    if( !bGotInfo )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Did not get basic info");
        return false;
    }

#ifdef HAVE_JXL_THREADS
    const char* pszNumThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    uint32_t nMaxThreads =
        static_cast<uint32_t>(EQUAL(pszNumThreads, "ALL_CPUS") ? CPLGetNumCPUs() : atoi(pszNumThreads));
    if( nMaxThreads > 1024 )
        nMaxThreads = 1024; // to please Coverity

    const uint32_t nThreads = std::min(
        nMaxThreads, JxlResizableParallelRunnerSuggestThreads(
                                     info.xsize, info.ysize));
    CPLDebug("JPEGXL", "Using %u threads", nThreads);
    JxlResizableParallelRunnerSetThreads(m_parallelRunner.get(), nThreads);
#endif

    // Instanciate bands
    const int nNonExtraBands = l_nBands - m_nNonAlphaExtraChannels;
    for(int i = 1; i <= l_nBands; i++ )
    {
        GDALColorInterp eInterp = GCI_Undefined;
        if( info.num_color_channels == 1 )
        {
            if( i == 1 && l_nBands <= 2 )
                eInterp = GCI_GrayIndex;
            else if( i == 2 && info.num_extra_channels == 1 && info.alpha_bits != 0 )
                eInterp = GCI_AlphaBand;
        }
        else if( info.num_color_channels == 3 )
        {
            if( i <= 3 )
                eInterp = static_cast<GDALColorInterp>(GCI_RedBand + (i-1));
            else if( i == 4 && info.num_extra_channels == 1 && info.alpha_bits != 0 )
                eInterp = GCI_AlphaBand;
        }
        std::string osBandName;

        if( i - 1 >= nNonExtraBands )
        {
            JxlExtraChannelInfo sExtraInfo;
            memset(&sExtraInfo, 0, sizeof(sExtraInfo));
            const size_t nIndex = static_cast<size_t>(i - 1 - nNonExtraBands);
            if( JxlDecoderGetExtraChannelInfo(m_decoder.get(), nIndex, &sExtraInfo) == JXL_DEC_SUCCESS )
            {
                if( sExtraInfo.name_length > 0 )
                {
                    std::string osName;
                    osName.resize(sExtraInfo.name_length);
                    if( JxlDecoderGetExtraChannelName(
                        m_decoder.get(), nIndex,
                        &osName[0], osName.size() + 1) == JXL_DEC_SUCCESS &&
                        osName != CPLSPrintf("Band %d", i) )
                    {
                        osBandName = osName;
                    }
                }
                else
                {
                    switch( sExtraInfo.type )
                    {
                        case JXL_CHANNEL_ALPHA: eInterp = GCI_AlphaBand; break;
                        case JXL_CHANNEL_DEPTH: osBandName = "Depth channel"; break;
                        case JXL_CHANNEL_SPOT_COLOR: osBandName = "Spot color channel"; break;
                        case JXL_CHANNEL_SELECTION_MASK: osBandName = "Selection mask channel"; break;
                        case JXL_CHANNEL_BLACK: osBandName = "Black channel"; break;
                        case JXL_CHANNEL_CFA: osBandName = "CFA channel"; break;
                        case JXL_CHANNEL_THERMAL: osBandName = "Thermal channel"; break;
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
                }
            }
        }

        auto poBand = new JPEGXLRasterBand(
            this, i, eDT, static_cast<int>(info.bits_per_sample), eInterp);
        SetBand(i, poBand);
        if( !osBandName.empty() )
            poBand->SetDescription(osBandName.c_str());
    }

    if( l_nBands > 1 )
    {
        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }

    // Initialize any PAM information.
    SetDescription(poOpenInfo->pszFilename);
    TryLoadXML( poOpenInfo->GetSiblingFiles() );
    oOvManager.Initialize(this, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

    nPamFlags &= ~GPF_DIRTY;

    return true;
}

/************************************************************************/
/*                        GetDecodedImage()                             */
/************************************************************************/

const std::vector<GByte>& JPEGXLDataset::GetDecodedImage()
{
    if( m_bDecodingFailed || !m_abyImage.empty() )
        return m_abyImage;

    const auto eDT = GetRasterBand(1)->GetRasterDataType();
    const auto nDataSize = GDALGetDataTypeSizeBytes(eDT);
    const int nNonExtraBands = nBands - m_nNonAlphaExtraChannels;
    if( static_cast<size_t>(nRasterXSize) >
            std::numeric_limits<size_t>::max() / nRasterYSize / nDataSize / nNonExtraBands )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Image too big for architecture");
        m_bDecodingFailed = true;
        return m_abyImage;
    }

    try
    {
        m_abyImage.resize(static_cast<size_t>(nRasterXSize) * nRasterYSize * nNonExtraBands * nDataSize);
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate image buffer: %s", e.what());
        m_bDecodingFailed = true;
        return m_abyImage;
    }

    m_abyExtraChannels.resize( m_nNonAlphaExtraChannels );
    for( int i = 0; i < m_nNonAlphaExtraChannels; ++i )
    {
        try
        {
            m_abyExtraChannels[i].resize(static_cast<size_t>(nRasterXSize) * nRasterYSize * nDataSize);
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate image buffer: %s", e.what());
            m_bDecodingFailed = true;
            return m_abyImage;
        }
    }

    GetDecodedImage(m_abyImage.data(), m_abyImage.size());

    if( m_bDecodingFailed )
        m_abyImage.clear();

    return m_abyImage;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **JPEGXLDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:XMP", "EXIF", nullptr);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char** JPEGXLDataset::GetMetadata(const char* pszDomain)
{
#ifdef HAVE_JXL_BOX_API
    if( pszDomain != nullptr && EQUAL(pszDomain, "xml:XMP") && !m_osXMP.empty() )
    {
        m_apszXMP[0] = &m_osXMP[0];
        return m_apszXMP;
    }

    if( pszDomain != nullptr && EQUAL(pszDomain, "EXIF") && !m_aosEXIFMetadata.empty() )
    {
        return m_aosEXIFMetadata.List();
    }
#endif
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char* JPEGXLDataset::GetMetadataItem(const char* pszName, const char* pszDomain)
{
#ifdef HAVE_JXL_BOX_API
    if( pszDomain != nullptr && EQUAL(pszDomain, "EXIF") && !m_aosEXIFMetadata.empty() )
    {
        return m_aosEXIFMetadata.FetchNameValue(pszName);
    }
    else if( pszDomain != nullptr && EQUAL(pszDomain, "_DEBUG_") )
    {
        if( EQUAL(pszName, "HAS_JPEG_RECONSTRUCTION_DATA") )
            return m_bHasJPEGReconstructionData ? "YES" : "NO";
    }
#endif

    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                        GetDecodedImage()                             */
/************************************************************************/

void JPEGXLDataset::GetDecodedImage(void* pabyOuputData, size_t nOutputDataSize)
{
    JxlDecoderRewind(m_decoder.get());
    VSIFSeekL(m_fp, 0, SEEK_SET);

    if( JxlDecoderSubscribeEvents(m_decoder.get(), JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlDecoderSubscribeEvents() failed");
        return;
    }

    const auto eDT = GetRasterBand(1)->GetRasterDataType();
    while(true)
    {
        const JxlDecoderStatus status = JxlDecoderProcessInput(m_decoder.get());
        if( status == JXL_DEC_ERROR )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Decoding error");
            m_bDecodingFailed = true;
            break;
        }
        else if( status == JXL_DEC_NEED_MORE_INPUT )
        {
            JxlDecoderReleaseInput(m_decoder.get());

            const size_t nRead = VSIFReadL(m_abyInputData.data(), 1, m_abyInputData.size(), m_fp);
            if( nRead == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Decoder expected more input, but no more available");
                m_bDecodingFailed = true;
                break;
            }
            if( JxlDecoderSetInput(m_decoder.get(), m_abyInputData.data(), nRead) !=
                    JXL_DEC_SUCCESS )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "JxlDecoderSetInput() failed");
                m_bDecodingFailed = true;
                break;
            }
        }
        else if( status == JXL_DEC_SUCCESS )
        {
            break;
        }
        else if( status == JXL_DEC_FULL_IMAGE )
        {
            // ok
        }
        else if( status == JXL_DEC_NEED_IMAGE_OUT_BUFFER )
        {
            JxlPixelFormat format = {static_cast<uint32_t>(nBands - m_nNonAlphaExtraChannels),
                                     eDT == GDT_Byte ?   JXL_TYPE_UINT8:
                                     eDT == GDT_UInt16 ? JXL_TYPE_UINT16:
                                                         JXL_TYPE_FLOAT,
                                     JXL_NATIVE_ENDIAN,
                                     0 /* alignment */
            };

            size_t buffer_size;
            if( JxlDecoderImageOutBufferSize(m_decoder.get(), &format, &buffer_size)
                    != JXL_DEC_SUCCESS  )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderImageOutBufferSize failed()");
                m_bDecodingFailed = true;
                break;
            }
            if( buffer_size != nOutputDataSize )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderImageOutBufferSize returned an unexpected buffer_size");
                m_bDecodingFailed = true;
                break;
            }

            // It could be interesting to use JxlDecoderSetImageOutCallback()
            // to do progressive decoding, but at the time of writing, libjxl
            // seems to just call the callback when all the image is decompressed
            if( JxlDecoderSetImageOutBuffer(
                    m_decoder.get(), &format, pabyOuputData, nOutputDataSize)
                    != JXL_DEC_SUCCESS )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlDecoderSetImageOutBuffer failed()");
                m_bDecodingFailed = true;
                break;
            }

            format.num_channels = 1;
            for( int i = 0; i < m_nNonAlphaExtraChannels; ++i )
            {
                if( JxlDecoderExtraChannelBufferSize(m_decoder.get(), &format, &buffer_size, i)
                        != JXL_DEC_SUCCESS  )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "JxlDecoderExtraChannelBufferSize failed()");
                    m_bDecodingFailed = true;
                    break;
                }
                if( buffer_size != m_abyExtraChannels[i].size() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "JxlDecoderExtraChannelBufferSize returned an unexpected buffer_size");
                    m_bDecodingFailed = true;
                    break;
                }
                if( JxlDecoderSetExtraChannelBuffer(
                        m_decoder.get(), &format, m_abyExtraChannels[i].data(), m_abyExtraChannels[i].size(), i)
                        != JXL_DEC_SUCCESS )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "JxlDecoderSetExtraChannelBuffer failed()");
                    m_bDecodingFailed = true;
                    break;
                }
            }
            if( m_bDecodingFailed )
            {
                break;
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unexpected decoder state: %d",
                     status);
        }
    }

    // Rescale from 8-bits/16-bits
    if( m_nBits < GDALGetDataTypeSize(eDT) )
    {
        const auto Rescale = [this, eDT](void* pBuffer, int nChannels)
        {
            const size_t nSamples = static_cast<size_t>(nRasterXSize) * nRasterYSize * nChannels;
            const int nMaxVal = (1 << m_nBits) - 1;
            if( eDT == GDT_Byte )
            {
                const int nHalfMaxWidth = 127;
                GByte* panData = static_cast<GByte*>(pBuffer);
                for( size_t i = 0; i < nSamples; ++i )
                {
                    panData[i] = static_cast<GByte>((panData[i] * nMaxVal + nHalfMaxWidth) / 255);
                }
            }
            else if( eDT == GDT_UInt16 )
            {
                const int nHalfMaxWidth = 32767;
                uint16_t* panData = static_cast<uint16_t*>(pBuffer);
                for( size_t i = 0; i < nSamples; ++i )
                {
                    panData[i] = static_cast<uint16_t>((panData[i] * nMaxVal + nHalfMaxWidth) / 65535);
                }
            }
        };

        Rescale(pabyOuputData, nBands - m_nNonAlphaExtraChannels);
        for( int i = 0; i < m_nNonAlphaExtraChannels; ++i )
        {
            Rescale(m_abyExtraChannels[i].data(), 1);
        }
    }

}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JPEGXLDataset::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff,
                                    int nXSize, int nYSize,
                                    void *pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    int nBandCount, int *panBandMap,
                                    GSpacing nPixelSpace, GSpacing nLineSpace,
                                    GSpacing nBandSpace,
                                    GDALRasterIOExtraArg *psExtraArg )

{
    const auto AreSequentialBands = [](const int* panItems, int nItems)
    {
        for( int i = 0; i < nItems; i++ )
        {
            if( panItems[i] != i + 1 )
                return false;
        }
        return true;
    };

    if( eRWFlag == GF_Read &&
        nXOff == 0 && nYOff == 0 &&
        nXSize == nRasterXSize && nYSize == nRasterYSize &&
        nBufXSize == nXSize && nBufYSize == nYSize )
    {
        // Get the full image in a pixel-interleaved way
        if( m_bDecodingFailed )
            return CE_Failure;

        CPLDebug("JPEGXL", "Using optimized IRasterIO() code path");

        const auto nBufTypeSize = GDALGetDataTypeSizeBytes(eBufType);
        const bool bIsPixelInterleaveBuffer =
            ((nBandSpace == 0 && nBandCount == 1) || nBandSpace == nBufTypeSize) &&
            nPixelSpace == nBufTypeSize * nBandCount &&
            nLineSpace == nPixelSpace * nRasterXSize;

        const auto eNativeDT = GetRasterBand(1)->GetRasterDataType();
        const auto nNativeDataSize = GDALGetDataTypeSizeBytes(eNativeDT);
        const bool bIsBandSequential = AreSequentialBands(panBandMap, nBandCount);
        if( eBufType == eNativeDT && bIsBandSequential && nBandCount == nBands &&
            m_nNonAlphaExtraChannels == 0 && bIsPixelInterleaveBuffer )
        {
            // We can directly use the user output buffer
            GetDecodedImage(pData,
                static_cast<size_t>(nRasterXSize) * nRasterYSize * nBands * nNativeDataSize);
            return m_bDecodingFailed ? CE_Failure : CE_None;
        }

        const auto& abyDecodedImage = GetDecodedImage();
        if( abyDecodedImage.empty() )
        {
            return CE_Failure;
        }
        const int nNonExtraBands = nBands - m_nNonAlphaExtraChannels;
        if( bIsPixelInterleaveBuffer && bIsBandSequential &&
            nBandCount == nNonExtraBands )
        {
            GDALCopyWords64(abyDecodedImage.data(), eNativeDT, nNativeDataSize,
                            pData, eBufType, nBufTypeSize,
                            static_cast<GPtrDiff_t>(nRasterXSize) * nRasterYSize * nBandCount);
        }
        else
        {
            for( int iBand = 0; iBand < nBandCount; iBand ++ )
            {
                const int iSrcBand = panBandMap[iBand] - 1;
                if( iSrcBand < nNonExtraBands )
                {
                    for( int iY = 0; iY < nRasterYSize; iY++ )
                    {
                        const GByte* pSrc = abyDecodedImage.data() +
                            (static_cast<size_t>(iY) * nRasterXSize * nNonExtraBands +
                             iSrcBand) * nNativeDataSize;
                        GByte* pDst = static_cast<GByte*>(pData) +
                            iY * nLineSpace + iBand * nBandSpace;
                        GDALCopyWords(pSrc, eNativeDT, nNativeDataSize * nNonExtraBands,
                                      pDst, eBufType, static_cast<int>(nPixelSpace),
                                      nRasterXSize);
                    }
                }
                else
                {
                    for( int iY = 0; iY < nRasterYSize; iY++ )
                    {
                        const GByte* pSrc = m_abyExtraChannels[iSrcBand - nNonExtraBands].data() +
                            static_cast<size_t>(iY) * nRasterXSize * nNativeDataSize;
                        GByte* pDst = static_cast<GByte*>(pData) +
                            iY * nLineSpace + iBand * nBandSpace;
                        GDALCopyWords(pSrc, eNativeDT, nNativeDataSize,
                                      pDst, eBufType, static_cast<int>(nPixelSpace),
                                      nRasterXSize);
                    }
                }
            }
        }
        return CE_None;
    }

    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nBandCount, panBandMap,
                                     nPixelSpace, nLineSpace, nBandSpace,
                                     psExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr JPEGXLRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff,
                                    int nXSize, int nYSize,
                                    void *pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    GSpacing nPixelSpace, GSpacing nLineSpace,
                                    GDALRasterIOExtraArg *psExtraArg )

{
    if( eRWFlag == GF_Read &&
        nXOff == 0 && nYOff == 0 &&
        nXSize == nRasterXSize && nYSize == nRasterYSize &&
        nBufXSize == nXSize && nBufYSize == nYSize )
    {
        return cpl::down_cast<JPEGXLDataset*>(poDS)->IRasterIO(
            GF_Read, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            1, &nBand,
            nPixelSpace, nLineSpace, 0,
            psExtraArg);
    }

    return GDALPamRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace,
                                        psExtraArg);
}

/************************************************************************/
/*                          OpenStaticPAM()                             */
/************************************************************************/

GDALPamDataset* JPEGXLDataset::OpenStaticPAM(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) )
        return nullptr;

    auto poDS = cpl::make_unique<JPEGXLDataset>();
    if( !poDS->Open(poOpenInfo) )
        return nullptr;

    return poDS.release();
}

/************************************************************************/
/*                          OpenStatic()                                */
/************************************************************************/

GDALDataset* JPEGXLDataset::OpenStatic(GDALOpenInfo* poOpenInfo)
{
    return OpenStaticPAM(poOpenInfo);
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *
JPEGXLDataset::CreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                           int /*bStrict*/, char ** papszOptions,
                           GDALProgressFunc pfnProgress, void *pProgressData )

{
    if( poSrcDS->GetRasterXSize() <= 0 || poSrcDS->GetRasterYSize() <= 0 ||
        poSrcDS->GetRasterCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid source dataset");
        return nullptr;
    }

    JxlPixelFormat format = {0, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    const auto eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    switch( eDT )
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

    auto encoder = JxlEncoderMake(nullptr);
    if( !encoder )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlEncoderMake() failed");
        return nullptr;
    }

    const char* pszNBits = CSLFetchNameValue(papszOptions, "NBITS");
    if( pszNBits == nullptr )
        pszNBits = poSrcDS->GetRasterBand(1)->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
    const int nBits = ((eDT == GDT_Byte || eDT == GDT_UInt16) && pszNBits != nullptr) ?
        atoi(pszNBits) : GDALGetDataTypeSize(eDT);

    JxlBasicInfo basic_info;
    JxlEncoderInitBasicInfo(&basic_info);
    basic_info.xsize = poSrcDS->GetRasterXSize();
    basic_info.ysize = poSrcDS->GetRasterYSize();
    basic_info.bits_per_sample = nBits;
    basic_info.orientation = JXL_ORIENT_IDENTITY;
    if ( format.data_type == JXL_TYPE_FLOAT )
    {
        basic_info.exponent_bits_per_sample=8;
    }

    const int nSrcBands = poSrcDS->GetRasterCount();

    bool bHasInterleavedAlphaBand = false;
    if( nSrcBands == 1 )
    {
        basic_info.num_color_channels = 1;
    }
    else if( nSrcBands == 2 )
    {
        basic_info.num_color_channels = 1;
        basic_info.num_extra_channels = 1;
        if( poSrcDS->GetRasterBand(2)->GetColorInterpretation() == GCI_AlphaBand )
        {
            bHasInterleavedAlphaBand = true;
            basic_info.alpha_bits = basic_info.bits_per_sample;
            basic_info.alpha_exponent_bits = basic_info.exponent_bits_per_sample;
        }
    }
    else /* if( nSrcBands >= 3 ) */
    {
        if( poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_RedBand &&
            poSrcDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
            poSrcDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand )
        {
            basic_info.num_color_channels = 3;
            basic_info.num_extra_channels = nSrcBands - 3;
            if( nSrcBands >= 4 &&
                poSrcDS->GetRasterBand(4)->GetColorInterpretation() == GCI_AlphaBand )
            {
                bHasInterleavedAlphaBand = true;
                basic_info.alpha_bits = basic_info.bits_per_sample;
                basic_info.alpha_exponent_bits = basic_info.exponent_bits_per_sample;
            }
        }
        else
        {
            basic_info.num_color_channels = 1;
            basic_info.num_extra_channels = nSrcBands - 1;
        }
    }

    const int nBaseChannels = static_cast<int>(basic_info.num_color_channels +
                                               (bHasInterleavedAlphaBand ? 1 : 0));
    format.num_channels = nBaseChannels;

#ifndef HAVE_JxlEncoderInitExtraChannelInfo
    if( basic_info.num_extra_channels != (bHasInterleavedAlphaBand ? 1 : 0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This version of libjxl does not support "
                 "creating non-alpha extra channels.");
        return nullptr;
    }
#endif

#ifdef HAVE_JXL_THREADS
    auto parallelRunner = JxlResizableParallelRunnerMake(nullptr);
    if( !parallelRunner )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlResizableParallelRunnerMake() failed");
        return nullptr;
    }

    const char* pszNumThreads = CSLFetchNameValue( papszOptions, "NUM_THREADS" );
    if( pszNumThreads == nullptr )
        pszNumThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
    uint32_t nMaxThreads =
        static_cast<uint32_t>(EQUAL(pszNumThreads, "ALL_CPUS") ? CPLGetNumCPUs() : atoi(pszNumThreads));
    if( nMaxThreads > 1024 )
        nMaxThreads = 1024; // to please Coverity

    const uint32_t nThreads = std::min(
        nMaxThreads, JxlResizableParallelRunnerSuggestThreads(
                                     basic_info.xsize, basic_info.ysize));
    CPLDebug("JPEGXL", "Using %u threads", nThreads);
    JxlResizableParallelRunnerSetThreads(parallelRunner.get(), nThreads);

    if( JxlEncoderSetParallelRunner(encoder.get(), JxlResizableParallelRunner, parallelRunner.get()) !=
            JXL_ENC_SUCCESS )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlEncoderSetParallelRunner() failed");
        return nullptr;
    }
#endif

#ifdef HAVE_JxlEncoderFrameSettingsCreate
    JxlEncoderOptions *opts = JxlEncoderFrameSettingsCreate(encoder.get(), nullptr);
#else
    JxlEncoderOptions *opts = JxlEncoderOptionsCreate(encoder.get(), nullptr);
#endif
    if( opts == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "JxlEncoderFrameSettingsCreate() failed");
        return nullptr;
    }

    const char* pszLossLess = CSLFetchNameValue(papszOptions, "LOSSLESS");
    const char* pszDistance = CSLFetchNameValue(papszOptions, "DISTANCE");
    const char* pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");

    const bool bLossless =
        (pszLossLess == nullptr && pszDistance == nullptr && pszQuality == nullptr) ||
        (pszLossLess != nullptr && CPLTestBool(pszLossLess));
    if( pszLossLess == nullptr && (pszDistance != nullptr || pszQuality != nullptr) )
    {
        CPLDebug("JPEGXL", "Using lossy mode");
    }
    if( (pszLossLess != nullptr && bLossless) && pszDistance != nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DISTANCE and LOSSLESS=YES are mutually exclusive");
        return nullptr;
    }
    if( (pszLossLess != nullptr && bLossless) && pszQuality != nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "QUALITY and LOSSLESS=YES are mutually exclusive");
        return nullptr;
    }
    if( pszDistance != nullptr && pszQuality != nullptr )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "QUALITY and DISTANCE are mutually exclusive");
        return nullptr;
    }

#ifdef HAVE_JxlEncoderSetCodestreamLevel
    if( poSrcDS->GetRasterXSize() > 262144 ||
        poSrcDS->GetRasterYSize() > 262144 ||
        poSrcDS->GetRasterXSize() > 268435456 / poSrcDS->GetRasterYSize() )
    {
        JxlEncoderSetCodestreamLevel(encoder.get(), 10);
    }
#endif

    if( bLossless )
    {
#ifdef HAVE_JxlEncoderSetCodestreamLevel
        if( nBits > 12 )
        {
            JxlEncoderSetCodestreamLevel(encoder.get(), 10);
        }
#endif

        JxlEncoderOptionsSetLossless(opts, TRUE);
        basic_info.uses_original_profile = JXL_TRUE;
    }
    else
    {
        float fDistance = pszDistance ? static_cast<float>(CPLAtof(pszDistance)) : 1.0f;
        if( pszQuality != nullptr )
        {
            const double quality = CPLAtof(pszQuality);
            // Quality settings roughly match libjpeg qualities.
            // Formulas taken from cjxl.cc
            if( quality >= 100 )
            {
              fDistance = 0;
            }
            else if( quality >= 30 )
            {
              fDistance = static_cast<float>(0.1 + (100 - quality) * 0.09);
            }
            else
            {
              fDistance = static_cast<float>(
                  6.4 + pow(2.5, (30 - quality) / 5.0f) / 6.25f);
            }
        }
        if( fDistance >= 0.0f && fDistance < 0.1f )
            fDistance = 0.1f;

#ifdef HAVE_JxlEncoderSetFrameDistance
        if( JxlEncoderSetFrameDistance(opts, fDistance) != JXL_ENC_SUCCESS )
#else
        if( JxlEncoderOptionsSetDistance(opts, fDistance) != JXL_ENC_SUCCESS )
#endif
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderSetFrameDistance() failed");
            return nullptr;
        }
    }

    const int nEffort = atoi(CSLFetchNameValueDef(papszOptions, "EFFORT", "5"));
#ifdef HAVE_JxlEncoderFrameSettingsSetOption
    if( JxlEncoderFrameSettingsSetOption(opts, JXL_ENC_FRAME_SETTING_EFFORT, nEffort) != JXL_ENC_SUCCESS )
#else
    if( JxlEncoderOptionsSetEffort(opts, nEffort) != JXL_ENC_SUCCESS )
#endif
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlEncoderFrameSettingsSetOption() failed");
        return nullptr;
    }

    // If the source dataset is a JPEG file, try to losslessly add it
    auto poSrcDriver = poSrcDS->GetDriver();
    std::vector<GByte> abyJPEG;
    const char* pszSourceColorSpace =
        poSrcDS->GetMetadataItem("SOURCE_COLOR_SPACE", "IMAGE_STRUCTURE");
    if( poSrcDriver && EQUAL(poSrcDriver->GetDescription(), "JPEG") &&
        !(pszSourceColorSpace && EQUAL(pszSourceColorSpace, "CMYK")) && // lossless transcoding from CMYK not supported
        bLossless )
    {
        auto fpJPEG = std::unique_ptr<VSILFILE, VSILFileReleaser>(
            VSIFOpenL(poSrcDS->GetDescription(), "rb"));
        if( fpJPEG )
        {
            VSIFSeekL(fpJPEG.get(), 0, SEEK_END);
            const auto nFileSize = VSIFTellL(fpJPEG.get());
            if( nFileSize > 2 &&
                nFileSize < std::numeric_limits<size_t>::max() / 2 )
            {
                try
                {
                    abyJPEG.resize(static_cast<size_t>(nFileSize));
                    VSIFSeekL(fpJPEG.get(), 0, SEEK_SET);
                    if( VSIFReadL(&abyJPEG[0], 1, abyJPEG.size(),
                                  fpJPEG.get()) == abyJPEG.size() &&
                        abyJPEG[0] == 0xff && abyJPEG[1] == 0xd8 )
                    {
                        std::vector<GByte> abyJPEGMod;
                        abyJPEGMod.reserve(abyJPEG.size());

                        // Append Start Of Image marker (0xff 0xd8)
                        abyJPEGMod.insert(abyJPEGMod.end(),
                                          abyJPEG.begin(),
                                          abyJPEG.begin() + 2);

                        // Rework JPEG data to remove APP (except APP0) and COM markers
                        // as it confuses libjxl, when trying to reconstruct
                        // a JPEG file
                        size_t i = 2;
                        while(i + 1 < abyJPEG.size())
                        {
                            if( abyJPEG[i] != 0xFF )
                            {
                                // Not a valid tag (shouldn't happen)
                                abyJPEGMod.clear();
                                break;
                            }

                            // Stop when encountering a marker that is not a APP or COM marker
                            const bool bIsCOM = abyJPEG[i+1] == 0xFE;
                            if( (abyJPEG[i+1] & 0xF0) != 0xE0 && !bIsCOM )
                            {
                                // Append all markers until end
                                abyJPEGMod.insert(abyJPEGMod.end(),
                                                  abyJPEG.begin() + i,
                                                  abyJPEG.end());
                                break;
                            }
                            const bool bIsAPP0 = abyJPEG[i+1] == 0xE0;

                            // Skip marker ID
                            i += 2;
                            // Check we can read chunk length
                            if( i + 1 >= abyJPEG.size() )
                            {
                                // Truncated JPEG file
                                abyJPEGMod.clear();
                                break;
                            }
                            const int nChunkLength = abyJPEG[i] * 256 + abyJPEG[i+1];
                            if( (bIsCOM || bIsAPP0) && i + nChunkLength <= abyJPEG.size() )
                            {
                                // Append COM or APP0 marker
                                abyJPEGMod.insert(abyJPEGMod.end(),
                                                  abyJPEG.begin() + i - 2,
                                                  abyJPEG.begin() + i + nChunkLength);
                            }
                            i += nChunkLength;
                        }
                        abyJPEG = std::move(abyJPEGMod);
                    }
                }
                catch( const std::exception& )
                {
                }
            }
        }
    }

    const char* pszICCProfile = CSLFetchNameValue(papszOptions, "SOURCE_ICC_PROFILE");
    if( pszICCProfile == nullptr )
    {
        pszICCProfile = poSrcDS->GetMetadataItem(
            "SOURCE_ICC_PROFILE", "COLOR_PROFILE");
    }
    if( pszICCProfile && pszICCProfile[0] != '\0' )
    {
        basic_info.uses_original_profile = JXL_TRUE;
    }

    if( abyJPEG.empty() )
    {
        if( JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(encoder.get(), &basic_info) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderSetBasicInfo() failed");
            return nullptr;
        }

        if( pszICCProfile && pszICCProfile[0] != '\0' )
        {
            char *pEmbedBuffer = CPLStrdup(pszICCProfile);
            GInt32 nEmbedLen = CPLBase64DecodeInPlace(reinterpret_cast<GByte *>(pEmbedBuffer));
            if( JXL_ENC_SUCCESS != JxlEncoderSetICCProfile(
                                            encoder.get(),
                                            reinterpret_cast<GByte *>(pEmbedBuffer),
                                            nEmbedLen) )
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
                basic_info.num_color_channels == 1 /*is_gray*/);
            if( JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding(encoder.get(), &color_encoding) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetColorEncoding() failed");
                return nullptr;
            }
        }
    }

#ifdef HAVE_JxlEncoderInitExtraChannelInfo
    if( abyJPEG.empty() &&
        basic_info.num_extra_channels > 0 && basic_info.alpha_bits == 0 )
    {
        if( basic_info.num_extra_channels >= 5 )
            JxlEncoderSetCodestreamLevel(encoder.get(), 10);

        JxlExtraChannelInfo extra_channel_info;
        JxlEncoderInitExtraChannelInfo(JXL_CHANNEL_OPTIONAL,
                                       &extra_channel_info);
        extra_channel_info.bits_per_sample = basic_info.bits_per_sample;
        extra_channel_info.exponent_bits_per_sample = basic_info.exponent_bits_per_sample;
        for( int i = (bHasInterleavedAlphaBand ? 1 : 0);
                    i < static_cast<int>(basic_info.num_extra_channels); ++i )
        {
            const uint32_t nIndex = static_cast<uint32_t>(i);
            if( JXL_ENC_SUCCESS != JxlEncoderSetExtraChannelInfo(
                    encoder.get(), nIndex, &extra_channel_info) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetExtraChannelInfo() failed");
                return nullptr;
            }
            const int nBand = static_cast<int>(1 + basic_info.num_color_channels + i);
            std::string osChannelName(CPLSPrintf("Band %d", nBand));
            const char* pszDescription = poSrcDS->GetRasterBand(nBand)->GetDescription();
            if( pszDescription && pszDescription[0] != '\0' )
                osChannelName = pszDescription;
            if( JXL_ENC_SUCCESS != JxlEncoderSetExtraChannelName(
                    encoder.get(), nIndex,
                    osChannelName.data(), osChannelName.size()) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderSetExtraChannelName() failed");
                return nullptr;
            }
        }
    }
#endif

#ifdef HAVE_JXL_BOX_API
    const bool bCompressBox =
        CPLFetchBool(papszOptions, "COMPRESS_BOXES", false);

    // Write "xml " box with xml:XMP metadata
    const bool bWriteXMP =
        CPLFetchBool(papszOptions, "WRITE_XMP", true);
    char** papszXMP = poSrcDS->GetMetadata("xml:XMP");
    if( papszXMP && bWriteXMP )
    {
        JxlEncoderUseBoxes(encoder.get());

        const char* pszXMP = papszXMP[0];
        if( JxlEncoderAddBox(encoder.get(), "xml ",
                             reinterpret_cast<const uint8_t*>(pszXMP),
                             strlen(pszXMP),
                             bCompressBox) != JXL_ENC_SUCCESS )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderAddBox() failed");
            return nullptr;
        }
    }

    // Write "Exif" box with EXIF metadata
    // Look for EXIF metadata first in the EXIF metadata domain, and fallback
    // to main domain.
    const bool bWriteExifMetadata =
            CPLFetchBool(papszOptions, "WRITE_EXIF_METADATA", true);
    char** papszEXIF = poSrcDS->GetMetadata("EXIF");
    bool bEXIFFromMainDomain = false;
    if( papszEXIF == nullptr && bWriteExifMetadata )
    {
        char** papszMetadata = poSrcDS->GetMetadata();
        for( CSLConstList papszIter = papszMetadata; papszIter && *papszIter; ++papszIter )
        {
            if( STARTS_WITH(*papszIter, "EXIF_") )
            {
                papszEXIF = papszMetadata;
                bEXIFFromMainDomain = true;
                break;
            }
        }
    }
    if( papszEXIF && bWriteExifMetadata )
    {
        GUInt32 nMarkerSize = 0;
        GByte* pabyEXIF =
            EXIFCreate(papszEXIF,
                       nullptr, 0, 0, 0, // overview
                       &nMarkerSize);
        CPLAssert(nMarkerSize > 6 && memcmp(pabyEXIF, "Exif\0\0", 6) == 0);
        // Add 4 leading bytes at 0
        std::vector<GByte> abyEXIF(4 + nMarkerSize - 6);
        memcpy(&abyEXIF[4], pabyEXIF + 6, nMarkerSize - 6);
        CPLFree(pabyEXIF);

        JxlEncoderUseBoxes(encoder.get());
        if( JxlEncoderAddBox(encoder.get(), "Exif",
                             abyEXIF.data(), abyEXIF.size(),
                             bCompressBox) != JXL_ENC_SUCCESS )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderAddBox() failed");
            return nullptr;
        }
    }

    // Write GeoJP2 box in a JUMBF box from georeferencing information
    const bool bWriteGeoJP2 =
        CPLFetchBool(papszOptions, "WRITE_GEOJP2", true);
    double adfGeoTransform[6];
    const bool bHasGeoTransform = poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None;
    const OGRSpatialReference* poSRS = poSrcDS->GetSpatialRef();
    const int nGCPCount = poSrcDS->GetGCPCount();
    char** papszRPCMD = poSrcDS->GetMetadata("RPC");
    if( bWriteGeoJP2 && (poSRS != nullptr || bHasGeoTransform || nGCPCount || papszRPCMD) )
    {
        GDALJP2Metadata oJP2Metadata;
        if( poSRS )
            oJP2Metadata.SetSpatialRef(poSRS);
        if( bHasGeoTransform )
            oJP2Metadata.SetGeoTransform(adfGeoTransform);
        if( nGCPCount )
        {
            const OGRSpatialReference* poSRSGCP = poSrcDS->GetGCPSpatialRef();
            if( poSRSGCP )
                oJP2Metadata.SetSpatialRef(poSRSGCP);
            oJP2Metadata.SetGCPs(nGCPCount, poSrcDS->GetGCPs());
        }
        if( papszRPCMD )
            oJP2Metadata.SetRPCMD(papszRPCMD);

        const char* pszAreaOfPoint = poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        oJP2Metadata.bPixelIsPoint =
            pszAreaOfPoint && EQUAL(pszAreaOfPoint, GDALMD_AOP_POINT);

        auto poJP2GeoTIFF = std::unique_ptr<GDALJP2Box>(oJP2Metadata.CreateJP2GeoTIFF());
        if( poJP2GeoTIFF )
        {
            // Per JUMBF spec: UUID Content Type. The JUMBF box contains exactly one UUID box
            const GByte abyUUIDTypeUUID[16] = {
                0x75, 0x75, 0x69, 0x64,
                0x00, 0x11, 0x00, 0x10,
                0x80, 0x00, 0x00, 0xAA,
                0x00, 0x38, 0x9B, 0x71 };
            auto poJUMBFDescrBox = std::unique_ptr<GDALJP2Box>(
                GDALJP2Box::CreateJUMBFDescriptionBox(
                    abyUUIDTypeUUID, "GeoJP2 box"));
            const GDALJP2Box* poJP2GeoTIFFConst = poJP2GeoTIFF.get();
            auto poJUMBFBox = std::unique_ptr<GDALJP2Box>(
                GDALJP2Box::CreateJUMBFBox(poJUMBFDescrBox.get(),
                                           1,
                                           &poJP2GeoTIFFConst));

            JxlEncoderUseBoxes(encoder.get());

            const GByte* pabyBoxData = poJUMBFBox->GetWritableBoxData();
            if( JxlEncoderAddBox(encoder.get(), "jumb",
                                 pabyBoxData, static_cast<size_t>(poJUMBFBox->GetBoxLength()),
                                 bCompressBox) != JXL_ENC_SUCCESS )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "JxlEncoderAddBox() failed");
                return nullptr;
            }
        }
    }
#endif

    auto fp = std::unique_ptr<VSILFILE, VSILFileReleaser>(VSIFOpenL(pszFilename, "wb"));
    if( !fp )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot create %s: %s", pszFilename,
                 VSIStrerror(errno));
        return nullptr;
    }

    if( !abyJPEG.empty() )
    {
        CPLDebug("JPEGXL", "Adding JPEG frame");
        JxlEncoderStoreJPEGMetadata(encoder.get(), true);
        if( JxlEncoderAddJPEGFrame(opts, abyJPEG.data(),
                                    abyJPEG.size()) != JXL_ENC_SUCCESS )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderAddJPEGFrame() failed");
            return nullptr;
        }
    }
    else
    {
        const auto nDataSize = GDALGetDataTypeSizeBytes(eDT);

        if( static_cast<size_t>(poSrcDS->GetRasterXSize()) >
                std::numeric_limits<size_t>::max() / poSrcDS->GetRasterYSize()
                    / nBaseChannels / nDataSize )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Image too big for architecture");
            return nullptr;
        }
        const size_t nInputDataSize = static_cast<size_t>(poSrcDS->GetRasterXSize()) *
                                      poSrcDS->GetRasterYSize() *
                                      nBaseChannels *
                                      nDataSize;

        std::vector<GByte> abyInputData;
        try
        {
            abyInputData.resize(nInputDataSize);
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate image buffer: %s", e.what());
            return nullptr;
        }

        if( poSrcDS->RasterIO(
                GF_Read, 0, 0, poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                abyInputData.data(), poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                eDT,
                nBaseChannels, nullptr,
                nDataSize * nBaseChannels,
                nDataSize * nBaseChannels * poSrcDS->GetRasterXSize(),
                nDataSize,
                nullptr) != CE_None )
        {
            return nullptr;
        }

        const auto Rescale = [eDT, nBits, poSrcDS](void* pBuffer, int nChannels)
        {
            // Rescale to 8-bits/16-bits
            if( nBits < GDALGetDataTypeSize(eDT) )
            {
                const size_t nSamples = static_cast<size_t>(poSrcDS->GetRasterXSize()) *
                    poSrcDS->GetRasterYSize() * nChannels;
                const int nMaxVal = (1 << nBits) - 1;
                const int nMavValHalf = nMaxVal / 2;
                if( eDT == GDT_Byte )
                {
                    uint8_t* panData = static_cast<uint8_t*>(pBuffer);
                    for( size_t i = 0; i < nSamples; ++i )
                    {
                        panData[i] = static_cast<GByte>(
                            (std::min(static_cast<int>(panData[i]), nMaxVal) * 255 + nMavValHalf) / nMaxVal);
                    }
                }
                else if( eDT == GDT_UInt16 )
                {
                    uint16_t* panData = static_cast<uint16_t*>(pBuffer);
                    for( size_t i = 0; i < nSamples; ++i )
                    {
                        panData[i] = static_cast<uint16_t>(
                            (std::min(static_cast<int>(panData[i]), nMaxVal) * 65535 + nMavValHalf) / nMaxVal);
                    }
                }
            }
        };

        Rescale(abyInputData.data(), nBaseChannels);

        if( JxlEncoderAddImageFrame(opts, &format, abyInputData.data(),
                                    abyInputData.size()) != JXL_ENC_SUCCESS )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JxlEncoderAddImageFrame() failed");
            return nullptr;
        }

#ifdef HAVE_JxlEncoderInitExtraChannelInfo
        format.num_channels = 1;
        for(int i = nBaseChannels; i < poSrcDS->GetRasterCount(); ++i )
        {
            if( poSrcDS->GetRasterBand(i+1)->RasterIO(
                GF_Read, 0, 0, poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                abyInputData.data(), poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                eDT, 0, 0, nullptr) != CE_None )
            {
                return nullptr;
            }

            Rescale(abyInputData.data(), 1);

            if( JxlEncoderSetExtraChannelBuffer(
                    opts, &format, abyInputData.data(),
                    static_cast<size_t>(poSrcDS->GetRasterXSize()) *
                      poSrcDS->GetRasterYSize() *
                      nDataSize,
                    i - nBaseChannels + (bHasInterleavedAlphaBand ? 1 : 0)) != JXL_ENC_SUCCESS )
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
    while(true)
    {
        size_t len = abyOutputBuffer.size();
        uint8_t* buf = abyOutputBuffer.data();
        JxlEncoderStatus process_result = JxlEncoderProcessOutput(
            encoder.get(), &buf, &len);
        if( process_result == JXL_ENC_ERROR )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                 "JxlEncoderProcessOutput() failed");
            return nullptr;
        }
        size_t nToWrite = abyOutputBuffer.size() - len;
        if( VSIFWriteL(abyOutputBuffer.data(), 1, nToWrite, fp.get()) != nToWrite )
        {
            CPLError(CE_Failure, CPLE_FileIO,
                 "VSIFWriteL() failed");
            return nullptr;
        }
        if( process_result != JXL_ENC_NEED_MORE_OUTPUT )
            break;
    }

    fp.reset();

    if( pfnProgress )
        pfnProgress(1.0, "", pProgressData);

    // Re-open file and clone missing info to PAM
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    auto poDS = OpenStaticPAM(&oOpenInfo);
    if( poDS )
    {
        // Do not create a .aux.xml file just for AREA_OR_POINT=Area
        const char* pszAreaOfPoint = poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        if( pszAreaOfPoint && EQUAL(pszAreaOfPoint, GDALMD_AOP_AREA) )
        {
            poDS->SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_AREA);
            poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
        }
#ifdef HAVE_JXL_BOX_API
        // When copying from JPEG, expose the EXIF metadata in the main domain,
        // so that PAM doesn't copy it.
        if( bEXIFFromMainDomain )
        {
            for( CSLConstList papszIter = papszEXIF; papszIter && *papszIter; ++papszIter )
            {
                if( STARTS_WITH(*papszIter, "EXIF_") )
                {
                    char* pszKey = nullptr;
                    const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
                    if( pszKey && pszValue )
                    {
                        poDS->SetMetadataItem(pszKey, pszValue);
                    }
                    CPLFree(pszKey);
                }
            }
            poDS->SetPamFlags(poDS->GetPamFlags() & ~GPF_DIRTY);
        }
#endif
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);
    }

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_JPEGXL()                         */
/************************************************************************/

void GDALRegister_JPEGXL()

{
    if( GDALGetDriverByName("JPEGXL") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("JPEGXL");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "JPEG-XL");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/jpegxl.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jxl");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jxl");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte UInt16 Float32");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='LOSSLESS' type='boolean' description='Whether JPEGXL compression should be lossless' default='YES'/>"
"   <Option name='EFFORT' type='int' description='Level of effort 1(fast)-9(slow)' default='5'/>"
"   <Option name='DISTANCE' type='float' description='Distance level for lossy compression (0=mathematically lossless, 1.0=visually lossless, usual range [0.5,3])' default='1.0' min='0.1' max='15.0'/>"
"   <Option name='QUALITY' type='float' description='Alternative setting to DISTANCE to specify lossy compression, roughly matching libjpeg quality setting in the [0,100] range' default='90' max='100'/>"
"   <Option name='NBITS' type='int' description='BITS for sub-byte files (1-7), sub-uint16_t (9-15)'/>"
"   <Option name='SOURCE_ICC_PROFILE' description='ICC profile encoded in Base64' type='string'/>\n"
#ifdef HAVE_JXL_THREADS
"   <Option name='NUM_THREADS' type='string' description='Number of worker threads for compression. Can be set to ALL_CPUS' default='ALL_CPUS'/>"
#endif
#ifdef HAVE_JXL_BOX_API
"   <Option name='WRITE_EXIF_METADATA' type='boolean' description='Whether to write EXIF_ metadata in a Exif box' default='YES'/>"
"   <Option name='WRITE_XMP' type='boolean' description='Whether to write xml:XMP metadata in a xml box' default='YES'/>"
"   <Option name='WRITE_GEOJP2' type='boolean' description='Whether to write georeferencing in a jumb.uuid box' default='YES'/>"
"   <Option name='COMPRESS_BOXES' type='boolean' description='Whether to decompress Exif/XMP/GeoJP2 boxes' default='NO'/>"
#endif
"</CreationOptionList>\n");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

#ifdef HAVE_JxlEncoderInitExtraChannelInfo
    poDriver->SetMetadataItem("JXL_ENCODER_SUPPORT_EXTRA_CHANNELS", "YES");
#endif

    poDriver->pfnIdentify = JPEGXLDataset::Identify;
    poDriver->pfnOpen = JPEGXLDataset::OpenStatic;
    poDriver->pfnCreateCopy = JPEGXLDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
