/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  CSV (comma separated value) file access.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  2002/09/04 06:16:32  warmerda
 * added CPLReadLine(NULL) to cleanup
 *
 * Revision 1.3  2001/07/18 04:00:49  warmerda
 * added CPL_CVSID
 *
 * Revision 1.2  2001/01/19 21:16:41  warmerda
 * expanded tabs
 *
 * Revision 1.1  2000/10/06 15:20:45  warmerda
 * New
 *
 * Revision 1.2  2000/08/29 21:08:08  warmerda
 * fallback to use CPLFindFile()
 *
 * Revision 1.1  2000/04/05 21:55:59  warmerda
 * New
 *
 */

#include "cpl_csv.h"
#include "cpl_conv.h"

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
} CSVTable;

static CSVTable *psCSVTableList = NULL;

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
/*      Is the table already in the list.                               */
/* -------------------------------------------------------------------- */
    for( psTable = psCSVTableList; psTable != NULL; psTable = psTable->psNext )
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
    fp = VSIFOpen( pszFilename, "r" );
    if( fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create an information structure about this table, and add to    */
/*      the front of the list.                                          */
/* -------------------------------------------------------------------- */
    psTable = (CSVTable *) CPLCalloc(sizeof(CSVTable),1);

    psTable->fp = fp;
    psTable->pszFilename = CPLStrdup( pszFilename );
    psTable->psNext = psCSVTableList;
    
    psCSVTableList = psTable;

/* -------------------------------------------------------------------- */
/*      Read the table header record containing the field names.        */
/* -------------------------------------------------------------------- */
    psTable->papszFieldNames = CSVReadParseLine( fp );

    return( psTable );
}

/************************************************************************/
/*                            CSVDeaccess()                             */
/************************************************************************/

void CSVDeaccess( const char * pszFilename )

{
    CSVTable    *psLast, *psTable;
    
/* -------------------------------------------------------------------- */
/*      A NULL means deaccess all tables.                               */
/* -------------------------------------------------------------------- */
    if( pszFilename == NULL )
    {
        while( psCSVTableList != NULL )
            CSVDeaccess( psCSVTableList->pszFilename );
        
        return;
    }

/* -------------------------------------------------------------------- */
/*      Find this table.                                                */
/* -------------------------------------------------------------------- */
    psLast = NULL;
    for( psTable = psCSVTableList;
         psTable != NULL && !EQUAL(psTable->pszFilename,pszFilename);
         psTable = psTable->psNext )
    {
        psLast = psTable;
    }

    if( psTable == NULL )
    {
        CPLDebug( "CPL_CSV", "CPLDeaccess( %s ) - no match.", pszFilename );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Remove the link from the list.                                  */
/* -------------------------------------------------------------------- */
    if( psLast != NULL )
        psLast->psNext = psTable->psNext;
    else
        psCSVTableList = psTable->psNext;

/* -------------------------------------------------------------------- */
/*      Free the table.                                                 */
/* -------------------------------------------------------------------- */
    VSIFClose( psTable->fp );

    CSLDestroy( psTable->papszFieldNames );
    CSLDestroy( psTable->papszRecFields );
    CPLFree( psTable->pszFilename );

    CPLFree( psTable );

    CPLReadLine( NULL );
}

/************************************************************************/
/*                          CSVReadParseLine()                          */
/*                                                                      */
/*      Read one line, and return split into fields.  The return        */
/*      result is a stringlist, in the sense of the CSL functions.      */
/************************************************************************/

char **CSVReadParseLine( FILE * fp )

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
        return CSLTokenizeStringComplex( pszLine, ",", TRUE, TRUE );

/* -------------------------------------------------------------------- */
/*      We must now count the quotes in our working string, and as      */
/*      long as it is odd, keep adding new lines.                       */
/* -------------------------------------------------------------------- */
    pszWorkLine = CPLStrdup( pszLine );

    while( TRUE )
    {
        int             i, nCount = 0;

        for( i = 0; pszWorkLine[i] != '\0'; i++ )
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

        pszWorkLine = (char *)
            CPLRealloc(pszWorkLine,
                       strlen(pszWorkLine) + strlen(pszLine) + 1);
        strcat( pszWorkLine, pszLine );
    }
    
    papszReturn = CSLTokenizeStringComplex( pszWorkLine, ",", TRUE, TRUE );

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

/* -------------------------------------------------------------------- */
/*      Does the current record match the criteria?  If so, just        */
/*      return it again.                                                */
/* -------------------------------------------------------------------- */
    if( iKeyField >= 0
        && iKeyField < CSLCount(psTable->papszRecFields)
        && CSVCompare(pszValue,psTable->papszRecFields[iKeyField],eCriteria) )
    {
        return psTable->papszRecFields;
    }

/* -------------------------------------------------------------------- */
/*      Scan the file from the beginning, replacing the ``current       */
/*      record'' in our structure with the one that is found.           */
/* -------------------------------------------------------------------- */
    VSIRewind( psTable->fp );
    CPLReadLine( psTable->fp );         /* throw away the header line */
    
    CSLDestroy( psTable->papszRecFields );
    psTable->papszRecFields =
        CSVScanLines( psTable->fp, iKeyField, pszValue, eCriteria );

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
/*                            CSVFilename()                             */
/*                                                                      */
/*      Return the full path to a particular CSV file.  This will       */
/*      eventually be something the application can override.           */
/************************************************************************/

static const char *(*pfnCSVFilenameHook)(const char *) = NULL;

const char * CSVFilename( const char *pszBasename )

{
    static char         szPath[512];

    if( pfnCSVFilenameHook == NULL )
    {
        FILE    *fp = NULL;
        const char *pszResult = CPLFindFile( "epsg_csv", pszBasename );

        if( pszResult != NULL )
            return pszResult;

        if( getenv("GEOTIFF_CSV") != NULL )
        {
            sprintf( szPath, "%s/%s", getenv("GEOTIFF_CSV"), pszBasename );
        }
        else if( (fp = fopen( "csv/horiz_cs.csv", "rt" )) != NULL )
        {
            sprintf( szPath, "csv/%s", pszBasename );
        }
        else
        {
            sprintf( szPath, "/usr/local/share/epsg_csv/%s", pszBasename );
        }

        if( fp != NULL )
            fclose( fp );
        
        return( szPath );
    }
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
 * @param CSVFileOverride The pointer to a function which will return the
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

void SetCSVFilenameHook( const char *(*pfnNewHook)( const char * ) )

{
    pfnCSVFilenameHook = pfnNewHook;
}
