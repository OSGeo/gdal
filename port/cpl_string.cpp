/**********************************************************************
 * $Id$
 *
 * Name:     cpl_string.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  String and Stringlist manipulation functions.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1998, Daniel Morissette
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
 **********************************************************************
 *
 * $Log$
 * Revision 1.16  2001/07/18 04:00:49  warmerda
 * added CPL_CVSID
 *
 * Revision 1.15  2001/01/19 21:16:41  warmerda
 * expanded tabs
 *
 * Revision 1.14  2000/10/06 15:19:03  warmerda
 * added CPLSetNameValueSeparator
 *
 * Revision 1.13  2000/08/22 17:47:50  warmerda
 * Fixed declaration of gnCPLSPrintfBuffer.
 *
 * Revision 1.12  2000/08/18 21:20:54  svillene
 * *** empty log message ***
 *
 * Revision 1.11  2000/03/30 05:38:48  warmerda
 * added CPLParseNameValue
 *
 * Revision 1.10  1999/06/26 14:05:10  warmerda
 * Added CSLFindString().
 *
 * Revision 1.9  1999/04/28 02:33:02  danmo
 * CSLInsertStrings(): make sure papszStrList is NULL-terminated properly
 *
 * Revision 1.8  1999/03/12 21:19:49  danmo
 * Fixed TokenizeStringComplex() vs strings ending with empty token,
 * and fixed a problem with CSLAdd/SetNameValue() vs empty string list.
 *
 * Revision 1.7  1999/03/09 21:29:57  warmerda
 * Added backslash escaping within string constants for tokenize function.
 *
 * Revision 1.6  1999/02/25 04:40:46  danmo
 * Modif. CSLLoad() to use CPLReadLine() (better handling of newlines)
 *
 * Revision 1.5  1999/02/17 01:41:58  warmerda
 * Added CSLGetField
 *
 * Revision 1.4  1998/12/15 19:01:40  warmerda
 * *** empty log message ***
 *
 * Revision 1.3  1998/12/05 23:04:21  warmerda
 * Use EQUALN() instead of strincmp() which doesn't exist on Linux.
 *
 * Revision 1.2  1998/12/04 21:40:42  danmo
 * Added more Name=Value manipulation fuctions
 *
 * Revision 1.1  1998/12/03 18:26:02  warmerda
 * New
 *
 **********************************************************************/

#include "cpl_string.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$");

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
 *                       CSLLoad()
 *
 * Load a test file into a stringlist.
 *
 * Lines are limited in length by the size fo the CPLReadLine() buffer.
 **********************************************************************/
char **CSLLoad(const char *pszFname)
{
    FILE        *fp;
    const char  *pszLine;
    char        **papszStrList=NULL;

    fp = VSIFOpen(pszFname, "rt");

    if (fp)
    {
        while(!VSIFEof(fp))
        {
            if ( (pszLine = CPLReadLine(fp)) != NULL )
            {
                papszStrList = CSLAddString(papszStrList, pszLine);
            }
        }

        VSIFClose(fp);
    }
    else
    {
        /* Unable to open file */
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "CSLLoad(%s): %s", pszFname, strerror(errno));
    }

    return papszStrList;
}

/**********************************************************************
 *                       CSLSave()
 *
 * Write a stringlist to a text file.
 *
 * Returns the number of lines written, or 0 if the file could not 
 * be written.
 **********************************************************************/
int  CSLSave(char **papszStrList, const char *pszFname)
{
    FILE    *fp;
    int     nLines=0;

    if (papszStrList)
    {
        if ((fp = VSIFOpen(pszFname, "wt")) != NULL)
        {
            while(*papszStrList != NULL)
            {
                if (VSIFPuts(*papszStrList, fp) == EOF ||
                    VSIFPutc('\n', fp) == EOF)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "CSLSave(%s): %s", pszFname, 
                             strerror(errno));
                    break;  /* A Problem happened... abort */
                }

                nLines++;
                papszStrList++;
            }

            VSIFClose(fp);
        }
        else
        {
            /* Unable to open file */
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "CSLSave(%s): %s", pszFname, strerror(errno));
        }
    }

    return nLines;
}

/**********************************************************************
 *                       CSLPrint()
 *
 * Print a StringList to fpOut.  If fpOut==NULL, then output is sent
 * to stdout.
 *
 * Returns the number of lines printed.
 **********************************************************************/
int  CSLPrint(char **papszStrList, FILE *fpOut)
{
    int     nLines=0;

    if (fpOut == NULL)
        fpOut = stdout;

    if (papszStrList)
    {
        while(*papszStrList != NULL)
        {
            VSIFPrintf(fpOut, "%s\n", *papszStrList);
            nLines++;
            papszStrList++;
        }
    }

    return nLines;
}


/**********************************************************************
 *                       CSLInsertStrings()
 *
 * Copies the contents of a StringList inside another StringList 
 * before the specified line.
 *
 * nInsertAtLineNo is a 0-based line index before which the new strings
 * should be inserted.  If this value is -1 or is larger than the actual 
 * number of strings in the list then the strings are added at the end
 * of the source StringList.
 *
 * Returns the modified StringList.
 **********************************************************************/
char **CSLInsertStrings(char **papszStrList, int nInsertAtLineNo, 
                        char **papszNewLines)
{
    int     i, nSrcLines, nDstLines, nToInsert;
    char    **ppszSrc, **ppszDst;

    if (papszNewLines == NULL ||
        ( nToInsert = CSLCount(papszNewLines) ) == 0)
        return papszStrList;    /* Nothing to do!*/

    nSrcLines = CSLCount(papszStrList);
    nDstLines = nSrcLines + nToInsert;

    /* Allocate room for the new strings */
    papszStrList = (char**)CPLRealloc(papszStrList, 
                                      (nDstLines+1)*sizeof(char*));

    /* Make sure the array is NULL-terminated... it may not be if
     * papszStrList was NULL before Realloc()
     */
    papszStrList[nSrcLines] = NULL;

    /* Make some room in the original list at the specified location 
     * Note that we also have to move the NULL pointer at the end of
     * the source StringList.
     */
    if (nInsertAtLineNo == -1 || nInsertAtLineNo > nSrcLines)
        nInsertAtLineNo = nSrcLines;

    ppszSrc = papszStrList + nSrcLines;
    ppszDst = papszStrList + nDstLines;

    for (i=nSrcLines; i>=nInsertAtLineNo; i--)
    {
        *ppszDst = *ppszSrc;
        ppszDst--;
        ppszSrc--;
    }

    /* Copy the strings to the list */
    ppszSrc = papszNewLines;
    ppszDst = papszStrList + nInsertAtLineNo;

    for (; *ppszSrc != NULL; ppszSrc++, ppszDst++)
    {
        *ppszDst = CPLStrdup(*ppszSrc);
    }
    
    return papszStrList;
}

/**********************************************************************
 *                       CSLInsertString()
 *
 * Insert a string at a given line number inside a StringList 
 *
 * nInsertAtLineNo is a 0-based line index before which the new string
 * should be inserted.  If this value is -1 or is larger than the actual 
 * number of strings in the list then the string is added at the end
 * of the source StringList.
 *
 * Returns the modified StringList.
 **********************************************************************/
char **CSLInsertString(char **papszStrList, int nInsertAtLineNo, 
                           char *pszNewLine)
{
    char *apszList[2];

    /* Create a temporary StringList and call CSLInsertStrings()
     */
    apszList[0] = pszNewLine;
    apszList[1] = NULL;

    return CSLInsertStrings(papszStrList, nInsertAtLineNo, apszList);
}


/**********************************************************************
 *                       CSLRemoveStrings()
 *
 * Remove strings inside a StringList 
 *
 * nFirstLineToDelete is the 0-based line index of the first line to 
 * remove. If this value is -1 or is larger than the actual 
 * number of strings in list then the nNumToRemove last strings are
 * removed.
 *
 * If ppapszRetStrings != NULL then the deleted strings won't be
 * free'd, they will be stored in a new StringList and the pointer to
 * this new list will be returned in *ppapszRetStrings.
 *
 * Returns the modified StringList.
 **********************************************************************/
char **CSLRemoveStrings(char **papszStrList, int nFirstLineToDelete,
                        int nNumToRemove, char ***ppapszRetStrings)
{
    int     i, nSrcLines, nDstLines;
    char    **ppszSrc, **ppszDst;

    nSrcLines = CSLCount(papszStrList);
    nDstLines = nSrcLines - nNumToRemove;

    if (nNumToRemove < 1 || nSrcLines == 0)
        return papszStrList;    /* Nothing to do!*/

    /* If operation will result in an empty StringList then don't waste
     * time here!
     */
    if (nDstLines < 1)
    {
        CSLDestroy(papszStrList);
        return NULL;
    }

    
    /* Remove lines from the source StringList...
     * Either free() each line or store them to a new StringList depending on
     * the caller's choice.
     */
    ppszDst = papszStrList + nFirstLineToDelete;

    if (ppapszRetStrings == NULL)
    {
        /* free() all the strings that will be removed.
         */
        for (i=0; i < nNumToRemove; i++)
        {
            CPLFree(*ppszDst);
            *ppszDst = NULL;
        }
    }
    else
    {
        /* Store the strings to remove in a new StringList
         */
        *ppapszRetStrings = (char **)CPLCalloc(nNumToRemove+1, sizeof(char*));

        for (i=0; i < nNumToRemove; i++)
        {
            (*ppapszRetStrings)[i] = *ppszDst;
            *ppszDst = NULL;
            ppszDst++;
        }
    }


    /* Shift down all the lines that follow the lines to remove.
     */
    if (nFirstLineToDelete == -1 || nFirstLineToDelete > nSrcLines)
        nFirstLineToDelete = nDstLines;

    ppszSrc = papszStrList + nFirstLineToDelete + nNumToRemove;
    ppszDst = papszStrList + nFirstLineToDelete;

    for ( ; *ppszSrc != NULL; ppszSrc++, ppszDst++)
    {
        *ppszDst = *ppszSrc;
    }
    /* Move the NULL pointer at the end of the StringList     */
    *ppszDst = *ppszSrc; 

    /* At this point, we could realloc() papszStrList to a smaller size, but
     * since this array will likely grow again in further operations on the
     * StringList we'll leave it as it is.
     */

    return papszStrList;
}

/************************************************************************/
/*                           CSLFindString()                            */
/*                                                                      */
/*      Find a string within a string list.  The string must match      */
/*      the full length, but the comparison is case insensitive.        */
/*      Return -1 on failure.                                           */
/************************************************************************/

int CSLFindString( char ** papszList, const char * pszTarget )

{
    int         i;

    if( papszList == NULL )
        return -1;

    for( i = 0; papszList[i] != NULL; i++ )
    {
        if( EQUAL(papszList[i],pszTarget) )
            return i;
    }

    return -1;
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
    char        **papszRetList = NULL;
    char        *pszToken;
    int         nTokenMax, nTokenLen;

    pszToken = (char *) CPLCalloc(10,1);
    nTokenMax = 10;
    
    while( pszString != NULL && *pszString != '\0' )
    {
        int     bInString = FALSE;

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

        /* If the last token is an empty token, then we have to catch
         * it now, otherwise we won't reenter the loop and it will be lost. 
         */
        if ( *pszString == '\0' && bAllowEmptyTokens &&
             strchr(pszDelimiters, *(pszString-1)) )
        {
            papszRetList = CSLAddString( papszRetList, "" );
        }
    }

    if( papszRetList == NULL )
        papszRetList = (char **) CPLCalloc(sizeof(char *),1);

    CPLFree( pszToken );

    return papszRetList;
}

/**********************************************************************
 *                       CPLSPrintf()
 *
 * My own version of CPLSPrintf() that works with 10 static buffer.
 *
 * It returns a ref. to a static buffer that should not be freed and
 * is valid only until the next call to CPLSPrintf().
 *
 * NOTE: This function should move to cpl_conv.cpp. 
 **********************************************************************/
/* For now, assume that a 8000 chars buffer will be enough.
 */
#define CPLSPrintf_BUF_SIZE 8000
#define CPLSPrintf_BUF_Count 10
static char gszCPLSPrintfBuffer[CPLSPrintf_BUF_Count][CPLSPrintf_BUF_SIZE];
static int gnCPLSPrintfBuffer = 0;

const char *CPLSPrintf(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsprintf(gszCPLSPrintfBuffer[gnCPLSPrintfBuffer], fmt, args);
    va_end(args);
    
   int nCurrent = gnCPLSPrintfBuffer;

    if (++gnCPLSPrintfBuffer == CPLSPrintf_BUF_Count)
      gnCPLSPrintfBuffer = 0;

    return gszCPLSPrintfBuffer[nCurrent];
}

/**********************************************************************
 *                       CSLAppendPrintf()
 *
 * Use CPLSPrintf() to append a new line at the end of a StringList.
 *
 * Returns the modified StringList.
 **********************************************************************/
char **CSLAppendPrintf(char **papszStrList, char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vsprintf(gszCPLSPrintfBuffer[gnCPLSPrintfBuffer], fmt, args);
    va_end(args);

    int nCurrent = gnCPLSPrintfBuffer;

    if (++gnCPLSPrintfBuffer == CPLSPrintf_BUF_Count)
      gnCPLSPrintfBuffer = 0;

    return CSLAddString(papszStrList, gszCPLSPrintfBuffer[nCurrent]);
}


/**********************************************************************
 *                       CSLFetchNameValue()
 *
 * In a StringList of "Name=Value" pairs, look for the
 * first value associated with the specified name.  The search is not
 * case sensitive.
 * ("Name:Value" pairs are also supported for backward compatibility
 * with older stuff.)
 * 
 * Returns a reference to the value in the StringList that the caller
 * should not attempt to free.
 *
 * Returns NULL if the name is not found.
 **********************************************************************/
const char *CSLFetchNameValue(char **papszStrList, const char *pszName)
{
    int nLen;

    if (papszStrList == NULL || pszName == NULL)
        return NULL;

    nLen = strlen(pszName);
    while(*papszStrList != NULL)
    {
        if (EQUALN(*papszStrList, pszName, nLen)
            && ( (*papszStrList)[nLen] == '=' || 
                 (*papszStrList)[nLen] == ':' ) )
        {
            return (*papszStrList)+nLen+1;
        }
        papszStrList++;
    }
    return NULL;
}

/**********************************************************************
 *                       CPLParseNameValue()
 **********************************************************************/

/**
 * Parse NAME=VALUE string into name and value components.
 *
 * Note that if ppszKey is non-NULL, the key (or name) portion will be
 * allocated using VSIMalloc(), and returned in that pointer.  It is the
 * applications responsibility to free this string, but the application should
 * not modify or free the returned value portion. 
 *
 * This function also support "NAME:VALUE" strings and will strip white
 * space from around the delimeter when forming name and value strings.
 *
 * Eventually CSLFetchNameValue() and friends may be modified to use 
 * CPLParseNameValue(). 
 * 
 * @param pszNameValue string in "NAME=VALUE" format. 
 * @param ppszKey optional pointer though which to really the name
 * portion. 
 * @return the value portion (pointing into original string). 
 */

const char *CPLParseNameValue(const char *pszNameValue, char **ppszKey )

{
    int  i;
    const char *pszValue;

    for( i = 0; pszNameValue[i] != '\0'; i++ )
    {
        if( pszNameValue[i] == '=' || pszNameValue[i] == ':' )
        {
            pszValue = pszNameValue + i + 1;
            while( *pszValue == ' ' || *pszValue == '\t' )
                pszValue++;

            if( ppszKey != NULL )
            {
                *ppszKey = (char *) CPLMalloc(i+1);
                strncpy( *ppszKey, pszNameValue, i );
                (*ppszKey)[i] = '\0';
                while( i > 0 && 
                       ( (*ppszKey)[i] == ' ' || (*ppszKey)[i] == '\t') )
                {
                    (*ppszKey)[i] = '\0';
                    i--;
                }
            }

            return pszValue;
        }

    }

    return NULL;
}

/**********************************************************************
 *                       CSLFetchNameValueMultiple()
 *
 * In a StringList of "Name=Value" pairs, look for all the
 * values with the specified name.  The search is not case
 * sensitive.
 * ("Name:Value" pairs are also supported for backward compatibility
 * with older stuff.)
 * 
 * Returns stringlist with one entry for each occurence of the
 * specified name.  The stringlist should eventually be destroyed
 * by calling CSLDestroy().
 *
 * Returns NULL if the name is not found.
 **********************************************************************/
char **CSLFetchNameValueMultiple(char **papszStrList, const char *pszName)
{
    int nLen;
    char **papszValues = NULL;

    if (papszStrList == NULL || pszName == NULL)
        return NULL;

    nLen = strlen(pszName);
    while(*papszStrList != NULL)
    {
        if (EQUALN(*papszStrList, pszName, nLen)
            && ( (*papszStrList)[nLen] == '=' || 
                 (*papszStrList)[nLen] == ':' ) )
        {
            papszValues = CSLAddString(papszValues, 
                                          (*papszStrList)+nLen+1);
        }
        papszStrList++;
    }

    return papszValues;
}


/**********************************************************************
 *                       CSLAddNameValue()
 *
 * Add a new entry to a StringList of "Name=Value" pairs,
 * ("Name:Value" pairs are also supported for backward compatibility
 * with older stuff.)
 * 
 * This function does not check if a "Name=Value" pair already exists
 * for that name and can generate multiple entryes for the same name.
 * Use CSLSetNameValue() if you want each name to have only one value.
 *
 * Returns the modified stringlist.
 **********************************************************************/
char **CSLAddNameValue(char **papszStrList, 
                    const char *pszName, const char *pszValue)
{
    const char *pszLine;

    if (pszName == NULL || pszValue==NULL)
        return papszStrList;

    pszLine = CPLSPrintf("%s=%s", pszName, pszValue);

    return CSLAddString(papszStrList, pszLine);
}

/**********************************************************************
 *                       CSLSetNameValue()
 *
 * Set the value for a given name in a StringList of "Name=Value" pairs
 * ("Name:Value" pairs are also supported for backward compatibility
 * with older stuff.)
 * 
 * If there is already a value for that name in the list then the value
 * is changed, otherwise a new "Name=Value" pair is added.
 *
 * Returns the modified stringlist.
 **********************************************************************/
char **CSLSetNameValue(char **papszList, 
                    const char *pszName, const char *pszValue)
{
    char **papszPtr;
    int nLen;

    if (pszName == NULL || pszValue==NULL)
        return papszList;

    nLen = strlen(pszName);
    papszPtr = papszList;
    while(papszPtr && *papszPtr != NULL)
    {
        if (EQUALN(*papszPtr, pszName, nLen)
            && ( (*papszPtr)[nLen] == '=' || 
                 (*papszPtr)[nLen] == ':' ) )
        {
            /* Found it!  
             * Change the value... make sure to keep the ':' or '='
             */
            char cSep;
            cSep = (*papszPtr)[nLen];

            free(*papszPtr);
            *papszPtr = CPLStrdup(CPLSPrintf("%s%c%s", pszName,
                                                       cSep, pszValue));

            return papszList;
        }
        papszPtr++;
    }

    /* The name does not exist yet... create a new entry
     */
    return CSLAddString(papszList, 
                           CPLSPrintf("%s=%s", pszName, pszValue));
}

/************************************************************************/
/*                      CSLSetNameValueSeparator()                      */
/************************************************************************/

/**
 * Replace the default separator (":" or "=") with the passed separator
 * in the given name/value list. 
 *
 * Note that if a separator other than ":" or "=" is used, the resulting
 * list will not be manipulatable by the CSL name/value functions any more.
 *
 * The CPLParseNameValue() function is used to break the existing lines, 
 * and it also strips white space from around the existing delimiter, thus
 * the old separator, and any white space will be replaced by the new
 * separator.  For formatting purposes it may be desireable to include some
 * white space in the new separator.  eg. ": " or " = ".
 * 
 * @param papszList the list to update.  Component strings may be freed
 * but the list array will remain at the same location.
 *
 * @param pszSeparator the new separator string to insert.  
 *
 */

void CSLSetNameValueSeparator( char ** papszList, const char *pszSeparator )

{
    int         nLines = CSLCount(papszList), iLine;

    for( iLine = 0; iLine < nLines; iLine++ )
    {
        char    *pszKey = NULL;
        const char *pszValue;
        char    *pszNewLine;

        pszValue = CPLParseNameValue( papszList[iLine], &pszKey );
        
        pszNewLine = (char *) CPLMalloc(strlen(pszValue)+strlen(pszKey)
                                        +strlen(pszSeparator)+1);
        strcpy( pszNewLine, pszKey );
        strcat( pszNewLine, pszSeparator );
        strcat( pszNewLine, pszValue );
        CPLFree( papszList[iLine] );
        papszList[iLine] = pszNewLine;
    }
}
