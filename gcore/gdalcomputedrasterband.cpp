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

/************************************************************************/
/*                        GDALComputedDataset                           */
/************************************************************************/

class GDALComputedDataset final : public GDALDataset
{
    friend class GDALComputedRasterBand;

    const GDALComputedRasterBand::Operation m_op;
    CPLStringList m_aosOptions{};
    std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> m_firstBandDS{};
    std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> m_secondBandDS{};
    GDALRasterBand *m_poFirstBand = nullptr;
    GDALRasterBand *m_poSecondBand = nullptr;
    VRTDataset m_oVRTDS;

    void AddSources();

    static const char *
    OperationToFunctionName(GDALComputedRasterBand::Operation op);

    GDALComputedDataset &operator=(const GDALComputedDataset &) = delete;
    GDALComputedDataset(GDALComputedDataset &&) = delete;
    GDALComputedDataset &operator=(GDALComputedDataset &&) = delete;

  public:
    GDALComputedDataset(const GDALComputedDataset &other);

    GDALComputedDataset(int nXSize, int nYSize, GDALDataType eDT,
                        int nBlockXSize, int nBlockYSize,
                        GDALComputedRasterBand::Operation op,
                        const GDALRasterBand &firstBand,
                        const GDALRasterBand *secondBand, double *pConstant);

    ~GDALComputedDataset() override;

    CPLErr GetGeoTransform(double *padfGeoTransform) override
    {
        auto poRefDS = m_poFirstBand->GetDataset();
        return poRefDS ? poRefDS->GetGeoTransform(padfGeoTransform)
                       : CE_Failure;
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        auto poRefDS = m_poFirstBand->GetDataset();
        return poRefDS ? poRefDS->GetSpatialRef() : nullptr;
    }
};

/************************************************************************/
/*                        GDALComputedDataset()                         */
/************************************************************************/

GDALComputedDataset::GDALComputedDataset(const GDALComputedDataset &other)
    : GDALDataset(), m_op(other.m_op), m_aosOptions(other.m_aosOptions),
      m_firstBandDS(nullptr), m_secondBandDS(nullptr),
      m_poFirstBand(other.m_poFirstBand), m_poSecondBand(other.m_poSecondBand),
      m_oVRTDS(other.GetRasterXSize(), other.GetRasterYSize(),
               other.m_oVRTDS.GetBlockXSize(), other.m_oVRTDS.GetBlockYSize())
{
    nRasterXSize = other.nRasterXSize;
    nRasterYSize = other.nRasterYSize;

    SetBand(
        1,
        new GDALComputedRasterBand(
            const_cast<const GDALComputedRasterBand &>(
                *cpl::down_cast<GDALComputedRasterBand *>(
                    const_cast<GDALComputedDataset &>(other).GetRasterBand(1))),
            true));

    m_oVRTDS.AddBand(other.m_oVRTDS.GetRasterBand(1)->GetRasterDataType(),
                     m_aosOptions.List());

    AddSources();
}

/************************************************************************/
/*                        GDALComputedDataset()                         */
/************************************************************************/

GDALComputedDataset::GDALComputedDataset(
    int nXSize, int nYSize, GDALDataType eDT, int nBlockXSize, int nBlockYSize,
    GDALComputedRasterBand::Operation op, const GDALRasterBand &firstBand,
    const GDALRasterBand *secondBand, double *pConstant)
    : m_op(op), m_poFirstBand(const_cast<GDALRasterBand *>(&firstBand)),
      m_poSecondBand(const_cast<GDALRasterBand *>(secondBand)),
      m_oVRTDS(nXSize, nYSize, nBlockXSize, nBlockYSize)
{
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
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
        if (op == GDALComputedRasterBand::Operation::OP_SUBTRACT && pConstant)
        {
            m_aosOptions.SetNameValue("PixelFunctionType", "sum");
            m_aosOptions.SetNameValue("_PIXELFN_ARG_k",
                                      CPLSPrintf("%.17g", -(*pConstant)));
        }
        else if (op == GDALComputedRasterBand::Operation::OP_DIVIDE &&
                 pConstant)
        {
            m_aosOptions.SetNameValue("PixelFunctionType", "mul");
            m_aosOptions.SetNameValue("_PIXELFN_ARG_k",
                                      CPLSPrintf("%.17g", 1.0 / (*pConstant)));
        }
        else
        {
            m_aosOptions.SetNameValue("PixelFunctionType",
                                      OperationToFunctionName(op));
            if (pConstant)
                m_aosOptions.SetNameValue("_PIXELFN_ARG_k",
                                          CPLSPrintf("%.17g", *pConstant));
        }
    }
    m_oVRTDS.AddBand(eDT, m_aosOptions.List());

    AddSources();
}

/************************************************************************/
/*                       ~GDALComputedDataset()                         */
/************************************************************************/

GDALComputedDataset::~GDALComputedDataset() = default;

/************************************************************************/
/*                  GDALComputedDataset::AddSources()                   */
/************************************************************************/

void GDALComputedDataset::AddSources()
{
    auto poSourcedRasterBand =
        cpl::down_cast<VRTSourcedRasterBand *>(m_oVRTDS.GetRasterBand(1));

    // For inputs that are instances of GDALComputedDataset, clone them
    // to make sure we do not depend on temporary instances,
    // such as "a + b + c", which is evaluated as "(a + b) + c", and the
    // temporary band/dataset corresponding to a + b will go out of scope
    // quickly.
    if (auto poComputedDS =
            dynamic_cast<GDALComputedDataset *>(m_poFirstBand->GetDataset()))
    {
        auto poComputedDSNew =
            std::make_unique<GDALComputedDataset>(*poComputedDS);
        m_poFirstBand = poComputedDSNew->GetRasterBand(1);
        m_firstBandDS.reset(poComputedDSNew.release());
    }
    poSourcedRasterBand->AddSimpleSource(m_poFirstBand);

    if (m_poSecondBand)
    {
        auto poDS = m_poSecondBand->GetDataset();
        if (auto poComputedDS = dynamic_cast<GDALComputedDataset *>(poDS))
        {
            auto poComputedDSNew =
                std::make_unique<GDALComputedDataset>(*poComputedDS);
            m_poSecondBand = poComputedDSNew->GetRasterBand(1);
            m_secondBandDS.reset(poComputedDSNew.release());
        }
        poSourcedRasterBand->AddSimpleSource(m_poSecondBand);
    }
}

/************************************************************************/
/*                       OperationToFunctionName()                      */
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
        case GDALComputedRasterBand::Operation::OP_CAST:
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

GDALComputedRasterBand::GDALComputedRasterBand(Operation op,
                                               const GDALRasterBand &firstBand,
                                               const GDALRasterBand &secondBand)
{
    nRasterXSize = firstBand.GetXSize();
    nRasterYSize = firstBand.GetYSize();
    const auto firstDT = firstBand.GetRasterDataType();
    const auto secondDT = secondBand.GetRasterDataType();
    if (op == Operation::OP_ADD && firstDT == GDT_Byte && secondDT == GDT_Byte)
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
        nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize, op,
        firstBand, &secondBand, nullptr);
    l_poDS->SetBand(1, this);
    m_poOwningDS.reset(l_poDS.release());
}

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(Operation op,
                                               const GDALRasterBand &firstBand,
                                               double constant)
{
    nRasterXSize = firstBand.GetXSize();
    nRasterYSize = firstBand.GetYSize();
    const auto firstDT = firstBand.GetRasterDataType();
    if (op == Operation::OP_ADD && firstDT == GDT_Byte && constant >= -128 &&
        constant <= 127 && std::floor(constant) == constant)
        eDataType = GDT_Byte;
    else if (firstDT == GDT_Float32 && static_cast<float>(constant) == constant)
        eDataType = GDT_Float32;
    else
        eDataType = GDT_Float64;
    firstBand.GetBlockSize(&nBlockXSize, &nBlockYSize);
    auto l_poDS = std::make_unique<GDALComputedDataset>(
        nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize, op,
        firstBand, nullptr, &constant);
    l_poDS->SetBand(1, this);
    m_poOwningDS.reset(l_poDS.release());
}

/************************************************************************/
/*                       GDALComputedRasterBand()                       */
/************************************************************************/

GDALComputedRasterBand::GDALComputedRasterBand(Operation op,
                                               const GDALRasterBand &firstBand,
                                               GDALDataType dt)
{
    CPLAssert(op == Operation::OP_CAST);
    nRasterXSize = firstBand.GetXSize();
    nRasterYSize = firstBand.GetYSize();
    eDataType = dt;
    firstBand.GetBlockSize(&nBlockXSize, &nBlockYSize);
    auto l_poDS = std::make_unique<GDALComputedDataset>(
        nRasterXSize, nRasterYSize, eDataType, nBlockXSize, nBlockYSize, op,
        firstBand, nullptr, nullptr);
    l_poDS->SetBand(1, this);
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
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr GDALComputedRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                          void *pData)
{
    auto l_poDS = cpl::down_cast<GDALComputedDataset *>(poDS);
    return l_poDS->m_oVRTDS.GetRasterBand(1)->ReadBlock(nBlockXOff, nBlockYOff,
                                                        pData);
}

/************************************************************************/
/*                           IRasterIO()                                */
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
