/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of OGROCISession, which encapsulates much of the
 *           direct access to OCI.
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

static OCIEnv *ghOracleEnvironment = NULL;

/************************************************************************/
/*                          OGRGetOCISession()                          */
/************************************************************************/

OGROCISession * OGRGetOCISession( const char *pszUserid, 
                                  const char *pszPassword,
                                  const char *pszDatabase )

{
    OGROCISession *poSession;

    poSession = new OGROCISession();
    if( poSession->EstablishSession( pszUserid, pszPassword, pszDatabase ) )
        return poSession;
    else
    {
        delete poSession;
        return NULL;
    }
}

/************************************************************************/
/*                           OGROCISession()                            */
/************************************************************************/

OGROCISession::OGROCISession()

{
    hEnv = NULL;
    hError = NULL;
    hSvcCtx = NULL;
    hDescribe = NULL;
    hGeometryTDO = NULL;
    hOrdinatesTDO = NULL;
}

/************************************************************************/
/*                           ~OGROCISession()                           */
/************************************************************************/

OGROCISession::~OGROCISession()

{
    if( hDescribe != NULL )
        OCIHandleFree((dvoid *)hDescribe, (ub4)OCI_HTYPE_DESCRIBE);

    if( hSvcCtx != NULL )
        OCILogoff( hSvcCtx, hError );
}

/************************************************************************/
/*                          EstablishSession()                          */
/************************************************************************/

int OGROCISession::EstablishSession( const char *pszUserid, 
                                     const char *pszPassword,
                                     const char *pszDatabase )

{
    sword nStatus;

/* -------------------------------------------------------------------- */
/*      Establish the environment.                                      */
/* -------------------------------------------------------------------- */
    if( ghOracleEnvironment == NULL )
    {
        nStatus = 
            OCIEnvCreate( &ghOracleEnvironment, OCI_OBJECT, (dvoid *)0, 
                          0, 0, 0, (size_t) 0, (dvoid **)0 );

        if( nStatus == -1 || ghOracleEnvironment == NULL )
        {
            CPLDebug("OCI", 
                     "OCIEnvCreate() failed with status %d.\n"
                     "Presumably Oracle is not properly installed, skipping.");
                      
            return FALSE;
        }
        
    }

    hEnv = ghOracleEnvironment;

/* -------------------------------------------------------------------- */
/*      Create the error handle.                                        */
/* -------------------------------------------------------------------- */
    nStatus = OCIHandleAlloc( (dvoid *) hEnv, (dvoid **) &hError, 
                              (ub4) OCI_HTYPE_ERROR, (size_t) 0, 
                              (dvoid **) NULL );

/* -------------------------------------------------------------------- */
/*      Attempt Logon.                                                  */
/* -------------------------------------------------------------------- */
    if( Failed( OCILogon( hEnv, hError, &hSvcCtx, 
                          (text *) pszUserid, 
                          strlen(pszUserid),
                          (text *) pszPassword, 
                          strlen(pszPassword),
                          (text *) pszDatabase, 
                          pszDatabase ? strlen(pszDatabase):0 ) ))
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create a describe handle.                                       */
/* -------------------------------------------------------------------- */
    if( Failed( 
        OCIHandleAlloc( hEnv, (dvoid **) &hDescribe, (ub4)OCI_HTYPE_DESCRIBE, 
                        (size_t)0, (dvoid **)0 ), 
        "OCIHandleAlloc(Describe)" ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try to get the MDSYS.SDO_GEOMETRY type object.                  */
/* -------------------------------------------------------------------- */
    hGeometryTDO = PinTDO( SDO_GEOMETRY );
    if( hGeometryTDO == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try to get the MDSYS.SDO_ORDINATE_ARRAY type object.            */
/* -------------------------------------------------------------------- */
    hOrdinatesTDO = PinTDO( "MDSYS.SDO_ORDINATE_ARRAY" );
    if( hOrdinatesTDO == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try to get the MDSYS.SDO_ELEM_INFO_ARRAY type object.           */
/* -------------------------------------------------------------------- */
    hElemInfoTDO = PinTDO( "MDSYS.SDO_ELEM_INFO_ARRAY" );
    if( hElemInfoTDO == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Record information about the session.                           */
/* -------------------------------------------------------------------- */
    this->pszUserid = CPLStrdup(pszUserid);
    this->pszPassword = CPLStrdup(pszPassword);
    this->pszDatabase = CPLStrdup(pszDatabase);

    return TRUE;
}

/************************************************************************/
/*                               Failed()                               */
/************************************************************************/

int OGROCISession::Failed( sword nStatus, const char *pszFunction )

{
    if( pszFunction == NULL )
        pszFunction = "<unnamed>";
    if( nStatus == OCI_ERROR )
    {
        sb4  nErrCode = 0;
        char szErrorMsg[10000];

        szErrorMsg[0] = '\0';
        if( hError != NULL )
        {
            OCIErrorGet( (dvoid *) hError, (ub4) 1, NULL, &nErrCode, 
                         (text *) szErrorMsg, (ub4) sizeof(szErrorMsg), 
                         OCI_HTYPE_ERROR );
        }
        szErrorMsg[sizeof(szErrorMsg)-1] = '\0';

        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s in %s", szErrorMsg, pszFunction );
        return TRUE;
    }
    else if( nStatus == OCI_NEED_DATA )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OCI_NEED_DATA" );
        return TRUE;
    }
    else if( nStatus == OCI_INVALID_HANDLE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OCI_INVALID_HANDLE in %s", pszFunction );
        return TRUE;
    }
    else if( nStatus == OCI_STILL_EXECUTING )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OCI_STILL_EXECUTING in %s", pszFunction );
        return TRUE;
    }
    else if( nStatus == OCI_CONTINUE )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OCI_CONTINUE in %s", pszFunction );
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                            GetParmInfo()                             */
/************************************************************************/

CPLErr 
OGROCISession::GetParmInfo( OCIParam *hParmDesc, OGRFieldDefn *poOGRDefn,
                            ub2 *pnOCIType, ub4 *pnOCILen )

{
    ub2 nOCIType, nOCILen;
    ub4 nColLen;
    char *pszColName;
    char szTermColName[128];
    
/* -------------------------------------------------------------------- */
/*      Get basic parameter details.                                    */
/* -------------------------------------------------------------------- */
    if( Failed( 
        OCIAttrGet( hParmDesc, OCI_DTYPE_PARAM, 
                    (dvoid **)&nOCIType, 0, OCI_ATTR_DATA_TYPE, hError ),
        "OCIAttrGet(Type)" ) )
        return CE_Failure;

    if( Failed( 
        OCIAttrGet( hParmDesc, OCI_DTYPE_PARAM, 
                    (dvoid **)&nOCILen, 0, OCI_ATTR_DATA_SIZE, hError ),
        "OCIAttrGet(Size)" ) )
        return CE_Failure;

    if( Failed(
        OCIAttrGet( hParmDesc, OCI_DTYPE_PARAM, (dvoid **)&pszColName,
                    &nColLen, OCI_ATTR_NAME, hError ), 
        "OCIAttrGet(Name)") )
        return CE_Failure;
    
    if( nColLen >= sizeof(szTermColName) )                              
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Column length (%d) longer than column name buffer (%d) in\n"
                  "OGROCISession::GetParmInfo()", 
                  nColLen, sizeof(szTermColName) );
        return CE_Failure;
    }

    strncpy( szTermColName, pszColName, nColLen );
    szTermColName[nColLen] = '\0';
    
    poOGRDefn->SetName( szTermColName );

/* -------------------------------------------------------------------- */
/*      Attempt to classify as an OGRType.                              */
/* -------------------------------------------------------------------- */
    switch( nOCIType )
    {
        case SQLT_CHR:
        case SQLT_AFC: /* CHAR(), NCHAR() */
            poOGRDefn->SetType( OFTString );
            if( nOCILen < 2048 )
                poOGRDefn->SetWidth( nOCILen );
            break;

        case SQLT_NUM:
        {
            // NOTE: OCI docs say this should be ub1 type, but we have
            // determined that oracle is actually returning a short so we
            // use that type and try to compensate for possible problems by
            // initializing, and dividing by 256 if it is large. 
            unsigned short byPrecision = 0;
            sb1  nScale;

            if( Failed(
                OCIAttrGet( hParmDesc, OCI_DTYPE_PARAM, (dvoid **)&byPrecision,
                            0, OCI_ATTR_PRECISION, hError ), 
                "OCIAttrGet(Precision)" ) )
                return CE_Failure;
            if( Failed(
                OCIAttrGet( hParmDesc, OCI_DTYPE_PARAM, (dvoid **)&nScale,
                            0, OCI_ATTR_SCALE, hError ), 
                "OCIAttrGet(Scale)") )
                return CE_Failure;
#ifdef notdef
            CPLDebug( "OCI", "%s: Scale=%d, Precision=%d", 
                      szTermColName, nScale, byPrecision );
#endif
            if( byPrecision > 255 )
                byPrecision = byPrecision / 256;

            if( nScale < 0 )
                poOGRDefn->SetType( OFTReal );
            else if( nScale > 0 )
            {
                poOGRDefn->SetType( OFTReal );
                poOGRDefn->SetWidth( byPrecision );
                poOGRDefn->SetPrecision( nScale );
            }
            else if( byPrecision < 38 )
            {
                poOGRDefn->SetType( OFTInteger );
                poOGRDefn->SetWidth( byPrecision );
            }
            else
            {
                poOGRDefn->SetType( OFTInteger );
            }
        }
        break;

        case SQLT_DATE:
            poOGRDefn->SetType( OFTString );
            poOGRDefn->SetWidth( 24 );
            break;

        case SQLT_RID:
        case SQLT_BIN:
        case SQLT_LBI:
        case 111: /* REF */
        case SQLT_CLOB:
        case SQLT_BLOB:
        case SQLT_FILE:
        case 208: /* UROWID */
            poOGRDefn->SetType( OFTBinary );
            break;

        default:
            poOGRDefn->SetType( OFTBinary );
            break;
    }

    if( pnOCIType != NULL )
        *pnOCIType = nOCIType;

    if( pnOCILen != NULL )
        *pnOCILen = nOCILen;

    return CE_None;
}

/************************************************************************/
/*                             CleanName()                              */
/*                                                                      */
/*      Modify a name in-place to be a well formed Oracle name.         */
/************************************************************************/

void OGROCISession::CleanName( char * pszName )

{
    int   i;

    if( strlen(pszName) > 30 )
        pszName[30] = '\0';

    for( i = 0; pszName[i] != '\0'; i++ )
    {
        pszName[i] = toupper(pszName[i]);
        
        if( (pszName[i] < '0' || pszName[i] > '9')
            && (pszName[i] < 'A' || pszName[i] > 'Z')
            && pszName[i] != '_' )
            pszName[i] = '_';
    }
}

/************************************************************************/
/*                               PinTDO()                               */
/*                                                                      */
/*      Fetch a Type Description Object for the named type.             */
/************************************************************************/

OCIType *OGROCISession::PinTDO( const char *pszType )

{
    OCIParam *hGeomParam = NULL;
    OCIRef *hGeomTypeRef = NULL;
    OCIType *hPinnedTDO = NULL;

    if( Failed( 
        OCIDescribeAny(hSvcCtx, hError, 
                       (text *) pszType, (ub4) strlen(pszType), 
                       OCI_OTYPE_NAME, (ub1)1, (ub1)OCI_PTYPE_TYPE, 
                       hDescribe ), 
        "GetTDO()->OCIDescribeAny()" ) )
        return NULL;

    if( Failed( 
        OCIAttrGet((dvoid *)hDescribe, (ub4)OCI_HTYPE_DESCRIBE,
                   (dvoid *)&hGeomParam, (ub4 *)0, (ub4)OCI_ATTR_PARAM, 
                   hError), "GetTDO()->OCIGetAttr(ATTR_PARAM)") )
        return NULL;

    if( Failed( 
        OCIAttrGet((dvoid *)hGeomParam, (ub4)OCI_DTYPE_PARAM,
                   (dvoid *)&hGeomTypeRef, (ub4 *)0, (ub4)OCI_ATTR_REF_TDO, 
                   hError), "GetTDO()->OCIAttrGet(ATTR_REF_TDO)" ) )
        return NULL;

    if( Failed( 
        OCIObjectPin(hEnv, hError, hGeomTypeRef, (OCIComplexObject *)0, 
                     OCI_PIN_ANY, OCI_DURATION_SESSION, 
                     OCI_LOCK_NONE, (dvoid **)&hPinnedTDO ),
        "GetTDO()->OCIObjectPin()" ) )
        return NULL;

    return hPinnedTDO;
}
