/******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 * hfatest.cpp
 *
 * Working test program for HFA support.
 *
 * $Log$
 * Revision 1.1  1999/01/04 05:28:13  warmerda
 * New
 *
 */

#include "hfa_p.h"

/************************************************************************/
/*                            HFADumpNode()                             */
/************************************************************************/

void	HFADumpNode( HFAEntry *poEntry, int nIndent, int bVerbose )

{
    static char	szSpaces[256];
    int		i;

    for( i = 0; i < nIndent*2; i++ )
        szSpaces[i] = ' ';
    szSpaces[nIndent*2] = '\0';

    printf( "%s%s(%s) %d @ %d\n", szSpaces,
            poEntry->GetName(), poEntry->GetType(),
            poEntry->GetDataSize(), poEntry->GetDataPos() );

    if( bVerbose )
    {
        strcat( szSpaces, "+ " );
        poEntry->DumpFieldValues( stdout, szSpaces );
        printf( "\n" );
    }

    if( poEntry->GetChild() != NULL )
        HFADumpNode( poEntry->GetChild(), nIndent+1, bVerbose );
    
    if( poEntry->GetNext() != NULL )
        HFADumpNode( poEntry->GetNext(), nIndent, bVerbose );
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "hfatest [-dd] [-dt] filename\n" );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    const char	*pszFilename = NULL;
    int		nDumpTree = FALSE;
    int		nDumpDict = FALSE;
    int		i;
    HFAHandle	hHFA;

/* -------------------------------------------------------------------- */
/*      Handle arguments.                                               */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"-dd") )
            nDumpDict = TRUE;
        else if( EQUAL(argv[i],"-dt") )
            nDumpTree = TRUE;
        else if( pszFilename == NULL )
            pszFilename = argv[i];
        else
        {
            Usage();
            exit( 1 );
        }
    }

    if( pszFilename == NULL )
    {
        Usage();
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    hHFA = HFAOpen( pszFilename, "r" );

    if( hHFA == NULL )
    {
        printf( "HFAOpen() failed.\n" );
        exit( 100 );
    }

/* -------------------------------------------------------------------- */
/*      Do we want to walk the tree dumping out general information?    */
/* -------------------------------------------------------------------- */
    if( nDumpDict )
    {
        printf( "%s\n", hHFA->pszDictionary );
        
        hHFA->poDictionary->Dump( stdout );
    }

/* -------------------------------------------------------------------- */
/*      Do we want to walk the tree dumping out general information?    */
/* -------------------------------------------------------------------- */
    if( nDumpTree )
    {
        HFADumpNode( hHFA->poRoot, 0, TRUE );
    }

    HFAClose( hHFA );
}
