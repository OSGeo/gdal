/**********************************************************************
 *
 * Name:     cpl_string.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  String and Stringlist manipulation functions.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1998, Daniel Morissette
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#undef WARN_STANDARD_PRINTF

#include "cpl_port.h"
#include "cpl_string.h"

#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>

#include <limits>

#include "cpl_config.h"
#include "cpl_multiproc.h"
#include "cpl_vsi.h"

#if !defined(va_copy) && defined(__va_copy)
#define va_copy __va_copy
#endif

CPL_CVSID("$Id$")

/*=====================================================================
                    StringList manipulation functions.
 =====================================================================*/

/**********************************************************************
 *                       CSLAddString()
 **********************************************************************/

/** Append a string to a StringList and return a pointer to the modified
 * StringList.
 *
 * If the input StringList is NULL, then a new StringList is created.
 * Note that CSLAddString performance when building a list is in O(n^2)
 * which can cause noticeable slow down when n > 10000.
 */
char **CSLAddString( char **papszStrList, const char *pszNewString )
{
    char** papszRet = CSLAddStringMayFail(papszStrList, pszNewString);
    if( papszRet == nullptr && pszNewString != nullptr )
        abort();
    return papszRet;
}

/** Same as CSLAddString() but may return NULL in case of (memory) failure */
char **CSLAddStringMayFail( char **papszStrList, const char *pszNewString )
{
    if( pszNewString == nullptr )
        return papszStrList;  // Nothing to do!

    char* pszDup = VSI_STRDUP_VERBOSE(pszNewString);
    if( pszDup == nullptr )
        return nullptr;

    // Allocate room for the new string.
    char** papszStrListNew = nullptr;
    int nItems = 0;

    if( papszStrList == nullptr )
        papszStrListNew = static_cast<char**>(
            VSI_CALLOC_VERBOSE( 2, sizeof(char*) ) );
    else
    {
        nItems = CSLCount(papszStrList);
        papszStrListNew = static_cast<char**>(
            VSI_REALLOC_VERBOSE( papszStrList, (nItems+2)*sizeof(char*) ) );
    }
    if( papszStrListNew == nullptr )
    {
        VSIFree(pszDup);
        return nullptr;
    }

    // Copy the string in the list.
    papszStrListNew[nItems] = pszDup;
    papszStrListNew[nItems+1] = nullptr;

    return papszStrListNew;
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
int CSLCount( CSLConstList papszStrList )
{
    if( !papszStrList )
        return 0;

    int nItems = 0;

    while( *papszStrList != nullptr )
    {
        ++nItems;
        ++papszStrList;
    }

    return nItems;
}

/************************************************************************/
/*                            CSLGetField()                             */
/************************************************************************/

/**
 * Fetches the indicated field, being careful not to crash if the field
 * doesn't exist within this string list.
 *
 * The returned pointer should not be freed, and doesn't necessarily last long.
 */
const char *CSLGetField( CSLConstList papszStrList, int iField )

{
    if( papszStrList == nullptr || iField < 0 )
        return( "" );

    for( int i = 0; i < iField+1; i++ )
    {
        if( papszStrList[i] == nullptr )
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
void CPL_STDCALL CSLDestroy( char **papszStrList )
{
    if( !papszStrList )
        return;

    for( char **papszPtr = papszStrList; *papszPtr != nullptr; ++papszPtr )
    {
        CPLFree(*papszPtr);
    }

    CPLFree(papszStrList);
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

char **CSLDuplicate( CSLConstList papszStrList )
{
    const int nLines = CSLCount(papszStrList);

    if( nLines == 0 )
        return nullptr;

    CSLConstList papszSrc = papszStrList;

    char **papszNewList = static_cast<char **>(
        CPLMalloc( (nLines + 1) * sizeof(char*) ) );

    char **papszDst = papszNewList;

    for( ; *papszSrc != nullptr; ++papszSrc, ++papszDst)
    {
        *papszDst = CPLStrdup(*papszSrc);
    }
    *papszDst = nullptr;

    return papszNewList;
}

/************************************************************************/
/*                               CSLMerge                               */
/************************************************************************/

/**
 * \brief Merge two lists.
 *
 * The two lists are merged, ensuring that if any keys appear in both
 * that the value from the second (papszOverride) list take precedence.
 *
 * @param papszOrig the original list, being modified.
 * @param papszOverride the list of items being merged in.  This list
 * is unaltered and remains owned by the caller.
 *
 * @return updated list.
 */

char **CSLMerge( char **papszOrig, CSLConstList papszOverride )

{
    if( papszOrig == nullptr && papszOverride != nullptr )
        return CSLDuplicate( papszOverride );

    if( papszOverride == nullptr )
        return papszOrig;

    for( int i = 0; papszOverride[i] != nullptr; ++i )
    {
        char *pszKey = nullptr;
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
 * @param nMaxLines maximum number of lines to read before stopping, or -1 for
 * no limit.
 * @param nMaxCols maximum number of characters in a line before stopping, or -1
 * for no limit.
 * @param papszOptions NULL-terminated array of options. Unused for now.
 *
 * @return a string list with the files lines, now owned by caller. To be freed
 * with CSLDestroy()
 *
 * @since GDAL 1.7.0
 */

char **CSLLoad2( const char *pszFname, int nMaxLines, int nMaxCols,
                 CSLConstList papszOptions )
{
    VSILFILE *fp = VSIFOpenL(pszFname, "rb");

    if( !fp )
    {
        if( CPLFetchBool( papszOptions, "EMIT_ERROR_IF_CANNOT_OPEN_FILE",
                          true ) )
        {
            // Unable to open file.
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "CSLLoad2(\"%s\") failed: unable to open file.",
                    pszFname );
        }
        return nullptr;
    }

    char **papszStrList=nullptr;
    int nLines = 0;
    int nAllocatedLines = 0;

    CPLErrorReset();
    while( !VSIFEofL(fp) && (nMaxLines == -1 || nLines < nMaxLines) )
    {
        const char *pszLine = CPLReadLine2L(fp, nMaxCols, papszOptions);
        if( pszLine == nullptr )
            break;

        if( nLines + 1 >= nAllocatedLines )
        {
            nAllocatedLines = 16 + nAllocatedLines * 2;
            char** papszStrListNew =
                static_cast<char**>(
                    VSIRealloc( papszStrList,
                                nAllocatedLines * sizeof(char*) ) );
            if( papszStrListNew == nullptr )
            {
                CPL_IGNORE_RET_VAL( VSIFCloseL(fp) );
                CPLReadLineL( nullptr );
                CPLError( CE_Failure, CPLE_OutOfMemory,
                          "CSLLoad2(\"%s\") "
                          "failed: not enough memory to allocate lines.",
                          pszFname );
                return papszStrList;
            }
            papszStrList = papszStrListNew;
        }
        papszStrList[nLines] = CPLStrdup(pszLine);
        papszStrList[nLines + 1] = nullptr;
        ++nLines;
    }

    CPL_IGNORE_RET_VAL( VSIFCloseL(fp) );

    // Free the internal thread local line buffer.
    CPLReadLineL( nullptr );

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
 * @return a string list with the files lines, now owned by caller. To be freed
 * with CSLDestroy()
 */

char **CSLLoad( const char *pszFname )
{
    return CSLLoad2(pszFname, -1, -1, nullptr);
}

/**********************************************************************
 *                       CSLSave()
 **********************************************************************/

/** Write a StringList to a text file.
 *
 * Returns the number of lines written, or 0 if the file could not
 * be written.
 */

int CSLSave( CSLConstList papszStrList, const char *pszFname )
{
    if( papszStrList == nullptr )
        return 0;

    VSILFILE *fp = VSIFOpenL( pszFname, "wt" );
    if( fp == nullptr )
    {
        // Unable to open file.
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "CSLSave(\"%s\") failed: unable to open output file.",
                  pszFname );
        return 0;
    }

    int nLines = 0;
    while( *papszStrList != nullptr )
    {
        if( VSIFPrintfL( fp, "%s\n", *papszStrList ) < 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "CSLSave(\"%s\") failed: unable to write to output file.",
                      pszFname );
            break;  // A Problem happened... abort.
        }

        ++nLines;
        ++papszStrList;
    }

    if( VSIFCloseL(fp) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "CSLSave(\"%s\") failed: unable to write to output file.",
                  pszFname );
    }

    return nLines;
}

/**********************************************************************
 *                       CSLPrint()
 **********************************************************************/

/** Print a StringList to fpOut.  If fpOut==NULL, then output is sent
 * to stdout.
 *
 * Returns the number of lines printed.
 */
int CSLPrint( CSLConstList papszStrList, FILE *fpOut )
{
    if( !papszStrList )
        return 0;

    if( fpOut == nullptr )
        fpOut = stdout;

    int nLines = 0;

    while( *papszStrList != nullptr )
    {
        if( VSIFPrintf(fpOut, "%s\n", *papszStrList) < 0 )
            return nLines;
        ++nLines;
        ++papszStrList;
    }

    return nLines;
}

/**********************************************************************
 *                       CSLInsertStrings()
**********************************************************************/

/** Copies the contents of a StringList inside another StringList
 * before the specified line.
 *
 * nInsertAtLineNo is a 0-based line index before which the new strings
 * should be inserted.  If this value is -1 or is larger than the actual
 * number of strings in the list then the strings are added at the end
 * of the source StringList.
 *
 * Returns the modified StringList.
 */

char **CSLInsertStrings( char **papszStrList, int nInsertAtLineNo,
                         CSLConstList papszNewLines )
{
    if( papszNewLines == nullptr )
        return papszStrList;  // Nothing to do!

    const int nToInsert = CSLCount(papszNewLines);
    if( nToInsert == 0 )
        return papszStrList;  // Nothing to do!

    const int nSrcLines = CSLCount(papszStrList);
    const int nDstLines = nSrcLines + nToInsert;

    // Allocate room for the new strings.
    papszStrList = static_cast<char**>(
        CPLRealloc( papszStrList, (nDstLines+1) * sizeof(char*) ) );

    // Make sure the array is NULL-terminated.  It may not be if
    // papszStrList was NULL before Realloc().
    papszStrList[nSrcLines] = nullptr;

    // Make some room in the original list at the specified location.
    // Note that we also have to move the NULL pointer at the end of
    // the source StringList.
    if( nInsertAtLineNo == -1 || nInsertAtLineNo > nSrcLines )
        nInsertAtLineNo = nSrcLines;

    {
        char **ppszSrc = papszStrList + nSrcLines;
        char **ppszDst = papszStrList + nDstLines;

        for( int i = nSrcLines; i >= nInsertAtLineNo; --i )
        {
            *ppszDst = *ppszSrc;
            --ppszDst;
            --ppszSrc;
        }
    }

    // Copy the strings to the list.
    CSLConstList ppszSrc = papszNewLines;
    char** ppszDst = papszStrList + nInsertAtLineNo;

    for( ; *ppszSrc != nullptr; ++ppszSrc, ++ppszDst )
    {
        *ppszDst = CPLStrdup(*ppszSrc);
    }

    return papszStrList;
}

/**********************************************************************
 *                       CSLInsertString()
 **********************************************************************/

/** Insert a string at a given line number inside a StringList
 *
 * nInsertAtLineNo is a 0-based line index before which the new string
 * should be inserted.  If this value is -1 or is larger than the actual
 * number of strings in the list then the string is added at the end
 * of the source StringList.
 *
 * Returns the modified StringList.
 */

char **CSLInsertString( char **papszStrList, int nInsertAtLineNo,
                        const char *pszNewLine )
{
    char *apszList[2] = { const_cast<char *>(pszNewLine), nullptr };

    return CSLInsertStrings(papszStrList, nInsertAtLineNo, apszList);
}

/**********************************************************************
 *                       CSLRemoveStrings()
**********************************************************************/

/** Remove strings inside a StringList
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
 */

char **CSLRemoveStrings( char **papszStrList, int nFirstLineToDelete,
                         int nNumToRemove, char ***ppapszRetStrings )
{
    const int nSrcLines = CSLCount(papszStrList);

    if( nNumToRemove < 1 || nSrcLines == 0 )
      return papszStrList;  // Nothing to do!

    // If operation will result in an empty StringList, don't waste
    // time here.
    const int nDstLines = nSrcLines - nNumToRemove;
    if( nDstLines < 1 )
    {
        CSLDestroy(papszStrList);
        return nullptr;
    }

    // Remove lines from the source StringList.
    // Either free() each line or store them to a new StringList depending on
    // the caller's choice.
    char **ppszDst = papszStrList + nFirstLineToDelete;

    if( ppapszRetStrings == nullptr )
    {
        // free() all the strings that will be removed.
        for( int i = 0; i < nNumToRemove; ++i )
        {
            CPLFree(*ppszDst);
            *ppszDst = nullptr;
        }
    }
    else
    {
        // Store the strings to remove in a new StringList.
        *ppapszRetStrings = static_cast<char **>(
            CPLCalloc( nNumToRemove+1, sizeof(char*) ) );

        for( int i=0; i < nNumToRemove; ++i )
        {
            (*ppapszRetStrings)[i] = *ppszDst;
            *ppszDst = nullptr;
            ++ppszDst;
        }
    }

    // Shift down all the lines that follow the lines to remove.
    if( nFirstLineToDelete == -1 || nFirstLineToDelete > nSrcLines )
        nFirstLineToDelete = nDstLines;

    char **ppszSrc = papszStrList + nFirstLineToDelete + nNumToRemove;
    ppszDst = papszStrList + nFirstLineToDelete;

    for( ; *ppszSrc != nullptr; ++ppszSrc, ++ppszDst )
    {
        *ppszDst = *ppszSrc;
    }
    // Move the NULL pointer at the end of the StringList.
    *ppszDst = *ppszSrc;

    // At this point, we could realloc() papszStrList to a smaller size, but
    // since this array will likely grow again in further operations on the
    // StringList we'll leave it as it is.
    return papszStrList;
}

/************************************************************************/
/*                           CSLFindString()                            */
/************************************************************************/

/**
 * Find a string within a string list (case insensitive).
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

int CSLFindString( CSLConstList papszList, const char * pszTarget )

{
    if( papszList == nullptr )
        return -1;

    for( int i = 0; papszList[i] != nullptr; ++i )
    {
        if( EQUAL(papszList[i], pszTarget) )
            return i;
    }

    return -1;
}

/************************************************************************/
/*                     CSLFindStringCaseSensitive()                     */
/************************************************************************/

/**
 * Find a string within a string list(case sensitive)
 *
 * Returns the index of the entry in the string list that contains the
 * target string.  The string in the string list must be a full match for
 * the target.
 *
 * @param papszList the string list to be searched.
 * @param pszTarget the string to be searched for.
 *
 * @return the index of the string within the list or -1 on failure.
 *
 * @since GDAL 2.0
 */

int CSLFindStringCaseSensitive( CSLConstList papszList,
                                const char * pszTarget )

{
    if( papszList == nullptr )
        return -1;

    for( int i = 0; papszList[i] != nullptr; ++i )
    {
        if( strcmp(papszList[i], pszTarget) == 0 )
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

int CSLPartialFindString( CSLConstList papszHaystack,
                          const char * pszNeedle )
{
    if( papszHaystack == nullptr || pszNeedle == nullptr )
        return -1;

    for( int i = 0; papszHaystack[i] != nullptr; ++i )
    {
        if( strstr(papszHaystack[i], pszNeedle) )
            return i;
    }

    return -1;
}

/**********************************************************************
 *                       CSLTokenizeString()
**********************************************************************/

/** Tokenizes a string and returns a StringList with one string for
 * each token.
 */
char **CSLTokenizeString( const char *pszString )
{
    return CSLTokenizeString2( pszString, " ", CSLT_HONOURSTRINGS );
}

/************************************************************************/
/*                      CSLTokenizeStringComplex()                      */
/************************************************************************/

/** Obsolete tokenizing api. Use CSLTokenizeString2() */
char ** CSLTokenizeStringComplex( const char * pszString,
                                  const char * pszDelimiters,
                                  int bHonourStrings, int bAllowEmptyTokens )
{
    int nFlags = 0;

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
 * delimiter(s) with a variety of options.  The returned result is a
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
    char **papszTokens =
        CSLTokenizeString2( pszCommand, " \t\n",
                            CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );

    for( int i = 0; papszTokens != NULL && papszTokens[i] != NULL; ++i )
        printf( "arg %d: '%s'", papszTokens[i] );  // ok

    CSLDestroy( papszTokens );
\endcode

 * @param pszString the string to be split into tokens.
 * @param pszDelimiters one or more characters to be used as token delimiters.
 * @param nCSLTFlags an ORing of one or more of the CSLT_ flag values.
 *
 * @return a string list of tokens owned by the caller.
 */

char ** CSLTokenizeString2( const char * pszString,
                            const char * pszDelimiters,
                            int nCSLTFlags )
{
    if( pszString == nullptr )
        return static_cast<char **>(
            CPLCalloc(sizeof(char *), 1));

    CPLStringList oRetList;
    const bool bHonourStrings = (nCSLTFlags & CSLT_HONOURSTRINGS) != 0;
    const bool bAllowEmptyTokens = (nCSLTFlags & CSLT_ALLOWEMPTYTOKENS) != 0;
    const bool bStripLeadSpaces = (nCSLTFlags & CSLT_STRIPLEADSPACES) != 0;
    const bool bStripEndSpaces = (nCSLTFlags & CSLT_STRIPENDSPACES) != 0;

    char *pszToken = static_cast<char *>(CPLCalloc(10, 1));
    int nTokenMax = 10;

    while( *pszString != '\0' )
    {
        bool bInString = false;
        bool bStartString = true;
        int nTokenLen = 0;

        // Try to find the next delimiter, marking end of token.
        for( ; *pszString != '\0'; ++pszString )
        {
            // Extend token buffer if we are running close to its end.
            if( nTokenLen >= nTokenMax-3 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = static_cast<char *>(
                    CPLRealloc( pszToken, nTokenMax ));
            }

            // End if this is a delimiter skip it and break.
            if( !bInString && strchr(pszDelimiters, *pszString) != nullptr )
            {
                ++pszString;
                break;
            }

            // If this is a quote, and we are honouring constant
            // strings, then process the constant strings, with out delim
            // but don't copy over the quotes.
            if( bHonourStrings && *pszString == '"' )
            {
                if( nCSLTFlags & CSLT_PRESERVEQUOTES )
                {
                    pszToken[nTokenLen] = *pszString;
                    ++nTokenLen;
                }

                bInString = !bInString;
                continue;
            }

            /*
             * Within string constants we allow for escaped quotes, but in
             * processing them we will unescape the quotes and \\ sequence
             * reduces to \
             */
            if( bInString && pszString[0] == '\\' )
            {
                if( pszString[1] == '"' || pszString[1] == '\\' )
                {
                    if( nCSLTFlags & CSLT_PRESERVEESCAPES )
                    {
                        pszToken[nTokenLen] = *pszString;
                        ++nTokenLen;
                    }

                    ++pszString;
                }
            }

            // Strip spaces at the token start if requested.
            if( !bInString && bStripLeadSpaces
                && bStartString
                && isspace(static_cast<unsigned char>( *pszString )) )
                continue;

            bStartString = false;

            pszToken[nTokenLen] = *pszString;
            ++nTokenLen;
        }

        // Strip spaces at the token end if requested.
        if( !bInString && bStripEndSpaces )
        {
            while( nTokenLen
                   && isspace(
                       static_cast<unsigned char>( pszToken[nTokenLen - 1]) ) )
                nTokenLen--;
        }

        pszToken[nTokenLen] = '\0';

        // Add the token.
        if( pszToken[0] != '\0' || bAllowEmptyTokens )
            oRetList.AddString( pszToken );
    }

    /*
     * If the last token was empty, then we need to capture
     * it now, as the loop would skip it.
     */
    if( *pszString == '\0' && bAllowEmptyTokens && oRetList.Count() > 0
        && strchr(pszDelimiters, *(pszString-1)) != nullptr )
    {
        oRetList.AddString( "" );
    }

    CPLFree( pszToken );

    if( oRetList.List() == nullptr )
    {
        // Prefer to return empty lists as a pointer to
        // a null pointer since some client code might depend on this.
        oRetList.Assign(static_cast<char **>(CPLCalloc(sizeof(char*), 1)));
    }

    return oRetList.StealList();
}

/**********************************************************************
 *                       CPLSPrintf()
 *
 * NOTE: This function should move to cpl_conv.cpp.
 **********************************************************************/

// For now, assume that a 8000 chars buffer will be enough.
constexpr int CPLSPrintf_BUF_SIZE = 8000;
constexpr int CPLSPrintf_BUF_Count = 10;

/** CPLSPrintf() that works with 10 static buffer.
 *
 * It returns a ref. to a static buffer that should not be freed and
 * is valid only until the next call to CPLSPrintf().
 */

const char *CPLSPrintf( CPL_FORMAT_STRING(const char *fmt), ... )
{
    va_list args;

/* -------------------------------------------------------------------- */
/*      Get the thread local buffer ring data.                          */
/* -------------------------------------------------------------------- */
    char *pachBufRingInfo = static_cast<char *>( CPLGetTLS( CTLS_CPLSPRINTF ) );

    if( pachBufRingInfo == nullptr )
    {
        pachBufRingInfo = static_cast<char *>(
            CPLCalloc(
                1, sizeof(int) + CPLSPrintf_BUF_Count*CPLSPrintf_BUF_SIZE));
        CPLSetTLS( CTLS_CPLSPRINTF, pachBufRingInfo, TRUE );
    }

/* -------------------------------------------------------------------- */
/*      Work out which string in the "ring" we want to use this         */
/*      time.                                                           */
/* -------------------------------------------------------------------- */
    int *pnBufIndex = reinterpret_cast<int *>( pachBufRingInfo );
    const size_t nOffset = sizeof(int) + *pnBufIndex * CPLSPrintf_BUF_SIZE;
    char *pachBuffer = pachBufRingInfo + nOffset;

    *pnBufIndex = (*pnBufIndex + 1) % CPLSPrintf_BUF_Count;

/* -------------------------------------------------------------------- */
/*      Format the result.                                              */
/* -------------------------------------------------------------------- */

    va_start(args, fmt);

    const int ret = CPLvsnprintf(pachBuffer, CPLSPrintf_BUF_SIZE-1, fmt, args);
    if( ret < 0 || ret >= CPLSPrintf_BUF_SIZE-1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "CPLSPrintf() called with too "
                  "big string. Output will be truncated !" );
    }

    va_end(args);

    return pachBuffer;
}

/**********************************************************************
 *                       CSLAppendPrintf()
 **********************************************************************/

/** Use CPLSPrintf() to append a new line at the end of a StringList.
 * Returns the modified StringList.
 */
char **CSLAppendPrintf( char **papszStrList,
                        CPL_FORMAT_STRING(const char *fmt), ... )
{
    va_list args;

    va_start( args, fmt );
    CPLString osWork;
    osWork.vPrintf( fmt, args );
    va_end( args );

    return CSLAddString(papszStrList, osWork);
}

/************************************************************************/
/*                            CPLVASPrintf()                            */
/************************************************************************/

/** This is intended to serve as an easy to use C callable vasprintf()
  * alternative.  Used in the GeoJSON library for instance */
int CPLVASPrintf( char **buf, CPL_FORMAT_STRING(const char *fmt), va_list ap )

{
    CPLString osWork;

    osWork.vPrintf( fmt, ap );

    if( buf )
        *buf = CPLStrdup(osWork.c_str());

    return static_cast<int>(osWork.size());
}

/************************************************************************/
/*                  CPLvsnprintf_get_end_of_formatting()                */
/************************************************************************/

static const char* CPLvsnprintf_get_end_of_formatting( const char* fmt )
{
    char ch = '\0';
    // Flag.
    for( ; (ch = *fmt) != '\0'; ++fmt )
    {
        if( ch == '\'' )
            continue;  // Bad idea as this is locale specific.
        if( ch == '-' || ch == '+' || ch == ' ' || ch == '#' || ch == '0' )
            continue;
        break;
    }

    // Field width.
    for( ; (ch = *fmt) != '\0'; ++fmt )
    {
        if( ch == '$' )
            return nullptr;  // Do not support this.
        if( *fmt >= '0' && *fmt <= '9' )
            continue;
        break;
    }

    // Precision.
    if( ch == '.' )
    {
        ++fmt;
        for( ; (ch = *fmt) != '\0'; ++fmt )
        {
            if( ch == '$' )
                return nullptr;  // Do not support this.
            if( *fmt >= '0' && *fmt <= '9' )
                continue;
            break;
        }
    }

    // Length modifier.
    for( ; (ch = *fmt) != '\0'; ++fmt )
    {
        if( ch == 'h' || ch == 'l' || ch == 'j' || ch == 'z' ||
            ch == 't' || ch == 'L' )
            continue;
        else if( ch == 'I' && fmt[1] == '6' && fmt[2] == '4' )
            fmt += 2;
        else
            return fmt;
    }

    return nullptr;
}

/************************************************************************/
/*                           CPLvsnprintf()                             */
/************************************************************************/

#define call_native_snprintf(type) \
    local_ret = snprintf(str + offset_out, size - offset_out, localfmt, \
                         va_arg(wrk_args, type))

/** vsnprintf() wrapper that is not sensitive to LC_NUMERIC settings.
  *
  * This function has the same contract as standard vsnprintf(), except that
  * formatting of floating-point numbers will use decimal point, whatever the
  * current locale is set.
  *
  * @param str output buffer
  * @param size size of the output buffer (including space for terminating nul)
  * @param fmt formatting string
  * @param args arguments
  * @return the number of characters (excluding terminating nul) that would be
  * written if size is big enough. Or potentially -1 with Microsoft C runtime
  * for Visual Studio < 2015.
  * @since GDAL 2.0
  */
int CPLvsnprintf( char *str, size_t size,
                  CPL_FORMAT_STRING(const char* fmt), va_list args )
{
    if( size == 0 )
        return vsnprintf(str, size, fmt, args);

    va_list wrk_args;

#ifdef va_copy
    va_copy( wrk_args, args );
#else
    wrk_args = args;
#endif

    const char* fmt_ori = fmt;
    size_t offset_out = 0;
    char ch = '\0';
    bool bFormatUnknown = false;

    for( ; (ch = *fmt) != '\0'; ++fmt )
    {
        if( ch == '%' )
        {
            const char* ptrend = CPLvsnprintf_get_end_of_formatting(fmt+1);
            if( ptrend == nullptr || ptrend - fmt >= 20 )
            {
                bFormatUnknown = true;
                break;
            }
            char end = *ptrend;
            char end_m1 = ptrend[-1];

            char localfmt[22] = {};
            memcpy(localfmt, fmt, ptrend - fmt + 1);
            localfmt[ptrend-fmt+1] = '\0';

            int local_ret = 0;
            if( end == '%' )
            {
                if( offset_out == size-1 )
                    break;
                local_ret = 1;
                str[offset_out] = '%';
            }
            else if( end == 'd' || end == 'i' ||  end == 'c' )
            {
                if( end_m1 == 'h' )
                    call_native_snprintf(int);
                else if( end_m1 == 'l' && ptrend[-2] != 'l' )
                    call_native_snprintf(long);
                else if( end_m1 == 'l' && ptrend[-2] == 'l' )
                    call_native_snprintf(GIntBig);
                else if( end_m1 == '4' && ptrend[-2] == '6' &&
                         ptrend[-3] == 'I' )
                    // Microsoft I64 modifier.
                    call_native_snprintf(GIntBig);
                else if( end_m1 == 'z')
                    call_native_snprintf(size_t);
                else if( (end_m1 >= 'a' && end_m1 <= 'z') ||
                         (end_m1 >= 'A' && end_m1 <= 'Z') )
                {
                    bFormatUnknown = true;
                    break;
                }
                else
                    call_native_snprintf(int);
            }
            else if( end == 'o' || end == 'u' || end == 'x' || end == 'X' )
            {
                if( end_m1 == 'h' )
                    call_native_snprintf(unsigned int);
                else if( end_m1 == 'l' && ptrend[-2] != 'l' )
                    call_native_snprintf(unsigned long);
                else if( end_m1 == 'l' && ptrend[-2] == 'l' )
                    call_native_snprintf(GUIntBig);
                else if( end_m1 == '4' && ptrend[-2] == '6' &&
                         ptrend[-3] == 'I' )
                    // Microsoft I64 modifier.
                    call_native_snprintf(GUIntBig);
                else if( end_m1 == 'z')
                    call_native_snprintf(size_t);
                else if( (end_m1 >= 'a' && end_m1 <= 'z') ||
                         (end_m1 >= 'A' && end_m1 <= 'Z') )
                {
                    bFormatUnknown = true;
                    break;
                }
                else
                    call_native_snprintf(unsigned int);
            }
            else if( end == 'e' || end == 'E' ||
                     end == 'f' || end == 'F' ||
                     end == 'g' || end == 'G' ||
                     end == 'a' || end == 'A' )
            {
                if( end_m1 == 'L' )
                    call_native_snprintf(long double);
                else
                    call_native_snprintf(double);
                // MSVC vsnprintf() returns -1.
                if( local_ret < 0 || offset_out + local_ret >= size )
                    break;
                for( int j = 0; j < local_ret; ++j )
                {
                    if( str[offset_out + j] == ',' )
                    {
                        str[offset_out + j] = '.';
                        break;
                    }
                }
            }
            else if( end == 's' )
            {
                const char* pszPtr = va_arg(wrk_args, const char*);
                CPLAssert(pszPtr);
                local_ret = snprintf(str + offset_out, size - offset_out,
                                     localfmt, pszPtr);
            }
            else if( end == 'p' )
            {
                call_native_snprintf(void*);
            }
            else
            {
                bFormatUnknown = true;
                break;
            }
            // MSVC vsnprintf() returns -1.
            if( local_ret < 0 || offset_out + local_ret >= size )
                break;
            offset_out += local_ret;
            fmt = ptrend;
        }
        else
        {
            if( offset_out == size-1 )
                break;
            str[offset_out++] = *fmt;
        }
    }
    if( ch == '\0' && offset_out < size )
        str[offset_out] = '\0';
    else
    {
        if( bFormatUnknown )
        {
            CPLDebug( "CPL", "CPLvsnprintf() called with unsupported "
                      "formatting string: %s", fmt_ori);
        }
#ifdef va_copy
        va_end( wrk_args );
        va_copy( wrk_args, args );
#else
        wrk_args = args;
#endif
#if defined(HAVE_VSNPRINTF)
        offset_out = vsnprintf(str, size, fmt_ori, wrk_args);
#else
        offset_out = vsprintf(str, fmt_ori, wrk_args);
#endif
    }

#ifdef va_copy
    va_end( wrk_args );
#endif

    return static_cast<int>( offset_out );
}

/************************************************************************/
/*                           CPLsnprintf()                              */
/************************************************************************/

#if !defined(ALIAS_CPLSNPRINTF_AS_SNPRINTF)

#if defined(__clang__) && __clang_major__ == 3 && __clang_minor__ <= 2
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif

/** snprintf() wrapper that is not sensitive to LC_NUMERIC settings.
  *
  * This function has the same contract as standard snprintf(), except that
  * formatting of floating-point numbers will use decimal point, whatever the
  * current locale is set.
  *
  * @param str output buffer
  * @param size size of the output buffer (including space for terminating nul)
  * @param fmt formatting string
  * @param ... arguments
  * @return the number of characters (excluding terminating nul) that would be
  * written if size is big enough. Or potentially -1 with Microsoft C runtime
  * for Visual Studio < 2015.
  * @since GDAL 2.0
  */

int CPLsnprintf( char *str, size_t size,
                 CPL_FORMAT_STRING(const char* fmt), ... )
{
    va_list args;

    va_start( args, fmt );
    const int ret = CPLvsnprintf( str, size, fmt, args );
    va_end( args );
    return ret;
}

#endif //  !defined(ALIAS_CPLSNPRINTF_AS_SNPRINTF)

/************************************************************************/
/*                           CPLsprintf()                               */
/************************************************************************/

/** sprintf() wrapper that is not sensitive to LC_NUMERIC settings.
  *
  * This function has the same contract as standard sprintf(), except that
  * formatting of floating-point numbers will use decimal point, whatever the
  * current locale is set.
  *
  * @param str output buffer (must be large enough to hold the result)
  * @param fmt formatting string
  * @param ... arguments
  * @return the number of characters (excluding terminating nul) written in
` * output buffer.
  * @since GDAL 2.0
  */
int CPLsprintf( char *str, CPL_FORMAT_STRING(const char* fmt), ... )
{
    va_list args;

    va_start( args, fmt );
    const int ret = CPLvsnprintf( str, INT_MAX, fmt, args );
    va_end( args );
    return ret;
}

/************************************************************************/
/*                           CPLprintf()                                */
/************************************************************************/

/** printf() wrapper that is not sensitive to LC_NUMERIC settings.
  *
  * This function has the same contract as standard printf(), except that
  * formatting of floating-point numbers will use decimal point, whatever the
  * current locale is set.
  *
  * @param fmt formatting string
  * @param ... arguments
  * @return the number of characters (excluding terminating nul) written in
  * output buffer.
  * @since GDAL 2.0
  */
int CPLprintf( CPL_FORMAT_STRING(const char* fmt), ... )
{
    va_list wrk_args, args;

    va_start( args, fmt );

#ifdef va_copy
    va_copy( wrk_args, args );
#else
    wrk_args = args;
#endif

    char szBuffer[4096] = {};
    // Quiet coverity by staring off nul terminated.
    int ret = CPLvsnprintf( szBuffer, sizeof(szBuffer), fmt, wrk_args );

#ifdef va_copy
    va_end( wrk_args );
#endif

    if( ret < int(sizeof(szBuffer))-1 )
        ret = printf("%s", szBuffer); /*ok*/
    else
    {
#ifdef va_copy
        va_copy( wrk_args, args );
#else
        wrk_args = args;
#endif

        ret = vfprintf(stdout, fmt, wrk_args);

#ifdef va_copy
        va_end( wrk_args );
#endif
    }

    va_end( args );

    return ret;
}

/************************************************************************/
/*                           CPLsscanf()                                */
/************************************************************************/

/** \brief sscanf() wrapper that is not sensitive to LC_NUMERIC settings.
  *
  * This function has the same contract as standard sscanf(), except that
  * formatting of floating-point numbers will use decimal point, whatever the
  * current locale is set.
  *
  * CAUTION: only works with a very limited number of formatting strings,
  * consisting only of "%lf" and regular characters.
  *
  * @param str input string
  * @param fmt formatting string
  * @param ... arguments
  * @return the number of matched patterns;
  * @since GDAL 2.0
  */
#ifdef DOXYGEN_XML
int CPLsscanf( const char* str, const char* fmt, ... )
#else
int CPLsscanf( const char* str, CPL_SCANF_FORMAT_STRING(const char* fmt), ... )
#endif
{
    bool error = false;
    int ret = 0;
    const char* fmt_ori = fmt;
    va_list args;

    va_start( args, fmt );
    for( ; *fmt != '\0' && *str != '\0'; ++fmt )
    {
        if( *fmt == '%' )
        {
            if( fmt[1] == 'l' && fmt[2] == 'f' )
            {
                fmt += 2;
                char* end;
                *(va_arg(args, double*)) = CPLStrtod(str, &end);
                if( end > str )
                {
                    ++ret;
                    str = end;
                }
                else
                    break;
            }
            else
            {
                error = true;
                break;
            }
        }
        else if( isspace(*fmt) )
        {
            while( *str != '\0' && isspace(*str) )
                ++str;
        }
        else if( *str != *fmt )
            break;
        else
            ++str;
    }
    va_end( args );

    if( error )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Format %s not supported by CPLsscanf()",
                 fmt_ori);
    }

    return ret;
}

#if defined(__clang__) && __clang_major__ == 3 && __clang_minor__ <= 2
#pragma clang diagnostic pop
#endif

/************************************************************************/
/*                         CPLTestBool()                                */
/************************************************************************/

/**
 * Test what boolean value contained in the string.
 *
 * If pszValue is "NO", "FALSE", "OFF" or "0" will be returned false.
 * Otherwise, true will be returned.
 *
 * @param pszValue the string should be tested.
 *
 * @return true or false.
 */

bool CPLTestBool( const char *pszValue )
{
    return !(EQUAL(pszValue, "NO")
             || EQUAL(pszValue, "FALSE")
             || EQUAL(pszValue, "OFF")
             || EQUAL(pszValue, "0"));
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
 * Deprecated.  Removed in GDAL 3.x.
 *
 * Use CPLTestBoolean() for C and CPLTestBool() for C++.
 *
 * @param pszValue the string should be tested.
 *
 * @return TRUE or FALSE.
 */

int CSLTestBoolean( const char *pszValue )
{
    return CPLTestBool( pszValue ) ? TRUE : FALSE;
}

/************************************************************************/
/*                         CPLTestBoolean()                             */
/************************************************************************/

/**
 * Test what boolean value contained in the string.
 *
 * If pszValue is "NO", "FALSE", "OFF" or "0" will be returned FALSE.
 * Otherwise, TRUE will be returned.
 *
 * Use this only in C code.  In C++, prefer CPLTestBool().
 *
 * @param pszValue the string should be tested.
 *
 * @return TRUE or FALSE.
 */

int CPLTestBoolean( const char *pszValue )
{
    return CPLTestBool( pszValue ) ? TRUE : FALSE;
}

/**********************************************************************
 *                       CPLFetchBool()
 **********************************************************************/

/** Check for boolean key value.
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
 * @return true or false
 */

bool CPLFetchBool( CSLConstList papszStrList, const char *pszKey,
                   bool bDefault )

{
    if( CSLFindString( papszStrList, pszKey ) != -1 )
        return true;

    const char * const pszValue =
        CSLFetchNameValue( papszStrList, pszKey );
    if( pszValue == nullptr )
        return bDefault;

    return CPLTestBool( pszValue );
}

/**********************************************************************
 *                       CSLFetchBoolean()
 **********************************************************************/

/** DEPRECATED.  Check for boolean key value.
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
 */

int CSLFetchBoolean( CSLConstList papszStrList, const char *pszKey, int bDefault )

{
    return CPLFetchBool( papszStrList, pszKey, CPL_TO_BOOL(bDefault) );
}

/************************************************************************/
/*                     CSLFetchNameValueDefaulted()                     */
/************************************************************************/

/** Same as CSLFetchNameValue() but return pszDefault in case of no match */
const char *CSLFetchNameValueDef( CSLConstList papszStrList,
                                  const char *pszName,
                                  const char *pszDefault )

{
    const char *pszResult = CSLFetchNameValue( papszStrList, pszName );
    if( pszResult != nullptr )
        return pszResult;

    return pszDefault;
}

/**********************************************************************
 *                       CSLFetchNameValue()
 **********************************************************************/

/** In a StringList of "Name=Value" pairs, look for the
 * first value associated with the specified name.  The search is not
 * case sensitive.
 * ("Name:Value" pairs are also supported for backward compatibility
 * with older stuff.)
 *
 * Returns a reference to the value in the StringList that the caller
 * should not attempt to free.
 *
 * Returns NULL if the name is not found.
 */

const char *CSLFetchNameValue( CSLConstList papszStrList,
                               const char *pszName )
{
    if( papszStrList == nullptr || pszName == nullptr )
        return nullptr;

    const size_t nLen = strlen(pszName);
    while( *papszStrList != nullptr )
    {
        if( EQUALN(*papszStrList, pszName, nLen)
            && ( (*papszStrList)[nLen] == '=' ||
                 (*papszStrList)[nLen] == ':' ) )
        {
            return (*papszStrList) + nLen + 1;
        }
        ++papszStrList;
    }
    return nullptr;
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
 * @return -1 on failure or the list index of the first occurrence
 * matching the given key.
 */

int CSLFindName( CSLConstList papszStrList, const char *pszName )
{
    if( papszStrList == nullptr || pszName == nullptr )
        return -1;

    const size_t nLen = strlen(pszName);
    int iIndex = 0;
    while( *papszStrList != nullptr )
    {
        if( EQUALN(*papszStrList, pszName, nLen)
            && ( (*papszStrList)[nLen] == '=' ||
                 (*papszStrList)[nLen] == ':' ) )
        {
            return iIndex;
        }
        ++iIndex;
        ++papszStrList;
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
 * allocated using CPLMalloc(), and returned in that pointer.  It is the
 * applications responsibility to free this string, but the application should
 * not modify or free the returned value portion.
 *
 * This function also support "NAME:VALUE" strings and will strip white
 * space from around the delimiter when forming name and value strings.
 *
 * Eventually CSLFetchNameValue() and friends may be modified to use
 * CPLParseNameValue().
 *
 * @param pszNameValue string in "NAME=VALUE" format.
 * @param ppszKey optional pointer though which to return the name
 * portion.
 *
 * @return the value portion (pointing into original string).
 */

const char *CPLParseNameValue( const char *pszNameValue, char **ppszKey )
{
    for( int i = 0; pszNameValue[i] != '\0'; ++i )
    {
        if( pszNameValue[i] == '=' || pszNameValue[i] == ':' )
        {
            const char *pszValue = pszNameValue + i + 1;
            while( *pszValue == ' ' || *pszValue == '\t' )
                ++pszValue;

            if( ppszKey != nullptr )
            {
                *ppszKey = static_cast<char *>(CPLMalloc(i + 1));
                memcpy( *ppszKey, pszNameValue, i );
                (*ppszKey)[i] = '\0';
                while( i > 0 &&
                       ( (*ppszKey)[i-1] == ' ' || (*ppszKey)[i-1] == '\t') )
                {
                    (*ppszKey)[i-1] = '\0';
                    i--;
                }
            }

            return pszValue;
        }
    }

    return nullptr;
}

/**********************************************************************
 *                       CSLFetchNameValueMultiple()
 **********************************************************************/

/** In a StringList of "Name=Value" pairs, look for all the
 * values with the specified name.  The search is not case
 * sensitive.
 * ("Name:Value" pairs are also supported for backward compatibility
 * with older stuff.)
 *
 * Returns StringList with one entry for each occurrence of the
 * specified name.  The StringList should eventually be destroyed
 * by calling CSLDestroy().
 *
 * Returns NULL if the name is not found.
 */

char **CSLFetchNameValueMultiple( CSLConstList papszStrList, const char *pszName )
{
    if( papszStrList == nullptr || pszName == nullptr )
        return nullptr;

    const size_t nLen = strlen(pszName);
    char **papszValues = nullptr;
    while( *papszStrList != nullptr )
    {
        if( EQUALN(*papszStrList, pszName, nLen)
            && ( (*papszStrList)[nLen] == '=' ||
                 (*papszStrList)[nLen] == ':' ) )
        {
            papszValues = CSLAddString(papszValues,
                                       (*papszStrList)+nLen+1);
        }
        ++papszStrList;
    }

    return papszValues;
}

/**********************************************************************
 *                       CSLAddNameValue()
 **********************************************************************/

/** Add a new entry to a StringList of "Name=Value" pairs,
 * ("Name:Value" pairs are also supported for backward compatibility
 * with older stuff.)
 *
 * This function does not check if a "Name=Value" pair already exists
 * for that name and can generate multiple entries for the same name.
 * Use CSLSetNameValue() if you want each name to have only one value.
 *
 * Returns the modified StringList.
 */

char **CSLAddNameValue( char **papszStrList,
                        const char *pszName, const char *pszValue )
{
    if( pszName == nullptr || pszValue==nullptr )
        return papszStrList;

    const size_t nLen = strlen(pszName)+strlen(pszValue)+2;
    char *pszLine = static_cast<char *>(CPLMalloc(nLen) );
    snprintf( pszLine, nLen, "%s=%s", pszName, pszValue );
    papszStrList = CSLAddString( papszStrList, pszLine );
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
 * @return modified StringList.
 */

char **CSLSetNameValue( char **papszList,
                        const char *pszName, const char *pszValue )
{
    if( pszName == nullptr )
        return papszList;

    size_t nLen = strlen(pszName);
    while( nLen > 0 && pszName[nLen-1] == ' ' )
        nLen --;
    char **papszPtr = papszList;
    while( papszPtr && *papszPtr != nullptr )
    {
        if( EQUALN(*papszPtr, pszName, nLen) )
        {
            size_t i;
            for( i = nLen; (*papszPtr)[i] == ' '; ++i )
            {
            }
            if( (*papszPtr)[i] == '=' || (*papszPtr)[i] == ':' )
            {
                // Found it.
                // Change the value... make sure to keep the ':' or '='.
                const char cSep = (*papszPtr)[i];

                CPLFree(*papszPtr);

                // If the value is NULL, remove this entry completely.
                if( pszValue == nullptr )
                {
                    while( papszPtr[1] != nullptr )
                    {
                        *papszPtr = papszPtr[1];
                        ++papszPtr;
                    }
                    *papszPtr = nullptr;
                }

                // Otherwise replace with new value.
                else
                {
                    const size_t nLen2 = strlen(pszName)+strlen(pszValue)+2;
                    *papszPtr = static_cast<char *>(CPLMalloc(nLen2) );
                    snprintf( *papszPtr, nLen2, "%s%c%s", pszName, cSep, pszValue );
                }
                return papszList;
            }
        }
        ++papszPtr;
    }

    if( pszValue == nullptr )
        return papszList;

    // The name does not exist yet.  Create a new entry.
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
 * list will not be manipulable by the CSL name/value functions any more.
 *
 * The CPLParseNameValue() function is used to break the existing lines,
 * and it also strips white space from around the existing delimiter, thus
 * the old separator, and any white space will be replaced by the new
 * separator.  For formatting purposes it may be desirable to include some
 * white space in the new separator.  e.g. ": " or " = ".
 *
 * @param papszList the list to update.  Component strings may be freed
 * but the list array will remain at the same location.
 *
 * @param pszSeparator the new separator string to insert.
 */

void CSLSetNameValueSeparator( char ** papszList, const char *pszSeparator )

{
    const int nLines = CSLCount(papszList);

    for( int iLine = 0; iLine < nLines; ++iLine )
    {
        char *pszKey = nullptr;
        const char *pszValue = CPLParseNameValue( papszList[iLine], &pszKey );
        if( pszValue == nullptr || pszKey == nullptr )
        {
            CPLFree( pszKey );
            continue;
        }

        char *pszNewLine = static_cast<char *>(
            CPLMalloc( strlen(pszValue) + strlen(pszKey)
                       + strlen(pszSeparator) + 1 ) );
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
 * reconstituted to its original form.  The escaping will even preserve
 * zero bytes allowing preservation of raw binary data.
 *
 * CPLES_BackslashQuotable(0): This scheme turns a binary string into
 * a form suitable to be placed within double quotes as a string constant.
 * The backslash, quote, '\\0' and newline characters are all escaped in
 * the usual C style.
 *
 * CPLES_XML(1): This scheme converts the '<', '>', '"' and '&' characters into
 * their XML/HTML equivalent (&lt;, &gt;, &quot; and &amp;) making a string safe
 * to embed as CDATA within an XML element.  The '\\0' is not escaped and
 * should not be included in the input.
 *
 * CPLES_URL(2): Everything except alphanumerics and the characters
 * '$', '-', '_', '.', '+', '!', '*', ''', '(', ')' and ',' (see RFC1738) are
 * converted to a percent followed by a two digit hex encoding of the character
 * (leading zero supplied if needed).  This is the mechanism used for encoding
 * values to be passed in URLs.
 *
 * CPLES_SQL(3): All single quotes are replaced with two single quotes.
 * Suitable for use when constructing literal values for SQL commands where
 * the literal will be enclosed in single quotes.
 *
 * CPLES_CSV(4): If the values contains commas, semicolons, tabs, double quotes,
 * or newlines it placed in double quotes, and double quotes in the value are
 * doubled. Suitable for use when constructing field values for .csv files.
 * Note that CPLUnescapeString() currently does not support this format, only
 * CPLEscapeString().  See cpl_csv.cpp for CSV parsing support.
 *
 * CPLES_SQLI(7): All double quotes are replaced with two double quotes.
 * Suitable for use when constructing identifiers for SQL commands where
 * the literal will be enclosed in double quotes.
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
    const size_t szLength =
        (nLength < 0) ? strlen(pszInput) : static_cast<size_t>(nLength);
#define nLength no_longer_use_me

    size_t nSizeAlloc = 1;
#if SIZEOF_VOIDP < 8
    bool bWrapAround = false;
    const auto IncSizeAlloc = [&nSizeAlloc, &bWrapAround](size_t inc)
    {
        constexpr size_t SZ_MAX = std::numeric_limits<size_t>::max();
        if( nSizeAlloc > SZ_MAX - inc )
        {
            bWrapAround = true;
            nSizeAlloc = 0;
        }
        nSizeAlloc += inc;
    };
#else
    const auto IncSizeAlloc = [&nSizeAlloc](size_t inc)
    {
        nSizeAlloc += inc;
    };
#endif

    if( nScheme == CPLES_BackslashQuotable )
    {
        for( size_t iIn = 0; iIn < szLength; iIn++ )
        {
            if( pszInput[iIn] == '\0' ||
                pszInput[iIn] == '\n' ||
                pszInput[iIn] == '"'  ||
                pszInput[iIn] == '\\' )
                IncSizeAlloc(2);
            else
                IncSizeAlloc(1);
        }
    }
    else if( nScheme == CPLES_XML || nScheme == CPLES_XML_BUT_QUOTES )
    {
        for( size_t iIn = 0; iIn < szLength; ++iIn )
        {
            if( pszInput[iIn] == '<' )
            {
                IncSizeAlloc(4);
            }
            else if( pszInput[iIn] == '>' )
            {
                IncSizeAlloc(4);
            }
            else if( pszInput[iIn] == '&' )
            {
                IncSizeAlloc(5);
            }
            else if( pszInput[iIn] == '"' && nScheme != CPLES_XML_BUT_QUOTES )
            {
                IncSizeAlloc(6);
            }
            // Python 2 does not display the UTF-8 character corresponding
            // to the byte-order mark (BOM), so escape it.
            else if( (reinterpret_cast<const unsigned char*>(pszInput))[iIn]
                         == 0xEF &&
                     (reinterpret_cast<const unsigned char*>(pszInput))[iIn+1]
                         == 0xBB &&
                     (reinterpret_cast<const unsigned char*>(pszInput))[iIn+2]
                         == 0xBF )
            {
                IncSizeAlloc(8);
                iIn += 2;
            }
            else if( (reinterpret_cast<const unsigned char*>(pszInput))[iIn]
                         < 0x20
                     && pszInput[iIn] != 0x9
                     && pszInput[iIn] != 0xA
                     && pszInput[iIn] != 0xD )
            {
                // These control characters are unrepresentable in XML format,
                // so we just drop them.  #4117
            }
            else
            {
                IncSizeAlloc(1);
            }
        }
    }
    else if( nScheme == CPLES_URL ) // Untested at implementation.
    {
        for( size_t iIn = 0; iIn < szLength; ++iIn )
        {
            if( (pszInput[iIn] >= 'a' && pszInput[iIn] <= 'z')
                || (pszInput[iIn] >= 'A' && pszInput[iIn] <= 'Z')
                || (pszInput[iIn] >= '0' && pszInput[iIn] <= '9')
                || pszInput[iIn] == '$' || pszInput[iIn] == '-'
                || pszInput[iIn] == '_' || pszInput[iIn] == '.'
                || pszInput[iIn] == '+' || pszInput[iIn] == '!'
                || pszInput[iIn] == '*' || pszInput[iIn] == '\''
                || pszInput[iIn] == '(' || pszInput[iIn] == ')'
                || pszInput[iIn] == ',' )
            {
                IncSizeAlloc(1);
            }
            else
            {
                IncSizeAlloc(3);
            }
        }
    }
    else if( nScheme == CPLES_SQL || nScheme == CPLES_SQLI )
    {
        const char chQuote = nScheme == CPLES_SQL ? '\'' : '\"';
        for( size_t iIn = 0; iIn < szLength; ++iIn )
        {
            if( pszInput[iIn] == chQuote )
            {
                IncSizeAlloc(2);
            }
            else
            {
                IncSizeAlloc(1);
            }
        }
    }
    else if( nScheme == CPLES_CSV || nScheme == CPLES_CSV_FORCE_QUOTING )
    {
        if( nScheme == CPLES_CSV &&
            strcspn( pszInput, "\",;\t\n\r" ) == szLength )
        {
            char *pszOutput = static_cast<char *>( VSI_MALLOC_VERBOSE( szLength + 1 ) );
            if( pszOutput == nullptr )
                return nullptr;
            memcpy(pszOutput, pszInput, szLength + 1);
            return pszOutput;
        }
        else
        {
            IncSizeAlloc(1);
            for( size_t iIn = 0; iIn < szLength; ++iIn )
            {
                if( pszInput[iIn] == '\"' )
                {
                    IncSizeAlloc(2);
                }
                else
                    IncSizeAlloc(1);
            }
            IncSizeAlloc(1);
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Undefined escaping scheme (%d) in CPLEscapeString()",
                  nScheme );
        return CPLStrdup("");
    }

#if SIZEOF_VOIDP < 8
    if( bWrapAround )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory in CPLEscapeString()");
        return nullptr;
    }
#endif

    char *pszOutput = static_cast<char *>( VSI_MALLOC_VERBOSE( nSizeAlloc ) );
    if( pszOutput == nullptr )
        return nullptr;

    size_t iOut = 0;

    if( nScheme == CPLES_BackslashQuotable )
    {
        for( size_t iIn = 0; iIn < szLength; iIn++ )
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
        pszOutput[iOut++] = '\0';
    }
    else if( nScheme == CPLES_XML || nScheme == CPLES_XML_BUT_QUOTES )
    {
        for( size_t iIn = 0; iIn < szLength; ++iIn )
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
            // Python 2 does not display the UTF-8 character corresponding
            // to the byte-order mark (BOM), so escape it.
            else if( (reinterpret_cast<const unsigned char*>(pszInput))[iIn]
                         == 0xEF &&
                     (reinterpret_cast<const unsigned char*>(pszInput))[iIn+1]
                         == 0xBB &&
                     (reinterpret_cast<const unsigned char*>(pszInput))[iIn+2]
                         == 0xBF )
            {
                pszOutput[iOut++] = '&';
                pszOutput[iOut++] = '#';
                pszOutput[iOut++] = 'x';
                pszOutput[iOut++] = 'F';
                pszOutput[iOut++] = 'E';
                pszOutput[iOut++] = 'F';
                pszOutput[iOut++] = 'F';
                pszOutput[iOut++] = ';';
                iIn += 2;
            }
            else if( (reinterpret_cast<const unsigned char*>(pszInput))[iIn]
                         < 0x20
                     && pszInput[iIn] != 0x9
                     && pszInput[iIn] != 0xA
                     && pszInput[iIn] != 0xD )
            {
                // These control characters are unrepresentable in XML format,
                // so we just drop them.  #4117
            }
            else
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
        }
        pszOutput[iOut++] = '\0';
    }
    else if( nScheme == CPLES_URL ) // Untested at implementation.
    {
        for( size_t iIn = 0; iIn < szLength; ++iIn )
        {
            if( (pszInput[iIn] >= 'a' && pszInput[iIn] <= 'z')
                || (pszInput[iIn] >= 'A' && pszInput[iIn] <= 'Z')
                || (pszInput[iIn] >= '0' && pszInput[iIn] <= '9')
                || pszInput[iIn] == '$' || pszInput[iIn] == '-'
                || pszInput[iIn] == '_' || pszInput[iIn] == '.'
                || pszInput[iIn] == '+' || pszInput[iIn] == '!'
                || pszInput[iIn] == '*' || pszInput[iIn] == '\''
                || pszInput[iIn] == '(' || pszInput[iIn] == ')'
                || pszInput[iIn] == ',' )
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
            else
            {
                snprintf( pszOutput+iOut, nSizeAlloc - iOut, "%%%02X",
                            static_cast<unsigned char>( pszInput[iIn] ) );
                iOut += 3;
            }
        }
        pszOutput[iOut++] = '\0';
    }
    else if( nScheme == CPLES_SQL || nScheme == CPLES_SQLI )
    {
        const char chQuote = nScheme == CPLES_SQL ? '\'' : '\"';
        for( size_t iIn = 0; iIn < szLength; ++iIn )
        {
            if( pszInput[iIn] == chQuote )
            {
                pszOutput[iOut++] = chQuote;
                pszOutput[iOut++] = chQuote;
            }
            else
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
        }
        pszOutput[iOut++] = '\0';
    }
    else if( nScheme == CPLES_CSV || nScheme == CPLES_CSV_FORCE_QUOTING )
    {
        pszOutput[iOut++] = '\"';

        for( size_t iIn = 0; iIn < szLength; ++iIn )
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

    return pszOutput;
#undef nLength
}

/************************************************************************/
/*                         CPLUnescapeString()                          */
/************************************************************************/

/**
 * Unescape a string.
 *
 * This function does the opposite of CPLEscapeString().  Given a string
 * with special values escaped according to some scheme, it will return a
 * new copy of the string returned to its original form.
 *
 * @param pszInput the input string.  This is a zero terminated string.
 * @param pnLength location to return the length of the unescaped string,
 * which may in some cases include embedded '\\0' characters.
 * @param nScheme the escaped scheme to undo (see CPLEscapeString() for a
 * list).  Does not yet support CSV.
 *
 * @return a copy of the unescaped string that should be freed by the
 * application using CPLFree() when no longer needed.
 */

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
char *CPLUnescapeString( const char *pszInput, int *pnLength, int nScheme )

{
    int iOut = 0;

    // TODO: Why times 4?
    char *pszOutput = static_cast<char *>(CPLMalloc(4 * strlen(pszInput) + 1));
    pszOutput[0] = '\0';

    if( nScheme == CPLES_BackslashQuotable )
    {
        for( int iIn = 0; pszInput[iIn] != '\0'; ++iIn )
        {
            if( pszInput[iIn] == '\\' )
            {
                ++iIn;
                if( pszInput[iIn] == '\0' )
                    break;
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
    else if( nScheme == CPLES_XML || nScheme == CPLES_XML_BUT_QUOTES )
    {
        char ch = '\0';
        for( int iIn = 0; (ch = pszInput[iIn]) != '\0'; ++iIn )
        {
            if( ch != '&' )
            {
                pszOutput[iOut++] = ch;
            }
            else if( STARTS_WITH_CI(pszInput+iIn, "&lt;") )
            {
                pszOutput[iOut++] = '<';
                iIn += 3;
            }
            else if( STARTS_WITH_CI(pszInput+iIn, "&gt;") )
            {
                pszOutput[iOut++] = '>';
                iIn += 3;
            }
            else if( STARTS_WITH_CI(pszInput+iIn, "&amp;") )
            {
                pszOutput[iOut++] = '&';
                iIn += 4;
            }
            else if( STARTS_WITH_CI(pszInput+iIn, "&apos;") )
            {
                pszOutput[iOut++] = '\'';
                iIn += 5;
            }
            else if( STARTS_WITH_CI(pszInput+iIn, "&quot;") )
            {
                pszOutput[iOut++] = '"';
                iIn += 5;
            }
            else if( STARTS_WITH_CI(pszInput+iIn, "&#x") )
            {
                wchar_t anVal[2] = {0 , 0};
                iIn += 3;

                unsigned int nVal = 0;
                while( true )
                {
                    ch = pszInput[iIn++];
                    if( ch >= 'a' && ch <= 'f' )
                        nVal = nVal * 16U +
                                static_cast<unsigned int>(ch - 'a' + 10);
                    else if( ch >= 'A' && ch <= 'F' )
                        nVal = nVal * 16U +
                                static_cast<unsigned int>(ch - 'A' + 10);
                    else if( ch >= '0' && ch <= '9' )
                        nVal = nVal * 16U +
                                static_cast<unsigned int>(ch - '0');
                    else
                        break;
                }
                anVal[0] = static_cast<wchar_t>(nVal);
                if( ch != ';' )
                    break;
                iIn--;

                char * pszUTF8 =
                    CPLRecodeFromWChar( anVal, "WCHAR_T", CPL_ENC_UTF8);
                int nLen = static_cast<int>(strlen(pszUTF8));
                memcpy(pszOutput + iOut, pszUTF8, nLen);
                CPLFree(pszUTF8);
                iOut += nLen;
            }
            else if( STARTS_WITH_CI(pszInput+iIn, "&#") )
            {
                wchar_t anVal[2] = { 0, 0 };
                iIn += 2;

                unsigned int nVal = 0;
                while( true )
                {
                    ch = pszInput[iIn++];
                    if( ch >= '0' && ch <= '9' )
                        nVal = nVal * 10U + static_cast<unsigned int>(ch - '0');
                    else
                        break;
                }
                anVal[0] = static_cast<wchar_t>(nVal);
                if( ch != ';' )
                    break;
                iIn--;

                char *pszUTF8 =
                    CPLRecodeFromWChar( anVal, "WCHAR_T", CPL_ENC_UTF8);
                const int nLen = static_cast<int>(strlen(pszUTF8));
                memcpy(pszOutput + iOut, pszUTF8, nLen);
                CPLFree(pszUTF8);
                iOut += nLen;
            }
            else
            {
                // Illegal escape sequence.
                CPLDebug( "CPL",
                          "Error unescaping CPLES_XML text, '&' character "
                          "followed by unhandled escape sequence." );
                break;
            }
        }
    }
    else if( nScheme == CPLES_URL )
    {
        for( int iIn = 0; pszInput[iIn] != '\0'; ++iIn )
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

                pszOutput[iOut++] = static_cast<char>( nHexChar );
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
    else if( nScheme == CPLES_SQL || nScheme == CPLES_SQLI )
    {
        char szQuote = nScheme == CPLES_SQL ? '\'' : '\"';
        for( int iIn = 0; pszInput[iIn] != '\0'; ++iIn )
        {
            if( pszInput[iIn] == szQuote && pszInput[iIn+1] == szQuote )
            {
                ++iIn;
                pszOutput[iOut++] = pszInput[iIn];
            }
            else
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
        }
    }
    else if( nScheme == CPLES_CSV )
    {
        CPLError( CE_Fatal, CPLE_NotSupported,
                  "CSV Unescaping not yet implemented.");
    }
    else
    {
        CPLError( CE_Fatal, CPLE_NotSupported,
                  "Unknown escaping style.");
    }

    pszOutput[iOut] = '\0';

    if( pnLength != nullptr )
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
    char *pszHex = static_cast<char *>(CPLMalloc(nBytes * 2 + 1));
    pszHex[nBytes*2] = '\0';

    constexpr char achHex[] = "0123456789ABCDEF";

    for( int i = 0; i < nBytes; ++i )
    {
        const int nLow = pabyData[i] & 0x0f;
        const int nHigh = (pabyData[i] & 0xf0) >> 4;

        pszHex[i*2] = achHex[nHigh];
        pszHex[i*2+1] = achHex[nLow];
    }

    return pszHex;
}

/************************************************************************/
/*                           CPLHexToBinary()                           */
/************************************************************************/

constexpr unsigned char hex2char[256] = {
    // Not Hex characters.
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // 0-9
    0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
    // A-F
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
    // Not Hex characters.
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // a-f
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // Not Hex characters (upper 128 characters).
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/**
 * Hexadecimal to binary translation
 *
 * @param pszHex the input hex encoded string.
 * @param pnBytes the returned count of decoded bytes placed here.
 *
 * @return returns binary buffer of data - free with CPLFree().
 */

GByte *CPLHexToBinary( const char *pszHex, int *pnBytes )
{
    const GByte* pabyHex = reinterpret_cast<const GByte*>(pszHex);
    const size_t nHexLen = strlen(pszHex);

    GByte *pabyWKB = static_cast<GByte *>( CPLMalloc(nHexLen / 2 + 2) );

    for( size_t i = 0; i < nHexLen/2; ++i )
    {
        const unsigned char h1 = hex2char[pabyHex[2*i]];
        const unsigned char h2 = hex2char[pabyHex[2*i+1]];

        // First character is high bits, second is low bits.
        pabyWKB[i] = static_cast<GByte>( (h1 << 4) | h2 );
    }
    pabyWKB[nHexLen/2] = 0;
    *pnBytes = static_cast<int>(nHexLen/2);

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

CPLValueType CPLGetValueType( const char* pszValue )
{
    // Doubles : "+25.e+3", "-25.e-3", "25.e3", "25e3", " 25e3 "
    // Not doubles: "25e 3", "25e.3", "-2-5e3", "2-5e3", "25.25.3", "-3d", "d1"
    //              "XXeYYYYYYYYYYYYYYYYYYY" that evaluates to infinity

    if( pszValue == nullptr )
        return CPL_VALUE_STRING;

    const char* pszValueInit = pszValue;

    // Skip leading spaces.
    while( isspace(static_cast<unsigned char>( *pszValue )) )
        ++pszValue;

    if( *pszValue == '\0' )
        return CPL_VALUE_STRING;

    // Skip leading + or -.
    if( *pszValue == '+' || *pszValue == '-' )
        ++pszValue;

    bool bFoundDot = false;
    bool bFoundExponent = false;
    bool bIsLastCharExponent = false;
    bool bIsReal = false;
    const char* pszAfterExponent = nullptr;
    bool bFoundMantissa = false;

    for( ; *pszValue != '\0'; ++pszValue )
    {
        if( isdigit(static_cast<unsigned char>( *pszValue )) )
        {
            bIsLastCharExponent = false;
            bFoundMantissa = true;
        }
        else if( isspace(static_cast<unsigned char>( *pszValue )) )
        {
            const char* pszTmp = pszValue;
            while( isspace(static_cast<unsigned char>( *pszTmp )) )
                ++pszTmp;
            if( *pszTmp == 0 )
                break;
            else
                return CPL_VALUE_STRING;
        }
        else if( *pszValue == '-' || *pszValue == '+' )
        {
            if( bIsLastCharExponent )
            {
                // Do nothing.
            }
            else
            {
                return CPL_VALUE_STRING;
            }
            bIsLastCharExponent = false;
        }
        else if( *pszValue == '.')
        {
            bIsReal = true;
            if( !bFoundDot && !bIsLastCharExponent )
                bFoundDot = true;
            else
                return CPL_VALUE_STRING;
            bIsLastCharExponent = false;
        }
        else if( *pszValue == 'D' || *pszValue == 'd'
                 || *pszValue == 'E' || *pszValue == 'e' )
        {
            if( !bFoundMantissa )
                return CPL_VALUE_STRING;
            if( !(pszValue[1] == '+' || pszValue[1] == '-' ||
                  isdigit(pszValue[1])) )
                return CPL_VALUE_STRING;

            bIsReal = true;
            if( !bFoundExponent )
                bFoundExponent = true;
            else
                return CPL_VALUE_STRING;
            pszAfterExponent = pszValue + 1;
            bIsLastCharExponent = true;
        }
        else
        {
            return CPL_VALUE_STRING;
        }
    }

    if( bIsReal && pszAfterExponent && strlen(pszAfterExponent) > 3 )
    {
        // cppcheck-suppress unreadVariable
        const double dfVal = CPLAtof(pszValueInit);
        if( CPLIsInf(dfVal) )
            return CPL_VALUE_STRING;
    }

    return bIsReal ? CPL_VALUE_REAL : CPL_VALUE_INTEGER;
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
char szDest[5] = {};
if( CPLStrlcpy(szDest, "abcde", sizeof(szDest)) >= sizeof(szDest) )
    fprintf(stderr, "truncation occurred !\n");
\endverbatim

 * @param pszDest   destination buffer
 * @param pszSrc    source string. Must be NUL terminated
 * @param nDestSize size of destination buffer (including space for the NUL
 *     terminator character)
 *
 * @return the length of the source string (=strlen(pszSrc))
 *
 * @since GDAL 1.7.0
 */
size_t CPLStrlcpy( char* pszDest, const char* pszSrc, size_t nDestSize )
{
    if( nDestSize == 0 )
        return strlen(pszSrc);

    char* pszDestIter = pszDest;
    const char* pszSrcIter = pszSrc;

    --nDestSize;
    while( nDestSize != 0 && *pszSrcIter != '\0' )
    {
        *pszDestIter = *pszSrcIter;
        ++pszDestIter;
        ++pszSrcIter;
        --nDestSize;
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
char szDest[5] = {};
CPLStrlcpy(szDest, "ab", sizeof(szDest));
if( CPLStrlcat(szDest, "cde", sizeof(szDest)) >= sizeof(szDest) )
    fprintf(stderr, "truncation occurred !\n");
\endverbatim

 * @param pszDest   destination buffer. Must be NUL terminated before
 *         running CPLStrlcat
 * @param pszSrc    source string. Must be NUL terminated
 * @param nDestSize size of destination buffer (including space for the
 *         NUL terminator character)
 *
 * @return the theoretical length of the destination string after concatenation
 *         (=strlen(pszDest_before) + strlen(pszSrc)).
 *         If strlen(pszDest_before) >= nDestSize, then it returns
 *         nDestSize + strlen(pszSrc)
 *
 * @since GDAL 1.7.0
 */
size_t CPLStrlcat( char* pszDest, const char* pszSrc, size_t nDestSize )
{
    char* pszDestIter = pszDest;

    while( nDestSize != 0 && *pszDestIter != '\0' )
    {
        ++pszDestIter;
        --nDestSize;
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
 * The CPLStrnlen() function returns min(strlen(pszStr), nMaxLen).
 * Only the first nMaxLen bytes of the string will be read. Useful to
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

size_t CPLStrnlen ( const char *pszStr, size_t nMaxLen )
{
    size_t nLen = 0;
    while( nLen < nMaxLen && *pszStr != '\0' )
    {
        ++nLen;
        ++pszStr;
    }
    return nLen;
}

/************************************************************************/
/*                            CSLParseCommandLine()                     */
/************************************************************************/

/**
 * Tokenize command line arguments in a list of strings.
 *
 * @param pszCommandLine  command line
 *
 * @return NULL terminated list of strings to free with CSLDestroy()
 *
 * @since GDAL 2.1
 */
char ** CSLParseCommandLine(const char* pszCommandLine)
{
    return CSLTokenizeString(pszCommandLine);
}
