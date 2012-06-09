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
#include <vector>

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
    
  public:
                        GDALMajorObject();
    virtual            ~GDALMajorObject();

    int                 GetMOFlags();
    void                SetMOFlags(int nFlags);
                        
    virtual const char *GetDescription() const;
    virtual void        SetDescription( const char * );

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

    void       Initialize( GDALDataset *poDS, const char *pszName = NULL, 
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
  public:
                GDALOpenInfo( const char * pszFile, GDALAccess eAccessIn,
                              char **papszSiblingFiles = NULL );
                ~GDALOpenInfo( void );

    char        *pszFilename;
    char        **papszSiblingFiles;

    GDALAccess  eAccess;

    int         bStatOK;
    int         bIsDirectory;

    FILE        *fp;

    int         nHeaderBytes;
    GByte       *pabyHeader;

};

/* ******************************************************************** */
/*                             GDALDataset                              */
/* ******************************************************************** */

/* Internal method for now. Might be subject to later revisions */
GDALDatasetH GDALOpenInternal( const char * pszFilename, GDALAccess eAccess,
                               const char* const * papszAllowedDrivers);
GDALDatasetH GDALOpenInternal( GDALOpenInfo& oOpenInfo,
                               const char* const * papszAllowedDrivers);

//! A set of associated raster bands, usually from one file.

class CPL_DLL GDALDataset : public GDALMajorObject
{
    friend GDALDatasetH CPL_STDCALL GDALOpen( const char *, GDALAccess);
    friend GDALDatasetH CPL_STDCALL GDALOpenShared( const char *, GDALAccess);

    /* Internal method for now. Might be subject to later revisions */
    friend GDALDatasetH GDALOpenInternal( const char *, GDALAccess, const char* const * papszAllowedDrivers);
    friend GDALDatasetH GDALOpenInternal( GDALOpenInfo& oOpenInfo,
                                          const char* const * papszAllowedDrivers);

    friend class GDALDriver;
    friend class GDALDefaultOverviews;
    friend class GDALProxyDataset;
    friend class GDALDriverManager;

  protected:
    GDALDriver  *poDriver;
    GDALAccess  eAccess;
    
    // Stored raster information.
    int         nRasterXSize;
    int         nRasterYSize;
    int         nBands;
    GDALRasterBand **papoBands;

    int         bForceCachedIO;

    int         nRefCount;
    int         bShared;

                GDALDataset(void);
    void        RasterInitialize( int, int );
    void        SetBand( int, GDALRasterBand * );

    GDALDefaultOverviews oOvManager;
    
    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );
    
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

    CPLErr BlockBasedRasterIO( GDALRWFlag, int, int, int, int,
                               void *, int, int, GDALDataType,
                               int, int *, int, int, int );
    void   BlockBasedFlushCache();

    virtual int         CloseDependentDatasets();

    friend class GDALRasterBand;
    
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

    virtual CPLErr          CreateMaskBand( int nFlags );

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
                          int, int *, int, int, int );

    int           Reference();
    int           Dereference();
    GDALAccess    GetAccess() { return eAccess; }

    int           GetShared();
    void          MarkAsShared();

    static GDALDataset **GetOpenDatasets( int *pnDatasetCount );

    CPLErr BuildOverviews( const char *, int, int *,
                           int, int *, GDALProgressFunc, void * );

    void ReportError(CPLErr eErrClass, int err_no, const char *fmt, ...)  CPL_PRINT_FUNC_FORMAT (4, 5);
};

/* ******************************************************************** */
/*                           GDALRasterBlock                            */
/* ******************************************************************** */

//! A single raster block in the block cache.

class CPL_DLL GDALRasterBlock
{
    GDALDataType        eType;
    
    int                 bDirty;
    int                 nLockCount;

    int                 nXOff;
    int                 nYOff;
       
    int                 nXSize;
    int                 nYSize;
    
    void                *pData;

    GDALRasterBand      *poBand;
    
    GDALRasterBlock     *poNext;
    GDALRasterBlock     *poPrevious;

  public:
                GDALRasterBlock( GDALRasterBand *, int, int );
    virtual     ~GDALRasterBlock();

    CPLErr      Internalize( void );
    void        Touch( void );      
    void        MarkDirty( void );  
    void        MarkClean( void );
    void        AddLock( void ) { nLockCount++; }
    void        DropLock( void ) { nLockCount--; }
    void        Detach();

    CPLErr      Write();

    GDALDataType GetDataType() { return eType; }
    int         GetXOff() { return nXOff; }
    int         GetYOff() { return nYOff; }
    int         GetXSize() { return nXSize; }
    int         GetYSize() { return nYSize; }
    int         GetDirty() { return bDirty; }
    int         GetLockCount() { return nLockCount; }

    void        *GetDataRef( void ) { return pData; }

    /// @brief Accessor to source GDALRasterBand object.
    /// @return source raster band of the raster block.
    GDALRasterBand *GetBand() { return poBand; }

    static int  FlushCacheBlock();
    static void Verify();

    static int  SafeLockBlock( GDALRasterBlock ** );
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

    GDALPaletteInterp GetPaletteInterpretation() const;

    int           GetColorEntryCount() const;
    const GDALColorEntry *GetColorEntry( int ) const;
    int           GetColorEntryAsRGB( int, GDALColorEntry * ) const;
    void          SetColorEntry( int, const GDALColorEntry * );
    int           CreateColorRamp( int, const GDALColorEntry * ,
                                   int, const GDALColorEntry * );
};

/* ******************************************************************** */
/*                            GDALRasterBand                            */
/* ******************************************************************** */

//! A single raster band (or channel).

class CPL_DLL GDALRasterBand : public GDALMajorObject
{
  private:
    CPLErr eFlushBlockErr;

    void           SetFlushBlockErr( CPLErr eErr );

    friend class GDALRasterBlock;

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

    int         bSubBlockingActive;
    int         nSubBlocksPerRow;
    int         nSubBlocksPerColumn;
    GDALRasterBlock **papoBlocks;

    int         nBlockReads;
    int         bForceCachedIO;

    GDALRasterBand *poMask;
    bool        bOwnMask;
    int         nMaskFlags;

    friend class GDALDataset;
    friend class GDALProxyRasterBand;

  protected:
    virtual CPLErr IReadBlock( int, int, void * ) = 0;
    virtual CPLErr IWriteBlock( int, int, void * );
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );
    CPLErr         OverviewRasterIO( GDALRWFlag, int, int, int, int,
                                     void *, int, int, GDALDataType,
                                     int, int );

    int            InitBlockInfo();

    CPLErr         AdoptBlock( int, int, GDALRasterBlock * );
    GDALRasterBlock *TryGetLockedBlockRef( int nXBlockOff, int nYBlockYOff );

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
                          int, int );
    CPLErr      ReadBlock( int, int, void * );

    CPLErr      WriteBlock( int, int, void * );

    GDALRasterBlock *GetLockedBlockRef( int nXBlockOff, int nYBlockOff, 
                                        int bJustInitialize = FALSE );
    CPLErr      FlushBlock( int = -1, int = -1, int bWriteDirtyBlock = TRUE );

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
    virtual GDALRasterBand *GetRasterSampleOverview( int );
    virtual CPLErr BuildOverviews( const char *, int, int *,
                                   GDALProgressFunc, void * );

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               GDALDataType eDT, char **papszOptions );

    virtual CPLErr  GetHistogram( double dfMin, double dfMax,
                          int nBuckets, int * panHistogram,
                          int bIncludeOutOfRange, int bApproxOK,
                          GDALProgressFunc, void *pProgressData );

    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets, int ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData);
    virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, int *panHistogram );

    virtual const GDALRasterAttributeTable *GetDefaultRAT();
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * );

    virtual GDALRasterBand *GetMaskBand();
    virtual int             GetMaskFlags();
    virtual CPLErr          CreateMaskBand( int nFlags );

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
                              int, int );

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

    int                 (*pfnIdentify)( GDALOpenInfo * );

    CPLErr              (*pfnRename)( const char * pszNewName,
                                      const char * pszOldName );
    CPLErr              (*pfnCopyFiles)( const char * pszNewName,
                                         const char * pszOldName );

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

    char        *pszHome;
    
 public:
                GDALDriverManager();
                ~GDALDriverManager();
                
    int         GetDriverCount( void );
    GDALDriver  *GetDriver( int );
    GDALDriver  *GetDriverByName( const char * );

    int         RegisterDriver( GDALDriver * );
    void        MoveDriver( GDALDriver *, int );
    void        DeregisterDriver( GDALDriver * );

    void        AutoLoadDrivers();
    void        AutoSkipDrivers();

    const char *GetHome();
    void        SetHome( const char * );
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
                                         int nBufXSize, int nBufYSize);

int CPL_DLL GDALOvLevelAdjust( int nOvLevel, int nXSize );

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


// Test if 2 floating point values match. Usefull when comparing values
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

CPL_C_END

CPLString GDALFindAssociatedFile( const char *pszBasename, const char *pszExt,
                                  char **papszSiblingFiles, int nFlags );

#include "cpl_vsi.h"
CPLErr EXIFExtractMetadata(char**& papszMetadata,
                           VSILFILE *fp, int nOffset,
                           int bSwabflag, int nTIFFHEADER,
                           int& nExifOffset, int& nInterOffset, int& nGPSOffset);

#define DIV_ROUND_UP(a, b) ( ((a) % (b)) == 0 ? ((a) / (b)) : (((a) / (b)) + 1) )

#endif /* ndef GDAL_PRIV_H_INCLUDED */
