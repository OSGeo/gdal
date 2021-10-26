/******************************************************************************
 *
 * Project:  DDS Driver
 * Purpose:  Implement GDAL DDS Support
 * Author:   Alan Boudreault, aboudreault@mapgears.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Alan Boudreault
 * Copyright (c) 2013,2019, Even Rouault <even dot rouault at spatialys.com>
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
 ******************************************************************************/

#include "crunch_headers.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <algorithm>

CPL_CVSID("$Id$")

using namespace crnlib;

#define DDS_SIGNATURE "DDS "

enum { DDS_COLOR_TYPE_RGB,
       DDS_COLOR_TYPE_RGB_ALPHA };

constexpr uint32_t cDXTBlockSize = 4;

/************************************************************************/
/* ==================================================================== */
/*                              DDSDataset                              */
/* ==================================================================== */
/************************************************************************/

class DDSDataset final: public GDALPamDataset
{
    friend class DDSRasterBand;

    VSILFILE           *fp = nullptr;
    int                 nCurrentYBlock = -1;
    crn_format          nFormat = cCRNFmtInvalid;
    uint32_t            nBytesPerBlock = 0;
    uint32_t            nCompressedSizePerStripe = 0;
    void               *pCompressedBuffer = nullptr;
    void               *pUncompressedBuffer = nullptr;

                        DDSDataset() = default;
                       ~DDSDataset();
public:
    static int          Identify(GDALOpenInfo* poOpenInfo);
    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateCopy(const char * pszFilename,
                                   GDALDataset *poSrcDS,
                                   int bStrict, char ** papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void * pProgressData);
};

/************************************************************************/
/* ==================================================================== */
/*                            DDSRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class DDSRasterBand final: public GDALPamRasterBand
{
protected:
        CPLErr          IReadBlock(int, int, void*) override;
        GDALColorInterp GetColorInterpretation() override {
            return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1); }

public:
                        DDSRasterBand(DDSDataset* poDS, int nBand);
};

/************************************************************************/
/* ==================================================================== */
/*                         DDSDatasetAllDecoded                         */
/* ==================================================================== */
/************************************************************************/

class DDSDatasetAllDecoded final: public GDALPamDataset
{
    friend class DDSRasterBandAllDecoded;

    std::vector<crn_uint32*> m_pImages{};
    crn_texture_desc         m_tex_desc{};

                        DDSDatasetAllDecoded() = default;
                       ~DDSDatasetAllDecoded();
public:
    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
};

/************************************************************************/
/* ==================================================================== */
/*                        DDSRasterBandAllDecoded                       */
/* ==================================================================== */
/************************************************************************/

class DDSRasterBandAllDecoded final: public GDALPamRasterBand
{
protected:
        CPLErr          IReadBlock(int, int, void*) override;
        GDALColorInterp GetColorInterpretation() override {
            return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1); }

public:
                        DDSRasterBandAllDecoded(DDSDatasetAllDecoded* poDS, int nBand);
};

/************************************************************************/
/*                           ~DDSDataset()                              */
/************************************************************************/

DDSDataset::~DDSDataset()
{
    if( fp )
        VSIFCloseL(fp);
    CPLFree(pCompressedBuffer);
    CPLFree(pUncompressedBuffer);
}

/************************************************************************/
/*                           DDSRasterBand()                            */
/************************************************************************/

DDSRasterBand::DDSRasterBand(DDSDataset* poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = cDXTBlockSize;
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr DDSRasterBand::IReadBlock(int, int nYBlock, void* pImage)
{
    auto poGDS = cpl::down_cast<DDSDataset*>(poDS);
    const size_t nUncompressedBandOffset = static_cast<size_t>(nRasterXSize) * cDXTBlockSize;
    if( nYBlock != poGDS->nCurrentYBlock )
    {
        auto nFileOffset =
            strlen(DDS_SIGNATURE) + sizeof(crnlib::DDSURFACEDESC2) +
            static_cast<vsi_l_offset>(poGDS->nCompressedSizePerStripe) * nYBlock;
        VSIFSeekL( poGDS->fp, nFileOffset, SEEK_SET );
        if( VSIFReadL( poGDS->pCompressedBuffer,
                       poGDS->nCompressedSizePerStripe, 1, poGDS->fp ) != 1 )
        {
            return CE_Failure;
        }

        const uint32_t num_blocks_x = (nRasterXSize + cDXTBlockSize - 1) / cDXTBlockSize;
        const GByte* pabySrc = static_cast<const GByte*>(poGDS->pCompressedBuffer);
        crn_uint32 anDstPixels[cDXTBlockSize * cDXTBlockSize]; // A << 24 | B << 16 | G << 8 | R
        GByte* pabyDst = static_cast<GByte*>(poGDS->pUncompressedBuffer);
        const bool bHasAlpha = poGDS->nBands == 4;
        for( uint32_t block_x = 0; block_x < num_blocks_x; block_x++ )
        {
            const void* pSrc_block = pabySrc +
                static_cast<size_t>(block_x) * poGDS->nBytesPerBlock;
            if( !crn_decompress_block(pSrc_block, anDstPixels, poGDS->nFormat) )
            {
                return CE_Failure;
            }
            const crn_uint32* pUncompressedPixels = anDstPixels;
            for (uint32_t y = 0; y < cDXTBlockSize; y++)
            {
                for (uint32_t x = 0; x < cDXTBlockSize; x++)
                {
                    const uint32_t actual_x = block_x * cDXTBlockSize + x;
                    if( actual_x < static_cast<uint32_t>(nRasterXSize) )
                    {
                        const auto nRGBAPixel = *pUncompressedPixels;
                        const auto offsetInBand =
                            actual_x + static_cast<size_t>(y) * nRasterXSize;
                        pabyDst[offsetInBand] =
                            static_cast<GByte>(nRGBAPixel & 0xff);
                        pabyDst[nUncompressedBandOffset + offsetInBand] =
                            static_cast<GByte>((nRGBAPixel >> 8) & 0xff);
                        pabyDst[2 * nUncompressedBandOffset + offsetInBand] =
                            static_cast<GByte>((nRGBAPixel >> 16) & 0xff);
                        if( bHasAlpha )
                        {
                            pabyDst[3 * nUncompressedBandOffset + offsetInBand] =
                                static_cast<GByte>((nRGBAPixel >> 24) & 0xff);
                        }
                    }
                    pUncompressedPixels++;
                }
            }
        }
        poGDS->nCurrentYBlock = nYBlock;
    }
    const GByte* pabyUncompressed =
        static_cast<const GByte*>(poGDS->pUncompressedBuffer);
    memcpy(pImage, pabyUncompressed + (nBand - 1) * nUncompressedBandOffset,
           nUncompressedBandOffset);
    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int DDSDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( poOpenInfo->fpL == nullptr || poOpenInfo->eAccess == GA_Update ||
        static_cast<size_t>(poOpenInfo->nHeaderBytes) <
            strlen(DDS_SIGNATURE) + sizeof(crnlib::DDSURFACEDESC2) )
    {
        return false;
    }

    // Check signature and dwSize member of DDSURFACEDESC2
    return memcmp(poOpenInfo->pabyHeader, DDS_SIGNATURE, strlen(DDS_SIGNATURE)) == 0 &&
           CPL_LSBUINT32PTR(poOpenInfo->pabyHeader + strlen(DDS_SIGNATURE)) ==
                sizeof(crnlib::DDSURFACEDESC2);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* DDSDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) )
        return nullptr;

    crnlib::DDSURFACEDESC2 ddsDesc;
    memcpy(&ddsDesc, poOpenInfo->pabyHeader + strlen(DDS_SIGNATURE), sizeof(ddsDesc));
#ifdef CPL_MSB
    {
        GUInt32* ddsDescAsUInt32 = reinterpret_cast<GUInt32*>(&ddsDesc);
        for( size_t i = 0; i < sizeof(ddsDesc) / sizeof(GUInt32); ++i )
        {
            CPL_LSBPTR32(&ddsDescAsUInt32[i]);
        }
    }
#endif
    if( ddsDesc.dwWidth == 0 ||
        ddsDesc.dwWidth > INT_MAX ||
        ddsDesc.dwHeight == 0 ||
        ddsDesc.dwHeight > INT_MAX )
    {
        return nullptr;
    }
    if( ddsDesc.ddpfPixelFormat.dwSize != sizeof(crnlib::DDPIXELFORMAT) )
    {
        CPLDebug("DDS", "Unsupported ddpfPixelFormat.dwSize = %u",
                 ddsDesc.ddpfPixelFormat.dwSize);
        return nullptr;
    }

    if( ddsDesc.ddpfPixelFormat.dwFlags != DDPF_FOURCC )
    {
#ifdef DEBUG
        CPLDebug("DDS", "Unsupported ddpfPixelFormat.dwFlags in regular path: %u",
                 ddsDesc.ddpfPixelFormat.dwFlags);
#endif
        return DDSDatasetAllDecoded::Open(poOpenInfo);
    }
    crn_format fmt = cCRNFmtInvalid;
    switch( ddsDesc.ddpfPixelFormat.dwFourCC )
    {
        case PIXEL_FMT_DXT1:
        case PIXEL_FMT_DXT1A:
            fmt = cCRNFmtDXT1;
            break;
        case PIXEL_FMT_DXT2:
        case PIXEL_FMT_DXT3:
            fmt = cCRNFmtDXT3;
            break;
        case PIXEL_FMT_DXT4:
        case PIXEL_FMT_DXT5:
            fmt = cCRNFmtDXT5;
            break;
        //case PIXEL_FMT_DXT5A:
        //    fmt = cCRNFmtDXT5A;
        //    break;
        case PIXEL_FMT_ETC1:
            fmt = cCRNFmtETC1;
            break;
        //case PIXEL_FMT_3DC:
        //    fmt = cCRNFmtDXN_YX;
        //    break;
        //case PIXEL_FMT_DXN:
        //    fmt = cCRNFmtDXN_XY;
        //    break;
        default:
        {
#ifdef DEBUG
            char szFourCC[5] = {};
            memcpy(&szFourCC[0], &ddsDesc.ddpfPixelFormat.dwFourCC, 4);
            CPLDebug("DDS", "Unhandled FOURCC = %s in regular path", szFourCC);
#endif
            return DDSDatasetAllDecoded::Open(poOpenInfo);
        }
    }

    const uint32_t bytesPerBlock = crn_get_bytes_per_dxt_block(fmt);
    const uint32_t num_blocks_x = (ddsDesc.dwWidth + cDXTBlockSize - 1) / cDXTBlockSize;
    const uint32_t num_blocks_y = (ddsDesc.dwHeight + cDXTBlockSize - 1) / cDXTBlockSize;
    const uint32_t compressed_size_per_row = num_blocks_x * bytesPerBlock;
    const vsi_l_offset nCompressedDataSize = static_cast<vsi_l_offset>(compressed_size_per_row) * num_blocks_y;

    VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
    if( VSIFTellL(poOpenInfo->fpL) < strlen(DDS_SIGNATURE) + sizeof(crnlib::DDSURFACEDESC2) + nCompressedDataSize )
    {
        CPLDebug("DDS", "File too small");
        return nullptr;
    }
    void* pCompressedBuffer = VSI_MALLOC_VERBOSE(compressed_size_per_row);
    const int l_nBands = fmt == cCRNFmtETC1 ? 3 : 4;
    void* pUncompressedBuffer = VSI_MALLOC3_VERBOSE(ddsDesc.dwWidth, cDXTBlockSize, l_nBands);
    if( pCompressedBuffer == nullptr || pUncompressedBuffer == nullptr )
    {
        VSIFree(pCompressedBuffer);
        VSIFree(pUncompressedBuffer);
        return nullptr;
    }

    DDSDataset* poDS = new DDSDataset();
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    poDS->nBytesPerBlock = bytesPerBlock;
    poDS->nFormat = fmt;
    poDS->nCompressedSizePerStripe = compressed_size_per_row;
    poDS->pCompressedBuffer = pCompressedBuffer;
    poDS->pUncompressedBuffer = pUncompressedBuffer;
    poDS->nRasterXSize = static_cast<int>(ddsDesc.dwWidth);
    poDS->nRasterYSize = static_cast<int>(ddsDesc.dwHeight);
    poDS->GDALDataset::SetMetadataItem("COMPRESSION",
                                       crn_get_format_string(fmt),
                                       "IMAGE_STRUCTURE");
    poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    for( int i = 0; i < l_nBands; i++ )
    {
        poDS->SetBand(i + 1, new DDSRasterBand(poDS, i+1));
    }

    return poDS;
}

/************************************************************************/
/*                        ~DDSDatasetAllDecoded()                       */
/************************************************************************/

DDSDatasetAllDecoded::~DDSDatasetAllDecoded()
{
    crn_free_all_images(&m_pImages[0], m_tex_desc);
}

/************************************************************************/
/*                        DDSRasterBandAllDecoded()                     */
/************************************************************************/

DDSRasterBandAllDecoded::DDSRasterBandAllDecoded(DDSDatasetAllDecoded* poDSIn,
                                                 int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = 1;
    eDataType = GDT_Byte;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr DDSRasterBandAllDecoded::IReadBlock(int, int nYBlock, void* pImage)
{
    auto poGDS = cpl::down_cast<DDSDatasetAllDecoded*>(poDS);
    GByte* pabyData = static_cast<GByte*>(pImage);
    const int nShift = (nBand - 1) * 8;
    for( int i = 0; i < nRasterXSize; i++ )
    {
        pabyData[i] = static_cast<GByte>(
            (poGDS->m_pImages[0][i + nYBlock * nRasterXSize] >> nShift) & 0xff);
    }
    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* DDSDatasetAllDecoded::Open(GDALOpenInfo* poOpenInfo)
{
    VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
    std::vector<GByte> data;
    const auto nFileSize = VSIFTellL(poOpenInfo->fpL);
    if( nFileSize > 100 * 1024 * 1024 )
        return nullptr;
    try
    {
        data.resize(static_cast<size_t>(nFileSize));
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return nullptr;
    }
    VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
    VSIFReadL(&data[0], 1, data.size(),poOpenInfo->fpL);

    std::vector<crn_uint32*> pImages;
    pImages.resize(cCRNMaxFaces * cCRNMaxLevels);
    crn_texture_desc tex_desc;
    const bool bRet = crn_decompress_dds_to_images(
        data.data(), static_cast<crn_uint32>(data.size()), &pImages[0], tex_desc);
#ifdef DEBUG
    CPLDebug("DDS", "w=%u h=%u faces=%u levels=%u fourCC=%c%c%c%c",
             tex_desc.m_width, tex_desc.m_height,
             tex_desc.m_faces, tex_desc.m_levels,
             static_cast<char>(tex_desc.m_fmt_fourcc & 0xff),
             static_cast<char>((tex_desc.m_fmt_fourcc >> 8) & 0xff),
             static_cast<char>((tex_desc.m_fmt_fourcc >> 16) & 0xff),
             static_cast<char>((tex_desc.m_fmt_fourcc >> 24) & 0xff));
#endif
    if( !bRet)
    {
        CPLDebug("DDS", "crn_decompress_dds_to_images() failed");
        return nullptr;
    }
    auto poDS = new DDSDatasetAllDecoded();
    poDS->nRasterXSize = static_cast<int>(tex_desc.m_width);
    poDS->nRasterYSize = static_cast<int>(tex_desc.m_height);
    poDS->m_tex_desc = tex_desc;
    poDS->m_pImages = std::move(pImages);
    constexpr int NBANDS = 4;
    poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    for( int i = 0; i < NBANDS; i++ )
    {
        poDS->SetBand(i + 1, new DDSRasterBandAllDecoded(poDS, i+1));
    }

    return poDS;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
DDSDataset::CreateCopy(const char * pszFilename, GDALDataset *poSrcDS,
                       int bStrict, char ** papszOptions,
                       GDALProgressFunc pfnProgress, void * pProgressData)

{
    int  nBands = poSrcDS->GetRasterCount();

    /* -------------------------------------------------------------------- */
    /*      Some rudimentary checks                                         */
    /* -------------------------------------------------------------------- */
    if (nBands != 3 && nBands != 4)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DDS driver doesn't support %d bands. Must be 3 (rgb) \n"
                 "or 4 (rgba) bands.\n",
                 nBands);

        return nullptr;
    }

    if (poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
    {
        CPLError( (bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                  "DDS driver doesn't support data type %s. "
                  "Only eight bit (Byte) bands supported. %s\n",
                  GDALGetDataTypeName(
                                      poSrcDS->GetRasterBand(1)->GetRasterDataType()),
                  (bStrict) ? "" : "Defaulting to Byte" );

        if (bStrict)
            return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Setup some parameters.                                          */
    /* -------------------------------------------------------------------- */
    int  nColorType = 0;

    if (nBands == 3)
      nColorType = DDS_COLOR_TYPE_RGB;
    else if (nBands == 4)
      nColorType = DDS_COLOR_TYPE_RGB_ALPHA;

    /* -------------------------------------------------------------------- */
    /*      Create the dataset.                                             */
    /* -------------------------------------------------------------------- */
    VSILFILE *fpImage = VSIFOpenL(pszFilename, "wb");
    if (fpImage == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Unable to create dds file %s.\n",
                 pszFilename);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the Crunch compressor                                    */
    /* -------------------------------------------------------------------- */

    /* Default values */
    crn_format fmt = cCRNFmtDXT3;
    crn_dxt_quality dxt_quality = cCRNDXTQualityNormal;
    bool srgb_colorspace = true;
    bool dxt1a_transparency = true;
    //bool generate_mipmaps = true;

    /* Check the texture format */
    const char *pszFormat = CSLFetchNameValue( papszOptions, "FORMAT" );

    if (pszFormat)
    {
        if (EQUAL(pszFormat, "dxt1"))
            fmt = cCRNFmtDXT1;
        else if (EQUAL(pszFormat, "dxt1a"))
            fmt = cCRNFmtDXT1;
        else if (EQUAL(pszFormat, "dxt3"))
            fmt = cCRNFmtDXT3;
        else if (EQUAL(pszFormat, "dxt5"))
            fmt = cCRNFmtDXT5;
        else if (EQUAL(pszFormat, "etc1"))
            fmt = cCRNFmtETC1;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal FORMAT value '%s', should be DXT1, DXT1A, DXT3, DXT5 or ETC1",
                      pszFormat );
            return nullptr;
        }
    }

    /* Check the compression quality */
    const char *pszQuality = CSLFetchNameValue( papszOptions, "QUALITY" );

    if (pszQuality)
    {
        if (EQUAL(pszQuality, "SUPERFAST"))
            dxt_quality = cCRNDXTQualitySuperFast;
        else if (EQUAL(pszQuality, "FAST"))
            dxt_quality = cCRNDXTQualityFast;
        else if (EQUAL(pszQuality, "NORMAL"))
            dxt_quality = cCRNDXTQualityNormal;
        else if (EQUAL(pszQuality, "BETTER"))
            dxt_quality = cCRNDXTQualityBetter;
        else if (EQUAL(pszQuality, "UBER"))
            dxt_quality = cCRNDXTQualityUber;
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal QUALITY value '%s', should be SUPERFAST, FAST, NORMAL, BETTER or UBER.",
                      pszQuality );
            return nullptr;
        }
    }

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    crn_comp_params comp_params;
    comp_params.m_format = fmt;
    comp_params.m_dxt_quality = dxt_quality;
    comp_params.set_flag(cCRNCompFlagPerceptual, srgb_colorspace);
    comp_params.set_flag(cCRNCompFlagDXT1AForTransparency, dxt1a_transparency);

    crn_block_compressor_context_t pContext = crn_create_block_compressor(comp_params);

    /* -------------------------------------------------------------------- */
    /*      Write the DDS header to the file.                               */
    /* -------------------------------------------------------------------- */

    VSIFWriteL(DDS_SIGNATURE, 1, strlen(DDS_SIGNATURE), fpImage);

    crnlib::DDSURFACEDESC2 ddsDesc;
    memset(&ddsDesc, 0, sizeof(ddsDesc));
    ddsDesc.dwSize = sizeof(ddsDesc);
    ddsDesc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_MIPMAPCOUNT | DDSD_PIXELFORMAT | DDSD_DEPTH ;
    ddsDesc.dwWidth = nXSize;
    ddsDesc.dwHeight = nYSize;
    ddsDesc.dwMipMapCount = 1;

    ddsDesc.ddpfPixelFormat.dwSize = sizeof(crnlib::DDPIXELFORMAT);
    ddsDesc.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
    ddsDesc.ddpfPixelFormat.dwFourCC = crn_get_format_fourcc(fmt);
    ddsDesc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;

    // Set pitch/linearsize field (some DDS readers require this field to be non-zero).
    uint32_t bits_per_pixel = crn_get_format_bits_per_texel(fmt);
    ddsDesc.lPitch = (((ddsDesc.dwWidth + 3) & ~3U) * ((ddsDesc.dwHeight + 3) & ~3U) * bits_per_pixel) >> 3;
    ddsDesc.dwFlags |= DDSD_LINEARSIZE;

#ifdef CPL_MSB
    {
        GUInt32* ddsDescAsUInt32 = reinterpret_cast<GUInt32*>(&ddsDesc);
        for( size_t i = 0; i < sizeof(ddsDesc) / sizeof(GUInt32); ++i )
        {
            CPL_LSBPTR32(&ddsDescAsUInt32[i]);
        }
    }
#endif
    VSIFWriteL(&ddsDesc, 1, sizeof(ddsDesc), fpImage);

    /* -------------------------------------------------------------------- */
    /*      Loop over image, compressing image data.                        */
    /* -------------------------------------------------------------------- */
    const uint32_t bytesPerBlock = crn_get_bytes_per_dxt_block(fmt);
    CPLErr eErr = CE_None;
    const uint32_t nYNumBlocks = (nYSize + cDXTBlockSize - 1) / cDXTBlockSize;
    const uint32_t num_blocks_x = (nXSize + cDXTBlockSize - 1) / cDXTBlockSize;
    const uint32_t total_compressed_size = num_blocks_x * bytesPerBlock;

    void *pCompressed_data = CPLMalloc(total_compressed_size);
    GByte* pabyScanlines = (GByte *) CPLMalloc(nBands * nXSize * cDXTBlockSize);
    crn_uint32 *pixels = (crn_uint32*) CPLMalloc(sizeof(crn_uint32)*cDXTBlockSize * cDXTBlockSize);
    crn_uint32 *src_image = nullptr;
    if (nColorType == DDS_COLOR_TYPE_RGB)
        src_image = (crn_uint32*) CPLMalloc(sizeof(crn_uint32)*nXSize*cDXTBlockSize);

    for (uint32_t iLine = 0; iLine < nYNumBlocks && eErr == CE_None; iLine++)
    {
        const uint32_t size_y = (iLine*cDXTBlockSize+cDXTBlockSize) < (uint32_t)nYSize ?
                           cDXTBlockSize : (cDXTBlockSize-((iLine*cDXTBlockSize+cDXTBlockSize)-(uint32_t)nYSize));

        eErr = poSrcDS->RasterIO(GF_Read, 0, iLine*cDXTBlockSize, nXSize, size_y,
                                 pabyScanlines, nXSize, size_y, GDT_Byte,
                                 nBands, nullptr,
                                 nBands,
                                 nBands * nXSize, 1, nullptr);

        if (eErr != CE_None)
            break;

        crn_uint32 *pSrc_image = nullptr;
        if (nColorType == DDS_COLOR_TYPE_RGB_ALPHA)
            pSrc_image = (crn_uint32*)pabyScanlines;
        else if (nColorType == DDS_COLOR_TYPE_RGB)
        { /* crunch needs 32bits integers */
            int nPixels = nXSize*cDXTBlockSize;
            for (int i=0; i<nPixels;++i)
            {
                int y = (i*3);
                src_image[i] = (255U <<24) | (pabyScanlines[y+2]<<16) | (pabyScanlines[y+1]<<8) |
                  pabyScanlines[y];
            }

            pSrc_image = &(src_image[0]);
        }

        for (crn_uint32 block_x = 0; block_x < num_blocks_x; block_x++)
        {
            // Exact block from image, clamping at the sides of non-divisible by
            // 4 images to avoid artifacts.
            crn_uint32 *pDst_pixels = pixels;
            for (uint32_t y = 0; y < cDXTBlockSize; y++)
            {
                const uint32_t actual_y = std::min(y, size_y - 1U);
                for (uint32_t x = 0; x < cDXTBlockSize; x++)
                {
                    const uint32_t actual_x =
                        std::min(nXSize - 1U, (block_x * cDXTBlockSize) + x);
                    *pDst_pixels++ = pSrc_image[actual_x + actual_y * nXSize];
                }
            }

            // Compress the DXTn block.
            crn_compress_block(pContext, pixels, static_cast<crn_uint8 *>(pCompressed_data) + block_x * bytesPerBlock);
        }

        VSIFWriteL(pCompressed_data, 1, total_compressed_size, fpImage);

        if (!pfnProgress( (iLine+1) / (double) nYNumBlocks,
                             nullptr, pProgressData))
        {
            eErr = CE_Failure;
            CPLError(CE_Failure, CPLE_UserInterrupt,
                      "User terminated CreateCopy()");
        }
    }

    CPLFree(src_image);
    CPLFree(pixels);
    CPLFree(pCompressed_data);
    CPLFree(pabyScanlines);
    crn_free_block_compressor(pContext);
    pContext = nullptr;

    VSIFCloseL(fpImage);

    if (eErr != CE_None)
        return nullptr;

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                          GDALRegister_DDS()                          */
/************************************************************************/

void GDALRegister_DDS()
{
    if( GDALGetDriverByName( "DDS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("DDS");
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "DirectDraw Surface");
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/dds.html" );
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dds");
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/dds");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='FORMAT' type='string-select' description='Texture format' default='DXT3'>\n"
        "     <Value>DXT1</Value>\n"
        "     <Value>DXT1A</Value>\n"
        "     <Value>DXT3</Value>\n"
        "     <Value>DXT5</Value>\n"
        "     <Value>ETC1</Value>\n"
        "   </Option>\n"
        "   <Option name='QUALITY' type='string-select' description='Compression Quality' default='NORMAL'>\n"
        "     <Value>SUPERFAST</Value>\n"
        "     <Value>FAST</Value>\n"
        "     <Value>NORMAL</Value>\n"
        "     <Value>BETTER</Value>\n"
        "     <Value>UBER</Value>\n"
        "   </Option>\n"
        "</CreationOptionList>\n" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->pfnIdentify = DDSDataset::Identify;
    poDriver->pfnOpen = DDSDataset::Open;
    poDriver->pfnCreateCopy = DDSDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
