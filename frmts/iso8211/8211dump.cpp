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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.7  2006/04/04 04:24:06  fwarmerdam
 * update contact info
 *
 * Revision 1.6  2003/11/11 20:53:53  warmerda
 * Report file offset before each record dumped.
 *
 * Revision 1.5  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.4  2000/06/16 18:02:08  warmerda
 * added SetRepeatingFlag hack support
 *
 * Revision 1.3  1999/11/18 19:03:04  warmerda
 * expanded tabs
 *
 * Revision 1.2  1999/05/08 20:15:19  warmerda
 * Added malloc_dump to watch for memory leaks
 *
 * Revision 1.1  1999/04/27 18:45:54  warmerda
 * New
 *
 * Revision 1.1  1999/03/23 13:56:13  warmerda
 * New
 *
 */

#include <stdio.h>
#include "iso8211.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$");


int main( int nArgc, char ** papszArgv )

{
    DDFModule   oModule;
    const char  *pszFilename = NULL;
    int         bFSPTHack = FALSE;

/* -------------------------------------------------------------------- */
/*      Check arguments.                                                */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-fspt_repeating") )
            bFSPTHack = TRUE;
        else
            pszFilename = papszArgv[iArg];
    }

    if( pszFilename == NULL )
    {
        printf( "Usage: 8211dump filename\n" );
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
/*      Dump header, and all recodrs.                                   */
/* -------------------------------------------------------------------- */
    DDFRecord       *poRecord;
    oModule.Dump( stdout );
    long nStartLoc;

    nStartLoc = VSIFTell( oModule.GetFP() );
    for( poRecord = oModule.ReadRecord();
         poRecord != NULL; poRecord = oModule.ReadRecord() )
    {
        printf( "File Offset: %ld\n", nStartLoc );
        poRecord->Dump( stdout );

        nStartLoc = VSIFTell( oModule.GetFP() );
    }

    oModule.Close();
    
#ifdef DBMALLOC
    malloc_dump(1);
#endif

}



