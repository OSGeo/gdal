/******************************************************************************
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
 ****************************************************************************/

#include "ogr_oci.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

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
    SetDescription( poFeatureDefn->GetName() );

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
    for( int iParam = 0; true; iParam++ )
    {
        OGRFieldDefn oField( "", OFTString );
        int          nStatus;
        OCIParam     *hParamDesc;
        ub2          nOCIType;
        ub4          nOCILen;

        nStatus =
            OCIParamGet( poCommand->GetStatement(), OCI_HTYPE_STMT,
                         poSession->hError, (dvoid**)&hParamDesc,
                         (ub4) iParam+1 );

        if( nStatus == OCI_ERROR )
            break;

        if( poSession->GetParamInfo( hParamDesc, &oField, &nOCIType, &nOCILen )
            != CE_None )
            break;

        if( oField.GetType() == OFTBinary && nOCIType == 108 )
        {
            CPLFree( pszGeomName );
            pszGeomName = CPLStrdup( oField.GetNameRef() );
            iGeomColumn = iParam;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Use the schema off the statement.                               */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn;

    poDefn = poCommand->GetResultDefn();
    if( iGeomColumn >= 0 )
        poDefn->SetGeomType(wkbUnknown);
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

    if( EQUAL(pszExpectedFIDName, "OGR_FID") && pszFIDName )
    {
        for(int i=0;i<poDefn->GetFieldCount();i++)
        {
            // This is presumably a Integer since we always create Integer64 with a
            // defined precision
            if( poDefn->GetFieldDefn(i)->GetType() == OFTInteger64 &&
                poDefn->GetFieldDefn(i)->GetWidth() == 0 )
            {
                poDefn->GetFieldDefn(i)->SetType(OFTInteger);
            }
        }
    }

    return poDefn;
}
