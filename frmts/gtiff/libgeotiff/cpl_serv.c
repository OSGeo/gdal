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
 * cpl_serv.c: Various Common Portability Library derived convenience functions
 *
 * $Log$
 * Revision 1.2  1999/09/08 18:12:55  warmerda
 * reimported
 *
 * Revision 1.4  1999/06/25 04:35:26  warmerda
 * Fixed to actually support long lines.
 *
 * Revision 1.3  1999/03/17 20:43:03  geotiff
 * Avoid use of size_t keyword
 *
 * Revision 1.2  1999/03/10 18:22:39  geotiff
 * Added string.h, fixed backslash escaping
 *
 * Revision 1.1  1999/03/09 15:57:04  geotiff
 * New
 *
 */

#include "cpl_serv.h"
#include "geo_tiffp.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#if defined(HAVE_STRINGS_H) && !defined(HAVE_STRING_H)
#  include <strings.h>
#endif

/************************************************************************/
/*                             CPLCalloc()                              */
/************************************************************************/

void *CPLCalloc( int nCount, int nSize )

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

void *CPLMalloc( int nSize )

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

void * CPLRealloc( void * pData, int nNewSize )

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

    pszReturn = VSIMalloc( strlen(pszString)+1 );
        
    if( pszReturn == NULL )
    {
        CPLError( CE_Fatal, CPLE_OutOfMemory,
                  "CPLStrdup(): Out of memory allocating %d bytes.\n",
                  strlen(pszString) );
        
    }

    strcpy( pszReturn, pszString );
    
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
/************************************************************************/

const char *CPLReadLine( FILE * fp )

{
    static char	*pszRLBuffer = NULL;
    static int	nRLBufferSize = 0;
    int		nLength, nReadSoFar = 0;

/* -------------------------------------------------------------------- */
/*      Loop reading chunks of the line till we get to the end of       */
/*      the line.                                                       */
/* -------------------------------------------------------------------- */
    do {
/* -------------------------------------------------------------------- */
/*      Grow the working buffer if we have it nearly full.  Fail out    */
/*      of read line if we can't reallocate it big enough (for          */
/*      instance for a _very large_ file with no newlines).             */
/* -------------------------------------------------------------------- */
        if( nRLBufferSize-nReadSoFar < 128 )
        {
            nRLBufferSize = nRLBufferSize*2 + 128;
            pszRLBuffer = (char *) VSIRealloc(pszRLBuffer, nRLBufferSize);
            if( pszRLBuffer == NULL )
            {
                nRLBufferSize = 0;
                return NULL;
            }
        }

/* -------------------------------------------------------------------- */
/*      Do the actual read.                                             */
/* -------------------------------------------------------------------- */
        if( VSIFGets( pszRLBuffer+nReadSoFar, nRLBufferSize-nReadSoFar, fp )
            == NULL )
            return NULL;

        nReadSoFar = strlen(pszRLBuffer);

    } while( nReadSoFar == nRLBufferSize - 1
             && pszRLBuffer[nRLBufferSize-2] != 13
             && pszRLBuffer[nRLBufferSize-2] != 10 );

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


/*=====================================================================
                    StringList manipulation functions.
 =====================================================================*/

/**********************************************************************
 *                       CSLAddString()
 *
 * Append a string to a StringList and return a pointer to the modified
 * StringList.
 * If the input StringList is NULL, then a new StringList is created.
 **********************************************************************/
char **CSLAddString(char **papszStrList, const char *pszNewString)
{
    int nItems=0;

    if (pszNewString == NULL)
        return papszStrList;    /* Nothing to do!*/

    /* Allocate room for the new string */
    if (papszStrList == NULL)
        papszStrList = (char**) CPLCalloc(2,sizeof(char*));
    else
    {
        nItems = CSLCount(papszStrList);
        papszStrList = (char**)CPLRealloc(papszStrList, 
                                          (nItems+2)*sizeof(char*));
    }

    /* Copy the string in the list */
    papszStrList[nItems] = CPLStrdup(pszNewString);
    papszStrList[nItems+1] = NULL;

    return papszStrList;
}

/**********************************************************************
 *                       CSLCount()
 *
 * Return the number of lines in a Stringlist.
 **********************************************************************/
int CSLCount(char **papszStrList)
{
    int nItems=0;

    if (papszStrList)
    {
        while(*papszStrList != NULL)
        {
            nItems++;
            papszStrList++;
        }
    }

    return nItems;
}


/************************************************************************/
/*                            CSLGetField()                             */
/*                                                                      */
/*      Fetches the indicated field, being careful not to crash if      */
/*      the field doesn't exist within this string list.  The           */
/*      returned pointer should not be freed, and doesn't               */
/*      necessarily last long.                                          */
/************************************************************************/

const char * CSLGetField( char ** papszStrList, int iField )

{
    int         i;

    if( papszStrList == NULL || iField < 0 )
        return( "" );

    for( i = 0; i < iField+1; i++ )
    {
        if( papszStrList[i] == NULL )
            return "";
    }

    return( papszStrList[iField] );
}

/**********************************************************************
 *                       CSLDestroy()
 *
 * Free all memory used by a StringList.
 **********************************************************************/
void CSLDestroy(char **papszStrList)
{
    char **papszPtr;

    if (papszStrList)
    {
        papszPtr = papszStrList;
        while(*papszPtr != NULL)
        {
            CPLFree(*papszPtr);
            papszPtr++;
        }

        CPLFree(papszStrList);
    }
}


/**********************************************************************
 *                       CSLDuplicate()
 *
 * Allocate and return a copy of a StringList.
 **********************************************************************/
char    **CSLDuplicate(char **papszStrList)
{
    char **papszNewList, **papszSrc, **papszDst;
    int  nLines;

    nLines = CSLCount(papszStrList);

    if (nLines == 0)
        return NULL;

    papszNewList = (char **)CPLMalloc((nLines+1)*sizeof(char*));
    papszSrc = papszStrList;
    papszDst = papszNewList;

    while(*papszSrc != NULL)
    {
        *papszDst = CPLStrdup(*papszSrc);

        papszSrc++;
        papszDst++;
    }
    *papszDst = NULL;

    return papszNewList;
}

/**********************************************************************
 *                       CSLTokenizeString()
 *
 * Tokenizes a string and returns a StringList with one string for
 * each token.
 **********************************************************************/
char    **CSLTokenizeString( const char *pszString )
{
    return CSLTokenizeStringComplex( pszString, " ", TRUE, FALSE );
}

/************************************************************************/
/*                      CSLTokenizeStringComplex()                      */
/*                                                                      */
/*      The ultimate tokenizer?                                         */
/************************************************************************/

char ** CSLTokenizeStringComplex( const char * pszString,
                                  const char * pszDelimiters,
                                  int bHonourStrings, int bAllowEmptyTokens )

{
    char	**papszRetList = NULL;
    char 	*pszToken;
    int		nTokenMax, nTokenLen;

    pszToken = (char *) CPLCalloc(10,1);
    nTokenMax = 10;
    
    while( pszString != NULL && *pszString != '\0' )
    {
        int	bInString = FALSE;

        nTokenLen = 0;
        
        /* Try to find the next delimeter, marking end of token */
        for( ; *pszString != '\0'; pszString++ )
        {

            /* End if this is a delimeter skip it and break. */
            if( !bInString && strchr(pszDelimiters, *pszString) != NULL )
            {
                pszString++;
                break;
            }
            
            /* If this is a quote, and we are honouring constant
               strings, then process the constant strings, with out delim
               but don't copy over the quotes */
            if( bHonourStrings && *pszString == '"' )
            {
                if( bInString )
                {
                    bInString = FALSE;
                    continue;
                }
                else
                {
                    bInString = TRUE;
                    continue;
                }
            }

            /* Within string constants we allow for escaped quotes, but
               in processing them we will unescape the quotes */
            if( bInString && pszString[0] == '\\' && pszString[1] == '"' )
            {
                pszString++;
            }

            /* Within string constants a \\ sequence reduces to \ */
            else if( bInString
                     && pszString[0] == '\\' && pszString[1] == '\\' )
            {
                pszString++;
            }

            if( nTokenLen >= nTokenMax-1 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = (char *) CPLRealloc( pszToken, nTokenMax );
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        pszToken[nTokenLen] = '\0';

        if( pszToken[0] != '\0' || bAllowEmptyTokens )
        {
            papszRetList = CSLAddString( papszRetList, pszToken );
        }
    }

    if( papszRetList == NULL )
        papszRetList = (char **) CPLCalloc(sizeof(char *),1);

    CPLFree( pszToken );

    return papszRetList;
}

/* static buffer to store the last error message.  We'll assume that error
 * messages cannot be longer than 2000 chars... which is quite reasonable
 * (that's 25 lines of 80 chars!!!)
 */
static char gszCPLLastErrMsg[2000] = "";
static int  gnCPLLastErrNo = 0;

static void (*gpfnCPLErrorHandler)(CPLErr, int, const char *) = NULL;

/**********************************************************************
 *                          CPLError()
 *
 * This function records an error code and displays the error message
 * to stderr.
 *
 * The error code can be accessed later using CPLGetLastErrNo()
 **********************************************************************/
void    CPLError(CPLErr eErrClass, int err_no, const char *fmt, ...)
{
    va_list args;

    /* Expand the error message 
     */
    va_start(args, fmt);
    vsprintf(gszCPLLastErrMsg, fmt, args);
    va_end(args);

    /* If the user provided his own error handling function, then call
     * it, otherwise print the error to stderr and return.
     */
    gnCPLLastErrNo = err_no;

    if (gpfnCPLErrorHandler != NULL)
    {
        gpfnCPLErrorHandler(eErrClass, err_no, gszCPLLastErrMsg);
    }
    else
    {
        fprintf(stderr, "ERROR %d: %s\n", gnCPLLastErrNo, gszCPLLastErrMsg);
    }

    if( eErrClass == CE_Fatal )
        abort();
}

/**********************************************************************
 *                          CPLErrorReset()
 *
 * Erase any traces of previous errors.
 **********************************************************************/
void    CPLErrorReset()
{
    gnCPLLastErrNo = 0;
    gszCPLLastErrMsg[0] = '\0';
}


/**********************************************************************
 *                          CPLGetLastErrorNo()
 *
 **********************************************************************/
int     CPLGetLastErrorNo()
{
    return gnCPLLastErrNo;
}

/**********************************************************************
 *                          CPLGetLastErrorMsg()
 *
 **********************************************************************/
const char* CPLGetLastErrorMsg()
{
    return gszCPLLastErrMsg;
}

/**********************************************************************
 *                          CPLSetErrorHandler()
 *
 * Allow the library's user to specify his own error handler function.
 *
 * A valid error handler is a C function with the following prototype:
 *
 *     void MyErrorHandler(int errno, const char *msg)
 *
 * Pass NULL to come back to the default behavior.
 **********************************************************************/

void     CPLSetErrorHandler(void (*pfnErrorHandler)(CPLErr, int, const char *))
{
    gpfnCPLErrorHandler = pfnErrorHandler;
}

/************************************************************************/
/*                             _CPLAssert()                             */
/*                                                                      */
/*      This function is called only when an assertion fails.           */
/************************************************************************/

void _CPLAssert( const char * pszExpression, const char * pszFile,
                 int iLine )

{
    CPLError( CE_Fatal, CPLE_AssertionFailed,
              "Assertion `%s' failed\n"
              "in file `%s', line %d\n",
              pszExpression, pszFile, iLine );
}
