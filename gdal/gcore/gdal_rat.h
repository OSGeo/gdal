/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  GDALRasterAttributeTable class declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef GDAL_RAT_H_INCLUDED
#define GDAL_RAT_H_INCLUDED

#include "cpl_minixml.h"

/************************************************************************/
/*                       GDALRasterAttributeField                       */
/*                                                                      */
/*      (private)                                                       */
/************************************************************************/

class GDALRasterAttributeField
{
public:
    CPLString         sName;

    GDALRATFieldType  eType;

    GDALRATFieldUsage eUsage;

    std::vector<GInt32> anValues;
    std::vector<double> adfValues;
    std::vector<CPLString> aosValues;
};

/************************************************************************/
/*                       GDALRasterAttributeTable                       */
/************************************************************************/

//! Raster Attribute Table container.

class CPL_DLL GDALRasterAttributeTable 
{
    friend const char * CPL_STDCALL GDALRATGetNameOfCol( GDALRasterAttributeTableH, int );
    friend const char * CPL_STDCALL GDALRATGetValueAsString( GDALRasterAttributeTableH, int, int );

private:
    std::vector<GDALRasterAttributeField> aoFields;

    int bLinearBinning;
    double dfRow0Min;
    double dfBinSize;

    void  AnalyseColumns();
    int   bColumnsAnalysed;
    int   nMinCol;
    int   nMaxCol;

    int   nRowCount;

    CPLString     osWorkingResult;

public:
    GDALRasterAttributeTable();
    GDALRasterAttributeTable(const GDALRasterAttributeTable&);
    ~GDALRasterAttributeTable();

    GDALRasterAttributeTable *Clone() const;
    
    int           GetColumnCount() const;

    const char   *GetNameOfCol( int ) const;
    GDALRATFieldUsage GetUsageOfCol( int ) const;
    GDALRATFieldType GetTypeOfCol( int ) const;
    
    int           GetColOfUsage( GDALRATFieldUsage ) const;

    int           GetRowCount() const;

    const char   *GetValueAsString( int iRow, int iField ) const;
    int           GetValueAsInt( int iRow, int iField ) const;
    double        GetValueAsDouble( int iRow, int iField ) const;

    void          SetValue( int iRow, int iField, const char *pszValue );
    void          SetValue( int iRow, int iField, double dfValue);
    void          SetValue( int iRow, int iField, int nValue );
    void          SetRowCount( int iCount );

    int           GetRowOfValue( double dfValue ) const;
    int           GetRowOfValue( int nValue ) const;
    int           GetColorOfValue( double dfValue, GDALColorEntry *psEntry ) const;

    double        GetRowMin( int iRow ) const;
    double        GetRowMax( int iRow ) const;

    CPLErr        CreateColumn( const char *pszFieldName, 
                                GDALRATFieldType eFieldType, 
                                GDALRATFieldUsage eFieldUsage );
    CPLErr        SetLinearBinning( double dfRow0Min, double dfBinSize );
    int           GetLinearBinning( double *pdfRow0Min, double *pdfBinSize ) const;

    CPLXMLNode   *Serialize() const;
    CPLErr        XMLInit( CPLXMLNode *, const char * );

    CPLErr        InitializeFromColorTable( const GDALColorTable * );
    GDALColorTable *TranslateToColorTable( int nEntryCount = -1 );
    
    void          DumpReadable( FILE * = NULL );
};

#endif /* ndef GDAL_RAT_H_INCLUDED */
