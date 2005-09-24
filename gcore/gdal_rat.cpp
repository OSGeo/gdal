/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALRasterAttributeTable and related classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2005/09/24 19:00:53  fwarmerdam
 * New
 *
 */

#include "gdal_priv.h"
#include "gdal_rat.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      GDALRasterAttributeTable()                      */
/*                                                                      */
/*      Simple initialization constructor.                              */
/************************************************************************/

GDALRasterAttributeTable::GDALRasterAttributeTable()

{
    nMinCol = -1;
    nMaxCol = -1;
    bRegularBinning = FALSE;
    dfRow0Min = -0.5;
    dfBinSize = 1.0;
    nRowCount = 0;
}

/************************************************************************/
/*                   GDALCreateRasterAttributeTable()                   */
/************************************************************************/

GDALRasterAttributeTableH CPL_STDCALL GDALCreateRasterAttributeTable()

{
    return (GDALRasterAttributeTableH) (new GDALRasterAttributeTable());
}

/************************************************************************/
/*                      GDALRasterAttributeTable()                      */
/*                                                                      */
/*      Copy constructor.                                               */
/************************************************************************/

GDALRasterAttributeTable::GDALRasterAttributeTable( 
    const GDALRasterAttributeTable &oOther )

{
    // We have tried to be careful to allow wholesale assignment
    *this = oOther;
}

/************************************************************************/
/*                     ~GDALRasterAttributeTable()                      */
/************************************************************************/

GDALRasterAttributeTable::~GDALRasterAttributeTable()

{
}

/************************************************************************/
/*                  GDALDestroyRasterAttributeTable()                   */
/************************************************************************/

void CPL_STDCALL 
GDALDestroyRasterAttributeTable( GDALRasterAttributeTableH hRAT )

{
    if( hRAT != NULL )
        delete (GDALRasterAttributeTable *) hRAT;
}

/************************************************************************/
/*                           GetColumnCount()                           */
/************************************************************************/

int GDALRasterAttributeTable::GetColumnCount() const

{
    return aoFields.size();
}

/************************************************************************/
/*                       GDALRATGetColumnCount()                        */
/************************************************************************/

int CPL_STDCALL GDALRATGetColumnCount( GDALRasterAttributeTableH hRAT )

{
    return ((GDALRasterAttributeTable *) hRAT)->GetColumnCount();
}

/************************************************************************/
/*                            GetNameOfCol()                            */
/************************************************************************/

CPLString GDALRasterAttributeTable::GetNameOfCol( int iCol ) const

{
    if( iCol < 0 || iCol >= (int) aoFields.size() )
        return "";

    else
        return aoFields[iCol].sName;
}

/************************************************************************/
/*                        GDALRATGetNameOfCol()                         */
/************************************************************************/

const char *CPL_STDCALL GDALRATGetNameOfCol( GDALRasterAttributeTableH hRAT,
                                             int iCol )

{
    // we don't just wrap the normal operator because we don't want to
    // return a temporary string, we want to return a pointer to the
    // internal column name. 

    GDALRasterAttributeTable *poRAT = (GDALRasterAttributeTable *) hRAT;

    if( iCol < 0 || iCol >= (int) poRAT->aoFields.size() )
        return "";

    else
        return poRAT->aoFields[iCol].sName.c_str();
}

/************************************************************************/
/*                           GetUsageOfCol()                            */
/************************************************************************/

GDALRATFieldUsage GDALRasterAttributeTable::GetUsageOfCol( int iCol ) const

{
    if( iCol < 0 || iCol >= (int) aoFields.size() )
        return GFU_Generic;

    else
        return aoFields[iCol].eUsage;
}

/************************************************************************/
/*                        GDALRATGetUsageOfCol()                        */
/************************************************************************/

GDALRATFieldUsage CPL_STDCALL 
GDALRATGetUsageOfCol( GDALRasterAttributeTableH hRAT, int iCol )

{
    return ((GDALRasterAttributeTable *) hRAT)->GetUsageOfCol( iCol );
}

/************************************************************************/
/*                            GetTypeOfCol()                            */
/************************************************************************/

GDALRATFieldType GDALRasterAttributeTable::GetTypeOfCol( int iCol ) const

{
    if( iCol < 0 || iCol >= (int) aoFields.size() )
        return GFT_Integer;

    else
        return aoFields[iCol].eType;
}

/************************************************************************/
/*                        GDALRATGetTypeOfCol()                         */
/************************************************************************/

GDALRATFieldType CPL_STDCALL 
GDALRATGetTypeOfCol( GDALRasterAttributeTableH hRAT, int iCol )

{
    return ((GDALRasterAttributeTable *) hRAT)->GetTypeOfCol( iCol );
}

/************************************************************************/
/*                           GetColOfUsage()                            */
/************************************************************************/

int GDALRasterAttributeTable::GetColOfUsage( GDALRATFieldUsage eUsage ) const

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
/*                        GDALRATGetUsageOfCol()                        */
/************************************************************************/

int CPL_STDCALL 
GDALRATGetColOfUsage( GDALRasterAttributeTableH hRAT, 
                      GDALRATFieldUsage eUsage )

{
    return ((GDALRasterAttributeTable *) hRAT)->GetColOfUsage( eUsage );
}

/************************************************************************/
/*                            GetRowCount()                             */
/************************************************************************/

int GDALRasterAttributeTable::GetRowCount() const

{
    return (int) nRowCount;
}

/************************************************************************/
/*                        GDALRATGetUsageOfCol()                        */
/************************************************************************/

int CPL_STDCALL 
GDALRATGetRowCount( GDALRasterAttributeTableH hRAT )

{
    return ((GDALRasterAttributeTable *) hRAT)->GetRowCount();
}

/************************************************************************/
/*                          GetValueAsString()                          */
/************************************************************************/

CPLString
GDALRasterAttributeTable::GetValueAsString( int iRow, int iField ) const

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
          char szValue[100];
          sprintf( szValue, "%d", aoFields[iField].anValues[iRow] );
          return szValue;
      }

      case GFT_Real:
      {
          char szValue[100];
          sprintf( szValue, "%.16g", aoFields[iField].adfValues[iRow] );
          return szValue;
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

const char * CPL_STDCALL 
GDALRATGetValueAsString( GDALRasterAttributeTableH hRAT, int iRow, int iField )

{
    GDALRasterAttributeTable *poRAT = (GDALRasterAttributeTable *) hRAT;

    poRAT->osWorkingResult = poRAT->GetValueAsString( iRow, iField );
    
    return poRAT->osWorkingResult.c_str();
}

/************************************************************************/
/*                           GetValueAsInt()                            */
/************************************************************************/

int 
GDALRasterAttributeTable::GetValueAsInt( int iRow, int iField ) const

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

int CPL_STDCALL 
GDALRATGetValueAsInt( GDALRasterAttributeTableH hRAT, int iRow, int iField )

{
    return ((GDALRasterAttributeTable *) hRAT)->GetValueAsInt( iRow, iField );
}

/************************************************************************/
/*                          GetValueAsDouble()                          */
/************************************************************************/

double
GDALRasterAttributeTable::GetValueAsDouble( int iRow, int iField ) const

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
        return atof( aoFields[iField].aosValues[iRow].c_str() );
    }

    return 0;
}

/************************************************************************/
/*                      GDALRATGetValueAsDouble()                       */
/************************************************************************/

double CPL_STDCALL 
GDALRATGetValueAsDouble( GDALRasterAttributeTableH hRAT, int iRow, int iField )

{
    return ((GDALRasterAttributeTable *) hRAT)->GetValueAsDouble(iRow,iField);
}

/************************************************************************/
/*                            SetRowCount()                             */
/************************************************************************/

void GDALRasterAttributeTable::SetRowCount( int nNewCount )

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
/*                         GDALRATSetRowCount()                         */
/************************************************************************/

void CPL_STDCALL 
GDALRATSetRowCount( GDALRasterAttributeTableH hRAT, int nNewCount )

{
    return ((GDALRasterAttributeTable *) hRAT)->SetRowCount( nNewCount );
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

void GDALRasterAttributeTable::SetValue( int iRow, int iField, 
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
        aoFields[iField].adfValues[iRow] = atof(pszValue);
        break;
        
      case GFT_String:
        aoFields[iField].aosValues[iRow] = pszValue;
        break;
    }
}

/************************************************************************/
/*                      GDALRATSetValueAsString()                       */
/************************************************************************/

void CPL_STDCALL 
GDALRATSetValueAsString( GDALRasterAttributeTableH hRAT, int iRow, int iField,
                         const char *pszValue )

{
    return ((GDALRasterAttributeTable *) hRAT)->SetValue(
        iRow, iField, pszValue );
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

void GDALRasterAttributeTable::SetValue( int iRow, int iField, 
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

void CPL_STDCALL 
GDALRATSetValueAsInt( GDALRasterAttributeTableH hRAT, int iRow, int iField,
                      int nValue )

{
    return ((GDALRasterAttributeTable *) hRAT)->SetValue(
        iRow, iField, nValue);
}

/************************************************************************/
/*                              SetValue()                              */
/************************************************************************/

void GDALRasterAttributeTable::SetValue( int iRow, int iField, 
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

          sprintf( szValue, "%.15g", dfValue );
          aoFields[iField].aosValues[iRow] = szValue;
      }
      break;
    }
}

/************************************************************************/
/*                      GDALRATSetValueAsDouble()                       */
/************************************************************************/

void CPL_STDCALL 
GDALRATSetValueAsDouble( GDALRasterAttributeTableH hRAT, int iRow, int iField,
                         double dfValue )

{
    return ((GDALRasterAttributeTable *) hRAT)->SetValue(
        iRow, iField, dfValue);
}

/************************************************************************/
/*                           GetRowOfValue()                            */
/************************************************************************/

int GDALRasterAttributeTable::GetRowOfValue( double dfValue ) const

{
/* -------------------------------------------------------------------- */
/*      Handle case of regular binning.                                 */
/* -------------------------------------------------------------------- */
    if( bRegularBinning )
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
/*                          SetLinearBinning()                          */
/************************************************************************/

CPLErr GDALRasterAttributeTable::SetLinearBinning( double dfRow0MinIn, 
                                                   double dfBinSizeIn )

{
    bRegularBinning = TRUE;
    dfRow0Min = dfRow0MinIn;
    dfBinSize = dfBinSizeIn;

    return CE_None;
}

/************************************************************************/
/*                          GetLinearBinning()                          */
/************************************************************************/

int GDALRasterAttributeTable::GetRegularBinning( double *pdfRow0Min,
                                                 double *pdfBinSize ) const

{
    if( !bRegularBinning )
        return FALSE;

    *pdfRow0Min = dfRow0Min;
    *pdfBinSize = dfBinSize;

    return TRUE;
}

/************************************************************************/
/*                            CreateColumn()                            */
/************************************************************************/

CPLErr GDALRasterAttributeTable::CreateColumn( CPLString osFieldName, 
                                               GDALRATFieldType eFieldType,
                                               GDALRATFieldUsage eFieldUsage )

{
    int iNewField = aoFields.size();

    aoFields.resize( iNewField+1 );

    aoFields[iNewField].sName = osFieldName;
    aoFields[iNewField].eType = eFieldType;
    aoFields[iNewField].eUsage = eFieldUsage;

    if( eFieldType == GFT_Integer )
        aoFields[iNewField].anValues.resize( nRowCount );
    else if( eFieldType == GFT_Real )
        aoFields[iNewField].adfValues.resize( nRowCount );
    else if( eFieldType == GFT_Integer )
        aoFields[iNewField].aosValues.resize( nRowCount );

    return CE_None;
}

/************************************************************************/
/*                        GDALRATCreateColumn()                         */
/************************************************************************/

CPLErr CPL_STDCALL GDALRATCreateColumn( GDALRasterAttributeTableH hRAT, 
                                        const char *pszFieldName, 
                                        GDALRATFieldType eFieldType,
                                        GDALRATFieldUsage eFieldUsage )

{
    return ((GDALRasterAttributeTable *) hRAT)->CreateColumn( pszFieldName, 
                                                              eFieldType,
                                                              eFieldUsage );
}

/************************************************************************/
/*                      InitializeFromColorTable()                      */
/************************************************************************/

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

CPLErr CPL_STDCALL 
GDALRATInitializeFromColorTable( GDALRasterAttributeTableH hRAT,
                                 GDALColorTableH hCT )
                                 

{
    return ((GDALRasterAttributeTable *) hRAT)->
        InitializeFromColorTable( (GDALColorTable *) hCT );
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
        SetLinearBinning( atof(CPLGetXMLValue( psTree, "Row0Min","" )), 
                          atof(CPLGetXMLValue( psTree, "BinSize","" )) );
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

                SetValue( iRow, iField++, psF->psChild->pszValue );
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

CPLXMLNode *GDALRasterAttributeTable::Serialize() const

{
    CPLXMLNode *psTree = NULL;
    CPLXMLNode *psRow = NULL;

    psTree = CPLCreateXMLNode( NULL, CXT_Element, "GDALRasterAttributeTable" );

/* -------------------------------------------------------------------- */
/*      Add attributes with regular binning info if appropriate.        */
/* -------------------------------------------------------------------- */
    char szValue[128];

    if( bRegularBinning )
    {
        sprintf( szValue, "%.16g", dfRow0Min );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psTree, CXT_Attribute, "Row0Min" ), 
            CXT_Text, szValue );

        sprintf( szValue, "%.16g", dfBinSize );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psTree, CXT_Attribute, "BinSize" ), 
            CXT_Text, szValue );
    }

/* -------------------------------------------------------------------- */
/*      Define each column.                                             */
/* -------------------------------------------------------------------- */
    int iCol;

    for( iCol = 0; iCol < (int) aoFields.size(); iCol++ )
    {
        CPLXMLNode *psCol;

        psCol = CPLCreateXMLNode( psTree, CXT_Element, "FieldDefn" );
        
        sprintf( szValue, "%d", iCol );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psCol, CXT_Attribute, "index" ), 
            CXT_Text, szValue );

        CPLCreateXMLElementAndValue( psCol, "Name", 
                                     aoFields[iCol].sName.c_str() );

        sprintf( szValue, "%d", (int) aoFields[iCol].eType );
        CPLCreateXMLElementAndValue( psCol, "Type", szValue );

        sprintf( szValue, "%d", (int) aoFields[iCol].eUsage );
        CPLCreateXMLElementAndValue( psCol, "Usage", szValue );
    }

/* -------------------------------------------------------------------- */
/*      Write out each row.                                             */
/* -------------------------------------------------------------------- */
    int iRow;

    for( iRow = 0; iRow < nRowCount; iRow++ )
    {
        psRow = CPLCreateXMLNode( psTree, CXT_Element, "Row" );

        sprintf( szValue, "%d", iRow );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psRow, CXT_Attribute, "index" ), 
            CXT_Text, szValue );

        for( iCol = 0; iCol < (int) aoFields.size(); iCol++ )
        {
            const char *pszValue = szValue;

            if( aoFields[iCol].eType == GFT_Integer )
                sprintf( szValue, "%d", aoFields[iCol].anValues[iRow] );
            else if( aoFields[iCol].eType == GFT_Real )
                sprintf( szValue, "%.16g", aoFields[iCol].adfValues[iRow] );
            else
                pszValue = aoFields[iCol].aosValues[iRow].c_str();

            CPLCreateXMLElementAndValue( psRow, "F", pszValue );
        }
    }

    return psTree;
}

/************************************************************************/
/*                            DumpReadable()                            */
/************************************************************************/

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

void CPL_STDCALL 
GDALRATDumpReadable( GDALRasterAttributeTableH hRAT, FILE *fp )

{
    return ((GDALRasterAttributeTable *) hRAT)->DumpReadable( fp );
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

GDALRasterAttributeTable *GDALRasterAttributeTable::Clone() const

{
    return new GDALRasterAttributeTable( *this );
}

/************************************************************************/
/*                            GDALRATClone()                            */
/************************************************************************/

GDALRasterAttributeTableH CPL_STDCALL 
GDALRATClone( GDALRasterAttributeTableH hRAT )

{
    return ((GDALRasterAttributeTable *) hRAT)->Clone();
}
