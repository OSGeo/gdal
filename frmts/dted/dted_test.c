#include "gdal.h"
#include "dted_api.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: dted_test [-trim] [-fill n] [-level n] <in_file> [<out_level>]\n" );
    exit(0);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/
int main( int argc, char ** argv )

{
    GDALDatasetH hSrcDS;
    int         iY, iX, nOutLevel=0, nXSize, nYSize, iArg, nFillDist=0;
    void        *pStream;
    GInt16      *panData;
    const char  *pszFilename = NULL;
    GDALRasterBandH hSrcBand;
    double       adfGeoTransform[6];
    int          bEnableTrim = FALSE;

/* -------------------------------------------------------------------- */
/*      Identify arguments.                                             */
/* -------------------------------------------------------------------- */

    for( iArg = 1; iArg < argc; iArg++ )
    {
        if( EQUAL(argv[iArg],"-trim") )
            bEnableTrim = TRUE;

        else if( EQUAL(argv[iArg],"-fill") )
            nFillDist = atoi(argv[++iArg]);

        else if( EQUAL(argv[iArg],"-level") )
            nOutLevel = atoi(argv[++iArg]);
        else
        {
            if( pszFilename != NULL )
                Usage();
            pszFilename = argv[iArg];
        }
    }		

    if( pszFilename == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open input file.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    hSrcDS = GDALOpen( pszFilename, GA_ReadOnly );
    if( hSrcDS == NULL )
        exit(1);

    hSrcBand = GDALGetRasterBand( hSrcDS, 1 );

    nXSize = GDALGetRasterXSize( hSrcDS );
    nYSize = GDALGetRasterYSize( hSrcDS );

    GDALGetGeoTransform( hSrcDS, adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Create output stream.                                           */
/* -------------------------------------------------------------------- */
    pStream = DTEDCreatePtStream( ".", nOutLevel );

    if( pStream == NULL )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Process all the profiles.                                       */
/* -------------------------------------------------------------------- */
    panData = (GInt16 *) malloc(sizeof(GInt16) * nXSize);

    for( iY = 0; iY < nYSize; iY++ )
    {
        GDALRasterIO( hSrcBand, GF_Read, 0, iY, nXSize, 1, 
                      panData, nXSize, 1, GDT_Int16, 0, 0 );
                      
        for( iX = 0; iX < nXSize; iX++ )
        {
            DTEDWritePt( pStream, 
                         adfGeoTransform[0] 
                         + adfGeoTransform[1] * (iX + 0.5)
                         + adfGeoTransform[2] * (iY + 0.5),
                         adfGeoTransform[3] 
                         + adfGeoTransform[4] * (iX + 0.5)
                         + adfGeoTransform[5] * (iY + 0.5),
                         panData[iX] );
        }
    }

    free( panData );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    if( bEnableTrim )
        DTEDPtStreamTrimEdgeOnlyTiles( pStream );

    if( nFillDist > 0 )
        DTEDFillPtStream( pStream, nFillDist );

    DTEDClosePtStream( pStream );
    GDALClose( hSrcDS );

    exit( 0 );
}
