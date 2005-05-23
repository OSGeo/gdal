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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.58  2005/05/23 06:44:48  fwarmerdam
 * blockrefs now fetched locked
 *
 * Revision 1.57  2005/05/13 18:19:15  fwarmerdam
 * Added SetDefaultHistogram
 *
 * Revision 1.56  2005/05/10 15:30:28  fwarmerdam
 * export some overview functions
 *
 * Revision 1.55  2005/05/10 04:49:24  fwarmerdam
 * added getdefaulthistogram and GDALOvLevelAdjust
 *
 * Revision 1.54  2005/04/27 16:29:33  fwarmerdam
 * GetHistogram() made virtual.  Added methods for getting and setting
 * statistics.  Added SetUnitType method.
 *
 * Revision 1.53  2005/04/07 17:31:00  fwarmerdam
 * added some brief descriptions of classes
 *
 * Revision 1.52  2005/04/04 15:24:48  fwarmerdam
 * Most C entry points now CPL_STDCALL
 *
 * Revision 1.51  2005/02/17 22:16:12  fwarmerdam
 * changed to use two level block cache
 *
 * Revision 1.50  2005/01/15 16:09:37  fwarmerdam
 * added SetOffset, SetScale methods
 *
 * Revision 1.49  2005/01/04 21:14:01  fwarmerdam
 * added GDAL_FORCE_CACHING config variable
 *
 * Revision 1.48  2004/12/02 20:32:03  fwarmerdam
 * added AdviseRead methods
 *
 * Revision 1.47  2004/04/06 19:22:25  dron
 * Removed GDALRasterBlock::IsCached(), added GDALRasterBand::IsBlockCached().
 *
 * Revision 1.46  2003/07/27 11:01:01  dron
 * GDALRasterBlock::IsCached() method added.
 *
 * Revision 1.45  2003/05/08 14:38:40  warmerda
 * added BlockBasedFlushCache
 *
 * Revision 1.44  2003/05/06 05:20:38  sperkins
 * cleaned up comments
 *
 * Revision 1.43  2003/05/06 05:13:36  sperkins
 * added Fill() and GDALFillRaster()
 *
 * Revision 1.42  2003/04/25 19:47:57  warmerda
 * added RasterIO on GDALDataset, added lock count on blocks
 *
 * Revision 1.41  2003/03/20 22:10:53  warmerda
 * added support for reporting cache thrashing
 *
 * Revision 1.40  2003/03/18 06:00:57  warmerda
 * made GDALRasterBand::FlushCache() virtual
 *
 * Revision 1.39  2003/01/28 16:07:30  warmerda
 * improved documentation
 *
 * Revision 1.38  2002/10/21 18:05:42  warmerda
 * added AutoSkipDrivers() method on driver manager
 *
 * Revision 1.37  2002/09/11 14:17:52  warmerda
 * make SetDescription() and GetDescription() virtual
 *
 * Revision 1.36  2002/09/04 06:52:05  warmerda
 * added unload driver support to GDALDriver
 *
 * Revision 1.35  2002/07/09 20:33:12  warmerda
 * expand tabs
 *
 * Revision 1.34  2002/06/19 18:19:59  warmerda
 * GDALOpenInfo: added bIsDirecory flag, removed VSIStatBuf
 *
 * Revision 1.33  2002/06/12 21:13:27  warmerda
 * use metadata based driver info
 *
 * Revision 1.32  2002/05/29 15:58:26  warmerda
 * removed GetDescription(), added SetColorInterpretation()
 *
 * Revision 1.31  2002/05/28 18:56:22  warmerda
 * added shared dataset concept
 *
 * Revision 1.30  2002/03/01 14:29:09  warmerda
 * added GetBand() method on GDALRasterBand
 *
 * Revision 1.29  2001/12/15 15:42:27  warmerda
 * *** empty log message ***
 *
 * Revision 1.28  2001/11/18 00:52:15  warmerda
 * removed GDALProjDef
 *
 * Revision 1.27  2001/11/16 21:36:01  warmerda
 * added the AddBand() method on GDALDataset
 *
 * Revision 1.26  2001/10/17 21:47:02  warmerda
 * added SetGCPs() on GDALDataset
 *
 * Revision 1.25  2001/10/02 13:15:16  warmerda
 * added CPL_DLL for GDALDefaultOverviews
 *
 * Revision 1.24  2001/06/20 16:08:54  warmerda
 * GDALDefaultOverviews now remembers ovr filename, and allows explicit setting
 *
 * Revision 1.23  2001/02/06 16:30:21  warmerda
 * Added gdal_frmts.h
 *
 * Revision 1.22  2000/10/06 15:26:08  warmerda
 * added SetNoDataValue, SetCategoryNames
 *
 * Revision 1.21  2000/07/20 13:38:26  warmerda
 * make GetGDALDriverManager public with CPL_DLL
 *
 * Revision 1.20  2000/04/30 23:22:16  warmerda
 * added CreateCopy support
 *
 * Revision 1.19  2000/04/21 21:55:01  warmerda
 * majorobject updates, and overview building
 *
 * Revision 1.18  2000/04/04 23:44:29  warmerda
 * added AutoLoadDrivers() to GDALDriverManager
 *
 * Revision 1.17  2000/03/31 13:41:24  warmerda
 * added gcps
 *
 * Revision 1.16  2000/03/24 00:09:05  warmerda
 * rewrote cache management
 *
 * Revision 1.15  2000/03/09 23:22:03  warmerda
 * added GetHistogram
 *
 * Revision 1.14  2000/03/06 02:20:35  warmerda
 * added colortables, overviews, etc
 *
 * Revision 1.12  2000/01/31 15:00:25  warmerda
 * added some documentation
 *
 * Revision 1.11  2000/01/31 14:24:36  warmerda
 * implemented dataset delete
 *
 * Revision 1.10  1999/11/11 21:59:07  warmerda
 * added GetDriver() for datasets
 *
 * Revision 1.9  1999/10/21 13:23:45  warmerda
 * Added a bit of driver related documentation.
 *
 * Revision 1.8  1999/10/21 12:04:11  warmerda
 * Reorganized header.
 *
 * Revision 1.7  1999/10/01 14:44:02  warmerda
 * added documentation
 *
 * Revision 1.6  1999/04/21 04:16:25  warmerda
 * experimental docs
 *
 * Revision 1.5  1999/01/11 15:36:18  warmerda
 * Added projections support, and a few other things.
 *
 * Revision 1.4  1998/12/31 18:54:25  warmerda
 * Implement initial GDALRasterBlock support, and block cache
 *
 * Revision 1.3  1998/12/06 22:17:09  warmerda
 * Fill out rasterio support.
 *
 * Revision 1.2  1998/12/03 18:34:06  warmerda
 * Update to use CPL
 *
 * Revision 1.1  1998/10/18 06:15:11  warmerda
 * Initial implementation.
 *
 */

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

/* -------------------------------------------------------------------- */
/*      Pull in the public declarations.  This gets the C apis, and     */
/*      also various constants.  However, we will still get to          */
/*      provide the real class definitions for the GDAL classes.        */
/* -------------------------------------------------------------------- */

#include "gdal.h"
#include "gdal_frmts.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"

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
    char             *pszDescription;
    char            **papszMetadata;
    
  public:
                        GDALMajorObject();
    virtual            ~GDALMajorObject();
                        
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
    GDALDataset *poDS;
    GDALDataset *poODS;
    
    char        *pszOvrFilename;
    
  public:
               GDALDefaultOverviews();
               ~GDALDefaultOverviews();

    void       Initialize( GDALDataset *poDS, const char *pszName = NULL, 
                           int bNameIsOVR = FALSE );
    int        IsInitialized() { return poDS != NULL; }

    int        GetOverviewCount(int);
    GDALRasterBand *GetOverview(int,int);

    CPLErr     BuildOverviews( const char * pszBasename,
                               const char * pszResampling, 
                               int nOverviews, int * panOverviewList,
                               int nBands, int * panBandList,
                               GDALProgressFunc pfnProgress,
                               void *pProgressData );
};

/* ******************************************************************** */
/*                             GDALDataset                              */
/* ******************************************************************** */

//! A set of associated raster bands, usually from one file.

class CPL_DLL GDALDataset : public GDALMajorObject
{
    friend GDALDatasetH CPL_STDCALL GDALOpen( const char *, GDALAccess);
    friend GDALDatasetH CPL_STDCALL GDALOpenShared( const char *, GDALAccess);
    friend class GDALDriver;

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
};

/* ******************************************************************** */
/*                           GDALRasterBlock                            */
/* ******************************************************************** */

/*! A cached raster block ... to be documented later. */

class CPL_DLL GDALRasterBlock
{
    GDALDataType        eType;
    
    int                 nAge;
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

    CPLErr      Internalize( void );    /* make copy of data */
    void        Touch( void );          /* update age */
    void        MarkDirty( void );      /* data has been modified since read */
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
    int         GetAge() { return nAge; }
    int         GetDirty() { return bDirty; }
    int         GetLockCount() { return nLockCount; }

    void        *GetDataRef( void ) { return pData; }

    GDALRasterBand *GetBand() { return poBand; }

    static int  FlushCacheBlock();
    static void Verify();

    static int  SafeLockBlock( GDALRasterBlock ** );
};

/* ******************************************************************** */
/*                             GDALColorTable                           */
/* ******************************************************************** */

class CPL_DLL GDALColorTable
{
    GDALPaletteInterp eInterp;

    int         nEntryCount;
    GDALColorEntry *paoEntries;

public:
                GDALColorTable( GDALPaletteInterp = GPI_RGB );
                ~GDALColorTable();

    GDALColorTable *Clone() const;

    GDALPaletteInterp GetPaletteInterpretation() const;

    int           GetColorEntryCount() const;
    const GDALColorEntry *GetColorEntry( int ) const;
    int           GetColorEntryAsRGB( int, GDALColorEntry * ) const;
    void          SetColorEntry( int, const GDALColorEntry * );
};

/* ******************************************************************** */
/*                            GDALRasterBand                            */
/* ******************************************************************** */

//! A single raster band (or channel).

class CPL_DLL GDALRasterBand : public GDALMajorObject
{
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

    friend class GDALDataset;
    friend class GDALRasterBlock;

  protected:
    virtual CPLErr IReadBlock( int, int, void * ) = 0;
    virtual CPLErr IWriteBlock( int, int, void * );
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );
    CPLErr         OverviewRasterIO( GDALRWFlag, int, int, int, int,
                                     void *, int, int, GDALDataType,
                                     int, int );

    void           InitBlockInfo();

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
    CPLErr      FlushBlock( int = -1, int = -1 );

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
    virtual CPLErr SetStatistics( double dfMin, double dfMax, 
                                  double dfMean, double dfStdDev );

    virtual int HasArbitraryOverviews();
    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);
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
};

/* ******************************************************************** */
/*                             GDALOpenInfo                             */
/*                                                                      */
/*      Structure of data about dataset for open functions.             */
/* ******************************************************************** */

class CPL_DLL GDALOpenInfo
{
  public:

                GDALOpenInfo( const char * pszFile, GDALAccess eAccessIn );
                ~GDALOpenInfo( void );
    
    char        *pszFilename;

    GDALAccess  eAccess;

    int         bStatOK;
    int         bIsDirectory;

    FILE        *fp;

    int         nHeaderBytes;
    GByte       *pabyHeader;

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
                                 GDALDataType eType, char ** papszOptions );

    CPLErr              Delete( const char * pszName );

    GDALDataset         *CreateCopy( const char *, GDALDataset *, 
                                     int, char **,
                                     GDALProgressFunc pfnProgress, 
                                     void * pProgressData );
    
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

/* ==================================================================== */
/*      An assortment of overview related stuff.                        */
/* ==================================================================== */

CPL_C_START

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
                           

CPLErr CPL_DLL 
GDALRegenerateOverviews( GDALRasterBand *, int, GDALRasterBand **,
                         const char *, GDALProgressFunc, void * );

int CPL_DLL GDALOvLevelAdjust( int nOvLevel, int nXSize );

CPL_C_END

#endif /* ndef GDAL_PRIV_H_INCLUDED */
