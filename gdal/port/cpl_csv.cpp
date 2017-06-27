/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CSV (comma separated value) file access.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
    char      **papszRecFields;
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
    CSVDeaccessInternal( static_cast<CSVTable **>( pData ), false, NULL );
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
        return NULL;
    if( ppsCSVTableList == NULL )
    {
        ppsCSVTableList = static_cast<CSVTable **>(
            VSI_CALLOC_VERBOSE( 1, sizeof(CSVTable*) ) );
        if( ppsCSVTableList == NULL )
            return NULL;
        CPLSetTLSWithFreeFunc( CTLS_CSVTABLEPTR, ppsCSVTableList, CSVFreeTLS );
    }

/* -------------------------------------------------------------------- */
/*      Is the table already in the list.                               */
/* -------------------------------------------------------------------- */
    for( CSVTable *psTable = *ppsCSVTableList;
         psTable != NULL;
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
    if( fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create an information structure about this table, and add to    */
/*      the front of the list.                                          */
/* -------------------------------------------------------------------- */
    CSVTable * const psTable = static_cast<CSVTable *>(
        VSI_CALLOC_VERBOSE( sizeof(CSVTable), 1 ) );
    if( psTable == NULL )
    {
        VSIFCloseL(fp);
        return NULL;
    }

    psTable->fp = fp;
    psTable->pszFilename = VSI_STRDUP_VERBOSE( pszFilename );
    if( psTable->pszFilename == NULL )
    {
        VSIFree(psTable);
        VSIFCloseL(fp);
        return NULL;
    }
    psTable->bNonUniqueKey = false;  // As far as we know now.
    psTable->psNext = *ppsCSVTableList;

    *ppsCSVTableList = psTable;

/* -------------------------------------------------------------------- */
/*      Read the table header record containing the field names.        */
/* -------------------------------------------------------------------- */
    psTable->papszFieldNames = CSVReadParseLineL( fp );

    return psTable;
}

/************************************************************************/
/*                            CSVDeaccess()                             */
/************************************************************************/

static void CSVDeaccessInternal( CSVTable **ppsCSVTableList, bool bCanUseTLS,
                                 const char * pszFilename )

{
    if( ppsCSVTableList == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      A NULL means deaccess all tables.                               */
/* -------------------------------------------------------------------- */
    if( pszFilename == NULL )
    {
        while( *ppsCSVTableList != NULL )
            CSVDeaccessInternal( ppsCSVTableList, bCanUseTLS,
                                 (*ppsCSVTableList)->pszFilename );

        return;
    }

/* -------------------------------------------------------------------- */
/*      Find this table.                                                */
/* -------------------------------------------------------------------- */
    CSVTable *psLast = NULL;
    CSVTable *psTable = *ppsCSVTableList;
    for( ;
         psTable != NULL && !EQUAL(psTable->pszFilename, pszFilename);
         psTable = psTable->psNext )
    {
        psLast = psTable;
    }

    if( psTable == NULL )
    {
        if( bCanUseTLS )
            CPLDebug( "CPL_CSV", "CPLDeaccess( %s ) - no match.", pszFilename );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Remove the link from the list.                                  */
/* -------------------------------------------------------------------- */
    if( psLast != NULL )
        psLast->psNext = psTable->psNext;
    else
        *ppsCSVTableList = psTable->psNext;

/* -------------------------------------------------------------------- */
/*      Free the table.                                                 */
/* -------------------------------------------------------------------- */
    if( psTable->fp != NULL )
        VSIFCloseL( psTable->fp );

    CSLDestroy( psTable->papszFieldNames );
    CSLDestroy( psTable->papszRecFields );
    CPLFree( psTable->pszFilename );
    CPLFree( psTable->panLineIndex );
    CPLFree( psTable->pszRawData );
    CPLFree( psTable->papszLines );

    CPLFree( psTable );

    if( bCanUseTLS )
        CPLReadLine( NULL );
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

static char **CSVSplitLine( const char *pszString, char chDelimiter )

{

    char *pszToken = static_cast<char *>( VSI_CALLOC_VERBOSE( 10, 1 ) );
    if( pszToken == NULL )
        return NULL;

    int nTokenMax = 10;
    char **papszRetList = NULL;

    while( pszString != NULL && *pszString != '\0' )
    {
        bool bInString = false;
        int nTokenLen = 0;

        /* Try to find the next delimiter, marking end of token */
        for( ; *pszString != '\0'; pszString++ )
        {

            /* End if this is a delimiter skip it and break. */
            if( !bInString && *pszString == chDelimiter )
            {
                pszString++;
                break;
            }

            if( *pszString == '"' )
            {
                if( !bInString || pszString[1] != '"' )
                {
                    bInString = !bInString;
                    continue;
                }
                else  /* doubled quotes in string resolve to one quote */
                {
                    pszString++;
                }
            }

            if( nTokenLen >= nTokenMax-2 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                char* pszTokenNew = static_cast<char *>(
                    VSI_REALLOC_VERBOSE( pszToken, nTokenMax ) );
                if( pszTokenNew == NULL )
                {
                    VSIFree(pszToken);
                    CSLDestroy(papszRetList);
                    return NULL;
                }
                pszToken = pszTokenNew;
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        pszToken[nTokenLen] = '\0';
        char** papszRetListNew = CSLAddStringMayFail( papszRetList, pszToken );
        if( papszRetListNew == NULL )
        {
            VSIFree(pszToken);
            CSLDestroy(papszRetList);
            return NULL;
        }
        papszRetList = papszRetListNew;

        /* If the last token is an empty token, then we have to catch
         * it now, otherwise we won't reenter the loop and it will be lost.
         */
        if( *pszString == '\0' && *(pszString-1) == chDelimiter )
        {
            papszRetListNew = CSLAddStringMayFail( papszRetList, "" );
            if( papszRetListNew == NULL )
            {
                VSIFree(pszToken);
                CSLDestroy(papszRetList);
                return NULL;
            }
            papszRetList = papszRetListNew;
        }
    }

    VSIFree( pszToken );

    return papszRetList;
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
        return NULL;

    return pszThisLine + i;
}

/************************************************************************/
/*                             CSVIngest()                              */
/*                                                                      */
/*      Load entire file into memory and setup index if possible.       */
/************************************************************************/

// TODO(schwehr): Clean up all the casting in CSVIngest.
static void CSVIngest( const char *pszFilename )

{
    CSVTable *psTable = CSVAccess( pszFilename );
    if( psTable == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Failed to open file: %s",
                  pszFilename );
        return;
    }

    if( psTable->pszRawData != NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Ingest whole file.                                              */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( psTable->fp, 0, SEEK_END ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed using seek end and tell to get file length: %s",
                  pszFilename );
        return;
    }
    const vsi_l_offset nFileLen = VSIFTellL( psTable->fp );
    if( static_cast<long>(nFileLen) == -1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed using seek end and tell to get file length: %s",
                  pszFilename );
        return;
    }
    VSIRewindL( psTable->fp );

    psTable->pszRawData = static_cast<char *>(
        VSI_MALLOC_VERBOSE( static_cast<size_t>(nFileLen) + 1) );
    if( psTable->pszRawData == NULL )
        return;
    if( VSIFReadL( psTable->pszRawData, 1,
                   static_cast<size_t>(nFileLen), psTable->fp )
        != static_cast<size_t>(nFileLen) )
    {
        CPLFree( psTable->pszRawData );
        psTable->pszRawData = NULL;

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
    if( psTable->papszLines == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Build a list of record pointers into the raw data buffer        */
/*      based on line terminators.  Zero terminate the line             */
/*      strings.                                                        */
/* -------------------------------------------------------------------- */
    /* skip header line */
    char *pszThisLine = CSVFindNextLine( psTable->pszRawData );

    int iLine = 0;
    while( pszThisLine != NULL && iLine < nMaxLineCount )
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
    if( psTable->panLineIndex == NULL )
        return;

    for( int i = 0; i < psTable->nLineCount; i++ )
    {
        psTable->panLineIndex[i] = atoi(psTable->papszLines[i]);

        if( i > 0 && psTable->panLineIndex[i] < psTable->panLineIndex[i-1] )
        {
            CPLFree( psTable->panLineIndex );
            psTable->panLineIndex = NULL;
            break;
        }
    }

    psTable->iLastLine = -1;

/* -------------------------------------------------------------------- */
/*      We should never need the file handle against, so close it.      */
/* -------------------------------------------------------------------- */
    VSIFCloseL( psTable->fp );
    psTable->fp = NULL;
}

/************************************************************************/
/*                        CSVDetectSeperator()                          */
/************************************************************************/

/** Detect which field separator is used.
 *
 * Currently, it can detect comma, semicolon, space or tabulation. In case of
 * ambiguity or no separator found, comma will be considered as the separator.
 *
 * @return ',', ';', ' ' or '\t'
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

char **CSVReadParseLine2( FILE * fp, char chDelimiter )

{
    CPLAssert( fp != NULL );
    if( fp == NULL )
        return NULL;

    const char *pszLine = CPLReadLine( fp );
    if( pszLine == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If there are no quotes, then this is the simple case.           */
/*      Parse, and return tokens.                                       */
/* -------------------------------------------------------------------- */
    if( strchr(pszLine, '\"') == NULL )
        return CSVSplitLine( pszLine, chDelimiter );

/* -------------------------------------------------------------------- */
/*      We must now count the quotes in our working string, and as      */
/*      long as it is odd, keep adding new lines.                       */
/* -------------------------------------------------------------------- */
    char *pszWorkLine = CPLStrdup( pszLine );

    int i = 0;
    int nCount = 0;
    size_t nWorkLineLength = strlen(pszWorkLine);

    while( true )
    {
        for( ; pszWorkLine[i] != '\0'; i++ )
        {
            if( pszWorkLine[i] == '\"'
                && (i == 0 || pszWorkLine[i-1] != '\\') )
                nCount++;
        }

        if( nCount % 2 == 0 )
            break;

        pszLine = CPLReadLine( fp );
        if( pszLine == NULL )
            break;

        const size_t nLineLen = strlen(pszLine);

        char* pszWorkLineTmp = static_cast<char *>(
            VSIRealloc(pszWorkLine,
                       nWorkLineLength + nLineLen + 2) );
        if( pszWorkLineTmp == NULL )
            break;
        pszWorkLine = pszWorkLineTmp;
        // The newline gets lost in CPLReadLine().
        strcat( pszWorkLine + nWorkLineLength, "\n" );
        strcat( pszWorkLine + nWorkLineLength, pszLine );

        nWorkLineLength += nLineLen + 1;
    }

    char **papszReturn = CSVSplitLine( pszWorkLine, chDelimiter );

    CPLFree( pszWorkLine );

    return papszReturn;
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
    CPLAssert( fp != NULL );
    if( fp == NULL )
        return NULL;

    const char *pszLine = CPLReadLineL( fp );
    if( pszLine == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If there are no quotes, then this is the simple case.           */
/*      Parse, and return tokens.                                       */
/* -------------------------------------------------------------------- */
    if( strchr(pszLine, '\"') == NULL )
        return CSVSplitLine( pszLine, chDelimiter );

/* -------------------------------------------------------------------- */
/*      We must now count the quotes in our working string, and as      */
/*      long as it is odd, keep adding new lines.                       */
/* -------------------------------------------------------------------- */
    char *pszWorkLine = CPLStrdup( pszLine );

    int i = 0;
    int nCount = 0;
    size_t nWorkLineLength = strlen(pszWorkLine);

    while( true )
    {
        for( ; pszWorkLine[i] != '\0'; i++ )
        {
            if( pszWorkLine[i] == '\"'
                && (i == 0 || pszWorkLine[i-1] != '\\') )
                nCount++;
        }

        if( nCount % 2 == 0 )
            break;

        pszLine = CPLReadLineL( fp );
        if( pszLine == NULL )
            break;

        const size_t nLineLen = strlen(pszLine);

        char* pszWorkLineTmp = static_cast<char *>(
            VSIRealloc(pszWorkLine,
                       nWorkLineLength + nLineLen + 2) );
        if( pszWorkLineTmp == NULL )
            break;

        pszWorkLine = pszWorkLineTmp;
        // The newline gets lost in CPLReadLine().
        strcat( pszWorkLine + nWorkLineLength, "\n" );
        strcat( pszWorkLine + nWorkLineLength, pszLine );

        nWorkLineLength += nLineLen + 1;
    }

    char **papszReturn = CSVSplitLine( pszWorkLine, chDelimiter );

    CPLFree( pszWorkLine );

    return papszReturn;
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
    CPLAssert( pszValue != NULL );
    CPLAssert( iKeyField >= 0 );
    CPLAssert( fp != NULL );

    bool bSelected = false;
    const int nTestValue = atoi(pszValue);
    char **papszFields = NULL;

    while( !bSelected ) {
        papszFields = CSVReadParseLine( fp );
        if( papszFields == NULL )
            return NULL;

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
            papszFields = NULL;
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
    CPLAssert( pszValue != NULL );
    CPLAssert( iKeyField >= 0 );
    CPLAssert( fp != NULL );

    bool bSelected = false;
    const int nTestValue = atoi(pszValue);
    char **papszFields = NULL;

    while( !bSelected ) {
        papszFields = CSVReadParseLineL( fp );
        if( papszFields == NULL )
            return NULL;

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
            papszFields = NULL;
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
    CPLAssert( psTable->panLineIndex != NULL );

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
        return NULL;

/* -------------------------------------------------------------------- */
/*      Parse target line, and update iLastLine indicator.              */
/* -------------------------------------------------------------------- */
    psTable->iLastLine = iResult;

    return CSVSplitLine( psTable->papszLines[iResult], ',' );
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
    CPLAssert( pszValue != NULL );
    CPLAssert( iKeyField >= 0 );

    const int nTestValue = atoi(pszValue);

/* -------------------------------------------------------------------- */
/*      Short cut for indexed files.                                    */
/* -------------------------------------------------------------------- */
    if( iKeyField == 0 && eCriteria == CC_Integer
        && psTable->panLineIndex != NULL )
        return CSVScanLinesIndexed( psTable, nTestValue );

/* -------------------------------------------------------------------- */
/*      Scan from in-core lines.                                        */
/* -------------------------------------------------------------------- */
    char **papszFields = NULL;
    bool bSelected = false;

    while( !bSelected && psTable->iLastLine+1 < psTable->nLineCount ) {
        psTable->iLastLine++;
        papszFields = CSVSplitLine( psTable->papszLines[psTable->iLastLine],
                                    ',' );

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
            papszFields = NULL;
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
    CPLAssert( pszFilename != NULL );

    CSVTable * const psTable = CSVAccess( pszFilename );
    if( psTable == NULL )
        return NULL;

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
        return NULL;

    psTable->iLastLine++;
    CSLDestroy( psTable->papszRecFields );
    psTable->papszRecFields =
        CSVSplitLine( psTable->papszLines[psTable->iLastLine], ',' );

    return psTable->papszRecFields;
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
/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( pszFilename != NULL );

    if( iKeyField < 0 )
        return NULL;

    CSVTable * const psTable = CSVAccess( pszFilename );
    if( psTable == NULL )
        return NULL;

    CSVIngest( pszFilename );

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

    if( psTable->pszRawData != NULL )
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
    CPLAssert( fp != NULL && pszFieldName != NULL );

    VSIRewind( fp );

    char **papszFields = CSVReadParseLine( fp );
    for( int i = 0; papszFields != NULL && papszFields[i] != NULL; i++ )
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
    CPLAssert( fp != NULL && pszFieldName != NULL );

    VSIRewindL( fp );

    char **papszFields = CSVReadParseLineL( fp );
    for( int i = 0; papszFields != NULL && papszFields[i] != NULL; i++ )
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

int CSVGetFileFieldId( const char * pszFilename, const char * pszFieldName )

{
/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( pszFilename != NULL );

    CSVTable * const psTable = CSVAccess( pszFilename );
    if( psTable == NULL )
        return -1;

/* -------------------------------------------------------------------- */
/*      Find the requested field.                                       */
/* -------------------------------------------------------------------- */
    for( int i = 0;
         psTable->papszFieldNames != NULL
             && psTable->papszFieldNames[i] != NULL;
         i++ )
    {
        if( EQUAL(psTable->papszFieldNames[i], pszFieldName) )
        {
            return i;
        }
    }

    return -1;
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
        return NULL;

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
    if( psTable == NULL )
        return "";

/* -------------------------------------------------------------------- */
/*      Find the correct record.                                        */
/* -------------------------------------------------------------------- */
    char **papszRecord = CSVScanFileByName( pszFilename, pszKeyFieldName,
                                            pszKeyFieldValue, eCriteria );
    if( papszRecord == NULL )
        return "";

/* -------------------------------------------------------------------- */
/*      Figure out which field we want out of this.                     */
/* -------------------------------------------------------------------- */
    const int iTargetField = CSVGetFileFieldId( pszFilename, pszTargetField );
    if( iTargetField < 0 )
        return "";

    for( int i=0; papszRecord[i] != NULL; ++i )
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
    if( ppsCSVTableList != NULL )
    {
        const size_t nBasenameLen = strlen(pszBasename);

        for( const CSVTable *psTable = *ppsCSVTableList;
             psTable != NULL;
             psTable = psTable->psNext )
        {
            const size_t nFullLen = strlen(psTable->pszFilename);

            if( nFullLen > nBasenameLen
                && strcmp(psTable->pszFilename+nFullLen-nBasenameLen,
                          pszBasename) == 0
                && strchr("/\\", psTable->pszFilename[+nFullLen-nBasenameLen-1])
                          != NULL )
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
    if( pTLSData == NULL && !bMemoryError )
    {
        pTLSData = static_cast<DefaultCSVFileNameTLS *>(
            VSI_CALLOC_VERBOSE( 1, sizeof(DefaultCSVFileNameTLS) ) );
        if( pTLSData )
            CPLSetTLS( CTLS_CSVDEFAULTFILENAME, pTLSData, TRUE );
    }
    if( pTLSData == NULL )
        return "/not_existing_dir/not_existing_path";

    const char *pszResult = CPLFindFile( "epsg_csv", pszBasename );

    if( pszResult != NULL )
        return pszResult;

    if( !pTLSData->bCSVFinderInitialized )
    {
        pTLSData->bCSVFinderInitialized = true;

        if( CPLGetConfigOption("GEOTIFF_CSV", NULL) != NULL )
            CPLPushFinderLocation( CPLGetConfigOption("GEOTIFF_CSV", NULL));

        if( CPLGetConfigOption("GDAL_DATA", NULL) != NULL )
            CPLPushFinderLocation( CPLGetConfigOption("GDAL_DATA", NULL) );

        pszResult = CPLFindFile( "epsg_csv", pszBasename );

        if( pszResult != NULL )
            return pszResult;
    }

#ifdef GDAL_NO_HARDCODED_FIND
    // For systems like sandboxes that do not allow other checks.
    CPLDebug( "CPL_CSV",
              "Failed to find file in GDALDefaultCSVFilename.  "
              "Returning original basename: %s",
              pszBasename );
    strcpy( pTLSData->szPath, pszBasename );
    return pTLSData->szPath;
#else

#ifdef GDAL_PREFIX
  #ifdef MACOSX_FRAMEWORK
    strcpy( pTLSData->szPath, GDAL_PREFIX "/Resources/epsg_csv/" );
    CPLStrlcat( pTLSData->szPath, pszBasename, sizeof(pTLSData->szPath) );
  #else
    strcpy( pTLSData->szPath, GDAL_PREFIX "/share/epsg_csv/" );
    CPLStrlcat( pTLSData->szPath, pszBasename, sizeof(pTLSData->szPath) );
  #endif
#else
    strcpy( pTLSData->szPath, "/usr/local/share/epsg_csv/" );
    CPLStrlcat( pTLSData->szPath, pszBasename, sizeof(pTLSData->szPath) );
#endif  // GDAL_PREFIX

    VSILFILE *fp = VSIFOpenL( pTLSData->szPath, "rt" );
    if( fp == NULL )
        CPLStrlcpy( pTLSData->szPath, pszBasename, sizeof(pTLSData->szPath) );

    if( fp != NULL )
        VSIFCloseL( fp );

    return pTLSData->szPath;
#endif  // GDAL_NO_HARDCODED_FIND
}

/************************************************************************/
/*                            CSVFilename()                             */
/*                                                                      */
/*      Return the full path to a particular CSV file.  This will       */
/*      eventually be something the application can override.           */
/************************************************************************/

CPL_C_START
static const char *(*pfnCSVFilenameHook)(const char *) = NULL;
CPL_C_END

const char * CSVFilename( const char *pszBasename )

{
    if( pfnCSVFilenameHook == NULL )
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

#ifdef WIN32
    sprintf( szPath, "%s\\%s", CSVDirName, pszInput );
#else
    sprintf( szPath, "%s/%s", CSVDirName, pszInput );
#endif

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
