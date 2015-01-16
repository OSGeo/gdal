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
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 * Independent Security Audit 2003/04/04 Andrey Kiselev:
 *   Completed audit of this module. All functions may be used without buffer
 *   overflows and stack corruptions with any kind of input data strings with
 *   except of CPLSPrintf() and CSLAppendPrintf() (see note below).
 * 
 * Security Audit 2003/03/28 warmerda:
 *   Completed security audit.  I believe that this module may be safely used 
 *   to parse tokenize arbitrary input strings, assemble arbitrary sets of
 *   names values into string lists, unescape and escape text even if provided
 *   by a potentially hostile source.   
 *
 *   CPLSPrintf() and CSLAppendPrintf() may not be safely invoked on 
 *   arbitrary length inputs since it has a fixed size output buffer on system 
 *   without vsnprintf(). 
 *
 **********************************************************************/

#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_multiproc.h"

#if defined(WIN32CE)
#  include <wce_errno.h>
#  include <wce_string.h>
#endif

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
 * Note that CSLAddString performance when building a list is in O(n^2)
 * which can cause noticable slow down when n > 10000.
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

/************************************************************************/
/*                              CSLCount()                              */
/************************************************************************/

/**
 * Return number of items in a string list.
 *
 * Returns the number of items in a string list, not counting the 
 * terminating NULL.  Passing in NULL is safe, and will result in a count
 * of zero.  
 *
 * Lists are counted by iterating through them so long lists will
 * take more time than short lists.  Care should be taken to avoid using
 * CSLCount() as an end condition for loops as it will result in O(n^2)
 * behavior. 
 *
 * @param papszStrList the string list to count.
 * 
 * @return the number of entries.
 */
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

/************************************************************************/
/*                             CSLDestroy()                             */
/************************************************************************/

/**
 * Free string list.
 * 
 * Frees the passed string list (null terminated array of strings).
 * It is safe to pass NULL. 
 *
 * @param papszStrList the list to free.
 */
void CPL_STDCALL CSLDestroy(char **papszStrList)
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

/************************************************************************/
/*                            CSLDuplicate()                            */
/************************************************************************/

/**
 * Clone a string list.
 *
 * Efficiently allocates a copy of a string list.  The returned list is
 * owned by the caller and should be freed with CSLDestroy().
 *
 * @param papszStrList the input string list.
 * 
 * @return newly allocated copy.
 */

char **CSLDuplicate(char **papszStrList)
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

/************************************************************************/
/*                               CSLMerge                               */
/************************************************************************/

/**
 * \brief Merge two lists.
 *
 * The two lists are merged, ensuring that if any keys appear in both
 * that the value from the second (papszOverride) list take precidence.
 *
 * @param papszOrig the original list, being modified.
 * @param papszOverride the list of items being merged in.  This list
 * is unaltered and remains owned by the caller.
 *
 * @return updated list.
 */

char **CSLMerge( char **papszOrig, char **papszOverride )

{
    int i;

    if( papszOrig == NULL && papszOverride != NULL )
        return CSLDuplicate( papszOverride );
    
    if( papszOverride == NULL )
        return papszOrig;

    for( i = 0; papszOverride[i] != NULL; i++ )
    {
        char *pszKey = NULL;
        const char *pszValue = CPLParseNameValue( papszOverride[i], &pszKey );

        papszOrig = CSLSetNameValue( papszOrig, pszKey, pszValue );
        CPLFree( pszKey );
    }

    return papszOrig;
}

/************************************************************************/
/*                             CSLLoad2()                               */
/************************************************************************/

/**
 * Load a text file into a string list.
 *
 * The VSI*L API is used, so VSIFOpenL() supported objects that aren't
 * physical files can also be accessed.  Files are returned as a string list,
 * with one item in the string list per line.  End of line markers are
 * stripped (by CPLReadLineL()). 
 *
 * If reading the file fails a CPLError() will be issued and NULL returned.
 *
 * @param pszFname the name of the file to read.
 * @param nMaxLines maximum number of lines to read before stopping, or -1 for no limit.
 * @param nMaxCols  maximum number of characters in a line before stopping, or -1 for no limit.
 * @param papszOptions NULL-terminated array of options. Unused for now.
 * 
 * @return a string list with the files lines, now owned by caller. To be freed with CSLDestroy()
 *
 * @since GDAL 1.7.0
 */

char **CSLLoad2(const char *pszFname, int nMaxLines, int nMaxCols, char** papszOptions)
{
    VSILFILE    *fp;
    const char  *pszLine;
    char        **papszStrList=NULL;
    int          nLines = 0;
    int          nAllocatedLines = 0;

    fp = VSIFOpenL(pszFname, "rb");

    if (fp)
    {
        CPLErrorReset();
        while(!VSIFEofL(fp) && (nMaxLines == -1 || nLines < nMaxLines))
        {
            if ( (pszLine = CPLReadLine2L(fp, nMaxCols, papszOptions)) != NULL )
            {
                if (nLines + 1 >= nAllocatedLines)
                {
                    char** papszStrListNew;
                    nAllocatedLines = 16 + nAllocatedLines * 2;
                    papszStrListNew = (char**) VSIRealloc(papszStrList,
                                                nAllocatedLines * sizeof(char*));
                    if (papszStrListNew == NULL)
                    {
                        VSIFCloseL(fp);
                        CPLReadLineL( NULL );
                        CPLError( CE_Failure, CPLE_OutOfMemory,
                             "CSLLoad2(\"%s\") failed: not enough memory to allocate lines.",
                            pszFname );
                        return papszStrList;
                    }
                    papszStrList = papszStrListNew;
                }
                papszStrList[nLines] = CPLStrdup(pszLine);
                papszStrList[nLines + 1] = NULL;
                nLines ++;
            }
            else
                break;
        }

        VSIFCloseL(fp);

        CPLReadLineL( NULL );
    }
    else
    {
        if (CSLFetchBoolean(papszOptions, "EMIT_ERROR_IF_CANNOT_OPEN_FILE", TRUE))
        {
            /* Unable to open file */
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "CSLLoad2(\"%s\") failed: unable to open file.",
                    pszFname );
        }
    }

    return papszStrList;
}

/************************************************************************/
/*                              CSLLoad()                               */
/************************************************************************/

/**
 * Load a text file into a string list.
 *
 * The VSI*L API is used, so VSIFOpenL() supported objects that aren't
 * physical files can also be accessed.  Files are returned as a string list,
 * with one item in the string list per line.  End of line markers are
 * stripped (by CPLReadLineL()). 
 *
 * If reading the file fails a CPLError() will be issued and NULL returned.
 *
 * @param pszFname the name of the file to read.
 * 
 * @return a string list with the files lines, now owned by caller. To be freed with CSLDestroy()
 */
 
char **CSLLoad(const char *pszFname)
{
    return CSLLoad2(pszFname, -1, -1, NULL);
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
    VSILFILE *fp;
    int     nLines = 0;

    if (papszStrList)
    {
        if ((fp = VSIFOpenL(pszFname, "wt")) != NULL)
        {
            while(*papszStrList != NULL)
            {
                if( VSIFPrintfL( fp, "%s\n", *papszStrList ) < 1 )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                    "CSLSave(\"%s\") failed: unable to write to output file.",
                              pszFname );
                    break;  /* A Problem happened... abort */
                }

                nLines++;
                papszStrList++;
            }

            VSIFCloseL(fp);
        }
        else
        {
            /* Unable to open file */
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "CSLSave(\"%s\") failed: unable to open output file.",
                      pszFname );
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
                       const char *pszNewLine)
{
    char *apszList[2];

    /* Create a temporary StringList and call CSLInsertStrings()
     */
    apszList[0] = (char *) pszNewLine;
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
/************************************************************************/

/**
 * Find a string within a string list.
 *
 * Returns the index of the entry in the string list that contains the 
 * target string.  The string in the string list must be a full match for
 * the target, but the search is case insensitive. 
 * 
 * @param papszList the string list to be searched.
 * @param pszTarget the string to be searched for. 
 * 
 * @return the index of the string within the list or -1 on failure.
 */

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

/************************************************************************/
/*                           CSLPartialFindString()                     */
/************************************************************************/

/**
 * Find a substring within a string list.
 *
 * Returns the index of the entry in the string list that contains the 
 * target string as a substring.  The search is case sensitive (unlike 
 * CSLFindString()). 
 * 
 * @param papszHaystack the string list to be searched.
 * @param pszNeedle the substring to be searched for. 
 * 
 * @return the index of the string within the list or -1 on failure.
 */

int CSLPartialFindString( char **papszHaystack, const char * pszNeedle )
{
    int i;
    if (papszHaystack == NULL || pszNeedle == NULL)
        return -1;

    for (i = 0; papszHaystack[i] != NULL; i++) 
    {
        if (strstr(papszHaystack[i],pszNeedle))
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
    return CSLTokenizeString2( pszString, " ", CSLT_HONOURSTRINGS );
}

/************************************************************************/
/*                      CSLTokenizeStringComplex()                      */
/*                                                                      */
/*      Obsolete tokenizing api.                                        */
/************************************************************************/

char ** CSLTokenizeStringComplex( const char * pszString,
                                  const char * pszDelimiters,
                                  int bHonourStrings, int bAllowEmptyTokens )

{
    int         nFlags = 0;

    if( bHonourStrings )
        nFlags |= CSLT_HONOURSTRINGS;
    if( bAllowEmptyTokens )
        nFlags |= CSLT_ALLOWEMPTYTOKENS;

    return CSLTokenizeString2( pszString, pszDelimiters, nFlags );
}

/************************************************************************/
/*                         CSLTokenizeString2()                         */
/************************************************************************/

/**
 * Tokenize a string. 
 *
 * This function will split a string into tokens based on specified'
 * delimeter(s) with a variety of options.  The returned result is a
 * string list that should be freed with CSLDestroy() when no longer
 * needed.
 * 
 * The available parsing options are:
 * 
 * - CSLT_ALLOWEMPTYTOKENS: allow the return of empty tokens when two 
 * delimiters in a row occur with no other text between them.  If not set, 
 * empty tokens will be discarded;
 * - CSLT_STRIPLEADSPACES: strip leading space characters from the token (as
 * reported by isspace());
 * - CSLT_STRIPENDSPACES: strip ending space characters from the token (as
 * reported by isspace());
 * - CSLT_HONOURSTRINGS: double quotes can be used to hold values that should 
 * not be broken into multiple tokens; 
 * - CSLT_PRESERVEQUOTES: string quotes are carried into the tokens when this
 * is set, otherwise they are removed;
 * - CSLT_PRESERVEESCAPES: if set backslash escapes (for backslash itself, 
 * and for literal double quotes) will be preserved in the tokens, otherwise
 * the backslashes will be removed in processing.
 *
 * \b Example:
 * 
 * Parse a string into tokens based on various white space (space, newline, 
 * tab) and then print out results and cleanup.  Quotes may be used to hold 
 * white space in tokens.

\code
    char **papszTokens;
    int i;

    papszTokens = 
        CSLTokenizeString2( pszCommand, " \t\n", 
                            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );

    for( i = 0; papszTokens != NULL && papszTokens[i] != NULL; i++ )
        printf( "arg %d: '%s'", papszTokens[i] );
    CSLDestroy( papszTokens );
\endcode

 * @param pszString the string to be split into tokens.
 * @param pszDelimiters one or more characters to be used as token delimeters.
 * @param nCSLTFlags an ORing of one or more of the CSLT_ flag values.
 *
 * @return a string list of tokens owned by the caller.
 */

char ** CSLTokenizeString2( const char * pszString,
                            const char * pszDelimiters,
                            int nCSLTFlags )

{
    if( pszString == NULL )
        return (char **) CPLCalloc(sizeof(char *),1);
    CPLStringList oRetList;
    char        *pszToken;
    int         nTokenMax, nTokenLen;
    int         bHonourStrings = (nCSLTFlags & CSLT_HONOURSTRINGS);
    int         bAllowEmptyTokens = (nCSLTFlags & CSLT_ALLOWEMPTYTOKENS);
    int         bStripLeadSpaces = (nCSLTFlags & CSLT_STRIPLEADSPACES);
    int         bStripEndSpaces = (nCSLTFlags & CSLT_STRIPENDSPACES);

    pszToken = (char *) CPLCalloc(10,1);
    nTokenMax = 10;
    
    while( pszString != NULL && *pszString != '\0' )
    {
        int     bInString = FALSE;
        int     bStartString = TRUE;

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
                if( nCSLTFlags & CSLT_PRESERVEQUOTES )
                {
                    pszToken[nTokenLen] = *pszString;
                    nTokenLen++;
                }

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

            /*
             * Within string constants we allow for escaped quotes, but in
             * processing them we will unescape the quotes and \\ sequence
             * reduces to \
             */
            if( bInString && pszString[0] == '\\' )
            {
                if ( pszString[1] == '"' || pszString[1] == '\\' )
                {
                    if( nCSLTFlags & CSLT_PRESERVEESCAPES )
                    {
                        pszToken[nTokenLen] = *pszString;
                        nTokenLen++;
                    }

                    pszString++;
                }
            }

            /*
             * Strip spaces at the token start if requested.
             */
            if ( !bInString && bStripLeadSpaces
                 && bStartString && isspace((unsigned char)*pszString) )
                continue;

            bStartString = FALSE;

            /*
             * Extend token buffer if we are running close to its end.
             */
            if( nTokenLen >= nTokenMax-3 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = (char *) CPLRealloc( pszToken, nTokenMax );
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        /*
         * Strip spaces at the token end if requested.
         */
        if ( !bInString && bStripEndSpaces )
        {
            while ( nTokenLen && isspace((unsigned char)pszToken[nTokenLen - 1]) )
                nTokenLen--;
        }

        pszToken[nTokenLen] = '\0';

        /*
         * Add the token.
         */
        if( pszToken[0] != '\0' || bAllowEmptyTokens )
            oRetList.AddString( pszToken );
    }

    /*
     * If the last token was empty, then we need to capture
     * it now, as the loop would skip it.
     */
    if( *pszString == '\0' && bAllowEmptyTokens && oRetList.Count() > 0 
        && strchr(pszDelimiters,*(pszString-1)) != NULL )
    {
        oRetList.AddString( "" );
    }

    CPLFree( pszToken );

    if( oRetList.List() == NULL )
    {
        // we prefer to return empty lists as a pointer to 
        // a null pointer since some client code might depend on this.
        oRetList.Assign( (char**) CPLCalloc(sizeof(char*),1) );
    }

    return oRetList.StealList();
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

const char *CPLSPrintf(const char *fmt, ...)
{
    va_list args;

/* -------------------------------------------------------------------- */
/*      Get the thread local buffer ring data.                          */
/* -------------------------------------------------------------------- */
    char *pachBufRingInfo = (char *) CPLGetTLS( CTLS_CPLSPRINTF );

    if( pachBufRingInfo == NULL )
    {
        pachBufRingInfo = (char *) 
            CPLCalloc(1,sizeof(int)+CPLSPrintf_BUF_Count*CPLSPrintf_BUF_SIZE);
        CPLSetTLS( CTLS_CPLSPRINTF, pachBufRingInfo, TRUE );
    }

/* -------------------------------------------------------------------- */
/*      Work out which string in the "ring" we want to use this         */
/*      time.                                                           */
/* -------------------------------------------------------------------- */
    int *pnBufIndex = (int *) pachBufRingInfo;
    int nOffset = sizeof(int) + *pnBufIndex * CPLSPrintf_BUF_SIZE;
    char *pachBuffer = pachBufRingInfo + nOffset;

    *pnBufIndex = (*pnBufIndex + 1) % CPLSPrintf_BUF_Count;

/* -------------------------------------------------------------------- */
/*      Format the result.                                              */
/* -------------------------------------------------------------------- */

    va_start(args, fmt);
#if defined(HAVE_VSNPRINTF)
    vsnprintf(pachBuffer, CPLSPrintf_BUF_SIZE-1, fmt, args);
#else
    vsprintf(pachBuffer, fmt, args);
#endif
    va_end(args);
    
    return pachBuffer;
}

/**********************************************************************
 *                       CSLAppendPrintf()
 *
 * Use CPLSPrintf() to append a new line at the end of a StringList.
 *
 * Returns the modified StringList.
 **********************************************************************/
char **CSLAppendPrintf(char **papszStrList, const char *fmt, ...)
{
    CPLString osWork;
    va_list args;

    va_start( args, fmt );
    osWork.vPrintf( fmt, args );
    va_end( args );

    return CSLAddString(papszStrList, osWork);
}

/************************************************************************/
/*                            CPLVASPrintf()                            */
/*                                                                      */
/*      This is intended to serve as an easy to use C callabable        */
/*      vasprintf() alternative.  Used in the geojson library for       */
/*      instance.                                                       */
/************************************************************************/

int CPLVASPrintf( char **buf, const char *fmt, va_list ap )

{
    CPLString osWork;

    osWork.vPrintf( fmt, ap );

    if( buf )
        *buf = CPLStrdup(osWork.c_str());

    return strlen(osWork);
}

/************************************************************************/
/*                         CSLTestBoolean()                             */
/************************************************************************/

/**
 * Test what boolean value contained in the string.
 *
 * If pszValue is "NO", "FALSE", "OFF" or "0" will be returned FALSE.
 * Otherwise, TRUE will be returned.
 *
 * @param pszValue the string should be tested.
 * 
 * @return TRUE or FALSE.
 */

int CSLTestBoolean( const char *pszValue )
{
    if( EQUAL(pszValue,"NO")
        || EQUAL(pszValue,"FALSE") 
        || EQUAL(pszValue,"OFF") 
        || EQUAL(pszValue,"0") )
        return FALSE;
    else
        return TRUE;
}

/**********************************************************************
 *                       CSLFetchBoolean()
 *
 * Check for boolean key value.
 *
 * In a StringList of "Name=Value" pairs, look to see if there is a key
 * with the given name, and if it can be interpreted as being TRUE.  If
 * the key appears without any "=Value" portion it will be considered true. 
 * If the value is NO, FALSE or 0 it will be considered FALSE otherwise
 * if the key appears in the list it will be considered TRUE.  If the key
 * doesn't appear at all, the indicated default value will be returned. 
 * 
 * @param papszStrList the string list to search.
 * @param pszKey the key value to look for (case insensitive).
 * @param bDefault the value to return if the key isn't found at all. 
 * 
 * @return TRUE or FALSE 
 **********************************************************************/

int CSLFetchBoolean( char **papszStrList, const char *pszKey, int bDefault )

{
    const char *pszValue;

    if( CSLFindString( papszStrList, pszKey ) != -1 )
        return TRUE;

    pszValue = CSLFetchNameValue(papszStrList, pszKey );
    if( pszValue == NULL )
        return bDefault;
    else 
        return CSLTestBoolean( pszValue );
}

/************************************************************************/
/*                     CSLFetchNameValueDefaulted()                     */
/************************************************************************/

const char *CSLFetchNameValueDef( char **papszStrList, 
                                  const char *pszName,
                                  const char *pszDefault )

{
    const char *pszResult = CSLFetchNameValue( papszStrList, pszName );
    if( pszResult )
        return pszResult;
    else
        return pszDefault;
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
    size_t nLen;

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

/************************************************************************/
/*                            CSLFindName()                             */
/************************************************************************/

/**
 * Find StringList entry with given key name.
 *
 * @param papszStrList the string list to search.
 * @param pszName the key value to look for (case insensitive).
 *
 * @return -1 on failure or the list index of the first occurance 
 * matching the given key.
 */

int CSLFindName(char **papszStrList, const char *pszName)
{
    size_t nLen;
    int    iIndex = 0;

    if (papszStrList == NULL || pszName == NULL)
        return -1;

    nLen = strlen(pszName);
    while(*papszStrList != NULL)
    {
        if (EQUALN(*papszStrList, pszName, nLen)
            && ( (*papszStrList)[nLen] == '=' || 
                 (*papszStrList)[nLen] == ':' ) )
        {
            return iIndex;
        }
        iIndex++;
        papszStrList++;
    }
    return -1;
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
 * @param ppszKey optional pointer though which to return the name
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
    size_t nLen;
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
    char *pszLine;

    if (pszName == NULL || pszValue==NULL)
        return papszStrList;

    pszLine = (char *) CPLMalloc(strlen(pszName)+strlen(pszValue)+2);
    sprintf( pszLine, "%s=%s", pszName, pszValue );
    papszStrList = CSLAddString(papszStrList, pszLine);
    CPLFree( pszLine );

    return papszStrList;
}

/************************************************************************/
/*                          CSLSetNameValue()                           */
/************************************************************************/

/**
 * Assign value to name in StringList.
 *
 * Set the value for a given name in a StringList of "Name=Value" pairs
 * ("Name:Value" pairs are also supported for backward compatibility
 * with older stuff.)
 * 
 * If there is already a value for that name in the list then the value
 * is changed, otherwise a new "Name=Value" pair is added.
 *
 * @param papszList the original list, the modified version is returned.
 * @param pszName the name to be assigned a value.  This should be a well
 * formed token (no spaces or very special characters). 
 * @param pszValue the value to assign to the name.  This should not contain
 * any newlines (CR or LF) but is otherwise pretty much unconstrained.  If
 * NULL any corresponding value will be removed.
 *
 * @return modified stringlist.
 */

char **CSLSetNameValue(char **papszList, 
                       const char *pszName, const char *pszValue)
{
    char **papszPtr;
    size_t nLen;

    if (pszName == NULL )
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

            CPLFree(*papszPtr);

            /* 
             * If the value is NULL, remove this entry completely/
             */
            if( pszValue == NULL )
            {
                while( papszPtr[1] != NULL )
                {
                    *papszPtr = papszPtr[1];
                    papszPtr++;
                }
                *papszPtr = NULL;
            }

            /*
             * Otherwise replace with new value.
             */
            else
            {
                *papszPtr = (char *) CPLMalloc(strlen(pszName)+strlen(pszValue)+2);
                sprintf( *papszPtr, "%s%c%s", pszName, cSep, pszValue );
            }
            return papszList;
        }
        papszPtr++;
    }

    if( pszValue == NULL )
        return papszList;

    /* The name does not exist yet... create a new entry
     */
    return CSLAddNameValue(papszList, pszName, pszValue);
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
        char        *pszKey = NULL;
        const char  *pszValue;
        char        *pszNewLine;

        pszValue = CPLParseNameValue( papszList[iLine], &pszKey );
        if( pszValue == NULL || pszKey == NULL )
            continue;
        
        pszNewLine = (char *) CPLMalloc( strlen(pszValue) + strlen(pszKey)
                                         + strlen(pszSeparator) + 1 );
        strcpy( pszNewLine, pszKey );
        strcat( pszNewLine, pszSeparator );
        strcat( pszNewLine, pszValue );
        CPLFree( papszList[iLine] );
        papszList[iLine] = pszNewLine;
        CPLFree( pszKey );
    }
}

/************************************************************************/
/*                          CPLEscapeString()                           */
/************************************************************************/

/**
 * Apply escaping to string to preserve special characters.
 *
 * This function will "escape" a variety of special characters
 * to make the string suitable to embed within a string constant
 * or to write within a text stream but in a form that can be
 * reconstitued to it's original form.  The escaping will even preserve
 * zero bytes allowing preservation of raw binary data.
 *
 * CPLES_BackslashQuotable(0): This scheme turns a binary string into 
 * a form suitable to be placed within double quotes as a string constant.
 * The backslash, quote, '\\0' and newline characters are all escaped in 
 * the usual C style. 
 *
 * CPLES_XML(1): This scheme converts the '<', '>', '"' and '&' characters into
 * their XML/HTML equivelent (&lt;, &gt;, &quot; and &amp;) making a string safe
 * to embed as CDATA within an XML element.  The '\\0' is not escaped and 
 * should not be included in the input.
 *
 * CPLES_URL(2): Everything except alphanumerics and the underscore are 
 * converted to a percent followed by a two digit hex encoding of the character
 * (leading zero supplied if needed).  This is the mechanism used for encoding
 * values to be passed in URLs.
 *
 * CPLES_SQL(3): All single quotes are replaced with two single quotes.  
 * Suitable for use when constructing literal values for SQL commands where
 * the literal will be enclosed in single quotes.
 *
 * CPLES_CSV(4): If the values contains commas, semicolons, tabs, double quotes, or newlines it 
 * placed in double quotes, and double quotes in the value are doubled.
 * Suitable for use when constructing field values for .csv files.  Note that
 * CPLUnescapeString() currently does not support this format, only 
 * CPLEscapeString().  See cpl_csv.cpp for csv parsing support.
 *
 * @param pszInput the string to escape.  
 * @param nLength The number of bytes of data to preserve.  If this is -1
 * the strlen(pszString) function will be used to compute the length.
 * @param nScheme the encoding scheme to use.  
 *
 * @return an escaped, zero terminated string that should be freed with 
 * CPLFree() when no longer needed.
 */

char *CPLEscapeString( const char *pszInput, int nLength, 
                       int nScheme )

{
    char        *pszOutput;
    char        *pszShortOutput;

    if( nLength == -1 )
        nLength = strlen(pszInput);

    pszOutput = (char *) CPLMalloc( nLength * 6 + 1 );
    
    if( nScheme == CPLES_BackslashQuotable )
    {
        int iOut = 0, iIn;

        for( iIn = 0; iIn < nLength; iIn++ )
        {
            if( pszInput[iIn] == '\0' )
            {
                pszOutput[iOut++] = '\\';
                pszOutput[iOut++] = '0';
            }
            else if( pszInput[iIn] == '\n' )
            {
                pszOutput[iOut++] = '\\';
                pszOutput[iOut++] = 'n';
            }
            else if( pszInput[iIn] == '"' )
            {
                pszOutput[iOut++] = '\\';
                pszOutput[iOut++] = '\"';
            }
            else if( pszInput[iIn] == '\\' )
            {
                pszOutput[iOut++] = '\\';
                pszOutput[iOut++] = '\\';
            }
            else
                pszOutput[iOut++] = pszInput[iIn];
        }
        pszOutput[iOut] = '\0';
    }
    else if( nScheme == CPLES_URL ) /* Untested at implementation */
    {
        int iOut = 0, iIn;

        for( iIn = 0; iIn < nLength; iIn++ )
        {
            if( (pszInput[iIn] >= 'a' && pszInput[iIn] <= 'z')
                || (pszInput[iIn] >= 'A' && pszInput[iIn] <= 'Z')
                || (pszInput[iIn] >= '0' && pszInput[iIn] <= '9')
                || pszInput[iIn] == '_' || pszInput[iIn] == '.' )
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
            else
            {
                sprintf( pszOutput+iOut, "%%%02X", ((unsigned char*)pszInput)[iIn] );
                iOut += 3;
            }
        }
        pszOutput[iOut] = '\0';
    }
    else if( nScheme == CPLES_XML || nScheme == CPLES_XML_BUT_QUOTES )
    {
        int iOut = 0, iIn;

        for( iIn = 0; iIn < nLength; iIn++ )
        {
            if( pszInput[iIn] == '<' )
            {
                pszOutput[iOut++] = '&';
                pszOutput[iOut++] = 'l';
                pszOutput[iOut++] = 't';
                pszOutput[iOut++] = ';';
            }
            else if( pszInput[iIn] == '>' )
            {
                pszOutput[iOut++] = '&';
                pszOutput[iOut++] = 'g';
                pszOutput[iOut++] = 't';
                pszOutput[iOut++] = ';';
            }
            else if( pszInput[iIn] == '&' )
            {
                pszOutput[iOut++] = '&';
                pszOutput[iOut++] = 'a';
                pszOutput[iOut++] = 'm';
                pszOutput[iOut++] = 'p';
                pszOutput[iOut++] = ';';
            }
            else if( pszInput[iIn] == '"' && nScheme != CPLES_XML_BUT_QUOTES )
            {
                pszOutput[iOut++] = '&';
                pszOutput[iOut++] = 'q';
                pszOutput[iOut++] = 'u';
                pszOutput[iOut++] = 'o';
                pszOutput[iOut++] = 't';
                pszOutput[iOut++] = ';';
            }
            else if( ((GByte*)pszInput)[iIn] < 0x20 
                     && pszInput[iIn] != 0x9
                     && pszInput[iIn] != 0xA 
                     && pszInput[iIn] != 0xD ) 
            {
                // These control characters are unrepresentable in XML format, 
                // so we just drop them.  #4117
            }
            else
                pszOutput[iOut++] = pszInput[iIn];
        }
        pszOutput[iOut] = '\0';
    }
    else if( nScheme == CPLES_SQL )
    {
        int iOut = 0, iIn;

        for( iIn = 0; iIn < nLength; iIn++ )
        {
            if( pszInput[iIn] == '\'' )
            {
                pszOutput[iOut++] = '\'';
                pszOutput[iOut++] = '\'';
            }
            else
                pszOutput[iOut++] = pszInput[iIn];
        }
        pszOutput[iOut] = '\0';
    }
    else if( nScheme == CPLES_CSV )
    {
        if( strchr( pszInput, '\"' ) == NULL
            && strchr( pszInput, ',') == NULL
            && strchr( pszInput, ';') == NULL
            && strchr( pszInput, '\t') == NULL
            && strchr( pszInput, 10) == NULL 
            && strchr( pszInput, 13) == NULL )
        {
            strcpy( pszOutput, pszInput );
        }
        else
        {
            int iOut = 1, iIn;

            pszOutput[0] = '\"';

            for( iIn = 0; iIn < nLength; iIn++ )
            {
                if( pszInput[iIn] == '\"' )
                {
                    pszOutput[iOut++] = '\"';
                    pszOutput[iOut++] = '\"';
                }
                else
                    pszOutput[iOut++] = pszInput[iIn];
            }
            pszOutput[iOut++] = '\"';
            pszOutput[iOut++] = '\0';
        }
    }
    else
    {
        pszOutput[0] = '\0';
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Undefined escaping scheme (%d) in CPLEscapeString()",
                  nScheme );
    }

    pszShortOutput = CPLStrdup( pszOutput );
    CPLFree( pszOutput );

    return pszShortOutput;
}

/************************************************************************/
/*                         CPLUnescapeString()                          */
/************************************************************************/

/**
 * Unescape a string.
 *
 * This function does the opposite of CPLEscapeString().  Given a string
 * with special values escaped according to some scheme, it will return a
 * new copy of the string returned to it's original form. 
 *
 * @param pszInput the input string.  This is a zero terminated string.
 * @param pnLength location to return the length of the unescaped string, 
 * which may in some cases include embedded '\\0' characters.
 * @param nScheme the escaped scheme to undo (see CPLEscapeString() for a
 * list). 
 * 
 * @return a copy of the unescaped string that should be freed by the 
 * application using CPLFree() when no longer needed.
 */

char *CPLUnescapeString( const char *pszInput, int *pnLength, int nScheme )

{
    char *pszOutput;
    int iOut=0, iIn;

    pszOutput = (char *) CPLMalloc(4 * strlen(pszInput)+1);
    pszOutput[0] = '\0';

    if( nScheme == CPLES_XML || nScheme == CPLES_XML_BUT_QUOTES  )
    {
        char ch;
        for( iIn = 0; (ch = pszInput[iIn]) != '\0'; iIn++ )
        {
            if( ch != '&' )
            {
                pszOutput[iOut++] = ch;
            }
            else if( EQUALN(pszInput+iIn,"&lt;",4) )
            {
                pszOutput[iOut++] = '<';
                iIn += 3;
            }
            else if( EQUALN(pszInput+iIn,"&gt;",4) )
            {
                pszOutput[iOut++] = '>';
                iIn += 3;
            }
            else if( EQUALN(pszInput+iIn,"&amp;",5) )
            {
                pszOutput[iOut++] = '&';
                iIn += 4;
            }
            else if( EQUALN(pszInput+iIn,"&apos;",6) )
            {
                pszOutput[iOut++] = '\'';
                iIn += 5;
            }
            else if( EQUALN(pszInput+iIn,"&quot;",6) )
            {
                pszOutput[iOut++] = '"';
                iIn += 5;
            }
            else if( EQUALN(pszInput+iIn,"&#x",3) )
            {
                wchar_t anVal[2] = {0 , 0};
                iIn += 3;

                while(TRUE)
                {
                    ch = pszInput[iIn ++];
                    if (ch >= 'a' && ch <= 'f')
                        anVal[0] = anVal[0] * 16 + ch - 'a' + 10;
                    else if (ch >= 'A' && ch <= 'A')
                        anVal[0] = anVal[0] * 16 + ch - 'A' + 10;
                    else if (ch >= '0' && ch <= '9')
                        anVal[0] = anVal[0] * 16 + ch - '0';
                    else
                        break;
                }
                if (ch != ';')
                    break;
                iIn --;

                char * pszUTF8 = CPLRecodeFromWChar( anVal, "WCHAR_T", CPL_ENC_UTF8);
                int nLen = strlen(pszUTF8);
                memcpy(pszOutput + iOut, pszUTF8, nLen);
                CPLFree(pszUTF8);
                iOut += nLen;
            }
            else if( EQUALN(pszInput+iIn,"&#",2) )
            {
                char ch;
                wchar_t anVal[2] = {0 , 0};
                iIn += 2;

                while(TRUE)
                {
                    ch = pszInput[iIn ++];
                    if (ch >= '0' && ch <= '9')
                        anVal[0] = anVal[0] * 10 + ch - '0';
                    else
                        break;
                }
                if (ch != ';')
                    break;
                iIn --;

                char * pszUTF8 = CPLRecodeFromWChar( anVal, "WCHAR_T", CPL_ENC_UTF8);
                int nLen = strlen(pszUTF8);
                memcpy(pszOutput + iOut, pszUTF8, nLen);
                CPLFree(pszUTF8);
                iOut += nLen;
            }
            else
            {
                /* illegal escape sequence */
                CPLDebug( "CPL",
                          "Error unescaping CPLES_XML text, '&' character followed by unhandled escape sequence." );
                break;
            }
        }
    }
    else if( nScheme == CPLES_URL )
    {
        for( iIn = 0; pszInput[iIn] != '\0'; iIn++ )
        {
            if( pszInput[iIn] == '%' 
                && pszInput[iIn+1] != '\0' 
                && pszInput[iIn+2] != '\0' )
            {
                int nHexChar = 0;

                if( pszInput[iIn+1] >= 'A' && pszInput[iIn+1] <= 'F' )
                    nHexChar += 16 * (pszInput[iIn+1] - 'A' + 10);
                else if( pszInput[iIn+1] >= 'a' && pszInput[iIn+1] <= 'f' )
                    nHexChar += 16 * (pszInput[iIn+1] - 'a' + 10);
                else if( pszInput[iIn+1] >= '0' && pszInput[iIn+1] <= '9' )
                    nHexChar += 16 * (pszInput[iIn+1] - '0');
                else
                    CPLDebug( "CPL", 
                              "Error unescaping CPLES_URL text, percent not "
                              "followed by two hex digits." );
                    
                if( pszInput[iIn+2] >= 'A' && pszInput[iIn+2] <= 'F' )
                    nHexChar += pszInput[iIn+2] - 'A' + 10;
                else if( pszInput[iIn+2] >= 'a' && pszInput[iIn+2] <= 'f' )
                    nHexChar += pszInput[iIn+2] - 'a' + 10;
                else if( pszInput[iIn+2] >= '0' && pszInput[iIn+2] <= '9' )
                    nHexChar += pszInput[iIn+2] - '0';
                else
                    CPLDebug( "CPL", 
                              "Error unescaping CPLES_URL text, percent not "
                              "followed by two hex digits." );

                pszOutput[iOut++] = (char) nHexChar;
                iIn += 2;
            }
            else if( pszInput[iIn] == '+' )
            {
                pszOutput[iOut++] = ' ';
            }   
            else
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
        }
    }
    else if( nScheme == CPLES_SQL )
    {
        for( iIn = 0; pszInput[iIn] != '\0'; iIn++ )
        {
            if( pszInput[iIn] == '\'' && pszInput[iIn+1] == '\'' )
            {
                iIn++;
                pszOutput[iOut++] = pszInput[iIn];
            }
            else
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
        }
    }
    else /* if( nScheme == CPLES_BackslashQuoteable ) */
    {
        for( iIn = 0; pszInput[iIn] != '\0'; iIn++ )
        {
            if( pszInput[iIn] == '\\' )
            {
                iIn++;
                if( pszInput[iIn] == 'n' )
                    pszOutput[iOut++] = '\n';
                else if( pszInput[iIn] == '0' )
                    pszOutput[iOut++] = '\0';
                else 
                    pszOutput[iOut++] = pszInput[iIn];
            }
            else
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
        }
    }

    pszOutput[iOut] = '\0';

    if( pnLength != NULL )
        *pnLength = iOut;

    return pszOutput;
}

/************************************************************************/
/*                           CPLBinaryToHex()                           */
/************************************************************************/

/**
 * Binary to hexadecimal translation.
 *
 * @param nBytes number of bytes of binary data in pabyData.
 * @param pabyData array of data bytes to translate. 
 * 
 * @return hexadecimal translation, zero terminated.  Free with CPLFree().
 */

char *CPLBinaryToHex( int nBytes, const GByte *pabyData )

{
    char *pszHex = (char *) CPLMalloc(nBytes * 2 + 1 );
    int i;
    static const char achHex[] = "0123456789ABCDEF";

    pszHex[nBytes*2] = '\0';

    for( i = 0; i < nBytes; i++ )
    {
        int nLow = pabyData[i] & 0x0f;
        int nHigh = (pabyData[i] & 0xf0) >> 4;

        pszHex[i*2] = achHex[nHigh];
        pszHex[i*2+1] = achHex[nLow];
    }

    return pszHex;
}


/************************************************************************/
/*                           CPLHexToBinary()                           */
/************************************************************************/

/**
 * Hexadecimal to binary translation
 *
 * @param pszHex the input hex encoded string.
 * @param pnBytes the returned count of decoded bytes placed here.
 *
 * @return returns binary buffer of data - free with CPLFree().
 */

static const unsigned char hex2char[256] = {
    /* not Hex characters */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0-9 */
    0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
       /* A-F */
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
    /* not Hex characters */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      /* a-f */
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    /* not Hex characters (upper 128 characters) */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

GByte *CPLHexToBinary( const char *pszHex, int *pnBytes )
{
    size_t  nHexLen = strlen(pszHex);
    size_t i;
    register unsigned char h1, h2;
    GByte *pabyWKB; 

    pabyWKB = (GByte *) CPLMalloc(nHexLen / 2 + 2);
            
    for( i = 0; i < nHexLen/2; i++ )
    {
        h1 = hex2char[(int)pszHex[2*i]];
        h2 = hex2char[(int)pszHex[2*i+1]];

        /* First character is high bits, second is low bits */
        pabyWKB[i] = (GByte)((h1 << 4) | h2);
    }
    pabyWKB[nHexLen/2] = 0;
    *pnBytes = nHexLen/2;
    
    return pabyWKB;

}

/************************************************************************/
/*                         CPLGetValueType()                            */
/************************************************************************/

/**
 * Detect the type of the value contained in a string, whether it is
 * a real, an integer or a string
 * Leading and trailing spaces are skipped in the analysis.
 *
 * Note: in the context of this function, integer must be understood in a
 * broad sense. It does not mean that the value can fit into a 32 bit integer
 * for example. It might be larger.
 *
 * @param pszValue the string to analyze
 *
 * @return returns the type of the value contained in the string.
 */

CPLValueType CPLGetValueType(const char* pszValue)
{
    /*
    doubles : "+25.e+3", "-25.e-3", "25.e3", "25e3", " 25e3 "
    not doubles: "25e 3", "25e.3", "-2-5e3", "2-5e3", "25.25.3", "-3d"
    */

    int bFoundDot = FALSE;
    int bFoundExponent = FALSE;
    int bIsLastCharExponent = FALSE;
    int bIsReal = FALSE;

    if (pszValue == NULL)
        return CPL_VALUE_STRING;

    /* Skip leading spaces */
    while( isspace( (unsigned char)*pszValue ) )
        pszValue ++;

    if (*pszValue == '\0')
        return CPL_VALUE_STRING;

    /* Skip leading + or - */
    if (*pszValue == '+' || *pszValue == '-')
        pszValue ++;

    for(; *pszValue != '\0'; pszValue++ )
    {
        if( isdigit( *pszValue))
        {
            bIsLastCharExponent = FALSE;
            /* do nothing */
        }
        else if ( isspace ((unsigned char)*pszValue) )
        {
            const char* pszTmp = pszValue;
            while( isspace( (unsigned char)*pszTmp ) )
                pszTmp ++;
            if (*pszTmp == 0)
                break;
            else
                return CPL_VALUE_STRING;
        }
        else if ( *pszValue == '-' || *pszValue == '+' )
        {
            if (bIsLastCharExponent)
            {
                /* do nothing */
            }
            else
                return CPL_VALUE_STRING;
            bIsLastCharExponent = FALSE;
        }
        else if ( *pszValue == '.')
        {
            bIsReal = TRUE;
            if (!bFoundDot && bIsLastCharExponent == FALSE)
                bFoundDot = TRUE;
            else
                return CPL_VALUE_STRING;
            bIsLastCharExponent = FALSE;
        }
        else if (*pszValue == 'D' || *pszValue == 'd'
                 || *pszValue == 'E' || *pszValue == 'e' )
        {
            if (!(pszValue[1] == '+' || pszValue[1] == '-' ||
                  isdigit(pszValue[1])))
                return CPL_VALUE_STRING;

            bIsReal = TRUE;
            if (!bFoundExponent)
                bFoundExponent = TRUE;
            else
                return CPL_VALUE_STRING;
            bIsLastCharExponent = TRUE;
        }
        else 
        {
            return CPL_VALUE_STRING;
        }
    }

    return (bIsReal) ? CPL_VALUE_REAL : CPL_VALUE_INTEGER;
}

/************************************************************************/
/*                              CPLStrlcpy()                            */
/************************************************************************/

/**
 * Copy source string to a destination buffer.
 *
 * This function ensures that the destination buffer is always NUL terminated
 * (provided that its length is at least 1).
 *
 * This function is designed to be a safer, more consistent, and less error
 * prone replacement for strncpy. Its contract is identical to libbsd's strlcpy.
 *
 * Truncation can be detected by testing if the return value of CPLStrlcpy
 * is greater or equal to nDestSize.

\verbatim
char szDest[5];
if (CPLStrlcpy(szDest, "abcde", sizeof(szDest)) >= sizeof(szDest))
    fprintf(stderr, "truncation occured !\n");
\endverbatim

 * @param pszDest   destination buffer
 * @param pszSrc    source string. Must be NUL terminated
 * @param nDestSize size of destination buffer (including space for the NUL terminator character)
 *
 * @return the length of the source string (=strlen(pszSrc))
 *
 * @since GDAL 1.7.0
 */
size_t CPLStrlcpy(char* pszDest, const char* pszSrc, size_t nDestSize)
{
    char* pszDestIter = pszDest;
    const char* pszSrcIter = pszSrc;

    if (nDestSize == 0)
        return strlen(pszSrc);

    nDestSize --;
    while(nDestSize != 0 && *pszSrcIter != '\0')
    {
        *pszDestIter = *pszSrcIter;
        pszDestIter ++;
        pszSrcIter ++;
        nDestSize --;
    }
    *pszDestIter = '\0';
    return pszSrcIter - pszSrc + strlen(pszSrcIter);
}

/************************************************************************/
/*                              CPLStrlcat()                            */
/************************************************************************/

/**
 * Appends a source string to a destination buffer.
 *
 * This function ensures that the destination buffer is always NUL terminated
 * (provided that its length is at least 1 and that there is at least one byte
 * free in pszDest, that is to say strlen(pszDest_before) < nDestSize)
 *
 * This function is designed to be a safer, more consistent, and less error
 * prone replacement for strncat. Its contract is identical to libbsd's strlcat.
 *
 * Truncation can be detected by testing if the return value of CPLStrlcat
 * is greater or equal to nDestSize.

\verbatim
char szDest[5];
CPLStrlcpy(szDest, "ab", sizeof(szDest));
if (CPLStrlcat(szDest, "cde", sizeof(szDest)) >= sizeof(szDest))
    fprintf(stderr, "truncation occured !\n");
\endverbatim

 * @param pszDest   destination buffer. Must be NUL terminated before running CPLStrlcat
 * @param pszSrc    source string. Must be NUL terminated
 * @param nDestSize size of destination buffer (including space for the NUL terminator character)
 *
 * @return the thoretical length of the destination string after concatenation
 *         (=strlen(pszDest_before) + strlen(pszSrc)).
 *         If strlen(pszDest_before) >= nDestSize, then it returns nDestSize + strlen(pszSrc)
 *
 * @since GDAL 1.7.0
 */
size_t CPLStrlcat(char* pszDest, const char* pszSrc, size_t nDestSize)
{
    char* pszDestIter = pszDest;

    while(nDestSize != 0 && *pszDestIter != '\0')
    {
        pszDestIter ++;
        nDestSize --;
    }

    return pszDestIter - pszDest + CPLStrlcpy(pszDestIter, pszSrc, nDestSize);
}

/************************************************************************/
/*                              CPLStrnlen()                            */
/************************************************************************/

/**
 * Returns the length of a NUL terminated string by reading at most
 * the specified number of bytes.
 *
 * The CPLStrnlen() function returns MIN(strlen(pszStr), nMaxLen).
 * Only the first nMaxLen bytes of the string will be read. Usefull to
 * test if a string contains at least nMaxLen characters without reading
 * the full string up to the NUL terminating character.
 *
 * @param pszStr    a NUL terminated string
 * @param nMaxLen   maximum number of bytes to read in pszStr
 *
 * @return strlen(pszStr) if the length is lesser than nMaxLen, otherwise
 * nMaxLen if the NUL character has not been found in the first nMaxLen bytes.
 *
 * @since GDAL 1.7.0
 */
 
size_t CPLStrnlen (const char *pszStr, size_t nMaxLen)
{
    size_t nLen = 0;
    while(nLen < nMaxLen && *pszStr != '\0')
    {
        nLen ++;
        pszStr ++;
    }
    return nLen;
}
