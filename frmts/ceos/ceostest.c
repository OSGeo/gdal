/******************************************************************************
 * $Id$
 *
 * Project:  CEOS Translator
 * Purpose:  Test mainline.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2001/10/19 15:36:55  warmerda
 * added support for filename on commandline
 *
 * Revision 1.1  1999/05/05 17:32:38  warmerda
 * New
 *
 */

#include "ceosopen.h"

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    CEOSImage	*psCEOS;
    char	data[20000];
    int		i;

    if( nArgc > 1 )
        psCEOS = CEOSOpen( papszArgv[1], "rb" );
    else
        psCEOS = CEOSOpen( "imag_01.dat", "rb" );

    printf( "%d x %d x %d with %d bits/pixel.\n",
            psCEOS->nPixels, psCEOS->nLines, psCEOS->nBands,
            psCEOS->nBitsPerPixel );

    for( i = 0; i < psCEOS->nLines; i++ )
    {
        CEOSReadScanline( psCEOS, 1, i+1, data );
    }

    CEOSClose( psCEOS );
}
