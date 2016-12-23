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
/* Caution: if changing this prototype, also change in swig/include/gdal_python.i
   where it is redefined */
GDALRasterBandH CPL_DLL MEMCreateRasterBand( GDALDataset *, int, GByte *,
                                             GDALDataType, int, int, int );
GDALRasterBandH CPL_DLL MEMCreateRasterBandEx( GDALDataset *, int, GByte *,
                                               GDALDataType, GSpacing, GSpacing,
                                               int );
CPL_C_END

/************************************************************************/
/*                            MEMDataset                                */
/************************************************************************/

class MEMRasterBand;

class CPL_DLL MEMDataset : public GDALDataset
{
    friend class MEMRasterBand;

    int         bGeoTransformSet;
    double      adfGeoTransform[6];

    char        *pszProjection;

    int          nGCPCount;
    GDAL_GCP    *pasGCPs;
    CPLString    osGCPProjection;

    int          m_nOverviewDSCount;
    GDALDataset  **m_papoOverviewDS;

#if 0
  protected:
    virtual int                 EnterReadWrite(GDALRWFlag eRWFlag);
    virtual void                LeaveReadWrite();
#endif

  public:
                 MEMDataset();
    virtual      ~MEMDataset();

    virtual const char *GetProjectionRef() CPL_OVERRIDE;
    virtual CPLErr SetProjection( const char * ) CPL_OVERRIDE;

    virtual CPLErr GetGeoTransform( double * ) CPL_OVERRIDE;
    virtual CPLErr SetGeoTransform( double * ) CPL_OVERRIDE;

    virtual void *GetInternalHandle( const char * ) CPL_OVERRIDE;

    virtual int    GetGCPCount() CPL_OVERRIDE;
    virtual const char *GetGCPProjection() CPL_OVERRIDE;
    virtual const GDAL_GCP *GetGCPs() CPL_OVERRIDE;
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection ) CPL_OVERRIDE;

    virtual CPLErr        AddBand( GDALDataType eType,
                                   char **papszOptions=NULL ) CPL_OVERRIDE;
    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpaceBuf,
                               GSpacing nLineSpaceBuf,
                               GSpacing nBandSpaceBuf,
                               GDALRasterIOExtraArg* psExtraArg) CPL_OVERRIDE;
    virtual CPLErr  IBuildOverviews( const char *pszResampling,
                                     int nOverviews, int *panOverviewList,
                                     int nListBands, int *panBandList,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData ) CPL_OVERRIDE;

    virtual CPLErr          CreateMaskBand( int nFlagsIn ) CPL_OVERRIDE;

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
  private:
                MEMRasterBand( GByte *pabyDataIn, GDALDataType eTypeIn,
                               int nXSizeIn, int nYSizeIn );

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
                                  int bAssumeOwnership,
                                  const char * pszPixelType = NULL );
    virtual        ~MEMRasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) CPL_OVERRIDE;
    virtual CPLErr IWriteBlock( int, int, void * ) CPL_OVERRIDE;
    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpaceBuf,
                                  GSpacing nLineSpaceBuf,
                                  GDALRasterIOExtraArg* psExtraArg ) CPL_OVERRIDE;
    virtual double GetNoDataValue( int *pbSuccess = NULL ) CPL_OVERRIDE;
    virtual CPLErr SetNoDataValue( double ) CPL_OVERRIDE;
    virtual CPLErr DeleteNoDataValue() CPL_OVERRIDE;

    virtual GDALColorInterp GetColorInterpretation() CPL_OVERRIDE;
    virtual GDALColorTable *GetColorTable() CPL_OVERRIDE;
    virtual CPLErr SetColorTable( GDALColorTable * ) CPL_OVERRIDE;

    virtual CPLErr SetColorInterpretation( GDALColorInterp ) CPL_OVERRIDE;

    virtual const char *GetUnitType() CPL_OVERRIDE;
    CPLErr SetUnitType( const char * ) CPL_OVERRIDE;

    virtual char **GetCategoryNames() CPL_OVERRIDE;
    virtual CPLErr SetCategoryNames( char ** ) CPL_OVERRIDE;

    virtual double GetOffset( int *pbSuccess = NULL ) CPL_OVERRIDE;
    CPLErr SetOffset( double ) CPL_OVERRIDE;
    virtual double GetScale( int *pbSuccess = NULL ) CPL_OVERRIDE;
    CPLErr SetScale( double ) CPL_OVERRIDE;

    virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig *panHistogram ) CPL_OVERRIDE;
    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets,
                                        GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData) CPL_OVERRIDE;

    virtual int GetOverviewCount() CPL_OVERRIDE;
    virtual GDALRasterBand *GetOverview(int) CPL_OVERRIDE;

    virtual CPLErr          CreateMaskBand( int nFlagsIn ) CPL_OVERRIDE;

    // Allow access to MEM driver's private internal memory buffer.
    GByte *GetData() const { return(pabyData); }
};

#endif /* ndef MEMDATASET_H_INCLUDED */
