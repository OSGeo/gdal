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

#include <limits>
#include <algorithm>

/* This file is to be used with openjpeg 2.1 or later */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

#include <openjpeg.h>
#include <opj_config.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

typedef opj_codec_t jp2_codec;
typedef opj_image_t jp2_image;
typedef opj_stream_t jp2_stream;

typedef opj_image_cmptparm_t jp2_image_comp_param;
typedef opj_image_comp_t jp2_image_comp;

#define IS_OPENJPEG_OR_LATER(major, minor, patch)                              \
    ((OPJ_VERSION_MAJOR * 10000 + OPJ_VERSION_MINOR * 100 +                    \
      OPJ_VERSION_BUILD) >= ((major)*10000 + (minor)*100 + (patch)))

/************************************************************************/
/*                 JP2OpenJPEG_WarningCallback()                        */
/************************************************************************/

static void JP2OpenJPEG_WarningCallback(const char *pszMsg,
                                        CPL_UNUSED void *unused)
{
    if (strcmp(pszMsg, "No incltree created.\n") == 0 ||
        strcmp(pszMsg, "No imsbtree created.\n") == 0 ||
        strcmp(pszMsg, "tgt_create tree->numnodes == 0, no tree created.\n") ==
            0)
    {
        // Ignore warnings related to empty tag-trees. There's nothing wrong
        // about that.
        // Fixed submitted upstream with
        // https://github.com/uclouvain/openjpeg/pull/893
        return;
    }
    if (strcmp(pszMsg, "Empty SOT marker detected: Psot=12.\n") == 0)
    {
        static int bWarningEmitted = FALSE;
        if (bWarningEmitted)
            return;
        bWarningEmitted = TRUE;
    }
    if (strcmp(pszMsg, "JP2 box which are after the codestream will not be "
                       "read by this function.\n") == 0)
    {
        return;
    }

    std::string osMsg(pszMsg);
    if (!osMsg.empty() && osMsg.back() == '\n')
        osMsg.resize(osMsg.size() - 1);
    CPLError(CE_Warning, CPLE_AppDefined, "%s", osMsg.c_str());
}

/************************************************************************/
/*                 JP2OpenJPEG_InfoCallback()                           */
/************************************************************************/

static void JP2OpenJPEG_InfoCallback(const char *pszMsg,
                                     CPL_UNUSED void *unused)
{
    std::string osMsg(pszMsg);
    if (!osMsg.empty() && osMsg.back() == '\n')
        osMsg.resize(osMsg.size() - 1);
    CPLDebug("JP2OpenJPEG", "info: %s", osMsg.c_str());
}

/************************************************************************/
/*                  JP2OpenJPEG_ErrorCallback()                         */
/************************************************************************/

static void JP2OpenJPEG_ErrorCallback(const char *pszMsg,
                                      CPL_UNUSED void *unused)
{
    CPLError(CE_Failure, CPLE_AppDefined, "%s", pszMsg);
}

/************************************************************************/
/*                      JP2Dataset_Read()                               */
/************************************************************************/

static size_t JP2Dataset_Read(void *pBuffer, size_t nBytes, void *pUserData)
{
    JP2File *psJP2File = (JP2File *)pUserData;
    size_t nRet =
        static_cast<size_t>(VSIFReadL(pBuffer, 1, nBytes, psJP2File->fp_));
#ifdef DEBUG_IO
    CPLDebug(OPJCodecWrapper::debugId(),
             "JP2Dataset_Read(" CPL_FRMT_GUIB ") = " CPL_FRMT_GUIB,
             static_cast<GUIntBig>(nBytes), static_cast<GUIntBig>(nRet));
#endif
    if (nRet == 0)
        nRet = static_cast<size_t>(-1);

    return nRet;
}

/************************************************************************/
/*                      JP2Dataset_Write()                              */
/************************************************************************/

static size_t JP2Dataset_Write(void *pBuffer, size_t nBytes, void *pUserData)
{
    JP2File *psJP2File = (JP2File *)pUserData;
    size_t nRet =
        static_cast<size_t>(VSIFWriteL(pBuffer, 1, nBytes, psJP2File->fp_));
#ifdef DEBUG_IO
    CPLDebug(OPJCodecWrapper::debugId(),
             "JP2Dataset_Write(" CPL_FRMT_GUIB ") = " CPL_FRMT_GUIB,
             static_cast<GUIntBig>(nBytes), static_cast<GUIntBig>(nRet));
#endif
    if (nRet != nBytes)
        return static_cast<size_t>(-1);
    return nRet;
}

/************************************************************************/
/*                       JP2Dataset_Seek()                              */
/************************************************************************/

static OPJ_BOOL JP2Dataset_Seek(int64_t nBytes, void *pUserData)
{
    JP2File *psJP2File = (JP2File *)pUserData;
#ifdef DEBUG_IO
    CPLDebug(OPJCodecWrapper::debugId(), "JP2Dataset_Seek(" CPL_FRMT_GUIB ")",
             static_cast<GUIntBig>(nBytes));
#endif
    return VSIFSeekL(psJP2File->fp_, psJP2File->nBaseOffset + nBytes,
                     SEEK_SET) == 0;
}

/************************************************************************/
/*                     JP2Dataset_Skip()                                */
/************************************************************************/

static int64_t JP2Dataset_Skip(int64_t nBytes, void *pUserData)
{
    JP2File *psJP2File = (JP2File *)pUserData;
    vsi_l_offset nOffset = VSIFTellL(psJP2File->fp_);
    nOffset += nBytes;
#ifdef DEBUG_IO
    CPLDebug(OPJCodecWrapper::debugId(),
             "JP2Dataset_Skip(" CPL_FRMT_GUIB " -> " CPL_FRMT_GUIB ")",
             static_cast<GUIntBig>(nBytes), static_cast<GUIntBig>(nOffset));
#endif
    VSIFSeekL(psJP2File->fp_, nOffset, SEEK_SET);
    return nBytes;
}

/************************************************************************/
/* ==================================================================== */
/*                           OPJCodecWrapper                            */
/* ==================================================================== */
/************************************************************************/

struct OPJCodecWrapper
{
    OPJCodecWrapper(void)
        : pCodec(nullptr), pStream(nullptr), psImage(nullptr),
          pasBandParams(nullptr), psJP2File(nullptr)
    {
    }

    explicit OPJCodecWrapper(OPJCodecWrapper *rhs)
        : pCodec(rhs->pCodec), pStream(rhs->pStream), psImage(rhs->psImage),
          pasBandParams(rhs->pasBandParams), psJP2File(rhs->psJP2File)
    {
        rhs->pCodec = nullptr;
        rhs->pStream = nullptr;
        rhs->psImage = nullptr;
        rhs->pasBandParams = nullptr;
        rhs->psJP2File = nullptr;
    }

    ~OPJCodecWrapper(void)
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

    void transfer(OPJCodecWrapper *rhs)
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
                return OPJ_CLRSPC_UNKNOWN;
                break;
            case JP2_CLRSPC_SRGB:
                return OPJ_CLRSPC_SRGB;
                break;
            case JP2_CLRSPC_GRAY:
                return OPJ_CLRSPC_GRAY;
                break;
            case JP2_CLRSPC_SYCC:
                return OPJ_CLRSPC_SYCC;
                break;
            case JP2_CODEC_J2K:
                return OPJ_CODEC_J2K;
                break;
            case JP2_CODEC_JP2:
                return OPJ_CODEC_JP2;
                break;
            default:
                return INT_MAX;
                break;
        }
    }

    std::string getComment(void)
    {
        (void)this;
        std::string osComment = "Created by OpenJPEG version ";

        return osComment + opj_version();
    }

    void updateStrict(CPL_UNUSED bool strict)
    {
        // prevent linter from treating this as potential static method
        (void)this;
#if IS_OPENJPEG_OR_LATER(2, 5, 0)
        if (!strict)
            opj_decoder_set_strict_mode(pCodec, false);
#endif
    }

    static const char *debugId(void)
    {
        return "OPENJPEG";
    }

    void allocComponentParams(int nBands)
    {
        pasBandParams = (jp2_image_comp_param *)CPLMalloc(
            nBands * sizeof(jp2_image_comp_param));
    }

    void free(void)
    {
        if (pStream)
            opj_stream_destroy(pStream);
        pStream = nullptr;
        if (pCodec)
            opj_destroy_codec(pCodec);
        pCodec = nullptr;
        if (psImage)
            opj_image_destroy(psImage);
        psImage = nullptr;

        ::free(pasBandParams);
        pasBandParams = nullptr;

        CPLFree(psJP2File);
        psJP2File = nullptr;
    }

    static bool preferPerBlockDeCompress(void)
    {
        return true;
    }

    static uint32_t stride(jp2_image_comp *comp)
    {
        return comp->w;
    }

    bool setUpDecompress(CPL_UNUSED int numThreads,
                         vsi_l_offset nCodeStreamLength, uint32_t *nTileW,
                         uint32_t *nTileH, int *numResolutions)
    {

        OPJCodecWrapper codec;
        pCodec = opj_create_decompress(
            (OPJ_CODEC_FORMAT)OPJCodecWrapper::cvtenum(JP2_CODEC_J2K));
        if (pCodec == nullptr)
            return false;

        opj_set_info_handler(pCodec, JP2OpenJPEG_InfoCallback, nullptr);
        opj_set_warning_handler(pCodec, JP2OpenJPEG_WarningCallback, nullptr);
        opj_set_error_handler(pCodec, JP2OpenJPEG_ErrorCallback, nullptr);

        opj_dparameters_t decompressParams;
        opj_set_default_decoder_parameters(&decompressParams);
        if (!opj_setup_decoder(pCodec, &decompressParams))
        {
            opj_destroy_codec(pCodec);
            return false;
        }

        if (getenv("OPJ_NUM_THREADS") == nullptr)
        {
            opj_codec_set_threads(pCodec, numThreads);
        }

        pStream = CreateReadStream(psJP2File, nCodeStreamLength);
        if (pStream == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "CreateReadStream() failed");
            free();
            CPLFree(psJP2File);
            return false;
        }
        if (VSIFSeekL(psJP2File->fp_, psJP2File->nBaseOffset, SEEK_SET) == -1 ||
            !opj_read_header(pStream, pCodec, &psImage))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "opj_read_header() failed");
            free();
            CPLFree(psJP2File);
            return false;
        }

        auto pCodeStreamInfo = opj_get_cstr_info(pCodec);
        *nTileW = pCodeStreamInfo->tdx;
        *nTileH = pCodeStreamInfo->tdy;
#ifdef DEBUG
        uint32_t nX0, nY0;
        uint32_t nTilesX, nTilesY;
        nX0 = pCodeStreamInfo->tx0;
        nY0 = pCodeStreamInfo->ty0;
        nTilesX = pCodeStreamInfo->tw;
        nTilesY = pCodeStreamInfo->th;
        int mct = pCodeStreamInfo->m_default_tile_info.mct;
#endif
        *numResolutions =
            pCodeStreamInfo->m_default_tile_info.tccp_info[0].numresolutions;
        opj_destroy_cstr_info(&pCodeStreamInfo);
        if (psImage == nullptr)
        {
            free();
            CPLFree(psJP2File);
            return false;
        }
#ifdef DEBUG
        CPLDebug(OPJCodecWrapper::debugId(), "nX0 = %u", nX0);
        CPLDebug(OPJCodecWrapper::debugId(), "nY0 = %u", nY0);
        CPLDebug(OPJCodecWrapper::debugId(), "nTileW = %u", *nTileW);
        CPLDebug(OPJCodecWrapper::debugId(), "nTileH = %u", *nTileH);
        CPLDebug(OPJCodecWrapper::debugId(), "nTilesX = %u", nTilesX);
        CPLDebug(OPJCodecWrapper::debugId(), "nTilesY = %u", nTilesY);
        CPLDebug(OPJCodecWrapper::debugId(), "mct = %d", mct);
        CPLDebug(OPJCodecWrapper::debugId(), "psImage->x0 = %u", psImage->x0);
        CPLDebug(OPJCodecWrapper::debugId(), "psImage->y0 = %u", psImage->y0);
        CPLDebug(OPJCodecWrapper::debugId(), "psImage->x1 = %u", psImage->x1);
        CPLDebug(OPJCodecWrapper::debugId(), "psImage->y1 = %u", psImage->y1);
        CPLDebug(OPJCodecWrapper::debugId(), "psImage->numcomps = %d",
                 psImage->numcomps);
        // CPLDebug(OPJCodecWrapper::debugId(), "psImage->color_space = %d", psImage->color_space);
        CPLDebug(OPJCodecWrapper::debugId(), "numResolutions = %d",
                 *numResolutions);
        for (int i = 0; i < (int)psImage->numcomps; i++)
        {
            CPLDebug(OPJCodecWrapper::debugId(), "psImage->comps[%d].dx = %u",
                     i, psImage->comps[i].dx);
            CPLDebug(OPJCodecWrapper::debugId(), "psImage->comps[%d].dy = %u",
                     i, psImage->comps[i].dy);
            CPLDebug(OPJCodecWrapper::debugId(), "psImage->comps[%d].x0 = %u",
                     i, psImage->comps[i].x0);
            CPLDebug(OPJCodecWrapper::debugId(), "psImage->comps[%d].y0 = %u",
                     i, psImage->comps[i].y0);
            CPLDebug(OPJCodecWrapper::debugId(), "psImage->comps[%d].w = %u", i,
                     psImage->comps[i].w);
            CPLDebug(OPJCodecWrapper::debugId(), "psImage->comps[%d].h = %u", i,
                     psImage->comps[i].h);
            CPLDebug(OPJCodecWrapper::debugId(),
                     "psImage->comps[%d].resno_decoded = %d", i,
                     psImage->comps[i].resno_decoded);
            CPLDebug(OPJCodecWrapper::debugId(),
                     "psImage->comps[%d].factor = %d", i,
                     psImage->comps[i].factor);
            CPLDebug(OPJCodecWrapper::debugId(), "psImage->comps[%d].prec = %d",
                     i, psImage->comps[i].prec);
            CPLDebug(OPJCodecWrapper::debugId(), "psImage->comps[%d].sgnd = %d",
                     i, psImage->comps[i].sgnd);
        }
#endif
        if (psImage->x1 <= psImage->x0 || psImage->y1 <= psImage->y0 ||
            psImage->numcomps == 0 || (psImage->comps[0].w >> 31) != 0 ||
            (psImage->comps[0].h >> 31) != 0 || (*nTileW >> 31) != 0 ||
            (*nTileH >> 31) != 0 ||
            psImage->comps[0].w != psImage->x1 - psImage->x0 ||
            psImage->comps[0].h != psImage->y1 - psImage->y0)
        {
            CPLDebug(OPJCodecWrapper::debugId(),
                     "Unable to handle that image (1)");
            free();
            CPLFree(psJP2File);
            return false;
        }
        return true;
    }

    static bool preferPerTileCompress(void)
    {
        return true;
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

        opj_cparameters_t compressParams;
        opj_set_default_encoder_parameters(&compressParams);
        if (bSOP)
            compressParams.csty |= 0x02;
        if (bEPH)
            compressParams.csty |= 0x04;
        compressParams.cp_disto_alloc = 1;
        compressParams.tcp_numlayers = (int)adfRates.size();
        for (int i = 0; i < (int)adfRates.size(); i++)
            compressParams.tcp_rates[i] = (float)adfRates[i];
        compressParams.cp_tx0 = 0;
        compressParams.cp_ty0 = 0;
        compressParams.tile_size_on = TRUE;
        compressParams.cp_tdx = nBlockXSize;
        compressParams.cp_tdy = nBlockYSize;
        compressParams.irreversible = bIsIrreversible;
        compressParams.numresolution = nNumResolutions;
        compressParams.prog_order = (OPJ_PROG_ORDER)eProgOrder;
        compressParams.tcp_mct = static_cast<char>(bYCC);
        compressParams.cblockw_init = nCblockW;
        compressParams.cblockh_init = nCblockH;
        compressParams.mode = 0;

        std::string osComment;
        const char *pszCOM = CSLFetchNameValue(papszOptions, "COMMENT");
        if (pszCOM)
        {
            osComment = pszCOM;
            compressParams.cp_comment = &osComment[0];
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
            compressParams.cp_comment = &osComment[0];
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
                    compressParams.mode = nVal;
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
                        compressParams.mode |= (1 << 0);
                    }
                    else if (EQUAL(*papszIter, "RESET"))
                    {
                        compressParams.mode |= (1 << 1);
                    }
                    else if (EQUAL(*papszIter, "TERMALL"))
                    {
                        compressParams.mode |= (1 << 2);
                    }
                    else if (EQUAL(*papszIter, "VSC"))
                    {
                        compressParams.mode |= (1 << 3);
                    }
                    else if (EQUAL(*papszIter, "PREDICTABLE"))
                    {
                        compressParams.mode |= (1 << 4);
                    }
                    else if (EQUAL(*papszIter, "SEGSYM"))
                    {
                        compressParams.mode |= (1 << 5);
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
        for (int i = 0; i < nPrecincts && i < OPJ_J2K_MAXRLVLS; i++)
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
            compressParams.tp_on = 1;
            compressParams.tp_flag = 'R';
        }
        else if (EQUAL(pszTileParts, "LAYERS"))
        {
            if (compressParams.tcp_numlayers == 1)
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "TILEPARTS=LAYERS has no real interest with single-layer "
                    "codestream");
            }
            compressParams.tp_on = 1;
            compressParams.tp_flag = 'L';
        }
        else if (EQUAL(pszTileParts, "COMPONENTS"))
        {
            compressParams.tp_on = 1;
            compressParams.tp_flag = 'C';
        }
        else if (!EQUAL(pszTileParts, "DISABLED"))
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Invalid value for TILEPARTS");
        }

        if (bProfile1)
        {
            compressParams.rsiz = OPJ_PROFILE_1;
        }

        /* Always ask OpenJPEG to do codestream only. We will take care */
        /* of JP2 boxes */
        pCodec = opj_create_compress(
            (OPJ_CODEC_FORMAT)OPJCodecWrapper::cvtenum(JP2_CODEC_J2K));
        if (pCodec == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "opj_create_compress() failed");
            return false;
        }

        opj_set_info_handler(pCodec, JP2OpenJPEG_InfoCallback, nullptr);
        opj_set_warning_handler(pCodec, JP2OpenJPEG_WarningCallback, nullptr);
        opj_set_error_handler(pCodec, JP2OpenJPEG_ErrorCallback, nullptr);

        psImage = opj_image_tile_create(nBands, pasBandParams,
                                        (OPJ_COLOR_SPACE)eColorSpace);

        if (psImage == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "opj_image_tile_create() failed");
            free();
            return false;
        }

        psImage->x0 = 0;
        psImage->y0 = 0;
        psImage->x1 = nXSize;
        psImage->y1 = nYSize;
        psImage->color_space = (OPJ_COLOR_SPACE)eColorSpace;
        psImage->numcomps = nBands;

        if (!opj_setup_encoder(pCodec, &compressParams, psImage))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "opj_setup_encoder() failed");
            free();
            return false;
        }

#if IS_OPENJPEG_OR_LATER(2, 4, 0)
        if (getenv("OPJ_NUM_THREADS") == nullptr)
            opj_codec_set_threads(pCodec, numThreads);
        CPLStringList aosOptions;
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "PLT", "FALSE")))
        {
            aosOptions.AddString("PLT=YES");
        }

#if IS_OPENJPEG_OR_LATER(2, 5, 0)
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "TLM", "FALSE")))
        {
            aosOptions.AddString("TLM=YES");
        }
#endif

        if (!opj_encoder_set_extra_options(pCodec, aosOptions.List()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "opj_encoder_set_extra_options() failed");
            free();
            return false;
        }
#endif
        pStream = opj_stream_create(1024 * 1024, FALSE);
        opj_stream_set_write_function(pStream, JP2Dataset_Write);
        opj_stream_set_seek_function(pStream, JP2Dataset_Seek);
        opj_stream_set_skip_function(pStream, JP2Dataset_Skip);
        opj_stream_set_user_data(pStream, psJP2File, nullptr);

        return opj_start_compress(pCodec, psImage, pStream);
    }

    bool compressTile(int tileIndex, GByte *buff, uint32_t buffLen)
    {
        if (!pCodec || !pStream)
            return false;
        return opj_write_tile(pCodec, tileIndex, buff, buffLen, pStream);
    }

    bool finishCompress(void)
    {
        bool rc = false;
        if (pCodec && pStream)
            rc = opj_end_compress(pCodec, pStream);
        if (!rc)
            CPLError(CE_Failure, CPLE_AppDefined, "opj_end_compress() failed");
        free();
        return rc;
    }

    void cleanUpDecompress(void)
    {
        if (pCodec && pStream)
            opj_end_decompress(pCodec, pStream);
        free();
    }

    /************************************************************************/
    /*                    CreateReadStream()                                */
    /************************************************************************/
    static jp2_stream *CreateReadStream(JP2File *psJP2File, vsi_l_offset nSize)
    {
        if (!psJP2File)
            return nullptr;
        auto pStream = opj_stream_create(
            1024, TRUE);  // Default 1MB is way too big for some datasets
        if (pStream == nullptr)
            return nullptr;

        VSIFSeekL(psJP2File->fp_, psJP2File->nBaseOffset, SEEK_SET);
        opj_stream_set_user_data_length(pStream, nSize);

        opj_stream_set_read_function(pStream, JP2Dataset_Read);
        opj_stream_set_seek_function(pStream, JP2Dataset_Seek);
        opj_stream_set_skip_function(pStream, JP2Dataset_Skip);
        opj_stream_set_user_data(pStream, psJP2File, nullptr);

        return pStream;
    }

    jp2_codec *pCodec;
    jp2_stream *pStream;
    jp2_image *psImage;
    jp2_image_comp_param *pasBandParams;
    JP2File *psJP2File;
};

/************************************************************************/
/* ==================================================================== */
/*                           JP2OPJDatasetBase                          */
/* ==================================================================== */
/************************************************************************/

struct JP2OPJDatasetBase : public JP2DatasetBase
{
    int eColorSpace = OPJCodecWrapper::cvtenum(JP2_CLRSPC_UNKNOWN);
    OPJCodecWrapper *m_codec = nullptr;
    int *m_pnLastLevel = nullptr;
    bool m_bStrict = true;

    void init(void)
    {
        (void)this;
    }

    void deinit(void)
    {
        (void)this;
    }

    CPLErr readBlockInit(VSILFILE *fpIn, OPJCodecWrapper *codec, int nBlockXOff,
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
                codec->pCodec != nullptr && *codec->pStream != nullptr &&
                m_codec->psImage != nullptr)
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
            codec->pCodec = opj_create_decompress(
                (OPJ_CODEC_FORMAT)OPJCodecWrapper::cvtenum(JP2_CODEC_J2K));
            if (codec->pCodec == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "opj_create_decompress() failed");
                return CE_Failure;
            }

            opj_set_info_handler(codec->pCodec, JP2OpenJPEG_InfoCallback,
                                 nullptr);
            opj_set_warning_handler(codec->pCodec, JP2OpenJPEG_WarningCallback,
                                    nullptr);
            opj_set_error_handler(codec->pCodec, JP2OpenJPEG_ErrorCallback,
                                  nullptr);

            opj_dparameters_t parameters;
            opj_set_default_decoder_parameters(&parameters);
            if (!opj_setup_decoder(codec->pCodec, &parameters))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "opj_setup_decoder() failed");
                return CE_Failure;
            }
#if IS_OPENJPEG_OR_LATER(2, 5, 0)
            if (!m_bStrict)
            {
                opj_decoder_set_strict_mode(codec->pCodec, false);
            }
#endif
            if (m_codec && m_codec->psJP2File)
            {
                codec->pStream = OPJCodecWrapper::CreateReadStream(
                    m_codec->psJP2File, nCodeStreamLength);
            }
            else
            {
                codec->open(fpIn, nCodeStreamStart);
                codec->pStream = OPJCodecWrapper::CreateReadStream(
                    codec->psJP2File, nCodeStreamLength);
            }
            if (!codec->pStream)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "OPJCodecWrapper::CreateReadStream() failed");
                return CE_Failure;
            }

            if (getenv("OPJ_NUM_THREADS") == nullptr)
            {
                if (m_nBlocksToLoad <= 1)
                    opj_codec_set_threads(codec->pCodec, GetNumThreads());
                else
                    opj_codec_set_threads(codec->pCodec,
                                          GetNumThreads() / m_nBlocksToLoad);
            }

            if (!opj_read_header(codec->pStream, codec->pCodec,
                                 &codec->psImage))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "opj_read_header() failed (psImage=%p)",
                         codec->psImage);
                // Hopefully the situation is better on openjpeg 2.2 regarding
                // cleanup
                // We may leak objects, but the cleanup of openjpeg can cause
                // double frees sometimes...
                return CE_Failure;
            }
        }
        if (!opj_set_decoded_resolution_factor(codec->pCodec, iLevel))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "opj_set_decoded_resolution_factor() failed");
            return CE_Failure;
        }
        if (bUseSetDecodeArea)
        {
            /* We need to explicitly set the resolution factor on the image */
            /* otherwise opj_set_decode_area() will assume we decode at full */
            /* resolution. */
            /* If using parameters.cp_reduce instead of
          * opj_set_decoded_resolution_factor() */
            /* we wouldn't need to do that, as opj_read_header() would automatically
          */
            /* assign the comps[].factor to the appropriate value */
            for (unsigned int iBand = 0; iBand < codec->psImage->numcomps;
                 iBand++)
            {
                codec->psImage->comps[iBand].factor = iLevel;
            }
            /* The decode area must be expressed in grid reference, ie at full*/
            /* scale */
            if (!opj_set_decode_area(
                    codec->pCodec, codec->psImage,
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
                         "opj_set_decode_area() failed");
                return CE_Failure;
            }
            if (!opj_decode(codec->pCodec, codec->pStream, codec->psImage))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "opj_decode() failed");
                return CE_Failure;
            }
        }
        else
        {
            if (!opj_get_decoded_tile(codec->pCodec, codec->pStream,
                                      codec->psImage, nTileNumber))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "opj_get_decoded_tile() failed");
                return CE_Failure;
            }
        }

        return CE_None;
    }

    void cache(CPL_UNUSED JP2OPJDatasetBase *rhs)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (m_codec && rhs)
            m_codec->transfer(rhs->m_codec);
    }

    void cacheNew(CPL_UNUSED OPJCodecWrapper *codec)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (!codec)
            return;
        if (m_codec)
            m_codec = new OPJCodecWrapper(codec);
    }

    void cache(CPL_UNUSED OPJCodecWrapper *codec)
    {
        // prevent linter from treating this as potential static method
        (void)this;
        if (!codec)
            return;

        if (m_codec && CPLTestBool(CPLGetConfigOption(
                           "USE_OPENJPEG_SINGLE_TILE_OPTIM", "YES")))
        {
            codec->transfer(m_codec);
        }
        else
        {
            codec->cleanUpDecompress();
        }
    }

    void openCompleteJP2(OPJCodecWrapper *codec)
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
        // prevent linter from treating this as potential static method
        (void)this;
        if (iLevel == 0)
        {
            if (m_codec)
                m_codec->free();
            delete m_pnLastLevel;
            m_pnLastLevel = nullptr;
        }
    }
};
