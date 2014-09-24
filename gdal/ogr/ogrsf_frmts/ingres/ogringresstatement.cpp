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
    memset( &queryInfo, 0, sizeof(queryInfo) );
    bDebug = TRUE;

    bHaveParm = FALSE;
    nParmLen = 0;
    pabyParmData = NULL;

//    CPLDebug( "INGRES", "Create Statement %p", this );
}

/************************************************************************/
/*                        ~OGRIngresStatement()                         */
/************************************************************************/

OGRIngresStatement::~OGRIngresStatement()

{
    Close();
}

void OGRIngresStatement::Close()
{
    IIAPI_WAITPARM  waitParm = { -1 };

    ClearDynamicColumns();

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

    // Set the descriptorCount to zero to avoid attempting to refree it
    // in another Close call.
    getDescrParm.gd_descriptorCount = 0;

    CPLFree( papszFields );
    CPLFree( pabyWrkBuffer );
    CPLFree( pasDataBuffer );
    CPLFree( pabyParmData );

    papszFields = NULL;
    pabyWrkBuffer = NULL;
    pasDataBuffer = NULL;
    pabyParmData = NULL;
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
    queryParm.qy_parameters = bHaveParm;
    queryParm.qy_tranHandle = NULL;
    queryParm.qy_stmtHandle = NULL;

    if( bDebug )
        CPLDebug( "INGRES", "IIapi_query(%s)", pszStatement );

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

    hTransaction = queryParm.qy_tranHandle;
    hStmt = queryParm.qy_stmtHandle;

    if( hStmt == NULL )						
    {
        CPLDebug( "INGRES", "No resulting statement." );
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Do we have parameters to send?                                  */
/* -------------------------------------------------------------------- */
    if( bHaveParm )
    {
        if( !SendParms() )
            return FALSE;
    }

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

    if( getDescrParm.gd_genParm.gp_status != IIAPI_ST_SUCCESS )
    {
        if( getDescrParm.gd_genParm.gp_errorHandle )
        {
            if( !bDebug )
                CPLDebug( "INGRES", "IIapi_query(%s)", pszStatement );

            ReportError( &(getDescrParm.gd_genParm), "IIapi_getDescriptor()" );
            return FALSE;
        }
        else
        {
            if( bDebug )
                CPLDebug( "INGRES", "Got gp_status = %d from getDescriptor.",
                          getDescrParm.gd_genParm.gp_status );
        }
    }

/* -------------------------------------------------------------------- */
/*      Get query info.                                                 */
/* -------------------------------------------------------------------- */
#ifdef notdef
    // For reasons I don't understand, calling getQueryInfo seems to screw
    // up access to query results, so this is disabled.
    queryInfo.gq_stmtHandle = hStmt;

    IIapi_getQueryInfo( &queryInfo );

    while( queryInfo.gq_genParm.gp_completed == FALSE )
        IIapi_wait( &waitParm );

    CPLDebug( "INGRES", 
              "gq_flags=%x, gq_mask=%x, gq_rowCount=%d, gq_rowStatus=%d, rowPosition=%d", 
              queryInfo.gq_flags, 
              queryInfo.gq_mask, 
              queryInfo.gq_rowCount, 
              queryInfo.gq_rowStatus, 
              queryInfo.gq_rowPosition ); 
              
    if( queryInfo.gq_genParm.gp_status != IIAPI_ST_SUCCESS )
    {
        ReportError( &(queryInfo.gq_genParm), "IIapi_getQueryInfo()" );
        return FALSE;
    }
#endif

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
    {
        getColParm.gc_columnData[i].dv_value = papszFields[i];
    }

/* -------------------------------------------------------------------- */
/*      We don't want papszFields[] pointing to anything but            */
/*      dynamically allocated data for long (blob) fields, so clear     */
/*      these pointers now.                                             */
/* -------------------------------------------------------------------- */
    for( i = 0; i < getDescrParm.gd_descriptorCount; i++ )
    {
        if( IsColumnLong( i ) )
            papszFields[i] = NULL;
    }

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
    int                 iBaseCol;

    ClearDynamicColumns();

    if( hStmt == NULL )
        return NULL;

/* ==================================================================== */
/*      Loop over all column, processing the columns in sub-groups      */
/*      so we can isolate blob columns for special handling.            */
/* ==================================================================== */
    for( iBaseCol = 0; iBaseCol < getDescrParm.gd_descriptorCount; iBaseCol++)
    {
        getColParm.gc_columnCount = 1;
        getColParm.gc_columnData = pasDataBuffer + iBaseCol;

/* -------------------------------------------------------------------- */
/*      Fetch column(s)                                                 */
/* -------------------------------------------------------------------- */
        if( !IsColumnLong( iBaseCol ) )
        {
            IIapi_getColumns( &getColParm );
    
            while( getColParm.gc_genParm.gp_completed == FALSE )
                IIapi_wait( &waitParm );

            if( getColParm.gc_genParm.gp_status >= IIAPI_ST_NO_DATA )
                return NULL;

            papszFields[iBaseCol][pasDataBuffer[iBaseCol].dv_length] = '\0';
        }

/* -------------------------------------------------------------------- */
/*      blob columns may require some extra processing.                 */
/* -------------------------------------------------------------------- */
        else
        {
            GUInt16 nSegmentLen;
            char   *pachData = NULL;
            int     nDataLen = 0;

            do {
                IIapi_getColumns( &getColParm );
    
                while( getColParm.gc_genParm.gp_completed == FALSE )
                    IIapi_wait( &waitParm );

                if( getColParm.gc_genParm.gp_status >= IIAPI_ST_NO_DATA )
                    return NULL;

                memcpy( &nSegmentLen, pasDataBuffer[iBaseCol].dv_value, 2 );

                pachData = (char *) CPLRealloc(pachData,
                                               nDataLen + nSegmentLen + 1 );

                memcpy( pachData + nDataLen,
                        ((char *) pasDataBuffer[iBaseCol].dv_value)+2,
                        nSegmentLen );

                nDataLen += nSegmentLen;
                pachData[nDataLen] = '\0';
            } while( getColParm.gc_moreSegments );

            papszFields[iBaseCol] = pachData;
        }
    }

    return papszFields;
}

/************************************************************************/
/*                        ClearDynamicColumns()                         */
/*                                                                      */
/*      Free dynamic buffers associated with long/blob columns.         */
/************************************************************************/

void OGRIngresStatement::ClearDynamicColumns()

{
    int i;

    for( i = 0; i < getDescrParm.gd_descriptorCount; i++ )
    {
        if( IsColumnLong( i ) )
        {
            CPLFree( papszFields[i] );
            papszFields[i] = NULL;
        }
    }
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
/*                            IsColumnLong()                            */
/*                                                                      */
/*      Returns TRUE if the indicated column (zero based) is a blob     */
/*      type (long varchar, long byte, etc)                             */
/************************************************************************/

int OGRIngresStatement::IsColumnLong( int iColumn )

{
    if( iColumn < 0 || iColumn >= getDescrParm.gd_descriptorCount )
        return FALSE;

    switch( getDescrParm.gd_descriptor[iColumn].ds_dataType )
    {
      case IIAPI_LVCH_TYPE:
      case IIAPI_LBYTE_TYPE:
      case IIAPI_LNVCH_TYPE:
      case IIAPI_LTXT_TYPE:
        return TRUE;

      default:
        return FALSE;
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
    if ( ! genParm->gp_errorHandle )
    { 
        CPLDebug( "INGRES", "No gp_errorHandle in ReportError(%s)", 
                  pszDescription );
        return;
    }

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

/************************************************************************/
/*                             SendParms()                              */
/************************************************************************/

int OGRIngresStatement::SendParms()

{
    IIAPI_SETDESCRPARM		setDescrParm;
    IIAPI_PUTPARMPARM		putParmParm;
    IIAPI_DESCRIPTOR    	DescrBuffer;
    IIAPI_DATAVALUE    		DataBuffer;
    IIAPI_WAITPARM	        waitParm = { -1 };

/* -------------------------------------------------------------------- */
/*      Describe the parameter.                                         */
/* -------------------------------------------------------------------- */
    setDescrParm.sd_genParm.gp_callback = NULL;
    setDescrParm.sd_genParm.gp_closure = NULL;
    setDescrParm.sd_stmtHandle = hStmt;
    setDescrParm.sd_descriptorCount = 1;
    setDescrParm.sd_descriptor = ( IIAPI_DESCRIPTOR * )( &DescrBuffer );
 
    setDescrParm.sd_descriptor[0].ds_dataType = eParmType;
    setDescrParm.sd_descriptor[0].ds_nullable = FALSE;
    setDescrParm.sd_descriptor[0].ds_length = (II_UINT2) (nParmLen+2);
    setDescrParm.sd_descriptor[0].ds_precision = 0;
    setDescrParm.sd_descriptor[0].ds_scale = 0;
    setDescrParm.sd_descriptor[0].ds_columnType = IIAPI_COL_QPARM;
    setDescrParm.sd_descriptor[0].ds_columnName = NULL;

    IIapi_setDescriptor( &setDescrParm );
	
    while( setDescrParm.sd_genParm.gp_completed == FALSE )
	IIapi_wait( &waitParm );

    if( setDescrParm.sd_genParm.gp_status != IIAPI_ST_SUCCESS )
    {
        ReportError( &(setDescrParm.sd_genParm), "SendParm()" );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Send the parameter                                              */
/* -------------------------------------------------------------------- */
    GByte  abyChunk[2000];
    int    nBytesSent = 0;

    putParmParm.pp_genParm.gp_callback = NULL;
    putParmParm.pp_genParm.gp_closure = NULL;
    putParmParm.pp_stmtHandle = hStmt;
    putParmParm.pp_parmCount = 1;

    while( nBytesSent < nParmLen )
    {
        GInt16 nLen = (GInt16) MIN((int)sizeof(abyChunk)-2,nParmLen-nBytesSent);

        // presuming we want machine local order...
        memcpy( abyChunk, &nLen, sizeof(nLen) );
        memcpy( abyChunk+2, pabyParmData + nBytesSent, nLen );
        nBytesSent += nLen;

        putParmParm.pp_parmData = &DataBuffer;
        putParmParm.pp_parmData[0].dv_null = FALSE;
        putParmParm.pp_parmData[0].dv_length = nLen+2;
        putParmParm.pp_parmData[0].dv_value = abyChunk;

	putParmParm.pp_moreSegments = nBytesSent < nParmLen;

	IIapi_putParms( &putParmParm );

	while( putParmParm.pp_genParm.gp_completed == FALSE )
	    IIapi_wait( &waitParm );

	if ( putParmParm.pp_genParm.gp_status != IIAPI_ST_SUCCESS )
        {
            ReportError( &(putParmParm.pp_genParm), "SendParm()" );
            
            return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                         addInputParameter()                          */
/*                                                                      */
/*      Add data to be treated as an input parameter for the SQL        */
/*      query.  For now we internally only support one parameter,       */
/*      but we might change that in the future.                         */
/************************************************************************/

void OGRIngresStatement::addInputParameter( 
    IIAPI_DT_ID eDType, int nLength, GByte *pabyData )

{
    CPLAssert( !bHaveParm );
    CPLAssert( eDType == IIAPI_LVCH_TYPE || eDType == IIAPI_LBYTE_TYPE ); 
    // support long varchar and long byte

    bHaveParm = TRUE;
    nParmLen = nLength;
    eParmType = eDType;
    pabyParmData = (GByte *) CPLCalloc(1,nLength+1);
    memcpy( pabyParmData, pabyData, nLength);
}
