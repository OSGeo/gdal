/******************************************************************************
 * $Id$
 *
 * Project:  Raw Translator
 * Purpose:  Implementation of RawDataset class.  Intented to be subclassed
 *           by other raw formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.20  2006/06/08 14:35:15  fwarmerdam
 * IRasterio now only protected so it can be overridden
 *
 * Revision 1.19  2006/06/08 10:33:11  dron
 * Make RawRasterBand::IRasterIO() protected method.
 *
 * Revision 1.18  2006/04/10 16:28:33  fwarmerdam
 * Fixed contact info.
 *
 * Revision 1.17  2005/05/05 13:55:42  fwarmerdam
 * PAM Enable
 *
 * Revision 1.16  2004/11/02 20:21:38  fwarmerdam
 * added support for category names
 *
 * Revision 1.15  2004/07/31 04:53:21  warmerda
 * added various query methods
 *
 * Revision 1.14  2004/06/02 20:57:55  warmerda
 * centralize initialization
 *
 * Revision 1.13  2004/05/28 18:16:22  warmerda
 * Added support for hold colortable and interp on RawRasterBand
 *
 * Revision 1.12  2003/07/27 11:04:40  dron
 * Added RawRasterBand::IsLineLoaded() method.
 *
 * Revision 1.11  2003/05/02 16:00:17  dron
 * Implemented RawRasterBand::IRasterIO() method. Introduced `dirty' flag.
 *
 * Revision 1.10  2003/03/18 06:00:26  warmerda
 * Added FlushCache() method on rawrasterband.
 *
 * Revision 1.9  2002/11/23 18:54:47  warmerda
 * added setnodatavalue
 *
 * Revision 1.8  2002/03/21 16:22:03  warmerda
 * fixed friend declarations
 *
 * Revision 1.7  2001/12/12 18:15:46  warmerda
 * preliminary update for large raw file support
 *
 * Revision 1.6  2001/03/23 03:25:32  warmerda
 * Added nodata support
 *
 * Revision 1.5  2000/08/16 15:51:17  warmerda
 * allow floating (datasetless) raw bands
 *
 * Revision 1.4  2000/07/20 13:38:56  warmerda
 * make classes public with CPL_DLL
 *
 * Revision 1.3  2000/03/31 13:36:41  warmerda
 * RawRasterBand no longer depends on RawDataset
 *
 * Revision 1.2  1999/08/13 02:36:57  warmerda
 * added write support
 *
 * Revision 1.1  1999/07/23 19:34:34  warmerda
 * New
 *
 */

#include "gdal_pam.h"

/************************************************************************/
/* ==================================================================== */
/*				RawDataset				*/
/* ==================================================================== */
/************************************************************************/

class RawRasterBand;

class CPL_DLL RawDataset : public GDALPamDataset
{
    friend class RawRasterBand;

  protected:
    virtual CPLErr      IRasterIO( GDALRWFlag, int, int, int, int,
                                   void *, int, int, GDALDataType,
                                   int, int *, int, int, int );
  public:
                 RawDataset();
                 ~RawDataset();

};

/************************************************************************/
/* ==================================================================== */
/*                            RawRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class CPL_DLL RawRasterBand : public GDALPamRasterBand
{
protected:
    friend class RawDataset;

    FILE	*fpRaw;
    int         bIsVSIL;

    vsi_l_offset nImgOffset;
    int		nPixelOffset;
    int		nLineOffset;
    int         nLineSize;
    int		bNativeOrder;

    int		bNoDataSet;
    double	dfNoDataValue;
    
    int		nLoadedScanline;
    void	*pLineBuffer;
    int         bDirty;

    GDALColorTable *poCT;
    GDALColorInterp eInterp;

    char           **papszCategoryNames;

    int         Seek( vsi_l_offset, int );
    size_t      Read( void *, size_t, size_t );
    size_t      Write( void *, size_t, size_t );

    CPLErr      AccessBlock( vsi_l_offset nBlockOff, int nBlockSize,
                             void * pData );
    int         IsLineLoaded( int nLineOff, int nLines );
    void        Initialize();

    virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int );

  public:

                 RawRasterBand( GDALDataset *poDS, int nBand, FILE * fpRaw, 
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int bIsVSIL = FALSE );

                 RawRasterBand( FILE * fpRaw, 
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int nXSize, int nYSize, int bIsVSIL = FALSE );

                 ~RawRasterBand();

    // should override RasterIO eventually.
    
    virtual CPLErr  IReadBlock( int, int, void * );
    virtual CPLErr  IWriteBlock( int, int, void * );

    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr SetColorTable( GDALColorTable * ); 
    virtual CPLErr SetColorInterpretation( GDALColorInterp );

    virtual CPLErr  SetNoDataValue( double );
    virtual double  GetNoDataValue( int *pbSuccess = NULL );

    virtual char **GetCategoryNames();
    virtual CPLErr SetCategoryNames( char ** );

    virtual CPLErr  FlushCache();

    CPLErr          AccessLine( int iLine );
    // this is deprecated.
    void	 StoreNoDataValue( double );

    // Query methods for internal data. 
    vsi_l_offset GetImgOffset() { return nImgOffset; }
    int          GetPixelOffset() { return nPixelOffset; }
    int          GetLineOffset() { return nLineOffset; }
    int          GetNativeOrder() { return bNativeOrder; }
    int          GetIsVSIL() { return bIsVSIL; }
    FILE        *GetFP() { return fpRaw; }
};

