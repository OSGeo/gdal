/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALRasterAttributeTable and related classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_priv.h"
#include "gdal_rat.h"

CPL_CVSID("$Id$");

/**
 * \class GDALRasterAttributeTable
 *
 * The GDALRasterAttributeTable (or RAT) class is used to encapsulate a table
 * used to provide attribute information about pixel values.  Each row
 * in the table applies to a range of pixel values (or a single value in
 * some cases), and might have attributes such as the histogram count for
 * that range, the color pixels of that range should be drawn names of classes
 * or any other generic information. 
 *
 * Raster attribute tables can be used to represent histograms, color tables,
 * and classification information.  
 *
 * Each column in a raster attribute table has a name, a type (integer,
 * floating point or string), and a GDALRATFieldUsage.  The usage distinguishes 
 * columns with particular understood purposes (such as color, histogram 
 * count, name) and columns that have specific purposes not understood by 
 * the library (long label, suitability_for_growing_wheat, etc).  
 *
 * In the general case each row has a column indicating the minimum pixel
 * values falling into that category, and a column indicating the maximum
 * pixel value.  These are indicated with usage values of GFU_Min, and
 * GFU_Max.  In other cases where each row is a discrete pixel value, one
 * column of usage GFU_MinMax can be used.  
 * 
 * In other cases all the categories are of equal size and regularly spaced 
 * and the categorization information can be determine just by knowing the
 * value at which the categories start, and the size of a category.  This
 * is called "Linear Binning" and the information is kept specially on 
 * the raster attribute table as a whole.
 *
 * RATs are normally associated with GDALRasterBands and be be queried
 * using the GDALRasterBand::GetDefaultRAT() method.  
 */

/************************************************************************/
/*                  ~GDALRasterAttributeTable()                         */
/*                                                                      */
/*                      Virtual Destructor                              */
/************************************************************************/

GDALRasterAttributeTable::~GDALRasterAttributeTable()
{

}

/************************************************************************/
/*                              ValuesIO()                              */
/*                                                                      */
/*                      Default Implementations                         */
/************************************************************************/

/**
 * \brief Read or Write a block of doubles to/from the Attribute Table.
 *
 * This method is the same as the C function GDALRATValuesIOAsDouble().
 *
 * @param eRWFlag Either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param pdfData pointer to array of doubles to read/write. Should be at least iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of rows in table.
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, double *pdfData)
{
    int iIndex;

    if( (iStartRow + iLength) > GetRowCount() ) 
    {
        return CE_Failure;
    }

    if( eRWFlag == GF_Read )
    {
        for(iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++ )
        {
            pdfData[iIndex] = GetValueAsDouble(iIndex, iField);
        }
    }
    else
    {
        for(iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++ )
        {
            SetValue(iIndex, iField, pdfData[iIndex]);
        }
    }
    return CE_None;
}

/************************************************************************/
/*                       GDALRATValuesIOAsDouble()                      */
/************************************************************************/

/**
 * \brief Read or Write a block of doubles to/from the Attribute Table.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::ValuesIO()
 */
CPLErr CPL_STDCALL GDALRATValuesIOAsDouble( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag, 
                                        int iField, int iStartRow, int iLength, double *pdfData )

{
    VALIDATE_POINTER1( hRAT, "GDALRATValuesIOAsDouble", CE_Failure );

    return ((GDALRasterAttributeTable *) hRAT)->ValuesIO(eRWFlag, iField, iStartRow, iLength, pdfData);
}

/**
 * \brief Read or Write a block of integers to/from the Attribute Table.
 *
 * This method is the same as the C function GDALRATValuesIOAsInteger().
 *
 * @param eRWFlag Either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param pnData pointer to array of ints to read/write. Should be at least iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of rows in table.
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, int *pnData)
{
    int iIndex;

    if( (iStartRow + iLength) > GetRowCount() ) 
    {
        return CE_Failure;
    }

    if( eRWFlag == GF_Read )
    {
        for(iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++ )
        {
            pnData[iIndex] = GetValueAsInt(iIndex, iField);
        }
    }
    else
    {
        for(iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++ )
        {
            SetValue(iIndex, iField, pnData[iIndex]);
        }
    }
    return CE_None;
}

/************************************************************************/
/*                       GDALRATValuesIOAsInteger()                     */
/************************************************************************/

/**
 * \brief Read or Write a block of ints to/from the Attribute Table.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::ValuesIO()
 */
CPLErr CPL_STDCALL GDALRATValuesIOAsInteger( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag, 
                                        int iField, int iStartRow, int iLength, int *pnData)

{
    VALIDATE_POINTER1( hRAT, "GDALRATValuesIOAsInteger", CE_Failure );

    return ((GDALRasterAttributeTable *) hRAT)->ValuesIO(eRWFlag, iField, iStartRow, iLength, pnData);
}

/**
 * \brief Read or Write a block of strings to/from the Attribute Table.
 *
 * This method is the same as the C function GDALRATValuesIOAsString().
 * When reading, papszStrList must be already allocated to the correct size.
 * The caller is expected to call CPLFree on each read string.
 *
 * @param eRWFlag Either GF_Read or GF_Write
 * @param iField column of the Attribute Table
 * @param iStartRow start row to start reading/writing (zero based)
 * @param iLength number of rows to read or write
 * @param papszStrList pointer to array of strings to read/write. Should be at least iLength long.
 *
 * @return CE_None or CE_Failure if iStartRow + iLength greater than number of rows in table.
 */

CPLErr GDALRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength, char **papszStrList)
{
    int iIndex;

    if( (iStartRow + iLength) > GetRowCount() ) 
    {
        return CE_Failure;
    }

    if( eRWFlag == GF_Read )
    {
        for(iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++ )
        {
            papszStrList[iIndex] = VSIStrdup(GetValueAsString(iIndex, iField));
        }
    }
    else
    {
        for(iIndex = iStartRow; iIndex < (iStartRow + iLength); iIndex++ )
        {
            SetValue(iIndex, iField, papszStrList[iIndex]);
        }
    }
    return CE_None;
}

/************************************************************************/
/*                       GDALRATValuesIOAsString()                      */
/************************************************************************/

/**
 * \brief Read or Write a block of strings to/from the Attribute Table.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::ValuesIO()
 */
CPLErr CPL_STDCALL GDALRATValuesIOAsString( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag, 
                                        int iField, int iStartRow, int iLength, char **papszStrList)

{
    VALIDATE_POINTER1( hRAT, "GDALRATValuesIOAsString", CE_Failure );

    return ((GDALRasterAttributeTable *) hRAT)->ValuesIO(eRWFlag, iField, iStartRow, iLength, papszStrList);
}

/************************************************************************/
/*                            SetRowCount()                             */
/************************************************************************/

/**
 * \brief Set row count.
 *
 * Resizes the table to include the indicated number of rows.  Newly created
 * rows will be initialized to their default values - "" for strings, 
 * and zero for numeric fields. 
 *
 * This method is the same as the C function GDALRATSetRowCount().
 *
 * @param nNewCount the new number of rows.
 */

void GDALRasterAttributeTable::SetRowCount( CPL_UNUSED int nNewCount )
{
}

/************************************************************************/
/*                         GDALRATSetRowCount()                         */
/************************************************************************/

/**
 * \brief Set row count.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::SetRowCount()
 */
void CPL_STDCALL 
GDALRATSetRowCount( GDALRasterAttributeTableH hRAT, int nNewCount )

{
    VALIDATE_POINTER0( hRAT, "GDALRATSetRowCount" );

    ((GDALRasterAttributeTable *) hRAT)->SetRowCount( nNewCount );
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/************************************************************************/

/**
 * \brief Get row for pixel value.
 *
 * Given a raw pixel value, the raster attribute table is scanned to 
 * determine which row in the table applies to the pixel value.  The
 * row index is returned. 
 *
 * This method is the same as the C function GDALRATGetRowOfValue().
 *
 * @param dfValue the pixel value. 
 *
 * @return the row index or -1 if no row is appropriate. 
 */

int GDALRasterAttributeTable::GetRowOfValue( CPL_UNUSED double dfValue ) const
{
    return -1;
}

/************************************************************************/
/*                        GDALRATGetRowOfValue()                        */
/************************************************************************/

/**
 * \brief Get row for pixel value.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetRowOfValue()
 */
int CPL_STDCALL 
GDALRATGetRowOfValue( GDALRasterAttributeTableH hRAT, double dfValue )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetRowOfValue", 0 );

    return ((GDALRasterAttributeTable *) hRAT)->GetRowOfValue( dfValue );
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/*                                                                      */
/*      Int arg for now just converted to double.  Perhaps we will      */
/*      handle this in a special way some day?                          */
/************************************************************************/

int GDALRasterAttributeTable::GetRowOfValue( int nValue ) const

{
    return GetRowOfValue( (double) nValue );
}

/************************************************************************/
/*                            CreateColumn()                            */
/************************************************************************/

/**
 * \brief Create new column.
 *
 * If the table already has rows, all row values for the new column will
 * be initialized to the default value ("", or zero).  The new column is
 * always created as the last column, can will be column (field) 
 * "GetColumnCount()-1" after CreateColumn() has completed successfully.
 * 
 * This method is the same as the C function GDALRATCreateColumn().
 *
 * @param pszFieldName the name of the field to create.
 * @param eFieldType the field type (integer, double or string).
 * @param eFieldUsage the field usage, GFU_Generic if not known.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr GDALRasterAttributeTable::CreateColumn( CPL_UNUSED const char *pszFieldName,
                                               CPL_UNUSED GDALRATFieldType eFieldType,
                                               CPL_UNUSED GDALRATFieldUsage eFieldUsage )
{
    return CE_Failure;
}

/************************************************************************/
/*                        GDALRATCreateColumn()                         */
/************************************************************************/

/**
 * \brief Create new column.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::CreateColumn()
 */
CPLErr CPL_STDCALL GDALRATCreateColumn( GDALRasterAttributeTableH hRAT, 
                                        const char *pszFieldName, 
                                        GDALRATFieldType eFieldType,
                                        GDALRATFieldUsage eFieldUsage )

{
    VALIDATE_POINTER1( hRAT, "GDALRATCreateColumn", CE_Failure );

    return ((GDALRasterAttributeTable *) hRAT)->CreateColumn( pszFieldName, 
                                                              eFieldType,
                                                              eFieldUsage );
}

/************************************************************************/
/*                          SetLinearBinning()                          */
/************************************************************************/

/**
 * \brief Set linear binning information.
 *
 * For RATs with equal sized categories (in pixel value space) that are
 * evenly spaced, this method may be used to associate the linear binning
 * information with the table.
 *
 * This method is the same as the C function GDALRATSetLinearBinning().
 *
 * @param dfRow0MinIn the lower bound (pixel value) of the first category.
 * @param dfBinSizeIn the width of each category (in pixel value units). 
 *
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr GDALRasterAttributeTable::SetLinearBinning( CPL_UNUSED double dfRow0MinIn,
                                                   CPL_UNUSED double dfBinSizeIn )
{
    return CE_Failure;
}

/************************************************************************/
/*                      GDALRATSetLinearBinning()                       */
/************************************************************************/

/**
 * \brief Set linear binning information.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::SetLinearBinning()
 */
CPLErr CPL_STDCALL 
GDALRATSetLinearBinning( GDALRasterAttributeTableH hRAT, 
                         double dfRow0Min, double dfBinSize )

{
    VALIDATE_POINTER1( hRAT, "GDALRATSetLinearBinning", CE_Failure );

    return ((GDALRasterAttributeTable *) hRAT)->SetLinearBinning(
        dfRow0Min, dfBinSize );
}

/************************************************************************/
/*                          GetLinearBinning()                          */
/************************************************************************/

/**
 * \brief Get linear binning information.
 *
 * Returns linear binning information if any is associated with the RAT.
 *
 * This method is the same as the C function GDALRATGetLinearBinning().
 *
 * @param pdfRow0Min (out) the lower bound (pixel value) of the first category.
 * @param pdfBinSize (out) the width of each category (in pixel value units).
 *
 * @return TRUE if linear binning information exists or FALSE if there is none.
 */

int GDALRasterAttributeTable::GetLinearBinning( CPL_UNUSED double *pdfRow0Min,
                                                CPL_UNUSED double *pdfBinSize ) const
{
    return FALSE;
}

/************************************************************************/
/*                      GDALRATGetLinearBinning()                       */
/************************************************************************/

/**
 * \brief Get linear binning information.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetLinearBinning()
 */
int CPL_STDCALL 
GDALRATGetLinearBinning( GDALRasterAttributeTableH hRAT, 
                         double *pdfRow0Min, double *pdfBinSize )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetLinearBinning", 0 );

    return ((GDALRasterAttributeTable *) hRAT)->GetLinearBinning(
        pdfRow0Min, pdfBinSize );
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

CPLXMLNode *GDALRasterAttributeTable::Serialize() const

{
    CPLXMLNode *psTree = NULL;
    CPLXMLNode *psRow = NULL;

    if( ( GetColumnCount() == 0 ) && ( GetRowCount() == 0 ) ) 
 	    return NULL;

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "GDALRasterAttributeTable" );

/* -------------------------------------------------------------------- */
/*      Add attributes with regular binning info if appropriate.        */
/* -------------------------------------------------------------------- */
    char szValue[128];
    double dfRow0Min, dfBinSize;

    if( GetLinearBinning(&dfRow0Min, &dfBinSize) )
    {
        CPLsprintf( szValue, "%.16g", dfRow0Min );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psTree, CXT_Attribute, "Row0Min" ), 
            CXT_Text, szValue );

        CPLsprintf( szValue, "%.16g", dfBinSize );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psTree, CXT_Attribute, "BinSize" ), 
            CXT_Text, szValue );
    }

/* -------------------------------------------------------------------- */
/*      Define each column.                                             */
/* -------------------------------------------------------------------- */
    int iCol;
    int iColCount = GetColumnCount();

    for( iCol = 0; iCol < iColCount; iCol++ )
    {
        CPLXMLNode *psCol;

        psCol = CPLCreateXMLNode( psTree, CXT_Element, "FieldDefn" );
        
        sprintf( szValue, "%d", iCol );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psCol, CXT_Attribute, "index" ), 
            CXT_Text, szValue );

        CPLCreateXMLElementAndValue( psCol, "Name", 
                                     GetNameOfCol(iCol) );

        sprintf( szValue, "%d", (int) GetTypeOfCol(iCol) );
        CPLCreateXMLElementAndValue( psCol, "Type", szValue );

        sprintf( szValue, "%d", (int) GetUsageOfCol(iCol) );
        CPLCreateXMLElementAndValue( psCol, "Usage", szValue );
    }

/* -------------------------------------------------------------------- */
/*      Write out each row.                                             */
/* -------------------------------------------------------------------- */
    int iRow;
    int iRowCount = GetRowCount();
    CPLXMLNode *psTail = NULL;

    for( iRow = 0; iRow < iRowCount; iRow++ )
    {
        psRow = CPLCreateXMLNode( NULL, CXT_Element, "Row" );
        if( psTail == NULL )
            CPLAddXMLChild( psTree, psRow );
        else
            psTail->psNext = psRow;
        psTail = psRow;

        sprintf( szValue, "%d", iRow );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psRow, CXT_Attribute, "index" ), 
            CXT_Text, szValue );

        for( iCol = 0; iCol < iColCount; iCol++ )
        {
            const char *pszValue = szValue;

            if( GetTypeOfCol(iCol) == GFT_Integer )
                sprintf( szValue, "%d", GetValueAsInt(iRow, iCol) );
            else if( GetTypeOfCol(iCol) == GFT_Real )
                CPLsprintf( szValue, "%.16g", GetValueAsDouble(iRow, iCol) );
            else
                pszValue = GetValueAsString(iRow, iCol);

            CPLCreateXMLElementAndValue( psRow, "F", pszValue );
        }
    }

    return psTree;
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr GDALRasterAttributeTable::XMLInit( CPLXMLNode *psTree, 
                                          const char * /*pszVRTPath*/ )

{
    CPLAssert( GetRowCount() == 0 && GetColumnCount() == 0 );
    
/* -------------------------------------------------------------------- */
/*      Linear binning.                                                 */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLValue( psTree, "Row0Min", NULL ) 
        && CPLGetXMLValue( psTree, "BinSize", NULL ) )
    {
        SetLinearBinning( CPLAtof(CPLGetXMLValue( psTree, "Row0Min","" )), 
                          CPLAtof(CPLGetXMLValue( psTree, "BinSize","" )) );
    }

/* -------------------------------------------------------------------- */
/*      Column definitions                                              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psChild;

    for( psChild = psTree->psChild; psChild != NULL; psChild = psChild->psNext)
    {
        if( psChild->eType == CXT_Element 
            && EQUAL(psChild->pszValue,"FieldDefn") )
        {
            CreateColumn( 
              CPLGetXMLValue( psChild, "Name", "" ), 
              (GDALRATFieldType) atoi(CPLGetXMLValue( psChild, "Type", "1" )),
              (GDALRATFieldUsage) atoi(CPLGetXMLValue( psChild, "Usage","0")));
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Row data.                                                       */
/* -------------------------------------------------------------------- */
    for( psChild = psTree->psChild; psChild != NULL; psChild = psChild->psNext)
    {
        if( psChild->eType == CXT_Element 
            && EQUAL(psChild->pszValue,"Row") )
        {
            int iRow = atoi(CPLGetXMLValue(psChild,"index","0"));
            int iField = 0;
            CPLXMLNode *psF;

            for( psF = psChild->psChild; psF != NULL; psF = psF->psNext )
            {
                if( psF->eType != CXT_Element || !EQUAL(psF->pszValue,"F") )
                    continue;

                if( psF->psChild != NULL && psF->psChild->eType == CXT_Text )
                    SetValue( iRow, iField++, psF->psChild->pszValue );
                else
                    SetValue( iRow, iField++, "" );
            }
        }
    }

    return CE_None;
}


/************************************************************************/
/*                      InitializeFromColorTable()                      */
/************************************************************************/

/**
 * \brief Initialize from color table.
 *
 * This method will setup a whole raster attribute table based on the
 * contents of the passed color table.  The Value (GFU_MinMax), 
 * Red (GFU_Red), Green (GFU_Green), Blue (GFU_Blue), and Alpha (GFU_Alpha)
 * fields are created, and a row is set for each entry in the color table. 
 * 
 * The raster attribute table must be empty before calling 
 * InitializeFromColorTable(). 
 *
 * The Value fields are set based on the implicit assumption with color
 * tables that entry 0 applies to pixel value 0, 1 to 1, etc. 
 *
 * This method is the same as the C function GDALRATInitializeFromColorTable().
 *
 * @param poTable the color table to copy from.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr GDALRasterAttributeTable::InitializeFromColorTable( 
    const GDALColorTable *poTable )

{
    int iRow;

    if( GetRowCount() > 0 || GetColumnCount() > 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Raster Attribute Table not empty in InitializeFromColorTable()" );
        return CE_Failure;
    }

    SetLinearBinning( 0.0, 1.0 );
    CreateColumn( "Value", GFT_Integer, GFU_MinMax );
    CreateColumn( "Red", GFT_Integer, GFU_Red );
    CreateColumn( "Green", GFT_Integer, GFU_Green );
    CreateColumn( "Blue", GFT_Integer, GFU_Blue );
    CreateColumn( "Alpha", GFT_Integer, GFU_Alpha );

    SetRowCount( poTable->GetColorEntryCount() );

    for( iRow = 0; iRow < poTable->GetColorEntryCount(); iRow++ )
    {
        GDALColorEntry sEntry;

        poTable->GetColorEntryAsRGB( iRow, &sEntry );

        SetValue( iRow, 0, iRow );
        SetValue( iRow, 1, sEntry.c1 );
        SetValue( iRow, 2, sEntry.c2 );
        SetValue( iRow, 3, sEntry.c3 );
        SetValue( iRow, 4, sEntry.c4 );
    }

    return CE_None;
}

/************************************************************************/
/*                  GDALRATInitializeFromColorTable()                   */
/************************************************************************/

/**
 * \brief Initialize from color table.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::InitializeFromColorTable()
 */
CPLErr CPL_STDCALL 
GDALRATInitializeFromColorTable( GDALRasterAttributeTableH hRAT,
                                 GDALColorTableH hCT )
                                 

{
    VALIDATE_POINTER1( hRAT, "GDALRATInitializeFromColorTable", CE_Failure );

    return ((GDALRasterAttributeTable *) hRAT)->
        InitializeFromColorTable( (GDALColorTable *) hCT );
}

/************************************************************************/
/*                       TranslateToColorTable()                        */
/************************************************************************/

/**
 * \brief Translate to a color table.
 *
 * This method will attempt to create a corresponding GDALColorTable from
 * this raster attribute table. 
 * 
 * This method is the same as the C function GDALRATTranslateToColorTable().
 *
 * @param nEntryCount The number of entries to produce (0 to nEntryCount-1), or -1 to auto-determine the number of entries.
 *
 * @return the generated color table or NULL on failure.
 */

GDALColorTable *GDALRasterAttributeTable::TranslateToColorTable( 
    int nEntryCount )

{
/* -------------------------------------------------------------------- */
/*      Establish which fields are red, green, blue and alpha.          */
/* -------------------------------------------------------------------- */
    int iRed, iGreen, iBlue, iAlpha;

    iRed = GetColOfUsage( GFU_Red );
    iGreen = GetColOfUsage( GFU_Green );
    iBlue = GetColOfUsage( GFU_Blue );
    iAlpha = GetColOfUsage( GFU_Alpha );

    if( iRed == -1 || iGreen == -1 || iBlue == -1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If we aren't given an explicit number of values to scan for,    */
/*      search for the maximum "max" value.                             */
/* -------------------------------------------------------------------- */
    if( nEntryCount == -1 )
    {
        int  iRow;
        int  iMaxCol;

        iMaxCol = GetColOfUsage( GFU_Max );
        if( iMaxCol == -1 )
            iMaxCol = GetColOfUsage( GFU_MinMax );

        if( iMaxCol == -1 || GetRowCount() == 0 )
            return NULL;
    
        for( iRow = 0; iRow < GetRowCount(); iRow++ )
            nEntryCount = MAX(nEntryCount,GetValueAsInt(iRow,iMaxCol)+1);

        if( nEntryCount < 0 )
            return NULL;

        // restrict our number of entries to something vaguely sensible
        nEntryCount = MIN(65535,nEntryCount);
    }

/* -------------------------------------------------------------------- */
/*      Assign values to color table.                                   */
/* -------------------------------------------------------------------- */
    GDALColorTable *poCT = new GDALColorTable();
    int iEntry;

    for( iEntry = 0; iEntry < nEntryCount; iEntry++ )
    {
        GDALColorEntry sColor;
        int iRow = GetRowOfValue( iEntry );

        if( iRow == -1 )
        {
            sColor.c1 = sColor.c2 = sColor.c3 = sColor.c4 = 0;
        }
        else
        {
            sColor.c1 = (short) GetValueAsInt( iRow, iRed );
            sColor.c2 = (short) GetValueAsInt( iRow, iGreen );
            sColor.c3 = (short) GetValueAsInt( iRow, iBlue );
            if( iAlpha == -1 )
                sColor.c4 = 255;
            else
                sColor.c4 = (short) GetValueAsInt( iRow, iAlpha );
        }
        
        poCT->SetColorEntry( iEntry, &sColor );
    }

    return poCT;
}

/************************************************************************/
/*                  GDALRATInitializeFromColorTable()                   */
/************************************************************************/

/**
 * \brief Translate to a color table.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::TranslateToColorTable()
 */
GDALColorTableH CPL_STDCALL 
GDALRATTranslateToColorTable( GDALRasterAttributeTableH hRAT,
                              int nEntryCount )
                                 

{
    VALIDATE_POINTER1( hRAT, "GDALRATTranslateToColorTable", NULL );

    return ((GDALRasterAttributeTable *) hRAT)->
        TranslateToColorTable( nEntryCount );
}


/************************************************************************/
/*                            DumpReadable()                            */
/************************************************************************/

/**
 * \brief Dump RAT in readable form.
 *
 * Currently the readable form is the XML encoding ... only barely 
 * readable. 
 *
 * This method is the same as the C function GDALRATDumpReadable().
 *
 * @param fp file to dump to or NULL for stdout. 
 */

void GDALRasterAttributeTable::DumpReadable( FILE * fp )

{
    CPLXMLNode *psTree = Serialize();
    char *pszXMLText = CPLSerializeXMLTree( psTree );

    CPLDestroyXMLNode( psTree );

    if( fp == NULL )
        fp = stdout;

    fprintf( fp, "%s\n", pszXMLText );

    CPLFree( pszXMLText );
}

/************************************************************************/
/*                        GDALRATDumpReadable()                         */
/************************************************************************/

/**
 * \brief Dump RAT in readable form.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::DumpReadable()
 */
void CPL_STDCALL 
GDALRATDumpReadable( GDALRasterAttributeTableH hRAT, FILE *fp )

{
    VALIDATE_POINTER0( hRAT, "GDALRATDumpReadable" );

    ((GDALRasterAttributeTable *) hRAT)->DumpReadable( fp );
}


/* \class GDALDefaultRasterAttributeTable
 * 
 * An implementation of GDALRasterAttributeTable that keeps
 * all data in memory. This is the same as the implementation
 * of GDALRasterAttributeTable in GDAL <= 1.10.
 */

/************************************************************************/
/*                  GDALDefaultRasterAttributeTable()                   */
/*                                                                      */
/*      Simple initialization constructor.                              */
/************************************************************************/

//! Construct empty table.

GDALDefaultRasterAttributeTable::GDALDefaultRasterAttributeTable()

{
    bColumnsAnalysed = FALSE;
    nMinCol = -1;
    nMaxCol = -1;
    bLinearBinning = FALSE;
    dfRow0Min = -0.5;
    dfBinSize = 1.0;
    nRowCount = 0;
}

/************************************************************************/
/*                   GDALCreateRasterAttributeTable()                   */
/************************************************************************/

/**
 * \brief Construct empty table.
 *
 * This function is the same as the C++ method GDALDefaultRasterAttributeTable::GDALDefaultRasterAttributeTable()
 */
GDALRasterAttributeTableH CPL_STDCALL GDALCreateRasterAttributeTable()

{
    return (GDALRasterAttributeTableH) (new GDALDefaultRasterAttributeTable());
}

/************************************************************************/
/*                  GDALDefaultRasterAttributeTable()                   */
/************************************************************************/

//! Copy constructor.

GDALDefaultRasterAttributeTable::GDALDefaultRasterAttributeTable( 
    const GDALDefaultRasterAttributeTable &oOther )

{
    // We have tried to be careful to allow wholesale assignment
    *this = oOther;
}

/************************************************************************/
/*                 ~GDALDefaultRasterAttributeTable()                   */
/*                                                                      */
/*      All magic done by magic by the container destructors.           */
/************************************************************************/

GDALDefaultRasterAttributeTable::~GDALDefaultRasterAttributeTable()

{
}

/************************************************************************/
/*                  GDALDestroyRasterAttributeTable()                   */
/************************************************************************/

/**
 * \brief Destroys a RAT.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::~GDALRasterAttributeTable()
 */
void CPL_STDCALL 
GDALDestroyRasterAttributeTable( GDALRasterAttributeTableH hRAT )

{
    if( hRAT != NULL )
        delete static_cast<GDALRasterAttributeTable *>(hRAT);
}

/************************************************************************/
/*                           AnalyseColumns()                           */
/*                                                                      */
/*      Internal method to work out which column to use for various     */
/*      tasks.                                                          */
/************************************************************************/

void GDALDefaultRasterAttributeTable::AnalyseColumns()

{
    bColumnsAnalysed = TRUE;

    nMinCol = GetColOfUsage( GFU_Min );
    if( nMinCol == -1 )
        nMinCol = GetColOfUsage( GFU_MinMax );

    nMaxCol = GetColOfUsage( GFU_Max );
    if( nMaxCol == -1 )
        nMaxCol = GetColOfUsage( GFU_MinMax );
}

/************************************************************************/
/*                           GetColumnCount()                           */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetColumnCount() const

{
    return aoFields.size();
}

/************************************************************************/
/*                       GDALRATGetColumnCount()                        */
/************************************************************************/

/**
 * \brief Fetch table column count.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetColumnCount()
 */
int CPL_STDCALL GDALRATGetColumnCount( GDALRasterAttributeTableH hRAT )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetColumnCount", 0 );

    return ((GDALRasterAttributeTable *) hRAT)->GetColumnCount();
}

/************************************************************************/
/*                            GetNameOfCol()                            */
/************************************************************************/

const char *GDALDefaultRasterAttributeTable::GetNameOfCol( int iCol ) const

{
    if( iCol < 0 || iCol >= (int) aoFields.size() )
        return "";

    else
        return aoFields[iCol].sName;
}

/************************************************************************/
/*                        GDALRATGetNameOfCol()                         */
/************************************************************************/

/**
 * \brief Fetch name of indicated column.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetNameOfCol()
 */
const char *CPL_STDCALL GDALRATGetNameOfCol( GDALRasterAttributeTableH hRAT,
                                             int iCol )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetNameOfCol", NULL );

    return ((GDALRasterAttributeTable *) hRAT)->GetNameOfCol( iCol );
}

/************************************************************************/
/*                           GetUsageOfCol()                            */
/************************************************************************/

GDALRATFieldUsage GDALDefaultRasterAttributeTable::GetUsageOfCol( int iCol ) const

{
    if( iCol < 0 || iCol >= (int) aoFields.size() )
        return GFU_Generic;

    else
        return aoFields[iCol].eUsage;
}

/************************************************************************/
/*                        GDALRATGetUsageOfCol()                        */
/************************************************************************/

/**
 * \brief Fetch column usage value. 
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetUsageOfColetNameOfCol()
 */
GDALRATFieldUsage CPL_STDCALL 
GDALRATGetUsageOfCol( GDALRasterAttributeTableH hRAT, int iCol )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetUsageOfCol", GFU_Generic );

    return ((GDALRasterAttributeTable *) hRAT)->GetUsageOfCol( iCol );
}

/************************************************************************/
/*                            GetTypeOfCol()                            */
/************************************************************************/

GDALRATFieldType GDALDefaultRasterAttributeTable::GetTypeOfCol( int iCol ) const

{
    if( iCol < 0 || iCol >= (int) aoFields.size() )
        return GFT_Integer;

    else
        return aoFields[iCol].eType;
}

/************************************************************************/
/*                        GDALRATGetTypeOfCol()                         */
/************************************************************************/

/**
 * \brief Fetch column type.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetTypeOfCol()
 */
GDALRATFieldType CPL_STDCALL 
GDALRATGetTypeOfCol( GDALRasterAttributeTableH hRAT, int iCol )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetTypeOfCol", GFT_Integer );

    return ((GDALRasterAttributeTable *) hRAT)->GetTypeOfCol( iCol );
}

/************************************************************************/
/*                           GetColOfUsage()                            */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetColOfUsage( GDALRATFieldUsage eUsage ) const

{
    unsigned int i;

    for( i = 0; i < aoFields.size(); i++ )
    {
        if( aoFields[i].eUsage == eUsage )
            return i;
    }

    return -1;
}

/************************************************************************/
/*                        GDALRATGetColOfUsage()                        */
/************************************************************************/

/**
 * \brief Fetch column index for given usage.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetColOfUsage()
 */
int CPL_STDCALL 
GDALRATGetColOfUsage( GDALRasterAttributeTableH hRAT, 
                      GDALRATFieldUsage eUsage )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetColOfUsage", 0 );

    return ((GDALRasterAttributeTable *) hRAT)->GetColOfUsage( eUsage );
}

/************************************************************************/
/*                            GetRowCount()                             */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetRowCount() const

{
    return (int) nRowCount;
}

/************************************************************************/
/*                        GDALRATGetUsageOfCol()                        */
/************************************************************************/
/**
 * \brief Fetch row count.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetRowCount()
 */
int CPL_STDCALL 
GDALRATGetRowCount( GDALRasterAttributeTableH hRAT )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetRowCount", 0 );

    return ((GDALRasterAttributeTable *) hRAT)->GetRowCount();
}

/************************************************************************/
/*                          GetValueAsString()                          */
/************************************************************************/

const char *
GDALDefaultRasterAttributeTable::GetValueAsString( int iRow, int iField ) const

{
    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return "";
    }

    if( iRow < 0 || iRow >= nRowCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iRow (%d) out of range.", iRow );

        return "";
    }

    switch( aoFields[iField].eType )
    {
      case GFT_Integer:
      {
          ((GDALDefaultRasterAttributeTable *) this)->
              osWorkingResult.Printf( "%d", aoFields[iField].anValues[iRow] );
          return osWorkingResult;
      }

      case GFT_Real:
      {
          ((GDALDefaultRasterAttributeTable *) this)->
             osWorkingResult.Printf( "%.16g", aoFields[iField].adfValues[iRow]);
          return osWorkingResult;
      }

      case GFT_String:
      {
          return aoFields[iField].aosValues[iRow];
      }
    }

    return "";
}

/************************************************************************/
/*                      GDALRATGetValueAsString()                       */
/************************************************************************/
/**
 * \brief Fetch field value as a string.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetValueAsString()
 */
const char * CPL_STDCALL 
GDALRATGetValueAsString( GDALRasterAttributeTableH hRAT, int iRow, int iField )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetValueAsString", NULL );

    return ((GDALRasterAttributeTable *) hRAT)->GetValueAsString(iRow, iField);
}

/************************************************************************/
/*                           GetValueAsInt()                            */
/************************************************************************/

int 
GDALDefaultRasterAttributeTable::GetValueAsInt( int iRow, int iField ) const

{
    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return 0;
    }

    if( iRow < 0 || iRow >= nRowCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iRow (%d) out of range.", iRow );

        return 0;
    }

    switch( aoFields[iField].eType )
    {
      case GFT_Integer:
        return aoFields[iField].anValues[iRow];

      case GFT_Real:
        return (int)  aoFields[iField].adfValues[iRow];

      case GFT_String:
        return atoi( aoFields[iField].aosValues[iRow].c_str() );
    }

    return 0;
}

/************************************************************************/
/*                        GDALRATGetValueAsInt()                        */
/************************************************************************/

/**
 * \brief Fetch field value as a integer.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetValueAsInt()
 */
int CPL_STDCALL 
GDALRATGetValueAsInt( GDALRasterAttributeTableH hRAT, int iRow, int iField )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetValueAsInt", 0 );

    return ((GDALRasterAttributeTable *) hRAT)->GetValueAsInt( iRow, iField );
}

/************************************************************************/
/*                          GetValueAsDouble()                          */
/************************************************************************/

double
GDALDefaultRasterAttributeTable::GetValueAsDouble( int iRow, int iField ) const

{
    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return 0;
    }

    if( iRow < 0 || iRow >= nRowCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iRow (%d) out of range.", iRow );

        return 0;
    }

    switch( aoFields[iField].eType )
    {
      case GFT_Integer:
        return aoFields[iField].anValues[iRow];

      case GFT_Real:
        return aoFields[iField].adfValues[iRow];

      case GFT_String:
        return CPLAtof( aoFields[iField].aosValues[iRow].c_str() );
    }

    return 0;
}

/************************************************************************/
/*                      GDALRATGetValueAsDouble()                       */
/************************************************************************/

/**
 * \brief Fetch field value as a double.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::GetValueAsDouble()
 */
double CPL_STDCALL 
GDALRATGetValueAsDouble( GDALRasterAttributeTableH hRAT, int iRow, int iField )

{
    VALIDATE_POINTER1( hRAT, "GDALRATGetValueAsDouble", 0 );

    return ((GDALRasterAttributeTable *) hRAT)->GetValueAsDouble(iRow,iField);
}

/************************************************************************/
/*                            SetRowCount()                             */
/************************************************************************/

void GDALDefaultRasterAttributeTable::SetRowCount( int nNewCount )

{
    if( nNewCount == nRowCount )
        return;

    unsigned int iField;
    for( iField = 0; iField < aoFields.size(); iField++ )
    {
        switch( aoFields[iField].eType )
        {
          case GFT_Integer:
            aoFields[iField].anValues.resize( nNewCount );
            break;

          case GFT_Real:
            aoFields[iField].adfValues.resize( nNewCount );
            break;

          case GFT_String:
            aoFields[iField].aosValues.resize( nNewCount );
            break;
        }
    }

    nRowCount = nNewCount;
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

void GDALDefaultRasterAttributeTable::SetValue( int iRow, int iField, 
                                         const char *pszValue )

{
    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return;
    }

    if( iRow == nRowCount )
        SetRowCount( nRowCount+1 );
    
    if( iRow < 0 || iRow >= nRowCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iRow (%d) out of range.", iRow );

        return;
    }

    switch( aoFields[iField].eType )
    {
      case GFT_Integer:
        aoFields[iField].anValues[iRow] = atoi(pszValue);
        break;
        
      case GFT_Real:
        aoFields[iField].adfValues[iRow] = CPLAtof(pszValue);
        break;
        
      case GFT_String:
        aoFields[iField].aosValues[iRow] = pszValue;
        break;
    }
}

/************************************************************************/
/*                      GDALRATSetValueAsString()                       */
/************************************************************************/

/**
 * \brief Set field value from string.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::SetValue()
 */
void CPL_STDCALL 
GDALRATSetValueAsString( GDALRasterAttributeTableH hRAT, int iRow, int iField,
                         const char *pszValue )

{
    VALIDATE_POINTER0( hRAT, "GDALRATSetValueAsString" );

    ((GDALRasterAttributeTable *) hRAT)->SetValue( iRow, iField, pszValue );
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

void GDALDefaultRasterAttributeTable::SetValue( int iRow, int iField, 
                                         int nValue )

{
    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return;
    }

    if( iRow == nRowCount )
        SetRowCount( nRowCount+1 );
    
    if( iRow < 0 || iRow >= nRowCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iRow (%d) out of range.", iRow );

        return;
    }

    switch( aoFields[iField].eType )
    {
      case GFT_Integer:
        aoFields[iField].anValues[iRow] = nValue;
        break;
        
      case GFT_Real:
        aoFields[iField].adfValues[iRow] = nValue;
        break;
        
      case GFT_String:
      {
          char szValue[100];

          sprintf( szValue, "%d", nValue );
          aoFields[iField].aosValues[iRow] = szValue;
      }
      break;
    }
}

/************************************************************************/
/*                        GDALRATSetValueAsInt()                        */
/************************************************************************/

/**
 * \brief Set field value from integer.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::SetValue()
 */
void CPL_STDCALL 
GDALRATSetValueAsInt( GDALRasterAttributeTableH hRAT, int iRow, int iField,
                      int nValue )

{
    VALIDATE_POINTER0( hRAT, "GDALRATSetValueAsInt" );

    ((GDALRasterAttributeTable *) hRAT)->SetValue( iRow, iField, nValue);
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

void GDALDefaultRasterAttributeTable::SetValue( int iRow, int iField, 
                                         double dfValue )

{
    if( iField < 0 || iField >= (int) aoFields.size() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iField (%d) out of range.", iField );

        return;
    }

    if( iRow == nRowCount )
        SetRowCount( nRowCount+1 );
    
    if( iRow < 0 || iRow >= nRowCount )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "iRow (%d) out of range.", iRow );

        return;
    }

    switch( aoFields[iField].eType )
    {
      case GFT_Integer:
        aoFields[iField].anValues[iRow] = (int) dfValue;
        break;
        
      case GFT_Real:
        aoFields[iField].adfValues[iRow] = dfValue;
        break;
        
      case GFT_String:
      {
          char szValue[100];

          CPLsprintf( szValue, "%.15g", dfValue );
          aoFields[iField].aosValues[iRow] = szValue;
      }
      break;
    }
}

/************************************************************************/
/*                      GDALRATSetValueAsDouble()                       */
/************************************************************************/

/**
 * \brief Set field value from double.
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::SetValue()
 */
void CPL_STDCALL 
GDALRATSetValueAsDouble( GDALRasterAttributeTableH hRAT, int iRow, int iField,
                         double dfValue )

{
    VALIDATE_POINTER0( hRAT, "GDALRATSetValueAsDouble" );

    ((GDALRasterAttributeTable *) hRAT)->SetValue( iRow, iField, dfValue );
}

/************************************************************************/
/*                       ChangesAreWrittenToFile()                      */
/************************************************************************/

int GDALDefaultRasterAttributeTable::ChangesAreWrittenToFile()
{
    // GDALRasterBand.SetDefaultRAT needs to be called on instances of 
    // GDALDefaultRasterAttributeTable since changes are just in-memory
    return FALSE;
}

/************************************************************************/
/*                   GDALRATChangesAreWrittenToFile()                   */
/************************************************************************/

/**
 * \brief Determine whether changes made to this RAT are reflected directly in the dataset
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::ChangesAreWrittenToFile()
 */
int CPL_STDCALL
GDALRATChangesAreWrittenToFile( GDALRasterAttributeTableH hRAT )
{
    VALIDATE_POINTER1( hRAT, "GDALRATChangesAreWrittenToFile", FALSE );

    return ((GDALRasterAttributeTable *) hRAT)->ChangesAreWrittenToFile();
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetRowOfValue( double dfValue ) const

{
/* -------------------------------------------------------------------- */
/*      Handle case of regular binning.                                 */
/* -------------------------------------------------------------------- */
    if( bLinearBinning )
    {
        int iBin = (int) floor((dfValue - dfRow0Min) / dfBinSize);
        if( iBin < 0 || iBin >= nRowCount )
            return -1;
        else
            return iBin;
    }

/* -------------------------------------------------------------------- */
/*      Do we have any information?                                     */
/* -------------------------------------------------------------------- */
    const GDALRasterAttributeField *poMin, *poMax;

    if( !bColumnsAnalysed )
        ((GDALDefaultRasterAttributeTable *) this)->AnalyseColumns();

    if( nMinCol == -1 && nMaxCol == -1 )
        return -1;

    if( nMinCol != -1 )
        poMin = &(aoFields[nMinCol]);
    else
        poMin = NULL;
    
    if( nMaxCol != -1 )
        poMax = &(aoFields[nMaxCol]);
    else
        poMax = NULL;
    
/* -------------------------------------------------------------------- */
/*      Search through rows for match.                                  */
/* -------------------------------------------------------------------- */
    int   iRow;

    for( iRow = 0; iRow < nRowCount; iRow++ )
    {
        if( poMin != NULL )
        {
            if( poMin->eType == GFT_Integer )
            {
                while( iRow < nRowCount && dfValue < poMin->anValues[iRow] ) 
                    iRow++;
            }
            else if( poMin->eType == GFT_Real )
            {
                while( iRow < nRowCount && dfValue < poMin->adfValues[iRow] )
                    iRow++;
            }

            if( iRow == nRowCount )
                break;
        }

        if( poMax != NULL )
        {
            if( (poMax->eType == GFT_Integer 
                 && dfValue > poMax->anValues[iRow] ) 
                || (poMax->eType == GFT_Real 
                    && dfValue > poMax->adfValues[iRow] ) )
                continue;
        }

        return iRow;
    }

    return -1;
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/*                                                                      */
/*      Int arg for now just converted to double.  Perhaps we will      */
/*      handle this in a special way some day?                          */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetRowOfValue( int nValue ) const

{
    return GetRowOfValue( (double) nValue );
}

/************************************************************************/
/*                          SetLinearBinning()                          */
/************************************************************************/

CPLErr GDALDefaultRasterAttributeTable::SetLinearBinning( double dfRow0MinIn, 
                                                   double dfBinSizeIn )

{
    bLinearBinning = TRUE;
    dfRow0Min = dfRow0MinIn;
    dfBinSize = dfBinSizeIn;

    return CE_None;
}

/************************************************************************/
/*                          GetLinearBinning()                          */
/************************************************************************/

int GDALDefaultRasterAttributeTable::GetLinearBinning( double *pdfRow0Min,
                                                double *pdfBinSize ) const

{
    if( !bLinearBinning )
        return FALSE;

    *pdfRow0Min = dfRow0Min;
    *pdfBinSize = dfBinSize;

    return TRUE;
}

/************************************************************************/
/*                            CreateColumn()                            */
/************************************************************************/

CPLErr GDALDefaultRasterAttributeTable::CreateColumn( const char *pszFieldName, 
                                               GDALRATFieldType eFieldType,
                                               GDALRATFieldUsage eFieldUsage )

{
    int iNewField = aoFields.size();

    aoFields.resize( iNewField+1 );

    aoFields[iNewField].sName = pszFieldName;

    // color columns should be int 0..255 
    if( ( eFieldUsage == GFU_Red ) || ( eFieldUsage == GFU_Green ) || 
        ( eFieldUsage == GFU_Blue ) || ( eFieldUsage == GFU_Alpha ) )
    {
        eFieldType = GFT_Integer;
    }
    aoFields[iNewField].eType = eFieldType;
    aoFields[iNewField].eUsage = eFieldUsage;

    if( eFieldType == GFT_Integer )
        aoFields[iNewField].anValues.resize( nRowCount );
    else if( eFieldType == GFT_Real )
        aoFields[iNewField].adfValues.resize( nRowCount );
    else if( eFieldType == GFT_String )
        aoFields[iNewField].aosValues.resize( nRowCount );

    return CE_None;
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

GDALDefaultRasterAttributeTable *GDALDefaultRasterAttributeTable::Clone() const

{
    return new GDALDefaultRasterAttributeTable( *this );
}

/************************************************************************/
/*                            GDALRATClone()                            */
/************************************************************************/

/**
 * \brief Copy Raster Attribute Table
 *
 * This function is the same as the C++ method GDALRasterAttributeTable::Clone()
 */
GDALRasterAttributeTableH CPL_STDCALL 
GDALRATClone( GDALRasterAttributeTableH hRAT )

{
    VALIDATE_POINTER1( hRAT, "GDALRATClone", NULL );

    return ((GDALRasterAttributeTable *) hRAT)->Clone();
}
