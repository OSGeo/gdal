/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57ClassRegistrar class for keeping track of
 *           information on S57 object classes.
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
    papapszClassesFields = NULL;
    pachAttrClass = NULL;
    pachAttrType = NULL;
    panAttrIndex = NULL;
    papszAttrNames = NULL;
    papszAttrAcronym = NULL;
    papapszAttrValues = NULL;
}

/************************************************************************/
/*                         ~S57ClassRegistrar()                         */
/************************************************************************/

S57ClassRegistrar::~S57ClassRegistrar()

{
    int i;

    CSLDestroy( papszClassesInfo );
    CSLDestroy( papszTempResult );
    
    if( papapszClassesFields != NULL )
    {
        for( i = 0; i < nClasses; i++ )
            CSLDestroy( papapszClassesFields[i] );
        CPLFree( papapszClassesFields );
    }
    if( papszAttrNames )
    {
        for( i = 0; i < MAX_ATTRIBUTES; i++ )
        {
            CPLFree( papszAttrNames[i] );
            CPLFree( papszAttrAcronym[i] );
        }
        CPLFree( papszAttrNames );
        CPLFree( papszAttrAcronym );
    }
    CPLFree( pachAttrType );
    CPLFree( pachAttrClass );
    CPLFree( panAttrIndex );
}

/************************************************************************/
/*                              FindFile()                              */
/************************************************************************/

int S57ClassRegistrar::FindFile( const char *pszTarget, 
                                 const char *pszDirectory, 
                                 int bReportErr,
                                 VSILFILE **pfp )

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

    *pfp = VSIFOpenL( pszFilename, "rb" );

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

const char *S57ClassRegistrar::ReadLine( VSILFILE * fp )

{
    if( fp != NULL )
        return CPLReadLineL( fp );

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
                                 const char * pszProfile,
                                 int bReportErr )

{
    VSILFILE   *fp;
    char        szTargetFile[1024];

    if( pszDirectory == NULL )
        pszDirectory = CPLGetConfigOption("S57_CSV",NULL);

/* ==================================================================== */
/*      Read the s57objectclasses file.                                 */
/* ==================================================================== */
    if( pszProfile == NULL )
        pszProfile = CPLGetConfigOption( "S57_PROFILE", "" );
    
    if( EQUAL(pszProfile, "Additional_Military_Layers") )
    {
       sprintf( szTargetFile, "s57objectclasses_%s.csv", "aml" );
    }
    else if ( EQUAL(pszProfile, "Inland_Waterways") )
    {
       sprintf( szTargetFile, "s57objectclasses_%s.csv", "iw" );
    }
    else if( strlen(pszProfile) > 0 )
    {
       snprintf( szTargetFile, sizeof(szTargetFile), "s57objectclasses_%s.csv", pszProfile );
    }
    else
    {
       strcpy( szTargetFile, "s57objectclasses.csv" );
    }

    if( !FindFile( szTargetFile, pszDirectory, bReportErr, &fp ) )
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
        if( fp != NULL )
            VSIFCloseL( fp );
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
        VSIFCloseL( fp );
    iCurrentClass = -1;

    if( nClasses == 0 )
        return FALSE;

/* ==================================================================== */
/*      Read the attributes list.                                       */
/* ==================================================================== */

    if( EQUAL(pszProfile, "Additional_Military_Layers") )
    {
        sprintf( szTargetFile, "s57attributes_%s.csv", "aml" );
    }
    else if ( EQUAL(pszProfile, "Inland_Waterways") )
    {
       sprintf( szTargetFile, "s57attributes_%s.csv", "iw" );
    }
    else if( strlen(pszProfile) > 0 )
    {
       snprintf( szTargetFile, sizeof(szTargetFile), "s57attributes_%s.csv", pszProfile );
    }
    else
    {
       strcpy( szTargetFile, "s57attributes.csv" );
    }
       
    if( !FindFile( szTargetFile, pszDirectory, bReportErr, &fp ) )
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
        if( fp != NULL )
            VSIFCloseL( fp );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Prepare arrays for the per-attribute information.               */
/* -------------------------------------------------------------------- */
    papszAttrNames = (char **) CPLCalloc(sizeof(char *),MAX_ATTRIBUTES);
    papszAttrAcronym = (char **) CPLCalloc(sizeof(char *),MAX_ATTRIBUTES);
    //papapszAttrValues = (char ***) CPLCalloc(sizeof(char **),MAX_ATTRIBUTES);
    pachAttrType = (char *) CPLCalloc(sizeof(char),MAX_ATTRIBUTES);
    pachAttrClass = (char *) CPLCalloc(sizeof(char),MAX_ATTRIBUTES);
    panAttrIndex = (GUInt16 *) CPLCalloc(sizeof(GUInt16),MAX_ATTRIBUTES);
    
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
        if( iAttr < 0 || iAttr >= MAX_ATTRIBUTES )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Invalid S57 attribute %s",
                      papszTokens[2] );
            continue;
        }

        if( papszAttrNames[iAttr] != NULL )
        {
            CPLDebug( "S57", "Duplicate definition for attribute %d:%s", 
                      iAttr, papszTokens[2] );
            continue;
        }
        
        papszAttrNames[iAttr] = CPLStrdup(papszTokens[1]);
        papszAttrAcronym[iAttr] = CPLStrdup(papszTokens[2]);
        pachAttrType[iAttr] = papszTokens[3][0];
        pachAttrClass[iAttr] = papszTokens[4][0];

        CSLDestroy( papszTokens );
    }

    if( fp != NULL )
        VSIFCloseL( fp );
    
/* -------------------------------------------------------------------- */
/*      Build unsorted index of attributes.                             */
/* -------------------------------------------------------------------- */
    nAttrCount = 0;
    for( iAttr = 0; iAttr < MAX_ATTRIBUTES; iAttr++ )
    {
        if( papszAttrAcronym[iAttr] != NULL )
            panAttrIndex[nAttrCount++] = iAttr;
    }
    nAttrMax = panAttrIndex[nAttrCount-1];

/* -------------------------------------------------------------------- */
/*      Sort index by acronym.                                          */
/* -------------------------------------------------------------------- */
    int         bModified;
    GUInt16     nTemp;

    do
    {
        bModified = FALSE;
        for( iAttr = 0; iAttr < nAttrCount-1; iAttr++ )
        {
            if( strcmp(papszAttrAcronym[panAttrIndex[iAttr]],
                       papszAttrAcronym[panAttrIndex[iAttr+1]]) > 0 )
            {
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

/* -------------------------------------------------------------------- */
/*      Do we have our cache of class information field lists?          */
/* -------------------------------------------------------------------- */
    if( papapszClassesFields == NULL )
    {
        papapszClassesFields = (char ***) CPLCalloc(sizeof(void*),nClasses);
    }

/* -------------------------------------------------------------------- */
/*      Has this info been parsed yet?                                  */
/* -------------------------------------------------------------------- */
    if( papapszClassesFields[nNewIndex] == NULL )
        papapszClassesFields[nNewIndex] = 
            CSLTokenizeStringComplex( papszClassesInfo[nNewIndex],
                                      ",", TRUE, TRUE );

    papszCurrentFields = papapszClassesFields[nNewIndex];

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

        if( strcmp(GetAcronym(),pszAcronym) == 0 )
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
    if( iCurrentClass >= 0 && papszCurrentFields[0] != NULL )
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
        && papszCurrentFields[0] != NULL 
        && papszCurrentFields[1] != NULL )
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
        && papszCurrentFields[0] != NULL
        && papszCurrentFields[1] != NULL
        && papszCurrentFields[2] != NULL
        && papszCurrentFields[3] != NULL
        && papszCurrentFields[4] != NULL
        && papszCurrentFields[5] != NULL 
        && papszCurrentFields[6] != NULL )
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

int    S57ClassRegistrar::FindAttrByAcronym( const char * pszName )

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
