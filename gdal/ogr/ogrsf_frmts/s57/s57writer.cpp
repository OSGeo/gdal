/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57Writer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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
    poClassContentExplorer = NULL;

    nCOMF = 10000000;
    nSOMF = 10;
}

/************************************************************************/
/*                             ~S57Writer()                             */
/************************************************************************/

S57Writer::~S57Writer()

{
    Close();
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
                     dsc_elementary, 
                     dtc_char_string );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the '0001' definition.                                   */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0001", "ISO 8211 Record Identifier", "", 
                     dsc_elementary, dtc_bit_string,
                     "(b12)" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the DSID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "DSID", "Data set identification field", "",
                     dsc_vector, dtc_mixed_data_type );

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
                     dsc_vector, dtc_mixed_data_type );

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
                     dsc_vector, dtc_mixed_data_type );

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
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "RVER", "b12" );
    poFDefn->AddSubfield( "RUIN", "b11" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the VRPC field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "VRPC", "Vector Record Pointer Control field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "VPUI", "b11" );
    poFDefn->AddSubfield( "VPIX", "b12" );
    poFDefn->AddSubfield( "NVPT", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the VRPT field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "VRPT", "Vector record pointer field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "NAME", "B(40)" );
    poFDefn->AddSubfield( "ORNT", "b11" );
    poFDefn->AddSubfield( "USAG", "b11" );
    poFDefn->AddSubfield( "TOPI", "b11" );
    poFDefn->AddSubfield( "MASK", "b11" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the ATTV field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "ATTV", "Vector record attribute field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "ATTL", "b12" );
    poFDefn->AddSubfield( "ATVL", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SGCC field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SGCC", "Coordinate Control Field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "CCUI", "b11" );
    poFDefn->AddSubfield( "CCIX", "b12" );
    poFDefn->AddSubfield( "CCNC", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SG2D field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SG2D", "2-D coordinate field", "*",
                     dsc_array, dtc_bit_string );

    poFDefn->AddSubfield( "YCOO", "b24" );
    poFDefn->AddSubfield( "XCOO", "b24" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SG3D field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SG3D", "3-D coordinate (sounding array) field", "*",
                     dsc_array, dtc_bit_string );

    poFDefn->AddSubfield( "YCOO", "b24" );
    poFDefn->AddSubfield( "XCOO", "b24" );
    poFDefn->AddSubfield( "VE3D", "b24" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FRID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FRID", "Feature record identifier field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "b11" );
    poFDefn->AddSubfield( "RCID", "b14" );
    poFDefn->AddSubfield( "PRIM", "b11" );
    poFDefn->AddSubfield( "GRUP", "b11" );
    poFDefn->AddSubfield( "OBJL", "b12" );
    poFDefn->AddSubfield( "RVER", "b12" );
    poFDefn->AddSubfield( "RUIN", "b11" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FOID field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FOID", "Feature object identifier field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "AGEN", "b12" );
    poFDefn->AddSubfield( "FIDN", "b14" );
    poFDefn->AddSubfield( "FIDS", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the ATTF field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "ATTF", "Feature record attribute field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "ATTL", "b12" );
    poFDefn->AddSubfield( "ATVL", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the NATF field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "NATF", "Feature record national attribute field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "ATTL", "b12" );
    poFDefn->AddSubfield( "ATVL", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FFPC field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FFPC", "Feature record to feature object pointer control field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "FFUI", "b11" );
    poFDefn->AddSubfield( "FFIX", "b12" );
    poFDefn->AddSubfield( "NFPT", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FFPT field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FFPT", "Feature record to feature object pointer field", "*",
                     dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "LNAM", "B(64)" );
    poFDefn->AddSubfield( "RIND", "b11" );
    poFDefn->AddSubfield( "COMT", "A" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FSPC field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FSPC", "Feature record to spatial record pointer control field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "FSUI", "b11" );
    poFDefn->AddSubfield( "FSIX", "b12" );
    poFDefn->AddSubfield( "NSPT", "b12" );

    poModule->AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the FSPT field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "FSPT", "Feature record to spatial record pointer field", 
                     "*", dsc_array, dtc_mixed_data_type );

    poFDefn->AddSubfield( "NAME", "B(40)" );
    poFDefn->AddSubfield( "ORNT", "b11" );
    poFDefn->AddSubfield( "USAG", "b11" );
    poFDefn->AddSubfield( "MASK", "b11" );

    poModule->AddField( poFDefn );

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

int S57Writer::WriteDSID( int nEXPP /*1*/, int nINTU /*4*/,
                          const char *pszDSNM, const char *pszEDTN,
                          const char *pszUPDN, const char *pszUADT,
                          const char *pszISDT, const char *pszSTED,
                          int nAGEN, const char *pszCOMT,
                          int nNOMR, int nNOGR, int nNOLR, int nNOIN,
                          int nNOCN, int nNOED )

{
/* -------------------------------------------------------------------- */
/*      Default values.                                                 */
/* -------------------------------------------------------------------- */
    if( pszDSNM == NULL )
        pszDSNM = "";
    if( pszEDTN == NULL )
        pszEDTN = "2";
    if( pszUPDN == NULL )
        pszUPDN = "0";
    if( pszISDT == NULL )
        pszISDT = "20030801";
    if( pszUADT == NULL )
        pszUADT = pszISDT;
    if( pszSTED == NULL )
        pszSTED = "03.1";
    if( pszCOMT == NULL )
        pszCOMT = "";

/* -------------------------------------------------------------------- */
/*      Add the DSID field.                                             */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = MakeRecord();
    /* DDFField *poField; */

    /* poField = */ poRec->AddField( poModule->FindFieldDefn( "DSID" ) );

    poRec->SetIntSubfield   ( "DSID", 0, "RCNM", 0, 10 );
    poRec->SetIntSubfield   ( "DSID", 0, "RCID", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "EXPP", 0, nEXPP );
    poRec->SetIntSubfield   ( "DSID", 0, "INTU", 0, nINTU );
    poRec->SetStringSubfield( "DSID", 0, "DSNM", 0, pszDSNM );
    poRec->SetStringSubfield( "DSID", 0, "EDTN", 0, pszEDTN );
    poRec->SetStringSubfield( "DSID", 0, "UPDN", 0, pszUPDN );
    poRec->SetStringSubfield( "DSID", 0, "UADT", 0, pszUADT );
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
    /* poField = */ poRec->AddField( poModule->FindFieldDefn( "DSSI" ) );

    poRec->SetIntSubfield   ( "DSSI", 0, "DSTR", 0, 2 ); // "Chain node"
    poRec->SetIntSubfield   ( "DSSI", 0, "AALL", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NALL", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOMR", 0, nNOMR ); // Meta records
    poRec->SetIntSubfield   ( "DSSI", 0, "NOCR", 0, 0 ); // Cartographic records are not permitted in ENC
    poRec->SetIntSubfield   ( "DSSI", 0, "NOGR", 0, nNOGR ); // Geo records
    poRec->SetIntSubfield   ( "DSSI", 0, "NOLR", 0, nNOLR ); // Collection records
    poRec->SetIntSubfield   ( "DSSI", 0, "NOIN", 0, nNOIN ); // Isolated node records
    poRec->SetIntSubfield   ( "DSSI", 0, "NOCN", 0, nNOCN ); // Connected node records
    poRec->SetIntSubfield   ( "DSSI", 0, "NOED", 0, nNOED ); // Edge records
    poRec->SetIntSubfield   ( "DSSI", 0, "NOFA", 0, 0 ); // Face are not permitted in chain node structure

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

int S57Writer::WriteDSPM( int nHDAT, int nVDAT, int nSDAT, int nCSCL )

{
    if( nHDAT == 0 )
        nHDAT = 2;
    if( nVDAT == 0 )
        nVDAT = 17;
    if( nSDAT == 0 )
        nSDAT = 23;
    if( nCSCL == 0 )
        nCSCL = 52000;

/* -------------------------------------------------------------------- */
/*      Add the DSID field.                                             */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = MakeRecord();
    /* DDFField *poField; */

    /* poField = */ poRec->AddField( poModule->FindFieldDefn( "DSPM" ) );

    poRec->SetIntSubfield   ( "DSPM", 0, "RCNM", 0, 20 );
    poRec->SetIntSubfield   ( "DSPM", 0, "RCID", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "HDAT", 0, nHDAT ); // Must be 2 for ENC
    poRec->SetIntSubfield   ( "DSPM", 0, "VDAT", 0, nVDAT );
    poRec->SetIntSubfield   ( "DSPM", 0, "SDAT", 0, nSDAT );
    poRec->SetIntSubfield   ( "DSPM", 0, "CSCL", 0, nCSCL );
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
    unsigned char abyData[2];

    abyData[0] = nNext0001Index % 256;
    abyData[1] = (unsigned char) (nNext0001Index / 256); 

    poField = poRec->AddField( poModule->FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, (const char *) abyData, 2 );

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

    for( i = 0; i < nVertCount; i++ )
    {
        GInt32 nXCOO, nYCOO, nVE3D;

        nXCOO = CPL_LSBWORD32((GInt32) floor(padfX[i] * nCOMF + 0.5));
        nYCOO = CPL_LSBWORD32((GInt32) floor(padfY[i] * nCOMF + 0.5));
        
        if( padfZ == NULL )
        {
            memcpy( pabyRawData + i * 8, &nYCOO, 4 );
            memcpy( pabyRawData + i * 8 + 4, &nXCOO, 4 );
        }
        else
        {
            nVE3D = CPL_LSBWORD32((GInt32) floor( padfZ[i] * nSOMF + 0.5 ));
            memcpy( pabyRawData + i * 12, &nYCOO, 4 );
            memcpy( pabyRawData + i * 12 + 4, &nXCOO, 4 );
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
    /* DDFField *poField; */
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

/* -------------------------------------------------------------------- */
/*      Add the VRID field.                                             */
/* -------------------------------------------------------------------- */

    /* poField = */ poRec->AddField( poModule->FindFieldDefn( "VRID" ) );

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
/*      Handle LINESTRINGs (edge) geometry.                             */
/* -------------------------------------------------------------------- */
    else if( poGeom != NULL 
             && wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        OGRLineString *poLS = (OGRLineString *) poGeom;
        int i, nVCount = poLS->getNumPoints();
        double *padfX, *padfY;

        CPLAssert( poFeature->GetFieldAsInteger( "RCNM") == RCNM_VE );

        padfX = (double *) CPLMalloc(sizeof(double) * nVCount);
        padfY = (double *) CPLMalloc(sizeof(double) * nVCount);

        for( i = 0; i < nVCount; i++ )
        {
            padfX[i] = poLS->getX(i);
            padfY[i] = poLS->getY(i);
        }

        if (nVCount)
            WriteGeometry( poRec, nVCount, padfX, padfY, NULL );

        CPLFree( padfX );
        CPLFree( padfY );

    }

/* -------------------------------------------------------------------- */
/*      edge node linkages.                                             */
/* -------------------------------------------------------------------- */
    if( poFeature->GetDefnRef()->GetFieldIndex( "NAME_RCNM_0" ) >= 0 )
    {
        /* DDFField *poField; */
        char     szName[5];
        int      nRCID;

        CPLAssert( poFeature->GetFieldAsInteger( "NAME_RCNM_0") == RCNM_VC );

        /* poField = */ poRec->AddField( poModule->FindFieldDefn( "VRPT" ) );

        nRCID = poFeature->GetFieldAsInteger( "NAME_RCID_0");
        szName[0] = RCNM_VC;
        szName[1] = nRCID & 0xff;
        szName[2] = (char) ((nRCID & 0xff00) >> 8);
        szName[3] = (char) ((nRCID & 0xff0000) >> 16);
        szName[4] = (char) ((nRCID & 0xff000000) >> 24);
        
        poRec->SetStringSubfield( "VRPT", 0, "NAME", 0, szName, 5 );
        poRec->SetIntSubfield   ( "VRPT", 0, "ORNT", 0, 
                                  poFeature->GetFieldAsInteger( "ORNT_0") );
        poRec->SetIntSubfield   ( "VRPT", 0, "USAG", 0, 
                                  poFeature->GetFieldAsInteger( "USAG_0") );
        poRec->SetIntSubfield   ( "VRPT", 0, "TOPI", 0, 
                                  poFeature->GetFieldAsInteger( "TOPI_0") );
        poRec->SetIntSubfield   ( "VRPT", 0, "MASK", 0, 
                                  poFeature->GetFieldAsInteger( "MASK_0") );
        
        nRCID = poFeature->GetFieldAsInteger( "NAME_RCID_1");
        szName[0] = RCNM_VC;
        szName[1] = nRCID & 0xff;
        szName[2] = (char) ((nRCID & 0xff00) >> 8);
        szName[3] = (char) ((nRCID & 0xff0000) >> 16);
        szName[4] = (char) ((nRCID & 0xff000000) >> 24);

        poRec->SetStringSubfield( "VRPT", 0, "NAME", 1, szName, 5 );
        poRec->SetIntSubfield   ( "VRPT", 0, "ORNT", 1, 
                                  poFeature->GetFieldAsInteger( "ORNT_1") );
        poRec->SetIntSubfield   ( "VRPT", 0, "USAG", 1, 
                                  poFeature->GetFieldAsInteger( "USAG_1") );
        poRec->SetIntSubfield   ( "VRPT", 0, "TOPI", 1, 
                                  poFeature->GetFieldAsInteger( "TOPI_1") );
        poRec->SetIntSubfield   ( "VRPT", 0, "MASK", 1, 
                                  poFeature->GetFieldAsInteger( "MASK_1") );
    }

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return TRUE;
}

/************************************************************************/
/*                             GetHEXChar()                             */
/************************************************************************/

static char GetHEXChar( const char *pszSrcHEXString )

{
    int nResult = 0;

    if( pszSrcHEXString[0] == '\0' || pszSrcHEXString[1] == '\0' )
        return (char) 0;

    if( pszSrcHEXString[0] >= '0' && pszSrcHEXString[0] <= '9' )
        nResult += (pszSrcHEXString[0] - '0') * 16;
    else if( pszSrcHEXString[0] >= 'a' && pszSrcHEXString[0] <= 'f' )
        nResult += (pszSrcHEXString[0] - 'a' + 10) * 16;
    else if( pszSrcHEXString[0] >= 'A' && pszSrcHEXString[0] <= 'F' )
        nResult += (pszSrcHEXString[0] - 'A' + 10) * 16;

    if( pszSrcHEXString[1] >= '0' && pszSrcHEXString[1] <= '9' )
        nResult += pszSrcHEXString[1] - '0';
    else if( pszSrcHEXString[1] >= 'a' && pszSrcHEXString[1] <= 'f' )
        nResult += pszSrcHEXString[1] - 'a' + 10;
    else if( pszSrcHEXString[1] >= 'A' && pszSrcHEXString[1] <= 'F' )
        nResult += pszSrcHEXString[1] - 'A' + 10;

    return (char) nResult;
}

/************************************************************************/
/*                        WriteCompleteFeature()                        */
/************************************************************************/

int S57Writer::WriteCompleteFeature( OGRFeature *poFeature )

{
    OGRFeatureDefn *poFDefn = poFeature->GetDefnRef();

/* -------------------------------------------------------------------- */
/*      We handle primitives in a seperate method.                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(poFDefn->GetName(),OGRN_VI) 
        || EQUAL(poFDefn->GetName(),OGRN_VC) 
        || EQUAL(poFDefn->GetName(),OGRN_VE) )
        return WritePrimitive( poFeature );

/* -------------------------------------------------------------------- */
/*      Create the record.                                              */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = MakeRecord();

/* -------------------------------------------------------------------- */
/*      Add the FRID.                                                   */
/* -------------------------------------------------------------------- */
    DDFField *poField;

    poField = poRec->AddField( poModule->FindFieldDefn( "FRID" ) );

    poRec->SetIntSubfield   ( "FRID", 0, "RCNM", 0, 100 );
    poRec->SetIntSubfield   ( "FRID", 0, "RCID", 0, 
                              poFeature->GetFieldAsInteger( "RCID" ) );
    poRec->SetIntSubfield   ( "FRID", 0, "PRIM", 0, 
                              poFeature->GetFieldAsInteger( "PRIM" ) );
    poRec->SetIntSubfield   ( "FRID", 0, "GRUP", 0, 
                              poFeature->GetFieldAsInteger( "GRUP") );
    poRec->SetIntSubfield   ( "FRID", 0, "OBJL", 0, 
                              poFeature->GetFieldAsInteger( "OBJL") );
    poRec->SetIntSubfield   ( "FRID", 0, "RVER", 0, 1 ); /* always new insert*/
    poRec->SetIntSubfield   ( "FRID", 0, "RUIN", 0, 1 );
        
/* -------------------------------------------------------------------- */
/*      Add the FOID                                                    */
/* -------------------------------------------------------------------- */
    poField = poRec->AddField( poModule->FindFieldDefn( "FOID" ) );

    poRec->SetIntSubfield   ( "FOID", 0, "AGEN", 0, 
                              poFeature->GetFieldAsInteger( "AGEN") );
    poRec->SetIntSubfield   ( "FOID", 0, "FIDN", 0, 
                              poFeature->GetFieldAsInteger( "FIDN") );
    poRec->SetIntSubfield   ( "FOID", 0, "FIDS", 0, 
                              poFeature->GetFieldAsInteger( "FIDS") );

/* -------------------------------------------------------------------- */
/*      ATTF support.                                                   */
/* -------------------------------------------------------------------- */
    
    if( poRegistrar != NULL 
        && poClassContentExplorer->SelectClass( poFeature->GetDefnRef()->GetName() )
        && !WriteATTF( poRec, poFeature ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Add the FSPT if needed.                                         */
/* -------------------------------------------------------------------- */
    if( poFeature->IsFieldSet( poFeature->GetFieldIndex("NAME_RCNM") ) )
    {
        int nItemCount, i;
        const int *panRCNM, *panRCID, *panORNT, *panUSAG, *panMASK;
        unsigned char *pabyRawData;
        int nRawDataSize;

        panRCNM = poFeature->GetFieldAsIntegerList( "NAME_RCNM", &nItemCount );
        panRCID = poFeature->GetFieldAsIntegerList( "NAME_RCID", &nItemCount );
        panORNT = poFeature->GetFieldAsIntegerList( "ORNT", &nItemCount );
        panUSAG = poFeature->GetFieldAsIntegerList( "USAG", &nItemCount );
        panMASK = poFeature->GetFieldAsIntegerList( "MASK", &nItemCount );

        CPLAssert( sizeof(int) == sizeof(GInt32) );

        nRawDataSize = nItemCount * 8;
        pabyRawData = (unsigned char *) CPLMalloc(nRawDataSize);

        for( i = 0; i < nItemCount; i++ )
        {
            GInt32 nRCID = CPL_LSBWORD32(panRCID[i]);

            pabyRawData[i*8 + 0] = (GByte) panRCNM[i];
            memcpy( pabyRawData + i*8 + 1, &nRCID, 4 );
            pabyRawData[i*8 + 5] = (GByte) panORNT[i];
            pabyRawData[i*8 + 6] = (GByte) panUSAG[i];
            pabyRawData[i*8 + 7] = (GByte) panMASK[i];
        }

        poField = poRec->AddField( poModule->FindFieldDefn( "FSPT" ) );
        poRec->SetFieldRaw( poField, 0, 
                            (const char *) pabyRawData, nRawDataSize );
        CPLFree( pabyRawData );
    }

/* -------------------------------------------------------------------- */
/*      Add the FFPT if needed.                                         */
/* -------------------------------------------------------------------- */
    char **papszLNAM_REFS = poFeature->GetFieldAsStringList( "LNAM_REFS" );

    if( CSLCount(papszLNAM_REFS) > 0 )
    {
        int i, nRefCount = CSLCount(papszLNAM_REFS);
        const int *panRIND = 
            poFeature->GetFieldAsIntegerList( "FFPT_RIND", NULL );

        poRec->AddField( poModule->FindFieldDefn( "FFPT" ) );

        for( i = 0; i < nRefCount; i++ )
        {
            char szLNAM[9];

            if( strlen(papszLNAM_REFS[i]) < 16 )
                continue;

            // AGEN
            szLNAM[1] = GetHEXChar( papszLNAM_REFS[i] + 0 );
            szLNAM[0] = GetHEXChar( papszLNAM_REFS[i] + 2 );
            
            // FIDN
            szLNAM[5] = GetHEXChar( papszLNAM_REFS[i] + 4 );
            szLNAM[4] = GetHEXChar( papszLNAM_REFS[i] + 6 );
            szLNAM[3] = GetHEXChar( papszLNAM_REFS[i] + 8 );
            szLNAM[2] = GetHEXChar( papszLNAM_REFS[i] + 10 );

            // FIDS
            szLNAM[7] = GetHEXChar( papszLNAM_REFS[i] + 12 );
            szLNAM[6] = GetHEXChar( papszLNAM_REFS[i] + 14 );

            szLNAM[8] = '\0';

            poRec->SetStringSubfield( "FFPT", 0, "LNAM", i, 
                                      (char *) szLNAM, 8 );
            poRec->SetIntSubfield( "FFPT", 0, "RIND", i, 
                                   panRIND[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write out the record.                                           */
/* -------------------------------------------------------------------- */
    poRec->Write();
    delete poRec;

    return TRUE;
}

/************************************************************************/
/*                           SetClassBased()                            */
/************************************************************************/

void S57Writer::SetClassBased( S57ClassRegistrar * poReg,
                               S57ClassContentExplorer* poClassContentExplorerIn )

{
    poRegistrar = poReg;
    poClassContentExplorer = poClassContentExplorerIn;
}

/************************************************************************/
/*                             WriteATTF()                              */
/************************************************************************/

int S57Writer::WriteATTF( DDFRecord *poRec, OGRFeature *poFeature )
{
    int nRawSize=0, nACount = 0;
    char achRawData[5000];
    char **papszAttrList; 

    CPLAssert( poRegistrar != NULL );

/* -------------------------------------------------------------------- */
/*      Loop over all attributes.                                       */
/* -------------------------------------------------------------------- */
    papszAttrList = poClassContentExplorer->GetAttributeList(NULL); 
    
    for( int iAttr = 0; papszAttrList[iAttr] != NULL; iAttr++ )
    {
        int iField = poFeature->GetFieldIndex( papszAttrList[iAttr] );
        OGRFieldType eFldType = 
            poFeature->GetDefnRef()->GetFieldDefn(iField)->GetType();
        int nATTLInt;
        GUInt16 nATTL;
        const char *pszATVL;
        
        if( iField < 0 )
            continue;

        if( !poFeature->IsFieldSet( iField ) )
            continue;

        nATTLInt = poRegistrar->FindAttrByAcronym( papszAttrList[iAttr] );
        if( nATTLInt == -1 )
            continue;

        nATTL = (GUInt16)nATTLInt;
        nATTL = CPL_LSBWORD16( nATTL );
        memcpy( achRawData + nRawSize, &nATTL, 2 );
        nRawSize += 2;
        
        pszATVL = poFeature->GetFieldAsString( iField );

        // Special hack to handle special "empty" marker in integer fields.
        if( atoi(pszATVL) == EMPTY_NUMBER_MARKER 
            && (eFldType == OFTInteger || eFldType == OFTReal) )
            pszATVL = "";

        // Watch for really long data.
        if( strlen(pszATVL) + nRawSize + 10 > sizeof(achRawData) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Too much ATTF data for fixed buffer size." );
            return FALSE;
        }

        // copy data into record buffer.
        memcpy( achRawData + nRawSize, pszATVL, strlen(pszATVL) );
        nRawSize += strlen(pszATVL);
        achRawData[nRawSize++] = DDF_UNIT_TERMINATOR;
    
        nACount++;
    }

/* -------------------------------------------------------------------- */
/*      If we got no attributes, return without adding ATTF.            */
/* -------------------------------------------------------------------- */
    if( nACount == 0 )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Write the new field value.                                      */
/* -------------------------------------------------------------------- */
    DDFField *poField;

    poField = poRec->AddField( poModule->FindFieldDefn( "ATTF" ) );

    return poRec->SetFieldRaw( poField, 0, achRawData, nRawSize );
}
