/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIngresResultLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_conv.h"
#include "ogr_ingres.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* Macro for check the data type is geometry type                       */
/************************************************************************/
#define  IS_IIAPI_GEOM_TYPE(t) ((t)==IIAPI_GEOM_TYPE \
    || (t)==IIAPI_POINT_TYPE || (t)==IIAPI_LINE_TYPE || (t)==IIAPI_POLY_TYPE \
    || (t)==IIAPI_MPOINT_TYPE || (t)==IIAPI_MLINE_TYPE || (t)==IIAPI_MPOLY_TYPE \
    || (t)==IIAPI_GEOMC_TYPE)

/************************************************************************/
/*                        OGRIngresResultLayer()                         */
/************************************************************************/

OGRIngresResultLayer::OGRIngresResultLayer( OGRIngresDataSource *poDSIn, 
                                            const char * pszRawQueryIn,
                                            OGRIngresStatement *poResultSetIn )
{
    poDS = poDSIn;

    iNextShapeId = 0;

    pszRawStatement = CPLStrdup(pszRawQueryIn);

    poResultSet = poResultSetIn;

    poFeatureDefn = ReadResultDefinition();

    BuildFullQueryStatement();    
}

/************************************************************************/
/*                        ~OGRIngresResultLayer()                        */
/************************************************************************/

OGRIngresResultLayer::~OGRIngresResultLayer()

{
    CPLFree( pszRawStatement );
}

/************************************************************************/
/*                    ParseSQLStmt                                      */
/************************************************************************/

OGRErr OGRIngresResultLayer::ParseSQLStmt(OGRIngresSelectStmt& oSelectStmt,
                                          const char* pszRawSQL)
{
    if (pszRawSQL == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Sql Statement is null.");
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /* 																*/
    /* -------------------------------------------------------------------- */
    char *pszSQL = CPLStrdup(pszRawSQL); 
    pszSQL = CPLStrlwr(pszSQL);
    char *pStart = pszSQL; 
    char *pEnd = pszSQL;
    char *pCursor = 0;
    bool bNewField = true;
    CPLString osString;

    /* -------------------------------------------------------------------- */
    /* Resolve SELECT														*/
    /* -------------------------------------------------------------------- */
    pStart = strstr(pszSQL, "select");
    if (pStart == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Sql is not a select SQL: %s", 
            pszSQL);
        CPLFree(pszSQL);

        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*  Now we try to parse the select list									*/
    /* -------------------------------------------------------------------- */
    pEnd = strstr(pszSQL, "from");
    if (pEnd == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Sql is not a select SQL: %s", 
            pszSQL);
        CPLFree(pszSQL);

        return OGRERR_FAILURE;
    }

    pStart += strlen("select");

    for (pCursor = pStart ; pCursor <= pEnd; pCursor++)
    {
        if (*pCursor == ' ' || *pCursor == '\t' )
        {
            continue;
        }
        else if (*pCursor == ',')
        {
            /* we got a new field */
            osString = pStart;
            osString = osString.substr(0, pCursor-pStart);
            osString.Trim();
            oSelectStmt.papszFieldList = CSLAddString(oSelectStmt.papszFieldList, 
                osString.c_str());
            bNewField = true;
            continue;
        }
        else if (pCursor == pEnd)
        {
            osString = pStart;
            osString = osString.substr(0, pCursor-pStart);
            osString.Trim();
            oSelectStmt.papszFieldList = CSLAddString(oSelectStmt.papszFieldList, 
                osString.c_str());
            bNewField = false;
        }        
        else 
        {
            if (bNewField)
            {
                pStart = pCursor;
                bNewField = false;
            }            
        }        
    }
    
    /* -------------------------------------------------------------------- */
    /*  From List															*/
    /* -------------------------------------------------------------------- */
    pStart = pEnd;
    pStart += strlen("from");

    pEnd = strstr(pStart, "where");
    if (pEnd == NULL)
    {
        /* no where clause */
        oSelectStmt.osFromList = pStart;
        oSelectStmt.osFromList.Trim();
    }
    else
    {
        oSelectStmt.osFromList = pStart;
        oSelectStmt.osFromList = oSelectStmt.osFromList.substr(0, pEnd-pStart);
        oSelectStmt.osFromList.Trim();

        /* Where clause */
        /* because where clause may case sensitive, so we must use the 
           original one */
        int nOffset = 0;
        pStart = strstr(pszSQL, "where");
        nOffset = pStart - pszSQL;
        pStart = (char *)pszRawSQL + nOffset;
        pStart += strlen("where");

        oSelectStmt.osWhereClause = pStart;
        oSelectStmt.osWhereClause.Trim();
    }
    
    CPLFree(pszSQL);

    return OGRERR_NONE;
}

/************************************************************************/
/*                   ReparseQueryStatement                              */
/************************************************************************/

OGRErr OGRIngresResultLayer::ReparseQueryStatement()
{
    if (pszRawStatement == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Sql Statement is null.");
        return OGRERR_FAILURE;
    }
    
    OGRIngresSelectStmt oSelectStmt;
    CPLString osNewSQL;

    /* -------------------------------------------------------------------- */
    /*  Parse the raw statement into select statement class         		*/
    /* -------------------------------------------------------------------- */
    if (ParseSQLStmt(oSelectStmt, pszRawStatement) != OGRERR_NONE)
    {
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /* Rebuild the Sql statement											*/
    /* -------------------------------------------------------------------- */
    osNewSQL = "SELECT ";
    int nFieldCount = CSLCount(oSelectStmt.papszFieldList);

    for(int iRawField = 0; 
        iRawField < (int) poResultSet->getDescrParm.gd_descriptorCount; 
        iRawField++ )
    {
        IIAPI_DESCRIPTOR *psFDesc = 
            poResultSet->getDescrParm.gd_descriptor + iRawField;
        CPLString osFieldName;

        if (iRawField >= nFieldCount
            || EQUAL(CSLGetField(oSelectStmt.papszFieldList, iRawField) , "*"))
        {
            osFieldName = psFDesc->ds_columnName;
        }
        else
        {
            osFieldName = CSLGetField(oSelectStmt.papszFieldList, iRawField);
        }
        
        if (iRawField)
        {
            osNewSQL += ", ";
        }        
        
        if (IS_IIAPI_GEOM_TYPE(psFDesc->ds_dataType))
        {
            // @todo 
            // if there is a alias from geometry column, need parse
            osNewSQL += "ASBINARY(";
            osNewSQL += osFieldName;
            osNewSQL += ") AS ";
            osNewSQL += psFDesc->ds_columnName;
        }
        else
        {
            osNewSQL += osFieldName;
        }
        
        osNewSQL += " ";
    }

    osNewSQL += " FROM ";
    osNewSQL += oSelectStmt.osFromList;

    if (oSelectStmt.osWhereClause.size())
    {   
        osNewSQL += " WHERE ";
        osNewSQL += oSelectStmt.osWhereClause;
    }
    
    /* -------------------------------------------------------------------- */
    /* Replace the old sql with the new one									*/
    /* -------------------------------------------------------------------- */
    CPLFree( pszRawStatement );
    pszRawStatement = CPLStrdup(osNewSQL.c_str());

    BuildFullQueryStatement();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIngresResultLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return osFIDColumn.size() != 0;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCCreateField) )
        return FALSE;

    else if( EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCDeleteFeature) )
        return FALSE;

    else 
        return OGRIngresLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                        ReadResultDefinition()                        */
/*                                                                      */
/*      Build a schema from the current resultset.                      */
/************************************************************************/

OGRFeatureDefn *OGRIngresResultLayer::ReadResultDefinition()

{
/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( "sql_statement" );
    int            iRawField;
    OGRwkbGeometryType eType = wkbNone;

    poDefn->Reference();

    for( iRawField = 0; 
         iRawField < (int) poResultSet->getDescrParm.gd_descriptorCount; 
         iRawField++ )
    {
        IIAPI_DESCRIPTOR *psFDesc = 
            poResultSet->getDescrParm.gd_descriptor + iRawField;
        OGRFieldDefn    oField( psFDesc->ds_columnName, OFTString);

        switch( psFDesc->ds_dataType )
        {
          case IIAPI_CHR_TYPE:
          case IIAPI_CHA_TYPE:
            // string - fixed width.
            oField.SetWidth( psFDesc->ds_length );
            poDefn->AddFieldDefn( &oField );
            break;

          case IIAPI_LVCH_TYPE:
          case IIAPI_LTXT_TYPE:
          case IIAPI_VCH_TYPE:
          case IIAPI_TXT_TYPE:
            // default variable length string
            poDefn->AddFieldDefn( &oField );
            break;

          case IIAPI_INT_TYPE:
            oField.SetType( OFTInteger );
            poDefn->AddFieldDefn( &oField );

            // if osFIDColumn is not defined, we have to use the first interger
            // column. better way?
            if (osFIDColumn.size() == 0)
            {
                osFIDColumn = psFDesc->ds_columnName;
            }
            break;

          case IIAPI_FLT_TYPE:
            oField.SetType( OFTReal );
            poDefn->AddFieldDefn( &oField );
            break;

          case IIAPI_DEC_TYPE:
            oField.SetWidth( psFDesc->ds_precision );
            if( psFDesc->ds_scale == 0 )
                oField.SetType( OFTInteger );
            else
            {
                oField.SetType( OFTReal );
                oField.SetPrecision( psFDesc->ds_scale );
            }
            poDefn->AddFieldDefn( &oField );
            break;

          case IIAPI_GEOM_TYPE:
              osGeomColumn = psFDesc->ds_columnName;
              eType = wkbUnknown;
              break;

          case IIAPI_POINT_TYPE:
              osGeomColumn = psFDesc->ds_columnName;
              eType =  wkbPoint;
              break;

          case IIAPI_LINE_TYPE:
              osGeomColumn = psFDesc->ds_columnName;
              eType =  wkbLineString;
              break;

          case IIAPI_POLY_TYPE:
              osGeomColumn = psFDesc->ds_columnName;
              eType =  wkbPolygon;
              break;

          case IIAPI_MPOINT_TYPE:
              osGeomColumn = psFDesc->ds_columnName;
              eType =  wkbMultiPoint;
              break;

          case IIAPI_MLINE_TYPE:
              osGeomColumn = psFDesc->ds_columnName;
              eType =  wkbMultiLineString;
              break;

          case IIAPI_MPOLY_TYPE:
              osGeomColumn = psFDesc->ds_columnName;
              eType =  wkbMultiPolygon;
              break;

          case IIAPI_GEOMC_TYPE:
              osGeomColumn = psFDesc->ds_columnName;
              eType =  wkbGeometryCollection;
              break;

          default:
            // any other field we ignore. 
            break;
        }
    }

    poDefn->SetGeomType( eType );

    if (eType != wkbNone)
    {
        ReparseQueryStatement();
    }    

    return poDefn;
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRIngresResultLayer::BuildFullQueryStatement()

{
    osQueryStatement = pszRawStatement;
    OGRIngresSelectStmt oSelectStmt;

    if ( ParseSQLStmt(oSelectStmt, pszRawStatement) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to parse sql: %s",
            pszRawStatement);
        return ;
    }

    /* -------------------------------------------------------------------- */
    /* Other query filter													*/
    /* -------------------------------------------------------------------- */
    if (osWHERE.size())
    {
        if (oSelectStmt.osWhereClause.size())
        {
            osQueryStatement += " AND ";
        }
        else
        {
            osQueryStatement += " WHERE ";
        }

        osQueryStatement += osWHERE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIngresResultLayer::ResetReading()

{
    BuildFullQueryStatement();
    OGRIngresLayer::ResetReading();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRIngresResultLayer::GetFeatureCount( int bForce )

{
    // I wonder if we could do anything smart here...
    // ... not till Ingres grows up (HB)
    OGRIngresSelectStmt oSelectStmt;
    OGRIngresStatement oStmt(poDS->GetTransaction());

    if ( ParseSQLStmt(oSelectStmt, pszRawStatement) != OGRERR_NONE)
    {
        return OGRIngresLayer::GetFeatureCount( bForce );
    }
    
    CPLString osSqlCmd;
    osSqlCmd.Printf("SELECT INT4(COUNT(%s)) FROM %s ",
        CSLCount(oSelectStmt.papszFieldList) == 0 ? "*" : CSLGetField(oSelectStmt.papszFieldList, 0), 
        oSelectStmt.osFromList.c_str());

    if (oSelectStmt.osWhereClause.size())
    {
        osSqlCmd += " WHERE ";
        osSqlCmd += oSelectStmt.osWhereClause;
    }    

    /* -------------------------------------------------------------------- */
    /* Other query filter													*/
    /* -------------------------------------------------------------------- */
    if (osWHERE.size())
    {
        if (oSelectStmt.osWhereClause.size())
        {
            osSqlCmd += " AND ";
        }
        else
        {
            osSqlCmd += " WHERE ";
        }

        osSqlCmd += osWHERE;
    }

    if (m_poFilterGeom)
    {
        OGRIngresLayer::BindQueryGeometry(&oStmt);
    }
    
    CPLDebug("Ingres", osSqlCmd.c_str());

    if (!oStmt.ExecuteSQL(osSqlCmd.c_str()))
    {
        return OGRIngresLayer::GetFeatureCount( bForce );
    }
    
    char **ppRow = oStmt.GetRow();
    int nCount = *(int *)(ppRow[0]);

    return nCount;
}
