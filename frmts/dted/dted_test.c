#include "dted_api.h"
#include <stdio.h>

int main( int argc, char ** argv )

{
    DTEDInfo    *psInfo;
    int         iY, iX, nOutLevel;
    void        *pStream;
    GInt16      *panData;
    const char  *pszFilename;

    if( argc > 1 )
        pszFilename = argv[1];
    else
    {
        printf( "Usage: dted_test <in_file> [<out_level>]\n" );
        exit(0);
    }

    if( argc > 2 )
        nOutLevel = atoi(argv[2]);
    else
        nOutLevel = 0;
    
/* -------------------------------------------------------------------- */
/*      Open input file.                                                */
/* -------------------------------------------------------------------- */
    psInfo = DTEDOpen( pszFilename, "rb", FALSE );
    if( psInfo == NULL )
        exit(1);

/* -------------------------------------------------------------------- */
/*      Create output stream.                                           */
/* -------------------------------------------------------------------- */
    pStream = DTEDCreatePtStream( ".", nOutLevel );

    if( pStream == NULL )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Process all the profiles.                                       */
/* -------------------------------------------------------------------- */
    panData = (GInt16 *) malloc(sizeof(GInt16) * psInfo->nYSize);

    for( iX = 0; iX < psInfo->nXSize; iX++ )
    {
        DTEDReadProfile( psInfo, iX, panData );
        
        for( iY = 0; iY < psInfo->nYSize; iY++ )
        {
            DTEDWritePt( pStream, 
                         psInfo->dfULCornerX + iX * psInfo->dfPixelSizeX
                         + psInfo->dfPixelSizeX * 0.5,
                         psInfo->dfULCornerY 
                         - (psInfo->nYSize-iY-1) * psInfo->dfPixelSizeX
                         - psInfo->dfPixelSizeY * 0.5,
                         panData[iY] );
        }
    }

    free( panData );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    DTEDFillPtStream( pStream, 2 );
    DTEDClosePtStream( pStream );
    DTEDClose( psInfo );

    exit( 0 );
}
