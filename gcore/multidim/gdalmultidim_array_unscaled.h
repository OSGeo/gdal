/******************************************************************************
 *
 * Name:     gdalmultidim_array_unscaled.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALMDArrayUnscaled
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALMULTIDIM_ARRAY_UNSCALED_H
#define GDALMULTIDIM_ARRAY_UNSCALED_H

#include "gdal_multidim.h"
#include "gdal_pam_multidim.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         GDALMDArrayUnscaled                          */
/************************************************************************/

class GDALMDArrayUnscaled final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    const GDALExtendedDataType m_dt;
    bool m_bHasNoData;
    const double m_dfScale;
    const double m_dfOffset;
    std::vector<GByte> m_abyRawNoData{};

  protected:
    explicit GDALMDArrayUnscaled(const std::shared_ptr<GDALMDArray> &poParent,
                                 double dfScale, double dfOffset,
                                 double dfOverriddenDstNodata, GDALDataType eDT)
        : GDALAbstractMDArray(std::string(),
                              "Unscaled view of " + poParent->GetFullName()),
          GDALPamMDArray(
              std::string(), "Unscaled view of " + poParent->GetFullName(),
              GDALPamMultiDim::GetPAM(poParent), poParent->GetContext()),
          m_poParent(std::move(poParent)),
          m_dt(GDALExtendedDataType::Create(eDT)),
          m_bHasNoData(m_poParent->GetRawNoDataValue() != nullptr),
          m_dfScale(dfScale), m_dfOffset(dfOffset)
    {
        m_abyRawNoData.resize(m_dt.GetSize());
        const auto eNonComplexDT =
            GDALGetNonComplexDataType(m_dt.GetNumericDataType());
        GDALCopyWords64(
            &dfOverriddenDstNodata, GDT_Float64, 0, m_abyRawNoData.data(),
            eNonComplexDT, GDALGetDataTypeSizeBytes(eNonComplexDT),
            GDALDataTypeIsComplex(m_dt.GetNumericDataType()) ? 2 : 1);
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

    bool IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                const GDALExtendedDataType &bufferDataType,
                const void *pSrcBuffer) override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override
    {
        return m_poParent->AdviseRead(arrayStartIdx, count, papszOptions);
    }

  public:
    static std::shared_ptr<GDALMDArrayUnscaled>
    Create(const std::shared_ptr<GDALMDArray> &poParent, double dfScale,
           double dfOffset, double dfDstNodata, GDALDataType eDT)
    {
        auto newAr(std::shared_ptr<GDALMDArrayUnscaled>(new GDALMDArrayUnscaled(
            poParent, dfScale, dfOffset, dfDstNodata, eDT)));
        newAr->SetSelf(newAr);
        return newAr;
    }

    bool IsWritable() const override
    {
        return m_poParent->IsWritable();
    }

    const std::string &GetFilename() const override
    {
        return m_poParent->GetFilename();
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_poParent->GetDimensions();
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_dt;
    }

    const std::string &GetUnit() const override
    {
        return m_poParent->GetUnit();
    }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        return m_poParent->GetSpatialRef();
    }

    const void *GetRawNoDataValue() const override
    {
        return m_bHasNoData ? m_abyRawNoData.data() : nullptr;
    }

    bool SetRawNoDataValue(const void *pRawNoData) override
    {
        m_bHasNoData = true;
        memcpy(m_abyRawNoData.data(), pRawNoData, m_dt.GetSize());
        return true;
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        return m_poParent->GetBlockSize();
    }

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override
    {
        return m_poParent->GetAttribute(osName);
    }

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override
    {
        return m_poParent->GetAttributes(papszOptions);
    }

    bool SetUnit(const std::string &osUnit) override
    {
        return m_poParent->SetUnit(osUnit);
    }

    bool SetSpatialRef(const OGRSpatialReference *poSRS) override
    {
        return m_poParent->SetSpatialRef(poSRS);
    }

    std::shared_ptr<GDALAttribute>
    CreateAttribute(const std::string &osName,
                    const std::vector<GUInt64> &anDimensions,
                    const GDALExtendedDataType &oDataType,
                    CSLConstList papszOptions = nullptr) override
    {
        return m_poParent->CreateAttribute(osName, anDimensions, oDataType,
                                           papszOptions);
    }
};

//! @endcond

#endif  // GDALMULTIDIM_ARRAY_UNSCALED_H
