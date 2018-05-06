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
#include "gdal_rat.h"

#include <memory>

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
    CPL_DISALLOW_COPY_ASSIGN(MEMDataset)

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

    virtual const char *GetProjectionRef() override;
    virtual CPLErr SetProjection( const char * ) override;

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual CPLErr SetGeoTransform( double * ) override;

    virtual void *GetInternalHandle( const char * ) override;

    virtual int    GetGCPCount() override;
    virtual const char *GetGCPProjection() override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection ) override;

    virtual CPLErr        AddBand( GDALDataType eType,
                                   char **papszOptions=nullptr ) override;
    virtual CPLErr  IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpaceBuf,
                               GSpacing nLineSpaceBuf,
                               GSpacing nBandSpaceBuf,
                               GDALRasterIOExtraArg* psExtraArg) override;
    virtual CPLErr  IBuildOverviews( const char *pszResampling,
                                     int nOverviews, int *panOverviewList,
                                     int nListBands, int *panBandList,
                                     GDALProgressFunc pfnProgress,
                                     void * pProgressData ) override;

    virtual CPLErr          CreateMaskBand( int nFlagsIn ) override;

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

    CPL_DISALLOW_COPY_ASSIGN(MEMRasterBand)

  protected:
    friend      class MEMDataset;

    GByte      *pabyData;
    GSpacing    nPixelOffset;
    GSpacing    nLineOffset;
    int         bOwnData;

    int         bNoDataSet;
    double      dfNoData;

    std::unique_ptr<GDALColorTable> m_poColorTable;
    GDALColorInterp eColorInterp;

    CPLString      m_osUnitType;
    CPLStringList  m_aosCategoryNames;

    double         dfOffset;
    double         dfScale;

    CPLXMLNode    *psSavedHistograms;

    std::unique_ptr<GDALRasterAttributeTable> m_poRAT;

  public:
                   MEMRasterBand( GDALDataset *poDS, int nBand,
                                  GByte *pabyData, GDALDataType eType,
                                  GSpacing nPixelOffset, GSpacing nLineOffset,
                                  int bAssumeOwnership,
                                  const char * pszPixelType = nullptr );
    virtual        ~MEMRasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;
    virtual CPLErr IRasterIO( GDALRWFlag eRWFlag,
                                  int nXOff, int nYOff, int nXSize, int nYSize,
                                  void * pData, int nBufXSize, int nBufYSize,
                                  GDALDataType eBufType,
                                  GSpacing nPixelSpaceBuf,
                                  GSpacing nLineSpaceBuf,
                                  GDALRasterIOExtraArg* psExtraArg ) override;
    virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
    virtual CPLErr SetNoDataValue( double ) override;
    virtual CPLErr DeleteNoDataValue() override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;
    virtual CPLErr SetColorTable( GDALColorTable * ) override;

    virtual GDALRasterAttributeTable *GetDefaultRAT() override;
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * poRAT ) override;

    virtual CPLErr SetColorInterpretation( GDALColorInterp ) override;

    virtual const char *GetUnitType() override;
    CPLErr SetUnitType( const char * ) override;

    virtual char **GetCategoryNames() override;
    virtual CPLErr SetCategoryNames( char ** ) override;

    virtual double GetOffset( int *pbSuccess = nullptr ) override;
    CPLErr SetOffset( double ) override;
    virtual double GetScale( int *pbSuccess = nullptr ) override;
    CPLErr SetScale( double ) override;

    virtual CPLErr SetDefaultHistogram( double dfMin, double dfMax,
                                        int nBuckets, GUIntBig *panHistogram ) override;
    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets,
                                        GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData) override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int) override;

    virtual CPLErr          CreateMaskBand( int nFlagsIn ) override;

    // Allow access to MEM driver's private internal memory buffer.
    GByte *GetData() const { return(pabyData); }
};

#endif /* ndef MEMDATASET_H_INCLUDED */
