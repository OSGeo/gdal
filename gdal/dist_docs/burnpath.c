#include <stdio.h>
#include <string.h>

const static int block_size = 10000;

/************************************************************************/
/*                                main()                                */
/************************************************************************/
int main( int argc, char ** argv )

{
    FILE	*fp;
    int		offset, size, overlap;
    const char *marker;
    const char *path;
    const char *targetfile;
    char        blockbuf[block_size+1];

/* -------------------------------------------------------------------- */
/*      Usage message.                                                  */
/* -------------------------------------------------------------------- */
    if( argc < 4 )
    {
        printf( "\n" );
        printf( "Usage: burnpath <targetfile> <marker_string> <path>\n" );
        printf( "\n" );
        printf( "eg. \n" );
        printf( "   %% burnpath /opt/lib/libgdal.1.1.so __INST_DATA_TARGET: /opt/share/gdal\n" );
        exit( 1 );
    }

    targetfile = argv[1];
    marker = argv[2];
    path = argv[3];

    overlap = strlen(marker) + strlen(path) + 1;

/* -------------------------------------------------------------------- */
/*      Open the target file.                                           */
/* -------------------------------------------------------------------- */
    fp = fopen( targetfile, "r+" );
    if( fp == NULL )
    {
        perror( "fopen" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Establish the file length.                                      */
/* -------------------------------------------------------------------- */
    fseek( fp, 0, SEEK_END );
    size = ftell( fp );
    fseek( fp, 0, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Read in the file in overlapping chunks.  We assume the          */
/*      "space" after the marker could be up to 200 bytes.              */
/* -------------------------------------------------------------------- */
    for( offset = 0; offset < size; offset += block_size - overlap )
    {
        int    block_bytes, block_modified = 0, i;

        if( offset + block_size < size )
            block_bytes = block_size;
        else
            block_bytes = size - offset;

        if( fseek( fp, offset, SEEK_SET ) != 0 )
        {
            perror( "fseek" );
            exit( 1 );
        }

        if( fread( blockbuf, block_bytes, 1, fp ) != 1 )
        {
            perror( "fread" );
            exit( 1 );
        }
        blockbuf[block_bytes] = '\0';

        for( i = 0; i < block_bytes - overlap; i++ )
        {
            if( blockbuf[i] == marker[0]
                && strncmp( blockbuf + i, marker, strlen(marker) ) == 0 )
            {
                strcpy( blockbuf+i+strlen(marker), path );
                block_modified = 1;
            }
        }

        if( block_modified )
        {
            if( fseek( fp, offset, SEEK_SET ) != 0 )
            {
                perror( "fseek" );
                exit( 1 );
            }

            if( fwrite( blockbuf, block_bytes, 1, fp ) != 1 )
            {
                perror( "fwrite" );
                exit( 1 );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      We are done.                                                    */
/* -------------------------------------------------------------------- */
    fclose( fp );

    exit( 0 );
}
