/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCISelectLayer class.  This class 
 *           provides read semantics on the result of a SELECT statement.
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
 * Revision 1.5  2005/08/09 21:02:06  hobu
 * Updated ReadTableDefinition to use the OCI_FID
 * variable from the environment if it is available
 *
 * Revision 1.4  2003/05/21 03:54:01  warmerda
 * expand tabs
 *
 * Revision 1.3  2003/01/06 17:59:07  warmerda
 * initialize FID field
 *
 * Revision 1.2  2002/12/28 04:38:36  warmerda
 * converted to unix file conventions
 *
 * Revision 1.1  2002/12/28 04:07:27  warmerda
 * New
 *
 */

#include "ogr_oci.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGROCISelectLayer()                         */
/************************************************************************/

OGROCISelectLayer::OGROCISelectLayer( OGROCIDataSource *poDSIn, 
                                      const char * pszQuery,
                                      OGROCIStatement *poDescribedCommand )

{
    poDS = poDSIn;

    iNextShapeId = 0;

    poFeatureDefn = ReadTableDefinition( poDescribedCommand );

    pszQueryStatement = CPLStrdup(pszQuery);
    
    ResetReading();
}

/************************************************************************/
/*                         ~OGROCISelectLayer()                          */
/************************************************************************/

OGROCISelectLayer::~OGROCISelectLayer()

{
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build layer definition from the described information about     */
/*      the command.                                                    */
/************************************************************************/

OGRFeatureDefn *
OGROCISelectLayer::ReadTableDefinition( OGROCIStatement *poCommand )

{
    OGROCISession      *poSession = poDS->GetSession();

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    for( int iParm = 0; TRUE; iParm++ )
    {                                                           
        OGRFieldDefn oField( "", OFTString );
        int          nStatus;
        OCIParam     *hParmDesc;
        ub2          nOCIType;
        ub4          nOCILen;

        nStatus = 
            OCIParamGet( poCommand->GetStatement(), OCI_HTYPE_STMT, 
                         poSession->hError, (dvoid**)&hParmDesc, 
                         (ub4) iParm+1 );

        if( nStatus == OCI_ERROR )
            break;

        if( poSession->GetParmInfo( hParmDesc, &oField, &nOCIType, &nOCILen )
            != CE_None )
            break;

        if( oField.GetType() == OFTBinary && nOCIType == 108 )
        {
            CPLFree( pszGeomName );
            pszGeomName = CPLStrdup( oField.GetNameRef() );
            iGeomColumn = iParm;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Use the schema off the statement.                               */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn;

    poDefn = poCommand->GetResultDefn();
    poDefn->Reference();

/* -------------------------------------------------------------------- */
/*      Do we have an FID?                                              */
/* -------------------------------------------------------------------- */
    const char *pszExpectedFIDName = 
        CPLGetConfigOption( "OCI_FID", "OGR_FID" );
    if( poDefn->GetFieldIndex(pszExpectedFIDName) > -1 )
    {
        iFIDColumn = poDefn->GetFieldIndex(pszExpectedFIDName);
        pszFIDName = CPLStrdup(poDefn->GetFieldDefn(iFIDColumn)->GetNameRef());
    }

    return poDefn;
}
