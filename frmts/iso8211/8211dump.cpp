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
 * Revision 1.1  1999/04/27 18:45:54  warmerda
 * New
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include <stdio.h>
#include "iso8211.h"


int main( int nArgc, char ** papszArgv )

{
    DDFModule	oModule;
    const char	*pszFilename = "SC01CATD.DDF";

    if( nArgc > 1 )
        pszFilename = papszArgv[1];

    if( oModule.Open( pszFilename ) )
    {
        DDFRecord	*poRecord;
        
        oModule.Dump( stdout );

        for( poRecord = oModule.ReadRecord();
             poRecord != NULL; poRecord = oModule.ReadRecord() )
        {
            poRecord->Dump( stdout );
        }
    }
}



