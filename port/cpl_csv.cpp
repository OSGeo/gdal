/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CSV (comma separated value) file access.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_csv.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "gdal_csv.h"

CPL_CVSID("$Id$")

/* ==================================================================== */
/*      The CSVTable is a persistent set of info about an open CSV      */
/*      table.  While it doesn't currently maintain a record index,     */
/*      or in-memory copy of the table, it could be changed to do so    */
/*      in the future.                                                  */
/* ==================================================================== */
typedef struct ctb {
    VSILFILE   *fp;
    struct ctb *psNext;
    char       *pszFilename;
    char      **papszFieldNames;
    int        *panFieldNamesLength;
    char      **papszRecFields;
    int         nFields;
    int         iLastLine;
    bool        bNonUniqueKey;

    /* Cache for whole file */
    int         nLineCount;
    char      **papszLines;
    int        *panLineIndex;
    char       *pszRawData;
} CSVTable;

static void CSVDeaccessInternal( CSVTable **ppsCSVTableList, bool bCanUseTLS,
                                 const char * pszFilename );

/************************************************************************/
/*                            CSVFreeTLS()                              */
/************************************************************************/
static void CSVFreeTLS( void* pData )
{
    CSVDeaccessInternal( static_cast<CSVTable **>( pData ), false, nullptr );
    CPLFree(pData);
}

/* It would likely be better to share this list between threads, but
   that will require some rework. */

/************************************************************************/
/*                             CSVAccess()                              */
/*                                                                      */
/*      This function will fetch a handle to the requested table.       */
/*      If not found in the ``open table list'' the table will be       */
/*      opened and added to the list.  Eventually this function may     */
/*      become public with an abstracted return type so that            */
/*      applications can set options about the table.  For now this     */
/*      isn't done.                                                     */
/************************************************************************/

static CSVTable *CSVAccess( const char * pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Fetch the table, and allocate the thread-local pointer to it    */
/*      if there isn't already one.                                     */
/* -------------------------------------------------------------------- */
    int bMemoryError = FALSE;
    CSVTable **ppsCSVTableList = static_cast<CSVTable **>(
        CPLGetTLSEx( CTLS_CSVTABLEPTR, &bMemoryError ) );
    if( bMemoryError )
        return nullptr;
    if( ppsCSVTableList == nullptr )
    {
        ppsCSVTableList = static_cast<CSVTable **>(
            VSI_CALLOC_VERBOSE( 1, sizeof(CSVTable*) ) );
        if( ppsCSVTableList == nullptr )
            return nullptr;
        CPLSetTLSWithFreeFunc( CTLS_CSVTABLEPTR, ppsCSVTableList, CSVFreeTLS );
    }

/* -------------------------------------------------------------------- */
/*      Is the table already in the list.                               */
/* -------------------------------------------------------------------- */
    for( CSVTable *psTable = *ppsCSVTableList;
         psTable != nullptr;
         psTable = psTable->psNext )
    {
        if( EQUAL(psTable->pszFilename, pszFilename) )
        {
            /*
             * Eventually we should consider promoting to the front of
             * the list to accelerate frequently accessed tables.
             */
            return psTable;
        }
    }

/* -------------------------------------------------------------------- */
/*      If not, try to open it.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create an information structure about this table, and add to    */
/*      the front of the list.                                          */
/* -------------------------------------------------------------------- */
    CSVTable * const psTable = static_cast<CSVTable *>(
        VSI_CALLOC_VERBOSE( sizeof(CSVTable), 1 ) );
    if( psTable == nullptr )
    {
        VSIFCloseL(fp);
        return nullptr;
    }

    psTable->fp = fp;
    psTable->pszFilename = VSI_STRDUP_VERBOSE( pszFilename );
    if( psTable->pszFilename == nullptr )
    {
        VSIFree(psTable);
        VSIFCloseL(fp);
        return nullptr;
    }
    psTable->bNonUniqueKey = false;  // As far as we know now.
    psTable->psNext = *ppsCSVTableList;

    *ppsCSVTableList = psTable;

/* -------------------------------------------------------------------- */
/*      Read the table header record containing the field names.        */
/* -------------------------------------------------------------------- */
    psTable->papszFieldNames = CSVReadParseLineL( fp );
    psTable->nFields = CSLCount(psTable->papszFieldNames);
    psTable->panFieldNamesLength = static_cast<int*>(
        CPLMalloc(sizeof(int) * psTable->nFields));
    for(int i = 0; i < psTable->nFields &&
        /* null-pointer check to avoid a false positive from CLang S.A. */
                   psTable->papszFieldNames != nullptr; i++ )
    {
        psTable->panFieldNamesLength[i] = static_cast<int>(
            strlen(psTable->papszFieldNames[i]));
    }

    return psTable;
}

/************************************************************************/
/*                            CSVDeaccess()                             */
/************************************************************************/

static void CSVDeaccessInternal( CSVTable **ppsCSVTableList, bool bCanUseTLS,
                                 const char * pszFilename )

{
    if( ppsCSVTableList == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      A NULL means deaccess all tables.                               */
/* -------------------------------------------------------------------- */
    if( pszFilename == nullptr )
    {
        while( *ppsCSVTableList != nullptr )
            CSVDeaccessInternal( ppsCSVTableList, bCanUseTLS,
                                 (*ppsCSVTableList)->pszFilename );

        return;
    }

/* -------------------------------------------------------------------- */
/*      Find this table.                                                */
/* -------------------------------------------------------------------- */
    CSVTable *psLast = nullptr;
    CSVTable *psTable = *ppsCSVTableList;
    for( ;
         psTable != nullptr && !EQUAL(psTable->pszFilename, pszFilename);
         psTable = psTable->psNext )
    {
        psLast = psTable;
    }

    if( psTable == nullptr )
    {
        if( bCanUseTLS )
            CPLDebug( "CPL_CSV", "CPLDeaccess( %s ) - no match.", pszFilename );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Remove the link from the list.                                  */
/* -------------------------------------------------------------------- */
    if( psLast != nullptr )
        psLast->psNext = psTable->psNext;
    else
        *ppsCSVTableList = psTable->psNext;

/* -------------------------------------------------------------------- */
/*      Free the table.                                                 */
/* -------------------------------------------------------------------- */
    if( psTable->fp != nullptr )
        VSIFCloseL( psTable->fp );

    CSLDestroy( psTable->papszFieldNames );
    CPLFree( psTable->panFieldNamesLength );
    CSLDestroy( psTable->papszRecFields );
    CPLFree( psTable->pszFilename );
    CPLFree( psTable->panLineIndex );
    CPLFree( psTable->pszRawData );
    CPLFree( psTable->papszLines );

    CPLFree( psTable );

    if( bCanUseTLS )
        CPLReadLine( nullptr );
}

void CSVDeaccess( const char * pszFilename )
{
/* -------------------------------------------------------------------- */
/*      Fetch the table, and allocate the thread-local pointer to it    */
/*      if there isn't already one.                                     */
/* -------------------------------------------------------------------- */
    int bMemoryError = FALSE;
    CSVTable **ppsCSVTableList = static_cast<CSVTable **>(
        CPLGetTLSEx( CTLS_CSVTABLEPTR, &bMemoryError ) );

    CSVDeaccessInternal(ppsCSVTableList, true, pszFilename);
}

/************************************************************************/
/*                            CSVSplitLine()                            */
/*                                                                      */
/*      Tokenize a CSV line into fields in the form of a string         */
/*      list.  This is used instead of the CPLTokenizeString()          */
/*      because it provides correct CSV escaping and quoting            */
/*      semantics.                                                      */
/************************************************************************/

static char **CSVSplitLine( const char *pszString,
                            const char *pszDelimiter,
                            bool bKeepLeadingAndClosingQuotes,
                            bool bMergeDelimiter )

{
    CPLStringList aosRetList;
    if( pszString == nullptr )
        return static_cast<char **>(CPLCalloc(sizeof(char *), 1));

    char *pszToken = static_cast<char *>(CPLCalloc(10, 1));
    int nTokenMax = 10;
    const size_t nDelimiterLength = strlen(pszDelimiter);

    const char* pszIter = pszString;
    while( *pszIter != '\0' )
    {
        bool bInString = false;

        int nTokenLen = 0;

        // Try to find the next delimiter, marking end of token.
        do
        {
            // End if this is a delimiter skip it and break.
            if( !bInString && strncmp(pszIter, pszDelimiter, nDelimiterLength) == 0 )
            {
                pszIter += nDelimiterLength;
                if( bMergeDelimiter )
                {
                    while( strncmp(pszIter, pszDelimiter, nDelimiterLength) == 0 )
                        pszIter += nDelimiterLength;
                }
                break;
            }

            if( *pszIter == '"' )
            {
                if( !bInString || pszIter[1] != '"' )
                {
                    bInString = !bInString;
                    if( !bKeepLeadingAndClosingQuotes )
                        continue;
                }
                else  // Doubled quotes in string resolve to one quote.
                {
                    pszIter++;
                }
            }

            if( nTokenLen >= nTokenMax - 2 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = static_cast<char *>(CPLRealloc(pszToken, nTokenMax));
            }

            pszToken[nTokenLen] = *pszIter;
            nTokenLen++;
        } while( *(++pszIter) != '\0' );

        pszToken[nTokenLen] = '\0';
        aosRetList.AddString(pszToken);

        // If the last token is an empty token, then we have to catch
        // it now, otherwise we won't reenter the loop and it will be lost.
        if( *pszIter == '\0' &&
            pszIter - pszString >= static_cast<int>(nDelimiterLength) &&
            strncmp(pszIter - nDelimiterLength, pszDelimiter, nDelimiterLength) == 0 )
        {
            aosRetList.AddString("");
        }
    }

    CPLFree(pszToken);

    if( aosRetList.Count() == 0 )
        return static_cast<char **>(CPLCalloc(sizeof(char *), 1));
    else
        return aosRetList.StealList();
}


/************************************************************************/
/*                          CSVFindNextLine()                           */
/*                                                                      */
/*      Find the start of the next line, while at the same time zero    */
/*      terminating this line.  Take into account that there may be     */
/*      newline indicators within quoted strings, and that quotes       */
/*      can be escaped with a backslash.                                */
/************************************************************************/

static char *CSVFindNextLine( char *pszThisLine )

{
    int i = 0;  // i is used after the for loop.

    for( int nQuoteCount = 0; pszThisLine[i] != '\0'; i++ )
    {
        if( pszThisLine[i] == '\"'
            && (i == 0 || pszThisLine[i-1] != '\\') )
            nQuoteCount++;

        if( (pszThisLine[i] == 10 || pszThisLine[i] == 13)
            && (nQuoteCount % 2) == 0 )
            break;
    }

    while( pszThisLine[i] == 10 || pszThisLine[i] == 13 )
        pszThisLine[i++] = '\0';

    if( pszThisLine[i] == '\0' )
        return nullptr;

    return pszThisLine + i;
}

/************************************************************************/
/*                             CSVIngest()                              */
/*                                                                      */
/*      Load entire file into memory and setup index if possible.       */
/************************************************************************/

// TODO(schwehr): Clean up all the casting in CSVIngest.
static void CSVIngest( CSVTable *psTable )

{
    if( psTable->pszRawData != nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Ingest whole file.                                              */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( psTable->fp, 0, SEEK_END ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed using seek end and tell to get file length: %s",
                  psTable->pszFilename );
        return;
    }
    const vsi_l_offset nFileLen = VSIFTellL( psTable->fp );
    if( static_cast<long>(nFileLen) == -1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed using seek end and tell to get file length: %s",
                  psTable->pszFilename );
        return;
    }
    VSIRewindL( psTable->fp );

    psTable->pszRawData = static_cast<char *>(
        VSI_MALLOC_VERBOSE( static_cast<size_t>(nFileLen) + 1) );
    if( psTable->pszRawData == nullptr )
        return;
    if( VSIFReadL( psTable->pszRawData, 1,
                   static_cast<size_t>(nFileLen), psTable->fp )
        != static_cast<size_t>(nFileLen) )
    {
        CPLFree( psTable->pszRawData );
        psTable->pszRawData = nullptr;

        CPLError( CE_Failure, CPLE_FileIO, "Read of file %s failed.",
                  psTable->pszFilename );
        return;
    }

    psTable->pszRawData[nFileLen] = '\0';

/* -------------------------------------------------------------------- */
/*      Get count of newlines so we can allocate line array.            */
/* -------------------------------------------------------------------- */
    int nMaxLineCount = 0;
    for( int i = 0; i < static_cast<int>(nFileLen); i++ )
    {
        if( psTable->pszRawData[i] == 10 )
            nMaxLineCount++;
    }

    psTable->papszLines = static_cast<char **>(
        VSI_CALLOC_VERBOSE( sizeof(char*), nMaxLineCount ) );
    if( psTable->papszLines == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Build a list of record pointers into the raw data buffer        */
/*      based on line terminators.  Zero terminate the line             */
/*      strings.                                                        */
/* -------------------------------------------------------------------- */
    /* skip header line */
    char *pszThisLine = CSVFindNextLine( psTable->pszRawData );

    int iLine = 0;
    while( pszThisLine != nullptr && iLine < nMaxLineCount )
    {
        if( pszThisLine[0] != '#' )
            psTable->papszLines[iLine++] = pszThisLine;
        pszThisLine = CSVFindNextLine( pszThisLine );
    }

    psTable->nLineCount = iLine;

/* -------------------------------------------------------------------- */
/*      Allocate and populate index array.  Ensure they are in          */
/*      ascending order so that binary searches can be done on the      */
/*      array.                                                          */
/* -------------------------------------------------------------------- */
    psTable->panLineIndex = static_cast<int *>(
        VSI_MALLOC_VERBOSE( sizeof(int) * psTable->nLineCount ) );
    if( psTable->panLineIndex == nullptr )
        return;

    for( int i = 0; i < psTable->nLineCount; i++ )
    {
        psTable->panLineIndex[i] = atoi(psTable->papszLines[i]);

        if( i > 0 && psTable->panLineIndex[i] < psTable->panLineIndex[i-1] )
        {
            CPLFree( psTable->panLineIndex );
            psTable->panLineIndex = nullptr;
            break;
        }
    }

    psTable->iLastLine = -1;

/* -------------------------------------------------------------------- */
/*      We should never need the file handle against, so close it.      */
/* -------------------------------------------------------------------- */
    VSIFCloseL( psTable->fp );
    psTable->fp = nullptr;
}

static void CSVIngest( const char *pszFilename )

{
    CSVTable *psTable = CSVAccess( pszFilename );
    if( psTable == nullptr )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Failed to open file: %s",
                  pszFilename );
        return;
    }
    CSVIngest(psTable);
}

/************************************************************************/
/*                        CSVDetectSeperator()                          */
/************************************************************************/

/** Detect which field separator is used.
 *
 * Currently, it can detect comma, semicolon, space or tabulation. In case of
 * ambiguity or no separator found, comma will be considered as the separator.
 *
 * @return ',', ';', ' ' or tabulation character.
 */
char CSVDetectSeperator( const char* pszLine )
{
    bool bInString = false;
    char chDelimiter = '\0';
    int nCountSpace = 0;

    for( ; *pszLine != '\0'; pszLine++ )
    {
        if( !bInString && ( *pszLine == ',' || *pszLine == ';'
                            || *pszLine == '\t'))
        {
            if( chDelimiter == '\0' )
            {
                chDelimiter = *pszLine;
            }
            else if( chDelimiter != *pszLine )
            {
                // The separator is not consistent on the line.
                CPLDebug( "CSV", "Inconsistent separator. '%c' and '%c' found. "
                          "Using ',' as default",
                          chDelimiter, *pszLine);
                chDelimiter = ',';
                break;
            }
        }
        else if( !bInString && *pszLine == ' ' )
        {
            nCountSpace++;
        }
        else if( *pszLine == '"' )
        {
            if( !bInString || pszLine[1] != '"' )
            {
                bInString = !bInString;
                continue;
            }
            else  /* doubled quotes in string resolve to one quote */
            {
                pszLine++;
            }
        }
    }

    if( chDelimiter == '\0' )
    {
        if( nCountSpace > 0 )
            chDelimiter = ' ';
        else
            chDelimiter = ',';
    }

    return chDelimiter;
}

/************************************************************************/
/*                      CSVReadParseLine3L()                            */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/************************************************************************/

static
char **CSVReadParseLineGeneric( void* fp,
                                const char* (*pfnReadLine)(void*, size_t),
                                size_t nMaxLineSize,
                                const char* pszDelimiter,
                                bool bHonourStrings,
                                bool bKeepLeadingAndClosingQuotes,
                                bool bMergeDelimiter,
                                bool bSkipBOM )
{
    const char *pszLine = pfnReadLine(fp, nMaxLineSize);
    if( pszLine == nullptr )
        return nullptr;

    if( bSkipBOM )
    {
        // Skip BOM.
        const GByte *pabyData = reinterpret_cast<const GByte *>(pszLine);
        if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
            pszLine += 3;
    }

    // Special fix to read NdfcFacilities.xls with un-balanced double quotes.
    if( !bHonourStrings )
    {
        return CSLTokenizeStringComplex(pszLine, pszDelimiter, FALSE, TRUE);
    }

    // If there are no quotes, then this is the simple case.
    // Parse, and return tokens.
    if( strchr(pszLine,'\"') == nullptr )
        return CSVSplitLine(pszLine, pszDelimiter,
                            bKeepLeadingAndClosingQuotes,
                            bMergeDelimiter);

    try
    {
        // We must now count the quotes in our working string, and as
        // long as it is odd, keep adding new lines.
        std::string osWorkLine(pszLine);

        size_t i = 0;
        int nCount = 0;

        while( true )
        {
            for( ; i < osWorkLine.size(); i++ )
            {
                if( osWorkLine[i] == '\"' )
                    nCount++;
            }

            if( nCount % 2 == 0 )
                break;

            pszLine = pfnReadLine(fp, nMaxLineSize);
            if( pszLine == nullptr )
                break;

            osWorkLine.append("\n");
            osWorkLine.append(pszLine);
        }

        char **papszReturn =
            CSVSplitLine(osWorkLine.c_str(),
                         pszDelimiter,
                         bKeepLeadingAndClosingQuotes,
                         bMergeDelimiter);

        return papszReturn;
    }
    catch( const std::exception& e )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                          CSVReadParseLine()                          */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/*                                                                      */
/*      Deprecated.  Replaced by CSVReadParseLineL().                   */
/************************************************************************/

char **CSVReadParseLine( FILE * fp )
{
    return CSVReadParseLine2(fp, ',');
}

static const char* ReadLineClassicalFile(void* fp, size_t /* nMaxLineSize */)
{
    return CPLReadLine(static_cast<FILE*>(fp));
}

char **CSVReadParseLine2( FILE * fp, char chDelimiter )
{
    CPLAssert( fp != nullptr );
    if( fp == nullptr )
        return nullptr;

    char szDelimiter[2] = { chDelimiter, 0 };
    return CSVReadParseLineGeneric( fp, ReadLineClassicalFile,
                                    0, // nMaxLineSize,
                                    szDelimiter,
                                    true, // bHonourStrings
                                    false, // bKeepLeadingAndClosingQuotes
                                    false, // bMergeDelimiter
                                    true /* bSkipBOM */);
}

/************************************************************************/
/*                          CSVReadParseLineL()                         */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/*                                                                      */
/*      Replaces CSVReadParseLine().  These functions use the VSI       */
/*      layer to allow reading from other file containers.              */
/************************************************************************/

char **CSVReadParseLineL( VSILFILE * fp )
{
    return CSVReadParseLine2L(fp, ',');
}

char **CSVReadParseLine2L( VSILFILE * fp, char chDelimiter )

{
    CPLAssert( fp != nullptr );
    if( fp == nullptr )
        return nullptr;

    char szDelimiter[2] = { chDelimiter, 0 };
    return CSVReadParseLine3L( fp,
                               0, //nMaxLineSize
                               szDelimiter,
                               true, // bHonourStrings
                               false, // bKeepLeadingAndClosingQuotes
                               false, // bMergeDelimiter
                               true /* bSkipBOM */);
}

/************************************************************************/
/*                      ReadLineLargeFile()                             */
/************************************************************************/

static const char* ReadLineLargeFile(void* fp, size_t nMaxLineSize )
{
    int nBufLength = 0;
    return CPLReadLine3L( static_cast<VSILFILE*>(fp),
                          nMaxLineSize == 0 ?
                              -1 :
                              static_cast<int>(nMaxLineSize),
                          &nBufLength,
                          nullptr );
}

/************************************************************************/
/*                      CSVReadParseLine3L()                            */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/************************************************************************/

/** Read one line, and return split into fields.
 * The return result is a stringlist, in the sense of the CSL functions.
 *
 * @param fp File handle. Must not be NULL
 * @param nMaxLineSize Maximum line size, or 0 for unlimited.
 * @param pszDelimiter Delimiter sequence for readers (can be multiple bytes)
 * @param bHonourStrings Should be true, unless double quotes should not be
 *                       considered when separating fields.
 * @param bKeepLeadingAndClosingQuotes Whether the leading and closing double
 *                                     quote characters should be kept.
 * @param bMergeDelimiter Whether consecutive delimiters should be considered
 *                        as a single one. Should generally be set to false.
 * @param bSkipBOM Whether leading UTF-8 BOM should be skipped.
 */
char **CSVReadParseLine3L( VSILFILE *fp,
                           size_t nMaxLineSize,
                           const char* pszDelimiter,
                           bool bHonourStrings,
                           bool bKeepLeadingAndClosingQuotes,
                           bool bMergeDelimiter,
                           bool bSkipBOM )

{
    return CSVReadParseLineGeneric( fp, ReadLineLargeFile,
                                    nMaxLineSize,
                                    pszDelimiter,
                                    bHonourStrings,
                                    bKeepLeadingAndClosingQuotes,
                                    bMergeDelimiter,
                                    bSkipBOM );
}

/************************************************************************/
/*                             CSVCompare()                             */
/*                                                                      */
/*      Compare a field to a search value using a particular            */
/*      criteria.                                                       */
/************************************************************************/

static bool CSVCompare( const char * pszFieldValue, const char * pszTarget,
                       CSVCompareCriteria eCriteria )

{
    if( eCriteria == CC_ExactString )
    {
        return( strcmp( pszFieldValue, pszTarget ) == 0 );
    }
    else if( eCriteria == CC_ApproxString )
    {
        return EQUAL( pszFieldValue, pszTarget );
    }
    else if( eCriteria == CC_Integer )
    {
        return( CPLGetValueType(pszFieldValue) == CPL_VALUE_INTEGER &&
                atoi(pszFieldValue) == atoi(pszTarget) );
    }

    return false;
}

/************************************************************************/
/*                            CSVScanLines()                            */
/*                                                                      */
/*      Read the file scanline for lines where the key field equals     */
/*      the indicated value with the suggested comparison criteria.     */
/*      Return the first matching line split into fields.               */
/*                                                                      */
/*      Deprecated.  Replaced by CSVScanLinesL().                       */
/************************************************************************/

char **CSVScanLines( FILE *fp, int iKeyField, const char * pszValue,
                     CSVCompareCriteria eCriteria )

{
    CPLAssert( pszValue != nullptr );
    CPLAssert( iKeyField >= 0 );
    CPLAssert( fp != nullptr );

    bool bSelected = false;
    const int nTestValue = atoi(pszValue);
    char **papszFields = nullptr;

    while( !bSelected ) {
        papszFields = CSVReadParseLine( fp );
        if( papszFields == nullptr )
            return nullptr;

        if( CSLCount( papszFields ) < iKeyField+1 )
        {
            /* not selected */
        }
        else if( eCriteria == CC_Integer
                 && atoi(papszFields[iKeyField]) == nTestValue )
        {
            bSelected = true;
        }
        else
        {
            bSelected = CSVCompare( papszFields[iKeyField], pszValue,
                                    eCriteria );
        }

        if( !bSelected )
        {
            CSLDestroy( papszFields );
            papszFields = nullptr;
        }
    }

    return papszFields;
}

/************************************************************************/
/*                            CSVScanLinesL()                           */
/*                                                                      */
/*      Read the file scanline for lines where the key field equals     */
/*      the indicated value with the suggested comparison criteria.     */
/*      Return the first matching line split into fields.               */
/************************************************************************/

char **CSVScanLinesL( VSILFILE *fp, int iKeyField, const char * pszValue,
                      CSVCompareCriteria eCriteria )

{
    CPLAssert( pszValue != nullptr );
    CPLAssert( iKeyField >= 0 );
    CPLAssert( fp != nullptr );

    bool bSelected = false;
    const int nTestValue = atoi(pszValue);
    char **papszFields = nullptr;

    while( !bSelected ) {
        papszFields = CSVReadParseLineL( fp );
        if( papszFields == nullptr )
            return nullptr;

        if( CSLCount( papszFields ) < iKeyField+1 )
        {
            /* not selected */
        }
        else if( eCriteria == CC_Integer
                 && atoi(papszFields[iKeyField]) == nTestValue )
        {
            bSelected = true;
        }
        else
        {
            bSelected = CSVCompare( papszFields[iKeyField], pszValue,
                                    eCriteria );
        }

        if( !bSelected )
        {
            CSLDestroy( papszFields );
            papszFields = nullptr;
        }
    }

    return papszFields;
}

/************************************************************************/
/*                        CSVScanLinesIndexed()                         */
/*                                                                      */
/*      Read the file scanline for lines where the key field equals     */
/*      the indicated value with the suggested comparison criteria.     */
/*      Return the first matching line split into fields.               */
/************************************************************************/

static char **
CSVScanLinesIndexed( CSVTable *psTable, int nKeyValue )

{
    CPLAssert( psTable->panLineIndex != nullptr );

/* -------------------------------------------------------------------- */
/*      Find target record with binary search.                          */
/* -------------------------------------------------------------------- */
    int iTop = psTable->nLineCount-1;
    int iBottom = 0;
    int iResult = -1;

    while( iTop >= iBottom )
    {
        const int iMiddle = (iTop + iBottom) / 2;
        if( psTable->panLineIndex[iMiddle] > nKeyValue )
            iTop = iMiddle - 1;
        else if( psTable->panLineIndex[iMiddle] < nKeyValue )
            iBottom = iMiddle + 1;
        else
        {
            iResult = iMiddle;
            // if a key is not unique, select the first instance of it.
            while( iResult > 0
                   && psTable->panLineIndex[iResult-1] == nKeyValue )
            {
                psTable->bNonUniqueKey = true;
                iResult--;
            }
            break;
        }
    }

    if( iResult == -1 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Parse target line, and update iLastLine indicator.              */
/* -------------------------------------------------------------------- */
    psTable->iLastLine = iResult;

    return CSVSplitLine( psTable->papszLines[iResult], ",", false, false );
}

/************************************************************************/
/*                        CSVScanLinesIngested()                        */
/*                                                                      */
/*      Read the file scanline for lines where the key field equals     */
/*      the indicated value with the suggested comparison criteria.     */
/*      Return the first matching line split into fields.               */
/************************************************************************/

static char **
CSVScanLinesIngested( CSVTable *psTable, int iKeyField, const char * pszValue,
                      CSVCompareCriteria eCriteria )

{
    CPLAssert( pszValue != nullptr );
    CPLAssert( iKeyField >= 0 );

    const int nTestValue = atoi(pszValue);

/* -------------------------------------------------------------------- */
/*      Short cut for indexed files.                                    */
/* -------------------------------------------------------------------- */
    if( iKeyField == 0 && eCriteria == CC_Integer
        && psTable->panLineIndex != nullptr )
        return CSVScanLinesIndexed( psTable, nTestValue );

/* -------------------------------------------------------------------- */
/*      Scan from in-core lines.                                        */
/* -------------------------------------------------------------------- */
    char **papszFields = nullptr;
    bool bSelected = false;

    while( !bSelected && psTable->iLastLine+1 < psTable->nLineCount ) {
        psTable->iLastLine++;
        papszFields = CSVSplitLine( psTable->papszLines[psTable->iLastLine],
                                    ",",
                                    false, false );

        if( CSLCount( papszFields ) < iKeyField+1 )
        {
            /* not selected */
        }
        else if( eCriteria == CC_Integer
                 && atoi(papszFields[iKeyField]) == nTestValue )
        {
            bSelected = true;
        }
        else
        {
            bSelected = CSVCompare( papszFields[iKeyField], pszValue,
                                    eCriteria );
        }

        if( !bSelected )
        {
            CSLDestroy( papszFields );
            papszFields = nullptr;
        }
    }

    return papszFields;
}

/************************************************************************/
/*                           CSVGetNextLine()                           */
/*                                                                      */
/*      Fetch the next line of a CSV file based on a passed in          */
/*      filename.  Returns NULL at end of file, or if file is not       */
/*      really established.                                             */
/************************************************************************/

char **CSVGetNextLine( const char *pszFilename )

{

/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( pszFilename != nullptr );

    CSVTable * const psTable = CSVAccess( pszFilename );
    if( psTable == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we use CSVGetNextLine() we can pretty much assume we have    */
/*      a non-unique key.                                               */
/* -------------------------------------------------------------------- */
    psTable->bNonUniqueKey = true;

/* -------------------------------------------------------------------- */
/*      Do we have a next line available?  This only works for          */
/*      ingested tables I believe.                                      */
/* -------------------------------------------------------------------- */
    if( psTable->iLastLine+1 >= psTable->nLineCount )
        return nullptr;

    psTable->iLastLine++;
    CSLDestroy( psTable->papszRecFields );
    psTable->papszRecFields =
        CSVSplitLine( psTable->papszLines[psTable->iLastLine], ",", false, false );

    return psTable->papszRecFields;
}

/************************************************************************/
/*                            CSVScanFile()                             */
/*                                                                      */
/*      Scan a whole file using criteria similar to above, but also     */
/*      taking care of file opening and closing.                        */
/************************************************************************/

static
char **CSVScanFile( CSVTable * const psTable, int iKeyField,
                    const char * pszValue, CSVCompareCriteria eCriteria )
{
    CSVIngest( psTable->pszFilename );

/* -------------------------------------------------------------------- */
/*      Does the current record match the criteria?  If so, just        */
/*      return it again.                                                */
/* -------------------------------------------------------------------- */
    if( iKeyField >= 0
        && iKeyField < CSLCount(psTable->papszRecFields)
        && CSVCompare(psTable->papszRecFields[iKeyField], pszValue, eCriteria)
        && !psTable->bNonUniqueKey )
    {
        return psTable->papszRecFields;
    }

/* -------------------------------------------------------------------- */
/*      Scan the file from the beginning, replacing the ``current       */
/*      record'' in our structure with the one that is found.           */
/* -------------------------------------------------------------------- */
    psTable->iLastLine = -1;
    CSLDestroy( psTable->papszRecFields );

    if( psTable->pszRawData != nullptr )
        psTable->papszRecFields =
            CSVScanLinesIngested( psTable, iKeyField, pszValue, eCriteria );
    else
    {
        VSIRewindL( psTable->fp );
        CPLReadLineL( psTable->fp );         /* throw away the header line */

        psTable->papszRecFields =
            CSVScanLinesL( psTable->fp, iKeyField, pszValue, eCriteria );
    }

    return psTable->papszRecFields;
}

char **CSVScanFile( const char * pszFilename, int iKeyField,
                    const char * pszValue, CSVCompareCriteria eCriteria )

{
/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( pszFilename != nullptr );

    if( iKeyField < 0 )
        return nullptr;

    CSVTable * const psTable = CSVAccess( pszFilename );
    if( psTable == nullptr )
        return nullptr;

    return CSVScanFile( psTable, iKeyField, pszValue, eCriteria );
}

/************************************************************************/
/*                           CPLGetFieldId()                            */
/*                                                                      */
/*      Read the first record of a CSV file (rewinding to be sure),     */
/*      and find the field with the indicated name.  Returns -1 if      */
/*      it fails to find the field name.  Comparison is case            */
/*      insensitive, but otherwise exact.  After this function has      */
/*      been called the file pointer will be positioned just after      */
/*      the first record.                                               */
/*                                                                      */
/*      Deprecated.  Replaced by CPLGetFieldIdL().                      */
/************************************************************************/

int CSVGetFieldId( FILE * fp, const char * pszFieldName )

{
    CPLAssert( fp != nullptr && pszFieldName != nullptr );

    VSIRewind( fp );

    char **papszFields = CSVReadParseLine( fp );
    for( int i = 0; papszFields != nullptr && papszFields[i] != nullptr; i++ )
    {
        if( EQUAL(papszFields[i], pszFieldName) )
        {
            CSLDestroy( papszFields );
            return i;
        }
    }

    CSLDestroy( papszFields );

    return -1;
}

/************************************************************************/
/*                           CPLGetFieldIdL()                           */
/*                                                                      */
/*      Read the first record of a CSV file (rewinding to be sure),     */
/*      and find the field with the indicated name.  Returns -1 if      */
/*      it fails to find the field name.  Comparison is case            */
/*      insensitive, but otherwise exact.  After this function has      */
/*      been called the file pointer will be positioned just after      */
/*      the first record.                                               */
/************************************************************************/

int CSVGetFieldIdL( VSILFILE * fp, const char * pszFieldName )

{
    CPLAssert( fp != nullptr && pszFieldName != nullptr );

    VSIRewindL( fp );

    char **papszFields = CSVReadParseLineL( fp );
    for( int i = 0; papszFields != nullptr && papszFields[i] != nullptr; i++ )
    {
        if( EQUAL(papszFields[i], pszFieldName) )
        {
            CSLDestroy( papszFields );
            return i;
        }
    }

    CSLDestroy( papszFields );

    return -1;
}

/************************************************************************/
/*                         CSVGetFileFieldId()                          */
/*                                                                      */
/*      Same as CPLGetFieldId(), except that we get the file based      */
/*      on filename, rather than having an existing handle.             */
/************************************************************************/

static int CSVGetFileFieldId( CSVTable * const psTable, const char * pszFieldName )

{
/* -------------------------------------------------------------------- */
/*      Find the requested field.                                       */
/* -------------------------------------------------------------------- */
    const int nFieldNameLength = static_cast<int>(strlen(pszFieldName));
    for( int i = 0;
         psTable->papszFieldNames != nullptr
             && psTable->papszFieldNames[i] != nullptr;
         i++ )
    {
        if( psTable->panFieldNamesLength[i] == nFieldNameLength &&
            EQUALN(psTable->papszFieldNames[i], pszFieldName,
                   nFieldNameLength) )
        {
            return i;
        }
    }

    return -1;
}

int CSVGetFileFieldId( const char * pszFilename, const char * pszFieldName )

{
/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( pszFilename != nullptr );

    CSVTable * const psTable = CSVAccess( pszFilename );
    if( psTable == nullptr )
        return -1;
    return CSVGetFileFieldId( psTable, pszFieldName );
}


/************************************************************************/
/*                         CSVScanFileByName()                          */
/*                                                                      */
/*      Same as CSVScanFile(), but using a field name instead of a      */
/*      field number.                                                   */
/************************************************************************/

char **CSVScanFileByName( const char * pszFilename,
                          const char * pszKeyFieldName,
                          const char * pszValue, CSVCompareCriteria eCriteria )

{
    const int iKeyField = CSVGetFileFieldId( pszFilename, pszKeyFieldName );
    if( iKeyField == -1 )
        return nullptr;

    return CSVScanFile( pszFilename, iKeyField, pszValue, eCriteria );
}

/************************************************************************/
/*                            CSVGetField()                             */
/*                                                                      */
/*      The all-in-one function to fetch a particular field value       */
/*      from a CSV file.  Note this function will return an empty       */
/*      string, rather than NULL if it fails to find the desired        */
/*      value for some reason.  The caller can't establish that the     */
/*      fetch failed.                                                   */
/************************************************************************/

const char *CSVGetField( const char * pszFilename,
                         const char * pszKeyFieldName,
                         const char * pszKeyFieldValue,
                         CSVCompareCriteria eCriteria,
                         const char * pszTargetField )

{
/* -------------------------------------------------------------------- */
/*      Find the table.                                                 */
/* -------------------------------------------------------------------- */
    CSVTable * const psTable = CSVAccess( pszFilename );
    if( psTable == nullptr )
        return "";

    const int iKeyField = CSVGetFileFieldId( psTable, pszKeyFieldName );
    if( iKeyField == -1 )
        return "";

/* -------------------------------------------------------------------- */
/*      Find the correct record.                                        */
/* -------------------------------------------------------------------- */
    char **papszRecord = CSVScanFile( psTable, iKeyField,
                                      pszKeyFieldValue, eCriteria );
    if( papszRecord == nullptr )
        return "";

/* -------------------------------------------------------------------- */
/*      Figure out which field we want out of this.                     */
/* -------------------------------------------------------------------- */
    const int iTargetField = CSVGetFileFieldId( psTable, pszTargetField );
    if( iTargetField < 0 )
        return "";

    for( int i=0; papszRecord[i] != nullptr; ++i )
    {
        if( i == iTargetField )
            return papszRecord[iTargetField];
    }
    return "";
}

/************************************************************************/
/*                       GDALDefaultCSVFilename()                       */
/************************************************************************/

typedef struct
{
    char szPath[512];
    bool bCSVFinderInitialized;
} DefaultCSVFileNameTLS;

const char * GDALDefaultCSVFilename( const char *pszBasename )

{
/* -------------------------------------------------------------------- */
/*      Do we already have this file accessed?  If so, just return      */
/*      the existing path without any further probing.                  */
/* -------------------------------------------------------------------- */
    int bMemoryError = FALSE;
    CSVTable **ppsCSVTableList = static_cast<CSVTable **>(
      CPLGetTLSEx( CTLS_CSVTABLEPTR, &bMemoryError ) );
    if( ppsCSVTableList != nullptr )
    {
        const size_t nBasenameLen = strlen(pszBasename);

        for( const CSVTable *psTable = *ppsCSVTableList;
             psTable != nullptr;
             psTable = psTable->psNext )
        {
            const size_t nFullLen = strlen(psTable->pszFilename);

            if( nFullLen > nBasenameLen
                && strcmp(psTable->pszFilename+nFullLen-nBasenameLen,
                          pszBasename) == 0
                && strchr("/\\", psTable->pszFilename[+nFullLen-nBasenameLen-1])
                          != nullptr )
            {
                return psTable->pszFilename;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we need to look harder for it.                        */
/* -------------------------------------------------------------------- */
    DefaultCSVFileNameTLS* pTLSData =
        static_cast<DefaultCSVFileNameTLS *>(
            CPLGetTLSEx( CTLS_CSVDEFAULTFILENAME, &bMemoryError ) );
    if( pTLSData == nullptr && !bMemoryError )
    {
        pTLSData = static_cast<DefaultCSVFileNameTLS *>(
            VSI_CALLOC_VERBOSE( 1, sizeof(DefaultCSVFileNameTLS) ) );
        if( pTLSData )
            CPLSetTLS( CTLS_CSVDEFAULTFILENAME, pTLSData, TRUE );
    }
    if( pTLSData == nullptr )
        return "/not_existing_dir/not_existing_path";

    const char *pszResult = CPLFindFile( "gdal", pszBasename );

    if( pszResult != nullptr )
        return pszResult;

    if( !pTLSData->bCSVFinderInitialized )
    {
        pTLSData->bCSVFinderInitialized = true;

        if( CPLGetConfigOption("GDAL_DATA", nullptr) != nullptr )
            CPLPushFinderLocation( CPLGetConfigOption("GDAL_DATA", nullptr) );

        pszResult = CPLFindFile( "gdal", pszBasename );

        if( pszResult != nullptr )
            return pszResult;
    }

    // For systems like sandboxes that do not allow other checks.
    CPLDebug( "CPL_CSV",
              "Failed to find file in GDALDefaultCSVFilename.  "
              "Returning original basename: %s",
              pszBasename );
    CPLStrlcpy(pTLSData->szPath, pszBasename, sizeof(pTLSData->szPath));
    return pTLSData->szPath;
}

/************************************************************************/
/*                            CSVFilename()                             */
/*                                                                      */
/*      Return the full path to a particular CSV file.  This will       */
/*      eventually be something the application can override.           */
/************************************************************************/

CPL_C_START
static const char *(*pfnCSVFilenameHook)(const char *) = nullptr;
CPL_C_END

const char * CSVFilename( const char *pszBasename )

{
    if( pfnCSVFilenameHook == nullptr )
        return GDALDefaultCSVFilename( pszBasename );

    return pfnCSVFilenameHook( pszBasename );
}

/************************************************************************/
/*                         SetCSVFilenameHook()                         */
/*                                                                      */
/*      Applications can use this to set a function that will           */
/*      massage CSV filenames.                                          */
/************************************************************************/

/**
 * Override CSV file search method.
 *
 * @param pfnNewHook The pointer to a function which will return the
 * full path for a given filename.
 *

This function allows an application to override how the GTIFGetDefn()
and related function find the CSV (Comma Separated Value) values
required. The pfnHook argument should be a pointer to a function that
will take in a CSV filename and return a full path to the file. The
returned string should be to an internal static buffer so that the
caller doesn't have to free the result.

<b>Example:</b><br>

The listgeo utility uses the following override function if the user
specified a CSV file directory with the -t commandline switch (argument
put into CSVDirName).  <p>

<pre>

    ...
    SetCSVFilenameHook( CSVFileOverride );
    ...

static const char *CSVFileOverride( const char * pszInput )

{
    static char szPath[1024] = {};

    sprintf( szPath, "%s/%s", CSVDirName, pszInput );

    return szPath;
}
</pre>

*/

CPL_C_START
void SetCSVFilenameHook( const char *(*pfnNewHook)( const char * ) )

{
    pfnCSVFilenameHook = pfnNewHook;
}
CPL_C_END
