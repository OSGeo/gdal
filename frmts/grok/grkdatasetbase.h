/******************************************************************************
 *
 * Author:   Aaron Boxer, <boxerab at protonmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, European Union (European Environment Agency)
 * Copyright (c) 2023, Grok Image Compression Inc.
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
#pragma once

#include <grok.h>
#include <grk_config.h>
#include <algorithm>

typedef grk_image jp2_image;
typedef grk_image_comp jp2_image_comp;
typedef grk_codec jp2_codec;

/************************************************************************/
/*                 JP2_WarningCallback()                                */
/************************************************************************/

static void JP2_WarningCallback(const char *pszMsg, CPL_UNUSED void *unused)
{
    std::string osMsg(pszMsg);
    if (!osMsg.empty() && osMsg.back() == '\n')
        osMsg.resize(osMsg.size() - 1);
    CPLError(CE_Warning, CPLE_AppDefined, "%s", osMsg.c_str());
}

/************************************************************************/
/*                 JP2_InfoCallback()                                   */
/************************************************************************/

static void JP2_InfoCallback(const char *pszMsg, CPL_UNUSED void *unused)
{
    std::string osMsg(pszMsg);
    if (!osMsg.empty() && osMsg.back() == '\n')
        osMsg.resize(osMsg.size() - 1);
    CPLDebug("GROK", "info: %s", osMsg.c_str());
}

/************************************************************************/
/*                  JP2_ErrorCallback()                                 */
/************************************************************************/

static void JP2_ErrorCallback(const char *pszMsg, CPL_UNUSED void *unused)
{
    CPLError(CE_Failure, CPLE_AppDefined, "%s", pszMsg);
}

/************************************************************************/
/*                      JP2Dataset_Read()                               */
/************************************************************************/

static size_t JP2Dataset_Read(uint8_t *pBuffer, size_t nBytes, void *pUserData)
{
    JP2File *psJP2File = (JP2File *)pUserData;
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
/*                      JP2Dataset_Write()                              */
/************************************************************************/

static size_t JP2Dataset_Write(const uint8_t *pBuffer, size_t nBytes,
                               void *pUserData)
{
    JP2File *psJP2File = (JP2File *)pUserData;
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
/*                       JP2Dataset_Seek()                              */
/************************************************************************/

static bool JP2Dataset_Seek(uint64_t nBytes, void *pUserData)
{
    JP2File *psJP2File = (JP2File *)pUserData;
#ifdef DEBUG_IO
    CPLDebug(debugId(), "JP2Dataset_Seek(" CPL_FRMT_GUIB ")",
             static_cast<GUIntBig>(nBytes));
#endif
    return VSIFSeekL(psJP2File->fp_, psJP2File->nBaseOffset + nBytes,
                     SEEK_SET) == 0;
}

struct GRKCodecWrapper
{
    GRKCodecWrapper(void)
        : pCodec(nullptr), psImage(nullptr), pasBandParams(nullptr),
          psJP2File(nullptr)
    {
        grk_compress_set_default_params(&compressParams);
        grk_decompress_set_default_params(&decompressParams);
    }
    explicit GRKCodecWrapper(GRKCodecWrapper *rhs)
        : pCodec(rhs->pCodec), psImage(rhs->psImage),
          pasBandParams(rhs->pasBandParams), psJP2File(rhs->psJP2File)
    {
        rhs->pCodec = nullptr;
        rhs->psImage = nullptr;
        rhs->pasBandParams = nullptr;
        rhs->psJP2File = nullptr;
    }
    ~GRKCodecWrapper(void)
    {
        free();
    }
    void open(VSILFILE *fp, vsi_l_offset offset)
    {
        psJP2File = static_cast<JP2File *>(CPLMalloc(sizeof(JP2File)));
        psJP2File->fp_ = fp;
        psJP2File->nBaseOffset = offset;
    }
    void open(VSILFILE *fp)
    {
        psJP2File = static_cast<JP2File *>(CPLMalloc(sizeof(JP2File)));
        psJP2File->fp_ = fp;
        psJP2File->nBaseOffset = VSIFTellL(fp);
    }
    void transfer(GRKCodecWrapper *rhs)
    {
        pCodec = rhs->pCodec;
        rhs->pCodec = nullptr;
        psImage = rhs->psImage;
        rhs->psImage = nullptr;
        psJP2File = rhs->psJP2File;
        rhs->psJP2File = nullptr;
    }

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

    std::string getComment(void)
    {
        (void)this;
        std::string osComment = "Created by Grok version ";

        return osComment + grk_version();
    }
    void updateStrict(CPL_UNUSED bool strict)
    {
        // prevent linter from treating this as potential static method
        (void)this;
    }

    /* Depending on the way OpenJPEG <= r2950 is built, YCC with 4 bands might
   * work on Debug mode, but this relies on unreliable stack buffer overflows,
   * so better err on the safe side */
    static bool supportsYCC_4Band(void)
    {
        return true;
    }

    static const char *debugId(void)
    {
        return "GROK";
    }

    void allocComponentParams(int nBands)
    {
        pasBandParams =
            (grk_image_comp *)CPLMalloc(nBands * sizeof(grk_image_comp));
    }

    void free(void)
    {
        if (pCodec)
            grk_object_unref(pCodec);
        pCodec = nullptr;
        // todo: unref compression image only
        psImage = nullptr;

        ::free(pasBandParams);
        pasBandParams = nullptr;

        CPLFree(psJP2File);
        psJP2File = nullptr;
    }

    static bool preferPerBlockDeCompress(void)
    {
        return false;
    }

    static uint32_t stride(jp2_image_comp *comp)
    {
        return comp->stride;
    }

    bool setUpDecompress(CPL_UNUSED int numThreads,
                         vsi_l_offset nCodeStreamLength, uint32_t *nTileW,
                         uint32_t *nTileH, int *numResolutions)
    {
        grk_stream_params streamParams;
        grk_set_default_stream_params(&streamParams);
        streamParams.seek_fn = JP2Dataset_Seek;
        streamParams.read_fn = JP2Dataset_Read;
        streamParams.user_data = psJP2File;
        streamParams.stream_len = nCodeStreamLength;
        pCodec = grk_decompress_init(&streamParams, &decompressParams.core);
        if (pCodec == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "setUpDecompress() failed");
            free();
            CPLFree(psJP2File);
            return false;
        }
        //        if (getenv("GRK_NUM_THREADS") == nullptr)
        //        {
        //            opj_codec_set_threads(pCodec, numThreads);
        //        }
        // read j2k header
        grk_header_info headerInfo;
        memset(&headerInfo, 0, sizeof(headerInfo));
        VSIFSeekL(psJP2File->fp_, psJP2File->nBaseOffset, SEEK_SET);
        if (!grk_decompress_read_header(pCodec, &headerInfo))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "setUpDecompress() failed");
            free();
            CPLFree(psJP2File);
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
        psImage = grk_decompress_get_composited_image(pCodec);
        if (psImage == nullptr)
        {
            free();
            CPLFree(psJP2File);
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
        //        CPLDebug(debugId(), "psImage->comps[%d].resno_decoded = %d", i,
        //                 psImage->comps[i].resno_decoded);
        //        CPLDebug(debugId(), "psImage->comps[%d].factor = %d", i,
        //                 psImage->comps[i].factor);
        // CPLDebug(debugId(), "psImage->color_space = %d", psImage->color_space);
        CPLDebug(debugId(), "numResolutions = %d", *numResolutions);
        for (int i = 0; i < (int)psImage->numcomps; i++)
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
            CPLFree(psJP2File);
            return false;
        }
        return true;
    }

    static bool preferPerTileCompress(void)
    {
        return false;
    }

    bool initCompress(char **papszOptions, const std::vector<double> &adfRates,
                      int nBlockXSize, int nBlockYSize, bool bIsIrreversible,
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
        compressParams.allocationByRateDistoration = true;
        compressParams.numlayers = (int)adfRates.size();
        for (int i = 0; i < (int)adfRates.size(); i++)
            compressParams.layer_rate[i] = (float)adfRates[i];
        compressParams.tx0 = 0;
        compressParams.ty0 = 0;
        compressParams.tile_size_on = TRUE;
        compressParams.t_width = nBlockXSize;
        compressParams.t_height = nBlockYSize;
        compressParams.irreversible = bIsIrreversible;
        compressParams.numresolution = nNumResolutions;
        compressParams.prog_order = (GRK_PROG_ORDER)eProgOrder;
        compressParams.mct = static_cast<char>(bYCC);
        compressParams.cblockw_init = nCblockW;
        compressParams.cblockh_init = nCblockH;
        compressParams.cblk_sty = 0;

        std::string osComment;
        const char *pszCOM = CSLFetchNameValue(papszOptions, "COMMENT");
        if (pszCOM)
        {
            osComment = pszCOM;
            compressParams.num_comments = 1;
            compressParams.comment[0] = &osComment[0];
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
        }

        // Was buggy before for some of the options
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
        for (int i = 0; i < nPrecincts && i < GRK_J2K_MAXRLVLS; i++)
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
            compressParams.enableTilePartGeneration = true;
            compressParams.newTilePartProgressionDivider = 'R';
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
            compressParams.enableTilePartGeneration = true;
            compressParams.newTilePartProgressionDivider = 'L';
        }
        else if (EQUAL(pszTileParts, "COMPONENTS"))
        {
            compressParams.enableTilePartGeneration = true;
            compressParams.newTilePartProgressionDivider = 'C';
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
        //  if (getenv("GRK_NUM_THREADS") == nullptr)
        //      opj_codec_set_threads(pCodec, numThreads);
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "PLT", "FALSE")))
        {
            compressParams.writePLT = true;
        }
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "TLM", "FALSE")))
        {
            compressParams.writeTLM = true;
        }

        psImage = grk_image_new(nBands, pasBandParams,
                                (GRK_COLOR_SPACE)eColorSpace, true);
        if (psImage == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "grk_image_new() failed");
            free();
            return false;
        }

        psImage->x0 = 0;
        psImage->y0 = 0;
        psImage->x1 = nXSize;
        psImage->y1 = nYSize;
        psImage->color_space = (GRK_COLOR_SPACE)eColorSpace;
        psImage->numcomps = nBands;

        grk_stream_params streamParams;
        grk_set_default_stream_params(&streamParams);
        streamParams.seek_fn = JP2Dataset_Seek;
        streamParams.write_fn = JP2Dataset_Write;
        streamParams.user_data = psJP2File;

        /* Always ask Grok to do codestream only. We will take care */
        /* of JP2 boxes */
        pCodec = grk_compress_init(&streamParams, &compressParams, psImage);
        if (pCodec == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "grk_compress_init() failed");
            return false;
        }

        return true;
    }

    bool compressTile(CPL_UNUSED int tileIndex, CPL_UNUSED GByte *buff,
                      CPL_UNUSED uint32_t buffLen)
    {
        return grk_compress(pCodec, nullptr) != 0;
    }

    bool finishCompress(void)
    {
        free();
        return true;
    }

    void cleanUpDecompress(void)
    {
        free();
    }

    grk_decompress_parameters decompressParams;
    grk_cparameters compressParams;
    jp2_codec *pCodec;
    jp2_image *psImage;
    grk_image_comp *pasBandParams;
    JP2File *psJP2File;
};

struct JP2GRKDatasetBase : public JP2DatasetBase
{
    JP2_COLOR_SPACE eColorSpace = GRKCodecWrapper::cvtenum(JP2_CLRSPC_UNKNOWN);
    GRKCodecWrapper *m_codec = nullptr;
    int *m_pnLastLevel = nullptr;
    bool m_bStrict;

    ~JP2GRKDatasetBase(void)
    {
        delete m_codec;
    }

    void init(void)
    {
        grk_initialize(nullptr, 0, false);
        grk_set_msg_handlers(JP2_InfoCallback, nullptr, JP2_WarningCallback,
                             nullptr, JP2_ErrorCallback, nullptr);
    }

    void deinit(void)
    {
    }

    CPLErr readBlockInit(VSILFILE *fpIn, GRKCodecWrapper *codec, int nBlockXOff,
                         int nBlockYOff, int nRasterXSize, int nRasterYSize,
                         int nBlockXSize, int nBlockYSize, int nTileNumber)
    {
        const int nWidthToRead =
            std::min(nBlockXSize, nRasterXSize - nBlockXOff * nBlockXSize);
        const int nHeightToRead =
            std::min(nBlockYSize, nRasterYSize - nBlockYOff * nBlockYSize);

        if (!codec)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "null codec");
            return CE_Failure;
        }

        if (m_codec && CPLTestBool(CPLGetConfigOption(
                           "USE_OPENJPEG_SINGLE_TILE_OPTIM", "YES")))
        {
            if ((*m_pnLastLevel == -1 || *m_pnLastLevel == iLevel) &&
                m_codec->pCodec != nullptr && m_codec->psImage != nullptr)
            {
                codec->transfer(m_codec);
            }
            else
            {
                // For some reason, we need to "reboot" all the machinery if
                // changing of overview level. Should be fixed in openjpeg
                m_codec->free();
            }
        }
        *m_pnLastLevel = iLevel;

        if (codec->pCodec == nullptr)
        {
            grk_decompress_parameters decompressParams;
            grk_decompress_set_default_params(&decompressParams);
            decompressParams.core.reduce = iLevel;

            grk_stream_params streamParams;
            grk_set_default_stream_params(&streamParams);
            streamParams.seek_fn = JP2Dataset_Seek;
            streamParams.read_fn = JP2Dataset_Read;
            streamParams.stream_len = nCodeStreamLength;
            if (m_codec && m_codec->psJP2File)
            {
                streamParams.user_data = m_codec->psJP2File;
                if (VSIFSeekL(m_codec->psJP2File->fp_,
                              m_codec->psJP2File->nBaseOffset, SEEK_SET))
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "VSIFSeekL failed");
                    return CE_Failure;
                }
            }
            else
            {
                codec->open(fpIn, nCodeStreamStart);
                streamParams.user_data = codec->psJP2File;
                if (VSIFSeekL(codec->psJP2File->fp_,
                              codec->psJP2File->nBaseOffset, SEEK_SET))
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "VSIFSeekL failed");
                    return CE_Failure;
                }
            }
            codec->pCodec =
                grk_decompress_init(&streamParams, &decompressParams.core);
            if (codec->pCodec == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "grk_decompress_init() failed");
                return CE_Failure;
            }
            //            if (getenv("GRK_NUM_THREADS") == nullptr)
            //            {
            //                if (m_nBlocksToLoad <= 1)
            //                    opj_codec_set_threads(local_ctx->pCodec, GetNumThreads());
            //                else
            //                    opj_codec_set_threads(local_ctx->pCodec,
            //                                          GetNumThreads() / m_nBlocksToLoad);
            //            }
        }
        grk_header_info headerInfo;
        memset(&headerInfo, 0, sizeof(headerInfo));
        if (!grk_decompress_read_header(codec->pCodec, &headerInfo))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "grk_decompress_read_header() failed (psImage=%p)",
                     codec->psImage);
            // Hopefully the situation is better on openjpeg 2.2 regarding
            // cleanup
            // We may leak objects, but the cleanup of openjpeg can cause
            // double frees sometimes...
            return CE_Failure;
        }
        if (bUseSetDecodeArea)
        {
            /* The decode area must be expressed in grid reference, ie at full*/
            /* scale */
            if (!grk_decompress_set_window(
                    codec->pCodec,
                    m_nX0 + static_cast<int>(
                                static_cast<GIntBig>(nBlockXOff * nBlockXSize) *
                                nParentXSize / nRasterXSize),
                    m_nY0 + static_cast<int>(
                                static_cast<GIntBig>(nBlockYOff * nBlockYSize) *
                                nParentYSize / nRasterYSize),
                    m_nX0 + static_cast<int>(
                                static_cast<GIntBig>(nBlockXOff * nBlockXSize +
                                                     nWidthToRead) *
                                nParentXSize / nRasterXSize),
                    m_nY0 + static_cast<int>(
                                static_cast<GIntBig>(nBlockYOff * nBlockYSize +
                                                     nHeightToRead) *
                                nParentYSize / nRasterYSize)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "grk_decompress_set_window() failed");
                return CE_Failure;
            }
            if (!grk_decompress(codec->pCodec, nullptr))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "grk_decompress() failed");
                return CE_Failure;
            }
        }
        else
        {
            if (!grk_decompress_tile(codec->pCodec, nTileNumber))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "grk_decompress_tile() failed");
                return CE_Failure;
            }
        }

        codec->psImage = grk_decompress_get_composited_image(codec->pCodec);
        if (!codec->psImage)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "grk_decompress_get_composited_image() failed");
            return CE_Failure;
        }

        return CE_None;
    }

    void cache(JP2GRKDatasetBase *rhs)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (m_codec && rhs)
            m_codec->transfer(rhs->m_codec);
    }

    void cacheNew(GRKCodecWrapper *codec)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (!codec)
            return;
        m_codec = new GRKCodecWrapper(codec);
    }

    void cache(GRKCodecWrapper *codec)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (!codec)
            return;
        if (m_codec && CPLTestBool(CPLGetConfigOption(
                           "USE_OPENJPEG_SINGLE_TILE_OPTIM", "YES")))
        {
            m_codec->transfer(codec);
        }
        else
        {
            codec->cleanUpDecompress();
        }
    }

    void openCompleteJP2(GRKCodecWrapper *codec)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (bSingleTiled && bUseSetDecodeArea)
        {
            // nothing
        }
        else
        {
            if (codec)
                codec->free();
        }
    }

    void closeJP2(void)
    {
        if (iLevel == 0)
        {
            if (m_codec)
                m_codec->free();
            delete m_pnLastLevel;
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
