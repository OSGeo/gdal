/******************************************************************************
 *
 * Name:     hfadataset.cpp
 * Project:  Erdas Imagine Driver
 * Purpose:  Main driver for Erdas Imagine format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef HFADATASET_H_INCLUDED
#define HFADATASET_H_INCLUDED

#include <cstddef>
#include <vector>

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_rat.h"
#include "hfa_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"

/************************************************************************/
/* ==================================================================== */
/*                              HFADataset                              */
/* ==================================================================== */
/************************************************************************/

class HFARasterBand;

class HFADataset final : public GDALPamDataset
{
    friend class HFARasterBand;

    HFAHandle   hHFA;

    bool        bMetadataDirty;

    bool        bGeoDirty;
    double      adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};

    bool        bIgnoreUTM;

    CPLErr      ReadProjection();
    CPLErr      WriteProjection();
    bool        bForceToPEString;

    int         nGCPCount;
    GDAL_GCP    asGCPList[36];

    void        UseXFormStack( int nStepCount,
                               Efga_Polynomial *pasPolyListForward,
                               Efga_Polynomial *pasPolyListReverse );

  protected:
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;

  public:
             HFADataset();
    virtual ~HFADataset();

    static int          Identify( GDALOpenInfo * );
    static CPLErr       Rename( const char *pszNewName,
                                const char *pszOldName );
    static CPLErr       CopyFiles( const char *pszNewName,
                                   const char *pszOldName );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParamList );
    static GDALDataset *CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );
    static CPLErr       Delete( const char *pszFilename );

    virtual char **GetFileList() override;

    const OGRSpatialReference* GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override;

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual CPLErr SetGeoTransform( double * ) override;

    virtual int    GetGCPCount() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;

    virtual CPLErr SetMetadata( char **, const char * = "" ) override;
    virtual CPLErr SetMetadataItem( const char *, const char *,
                                    const char * = "" ) override;

    virtual void   FlushCache() override;
    virtual CPLErr IBuildOverviews( const char *pszResampling,
                                    int nOverviews, int *panOverviewList,
                                    int nListBands, int *panBandList,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                            HFARasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HFARasterBand final : public GDALPamRasterBand
{
    friend class HFADataset;
    friend class HFARasterAttributeTable;

    GDALColorTable *poCT;

    EPTType     eHFADataType;

    int         nOverviews;
    int         nThisOverview;
    HFARasterBand **papoOverviewBands;

    CPLErr      CleanOverviews();

    HFAHandle   hHFA;

    bool        bMetadataDirty;

    GDALRasterAttributeTable *poDefaultRAT;

    void        ReadAuxMetadata();
    void        ReadHistogramMetadata();
    void        EstablishOverviews();
    CPLErr      WriteNamedRAT( const char *pszName,
                               const GDALRasterAttributeTable *poRAT );

  public:
                   HFARasterBand( HFADataset *, int, int );
    virtual        ~HFARasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;

    virtual const char *GetDescription() const override;
    virtual void        SetDescription( const char * ) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;
    virtual CPLErr          SetColorTable( GDALColorTable * ) override;
    virtual int    GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview( int ) override;

    virtual double GetMinimum( int *pbSuccess = nullptr ) override;
    virtual double GetMaximum( int *pbSuccess = nullptr ) override;
    virtual double GetNoDataValue( int *pbSuccess = nullptr ) override;
    virtual CPLErr SetNoDataValue( double dfValue ) override;

    virtual CPLErr SetMetadata( char **, const char * = "" ) override;
    virtual CPLErr SetMetadataItem( const char *, const char *,
                                    const char * = "" ) override;
    virtual CPLErr BuildOverviews( const char *, int, int *,
                                   GDALProgressFunc, void * ) override;

    virtual CPLErr GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                        int *pnBuckets,
                                        GUIntBig ** ppanHistogram,
                                        int bForce,
                                        GDALProgressFunc, void *pProgressData ) override;

    virtual GDALRasterAttributeTable *GetDefaultRAT() override;
    virtual CPLErr SetDefaultRAT( const GDALRasterAttributeTable * ) override;
};

class HFAAttributeField
{
  public:
    CPLString         sName;
    GDALRATFieldType  eType;
    GDALRATFieldUsage eUsage;
    int               nDataOffset;
    int               nElementSize;
    HFAEntry         *poColumn;
    bool              bIsBinValues;  // Handled differently.
    bool              bConvertColors;  // Map 0-1 floats to 0-255 ints.
};

class HFARasterAttributeTable final : public GDALRasterAttributeTable
{
  private:
    HFAHandle   hHFA;
    HFAEntry   *poDT;
    CPLString   osName;
    int         nBand;
    GDALAccess  eAccess;

    std::vector<HFAAttributeField> aoFields;
    int         nRows;

    bool bLinearBinning;
    double dfRow0Min;
    double dfBinSize;
    GDALRATTableType eTableType;

    CPLString osWorkingResult;

    void AddColumn( const char *pszName, GDALRATFieldType eType,
                    GDALRATFieldUsage eUsage,
                    int nDataOffset, int nElementSize, HFAEntry *poColumn,
                    bool bIsBinValues=false,
                    bool bConvertColors=false )
    {
        HFAAttributeField aField;
        aField.sName = pszName;
        aField.eType = eType;
        aField.eUsage = eUsage;
        aField.nDataOffset = nDataOffset;
        aField.nElementSize = nElementSize;
        aField.poColumn = poColumn;
        aField.bIsBinValues = bIsBinValues;
        aField.bConvertColors = bConvertColors;

        aoFields.push_back(aField);
    }

    void CreateDT()
    {
        poDT = HFAEntry::New( hHFA->papoBand[nBand-1]->psInfo,
                              osName, "Edsc_Table",
                              hHFA->papoBand[nBand-1]->poNode );
        poDT->SetIntField( "numrows", nRows );
    }

  public:
    HFARasterAttributeTable( HFARasterBand *poBand, const char *pszName );
    virtual ~HFARasterAttributeTable();

    GDALRasterAttributeTable *Clone() const override;

    virtual int           GetColumnCount() const override;

    virtual const char   *GetNameOfCol( int ) const override;
    virtual GDALRATFieldUsage GetUsageOfCol( int ) const override;
    virtual GDALRATFieldType GetTypeOfCol( int ) const override;

    virtual int           GetColOfUsage( GDALRATFieldUsage ) const override;

    virtual int           GetRowCount() const override;

    virtual const char   *GetValueAsString( int iRow, int iField ) const override;
    virtual int           GetValueAsInt( int iRow, int iField ) const override;
    virtual double        GetValueAsDouble( int iRow, int iField ) const override;

    virtual void          SetValue( int iRow, int iField,
                                    const char *pszValue ) override;
    virtual void          SetValue( int iRow, int iField, double dfValue ) override;
    virtual void          SetValue( int iRow, int iField, int nValue ) override;

    virtual CPLErr        ValuesIO( GDALRWFlag eRWFlag, int iField,
                                    int iStartRow, int iLength,
                                    double *pdfData ) override;
    virtual CPLErr        ValuesIO( GDALRWFlag eRWFlag, int iField,
                                    int iStartRow, int iLength, int *pnData ) override;
    virtual CPLErr        ValuesIO( GDALRWFlag eRWFlag, int iField,
                                    int iStartRow, int iLength,
                                    char **papszStrList ) override;

    virtual int           ChangesAreWrittenToFile() override;
    virtual void          SetRowCount( int iCount ) override;

    virtual int           GetRowOfValue( double dfValue ) const override;
    virtual int           GetRowOfValue( int nValue ) const override;

    virtual CPLErr        CreateColumn( const char *pszFieldName,
                                        GDALRATFieldType eFieldType,
                                        GDALRATFieldUsage eFieldUsage ) override;
    virtual CPLErr        SetLinearBinning( double dfRow0Min,
                                            double dfBinSize ) override;
    virtual int           GetLinearBinning( double *pdfRow0Min,
                                            double *pdfBinSize ) const override;

    virtual CPLXMLNode   *Serialize() const override;

    virtual CPLErr        SetTableType(const GDALRATTableType eInTableType) override;
    virtual GDALRATTableType GetTableType() const override;
    virtual void          RemoveStatistics() override;

protected:
    CPLErr                ColorsIO( GDALRWFlag eRWFlag, int iField,
                                    int iStartRow, int iLength, int *pnData );
};

#endif  // HFADATASET_H_INCLUDED
