#include "aigrid.h"

/************************************************************************/
/*                             DumpMagic()                              */
/*                                                                      */
/*      Dump the magic ``block type byte'' for each existing block.     */
/************************************************************************/

static void DumpMagic( AIGInfo_t * psInfo )

{
    int		i;

    for( i = 0; i < psInfo->nBlocks; i++ )
    {
        GByte	byMagic;
        
        if( psInfo->panBlockSize[i] == 0 )
            continue;

        VSIFSeek( psInfo->fpGrid, psInfo->panBlockOffset[i]+2, SEEK_SET );
        VSIFRead( &byMagic, 1, 1, psInfo->fpGrid );

        printf( "%02x %d/%d\n", byMagic, i, psInfo->panBlockOffset[i] );
    }
}


/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    AIGInfo_t	*psInfo;
    GUInt32 	*panRaster;
    int		i, j;
    int		bMagic = FALSE;

/* -------------------------------------------------------------------- */
/*      Process arguments.                                              */
/* -------------------------------------------------------------------- */
    while( argc > 1 && argv[1][0] == '-' )
    {
        if( EQUAL(argv[1],"-magic") )
            bMagic = TRUE;

        argc--;
        argv++;
    }
    
    if( argc < 2 ) {
        printf( "Usage: aitest coverage\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open dataset.                                                   */
/* -------------------------------------------------------------------- */
    psInfo = AIGOpen( argv[1], "r" );
    if( psInfo == NULL )
        exit( 1 );
    
/* -------------------------------------------------------------------- */
/*      Do we want a dump of all the ``magic'' numbers for              */
/*      instantated blocks?                                             */
/* -------------------------------------------------------------------- */
    if( bMagic)
        DumpMagic( psInfo );
    
/* -------------------------------------------------------------------- */
/*      Read a block, and report it's contents.                         */
/* -------------------------------------------------------------------- */

    panRaster = (GUInt32 *)
        CPLMalloc(psInfo->nBlockXSize * psInfo->nBlockYSize * 4);
    
    AIGReadBlock( psInfo->fpGrid,
                  psInfo->panBlockOffset[1], psInfo->panBlockSize[1],
                  psInfo->nBlockXSize, psInfo->nBlockYSize,
                  panRaster );

    for( j = 0; j < psInfo->nBlockYSize; j++ )
    {
        for( i = 0; i < psInfo->nBlockXSize; i++ )
        {
            if( i > 18 )
            {
                printf( "..." );
                break;
            }

            if( panRaster[i+j*psInfo->nBlockXSize] == GRID_NO_DATA )
                printf( "-*- " );
            else
                printf( "%3d ", panRaster[i+j*psInfo->nBlockXSize] );
        }
        printf( "\n" );
    }

    AIGClose( psInfo );

    exit( 0 );
}
