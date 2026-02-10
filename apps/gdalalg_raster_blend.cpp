/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "blend" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2009, Frank Warmerdam

 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_blend.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <utility>

#if defined(__x86_64) || defined(_M_X64)
#define HAVE_SSE2
#include <immintrin.h>
#endif
#ifdef HAVE_SSE2
#include "gdalsse_priv.h"
#endif

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                           CompositionModes                           */
/************************************************************************/
std::map<CompositionMode, std::string> CompositionModes()
{
    return {
        {CompositionMode::SRC_OVER, "src-over"},
        {CompositionMode::HSV_VALUE, "hsv-value"},
        {CompositionMode::MULTIPLY, "multiply"},
        {CompositionMode::SCREEN, "screen"},
        {CompositionMode::OVERLAY, "overlay"},
        {CompositionMode::HARD_LIGHT, "hard-light"},
        {CompositionMode::DARKEN, "darken"},
        {CompositionMode::LIGHTEN, "lighten"},
        {CompositionMode::COLOR_BURN, "color-burn"},
        {CompositionMode::COLOR_DODGE, "color-dodge"},
    };
}

/************************************************************************/
/*                      CompositionModeToString()                       */
/************************************************************************/

std::string CompositionModeToString(CompositionMode mode)
{
    const auto &modes = CompositionModes();
    const auto &iter = modes.find(mode);
    if (iter != modes.end())
    {
        return iter->second;
    }
    CPLError(CE_Failure, CPLE_IllegalArg,
             "Invalid composition mode value: %d, returning 'src-over'",
             static_cast<int>(mode));
    return "src-over";
}

/************************************************************************/
/*                    CompositionModesIdentifiers()                     */
/************************************************************************/

std::vector<std::string> CompositionModesIdentifiers()
{
    const auto &modes = CompositionModes();
    std::vector<std::string> identifiers;
    for (const auto &pair : modes)
    {
        identifiers.push_back(pair.second);
    }
    return identifiers;
}

/************************************************************************/
/*                     CompositionModeFromString()                      */
/************************************************************************/

CompositionMode CompositionModeFromString(const std::string &str)
{
    const auto &modes = CompositionModes();
    auto iter =
        std::find_if(modes.begin(), modes.end(),
                     [&str](const auto &pair) { return pair.second == str; });
    if (iter != modes.end())
    {
        return iter->first;
    }
    CPLError(CE_Failure, CPLE_IllegalArg,
             "Invalid composition identifier: %s, returning SRC_OVER",
             str.c_str());
    return CompositionMode::SRC_OVER;
}

/************************************************************************/
/*                   MinBandCountForCompositionMode()                   */
/************************************************************************/

//! Returns the minimum number of bands required for the given composition mode
int MinBandCountForCompositionMode(CompositionMode mode)
{
    switch (mode)
    {
        case CompositionMode::HSV_VALUE:
            return 3;
        case CompositionMode::SRC_OVER:
        case CompositionMode::MULTIPLY:
        case CompositionMode::SCREEN:
        case CompositionMode::OVERLAY:
        case CompositionMode::HARD_LIGHT:
        case CompositionMode::DARKEN:
        case CompositionMode::LIGHTEN:
        case CompositionMode::COLOR_BURN:
        case CompositionMode::COLOR_DODGE:
            return 1;
    }
    // unreachable...
    return 1;
}

/************************************************************************/
/*                   MaxBandCountForCompositionMode()                   */
/************************************************************************/

/**
 *  Returns the maximum number of bands allowed for the given composition mode
 */
int MaxBandCountForCompositionMode(CompositionMode mode)
{
    switch (mode)
    {
        case CompositionMode::SRC_OVER:
        case CompositionMode::HSV_VALUE:
        case CompositionMode::MULTIPLY:
        case CompositionMode::SCREEN:
        case CompositionMode::OVERLAY:
        case CompositionMode::HARD_LIGHT:
        case CompositionMode::DARKEN:
        case CompositionMode::LIGHTEN:
        case CompositionMode::COLOR_BURN:
        case CompositionMode::COLOR_DODGE:
            return 4;
    }
    // unreachable...
    return 4;
}

/************************************************************************/
/*              BandCountIsCompatibleWithCompositionMode()              */
/************************************************************************/

//! Checks whether the number of bands is compatible with the given composition mode
bool BandCountIsCompatibleWithCompositionMode(int bandCount,
                                              CompositionMode mode)
{
    const int minBands = MinBandCountForCompositionMode(mode);
    const int maxBands = MaxBandCountForCompositionMode(mode);
    return minBands <= bandCount && bandCount <= maxBands;
}

/************************************************************************/
/*                            MulScale255()                             */
/************************************************************************/

/** Multiply 2 bytes considering them as ratios with 255 = 100%, and return their product unscaled to [0, 255], by ceiling */
inline GByte MulScale255(GByte a, GByte b)
{
    return static_cast<GByte>((a * b + 255) / 256);
}

/************************************************************************/
/*                         ProcessAlphaChannels                         */
/************************************************************************/

static inline void ProcessAlphaChannels(size_t i,
                                        const GByte *CPL_RESTRICT pabyA,
                                        const GByte *CPL_RESTRICT pabyOverlayA,
                                        int nOpacity, bool bSwappedOpacity,
                                        GByte &outA, GByte &outOverlaA,
                                        GByte &outFinalAlpha)
{
    // Apply opacity depending on whether overlay and base were swapped
    const GByte byOpacity = static_cast<GByte>(nOpacity);
    if (!bSwappedOpacity)
    {
        outOverlaA =
            pabyOverlayA ? MulScale255(pabyOverlayA[i], byOpacity) : byOpacity;

        outA = pabyA ? pabyA[i] : 255;
    }
    else
    {
        outOverlaA = pabyOverlayA ? pabyOverlayA[i] : 255;
        if (outOverlaA != 255)
        {
            outOverlaA = pabyOverlayA ? pabyOverlayA[i] : 255;
        }
        outA = pabyA ? MulScale255(pabyA[i], byOpacity) : byOpacity;
    }

    // Da'  = Sa + Da - Sa.Da
    outFinalAlpha =
        static_cast<GByte>(outOverlaA + outA - MulScale255(outOverlaA, outA));
}

/************************************************************************/
/*                            DivScale255()                             */
/************************************************************************/

/** Divide 2 bytes considering them as ratios with 255 = 100%, and return their quotient unscaled to [0, 255], by flooring
 *  \warning Caution: this function does not check that the result actually fits on a byte, and just casts the computed value to byte.
 */
inline GByte DivScale255(GByte a, GByte b)
{
    if (a == 0)
    {
        return 0;
    }
    else if (b == 0)
    {
        return 255;
    }
    else
    {
        const int nRes = (a * 255) / b;
#ifdef DEBUG
        CPLAssert(nRes <= 255);
#endif
        return static_cast<GByte>(nRes);
    }
}

/************************************************************************/
/*                         PremultiplyChannels                          */
/************************************************************************/

//! Premultiply RGB channels by alpha (A)
static inline void PremultiplyChannels(size_t i, const GByte *pabyR,
                                       const GByte *pabyG, const GByte *pabyB,
                                       GByte &outR, GByte &outG, GByte &outB,
                                       const GByte &A)

{
    if (A == 255)
    {
        outR = pabyR ? pabyR[i] : 255;
        outG = pabyG ? pabyG[i] : outR;  // in case only R is present
        outB = pabyB ? pabyB[i] : outR;  // in case only R is present
    }
    else
    {
        outR = pabyR ? MulScale255(pabyR[i], A) : A;
        outG = pabyG ? MulScale255(pabyG[i], A)
                     : outR;  // in case only R is present
        outB = pabyB ? MulScale255(pabyB[i], A)
                     : outR;  // in case only R is present
    }
}

/************************************************************************/
/*         GDALRasterBlendAlgorithm::GDALRasterBlendAlgorithm()         */
/************************************************************************/

GDALRasterBlendAlgorithm::GDALRasterBlendAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetAddDefaultArguments(false)
              .SetInputDatasetHelpMsg(_("Input raster dataset"))
              .SetInputDatasetAlias("color-input")
              .SetInputDatasetMetaVar("COLOR-INPUT")
              .SetOutputDatasetHelpMsg(_("Output raster dataset")))
{
    const auto AddOverlayDatasetArg = [this]()
    {
        auto &arg = AddArg("overlay", 0, _("Overlay dataset"),
                           &m_overlayDataset, GDAL_OF_RASTER)
                        .SetPositional()
                        .SetRequired();

        SetAutoCompleteFunctionForFilename(arg, GDAL_OF_RASTER);
    };

    if (standaloneStep)
    {
        AddRasterInputArgs(false, false);
        AddOverlayDatasetArg();
        AddProgressArg();
        AddRasterOutputArgs(false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
        AddOverlayDatasetArg();
    }

    const std::vector<std::string> compositionModeChoices{
        CompositionModesIdentifiers()};
    AddArg("operator", 0, _("Composition operator"), &m_operatorIdentifier)
        .SetChoices(compositionModeChoices)
        .SetDefault(CompositionModeToString(CompositionMode::SRC_OVER))
        .AddAction(
            [this]()
            { m_operator = CompositionModeFromString(m_operatorIdentifier); });

    AddArg("opacity", 0,
           _("Opacity percentage to apply to the overlay dataset (0=fully "
             "transparent, 100=full use of overlay opacity)"),
           &m_opacity)
        .SetDefault(m_opacity)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(OPACITY_INPUT_RANGE);

    AddValidationAction([this]() { return ValidateGlobal(); });
}

namespace
{

/************************************************************************/
/*                             BlendDataset                             */
/************************************************************************/

class BlendDataset final : public GDALDataset
{
  public:
    BlendDataset(GDALDataset &oColorDS, GDALDataset &oOverlayDS,
                 const CompositionMode eOperator, int nOpacity255Scale,
                 bool bSwappedOpacity);
    ~BlendDataset() override;

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override
    {
        return m_oColorDS.GetGeoTransform(gt);
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oColorDS.GetSpatialRef();
    }

    bool AcquireSourcePixels(int nXOff, int nYOff, int nXSize, int nYSize,
                             int nBufXSize, int nBufYSize,
                             GDALRasterIOExtraArg *psExtraArg);

  protected:
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount,
                     BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

  private:
    friend class BlendBand;
    GDALDataset &m_oColorDS;
    GDALDataset &m_oOverlayDS;
    const CompositionMode m_operator;
    const int m_opacity255Scale;
    std::vector<std::unique_ptr<BlendDataset>> m_apoOverviews{};
    int m_nCachedXOff = 0;
    int m_nCachedYOff = 0;
    int m_nCachedXSize = 0;
    int m_nCachedYSize = 0;
    int m_nCachedBufXSize = 0;
    int m_nCachedBufYSize = 0;
    GDALRasterIOExtraArg m_sCachedExtraArg{};
    std::vector<GByte> m_abyBuffer{};
    bool m_ioError = false;
    bool m_bSwappedOpacity = false;
};

/************************************************************************/
/*                             rgb_to_hs()                              */
/************************************************************************/

// rgb comes in as [r,g,b] with values in the range [0,255]. The returned
// values will be with hue and saturation in the range [0,1].

// Derived from hsv_merge.py

static void rgb_to_hs(int r, int g, int b, float *h, float *s)
{
    int minc, maxc;
    if (r <= g)
    {
        if (r <= b)
        {
            minc = r;
            maxc = std::max(g, b);
        }
        else /* b < r */
        {
            minc = b;
            maxc = g;
        }
    }
    else /* g < r */
    {
        if (g <= b)
        {
            minc = g;
            maxc = std::max(r, b);
        }
        else /* b < g */
        {
            minc = b;
            maxc = r;
        }
    }
    const int maxc_minus_minc = maxc - minc;
    if (s)
        *s = maxc_minus_minc / static_cast<float>(std::max(1, maxc));
    if (h)
    {
        const float maxc_minus_minc_times_6 =
            maxc_minus_minc == 0 ? 1.0f : 6.0f * maxc_minus_minc;
        if (maxc == b)
            *h = 4.0f / 6.0f + (r - g) / maxc_minus_minc_times_6;
        else if (maxc == g)
            *h = 2.0f / 6.0f + (b - r) / maxc_minus_minc_times_6;
        else
        {
            const float tmp = (g - b) / maxc_minus_minc_times_6;
            *h = tmp < 0.0f ? tmp + 1.0f : tmp;
        }
    }
}

/************************************************************************/
/*                            choose_among()                            */
/************************************************************************/

template <typename T>
static inline T choose_among(int idx, T a0, T a1, T a2, T a3, T a4, T a5)
{
    switch (idx)
    {
        case 0:
            return a0;
        case 1:
            return a1;
        case 2:
            return a2;
        case 3:
            return a3;
        case 4:
            return a4;
        default:
            break;
    }
    return a5;
}

/************************************************************************/
/*                             hsv_to_rgb()                             */
/************************************************************************/

// hsv comes in as [h,s,v] with hue and saturation in the range [0,1],
// but value in the range [0,255].

// Derived from hsv_merge.py

static void hsv_to_rgb(float h, float s, GByte v, GByte *r, GByte *g, GByte *b)
{
    const int i = static_cast<int>(6.0f * h);
    const float f = 6.0f * h - i;
    const GByte p = static_cast<GByte>(v * (1.0f - s) + 0.5f);
    const GByte q = static_cast<GByte>(v * (1.0f - s * f) + 0.5f);
    const GByte t = static_cast<GByte>(v * (1.0f - s * (1.0f - f)) + 0.5f);

    if (r)
        *r = choose_among(i, v, q, p, p, t, v);
    if (g)
        *g = choose_among(i, t, v, v, q, p, p);
    if (b)
        *b = choose_among(i, p, p, t, v, v, q);
}

/************************************************************************/
/*                           XMM_RGB_to_HS()                            */
/************************************************************************/

#ifdef HAVE_SSE2
static inline void
XMM_RGB_to_HS(const GByte *CPL_RESTRICT pInR, const GByte *CPL_RESTRICT pInG,
              const GByte *CPL_RESTRICT pInB, const XMMReg4Float &zero,
              const XMMReg4Float &one, const XMMReg4Float &six,
              const XMMReg4Float &two_over_six,
              const XMMReg4Float &four_over_six, XMMReg4Float &h,
              XMMReg4Float &s)
{
    const auto r = XMMReg4Float::Load4Val(pInR);
    const auto g = XMMReg4Float::Load4Val(pInG);
    const auto b = XMMReg4Float::Load4Val(pInB);
    const auto minc = XMMReg4Float::Min(XMMReg4Float::Min(r, g), b);
    const auto maxc = XMMReg4Float::Max(XMMReg4Float::Max(r, g), b);
    const auto max_minus_min = maxc - minc;
    s = max_minus_min / XMMReg4Float::Max(one, maxc);
    const auto inv_max_minus_min_times_6_0 =
        XMMReg4Float::Ternary(XMMReg4Float::Equals(max_minus_min, zero), one,
                              six * max_minus_min)
            .inverse();
    const auto tmp = (g - b) * inv_max_minus_min_times_6_0;
    h = XMMReg4Float::Ternary(
        XMMReg4Float::Equals(maxc, b),
        four_over_six + (r - g) * inv_max_minus_min_times_6_0,
        XMMReg4Float::Ternary(
            XMMReg4Float::Equals(maxc, g),
            two_over_six + (b - r) * inv_max_minus_min_times_6_0,
            XMMReg4Float::Ternary(XMMReg4Float::Lesser(tmp, zero), tmp + one,
                                  tmp)));
}
#endif

/************************************************************************/
/*                          patch_value_line()                          */
/************************************************************************/

static
#ifdef __GNUC__
    __attribute__((__noinline__))
#endif
    void patch_value_line(int nCount, const GByte *CPL_RESTRICT pInR,
                          const GByte *CPL_RESTRICT pInG,
                          const GByte *CPL_RESTRICT pInB,
                          const GByte *CPL_RESTRICT pInGray,
                          GByte *CPL_RESTRICT pOutR, GByte *CPL_RESTRICT pOutG,
                          GByte *CPL_RESTRICT pOutB)
{
    int i = 0;
#ifdef HAVE_SSE2
    const auto zero = XMMReg4Float::Zero();
    const auto one = XMMReg4Float::Set1(1.0f);
    const auto six = XMMReg4Float::Set1(6.0f);
    const auto two_over_six = XMMReg4Float::Set1(2.0f / 6.0f);
    const auto four_over_six = two_over_six + two_over_six;

    constexpr int ELTS = 8;
    for (; i + (ELTS - 1) < nCount; i += ELTS)
    {
        XMMReg4Float h0, s0;
        XMM_RGB_to_HS(pInR + i, pInG + i, pInB + i, zero, one, six,
                      two_over_six, four_over_six, h0, s0);
        XMMReg4Float h1, s1;
        XMM_RGB_to_HS(pInR + i + ELTS / 2, pInG + i + ELTS / 2,
                      pInB + i + ELTS / 2, zero, one, six, two_over_six,
                      four_over_six, h1, s1);

        XMMReg4Float v0, v1;
        XMMReg4Float::Load8Val(pInGray + i, v0, v1);

        const auto half = XMMReg4Float::Set1(0.5f);
        const auto six_h0 = six * h0;
        const auto idx0 = six_h0.truncate_to_int();
        const auto f0 = six_h0 - idx0.cast_to_float();
        const auto p0 = (v0 * (one - s0) + half).truncate_to_int();
        const auto q0 = (v0 * (one - s0 * f0) + half).truncate_to_int();
        const auto t0 = (v0 * (one - s0 * (one - f0)) + half).truncate_to_int();

        const auto six_h1 = six * h1;
        const auto idx1 = six_h1.truncate_to_int();
        const auto f1 = six_h1 - idx1.cast_to_float();
        const auto p1 = (v1 * (one - s1) + half).truncate_to_int();
        const auto q1 = (v1 * (one - s1 * f1) + half).truncate_to_int();
        const auto t1 = (v1 * (one - s1 * (one - f1)) + half).truncate_to_int();

        const auto idx = XMMReg8Byte::Pack(idx0, idx1);
        const auto v =
            XMMReg8Byte::Pack(v0.truncate_to_int(), v1.truncate_to_int());
        const auto p = XMMReg8Byte::Pack(p0, p1);
        const auto q = XMMReg8Byte::Pack(q0, q1);
        const auto t = XMMReg8Byte::Pack(t0, t1);

        const auto equalsTo0 = XMMReg8Byte::Equals(idx, XMMReg8Byte::Zero());
        const auto one8Byte = XMMReg8Byte::Set1(1);
        const auto equalsTo1 = XMMReg8Byte::Equals(idx, one8Byte);
        const auto two8Byte = one8Byte + one8Byte;
        const auto equalsTo2 = XMMReg8Byte::Equals(idx, two8Byte);
        const auto four8Byte = two8Byte + two8Byte;
        const auto equalsTo4 = XMMReg8Byte::Equals(idx, four8Byte);
        const auto equalsTo3 = XMMReg8Byte::Equals(idx, four8Byte - one8Byte);
        // clang-format off
        if (pOutR)
        {
            const auto out_r =
                XMMReg8Byte::Ternary(equalsTo0, v,
                XMMReg8Byte::Ternary(equalsTo1, q,
                XMMReg8Byte::Ternary(XMMReg8Byte::Or(equalsTo2, equalsTo3), p,
                XMMReg8Byte::Ternary(equalsTo4, t, v))));
            out_r.Store8Val(pOutR + i);
        }
        if (pOutG)
        {
            const auto out_g =
                XMMReg8Byte::Ternary(equalsTo0, t,
                XMMReg8Byte::Ternary(XMMReg8Byte::Or(equalsTo1, equalsTo2), v,
                XMMReg8Byte::Ternary(equalsTo3, q, p)));
            out_g.Store8Val(pOutG + i);
        }
        if (pOutB)
        {
            const auto out_b =
                XMMReg8Byte::Ternary(XMMReg8Byte::Or(equalsTo0, equalsTo1), p,
                XMMReg8Byte::Ternary(equalsTo2, t,
                XMMReg8Byte::Ternary(XMMReg8Byte::Or(equalsTo3, equalsTo4),
                                     v, q)));
            out_b.Store8Val(pOutB + i);
        }
        // clang-format on
    }
#endif

    for (; i < nCount; ++i)
    {
        float h, s;
        rgb_to_hs(pInR[i], pInG[i], pInB[i], &h, &s);
        hsv_to_rgb(h, s, pInGray[i], pOutR ? pOutR + i : nullptr,
                   pOutG ? pOutG + i : nullptr, pOutB ? pOutB + i : nullptr);
    }
}

/************************************************************************/
/*                              BlendBand                               */
/************************************************************************/

class BlendBand final : public GDALRasterBand
{
  public:
    BlendBand(BlendDataset &oBlendDataset, int nBandIn)
        : m_oBlendDataset(oBlendDataset)
    {
        nBand = nBandIn;
        nRasterXSize = oBlendDataset.GetRasterXSize();
        nRasterYSize = oBlendDataset.GetRasterYSize();
        oBlendDataset.m_oColorDS.GetRasterBand(1)->GetBlockSize(&nBlockXSize,
                                                                &nBlockYSize);
        eDataType = GDT_UInt8;
    }

    GDALColorInterp GetColorInterpretation() override
    {
        if (m_oBlendDataset.GetRasterCount() <= 2 && nBand == 1)
            return GCI_GrayIndex;
        else if (m_oBlendDataset.GetRasterCount() == 2 || nBand == 4)
            return GCI_AlphaBand;
        else
            return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1);
    }

    int GetOverviewCount() override
    {
        return static_cast<int>(m_oBlendDataset.m_apoOverviews.size());
    }

    GDALRasterBand *GetOverview(int idx) override
    {
        return idx >= 0 && idx < GetOverviewCount()
                   ? m_oBlendDataset.m_apoOverviews[idx]->GetRasterBand(nBand)
                   : nullptr;
    }

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override
    {
        int nReqXSize = 0;
        int nReqYSize = 0;
        GetActualBlockSize(nBlockXOff, nBlockYOff, &nReqXSize, &nReqYSize);
        return RasterIO(GF_Read, nBlockXOff * nBlockXSize,
                        nBlockYOff * nBlockYSize, nReqXSize, nReqYSize, pData,
                        nReqXSize, nReqYSize, GDT_UInt8, 1, nBlockXSize,
                        nullptr);
    }

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

  private:
    BlendDataset &m_oBlendDataset;
};

/************************************************************************/
/*                     BlendDataset::BlendDataset()                     */
/************************************************************************/

BlendDataset::BlendDataset(GDALDataset &oColorDS, GDALDataset &oOverlayDS,
                           const CompositionMode eOperator,
                           int nOpacity255Scale, bool bSwappedOpacity)
    : m_oColorDS(oColorDS), m_oOverlayDS(oOverlayDS), m_operator(eOperator),
      m_opacity255Scale(nOpacity255Scale), m_bSwappedOpacity(bSwappedOpacity)
{
    m_oColorDS.Reference();
    m_oOverlayDS.Reference();

    CPLAssert(oColorDS.GetRasterXSize() == oOverlayDS.GetRasterXSize());
    CPLAssert(oColorDS.GetRasterYSize() == oOverlayDS.GetRasterYSize());
    nRasterXSize = oColorDS.GetRasterXSize();
    nRasterYSize = oColorDS.GetRasterYSize();
    const int nOvrCount = oOverlayDS.GetRasterBand(1)->GetOverviewCount();
    bool bCanCreateOvr = true;
    for (int iBand = 1; iBand <= oColorDS.GetRasterCount(); ++iBand)
    {
        SetBand(iBand, std::make_unique<BlendBand>(*this, iBand));
        bCanCreateOvr =
            bCanCreateOvr &&
            (iBand > oColorDS.GetRasterCount() ||
             oColorDS.GetRasterBand(iBand)->GetOverviewCount() == nOvrCount) &&
            (iBand > oOverlayDS.GetRasterCount() ||
             oOverlayDS.GetRasterBand(iBand)->GetOverviewCount() == nOvrCount);
        const int nColorBandxIdx =
            iBand <= oColorDS.GetRasterCount() ? iBand : 1;
        const int nOverlayBandIdx =
            iBand <= oOverlayDS.GetRasterCount() ? iBand : 1;
        for (int iOvr = 0; iOvr < nOvrCount && bCanCreateOvr; ++iOvr)
        {
            const auto poColorOvrBand =
                oColorDS.GetRasterBand(nColorBandxIdx)->GetOverview(iOvr);
            const auto poGSOvrBand =
                oOverlayDS.GetRasterBand(nOverlayBandIdx)->GetOverview(iOvr);
            bCanCreateOvr =
                poColorOvrBand->GetDataset() != &oColorDS &&
                poColorOvrBand->GetDataset() == oColorDS.GetRasterBand(1)
                                                    ->GetOverview(iOvr)
                                                    ->GetDataset() &&
                poGSOvrBand->GetDataset() != &oOverlayDS &&
                poGSOvrBand->GetDataset() == oOverlayDS.GetRasterBand(1)
                                                 ->GetOverview(iOvr)
                                                 ->GetDataset() &&
                poColorOvrBand->GetXSize() == poGSOvrBand->GetXSize() &&
                poColorOvrBand->GetYSize() == poGSOvrBand->GetYSize();
        }
    }

    SetDescription(CPLSPrintf("Blend %s width %s", m_oColorDS.GetDescription(),
                              m_oOverlayDS.GetDescription()));
    if (nBands > 1)
    {
        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }

    if (bCanCreateOvr)
    {
        for (int iOvr = 0; iOvr < nOvrCount; ++iOvr)
        {
            m_apoOverviews.push_back(std::make_unique<BlendDataset>(
                *(oColorDS.GetRasterBand(1)->GetOverview(iOvr)->GetDataset()),
                *(oOverlayDS.GetRasterBand(1)->GetOverview(iOvr)->GetDataset()),
                m_operator, nOpacity255Scale, bSwappedOpacity));
        }
    }
}

/************************************************************************/
/*                    ~BlendDataset::BlendDataset()                     */
/************************************************************************/

BlendDataset::~BlendDataset()
{
    m_oColorDS.ReleaseRef();
    m_oOverlayDS.ReleaseRef();
}

/************************************************************************/
/*                 BlendDataset::AcquireSourcePixels()                  */
/************************************************************************/

bool BlendDataset::AcquireSourcePixels(int nXOff, int nYOff, int nXSize,
                                       int nYSize, int nBufXSize, int nBufYSize,
                                       GDALRasterIOExtraArg *psExtraArg)
{
    if (nXOff == m_nCachedXOff && nYOff == m_nCachedYOff &&
        nXSize == m_nCachedXSize && nYSize == m_nCachedYSize &&
        nBufXSize == m_nCachedBufXSize && nBufYSize == m_nCachedBufYSize &&
        psExtraArg->eResampleAlg == m_sCachedExtraArg.eResampleAlg &&
        psExtraArg->bFloatingPointWindowValidity ==
            m_sCachedExtraArg.bFloatingPointWindowValidity &&
        (!psExtraArg->bFloatingPointWindowValidity ||
         (psExtraArg->dfXOff == m_sCachedExtraArg.dfXOff &&
          psExtraArg->dfYOff == m_sCachedExtraArg.dfYOff &&
          psExtraArg->dfXSize == m_sCachedExtraArg.dfXSize &&
          psExtraArg->dfYSize == m_sCachedExtraArg.dfYSize)))
    {
        return !m_abyBuffer.empty();
    }

    const int nColorCount = m_oColorDS.GetRasterCount();
    const int nOverlayCount = m_oOverlayDS.GetRasterCount();
    const int nCompsInBuffer = nColorCount + nOverlayCount;
    assert(nCompsInBuffer > 0);

    if (static_cast<size_t>(nBufXSize) >
        std::numeric_limits<size_t>::max() / nBufYSize / nCompsInBuffer)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating temporary buffer");
        m_abyBuffer.clear();
        m_ioError = true;
        return false;
    }

    const size_t nPixelCount = static_cast<size_t>(nBufXSize) * nBufYSize;
    try
    {
        if (m_abyBuffer.size() < nPixelCount * nCompsInBuffer)
            m_abyBuffer.resize(nPixelCount * nCompsInBuffer);
    }
    catch (const std::exception &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory allocating temporary buffer");
        m_abyBuffer.clear();
        m_ioError = true;
        return false;
    }

    const bool bOK =
        (m_oColorDS.RasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                             m_abyBuffer.data(), nBufXSize, nBufYSize,
                             GDT_UInt8, nColorCount, nullptr, 1, nBufXSize,
                             static_cast<GSpacing>(nPixelCount),
                             psExtraArg) == CE_None &&
         m_oOverlayDS.RasterIO(
             GF_Read, nXOff, nYOff, nXSize, nYSize,
             m_abyBuffer.data() + nPixelCount * nColorCount, nBufXSize,
             nBufYSize, GDT_UInt8, nOverlayCount, nullptr, 1, nBufXSize,
             static_cast<GSpacing>(nPixelCount), psExtraArg) == CE_None);
    if (bOK)
    {
        m_nCachedXOff = nXOff;
        m_nCachedYOff = nYOff;
        m_nCachedXSize = nXSize;
        m_nCachedYSize = nYSize;
        m_nCachedBufXSize = nBufXSize;
        m_nCachedBufYSize = nBufYSize;
        m_sCachedExtraArg = *psExtraArg;
    }
    else
    {
        m_abyBuffer.clear();
        m_ioError = true;
    }
    return bOK;
}

/************************************************************************/
/*                             gTabInvDstA                              */
/************************************************************************/

constexpr int SHIFT_DIV_DSTA = 8;

// Table of (255 * 256 + k/2) / k values for k in [0,255]
constexpr auto gTabInvDstA = []()
{
    std::array<uint16_t, 256> arr{};

    arr[0] = 0;
    for (int k = 1; k <= 255; ++k)
    {
        arr[k] = static_cast<uint16_t>(((255 << SHIFT_DIV_DSTA) + (k / 2)) / k);
    }

    return arr;
}();

/************************************************************************/
/*                        BlendMultiply_Generic                         */
/************************************************************************/

static void BlendMultiply_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    GSpacing nPixelSpace, GSpacing nBandSpace, size_t i, size_t N,
    GByte nOpacity, int nOutputBands, bool bSwappedOpacity)
{

    // Generic formulas from Mapserver
    // Dca' = Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = Sa + Da - Sa.Da

    // TODO: optimize for the various cases (with/without alpha, grayscale/RGB)
    // TODO: optimize mathematically to avoid redundant computations

    GSpacing nDstOffset = 0;

    for (; i < N; ++i)
    {

        GByte nOverlayR, nOverlayG, nOverlayB, nOverlayA;
        GByte nR, nG, nB, nA;
        GByte nFinalAlpha;
        ProcessAlphaChannels(i, pabyA, pabyOverlayA, nOpacity, bSwappedOpacity,
                             nA, nOverlayA, nFinalAlpha);
        PremultiplyChannels(i, pabyR, pabyG, pabyB, nR, nG, nB, nA);
        PremultiplyChannels(i, pabyOverlayR, pabyOverlayG, pabyOverlayB,
                            nOverlayR, nOverlayG, nOverlayB, nOverlayA);

        // Result
        auto processComponent = [&nFinalAlpha](GByte C, GByte A, GByte OverlayC,
                                               GByte OverlayA) -> GByte
        {
            return DivScale255((MulScale255(C, OverlayC) +
                                MulScale255(C, 255 - OverlayA) +
                                MulScale255(OverlayC, 255 - A)),
                               nFinalAlpha);
        };

        pabyDst[nDstOffset] = processComponent(nR, nA, nOverlayR, nOverlayA);

        // Grayscale with alpha
        if (nOutputBands == 2)
        {
            pabyDst[nDstOffset + nBandSpace] = nFinalAlpha;
        }
        else
        {
            // RBG and RGBA
            if (nOutputBands >= 3)
            {
                pabyDst[nDstOffset + nBandSpace] =
                    processComponent(nG, nA, nOverlayG, nOverlayA);
                pabyDst[nDstOffset + 2 * nBandSpace] =
                    processComponent(nB, nA, nOverlayB, nOverlayA);
            }

            // RGBA
            if (nOutputBands == 4)
            {
                pabyDst[nDstOffset + 3 * nBandSpace] = nFinalAlpha;
            }
        }
        nDstOffset += nPixelSpace;
    }
}

/************************************************************************/
/*                         BlendScreen_Generic                          */
/************************************************************************/

static void BlendScreen_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    GSpacing nPixelSpace, GSpacing nBandSpace, size_t i, size_t N,
    GByte nOpacity, int nOutputBands, bool bSwappedOpacity)
{

    // Generic formulas from Mapserver
    // Dca' = Sca + Dca - Sca.Dca
    // Da'  = Sa + Da - Sa.Da

    // TODO: optimize for the various cases (with/without alpha, grayscale/RGB)
    // TODO: optimize mathematically to avoid redundant computations

    GSpacing nDstOffset = 0;

    for (; i < N; ++i)
    {

        GByte nOverlayR, nOverlayG, nOverlayB, nOverlayA;
        GByte nR, nG, nB, nA;
        GByte nFinalAlpha;
        ProcessAlphaChannels(i, pabyA, pabyOverlayA, nOpacity, bSwappedOpacity,
                             nA, nOverlayA, nFinalAlpha);
        PremultiplyChannels(i, pabyR, pabyG, pabyB, nR, nG, nB, nA);
        PremultiplyChannels(i, pabyOverlayR, pabyOverlayG, pabyOverlayB,
                            nOverlayR, nOverlayG, nOverlayB, nOverlayA);

        // Result

        auto processComponent = [&nFinalAlpha](GByte C, GByte OverlayC) -> GByte
        {
            return DivScale255(C + OverlayC - MulScale255(C, OverlayC),
                               nFinalAlpha);
        };

        pabyDst[nDstOffset] = processComponent(nR, nOverlayR);

        // Grayscale with alpha
        if (nOutputBands == 2)
        {
            pabyDst[nDstOffset + nBandSpace] = nFinalAlpha;
        }
        else
        {
            // RBG and RGBA
            if (nOutputBands >= 3)
            {
                pabyDst[nDstOffset + nBandSpace] =
                    processComponent(nG, nOverlayG);
                pabyDst[nDstOffset + 2 * nBandSpace] =
                    processComponent(nB, nOverlayB);
            }

            // RGBA
            if (nOutputBands == 4)
            {
                pabyDst[nDstOffset + 3 * nBandSpace] = nFinalAlpha;
            }
        }
        nDstOffset += nPixelSpace;
    }
}

/************************************************************************/
/*                         BlendOverlay_Generic                         */
/************************************************************************/

static void BlendOverlay_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    GSpacing nPixelSpace, GSpacing nBandSpace, size_t i, size_t N,
    GByte nOpacity, int nOutputBands, bool bSwappedOpacity)
{

    // Generic formulas from Mapserver
    // Where "D" is destination, "S" is source (overlay)
    // if 2.Dca < Da
    //   Dca' = 2.Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
    // otherwise
    //   Dca' = Sa.Da - 2.(Da - Dca).(Sa - Sca) + Sca.(1 - Da) + Dca.(1 - Sa)
    //
    // Da'  = Sa + Da - Sa.Da

    // TODO: optimize for the various cases (with/without alpha, grayscale/RGB)
    // TODO: optimize mathematically to avoid redundant computations

    GSpacing nDstOffset = 0;

    for (; i < N; ++i)
    {

        GByte nOverlayR, nOverlayG, nOverlayB, nOverlayA;
        GByte nR, nG, nB, nA;
        GByte nFinalAlpha;
        ProcessAlphaChannels(i, pabyA, pabyOverlayA, nOpacity, bSwappedOpacity,
                             nA, nOverlayA, nFinalAlpha);
        PremultiplyChannels(i, pabyR, pabyG, pabyB, nR, nG, nB, nA);
        PremultiplyChannels(i, pabyOverlayR, pabyOverlayG, pabyOverlayB,
                            nOverlayR, nOverlayG, nOverlayB, nOverlayA);

        // Sa.Da
        const GByte nAlphaMul{MulScale255(nOverlayA, nA)};

        auto processComponent_lessThan = [&nFinalAlpha](GByte C, GByte A,
                                                        GByte OverlayC,
                                                        GByte OverlayA) -> GByte
        {
            return DivScale255(2 * MulScale255(C, OverlayC) +
                                   MulScale255(C, 255 - OverlayA) +
                                   MulScale255(OverlayC, 255 - A),
                               nFinalAlpha);
        };

        auto processComponent_greaterEqual =
            [&nFinalAlpha, &nAlphaMul](GByte C, GByte A, GByte OverlayC,
                                       GByte OverlayA) -> GByte
        {
            return DivScale255(nAlphaMul -
                                   2 * MulScale255(A - C, OverlayA - OverlayC) +
                                   MulScale255(C, 255 - OverlayA) +
                                   MulScale255(OverlayC, 255 - A),
                               nFinalAlpha);
        };

        if (2 * nR < nA)
        {
            pabyDst[nDstOffset] =
                processComponent_lessThan(nR, nA, nOverlayR, nOverlayA);
        }
        else
        {
            pabyDst[nDstOffset] =
                processComponent_greaterEqual(nR, nA, nOverlayR, nOverlayA);
        }

        // Grayscale with alpha
        if (nOutputBands == 2)
        {
            pabyDst[nDstOffset + nBandSpace] = nFinalAlpha;
        }
        else
        {
            // RBG and RGBA
            if (nOutputBands >= 3)
            {

                if (2 * nG < nA)
                {
                    pabyDst[nDstOffset + nBandSpace] =
                        processComponent_lessThan(nG, nA, nOverlayG, nOverlayA);
                }
                else
                {
                    pabyDst[nDstOffset + nBandSpace] =
                        processComponent_greaterEqual(nG, nA, nOverlayG,
                                                      nOverlayA);
                }

                if (2 * nB < nA)
                {
                    pabyDst[nDstOffset + 2 * nBandSpace] =
                        processComponent_lessThan(nB, nA, nOverlayB, nOverlayA);
                }
                else
                {
                    pabyDst[nDstOffset + 2 * nBandSpace] =
                        processComponent_greaterEqual(nB, nA, nOverlayB,
                                                      nOverlayA);
                }
            }

            // RGBA
            if (nOutputBands == 4)
            {
                pabyDst[nDstOffset + 3 * nBandSpace] = nFinalAlpha;
            }
        }
        nDstOffset += nPixelSpace;
    }
}

/************************************************************************/
/*                        BlendHardLight_Generic                        */
/************************************************************************/

static void BlendHardLight_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    GSpacing nPixelSpace, GSpacing nBandSpace, size_t i, size_t N,
    GByte nOpacity, int nOutputBands, bool bSwappedOpacity)
{
    // Hard Light is Overlay with roles of source and overlay swapped
    BlendOverlay_Generic(pabyOverlayR, pabyOverlayG, pabyOverlayB, pabyOverlayA,
                         pabyR, pabyG, pabyB, pabyA, pabyDst, nPixelSpace,
                         nBandSpace, i, N, nOpacity, nOutputBands,
                         !bSwappedOpacity);
}

/************************************************************************/
/*                         BlendDarken_Generic                          */
/************************************************************************/

static void BlendDarken_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    GSpacing nPixelSpace, GSpacing nBandSpace, size_t i, size_t N,
    GByte nOpacity, int nOutputBands, bool bSwappedOpacity)
{
    // Generic formulas from Mapserver
    // Dca' = min(Sca.Da, Dca.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = Sa + Da - Sa.Da

    // TODO: optimize for the various cases (with/without alpha, grayscale/RGB)
    // TODO: optimize mathematically to avoid redundant computations

    GSpacing nDstOffset = 0;

    for (; i < N; ++i)
    {

        GByte nOverlayR, nOverlayG, nOverlayB, nOverlayA;
        GByte nR, nG, nB, nA;
        GByte nFinalAlpha;
        ProcessAlphaChannels(i, pabyA, pabyOverlayA, nOpacity, bSwappedOpacity,
                             nA, nOverlayA, nFinalAlpha);
        PremultiplyChannels(i, pabyR, pabyG, pabyB, nR, nG, nB, nA);
        PremultiplyChannels(i, pabyOverlayR, pabyOverlayG, pabyOverlayB,
                            nOverlayR, nOverlayG, nOverlayB, nOverlayA);

        // Result

        auto processComponent = [&nFinalAlpha](GByte C, GByte A, GByte OverlayC,
                                               GByte OverlayA) -> GByte
        {
            return DivScale255(
                std::min(MulScale255(OverlayC, A), MulScale255(C, OverlayA)) +
                    MulScale255(C, 255 - OverlayA) +
                    MulScale255(OverlayC, 255 - A),
                nFinalAlpha);
        };

        pabyDst[nDstOffset] = processComponent(nR, nA, nOverlayR, nOverlayA);

        // Grayscale with alpha
        if (nOutputBands == 2)
        {
            pabyDst[nDstOffset + nBandSpace] = nFinalAlpha;
        }
        else
        {
            // RBG and RGBA
            if (nOutputBands >= 3)
            {
                pabyDst[nDstOffset + nBandSpace] =
                    processComponent(nG, nA, nOverlayG, nOverlayA);
                pabyDst[nDstOffset + 2 * nBandSpace] =
                    processComponent(nB, nA, nOverlayB, nOverlayA);
            }

            // RGBA
            if (nOutputBands == 4)
            {
                pabyDst[nDstOffset + 3 * nBandSpace] = nFinalAlpha;
            }
        }
        nDstOffset += nPixelSpace;
    }
}

/************************************************************************/
/*                         BlendLighten_Generic                         */
/************************************************************************/

static void BlendLighten_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    GSpacing nPixelSpace, GSpacing nBandSpace, size_t i, size_t N,
    GByte nOpacity, int nOutputBands, bool bSwappedOpacity)
{

    // Generic formulas from Mapserver
    // Dca' = max(Sca.Da, Dca.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = Sa + Da - Sa.Da

    // TODO: optimize for the various cases (with/without alpha, grayscale/RGB)
    // TODO: optimize mathematically to avoid redundant computations

    GSpacing nDstOffset = 0;

    for (; i < N; ++i)
    {

        GByte nOverlayR, nOverlayG, nOverlayB, nOverlayA;
        GByte nR, nG, nB, nA;
        GByte nFinalAlpha;
        ProcessAlphaChannels(i, pabyA, pabyOverlayA, nOpacity, bSwappedOpacity,
                             nA, nOverlayA, nFinalAlpha);
        PremultiplyChannels(i, pabyR, pabyG, pabyB, nR, nG, nB, nA);
        PremultiplyChannels(i, pabyOverlayR, pabyOverlayG, pabyOverlayB,
                            nOverlayR, nOverlayG, nOverlayB, nOverlayA);

        // Result

        auto processComponent = [&nFinalAlpha](GByte C, GByte A, GByte OverlayC,
                                               GByte OverlayA) -> GByte
        {
            return DivScale255(
                std::max(MulScale255(OverlayC, A), MulScale255(C, OverlayA)) +
                    MulScale255(C, 255 - OverlayA) +
                    MulScale255(OverlayC, 255 - A),
                nFinalAlpha);
        };

        pabyDst[nDstOffset] = processComponent(nR, nA, nOverlayR, nOverlayA);

        // Grayscale with alpha
        if (nOutputBands == 2)
        {
            pabyDst[nDstOffset + nBandSpace] = nFinalAlpha;
        }
        else
        {
            // RBG and RGBA
            if (nOutputBands >= 3)
            {
                pabyDst[nDstOffset + nBandSpace] =
                    processComponent(nG, nA, nOverlayG, nOverlayA);
                pabyDst[nDstOffset + 2 * nBandSpace] =
                    processComponent(nB, nA, nOverlayB, nOverlayA);
            }

            // RGBA
            if (nOutputBands == 4)
            {
                pabyDst[nDstOffset + 3 * nBandSpace] = nFinalAlpha;
            }
        }
        nDstOffset += nPixelSpace;
    }
}

/************************************************************************/
/*                       BlendColorDodge_Generic                        */
/************************************************************************/

static void BlendColorDodge_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    GSpacing nPixelSpace, GSpacing nBandSpace, size_t i, size_t N,
    GByte nOpacity, int nOutputBands, bool bSwappedOpacity)
{

    // Generic formulas from Mapserver
    // if Sca.Da + Dca.Sa >= Sa.Da
    //   Dca' = Sa.Da + Sca.(1 - Da) + Dca.(1 - Sa)
    // otherwise
    //   Dca' = Dca.Sa/(1-Sca/Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
    //
    // Da'  = Sa + Da - Sa.Da

    // TODO: optimize for the various cases (with/without alpha, grayscale/RGB)
    // TODO: optimize mathematically to avoid redundant computations

    GSpacing nDstOffset = 0;

    for (; i < N; ++i)
    {

        GByte nOverlayR, nOverlayG, nOverlayB, nOverlayA;
        GByte nR, nG, nB, nA;
        GByte nFinalAlpha;
        ProcessAlphaChannels(i, pabyA, pabyOverlayA, nOpacity, bSwappedOpacity,
                             nA, nOverlayA, nFinalAlpha);
        PremultiplyChannels(i, pabyR, pabyG, pabyB, nR, nG, nB, nA);
        PremultiplyChannels(i, pabyOverlayR, pabyOverlayG, pabyOverlayB,
                            nOverlayR, nOverlayG, nOverlayB, nOverlayA);

        // Result

        const GByte alphaMul255{MulScale255(nOverlayA, nA)};

        // Dca' = Sa.Da + Sca.(1 - Da) + Dca.(1 - Sa)
        auto processComponent_greaterEqual =
            [&nFinalAlpha, &alphaMul255](GByte C, GByte A, GByte OverlayC,
                                         GByte OverlayA) -> GByte
        {
            return DivScale255(alphaMul255 + MulScale255(C, 255 - OverlayA) +
                                   MulScale255(OverlayC, 255 - A),
                               nFinalAlpha);
        };

        // Dca.Sa/(1-Sca/Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
        auto processComponent_lessThan = [&nFinalAlpha](GByte C, GByte A,
                                                        GByte OverlayC,
                                                        GByte OverlayA) -> GByte
        {
            return DivScale255(
                DivScale255(MulScale255(C, OverlayA),
                            255 - DivScale255(OverlayC, OverlayA)) +
                    MulScale255(C, 255 - OverlayA) +
                    MulScale255(OverlayC, 255 - A),
                nFinalAlpha);
        };

        auto branchingCondition = [&alphaMul255](GByte C, GByte A,
                                                 GByte OverlayC,
                                                 GByte OverlayA) -> bool
        {
            return MulScale255(OverlayC, A) + MulScale255(C, OverlayA) >=
                   alphaMul255;
        };

        if (branchingCondition(nR, nA, nOverlayR, nOverlayA))
        {
            pabyDst[nDstOffset] =
                processComponent_greaterEqual(nR, nA, nOverlayR, nOverlayA);
        }
        else
        {
            pabyDst[nDstOffset] =
                processComponent_lessThan(nR, nA, nOverlayR, nOverlayA);
        }

        // Grayscale with alpha
        if (nOutputBands == 2)
        {
            pabyDst[nDstOffset + nBandSpace] = nFinalAlpha;
        }
        else
        {
            // RBG and RGBA
            if (nOutputBands >= 3)
            {
                if (branchingCondition(nG, nA, nOverlayG, nOverlayA))
                {
                    pabyDst[nDstOffset + nBandSpace] =
                        processComponent_greaterEqual(nG, nA, nOverlayG,
                                                      nOverlayA);
                }
                else
                {
                    pabyDst[nDstOffset + nBandSpace] =
                        processComponent_lessThan(nG, nA, nOverlayG, nOverlayA);
                }

                if (branchingCondition(nB, nA, nOverlayB, nOverlayA))
                {
                    pabyDst[nDstOffset + 2 * nBandSpace] =
                        processComponent_greaterEqual(nB, nA, nOverlayB,
                                                      nOverlayA);
                }
                else
                {
                    pabyDst[nDstOffset + 2 * nBandSpace] =
                        processComponent_lessThan(nB, nA, nOverlayB, nOverlayA);
                }
            }

            // RGBA
            if (nOutputBands == 4)
            {
                pabyDst[nDstOffset + 3 * nBandSpace] = nFinalAlpha;
            }
        }
        nDstOffset += nPixelSpace;
    }
}

/************************************************************************/
/*                        BlendColorBurn_Generic                        */
/************************************************************************/

static void BlendColorBurn_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    GSpacing nPixelSpace, GSpacing nBandSpace, size_t i, size_t N,
    GByte nOpacity, int nOutputBands, bool bSwappedOpacity)
{

    // if Sca.Da + Dca.Sa <= Sa.Da
    //   Dca' = Sca.(1 - Da) + Dca.(1 - Sa)
    // otherwise
    //   Dca' = Sa.(Sca.Da + Dca.Sa - Sa.Da)/Sca + Sca.(1 - Da) + Dca.(1 - Sa)
    // Da'  = Sa + Da - Sa.Da

    // TODO: optimize for the various cases (with/without alpha, grayscale/RGB)
    // TODO: optimize mathematically to avoid redundant computations

    GSpacing nDstOffset = 0;

    for (; i < N; ++i)
    {

        GByte nOverlayR, nOverlayG, nOverlayB, nOverlayA;
        GByte nR, nG, nB, nA;
        GByte nFinalAlpha;
        ProcessAlphaChannels(i, pabyA, pabyOverlayA, nOpacity, bSwappedOpacity,
                             nA, nOverlayA, nFinalAlpha);
        PremultiplyChannels(i, pabyR, pabyG, pabyB, nR, nG, nB, nA);
        PremultiplyChannels(i, pabyOverlayR, pabyOverlayG, pabyOverlayB,
                            nOverlayR, nOverlayG, nOverlayB, nOverlayA);

        // Result

        const GByte alphaMul255{MulScale255(nOverlayA, nA)};

        auto processComponent_lessEqual =
            [&nFinalAlpha](GByte C, GByte A, GByte OverlayC,
                           GByte OverlayA) -> GByte
        {
            return DivScale255(MulScale255(C, 255 - OverlayA) +
                                   MulScale255(OverlayC, 255 - A),
                               nFinalAlpha);
        };

        // The simplified formula below was tested and verified with the floating point equivalent [0..1] normalized version
        auto processComponent_greater =
            [&nFinalAlpha, &alphaMul255](GByte C, GByte A, GByte OverlayC,
                                         GByte OverlayA) -> GByte
        {
            const GByte C_unpremultiplied = DivScale255(C, A);
            const GByte Overlay_C_unpremultiplied =
                DivScale255(OverlayC, OverlayA);
            return DivScale255(
                MulScale255(alphaMul255, (C_unpremultiplied +
                                          Overlay_C_unpremultiplied - 255)) +
                    MulScale255(C, 255 - OverlayA) +
                    MulScale255(OverlayC, 255 - A),
                nFinalAlpha);
        };

        auto branchingCondition = [&alphaMul255](GByte C, GByte A,
                                                 GByte OverlayC,
                                                 GByte OverlayA) -> bool
        {
            return MulScale255(OverlayC, A) + MulScale255(C, OverlayA) <=
                   alphaMul255;
        };

        if (branchingCondition(nR, nA, nOverlayR, nOverlayA))
        {
            pabyDst[nDstOffset] =
                processComponent_lessEqual(nR, nA, nOverlayR, nOverlayA);
        }
        else
        {
            pabyDst[nDstOffset] =
                processComponent_greater(nR, nA, nOverlayR, nOverlayA);
        }

        // Grayscale with alpha
        if (nOutputBands == 2)
        {
            pabyDst[nDstOffset + nBandSpace] = nFinalAlpha;
        }
        else
        {
            // RBG and RGBA
            if (nOutputBands >= 3)
            {
                if (branchingCondition(nG, nA, nOverlayG, nOverlayA))
                {
                    pabyDst[nDstOffset + nBandSpace] =
                        processComponent_lessEqual(nG, nA, nOverlayG,
                                                   nOverlayA);
                }
                else
                {
                    pabyDst[nDstOffset + nBandSpace] =
                        processComponent_greater(nG, nA, nOverlayG, nOverlayA);
                }

                if (branchingCondition(nB, nA, nOverlayB, nOverlayA))
                {
                    pabyDst[nDstOffset + 2 * nBandSpace] =
                        processComponent_lessEqual(nB, nA, nOverlayB,
                                                   nOverlayA);
                }
                else
                {
                    pabyDst[nDstOffset + 2 * nBandSpace] =
                        processComponent_greater(nB, nA, nOverlayB, nOverlayA);
                }
            }

            // RGBA
            if (nOutputBands == 4)
            {
                pabyDst[nDstOffset + 3 * nBandSpace] = nFinalAlpha;
            }
        }
        nDstOffset += nPixelSpace;
    }
}

/************************************************************************/
/*                       BlendSrcOverRGBA_SSE2()                        */
/************************************************************************/

#ifdef HAVE_SSE2
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static int BlendSrcOverRGBA_SSE2(const GByte *CPL_RESTRICT pabyR,
                                 const GByte *CPL_RESTRICT pabyG,
                                 const GByte *CPL_RESTRICT pabyB,
                                 const GByte *CPL_RESTRICT pabyA,
                                 const GByte *CPL_RESTRICT pabyOverlayR,
                                 const GByte *CPL_RESTRICT pabyOverlayG,
                                 const GByte *CPL_RESTRICT pabyOverlayB,
                                 const GByte *CPL_RESTRICT pabyOverlayA,
                                 GByte *CPL_RESTRICT pabyDst,
                                 GSpacing nBandSpace, int N, GByte nOpacity)
{
    // See scalar code after call to BlendSrcOverRGBA_SSE2() below for the
    // non-obfuscated formulas...

    const auto load_and_unpack = [](const void *p)
    {
        const auto zero = _mm_setzero_si128();
        auto overlayA = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        return std::make_pair(_mm_unpacklo_epi8(overlayA, zero),
                              _mm_unpackhi_epi8(overlayA, zero));
    };

    const auto pack_and_store = [](void *p, __m128i lo, __m128i hi)
    {
        _mm_storeu_si128(reinterpret_cast<__m128i *>(p),
                         _mm_packus_epi16(lo, hi));
    };

    const auto mul16bit_8bit_result = [](__m128i a, __m128i b)
    {
        const auto r255 = _mm_set1_epi16(255);
        return _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(a, b), r255), 8);
    };

    const auto opacity = _mm_set1_epi16(nOpacity);
    const auto r255 = _mm_set1_epi16(255);
    const int16_t *tabInvDstASigned =
        reinterpret_cast<const int16_t *>(gTabInvDstA.data());
    constexpr int REG_WIDTH = static_cast<int>(sizeof(opacity));

    int i = 0;
    for (; i <= N - REG_WIDTH; i += REG_WIDTH)
    {
        auto [overlayA_lo, overlayA_hi] = load_and_unpack(pabyOverlayA + i);
        auto [srcA_lo, srcA_hi] = load_and_unpack(pabyA + i);
        overlayA_lo = mul16bit_8bit_result(overlayA_lo, opacity);
        overlayA_hi = mul16bit_8bit_result(overlayA_hi, opacity);
        auto srcAMul255MinusOverlayA_lo =
            mul16bit_8bit_result(srcA_lo, _mm_sub_epi16(r255, overlayA_lo));
        auto srcAMul255MinusOverlayA_hi =
            mul16bit_8bit_result(srcA_hi, _mm_sub_epi16(r255, overlayA_hi));
        auto dstA_lo = _mm_add_epi16(overlayA_lo, srcAMul255MinusOverlayA_lo);
        auto dstA_hi = _mm_add_epi16(overlayA_hi, srcAMul255MinusOverlayA_hi);

        // This would be the equivalent of a "_mm_i16gather_epi16" operation
        // which does not exist...
        // invDstA_{i} = [tabInvDstASigned[dstA_{i}] for i in range(8)]
        auto invDstA_lo = _mm_undefined_si128();
        auto invDstA_hi = _mm_undefined_si128();
#define SET_INVDSTA(k)                                                         \
    do                                                                         \
    {                                                                          \
        const int idxLo = _mm_extract_epi16(dstA_lo, k);                       \
        const int idxHi = _mm_extract_epi16(dstA_hi, k);                       \
        invDstA_lo = _mm_insert_epi16(invDstA_lo, tabInvDstASigned[idxLo], k); \
        invDstA_hi = _mm_insert_epi16(invDstA_hi, tabInvDstASigned[idxHi], k); \
    } while (0)

        SET_INVDSTA(0);
        SET_INVDSTA(1);
        SET_INVDSTA(2);
        SET_INVDSTA(3);
        SET_INVDSTA(4);
        SET_INVDSTA(5);
        SET_INVDSTA(6);
        SET_INVDSTA(7);

        pack_and_store(pabyDst + i + 3 * nBandSpace, dstA_lo, dstA_hi);

#define PROCESS_COMPONENT(pabySrc, pabyOverlay, iBand)                         \
    do                                                                         \
    {                                                                          \
        auto [src_lo, src_hi] = load_and_unpack((pabySrc) + i);                \
        auto [overlay_lo, overlay_hi] = load_and_unpack((pabyOverlay) + i);    \
        auto dst_lo = _mm_srli_epi16(                                          \
            _mm_add_epi16(                                                     \
                _mm_add_epi16(                                                 \
                    _mm_mullo_epi16(overlay_lo, overlayA_lo),                  \
                    _mm_mullo_epi16(src_lo, srcAMul255MinusOverlayA_lo)),      \
                r255),                                                         \
            8);                                                                \
        auto dst_hi = _mm_srli_epi16(                                          \
            _mm_add_epi16(                                                     \
                _mm_add_epi16(                                                 \
                    _mm_mullo_epi16(overlay_hi, overlayA_hi),                  \
                    _mm_mullo_epi16(src_hi, srcAMul255MinusOverlayA_hi)),      \
                r255),                                                         \
            8);                                                                \
        dst_lo = mul16bit_8bit_result(dst_lo, invDstA_lo);                     \
        dst_hi = mul16bit_8bit_result(dst_hi, invDstA_hi);                     \
        pack_and_store(pabyDst + i + (iBand) * nBandSpace, dst_lo, dst_hi);    \
    } while (0)

        PROCESS_COMPONENT(pabyR, pabyOverlayR, 0);
        PROCESS_COMPONENT(pabyG, pabyOverlayG, 1);
        PROCESS_COMPONENT(pabyB, pabyOverlayB, 2);
    }
    return i;
}
#endif

template <bool bPixelSpaceIsOne>
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void BlendSrcOverRGBA_Generic(
    const GByte *CPL_RESTRICT pabyR, const GByte *CPL_RESTRICT pabyG,
    const GByte *CPL_RESTRICT pabyB, const GByte *CPL_RESTRICT pabyA,
    const GByte *CPL_RESTRICT pabyOverlayR,
    const GByte *CPL_RESTRICT pabyOverlayG,
    const GByte *CPL_RESTRICT pabyOverlayB,
    const GByte *CPL_RESTRICT pabyOverlayA, GByte *CPL_RESTRICT pabyDst,
    [[maybe_unused]] GSpacing nPixelSpace, GSpacing nBandSpace, int i, int N,
    GByte nOpacity)
{
#if !(defined(HAVE_SSE2) && defined(__GNUC__))
    if constexpr (!bPixelSpaceIsOne)
    {
        assert(nPixelSpace != 1);
    }
#endif
    [[maybe_unused]] GSpacing nDstOffset = 0;
#if defined(HAVE_SSE2) && defined(__clang__) && !defined(__INTEL_CLANG_COMPILER)
// No need to vectorize for SSE2 and clang
#pragma clang loop vectorize(disable)
#endif
    for (; i < N; ++i)
    {
        const GByte nOverlayR = pabyOverlayR[i];
        const GByte nOverlayG = pabyOverlayG[i];
        const GByte nOverlayB = pabyOverlayB[i];
        const GByte nOverlayA =
            static_cast<GByte>((pabyOverlayA[i] * nOpacity + 255) / 256);
        const GByte nR = pabyR[i];
        const GByte nG = pabyG[i];
        const GByte nB = pabyB[i];
        const GByte nA = pabyA[i];
        const GByte nSrcAMul255MinusOverlayA =
            static_cast<GByte>((nA * (255 - nOverlayA) + 255) / 256);
        const GByte nDstA =
            static_cast<GByte>(nOverlayA + nSrcAMul255MinusOverlayA);
        GByte nDstR = static_cast<GByte>(
            (nOverlayR * nOverlayA + nR * nSrcAMul255MinusOverlayA + 255) /
            256);
        GByte nDstG = static_cast<GByte>(
            (nOverlayG * nOverlayA + nG * nSrcAMul255MinusOverlayA + 255) /
            256);
        GByte nDstB = static_cast<GByte>(
            (nOverlayB * nOverlayA + nB * nSrcAMul255MinusOverlayA + 255) /
            256);
        // nInvDstA = (255 << SHIFT_DIV_DSTA) / nDstA;
        const uint16_t nInvDstA = gTabInvDstA[nDstA];
        constexpr unsigned ROUND_OFFSET_DIV_DSTA = ((1 << SHIFT_DIV_DSTA) - 1);
        nDstR = static_cast<GByte>((nDstR * nInvDstA + ROUND_OFFSET_DIV_DSTA) >>
                                   SHIFT_DIV_DSTA);
        nDstG = static_cast<GByte>((nDstG * nInvDstA + ROUND_OFFSET_DIV_DSTA) >>
                                   SHIFT_DIV_DSTA);
        nDstB = static_cast<GByte>((nDstB * nInvDstA + ROUND_OFFSET_DIV_DSTA) >>
                                   SHIFT_DIV_DSTA);
        if constexpr (bPixelSpaceIsOne)
        {
            pabyDst[i] = nDstR;
            pabyDst[i + nBandSpace] = nDstG;
            pabyDst[i + 2 * nBandSpace] = nDstB;
            pabyDst[i + 3 * nBandSpace] = nDstA;
        }
        else
        {
            pabyDst[nDstOffset] = nDstR;
            pabyDst[nDstOffset + nBandSpace] = nDstG;
            pabyDst[nDstOffset + 2 * nBandSpace] = nDstB;
            pabyDst[nDstOffset + 3 * nBandSpace] = nDstA;
            nDstOffset += nPixelSpace;
        }
    }
}

/************************************************************************/
/*                      BlendDataset::IRasterIO()                       */
/************************************************************************/

CPLErr BlendDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                               int nXSize, int nYSize, void *pData,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, int nBandCount,
                               BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                               GSpacing nLineSpace, GSpacing nBandSpace,
                               GDALRasterIOExtraArg *psExtraArg)
{
    // Try to pass the request to the most appropriate overview dataset.
    if (nBufXSize < nXSize && nBufYSize < nYSize)
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    GByte *const CPL_RESTRICT pabyDst = static_cast<GByte *>(pData);
    const int nColorCount = m_oColorDS.GetRasterCount();
    const int nOverlayCount = m_oOverlayDS.GetRasterCount();

    /************************************************************************/
    /* HSV_VALUE                                                            */
    /*************************************************************************/
    if (nOverlayCount == 1 && m_opacity255Scale == 255 &&
        m_operator == CompositionMode::HSV_VALUE && eRWFlag == GF_Read &&
        eBufType == GDT_UInt8 && nBandCount == nBands &&
        IsAllBands(nBands, panBandMap) &&
        AcquireSourcePixels(nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                            psExtraArg))
    {
        const size_t nPixelCount = static_cast<size_t>(nBufXSize) * nBufYSize;
        const GByte *pabyR = m_abyBuffer.data();
        const GByte *pabyG = m_abyBuffer.data() + nPixelCount;
        const GByte *pabyB = m_abyBuffer.data() + nPixelCount * 2;
        const GByte *pabyValue = m_abyBuffer.data() + nPixelCount * nColorCount;
        size_t nSrcIdx = 0;
        for (int j = 0; j < nBufYSize; ++j)
        {
            auto nDstOffset = j * nLineSpace;
            if (nPixelSpace == 1 && nLineSpace >= nPixelSpace * nBufXSize &&
                nBandSpace >= nLineSpace * nBufYSize)
            {
                patch_value_line(nBufXSize, pabyR + nSrcIdx, pabyG + nSrcIdx,
                                 pabyB + nSrcIdx, pabyValue + nSrcIdx,
                                 pabyDst + nDstOffset,
                                 pabyDst + nDstOffset + nBandSpace,
                                 pabyDst + nDstOffset + 2 * nBandSpace);
                nSrcIdx += nBufXSize;
            }
            else
            {
                for (int i = 0; i < nBufXSize;
                     ++i, ++nSrcIdx, nDstOffset += nPixelSpace)
                {
                    float h, s;
                    rgb_to_hs(pabyR[nSrcIdx], pabyG[nSrcIdx], pabyB[nSrcIdx],
                              &h, &s);
                    hsv_to_rgb(h, s, pabyValue[nSrcIdx],
                               &pabyDst[nDstOffset + 0 * nBandSpace],
                               &pabyDst[nDstOffset + 1 * nBandSpace],
                               &pabyDst[nDstOffset + 2 * nBandSpace]);
                }
            }
        }
        if (nColorCount == 4)
        {
            for (int j = 0; j < nBufYSize; ++j)
            {
                auto nDstOffset = 3 * nBandSpace + j * nLineSpace;
                const GByte *pabyA = m_abyBuffer.data() + nPixelCount * 3;
                GDALCopyWords64(pabyA, GDT_UInt8, 1, pabyDst + nDstOffset,
                                GDT_UInt8, static_cast<int>(nPixelSpace),
                                nBufXSize);
            }
        }

        return CE_None;
    }

    /************************************************************************/
    /* SRC_OVER                                                             */
    /************************************************************************/
    else if (nOverlayCount == 4 && nColorCount == 4 &&
             m_operator == CompositionMode::SRC_OVER && eRWFlag == GF_Read &&
             eBufType == GDT_UInt8 && nBandCount == nBands &&
             IsAllBands(nBands, panBandMap) &&
             AcquireSourcePixels(nXOff, nYOff, nXSize, nYSize, nBufXSize,
                                 nBufYSize, psExtraArg))
    {
        const GByte nOpacity = static_cast<GByte>(m_opacity255Scale);
        const size_t nPixelCount = static_cast<size_t>(nBufXSize) * nBufYSize;
        const GByte *CPL_RESTRICT pabyR = m_abyBuffer.data();
        const GByte *CPL_RESTRICT pabyG = m_abyBuffer.data() + nPixelCount;
        const GByte *CPL_RESTRICT pabyB = m_abyBuffer.data() + nPixelCount * 2;
        const GByte *CPL_RESTRICT pabyA = m_abyBuffer.data() + nPixelCount * 3;
        const GByte *CPL_RESTRICT pabyOverlayR =
            m_abyBuffer.data() + nPixelCount * nColorCount;
        const GByte *CPL_RESTRICT pabyOverlayG =
            m_abyBuffer.data() + nPixelCount * (nColorCount + 1);
        const GByte *CPL_RESTRICT pabyOverlayB =
            m_abyBuffer.data() + nPixelCount * (nColorCount + 2);
        const GByte *CPL_RESTRICT pabyOverlayA =
            m_abyBuffer.data() + nPixelCount * (nColorCount + 3);
        size_t nSrcIdx = 0;
        for (int j = 0; j < nBufYSize; ++j)
        {
            auto nDstOffset = j * nLineSpace;
            int i = 0;
#ifdef HAVE_SSE2
            if (nPixelSpace == 1)
            {
                i = BlendSrcOverRGBA_SSE2(
                    pabyR + nSrcIdx, pabyG + nSrcIdx, pabyB + nSrcIdx,
                    pabyA + nSrcIdx, pabyOverlayR + nSrcIdx,
                    pabyOverlayG + nSrcIdx, pabyOverlayB + nSrcIdx,
                    pabyOverlayA + nSrcIdx, pabyDst + nDstOffset, nBandSpace,
                    nBufXSize, nOpacity);
                nSrcIdx += i;
                nDstOffset += i;
            }
#endif
#if !(defined(HAVE_SSE2) && defined(__GNUC__))
            if (nPixelSpace == 1)
            {
                // Note: clang 20 does a rather good job at autovectorizing
                // for SSE2, but BlendSrcOverRGBA_SSE2() performs better.
                BlendSrcOverRGBA_Generic</* bPixelSpaceIsOne = */ true>(
                    pabyR + nSrcIdx, pabyG + nSrcIdx, pabyB + nSrcIdx,
                    pabyA + nSrcIdx, pabyOverlayR + nSrcIdx,
                    pabyOverlayG + nSrcIdx, pabyOverlayB + nSrcIdx,
                    pabyOverlayA + nSrcIdx, pabyDst + nDstOffset,
                    /*nPixelSpace = */ 1, nBandSpace, i, nBufXSize, nOpacity);
            }
            else
#endif
            {
                BlendSrcOverRGBA_Generic</* bPixelSpaceIsOne = */ false>(
                    pabyR + nSrcIdx, pabyG + nSrcIdx, pabyB + nSrcIdx,
                    pabyA + nSrcIdx, pabyOverlayR + nSrcIdx,
                    pabyOverlayG + nSrcIdx, pabyOverlayB + nSrcIdx,
                    pabyOverlayA + nSrcIdx, pabyDst + nDstOffset, nPixelSpace,
                    nBandSpace, i, nBufXSize, nOpacity);
            }
            nSrcIdx += nBufXSize - i;
        }
        return CE_None;
    }

    /************************************************************************/
    /* OTHER OPERATORS                                                      */
    /************************************************************************/

    else if ((m_operator == CompositionMode::MULTIPLY ||
              m_operator == CompositionMode::OVERLAY ||
              m_operator == CompositionMode::SCREEN ||
              m_operator == CompositionMode::HARD_LIGHT ||
              m_operator == CompositionMode::DARKEN ||
              m_operator == CompositionMode::LIGHTEN ||
              m_operator == CompositionMode::COLOR_BURN ||
              m_operator == CompositionMode::COLOR_DODGE) &&
             eRWFlag == GF_Read && eBufType == GDT_UInt8 &&
             nBandCount == nBands && IsAllBands(nBands, panBandMap) &&
             AcquireSourcePixels(nXOff, nYOff, nXSize, nYSize, nBufXSize,
                                 nBufYSize, psExtraArg))
    {
        // We should have optimized paths for 1, 2, 3 and 4 bands on input and overlay
        // permutations but let's keep it simple for now.
        const GByte nOpacity = static_cast<GByte>(m_opacity255Scale);
        const size_t nPixelCount = static_cast<size_t>(nBufXSize) * nBufYSize;
        const GByte *CPL_RESTRICT pabyR = m_abyBuffer.data();
        GByte *CPL_RESTRICT pabyG = nullptr;
        GByte *CPL_RESTRICT pabyB = nullptr;
        GByte *CPL_RESTRICT pabyA = nullptr;
        switch (nColorCount)
        {
            case 2:
                pabyA = m_abyBuffer.data() + nPixelCount;
                break;
            case 3:
                pabyG = m_abyBuffer.data() + nPixelCount;
                pabyB = m_abyBuffer.data() + nPixelCount * 2;
                break;
            case 4:
                pabyG = m_abyBuffer.data() + nPixelCount;
                pabyB = m_abyBuffer.data() + nPixelCount * 2;
                pabyA = m_abyBuffer.data() + nPixelCount * 3;
                break;
        }

        const GByte *CPL_RESTRICT pabyOverlayR =
            m_abyBuffer.data() + nPixelCount * nColorCount;
        GByte *CPL_RESTRICT pabyOverlayG = nullptr;
        GByte *CPL_RESTRICT pabyOverlayB = nullptr;
        GByte *CPL_RESTRICT pabyOverlayA = nullptr;
        switch (nOverlayCount)
        {
            case 2:
                pabyOverlayA =
                    m_abyBuffer.data() + nPixelCount * (nColorCount + 1);
                break;
            case 3:
                pabyOverlayG =
                    m_abyBuffer.data() + nPixelCount * (nColorCount + 1);
                pabyOverlayB =
                    m_abyBuffer.data() + nPixelCount * (nColorCount + 2);
                break;
            case 4:
                pabyOverlayG =
                    m_abyBuffer.data() + nPixelCount * (nColorCount + 1);
                pabyOverlayB =
                    m_abyBuffer.data() + nPixelCount * (nColorCount + 2);
                pabyOverlayA =
                    m_abyBuffer.data() + nPixelCount * (nColorCount + 3);
                break;
        }

        size_t nSrcIdx = 0;
        for (int j = 0; j < nBufYSize; ++j)
        {
            auto nDstOffset = j * nLineSpace;
            int i = 0;

            const GByte *CPL_RESTRICT pabyOverlayG_current =
                pabyOverlayG ? pabyOverlayG + nSrcIdx : nullptr;
            const GByte *CPL_RESTRICT pabyOverlayB_current =
                pabyOverlayB ? pabyOverlayB + nSrcIdx : nullptr;
            const GByte *CPL_RESTRICT pabyOverlayA_current =
                pabyOverlayA ? pabyOverlayA + nSrcIdx : nullptr;

            const GByte *CPL_RESTRICT pabyG_current =
                pabyG ? pabyG + nSrcIdx : nullptr;
            const GByte *CPL_RESTRICT pabyB_current =
                pabyB ? pabyB + nSrcIdx : nullptr;
            const GByte *CPL_RESTRICT pabyA_current =
                pabyA ? pabyA + nSrcIdx : nullptr;

            // Determine the number of  bands
            const int nInputBands{1 + (pabyG ? 2 : 0) + (pabyA ? 1 : 0)};
            const int nOverlayBands{1 + (pabyOverlayG ? 2 : 0) +
                                    (pabyOverlayA ? 1 : 0)};
            const int nOutputBands = std::max(nInputBands, nOverlayBands);

            if (m_operator == CompositionMode::SCREEN)
                BlendScreen_Generic(
                    pabyR + nSrcIdx, pabyG_current, pabyB_current,
                    pabyA_current, pabyOverlayR + nSrcIdx, pabyOverlayG_current,
                    pabyOverlayB_current, pabyOverlayA_current,
                    pabyDst + nDstOffset, nPixelSpace, nBandSpace, i, nBufXSize,
                    nOpacity, nOutputBands, m_bSwappedOpacity);
            else if (m_operator == CompositionMode::MULTIPLY)
                BlendMultiply_Generic(
                    pabyR + nSrcIdx, pabyG_current, pabyB_current,
                    pabyA_current, pabyOverlayR + nSrcIdx, pabyOverlayG_current,
                    pabyOverlayB_current, pabyOverlayA_current,
                    pabyDst + nDstOffset, nPixelSpace, nBandSpace, i, nBufXSize,
                    nOpacity, nOutputBands, m_bSwappedOpacity);
            else if (m_operator == CompositionMode::HARD_LIGHT)
                BlendHardLight_Generic(
                    pabyR + nSrcIdx, pabyG_current, pabyB_current,
                    pabyA_current, pabyOverlayR + nSrcIdx, pabyOverlayG_current,
                    pabyOverlayB_current, pabyOverlayA_current,
                    pabyDst + nDstOffset, nPixelSpace, nBandSpace, i, nBufXSize,
                    nOpacity, nOutputBands, m_bSwappedOpacity);
            else if (m_operator == CompositionMode::OVERLAY)
                BlendOverlay_Generic(
                    pabyR + nSrcIdx, pabyG_current, pabyB_current,
                    pabyA_current, pabyOverlayR + nSrcIdx, pabyOverlayG_current,
                    pabyOverlayB_current, pabyOverlayA_current,
                    pabyDst + nDstOffset, nPixelSpace, nBandSpace, i, nBufXSize,
                    nOpacity, nOutputBands, m_bSwappedOpacity);
            else if (m_operator == CompositionMode::DARKEN)
                BlendDarken_Generic(
                    pabyR + nSrcIdx, pabyG_current, pabyB_current,
                    pabyA_current, pabyOverlayR + nSrcIdx, pabyOverlayG_current,
                    pabyOverlayB_current, pabyOverlayA_current,
                    pabyDst + nDstOffset, nPixelSpace, nBandSpace, i, nBufXSize,
                    nOpacity, nOutputBands, m_bSwappedOpacity);
            else if (m_operator == CompositionMode::LIGHTEN)
                BlendLighten_Generic(
                    pabyR + nSrcIdx, pabyG_current, pabyB_current,
                    pabyA_current, pabyOverlayR + nSrcIdx, pabyOverlayG_current,
                    pabyOverlayB_current, pabyOverlayA_current,
                    pabyDst + nDstOffset, nPixelSpace, nBandSpace, i, nBufXSize,
                    nOpacity, nOutputBands, m_bSwappedOpacity);
            else if (m_operator == CompositionMode::COLOR_BURN)
                BlendColorBurn_Generic(
                    pabyR + nSrcIdx, pabyG_current, pabyB_current,
                    pabyA_current, pabyOverlayR + nSrcIdx, pabyOverlayG_current,
                    pabyOverlayB_current, pabyOverlayA_current,
                    pabyDst + nDstOffset, nPixelSpace, nBandSpace, i, nBufXSize,
                    nOpacity, nOutputBands, m_bSwappedOpacity);
            else if (m_operator == CompositionMode::COLOR_DODGE)
                BlendColorDodge_Generic(
                    pabyR + nSrcIdx, pabyG_current, pabyB_current,
                    pabyA_current, pabyOverlayR + nSrcIdx, pabyOverlayG_current,
                    pabyOverlayB_current, pabyOverlayA_current,
                    pabyDst + nDstOffset, nPixelSpace, nBandSpace, i, nBufXSize,
                    nOpacity, nOutputBands, m_bSwappedOpacity);
            else
            {
                CPLAssert(false);
            }

            nSrcIdx += nBufXSize - i;
        }
        return CE_None;
    }

    /************************************************************************/
    /* ERRORS                                                               */
    /************************************************************************/
    else if (m_ioError)
    {
        return CE_Failure;
    }
    else
    {
        const CPLErr eErr = GDALDataset::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg);
        m_ioError = eErr != CE_None;
        return eErr;
    }
}

/************************************************************************/
/*                       SrcOverRGBOneComponent()                       */
/************************************************************************/

// GCC and clang do a god job a auto vectorizing the below function
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("tree-vectorize")))
#endif
static void SrcOverRGB(const uint8_t *const __restrict pabyOverlay,
                       const uint8_t *const __restrict pabySrc,
                       uint8_t *const __restrict pabyDst, const size_t N,
                       const uint8_t nOpacity)
{
    for (size_t i = 0; i < N; ++i)
    {
        const uint8_t nOverlay = pabyOverlay[i];
        const uint8_t nSrc = pabySrc[i];
        pabyDst[i] = static_cast<uint8_t>(
            (nOverlay * nOpacity + nSrc * (255 - nOpacity) + 255) / 256);
    }
}

/************************************************************************/
/*                        BlendBand::IRasterIO()                        */
/************************************************************************/

CPLErr BlendBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                            int nXSize, int nYSize, void *pData, int nBufXSize,
                            int nBufYSize, GDALDataType eBufType,
                            GSpacing nPixelSpace, GSpacing nLineSpace,
                            GDALRasterIOExtraArg *psExtraArg)
{
    // Try to pass the request to the most appropriate overview dataset.
    if (nBufXSize < nXSize && nBufYSize < nYSize)
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    const size_t nPixelCount = static_cast<size_t>(nBufXSize) * nBufYSize;
    const int nColorCount = m_oBlendDataset.m_oColorDS.GetRasterCount();
    const int nOverlayCount = m_oBlendDataset.m_oOverlayDS.GetRasterCount();
    if (nBand == 4 && m_oBlendDataset.m_operator == CompositionMode::HSV_VALUE)
    {
        if (nColorCount == 3)
        {
            GByte ch = 255;
            for (int iY = 0; iY < nBufYSize; ++iY)
            {
                GDALCopyWords64(&ch, GDT_UInt8, 0,
                                static_cast<GByte *>(pData) + iY * nLineSpace,
                                eBufType, static_cast<int>(nPixelSpace),
                                nBufXSize);
            }
            return CE_None;
        }
        else
        {
            CPLAssert(nColorCount == 4);
            return m_oBlendDataset.m_oColorDS.GetRasterBand(4)->RasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                nBufYSize, eBufType, nPixelSpace, nLineSpace, psExtraArg);
        }
    }
    else if (nOverlayCount == 3 && nColorCount == 3 &&
             m_oBlendDataset.m_operator == CompositionMode::SRC_OVER &&
             eRWFlag == GF_Read && eBufType == GDT_UInt8 &&
             m_oBlendDataset.AcquireSourcePixels(nXOff, nYOff, nXSize, nYSize,
                                                 nBufXSize, nBufYSize,
                                                 psExtraArg))
    {
        const int nOpacity = m_oBlendDataset.m_opacity255Scale;
        const GByte *const CPL_RESTRICT pabySrc =
            m_oBlendDataset.m_abyBuffer.data() + nPixelCount * (nBand - 1);
        const GByte *const CPL_RESTRICT pabyOverlay =
            m_oBlendDataset.m_abyBuffer.data() +
            nPixelCount * (nColorCount + nBand - 1);
        GByte *const CPL_RESTRICT pabyDst = static_cast<GByte *>(pData);
        size_t nSrcIdx = 0;
        for (int j = 0; j < nBufYSize; ++j)
        {
            auto nDstOffset = j * nLineSpace;
            if (nPixelSpace == 1)
            {
                SrcOverRGB(pabyOverlay + nSrcIdx, pabySrc + nSrcIdx,
                           pabyDst + nDstOffset, nBufXSize,
                           static_cast<uint8_t>(nOpacity));
                nSrcIdx += nBufXSize;
            }
            else
            {
                for (int i = 0; i < nBufXSize;
                     ++i, ++nSrcIdx, nDstOffset += nPixelSpace)
                {
                    const int nOverlay = pabyOverlay[nSrcIdx];
                    const int nSrc = pabySrc[nSrcIdx];
                    pabyDst[nDstOffset] = static_cast<GByte>(
                        (nOverlay * nOpacity + nSrc * (255 - nOpacity) + 255) /
                        256);
                }
            }
        }
        return CE_None;
    }
    else if (eRWFlag == GF_Read && eBufType == GDT_UInt8 &&
             m_oBlendDataset.AcquireSourcePixels(nXOff, nYOff, nXSize, nYSize,
                                                 nBufXSize, nBufYSize,
                                                 psExtraArg))
    {
        GByte *pabyDst = static_cast<GByte *>(pData);

        if (m_oBlendDataset.m_operator == CompositionMode::MULTIPLY ||
            m_oBlendDataset.m_operator == CompositionMode::SCREEN ||
            m_oBlendDataset.m_operator == CompositionMode::HARD_LIGHT ||
            m_oBlendDataset.m_operator == CompositionMode::OVERLAY ||
            m_oBlendDataset.m_operator == CompositionMode::DARKEN ||
            m_oBlendDataset.m_operator == CompositionMode::LIGHTEN ||
            m_oBlendDataset.m_operator == CompositionMode::COLOR_BURN ||
            m_oBlendDataset.m_operator == CompositionMode::COLOR_DODGE)
        {
            CPLAssert(nBand <= 4);
            const GByte *pabyR = m_oBlendDataset.m_abyBuffer.data();
            const GByte *pabyG =
                nColorCount >= 3
                    ? m_oBlendDataset.m_abyBuffer.data() + nPixelCount
                    : nullptr;
            const GByte *pabyB =
                nColorCount >= 3
                    ? m_oBlendDataset.m_abyBuffer.data() + nPixelCount * 2
                    : nullptr;

            GByte *pabyA = nullptr;

            if (nColorCount == 2)
            {
                pabyA = m_oBlendDataset.m_abyBuffer.data() + nPixelCount;
            }
            else if (nColorCount == 4)
            {
                pabyA = m_oBlendDataset.m_abyBuffer.data() + nPixelCount * 3;
            }

            // Retrieve single band value as R
            const GByte *pabyOverlayR =
                m_oBlendDataset.m_abyBuffer.data() + nPixelCount * nColorCount;

            const GByte *pabyOverlayG =
                nOverlayCount >= 3 ? m_oBlendDataset.m_abyBuffer.data() +
                                         nPixelCount * (nColorCount + 1)
                                   : nullptr;
            const GByte *pabyOverlayB =
                nOverlayCount >= 3 ? m_oBlendDataset.m_abyBuffer.data() +
                                         nPixelCount * (nColorCount + 2)
                                   : nullptr;
            const GByte *pabyOverlayA =
                (nOverlayCount == 2 || nOverlayCount == 4)
                    ? m_oBlendDataset.m_abyBuffer.data() +
                          nPixelCount * (nColorCount + nOverlayCount - 1)
                    : nullptr;

            // Determine the number of bands
            const int nInputBands{1 + (pabyG ? 2 : 0) + (pabyA ? 1 : 0)};
            const int nOverlayBands{1 + (pabyOverlayG ? 2 : 0) +
                                    (pabyOverlayA ? 1 : 0)};
            const int nOutputBands = std::max(nInputBands, nOverlayBands);
            const bool bSwappedOpacity{m_oBlendDataset.m_bSwappedOpacity};

            size_t nSrcIdx = 0;
            for (int j = 0; j < nBufYSize; ++j)
            {
                auto nDstOffset = j * nLineSpace;
                for (int i = 0; i < nBufXSize;
                     ++i, ++nSrcIdx, nDstOffset += nPixelSpace)
                {
                    // TODO: This need to be optimized for requesting a single band
                    std::vector<GByte> byteBuffer;
                    byteBuffer.resize(std::max(nColorCount, nOverlayCount));
                    if (m_oBlendDataset.m_operator == CompositionMode::SCREEN)
                        BlendScreen_Generic(
                            pabyR, pabyG, pabyB, pabyA, pabyOverlayR,
                            pabyOverlayG, pabyOverlayB, pabyOverlayA,
                            byteBuffer.data(), nPixelSpace, 1, nSrcIdx, 1,
                            static_cast<GByte>(
                                m_oBlendDataset.m_opacity255Scale),
                            nOutputBands, bSwappedOpacity);
                    else if (m_oBlendDataset.m_operator ==
                             CompositionMode::HARD_LIGHT)
                        BlendHardLight_Generic(
                            pabyR, pabyG, pabyB, pabyA, pabyOverlayR,
                            pabyOverlayG, pabyOverlayB, pabyOverlayA,
                            byteBuffer.data(), nPixelSpace, 1, nSrcIdx, 1,
                            static_cast<GByte>(
                                m_oBlendDataset.m_opacity255Scale),
                            nOutputBands, bSwappedOpacity);
                    else if (m_oBlendDataset.m_operator ==
                             CompositionMode::OVERLAY)
                        BlendOverlay_Generic(
                            pabyR, pabyG, pabyB, pabyA, pabyOverlayR,
                            pabyOverlayG, pabyOverlayB, pabyOverlayA,
                            byteBuffer.data(), nPixelSpace, 1, nSrcIdx, 1,
                            static_cast<GByte>(
                                m_oBlendDataset.m_opacity255Scale),
                            nOutputBands, bSwappedOpacity);
                    else if (m_oBlendDataset.m_operator ==
                             CompositionMode::MULTIPLY)
                        BlendMultiply_Generic(
                            pabyR, pabyG, pabyB, pabyA, pabyOverlayR,
                            pabyOverlayG, pabyOverlayB, pabyOverlayA,
                            byteBuffer.data(), nPixelSpace, 1, nSrcIdx, 1,
                            static_cast<GByte>(
                                m_oBlendDataset.m_opacity255Scale),
                            nOutputBands, bSwappedOpacity);
                    else if (m_oBlendDataset.m_operator ==
                             CompositionMode::DARKEN)
                        BlendDarken_Generic(
                            pabyR, pabyG, pabyB, pabyA, pabyOverlayR,
                            pabyOverlayG, pabyOverlayB, pabyOverlayA,
                            byteBuffer.data(), nPixelSpace, 1, nSrcIdx, 1,
                            static_cast<GByte>(
                                m_oBlendDataset.m_opacity255Scale),
                            nOutputBands, bSwappedOpacity);
                    else if (m_oBlendDataset.m_operator ==
                             CompositionMode::LIGHTEN)
                        BlendLighten_Generic(
                            pabyR, pabyG, pabyB, pabyA, pabyOverlayR,
                            pabyOverlayG, pabyOverlayB, pabyOverlayA,
                            byteBuffer.data(), nPixelSpace, 1, nSrcIdx, 1,
                            static_cast<GByte>(
                                m_oBlendDataset.m_opacity255Scale),
                            nOutputBands, bSwappedOpacity);
                    else if (m_oBlendDataset.m_operator ==
                             CompositionMode::COLOR_BURN)
                        BlendColorBurn_Generic(
                            pabyR, pabyG, pabyB, pabyA, pabyOverlayR,
                            pabyOverlayG, pabyOverlayB, pabyOverlayA,
                            byteBuffer.data(), nPixelSpace, 1, nSrcIdx, 1,
                            static_cast<GByte>(
                                m_oBlendDataset.m_opacity255Scale),
                            nOutputBands, bSwappedOpacity);
                    else if (m_oBlendDataset.m_operator ==
                             CompositionMode::COLOR_DODGE)
                        BlendColorDodge_Generic(
                            pabyR, pabyG, pabyB, pabyA, pabyOverlayR,
                            pabyOverlayG, pabyOverlayB, pabyOverlayA,
                            byteBuffer.data(), nPixelSpace, 1, nSrcIdx, 1,
                            static_cast<GByte>(
                                m_oBlendDataset.m_opacity255Scale),
                            nOutputBands, bSwappedOpacity);
                    else
                    {
                        CPLAssert(false);
                    }

                    pabyDst[nDstOffset] = byteBuffer[nBand - 1];
                }
            }
        }
        else if (m_oBlendDataset.m_operator == CompositionMode::SRC_OVER)
        {
            const auto RGBToGrayScale = [](int R, int G, int B)
            {
                // Equivalent to R * 0.299 + G * 0.587 + B * 0.114
                // but using faster computation
                return (R * 306 + G * 601 + B * 117) / 1024;
            };

            const GByte *paby =
                (nBand <= nColorCount) ? m_oBlendDataset.m_abyBuffer.data() +
                                             nPixelCount * (nBand - 1)
                : (nBand == 4 && nColorCount == 2)
                    ? m_oBlendDataset.m_abyBuffer.data() + nPixelCount
                    : nullptr;
            const GByte *pabyA =
                (nColorCount == 4)
                    ? m_oBlendDataset.m_abyBuffer.data() + nPixelCount * 3
                : nColorCount == 2
                    ? m_oBlendDataset.m_abyBuffer.data() + nPixelCount
                    : nullptr;
            const GByte *pabyOverlay =
                (nBand <= nOverlayCount)
                    ? m_oBlendDataset.m_abyBuffer.data() +
                          nPixelCount * (nColorCount + nBand - 1)
                : (nBand <= 3) ? m_oBlendDataset.m_abyBuffer.data() +
                                     nPixelCount * nColorCount
                               : nullptr;
            const GByte *pabyOverlayA =
                (nOverlayCount == 2 || nOverlayCount == 4)
                    ? m_oBlendDataset.m_abyBuffer.data() +
                          nPixelCount * (nColorCount + nOverlayCount - 1)
                    : nullptr;
            const GByte *pabyOverlayR =
                (nOverlayCount >= 3 && nColorCount < 3 && nBand <= 3)
                    ? m_oBlendDataset.m_abyBuffer.data() +
                          nPixelCount * nColorCount
                    : nullptr;
            const GByte *pabyOverlayG =
                (nOverlayCount >= 3 && nColorCount < 3 && nBand <= 3)
                    ? m_oBlendDataset.m_abyBuffer.data() +
                          nPixelCount * (nColorCount + 1)
                    : nullptr;
            const GByte *pabyOverlayB =
                (nOverlayCount >= 3 && nColorCount < 3 && nBand <= 3)
                    ? m_oBlendDataset.m_abyBuffer.data() +
                          nPixelCount * (nColorCount + 2)
                    : nullptr;

            size_t nSrcIdx = 0;
            for (int j = 0; j < nBufYSize; ++j)
            {
                auto nDstOffset = j * nLineSpace;
                for (int i = 0; i < nBufXSize;
                     ++i, ++nSrcIdx, nDstOffset += nPixelSpace)
                {
                    // Corrected to take into account m_opacity255Scale
                    const int nOverlayA =
                        pabyOverlayA ? ((pabyOverlayA[nSrcIdx] *
                                             m_oBlendDataset.m_opacity255Scale +
                                         255) /
                                        256)
                                     : m_oBlendDataset.m_opacity255Scale;

                    const int nSrcA = pabyA ? pabyA[nSrcIdx] : 255;

                    const int nSrcAMul255MinusOverlayA =
                        (nSrcA * (255 - nOverlayA) + 255) / 256;
                    const int nDstA = nOverlayA + nSrcAMul255MinusOverlayA;
                    if (nBand != 4)
                    {
                        const int nOverlay =
                            (pabyOverlayR && pabyOverlayG && pabyOverlayB)
                                ? RGBToGrayScale(pabyOverlayR[nSrcIdx],
                                                 pabyOverlayG[nSrcIdx],
                                                 pabyOverlayB[nSrcIdx])
                            : pabyOverlay ? pabyOverlay[nSrcIdx]
                                          : 255;

                        const int nSrc = paby ? paby[nSrcIdx] : 255;
                        int nDst = (nOverlay * nOverlayA +
                                    nSrc * nSrcAMul255MinusOverlayA + 255) /
                                   256;
                        if (nDstA != 0 && nDstA != 255)
                            nDst = (nDst * 255 + (nDstA / 2)) / nDstA;
                        pabyDst[nDstOffset] =
                            static_cast<GByte>(std::min(nDst, 255));
                    }
                    else
                    {
                        pabyDst[nDstOffset] = static_cast<GByte>(nDstA);
                    }
                }
            }
        }
        else if (nOverlayCount == 1 && m_oBlendDataset.m_opacity255Scale == 255)
        {
            const GByte *pabyR = m_oBlendDataset.m_abyBuffer.data();
            const GByte *pabyG =
                m_oBlendDataset.m_abyBuffer.data() + nPixelCount;
            const GByte *pabyB =
                m_oBlendDataset.m_abyBuffer.data() + nPixelCount * 2;
            CPLAssert(m_oBlendDataset.m_operator == CompositionMode::HSV_VALUE);
            size_t nSrcIdx = 0;
            const GByte *pabyValue =
                m_oBlendDataset.m_abyBuffer.data() + nPixelCount * nColorCount;
            for (int j = 0; j < nBufYSize; ++j)
            {
                auto nDstOffset = j * nLineSpace;
                if (nPixelSpace == 1 && nLineSpace >= nPixelSpace * nBufXSize)
                {
                    patch_value_line(
                        nBufXSize, pabyR + nSrcIdx, pabyG + nSrcIdx,
                        pabyB + nSrcIdx, pabyValue + nSrcIdx,
                        nBand == 1 ? pabyDst + nDstOffset : nullptr,
                        nBand == 2 ? pabyDst + nDstOffset : nullptr,
                        nBand == 3 ? pabyDst + nDstOffset : nullptr);
                    nSrcIdx += nBufXSize;
                }
                else
                {
                    for (int i = 0; i < nBufXSize;
                         ++i, ++nSrcIdx, nDstOffset += nPixelSpace)
                    {
                        float h, s;
                        rgb_to_hs(pabyR[nSrcIdx], pabyG[nSrcIdx],
                                  pabyB[nSrcIdx], &h, &s);
                        if (nBand == 1)
                        {
                            hsv_to_rgb(h, s, pabyValue[nSrcIdx],
                                       &pabyDst[nDstOffset], nullptr, nullptr);
                        }
                        else if (nBand == 2)
                        {
                            hsv_to_rgb(h, s, pabyValue[nSrcIdx], nullptr,
                                       &pabyDst[nDstOffset], nullptr);
                        }
                        else
                        {
                            CPLAssert(nBand == 3);
                            hsv_to_rgb(h, s, pabyValue[nSrcIdx], nullptr,
                                       nullptr, &pabyDst[nDstOffset]);
                        }
                    }
                }
            }
        }
        else
        {
            CPLAssert(m_oBlendDataset.m_operator == CompositionMode::HSV_VALUE);
            CPLAssert(nBand <= 3);
            const GByte *pabyR = m_oBlendDataset.m_abyBuffer.data();
            const GByte *pabyG =
                m_oBlendDataset.m_abyBuffer.data() + nPixelCount;
            const GByte *pabyB =
                m_oBlendDataset.m_abyBuffer.data() + nPixelCount * 2;
            const GByte *pabyValue =
                m_oBlendDataset.m_abyBuffer.data() + nPixelCount * nColorCount;
            const GByte *pabyOverlayR =
                nOverlayCount >= 3 ? m_oBlendDataset.m_abyBuffer.data() +
                                         nPixelCount * nColorCount
                                   : nullptr;
            const GByte *pabyOverlayG =
                nOverlayCount >= 3 ? m_oBlendDataset.m_abyBuffer.data() +
                                         nPixelCount * (nColorCount + 1)
                                   : nullptr;
            const GByte *pabyOverlayB =
                nOverlayCount >= 3 ? m_oBlendDataset.m_abyBuffer.data() +
                                         nPixelCount * (nColorCount + 2)
                                   : nullptr;
            const GByte *pabyOverlayA =
                (nOverlayCount == 2 || nOverlayCount == 4)
                    ? m_oBlendDataset.m_abyBuffer.data() +
                          nPixelCount * (nColorCount + nOverlayCount - 1)
                    : nullptr;

            size_t nSrcIdx = 0;
            for (int j = 0; j < nBufYSize; ++j)
            {
                auto nDstOffset = j * nLineSpace;
                for (int i = 0; i < nBufXSize;
                     ++i, ++nSrcIdx, nDstOffset += nPixelSpace)
                {
                    const int nColorR = pabyR[nSrcIdx];
                    const int nColorG = pabyG[nSrcIdx];
                    const int nColorB = pabyB[nSrcIdx];
                    const int nOverlayV =
                        (pabyOverlayR && pabyOverlayG && pabyOverlayB)
                            ? std::max({pabyOverlayR[nSrcIdx],
                                        pabyOverlayG[nSrcIdx],
                                        pabyOverlayB[nSrcIdx]})
                            : pabyValue[nSrcIdx];
                    const int nOverlayA =
                        pabyOverlayA ? ((pabyOverlayA[nSrcIdx] *
                                             m_oBlendDataset.m_opacity255Scale +
                                         255) /
                                        256)
                                     : m_oBlendDataset.m_opacity255Scale;
                    const int nColorValue =
                        std::max({nColorR, nColorG, nColorB});

                    float h, s;
                    rgb_to_hs(pabyR[nSrcIdx], pabyG[nSrcIdx], pabyB[nSrcIdx],
                              &h, &s);

                    const GByte nTargetValue = static_cast<GByte>(
                        (nOverlayV * nOverlayA +
                         nColorValue * (255 - nOverlayA) + 255) /
                        256);

                    if (nBand == 1)
                    {
                        hsv_to_rgb(h, s, nTargetValue, &pabyDst[nDstOffset],
                                   nullptr, nullptr);
                    }
                    else if (nBand == 2)
                    {
                        hsv_to_rgb(h, s, nTargetValue, nullptr,
                                   &pabyDst[nDstOffset], nullptr);
                    }
                    else
                    {
                        CPLAssert(nBand == 3);
                        hsv_to_rgb(h, s, nTargetValue, nullptr, nullptr,
                                   &pabyDst[nDstOffset]);
                    }
                }
            }
        }

        return CE_None;
    }
    else if (m_oBlendDataset.m_ioError)
    {
        return CE_Failure;
    }
    else
    {
        const CPLErr eErr = GDALRasterBand::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg);
        m_oBlendDataset.m_ioError = eErr != CE_None;
        return eErr;
    }
}

}  // namespace

/************************************************************************/
/*              GDALRasterBlendAlgorithm::ValidateGlobal()              */
/************************************************************************/

bool GDALRasterBlendAlgorithm::ValidateGlobal()
{
    auto poSrcDS =
        m_inputDataset.empty() ? nullptr : m_inputDataset[0].GetDatasetRef();
    auto poOverlayDS = m_overlayDataset.GetDatasetRef();
    if (poSrcDS)
    {
        if (poSrcDS->GetRasterCount() == 0 || poSrcDS->GetRasterCount() > 4 ||
            poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt8)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only 1-band, 2-band, 3-band or 4-band Byte dataset "
                        "supported as input");
            return false;
        }
    }
    if (poOverlayDS)
    {
        if (poOverlayDS->GetRasterCount() == 0 ||
            poOverlayDS->GetRasterCount() > 4 ||
            poOverlayDS->GetRasterBand(1)->GetRasterDataType() != GDT_UInt8)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only 1-band, 2-band, 3-band or 4-band Byte dataset "
                        "supported as overlay");
            return false;
        }
    }

    if (poSrcDS && poOverlayDS)
    {
        if (poSrcDS->GetRasterXSize() != poOverlayDS->GetRasterXSize() ||
            poSrcDS->GetRasterYSize() != poOverlayDS->GetRasterYSize())
        {
            ReportError(CE_Failure, CPLE_IllegalArg,
                        "Input dataset and overlay dataset must have "
                        "the same dimensions");
            return false;
        }

        if (!BandCountIsCompatibleWithCompositionMode(poSrcDS->GetRasterCount(),
                                                      m_operator))
        {
            const int minRequiredBands{
                MinBandCountForCompositionMode(m_operator)};
            const int maxRequiredBands{
                MaxBandCountForCompositionMode(m_operator)};
            if (minRequiredBands != maxRequiredBands)
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Input dataset has %d band(s), but operator %s "
                            "requires between %d and %d bands",
                            poSrcDS->GetRasterCount(),
                            CompositionModeToString(m_operator).c_str(),
                            minRequiredBands, maxRequiredBands);
            else
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Input dataset has %d band(s), but operator %s "
                            "requires %d bands",
                            poSrcDS->GetRasterCount(),
                            CompositionModeToString(m_operator).c_str(),
                            minRequiredBands);
            return false;
        }
    }

    // Check that for LIGHTEN and DARKEN, the source dataset and destination dataset
    // have the same number of color bands (do not consider alpha)
    if (poSrcDS && poOverlayDS &&
        (m_operator == CompositionMode::LIGHTEN ||
         m_operator == CompositionMode::DARKEN))
    {
        const int nSrcColorBands =
            (poSrcDS->GetRasterCount() == 2 || poSrcDS->GetRasterCount() == 4)
                ? poSrcDS->GetRasterCount() - 1
                : poSrcDS->GetRasterCount();
        const int nOverlayColorBands = (poOverlayDS->GetRasterCount() == 2 ||
                                        poOverlayDS->GetRasterCount() == 4)
                                           ? poOverlayDS->GetRasterCount() - 1
                                           : poOverlayDS->GetRasterCount();
        if (nSrcColorBands != nOverlayColorBands)
        {
            ReportError(
                CE_Failure, CPLE_IllegalArg,
                "For LIGHTEN and DARKEN operators, the source dataset "
                "and overlay dataset must have the same number of "
                "bands (without considering alpha). They have %d and %d "
                "bands respectively",
                nSrcColorBands, nOverlayColorBands);
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                 GDALRasterBlendAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterBlendAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poOverlayDS = m_overlayDataset.GetDatasetRef();
    CPLAssert(poOverlayDS);

    // If any of the dataset single band has a color table implicitly convert it to RGBA by calling
    // GDALTranslate with -expand RGBA
    auto convertToRGBAifNeeded =
        [](GDALDataset *&poDS,
           std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> &ds)
        -> bool
    {
        if (poDS->GetRasterCount() == 1 &&
            poDS->GetRasterBand(1)->GetColorTable() != nullptr)
        {
            CPLStringList aosOptions;
            aosOptions.AddString("-of");
            aosOptions.AddString("VRT");
            aosOptions.AddString("-expand");
            aosOptions.AddString("RGBA");
            GDALTranslateOptions *translateOptions =
                GDALTranslateOptionsNew(aosOptions.List(), nullptr);

            ds.reset(GDALDataset::FromHandle(GDALTranslate(
                "", GDALDataset::ToHandle(poDS), translateOptions, nullptr)));

            GDALTranslateOptionsFree(translateOptions);

            if (ds != nullptr)
            {
                poDS = ds.get();
                return true;
            }
            else
            {
                return false;
            }
        }
        return true;
    };

    if (!convertToRGBAifNeeded(poSrcDS, m_poTmpSrcDS))
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Conversion of source dataset color table to RGBA failed");
        return false;
    }

    if (!convertToRGBAifNeeded(poOverlayDS, m_poTmpOverlayDS))
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Conversion of overlay dataset color table to RGBA failed");
        return false;
    }

    if (!ValidateGlobal())
        return false;

    const int nOpacity255Scale =
        (m_opacity * 255 + OPACITY_INPUT_RANGE / 2) / OPACITY_INPUT_RANGE;

    bool bSwappedOpacity = false;
    // Many algorithms are commutative regarding the two inputs but BlendDataset assume
    // RGB(A) is in the source (and not in the overlay).
    if ((m_operator == CompositionMode::MULTIPLY ||
         m_operator == CompositionMode::SCREEN ||
         m_operator == CompositionMode::HARD_LIGHT ||
         m_operator == CompositionMode::OVERLAY) &&
        (poSrcDS->GetRasterCount() < poOverlayDS->GetRasterCount()))
    {
        bSwappedOpacity = true;
        std::swap(poSrcDS, poOverlayDS);
    }

    m_outputDataset.Set(std::make_unique<BlendDataset>(
        *poSrcDS, *poOverlayDS, m_operator, nOpacity255Scale, bSwappedOpacity));

    return true;
}

GDALRasterBlendAlgorithmStandalone::~GDALRasterBlendAlgorithmStandalone() =
    default;

//! @endcond
