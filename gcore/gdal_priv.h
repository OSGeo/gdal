/******************************************************************************
 * $Id$
 *
 * Name:     gdal_priv.h
 * Project:  GDAL Core
 * Purpose:  GDAL Core C++/Private declarations. 
 * Author:   Frank Warmerdam, warmerda@home.com
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
#include "cpl_vsi.h"
#include "cpl_conv.h"

/* ******************************************************************** */
/*                           GDALMajorObject                            */
/*                                                                      */
/*      Base class providing metadata, description and other            */
/*      services shared by major objects.                               */
/* ******************************************************************** */

class CPL_DLL GDALMajorObject
{
  public:
    const char *	GetDescription( void * );
    void		SetDescription( const char * );
};

/* ******************************************************************** */
/*                             GDALProjDef                              */
/* ******************************************************************** */

//! Encapsulates a projections definition.

class CPL_DLL GDALProjDef
{
    void	*psPJ;

    char	*pszProjection;

  public:
    		GDALProjDef( const char * = NULL );
                ~GDALProjDef();

    CPLErr	ToLongLat( double * padfX, double * padfY );
    CPLErr	FromLongLat( double * padfX, double * padfY );

    const char  *GetProjectionString( void ) { return pszProjection; }
    CPLErr	SetProjectionString( const char * );
};


/* ******************************************************************** */
/*                             GDALDataset                              */
/* ******************************************************************** */

/**
 * A dataset encapsulating one or more raster bands.
 *
 * Use GDALOpen() to create a GDALDataset for a named file.
 */

class CPL_DLL GDALDataset : public GDALMajorObject
{
    friend GDALDatasetH GDALOpen( const char *, GDALAccess);
    
  protected:
    GDALDriver	*poDriver;
    GDALAccess	eAccess;
    
    // Stored raster information.
    int		nRasterXSize;
    int		nRasterYSize;
    int		nBands;
    GDALRasterBand **papoBands;

    int         nRefCount;

    		GDALDataset(void);
    void        RasterInitialize( int, int );
    void        SetBand( int, GDALRasterBand * );

  public:
    virtual	~GDALDataset();

    int		GetRasterXSize( void );
    int		GetRasterYSize( void );
    int		GetRasterCount( void );
    GDALRasterBand *GetRasterBand( int );

    virtual void FlushCache(void);

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual void *GetInternalHandle( const char * );
    virtual GDALDriver *GetDriver(void);
 
    int           Reference();
    int           Dereference();
};

/* ******************************************************************** */
/*                           GDALRasterBlock                            */
/* ******************************************************************** */

/*! A cached raster block ... to be documented later. */

class CPL_DLL GDALRasterBlock
{
    GDALDataType	eType;
    
    int			nAge;
    int			bDirty;

    int			nXSize;
    int			nYSize;
    
    void		*pData;

  public:
		GDALRasterBlock( int, int, GDALDataType, void * );
    virtual	~GDALRasterBlock();

    CPLErr	Internalize( void );	/* make copy of data */
    void	Touch( void );		/* update age */
    void	MarkDirty( void );      /* data has been modified since read */
    void	MarkClean( void );     

    GDALDataType GetDataType() { return eType; }
    int		GetXSize() { return nXSize; }
    int		GetYSize() { return nYSize; }
    int		GetAge() { return nAge; }
    int		GetDirty() { return bDirty; }

    void	*GetDataRef( void ) { return pData; }
};


/* ******************************************************************** */
/*                             GDALColorTable                           */
/* ******************************************************************** */

class CPL_DLL GDALColorTable
{
    GDALPaletteInterp eInterp;

    int		nEntryCount;
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
    GDALDataset	*poDS;
    int		nBand; /* 1 based */

    int	        nRasterXSize;
    int         nRasterYSize;
    
    GDALDataType eDataType;
    GDALAccess	eAccess;

    /* stuff related to blocking, and raster cache */
    int		nBlockXSize;
    int		nBlockYSize;
    int		nBlocksPerRow;
    int		nBlocksPerColumn;

    int		nLoadedBlocks;
    int		nMaxLoadableBlocks;
    
    GDALRasterBlock **papoBlocks;

    friend class GDALDataset;

  protected:
    virtual CPLErr IReadBlock( int, int, void * ) = 0;
    virtual CPLErr IWriteBlock( int, int, void * );
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );
    CPLErr         OverviewRasterIO( GDALRWFlag, int, int, int, int,
                                     void *, int, int, GDALDataType,
                                     int, int );

    CPLErr	   FlushBlock( int = -1, int = -1 );
    CPLErr	   AdoptBlock( int, int, GDALRasterBlock * );
    void           InitBlockInfo();

  public:
                GDALRasterBand();
                
    virtual	~GDALRasterBand();

    int		GetXSize();
    int		GetYSize();

    GDALDataType GetRasterDataType( void );
    void	GetBlockSize( int *, int * );
    GDALAccess	GetAccess();
    
    CPLErr 	RasterIO( GDALRWFlag, int, int, int, int,
                          void *, int, int, GDALDataType,
                          int, int );
    CPLErr 	ReadBlock( int, int, void * );

    CPLErr 	WriteBlock( int, int, void * );

    GDALRasterBlock *GetBlockRef( int, int );
    CPLErr	FlushCache();

    // New OpengIS CV_SampleDimension stuff.

    virtual const char  *GetDescription();
    virtual char **GetCategoryNames();
    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum(int *pbSuccess = NULL );
    virtual double GetOffset( int *pbSuccess = NULL );
    virtual double GetScale( int *pbSuccess = NULL );
    virtual const char *GetUnitType();
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();

    virtual int HasArbitraryOverviews();
    virtual int GetOverviewCount();
    virtual GDALRasterBand *GetOverview(int);
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
    
    char	*pszFilename;

    GDALAccess	eAccess;

    GBool	bStatOK;
    VSIStatBuf	sStat;
    
    FILE	*fp;

    int		nHeaderBytes;
    GByte	*pabyHeader;

};

/* ******************************************************************** */
/*                              GDALDriver                              */
/* ******************************************************************** */

/**
 * An instance of this class is created for each supported format, and
 * manages information about the format.
 * 
 * This roughly corresponds to a file format, though some          
 * drivers may be gateways to many formats through a secondary     
 * multi-library.                                                  
 */

class CPL_DLL GDALDriver
{
  public:
    			GDALDriver();
                        ~GDALDriver();

    /** Short (symbolic) format name. */
    char		*pszShortName;

    /** Long format name */
    char		*pszLongName;

    /** Help mechanism yet to be defined. */
    char		*pszHelpTopic;
    
    GDALDataset 	*(*pfnOpen)( GDALOpenInfo * );

    GDALDataset		*(*pfnCreate)( const char * pszName,
                                       int nXSize, int nYSize, int nBands,
                                       GDALDataType eType,
                                       char ** papszOptions );

    GDALDataset		*Create( const char * pszName,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszOptions );

    CPLErr		(*pfnDelete)( const char * pszName );

    CPLErr		Delete( const char * pszName );

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

class CPL_DLL GDALDriverManager
{
    int		nDrivers;
    GDALDriver	**papoDrivers;

 public:
    		GDALDriverManager();
                ~GDALDriverManager();
                
    int		GetDriverCount( void );
    GDALDriver  *GetDriver( int );
    GDALDriver  *GetDriverByName( const char * );

    int		RegisterDriver( GDALDriver * );
    void	MoveDriver( GDALDriver *, int );
    void	DeregisterDriver( GDALDriver * );
};

CPL_C_START
GDALDriverManager * GetGDALDriverManager( void );
CPL_C_END

#endif /* ndef GDAL_PRIV_H_INCLUDED */
