/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Simple test mainline to dump info about NITF file. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 * Revision 1.1  2002/12/02 06:09:29  warmerda
 * New
 *
 */

#include "nitflib.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    NITFFile	*psFile;
    int          iSegment;

    if( nArgc < 2 )
    {
        printf( "Usage: nitfdump <filename>\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    psFile = NITFOpen( papszArgv[1], FALSE );
    if( psFile == NULL )
        exit( 2 );

/* -------------------------------------------------------------------- */
/*      Dump general info about segments.                               */
/* -------------------------------------------------------------------- */
    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;

        printf( "Segment %d (Type=%s):\n", 
                iSegment + 1, psSegInfo->szSegmentType );

        printf( "  HeaderStart=%d, HeaderSize=%d, DataStart=%d, DataSize=%d\n",
                psSegInfo->nSegmentHeaderStart,
                psSegInfo->nSegmentHeaderSize, 
                psSegInfo->nSegmentStart,
                psSegInfo->nSegmentSize );
        printf( "\n" );
    }

/* -------------------------------------------------------------------- */
/*      Close.                                                          */
/* -------------------------------------------------------------------- */
    NITFClose( psFile );

    exit( 0 );
}


