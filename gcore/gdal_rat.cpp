/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALRasterAttributeTable and related classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_rat.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif
#include "json.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include "ogrlibjsonutils.h"

// NOTE: keep the below description in sync with doc/source/user/raster_data_model.rst::raster_data_model_rat

/**
 * \class GDALRasterAttributeTable
 *
 * The GDALRasterAttributeTable (or RAT) class is used to encapsulate a table
 * used to provide attribute information about pixel values.  Each row
 * in the table applies to a range of pixel values (or a single value in
 * some cases), and might have attributes such as the histogram count for
 * that range, the color pixels of that range should be drawn names of classes
 * or any other generic information.
 *
 * Raster attribute tables can be used to represent histograms, color tables,
 * and classification information.
 *
 * Each column in a raster attribute table has a name, a type (integer,
 * floating point, string, boolean, date time, geometries encoded as WKB),
 * and a GDALRATFieldUsage.
 * The usage distinguishes columns with particular understood purposes
 * (such as color, histogram count, name) and columns that have specific
 * purposes not understood by the library (long label,
 * suitability_for_growing_wheat, etc).
 *
 * In the general case each row has a column indicating the minimum pixel
 * values falling into that category, and a column indicating the maximum
 * pixel value.  These are indicated with usage values of GFU_Min, and
 * GFU_Max.  In other cases where each row is a discrete pixel value, one
 * column of usage GFU_MinMax can be used.
 *
 * In other cases all the categories are of equal size and regularly spaced
 * and the categorization information can be determined just by knowing the
 * value at which the categories start, and the size of a category.  This
 * is called "Linear Binning" and the information is kept specially on
 * the raster attribute table as a whole.
 *
 * RATs are normally associated with GDALRasterBands and can be queried
 * using the GDALRasterBand::GetDefaultRAT() method.
 */

/************************************************************************/
/*                      GDALGetRATFieldTypeName()                       */
/************************************************************************/

/** Return the string representation of a GDALRATFieldType.
 *
 * @since 3.12
 */
const char *GDALGetRATFieldTypeName(GDALRATFieldType eType)
{
#define CASE_GFT(x)                                                            \
    case GFT_##x:                                                              \
        return #x

    switch (eType)
    {
        CASE_GFT(Integer);
        CASE_GFT(String);
        CASE_GFT(Real);
        CASE_GFT(Boolean);
        CASE_GFT(DateTime);
        case GFT_WKBGeometry:
            break;
    }
    return "WKBGeometry";

#undef CASE_GFT
}

/************************************************************************/
/*                      GDALGetRATFieldUsageName()                      */
/************************************************************************/

/** Return the string representation of a GDALRATFieldUsage.
 *
 * @since 3.12
 */
const char *GDALGetRATFieldUsageName(GDALRATFieldUsage eUsage)
{
#define CASE_GFU(x)                                                            \
    case GFU_##x:                                                              \
        return #x

    switch (eUsage)
    {
        CASE_GFU(Generic);
        CASE_GFU(PixelCount);
        CASE_GFU(Name);
        CASE_GFU(Min);
        CASE_GFU(Max);
        CASE_GFU(MinMax);
        CASE_GFU(Red);
        CASE_GFU(Green);
        CASE_GFU(Blue);
        CASE_GFU(Alpha);
        CASE_GFU(RedMin);
        CASE_GFU(GreenMin);
        CASE_GFU(BlueMin);
        CASE_GFU(AlphaMin);
        CASE_GFU(RedMax);
        CASE_GFU(GreenMax);
        CASE_GFU(BlueMax);
        CASE_GFU(AlphaMax);
        case GFU_MaxCount:
            break;
    }
    return "MaxCount";

#undef CASE_GFU
}

/************************************************************************/
/*                  ~GDALRasterAttributeTable()                         */
/*                                                                      */
/*                      Virtual Destructor                              */
/************************************************************************/

GDALRasterAttributeTable::~GDALRasterAttributeTable() = default;

/************************************************************************/
/*                              ValuesIO()                              */
/*                                                                      */
/*                      Default Implementations                         */
/************************************************************************/

/**
 * \brief Read or Write a block of doubles to/from the Attribute Table.
 *
 * This method is the same as the C function GDALRATValuesIOAsDouble().
 *
 * @param eRWFlag either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param pdfData pointer to array of doubles to read/write. Should be at least
 *   iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of
 *   rows in table.
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          double *pdfData)
{
    if ((iStartRow + iLength) > GetRowCount())
    {
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eRWFlag == GF_Read)
    {
        for (int iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++)
        {
            pdfData[iIndex - iStartRow] = GetValueAsDouble(iIndex, iField);
        }
    }
    else
    {
        for (int iIndex = iStartRow;
             eErr == CE_None && iIndex < (iStartRow + iLength); iIndex++)
        {
            eErr = SetValue(iIndex, iField, pdfData[iIndex - iStartRow]);
        }
    }
    return eErr;
}

/************************************************************************/
/*                      GDALRATValuesIOAsDouble()                       */
/************************************************************************/

/**
 * \brief Read or Write a block of doubles to/from the Attribute Table.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::ValuesIO()
 */
CPLErr CPL_STDCALL GDALRATValuesIOAsDouble(GDALRasterAttributeTableH hRAT,
                                           GDALRWFlag eRWFlag, int iField,
                                           int iStartRow, int iLength,
                                           double *pdfData)

{
    VALIDATE_POINTER1(hRAT, "GDALRATValuesIOAsDouble", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->ValuesIO(
        eRWFlag, iField, iStartRow, iLength, pdfData);
}

/**
 * \brief Read or Write a block of integers to/from the Attribute Table.
 *
 * This method is the same as the C function GDALRATValuesIOAsInteger().
 *
 * @param eRWFlag either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param pnData pointer to array of ints to read/write. Should be at least
 *     iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of
 *     rows in table.
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          int *pnData)
{
    if ((iStartRow + iLength) > GetRowCount())
    {
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eRWFlag == GF_Read)
    {
        for (int iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++)
        {
            pnData[iIndex - iStartRow] = GetValueAsInt(iIndex, iField);
        }
    }
    else
    {
        for (int iIndex = iStartRow;
             eErr == CE_None && iIndex < (iStartRow + iLength); iIndex++)
        {
            eErr = SetValue(iIndex, iField, pnData[iIndex - iStartRow]);
        }
    }
    return eErr;
}

/************************************************************************/
/*                      GDALRATValuesIOAsInteger()                      */
/************************************************************************/

/**
 * \brief Read or Write a block of ints to/from the Attribute Table.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::ValuesIO()
 */
CPLErr CPL_STDCALL GDALRATValuesIOAsInteger(GDALRasterAttributeTableH hRAT,
                                            GDALRWFlag eRWFlag, int iField,
                                            int iStartRow, int iLength,
                                            int *pnData)

{
    VALIDATE_POINTER1(hRAT, "GDALRATValuesIOAsInteger", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->ValuesIO(
        eRWFlag, iField, iStartRow, iLength, pnData);
}

/**
 * \brief Read or Write a block of strings to/from the Attribute Table.
 *
 * This method is the same as the C function GDALRATValuesIOAsString().
 * When reading, papszStrList must be already allocated to the correct size.
 * The caller is expected to call CPLFree on each read string.
 *
 * @param eRWFlag either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param papszStrList pointer to array of strings to read/write. Should be at
 *   least iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of
 *   rows in table.
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          char **papszStrList)
{
    if ((iStartRow + iLength) > GetRowCount())
    {
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eRWFlag == GF_Read)
    {
        for (int iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++)
        {
            papszStrList[iIndex - iStartRow] =
                VSIStrdup(GetValueAsString(iIndex, iField));
        }
    }
    else
    {
        for (int iIndex = iStartRow;
             eErr == CE_None && iIndex < (iStartRow + iLength); iIndex++)
        {
            eErr = SetValue(iIndex, iField, papszStrList[iIndex - iStartRow]);
        }
    }
    return eErr;
}

/************************************************************************/
/*                      GDALRATValuesIOAsString()                       */
/************************************************************************/

/**
 * \brief Read or Write a block of strings to/from the Attribute Table.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::ValuesIO()
 */
CPLErr CPL_STDCALL GDALRATValuesIOAsString(GDALRasterAttributeTableH hRAT,
                                           GDALRWFlag eRWFlag, int iField,
                                           int iStartRow, int iLength,
                                           char **papszStrList)

{
    VALIDATE_POINTER1(hRAT, "GDALRATValuesIOAsString", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->ValuesIO(
        eRWFlag, iField, iStartRow, iLength, papszStrList);
}

/************************************************************************/
/*                              ValuesIO()                              */
/************************************************************************/

/**
 * \brief Read or Write a block of booleans to/from the Attribute Table.
 *
 * This method is the same as the C function GDALRATValuesIOAsBoolean().
 *
 * @param eRWFlag either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param pbData pointer to array of booleans to read/write. Should be at least
 *     iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of
 *     rows in table.
 * @since 3.12
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          bool *pbData)
{
    if ((iStartRow + iLength) > GetRowCount())
    {
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eRWFlag == GF_Read)
    {
        for (int iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++)
        {
            pbData[iIndex - iStartRow] = GetValueAsBoolean(iIndex, iField);
        }
    }
    else
    {
        for (int iIndex = iStartRow;
             eErr == CE_None && iIndex < (iStartRow + iLength); iIndex++)
        {
            eErr = SetValue(iIndex, iField, pbData[iIndex - iStartRow]);
        }
    }
    return eErr;
}

/************************************************************************/
/*                      GDALRATValuesIOAsBoolean()                      */
/************************************************************************/

/**
 * \brief Read or Write a block of booleans to/from the Attribute Table.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::ValuesIO()
 *
 * @since 3.12
 */
CPLErr GDALRATValuesIOAsBoolean(GDALRasterAttributeTableH hRAT,
                                GDALRWFlag eRWFlag, int iField, int iStartRow,
                                int iLength, bool *pbData)

{
    VALIDATE_POINTER1(hRAT, "GDALRATValuesIOAsBoolean", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->ValuesIO(
        eRWFlag, iField, iStartRow, iLength, pbData);
}

/************************************************************************/
/*                              ValuesIO()                              */
/************************************************************************/

/**
 * \brief Read or Write a block of DateTime to/from the Attribute Table.
 *
 * This method is the same as the C function GDALRATValuesIOAsDateTime().
 *
 * @param eRWFlag either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param psDateTime pointer to array of DateTime to read/write. Should be at
 *                   least iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of
 *     rows in table.
 * @since 3.12
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          GDALRATDateTime *psDateTime)
{
    if ((iStartRow + iLength) > GetRowCount())
    {
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eRWFlag == GF_Read)
    {
        for (int iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++)
        {
            psDateTime[iIndex - iStartRow] = GetValueAsDateTime(iIndex, iField);
        }
    }
    else
    {
        for (int iIndex = iStartRow;
             eErr == CE_None && iIndex < (iStartRow + iLength); iIndex++)
        {
            eErr = SetValue(iIndex, iField, psDateTime[iIndex - iStartRow]);
        }
    }
    return eErr;
}

/************************************************************************/
/*                     GDALRATValuesIOAsDateTime()                      */
/************************************************************************/

/**
 * \brief Read or Write a block of date-times to/from the Attribute Table.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::ValuesIO()
 *
 * @since 3.12
 */
CPLErr GDALRATValuesIOAsDateTime(GDALRasterAttributeTableH hRAT,
                                 GDALRWFlag eRWFlag, int iField, int iStartRow,
                                 int iLength, GDALRATDateTime *psDateTime)

{
    VALIDATE_POINTER1(hRAT, "GDALRATValuesIOAsDateTime", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->ValuesIO(
        eRWFlag, iField, iStartRow, iLength, psDateTime);
}

/************************************************************************/
/*                              ValuesIO()                              */
/************************************************************************/

/**
 * \brief Read or Write a block of WKB-encoded geometries to/from the Attribute Table.
 *
 * When reading, each ppabyWKB[] should be CPLFree'd() after use.
 *
 * This method is the same as the C function GDALRATValuesIOAsWKBGeometry().
 *
 * @param eRWFlag either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param ppabyWKB pointer to array of pointer of WKB-encoded geometries to
 *                 read/write. Should be at least iLength long.
 * @param pnWKBSize pointer to array of WKB size.
 *                  Should be at least iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of
 *     rows in table.
 * @since 3.12
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          GByte **ppabyWKB, size_t *pnWKBSize)
{
    if ((iStartRow + iLength) > GetRowCount())
    {
        return CE_Failure;
    }

    CPLErr eErr = CE_None;
    if (eRWFlag == GF_Read)
    {
        for (int iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++)
        {
            size_t nSize = 0;
            const GByte *pabyWKB = GetValueAsWKBGeometry(iIndex, iField, nSize);
            pnWKBSize[iIndex - iStartRow] = nSize;
            if (nSize)
            {
                ppabyWKB[iIndex - iStartRow] =
                    static_cast<GByte *>(CPLMalloc(nSize));
                memcpy(ppabyWKB[iIndex - iStartRow], pabyWKB, nSize);
            }
            else
            {
                ppabyWKB[iIndex - iStartRow] = nullptr;
            }
        }
    }
    else
    {
        for (int iIndex = iStartRow;
             eErr == CE_None && iIndex < (iStartRow + iLength); iIndex++)
        {
            eErr = SetValue(iIndex, iField, ppabyWKB[iIndex - iStartRow],
                            pnWKBSize[iIndex - iStartRow]);
        }
    }
    return eErr;
}

/************************************************************************/
/*                    GDALRATValuesIOAsWKBGeometry()                    */
/************************************************************************/

/**
 * \brief Read or Write a block of WKB-encoded geometries to/from the Attribute Table.
 *
 * When reading, each ppabyWKB[] should be CPLFree'd() after use.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::ValuesIO()
 *
 * @since 3.12
 */
CPLErr GDALRATValuesIOAsWKBGeometry(GDALRasterAttributeTableH hRAT,
                                    GDALRWFlag eRWFlag, int iField,
                                    int iStartRow, int iLength,
                                    GByte **ppabyWKB, size_t *pnWKBSize)

{
    VALIDATE_POINTER1(hRAT, "GDALRATValuesIOAsWKBGeometry", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->ValuesIO(
        eRWFlag, iField, iStartRow, iLength, ppabyWKB, pnWKBSize);
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     ValuesIOBooleanFromIntoInt()                     */
/************************************************************************/

CPLErr GDALRasterAttributeTable::ValuesIOBooleanFromIntoInt(
    GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, bool *pbData)
{
    if (eRWFlag == GF_Read)
    {
        std::vector<int> anData(iLength);
        CPLErr eErr =
            ValuesIO(eRWFlag, iField, iStartRow, iLength, anData.data());
        if (eErr == CE_None)
        {
            for (int i = 0; i < iLength; ++i)
            {
                pbData[i] = anData[i] != 0;
            }
        }
        return eErr;
    }
    else
    {
        std::vector<int> anData;
        anData.reserve(iLength);
        for (int i = 0; i < iLength; ++i)
            anData.push_back(pbData[i]);
        return ValuesIO(eRWFlag, iField, iStartRow, iLength, anData.data());
    }
}

/************************************************************************/
/*                          DateTimeToString()                          */
/************************************************************************/

/* static */
std::string
GDALRasterAttributeTable::DateTimeToString(const GDALRATDateTime &sDateTime)
{
    if (!sDateTime.bIsValid)
        return std::string();
    return CPLString().Printf(
        "%04d-%02d-%02dT%02d:%02d:%06.3f%c%02d:%02d", sDateTime.nYear,
        sDateTime.nMonth, sDateTime.nDay, sDateTime.nHour, sDateTime.nMinute,
        static_cast<double>(sDateTime.fSecond),
        sDateTime.bPositiveTimeZone ? '+' : '-', sDateTime.nTimeZoneHour,
        sDateTime.nTimeZoneMinute);
}

/************************************************************************/
/*                          StringToDateTime()                          */
/************************************************************************/

/* static */
bool GDALRasterAttributeTable::StringToDateTime(const char *pszStr,
                                                GDALRATDateTime &sDateTime)
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
            sField.Date.TZFlag <= 2 ? false : sField.Date.TZFlag >= 100;
        sDateTime.nTimeZoneHour = sField.Date.TZFlag <= 2
                                      ? 0
                                      : std::abs(sField.Date.TZFlag - 100) / 4;
        sDateTime.nTimeZoneMinute =
            sField.Date.TZFlag <= 2
                ? 0
                : (std::abs(sField.Date.TZFlag - 100) % 4) * 15;
        sDateTime.bIsValid = true;
        return true;
    }
    else
    {
        sDateTime = GDALRATDateTime();
        return false;
    }
}

/************************************************************************/
/*                   ValuesIODateTimeFromIntoString()                   */
/************************************************************************/

CPLErr GDALRasterAttributeTable::ValuesIODateTimeFromIntoString(
    GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
    GDALRATDateTime *psDateTime)
{
    if (eRWFlag == GF_Read)
    {
        std::vector<char *> apszStrList(iLength);
        CPLErr eErr =
            ValuesIO(eRWFlag, iField, iStartRow, iLength, apszStrList.data());
        if (eErr == CE_None)
        {
            for (int i = 0; i < iLength; ++i)
            {
                StringToDateTime(apszStrList[i], psDateTime[i]);
            }
        }
        for (int i = 0; i < iLength; ++i)
            VSIFree(apszStrList[i]);
        return eErr;
    }
    else
    {
        std::vector<std::string> asStr;
        std::vector<char *> apszStr;
        asStr.reserve(iLength);
        apszStr.reserve(iLength);
        for (int i = 0; i < iLength; ++i)
        {
            asStr.push_back(DateTimeToString(psDateTime[i]));
            apszStr.push_back(asStr.back().data());
        }
        return ValuesIO(eRWFlag, iField, iStartRow, iLength, apszStr.data());
    }
}

/************************************************************************/
/*                          WKBGeometryToWKT()                          */
/************************************************************************/

/* static */
std::string GDALRasterAttributeTable::WKBGeometryToWKT(const void *pabyWKB,
                                                       size_t nWKBSize)
{
    std::string osWKT;
    if (nWKBSize)
    {
        OGRGeometry *poGeometry = nullptr;
        if (OGRGeometryFactory::createFromWkb(pabyWKB, nullptr, &poGeometry,
                                              nWKBSize,
                                              wkbVariantIso) == OGRERR_NONE)
        {
            osWKT = poGeometry->exportToWkt();
        }
        delete poGeometry;
    }
    return osWKT;
}

/************************************************************************/
/*                          WKTGeometryToWKB()                          */
/************************************************************************/

/* static */
std::vector<GByte>
GDALRasterAttributeTable::WKTGeometryToWKB(const char *pszWKT)
{
    std::vector<GByte> abyWKB;
    OGRGeometry *poGeom = nullptr;
    if (pszWKT[0] && OGRGeometryFactory::createFromWkt(pszWKT, nullptr,
                                                       &poGeom) == OGRERR_NONE)
    {
        const size_t nWKBSize = poGeom->WkbSize();
        abyWKB.resize(nWKBSize);
        poGeom->exportToWkb(wkbNDR, abyWKB.data(), wkbVariantIso);
    }
    delete poGeom;
    return abyWKB;
}

/************************************************************************/
/*                 ValuesIOWKBGeometryFromIntoString()                  */
/************************************************************************/

CPLErr GDALRasterAttributeTable::ValuesIOWKBGeometryFromIntoString(
    GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
    GByte **ppabyWKB, size_t *pnWKBSize)
{
    if (eRWFlag == GF_Read)
    {
        std::vector<char *> apszStrList(iLength);
        CPLErr eErr =
            ValuesIO(eRWFlag, iField, iStartRow, iLength, apszStrList.data());
        if (eErr == CE_None)
        {
            for (int i = 0; i < iLength; ++i)
            {
                auto abyWKB = WKTGeometryToWKB(apszStrList[i]);
                if (abyWKB.empty())
                {
                    ppabyWKB[i] = nullptr;
                    pnWKBSize[i] = 0;
                }
                else
                {
                    ppabyWKB[i] =
                        static_cast<GByte *>(CPLMalloc(abyWKB.size()));
                    memcpy(ppabyWKB[i], abyWKB.data(), abyWKB.size());
                    pnWKBSize[i] = abyWKB.size();
                }
            }
        }
        for (int i = 0; i < iLength; ++i)
            VSIFree(apszStrList[i]);
        return eErr;
    }
    else
    {
        std::vector<std::string> asStr;
        std::vector<char *> apszStr;
        asStr.reserve(iLength);
        apszStr.reserve(iLength);
        for (int i = 0; i < iLength; ++i)
        {
            asStr.push_back(WKBGeometryToWKT(ppabyWKB[i], pnWKBSize[i]));
            apszStr.push_back(asStr.back().data());
        }
        return ValuesIO(eRWFlag, iField, iStartRow, iLength, apszStr.data());
    }
}

//! @endcond

/************************************************************************/
/*                            SetRowCount()                             */
/************************************************************************/

/**
 * \brief Set row count.
 *
 * Resizes the table to include the indicated number of rows.  Newly created
 * rows will be initialized to their default values - "" for strings,
 * and zero for numeric fields.
 *
 * This method is the same as the C function GDALRATSetRowCount().
 *
 * @param nNewCount the new number of rows.
 */

void GDALRasterAttributeTable::SetRowCount(CPL_UNUSED int nNewCount)
{
}

/************************************************************************/
/*                         GDALRATSetRowCount()                         */
/************************************************************************/

/**
 * \brief Set row count.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetRowCount()
 *
 * @param hRAT RAT handle.
 * @param nNewCount the new number of rows.
 */
void CPL_STDCALL GDALRATSetRowCount(GDALRasterAttributeTableH hRAT,
                                    int nNewCount)

{
    VALIDATE_POINTER0(hRAT, "GDALRATSetRowCount");

    GDALRasterAttributeTable::FromHandle(hRAT)->SetRowCount(nNewCount);
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/************************************************************************/

/**
 * \fn GDALRasterAttributeTable::GetRowOfValue(double) const
 * \brief Get row for pixel value.
 *
 * Given a raw pixel value, the raster attribute table is scanned to
 * determine which row in the table applies to the pixel value.  The
 * row index is returned.
 *
 * This method is the same as the C function GDALRATGetRowOfValue().
 *
 * @param dfValue the pixel value.
 *
 * @return the row index or -1 if no row is appropriate.
 */

/**/
/**/

int GDALRasterAttributeTable::GetRowOfValue(double /* dfValue */) const
{
    return -1;
}

/************************************************************************/
/*                        GDALRATGetRowOfValue()                        */
/************************************************************************/

/**
 * \brief Get row for pixel value.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetRowOfValue()
 */
int CPL_STDCALL GDALRATGetRowOfValue(GDALRasterAttributeTableH hRAT,
                                     double dfValue)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetRowOfValue", 0);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetRowOfValue(dfValue);
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/************************************************************************/

/**
 * \brief Get row for pixel value.
 *
 * Given a raw pixel value, the raster attribute table is scanned to
 * determine which row in the table applies to the pixel value.  The
 * row index is returned.
 *
 * Int arg for now just converted to double.  Perhaps we will
 * handle this in a special way some day?
 *
 * This method is the same as the C function GDALRATGetRowOfValue().
 *
 * @param nValue the pixel value.
 *
 * @return the row index or -1 if no row is appropriate.
 */

int GDALRasterAttributeTable::GetRowOfValue(int nValue) const

{
    return GetRowOfValue(static_cast<double>(nValue));
}

/************************************************************************/
/*                            CreateColumn()                            */
/************************************************************************/

/**
 * \fn GDALRasterAttributeTable::CreateColumn(const char*, GDALRATFieldType,
 * GDALRATFieldUsage) \brief Create new column.
 *
 * If the table already has rows, all row values for the new column will
 * be initialized to the default value ("", or zero).  The new column is
 * always created as the last column, and will be column (field)
 * "GetColumnCount()-1" after CreateColumn() has completed successfully.
 *
 * This method is the same as the C function GDALRATCreateColumn().
 *
 * @param pszFieldName the name of the field to create.
 * @param eFieldType the field type (integer, double or string).
 * @param eFieldUsage the field usage, GFU_Generic if not known.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

/**/
/**/

CPLErr
GDALRasterAttributeTable::CreateColumn(const char * /* pszFieldName */,
                                       GDALRATFieldType /* eFieldType */,
                                       GDALRATFieldUsage /* eFieldUsage */)
{
    return CE_Failure;
}

/************************************************************************/
/*                        GDALRATCreateColumn()                         */
/************************************************************************/

/**
 * \brief Create new column.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::CreateColumn()
 */
CPLErr CPL_STDCALL GDALRATCreateColumn(GDALRasterAttributeTableH hRAT,
                                       const char *pszFieldName,
                                       GDALRATFieldType eFieldType,
                                       GDALRATFieldUsage eFieldUsage)

{
    VALIDATE_POINTER1(hRAT, "GDALRATCreateColumn", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->CreateColumn(
        pszFieldName, eFieldType, eFieldUsage);
}

/************************************************************************/
/*                          SetLinearBinning()                          */
/************************************************************************/

/**
 * \brief Set linear binning information.
 *
 * For RATs with equal sized categories (in pixel value space) that are
 * evenly spaced, this method may be used to associate the linear binning
 * information with the table.
 *
 * This method is the same as the C function GDALRATSetLinearBinning().
 *
 * @param dfRow0MinIn the lower bound (pixel value) of the first category.
 * @param dfBinSizeIn the width of each category (in pixel value units).
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr GDALRasterAttributeTable::SetLinearBinning(CPL_UNUSED double dfRow0MinIn,
                                                  CPL_UNUSED double dfBinSizeIn)
{
    return CE_Failure;
}

/************************************************************************/
/*                      GDALRATSetLinearBinning()                       */
/************************************************************************/

/**
 * \brief Set linear binning information.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetLinearBinning()
 */
CPLErr CPL_STDCALL GDALRATSetLinearBinning(GDALRasterAttributeTableH hRAT,
                                           double dfRow0Min, double dfBinSize)

{
    VALIDATE_POINTER1(hRAT, "GDALRATSetLinearBinning", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->SetLinearBinning(
        dfRow0Min, dfBinSize);
}

/************************************************************************/
/*                          GetLinearBinning()                          */
/************************************************************************/

/**
 * \brief Get linear binning information.
 *
 * Returns linear binning information if any is associated with the RAT.
 *
 * This method is the same as the C function GDALRATGetLinearBinning().
 *
 * @param pdfRow0Min (out) the lower bound (pixel value) of the first category.
 * @param pdfBinSize (out) the width of each category (in pixel value units).
 *
 * @return TRUE if linear binning information exists or FALSE if there is none.
 */

int GDALRasterAttributeTable::GetLinearBinning(
    CPL_UNUSED double *pdfRow0Min, CPL_UNUSED double *pdfBinSize) const
{
    return false;
}

/************************************************************************/
/*                      GDALRATGetLinearBinning()                       */
/************************************************************************/

/**
 * \brief Get linear binning information.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetLinearBinning()
 */
int CPL_STDCALL GDALRATGetLinearBinning(GDALRasterAttributeTableH hRAT,
                                        double *pdfRow0Min, double *pdfBinSize)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetLinearBinning", 0);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetLinearBinning(
        pdfRow0Min, pdfBinSize);
}

/************************************************************************/
/*                        GDALRATGetTableType()                         */
/************************************************************************/

/**
 * \brief Get Rat Table Type
 *
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetTableType()
 */
GDALRATTableType CPL_STDCALL GDALRATGetTableType(GDALRasterAttributeTableH hRAT)
{
    VALIDATE_POINTER1(hRAT, "GDALRATGetTableType", GRTT_THEMATIC);

    return GDALDefaultRasterAttributeTable::FromHandle(hRAT)->GetTableType();
}

/************************************************************************/
/*                        GDALRATSetTableType()                         */
/************************************************************************/

/**
 * \brief Set RAT Table Type
 *
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetTableType()
 */
CPLErr CPL_STDCALL GDALRATSetTableType(GDALRasterAttributeTableH hRAT,
                                       const GDALRATTableType eInTableType)

{
    VALIDATE_POINTER1(hRAT, "GDALRATSetTableType", CE_Failure);

    return GDALDefaultRasterAttributeTable::FromHandle(hRAT)->SetTableType(
        eInTableType);
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

/** Serialize as a XML tree.
 * @return XML tree.
 */
CPLXMLNode *GDALRasterAttributeTable::Serialize() const

{
    if ((GetColumnCount() == 0) && (GetRowCount() == 0))
        return nullptr;

    CPLXMLNode *psTree =
        CPLCreateXMLNode(nullptr, CXT_Element, "GDALRasterAttributeTable");

    /* -------------------------------------------------------------------- */
    /*      Add attributes with regular binning info if appropriate.        */
    /* -------------------------------------------------------------------- */
    char szValue[128] = {'\0'};
    double dfRow0Min = 0.0;
    double dfBinSize = 0.0;

    if (GetLinearBinning(&dfRow0Min, &dfBinSize))
    {
        CPLsnprintf(szValue, sizeof(szValue), "%.16g", dfRow0Min);
        CPLCreateXMLNode(CPLCreateXMLNode(psTree, CXT_Attribute, "Row0Min"),
                         CXT_Text, szValue);

        CPLsnprintf(szValue, sizeof(szValue), "%.16g", dfBinSize);
        CPLCreateXMLNode(CPLCreateXMLNode(psTree, CXT_Attribute, "BinSize"),
                         CXT_Text, szValue);
    }

    /* -------------------------------------------------------------------- */
    /*      Store table type                                                */
    /* -------------------------------------------------------------------- */
    const GDALRATTableType tableType = GetTableType();
    if (tableType == GRTT_ATHEMATIC)
    {
        CPLsnprintf(szValue, sizeof(szValue), "athematic");
    }
    else
    {
        CPLsnprintf(szValue, sizeof(szValue), "thematic");
    }
    CPLCreateXMLNode(CPLCreateXMLNode(psTree, CXT_Attribute, "tableType"),
                     CXT_Text, szValue);

    /* -------------------------------------------------------------------- */
    /*      Define each column.                                             */
    /* -------------------------------------------------------------------- */
    const int iColCount = GetColumnCount();

    for (int iCol = 0; iCol < iColCount; iCol++)
    {
        CPLXMLNode *psCol = CPLCreateXMLNode(psTree, CXT_Element, "FieldDefn");

        snprintf(szValue, sizeof(szValue), "%d", iCol);
        CPLCreateXMLNode(CPLCreateXMLNode(psCol, CXT_Attribute, "index"),
                         CXT_Text, szValue);

        CPLCreateXMLElementAndValue(psCol, "Name", GetNameOfCol(iCol));

        snprintf(szValue, sizeof(szValue), "%d",
                 static_cast<int>(GetTypeOfCol(iCol)));
        CPLXMLNode *psType =
            CPLCreateXMLElementAndValue(psCol, "Type", szValue);
        CPLAddXMLAttributeAndValue(psType, "typeAsString",
                                   GDALGetRATFieldTypeName(GetTypeOfCol(iCol)));

        snprintf(szValue, sizeof(szValue), "%d",
                 static_cast<int>(GetUsageOfCol(iCol)));
        CPLXMLNode *psUsage =
            CPLCreateXMLElementAndValue(psCol, "Usage", szValue);
        CPLAddXMLAttributeAndValue(
            psUsage, "usageAsString",
            GDALGetRATFieldUsageName(GetUsageOfCol(iCol)));
    }

    /* -------------------------------------------------------------------- */
    /*      Write out each row.                                             */
    /* -------------------------------------------------------------------- */
    const int iRowCount = GetRowCount();
    CPLXMLNode *psTail = nullptr;
    CPLXMLNode *psRow = nullptr;

    for (int iRow = 0; iRow < iRowCount; iRow++)
    {
        psRow = CPLCreateXMLNode(nullptr, CXT_Element, "Row");
        if (psTail == nullptr)
            CPLAddXMLChild(psTree, psRow);
        else
            psTail->psNext = psRow;
        psTail = psRow;

        snprintf(szValue, sizeof(szValue), "%d", iRow);
        CPLCreateXMLNode(CPLCreateXMLNode(psRow, CXT_Attribute, "index"),
                         CXT_Text, szValue);

        std::string osStr;
        for (int iCol = 0; iCol < iColCount; iCol++)
        {
            const char *pszValue = szValue;

            switch (GetTypeOfCol(iCol))
            {
                case GFT_Integer:
                    snprintf(szValue, sizeof(szValue), "%d",
                             GetValueAsInt(iRow, iCol));
                    break;

                case GFT_Real:
                    CPLsnprintf(szValue, sizeof(szValue), "%.16g",
                                GetValueAsDouble(iRow, iCol));
                    break;

                case GFT_String:
                    pszValue = GetValueAsString(iRow, iCol);
                    break;

                case GFT_Boolean:
                    pszValue = GetValueAsBoolean(iRow, iCol) ? "true" : "false";
                    break;

                case GFT_DateTime:
                    osStr = DateTimeToString(GetValueAsDateTime(iRow, iCol));
                    pszValue = osStr.c_str();
                    break;

                case GFT_WKBGeometry:
                {
                    size_t nWKBSize = 0;
                    const GByte *pabyWKB =
                        GetValueAsWKBGeometry(iRow, iCol, nWKBSize);
                    osStr = WKBGeometryToWKT(pabyWKB, nWKBSize);
                    pszValue = osStr.c_str();
                    break;
                }
            }

            CPLCreateXMLElementAndValue(psRow, "F", pszValue);
        }
    }

    return psTree;
}

/************************************************************************/
/*                           SerializeJSON()                            */
/************************************************************************/

/** Serialize as a JSON object.
 * @return JSON object (of type json_object*)
 */
void *GDALRasterAttributeTable::SerializeJSON() const

{
    json_object *poRAT = json_object_new_object();

    if ((GetColumnCount() == 0) && (GetRowCount() == 0))
        return poRAT;

    /* -------------------------------------------------------------------- */
    /*      Add attributes with regular binning info if appropriate.        */
    /* -------------------------------------------------------------------- */
    double dfRow0Min = 0.0;
    double dfBinSize = 0.0;
    json_object *poRow0Min = nullptr;
    json_object *poBinSize = nullptr;
    json_object *poTableType = nullptr;

    if (GetLinearBinning(&dfRow0Min, &dfBinSize))
    {
        poRow0Min = json_object_new_double_with_precision(dfRow0Min, 16);
        json_object_object_add(poRAT, "row0Min", poRow0Min);

        poBinSize = json_object_new_double_with_precision(dfBinSize, 16);
        json_object_object_add(poRAT, "binSize", poBinSize);
    }

    /* -------------------------------------------------------------------- */
    /*      Table Type                                                      */
    /* -------------------------------------------------------------------- */
    const GDALRATTableType tableType = GetTableType();
    if (tableType == GRTT_ATHEMATIC)
    {
        poTableType = json_object_new_string("athematic");
    }
    else
    {
        poTableType = json_object_new_string("thematic");
    }
    json_object_object_add(poRAT, "tableType", poTableType);

    /* -------------------------------------------------------------------- */
    /*      Define each column.                                             */
    /* -------------------------------------------------------------------- */
    const int iColCount = GetColumnCount();
    json_object *poFieldDefnArray = json_object_new_array();

    for (int iCol = 0; iCol < iColCount; iCol++)
    {
        json_object *const poFieldDefn = json_object_new_object();

        json_object *const poColumnIndex = json_object_new_int(iCol);
        json_object_object_add(poFieldDefn, "index", poColumnIndex);

        json_object *const poName = json_object_new_string(GetNameOfCol(iCol));
        json_object_object_add(poFieldDefn, "name", poName);

        json_object *const poType =
            json_object_new_int(static_cast<int>(GetTypeOfCol(iCol)));
        json_object_object_add(poFieldDefn, "type", poType);

        json_object *const poUsage =
            json_object_new_int(static_cast<int>(GetUsageOfCol(iCol)));
        json_object_object_add(poFieldDefn, "usage", poUsage);

        json_object_array_add(poFieldDefnArray, poFieldDefn);
    }

    json_object_object_add(poRAT, "fieldDefn", poFieldDefnArray);

    /* -------------------------------------------------------------------- */
    /*      Write out each row.                                             */
    /* -------------------------------------------------------------------- */
    const int iRowCount = GetRowCount();
    json_object *poRowArray = json_object_new_array();

    for (int iRow = 0; iRow < iRowCount; iRow++)
    {
        json_object *const poRow = json_object_new_object();

        json_object *const poRowIndex = json_object_new_int(iRow);
        json_object_object_add(poRow, "index", poRowIndex);

        json_object *const poFArray = json_object_new_array();

        for (int iCol = 0; iCol < iColCount; iCol++)
        {
            json_object *poF = nullptr;
            switch (GetTypeOfCol(iCol))
            {
                case GFT_Integer:
                    poF = json_object_new_int(GetValueAsInt(iRow, iCol));
                    break;

                case GFT_Real:
                    poF = json_object_new_double_with_precision(
                        GetValueAsDouble(iRow, iCol), 16);
                    break;

                case GFT_String:
                    poF = json_object_new_string(GetValueAsString(iRow, iCol));
                    break;

                case GFT_Boolean:
                    poF =
                        json_object_new_boolean(GetValueAsBoolean(iRow, iCol));
                    break;

                case GFT_DateTime:
                case GFT_WKBGeometry:
                {
                    const char *pszV = GetValueAsString(iRow, iCol);
                    if (pszV[0])
                        poF = json_object_new_string(pszV);
                    break;
                }
            }

            json_object_array_add(poFArray, poF);
        }
        json_object_object_add(poRow, "f", poFArray);
        json_object_array_add(poRowArray, poRow);
    }
    json_object_object_add(poRAT, "row", poRowArray);

    return poRAT;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

/** Deserialize from XML.
 * @param psTree XML tree
 * @return error code.
 */
CPLErr GDALRasterAttributeTable::XMLInit(const CPLXMLNode *psTree,
                                         const char * /*pszVRTPath*/)

{
    CPLAssert(GetRowCount() == 0 && GetColumnCount() == 0);

    /* -------------------------------------------------------------------- */
    /*      Linear binning.                                                 */
    /* -------------------------------------------------------------------- */
    if (CPLGetXMLValue(psTree, "Row0Min", nullptr) &&
        CPLGetXMLValue(psTree, "BinSize", nullptr))
    {
        SetLinearBinning(CPLAtof(CPLGetXMLValue(psTree, "Row0Min", "")),
                         CPLAtof(CPLGetXMLValue(psTree, "BinSize", "")));
    }

    /* -------------------------------------------------------------------- */
    /*      Table Type                                                      */
    /* -------------------------------------------------------------------- */
    if (CPLGetXMLValue(psTree, "tableType", nullptr))
    {
        const char *pszValue = CPLGetXMLValue(psTree, "tableType", "thematic");
        if (EQUAL(pszValue, "athematic"))
        {
            SetTableType(GRTT_ATHEMATIC);
        }
        else
        {
            SetTableType(GRTT_THEMATIC);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Column definitions                                              */
    /* -------------------------------------------------------------------- */

    for (CPLXMLNode *psChild = psTree->psChild; psChild != nullptr;
         psChild = psChild->psNext)
    {
        if (psChild->eType == CXT_Element &&
            EQUAL(psChild->pszValue, "FieldDefn"))
        {
            int nType = atoi(CPLGetXMLValue(psChild, "Type", "1"));
            if (nType < 0 || nType >= GFT_MaxCount)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid RAT field type: %d(%s). Dealing as if it was "
                         "String",
                         nType,
                         CPLGetXMLValue(psChild, "typeAsString", "(unknown)"));
                nType = GFT_String;
            }
            int nUsage = atoi(CPLGetXMLValue(psChild, "Usage", "0"));
            if (nUsage < 0 || nUsage >= GFU_MaxCount)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid RAT field usage: %d(%s). Dealing as if it "
                         "was Generic",
                         nUsage,
                         CPLGetXMLValue(psChild, "usageAsString", "(unknown)"));
                nUsage = GFU_Generic;
            }
            CreateColumn(CPLGetXMLValue(psChild, "Name", ""),
                         static_cast<GDALRATFieldType>(nType),
                         static_cast<GDALRATFieldUsage>(nUsage));
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Row data.                                                       */
    /* -------------------------------------------------------------------- */
    for (const CPLXMLNode *psChild = psTree->psChild; psChild != nullptr;
         psChild = psChild->psNext)
    {
        if (psChild->eType == CXT_Element && EQUAL(psChild->pszValue, "Row"))
        {
            const int iRow = atoi(CPLGetXMLValue(psChild, "index", "0"));
            int iField = 0;

            for (CPLXMLNode *psF = psChild->psChild; psF != nullptr;
                 psF = psF->psNext)
            {
                if (psF->eType != CXT_Element || !EQUAL(psF->pszValue, "F"))
                    continue;

                if (psF->psChild != nullptr && psF->psChild->eType == CXT_Text)
                    SetValue(iRow, iField++, psF->psChild->pszValue);
                else
                    SetValue(iRow, iField++, "");
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                      InitializeFromColorTable()                      */
/************************************************************************/

/**
 * \brief Initialize from color table.
 *
 * This method will setup a whole raster attribute table based on the
 * contents of the passed color table.  The Value (GFU_MinMax),
 * Red (GFU_Red), Green (GFU_Green), Blue (GFU_Blue), and Alpha (GFU_Alpha)
 * fields are created, and a row is set for each entry in the color table.
 *
 * The raster attribute table must be empty before calling
 * InitializeFromColorTable().
 *
 * The Value fields are set based on the implicit assumption with color
 * tables that entry 0 applies to pixel value 0, 1 to 1, etc.
 *
 * This method is the same as the C function GDALRATInitializeFromColorTable().
 *
 * @param poTable the color table to copy from.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr GDALRasterAttributeTable::InitializeFromColorTable(
    const GDALColorTable *poTable)

{
    if (GetRowCount() > 0 || GetColumnCount() > 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Raster Attribute Table not empty in "
                 "InitializeFromColorTable()");
        return CE_Failure;
    }

    SetLinearBinning(0.0, 1.0);
    CreateColumn("Value", GFT_Integer, GFU_MinMax);
    CreateColumn("Red", GFT_Integer, GFU_Red);
    CreateColumn("Green", GFT_Integer, GFU_Green);
    CreateColumn("Blue", GFT_Integer, GFU_Blue);
    CreateColumn("Alpha", GFT_Integer, GFU_Alpha);

    SetRowCount(poTable->GetColorEntryCount());

    for (int iRow = 0; iRow < poTable->GetColorEntryCount(); iRow++)
    {
        GDALColorEntry sEntry;

        poTable->GetColorEntryAsRGB(iRow, &sEntry);

        SetValue(iRow, 0, iRow);
        SetValue(iRow, 1, sEntry.c1);
        SetValue(iRow, 2, sEntry.c2);
        SetValue(iRow, 3, sEntry.c3);
        SetValue(iRow, 4, sEntry.c4);
    }

    return CE_None;
}

/************************************************************************/
/*                  GDALRATInitializeFromColorTable()                   */
/************************************************************************/

/**
 * \brief Initialize from color table.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::InitializeFromColorTable()
 */
CPLErr CPL_STDCALL GDALRATInitializeFromColorTable(
    GDALRasterAttributeTableH hRAT, GDALColorTableH hCT)

{
    VALIDATE_POINTER1(hRAT, "GDALRATInitializeFromColorTable", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->InitializeFromColorTable(
        GDALColorTable::FromHandle(hCT));
}

/************************************************************************/
/*                       TranslateToColorTable()                        */
/************************************************************************/

/**
 * \brief Translate to a color table.
 *
 * This method will attempt to create a corresponding GDALColorTable from
 * this raster attribute table.
 *
 * This method is the same as the C function GDALRATTranslateToColorTable().
 *
 * @param nEntryCount The number of entries to produce (0 to nEntryCount-1),
 * or -1 to auto-determine the number of entries.
 *
 * @return the generated color table or NULL on failure.
 */

GDALColorTable *GDALRasterAttributeTable::TranslateToColorTable(int nEntryCount)

{
    /* -------------------------------------------------------------------- */
    /*      Establish which fields are red, green, blue and alpha.          */
    /* -------------------------------------------------------------------- */
    const int iRed = GetColOfUsage(GFU_Red);
    const int iGreen = GetColOfUsage(GFU_Green);
    const int iBlue = GetColOfUsage(GFU_Blue);

    if (iRed == -1 || iGreen == -1 || iBlue == -1)
        return nullptr;

    const int iAlpha = GetColOfUsage(GFU_Alpha);

    /* -------------------------------------------------------------------- */
    /*      If we aren't given an explicit number of values to scan for,    */
    /*      search for the maximum "max" value.                             */
    /* -------------------------------------------------------------------- */
    if (nEntryCount == -1)
    {
        int iMaxCol = GetColOfUsage(GFU_Max);
        if (iMaxCol == -1)
            iMaxCol = GetColOfUsage(GFU_MinMax);

        if (iMaxCol == -1 || GetRowCount() == 0)
            return nullptr;

        for (int iRow = 0; iRow < GetRowCount(); iRow++)
        {
            nEntryCount = std::max(
                nEntryCount, std::min(65535, GetValueAsInt(iRow, iMaxCol)) + 1);
        }

        if (nEntryCount < 0)
            return nullptr;

        // Restrict our number of entries to something vaguely sensible.
        nEntryCount = std::min(65535, nEntryCount);
    }

    /* -------------------------------------------------------------------- */
    /*      Assign values to color table.                                   */
    /* -------------------------------------------------------------------- */
    GDALColorTable *poCT = new GDALColorTable();

    for (int iEntry = 0; iEntry < nEntryCount; iEntry++)
    {
        GDALColorEntry sColor = {0, 0, 0, 0};
        const int iRow = GetRowOfValue(iEntry);

        if (iRow != -1)
        {
            sColor.c1 = static_cast<short>(GetValueAsInt(iRow, iRed));
            sColor.c2 = static_cast<short>(GetValueAsInt(iRow, iGreen));
            sColor.c3 = static_cast<short>(GetValueAsInt(iRow, iBlue));
            if (iAlpha == -1)
                sColor.c4 = 255;
            else
                sColor.c4 = static_cast<short>(GetValueAsInt(iRow, iAlpha));
        }

        poCT->SetColorEntry(iEntry, &sColor);
    }

    return poCT;
}

/************************************************************************/
/*                  GDALRATInitializeFromColorTable()                   */
/************************************************************************/

/**
 * \brief Translate to a color table.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::TranslateToColorTable()
 */
GDALColorTableH CPL_STDCALL
GDALRATTranslateToColorTable(GDALRasterAttributeTableH hRAT, int nEntryCount)

{
    VALIDATE_POINTER1(hRAT, "GDALRATTranslateToColorTable", nullptr);

    return GDALRasterAttributeTable::FromHandle(hRAT)->TranslateToColorTable(
        nEntryCount);
}

/************************************************************************/
/*                            DumpReadable()                            */
/************************************************************************/

/**
 * \brief Dump RAT in readable form.
 *
 * Currently the readable form is the XML encoding ... only barely
 * readable.
 *
 * This method is the same as the C function GDALRATDumpReadable().
 *
 * @param fp file to dump to or NULL for stdout.
 */

void GDALRasterAttributeTable::DumpReadable(FILE *fp)

{
    CPLXMLNode *psTree = Serialize();
    char *const pszXMLText = CPLSerializeXMLTree(psTree);

    CPLDestroyXMLNode(psTree);

    if (fp == nullptr)
        fp = stdout;

    fprintf(fp, "%s\n", pszXMLText);

    CPLFree(pszXMLText);
}

/************************************************************************/
/*                        GDALRATDumpReadable()                         */
/************************************************************************/

/**
 * \brief Dump RAT in readable form.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::DumpReadable()
 */
void CPL_STDCALL GDALRATDumpReadable(GDALRasterAttributeTableH hRAT, FILE *fp)

{
    VALIDATE_POINTER0(hRAT, "GDALRATDumpReadable");

    GDALRasterAttributeTable::FromHandle(hRAT)->DumpReadable(fp);
}

/* \class GDALDefaultRasterAttributeTable
 *
 * An implementation of GDALRasterAttributeTable that keeps
 * all data in memory. This is the same as the implementation
 * of GDALRasterAttributeTable in GDAL <= 1.10.
 */

/************************************************************************/
/*                  GDALDefaultRasterAttributeTable()                   */
/*                                                                      */
/*      Simple initialization constructor.                              */
/************************************************************************/

//! Construct empty table.

GDALDefaultRasterAttributeTable::GDALDefaultRasterAttributeTable() = default;

/************************************************************************/
/*                   GDALCreateRasterAttributeTable()                   */
/************************************************************************/

/**
 * \brief Construct empty table.
 *
 * This function is the same as the C++ method
 * GDALDefaultRasterAttributeTable::GDALDefaultRasterAttributeTable()
 */
GDALRasterAttributeTableH CPL_STDCALL GDALCreateRasterAttributeTable()

{
    return new GDALDefaultRasterAttributeTable();
}

/************************************************************************/
/*                 ~GDALDefaultRasterAttributeTable()                   */
/*                                                                      */
/*      All magic done by magic by the container destructors.           */
/************************************************************************/

GDALDefaultRasterAttributeTable::~GDALDefaultRasterAttributeTable() = default;

/************************************************************************/
/*                  GDALDestroyRasterAttributeTable()                   */
/************************************************************************/

/**
 * \brief Destroys a RAT.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::~GDALRasterAttributeTable()
 */
void CPL_STDCALL GDALDestroyRasterAttributeTable(GDALRasterAttributeTableH hRAT)

{
    if (hRAT != nullptr)
        delete GDALRasterAttributeTable::FromHandle(hRAT);
}

/************************************************************************/
/*                           AnalyseColumns()                           */
/*                                                                      */
/*      Internal method to work out which column to use for various     */
/*      tasks.                                                          */
/************************************************************************/

void GDALDefaultRasterAttributeTable::AnalyseColumns()

{
    bColumnsAnalysed = true;

    nMinCol = GetColOfUsage(GFU_Min);
    if (nMinCol == -1)
        nMinCol = GetColOfUsage(GFU_MinMax);

    nMaxCol = GetColOfUsage(GFU_Max);
    if (nMaxCol == -1)
        nMaxCol = GetColOfUsage(GFU_MinMax);
}

/************************************************************************/
/*                           GetColumnCount()                           */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetColumnCount() const

{
    return static_cast<int>(aoFields.size());
}

/************************************************************************/
/*                       GDALRATGetColumnCount()                        */
/************************************************************************/

/**
 * \brief Fetch table column count.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetColumnCount()
 */
int CPL_STDCALL GDALRATGetColumnCount(GDALRasterAttributeTableH hRAT)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetColumnCount", 0);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetColumnCount();
}

/************************************************************************/
/*                            GetNameOfCol()                            */
/************************************************************************/

/** \brief Fetch name of indicated column.
 * @param iCol column index.
 * @return name.
 */
const char *GDALDefaultRasterAttributeTable::GetNameOfCol(int iCol) const

{
    if (iCol < 0 || iCol >= static_cast<int>(aoFields.size()))
        return "";

    return aoFields[iCol].sName;
}

/************************************************************************/
/*                        GDALRATGetNameOfCol()                         */
/************************************************************************/

/**
 * \brief Fetch name of indicated column.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetNameOfCol()
 * @param hRAT RAT handle.
 * @param iCol column index.
 * @return name.
 */
const char *CPL_STDCALL GDALRATGetNameOfCol(GDALRasterAttributeTableH hRAT,
                                            int iCol)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetNameOfCol", nullptr);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetNameOfCol(iCol);
}

/************************************************************************/
/*                           GetUsageOfCol()                            */
/************************************************************************/

/**
 * \brief Fetch column usage value.
 *
 * @param iCol column index.
 * @return usage.
 */
GDALRATFieldUsage GDALDefaultRasterAttributeTable::GetUsageOfCol(int iCol) const

{
    if (iCol < 0 || iCol >= static_cast<int>(aoFields.size()))
        return GFU_Generic;

    return aoFields[iCol].eUsage;
}

/************************************************************************/
/*                        GDALRATGetUsageOfCol()                        */
/************************************************************************/

/**
 * \brief Fetch column usage value.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetUsageOfCol()
 * @param hRAT RAT handle.
 * @param iCol column index.
 * @return usage.
 */
GDALRATFieldUsage CPL_STDCALL
GDALRATGetUsageOfCol(GDALRasterAttributeTableH hRAT, int iCol)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetUsageOfCol", GFU_Generic);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetUsageOfCol(iCol);
}

/************************************************************************/
/*                            GetTypeOfCol()                            */
/************************************************************************/

/**
 * \brief Fetch column type.
 *
 * @param iCol column index.
 * @return type.
 */
GDALRATFieldType GDALDefaultRasterAttributeTable::GetTypeOfCol(int iCol) const

{
    if (iCol < 0 || iCol >= static_cast<int>(aoFields.size()))
        return GFT_Integer;

    return aoFields[iCol].eType;
}

/************************************************************************/
/*                        GDALRATGetTypeOfCol()                         */
/************************************************************************/

/**
 * \brief Fetch column type.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetTypeOfCol()
 * @param hRAT RAT handle.
 * @param iCol column index.
 * @return type.
 */
GDALRATFieldType CPL_STDCALL GDALRATGetTypeOfCol(GDALRasterAttributeTableH hRAT,
                                                 int iCol)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetTypeOfCol", GFT_Integer);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetTypeOfCol(iCol);
}

/************************************************************************/
/*                           GetColOfUsage()                            */
/************************************************************************/

/** Return the index of the column that corresponds to the passed usage.
 * @param eUsage usage.
 * @return column index, or -1 in case of error.
 */
int GDALDefaultRasterAttributeTable::GetColOfUsage(
    GDALRATFieldUsage eUsage) const

{
    for (unsigned int i = 0; i < aoFields.size(); i++)
    {
        if (aoFields[i].eUsage == eUsage)
            return i;
    }

    return -1;
}

/************************************************************************/
/*                        GDALRATGetColOfUsage()                        */
/************************************************************************/

/**
 * \brief Fetch column index for given usage.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetColOfUsage()
 */
int CPL_STDCALL GDALRATGetColOfUsage(GDALRasterAttributeTableH hRAT,
                                     GDALRATFieldUsage eUsage)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetColOfUsage", 0);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetColOfUsage(eUsage);
}

/************************************************************************/
/*                            GetRowCount()                             */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetRowCount() const

{
    return static_cast<int>(nRowCount);
}

/************************************************************************/
/*                        GDALRATGetUsageOfCol()                        */
/************************************************************************/
/**
 * \brief Fetch row count.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetRowCount()
 */
int CPL_STDCALL GDALRATGetRowCount(GDALRasterAttributeTableH hRAT)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetRowCount", 0);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetRowCount();
}

/************************************************************************/
/*                          GetValueAsString()                          */
/************************************************************************/

const char *GDALDefaultRasterAttributeTable::GetValueAsString(int iRow,
                                                              int iField) const

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return "";
    }

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return "";
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
        {
            const_cast<GDALDefaultRasterAttributeTable *>(this)
                ->osWorkingResult.Printf("%d", aoFields[iField].anValues[iRow]);
            return osWorkingResult;
        }

        case GFT_Real:
        {
            const_cast<GDALDefaultRasterAttributeTable *>(this)
                ->osWorkingResult.Printf("%.16g",
                                         aoFields[iField].adfValues[iRow]);
            return osWorkingResult;
        }

        case GFT_String:
        {
            return aoFields[iField].aosValues[iRow];
        }

        case GFT_Boolean:
        {
            return aoFields[iField].abValues[iRow] ? "true" : "false";
        }

        case GFT_DateTime:
        {
            const auto &sDateTime = aoFields[iField].asDateTimeValues[iRow];
            const_cast<GDALDefaultRasterAttributeTable *>(this)
                ->osWorkingResult = DateTimeToString(sDateTime);
            return osWorkingResult;
        }

        case GFT_WKBGeometry:
        {
            OGRGeometry *poGeom = nullptr;
            if (!aoFields[iField].aabyWKBGeometryValues[iRow].empty() &&
                OGRGeometryFactory::createFromWkb(
                    aoFields[iField].aabyWKBGeometryValues[iRow].data(),
                    nullptr, &poGeom,
                    aoFields[iField].aabyWKBGeometryValues[iRow].size(),
                    wkbVariantIso) == OGRERR_NONE)
            {
                const_cast<GDALDefaultRasterAttributeTable *>(this)
                    ->osWorkingResult = poGeom->exportToWkt();
            }
            else
            {
                const_cast<GDALDefaultRasterAttributeTable *>(this)
                    ->osWorkingResult.clear();
            }
            delete poGeom;
            return osWorkingResult;
        }
    }

    return "";
}

/************************************************************************/
/*                      GDALRATGetValueAsString()                       */
/************************************************************************/
/**
 * \brief Fetch field value as a string.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetValueAsString()
 */
const char *CPL_STDCALL GDALRATGetValueAsString(GDALRasterAttributeTableH hRAT,
                                                int iRow, int iField)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetValueAsString", nullptr);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetValueAsString(iRow,
                                                                        iField);
}

/************************************************************************/
/*                           GetValueAsInt()                            */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetValueAsInt(int iRow, int iField) const

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return 0;
    }

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return 0;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
            return aoFields[iField].anValues[iRow];

        case GFT_Real:
            return static_cast<int>(aoFields[iField].adfValues[iRow]);

        case GFT_String:
            return atoi(aoFields[iField].aosValues[iRow].c_str());

        case GFT_Boolean:
            return aoFields[iField].abValues[iRow];

        case GFT_DateTime:
        case GFT_WKBGeometry:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            break;
    }

    return 0;
}

/************************************************************************/
/*                        GDALRATGetValueAsInt()                        */
/************************************************************************/

/**
 * \brief Fetch field value as a integer.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetValueAsInt()
 */
int CPL_STDCALL GDALRATGetValueAsInt(GDALRasterAttributeTableH hRAT, int iRow,
                                     int iField)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetValueAsInt", 0);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetValueAsInt(iRow,
                                                                     iField);
}

/************************************************************************/
/*                          GetValueAsDouble()                          */
/************************************************************************/

double GDALDefaultRasterAttributeTable::GetValueAsDouble(int iRow,
                                                         int iField) const

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return 0;
    }

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return 0;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
            return aoFields[iField].anValues[iRow];

        case GFT_Real:
            return aoFields[iField].adfValues[iRow];

        case GFT_String:
            return CPLAtof(aoFields[iField].aosValues[iRow].c_str());

        case GFT_Boolean:
            return aoFields[iField].abValues[iRow];

        case GFT_DateTime:
        case GFT_WKBGeometry:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            break;
    }

    return 0;
}

/************************************************************************/
/*                      GDALRATGetValueAsDouble()                       */
/************************************************************************/

/**
 * \brief Fetch field value as a double.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetValueAsDouble()
 */
double CPL_STDCALL GDALRATGetValueAsDouble(GDALRasterAttributeTableH hRAT,
                                           int iRow, int iField)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetValueAsDouble", 0);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetValueAsDouble(iRow,
                                                                        iField);
}

/************************************************************************/
/*                         GetValueAsBoolean()                          */
/************************************************************************/

bool GDALDefaultRasterAttributeTable::GetValueAsBoolean(int iRow,
                                                        int iField) const

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return false;
    }

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return false;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
            return aoFields[iField].anValues[iRow] != 0;

        case GFT_Real:
            return aoFields[iField].adfValues[iRow] != 0;

        case GFT_String:
            return CPLTestBool(aoFields[iField].aosValues[iRow].c_str());

        case GFT_Boolean:
            return aoFields[iField].abValues[iRow];

        case GFT_DateTime:
        case GFT_WKBGeometry:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            break;
    }

    return false;
}

/************************************************************************/
/*                      GDALRATGetValueAsBoolean()                      */
/************************************************************************/

/**
 * \brief Fetch field value as a boolean.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetValueAsBoolean()
 *
 * \since 3.12
 */
bool GDALRATGetValueAsBoolean(GDALRasterAttributeTableH hRAT, int iRow,
                              int iField)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetValueAsBoolean", false);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetValueAsBoolean(
        iRow, iField);
}

/************************************************************************/
/*                         GetValueAsDateTime()                         */
/************************************************************************/

GDALRATDateTime
GDALDefaultRasterAttributeTable::GetValueAsDateTime(int iRow, int iField) const

{
    GDALRATDateTime dt;

    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return dt;
    }

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return dt;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_String:
            StringToDateTime(aoFields[iField].aosValues[iRow].c_str(), dt);
            break;

        case GFT_DateTime:
            dt = aoFields[iField].asDateTimeValues[iRow];
            break;

        case GFT_Integer:
        case GFT_Real:
        case GFT_Boolean:
        case GFT_WKBGeometry:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            break;
    }

    return dt;
}

/************************************************************************/
/*                     GDALRATGetValueAsDateTime()                      */
/************************************************************************/

/**
 * \brief Fetch field value as a datetime.
 *
 * The value of the requested column in the requested row is returned
 * as a datetime. Besides being called on a GFT_DateTime field, it
 * is also possible to call this method on a string field that contains a
 * ISO-8601 encoded datetime.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetValueAsDateTime()
 *
 * @param hRAT Raster attribute table handle. Must NOT be null.
 * @param iRow Row index (0-based indexing)
 * @param iField Field index (0-based indexing)
 * @param[out] psDateTime Output date time struct. Must NOT be null.
 * @return error code.
 *
 * \since 3.12
 */
CPLErr GDALRATGetValueAsDateTime(GDALRasterAttributeTableH hRAT, int iRow,
                                 int iField, GDALRATDateTime *psDateTime)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetValueAsBoolean", CE_Failure);
    VALIDATE_POINTER1(psDateTime, "GDALRATGetValueAsBoolean", CE_Failure);

    const auto nErrorCounter = CPLGetErrorCounter();
    *psDateTime =
        GDALRasterAttributeTable::FromHandle(hRAT)->GetValueAsDateTime(iRow,
                                                                       iField);
    return nErrorCounter == CPLGetErrorCounter() ? CE_None : CE_Failure;
}

/************************************************************************/
/*                       GetValueAsWKBGeometry()                        */
/************************************************************************/

const GByte *
GDALDefaultRasterAttributeTable::GetValueAsWKBGeometry(int iRow, int iField,
                                                       size_t &nWKBSize) const

{
    nWKBSize = 0;

    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return nullptr;
    }

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return nullptr;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_String:
        {
            auto abyWKB =
                WKTGeometryToWKB(aoFields[iField].aosValues[iRow].c_str());
            if (!abyWKB.empty())
            {
                nWKBSize = abyWKB.size();
                m_abyWKB = std::move(abyWKB);
                return m_abyWKB.data();
            }
            return nullptr;
        }

        case GFT_WKBGeometry:
        {
            nWKBSize = aoFields[iField].aabyWKBGeometryValues[iRow].size();
            return nWKBSize
                       ? aoFields[iField].aabyWKBGeometryValues[iRow].data()
                       : nullptr;
        }

        case GFT_Integer:
        case GFT_Real:
        case GFT_Boolean:
        case GFT_DateTime:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            break;
    }

    return nullptr;
}

/************************************************************************/
/*                    GDALRATGetValueAsWKBGeometry()                    */
/************************************************************************/

/**
 * \brief Fetch field value as a WKB-encoded geometry.
 *
 * The value of the requested column in the requested row is returned
 * as a WKB geometry. Besides being called on a GFT_WKBGeometry field, it
 * is also possible to call this method on a string field that contains a WKT
 * encoded geometry.
 *
 * The returned pointer may be invalidated by a following call  call to a method
 * of this GDALRasterAttributeTable instance.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::GetValueAsWKBGeometry()
 *
 * \since 3.12
 */
const GByte *GDALRATGetValueAsWKBGeometry(GDALRasterAttributeTableH hRAT,
                                          int iRow, int iField,
                                          size_t *pnWKBSize)

{
    VALIDATE_POINTER1(hRAT, "GDALRATGetValueAsWKBGeometry", nullptr);
    VALIDATE_POINTER1(pnWKBSize, "GDALRATGetValueAsWKBGeometry", nullptr);

    return GDALRasterAttributeTable::FromHandle(hRAT)->GetValueAsWKBGeometry(
        iRow, iField, *pnWKBSize);
}

/************************************************************************/
/*                            SetRowCount()                             */
/************************************************************************/

/** Set row count.
 * @param nNewCount new count.
 */
void GDALDefaultRasterAttributeTable::SetRowCount(int nNewCount)

{
    if (nNewCount == nRowCount)
        return;

    for (auto &oField : aoFields)
    {
        switch (oField.eType)
        {
            case GFT_Integer:
                oField.anValues.resize(nNewCount);
                break;

            case GFT_Real:
                oField.adfValues.resize(nNewCount);
                break;

            case GFT_String:
                oField.aosValues.resize(nNewCount);
                break;

            case GFT_Boolean:
                oField.abValues.resize(nNewCount);
                break;

            case GFT_DateTime:
                oField.asDateTimeValues.resize(nNewCount);
                break;

            case GFT_WKBGeometry:
                oField.aabyWKBGeometryValues.resize(nNewCount);
                break;
        }
    }

    nRowCount = nNewCount;
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

/** Set value
 * @param iRow row index.
 * @param iField field index.
 * @param pszValue value.
 */
CPLErr GDALDefaultRasterAttributeTable::SetValue(int iRow, int iField,
                                                 const char *pszValue)

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iRow == nRowCount)
        SetRowCount(nRowCount + 1);

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return CE_Failure;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
            aoFields[iField].anValues[iRow] = atoi(pszValue);
            break;

        case GFT_Real:
            aoFields[iField].adfValues[iRow] = CPLAtof(pszValue);
            break;

        case GFT_String:
            aoFields[iField].aosValues[iRow] = pszValue;
            break;

        case GFT_Boolean:
            aoFields[iField].abValues[iRow] = CPLTestBool(pszValue);
            break;

        case GFT_DateTime:
        {
            GDALRATDateTime sDateTime;
            StringToDateTime(pszValue, sDateTime);
            aoFields[iField].asDateTimeValues[iRow] = std::move(sDateTime);
            break;
        }

        case GFT_WKBGeometry:
        {
            auto abyWKB = WKTGeometryToWKB(pszValue);
            aoFields[iField].aabyWKBGeometryValues[iRow] = std::move(abyWKB);
            break;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                      GDALRATSetValueAsString()                       */
/************************************************************************/

/**
 * \brief Set field value from string.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetValue()
 * @param hRAT RAT handle.
 * @param iRow row index.
 * @param iField field index.
 * @param pszValue value.
 */
void CPL_STDCALL GDALRATSetValueAsString(GDALRasterAttributeTableH hRAT,
                                         int iRow, int iField,
                                         const char *pszValue)

{
    VALIDATE_POINTER0(hRAT, "GDALRATSetValueAsString");

    GDALRasterAttributeTable::FromHandle(hRAT)->SetValue(iRow, iField,
                                                         pszValue);
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

CPLErr GDALDefaultRasterAttributeTable::SetValue(int iRow, int iField,
                                                 int nValue)

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iRow == nRowCount)
        SetRowCount(nRowCount + 1);

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return CE_Failure;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
            aoFields[iField].anValues[iRow] = nValue;
            break;

        case GFT_Real:
            aoFields[iField].adfValues[iRow] = nValue;
            break;

        case GFT_String:
        {
            char szValue[100];

            snprintf(szValue, sizeof(szValue), "%d", nValue);
            aoFields[iField].aosValues[iRow] = szValue;
            break;
        }

        case GFT_Boolean:
            aoFields[iField].abValues[iRow] = nValue != 0;
            break;

        case GFT_DateTime:
        case GFT_WKBGeometry:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                        GDALRATSetValueAsInt()                        */
/************************************************************************/

/**
 * \brief Set field value from integer.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetValue()
 */
void CPL_STDCALL GDALRATSetValueAsInt(GDALRasterAttributeTableH hRAT, int iRow,
                                      int iField, int nValue)

{
    VALIDATE_POINTER0(hRAT, "GDALRATSetValueAsInt");

    GDALRasterAttributeTable::FromHandle(hRAT)->SetValue(iRow, iField, nValue);
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

CPLErr GDALDefaultRasterAttributeTable::SetValue(int iRow, int iField,
                                                 double dfValue)

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iRow == nRowCount)
        SetRowCount(nRowCount + 1);

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return CE_Failure;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
            aoFields[iField].anValues[iRow] = static_cast<int>(dfValue);
            break;

        case GFT_Real:
            aoFields[iField].adfValues[iRow] = dfValue;
            break;

        case GFT_String:
        {
            char szValue[100] = {'\0'};

            CPLsnprintf(szValue, sizeof(szValue), "%.15g", dfValue);
            aoFields[iField].aosValues[iRow] = szValue;
            break;
        }

        case GFT_Boolean:
            aoFields[iField].abValues[iRow] = dfValue != 0;
            break;

        case GFT_DateTime:
        case GFT_WKBGeometry:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                      GDALRATSetValueAsDouble()                       */
/************************************************************************/

/**
 * \brief Set field value from double.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetValue()
 */
void CPL_STDCALL GDALRATSetValueAsDouble(GDALRasterAttributeTableH hRAT,
                                         int iRow, int iField, double dfValue)

{
    VALIDATE_POINTER0(hRAT, "GDALRATSetValueAsDouble");

    GDALRasterAttributeTable::FromHandle(hRAT)->SetValue(iRow, iField, dfValue);
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

CPLErr GDALDefaultRasterAttributeTable::SetValue(int iRow, int iField,
                                                 bool bValue)

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iRow == nRowCount)
        SetRowCount(nRowCount + 1);

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return CE_Failure;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
        {
            aoFields[iField].anValues[iRow] = bValue ? 1 : 0;
            break;
        }
        case GFT_String:
        {
            aoFields[iField].aosValues[iRow] = bValue ? "true" : "false";
            break;
        }
        case GFT_Real:
        {
            aoFields[iField].adfValues[iRow] = bValue ? 1 : 0;
            break;
        }
        case GFT_Boolean:
        {
            aoFields[iField].abValues[iRow] = bValue;
            break;
        }
        case GFT_DateTime:
        case GFT_WKBGeometry:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                      GDALRATSetValueAsBoolean()                      */
/************************************************************************/

/**
 * \brief Set field value from a boolean value.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetValue()
 *
 * \since 3.12
 */
CPLErr GDALRATSetValueAsBoolean(GDALRasterAttributeTableH hRAT, int iRow,
                                int iField, bool bValue)

{
    VALIDATE_POINTER1(hRAT, "GDALRATSetValueAsBoolean", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->SetValue(iRow, iField,
                                                                bValue);
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

CPLErr
GDALDefaultRasterAttributeTable::SetValue(int iRow, int iField,
                                          const GDALRATDateTime &sDateTime)

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iRow == nRowCount)
        SetRowCount(nRowCount + 1);

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return CE_Failure;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_String:
        {
            aoFields[iField].aosValues[iRow] = DateTimeToString(sDateTime);
            break;
        }

        case GFT_DateTime:
        {
            aoFields[iField].asDateTimeValues[iRow] = sDateTime;
            break;
        }

        case GFT_Integer:
        case GFT_Real:
        case GFT_Boolean:
        case GFT_WKBGeometry:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                     GDALRATSetValueAsDateTime()                      */
/************************************************************************/

/**
 * \brief Set field value from datetime.
 *
 * Note that the GDALRATDateTime::bIsValid field must be set to true if
 * the date time is valid.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetValue()
 *
 * \since 3.12
 */
CPLErr GDALRATSetValueAsDateTime(GDALRasterAttributeTableH hRAT, int iRow,
                                 int iField, const GDALRATDateTime *psDateTime)

{
    VALIDATE_POINTER1(hRAT, "GDALRATSetValueAsDateTime", CE_Failure);
    VALIDATE_POINTER1(psDateTime, "GDALRATSetValueAsDateTime", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->SetValue(iRow, iField,
                                                                *psDateTime);
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

CPLErr GDALDefaultRasterAttributeTable::SetValue(int iRow, int iField,
                                                 const void *pabyWKB,
                                                 size_t nWKBSize)

{
    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iRow == nRowCount)
        SetRowCount(nRowCount + 1);

    if (iRow < 0 || iRow >= nRowCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iRow (%d) out of range.", iRow);

        return CE_Failure;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_String:
        {
            aoFields[iField].aosValues[iRow] =
                WKBGeometryToWKT(pabyWKB, nWKBSize);
            break;
        }

        case GFT_WKBGeometry:
        {
            if (nWKBSize)
                aoFields[iField].aabyWKBGeometryValues[iRow].assign(
                    static_cast<const GByte *>(pabyWKB),
                    static_cast<const GByte *>(pabyWKB) + nWKBSize);
            else
                aoFields[iField].aabyWKBGeometryValues[iRow].clear();
            break;
        }

        case GFT_Integer:
        case GFT_Real:
        case GFT_Boolean:
        case GFT_DateTime:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Incompatible RAT field type");
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                    GDALRATSetValueAsWKBGeometry()                    */
/************************************************************************/

/**
 * \brief Set field value from a WKB-encoded geometry.
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SetValue()
 *
 * \since 3.12
 */
CPLErr GDALRATSetValueAsWKBGeometry(GDALRasterAttributeTableH hRAT, int iRow,
                                    int iField, const void *pabyWKB,
                                    size_t nWKBSize)

{
    VALIDATE_POINTER1(hRAT, "GDALRATSetValueAsWKBGeometry", CE_Failure);

    return GDALRasterAttributeTable::FromHandle(hRAT)->SetValue(
        iRow, iField, pabyWKB, nWKBSize);
}

/************************************************************************/
/*                      ChangesAreWrittenToFile()                       */
/************************************************************************/

int GDALDefaultRasterAttributeTable::ChangesAreWrittenToFile()
{
    // GDALRasterBand.SetDefaultRAT needs to be called on instances of
    // GDALDefaultRasterAttributeTable since changes are just in-memory
    return false;
}

/************************************************************************/
/*                   GDALRATChangesAreWrittenToFile()                   */
/************************************************************************/

/**
 * \brief Determine whether changes made to this RAT are reflected directly in
 * the dataset
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::ChangesAreWrittenToFile()
 */
int CPL_STDCALL GDALRATChangesAreWrittenToFile(GDALRasterAttributeTableH hRAT)
{
    VALIDATE_POINTER1(hRAT, "GDALRATChangesAreWrittenToFile", false);

    return GDALRasterAttributeTable::FromHandle(hRAT)
        ->ChangesAreWrittenToFile();
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetRowOfValue(double dfValue) const

{
    /* -------------------------------------------------------------------- */
    /*      Handle case of regular binning.                                 */
    /* -------------------------------------------------------------------- */
    if (bLinearBinning)
    {
        const int iBin =
            static_cast<int>(floor((dfValue - dfRow0Min) / dfBinSize));
        if (iBin < 0 || iBin >= nRowCount)
            return -1;

        return iBin;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have any information?                                     */
    /* -------------------------------------------------------------------- */
    if (!bColumnsAnalysed)
        const_cast<GDALDefaultRasterAttributeTable *>(this)->AnalyseColumns();

    if (nMinCol == -1 && nMaxCol == -1)
        return -1;

    const GDALRasterAttributeField *poMin = nullptr;
    if (nMinCol != -1)
        poMin = &(aoFields[nMinCol]);
    else
        poMin = nullptr;

    const GDALRasterAttributeField *poMax = nullptr;
    if (nMaxCol != -1)
        poMax = &(aoFields[nMaxCol]);
    else
        poMax = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Search through rows for match.                                  */
    /* -------------------------------------------------------------------- */
    for (int iRow = 0; iRow < nRowCount; iRow++)
    {
        if (poMin != nullptr)
        {
            if (poMin->eType == GFT_Integer)
            {
                while (iRow < nRowCount && dfValue < poMin->anValues[iRow])
                    iRow++;
            }
            else if (poMin->eType == GFT_Real)
            {
                while (iRow < nRowCount && dfValue < poMin->adfValues[iRow])
                    iRow++;
            }

            if (iRow == nRowCount)
                break;
        }

        if (poMax != nullptr)
        {
            if ((poMax->eType == GFT_Integer &&
                 dfValue > poMax->anValues[iRow]) ||
                (poMax->eType == GFT_Real && dfValue > poMax->adfValues[iRow]))
                continue;
        }

        return iRow;
    }

    return -1;
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/*                                                                      */
/*      Int arg for now just converted to double.  Perhaps we will      */
/*      handle this in a special way some day?                          */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetRowOfValue(int nValue) const

{
    return GetRowOfValue(static_cast<double>(nValue));
}

/************************************************************************/
/*                          SetLinearBinning()                          */
/************************************************************************/

CPLErr GDALDefaultRasterAttributeTable::SetLinearBinning(double dfRow0MinIn,
                                                         double dfBinSizeIn)

{
    bLinearBinning = true;
    dfRow0Min = dfRow0MinIn;
    dfBinSize = dfBinSizeIn;

    return CE_None;
}

/************************************************************************/
/*                          GetLinearBinning()                          */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetLinearBinning(double *pdfRow0Min,
                                                      double *pdfBinSize) const

{
    if (!bLinearBinning)
        return false;

    *pdfRow0Min = dfRow0Min;
    *pdfBinSize = dfBinSize;

    return true;
}

/************************************************************************/
/*                            GetTableType()                            */
/************************************************************************/

/**
 * \brief Get RAT Table Type
 *
 * Returns whether table type is thematic or athematic
 *
 * This method is the same as the C function GDALRATGetTableType().
 *
 *
 * @return GRTT_THEMATIC or GRTT_ATHEMATIC
 */

GDALRATTableType GDALDefaultRasterAttributeTable::GetTableType() const
{
    return eTableType;
}

/************************************************************************/
/*                            SetTableType()                            */
/************************************************************************/

/**
 * \brief Set RAT Table Type
 *
 * Set whether table type is thematic or athematic
 *
 * This method is the same as the C function GDALRATSetTableType().
 *
 * @param eInTableType the new RAT table type (GRTT_THEMATIC or GRTT_ATHEMATIC)
 *
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr GDALDefaultRasterAttributeTable::SetTableType(
    const GDALRATTableType eInTableType)
{
    eTableType = eInTableType;
    return CE_None;
}

/************************************************************************/
/*                            CreateColumn()                            */
/************************************************************************/

CPLErr
GDALDefaultRasterAttributeTable::CreateColumn(const char *pszFieldName,
                                              GDALRATFieldType eFieldType,
                                              GDALRATFieldUsage eFieldUsage)

{
    const size_t iNewField = aoFields.size();

    aoFields.resize(iNewField + 1);

    aoFields[iNewField].sName = pszFieldName;

    // color columns should be int 0..255
    if ((eFieldUsage == GFU_Red) || (eFieldUsage == GFU_Green) ||
        (eFieldUsage == GFU_Blue) || (eFieldUsage == GFU_Alpha))
    {
        eFieldType = GFT_Integer;
    }
    aoFields[iNewField].eType = eFieldType;
    aoFields[iNewField].eUsage = eFieldUsage;

    switch (eFieldType)
    {
        case GFT_Integer:
            aoFields[iNewField].anValues.resize(nRowCount);
            break;

        case GFT_Real:
            aoFields[iNewField].adfValues.resize(nRowCount);
            break;

        case GFT_String:
            aoFields[iNewField].aosValues.resize(nRowCount);
            break;

        case GFT_Boolean:
            aoFields[iNewField].abValues.resize(nRowCount);
            break;

        case GFT_DateTime:
            aoFields[iNewField].asDateTimeValues.resize(nRowCount);
            break;

        case GFT_WKBGeometry:
            aoFields[iNewField].aabyWKBGeometryValues.resize(nRowCount);
            break;
    }
    return CE_None;
}

/************************************************************************/
/*                          RemoveStatistics()                          */
/************************************************************************/

/**
 * \brief Remove Statistics from RAT
 *
 * Remove statistics (such as histogram) from the RAT. This is important
 * if these have been invalidated, for example by cropping the image.
 *
 * This method is the same as the C function GDALRATRemoveStatistics().
 *
 */

void GDALDefaultRasterAttributeTable::RemoveStatistics()

{
    // since we are storing the fields in a vector it will generally
    // be faster to create a new vector and replace the old one
    // rather than actually erasing columns.
    std::vector<GDALRasterAttributeField> aoNewFields;
    for (const auto &field : aoFields)
    {
        switch (field.eUsage)
        {
            case GFU_PixelCount:
            case GFU_Min:
            case GFU_Max:
            case GFU_RedMin:
            case GFU_GreenMin:
            case GFU_BlueMin:
            case GFU_AlphaMin:
            case GFU_RedMax:
            case GFU_GreenMax:
            case GFU_BlueMax:
            case GFU_AlphaMax:
            {
                break;
            }

            default:
                if (field.sName != "Histogram")
                {
                    aoNewFields.push_back(field);
                }
        }
    }
    aoFields = std::move(aoNewFields);
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

GDALDefaultRasterAttributeTable *GDALDefaultRasterAttributeTable::Clone() const

{
    return new GDALDefaultRasterAttributeTable(*this);
}

/************************************************************************/
/*                            GDALRATClone()                            */
/************************************************************************/

/**
 * \brief Copy Raster Attribute Table
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::Clone()
 */
GDALRasterAttributeTableH CPL_STDCALL
GDALRATClone(const GDALRasterAttributeTableH hRAT)

{
    VALIDATE_POINTER1(hRAT, "GDALRATClone", nullptr);

    return GDALRasterAttributeTable::FromHandle(hRAT)->Clone();
}

/************************************************************************/
/*                        GDALRATSerializeJSON()                        */
/************************************************************************/

/**
 * \brief Serialize Raster Attribute Table in Json format
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::SerializeJSON()
 */
void *CPL_STDCALL GDALRATSerializeJSON(GDALRasterAttributeTableH hRAT)

{
    VALIDATE_POINTER1(hRAT, "GDALRATSerializeJSON", nullptr);

    return GDALRasterAttributeTable::FromHandle(hRAT)->SerializeJSON();
}

/************************************************************************/
/*                      GDALRATRemoveStatistics()                       */
/************************************************************************/

/**
 * \brief Remove Statistics from RAT
 *
 * This function is the same as the C++ method
 * GDALRasterAttributeTable::RemoveStatistics()
 *
 */
void CPL_STDCALL GDALRATRemoveStatistics(GDALRasterAttributeTableH hRAT)

{
    VALIDATE_POINTER0(hRAT, "GDALRATRemoveStatistics");

    GDALRasterAttributeTable::FromHandle(hRAT)->RemoveStatistics();
}
