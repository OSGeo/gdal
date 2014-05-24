/******************************************************************************
 * $Id$
 *
 * Project:  XLS Translator
 * Purpose:  Implements OGRXLSLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <freexl.h>

#include "ogr_xls.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRXLSLayer()                             */
/************************************************************************/

OGRXLSLayer::OGRXLSLayer( OGRXLSDataSource* poDSIn,
                          const char* pszSheetname,
                          int iSheetIn,
                          int nRowsIn,
                          unsigned short nColsIn )

{
    poDS = poDSIn;
    iSheet = iSheetIn;
    nNextFID = 0;
    bFirstLineIsHeaders = FALSE;
    poFeatureDefn = NULL;
    pszName = CPLStrdup(pszSheetname);
    nRows = nRowsIn;
    nCols = nColsIn;
    SetDescription( pszName );
}

/************************************************************************/
/*                            ~OGRXLSLayer()                            */
/************************************************************************/

OGRXLSLayer::~OGRXLSLayer()

{
    CPLFree(pszName);
    if (poFeatureDefn)
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRXLSLayer::ResetReading()

{
    if (poFeatureDefn != NULL)
    {
        nNextFID = bFirstLineIsHeaders ? 1 : 0;
    }
}

/************************************************************************/
/*                          DetectHeaderLine()                          */
/************************************************************************/

void OGRXLSLayer::DetectHeaderLine(const void* xlshandle)

{
    unsigned short i;
    FreeXL_CellValue sCellValue;
    int nCountTextOnSecondLine = 0;
    for(i = 0; i < nCols && nRows >= 2; i ++)
    {
        if (freexl_get_cell_value(xlshandle, 0, i, &sCellValue) == FREEXL_OK)
        {
            if (sCellValue.type != FREEXL_CELL_TEXT &&
                sCellValue.type != FREEXL_CELL_SST_TEXT)
            {
                /* If the values in the first line are not text, then it is */
                /* not a header line */
                break;
            }
        }
        if (freexl_get_cell_value(xlshandle, 1, i, &sCellValue) == FREEXL_OK)
        {
            if (sCellValue.type == FREEXL_CELL_TEXT ||
                sCellValue.type == FREEXL_CELL_SST_TEXT)
            {
                /* If there are only text values on the second line, then we cannot */
                /* know if it is a header line or just a regular line */
                nCountTextOnSecondLine ++;
            }
        }
    }

    const char* pszXLSHeaders = CPLGetConfigOption("OGR_XLS_HEADERS", "");
    if (EQUAL(pszXLSHeaders, "FORCE"))
        bFirstLineIsHeaders = TRUE;
    else if (EQUAL(pszXLSHeaders, "DISABLE"))
        bFirstLineIsHeaders = FALSE;
    else if (i == nCols && nCountTextOnSecondLine != nCols)
        bFirstLineIsHeaders = TRUE;
}

/************************************************************************/
/*                         DetectColumnTypes()                          */
/************************************************************************/

void OGRXLSLayer::DetectColumnTypes(const void* xlshandle,
                                    int* paeFieldTypes)

{
    int j;
    unsigned short i;
    FreeXL_CellValue sCellValue;
    for(j = bFirstLineIsHeaders ? 1 : 0; j < nRows; j ++)
    {
        for(i = 0; i < nCols; i ++)
        {
            if (freexl_get_cell_value(xlshandle, j, i, &sCellValue) == FREEXL_OK)
            {
                OGRFieldType eType = (OGRFieldType) paeFieldTypes[i];
                switch (sCellValue.type)
                {
                    case FREEXL_CELL_INT:
                        eType = OFTInteger;
                        break;
                    case FREEXL_CELL_DOUBLE:
                        eType = OFTReal;
                        break;
                    case FREEXL_CELL_TEXT:
                    case FREEXL_CELL_SST_TEXT:
                        eType = OFTString;
                        break;
                    case FREEXL_CELL_DATE:
                        eType = OFTDate;
                        break;
                    case FREEXL_CELL_DATETIME:
                        eType = OFTDateTime;
                        break;
                    case FREEXL_CELL_TIME:
                        eType = OFTTime;
                        break;
                    case FREEXL_CELL_NULL:
                        break;
                    default:
                        break;
                }

                if (paeFieldTypes[i] < 0)
                {
                    paeFieldTypes[i] = (int) eType;
                }
                else if ((int)eType != paeFieldTypes[i])
                {
                    if ((paeFieldTypes[i] == OFTDate ||
                         paeFieldTypes[i] == OFTTime ||
                         paeFieldTypes[i] == OFTDateTime) &&
                        (eType == OFTDate || eType == OFTTime || eType == OFTDateTime))
                        paeFieldTypes[i] = OFTDateTime;
                    else if (paeFieldTypes[i] == OFTReal && eType == OFTInteger)
                        /* nothing */ ;
                    else if (paeFieldTypes[i] == OFTInteger && eType == OFTReal)
                        paeFieldTypes[i] = OFTReal;
                    else
                        paeFieldTypes[i] = OFTString;
                }
            }
        }
    }
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRXLSLayer::GetLayerDefn()
{
    if (poFeatureDefn)
        return poFeatureDefn;

    poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    const void* xlshandle = poDS->GetXLSHandle();
    if (xlshandle == NULL)
        return poFeatureDefn;

    freexl_select_active_worksheet(xlshandle, iSheet);

    if (nRows > 0)
    {
        unsigned short i;
        FreeXL_CellValue sCellValue;

        DetectHeaderLine(xlshandle);

        int* paeFieldTypes = (int* )
                            CPLMalloc(nCols * sizeof(int));
        for(i = 0; i < nCols; i ++)
        {
            paeFieldTypes[i] = -1;
        }

        const char* pszXLSFieldTypes =
                        CPLGetConfigOption("OGR_XLS_FIELD_TYPES", "");
        if (!EQUAL(pszXLSFieldTypes, "STRING"))
            DetectColumnTypes(xlshandle, paeFieldTypes);

        for(i = 0; i < nCols; i ++)
        {
            OGRFieldType eType = (OGRFieldType) paeFieldTypes[i];
            if (paeFieldTypes[i] < 0)
                eType = OFTString;
            if (bFirstLineIsHeaders &&
                freexl_get_cell_value(xlshandle, 0, i, &sCellValue) == FREEXL_OK &&
                (sCellValue.type == FREEXL_CELL_TEXT ||
                 sCellValue.type == FREEXL_CELL_SST_TEXT))
            {
                OGRFieldDefn oField(sCellValue.value.text_value, eType);
                poFeatureDefn->AddFieldDefn(&oField);
            }
            else
            {
                OGRFieldDefn oField(CPLSPrintf("Field%d", i+1),  eType);
                poFeatureDefn->AddFieldDefn(&oField);
            }
        }

        CPLFree(paeFieldTypes);

    }

    ResetReading();

    return poFeatureDefn;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRXLSLayer::GetFeatureCount( int bForce )
{
    if  ( m_poAttrQuery == NULL /* && m_poFilterGeom == NULL */ )
    {
        const char* pszXLSHeaders = CPLGetConfigOption("OGR_XLS_HEADERS", "");
        if(EQUAL(pszXLSHeaders, "DISABLE"))
            return nRows;

        GetLayerDefn();
        return bFirstLineIsHeaders ? nRows - 1 : nRows;
    }

    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRXLSLayer::GetNextFeature()
{
    GetLayerDefn();

    OGRFeature  *poFeature;

    while(TRUE)
    {
        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if(/*(m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && */ (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRXLSLayer::GetNextRawFeature()
{
    if (nNextFID == nRows)
        return NULL;

    const void* xlshandle = poDS->GetXLSHandle();
    if (xlshandle == NULL)
        return NULL;

    freexl_select_active_worksheet(xlshandle, iSheet);

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    FreeXL_CellValue sCellValue;
    for(unsigned short i=0;i<(unsigned short )poFeatureDefn->GetFieldCount(); i++)
    {
        if (freexl_get_cell_value(xlshandle, nNextFID, i, &sCellValue) == FREEXL_OK)
        {
            switch (sCellValue.type)
            {
                case FREEXL_CELL_INT:
                    poFeature->SetField(i, sCellValue.value.int_value);
                    break;
                case FREEXL_CELL_DOUBLE:
                    poFeature->SetField(i, sCellValue.value.double_value);
                    break;
                case FREEXL_CELL_TEXT:
                case FREEXL_CELL_SST_TEXT:
                    poFeature->SetField(i, sCellValue.value.text_value);
                    break;
                case FREEXL_CELL_DATE:
                case FREEXL_CELL_DATETIME:
                case FREEXL_CELL_TIME:
                    poFeature->SetField(i, sCellValue.value.text_value);
                    break;
                case FREEXL_CELL_NULL:
                    break;
                default:
                    CPLDebug("XLS", "Unknown cell type = %d", sCellValue.type);
                    break;
            }
        }
    }

    poFeature->SetFID(nNextFID + 1);
    nNextFID ++;

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRXLSLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return m_poAttrQuery == NULL /* && m_poFilterGeom == NULL */;

    return FALSE;
}

