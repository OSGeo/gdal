/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Dump 8211 file in verbose form - just a junk program. 
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
 * Revision 1.2  1999/05/07 13:45:01  warmerda
 * major upgrade to use iso8211lib
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include <stdio.h>
#include <iostream>
#include <fstream>
#include "io/sio_Reader.h"
#include "io/sio_8211Converter.h"

#include "container/sc_Record.h"
#include "sdts_al.h"

sio_8211Converter_BI8   converter_bi8;
sio_8211Converter_BI16	converter_bi16;
sio_8211Converter_BI24  converter_bi24;
sio_8211Converter_BI32	converter_bi32;
sio_8211Converter_BUI8	converter_bui8;
sio_8211Converter_BUI16	converter_bui16;
sio_8211Converter_BUI24	converter_bui24;
sio_8211Converter_BUI32	converter_bui32;
sio_8211Converter_BFP32	converter_bfp32;
sio_8211Converter_BFP64 converter_bfp64;

int main( int nArgc, char ** papszArgv )

{
    sio_8211_converter_dictionary converters; // hints for reader for binary data
    
    converters["X"] = &converter_bi32; // set up default converter hints
    converters["Y"] = &converter_bi32; // for these mnemonics
    converters["ELEVATION"] = &converter_bi16;

    if( nArgc < 2 )
    {
        printf( "Usage: sdtsdump filename\n" );
        exit( 1 );
    }

    ifstream	ifs;
    
    ifs.open( papszArgv[1] );
    if( !ifs )
    {
        printf( "Unable to open `%s'\n", papszArgv[1] );
        exit( 1 );
    }

    sio_8211Reader	oReader( ifs, &converters );
    sio_8211ForwardIterator oIter( oReader );
    scal_Record		oRecord;

    while( oIter )
    {
        oIter.get( oRecord );

        cout << oRecord << "\n";

        cout << "\n--- record boundary ---\n\n";
        
        ++oIter;
    }

    ifs.close();
}



