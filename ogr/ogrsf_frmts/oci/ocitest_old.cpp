/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Test mainline for Oracle Spatial Driver low level functions.
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
 * Revision 1.1  2002/12/28 04:07:27  warmerda
 * New
 *
 */

#include "ogr_oci.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    OGROCISession *poSession = NULL;
    const char *pszStatement = "SELECT * FROM NEPSITE";
//    const char *pszStatement = "SELECT * FROM SCOTT.EMP";
    int nStatus, nColCount;
    char *apszColValues[1024];
    char *apszColNames[1024];

    poSession = OGRGetOCISession( "system", "LetoKing", "" );

    printf( "poSession = %p\n", poSession );

    nStatus = 
        OCIStmtPrepare( poSession->hStatement, poSession->hError, 
                        (text *) pszStatement, strlen(pszStatement),
                        (ub4)OCI_NTV_SYNTAX, (ub4)OCI_DEFAULT );
    printf( "nStatus = %d\n", nStatus );

    nStatus = 
        OCIStmtExecute( poSession->hSvcCtx, poSession->hStatement, 
                        poSession->hError, (ub4)0, (ub4)0, 
                        (OCISnapshot *)NULL, (OCISnapshot *)NULL, 
                        (ub4)OCI_DEFAULT );

    /* Describe the result */
    for( int iParm = 1; TRUE; iParm++ )
    {
        OCIParam  *hParmDesc;
        ub2        nDType, nWidth;
        char      *pszColName;
        ub4        nColLen;
        OCIDefine *hDefn = NULL;

        nStatus = 
            OCIParamGet( poSession->hStatement, OCI_HTYPE_STMT, 
                         poSession->hError, (dvoid**)&hParmDesc, (ub4) iParm );
        printf( "nStatus (OCIParamGet) = %d\n", nStatus );
        if( nStatus != OCI_SUCCESS )
        {
            nColCount = iParm - 1;
            break;
        }
        
        nStatus = 
            OCIAttrGet( hParmDesc, OCI_DTYPE_PARAM, (dvoid **)&nDType, 0,
                        OCI_ATTR_DATA_TYPE, poSession->hError );
        printf( "nStatus (OCIAttrGet) = %d\n", nStatus );

        nStatus = 
            OCIAttrGet( hParmDesc, OCI_DTYPE_PARAM, (dvoid **)&nWidth, 0,
                        OCI_ATTR_DATA_SIZE, poSession->hError );
        printf( "nStatus (OCIAttrGet) = %d\n", nStatus );

        nStatus = 
            OCIAttrGet( hParmDesc, OCI_DTYPE_PARAM, (dvoid **)&pszColName,
                        &nColLen, OCI_ATTR_NAME, poSession->hError );
        printf( "nStatus (OCIAttrGet) = %d\n", nStatus );

        apszColNames[iParm-1] = (char *) CPLMalloc(nColLen+1);
        strncpy( apszColNames[iParm-1], pszColName, nColLen );
        apszColNames[iParm-1][nColLen] = '\0';
        printf( "  Column %s: %d/%d\n", apszColNames[iParm-1], nDType, nWidth);

        if( nDType != 108 )
        {
            apszColValues[iParm-1] = (char *) CPLMalloc(nWidth+1);
            nStatus = 
                OCIDefineByPos( poSession->hStatement, &hDefn, poSession->hError,
                                iParm, (ub1 *) apszColValues[iParm-1], nWidth, 
                                SQLT_STR, NULL, NULL, NULL, OCI_DEFAULT );
            printf( "nStatus (OCIDefineByPos) = %d\n", nStatus );
        }
        else
            apszColValues[iParm-1] = NULL;
    }

    for( ; TRUE; )
    {
        printf( "\n" );

        nStatus = OCIStmtFetch( poSession->hStatement, poSession->hError, 1, 
                                OCI_FETCH_NEXT, OCI_DEFAULT );
        if( nStatus != OCI_SUCCESS )
        {
            poSession->Failed( nStatus, "OCIStmtFetch" );
            if( nStatus == OCI_NO_DATA )
                break;
        }

        for( iParm = 0; iParm < nColCount; iParm++ )
        {
            if( apszColValues[iParm] != NULL )
                printf( "%s = %s\n", 
                        apszColNames[iParm], 
                        apszColValues[iParm] );
        }
    }
}




