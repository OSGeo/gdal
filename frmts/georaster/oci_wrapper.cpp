/******************************************************************************
 *
 * Name:     oci_wrapper.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Limited wrapper for OCI (Oracle Call Interfaces)
 * Author:   Ivan Lucena [ivan.lucena at oracle.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "oci_wrapper.h"

static const OW_CellDepth ahOW_CellDepth[] = {{"8BIT_U", GDT_UInt8},
                                              {"16BIT_U", GDT_UInt16},
                                              {"16BIT_S", GDT_Int16},
                                              {"32BIT_U", GDT_UInt32},
                                              {"32BIT_S", GDT_Int32},
                                              {"32BIT_REAL", GDT_Float32},
                                              {"64BIT_REAL", GDT_Float64},
                                              {"32BIT_COMPLEX", GDT_CFloat32},
                                              {"64BIT_COMPLEX", GDT_CFloat64},
                                              {"1BIT", GDT_UInt8},
                                              {"2BIT", GDT_UInt8},
                                              {"4BIT", GDT_UInt8}};

/************************************************************************/
/*                            OWSessionPool                             */
/************************************************************************/

/*************************************************
 *  Create Connection from the Extproc Context
 *************************************************/

OWSessionPool::OWSessionPool(const char *pszUserIn, const char *pszPasswordIn,
                             const char *pszServerIn)
{
    CPLDebug("GEOR", "Creating session pool with user=%s, pwd=%s, server=%s",
             pszUserIn, pszPasswordIn, pszServerIn);

    pszUser = CPLStrdup(pszUserIn);
    pszPassword = CPLStrdup(pszPasswordIn);
    pszServer = CPLStrdup(pszServerIn);

    // ------------------------------------------------------
    //  OCI authentication options
    // ------------------------------------------------------

    const char *pszUserId = "/";

    nPoolMode |= OCI_SPC_STMTCACHE;
    nSessMode |= OCI_SESSGET_SPOOL;

    if (EQUAL(pszPassword, "") && EQUAL(pszUser, ""))
    {
        //Authenticated using external credentials
        nSessMode |= OCI_SESSGET_CREDEXT;
    }
    else
    {
        char **papszUserName = CSLTokenizeString2(
            pszUserIn, "[]", CSLT_HONOURSTRINGS | CSLT_PRESERVEQUOTES);

        if (CSLCount(papszUserName) == 2)
        {
            //Authenticated using proxy credentials
            nSessMode |= OCI_SESSGET_CREDPROXY;
        }
        else
        {
            //Authenticated using user/pwd, session pool is homogeneous
            nPoolMode |= OCI_SPC_HOMOGENEOUS;
        }

        CSLDestroy(papszUserName);

        pszUserId = pszUser;
    }

    // ------------------------------------------------------
    //  Initialize Environment handler
    // ------------------------------------------------------

    if (CheckError(OCIEnvCreate(&hEnv,
                                (OCI_DEFAULT | OCI_OBJECT | OCI_THREADED),
                                nullptr, nullptr, nullptr, nullptr, 0, nullptr),
                   nullptr))
    {
        return;
    }

    // ------------------------------------------------------
    //  Initialize Error handler
    // ------------------------------------------------------

    if (CheckError(OCIHandleAlloc(static_cast<dvoid *>(hEnv),
                                  reinterpret_cast<dvoid **>(&hError),
                                  OCI_HTYPE_ERROR, 0, nullptr),
                   nullptr))
    {
        return;
    }

    // ------------------------------------------------------
    //  Initialize Pool handler
    // ------------------------------------------------------

    if (CheckError(OCIHandleAlloc(static_cast<dvoid *>(hEnv),
                                  reinterpret_cast<dvoid **>(&hPool),
                                  OCI_HTYPE_SPOOL, 0, nullptr),
                   hError))
    {
        return;
    }

    // ------------------------------------------------------
    //  Create the Session Pool
    // ------------------------------------------------------

    if (CheckError(
            OCISessionPoolCreate(
                hEnv, hError, hPool, reinterpret_cast<OraText **>(&pszPoolName),
                &nPoolNameLen,
                reinterpret_cast<OraText *>(const_cast<char *>(pszServerIn)),
                static_cast<unsigned>(strlen(pszServerIn)), nSessMin, nSessMax,
                nSessIncr,
                reinterpret_cast<OraText *>(const_cast<char *>(pszUserId)),
                static_cast<unsigned>(strlen(pszUserId)),
                reinterpret_cast<OraText *>(const_cast<char *>(pszPasswordIn)),
                static_cast<unsigned>(strlen(pszPasswordIn)), nPoolMode),
            hError))
    {
        CPLDebug("OCI", "Session pool creation failed");
        return;
    }

    bSucceeded = true;
}

OWSessionPool::~OWSessionPool()
{
    CPLFree(pszUser);
    CPLFree(pszPassword);
    CPLFree(pszServer);

    if (CheckError(OCISessionPoolDestroy(hPool, hError, OCI_SPD_FORCE), hError))
    {
        ;
    }

    if (hPool)
    {
        OCIHandleFree(static_cast<dvoid *>(hPool), OCI_HTYPE_SPOOL);
    }

    if (hError)
    {
        OCIHandleFree(static_cast<dvoid *>(hError), OCI_HTYPE_ERROR);
    }
}

void OWSessionPool::ReInitialize(ub4 nSessMinIn, ub4 nSessMaxIn,
                                 ub4 nSessIncrIn)
{
    if ((nSessMin == nSessMinIn) && (nSessMax == nSessMaxIn) &&
        (nSessIncr == nSessIncrIn))
        return;

    CPLDebug("GEOR", "Reinitialize the Session pool %s with %d, %d, %d",
             pszPoolName, nSessMinIn, nSessMaxIn, nSessIncrIn);
    nSessMin = nSessMinIn;
    nSessMax = nSessMaxIn;
    nSessIncr = nSessIncrIn;

    // ------------------------------------------------------
    //  Reinitialize the Session Pool
    // ------------------------------------------------------

    if (CheckError(
            OCISessionPoolCreate(
                hEnv, hError, hPool, reinterpret_cast<OraText **>(&pszPoolName),
                &nPoolNameLen, nullptr, 0, nSessMinIn, nSessMaxIn, nSessIncrIn,
                nullptr, 0, nullptr, 0, OCI_SPC_REINITIALIZE),
            hError))
    {
        return;
    }
}

OWConnection *OWSessionPool::GetConnection(const char *pszUserIn,
                                           const char *pszPasswordIn,
                                           const char *pszServerIn)
{
    return new OWConnection(this, pszUserIn, pszPasswordIn, pszServerIn);
}

/************************************************************************/
/*                             OWConnection                             */
/************************************************************************/

OWConnection::OWConnection(OCIExtProcContext *poWithContext)
{
    pszUser = CPLStrdup("");
    pszPassword = CPLStrdup("");
    pszServer = CPLStrdup("");

    if (!poWithContext)
    {
        return;
    }

    // ------------------------------------------------------
    //  Retrieve connection handlers
    // ------------------------------------------------------

    if (CheckError(OCIExtProcGetEnv(poWithContext, &hEnv, &hSvcCtx, &hError),
                   nullptr))
    {
        return;
    }

    //  -------------------------------------------------------------------
    //  Get User Name and Schema from SYS_CONTEXT on EXTPROC
    //  -------------------------------------------------------------------

    char szUser[OWTEXT];
    char szSchema[OWTEXT];

    szUser[0] = '\0';
    szSchema[0] = '\0';

    OWStatement *poStmt = CreateStatement(
        "select sys_context('userenv','session_user'),\n"
        "       sys_context('userenv','current_schema') || '.'\n"
        "from dual\n");

    poStmt->Define(szUser);
    poStmt->Define(szSchema);

    CPL_IGNORE_RET_VAL(poStmt->Execute());

    pszExtProcSchema = CPLStrdup(szSchema);
    pszExtProcUser = CPLStrdup(szUser);

    CPLDebug("GEOR", "User from sys_context = %s", pszExtProcUser);

    delete poStmt;

    QueryVersion();

    bSucceeded = true;
    bExtProc = true;
}

/***************************************
 *  Create Connection from User/Pwd
 ***************************************/
OWConnection::OWConnection(const char *pszUserIn, const char *pszPasswordIn,
                           const char *pszServerIn)
{
    pszUser = CPLStrdup(pszUserIn);
    pszPassword = CPLStrdup(pszPasswordIn);
    pszServer = CPLStrdup(pszServerIn);

    // ------------------------------------------------------
    //  Operational Systems's authentication option
    // ------------------------------------------------------

    const char *pszUserId = "/";

    ub4 eCred = OCI_CRED_RDBMS;

    if (EQUAL(pszPassword, "") && EQUAL(pszUser, ""))
    {
        eCred = OCI_CRED_EXT;
    }
    else
    {
        pszUserId = pszUser;
    }

    // ------------------------------------------------------
    //  Initialize Environment handler
    // ------------------------------------------------------

    if (CheckError(OCIEnvCreate(&hEnv,
                                (OCI_DEFAULT | OCI_OBJECT | OCI_THREADED),
                                nullptr, nullptr, nullptr, nullptr, 0, nullptr),
                   nullptr))
    {
        return;
    }

    // ------------------------------------------------------
    //  Initialize Error handler
    // ------------------------------------------------------

    if (CheckError(OCIHandleAlloc(static_cast<dvoid *>(hEnv),
                                  reinterpret_cast<dvoid **>(&hError),
                                  OCI_HTYPE_ERROR, 0, nullptr),
                   nullptr))
    {
        return;
    }

    // ------------------------------------------------------
    //  Initialize Server Context
    // ------------------------------------------------------

    if (CheckError(OCIHandleAlloc(static_cast<dvoid *>(hEnv),
                                  reinterpret_cast<dvoid **>(&hSvcCtx),
                                  OCI_HTYPE_SVCCTX, 0, nullptr),
                   hError))
    {
        return;
    }

    // ------------------------------------------------------
    //  Allocate Server and Authentication (Session) handler
    // ------------------------------------------------------

    if (CheckError(OCIHandleAlloc(static_cast<dvoid *>(hEnv),
                                  reinterpret_cast<dvoid **>(&hServer),
                                  OCI_HTYPE_SERVER, 0, nullptr),
                   hError))
    {
        return;
    }

    if (CheckError(OCIHandleAlloc(static_cast<dvoid *>(hEnv),
                                  reinterpret_cast<dvoid **>(&hSession),
                                  OCI_HTYPE_SESSION, 0, nullptr),
                   hError))
    {
        return;
    }

    // ------------------------------------------------------
    //  Attach to the server
    // ------------------------------------------------------

    if (CheckError(OCIServerAttach(
                       hServer, hError,
                       reinterpret_cast<text *>(const_cast<char *>(pszServer)),
                       static_cast<sb4>(strlen(pszServer)), 0),
                   hError))
    {
        return;
    }

    if (CheckError(OCIAttrSet(static_cast<dvoid *>(hSession), OCI_HTYPE_SESSION,
                              const_cast<char *>(pszUserId),
                              static_cast<ub4>(strlen(pszUserId)),
                              OCI_ATTR_USERNAME, hError),
                   hError))
    {
        return;
    }

    if (CheckError(OCIAttrSet(static_cast<dvoid *>(hSession), OCI_HTYPE_SESSION,
                              pszPassword,
                              static_cast<ub4>(strlen(pszPassword)),
                              OCI_ATTR_PASSWORD, hError),
                   hError))
    {
        return;
    }

    if (CheckError(OCIAttrSet(static_cast<dvoid *>(hSvcCtx), OCI_HTYPE_SVCCTX,
                              static_cast<dvoid *>(hServer), 0, OCI_ATTR_SERVER,
                              static_cast<OCIError *>(hError)),
                   hError))
    {
        return;
    }

    // ------------------------------------------------------
    //  Initialize Session
    // ------------------------------------------------------

    if (CheckError(
            OCISessionBegin(hSvcCtx, hError, hSession, eCred, OCI_DEFAULT),
            hError))
    {
        return;
    }

    // ------------------------------------------------------
    //  Initialize Service
    // ------------------------------------------------------

    if (CheckError(OCIAttrSet(static_cast<dvoid *>(hSvcCtx), OCI_HTYPE_SVCCTX,
                              static_cast<dvoid *>(hSession), 0,
                              OCI_ATTR_SESSION, hError),
                   hError))
    {
        return;
    }

    // -----------------------------------------------------------------
    //  If no user specified or proxy user used, query the session user
    // -----------------------------------------------------------------
    QueryAndInit(EQUAL(pszUser, "") ||
                 (strchr(pszUser, '[') && strchr(pszUser, ']')));

    bSucceeded = true;
}

/**********************************************
 *  Create Connection from the Session Pool
 **********************************************/
OWConnection::OWConnection(OWSessionPool *hPool, const char *pszUserIn,
                           const char *pszPasswordIn, const char *pszServerIn)
{
    hEnv = hPool->hEnv;
    pszUser = CPLStrdup(pszUserIn);
    pszPassword = CPLStrdup(pszPasswordIn);
    pszServer = CPLStrdup(pszServerIn);

    CPLDebug("GEOR", "Creating OWConnection from the pool using %s, %s, %s",
             pszUserIn, pszPasswordIn, pszServerIn);

    // ------------------------------------------------------
    //  Initialize Error handler
    // ------------------------------------------------------

    if (CheckError(OCIHandleAlloc(static_cast<dvoid *>(hPool->hEnv),
                                  reinterpret_cast<dvoid **>(&hError),
                                  OCI_HTYPE_ERROR, 0, nullptr),
                   nullptr))
    {
        return;
    }

    // ------------------------------------------------------
    //  Create authentication info if not a homogeneous pool
    // ------------------------------------------------------

    if (!(hPool->nPoolMode & OCI_SPC_HOMOGENEOUS))
    {

        // ------------------------------------------------------
        //  Initialize Authentication Information Handle
        // ------------------------------------------------------

        if (CheckError(OCIHandleAlloc(static_cast<dvoid *>(hPool->hEnv),
                                      reinterpret_cast<dvoid **>(&hAuth),
                                      OCI_HTYPE_AUTHINFO, 0, nullptr),
                       hError))
        {
            return;
        }

        // ------------------------------------------------------
        //  Set User Name and Paassword
        // ------------------------------------------------------

        const char *pszUserId = "/";
        if (!(hPool->nSessMode & OCI_SESSGET_CREDEXT))
        {
            pszUserId = pszUserIn;
        }

        if (CheckError(
                OCIAttrSet(
                    static_cast<dvoid *>(hAuth), OCI_HTYPE_AUTHINFO,
                    static_cast<dvoid *>(const_cast<char *>(pszUserId)),
                    static_cast<ub4>(strlen(const_cast<char *>(pszUserId))),
                    OCI_ATTR_USERNAME, hError),
                hError))
        {
            return;
        }

        if (CheckError(OCIAttrSet(static_cast<dvoid *>(hAuth),
                                  OCI_HTYPE_AUTHINFO,
                                  static_cast<dvoid *>(pszPassword),
                                  static_cast<ub4>(strlen((char *)pszPassword)),
                                  OCI_ATTR_PASSWORD, hError),
                       hError))
        {
            return;
        }
    }

    // ------------------------------------------------------
    // Get the Session
    // ------------------------------------------------------

    boolean bFound = false;
    OraText *retTagInfo = nullptr;
    ub4 retTagInfoLen;
    if (CheckError(
            OCISessionGet(hPool->hEnv, hError, &hSvcCtx, hAuth,
                          reinterpret_cast<OraText *>(hPool->pszPoolName),
                          hPool->nPoolNameLen, nullptr, 0,
                          static_cast<OraText **>(&retTagInfo),
                          static_cast<ub4 *>(&retTagInfoLen),
                          static_cast<boolean *>(&bFound), hPool->nSessMode),
            hError))
    {
        return;
    }

    // -----------------------------------------------------------------
    //  If no user specified or proxy user used, query the session user
    // -----------------------------------------------------------------
    QueryAndInit((hPool->nSessMode & OCI_SESSGET_CREDEXT) ||
                 (hPool->nSessMode & OCI_SESSGET_CREDPROXY));

    bFromPool = true;
    bSucceeded = true;
}

void OWConnection::QueryAndInit(bool bQuerySessionUser)
{
    QueryVersion();

    if (bQuerySessionUser)
    {
        OWStatement *poStmt =
            CreateStatement("select sys_context('userenv','session_user')\n"
                            "from dual\n");

        pszSessionUser = static_cast<char *>(CPLMalloc(OWNAME));
        poStmt->Define(pszSessionUser);
        CPL_IGNORE_RET_VAL(poStmt->Execute());
        delete poStmt;
        CPLDebug("OCI: ", "Implicit User: %s\n", pszSessionUser);
    }

    // ------------------------------------------------------
    //  Initialize/Describe types
    // ------------------------------------------------------

    CheckError(OCIHandleAlloc(static_cast<dvoid *>(hEnv),
                              reinterpret_cast<dvoid **>(&hDescribe),
                              OCI_HTYPE_DESCRIBE, 0, nullptr),
               hError);

    hNumArrayTDO = DescribeType(SDO_NUMBER_ARRAY);
    hGeometryTDO = DescribeType(SDO_GEOMETRY);
    hGeoRasterTDO = DescribeType(SDO_GEORASTER);
    hElemArrayTDO = DescribeType(SDO_ELEM_INFO_ARRAY);
    hOrdnArrayTDO = DescribeType(SDO_ORDINATE_ARRAY);

    if (nVersion > 10)
    {
        hPCTDO = DescribeType(SDO_PC);
    }
}

void OWConnection::QueryVersion()
{
    // ------------------------------------------------------
    //  Get Character Size based on current Locale
    // ------------------------------------------------------

    OCINlsNumericInfoGet(hEnv, hError, &nCharSize, OCI_NLS_CHARSET_MAXBYTESZ);

    // ------------------------------------------------------
    //  Get Server Version
    // ------------------------------------------------------

    char szVersionTxt[OWTEXT];

    OCIServerVersion(hSvcCtx, hError, reinterpret_cast<text *>(szVersionTxt),
                     OWTEXT, OCI_HTYPE_SVCCTX);

    nVersion = OWParseServerVersion(szVersionTxt);
}

OWConnection::~OWConnection()
{
    CPLFree(pszUser);
    CPLFree(pszSessionUser);
    CPLFree(pszPassword);
    CPLFree(pszServer);

    DestroyType(hNumArrayTDO);
    DestroyType(hGeometryTDO);
    DestroyType(hGeoRasterTDO);
    DestroyType(hElemArrayTDO);
    DestroyType(hOrdnArrayTDO);
    if (hPCTDO)
        DestroyType(hPCTDO);

    if (hDescribe)
        OCIHandleFree(static_cast<dvoid *>(hDescribe), OCI_HTYPE_DESCRIBE);

    // ------------------------------------------------------
    //  Do not free OCI handles from a external procedure
    // ------------------------------------------------------

    if (bExtProc)
    {
        CPLFree(pszExtProcUser);
        CPLFree(pszExtProcSchema);
        return;
    }

    // ------------------------------------------------------
    // Release sessions to the session pool
    // ------------------------------------------------------

    if (bFromPool)
    {
        CPLDebug("GEOR", "Releasing the OWConnection to the pool");
        if (hAuth)
        {
            OCIHandleFree(static_cast<dvoid *>(hAuth), OCI_HTYPE_AUTHINFO);
        }

        if (CheckError(
                OCISessionRelease(hSvcCtx, hError, nullptr, 0, OCI_DEFAULT),
                hError))
        {
            return;
        }

        if (hError)
        {
            OCIHandleFree(static_cast<dvoid *>(hError), OCI_HTYPE_ERROR);
        }

        return;
    }

    // ------------------------------------------------------
    //  Terminate session and free OCI handles
    // ------------------------------------------------------

    if (hSvcCtx && hError && hSession)
    {
        OCISessionEnd(hSvcCtx, hError, hSession, 0);
    }

    if (hSvcCtx && hError)
    {
        OCIServerDetach(hServer, hError, OCI_DEFAULT);
    }

    if (hServer)
    {
        OCIHandleFree(static_cast<dvoid *>(hServer), OCI_HTYPE_SERVER);
    }

    if (hSvcCtx)
    {
        OCIHandleFree(static_cast<dvoid *>(hSvcCtx), OCI_HTYPE_SVCCTX);
    }

    if (hError)
    {
        OCIHandleFree(static_cast<dvoid *>(hError), OCI_HTYPE_ERROR);
    }

    if (hSession)
    {
        OCIHandleFree(static_cast<dvoid *>(hSession), OCI_HTYPE_SESSION);
    }
}

OCIType *OWConnection::DescribeType(const char *pszTypeName)
{
    OCIParam *hParam = nullptr;
    OCIRef *hRef = nullptr;
    OCIType *hType = nullptr;

    CheckError(OCIDescribeAny(
                   hSvcCtx, hError,
                   reinterpret_cast<text *>(const_cast<char *>(pszTypeName)),
                   static_cast<ub4>(strlen(pszTypeName)), OCI_OTYPE_NAME,
                   OCI_DEFAULT, OCI_PTYPE_TYPE, hDescribe),
               hError);

    CheckError(OCIAttrGet(hDescribe, OCI_HTYPE_DESCRIBE, &hParam, nullptr,
                          OCI_ATTR_PARAM, hError),
               hError);

    CheckError(OCIAttrGet(hParam, OCI_DTYPE_PARAM, &hRef, nullptr,
                          OCI_ATTR_REF_TDO, hError),
               hError);

    CheckError(OCIObjectPin(hEnv, hError, hRef, nullptr, (OCIPinOpt)OCI_PIN_ANY,
                            (OCIDuration)OCI_DURATION_SESSION,
                            (OCILockOpt)OCI_LOCK_NONE,
                            reinterpret_cast<dvoid **>(&hType)),
               hError);
    /*
        OCIType*  hType     = nullptr;

        CheckError( OCITypeByName(
            hEnv,
            hError,
            hSvcCtx,
            (text*) TYPE_OWNER,
            (ub4) strlen(TYPE_OWNER),
            (text*) pszTypeName,
            (ub4) strlen(pszTypeName),
            (text*) 0,
            (ub4) 0,
            OCI_DURATION_SESSION,
            OCI_TYPEGET_HEADER,
            &hType), hError );
    */
    return hType;
}

void OWConnection::CreateType(sdo_geometry **pphData)
{
    CheckError(OCIObjectNew(hEnv, hError, hSvcCtx, OCI_TYPECODE_OBJECT,
                            hGeometryTDO, nullptr, OCI_DURATION_CALL, TRUE,
                            reinterpret_cast<dvoid **>(pphData)),
               hError);
}

void OWConnection::DestroyType(OCIType *phType)
{
    if (phType == nullptr)
    {
        return;
    }

    CheckError(OCIObjectUnpin(hEnv, hError, phType), hError);
}

void OWConnection::DestroyType(sdo_geometry **pphData)
{
    CheckError(OCIObjectFree(hEnv, hError, *pphData, (ub2)0), nullptr);
}

void OWConnection::CreateType(OCIArray **phData, OCIType *otype)
{
    CheckError(OCIObjectNew(hEnv, hError, hSvcCtx, OCI_TYPECODE_VARRAY, otype,
                            nullptr, OCI_DURATION_SESSION, FALSE,
                            reinterpret_cast<dvoid **>(phData)),
               hError);
}

void OWConnection::DestroyType(OCIArray **phData)
{
    CheckError(OCIObjectFree(hEnv, hError, reinterpret_cast<OCIColl *>(*phData),
                             (ub2)0),
               nullptr);
}

OWStatement *OWConnection::CreateStatement(const char *pszStatement)
{
    OWStatement *poStatement = new OWStatement(this, pszStatement);

    return poStatement;
}

OCIParam *OWConnection::GetDescription(char *pszTable)
{
    OCIParam *phParam = nullptr;
    OCIParam *phAttrs = nullptr;

    CheckError(
        OCIDescribeAny(hSvcCtx, hError, reinterpret_cast<text *>(pszTable),
                       (ub4)strlen(pszTable), (ub1)OCI_OTYPE_NAME,
                       (ub1)OCI_DEFAULT, (ub1)OCI_PTYPE_TABLE, hDescribe),
        hError);

    CheckError(OCIAttrGet(hDescribe, (ub4)OCI_HTYPE_DESCRIBE, &phParam, nullptr,
                          (ub4)OCI_ATTR_PARAM, hError),
               hError);

    CheckError(OCIAttrGet(phParam, (ub4)OCI_DTYPE_PARAM, &phAttrs, nullptr,
                          (ub4)OCI_ATTR_LIST_COLUMNS, hError),
               hError);

    return phAttrs;
}

bool OWConnection::GetNextField(OCIParam *phTable, int nIndex, char *pszName,
                                int *pnType, int *pnSize, int *pnPrecision,
                                signed short *pnScale)
{
    OCIParam *hParamDesc = nullptr;

    sword nStatus = 0;

    nStatus = OCIParamGet(phTable, (ub4)OCI_DTYPE_PARAM, hError,
                          reinterpret_cast<dvoid **>(&hParamDesc),  // Warning
                          (ub4)nIndex + 1);

    if (nStatus != OCI_SUCCESS)
    {
        return false;
    }

    char *pszFieldName = nullptr;
    ub4 nNameLength = 0;

    CheckError(OCIAttrGet(hParamDesc, (ub4)OCI_DTYPE_PARAM, &pszFieldName,
                          (ub4 *)&nNameLength, (ub4)OCI_ATTR_NAME, hError),
               hError);

    ub2 nOCIType = 0;

    CheckError(OCIAttrGet(hParamDesc, (ub4)OCI_DTYPE_PARAM, &nOCIType, nullptr,
                          (ub4)OCI_ATTR_DATA_TYPE, hError),
               hError);

    ub2 nOCILen = 0;

    CheckError(OCIAttrGet(hParamDesc, (ub4)OCI_DTYPE_PARAM, &nOCILen, nullptr,
                          (ub4)OCI_ATTR_DATA_SIZE, hError),
               hError);

    unsigned short nOCIPrecision = 0;
    sb1 nOCIScale = 0;

    if (nOCIType == SQLT_NUM)
    {
        CheckError(OCIAttrGet(hParamDesc, (ub4)OCI_DTYPE_PARAM, &nOCIPrecision,
                              nullptr, (ub4)OCI_ATTR_PRECISION, hError),
                   hError);

        CheckError(OCIAttrGet(hParamDesc, (ub4)OCI_DTYPE_PARAM, &nOCIScale,
                              nullptr, (ub4)OCI_ATTR_SCALE, hError),
                   hError);

        if (nOCIPrecision > 255)  // Lesson learned from ogrocisession.cpp
        {
            nOCIPrecision = nOCIPrecision / 256;
        }
    }

    nNameLength = MIN(nNameLength, OWNAME);

    strncpy(pszName, pszFieldName, nNameLength);
    pszName[nNameLength] = '\0';

    *pnType = (int)nOCIType;
    *pnSize = (int)nOCILen;
    *pnPrecision = (int)nOCIPrecision;
    *pnScale = (signed short)nOCIScale;

    return true;
}

bool OWConnection::StartTransaction()
{
    CheckError(OCITransStart(hSvcCtx, hError, (uword)30, OCI_TRANS_NEW),
               hError);

    return true;
}

bool OWConnection::Commit()
{
    CheckError(OCITransCommit(hSvcCtx, hError, OCI_DEFAULT), hError);

    return true;
}

/************************************************************************/
/*                             OWStatement                              */
/************************************************************************/

OWStatement::OWStatement(OWConnection *pConnect, const char *pszStatement)
    : poConnection(pConnect), hError(poConnection->hError)
{
    //  -----------------------------------------------------------
    //  Create Statement handler
    //  -----------------------------------------------------------

    OCIStmt *hStatement = nullptr;

    CheckError(OCIHandleAlloc(static_cast<dvoid *>(poConnection->hEnv),
                              reinterpret_cast<dvoid **>(&hStatement),
                              (ub4)OCI_HTYPE_STMT, (size_t)0, nullptr),
               hError);

    hStmt = hStatement;  // Save Statement Handle

    //  -----------------------------------------------------------
    //  Prepare Statement
    //  -----------------------------------------------------------

    CheckError(OCIStmtPrepare(
                   hStmt, hError,
                   reinterpret_cast<text *>(const_cast<char *>(pszStatement)),
                   (ub4)strlen(pszStatement), (ub4)OCI_NTV_SYNTAX,
                   (ub4)OCI_DEFAULT),
               hError);

    //  -----------------------------------------------------------
    //  Get Statement type
    //  -----------------------------------------------------------

    ub2 nStmtType;

    CheckError(OCIAttrGet(static_cast<dvoid *>(hStmt), (ub4)OCI_HTYPE_STMT,
                          &nStmtType, nullptr, (ub4)OCI_ATTR_STMT_TYPE, hError),
               hError);

    //  -----------------------------------------------------------
    //  Set Statement mode
    //  -----------------------------------------------------------

    if (nStmtType != OCI_STMT_SELECT)
    {
        nStmtMode = OCI_DEFAULT;
    }

    CPLDebug("PL/SQL", "\n%s\n", pszStatement);
}

OWStatement::~OWStatement()
{
    OCIHandleFree(static_cast<dvoid *>(hStmt), (ub4)OCI_HTYPE_STMT);
}

bool OWStatement::Execute(int nRows)
{
    sword nStatus =
        OCIStmtExecute(poConnection->hSvcCtx, hStmt, hError, (ub4)nRows, (ub4)0,
                       nullptr, nullptr, nStmtMode);

    if (CheckError(nStatus, hError))
    {
        return false;
    }

    if (nStatus == OCI_SUCCESS_WITH_INFO || nStatus == OCI_NO_DATA)
    {
        return false;
    }

    return true;
}

bool OWStatement::Fetch(int nRows)
{
    sword nStatus = 0;

    nStatus =
        OCIStmtFetch2(static_cast<OCIStmt *>(hStmt),
                      static_cast<OCIError *>(poConnection->hError), (ub4)nRows,
                      (ub2)OCI_FETCH_NEXT, (sb4)0, (ub4)OCI_DEFAULT);

    if (nStatus == OCI_NO_DATA)
    {
        return false;
    }

    if (CheckError(nStatus, poConnection->hError))
    {
        return false;
    }

    return true;
}

void OWStatement::Bind(int *pnData)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, pnData,
                            (sb4)sizeof(int), (ub2)SQLT_INT, nullptr, nullptr,
                            nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Bind(long *pnData)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, pnData,
                            (sb4)sizeof(long), (ub2)SQLT_INT, nullptr, nullptr,
                            nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Bind(long long *pnData)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, pnData,
                            (sb4)sizeof(long long), (ub2)SQLT_INT, nullptr,
                            nullptr, nullptr, (ub4)0, nullptr,
                            (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Bind(double *pnData)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, pnData,
                            (sb4)sizeof(double), (ub2)SQLT_BDOUBLE, nullptr,
                            nullptr, nullptr, (ub4)0, nullptr,
                            (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Bind(char *pData, long nData)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, pData,
                            (sb4)nData, (ub2)SQLT_LBI, nullptr, nullptr,
                            nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Bind(sdo_geometry **pphData)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, nullptr,
                            (sb4)0, (ub2)SQLT_NTY, nullptr, nullptr, nullptr,
                            (ub4)0, nullptr, (ub4)OCI_DEFAULT),
               hError);

    CheckError(OCIBindObject(hBind, hError, poConnection->hGeometryTDO,
                             reinterpret_cast<dvoid **>(pphData), nullptr,
                             nullptr, nullptr),
               hError);
}

void OWStatement::Bind(OCILobLocator **pphLocator)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, pphLocator,
                            (sb4)-1, (ub2)SQLT_CLOB, nullptr, nullptr, nullptr,
                            (ub4)0, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Bind(OCIArray **pphData, OCIType *type)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, nullptr,
                            (sb4)0, (ub2)SQLT_NTY, nullptr, nullptr, nullptr,
                            (ub4)0, nullptr, (ub4)OCI_DEFAULT),
               hError);

    CheckError(OCIBindObject(hBind, hError, type,
                             reinterpret_cast<dvoid **>(pphData), nullptr,
                             nullptr, nullptr),
               hError);
}

void OWStatement::Bind(char *pszData, int nSize)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd, pszData,
                            (sb4)nSize, (ub2)SQLT_STR, nullptr, nullptr,
                            nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Define(int *pnData)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, pnData,
                              (sb4)sizeof(int), (ub2)SQLT_INT, nullptr, nullptr,
                              nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Define(long *pnData)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, pnData,
                              (sb4)sizeof(long int), (ub2)SQLT_INT, nullptr,
                              nullptr, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Define(long long *pnData)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, pnData,
                              (sb4)sizeof(long long), (ub2)SQLT_INT, nullptr,
                              nullptr, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Define(double *pfdData)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, pfdData,
                              (sb4)sizeof(double), (ub2)SQLT_BDOUBLE, nullptr,
                              nullptr, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Define(char *pszData, int nSize)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, pszData,
                              (sb4)nSize, (ub2)SQLT_STR, nullptr, nullptr,
                              nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::Define(OCILobLocator **pphLocator)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDescriptorAlloc(poConnection->hEnv,
                                  reinterpret_cast<void **>(pphLocator),
                                  OCI_DTYPE_LOB, 0, nullptr),
               hError);

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol,
                              pphLocator, (sb4)0, (ub2)SQLT_BLOB, nullptr,
                              nullptr, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

void OWStatement::WriteCLob(OCILobLocator **pphLocator, char *pszData)
{
    nNextCol++;

    if (CheckError(OCIDescriptorAlloc(poConnection->hEnv,
                                      reinterpret_cast<void **>(pphLocator),
                                      OCI_DTYPE_LOB, (size_t)0, nullptr),
                   hError))
    {
        CPLDebug("OCI", "Error in WriteCLob");
        return;
    }

    if (CheckError(OCILobCreateTemporary(
                       poConnection->hSvcCtx, poConnection->hError, *pphLocator,
                       (ub4)OCI_DEFAULT, (ub1)OCI_DEFAULT, (ub1)OCI_TEMP_CLOB,
                       false, OCI_DURATION_SESSION),
                   hError))
    {
        CPLDebug("OCI", "Error in WriteCLob creating temporary lob");
        return;
    }

    ub4 nAmont = (ub4)strlen(pszData);

    if (CheckError(OCILobWrite(poConnection->hSvcCtx, hError, *pphLocator,
                               &nAmont, (ub4)1, pszData, (ub4)strlen(pszData),
                               (ub1)OCI_ONE_PIECE, nullptr, nullptr, (ub2)0,
                               (ub1)SQLCS_IMPLICIT),
                   hError))
    {
        CPLDebug("OCI", "Error in WriteCLob writing the lob");
        return;
    }
}

void OWStatement::Define(OCIArray **pphData)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, nullptr,
                              (sb4)0, (ub2)SQLT_NTY, nullptr, nullptr, nullptr,
                              (ub4)OCI_DEFAULT),
               hError);

    CheckError(OCIDefineObject(hDefine, hError, poConnection->hNumArrayTDO,
                               reinterpret_cast<dvoid **>(pphData), nullptr,
                               nullptr, nullptr),
               hError);
}

void OWStatement::Define(sdo_georaster **pphData)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, nullptr,
                              (sb4)0, (ub2)SQLT_NTY, nullptr, nullptr, nullptr,
                              (ub4)OCI_DEFAULT),
               hError);

    CheckError(OCIDefineObject(hDefine, hError, poConnection->hGeoRasterTDO,
                               reinterpret_cast<dvoid **>(pphData), nullptr,
                               nullptr, nullptr),
               hError);
}

void OWStatement::Define(sdo_geometry **pphData)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, nullptr,
                              (sb4)0, (ub2)SQLT_NTY, nullptr, nullptr, nullptr,
                              (ub4)OCI_DEFAULT),
               hError);

    CheckError(OCIDefineObject(hDefine, hError, poConnection->hGeometryTDO,
                               reinterpret_cast<dvoid **>(pphData), nullptr,
                               nullptr, nullptr),
               hError);
}

void OWStatement::Define(sdo_pc **pphData)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol, nullptr,
                              (sb4)0, (ub2)SQLT_NTY, nullptr, nullptr, nullptr,
                              (ub4)OCI_DEFAULT),
               hError);

    CheckError(OCIDefineObject(hDefine, hError, poConnection->hPCTDO,
                               reinterpret_cast<dvoid **>(pphData), nullptr,
                               nullptr, nullptr),
               hError);
}

void OWStatement::Define(OCILobLocator **pphLocator, long nIterations)
{
    OCIDefine *hDefine = nullptr;

    nNextCol++;

    long i;

    for (i = 0; i < nIterations; i++)
    {
        OCIDescriptorAlloc(poConnection->hEnv,
                           reinterpret_cast<void **>(&pphLocator[i]),
                           OCI_DTYPE_LOB, (size_t)0, nullptr);
    }

    CheckError(OCIDefineByPos(hStmt, &hDefine, hError, (ub4)nNextCol,
                              pphLocator, (sb4)-1, (ub2)SQLT_BLOB, nullptr,
                              nullptr, nullptr, (ub4)OCI_DEFAULT),
               hError);
}

int OWStatement::GetInteger(OCINumber *ppoData)
{
    sb4 nRetVal;

    CheckError(OCINumberToInt(hError, ppoData, (uword)sizeof(sb4),
                              OCI_NUMBER_SIGNED, &nRetVal),
               hError);

    return nRetVal;
}

double OWStatement::GetDouble(OCINumber *ppoData)
{
    double dfRetVal = 0.0;

    CheckError(
        OCINumberToReal(hError, ppoData, (uword)sizeof(dfRetVal), &dfRetVal),
        hError);

    return dfRetVal;
}

char *OWStatement::GetString(OCIString *ppoData)
{
    return (char *)OCIStringPtr(poConnection->hEnv, ppoData);
}

void OWStatement::Free(OCILobLocator **pphLocator, int nCount)
{
    if (nCount > 0 && pphLocator != nullptr)
    {
        int i = 0;
        for (i = 0; i < nCount; i++)
        {
            if (pphLocator[i] != nullptr)
            {
                OCIDescriptorFree(&pphLocator[i], OCI_DTYPE_LOB);
            }
        }
    }
}

int OWStatement::GetElement(OCIArray **ppoData, int nIndex, int *pnResult)
{
    boolean exists;
    OCINumber *oci_number = nullptr;
    ub4 element_type;

    *pnResult = 0;

    if (CheckError(OCICollGetElem(poConnection->hEnv, hError,
                                  reinterpret_cast<OCIColl *>(*ppoData),
                                  (sb4)nIndex, &exists,
                                  reinterpret_cast<dvoid **>(&oci_number),
                                  nullptr),
                   hError))
    {
        return *pnResult;
    }

    if (CheckError(OCINumberToInt(hError, oci_number, (uword)sizeof(ub4),
                                  OCI_NUMBER_UNSIGNED, &element_type),
                   hError))
    {
        return *pnResult;
    }

    *pnResult = (int)element_type;

    return *pnResult;
}

double OWStatement::GetElement(OCIArray **ppoData, int nIndex,
                               double *pdfResult)
{
    boolean exists;
    OCINumber *oci_number = nullptr;
    double element_type;

    *pdfResult = 0.0;

    if (CheckError(OCICollGetElem(poConnection->hEnv, hError,
                                  reinterpret_cast<OCIColl *>(*ppoData),
                                  (sb4)nIndex, &exists,
                                  reinterpret_cast<dvoid **>(&oci_number),
                                  nullptr),
                   hError))
    {
        return *pdfResult;
    }

    if (CheckError(OCINumberToReal(hError, oci_number, (uword)sizeof(double),
                                   &element_type),
                   hError))
    {
        return *pdfResult;
    }

    *pdfResult = (double)element_type;

    return *pdfResult;
}

void OWStatement::AddElement(OCIArray *poData, int nValue)
{
    OCINumber oci_number;

    CheckError(OCINumberFromInt(hError, &nValue, (uword)sizeof(ub4),
                                OCI_NUMBER_UNSIGNED, &oci_number),
               hError);

    CheckError(OCICollAppend(poConnection->hEnv, hError, &oci_number, nullptr,
                             reinterpret_cast<OCIColl *>(poData)),
               hError);
}

void OWStatement::AddElement(OCIArray *poData, double dfValue)
{
    OCINumber oci_number;

    CheckError(
        OCINumberFromReal(hError, &dfValue, (uword)sizeof(double), &oci_number),
        hError);

    CheckError(OCICollAppend(poConnection->hEnv, hError, &oci_number, nullptr,
                             reinterpret_cast<OCIColl *>(poData)),
               hError);
}

unsigned long OWStatement::GetBlobLength(OCILobLocator *phLocator)
{
    ub8 nSize = (ub8)0;

    if (CheckError(OCILobGetLength2(poConnection->hSvcCtx, poConnection->hError,
                                    phLocator, (ub8 *)&nSize),
                   hError))
    {
        return 0;
    }

    return static_cast<unsigned long>(nSize);
}

unsigned long OWStatement::ReadBlob(OCILobLocator *phLocator, void *pBuffer,
                                    unsigned long nSize)
{
    return ReadBlob(phLocator, pBuffer, (unsigned long)1, nSize);
}

unsigned long OWStatement::ReadBlob(OCILobLocator *phLocator, void *pBuffer,
                                    unsigned long nOffset, unsigned long nSize)
{
    ub8 nAmont = (ub8)nSize;

    if (CheckError(OCILobRead2(poConnection->hSvcCtx, hError, phLocator,
                               (ub8 *)&nAmont, nullptr, (ub8)nOffset,
                               (void *)pBuffer, (ub8)nSize, (ub1)OCI_ONE_PIECE,
                               nullptr, (OCICallbackLobRead2) nullptr, (ub2)0,
                               (ub1)SQLCS_IMPLICIT),
                   hError))
    {
        return 0;
    }

    return (unsigned long)nAmont;
}

bool OWStatement::WriteBlob(OCILobLocator *phLocator, void *pBuffer,
                            unsigned long nSize)
{
    ub8 nAmont = WriteBlob(phLocator, pBuffer, (unsigned long)1, nSize);

    return (nAmont == (ub8)nSize);
}

unsigned long OWStatement::WriteBlob(OCILobLocator *phLocator, void *pBuffer,
                                     unsigned long nOffset, unsigned long nSize)
{
    ub8 nAmont = (ub8)nSize;

    if (CheckError(OCILobWrite2(poConnection->hSvcCtx, hError, phLocator,
                                &nAmont, nullptr, (ub8)nOffset, pBuffer,
                                (ub8)nSize, (ub1)OCI_ONE_PIECE, nullptr,
                                nullptr, (ub2)0, (ub1)SQLCS_IMPLICIT),
                   hError))
    {
        return (unsigned long)0;
    }

    return static_cast<unsigned long>(nAmont);
}

bool OWStatement::TrimLob(OCILobLocator *phLocator, unsigned long newLen)
{
    if (CheckError(
            OCILobTrim2(poConnection->hSvcCtx, hError, phLocator, (ub8)newLen),
            hError))
    {
        return 0;
    }

    return 1;
}

char *OWStatement::ReadCLob(OCILobLocator *phLocator)
{
    ub4 nSize = 0;
    ub4 nAmont = 0;

    char *pszBuffer = nullptr;

    if (CheckError(
            OCILobGetLength(poConnection->hSvcCtx, hError, phLocator, &nSize),
            hError))
    {
        return nullptr;
    }

    nSize *= this->poConnection->nCharSize;

    pszBuffer = (char *)VSICalloc(1, nSize + 1);

    if (pszBuffer == nullptr)
    {
        return nullptr;
    }

    if (CheckError(OCILobRead(poConnection->hSvcCtx, hError, phLocator, &nAmont,
                              (ub4)1, pszBuffer, (ub4)nSize, nullptr, nullptr,
                              (ub2)0, (ub1)SQLCS_IMPLICIT),
                   hError))
    {
        CPLFree(pszBuffer);
        return nullptr;
    }

    pszBuffer[nAmont] = '\0';

    return pszBuffer;
}

// Free OCIDescriptor for the LOB, if it is temporary lob, it is freed too.
void OWStatement::FreeLob(OCILobLocator *phLocator)
{
    boolean is_temporary;

    if (phLocator == nullptr)
        return;

    if (CheckError(OCILobIsTemporary(poConnection->hEnv, hError, phLocator,
                                     &is_temporary),
                   hError))
    {
        CPLDebug("OCI", "OCILobIsTemporary failed");
        OCIDescriptorFree(phLocator, OCI_DTYPE_LOB);
        return;
    }

    if (is_temporary)
    {
        if (CheckError(
                OCILobFreeTemporary(poConnection->hSvcCtx, hError, phLocator),
                hError))
        {
            CPLDebug("OCI", "OCILobFreeTemporary failed");
            OCIDescriptorFree(phLocator, OCI_DTYPE_LOB);
            return;
        }
    }

    OCIDescriptorFree(phLocator, OCI_DTYPE_LOB);
}

void OWStatement::BindName(const char *pszName, int *pnData)
{
    OCIBind *hBind = nullptr;

    CheckError(
        OCIBindByName(
            static_cast<OCIStmt *>(hStmt), reinterpret_cast<OCIBind **>(&hBind),
            static_cast<OCIError *>(hError),
            reinterpret_cast<text *>(const_cast<char *>(pszName)), (sb4)-1,
            static_cast<dvoid *>(pnData), (sb4)sizeof(int), (ub2)SQLT_INT,
            nullptr, nullptr, nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
        hError);
}

void OWStatement::BindName(const char *pszName, long *pnData)
{
    OCIBind *hBind = nullptr;

    CheckError(
        OCIBindByName(
            static_cast<OCIStmt *>(hStmt), reinterpret_cast<OCIBind **>(&hBind),
            static_cast<OCIError *>(hError),
            reinterpret_cast<text *>(const_cast<char *>(pszName)), (sb4)-1,
            static_cast<dvoid *>(pnData), (sb4)sizeof(long), (ub2)SQLT_INT,
            nullptr, nullptr, nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
        hError);
}

void OWStatement::BindName(const char *pszName, long long *pnData)
{
    OCIBind *hBind = nullptr;

    CheckError(
        OCIBindByName(
            static_cast<OCIStmt *>(hStmt), reinterpret_cast<OCIBind **>(&hBind),
            static_cast<OCIError *>(hError),
            reinterpret_cast<text *>(const_cast<char *>(pszName)), (sb4)-1,
            static_cast<dvoid *>(pnData), (sb4)sizeof(long long), (ub2)SQLT_INT,
            nullptr, nullptr, nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
        hError);
}

void OWStatement::BindName(const char *pszName, double *pnData)
{
    OCIBind *hBind = nullptr;

    CheckError(
        OCIBindByName(static_cast<OCIStmt *>(hStmt),
                      reinterpret_cast<OCIBind **>(&hBind),
                      static_cast<OCIError *>(hError),
                      reinterpret_cast<text *>(const_cast<char *>(pszName)),
                      (sb4)-1, static_cast<dvoid *>(pnData),
                      (sb4)sizeof(double), (ub2)SQLT_BDOUBLE, nullptr, nullptr,
                      nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
        hError);
}

void OWStatement::BindName(const char *pszName, char *pszData, int nSize)
{
    OCIBind *hBind = nullptr;

    CheckError(
        OCIBindByName(
            static_cast<OCIStmt *>(hStmt), reinterpret_cast<OCIBind **>(&hBind),
            static_cast<OCIError *>(hError),
            reinterpret_cast<text *>(const_cast<char *>(pszName)), (sb4)-1,
            static_cast<dvoid *>(pszData), (sb4)nSize, (ub2)SQLT_STR, nullptr,
            nullptr, nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
        hError);
}

void OWStatement::BindName(const char *pszName, OCILobLocator **pphLocator)
{
    OCIBind *hBind = nullptr;

    CheckError(OCIDescriptorAlloc(poConnection->hEnv,
                                  reinterpret_cast<void **>(pphLocator),
                                  OCI_DTYPE_LOB, 0, nullptr),
               hError);

    CheckError(
        OCIBindByName(
            static_cast<OCIStmt *>(hStmt), reinterpret_cast<OCIBind **>(&hBind),
            static_cast<OCIError *>(hError),
            reinterpret_cast<text *>(const_cast<char *>(pszName)), (sb4)-1,
            static_cast<dvoid *>(pphLocator), (sb4)-1, (ub2)SQLT_CLOB, nullptr,
            nullptr, nullptr, (ub4)0, nullptr, (ub4)OCI_DEFAULT),
        hError);
}

void OWStatement::BindArray(void *pData, long nSize)
{
    OCIBind *hBind = nullptr;

    nNextBnd++;

    CheckError(OCIBindByPos(hStmt, &hBind, hError, (ub4)nNextBnd,
                            static_cast<dvoid *>(pData),
                            (sb4)nSize * sizeof(double), (ub2)SQLT_BIN, nullptr,
                            nullptr, nullptr, (ub4)0, nullptr,
                            (ub4)OCI_DEFAULT),
               hError);

    CheckError(OCIBindArrayOfStruct(hBind, hError, (ub4)nSize * sizeof(double),
                                    (ub4)0, (ub4)0, (ub4)0),
               hError);
}

/************************************************************************/
/*              Check for valid integer number in a string              */
/************************************************************************/

bool OWIsNumeric(const char *pszText)
{
    if (pszText == nullptr)
    {
        return false;
    }

    const char *pszPos = pszText;

    while (*pszPos != '\0')
    {
        if (*pszPos < '0' || *pszPos > '9')
            return false;
        pszPos++;
    }

    return true;
}

/************************************************************************/
/*                            Remove quotes                             */
/************************************************************************/

char *OWRemoveQuotes(const char *pszText)
{
    const size_t nSize = strlen(pszText);

    if (nSize > 2 && pszText[0] != '"' && pszText[nSize - 1] != '"')
    {
        return CPLStrdup(pszText);
    }

    char *pszResult = static_cast<char *>(CPLMalloc(nSize - 1));

    CPLStrlcpy(pszResult, &pszText[1], nSize - 1);

    return pszResult;
}

/************************************************************************/
/*                    To upper if there is no quotes                    */
/************************************************************************/

void OWUpperIfNoQuotes(char *pszText)
{
    const size_t nSize = strlen(pszText);

    if (nSize > 2 && pszText[0] == '"' && pszText[nSize - 1] == '"')
    {
        return;
    }

    for (size_t i = 0; i < nSize; i++)
    {
        pszText[i] =
            static_cast<char>(toupper(static_cast<unsigned char>(pszText[i])));
    }
}

/************************************************************************/
/*                 Parse Value after a Hint on a string                 */
/************************************************************************/
static CPLString OWParseValue(const char *pszText, const char *pszSeparators,
                              const char *pszHint, int nOffset)
{
    if (pszText == nullptr)
        return nullptr;

    int i = 0;
    int nCount = 0;

    char **papszTokens =
        CSLTokenizeString2(pszText, pszSeparators, CSLT_PRESERVEQUOTES);

    nCount = CSLCount(papszTokens);
    CPLString osResult;

    for (i = 0; (i + nOffset) < nCount; i++)
    {
        if (EQUAL(papszTokens[i], pszHint))
        {
            osResult = papszTokens[i + nOffset];
            break;
        }
    }

    CSLDestroy(papszTokens);

    return osResult;
}

/************************************************************************/
/*                     Parse SDO_GEOR.INIT entries                      */
/************************************************************************/

/* Input Examples:
 *
 * "ID, RASTER, NAME VALUES (102, SDO_GEOR.INIT('RDT_80', 80), 'Nashua')"
 *
 */

CPLString OWParseSDO_GEOR_INIT(const char *pszInsert, int nField)
{
    char szUpcase[OWTEXT];
    char *pszIn = nullptr;

    snprintf(szUpcase, sizeof(szUpcase), "%s", pszInsert);

    for (pszIn = szUpcase; *pszIn != '\0'; pszIn++)
    {
        *pszIn = (char)toupper(static_cast<unsigned char>(*pszIn));
    }

    char *pszStart = strstr(szUpcase, "SDO_GEOR.INIT");

    if (pszStart == nullptr)
    {
        return "";
    }

    char *pszEnd = strstr(pszStart, ")");

    if (pszEnd == nullptr)
    {
        return "";
    }

    pszStart += strlen("SDO_GEOR.");

    pszEnd++;

    int nLength = static_cast<int>(pszEnd - pszStart + 1);

    char szBuffer[OWTEXT];

    strncpy(szBuffer, pszStart, nLength);
    szBuffer[nLength] = '\0';

    auto osValue = OWParseValue(szBuffer, " (,)", "INIT", nField);

    return EQUAL(osValue, "") ? "NULL" : osValue;
}

/************************************************************************/
/*                        Parse Release Version                         */
/************************************************************************/

/* Input Examples:
 *
 * "Oracle Database 11g Enterprise Edition Release 11.1.0.6.0 - Production
 * With the Partitioning, OLAP, Data Mining and Real Application Testing
 * options"
 *
 */

int OWParseServerVersion(const char *pszText)
{
    return atoi(OWParseValue(pszText, " .", "Release", 1));
}

/************************************************************************/
/*                           Parse EPSG Codes                           */
/************************************************************************/

/* Input Examples:
 *
 *      DATUM["World Geodetic System 1984 (EPSG ID 6326)",
 *      SPHEROID["WGS 84 (EPSG ID 7030)",6378137,298.257223563]],
 *      PROJECTION["UTM zone 50N (EPSG OP 16050)"],
 */

int OWParseEPSG(const char *pszText)
{
    return atoi(OWParseValue(pszText, " ()", "EPSG", 2));
}

/************************************************************************/
/*                    Convert Data type description                     */
/************************************************************************/

GDALDataType OWGetDataType(const char *pszCellDepth)
{
    unsigned int i;

    for (i = 0; i < (sizeof(ahOW_CellDepth) / sizeof(OW_CellDepth)); i++)
    {
        if (EQUAL(ahOW_CellDepth[i].pszValue, pszCellDepth))
        {
            return ahOW_CellDepth[i].eDataType;
        }
    }

    return GDT_Unknown;
}

/************************************************************************/
/*                    Convert Data type description                     */
/************************************************************************/

const char *OWSetDataType(const GDALDataType eType)

{
    unsigned int i;

    for (i = 0; i < (sizeof(ahOW_CellDepth) / sizeof(OW_CellDepth)); i++)
    {
        if (ahOW_CellDepth[i].eDataType == eType)
        {
            return ahOW_CellDepth[i].pszValue;
        }
    }

    return "Unknown";
}

/************************************************************************/
/*                          Check for Failure                           */
/************************************************************************/
bool CheckError(sword nStatus, OCIError *hError)
{
    text szMsg[OWTEXT];
    sb4 nCode = 0;

    switch (nStatus)
    {
        case OCI_SUCCESS:
            return false;
            break;
        case OCI_NEED_DATA:
            CPLError(CE_Failure, CPLE_AppDefined, "OCI_NEED_DATA\n");
            break;
        case OCI_NO_DATA:
            CPLError(CE_Failure, CPLE_AppDefined, "OCI_NODATA\n");
            break;
        case OCI_INVALID_HANDLE:
            CPLError(CE_Failure, CPLE_AppDefined, "OCI_INVALID_HANDLE");
            break;
        case OCI_STILL_EXECUTING:
            CPLError(CE_Failure, CPLE_AppDefined, "OCI_STILL_EXECUTE\n");
            break;
        case OCI_CONTINUE:
            CPLError(CE_Failure, CPLE_AppDefined, "OCI_CONTINUE\n");
            break;
        case OCI_ERROR:

        case OCI_SUCCESS_WITH_INFO:

            if (hError == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "OCI_ERROR with no error handler");
            }

            OCIErrorGet(static_cast<dvoid *>(hError), (ub4)1, nullptr, &nCode,
                        szMsg, (ub4)sizeof(szMsg), OCI_HTYPE_ERROR);

            if (nCode == 1405)  // Null field
            {
                return false;
            }
            else if (
                nCode == 28002 ||
                nCode ==
                    28098)  // password expires codes (ORA-28002, ORA-28098)
            {
                static bool bPasswordExpiredLogged = false;
                if (!bPasswordExpiredLogged)
                {
                    bPasswordExpiredLogged = true;
                    // Workaround, when this is called with gdal_translate,
                    // the error message is not printed because it has
                    // an error handler that suppresses the message.
                    // It pushes a default error handler that prints the message,
                    // and then pops it to restore the previous error handler.

                    CPLPushErrorHandler(CPLDefaultErrorHandler);
                    CPLError(CE_Warning, CPLE_AppDefined, "%s", szMsg);
                    CPLPopErrorHandler();
                }

                return false;
            }

            CPLError(CE_Failure, CPLE_AppDefined, "%s", szMsg);

            break;

        default:

            if (hError == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "OCI_ERROR with no error handler");
            }

            OCIErrorGet(static_cast<dvoid *>(hError), (ub4)1, nullptr, &nCode,
                        szMsg, (ub4)sizeof(szMsg), OCI_HTYPE_ERROR);

            CPLError(CE_Failure, CPLE_AppDefined, "%s", szMsg);

            break;
    }

    return true;
}
