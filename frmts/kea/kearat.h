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

class KEARasterAttributeTable : public GDALDefaultRasterAttributeTable
{
  private:
    kealib::KEAAttributeTable *m_poKEATable;
    std::vector<kealib::KEAATTField> m_aoFields;
    CPLString osWorkingResult;
    KEARasterBand *m_poBand;
    CPLMutex *m_hMutex;

  public:
    KEARasterAttributeTable(kealib::KEAAttributeTable *poKEATable,
                            KEARasterBand *poBand);
    ~KEARasterAttributeTable();

    GDALDefaultRasterAttributeTable *Clone() const override;

    virtual int GetColumnCount() const override;

    virtual const char *GetNameOfCol(int) const override;
    virtual GDALRATFieldUsage GetUsageOfCol(int) const override;
    virtual GDALRATFieldType GetTypeOfCol(int) const override;

    virtual int GetColOfUsage(GDALRATFieldUsage) const override;

    virtual int GetRowCount() const override;

    virtual const char *GetValueAsString(int iRow, int iField) const override;
    virtual int GetValueAsInt(int iRow, int iField) const override;
    virtual double GetValueAsDouble(int iRow, int iField) const override;

    virtual void SetValue(int iRow, int iField, const char *pszValue) override;
    virtual void SetValue(int iRow, int iField, double dfValue) override;
    virtual void SetValue(int iRow, int iField, int nValue) override;

    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, double *pdfData) override;
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, int *pnData) override;
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, char **papszStrList) override;

    virtual int ChangesAreWrittenToFile() override;
    virtual void SetRowCount(int iCount) override;

    virtual CPLErr CreateColumn(const char *pszFieldName,
                                GDALRATFieldType eFieldType,
                                GDALRATFieldUsage eFieldUsage) override;
    virtual CPLErr SetLinearBinning(double dfRow0Min,
                                    double dfBinSize) override;
    virtual int GetLinearBinning(double *pdfRow0Min,
                                 double *pdfBinSize) const override;

    virtual CPLXMLNode *Serialize() const override;

    virtual GDALRATTableType GetTableType() const override;
    virtual CPLErr SetTableType(const GDALRATTableType eInTableType) override;
};

#endif  // KEARAT_H
