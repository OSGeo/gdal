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
#include "gdal_priv.h"

// Clone and Serialize are allowed to fail if GetRowCount()*GetColCount()
// greater than this number
#define RAT_MAX_ELEM_FOR_CLONE  1000000

/************************************************************************/
/*                       GDALRasterAttributeTable                       */
/************************************************************************/

//! Raster Attribute Table interface.
class GDALDefaultRasterAttributeTable;

class CPL_DLL GDALRasterAttributeTable
{
public:
    virtual ~GDALRasterAttributeTable();
    /**
     * \brief Copy Raster Attribute Table
     *
     * Creates a new copy of an existing raster attribute table.  The new copy
     * becomes the responsibility of the caller to destroy.
     * May fail (return nullptr) if the attribute table is too large to clone
     * (GetRowCount() * GetColCount() > RAT_MAX_ELEM_FOR_CLONE)
     *
     * This method is the same as the C function GDALRATClone().
     *
     * @return new copy of the RAT as an in-memory implementation.
     */
    virtual GDALRasterAttributeTable *Clone() const = 0;

    /**
     * \brief Fetch table column count.
     *
     * This method is the same as the C function GDALRATGetColumnCount().
     *
     * @return the number of columns.
     */
    virtual int           GetColumnCount() const = 0;

    /**
     * \brief Fetch name of indicated column.
     *
     * This method is the same as the C function GDALRATGetNameOfCol().
     *
     * @param iCol the column index (zero based).
     *
     * @return the column name or an empty string for invalid column numbers.
     */
    virtual const char   *GetNameOfCol( int iCol ) const = 0;

    /**
     * \brief Fetch column usage value.
     *
     * This method is the same as the C function GDALRATGetUsageOfCol().
     *
     * @param iCol the column index (zero based).
     *
     * @return the column usage, or GFU_Generic for improper column numbers.
     */
    virtual GDALRATFieldUsage GetUsageOfCol( int iCol ) const = 0;

    /**
     * \brief Fetch column type.
     *
     * This method is the same as the C function GDALRATGetTypeOfCol().
     *
     * @param iCol the column index (zero based).
     *
     * @return column type or GFT_Integer if the column index is illegal.
     */
    virtual GDALRATFieldType GetTypeOfCol( int iCol ) const = 0;

    /**
     * \brief Fetch column index for given usage.
     *
     * Returns the index of the first column of the requested usage type, or -1
     * if no match is found.
     *
     * This method is the same as the C function GDALRATGetUsageOfCol().
     *
     * @param eUsage usage type to search for.
     *
     * @return column index, or -1 on failure.
     */
    virtual int           GetColOfUsage( GDALRATFieldUsage eUsage ) const = 0;

    /**
     * \brief Fetch row count.
     *
     * This method is the same as the C function GDALRATGetRowCount().
     *
     * @return the number of rows.
     */
    virtual int           GetRowCount() const = 0;

    /**
     * \brief Fetch field value as a string.
     *
     * The value of the requested column in the requested row is returned
     * as a string.  If the field is numeric, it is formatted as a string
     * using default rules, so some precision may be lost.
     *
     * The returned string is temporary and cannot be expected to be
     * available after the next GDAL call.
     *
     * This method is the same as the C function GDALRATGetValueAsString().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     *
     * @return field value.
     */
    virtual const char   *GetValueAsString( int iRow, int iField ) const = 0;

    /**
     * \brief Fetch field value as a integer.
     *
     * The value of the requested column in the requested row is returned
     * as an integer.  Non-integer fields will be converted to integer with
     * the possibility of data loss.
     *
     * This method is the same as the C function GDALRATGetValueAsInt().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     *
     * @return field value
     */
    virtual int           GetValueAsInt( int iRow, int iField ) const = 0;

    /**
     * \brief Fetch field value as a double.
     *
     * The value of the requested column in the requested row is returned
     * as a double.   Non double fields will be converted to double with
     * the possibility of data loss.
     *
     * This method is the same as the C function GDALRATGetValueAsDouble().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     *
     * @return field value
     */
    virtual double        GetValueAsDouble( int iRow, int iField ) const = 0;

    /**
     * \brief Set field value from string.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value.  The value will be automatically converted for other field
     * types, with a possible loss of precision.
     *
     * This method is the same as the C function GDALRATSetValueAsString().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param pszValue the value to assign.
     */
    virtual void          SetValue( int iRow, int iField,
                                    const char *pszValue ) = 0;

    /**
     * \brief Set field value from integer.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value.  The value will be automatically converted for other field
     * types, with a possible loss of precision.
     *
     * This method is the same as the C function GDALRATSetValueAsInteger().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param nValue the value to assign.
     */
    virtual void          SetValue( int iRow, int iField, int nValue ) = 0;

    /**
     * \brief Set field value from double.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value.  The value will be automatically converted for other field
     * types, with a possible loss of precision.
     *
     * This method is the same as the C function GDALRATSetValueAsDouble().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param dfValue the value to assign.
     */
    virtual void          SetValue( int iRow, int iField, double dfValue) = 0;

    /**
     * \brief Determine whether changes made to this RAT are reflected directly
     * in the dataset
     *
     * If this returns FALSE then GDALRasterBand.SetDefaultRAT() should be
     * called. Otherwise this is unnecessary since changes to this object are
     * reflected in the dataset.
     *
     * This method is the same as the C function
     * GDALRATChangesAreWrittenToFile().
     *
     */
    virtual int           ChangesAreWrittenToFile() = 0;

    /**
     * \brief Set the RAT table type.
     *
     * Set whether the RAT is thematic or athematic (continuous).
     *
     * @since GDAL 2.4
     */
    virtual CPLErr        SetTableType(const GDALRATTableType eInTableType) = 0;

    /**
     * \brief Get the RAT table type.
     *
     * Indicates whether the RAT is thematic or athematic (continuous).
     *
     * @since GDAL 2.4
     * @return table type
     */
    virtual GDALRATTableType GetTableType() const = 0;

    virtual CPLErr        ValuesIO( GDALRWFlag eRWFlag, int iField,
                                    int iStartRow, int iLength,
                                    double *pdfData);
    virtual CPLErr        ValuesIO( GDALRWFlag eRWFlag, int iField,
                                    int iStartRow, int iLength, int *pnData);
    virtual CPLErr        ValuesIO( GDALRWFlag eRWFlag, int iField,
                                    int iStartRow, int iLength,
                                    char **papszStrList);

    virtual void          SetRowCount( int iCount );
    virtual int           GetRowOfValue( double dfValue ) const;
    virtual int           GetRowOfValue( int nValue ) const;

    virtual CPLErr        CreateColumn( const char *pszFieldName,
                                        GDALRATFieldType eFieldType,
                                        GDALRATFieldUsage eFieldUsage );
    virtual CPLErr        SetLinearBinning( double dfRow0Min,
                                            double dfBinSize );
    virtual int           GetLinearBinning( double *pdfRow0Min,
                                            double *pdfBinSize ) const;

    /**
     * \brief Serialize
     *
     * May fail (return nullptr) if the attribute table is too large to serialize
     * (GetRowCount() * GetColCount() > RAT_MAX_ELEM_FOR_CLONE)
     */
    virtual CPLXMLNode   *Serialize() const;
    virtual void   *SerializeJSON() const;
    virtual CPLErr        XMLInit( CPLXMLNode *, const char * );

    virtual CPLErr        InitializeFromColorTable( const GDALColorTable * );
    virtual GDALColorTable *TranslateToColorTable( int nEntryCount = -1 );

    virtual void          DumpReadable( FILE * = nullptr );

    /** Convert a GDALRasterAttributeTable* to a GDALRasterAttributeTableH.
     * @since GDAL 2.3
     */
    static inline GDALRasterAttributeTableH ToHandle(GDALRasterAttributeTable* poRAT)
        { return static_cast<GDALRasterAttributeTableH>(poRAT); }

    /** Convert a GDALRasterAttributeTableH to a GDALRasterAttributeTable*.
     * @since GDAL 2.3
     */
    static inline GDALRasterAttributeTable* FromHandle(GDALRasterAttributeTableH hRAT)
        { return static_cast<GDALRasterAttributeTable*>(hRAT); }

    /**
     * \brief Remove statistics from the RAT.
     *
     * @since GDAL 2.4
     */
    virtual void          RemoveStatistics() = 0;
};

/************************************************************************/
/*                       GDALRasterAttributeField                       */
/*                                                                      */
/*      (private)                                                       */
/************************************************************************/
//! @cond Doxygen_Suppress
class GDALRasterAttributeField
{
 public:
    CPLString         sName{};

    GDALRATFieldType  eType = GFT_Integer;

    GDALRATFieldUsage eUsage = GFU_Generic;

    std::vector<GInt32> anValues{};
    std::vector<double> adfValues{};
    std::vector<CPLString> aosValues{};
};
//! @endcond

/************************************************************************/
/*                    GDALDefaultRasterAttributeTable                   */
/************************************************************************/

//! Raster Attribute Table container.

class CPL_DLL GDALDefaultRasterAttributeTable : public GDALRasterAttributeTable
{
 private:
    std::vector<GDALRasterAttributeField> aoFields{};

    int bLinearBinning = false;  // TODO(schwehr): Can this be a bool?
    double dfRow0Min = -0.5;
    double dfBinSize = 1.0;

    GDALRATTableType eTableType;

    void  AnalyseColumns();
    int   bColumnsAnalysed = false;  // TODO(schwehr): Can this be a bool?
    int   nMinCol = -1;
    int   nMaxCol = -1;

    int   nRowCount = 0;

    CPLString osWorkingResult{};

 public:
    GDALDefaultRasterAttributeTable();
    ~GDALDefaultRasterAttributeTable() override;

    GDALDefaultRasterAttributeTable *Clone() const override;

    int GetColumnCount() const override;

    const char *GetNameOfCol( int ) const override;
    GDALRATFieldUsage GetUsageOfCol( int ) const override;
    GDALRATFieldType GetTypeOfCol( int ) const override;

    int GetColOfUsage( GDALRATFieldUsage ) const override;

    int GetRowCount() const override;

    const char *GetValueAsString( int iRow, int iField ) const override;
    int GetValueAsInt( int iRow, int iField ) const override;
    double GetValueAsDouble( int iRow, int iField ) const override;

    void SetValue( int iRow, int iField,
                   const char *pszValue ) override;
    void SetValue( int iRow, int iField, double dfValue) override;
    void SetValue( int iRow, int iField, int nValue ) override;

    int ChangesAreWrittenToFile() override;
    void SetRowCount( int iCount ) override;

    int GetRowOfValue( double dfValue ) const override;
    int GetRowOfValue( int nValue ) const override;

    CPLErr CreateColumn( const char *pszFieldName,
                         GDALRATFieldType eFieldType,
                         GDALRATFieldUsage eFieldUsage ) override;
    CPLErr SetLinearBinning( double dfRow0Min,
                             double dfBinSize ) override;
    int GetLinearBinning( double *pdfRow0Min,
                          double *pdfBinSize ) const override;

    CPLErr        SetTableType(const GDALRATTableType eInTableType) override;
    GDALRATTableType GetTableType() const override;

    void          RemoveStatistics() override;
};

#endif /* ndef GDAL_RAT_H_INCLUDED */
