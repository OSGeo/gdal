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
 * SPDX-License-Identifier: MIT
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

    HFAHandle hHFA = nullptr;

    bool bMetadataDirty = false;

    bool bGeoDirty = false;
    GDALGeoTransform m_gt{};
    OGRSpatialReference m_oSRS{};

    bool bIgnoreUTM = false;

    CPLErr ReadProjection();
    CPLErr WriteProjection();
    bool bForceToPEString = false;
    bool bDisablePEString = false;

    std::vector<gdal::GCP> m_aoGCPs{};

    void UseXFormStack(int nStepCount, Efga_Polynomial *pasPolyListForward,
                       Efga_Polynomial *pasPolyListReverse);

  protected:
    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, int, BANDMAP_TYPE, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

  public:
    HFADataset();
    ~HFADataset() override;

    static int Identify(GDALOpenInfo *);
    static CPLErr Rename(const char *pszNewName, const char *pszOldName);
    static CPLErr CopyFiles(const char *pszNewName, const char *pszOldName);
    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszParamList);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
    static CPLErr Delete(const char *pszFilename);

    char **GetFileList() override;

    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    CPLErr SetGeoTransform(const GDALGeoTransform &gt) override;

    int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    const GDAL_GCP *GetGCPs() override;

    CPLErr SetMetadata(CSLConstList, const char * = "") override;
    CPLErr SetMetadataItem(const char *, const char *,
                           const char * = "") override;

    CPLErr FlushCache(bool bAtClosing) override;
    CPLErr IBuildOverviews(const char *pszResampling, int nOverviews,
                           const int *panOverviewList, int nListBands,
                           const int *panBandList, GDALProgressFunc pfnProgress,
                           void *pProgressData,
                           CSLConstList papszOptions) override;
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

    EPTType eHFADataType;

    int nOverviews;
    int nThisOverview;
    HFARasterBand **papoOverviewBands;

    CPLErr CleanOverviews();

    HFAHandle hHFA;

    bool bMetadataDirty;

    GDALRasterAttributeTable *poDefaultRAT;

    void ReadAuxMetadata();
    void ReadHistogramMetadata();
    void EstablishOverviews();
    CPLErr WriteNamedRAT(const char *pszName,
                         const GDALRasterAttributeTable *poRAT);

  public:
    HFARasterBand(HFADataset *, int, int);
    ~HFARasterBand() override;

    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IWriteBlock(int, int, void *) override;

    const char *GetDescription() const override;
    void SetDescription(const char *) override;

    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
    CPLErr SetColorTable(GDALColorTable *) override;
    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int) override;

    double GetMinimum(int *pbSuccess = nullptr) override;
    double GetMaximum(int *pbSuccess = nullptr) override;
    double GetNoDataValue(int *pbSuccess = nullptr) override;
    CPLErr SetNoDataValue(double dfValue) override;

    CPLErr SetMetadata(CSLConstList, const char * = "") override;
    CPLErr SetMetadataItem(const char *, const char *,
                           const char * = "") override;
    virtual CPLErr BuildOverviews(const char *, int, const int *,
                                  GDALProgressFunc, void *,
                                  CSLConstList papszOptions) override;

    CPLErr GetDefaultHistogram(double *pdfMin, double *pdfMax, int *pnBuckets,
                               GUIntBig **ppanHistogram, int bForce,
                               GDALProgressFunc, void *pProgressData) override;

    GDALRasterAttributeTable *GetDefaultRAT() override;
    CPLErr SetDefaultRAT(const GDALRasterAttributeTable *) override;
};

class HFAAttributeField
{
  public:
    CPLString sName;
    GDALRATFieldType eType;
    GDALRATFieldUsage eUsage;
    int nDataOffset;
    int nElementSize;
    HFAEntry *poColumn;
    bool bIsBinValues;    // Handled differently.
    bool bConvertColors;  // Map 0-1 floats to 0-255 ints.
};

class HFARasterAttributeTable final : public GDALRasterAttributeTable
{
  private:
    HFAHandle hHFA;
    HFAEntry *poDT;
    CPLString osName;
    int nBand;
    GDALAccess eAccess;

    std::vector<HFAAttributeField> aoFields;
    int nRows;
    mutable std::vector<GByte> m_abyWKB{};

    bool bLinearBinning;
    double dfRow0Min;
    double dfBinSize;
    GDALRATTableType eTableType;

    CPLString osWorkingResult;

    void AddColumn(const char *pszName, GDALRATFieldType eType,
                   GDALRATFieldUsage eUsage, int nDataOffset, int nElementSize,
                   HFAEntry *poColumn, bool bIsBinValues = false,
                   bool bConvertColors = false)
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

        aoFields.push_back(std::move(aField));
    }

    void CreateDT()
    {
        poDT = HFAEntry::New(hHFA->papoBand[nBand - 1]->psInfo, osName,
                             "Edsc_Table", hHFA->papoBand[nBand - 1]->poNode);
        poDT->SetIntField("numrows", nRows);
    }

  public:
    HFARasterAttributeTable(HFARasterBand *poBand, const char *pszName);
    ~HFARasterAttributeTable() override;

    GDALRasterAttributeTable *Clone() const override;

    int GetColumnCount() const override;

    const char *GetNameOfCol(int) const override;
    GDALRATFieldUsage GetUsageOfCol(int) const override;
    GDALRATFieldType GetTypeOfCol(int) const override;

    int GetColOfUsage(GDALRATFieldUsage) const override;

    int GetRowCount() const override;

    const char *GetValueAsString(int iRow, int iField) const override;
    int GetValueAsInt(int iRow, int iField) const override;
    double GetValueAsDouble(int iRow, int iField) const override;
    bool GetValueAsBoolean(int iRow, int iField) const override;
    GDALRATDateTime GetValueAsDateTime(int iRow, int iField) const override;
    const GByte *GetValueAsWKBGeometry(int iRow, int iField,
                                       size_t &nWKBSize) const override;

    CPLErr SetValue(int iRow, int iField, const char *pszValue) override;
    CPLErr SetValue(int iRow, int iField, double dfValue) override;
    CPLErr SetValue(int iRow, int iField, int nValue) override;
    CPLErr SetValue(int iRow, int iField, bool bValue) override;
    CPLErr SetValue(int iRow, int iField,
                    const GDALRATDateTime &sDateTime) override;
    CPLErr SetValue(int iRow, int iField, const void *pabyWKB,
                    size_t nWKBSize) override;

    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    double *pdfData) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    int *pnData) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    char **papszStrList) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    bool *pbData) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    GDALRATDateTime *psDateTime) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    GByte **ppabyWKB, size_t *pnWKBSize) override;

    int ChangesAreWrittenToFile() override;
    void SetRowCount(int iCount) override;

    int GetRowOfValue(double dfValue) const override;
    int GetRowOfValue(int nValue) const override;

    virtual CPLErr CreateColumn(const char *pszFieldName,
                                GDALRATFieldType eFieldType,
                                GDALRATFieldUsage eFieldUsage) override;
    virtual CPLErr SetLinearBinning(double dfRow0Min,
                                    double dfBinSize) override;
    virtual int GetLinearBinning(double *pdfRow0Min,
                                 double *pdfBinSize) const override;

    CPLXMLNode *Serialize() const override;

    CPLErr SetTableType(const GDALRATTableType eInTableType) override;
    GDALRATTableType GetTableType() const override;
    void RemoveStatistics() override;

  protected:
    CPLErr ColorsIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    int *pnData);
};

#endif  // HFADATASET_H_INCLUDED
