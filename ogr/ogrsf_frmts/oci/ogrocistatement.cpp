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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2002/12/28 04:38:36  warmerda
 * converted to unix file conventions
 *
 * Revision 1.1  2002/12/28 04:07:27  warmerda
 * New
 *
 */

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
    panFieldMap = NULL;
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
/*                              Execute()                               */
/************************************************************************/

CPLErr OGROCIStatement::Execute( const char *pszSQLStatement,
                                 int nMode )

{
    Clean();

    CPLDebug( "OCI", "Execute(%s)", pszSQLStatement );

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

/* -------------------------------------------------------------------- */
/*      Work out some details about execution mode.                     */
/* -------------------------------------------------------------------- */
    int  bSelect = EQUALN(pszSQLStatement,"SELECT",5);
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
                        (OCISnapshot *)NULL, (OCISnapshot *)NULL, 
                        (ub4) (bSelect ? OCI_DEFAULT : OCI_COMMIT_ON_SUCCESS)),
        "OCIStmtExecute" ) )
        return CE_Failure;

    if( !bSelect )
        return CE_None;

/* ==================================================================== */
/*      Establish result column definitions, and setup parameter        */
/*      defines.                                                        */
/* ==================================================================== */
    poDefn = new OGRFeatureDefn( pszSQLStatement );
    poDefn->Reference();

    for( int iParm = 0; TRUE; iParm++ )
    {								
        OGRFieldDefn oField( "", OFTString );
        int          nStatus;
        OCIParam     *hParmDesc;
        ub2          nOCIType;
        ub4          nOCILen;

        panFieldMap = (int *) CPLRealloc(panFieldMap, sizeof(int)*(iParm+1));

/* -------------------------------------------------------------------- */
/*      Get parameter definition.                                       */
/* -------------------------------------------------------------------- */
        nStatus = 
            OCIParamGet( hStatement, OCI_HTYPE_STMT, poSession->hError, 
                         (dvoid**)&hParmDesc, (ub4) iParm+1 );

        if( nStatus == OCI_ERROR )
            break;

        nRawColumnCount++;
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
            nBufWidth = oField.GetWidth();
        
        papszCurColumn = (char **) 
            CPLRealloc(papszCurColumn, sizeof(char*)*(nOGRField+2));
        papszCurColumn[nOGRField+1] = NULL;

        papszCurColumn[nOGRField] = (char *) CPLCalloc(1,nBufWidth+1);

        panCurColumnInd = (sb2 *) 
            CPLRealloc(panCurColumnInd,sizeof(sb2) * (nOGRField+1));
        
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

