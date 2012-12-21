/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CSV (comma separated value) file access.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "cpl_csv.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gdal_csv.h"

CPL_CVSID("$Id$");

/* ==================================================================== */
/*      The CSVTable is a persistant set of info about an open CSV      */
/*      table.  While it doesn't currently maintain a record index,     */
/*      or in-memory copy of the table, it could be changed to do so    */
/*      in the future.                                                  */
/* ==================================================================== */
typedef struct ctb {
    FILE        *fp;

    struct ctb *psNext;

    char        *pszFilename;

    char        **papszFieldNames;

    char        **papszRecFields;

    int         iLastLine;

    int         bNonUniqueKey;

    /* Cache for whole file */
    int         nLineCount;
    char        **papszLines;
    int         *panLineIndex;
    char        *pszRawData;
} CSVTable;


static void CSVDeaccessInternal( CSVTable **ppsCSVTableList, int bCanUseTLS, const char * pszFilename );

/************************************************************************/
/*                            CSVFreeTLS()                              */
/************************************************************************/
static void CSVFreeTLS(void* pData)
{
    CSVDeaccessInternal( (CSVTable **)pData, FALSE, NULL );
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
    CSVTable    *psTable;
    FILE        *fp;

/* -------------------------------------------------------------------- */
/*      Fetch the table, and allocate the thread-local pointer to it    */
/*      if there isn't already one.                                     */
/* -------------------------------------------------------------------- */
    CSVTable **ppsCSVTableList;

    ppsCSVTableList = (CSVTable **) CPLGetTLS( CTLS_CSVTABLEPTR );
    if( ppsCSVTableList == NULL )
    {
        ppsCSVTableList = (CSVTable **) CPLCalloc(1,sizeof(CSVTable*));
        CPLSetTLSWithFreeFunc( CTLS_CSVTABLEPTR, ppsCSVTableList, CSVFreeTLS );
    }

/* -------------------------------------------------------------------- */
/*      Is the table already in the list.                               */
/* -------------------------------------------------------------------- */
    for( psTable = *ppsCSVTableList; 
         psTable != NULL; 
         psTable = psTable->psNext )
    {
        if( EQUAL(psTable->pszFilename,pszFilename) )
        {
            /*
             * Eventually we should consider promoting to the front of
             * the list to accelerate frequently accessed tables.
             */

            return( psTable );
        }
    }

/* -------------------------------------------------------------------- */
/*      If not, try to open it.                                         */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "rb" );
    if( fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create an information structure about this table, and add to    */
/*      the front of the list.                                          */
/* -------------------------------------------------------------------- */
    psTable = (CSVTable *) CPLCalloc(sizeof(CSVTable),1);

    psTable->fp = fp;
    psTable->pszFilename = CPLStrdup( pszFilename );
    psTable->bNonUniqueKey = FALSE; /* as far as we know now */
    psTable->psNext = *ppsCSVTableList;
    
    *ppsCSVTableList = psTable;

/* -------------------------------------------------------------------- */
/*      Read the table header record containing the field names.        */
/* -------------------------------------------------------------------- */
    psTable->papszFieldNames = CSVReadParseLine( fp );

    return( psTable );
}

/************************************************************************/
/*                            CSVDeaccess()                             */
/************************************************************************/

static void CSVDeaccessInternal( CSVTable **ppsCSVTableList, int bCanUseTLS, const char * pszFilename )

{
    CSVTable    *psLast, *psTable;
    
    if( ppsCSVTableList == NULL )
        return;
    
/* -------------------------------------------------------------------- */
/*      A NULL means deaccess all tables.                               */
/* -------------------------------------------------------------------- */
    if( pszFilename == NULL )
    {
        while( *ppsCSVTableList != NULL )
            CSVDeaccessInternal( ppsCSVTableList, bCanUseTLS, (*ppsCSVTableList)->pszFilename );
        
        return;
    }

/* -------------------------------------------------------------------- */
/*      Find this table.                                                */
/* -------------------------------------------------------------------- */
    psLast = NULL;
    for( psTable = *ppsCSVTableList;
         psTable != NULL && !EQUAL(psTable->pszFilename,pszFilename);
         psTable = psTable->psNext )
    {
        psLast = psTable;
    }

    if( psTable == NULL )
    {
        if (bCanUseTLS)
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
        VSIFClose( psTable->fp );

    CSLDestroy( psTable->papszFieldNames );
    CSLDestroy( psTable->papszRecFields );
    CPLFree( psTable->pszFilename );
    CPLFree( psTable->panLineIndex );
    CPLFree( psTable->pszRawData );
    CPLFree( psTable->papszLines );

    CPLFree( psTable );

    if (bCanUseTLS)
        CPLReadLine( NULL );
}

void CSVDeaccess( const char * pszFilename )
{
    CSVTable **ppsCSVTableList;
/* -------------------------------------------------------------------- */
/*      Fetch the table, and allocate the thread-local pointer to it    */
/*      if there isn't already one.                                     */
/* -------------------------------------------------------------------- */
    ppsCSVTableList = (CSVTable **) CPLGetTLS( CTLS_CSVTABLEPTR );

    CSVDeaccessInternal(ppsCSVTableList, TRUE, pszFilename);
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
                pszToken = (char *) CPLRealloc( pszToken, nTokenMax );
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        pszToken[nTokenLen] = '\0';
        papszRetList = CSLAddString( papszRetList, pszToken );

        /* If the last token is an empty token, then we have to catch
         * it now, otherwise we won't reenter the loop and it will be lost. 
         */
        if ( *pszString == '\0' && *(pszString-1) == chDelimiter )
        {
            papszRetList = CSLAddString( papszRetList, "" );
        }
    }

    if( papszRetList == NULL )
        papszRetList = (char **) CPLCalloc(sizeof(char *),1);

    CPLFree( pszToken );

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
    int  nQuoteCount = 0, i;

    for( i = 0; pszThisLine[i] != '\0'; i++ )
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
    else
        return pszThisLine + i;
}

/************************************************************************/
/*                             CSVIngest()                              */
/*                                                                      */
/*      Load entire file into memory and setup index if possible.       */
/************************************************************************/

static void CSVIngest( const char *pszFilename )

{
    CSVTable *psTable = CSVAccess( pszFilename );
    int       nFileLen, i, nMaxLineCount, iLine = 0;
    char *pszThisLine;

    if( psTable->pszRawData != NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Ingest whole file.                                              */
/* -------------------------------------------------------------------- */
    VSIFSeek( psTable->fp, 0, SEEK_END );
    nFileLen = VSIFTell( psTable->fp );
    VSIRewind( psTable->fp );

    psTable->pszRawData = (char *) CPLMalloc(nFileLen+1);
    if( (int) VSIFRead( psTable->pszRawData, 1, nFileLen, psTable->fp ) 
        != nFileLen )
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
    nMaxLineCount = 0;
    for( i = 0; i < nFileLen; i++ )
    {
        if( psTable->pszRawData[i] == 10 )
            nMaxLineCount++;
    }

    psTable->papszLines = (char **) CPLCalloc(sizeof(char*),nMaxLineCount);
    
/* -------------------------------------------------------------------- */
/*      Build a list of record pointers into the raw data buffer        */
/*      based on line terminators.  Zero terminate the line             */
/*      strings.                                                        */
/* -------------------------------------------------------------------- */
    /* skip header line */
    pszThisLine = CSVFindNextLine( psTable->pszRawData );

    while( pszThisLine != NULL && iLine < nMaxLineCount )
    {
        psTable->papszLines[iLine++] = pszThisLine;
        pszThisLine = CSVFindNextLine( pszThisLine );
    }

    psTable->nLineCount = iLine;

/* -------------------------------------------------------------------- */
/*      Allocate and populate index array.  Ensure they are in          */
/*      ascending order so that binary searches can be done on the      */
/*      array.                                                          */
/* -------------------------------------------------------------------- */
    psTable->panLineIndex = (int *) CPLMalloc(sizeof(int)*psTable->nLineCount);
    for( i = 0; i < psTable->nLineCount; i++ )
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
    VSIFClose( psTable->fp );
    psTable->fp = NULL;
}

/************************************************************************/
/*                        CSVDetectSeperator()                          */
/************************************************************************/

/** Detect which field separator is used.
 *
 * Currently, it can detect comma, semicolon or tabulation. In case of
 * ambiguity or no separator found, comma will be considered as the separator.
 *
 * @return ',', ';' or '\t'
 */
char CSVDetectSeperator (const char* pszLine)
{
    int     bInString = FALSE;
    char    chDelimiter = '\0';

    for( ; *pszLine != '\0'; pszLine++ )
    {
        if( !bInString && (*pszLine == ',' || *pszLine == ';' || *pszLine == '\t'))
        {
            if (chDelimiter == '\0')
                chDelimiter = *pszLine;
            else if (chDelimiter != *pszLine)
            {
                /* The separator is not consistant on the line. */
                CPLDebug("CSV", "Inconsistant separator. '%c' and '%c' found. Using ',' as default",
                         chDelimiter, *pszLine);
                chDelimiter = ',';
                break;
            }
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

    if (chDelimiter == '\0')
        chDelimiter = ',';

    return chDelimiter;
}

/************************************************************************/
/*                          CSVReadParseLine()                          */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/************************************************************************/

char **CSVReadParseLine( FILE * fp )
{
    return CSVReadParseLine2(fp, ',');
}

char **CSVReadParseLine2( FILE * fp, char chDelimiter )

{
    const char  *pszLine;
    char        *pszWorkLine;
    char        **papszReturn;

    CPLAssert( fp != NULL );
    if( fp == NULL )
        return( NULL );
    
    pszLine = CPLReadLine( fp );
    if( pszLine == NULL )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      If there are no quotes, then this is the simple case.           */
/*      Parse, and return tokens.                                       */
/* -------------------------------------------------------------------- */
    if( strchr(pszLine,'\"') == NULL )
        return CSVSplitLine( pszLine, chDelimiter );

/* -------------------------------------------------------------------- */
/*      We must now count the quotes in our working string, and as      */
/*      long as it is odd, keep adding new lines.                       */
/* -------------------------------------------------------------------- */
    pszWorkLine = CPLStrdup( pszLine );

    int i = 0, nCount = 0;
    int nWorkLineLength = strlen(pszWorkLine);

    while( TRUE )
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

        int nLineLen = strlen(pszLine);

        char* pszWorkLineTmp = (char *)
            VSIRealloc(pszWorkLine,
                       nWorkLineLength + nLineLen + 2);
        if (pszWorkLineTmp == NULL)
            break;
        pszWorkLine = pszWorkLineTmp;
        strcat( pszWorkLine + nWorkLineLength, "\n" ); // This gets lost in CPLReadLine().
        strcat( pszWorkLine + nWorkLineLength, pszLine );

        nWorkLineLength += nLineLen + 1;
    }
    
    papszReturn = CSVSplitLine( pszWorkLine, chDelimiter );

    CPLFree( pszWorkLine );

    return papszReturn;
}

/************************************************************************/
/*                             CSVCompare()                             */
/*                                                                      */
/*      Compare a field to a search value using a particular            */
/*      criteria.                                                       */
/************************************************************************/

static int CSVCompare( const char * pszFieldValue, const char * pszTarget,
                       CSVCompareCriteria eCriteria )

{
    if( eCriteria == CC_ExactString )
    {
        return( strcmp( pszFieldValue, pszTarget ) == 0 );
    }
    else if( eCriteria == CC_ApproxString )
    {
        return( EQUAL( pszFieldValue, pszTarget ) );
    }
    else if( eCriteria == CC_Integer )
    {
        return( atoi(pszFieldValue) == atoi(pszTarget) );
    }

    return FALSE;
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
    char        **papszFields = NULL;
    int         bSelected = FALSE, nTestValue;

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
        else if( eCriteria == CC_Integer
                 && atoi(papszFields[iKeyField]) == nTestValue )
        {
            bSelected = TRUE;
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
    
    return( papszFields );
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
    int         iTop, iBottom, iMiddle, iResult = -1;

    CPLAssert( psTable->panLineIndex != NULL );

/* -------------------------------------------------------------------- */
/*      Find target record with binary search.                          */
/* -------------------------------------------------------------------- */
    iTop = psTable->nLineCount-1;
    iBottom = 0;

    while( iTop >= iBottom )
    {
        iMiddle = (iTop + iBottom) / 2;
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
                psTable->bNonUniqueKey = TRUE; 
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
    char        **papszFields = NULL;
    int         bSelected = FALSE, nTestValue;

    CPLAssert( pszValue != NULL );
    CPLAssert( iKeyField >= 0 );

    nTestValue = atoi(pszValue);
    
/* -------------------------------------------------------------------- */
/*      Short cut for indexed files.                                    */
/* -------------------------------------------------------------------- */
    if( iKeyField == 0 && eCriteria == CC_Integer 
        && psTable->panLineIndex != NULL )
        return CSVScanLinesIndexed( psTable, nTestValue );
    
/* -------------------------------------------------------------------- */
/*      Scan from in-core lines.                                        */
/* -------------------------------------------------------------------- */
    while( !bSelected && psTable->iLastLine+1 < psTable->nLineCount ) {
        psTable->iLastLine++;
        papszFields = CSVSplitLine( psTable->papszLines[psTable->iLastLine], ',' );

        if( CSLCount( papszFields ) < iKeyField+1 )
        {
            /* not selected */
        }
        else if( eCriteria == CC_Integer
                 && atoi(papszFields[iKeyField]) == nTestValue )
        {
            bSelected = TRUE;
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
    
    return( papszFields );
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
    CSVTable *psTable;

/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( pszFilename != NULL );

    psTable = CSVAccess( pszFilename );
    if( psTable == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      If we use CSVGetNextLine() we can pretty much assume we have    */
/*      a non-unique key.                                               */
/* -------------------------------------------------------------------- */
    psTable->bNonUniqueKey = TRUE; 

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
    CSVTable    *psTable;

/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( pszFilename != NULL );

    if( iKeyField < 0 )
        return NULL;

    psTable = CSVAccess( pszFilename );
    if( psTable == NULL )
        return NULL;
    
    CSVIngest( pszFilename );

/* -------------------------------------------------------------------- */
/*      Does the current record match the criteria?  If so, just        */
/*      return it again.                                                */
/* -------------------------------------------------------------------- */
    if( iKeyField >= 0
        && iKeyField < CSLCount(psTable->papszRecFields)
        && CSVCompare(pszValue,psTable->papszRecFields[iKeyField],eCriteria)
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
        VSIRewind( psTable->fp );
        CPLReadLine( psTable->fp );         /* throw away the header line */
    
        psTable->papszRecFields =
            CSVScanLines( psTable->fp, iKeyField, pszValue, eCriteria );
    }

    return( psTable->papszRecFields );
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
/************************************************************************/

int CSVGetFieldId( FILE * fp, const char * pszFieldName )

{
    char        **papszFields;
    int         i;
    
    CPLAssert( fp != NULL && pszFieldName != NULL );

    VSIRewind( fp );

    papszFields = CSVReadParseLine( fp );
    for( i = 0; papszFields != NULL && papszFields[i] != NULL; i++ )
    {
        if( EQUAL(papszFields[i],pszFieldName) )
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
    CSVTable    *psTable;
    int         i;
    
/* -------------------------------------------------------------------- */
/*      Get access to the table.                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( pszFilename != NULL );

    psTable = CSVAccess( pszFilename );
    if( psTable == NULL )
        return -1;

/* -------------------------------------------------------------------- */
/*      Find the requested field.                                       */
/* -------------------------------------------------------------------- */
    for( i = 0;
         psTable->papszFieldNames != NULL
             && psTable->papszFieldNames[i] != NULL;
         i++ )
    {
        if( EQUAL(psTable->papszFieldNames[i],pszFieldName) )
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
    int         iKeyField;

    iKeyField = CSVGetFileFieldId( pszFilename, pszKeyFieldName );
    if( iKeyField == -1 )
        return NULL;

    return( CSVScanFile( pszFilename, iKeyField, pszValue, eCriteria ) );
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
    CSVTable    *psTable;
    char        **papszRecord;
    int         iTargetField;
    
/* -------------------------------------------------------------------- */
/*      Find the table.                                                 */
/* -------------------------------------------------------------------- */
    psTable = CSVAccess( pszFilename );
    if( psTable == NULL )
        return "";

/* -------------------------------------------------------------------- */
/*      Find the correct record.                                        */
/* -------------------------------------------------------------------- */
    papszRecord = CSVScanFileByName( pszFilename, pszKeyFieldName,
                                     pszKeyFieldValue, eCriteria );

    if( papszRecord == NULL )
        return "";

/* -------------------------------------------------------------------- */
/*      Figure out which field we want out of this.                     */
/* -------------------------------------------------------------------- */
    iTargetField = CSVGetFileFieldId( pszFilename, pszTargetField );
    if( iTargetField < 0 )
        return "";

    if( iTargetField >= CSLCount( papszRecord ) )
        return "";

    return( papszRecord[iTargetField] );
}

/************************************************************************/
/*                       GDALDefaultCSVFilename()                       */
/************************************************************************/

typedef struct
{
    char szPath[512];
    int  bCSVFinderInitialized;
} DefaultCSVFileNameTLS;


const char * GDALDefaultCSVFilename( const char *pszBasename )

{
/* -------------------------------------------------------------------- */
/*      Do we already have this file accessed?  If so, just return      */
/*      the existing path without any further probing.                  */
/* -------------------------------------------------------------------- */
    CSVTable **ppsCSVTableList;

    ppsCSVTableList = (CSVTable **) CPLGetTLS( CTLS_CSVTABLEPTR );
    if( ppsCSVTableList != NULL )
    {
        CSVTable *psTable;
        int nBasenameLen = strlen(pszBasename);

        for( psTable = *ppsCSVTableList; 
             psTable != NULL; 
             psTable = psTable->psNext )
        {
            int nFullLen = strlen(psTable->pszFilename);

            if( nFullLen > nBasenameLen 
                && strcmp(psTable->pszFilename+nFullLen-nBasenameLen,
                          pszBasename) == 0 
                && strchr("/\\",psTable->pszFilename[+nFullLen-nBasenameLen-1])
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
            (DefaultCSVFileNameTLS *) CPLGetTLS( CTLS_CSVDEFAULTFILENAME );
    if (pTLSData == NULL)
    {
        pTLSData = (DefaultCSVFileNameTLS*) CPLCalloc(1, sizeof(DefaultCSVFileNameTLS));
        CPLSetTLS( CTLS_CSVDEFAULTFILENAME, pTLSData, TRUE );
    }

    FILE    *fp = NULL;
    const char *pszResult;

    pszResult = CPLFindFile( "epsg_csv", pszBasename );

    if( pszResult != NULL )
        return pszResult;

    if( !pTLSData->bCSVFinderInitialized )
    {
        pTLSData->bCSVFinderInitialized = TRUE;

        if( CPLGetConfigOption("GEOTIFF_CSV",NULL) != NULL )
            CPLPushFinderLocation( CPLGetConfigOption("GEOTIFF_CSV",NULL));
            
        if( CPLGetConfigOption("GDAL_DATA",NULL) != NULL )
            CPLPushFinderLocation( CPLGetConfigOption("GDAL_DATA",NULL) );

        pszResult = CPLFindFile( "epsg_csv", pszBasename );

        if( pszResult != NULL )
            return pszResult;
    }
            
    if( (fp = fopen( "csv/horiz_cs.csv", "rt" )) != NULL )
    {
        strcpy( pTLSData->szPath, "csv/" );
        CPLStrlcat( pTLSData->szPath, pszBasename, sizeof(pTLSData->szPath) );
    }
    else
    {
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
#endif
        if( (fp = fopen( pTLSData->szPath, "rt" )) == NULL )
            CPLStrlcpy( pTLSData->szPath, pszBasename, sizeof(pTLSData->szPath) );
    }

    if( fp != NULL )
        fclose( fp );
        
    return( pTLSData->szPath );
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
    else
        return( pfnCSVFilenameHook( pszBasename ) );
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

This function allows an application to override how the GTIFGetDefn() and related function find the CSV (Comma Separated
Value) values required. The pfnHook argument should be a pointer to a function that will take in a CSV filename and return a
full path to the file. The returned string should be to an internal static buffer so that the caller doesn't have to free the result.

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
    static char         szPath[1024];

#ifdef WIN32
    sprintf( szPath, "%s\\%s", CSVDirName, pszInput );
#else    
    sprintf( szPath, "%s/%s", CSVDirName, pszInput );
#endif    

    return( szPath );
}
</pre>

*/

CPL_C_START
void SetCSVFilenameHook( const char *(*pfnNewHook)( const char * ) )

{
    pfnCSVFilenameHook = pfnNewHook;
}
CPL_C_END
