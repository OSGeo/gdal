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
 * Revision 1.2  2002/12/03 04:43:54  warmerda
 * lots of work
 *
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
        printf( "Usage: nitfdump <nitf_filename>\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    psFile = NITFOpen( papszArgv[1], FALSE );
    if( psFile == NULL )
        exit( 2 );

/* -------------------------------------------------------------------- */
/*      Dump first TRE tag if there are any.                            */
/* -------------------------------------------------------------------- */
    if( psFile->pachTRE != NULL )
        printf( "File contains %d bytes of TRE data.  "
                "The first tag is %6.6s.\n\n", 
                psFile->nTREBytes, psFile->pachTRE );

/* -------------------------------------------------------------------- */
/*      Report info from location table, if found.                      */
/* -------------------------------------------------------------------- */
    if( psFile->nLocCount > 0 )
    {
        int i;
        printf( "Location Table\n" );
        for( i = 0; i < psFile->nLocCount; i++ )
        {
            printf( "  LocId=%d, Offset=%d, Size=%d\n", 
                    psFile->pasLocations[i].nLocId,
                    psFile->pasLocations[i].nLocOffset,
                    psFile->pasLocations[i].nLocSize );
        }
        printf( "\n" );
    }

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
/*      Report details of images.                                       */
/* -------------------------------------------------------------------- */
    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;
        NITFImage *psImage;
        int iBand;

        if( !EQUAL(psSegInfo->szSegmentType,"IM") )
            continue;
        
        psImage = NITFImageAccess( psFile, iSegment );
        if( psImage == NULL )
        {
            printf( "NITFAccessImage(%d) failed!\n", iSegment );
            continue;
        }

        printf( "Image Segment %d, %dPx%dLx%dB x %dbits:\n", 
                iSegment, psImage->nRows, psImage->nCols, psImage->nBands,
                psImage->nBitsPerSample );
        printf( "  PVTYPE=%s, IREP=%s, ICAT=%s, IMODE=%c, IC=%s, COMRAT=%s\n", 
                psImage->szPVType, psImage->szIREP, psImage->szICAT,
                psImage->chIMODE, psImage->szIC, psImage->szCOMRAT );
        printf( "  %d x %d blocks of size %d x %d\n",
                psImage->nBlocksPerRow, psImage->nBlocksPerColumn,
                psImage->nBlockWidth, psImage->nBlockHeight );

        if( strlen(psImage->pszComments) > 0 )
            printf( "  Comments:\n%s\n", psImage->pszComments );

        for( iBand = 0; iBand < psImage->nBands; iBand++ )
        {
            NITFBandInfo *psBandInfo = psImage->pasBandInfo + iBand;

            printf( "  Band %d: IREPBAND=%s, ISUBCAT=%s, %d LUT entries.\n",
                    iBand + 1, psBandInfo->szIREPBAND, psBandInfo->szISUBCAT,
                    psBandInfo->nSignificantLUTEntries );
        }
    }

/* -------------------------------------------------------------------- */
/*      Close.                                                          */
/* -------------------------------------------------------------------- */
    NITFClose( psFile );

    exit( 0 );
}


