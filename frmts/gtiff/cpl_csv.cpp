/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * cpl_csv.c: Support functions for accessing CSV files.
 *
 * $Log$
 * Revision 1.1  1999/01/05 16:52:36  warmerda
 * New
 *
 */

#include "cpl_csv.h"

/************************************************************************/
/*                          CSVReadParseLine()                          */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/************************************************************************/

char **CSVReadParseLine( FILE * fp )

{
    const char	*pszLine;

    CPLAssert( fp != NULL );
    if( fp == NULL )
        return( NULL );
    
    pszLine = CPLReadLine( fp );

    if( pszLine == NULL )
        return( NULL );

    return( CSLTokenizeStringComplex( pszLine, ",", TRUE, TRUE ) );
}

/************************************************************************/
/*                            CSVScanLines()                            */
/*                                                                      */
/*      Read the file scanline for lines where the key field equals     */
/*      the indicated value with the suggested comparison criteria.     */
/*      Return the first matching line split into fields.               */
/************************************************************************/

char **CSVScanLines( FILE *fp, int iKeyField, const char * pszValue,
                     CSVCompareCriteria eCriteria )

{
    char	**papszFields = NULL;
    int		bSelected = FALSE, nTestValue;

    CPLAssert( pszValue != NULL );
    CPLAssert( iKeyField >= 0 );
    CPLAssert( fp != NULL );
    
    nTestValue = atoi(pszValue);
    
    while( !bSelected ) {
        papszFields = CSVReadParseLine( fp );
        if( papszFields == NULL )
            return( NULL );

        if( CSLCount( papszFields ) < iKeyField+1 )
        {
            /* not selected */
        }
        else if( eCriteria == CC_ExactString
                 && strcmp( pszValue, papszFields[iKeyField] ) == 0 )
        {
            bSelected = TRUE;
        }
        else if( eCriteria == CC_ApproxString
                 && EQUAL( pszValue, papszFields[iKeyField] ) )
        {
            bSelected = TRUE;
        }
        else if( eCriteria == CC_Integer
                 && atoi(papszFields[iKeyField]) == nTestValue )
        {
            bSelected = TRUE;
        }

        if( !bSelected )
        {
            CSLDestroy( papszFields );
            papszFields = NULL;
        }
    }
    
    return( papszFields );
}

/************************************************************************/
/*                            CSVScanFile()                             */
/*                                                                      */
/*      Scan a whole file using criteria similar to above, but also     */
/*      taking care of file opening and closing.                        */
/************************************************************************/

char **CSVScanFile( const char * pszFilename, int iKeyField,
                    const char * pszValue, CSVCompareCriteria eCriteria )

{
    FILE	*fp;
    char	**papszResult;

    CPLAssert( pszFilename != NULL );

    fp = VSIFOpen( pszFilename, "r" );
    if( fp == NULL )
        return NULL;

    papszResult = CSVScanLines( fp, iKeyField, pszValue, eCriteria );
    
    VSIFClose( fp );
        
    return( papszResult );
}
