/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57ClassRegistrar class for keeping track of
 *           information on S57 object classes.
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
 * Revision 1.8  2003/09/12 21:18:54  warmerda
 * open csv files in binary mode, not text mode
 *
 * Revision 1.7  2001/12/17 22:38:42  warmerda
 * restructure LoadInfo() to support in-code tables
 *
 * Revision 1.6  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.5  2000/12/21 21:58:10  warmerda
 * Append class numbers to list rather than inserting at beginning, to
 * preserve original order.
 *
 * Revision 1.4  2000/08/30 09:14:08  warmerda
 * added use of CPLFindFile
 *
 * Revision 1.3  2000/06/07 20:50:58  warmerda
 * make CSV location configurable with env variable
 *
 * Revision 1.2  1999/11/18 19:01:25  warmerda
 * expanded tabs
 *
 * Revision 1.1  1999/11/08 22:22:55  warmerda
 * New
 *
 */

#include "s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");


#ifdef S57_BUILTIN_CLASSES
#include "s57tables.h"
#endif

/************************************************************************/
/*                         S57ClassRegistrar()                          */
/************************************************************************/

S57ClassRegistrar::S57ClassRegistrar()

{
    nClasses = 0;
    papszClassesInfo = NULL;
    
    iCurrentClass = -1;

    papszCurrentFields = NULL;
    papszTempResult = NULL;
    papszNextLine = NULL;
}

/************************************************************************/
/*                         ~S57ClassRegistrar()                         */
/************************************************************************/

S57ClassRegistrar::~S57ClassRegistrar()

{
    CSLDestroy( papszClassesInfo );
    CSLDestroy( papszCurrentFields );
    CSLDestroy( papszTempResult );
}

/************************************************************************/
/*                              FindFile()                              */
/************************************************************************/

int S57ClassRegistrar::FindFile( const char *pszTarget, 
                                 const char *pszDirectory, 
                                 int bReportErr,
                                 FILE **pfp )

{
    const char *pszFilename;
    
    if( pszDirectory == NULL )
    {
        pszFilename = CPLFindFile( "s57", pszTarget );
        if( pszFilename == NULL )
            pszFilename = pszTarget;
    }
    else
    {
        pszFilename = CPLFormFilename( pszDirectory, pszTarget, NULL );
    }

    *pfp = VSIFOpen( pszFilename, "rb" );

#ifdef S57_BUILTIN_CLASSES
    if( *pfp == NULL )
    {
        if( EQUAL(pszTarget, "s57objectclasses.csv") )
            papszNextLine = gpapszS57Classes;
        else
            papszNextLine = gpapszS57attributes;
    }
#else
    if( *pfp == NULL )
    {
        if( bReportErr )
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to open %s.\n",
                      pszFilename );
        return FALSE;
    }
#endif

    return TRUE;
}

/************************************************************************/
/*                              ReadLine()                              */
/*                                                                      */
/*      Read a line from the provided file, or from the "built-in"      */
/*      configuration file line list if the file is NULL.               */
/************************************************************************/

const char *S57ClassRegistrar::ReadLine( FILE * fp )

{
    if( fp != NULL )
        return CPLReadLine( fp );

    if( papszNextLine == NULL )
        return NULL;

    if( *papszNextLine == NULL )
    {
        papszNextLine = NULL;
        return NULL;
    }
    else
        return *(papszNextLine++);
}

/************************************************************************/
/*                              LoadInfo()                              */
/************************************************************************/

int S57ClassRegistrar::LoadInfo( const char * pszDirectory, 
                                 int bReportErr )

{
    FILE        *fp;

    if( pszDirectory == NULL && getenv( "S57_CSV" ) != NULL )
        pszDirectory = getenv( "S57_CSV" );

/* ==================================================================== */
/*      Read the s57objectclasses file.                                 */
/* ==================================================================== */
    if( !FindFile( "s57objectclasses.csv", pszDirectory, bReportErr, &fp ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Skip the line defining the column titles.                       */
/* -------------------------------------------------------------------- */
    const char * pszLine = ReadLine( fp );

    if( !EQUAL(pszLine,
               "\"Code\",\"ObjectClass\",\"Acronym\",\"Attribute_A\","
               "\"Attribute_B\",\"Attribute_C\",\"Class\",\"Primitives\"" ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "s57objectclasses columns don't match expected format!\n" );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read and form string list.                                      */
/* -------------------------------------------------------------------- */
    
    CSLDestroy( papszClassesInfo );
    papszClassesInfo = (char **) CPLCalloc(sizeof(char *),MAX_CLASSES);

    nClasses = 0;

    while( nClasses < MAX_CLASSES
           && (pszLine = ReadLine(fp)) != NULL )
    {
        papszClassesInfo[nClasses] = CPLStrdup(pszLine);
        if( papszClassesInfo[nClasses] == NULL )
            break;

        nClasses++;
    }

    if( nClasses == MAX_CLASSES )
        CPLError( CE_Warning, CPLE_AppDefined,
                  "MAX_CLASSES exceeded in S57ClassRegistrar::LoadInfo().\n" );

/* -------------------------------------------------------------------- */
/*      Cleanup, and establish state.                                   */
/* -------------------------------------------------------------------- */
    if( fp != NULL )
        VSIFClose( fp );
    iCurrentClass = -1;

    if( nClasses == 0 )
        return FALSE;

/* ==================================================================== */
/*      Read the attributes list.                                       */
/* ==================================================================== */
    if( !FindFile( "s57attributes.csv", pszDirectory, bReportErr, &fp ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Skip the line defining the column titles.                       */
/* -------------------------------------------------------------------- */
    pszLine = ReadLine( fp );

    if( !EQUAL(pszLine,
          "\"Code\",\"Attribute\",\"Acronym\",\"Attributetype\",\"Class\"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "s57attributes columns don't match expected format!\n" );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Prepare arrays for the per-attribute information.               */
/* -------------------------------------------------------------------- */
    nAttrMax = MAX_ATTRIBUTES-1;
    papszAttrNames = (char **) CPLCalloc(sizeof(char *),nAttrMax);
    papszAttrAcronym = (char **) CPLCalloc(sizeof(char *),nAttrMax);
    papapszAttrValues = (char ***) CPLCalloc(sizeof(char **),nAttrMax);
    pachAttrType = (char *) CPLCalloc(sizeof(char),nAttrMax);
    pachAttrClass = (char *) CPLCalloc(sizeof(char),nAttrMax);
    panAttrIndex = (int *) CPLCalloc(sizeof(int),nAttrMax);
    
/* -------------------------------------------------------------------- */
/*      Read and form string list.                                      */
/* -------------------------------------------------------------------- */
    int         iAttr;
    
    while( (pszLine = ReadLine(fp)) != NULL )
    {
        char    **papszTokens = CSLTokenizeStringComplex( pszLine, ",",
                                                          TRUE, TRUE );

        if( CSLCount(papszTokens) < 5 )
        {
            CPLAssert( FALSE );
            continue;
        }
        
        iAttr = atoi(papszTokens[0]);
        if( iAttr < 0 || iAttr >= nAttrMax
            || papszAttrNames[iAttr] != NULL )
        {
            CPLAssert( FALSE );
            continue;
        }
        
        papszAttrNames[iAttr] = CPLStrdup(papszTokens[1]);
        papszAttrAcronym[iAttr] = CPLStrdup(papszTokens[2]);
        pachAttrType[iAttr] = papszTokens[3][0];
        pachAttrClass[iAttr] = papszTokens[4][0];

        CSLDestroy( papszTokens );
    }

    if( fp != NULL )
        VSIFClose( fp );
    
/* -------------------------------------------------------------------- */
/*      Build unsorted index of attributes.                             */
/* -------------------------------------------------------------------- */
    nAttrCount = 0;
    for( iAttr = 0; iAttr < nAttrMax; iAttr++ )
    {
        if( papszAttrAcronym[iAttr] != NULL )
            panAttrIndex[nAttrCount++] = iAttr;
    }

/* -------------------------------------------------------------------- */
/*      Sort index by acronym.                                          */
/* -------------------------------------------------------------------- */
    int         bModified;

    do
    {
        bModified = FALSE;
        for( iAttr = 0; iAttr < nAttrCount-1; iAttr++ )
        {
            if( strcmp(papszAttrAcronym[panAttrIndex[iAttr]],
                       papszAttrAcronym[panAttrIndex[iAttr+1]]) > 0 )
            {
                int     nTemp;

                nTemp = panAttrIndex[iAttr];
                panAttrIndex[iAttr] = panAttrIndex[iAttr+1];
                panAttrIndex[iAttr+1] = nTemp;

                bModified = TRUE;
            }
        }
    } while( bModified );
    
    return TRUE;
}

/************************************************************************/
/*                         SelectClassByIndex()                         */
/************************************************************************/

int S57ClassRegistrar::SelectClassByIndex( int nNewIndex )

{
    if( nNewIndex < 0 || nNewIndex >= nClasses )
        return FALSE;

    CSLDestroy( papszCurrentFields );
    papszCurrentFields = CSLTokenizeStringComplex( papszClassesInfo[nNewIndex],
                                                   ",", TRUE, TRUE );

    iCurrentClass = nNewIndex;

    return TRUE;
}

/************************************************************************/
/*                             SelectClass()                            */
/************************************************************************/

int S57ClassRegistrar::SelectClass( int nOBJL )

{
    for( int i = 0; i < nClasses; i++ )
    {
        if( atoi(papszClassesInfo[i]) == nOBJL )
            return SelectClassByIndex( i );
    }

    return FALSE;
}

/************************************************************************/
/*                            SelectClass()                             */
/************************************************************************/

int S57ClassRegistrar::SelectClass( const char *pszAcronym )

{
    for( int i = 0; i < nClasses; i++ )
    {
        if( !SelectClassByIndex( i ) )
            continue;

        if( EQUAL(GetAcronym(),pszAcronym) )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                              GetOBJL()                               */
/************************************************************************/

int S57ClassRegistrar::GetOBJL()

{
    if( iCurrentClass >= 0 )
        return atoi(papszClassesInfo[iCurrentClass]);
    else
        return -1;
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

const char * S57ClassRegistrar::GetDescription()

{
    if( iCurrentClass >= 0
        && CSLCount(papszCurrentFields) > 1 )
        return papszCurrentFields[1];
    else
        return NULL;
}

/************************************************************************/
/*                             GetAcronym()                             */
/************************************************************************/

const char * S57ClassRegistrar::GetAcronym()

{
    if( iCurrentClass >= 0
        && CSLCount(papszCurrentFields) > 2 )
        return papszCurrentFields[2];
    else
        return NULL;
}

/************************************************************************/
/*                          GetAttributeList()                          */
/*                                                                      */
/*      The passed string can be "a", "b", "c" or NULL for all.  The    */
/*      returned list remained owned by this object, not the caller.    */
/************************************************************************/

char **S57ClassRegistrar::GetAttributeList( const char * pszType )

{
    if( iCurrentClass < 0 )
        return NULL;
    
    CSLDestroy( papszTempResult );
    papszTempResult = NULL;
    
    for( int iColumn = 3; iColumn < 6; iColumn++ )
    {
        if( pszType != NULL && iColumn == 3 && !EQUAL(pszType,"a") )
            continue;
        
        if( pszType != NULL && iColumn == 4 && !EQUAL(pszType,"b") )
            continue;
        
        if( pszType != NULL && iColumn == 5 && !EQUAL(pszType,"c") )
            continue;

        char    **papszTokens;

        papszTokens =
            CSLTokenizeStringComplex( papszCurrentFields[iColumn], ";",
                                      TRUE, FALSE );

        papszTempResult = CSLInsertStrings( papszTempResult, -1,
                                            papszTokens );

        CSLDestroy( papszTokens );
    }

    return papszTempResult;
}

/************************************************************************/
/*                            GetClassCode()                            */
/************************************************************************/

char S57ClassRegistrar::GetClassCode()

{
    if( iCurrentClass >= 0
        && CSLCount(papszCurrentFields) > 6 )
        return papszCurrentFields[6][0];
    else
        return '\0';
}

/************************************************************************/
/*                           GetPrimitives()                            */
/************************************************************************/

char **S57ClassRegistrar::GetPrimitives()

{
    if( iCurrentClass >= 0
        && CSLCount(papszCurrentFields) > 7 )
    {
        CSLDestroy( papszTempResult );
        papszTempResult = 
            CSLTokenizeStringComplex( papszCurrentFields[7], ";",
                                      TRUE, FALSE );
        return papszTempResult;
    }
    else
        return NULL;
}

/************************************************************************/
/*                         FindAttrByAcronym()                          */
/************************************************************************/

int     S57ClassRegistrar::FindAttrByAcronym( const char * pszName )

{
    int         iStart, iEnd, iCandidate;

    iStart = 0;
    iEnd = nAttrCount-1;

    while( iStart <= iEnd )
    {
        int     nCompareValue;
        
        iCandidate = (iStart + iEnd)/2;
        nCompareValue =
            strcmp( pszName, papszAttrAcronym[panAttrIndex[iCandidate]] );

        if( nCompareValue < 0 )
        {
            iEnd = iCandidate-1;
        }
        else if( nCompareValue > 0 )
        {
            iStart = iCandidate+1;
        }
        else
            return panAttrIndex[iCandidate];
    }

    return -1;
}
