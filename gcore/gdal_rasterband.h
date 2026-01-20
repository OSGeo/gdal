/******************************************************************************
 *
 * Name:     gdal_rasterband.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALRasterBand class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALRASTERBAND_H_INCLUDED
#define GDALRASTERBAND_H_INCLUDED

#include "cpl_port.h"
#include "gdal.h"
#include "gdal_majorobject.h"

#include <cstddef>
#include <cstdint>
#include <complex>
#include <iterator>
#include <memory>
#if __cplusplus >= 202002L
#include <span>
#endif
#include <vector>

//! @cond Doxygen_Suppress
#if !defined(OPTIONAL_OUTSIDE_GDAL)
#if defined(GDAL_COMPILATION)
#define OPTIONAL_OUTSIDE_GDAL(val)
#else
#define OPTIONAL_OUTSIDE_GDAL(val) = val
#endif
#endif
//! @endcond

/* ******************************************************************** */
/*                            GDALRasterBand                            */
/* ******************************************************************** */

class GDALAbstractBandBlockCache;
class GDALColorTable;
class GDALDataset;
class GDALDoublePointsCache;
class GDALRasterAttributeTable;
class GDALRasterBlock;
class GDALMDArray;
class OGRSpatialReference;

/** Range of values found in a mask band */
typedef enum
{
    GMVR_UNKNOWN, /*! Unknown (can also be used for any values between 0 and 255
                     for a Byte band) */
    GMVR_0_AND_1_ONLY,   /*! Only 0 and 1 */
    GMVR_0_AND_255_ONLY, /*! Only 0 and 255 */
} GDALMaskValueRange;

/** Suggested/most efficient access pattern to blocks. */
typedef int GDALSuggestedBlockAccessPattern;

/** Unknown, or no particular read order is suggested. */
constexpr GDALSuggestedBlockAccessPattern GSBAP_UNKNOWN = 0;

/** Random access to blocks is efficient. */
constexpr GDALSuggestedBlockAccessPattern GSBAP_RANDOM = 1;

/** Reading by strips from top to bottom is the most efficient. */
constexpr GDALSuggestedBlockAccessPattern GSBAP_TOP_TO_BOTTOM = 2;

/** Reading by strips from bottom to top is the most efficient. */
constexpr GDALSuggestedBlockAccessPattern GSBAP_BOTTOM_TO_TOP = 3;

/** Reading the largest chunk from the raster is the most efficient (can be
 * combined with above values). */
constexpr GDALSuggestedBlockAccessPattern GSBAP_LARGEST_CHUNK_POSSIBLE = 0x100;

class GDALComputedRasterBand;

/** A rectangular subset of pixels within a raster */
class GDALRasterWindow
{
  public:
    /** left offset of the window */
    int nXOff;

    /** top offset of the window */
    int nYOff;

    /** window width */
    int nXSize;

    /** window height */
    int nYSize;
};

/** A single raster band (or channel). */

class CPL_DLL GDALRasterBand : public GDALMajorObject
{
  private:
    friend class GDALArrayBandBlockCache;
    friend class GDALHashSetBandBlockCache;
    friend class GDALRasterBlock;
    friend class GDALDataset;

    CPLErr eFlushBlockErr = CE_None;
    GDALAbstractBandBlockCache *poBandBlockCache = nullptr;

    CPL_INTERNAL void SetFlushBlockErr(CPLErr eErr);
    CPL_INTERNAL CPLErr UnreferenceBlock(GDALRasterBlock *poBlock);
    CPL_INTERNAL void IncDirtyBlocks(int nInc);

    CPL_INTERNAL CPLErr RasterIOInternal(
        GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
        void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
        GSpacing nPixelSpace, GSpacing nLineSpace,
        GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;

    CPL_INTERNAL bool HasNoData() const;

  protected:
    GDALRasterBand();
    explicit GDALRasterBand(int bForceCachedIO);

    //! @cond Doxygen_Suppress
    GDALRasterBand(GDALRasterBand &&) = default;
    //! @endcond

    //! @cond Doxygen_Suppress
    GDALDataset *poDS = nullptr;
    int nBand = 0; /* 1 based */

    int nRasterXSize = 0;
    int nRasterYSize = 0;

    GDALDataType eDataType = GDT_UInt8;
    GDALAccess eAccess = GA_ReadOnly;

    /* stuff related to blocking, and raster cache */
    int nBlockXSize = -1;
    int nBlockYSize = -1;
    int nBlocksPerRow = 0;
    int nBlocksPerColumn = 0;

    int nBlockReads = 0;
    int bForceCachedIO = 0;

    friend class GDALComputedRasterBand;
    friend class GDALComputedDataset;

    class GDALRasterBandOwnedOrNot
    {
      public:
        GDALRasterBandOwnedOrNot() = default;

        GDALRasterBandOwnedOrNot(GDALRasterBandOwnedOrNot &&) = default;

        void reset()
        {
            m_poBandOwned.reset();
            m_poBandRef = nullptr;
        }

        void resetNotOwned(GDALRasterBand *poBand)
        {
            m_poBandOwned.reset();
            m_poBandRef = poBand;
        }

        void reset(std::unique_ptr<GDALRasterBand> poBand)
        {
            m_poBandOwned = std::move(poBand);
            m_poBandRef = nullptr;
        }

        const GDALRasterBand *get() const
        {
            return static_cast<const GDALRasterBand *>(*this);
        }

        GDALRasterBand *get()
        {
            return static_cast<GDALRasterBand *>(*this);
        }

        bool IsOwned() const
        {
            return m_poBandOwned != nullptr;
        }

        operator const GDALRasterBand *() const
        {
            return m_poBandOwned ? m_poBandOwned.get() : m_poBandRef;
        }

        operator GDALRasterBand *()
        {
            return m_poBandOwned ? m_poBandOwned.get() : m_poBandRef;
        }

      private:
        CPL_DISALLOW_COPY_ASSIGN(GDALRasterBandOwnedOrNot)
        std::unique_ptr<GDALRasterBand> m_poBandOwned{};
        GDALRasterBand *m_poBandRef = nullptr;
    };

    GDALRasterBandOwnedOrNot poMask{};
    bool m_bEnablePixelTypeSignedByteWarning =
        true;  // Remove me in GDAL 4.0. See GetMetadataItem() implementation
    int nMaskFlags = 0;

    void InvalidateMaskBand();

    friend class GDALProxyRasterBand;
    friend class GDALDefaultOverviews;

    CPLErr
    RasterIOResampled(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                      int nYSize, void *pData, int nBufXSize, int nBufYSize,
                      GDALDataType eBufType, GSpacing nPixelSpace,
                      GSpacing nLineSpace,
                      GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;

    int EnterReadWrite(GDALRWFlag eRWFlag);
    void LeaveReadWrite();
    void InitRWLock();
    void SetValidPercent(GUIntBig nSampleCount, GUIntBig nValidCount);

    mutable GDALDoublePointsCache *m_poPointsCache = nullptr;

    //! @endcond

  protected:
    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) = 0;
    virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff, void *pData);

    virtual CPLErr
    IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
              void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
              GSpacing nPixelSpace, GSpacing nLineSpace,
              GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;

    virtual int IGetDataCoverageStatus(int nXOff, int nYOff, int nXSize,
                                       int nYSize, int nMaskFlagStop,
                                       double *pdfDataPct);

    virtual bool
    EmitErrorMessageIfWriteNotSupported(const char *pszCaller) const;

    //! @cond Doxygen_Suppress
    CPLErr
    OverviewRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;

    CPLErr TryOverviewRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                               int nXSize, int nYSize, void *pData,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, GSpacing nPixelSpace,
                               GSpacing nLineSpace,
                               GDALRasterIOExtraArg *psExtraArg, int *pbTried);

    CPLErr SplitRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                         int nYSize, void *pData, int nBufXSize, int nBufYSize,
                         GDALDataType eBufType, GSpacing nPixelSpace,
                         GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)
        CPL_WARN_UNUSED_RESULT;

    int InitBlockInfo();

    void AddBlockToFreeList(GDALRasterBlock *);

    bool HasBlockCache() const
    {
        return poBandBlockCache != nullptr;
    }

    bool HasDirtyBlocks() const;

    //! @endcond

  public:
    ~GDALRasterBand() override;

    int GetXSize() const;
    int GetYSize() const;
    int GetBand() const;
    GDALDataset *GetDataset() const;

    GDALDataType GetRasterDataType(void) const;
    void GetBlockSize(int *pnXSize, int *pnYSize) const;
    CPLErr GetActualBlockSize(int nXBlockOff, int nYBlockOff, int *pnXValid,
                              int *pnYValid) const;

    virtual GDALSuggestedBlockAccessPattern
    GetSuggestedBlockAccessPattern() const;

    GDALAccess GetAccess();

#ifndef DOXYGEN_SKIP
    CPLErr RasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                    int nYSize, void *pData, int nBufXSize, int nBufYSize,
                    GDALDataType eBufType, GSpacing nPixelSpace,
                    GSpacing nLineSpace,
                    GDALRasterIOExtraArg *psExtraArg
                        OPTIONAL_OUTSIDE_GDAL(nullptr)) CPL_WARN_UNUSED_RESULT;
#else
    CPLErr RasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                    int nYSize, void *pData, int nBufXSize, int nBufYSize,
                    GDALDataType eBufType, GSpacing nPixelSpace,
                    GSpacing nLineSpace,
                    GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;
#endif

    template <class T>
    CPLErr ReadRaster(T *pData, size_t nArrayEltCount = 0, double dfXOff = 0,
                      double dfYOff = 0, double dfXSize = 0, double dfYSize = 0,
                      size_t nBufXSize = 0, size_t nBufYSize = 0,
                      GDALRIOResampleAlg eResampleAlg = GRIORA_NearestNeighbour,
                      GDALProgressFunc pfnProgress = nullptr,
                      void *pProgressData = nullptr) const;

    template <class T>
    CPLErr ReadRaster(std::vector<T> &vData, double dfXOff = 0,
                      double dfYOff = 0, double dfXSize = 0, double dfYSize = 0,
                      size_t nBufXSize = 0, size_t nBufYSize = 0,
                      GDALRIOResampleAlg eResampleAlg = GRIORA_NearestNeighbour,
                      GDALProgressFunc pfnProgress = nullptr,
                      void *pProgressData = nullptr) const;

#if __cplusplus >= 202002L
    //! @cond Doxygen_Suppress
    template <class T>
    inline CPLErr
    ReadRaster(std::span<T> pData, double dfXOff = 0, double dfYOff = 0,
               double dfXSize = 0, double dfYSize = 0, size_t nBufXSize = 0,
               size_t nBufYSize = 0,
               GDALRIOResampleAlg eResampleAlg = GRIORA_NearestNeighbour,
               GDALProgressFunc pfnProgress = nullptr,
               void *pProgressData = nullptr) const
    {
        return ReadRaster(pData.data(), pData.size(), dfXOff, dfYOff, dfXSize,
                          dfYSize, nBufXSize, nBufYSize, eResampleAlg,
                          pfnProgress, pProgressData);
    }

    //! @endcond
#endif

    GDALComputedRasterBand
    operator+(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator+(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator+(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator-(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator-(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator-(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator*(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator*(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator*(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator/(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator/(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator/(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator>(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator>(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator>(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator>=(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator>=(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator>=(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator<(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator<(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator<(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator<=(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator<=(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator<=(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator==(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator==(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator==(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator!=(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator!=(double cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator!=(double cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#endif

    GDALComputedRasterBand
    operator&&(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator&&(bool cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator&&(bool cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand
    operator||(const GDALRasterBand &other) const CPL_WARN_UNUSED_RESULT;
    GDALComputedRasterBand operator||(bool cst) const CPL_WARN_UNUSED_RESULT;
    friend GDALComputedRasterBand CPL_DLL
    operator||(bool cst, const GDALRasterBand &other) CPL_WARN_UNUSED_RESULT;

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    GDALComputedRasterBand operator!() const CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand operator-() const CPL_WARN_UNUSED_RESULT;

    GDALComputedRasterBand AsType(GDALDataType) const CPL_WARN_UNUSED_RESULT;

    CPLErr ReadBlock(int nXBlockOff, int nYBlockOff,
                     void *pImage) CPL_WARN_UNUSED_RESULT;

    CPLErr WriteBlock(int nXBlockOff, int nYBlockOff,
                      void *pImage) CPL_WARN_UNUSED_RESULT;

    // This method should only be overloaded by GDALProxyRasterBand
    virtual GDALRasterBlock *
    GetLockedBlockRef(int nXBlockOff, int nYBlockOff,
                      int bJustInitialize = FALSE) CPL_WARN_UNUSED_RESULT;

    // This method should only be overloaded by GDALProxyRasterBand
    virtual GDALRasterBlock *
    TryGetLockedBlockRef(int nXBlockOff,
                         int nYBlockYOff) CPL_WARN_UNUSED_RESULT;

    // This method should only be overloaded by GDALProxyRasterBand
    virtual CPLErr FlushBlock(int nXBlockOff, int nYBlockOff,
                              int bWriteDirtyBlock = TRUE);

    unsigned char *
    GetIndexColorTranslationTo(/* const */ GDALRasterBand *poReferenceBand,
                               unsigned char *pTranslationTable = nullptr,
                               int *pApproximateMatching = nullptr);

    // New OpengIS CV_SampleDimension stuff.

    virtual CPLErr FlushCache(bool bAtClosing = false);
    virtual CPLErr DropCache();
    virtual char **GetCategoryNames();
    virtual double GetNoDataValue(int *pbSuccess = nullptr);
    virtual int64_t GetNoDataValueAsInt64(int *pbSuccess = nullptr);
    virtual uint64_t GetNoDataValueAsUInt64(int *pbSuccess = nullptr);
    virtual double GetMinimum(int *pbSuccess = nullptr);
    virtual double GetMaximum(int *pbSuccess = nullptr);
    virtual double GetOffset(int *pbSuccess = nullptr);
    virtual double GetScale(int *pbSuccess = nullptr);
    virtual const char *GetUnitType();
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0);

    virtual CPLErr SetCategoryNames(char **papszNames);
    virtual CPLErr SetNoDataValue(double dfNoData);
    virtual CPLErr SetNoDataValueAsInt64(int64_t nNoData);
    virtual CPLErr SetNoDataValueAsUInt64(uint64_t nNoData);
    CPLErr SetNoDataValueAsString(const char *pszNoData,
                                  bool *pbCannotBeExactlyRepresented = nullptr);
    virtual CPLErr DeleteNoDataValue();
    virtual CPLErr SetColorTable(GDALColorTable *poCT);
    virtual CPLErr SetColorInterpretation(GDALColorInterp eColorInterp);
    virtual CPLErr SetOffset(double dfNewOffset);
    virtual CPLErr SetScale(double dfNewScale);
    virtual CPLErr SetUnitType(const char *pszNewValue);

    virtual CPLErr GetStatistics(int bApproxOK, int bForce, double *pdfMin,
                                 double *pdfMax, double *pdfMean,
                                 double *padfStdDev);
    virtual CPLErr ComputeStatistics(int bApproxOK, double *pdfMin,
                                     double *pdfMax, double *pdfMean,
                                     double *pdfStdDev, GDALProgressFunc,
                                     void *pProgressData);
    virtual CPLErr SetStatistics(double dfMin, double dfMax, double dfMean,
                                 double dfStdDev);
    virtual CPLErr ComputeRasterMinMax(int bApproxOK, double *adfMinMax);
    virtual CPLErr ComputeRasterMinMaxLocation(double *pdfMin, double *pdfMax,
                                               int *pnMinX, int *pnMinY,
                                               int *pnMaxX, int *pnMaxY);

// Only defined when Doxygen enabled
#ifdef DOXYGEN_SKIP
    CPLErr SetMetadata(char **papszMetadata, const char *pszDomain) override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain) override;
#endif
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;

    virtual int HasArbitraryOverviews();
    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int i);
    virtual GDALRasterBand *GetRasterSampleOverview(GUIntBig);
    virtual CPLErr BuildOverviews(const char *pszResampling, int nOverviews,
                                  const int *panOverviewList,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData,
                                  CSLConstList papszOptions);

    virtual CPLErr AdviseRead(int nXOff, int nYOff, int nXSize, int nYSize,
                              int nBufXSize, int nBufYSize,
                              GDALDataType eBufType, char **papszOptions);

    virtual CPLErr GetHistogram(double dfMin, double dfMax, int nBuckets,
                                GUIntBig *panHistogram, int bIncludeOutOfRange,
                                int bApproxOK, GDALProgressFunc,
                                void *pProgressData);

    virtual CPLErr GetDefaultHistogram(double *pdfMin, double *pdfMax,
                                       int *pnBuckets, GUIntBig **ppanHistogram,
                                       int bForce, GDALProgressFunc,
                                       void *pProgressData);
    virtual CPLErr SetDefaultHistogram(double dfMin, double dfMax, int nBuckets,
                                       GUIntBig *panHistogram);

    virtual GDALRasterAttributeTable *GetDefaultRAT();
    virtual CPLErr SetDefaultRAT(const GDALRasterAttributeTable *poRAT);

    virtual GDALRasterBand *GetMaskBand();
    virtual int GetMaskFlags();
    virtual CPLErr CreateMaskBand(int nFlagsIn);
    virtual bool IsMaskBand() const;
    virtual GDALMaskValueRange GetMaskValueRange() const;
    bool HasConflictingMaskSources(std::string *posDetailMessage = nullptr,
                                   bool bMentionPrioritarySource = true) const;

    virtual CPLVirtualMem *
    GetVirtualMemAuto(GDALRWFlag eRWFlag, int *pnPixelSpace,
                      GIntBig *pnLineSpace,
                      char **papszOptions) CPL_WARN_UNUSED_RESULT;

    int GetDataCoverageStatus(int nXOff, int nYOff, int nXSize, int nYSize,
                              int nMaskFlagStop = 0,
                              double *pdfDataPct = nullptr);

    std::shared_ptr<GDALMDArray> AsMDArray() const;

    CPLErr InterpolateAtGeolocation(
        double dfGeolocX, double dfGeolocY, const OGRSpatialReference *poSRS,
        GDALRIOResampleAlg eInterpolation, double *pdfRealValue,
        double *pdfImagValue = nullptr,
        CSLConstList papszTransformerOptions = nullptr) const;

    virtual CPLErr InterpolateAtPoint(double dfPixel, double dfLine,
                                      GDALRIOResampleAlg eInterpolation,
                                      double *pdfRealValue,
                                      double *pdfImagValue = nullptr) const;

    //! @cond Doxygen_Suppress
    class CPL_DLL WindowIterator
    {
      public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;

        using value_type = GDALRasterWindow;
        using pointer = value_type *;
        using reference = value_type &;

        WindowIterator(int nRasterXSize, int nRasterYSize, int nBlockXSize,
                       int nBlockYSize, int nRow, int nCol);

        bool operator==(const WindowIterator &other) const;

        bool operator!=(const WindowIterator &other) const;

        value_type operator*() const;

        WindowIterator &operator++();

      private:
        int m_nRasterXSize;
        int m_nRasterYSize;
        int m_nBlockXSize;
        int m_nBlockYSize;
        int m_row;
        int m_col;
    };

    class CPL_DLL WindowIteratorWrapper
    {
      public:
        explicit WindowIteratorWrapper(const GDALRasterBand &band,
                                       size_t maxSize = 0);

        explicit WindowIteratorWrapper(const GDALRasterBand &band1,
                                       const GDALRasterBand &band2,
                                       size_t maxSize = 0);

        uint64_t count() const;

        WindowIterator begin() const;

        WindowIterator end() const;

      private:
        const int m_nRasterXSize;
        const int m_nRasterYSize;
        int m_nBlockXSize;
        int m_nBlockYSize;

        WindowIteratorWrapper(int nRasterXSize, int nRasterYSize,
                              int nBlockXSize, int nBlockYSize, size_t maxSize);
    };

    //! @endcond

    WindowIteratorWrapper IterateWindows(size_t maxSize = 0) const;

    virtual bool MayMultiBlockReadingBeMultiThreaded() const;

#ifndef DOXYGEN_XML
    void ReportError(CPLErr eErrClass, CPLErrorNum err_no, const char *fmt,
                     ...) const CPL_PRINT_FUNC_FORMAT(4, 5);
#endif

    //! @cond Doxygen_Suppress
    static void ThrowIfNotSameDimensions(const GDALRasterBand &first,
                                         const GDALRasterBand &second);

    //! @endcond

    /** Convert a GDALRasterBand* to a GDALRasterBandH.
     */
    static inline GDALRasterBandH ToHandle(GDALRasterBand *poBand)
    {
        return static_cast<GDALRasterBandH>(poBand);
    }

    /** Convert a GDALRasterBandH to a GDALRasterBand*.
     */
    static inline GDALRasterBand *FromHandle(GDALRasterBandH hBand)
    {
        return static_cast<GDALRasterBand *>(hBand);
    }

    //! @cond Doxygen_Suppress
    // Remove me in GDAL 4.0. See GetMetadataItem() implementation
    // Internal use in GDAL only !
    virtual void EnablePixelTypeSignedByteWarning(bool b)
#ifndef GDAL_COMPILATION
        CPL_WARN_DEPRECATED("Do not use that method outside of GDAL!")
#endif
            ;

    //! @endcond

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALRasterBand)
};

//! @cond Doxygen_Suppress
#define GDAL_EXTERN_TEMPLATE_READ_RASTER(T)                                    \
    extern template CPLErr GDALRasterBand::ReadRaster<T>(                      \
        T * pData, size_t nArrayEltCount, double dfXOff, double dfYOff,        \
        double dfXSize, double dfYSize, size_t nBufXSize, size_t nBufYSize,    \
        GDALRIOResampleAlg eResampleAlg, GDALProgressFunc pfnProgress,         \
        void *pProgressData) const;

GDAL_EXTERN_TEMPLATE_READ_RASTER(uint8_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER(int8_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER(uint16_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER(int16_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER(uint32_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER(int32_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER(uint64_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER(int64_t)
#ifdef CPL_FLOAT_H_INCLUDED
GDAL_EXTERN_TEMPLATE_READ_RASTER(GFloat16)
#endif
GDAL_EXTERN_TEMPLATE_READ_RASTER(float)
GDAL_EXTERN_TEMPLATE_READ_RASTER(double)
// Not allowed by C++ standard
// GDAL_EXTERN_TEMPLATE_READ_RASTER(std::complex<int16_t>)
// GDAL_EXTERN_TEMPLATE_READ_RASTER(std::complex<int32_t>)
GDAL_EXTERN_TEMPLATE_READ_RASTER(std::complex<float>)
GDAL_EXTERN_TEMPLATE_READ_RASTER(std::complex<double>)

#define GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(T)                             \
    extern template CPLErr GDALRasterBand::ReadRaster<T>(                      \
        std::vector<T> & vData, double dfXOff, double dfYOff, double dfXSize,  \
        double dfYSize, size_t nBufXSize, size_t nBufYSize,                    \
        GDALRIOResampleAlg eResampleAlg, GDALProgressFunc pfnProgress,         \
        void *pProgressData) const;

GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(uint8_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(int8_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(uint16_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(int16_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(uint32_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(int32_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(uint64_t)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(int64_t)
#ifdef CPL_FLOAT_H_INCLUDED
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(GFloat16)
#endif
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(float)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(double)
// Not allowed by C++ standard
// GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(std::complex<int16_t>)
// GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(std::complex<int32_t>)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(std::complex<float>)
GDAL_EXTERN_TEMPLATE_READ_RASTER_VECTOR(std::complex<double>)

//! @endcond

#include "gdal_computedrasterband.h"

#endif
