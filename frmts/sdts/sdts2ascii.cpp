/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Mainline test program for converting SDTS TVP (or at least USGS
 *           dlg) datasets to an ASCII dump.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  1999/03/23 15:58:29  warmerda
 * added attribute support, dump catalog
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include "sdts_al.h"

int main( int nArgc, char ** papszArgv )

{
    SDTS_IREF	oIREF;
    SDTS_CATD	oCATD;
    int		i;
    const char	*pszCATDFilename;

    if( nArgc < 2 )
        pszCATDFilename = "dlg/TR01CATD.DDF";
    else
        pszCATDFilename = papszArgv[1];

/* -------------------------------------------------------------------- */
/*      Read the catalog.                                               */
/* -------------------------------------------------------------------- */
    if( !oCATD.Read( pszCATDFilename ) )
    {
        cerr << "Failed to read CATD file\n";
        exit( 100 );
    }

    printf( "Catalog:\n" );
    for( i = 0; i < oCATD.getEntryCount(); i++ )
    {
        printf( "  %s: `%s'\n",
                oCATD.getEntryModule(i).c_str(),
                oCATD.getEntryType(i).c_str() );
    }
    printf( "\n" );
    
/* -------------------------------------------------------------------- */
/*      Dump the internal reference information.                        */
/* -------------------------------------------------------------------- */
    cout << "IREF filename = " << oCATD.getModuleFilePath( "IREF" ) << "\n";
    if( oIREF.Read( oCATD.getModuleFilePath( "IREF" ) ) )
    {
        cout << "X Label = " << oIREF.osXAxisName << "\n";
        cout << "X Scale = " << oIREF.dfXScale << "\n";
    }

/* -------------------------------------------------------------------- */
/*      Dump the first line file.                                       */
/* -------------------------------------------------------------------- */
    SDTSLineReader oLineReader( &oIREF );
    
    if( oLineReader.Open( oCATD.getModuleFilePath( "LE01" ) ) )
    {
        SDTSRawLine	*poRawLine;
        
        while( (poRawLine = oLineReader.GetNextLine()) != NULL )
        {
            poRawLine->Dump( stdout );
            delete poRawLine;
        }
        
        oLineReader.Close();
    }
    
/* -------------------------------------------------------------------- */
/*	Dump all modules of type "Primary Attribute" in the catalog.	*/
/* -------------------------------------------------------------------- */
    SDTSAttrReader oAttrReader( &oIREF );

    for( i = 0; i < oCATD.getEntryCount(); i++ )
    {
        if( oCATD.getEntryType(i) == "Attribute Primary         " )
        {
            if( oAttrReader.Open(
                	oCATD.getModuleFilePath( oCATD.getEntryModule(i)) ) )
            {
                SDTSAttrRecord	*poRecord;

                while( (poRecord = oAttrReader.GetNextRecord()) != NULL )
                {
                    fprintf( stdout,
                             "\nRecord %s:%ld\n",
                             poRecord->oRecordId.szModule,
                             poRecord->oRecordId.nRecord );
                             
                    cout << *(poRecord->GetSubfieldList());

                    delete poRecord;
                }

                oAttrReader.Close();
            }
        }
    }
    
}


