/*
 * $Id$
 *  kearat.cpp
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 *  Permission is hereby granted, free of charge, to any person 
 *  obtaining a copy of this software and associated documentation 
 *  files (the "Software"), to deal in the Software without restriction, 
 *  including without limitation the rights to use, copy, modify, 
 *  merge, publish, distribute, sublicense, and/or sell copies of the 
 *  Software, and to permit persons to whom the Software is furnished 
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be 
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR 
 *  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
 *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "kearat.h"

KEARasterAttributeTable::KEARasterAttributeTable(kealib::KEAAttributeTable *poKEATable)
{
    for( size_t nColumnIndex = 0; nColumnIndex < poKEATable->getMaxGlobalColIdx(); nColumnIndex++ )
    {
        kealib::KEAATTField sKEAField;
        try
        {
            sKEAField = poKEATable->getField(nColumnIndex);
        }
        catch(kealib::KEAATTException &e)
        {
            // pKEATable->getField raised exception because we have a missing column
            continue;
        }
        m_aoFields.push_back(sKEAField);
    }
    m_poKEATable = poKEATable;
}

KEARasterAttributeTable::~KEARasterAttributeTable()
{
    // can't just delete thanks to Windows
    kealib::KEAAttributeTable::destroyAttributeTable(m_poKEATable);
}

GDALDefaultRasterAttributeTable *KEARasterAttributeTable::Clone() const
{
    if( ( GetRowCount() * GetColumnCount() ) > RAT_MAX_ELEM_FOR_CLONE )
        return NULL;

    GDALDefaultRasterAttributeTable *poRAT = new GDALDefaultRasterAttributeTable();

    for( int iCol = 0; iCol < (int)m_aoFields.size(); iCol++)
    {
        CPLString sName = m_aoFields[iCol].name;
        CPLString sUsage = m_aoFields[iCol].usage;
        GDALRATFieldUsage eGDALUsage;
        if( sUsage == "PixelCount" )
            eGDALUsage = GFU_PixelCount;
        else if( sUsage == "Name" )
            eGDALUsage = GFU_Name;
        else if( sUsage == "Red" )
            eGDALUsage = GFU_Red;
        else if( sUsage == "Green" )
            eGDALUsage = GFU_Green;
        else if( sUsage == "Blue" )
            eGDALUsage = GFU_Blue;
        else if( sUsage == "Alpha" )
            eGDALUsage = GFU_Alpha;
        else
        {
            // don't recognise any other special names - generic column
            eGDALUsage = GFU_Generic;
        }

        GDALRATFieldType eGDALType;
        switch( m_aoFields[iCol].dataType )
        {
            case kealib::kea_att_bool:
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
        poRAT->CreateColumn(sName, eGDALType, eGDALUsage);
        poRAT->SetRowCount(m_poKEATable->getSize());
        
        if( m_poKEATable->getSize() == 0 )
            continue;

        if( eGDALType == GFT_Integer )
        {
            int *panColData = (int*)VSIMalloc2(sizeof(int), m_poKEATable->getSize());
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::Clone");
                delete poRAT;
                return NULL;
            }

            if( (const_cast<KEARasterAttributeTable*>(this))->
                        ValuesIO(GF_Read, iCol, 0, m_poKEATable->getSize(), panColData ) != CE_None )
            {
                CPLFree(panColData);
                delete poRAT;
                return NULL;
            }           

            for( int iRow = 0; iRow < (int)m_poKEATable->getSize(); iRow++ )
            {
                poRAT->SetValue(iRow, iCol, panColData[iRow]);            
            }
            CPLFree(panColData);
        }
        if( eGDALType == GFT_Real )
        {
            double *padfColData = (double*)VSIMalloc2(sizeof(double), m_poKEATable->getSize());
            if( padfColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::Clone");
                delete poRAT;
                return NULL;
            }
            if( (const_cast<KEARasterAttributeTable*>(this))->
                        ValuesIO(GF_Read, iCol, 0, m_poKEATable->getSize(), padfColData ) != CE_None )
            {
                CPLFree(padfColData);
                delete poRAT;
                return NULL;
            }           

            for( int iRow = 0; iRow < (int)m_poKEATable->getSize(); iRow++ )
            {
                poRAT->SetValue(iRow, iCol, padfColData[iRow]);            
            }
            CPLFree(padfColData);
        }
        if( eGDALType == GFT_String )
        {
            char **papszColData = (char**)VSIMalloc2(sizeof(char*), m_poKEATable->getSize());
            if( papszColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::Clone");
                delete poRAT;
                return NULL;
            }

            if( (const_cast<KEARasterAttributeTable*>(this))->
                    ValuesIO(GF_Read, iCol, 0, m_poKEATable->getSize(), papszColData ) != CE_None )
            {
                CPLFree(papszColData);
                delete poRAT;
                return NULL;
            }           

            for( int iRow = 0; iRow < (int)m_poKEATable->getSize(); iRow++ )
            {
                poRAT->SetValue(iRow, iCol, papszColData[iRow]);
                CPLFree(papszColData[iRow]);
            }
            CPLFree(papszColData);
        }
    }
    return poRAT;
}


int KEARasterAttributeTable::GetColumnCount() const
{
    return (int)m_aoFields.size();
}


const char *KEARasterAttributeTable::GetNameOfCol(int nCol) const
{
     if( ( nCol < 0 ) || ( nCol >= (int)m_aoFields.size() ) )
        return NULL;

    return m_aoFields[nCol].name.c_str();
}

GDALRATFieldUsage KEARasterAttributeTable::GetUsageOfCol( int nCol ) const
{
    if( ( nCol < 0 ) || ( nCol >= (int)m_aoFields.size() ) )
        return GFU_Generic;

    GDALRATFieldUsage eGDALUsage;
    std::string keausage = m_aoFields[nCol].usage;    

    if( keausage == "PixelCount" )
        eGDALUsage = GFU_PixelCount;
    else if( keausage == "Name" )
        eGDALUsage = GFU_Name;
    else if( keausage == "Red" )
        eGDALUsage = GFU_Red;
    else if( keausage == "Green" )
        eGDALUsage = GFU_Green;
    else if( keausage == "Blue" )
        eGDALUsage = GFU_Blue;
    else if( keausage == "Alpha" )
        eGDALUsage = GFU_Alpha;
    else
    {
        // don't recognise any other special names - generic column
        eGDALUsage = GFU_Generic;
    }

    return eGDALUsage;
}

GDALRATFieldType KEARasterAttributeTable::GetTypeOfCol( int nCol ) const
{
    if( ( nCol < 0 ) || ( nCol >= (int)m_aoFields.size() ) )
        return GFT_Integer;

    GDALRATFieldType eGDALType;
    switch( m_aoFields[nCol].dataType )
    {
        case kealib::kea_att_bool:
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


int KEARasterAttributeTable::GetColOfUsage( GDALRATFieldUsage eUsage ) const
{
    unsigned int i;

    std::string keausage;
    switch(eUsage)
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

    for( i = 0; i < m_aoFields.size(); i++ )
    {
        if( m_aoFields[i].usage == keausage )
            return i;
    }
    return -1;
}

int KEARasterAttributeTable::GetRowCount() const
{
    return (int)m_poKEATable->getSize();
}

const char *KEARasterAttributeTable::GetValueAsString( int iRow, int iField ) const
{
    // Get ValuesIO do do the work
    char *apszStrList[1];
    if( (const_cast<KEARasterAttributeTable*>(this))->
                ValuesIO(GF_Read, iField, iRow, 1, apszStrList ) != CPLE_None )
    {
        return "";
    }

    const_cast<KEARasterAttributeTable*>(this)->osWorkingResult = apszStrList[0];
    CPLFree(apszStrList[0]);

    return osWorkingResult;
}

int KEARasterAttributeTable::GetValueAsInt( int iRow, int iField ) const
{
    // Get ValuesIO do do the work
    int nValue;
    if( (const_cast<KEARasterAttributeTable*>(this))->
                ValuesIO(GF_Read, iField, iRow, 1, &nValue ) != CE_None )
    {
        return 0;
    }

    return nValue;
}

double KEARasterAttributeTable::GetValueAsDouble( int iRow, int iField ) const
{
    // Get ValuesIO do do the work
    double dfValue;
    if( (const_cast<KEARasterAttributeTable*>(this))->
                ValuesIO(GF_Read, iField, iRow, 1, &dfValue ) != CE_None )
    {
        return 0;
    }

    return dfValue;
}

void KEARasterAttributeTable::SetValue( int iRow, int iField, const char *pszValue )
{
    // Get ValuesIO do do the work
    ValuesIO(GF_Write, iField, iRow, 1, const_cast<char**>(&pszValue) );
}

void KEARasterAttributeTable::SetValue( int iRow, int iField, double dfValue)
{
    // Get ValuesIO do do the work
    ValuesIO(GF_Write, iField, iRow, 1, &dfValue );
}

void KEARasterAttributeTable::SetValue( int iRow, int iField, int nValue )
{
    // Get ValuesIO do do the work
    ValuesIO(GF_Write, iField, iRow, 1, &nValue );
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, double *pdfData)
{
    /*if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/

    if( iField < 0 || iField >= (int) m_aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return CE_Failure;
    }

    if( iStartRow < 0 || (iStartRow+iLength) > (int)m_poKEATable->getSize() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iStartRow (%d) + iLength(%d) out of range.", iStartRow, iLength );

        return CE_Failure;
    }

    switch( m_aoFields[iField].dataType )
    {
        case kealib::kea_att_bool:
        case kealib::kea_att_int:
        {
            // allocate space for ints
            int *panColData = (int*)VSIMalloc2(iLength, sizeof(int) );
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied doubles to ints
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = pdfData[i];
            }

            // do the ValuesIO as ints
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData );
            if( eVal != CE_None )
            {
                CPLFree(panColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // copy them back to doubles
                for( int i = 0; i < iLength; i++ )
                    pdfData[i] = panColData[i];
            }

            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_float:
        {
            try
            {
                if( eRWFlag == GF_Read )
                    m_poKEATable->getFloatFields(iStartRow, iLength, m_aoFields[iField].idx, pdfData);
                else
                    m_poKEATable->setFloatFields(iStartRow, iLength, m_aoFields[iField].idx, pdfData);
            }
            catch(kealib::KEAException &e)
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Failed to read/write attribute table: %s", e.what() );
                return CE_Failure;
            }
        }
        break;
        case kealib::kea_att_string:
        {
            // allocate space for string pointers
            char **papszColData = (char**)VSIMalloc2(iLength, sizeof(char*));
            if( papszColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied doubles to strings
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf( "%.16g", pdfData[i] );
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // do the ValuesIO as strings
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData );
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
                // copy them back to doubles
                for( int i = 0; i < iLength; i++ )
                    pdfData[i] = atof(papszColData[i]);
            }

            // either we allocated them for write, or they were allocated
            // by ValuesIO on read
            for( int i = 0; i < iLength; i++ )
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
        default:
            break;
    }
    return CE_None;
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, int *pnData)
{
    /*if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/

    if( iField < 0 || iField >= (int) m_aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return CE_Failure;
    }

    if( iStartRow < 0 || (iStartRow+iLength) > (int)m_poKEATable->getSize() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iStartRow (%d) + iLength(%d) out of range.", iStartRow, iLength );

        return CE_Failure;
    }

    switch( m_aoFields[iField].dataType )
    {
        case kealib::kea_att_bool:
        {
            // need to convert to/from bools
            bool *panColData = (bool*)VSIMalloc2(iLength, sizeof(bool) );
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied ints to bools
                for( int i = 0; i < iLength; i++ )
                {
                    panColData[i] = (pnData[i] != 0);
                }
            }

            try
            {
                if( eRWFlag == GF_Read )
                    m_poKEATable->getBoolFields(iStartRow, iLength, m_aoFields[iField].idx, panColData);
                else
                    m_poKEATable->setBoolFields(iStartRow, iLength, m_aoFields[iField].idx, panColData);
            }
            catch(kealib::KEAException &e)
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Failed to read/write attribute table: %s", e.what() );
                return CE_Failure;
            }

            if( eRWFlag == GF_Read )
            {
                // copy them back to ints
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = panColData[i]? 1 : 0;
            }
            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_int:
        {
            // need to convert to/from int64_t
            int64_t *panColData = (int64_t*)VSIMalloc2(iLength, sizeof(int64_t) );
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied ints to int64t
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = pnData[i];
            }

            try
            {
                if( eRWFlag == GF_Read )
                    m_poKEATable->getIntFields(iStartRow, iLength, m_aoFields[iField].idx, panColData);
                else
                    m_poKEATable->setIntFields(iStartRow, iLength, m_aoFields[iField].idx, panColData);
            }
            catch(kealib::KEAException &e)
            {
                fprintf(stderr,"Failed to read/write attribute table: %s %d %d %ld\n", e.what(), iStartRow, iLength, m_poKEATable->getSize() );
                CPLError( CE_Failure, CPLE_AppDefined, "Failed to read/write attribute table: %s", e.what() );
                return CE_Failure;
            }

            if( eRWFlag == GF_Read )
            {
                // copy them back to ints
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = panColData[i];
            }
            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_float:
        {
            // allocate space for doubles
            double *padfColData = (double*)VSIMalloc2(iLength, sizeof(double) );
            if( padfColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied ints to doubles
                for( int i = 0; i < iLength; i++ )
                    padfColData[i] = pnData[i];
            }

            // do the ValuesIO as doubles
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData );
            if( eVal != CE_None )
            {
                CPLFree(padfColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // copy them back to ints
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = padfColData[i];
            }

            CPLFree(padfColData);
        }
        break;
        case kealib::kea_att_string:
        {
            // allocate space for string pointers
            char **papszColData = (char**)VSIMalloc2(iLength, sizeof(char*));
            if( papszColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }

            if( eRWFlag == GF_Write )
            {
                // copy the application supplied ints to strings
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf( "%d", pnData[i] );
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // do the ValuesIO as strings
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData );
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
                // copy them back to ints
                for( int i = 0; i < iLength; i++ )
                    pnData[i] = atol(papszColData[i]);
            }

            // either we allocated them for write, or they were allocated
            // by ValuesIO on read
            for( int i = 0; i < iLength; i++ )
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
        default:
            break;
    }
    return CE_None;
}

CPLErr KEARasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, char **papszStrList)
{
    /*if( ( eRWFlag == GF_Write ) && ( this->eAccess == GA_ReadOnly ) )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/

    if( iField < 0 || iField >= (int) m_aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return CE_Failure;
    }

    if( iStartRow < 0 || (iStartRow+iLength) > (int)m_poKEATable->getSize() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iStartRow (%d) + iLength(%d) out of range.", iStartRow, iLength );

        return CE_Failure;
    }

    switch( m_aoFields[iField].dataType )
    {
        case kealib::kea_att_bool:
        case kealib::kea_att_int:
        {
            // allocate space for ints
            int *panColData = (int*)VSIMalloc2(iLength, sizeof(int) );
            if( panColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }
            if( eRWFlag == GF_Write )
            {
                // convert user supplied strings to ints
                for( int i = 0; i < iLength; i++ )
                    panColData[i] = atol(papszStrList[i]);
            }

            // call values IO to read/write ints
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData);
            if( eVal != CE_None )
            {
                CPLFree(panColData);
                return eVal;
            }


            if( eRWFlag == GF_Read )
            {
                // convert ints back to strings
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf( "%d", panColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(panColData);
        }
        break;
        case kealib::kea_att_float:
        {
            // allocate space for doubles
            double *padfColData = (double*)VSIMalloc2(iLength, sizeof(double) );
            if( padfColData == NULL )
            {
                CPLError( CE_Failure, CPLE_OutOfMemory,
                    "Memory Allocation failed in KEARasterAttributeTable::ValuesIO");
                return CE_Failure;
            }
            
            if( eRWFlag == GF_Write )
            {
                // convert user supplied strings to doubles
                for( int i = 0; i < iLength; i++ )
                    padfColData[i] = atof(papszStrList[i]);
            }

            // call value IO to read/write doubles
            CPLErr eVal = ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData);
            if( eVal != CE_None )
            {
                CPLFree(padfColData);
                return eVal;
            }

            if( eRWFlag == GF_Read )
            {
                // convert doubles back to strings
                for( int i = 0; i < iLength; i++ )
                {
                    osWorkingResult.Printf( "%.16g", padfColData[i]);
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
                if( eRWFlag == GF_Read )
                {
                    std::vector<std::string> aStrings;
                    m_poKEATable->getStringFields(iStartRow, iLength, m_aoFields[iField].idx, &aStrings);
                    for( std::vector<std::string>::size_type i = 0; i < aStrings.size(); i++ )
                    {
                        // Copy using CPLStrdup so user can call CPLFree
                        papszStrList[i] = CPLStrdup(aStrings[i].c_str());
                    }
                }
                else
                {
                    // need to convert to a vector first
                    std::vector<std::string> aStrings;
                    for( int i = 0; i < iLength; i++ )
                    {
                        aStrings.push_back(papszStrList[i]);
                    }
                    m_poKEATable->setStringFields(iStartRow, iLength, m_aoFields[iField].idx, &aStrings);
                }
            }
            catch(kealib::KEAException &e)
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Failed to read/write attribute table: %s", e.what() );
                return CE_Failure;
            }
        }
        break;
        default:
            break;
    }
    return CE_None;
}

int KEARasterAttributeTable::ChangesAreWrittenToFile()
{
    return TRUE;
}

void KEARasterAttributeTable::SetRowCount( int iCount )
{
    /*if( this->eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return;
    }*/

    if( iCount > (int)m_poKEATable->getSize() )
    {
        m_poKEATable->addRows(iCount - m_poKEATable->getSize());
    }
    // can't shrink
}

CPLErr KEARasterAttributeTable::CreateColumn( const char *pszFieldName, 
                                GDALRATFieldType eFieldType, 
                                GDALRATFieldUsage eFieldUsage )
{
    /*if( this->eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
            "Dataset not open in update mode");
        return CE_Failure;
    }*/

    std::string strUsage = "Generic";
    switch(eFieldUsage)
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
        if(eFieldType == GFT_Integer)
        {
            m_poKEATable->addAttIntField(pszFieldName, 0, strUsage);
        }
        else if(eFieldType == GFT_Real)
        {
            m_poKEATable->addAttFloatField(pszFieldName, 0, strUsage);
        }
        else
        {
            m_poKEATable->addAttStringField(pszFieldName, "", strUsage);
        }

        // assume we can just grab this now
        kealib::KEAATTField sKEAField = m_poKEATable->getField(pszFieldName);
        m_aoFields.push_back(sKEAField);
    }
    catch(kealib::KEAException &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Failed to add column: %s", e.what() );
        return CE_Failure;
    }

    return CE_None;
}

CPLXMLNode *KEARasterAttributeTable::Serialize() const
{
    if( ( GetRowCount() * GetColumnCount() ) > RAT_MAX_ELEM_FOR_CLONE )
        return NULL;

    return GDALRasterAttributeTable::Serialize();
}
