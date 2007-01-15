/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Functions for translating back and forth between XMLCh and char.
 *           We assume that XMLCh is a simple numeric type that we can 
 *           correspond 1:1 with char values, but that it likely is larger
 *           than a char.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  2004/03/10 18:10:27  warmerda
 * Avoid warning.
 *
 * Revision 1.3  2003/05/21 03:48:35  warmerda
 * Expand tabs
 *
 * Revision 1.2  2002/01/24 17:35:43  warmerda
 * rewrote as a set of simple functions for performance
 *
 * Revision 1.1  2002/01/04 19:46:30  warmerda
 * New
 */

#include "gmlreaderp.h"
#include "cpl_vsi.h"

/************************************************************************/
/*                             tr_strcmp()                              */
/************************************************************************/

int tr_strcmp( const char *pszCString, const XMLCh *panXMLString )

{
    int i = 0;

    while( pszCString[i] != 0 && panXMLString[i] != 0 
           && pszCString[i] == panXMLString[i] ) {}

    if( pszCString[i] == 0 && panXMLString[i] == 0 )
        return 0;
    else if( pszCString[i] < panXMLString[i] )
        return -1;
    else
        return 1;
}

/************************************************************************/
/*                             tr_strcpy()                              */
/************************************************************************/

void tr_strcpy( XMLCh *panXMLString, const char *pszCString )

{
    while( *pszCString != 0 )
        *(panXMLString++) = *(pszCString++);
    *panXMLString = 0;
}

void tr_strcpy( char *pszCString, const XMLCh *panXMLString )

{
    while( *panXMLString != 0 )
        *(pszCString++) = (char) *(panXMLString++);
    *pszCString = 0;
}

/************************************************************************/
/*                             tr_strlen()                              */
/************************************************************************/

int tr_strlen( const XMLCh *panXMLString )

{
    int nLength = 0;
    
    while( *(panXMLString++) != 0 )
        nLength++;

    return nLength;
}

/************************************************************************/
/*                             tr_strdup()                              */
/************************************************************************/

char *tr_strdup( const XMLCh *panXMLString )

{
    char        *pszBuffer;
    int         nLength = tr_strlen( panXMLString );

    pszBuffer = (char *) VSIMalloc(nLength+1);
    tr_strcpy( pszBuffer, panXMLString );

    return pszBuffer;
}
