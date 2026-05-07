/*******************************************************************************
 *
 * Author:   Aaron Boxer, <aaron.boxer at grokcompression dot com>
 *
 *******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, European Union (European Environment Agency)
 * Copyright (c) 2026, Grok Image Compression Inc.
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/

/**
 * @file grkdatasetbase.h
 * @brief Grok JPEG 2000 GDAL driver — codec wrapper and dataset base class.
 *
 * This header defines two main types used by the JP2Grok driver:
 *
 * 1. **GRKCodecWrapper** — wraps the Grok codec (grk_object*), image,
 *    file handle, and compress/decompress parameters.  It presents the
 *    same interface expected by the templated shared code in
 *    jp2opjlikedataset.cpp (Open, CreateCopy).  Ownership is move-only:
 *    the "copy" constructor actually transfers all resources.
 *
 * 2. **JP2GRKDatasetBase** — the dataset base class that plugs into
 *    JP2DatasetBase.  It implements DirectRasterIO-based reading
 *    (bypassing GDAL's block cache entirely) and asynchronous Grok
 *    decompression.
 *
 * ## Read architecture
 *
 * Unlike the OpenJPEG driver which uses SetDecodeArea + ReadBlock,
 * the Grok driver always returns canPerformDirectIO() == true, causing
 * ALL raster reads to flow through:
 *
 *   Band::IRasterIO -> Dataset::DirectRasterIO -> decompressAsynch
 *     -> grk_decompress / grk_decompress_wait -> CopyTiles -> CopyTileData
 *
 * decompressAsynch() calls grk_decompress() once on the first read
 * to start async decoding of the full image.  Subsequent reads reuse
 * the cached tile-grid info with tile coordinates recomputed for
 * each requested swath.
 *
 * ## Overview handling
 *
 * JPEG2000 images contain multiple resolution levels (overviews).
 * Overview datasets are created during Open() by the shared code in
 * jp2opjlikedataset.cpp.  Each overview dataset lazily creates its own
 * GRKCodecWrapper on first read (in decompressAsynch), configured with
 * the appropriate reduce level (iLevel).
 *
 * ## Compression
 *
 * Grok compresses the entire image in a single grk_compress() call.
 * The compressTile() method accumulates tile data into the Grok image
 * component buffers (psImage->comps[band].data), and finishCompress()
 * triggers the actual encode.  For YCBCR420, subsampled Cb/Cr planes
 * are interleaved per the JPEG2000 convention (dx=dy=2).
 */
#pragma once

#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <algorithm>

#ifdef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#define STAT_FUNC _stat
#else
#include <sys/stat.h>
#define STAT_FUNC stat
#endif

#include <grok.h>
#include <grk_config.h>

#include "gdal_thread_pool.h"
#include "gdaljp2metadata.h"

#ifdef HAVE_CURL
#include "cpl_aws.h"
#endif

/**
 * @brief Bounded string copy that always null-terminates.
 *
 * Copies up to N-1 characters from @p src into a fixed-size char array,
 * ensuring the result is always null-terminated.
 */
template <size_t N> void safe_strcpy(char (&dest)[N], const char *src)
{
    size_t len = strnlen(src, N - 1);
    memcpy(dest, src, len);
    dest[len] = '\0';
}

/** @overload safe_strcpy for std::string source */
template <size_t N> void safe_strcpy(char (&dest)[N], const std::string &src)
{
    size_t len = strnlen(src.c_str(), N - 1);
    memcpy(dest, src.c_str(), len);
    dest[len] = '\0';
}

/************************************************************************/
/*                            GrokCanRead()                             */
/************************************************************************/

/**
 * @brief Check if Grok can read the file directly (bypassing VSILFILE).
 *
 * When true, Grok opens the file via its own I/O (streamParams.file)
 * which is more efficient. When false, GDAL's VSILFILE callbacks are used.
 *
 * @param filename file path
 * @return true if Grok can use native file I/O
 */
static bool GrokCanRead(const char *filename)
{
    if (filename == nullptr)
        return false;

#if defined(HAVE_CURL) && defined(GRK_HAS_LIBCURL)
    // Cloud storage: Grok handles S3 fetching natively via libcurl.
    // GDAL resolves AWS credentials and passes them to Grok via
    // grk_stream_params (username/password/bearer_token/region).
    if (strncmp(filename, "/vsis3/", 7) == 0)
        return true;
#endif

    // Any other GDAL virtual filesystem must go through VSILFILE callbacks
    if (strncmp(filename, "/vsi", 4) == 0)
        return false;

    // Local file: verify it exists and is accessible
    struct STAT_FUNC localStat;
    return STAT_FUNC(filename, &localStat) == 0;
}

/**
 * @struct PostPreload
 * @brief Result returned by decompressAsynch() describing the decoded swath.
 *
 * Contains both pixel coordinates (x0,y0,x1,y1) for the requested region
 * and tile-grid coordinates (tile_x0..tile_y1) identifying which tiles
 * were decoded.  CopyTiles() iterates over the tile range to extract
 * decoded pixel data.
 */
struct PostPreload
{
    bool asynch_ = false;        ///< True if decompress succeeded
    bool rowCopyDone_ = false;   ///< True if per-row copy was done during wait
    uint32_t x0 = 0;             ///< Decoded region left (pixels)
    uint32_t y0 = 0;             ///< Decoded region top (pixels)
    uint32_t x1 = 0;             ///< Decoded region right (pixels)
    uint32_t y1 = 0;             ///< Decoded region bottom (pixels)
    uint16_t tile_x0 = 0;        ///< First tile column (inclusive)
    uint16_t tile_y0 = 0;        ///< First tile row (inclusive)
    uint16_t tile_x1 = 0;        ///< Last tile column (exclusive)
    uint16_t tile_y1 = 0;        ///< Last tile row (exclusive)
    uint16_t num_tile_cols = 0;  ///< Total tile columns in image grid
    int enoughMemoryToLoadOtherBands = FALSE;
};

/// Callback invoked per tile-row during row-by-row decompression wait.
/// Receives a PostPreload covering one tile row's worth of tiles.
using RowCopyFunc = std::function<CPLErr(PostPreload &)>;

/************************************************************************/
/*                        JP2_WarningCallback()                         */
/************************************************************************/

static void JP2_WarningCallback(const char *pszMsg, CPL_UNUSED void *unused)
{
    std::string osMsg(pszMsg);
    if (!osMsg.empty() && osMsg.back() == '\n')
        osMsg.resize(osMsg.size() - 1);
    CPLError(CE_Warning, CPLE_AppDefined, "%s", osMsg.c_str());
}

/************************************************************************/
/*                          JP2_InfoCallback()                          */
/************************************************************************/

static void JP2_InfoCallback(const char *pszMsg, CPL_UNUSED void *unused)
{
    std::string osMsg(pszMsg);
    if (!osMsg.empty() && osMsg.back() == '\n')
        osMsg.resize(osMsg.size() - 1);
    CPLDebug("GROK", "%s", osMsg.c_str());
}

/************************************************************************/
/*                         JP2_ErrorCallback()                          */
/************************************************************************/

static void JP2_ErrorCallback(const char *pszMsg, CPL_UNUSED void *unused)
{
    CPLError(CE_Failure, CPLE_AppDefined, "%s", pszMsg);
}

/************************************************************************/
/*                         JP2_DebugCallback()                          */
/************************************************************************/

static void JP2_DebugCallback(const char *pszMsg, CPL_UNUSED void *unused)
{
    std::string osMsg(pszMsg);
    if (!osMsg.empty() && osMsg.back() == '\n')
        osMsg.resize(osMsg.size() - 1);
    CPLDebug("GROK", "%s", osMsg.c_str());
}

/************************************************************************/
/*                          JP2Dataset_Write()                          */
/************************************************************************/

/**
 * @brief VSILFILE write callback for Grok stream I/O.
 *
 * Used when GrokCanRead() returns false (e.g. /vsimem/, /vsicurl/).
 * Also always used for compression output.
 */
static size_t JP2Dataset_Write(const uint8_t *pBuffer, size_t nBytes,
                               void *pUserData)
{
    auto psJP2File = static_cast<JP2File *>(pUserData);
    size_t nRet =
        static_cast<size_t>(VSIFWriteL(pBuffer, 1, nBytes, psJP2File->fp_));
#ifdef DEBUG_IO
    CPLDebug(debugId(), "JP2Dataset_Write(" CPL_FRMT_GUIB ") = " CPL_FRMT_GUIB,
             static_cast<GUIntBig>(nBytes), static_cast<GUIntBig>(nRet));
#endif
    if (nRet != nBytes)
        return static_cast<size_t>(-1);
    return nRet;
}

/************************************************************************/
/*                          JP2Dataset_Read()                           */
/************************************************************************/

/**
 * @brief VSILFILE read callback for Grok stream I/O.
 *
 * Returns (size_t)-1 on EOF, as expected by the Grok stream API.
 */
static size_t JP2Dataset_Read(uint8_t *pBuffer, size_t nBytes, void *pUserData)
{
    auto psJP2File = static_cast<JP2File *>(pUserData);
    size_t nRet =
        static_cast<size_t>(VSIFReadL(pBuffer, 1, nBytes, psJP2File->fp_));
#ifdef DEBUG_IO
    CPLDebug(debugId(), "JP2Dataset_Read(" CPL_FRMT_GUIB ") = " CPL_FRMT_GUIB,
             static_cast<GUIntBig>(nBytes), static_cast<GUIntBig>(nRet));
#endif
    if (nRet == 0)
        nRet = static_cast<size_t>(-1);

    return nRet;
}

/************************************************************************/
/*                          JP2Dataset_Seek()                           */
/************************************************************************/

/**
 * @brief VSILFILE seek callback for Grok stream I/O.
 *
 * Seeks are relative to psJP2File->nBaseOffset, which is the start of
 * the codestream within the file (non-zero when JP2 boxes precede the
 * codestream, or when reading from NITF).
 */
static bool JP2Dataset_Seek(uint64_t nBytes, void *pUserData)
{
    auto psJP2File = static_cast<JP2File *>(pUserData);
#ifdef DEBUG_IO
    CPLDebug(debugId(), "JP2Dataset_Seek(" CPL_FRMT_GUIB ")",
             static_cast<GUIntBig>(nBytes));
#endif
    return VSIFSeekL(psJP2File->fp_, psJP2File->nBaseOffset + nBytes,
                     SEEK_SET) == 0;
}

/**
 * @struct GRKCodecWrapper
 * @brief Generic wrapper around grok-specific objects.
 *
 * This wraps the Grok JPEG2000 codec (grk_object*) along with its
 * image, file handle, and compression/decompression parameters.
 *
 * OWNERSHIP MODEL: The "copy constructor" GRKCodecWrapper(GRKCodecWrapper*)
 * is actually a MOVE — it transfers ownership of pCodec, psImage, etc.
 * from the source and nulls them out. Only one GRKCodecWrapper can own
 * a given codec at a time. The main dataset's m_codec is populated via
 * openCompleteJP2() → cacheNew(), which moves the codec from the local
 * context. Overview datasets do NOT get their own codecs.
 *
 */
struct GRKCodecWrapper
{
    typedef grk_image jp2_image;
    typedef grk_image_comp jp2_image_comp;
    typedef grk_object jp2_codec;

    /**
     * @brief Constructs a new GRKCodecWrapper object
     *
     */
    GRKCodecWrapper(void)
    {
        grk_compress_set_default_params(&compressParams);
    }

    /**
     * @brief Move-constructs a GRKCodecWrapper, taking ownership from rhs.
     * After construction, rhs is empty (all pointers null).
     *
     * @param rhs source wrapper to move from
     */
    explicit GRKCodecWrapper(GRKCodecWrapper *rhs)
        : pCodec(rhs->pCodec), psImage(rhs->psImage),
          bImageOwned(rhs->bImageOwned), pasBandParams(rhs->pasBandParams),
          psJP2File(rhs->psJP2File), irreversible(rhs->irreversible)
    {
        rhs->pCodec = nullptr;
        rhs->psImage = nullptr;
        rhs->pasBandParams = nullptr;
        rhs->psJP2File = nullptr;
        rhs->bImageOwned = false;
    }

    /**
     * @brief Destroys the GRKCodecWrapper object
     *
     */
    ~GRKCodecWrapper(void)
    {
        free();
    }

    /**
     * @brief Returns stride of @ref jp2_image_comp
     *
     * @param comp @ref jp2_image_comp
     * @return uint32_t stride
     */
    static uint32_t stride(jp2_image_comp *comp)
    {
        return comp->stride;
    }

    /**
     * @brief Opens a @ref VSILFILE virtual file
     *
     * @param fp @ref VSILFILE
     * @param offset offset into file
     */
    void open(VSILFILE *fp, vsi_l_offset offset)
    {
        psJP2File = static_cast<JP2File *>(CPLMalloc(sizeof(JP2File)));
        psJP2File->fp_ = fp;
        psJP2File->nBaseOffset = offset;
    }

    /**
     * @brief Opens a @ref VSILFILE virtual file
     *
     * @param fp @ref VSIFILE virtual file
     * Offset is set to current file offset
     */
    void open(VSILFILE *fp)
    {
        psJP2File = static_cast<JP2File *>(CPLMalloc(sizeof(JP2File)));
        psJP2File->fp_ = fp;
        psJP2File->nBaseOffset = VSIFTellL(fp);
    }

    /**
     * @brief Transfers contents of another wrapper to this one
     *
     * @param rhs transferee
     */
    void transfer(GRKCodecWrapper *rhs)
    {
        pCodec = rhs->pCodec;
        rhs->pCodec = nullptr;
        psImage = rhs->psImage;
        rhs->psImage = nullptr;
        psJP2File = rhs->psJP2File;
        rhs->psJP2File = nullptr;
        irreversible = rhs->irreversible;
        bImageOwned = rhs->bImageOwned;
        rhs->bImageOwned = false;
    }

    /**
     * @brief Converts a generic enumeration to an analogous Grok enumeration
     *
     * @param enumeration @ref JP2_ENUM
     * @return int Grok enumeration
     */
    static int cvtenum(JP2_ENUM enumeration)
    {
        switch (enumeration)
        {
            case JP2_CLRSPC_UNKNOWN:
                return GRK_CLRSPC_UNKNOWN;
                break;
            case JP2_CLRSPC_SRGB:
                return GRK_CLRSPC_SRGB;
                break;
            case JP2_CLRSPC_GRAY:
                return GRK_CLRSPC_GRAY;
                break;
            case JP2_CLRSPC_SYCC:
                return GRK_CLRSPC_SYCC;
                break;
            case JP2_CODEC_J2K:
                return GRK_CODEC_J2K;
                break;
            case JP2_CODEC_JP2:
                return GRK_CODEC_JP2;
                break;
            default:
                return INT_MAX;
                break;
        }
    }

    /**
     * @brief Gets the comment used for JPEG 2000 file compression
     *
     * @return std::string comment
     */
    std::string getComment(void)
    {
        (void)this;
        std::string osCreatedBy = "Created by Grok version ";

        return osCreatedBy + grk_version();
    }

    /**
     * @brief This is unused but is here for compatibility with OpenJPEG
     *
     * @param strict unused, for OpenJPEG compatibility
     */
    void updateStrict(CPL_UNUSED bool strict)
    {
        // prevent linter from treating this as potential static method
        (void)this;
    }

    /**
     * @brief Gets debug id
     *
     * @return const char* debug id
     */
    static const char *debugId(void)
    {
        return "GROK";
    }

    /**
     * @brief Allocates component parameters
     *
     * @param nBands number of bands
     */
    void allocComponentParams(int nBands)
    {
        // need to zero-init the component structs
        pasBandParams = static_cast<grk_image_comp *>(
            CPLCalloc(nBands, sizeof(grk_image_comp)));
    }

    /**
     * @brief Frees wrapper resources
     *
     */
    void free(void)
    {
        if (pCodec)
            grk_object_unref(pCodec);
        pCodec = nullptr;
        if (psImage && bImageOwned)
            grk_object_unref(&psImage->obj);
        psImage = nullptr;
        bImageOwned = false;

        ::free(pasBandParams);
        pasBandParams = nullptr;

        CPLFree(psJP2File);
        psJP2File = nullptr;
    }

    /**
     * @brief Returns false because the Grok driver bypasses block-based I/O.
     *
     * When false, the shared Open code in jp2opjlikedataset.cpp does not
     * set bUseSetDecodeArea, and all reads go through DirectRasterIO
     * instead of the ReadBlock path.
     */
    static bool preferPerBlockDeCompress(void)
    {
        return false;
    }

    /**
     * @brief Sets up decompression
     *
     * @param numThreads number of threads for Grok's thread pool
     * @param nCodeStreamLength code stream length
     * @param nTileW tile width
     * @param nTileH tile height
     * @param numResolutions number of resolutions
     * @return true if successful
     */
    bool setUpDecompress(int numThreads, const char *pszFilename,
                         vsi_l_offset nCodeStreamLength, uint32_t *nTileW,
                         uint32_t *nTileH, int *numResolutions)
    {
        // grk_initialize is called exactly once via std::call_once
        // in init(), so we do not call it here.
        (void)numThreads;  // thread count was resolved in init()
        grk_stream_params streamParams = {};
        streamParams.stream_len = nCodeStreamLength;
        if (GrokCanRead(pszFilename))
        {
            CPLDebug("GROK", "Native I/O for: %s", pszFilename);
            safe_strcpy(streamParams.file, pszFilename);
            streamParams.initial_offset = psJP2File->nBaseOffset;
#if defined(HAVE_CURL) && defined(GRK_HAS_LIBCURL)
            // For /vsis3/ paths, resolve AWS credentials through GDAL's
            // full authentication chain and pass them to Grok so it can
            // handle S3 fetching natively via libcurl.
            if (strncmp(pszFilename, "/vsis3/", 7) == 0)
            {
                auto poHelper = std::unique_ptr<VSIS3HandleHelper>(
                    VSIS3HandleHelper::BuildFromURI(pszFilename + 7, "/vsis3/",
                                                    true));
                if (poHelper)
                {
                    const auto &osAccessKeyId = poHelper->GetAccessKeyId();
                    const auto &osSecretAccessKey =
                        poHelper->GetSecretAccessKey();
                    const auto &osSessionToken = poHelper->GetSessionToken();
                    const auto &osRegion = poHelper->GetRegion();
                    const auto &osEndpoint = poHelper->GetEndpoint();

                    if (!osAccessKeyId.empty())
                        safe_strcpy(streamParams.username, osAccessKeyId);
                    if (!osSecretAccessKey.empty())
                        safe_strcpy(streamParams.password, osSecretAccessKey);
                    if (!osSessionToken.empty())
                        safe_strcpy(streamParams.bearer_token, osSessionToken);
                    if (!osRegion.empty())
                        safe_strcpy(streamParams.region, osRegion);
                    if (!osEndpoint.empty())
                        safe_strcpy(streamParams.s3_endpoint, osEndpoint);
                    streamParams.s3_use_https =
                        poHelper->GetUseHTTPS() ? 1 : -1;
                    streamParams.s3_use_virtual_hosting =
                        poHelper->GetVirtualHosting() ? 1 : -1;
                    streamParams.s3_no_sign_request =
                        poHelper->GetCredentialsSource() ==
                        AWSCredentialsSource::NO_SIGN_REQUEST;
                    streamParams.s3_allow_insecure = CPLTestBool(
                        CPLGetConfigOption("GDAL_HTTP_UNSAFESSL", "NO"));
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Could not resolve AWS credentials "
                             "for %s",
                             pszFilename);
                }
            }
#endif
        }
        else
        {
            CPLDebug("GROK", "VSILFILE callback I/O for: %s", pszFilename);
            streamParams.read_fn = JP2Dataset_Read;
            streamParams.seek_fn = JP2Dataset_Seek;
            streamParams.user_data = psJP2File;
        }
        pCodec = grk_decompress_init(&streamParams, &decompressParams);
        if (pCodec == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "setUpDecompress() failed");
            free();
            return false;
        }
        // read j2k header
        grk_header_info headerInfo;
        memset(&headerInfo, 0, sizeof(headerInfo));
        if (!grk_decompress_read_header(pCodec, &headerInfo))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "setUpDecompress() failed");
            free();
            return false;
        }

        *nTileW = headerInfo.t_width;
        *nTileH = headerInfo.t_height;
#ifdef DEBUG
        uint32_t nX0, nY0;
        uint32_t nTilesX, nTilesY;
        nX0 = headerInfo.tx0;
        nY0 = headerInfo.ty0;
        nTilesX = headerInfo.t_grid_width;
        nTilesY = headerInfo.t_grid_height;
        int mct = headerInfo.mct;
#endif
        *numResolutions = headerInfo.numresolutions;
        irreversible = headerInfo.irreversible;
        psImage = grk_decompress_get_image(pCodec);
        if (psImage == nullptr)
        {
            free();
            return false;
        }
#ifdef DEBUG
        CPLDebug(debugId(), "nX0 = %u", nX0);
        CPLDebug(debugId(), "nY0 = %u", nY0);
        CPLDebug(debugId(), "nTileW = %u", *nTileW);
        CPLDebug(debugId(), "nTileH = %u", *nTileH);
        CPLDebug(debugId(), "nTilesX = %u", nTilesX);
        CPLDebug(debugId(), "nTilesY = %u", nTilesY);
        CPLDebug(debugId(), "mct = %d", mct);
        CPLDebug(debugId(), "psImage->x0 = %u", psImage->x0);
        CPLDebug(debugId(), "psImage->y0 = %u", psImage->y0);
        CPLDebug(debugId(), "psImage->x1 = %u", psImage->x1);
        CPLDebug(debugId(), "psImage->y1 = %u", psImage->y1);
        CPLDebug(debugId(), "psImage->numcomps = %d", psImage->numcomps);
        //CPLDebug(debugId(), "psImage->comps[%d].resno_decoded = %d", i,
        //                 psImage->comps[i].resno_decoded);
        //CPLDebug(debugId(), "psImage->comps[%d].factor = %d", i,
        //                 psImage->comps[i].factor);
        //CPLDebug(debugId(), "psImage->color_space = %d", psImage->color_space);
        CPLDebug(debugId(), "numResolutions = %d", *numResolutions);
        for (uint16_t i = 0; i < psImage->numcomps; i++)
        {
            CPLDebug(debugId(), "psImage->comps[%d].dx = %u", i,
                     psImage->comps[i].dx);
            CPLDebug(debugId(), "psImage->comps[%d].dy = %u", i,
                     psImage->comps[i].dy);
            CPLDebug(debugId(), "psImage->comps[%d].x0 = %u", i,
                     psImage->comps[i].x0);
            CPLDebug(debugId(), "psImage->comps[%d].y0 = %u", i,
                     psImage->comps[i].y0);
            CPLDebug(debugId(), "psImage->comps[%d].w = %u", i,
                     psImage->comps[i].w);
            CPLDebug(debugId(), "psImage->comps[%d].stride = %u", i,
                     psImage->comps[i].stride);
            CPLDebug(debugId(), "psImage->comps[%d].h = %u", i,
                     psImage->comps[i].h);
            CPLDebug(debugId(), "psImage->comps[%d].prec = %d", i,
                     psImage->comps[i].prec);
            CPLDebug(debugId(), "psImage->comps[%d].sgnd = %d", i,
                     psImage->comps[i].sgnd);
        }
#endif
        if (psImage->x1 <= psImage->x0 || psImage->y1 <= psImage->y0 ||
            psImage->numcomps == 0 || (psImage->comps[0].w >> 31) != 0 ||
            (psImage->comps[0].h >> 31) != 0 || (*nTileW >> 31) != 0 ||
            (*nTileH >> 31) != 0 ||
            psImage->comps[0].w != psImage->x1 - psImage->x0 ||
            psImage->comps[0].h != psImage->y1 - psImage->y0)
        {
            CPLDebug(debugId(), "Unable to handle that image (1)");
            free();
            return false;
        }
        return true;
    }

    /**
     * @brief Determines if this driver prefers per-tile compression.
     * Hard-coded to false;
     *
     */
    static bool preferPerTileCompress(void)
    {
        return false;
    }

    /**
     * @brief Initializes compression
     *
     * @param papszOptions options
     * @param adfRates rates
     * @param nBlockXSize block width
     * @param nBlockYSize block height
     * @param bIsIrreversible is compression irreversible
     * @param nNumResolutions number of resolutions
     * @param eProgOrder progression order
     * @param bYCC YCC
     * @param nCblockW code block width
     * @param nCblockH code block height
     * @param bYCBCR420 YCbCr 420
     * @param bProfile1 JPEG 2000 Profile 1
     * @param nBands number of bands
     * @param nXSize image width
     * @param nYSize image height
     * @param eColorSpace colour space
     * @param numThreads number of threads
     * @return true if successful
     */
    bool initCompress(CSLConstList papszOptions,
                      const std::vector<double> &adfRates, int nBlockXSize,
                      int nBlockYSize, bool bIsIrreversible,
                      int nNumResolutions, JP2_PROG_ORDER eProgOrder, int bYCC,
                      int nCblockW, int nCblockH, int bYCBCR420, int bProfile1,
                      int nBands, int nXSize, int nYSize,
                      JP2_COLOR_SPACE eColorSpace, CPL_UNUSED int numThreads)
    {
        int bSOP =
            CPLTestBool(CSLFetchNameValueDef(papszOptions, "SOP", "FALSE"));
        int bEPH =
            CPLTestBool(CSLFetchNameValueDef(papszOptions, "EPH", "FALSE"));

        if (bSOP)
            compressParams.csty |= 0x02;
        if (bEPH)
            compressParams.csty |= 0x04;

        // For truly lossless compression (reversible wavelet with rate
        // 1.0 from QUALITY=100), don't use rate-distortion allocation.
        // A rate of 1.0 means "target 1 byte/pixel compressed", but
        // lossless may need MORE than 1 byte/pixel.  Setting rate to 0
        // tells Grok to keep all data (no rate constraint).
        bool bLossless = !bIsIrreversible && !adfRates.empty() &&
                         adfRates.back() == 1.0 && !bYCBCR420;
        if (bLossless)
        {
            compressParams.numlayers = 1;
            compressParams.layer_rate[0] = 0;
            compressParams.allocation_by_rate_distortion = false;
        }
        else
        {
            compressParams.allocation_by_rate_distortion = true;
            compressParams.numlayers = static_cast<uint16_t>(adfRates.size());
            for (size_t i = 0; i < adfRates.size(); i++)
                compressParams.layer_rate[i] = static_cast<float>(adfRates[i]);
        }
        compressParams.tx0 = 0;
        compressParams.ty0 = 0;
        compressParams.tile_size_on = TRUE;
        compressParams.t_width = nBlockXSize;
        compressParams.t_height = nBlockYSize;
        compressParams.irreversible = bIsIrreversible;
        compressParams.numresolution = nNumResolutions;
        compressParams.prog_order = static_cast<GRK_PROG_ORDER>(eProgOrder);
        compressParams.mct = static_cast<char>(bYCC);
        compressParams.cblockw_init = nCblockW;
        compressParams.cblockh_init = nCblockH;
        compressParams.cblk_sty = 0;

        // osComment is a member so it outlives this function and keeps
        // compressParams.comment[0] valid until grk_compress() is called
        const char *pszCOM = CSLFetchNameValue(papszOptions, "COMMENT");
        if (pszCOM)
        {
            osComment = pszCOM;
            compressParams.num_comments = 1;
            compressParams.comment[0] = &osComment[0];
            compressParams.comment_len[0] =
                static_cast<uint16_t>(osComment.size());
        }
        else if (!bIsIrreversible)
        {
            osComment = getComment();
            if (adfRates.back() == 1.0 && !bYCBCR420)
            {
                osComment += ". LOSSLESS settings used";
            }
            else
            {
                osComment += ". LOSSY settings used";
            }
            compressParams.num_comments = 1;
            compressParams.comment[0] = &osComment[0];
            compressParams.comment_len[0] =
                static_cast<uint16_t>(osComment.size());
        }

        const char *pszCodeBlockStyle =
            CSLFetchNameValue(papszOptions, "CODEBLOCK_STYLE");
        if (pszCodeBlockStyle)
        {
            if (CPLGetValueType(pszCodeBlockStyle) == CPL_VALUE_INTEGER)
            {
                int nVal = atoi(pszCodeBlockStyle);
                if (nVal >= 0 && nVal <= 63)
                {
                    compressParams.cblk_sty = nVal;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Invalid value for CODEBLOCK_STYLE: %s. "
                             "Should be >= 0 and <= 63",
                             pszCodeBlockStyle);
                }
            }
            else
            {
                char **papszTokens =
                    CSLTokenizeString2(pszCodeBlockStyle, ", ", 0);
                for (char **papszIter = papszTokens; papszIter && *papszIter;
                     ++papszIter)
                {
                    if (EQUAL(*papszIter, "BYPASS"))
                    {
                        compressParams.cblk_sty |= (1 << 0);
                    }
                    else if (EQUAL(*papszIter, "RESET"))
                    {
                        compressParams.cblk_sty |= (1 << 1);
                    }
                    else if (EQUAL(*papszIter, "TERMALL"))
                    {
                        compressParams.cblk_sty |= (1 << 2);
                    }
                    else if (EQUAL(*papszIter, "VSC"))
                    {
                        compressParams.cblk_sty |= (1 << 3);
                    }
                    else if (EQUAL(*papszIter, "PREDICTABLE"))
                    {
                        compressParams.cblk_sty |= (1 << 4);
                    }
                    else if (EQUAL(*papszIter, "SEGSYM"))
                    {
                        compressParams.cblk_sty |= (1 << 5);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "Unrecognized option for CODEBLOCK_STYLE: %s",
                                 *papszIter);
                    }
                }
                CSLDestroy(papszTokens);
            }
        }
        /* Add precincts */
        const char *pszPrecincts = CSLFetchNameValueDef(
            papszOptions, "PRECINCTS",
            "{512,512},{256,512},{128,512},{64,512},{32,512},{"
            "16,512},{8,512},{4,512},{2,512}");
        char **papszTokens =
            CSLTokenizeStringComplex(pszPrecincts, "{},", FALSE, FALSE);
        int nPrecincts = CSLCount(papszTokens) / 2;
        for (int i = 0; i < nPrecincts && i < GRK_MAXRLVLS; i++)
        {
            int nPCRW = atoi(papszTokens[2 * i]);
            int nPCRH = atoi(papszTokens[2 * i + 1]);
            if (nPCRW < 1 || nPCRH < 1)
                break;
            compressParams.csty |= 0x01;
            compressParams.res_spec++;
            compressParams.prcw_init[i] = nPCRW;
            compressParams.prch_init[i] = nPCRH;
        }
        CSLDestroy(papszTokens);

        /* Add tileparts setting */
        const char *pszTileParts =
            CSLFetchNameValueDef(papszOptions, "TILEPARTS", "DISABLED");
        if (EQUAL(pszTileParts, "RESOLUTIONS"))
        {
            compressParams.enable_tile_part_generation = true;
            compressParams.new_tile_part_progression_divider = 'R';
        }
        else if (EQUAL(pszTileParts, "LAYERS"))
        {
            if (compressParams.numlayers == 1)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "TILEPARTS=LAYERS has no real interest with single-layer "
                    "codestream");
            }
            compressParams.enable_tile_part_generation = true;
            compressParams.new_tile_part_progression_divider = 'L';
        }
        else if (EQUAL(pszTileParts, "COMPONENTS"))
        {
            compressParams.enable_tile_part_generation = true;
            compressParams.new_tile_part_progression_divider = 'C';
        }
        else if (!EQUAL(pszTileParts, "DISABLED"))
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Invalid value for TILEPARTS");
        }

        if (bProfile1)
        {
            compressParams.rsiz = GRK_PROFILE_1;
        }
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "PLT", "FALSE")))
        {
            compressParams.write_plt = true;
        }
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "TLM", "FALSE")))
        {
            compressParams.write_tlm = true;
        }

        psImage =
            grk_image_new(nBands, pasBandParams,
                          static_cast<GRK_COLOR_SPACE>(eColorSpace), true);
        if (psImage == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "grk_image_new() failed");
            free();
            return false;
        }
        bImageOwned = true;

        psImage->x0 = 0;
        psImage->y0 = 0;
        psImage->x1 = nXSize;
        psImage->y1 = nYSize;
        psImage->color_space = static_cast<GRK_COLOR_SPACE>(eColorSpace);
        psImage->numcomps = nBands;

        /* Default to J2K; setupJP2Metadata() switches to GRK_FMT_JP2
         * when writing JP2 files.  Codec creation is deferred to
         * initCodec() so JP2 metadata can be set first. */
        compressParams.cod_format = GRK_FMT_J2K;

        // Store tile grid info for compressTile
        compressBlockXSize = nBlockXSize;
        compressBlockYSize = nBlockYSize;
        compressImageWidth = nXSize;
        compressImageHeight = nYSize;
        compressNumBands = nBands;
        compressIsYCBCR420 = (bYCBCR420 != 0);

        // Zero all component buffers once.  The Grok library allocates
        // component data without clearing it, and the stride may be wider
        // than the image width due to SIMD alignment.  Uninitialized
        // padding can corrupt the compressed codestream.
        for (int band = 0; band < nBands; band++)
        {
            auto comp = &psImage->comps[band];
            if (comp->data)
                memset(comp->data, 0,
                       static_cast<size_t>(comp->stride) * comp->h *
                           sizeof(int32_t));
        }

        return true;
    }

    /**
     * @brief Extract cdef and palette info from Grok's parsed JP2 boxes.
     */
    void extractJP2BoxInfo(int nBands, int &nRedIndex, int &nGreenIndex,
                           int &nBlueIndex, int &nAlphaIndex,
                           GDALColorTable **ppoCT)
    {
        if (!psImage || !psImage->meta)
            return;

        auto *cdef = psImage->meta->color.channel_definition;
        if (cdef &&
            cdef->num_channel_descriptions == static_cast<uint16_t>(nBands))
        {
            nRedIndex = nGreenIndex = nBlueIndex = -1;
            for (int i = 0; i < nBands; i++)
            {
                int CNi = cdef->descriptions[i].channel;
                int Typi = cdef->descriptions[i].typ;
                int Asoci = cdef->descriptions[i].asoc;
                if (CNi < 0 || CNi >= nBands)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong value of CN%d=%d", i, CNi);
                    break;
                }
                if (Typi == 0)
                {
                    if (Asoci == 1)
                        nRedIndex = CNi;
                    else if (Asoci == 2)
                        nGreenIndex = CNi;
                    else if (Asoci == 3)
                        nBlueIndex = CNi;
                    else if (Asoci < 0 || (Asoci > nBands && Asoci != 65535))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Wrong value of Asoc%d=%d", i, Asoci);
                        break;
                    }
                }
                else if (Typi == 1)
                    nAlphaIndex = CNi;
            }
        }
        else if (cdef)
            CPLDebug(debugId(), "Unsupported cdef content");

        auto *pal = psImage->meta->color.palette;
        if (pal && pal->num_entries <= 256 &&
            (pal->num_channels == 3 || pal->num_channels == 4))
        {
            bool allPrec7 = true;
            for (int c = 0; c < pal->num_channels; c++)
                if (pal->channel_prec[c] != 7)
                {
                    allPrec7 = false;
                    break;
                }
            if (allPrec7)
            {
                *ppoCT = new GDALColorTable();
                for (int i = 0; i < pal->num_entries; i++)
                {
                    GDALColorEntry e;
                    int off = i * pal->num_channels;
                    e.c1 = static_cast<short>(pal->lut[off]);
                    e.c2 = static_cast<short>(pal->lut[off + 1]);
                    e.c3 = static_cast<short>(pal->lut[off + 2]);
                    e.c4 = (pal->num_channels == 4)
                               ? static_cast<short>(pal->lut[off + 3])
                               : 255;
                    (*ppoCT)->SetColorEntry(i, &e);
                }
            }
        }
    }

    /**
     * @brief Set JP2 metadata on the image for Grok-native JP2 writing.
     *
     * Called between initCompress() and initCodec().
     */
    void setupJP2Metadata(bool bInspireTG, int bProfile1, bool bGeoBoxesAfter,
                          GDALJP2Metadata *poJP2MD, GDALJP2Box *poGMLJP2Box,
                          int nAlphaBandIndex, int nRedBandIndex,
                          int nGreenBandIndex, int nBlueBandIndex,
                          JP2_COLOR_SPACE eColorSpace, int nBands,
                          GDALColorTable *poCT, GDALDataset *poSrcDS,
                          CSLConstList papszOptions)
    {
        compressParams.cod_format = GRK_FMT_JP2;

        if (!psImage->meta)
            psImage->meta = grk_image_meta_new();

        /* JPX branding + reader requirements */
        const bool bJPXBranding =
            CPLFetchBool(papszOptions, "JPX", true) && poGMLJP2Box != nullptr;
        if (bJPXBranding)
        {
            compressParams.jpx_branding = true;
            compressParams.write_rreq = true;
            compressParams.num_rreq_standard_features = 0;
            compressParams.rreq_standard_features
                [compressParams.num_rreq_standard_features++] =
                bProfile1 ? 4 : 5;
            compressParams.rreq_standard_features
                [compressParams.num_rreq_standard_features++] = 67;
        }
        if (bInspireTG && poGMLJP2Box != nullptr &&
            !CPLFetchBool(papszOptions, "JPX", true))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "INSPIRE_TG=YES implies GMLJP2 which recommends "
                     "JPX capability");
        }

        compressParams.geoboxes_after_jp2c = bGeoBoxesAfter;

        const bool bWriteMetadata =
            CPLFetchBool(papszOptions, "WRITE_METADATA", false);
        const bool bMainMDOnly =
            CPLFetchBool(papszOptions, "MAIN_MD_DOMAIN_ONLY", false);
        const bool bIPR =
            poSrcDS->GetMetadata("xml:IPR") != nullptr && bWriteMetadata;

        if (bIPR)
        {
            compressParams.rreq_standard_features
                [compressParams.num_rreq_standard_features++] = 35;
        }

        /* GeoTIFF UUID */
        if (poJP2MD)
        {
            std::unique_ptr<GDALJP2Box> poGeoBox(poJP2MD->CreateJP2GeoTIFF());
            if (poGeoBox)
            {
                const GByte *p = poGeoBox->GetWritableData();
                GIntBig n = poGeoBox->GetDataLength();
                if (n > 16) /* skip 16-byte UUID prefix */
                    grk_image_meta_set_field(psImage->meta, "geotiff", p + 16,
                                             static_cast<size_t>(n - 16));
            }
        }

        /* IPR box */
        if (bIPR)
        {
            std::unique_ptr<GDALJP2Box> poIPRBox(
                GDALJP2Metadata::CreateIPRBox(poSrcDS));
            if (poIPRBox)
            {
                const GByte *p = poIPRBox->GetWritableData();
                GIntBig n = poIPRBox->GetDataLength();
                if (n > 0)
                    grk_image_meta_set_field(psImage->meta, "ipr", p,
                                             static_cast<size_t>(n));
            }
        }

        /* GMLJP2 asoc boxes */
        if (poGMLJP2Box)
        {
            const GByte *asocData = poGMLJP2Box->GetWritableData();
            int asocLen = static_cast<int>(poGMLJP2Box->GetDataLength());
            flattenAndSetAsocBoxes(psImage, asocData, asocLen);
        }

        /* XMP UUID */
        if (bWriteMetadata && !bMainMDOnly)
        {
            std::unique_ptr<GDALJP2Box> poXMPBox(
                GDALJP2Metadata::CreateXMPBox(poSrcDS));
            if (poXMPBox)
            {
                const GByte *p = poXMPBox->GetWritableData();
                GIntBig n = poXMPBox->GetDataLength();
                if (n > 16)
                    grk_image_meta_set_field(psImage->meta, "xmp", p + 16,
                                             static_cast<size_t>(n - 16));
            }
        }

        /* GDAL metadata XML box */
        if (bWriteMetadata)
        {
            std::unique_ptr<GDALJP2Box> poMDBox(
                GDALJP2Metadata::CreateGDALMultiDomainMetadataXMLBox(
                    poSrcDS, bMainMDOnly));
            if (poMDBox)
            {
                const GByte *p = poMDBox->GetWritableData();
                GIntBig n = poMDBox->GetDataLength();
                if (n > 0)
                    grk_image_meta_set_field(psImage->meta, "xml", p,
                                             static_cast<size_t>(n));
            }
        }

        /* cdef: component type/association */
        setupCdef(nAlphaBandIndex, nRedBandIndex, nGreenBandIndex,
                  nBlueBandIndex, eColorSpace, nBands, poCT, papszOptions);

        /* Display resolution */
        setupResolution(poSrcDS);
    }

  private:
    void setupCdef(int nAlphaBandIndex, int nRedBandIndex, int nGreenBandIndex,
                   int nBlueBandIndex, JP2_COLOR_SPACE eColorSpace, int nBands,
                   GDALColorTable *poCT, CSLConstList papszOptions)
    {
        bool needCdef =
            (((nBands == 3 || nBands == 4) &&
              (eColorSpace == static_cast<JP2_COLOR_SPACE>(GRK_CLRSPC_SRGB) ||
               eColorSpace == static_cast<JP2_COLOR_SPACE>(GRK_CLRSPC_SYCC)) &&
              (nRedBandIndex != 0 || nGreenBandIndex != 1 ||
               nBlueBandIndex != 2)) ||
             nAlphaBandIndex >= 0);
        if (!needCdef)
            return;

        int nComponents = nBands;
        if (poCT != nullptr)
        {
            int nCTComp =
                atoi(CSLFetchNameValueDef(papszOptions, "CT_COMPONENTS", "0"));
            if (nCTComp == 4)
                nComponents = 4;
        }
        for (int i = 0; i < nComponents; i++)
        {
            if (i != nAlphaBandIndex)
            {
                psImage->comps[i].type = GRK_CHANNEL_TYPE_COLOUR;
                if (eColorSpace ==
                        static_cast<JP2_COLOR_SPACE>(GRK_CLRSPC_GRAY) &&
                    i == 0)
                    psImage->comps[i].association = GRK_CHANNEL_ASSOC_COLOUR_1;
                else if (i == nRedBandIndex)
                    psImage->comps[i].association = GRK_CHANNEL_ASSOC_COLOUR_1;
                else if (i == nGreenBandIndex)
                    psImage->comps[i].association = GRK_CHANNEL_ASSOC_COLOUR_2;
                else if (i == nBlueBandIndex)
                    psImage->comps[i].association = GRK_CHANNEL_ASSOC_COLOUR_3;
            }
            else
            {
                psImage->comps[i].type = GRK_CHANNEL_TYPE_OPACITY;
                psImage->comps[i].association = GRK_CHANNEL_ASSOC_WHOLE_IMAGE;
            }
        }
    }

    void setupResolution(GDALDataset *poSrcDS)
    {
        if (poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION") == nullptr ||
            poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION") == nullptr ||
            poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT") == nullptr)
            return;

        double dfXRes =
            CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION"));
        double dfYRes =
            CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION"));
        int nResUnit = atoi(poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT"));
        if (nResUnit == 2)
        {
            dfXRes = dfXRes * 39.37 / 100.0;
            dfYRes = dfYRes * 39.37 / 100.0;
        }
        if ((nResUnit == 2 || nResUnit == 3) && dfXRes > 0 && dfYRes > 0 &&
            dfXRes < 65535 && dfYRes < 65535)
        {
            compressParams.write_display_resolution = true;
            compressParams.display_resolution[0] = dfXRes * 100.0;
            compressParams.display_resolution[1] = dfYRes * 100.0;
        }
    }

    static void flattenAndSetAsocBoxes(grk_image *image, const GByte *asocData,
                                       int asocDataLen)
    {
        struct AsocEntry
        {
            uint32_t level;
            std::string label;
            std::vector<uint8_t> xml;
        };

        std::vector<AsocEntry> entries;

        std::function<void(const GByte *, int, uint32_t)> flattenAsoc;
        flattenAsoc = [&](const GByte *data, int dataLen, uint32_t level)
        {
            int offset = 0;
            AsocEntry thisEntry;
            thisEntry.level = level;
            std::vector<std::pair<const GByte *, int>> childAsocs;

            while (offset + 8 <= dataLen)
            {
                uint32_t sz = static_cast<uint32_t>(
                    (static_cast<uint32_t>(data[offset]) << 24) |
                    (data[offset + 1] << 16) | (data[offset + 2] << 8) |
                    data[offset + 3]);
                if (sz < 8 || offset + static_cast<int>(sz) > dataLen)
                    break;
                const GByte *boxContent = data + offset + 8;
                uint32_t boxContentLen = sz - 8;

                if (memcmp(data + offset + 4, "lbl ", 4) == 0)
                    thisEntry.label.assign(
                        reinterpret_cast<const char *>(boxContent),
                        boxContentLen);
                else if (memcmp(data + offset + 4, "xml ", 4) == 0)
                    thisEntry.xml.assign(boxContent,
                                         boxContent + boxContentLen);
                else if (memcmp(data + offset + 4, "asoc", 4) == 0)
                    childAsocs.push_back(
                        {boxContent, static_cast<int>(boxContentLen)});
                offset += static_cast<int>(sz);
            }
            entries.push_back(std::move(thisEntry));
            for (auto &child : childAsocs)
                flattenAsoc(child.first, child.second, level + 1);
        };
        flattenAsoc(asocData, asocDataLen, 0);

        if (!entries.empty())
        {
            std::vector<grk_asoc> asocs(entries.size());
            for (size_t i = 0; i < entries.size(); i++)
            {
                asocs[i].level = entries[i].level;
                asocs[i].label = entries[i].label.c_str();
                asocs[i].xml =
                    entries[i].xml.empty() ? nullptr : entries[i].xml.data();
                asocs[i].xml_len = static_cast<uint32_t>(entries[i].xml.size());
            }
            grk_image_meta_set_asocs(image->meta, asocs.data(),
                                     static_cast<uint32_t>(asocs.size()));
        }
    }

  public:
    /**
     * @brief Returns true if the codec uses native file I/O for writing.
     *
     * When true, the VSILFILE passed to open() was closed and should
     * not be used or closed by the caller.
     */
    bool ownsFile() const
    {
        return psJP2File && psJP2File->fp_ == nullptr;
    }

    /**
     * @brief Create the Grok codec after metadata has been set.
     *
     * For local files and /vsis3/, uses native file I/O (closing the
     * VSILFILE).  For other VSI paths, uses VSILFILE callbacks.
     */
    bool initCodec(const char *pszFilename, VSIVirtualHandleUniquePtr &fpOwner)
    {
        grk_stream_params streamParams = {};
        if (pszFilename && GrokCanRead(pszFilename))
        {
            CPLDebug("GROK", "Native file write for: %s", pszFilename);
            safe_strcpy(streamParams.file, pszFilename);
            fpOwner.reset();  // close before Grok opens natively
            if (psJP2File)
                psJP2File->fp_ = nullptr;
        }
        else
        {
            streamParams.seek_fn = JP2Dataset_Seek;
            streamParams.write_fn = JP2Dataset_Write;
            streamParams.user_data = psJP2File;
        }

        pCodec = grk_compress_init(&streamParams, &compressParams, psImage);
        if (pCodec == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "grk_compress_init() failed");
            return false;
        }
        return true;
    }

    /**
     * @brief Shared transcode implementation: copy codestream while
     *        rewriting JP2 metadata boxes from @p poDS.
     *
     * Used by both rewriteBoxes() (in-place rewrite via tmp file) and
     * transcode() (explicit src -> dst with extra cparams set by caller).
     *
     * @param pszSrcFilename source file path
     * @param pszDstFilename destination file path (may equal src only via
     *                       external caller serialization — prefer tmp)
     * @param poDS           dataset carrying metadata for output boxes
     * @param cparams        compression parameters (already configured by
     *                       caller for PLT/TLM/SOP/EPH/progression etc.)
     * @param pszErrorCtx    short context label used in error messages
     * @return true on success
     */
    static bool doTranscode(const char *pszSrcFilename,
                            const char *pszDstFilename, GDALDataset *poDS,
                            grk_cparameters *cparams, const char *pszErrorCtx)
    {
        grk_stream_params srcStreamParams{};
        safe_strcpy(srcStreamParams.file, pszSrcFilename);

        grk_decompress_parameters dparams{};
        auto *decCodec = grk_decompress_init(&srcStreamParams, &dparams);
        if (!decCodec)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "grk_decompress_init() failed for %s", pszErrorCtx);
            return false;
        }

        grk_header_info srcHeader{};
        if (!grk_decompress_read_header(decCodec, &srcHeader))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "grk_decompress_read_header() failed for %s", pszErrorCtx);
            grk_object_unref(decCodec);
            return false;
        }

        auto *srcImage = grk_decompress_get_image(decCodec);
        if (!srcImage)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "grk_decompress_get_image() failed for %s", pszErrorCtx);
            grk_object_unref(decCodec);
            return false;
        }

        if (!srcImage->meta)
            srcImage->meta = grk_image_meta_new();

        /* GeoTIFF UUID */
        GDALJP2Metadata oJP2MD;
        if (poDS->GetGCPCount() > 0)
        {
            oJP2MD.SetGCPs(poDS->GetGCPCount(), poDS->GetGCPs());
            oJP2MD.SetSpatialRef(poDS->GetGCPSpatialRef());
        }
        else
        {
            const OGRSpatialReference *poSRS = poDS->GetSpatialRef();
            if (poSRS)
                oJP2MD.SetSpatialRef(poSRS);
            GDALGeoTransform gt;
            if (poDS->GetGeoTransform(gt) == CE_None &&
                gt != GDALGeoTransform())
                oJP2MD.SetGeoTransform(gt);
        }
        const char *pszAreaOrPoint =
            poDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        oJP2MD.bPixelIsPoint =
            pszAreaOrPoint && EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);

        {
            std::unique_ptr<GDALJP2Box> poGeoBox(oJP2MD.CreateJP2GeoTIFF());
            if (poGeoBox)
            {
                const GByte *p = poGeoBox->GetWritableData();
                GIntBig n = poGeoBox->GetDataLength();
                if (n > 16)
                    grk_image_meta_set_field(srcImage->meta, "geotiff", p + 16,
                                             static_cast<size_t>(n - 16));
            }
        }

        /* IPR box */
        {
            std::unique_ptr<GDALJP2Box> poIPRBox(
                GDALJP2Metadata::CreateIPRBox(poDS));
            if (poIPRBox)
            {
                const GByte *p = poIPRBox->GetWritableData();
                GIntBig n = poIPRBox->GetDataLength();
                if (n > 0)
                    grk_image_meta_set_field(srcImage->meta, "ipr", p,
                                             static_cast<size_t>(n));
            }
        }

        /* XMP UUID */
        {
            std::unique_ptr<GDALJP2Box> poXMPBox(
                GDALJP2Metadata::CreateXMPBox(poDS));
            if (poXMPBox)
            {
                const GByte *p = poXMPBox->GetWritableData();
                GIntBig n = poXMPBox->GetDataLength();
                if (n > 16)
                    grk_image_meta_set_field(srcImage->meta, "xmp", p + 16,
                                             static_cast<size_t>(n - 16));
            }
        }

        /* GDAL metadata XML box */
        {
            std::unique_ptr<GDALJP2Box> poMDBox(
                GDALJP2Metadata::CreateGDALMultiDomainMetadataXMLBox(poDS,
                                                                     false));
            if (poMDBox)
            {
                const GByte *p = poMDBox->GetWritableData();
                GIntBig n = poMDBox->GetDataLength();
                if (n > 0)
                    grk_image_meta_set_field(srcImage->meta, "xml", p,
                                             static_cast<size_t>(n));
            }
        }

        /* GMLJP2 asoc boxes */
        bool bHasSRS = poDS->GetSpatialRef() != nullptr &&
                       !poDS->GetSpatialRef()->IsEmpty();
        GDALGeoTransform gt;
        bool bHasGT =
            poDS->GetGeoTransform(gt) == CE_None && gt != GDALGeoTransform();
        if (bHasSRS && bHasGT && poDS->GetGCPCount() == 0)
        {
            std::unique_ptr<GDALJP2Box> poGMLBox(oJP2MD.CreateGMLJP2(
                poDS->GetRasterXSize(), poDS->GetRasterYSize()));
            if (poGMLBox)
            {
                const GByte *asocData = poGMLBox->GetWritableData();
                int asocLen = static_cast<int>(poGMLBox->GetDataLength());
                flattenAndSetAsocBoxes(srcImage, asocData, asocLen);
            }
        }

        grk_stream_params transSrcParams{};
        safe_strcpy(transSrcParams.file, pszSrcFilename);

        grk_stream_params dstParams{};
        safe_strcpy(dstParams.file, pszDstFilename);

        uint64_t written =
            grk_transcode(&transSrcParams, &dstParams, cparams, srcImage);
        grk_object_unref(decCodec);

        if (written == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "grk_transcode() failed for %s", pszErrorCtx);
            return false;
        }

        return true;
    }

    /**
     * @brief Rewrite JP2 boxes via Grok transcode.
     *
     * Replaces the driver-side box rewriting logic in Close() by using
     * grk_transcode() to copy the codestream verbatim while writing
     * updated metadata boxes.
     *
     * @param pszFilename source (and destination) file path
     * @param poDS        dataset carrying updated metadata
     * @return true on success
     */
    static bool rewriteBoxes(const char *pszFilename, GDALDataset *poDS)
    {
        CPLDebug("GROK", "Rewriting boxes via transcode for %s", pszFilename);

        grk_cparameters cparams{};
        grk_compress_set_default_params(&cparams);
        cparams.cod_format = GRK_FMT_JP2;

        CPLString osTmpFilename(CPLSPrintf("%s.tmp", pszFilename));

        if (!doTranscode(pszFilename, osTmpFilename.c_str(), poDS, &cparams,
                         "box rewrite"))
        {
            VSIUnlink(osTmpFilename);
            return false;
        }

        if (VSIRename(osTmpFilename, pszFilename) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed to rename %s to %s",
                     osTmpFilename.c_str(), pszFilename);
            VSIUnlink(osTmpFilename);
            return false;
        }

        return true;
    }

    /**
     * @brief Transcode a JP2 file without full decompression/recompression.
     *
     * Copies the codestream while optionally inserting PLT/TLM/SOP/EPH
     * markers and/or reordering the progression order.  Metadata boxes
     * (GeoTIFF, XMP, GMLJP2, etc.) are written from @p poDS.
     *
     * @param pszSrcFilename source JP2/J2K file path
     * @param pszDstFilename destination file path
     * @param poDS           dataset carrying metadata for output boxes
     * @param papszOptions   creation options (PLT, TLM, SOP, EPH, PROGRESSION)
     * @return true on success
     */
    static bool transcode(const char *pszSrcFilename,
                          const char *pszDstFilename, GDALDataset *poDS,
                          CSLConstList papszOptions)
    {
        CPLDebug("GROK", "Transcoding %s -> %s", pszSrcFilename,
                 pszDstFilename);

        /* Warn about options that are meaningless in transcode mode */
        static const char *const apszIgnoredOptions[] = {"QUALITY",
                                                         "REVERSIBLE",
                                                         "BLOCKXSIZE",
                                                         "BLOCKYSIZE",
                                                         "RESOLUTIONS",
                                                         "YCBCR420",
                                                         "YCC",
                                                         "NBITS",
                                                         "1BIT_ALPHA",
                                                         "PRECINCTS",
                                                         "TILEPARTS",
                                                         "CODEBLOCK_WIDTH",
                                                         "CODEBLOCK_HEIGHT",
                                                         "CODEBLOCK_STYLE",
                                                         "USE_SRC_CODESTREAM",
                                                         nullptr};
        for (int i = 0; apszIgnoredOptions[i]; i++)
        {
            if (CSLFetchNameValue(papszOptions, apszIgnoredOptions[i]))
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Option %s ignored when TRANSCODE=YES",
                         apszIgnoredOptions[i]);
            }
        }

        grk_cparameters cparams{};
        grk_compress_set_default_params(&cparams);
        cparams.cod_format = GRK_FMT_JP2;

        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "PLT", "FALSE")))
            cparams.write_plt = true;
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "TLM", "FALSE")))
            cparams.write_tlm = true;
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "SOP", "FALSE")))
            cparams.write_sop = true;
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "EPH", "FALSE")))
            cparams.write_eph = true;

        const char *pszProgOrder =
            CSLFetchNameValue(papszOptions, "PROGRESSION");
        if (pszProgOrder)
        {
            if (EQUAL(pszProgOrder, "LRCP"))
                cparams.transcode_prog_order = GRK_LRCP;
            else if (EQUAL(pszProgOrder, "RLCP"))
                cparams.transcode_prog_order = GRK_RLCP;
            else if (EQUAL(pszProgOrder, "RPCL"))
                cparams.transcode_prog_order = GRK_RPCL;
            else if (EQUAL(pszProgOrder, "PCRL"))
                cparams.transcode_prog_order = GRK_PCRL;
            else if (EQUAL(pszProgOrder, "CPRL"))
                cparams.transcode_prog_order = GRK_CPRL;
            else
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Unknown PROGRESSION value for transcode: %s",
                         pszProgOrder);
        }

        return doTranscode(pszSrcFilename, pszDstFilename, poDS, &cparams,
                           "transcode");
    }

    /**
     * @brief Copies tile pixel data into the Grok image component buffers.
     *
     * Grok compresses the whole image in one shot, so this method
     * accumulates tile data into psImage->comps[band].data.  The actual
     * compression happens in finishCompress().
     *
     * For YCBCR420, the incoming buffer is packed as:
     *   Y  (W*H) | Cb (W/2*H/2) | Cr (W/2*H/2) [| Alpha (W*H)]
     * and the Cb/Cr components have dx=dy=2 (half resolution).
     *
     * For normal BSQ, the buffer is band-sequential with equal-sized planes.
     *
     * @param tileIndex tile index (row-major order)
     * @param buff pixel buffer
     * @param buffLen buffer length in bytes
     * @return true if successful
     */
    bool compressTile(int tileIndex, GByte *buff, uint32_t buffLen)
    {
        (void)buffLen;
        if (!psImage || !buff)
            return false;

        const int nTilesX =
            (compressImageWidth + compressBlockXSize - 1) / compressBlockXSize;
        const int tileRow = tileIndex / nTilesX;
        const int tileCol = tileIndex % nTilesX;

        const int tileXOff = tileCol * compressBlockXSize;
        const int tileYOff = tileRow * compressBlockYSize;
        const int nWidthToRead =
            std::min(compressBlockXSize, compressImageWidth - tileXOff);
        const int nHeightToRead =
            std::min(compressBlockYSize, compressImageHeight - tileYOff);

        const int nBands = compressNumBands;
        // Derive data type size from first band's precision
        const int nDataTypeSize = (psImage->comps[0].prec + 7) / 8;

        if (compressIsYCBCR420)
        {
            return compressTileYCBCR420(buff, tileXOff, tileYOff, nWidthToRead,
                                        nHeightToRead);
        }

        const int tilePixels = nWidthToRead * nHeightToRead;

        for (int band = 0; band < nBands; band++)
        {
            auto comp = &psImage->comps[band];
            auto destData = static_cast<int32_t *>(comp->data);
            if (!destData)
                return false;

            const GByte *srcBand =
                buff + static_cast<size_t>(band) * tilePixels * nDataTypeSize;

            for (int y = 0; y < nHeightToRead; y++)
            {
                const int imgY = tileYOff + y;
                for (int x = 0; x < nWidthToRead; x++)
                {
                    const int imgX = tileXOff + x;
                    const int srcIdx = y * nWidthToRead + x;
                    const size_t dstIdx =
                        static_cast<size_t>(imgY) * comp->stride + imgX;

                    int32_t value = 0;
                    switch (nDataTypeSize)
                    {
                        case 1:
                            value = srcBand[srcIdx];
                            break;
                        case 2:
                            if (comp->sgnd)
                                value = reinterpret_cast<const GInt16 *>(
                                    srcBand)[srcIdx];
                            else
                                value = reinterpret_cast<const GUInt16 *>(
                                    srcBand)[srcIdx];
                            break;
                        case 4:
                            if (comp->sgnd)
                                value = reinterpret_cast<const GInt32 *>(
                                    srcBand)[srcIdx];
                            else
                                value = static_cast<int32_t>(
                                    reinterpret_cast<const GUInt32 *>(
                                        srcBand)[srcIdx]);
                            break;
                    }
                    destData[dstIdx] = value;
                }
            }
        }

        return true;
    }

    /**
     * @brief Handles YCBCR420 tile data.
     *
     * Buffer layout: Y (W*H) | Cb (W/2*H/2) | Cr (W/2*H/2) [| Alpha (W*H)]
     * Components 1,2 (Cb,Cr) have dx=dy=2 so their image dimensions
     * are half of band 0 (Y).
     */
    bool compressTileYCBCR420(const GByte *buff, int tileXOff, int tileYOff,
                              int nWidthToRead, int nHeightToRead)
    {
        const int tilePixels = nWidthToRead * nHeightToRead;
        const int halfW = nWidthToRead / 2;
        const int halfH = nHeightToRead / 2;
        const int halfPixels = halfW * halfH;

        // Y plane
        {
            auto comp = &psImage->comps[0];
            auto dest = static_cast<int32_t *>(comp->data);
            if (!dest)
                return false;
            const GByte *src = buff;
            for (int y = 0; y < nHeightToRead; y++)
            {
                const int imgY = tileYOff + y;
                for (int x = 0; x < nWidthToRead; x++)
                {
                    dest[static_cast<size_t>(imgY) * comp->stride + tileXOff +
                         x] = src[y * nWidthToRead + x];
                }
            }
        }

        // Cb plane (half resolution)
        {
            auto comp = &psImage->comps[1];
            auto dest = static_cast<int32_t *>(comp->data);
            if (!dest)
                return false;
            const GByte *src = buff + tilePixels;
            for (int y = 0; y < halfH; y++)
            {
                const int imgY = tileYOff / 2 + y;
                for (int x = 0; x < halfW; x++)
                {
                    dest[static_cast<size_t>(imgY) * comp->stride +
                         tileXOff / 2 + x] = src[y * halfW + x];
                }
            }
        }

        // Cr plane (half resolution)
        {
            auto comp = &psImage->comps[2];
            auto dest = static_cast<int32_t *>(comp->data);
            if (!dest)
                return false;
            const GByte *src = buff + tilePixels + halfPixels;
            for (int y = 0; y < halfH; y++)
            {
                const int imgY = tileYOff / 2 + y;
                for (int x = 0; x < halfW; x++)
                {
                    dest[static_cast<size_t>(imgY) * comp->stride +
                         tileXOff / 2 + x] = src[y * halfW + x];
                }
            }
        }

        // Alpha plane (full resolution, if present)
        if (compressNumBands == 4)
        {
            auto comp = &psImage->comps[3];
            auto dest = static_cast<int32_t *>(comp->data);
            if (!dest)
                return false;
            const GByte *src = buff + tilePixels + 2 * halfPixels;
            for (int y = 0; y < nHeightToRead; y++)
            {
                const int imgY = tileYOff + y;
                for (int x = 0; x < nWidthToRead; x++)
                {
                    dest[static_cast<size_t>(imgY) * comp->stride + tileXOff +
                         x] = src[y * nWidthToRead + x];
                }
            }
        }

        return true;
    }

    /**
     * @brief Finishes compression by calling grk_compress for the whole image.
     *
     * @return true if successful
     */
    bool finishCompress(void)
    {
        bool rc = false;
        if (pCodec)
            rc = grk_compress(pCodec, nullptr) != 0;
        if (!rc)
            CPLError(CE_Failure, CPLE_AppDefined, "grk_compress() failed");
        free();
        return rc;
    }

    void cleanUpDecompress(void)
    {
        free();
    }

    grk_decompress_parameters decompressParams{};  ///< Decompress config
    grk_cparameters compressParams{};              ///< Compress config
    jp2_codec *pCodec = nullptr;   ///< Grok codec handle (owned)
    jp2_image *psImage = nullptr;  ///< Decoded/encoded image
    bool bImageOwned =
        false;  ///< True when psImage was created by grk_image_new
    grk_image_comp *pasBandParams = nullptr;  ///< Band params for compression
    JP2File *psJP2File = nullptr;  ///< VSILFILE wrapper for stream callbacks
    std::string osComment;  ///< COM marker text; must outlive compressParams

    // Tile grid dimensions stored by initCompress for use in compressTile
    int compressBlockXSize = 0;
    int compressBlockYSize = 0;
    int compressImageWidth = 0;
    int compressImageHeight = 0;
    int compressNumBands = 0;
    bool compressIsYCBCR420 = false;

    // True if image uses irreversible (9/7) DWT — decoded data is float
    bool irreversible = false;
};

/**
 * @struct JP2GRKDatasetBase
 * @brief Grok dataset base class implementing DirectRasterIO-based reading.
 *
 * ARCHITECTURE:
 * canPerformDirectIO() returns true, so ALL reads bypass the block-based
 * ReadBlock path and go through:
 *   Band::IRasterIO -> Dataset::DirectRasterIO -> decompressAsynch -> CopyTiles
 *
 * decompressAsynch() calls grk_decompress() once (initializedAsync flag),
 * then caches the PostPreload result.  Subsequent reads reuse the cached
 * tile grid info with tile coordinates recomputed per swath.
 *
 * OVERVIEW HANDLING:
 * Internal JPEG2000 resolution-level overviews are created during Open()
 * by the shared code in jp2opjlikedataset.cpp.  Each overview dataset
 * lazily creates its own GRKCodecWrapper on first read (in
 * decompressAsynch()), configured with core.reduce = iLevel to decode
 * at the appropriate resolution.
 *
 * JP2 FORMAT DETECTION:
 * JP2FindCodeStream() returns nCodeStreamStart=0 when canPerformDirectIO()
 * is true (to let Grok read the full JP2 file from offset 0).  Format
 * detection in Open() uses the file header signature instead of
 * nCodeStreamStart to correctly identify JP2 vs J2K format.  This ensures
 * JP2 boxes (cdef, pclr) are parsed for alpha/color table detection.
 *
 * 1-BIT ALPHA PROMOTION:
 * When a JP2 file has an alpha band at 1-bit precision, the cdef box
 * identifies it during Open().  bHas1BitAlpha (on JP2DatasetBase) is set,
 * and DirectRasterIO passes the alpha band's 0-based index to CopyTileData
 * which multiplies decoded 0/1 values by 255.
 */
struct JP2GRKDatasetBase : public JP2DatasetBase
{
    typedef grk_image jp2_image;
    typedef grk_image_comp jp2_image_comp;
    typedef grk_object jp2_codec;
    typedef void (*decompress_callback)(void *codec, uint16_t tile_index,
                                        jp2_image *tile_image,
                                        uint8_t reduction, void *user_data);

    JP2_COLOR_SPACE eColorSpace = GRKCodecWrapper::cvtenum(JP2_CLRSPC_UNKNOWN);
    GRKCodecWrapper *m_codec =
        nullptr;  ///< Grok codec (owned, may be lazy-init for overviews)
    int *m_pnLastLevel = nullptr;   ///< Shared across main + overview datasets
    bool m_bStrict = false;         ///< Strict decoding mode (unused for Grok)
    bool fullDecompress_ = false;   ///< True if full-image decode requested
    uint32_t area_x0_ = 0;          ///< AdviseRead region left
    uint32_t area_y0_ = 0;          ///< AdviseRead region top
    uint32_t area_x1_ = 0;          ///< AdviseRead region right
    uint32_t area_y1_ = 0;          ///< AdviseRead region bottom
    bool initializedAsync = false;  ///< True after first grk_decompress() call
    PostPreload cachedPostPreload_{};    ///< Cached first-call tile grid info
    bool hasCachedPostPreload_ = false;  ///< True after first decompressAsynch

    /**
     * @brief Destroys the JP2GRKDatasetBase object
     * 
     */
    virtual ~JP2GRKDatasetBase();

    /**
     * @brief Initializes Grok library and sets up message handlers.
     *
     * The GRK_DEBUG environment variable controls verbosity:
     *   0   = no callbacks (silent)
     *   1   = errors only
     *   2   = errors + warnings
     *   3   = errors + warnings + info
     *   4+  = all (including debug)
     * Unset = errors + warnings + info (default)
     */
    void init(void)
    {
        // Initialize Grok's global thread pool and message handlers
        // exactly once per process using std::call_once.  The thread
        // count is resolved from GDAL_NUM_THREADS on the very first
        // call and never changed afterwards.  This avoids the
        // create-destroy-recreate cycle that caused stale executor
        // state (orphaned task graphs, corrupted wavelet buffers).
        static std::once_flag grokInitFlag;
        std::call_once(
            grokInitFlag,
            []()
            {
                grk_initialize(
                    nullptr,
                    GDALGetNumThreads(GDAL_DEFAULT_MAX_THREAD_COUNT, true),
                    nullptr);

                grk_msg_handlers handlers = {};
                const char *debug_env = std::getenv("GRK_DEBUG");
                if (debug_env)
                {
                    int level = std::atoi(debug_env);
                    if (level >= 1)
                        handlers.error_callback = JP2_ErrorCallback;
                    if (level >= 2)
                        handlers.warn_callback = JP2_WarningCallback;
                    if (level >= 3)
                        handlers.info_callback = JP2_InfoCallback;
                    if (level >= 4)
                        handlers.debug_callback = JP2_DebugCallback;
                }
                else
                {
                    handlers.info_callback = JP2_InfoCallback;
                    handlers.warn_callback = JP2_WarningCallback;
                    handlers.error_callback = JP2_ErrorCallback;
                }
                grk_set_msg_handlers(handlers);
            });
    }

    /**
     * @brief Hint that a particular region will be read next.
     *
     * Stores the region so decompressAsynch() can use it instead of
     * the per-IRasterIO swath coordinates.  Also resets initializedAsync
     * so a fresh grk_decompress() is issued for the new region.
     */
    CPLErr AdviseRead([[maybe_unused]] int nXOff, [[maybe_unused]] int nYOff,
                      [[maybe_unused]] int nXSize, [[maybe_unused]] int nYSize,
                      [[maybe_unused]] int nBufXSize,
                      [[maybe_unused]] int nBufYSize,
                      [[maybe_unused]] GDALDataType eDT,
                      [[maybe_unused]] int nBandCount,
                      [[maybe_unused]] int *panBandList,
                      [[maybe_unused]] CSLConstList papszOptions)
    {
        area_x0_ = nXOff;
        area_y0_ = nYOff;
        area_x1_ = nXOff + nXSize;
        area_y1_ = nYOff + nYSize;
        fullDecompress_ = (nXOff == m_nX0 && nYOff == m_nY0 &&
                           nXSize == nParentXSize && nYSize == nParentYSize);
        initializedAsync = false;

        return CE_None;
    }

    /**
     * @brief Returns true: the Grok driver always uses DirectRasterIO.
     *
     * This causes all reads to bypass the block/ReadBlock path and go
     * through DirectRasterIO -> decompressAsynch -> CopyTiles.
     * It also affects JP2FindCodeStream() which returns nCodeStreamStart=0
     * for JP2 files (so Grok reads the full JP2 from the start), and
     * prevents internal overview creation in the shared Open code.
     */
    static bool canPerformDirectIO(void)
    {
        return true;
    }

    // Simple thread pool implementation
    class ThreadPool
    {
      public:
        explicit ThreadPool(size_t numThreads)
        {
            for (size_t i = 0; i < numThreads; ++i)
            {
                workers.emplace_back(
                    [this]
                    {
                        while (true)
                        {
                            std::function<void()> task;
                            {
                                std::unique_lock<std::mutex> lock(queueMutex);
                                condition.wait(
                                    lock,
                                    [this] { return stop || !tasks.empty(); });
                                if (stop && tasks.empty())
                                    return;
                                task = std::move(tasks.front());
                                tasks.pop();
                            }
                            task();
                        }
                    });
            }
        }

        ~ThreadPool()
        {
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                stop = true;
            }
            condition.notify_all();
            for (std::thread &worker : workers)
            {
                if (worker.joinable())
                    worker.join();
            }
        }

        void enqueue(std::function<void()> task)
        {
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                if (stop)
                    return;
                tasks.emplace(std::move(task));
            }
            condition.notify_one();
        }

      private:
        std::vector<std::thread> workers{};
        std::queue<std::function<void()>> tasks{};
        std::mutex queueMutex{};
        std::condition_variable condition{};
        bool stop = false;
    };

    /**
     * @brief Scalar fallback: copies decoded tile data for subsampled components.
     *
     * Only called when one or more components have dx/dy != 1 (e.g. YCBCR420).
     * For non-subsampled components, CopyTiles delegates to
     * grk_decompress_schedule_swath_copy() for SIMD-accelerated copying.
     */
    static CPLErr CopyTileData(grk_image *img, int nXOff, int nYOff, int nXSize,
                               int nYSize, void *pData, int /*nBufXSize*/,
                               int /*nBufYSize*/, GDALDataType eBufType,
                               int nBandCount, const int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace, int nPromoteAlphaBandIdx)
    {
        // Get tile position and size from component 0 (full resolution)
        const int tileXOff = img->x0;
        const int tileYOff = img->y0;
        const int tileWidth = static_cast<int>(img->comps[0].w);
        const int tileHeight = static_cast<int>(img->comps[0].h);

        // Compute overlap between tile and requested window
        const int xStart = std::max(nXOff, tileXOff);
        const int xEnd = std::min(nXOff + nXSize, tileXOff + tileWidth);
        const int yStart = std::max(nYOff, tileYOff);
        const int yEnd = std::min(nYOff + nYSize, tileYOff + tileHeight);

        if (xStart >= xEnd || yStart >= yEnd)
            return CE_None;

        // can use fast path when there is no sub-sampling, no alpha promotion
        // and output pixels are contiguous
        const int elemSize = GDALGetDataTypeSizeBytes(eBufType);
        bool canFastCopy = (nPromoteAlphaBandIdx < 0 &&
                            nPixelSpace == elemSize && elemSize > 0);
        for (int i = 0; i < nBandCount && canFastCopy; ++i)
        {
            const int b = panBandMap[i] - 1;
            if (b < 0 || b >= img->numcomps)
                canFastCopy = false;
            else
            {
                const auto &comp = img->comps[b];
                if (comp.dx != 1 || comp.dy != 1 || !comp.data)
                    canFastCopy = false;
            }
        }

        if (canFastCopy)
        {
            const int copyWidth = xEnd - xStart;
            for (int i = 0; i < nBandCount; ++i)
            {
                const int srcBandIdx = panBandMap[i] - 1;
                const auto &comp = img->comps[srcBandIdx];
                const int srcXStart = xStart - tileXOff;

                for (int iY = yStart; iY < yEnd; ++iY)
                {
                    const int srcRow = iY - tileYOff;
                    const int dstRow = iY - nYOff;
                    const int dstX = xStart - nXOff;
                    auto dst = static_cast<uint8_t *>(pData) +
                               dstRow * nLineSpace + dstX * nPixelSpace +
                               i * nBandSpace;

                    if (comp.data_type == GRK_INT_16)
                    {
                        const auto src =
                            static_cast<int16_t *>(comp.data) +
                            srcRow * static_cast<int>(comp.stride) + srcXStart;
                        if (eBufType == GDT_UInt16 || eBufType == GDT_Int16)
                        {
                            memcpy(dst, src, copyWidth * sizeof(int16_t));
                        }
                        else if (eBufType == GDT_Byte)
                        {
                            for (int x = 0; x < copyWidth; ++x)
                                dst[x] = static_cast<GByte>(src[x]);
                        }
                        else if (eBufType == GDT_Int32 ||
                                 eBufType == GDT_UInt32)
                        {
                            auto dst32 = reinterpret_cast<int32_t *>(dst);
                            for (int x = 0; x < copyWidth; ++x)
                                dst32[x] = src[x];
                        }
                    }
                    else
                    {
                        const auto src =
                            static_cast<int32_t *>(comp.data) +
                            srcRow * static_cast<int>(comp.stride) + srcXStart;
                        if (eBufType == GDT_Int32 || eBufType == GDT_UInt32)
                        {
                            memcpy(dst, src, copyWidth * sizeof(int32_t));
                        }
                        else if (eBufType == GDT_UInt16 ||
                                 eBufType == GDT_Int16)
                        {
                            auto dst16 = reinterpret_cast<int16_t *>(dst);
                            for (int x = 0; x < copyWidth; ++x)
                                dst16[x] = static_cast<int16_t>(src[x]);
                        }
                        else if (eBufType == GDT_Byte)
                        {
                            for (int x = 0; x < copyWidth; ++x)
                                dst[x] = static_cast<GByte>(src[x]);
                        }
                    }
                }
            }
            return CE_None;
        }

        // Scalar per-pixel loop that handles dx/dy subsampling, alpha promotion,
        // and all five output types.
        CPLErr eErr = CE_None;
        for (int iY = yStart; iY < yEnd; ++iY)
        {
            for (int iX = xStart; iX < xEnd; ++iX)
            {
                for (int i = 0; i < nBandCount; ++i)
                {
                    const int srcBandIdx = panBandMap[i] - 1;
                    if (srcBandIdx >= img->numcomps)
                        continue;

                    const auto &comp = img->comps[srcBandIdx];
                    if (!comp.data)
                    {
                        eErr = CE_Failure;
                        break;
                    }

                    int tileX = (iX - tileXOff) / comp.dx;
                    int tileY = (iY - tileYOff) / comp.dy;
                    if (tileX >= static_cast<int>(comp.w))
                        tileX = comp.w - 1;
                    if (tileY >= static_cast<int>(comp.h))
                        tileY = comp.h - 1;
                    const int tileIdx =
                        tileY * static_cast<int>(comp.stride) + tileX;

                    const int bufX = iX - nXOff;
                    const int bufY = iY - nYOff;
                    const GPtrDiff_t dstOffset =
                        bufX * nPixelSpace + bufY * nLineSpace + i * nBandSpace;

                    // Grok v20.3.x can use 16 bit storage for images with
                    // precision ≤ 12 bits - we need to  cast to correct type
                    int32_t value;
                    if (comp.data_type == GRK_INT_16)
                        value = static_cast<int16_t *>(comp.data)[tileIdx];
                    else
                        value = static_cast<int32_t *>(comp.data)[tileIdx];
                    if (srcBandIdx == nPromoteAlphaBandIdx)
                        value *= 255;
                    if (eBufType == GDT_Byte)
                    {
                        static_cast<GByte *>(pData)[dstOffset] =
                            static_cast<GByte>(value);
                    }
                    else if (eBufType == GDT_Int16)
                    {
                        static_cast<GInt16 *>(pData)[dstOffset / 2] =
                            static_cast<GInt16>(value);
                    }
                    else if (eBufType == GDT_UInt16)
                    {
                        static_cast<GUInt16 *>(pData)[dstOffset / 2] =
                            static_cast<GUInt16>(value);
                    }
                    else if (eBufType == GDT_Int32)
                    {
                        static_cast<GInt32 *>(pData)[dstOffset / 4] = value;
                    }
                    else if (eBufType == GDT_UInt32)
                    {
                        static_cast<GUInt32 *>(pData)[dstOffset / 4] =
                            static_cast<GUInt32>(value);
                    }
                }
            }
        }

        return eErr;
    }

    /**
     * @brief Dispatches tile copy to the output buffer.
     *
     * Fast path (non-subsampled components): delegates to
     * grk_decompress_schedule_swath_copy() which uses Grok's existing
     * Taskflow executor to copy tiles in parallel — no extra thread pool.
     *
     * Slow path (any component with dx/dy != 1, e.g. YCBCR420): creates
     * a GDAL ThreadPool and calls CopyTileData() per tile (scalar loop
     * that handles chroma upsampling).
     *
     * NOTE: grk_decompress_get_tile_image is serialized in the slow path
     * only — the fast path uses Grok's internal tile cache directly.
     */
    CPLErr CopyTiles(PostPreload &postPreload, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount,
                     const int *panBandMap, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     int nPromoteAlphaBandIdx)
    {
        // ---- Upfront subsampling check ----
        // If any requested component has dx/dy != 1, fall back to the
        // scalar slow path; otherwise use the Taskflow-based fast path.
        // The fast path is only used for multi-tile images because
        // single-tile images require grk_decompress_get_image() to
        // trigger post-processing (wait(nullptr) -> postMulti_).
        const int nTotalTiles = (postPreload.tile_x1 - postPreload.tile_x0) *
                                (postPreload.tile_y1 - postPreload.tile_y0);
        bool canUseFastPath = (nTotalTiles > 1 && m_codec && m_codec->psImage);
        if (canUseFastPath)
        {
            for (int i = 0; i < nBandCount && canUseFastPath; ++i)
            {
                const int b = panBandMap[i] - 1;
                if (b < 0 ||
                    b >= static_cast<int>(m_codec->psImage->numcomps) ||
                    m_codec->psImage->comps[b].dx != 1 ||
                    m_codec->psImage->comps[b].dy != 1)
                    canUseFastPath = false;
            }
        }

        if (canUseFastPath)
        {
            // ---- Fast path: delegate to Grok Taskflow executor ----
            // grk_decompress_schedule_swath_copy enqueues one Taskflow task
            // per tile and grk_decompress_wait_swath_copy waits for them.
            // No extra thread pool is created.
            uint8_t prec = 8;
            bool sgnd = false;
            if (eBufType == GDT_Int16)
            {
                prec = 16;
                sgnd = true;
            }
            else if (eBufType == GDT_UInt16)
            {
                prec = 16;
            }
            else if (eBufType == GDT_Int32)
            {
                prec = 32;
                sgnd = true;
            }
            else if (eBufType == GDT_UInt32)
            {
                prec = 32;
            }

            grk_wait_swath waitSwath = {};
            waitSwath.tile_x0 = postPreload.tile_x0;
            waitSwath.tile_y0 = postPreload.tile_y0;
            waitSwath.tile_x1 = postPreload.tile_x1;
            waitSwath.tile_y1 = postPreload.tile_y1;
            waitSwath.num_tile_cols = postPreload.num_tile_cols;

            grk_swath_buffer buf = {};
            buf.data = pData;
            buf.prec = prec;
            buf.sgnd = sgnd;
            buf.numcomps = static_cast<uint16_t>(nBandCount);
            buf.band_map = const_cast<int *>(panBandMap);
            buf.pixel_space = static_cast<int64_t>(nPixelSpace);
            buf.line_space = static_cast<int64_t>(nLineSpace);
            buf.band_space = static_cast<int64_t>(nBandSpace);
            buf.promote_alpha = nPromoteAlphaBandIdx;
            buf.x0 = static_cast<uint32_t>(nXOff);
            buf.y0 = static_cast<uint32_t>(nYOff);
            buf.x1 = static_cast<uint32_t>(nXOff + nXSize);
            buf.y1 = static_cast<uint32_t>(nYOff + nYSize);

            grk_decompress_schedule_swath_copy(m_codec->pCodec, &waitSwath,
                                               &buf);
            grk_decompress_wait_swath_copy(m_codec->pCodec);
            return CE_None;
        }

        // ---- Slow path: subsampled components or single tile ----
        CPLErr eErr = CE_None;

        // Helper lambda: get tile image, handling single-tile fallback
        auto getTileImage = [&](uint16_t tileno) -> grk_image *
        {
            grk_image *img =
                grk_decompress_get_tile_image(m_codec->pCodec, tileno, true);
            if (!img)
                img = grk_decompress_get_image(m_codec->pCodec);
            return img;
        };

        if (nTotalTiles <= 1)
        {
            // Single tile: no thread pool overhead
            uint16_t tileno = postPreload.tile_y0 * postPreload.num_tile_cols +
                              postPreload.tile_x0;
            grk_image *img = getTileImage(tileno);
            if (!img)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "grk_decompress_get_tile_image() returned "
                         "NULL for tile %d",
                         tileno);
                return CE_Failure;
            }
            return CopyTileData(img, nXOff, nYOff, nXSize, nYSize, pData,
                                nBufXSize, nBufYSize, eBufType, nBandCount,
                                panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                                nPromoteAlphaBandIdx);
        }

        // Multi-tile: use thread pool for parallel processing
        std::atomic<bool> errorOccurred(false);
        size_t numThreads = static_cast<size_t>(
            GDALGetNumThreads(/* nMaxVal = */ -1,
                              /* bDefaultAllCPUs = */ true));
        ThreadPool pool(numThreads);
        std::mutex codecMutex;

        for (uint16_t ty = postPreload.tile_y0; ty < postPreload.tile_y1; ++ty)
        {
            for (uint16_t tx = postPreload.tile_x0; tx < postPreload.tile_x1;
                 ++tx)
            {
                uint16_t tileno = ty * postPreload.num_tile_cols + tx;
                pool.enqueue(
                    [&, tileno]
                    {
                        if (errorOccurred)
                            return;

                        grk_image *img = nullptr;
                        {
                            std::lock_guard<std::mutex> lock(codecMutex);
                            img = grk_decompress_get_tile_image(m_codec->pCodec,
                                                                tileno, true);
                            if (!img)
                                img = grk_decompress_get_image(m_codec->pCodec);
                        }
                        if (!img)
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "grk_decompress_get_tile_image() returned "
                                     "NULL for tile %d",
                                     tileno);
                            errorOccurred = true;
                            return;
                        }
                        CPLErr tileErr = CopyTileData(
                            img, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                            nBufYSize, eBufType, nBandCount, panBandMap,
                            nPixelSpace, nLineSpace, nBandSpace,
                            nPromoteAlphaBandIdx);
                        if (tileErr != CE_None)
                            errorOccurred = true;
                    });
            }
        }

        // ThreadPool destructor waits for all tasks to complete.
        if (errorOccurred)
            eErr = CE_Failure;

        return eErr;
    }

    /**
     * @brief Main read entry point for the Grok driver.
     *
     * Called from JP2OPJLikeRasterBand::IRasterIO and
     * JP2OPJLikeDataset::IRasterIO when canPerformDirectIO() is true.
     * Triggers decompression (or uses cached results) and copies tile
     * data to the output buffer.
     */
    CPLErr DirectRasterIO(GDALRWFlag /* eRWFlag */, int nXOff, int nYOff,
                          int nXSize, int nYSize, void *pData, int nBufXSize,
                          int nBufYSize, GDALDataType eBufType, int nBandCount,
                          const int *panBandMap, GSpacing nPixelSpace,
                          GSpacing nLineSpace, GSpacing nBandSpace,
                          [[maybe_unused]] GDALRasterIOExtraArg *psExtraArg)
    {
        // 0-based component index to promote from 1-bit to 8-bit, or -1
        int nPromoteAlphaBandIdx = this->bHas1BitAlpha ? this->nAlphaIndex : -1;
        // Validate buffer data type
        if (eBufType != GDT_Byte && eBufType != GDT_Int16 &&
            eBufType != GDT_UInt16 && eBufType != GDT_Int32 &&
            eBufType != GDT_UInt32)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "DirectRasterIO: unsupported buffer data type %s",
                     GDALGetDataTypeName(eBufType));
            return CE_Failure;
        }

        CPLErr eErr = CE_None;
        const bool bNeedResample = (nBufXSize != nXSize || nBufYSize != nYSize);

        // Set up copy target: use a temp buffer when resampling,
        // otherwise write directly to the caller's buffer.
        const int nDTSize = GDALGetDataTypeSizeBytes(eBufType);
        GByte *pTmp = nullptr;
        void *pCopyTarget = pData;
        GSpacing cpPixel = nPixelSpace;
        GSpacing cpLine = nLineSpace;
        GSpacing cpBand = nBandSpace;
        int cpBufXSize = nBufXSize;
        int cpBufYSize = nBufYSize;

        if (bNeedResample)
        {
            const size_t nTmpBufSize =
                static_cast<size_t>(nXSize) * nYSize * nBandCount * nDTSize;
            pTmp = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nTmpBufSize));
            if (!pTmp)
                return CE_Failure;
            memset(pTmp, 0, nTmpBufSize);
            pCopyTarget = pTmp;
            cpPixel = static_cast<GSpacing>(nDTSize);
            cpLine = static_cast<GSpacing>(nXSize) * nDTSize;
            cpBand = static_cast<GSpacing>(nXSize) * nYSize * nDTSize;
            cpBufXSize = nXSize;
            cpBufYSize = nYSize;
        }

        // Row copy callback: copies one tile-row's decoded data to
        // the output buffer.  Called from decompressAsynch's row-by-row
        // wait loop so tiles are consumed before the next wait releases them.
        auto rowCopy = [&](PostPreload &rowPost) -> CPLErr
        {
            return CopyTiles(rowPost, nXOff, nYOff, nXSize, nYSize, pCopyTarget,
                             cpBufXSize, cpBufYSize, eBufType, nBandCount,
                             panBandMap, cpPixel, cpLine, cpBand,
                             nPromoteAlphaBandIdx);
        };

        // When reading a subset of bands, tile images must persist
        // across per-band reads.  Use CACHE_IMAGE so that tile row
        // release keeps the decoded pixels alive.
        const int nTotalComps =
            (m_codec && m_codec->psImage) ? m_codec->psImage->numcomps : 0;
        const bool needPersistentTiles =
            (!this->bSingleTiled && nBandCount < nTotalComps);

        auto postPreload =
            decompressAsynch(nullptr, nullptr, nXOff, nYOff, nXSize, nYSize,
                             rowCopy, needPersistentTiles);

        try
        {
            // If row-by-row copy was NOT done (single-tile or cached path),
            // copy all tiles now.
            if (!postPreload.rowCopyDone_ && postPreload.asynch_)
            {
                eErr = CopyTiles(postPreload, nXOff, nYOff, nXSize, nYSize,
                                 pCopyTarget, cpBufXSize, cpBufYSize, eBufType,
                                 nBandCount, panBandMap, cpPixel, cpLine,
                                 cpBand, nPromoteAlphaBandIdx);
            }
            else if (!postPreload.asynch_)
            {
                eErr = CE_Failure;
            }

            if (bNeedResample && eErr == CE_None)
            {
                // Nearest-neighbour resample from full-res tmp to
                // the caller's output buffer.
                for (int iBand = 0; iBand < nBandCount; ++iBand)
                {
                    for (int iY = 0; iY < nBufYSize; ++iY)
                    {
                        const int srcY = static_cast<int>(
                            static_cast<double>(iY) * nYSize / nBufYSize);
                        for (int iX = 0; iX < nBufXSize; ++iX)
                        {
                            const int srcX = static_cast<int>(
                                static_cast<double>(iX) * nXSize / nBufXSize);
                            GDALCopyWords64(pTmp + iBand * cpBand +
                                                srcY * cpLine + srcX * cpPixel,
                                            eBufType, 0,
                                            static_cast<GByte *>(pData) +
                                                iBand * nBandSpace +
                                                iY * nLineSpace +
                                                iX * nPixelSpace,
                                            eBufType, 0, 1);
                        }
                    }
                }
            }
        }
        catch (...)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected exception occurred in "
                     "GrokDataset::DirectRasterIO()");
            eErr = CE_Failure;
        }

        VSIFree(pTmp);
        return eErr;
    }

    /**
     * @brief Initializes or reuses Grok async decompression.
     *
     * On first call (initializedAsync=false), configures decompress params
     * for the full image (or AdviseRead region) and calls grk_decompress()
     * to start async decoding, then grk_decompress_wait() for the first
     * swath.  The result is cached.
     *
     * Subsequent calls return the cached tile-grid info with tile
     * coordinates recomputed for the requested swath.  This avoids
     * calling grk_decompress_wait() again (which hangs after the
     * initial decompress completes).
     *
     * @param cb Decompress callback (unused in DirectRasterIO path)
     * @param user_data Callback user data
     * @param swath_x0 Swath origin x in image coordinates
     * @param swath_y0 Swath origin y in image coordinates
     * @param swath_width Swath width
     * @param swath_height Swath height
     * @return PostPreload with tile range and coordinates
     */
    PostPreload decompressAsynch(decompress_callback cb, void *user_data,
                                 int swath_x0, int swath_y0, int swath_width,
                                 int swath_height,
                                 RowCopyFunc rowCopy = nullptr,
                                 bool needPersistentTiles = false)
    {
        PostPreload rc;

        // Lazy codec initialization for overview datasets.
        // Overview datasets are created without a codec during Open;
        // initialise one on first read so decompression at this
        // reduce level can proceed.
        if (!m_codec && iLevel > 0)
        {
            m_codec = new GRKCodecWrapper();
            m_codec->open(fp_, nCodeStreamStart);
            uint32_t nTileW = 0, nTileH = 0;
            int numRes = 0;
            if (!m_codec->setUpDecompress(GetNumThreads(), m_osFilename.c_str(),
                                          nCodeStreamLength, &nTileW, &nTileH,
                                          &numRes))
            {
                delete m_codec;
                m_codec = nullptr;
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to init overview codec for level %d", iLevel);
                return rc;
            }
            // Grok may emit warnings (e.g. unknown JP2 box types)
            // while reading the header.  These are harmless and were
            // already reported when the main dataset was opened;
            // clear them so callers don't see stale warnings.
            CPLErrorReset();
        }

        // After the first decompress completes, tile data stays in the
        // codec's tile cache.  Call grk_decompress_wait() for each
        // swath to get correct tile coordinates and trigger tile
        // cleanup for previous swaths in TileCompletion.
        if (hasCachedPostPreload_)
        {
            PostPreload cached = cachedPostPreload_;
            // rowCopyDone_ was set during the initial row-by-row wait
            // loop.  Subsequent calls use the cached path (no row-by-row
            // loop), so the caller must call CopyTiles itself.
            cached.rowCopyDone_ = false;
            cached.x0 = swath_x0;
            cached.y0 = swath_y0;
            cached.x1 = swath_x0 + swath_width;
            cached.y1 = swath_y0 + swath_height;

            // Recompute tile range covering this swath
            uint32_t tw = m_nTileWidth;
            uint32_t th = m_nTileHeight;
            if (tw > 0 && th > 0)
            {
                cached.tile_x0 = static_cast<uint16_t>(swath_x0 / tw);
                cached.tile_y0 = static_cast<uint16_t>(swath_y0 / th);
                cached.tile_x1 = static_cast<uint16_t>(
                    (swath_x0 + swath_width + tw - 1) / tw);
                cached.tile_y1 = static_cast<uint16_t>(
                    (swath_y0 + swath_height + th - 1) / th);
            }

            // Call grk_decompress_wait with swath coordinates to
            // trigger TileCompletion row-based tile cleanup.
            // This avoids re-calling grk_decompress() which would
            // create a new TileCompletion that never gets notified.
            grk_wait_swath sw = {};
            sw.x0 = cached.x0;
            sw.y0 = cached.y0;
            sw.x1 = cached.x1;
            sw.y1 = cached.y1;
            grk_decompress_wait(m_codec->pCodec, &sw);

            return cached;
        }

        if (!initializedAsync)
        {
            grk_decompress_parameters decompressParams = {};
            decompressParams.asynchronous = true;
            decompressParams.simulate_synchronous = true;
            decompressParams.decompress_callback = cb;
            decompressParams.decompress_callback_user_data = user_data;
            decompressParams.core.tile_cache_strategy =
                needPersistentTiles ? GRK_TILE_CACHE_IMAGE
                                    : GRK_TILE_CACHE_NONE;
            // For multi-tile images, skip composite allocation to avoid
            // a full-resolution buffer.  Single-tile images need the
            // composite because Grok stores their data there (not in the
            // per-tile image), so grk_decompress_get_tile_image returns NULL.
            if (!this->bSingleTiled)
                decompressParams.core.skip_allocate_composite = true;
            decompressParams.core.reduce = iLevel;

            if (!this->fullDecompress_)
            {
                if (area_x0_ == 0 && area_y0_ == 0 && area_x1_ == 0 &&
                    area_y1_ == 0)
                {
                    // No AdviseRead region: use swath coordinates.
                    // to trigger full image asynch decompress
                    decompressParams.dw_x0 = swath_x0;
                    decompressParams.dw_y0 = swath_y0;
                    decompressParams.dw_x1 = swath_x0 + swath_width;
                    decompressParams.dw_y1 = swath_y0 + swath_height;
                }
                else
                {
                    decompressParams.dw_x0 = area_x0_;
                    decompressParams.dw_y0 = area_y0_;
                    decompressParams.dw_x1 = area_x1_;
                    decompressParams.dw_y1 = area_y1_;
                }
            }

            if (!grk_decompress_update(&decompressParams, m_codec->pCodec))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "grk_decompress_update() failed");
                return rc;
            }
            if (!grk_decompress(m_codec->pCodec, nullptr))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "grk_decompress() failed");
                return rc;
            }
            initializedAsync = true;
        }

        // For multi-tile images, wait row-by-row to advance
        // TileCompletion::lastCleared after each row.  This prevents
        // a deadlock in Grok's batch scheduler, which gates new tile
        // scheduling on lastCleared (back-pressure).  If we waited for
        // the entire image at once, lastCleared would never advance
        // and the scheduler would stall after the initial headroom.
        //
        // When a rowCopy callback is provided, each row's tile data
        // is copied to the output buffer immediately after its wait
        // completes — before the next wait releases those tiles.
        // This enables RSS clearing while preserving data correctness.
        grk_wait_swath swath = {};
        if (!this->bSingleTiled && m_nTileHeight > 0)
        {
            const int fullX0 = swath_x0;
            const int fullX1 = swath_x0 + swath_width;
            const int fullY1 = swath_y0 + swath_height;
            for (int rowY = swath_y0; rowY < fullY1; rowY += m_nTileHeight)
            {
                swath.x0 = fullX0;
                swath.y0 = rowY;
                swath.x1 = fullX1;
                swath.y1 =
                    std::min(rowY + static_cast<int>(m_nTileHeight), fullY1);
                grk_decompress_wait(m_codec->pCodec, &swath);

                // Copy this row's tiles before the next wait releases them
                if (rowCopy)
                {
                    PostPreload rowPost;
                    rowPost.asynch_ = true;
                    rowPost.tile_x0 = swath.tile_x0;
                    rowPost.tile_y0 = swath.tile_y0;
                    rowPost.tile_x1 = swath.tile_x1;
                    rowPost.tile_y1 = swath.tile_y1;
                    rowPost.num_tile_cols = swath.num_tile_cols;
                    CPLErr rowErr = rowCopy(rowPost);
                    if (rowErr != CE_None)
                    {
                        rc.asynch_ = false;
                        return rc;
                    }
                }
            }
            if (rowCopy)
                rc.rowCopyDone_ = true;
            // swath.tile_* was set by the last per-row wait.
            // Recompute to cover the full requested region.
            // (Do NOT call grk_decompress_wait again — earlier
            // row waits already released those tiles.)
            swath.x0 = swath_x0;
            swath.y0 = swath_y0;
            swath.x1 = swath_x0 + swath_width;
            swath.y1 = swath_y0 + swath_height;
        }
        else
        {
            swath.x0 = swath_x0;
            swath.y0 = swath_y0;
            swath.x1 = swath_x0 + swath_width;
            swath.y1 = swath_y0 + swath_height;
            grk_decompress_wait(m_codec->pCodec, &swath);
        }

        rc.asynch_ = true;
        rc.x0 = swath.x0;
        rc.y0 = swath.y0;
        rc.x1 = swath.x1;
        rc.y1 = swath.y1;

        rc.tile_x0 = swath.tile_x0;
        rc.tile_y0 = swath.tile_y0;
        rc.tile_x1 = swath.tile_x1;
        rc.tile_y1 = swath.tile_y1;
        rc.num_tile_cols = swath.num_tile_cols;

        cachedPostPreload_ = rc;
        hasCachedPostPreload_ = true;

        return rc;
    }

    /**
     * @brief Initialize decompression for a specific block
     *
     * @param fpIn @ref VSILFILE
     * @param codecWrapper codec wrapper
     * @param nBlockXOff block x offset
     * @param nBlockYOff block y offset
     * @param nRasterXSize_ image width
     * @param nRasterYSize_ image height
     * @param nBlockXSize block width
     * @param nBlockYSize block height
     * @param nTileNumber tile number
     * @return CPLErr error return code
     */
    CPLErr readBlockInit(CPL_UNUSED VSILFILE *fpIn,
                         GRKCodecWrapper *codecWrapper, int nBlockXOff,
                         int nBlockYOff, int nRasterXSize_, int nRasterYSize_,
                         int nBlockXSize, int nBlockYSize,
                         CPL_UNUSED int nTileNumber)
    {
        const int nXOff = nBlockXOff * nBlockXSize;
        const int nYOff = nBlockYOff * nBlockYSize;
        const int nWidthToRead = std::min(nBlockXSize, nRasterXSize_ - nXOff);
        const int nHeightToRead = std::min(nBlockYSize, nRasterYSize_ - nYOff);

        // The async decompress pipeline decodes a fixed decode window and
        // cleans up tiles after processing.  IReadBlock calls us for each
        // block, potentially a different tile each time.  For multi-tile
        // images, reset the codec so a fresh decompress can target just
        // this block's region.
        if (!bSingleTiled && (hasCachedPostPreload_ || initializedAsync))
        {
            delete m_codec;
            m_codec = new GRKCodecWrapper();
            m_codec->open(fp_, nCodeStreamStart);
            uint32_t tileW = 0, tileH = 0;
            int numRes = 0;
            if (!m_codec->setUpDecompress(GetNumThreads(), m_osFilename.c_str(),
                                          nCodeStreamLength, &tileW, &tileH,
                                          &numRes))
            {
                delete m_codec;
                m_codec = nullptr;
                CPLError(CE_Failure, CPLE_AppDefined,
                         "readBlockInit: codec reinit failed");
                return CE_Failure;
            }
            CPLErrorReset();
            hasCachedPostPreload_ = false;
            initializedAsync = false;
        }

        auto postPreload = decompressAsynch(nullptr, nullptr, nXOff, nYOff,
                                            nWidthToRead, nHeightToRead);
        if (!postPreload.asynch_)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "readBlockInit: decompressAsynch() failed");
            return CE_Failure;
        }

        // Compute the tile index from block coordinates (not from
        // postPreload.tile_x0/y0, which reflect the full decompress range).
        uint16_t tileno =
            static_cast<uint16_t>(nBlockYOff) * postPreload.num_tile_cols +
            static_cast<uint16_t>(nBlockXOff);
        grk_image *img =
            grk_decompress_get_tile_image(m_codec->pCodec, tileno, true);
        // For single-tile images, Grok puts data in the composite
        // image rather than the per-tile image
        if (!img)
            img = grk_decompress_get_image(m_codec->pCodec);
        if (!img)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "readBlockInit: grk_decompress_get_tile_image() "
                     "returned NULL for tile %d",
                     tileno);
            return CE_Failure;
        }
        codecWrapper->psImage = img;
        return CE_None;
    }

    /**
     * @brief Caches another JP2GRKDatasetBase in this one
     *
     * @param rhs another JP2GRKDatasetBase
     */
    void cache(JP2GRKDatasetBase *rhs)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (m_codec && rhs)
            m_codec->transfer(rhs->m_codec);
    }

    /**
     * @brief Caches another @ref GRKCodecWrapper in this one's codec wrapper
     * A new @ref GRKCodecWrapper is created.
     *
     * @param codecWrapper another @ref GRKCodecWrapper
     */
    void cacheNew(GRKCodecWrapper *codecWrapper)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (!codecWrapper)
            return;
        m_codec = new GRKCodecWrapper(codecWrapper);
    }

    /**
     * @brief Cleans up an @ref GRKCodecWrapper
     *
     * @param codecWrapper codec wrapper to clean up
     */
    void cache(GRKCodecWrapper *codecWrapper)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (codecWrapper)
            codecWrapper->cleanUpDecompress();
    }

    /**
     * @brief Takes ownership of the codec after JP2 Open completes.
     *
     * For non-single-tiled or non-decode-area modes (i.e. the Grok
     * driver's normal path), moves the codec from the local context
     * to this dataset's m_codec. After this, localctx is empty.
     *
     * Overview datasets lazily create their own codecs on first read
     * in decompressAsynch().
     *
     * @param codecWrapper Local codec context to move from
     */
    void openCompleteJP2(GRKCodecWrapper *codecWrapper)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (!bSingleTiled || !bUseSetDecodeArea)
        {
            cacheNew(codecWrapper);
            codecWrapper->pCodec = nullptr;
            codecWrapper->free();
        }
    }

    /**
     * @brief Releases codec resources on dataset close.
     *
     * For all datasets (main and overview), the codec is freed and
     * m_codec is deleted and nulled.  Only the main dataset
     * (iLevel==0) deletes the shared m_pnLastLevel pointer.
     */
    void closeJP2(void)
    {
        if (m_codec)
        {
            // Ensure the decompress worker thread and all futures
            // are joined/waited before destroying the codec.
            grk_decompress_wait(m_codec->pCodec, nullptr);
            m_codec->free();
            delete m_codec;
            m_codec = nullptr;
        }
        if (iLevel == 0)
        {
            delete m_pnLastLevel;
            m_pnLastLevel = nullptr;
        }
    }

    static void setMetaData(GDALDriver *poDriver)
    {
        poDriver->SetMetadataItem(
            GDAL_DMD_OPENOPTIONLIST,
            "<OpenOptionList>"
            "   <Option name='1BIT_ALPHA_PROMOTION' type='boolean' "
            "description='Whether a 1-bit alpha channel should be promoted to "
            "8-bit' default='YES'/>"
            "   <Option name='OPEN_REMOTE_GML' type='boolean' "
            "description='Whether "
            "to load remote vector layers referenced by a link in a GMLJP2 v2 "
            "box' "
            "default='NO'/>"
            "   <Option name='GEOREF_SOURCES' type='string' description='Comma "
            "separated list made with values "
            "INTERNAL/GMLJP2/GEOJP2/WORLDFILE/PAM/NONE that describe the "
            "priority "
            "order for georeferencing' default='PAM,GEOJP2,GMLJP2,WORLDFILE'/>"
            "   <Option name='USE_TILE_AS_BLOCK' type='boolean' "
            "description='Whether to always use the JPEG-2000 block size as "
            "the "
            "GDAL block size' default='NO'/>"
            "</OpenOptionList>");

        poDriver->SetMetadataItem(
            GDAL_DMD_CREATIONOPTIONLIST,
            "<CreationOptionList>"
            "   <Option name='CODEC' type='string-select' default='according "
            "to "
            "file extension. If unknown, default to J2K'>"
            "       <Value>JP2</Value>"
            "       <Value>J2K</Value>"
            "   </Option>"
            "   <Option name='GeoJP2' type='boolean' description='Whether to "
            "emit "
            "a GeoJP2 box' default='YES'/>"
            "   <Option name='GMLJP2' type='boolean' description='Whether to "
            "emit "
            "a GMLJP2 v1 box' default='YES'/>"
            "   <Option name='GMLJP2V2_DEF' type='string' "
            "description='Definition "
            "file to describe how a GMLJP2 v2 box should be generated. If set "
            "to "
            "YES, a minimal instance will be created'/>"
            "   <Option name='QUALITY' type='string' description='Single "
            "quality "
            "value or comma separated list of increasing quality values for "
            "several layers, each in the 0-100 range' default='25'/>"
            "   <Option name='REVERSIBLE' type='boolean' description='True if "
            "the "
            "compression is reversible' default='false'/>"
            "   <Option name='RESOLUTIONS' type='int' description='Number of "
            "resolutions.' min='1' max='30'/>"
            "   <Option name='BLOCKXSIZE' type='int' description='Tile Width' "
            "default='1024'/>"
            "   <Option name='BLOCKYSIZE' type='int' description='Tile Height' "
            "default='1024'/>"
            "   <Option name='PROGRESSION' type='string-select' default='LRCP'>"
            "       <Value>LRCP</Value>"
            "       <Value>RLCP</Value>"
            "       <Value>RPCL</Value>"
            "       <Value>PCRL</Value>"
            "       <Value>CPRL</Value>"
            "   </Option>"
            "   <Option name='SOP' type='boolean' description='True to insert "
            "SOP "
            "markers' default='false'/>"
            "   <Option name='EPH' type='boolean' description='True to insert "
            "EPH "
            "markers' default='false'/>"
            "   <Option name='YCBCR420' type='boolean' description='if RGB "
            "must be "
            "resampled to YCbCr 4:2:0' default='false'/>"
            "   <Option name='YCC' type='boolean' description='if RGB must be "
            "transformed to YCC color space (lossless MCT transform)' "
            "default='YES'/>"
            "   <Option name='NBITS' type='int' description='Bits (precision) "
            "for "
            "sub-byte files (1-7), sub-uint16 (9-15), sub-uint32 (17-31)'/>"
            "   <Option name='1BIT_ALPHA' type='boolean' description='Whether "
            "to "
            "encode the alpha channel as a 1-bit channel' default='NO'/>"
            "   <Option name='ALPHA' type='boolean' description='Whether to "
            "force "
            "encoding last channel as alpha channel' default='NO'/>"
            "   <Option name='PROFILE' type='string-select' description='Which "
            "codestream profile to use' default='AUTO'>"
            "       <Value>AUTO</Value>"
            "       <Value>UNRESTRICTED</Value>"
            "       <Value>PROFILE_1</Value>"
            "   </Option>"
            "   <Option name='INSPIRE_TG' type='boolean' description='Whether "
            "to "
            "use features that comply with Inspire Orthoimagery Technical "
            "Guidelines' default='NO'/>"
            "   <Option name='JPX' type='boolean' description='Whether to "
            "advertise JPX features when a GMLJP2 box is written (or use JPX "
            "branding if GMLJP2 v2)' default='YES'/>"
            "   <Option name='GEOBOXES_AFTER_JP2C' type='boolean' "
            "description='Whether to place GeoJP2/GMLJP2 boxes after the "
            "code-stream' default='NO'/>"
            "   <Option name='PRECINCTS' type='string' description='Precincts "
            "size "
            "as a string of the form {w,h},{w,h},... with power-of-two "
            "values'/>"
            "   <Option name='TILEPARTS' type='string-select' "
            "description='Whether "
            "to generate tile-parts and according to which criterion' "
            "default='DISABLED'>"
            "       <Value>DISABLED</Value>"
            "       <Value>RESOLUTIONS</Value>"
            "       <Value>LAYERS</Value>"
            "       <Value>COMPONENTS</Value>"
            "   </Option>"
            "   <Option name='CODEBLOCK_WIDTH' type='int' "
            "description='Codeblock "
            "width' default='64' min='4' max='1024'/>"
            "   <Option name='CODEBLOCK_HEIGHT' type='int' "
            "description='Codeblock "
            "height' default='64' min='4' max='1024'/>"
            "   <Option name='CT_COMPONENTS' type='int' min='3' max='4' "
            "description='If there is one color table, number of color table "
            "components to write. Autodetected if not specified.'/>"
            "   <Option name='WRITE_METADATA' type='boolean' "
            "description='Whether "
            "metadata should be written, in a dedicated JP2 XML box' "
            "default='NO'/>"
            "   <Option name='MAIN_MD_DOMAIN_ONLY' type='boolean' "
            "description='(Only if WRITE_METADATA=YES) Whether only metadata "
            "from "
            "the main domain should be written' default='NO'/>"
            "   <Option name='USE_SRC_CODESTREAM' type='boolean' "
            "description='When "
            "source dataset is JPEG2000, whether to reuse the codestream of "
            "the "
            "source dataset unmodified' default='NO'/>"
            "   <Option name='TRANSCODE' type='boolean' "
            "description='Transcode source JP2 without full decompression. "
            "Supports PLT, TLM, SOP, EPH, and PROGRESSION options.' "
            "default='NO'/>"
            "   <Option name='CODEBLOCK_STYLE' type='string' "
            "description='Comma-separated combination of BYPASS, RESET, "
            "TERMALL, "
            "VSC, PREDICTABLE, SEGSYM or value between 0 and 63'/>"
            "   <Option name='PLT' type='boolean' description='True to insert "
            "PLT "
            "marker segments' default='false'/>"
            "   <Option name='TLM' type='boolean' description='True to insert "
            "TLM "
            "marker segments' default='false'/>"
            "   <Option name='COMMENT' type='string' description='Content of "
            "the "
            "comment (COM) marker'/>"
            "</CreationOptionList>");
    }
};
