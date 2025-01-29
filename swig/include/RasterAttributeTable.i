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

typedef int GDALRATFieldType;
typedef int GDALRATFieldUsage;
typedef int GDALRATTableType;

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

#if defined(SWIGPYTHON)
    CPLErr ReadValuesIOAsString( int iField, int iStartRow, int iLength, char **ppszData ) {
        return GDALRATValuesIOAsString(self, GF_Read, iField, iStartRow, iLength, ppszData);
    }

    CPLErr ReadValuesIOAsInteger( int iField, int iStartRow, int iLength, int *pnData ) {
        return GDALRATValuesIOAsInteger(self, GF_Read, iField, iStartRow, iLength, pnData);
    }

    CPLErr ReadValuesIOAsDouble( int iField, int iStartRow, int iLength, double *pdfData ) {
        return GDALRATValuesIOAsDouble(self, GF_Read, iField, iStartRow, iLength, pdfData);
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

    void SetRowCount( int nCount ) {
        GDALRATSetRowCount( self, nCount );
    }

    int CreateColumn( const char *pszName, GDALRATFieldType eType,
                      GDALRATFieldUsage eUsage ) {
        return GDALRATCreateColumn( self, pszName, eType, eUsage );
    }

    /* Interface method added for GDAL 1.7.0 */
    %apply (double *OUTPUT){double *pdfRow0Min, double *pdfBinSize};
    bool GetLinearBinning( double *pdfRow0Min, double *pdfBinSize )
    {
        return (GDALRATGetLinearBinning(self, pdfRow0Min, pdfBinSize) != 0) ? true : false;
    }
    %clear double *pdfRow0Min, double *pdfBinSize;

    /* Interface method added for GDAL 1.7.0 */
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
