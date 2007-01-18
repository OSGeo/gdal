#include "cpl_minixml.h"
#include "cpl_conv.h"


int main( int argc, char **argv )

{
    CPLXMLNode *poTree;
    static char  szXML[10000000];
    FILE       *fp;
    int        nLen;

    if( argc == 1 )
        fp = stdin;
    else if( argv[1][0] == '-' )
    {
        printf( "Usage: xmlreformat [filename]\n" );
        exit( 0 );
    }
    else
    {
        fp = fopen( argv[1], "rt" );
        if( fp == NULL )
        {
            printf( "Failed to open file %s.\n", argv[1] );
            exit( 1 );
        }
    }

    nLen = fread( szXML, 1, sizeof(szXML), fp );

    if( fp != stdin )
        fclose( fp );

    szXML[nLen] = '\0';

    poTree = CPLParseXMLString( szXML );
    if( poTree != NULL )
    {
        char *pszRawXML = CPLSerializeXMLTree( poTree );
        printf( "%s", pszRawXML );
        CPLFree( pszRawXML );
        CPLDestroyXMLNode( poTree );
    }

    return 0;
}
