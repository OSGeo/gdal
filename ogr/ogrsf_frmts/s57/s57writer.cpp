/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57Writer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
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
 * Revision 1.1  2003/09/09 16:47:06  warmerda
 * New
 *
 */

#include "s57.h"
#include "ogr_api.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             S57Writer()                              */
/************************************************************************/

S57Writer::S57Writer()

{
    poModule = NULL;
    poRegistrar = NULL;

    nCOMF = 10000000;
    nSOMF = 10;
}

/************************************************************************/
/*                             ~S57Writer()                             */
/************************************************************************/

S57Writer::~S57Writer()

{
    Close();
    if( poRegistrar != NULL )
        delete poRegistrar;
}

/************************************************************************/
/*                               Close()                                */
/*                                                                      */
/*      Close the current S-57 dataset.                                 */
/************************************************************************/

int S57Writer::Close()

{
    if( poModule != NULL )
    {
        poModule->Close();
        delete poModule;
        poModule = NULL;
    }
    return TRUE;
}

/************************************************************************/
/*                           CreateS57File()                            */
/*                                                                      */
/*      Create a new output ISO8211 file with all the S-57 data         */
/*      definitions.                                                    */
/************************************************************************/

int S57Writer::CreateS57File( const char *pszFilename )

{
    DDFModule  oModule;
    DDFFieldDefn *poFDefn;

    Close();

    nNext0001Index = 1;

/* -------------------------------------------------------------------- */
/*      Create and initialize new module.                               */
/* -------------------------------------------------------------------- */
    poModule = new DDFModule();
    poModule->Initialize();

/* -------------------------------------------------------------------- */
/*      Create the '0000' definition.                                   */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0000", "", "0001DSIDDSIDDSSI0001DSPM0001VRIDVRIDATTVVRIDVRPCVRIDVRPTVRIDSGCCVRIDSG2DVRIDSG3D0001FRIDFRIDFOIDFRIDATTFFRIDNATFFRIDFFPCFRIDFFPTFRIDFSPCFRIDFSPT",
                     DDFFieldDefn::elementary, 
                     DDFFieldDefn::char_string );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the '0001' definition.                                   */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0001", "ISO 8211 Record Identifier", "", 
                     DDFFieldDefn::elementary, DDFFieldDefn::bit_string,
                     "(b12)" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the DSID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "DSID", "Data set identification field", "",
                     DDFFieldDefn::vector, DDFFieldDefn::mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "EXPP", "b11" );
    poFDefn->AddSubfield( "INTU", "b11" );
    poFDefn->AddSubfield( "DSNM", "A" );
    poFDefn->AddSubfield( "EDTN", "A" );
    poFDefn->AddSubfield( "UPDN", "A" );
    poFDefn->AddSubfield( "UADT", "A(8)" );
    poFDefn->AddSubfield( "ISDT", "A(8)" );
    poFDefn->AddSubfield( "STED", "R(4)" );
    poFDefn->AddSubfield( "PRSP", "b11" );
    poFDefn->AddSubfield( "PSDN", "A" );
    poFDefn->AddSubfield( "PRED", "A" );
    poFDefn->AddSubfield( "PROF", "b11" );
    poFDefn->AddSubfield( "AGEN", "b12" );
    poFDefn->AddSubfield( "COMT", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the DSSI field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "DSSI", "Data set structure information field", "",
                     DDFFieldDefn::vector, DDFFieldDefn::mixed_data_type );

    poFDefn->AddSubfield( "DSTR", "b11" );
    poFDefn->AddSubfield( "AALL", "b11" );
    poFDefn->AddSubfield( "NALL", "b11" );
    poFDefn->AddSubfield( "NOMR", "b14" );
    poFDefn->AddSubfield( "NOCR", "b14" );
    poFDefn->AddSubfield( "NOGR", "b14" );
    poFDefn->AddSubfield( "NOLR", "b14" );
    poFDefn->AddSubfield( "NOIN", "b14" );
    poFDefn->AddSubfield( "NOCN", "b14" );
    poFDefn->AddSubfield( "NOED", "b14" );
    poFDefn->AddSubfield( "NOFA", "b14" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the DSPM field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "DSPM", "Data set parameter field", "",
                     DDFFieldDefn::vector, DDFFieldDefn::mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "HDAT", "b11" );
    poFDefn->AddSubfield( "VDAT", "b11" );
    poFDefn->AddSubfield( "SDAT", "b11" );
    poFDefn->AddSubfield( "CSCL", "b14" );
    poFDefn->AddSubfield( "DUNI", "b11" );
    poFDefn->AddSubfield( "HUNI", "b11" );
    poFDefn->AddSubfield( "PUNI", "b11" );
    poFDefn->AddSubfield( "COUN", "b11" );
    poFDefn->AddSubfield( "COMF", "b14" );
    poFDefn->AddSubfield( "SOMF", "b14" );
    poFDefn->AddSubfield( "COMT", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the VRID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "VRID", "Vector record identifier field", "",
                     DDFFieldDefn::vector, DDFFieldDefn::mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "RVER", "b12" );
    poFDefn->AddSubfield( "RUIN", "b11" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the ATTV field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "ATTV", "Vector record attribute field", "",
                     DDFFieldDefn::vector, DDFFieldDefn::mixed_data_type );

    /* how do I mark this as repeating? */
    poFDefn->AddSubfield( "ATTL", "b12" );
    poFDefn->AddSubfield( "ATVL", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SG2D field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SG2D", "2-D coordinate field", "*",
                     DDFFieldDefn::vector, DDFFieldDefn::mixed_data_type );

    /* how do I mark this as repeating? */
    poFDefn->AddSubfield( "YCOO", "b24" );
    poFDefn->AddSubfield( "XCOO", "b24" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SG3D field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SG3D", "3-D coordinate (sounding array) field", "*",
                     DDFFieldDefn::vector, DDFFieldDefn::mixed_data_type );

    /* how do I mark this as repeating? */
    poFDefn->AddSubfield( "YCOO", "b24" );
    poFDefn->AddSubfield( "XCOO", "b24" );
    poFDefn->AddSubfield( "VE3D", "b24" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */

// need to add: VRPC, VRPT, SGCC, FRID, FOID, ATTF, NATF, FFPC, 
// FFPT, FSPC, and FSPT

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    if( !poModule->Create( pszFilename ) )
    {
        delete poModule;
        poModule = NULL;
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                             WriteDSID()                              */
/************************************************************************/

int S57Writer::WriteDSID( const char *pszDSNM, const char *pszISDT, 
                          const char *pszSTED, int nAGEN, 
                          const char *pszCOMT )

{
/* -------------------------------------------------------------------- */
/*      Default values.                                                 */
/* -------------------------------------------------------------------- */
    if( pszDSNM == NULL )
        pszDSNM = "";
    if( pszISDT == NULL )
        pszISDT = "20030801";
    if( pszSTED == NULL )
        pszSTED = "03.1";
    if( pszCOMT == NULL )
        pszCOMT = "";

/* -------------------------------------------------------------------- */
/*      Add the DSID field.                                             */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = MakeRecord();
    DDFField *poField;

    poField = poRec->AddField( poModule->FindFieldDefn( "DSID" ) );

    poRec->SetIntSubfield   ( "DSID", 0, "RCNM", 0, 10 );
    poRec->SetIntSubfield   ( "DSID", 0, "RCID", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "EXPP", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "INTU", 0, 4 );
    poRec->SetStringSubfield( "DSID", 0, "DSNM", 0, pszDSNM );
    poRec->SetStringSubfield( "DSID", 0, "EDTN", 0, "2" );
    poRec->SetStringSubfield( "DSID", 0, "UPDN", 0, "0" );
    poRec->SetStringSubfield( "DSID", 0, "UADT", 0, pszISDT );
    poRec->SetStringSubfield( "DSID", 0, "ISDT", 0, pszISDT );
    poRec->SetStringSubfield( "DSID", 0, "STED", 0, pszSTED );
    poRec->SetIntSubfield   ( "DSID", 0, "PRSP", 0, 1 );
    poRec->SetStringSubfield( "DSID", 0, "PSDN", 0, "" );
    poRec->SetStringSubfield( "DSID", 0, "PRED", 0, "2.0" );
    poRec->SetIntSubfield   ( "DSID", 0, "PROF", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "AGEN", 0, nAGEN );
    poRec->SetStringSubfield( "DSID", 0, "COMT", 0, pszCOMT );

/* -------------------------------------------------------------------- */
/*      Add the DSSI record.  Eventually we will need to return and     */
/*      correct these when we are finished writing.                     */
/* -------------------------------------------------------------------- */
    poField = poRec->AddField( poModule->FindFieldDefn( "DSSI" ) );

    poRec->SetIntSubfield   ( "DSSI", 0, "DSTR", 0, 2 );
    poRec->SetIntSubfield   ( "DSSI", 0, "AALL", 0, 1 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NALL", 0, 1 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOMR", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOCR", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOGR", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOLR", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOIN", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOCN", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOED", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOFA", 0, 0 );

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return TRUE;
}

/************************************************************************/
/*                             WriteDSPM()                              */
/************************************************************************/

int S57Writer::WriteDSPM( int nScale )

{
    if( nScale == 0 )
        nScale = 52000;

/* -------------------------------------------------------------------- */
/*      Add the DSID field.                                             */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = MakeRecord();
    DDFField *poField;

    poField = poRec->AddField( poModule->FindFieldDefn( "DSPM" ) );

    poRec->SetIntSubfield   ( "DSPM", 0, "RCNM", 0, 20 );
    poRec->SetIntSubfield   ( "DSPM", 0, "RCID", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "HDAT", 0, 2 );
    poRec->SetIntSubfield   ( "DSPM", 0, "VDAT", 0, 17 );
    poRec->SetIntSubfield   ( "DSPM", 0, "SDAT", 0, 23 );
    poRec->SetIntSubfield   ( "DSPM", 0, "CSCL", 0, nScale );
    poRec->SetIntSubfield   ( "DSPM", 0, "DUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "HUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "PUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "COUN", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "COMF", 0, nCOMF );
    poRec->SetIntSubfield   ( "DSPM", 0, "SOMF", 0, nSOMF );

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return TRUE;
}

/************************************************************************/
/*                             MakeRecord()                             */
/*                                                                      */
/*      Create a new empty record, and append a 0001 field with a       */
/*      properly set record index in it.                                */
/************************************************************************/

DDFRecord *S57Writer::MakeRecord()

{
    DDFRecord *poRec = new DDFRecord( poModule );
    DDFField *poField;
    unsigned char abyData[3];

    abyData[0] = nNext0001Index % 256;
    abyData[1] = nNext0001Index / 256; 
    abyData[2] = 0x1e;

    poField = poRec->AddField( poModule->FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, (const char *) abyData, 3 );

    nNext0001Index++;

    return poRec;
}

/************************************************************************/
/*                           WriteGeometry()                            */
/************************************************************************/

int S57Writer::WriteGeometry( DDFRecord *poRec, int nVertCount, 
                              double *padfX, double *padfY, double *padfZ )

{
    const char *pszFieldName = "SG2D";
    DDFField *poField;
    int nRawDataSize, i, nSuccess;
    unsigned char *pabyRawData;

    if( padfZ != NULL )
        pszFieldName = "SG3D";

    poField = poRec->AddField( poModule->FindFieldDefn( pszFieldName ) );

    if( padfZ )
        nRawDataSize = 12 * nVertCount;
    else
        nRawDataSize = 8 * nVertCount;

    pabyRawData = (unsigned char *) CPLMalloc(nRawDataSize);

    for( i = 0; i < nRawDataSize; i++ )
    {
        GInt32 nXCOO, nYCOO, nVE3D;

        nXCOO = (GInt32) floor(padfX[i] * nCOMF + 0.5);
        nYCOO = (GInt32) floor(padfY[i] * nCOMF + 0.5);
        
        if( padfZ == NULL )
        {
            memcpy( pabyRawData + i * 8, &nYCOO, 4 );
            memcpy( pabyRawData + i * 8 + 4, &nYCOO, 4 );
        }
        else
        {
            nVE3D = (GInt32) floor( padfZ[i] * nSOMF + 0.5 );
            memcpy( pabyRawData + i * 12, &nYCOO, 4 );
            memcpy( pabyRawData + i * 12 + 4, &nYCOO, 4 );
            memcpy( pabyRawData + i * 12 + 8, &nVE3D, 4 );
        }
    }

    nSuccess = poRec->SetFieldRaw( poField, 0, 
                                   (const char *) pabyRawData, nRawDataSize );

    CPLFree( pabyRawData );

    return nSuccess;
}

/************************************************************************/
/*                           WritePrimitive()                           */
/************************************************************************/

int S57Writer::WritePrimitive( OGRFeature *poFeature )

{
    DDFRecord *poRec = MakeRecord();
    DDFField *poField;
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

/* -------------------------------------------------------------------- */
/*      Add the VRID field.                                             */
/* -------------------------------------------------------------------- */

    poField = poRec->AddField( poModule->FindFieldDefn( "VRID" ) );

    poRec->SetIntSubfield   ( "VRID", 0, "RCNM", 0, 
                              poFeature->GetFieldAsInteger( "RCNM") );
    poRec->SetIntSubfield   ( "VRID", 0, "RCID", 0, 
                              poFeature->GetFieldAsInteger( "RCID") );
    poRec->SetIntSubfield   ( "VRID", 0, "RVER", 0, 1 );
    poRec->SetIntSubfield   ( "VRID", 0, "RUIN", 0, 1 );

/* -------------------------------------------------------------------- */
/*      Handle simple point.                                            */
/* -------------------------------------------------------------------- */
    if( poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        double dfX, dfY, dfZ;
        OGRPoint *poPoint = (OGRPoint *) poGeom;

        CPLAssert( poFeature->GetFieldAsInteger( "RCNM") == RCNM_VI 
                   || poFeature->GetFieldAsInteger( "RCNM") == RCNM_VC ); 

        dfX = poPoint->getX();
        dfY = poPoint->getY();
        dfZ = poPoint->getZ();
        
        if( dfZ == 0.0 )
            WriteGeometry( poRec, 1, &dfX, &dfY, NULL );
        else
            WriteGeometry( poRec, 1, &dfX, &dfY, &dfZ );
    }

/* -------------------------------------------------------------------- */
/*      For multipoints we assuming SOUNDG, and write out as SG3D.      */
/* -------------------------------------------------------------------- */
    else if( poGeom != NULL 
             && wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint )
    {
        OGRMultiPoint *poMP = (OGRMultiPoint *) poGeom;
        int i, nVCount = poMP->getNumGeometries();
        double *padfX, *padfY, *padfZ;

        CPLAssert( poFeature->GetFieldAsInteger( "RCNM") == RCNM_VI 
                   || poFeature->GetFieldAsInteger( "RCNM") == RCNM_VC ); 

        padfX = (double *) CPLMalloc(sizeof(double) * nVCount);
        padfY = (double *) CPLMalloc(sizeof(double) * nVCount);
        padfZ = (double *) CPLMalloc(sizeof(double) * nVCount);

        for( i = 0; i < nVCount; i++ )
        {
            OGRPoint *poPoint = (OGRPoint *) poMP->getGeometryRef( i );
            padfX[i] = poPoint->getX();
            padfY[i] = poPoint->getY();
            padfZ[i] = poPoint->getZ();
        }

        WriteGeometry( poRec, nVCount, padfX, padfY, padfZ );

        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );
    }

/* -------------------------------------------------------------------- */
/*      Handle LINESTRINGs (edges).                                     */
/* -------------------------------------------------------------------- */
    else if( poGeom != NULL 
             && wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        OGRLineString *poLS = (OGRLineString *) poGeom;
        int i, nVCount = poLS->getNumPoints();
        double *padfX, *padfY;

        CPLAssert( poFeature->GetFieldAsInteger( "RCNM") == RCNM_VE );

        poField = poRec->AddField( poModule->FindFieldDefn( "SG2D" ) );
        
        padfX = (double *) CPLMalloc(sizeof(double) * nVCount);
        padfY = (double *) CPLMalloc(sizeof(double) * nVCount);

        for( i = 0; i < nVCount; i++ )
        {
            padfX[i] = poLS->getX(i);
            padfY[i] = poLS->getY(i);
        }

        WriteGeometry( poRec, nVCount, padfX, padfY, NULL );

        CPLFree( padfX );
        CPLFree( padfY );
    }

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return TRUE;
}

/************************************************************************/
/*                        WriteCompleteFeature()                        */
/************************************************************************/

int S57Writer::WriteCompleteFeature( OGRFeature *poFeature )

{
    OGRFeatureDefn *poFDefn = poFeature->GetDefnRef();

    if( EQUAL(poFDefn->GetName(),OGRN_VI) 
        || EQUAL(poFDefn->GetName(),OGRN_VC) 
        || EQUAL(poFDefn->GetName(),OGRN_VE) )
        return WritePrimitive( poFeature );

    return FALSE;
}
