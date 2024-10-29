/******************************************************************************
 * $Id$
 *
 * Project:  Memory Array Translator
 * Purpose:  Declaration of MEMDataset, and MEMRasterBand.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MEMDATASET_H_INCLUDED
#define MEMDATASET_H_INCLUDED

#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_rat.h"

#include <memory>

CPL_C_START
/* Caution: if changing this prototype, also change in
   swig/include/gdal_python.i where it is redefined */
GDALRasterBandH CPL_DLL MEMCreateRasterBand(GDALDataset *, int, GByte *,
                                            GDALDataType, int, int, int);
GDALRasterBandH CPL_DLL MEMCreateRasterBandEx(GDALDataset *, int, GByte *,
                                              GDALDataType, GSpacing, GSpacing,
                                              int);
CPL_C_END

/************************************************************************/
/*                            MEMDataset                                */
/************************************************************************/

class MEMRasterBand;

class CPL_DLL MEMDataset CPL_NON_FINAL : public GDALDataset
{
    CPL_DISALLOW_COPY_ASSIGN(MEMDataset)

    friend class MEMRasterBand;

    int bGeoTransformSet;
    double adfGeoTransform[6];

    OGRSpatialReference m_oSRS{};

    std::vector<gdal::GCP> m_aoGCPs{};
    OGRSpatialReference m_oGCPSRS{};

    std::vector<std::unique_ptr<GDALDataset>> m_apoOverviewDS{};

    struct Private;
    std::unique_ptr<Private> m_poPrivate;

#if 0
  protected:
    virtual int                 EnterReadWrite(GDALRWFlag eRWFlag);
    virtual void                LeaveReadWrite();
#endif

    friend void GDALRegister_MEM();

    // cppcheck-suppress unusedPrivateFunction
    static GDALDataset *CreateBase(const char *pszFilename, int nXSize,
                                   int nYSize, int nBands, GDALDataType eType,
                                   char **papszParamList);

  protected:
    bool CanBeCloned(int nScopeFlags, bool bCanShareState) const override;

    std::unique_ptr<GDALDataset> Clone(int nScopeFlags,
                                       bool bCanShareState) const override;

  public:
    MEMDataset();
    virtual ~MEMDataset();

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr GetGeoTransform(double *) override;
    virtual CPLErr SetGeoTransform(double *) override;

    virtual void *GetInternalHandle(const char *) override;

    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    CPLErr SetGCPs(int nGCPCount, const GDAL_GCP *pasGCPList,
                   const OGRSpatialReference *poSRS) override;
    virtual CPLErr AddBand(GDALDataType eType,
                           char **papszOptions = nullptr) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, BANDMAP_TYPE panBandMap,
                             GSpacing nPixelSpaceBuf, GSpacing nLineSpaceBuf,
                             GSpacing nBandSpaceBuf,
                             GDALRasterIOExtraArg *psExtraArg) override;
    virtual CPLErr IBuildOverviews(const char *pszResampling, int nOverviews,
                                   const int *panOverviewList, int nListBands,
                                   const int *panBandList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions) override;

    virtual CPLErr CreateMaskBand(int nFlagsIn) override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override;

    void AddMEMBand(GDALRasterBandH hMEMBand);

    static GDALDataset *Open(GDALOpenInfo *);
    static MEMDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                              int nBands, GDALDataType eType,
                              char **papszParamList);
    static GDALDataset *
    CreateMultiDimensional(const char *pszFilename,
                           CSLConstList papszRootGroupOptions,
                           CSLConstList papszOptions);
};

/************************************************************************/
/*                            MEMRasterBand                             */
/************************************************************************/

class CPL_DLL MEMRasterBand CPL_NON_FINAL : public GDALPamRasterBand
{
  private:
    CPL_DISALLOW_COPY_ASSIGN(MEMRasterBand)

  protected:
    friend class MEMDataset;

    GByte *pabyData;
    GSpacing nPixelOffset;
    GSpacing nLineOffset;
    int bOwnData;

    bool m_bIsMask = false;

    MEMRasterBand(GByte *pabyDataIn, GDALDataType eTypeIn, int nXSizeIn,
                  int nYSizeIn, bool bOwnDataIn);

  public:
    MEMRasterBand(GDALDataset *poDS, int nBand, GByte *pabyData,
                  GDALDataType eType, GSpacing nPixelOffset,
                  GSpacing nLineOffset, int bAssumeOwnership,
                  const char *pszPixelType = nullptr);
    virtual ~MEMRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int, int, void *) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             GSpacing nPixelSpaceBuf, GSpacing nLineSpaceBuf,
                             GDALRasterIOExtraArg *psExtraArg) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual CPLErr CreateMaskBand(int nFlagsIn) override;
    virtual bool IsMaskBand() const override;

    // Allow access to MEM driver's private internal memory buffer.
    GByte *GetData() const
    {
        return (pabyData);
    }
};

#endif /* ndef MEMDATASET_H_INCLUDED */
