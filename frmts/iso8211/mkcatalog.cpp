/* ****************************************************************************
 * $Id$
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2003/07/03 15:40:13  warmerda
 * new
 *
 */

#include "iso8211.h"

CPL_CVSID("$Id$");


/* **********************************************************************/
/*                                main()                                */
/* **********************************************************************/

int main( int nArgc, char ** papszArgv )

{
    DDFModule  oModule;
    DDFFieldDefn *poFDefn;

    oModule.Initialize();

/* -------------------------------------------------------------------- */
/*      Create the '0000' definition.                                   */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0000", "", "0001CATD", 
                     DDFFieldDefn::elementary, 
                     DDFFieldDefn::char_string );

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the '0000' definition.                                   */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "0001", "ISO 8211 Record Identifier", "", 
                     DDFFieldDefn::elementary, DDFFieldDefn::bit_string,
                     "(b12)" );

    oModule.AddField( poFDefn );

/* -------------------------------------------------------------------- */
/*      Create the CATD field.                                          */
/* -------------------------------------------------------------------- */
    poFDefn = new DDFFieldDefn();

    poFDefn->Create( "CATD", "Catalog Directory field", "",
                     DDFFieldDefn::vector, DDFFieldDefn::mixed_data_type );

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
    DDFField *poField;

    poField = poRec->AddField( oModule.FindFieldDefn( "0001" ) );
    poRec->SetFieldRaw( poField, 0, "\0\0\036", 3 );

    poField = poRec->AddField( oModule.FindFieldDefn( "CATD" ) );
//    poRec->SetFieldRaw( poField, 0, "\0\0\036", 3 );

    poRec->Write();
}
