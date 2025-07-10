/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "color-merge" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2009, Frank Warmerdam

 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_color_merge.h"

#include "cpl_conv.h"
#include "gdal_priv.h"

#include <algorithm>
#include <limits>

#if defined(__x86_64) || defined(_M_X64)
#define HAVE_SSE2
#endif
#ifdef HAVE_SSE2
#include "gdalsse_priv.h"
#endif

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*     GDALRasterColorMergeAlgorithm::GDALRasterColorMergeAlgorithm()   */
/************************************************************************/

GDALRasterColorMergeAlgorithm::GDALRasterColorMergeAlgorithm(
    bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetAddDefaultArguments(false)
              .SetInputDatasetHelpMsg(_("Input RGB/RGBA raster dataset"))
              .SetInputDatasetAlias("color-input")
              .SetInputDatasetMetaVar("COLOR-INPUT")
              .SetOutputDatasetHelpMsg(_("Output RGB/RGBA raster dataset")))
{
    const auto AddGrayscaleDataset = [this]()
    {
        auto &arg = AddArg("grayscale", 0, _("Grayscale dataset"),
                           &m_grayScaleDataset, GDAL_OF_RASTER)
                        .SetPositional()
                        .SetRequired();

        SetAutoCompleteFunctionForFilename(arg, GDAL_OF_RASTER);
    };

    if (standaloneStep)
    {
        AddRasterInputArgs(false, false);
        AddGrayscaleDataset();
        AddProgressArg();
        AddRasterOutputArgs(false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
        AddGrayscaleDataset();
    }
}

namespace
{

/************************************************************************/
/*                        HSVMergeDataset                               */
/************************************************************************/

class HSVMergeDataset final : public GDALDataset
{
  public:
    HSVMergeDataset(GDALDataset &oColorDS, GDALDataset &oGrayScaleDS);

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
    friend class HSVMergeBand;
    GDALDataset &m_oColorDS;
    GDALDataset &m_oGrayScaleDS;
    std::vector<std::unique_ptr<HSVMergeDataset>> m_apoOverviews{};
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
        const auto f0 = six_h0 - idx0.to_float();
        const auto p0 = (v0 * (one - s0) + half).truncate_to_int();
        const auto q0 = (v0 * (one - s0 * f0) + half).truncate_to_int();
        const auto t0 = (v0 * (one - s0 * (one - f0)) + half).truncate_to_int();

        const auto six_h1 = six * h1;
        const auto idx1 = six_h1.truncate_to_int();
        const auto f1 = six_h1 - idx1.to_float();
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
/*                           HSVMergeBand                               */
/************************************************************************/

class HSVMergeBand final : public GDALRasterBand
{
  public:
    HSVMergeBand(HSVMergeDataset &oHSVMergeDataset, int nBandIn)
        : m_oHSVMergeDataset(oHSVMergeDataset)
    {
        nBand = nBandIn;
        nRasterXSize = oHSVMergeDataset.GetRasterXSize();
        nRasterYSize = oHSVMergeDataset.GetRasterYSize();
        oHSVMergeDataset.m_oColorDS.GetRasterBand(1)->GetBlockSize(
            &nBlockXSize, &nBlockYSize);
        eDataType = GDT_Byte;
    }

    GDALColorInterp GetColorInterpretation() override
    {
        return m_oHSVMergeDataset.m_oColorDS.GetRasterBand(nBand)
            ->GetColorInterpretation();
    }

    int GetOverviewCount() override
    {
        return static_cast<int>(m_oHSVMergeDataset.m_apoOverviews.size());
    }

    GDALRasterBand *GetOverview(int idx) override
    {
        return idx >= 0 && idx < GetOverviewCount()
                   ? m_oHSVMergeDataset.m_apoOverviews[idx]->GetRasterBand(
                         nBand)
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
                        nReqXSize, nReqYSize, GDT_Byte, 1, nBlockXSize,
                        nullptr);
    }

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

  private:
    HSVMergeDataset &m_oHSVMergeDataset;
};

/************************************************************************/
/*                 HSVMergeDataset::HSVMergeDataset()                   */
/************************************************************************/

HSVMergeDataset::HSVMergeDataset(GDALDataset &oColorDS,
                                 GDALDataset &oGrayScaleDS)
    : m_oColorDS(oColorDS), m_oGrayScaleDS(oGrayScaleDS)
{
    CPLAssert(oColorDS.GetRasterCount() == 3 || oColorDS.GetRasterCount() == 4);
    CPLAssert(oColorDS.GetRasterXSize() == oGrayScaleDS.GetRasterXSize());
    CPLAssert(oColorDS.GetRasterYSize() == oGrayScaleDS.GetRasterYSize());
    nRasterXSize = oColorDS.GetRasterXSize();
    nRasterYSize = oColorDS.GetRasterYSize();
    const int nOvrCount = oGrayScaleDS.GetRasterBand(1)->GetOverviewCount();
    bool bCanCreateOvr = true;
    for (int iBand = 1; iBand <= oColorDS.GetRasterCount(); ++iBand)
    {
        SetBand(iBand, std::make_unique<HSVMergeBand>(*this, iBand));
        bCanCreateOvr =
            bCanCreateOvr &&
            oColorDS.GetRasterBand(iBand)->GetOverviewCount() == nOvrCount;
        for (int iOvr = 0; iOvr < nOvrCount && bCanCreateOvr; ++iOvr)
        {
            const auto poColorOvrBand =
                oColorDS.GetRasterBand(iBand)->GetOverview(iOvr);
            const auto poGSOvrBand =
                oGrayScaleDS.GetRasterBand(1)->GetOverview(iOvr);
            bCanCreateOvr =
                poColorOvrBand->GetDataset() != &oColorDS &&
                poColorOvrBand->GetDataset() == oColorDS.GetRasterBand(1)
                                                    ->GetOverview(iOvr)
                                                    ->GetDataset() &&
                poGSOvrBand->GetDataset() != &oGrayScaleDS &&
                poColorOvrBand->GetXSize() == poGSOvrBand->GetXSize() &&
                poColorOvrBand->GetYSize() == poGSOvrBand->GetYSize();
        }
    }

    SetDescription(CPLSPrintf("Merge %s with %s", m_oColorDS.GetDescription(),
                              m_oGrayScaleDS.GetDescription()));
    SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    if (bCanCreateOvr)
    {
        for (int iOvr = 0; iOvr < nOvrCount; ++iOvr)
        {
            m_apoOverviews.push_back(std::make_unique<HSVMergeDataset>(
                *(oColorDS.GetRasterBand(1)->GetOverview(iOvr)->GetDataset()),
                *(oGrayScaleDS.GetRasterBand(1)
                      ->GetOverview(iOvr)
                      ->GetDataset())));
        }
    }
}

/************************************************************************/
/*               HSVMergeDataset::AcquireSourcePixels()                 */
/************************************************************************/

bool HSVMergeDataset::AcquireSourcePixels(int nXOff, int nYOff, int nXSize,
                                          int nYSize, int nBufXSize,
                                          int nBufYSize,
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

    constexpr int N_COMPS_IN_BUFFER = 4;  //  RGB + Grayscale

    if (static_cast<size_t>(nBufXSize) >
        std::numeric_limits<size_t>::max() / nBufYSize / N_COMPS_IN_BUFFER)
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
        if (m_abyBuffer.size() < nPixelCount * N_COMPS_IN_BUFFER)
            m_abyBuffer.resize(nPixelCount * N_COMPS_IN_BUFFER);
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
        (m_oColorDS.RasterIO(
             GF_Read, nXOff, nYOff, nXSize, nYSize, m_abyBuffer.data(),
             nBufXSize, nBufYSize, GDT_Byte, 3, nullptr, 1, nBufXSize,
             static_cast<GSpacing>(nPixelCount), psExtraArg) == CE_None &&
         m_oGrayScaleDS.GetRasterBand(1)->RasterIO(
             GF_Read, nXOff, nYOff, nXSize, nYSize,
             m_abyBuffer.data() + nPixelCount * 3, nBufXSize, nBufYSize,
             GDT_Byte, 1, nBufXSize, psExtraArg) == CE_None);
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
/*                     HSVMergeDataset::IRasterIO()                     */
/************************************************************************/

CPLErr HSVMergeDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
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

    GByte *pabyDst = static_cast<GByte *>(pData);
    if (eRWFlag == GF_Read && eBufType == GDT_Byte && nBandCount == nBands &&
        panBandMap[0] == 1 && panBandMap[1] == 2 && panBandMap[2] == 3 &&
        (nBandCount == 3 || panBandMap[3] == 4) &&
        AcquireSourcePixels(nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize,
                            psExtraArg) &&
        (nBandCount == 3 ||
         m_oColorDS.GetRasterBand(4)->RasterIO(
             GF_Read, nXOff, nYOff, nXSize, nYSize, pabyDst + nBandSpace * 3,
             nBufXSize, nBufYSize, eBufType, nPixelSpace, nLineSpace,
             psExtraArg) == CE_None))
    {
        const size_t nPixelCount = static_cast<size_t>(nBufXSize) * nBufYSize;
        const GByte *pabyR = m_abyBuffer.data();
        const GByte *pabyG = m_abyBuffer.data() + nPixelCount;
        const GByte *pabyB = m_abyBuffer.data() + nPixelCount * 2;
        const GByte *pabyGrayScale = m_abyBuffer.data() + nPixelCount * 3;
        size_t nSrcIdx = 0;
        for (int j = 0; j < nBufYSize; ++j)
        {
            auto nDstOffset = j * nLineSpace;
            if (nPixelSpace == 1 && nLineSpace >= nPixelSpace * nBufXSize &&
                nBandSpace >= nLineSpace * nBufYSize)
            {
                patch_value_line(nBufXSize, pabyR + nSrcIdx, pabyG + nSrcIdx,
                                 pabyB + nSrcIdx, pabyGrayScale + nSrcIdx,
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
                    hsv_to_rgb(h, s, pabyGrayScale[nSrcIdx],
                               &pabyDst[nDstOffset + 0 * nBandSpace],
                               &pabyDst[nDstOffset + 1 * nBandSpace],
                               &pabyDst[nDstOffset + 2 * nBandSpace]);
                }
            }
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
/*                   HSVMergeDataset::IRasterIO()                       */
/************************************************************************/

CPLErr HSVMergeBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                               int nXSize, int nYSize, void *pData,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, GSpacing nPixelSpace,
                               GSpacing nLineSpace,
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

    if (nBand >= 4)
    {
        return m_oHSVMergeDataset.m_oColorDS.GetRasterBand(nBand)->RasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg);
    }
    else if (eRWFlag == GF_Read && eBufType == GDT_Byte &&
             m_oHSVMergeDataset.AcquireSourcePixels(nXOff, nYOff, nXSize,
                                                    nYSize, nBufXSize,
                                                    nBufYSize, psExtraArg))
    {
        GByte *pabyDst = static_cast<GByte *>(pData);
        const GByte *pabyR = m_oHSVMergeDataset.m_abyBuffer.data();
        const size_t nPixelCount = static_cast<size_t>(nBufXSize) * nBufYSize;
        const GByte *pabyG =
            m_oHSVMergeDataset.m_abyBuffer.data() + nPixelCount;
        const GByte *pabyB =
            m_oHSVMergeDataset.m_abyBuffer.data() + nPixelCount * 2;
        const GByte *pabyGrayScale =
            m_oHSVMergeDataset.m_abyBuffer.data() + nPixelCount * 3;
        size_t nSrcIdx = 0;
        for (int j = 0; j < nBufYSize; ++j)
        {
            auto nDstOffset = j * nLineSpace;
            if (nPixelSpace == 1 && nLineSpace >= nPixelSpace * nBufXSize)
            {
                patch_value_line(nBufXSize, pabyR + nSrcIdx, pabyG + nSrcIdx,
                                 pabyB + nSrcIdx, pabyGrayScale + nSrcIdx,
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
                    rgb_to_hs(pabyR[nSrcIdx], pabyG[nSrcIdx], pabyB[nSrcIdx],
                              &h, &s);
                    if (nBand == 1)
                    {
                        hsv_to_rgb(h, s, pabyGrayScale[nSrcIdx],
                                   &pabyDst[nDstOffset], nullptr, nullptr);
                    }
                    else if (nBand == 2)
                    {
                        hsv_to_rgb(h, s, pabyGrayScale[nSrcIdx], nullptr,
                                   &pabyDst[nDstOffset], nullptr);
                    }
                    else
                    {
                        CPLAssert(nBand == 3);
                        hsv_to_rgb(h, s, pabyGrayScale[nSrcIdx], nullptr,
                                   nullptr, &pabyDst[nDstOffset]);
                    }
                }
            }
        }

        return CE_None;
    }
    else if (m_oHSVMergeDataset.m_ioError)
    {
        return CE_Failure;
    }
    else
    {
        const CPLErr eErr = GDALRasterBand::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg);
        m_oHSVMergeDataset.m_ioError = eErr != CE_None;
        return eErr;
    }
}

}  // namespace

/************************************************************************/
/*                 GDALRasterColorMergeAlgorithm::RunStep()             */
/************************************************************************/

bool GDALRasterColorMergeAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poGrayScaleDS = m_grayScaleDataset.GetDatasetRef();
    CPLAssert(poGrayScaleDS);

    if ((poSrcDS->GetRasterCount() != 3 && poSrcDS->GetRasterCount() != 4) ||
        poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Only 3 or 4-band Byte dataset supported as input");
        return false;
    }

    if (poGrayScaleDS->GetRasterCount() != 1 ||
        poGrayScaleDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Only 1-band Byte dataset supported as grayscale dataset");
        return false;
    }

    if (poSrcDS->GetRasterXSize() != poGrayScaleDS->GetRasterXSize() ||
        poSrcDS->GetRasterYSize() != poGrayScaleDS->GetRasterYSize())
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "Input RGB/RGBA dataset and grayscale dataset must have "
                    "the same dimensions");
        return false;
    }

    m_outputDataset.Set(
        std::make_unique<HSVMergeDataset>(*poSrcDS, *poGrayScaleDS));

    return true;
}

GDALRasterColorMergeAlgorithmStandalone::
    ~GDALRasterColorMergeAlgorithmStandalone() = default;

//! @endcond
