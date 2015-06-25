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
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <vector>
#include <map>
#include "ogr_core.h"

#define GMO_VALID                0x0001
#define GMO_IGNORE_UNIMPLEMENTED 0x0002
#define GMO_SUPPORT_MD           0x0004
#define GMO_SUPPORT_MDMD         0x0008
#define GMO_MD_DIRTY             0x0010
#define GMO_PAM_CLASS            0x0020

/************************************************************************/
/*                       GDALMultiDomainMetadata                        */
/************************************************************************/

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
};

/* ******************************************************************** */
/*                           GDALMajorObject                            */
/*                                                                      */
/*      Base class providing metadata, description and other            */
/*      services shared by major objects.                               */
/* ******************************************************************** */

//! Object with metadata.

class CPL_DLL GDALMajorObject
{
  protected:
    int                 nFlags; // GMO_* flags. 
    CPLString           sDescription;
    GDALMultiDomainMetadata oMDMD;
    
    char               **BuildMetadataDomainList(char** papszList, int bCheckNonEmpty, ...) CPL_NULL_TERMINATED;
    
  public:
                        GDALMajorObject();
    virtual            ~GDALMajorObject();

    int                 GetMOFlags();
    void                SetMOFlags(int nFlagsIn);
                        
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
};

/* ******************************************************************** */
/*                         GDALDefaultOverviews                         */
/* ******************************************************************** */
class CPL_DLL GDALDefaultOverviews
{
    friend class GDALDataset;

    GDALDataset *poDS;
    GDALDataset *poODS;
    
    CPLString   osOvrFilename;

    int         bOvrIsAux;

    int         bCheckedForMask;
    int         bOwnMaskDS;
    GDALDataset *poMaskDS;

    // for "overview datasets" we record base level info so we can 
    // find our way back to get overview masks.
    GDALDataset *poBaseDS;

    // Stuff for deferred initialize/overviewscans...
    bool        bCheckedForOverviews;
    void        OverviewScan();
    char       *pszInitName;
    int         bInitNameIsOVR;
    char      **papszInitSiblingFiles;

  public:
               GDALDefaultOverviews();
               ~GDALDefaultOverviews();

    void       Initialize( GDALDataset *poDSIn, const char *pszName = NULL, 
                           char **papszSiblingFiles = NULL,
                           int bNameIsOVR = FALSE );

    int        IsInitialized();

    int        CloseDependentDatasets();

    // Overview Related

    int        GetOverviewCount(int);
    GDALRasterBand *GetOverview(int,int);

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

    int        HaveMaskFile( char **papszSiblings = NULL, 
                             const char *pszBasename = NULL );

    char**     GetSiblingFiles() { return papszInitSiblingFiles; }
};

/* ******************************************************************** */
/*                             GDALOpenInfo                             */
/*                                                                      */
/*      Structure of data about dataset for open functions.             */
/* ******************************************************************** */

class CPL_DLL GDALOpenInfo
{
    int         bHasGotSiblingFiles;
    char        **papszSiblingFiles;
    int         nHeaderBytesTried;

  public:
                GDALOpenInfo( const char * pszFile, int nOpenFlagsIn,
                              char **papszSiblingFiles = NULL );
                ~GDALOpenInfo( void );

    char        *pszFilename;
    char**      papszOpenOptions;

    GDALAccess  eAccess;
    int         nOpenFlags;

    int         bStatOK;
    int         bIsDirectory;

    VSILFILE   *fpL;

    int         nHeaderBytes;
    GByte       *pabyHeader;

    int         TryToIngest(int nBytes);
    char      **GetSiblingFiles();
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
typedef struct GDALSQLParseInfo GDALSQLParseInfo;

#ifdef DETECT_OLD_IRASTERIO
typedef void signature_changed;
#endif

#ifdef GDAL_COMPILATION
#define OPTIONAL_OUTSIDE_GDAL(val)
#else
#define OPTIONAL_OUTSIDE_GDAL(val) = val
#endif

//! A set of associated raster bands, usually from one file.

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
    
    void AddToDatasetOpenList();

  protected:
    GDALDriver  *poDriver;
    GDALAccess  eAccess;

    // Stored raster information.
    int         nRasterXSize;
    int         nRasterYSize;
    int         nBands;
    GDALRasterBand **papoBands;

    int         nOpenFlags;

    int         nRefCount;
    GByte       bForceCachedIO;
    GByte       bShared;
    GByte       bIsInternal;
    GByte       bSuppressOnClose;

                GDALDataset(void);

    void        RasterInitialize( int, int );
    void        SetBand( int, GDALRasterBand * );

    GDALDefaultOverviews oOvManager;
    
    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );

#ifdef DETECT_OLD_IRASTERIO
    virtual signature_changed IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int ) {};
#endif

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, GSpacing, GSpacing, GSpacing,
                              GDALRasterIOExtraArg* psExtraArg );

    CPLErr BlockBasedRasterIO( GDALRWFlag, int, int, int, int,
                               void *, int, int, GDALDataType,
                               int, int *, GSpacing, GSpacing, GSpacing,
                               GDALRasterIOExtraArg* psExtraArg );
    void   BlockBasedFlushCache();

    CPLErr ValidateRasterIOOrAdviseReadParameters(
                               const char* pszCallingFunc,
                               int* pbStopProcessingOnCENone,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               int nBandCount, int *panBandMap);

    virtual int         CloseDependentDatasets();
    
    int                 ValidateLayerCreationOptions( const char* const* papszLCO );
    
    char            **papszOpenOptions;

    friend class GDALRasterBand;
    
    int                 EnterReadWrite(GDALRWFlag eRWFlag);
    void                LeaveReadWrite();
    
    
  public:
    virtual     ~GDALDataset();

    int         GetRasterXSize( void );
    int         GetRasterYSize( void );
    int         GetRasterCount( void );
    GDALRasterBand *GetRasterBand( int );

    virtual void FlushCache(void);

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual CPLErr        AddBand( GDALDataType eType, 
                                   char **papszOptions=NULL );

    virtual void *GetInternalHandle( const char * );
    virtual GDALDriver *GetDriver(void);
    virtual char      **GetFileList(void);
    
    virtual     const char* GetDriverName();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
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

    CPLErr      RasterIO( GDALRWFlag, int, int, int, int,
                          void *, int, int, GDALDataType,
                          int, int *, GSpacing, GSpacing, GSpacing,
                          GDALRasterIOExtraArg* psExtraArg
#ifndef DOXYGEN_SKIP
                          OPTIONAL_OUTSIDE_GDAL(NULL)
#endif
                          );

    int           Reference();
    int           Dereference();
    GDALAccess    GetAccess() { return eAccess; }

    int           GetShared();
    void          MarkAsShared();
    
    void          MarkSuppressOnClose() { bSuppressOnClose = TRUE; }
    
    char        **GetOpenOptions() { return papszOpenOptions; }

    static GDALDataset **GetOpenDatasets( int *pnDatasetCount );

    CPLErr BuildOverviews( const char *, int, int *,
                           int, int *, GDALProgressFunc, void * );

    void ReportError(CPLErr eErrClass, int err_no, const char *fmt, ...)  CPL_PRINT_FUNC_FORMAT (4, 5);

private:
    CPLMutex        *m_hMutex;

    OGRLayer*       BuildLayerFromSelectInfo(swq_select* psSelectInfo,
                                             OGRGeometry *poSpatialFilter,
                                             const char *pszDialect,
                                             swq_select_parse_options* poSelectParseOptions);

  public:

    virtual int         GetLayerCount();
    virtual OGRLayer    *GetLayer(int);
    virtual OGRLayer    *GetLayerByName(const char *);
    virtual OGRErr      DeleteLayer(int);

    virtual int         TestCapability( const char * );

    virtual OGRLayer   *CreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );
    virtual OGRLayer   *CopyLayer( OGRLayer *poSrcLayer, 
                                   const char *pszNewName, 
                                   char **papszOptions = NULL );

    virtual OGRStyleTable *GetStyleTable();
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );
                            
    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

    virtual OGRLayer *  ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poResultsSet );

    int                 GetRefCount() const;
    int                 GetSummaryRefCount() const;
    OGRErr              Release();

    virtual OGRErr      StartTransaction(int bForce=FALSE);
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();
    
    static int          IsGenericSQLDialect(const char* pszDialect);
    
    // Semi-public methods. Only to be used by in-tree drivers.
    GDALSQLParseInfo*   BuildParseInfo(swq_select* psSelectInfo,
                                       swq_select_parse_options* poSelectParseOptions);
    void                DestroyParseInfo(GDALSQLParseInfo* psParseInfo );
    OGRLayer *          ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect,
                                    swq_select_parse_options* poSelectParseOptions);

  protected:

    virtual OGRLayer   *ICreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );

    OGRErr              ProcessSQLCreateIndex( const char * );
    OGRErr              ProcessSQLDropIndex( const char * );
    OGRErr              ProcessSQLDropTable( const char * );
    OGRErr              ProcessSQLAlterTableAddColumn( const char * );
    OGRErr              ProcessSQLAlterTableDropColumn( const char * );
    OGRErr              ProcessSQLAlterTableAlterColumn( const char * );
    OGRErr              ProcessSQLAlterTableRenameColumn( const char * );

    OGRStyleTable      *m_poStyleTable;
};

/* ******************************************************************** */
/*                           GDALRasterBlock                            */
/* ******************************************************************** */

//! A single raster block in the block cache.
// And the global block manager that manages a least-recently-used list of
// blocks from various datasets/bands

class CPL_DLL GDALRasterBlock
{
    friend class GDALAbstractBandBlockCache;
    
    GDALDataType        eType;
    
    int                 bDirty;
    volatile int        nLockCount;

    int                 nXOff;
    int                 nYOff;
       
    int                 nXSize;
    int                 nYSize;
    
    void                *pData;

    GDALRasterBand      *poBand;
    
    GDALRasterBlock     *poNext;
    GDALRasterBlock     *poPrevious;
    
    int                  bMustDetach;

    void        Detach_unlocked( void );
    void        Touch_unlocked( void );
    
    void        RecycleFor( int nXOffIn, int nYOffIn );

  public:
                GDALRasterBlock( GDALRasterBand *, int, int );
                GDALRasterBlock( int nXOffIn, int nYOffIn ); /* only for lookup purpose */
    virtual     ~GDALRasterBlock();

    CPLErr      Internalize( void );
    void        Touch( void );      
    void        MarkDirty( void );  
    void        MarkClean( void );
    int         AddLock( void ) { return CPLAtomicInc(&nLockCount); }
    int         DropLock( void ) { return CPLAtomicDec(&nLockCount); }
    void        Detach();

    CPLErr      Write();

    GDALDataType GetDataType() { return eType; }
    int         GetXOff() { return nXOff; }
    int         GetYOff() { return nYOff; }
    int         GetXSize() { return nXSize; }
    int         GetYSize() { return nYSize; }
    int         GetDirty() { return bDirty; }

    void        *GetDataRef( void ) { return pData; }
    int          GetBlockSize() { return nXSize * nYSize * (GDALGetDataTypeSize(eType) / 8); }

    int          TakeLock();
    int          DropLockForRemovalFromStorage();

    /// @brief Accessor to source GDALRasterBand object.
    /// @return source raster band of the raster block.
    GDALRasterBand *GetBand() { return poBand; }

    static void FlushDirtyBlocks();
    static int  FlushCacheBlock(int bDirtyBlocksOnly = FALSE);
    static void Verify();

    /* Should only be called by GDALDestroyDriverManager() */
    static void DestroyRBMutex();
};

/* ******************************************************************** */
/*                             GDALColorTable                           */
/* ******************************************************************** */

/*! A color table / palette. */

class CPL_DLL GDALColorTable
{
    GDALPaletteInterp eInterp;

    std::vector<GDALColorEntry> aoEntries;

public:
                GDALColorTable( GDALPaletteInterp = GPI_RGB );
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
};

/* ******************************************************************** */
/*                       GDALAbstractBandBlockCache                     */
/* ******************************************************************** */

//! This manages how a raster band store its cached block.
// CPL_DLL is just technical here. This is really a private concept
// only used by GDALRasterBand implementation.

class CPL_DLL GDALAbstractBandBlockCache
{
        // List of blocks that can be freed or recycled, and its lock
        CPLLock          *hSpinLock;
        GDALRasterBlock  *psListBlocksToFree;

        // Band keep alive counter, and its lock & condition
        CPLCond          *hCond;
        CPLMutex         *hCondMutex;
        volatile int      nKeepAliveCounter;

    protected:
        GDALRasterBand   *poBand;

        void              FreeDanglingBlocks();
        void              UnreferenceBlockBase();
        void              WaitKeepAliveCounter();

    public:
                     GDALAbstractBandBlockCache(GDALRasterBand* poBand);
            virtual ~GDALAbstractBandBlockCache();

            GDALRasterBlock* CreateBlock(int nXBlockOff, int nYBlockOff);
            void             AddBlockToFreeList( GDALRasterBlock * );
            
            virtual int              Init() = 0;
            virtual int              IsInitOK() = 0;
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

/* ******************************************************************** */
/*                            GDALRasterBand                            */
/* ******************************************************************** */

//! A single raster band (or channel).

class CPL_DLL GDALRasterBand : public GDALMajorObject
{
  private:
    friend class GDALArrayBandBlockCache;
    friend class GDALHashSetBandBlockCache;
    friend class GDALRasterBlock;

    CPLErr eFlushBlockErr;
    GDALAbstractBandBlockCache* poBandBlockCache;

    void           SetFlushBlockErr( CPLErr eErr );
    CPLErr         UnreferenceBlock( GDALRasterBlock* poBlock );

  protected:
    GDALDataset *poDS;
    int         nBand; /* 1 based */

    int         nRasterXSize;
    int         nRasterYSize;
    
    GDALDataType eDataType;
    GDALAccess  eAccess;

    /* stuff related to blocking, and raster cache */
    int         nBlockXSize;
    int         nBlockYSize;
    int         nBlocksPerRow;
    int         nBlocksPerColumn;

    int         nBlockReads;
    int         bForceCachedIO;

    GDALRasterBand *poMask;
    bool        bOwnMask;
    int         nMaskFlags;

    void        InvalidateMaskBand();

    friend class GDALDataset;
    friend class GDALProxyRasterBand;
    friend class GDALDefaultOverviews;

    CPLErr RasterIOResampled( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg );

    int          EnterReadWrite(GDALRWFlag eRWFlag);
    void         LeaveReadWrite();

  protected:
    virtual CPLErr IReadBlock( int, int, void * ) = 0;
    virtual CPLErr IWriteBlock( int, int, void * );

#ifdef DETECT_OLD_IRASTERIO
    virtual signature_changed IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int ) {};
#endif

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg );
    CPLErr         OverviewRasterIO( GDALRWFlag, int, int, int, int,
                                     void *, int, int, GDALDataType,
                                     GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg );

    int            InitBlockInfo();

    CPLErr         AdoptBlock( GDALRasterBlock * );
    GDALRasterBlock *TryGetLockedBlockRef( int nXBlockOff, int nYBlockYOff );
    void           AddBlockToFreeList( GDALRasterBlock * );

  public:
                GDALRasterBand();
                
    virtual     ~GDALRasterBand();

    int         GetXSize();
    int         GetYSize();
    int         GetBand();
    GDALDataset*GetDataset();

    GDALDataType GetRasterDataType( void );
    void        GetBlockSize( int *, int * );
    GDALAccess  GetAccess();
    
    CPLErr      RasterIO( GDALRWFlag, int, int, int, int,
                          void *, int, int, GDALDataType,
                          GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg
#ifndef DOXYGEN_SKIP
                          OPTIONAL_OUTSIDE_GDAL(NULL)
#endif
                          );
    CPLErr      ReadBlock( int, int, void * );

    CPLErr      WriteBlock( int, int, void * );

    GDALRasterBlock *GetLockedBlockRef( int nXBlockOff, int nYBlockOff, 
                                        int bJustInitialize = FALSE );
    CPLErr      FlushBlock( int, int, int bWriteDirtyBlock = TRUE );

    unsigned char*  GetIndexColorTranslationTo(/* const */ GDALRasterBand* poReferenceBand,
                                               unsigned char* pTranslationTable = NULL,
                                               int* pApproximateMatching = NULL);

    // New OpengIS CV_SampleDimension stuff.

    virtual CPLErr FlushCache();
    virtual char **GetCategoryNames();
    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum(int *pbSuccess = NULL );
    virtual double GetOffset( int *pbSuccess = NULL );
    virtual double GetScale( int *pbSuccess = NULL );
    virtual const char *GetUnitType();
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr Fill(double dfRealValue, double dfImaginaryValue = 0);

    virtual CPLErr SetCategoryNames( char ** );
    virtual CPLErr SetNoDataValue( double );
    virtual CPLErr DeleteNoDataValue();
    virtual CPLErr SetColorTable( GDALColorTable * ); 
    virtual CPLErr SetColorInterpretation( GDALColorInterp );
    virtual CPLErr SetOffset( double );
    virtual CPLErr SetScale( double );
    virtual CPLErr SetUnitType( const char * );

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

    virtual int HasArbitraryOverviews();
    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);
    virtual GDALRasterBand *GetRasterSampleOverview( GUIntBig );
    virtual CPLErr BuildOverviews( const char *, int, int *,
                                   GDALProgressFunc, void * );

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               GDALDataType eDT, char **papszOptions );

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
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * );

    virtual GDALRasterBand *GetMaskBand();
    virtual int             GetMaskFlags();
    virtual CPLErr          CreateMaskBand( int nFlagsIn );

    virtual CPLVirtualMem  *GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                               int *pnPixelSpace,
                                               GIntBig *pnLineSpace,
                                               char **papszOptions );

    void ReportError(CPLErr eErrClass, int err_no, const char *fmt, ...)  CPL_PRINT_FUNC_FORMAT (4, 5);
};

/* ******************************************************************** */
/*                         GDALAllValidMaskBand                         */
/* ******************************************************************** */

class CPL_DLL GDALAllValidMaskBand : public GDALRasterBand
{
  protected:
    virtual CPLErr IReadBlock( int, int, void * );

  public:
                GDALAllValidMaskBand( GDALRasterBand * );
    virtual     ~GDALAllValidMaskBand();

    virtual GDALRasterBand *GetMaskBand();
    virtual int             GetMaskFlags();
};

/* ******************************************************************** */
/*                         GDALNoDataMaskBand                           */
/* ******************************************************************** */

class CPL_DLL GDALNoDataMaskBand : public GDALRasterBand
{
    double          dfNoDataValue;
    GDALRasterBand *poParent;

  protected:
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg );

  public:
                GDALNoDataMaskBand( GDALRasterBand * );
    virtual     ~GDALNoDataMaskBand();
};

/* ******************************************************************** */
/*                  GDALNoDataValuesMaskBand                            */
/* ******************************************************************** */

class CPL_DLL GDALNoDataValuesMaskBand : public GDALRasterBand
{
    double      *padfNodataValues;

  protected:
    virtual CPLErr IReadBlock( int, int, void * );

  public:
                GDALNoDataValuesMaskBand( GDALDataset * );
    virtual     ~GDALNoDataValuesMaskBand();
};

/* ******************************************************************** */
/*                         GDALRescaledAlphaBand                        */
/* ******************************************************************** */

class GDALRescaledAlphaBand : public GDALRasterBand
{
    GDALRasterBand *poParent;
    void           *pTemp;

  protected:
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing, GSpacing, GDALRasterIOExtraArg* psExtraArg );

  public:
                GDALRescaledAlphaBand( GDALRasterBand * );
    virtual     ~GDALRescaledAlphaBand();
};

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
                        ~GDALDriver();

    virtual CPLErr      SetMetadataItem( const char * pszName,
                                 const char * pszValue,
                                 const char * pszDomain = "" );

/* -------------------------------------------------------------------- */
/*      Public C++ methods.                                             */
/* -------------------------------------------------------------------- */
    GDALDataset         *Create( const char * pszName,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszOptions ) CPL_WARN_UNUSED_RESULT;

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
    GDALDataset         *(*pfnOpen)( GDALOpenInfo * );

    GDALDataset         *(*pfnCreate)( const char * pszName,
                                       int nXSize, int nYSize, int nBands,
                                       GDALDataType eType,
                                       char ** papszOptions );

    CPLErr              (*pfnDelete)( const char * pszName );

    GDALDataset         *(*pfnCreateCopy)( const char *, GDALDataset *, 
                                           int, char **,
                                           GDALProgressFunc pfnProgress, 
                                           void * pProgressData );

    void                *pDriverData;

    void                (*pfnUnloadDriver)(GDALDriver *);

    /* Return 1 if the passed file is certainly recognized by the driver */
    /* Return 0 if the passed file is certainly NOT recognized by the driver */
    /* Return -1 if the passed file may be or may not be recognized by the driver,
       and that a potentially costly test must be done with pfnOpen */
    int                 (*pfnIdentify)( GDALOpenInfo * );

    CPLErr              (*pfnRename)( const char * pszNewName,
                                      const char * pszOldName );
    CPLErr              (*pfnCopyFiles)( const char * pszNewName,
                                         const char * pszOldName );

    /* For legacy OGR drivers */
    GDALDataset         *(*pfnOpenWithDriverArg)( GDALDriver*, GDALOpenInfo * );
    GDALDataset         *(*pfnCreateVectorOnly)( GDALDriver*,
                                                 const char * pszName,
                                                 char ** papszOptions );
    CPLErr              (*pfnDeleteDataSource)( GDALDriver*,
                                                 const char * pszName );

/* -------------------------------------------------------------------- */
/*      Helper methods.                                                 */
/* -------------------------------------------------------------------- */
    GDALDataset         *DefaultCreateCopy( const char *, GDALDataset *, 
                                            int, char **,
                                            GDALProgressFunc pfnProgress, 
                                            void * pProgressData ) CPL_WARN_UNUSED_RESULT;
    static CPLErr        DefaultCopyMasks( GDALDataset *poSrcDS, 
                                           GDALDataset *poDstDS, 
                                           int bStrict );
    static CPLErr       QuietDelete( const char * pszName );

    CPLErr              DefaultRename( const char * pszNewName,
                                       const char * pszOldName );
    CPLErr              DefaultCopyFiles( const char * pszNewName,
                                          const char * pszOldName );
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
    int         nDrivers;
    GDALDriver  **papoDrivers;
    std::map<CPLString, GDALDriver*> oMapNameToDrivers;
    
    GDALDriver  *GetDriver_unlocked( int iDriver )
            { return (iDriver >= 0 && iDriver < nDrivers) ? papoDrivers[iDriver] : NULL; }
    
    GDALDriver  *GetDriverByName_unlocked( const char * pszName )
            { return oMapNameToDrivers[CPLString(pszName).toupper()]; }
    
 public:
                GDALDriverManager();
                ~GDALDriverManager();
                
    int         GetDriverCount( void );
    GDALDriver  *GetDriver( int );
    GDALDriver  *GetDriverByName( const char * );

    int         RegisterDriver( GDALDriver * );
    void        DeregisterDriver( GDALDriver * );

    void        AutoLoadDrivers();
    void        AutoSkipDrivers();
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
  protected:
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

  public:
    GDALAsyncReader();
    virtual ~GDALAsyncReader();

    GDALDataset* GetGDALDataset() {return poDS;}
    int GetXOffset() {return nXOff;}
    int GetYOffset() {return nYOff;}
    int GetXSize() {return nXSize;}
    int GetYSize() {return nYSize;}
    void * GetBuffer() {return pBuf;}
    int GetBufferXSize() {return nBufXSize;}
    int GetBufferYSize() {return nBufYSize;}
    GDALDataType GetBufferType() {return eBufType;}
    int GetBandCount() {return nBandCount;}
    int* GetBandMap() {return panBandMap;}
    int GetPixelSpace() {return nPixelSpace;}
    int GetLineSpace() {return nLineSpace;}
    int GetBandSpace() {return nBandSpace;}

    virtual GDALAsyncStatusType 
        GetNextUpdatedRegion(double dfTimeout,
                             int* pnBufXOff, int* pnBufYOff,
                             int* pnBufXSize, int* pnBufYSize) = 0;
    virtual int LockBuffer( double dfTimeout = -1.0 );
    virtual void UnlockBuffer();
};

/* ==================================================================== */
/*      An assortment of overview related stuff.                        */
/* ==================================================================== */

/* Not a public symbol for the moment */
CPLErr 
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
                        void * pChunk,
                        GByte * pabyChunkNodataMask,
                        int nChunkXOff, int nChunkXSize,
                        int nChunkYOff, int nChunkYSize,
                        int nDstXOff, int nDstXOff2,
                        int nDstYOff, int nDstYOff2,
                        GDALRasterBand * poOverview,
                        const char * pszResampling,
                        int bHasNoData, float fNoDataValue,
                        GDALColorTable* poColorTable,
                        GDALDataType eSrcDataType);

GDALResampleFunction GDALGetResampleFunction(const char* pszResampling,
                                                 int* pnRadius);
GDALDataType GDALGetOvrWorkDataType(const char* pszResampling,
                                        GDALDataType eSrcDataType);

CPL_C_START

#ifndef WIN32CE

CPLErr CPL_DLL
HFAAuxBuildOverviews( const char *pszOvrFilename, GDALDataset *poParentDS,
                      GDALDataset **ppoDS,
                      int nBands, int *panBandList,
                      int nNewOverviews, int *panNewOverviewList, 
                      const char *pszResampling, 
                      GDALProgressFunc pfnProgress, 
                      void *pProgressData );

#endif /* WIN32CE */

CPLErr CPL_DLL 
GTIFFBuildOverviews( const char * pszFilename,
                     int nBands, GDALRasterBand **papoBandList, 
                     int nOverviews, int * panOverviewList,
                     const char * pszResampling, 
                     GDALProgressFunc pfnProgress, void * pProgressData );

CPLErr CPL_DLL
GDALDefaultBuildOverviews( GDALDataset *hSrcDS, const char * pszBasename,
                           const char * pszResampling, 
                           int nOverviews, int * panOverviewList,
                           int nBands, int * panBandList,
                           GDALProgressFunc pfnProgress, void * pProgressData);
                           
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
/*      Misc functions.                                                 */
/* ==================================================================== */

CPLErr CPL_DLL GDALParseGMLCoverage( CPLXMLNode *psTree, 
                                     int *pnXSize, int *pnYSize,
                                     double *padfGeoTransform,
                                     char **ppszProjection );

/* ==================================================================== */
/*  Infrastructure to check that dataset characteristics are valid      */
/* ==================================================================== */

int CPL_DLL GDALCheckDatasetDimensions( int nXSize, int nYSize );
int CPL_DLL GDALCheckBandCount( int nBands, int bIsZeroAllowed );


// Test if 2 floating point values match. Useful when comparing values
// stored as a string at some point. See #3573, #4183, #4506
#define ARE_REAL_EQUAL(dfVal1, dfVal2) \
 (dfVal1 == dfVal2 || fabs(dfVal1 - dfVal2) < 1e-10 || (dfVal2 != 0 && fabs(1 - dfVal1 / dfVal2) < 1e-10 ))

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
GDALDriver* GDALGetAPIPROXYDriver();
void GDALSetResponsiblePIDForCurrentThread(GIntBig responsiblePID);
GIntBig GDALGetResponsiblePIDForCurrentThread();

CPLString GDALFindAssociatedFile( const char *pszBasename, const char *pszExt,
                                  char **papszSiblingFiles, int nFlags );

CPLErr EXIFExtractMetadata(char**& papszMetadata,
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

void GDALRasterIOExtraArgSetResampleAlg(GDALRasterIOExtraArg* psExtraArg,
                                        int nXSize, int nYSize,
                                        int nBufXSize, int nBufYSize);

/* CPL_DLL exported, but only for gdalwarp */
GDALDataset CPL_DLL* GDALCreateOverviewDataset(GDALDataset* poDS, int nOvrLevel,
                                               int bThisLevelOnly, int bOwnDS);

#define DIV_ROUND_UP(a, b) ( ((a) % (b)) == 0 ? ((a) / (b)) : (((a) / (b)) + 1) )

// Number of data samples that will be used to compute approximate statistics
// (minimum value, maximum value, etc.)
#define GDALSTAT_APPROX_NUMSAMPLES 2500

CPL_C_START
/* Caution: for technical reason this declaration is duplicated in gdal_crs.c */
/* so any signature change should be reflected there too */
void GDALSerializeGCPListToXML( CPLXMLNode* psParentNode,
                                GDAL_GCP* pasGCPList,
                                int nGCPCount,
                                const char* pszGCPProjection );
void GDALDeserializeGCPListFromXML( CPLXMLNode* psGCPList,
                                    GDAL_GCP** ppasGCPList,
                                    int* pnGCPCount,
                                    char** ppszGCPProjection );
CPL_C_END

void GDALSerializeOpenOptionsToXML( CPLXMLNode* psParentNode, char** papszOpenOptions);
char** GDALDeserializeOpenOptionsFromXML( CPLXMLNode* psParentNode );

int GDALCanFileAcceptSidecarFile(const char* pszFilename);

#endif /* ndef GDAL_PRIV_H_INCLUDED */
