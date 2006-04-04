/******************************************************************************
 * $Id$
 *
 * Project:  Memory Array Translator
 * Purpose:  Declaration of MEMDataset, and MEMRasterBand.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.13  2006/04/04 04:42:39  fwarmerdam
 * update contact info
 *
 * Revision 1.12  2005/05/19 20:39:54  dron
 * Derive the MEMRasterBand from GDALPamRasterBand.
 *
 * Revision 1.11  2005/04/19 19:56:11  fwarmerdam
 * Explicitly mark destructor as virtual.
 *
 * Revision 1.10  2004/04/15 18:54:10  warmerda
 * added UnitType, Offset, Scale and CategoryNames support
 *
 * Revision 1.9  2002/11/20 05:18:09  warmerda
 * added AddBand() implementation
 *
 * Revision 1.8  2002/06/10 21:31:57  warmerda
 * preserve projection and geotransform
 *
 * Revision 1.7  2002/05/29 16:01:54  warmerda
 * fixed SetColorInterpretation
 *
 * Revision 1.6  2002/04/12 17:37:31  warmerda
 * added colortable support
 *
 * Revision 1.5  2002/03/01 16:45:53  warmerda
 * added support for retaining nodata value
 *
 * Revision 1.4  2001/10/26 20:03:28  warmerda
 * added C entry point for creating MEMRasterBand
 *
 * Revision 1.3  2000/07/20 13:38:48  warmerda
 * make classes public with CPL_DLL
 *
 * Revision 1.2  2000/07/19 19:07:04  warmerda
 * break linkage between MEMDataset and MEMRasterBand
 *
 * Revision 1.1  2000/07/19 15:55:11  warmerda
 * New
 *
 */

#ifndef MEMDATASET_H_INCLUDED
#define MEMDATASET_H_INCLUDED

#include "gdal_pam.h"
#include "gdal_priv.h"

CPL_C_START
void	GDALRegister_MEM(void);
GDALRasterBandH CPL_DLL MEMCreateRasterBand( GDALDataset *, int, GByte *,
                                             GDALDataType, int, int, int );
CPL_C_END

/************************************************************************/
/*				MEMDataset				*/
/************************************************************************/

class MEMRasterBand;

class CPL_DLL MEMDataset : public GDALDataset
{
    int         bGeoTransformSet;
    double	adfGeoTransform[6];

    char        *pszProjection;

  public:
                 MEMDataset();
    virtual      ~MEMDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual CPLErr        AddBand( GDALDataType eType, 
                                   char **papszOptions=NULL );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            MEMRasterBand                             */
/************************************************************************/

class CPL_DLL MEMRasterBand : public GDALPamRasterBand
{
  protected:

    GByte      *pabyData;
    int         nPixelOffset;
    int         nLineOffset;
    int         bOwnData;

    int         bNoDataSet;
    double      dfNoData;

    GDALColorTable *poColorTable;
    GDALColorInterp eColorInterp;

    char           *pszUnitType;
    char           **papszCategoryNames;
    
    double         dfOffset;
    double         dfScale;

  public:

                   MEMRasterBand( GDALDataset *poDS, int nBand,
                                  GByte *pabyData, GDALDataType eType,
                                  int nPixelOffset, int nLineOffset,
                                  int bAssumeOwnership );
    virtual        ~MEMRasterBand();

    // should override RasterIO eventually.

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual CPLErr SetNoDataValue( double );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr SetColorTable( GDALColorTable * ); 

    virtual CPLErr SetColorInterpretation( GDALColorInterp );

    virtual const char *GetUnitType();
    CPLErr SetUnitType( const char * ); 

    virtual char **GetCategoryNames();
    virtual CPLErr SetCategoryNames( char ** );

    virtual double GetOffset( int *pbSuccess = NULL );
    CPLErr SetOffset( double );
    virtual double GetScale( int *pbSuccess = NULL );
    CPLErr SetScale( double );
};

#endif /* ndef MEMDATASET_H_INCLUDED */

