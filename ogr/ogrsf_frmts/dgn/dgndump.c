/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Temporary low level DGN dumper application.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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
 * Revision 1.3  2001/01/10 16:10:57  warmerda
 * Added extents reporting
 *
 * Revision 1.2  2000/12/28 21:27:38  warmerda
 * added summary report
 *
 * Revision 1.1  2000/12/14 17:11:18  warmerda
 * New
 *
 */

#include "dgnlibp.h"

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    DGNHandle   hDGN;
    DGNElemCore *psElement;
    const char	*pszFilename;
    int         bSummary = FALSE;

    if( argc < 2 )
    {
        printf( "Usage: dgndump [-s] filename.dgn\n" );
        exit( 1 );
    }
    
    if( strcmp(argv[1],"-s") == 0 )
    {
        bSummary = TRUE;
        pszFilename = argv[2];
    }
    else
        pszFilename = argv[1];

    hDGN = DGNOpen( pszFilename );
    if( hDGN == NULL )
        exit( 1 );

    if( !bSummary )
    {
        while( (psElement=DGNReadElement(hDGN)) != NULL )
        {
            DGNDumpElement( hDGN, psElement, stdout );

            {
                DGNInfo	*psDGNInfo = (DGNInfo *) hDGN;
                int	 nAttIndex;

                nAttIndex = psDGNInfo->abyElem[30]
                          + psDGNInfo->abyElem[31] * 256;

                if( nAttIndex * 2 + 32 < psDGNInfo->nElemBytes )
                    printf( "index to attribute linkage: %d\n", 
                            nAttIndex );
            }
            DGNFreeElement( hDGN, psElement );
        }
    }
    else
    {
        const DGNElementInfo 	*pasEI;
        int			nCount, i, nLevel, nType;
        int			anLevelTypeCount[128*64];
        int			anLevelCount[64];
        int			anTypeCount[128];
        double			adfExtents[6];

        DGNGetExtents( hDGN, adfExtents );
        printf( "X Range: %.2f to %.2f\n", 
                adfExtents[0], adfExtents[3] );
        printf( "Y Range: %.2f to %.2f\n", 
                adfExtents[1], adfExtents[4] );
        printf( "Z Range: %.2f to %.2f\n", 
                adfExtents[2], adfExtents[5] );

        pasEI = DGNGetElementIndex( hDGN, &nCount );

        printf( "Total Elements: %d\n", nCount );
        
        memset( anLevelTypeCount, 0, 128*64*sizeof(int) );
        memset( anLevelCount, 0, 64*sizeof(int) );
        memset( anTypeCount, 0, 128*sizeof(int) );

        for( i = 0; i < nCount; i++ )
        {
            anLevelTypeCount[pasEI[i].level * 128 + pasEI[i].type]++;
            anLevelCount[pasEI[i].level]++;
            anTypeCount[pasEI[i].type]++;
        }

        printf( "\n" );
        printf( "Per Type Report\n" );
        printf( "===============\n" );

        for( nType = 0; nType < 128; nType++ )
        {
            if( anTypeCount[nType] != 0 )
            {
                printf( "Type %s: %d\n", 
                        DGNTypeToName( nType ), 
                        anTypeCount[nType] );
            }
        }

        printf( "\n" );
        printf( "Per Level Report\n" );
        printf( "================\n" );

        for( nLevel = 0; nLevel < 64; nLevel++ )
        {
            if( anLevelCount[nLevel] == 0 )
                continue;

            printf( "Level %d, %d elements:\n", 
                    nLevel, 
                    anLevelCount[nLevel] );

            for( nType = 0; nType < 128; nType++ )
            {
                if( anLevelTypeCount[nLevel * 128 + nType] != 0 )
                {
                    printf( "  Type %s: %d\n", 
                            DGNTypeToName( nType ), 
                            anLevelTypeCount[nLevel*128 + nType] );
                }
            }

            printf( "\n" );
        }
    }

    DGNClose( hDGN );

    return 0;
}
