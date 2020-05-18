/* ****************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Example program dumping data in 8211 data to stdout.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
#include "iso8211.h"

#include <algorithm>

CPL_CVSID("$Id$")

static void ViewRecordField( DDFField * poField );
static int ViewSubfield( DDFSubfieldDefn *poSFDefn,
                         const char * pachFieldData,
                         int nBytesRemaining );

/* **********************************************************************/
/*                                main()                                */
/* **********************************************************************/

int main( int nArgc, char ** papszArgv )

{
    DDFModule   oModule;
    const char  *pszFilename = nullptr;
    int         bFSPTHack = FALSE;

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-fspt_repeating") )
            bFSPTHack = TRUE;
        else
            pszFilename = papszArgv[iArg];
    }

    if( pszFilename == nullptr )
    {
        printf( "Usage: 8211view filename\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open the file.  Note that by default errors are reported to     */
/*      stderr, so we don't bother doing it ourselves.                  */
/* -------------------------------------------------------------------- */
    if( !oModule.Open( pszFilename ) )
    {
        exit( 1 );
    }

    if( bFSPTHack )
    {
        DDFFieldDefn *poFSPT = oModule.FindFieldDefn( "FSPT" );

        if( poFSPT == nullptr )
            fprintf( stderr,
                     "unable to find FSPT field to set repeating flag.\n" );
        else
            poFSPT->SetRepeatingFlag( TRUE );
    }

/* -------------------------------------------------------------------- */
/*      Loop reading records till there are none left.                  */
/* -------------------------------------------------------------------- */
    DDFRecord *poRecord = nullptr;
    int iRecord = 0;

    while( (poRecord = oModule.ReadRecord()) != nullptr )
    {
        printf( "Record %d (%d bytes)\n",
                ++iRecord, poRecord->GetDataSize() );

        /* ------------------------------------------------------------ */
        /*      Loop over each field in this particular record.         */
        /* ------------------------------------------------------------ */
        for( int iField = 0; iField < poRecord->GetFieldCount(); iField++ )
        {
            DDFField    *poField = poRecord->GetField( iField );

            ViewRecordField( poField );
        }
    }
}

/* **********************************************************************/
/*                          ViewRecordField()                           */
/*                                                                      */
/*      Dump the contents of a field instance in a record.              */
/* **********************************************************************/

static void ViewRecordField( DDFField * poField )

{
    DDFFieldDefn *poFieldDefn = poField->GetFieldDefn();

    // Report general information about the field.
    printf( "    Field %s: %s\n",
            poFieldDefn->GetName(), poFieldDefn->GetDescription() );

    // Get pointer to this fields raw data.  We will move through
    // it consuming data as we report subfield values.

    const char  *pachFieldData = poField->GetData();
    int nBytesRemaining = poField->GetDataSize();

    /* -------------------------------------------------------- */
    /*      Loop over the repeat count for this fields          */
    /*      subfields.  The repeat count will almost            */
    /*      always be one.                                      */
    /* -------------------------------------------------------- */
    for( int iRepeat = 0; iRepeat < poField->GetRepeatCount(); iRepeat++ )
    {

        /* -------------------------------------------------------- */
        /*   Loop over all the subfields of this field, advancing   */
        /*   the data pointer as we consume data.                   */
        /* -------------------------------------------------------- */
        for( int iSF = 0; iSF < poFieldDefn->GetSubfieldCount(); iSF++ )
        {
            DDFSubfieldDefn *poSFDefn = poFieldDefn->GetSubfield( iSF );
            int nBytesConsumed =
                ViewSubfield( poSFDefn, pachFieldData, nBytesRemaining );

            nBytesRemaining -= nBytesConsumed;
            pachFieldData += nBytesConsumed;
        }
    }
}

/* **********************************************************************/
/*                            ViewSubfield()                            */
/* **********************************************************************/

static int ViewSubfield( DDFSubfieldDefn *poSFDefn,
                         const char * pachFieldData,
                         int nBytesRemaining )

{
    int         nBytesConsumed = 0;

    switch( poSFDefn->GetType() )
    {
      case DDFInt:
        if( poSFDefn->GetBinaryFormat() == DDFSubfieldDefn::UInt )
            printf( "        %s = %u\n",
                    poSFDefn->GetName(),
                    static_cast<unsigned int>(
                        poSFDefn->ExtractIntData( pachFieldData, nBytesRemaining,
                                              &nBytesConsumed )) );
        else
            printf( "        %s = %d\n",
                    poSFDefn->GetName(),
                    poSFDefn->ExtractIntData( pachFieldData, nBytesRemaining,
                                              &nBytesConsumed ) );
        break;

      case DDFFloat:
        printf( "        %s = %f\n",
                poSFDefn->GetName(),
                poSFDefn->ExtractFloatData( pachFieldData, nBytesRemaining,
                                            &nBytesConsumed ) );
        break;

      case DDFString:
        printf( "        %s = `%s'\n",
                poSFDefn->GetName(),
                poSFDefn->ExtractStringData( pachFieldData, nBytesRemaining,
                                             &nBytesConsumed ) );
        break;

      case DDFBinaryString:
      {
          GByte *pabyBString = (GByte *)
              poSFDefn->ExtractStringData( pachFieldData, nBytesRemaining,
                                           &nBytesConsumed );

          printf( "        %s = 0x", poSFDefn->GetName() );
          for( int i = 0; i < std::min(nBytesConsumed, 24); i++ )
              printf( "%02X", pabyBString[i] );

          if( nBytesConsumed > 24 )
              printf( "%s", "..." );

          // rjensen 19-Feb-2002 S57 quick hack. decode NAME and LNAM bitfields
          if ( EQUAL(poSFDefn->GetName(),"NAME") )
          {
              const int vrid_rcnm=pabyBString[0];
              const int vrid_rcid=pabyBString[1] + (pabyBString[2]*256)+
                  (pabyBString[3]*65536)+ (pabyBString[4]*16777216);
              printf("\tVRID RCNM = %d,RCID = %d",vrid_rcnm,vrid_rcid);
          }
          else if ( EQUAL(poSFDefn->GetName(),"LNAM") )
          {
              const int foid_agen=pabyBString[0] + (pabyBString[1]*256);
              const int foid_find=pabyBString[2] + (pabyBString[3]*256)+
                  (pabyBString[4]*65536)+ (pabyBString[5]*16777216);
              const int foid_fids=pabyBString[6] + (pabyBString[7]*256);
              printf("\tFOID AGEN = %d,FIDN = %d,FIDS = %d",
                     foid_agen,foid_find,foid_fids);
          }

          printf( "\n" );
      }
      break;
    }

    return nBytesConsumed;
}
