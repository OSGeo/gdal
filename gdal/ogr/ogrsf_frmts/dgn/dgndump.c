/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Temporary low level DGN dumper application.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Avenza Systems Inc, http://www.avenza.com/
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

#include "dgnlibp.h"

CPL_CVSID("$Id$");

static void DGNDumpRawElement( DGNHandle hDGN, DGNElemCore *psCore,
                               FILE *fpOut );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: dgndump [-e xmin ymin xmax ymax] [-s] [-r n] filename.dgn\n" );
    printf( "\n" );
    printf( "  -e xmin ymin xmax ymax: only get elements within extents.\n" );
    printf( "  -s: produce summary report of element types and levels.\n");
    printf( "  -r n: report raw binary contents of elements of type n.\n");

    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    DGNHandle   hDGN;
    DGNElemCore *psElement;
    const char  *pszFilename = NULL;
    int         bSummary = FALSE, iArg, bRaw = FALSE, bReportExtents = FALSE;
    char        achRaw[128];
    double      dfSFXMin=0.0, dfSFXMax=0.0, dfSFYMin=0.0, dfSFYMax=0.0;

    memset( achRaw, 0, 128 );

    for( iArg = 1; iArg < argc; iArg++ )
    {
        if( strcmp(argv[iArg],"-s") == 0 )
        {
            bSummary = TRUE;
        }
        else if( strcmp(argv[iArg],"-e") == 0 && iArg < argc-4 )
        {
            dfSFXMin = atof(argv[iArg+1]);
            dfSFYMin = atof(argv[iArg+2]);
            dfSFXMax = atof(argv[iArg+3]);
            dfSFYMax = atof(argv[iArg+4]);
            iArg += 4;
        }
        else if( strcmp(argv[iArg],"-r") == 0 && iArg < argc-1 )
        {
            achRaw[MAX(0,MIN(127,atoi(argv[iArg+1])))] = 1;
            bRaw = TRUE;
            iArg++;
        }
        else if( strcmp(argv[iArg],"-extents") == 0 )
        {
            bReportExtents = TRUE;
        }
        else if( argv[iArg][0] == '-' || pszFilename != NULL )
            Usage();
        else 
            pszFilename = argv[iArg];
    }

    if( pszFilename == NULL )
        Usage();

    hDGN = DGNOpen( pszFilename, FALSE );
    if( hDGN == NULL )
        exit( 1 );

    if( bRaw )
        DGNSetOptions( hDGN, DGNO_CAPTURE_RAW_DATA );

    DGNSetSpatialFilter( hDGN, dfSFXMin, dfSFYMin, dfSFXMax, dfSFYMax );

    if( !bSummary )
    {
        while( (psElement=DGNReadElement(hDGN)) != NULL )
        {
            DGNDumpElement( hDGN, psElement, stdout );

            CPLAssert( psElement->type >= 0 && psElement->type < 128 );

            if( achRaw[psElement->type] != 0 )
                DGNDumpRawElement( hDGN, psElement, stdout );

            if( bReportExtents )
            {
                DGNPoint sMin, sMax;
                if( DGNGetElementExtents( hDGN, psElement, &sMin, &sMax ) )
                    printf( "  Extents: (%.6f,%.6f,%.6f)\n"
                            "        to (%.6f,%.6f,%.6f)\n",
                            sMin.x, sMin.y, sMin.z, 
                            sMax.x, sMax.y, sMax.z );
            }

            DGNFreeElement( hDGN, psElement );
        }
    }
    else
    {
        const DGNElementInfo    *pasEI;
        int                     nCount, i, nLevel, nType;
        int                     anLevelTypeCount[128*64];
        int                     anLevelCount[64];
        int                     anTypeCount[128];
        double                  adfExtents[6];

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

/************************************************************************/
/*                         DGNDumpRawElement()                          */
/************************************************************************/

static void DGNDumpRawElement( DGNHandle hDGN, DGNElemCore *psCore, 
                               FILE *fpOut )

{
    int         i, iChar = 0;
    char        szLine[80];

    fprintf( fpOut, "  Raw Data (%d bytes):\n", psCore->raw_bytes );
    for( i = 0; i < psCore->raw_bytes; i++ )
    {
        char    szHex[3];

        if( (i % 16) == 0 )                                             
        {
            sprintf( szLine, "%6d: %71s", i, " " );
            iChar = 0;
        }

        sprintf( szHex, "%02x", psCore->raw_data[i] );
        strncpy( szLine+8+iChar*2, szHex, 2 );
        
        if( psCore->raw_data[i] < 32 || psCore->raw_data[i] > 127 )
            szLine[42+iChar] = '.';
        else
            szLine[42+iChar] = psCore->raw_data[i];

        if( i == psCore->raw_bytes - 1 || (i+1) % 16 == 0 )
        {
            fprintf( fpOut, "%s\n", szLine );
        }

        iChar++;
    }
}



