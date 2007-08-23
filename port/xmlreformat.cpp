/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  XML Reformatting - mostly for testing minixml implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

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

