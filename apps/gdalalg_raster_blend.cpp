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

constexpr const char *SRC_OVER = "src-over";
constexpr const char *HSV_VALUE = "hsv-value";

/************************************************************************/
/*       GDALRasterBlendAlgorithm::GDALRasterBlendAlgorithm()           */
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

    AddArg("operator", 0, _("Composition operator"), &m_operator)
        .SetChoices(SRC_OVER, HSV_VALUE)
        .SetDefault(SRC_OVER);
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
/*                            BlendDataset                              */
/************************************************************************/

class BlendDataset final : public GDALDataset
{
  public:
    BlendDataset(GDALDataset &oColorDS, GDALDataset &oOverlayDS,
                 const std::string &sOperator, int nOpacity255Scale);
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
    const std::string m_operator;
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
};

/************************************************************************/
/*                           rgb_to_hs()                                */
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
/*                           choose_among()                             */
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
/*                           hsv_to_rgb()                               */
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
/*                         patch_value_line()                           */
/************************************************************************/

static
#ifdef __GNUC__
    __attribute__((__noinline__))
#endif
    void
    patch_value_line(int nCount, const GByte *CPL_RESTRICT pInR,
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
/*                          BlendBand                                   */
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
/*                       BlendDataset::BlendDataset()                   */
/************************************************************************/

BlendDataset::BlendDataset(GDALDataset &oColorDS, GDALDataset &oOverlayDS,
                           const std::string &sOperator, int nOpacity255Scale)
    : m_oColorDS(oColorDS), m_oOverlayDS(oOverlayDS), m_operator(sOperator),
      m_opacity255Scale(nOpacity255Scale)
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
                m_operator, nOpacity255Scale));
        }
    }
}

/************************************************************************/
/*                     ~BlendDataset::BlendDataset()                    */
/************************************************************************/

BlendDataset::~BlendDataset()
{
    m_oColorDS.ReleaseRef();
    m_oOverlayDS.ReleaseRef();
}

/************************************************************************/
/*                  BlendDataset::AcquireSourcePixels()                 */
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
/*                         BlendSrcOverRGBA_SSE2()                      */
/************************************************************************/

#ifdef HAVE_SSE2
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static int
BlendSrcOverRGBA_SSE2(const GByte *CPL_RESTRICT pabyR,
                      const GByte *CPL_RESTRICT pabyG,
                      const GByte *CPL_RESTRICT pabyB,
                      const GByte *CPL_RESTRICT pabyA,
                      const GByte *CPL_RESTRICT pabyOverlayR,
                      const GByte *CPL_RESTRICT pabyOverlayG,
                      const GByte *CPL_RESTRICT pabyOverlayB,
                      const GByte *CPL_RESTRICT pabyOverlayA,
                      GByte *CPL_RESTRICT pabyDst, GSpacing nBandSpace, int N,
                      GByte nOpacity)
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

    const auto pack_and_store = [](void *p, __m128i lo, __m128i hi) {
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
        pack_and_store(pabyDst + i + (iBand)*nBandSpace, dst_lo, dst_hi);      \
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
static void
BlendSrcOverRGBA_Generic(const GByte *CPL_RESTRICT pabyR,
                         const GByte *CPL_RESTRICT pabyG,
                         const GByte *CPL_RESTRICT pabyB,
                         const GByte *CPL_RESTRICT pabyA,
                         const GByte *CPL_RESTRICT pabyOverlayR,
                         const GByte *CPL_RESTRICT pabyOverlayG,
                         const GByte *CPL_RESTRICT pabyOverlayB,
                         const GByte *CPL_RESTRICT pabyOverlayA,
                         GByte *CPL_RESTRICT pabyDst,
                         [[maybe_unused]] GSpacing nPixelSpace,
                         GSpacing nBandSpace, int i, int N, GByte nOpacity)
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
/*                       BlendDataset::IRasterIO()                      */
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
    if (nOverlayCount == 1 && m_opacity255Scale == 255 &&
        m_operator == HSV_VALUE && eRWFlag == GF_Read &&
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
    else if (nOverlayCount == 4 && nColorCount == 4 && m_operator == SRC_OVER &&
             eRWFlag == GF_Read && eBufType == GDT_UInt8 &&
             nBandCount == nBands && IsAllBands(nBands, panBandMap) &&
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
/*                        SrcOverRGBOneComponent()                      */
/************************************************************************/

// GCC and clang do a god job a auto vectorizing the below function
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("tree-vectorize")))
#endif
static void
SrcOverRGB(const uint8_t *const __restrict pabyOverlay,
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
    if (nBand == 4 && m_oBlendDataset.m_operator == HSV_VALUE)
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
             m_oBlendDataset.m_operator == SRC_OVER && eRWFlag == GF_Read &&
             eBufType == GDT_UInt8 &&
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
        if (m_oBlendDataset.m_operator == SRC_OVER)
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
            CPLAssert(m_oBlendDataset.m_operator == HSV_VALUE);
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
            CPLAssert(m_oBlendDataset.m_operator == HSV_VALUE);
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
/*                GDALRasterBlendAlgorithm::ValidateGlobal()            */
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

        if (m_operator == HSV_VALUE && poSrcDS->GetRasterCount() != 3 &&
            poSrcDS->GetRasterCount() != 4)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Operator %s requires a 3-band or 4-band input dataset",
                        HSV_VALUE);
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                   GDALRasterBlendAlgorithm::RunStep()                */
/************************************************************************/

bool GDALRasterBlendAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poOverlayDS = m_overlayDataset.GetDatasetRef();
    CPLAssert(poOverlayDS);

    if (!ValidateGlobal())
        return false;

    const int nOpacity255Scale =
        (m_opacity * 255 + OPACITY_INPUT_RANGE / 2) / OPACITY_INPUT_RANGE;

    m_outputDataset.Set(std::make_unique<BlendDataset>(
        *poSrcDS, *poOverlayDS, m_operator, nOpacity255Scale));

    return true;
}

GDALRasterBlendAlgorithmStandalone::~GDALRasterBlendAlgorithmStandalone() =
    default;

//! @endcond
