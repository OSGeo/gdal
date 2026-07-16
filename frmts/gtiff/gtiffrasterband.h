/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GTIFFRASTERBAND_H_INCLUDED
#define GTIFFRASTERBAND_H_INCLUDED

#include "gdal_pam.h"
#include "gdal_rat.h"

#include "gtiff.h"

#include <set>

/************************************************************************/
/* ==================================================================== */
/*                            GTiffRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class GTiffDataset;

class GTiffRasterBand CPL_NON_FINAL : public GDALPamRasterBand
{
    CPL_DISALLOW_COPY_ASSIGN(GTiffRasterBand)

    friend class GTiffDataset;

    double m_dfOffset = 0;
    double m_dfScale = 1;
    CPLString m_osUnitType{};
    CPLString m_osDescription{};
    GDALColorInterp m_eBandInterp = GCI_Undefined;
    std::set<GTiffRasterBand **> m_aSetPSelf{};
    bool m_bHaveOffsetScale = false;
    bool m_bRATSet = false;
    bool m_bRATTriedReadingFromPAM = false;
    std::unique_ptr<GDALRasterAttributeTable> m_poRAT{};

    int DirectIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                 int nYSize, void *pData, int nBufXSize, int nBufYSize,
                 GDALDataType eBufType, GSpacing nPixelSpace,
                 GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg);

    static void DropReferenceVirtualMem(void *pUserData);
    CPLVirtualMem *GetVirtualMemAutoInternal(GDALRWFlag eRWFlag,
                                             int *pnPixelSpace,
                                             GIntBig *pnLineSpace,
                                             CSLConstList papszOptions);

  protected:
    GTiffDataset *m_poGDS = nullptr;
    GDALMultiDomainMetadata m_oGTiffMDMD{};

    double m_dfNoDataValue = DEFAULT_NODATA_VALUE;
    bool m_bNoDataSet = false;

    int64_t m_nNoDataValueInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;
    bool m_bNoDataSetAsInt64 = false;

    uint64_t m_nNoDataValueUInt64 = GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
    bool m_bNoDataSetAsUInt64 = false;

    void NullBlock(void *pData);
    CPLErr FillCacheForOtherBands(int nBlockXOff, int nBlockYOff);
    void CacheMaskForBlock(int nBlockXOff, int nBlockYOff);
    void ResetNoDataValues(bool bResetDatasetToo);

    int ComputeBlockId(int nBlockXOff, int nBlockYOff) const;

  public:
    GTiffRasterBand(GTiffDataset *, int);
    ~GTiffRasterBand() override;

    virtual bool IsBaseGTiffClass() const
    {
        return true;
    }

    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IWriteBlock(int, int, void *) override;

    virtual GDALSuggestedBlockAccessPattern
    GetSuggestedBlockAccessPattern() const override
    {
        return GSBAP_RANDOM;
    }

    virtual int IGetDataCoverageStatus(int nXOff, int nYOff, int nXSize,
                                       int nYSize, int nMaskFlagStop,
                                       double *pdfDataPct) override;

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override final;

    const char *GetDescription() const override final;
    void SetDescription(const char *) override final;

    GDALColorInterp GetColorInterpretation() override /*final*/;
    GDALColorTable *GetColorTable() override /*final*/;
    CPLErr SetColorTable(GDALColorTable *) override final;

    CPLErr SetNoDataValue(double) override final;
    CPLErr SetNoDataValueAsInt64(int64_t nNoData) override final;
    CPLErr SetNoDataValueAsUInt64(uint64_t nNoData) override final;
    double GetNoDataValue(int *pbSuccess = nullptr) override final;
    int64_t GetNoDataValueAsInt64(int *pbSuccess = nullptr) override final;
    uint64_t GetNoDataValueAsUInt64(int *pbSuccess = nullptr) override final;
    CPLErr DeleteNoDataValue() override final;

    double GetOffset(int *pbSuccess = nullptr) override final;
    CPLErr SetOffset(double dfNewValue) override final;
    double GetScale(int *pbSuccess = nullptr) override final;
    CPLErr SetScale(double dfNewValue) override final;
    const char *GetUnitType() override final;
    CPLErr SetUnitType(const char *pszNewValue) override final;
    CPLErr SetColorInterpretation(GDALColorInterp) override;

    char **GetMetadataDomainList() override final;
    CPLErr SetMetadata(CSLConstList, const char * = "") override final;
    CSLConstList GetMetadata(const char *pszDomain = "") override final;
    CPLErr SetMetadataItem(const char *, const char *,
                           const char * = "") override final;
    virtual const char *
    GetMetadataItem(const char *pszName,
                    const char *pszDomain = "") override final;
    int GetOverviewCount() override final;
    GDALRasterBand *GetOverview(int) override final;

    GDALRasterBand *GetMaskBand() override final;
    int GetMaskFlags() override final;
    CPLErr CreateMaskBand(int nFlags) override final;
    bool IsMaskBand() const override final;
    GDALMaskValueRange GetMaskValueRange() const override final;

    virtual CPLVirtualMem *
    GetVirtualMemAuto(GDALRWFlag eRWFlag, int *pnPixelSpace,
                      GIntBig *pnLineSpace,
                      CSLConstList papszOptions) override final;

    GDALRasterAttributeTable *GetDefaultRAT() override final;
    virtual CPLErr
    SetDefaultRAT(const GDALRasterAttributeTable *) override final;
    CPLErr GetHistogram(double dfMin, double dfMax, int nBuckets,
                        GUIntBig *panHistogram, int bIncludeOutOfRange,
                        int bApproxOK, GDALProgressFunc,
                        void *pProgressData) override final;

    CPLErr GetDefaultHistogram(double *pdfMin, double *pdfMax, int *pnBuckets,
                               GUIntBig **ppanHistogram, int bForce,
                               GDALProgressFunc,
                               void *pProgressData) override final;

    bool MayMultiBlockReadingBeMultiThreaded() const override final;
};

#endif  //  GTIFFRASTERBAND_H_INCLUDED
