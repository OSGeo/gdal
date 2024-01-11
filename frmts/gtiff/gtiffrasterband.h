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

#ifndef GTIFFRASTERBAND_H_INCLUDED
#define GTIFFRASTERBAND_H_INCLUDED

#include "gdal_pam.h"

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

    int DirectIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                 int nYSize, void *pData, int nBufXSize, int nBufYSize,
                 GDALDataType eBufType, GSpacing nPixelSpace,
                 GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg);

    static void DropReferenceVirtualMem(void *pUserData);
    CPLVirtualMem *GetVirtualMemAutoInternal(GDALRWFlag eRWFlag,
                                             int *pnPixelSpace,
                                             GIntBig *pnLineSpace,
                                             char **papszOptions);

    void *CacheMultiRange(int nXOff, int nYOff, int nXSize, int nYSize,
                          int nBufXSize, int nBufYSize,
                          GDALRasterIOExtraArg *psExtraArg);

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
    virtual ~GTiffRasterBand();

    virtual bool IsBaseGTiffClass() const
    {
        return true;
    }

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;

    virtual GDALSuggestedBlockAccessPattern
    GetSuggestedBlockAccessPattern() const override
    {
        return GSBAP_RANDOM;
    }

    virtual int IGetDataCoverageStatus(int nXOff, int nYOff, int nXSize,
                                       int nYSize, int nMaskFlagStop,
                                       double *pdfDataPct) override;

    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg *psExtraArg) override final;

    virtual const char *GetDescription() const override final;
    virtual void SetDescription(const char *) override final;

    virtual GDALColorInterp GetColorInterpretation() override /*final*/;
    virtual GDALColorTable *GetColorTable() override /*final*/;
    virtual CPLErr SetColorTable(GDALColorTable *) override final;

    CPLErr SetNoDataValue(double) override final;
    CPLErr SetNoDataValueAsInt64(int64_t nNoData) override final;
    CPLErr SetNoDataValueAsUInt64(uint64_t nNoData) override final;
    double GetNoDataValue(int *pbSuccess = nullptr) override final;
    int64_t GetNoDataValueAsInt64(int *pbSuccess = nullptr) override final;
    uint64_t GetNoDataValueAsUInt64(int *pbSuccess = nullptr) override final;
    CPLErr DeleteNoDataValue() override final;

    virtual double GetOffset(int *pbSuccess = nullptr) override final;
    virtual CPLErr SetOffset(double dfNewValue) override final;
    virtual double GetScale(int *pbSuccess = nullptr) override final;
    virtual CPLErr SetScale(double dfNewValue) override final;
    virtual const char *GetUnitType() override final;
    virtual CPLErr SetUnitType(const char *pszNewValue) override final;
    virtual CPLErr SetColorInterpretation(GDALColorInterp) override final;

    virtual char **GetMetadataDomainList() override final;
    virtual CPLErr SetMetadata(char **, const char * = "") override final;
    virtual char **GetMetadata(const char *pszDomain = "") override final;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override final;
    virtual const char *
    GetMetadataItem(const char *pszName,
                    const char *pszDomain = "") override final;
    virtual int GetOverviewCount() override final;
    virtual GDALRasterBand *GetOverview(int) override final;

    virtual GDALRasterBand *GetMaskBand() override final;
    virtual int GetMaskFlags() override final;
    virtual CPLErr CreateMaskBand(int nFlags) override final;
    virtual bool IsMaskBand() const override final;
    virtual GDALMaskValueRange GetMaskValueRange() const override final;

    virtual CPLVirtualMem *
    GetVirtualMemAuto(GDALRWFlag eRWFlag, int *pnPixelSpace,
                      GIntBig *pnLineSpace, char **papszOptions) override final;

    GDALRasterAttributeTable *GetDefaultRAT() override final;
    virtual CPLErr
    SetDefaultRAT(const GDALRasterAttributeTable *) override final;
    virtual CPLErr GetHistogram(double dfMin, double dfMax, int nBuckets,
                                GUIntBig *panHistogram, int bIncludeOutOfRange,
                                int bApproxOK, GDALProgressFunc,
                                void *pProgressData) override final;

    virtual CPLErr GetDefaultHistogram(double *pdfMin, double *pdfMax,
                                       int *pnBuckets, GUIntBig **ppanHistogram,
                                       int bForce, GDALProgressFunc,
                                       void *pProgressData) override final;
};

#endif  //  GTIFFRASTERBAND_H_INCLUDED
