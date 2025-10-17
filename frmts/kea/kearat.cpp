/*
 *  kearat.cpp
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "kearat.h"

KEARasterAttributeTable::KEARasterAttributeTable(
    kealib::KEAAttributeTable *poKEATable, KEARasterBand *poBand)
{
    this->m_hMutex = CPLCreateMutex();
    CPLReleaseMutex(this->m_hMutex);
    for (size_t nColumnIndex = 0;
         nColumnIndex < poKEATable->getMaxGlobalColIdx(); nColumnIndex++)
    {
        kealib::KEAATTField sKEAField;
        try
        {
            sKEAField = poKEATable->getField(nColumnIndex);
        }
        catch (const kealib::KEAATTException &)
        {
            // pKEATable->getField raised exception because we have a missing
            // column
            continue;
        }
        m_aoFields.push_back(std::move(sKEAField));
    }
    m_poKEATable = poKEATable;
    m_poBand = poBand;
}

KEARasterAttributeTable::~KEARasterAttributeTable()
{
    // can't just delete thanks to Windows
    kealib::KEAAttributeTable::destroyAttributeTable(m_poKEATable);
    CPLDestroyMutex(m_hMutex);
    m_hMutex = nullptr;
}

GDALDefaultRasterAttributeTable *KEARasterAttributeTable::Clone() const
{
    const int nColCount = GetColumnCount();
    if (nColCount > 0 && GetRowCount() > RAT_MAX_ELEM_FOR_CLONE / nColCount)
        return nullptr;

    auto poRAT = std::make_unique<GDALDefaultRasterAttributeTable>();

    for (int iCol = 0; iCol < (int)m_aoFields.size(); iCol++)
    {
        const auto &sName = m_aoFields[iCol].name;
        const auto &sUsage = m_aoFields[iCol].usage;
        GDALRATFieldUsage eGDALUsage;
        if (sUsage == "PixelCount")
            eGDALUsage = GFU_PixelCount;
        else if (sUsage == "Name")
            eGDALUsage = GFU_Name;
        else if (sUsage == "Red")
            eGDALUsage = GFU_Red;
        else if (sUsage == "Green")
            eGDALUsage = GFU_Green;
        else if (sUsage == "Blue")
            eGDALUsage = GFU_Blue;
        else if (sUsage == "Alpha")
            eGDALUsage = GFU_Alpha;
        else
        {
            // don't recognise any other special names - generic column
            eGDALUsage = GFU_Generic;
        }

        const GDALRATFieldType eGDALType = GetTypeOfCol(iCol);
        poRAT->CreateColumn(sName.c_str(), eGDALType, eGDALUsage);
        poRAT->SetRowCount(static_cast<int>(m_poKEATable->getSize()));

        if (m_poKEATable->getSize() == 0)
            continue;

        switch (eGDALType)
        {
            case GFT_Integer:
            {
                int *panColData = (int *)VSI_MALLOC2_VERBOSE(
                    sizeof(int), m_poKEATable->getSize());
                if (panColData == nullptr)
                {
                    return nullptr;
                }

                if ((const_cast<KEARasterAttributeTable *>(this))
                        ->ValuesIO(GF_Read, iCol, 0,
                                   static_cast<int>(m_poKEATable->getSize()),
                                   panColData) != CE_None)
                {
                    CPLFree(panColData);
                    return nullptr;
                }

                for (int iRow = 0; iRow < (int)m_poKEATable->getSize(); iRow++)
                {
                    poRAT->SetValue(iRow, iCol, panColData[iRow]);
                }
                CPLFree(panColData);
                break;
            }

            case GFT_Real:
            {
                double *padfColData = (double *)VSI_MALLOC2_VERBOSE(
                    sizeof(double), m_poKEATable->getSize());
                if (padfColData == nullptr)
                {
                    return nullptr;
                }
                if ((const_cast<KEARasterAttributeTable *>(this))
                        ->ValuesIO(GF_Read, iCol, 0,
                                   static_cast<int>(m_poKEATable->getSize()),
                                   padfColData) != CE_None)
                {
                    CPLFree(padfColData);
                    return nullptr;
                }

                for (int iRow = 0; iRow < (int)m_poKEATable->getSize(); iRow++)
                {
                    poRAT->SetValue(iRow, iCol, padfColData[iRow]);
                }
                CPLFree(padfColData);
                break;
            }

            case GFT_String:
            {
                char **papszColData = (char **)VSI_MALLOC2_VERBOSE(
                    sizeof(char *), m_poKEATable->getSize());
                if (papszColData == nullptr)
                {
                    return nullptr;
                }

                if ((const_cast<KEARasterAttributeTable *>(this))
                        ->ValuesIO(GF_Read, iCol, 0,
                                   static_cast<int>(m_poKEATable->getSize()),
                                   papszColData) != CE_None)
                {
                    CPLFree(papszColData);
                    return nullptr;
                }

                for (int iRow = 0; iRow < (int)m_poKEATable->getSize(); iRow++)
                {
                    poRAT->SetValue(iRow, iCol, papszColData[iRow]);
                    CPLFree(papszColData[iRow]);
                }
                CPLFree(papszColData);
                break;
            }

            case GFT_Boolean:
            {
                bool *pabColData = (bool *)VSI_MALLOC2_VERBOSE(
                    sizeof(bool), m_poKEATable->getSize());
                if (pabColData == nullptr)
                {
                    return nullptr;
                }

                if ((const_cast<KEARasterAttributeTable *>(this))
                        ->ValuesIO(GF_Read, iCol, 0,
                                   static_cast<int>(m_poKEATable->getSize()),
                                   pabColData) != CE_None)
                {
                    CPLFree(pabColData);
                    return nullptr;
                }

                for (int iRow = 0; iRow < (int)m_poKEATable->getSize(); iRow++)
                {
                    poRAT->SetValue(iRow, iCol, pabColData[iRow]);
                }
                CPLFree(pabColData);
                break;
            }

            case GFT_DateTime:
            case GFT_WKBGeometry:
                CPLAssert(false);
                break;
        }
    }

    poRAT->SetTableType(this->GetTableType());

    return poRAT.release();
}

int KEARasterAttributeTable::GetColumnCount() const
{
    return (int)m_aoFields.size();
}

const char *KEARasterAttributeTable::GetNameOfCol(int nCol) const
{
    if ((nCol < 0) || (nCol >= (int)m_aoFields.size()))
        return nullptr;

    return m_aoFields[nCol].name.c_str();
}

GDALRATFieldUsage KEARasterAttributeTable::GetUsageOfCol(int nCol) const
{
    if ((nCol < 0) || (nCol >= (int)m_aoFields.size()))
        return GFU_Generic;

    GDALRATFieldUsage eGDALUsage;
    std::string keausage = m_aoFields[nCol].usage;

    if (keausage == "PixelCount")
        eGDALUsage = GFU_PixelCount;
    else if (keausage == "Name")
        eGDALUsage = GFU_Name;
    else if (keausage == "Red")
        eGDALUsage = GFU_Red;
    else if (keausage == "Green")
        eGDALUsage = GFU_Green;
    else if (keausage == "Blue")
        eGDALUsage = GFU_Blue;
    else if (keausage == "Alpha")
        eGDALUsage = GFU_Alpha;
    else
    {
        // don't recognise any other special names - generic column
        eGDALUsage = GFU_Generic;
    }

    return eGDALUsage;
}

GDALRATFieldType KEARasterAttributeTable::GetTypeOfCol(int nCol) const
{
    if ((nCol < 0) || (nCol >= (int)m_aoFields.size()))
        return GFT_Integer;

    GDALRATFieldType eGDALType;
    switch (m_aoFields[nCol].dataType)
    {
        case kealib::kea_att_bool:
            eGDALType = GFT_Boolean;
            break;
        case kealib::kea_att_int:
            eGDALType = GFT_Integer;
            break;
        case kealib::kea_att_float:
            eGDALType = GFT_Real;
            break;
        case kealib::kea_att_string:
            eGDALType = GFT_String;
            break;
        default:
            eGDALType = GFT_Integer;
            break;
    }
    return eGDALType;
}

int KEARasterAttributeTable::GetColOfUsage(GDALRATFieldUsage eUsage) const
{
    unsigned int i;

    std::string keausage;
    switch (eUsage)
    {
        case GFU_PixelCount:
            keausage = "PixelCount";
            break;
        case GFU_Name:
            keausage = "Name";
            break;
        case GFU_Red:
            keausage = "Red";
            break;
        case GFU_Green:
            keausage = "Green";
            break;
        case GFU_Blue:
            keausage = "Blue";
            break;
        case GFU_Alpha:
            keausage = "Alpha";
            break;
        default:
            keausage = "Generic";
            break;
    }

    for (i = 0; i < m_aoFields.size(); i++)
    {
        if (m_aoFields[i].usage == keausage)
            return i;
    }
    return -1;
}

int KEARasterAttributeTable::GetRowCount() const
{
    return (int)m_poKEATable->getSize();
}

const char *KEARasterAttributeTable::GetValueAsString(int iRow,
                                                      int iField) const
{
    /// Let ValuesIO do the work.
    char *apszStrList[1];
    if ((const_cast<KEARasterAttributeTable *>(this))
            ->ValuesIO(GF_Read, iField, iRow, 1, apszStrList) != CE_None)
    {
        return "";
    }

    const_cast<KEARasterAttributeTable *>(this)->osWorkingResult =
        apszStrList[0];
    CPLFree(apszStrList[0]);

    return osWorkingResult;
}

int KEARasterAttributeTable::GetValueAsInt(int iRow, int iField) const
{
    // Let ValuesIO do the work.
    int nValue = 0;
    if ((const_cast<KEARasterAttributeTable *>(this))
            ->ValuesIO(GF_Read, iField, iRow, 1, &nValue) != CE_None)
    {
        return 0;
    }

    return nValue;
}

double KEARasterAttributeTable::GetValueAsDouble(int iRow, int iField) const
{
    // Let ValuesIO do the work.
    double dfValue = 0.0;
    if ((const_cast<KEARasterAttributeTable *>(this))
            ->ValuesIO(GF_Read, iField, iRow, 1, &dfValue) != CE_None)
    {
        return 0;
    }

    return dfValue;
}

bool KEARasterAttributeTable::GetValueAsBoolean(int iRow, int iField) const
{
    // Let ValuesIO do the work.
    bool bValue = false;
    if ((const_cast<KEARasterAttributeTable *>(this))
            ->ValuesIO(GF_Read, iField, iRow, 1, &bValue) != CE_None)
    {
        return false;
    }

    return bValue;
}

GDALRATDateTime KEARasterAttributeTable::GetValueAsDateTime(int iRow,
                                                            int iField) const
{
    // Let ValuesIO do the work.
    GDALRATDateTime value;
    const_cast<KEARasterAttributeTable *>(this)->ValuesIO(GF_Read, iField, iRow,
                                                          1, &value);
    return value;
}

const GByte *
KEARasterAttributeTable::GetValueAsWKBGeometry(int iRow, int iField,
                                               size_t &nWKBSize) const
{
    // Let ValuesIO do the work.
    GByte *pabyWKB = nullptr;
    const_cast<KEARasterAttributeTable *>(this)->ValuesIO(
        GF_Read, iField, iRow, 1, &pabyWKB, &nWKBSize);
    if (nWKBSize)
        m_abyCachedWKB.assign(pabyWKB, pabyWKB + nWKBSize);
    VSIFree(pabyWKB);
    return nWKBSize ? m_abyCachedWKB.data() : nullptr;
}

CPLErr KEARasterAttributeTable::SetValue(int iRow, int iField,
                                         const char *pszValue)
{
    // Let ValuesIO do the work.
    char *apszValues[1] = {const_cast<char *>(pszValue)};
    return ValuesIO(GF_Write, iField, iRow, 1, apszValues);
}

CPLErr KEARasterAttributeTable::SetValue(int iRow, int iField, double dfValue)
{
    // Let ValuesIO do the work.
    return ValuesIO(GF_Write, iField, iRow, 1, &dfValue);
}

CPLErr KEARasterAttributeTable::SetValue(int iRow, int iField, int nValue)
{
    // Let ValuesIO do the work.
    return ValuesIO(GF_Write, iField, iRow, 1, &nValue);
}

CPLErr KEARasterAttributeTable::SetValue(int iRow, int iField, bool bValue)
{
    // Let ValuesIO do the work.
    return ValuesIO(GF_Write, iField, iRow, 1, &bValue);
}

CPLErr KEARasterAttributeTable::SetValue(int iRow, int iField,
                                         const GDALRATDateTime &sDateTime)
{
    // Let ValuesIO do the work.
    return ValuesIO(GF_Write, iField, iRow, 1,
                    const_cast<GDALRATDateTime *>(&sDateTime));
}

CPLErr KEARasterAttributeTable::SetValue(int iRow, int iField,
                                         const void *pabyWKB, size_t nWKBSize)
{
    // Let ValuesIO do the work.
    const GByte **ppabyWKB = reinterpret_cast<const GByte **>(&pabyWKB);
    return ValuesIO(GF_Write, iField, iRow, 1, const_cast<GByte **>(ppabyWKB),
                    &nWKBSize);
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         double *pdfData)
{
    /*if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/
    CPLMutexHolderD(&m_hMutex);

    if (iField < 0 || iField >= (int)m_aoFields.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iStartRow < 0 || (iStartRow + iLength) > (int)m_poKEATable->getSize())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.", iStartRow,
                 iLength);

        return CE_Failure;
    }

    switch (m_aoFields[iField].dataType)
    {
        case kealib::kea_att_bool:
        case kealib::kea_att_int:
        {
            // allocate space for ints
            int *panColData = (int *)VSI_MALLOC2_VERBOSE(iLength, sizeof(int));
            if (panColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied doubles to ints
                for (int i = 0; i < iLength; i++)
                    panColData[i] = static_cast<int>(pdfData[i]);
            }

            // do the ValuesIO as ints
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData);
            if (eVal != CE_None)
            {
                CPLFree(panColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to doubles
                for (int i = 0; i < iLength; i++)
                    pdfData[i] = panColData[i];
            }

            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_float:
        {
            try
            {
                if (eRWFlag == GF_Read)
                    m_poKEATable->getFloatFields(
                        iStartRow, iLength, m_aoFields[iField].idx, pdfData);
                else
                    m_poKEATable->setFloatFields(
                        iStartRow, iLength, m_aoFields[iField].idx, pdfData);
            }
            catch (kealib::KEAException &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to read/write attribute table: %s", e.what());
                return CE_Failure;
            }
        }
        break;
        case kealib::kea_att_string:
        {
            // allocate space for string pointers
            char **papszColData =
                (char **)VSI_MALLOC2_VERBOSE(iLength, sizeof(char *));
            if (papszColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied doubles to strings
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.Printf("%.16g", pdfData[i]);
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // do the ValuesIO as strings
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData);
            if (eVal != CE_None)
            {
                if (eRWFlag == GF_Write)
                {
                    for (int i = 0; i < iLength; i++)
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to doubles
                for (int i = 0; i < iLength; i++)
                    pdfData[i] = CPLAtof(papszColData[i]);
            }

            // either we allocated them for write, or they were allocated
            // by ValuesIO on read
            for (int i = 0; i < iLength; i++)
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
        default:
            break;
    }
    return CE_None;
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         bool *pbData)
{
    /*if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/
    CPLMutexHolderD(&m_hMutex);

    if (iField < 0 || iField >= (int)m_aoFields.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iStartRow < 0 || (iStartRow + iLength) > (int)m_poKEATable->getSize())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.", iStartRow,
                 iLength);

        return CE_Failure;
    }

    switch (m_aoFields[iField].dataType)
    {
        case kealib::kea_att_bool:
        {
            try
            {
                if (eRWFlag == GF_Read)
                    m_poKEATable->getBoolFields(iStartRow, iLength,
                                                m_aoFields[iField].idx, pbData);
                else
                    m_poKEATable->setBoolFields(iStartRow, iLength,
                                                m_aoFields[iField].idx, pbData);
            }
            catch (kealib::KEAException &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to read/write attribute table: %s", e.what());
                return CE_Failure;
            }
        }
        break;
        case kealib::kea_att_int:
        {
            // need to convert to/from int64_t
            int64_t *panColData =
                (int64_t *)VSI_MALLOC2_VERBOSE(iLength, sizeof(int64_t));
            if (panColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied bools to int64t
                for (int i = 0; i < iLength; i++)
                    panColData[i] = pbData[i];
            }

            try
            {
                if (eRWFlag == GF_Read)
                    m_poKEATable->getIntFields(
                        iStartRow, iLength, m_aoFields[iField].idx, panColData);
                else
                    m_poKEATable->setIntFields(
                        iStartRow, iLength, m_aoFields[iField].idx, panColData);
            }
            catch (kealib::KEAException &e)
            {
                // fprintf(stderr,"Failed to read/write attribute table: %s %d
                // %d %ld\n", e.what(), iStartRow, iLength,
                // m_poKEATable->getSize() );
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to read/write attribute table: %s", e.what());
                return CE_Failure;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to bools
                for (int i = 0; i < iLength; i++)
                    pbData[i] = panColData[i] != 0;
            }
            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_float:
        {
            // allocate space for doubles
            double *padfColData =
                (double *)VSI_MALLOC2_VERBOSE(iLength, sizeof(double));
            if (padfColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied ints to doubles
                for (int i = 0; i < iLength; i++)
                    padfColData[i] = pbData[i];
            }

            // do the ValuesIO as doubles
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData);
            if (eVal != CE_None)
            {
                CPLFree(padfColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to ints
                for (int i = 0; i < iLength; i++)
                    pbData[i] = padfColData[i] != 0;
            }

            CPLFree(padfColData);
        }
        break;
        case kealib::kea_att_string:
        {
            // allocate space for string pointers
            char **papszColData =
                (char **)VSI_MALLOC2_VERBOSE(iLength, sizeof(char *));
            if (papszColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied ints to strings
                for (int i = 0; i < iLength; i++)
                {
                    papszColData[i] = CPLStrdup(pbData[i] ? "true" : "false");
                }
            }

            // do the ValuesIO as strings
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData);
            if (eVal != CE_None)
            {
                if (eRWFlag == GF_Write)
                {
                    for (int i = 0; i < iLength; i++)
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to ints
                for (int i = 0; i < iLength; i++)
                    pbData[i] = CPLTestBool(papszColData[i]);
            }

            // either we allocated them for write, or they were allocated
            // by ValuesIO on read
            for (int i = 0; i < iLength; i++)
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
        default:
            break;
    }
    return CE_None;
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         int *pnData)
{
    /*if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/
    CPLMutexHolderD(&m_hMutex);

    if (iField < 0 || iField >= (int)m_aoFields.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iStartRow < 0 || (iStartRow + iLength) > (int)m_poKEATable->getSize())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.", iStartRow,
                 iLength);

        return CE_Failure;
    }

    switch (m_aoFields[iField].dataType)
    {
        case kealib::kea_att_bool:
        {
            // need to convert to/from bools
            bool *panColData =
                (bool *)VSI_MALLOC2_VERBOSE(iLength, sizeof(bool));
            if (panColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied ints to bools
                for (int i = 0; i < iLength; i++)
                {
                    panColData[i] = (pnData[i] != 0);
                }
            }

            try
            {
                if (eRWFlag == GF_Read)
                    m_poKEATable->getBoolFields(
                        iStartRow, iLength, m_aoFields[iField].idx, panColData);
                else
                    m_poKEATable->setBoolFields(
                        iStartRow, iLength, m_aoFields[iField].idx, panColData);
            }
            catch (kealib::KEAException &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to read/write attribute table: %s", e.what());
                return CE_Failure;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to ints
                for (int i = 0; i < iLength; i++)
                    pnData[i] = panColData[i] ? 1 : 0;
            }
            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_int:
        {
            // need to convert to/from int64_t
            int64_t *panColData =
                (int64_t *)VSI_MALLOC2_VERBOSE(iLength, sizeof(int64_t));
            if (panColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied ints to int64t
                for (int i = 0; i < iLength; i++)
                    panColData[i] = pnData[i];
            }

            try
            {
                if (eRWFlag == GF_Read)
                    m_poKEATable->getIntFields(
                        iStartRow, iLength, m_aoFields[iField].idx, panColData);
                else
                    m_poKEATable->setIntFields(
                        iStartRow, iLength, m_aoFields[iField].idx, panColData);
            }
            catch (kealib::KEAException &e)
            {
                // fprintf(stderr,"Failed to read/write attribute table: %s %d
                // %d %ld\n", e.what(), iStartRow, iLength,
                // m_poKEATable->getSize() );
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to read/write attribute table: %s", e.what());
                return CE_Failure;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to ints
                for (int i = 0; i < iLength; i++)
                    pnData[i] = static_cast<int>(panColData[i]);
            }
            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_float:
        {
            // allocate space for doubles
            double *padfColData =
                (double *)VSI_MALLOC2_VERBOSE(iLength, sizeof(double));
            if (padfColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied ints to doubles
                for (int i = 0; i < iLength; i++)
                    padfColData[i] = pnData[i];
            }

            // do the ValuesIO as doubles
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData);
            if (eVal != CE_None)
            {
                CPLFree(padfColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to ints
                for (int i = 0; i < iLength; i++)
                    pnData[i] = static_cast<int>(padfColData[i]);
            }

            CPLFree(padfColData);
        }
        break;
        case kealib::kea_att_string:
        {
            // allocate space for string pointers
            char **papszColData =
                (char **)VSI_MALLOC2_VERBOSE(iLength, sizeof(char *));
            if (papszColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // copy the application supplied ints to strings
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.Printf("%d", pnData[i]);
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // do the ValuesIO as strings
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData);
            if (eVal != CE_None)
            {
                if (eRWFlag == GF_Write)
                {
                    for (int i = 0; i < iLength; i++)
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // copy them back to ints
                for (int i = 0; i < iLength; i++)
                    pnData[i] = atoi(papszColData[i]);
            }

            // either we allocated them for write, or they were allocated
            // by ValuesIO on read
            for (int i = 0; i < iLength; i++)
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
        default:
            break;
    }
    return CE_None;
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         char **papszStrList)
{
    /*if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/
    CPLMutexHolderD(&m_hMutex);

    if (iField < 0 || iField >= (int)m_aoFields.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iStartRow < 0 || (iStartRow + iLength) > (int)m_poKEATable->getSize())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.", iStartRow,
                 iLength);

        return CE_Failure;
    }

    switch (m_aoFields[iField].dataType)
    {
        case kealib::kea_att_bool:
        {
            // allocate space for bools
            bool *pabColData =
                (bool *)VSI_MALLOC2_VERBOSE(iLength, sizeof(bool));
            if (pabColData == nullptr)
            {
                return CE_Failure;
            }
            if (eRWFlag == GF_Write)
            {
                // convert user supplied strings to ints
                for (int i = 0; i < iLength; i++)
                    pabColData[i] = CPLTestBool(papszStrList[i]);
            }

            // call values IO to read/write ints
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, pabColData);
            if (eVal != CE_None)
            {
                CPLFree(pabColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // convert ints back to strings
                for (int i = 0; i < iLength; i++)
                {
                    papszStrList[i] =
                        CPLStrdup(pabColData[i] ? "true" : "false");
                }
            }
            CPLFree(pabColData);
        }
        break;
        case kealib::kea_att_int:
        {
            // allocate space for ints
            int *panColData = (int *)VSI_MALLOC2_VERBOSE(iLength, sizeof(int));
            if (panColData == nullptr)
            {
                return CE_Failure;
            }
            if (eRWFlag == GF_Write)
            {
                // convert user supplied strings to ints
                for (int i = 0; i < iLength; i++)
                    panColData[i] = atoi(papszStrList[i]);
            }

            // call values IO to read/write ints
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData);
            if (eVal != CE_None)
            {
                CPLFree(panColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // convert ints back to strings
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.Printf("%d", panColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_float:
        {
            // allocate space for doubles
            double *padfColData =
                (double *)VSI_MALLOC2_VERBOSE(iLength, sizeof(double));
            if (padfColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // convert user supplied strings to doubles
                for (int i = 0; i < iLength; i++)
                    padfColData[i] = CPLAtof(papszStrList[i]);
            }

            // call value IO to read/write doubles
            CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData);
            if (eVal != CE_None)
            {
                CPLFree(padfColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // convert doubles back to strings
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.Printf("%.16g", padfColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(padfColData);
        }
        break;
        case kealib::kea_att_string:
        {
            try
            {
                if (eRWFlag == GF_Read)
                {
                    std::vector<std::string> aStrings;
                    m_poKEATable->getStringFields(
                        iStartRow, iLength, m_aoFields[iField].idx, &aStrings);
                    for (std::vector<std::string>::size_type i = 0;
                         i < aStrings.size(); i++)
                    {
                        // Copy using CPLStrdup so user can call CPLFree
                        papszStrList[i] = CPLStrdup(aStrings[i].c_str());
                    }
                }
                else
                {
                    // need to convert to a vector first
                    std::vector<std::string> aStrings;
                    for (int i = 0; i < iLength; i++)
                    {
                        aStrings.push_back(papszStrList[i]);
                    }
                    m_poKEATable->setStringFields(
                        iStartRow, iLength, m_aoFields[iField].idx, &aStrings);
                }
            }
            catch (kealib::KEAException &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to read/write attribute table: %s", e.what());
                return CE_Failure;
            }
        }
        break;
        default:
            break;
    }
    return CE_None;
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         GDALRATDateTime *psDateTime)
{
    return ValuesIODateTimeFromIntoString(eRWFlag, iField, iStartRow, iLength,
                                          psDateTime);
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         GByte **ppabyWKB, size_t *pnWKBSize)
{
    return ValuesIOWKBGeometryFromIntoString(eRWFlag, iField, iStartRow,
                                             iLength, ppabyWKB, pnWKBSize);
}

int KEARasterAttributeTable::ChangesAreWrittenToFile()
{
    return TRUE;
}

void KEARasterAttributeTable::SetRowCount(int iCount)
{
    /*if( this->eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return;
    }*/

    if (iCount > (int)m_poKEATable->getSize())
    {
        m_poKEATable->addRows(iCount - m_poKEATable->getSize());
    }
    // can't shrink
}

CPLErr KEARasterAttributeTable::CreateColumn(const char *pszFieldName,
                                             GDALRATFieldType eFieldType,
                                             GDALRATFieldUsage eFieldUsage)
{
    /*if( this->eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/
    CPLMutexHolderD(&m_hMutex);

    const char *strUsage = "Generic";
    switch (eFieldUsage)
    {
        case GFU_PixelCount:
            strUsage = "PixelCount";
            eFieldType = GFT_Real;
            break;
        case GFU_Name:
            strUsage = "Name";
            eFieldType = GFT_String;
            break;
        case GFU_Red:
            strUsage = "Red";
            eFieldType = GFT_Integer;
            break;
        case GFU_Green:
            strUsage = "Green";
            eFieldType = GFT_Integer;
            break;
        case GFU_Blue:
            strUsage = "Blue";
            eFieldType = GFT_Integer;
            break;
        case GFU_Alpha:
            strUsage = "Alpha";
            eFieldType = GFT_Integer;
            break;
        default:
            // leave as "Generic"
            break;
    }

    try
    {
        switch (eFieldType)
        {
            case GFT_Integer:
                m_poKEATable->addAttIntField(pszFieldName, 0, strUsage);
                break;

            case GFT_Boolean:
                m_poKEATable->addAttBoolField(pszFieldName, 0, strUsage);
                break;

            case GFT_Real:
                m_poKEATable->addAttFloatField(pszFieldName, 0, strUsage);
                break;

            case GFT_String:
            case GFT_DateTime:
            case GFT_WKBGeometry:
                m_poKEATable->addAttStringField(pszFieldName, "", strUsage);
                break;
        }

        // assume we can just grab this now
        m_aoFields.push_back(m_poKEATable->getField(pszFieldName));
    }
    catch (kealib::KEAException &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to add column: %s",
                 e.what());
        return CE_Failure;
    }

    return CE_None;
}

CPLErr KEARasterAttributeTable::SetLinearBinning(double ldfRow0Min,
                                                 double ldfBinSize)
{
    size_t nRows = m_poKEATable->getSize();

    osWorkingResult.Printf("%.16g", ldfRow0Min);
    m_poBand->SetMetadataItem("STATISTICS_HISTOMIN", osWorkingResult);
    osWorkingResult.Printf("%.16g", (nRows - 1) * ldfBinSize + ldfRow0Min);
    m_poBand->SetMetadataItem("STATISTICS_HISTOMAX", osWorkingResult);

    // STATISTICS_HISTONUMBINS now returned by metadata

    return CE_None;
}

int KEARasterAttributeTable::GetLinearBinning(double *pdfRow0Min,
                                              double *pdfBinSize) const
{
    const char *pszMin = m_poBand->GetMetadataItem("STATISTICS_HISTOMIN");
    const char *pszMax = m_poBand->GetMetadataItem("STATISTICS_HISTOMAX");
    if ((pszMin == nullptr) || (pszMax == nullptr))
    {
        return FALSE;
    }
    *pdfRow0Min = atof(pszMin);
    *pdfBinSize = (atof(pszMax) - *pdfRow0Min) / (m_poKEATable->getSize() - 1);

    return TRUE;
}

CPLXMLNode *KEARasterAttributeTable::Serialize() const
{
    const int nColCount = GetColumnCount();
    if (nColCount > 0 && GetRowCount() > RAT_MAX_ELEM_FOR_CLONE / nColCount)
        return nullptr;

    return GDALRasterAttributeTable::Serialize();
}

GDALRATTableType KEARasterAttributeTable::GetTableType() const
{
    kealib::KEALayerType keaType = m_poBand->getLayerType();
    if (keaType == kealib::kea_continuous)
    {
        return GRTT_ATHEMATIC;
    }
    else
    {
        return GRTT_THEMATIC;
    }
}

CPLErr
KEARasterAttributeTable::SetTableType(const GDALRATTableType eInTableType)
{
    kealib::KEALayerType keaType = (eInTableType == GRTT_ATHEMATIC)
                                       ? kealib::kea_continuous
                                       : kealib::kea_thematic;
    try
    {
        m_poBand->setLayerType(keaType);
        return CE_None;
    }
    catch (const kealib::KEAIOException &)
    {
        return CE_Failure;
    }
}

void KEARasterAttributeTable::RemoveStatistics()
{
    // TODO ?
}
