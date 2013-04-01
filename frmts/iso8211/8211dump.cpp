/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Dump 8211 file in verbose form - just a junk program. 
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
#include "cpl_vsi.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");


int main( int nArgc, char ** papszArgv )

{
    DDFModule   oModule;
    const char  *pszFilename = NULL;
    int         bFSPTHack = FALSE;
    int         bXML = FALSE;

/* -------------------------------------------------------------------- */
/*      Check arguments.                                                */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-fspt_repeating") )
            bFSPTHack = TRUE;
        else if( EQUAL(papszArgv[iArg],"-xml") )
            bXML = TRUE;
        else
            pszFilename = papszArgv[iArg];
    }

    if( pszFilename == NULL )
    {
        printf( "Usage: 8211dump [-xml] [-fspt_repeating] filename\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open file.                                                      */
/* -------------------------------------------------------------------- */
    if( !oModule.Open( pszFilename ) )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Apply FSPT hack if required.                                    */
/* -------------------------------------------------------------------- */
    if( bFSPTHack )
    {
        DDFFieldDefn *poFSPT = oModule.FindFieldDefn( "FSPT" );

        if( poFSPT == NULL )
            fprintf( stderr, 
                     "unable to find FSPT field to set repeating flag.\n" );
        else
            poFSPT->SetRepeatingFlag( TRUE );
    }

/* -------------------------------------------------------------------- */
/*      Dump header, and all records.                                   */
/* -------------------------------------------------------------------- */
    DDFRecord       *poRecord;
    if( bXML )
    {
        printf("<DDFModule>\n");

        int nFieldDefnCount = oModule.GetFieldCount();
        for( int i = 0; i < nFieldDefnCount; i++ )
        {
            DDFFieldDefn* poFieldDefn = oModule.GetField(i);
            const char* pszDataStructCode;
            switch( poFieldDefn->GetDataStructCode() )
            {
                case dsc_elementary:
                    pszDataStructCode = "elementary";
                    break;
                    
                case dsc_vector:
                    pszDataStructCode = "vector";
                    break;
                    
                case dsc_array:
                    pszDataStructCode = "array";
                    break;
                    
                case dsc_concatenated:
                    pszDataStructCode = "concatenated";
                    break;
                    
                default:
                    pszDataStructCode = "(unknown)";
                    break;
            }

            const char* pszDataTypeCode;
            switch( poFieldDefn->GetDataTypeCode() )
            {
                case dtc_char_string:
                    pszDataTypeCode = "char_string";
                    break;
                    
                case dtc_implicit_point:
                    pszDataTypeCode = "implicit_point";
                    break;
                    
                case dtc_explicit_point:
                    pszDataTypeCode = "explicit_point";
                    break;
                    
                case dtc_explicit_point_scaled:
                    pszDataTypeCode = "explicit_point_scaled";
                    break;
                    
                case dtc_char_bit_string:
                    pszDataTypeCode = "char_bit_string";
                    break;
                    
                case dtc_bit_string:
                    pszDataTypeCode = "bit_string";
                    break;
                    
                case dtc_mixed_data_type:
                    pszDataTypeCode = "mixed_data_type";
                    break;

                default:
                    pszDataTypeCode = "(unknown)";
                    break;
            }
            
            printf("<DDFFieldDefn tag=\"%s\" fieldName=\"%s\" arrayDescr=\"%s\" "
                   "formatControls=\"%s\" dataStructCode=\"%s\" dataTypeCode=\"%s\">\n",
                   poFieldDefn->GetName(),
                   poFieldDefn->GetDescription(),
                   poFieldDefn->GetArrayDescr(),
                   poFieldDefn->GetFormatControls(),
                   pszDataStructCode,
                   pszDataTypeCode);
            int nSubfieldCount = poFieldDefn->GetSubfieldCount();
            for( int iSubField = 0; iSubField < nSubfieldCount; iSubField++ )
            {
                DDFSubfieldDefn* poSubFieldDefn = poFieldDefn->GetSubfield(iSubField);
                printf("  <DDFSubfieldDefn name=\"%s\" format=\"%s\"/>\n",
                       poSubFieldDefn->GetName(), poSubFieldDefn->GetFormat());
            }
            printf("</DDFFieldDefn>\n");
        }

        for( poRecord = oModule.ReadRecord();
             poRecord != NULL; poRecord = oModule.ReadRecord() )
        {
            printf("<DDFRecord>\n");
            int nFieldCount = poRecord->GetFieldCount();
            for( int iField = 0; iField < nFieldCount; iField++ )
            {
                DDFField* poField = poRecord->GetField(iField);
                DDFFieldDefn* poDefn = poField->GetFieldDefn();
                const char* pszFieldName = poDefn->GetName();
                printf("  <DDFField name=\"%s\"", pszFieldName);
                if( poField->GetRepeatCount() > 1 )
                    printf(" repeatCount=\"%d\"", poField->GetRepeatCount());
                int iOffset = 0, nLoopCount;
                int nRepeatCount = poField->GetRepeatCount();
                const char* pachData = poField->GetData();
                int nDataSize = poField->GetDataSize();
                if( nRepeatCount == 1 && poDefn->GetSubfieldCount() == 0 )
                {
                    printf(" value=\"0x");
                    for( int i = 0; i < nDataSize - 1; i++ )
                        printf( "%02X", pachData[i] );
                    printf("\">\n");
                }
                else
                    printf(">\n");
                for( nLoopCount = 0; nLoopCount < nRepeatCount; nLoopCount++ )
                {
                    for( int iSubField = 0; iSubField < poDefn->GetSubfieldCount(); iSubField++ )
                    {
                        int         nBytesConsumed;
                        DDFSubfieldDefn* poSubFieldDefn = poDefn->GetSubfield(iSubField);
                        const char* pszSubFieldName = poSubFieldDefn->GetName();
                        printf("    <DDFSubfield name=\"%s\" ", pszSubFieldName);
                        DDFDataType eType = poSubFieldDefn->GetType();
                        const char* pachSubdata = pachData + iOffset;
                        int nMaxBytes = nDataSize - iOffset;
                        if( eType == DDFFloat )
                            printf("type=\"float\">%f",
                                   poSubFieldDefn->ExtractFloatData( pachSubdata, nMaxBytes, NULL ) );
                        else if( eType == DDFInt )
                            printf("type=\"integer\">%d",
                                   poSubFieldDefn->ExtractIntData( pachSubdata, nMaxBytes, NULL ) );
                        else if( eType == DDFBinaryString )
                        {
                            int     nBytes, i;
                            GByte   *pabyBString = (GByte *)
                                poSubFieldDefn->ExtractStringData( pachSubdata, nMaxBytes, &nBytes );

                            printf( "type=\"binary\">0x" );
                            for( i = 0; i < nBytes; i++ )
                                printf( "%02X", pabyBString[i] );
                        }
                        else
                        {
                            GByte* pabyString = (GByte *)poSubFieldDefn->ExtractStringData( pachSubdata, nMaxBytes, NULL );
                            int bBinary = FALSE;
                            int i;
                            for( i = 0; pabyString[i] != '\0'; i ++ )
                            {
                                if( pabyString[i] < 32 || pabyString[i] > 127 )
                                {
                                    bBinary = TRUE;
                                    break;
                                }
                            }
                            if( bBinary )
                            {
                                printf( "type=\"binary\">0x" );
                                for( i = 0; pabyString[i] != '\0'; i ++ )
                                    printf( "%02X", pabyString[i] );
                            }
                            else
                            {
                                char* pszEscaped = CPLEscapeString((const char*)pabyString, -1, CPLES_XML);
                                printf("type=\"string\">%s", pszEscaped);
                                CPLFree(pszEscaped);
                            }
                        }
                        printf("</DDFSubfield>\n");

                        poSubFieldDefn->GetDataLength( pachSubdata, nMaxBytes, &nBytesConsumed );

                        iOffset += nBytesConsumed;
                    }
                }
                printf("  </DDFField>\n");
            }
            printf("</DDFRecord>\n");
        }
        printf("</DDFModule>\n");
    }
    else
    {
        oModule.Dump( stdout );
        long nStartLoc;

        nStartLoc = VSIFTellL( oModule.GetFP() );
        for( poRecord = oModule.ReadRecord();
            poRecord != NULL; poRecord = oModule.ReadRecord() )
        {
            printf( "File Offset: %ld\n", nStartLoc );
            poRecord->Dump( stdout );

            nStartLoc = VSIFTellL( oModule.GetFP() );
        }
    }

    oModule.Close();
    
#ifdef DBMALLOC
    malloc_dump(1);
#endif

}



