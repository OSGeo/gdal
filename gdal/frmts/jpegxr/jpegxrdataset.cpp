/******************************************************************************
 *
 * Project:  JPEGXR driver based on jxrlib library
 * Purpose:  JPEGXR driver based on jxrlib library
 * Author:   Mateusz Loskot <mateusz at loskot dot net>
 *
 ******************************************************************************
 * Copyright (c) 2017, Mateusz Loskot <mateusz at loskot dot net>
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

#ifndef __ANSI__
#define __ANSI__
#endif
#include <JXRGlue.h>

#include <exception>

// TODO: Move to GDAL-wide common config header
#ifdef __clang__
#if __has_feature(cxx_noexcept)
#define GDAL_NOEXCEPT noexcept
#endif
#else
#if defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ * 10 + __GNUC_MINOR__ >= 46 || \
    defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 190023026
#define GDAL_NOEXCEPT noexcept
#else
#define GDAL_NOEXCEPT
#endif
#endif

// QP tables copied from the Open Source implementation of JPEG XR by Microsoft
// Source: https://jxrlib.codeplex.com/
// License: New BSD License(BSD)
// Copyright(c) 2009, Microsoft. All rights reserved.
namespace QP
{
// Quantization Parameters for supported Bit Depths
// Y, U, V, YHP, UHP, VHP

int const BD8_420[11][6] = // for 8 Bit Depth only
{
    { 66, 65, 70, 72, 72, 77 },
    { 59, 58, 63, 64, 63, 68 },
    { 52, 51, 57, 56, 56, 61 },
    { 48, 48, 54, 51, 50, 55 },
    { 43, 44, 48, 46, 46, 49 },
    { 37, 37, 42, 38, 38, 43 },
    { 26, 28, 31, 27, 28, 31 },
    { 16, 17, 22, 16, 17, 21 },
    { 10, 11, 13, 10, 10, 13 },
    { 5,  5,  6,  5,  5,  6 },
    { 2,  2,  3,  2,  2,  2 }
};

int const BD8[12][6] =
{
    { 67, 79, 86, 72, 90, 98 },
    { 59, 74, 80, 64, 83, 89 },
    { 53, 68, 75, 57, 76, 83 },
    { 49, 64, 71, 53, 70, 77 },
    { 45, 60, 67, 48, 67, 74 },
    { 40, 56, 62, 42, 59, 66 },
    { 33, 49, 55, 35, 51, 58 },
    { 27, 44, 49, 28, 45, 50 },
    { 20, 36, 42, 20, 38, 44 },
    { 13, 27, 34, 13, 28, 34 },
    { 7, 17, 21,  8, 17, 21 }, // Photoshop 100%
    { 2,  5,  6,  2,  5,  6 }
};

int const BD16[11][6] =
{
    { 197, 203, 210, 202, 207, 213 },
    { 174, 188, 193, 180, 189, 196 },
    { 152, 167, 173, 156, 169, 174 },
    { 135, 152, 157, 137, 153, 158 },
    { 119, 137, 141, 119, 138, 142 },
    { 102, 120, 125, 100, 120, 124 },
    { 82,  98, 104,  79,  98, 103 },
    { 60,  76,  81,  58,  76,  81 },
    { 39,  52,  58,  36,  52,  58 },
    { 16,  27,  33,  14,  27,  33 },
    { 5,   8,   9,   4,   7,   8 }
};

int const BD16f[11][6] =
{
    { 148, 177, 171, 165, 187, 191 },
    { 133, 155, 153, 147, 172, 181 },
    { 114, 133, 138, 130, 157, 167 },
    { 97, 118, 120, 109, 137, 144 },
    { 76,  98, 103,  85, 115, 121 },
    { 63,  86,  91,  62,  96,  99 },
    { 46,  68,  71,  43,  73,  75 },
    { 29,  48,  52,  27,  48,  51 },
    { 16,  30,  35,  14,  29,  34 },
    { 8,  14,  17,   7,  13,  17 },
    { 3,   5,   7,   3,   5,   6 }
};

int const BD32f[11][6] =
{
    { 194, 206, 209, 204, 211, 217 },
    { 175, 187, 196, 186, 193, 205 },
    { 157, 170, 177, 167, 180, 190 },
    { 133, 152, 156, 144, 163, 168 },
    { 116, 138, 142, 117, 143, 148 },
    { 98, 120, 123,  96, 123, 126 },
    { 80,  99, 102,  78,  99, 102 },
    { 65,  79,  84,  63,  79,  84 },
    { 48,  61,  67,  45,  60,  66 },
    { 27,  41,  46,  24,  40,  45 },
    { 3,  22,  24,   2,  21,  22 }
};

} // QP namespace

/************************************************************************/
/*                    JPEGXRException()                                 */
/************************************************************************/

struct JPEGXRException final : std::exception
{
    ERR nErr; // jxrlib/image/sys/windowsmediaphoto.h
    JPEGXRException() = default;
    explicit JPEGXRException(ERR nErrorCode) : nErr(nErrorCode) {}

    char const* what() const GDAL_NOEXCEPT override
    {
        switch (nErr)
        {
        case WMP_errSuccess: return "Success";
        case WMP_errFail: return "Fail";
        case WMP_errNotYetImplemented: return "NotYetImplemented";
        case WMP_errAbstractMethod: return "AbstractMethod:";
        case WMP_errOutOfMemory: return "OutOfMemory";
        case WMP_errFileIO: return "FileIO";
        case WMP_errBufferOverflow: return "BufferOverflow";
        case WMP_errInvalidParameter: return "InvalidParameter";
        case WMP_errInvalidArgument: return "InvalidArgument";
        case WMP_errUnsupportedFormat: return "UnsupportedFormat";
        case WMP_errIncorrectCodecVersion: return "IncorrectCodecVersion";
        case WMP_errIndexNotFound: return "IndexNotFound";
        case WMP_errOutOfSequence: return "OutOfSequence:";
        case WMP_errNotInitialized: return "NotInitialized";
        case WMP_errMustBeMultipleOf16LinesUntilLastCall: return "MustBeMultipleOf16LinesUntilLastCall";
        case WMP_errPlanarAlphaBandedEncRequiresTempFile: return "PlanarAlphaBandedEncRequiresTempFile";
        case WMP_errAlphaModeCannotBeTranscoded: return "AlphaModeCannotBeTranscoded";
        case WMP_errIncorrectCodecSubVersion: return "IncorrectCodecSubVersion";
        default: return "Unknown";
        }
    }
};

/************************************************************************/
/* ==================================================================== */
/*                         JPEGXRDecoder                                */
/* ==================================================================== */
/************************************************************************/

struct JPEGXRDecoder final
{
    JPEGXRDecoder()
        : pCodecFactory(NULL), pImageDecode(NULL)
    {
    }

    ~JPEGXRDecoder()
    {
        if (pImageDecode)
            pImageDecode->Release(&pImageDecode);
        pImageDecode = NULL;

        if (pCodecFactory)
            pCodecFactory->Release(&pCodecFactory);
        pCodecFactory = NULL;
    }

    void Initialize(char const* pszFilename)
    {
        CPLAssert(!pCodecFactory);
        CPLAssert(!pImageDecode);

        /* -------------------------------------------------------------------- */
        /*      Create decoder                                                  */
        /* -------------------------------------------------------------------- */
        ERR nErr = PKCreateCodecFactory(&pCodecFactory, WMP_SDK_VERSION);
        if (nErr != WMP_errSuccess || !pCodecFactory)
            throw JPEGXRException(nErr);

        nErr = pCodecFactory->CreateDecoderFromFile(pszFilename, &pImageDecode);
        if (nErr != WMP_errSuccess || !pImageDecode)
            throw JPEGXRException(nErr);

        PKPixelInfo pi;
        GetPixelInfo(pi);

        // Alpha
        // 0: Decode without alpha channel
        // 1: Decode only alpha channel (TODO: unused, might be controlled with driver option)
        // 2: Decode image & alpha (default)
        if ((pi.grBit & PK_pixfmtHasAlpha) != 0)
            pImageDecode->WMP.wmiSCP.uAlphaMode = 2;
        else
            pImageDecode->WMP.wmiSCP.uAlphaMode = 0;

        /* -------------------------------------------------------------------- */
        /* Self-check understanding of jxrlib internals and data consistency.   */
        /* -------------------------------------------------------------------- */
#ifdef DEBUG
        CPLAssert(pi.cChannel == pi.uSamplePerPixel);
        CPLAssert(pImageDecode->WMP.wmiI.cfColorFormat == pi.cfColorFormat);
        CPLAssert(pImageDecode->WMP.wmiI.bRGB == ((pi.grBit & PK_pixfmtBGR) == 0));
        CPLAssert(pImageDecode->WMP.wmiI.cBitsPerUnit == pi.cbitUnit);
        CPLAssert(pImageDecode->WMP.wmiI.cBitsPerUnit == pi.uSamplePerPixel * pi.uBitsPerSample);
#endif

        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

    int GetBytesPerPixel() const
    {
        if (!pImageDecode || pImageDecode->WMP.wmiI.cBitsPerUnit == 0)
            throw JPEGXRException(WMP_errNotInitialized);

        return static_cast<int>(pImageDecode->WMP.wmiI.cBitsPerUnit) / 8;
    }

    int GetSamplePerPixel() const
    {
        if (!pImageDecode)
            throw JPEGXRException(WMP_errNotInitialized);

        PKPixelInfo pi;
        GetPixelInfo(pi);

        CPLAssert(pi.uSamplePerPixel == pi.cChannel);
        return static_cast<int>(pi.uSamplePerPixel);
    }

    void GetPixelFormat(PKPixelFormatGUID& pf) const
    {
        memset(&pf, 0, sizeof(pf));

        if (!pImageDecode)
            throw JPEGXRException(WMP_errNotInitialized);

        ERR nErr = pImageDecode->GetPixelFormat(pImageDecode, &pf);
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

    void GetPixelInfo(PKPixelInfo& pi) const
    {
        memset(&pi, 0, sizeof(pi));

        PKPixelFormatGUID pf;
        GetPixelFormat(pf);

        pi.pGUIDPixFmt = &pf;
        ERR nErr = PixelFormatLookup(&pi, LOOKUP_FORWARD);
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

    void GetSize(int& nXSize, int& nYSize) const
    {
        nXSize = nYSize = 0;

        if (!pImageDecode)
            throw JPEGXRException(WMP_errNotInitialized);

        I32 nRasterXSize{0}, nRasterYSize{0};
        ERR nErr = pImageDecode->GetSize(pImageDecode, &nRasterXSize, &nRasterYSize);
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);

        nXSize = static_cast<int>(nRasterXSize);
        nYSize = static_cast<int>(nRasterYSize);
    }

    void SetVerbose(bool bVerbose)
    {
        if (pImageDecode)
            pImageDecode->WMP.wmiSCP.bVerbose = bVerbose;
    }

    void Read(GByte* pabyData, int nXOff, int nYOff, int nXSize, int nYSize)
    {
        if (!pImageDecode)
            throw JPEGXRException(WMP_errNotInitialized);
        if (!pabyData)
            throw JPEGXRException(WMP_errInvalidArgument);

        int const nBytesPerPixel = GetBytesPerPixel();
        U32 nStride = static_cast<U32>(nXSize) * nBytesPerPixel;

        PKRect rc = {0, 0, 0, 0};
        rc.X = nXOff;
        rc.Y = nYOff;
        rc.Width = nXSize;
        rc.Height = nYSize;

        ERR nErr = pImageDecode->Copy(pImageDecode, &rc, pabyData, nStride);
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

private:
    PKCodecFactory* pCodecFactory;
    PKImageDecode* pImageDecode;
};

/************************************************************************/
/* ==================================================================== */
/*                         JPEGXREncoderConfig                          */
/* ==================================================================== */
/************************************************************************/
namespace
{

enum Quality
{
    QUALITY_UNSET = -1,
    QUALITY_LOWEST = 0,
    QUALITY_CUSTOM = 1,
    QUALITY_LOSSLESS = 100
};
enum Overlap
{
    OVERLAP_UNSET = -1,
    OVERLAP_NONE = OL_NONE,
    OVERLAP_ONE = OL_ONE, // default
    OVERLAP_TWO = OL_TWO,
    OVERLAP_COUNT
};

enum  Subsampling
{
    SUBSAMPLING_UNSET = -1,
    SUBSAMPLING_YONLY,
    SUBSAMPLING_420,
    SUBSAMPLING_422,
    SUBSAMPLING_444, // default
    SUBSAMPLING_COUNT
};

} // unnamed namespace

struct JPEGXREncoderConfig final
{
    Quality GetQuality() const
    {
        if (eQuality == QUALITY_UNSET)
        {
            const_cast<JPEGXREncoderConfig*>(this)->eQuality = QUALITY_LOSSLESS;
            const_cast<JPEGXREncoderConfig*>(this)->fQuality = 1.0f;
        }
        CPLAssert(eQuality != QUALITY_UNSET);
        return eQuality;
    }

    Overlap GetOverlap() const
    {
        if (eOverlap == OVERLAP_UNSET)
        {
            JPEGXREncoderConfig* pThis = const_cast<JPEGXREncoderConfig*>(this);
            if (GetQuality() == QUALITY_LOSSLESS)
            {
                pThis->eOverlap = OVERLAP_NONE;
            }
            else
            {
                // Image width must be at least 2 MB wide for subsampled chroma and
                // two levels of overlap
                if (fQuality >= 0.5F /*|| pImageEncode->uWidth < 2 * MB_WIDTH_PIXEL*/)
                    pThis->eOverlap = OVERLAP_ONE;
                else
                    pThis->eOverlap = OVERLAP_TWO;
            }
        }
        CPLAssert(eOverlap != OVERLAP_UNSET);
        return eOverlap;
    }

    OVERLAP GetOVERLAP() const
    {
        Overlap ol = GetOverlap();
        if (ol > OVERLAP_UNSET && ol < OVERLAP_COUNT)
        {
            return OVERLAP(static_cast<int>(ol));
        }

        throw JPEGXRException(WMP_errInvalidParameter);
    }

    Subsampling GetSubsampling() const
    {
        if (eSubsampling == SUBSAMPLING_UNSET)
            const_cast<JPEGXREncoderConfig*>(this)->eSubsampling = SUBSAMPLING_444;
        CPLAssert(eSubsampling != SUBSAMPLING_UNSET);
        return eSubsampling;
    }

    void SetQuality(int n)
    {
        CPLAssert(QUALITY_LOWEST <= n && n <= QUALITY_LOSSLESS);
        if (QUALITY_LOWEST <= n && n <= QUALITY_LOSSLESS)
        {
            fQuality = n / 100.0f;

            if (fQuality > QUALITY_LOWEST && fQuality < QUALITY_LOSSLESS)
                eQuality = QUALITY_CUSTOM;
        }
    }

    void SetOverlap(int n)
    {
        CPLAssert(OVERLAP_UNSET < n && n < OVERLAP_COUNT);
        if (OVERLAP_UNSET < n && n < OVERLAP_COUNT)
            eOverlap = static_cast<Overlap>(n);
    }

    void SetSubsampling(int n)
    {
        CPLAssert(SUBSAMPLING_420 <= n && n < SUBSAMPLING_COUNT);
        if (SUBSAMPLING_420 <= n && n < SUBSAMPLING_COUNT)
            eSubsampling = static_cast<Subsampling>(n);
    }

public:
    float fQuality = 1.0;

private:
    Quality eQuality = QUALITY_UNSET;
    Overlap eOverlap = OVERLAP_UNSET;
    Subsampling eSubsampling = SUBSAMPLING_UNSET;
};

/************************************************************************/
/* ==================================================================== */
/*                         JPEGXREncoder                                */
/* ==================================================================== */
/************************************************************************/

struct JPEGXREncoder final
{
    friend struct JPEGXREncoderConfig;

    JPEGXREncoder()
        : pFactory(NULL)
        , pCodecFactory(NULL)
        , pImageEncode(NULL)
        , pEncodeStream(NULL)
    {
    }

    ~JPEGXREncoder()
    {
        // No need to release pEncodeStream.
        // The encoder owns the stream and PKImageEncode_Release releases it.
        if (pImageEncode)
            pImageEncode->Release(&pImageEncode); // also releases attached stream
        pImageEncode = NULL;

        if (pCodecFactory)
            pCodecFactory->Release(&pCodecFactory);
        pCodecFactory = NULL;

        if (pFactory)
            pFactory->Release(&pFactory);
        pFactory = NULL;
    }

    void Initialize(char const* pszFilename)
    {
        CPLAssert(!pFactory);
        CPLAssert(!pCodecFactory);
        CPLAssert(!pImageEncode);

        /* -------------------------------------------------------------------- */
        /*      Create encoding stream                                          */
        /* -------------------------------------------------------------------- */
        ERR nErr = PKCreateFactory(&pFactory, PK_SDK_VERSION);
        if (nErr != WMP_errSuccess || !pFactory)
            throw JPEGXRException(nErr);

        nErr = pFactory->CreateStreamFromFilename(&pEncodeStream, pszFilename, "wb");
        if (nErr != WMP_errSuccess || !pEncodeStream)
            throw JPEGXRException(nErr);

        /* -------------------------------------------------------------------- */
        /*      Create encoder                                                  */
        /* -------------------------------------------------------------------- */
        nErr = PKCreateCodecFactory(&pCodecFactory, WMP_SDK_VERSION);
        if (nErr != WMP_errSuccess || !pCodecFactory)
            throw JPEGXRException(nErr);

        nErr = pCodecFactory->CreateCodec(&IID_PKImageWmpEncode,
                                          reinterpret_cast<void**>(&pImageEncode));
        if (nErr != WMP_errSuccess || !pImageEncode)
            throw JPEGXRException(nErr);

        /* -------------------------------------------------------------------- */
        /*      Set default encoding parameters                                 */
        /* -------------------------------------------------------------------- */
        CPLAssert(pImageEncode);
        CPLAssert(pEncodeStream);

        CWMIStrCodecParam wmiSCP;
        memset(&wmiSCP, 0, sizeof(CWMIStrCodecParam));
        wmiSCP.bVerbose = FALSE;
        wmiSCP.bProgressiveMode = FALSE;
        wmiSCP.bdBitDepth = BD_LONG;
        wmiSCP.bfBitstreamFormat = FREQUENCY;
        wmiSCP.cfColorFormat = YUV_444;
        wmiSCP.uAlphaMode = 0;
        wmiSCP.cNumOfSliceMinus1H = 0;
        wmiSCP.cNumOfSliceMinus1V = 0;
        wmiSCP.olOverlap = OL_NONE;
        wmiSCP.sbSubband = SB_ALL;
        wmiSCP.uiDefaultQPIndex = 1;
        wmiSCP.uiDefaultQPIndexAlpha = 1;
        // NOTE: Post-Initialize modifications of the wmiSCP is possible via pImageEncode->WMP.wmiSCP

        nErr = pImageEncode->Initialize(pImageEncode, pEncodeStream, &wmiSCP, sizeof(wmiSCP));
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);

        // Defaults from generic PKImageEncode_Initialize (JXRGlue.c)
        pImageEncode->cFrame = 1;
        pImageEncode->fResX = 96;
        pImageEncode->fResY = 96;

        /* -------------------------------------------------------------------- */
        /* Self-check understanding of jxrlib internals and data consistency.   */
        /* -------------------------------------------------------------------- */
#ifdef DEBUG
        CPLAssert(pImageEncode->bWMP);
        CPLAssert(pImageEncode->pStream);
        CPLAssert(pImageEncode->guidPixFormat == GUID{});
        CPLAssert(pImageEncode->cFrame == 1);
        CPLAssert(pImageEncode->fResX == 96);
        CPLAssert(pImageEncode->fResY == 96);
#endif

        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

    void SetPixelFormat(PKPixelFormatGUID const& pf)
    {
        if (!pImageEncode)
            throw JPEGXRException(WMP_errNotInitialized);

        if (IsEqualGUID(pf, GUID_PKPixelFormatBlackWhite) ||
            IsEqualGUID(pf, GUID_PKPixelFormat8bppGray))
        {
            pImageEncode->WMP.wmiSCP.cfColorFormat = Y_ONLY;
        }
        else
        {
            pImageEncode->WMP.wmiSCP.cfColorFormat = YUV_444;
        }

        ERR nErr = pImageEncode->SetPixelFormat(pImageEncode, pf);
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

    void SetResolution(float fResX, float fResY)
    {
        if (!pImageEncode)
            throw JPEGXRException(WMP_errNotInitialized);

        ERR nErr = pImageEncode->SetResolution(pImageEncode, fResX, fResY);
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

    void SetSize(int nXSize, int nYSize)
    {
        if (!pImageEncode)
            throw JPEGXRException(WMP_errNotInitialized);

        ERR nErr = pImageEncode->SetSize(pImageEncode, nXSize, nYSize);
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

    void SetVerbose(bool bVerbose)
    {
        if (pImageEncode)
            pImageEncode->WMP.wmiSCP.bVerbose = bVerbose;
    }

    void Finalize(JPEGXREncoderConfig const& config)
    {
        CPLAssert(pImageEncode);

        // Update default codec parameters with any user-defined configuration/options.
        CWMIStrCodecParam& wmiSCP = pImageEncode->WMP.wmiSCP;

        // Calculate image quality / quantization parameters
        if (config.GetQuality() == QUALITY_LOSSLESS)
        {
            wmiSCP.uiDefaultQPIndex = 1;
        }
        else
        {
            PKPixelInfo pi;
            memset(&pi, 0, sizeof(PKPixelInfo));
            pi.pGUIDPixFmt = &pImageEncode->guidPixFormat;
            ERR nErr = PixelFormatLookup(&pi, LOOKUP_FORWARD);
            if (nErr != WMP_errSuccess)
                throw JPEGXRException(nErr);

            wmiSCP.olOverlap = config.GetOVERLAP();

            if (IsEqualGUID(pImageEncode->guidPixFormat, GUID_PKPixelFormatBlackWhite))
            {
                // TODO: B&W to be tested
                //CPLAssert(pi.bdBitDepth == BD_1);
                //wmiSCP->uiDefaultQPIndex = (U8)(8 - 5.0f * iq_float + 0.5f);
                throw JPEGXRException(WMP_errUnsupportedFormat);
            }
            else
            {
                // Microsoft in JxrEncApp.c noted:
                // Remap [0.8, 0.866, 0.933, 1.0] to [0.8, 0.9, 1.0, 1.1]
                // to use 8-bit QP table (0.933 == Photoshop JPEG 100)
                float fQuality = config.fQuality;
                if (fQuality > 0.8f &&
                    pi.bdBitDepth == BD_8 &&
                    wmiSCP.cfColorFormat != YUV_420 &&
                    wmiSCP.cfColorFormat != YUV_422)
                {
                    fQuality = 0.8f + (fQuality - 0.8f) * 1.5f;
                }

                int const qi = static_cast<int>(10.f * fQuality);
                float const qf = 10.f * fQuality - static_cast<float>(qi);
                int const* pQP = NULL;
                if (wmiSCP.cfColorFormat == YUV_420 || wmiSCP.cfColorFormat == YUV_422)
                {
                    pQP = QP::BD8_420[qi];
                }
                else
                {
                    if (pi.bdBitDepth == BD_8)
                        pQP = QP::BD8[qi];
                    else if (pi.bdBitDepth == BD_16)
                        pQP = QP::BD16[qi];
                    else if (pi.bdBitDepth == BD_16F)
                        pQP = QP::BD16f[qi];
                    else
                        pQP = QP::BD32f[qi];
                }
                CPLAssert(pQP);

                wmiSCP.uiDefaultQPIndex    = (U8)(0.5f + pQP[0] * (1.f - qf) + (pQP + 6)[0] * qf);
                wmiSCP.uiDefaultQPIndexU   = (U8)(0.5f + pQP[1] * (1.f - qf) + (pQP + 6)[1] * qf);
                wmiSCP.uiDefaultQPIndexV   = (U8)(0.5f + pQP[2] * (1.f - qf) + (pQP + 6)[2] * qf);
                wmiSCP.uiDefaultQPIndexYHP = (U8)(0.5f + pQP[3] * (1.f - qf) + (pQP + 6)[3] * qf);
                wmiSCP.uiDefaultQPIndexUHP = (U8)(0.5f + pQP[4] * (1.f - qf) + (pQP + 6)[4] * qf);
                wmiSCP.uiDefaultQPIndexVHP = (U8)(0.5f + pQP[5] * (1.f - qf) + (pQP + 6)[5] * qf);
            }
        }

        // TODO: Tiling
    }

    void Write(GByte /*const*/ *pabyData, int nXStride, int nYSize)
    {
        if (!pImageEncode)
            throw JPEGXRException(WMP_errNotInitialized);

        U32 const cLine = static_cast<U32>(nYSize);
        U32 const cbStride = static_cast<U32>(nXStride);

        ERR nErr = pImageEncode->WritePixels(pImageEncode,
                                             cLine,
                                             pabyData,
                                             cbStride);
        if (nErr != WMP_errSuccess)
            throw JPEGXRException(nErr);
    }

private:
    PKFactory* pFactory;
    PKCodecFactory* pCodecFactory;
    PKImageEncode* pImageEncode;
    WMPStream* pEncodeStream;
};


/************************************************************************/
/* ==================================================================== */
/*                           JPEGXRDataset                              */
/* ==================================================================== */
/************************************************************************/

class JPEGXRDataset final : public GDALPamDataset
{
    friend class JPEGXRRasterBand;
private:
    JPEGXRDecoder oDecoder;
    GByte* pabyUncompressedData;
    bool bHasUncompressed;

    CPLErr Uncompress();

public:
    JPEGXRDataset();
    ~JPEGXRDataset();

    static int          Identify(GDALOpenInfo*);
    static GDALDataset* Open(GDALOpenInfo*);
    static GDALDataset* CreateCopy(char const* pszFilename,
                                   GDALDataset* poSrcDS,
                                   int bStrict,
                                   char** papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void* pProgressData);
};

/************************************************************************/
/* ==================================================================== */
/*                         JPEGXRRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class JPEGXRRasterBand final : public GDALPamRasterBand
{
    friend class JPEGXRDataset;

  public:

    JPEGXRRasterBand(JPEGXRDataset* poDS, int nBand);

    CPLErr          IReadBlock(int, int, void*) override;
    GDALColorInterp GetColorInterpretation() override;
};


/************************************************************************/
/*                        JPEGXRRasterBand()                            */
/************************************************************************/

JPEGXRRasterBand::JPEGXRRasterBand(JPEGXRDataset* poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    nRasterXSize = poDSIn->nRasterXSize;
    nRasterYSize = poDSIn->nRasterYSize;
    nBlockXSize = nRasterXSize;
    nBlockYSize = nRasterYSize;
    eDataType = GDT_Byte; // TODO
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr JPEGXRRasterBand::IReadBlock(int /*nBlockXOff*/, int /*nBlockYOff*/,
                                    void* pImage)
{
    JPEGXRDataset* poJDS = static_cast<JPEGXRDataset*>(poDS);

    if (!poJDS->bHasUncompressed)
    {
        CPLErr eErr = poJDS->Uncompress();
        if (eErr != CE_None)
            return eErr;
    }

    if (!poJDS->pabyUncompressedData)
        return CE_Failure;

    if (eDataType == GDT_Byte)
    {
        for(int j=0;j<nBlockYSize;j++)
        {
            for(int i=0;i<nBlockXSize;i++)
            {
                ((GByte*)pImage)[j * nBlockXSize + i] =
                    poJDS->pabyUncompressedData[
                            poJDS->nBands * (j * nBlockXSize + i) + nBand - 1];
            }
        }
    }
    else
    {
        CPLAssert(!"TODO");
    }

#ifdef JPEGXR_DEBUG
    {
        int nBytesPerBand = nRasterXSize * nRasterYSize * GDALGetDataTypeSizeBytes(GetRasterDataType());
        char* pszHex = CPLBinaryToHex(nBytesPerBand, (const GByte*)pImage);
        CPLDebug("JPEGXR", pszHex);
        CPLFree(pszHex);
    }
#endif

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp JPEGXRRasterBand::GetColorInterpretation()
{
    try
    {
        JPEGXRDataset* poJDS = static_cast<JPEGXRDataset*>(poDS);
        JPEGXRDecoder& oDecoder = poJDS->oDecoder;

        PKPixelInfo pi;
        oDecoder.GetPixelInfo(pi);

        CPLAssert(static_cast<int>(pi.cChannel) == poJDS->nBands);

        switch (poJDS->nBands)
        {
        case 1:
        {
            CPLAssert(pi.cfColorFormat == Y_ONLY);
            CPLAssert(pi.uInterpretation == PK_PI_B0 || pi.uInterpretation == PK_PI_W0); // TIFF
            return GCI_GrayIndex;
        }
        case 3:
        case 4:
        {
            if (pi.cfColorFormat == CF_RGB)
            {
                CPLAssert(pi.uInterpretation == PK_PI_RGB);
                if (pi.grBit & PK_pixfmtBGR)
                {
                    CPLAssert(IsEqualGUID(*pi.pGUIDPixFmt, GUID_PKPixelFormat24bppBGR)
                           || IsEqualGUID(*pi.pGUIDPixFmt, GUID_PKPixelFormat32bppBGRA));
                    // BGR
                    switch (nBand)
                    {
                    case 1: return GCI_BlueBand;
                    case 2: return GCI_GreenBand;
                    case 3: return GCI_RedBand;
                    case 4:
                    {
                        if (pi.grBit & PK_pixfmtHasAlpha)
                            return GCI_AlphaBand;
                        break;
                    }
                    default: CPLAssert(0);
                    }
                }
                else
                {
                    CPLAssert(IsEqualGUID(*pi.pGUIDPixFmt, GUID_PKPixelFormat24bppRGB)
                           || IsEqualGUID(*pi.pGUIDPixFmt, GUID_PKPixelFormat32bppRGBA));

                    // RGB
                    switch (nBand)
                    {
                    case 1: return GCI_RedBand;
                    case 2: return GCI_GreenBand;
                    case 3: return GCI_BlueBand;
                    case 4:
                    {
                        if (pi.grBit & PK_pixfmtHasAlpha)
                            return GCI_AlphaBand;
                        break;
                    }
                    default: CPLAssert(0);
                    }
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                    "JPEGXR - YUV or CMYK color format not yet unsupported.");
            }
        }
        }
    }
    catch (JPEGXRException const& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "JPEGXR - Color interpretation access failed : %s", e.what());
    }
    return GCI_Undefined;
}

/************************************************************************/
/*                        JPEGXRDataset()                               */
/************************************************************************/

JPEGXRDataset::JPEGXRDataset()
    : pabyUncompressedData(NULL)
    , bHasUncompressed(false)
{
}

/************************************************************************/
/*                       ~JPEGXRDataset()                               */
/************************************************************************/

JPEGXRDataset::~JPEGXRDataset()
{
    VSIFree(pabyUncompressedData);
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset* JPEGXRDataset::CreateCopy(char const* pszFilename,
                                       GDALDataset* poSrcDS,
                                       int bStrict,
                                       char** papszOptions,
                                       GDALProgressFunc /*pfnProgress*/,
                                       void* /*pProgressData*/)
{
    int const nBands = poSrcDS->GetRasterCount();
    int const nXSize = poSrcDS->GetRasterXSize();
    int const nYSize = poSrcDS->GetRasterYSize();

    /* -------------------------------------------------------------------- */
    /*      Some some rudimentary checks                                    */
    /* -------------------------------------------------------------------- */
    if (nBands != 1 && nBands != 3 && nBands != 4)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "JPEGXR driver doesn't support %d bands.  Must be 1 (grey), "
            "3 (RGB) or 4 bands.\n", nBands);
        return NULL;
    }

    GDALDataType const eDT = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    if (eDT != GDT_Byte) // TODO
    {
        CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
            "JPEGXR driver doesn't support data type %s",
            GDALGetDataTypeName(
                poSrcDS->GetRasterBand(1)->GetRasterDataType()));

        if (bStrict)
            return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Collect configuration options                                   */
    /* -------------------------------------------------------------------- */
    JPEGXREncoderConfig config; // sets sensible defaults

    char const* pszValue = CSLFetchNameValue(papszOptions, "QUALITY");
    if (pszValue)
    {
        int n = atoi(pszValue);
        if (n < QUALITY_LOWEST || n > QUALITY_LOSSLESS)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                "QUALITY=%s is not a legal value in the range %d-%d.",
                pszValue, QUALITY_LOWEST, QUALITY_LOSSLESS);
            return NULL;
        }
        config.SetQuality(n);
    }

    pszValue = CSLFetchNameValue(papszOptions, "OVERLAP");
    if (pszValue)
    {
        int n = atoi(pszValue);
        if (OVERLAP_NONE < 0 || n > OVERLAP_COUNT)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                "OVERLAP=%s is not a legal value in the range %d-%d.",
                pszValue, OVERLAP_NONE, OVERLAP_COUNT-1);
            return NULL;
        }
        config.SetOverlap(n);
    }

    pszValue = CSLFetchNameValue(papszOptions, "SUBSAMPLING");
    if (pszValue)
    {
        int n = atoi(pszValue);
        if (SUBSAMPLING_420 < 0 || n > SUBSAMPLING_COUNT)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                "SUBSAMPLING=%s is not a legal value in the range %d-%d.",
                pszValue, SUBSAMPLING_420, SUBSAMPLING_COUNT-1);
            return NULL;
        }
        config.SetSubsampling(n);
    }
    /* -------------------------------------------------------------------- */
    /*      Read source data                                                */
    /* -------------------------------------------------------------------- */
    int nWordSize = GDALGetDataTypeSizeBytes(eDT);
    int nUncompressedSize = nXSize * nYSize * nBands * nWordSize;
    GByte* pabyUncompressedData = (GByte*)VSI_MALLOC_VERBOSE(nUncompressedSize);
    if (!pabyUncompressedData)
    {
        VSIFree(pabyUncompressedData);
        return NULL;
    }

    CPLErr eErr;
    eErr = poSrcDS->RasterIO(GF_Read, 0, 0, nXSize, nYSize,
        pabyUncompressedData, nXSize, nYSize,
        eDT, nBands, NULL,
        nBands * nWordSize, nBands * nWordSize * nXSize, nWordSize, NULL);
    if (eErr != CE_None)
    {
        VSIFree(pabyUncompressedData);
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Setup encoder                                                   */
    /* -------------------------------------------------------------------- */
    try
    {
        JPEGXREncoder oEncoder;
        oEncoder.Initialize(pszFilename);
        oEncoder.SetVerbose(false); // TODO: option?
        oEncoder.SetSize(nXSize, nYSize);
        oEncoder.SetResolution(72, 72);

        if (nBands == 1 && poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_GrayIndex)
        {
            oEncoder.SetPixelFormat(GUID_PKPixelFormat8bppGray);
        }
        else if (nBands == 3 || nBands == 4)
        {
            // RGB
            if (poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_RedBand &&
                poSrcDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
                poSrcDS->GetRasterBand(3)->GetColorInterpretation() == GCI_BlueBand)
            {
                if (nBands == 4 && poSrcDS->GetRasterBand(4)->GetColorInterpretation() == GCI_AlphaBand)
                    oEncoder.SetPixelFormat(GUID_PKPixelFormat32bppRGBA);
                else
                    oEncoder.SetPixelFormat(GUID_PKPixelFormat32bppRGB);
            }
            // BGR
            else if (
                poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_BlueBand &&
                poSrcDS->GetRasterBand(2)->GetColorInterpretation() == GCI_GreenBand &&
                poSrcDS->GetRasterBand(3)->GetColorInterpretation() == GCI_RedBand)
            {
                if (nBands == 4 && poSrcDS->GetRasterBand(4)->GetColorInterpretation() == GCI_AlphaBand)
                    oEncoder.SetPixelFormat(GUID_PKPixelFormat32bppBGRA);
                else
                    oEncoder.SetPixelFormat(GUID_PKPixelFormat24bppBGR);
            }
        }
        oEncoder.Finalize(config);

        oEncoder.Write(pabyUncompressedData, nXSize * nBands, nYSize);
        VSIFree(pabyUncompressedData);
    }
    catch (JPEGXRException const& e)
    {
        VSIFree(pabyUncompressedData);
        CPLError(CE_Failure, CPLE_AppDefined,
            "JPEGXR - Encoding failed : %s", e.what());
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Re-open dataset, and copy any auxiliary pam information.        */
    /* -------------------------------------------------------------------- */

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    JPEGXRDataset *poDS = static_cast<JPEGXRDataset*>(JPEGXRDataset::Open(&oOpenInfo));
    if (poDS)
    {
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT & (~GCIF_METADATA));
    }
    return poDS;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int JPEGXRDataset::Identify(GDALOpenInfo * poOpenInfo)
{
    GByte* pabyHeader = poOpenInfo->pabyHeader;
    int nHeaderBytes = poOpenInfo->nHeaderBytes;

    if (nHeaderBytes < 4)
        return FALSE;

    // JPEG XR signature of file created by
    // - pre-release encoder (Version 0) is 0x4949bc00
    // - released encoder (Version 1) is 0x4949bc01

    if (pabyHeader[0] != 0x49
        || pabyHeader[1] != 0x49
        || pabyHeader[2] != 0xbc)
        return FALSE;

    if (pabyHeader[3] != 0x00 && pabyHeader[3] != 0x01)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

GDALDataset *JPEGXRDataset::Open( GDALOpenInfo * poOpenInfo)
{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == NULL)
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    JPEGXRDataset* poDS = new JPEGXRDataset();
    JPEGXRDecoder& oDecoder = poDS->oDecoder;

    int nBands = 0;

/* -------------------------------------------------------------------- */
/*      Initialize decoder and describe source image.                   */
/* -------------------------------------------------------------------- */
    try
    {
        oDecoder.Initialize(poOpenInfo->pszFilename);
        oDecoder.GetSize(poDS->nRasterXSize, poDS->nRasterYSize);
        nBands = oDecoder.GetSamplePerPixel();
    }
    catch (JPEGXRException const& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "JPEGXR - Decoder initialization failed : %s", e.what());
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    CPLAssert(nBands > 0);

    for (int iBand = 1; iBand <= nBands; iBand++)
    {
        poDS->SetBand(iBand, new JPEGXRRasterBand(poDS, iBand));
    }

    CPLAssert(poDS->nBands == nBands);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    return poDS;
}

/************************************************************************/
/*                             Uncompress()                             */
/************************************************************************/

CPLErr JPEGXRDataset::Uncompress()
{
    try
    {
        if (bHasUncompressed)
            return CE_None;

        size_t nUncompressedSize = nRasterXSize * nRasterYSize * nBands
            * GDALGetDataTypeSizeBytes(GetRasterBand(1)->GetRasterDataType());
        pabyUncompressedData = (GByte*)VSI_MALLOC_VERBOSE(nUncompressedSize);
        if (!pabyUncompressedData)
            return CE_Failure;

        oDecoder.Read(pabyUncompressedData, 0, 0, nRasterXSize, nRasterYSize);

        bHasUncompressed = true;
        return CE_None;
    }
    catch (JPEGXRException const& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "JPEGXR - Decompression of data failed : %s", e.what());
    }
    return CE_Failure;
}

/************************************************************************/
/*                          GDALRegister_JPEGXR()                       */
/************************************************************************/

void GDALRegister_JPEGXR()

{
    if(GDALGetDriverByName("JPEGXR") != NULL)
        return;

    GDALDriver* poDriver = new GDALDriver();

    poDriver->SetDescription( "JPEGXR" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "JPEG XR driver based on jxrlib library" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_jpegxr.html" );
    // NOTE: The HD Photo format is a pre-standard implementation of the JPEG XR format.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/gg430023.aspx
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, ".jxr .hdp .wdp" );
    poDriver->SetMetadataItem( GDAL_DMD_MIMETYPE, "image/vnd.ms-photo" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" ); // TODO
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='QUALITY' type='int'/>\n"
        "   <Option name='OVERLAP' type='int'/>\n"
        "   <Option name='SUBSAMPLING' type='int'/>\n"
        "</CreationOptionList>\n" );
    poDriver->pfnIdentify = JPEGXRDataset::Identify;
    poDriver->pfnOpen = JPEGXRDataset::Open;
    poDriver->pfnCreateCopy = JPEGXRDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
