#include "dted_api.h"
#include <stdio.h>

int main()

{
    DTEDInfo    *psInfo;
    int         iY, iX;
    void        *pStream;
    GInt16      *panData;

/* -------------------------------------------------------------------- */
/*      Open input file.                                                */
/* -------------------------------------------------------------------- */
    psInfo = DTEDOpen( "n43.dt0", "rb", FALSE );
    if( psInfo == NULL )
        exit(1);

/* -------------------------------------------------------------------- */
/*      Create output stream.                                           */
/* -------------------------------------------------------------------- */
    pStream = DTEDCreatePtStream( ".", 0 );

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
                         psInfo->dfULCornerX + iX * psInfo->dfPixelSizeX * 2
                         + psInfo->dfPixelSizeX * 0.5,
                         psInfo->dfULCornerY 
                         - (psInfo->nYSize-iY-1) * psInfo->dfPixelSizeX * 2
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
