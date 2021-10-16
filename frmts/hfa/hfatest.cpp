/******************************************************************************
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Testing mainline for HFA services - transitory.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "hfa_p.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "hfatest [-dd] [-dt] [-dr] filename\n" );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
/* -------------------------------------------------------------------- */
/*      Handle arguments.                                               */
/* -------------------------------------------------------------------- */
    const char *pszFilename = nullptr;
    bool bDumpTree = false;
    bool bDumpDict = false;
    bool bRastReport = false;

    for( int i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "-dd") )
        {
            bDumpDict = true;
        }
        else if( EQUAL(argv[i], "-dt") )
        {
            bDumpTree = true;
        }
        else if( EQUAL(argv[i], "-dr") )
        {
            bRastReport = true;
        }
        else if( pszFilename == nullptr )
        {
            pszFilename = argv[i];
        }
        else
        {
            Usage();
            exit( 1 );
        }
    }

    if( pszFilename == nullptr )
    {
        Usage();
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    HFAHandle hHFA = HFAOpen( pszFilename, "r" );

    if( hHFA == nullptr )
    {
        printf( "HFAOpen() failed.\n" );
        exit( 100 );
    }

/* -------------------------------------------------------------------- */
/*      Do we want to walk the tree dumping out general information?    */
/* -------------------------------------------------------------------- */
    if( bDumpDict )
    {
        HFADumpDictionary( hHFA, stdout );
    }

/* -------------------------------------------------------------------- */
/*      Do we want to walk the tree dumping out general information?    */
/* -------------------------------------------------------------------- */
    if( bDumpTree )
    {
        HFADumpTree( hHFA, stdout );
    }

/* -------------------------------------------------------------------- */
/*      Dump indirectly collected data about bands.                     */
/* -------------------------------------------------------------------- */
    int nXSize = 0;
    int nYSize = 0;
    int nBands = 0;
    HFAGetRasterInfo( hHFA, &nXSize, &nYSize, &nBands );

    if( bRastReport )
    {
        printf( "Raster Size = %d x %d\n", nXSize, nYSize );

        for( int i = 1; i <= nBands; i++ )
        {
            EPTType eDataType;
            int nBlockXSize = 0;
            int nBlockYSize = 0;
            int nCompressionType = 0;

            HFAGetBandInfo( hHFA, i, &eDataType, &nBlockXSize, &nBlockYSize,
                            &nCompressionType );
            const int nOverviews = HFAGetOverviewCount( hHFA, i );

            printf( "Band %d: %dx%d tiles, type = %d\n",
                    i, nBlockXSize, nBlockYSize, eDataType );

            for( int iOverview=0; iOverview < nOverviews; iOverview++ )
            {
                HFAGetOverviewInfo( hHFA, i, iOverview,
                                    &nXSize, &nYSize,
                                    &nBlockXSize, &nBlockYSize, nullptr );
                printf( "  Overview: %dx%d (blocksize %dx%d)\n",
                        nXSize, nYSize, nBlockXSize, nBlockYSize );
            }

            int nColors = 0;
            double *padfRed = nullptr;
            double *padfGreen = nullptr;
            double *padfBlue = nullptr;
            double *padfAlpha = nullptr;
            double *padfBins = nullptr;
            if( HFAGetPCT( hHFA, i, &nColors, &padfRed, &padfGreen,
                           &padfBlue, &padfAlpha, &padfBins )
                == CE_None )
            {
                for( int j = 0; j < nColors; j++ )
                {
                    printf( "PCT[%d] = %f,%f,%f %f\n",
                            (padfBins != nullptr)
                            ? static_cast<int>(padfBins[j])
                            : j,
                            padfRed[j], padfGreen[j],
                            padfBlue[j], padfAlpha[j]);
                }
            }

/* -------------------------------------------------------------------- */
/*      Report statistics.  We need to dig directly into the C++ API.   */
/* -------------------------------------------------------------------- */
            HFABand *poBand = hHFA->papoBand[i-1];
            HFAEntry *poStats = poBand->poNode->GetNamedChild( "Statistics" );

            if( poStats != nullptr )
            {
                printf( "  Min: %g   Max: %g   Mean: %g\n",
                        poStats->GetDoubleField( "minimum" ),
                        poStats->GetDoubleField( "maximum" ),
                        poStats->GetDoubleField( "mean" ) );
                printf( "  Median: %g   Mode: %g   Stddev: %g\n",
                        poStats->GetDoubleField( "median" ),
                        poStats->GetDoubleField( "mode" ),
                        poStats->GetDoubleField( "stddev" ) );
            }
            else
            {
                printf( "   No Statistics found.\n" );
            }
        }

/* -------------------------------------------------------------------- */
/*      Dump the map info structure.                                    */
/* -------------------------------------------------------------------- */
        const Eprj_MapInfo *psMapInfo = HFAGetMapInfo( hHFA );

        if( psMapInfo != nullptr )
        {
            printf( "MapInfo.proName = %s\n", psMapInfo->proName );
            printf( "MapInfo.upperLeftCenter.x = %.2f\n",
                    psMapInfo->upperLeftCenter.x );
            printf( "MapInfo.upperLeftCenter.y = %.2f\n",
                    psMapInfo->upperLeftCenter.y );
        }
        else
        {
            printf( "No Map Info found\n" );
        }
    }

    // const Eprj_ProParameters *psProParameters;
    // psProParameters =
    HFAGetProParameters( hHFA );

    // const Eprj_Datum *psDatum;
    // psDatum =
    HFAGetDatum( hHFA );

    HFAClose( hHFA );

    VSICleanupFileManager();
    CPLCleanupTLS();

    exit( 0 );
}
