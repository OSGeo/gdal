/******************************************************************************
 *
 * Name:     gdal_dataset.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALDataset class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALDATASET_H_INCLUDED
#define GDALDATASET_H_INCLUDED

#include "cpl_port.h"
#include "gdal_fwd.h"
#include "gdal.h"
#include "gdal_geotransform.h"
#include "gdal_majorobject.h"
#include "gdal_defaultoverviews.h"
#include "ogr_core.h"     // for OGRErr
#include "ogr_feature.h"  // for OGRFeatureUniquePtr

#include <cstddef>
#include <iterator>
#include <memory>
#include <vector>

/* ******************************************************************** */
/*                             GDALDataset                              */
/* ******************************************************************** */

class OGRGeometry;
class OGRLayer;
class OGRSpatialReference;
class OGRStyleTable;
class swq_select;
class swq_select_parse_options;
class GDALAsyncReader;
class GDALDriver;
class GDALGroup;
class GDALMDArray;
class GDALRasterBand;
class GDALRelationship;
class GDALOpenInfo;

//! @cond Doxygen_Suppress
typedef struct GDALSQLParseInfo GDALSQLParseInfo;
//! @endcond

//! @cond Doxygen_Suppress
#if !defined(OPTIONAL_OUTSIDE_GDAL)
#if defined(GDAL_COMPILATION)
#define OPTIONAL_OUTSIDE_GDAL(val)
#else
#define OPTIONAL_OUTSIDE_GDAL(val) = val
#endif
#endif
//! @endcond

//! @cond Doxygen_Suppress
// This macro can be defined to check that GDALDataset::IRasterIO()
// implementations do not alter the passed panBandList. It is not defined
// by default (and should not!), hence int* is used.
#if defined(GDAL_BANDMAP_TYPE_CONST_SAFE)
#define BANDMAP_TYPE const int *
#else
#define BANDMAP_TYPE int *
#endif
//! @endcond

/** A set of associated raster bands, usually from one file. */
class CPL_DLL GDALDataset : public GDALMajorObject
{
    friend GDALDatasetH CPL_STDCALL
    GDALOpenEx(const char *pszFilename, unsigned int nOpenFlags,
               const char *const *papszAllowedDrivers,
               const char *const *papszOpenOptions,
               const char *const *papszSiblingFiles);
    friend CPLErr CPL_STDCALL GDALClose(GDALDatasetH hDS);

    friend class GDALDriver;
    friend class GDALDefaultOverviews;
    friend class GDALProxyDataset;
    friend class GDALDriverManager;

    CPL_INTERNAL void AddToDatasetOpenList();

    CPL_INTERNAL void UnregisterFromSharedDataset();

    CPL_INTERNAL static void ReportErrorV(const char *pszDSName,
                                          CPLErr eErrClass, CPLErrorNum err_no,
                                          const char *fmt, va_list args);

  protected:
    //! @cond Doxygen_Suppress
    GDALDriver *poDriver = nullptr;
    GDALAccess eAccess = GA_ReadOnly;

    // Stored raster information.
    int nRasterXSize = 512;
    int nRasterYSize = 512;
    int nBands = 0;
    GDALRasterBand **papoBands = nullptr;

    static constexpr int OPEN_FLAGS_CLOSED = -1;
    int nOpenFlags =
        0;  // set to OPEN_FLAGS_CLOSED after Close() has been called

    int nRefCount = 1;
    bool bForceCachedIO = false;
    bool bShared = false;
    bool bIsInternal = true;
    bool bSuppressOnClose = false;

    mutable std::map<std::string, std::unique_ptr<OGRFieldDomain>>
        m_oMapFieldDomains{};

    GDALDataset(void);
    explicit GDALDataset(int bForceCachedIO);

    void RasterInitialize(int, int);
    void SetBand(int nNewBand, GDALRasterBand *poBand);
    void SetBand(int nNewBand, std::unique_ptr<GDALRasterBand> poBand);

    GDALDefaultOverviews oOvManager{};

    virtual CPLErr IBuildOverviews(const char *pszResampling, int nOverviews,
                                   const int *panOverviewList, int nListBands,
                                   const int *panBandList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions);

    virtual CPLErr
    IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
              void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
              int nBandCount, BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
              GSpacing nLineSpace, GSpacing nBandSpace,
              GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;

    /* This method should only be be overloaded by GDALProxyDataset */
    virtual CPLErr
    BlockBasedRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                       int nYSize, void *pData, int nBufXSize, int nBufYSize,
                       GDALDataType eBufType, int nBandCount,
                       const int *panBandMap, GSpacing nPixelSpace,
                       GSpacing nLineSpace, GSpacing nBandSpace,
                       GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;
    CPLErr BlockBasedFlushCache(bool bAtClosing);

    CPLErr
    BandBasedRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                      int nYSize, void *pData, int nBufXSize, int nBufYSize,
                      GDALDataType eBufType, int nBandCount,
                      const int *panBandMap, GSpacing nPixelSpace,
                      GSpacing nLineSpace, GSpacing nBandSpace,
                      GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;

    CPLErr
    RasterIOResampled(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                      int nYSize, void *pData, int nBufXSize, int nBufYSize,
                      GDALDataType eBufType, int nBandCount,
                      const int *panBandMap, GSpacing nPixelSpace,
                      GSpacing nLineSpace, GSpacing nBandSpace,
                      GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;

    CPLErr ValidateRasterIOOrAdviseReadParameters(
        const char *pszCallingFunc, int *pbStopProcessingOnCENone, int nXOff,
        int nYOff, int nXSize, int nYSize, int nBufXSize, int nBufYSize,
        int nBandCount, const int *panBandMap);

    CPLErr TryOverviewRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                               int nXSize, int nYSize, void *pData,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, int nBandCount,
                               const int *panBandMap, GSpacing nPixelSpace,
                               GSpacing nLineSpace, GSpacing nBandSpace,
                               GDALRasterIOExtraArg *psExtraArg, int *pbTried);

    void ShareLockWithParentDataset(GDALDataset *poParentDataset);

    bool m_bCanBeReopened = false;

    virtual bool CanBeCloned(int nScopeFlags, bool bCanShareState) const;

    friend class GDALThreadSafeDataset;
    friend class MEMDataset;
    virtual std::unique_ptr<GDALDataset> Clone(int nScopeFlags,
                                               bool bCanShareState) const;

    //! @endcond

    void CleanupPostFileClosing();

    virtual int CloseDependentDatasets();
    //! @cond Doxygen_Suppress
    int ValidateLayerCreationOptions(const char *const *papszLCO);

    char **papszOpenOptions = nullptr;

    friend class GDALRasterBand;

    // The below methods related to read write mutex are fragile logic, and
    // should not be used by out-of-tree code if possible.
    int EnterReadWrite(GDALRWFlag eRWFlag);
    void LeaveReadWrite();
    void InitRWLock();

    void TemporarilyDropReadWriteLock();
    void ReacquireReadWriteLock();

    void DisableReadWriteMutex();

    int AcquireMutex();
    void ReleaseMutex();

    bool IsAllBands(int nBandCount, const int *panBandList) const;
    //! @endcond

  public:
    ~GDALDataset() override;

    virtual CPLErr Close(GDALProgressFunc pfnProgress = nullptr,
                         void *pProgressData = nullptr);

    virtual bool GetCloseReportsProgress() const;

    virtual bool CanReopenWithCurrentDescription() const;

    int GetRasterXSize() const;
    int GetRasterYSize() const;
    int GetRasterCount() const;
    GDALRasterBand *GetRasterBand(int);
    const GDALRasterBand *GetRasterBand(int) const;

    /**
     * @brief SetQueryLoggerFunc
     * @param pfnQueryLoggerFuncIn query logger function callback
     * @param poQueryLoggerArgIn arguments passed to the query logger function
     * @return true on success
     */
    virtual bool SetQueryLoggerFunc(GDALQueryLoggerFunc pfnQueryLoggerFuncIn,
                                    void *poQueryLoggerArgIn);

    /** Class returned by GetBands() that act as a container for raster bands.
     */
    class CPL_DLL Bands
    {
      private:
        friend class GDALDataset;
        GDALDataset *m_poSelf;

        CPL_INTERNAL explicit Bands(GDALDataset *poSelf) : m_poSelf(poSelf)
        {
        }

        class CPL_DLL Iterator
        {
            struct Private;
            std::unique_ptr<Private> m_poPrivate;

          public:
            Iterator(GDALDataset *poDS, bool bStart);
            Iterator(const Iterator &oOther);  // declared but not defined.
                                               // Needed for gcc 5.4 at least
            Iterator(Iterator &&oOther) noexcept;  // declared but not defined.
                // Needed for gcc 5.4 at least
            ~Iterator();
            GDALRasterBand *operator*();
            Iterator &operator++();
            bool operator!=(const Iterator &it) const;
        };

      public:
        const Iterator begin() const;

        const Iterator end() const;

        size_t size() const;

        GDALRasterBand *operator[](int iBand);
        GDALRasterBand *operator[](size_t iBand);
    };

    Bands GetBands();

    /** Class returned by GetBands() that act as a container for raster bands.
     */
    class CPL_DLL ConstBands
    {
      private:
        friend class GDALDataset;
        const GDALDataset *const m_poSelf;

        CPL_INTERNAL explicit ConstBands(const GDALDataset *poSelf)
            : m_poSelf(poSelf)
        {
        }

        class CPL_DLL Iterator
        {
            struct Private;
            std::unique_ptr<Private> m_poPrivate;

          public:
            Iterator(const GDALDataset *poDS, bool bStart);
            ~Iterator();
            const GDALRasterBand *operator*() const;
            Iterator &operator++();
            bool operator!=(const Iterator &it) const;
        };

      public:
        const Iterator begin() const;

        const Iterator end() const;

        size_t size() const;

        const GDALRasterBand *operator[](int iBand) const;
        const GDALRasterBand *operator[](size_t iBand) const;
    };

    ConstBands GetBands() const;

    virtual CPLErr FlushCache(bool bAtClosing = false);
    virtual CPLErr DropCache();

    virtual GIntBig GetEstimatedRAMUsage();

    virtual const OGRSpatialReference *GetSpatialRef() const;
    virtual CPLErr SetSpatialRef(const OGRSpatialReference *poSRS);

    virtual const OGRSpatialReference *GetSpatialRefRasterOnly() const;
    virtual const OGRSpatialReference *GetSpatialRefVectorOnly() const;

    // Compatibility layer
    const char *GetProjectionRef(void) const;
    CPLErr SetProjection(const char *pszProjection);

    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const;
    virtual CPLErr SetGeoTransform(const GDALGeoTransform &gt);

    CPLErr GetGeoTransform(double *padfGeoTransform) const
#if defined(GDAL_COMPILATION) && !defined(DOXYGEN_XML)
        CPL_WARN_DEPRECATED("Use GetGeoTransform(GDALGeoTransform&) instead")
#endif
            ;

    CPLErr SetGeoTransform(const double *padfGeoTransform)
#if defined(GDAL_COMPILATION) && !defined(DOXYGEN_XML)
        CPL_WARN_DEPRECATED(
            "Use SetGeoTransform(const GDALGeoTransform&) instead")
#endif
            ;

    virtual CPLErr GetExtent(OGREnvelope *psExtent,
                             const OGRSpatialReference *poCRS = nullptr) const;
    virtual CPLErr GetExtentWGS84LongLat(OGREnvelope *psExtent) const;

    CPLErr GeolocationToPixelLine(
        double dfGeolocX, double dfGeolocY, const OGRSpatialReference *poSRS,
        double *pdfPixel, double *pdfLine,
        CSLConstList papszTransformerOptions = nullptr) const;

    virtual CPLErr AddBand(GDALDataType eType, char **papszOptions = nullptr);

    virtual void *GetInternalHandle(const char *pszHandleName);
    virtual GDALDriver *GetDriver(void);
    virtual char **GetFileList(void);

    const char *GetDriverName() const;

    virtual const OGRSpatialReference *GetGCPSpatialRef() const;
    virtual int GetGCPCount();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs(int nGCPCount, const GDAL_GCP *pasGCPList,
                           const OGRSpatialReference *poGCP_SRS);

    // Compatibility layer
    const char *GetGCPProjection() const;
    CPLErr SetGCPs(int nGCPCount, const GDAL_GCP *pasGCPList,
                   const char *pszGCPProjection);

    virtual CPLErr AdviseRead(int nXOff, int nYOff, int nXSize, int nYSize,
                              int nBufXSize, int nBufYSize, GDALDataType eDT,
                              int nBandCount, int *panBandList,
                              char **papszOptions);

    virtual CPLErr CreateMaskBand(int nFlagsIn);

    virtual GDALAsyncReader *
    BeginAsyncReader(int nXOff, int nYOff, int nXSize, int nYSize, void *pBuf,
                     int nBufXSize, int nBufYSize, GDALDataType eBufType,
                     int nBandCount, int *panBandMap, int nPixelSpace,
                     int nLineSpace, int nBandSpace, char **papszOptions);
    virtual void EndAsyncReader(GDALAsyncReader *poARIO);

    //! @cond Doxygen_Suppress
    struct RawBinaryLayout
    {
        enum class Interleaving
        {
            UNKNOWN,
            BIP,
            BIL,
            BSQ
        };
        std::string osRawFilename{};
        Interleaving eInterleaving = Interleaving::UNKNOWN;
        GDALDataType eDataType = GDT_Unknown;
        bool bLittleEndianOrder = false;

        vsi_l_offset nImageOffset = 0;
        GIntBig nPixelOffset = 0;
        GIntBig nLineOffset = 0;
        GIntBig nBandOffset = 0;
    };

    virtual bool GetRawBinaryLayout(RawBinaryLayout &);
    //! @endcond

#ifndef DOXYGEN_SKIP
    CPLErr RasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                    int nYSize, void *pData, int nBufXSize, int nBufYSize,
                    GDALDataType eBufType, int nBandCount,
                    const int *panBandMap, GSpacing nPixelSpace,
                    GSpacing nLineSpace, GSpacing nBandSpace,
                    GDALRasterIOExtraArg *psExtraArg
                        OPTIONAL_OUTSIDE_GDAL(nullptr)) CPL_WARN_UNUSED_RESULT;
#else
    CPLErr RasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                    int nYSize, void *pData, int nBufXSize, int nBufYSize,
                    GDALDataType eBufType, int nBandCount,
                    const int *panBandMap, GSpacing nPixelSpace,
                    GSpacing nLineSpace, GSpacing nBandSpace,
                    GDALRasterIOExtraArg *psExtraArg) CPL_WARN_UNUSED_RESULT;
#endif

    virtual CPLStringList GetCompressionFormats(int nXOff, int nYOff,
                                                int nXSize, int nYSize,
                                                int nBandCount,
                                                const int *panBandList);
    virtual CPLErr ReadCompressedData(const char *pszFormat, int nXOff,
                                      int nYOff, int nXSize, int nYSize,
                                      int nBands, const int *panBandList,
                                      void **ppBuffer, size_t *pnBufferSize,
                                      char **ppszDetailedFormat);

    int Reference();
    int Dereference();
    int ReleaseRef();

    /** Return access mode.
     * @return access mode.
     */
    GDALAccess GetAccess() const
    {
        return eAccess;
    }

    int GetShared() const;
    void MarkAsShared();

    void MarkSuppressOnClose();
    void UnMarkSuppressOnClose();

    /** Return MarkSuppressOnClose flag.
    * @return MarkSuppressOnClose flag.
    */
    bool IsMarkedSuppressOnClose() const
    {
        return bSuppressOnClose;
    }

    /** Return open options.
     * @return open options.
     */
    char **GetOpenOptions()
    {
        return papszOpenOptions;
    }

    bool IsThreadSafe(int nScopeFlags) const;

#ifndef DOXYGEN_SKIP
    /** Return open options.
     * @return open options.
     */
    CSLConstList GetOpenOptions() const
    {
        return papszOpenOptions;
    }
#endif

    static GDALDataset **GetOpenDatasets(int *pnDatasetCount);

#ifndef DOXYGEN_SKIP
    CPLErr
    BuildOverviews(const char *pszResampling, int nOverviews,
                   const int *panOverviewList, int nListBands,
                   const int *panBandList, GDALProgressFunc pfnProgress,
                   void *pProgressData,
                   CSLConstList papszOptions OPTIONAL_OUTSIDE_GDAL(nullptr));
#else
    CPLErr BuildOverviews(const char *pszResampling, int nOverviews,
                          const int *panOverviewList, int nListBands,
                          const int *panBandList, GDALProgressFunc pfnProgress,
                          void *pProgressData, CSLConstList papszOptions);
#endif

    virtual CPLErr AddOverviews(const std::vector<GDALDataset *> &apoSrcOvrDS,
                                GDALProgressFunc pfnProgress,
                                void *pProgressData, CSLConstList papszOptions);

    CPLErr GetInterBandCovarianceMatrix(
        double *padfCovMatrix, size_t nSize, int nBandCount = 0,
        const int *panBandList = nullptr, bool bApproxOK = false,
        bool bForce = false, bool bWriteIntoMetadata = true,
        int nDeltaDegreeOfFreedom = 1, GDALProgressFunc pfnProgress = nullptr,
        void *pProgressData = nullptr);

    std::vector<double> GetInterBandCovarianceMatrix(
        int nBandCount = 0, const int *panBandList = nullptr,
        bool bApproxOK = false, bool bForce = false,
        bool bWriteIntoMetadata = true, int nDeltaDegreeOfFreedom = 1,
        GDALProgressFunc pfnProgress = nullptr, void *pProgressData = nullptr);

    CPLErr ComputeInterBandCovarianceMatrix(
        double *padfCovMatrix, size_t nSize, int nBandCount = 0,
        const int *panBandList = nullptr, bool bApproxOK = false,
        bool bWriteIntoMetadata = true, int nDeltaDegreeOfFreedom = 1,
        GDALProgressFunc pfnProgress = nullptr, void *pProgressData = nullptr);

    std::vector<double> ComputeInterBandCovarianceMatrix(
        int nBandCount = 0, const int *panBandList = nullptr,
        bool bApproxOK = false, bool bWriteIntoMetadata = true,
        int nDeltaDegreeOfFreedom = 1, GDALProgressFunc pfnProgress = nullptr,
        void *pProgressData = nullptr);

#ifndef DOXYGEN_XML
    void ReportError(CPLErr eErrClass, CPLErrorNum err_no, const char *fmt,
                     ...) const CPL_PRINT_FUNC_FORMAT(4, 5);

    static void ReportError(const char *pszDSName, CPLErr eErrClass,
                            CPLErrorNum err_no, const char *fmt, ...)
        CPL_PRINT_FUNC_FORMAT(4, 5);
#endif

    CSLConstList GetMetadata(const char *pszDomain = "") override;

// Only defined when Doxygen enabled
#ifdef DOXYGEN_SKIP
    CPLErr SetMetadata(char **papszMetadata, const char *pszDomain) override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain) override;
#endif

    char **GetMetadataDomainList() override;

    virtual void ClearStatistics();

    std::shared_ptr<GDALMDArray> AsMDArray(CSLConstList papszOptions = nullptr);

    /** Convert a GDALDataset* to a GDALDatasetH.
     */
    static inline GDALDatasetH ToHandle(GDALDataset *poDS)
    {
        return static_cast<GDALDatasetH>(poDS);
    }

    /** Convert a GDALDatasetH to a GDALDataset*.
     */
    static inline GDALDataset *FromHandle(GDALDatasetH hDS)
    {
        return static_cast<GDALDataset *>(hDS);
    }

    /** @see GDALOpenEx().
     */
    static GDALDataset *Open(const char *pszFilename,
                             unsigned int nOpenFlags = 0,
                             const char *const *papszAllowedDrivers = nullptr,
                             const char *const *papszOpenOptions = nullptr,
                             const char *const *papszSiblingFiles = nullptr)
    {
        return FromHandle(GDALOpenEx(pszFilename, nOpenFlags,
                                     papszAllowedDrivers, papszOpenOptions,
                                     papszSiblingFiles));
    }

    static std::unique_ptr<GDALDataset>
    Open(GDALOpenInfo *poOpenInfo,
         const char *const *papszAllowedDrivers = nullptr,
         const char *const *papszOpenOptions = nullptr);

    /** Object returned by GetFeatures() iterators */
    struct FeatureLayerPair
    {
        /** Unique pointer to a OGRFeature. */
        OGRFeatureUniquePtr feature{};

        /** Layer to which the feature belongs to. */
        OGRLayer *layer = nullptr;
    };

    //! @cond Doxygen_Suppress
    // SetEnableOverviews() only to be used by GDALOverviewDataset
    void SetEnableOverviews(bool bEnable);

    // Only to be used by driver's GetOverviewCount() method.
    bool AreOverviewsEnabled() const;

    static void ReportUpdateNotSupportedByDriver(const char *pszDriverName);
    //! @endcond

  private:
    class Private;
    Private *m_poPrivate;

    CPL_INTERNAL OGRLayer *BuildLayerFromSelectInfo(
        swq_select *psSelectInfo, OGRGeometry *poSpatialFilter,
        const char *pszDialect, swq_select_parse_options *poSelectParseOptions);
    CPLStringList oDerivedMetadataList{};

  public:
    virtual int GetLayerCount() const;
    virtual const OGRLayer *GetLayer(int iLayer) const;

    OGRLayer *GetLayer(int iLayer)
    {
        return const_cast<OGRLayer *>(
            const_cast<const GDALDataset *>(this)->GetLayer(iLayer));
    }

    virtual bool IsLayerPrivate(int iLayer) const;

    /** Class returned by GetLayers() that acts as a range of layers.
     */
    class CPL_DLL Layers
    {
      private:
        friend class GDALDataset;
        GDALDataset *m_poSelf;

        CPL_INTERNAL explicit Layers(GDALDataset *poSelf) : m_poSelf(poSelf)
        {
        }

      public:
        /** Layer iterator.
         */
        class CPL_DLL Iterator
        {
            struct Private;
            std::unique_ptr<Private> m_poPrivate;

          public:
            using value_type = OGRLayer *; /**< value_type */
            using reference = OGRLayer *;  /**< reference */
            using difference_type = void;  /**< difference_type */
            using pointer = void;          /**< pointer */
            using iterator_category =
                std::input_iterator_tag; /**< iterator_category */

            Iterator(); /**< Default constructor */
            Iterator(GDALDataset *poDS, bool bStart); /**< Constructor */
            Iterator(const Iterator &oOther);         /**< Copy constructor */
            Iterator(Iterator &&oOther) noexcept;     /**< Move constructor */
            ~Iterator();                              /**< Destructor */

            Iterator &
            operator=(const Iterator &oOther); /**< Assignment operator */
            Iterator &operator=(
                Iterator &&oOther) noexcept; /**< Move assignment operator */

            value_type operator*() const; /**< Dereference operator */
            Iterator &operator++();       /**< Pre-increment operator */
            Iterator operator++(int);     /**< Post-increment operator */
            bool operator!=(const Iterator &it)
                const; /**< Difference comparison operator */
        };

        Iterator begin() const;
        Iterator end() const;

        size_t size() const;

        OGRLayer *operator[](int iLayer);
        OGRLayer *operator[](size_t iLayer);
        OGRLayer *operator[](const char *pszLayername);
    };

    Layers GetLayers();

    /** Class returned by GetLayers() that acts as a range of layers.
     * @since GDAL 3.12
     */
    class CPL_DLL ConstLayers
    {
      private:
        friend class GDALDataset;
        const GDALDataset *m_poSelf;

        CPL_INTERNAL explicit ConstLayers(const GDALDataset *poSelf)
            : m_poSelf(poSelf)
        {
        }

      public:
        /** Layer iterator.
         * @since GDAL 3.12
         */
        class CPL_DLL Iterator
        {
            struct Private;
            std::unique_ptr<Private> m_poPrivate;

          public:
            using value_type = const OGRLayer *; /**< value_type */
            using reference = const OGRLayer *;  /**< reference */
            using difference_type = void;        /**< difference_type */
            using pointer = void;                /**< pointer */
            using iterator_category =
                std::input_iterator_tag; /**< iterator_category */

            Iterator(); /**< Default constructor */
            Iterator(const GDALDataset *poDS, bool bStart); /**< Constructor */
            Iterator(const Iterator &oOther);     /**< Copy constructor */
            Iterator(Iterator &&oOther) noexcept; /**< Move constructor */
            ~Iterator();                          /**< Destructor */

            Iterator &
            operator=(const Iterator &oOther); /**< Assignment operator */
            Iterator &operator=(
                Iterator &&oOther) noexcept; /**< Move assignment operator */

            value_type operator*() const; /**< Dereference operator */
            Iterator &operator++();       /**< Pre-increment operator */
            Iterator operator++(int);     /**< Post-increment operator */
            bool operator!=(const Iterator &it)
                const; /**< Difference comparison operator */
        };

        Iterator begin() const;
        Iterator end() const;

        size_t size() const;

        const OGRLayer *operator[](int iLayer);
        const OGRLayer *operator[](size_t iLayer);
        const OGRLayer *operator[](const char *pszLayername);
    };

    ConstLayers GetLayers() const;

    virtual OGRLayer *GetLayerByName(const char *);

    int GetLayerIndex(const char *pszName) const;

    virtual OGRErr DeleteLayer(int iLayer);

    virtual void ResetReading();
    virtual OGRFeature *GetNextFeature(OGRLayer **ppoBelongingLayer,
                                       double *pdfProgressPct,
                                       GDALProgressFunc pfnProgress,
                                       void *pProgressData);

    /** Class returned by GetFeatures() that act as a container for vector
     * features. */
    class CPL_DLL Features
    {
      private:
        friend class GDALDataset;
        GDALDataset *m_poSelf;

        CPL_INTERNAL explicit Features(GDALDataset *poSelf) : m_poSelf(poSelf)
        {
        }

        class CPL_DLL Iterator
        {
            struct Private;
            std::unique_ptr<Private> m_poPrivate;

          public:
            Iterator(GDALDataset *poDS, bool bStart);
            Iterator(const Iterator &oOther);  // declared but not defined.
                                               // Needed for gcc 5.4 at least
            Iterator(Iterator &&oOther) noexcept;  // declared but not defined.
                // Needed for gcc 5.4 at least
            ~Iterator();
            const FeatureLayerPair &operator*() const;
            Iterator &operator++();
            bool operator!=(const Iterator &it) const;
        };

      public:
        const Iterator begin() const;

        const Iterator end() const;
    };

    Features GetFeatures();

    virtual int TestCapability(const char *) const;

    virtual std::vector<std::string>
    GetFieldDomainNames(CSLConstList papszOptions = nullptr) const;

    virtual const OGRFieldDomain *GetFieldDomain(const std::string &name) const;

    virtual bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                                std::string &failureReason);

    virtual bool DeleteFieldDomain(const std::string &name,
                                   std::string &failureReason);

    virtual bool UpdateFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                                   std::string &failureReason);

    virtual std::vector<std::string>
    GetRelationshipNames(CSLConstList papszOptions = nullptr) const;

    virtual const GDALRelationship *
    GetRelationship(const std::string &name) const;

    virtual bool
    AddRelationship(std::unique_ptr<GDALRelationship> &&relationship,
                    std::string &failureReason);

    virtual bool DeleteRelationship(const std::string &name,
                                    std::string &failureReason);

    virtual bool
    UpdateRelationship(std::unique_ptr<GDALRelationship> &&relationship,
                       std::string &failureReason);

    //! @cond Doxygen_Suppress
    OGRLayer *CreateLayer(const char *pszName);

    OGRLayer *CreateLayer(const char *pszName, std::nullptr_t);
    //! @endcond

    OGRLayer *CreateLayer(const char *pszName,
                          const OGRSpatialReference *poSpatialRef,
                          OGRwkbGeometryType eGType = wkbUnknown,
                          CSLConstList papszOptions = nullptr);

    OGRLayer *CreateLayer(const char *pszName,
                          const OGRGeomFieldDefn *poGeomFieldDefn,
                          CSLConstList papszOptions = nullptr);

    virtual OGRLayer *CopyLayer(OGRLayer *poSrcLayer, const char *pszNewName,
                                char **papszOptions = nullptr);

    virtual OGRStyleTable *GetStyleTable();
    virtual void SetStyleTableDirectly(OGRStyleTable *poStyleTable);

    virtual void SetStyleTable(OGRStyleTable *poStyleTable);

    virtual OGRLayer *ExecuteSQL(const char *pszStatement,
                                 OGRGeometry *poSpatialFilter,
                                 const char *pszDialect);
    virtual void ReleaseResultSet(OGRLayer *poResultsSet);
    virtual OGRErr AbortSQL();

    int GetRefCount() const;
    int GetSummaryRefCount() const;
    OGRErr Release();

    virtual OGRErr StartTransaction(int bForce = FALSE);
    virtual OGRErr CommitTransaction();
    virtual OGRErr RollbackTransaction();

    virtual std::shared_ptr<GDALGroup> GetRootGroup() const;

    static std::string BuildFilename(const char *pszFilename,
                                     const char *pszReferencePath,
                                     bool bRelativeToReferencePath);

    //! @cond Doxygen_Suppress
    static int IsGenericSQLDialect(const char *pszDialect);

    // Semi-public methods. Only to be used by in-tree drivers.
    GDALSQLParseInfo *
    BuildParseInfo(swq_select *psSelectInfo,
                   swq_select_parse_options *poSelectParseOptions);
    static void DestroyParseInfo(GDALSQLParseInfo *psParseInfo);
    OGRLayer *ExecuteSQL(const char *pszStatement, OGRGeometry *poSpatialFilter,
                         const char *pszDialect,
                         swq_select_parse_options *poSelectParseOptions);

    static constexpr const char *const apszSpecialSubDatasetSyntax[] = {
        "NITF_IM:{ANY}:{FILENAME}", "PDF:{ANY}:{FILENAME}",
        "RASTERLITE:{FILENAME},{ANY}", "TILEDB:\"{FILENAME}\":{ANY}",
        "TILEDB:{FILENAME}:{ANY}"};

    //! @endcond

  protected:
    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions);

    //! @cond Doxygen_Suppress
    OGRErr ProcessSQLCreateIndex(const char *);
    OGRErr ProcessSQLDropIndex(const char *);
    OGRErr ProcessSQLDropTable(const char *);
    OGRErr ProcessSQLAlterTableAddColumn(const char *);
    OGRErr ProcessSQLAlterTableDropColumn(const char *);
    OGRErr ProcessSQLAlterTableAlterColumn(const char *);
    OGRErr ProcessSQLAlterTableRenameColumn(const char *);

    OGRStyleTable *m_poStyleTable = nullptr;

    friend class GDALProxyPoolDataset;
    //! @endcond

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALDataset)
};

//! @cond Doxygen_Suppress
struct CPL_DLL GDALDatasetUniquePtrDeleter
{
    void operator()(GDALDataset *poDataset) const
    {
        GDALClose(poDataset);
    }
};

//! @endcond

//! @cond Doxygen_Suppress
struct CPL_DLL GDALDatasetUniquePtrReleaser
{
    void operator()(GDALDataset *poDataset) const
    {
        if (poDataset)
            poDataset->Release();
    }
};

//! @endcond

/** Unique pointer type for GDALDataset.
 * Appropriate for use on datasets open in non-shared mode and onto which
 * reference counter has not been manually modified.
 */
using GDALDatasetUniquePtr =
    std::unique_ptr<GDALDataset, GDALDatasetUniquePtrDeleter>;

#endif
