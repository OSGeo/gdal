#include "aigrid.h"

/************************************************************************/
/*                             DumpMagic()                              */
/*                                                                      */
/*      Dump the magic ``block type byte'' for each existing block.     */
/************************************************************************/

static void DumpMagic( AIGInfo_t * psInfo, int bVerbose )

{
    int		i;

    for( i = 0; i < psInfo->nBlocks; i++ )
    {
        GByte	byMagic;
        int	bReport = bVerbose;
        
        if( psInfo->panBlockSize[i] == 0 )
            continue;

        VSIFSeek( psInfo->fpGrid, psInfo->panBlockOffset[i]+2, SEEK_SET );

        if( psInfo->nCellType == AIG_CELLTYPE_INT )
        {
            VSIFRead( &byMagic, 1, 1, psInfo->fpGrid );

            if( byMagic != 0 && byMagic != 0x43 && byMagic != 0x04
                && byMagic != 0x08 && byMagic != 0x10 && byMagic != 0xd7 
                && byMagic != 0xdf && byMagic != 0xe0 && byMagic != 0xfc
                && byMagic != 0xf8 && byMagic != 0xff && byMagic != 0x41
                && byMagic != 0x40 && byMagic != 0x42 && byMagic != 0xf0
                && byMagic != 0xcf && byMagic != 0x01 )
                bReport = TRUE;

            if( byMagic == 0 && psInfo->panBlockSize[i] > 8 )
                bReport = TRUE;
        }
        else
        {
            if( psInfo->panBlockSize[i] !=
                psInfo->nBlockXSize*psInfo->nBlockYSize*sizeof(float) )
                bReport = TRUE;
        }

        if( bReport )
        {
            printf( " %02x %d/%d@%d\n", byMagic, i,
                    psInfo->panBlockSize[i],
                    psInfo->panBlockOffset[i] );
        }
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
        printf( "Usage: aitest [-magic] coverage\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open dataset.                                                   */
/* -------------------------------------------------------------------- */
    psInfo = AIGOpen( argv[1], "r" );
    if( psInfo == NULL )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Dump general information                                        */
/* -------------------------------------------------------------------- */
    printf( "%d pixels x %d lines.\n", psInfo->nPixels, psInfo->nLines );
    printf( "Lower Left = (%f,%f)   Upper Right = (%f,%f)\n",
            psInfo->dfLLX,
            psInfo->dfLLY,
            psInfo->dfURX,
            psInfo->dfURY );

    if( psInfo->nCellType == AIG_CELLTYPE_INT )
        printf( "Integer coverage, %dx%d blocks.\n",
                psInfo->nBlockXSize, psInfo->nBlockYSize );
    else
        printf( "Floating point coverage, %dx%d blocks.\n",
                psInfo->nBlockXSize, psInfo->nBlockYSize );

    printf( "Stats - Min=%f, Max=%f, Mean=%f, StdDev=%f\n",
            psInfo->dfMin,
            psInfo->dfMax,
            psInfo->dfMean,
            psInfo->dfStdDev );
    
/* -------------------------------------------------------------------- */
/*      Do we want a dump of all the ``magic'' numbers for              */
/*      instantated blocks?                                             */
/* -------------------------------------------------------------------- */
    DumpMagic( psInfo, bMagic );
    
/* -------------------------------------------------------------------- */
/*      Read a block, and report it's contents.                         */
/* -------------------------------------------------------------------- */
    panRaster = (GUInt32 *)
        CPLMalloc(psInfo->nBlockXSize * psInfo->nBlockYSize * 4);
    
    while( argc > 2 && (atoi(argv[2]) > 0 || argv[2][0] == '0') )
    {
        int	nBlock = atoi(argv[2]);

        argv++;
        argc--;
        
        AIGReadBlock( psInfo->fpGrid,
                      psInfo->panBlockOffset[nBlock],
                      psInfo->panBlockSize[nBlock],
                      psInfo->nBlockXSize, psInfo->nBlockYSize,
                      panRaster, psInfo->nCellType );

        printf( "\nBlock %d:\n", nBlock );
        
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
                else if( psInfo->nCellType == AIG_CELLTYPE_FLOAT )
                    printf( "%f ",
                            ((float *) panRaster)[i+j*psInfo->nBlockXSize] );
                else 
                    printf( "%3d ", panRaster[i+j*psInfo->nBlockXSize] );
            }
            printf( "\n" );
        }
    }

    CPLFree( panRaster );

    AIGClose( psInfo );

    exit( 0 );
}
