#include <assert.h>
#include "tiffio.h"
#include "aigrid.h"

int main( int argc, char **argv )

{
    const char	*in_file, *out_file;

    if( argc != 3 )
    {
        printf( "Usage: aigrid2tif <in_grid_file> <out_tiff_file>\n" );
        exit( 1 );
    }

    in_file = argv[1];
    out_file = argv[2];

/* -------------------------------------------------------------------- */
/*      Open the input file to get general info.                        */
/* -------------------------------------------------------------------- */
    AIGInfo_t	*psAIG;
    int         nTiles;

    psAIG = AIGOpen( in_file, "r" );

    if( psAIG == NULL )
        exit( 2 );

    nTiles = (psAIG->nLines + 3) / 4;

/* -------------------------------------------------------------------- */
/*      Create the output file with the correct size.  It will          */
/*      contain one strip (line) per input tile in the AIGRID file.     */
/* -------------------------------------------------------------------- */
    int nDataSize = 100;
    GByte *pabyData = (GByte *) CPLCalloc(140,1);
    TIFF *hTIFF;
    
    hTIFF = TIFFOpen( out_file, "w" );

    TIFFSetField( hTIFF, TIFFTAG_COMPRESSION, COMPRESSION_CCITTRLE );
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, 256 );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, nTiles*4 );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, 1 );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );
    TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, 4 );

    for( int iStrip = 0; iStrip < nTiles; iStrip++ )
    {
        int iBlock = iStrip * psAIG->nBlocksPerRow;
        unsigned char abyBlockSize[2], byMagic, byMinSize;

        assert( psAIG->panBlockSize[iBlock] != 0 );

        VSIFSeek( psAIG->fpGrid, psAIG->panBlockOffset[iBlock], SEEK_SET );
        VSIFRead( abyBlockSize, 2, 1, psAIG->fpGrid );
        VSIFRead( &byMagic, 1, 1, psAIG->fpGrid );
        VSIFRead( &byMinSize, 1, 1, psAIG->fpGrid );

        assert( byMagic == 0xff );
        assert( byMinSize <= 4 );

        nDataSize = (abyBlockSize[0]*256 + abyBlockSize[1])*2 - 2 - byMinSize;

        /* skip min value */
        VSIFSeek( psAIG->fpGrid, byMinSize, SEEK_CUR );

        /* read data */
        VSIFRead( pabyData, nDataSize, 1, psAIG->fpGrid );

        /* write to tiff strip */
        TIFFWriteRawStrip( hTIFF, iStrip, pabyData, nDataSize );
    }

    TIFFClose( hTIFF );
    
    
    AIGClose( psAIG );
}

