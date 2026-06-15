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
#include <set>
#include <utility>

/************************************************************************/
/*                         GDALComputedDataset                          */
/************************************************************************/

class GDALComputedDataset final : public GDALDataset
{
    friend class GDALComputedRasterBand;

    std::string m_expr{};
    CPLStringList m_aosOptions{};
    std::vector<std::pair<std::string, GDALRasterBand *>> m_apoBands{};
    VRTDataset m_oVRTDS;

    std::pair<bool, std::string> AddSourceBand(const GDALRasterBand *band);

    void AddSources(GDALComputedRasterBand *poBand);

    static const char *
    OperationToFunctionName(GDALComputedRasterBand::Operation op,
                            bool bForceMuparser = false);

    GDALComputedDataset &operator=(const GDALComputedDataset &) = delete;
    GDALComputedDataset(GDALComputedDataset &&) = delete;
    GDALComputedDataset &operator=(GDALComputedDataset &&) = delete;

  public:
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
/*                           AddSourceBand()                            */
/************************************************************************/

/** Returns a pair (bIsExprBand, osBandName) */
std::pair<bool, std::string>
GDALComputedDataset::AddSourceBand(const GDALRasterBand *band)
{
    auto bandDS = band->GetDataset();
    if (auto poComputedDS = dynamic_cast<GDALComputedDataset *>(bandDS))
    {
        m_apoBands.insert(m_apoBands.end(), poComputedDS->m_apoBands.begin(),
                          poComputedDS->m_apoBands.end());
        return {true, poComputedDS->m_expr};
    }
    else
    {
        std::string osName = CPLSPrintf("band_%p", band);
        m_apoBands.emplace_back(osName, const_cast<GDALRasterBand *>(band));
        return {false, osName};
    }
}

/************************************************************************/
/*                        GDALComputedDataset()                         */
/************************************************************************/

GDALComputedDataset::GDALComputedDataset(
    GDALComputedRasterBand *poBand, int nXSize, int nYSize, GDALDataType eDT,
    int nBlockXSize, int nBlockYSize, GDALComputedRasterBand::Operation op,
    const GDALRasterBand *firstBand, double *pFirstConstant,
    const GDALRasterBand *secondBand, double *pSecondConstant)
    : m_oVRTDS(nXSize, nYSize, nBlockXSize, nBlockYSize)
{
    CPLAssert(firstBand != nullptr || secondBand != nullptr);
    std::string osFirstBand;
    bool bCanUseBuiltin = true;
    if (firstBand)
    {
        const auto [bIsExprBand, name] = AddSourceBand(firstBand);
        bCanUseBuiltin = bCanUseBuiltin && !bIsExprBand;
        if (bIsExprBand)
            osFirstBand = "(";
        osFirstBand += name;
        if (bIsExprBand)
            osFirstBand += ')';
    }
    std::string osSecondBand;
    if (secondBand)
    {
        const auto [bIsExprBand, name] = AddSourceBand(secondBand);
        bCanUseBuiltin = bCanUseBuiltin && !bIsExprBand;
        if (bIsExprBand)
            osSecondBand = "(";
        osSecondBand += name;
        if (bIsExprBand)
            osSecondBand += ')';
    }

    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if (auto poSrcDS = m_apoBands.front().second->GetDataset())
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

        m_expr = osFirstBand;
        if (m_apoBands.size() > 1)
        {
            m_aosOptions.SetNameValue("subclass", "VRTDerivedRasterBand");
            m_aosOptions.SetNameValue("PixelFunctionType", "expression");
            m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                      m_expr.c_str());
        }
        else
        {
            m_aosOptions.SetNameValue("subclass", "VRTSourcedRasterBand");
        }
    }
    else
    {
        m_aosOptions.SetNameValue("subclass", "VRTDerivedRasterBand");
        if (IsComparisonOperator(op))
        {
            m_aosOptions.SetNameValue("PixelFunctionType", "expression");
            if (firstBand && secondBand)
            {
                m_expr = osFirstBand;
                m_expr += ' ';
                m_expr += GDALComputedDataset::OperationToFunctionName(op);
                m_expr += ' ';
                m_expr += osSecondBand;
            }
            else if (firstBand && pSecondConstant)
            {
                m_expr = osFirstBand;
                m_expr += ' ';
                m_expr += GDALComputedDataset::OperationToFunctionName(op);
                m_expr += ' ';
                m_expr += CPLSPrintf("%.17g", *pSecondConstant);
            }
            else if (pFirstConstant && secondBand)
            {
                m_expr = CPLSPrintf("%.17g", *pFirstConstant);
                m_expr += ' ';
                m_expr += GDALComputedDataset::OperationToFunctionName(op);
                m_expr += ' ';
                m_expr += osSecondBand;
            }
            else
            {
                CPLAssert(false);
            }
            m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                      m_expr.c_str());
        }
        else if (op == GDALComputedRasterBand::Operation::OP_SUBTRACT &&
                 pSecondConstant)
        {
            m_aosOptions.SetNameValue("PixelFunctionType", "sum");
            m_aosOptions.SetNameValue("_PIXELFN_ARG_k",
                                      CPLSPrintf("%.17g", -(*pSecondConstant)));
            m_expr = osFirstBand;
            m_expr += CPLSPrintf(" - %.17g", *pSecondConstant);
        }
        else if (op == GDALComputedRasterBand::Operation::OP_DIVIDE)
        {
            if (pSecondConstant)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "mul");
                m_aosOptions.SetNameValue(
                    "_PIXELFN_ARG_k",
                    CPLSPrintf("%.17g", 1.0 / (*pSecondConstant)));
                m_expr = osFirstBand;
                m_expr += CPLSPrintf(" * %.17g", 1.0 / (*pSecondConstant));
            }
            else if (pFirstConstant)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "inv");
                m_aosOptions.SetNameValue("_PIXELFN_ARG_k",
                                          CPLSPrintf("%.17g", *pFirstConstant));
                m_expr = CPLSPrintf("%.17g / ", *pFirstConstant);
                m_expr += osSecondBand;
            }
            else
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "div");
                m_expr = osFirstBand;
                m_expr += " / ";
                m_expr += osSecondBand;
            }
        }
        else if (op == GDALComputedRasterBand::Operation::OP_LOG)
        {
            CPLAssert(firstBand);
            CPLAssert(!secondBand);
            CPLAssert(!pFirstConstant);
            CPLAssert(!pSecondConstant);
            m_aosOptions.SetNameValue("PixelFunctionType", "expression");
            m_expr = "log(";
            m_expr += osFirstBand;
            m_expr += ')';
            m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                      m_expr.c_str());
        }
        else if (op == GDALComputedRasterBand::Operation::OP_POW)
        {
            if (firstBand && secondBand)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "expression");
                m_expr = osFirstBand;
                m_expr += " ^ ";
                m_expr += osSecondBand;
                m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                          m_expr.c_str());
            }
            else if (firstBand && pSecondConstant)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "pow");
                m_aosOptions.SetNameValue(
                    "_PIXELFN_ARG_power",
                    CPLSPrintf("%.17g", *pSecondConstant));
                m_expr = osFirstBand;
                m_expr += " ^ ";
                m_expr += CPLSPrintf("%.17g", *pSecondConstant);
            }
            else if (pFirstConstant && secondBand)
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "exp");
                m_aosOptions.SetNameValue("_PIXELFN_ARG_base",
                                          CPLSPrintf("%.17g", *pFirstConstant));
                m_expr = CPLSPrintf("%.17g", *pFirstConstant);
                m_expr += " ^ ";
                m_expr += osSecondBand;
            }
            else
            {
                CPLAssert(false);
            }
        }
        else
        {
            if (firstBand && secondBand)
            {
                if (op == GDALComputedRasterBand::Operation::OP_MIN ||
                    op == GDALComputedRasterBand::Operation::OP_MAX ||
                    op == GDALComputedRasterBand::Operation::OP_MEAN)
                {
                    m_expr +=
                        GDALComputedDataset::OperationToFunctionName(op, true);
                    m_expr += '(';
                    m_expr += osFirstBand;
                    m_expr += ',';
                    m_expr += osSecondBand;
                    m_expr += ')';
                }
                else
                {
                    m_expr = osFirstBand;
                    m_expr += ' ';
                    m_expr +=
                        GDALComputedDataset::OperationToFunctionName(op, true);
                    m_expr += ' ';
                    m_expr += osSecondBand;
                }
            }
            else if (firstBand && pSecondConstant)
            {
                m_expr = osFirstBand;
                m_expr += ' ';
                m_expr +=
                    GDALComputedDataset::OperationToFunctionName(op, true);
                m_expr += ' ';
                m_expr += CPLSPrintf("%.17g", *pSecondConstant);
            }
            else if (pFirstConstant && secondBand)
            {
                m_expr = CPLSPrintf("%.17g", *pFirstConstant);
                m_expr += ' ';
                m_expr +=
                    GDALComputedDataset::OperationToFunctionName(op, true);
                m_expr += ' ';
                m_expr += osSecondBand;
            }
            else
            {
                m_expr = GDALComputedDataset::OperationToFunctionName(op, true);
                m_expr += '(';
                m_expr += osFirstBand;
                m_expr += ')';
            }

            if (bCanUseBuiltin)
            {
                m_aosOptions.SetNameValue("PixelFunctionType",
                                          OperationToFunctionName(op));
                if (pSecondConstant)
                {
                    m_aosOptions.SetNameValue(
                        "_PIXELFN_ARG_k",
                        CPLSPrintf("%.17g", *pSecondConstant));
                }
            }
            else
            {
                m_aosOptions.SetNameValue("PixelFunctionType", "expression");
                m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                          m_expr.c_str());
            }
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
    : m_oVRTDS(nXSize, nYSize, nBlockXSize, nBlockYSize)
{
    bool bCanUseBuiltin = true;
    std::vector<std::string> aosNames;
    for (const GDALRasterBand *poIterBand : bands)
    {
        const auto [bIsExprBand, name] = AddSourceBand(poIterBand);
        bCanUseBuiltin = bCanUseBuiltin && !bIsExprBand;
        if (!bIsExprBand)
            aosNames.push_back(name);
        else
            aosNames.push_back(std::string("(").append(name).append(")"));
    }

    nRasterXSize = nXSize;
    nRasterYSize = nYSize;

    if (auto poSrcDS = m_apoBands.front().second->GetDataset())
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
        m_expr = aosNames[0];
        m_expr += " ? ";
        m_expr += aosNames[1];
        m_expr += " : ";
        m_expr += aosNames[2];

        m_aosOptions.SetNameValue("PixelFunctionType", "expression");
        m_aosOptions.SetNameValue("_PIXELFN_ARG_expression", m_expr.c_str());
    }
    else
    {
        m_expr = OperationToFunctionName(op);
        m_expr += '(';
        bool first = true;
        for (const auto &name : aosNames)
        {
            if (!first)
                m_expr += ", ";
            m_expr += name;
            first = false;
        }
        if (!std::isnan(constant))
        {
            if (!first)
                m_expr += ", ";
            m_expr += CPLSPrintf("%.17g", constant);
        }
        m_expr += ')';

        if (bCanUseBuiltin)
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
        else
        {
            m_aosOptions.SetNameValue("PixelFunctionType", "expression");
            m_aosOptions.SetNameValue("_PIXELFN_ARG_expression",
                                      m_expr.c_str());
        }
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
    std::vector<GDALRasterBand *> apoSrcBands;
    for (auto &[_, band] : m_apoBands)
    {
        apoSrcBands.push_back(band);
    }
    const bool bSameNDV = HaveAllBandsSameNoDataValue(
        apoSrcBands.data(), m_apoBands.size(), hasAtLeastOneNDV, singleNDV);

    std::set<std::string> alreadyAdded;
    for (auto &[name, band] : m_apoBands)
    {
        if (alreadyAdded.insert(name).second)
        {
            int bHasNoData = false;
            const double dfNoData = band->GetNoDataValue(&bHasNoData);
            if (bHasNoData)
            {
                poSourcedRasterBand->AddComplexSource(
                    band, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, dfNoData);
            }
            else
            {
                poSourcedRasterBand->AddSimpleSource(band);
            }
            poSourcedRasterBand->m_papoSources.back()->SetName(name.c_str());
        }
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
    GDALComputedRasterBand::Operation op, bool bForceMuparser)
{
    const char *ret = "";
    switch (op)
    {
        case GDALComputedRasterBand::Operation::OP_ADD:
            ret = bForceMuparser ? "+" : "sum";
            break;
        case GDALComputedRasterBand::Operation::OP_SUBTRACT:
            ret = bForceMuparser ? "-" : "diff";
            break;
        case GDALComputedRasterBand::Operation::OP_MULTIPLY:
            ret = bForceMuparser ? "*" : "mul";
            break;
        case GDALComputedRasterBand::Operation::OP_DIVIDE:
            ret = bForceMuparser ? "/" : "div";
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
            ret = bForceMuparser ? "abs" : "mod";
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
