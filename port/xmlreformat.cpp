#include "cpl_minixml.h"

int main( int argc, char **argv )

{
    CPLXMLNode *poTree;
    char       szXML[1000000];
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
        printf( "%s", CPLSerializeXMLTree( poTree ) );
}
