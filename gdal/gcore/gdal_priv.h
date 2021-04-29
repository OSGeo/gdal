/******************************************************************************
 * $Id$
 *
 * Name:     gdal_priv.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GDAL_PRIV_H_INCLUDED
#define GDAL_PRIV_H_INCLUDED

/**
 * \file gdal_priv.h
 *
 * C++ GDAL entry points.
 */

/* -------------------------------------------------------------------- */
/*      Predeclare various classes before pulling in gdal.h, the        */
/*      public declarations.                                            */
/* -------------------------------------------------------------------- */
class GDALMajorObject;
class GDALDataset;
class GDALRasterBand;
class GDALDriver;
class GDALRasterAttributeTable;
class GDALProxyDataset;
class GDALProxyRasterBand;
class GDALAsyncReader;

/* -------------------------------------------------------------------- */
/*      Pull in the public declarations.  This gets the C apis, and     */
/*      also various constants.  However, we will still get to          */
/*      provide the real class definitions for the GDAL classes.        */
/* -------------------------------------------------------------------- */

#include "gdal.h"
#include "gdal_frmts.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include "cpl_atomic_ops.h"

#include <stdarg.h>

#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <vector>

#include "ogr_core.h"
#include "ogr_feature.h"

//! @cond Doxygen_Suppress
#define GMO_VALID                0x0001
#define GMO_IGNORE_UNIMPLEMENTED 0x0002
#define GMO_SUPPORT_MD           0x0004
#define GMO_SUPPORT_MDMD         0x0008
#define GMO_MD_DIRTY             0x0010
#define GMO_PAM_CLASS            0x0020
//! @endcond

/************************************************************************/
/*                       GDALMultiDomainMetadata                        */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALMultiDomainMetadata
{
private:
    char **papszDomainList;
    CPLStringList **papoMetadataLists;

public:
    GDALMultiDomainMetadata();
    ~GDALMultiDomainMetadata();

    int         XMLInit( CPLXMLNode *psMetadata, int bMerge );
    CPLXMLNode  *Serialize();

    char      **GetDomainList() { return papszDomainList; }

    char      **GetMetadata( const char * pszDomain = "" );
    CPLErr      SetMetadata( char ** papszMetadata,
                             const char * pszDomain = "" );
    const char *GetMetadataItem( const char * pszName,
                                 const char * pszDomain = "" );
    CPLErr      SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain = "" );

    void        Clear();

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALMultiDomainMetadata)
};
//! @endcond

/* ******************************************************************** */
/*                           GDALMajorObject                            */
/*                                                                      */
/*      Base class providing metadata, description and other            */
/*      services shared by major objects.                               */
/* ******************************************************************** */

/** Object with metadata. */
class CPL_DLL GDALMajorObject
{
  protected:
//! @cond Doxygen_Suppress
    int                 nFlags; // GMO_* flags.
    CPLString           sDescription{};
    GDALMultiDomainMetadata oMDMD{};

//! @endcond

    char               **BuildMetadataDomainList( char** papszList,
                                                  int bCheckNonEmpty, ... ) CPL_NULL_TERMINATED;
  public:
                        GDALMajorObject();
    virtual            ~GDALMajorObject();

    int                 GetMOFlags() const;
    void                SetMOFlags( int nFlagsIn );

    virtual const char *GetDescription() const;
    virtual void        SetDescription( const char * );

    virtual char      **GetMetadataDomainList();

    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" );

    /** Convert a GDALMajorObject* to a GDALMajorObjectH.
     * @since GDAL 2.3
     */
    static inline GDALMajorObjectH ToHandle(GDALMajorObject* poMajorObject)
        { return static_cast<GDALMajorObjectH>(poMajorObject); }

    /** Convert a GDALMajorObjectH to a GDALMajorObject*.
     * @since GDAL 2.3
     */
    static inline GDALMajorObject* FromHandle(GDALMajorObjectH hMajorObject)
        { return static_cast<GDALMajorObject*>(hMajorObject); }
};

/* ******************************************************************** */
/*                         GDALDefaultOverviews                         */
/* ******************************************************************** */

//! @cond Doxygen_Suppress
class CPL_DLL GDALDefaultOverviews
{
    friend class GDALDataset;

    GDALDataset *poDS;
    GDALDataset *poODS;

    CPLString   osOvrFilename{};

    bool        bOvrIsAux;

    bool        bCheckedForMask;
    bool        bOwnMaskDS;
    GDALDataset *poMaskDS;

    // For "overview datasets" we record base level info so we can
    // find our way back to get overview masks.
    GDALDataset *poBaseDS;

    // Stuff for deferred initialize/overviewscans.
    bool        bCheckedForOverviews;
    void        OverviewScan();
    char       *pszInitName;
    bool        bInitNameIsOVR;
    char      **papszInitSiblingFiles;

  public:
               GDALDefaultOverviews();
               ~GDALDefaultOverviews();

    void       Initialize( GDALDataset *poDSIn, const char *pszName = nullptr,
                           char **papszSiblingFiles = nullptr,
                           int bNameIsOVR = FALSE );

    void       TransferSiblingFiles( char** papszSiblingFiles );

    int        IsInitialized();

    int        CloseDependentDatasets();

    // Overview Related

    int        GetOverviewCount( int nBand );
    GDALRasterBand *GetOverview( int nBand, int iOverview );

    CPLErr     BuildOverviews( const char * pszBasename,
                               const char * pszResampling,
                               int nOverviews, int * panOverviewList,
                               int nBands, int * panBandList,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData );

    CPLErr     BuildOverviewsSubDataset( const char * pszPhysicalFile,
                                         const char * pszResampling,
                                         int nOverviews, int * panOverviewList,
                                         int nBands, int * panBandList,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData );

    CPLErr     CleanOverviews();

    // Mask Related

    CPLErr     CreateMaskBand( int nFlags, int nBand = -1 );
    GDALRasterBand *GetMaskBand( int nBand );
    int        GetMaskFlags( int nBand );

    int        HaveMaskFile( char **papszSiblings = nullptr,
                             const char *pszBasename = nullptr );

    char**     GetSiblingFiles() { return papszInitSiblingFiles; }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALDefaultOverviews)
};
//! @endcond

/* ******************************************************************** */
/*                             GDALOpenInfo                             */
/* ******************************************************************** */

/** Class for dataset open functions. */
class CPL_DLL GDALOpenInfo
{
    bool        bHasGotSiblingFiles;
    char        **papszSiblingFiles;
    int         nHeaderBytesTried;

  public:
                GDALOpenInfo( const char * pszFile, int nOpenFlagsIn,
                              const char * const * papszSiblingFiles = nullptr );
                ~GDALOpenInfo( void );

    /** Filename */
    char        *pszFilename;
    /** Open options */
    char**      papszOpenOptions;

    /** Access flag */
    GDALAccess  eAccess;
    /** Open flags */
    int         nOpenFlags;

    /** Whether stat()'ing the file was successful */
    int         bStatOK;
    /** Whether the file is a directory */
    int         bIsDirectory;

    /** Pointer to the file */
    VSILFILE   *fpL;

    /** Number of bytes in pabyHeader */
    int         nHeaderBytes;
    /** Buffer with first bytes of the file */
    GByte       *pabyHeader;

    /** Allowed drivers (NULL for all) */
    const char* const* papszAllowedDrivers;

    int         TryToIngest(int nBytes);
    char      **GetSiblingFiles();
    char      **StealSiblingFiles();
    bool        AreSiblingFilesLoaded() const;

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALOpenInfo)
};

/* ******************************************************************** */
/*                             GDALDataset                              */
/* ******************************************************************** */

class OGRLayer;
class OGRGeometry;
class OGRSpatialReference;
class OGRStyleTable;
class swq_select;
class swq_select_parse_options;
class GDALGroup;

//! @cond Doxygen_Suppress
typedef struct GDALSQLParseInfo GDALSQLParseInfo;
//! @endcond

//! @cond Doxygen_Suppress
#ifdef GDAL_COMPILATION
#define OPTIONAL_OUTSIDE_GDAL(val)
#else
#define OPTIONAL_OUTSIDE_GDAL(val) = val
#endif
//! @endcond

/** A set of associated raster bands, usually from one file. */
class CPL_DLL GDALDataset : public GDALMajorObject
{
    friend GDALDatasetH CPL_STDCALL GDALOpenEx( const char* pszFilename,
                                 unsigned int nOpenFlags,
                                 const char* const* papszAllowedDrivers,
                                 const char* const* papszOpenOptions,
                                 const char* const* papszSiblingFiles );
    friend void CPL_STDCALL GDALClose( GDALDatasetH hDS );

    friend class GDALDriver;
    friend class GDALDefaultOverviews;
    friend class GDALProxyDataset;
    friend class GDALDriverManager;

    CPL_INTERNAL void AddToDatasetOpenList();

    CPL_INTERNAL static void ReportErrorV(
                                     const char* pszDSName,
                                     CPLErr eErrClass, CPLErrorNum err_no,
                                     const char *fmt, va_list args);
  protected:
//! @cond Doxygen_Suppress
    GDALDriver  *poDriver = nullptr;
    GDALAccess  eAccess = GA_ReadOnly;

    // Stored raster information.
    int         nRasterXSize = 512;
    int         nRasterYSize = 512;
    int         nBands = 0;
    GDALRasterBand **papoBands = nullptr;

    int         nOpenFlags = 0;

    int         nRefCount = 1;
    bool        bForceCachedIO = false;
    bool        bShared = false;
    bool        bIsInternal = true;
    bool        bSuppressOnClose = false;

    mutable std::map<std::string, std::unique_ptr<OGRFieldDomain>> m_oMapFieldDomains{};

                GDALDataset(void);
    explicit    GDALDataset(int bForceCachedIO);

    void        RasterInitialize( int, int );
    void        SetBand( int, GDALRasterBand * );

    GDALDefaultOverviews oOvManager{};

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, GSpacing, GSpacing, GSpacing,
                              GDALRasterIOExtraArg* psExtraArg ) CPL_WARN_UNUSED_RESULT;

    CPLErr BlockBasedRasterIO( GDALRWFlag, int, int, int, int,
                               void *, int, int, GDALDataType,
                               int, int *, GSpacing, GSpacing, GSpacing,
                               GDALRasterIOExtraArg* psExtraArg ) CPL_WARN_UNUSED_RESULT;
    void   BlockBasedFlushCache();

    CPLErr BandBasedRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg ) CPL_WARN_UNUSED_RESULT;

    CPLErr RasterIOResampled( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg ) CPL_WARN_UNUSED_RESULT;

    CPLErr ValidateRasterIOOrAdviseReadParameters(
                               const char* pszCallingFunc,
                               int* pbStopProcessingOnCENone,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize,
                               int nBandCount, int *panBandMap);

    CPLErr TryOverviewRasterIO( GDALRWFlag eRWFlag,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void * pData, int nBufXSize, int nBufYSize,
                                GDALDataType eBufType,
                                int nBandCount, int *panBandMap,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GSpacing nBandSpace,
                                GDALRasterIOExtraArg* psExtraArg,
                                int* pbTried);

    void  ShareLockWithParentDataset(GDALDataset* poParentDataset);

//! @endcond
    virtual int         CloseDependentDatasets();
//! @cond Doxygen_Suppress
    int                 ValidateLayerCreationOptions( const char* const* papszLCO );

    char            **papszOpenOptions = nullptr;

    friend class GDALRasterBand;

    // The below methods related to read write mutex are fragile logic, and
    // should not be used by out-of-tree code if possible.
    int                 EnterReadWrite(GDALRWFlag eRWFlag);
    void                LeaveReadWrite();
    void                InitRWLock();

    void                TemporarilyDropReadWriteLock();
    void                ReacquireReadWriteLock();

    void                DisableReadWriteMutex();

    int          AcquireMutex();
    void         ReleaseMutex();
//! @endcond

  public:
     ~GDALDataset() override;

    int GetRasterXSize();
    int GetRasterYSize();
    int GetRasterCount();
    GDALRasterBand *GetRasterBand( int );

    /** Class returned by GetBands() that act as a container for raster bands. */
    class CPL_DLL Bands
    {
      private:

        friend class GDALDataset;
        GDALDataset* m_poSelf;
        CPL_INTERNAL explicit Bands(GDALDataset* poSelf): m_poSelf(poSelf) {}

        class CPL_DLL Iterator
        {
                struct Private;
                std::unique_ptr<Private> m_poPrivate;
            public:
                Iterator(GDALDataset* poDS, bool bStart);
                Iterator(const Iterator& oOther); // declared but not defined. Needed for gcc 5.4 at least
                Iterator(Iterator&& oOther) noexcept; // declared but not defined. Needed for gcc 5.4 at least
                ~Iterator();
                GDALRasterBand* operator*();
                Iterator& operator++();
                bool operator!=(const Iterator& it) const;
        };

      public:

        const Iterator begin() const;

        const Iterator end() const;

        size_t size() const;

        GDALRasterBand* operator[](int iBand);
        GDALRasterBand* operator[](size_t iBand);
    };

    Bands              GetBands();

    virtual void FlushCache(void);

    virtual const OGRSpatialReference* GetSpatialRef() const;
    virtual CPLErr SetSpatialRef(const OGRSpatialReference* poSRS);

    // Compatibility layer
    const char *GetProjectionRef(void) const;
    CPLErr SetProjection( const char * pszProjection );

    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual CPLErr SetGeoTransform( double * padfTransform );

    virtual CPLErr        AddBand( GDALDataType eType,
                                   char **papszOptions=nullptr );

    virtual void *GetInternalHandle( const char * pszHandleName );
    virtual GDALDriver *GetDriver(void);
    virtual char      **GetFileList(void);

    virtual     const char* GetDriverName();

    virtual const OGRSpatialReference* GetGCPSpatialRef() const;
    virtual int    GetGCPCount();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const OGRSpatialReference * poGCP_SRS );

    // Compatibility layer
    const char *GetGCPProjection();
    CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const char *pszGCPProjection );

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eDT,
                               int nBandCount, int *panBandList,
                               char **papszOptions );

    virtual CPLErr          CreateMaskBand( int nFlagsIn );

    virtual GDALAsyncReader*
        BeginAsyncReader(int nXOff, int nYOff, int nXSize, int nYSize,
                         void *pBuf, int nBufXSize, int nBufYSize,
                         GDALDataType eBufType,
                         int nBandCount, int* panBandMap,
                         int nPixelSpace, int nLineSpace, int nBandSpace,
                         char **papszOptions);
    virtual void EndAsyncReader(GDALAsyncReader *);

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
        std::string         osRawFilename{};
        Interleaving        eInterleaving = Interleaving::UNKNOWN;
        GDALDataType        eDataType = GDT_Unknown;
        bool                bLittleEndianOrder = false;

        vsi_l_offset        nImageOffset = 0;
        GIntBig             nPixelOffset = 0;
        GIntBig             nLineOffset = 0;
        GIntBig             nBandOffset = 0;
    };

    virtual bool GetRawBinaryLayout(RawBinaryLayout&);
//! @endcond

    CPLErr      RasterIO( GDALRWFlag, int, int, int, int,
                          void *, int, int, GDALDataType,
                          int, int *, GSpacing, GSpacing, GSpacing,
                          GDALRasterIOExtraArg* psExtraArg
#ifndef DOXYGEN_SKIP
                          OPTIONAL_OUTSIDE_GDAL(nullptr)
#endif
                          ) CPL_WARN_UNUSED_RESULT;

    int           Reference();
    int           Dereference();
    int           ReleaseRef();

    /** Return access mode.
     * @return access mode.
     */
    GDALAccess    GetAccess() const { return eAccess; }

    int           GetShared() const;
    void          MarkAsShared();

    /** Set that the dataset must be deleted on close. */
    void          MarkSuppressOnClose() { bSuppressOnClose = true; }

    /** Return open options.
     * @return open options.
     */
    char        **GetOpenOptions() { return papszOpenOptions; }

    static GDALDataset **GetOpenDatasets( int *pnDatasetCount );

    CPLErr BuildOverviews( const char *, int, int *,
                           int, int *, GDALProgressFunc, void * );

#ifndef DOXYGEN_XML
    void ReportError(CPLErr eErrClass, CPLErrorNum err_no, const char *fmt, ...)  CPL_PRINT_FUNC_FORMAT (4, 5);

    static void ReportError(const char* pszDSName,
                            CPLErr eErrClass, CPLErrorNum err_no,
                            const char *fmt, ...)  CPL_PRINT_FUNC_FORMAT (4, 5);
#endif

    char ** GetMetadata(const char * pszDomain = "") override;

// Only defined when Doxygen enabled
#ifdef DOXYGEN_SKIP
    CPLErr      SetMetadata( char ** papszMetadata,
                             const char * pszDomain ) override;
    CPLErr      SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain ) override;
#endif

    char **GetMetadataDomainList() override;

    virtual void ClearStatistics();

    /** Convert a GDALDataset* to a GDALDatasetH.
     * @since GDAL 2.3
     */
    static inline GDALDatasetH ToHandle(GDALDataset* poDS)
        { return static_cast<GDALDatasetH>(poDS); }

    /** Convert a GDALDatasetH to a GDALDataset*.
     * @since GDAL 2.3
     */
    static inline GDALDataset* FromHandle(GDALDatasetH hDS)
        { return static_cast<GDALDataset*>(hDS); }

    /** @see GDALOpenEx().
     * @since GDAL 2.3
     */
    static GDALDataset* Open( const char* pszFilename,
                              unsigned int nOpenFlags = 0,
                              const char* const* papszAllowedDrivers = nullptr,
                              const char* const* papszOpenOptions = nullptr,
                              const char* const* papszSiblingFiles = nullptr )
    {
        return FromHandle(GDALOpenEx(pszFilename, nOpenFlags,
                                      papszAllowedDrivers,
                                      papszOpenOptions,
                                      papszSiblingFiles));
    }

    /** Object returned by GetFeatures() iterators */
    struct FeatureLayerPair
    {
        /** Unique pointer to a OGRFeature. */
        OGRFeatureUniquePtr feature{};

        /** Layer to which the feature belongs to. */
        OGRLayer* layer = nullptr;
    };

//! @cond Doxygen_Suppress
    // SetEnableOverviews() only to be used by GDALOverviewDataset
    void SetEnableOverviews(bool bEnable);

    // Only to be used by driver's GetOverviewCount() method.
    bool AreOverviewsEnabled() const;
//! @endcond

private:
    class Private;
    Private *m_poPrivate;

    CPL_INTERNAL OGRLayer*       BuildLayerFromSelectInfo(swq_select* psSelectInfo,
                                             OGRGeometry *poSpatialFilter,
                                             const char *pszDialect,
                                             swq_select_parse_options* poSelectParseOptions);
    CPLStringList oDerivedMetadataList{};

  public:

    virtual int         GetLayerCount();
    virtual OGRLayer    *GetLayer(int iLayer);

    /** Class returned by GetLayers() that acts as a range of layers.
     * @since GDAL 2.3
     */
    class CPL_DLL Layers
    {
      private:

        friend class GDALDataset;
        GDALDataset* m_poSelf;
        CPL_INTERNAL explicit Layers(GDALDataset* poSelf): m_poSelf(poSelf) {}

      public:

        /** Layer iterator.
         * @since GDAL 2.3
         */
        class CPL_DLL Iterator
        {
                struct Private;
                std::unique_ptr<Private> m_poPrivate;
            public:

                using value_type = OGRLayer*; /**< value_type */
                using reference = OGRLayer*; /**< reference */
                using difference_type = void; /**< difference_type */
                using pointer = void; /**< pointer */
                using iterator_category = std::input_iterator_tag; /**< iterator_category */

                Iterator(); /**< Default constructor */
                Iterator(GDALDataset* poDS, bool bStart);  /**< Constructor */
                Iterator(const Iterator& oOther);  /**< Copy constructor */
                Iterator(Iterator&& oOther) noexcept;  /**< Move constructor */
                ~Iterator(); /**< Destructor */

                Iterator& operator=(const Iterator& oOther);  /**< Assignment operator */
                Iterator& operator=(Iterator&& oOther) noexcept; /**< Move assignment operator */

                OGRLayer* operator*() const; /**< Dereference operator */
                Iterator& operator++(); /**< Pre-increment operator */
                Iterator operator++(int); /**< Post-increment operator */
                bool operator!=(const Iterator& it) const; /**< Difference comparison operator */
        };

        Iterator begin() const;
        Iterator end() const;

        size_t size() const;

        OGRLayer* operator[](int iLayer);
        OGRLayer* operator[](size_t iLayer);
        OGRLayer* operator[](const char* pszLayername);
    };

    Layers              GetLayers();

    virtual OGRLayer    *GetLayerByName(const char *);
    virtual OGRErr      DeleteLayer(int iLayer);

    virtual void        ResetReading();
    virtual OGRFeature* GetNextFeature( OGRLayer** ppoBelongingLayer,
                                        double* pdfProgressPct,
                                        GDALProgressFunc pfnProgress,
                                        void* pProgressData );


    /** Class returned by GetFeatures() that act as a container for vector features. */
    class CPL_DLL Features
    {
      private:

        friend class GDALDataset;
        GDALDataset* m_poSelf;
        CPL_INTERNAL explicit Features(GDALDataset* poSelf): m_poSelf(poSelf) {}

        class CPL_DLL Iterator
        {
                struct Private;
                std::unique_ptr<Private> m_poPrivate;
            public:
                Iterator(GDALDataset* poDS, bool bStart);
                Iterator(const Iterator& oOther); // declared but not defined. Needed for gcc 5.4 at least
                Iterator(Iterator&& oOther) noexcept; // declared but not defined. Needed for gcc 5.4 at least
                ~Iterator();
                const FeatureLayerPair& operator*() const;
                Iterator& operator++();
                bool operator!=(const Iterator& it) const;
        };

      public:

        const Iterator begin() const;

        const Iterator end() const;
    };

    Features            GetFeatures();

    virtual int         TestCapability( const char * );

    virtual const OGRFieldDomain* GetFieldDomain(const std::string& name) const;

    virtual bool        AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                       std::string& failureReason);

    virtual OGRLayer   *CreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = nullptr,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = nullptr );
    virtual OGRLayer   *CopyLayer( OGRLayer *poSrcLayer,
                                   const char *pszNewName,
                                   char **papszOptions = nullptr );

    virtual OGRStyleTable *GetStyleTable();
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );

    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

    virtual OGRLayer *  ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poResultsSet );
    virtual OGRErr      AbortSQL( );

    int                 GetRefCount() const;
    int                 GetSummaryRefCount() const;
    OGRErr              Release();

    virtual OGRErr      StartTransaction(int bForce=FALSE);
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual std::shared_ptr<GDALGroup> GetRootGroup() const;

//! @cond Doxygen_Suppress
    static int          IsGenericSQLDialect(const char* pszDialect);

    // Semi-public methods. Only to be used by in-tree drivers.
    GDALSQLParseInfo*   BuildParseInfo(swq_select* psSelectInfo,
                                       swq_select_parse_options* poSelectParseOptions);
    static void         DestroyParseInfo(GDALSQLParseInfo* psParseInfo );
    OGRLayer *          ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect,
                                    swq_select_parse_options* poSelectParseOptions);
//! @endcond

  protected:
    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = nullptr,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = nullptr );

//! @cond Doxygen_Suppress
    OGRErr              ProcessSQLCreateIndex( const char * );
    OGRErr              ProcessSQLDropIndex( const char * );
    OGRErr              ProcessSQLDropTable( const char * );
    OGRErr              ProcessSQLAlterTableAddColumn( const char * );
    OGRErr              ProcessSQLAlterTableDropColumn( const char * );
    OGRErr              ProcessSQLAlterTableAlterColumn( const char * );
    OGRErr              ProcessSQLAlterTableRenameColumn( const char * );

    OGRStyleTable      *m_poStyleTable = nullptr;

    // Compatibility layers
    const OGRSpatialReference* GetSpatialRefFromOldGetProjectionRef() const;
    CPLErr OldSetProjectionFromSetSpatialRef(const OGRSpatialReference* poSRS);
    const OGRSpatialReference* GetGCPSpatialRefFromOldGetGCPProjection() const;
    CPLErr OldSetGCPsFromNew( int nGCPCount, const GDAL_GCP *pasGCPList,
                              const OGRSpatialReference * poGCP_SRS );

    friend class GDALProxyPoolDataset;
    virtual const char *_GetProjectionRef();
    const char *GetProjectionRefFromSpatialRef(const OGRSpatialReference*) const;
    virtual const char *_GetGCPProjection();
    const char *GetGCPProjectionFromSpatialRef(const OGRSpatialReference* poSRS) const;
    virtual CPLErr _SetProjection( const char * pszProjection );
    virtual CPLErr _SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const char *pszGCPProjection );
//! @endcond

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALDataset)
};

//! @cond Doxygen_Suppress
struct CPL_DLL GDALDatasetUniquePtrDeleter
{
    void operator()(GDALDataset* poDataset) const
        { GDALClose(poDataset); }
};
//! @endcond

/** Unique pointer type for GDALDataset.
 * Appropriate for use on datasets open in non-shared mode and onto which
 * reference counter has not been manually modified.
 * @since GDAL 2.3
 */
using GDALDatasetUniquePtr = std::unique_ptr<GDALDataset, GDALDatasetUniquePtrDeleter>;

/* ******************************************************************** */
/*                           GDALRasterBlock                            */
/* ******************************************************************** */

/** A single raster block in the block cache.
 *
 * And the global block manager that manages a least-recently-used list of
 * blocks from various datasets/bands */
class CPL_DLL GDALRasterBlock
{
    friend class GDALAbstractBandBlockCache;

    GDALDataType        eType;

    bool                bDirty;
    volatile int        nLockCount;

    int                 nXOff;
    int                 nYOff;

    int                 nXSize;
    int                 nYSize;

    void                *pData;

    GDALRasterBand      *poBand;

    GDALRasterBlock     *poNext;
    GDALRasterBlock     *poPrevious;

    bool                 bMustDetach;

    CPL_INTERNAL void        Detach_unlocked( void );
    CPL_INTERNAL void        Touch_unlocked( void );

    CPL_INTERNAL void        RecycleFor( int nXOffIn, int nYOffIn );

  public:
                GDALRasterBlock( GDALRasterBand *, int, int );
                GDALRasterBlock( int nXOffIn, int nYOffIn ); /* only for lookup purpose */
    virtual     ~GDALRasterBlock();

    CPLErr      Internalize( void );
    void        Touch( void );
    void        MarkDirty( void );
    void        MarkClean( void );
    /** Increment the lock count */
    int         AddLock( void ) { return CPLAtomicInc(&nLockCount); }
    /** Decrement the lock count */
    int         DropLock( void ) { return CPLAtomicDec(&nLockCount); }
    void        Detach();

    CPLErr      Write();

    /** Return the data type
     * @return data type
     */
    GDALDataType GetDataType() const { return eType; }
    /** Return the x offset of the top-left corner of the block
     * @return x offset
     */
    int         GetXOff() const { return nXOff; }
    /** Return the y offset of the top-left corner of the block
     * @return y offset
     */
    int         GetYOff() const { return nYOff; }
    /** Return the width of the block
     * @return width
     */
    int         GetXSize() const { return nXSize; }
    /** Return the height of the block
     * @return height
     */
    int         GetYSize() const { return nYSize; }
    /** Return the dirty flag
     * @return dirty flag
     */
    int         GetDirty() const { return bDirty; }
    /** Return the data buffer
     * @return data buffer
     */
    void        *GetDataRef( void ) { return pData; }
    /** Return the block size in bytes
     * @return block size.
     */
    GPtrDiff_t   GetBlockSize() const {
        return static_cast<GPtrDiff_t>(nXSize) * nYSize * GDALGetDataTypeSizeBytes(eType); }

    int          TakeLock();
    int          DropLockForRemovalFromStorage();

    /// @brief Accessor to source GDALRasterBand object.
    /// @return source raster band of the raster block.
    GDALRasterBand *GetBand() { return poBand; }

    static void FlushDirtyBlocks();
    static int  FlushCacheBlock(int bDirtyBlocksOnly = FALSE);
    static void Verify();

    static void EnterDisableDirtyBlockFlush();
    static void LeaveDisableDirtyBlockFlush();

#ifdef notdef
    static void CheckNonOrphanedBlocks(GDALRasterBand* poBand);
    void        DumpBlock();
    static void DumpAll();
#endif

    /* Should only be called by GDALDestroyDriverManager() */
//! @cond Doxygen_Suppress
    CPL_INTERNAL static void DestroyRBMutex();
//! @endcond

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALRasterBlock)
};

/* ******************************************************************** */
/*                             GDALColorTable                           */
/* ******************************************************************** */

/** A color table / palette. */

class CPL_DLL GDALColorTable
{
    GDALPaletteInterp eInterp;

    std::vector<GDALColorEntry> aoEntries{};

public:
    explicit     GDALColorTable( GDALPaletteInterp = GPI_RGB );
                ~GDALColorTable();

    GDALColorTable *Clone() const;
    int             IsSame(const GDALColorTable* poOtherCT) const;

    GDALPaletteInterp GetPaletteInterpretation() const;

    int           GetColorEntryCount() const;
    const GDALColorEntry *GetColorEntry( int ) const;
    int           GetColorEntryAsRGB( int, GDALColorEntry * ) const;
    void          SetColorEntry( int, const GDALColorEntry * );
    int           CreateColorRamp( int, const GDALColorEntry * ,
                                   int, const GDALColorEntry * );

    /** Convert a GDALColorTable* to a GDALRasterBandH.
     * @since GDAL 2.3
     */
    static inline GDALColorTableH ToHandle(GDALColorTable* poCT)
        { return static_cast<GDALColorTableH>(poCT); }

    /** Convert a GDALColorTableH to a GDALColorTable*.
     * @since GDAL 2.3
     */
    static inline GDALColorTable* FromHandle(GDALColorTableH hCT)
        { return static_cast<GDALColorTable*>(hCT); }

};

/* ******************************************************************** */
/*                       GDALAbstractBandBlockCache                     */
/* ******************************************************************** */

 //! @cond Doxygen_Suppress

//! This manages how a raster band store its cached block.
// only used by GDALRasterBand implementation.

class GDALAbstractBandBlockCache
{
        // List of blocks that can be freed or recycled, and its lock
        CPLLock          *hSpinLock = nullptr;
        GDALRasterBlock  *psListBlocksToFree = nullptr;

        // Band keep alive counter, and its lock & condition
        CPLCond          *hCond = nullptr;
        CPLMutex         *hCondMutex = nullptr;
        volatile int      nKeepAliveCounter = 0;

        volatile int      m_nDirtyBlocks = 0;

        CPL_DISALLOW_COPY_ASSIGN(GDALAbstractBandBlockCache)

    protected:
        GDALRasterBand   *poBand;

        int               m_nInitialDirtyBlocksInFlushCache = 0;
        int               m_nLastTick = -1;

        void              FreeDanglingBlocks();
        void              UnreferenceBlockBase();

        void              StartDirtyBlockFlushingLog();
        void              UpdateDirtyBlockFlushingLog();
        void              EndDirtyBlockFlushingLog();

    public:
            explicit GDALAbstractBandBlockCache(GDALRasterBand* poBand);
            virtual ~GDALAbstractBandBlockCache();

            GDALRasterBlock* CreateBlock(int nXBlockOff, int nYBlockOff);
            void             AddBlockToFreeList( GDALRasterBlock * );
            void             IncDirtyBlocks(int nInc);
            void             WaitCompletionPendingTasks();

            virtual bool             Init() = 0;
            virtual bool             IsInitOK() = 0;
            virtual CPLErr           FlushCache() = 0;
            virtual CPLErr           AdoptBlock( GDALRasterBlock* poBlock ) = 0;
            virtual GDALRasterBlock *TryGetLockedBlockRef( int nXBlockOff,
                                                           int nYBlockYOff ) = 0;
            virtual CPLErr           UnreferenceBlock( GDALRasterBlock* poBlock ) = 0;
            virtual CPLErr           FlushBlock( int nXBlockOff, int nYBlockOff,
                                                 int bWriteDirtyBlock ) = 0;
};

GDALAbstractBandBlockCache* GDALArrayBandBlockCacheCreate(GDALRasterBand* poBand);
GDALAbstractBandBlockCache* GDALHashSetBandBlockCacheCreate(GDALRasterBand* poBand);

//! @endcond

/* ******************************************************************** */
/*                            GDALRasterBand                            */
/* ******************************************************************** */

class GDALMDArray;

/** A single raster band (or channel). */

class CPL_DLL GDALRasterBand : public GDALMajorObject
{
  private:
    friend class GDALArrayBandBlockCache;
    friend class GDALHashSetBandBlockCache;
    friend class GDALRasterBlock;
    friend class GDALDataset;

    CPLErr eFlushBlockErr = CE_None;
    GDALAbstractBandBlockCache* poBandBlockCache = nullptr;

    CPL_INTERNAL void           SetFlushBlockErr( CPLErr eErr );
    CPL_INTERNAL CPLErr         UnreferenceBlock( GDALRasterBlock* poBlock );
    CPL_INTERNAL void           SetValidPercent( GUIntBig nSampleCount, GUIntBig nValidCount );
    CPL_INTERNAL void           IncDirtyBlocks(int nInc);

  protected:
//! @cond Doxygen_Suppress
    GDALDataset *poDS = nullptr;
    int         nBand = 0; /* 1 based */

    int         nRasterXSize = 0;
    int         nRasterYSize = 0;

    GDALDataType eDataType = GDT_Byte;
    GDALAccess  eAccess = GA_ReadOnly;

    /* stuff related to blocking, and raster cache */
    int         nBlockXSize = -1;
    int         nBlockYSize = -1;
    int         nBlocksPerRow = 0;
    int         nBlocksPerColumn = 0;

    int         nBlockReads = 0;
    int         bForceCachedIO = 0;

    GDALRasterBand *poMask = nullptr;
    bool        bOwnMask = false;
    int         nMaskFlags = 0;

    void        InvalidateMaskBand();

    friend class GDALProxyRasterBand;
    friend class GDALDefaultOverviews;

    CPLErr RasterIOResampled( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg ) CPL_WARN_UNUSED_RESULT;

    int          EnterReadWrite(GDALRWFlag eRWFlag);
    void         LeaveReadWrite();
    void         InitRWLock();
//! @endcond

  protected:
    virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void * pData ) = 0;
    virtual CPLErr IWriteBlock( int nBlockXOff, int nBlockYOff, void * pData );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg ) CPL_WARN_UNUSED_RESULT;

    virtual int IGetDataCoverageStatus( int nXOff, int nYOff,
                                        int nXSize, int nYSize,
                                        int nMaskFlagStop,
                                        double* pdfDataPct);
//! @cond Doxygen_Suppress
    CPLErr         OverviewRasterIO( GDALRWFlag, int, int, int, int,
                                     void *, int, int, GDALDataType,
                                     GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg ) CPL_WARN_UNUSED_RESULT;

    CPLErr TryOverviewRasterIO( GDALRWFlag eRWFlag,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void * pData, int nBufXSize, int nBufYSize,
                                GDALDataType eBufType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg,
                                int* pbTried );

    int            InitBlockInfo();

    void           AddBlockToFreeList( GDALRasterBlock * );
//! @endcond

    GDALRasterBlock *TryGetLockedBlockRef( int nXBlockOff, int nYBlockYOff );

  public:
                GDALRasterBand();
    explicit    GDALRasterBand(int bForceCachedIO);

    ~GDALRasterBand() override;

    int         GetXSize();
    int         GetYSize();
    int         GetBand();
    GDALDataset*GetDataset();

    GDALDataType GetRasterDataType( void );
    void        GetBlockSize( int *, int * );
    CPLErr      GetActualBlockSize ( int, int, int *, int * );
    GDALAccess  GetAccess();

    CPLErr      RasterIO( GDALRWFlag, int, int, int, int,
                          void *, int, int, GDALDataType,
                          GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg
#ifndef DOXYGEN_SKIP
                          OPTIONAL_OUTSIDE_GDAL(nullptr)
#endif
                          ) CPL_WARN_UNUSED_RESULT;
    CPLErr      ReadBlock( int, int, void * ) CPL_WARN_UNUSED_RESULT;

    CPLErr      WriteBlock( int, int, void * ) CPL_WARN_UNUSED_RESULT;

    GDALRasterBlock *GetLockedBlockRef( int nXBlockOff, int nYBlockOff,
                                        int bJustInitialize = FALSE ) CPL_WARN_UNUSED_RESULT;
    CPLErr      FlushBlock( int, int, int bWriteDirtyBlock = TRUE );

    unsigned char*  GetIndexColorTranslationTo(/* const */ GDALRasterBand* poReferenceBand,
                                               unsigned char* pTranslationTable = nullptr,
                                               int* pApproximateMatching = nullptr);

    // New OpengIS CV_SampleDimension stuff.

    virtual CPLErr FlushCache();
    virtual char **GetCategoryNames();
    virtual double GetNoDataValue( int *pbSuccess = nullptr );
    virtual double GetMinimum( int *pbSuccess = nullptr );
    virtual double GetMaximum(int *pbSuccess = nullptr );
    virtual double GetOffset( int *pbSuccess = nullptr );
    virtual double GetScale( int *pbSuccess = nullptr );
    virtual const char *GetUnitType();
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0);

    virtual CPLErr SetCategoryNames( char ** papszNames );
    virtual CPLErr SetNoDataValue( double dfNoData );
    virtual CPLErr DeleteNoDataValue();
    virtual CPLErr SetColorTable( GDALColorTable * poCT );
    virtual CPLErr SetColorInterpretation( GDALColorInterp eColorInterp );
    virtual CPLErr SetOffset( double dfNewOffset );
    virtual CPLErr SetScale( double dfNewScale );
    virtual CPLErr SetUnitType( const char * pszNewValue );

    virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax,
                                  double *pdfMean, double *padfStdDev );
    virtual CPLErr ComputeStatistics( int bApproxOK,
                                      double *pdfMin, double *pdfMax,
                                      double *pdfMean, double *pdfStdDev,
                                      GDALProgressFunc, void *pProgressData );
    virtual CPLErr SetStatistics( double dfMin, double dfMax,
                                  double dfMean, double dfStdDev );
    virtual CPLErr ComputeRasterMinMax( int, double* );

// Only defined when Doxygen enabled
#ifdef DOXYGEN_SKIP
    CPLErr      SetMetadata( char ** papszMetadata,
                             const char * pszDomain ) override;
    CPLErr      SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain ) override;
#endif

    virtual int HasArbitraryOverviews();
    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);
    virtual GDALRasterBand *GetRasterSampleOverview( GUIntBig );
    virtual CPLErr BuildOverviews( const char * pszResampling,
                                   int nOverviews,
                                   int * panOverviewList,
                                   GDALProgressFunc pfnProgress,
                                   void * pProgressData );

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, char **papszOptions );

    virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                          int nBuckets, GUIntBig * panHistogram,
                          int bIncludeOutOfRange, int bApproxOK,
                          GDALProgressFunc, void *pProgressData );

    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets, GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData);
    virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig *panHistogram );

    virtual GDALRasterAttributeTable *GetDefaultRAT();
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * poRAT );

    virtual GDALRasterBand *GetMaskBand();
    virtual int             GetMaskFlags();
    virtual CPLErr          CreateMaskBand( int nFlagsIn );

    virtual CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions ) CPL_WARN_UNUSED_RESULT;

    int GetDataCoverageStatus( int nXOff, int nYOff,
                               int nXSize, int nYSize,
                               int nMaskFlagStop = 0,
                               double* pdfDataPct = nullptr );

    std::shared_ptr<GDALMDArray> AsMDArray() const;

#ifndef DOXYGEN_XML
    void ReportError(CPLErr eErrClass, CPLErrorNum err_no, const char *fmt, ...)  CPL_PRINT_FUNC_FORMAT (4, 5);
#endif

    /** Convert a GDALRasterBand* to a GDALRasterBandH.
     * @since GDAL 2.3
     */
    static inline GDALRasterBandH ToHandle(GDALRasterBand* poBand)
        { return static_cast<GDALRasterBandH>(poBand); }

    /** Convert a GDALRasterBandH to a GDALRasterBand*.
     * @since GDAL 2.3
     */
    static inline GDALRasterBand* FromHandle(GDALRasterBandH hBand)
        { return static_cast<GDALRasterBand*>(hBand); }

private:
    CPL_DISALLOW_COPY_ASSIGN(GDALRasterBand)
};

//! @cond Doxygen_Suppress
/* ******************************************************************** */
/*                         GDALAllValidMaskBand                         */
/* ******************************************************************** */

class CPL_DLL GDALAllValidMaskBand : public GDALRasterBand
{
  protected:
    CPLErr IReadBlock( int, int, void * ) override;

    CPL_DISALLOW_COPY_ASSIGN(GDALAllValidMaskBand)

  public:
    explicit     GDALAllValidMaskBand( GDALRasterBand * );
    ~GDALAllValidMaskBand() override;

    GDALRasterBand *GetMaskBand() override;
    int             GetMaskFlags() override;

    CPLErr ComputeStatistics( int bApproxOK,
                            double *pdfMin, double *pdfMax,
                            double *pdfMean, double *pdfStdDev,
                            GDALProgressFunc, void *pProgressData ) override;

};

/* ******************************************************************** */
/*                         GDALNoDataMaskBand                           */
/* ******************************************************************** */

class CPL_DLL GDALNoDataMaskBand : public GDALRasterBand
{
    double          dfNoDataValue;
    GDALRasterBand *poParent;

    CPL_DISALLOW_COPY_ASSIGN(GDALNoDataMaskBand)

  protected:
    CPLErr IReadBlock( int, int, void * ) override;
    CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                      void *, int, int, GDALDataType,
                      GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg ) override;

  public:
    explicit GDALNoDataMaskBand( GDALRasterBand * );
    ~GDALNoDataMaskBand() override;

    static bool IsNoDataInRange(double dfNoDataValue,
                                GDALDataType eDataType);
};

/* ******************************************************************** */
/*                  GDALNoDataValuesMaskBand                            */
/* ******************************************************************** */

class CPL_DLL GDALNoDataValuesMaskBand : public GDALRasterBand
{
    double      *padfNodataValues;

    CPL_DISALLOW_COPY_ASSIGN(GDALNoDataValuesMaskBand)

  protected:
    CPLErr IReadBlock( int, int, void * ) override;

  public:
    explicit     GDALNoDataValuesMaskBand( GDALDataset * );
    ~GDALNoDataValuesMaskBand() override;
};

/* ******************************************************************** */
/*                         GDALRescaledAlphaBand                        */
/* ******************************************************************** */

class GDALRescaledAlphaBand : public GDALRasterBand
{
    GDALRasterBand *poParent;
    void           *pTemp;

    CPL_DISALLOW_COPY_ASSIGN(GDALRescaledAlphaBand)

  protected:
    CPLErr IReadBlock( int, int, void * ) override;
    CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                      void *, int, int, GDALDataType,
                      GSpacing, GSpacing,
                      GDALRasterIOExtraArg* psExtraArg ) override;

  public:
    explicit GDALRescaledAlphaBand( GDALRasterBand * );
    ~GDALRescaledAlphaBand() override;
};
//! @endcond

/* ******************************************************************** */
/*                          GDALIdentifyEnum                            */
/* ******************************************************************** */

/**
 * Enumeration used by GDALDriver::pfnIdentify().
 *
 * @since GDAL 2.1
 */
typedef enum
{
    /** Identify could not determine if the file is recognized or not by the probed driver. */
    GDAL_IDENTIFY_UNKNOWN = -1,
    /** Identify determined the file is not recognized by the probed driver. */
    GDAL_IDENTIFY_FALSE = 0,
    /** Identify determined the file is recognized by the probed driver. */
    GDAL_IDENTIFY_TRUE = 1
} GDALIdentifyEnum;

/* ******************************************************************** */
/*                              GDALDriver                              */
/* ******************************************************************** */

/**
 * \brief Format specific driver.
 *
 * An instance of this class is created for each supported format, and
 * manages information about the format.
 *
 * This roughly corresponds to a file format, though some
 * drivers may be gateways to many formats through a secondary
 * multi-library.
 */

class CPL_DLL GDALDriver : public GDALMajorObject
{
  public:
    GDALDriver();
    ~GDALDriver() override;

    CPLErr      SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain = "" ) override;

/* -------------------------------------------------------------------- */
/*      Public C++ methods.                                             */
/* -------------------------------------------------------------------- */
    GDALDataset         *Create( const char * pszName,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszOptions ) CPL_WARN_UNUSED_RESULT;

    GDALDataset         *CreateMultiDimensional( const char * pszName,
                                                 CSLConstList papszRootGroupOptions,
                                                 CSLConstList papszOptions ) CPL_WARN_UNUSED_RESULT;

    CPLErr              Delete( const char * pszName );
    CPLErr              Rename( const char * pszNewName,
                                const char * pszOldName );
    CPLErr              CopyFiles( const char * pszNewName,
                                   const char * pszOldName );

    GDALDataset         *CreateCopy( const char *, GDALDataset *,
                                     int, char **,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData ) CPL_WARN_UNUSED_RESULT;

/* -------------------------------------------------------------------- */
/*      The following are semiprivate, not intended to be accessed      */
/*      by anyone but the formats instantiating and populating the      */
/*      drivers.                                                        */
/* -------------------------------------------------------------------- */
//! @cond Doxygen_Suppress
    GDALDataset         *(*pfnOpen)( GDALOpenInfo * );

    GDALDataset         *(*pfnCreate)( const char * pszName,
                                       int nXSize, int nYSize, int nBands,
                                       GDALDataType eType,
                                       char ** papszOptions );

    GDALDataset         *(*pfnCreateEx)( GDALDriver*, const char * pszName,
                                       int nXSize, int nYSize, int nBands,
                                       GDALDataType eType,
                                       char ** papszOptions );

    GDALDataset         *(*pfnCreateMultiDimensional)( const char * pszName,
                                                       CSLConstList papszRootGroupOptions,
                                                       CSLConstList papszOptions );

    CPLErr              (*pfnDelete)( const char * pszName );

    GDALDataset         *(*pfnCreateCopy)( const char *, GDALDataset *,
                                           int, char **,
                                           GDALProgressFunc pfnProgress,
                                           void * pProgressData );

    void                *pDriverData;

    void                (*pfnUnloadDriver)(GDALDriver *);

    /** Identify() if the file is recognized or not by the driver.

       Return GDAL_IDENTIFY_TRUE (1) if the passed file is certainly recognized by the driver.
       Return GDAL_IDENTIFY_FALSE (0) if the passed file is certainly NOT recognized by the driver.
       Return GDAL_IDENTIFY_UNKNOWN (-1) if the passed file may be or may not be recognized by the driver,
       and that a potentially costly test must be done with pfnOpen.
    */
    int                 (*pfnIdentify)( GDALOpenInfo * );
    int                 (*pfnIdentifyEx)( GDALDriver*, GDALOpenInfo * );

    CPLErr              (*pfnRename)( const char * pszNewName,
                                      const char * pszOldName );
    CPLErr              (*pfnCopyFiles)( const char * pszNewName,
                                         const char * pszOldName );

    // Used for legacy OGR drivers, and Python drivers
    GDALDataset         *(*pfnOpenWithDriverArg)( GDALDriver*, GDALOpenInfo * );

    /* For legacy OGR drivers */
    GDALDataset         *(*pfnCreateVectorOnly)( GDALDriver*,
                                                 const char * pszName,
                                                 char ** papszOptions );
    CPLErr              (*pfnDeleteDataSource)( GDALDriver*,
                                                 const char * pszName );
//! @endcond

/* -------------------------------------------------------------------- */
/*      Helper methods.                                                 */
/* -------------------------------------------------------------------- */
//! @cond Doxygen_Suppress
    GDALDataset         *DefaultCreateCopy( const char *, GDALDataset *,
                                            int, char **,
                                            GDALProgressFunc pfnProgress,
                                            void * pProgressData ) CPL_WARN_UNUSED_RESULT;

    static CPLErr        DefaultCreateCopyMultiDimensional(
                                     GDALDataset *poSrcDS,
                                     GDALDataset *poDstDS,
                                     bool bStrict,
                                     CSLConstList /*papszOptions*/,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData );

    static CPLErr        DefaultCopyMasks( GDALDataset *poSrcDS,
                                           GDALDataset *poDstDS,
                                           int bStrict );
    static CPLErr        DefaultCopyMasks( GDALDataset *poSrcDS,
                                           GDALDataset *poDstDS,
                                           int bStrict,
                                           CSLConstList papszOptions,
                                           GDALProgressFunc pfnProgress,
                                           void * pProgressData );
//! @endcond
    static CPLErr       QuietDelete( const char * pszName,
                                     const char *const *papszAllowedDrivers = nullptr);

//! @cond Doxygen_Suppress
    static CPLErr       DefaultRename( const char * pszNewName,
                                       const char * pszOldName );
    static CPLErr       DefaultCopyFiles( const char * pszNewName,
                                          const char * pszOldName );
//! @endcond

    /** Convert a GDALDriver* to a GDALDriverH.
     * @since GDAL 2.3
     */
    static inline GDALDriverH ToHandle(GDALDriver* poDriver)
        { return static_cast<GDALDriverH>(poDriver); }

    /** Convert a GDALDriverH to a GDALDriver*.
     * @since GDAL 2.3
     */
    static inline GDALDriver* FromHandle(GDALDriverH hDriver)
        { return static_cast<GDALDriver*>(hDriver); }

private:
    CPL_DISALLOW_COPY_ASSIGN(GDALDriver)
};

/* ******************************************************************** */
/*                          GDALDriverManager                           */
/* ******************************************************************** */

/**
 * Class for managing the registration of file format drivers.
 *
 * Use GetGDALDriverManager() to fetch the global singleton instance of
 * this class.
 */

class CPL_DLL GDALDriverManager : public GDALMajorObject
{
    int         nDrivers = 0;
    GDALDriver  **papoDrivers = nullptr;
    std::map<CPLString, GDALDriver*> oMapNameToDrivers{};

    GDALDriver  *GetDriver_unlocked( int iDriver )
            { return (iDriver >= 0 && iDriver < nDrivers) ?
                  papoDrivers[iDriver] : nullptr; }

    GDALDriver  *GetDriverByName_unlocked( const char * pszName )
            { return oMapNameToDrivers[CPLString(pszName).toupper()]; }

    static char** GetSearchPaths(const char* pszGDAL_DRIVER_PATH);

    static void   CleanupPythonDrivers();

    CPL_DISALLOW_COPY_ASSIGN(GDALDriverManager)

 public:
                GDALDriverManager();
                ~GDALDriverManager();

    int         GetDriverCount( void ) const;
    GDALDriver  *GetDriver( int );
    GDALDriver  *GetDriverByName( const char * );

    int         RegisterDriver( GDALDriver * );
    void        DeregisterDriver( GDALDriver * );

    // AutoLoadDrivers is a no-op if compiled with GDAL_NO_AUTOLOAD defined.
    static void        AutoLoadDrivers();
    void        AutoSkipDrivers();

    static void        AutoLoadPythonDrivers();
};

CPL_C_START
GDALDriverManager CPL_DLL * GetGDALDriverManager( void );
CPL_C_END

/* ******************************************************************** */
/*                          GDALAsyncReader                             */
/* ******************************************************************** */

/**
 * Class used as a session object for asynchronous requests.  They are
 * created with GDALDataset::BeginAsyncReader(), and destroyed with
 * GDALDataset::EndAsyncReader().
 */
class CPL_DLL GDALAsyncReader
{

    CPL_DISALLOW_COPY_ASSIGN(GDALAsyncReader)

  protected:
//! @cond Doxygen_Suppress
    GDALDataset* poDS;
    int          nXOff;
    int          nYOff;
    int          nXSize;
    int          nYSize;
    void *       pBuf;
    int          nBufXSize;
    int          nBufYSize;
    GDALDataType eBufType;
    int          nBandCount;
    int*         panBandMap;
    int          nPixelSpace;
    int          nLineSpace;
    int          nBandSpace;
//! @endcond

  public:
    GDALAsyncReader();
    virtual ~GDALAsyncReader();

    /** Return dataset.
     * @return dataset
     */
    GDALDataset* GetGDALDataset() {return poDS;}
    /** Return x offset.
     * @return x offset.
     */
    int GetXOffset() const { return nXOff; }
    /** Return y offset.
     * @return y offset.
     */
    int GetYOffset() const { return nYOff; }
    /** Return width.
     * @return width
     */
    int GetXSize() const { return nXSize; }
    /** Return height.
     * @return height
     */
    int GetYSize() const { return nYSize; }
    /** Return buffer.
     * @return buffer
     */
    void * GetBuffer() {return pBuf;}
    /** Return buffer width.
     * @return buffer width.
     */
    int GetBufferXSize() const { return nBufXSize; }
    /** Return buffer height.
     * @return buffer height.
     */
    int GetBufferYSize() const { return nBufYSize; }
    /** Return buffer data type.
     * @return buffer data type.
     */
    GDALDataType GetBufferType() const { return eBufType; }
    /** Return band count.
     * @return band count
     */
    int GetBandCount() const { return nBandCount; }
    /** Return band map.
     * @return band map.
     */
    int* GetBandMap() { return panBandMap; }
    /** Return pixel spacing.
     * @return pixel spacing.
     */
    int GetPixelSpace() const { return nPixelSpace; }
    /** Return line spacing.
     * @return line spacing.
     */
    int GetLineSpace() const { return nLineSpace; }
    /** Return band spacing.
     * @return band spacing.
     */
    int GetBandSpace() const { return nBandSpace; }

    virtual GDALAsyncStatusType
        GetNextUpdatedRegion(double dfTimeout,
                             int* pnBufXOff, int* pnBufYOff,
                             int* pnBufXSize, int* pnBufYSize) = 0;
    virtual int LockBuffer( double dfTimeout = -1.0 );
    virtual void UnlockBuffer();
};

/* ******************************************************************** */
/*                       Multidimensional array API                     */
/* ******************************************************************** */

class GDALMDArray;
class GDALAttribute;
class GDALDimension;
class GDALEDTComponent;

/* ******************************************************************** */
/*                         GDALExtendedDataType                         */
/* ******************************************************************** */

/**
 * Class used to represent potentially complex data types.
 * Several classes of data types are supported: numeric (based on GDALDataType),
 * compound or string.
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALExtendedDataType
{
public:
    ~GDALExtendedDataType();

    GDALExtendedDataType(const GDALExtendedDataType&);

    GDALExtendedDataType& operator= (GDALExtendedDataType&&);

    static GDALExtendedDataType Create(GDALDataType eType);
    static GDALExtendedDataType Create(const std::string& osName,
                                       size_t nTotalSize,
                                       std::vector<std::unique_ptr<GDALEDTComponent>>&& components);
    static GDALExtendedDataType CreateString(size_t nMaxStringLength = 0);

    bool operator== (const GDALExtendedDataType& ) const;
    /** Non-equality operator */
    bool operator!= (const GDALExtendedDataType& other) const { return !(operator==(other)); }

    /** Return type name.
     *
     * This is the same as the C function GDALExtendedDataTypeGetName()
     */
    const std::string&        GetName() const { return m_osName; }

    /** Return type class.
     *
     * This is the same as the C function GDALExtendedDataTypeGetClass()
     */
    GDALExtendedDataTypeClass GetClass() const { return m_eClass; }

    /** Return numeric data type (only valid when GetClass() == GEDTC_NUMERIC)
     *
     * This is the same as the C function GDALExtendedDataTypeGetNumericDataType()
     */
    GDALDataType              GetNumericDataType() const { return m_eNumericDT;  }

    /** Return the components of the data type (only valid when GetClass() == GEDTC_COMPOUND)
     *
     * This is the same as the C function GDALExtendedDataTypeGetComponents()
     */
    const std::vector<std::unique_ptr<GDALEDTComponent>>& GetComponents() const { return m_aoComponents; }

    /** Return data type size in bytes.
     *
     * For a string, this will be size of a char* pointer.
     *
     * This is the same as the C function GDALExtendedDataTypeGetSize()
     */
    size_t                    GetSize() const { return m_nSize; }

    /** Return the maximum length of a string in bytes.
     *
     * 0 indicates unknown/unlimited string.
     */
    size_t                    GetMaxStringLength() const { return m_nMaxStringLength; }

    bool CanConvertTo(const GDALExtendedDataType& other) const;

    bool NeedsFreeDynamicMemory() const;

    void FreeDynamicMemory(void* pBuffer) const;

    static
    bool CopyValue(const void* pSrc, const GDALExtendedDataType& srcType,
                     void* pDst, const GDALExtendedDataType& dstType);

private:
    explicit GDALExtendedDataType(size_t nMaxStringLength = 0);
    explicit  GDALExtendedDataType(GDALDataType eType);
    GDALExtendedDataType(const std::string& osName,
                         size_t nTotalSize,
                         std::vector<std::unique_ptr<GDALEDTComponent>>&& components);

    std::string m_osName{};
    GDALExtendedDataTypeClass m_eClass = GEDTC_NUMERIC;
    GDALDataType m_eNumericDT = GDT_Unknown;
    std::vector<std::unique_ptr<GDALEDTComponent>> m_aoComponents{};
    size_t m_nSize = 0;
    size_t m_nMaxStringLength = 0;
};

/* ******************************************************************** */
/*                            GDALEDTComponent                          */
/* ******************************************************************** */

/**
 * Class for a component of a compound extended data type.
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALEDTComponent
{
public:
    ~GDALEDTComponent();
    GDALEDTComponent(const std::string& name, size_t offset, const GDALExtendedDataType& type);
    GDALEDTComponent(const GDALEDTComponent&);

    bool operator== (const GDALEDTComponent& ) const;

    /** Return the name.
     *
     * This is the same as the C function GDALEDTComponentGetName().
     */
    const std::string&   GetName() const { return m_osName; }

    /** Return the offset (in bytes) of the component in the compound data type.
     *
     * This is the same as the C function GDALEDTComponentGetOffset().
     */
    size_t               GetOffset() const { return m_nOffset; }

    /** Return the data type of the component.
     *
     * This is the same as the C function GDALEDTComponentGetType().
     */
    const GDALExtendedDataType& GetType() const { return m_oType; }

private:
    std::string          m_osName;
    size_t               m_nOffset;
    GDALExtendedDataType m_oType;
};

/* ******************************************************************** */
/*                            GDALIHasAttribute                         */
/* ******************************************************************** */

/**
 * Interface used to get a single GDALAttribute or a set of GDALAttribute
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALIHasAttribute
{
protected:
    std::shared_ptr<GDALAttribute> GetAttributeFromAttributes(const std::string& osName) const;

public:
    virtual ~GDALIHasAttribute();

    virtual std::shared_ptr<GDALAttribute> GetAttribute(const std::string& osName) const;

    virtual std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList papszOptions = nullptr) const;

    virtual std::shared_ptr<GDALAttribute> CreateAttribute(
        const std::string& osName,
        const std::vector<GUInt64>& anDimensions,
        const GDALExtendedDataType& oDataType,
        CSLConstList papszOptions = nullptr);
};

/* ******************************************************************** */
/*                               GDALGroup                              */
/* ******************************************************************** */

/**
 * Class modeling a named container of GDALAttribute, GDALMDArray, OGRLayer or other
 * GDALGroup. Hence GDALGroup can describe a hierarchy of objects.
 *
 * This is based on the <a href="https://portal.opengeospatial.org/files/81716#_hdf5_group">HDF5 group concept</a>
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALGroup: public GDALIHasAttribute
{
protected:
//! @cond Doxygen_Suppress
    std::string m_osName{};
    std::string m_osFullName{};

    GDALGroup(const std::string& osParentName, const std::string& osName);

    const GDALGroup* GetInnerMostGroup(const std::string& osPathOrArrayOrDim,
                                       std::shared_ptr<GDALGroup>& curGroupHolder,
                                       std::string& osLastPart) const;
//! @endcond

public:
    virtual ~GDALGroup();

    /** Return the name of the group.
     *
     * This is the same as the C function GDALGroupGetName().
     */
    const std::string& GetName() const { return m_osName; }

    /** Return the full name of the group.
     *
     * This is the same as the C function GDALGroupGetFullName().
     */
    const std::string& GetFullName() const { return m_osFullName; }

    virtual std::vector<std::string> GetMDArrayNames(CSLConstList papszOptions = nullptr) const;
    virtual std::shared_ptr<GDALMDArray> OpenMDArray(const std::string& osName,
                                                     CSLConstList papszOptions = nullptr) const;

    virtual std::vector<std::string> GetGroupNames(CSLConstList papszOptions = nullptr) const;
    virtual std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                                 CSLConstList papszOptions = nullptr) const;

    virtual std::vector<std::string> GetVectorLayerNames(CSLConstList papszOptions = nullptr) const;
    virtual OGRLayer* OpenVectorLayer(const std::string& osName,
                                      CSLConstList papszOptions = nullptr) const;

    virtual std::vector<std::shared_ptr<GDALDimension>> GetDimensions(CSLConstList papszOptions = nullptr) const;

    virtual std::shared_ptr<GDALGroup> CreateGroup(const std::string& osName,
                                                   CSLConstList papszOptions = nullptr);

    virtual std::shared_ptr<GDALDimension> CreateDimension(const std::string& osName,
                                                           const std::string& osType,
                                                           const std::string& osDirection,
                                                           GUInt64 nSize,
                                                           CSLConstList papszOptions = nullptr);

    virtual std::shared_ptr<GDALMDArray> CreateMDArray(const std::string& osName,
                                                       const std::vector<std::shared_ptr<GDALDimension>>& aoDimensions,
                                                       const GDALExtendedDataType& oDataType,
                                                       CSLConstList papszOptions = nullptr);

    GUInt64 GetTotalCopyCost() const;

    virtual bool CopyFrom(const std::shared_ptr<GDALGroup>& poDstRootGroup,
                          GDALDataset* poSrcDS,
                          const std::shared_ptr<GDALGroup>& poSrcGroup,
                          bool bStrict,
                          GUInt64& nCurCost,
                          const GUInt64 nTotalCost,
                          GDALProgressFunc pfnProgress,
                          void * pProgressData,
                          CSLConstList papszOptions = nullptr);

    virtual CSLConstList GetStructuralInfo() const;

    std::shared_ptr<GDALMDArray> OpenMDArrayFromFullname(
                                        const std::string& osFullName,
                                        CSLConstList papszOptions = nullptr) const;

    std::shared_ptr<GDALMDArray> ResolveMDArray(const std::string& osName,
                                                const std::string& osStartingPath,
                                                CSLConstList papszOptions = nullptr) const;

    std::shared_ptr<GDALGroup> OpenGroupFromFullname(
                                        const std::string& osFullName,
                                        CSLConstList papszOptions = nullptr) const;

    std::shared_ptr<GDALDimension> OpenDimensionFromFullname(
                                        const std::string& osFullName) const;

//! @cond Doxygen_Suppress
    static constexpr GUInt64 COPY_COST = 1000;
//! @endcond
};

/* ******************************************************************** */
/*                          GDALAbstractMDArray                         */
/* ******************************************************************** */

/**
 * Abstract class, implemented by GDALAttribute and GDALMDArray.
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALAbstractMDArray
{
protected:
//! @cond Doxygen_Suppress
    std::string m_osName{};
    std::string m_osFullName{};
    std::weak_ptr<GDALAbstractMDArray> m_pSelf{};

    GDALAbstractMDArray(const std::string& osParentName, const std::string& osName);

    void SetSelf(std::weak_ptr<GDALAbstractMDArray> self) { m_pSelf = self; }

    bool CheckReadWriteParams(const GUInt64* arrayStartIdx,
                              const size_t* count,
                              const GInt64*& arrayStep,
                              const GPtrDiff_t*& bufferStride,
                              const GDALExtendedDataType& bufferDataType,
                              const void* buffer,
                              const void* buffer_alloc_start,
                              size_t buffer_alloc_size,
                              std::vector<GInt64>& tmp_arrayStep,
                              std::vector<GPtrDiff_t>& tmp_bufferStride) const;

    virtual bool IRead(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer) const = 0;

    virtual bool IWrite(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer);
//! @endcond

public:
    virtual ~GDALAbstractMDArray();

    /** Return the name of an array or attribute.
     *
     * This is the same as the C function GDALMDArrayGetName() or GDALAttributeGetName().
     */
    const std::string& GetName() const{ return m_osName; }

    /** Return the name of an array or attribute.
     *
     * This is the same as the C function GDALMDArrayGetFullName() or GDALAttributeGetFullName().
     */
    const std::string& GetFullName() const{ return m_osFullName; }

    GUInt64 GetTotalElementsCount() const;

    virtual size_t GetDimensionCount() const;

    virtual const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const = 0;

    virtual const GDALExtendedDataType &GetDataType() const = 0;

    virtual std::vector<GUInt64> GetBlockSize() const;

    virtual std::vector<size_t> GetProcessingChunkSize(size_t nMaxChunkMemory) const;

    /** Type of pfnFunc argument of ProcessPerChunk().
     * @param array Array on which ProcessPerChunk was called.
     * @param chunkArrayStartIdx Values representing the starting index to use
     *                           in each dimension (in [0, aoDims[i].GetSize()-1] range)
     *                           for the current chunk.
     *                           Will be nullptr for a zero-dimensional array.
     * @param chunkCount         Values representing the number of values to use in
     *                           each dimension for the current chunk.
     *                           Will be nullptr for a zero-dimensional array.
     * @param iCurChunk          Number of current chunk being processed.
     *                           In [1, nChunkCount] range.
     * @param nChunkCount        Total number of chunks to process.
     * @param pUserData          User data.
     * @return return true in case of success.
     */
    typedef bool (*FuncProcessPerChunkType)(
                                GDALAbstractMDArray* array,
                                const GUInt64* chunkArrayStartIdx,
                                const size_t* chunkCount,
                                GUInt64 iCurChunk,
                                GUInt64 nChunkCount,
                                void* pUserData);

    virtual bool ProcessPerChunk(const GUInt64* arrayStartIdx,
                                 const GUInt64* count,
                                 const size_t* chunkSize,
                                 FuncProcessPerChunkType pfnFunc,
                                 void* pUserData);

    bool Read(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      void* pDstBuffer,
                      const void* pDstBufferAllocStart = nullptr,
                      size_t nDstBufferAllocSize = 0) const;

    bool Write(const GUInt64* arrayStartIdx,     // array of size GetDimensionCount()
                      const size_t* count,                 // array of size GetDimensionCount()
                      const GInt64* arrayStep,        // step in elements
                      const GPtrDiff_t* bufferStride, // stride in elements
                      const GDALExtendedDataType& bufferDataType,
                      const void* pSrcBuffer,
                      const void* pSrcBufferAllocStart = nullptr,
                      size_t nSrcBufferAllocSize = 0);
};

/* ******************************************************************** */
/*                              GDALRawResult                           */
/* ******************************************************************** */

/**
 * Store the raw result of an attribute value, which might contain dynamically
 * allocated structures (like pointer to strings).
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALRawResult
{
private:
    GDALExtendedDataType m_dt;
    size_t m_nEltCount;
    size_t m_nSize;
    GByte* m_raw;

    void FreeMe();

    GDALRawResult(const GDALRawResult&) = delete;
    GDALRawResult& operator=(const GDALRawResult&) = delete;

protected:
    friend class GDALAttribute;
//! @cond Doxygen_Suppress
    GDALRawResult(GByte* raw,
                  const GDALExtendedDataType& dt,
                  size_t nEltCount);
//! @endcond

public:
    ~GDALRawResult();
    GDALRawResult(GDALRawResult&&);
    GDALRawResult& operator=(GDALRawResult&&);

    /** Return byte at specified index. */
    const GByte& operator[](size_t idx) const { return m_raw[idx]; }
    /** Return pointer to the start of data. */
    const GByte* data() const { return m_raw; }
    /** Return the size in bytes of the raw result. */
    size_t size() const { return m_nSize; }

//! @cond Doxygen_Suppress
    GByte* StealData();
//! @endcond
};


/* ******************************************************************** */
/*                              GDALAttribute                           */
/* ******************************************************************** */

/**
 * Class modeling an attribute that has a name, a value and a type, and is
 * typically used to describe a metadata item. The value can be (for the
 * HDF5 format) in the general case a multidimensional array of "any" type
 * (in most cases, this will be a single value of string or numeric type)
 *
 * This is based on the <a href="https://portal.opengeospatial.org/files/81716#_hdf5_attribute">HDF5 attribute concept</a>
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALAttribute: virtual public GDALAbstractMDArray
{
    mutable std::string m_osCachedVal{};

protected:
//! @cond Doxygen_Suppress
    GDALAttribute(const std::string& osParentName, const std::string& osName);
//! @endcond

public:

    std::vector<GUInt64> GetDimensionsSize() const;

    GDALRawResult ReadAsRaw() const;
    const char* ReadAsString() const;
    int ReadAsInt() const;
    double ReadAsDouble() const;
    CPLStringList ReadAsStringArray() const;
    std::vector<int> ReadAsIntArray() const;
    std::vector<double> ReadAsDoubleArray() const;

    using GDALAbstractMDArray::Write;
    bool Write(const void* pabyValue, size_t nLen);
    bool Write(const char*);
    bool WriteInt(int);
    bool Write(double);
    bool Write(CSLConstList);
    bool Write(const double*, size_t);

//! @cond Doxygen_Suppress
    static constexpr GUInt64 COPY_COST = 100;
//! @endcond

};

/************************************************************************/
/*                            GDALAttributeString                       */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALAttributeString final: public GDALAttribute
{
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt = GDALExtendedDataType::CreateString();
    std::string m_osValue;

protected:

    bool IRead(const GUInt64* ,
               const size_t* ,
               const GInt64* ,
               const GPtrDiff_t* ,
               const GDALExtendedDataType& bufferDataType,
               void* pDstBuffer) const override;

public:
    GDALAttributeString(const std::string& osParentName,
                  const std::string& osName,
                  const std::string& osValue);

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override;

    const GDALExtendedDataType &GetDataType() const override;
};
//! @endcond

/************************************************************************/
/*                           GDALAttributeNumeric                       */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALAttributeNumeric final: public GDALAttribute
{
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    GDALExtendedDataType m_dt;
    int m_nValue = 0;
    double m_dfValue = 0;
    std::vector<GUInt32> m_anValuesUInt32{};

protected:

    bool IRead(const GUInt64* ,
               const size_t* ,
               const GInt64* ,
               const GPtrDiff_t* ,
               const GDALExtendedDataType& bufferDataType,
               void* pDstBuffer) const override;

public:
    GDALAttributeNumeric(const std::string& osParentName,
                  const std::string& osName,
                  double dfValue);
    GDALAttributeNumeric(const std::string& osParentName,
                  const std::string& osName,
                  int nValue);
    GDALAttributeNumeric(const std::string& osParentName,
                  const std::string& osName,
                  const std::vector<GUInt32>& anValues);

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override;

    const GDALExtendedDataType &GetDataType() const override;
};
//! @endcond

/* ******************************************************************** */
/*                              GDALMDArray                             */
/* ******************************************************************** */

/**
 * Class modeling a multi-dimensional array. It has a name, values organized
 * as an array and a list of GDALAttribute.
 *
 * This is based on the <a href="https://portal.opengeospatial.org/files/81716#_hdf5_dataset">HDF5 dataset concept</a>
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALMDArray: virtual public GDALAbstractMDArray, public GDALIHasAttribute
{
    std::shared_ptr<GDALMDArray> GetView(const std::vector<GUInt64>& indices) const;

    inline std::shared_ptr<GDALMDArray> atInternal(std::vector<GUInt64>& indices) const
    {
        return GetView(indices);
    }

    template<typename... GUInt64VarArg>
    // cppcheck-suppress functionStatic
    inline std::shared_ptr<GDALMDArray> atInternal(std::vector<GUInt64>& indices,
                                            GUInt64 idx, GUInt64VarArg... tail) const
    {
        indices.push_back(idx);
        return atInternal(indices, tail...);
    }

    bool SetStatistics( GDALDataset* poDS,
                        bool bApproxStats,
                        double dfMin, double dfMax,
                        double dfMean, double dfStdDev,
                        GUInt64 nValidCount );

protected:
//! @cond Doxygen_Suppress
    GDALMDArray(const std::string& osParentName, const std::string& osName);

    virtual bool IAdviseRead(const GUInt64* arrayStartIdx,
                             const size_t* count) const;

//! @endcond

public:

    GUInt64 GetTotalCopyCost() const;

    virtual bool CopyFrom(GDALDataset* poSrcDS,
                          const GDALMDArray* poSrcArray,
                          bool bStrict,
                          GUInt64& nCurCost,
                          const GUInt64 nTotalCost,
                          GDALProgressFunc pfnProgress,
                          void * pProgressData);

    /** Return whether an array is writable; */
    virtual bool IsWritable() const = 0;

    virtual CSLConstList GetStructuralInfo() const;

    virtual const std::string& GetUnit() const;

    virtual bool SetUnit(const std::string& osUnit);

    virtual bool SetSpatialRef(const OGRSpatialReference* poSRS);

    virtual std::shared_ptr<OGRSpatialReference> GetSpatialRef() const;

    virtual const void* GetRawNoDataValue() const;

    double GetNoDataValueAsDouble(bool* pbHasNoData = nullptr) const;

    virtual bool SetRawNoDataValue(const void* pRawNoData);

    bool SetNoDataValue(double dfNoData);

    virtual double GetOffset(bool* pbHasOffset = nullptr, GDALDataType* peStorageType = nullptr) const;

    virtual double GetScale(bool* pbHasScale = nullptr, GDALDataType* peStorageType = nullptr) const;

    virtual bool SetOffset(double dfOffset, GDALDataType eStorageType = GDT_Unknown);

    virtual bool SetScale(double dfScale, GDALDataType eStorageType = GDT_Unknown);

    std::shared_ptr<GDALMDArray> GetView(const std::string& viewExpr) const;

    std::shared_ptr<GDALMDArray> operator[](const std::string& fieldName) const;

    /** Return a view of the array using integer indexing.
    *
    * Equivalent of GetView("[indices_0,indices_1,.....,indices_last]")
    *
    * Example:
    * \code
    * ar->at(0,3,2)
    * \endcode
    */
    template<typename... GUInt64VarArg>
    // cppcheck-suppress functionStatic
    std::shared_ptr<GDALMDArray> at(GUInt64 idx, GUInt64VarArg... tail) const
    {
        std::vector<GUInt64> indices;
        indices.push_back(idx);
        return atInternal(indices, tail...);
    }

    virtual std::shared_ptr<GDALMDArray> Transpose(const std::vector<int>& anMapNewAxisToOldAxis) const;

    std::shared_ptr<GDALMDArray> GetUnscaled() const;

    virtual std::shared_ptr<GDALMDArray> GetMask(CSLConstList papszOptions) const;

    virtual GDALDataset* AsClassicDataset(size_t iXDim, size_t iYDim) const;

    virtual CPLErr GetStatistics( GDALDataset* poDS,
                                  bool bApproxOK, bool bForce,
                                  double *pdfMin, double *pdfMax,
                                  double *pdfMean, double *padfStdDev,
                                  GUInt64* pnValidCount,
                                  GDALProgressFunc pfnProgress, void *pProgressData );

    virtual bool ComputeStatistics( GDALDataset* poDS,
                                    bool bApproxOK,
                                    double *pdfMin, double *pdfMax,
                                    double *pdfMean, double *pdfStdDev,
                                    GUInt64* pnValidCount,
                                    GDALProgressFunc, void *pProgressData );

    bool AdviseRead(const GUInt64* arrayStartIdx,
                    const size_t* count) const;

//! @cond Doxygen_Suppress
    static constexpr GUInt64 COPY_COST = 1000;

    bool CopyFromAllExceptValues(const GDALMDArray* poSrcArray,
                                          bool bStrict,
                                          GUInt64& nCurCost,
                                          const GUInt64 nTotalCost,
                                          GDALProgressFunc pfnProgress,
                                          void * pProgressData);
    struct Range
    {
        GUInt64 m_nStartIdx;
        GInt64  m_nIncr;
        Range(GUInt64 nStartIdx = 0, GInt64 nIncr = 0):
            m_nStartIdx(nStartIdx), m_nIncr(nIncr) {}
    };

    struct ViewSpec
    {
        std::string m_osFieldName{};

        // or

        std::vector<size_t> m_mapDimIdxToParentDimIdx{}; // of size m_dims.size()
        std::vector<Range> m_parentRanges{} ; // of size m_poParent->GetDimensionCount()
    };

    virtual std::shared_ptr<GDALMDArray> GetView(const std::string& viewExpr,
                                                 bool bRenameDimensions,
                                                 std::vector<ViewSpec>& viewSpecs) const;
//! @endcond
};


/************************************************************************/
/*                     GDALMDArrayRegularlySpaced                       */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALMDArrayRegularlySpaced: public GDALMDArray
{
    double m_dfStart;
    double m_dfIncrement;
    double m_dfOffsetInIncrement;
    GDALExtendedDataType m_dt = GDALExtendedDataType::Create(GDT_Float64);
    std::vector<std::shared_ptr<GDALDimension>> m_dims;
    std::vector<std::shared_ptr<GDALAttribute>> m_attributes{};

protected:

    bool IRead(const GUInt64* ,
               const size_t* ,
               const GInt64* ,
               const GPtrDiff_t* ,
               const GDALExtendedDataType& bufferDataType,
               void* pDstBuffer) const override;

public:
    GDALMDArrayRegularlySpaced(
                const std::string& osParentName,
                const std::string& osName,
                const std::shared_ptr<GDALDimension>& poDim,
                double dfStart, double dfIncrement,
                double dfOffsetInIncrement);

    bool IsWritable() const override { return false; }

    const std::vector<std::shared_ptr<GDALDimension>>& GetDimensions() const override;

    const GDALExtendedDataType &GetDataType() const override;

    std::vector<std::shared_ptr<GDALAttribute>> GetAttributes(CSLConstList) const override;

    void AddAttribute(const std::shared_ptr<GDALAttribute>& poAttr);
};
//! @endcond

/* ******************************************************************** */
/*                            GDALDimension                             */
/* ******************************************************************** */

/**
 * Class modeling a a dimension / axis used to index multidimensional arrays.
 * It has a name, a size (that is the number of values that can be indexed along
 * the dimension), a type (see GDALDimension::GetType()), a direction
 * (see GDALDimension::GetDirection()), a unit and can optionally point to a GDALMDArray variable,
 * typically one-dimensional, describing the values taken by the dimension.
 * For a georeferenced GDALMDArray and its X dimension, this will be typically
 * the values of the easting/longitude for each grid point.
 *
 * @since GDAL 3.1
 */
class CPL_DLL GDALDimension
{
public:
//! @cond Doxygen_Suppress
    GDALDimension(const std::string& osParentName,
                  const std::string& osName,
                  const std::string& osType,
                  const std::string& osDirection,
                  GUInt64 nSize);
//! @endcond

    virtual ~GDALDimension();

    /** Return the name.
     *
     * This is the same as the C function GDALDimensionGetName()
     */
    const std::string& GetName() const { return m_osName; }

    /** Return the full name.
     *
     * This is the same as the C function GDALDimensionGetFullName()
     */
    const std::string& GetFullName() const { return m_osFullName; }

    /** Return the axis type.
     *
     * Predefined values are:
     * HORIZONTAL_X, HORIZONTAL_Y, VERTICAL, TEMPORAL, PARAMETRIC
     * Other values might be returned. Empty value means unknown.
     *
     * This is the same as the C function GDALDimensionGetType()
     */
    const std::string& GetType() const { return m_osType; }

    /** Return the axis direction.
     *
     * Predefined values are:
     * EAST, WEST, SOUTH, NORTH, UP, DOWN, FUTURE, PAST
     * Other values might be returned. Empty value means unknown.
     *
     * This is the same as the C function GDALDimensionGetDirection()
     */
    const std::string& GetDirection() const { return m_osDirection; }

    /** Return the size, that is the number of values along the dimension.
     *
     * This is the same as the C function GDALDimensionGetSize()
     */
    GUInt64 GetSize() const { return m_nSize; }

    virtual std::shared_ptr<GDALMDArray> GetIndexingVariable() const;

    virtual bool SetIndexingVariable(std::shared_ptr<GDALMDArray> poIndexingVariable);

protected:
//! @cond Doxygen_Suppress
    std::string m_osName;
    std::string m_osFullName;
    std::string m_osType;
    std::string m_osDirection;
    GUInt64 m_nSize;
//! @endcond
};


/************************************************************************/
/*                   GDALDimensionWeakIndexingVar()                     */
/************************************************************************/

//! @cond Doxygen_Suppress
class CPL_DLL GDALDimensionWeakIndexingVar: public GDALDimension
{
    std::weak_ptr<GDALMDArray> m_poIndexingVariable{};

public:
    GDALDimensionWeakIndexingVar(const std::string& osParentName,
                  const std::string& osName,
                  const std::string& osType,
                  const std::string& osDirection,
                  GUInt64 nSize);

    std::shared_ptr<GDALMDArray> GetIndexingVariable() const override;

    bool SetIndexingVariable(std::shared_ptr<GDALMDArray> poIndexingVariable) override;
};
//! @endcond

/* ==================================================================== */
/*      An assortment of overview related stuff.                        */
/* ==================================================================== */

//! @cond Doxygen_Suppress
/* Only exported for drivers as plugin. Signature may change */
CPLErr CPL_DLL
GDALRegenerateOverviewsMultiBand(int nBands, GDALRasterBand** papoSrcBands,
                                 int nOverviews,
                                 GDALRasterBand*** papapoOverviewBands,
                                 const char * pszResampling,
                                 GDALProgressFunc pfnProgress, void * pProgressData );

typedef CPLErr (*GDALResampleFunction)
                      ( double dfXRatioDstToSrc,
                        double dfYRatioDstToSrc,
                        double dfSrcXDelta,
                        double dfSrcYDelta,
                        GDALDataType eWrkDataType,
                        const void * pChunk,
                        const GByte * pabyChunkNodataMask,
                        int nChunkXOff, int nChunkXSize,
                        int nChunkYOff, int nChunkYSize,
                        int nDstXOff, int nDstXOff2,
                        int nDstYOff, int nDstYOff2,
                        GDALRasterBand * poOverview,
                        void** ppDstBuffer,
                        GDALDataType* peDstBufferDataType,
                        const char * pszResampling,
                        int bHasNoData, float fNoDataValue,
                        GDALColorTable* poColorTable,
                        GDALDataType eSrcDataType,
                        bool bPropagateNoData );

GDALResampleFunction GDALGetResampleFunction(const char* pszResampling,
                                                 int* pnRadius);

GDALDataType GDALGetOvrWorkDataType(const char* pszResampling,
                                        GDALDataType eSrcDataType);

CPL_C_START

CPLErr CPL_DLL
HFAAuxBuildOverviews( const char *pszOvrFilename, GDALDataset *poParentDS,
                      GDALDataset **ppoDS,
                      int nBands, int *panBandList,
                      int nNewOverviews, int *panNewOverviewList,
                      const char *pszResampling,
                      GDALProgressFunc pfnProgress,
                      void *pProgressData );

CPLErr CPL_DLL
GTIFFBuildOverviews( const char * pszFilename,
                     int nBands, GDALRasterBand **papoBandList,
                     int nOverviews, int * panOverviewList,
                     const char * pszResampling,
                     GDALProgressFunc pfnProgress, void * pProgressData );

int CPL_DLL GDALBandGetBestOverviewLevel(GDALRasterBand* poBand,
                                         int &nXOff, int &nYOff,
                                         int &nXSize, int &nYSize,
                                         int nBufXSize, int nBufYSize) CPL_WARN_DEPRECATED("Use GDALBandGetBestOverviewLevel2 instead");
int CPL_DLL GDALBandGetBestOverviewLevel2(GDALRasterBand* poBand,
                                         int &nXOff, int &nYOff,
                                         int &nXSize, int &nYSize,
                                         int nBufXSize, int nBufYSize,
                                         GDALRasterIOExtraArg* psExtraArg);

int CPL_DLL GDALOvLevelAdjust( int nOvLevel, int nXSize ) CPL_WARN_DEPRECATED("Use GDALOvLevelAdjust2 instead");
int CPL_DLL GDALOvLevelAdjust2( int nOvLevel, int nXSize, int nYSize );
int CPL_DLL GDALComputeOvFactor( int nOvrXSize, int nRasterXSize,
                                 int nOvrYSize, int nRasterYSize );

GDALDataset CPL_DLL *
GDALFindAssociatedAuxFile( const char *pszBasefile, GDALAccess eAccess,
                           GDALDataset *poDependentDS );

/* ==================================================================== */
/*  Infrastructure to check that dataset characteristics are valid      */
/* ==================================================================== */

int CPL_DLL GDALCheckDatasetDimensions( int nXSize, int nYSize );
int CPL_DLL GDALCheckBandCount( int nBands, int bIsZeroAllowed );

/* Internal use only */

/* CPL_DLL exported, but only for in-tree drivers that can be built as plugins */
int CPL_DLL GDALReadWorldFile2( const char *pszBaseFilename, const char *pszExtension,
                                double *padfGeoTransform, char** papszSiblingFiles,
                                char** ppszWorldFileNameOut);
int GDALReadTabFile2( const char * pszBaseFilename,
                      double *padfGeoTransform, char **ppszWKT,
                      int *pnGCPCount, GDAL_GCP **ppasGCPs,
                      char** papszSiblingFiles, char** ppszTabFileNameOut );

void CPL_DLL GDALCopyRasterIOExtraArg(GDALRasterIOExtraArg* psDestArg,
                                      GDALRasterIOExtraArg* psSrcArg);

CPL_C_END

void GDALNullifyOpenDatasetsList();
CPLMutex** GDALGetphDMMutex();
CPLMutex** GDALGetphDLMutex();
void GDALNullifyProxyPoolSingleton();
void GDALSetResponsiblePIDForCurrentThread(GIntBig responsiblePID);
GIntBig GDALGetResponsiblePIDForCurrentThread();

CPLString GDALFindAssociatedFile( const char *pszBasename, const char *pszExt,
                                  CSLConstList papszSiblingFiles, int nFlags );

CPLErr CPL_DLL EXIFExtractMetadata(char**& papszMetadata,
                           void *fpL, int nOffset,
                           int bSwabflag, int nTIFFHEADER,
                           int& nExifOffset, int& nInterOffset, int& nGPSOffset);

int GDALValidateOpenOptions( GDALDriverH hDriver,
                             const char* const* papszOptionOptions);
int GDALValidateOptions( const char* pszOptionList,
                         const char* const* papszOptionsToValidate,
                         const char* pszErrorMessageOptionType,
                         const char* pszErrorMessageContainerName);

GDALRIOResampleAlg GDALRasterIOGetResampleAlg(const char* pszResampling);
const char* GDALRasterIOGetResampleAlg(GDALRIOResampleAlg eResampleAlg);

void GDALRasterIOExtraArgSetResampleAlg(GDALRasterIOExtraArg* psExtraArg,
                                        int nXSize, int nYSize,
                                        int nBufXSize, int nBufYSize);


GDALDataset* GDALCreateOverviewDataset(GDALDataset* poDS, int nOvrLevel,
                                       int bThisLevelOnly);

// Should cover particular cases of #3573, #4183, #4506, #6578
// Behavior is undefined if fVal1 or fVal2 are NaN (should be tested before
// calling this function)
template<class T> inline bool ARE_REAL_EQUAL(T fVal1, T fVal2, int ulp = 2)
{
    return fVal1 == fVal2 || /* Should cover infinity */
           std::abs(fVal1 - fVal2) < std::numeric_limits<float>::epsilon() * std::abs(fVal1+fVal2) * ulp;
}

double GDALAdjustNoDataCloseToFloatMax(double dfVal);

#define DIV_ROUND_UP(a, b) ( ((a) % (b)) == 0 ? ((a) / (b)) : (((a) / (b)) + 1) )

// Number of data samples that will be used to compute approximate statistics
// (minimum value, maximum value, etc.)
#define GDALSTAT_APPROX_NUMSAMPLES 2500

void GDALSerializeGCPListToXML( CPLXMLNode* psParentNode,
                                GDAL_GCP* pasGCPList,
                                int nGCPCount,
                                const OGRSpatialReference* poGCP_SRS );
void GDALDeserializeGCPListFromXML( CPLXMLNode* psGCPList,
                                    GDAL_GCP** ppasGCPList,
                                    int* pnGCPCount,
                                    OGRSpatialReference** ppoGCP_SRS );

void GDALSerializeOpenOptionsToXML( CPLXMLNode* psParentNode, char** papszOpenOptions);
char** GDALDeserializeOpenOptionsFromXML( CPLXMLNode* psParentNode );

int GDALCanFileAcceptSidecarFile(const char* pszFilename);

bool GDALCanReliablyUseSiblingFileList(const char* pszFilename);

bool CPL_DLL GDALIsDriverDeprecatedForGDAL35StillEnabled(const char* pszDriverName, const char* pszExtraMsg = "");

//! @endcond

#endif /* ndef GDAL_PRIV_H_INCLUDED */
