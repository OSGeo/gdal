/******************************************************************************
 *
 * Project:  Raw Translator
 * Purpose:  Implementation of RawDataset class.  Intended to be subclassed
 *           by other raw formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_FRMTS_RAW_RAWDATASET_H_INCLUDED
#define GDAL_FRMTS_RAW_RAWDATASET_H_INCLUDED

#include "gdal_pam.h"

#include <atomic>
#include <utility>

/************************************************************************/
/* ==================================================================== */
/*                              RawDataset                              */
/* ==================================================================== */
/************************************************************************/

class RawRasterBand;

/**
 * \brief Abstract Base Class dedicated to define new raw dataset types.
 */
class CPL_DLL RawDataset : public GDALPamDataset
{
    friend class RawRasterBand;

  protected:
    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, int, BANDMAP_TYPE, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;
    virtual CPLErr Close() override = 0;

  public:
    RawDataset();
    virtual ~RawDataset() = 0;

    enum class Interleave
    {
        BSQ,
        BIL,
        BIP,
    };

    bool GetRawBinaryLayout(GDALDataset::RawBinaryLayout &) override;
    void ClearCachedConfigOption(void);

  private:
    CPL_DISALLOW_COPY_ASSIGN(RawDataset)
  protected:
    std::atomic<int> cachedCPLOneBigReadOption = {
        0};  // [0-7] bits are "valid", [8-15] bits are "value"
};

/************************************************************************/
/* ==================================================================== */
/*                            RawRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/**
 * \brief Abstract Base Class dedicated to define raw datasets.
 * \note It is not defined an Abstract Base Class, but it is advised to
 * consider it as such and not use it directly in client's code.
 */
class CPL_DLL RawRasterBand : public GDALPamRasterBand
{
  public:
    enum class ByteOrder
    {
        ORDER_LITTLE_ENDIAN,
        ORDER_BIG_ENDIAN,
        ORDER_VAX  // only valid for Float32, Float64, CFloat32 and CFloat64
    };

#ifdef CPL_LSB
    static constexpr ByteOrder NATIVE_BYTE_ORDER =
        ByteOrder::ORDER_LITTLE_ENDIAN;
#else
    static constexpr ByteOrder NATIVE_BYTE_ORDER = ByteOrder::ORDER_BIG_ENDIAN;
#endif

  protected:
    friend class RawDataset;

    static constexpr int NO_SCANLINE_LOADED = -1;

    VSILFILE *fpRawL{};

    vsi_l_offset nImgOffset{};
    int nPixelOffset{};
    int nLineOffset{};
    int nLineSize{};
    ByteOrder eByteOrder{};

    int nLoadedScanline = NO_SCANLINE_LOADED;
    void *pLineBuffer{};
    void *pLineStart{};
    bool bNeedFileFlush = false;
    bool bLoadedScanlineDirty = false;  // true when the buffer has
                                        // modified content that needs to
                                        // be pushed to disk

    GDALColorTable *poCT{};
    GDALColorInterp eInterp = GCI_Undefined;

    char **papszCategoryNames{};

    int bOwnsFP{};

    int Seek(vsi_l_offset, int);
    size_t Read(void *, size_t, size_t);
    size_t Write(void *, size_t, size_t);

    CPLErr AccessBlock(vsi_l_offset nBlockOff, size_t nBlockSize, void *pData,
                       size_t nValues);
    int IsSignificantNumberOfLinesLoaded(int nLineOff, int nLines);
    void Initialize();

    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, GSpacing nPixelSpace, GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    int CanUseDirectIO(int nXOff, int nYOff, int nXSize, int nYSize,
                       GDALDataType eBufType, GDALRasterIOExtraArg *psExtraArg);

  public:
    enum class OwnFP
    {
        NO,
        YES
    };

    // IsValid() should be called afterwards
    RawRasterBand(GDALDataset *poDS, int nBand, VSILFILE *fpRaw,
                  vsi_l_offset nImgOffset, int nPixelOffset, int nLineOffset,
                  GDALDataType eDataType, int bNativeOrder, OwnFP bOwnsFP);

    // IsValid() should be called afterwards
    RawRasterBand(GDALDataset *poDS, int nBand, VSILFILE *fpRaw,
                  vsi_l_offset nImgOffset, int nPixelOffset, int nLineOffset,
                  GDALDataType eDataType, ByteOrder eByteOrder, OwnFP bOwnsFP);

    // IsValid() should be called afterwards
    RawRasterBand(VSILFILE *fpRaw, vsi_l_offset nImgOffset, int nPixelOffset,
                  int nLineOffset, GDALDataType eDataType, int bNativeOrder,
                  int nXSize, int nYSize, OwnFP bOwnsFP);

    // IsValid() should be called afterwards
    RawRasterBand(VSILFILE *fpRaw, vsi_l_offset nImgOffset, int nPixelOffset,
                  int nLineOffset, GDALDataType eDataType, ByteOrder eByteOrder,
                  int nXSize, int nYSize, OwnFP bOwnsFP);

    // Returns nullptr in case of error
    static std::unique_ptr<RawRasterBand>
    Create(GDALDataset *poDS, int nBand, VSILFILE *fpRaw,
           vsi_l_offset nImgOffset, int nPixelOffset, int nLineOffset,
           GDALDataType eDataType, ByteOrder eByteOrder, OwnFP bOwnsFP);

    // Returns nullptr in case of error
    static std::unique_ptr<RawRasterBand>
    Create(VSILFILE *fpRaw, vsi_l_offset nImgOffset, int nPixelOffset,
           int nLineOffset, GDALDataType eDataType, ByteOrder eByteOrder,
           int nXSize, int nYSize, OwnFP bOwnsFP);

    virtual ~RawRasterBand() /* = 0 */;

    bool IsValid() const
    {
        return pLineStart != nullptr;
    }

    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IWriteBlock(int, int, void *) override;

    GDALColorTable *GetColorTable() override;
    GDALColorInterp GetColorInterpretation() override;
    CPLErr SetColorTable(GDALColorTable *) override;
    CPLErr SetColorInterpretation(GDALColorInterp) override;

    char **GetCategoryNames() override;
    CPLErr SetCategoryNames(char **) override;

    CPLErr FlushCache(bool bAtClosing) override;

    CPLVirtualMem *GetVirtualMemAuto(GDALRWFlag eRWFlag, int *pnPixelSpace,
                                     GIntBig *pnLineSpace,
                                     char **papszOptions) override;

    CPLErr AccessLine(int iLine);

    void SetAccess(GDALAccess eAccess);

    // this is deprecated.
    void StoreNoDataValue(double);

    // Query methods for internal data.
    vsi_l_offset GetImgOffset() const
    {
        return nImgOffset;
    }

    int GetPixelOffset() const
    {
        return nPixelOffset;
    }

    int GetLineOffset() const
    {
        return nLineOffset;
    }

    ByteOrder GetByteOrder() const
    {
        return eByteOrder;
    }

    VSILFILE *GetFPL() const
    {
        return fpRawL;
    }

    int GetOwnsFP() const
    {
        return bOwnsFP;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(RawRasterBand)

    bool NeedsByteOrderChange() const;
    void DoByteSwap(void *pBuffer, size_t nValues, int nByteSkip,
                    bool bDiskToCPU) const;
    bool IsBIP() const;
    vsi_l_offset ComputeFileOffset(int iLine) const;
    bool FlushCurrentLine(bool bNeedUsableBufferAfter);
    CPLErr BIPWriteBlock(int nBlockYOff, int nCallingBand, const void *pImage);
};

#ifdef GDAL_COMPILATION

bool CPL_DLL RAWDatasetCheckMemoryUsage(int nXSize, int nYSize, int nBands,
                                        int nDTSize, int nPixelOffset,
                                        int nLineOffset,
                                        vsi_l_offset nHeaderSize,
                                        vsi_l_offset nBandOffset, VSILFILE *fp);

#endif

#endif  // GDAL_FRMTS_RAW_RAWDATASET_H_INCLUDED
