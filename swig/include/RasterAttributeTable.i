/******************************************************************************
 * $Id$
 *
 * Name:     RAT.i
 * Project:  GDAL Python Interface
 * Purpose:  SWIG Interface for GDALRasterAttributeTable class.
 * Author:   Frank Warmerdam
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
    /*! Integer field */	   	   GFT_Integer ,
    /*! Floating point (double) field */   GFT_Real,
    /*! String field */                    GFT_String
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
}

};
