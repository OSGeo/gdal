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
 ****************************************************************************/

#ifndef MEMDATASET_H_INCLUDED
#define MEMDATASET_H_INCLUDED

#include "gdal_pam.h"
#include "gdal_priv.h"

CPL_C_START
void	GDALRegister_MEM(void);
/* Caution: if changing this prototype, also change in swig/include/gdal_python.i
   where it is redefined */
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

    int          nGCPCount;
    GDAL_GCP    *pasGCPs;
    CPLString    osGCPProjection;

  public:
                 MEMDataset();
    virtual      ~MEMDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );

    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual void *GetInternalHandle( const char * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection );

    virtual CPLErr        AddBand( GDALDataType eType, 
                                   char **papszOptions=NULL );
    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpaceBuf,
                               GSpacing nLineSpaceBuf,
                               GSpacing nBandSpaceBuf,
                               GDALRasterIOExtraArg* psExtraArg);
    
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
    friend      class MEMDataset;

    GByte      *pabyData;
    GSpacing    nPixelOffset;
    GSpacing    nLineOffset;
    int         bOwnData;

    int         bNoDataSet;
    double      dfNoData;

    GDALColorTable *poColorTable;
    GDALColorInterp eColorInterp;

    char           *pszUnitType;
    char           **papszCategoryNames;
    
    double         dfOffset;
    double         dfScale;

    CPLXMLNode    *psSavedHistograms;
  public:

                   MEMRasterBand( GDALDataset *poDS, int nBand,
                                  GByte *pabyData, GDALDataType eType,
                                  GSpacing nPixelOffset, GSpacing nLineOffset,
                                  int bAssumeOwnership,  const char * pszPixelType = NULL);
    virtual        ~MEMRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpaceBuf,
                                  GSpacing nLineSpaceBuf,
                                  GDALRasterIOExtraArg* psExtraArg );
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

    virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig *panHistogram );
    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets, GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData);

    // allow access to MEM driver's private internal memory buffer
    GByte *GetData(void) const {return(pabyData);}
};

#endif /* ndef MEMDATASET_H_INCLUDED */

