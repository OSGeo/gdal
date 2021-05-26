/******************************************************************************
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

CPL_CVSID("$Id$")

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
        return nullptr;
    }
}

/************************************************************************/
/*                           OGROCISession()                            */
/************************************************************************/

OGROCISession::OGROCISession()

{
    hEnv = nullptr;
    hError = nullptr;
    hSvcCtx = nullptr;
    hServer = nullptr;
    hSession = nullptr;
    hDescribe = nullptr;
    hGeometryTDO = nullptr;
    hOrdinatesTDO = nullptr;
    hElemInfoTDO = nullptr;
    pszUserid = nullptr;
    pszPassword = nullptr;
    pszDatabase = nullptr;
    nServerVersion = 10;
    nServerRelease = 1;
    nMaxNameLength = 30;
}

/************************************************************************/
/*                           ~OGROCISession()                           */
/************************************************************************/

OGROCISession::~OGROCISession()

{
    if( hDescribe != nullptr )
        OCIHandleFree((dvoid *)hDescribe, (ub4)OCI_HTYPE_DESCRIBE);

    if( hSvcCtx != nullptr )
    {
        OCISessionEnd(hSvcCtx, hError, hSession, (ub4) 0);

        if( hSvcCtx && hError)
            OCIServerDetach(hServer, hError, (ub4) OCI_DEFAULT);

        if( hServer )
            OCIHandleFree((dvoid *) hServer, (ub4) OCI_HTYPE_SERVER);

        if( hSvcCtx )
            OCIHandleFree((dvoid *) hSvcCtx, (ub4) OCI_HTYPE_SVCCTX);

        if( hError )
            OCIHandleFree((dvoid *) hError, (ub4) OCI_HTYPE_ERROR);

        if( hSession )
            OCIHandleFree((dvoid *) hSession, (ub4) OCI_HTYPE_SESSION);

        if( hEnv )
            OCIHandleFree((dvoid *) hEnv, (ub4) OCI_HTYPE_ENV);
    }

    CPLFree( pszUserid );
    CPLFree( pszPassword );
    CPLFree( pszDatabase );
}

/************************************************************************/
/*                          EstablishSession()                          */
/************************************************************************/

int OGROCISession::EstablishSession( const char *pszUseridIn,
                                     const char *pszPasswordIn,
                                     const char *pszDatabaseIn )

{
/* -------------------------------------------------------------------- */
/*      Operational Systems's authentication option                     */
/* -------------------------------------------------------------------- */

    //Setting to "/" for backward compatibility
    const char* pszUser = "/";

    ub4 eCred = OCI_CRED_RDBMS;

    if( EQUAL(pszPasswordIn, "") &&
        EQUAL(pszUseridIn, "") )
    {
        //user and password are ignored in this credential type. 
        eCred = OCI_CRED_EXT;
    }
    else
    {
        pszUser = pszUseridIn;
    }

/* -------------------------------------------------------------------- */
/*      Initialize Environment handler                                  */
/* -------------------------------------------------------------------- */

    if( Failed( OCIEnvCreate( (OCIEnv **) &hEnv, OCI_THREADED | OCI_OBJECT,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              0,
                              nullptr ) ) )
    {
        return FALSE;
    }

    if( Failed( OCIHandleAlloc( (dvoid *) hEnv, (dvoid **) &hError,
                OCI_HTYPE_ERROR, (size_t) 0, (dvoid **) nullptr) ) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Initialize Server Context                                       */
/* -------------------------------------------------------------------- */

    if( Failed( OCIHandleAlloc( (dvoid *) hEnv, (dvoid **) &hServer,
                OCI_HTYPE_SERVER, (size_t) 0, (dvoid **) nullptr) ) )
    {
        return FALSE;
    }

    if( Failed( OCIHandleAlloc( (dvoid *) hEnv, (dvoid **) &hSvcCtx,
                OCI_HTYPE_SVCCTX, (size_t) 0, (dvoid **) nullptr) ) )
    {
        return FALSE;
    }

    if( Failed( OCIServerAttach( hServer, hError, (text*) pszDatabaseIn,
                static_cast<int>(strlen((char*) pszDatabaseIn)), 0) ) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Initialize Service Context                                      */
/* -------------------------------------------------------------------- */

    if( Failed( OCIAttrSet( (dvoid *) hSvcCtx, OCI_HTYPE_SVCCTX, (dvoid *)hServer,
                (ub4) 0, OCI_ATTR_SERVER, (OCIError *) hError) ) )
    {
        return FALSE;
    }

    if( Failed( OCIHandleAlloc((dvoid *) hEnv, (dvoid **)&hSession,
                (ub4) OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) nullptr) ) )
    {
        return FALSE;
    }

    if( Failed( OCIAttrSet((dvoid *) hSession, (ub4) OCI_HTYPE_SESSION,
                (dvoid *) pszUser, (ub4) strlen((char *) pszUser),
                (ub4) OCI_ATTR_USERNAME, hError) ) )
    {
        return FALSE;
    }

    if( Failed( OCIAttrSet((dvoid *) hSession, (ub4) OCI_HTYPE_SESSION,
                (dvoid *) pszPasswordIn, (ub4) strlen((char *) pszPasswordIn),
                (ub4) OCI_ATTR_PASSWORD, hError) ) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Initialize Session                                              */
/* -------------------------------------------------------------------- */

    if( Failed( OCISessionBegin(hSvcCtx, hError, hSession, eCred,
                (ub4) OCI_DEFAULT) ) )
    {
        CPLDebug("OCI", "OCISessionBegin() failed to initialize session");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Initialize Service                                              */
/* -------------------------------------------------------------------- */

    if( Failed( OCIAttrSet((dvoid *) hSvcCtx, (ub4) OCI_HTYPE_SVCCTX,
                (dvoid *) hSession, (ub4) 0,
                (ub4) OCI_ATTR_SESSION, hError) ) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create a describe handle.                                       */
/* -------------------------------------------------------------------- */

    if( Failed(
        OCIHandleAlloc( hEnv, (dvoid **) &hDescribe, (ub4)OCI_HTYPE_DESCRIBE,
                        (size_t)0, (dvoid **)nullptr ),
        "OCIHandleAlloc(Describe)" ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Try to get the MDSYS.SDO_GEOMETRY type object.                  */
/* -------------------------------------------------------------------- */
    /* If we have no MDSYS.SDO_GEOMETRY then we consider we are
        working along with the VRT driver and access non spatial tables.
        See #2202 for more details (Tamas Szekeres)*/
    if (OCIDescribeAny(hSvcCtx, hError,
                       (text *) SDO_GEOMETRY, (ub4) strlen(SDO_GEOMETRY),
                       OCI_OTYPE_NAME, (ub1) OCI_DEFAULT, (ub1)OCI_PTYPE_TYPE,
                       hDescribe ) != OCI_ERROR)
    {
        hGeometryTDO = PinTDO( SDO_GEOMETRY );
        if( hGeometryTDO == nullptr )
            return FALSE;

/* -------------------------------------------------------------------- */
/*      Try to get the MDSYS.SDO_ORDINATE_ARRAY type object.            */
/* -------------------------------------------------------------------- */
        hOrdinatesTDO = PinTDO( "MDSYS.SDO_ORDINATE_ARRAY" );
        if( hOrdinatesTDO == nullptr )
            return FALSE;

/* -------------------------------------------------------------------- */
/*      Try to get the MDSYS.SDO_ELEM_INFO_ARRAY type object.           */
/* -------------------------------------------------------------------- */
        hElemInfoTDO = PinTDO( "MDSYS.SDO_ELEM_INFO_ARRAY" );
        if( hElemInfoTDO == nullptr )
            return FALSE;
    }
/* -------------------------------------------------------------------- */
/*      Record information about the session.                           */
/* -------------------------------------------------------------------- */
    pszUserid = CPLStrdup(pszUseridIn);
    pszPassword = CPLStrdup(pszPasswordIn);
    pszDatabase = CPLStrdup(pszDatabaseIn);

/* -------------------------------------------------------------------- */
/*      Get server version information                                  */
/* -------------------------------------------------------------------- */

    char szVersionTxt[256];

    OCIServerVersion( hSvcCtx, hError, (text*) szVersionTxt, 
                    (ub4) sizeof(szVersionTxt), (ub1) OCI_HTYPE_SVCCTX );

    char** papszNameValue = CSLTokenizeString2( szVersionTxt, " .", 
                                                CSLT_STRIPLEADSPACES );

    int count = CSLCount( papszNameValue);

    for( int i = 0; i < count; i++)
    {
        if( EQUAL(papszNameValue[i], "Release") )
        {
            if( i + 1 < count )
            {
                nServerVersion = atoi(papszNameValue[i + 1]);
            }
            if( i + 2 < count )
            {
                nServerRelease = atoi(papszNameValue[i + 2]);
            }
            break;
        }
    }

    CPLDebug("OCI", "From '%s' :", szVersionTxt);
    CPLDebug("OCI", "Version:%d", nServerVersion);
    CPLDebug("OCI", "Release:%d", nServerRelease);

/* -------------------------------------------------------------------- */
/*      Set maximum name length (before 12.2 ? 30 : 128)                */
/* -------------------------------------------------------------------- */

    if( nServerVersion > 12 || (nServerVersion == 12 && nServerRelease >= 2) )
    {
        nMaxNameLength = 128;
    }

    CSLDestroy( papszNameValue );

/* -------------------------------------------------------------------- */
/*      Setting up the OGR compatible time formatting rules.            */
/* -------------------------------------------------------------------- */
    OGROCIStatement oSetNLSTimeFormat( this );
    if( oSetNLSTimeFormat.Execute( "ALTER SESSION SET NLS_DATE_FORMAT='YYYY/MM/DD' \
        NLS_TIME_FORMAT='HH24:MI:SS' NLS_TIME_TZ_FORMAT='HH24:MI:SS TZHTZM' \
        NLS_TIMESTAMP_FORMAT='YYYY/MM/DD HH24:MI:SS' \
        NLS_TIMESTAMP_TZ_FORMAT='YYYY/MM/DD HH24:MI:SS TZHTZM' \
        NLS_NUMERIC_CHARACTERS = '. '" ) != CE_None )
        return OGRERR_FAILURE;

    return TRUE;
}

/************************************************************************/
/*                               Failed()                               */
/************************************************************************/

int OGROCISession::Failed( sword nStatus, const char *pszFunction )

{
    if( pszFunction == nullptr )
        pszFunction = "<unnamed>";
    if( nStatus == OCI_ERROR )
    {
        sb4  nErrCode = 0;
        char szErrorMsg[10000];

        szErrorMsg[0] = '\0';
        if( hError != nullptr )
        {
            OCIErrorGet( (dvoid *) hError, (ub4) 1, nullptr, &nErrCode,
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
/*                            GetParamInfo()                             */
/************************************************************************/

CPLErr
OGROCISession::GetParamInfo( OCIParam *hParamDesc, OGRFieldDefn *poOGRDefn,
                            ub2 *pnOCIType, ub4 *pnOCILen )

{
    ub2 nOCIType, nOCILen;
    ub4 nColLen;
    ub1 bOCINull;
    char *pszColName;
    char szTermColName[128];

/* -------------------------------------------------------------------- */
/*      Get basic parameter details.                                    */
/* -------------------------------------------------------------------- */
    if( Failed(
        OCIAttrGet( hParamDesc, OCI_DTYPE_PARAM,
                    &nOCIType, nullptr, OCI_ATTR_DATA_TYPE, hError ),
        "OCIAttrGet(Type)" ) )
        return CE_Failure;

    if( Failed(
        OCIAttrGet( hParamDesc, OCI_DTYPE_PARAM,
                    &nOCILen, nullptr, OCI_ATTR_DATA_SIZE, hError ),
        "OCIAttrGet(Size)" ) )
        return CE_Failure;

    if( Failed(
        OCIAttrGet( hParamDesc, OCI_DTYPE_PARAM, &pszColName,
                    &nColLen, OCI_ATTR_NAME, hError ),
        "OCIAttrGet(Name)") )
        return CE_Failure;

    if( Failed(
        OCIAttrGet( hParamDesc, OCI_DTYPE_PARAM, &bOCINull,
                    nullptr, OCI_ATTR_IS_NULL, hError ),
        "OCIAttrGet(Null)") )
        return CE_Failure;

    if( nColLen >= sizeof(szTermColName) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Column length (%d) longer than column name buffer (%d) in\n"
                  "OGROCISession::GetParamInfo()",
                  nColLen, (int) sizeof(szTermColName) );
        return CE_Failure;
    }

    strncpy( szTermColName, pszColName, nColLen );
    szTermColName[nColLen] = '\0';

    poOGRDefn->SetName( szTermColName );
    poOGRDefn->SetNullable( bOCINull );

/* -------------------------------------------------------------------- */
/*      Attempt to classify as an OGRType.                              */
/* -------------------------------------------------------------------- */
    switch( nOCIType )
    {
        case SQLT_CHR:
        case SQLT_AFC: /* CHAR(), NCHAR() */
            poOGRDefn->SetType( OFTString );
            if( nOCILen <= 4000 )
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
                OCIAttrGet( hParamDesc, OCI_DTYPE_PARAM, &byPrecision,
                            nullptr, OCI_ATTR_PRECISION, hError ),
                "OCIAttrGet(Precision)" ) )
                return CE_Failure;
            if( Failed(
                OCIAttrGet( hParamDesc, OCI_DTYPE_PARAM, &nScale,
                            nullptr, OCI_ATTR_SCALE, hError ),
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
                poOGRDefn->SetType( (byPrecision < 10) ? OFTInteger : OFTInteger64 );
                poOGRDefn->SetWidth( byPrecision );
            }
            else
            {
                poOGRDefn->SetType( OFTInteger64 );
            }
        }
        break;

        case SQLT_DAT:
        case SQLT_DATE:
            poOGRDefn->SetType( OFTDate );
            break;
        case SQLT_TIMESTAMP:
        case SQLT_TIMESTAMP_TZ:
        case SQLT_TIMESTAMP_LTZ:
        case SQLT_TIME:
        case SQLT_TIME_TZ:
            poOGRDefn->SetType( OFTDateTime );
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

    if( pnOCIType != nullptr )
        *pnOCIType = nOCIType;

    if( pnOCILen != nullptr )
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

    if( strlen(pszName) > nMaxNameLength )
        pszName[nMaxNameLength] = '\0';

    for( i = 0; pszName[i] != '\0'; i++ )
    {
        pszName[i] = static_cast<char>(toupper(pszName[i]));

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
    OCIParam *hGeomParam = nullptr;
    OCIRef *hGeomTypeRef = nullptr;
    OCIType *hPinnedTDO = nullptr;

    if( Failed(
        OCIDescribeAny(hSvcCtx, hError,
                       (text *) pszType, (ub4) strlen(pszType),
                       OCI_OTYPE_NAME, (ub1)1, (ub1)OCI_PTYPE_TYPE,
                       hDescribe ),
        "GetTDO()->OCIDescribeAny()" ) )
        return nullptr;

    if( Failed(
        OCIAttrGet((dvoid *)hDescribe, (ub4)OCI_HTYPE_DESCRIBE,
                   (dvoid *)&hGeomParam, (ub4 *)nullptr, (ub4)OCI_ATTR_PARAM,
                   hError), "GetTDO()->OCIGetAttr(ATTR_PARAM)") )
        return nullptr;

    if( Failed(
        OCIAttrGet((dvoid *)hGeomParam, (ub4)OCI_DTYPE_PARAM,
                   (dvoid *)&hGeomTypeRef, (ub4 *)nullptr, (ub4)OCI_ATTR_REF_TDO,
                   hError), "GetTDO()->OCIAttrGet(ATTR_REF_TDO)" ) )
        return nullptr;

    if( Failed(
        OCIObjectPin(hEnv, hError, hGeomTypeRef, (OCIComplexObject *)nullptr,
                     OCI_PIN_ANY, OCI_DURATION_SESSION,
                     OCI_LOCK_NONE, (dvoid **)&hPinnedTDO ),
        "GetTDO()->OCIObjectPin()" ) )
        return nullptr;

    return hPinnedTDO;
}
