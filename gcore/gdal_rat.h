/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  GDALRasterAttributeTable class declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_RAT_H_INCLUDED
#define GDAL_RAT_H_INCLUDED

#if !defined(GDAL_COMPILATION) &&                                              \
    !defined(GDAL_RAT_SKIP_OTHER_GDAL_HEADERS) && !defined(GDAL_4_0_COMPAT)

#include "cpl_minixml.h"
#include "gdal_priv.h"

#else

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal.h"

#endif

#include <memory>
#include <vector>

class GDALColorTable;

// Clone and Serialize are allowed to fail if GetRowCount()*GetColCount()
// greater than this number
#define RAT_MAX_ELEM_FOR_CLONE 1000000

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
    virtual int GetColumnCount() const = 0;

    /**
     * \brief Fetch name of indicated column.
     *
     * This method is the same as the C function GDALRATGetNameOfCol().
     *
     * @param iCol the column index (zero based).
     *
     * @return the column name or an empty string for invalid column numbers.
     */
    virtual const char *GetNameOfCol(int iCol) const = 0;

    /**
     * \brief Fetch column usage value.
     *
     * This method is the same as the C function GDALRATGetUsageOfCol().
     *
     * @param iCol the column index (zero based).
     *
     * @return the column usage, or GFU_Generic for improper column numbers.
     */
    virtual GDALRATFieldUsage GetUsageOfCol(int iCol) const = 0;

    /**
     * \brief Fetch column type.
     *
     * This method is the same as the C function GDALRATGetTypeOfCol().
     *
     * @param iCol the column index (zero based).
     *
     * @return column type or GFT_Integer if the column index is illegal.
     */
    virtual GDALRATFieldType GetTypeOfCol(int iCol) const = 0;

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
    virtual int GetColOfUsage(GDALRATFieldUsage eUsage) const = 0;

    /**
     * \brief Fetch row count.
     *
     * This method is the same as the C function GDALRATGetRowCount().
     *
     * @return the number of rows.
     */
    virtual int GetRowCount() const = 0;

    /**
     * \brief Fetch field value as a string.
     *
     * The value of the requested column in the requested row is returned
     * as a string. This method is nominally called on fields of type GFT_String,
     * but it can be called on fields of other types as well.
     * If the field is numeric, it is formatted as a string
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
    virtual const char *GetValueAsString(int iRow, int iField) const = 0;

    /**
     * \brief Fetch field value as a integer.
     *
     * The value of the requested column in the requested row is returned
     * as an integer. This method is nominally called on fields of type
     * GFT_Integer, but it can be called on fields of other types as well.
     * Non-integer fields will be converted to integer with the possibility of
     * data loss.
     *
     * This method is the same as the C function GDALRATGetValueAsInt().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     *
     * @return field value
     */
    virtual int GetValueAsInt(int iRow, int iField) const = 0;

    /**
     * \brief Fetch field value as a double.
     *
     * The value of the requested column in the requested row is returned
     * as a double. This method is nominally called on fields of type
     * GFT_Real, but it can be called on fields of other types as well.
     * Non double fields will be converted to double with
     * the possibility of data loss.
     *
     * This method is the same as the C function GDALRATGetValueAsDouble().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     *
     * @return field value
     */
    virtual double GetValueAsDouble(int iRow, int iField) const = 0;

    /**
     * \brief Fetch field value as a boolean.
     *
     * The value of the requested column in the requested row is returned
     * as a boolean. This method is nominally called on fields of type
     * GFT_Boolean, but it can be called on fields of other types as well.
     * Non boolean fields will be converted to boolean with the possibility of
     * data loss.
     *
     * This method is the same as the C function GDALRATGetValueAsBoolean().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     *
     * @return field value
     * @since 3.12
     */
    virtual bool GetValueAsBoolean(int iRow, int iField) const = 0;

    /**
     * \brief Fetch field value as a datetime.
     *
     * The value of the requested column in the requested row is returned
     * as a datetime. Besides being called on a GFT_DateTime field, it
     * is also possible to call this method on a string field that contains a
     * ISO-8601 encoded datetime.
     *
     * This method is the same as the C function GDALRATGetValueAsDateTime().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     *
     * @return field value
     * @since 3.12
     */
    virtual GDALRATDateTime GetValueAsDateTime(int iRow, int iField) const = 0;

    /**
     * \brief Fetch field value as a WKB geometry.
     *
     * The value of the requested column in the requested row is returned
     * as a WKB geometry. Besides being called on a GFT_WKBGeometry field, it
     * is also possible to call this method on a string field that contains a WKT
     * encoded geometry.
     *
     * The returned pointer may be invalidated by a following call to a method
     * of this GDALRasterAttributeTable instance.
     *
     * This method is the same as the C function GDALRATGetValueAsWKBGeometry().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param[out] nWKBSize Number of bytes of the returned pointer
     * @return field value, or nullptr
     * @since 3.12
     */
    virtual const GByte *GetValueAsWKBGeometry(int iRow, int iField,
                                               size_t &nWKBSize) const = 0;

    /**
     * \brief Set field value from string.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value. This method is nominally called on fields of type
     * GFT_String, but it can be called on fields of other types as well.
     * The value will be automatically converted for other field
     * types, with a possible loss of precision.
     *
     * This method is the same as the C function GDALRATSetValueAsString().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param pszValue the value to assign.
     * @return (since 3.12) CE_None in case of success, error code otherwise
     */
    virtual CPLErr SetValue(int iRow, int iField, const char *pszValue) = 0;

    /**
     * \brief Set field value from integer.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value. This method is nominally called on fields of type
     * GFT_Integer, but it can be called on fields of other types as well.
     * The value will be automatically converted for other field
     * types, with a possible loss of precision.
     *
     * This method is the same as the C function GDALRATSetValueAsInteger().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param nValue the value to assign.
     * @return (since 3.12) CE_None in case of success, error code otherwise
     */
    virtual CPLErr SetValue(int iRow, int iField, int nValue) = 0;

    /**
     * \brief Set field value from double.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value. This method is nominally called on fields of type
     * GFT_Real, but it can be called on fields of other types as well.
     * The value will be automatically converted for other field
     * types, with a possible loss of precision.
     *
     * This method is the same as the C function GDALRATSetValueAsDouble().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param dfValue the value to assign.
     * @return (since 3.12) CE_None in case of success, error code otherwise
     */
    virtual CPLErr SetValue(int iRow, int iField, double dfValue) = 0;

    /**
     * \brief Set field value from boolean.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value.  This method is nominally called on fields of type
     * GFT_Boolean, but it can be called on fields of other types as well.
     * The value will be automatically converted for other field
     * types, with a possible loss of precision.
     *
     * This method is the same as the C function GDALRATSetValueAsBoolean().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param bValue the value to assign.
     * @return CE_None in case of success, error code otherwise
     * @since 3.12
     */
    virtual CPLErr SetValue(int iRow, int iField, bool bValue) = 0;

    /**
     * \brief Set field value from datetime.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value. Besides being called on a field of type GFT_DateTime, this
     * method can also be called on a field of type GFT_String, in which case
     * the datetime will be converted into its ISO-8601 representation.
     *
     * Note that the GDALRATDateTime::bIsValid field must be set to true if
     * the date time is valid.
     *
     * This method is the same as the C function GDALRATSetValueAsDateTime().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param sDateTime Date time value
     * @return CE_None in case of success, error code otherwise
     * @since 3.12
     */
    virtual CPLErr SetValue(int iRow, int iField,
                            const GDALRATDateTime &sDateTime) = 0;

    /**
     * \brief Set field value from a WKB geometry.
     *
     * The indicated field (column) on the indicated row is set from the
     * passed value. Besides being called on a field of type GFT_WKBGeometry, this
     * method can also be called on a field of type GFT_String, in which case
     * the datetime will be converted into its WKT geometry representation.
     *
     * This method is the same as the C function GDALRATSetValueAsWKBGeometry().
     *
     * @param iRow row to fetch (zero based).
     * @param iField column to fetch (zero based).
     * @param pabyWKB Pointer to a WKB encoded geometry
     * @param nWKBSize Number of bytes of pabyWKB.
     * @return CE_None in case of success, error code otherwise
     * @since 3.12
     */
    virtual CPLErr SetValue(int iRow, int iField, const void *pabyWKB,
                            size_t nWKBSize) = 0;

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
    virtual int ChangesAreWrittenToFile() = 0;

    /**
     * \brief Set the RAT table type.
     *
     * Set whether the RAT is thematic or athematic (continuous).
     *
     */
    virtual CPLErr SetTableType(const GDALRATTableType eInTableType) = 0;

    /**
     * \brief Get the RAT table type.
     *
     * Indicates whether the RAT is thematic or athematic (continuous).
     *
     * @return table type
     */
    virtual GDALRATTableType GetTableType() const = 0;

    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, double *pdfData);
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, int *pnData);
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, char **papszStrList);
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, bool *pbData);
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, GDALRATDateTime *pasDateTime);
    virtual CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow,
                            int iLength, GByte **ppabyWKB, size_t *pnWKBSize);

    virtual void SetRowCount(int iCount);
    virtual int GetRowOfValue(double dfValue) const;
    virtual int GetRowOfValue(int nValue) const;

    virtual CPLErr CreateColumn(const char *pszFieldName,
                                GDALRATFieldType eFieldType,
                                GDALRATFieldUsage eFieldUsage);
    virtual CPLErr SetLinearBinning(double dfRow0Min, double dfBinSize);
    virtual int GetLinearBinning(double *pdfRow0Min, double *pdfBinSize) const;

    /**
     * \brief Serialize
     *
     * May fail (return nullptr) if the attribute table is too large to
     * serialize (GetRowCount() * GetColCount() > RAT_MAX_ELEM_FOR_CLONE)
     */
    virtual CPLXMLNode *Serialize() const;
    virtual void *SerializeJSON() const;
    virtual CPLErr XMLInit(const CPLXMLNode *, const char *);

    virtual CPLErr InitializeFromColorTable(const GDALColorTable *);
    virtual GDALColorTable *TranslateToColorTable(int nEntryCount = -1);

    virtual void DumpReadable(FILE * = nullptr);

    /** Convert a GDALRasterAttributeTable* to a GDALRasterAttributeTableH.
     */
    static inline GDALRasterAttributeTableH
    ToHandle(GDALRasterAttributeTable *poRAT)
    {
        return static_cast<GDALRasterAttributeTableH>(poRAT);
    }

    /** Convert a GDALRasterAttributeTableH to a GDALRasterAttributeTable*.
     */
    static inline GDALRasterAttributeTable *
    FromHandle(GDALRasterAttributeTableH hRAT)
    {
        return static_cast<GDALRasterAttributeTable *>(hRAT);
    }

    /**
     * \brief Remove statistics from the RAT.
     *
     */
    virtual void RemoveStatistics() = 0;

    //! @cond Doxygen_Suppress
    static std::string DateTimeToString(const GDALRATDateTime &sDateTime);
    static bool StringToDateTime(const char *pszStr,
                                 GDALRATDateTime &sDateTime);

    static std::string WKBGeometryToWKT(const void *pabyWKB, size_t nWKBSize);
    static std::vector<GByte> WKTGeometryToWKB(const char *pszWKT);
    //! @endcond

  protected:
    //! @cond Doxygen_Suppress
    GDALRasterAttributeTable() = default;
    GDALRasterAttributeTable(const GDALRasterAttributeTable &) = default;
    GDALRasterAttributeTable &
    operator=(const GDALRasterAttributeTable &) = default;
    GDALRasterAttributeTable(GDALRasterAttributeTable &&) = default;
    GDALRasterAttributeTable &operator=(GDALRasterAttributeTable &&) = default;

    CPLErr ValuesIOBooleanFromIntoInt(GDALRWFlag eRWFlag, int iField,
                                      int iStartRow, int iLength, bool *pbData);
    CPLErr ValuesIODateTimeFromIntoString(GDALRWFlag eRWFlag, int iField,
                                          int iStartRow, int iLength,
                                          GDALRATDateTime *psDateTime);
    CPLErr ValuesIOWKBGeometryFromIntoString(GDALRWFlag eRWFlag, int iField,
                                             int iStartRow, int iLength,
                                             GByte **ppabyWKB,
                                             size_t *pnWKBSize);
    //! @endcond
};

/************************************************************************/
/*                   GDALDefaultRasterAttributeTable                    */
/************************************************************************/

//! Raster Attribute Table container.

class CPL_DLL GDALDefaultRasterAttributeTable : public GDALRasterAttributeTable
{
  private:
    struct GDALRasterAttributeField
    {
        CPLString sName{};

        GDALRATFieldType eType = GFT_Integer;

        GDALRATFieldUsage eUsage = GFU_Generic;

        std::vector<GInt32> anValues{};
        std::vector<double> adfValues{};
        std::vector<CPLString> aosValues{};
        std::vector<bool> abValues{};
        std::vector<GDALRATDateTime> asDateTimeValues{};
        std::vector<std::vector<GByte>> aabyWKBGeometryValues{};
    };

    std::vector<GDALRasterAttributeField> aoFields{};

    int bLinearBinning = false;  // TODO(schwehr): Can this be a bool?
    double dfRow0Min = -0.5;
    double dfBinSize = 1.0;

    GDALRATTableType eTableType = GRTT_THEMATIC;

    void AnalyseColumns();
    int bColumnsAnalysed = false;  // TODO(schwehr): Can this be a bool?
    int nMinCol = -1;
    int nMaxCol = -1;

    int nRowCount = 0;

    CPLString osWorkingResult{};
    mutable std::vector<GByte> m_abyWKB{};

  public:
    GDALDefaultRasterAttributeTable();
    ~GDALDefaultRasterAttributeTable() override;

    //! @cond Doxygen_Suppress
    GDALDefaultRasterAttributeTable(const GDALDefaultRasterAttributeTable &) =
        default;
    GDALDefaultRasterAttributeTable &
    operator=(const GDALDefaultRasterAttributeTable &) = default;
    GDALDefaultRasterAttributeTable(GDALDefaultRasterAttributeTable &&) =
        default;
    GDALDefaultRasterAttributeTable &
    operator=(GDALDefaultRasterAttributeTable &&) = default;
    //! @endcond

    GDALDefaultRasterAttributeTable *Clone() const override;

    int GetColumnCount() const override;

    const char *GetNameOfCol(int) const override;
    GDALRATFieldUsage GetUsageOfCol(int) const override;
    GDALRATFieldType GetTypeOfCol(int) const override;

    int GetColOfUsage(GDALRATFieldUsage) const override;

    int GetRowCount() const override;

    const char *GetValueAsString(int iRow, int iField) const override;
    int GetValueAsInt(int iRow, int iField) const override;
    double GetValueAsDouble(int iRow, int iField) const override;
    bool GetValueAsBoolean(int iRow, int iField) const override;
    GDALRATDateTime GetValueAsDateTime(int iRow, int iField) const override;
    const GByte *GetValueAsWKBGeometry(int iRow, int iField,
                                       size_t &nWKBSize) const override;

    CPLErr SetValue(int iRow, int iField, const char *pszValue) override;
    CPLErr SetValue(int iRow, int iField, double dfValue) override;
    CPLErr SetValue(int iRow, int iField, int nValue) override;
    CPLErr SetValue(int iRow, int iField, bool bValue) override;
    CPLErr SetValue(int iRow, int iField,
                    const GDALRATDateTime &sDateTime) override;
    CPLErr SetValue(int iRow, int iField, const void *pabyWKB,
                    size_t nWKBSize) override;

    int ChangesAreWrittenToFile() override;
    void SetRowCount(int iCount) override;

    int GetRowOfValue(double dfValue) const override;
    int GetRowOfValue(int nValue) const override;

    CPLErr CreateColumn(const char *pszFieldName, GDALRATFieldType eFieldType,
                        GDALRATFieldUsage eFieldUsage) override;
    CPLErr SetLinearBinning(double dfRow0Min, double dfBinSize) override;
    int GetLinearBinning(double *pdfRow0Min, double *pdfBinSize) const override;

    CPLErr SetTableType(const GDALRATTableType eInTableType) override;
    GDALRATTableType GetTableType() const override;

    void RemoveStatistics() override;
};

std::unique_ptr<GDALRasterAttributeTable>
    CPL_DLL GDALLoadVATDBF(const char *pszFilename);

#endif /* ndef GDAL_RAT_H_INCLUDED */
