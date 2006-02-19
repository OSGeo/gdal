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
 * $Log$
 * Revision 1.54  2006/02/19 21:54:34  mloskot
 * [WINCE] Changes related to Windows CE port of CPL. Most changes are #ifdef wrappers.
 *
 * Revision 1.53  2005/12/19 20:17:21  fwarmerdam
 * enable pszValue NULL to delete entry in CSLSetNameValue
 *
 * Revision 1.52  2005/12/06 07:33:12  fwarmerdam
 * fixed support for tokenizing string with trailing delimeter, bug 945
 *
 * Revision 1.51  2005/10/13 01:20:16  fwarmerdam
 * added CSLMerge()
 *
 * Revision 1.50  2005/09/30 19:11:40  fwarmerdam
 * fixed hextobinary conversion, bug 935
 *
 * Revision 1.49  2005/09/14 19:21:17  fwarmerdam
 * binary pointer is const in binarytohex
 *
 * Revision 1.48  2005/09/13 15:07:23  dron
 * Initialize counters in CPLHexToBinary().
 *
 * Revision 1.47  2005/09/11 21:09:27  fwarmerdam
 * use large file API in CSLLoad
 *
 * Revision 1.46  2005/09/05 20:19:08  fwarmerdam
 * fixed binarytohex function
 *
 * Revision 1.45  2005/08/31 03:31:15  fwarmerdam
 * added binarytohex/hextobinary
 *
 * Revision 1.44  2005/08/04 19:41:33  fwarmerdam
 * support setting null value in CSLSetNameValue
 *
 * Revision 1.43  2005/05/23 03:59:44  fwarmerdam
 * make cplsprintf buffer threadlocal
 *
 * Revision 1.42  2005/04/04 15:23:31  fwarmerdam
 * some functions now CPL_STDCALL
 *
 * Revision 1.41  2004/09/17 21:26:28  fwarmerdam
 * Yikes ... CPLEscapeString() was badly broken for BackslashEscapable.
 *
 * Revision 1.40  2004/08/16 20:23:46  warmerda
 * added .csv escaping
 *
 * Revision 1.39  2004/07/12 21:50:38  warmerda
 * Added SQL escaping style
 *
 * Revision 1.38  2004/04/23 22:23:32  warmerda
 * Fixed key memory leak in seldom used CSLSetNameValueSeperator().
 *
 * Revision 1.37  2003/12/02 15:56:47  warmerda
 * avoid use of CSLAddString() in tokenize, manage list ourselves
 *
 * Revision 1.36  2003/08/29 17:32:27  warmerda
 * Open file in binary mode for CSLLoad() since CPLReadline() works much
 * better then.
 *
 * Revision 1.35  2003/07/17 10:15:40  dron
 * CSLTestBoolean() added.
 *
 * Revision 1.34  2003/05/28 19:22:38  warmerda
 * fixed docs
 *
 * Revision 1.33  2003/05/21 04:20:30  warmerda
 * avoid warnings
 *
 * Revision 1.32  2003/04/04 14:57:38  dron
 * _vsnprintf() hack moved to the cpl_config.h.vc.
 *
 * Revision 1.31  2003/04/04 14:16:07  dron
 * Use _vsnprintf() in Windows environment.
 *
 * Revision 1.30  2003/03/28 05:29:53  warmerda
 * Fixed buffer overflow risk in escaping code (for XML method).  Avoid
 * use of CPLSPrintf() for name/value list assembly to avoid risk with long
 * key names or values.  Use vsnprintf() in CPLSPrintf() on platforms where it
 * is available.  Security audit complete.
 *
 * Revision 1.29  2003/03/27 21:32:08  warmerda
 * Fixed bug with escaped spaces.
 *
 * Revision 1.28  2003/03/11 21:33:02  warmerda
 * added URL encode/decode support, untested
 *
 * Revision 1.27  2003/01/30 19:15:55  warmerda
 * added some docs
 *
 * Revision 1.26  2003/01/14 14:31:16  warmerda
 * Added "OFF" as a negative response to CSLFetchBoolean().
 *
 * Revision 1.25  2002/10/07 19:35:38  dron
 * Fixed description for CSLFetchBoolean()
 *
 * Revision 1.24  2002/07/12 22:37:05  warmerda
 * added CSLFetchBoolean
 *
 * Revision 1.23  2002/07/09 20:25:25  warmerda
 * expand tabs
 *
 * Revision 1.22  2002/05/28 18:53:43  warmerda
 * added XML escaping support
 *
 * Revision 1.21  2002/04/26 14:55:26  warmerda
 * Added CPLEscapeString() and CPLUnescapeString() (unescape untested)
 *
 * Revision 1.20  2002/03/05 14:26:57  warmerda
 * expanded tabs
 *
 * Revision 1.19  2002/01/16 03:59:27  warmerda
 * added CPLTokenizeString2
 *
 * Revision 1.18  2001/12/11 22:40:26  warmerda
 * cleanup CPLReadLine buffer in CSLLoad()
 *
 * Revision 1.17  2001/11/07 14:31:16  warmerda
 * doc fix
 *
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

/**********************************************************************
 *                       CSLLoad()
 *
 * Load a test file into a stringlist.
 *
 * Lines are limited in length by the size of the CPLReadLine() buffer.
 **********************************************************************/
char **CSLLoad(const char *pszFname)
{
    FILE        *fp;
    const char  *pszLine;
    char        **papszStrList=NULL;

    fp = VSIFOpenL(pszFname, "rb");

    if (fp)
    {
        while(!VSIFEofL(fp))
        {
            if ( (pszLine = CPLReadLineL(fp)) != NULL )
            {
                papszStrList = CSLAddString(papszStrList, pszLine);
            }
        }

        VSIFCloseL(fp);

        CPLReadLineL( NULL );
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
    int     nLines = 0;

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
/*                                                                      */
/*      The ultimate tokenizer?                                         */
/************************************************************************/

char ** CSLTokenizeString2( const char * pszString,
                            const char * pszDelimiters,
                            int nCSLTFlags )

{
    char        **papszRetList = NULL;
    int         nRetMax = 0, nRetLen = 0;
    char        *pszToken;
    int         nTokenMax, nTokenLen;
    int         bHonourStrings = (nCSLTFlags & CSLT_HONOURSTRINGS);
    int         bAllowEmptyTokens = (nCSLTFlags & CSLT_ALLOWEMPTYTOKENS);

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

            /* Within string constants we allow for escaped quotes, but
               in processing them we will unescape the quotes */
            if( bInString && pszString[0] == '\\' && pszString[1] == '"' )
            {
                if( nCSLTFlags & CSLT_PRESERVEESCAPES )
                {
                    pszToken[nTokenLen] = *pszString;
                    nTokenLen++;
                }

                pszString++;
            }

            /* Within string constants a \\ sequence reduces to \ */
            else if( bInString 
                     && pszString[0] == '\\' && pszString[1] == '\\' )
            {
                if( nCSLTFlags & CSLT_PRESERVEESCAPES )
                {
                    pszToken[nTokenLen] = *pszString;
                    nTokenLen++;
                }
                pszString++;
            }

            if( nTokenLen >= nTokenMax-3 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = (char *) CPLRealloc( pszToken, nTokenMax );
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        pszToken[nTokenLen] = '\0';

        /*
         * Add the token.
         */
        if( pszToken[0] != '\0' || bAllowEmptyTokens )
        {
            if( nRetLen >= nRetMax - 1 )
            {
                nRetMax = nRetMax * 2 + 10;
                papszRetList = (char **) 
                    CPLRealloc(papszRetList, sizeof(char*) * nRetMax );
            }

            papszRetList[nRetLen++] = CPLStrdup( pszToken );
            papszRetList[nRetLen] = NULL;
        }
    }

    /*
     * If the last token was empty, then we need to capture
     * it now, as the loop would skip it.
     */
    if( *pszString == '\0' && bAllowEmptyTokens && nRetLen > 0 
        && strchr(pszDelimiters,*(pszString-1)) != NULL )
    {
        if( nRetLen >= nRetMax - 1 )
        {
            nRetMax = nRetMax * 2 + 10;
            papszRetList = (char **) 
                CPLRealloc(papszRetList, sizeof(char*) * nRetMax );
        }

        papszRetList[nRetLen++] = CPLStrdup("");
        papszRetList[nRetLen] = NULL;
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
static CPL_THREADLOCAL char gszCPLSPrintfBuffer[CPLSPrintf_BUF_Count][CPLSPrintf_BUF_SIZE];
static CPL_THREADLOCAL int gnCPLSPrintfBuffer = 0;

const char *CPLSPrintf(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
#if defined(HAVE_VSNPRINTF)
    vsnprintf(gszCPLSPrintfBuffer[gnCPLSPrintfBuffer], CPLSPrintf_BUF_SIZE-1,
              fmt, args);
#else
    vsprintf(gszCPLSPrintfBuffer[gnCPLSPrintfBuffer], fmt, args);
#endif
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
#if defined(HAVE_VSNPRINTF)
    vsnprintf(gszCPLSPrintfBuffer[gnCPLSPrintfBuffer], CPLSPrintf_BUF_SIZE-1,
              fmt, args);
#else
    vsprintf(gszCPLSPrintfBuffer[gnCPLSPrintfBuffer], fmt, args);
#endif
    va_end(args);

    int nCurrent = gnCPLSPrintfBuffer;

    if (++gnCPLSPrintfBuffer == CPLSPrintf_BUF_Count)
      gnCPLSPrintfBuffer = 0;

    return CSLAddString(papszStrList, gszCPLSPrintfBuffer[nCurrent]);
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
    int nLen;

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
 * The backslash, quote, '\0' and newline characters are all escaped in 
 * the usual C style. 
 *
 * CPLES_XML(1): This scheme converts the '<', '<' and '&' characters into
 * their XML/HTML equivelent (&gt;, &lt; and &amp;) making a string safe
 * to embed as CDATA within an XML element.  The '\0' is not escaped and 
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
 * CPLES_CSV(4): If the values contains commas, double quotes, or newlines it 
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
                || pszInput[iIn] == '_' )
            {
                pszOutput[iOut++] = pszInput[iIn];
            }
            else
            {
                sprintf( pszOutput, "%%%02X", pszInput[iIn] );
                iOut += 3;
            }
        }
        pszOutput[iOut] = '\0';
    }
    else if( nScheme == CPLES_XML )
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
            else if( pszInput[iIn] == '"' )
            {
                pszOutput[iOut++] = '&';
                pszOutput[iOut++] = 'q';
                pszOutput[iOut++] = 'u';
                pszOutput[iOut++] = 'o';
                pszOutput[iOut++] = 't';
                pszOutput[iOut++] = ';';
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
                else if( pszInput[iIn] == 13 )
                    /* drop DOS LF's in strings. */;
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
 * which may in some cases include embedded '\0' characters.
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

    pszOutput = (char *) CPLMalloc(strlen(pszInput)+1);
    pszOutput[0] = '\0';

    if( nScheme == CPLES_XML )
    {
        for( iIn = 0; pszInput[iIn] != '\0'; iIn++ )
        {
            if( EQUALN(pszInput+iIn,"&lt;",4) )
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
            else if( EQUALN(pszInput+iIn,"&quot;",6) )
            {
                pszOutput[iOut++] = '"';
                iIn += 5;
            }
            else
            {
                pszOutput[iOut++] = pszInput[iIn];
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
 * @param 

*/

GByte *CPLHexToBinary( const char *pszHex, int *pnBytes )

{
    int iSrc = 0, iDst = 0, nHexLen = strlen(pszHex);

    GByte *pabyWKB;

    pabyWKB = (GByte *) CPLMalloc(nHexLen / 2 + 2);

    while( pszHex[iSrc] != '\0' )
    {
        if( pszHex[iSrc] >= '0' && pszHex[iSrc] <= '9' )
            pabyWKB[iDst] = pszHex[iSrc] - '0';
        else if( pszHex[iSrc] >= 'A' && pszHex[iSrc] <= 'F' )
            pabyWKB[iDst] = pszHex[iSrc] - 'A' + 10;
        else if( pszHex[iSrc] >= 'a' && pszHex[iSrc] <= 'f' )
            pabyWKB[iDst] = pszHex[iSrc] - 'a' + 10;
        else 
            break;

        pabyWKB[iDst] *= 16;

        iSrc++;

        if( pszHex[iSrc] >= '0' && pszHex[iSrc] <= '9' )
            pabyWKB[iDst] += pszHex[iSrc] - '0';
        else if( pszHex[iSrc] >= 'A' && pszHex[iSrc] <= 'F' )
            pabyWKB[iDst] += pszHex[iSrc] - 'A' + 10;
        else if( pszHex[iSrc] >= 'a' && pszHex[iSrc] <= 'f' )
            pabyWKB[iDst] += pszHex[iSrc] - 'a' + 10;
        else
            break;

        iSrc++;
        iDst++;
    }
    
    pabyWKB[iDst] = 0;
    *pnBytes = iDst;

    return pabyWKB;
}
