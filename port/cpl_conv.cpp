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
 * cpl_conv.c: Various CPL convenience functions (from cpl_conv.h).
 *
 * $Log$
 * Revision 1.4  1999/01/02 20:29:53  warmerda
 * Allow zero length allocations
 *
 * Revision 1.3  1998/12/15 19:01:07  warmerda
 * Added CPLReadLine().
 *
 * Revision 1.2  1998/12/03 18:30:04  warmerda
 * Use CPLError() instead of GPSError().
 *
 * Revision 1.1  1998/12/02 19:33:23  warmerda
 * New
 *
 */

#include "cpl_conv.h"

/************************************************************************/
/*                             CPLCalloc()                              */
/************************************************************************/

void *CPLCalloc( size_t nCount, size_t nSize )

{
    void	*pReturn;

    if( nSize == 0 )
        return NULL;
    
    pReturn = VSICalloc( nCount, nSize );
    if( pReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLCalloc(): Out of memory allocating %d bytes.\n",
                  nSize * nCount );
    }

    return pReturn;
}

/************************************************************************/
/*                             CPLMalloc()                              */
/************************************************************************/

void *CPLMalloc( size_t nSize )

{
    void	*pReturn;

    if( nSize == 0 )
        return NULL;
    
    pReturn = VSIMalloc( nSize );
    if( pReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLMalloc(): Out of memory allocating %d bytes.\n",
                  nSize );
    }

    return pReturn;
}

/************************************************************************/
/*                             CPLRealloc()                             */
/************************************************************************/

void * CPLRealloc( void * pData, size_t nNewSize )

{
    void	*pReturn;

    if( pData == NULL )
        pReturn = VSIMalloc( nNewSize );
    else
        pReturn = VSIRealloc( pData, nNewSize );
    
    if( pReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLRealloc(): Out of memory allocating %d bytes.\n",
                  nNewSize );
    }

    return pReturn;
}

/************************************************************************/
/*                             CPLStrdup()                              */
/************************************************************************/

char *CPLStrdup( const char * pszString )

{
    char	*pszReturn;

    if( pszString == NULL )
        pszString = "";

    pszReturn = VSIStrdup( pszString );
        
    if( pszReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLStrdup(): Out of memory allocating %d bytes.\n",
                  strlen(pszString) );
        
    }
    
    return( pszReturn );
}

/************************************************************************/
/*                            CPLReadLine()                             */
/*                                                                      */
/*      Read a line of text from the given file handle, taking care     */
/*      to capture CR and/or LF and strip off ... equivelent of         */
/*      DKReadLine().  Pointer to an internal buffer is returned.       */
/*      The application shouldn't free it, or depend on it's value      */
/*      past the next call to CPLReadLine()                             */
/*                                                                      */
/*      TODO: Allow arbitrarily long lines ... currently limited to     */
/*      512 characters.                                                 */
/************************************************************************/

const char *CPLReadLine( FILE * fp )

{
    static char	*pszRLBuffer = NULL;
    static int	nRLBufferSize = 0;
    int		nLength;

/* -------------------------------------------------------------------- */
/*      Allocate our working buffer.  Eventually this should grow as    */
/*      needed ... we will implement that aspect later.                 */
/* -------------------------------------------------------------------- */
    if( nRLBufferSize < 512 )
    {
        nRLBufferSize = 512;
        pszRLBuffer = (char *) CPLRealloc(pszRLBuffer, nRLBufferSize);
    }

/* -------------------------------------------------------------------- */
/*      Do the actual read.                                             */
/* -------------------------------------------------------------------- */
    if( VSIFGets( pszRLBuffer, nRLBufferSize, fp ) == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Clear CR and LF off the end.                                    */
/* -------------------------------------------------------------------- */
    nLength = strlen(pszRLBuffer);
    if( nLength > 0
        && (pszRLBuffer[nLength-1] == 10 || pszRLBuffer[nLength-1] == 13) )
    {
        pszRLBuffer[--nLength] = '\0';
    }
    
    if( nLength > 0
        && (pszRLBuffer[nLength-1] == 10 || pszRLBuffer[nLength-1] == 13) )
    {
        pszRLBuffer[--nLength] = '\0';
    }

    return( pszRLBuffer );
}

