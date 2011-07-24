/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Testing mainline for HFA services - transitory.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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
 ****************************************************************************/

#include "hfa_p.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "hfatest [-dd] [-dt] [-dr] filename\n" );
}

/************************************************************************/
/* Stub for HFAPCSStructToWKT, defined in hfadataset.cpp but used by    */
/* hfaopen.cpp                                                          */
/************************************************************************/
#ifndef WIN32
char *
HFAPCSStructToWKT( const Eprj_Datum *psDatum,
                   const Eprj_ProParameters *psPro,
                   const Eprj_MapInfo *psMapInfo,
                   HFAEntry *poMapInformation )
{
    return NULL;
}
#endif

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    const char	*pszFilename = NULL;
    int		nDumpTree = FALSE;
    int		nDumpDict = FALSE;
    int		nRastReport = FALSE;
    int		i, nXSize, nYSize, nBands;
    HFAHandle	hHFA;
    const Eprj_MapInfo *psMapInfo;
    const Eprj_ProParameters *psProParameters;
    const Eprj_Datum *psDatum;

/* -------------------------------------------------------------------- */
/*      Handle arguments.                                               */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"-dd") )
            nDumpDict = TRUE;
        else if( EQUAL(argv[i],"-dt") )
            nDumpTree = TRUE;
        else if( EQUAL(argv[i],"-dr") )
            nRastReport = TRUE;
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
        HFADumpDictionary( hHFA, stdout );
    }

/* -------------------------------------------------------------------- */
/*      Do we want to walk the tree dumping out general information?    */
/* -------------------------------------------------------------------- */
    if( nDumpTree )
    {
        HFADumpTree( hHFA, stdout );
    }

/* -------------------------------------------------------------------- */
/*      Dump indirectly collected data about bands.                     */
/* -------------------------------------------------------------------- */
    HFAGetRasterInfo( hHFA, &nXSize, &nYSize, &nBands );

    if( nRastReport )
    {
        printf( "Raster Size = %d x %d\n", nXSize, nYSize );

        for( i = 1; i <= nBands; i++ )
        {
            int	nDataType, nColors, nOverviews, iOverview;
            double	*padfRed, *padfGreen, *padfBlue, *padfAlpha, *padfBins;
            int nBlockXSize, nBlockYSize, nCompressionType;
        
            HFAGetBandInfo( hHFA, i, &nDataType, &nBlockXSize, &nBlockYSize, 
                            &nCompressionType );
            nOverviews = HFAGetOverviewCount( hHFA, i );

            printf( "Band %d: %dx%d tiles, type = %d\n",
                    i, nBlockXSize, nBlockYSize, nDataType );

            for( iOverview=0; iOverview < nOverviews; iOverview++ )
            {
                HFAGetOverviewInfo( hHFA, i, iOverview, 
                                    &nXSize, &nYSize, 
                                    &nBlockXSize, &nBlockYSize, NULL );
                printf( "  Overview: %dx%d (blocksize %dx%d)\n", 
                        nXSize, nYSize, nBlockXSize, nBlockYSize );
            }

            if( HFAGetPCT( hHFA, i, &nColors, &padfRed, &padfGreen, 
			   &padfBlue, &padfAlpha, &padfBins )
                == CE_None )
            {
                int	j;

                for( j = 0; j < nColors; j++ )
                {
                    printf( "PCT[%d] = %f,%f,%f %f\n",
                            (padfBins != NULL) ? (int) padfBins[j] : j,
                            padfRed[j], padfGreen[j], 
			    padfBlue[j], padfAlpha[j]);
                }
            }

/* -------------------------------------------------------------------- */
/*      Report statistics.  We need to dig directly into the C++ API.   */
/* -------------------------------------------------------------------- */
            HFABand *poBand = hHFA->papoBand[i-1];
            HFAEntry *poStats = poBand->poNode->GetNamedChild( "Statistics" );

            if( poStats != NULL )
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
                printf( "   No Statistics found.\n" );
        }

/* -------------------------------------------------------------------- */
/*      Dump the map info structure.                                    */
/* -------------------------------------------------------------------- */
        psMapInfo = HFAGetMapInfo( hHFA );

        if( psMapInfo != NULL )
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
    
    psProParameters = HFAGetProParameters( hHFA );

    psDatum = HFAGetDatum( hHFA );
    
    HFAClose( hHFA );

    VSICleanupFileManager();
    CPLCleanupTLS();

#ifdef DBMALLOC
    malloc_dump(1);
#endif

    exit( 0 );
}
