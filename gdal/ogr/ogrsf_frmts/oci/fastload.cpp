/******************************************************************************
 * $Id$
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Test mainline for fast loading.
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

#include <stdio.h>
#include "ogr_oci.h"

int main()

{
    OGROCISession oSession;

    if( !oSession.EstablishSession( "warmerda", "LetoKing", 
                                    "gdal800.dreadfest.com" ) )
    {
        exit( 1 );
    }

    printf( "Session established.\n" );

    OGROCIStatement oStatement( &oSession );

    oStatement.Execute( "DROP TABLE fasttest" );
    oStatement.Execute( "CREATE TABLE fasttest (ifld INTEGER, cfld VARCHAR(4000), shape mdsys.sdo_geometry)" );
//    oStatement.Execute( "CREATE TABLE fasttest (ifld INTEGER, cfld VARCHAR(4000))" );

/* -------------------------------------------------------------------- */
/*      Prepare insert statement.                                       */
/* -------------------------------------------------------------------- */
    
    oStatement.Prepare( "INSERT INTO fasttest VALUES "
                        "(:field_1, :field_2, :field_3)" );
//    oStatement.Prepare( "INSERT INTO fasttest VALUES "
//                        "(:field_1, :field_2)" );
    
/* -------------------------------------------------------------------- */
/*      Do a conventional bind.                                         */
/* -------------------------------------------------------------------- */
    int anField1[100];
    char szField2[100*4];
    int anGType[100];
    int anSRID[100];
    OCIArray *aphElemInfos[100];
    OCIArray *aphOrdinates[100];
    SDO_GEOMETRY_TYPE  aoGeometries[100];
    SDO_GEOMETRY_ind   aoGeometryIndicators[100];
    SDO_GEOMETRY_TYPE *apoGeomMap[100];
    SDO_GEOMETRY_ind  *apoGeomIndMap[100];
    double adfX[100], adfY[100];

    memset( aphElemInfos, 0, sizeof(OCIArray*) * 100 );
    memset( aphOrdinates, 0, sizeof(OCIArray*) * 100 );
    memset( aoGeometries, 0, sizeof(SDO_GEOMETRY) * 100 );
    memset( aoGeometryIndicators, 0, sizeof(SDO_GEOMETRY_ind) * 100 );

    if( oStatement.BindScalar( ":field_1", anField1, 
                              sizeof(int), SQLT_INT ) != CE_None )
        exit( 1 );
    
    if( oStatement.BindScalar( ":field_2", szField2, 4, SQLT_STR ) != CE_None )
        exit( 1 );

    if( oStatement.BindObject( ":field_3", apoGeomMap, oSession.hGeometryTDO, 
                               (void**)apoGeomIndMap ) != CE_None )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Create array of arrays for elem_info and ordinates.             */
/* -------------------------------------------------------------------- */
    int iBindRow;
    for( iBindRow = 0; iBindRow < 100; iBindRow++ )
    {
        if( oSession.Failed(
                OCIObjectNew( oSession.hEnv, oSession.hError, 
                              oSession.hSvcCtx, OCI_TYPECODE_VARRAY,
                              oSession.hElemInfoTDO, (dvoid *)NULL, 
                              OCI_DURATION_SESSION,
                              FALSE, (dvoid **) (aphElemInfos + iBindRow)),
                "OCIObjectNew()") )
            exit( 1 );

        if( oSession.Failed(
                OCIObjectNew( oSession.hEnv, oSession.hError, 
                              oSession.hSvcCtx, OCI_TYPECODE_VARRAY,
                              oSession.hOrdinatesTDO, (dvoid *)NULL, 
                              OCI_DURATION_SESSION,
                              FALSE, (dvoid **) (aphOrdinates + iBindRow)),
                "OCIObjectNew()") )
            exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Populate VARRAYs                                                */
/* -------------------------------------------------------------------- */
    int iRow;

    for( iRow = 0; iRow < 100; iRow++ )
    {
        anField1[iRow] = iRow;                                         
        sprintf( szField2 + iRow*4, "%3d", iRow );
        anGType[iRow] = 3001;
        anSRID[iRow] = -1;
        adfX[iRow] = 100.0 + iRow;
        adfY[iRow] = 100.0 - iRow;

        //---------------------------------------------------------------
        int anElemInfo[3], nElemInfoCount;
        OCINumber oci_number; 
        int i;
        
        nElemInfoCount = 3;
        anElemInfo[0] = 1;
        anElemInfo[1] = 1;
        anElemInfo[2] = 1;

        // Prepare the VARRAY of ordinate values. 
        for (i = 0; i < nElemInfoCount; i++)
        {
            if( oSession.Failed( 
                OCINumberFromInt( oSession.hError, 
                                  (dvoid *) (anElemInfo + i),
                                  (uword)sizeof(int),
                                  OCI_NUMBER_SIGNED,
                                  &oci_number),
                "OCINumberFromInt") )
                exit( 1 );

            if( oSession.Failed( 
                OCICollAppend( oSession.hEnv, oSession.hError,
                               (dvoid *) &oci_number,
                               (dvoid *)0, aphElemInfos[iRow]),
                "OCICollAppend") )
                exit( 1 );
        }

        //---------------------------------------------------------------
        double adfOrdinates[6];
        int    nOrdCount;

        nOrdCount = 3;
        adfOrdinates[0] = iRow + 100;
        adfOrdinates[1] = iRow - 100;
        adfOrdinates[2] = 0.0;
        adfOrdinates[3] = iRow + 100;
        adfOrdinates[4] = iRow - 100;
        adfOrdinates[5] = 0.0;

        // Prepare the VARRAY of ordinate values. 
        for (i = 0; i < nOrdCount; i++)
        {
            if( oSession.Failed( 
                OCINumberFromReal( oSession.hError, 
                                  (dvoid *) (adfOrdinates + i),
                                  (uword)sizeof(double),
                                  &oci_number),
                "OCINumberFromReal") )
                exit( 1 );

            if( oSession.Failed( 
                OCICollAppend( oSession.hEnv, oSession.hError,
                               (dvoid *) &oci_number,
                               (dvoid *)0, aphOrdinates[iRow]),
                "OCICollAppend") )
                exit( 1 );
        }

        // -------------------------------------------------------------
        SDO_GEOMETRY_TYPE *poGeom = aoGeometries + iRow;
        SDO_GEOMETRY_ind  *poInd = aoGeometryIndicators + iRow;

        poInd->sdo_point._atomic = OCI_IND_NULL;

        if( oSession.Failed( 
                OCINumberFromInt( oSession.hError, 
                                  (dvoid *) (anGType + iRow),
                                  (uword)sizeof(int),
                                  OCI_NUMBER_SIGNED,
                                  &(poGeom->sdo_gtype)),
                "OCINumberFromInt" ) )
            exit( 1 );

        if( oSession.Failed( 
                OCINumberFromInt( oSession.hError, 
                                  (dvoid *) (anSRID + iRow),
                                  (uword)sizeof(int),
                                  OCI_NUMBER_SIGNED,
                                  &(poGeom->sdo_srid)),
                "OCINumberFromInt" ) )
            exit( 1 );

        poGeom->sdo_elem_info = aphElemInfos[iRow];
        poGeom->sdo_ordinates = aphOrdinates[iRow];

        apoGeomMap[iRow] = poGeom;
        apoGeomIndMap[iRow] = poInd;
    }

/* -------------------------------------------------------------------- */
/*      Execute the statement.                                          */
/* -------------------------------------------------------------------- */
    int iGroup;

    for( iGroup = 0; iGroup < 2; iGroup++ )
    {
        if( oSession.Failed( 
                OCIStmtExecute( oSession.hSvcCtx, oStatement.GetStatement(), 
                                oSession.hError, (ub4) 100, (ub4)0, 
                                (OCISnapshot *)NULL, (OCISnapshot *)NULL, 
                                (ub4) OCI_COMMIT_ON_SUCCESS ),
                "OCIStmtExecute" ) )
            exit( 1 );
    }

    printf( "Successful completion\n" );
    exit( 0 );
}
