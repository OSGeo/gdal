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

#include "cpl_port.h"
#include "hfadataset.h"
#include "hfa_p.h"

#include <cassert>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "hfa.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

constexpr double D2R = M_PI / 180.0;

constexpr double ARCSEC2RAD = M_PI / 648000.0;

int WritePeStringIfNeeded( const OGRSpatialReference *poSRS, HFAHandle hHFA );
void ClearSR( HFAHandle hHFA );

/************************************************************************/
/*                     HFARasterAttributeTable()                        */
/************************************************************************/

HFARasterAttributeTable::HFARasterAttributeTable(
    HFARasterBand *poBand, const char *pszName) :
    hHFA(poBand->hHFA),
    poDT(poBand->hHFA->papoBand[poBand->nBand-1]->
         poNode->GetNamedChild(pszName)),
    osName(pszName),
    nBand(poBand->nBand),
    eAccess(poBand->GetAccess()),
    nRows(0),
    bLinearBinning(false),
    dfRow0Min(0.0),
    dfBinSize(0.0),
    eTableType(GRTT_THEMATIC)
{
    if( poDT != nullptr )
    {
        nRows = poDT->GetIntField("numRows");

        // Scan under table for columns.
        for( HFAEntry *poDTChild = poDT->GetChild();
             poDTChild != nullptr;
             poDTChild = poDTChild->GetNext() )
        {
            if( EQUAL(poDTChild->GetType(), "Edsc_BinFunction") )
            {
                const double dfMax = poDTChild->GetDoubleField("maxLimit");
                const double dfMin = poDTChild->GetDoubleField("minLimit");
                const int nBinCount = poDTChild->GetIntField("numBins");

                if( nBinCount == nRows && dfMax != dfMin && nBinCount > 1 )
                {
                    // Can't call SetLinearBinning since it will re-write
                    // which we might not have permission to do.
                    bLinearBinning = true;
                    dfRow0Min = dfMin;
                    dfBinSize = (dfMax - dfMin) / (nBinCount - 1);
                }
            }

            if( EQUAL(poDTChild->GetType(), "Edsc_BinFunction840") )
            {
                const char *pszValue =
                    poDTChild->GetStringField("binFunction.type.string");
                if( pszValue && EQUAL(pszValue, "BFUnique") )
                {
                    AddColumn("BinValues", GFT_Real, GFU_MinMax, 0, 0,
                              poDTChild, true);
                }
            }

            if( !EQUAL(poDTChild->GetType(), "Edsc_Column") )
                continue;

            const int nOffset = poDTChild->GetIntField("columnDataPtr");
            const char *pszType = poDTChild->GetStringField("dataType");
            GDALRATFieldUsage eUsage = GFU_Generic;
            bool bConvertColors = false;

            if( pszType == nullptr || nOffset == 0 )
                continue;

            GDALRATFieldType eType;
            if( EQUAL(pszType, "real") )
                eType = GFT_Real;
            else if( EQUAL(pszType, "string") )
                eType = GFT_String;
            else if( STARTS_WITH_CI(pszType, "int") )
                eType = GFT_Integer;
            else
                continue;

            if( EQUAL(poDTChild->GetName(), "Histogram") )
                eUsage = GFU_PixelCount;
            else if( EQUAL(poDTChild->GetName(), "Red") )
            {
                eUsage = GFU_Red;
                // Treat color columns as ints regardless
                // of how they are stored.
                bConvertColors = eType == GFT_Real;
                eType = GFT_Integer;
            }
            else if( EQUAL(poDTChild->GetName(), "Green") )
            {
                eUsage = GFU_Green;
                bConvertColors = eType == GFT_Real;
                eType = GFT_Integer;
            }
            else if( EQUAL(poDTChild->GetName(), "Blue") )
            {
                eUsage = GFU_Blue;
                bConvertColors = eType == GFT_Real;
                eType = GFT_Integer;
            }
            else if( EQUAL(poDTChild->GetName(), "Opacity") )
            {
                eUsage = GFU_Alpha;
                bConvertColors = eType == GFT_Real;
                eType = GFT_Integer;
            }
            else if( EQUAL(poDTChild->GetName(), "Class_Names") )
                eUsage = GFU_Name;

            if( eType == GFT_Real )
            {
                AddColumn(poDTChild->GetName(), GFT_Real, eUsage,
                          nOffset, sizeof(double), poDTChild);
            }
            else if( eType == GFT_String )
            {
                int nMaxNumChars = poDTChild->GetIntField("maxNumChars");
                if( nMaxNumChars <= 0 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid nMaxNumChars = %d for column %s",
                             nMaxNumChars, poDTChild->GetName());
                    nMaxNumChars = 1;
                }
                AddColumn(poDTChild->GetName(), GFT_String, eUsage,
                          nOffset, nMaxNumChars, poDTChild);
            }
            else if( eType == GFT_Integer )
            {
                int nSize = sizeof(GInt32);
                if( bConvertColors )
                    nSize = sizeof(double);
                AddColumn(poDTChild->GetName(), GFT_Integer,
                          eUsage, nOffset, nSize, poDTChild,
                          false, bConvertColors);
            }
        }
    }
}

/************************************************************************/
/*                    ~HFARasterAttributeTable()                        */
/************************************************************************/

HFARasterAttributeTable::~HFARasterAttributeTable() {}

/************************************************************************/
/*                              Clone()                                 */
/************************************************************************/

GDALRasterAttributeTable *HFARasterAttributeTable::Clone() const
{
    if( (GetRowCount() * GetColumnCount()) > RAT_MAX_ELEM_FOR_CLONE )
        return nullptr;

    GDALDefaultRasterAttributeTable *poRAT =
        new GDALDefaultRasterAttributeTable();

    for( int iCol = 0; iCol < static_cast<int>(aoFields.size()); iCol++)
    {
        poRAT->CreateColumn(aoFields[iCol].sName, aoFields[iCol].eType,
                            aoFields[iCol].eUsage);
        poRAT->SetRowCount(nRows);

        if( aoFields[iCol].eType == GFT_Integer )
        {
            int *panColData =
                static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nRows));
            if( panColData == nullptr )
            {
                delete poRAT;
                return nullptr;
            }

            if( ((GDALDefaultRasterAttributeTable *)this)
                    ->ValuesIO(GF_Read, iCol, 0, nRows, panColData) != CE_None )
            {
                CPLFree(panColData);
                delete poRAT;
                return nullptr;
            }

            for( int iRow = 0; iRow < nRows; iRow++ )
            {
                poRAT->SetValue(iRow, iCol, panColData[iRow]);
            }
            CPLFree(panColData);
        }
        if( aoFields[iCol].eType == GFT_Real )
        {
            double *padfColData = static_cast<double *>(
                VSI_MALLOC2_VERBOSE(sizeof(double), nRows));
            if( padfColData == nullptr )
            {
                delete poRAT;
                return nullptr;
            }

            if( ((GDALDefaultRasterAttributeTable *)this)
                    ->ValuesIO(GF_Read, iCol, 0, nRows, padfColData) != CE_None )
            {
                CPLFree(padfColData);
                delete poRAT;
                return nullptr;
            }

            for( int iRow = 0; iRow < nRows; iRow++ )
            {
                poRAT->SetValue(iRow, iCol, padfColData[iRow]);
            }
            CPLFree(padfColData);
        }
        if( aoFields[iCol].eType == GFT_String )
        {
            char **papszColData = static_cast<char **>(
                VSI_MALLOC2_VERBOSE(sizeof(char *), nRows));
            if( papszColData == nullptr )
            {
                delete poRAT;
                return nullptr;
            }

            if( ((GDALDefaultRasterAttributeTable *)this)
                    ->ValuesIO(GF_Read, iCol, 0, nRows, papszColData) != CE_None )
            {
                CPLFree(papszColData);
                delete poRAT;
                return nullptr;
            }

            for( int iRow = 0; iRow < nRows; iRow++ )
            {
                poRAT->SetValue(iRow, iCol, papszColData[iRow]);
                CPLFree(papszColData[iRow]);
            }
            CPLFree(papszColData);
        }
    }

    if( bLinearBinning )
        poRAT->SetLinearBinning(dfRow0Min, dfBinSize);

    poRAT->SetTableType(this->GetTableType());

    return poRAT;
}

/************************************************************************/
/*                          GetColumnCount()                            */
/************************************************************************/

int HFARasterAttributeTable::GetColumnCount() const
{
    return static_cast<int>(aoFields.size());
}

/************************************************************************/
/*                          GetNameOfCol()                              */
/************************************************************************/

const char *HFARasterAttributeTable::GetNameOfCol( int nCol ) const
{
    if( nCol < 0 || nCol >= static_cast<int>(aoFields.size()) )
        return nullptr;

    return aoFields[nCol].sName;
}

/************************************************************************/
/*                          GetUsageOfCol()                             */
/************************************************************************/

GDALRATFieldUsage HFARasterAttributeTable::GetUsageOfCol( int nCol ) const
{
    if( nCol < 0 || nCol >= static_cast<int>(aoFields.size()) )
        return GFU_Generic;

    return aoFields[nCol].eUsage;
}

/************************************************************************/
/*                          GetTypeOfCol()                              */
/************************************************************************/

GDALRATFieldType HFARasterAttributeTable::GetTypeOfCol( int nCol ) const
{
    if( nCol < 0 || nCol >= static_cast<int>(aoFields.size()) )
        return GFT_Integer;

    return aoFields[nCol].eType;
}

/************************************************************************/
/*                          GetColOfUsage()                             */
/************************************************************************/

int HFARasterAttributeTable::GetColOfUsage( GDALRATFieldUsage eUsage ) const
{
    for( unsigned int i = 0; i < aoFields.size(); i++ )
    {
        if( aoFields[i].eUsage == eUsage )
            return i;
    }

    return -1;
}
/************************************************************************/
/*                          GetRowCount()                               */
/************************************************************************/

int HFARasterAttributeTable::GetRowCount() const { return nRows; }

/************************************************************************/
/*                      GetValueAsString()                              */
/************************************************************************/

const char *HFARasterAttributeTable::GetValueAsString( int iRow,
                                                       int iField ) const
{
    // Get ValuesIO do do the work.
    char *apszStrList[1] = { nullptr };
    if( ((HFARasterAttributeTable *)this)
            ->ValuesIO(GF_Read, iField, iRow, 1, apszStrList) != CE_None )
    {
        return "";
    }

    ((HFARasterAttributeTable *)this)->osWorkingResult = apszStrList[0];
    CPLFree(apszStrList[0]);

    return osWorkingResult;
}

/************************************************************************/
/*                        GetValueAsInt()                               */
/************************************************************************/

int HFARasterAttributeTable::GetValueAsInt( int iRow, int iField ) const
{
    // Get ValuesIO do do the work.
    int nValue = 0;
    if( ((HFARasterAttributeTable *)this)
            ->ValuesIO(GF_Read, iField, iRow, 1, &nValue) != CE_None )
    {
        return 0;
    }

    return nValue;
}

/************************************************************************/
/*                      GetValueAsDouble()                              */
/************************************************************************/

double HFARasterAttributeTable::GetValueAsDouble( int iRow, int iField ) const
{
    // Get ValuesIO do do the work.
    double dfValue = 0.0;
    if( ((HFARasterAttributeTable *)this)
            ->ValuesIO(GF_Read, iField, iRow, 1, &dfValue) != CE_None )
    {
        return 0.0;
    }

    return dfValue;
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void HFARasterAttributeTable::SetValue( int iRow, int iField,
                                        const char *pszValue )
{
    // Get ValuesIO do do the work.
    ValuesIO(GF_Write, iField, iRow, 1, (char **)&pszValue);
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void HFARasterAttributeTable::SetValue( int iRow, int iField, double dfValue )
{
    // Get ValuesIO do do the work.
    ValuesIO(GF_Write, iField, iRow, 1, &dfValue);
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void HFARasterAttributeTable::SetValue( int iRow, int iField, int nValue )
{
    // Get ValuesIO do do the work.
    ValuesIO(GF_Write, iField, iRow, 1, &nValue);
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr HFARasterAttributeTable::ValuesIO( GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          double *pdfData )
{
    if( eRWFlag == GF_Write && eAccess == GA_ReadOnly )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    if( iField < 0 || iField >= static_cast<int>(aoFields.size()) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iField (%d) out of range.", iField);

        return CE_Failure;
    }

    if( iStartRow < 0 ||
        iLength >= INT_MAX - iStartRow ||
        (iStartRow + iLength) > nRows )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.",
                 iStartRow, iLength);

        return CE_Failure;
    }

    if( aoFields[iField].bConvertColors )
    {
        // Convert to/from float color field.
        int *panColData =
            static_cast<int *>(VSI_MALLOC2_VERBOSE(iLength, sizeof(int)));
        if( panColData == nullptr )
        {
            CPLFree(panColData);
            return CE_Failure;
        }

        if( eRWFlag == GF_Write )
        {
            for( int i = 0; i < iLength; i++ )
                panColData[i] = static_cast<int>(pdfData[i]);
        }

        const CPLErr ret =
            ColorsIO(eRWFlag, iField, iStartRow, iLength, panColData);

        if( eRWFlag == GF_Read )
        {
            // Copy them back to doubles.
            for( int i = 0; i < iLength; i++ )
                pdfData[i] = panColData[i];
        }

        CPLFree(panColData);
        return ret;
    }

    switch( aoFields[iField].eType )
    {
        case GFT_Integer:
        {
            // Allocate space for ints.
            int *panColData = static_cast<int *>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(int)));
            if( panColData == nullptr )
            {
                CPLFree(panColData);
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // Copy the application supplied doubles to ints.
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = static_cast<int>(pdfData[i]);
            }

            // Do the ValuesIO as ints.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData );
            if( eVal != CE_None )
            {
                CPLFree(panColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // Copy them back to doubles.
                for( int i = 0; i < iLength; i++ )
                    pdfData[i] = panColData[i];
            }

            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            if( (eRWFlag == GF_Read ) && aoFields[iField].bIsBinValues )
            {
                // Probably could change HFAReadBFUniqueBins to only read needed
                // rows.
                double *padfBinValues =
                    HFAReadBFUniqueBins(aoFields[iField].poColumn,
                                        iStartRow+iLength);
                if( padfBinValues == nullptr )
                    return CE_Failure;
                memcpy(pdfData, &padfBinValues[iStartRow],
                       sizeof(double) * iLength);
                CPLFree(padfBinValues);
            }
            else
            {
                if( VSIFSeekL(
                       hHFA->fp,
                       aoFields[iField].nDataOffset +
                       (static_cast<vsi_l_offset>(iStartRow) *
                        aoFields[iField].nElementSize), SEEK_SET ) != 0 )
                {
                    return CE_Failure;
                }

                if( eRWFlag == GF_Read )
                {
                    if( static_cast<int>(
                           VSIFReadL(pdfData, sizeof(double),
                                     iLength, hHFA->fp )) != iLength )
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "HFARasterAttributeTable::ValuesIO: "
                            "Cannot read values");
                        return CE_Failure;
                    }
#ifdef CPL_MSB
                    GDALSwapWords(pdfData, 8, iLength, 8);
#endif
                }
                else
                {
#ifdef CPL_MSB
                    GDALSwapWords(pdfData, 8, iLength, 8);
#endif
                    // Note: HFAAllocateSpace now called by CreateColumn so
                    // space should exist.
                    if( static_cast<int>(
                            VSIFWriteL(pdfData, sizeof(double), iLength,
                                       hHFA->fp)) != iLength )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "HFARasterAttributeTable::ValuesIO: "
                                 "Cannot write values");
                        return CE_Failure;
                    }
#ifdef CPL_MSB
                    // Swap back.
                    GDALSwapWords(pdfData, 8, iLength, 8);
#endif
                }
            }
        }
        break;
        case GFT_String:
        {
            // Allocate space for string pointers.
            char **papszColData = static_cast<char **>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(char*)));
            if( papszColData == nullptr )
            {
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // Copy the application supplied doubles to strings.
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf("%.16g", pdfData[i]);
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // Do the ValuesIO as strings.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData );
            if( eVal != CE_None )
            {
                if( eRWFlag == GF_Write )
                {
                    for( int i = 0; i < iLength; i++ )
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // Copy them back to doubles.
                for( int i = 0; i < iLength; i++ )
                    pdfData[i] = CPLAtof(papszColData[i]);
            }

            // Either we allocated them for write, or they were allocated
            // by ValuesIO on read.
            for( int i = 0; i < iLength; i++ )
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr HFARasterAttributeTable::ValuesIO( GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          int *pnData )
{
    if( eRWFlag == GF_Write && eAccess == GA_ReadOnly )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    if( iField < 0 || iField >= static_cast<int>(aoFields.size()) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iField (%d) out of range.", iField);

        return CE_Failure;
    }

    if( iStartRow < 0 ||
        iLength >= INT_MAX - iStartRow ||
        (iStartRow + iLength) > nRows )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.",
                 iStartRow, iLength);

        return CE_Failure;
    }

    if( aoFields[iField].bConvertColors )
    {
        // Convert to/from float color field.
        return ColorsIO(eRWFlag, iField, iStartRow, iLength, pnData);
    }

    switch( aoFields[iField].eType )
    {
        case GFT_Integer:
        {
            if( VSIFSeekL(hHFA->fp,
                          aoFields[iField].nDataOffset +
                          (static_cast<vsi_l_offset>(iStartRow) *
                           aoFields[iField].nElementSize),
                          SEEK_SET) != 0 )
            {
                return CE_Failure;
            }
            GInt32 *panColData = static_cast<GInt32 *>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(GInt32)));
            if( panColData == nullptr )
            {
                return CE_Failure;
            }

            if( eRWFlag == GF_Read )
            {
                if( static_cast<int>(
                        VSIFReadL(panColData, sizeof(GInt32), iLength,
                                  hHFA->fp)) != iLength )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "HFARasterAttributeTable::ValuesIO: "
                             "Cannot read values");
                    CPLFree(panColData);
                    return CE_Failure;
                }
#ifdef CPL_MSB
                GDALSwapWords(panColData, 4, iLength, 4);
#endif
                // Now copy into application buffer. This extra step
                // may not be necessary if sizeof(int) == sizeof(GInt32).
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = panColData[i];
            }
            else
            {
                // Copy from application buffer.
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = pnData[i];

#ifdef CPL_MSB
                GDALSwapWords(panColData, 4, iLength, 4);
#endif
                // Note: HFAAllocateSpace now called by CreateColumn so space
                // should exist.
                if( static_cast<int>(
                        VSIFWriteL(panColData, sizeof(GInt32), iLength,
                                   hHFA->fp)) != iLength )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "HFARasterAttributeTable::ValuesIO: "
                             "Cannot write values");
                    CPLFree(panColData);
                    return CE_Failure;
                }
            }
            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            // Allocate space for doubles.
            double *padfColData = static_cast<double *>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(double)));
            if( padfColData == nullptr )
            {
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // Copy the application supplied ints to doubles.
                for( int i = 0; i < iLength; i++ )
                    padfColData[i] = pnData[i];
            }

            // Do the ValuesIO as doubles.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData );
            if( eVal != CE_None )
            {
                CPLFree(padfColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // Copy them back to ints.
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = static_cast<int>(padfColData[i]);
            }

            CPLFree(padfColData);
        }
        break;
        case GFT_String:
        {
            // Allocate space for string pointers.
            char **papszColData = static_cast<char **>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(char*)));
            if( papszColData == nullptr )
            {
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // Copy the application supplied ints to strings.
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf("%d", pnData[i]);
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // Do the ValuesIO as strings.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData );
            if( eVal != CE_None )
            {
                if( eRWFlag == GF_Write )
                {
                    for( int i = 0; i < iLength; i++ )
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // Copy them back to ints.
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = atoi(papszColData[i]);
            }

            // Either we allocated them for write, or they were allocated
            // by ValuesIO on read.
            for( int i = 0; i < iLength; i++ )
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr HFARasterAttributeTable::ValuesIO( GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          char **papszStrList )
{
    if( eRWFlag == GF_Write && eAccess == GA_ReadOnly )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    if( iField < 0 || iField >= static_cast<int>(aoFields.size()) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iField (%d) out of range.", iField);

        return CE_Failure;
    }

    if( iStartRow < 0 ||
        iLength >= INT_MAX - iStartRow ||
        (iStartRow + iLength) > nRows )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.",
                 iStartRow, iLength);

        return CE_Failure;
    }

    if( aoFields[iField].bConvertColors )
    {
        // Convert to/from float color field.
        int *panColData = static_cast<int *>(
            VSI_MALLOC2_VERBOSE(iLength, sizeof(int)));
        if( panColData == nullptr )
        {
            CPLFree(panColData);
            return CE_Failure;
        }

        if( eRWFlag == GF_Write )
        {
            for( int i = 0; i < iLength; i++ )
                panColData[i] = atoi(papszStrList[i]);
        }

        const CPLErr ret =
            ColorsIO(eRWFlag, iField, iStartRow, iLength, panColData);

        if( eRWFlag == GF_Read )
        {
            // Copy them back to strings.
            for( int i = 0; i < iLength; i++ )
            {
                osWorkingResult.Printf("%d", panColData[i]);
                papszStrList[i] = CPLStrdup(osWorkingResult);
            }
        }

        CPLFree(panColData);
        return ret;
    }

    switch( aoFields[iField].eType )
    {
        case GFT_Integer:
        {
            // Allocate space for ints.
            int *panColData = static_cast<int *>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(int)));
            if( panColData == nullptr )
            {
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // Convert user supplied strings to ints.
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = atoi(papszStrList[i]);
            }

            // Call values IO to read/write ints.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData);
            if( eVal != CE_None )
            {
                CPLFree(panColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // Convert ints back to strings.
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf("%d", panColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            // Allocate space for doubles.
            double *padfColData = static_cast<double *>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(double)));
            if( padfColData == nullptr )
            {
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // Convert user supplied strings to doubles.
                for( int i = 0; i < iLength; i++ )
                    padfColData[i] = CPLAtof(papszStrList[i]);
            }

            // Call value IO to read/write doubles.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData);
            if( eVal != CE_None )
            {
                CPLFree(padfColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // Convert doubles back to strings.
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf("%.16g", padfColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(padfColData);
        }
        break;
        case GFT_String:
        {
            if( VSIFSeekL(hHFA->fp,
                          aoFields[iField].nDataOffset +
                          (static_cast<vsi_l_offset>(iStartRow) *
                           aoFields[iField].nElementSize), SEEK_SET ) != 0 )
            {
                return CE_Failure;
            }
            char *pachColData = static_cast<char *>(
                VSI_MALLOC2_VERBOSE(iLength, aoFields[iField].nElementSize));
            if( pachColData == nullptr )
            {
                return CE_Failure;
            }

            if( eRWFlag == GF_Read )
            {
                if( static_cast<int>(
                        VSIFReadL(pachColData, aoFields[iField].nElementSize,
                                  iLength, hHFA->fp)) != iLength )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "HFARasterAttributeTable::ValuesIO: "
                             "Cannot read values");
                    CPLFree(pachColData);
                    return CE_Failure;
                }

                // Now copy into application buffer.
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.assign(
                        pachColData+aoFields[iField].nElementSize * i,
                        aoFields[iField].nElementSize );
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            else
            {
                // We need to check that these strings will fit in the allocated
                // space.
                int nNewMaxChars = aoFields[iField].nElementSize;
                for( int i = 0; i < iLength; i++ )
                {
                    const int nStringSize =
                        static_cast<int>(strlen(papszStrList[i])) + 1;
                    if( nStringSize > nNewMaxChars )
                        nNewMaxChars = nStringSize;
                }

                if( nNewMaxChars > aoFields[iField].nElementSize )
                {
                    // OK we have a problem: The allocated space is not big
                    // enough we need to re-allocate the space and update the
                    // pointers and copy across the old data.
                    const int nNewOffset =
                        HFAAllocateSpace(hHFA->papoBand[nBand-1]->psInfo,
                                         nRows * nNewMaxChars);
                    char *pszBuffer = static_cast<char *>(
                        VSIMalloc2(aoFields[iField].nElementSize,
                                   sizeof(char)));
                    for( int i = 0; i < nRows; i++ )
                    {
                        // Seek to the old place.
                        CPL_IGNORE_RET_VAL(
                            VSIFSeekL(hHFA->fp,
                                      aoFields[iField].nDataOffset +
                                      (static_cast<vsi_l_offset>(i) *
                                       aoFields[iField].nElementSize),
                                      SEEK_SET ));
                        // Read in old data.
                        CPL_IGNORE_RET_VAL(
                            VSIFReadL(pszBuffer, aoFields[iField].nElementSize,
                                      1, hHFA->fp ));
                        // Seek to new place.
                        bool bOK =
                            VSIFSeekL(hHFA->fp,
                                      nNewOffset +
                                      (static_cast<vsi_l_offset>(i) *
                                       nNewMaxChars), SEEK_SET ) == 0;
                        // Write data to new place.
                        bOK &= VSIFWriteL(pszBuffer,
                                          aoFields[iField].nElementSize, 1,
                                          hHFA->fp) == 1;
                        // Make sure there is a terminating null byte just to be
                        // safe.
                        const char cNullByte = '\0';
                        bOK &= VSIFWriteL(&cNullByte, sizeof(char), 1,
                                          hHFA->fp) == 1;
                        if( !bOK )
                        {
                            CPLFree(pszBuffer);
                            CPLFree(pachColData);
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "HFARasterAttributeTable::ValuesIO: "
                                     "Cannot write values");
                            return CE_Failure;
                        }
                    }
                    // Update our data structures.
                    aoFields[iField].nElementSize = nNewMaxChars;
                    aoFields[iField].nDataOffset = nNewOffset;
                    // Update file.
                    aoFields[iField].poColumn->SetIntField("columnDataPtr",
                                                           nNewOffset);
                    aoFields[iField].poColumn->SetIntField("maxNumChars",
                                                           nNewMaxChars);

                    // Note: There isn't an HFAFreeSpace so we can't un-allocate
                    // the old space in the file.
                    CPLFree(pszBuffer);

                    // Re-allocate our buffer.
                    CPLFree(pachColData);
                    pachColData = static_cast<char *>(
                        VSI_MALLOC2_VERBOSE(iLength, nNewMaxChars));
                    if( pachColData == nullptr )
                    {
                        return CE_Failure;
                    }

                    // Lastly seek to the right place in the new space ready to
                    // write.
                    if( VSIFSeekL(hHFA->fp,
                                  nNewOffset +
                                  (static_cast<vsi_l_offset>(iStartRow) *
                                   nNewMaxChars), SEEK_SET) != 0 )
                    {
                        VSIFree(pachColData);
                        return CE_Failure;
                    }
                }

                // Copy from application buffer.
                for( int i = 0; i < iLength; i++ )
                    strcpy(&pachColData[nNewMaxChars*i], papszStrList[i]);

                // Note: HFAAllocateSpace now called by CreateColumn so space
                // should exist.
                if( static_cast<int>(
                        VSIFWriteL(pachColData, aoFields[iField].nElementSize,
                                   iLength, hHFA->fp)) != iLength )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "HFARasterAttributeTable::ValuesIO: "
                             "Cannot write values");
                    CPLFree(pachColData);
                    return CE_Failure;
                }
            }
            CPLFree(pachColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                               ColorsIO()                              */
/************************************************************************/

// Handle the fact that HFA stores colours as floats, but we need to
// read them in as ints 0...255.
CPLErr HFARasterAttributeTable::ColorsIO( GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          int *pnData )
{
    // Allocate space for doubles.
    double *padfData =
        static_cast<double *>(VSI_MALLOC2_VERBOSE(iLength, sizeof(double)));
    if( padfData == nullptr )
    {
        return CE_Failure;
    }

    if( eRWFlag == GF_Write )
    {
        // Copy the application supplied ints to doubles
        // and convert 0..255 to 0..1 in the same manner
        // as the color table.
        for( int i = 0; i < iLength; i++ )
            padfData[i] = pnData[i] / 255.0;
    }

    if( VSIFSeekL(hHFA->fp,
                  aoFields[iField].nDataOffset +
                  (static_cast<vsi_l_offset>(iStartRow) *
                   aoFields[iField].nElementSize), SEEK_SET) != 0 )
    {
        CPLFree(padfData);
        return CE_Failure;
    }

    if( eRWFlag == GF_Read )
    {
        if( static_cast<int>(VSIFReadL(padfData, sizeof(double), iLength,
                                       hHFA->fp)) != iLength )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "HFARasterAttributeTable::ColorsIO: Cannot read values");
            CPLFree(padfData);
            return CE_Failure;
        }
#ifdef CPL_MSB
        GDALSwapWords(padfData, 8, iLength, 8);
#endif
    }
    else
    {
#ifdef CPL_MSB
        GDALSwapWords(padfData, 8, iLength, 8);
#endif
        // Note: HFAAllocateSpace now called by CreateColumn so space should
        // exist.
        if( static_cast<int>(
                VSIFWriteL(padfData, sizeof(double), iLength,
                           hHFA->fp)) != iLength )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "HFARasterAttributeTable::ColorsIO: Cannot write values");
            CPLFree(padfData);
            return CE_Failure;
        }
    }

    if( eRWFlag == GF_Read )
    {
        // Copy them back to ints converting 0..1 to 0..255 in
        // the same manner as the color table.
        // TODO(schwehr): Symbolic constants for 255 and 256.
        for( int i = 0; i < iLength; i++ )
            pnData[i] = std::min(255, static_cast<int>(padfData[i] * 256));
    }

    CPLFree(padfData);

    return CE_None;
}

/************************************************************************/
/*                       ChangesAreWrittenToFile()                      */
/************************************************************************/

int HFARasterAttributeTable::ChangesAreWrittenToFile() { return TRUE; }

/************************************************************************/
/*                          SetRowCount()                               */
/************************************************************************/

void HFARasterAttributeTable::SetRowCount( int iCount )
{
    if( eAccess == GA_ReadOnly )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return;
    }

    if( iCount > nRows )
    {
        // Making the RAT larger - a bit hard.
        // We need to re-allocate space on disc.
        for( int iCol = 0;
             iCol < static_cast<int>(aoFields.size());
             iCol++ )
        {
            // New space.
            const int nNewOffset =
                HFAAllocateSpace(hHFA->papoBand[nBand - 1]->psInfo,
                                 iCount * aoFields[iCol].nElementSize);

            // Only need to bother if there are actually rows.
            if( nRows > 0 )
            {
                // Temp buffer for this column.
                void *pData =
                    VSI_MALLOC2_VERBOSE(nRows, aoFields[iCol].nElementSize);
                if( pData == nullptr )
                {
                    return;
                }
                // Read old data.
                if( VSIFSeekL(hHFA->fp, aoFields[iCol].nDataOffset,
                              SEEK_SET) != 0 ||
                    static_cast<int>(
                        VSIFReadL(pData, aoFields[iCol].nElementSize,
                                  nRows, hHFA->fp)) != nRows )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "HFARasterAttributeTable::SetRowCount: "
                             "Cannot read values");
                    CPLFree(pData);
                    return;
                }

                // Write data - new space will be uninitialised.
                if( VSIFSeekL(hHFA->fp, nNewOffset, SEEK_SET ) != 0 ||
                    static_cast<int>(
                        VSIFWriteL(pData, aoFields[iCol].nElementSize,
                                   nRows, hHFA->fp)) != nRows )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "HFARasterAttributeTable::SetRowCount: "
                             "Cannot write values");
                    CPLFree(pData);
                    return;
                }
                CPLFree(pData);
            }

            // Update our data structures.
            aoFields[iCol].nDataOffset = nNewOffset;
            // Update file.
            aoFields[iCol].poColumn->SetIntField("columnDataPtr", nNewOffset);
            aoFields[iCol].poColumn->SetIntField("numRows", iCount);
        }
    }
    else if( iCount < nRows )
    {
        // Update the numRows.
        for( int iCol = 0;
             iCol < static_cast<int>(aoFields.size());
             iCol++ )
        {
            aoFields[iCol].poColumn->SetIntField("numRows", iCount);
        }
    }

    nRows = iCount;

    if( poDT != nullptr && EQUAL(poDT->GetType(), "Edsc_Table") )
    {
        poDT->SetIntField("numrows", iCount);
    }
}

/************************************************************************/
/*                          GetRowOfValue()                             */
/************************************************************************/
 int HFARasterAttributeTable::GetRowOfValue( double dfValue ) const
{
    // Handle case of regular binning.
    if( bLinearBinning )
    {
        const int iBin =
            static_cast<int>(floor((dfValue - dfRow0Min) / dfBinSize));
        if( iBin < 0 || iBin >= nRows )
            return -1;
         return iBin;
    }
     // Do we have any information?
    int nMinCol = GetColOfUsage(GFU_Min);
    if( nMinCol == -1 )
        nMinCol = GetColOfUsage(GFU_MinMax);
     int nMaxCol = GetColOfUsage(GFU_Max);
    if( nMaxCol == -1 )
        nMaxCol = GetColOfUsage(GFU_MinMax);
     if( nMinCol == -1 && nMaxCol == -1 )
        return -1;
     // Search through rows for match.
    for( int iRow = 0; iRow < nRows; iRow++ )
    {
        if( nMinCol != -1 )
        {
            while( iRow < nRows &&
                   dfValue < GetValueAsDouble(iRow, nMinCol) )
                iRow++;
             if( iRow == nRows )
                break;
        }
         if( nMaxCol != -1 )
        {
            if( dfValue > GetValueAsDouble(iRow, nMaxCol) )
                continue;
        }
         return iRow;
    }
     return -1;
}
 /************************************************************************/
/*                          GetRowOfValue()                             */
/*                                                                      */
/*      Int arg for now just converted to double.  Perhaps we will      */
/*      handle this in a special way some day?                          */
/************************************************************************/
 int HFARasterAttributeTable::GetRowOfValue( int nValue ) const
{
    return GetRowOfValue(static_cast<double>(nValue));
}

/************************************************************************/
/*                          CreateColumn()                              */
/************************************************************************/

CPLErr HFARasterAttributeTable::CreateColumn( const char *pszFieldName,
                                GDALRATFieldType eFieldType,
                                GDALRATFieldUsage eFieldUsage )
{
    if( eAccess == GA_ReadOnly )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    // Do we have a descriptor table already?
    if( poDT == nullptr || !EQUAL(poDT->GetType(), "Edsc_Table") )
        CreateDT();

    bool bConvertColors = false;

    // Imagine doesn't have a concept of usage - works of the names instead.
    // Must make sure name matches use.
    if( eFieldUsage == GFU_Red )
    {
        pszFieldName = "Red";
        // Create a real column in the file, but make it
        // available as int to GDAL.
        bConvertColors = true;
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_Green )
    {
        pszFieldName = "Green";
        bConvertColors = true;
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_Blue )
    {
        pszFieldName = "Blue";
        bConvertColors = true;
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_Alpha )
    {
        pszFieldName = "Opacity";
        bConvertColors = true;
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_PixelCount )
    {
        pszFieldName = "Histogram";
        // Histogram is always float in HFA.
        eFieldType = GFT_Real;
    }
    else if( eFieldUsage == GFU_Name )
    {
        pszFieldName = "Class_Names";
    }

    // Check to see if a column with pszFieldName exists and create it
    // if necessary.
    HFAEntry *poColumn = poDT->GetNamedChild(pszFieldName);

    if( poColumn == nullptr || !EQUAL(poColumn->GetType(), "Edsc_Column") )
        poColumn = HFAEntry::New(hHFA->papoBand[nBand - 1]->psInfo,
                                 pszFieldName, "Edsc_Column", poDT);

    poColumn->SetIntField("numRows", nRows);
    int nElementSize = 0;

    if( eFieldType == GFT_Integer )
    {
        nElementSize = sizeof(GInt32);
        poColumn->SetStringField("dataType", "integer");
    }
    else if( eFieldType == GFT_Real )
    {
        nElementSize = sizeof(double);
        poColumn->SetStringField("dataType", "real");
    }
    else if( eFieldType == GFT_String )
    {
        // Just have to guess here since we don't have any strings to check.
        nElementSize = 10;
        poColumn->SetStringField("dataType", "string");
        poColumn->SetIntField("maxNumChars", nElementSize);
    }
    else
    {
        // Cannot deal with any of the others yet.
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Writing this data type in a column is not supported "
                 "for this Raster Attribute Table.");
        return CE_Failure;
    }

    const int nOffset =
        HFAAllocateSpace(hHFA->papoBand[nBand - 1]->psInfo,
                         nRows * nElementSize);
    poColumn->SetIntField("columnDataPtr", nOffset);

    if( bConvertColors )
    {
        // GDAL Int column
        eFieldType = GFT_Integer;
    }

    AddColumn(pszFieldName, eFieldType, eFieldUsage,
              nOffset, nElementSize, poColumn, false, bConvertColors);

    return CE_None;
}

/************************************************************************/
/*                          SetLinearBinning()                          */
/************************************************************************/

CPLErr HFARasterAttributeTable::SetLinearBinning(
    double dfRow0MinIn, double dfBinSizeIn )
{
    if( eAccess == GA_ReadOnly )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    bLinearBinning = true;
    dfRow0Min = dfRow0MinIn;
    dfBinSize = dfBinSizeIn;

    // Do we have a descriptor table already?
    if( poDT == nullptr || !EQUAL(poDT->GetType(), "Edsc_Table") )
        CreateDT();

    // We should have an Edsc_BinFunction.
    HFAEntry *poBinFunction = poDT->GetNamedChild("#Bin_Function#");
    if( poBinFunction == nullptr ||
        !EQUAL(poBinFunction->GetType(), "Edsc_BinFunction") )
    {
        poBinFunction = HFAEntry::New(hHFA->papoBand[nBand - 1]->psInfo,
                                      "#Bin_Function#", "Edsc_BinFunction",
                                      poDT);
    }

    // Because of the BaseData we have to hardcode the size.
    poBinFunction->MakeData(30);

    poBinFunction->SetStringField("binFunction", "direct");
    poBinFunction->SetDoubleField("minLimit", dfRow0Min);
    poBinFunction->SetDoubleField(
        "maxLimit", (nRows - 1) * dfBinSize + dfRow0Min);
    poBinFunction->SetIntField("numBins", nRows);

    return CE_None;
}

/************************************************************************/
/*                          GetLinearBinning()                          */
/************************************************************************/

int HFARasterAttributeTable::GetLinearBinning( double *pdfRow0Min,
                                               double *pdfBinSize ) const
{
    if( !bLinearBinning )
        return FALSE;

    *pdfRow0Min = dfRow0Min;
    *pdfBinSize = dfBinSize;

    return TRUE;
}

/************************************************************************/
/*                              Serialize()                             */
/************************************************************************/

CPLXMLNode *HFARasterAttributeTable::Serialize() const
{
    if( GetRowCount() != 0 &&
        GetColumnCount() > RAT_MAX_ELEM_FOR_CLONE / GetRowCount() )
        return nullptr;

    return GDALRasterAttributeTable::Serialize();
}

/************************************************************************/
/*                              SetTableType()                             */
/************************************************************************/

CPLErr HFARasterAttributeTable::SetTableType(const GDALRATTableType eInTableType)
{
    eTableType = eInTableType;
    return CE_None;
}

/************************************************************************/
/*                              GetTableType()                             */
/************************************************************************/

GDALRATTableType HFARasterAttributeTable::GetTableType() const
{
    return eTableType;
}

void HFARasterAttributeTable::RemoveStatistics()
{
    // since we are storing the fields in a vector it will generally
    // be faster to create a new vector and replace the old one
    // rather than actually erasing columns.
    std::vector<HFAAttributeField> aoNewFields;
    for ( const auto& field : aoFields )
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
    aoFields = aoNewFields;
}

/************************************************************************/
/*                           HFARasterBand()                            */
/************************************************************************/

namespace {

// Convert 0..1 input color range to 0..255.
// Clamp overflow and underflow.
short ColorToShort(double val)
{
    const double dfScaled = val * 256.0;
    // Clamp to [0..255].
    const double dfClamped = std::max(0.0, std::min(255.0, dfScaled));
    return static_cast<short>(dfClamped);
}

}  // namespace

HFARasterBand::HFARasterBand( HFADataset *poDSIn, int nBandIn, int iOverview ) :
    poCT(nullptr),
    // eHFADataType
    nOverviews(-1),
    nThisOverview(iOverview),
    papoOverviewBands(nullptr),
    hHFA(poDSIn->hHFA),
    bMetadataDirty(false),
    poDefaultRAT(nullptr)
{
    if( iOverview == -1 )
        poDS = poDSIn;
    else
        poDS = nullptr;

    nBand = nBandIn;
    eAccess = poDSIn->GetAccess();

    int nCompression = 0;
    HFAGetBandInfo(hHFA, nBand, &eHFADataType,
                   &nBlockXSize, &nBlockYSize, &nCompression);

    // If this is an overview, we need to fetch the actual size,
    // and block size.
    if( iOverview > -1 )
    {
        EPTType eHFADataTypeO;

        nOverviews = 0;
        if( HFAGetOverviewInfo(
               hHFA, nBand, iOverview,
               &nRasterXSize, &nRasterYSize,
               &nBlockXSize, &nBlockYSize, &eHFADataTypeO ) != CE_None )
        {
            nRasterXSize = 0;
            nRasterYSize = 0;
            return;
        }

        // If we are an 8bit overview of a 1bit layer, we need to mark
        // ourselves as being "resample: average_bit2grayscale".
        if( eHFADataType == EPT_u1 && eHFADataTypeO == EPT_u8 )
        {
            GDALMajorObject::SetMetadataItem("RESAMPLING",
                                             "AVERAGE_BIT2GRAYSCALE");
            GDALMajorObject::SetMetadataItem("NBITS", "8");
        }
        eHFADataType = eHFADataTypeO;
    }

    // Set some other information.
    if( nCompression != 0 )
        GDALMajorObject::SetMetadataItem("COMPRESSION", "RLE",
                                         "IMAGE_STRUCTURE");

    switch( eHFADataType )
    {
    case EPT_u1:
    case EPT_u2:
    case EPT_u4:
    case EPT_u8:
    case EPT_s8:
        eDataType = GDT_Byte;
        break;

    case EPT_u16:
        eDataType = GDT_UInt16;
        break;

    case EPT_s16:
        eDataType = GDT_Int16;
        break;

    case EPT_u32:
        eDataType = GDT_UInt32;
        break;

    case EPT_s32:
        eDataType = GDT_Int32;
        break;

    case EPT_f32:
        eDataType = GDT_Float32;
        break;

    case EPT_f64:
        eDataType = GDT_Float64;
        break;

    case EPT_c64:
        eDataType = GDT_CFloat32;
        break;

    case EPT_c128:
        eDataType = GDT_CFloat64;
        break;

    default:
        eDataType = GDT_Byte;
        // This should really report an error, but this isn't
        // so easy from within constructors.
        CPLDebug("GDAL", "Unsupported pixel type in HFARasterBand: %d.",
                 eHFADataType);
        break;
    }

    if( HFAGetDataTypeBits(eHFADataType) < 8 )
    {
        GDALMajorObject::SetMetadataItem(
            "NBITS",
            CPLString().Printf("%d", HFAGetDataTypeBits(eHFADataType)),
            "IMAGE_STRUCTURE");
    }

    if( eHFADataType == EPT_s8 )
    {
        GDALMajorObject::SetMetadataItem("PIXELTYPE", "SIGNEDBYTE",
                                         "IMAGE_STRUCTURE");
    }

    // Collect color table if present.
    double *padfRed = nullptr;
    double *padfGreen = nullptr;
    double *padfBlue = nullptr;
    double *padfAlpha = nullptr;
    double *padfBins = nullptr;
    int nColors = 0;

    if( iOverview == -1 &&
        HFAGetPCT(hHFA, nBand, &nColors,
                     &padfRed, &padfGreen, &padfBlue, &padfAlpha,
                     &padfBins) == CE_None &&
        nColors > 0 )
    {
        poCT = new GDALColorTable();
        for( int iColor = 0; iColor < nColors; iColor++ )
        {
            // The following mapping assigns "equal sized" section of
            // the [0...1] range to each possible output value and avoid
            // rounding issues for the "normal" values generated using n/255.
            // See bug #1732 for some discussion.
            GDALColorEntry sEntry = {
                ColorToShort(padfRed[iColor]),
                ColorToShort(padfGreen[iColor]),
                ColorToShort(padfBlue[iColor]),
                ColorToShort(padfAlpha[iColor])
            };

            if( padfBins != nullptr )
            {
                const double dfIdx = padfBins[iColor];
                if( !(dfIdx >= 0.0 && dfIdx <= 65535.0) )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Invalid index padfBins[%d] = %g",
                             iColor, dfIdx);
                    break;
                }
                else
                {
                    poCT->SetColorEntry(static_cast<int>(dfIdx),
                                        &sEntry);
                }
            }
            else
            {
                poCT->SetColorEntry(iColor, &sEntry);
            }
        }
    }
}

/************************************************************************/
/*                           ~HFARasterBand()                           */
/************************************************************************/

HFARasterBand::~HFARasterBand()

{
    FlushCache(true);

    for( int iOvIndex = 0; iOvIndex < nOverviews; iOvIndex++ )
    {
        delete papoOverviewBands[iOvIndex];
    }
    CPLFree(papoOverviewBands);

    if( poCT != nullptr )
        delete poCT;

    if( poDefaultRAT )
        delete poDefaultRAT;
}

/************************************************************************/
/*                          ReadAuxMetadata()                           */
/************************************************************************/

void HFARasterBand::ReadAuxMetadata()

{
    // Only load metadata for full resolution layer.
    if( nThisOverview != -1 )
        return;

    HFABand *poBand = hHFA->papoBand[nBand-1];

    const char *const *pszAuxMetaData = GetHFAAuxMetaDataList();
    for( int i = 0; pszAuxMetaData[i] != nullptr; i += 4 )
    {
        HFAEntry *poEntry;
        if (strlen(pszAuxMetaData[i]) > 0)
        {
            poEntry = poBand->poNode->GetNamedChild(pszAuxMetaData[i]);
            if( poEntry == nullptr )
                continue;
        }
        else
        {
            poEntry = poBand->poNode;
            assert(poEntry);
        }

        const char *pszFieldName = pszAuxMetaData[i + 1] + 1;

        switch( pszAuxMetaData[i + 1][0] )
        {
          case 'd':
          {
              CPLString osValueList;

              CPLErr eErr = CE_None;
              int nCount = poEntry->GetFieldCount(pszFieldName, &eErr);
              if( nCount > 65536 )
              {
                  nCount = 65536;
                  CPLDebug("HFA", "Limiting %s to %d entries",
                           pszAuxMetaData[i + 2], nCount);
              }
              for( int iValue = 0;
                   eErr == CE_None && iValue < nCount;
                   iValue++ )
              {
                  CPLString osSubFieldName;
                  osSubFieldName.Printf("%s[%d]", pszFieldName, iValue);
                  const double dfValue =
                      poEntry->GetDoubleField(osSubFieldName, &eErr);
                  if( eErr != CE_None )
                      break;

                  char szValueAsString[100] = {};
                  CPLsnprintf(szValueAsString, sizeof(szValueAsString),
                              "%.14g", dfValue);

                  if( iValue > 0 )
                      osValueList += ",";
                  osValueList += szValueAsString;
              }
              if( eErr == CE_None )
                  SetMetadataItem(pszAuxMetaData[i + 2], osValueList);
          }
          break;
          case 'i':
          case 'l':
          {
              CPLString osValueList;

              CPLErr eErr = CE_None;
              int nCount = poEntry->GetFieldCount(pszFieldName, &eErr);
              if( nCount > 65536 )
              {
                  nCount = 65536;
                  CPLDebug("HFA", "Limiting %s to %d entries",
                           pszAuxMetaData[i + 2], nCount);
              }
              for( int iValue = 0;
                   eErr == CE_None && iValue < nCount;
                   iValue++ )
              {
                  CPLString osSubFieldName;
                  osSubFieldName.Printf("%s[%d]", pszFieldName, iValue);
                  int nValue = poEntry->GetIntField(osSubFieldName, &eErr);
                  if( eErr != CE_None )
                      break;

                  char szValueAsString[100] = {};
                  snprintf(szValueAsString, sizeof(szValueAsString),
                           "%d", nValue);

                  if( iValue > 0 )
                      osValueList += ",";
                  osValueList += szValueAsString;
              }
              if( eErr == CE_None )
                  SetMetadataItem(pszAuxMetaData[i + 2], osValueList);
          }
          break;
          case 's':
          case 'e':
          {
              CPLErr eErr = CE_None;
              const char *pszValue =
                  poEntry->GetStringField(pszFieldName, &eErr);
              if( eErr == CE_None )
                  SetMetadataItem(pszAuxMetaData[i + 2], pszValue);
          }
          break;
          default:
            CPLAssert(false);
        }
    }

    /* if we have a default RAT we can now set its thematic/athematic state
       from the metadata we just read in */
    if ( GetDefaultRAT() )
    {
        const char * psLayerType = GetMetadataItem( "LAYER_TYPE","" );
        if (psLayerType)
        {
            GetDefaultRAT()->SetTableType(EQUALN(psLayerType,"athematic",9)?GRTT_ATHEMATIC:GRTT_THEMATIC);
        }
    }
}

/************************************************************************/
/*                       ReadHistogramMetadata()                        */
/************************************************************************/

void HFARasterBand::ReadHistogramMetadata()

{
    // Only load metadata for full resolution layer.
    if( nThisOverview != -1 )
        return;

    HFABand *poBand = hHFA->papoBand[nBand-1];

    HFAEntry *poEntry =
        poBand->poNode->GetNamedChild("Descriptor_Table.Histogram");
    if( poEntry == nullptr )
        return;

    int nNumBins = poEntry->GetIntField("numRows");
    if( nNumBins < 0 )
        return;
    // TODO(schwehr): Can we do a better/tighter check?
    if( nNumBins > 1000000 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Unreasonably large histogram: %d", nNumBins);
        return;
    }

    // Fetch the histogram values.
    const int nOffset = poEntry->GetIntField("columnDataPtr");
    const char *pszType = poEntry->GetStringField("dataType");
    int nBinSize = 4;

    if( pszType != nullptr && STARTS_WITH_CI(pszType, "real") )
        nBinSize = 8;

    GUIntBig *panHistValues = static_cast<GUIntBig *>(
        VSI_MALLOC2_VERBOSE(sizeof(GUIntBig), nNumBins));
    GByte *pabyWorkBuf =
        static_cast<GByte *>(VSI_MALLOC2_VERBOSE(nBinSize, nNumBins));

    if( panHistValues == nullptr || pabyWorkBuf == nullptr )
    {
        VSIFree(panHistValues);
        VSIFree(pabyWorkBuf);
        return;
    }

    if( VSIFSeekL(hHFA->fp, nOffset, SEEK_SET) != 0 ||
        static_cast<int>(VSIFReadL(pabyWorkBuf, nBinSize, nNumBins,
                                   hHFA->fp)) != nNumBins)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot read histogram values.");
        CPLFree(panHistValues);
        CPLFree(pabyWorkBuf);
        return;
    }

    // Swap into local order.
    for( int i = 0; i < nNumBins; i++ )
        HFAStandard(nBinSize, pabyWorkBuf + i * nBinSize);

    if( nBinSize == 8 )  // Source is doubles.
    {
        const double *padfWorkBuf = reinterpret_cast<double *>(pabyWorkBuf);
        for( int i = 0; i < nNumBins; i++ )
        {
            const double dfNumber = padfWorkBuf[i];
            if( dfNumber >= static_cast<double>(
                                std::numeric_limits<GUIntBig>::max()) ||
                dfNumber < static_cast<double>(
                                std::numeric_limits<GUIntBig>::min()) ||
                CPLIsNan(dfNumber) )
            {
                CPLError(CE_Failure, CPLE_FileIO, "Out of range hist vals.");
                CPLFree(panHistValues);
                CPLFree(pabyWorkBuf);
                return;
            }
            panHistValues[i] = static_cast<GUIntBig>(dfNumber);
        }
    }
    else  // Source is 32bit integers.
    {
        const int *panWorkBuf = reinterpret_cast<int *>(pabyWorkBuf);
        for( int i = 0; i < nNumBins; i++ )
        {
            const int nNumber = panWorkBuf[i];
            // Positive int should always fit.
            if( nNumber < 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO, "Out of range hist vals.");
                CPLFree(panHistValues);
                CPLFree(pabyWorkBuf);
                return;
            }
            panHistValues[i] = static_cast<GUIntBig>(nNumber);
        }
    }

    CPLFree(pabyWorkBuf);
    pabyWorkBuf = nullptr;

    // Do we have unique values for the bins?
    double *padfBinValues = nullptr;
    HFAEntry *poBinEntry =
        poBand->poNode->GetNamedChild("Descriptor_Table.#Bin_Function840#");

    if( poBinEntry != nullptr &&
        EQUAL(poBinEntry->GetType(), "Edsc_BinFunction840") )
    {
        const char *pszValue =
            poBinEntry->GetStringField("binFunction.type.string");
        if( pszValue && EQUAL(pszValue, "BFUnique") )
            padfBinValues = HFAReadBFUniqueBins(poBinEntry, nNumBins);
    }

    if( padfBinValues )
    {
        int nMaxValue = 0;
        int nMinValue = 1000000;

        for( int i = 0; i < nNumBins; i++ )
        {
            const double dfCurrent = padfBinValues[i];

            if( dfCurrent != floor(dfCurrent) || /* not an integer value */
                dfCurrent < 0.0 || dfCurrent > 1000.0 )
            {
                CPLFree(padfBinValues);
                CPLFree(panHistValues);
                CPLDebug(
                    "HFA", "Unable to offer histogram because unique values "
                    "list is not convenient to reform as HISTOBINVALUES.");
                return;
            }

            nMaxValue = std::max(nMaxValue, static_cast<int>(dfCurrent));
            nMinValue = std::min(nMinValue, static_cast<int>(dfCurrent));
        }

        const int nNewBins = nMaxValue + 1;
        GUIntBig *panNewHistValues =
            static_cast<GUIntBig *>(CPLCalloc(sizeof(GUIntBig), nNewBins));

        for( int i = 0; i < nNumBins; i++ )
            panNewHistValues[static_cast<int>(padfBinValues[i])] =
                panHistValues[i];

        CPLFree(panHistValues);
        panHistValues = panNewHistValues;
        nNumBins = nNewBins;

        SetMetadataItem("STATISTICS_HISTOMIN", "0");
        SetMetadataItem("STATISTICS_HISTOMAX",
                        CPLString().Printf("%d", nMaxValue));
        SetMetadataItem("STATISTICS_HISTONUMBINS",
                        CPLString().Printf("%d", nMaxValue + 1));

        CPLFree(padfBinValues);
        padfBinValues = nullptr;
    }

    // Format into HISTOBINVALUES text format.
    unsigned int nBufSize = 1024;
    char *pszBinValues = static_cast<char *>(CPLMalloc(nBufSize));
    pszBinValues[0] = 0;
    int nBinValuesLen = 0;

    for( int nBin = 0; nBin < nNumBins; ++nBin )
    {
        char szBuf[32] = {};
        snprintf(szBuf, 31, CPL_FRMT_GUIB, panHistValues[nBin]);
        if( (nBinValuesLen + strlen(szBuf) + 2) > nBufSize )
        {
            nBufSize *= 2;
            char *pszNewBinValues = static_cast<char *>(
                VSI_REALLOC_VERBOSE(pszBinValues, nBufSize));
            if( pszNewBinValues == nullptr )
            {
                break;
            }
            pszBinValues = pszNewBinValues;
        }
        strcat(pszBinValues + nBinValuesLen, szBuf);
        strcat(pszBinValues + nBinValuesLen, "|");
        nBinValuesLen += static_cast<int>(strlen(pszBinValues + nBinValuesLen));
    }

    SetMetadataItem("STATISTICS_HISTOBINVALUES", pszBinValues);
    CPLFree(panHistValues);
    CPLFree(pszBinValues);
}

/************************************************************************/
/*                             GetNoDataValue()                         */
/************************************************************************/

double HFARasterBand::GetNoDataValue( int *pbSuccess )

{
    double dfNoData = 0.0;

    if( HFAGetBandNoData(hHFA, nBand, &dfNoData) )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return dfNoData;
    }

    return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                             SetNoDataValue()                         */
/************************************************************************/

CPLErr HFARasterBand::SetNoDataValue( double dfValue )
{
    return HFASetBandNoData(hHFA, nBand, dfValue);
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double HFARasterBand::GetMinimum( int *pbSuccess )

{
    const char *pszValue = GetMetadataItem("STATISTICS_MINIMUM");

    if( pszValue != nullptr )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return CPLAtofM(pszValue);
    }

    return GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double HFARasterBand::GetMaximum( int *pbSuccess )

{
    const char *pszValue = GetMetadataItem("STATISTICS_MAXIMUM");

    if( pszValue != nullptr )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return CPLAtofM(pszValue);
    }

    return GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                         EstablishOverviews()                         */
/*                                                                      */
/*      Delayed population of overview information.                     */
/************************************************************************/

void HFARasterBand::EstablishOverviews()

{
    if( nOverviews != -1 )
        return;

    nOverviews = HFAGetOverviewCount(hHFA, nBand);
    if( nOverviews > 0 )
    {
        papoOverviewBands = static_cast<HFARasterBand **>(
            CPLMalloc(sizeof(void*) * nOverviews));

        for( int iOvIndex = 0; iOvIndex < nOverviews; iOvIndex++ )
        {
            papoOverviewBands[iOvIndex] =
                new HFARasterBand((HFADataset *) poDS, nBand, iOvIndex);
            if( papoOverviewBands[iOvIndex]->GetXSize() == 0 )
            {
                delete papoOverviewBands[iOvIndex];
                papoOverviewBands[iOvIndex] = nullptr;
            }
        }
    }
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int HFARasterBand::GetOverviewCount()

{
    EstablishOverviews();

    if( nOverviews == 0 )
        return GDALRasterBand::GetOverviewCount();

    return nOverviews;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *HFARasterBand::GetOverview( int i )

{
    EstablishOverviews();

    if( nOverviews == 0 )
        return GDALRasterBand::GetOverview(i);
    else if( i < 0 || i >= nOverviews )
        return nullptr;
    else
        return papoOverviewBands[i];
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HFARasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void *pImage )

{
    CPLErr eErr = CE_None;

    if( nThisOverview == -1 )
        eErr = HFAGetRasterBlockEx(
            hHFA, nBand, nBlockXOff, nBlockYOff,
            pImage,
            nBlockXSize * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType) );
    else
        eErr = HFAGetOverviewRasterBlockEx(
            hHFA, nBand, nThisOverview,
            nBlockXOff, nBlockYOff,
            pImage,
            nBlockXSize * nBlockYSize * GDALGetDataTypeSizeBytes(eDataType) );

    if( eErr == CE_None && eHFADataType == EPT_u4 )
    {
        GByte *pabyData = static_cast<GByte *>(pImage);

        for( int ii = nBlockXSize * nBlockYSize - 2; ii >= 0; ii -= 2 )
        {
            int k = ii >> 1;
            pabyData[ii + 1] = (pabyData[k] >> 4) & 0xf;
            pabyData[ii] = (pabyData[k]) & 0xf;
        }
    }
    if( eErr == CE_None && eHFADataType == EPT_u2 )
    {
        GByte *pabyData = static_cast<GByte *>(pImage);

        for( int ii = nBlockXSize * nBlockYSize - 4; ii >= 0; ii -= 4 )
        {
            int k = ii >> 2;
            pabyData[ii + 3] = (pabyData[k] >> 6) & 0x3;
            pabyData[ii + 2] = (pabyData[k] >> 4) & 0x3;
            pabyData[ii + 1] = (pabyData[k] >> 2) & 0x3;
            pabyData[ii] = (pabyData[k]) & 0x3;
        }
    }
    if( eErr == CE_None && eHFADataType == EPT_u1)
    {
        GByte *pabyData = static_cast<GByte *>(pImage);

        for( int ii = nBlockXSize * nBlockYSize - 1; ii >= 0; ii-- )
        {
            if( (pabyData[ii >> 3] & (1 << (ii & 0x7))) )
                pabyData[ii] = 1;
            else
                pabyData[ii] = 0;
        }
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr HFARasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                   void *pImage )

{
    GByte *pabyOutBuf = static_cast<GByte *>(pImage);

    // Do we need to pack 1/2/4 bit data?
    // TODO(schwehr): Make symbolic constants with explanations.
    if( eHFADataType == EPT_u1 ||
        eHFADataType == EPT_u2 ||
        eHFADataType == EPT_u4 )
    {
        const int nPixCount = nBlockXSize * nBlockYSize;
        pabyOutBuf = static_cast<GByte *>(VSIMalloc2(nBlockXSize, nBlockYSize));
        if( pabyOutBuf == nullptr )
            return CE_Failure;

        if( eHFADataType == EPT_u1 )
        {
            for( int ii = 0; ii < nPixCount - 7; ii += 8 )
            {
                const int k = ii >> 3;
                // TODO(schwehr): Create a temp for (GByte *)pImage.
                pabyOutBuf[k] = (((GByte *)pImage)[ii] & 0x1) |
                                ((((GByte *)pImage)[ii + 1] & 0x1) << 1) |
                                ((((GByte *)pImage)[ii + 2] & 0x1) << 2) |
                                ((((GByte *)pImage)[ii + 3] & 0x1) << 3) |
                                ((((GByte *)pImage)[ii + 4] & 0x1) << 4) |
                                ((((GByte *)pImage)[ii + 5] & 0x1) << 5) |
                                ((((GByte *)pImage)[ii + 6] & 0x1) << 6) |
                                ((((GByte *)pImage)[ii + 7] & 0x1) << 7);
            }
        }
        else if( eHFADataType == EPT_u2 )
        {
            for( int ii = 0; ii < nPixCount - 3; ii += 4 )
            {
                const int k = ii >> 2;
                pabyOutBuf[k] = (((GByte *)pImage)[ii] & 0x3) |
                                ((((GByte *)pImage)[ii + 1] & 0x3) << 2) |
                                ((((GByte *)pImage)[ii + 2] & 0x3) << 4) |
                                ((((GByte *)pImage)[ii + 3] & 0x3) << 6);
            }
        }
        else if( eHFADataType == EPT_u4 )
        {
            for( int ii = 0; ii < nPixCount - 1; ii += 2 )
            {
                const int k = ii >> 1;
                pabyOutBuf[k] = (((GByte *)pImage)[ii] & 0xf) |
                                ((((GByte *)pImage)[ii + 1] & 0xf) << 4);
            }
        }
    }

    // Actually write out.
    const CPLErr nRetCode =
        nThisOverview == -1
        ? HFASetRasterBlock(hHFA, nBand, nBlockXOff, nBlockYOff, pabyOutBuf)
        : HFASetOverviewRasterBlock(hHFA, nBand, nThisOverview,
                                    nBlockXOff, nBlockYOff, pabyOutBuf);

    if( pabyOutBuf != pImage )
        CPLFree(pabyOutBuf);

    return nRetCode;
}

/************************************************************************/
/*                         GetDescription()                             */
/************************************************************************/

const char *HFARasterBand::GetDescription() const
{
    const char *pszName = HFAGetBandName(hHFA, nBand);

    if( pszName == nullptr )
        return GDALPamRasterBand::GetDescription();

    return pszName;
}

/************************************************************************/
/*                         SetDescription()                             */
/************************************************************************/
void HFARasterBand::SetDescription( const char *pszName )
{
    if( strlen(pszName) > 0 )
        HFASetBandName(hHFA, nBand, pszName);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp HFARasterBand::GetColorInterpretation()

{
    if( poCT != nullptr )
        return GCI_PaletteIndex;

    return GCI_Undefined;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *HFARasterBand::GetColorTable() { return poCT; }

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr HFARasterBand::SetColorTable( GDALColorTable *poCTable )

{
    if( GetAccess() == GA_ReadOnly )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Unable to set color table on read-only file.");
        return CE_Failure;
    }

    // Special case if we are clearing the color table.
    if( poCTable == nullptr )
    {
        delete poCT;
        poCT = nullptr;

        HFASetPCT(hHFA, nBand, 0, nullptr, nullptr, nullptr, nullptr);

        return CE_None;
    }

    // Write out the colortable, and update the configuration.
    int nColors = poCTable->GetColorEntryCount();

    /* -------------------------------------------------------------------- */
    /*      If we already have a non-empty RAT set and it's smaller than    */
    /*      the colour table, and all the trailing CT entries are the same, */
    /*      truncate the colour table. Helps when RATs travel via GTiff.    */
    /* -------------------------------------------------------------------- */
    const GDALRasterAttributeTable *poRAT = GetDefaultRAT();
    if (poRAT != nullptr && poRAT->GetRowCount() > 0 && poRAT->GetRowCount() < nColors)
    {
        bool match = true;
        const GDALColorEntry *color1 = poCTable->GetColorEntry(poRAT->GetRowCount());
        for (int i=poRAT->GetRowCount()+1; match && i<nColors; i++)
        {
            const GDALColorEntry *color2 = poCTable->GetColorEntry(i);
            match = (color1->c1 == color2->c1
                        && color1->c2 == color2->c2
                        && color1->c3 == color2->c3
                        && color1->c4 == color2->c4 );
        }
        if (match)
        {
            CPLDebug( "HFA", "SetColorTable: Truncating PCT size (%d) to RAT size (%d)", nColors, poRAT->GetRowCount() );
            nColors = poRAT->GetRowCount();
        }
    }

    double *padfRed =
        static_cast<double *>(CPLMalloc(sizeof(double) * nColors));
    double *padfGreen =
        static_cast<double *>(CPLMalloc(sizeof(double) * nColors));
    double *padfBlue =
        static_cast<double *>(CPLMalloc(sizeof(double) * nColors));
    double *padfAlpha =
        static_cast<double *>(CPLMalloc(sizeof(double) * nColors));

    for( int iColor = 0; iColor < nColors; iColor++ )
    {
        GDALColorEntry sRGB;

        poCTable->GetColorEntryAsRGB(iColor, &sRGB);

        padfRed[iColor] = sRGB.c1 / 255.0;
        padfGreen[iColor] = sRGB.c2 / 255.0;
        padfBlue[iColor] = sRGB.c3 / 255.0;
        padfAlpha[iColor] = sRGB.c4 / 255.0;
    }

    HFASetPCT(hHFA, nBand, nColors, padfRed, padfGreen, padfBlue, padfAlpha);

    CPLFree(padfRed);
    CPLFree(padfGreen);
    CPLFree(padfBlue);
    CPLFree(padfAlpha);

    if( poCT )
      delete poCT;

    poCT = poCTable->Clone();

    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr HFARasterBand::SetMetadata( char **papszMDIn, const char *pszDomain )

{
    bMetadataDirty = true;

    return GDALPamRasterBand::SetMetadata(papszMDIn, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr HFARasterBand::SetMetadataItem( const char *pszTag, const char *pszValue,
                                       const char *pszDomain )

{
    bMetadataDirty = true;

    return GDALPamRasterBand::SetMetadataItem(pszTag, pszValue, pszDomain);
}

/************************************************************************/
/*                           CleanOverviews()                           */
/************************************************************************/

CPLErr HFARasterBand::CleanOverviews()

{
    if( nOverviews == 0 )
        return CE_None;

    // Clear our reference to overviews as bands.
    for( int iOverview = 0; iOverview < nOverviews; iOverview++ )
        delete papoOverviewBands[iOverview];

    CPLFree(papoOverviewBands);
    papoOverviewBands = nullptr;
    nOverviews = 0;

    // Search for any RRDNamesList and destroy it.
    HFABand *poBand = hHFA->papoBand[nBand - 1];
    HFAEntry *poEntry = poBand->poNode->GetNamedChild("RRDNamesList");
    if( poEntry != nullptr )
    {
        poEntry->RemoveAndDestroy();
    }

    // Destroy and subsample layers under our band.
    for( HFAEntry *poChild = poBand->poNode->GetChild();
         poChild != nullptr; )
    {
        HFAEntry *poNext = poChild->GetNext();

        if( EQUAL(poChild->GetType(), "Eimg_Layer_SubSample") )
            poChild->RemoveAndDestroy();

        poChild = poNext;
    }

    // Clean up dependent file if we are the last band under the
    // assumption there will be nothing else referencing it after
    // this.
    if( hHFA->psDependent != hHFA && hHFA->psDependent != nullptr )
    {
        CPLString osFilename = CPLFormFilename(
            hHFA->psDependent->pszPath, hHFA->psDependent->pszFilename, nullptr);

        CPL_IGNORE_RET_VAL(HFAClose(hHFA->psDependent));
        hHFA->psDependent = nullptr;

        CPLDebug("HFA", "Unlink(%s)", osFilename.c_str());
        VSIUnlink(osFilename);
    }

    return CE_None;
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

CPLErr HFARasterBand::BuildOverviews( const char *pszResampling,
                                      int nReqOverviews, int *panOverviewList,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData )

{
    EstablishOverviews();

    if( nThisOverview != -1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to build overviews on an overview layer.");

        return CE_Failure;
    }

    if( nReqOverviews == 0 )
        return CleanOverviews();

    GDALRasterBand **papoOvBands = static_cast<GDALRasterBand **>(
        CPLCalloc(sizeof(void*), nReqOverviews));

    bool bNoRegen = false;
    if( STARTS_WITH_CI(pszResampling, "NO_REGEN:") )
    {
        pszResampling += 9;
        bNoRegen = true;
    }

    // Loop over overview levels requested.
    for( int iOverview = 0; iOverview < nReqOverviews; iOverview++ )
    {
        // Find this overview level.
        const int nReqOvLevel =
            GDALOvLevelAdjust2(panOverviewList[iOverview],
                               nRasterXSize, nRasterYSize);

        for( int i = 0; i < nOverviews && papoOvBands[iOverview] == nullptr; i++ )
        {
            if( papoOverviewBands[i] == nullptr )
            {
                CPLDebug("HFA",
                         "Shouldn't happen happened at line %d", __LINE__);
                continue;
            }

            const int nThisOvLevel = GDALComputeOvFactor(
                papoOverviewBands[i]->GetXSize(), GetXSize(),
                papoOverviewBands[i]->GetYSize(), GetYSize());

            if( nReqOvLevel == nThisOvLevel )
                papoOvBands[iOverview] = papoOverviewBands[i];
        }

        // If this overview level does not yet exist, create it now.
        if( papoOvBands[iOverview] == nullptr )
        {
            const int iResult = HFACreateOverview(hHFA, nBand,
                                                  panOverviewList[iOverview],
                                                  pszResampling);
            if( iResult < 0 )
            {
                CPLFree(papoOvBands);
                return CE_Failure;
            }

            if( papoOverviewBands == nullptr && nOverviews == 0 && iResult > 0 )
            {
                CPLDebug("HFA",
                         "Shouldn't happen happened at line %d", __LINE__);
                papoOverviewBands = static_cast<HFARasterBand **>(
                    CPLCalloc(sizeof(void *), iResult));
            }

            nOverviews = iResult + 1;
            papoOverviewBands = static_cast<HFARasterBand **>(
                CPLRealloc(papoOverviewBands, sizeof(void *) * nOverviews));
            papoOverviewBands[iResult] =
                new HFARasterBand((HFADataset *)poDS, nBand, iResult);

            papoOvBands[iOverview] = papoOverviewBands[iResult];
        }
    }

    CPLErr eErr = CE_None;

    if( !bNoRegen )
        eErr = GDALRegenerateOverviews((GDALRasterBandH) this,
                                       nReqOverviews,
                                       (GDALRasterBandH *) papoOvBands,
                                       pszResampling,
                                       pfnProgress, pProgressData);

    CPLFree(papoOvBands);

    return eErr;
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr
HFARasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax,
                                    int *pnBuckets, GUIntBig ** ppanHistogram,
                                    int bForce,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData )

{
    if( GetMetadataItem("STATISTICS_HISTOBINVALUES") != nullptr &&
        GetMetadataItem("STATISTICS_HISTOMIN") != nullptr &&
        GetMetadataItem("STATISTICS_HISTOMAX") != nullptr )
    {
        const char *pszBinValues = GetMetadataItem("STATISTICS_HISTOBINVALUES");

        *pdfMin = CPLAtof(GetMetadataItem("STATISTICS_HISTOMIN"));
        *pdfMax = CPLAtof(GetMetadataItem("STATISTICS_HISTOMAX"));

        *pnBuckets = 0;
        for( int i = 0; pszBinValues[i] != '\0'; i++ )
        {
            if( pszBinValues[i] == '|' )
                (*pnBuckets)++;
        }

        *ppanHistogram =
            static_cast<GUIntBig *>(CPLCalloc(sizeof(GUIntBig), *pnBuckets));

        const char *pszNextBin = pszBinValues;
        for( int i = 0; i < *pnBuckets; i++ )
        {
            (*ppanHistogram)[i] =
                static_cast<GUIntBig>(CPLAtoGIntBig(pszNextBin));

            while( *pszNextBin != '|' && *pszNextBin != '\0' )
                pszNextBin++;
            if( *pszNextBin == '|' )
                pszNextBin++;
        }

        // Adjust min/max to reflect outer edges of buckets.
        double dfBucketWidth = (*pdfMax - *pdfMin) / (*pnBuckets - 1);
        *pdfMax += 0.5 * dfBucketWidth;
        *pdfMin -= 0.5 * dfBucketWidth;

        return CE_None;
    }

    return GDALPamRasterBand::GetDefaultHistogram(pdfMin, pdfMax,
                                                  pnBuckets, ppanHistogram,
                                                  bForce,
                                                  pfnProgress,
                                                  pProgressData);
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

CPLErr HFARasterBand::SetDefaultRAT( const GDALRasterAttributeTable * poRAT )

{
    if( poRAT == nullptr )
        return CE_Failure;

    delete poDefaultRAT;
    poDefaultRAT = nullptr;

    CPLErr r = WriteNamedRAT("Descriptor_Table", poRAT);
    if (!r)
        GetDefaultRAT();

    return r;
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *HFARasterBand::GetDefaultRAT()

{
    if( poDefaultRAT == nullptr )
        poDefaultRAT = new HFARasterAttributeTable(this, "Descriptor_Table");

    return poDefaultRAT;
}

/************************************************************************/
/*                            WriteNamedRAT()                            */
/************************************************************************/

CPLErr HFARasterBand::WriteNamedRAT( const char * /*pszName*/,
                                     const GDALRasterAttributeTable *poRAT )
{
    // Find the requested table.
    HFAEntry *poDT =
        hHFA->papoBand[nBand - 1]->poNode->GetNamedChild("Descriptor_Table");
    if( poDT == nullptr || !EQUAL(poDT->GetType(), "Edsc_Table") )
        poDT = HFAEntry::New(hHFA->papoBand[nBand - 1]->psInfo,
                             "Descriptor_Table", "Edsc_Table",
                             hHFA->papoBand[nBand - 1]->poNode );

    const int nRowCount = poRAT->GetRowCount();

    poDT->SetIntField("numrows", nRowCount);
    // Check if binning is set on this RAT.
    double dfBinSize = 0.0;
    double dfRow0Min = 0.0;
    if( poRAT->GetLinearBinning(&dfRow0Min, &dfBinSize) )
    {
        // Then it should have an Edsc_BinFunction.
        HFAEntry *poBinFunction = poDT->GetNamedChild("#Bin_Function#");
        if( poBinFunction == nullptr ||
            !EQUAL(poBinFunction->GetType(), "Edsc_BinFunction") )
        {
            poBinFunction = HFAEntry::New(hHFA->papoBand[nBand - 1]->psInfo,
                                          "#Bin_Function#", "Edsc_BinFunction",
                                          poDT);
        }

        // direct for thematic layers, linear otherwise
        const char* pszLayerType =
            hHFA->papoBand[nBand-1]->poNode->GetStringField("layerType");
        if( pszLayerType == nullptr ||
            STARTS_WITH_CI(pszLayerType, "thematic") )
            poBinFunction->SetStringField("binFunctionType", "direct");
        else
            poBinFunction->SetStringField("binFunctionType", "linear" );

        poBinFunction->SetDoubleField("minLimit", dfRow0Min);
        poBinFunction->SetDoubleField(
           "maxLimit", (nRowCount - 1) * dfBinSize + dfRow0Min);
        poBinFunction->SetIntField("numBins", nRowCount);
    }

    // Loop through each column in the RAT.
    for( int col = 0; col < poRAT->GetColumnCount(); col++ )
    {
        const char *pszName = nullptr;

        if( poRAT->GetUsageOfCol(col) == GFU_Red )
        {
            pszName = "Red";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_Green )
        {
            pszName = "Green";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_Blue )
        {
            pszName = "Blue";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_Alpha )
        {
            pszName = "Opacity";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_PixelCount )
        {
            pszName = "Histogram";
        }
        else if( poRAT->GetUsageOfCol(col) == GFU_Name )
        {
            pszName = "Class_Names";
        }
        else
        {
            pszName = poRAT->GetNameOfCol(col);
        }

        // Check to see if a column with pszName exists and create if
        // if necessary.
        HFAEntry *poColumn = poDT->GetNamedChild(pszName);

        if( poColumn == nullptr || !EQUAL(poColumn->GetType(), "Edsc_Column") )
            poColumn = HFAEntry::New(hHFA->papoBand[nBand-1]->psInfo,
                                     pszName, "Edsc_Column",
                                     poDT);

        poColumn->SetIntField("numRows", nRowCount);
        // Color cols which are integer in GDAL are written as floats in HFA.
        bool bIsColorCol = false;
        if( poRAT->GetUsageOfCol(col) == GFU_Red ||
            poRAT->GetUsageOfCol(col) == GFU_Green ||
            poRAT->GetUsageOfCol(col) == GFU_Blue ||
            poRAT->GetUsageOfCol(col) == GFU_Alpha )
        {
            bIsColorCol = true;
        }

        // Write float also if a color column or histogram.
        if( poRAT->GetTypeOfCol(col) == GFT_Real ||
            bIsColorCol ||
            poRAT->GetUsageOfCol(col) == GFU_PixelCount )
        {
            const int nOffset =
                HFAAllocateSpace(hHFA->papoBand[nBand - 1]->psInfo,
                                 static_cast<GUInt32>(nRowCount) *
                                 static_cast<GUInt32>(sizeof(double)));
            poColumn->SetIntField("columnDataPtr", nOffset);
            poColumn->SetStringField("dataType", "real");

            double *padfColData =
                static_cast<double *>(CPLCalloc(nRowCount, sizeof(double)));
            for( int i = 0; i < nRowCount; i++)
            {
                if( bIsColorCol )
                    // Stored 0..1
                    padfColData[i] = poRAT->GetValueAsInt(i, col) / 255.0;
                else
                    padfColData[i] = poRAT->GetValueAsDouble(i, col);
            }
#ifdef CPL_MSB
            GDALSwapWords(padfColData, 8, nRowCount, 8);
#endif
            if( VSIFSeekL(hHFA->fp, nOffset, SEEK_SET ) != 0 ||
                VSIFWriteL(padfColData, nRowCount, sizeof(double),
                           hHFA->fp ) != sizeof(double) )
            {
                CPLError(CE_Failure, CPLE_FileIO, "WriteNamedRAT() failed");
                CPLFree(padfColData);
                return CE_Failure;
            }
            CPLFree(padfColData);
        }
        else if( poRAT->GetTypeOfCol(col) == GFT_String )
        {
            unsigned int nMaxNumChars = 0;
            // Find the length of the longest string.
            for( int i = 0; i < nRowCount; i++)
            {
                // Include terminating byte.
                const unsigned int nNumChars = static_cast<unsigned int>(
                    strlen(poRAT->GetValueAsString(i, col)) + 1);
                if( nMaxNumChars < nNumChars )
                {
                    nMaxNumChars = nNumChars;
                }
            }

            const int nOffset =
                HFAAllocateSpace(hHFA->papoBand[nBand - 1]->psInfo,
                                 (nRowCount + 1) * nMaxNumChars);
            poColumn->SetIntField("columnDataPtr", nOffset);
            poColumn->SetStringField("dataType", "string");
            poColumn->SetIntField("maxNumChars", nMaxNumChars);

            char *pachColData =
                static_cast<char *>(CPLCalloc(nRowCount + 1, nMaxNumChars));
            for( int i = 0; i < nRowCount; i++ )
            {
                strcpy(&pachColData[nMaxNumChars * i],
                       poRAT->GetValueAsString(i, col));
            }
            if( VSIFSeekL(hHFA->fp, nOffset, SEEK_SET) != 0 ||
                VSIFWriteL(pachColData, nRowCount,
                           nMaxNumChars, hHFA->fp) != nMaxNumChars )
            {
                CPLError(CE_Failure, CPLE_FileIO, "WriteNamedRAT() failed");
                CPLFree(pachColData);
                return CE_Failure;
            }
            CPLFree(pachColData);
        }
        else if( poRAT->GetTypeOfCol(col) == GFT_Integer )
        {
            const int nOffset = HFAAllocateSpace(
                hHFA->papoBand[nBand - 1]->psInfo,
                static_cast<GUInt32>(nRowCount) * (GUInt32)sizeof(GInt32));
            poColumn->SetIntField("columnDataPtr", nOffset);
            poColumn->SetStringField("dataType", "integer");

            GInt32 *panColData =
                static_cast<GInt32 *>(CPLCalloc(nRowCount, sizeof(GInt32)));
            for( int i = 0; i < nRowCount; i++)
            {
                panColData[i] = poRAT->GetValueAsInt(i, col);
            }
#ifdef CPL_MSB
            GDALSwapWords(panColData, 4, nRowCount, 4);
#endif
            if( VSIFSeekL(hHFA->fp, nOffset, SEEK_SET) != 0 ||
                VSIFWriteL(panColData, nRowCount,
                           sizeof(GInt32), hHFA->fp) != sizeof(GInt32) )
            {
                CPLError(CE_Failure, CPLE_FileIO, "WriteNamedRAT() failed");
                CPLFree(panColData);
                return CE_Failure;
            }
            CPLFree(panColData);
        }
        else
        {
            // Can't deal with any of the others yet.
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Writing this data type in a column is not supported "
                     "for this Raster Attribute Table.");
        }
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            HFADataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            HFADataset()                            */
/************************************************************************/

HFADataset::HFADataset() :
    hHFA(nullptr),
    bMetadataDirty(false),
    bGeoDirty(false),
    bIgnoreUTM(false),
    bForceToPEString(false),
    nGCPCount(0)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    memset(asGCPList, 0, sizeof(asGCPList));
    memset(adfGeoTransform, 0, sizeof(adfGeoTransform));
}

/************************************************************************/
/*                           ~HFADataset()                            */
/************************************************************************/

HFADataset::~HFADataset()

{
    HFADataset::FlushCache(true);

    // Destroy the raster bands if they exist.  We forcibly clean
    // them up now to avoid any effort to write to them after the
    // file is closed.
    for( int i = 0; i < nBands && papoBands != nullptr; i++ )
    {
        if( papoBands[i] != nullptr )
            delete papoBands[i];
    }

    CPLFree(papoBands);
    papoBands = nullptr;

    // Close the file.
    if( hHFA != nullptr )
    {
        if( HFAClose(hHFA) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
        hHFA = nullptr;
    }

    if( nGCPCount > 0 )
        GDALDeinitGCPs(36, asGCPList);
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void HFADataset::FlushCache(bool bAtClosing)

{
    GDALPamDataset::FlushCache(bAtClosing);

    if( eAccess != GA_Update )
        return;

    if( bGeoDirty )
        WriteProjection();

    if( bMetadataDirty && GetMetadata() != nullptr )
    {
        HFASetMetadata(hHFA, 0, GetMetadata());
        bMetadataDirty = false;
    }

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        HFARasterBand *poBand =
            static_cast<HFARasterBand *>(GetRasterBand(iBand + 1));
        if( poBand->bMetadataDirty && poBand->GetMetadata() != nullptr )
        {
            HFASetMetadata(hHFA, iBand + 1, poBand->GetMetadata());
            poBand->bMetadataDirty = false;
        }
    }

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs(nGCPCount, asGCPList);
    }
}

/************************************************************************/
/*                          WriteProjection()                           */
/************************************************************************/

CPLErr HFADataset::WriteProjection()

{
    bool bPEStringStored = false;

    bGeoDirty = false;

    const OGRSpatialReference& oSRS = m_oSRS;
    const bool bHaveSRS = !oSRS.IsEmpty();

    // Initialize projection and datum.
    Eprj_Datum sDatum;
    Eprj_ProParameters sPro;
    Eprj_MapInfo sMapInfo;
    memset(&sPro, 0, sizeof(sPro));
    memset(&sDatum, 0, sizeof(sDatum));
    memset(&sMapInfo, 0, sizeof(sMapInfo));

    // Collect datum information.
    OGRSpatialReference *poGeogSRS = bHaveSRS ? oSRS.CloneGeogCS() : nullptr;

    if( poGeogSRS )
    {
        sDatum.datumname =
            const_cast<char *>(poGeogSRS->GetAttrValue("GEOGCS|DATUM"));
        if( sDatum.datumname == nullptr )
            sDatum.datumname = const_cast<char *>("");

        // WKT to Imagine translation.
        const char* const* papszDatumMap = HFAGetDatumMap();
        for( int i = 0; papszDatumMap[i] != nullptr; i += 2 )
        {
            if( EQUAL(sDatum.datumname, papszDatumMap[i+1]) )
            {
                sDatum.datumname = (char *)papszDatumMap[i];
                break;
            }
        }

        // Map some EPSG datum codes directly to Imagine names.
        const int nGCS = poGeogSRS->GetEPSGGeogCS();

        if( nGCS == 4326 )
            sDatum.datumname = const_cast<char *>("WGS 84");
        if( nGCS == 4322 )
            sDatum.datumname = const_cast<char*>("WGS 1972");
        if( nGCS == 4267 )
            sDatum.datumname = const_cast<char *>("NAD27");
        if( nGCS == 4269 )
            sDatum.datumname = const_cast<char *>("NAD83");
        if( nGCS == 4283 )
            sDatum.datumname = const_cast<char *>("GDA94");
        if( nGCS == 6284 )
            sDatum.datumname = const_cast<char *>("Pulkovo 1942");

        if( poGeogSRS->GetTOWGS84(sDatum.params) == OGRERR_NONE )
        {
            sDatum.type = EPRJ_DATUM_PARAMETRIC;
            sDatum.params[3] *= -ARCSEC2RAD;
            sDatum.params[4] *= -ARCSEC2RAD;
            sDatum.params[5] *= -ARCSEC2RAD;
            sDatum.params[6] *= 1e-6;
        }
        else if( EQUAL(sDatum.datumname, "NAD27") )
        {
            sDatum.type = EPRJ_DATUM_GRID;
            sDatum.gridname = const_cast<char *>("nadcon.dat");
        }
        else
        {
            // We will default to this (effectively WGS84) for now.
            sDatum.type = EPRJ_DATUM_PARAMETRIC;
        }

        // Verify if we need to write a ESRI PE string.
        bPEStringStored = CPL_TO_BOOL(WritePeStringIfNeeded(&oSRS, hHFA));

        sPro.proSpheroid.sphereName =
            (char *)poGeogSRS->GetAttrValue("GEOGCS|DATUM|SPHEROID");
        sPro.proSpheroid.a = poGeogSRS->GetSemiMajor();
        sPro.proSpheroid.b = poGeogSRS->GetSemiMinor();
        sPro.proSpheroid.radius = sPro.proSpheroid.a;

        const double a2 = sPro.proSpheroid.a * sPro.proSpheroid.a;
        const double b2 = sPro.proSpheroid.b * sPro.proSpheroid.b;

        // a2 == 0 is non sensical of course. Just to please fuzzers
        sPro.proSpheroid.eSquared = (a2 == 0.0) ? 0.0 : (a2 - b2) / a2;
    }

    if( sDatum.datumname == nullptr )
        sDatum.datumname = const_cast<char *>("");
    if( sPro.proSpheroid.sphereName == nullptr )
        sPro.proSpheroid.sphereName = const_cast<char *>("");

    // Recognise various projections.
    const char *pszProjName = nullptr;

    if( bHaveSRS )
        pszProjName = oSRS.GetAttrValue("PROJCS|PROJECTION");

    if( bForceToPEString && !bPEStringStored )
    {
        char *pszPEString = nullptr;
        const char* const apszOptions[] = { "FORMAT=WKT1_ESRI", nullptr };
        oSRS.exportToWkt(&pszPEString, apszOptions);
        // Need to transform this into ESRI format.
        HFASetPEString(hHFA, pszPEString);
        CPLFree(pszPEString);

        bPEStringStored = true;
    }
    else if( pszProjName == nullptr )
    {
        if( bHaveSRS && oSRS.IsGeographic() )
        {
            sPro.proNumber = EPRJ_LATLONG;
            sPro.proName = const_cast<char *>("Geographic (Lat/Lon)");
        }
    }
    // TODO: Add State Plane.
    else if( !bIgnoreUTM && oSRS.GetUTMZone(nullptr) != 0 )
    {
        int bNorth = FALSE;
        const int nZone = oSRS.GetUTMZone(&bNorth);
        sPro.proNumber = EPRJ_UTM;
        sPro.proName = const_cast<char *>("UTM");
        sPro.proZone = nZone;
        if( bNorth )
            sPro.proParams[3] = 1.0;
        else
            sPro.proParams[3] = -1.0;
    }
    else if( EQUAL(pszProjName, SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        sPro.proNumber = EPRJ_ALBERS_CONIC_EQUAL_AREA;
        sPro.proName = const_cast<char *>("Albers Conical Equal Area");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_2) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        sPro.proNumber = EPRJ_LAMBERT_CONFORMAL_CONIC;
        sPro.proName = const_cast<char *>("Lambert Conformal Conic");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_2) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_MERCATOR_1SP) &&
             oSRS.GetProjParm(SRS_PP_SCALE_FACTOR) == 1.0 )
    {
        sPro.proNumber = EPRJ_MERCATOR;
        sPro.proName = const_cast<char *>("Mercator");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_MERCATOR_1SP) )
    {
        sPro.proNumber = EPRJ_MERCATOR_VARIANT_A;
        sPro.proName = const_cast<char *>("Mercator (Variant A)");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN)*D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN)*D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR);
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_KROVAK) )
    {
        sPro.proNumber = EPRJ_KROVAK;
        sPro.proName = const_cast<char *>("Krovak");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR);
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[9] = oSRS.GetProjParm(SRS_PP_PSEUDO_STD_PARALLEL_1);

        sPro.proParams[8] = 0.0;  // XY plane rotation
        sPro.proParams[10] = 1.0;  // X scale
        sPro.proParams[11] = 1.0;  // Y scale
    }
    else if( EQUAL(pszProjName, SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_POLAR_STEREOGRAPHIC;
        sPro.proName = const_cast<char *>("Polar Stereographic");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        // Hopefully the scale factor is 1.0!
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_POLYCONIC) )
    {
        sPro.proNumber = EPRJ_POLYCONIC;
        sPro.proName = const_cast<char *>("Polyconic");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_EQUIDISTANT_CONIC) )
    {
        sPro.proNumber = EPRJ_EQUIDISTANT_CONIC;
        sPro.proName = const_cast<char *>("Equidistant Conic");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_2) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] = 1.0;
    }
    else if( EQUAL(pszProjName, SRS_PT_TRANSVERSE_MERCATOR) )
    {
        sPro.proNumber = EPRJ_TRANSVERSE_MERCATOR;
        sPro.proName = const_cast<char *>("Transverse Mercator");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_STEREOGRAPHIC_EXTENDED;
        sPro.proName = const_cast<char *>("Stereographic (Extended)");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        sPro.proNumber = EPRJ_LAMBERT_AZIMUTHAL_EQUAL_AREA;
        sPro.proName = const_cast<char *>("Lambert Azimuthal Equal-area");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        sPro.proNumber = EPRJ_AZIMUTHAL_EQUIDISTANT;
        sPro.proName = const_cast<char *>("Azimuthal Equidistant");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_GNOMONIC) )
    {
        sPro.proNumber = EPRJ_GNOMONIC;
        sPro.proName = const_cast<char *>("Gnomonic");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_ORTHOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_ORTHOGRAPHIC;
        sPro.proName = const_cast<char *>("Orthographic");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_SINUSOIDAL) )
    {
        sPro.proNumber = EPRJ_SINUSOIDAL;
        sPro.proName = const_cast<char *>("Sinusoidal");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_EQUIRECTANGULAR) )
    {
        sPro.proNumber = EPRJ_EQUIRECTANGULAR;
        sPro.proName = const_cast<char *>("Equirectangular");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_MILLER_CYLINDRICAL) )
    {
        sPro.proNumber = EPRJ_MILLER_CYLINDRICAL;
        sPro.proName = const_cast<char *>("Miller Cylindrical");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        // Hopefully the latitude is zero!
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_VANDERGRINTEN) )
    {
        sPro.proNumber = EPRJ_VANDERGRINTEN;
        sPro.proName = const_cast<char *>("Van der Grinten");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        if( oSRS.GetProjParm(SRS_PP_RECTIFIED_GRID_ANGLE) == 0.0 )
        {
            sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR;
            sPro.proName = const_cast<char *>("Oblique Mercator (Hotine)");
            sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH) * D2R;
            sPro.proParams[4] =
                oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
            sPro.proParams[5] =
                oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
            sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
            sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
            sPro.proParams[12] = 1.0;
        }
        else
        {
            sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR_VARIANT_A;
            sPro.proName =
                const_cast<char *>("Hotine Oblique Mercator (Variant A)");
            sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
            sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH) * D2R;
            sPro.proParams[4] =
                oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
            sPro.proParams[5] =
                oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
            sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
            sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
            sPro.proParams[8] =
                oSRS.GetProjParm(SRS_PP_RECTIFIED_GRID_ANGLE) * D2R;
        }
    }
    else if( EQUAL(pszProjName, SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER) )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER;
        sPro.proName =
            const_cast<char *>("Hotine Oblique Mercator Azimuth Center");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[12] = 1.0;
    }
    else if( EQUAL(pszProjName, SRS_PT_ROBINSON) )
    {
        sPro.proNumber = EPRJ_ROBINSON;
        sPro.proName = const_cast<char *>("Robinson");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_MOLLWEIDE) )
    {
        sPro.proNumber = EPRJ_MOLLWEIDE;
        sPro.proName = const_cast<char *>("Mollweide");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_ECKERT_I) )
    {
        sPro.proNumber = EPRJ_ECKERT_I;
        sPro.proName = const_cast<char *>("Eckert I");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_ECKERT_II) )
    {
        sPro.proNumber = EPRJ_ECKERT_II;
        sPro.proName = const_cast<char *>("Eckert II");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_ECKERT_III) )
    {
        sPro.proNumber = EPRJ_ECKERT_III;
        sPro.proName = const_cast<char *>("Eckert III");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_ECKERT_IV) )
    {
        sPro.proNumber = EPRJ_ECKERT_IV;
        sPro.proName = const_cast<char *>("Eckert IV");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_ECKERT_V) )
    {
        sPro.proNumber = EPRJ_ECKERT_V;
        sPro.proName = const_cast<char *>("Eckert V");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_ECKERT_VI) )
    {
        sPro.proNumber = EPRJ_ECKERT_VI;
        sPro.proName = const_cast<char *>("Eckert VI");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_GALL_STEREOGRAPHIC) )
    {
        sPro.proNumber = EPRJ_GALL_STEREOGRAPHIC;
        sPro.proName = const_cast<char *>("Gall Stereographic");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_CASSINI_SOLDNER) )
    {
        sPro.proNumber = EPRJ_CASSINI;
        sPro.proName = const_cast<char *>("Cassini");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_TWO_POINT_EQUIDISTANT) )
    {
        sPro.proNumber = EPRJ_TWO_POINT_EQUIDISTANT;
        sPro.proName = const_cast<char *>("Two_Point_Equidistant");
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] =
            oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0) * D2R;
        sPro.proParams[9] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0) * D2R;
        sPro.proParams[10] =
            oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 60.0) * D2R;
        sPro.proParams[11] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_2, 60.0) * D2R;
    }
    else if( EQUAL(pszProjName, SRS_PT_BONNE) )
    {
        sPro.proNumber = EPRJ_BONNE;
        sPro.proName = const_cast<char *>("Bonne");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Loximuthal") )
    {
        sPro.proNumber = EPRJ_LOXIMUTHAL;
        sPro.proName = const_cast<char *>("Loximuthal");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm("central_parallel") * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Quartic_Authalic") )
    {
        sPro.proNumber = EPRJ_QUARTIC_AUTHALIC;
        sPro.proName = const_cast<char *>("Quartic Authalic");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Winkel_I") )
    {
        sPro.proNumber = EPRJ_WINKEL_I;
        sPro.proName = const_cast<char *>("Winkel I");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Winkel_II") )
    {
        sPro.proNumber = EPRJ_WINKEL_II;
        sPro.proName = const_cast<char *>("Winkel II");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Behrmann") )
    {
        sPro.proNumber = EPRJ_BEHRMANN;
        sPro.proName = const_cast<char *>("Behrmann");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Equidistant_Cylindrical") )
    {
        sPro.proNumber = EPRJ_EQUIDISTANT_CYLINDRICAL;
        sPro.proName = const_cast<char *>("Equidistant_Cylindrical");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_KROVAK) )
    {
        sPro.proNumber = EPRJ_KROVAK;
        sPro.proName = const_cast<char *>("Krovak");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[3] = oSRS.GetProjParm(SRS_PP_AZIMUTH) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] = oSRS.GetProjParm("XY_Plane_Rotation", 0.0) * D2R;
        sPro.proParams[9] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[10] = oSRS.GetProjParm("X_Scale", 1.0);
        sPro.proParams[11] = oSRS.GetProjParm("Y_Scale", 1.0);
    }
    else if( EQUAL(pszProjName, "Double_Stereographic") )
    {
        sPro.proNumber = EPRJ_DOUBLE_STEREOGRAPHIC;
        sPro.proName = const_cast<char *>("Double_Stereographic");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Aitoff") )
    {
        sPro.proNumber = EPRJ_AITOFF;
        sPro.proName = const_cast<char *>("Aitoff");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Craster_Parabolic") )
    {
        sPro.proNumber = EPRJ_CRASTER_PARABOLIC;
        sPro.proName = const_cast<char *>("Craster_Parabolic");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        sPro.proNumber = EPRJ_CYLINDRICAL_EQUAL_AREA;
        sPro.proName = const_cast<char *>("Cylindrical_Equal_Area");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Flat_Polar_Quartic") )
    {
        sPro.proNumber = EPRJ_FLAT_POLAR_QUARTIC;
        sPro.proName = const_cast<char *>("Flat_Polar_Quartic");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Times") )
    {
        sPro.proNumber = EPRJ_TIMES;
        sPro.proName = const_cast<char *>("Times");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Winkel_Tripel") )
    {
        sPro.proNumber = EPRJ_WINKEL_TRIPEL;
        sPro.proName = const_cast<char *>("Winkel_Tripel");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_STANDARD_PARALLEL_1) * D2R;
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Hammer_Aitoff") )
    {
        sPro.proNumber = EPRJ_HAMMER_AITOFF;
        sPro.proName = const_cast<char *>("Hammer_Aitoff");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Vertical_Near_Side_Perspective") )
    {
        sPro.proNumber = EPRJ_VERTICAL_NEAR_SIDE_PERSPECTIVE;
        sPro.proName = const_cast<char *>("Vertical_Near_Side_Perspective");
        sPro.proParams[2] = oSRS.GetProjParm("Height");
        sPro.proParams[4] =
            oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_CENTER, 75.0) * D2R;
        sPro.proParams[5] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER, 40.0) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }
    else if( EQUAL(pszProjName, "Hotine_Oblique_Mercator_Two_Point_Center") )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_CENTER;
        sPro.proName =
            const_cast<char *>("Hotine_Oblique_Mercator_Two_Point_Center");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[5] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER, 40.0) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] =
            oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0) * D2R;
        sPro.proParams[9] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0) * D2R;
        sPro.proParams[10] =
            oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 60.0) * D2R;
        sPro.proParams[11] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_2, 60.0) * D2R;
    }
    else if( EQUAL(pszProjName,
                   SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN) )
    {
        sPro.proNumber = EPRJ_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN;
        sPro.proName = const_cast<char *>(
            "Hotine_Oblique_Mercator_Two_Point_Natural_Origin");
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[5] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_CENTER, 40.0) * D2R;
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
        sPro.proParams[8] =
            oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_1, 0.0) * D2R;
        sPro.proParams[9] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_1, 0.0) * D2R;
        sPro.proParams[10] =
            oSRS.GetProjParm(SRS_PP_LONGITUDE_OF_POINT_2, 60.0) * D2R;
        sPro.proParams[11] =
            oSRS.GetProjParm(SRS_PP_LATITUDE_OF_POINT_2, 60.0) * D2R;
    }
    else if( EQUAL(pszProjName, "New_Zealand_Map_Grid") )
    {
        sPro.proType = EPRJ_EXTERNAL;
        sPro.proNumber = 0;
        sPro.proExeName = const_cast<char *>(EPRJ_EXTERNAL_NZMG);
        sPro.proName = const_cast<char *>("New Zealand Map Grid");
        sPro.proZone = 0;
        sPro.proParams[0] = 0;  // False easting etc not stored in .img it seems
        sPro.proParams[1] = 0;  // always fixed by definition.
        sPro.proParams[2] = 0;
        sPro.proParams[3] = 0;
        sPro.proParams[4] = 0;
        sPro.proParams[5] = 0;
        sPro.proParams[6] = 0;
        sPro.proParams[7] = 0;
    }
    else if( EQUAL(pszProjName, SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED) )
    {
        sPro.proNumber = EPRJ_TRANSVERSE_MERCATOR_SOUTH_ORIENTATED;
        sPro.proName =
            const_cast<char *>("Transverse Mercator (South Orientated)");
        sPro.proParams[4] = oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN) * D2R;
        sPro.proParams[5] = oSRS.GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN) * D2R;
        sPro.proParams[2] = oSRS.GetProjParm(SRS_PP_SCALE_FACTOR, 1.0);
        sPro.proParams[6] = oSRS.GetProjParm(SRS_PP_FALSE_EASTING);
        sPro.proParams[7] = oSRS.GetProjParm(SRS_PP_FALSE_NORTHING);
    }

    // Anything we can't map, we store as an ESRI PE_STRING.
    else if( oSRS.IsProjected() || oSRS.IsGeographic() )
    {
        if( !bPEStringStored )
        {
            char *pszPEString = nullptr;
            const char* const apszOptions[] = { "FORMAT=WKT1_ESRI", nullptr };
            oSRS.exportToWkt(&pszPEString, apszOptions);
            // Need to transform this into ESRI format.
            HFASetPEString(hHFA, pszPEString);
            CPLFree(pszPEString);
            bPEStringStored = true;
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Projection %s not supported for translation to Imagine.",
                 pszProjName);
    }

    // MapInfo
    const char *pszPROJCS = oSRS.GetAttrValue("PROJCS");

    if( pszPROJCS )
        sMapInfo.proName = (char *)pszPROJCS;
    else if( bHaveSRS && sPro.proName != nullptr )
        sMapInfo.proName = sPro.proName;
    else
        sMapInfo.proName = const_cast<char *>("Unknown");

    sMapInfo.upperLeftCenter.x = adfGeoTransform[0] + adfGeoTransform[1] * 0.5;
    sMapInfo.upperLeftCenter.y = adfGeoTransform[3] + adfGeoTransform[5] * 0.5;

    sMapInfo.lowerRightCenter.x =
        adfGeoTransform[0] + adfGeoTransform[1] * (GetRasterXSize() - 0.5);
    sMapInfo.lowerRightCenter.y =
        adfGeoTransform[3] + adfGeoTransform[5] * (GetRasterYSize() - 0.5);

    sMapInfo.pixelSize.width = std::abs(adfGeoTransform[1]);
    sMapInfo.pixelSize.height = std::abs(adfGeoTransform[5]);

    // Handle units.  Try to match up with a known name.
    sMapInfo.units = const_cast<char *>("meters");

    if( bHaveSRS && oSRS.IsGeographic() )
        sMapInfo.units = const_cast<char *>("dd");
    else if( bHaveSRS && oSRS.GetLinearUnits() != 1.0 )
    {
        double dfClosestDiff = 100.0;
        int iClosest = -1;
        const char *pszUnitName = nullptr;
        const double dfActualSize = oSRS.GetLinearUnits(&pszUnitName);

        const char* const* papszUnitMap = HFAGetUnitMap();
        for( int iUnit = 0; papszUnitMap[iUnit] != nullptr; iUnit += 2 )
        {
            if( fabs(CPLAtof(papszUnitMap[iUnit + 1]) - dfActualSize) <
                dfClosestDiff )
            {
                iClosest = iUnit;
                dfClosestDiff =
                    fabs(CPLAtof(papszUnitMap[iUnit+1])-dfActualSize);
            }
        }

        if( iClosest == -1 || fabs(dfClosestDiff / dfActualSize) > 0.0001 )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unable to identify Erdas units matching %s/%gm, "
                     "output units will be wrong.",
                     pszUnitName, dfActualSize);
        }
        else
        {
            sMapInfo.units = (char *)papszUnitMap[iClosest];
        }

        // We need to convert false easting and northing to meters.
        sPro.proParams[6] *= dfActualSize;
        sPro.proParams[7] *= dfActualSize;
    }

    // Write out definitions.
    if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 )
    {
        HFASetMapInfo(hHFA, &sMapInfo);
    }
    else
    {
        HFASetGeoTransform(hHFA,
                           sMapInfo.proName, sMapInfo.units,
                           adfGeoTransform);
    }

    if( bHaveSRS && sPro.proName != nullptr )
    {
        HFASetProParameters(hHFA, &sPro);
        HFASetDatum(hHFA, &sDatum);

        if( !bPEStringStored )
            HFASetPEString(hHFA, "");
    }
    else if( !bPEStringStored )
    {
        ClearSR(hHFA);
    }

    if( poGeogSRS != nullptr )
        delete poGeogSRS;

    return CE_None;
}

/************************************************************************/
/*                       WritePeStringIfNeeded()                        */
/************************************************************************/
int WritePeStringIfNeeded( const OGRSpatialReference* poSRS, HFAHandle hHFA )
{
    if( !poSRS || !hHFA )
        return FALSE;

    const char *pszGEOGCS = poSRS->GetAttrValue("GEOGCS");
    if( pszGEOGCS == nullptr )
        pszGEOGCS = "";

    const char *pszDatum = poSRS->GetAttrValue("DATUM");
    if( pszDatum == nullptr )
        pszDatum = "";

    // The strlen() checks are just there to make Coverity happy because it
    // doesn't seem to realize that STARTS_WITH() success implies them.
    const size_t gcsNameOffset =
         (strlen(pszGEOGCS) > strlen("GCS_") &&
          STARTS_WITH(pszGEOGCS, "GCS_")) ? strlen("GCS_") : 0;

    const size_t datumNameOffset =
        (strlen(pszDatum) > strlen("D_") &&
         STARTS_WITH(pszDatum, "D_")) ? strlen("D_") : 0;

    bool ret = false;
    if( CPLString(pszGEOGCS + gcsNameOffset).replaceAll(' ', '_').tolower() !=
        CPLString(pszDatum + datumNameOffset).replaceAll(' ', '_').tolower() )
    {
        ret = true;
    }
    else
    {
        const char *name = poSRS->GetAttrValue("PRIMEM");
        if( name && !EQUAL(name, "Greenwich") )
            ret = true;

        if( !ret )
        {
            const OGR_SRSNode *poAUnits = poSRS->GetAttrNode("GEOGCS|UNIT");
            const OGR_SRSNode *poChild =
                poAUnits == nullptr ? nullptr : poAUnits->GetChild(0);
            name = poChild == nullptr ? nullptr : poChild->GetValue();
            if( name && !EQUAL(name, "Degree") )
                ret = true;
        }
        if( !ret )
        {
            name = poSRS->GetAttrValue("UNIT");
            if( name )
            {
                ret = true;
                const char* const* papszUnitMap = HFAGetUnitMap();
                for( int i = 0; papszUnitMap[i] != nullptr; i += 2 )
                    if( EQUAL(name, papszUnitMap[i]) )
                        ret = false;
            }
        }
        if( !ret )
        {
            const int nGCS = poSRS->GetEPSGGeogCS();
            switch(nGCS)
            {
            case 4326:
                if( !EQUAL(pszDatum+datumNameOffset, "WGS_84") )
                    ret = true;
                break;
            case 4322:
                if( !EQUAL(pszDatum+datumNameOffset, "WGS_72") )
                    ret = true;
                break;
            case 4267:
                if( !EQUAL(pszDatum+datumNameOffset, "North_America_1927") )
                    ret = true;
                break;
            case 4269:
                if( !EQUAL(pszDatum+datumNameOffset, "North_America_1983") )
                    ret = true;
                break;
            }
        }
    }
    if( ret )
    {
        char *pszPEString = nullptr;
        OGRSpatialReference oSRSForESRI(*poSRS);
        oSRSForESRI.morphToESRI();
        oSRSForESRI.exportToWkt(&pszPEString);
        HFASetPEString(hHFA, pszPEString);
        CPLFree(pszPEString);
    }

    return ret;
}

/************************************************************************/
/*                              ClearSR()                               */
/************************************************************************/
void ClearSR( HFAHandle hHFA )
{
    for( int iBand = 0; iBand < hHFA->nBands; iBand++ )
    {
        HFAEntry *poMIEntry = nullptr;
        if( hHFA->papoBand[iBand]->poNode &&
            (poMIEntry = hHFA->papoBand[iBand]->poNode->
             GetNamedChild("Projection")) != nullptr )
        {
            poMIEntry->MarkDirty();
            poMIEntry->SetIntField("proType", 0);
            poMIEntry->SetIntField("proNumber", 0);
            poMIEntry->SetStringField("proExeName", "");
            poMIEntry->SetStringField("proName", "");
            poMIEntry->SetIntField("proZone", 0);
            poMIEntry->SetDoubleField("proParams[0]", 0.0);
            poMIEntry->SetDoubleField("proParams[1]", 0.0);
            poMIEntry->SetDoubleField("proParams[2]", 0.0);
            poMIEntry->SetDoubleField("proParams[3]", 0.0);
            poMIEntry->SetDoubleField("proParams[4]", 0.0);
            poMIEntry->SetDoubleField("proParams[5]", 0.0);
            poMIEntry->SetDoubleField("proParams[6]", 0.0);
            poMIEntry->SetDoubleField("proParams[7]", 0.0);
            poMIEntry->SetDoubleField("proParams[8]", 0.0);
            poMIEntry->SetDoubleField("proParams[9]", 0.0);
            poMIEntry->SetDoubleField("proParams[10]", 0.0);
            poMIEntry->SetDoubleField("proParams[11]", 0.0);
            poMIEntry->SetDoubleField("proParams[12]", 0.0);
            poMIEntry->SetDoubleField("proParams[13]", 0.0);
            poMIEntry->SetDoubleField("proParams[14]", 0.0);
            poMIEntry->SetStringField("proSpheroid.sphereName", "");
            poMIEntry->SetDoubleField("proSpheroid.a", 0.0);
            poMIEntry->SetDoubleField("proSpheroid.b", 0.0);
            poMIEntry->SetDoubleField("proSpheroid.eSquared", 0.0);
            poMIEntry->SetDoubleField("proSpheroid.radius", 0.0);
            HFAEntry *poDatumEntry = poMIEntry->GetNamedChild("Datum");
            if( poDatumEntry != nullptr )
            {
                poDatumEntry->MarkDirty();
                poDatumEntry->SetStringField("datumname", "");
                poDatumEntry->SetIntField("type", 0);
                poDatumEntry->SetDoubleField("params[0]", 0.0);
                poDatumEntry->SetDoubleField("params[1]", 0.0);
                poDatumEntry->SetDoubleField("params[2]", 0.0);
                poDatumEntry->SetDoubleField("params[3]", 0.0);
                poDatumEntry->SetDoubleField("params[4]", 0.0);
                poDatumEntry->SetDoubleField("params[5]", 0.0);
                poDatumEntry->SetDoubleField("params[6]", 0.0);
                poDatumEntry->SetStringField("gridname", "");
            }
            poMIEntry->FlushToDisk();
            char *peStr = HFAGetPEString(hHFA);
            if( peStr != nullptr && strlen(peStr) > 0)
                HFASetPEString(hHFA, "");
        }
    }
}

/************************************************************************/
/*                           ReadProjection()                           */
/************************************************************************/

CPLErr HFADataset::ReadProjection()

{
    // General case for Erdas style projections.
    //
    // We make a particular effort to adapt the mapinfo->proname as
    // the PROJCS[] name per #2422.
    const Eprj_Datum *psDatum = HFAGetDatum(hHFA);
    const Eprj_ProParameters *psPro = HFAGetProParameters(hHFA);
    const Eprj_MapInfo *psMapInfo = HFAGetMapInfo(hHFA);

    HFAEntry *poMapInformation = nullptr;
    if( psMapInfo == nullptr )
    {
        HFABand *poBand = hHFA->papoBand[0];
        poMapInformation = poBand->poNode->GetNamedChild("MapInformation");
    }

    m_oSRS.Clear();

    if( (psMapInfo == nullptr && poMapInformation == nullptr) ||
        ((!psDatum || strlen(psDatum->datumname) == 0 ||
          EQUAL(psDatum->datumname, "Unknown")) &&
         (!psPro || strlen(psPro->proName) == 0 ||
          EQUAL(psPro->proName, "Unknown")) &&
         (psMapInfo && (strlen(psMapInfo->proName) == 0 ||
                        EQUAL(psMapInfo->proName, "Unknown"))) &&
         (!psPro || psPro->proZone == 0)) )
    {
        return CE_None;
    }

    auto poSRS =
        HFAPCSStructToOSR(psDatum, psPro, psMapInfo, poMapInformation);
    if( poSRS )
        m_oSRS = *poSRS;

    // If we got a valid projection and managed to identify a EPSG code,
    // then do not use the ESRI PE String.
    const bool bTryReadingPEString =
        poSRS == nullptr || poSRS->GetAuthorityCode(nullptr) == nullptr;

    // Special logic for PE string in ProjectionX node.
    char *pszPE_COORDSYS = nullptr;
    if( bTryReadingPEString )
        pszPE_COORDSYS = HFAGetPEString(hHFA);

    OGRSpatialReference oSRSFromPE;
    oSRSFromPE.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( pszPE_COORDSYS != nullptr
        && strlen(pszPE_COORDSYS) > 0
        && oSRSFromPE.importFromWkt(pszPE_COORDSYS) == OGRERR_NONE )
    {
        m_oSRS = oSRSFromPE;

        // Copy TOWGS84 clause from HFA SRS to PE SRS.
        if( poSRS != nullptr )
        {
            double adfCoeffs[7];
            double adfCoeffsUnused[7];
            if( poSRS->GetTOWGS84(adfCoeffs, 7) == OGRERR_NONE &&
                m_oSRS.GetTOWGS84(adfCoeffsUnused, 7) == OGRERR_FAILURE )
            {
                m_oSRS.SetTOWGS84(adfCoeffs[0],
                                adfCoeffs[1],
                                adfCoeffs[2],
                                adfCoeffs[3],
                                adfCoeffs[4],
                                adfCoeffs[5],
                                adfCoeffs[6]);
            }
        }
    }

    CPLFree(pszPE_COORDSYS);

    return m_oSRS.IsEmpty() ? CE_Failure : CE_None;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr HFADataset::IBuildOverviews( const char *pszResampling,
                                    int nOverviews, int *panOverviewList,
                                    int nListBands, int *panBandList,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData )

{
    if( GetAccess() == GA_ReadOnly )
    {
        for( int i = 0; i < nListBands; i++ )
        {
            if( HFAGetOverviewCount(hHFA, panBandList[i]) > 0 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Cannot add external overviews when there are already "
                         "internal overviews");
                return CE_Failure;
            }
        }

        return GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
            pfnProgress, pProgressData);
    }

    for( int i = 0; i < nListBands; i++ )
    {
        void *pScaledProgressData = GDALCreateScaledProgress(
            i * 1.0 / nListBands, (i + 1) * 1.0 / nListBands,
            pfnProgress, pProgressData);

        GDALRasterBand *poBand = GetRasterBand(panBandList[i]);

        // GetRasterBand can return NULL.
        if( poBand == nullptr )
        {
            CPLError(CE_Failure, CPLE_ObjectNull, "GetRasterBand failed");
            GDALDestroyScaledProgress(pScaledProgressData);
            return CE_Failure;
        }

        const CPLErr eErr =
            poBand->BuildOverviews(pszResampling, nOverviews, panOverviewList,
                                   GDALScaledProgress, pScaledProgressData);

        GDALDestroyScaledProgress(pScaledProgressData);

        if( eErr != CE_None )
            return eErr;
    }

    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int HFADataset::Identify( GDALOpenInfo *poOpenInfo )

{
    // Verify that this is a HFA file.
    if( poOpenInfo->nHeaderBytes < 15 ||
        !STARTS_WITH_CI((char *) poOpenInfo->pabyHeader, "EHFA_HEADER_TAG") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HFADataset::Open( GDALOpenInfo * poOpenInfo )

{
    // Verify that this is a HFA file.
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if( !Identify(poOpenInfo) )
        return nullptr;
#endif

    // Open the file.
    HFAHandle hHFA =
      HFAOpen(poOpenInfo->pszFilename,
              (poOpenInfo->eAccess == GA_Update ? "r+" : "r"));

    if( hHFA == nullptr )
        return nullptr;

    // Create a corresponding GDALDataset.
    HFADataset *poDS = new HFADataset();

    poDS->hHFA = hHFA;
    poDS->eAccess = poOpenInfo->eAccess;

    // Establish raster info.
    HFAGetRasterInfo(hHFA, &poDS->nRasterXSize, &poDS->nRasterYSize,
                     &poDS->nBands);

    if( poDS->nBands == 0 )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to open %s, it has zero usable bands.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    if( poDS->nRasterXSize == 0 || poDS->nRasterYSize == 0 )
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to open %s, it has no pixels.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    // Get geotransform, or if that fails, try to find XForms to
    // build gcps, and metadata.
    if( !HFAGetGeoTransform(hHFA, poDS->adfGeoTransform) )
    {
        Efga_Polynomial *pasPolyListForward = nullptr;
        Efga_Polynomial *pasPolyListReverse = nullptr;
        const int nStepCount =
            HFAReadXFormStack(hHFA, &pasPolyListForward, &pasPolyListReverse);

        if( nStepCount > 0 )
        {
            poDS->UseXFormStack(nStepCount,
                                pasPolyListForward,
                                pasPolyListReverse);
            CPLFree(pasPolyListForward);
            CPLFree(pasPolyListReverse);
        }
    }

    poDS->ReadProjection();

    char **papszCM = HFAReadCameraModel(hHFA);

    if( papszCM != nullptr )
    {
        poDS->SetMetadata(papszCM, "CAMERA_MODEL");
        CSLDestroy(papszCM);
    }

    for( int i = 0; i < poDS->nBands; i++ )
    {
        poDS->SetBand(i + 1, new HFARasterBand(poDS, i + 1, -1));
    }

    // Collect GDAL custom Metadata, and "auxiliary" metadata from
    // well known HFA structures for the bands.  We defer this till
    // now to ensure that the bands are properly setup before
    // interacting with PAM.
    for( int i = 0; i < poDS->nBands; i++ )
    {
        HFARasterBand *poBand =
            static_cast<HFARasterBand *>(poDS->GetRasterBand(i + 1));

        char **papszMD = HFAGetMetadata(hHFA, i + 1);
        if( papszMD != nullptr )
        {
            poBand->SetMetadata(papszMD);
            CSLDestroy(papszMD);
        }

        poBand->ReadAuxMetadata();
        poBand->ReadHistogramMetadata();
    }

    // Check for GDAL style metadata.
    char **papszMD = HFAGetMetadata(hHFA, 0);
    if( papszMD != nullptr )
    {
        poDS->SetMetadata(papszMD);
        CSLDestroy(papszMD);
    }

    // Read the elevation metadata, if present.
    for( int iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        HFARasterBand *poBand =
            static_cast<HFARasterBand *>(poDS->GetRasterBand(iBand + 1));
        const char *pszEU = HFAReadElevationUnit(hHFA, iBand);

        if( pszEU != nullptr )
        {
            poBand->SetUnitType(pszEU);
            if( poDS->nBands == 1 )
            {
                poDS->SetMetadataItem("ELEVATION_UNITS", pszEU);
            }
        }
    }

    // Check for dependent dataset value.
    HFAInfo_t *psInfo = hHFA;
    HFAEntry *poEntry = psInfo->poRoot->GetNamedChild("DependentFile");
    if( poEntry != nullptr )
    {
        poDS->SetMetadataItem("HFA_DEPENDENT_FILE",
                              poEntry->GetStringField("dependent.string"),
                              "HFA");
    }

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    // Check for external overviews.
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    // Clear dirty metadata flags.
    for( int i = 0; i < poDS->nBands; i++ )
    {
        HFARasterBand *poBand =
            static_cast<HFARasterBand *>(poDS->GetRasterBand(i + 1));
        poBand->bMetadataDirty = false;
    }
    poDS->bMetadataDirty = false;

    return poDS;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *HFADataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr HFADataset::SetSpatialRef( const OGRSpatialReference* poSRS )

{
    m_oSRS.Clear();
    if( poSRS )
        m_oSRS = *poSRS;
    bGeoDirty = true;

    return CE_None;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr HFADataset::SetMetadata( char **papszMDIn, const char *pszDomain )

{
    bMetadataDirty = true;

    return GDALPamDataset::SetMetadata(papszMDIn, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr HFADataset::SetMetadataItem( const char *pszTag, const char *pszValue,
                                    const char *pszDomain )

{
    bMetadataDirty = true;

    return GDALPamDataset::SetMetadataItem(pszTag, pszValue, pszDomain);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HFADataset::GetGeoTransform( double *padfTransform )

{
    if( adfGeoTransform[0] != 0.0
        || adfGeoTransform[1] != 1.0
        || adfGeoTransform[2] != 0.0
        || adfGeoTransform[3] != 0.0
        || adfGeoTransform[4] != 0.0
        || adfGeoTransform[5] != 1.0 )
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr HFADataset::SetGeoTransform( double * padfTransform )

{
    memcpy(adfGeoTransform, padfTransform, sizeof(double) * 6);
    bGeoDirty = true;

    return CE_None;
}

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      Multi-band raster io handler.  Here we ensure that the block    */
/*      based loading is used for spill file rasters.  That is          */
/*      because they are effectively pixel interleaved, so              */
/*      processing all bands for a given block together avoid extra     */
/*      seeks.                                                          */
/************************************************************************/

CPLErr HFADataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg *psExtraArg )

{
    if( hHFA->papoBand[panBandMap[0] - 1]->fpExternal != nullptr
        && nBandCount > 1 )
        return GDALDataset::BlockBasedRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType,
            nBandCount, panBandMap,
            nPixelSpace, nLineSpace, nBandSpace, psExtraArg);

    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                  pData, nBufXSize, nBufYSize, eBufType,
                                  nBandCount, panBandMap,
                                  nPixelSpace, nLineSpace, nBandSpace,
                                  psExtraArg);
}

/************************************************************************/
/*                           UseXFormStack()                            */
/************************************************************************/

void HFADataset::UseXFormStack( int nStepCount,
                                Efga_Polynomial *pasPLForward,
                                Efga_Polynomial *pasPLReverse )

{
    // Generate GCPs using the transform.
    nGCPCount = 0;
    GDALInitGCPs(36, asGCPList);

    for( double dfYRatio = 0.0; dfYRatio < 1.001; dfYRatio += 0.2 )
    {
        for( double dfXRatio = 0.0; dfXRatio < 1.001; dfXRatio += 0.2 )
        {
            const double dfLine = 0.5 + (GetRasterYSize() - 1) * dfYRatio;
            const double dfPixel = 0.5 + (GetRasterXSize() - 1) * dfXRatio;
            const int iGCP = nGCPCount;

            asGCPList[iGCP].dfGCPPixel = dfPixel;
            asGCPList[iGCP].dfGCPLine = dfLine;

            asGCPList[iGCP].dfGCPX = dfPixel;
            asGCPList[iGCP].dfGCPY = dfLine;
            asGCPList[iGCP].dfGCPZ = 0.0;

            if( HFAEvaluateXFormStack(nStepCount, FALSE, pasPLReverse,
                                      &(asGCPList[iGCP].dfGCPX),
                                      &(asGCPList[iGCP].dfGCPY)) )
                nGCPCount++;
        }
    }

    // Store the transform as metadata.
    GDALMajorObject::SetMetadataItem(
        "XFORM_STEPS", CPLString().Printf("%d", nStepCount), "XFORMS");

    for( int iStep = 0; iStep < nStepCount; iStep++ )
    {
        GDALMajorObject::SetMetadataItem(
            CPLString().Printf("XFORM%d_ORDER", iStep),
            CPLString().Printf("%d", pasPLForward[iStep].order), "XFORMS");

        if( pasPLForward[iStep].order == 1 )
        {
            for( int i = 0; i < 4; i++ )
                GDALMajorObject::SetMetadataItem(
                    CPLString().Printf("XFORM%d_POLYCOEFMTX[%d]", iStep, i),
                    CPLString().Printf("%.15g",
                                       pasPLForward[iStep].polycoefmtx[i]),
                    "XFORMS");

            for( int i = 0; i < 2; i++ )
                GDALMajorObject::SetMetadataItem(
                    CPLString().Printf("XFORM%d_POLYCOEFVECTOR[%d]", iStep, i),
                    CPLString().Printf("%.15g",
                                       pasPLForward[iStep].polycoefvector[i]),
                    "XFORMS");

            continue;
        }

        int nCoefCount = 10;

        if( pasPLForward[iStep].order != 2 )
        {
            CPLAssert(pasPLForward[iStep].order == 3);
            nCoefCount = 18;
        }

        for( int i = 0; i < nCoefCount; i++ )
            GDALMajorObject::SetMetadataItem(
                CPLString().Printf("XFORM%d_FWD_POLYCOEFMTX[%d]", iStep, i),
                CPLString().Printf("%.15g", pasPLForward[iStep].polycoefmtx[i]),
                "XFORMS");

        for( int i = 0; i < 2; i++ )
            GDALMajorObject::SetMetadataItem(
                CPLString().Printf("XFORM%d_FWD_POLYCOEFVECTOR[%d]", iStep, i),
                CPLString().Printf("%.15g",
                                   pasPLForward[iStep].polycoefvector[i]),
                "XFORMS");

        for( int i = 0; i < nCoefCount; i++ )
            GDALMajorObject::SetMetadataItem(
                CPLString().Printf("XFORM%d_REV_POLYCOEFMTX[%d]", iStep, i),
                CPLString().Printf("%.15g", pasPLReverse[iStep].polycoefmtx[i]),
                "XFORMS");

        for( int i = 0; i < 2; i++ )
            GDALMajorObject::SetMetadataItem(
                CPLString().Printf("XFORM%d_REV_POLYCOEFVECTOR[%d]", iStep, i),
                CPLString().Printf("%.15g",
                                   pasPLReverse[iStep].polycoefvector[i]),
                "XFORMS");
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HFADataset::GetGCPCount()
{
    const int nPAMCount = GDALPamDataset::GetGCPCount();
    return nPAMCount > 0 ? nPAMCount : nGCPCount;
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *HFADataset::GetGCPSpatialRef() const

{
    const OGRSpatialReference* poSRS = GDALPamDataset::GetGCPSpatialRef();
    if( poSRS )
        return poSRS;
    return nGCPCount > 0 && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *HFADataset::GetGCPs()
{
    const GDAL_GCP* psPAMGCPs = GDALPamDataset::GetGCPs();
    if( psPAMGCPs )
        return psPAMGCPs;
    return asGCPList;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **HFADataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if( HFAGetIGEFilename(hHFA) != nullptr )
    {
        papszFileList = CSLAddString(papszFileList, HFAGetIGEFilename(hHFA));
    }

    // Request an overview to force opening of dependent overview files.
    if( nBands > 0 && GetRasterBand(1)->GetOverviewCount() > 0 )
        GetRasterBand(1)->GetOverview(0);

    if( hHFA->psDependent != nullptr )
    {
        HFAInfo_t *psDep = hHFA->psDependent;

        papszFileList = CSLAddString(
            papszFileList,
            CPLFormFilename(psDep->pszPath, psDep->pszFilename, nullptr));

        if( HFAGetIGEFilename(psDep) != nullptr )
            papszFileList =
                CSLAddString(papszFileList, HFAGetIGEFilename(psDep));
    }

    return papszFileList;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *HFADataset::Create( const char * pszFilenameIn,
                                 int nXSize, int nYSize, int nBandsIn,
                                 GDALDataType eType,
                                 char ** papszParamList )

{
    const int nBits =
        CSLFetchNameValue(papszParamList, "NBITS") != nullptr
        ? atoi(CSLFetchNameValue(papszParamList, "NBITS"))
        : 0;

    const char *pszPixelType =
        CSLFetchNameValue(papszParamList, "PIXELTYPE");
    if( pszPixelType == nullptr )
        pszPixelType = "";

    // Translate the data type.
    EPTType eHfaDataType;
    switch( eType )
    {
    case GDT_Byte:
        if( nBits == 1 )
            eHfaDataType = EPT_u1;
        else if( nBits == 2 )
            eHfaDataType = EPT_u2;
        else if( nBits == 4 )
            eHfaDataType = EPT_u4;
        else if( EQUAL(pszPixelType, "SIGNEDBYTE") )
            eHfaDataType = EPT_s8;
        else
            eHfaDataType = EPT_u8;
        break;

    case GDT_UInt16:
        eHfaDataType = EPT_u16;
        break;

    case GDT_Int16:
        eHfaDataType = EPT_s16;
        break;

    case GDT_Int32:
        eHfaDataType = EPT_s32;
        break;

    case GDT_UInt32:
        eHfaDataType = EPT_u32;
        break;

    case GDT_Float32:
        eHfaDataType = EPT_f32;
        break;

    case GDT_Float64:
        eHfaDataType = EPT_f64;
        break;

    case GDT_CFloat32:
        eHfaDataType = EPT_c64;
        break;

    case GDT_CFloat64:
        eHfaDataType = EPT_c128;
        break;

    default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Data type %s not supported by Erdas Imagine (HFA) format.",
                 GDALGetDataTypeName(eType));
        return nullptr;
    }

    // Create the new file.
    HFAHandle hHFA = HFACreate(pszFilenameIn, nXSize, nYSize, nBandsIn,
                                eHfaDataType, papszParamList);
    if( hHFA == nullptr )
        return nullptr;

    if( HFAClose(hHFA) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        return nullptr;
    }

    // Open the dataset normally.
    HFADataset *poDS = (HFADataset *)GDALOpen(pszFilenameIn, GA_Update);

    // Special creation option to disable checking for UTM
    // parameters when writing the projection.  This is a special
    // hack for sam.gillingham@nrm.qld.gov.au.
    if( poDS != nullptr )
    {
        poDS->bIgnoreUTM = CPLFetchBool(papszParamList, "IGNOREUTM", false);
    }

    // Sometimes we can improve ArcGIS compatibility by forcing
    // generation of a PEString instead of traditional Imagine
    // coordinate system descriptions.
    if( poDS != nullptr )
    {
        poDS->bForceToPEString =
            CPLFetchBool(papszParamList, "FORCETOPESTRING", false);
    }

    return poDS;
}

/************************************************************************/
/*                               Rename()                               */
/*                                                                      */
/*      Custom Rename() implementation that knows how to update         */
/*      filename references in .img and .aux files.                     */
/************************************************************************/

CPLErr HFADataset::Rename( const char *pszNewName, const char *pszOldName )

{
    // Rename all the files at the filesystem level.
    CPLErr eErr = GDALDriver::DefaultRename(pszNewName, pszOldName);
    if( eErr != CE_None )
        return eErr;

    // Now try to go into the .img file and update RRDNames[] lists.
    CPLString osOldBasename = CPLGetBasename(pszOldName);
    CPLString osNewBasename = CPLGetBasename(pszNewName);

    if( osOldBasename != osNewBasename )
    {
        HFAHandle hHFA = HFAOpen(pszNewName, "r+");

        if( hHFA != nullptr )
        {
            eErr = HFARenameReferences(hHFA, osNewBasename, osOldBasename);

            HFAGetOverviewCount(hHFA, 1);

            if( hHFA->psDependent != nullptr )
                HFARenameReferences(hHFA->psDependent,
                                    osNewBasename, osOldBasename);

            if( HFAClose(hHFA) != 0 )
                eErr = CE_Failure;
        }
    }

    return eErr;
}

/************************************************************************/
/*                             CopyFiles()                              */
/*                                                                      */
/*      Custom CopyFiles() implementation that knows how to update      */
/*      filename references in .img and .aux files.                     */
/************************************************************************/

CPLErr HFADataset::CopyFiles( const char *pszNewName, const char *pszOldName )

{
    // Rename all the files at the filesystem level.
    CPLErr eErr = GDALDriver::DefaultCopyFiles(pszNewName, pszOldName);

    if( eErr != CE_None )
        return eErr;

    // Now try to go into the .img file and update RRDNames[] lists.
    CPLString osOldBasename = CPLGetBasename(pszOldName);
    CPLString osNewBasename = CPLGetBasename(pszNewName);

    if( osOldBasename != osNewBasename )
    {
        HFAHandle hHFA = HFAOpen(pszNewName, "r+");

        if( hHFA != nullptr )
        {
            eErr = HFARenameReferences(hHFA, osNewBasename, osOldBasename);

            HFAGetOverviewCount(hHFA, 1);

            if( hHFA->psDependent != nullptr )
                HFARenameReferences(hHFA->psDependent,
                                    osNewBasename, osOldBasename);

            if( HFAClose(hHFA) != 0 )
                eErr = CE_Failure;
        }
    }

    return eErr;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
HFADataset::CreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                        int /* bStrict */,
                        char **papszOptions,
                        GDALProgressFunc pfnProgress, void *pProgressData )
{
    // Do we really just want to create an .aux file?
    const bool bCreateAux = CPLFetchBool(papszOptions, "AUX", false);

    // Establish a representative data type to use.
    char **papszModOptions = CSLDuplicate(papszOptions);
    if( !pfnProgress(0.0, nullptr, pProgressData) )
    {
        CSLDestroy(papszModOptions);
        return nullptr;
    }

    const int nBandCount = poSrcDS->GetRasterCount();
    GDALDataType eType = GDT_Byte;

    for( int iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand+1);
        eType = GDALDataTypeUnion(eType, poBand->GetRasterDataType());
    }

    // If we have PIXELTYPE metadata in the source, pass it
    // through as a creation option.
    if( CSLFetchNameValue(papszOptions, "PIXELTYPE") == nullptr &&
        nBandCount > 0 &&
        eType == GDT_Byte &&
        poSrcDS->GetRasterBand(1)->GetMetadataItem("PIXELTYPE",
                                                   "IMAGE_STRUCTURE") )
    {
        papszModOptions =
            CSLSetNameValue(papszModOptions, "PIXELTYPE",
                            poSrcDS->GetRasterBand(1)->GetMetadataItem(
                                "PIXELTYPE", "IMAGE_STRUCTURE"));
    }

    HFADataset *poDS =
        (HFADataset *)Create(pszFilename,
                             poSrcDS->GetRasterXSize(),
                             poSrcDS->GetRasterYSize(),
                             nBandCount,
                             eType, papszModOptions);

    CSLDestroy(papszModOptions);

    if( poDS == nullptr )
        return nullptr;

    // Does the source have a PCT or RAT for any of the bands?  If so, copy it over.
    for( int iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand(iBand + 1);

        GDALColorTable *poCT = poBand->GetColorTable();
        if( poCT != nullptr )
        {
            poDS->GetRasterBand(iBand + 1)->SetColorTable(poCT);
        }

        if( poBand->GetDefaultRAT() != nullptr )
            poDS->GetRasterBand(iBand+1)->SetDefaultRAT( poBand->GetDefaultRAT() );
    }

    // Do we have metadata for any of the bands or the dataset as a whole?
    if( poSrcDS->GetMetadata() != nullptr )
        poDS->SetMetadata(poSrcDS->GetMetadata());

    for( int iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand + 1);
        GDALRasterBand *poDstBand = poDS->GetRasterBand(iBand + 1);

        if( poSrcBand->GetMetadata() != nullptr )
            poDstBand->SetMetadata(poSrcBand->GetMetadata());

        if( strlen(poSrcBand->GetDescription()) > 0 )
            poDstBand->SetDescription(poSrcBand->GetDescription());

        int bSuccess = FALSE;
        const double dfNoDataValue = poSrcBand->GetNoDataValue(&bSuccess);
        if( bSuccess )
            poDstBand->SetNoDataValue(dfNoDataValue);
    }

    // Copy projection information.
    double adfGeoTransform[6] = {};

    if( poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None )
        poDS->SetGeoTransform(adfGeoTransform);

    const char *pszProj = poSrcDS->GetProjectionRef();
    if( pszProj != nullptr && strlen(pszProj) > 0 )
        poDS->SetProjection(pszProj);

    // Copy the imagery.
    if( !bCreateAux )
    {
        const CPLErr eErr = GDALDatasetCopyWholeRaster(
            (GDALDatasetH)poSrcDS, (GDALDatasetH)poDS, nullptr, pfnProgress,
            pProgressData);

        if( eErr != CE_None )
        {
            delete poDS;
            return nullptr;
        }
    }

    // Do we want to generate statistics and a histogram?
    if( CPLFetchBool(papszOptions, "STATISTICS", false) )
    {
        for( int iBand = 0; iBand < nBandCount; iBand++ )
        {
            GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand + 1);
            double dfMin = 0.0;
            double dfMax = 0.0;
            double dfMean = 0.0;
            double dfStdDev = 0.0;
            char **papszStatsMD = nullptr;

            // Statistics
            if( poSrcBand->GetStatistics(TRUE, FALSE, &dfMin, &dfMax, &dfMean,
                                         &dfStdDev) == CE_None ||
                poSrcBand->ComputeStatistics(TRUE, &dfMin, &dfMax, &dfMean,
                                             &dfStdDev, pfnProgress,
                                             pProgressData) == CE_None )
            {
                CPLString osValue;

                papszStatsMD =
                    CSLSetNameValue(papszStatsMD, "STATISTICS_MINIMUM",
                                    osValue.Printf("%.15g", dfMin));
                papszStatsMD =
                    CSLSetNameValue(papszStatsMD, "STATISTICS_MAXIMUM",
                                    osValue.Printf("%.15g", dfMax));
                papszStatsMD = CSLSetNameValue(papszStatsMD, "STATISTICS_MEAN",
                                               osValue.Printf("%.15g", dfMean));
                papszStatsMD =
                    CSLSetNameValue(papszStatsMD, "STATISTICS_STDDEV",
                                    osValue.Printf("%.15g", dfStdDev));
            }

            // Histogram
            int nBuckets = 0;
            GUIntBig *panHistogram = nullptr;

            if( poSrcBand->GetDefaultHistogram(&dfMin, &dfMax,
                                               &nBuckets, &panHistogram,
                                               TRUE,
                                               pfnProgress, pProgressData)
                == CE_None )
            {
                CPLString osValue;
                const double dfBinWidth = (dfMax - dfMin) / nBuckets;

                papszStatsMD = CSLSetNameValue(
                    papszStatsMD, "STATISTICS_HISTOMIN",
                    osValue.Printf("%.15g", dfMin + dfBinWidth * 0.5));
                papszStatsMD = CSLSetNameValue(
                    papszStatsMD, "STATISTICS_HISTOMAX",
                    osValue.Printf("%.15g", dfMax - dfBinWidth * 0.5));
                papszStatsMD =
                    CSLSetNameValue(papszStatsMD, "STATISTICS_HISTONUMBINS",
                                    osValue.Printf("%d", nBuckets));

                int nBinValuesLen = 0;
                char *pszBinValues =
                    static_cast<char *>(CPLCalloc(20, nBuckets + 1));
                for( int iBin = 0; iBin < nBuckets; iBin++ )
                {

                    strcat(pszBinValues + nBinValuesLen,
                           osValue.Printf(CPL_FRMT_GUIB, panHistogram[iBin]));
                    strcat(pszBinValues + nBinValuesLen, "|");
                    nBinValuesLen +=
                        static_cast<int>(strlen(pszBinValues + nBinValuesLen));
                }
                papszStatsMD = CSLSetNameValue(
                    papszStatsMD, "STATISTICS_HISTOBINVALUES", pszBinValues);
                CPLFree(pszBinValues);
            }

            CPLFree(panHistogram);

            if( CSLCount(papszStatsMD) > 0 )
                HFASetMetadata(poDS->hHFA, iBand + 1, papszStatsMD);

            CSLDestroy(papszStatsMD);
        }
    }

    // All report completion.
    if( !pfnProgress(1.0, nullptr, pProgressData) )
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        delete poDS;

        GDALDriver *poHFADriver = (GDALDriver *)GDALGetDriverByName("HFA");
        poHFADriver->Delete(pszFilename);
        return nullptr;
    }

    poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_HFA()                          */
/************************************************************************/

void GDALRegister_HFA()

{
    if( GDALGetDriverByName("HFA") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("HFA");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Erdas Imagine Images (.img)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/hfa.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "img");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32 Float32 Float64 "
                              "CFloat32 CFloat64");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='BLOCKSIZE' type='integer' description='tile width/height (32-2048)' default='64'/>"
"   <Option name='USE_SPILL' type='boolean' description='Force use of spill file'/>"
"   <Option name='COMPRESSED' alias='COMPRESS' type='boolean' description='compress blocks'/>"
"   <Option name='PIXELTYPE' type='string' description='By setting this to SIGNEDBYTE, a new Byte file can be forced to be written as signed byte'/>"
"   <Option name='AUX' type='boolean' description='Create an .aux file'/>"
"   <Option name='IGNOREUTM' type='boolean' description='Ignore UTM when selecting coordinate system - will use Transverse Mercator. Only used for Create() method'/>"
"   <Option name='NBITS' type='integer' description='Create file with special sub-byte data type (1/2/4)'/>"
"   <Option name='STATISTICS' type='boolean' description='Generate statistics and a histogram'/>"
"   <Option name='DEPENDENT_FILE' type='string' description='Name of dependent file (must not have absolute path)'/>"
"   <Option name='FORCETOPESTRING' type='boolean' description='Force use of ArcGIS PE String in file instead of Imagine coordinate system format'/>"
"</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = HFADataset::Open;
    poDriver->pfnCreate = HFADataset::Create;
    poDriver->pfnCreateCopy = HFADataset::CreateCopy;
    poDriver->pfnIdentify = HFADataset::Identify;
    poDriver->pfnRename = HFADataset::Rename;
    poDriver->pfnCopyFiles = HFADataset::CopyFiles;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
