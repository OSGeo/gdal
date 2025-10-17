/******************************************************************************
 *
 * Name:     RAT.i
 * Project:  GDAL Python Interface
 * Purpose:  SWIG Interface for GDALRasterAttributeTable class.
 * Author:   Frank Warmerdam
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

//************************************************************************
//
// Define the extensions for RasterAttributeTable
//
//************************************************************************
#ifndef SWIGCSHARP
typedef int GDALRATFieldType;
typedef int GDALRATFieldUsage;
typedef int GDALRATTableType;
#else
%rename (RATFieldType) GDALRATFieldType;
typedef enum {
    /*! Integer field */                         GFT_Integer,
    /*! Floating point (double) field */         GFT_Real,
    /*! String field */                          GFT_String,
    /*! Boolean field (GDAL >= 3.12) */          GFT_Boolean,
    /*! DateTime field (GDAL >= 3.12) */         GFT_DateTime,
    /*! Geometry field, as WKB (GDAL >= 3.12) */ GFT_WKBGeometry
} GDALRATFieldType;

%rename (RATFieldUsage) GDALRATFieldUsage;
typedef enum {
    /*! General purpose field. */          GFU_Generic = 0,
    /*! Histogram pixel count */           GFU_PixelCount = 1,
    /*! Class name */                      GFU_Name = 2,
    /*! Class range minimum */             GFU_Min = 3,
    /*! Class range maximum */             GFU_Max = 4,
    /*! Class value (min=max) */           GFU_MinMax = 5,
    /*! Red class color (0-255) */         GFU_Red = 6,
    /*! Green class color (0-255) */       GFU_Green = 7,
    /*! Blue class color (0-255) */        GFU_Blue = 8,
    /*! Alpha (0=transparent,255=opaque)*/ GFU_Alpha = 9,
    /*! Color Range Red Minimum */         GFU_RedMin = 10,
    /*! Color Range Green Minimum */       GFU_GreenMin = 11,
    /*! Color Range Blue Minimum */        GFU_BlueMin = 12,
    /*! Color Range Alpha Minimum */       GFU_AlphaMin = 13,
    /*! Color Range Red Maximum */         GFU_RedMax = 14,
    /*! Color Range Green Maximum */       GFU_GreenMax = 15,
    /*! Color Range Blue Maximum */        GFU_BlueMax = 16,
    /*! Color Range Alpha Maximum */       GFU_AlphaMax = 17,
    /*! Maximum GFU value */               GFU_MaxCount
} GDALRATFieldUsage;

%rename (RATTableType) GDALRATTableType;
typedef enum {
    /*! Thematic table type */            GRTT_THEMATIC,
    /*! Athematic table type */           GRTT_ATHEMATIC
} GDALRATTableType;
#endif /* CSHARP */

%rename (RATDateTime) GDALRATDateTime;

typedef struct
{
    /*! Year */ int nYear;
    /*! Month [1, 12] */ int nMonth;
    /*! Day [1, 31] */ int nDay;
    /*! Hour [0, 23] */ int nHour;
    /*! Minute [0, 59] */ int nMinute;
    /*! Second [0, 61) */ float fSecond;
    /*! Time zone hour [0, 23] */ int nTimeZoneHour;
    /*! Time zone minute: 0, 15, 30, 45 */ int nTimeZoneMinute;
    /*! Whether time zone is positive (or null) */ bool bPositiveTimeZone;
    /*! Whether this object is valid */ bool bIsValid;
} GDALRATDateTime;

%rename (RasterAttributeTable) GDALRasterAttributeTableShadow;

class GDALRasterAttributeTableShadow {
private:

public:

%extend {

    GDALRasterAttributeTableShadow() {
        return (GDALRasterAttributeTableShadow*)
		GDALCreateRasterAttributeTable();
    }

    ~GDALRasterAttributeTableShadow() {
        GDALDestroyRasterAttributeTable(self);
    }

    %newobject Clone();
    GDALRasterAttributeTableShadow* Clone() {
        return (GDALRasterAttributeTableShadow*) GDALRATClone(self);
    }

    int GetColumnCount() {
        return GDALRATGetColumnCount( self );
    }

    const char *GetNameOfCol(int iCol) {
        return GDALRATGetNameOfCol( self, iCol );
    }

    GDALRATFieldUsage GetUsageOfCol( int iCol ) {
        return GDALRATGetUsageOfCol( self, iCol );
    }

    GDALRATFieldType GetTypeOfCol( int iCol ) {
        return GDALRATGetTypeOfCol( self, iCol );
    }

    int GetColOfUsage( GDALRATFieldUsage eUsage ) {
        return GDALRATGetColOfUsage( self, eUsage );
    }

    int GetRowCount() {
        return GDALRATGetRowCount( self );
    }

    const char *GetValueAsString( int iRow, int iCol ) {
        return GDALRATGetValueAsString( self, iRow, iCol );
    }

    int GetValueAsInt( int iRow, int iCol ) {
        return GDALRATGetValueAsInt( self, iRow, iCol );
    }

    double GetValueAsDouble( int iRow, int iCol ) {
        return GDALRATGetValueAsDouble( self, iRow, iCol );
    }

    bool GetValueAsBoolean( int iRow, int iCol ) {
        return GDALRATGetValueAsBoolean( self, iRow, iCol );
    }

    GDALRATDateTime GetValueAsDateTime( int iRow, int iCol ) {
        GDALRATDateTime dt;
        GDALRATGetValueAsDateTime( self, iRow, iCol, &dt );
        return dt;
    }

#if defined(SWIGPYTHON)
    CPLErr GetValueAsWKBGeometry( int iRow, int iCol, size_t *nLen, char **pBuf) {
        const void* pRet = GDALRATGetValueAsWKBGeometry( self, iRow, iCol, nLen );
        *pBuf = (char*)VSIMalloc(*nLen);
        if (*nLen)
            memcpy(*pBuf, pRet, *nLen);
        return CE_None;
    }

    CPLErr SetValueAsWKBGeometry( int iRow, int iCol, int nLen, char *pBuf) {
        return GDALRATSetValueAsWKBGeometry(self, iRow, iCol, pBuf, nLen);
    }

    CPLErr ReadValuesIOAsString( int iField, int iStartRow, int iLength, char **ppszData ) {
        return GDALRATValuesIOAsString(self, GF_Read, iField, iStartRow, iLength, ppszData);
    }

    CPLErr ReadValuesIOAsInteger( int iField, int iStartRow, int iLength, int *pnData ) {
        return GDALRATValuesIOAsInteger(self, GF_Read, iField, iStartRow, iLength, pnData);
    }

    CPLErr ReadValuesIOAsDouble( int iField, int iStartRow, int iLength, double *pdfData ) {
        return GDALRATValuesIOAsDouble(self, GF_Read, iField, iStartRow, iLength, pdfData);
    }

    CPLErr ReadValuesIOAsBoolean( int iField, int iStartRow, int iLength, bool *pbData ) {
        return GDALRATValuesIOAsBoolean(self, GF_Read, iField, iStartRow, iLength, pbData);
    }
#endif

    %apply ( tostring argin ) { (const char* pszValue) };
    void SetValueAsString( int iRow, int iCol, const char *pszValue ) {
        GDALRATSetValueAsString( self, iRow, iCol, pszValue );
    }
    %clear (const char* pszValue );

    void SetValueAsInt( int iRow, int iCol, int nValue ) {
        GDALRATSetValueAsInt( self, iRow, iCol, nValue );
    }

    void SetValueAsDouble( int iRow, int iCol, double dfValue ) {
        GDALRATSetValueAsDouble( self, iRow, iCol, dfValue );
    }

    void SetValueAsBoolean( int iRow, int iCol, bool value ) {
        GDALRATSetValueAsBoolean( self, iRow, iCol, value );
    }

    void SetValueAsDateTime( int iRow, int iCol, GDALRATDateTime value ) {
        GDALRATSetValueAsDateTime( self, iRow, iCol, &value );
    }

    void SetRowCount( int nCount ) {
        GDALRATSetRowCount( self, nCount );
    }

    int CreateColumn( const char *pszName, GDALRATFieldType eType,
                      GDALRATFieldUsage eUsage ) {
        return GDALRATCreateColumn( self, pszName, eType, eUsage );
    }

    %apply (double *OUTPUT){double *pdfRow0Min, double *pdfBinSize};
    bool GetLinearBinning( double *pdfRow0Min, double *pdfBinSize )
    {
        return (GDALRATGetLinearBinning(self, pdfRow0Min, pdfBinSize) != 0) ? true : false;
    }
    %clear double *pdfRow0Min, double *pdfBinSize;

    int	SetLinearBinning (double dfRow0Min, double dfBinSize)
    {
        return GDALRATSetLinearBinning(self, dfRow0Min, dfBinSize);
    }

    /* TODO: omit color table translation */

    int GetRowOfValue( double dfValue ) {
        return GDALRATGetRowOfValue( self, dfValue );
    }

    int ChangesAreWrittenToFile() {
        return GDALRATChangesAreWrittenToFile( self );
    }

    void DumpReadable() {
        GDALRATDumpReadable( self, NULL );
    }

    void SetTableType( GDALRATTableType eTableType ) {
        GDALRATSetTableType( self, eTableType );
    }

    GDALRATTableType GetTableType() {
        return GDALRATGetTableType( self );
    }

    void RemoveStatistics() {
        GDALRATRemoveStatistics(self);
    }
}

};
