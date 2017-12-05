/* ****************************************************************************
 *
 * Project:  ISO8211 Library
 * Purpose:  Test ISO8211 writing capability.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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

#include "iso8211.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               mk_s57()                               */
/************************************************************************/

static void mk_s57()

{
    DDFModule  oModule;

    oModule.Initialize();

/* -------------------------------------------------------------------- */
/*      Create the '0000' definition.                                   */
/* -------------------------------------------------------------------- */
    DDFFieldDefn *poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0000", "", "0001DSIDDSIDDSSI0001DSPM0001VRIDVRIDATTVVRIDVRPCVRIDVRPTVRIDSGCCVRIDSG2DVRIDSG3D0001FRIDFRIDFOIDFRIDATTFFRIDNATFFRIDFFPCFRIDFFPTFRIDFSPCFRIDFSPT", dsc_elementary, dtc_char_string );

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the '0001' definition.                                   */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0001", "ISO 8211 Record Identifier", "",
                     dsc_elementary, dtc_bit_string,
                     "(b12)" );

    oModule.AddField( poFDefn );

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

    oModule.AddField( poFDefn );

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

    oModule.AddField( poFDefn );

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

    oModule.AddField( poFDefn );

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

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the ATTV field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "ATTV", "Vector record attribute field", "",
                     dsc_vector, dtc_mixed_data_type );

    /* how do I mark this as repeating? */
    poFDefn->AddSubfield( "ATTL", "b12" );
    poFDefn->AddSubfield( "ATVL", "A" );

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SG2D field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SG2D", "2-D coordinate field", "*",
                     dsc_vector, dtc_mixed_data_type );

    /* how do I mark this as repeating? */
    poFDefn->AddSubfield( "YCOO", "b24" );
    poFDefn->AddSubfield( "XCOO", "b24" );

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the SG3D field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "SG3D", "3-D coordinate (sounding array) field", "*",
                     dsc_vector, dtc_mixed_data_type );

    /* how do I mark this as repeating? */
    poFDefn->AddSubfield( "YCOO", "b24" );
    poFDefn->AddSubfield( "XCOO", "b24" );
    poFDefn->AddSubfield( "VE3D", "b24" );

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */

// need to add: VRPC, VRPT, SGCC, FRID, FOID, ATTF, NATF, FFPC,
// FFPT, FSPC, and FSPT

/* -------------------------------------------------------------------- */
/*      Create file.                                                    */
/* -------------------------------------------------------------------- */
    oModule.Dump( stdout );

    oModule.Create( "out.000" );

/* -------------------------------------------------------------------- */
/*      Create a record.                                                */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = new DDFRecord( &oModule );
    DDFField *poField = poRec->AddField( oModule.FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, "\1\0\036", 3 );

    /*poField = */ poRec->AddField( oModule.FindFieldDefn( "DSID" ) );

    poRec->SetIntSubfield   ( "DSID", 0, "RCNM", 0, 10 );
    poRec->SetIntSubfield   ( "DSID", 0, "RCID", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "EXPP", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "INTU", 0, 4 );
    poRec->SetStringSubfield( "DSID", 0, "DSNM", 0, "GB4X0000.000" );
    poRec->SetStringSubfield( "DSID", 0, "EDTN", 0, "2" );
    poRec->SetStringSubfield( "DSID", 0, "UPDN", 0, "0" );
    poRec->SetStringSubfield( "DSID", 0, "UADT", 0, "20010409" );
    poRec->SetStringSubfield( "DSID", 0, "ISDT", 0, "20010409" );
    poRec->SetFloatSubfield ( "DSID", 0, "STED", 0, 3.1 );
    poRec->SetIntSubfield   ( "DSID", 0, "PRSP", 0, 1 );
    poRec->SetStringSubfield( "DSID", 0, "PSDN", 0, "" );
    poRec->SetStringSubfield( "DSID", 0, "PRED", 0, "2.0" );
    poRec->SetIntSubfield   ( "DSID", 0, "PROF", 0, 1 );
    poRec->SetIntSubfield   ( "DSID", 0, "AGEN", 0, 540 );
    poRec->SetStringSubfield( "DSID", 0, "COMT", 0, "" );

    /*poField = */ poRec->AddField( oModule.FindFieldDefn( "DSSI" ) );

    poRec->SetIntSubfield   ( "DSSI", 0, "DSTR", 0, 2 );
    poRec->SetIntSubfield   ( "DSSI", 0, "AALL", 0, 1 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NALL", 0, 1 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOMR", 0, 22 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOCR", 0, 0 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOGR", 0, 2141 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOLR", 0, 15 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOIN", 0, 512 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOCN", 0, 2181 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOED", 0, 3192 );
    poRec->SetIntSubfield   ( "DSSI", 0, "NOFA", 0, 0 );

    poRec->Write();
    delete poRec;

/* -------------------------------------------------------------------- */
/*      Create a record.                                                */
/* -------------------------------------------------------------------- */
    poRec = new DDFRecord( &oModule );

    poField = poRec->AddField( oModule.FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, "\2\0\036", 3 );

    /*poField = */ poRec->AddField( oModule.FindFieldDefn( "DSPM" ) );

    poRec->SetIntSubfield   ( "DSPM", 0, "RCNM", 0, 20 );
    poRec->SetIntSubfield   ( "DSPM", 0, "RCID", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "HDAT", 0, 2 );
    poRec->SetIntSubfield   ( "DSPM", 0, "VDAT", 0, 17 );
    poRec->SetIntSubfield   ( "DSPM", 0, "SDAT", 0, 23 );
    poRec->SetIntSubfield   ( "DSPM", 0, "CSCL", 0, 52000 );
    poRec->SetIntSubfield   ( "DSPM", 0, "DUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "HUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "PUNI", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "COUN", 0, 1 );
    poRec->SetIntSubfield   ( "DSPM", 0, "COMF", 0, 1000000 );
    poRec->SetIntSubfield   ( "DSPM", 0, "SOMF", 0, 10 );

    poRec->Write();
    delete poRec;

/* -------------------------------------------------------------------- */
/*      Create a record.                                                */
/* -------------------------------------------------------------------- */
    poRec = new DDFRecord( &oModule );

    poField = poRec->AddField( oModule.FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, "\3\0\036", 3 );

    /*poField = */ poRec->AddField( oModule.FindFieldDefn( "VRID" ) );

    poRec->SetIntSubfield   ( "VRID", 0, "RCNM", 0, 110 );
    poRec->SetIntSubfield   ( "VRID", 0, "RCID", 0, 518 );
    poRec->SetIntSubfield   ( "VRID", 0, "RVER", 0, 1 );
    poRec->SetIntSubfield   ( "VRID", 0, "RUIN", 0, 1 );

    /*poField = */ poRec->AddField( oModule.FindFieldDefn( "SG3D" ) );

    poRec->SetIntSubfield   ( "SG3D", 0, "YCOO", 0, -325998702 );
    poRec->SetIntSubfield   ( "SG3D", 0, "XCOO", 0, 612175350 );
    poRec->SetIntSubfield   ( "SG3D", 0, "VE3D", 0, 174 );

    poRec->SetIntSubfield   ( "SG3D", 0, "YCOO", 1, -325995189 );
    poRec->SetIntSubfield   ( "SG3D", 0, "XCOO", 1, 612228812 );
    poRec->SetIntSubfield   ( "SG3D", 0, "VE3D", 1, 400 );

    poRec->Write();

    delete poRec;
}

/************************************************************************/
/*                             mk_catalog()                             */
/************************************************************************/

#if 0
void mk_catalog()

{
    DDFModule  oModule;

    oModule.Initialize();

/* -------------------------------------------------------------------- */
/*      Create the '0000' definition.                                   */
/* -------------------------------------------------------------------- */
    DDFFieldDefn *poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0000", "", "0001CATD",
                     dsc_elementary,
                     dtc_char_string );

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the '0000' definition.                                   */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0001", "ISO 8211 Record Identifier", "",
                     dsc_elementary, dtc_bit_string,
                     "(b12)" );

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the CATD field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "CATD", "Catalog Directory field", "",
                     dsc_vector, dtc_mixed_data_type );

    poFDefn->AddSubfield( "RCNM", "A(2)" );
    poFDefn->AddSubfield( "RCID", "I(10)" );
    poFDefn->AddSubfield( "FILE", "A" );
    poFDefn->AddSubfield( "LFIL", "A" );
    poFDefn->AddSubfield( "VOLM", "A" );
    poFDefn->AddSubfield( "IMPL", "A(3)" );
    poFDefn->AddSubfield( "SLAT", "R" );
    poFDefn->AddSubfield( "WLON", "R" );
    poFDefn->AddSubfield( "NLAT", "R" );
    poFDefn->AddSubfield( "ELON", "R" );
    poFDefn->AddSubfield( "CRCS", "A" );
    poFDefn->AddSubfield( "COMT", "A" );

    oModule.AddField( poFDefn );

    oModule.Dump( stdout );

    oModule.Create( "out.ddf" );

/* -------------------------------------------------------------------- */
/*      Create a record.                                                */
/* -------------------------------------------------------------------- */
    DDFRecord *poRec = new DDFRecord( &oModule );
    DDFField *poField = poRec->AddField( oModule.FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, "\0\0\036", 3 );

    poField = poRec->AddField( oModule.FindFieldDefn( "CATD" ) );
    poRec->SetStringSubfield( "CATD", 0, "RCNM", 0, "CD" );
    poRec->SetIntSubfield   ( "CATD", 0, "RCID", 0, 1 );
    poRec->SetStringSubfield( "CATD", 0, "FILE", 0, "CATALOG.030" );
    poRec->SetStringSubfield( "CATD", 0, "VOLM", 0, "V01X01" );
    poRec->SetStringSubfield( "CATD", 0, "IMPL", 0, "ASC" );
    poRec->SetStringSubfield( "CATD", 0, "COMT", 0,
                              "Exchange Set Catalog file" );
    poRec->Write();
    delete poRec;

/* -------------------------------------------------------------------- */
/*      Create a record.                                                */
/* -------------------------------------------------------------------- */
    poRec = new DDFRecord( &oModule );

    poField = poRec->AddField( oModule.FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, "\1\0\036", 3 );

    poField = poRec->AddField( oModule.FindFieldDefn( "CATD" ) );
    poRec->SetStringSubfield( "CATD", 0, "RCNM", 0, "CD" );
    poRec->SetIntSubfield   ( "CATD", 0, "RCID", 0, 2 );
    poRec->SetStringSubfield( "CATD", 0, "FILE", 0, "No410810.000" );
    poRec->SetStringSubfield( "CATD", 0, "VOLM", 0, "V01X01" );
    poRec->SetStringSubfield( "CATD", 0, "IMPL", 0, "BIN" );
    poRec->SetFloatSubfield ( "CATD", 0, "SLAT", 0, 59.000005 );
    poRec->SetFloatSubfield ( "CATD", 0, "WLON", 0, 4.999996 );
    poRec->SetFloatSubfield ( "CATD", 0, "NLAT", 0, 59.500003 );
    poRec->SetFloatSubfield ( "CATD", 0, "ELON", 0, 5.499997 );
    poRec->SetStringSubfield( "CATD", 0, "CRCS", 0, "555C3AD4" );
    poRec->Write();
    delete poRec;
}
#endif // mk_catalog not used.

/* **********************************************************************/
/*                                main()                                */
/* **********************************************************************/

int main( int /* nArgc */, char ** /* papszArgv */ )

{
    mk_s57();
}
