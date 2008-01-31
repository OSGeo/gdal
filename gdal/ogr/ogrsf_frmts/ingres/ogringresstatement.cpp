/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIngresStatement class.
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

#include "ogr_ingres.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRIngresStatement()                         */
/************************************************************************/

OGRIngresStatement::OGRIngresStatement( II_PTR hConn )

{
    this->hConn = hConn;

    pabyWrkBuffer = NULL;
    papszFields = NULL;
    pasDataBuffer = NULL;
    hStmt = NULL;
    hTransaction = NULL;

    memset( &getDescrParm, 0, sizeof(getDescrParm) );
}

/************************************************************************/
/*                        ~OGRIngresStatement()                         */
/************************************************************************/

OGRIngresStatement::~OGRIngresStatement()

{
    IIAPI_WAITPARM	waitParm = { -1 };

    if( hStmt != NULL )
    {
        IIAPI_CLOSEPARM closeParm;

        closeParm.cl_genParm.gp_callback = NULL;
        closeParm.cl_genParm.gp_closure = NULL;
        closeParm.cl_stmtHandle = hStmt;
        
        IIapi_close( &closeParm );

        while( closeParm.cl_genParm.gp_completed == FALSE )
            IIapi_wait( &waitParm );

        hStmt = NULL;
    }

    if( hTransaction != NULL )
    {
        IIAPI_COMMITPARM commitParm;

        commitParm.cm_genParm.gp_callback = NULL;
        commitParm.cm_genParm.gp_closure = NULL;
        commitParm.cm_tranHandle = hTransaction;

        IIapi_commit( &commitParm );

        while( commitParm.cm_genParm.gp_completed == FALSE )
            IIapi_wait( &waitParm );

        hTransaction = NULL;
    }

    CPLFree( papszFields );
    CPLFree( pabyWrkBuffer );
    CPLFree( pasDataBuffer );
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/*                                                                      */
/*      Execute an SQL statement.                                       */
/************************************************************************/

int OGRIngresStatement::ExecuteSQL( const char *pszStatement )

{
/* -------------------------------------------------------------------- */
/*      Issue the query.                                                */
/* -------------------------------------------------------------------- */
    IIAPI_WAITPARM	waitParm = { -1 };
    IIAPI_QUERYPARM	queryParm;

    queryParm.qy_genParm.gp_callback = NULL;
    queryParm.qy_genParm.gp_closure = NULL;
    queryParm.qy_connHandle = hConn;
    queryParm.qy_queryType = IIAPI_QT_QUERY;
    queryParm.qy_queryText = (II_CHAR *) pszStatement;
    queryParm.qy_parameters = FALSE;
    queryParm.qy_tranHandle = NULL;
    queryParm.qy_stmtHandle = NULL;

    IIapi_query( &queryParm );
  
/* -------------------------------------------------------------------- */
/*      Capture handles for result.                                     */
/* -------------------------------------------------------------------- */
    while( queryParm.qy_genParm.gp_completed == FALSE )
	IIapi_wait( &waitParm );

    if( queryParm.qy_genParm.gp_status != IIAPI_ST_SUCCESS 
        || hConn == NULL )
    {
        ReportError( &(queryParm.qy_genParm), 
                     CPLString().Printf( "IIapi_query(%s)", pszStatement ) );
        return FALSE;
    }

    if( queryParm.qy_stmtHandle == NULL )
        return FALSE;

    hTransaction = queryParm.qy_tranHandle;
    hStmt = queryParm.qy_stmtHandle;

/* -------------------------------------------------------------------- */
/*      Get description of result columns.                              */
/* -------------------------------------------------------------------- */
    getDescrParm.gd_genParm.gp_callback = NULL;
    getDescrParm.gd_genParm.gp_closure = NULL;
    getDescrParm.gd_stmtHandle = hStmt;
    getDescrParm.gd_descriptorCount = 0;
    getDescrParm.gd_descriptor = NULL;

    IIapi_getDescriptor( &getDescrParm );
    
    while( getDescrParm.gd_genParm.gp_completed == FALSE )
	IIapi_wait( &waitParm );

/* -------------------------------------------------------------------- */
/*      Setup buffers for returned rows.                                */
/* -------------------------------------------------------------------- */
    int  i, nBufWidth = 0;

    for( i = 0; i < getDescrParm.gd_descriptorCount; i++ )
        nBufWidth += getDescrParm.gd_descriptor[i].ds_length + 1;

    pabyWrkBuffer = (GByte *) CPLCalloc(1,nBufWidth);

    papszFields = (char **) CPLCalloc(sizeof(char *), 
                                     getDescrParm.gd_descriptorCount+1);

    nBufWidth = 0;
    for( i = 0; i < getDescrParm.gd_descriptorCount; i++ )
    {
        papszFields[i] = (char *) (pabyWrkBuffer + nBufWidth); 
        nBufWidth += getDescrParm.gd_descriptor[i].ds_length + 1;
    }

/* -------------------------------------------------------------------- */
/*      Setup the getColumns() argument.                                */
/* -------------------------------------------------------------------- */
    pasDataBuffer = (IIAPI_DATAVALUE *) 
        CPLCalloc(sizeof(IIAPI_DATAVALUE),getDescrParm.gd_descriptorCount);

    getColParm.gc_genParm.gp_callback = NULL;
    getColParm.gc_genParm.gp_closure = NULL;
    getColParm.gc_rowCount = 1;
    getColParm.gc_columnCount = getDescrParm.gd_descriptorCount;
    getColParm.gc_rowsReturned = 0;
    getColParm.gc_columnData =  pasDataBuffer;
    getColParm.gc_stmtHandle = hStmt;
    getColParm.gc_moreSegments = 0;

    for( i = 0; i < getDescrParm.gd_descriptorCount; i++ )
        getColParm.gc_columnData[i].dv_value = papszFields[i];

    return TRUE;
}

/************************************************************************/
/*                               GetRow()                               */
/*                                                                      */
/*      Get a row from the result set and return an array of            */
/*      pointers to the returned fields.  NULL is returned on error     */
/*      or if we have run out of rows.                                  */
/************************************************************************/

char **OGRIngresStatement::GetRow()

{
    IIAPI_WAITPARM	waitParm = { -1 };
    int                 i;

    IIapi_getColumns( &getColParm );
    
    while( getColParm.gc_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );
    
    if ( getColParm.gc_genParm.gp_status >= IIAPI_ST_NO_DATA )
        return NULL;

    for( i = 0; i < getDescrParm.gd_descriptorCount; i++ )
        papszFields[i][pasDataBuffer[i].dv_length] = '\0';

    return papszFields;
}

/************************************************************************/
/*                              DumpRow()                               */
/************************************************************************/

void OGRIngresStatement::DumpRow( FILE *fp )

{									
    int   i;

    fprintf( fp, "---------------\n" );
    for( i = 0; i < getDescrParm.gd_descriptorCount; i++ )
    {
        fprintf( fp, "  %s = %s\n", 
                 getDescrParm.gd_descriptor[i].ds_columnName, 
                 papszFields[i] );
    }                 
}

/************************************************************************/
/*                            ReportError()                             */
/************************************************************************/

void OGRIngresStatement::ReportError( IIAPI_GENPARM *genParm,
                                      const char *pszDescription )

{
    IIAPI_GETEINFOPARM  getErrParm; 

    /*
    ** Check API call status.
    */
    const char *pszCode = 
        (genParm->gp_status == IIAPI_ST_SUCCESS) ?  
        "IIAPI_ST_SUCCESS" :
        (genParm->gp_status == IIAPI_ST_MESSAGE) ?  
        "IIAPI_ST_MESSAGE" :
        (genParm->gp_status == IIAPI_ST_WARNING) ?  
        "IIAPI_ST_WARNING" :
        (genParm->gp_status == IIAPI_ST_NO_DATA) ?  
        "IIAPI_ST_NO_DATA" :
        (genParm->gp_status == IIAPI_ST_ERROR)   ?  
        "IIAPI_ST_ERROR"   :
        (genParm->gp_status == IIAPI_ST_FAILURE) ? 
        "IIAPI_ST_FAILURE" :
        (genParm->gp_status == IIAPI_ST_NOT_INITIALIZED) ?
        "IIAPI_ST_NOT_INITIALIZED" :
        (genParm->gp_status == IIAPI_ST_INVALID_HANDLE) ?
        "IIAPI_ST_INVALID_HANDLE"  :
        (genParm->gp_status == IIAPI_ST_OUT_OF_MEMORY) ?
        "IIAPI_ST_OUT_OF_MEMORY"   :
        "(unknown status)";

    /*
    ** Check for error information.
    */
    if ( ! genParm->gp_errorHandle ) return;
    getErrParm.ge_errorHandle = genParm->gp_errorHandle;
    
    CPLString osErrorMessage;
    CPLErr eType = CE_Failure;
        
    osErrorMessage.Printf( "%s: %s", pszDescription, pszCode );

    do
    { 
	/*
	** Invoke API function call.
 	*/
    	IIapi_getErrorInfo( &getErrParm );

 	/*
	** Break out of the loop if no data or failed.
	*/
    	if ( getErrParm.ge_status != IIAPI_ST_SUCCESS )
	    break;

	/*
	** Process result.
	*/

	switch( getErrParm.ge_type )
	{
           case IIAPI_GE_ERROR		: 
            eType = CE_Failure; 
            break;

          case IIAPI_GE_WARNING	:
            eType = CE_Warning;
            break;

          case IIAPI_GE_MESSAGE	:
            eType = CE_Debug;
            break;

          default:
            eType = CE_Failure;
            break;
	}

        CPLString osMoreMsg;

        osMoreMsg.Printf( "\n'%s' 0x%x\n%s",
                  getErrParm.ge_SQLSTATE, getErrParm.ge_errorCode,
                  getErrParm.ge_message ? getErrParm.ge_message : "NULL" );
        osErrorMessage += osMoreMsg;
    } while( 1 );

    CPLError( eType, CPLE_AppDefined, "%s", osErrorMessage.c_str() );
}

