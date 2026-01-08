/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  RAT utility
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "rat.h"
#include "ogr_p.h"

#include <cmath>

/************************************************************************/
/*                             CreateRAT()                              */
/************************************************************************/

std::unique_ptr<GDALRasterAttributeTable>
HDF5CreateRAT(const std::shared_ptr<GDALMDArray> &poValues,
              bool bFirstColIsMinMax)
{
    auto poRAT = std::make_unique<GDALDefaultRasterAttributeTable>();
    const auto &poComponents = poValues->GetDataType().GetComponents();
    for (const auto &poComponent : poComponents)
    {
        GDALRATFieldType eType;
        if (poComponent->GetType().GetClass() == GEDTC_NUMERIC)
        {
            if (GDALDataTypeIsInteger(
                    poComponent->GetType().GetNumericDataType()))
            {
                // S102 featureAttributeTable
                if (poComponent->GetName() ==
                        "featuresDetected."
                        "leastDepthOfDetectedFeaturesMeasured" ||
                    poComponent->GetName() ==
                        "featuresDetected.significantFeaturesDetected" ||
                    poComponent->GetName() == "fullSeafloorCoverageAchieved" ||
                    poComponent->GetName() == "bathyCoverage")
                {
                    eType = GFT_Boolean;
                }
                else
                {
                    eType = GFT_Integer;
                }
            }
            else
                eType = GFT_Real;
        }
        else
        {
            // S102 featureAttributeTable
            if (poComponent->GetName() == "surveyDateRange.dateStart" ||
                poComponent->GetName() == "surveyDateRange.dateEnd")
                eType = GFT_DateTime;
            else
                eType = GFT_String;
        }
        poRAT->CreateColumn(poComponent->GetName().c_str(), eType,
                            bFirstColIsMinMax && poRAT->GetColumnCount() == 0
                                ? GFU_MinMax
                                : GFU_Generic);
    }

    const auto &oValuesDT = poValues->GetDataType();
    std::vector<GByte> abyRow(oValuesDT.GetSize());
    const int nRows = static_cast<int>(poValues->GetDimensions()[0]->GetSize());
    for (int iRow = 0; iRow < nRows; iRow++)
    {
        const GUInt64 arrayStartIdx = static_cast<GUInt64>(iRow);
        const size_t count = 1;
        const GInt64 arrayStep = 0;
        const GPtrDiff_t bufferStride = 0;
        poValues->Read(&arrayStartIdx, &count, &arrayStep, &bufferStride,
                       oValuesDT, &abyRow[0]);
        int iCol = 0;
        for (const auto &poComponent : poComponents)
        {
            const auto eRATType = poRAT->GetTypeOfCol(iCol);
            if (eRATType == GFT_Integer || eRATType == GFT_Boolean)
            {
                int nValue = 0;
                GDALCopyWords(&abyRow[poComponent->GetOffset()],
                              poComponent->GetType().GetNumericDataType(), 0,
                              &nValue, GDT_Int32, 0, 1);
                poRAT->SetValue(iRow, iCol, nValue);
            }
            else if (eRATType == GFT_Real)
            {
                double dfValue = 0;
                GDALCopyWords(&abyRow[poComponent->GetOffset()],
                              poComponent->GetType().GetNumericDataType(), 0,
                              &dfValue, GDT_Float64, 0, 1);
                poRAT->SetValue(iRow, iCol, dfValue);
            }
            else
            {
                char *pszStr = nullptr;
                GDALExtendedDataType::CopyValue(
                    &abyRow[poComponent->GetOffset()], poComponent->GetType(),
                    &pszStr, GDALExtendedDataType::CreateString());
                if (pszStr)
                {
                    if (eRATType == GFT_DateTime)
                    {
                        GDALRATDateTime sDateTime;
                        if (strlen(pszStr) == 8 &&
                            sscanf(pszStr, "%04d%02d%02d", &(sDateTime.nYear),
                                   &(sDateTime.nMonth), &(sDateTime.nDay)) == 3)
                        {
                            sDateTime.bPositiveTimeZone = true;
                            sDateTime.bIsValid = true;
                        }
                        else
                        {
                            OGRField sField;
                            if (OGRParseDate(pszStr, &sField, 0))
                            {
                                sDateTime.nYear = sField.Date.Year;
                                sDateTime.nMonth = sField.Date.Month;
                                sDateTime.nDay = sField.Date.Day;
                                sDateTime.nHour = sField.Date.Hour;
                                sDateTime.nMinute = sField.Date.Minute;
                                sDateTime.fSecond = sField.Date.Second;
                                sDateTime.bPositiveTimeZone =
                                    sField.Date.TZFlag >= 100 ||
                                    sField.Date.TZFlag <= 2;
                                if (sField.Date.TZFlag > 2)
                                {
                                    sDateTime.nTimeZoneHour =
                                        std::abs(sField.Date.TZFlag - 100) / 4;
                                    sDateTime.nTimeZoneMinute =
                                        (std::abs(sField.Date.TZFlag - 100) %
                                         4) *
                                        15;
                                }
                                sDateTime.bIsValid = true;
                            }
                        }
                        poRAT->SetValue(iRow, iCol, sDateTime);
                    }
                    else
                    {
                        poRAT->SetValue(iRow, iCol, pszStr);
                    }
                }
                CPLFree(pszStr);
            }
            iCol++;
        }
        oValuesDT.FreeDynamicMemory(&abyRow[0]);
    }
    return poRAT;
}
