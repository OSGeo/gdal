/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of OGROCIStatement, which encapsulates the 
 *           preparation, executation and fetching from an SQL statement.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_oci.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGROCIStatement()                           */
/************************************************************************/

OGROCIStatement::OGROCIStatement( OGROCISession *poSessionIn )

{
    poSession = poSessionIn;
    hStatement = NULL;
    poDefn = NULL;

    nRawColumnCount = 0;
    papszCurColumn = NULL;
    papszCurImage = NULL;
    panCurColumnInd = NULL;
    panFieldMap = NULL;

    pszCommandText = NULL;
}

/************************************************************************/
/*                          ~OGROCIStatement()                          */
/************************************************************************/

OGROCIStatement::~OGROCIStatement()

{
    Clean();
}

/************************************************************************/
/*                               Clean()                                */
/************************************************************************/

void OGROCIStatement::Clean()

{
    int  i;

    CPLFree( pszCommandText );
    pszCommandText = NULL;

    if( papszCurColumn != NULL )
    {
        for( i = 0; papszCurColumn[i] != NULL; i++ )
            CPLFree( papszCurColumn[i] );
    }
    CPLFree( papszCurColumn );
    papszCurColumn = NULL;

    CPLFree( papszCurImage );
    papszCurImage = NULL;
    
    CPLFree( panCurColumnInd );
    panCurColumnInd = NULL;

    CPLFree( panFieldMap );
    panFieldMap = NULL;

    if( poDefn != NULL && poDefn->Dereference() <= 0 )
    {
        delete poDefn;
        poDefn = NULL;
    }

    if( hStatement != NULL )
    {
        OCIHandleFree((dvoid *)hStatement, (ub4)OCI_HTYPE_STMT);
        hStatement = NULL;
    }
}

/************************************************************************/
/*                              Prepare()                               */
/************************************************************************/

CPLErr OGROCIStatement::Prepare( const char *pszSQLStatement )

{
    Clean();

    CPLDebug( "OCI", "Prepare(%s)", pszSQLStatement );

    pszCommandText = CPLStrdup(pszSQLStatement);

    if( hStatement != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Statement already executed once on this OGROCIStatement." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a statement handle.                                    */
/* -------------------------------------------------------------------- */
    if( poSession->Failed( 
        OCIHandleAlloc( poSession->hEnv, (dvoid **) &hStatement, 
                        (ub4)OCI_HTYPE_STMT,(size_t)0, (dvoid **)0 ), 
        "OCIHandleAlloc(Statement)" ) )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Prepare the statement.                                          */
/* -------------------------------------------------------------------- */
    if( poSession->Failed(
        OCIStmtPrepare( hStatement, poSession->hError, 
                        (text *) pszSQLStatement, strlen(pszSQLStatement),
                        (ub4)OCI_NTV_SYNTAX, (ub4)OCI_DEFAULT ),
        "OCIStmtPrepare" ) )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                             BindObject()                             */
/************************************************************************/

CPLErr OGROCIStatement::BindObject( const char *pszPlaceName, 
                                    void *pahObjects, OCIType *hTDO,
                                    void **papIndicators )

{
    OCIBind *hBindOrd = NULL;

    if( poSession->Failed( 
            OCIBindByName( hStatement, &hBindOrd, poSession->hError,
                           (text *) pszPlaceName, (sb4) strlen(pszPlaceName), 
                           (dvoid *) 0, (sb4) 0, SQLT_NTY, (dvoid *)0, 
                           (ub2 *)0, (ub2 *)0, (ub4)0, (ub4 *)0, 
                           (ub4)OCI_DEFAULT),
            "OCIBindByName()") )
        return CE_Failure;
    
    if( poSession->Failed(
            OCIBindObject( hBindOrd, poSession->hError, hTDO,
                           (dvoid **) pahObjects, (ub4 *)0, 
                           (dvoid **)papIndicators, (ub4 *)0),
            "OCIBindObject()" ) )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                             BindScalar()                             */
/************************************************************************/

CPLErr OGROCIStatement::BindScalar( const char *pszPlaceName, 
                                    void *pData, int nDataLen,
                                    int nSQLType, sb2 *paeInd )

{
    OCIBind *hBindOrd = NULL;

    if( poSession->Failed( 
            OCIBindByName( hStatement, &hBindOrd, poSession->hError,
                           (text *) pszPlaceName, (sb4) strlen(pszPlaceName), 
                           (dvoid *) pData, (sb4) nDataLen, 
                           (ub2) nSQLType, (dvoid *)paeInd, (ub2 *)0, 
                           (ub2 *)0, (ub4)0, (ub4 *)0, 
                           (ub4)OCI_DEFAULT),
            "OCIBindByName()") )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                              Execute()                               */
/************************************************************************/

CPLErr OGROCIStatement::Execute( const char *pszSQLStatement,
                                 int nMode )

{
/* -------------------------------------------------------------------- */
/*      Prepare the statement if it is being passed in.                 */
/* -------------------------------------------------------------------- */
    if( pszSQLStatement != NULL )
    {
        CPLErr eErr = Prepare( pszSQLStatement );
        if( eErr != CE_None )
            return eErr;
    }

    if( hStatement == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No prepared statement in call to OGROCIStatement::Execute(NULL)" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Determine if this is a SELECT statement.                        */
/* -------------------------------------------------------------------- */
    ub2  nStmtType;

    if( poSession->Failed( 
        OCIAttrGet( hStatement, OCI_HTYPE_STMT,
                    &nStmtType, 0, OCI_ATTR_STMT_TYPE, poSession->hError ),
        "OCIAttrGet(ATTR_STMT_TYPE)") )
        return CE_Failure;
    
    int bSelect = (nStmtType == OCI_STMT_SELECT);

/* -------------------------------------------------------------------- */
/*      Work out some details about execution mode.                     */
/* -------------------------------------------------------------------- */
    if( nMode == -1 )
    {
        if( bSelect )
            nMode = OCI_DEFAULT;
        else
            nMode = OCI_COMMIT_ON_SUCCESS;
    }

/* -------------------------------------------------------------------- */
/*      Execute the statement.                                          */
/* -------------------------------------------------------------------- */
    if( poSession->Failed( 
        OCIStmtExecute( poSession->hSvcCtx, hStatement, 
                        poSession->hError, (ub4)bSelect ? 0 : 1, (ub4)0, 
                        (OCISnapshot *)NULL, (OCISnapshot *)NULL, nMode ),
        pszCommandText ) )
        return CE_Failure;

    if( !bSelect )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Count the columns.                                              */
/* -------------------------------------------------------------------- */
    for( nRawColumnCount = 0; TRUE; nRawColumnCount++ )
    {                                                           
        OCIParam     *hParmDesc;

        if( OCIParamGet( hStatement, OCI_HTYPE_STMT, poSession->hError, 
                         (dvoid**)&hParmDesc, 
                         (ub4) nRawColumnCount+1 ) != OCI_SUCCESS )
            break;
    }
    
    panFieldMap = (int *) CPLCalloc(sizeof(int),nRawColumnCount);
    
    papszCurColumn = (char **) CPLCalloc(sizeof(char*),nRawColumnCount+1);
    panCurColumnInd = (sb2 *) CPLCalloc(sizeof(sb2),nRawColumnCount+1);
        
/* ==================================================================== */
/*      Establish result column definitions, and setup parameter        */
/*      defines.                                                        */
/* ==================================================================== */
    poDefn = new OGRFeatureDefn( pszCommandText );
    poDefn->Reference();

    for( int iParm = 0; iParm < nRawColumnCount; iParm++ )
    {                                                           
        OGRFieldDefn oField( "", OFTString );
        OCIParam     *hParmDesc;
        ub2          nOCIType;
        ub4          nOCILen;

/* -------------------------------------------------------------------- */
/*      Get parameter definition.                                       */
/* -------------------------------------------------------------------- */
        if( poSession->Failed( 
            OCIParamGet( hStatement, OCI_HTYPE_STMT, poSession->hError, 
                         (dvoid**)&hParmDesc, (ub4) iParm+1 ),
            "OCIParamGet") )
            return CE_Failure;

        if( poSession->GetParmInfo( hParmDesc, &oField, &nOCIType, &nOCILen )
            != CE_None )
            return CE_Failure;

        if( oField.GetType() == OFTBinary )
        {
            panFieldMap[iParm] = -1;
            continue;                   
        }

        poDefn->AddFieldDefn( &oField );
        panFieldMap[iParm] = poDefn->GetFieldCount() - 1;

/* -------------------------------------------------------------------- */
/*      Prepare a binding.                                              */
/* -------------------------------------------------------------------- */
        int nBufWidth = 256, nOGRField = panFieldMap[iParm];
        OCIDefine *hDefn = NULL;

        if( oField.GetWidth() > 0 )
            /* extra space needed for the decimal separator the string 
            terminator and the negative sign (Tamas Szekeres)*/
            nBufWidth = oField.GetWidth() + 3;
        else if( oField.GetType() == OFTInteger )
            nBufWidth = 22;
        else if( oField.GetType() == OFTReal )
            nBufWidth = 36;
        else if ( oField.GetType() == OFTDateTime )
            nBufWidth = 40;
        else if ( oField.GetType() == OFTDate )
            nBufWidth = 20;

        papszCurColumn[nOGRField] = (char *) CPLMalloc(nBufWidth+2);
        CPLAssert( ((long) papszCurColumn[nOGRField]) % 2 == 0 );

        if( poSession->Failed(
            OCIDefineByPos( hStatement, &hDefn, poSession->hError,
                            iParm+1, 
                            (ub1 *) papszCurColumn[nOGRField], nBufWidth,
                            SQLT_STR, panCurColumnInd + nOGRField, 
                            NULL, NULL, OCI_DEFAULT ),
            "OCIDefineByPos" ) )
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                           SimpleFetchRow()                           */
/************************************************************************/

char **OGROCIStatement::SimpleFetchRow()

{
    int nStatus, i;
    
    if( papszCurImage == NULL )
    {
        papszCurImage = (char **) 
            CPLCalloc(sizeof(char *), nRawColumnCount+1 );
    }

    nStatus = OCIStmtFetch( hStatement, poSession->hError, 1, 
                            OCI_FETCH_NEXT, OCI_DEFAULT );

    if( nStatus == OCI_NO_DATA )
        return NULL;
    else if( poSession->Failed( nStatus, "OCIStmtFetch" ) )
        return NULL;

    for( i = 0; papszCurColumn[i] != NULL; i++ )
    {
        if( panCurColumnInd[i] == OCI_IND_NULL )
            papszCurImage[i] = NULL;
        else
            papszCurImage[i] = papszCurColumn[i];
    }

    return papszCurImage;
}

