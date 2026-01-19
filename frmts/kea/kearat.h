/*
 *  kearat.h
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef KEARAT_H
#define KEARAT_H

#include "gdal_priv.h"
#include "gdal_rat.h"

#include "keaband.h"

class KEARasterAttributeTable final : public GDALRasterAttributeTable
{
  private:
    kealib::KEAAttributeTable *m_poKEATable;
    std::vector<kealib::KEAATTField> m_aoFields;
    CPLString osWorkingResult;
    KEARasterBand *m_poBand;
    CPLMutex *m_hMutex;
    mutable std::vector<GByte> m_abyCachedWKB{};

  public:
    KEARasterAttributeTable(kealib::KEAAttributeTable *poKEATable,
                            KEARasterBand *poBand);
    ~KEARasterAttributeTable() override;

    GDALDefaultRasterAttributeTable *Clone() const override;

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
    CPLErr SetValue(int iRow, int iField, bool nValue) override;
    CPLErr SetValue(int iRow, int iField,
                    const GDALRATDateTime &sDateTime) override;
    CPLErr SetValue(int iRow, int iField, const void *pabyWKB,
                    size_t nWKBSize) override;

    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    double *pdfData) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    int *pnData) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    bool *pbData) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    char **papszStrList) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    GDALRATDateTime *pasDateTime) override;
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    GByte **ppabyWKB, size_t *pnWKBSize) override;

    int ChangesAreWrittenToFile() override;
    void SetRowCount(int iCount) override;

    virtual CPLErr CreateColumn(const char *pszFieldName,
                                GDALRATFieldType eFieldType,
                                GDALRATFieldUsage eFieldUsage) override;
    virtual CPLErr SetLinearBinning(double dfRow0Min,
                                    double dfBinSize) override;
    virtual int GetLinearBinning(double *pdfRow0Min,
                                 double *pdfBinSize) const override;

    CPLXMLNode *Serialize() const override;

    GDALRATTableType GetTableType() const override;
    CPLErr SetTableType(const GDALRATTableType eInTableType) override;
    void RemoveStatistics() override;
};

#endif  // KEARAT_H
