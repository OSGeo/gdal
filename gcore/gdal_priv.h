/******************************************************************************
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
 * gdal_priv.h
 *
 * This is the primary private include file used within the GDAL library.
 * Note this is a C++ include file, and can't be used by C code.
 * 
 * $Log$
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
class GDALGeoref;
class GDALDriver;

/* -------------------------------------------------------------------- */
/*      Pull in the public declarations.  This gets the C apis, and     */
/*      also various constants.  However, we will still get to          */
/*      provide the real class definitions for the GDAL classes.        */
/* -------------------------------------------------------------------- */

#include "gdal.h"
#include "cpl_vsi.h"

/************************************************************************/
/*                           GDALMajorObject                            */
/*                                                                      */
/*      Base class providing metadata, description and other            */
/*      services shared by major objects.                               */
/************************************************************************/

class CPL_DLL GDALMajorObject
{
  public:
    const char *	GetDescription( void * );
    void		SetDescription( const char * );
};

/************************************************************************/
/*                             GDALDataset                              */
/*                                                                      */
/*      Normally this is one file.                                      */
/************************************************************************/

class CPL_DLL GDALDataset : public GDALMajorObject
{
  protected:
    GDALDriver	*poDriver;
    GDALAccess	eAccess;
    
    // Stored raster information.
    int		nRasterXSize;
    int		nRasterYSize;
    int		nBands;
    GDALRasterBand **papoBands;

    		GDALDataset(void);
    void        RasterInitialize( int, int );
    void        SetBand( int, GDALRasterBand * );
    
  public:
    virtual	~GDALDataset();
    
    int		GetRasterXSize( void );
    int		GetRasterYSize( void );
    int		GetRasterCount( void );
    GDALGeoref  *GetRasterGeoref( void );
    GDALRasterBand *GetRasterBand( int );
};

/************************************************************************/
/*                            GDALRasterBand                            */
/*                                                                      */
/*      one band, or channel in a dataset.                              */
/************************************************************************/

class CPL_DLL GDALRasterBand : public GDALMajorObject
{
  protected:
    GDALDataset	*poDS;
    int		nBand; /* 1 based */
    
    GDALDataType eDataType;
    int		nBlockXSize;
    int		nBlockYSize;
    GDALAccess	eAccess;

    friend class GDALDataset;
    
  public:
                GDALRasterBand();
                
    virtual	~GDALRasterBand();
    
    GDALDataType GetRasterDataType( void );
    void	GetBlockSize( int *, int * );
    GDALAccess	GetAccess();
    
    virtual CPLErr RasterIO( GDALRWFlag, int, int, int, int,
                             void *, int, int, GDALDataType,
                             int, int );
    virtual CPLErr ReadBlock( int, int, void * ) = 0;
    virtual CPLErr WriteBlock( int, int, void * );
};

/************************************************************************/
/*                             GDALOpenInfo                             */
/*                                                                      */
/*      Structure of data about dataset for open functions.             */
/************************************************************************/

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

/************************************************************************/
/*                              GDALDriver                              */
/*                                                                      */
/*      This roughly corresponds to a file format, though some          */
/*      drivers may be gateways to many formats through a secondary     */
/*      multi-library.                                                  */
/************************************************************************/

class CPL_DLL GDALDriver
{
  public:
    			GDALDriver();
                        ~GDALDriver();
                        
    char		*pszShortName;
    char		*pszLongName;
    char		*pszHelpTopic;
    
    GDALDataset 	*(*pfnOpen)( GDALOpenInfo * );
    // eventually a create function.
};

/************************************************************************/
/*                          GDALDriverManager                           */
/************************************************************************/

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
