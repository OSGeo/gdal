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

/* -------------------------------------------------------------------- */
/*      Pull in the public declarations.  This gets the C apis, and     */
/*      also various constants.  However, we will still get to          */
/*      provide the real class definitions for the GDAL classes.        */
/* -------------------------------------------------------------------- */

#include "gdal.h"
#include "gdal_vsi.h"

/************************************************************************/
/*                           GDALMajorObject                            */
/*                                                                      */
/*      Base class providing metadata, description and other            */
/*      services shared by major objects.                               */
/************************************************************************/

class GDAL_DLL GDALMajorObject
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

class GDAL_DLL GDALDataset : public GDALMajorObject
{
  protected:
    // Stored raster information.
    int		nRasterXSize;
    int		nRasterYSize;
    int		nBands;
    GDALRasterBand **papoBands;
    
  public:
    
    void	Close( void );

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

class GDAL_DLL GDALRasterBand : public GDALMajorObject
{
    GDADataType	nDataType;
    int		nBlockXSize;
    int		nBlockYSize;
    
  public:
    GDALDataType GetRasterDataType( void );
    void	GetBlockSize( int *, int * );
    GBSErr	RasterIO( GDALRWFlag, int, int, int, int,
                          void *, int, int, GDALDataType,
                          int, int );
};

/************************************************************************/
/*                              GDALDriver                              */
/*                                                                      */
/*      This roughly corresponds to a file format, though some          */
/*      drivers may be gateways to many formats through a secondary     */
/*      multi-library.                                                  */
/************************************************************************/

class GDAL_DLL GDALDriver
{
  public:
    char		*pszShortName;
    char		*pszFullName;
    
    GDALDataset 	*(*pfnOpen)( GDALOpenInfo * );
    GBSErr		(*pfnClose)( GDALDataSet * );

    GBSErr		(*pfnRasterIO)( GDALRasterBand *,
                                        GDALRWFlag, int, int, int, int,
                                        void *, int, int, GDALDataType,
                                        int, int );
    GDSErr		(*pfnReadBlock)( GDALRasterBand *, int, int,
                                         void * pData );
    GDSErr		(*pfnWriteBlock)( GDALRasterBand *, int, int,
                                          void * pData );
};




#endif /* ndef GDAL_PRIV_H_INCLUDED */
