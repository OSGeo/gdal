/*
 * $Id$
 *  kearat.h
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

#ifndef KEARAT_H
#define KEARAT_H

#include "gdal_priv.h"
#include "gdal_rat.h"

#include "keaband.h"

class KEARasterAttributeTable : public GDALRasterAttributeTable
{
private:
    kealib::KEAAttributeTable *m_poKEATable;
    std::vector<kealib::KEAATTField> m_aoFields;
    CPLString osWorkingResult;

public:
    KEARasterAttributeTable(kealib::KEAAttributeTable *poKEATable);
    ~KEARasterAttributeTable();

    GDALDefaultRasterAttributeTable *Clone() const;

    virtual int           GetColumnCount() const;

    virtual const char   *GetNameOfCol( int ) const;
    virtual GDALRATFieldUsage GetUsageOfCol( int ) const;
    virtual GDALRATFieldType GetTypeOfCol( int ) const;

    virtual int           GetColOfUsage( GDALRATFieldUsage ) const;

    virtual int           GetRowCount() const;

    virtual const char   *GetValueAsString( int iRow, int iField ) const;
    virtual int           GetValueAsInt( int iRow, int iField ) const;
    virtual double        GetValueAsDouble( int iRow, int iField ) const;

    virtual void          SetValue( int iRow, int iField, const char *pszValue );
    virtual void          SetValue( int iRow, int iField, double dfValue);
    virtual void          SetValue( int iRow, int iField, int nValue );

    virtual CPLErr        ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, double *pdfData);
    virtual CPLErr        ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, int *pnData);
    virtual CPLErr        ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, char **papszStrList);

    virtual int           ChangesAreWrittenToFile();
    virtual void          SetRowCount( int iCount );

    virtual CPLErr        CreateColumn( const char *pszFieldName,
                                GDALRATFieldType eFieldType,
                                GDALRATFieldUsage eFieldUsage );

    virtual CPLXMLNode   *Serialize() const;
};

#endif //KEARAT_H
