/**********************************************************************
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

CPL_CVSID("$Id$")

int main( int argc, char **argv )

{
    // TODO(schwehr): Switch to using std::string.
    static char szXML[20000000] = {};
    FILE *fp = nullptr;

    if( argc == 1 )
    {
        fp = stdin;
    }
    else if( argv[1][0] == '-' )
    {
        printf( "Usage: xmlreformat [filename]\n" );/*ok*/
        exit( 0 );
    }
    else
    {
        fp = fopen( argv[1], "rt" );
        if( fp == nullptr )
        {
            printf( "Failed to open file %s.\n", argv[1] );/*ok*/
            exit( 1 );
        }
    }

    const int nLen = static_cast<int>(fread(szXML, 1, sizeof(szXML), fp));
    if( nLen >= static_cast<int>(sizeof(szXML)) - 2 )
    {
        fprintf( stderr,
                 "xmlreformat fixed sized buffer (%d bytes) exceeded.\n",
                 static_cast<int>(sizeof(szXML)) );
        exit(1);
    }

    if( fp != stdin )
        fclose( fp );

    szXML[nLen] = '\0';

    CPLXMLNode *poTree = CPLParseXMLString( szXML );
    if( poTree != nullptr )
    {
        char *pszRawXML = CPLSerializeXMLTree( poTree );
        printf( "%s", pszRawXML );/*ok*/
        CPLFree( pszRawXML );
        CPLDestroyXMLNode( poTree );
    }

    return 0;
}
