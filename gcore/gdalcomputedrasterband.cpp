/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALComputedDataset and GDALComputedRasterBand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_priv.h"
#include "vrtdataset.h"

#include <cmath>
#include <limits>

/************************************************************************/
/*                         GDALComputedDataset                          */
/************************************************************************/

class GDALComputedDataset final : public GDALDataset
{
    friend class GDALComputedRasterBand;

    const GDALComputedRasterBand::Operation m_op;
    CPLStringList m_aosOptions{};
    std::vector<std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser>>
        m_bandDS{};
    std::vector<GDALRasterBand *> m_poBands{};
    VRTDataset m_oVRTDS;

    void AddSources(GDALComputedRasterBand *poBand);

    static const char *
    OperationToFunctionName(GDALComputedRasterBand::Operation op);

    GDALComputedDataset &operator=(const GDALComputedDataset &) = delete;
    GDALComputedDataset(GDALComputedDataset &&) = delete;
    GDALComputedDataset &operator=(GDALComputedDataset &&) = delete;

  public:
    GDALComputedDataset(const GDALComputedDataset &other);

    GDALComputedDataset(GDALComputedRasterBand *poBand, int nXSize, int nYSize,
                        GDALDataType eDT, int nBlockXSize, int nBlockYSize,
                        GDALComputedRasterBand::Operation op,
                        const GDALRasterBand *firstBand, double *pFirstConstant,
                        const GDALRasterBand *secondBand,
                        double *pSecondConstant);

    GDALComputedDataset(GDALComputedRasterBand *poBand, int nXSize, int nYSize,
                        GDALDataType eDT, int nBlockXSize, int nBlockYSize,
                        GDALComputedRasterBand::Operation op,
                        const std::vector<const GDALRasterBand *> &bands,
                        double constant);

    ~GDALComputedDataset() override;

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override
    {
        return m_oVRTDS.GetGeoTransform(gt);
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oVRTDS.GetSpatialRef();
    }

    CSLConstList GetMetadata(const char *pszDomain) override
    {
        return m_oVRTDS.GetMetadata(pszDomain);
    }

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override
    {
        return m_oVRTDS.GetMetadataItem(pszName, pszDomain);
    }

    void *GetInternalHandle(const char *pszHandleName) override
    {
        if (pszHandleName && EQUAL(pszHandleName, "VRT_DATASET"))
            return &m_oVRTDS;
        return nullptr;
    }
};

/************************************************************************/
/*                        IsComparisonOperator()                        */
/************************************************************************/

static bool IsComparisonOperator(GDALComputedRasterBand::Operation op)
{
    switch (op)
    {
        case GDALComputedRasterBand::Operation::OP_GT:
        case GDALComputedRasterBand::Operation::OP_GE:
        case GDALComputedRasterBand::Operation::OP_LT:
        case GDALComputedRasterBand::Operation::OP_LE:
        case GDALComputedRasterBand::Operation::OP_EQ:
        case GDALComputedRasterBand::Operation::OP_NE:
        case GDALComputedRasterBand::Operation::OP_LOGICAL_AND:
        case GDALComputedRasterBand::Operation::OP_LOGICAL_OR:
            return true;
        default:
            break;
    }
    return false;
}

/************************************************************************/
/*                        GDALComputedDataset()                         */
/************************************************************************/

GDALComputedDataset::GDALComputedDataset(const GDALComputedDataset &other)
    : GDALDataset(), m_op(other.m_op), m_aosOptions(other.m_aosOptions),
      m_poBands(other.m_poBands),
      m_oVRTDS(other.GetRasterXSize(), other.GetRasterYSize(),
               other.m_oVRTDS.GetBlockXSize(), other.m_oVRTDS.GetBlockYSize())
{
    nRasterXSize = other.nRasterXSize;
    nRasterYSize = other.nRasterYSize;

    auto poBand = new GDALComputedRasterBand(
        const_cast<const GDALComputedRasterBand &>(
            *cpl::down_cast<GDALComputedRasterBand *>(
                const_cast<GDALComputedDataset &>(other).GetRasterBand(1))),
        true);
    SetBand(1, poBand);

    GDALGeoTransform gt;
    if (const_cast<VRTDataset &>(other.m_oVRTDS).GetGeoTransform(gt) == CE_None)
    {
        m_oVRTDS.SetGeoTransform(gt);
    }

    if (const auto *poSRS =
            const_cast<VRTDataset &>(other.m_oVRTDS).GetSpatialRef())
    {
        m_oVRTDS.SetSpatialRef(poSRS);
    }

    m_oVRTDS.AddBand(other.m_oVRTDS.GetRasterBand(1)->GetRasterDataType(),
                     m_aosOptions.List());

    AddSources(poBand);
}

/************************************************************************/
/*                        GDALComputedDataset()                         */
/************************************************************************/

GDALComputedDataset::GDALComputedDataset(
    GDALComputedRasterBand *poBand, int nXSize, int nYSize, GDALDataType eDT,
    int nBlockXSize, int nBlockYSize, GDALComputedRasterBand::Operation op,
    const GDALRasterBand *firstBand, double *pFirstConstant,
    const GDALRasterBand *secondBand, double *pSecondConstant)
    : m_op(op), m_oVRTDS(nXSize, nYSize, nBlockXSize, nBlockYSize)
{
    CPLAssert(firstBand != nullptr || secondBand != nullptr);
    if (firstBand)
        m_poBands.push_back(const_cast<GDALRasterBand *>(firstBand));
    if (secondBand)
        m_poBands.push_back(const_cast<GDALRasterBand *>(secondBand));

    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if (auto poSrcDS = m_poBands.front()->GetDataset())
    {
        GDALGeoTransform gt;
        if (poSrcDS->GetGeoTransform(gt) == CE_None)
        {
            m_oVRTDS.SetGeoTransform(gt);
        }

        if (const auto *poSRS = poSrcDS->GetSpatialRef())
        {
            m_oVRTDS.SetSpatialRef(poSRS);
        }
    }

    if (op == GDALComputedRasterBand::Operation::OP_CAST)
    {
#ifdef DEBUG
        // Just for code coverage...
        CPL_IGNORE_RET_VAL(GDALComputedDataset::OperationToFunctionName(op));
#endif
        m_aosOptions.SetNameValue("subclass", "VRTSourcedRasterBand");
    }
    else
    {
        m_aosOptions.SetNameValue("subclass", "VRTDerivedRasterBand");
        if (IsComparisonOperator(op))
        {
            m_aosOptions.SetNameValue("PixelFunctionType", "expression");
            if (firstBand && secondBand)
            {
                m_aosOptions.SetNameValue(
                    "_PIXELFN_ARG_expression",
                    CPLSPrintf(
                        "source1 %s source2",
                        GDALComputedDataset::OperationToFunctionName(op)));
            }
            else if (firstBand && pSecondConstant)
            {
                m_aosOptions.SetNameValue(
                    "_PIXELFN_ARG_expression",
                    CPLSPrintf("source1 %s %.17g",
                               GDALComputedDataset::OperationToFunctionName(op),
                               *pSecondConstant));
            }
            else if (pFirstConstant && secondBand)
            {
                m_aosOptions.SetNameValue(
                    "_PIXELFN_ARG_expression",
                    CPLSPrintf(
                        "%.17g %s source1", *pFirstConstant,
                        GDALComputedDataset::OperationToFunctionName(op)));
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if (op == GDALComputedRasterBand::Operation::OP_SUBTRACT &&
                 pSecondConstant)
        {
            m_aosOptions.SetNameValue("PixelFunctionType", "sum");
            m_aosOptions.SetNameValue("_PIXELFN_ARG_k",
                                      CPLSPrintf("%.17g", -(*pSecondConstant)));
        }
        else if (op == GDALComputedRasterBand::Operation::OP_DIVIDE)
        {
            if (pSecondConstant)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "mul");
                m_aosOptions.SetNameValue(
                    "_PIXELFN_ARG_k",
                    CPLSPrintf("%.17g", 1.0 / (*pSecondConstant)));
            }
            else if (pFirstConstant)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "inv");
                m_aosOptions.SetNameValue("_PIXELFN_ARG_k",
                                          CPLSPrintf("%.17g", *pFirstConstant));
            }
            else
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "div");
            }
        }
        else if (op == GDALComputedRasterBand::Operation::OP_LOG)
        {
            CPLAssert(firstBand);
            CPLAssert(!secondBand);
            CPLAssert(!pFirstConstant);
            CPLAssert(!pSecondConstant);
            m_aosOptions.SetNameValue("PixelFunctionType", "expression");
            m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                      "log(source1)");
        }
        else if (op == GDALComputedRasterBand::Operation::OP_POW)
        {
            if (firstBand && secondBand)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "expression");
                m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                          "source1 ^ source2");
            }
            else if (firstBand && pSecondConstant)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "pow");
                m_aosOptions.SetNameValue(
                    "_PIXELFN_ARG_power",
                    CPLSPrintf("%.17g", *pSecondConstant));
            }
            else if (pFirstConstant && secondBand)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "exp");
                m_aosOptions.SetNameValue("_PIXELFN_ARG_base",
                                          CPLSPrintf("%.17g", *pFirstConstant));
            }
            else
            {
                CPLAssert(false);
            }
        }
        else
        {
            m_aosOptions.SetNameValue("PixelFunctionType",
                                      OperationToFunctionName(op));
            if (pSecondConstant)
                m_aosOptions.SetNameValue(
                    "_PIXELFN_ARG_k", CPLSPrintf("%.17g", *pSecondConstant));
        }
    }
    m_aosOptions.SetNameValue("_PIXELFN_ARG_propagateNoData", "true");
    m_oVRTDS.AddBand(eDT, m_aosOptions.List());

    SetBand(1, poBand);

    AddSources(poBand);
}

/************************************************************************/
/*                        GDALComputedDataset()                         */
/************************************************************************/

GDALComputedDataset::GDALComputedDataset(
    GDALComputedRasterBand *poBand, int nXSize, int nYSize, GDALDataType eDT,
    int nBlockXSize, int nBlockYSize, GDALComputedRasterBand::Operation op,
    const std::vector<const GDALRasterBand *> &bands, double constant)
    : m_op(op), m_oVRTDS(nXSize, nYSize, nBlockXSize, nBlockYSize)
{
    for (const GDALRasterBand *poIterBand : bands)
        m_poBands.push_back(const_cast<GDALRasterBand *>(poIterBand));

    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if (auto poSrcDS = m_poBands.front()->GetDataset())
    {
        GDALGeoTransform gt;
        if (poSrcDS->GetGeoTransform(gt) == CE_None)
        {
            m_oVRTDS.SetGeoTransform(gt);
        }

        if (const auto *poSRS = poSrcDS->GetSpatialRef())
        {
            m_oVRTDS.SetSpatialRef(poSRS);
        }
    }

    m_aosOptions.SetNameValue("subclass", "VRTDerivedRasterBand");
    if (op == GDALComputedRasterBand::Operation::OP_TERNARY)
    {
        m_aosOptions.SetNameValue("PixelFunctionType", "expression");
        m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                  "source1 ? source2 : source3");
    }
    else
    {
        m_aosOptions.SetNameValue("PixelFunctionType",
                                  OperationToFunctionName(op));
        if (!std::isnan(constant))
        {
            m_aosOptions.SetNameValue("_PIXELFN_ARG_k",
                                      CPLSPrintf("%.17g", constant));
        }
        m_aosOptions.SetNameValue("_PIXELFN_ARG_propagateNoData", "true");
    }
    m_oVRTDS.AddBand(eDT, m_aosOptions.List());

    SetBand(1, poBand);

    AddSources(poBand);
}

/************************************************************************/
/*                        ~GDALComputedDataset()                        */
/************************************************************************/

GDALComputedDataset::~GDALComputedDataset() = default;

/************************************************************************/
/*                    HaveAllBandsSameNoDataValue()                     */
/************************************************************************/

static bool HaveAllBandsSameNoDataValue(GDALRasterBand **apoBands,
                                        size_t nBands, bool &hasAtLeastOneNDV,
                                        double &singleNDV)
{
    hasAtLeastOneNDV = false;
    singleNDV = 0;

    int bFirstBandHasNoData = false;
    for (size_t i = 0; i < nBands; ++i)
    {
        int bHasNoData = false;
        const double dfNoData = apoBands[i]->GetNoDataValue(&bHasNoData);
        if (bHasNoData)
            hasAtLeastOneNDV = true;
        if (i == 0)
        {
            bFirstBandHasNoData = bHasNoData;
            singleNDV = dfNoData;
        }
        else if (bHasNoData != bFirstBandHasNoData)
        {
            return false;
        }
        else if (bFirstBandHasNoData &&
                 !((std::isnan(singleNDV) && std::isnan(dfNoData)) ||
                   (singleNDV == dfNoData)))
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                  GDALComputedDataset::AddSources()                   */
/************************************************************************/

void GDALComputedDataset::AddSources(GDALComputedRasterBand *poBand)
{
    auto poSourcedRasterBand =
        cpl::down_cast<VRTSourcedRasterBand *>(m_oVRTDS.GetRasterBand(1));

    bool hasAtLeastOneNDV = false;
    double singleNDV = 0;
    const bool bSameNDV = HaveAllBandsSameNoDataValue(
        m_poBands.data(), m_poBands.size(), hasAtLeastOneNDV, singleNDV);

    // For inputs that are instances of GDALComputedDataset, clone them
    // to make sure we do not depend on temporary instances,
    // such as "a + b + c", which is evaluated as "(a + b) + c", and the
    // temporary band/dataset corresponding to a + b will go out of scope
    // quickly.
    for (GDALRasterBand *&band : m_poBands)
    {
        auto poDS = band->GetDataset();
        if (auto poComputedDS = dynamic_cast<GDALComputedDataset *>(poDS))
        {
            auto poComputedDSNew =
                std::make_unique<GDALComputedDataset>(*poComputedDS);
            band = poComputedDSNew->GetRasterBand(1);
            m_bandDS.emplace_back(poComputedDSNew.release());
        }

        int bHasNoData = false;
        const double dfNoData = band->GetNoDataValue(&bHasNoData);
        if (bHasNoData)
        {
            poSourcedRasterBand->AddComplexSource(band, -1, -1, -1, -1, -1, -1,
                                                  -1, -1, 0, 1, dfNoData);
        }
        else
        {
            poSourcedRasterBand->AddSimpleSource(band);
        }
        poSourcedRasterBand->m_papoSources.back()->SetName(CPLSPrintf(
            "source%d",
            static_cast<int>(poSourcedRasterBand->m_papoSources.size())));
    }
    if (hasAtLeastOneNDV)
    {
        poBand->m_bHasNoData = true;
        if (bSameNDV)
        {
            poBand->m_dfNoDataValue = singleNDV;
        }
        else
        {
            poBand->m_dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
        }
        poSourcedRasterBand->SetNoDataValue(poBand->m_dfNoDataValue);
    }
}

/************************************************************************/
/*                      OperationToFunctionName()                       */
/************************************************************************/

/* static */ const char *GDALComputedDataset::OperationToFunctionName(
    GDALComputedRasterBand::Operation op)
{
    const char *ret = "";
    switch (op)
    {
        case GDALComputedRasterBand::Operation::OP_ADD:
            ret = "sum";
            break;
        case GDALComputedRasterBand::Operation::OP_SUBTRACT:
            ret = "diff";
            break;
        case GDALComputedRasterBand::Operation::OP_MULTIPLY:
            ret = "mul";
            break;
        case GDALComputedRasterBand::Operation::OP_DIVIDE:
            ret = "div";
            break;
        case GDALComputedRasterBand::Operation::OP_MIN:
            ret = "min";
            break;
        case GDALComputedRasterBand::Operation::OP_MAX:
            ret = "max";
            break;
        case GDALComputedRasterBand::Operation::OP_MEAN:
            ret = "mean";
            break;
        case GDALComputedRasterBand::Operation::OP_GT:
            ret = ">";
            break;
        case GDALComputedRasterBand::Operation::OP_GE:
            ret = ">=";
            break;
        case GDALComputedRasterBand::Operation::OP_LT:
            ret = "<";
            break;
        case GDALComputedRasterBand::Operation::OP_LE:
            ret = "<=";
            break;
        case GDALComputedRasterBand::Operation::OP_EQ:
            ret = "==";
            break;
        case GDALComputedRasterBand::Operation::OP_NE:
            ret = "!=";
            break;
        case GDALComputedRasterBand::Operation::OP_LOGICAL_AND:
            ret = "&&";
            break;
        case GDALComputedRasterBand::Operation::OP_LOGICAL_OR:
            ret = "||";
            break;
        case GDALComputedRasterBand::Operation::OP_CAST:
        case GDALComputedRasterBand::Operation::OP_TERNARY:
            break;
        case GDALComputedRasterBand::Operation::OP_ABS:
            ret = "mod";
            break;
        case GDALComputedRasterBand::Operation::OP_SQRT:
            ret = "sqrt";
            break;
        case GDALComputedRasterBand::Operation::OP_LOG:
            ret = "log";
            break;
        case GDALComputedRasterBand::Operation::OP_LOG10:
            ret = "log10";
            break;
        case GDALComputedRasterBand::Operation::OP_POW:
            ret = "pow";
            break;
    }
    return ret;
}

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(
    const GDALComputedRasterBand &other, bool)
    : GDALRasterBand()
{
    nRasterXSize = other.nRasterXSize;
    nRasterYSize = other.nRasterYSize;
    eDataType = other.eDataType;
    nBlockXSize = other.nBlockXSize;
    nBlockYSize = other.nBlockYSize;
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(
    Operation op, const std::vector<const GDALRasterBand *> &bands,
    double constant)
{
    CPLAssert(op == Operation::OP_ADD || op == Operation::OP_MIN ||
              op == Operation::OP_MAX || op == Operation::OP_MEAN ||
              op == Operation::OP_TERNARY);

    CPLAssert(!bands.empty());
    nRasterXSize = bands[0]->GetXSize();
    nRasterYSize = bands[0]->GetYSize();
    eDataType = bands[0]->GetRasterDataType();
    for (size_t i = 1; i < bands.size(); ++i)
    {
        eDataType = GDALDataTypeUnion(eDataType, bands[i]->GetRasterDataType());
    }

    bool hasAtLeastOneNDV = false;
    double singleNDV = 0;
    const bool bSameNDV =
        HaveAllBandsSameNoDataValue(const_cast<GDALRasterBand **>(bands.data()),
                                    bands.size(), hasAtLeastOneNDV, singleNDV);

    if (!bSameNDV)
    {
        eDataType = eDataType == GDT_Float64 ? GDT_Float64 : GDT_Float32;
    }
    else if (op == Operation::OP_TERNARY)
    {
        CPLAssert(bands.size() == 3);
        eDataType = GDALDataTypeUnion(bands[1]->GetRasterDataType(),
                                      bands[2]->GetRasterDataType());
    }
    else if (!std::isnan(constant) && eDataType != GDT_Float64)
    {
        if (op == Operation::OP_MIN || op == Operation::OP_MAX)
        {
            eDataType = GDALDataTypeUnionWithValue(eDataType, constant, false);
        }
        else
        {
            eDataType =
                (static_cast<double>(static_cast<float>(constant)) == constant)
                    ? GDT_Float32
                    : GDT_Float64;
        }
    }
    bands[0]->GetBlockSize(&nBlockXSize, &nBlockYSize);
    auto l_poDS = std::make_unique<GDALComputedDataset>(
        this, nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize,
        op, bands, constant);
    m_poOwningDS.reset(l_poDS.release());
}

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(Operation op,
                                               const GDALRasterBand &firstBand,
                                               const GDALRasterBand &secondBand)
{
    nRasterXSize = firstBand.GetXSize();
    nRasterYSize = firstBand.GetYSize();

    bool hasAtLeastOneNDV = false;
    double singleNDV = 0;
    GDALRasterBand *apoBands[] = {const_cast<GDALRasterBand *>(&firstBand),
                                  const_cast<GDALRasterBand *>(&secondBand)};
    const bool bSameNDV =
        HaveAllBandsSameNoDataValue(apoBands, 2, hasAtLeastOneNDV, singleNDV);

    const auto firstDT = firstBand.GetRasterDataType();
    const auto secondDT = secondBand.GetRasterDataType();
    if (!bSameNDV)
        eDataType = (firstDT == GDT_Float64 || secondDT == GDT_Float64)
                        ? GDT_Float64
                        : GDT_Float32;
    else if (IsComparisonOperator(op))
        eDataType = GDT_UInt8;
    else if (op == Operation::OP_ADD && firstDT == GDT_UInt8 &&
             secondDT == GDT_UInt8)
        eDataType = GDT_UInt16;
    else if (firstDT == GDT_Float32 && secondDT == GDT_Float32)
        eDataType = GDT_Float32;
    else if ((op == Operation::OP_MIN || op == Operation::OP_MAX) &&
             firstDT == secondDT)
        eDataType = firstDT;
    else
        eDataType = GDT_Float64;
    firstBand.GetBlockSize(&nBlockXSize, &nBlockYSize);
    auto l_poDS = std::make_unique<GDALComputedDataset>(
        this, nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize,
        op, &firstBand, nullptr, &secondBand, nullptr);
    m_poOwningDS.reset(l_poDS.release());
}

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(Operation op, double constant,
                                               const GDALRasterBand &band)
{
    CPLAssert(op == Operation::OP_DIVIDE || IsComparisonOperator(op) ||
              op == Operation::OP_POW);

    nRasterXSize = band.GetXSize();
    nRasterYSize = band.GetYSize();
    const auto firstDT = band.GetRasterDataType();
    if (IsComparisonOperator(op))
        eDataType = GDT_UInt8;
    else if (firstDT == GDT_Float32 &&
             static_cast<double>(static_cast<float>(constant)) == constant)
        eDataType = GDT_Float32;
    else
        eDataType = GDT_Float64;
    band.GetBlockSize(&nBlockXSize, &nBlockYSize);
    auto l_poDS = std::make_unique<GDALComputedDataset>(
        this, nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize,
        op, nullptr, &constant, &band, nullptr);
    m_poOwningDS.reset(l_poDS.release());
}

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(Operation op,
                                               const GDALRasterBand &band,
                                               double constant)
{
    nRasterXSize = band.GetXSize();
    nRasterYSize = band.GetYSize();
    const auto firstDT = band.GetRasterDataType();
    if (IsComparisonOperator(op))
        eDataType = GDT_UInt8;
    else if (op == Operation::OP_ADD && firstDT == GDT_UInt8 &&
             constant >= -128 && constant <= 127 &&
             std::floor(constant) == constant)
        eDataType = GDT_UInt8;
    else if (firstDT == GDT_Float32 &&
             static_cast<double>(static_cast<float>(constant)) == constant)
        eDataType = GDT_Float32;
    else
        eDataType = GDT_Float64;
    band.GetBlockSize(&nBlockXSize, &nBlockYSize);
    auto l_poDS = std::make_unique<GDALComputedDataset>(
        this, nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize,
        op, &band, nullptr, nullptr, &constant);
    m_poOwningDS.reset(l_poDS.release());
}

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(Operation op,
                                               const GDALRasterBand &band)
{
    CPLAssert(op == Operation::OP_ABS || op == Operation::OP_SQRT ||
              op == Operation::OP_LOG || op == Operation::OP_LOG10);
    nRasterXSize = band.GetXSize();
    nRasterYSize = band.GetYSize();
    eDataType =
        band.GetRasterDataType() == GDT_Float32 ? GDT_Float32 : GDT_Float64;
    band.GetBlockSize(&nBlockXSize, &nBlockYSize);
    auto l_poDS = std::make_unique<GDALComputedDataset>(
        this, nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize,
        op, &band, nullptr, nullptr, nullptr);
    m_poOwningDS.reset(l_poDS.release());
}

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(Operation op,
                                               const GDALRasterBand &band,
                                               GDALDataType dt)
{
    CPLAssert(op == Operation::OP_CAST);
    nRasterXSize = band.GetXSize();
    nRasterYSize = band.GetYSize();
    eDataType = dt;
    band.GetBlockSize(&nBlockXSize, &nBlockYSize);
    auto l_poDS = std::make_unique<GDALComputedDataset>(
        this, nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize,
        op, &band, nullptr, nullptr, nullptr);
    m_poOwningDS.reset(l_poDS.release());
}

//! @endcond

/************************************************************************/
/*                      ~GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::~GDALComputedRasterBand()
{
    if (m_poOwningDS)
        cpl::down_cast<GDALComputedDataset *>(m_poOwningDS.get())->nBands = 0;
    poDS = nullptr;
}

/************************************************************************/
/*               GDALComputedRasterBand::GetNoDataValue()               */
/************************************************************************/

double GDALComputedRasterBand::GetNoDataValue(int *pbHasNoData)
{
    if (pbHasNoData)
        *pbHasNoData = m_bHasNoData;
    return m_dfNoDataValue;
}

/************************************************************************/
/*                   GDALComputedRasterBandRelease()                    */
/************************************************************************/

/** Release a GDALComputedRasterBandH
 *
 * @since 3.12
 */
void GDALComputedRasterBandRelease(GDALComputedRasterBandH hBand)
{
    delete GDALComputedRasterBand::FromHandle(hBand);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALComputedRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                          void *pData)
{
    auto l_poDS = cpl::down_cast<GDALComputedDataset *>(poDS);
    return l_poDS->m_oVRTDS.GetRasterBand(1)->ReadBlock(nBlockXOff, nBlockYOff,
                                                        pData);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALComputedRasterBand::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)
{
    auto l_poDS = cpl::down_cast<GDALComputedDataset *>(poDS);
    return l_poDS->m_oVRTDS.GetRasterBand(1)->RasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nPixelSpace, nLineSpace, psExtraArg);
}
