/******************************************************************************
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57ClassRegistrar class for keeping track of
 *           information on S57 object classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "s57.h"

CPL_CVSID("$Id$")

#ifdef S57_BUILTIN_CLASSES
#include "s57tables.h"
#endif

/************************************************************************/
/*                         S57ClassRegistrar()                          */
/************************************************************************/

S57ClassRegistrar::S57ClassRegistrar() :
    nClasses(0),
    nAttrCount(0),
    papszNextLine(nullptr)
{}

/************************************************************************/
/*                         ~S57ClassRegistrar()                         */
/************************************************************************/

S57ClassRegistrar::~S57ClassRegistrar()

{
    nClasses = 0;
    for( size_t i = 0; i < aoAttrInfos.size(); i++ )
        delete aoAttrInfos[i];
    aoAttrInfos.resize(0);
    nAttrCount = 0;
}

/************************************************************************/
/*                        S57ClassContentExplorer()                     */
/************************************************************************/

S57ClassContentExplorer::S57ClassContentExplorer(
    S57ClassRegistrar* poRegistrarIn ) :
    poRegistrar(poRegistrarIn),
    papapszClassesFields(nullptr),
    iCurrentClass(-1),
    papszCurrentFields(nullptr),
    papszTempResult(nullptr)
{}

/************************************************************************/
/*                        ~S57ClassContentExplorer()                    */
/************************************************************************/

S57ClassContentExplorer::~S57ClassContentExplorer()
{
    CSLDestroy( papszTempResult );

    if( papapszClassesFields != nullptr )
    {
        for( int i = 0; i < poRegistrar->nClasses; i++ )
            CSLDestroy( papapszClassesFields[i] );
        CPLFree( papapszClassesFields );
    }
}

/************************************************************************/
/*                              FindFile()                              */
/************************************************************************/

bool S57ClassRegistrar::FindFile( const char *pszTarget,
                                  const char *pszDirectory,
                                  bool bReportErr,
                                  VSILFILE **pfp )

{
    const char *pszFilename = nullptr;

    if( pszDirectory == nullptr )
    {
        pszFilename = CPLFindFile( "s57", pszTarget );
        if( pszFilename == nullptr )
            pszFilename = pszTarget;
    }
    else
    {
        pszFilename = CPLFormFilename( pszDirectory, pszTarget, nullptr );
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
    if( *pfp == nullptr )
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
    if( fp != nullptr )
        return CPLReadLineL( fp );

    if( papszNextLine == nullptr )
        return nullptr;

    if( *papszNextLine == nullptr )
    {
        papszNextLine = nullptr;
        return nullptr;
    }

    return *(papszNextLine++);
}

/************************************************************************/
/*                              LoadInfo()                              */
/************************************************************************/

bool S57ClassRegistrar::LoadInfo( const char * pszDirectory,
                                  const char * pszProfile,
                                  bool bReportErr )

{
    if( pszDirectory == nullptr )
        pszDirectory = CPLGetConfigOption("S57_CSV",nullptr);

/* ==================================================================== */
/*      Read the s57objectclasses file.                                 */
/* ==================================================================== */
    if( pszProfile == nullptr )
        pszProfile = CPLGetConfigOption( "S57_PROFILE", "" );

    char szTargetFile[1024];  // TODO: Get this off of the stack.
    if( EQUAL(pszProfile, "Additional_Military_Layers") )
    {
        // Has been suppressed in GDAL data/
       snprintf( szTargetFile, sizeof(szTargetFile), "s57objectclasses_%s.csv", "aml" );
    }
    else if ( EQUAL(pszProfile, "Inland_Waterways") )
    {
        // Has been suppressed in GDAL data/
       snprintf( szTargetFile, sizeof(szTargetFile), "s57objectclasses_%s.csv", "iw" );
    }
    else if( strlen(pszProfile) > 0 )
    {
       snprintf( szTargetFile, sizeof(szTargetFile), "s57objectclasses_%s.csv", pszProfile );
    }
    else
    {
       strcpy( szTargetFile, "s57objectclasses.csv" );
    }

    VSILFILE *fp = nullptr;
    if( !FindFile( szTargetFile, pszDirectory, bReportErr, &fp ) )
    {
        if( EQUAL(pszProfile, "Additional_Military_Layers") ||
            EQUAL(pszProfile, "Inland_Waterways") )
        {
            strcpy( szTargetFile, "s57objectclasses.csv" );
            if( !FindFile( szTargetFile, pszDirectory, bReportErr, &fp ) )
                return false;
        }
        return false;
    }

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
        if( fp != nullptr )
            VSIFCloseL( fp );
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Read and form string list.                                      */
/* -------------------------------------------------------------------- */
    apszClassesInfo.Clear();
    while( (pszLine = ReadLine(fp)) != nullptr )
    {
        if( strstr(pszLine, "###") != nullptr )
            continue;
        apszClassesInfo.AddString(pszLine);
    }

/* -------------------------------------------------------------------- */
/*      Cleanup, and establish state.                                   */
/* -------------------------------------------------------------------- */
    if( fp != nullptr )
        VSIFCloseL( fp );

    nClasses = apszClassesInfo.size();
    if( nClasses == 0 )
        return false;

/* ==================================================================== */
/*      Read the attributes list.                                       */
/* ==================================================================== */

    if( EQUAL(pszProfile, "Additional_Military_Layers") )
    {
        // Has been suppressed in GDAL data/
      snprintf( szTargetFile, sizeof(szTargetFile), "s57attributes_%s.csv", "aml" );
    }
    else if ( EQUAL(pszProfile, "Inland_Waterways") )
    {
        // Has been suppressed in GDAL data/
       snprintf( szTargetFile, sizeof(szTargetFile),"s57attributes_%s.csv", "iw" );
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
    {
        if( EQUAL(pszProfile, "Additional_Military_Layers") ||
            EQUAL(pszProfile, "Inland_Waterways") )
        {
            strcpy( szTargetFile, "s57attributes.csv" );
            if( !FindFile( szTargetFile, pszDirectory, bReportErr, &fp ) )
                return false;
        }
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Skip the line defining the column titles.                       */
/* -------------------------------------------------------------------- */
    pszLine = ReadLine( fp );

    if( !EQUAL(pszLine,
          "\"Code\",\"Attribute\",\"Acronym\",\"Attributetype\",\"Class\"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "s57attributes columns don't match expected format!\n" );
        if( fp != nullptr )
            VSIFCloseL( fp );
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Read and form string list.                                      */
/* -------------------------------------------------------------------- */
    while( (pszLine = ReadLine(fp)) != nullptr )
    {
        if( strstr(pszLine, "###") != nullptr )
            continue;

        char    **papszTokens = CSLTokenizeStringComplex( pszLine, ",",
                                                          TRUE, TRUE );

        if( CSLCount(papszTokens) < 5 )
        {
            CSLDestroy(papszTokens);
            continue;
        }

        int iAttr = atoi(papszTokens[0]);
        if( iAttr >= (int) aoAttrInfos.size() )
            aoAttrInfos.resize(iAttr+1);

        if( iAttr < 0 || aoAttrInfos[iAttr] != nullptr )
        {
            CPLDebug( "S57",
                      "Duplicate/corrupt definition for attribute %d:%s",
                      iAttr, papszTokens[2] );
            CSLDestroy( papszTokens );
            continue;
        }

        aoAttrInfos[iAttr] = new S57AttrInfo();
        aoAttrInfos[iAttr]->osName = papszTokens[1];
        aoAttrInfos[iAttr]->osAcronym = papszTokens[2];
        aoAttrInfos[iAttr]->chType = papszTokens[3][0];
        aoAttrInfos[iAttr]->chClass = papszTokens[4][0];
        anAttrIndex.push_back(iAttr);
        CSLDestroy( papszTokens );
    }

    if( fp != nullptr )
        VSIFCloseL( fp );

    nAttrCount = static_cast<int>(anAttrIndex.size());

/* -------------------------------------------------------------------- */
/*      Sort index by acronym.                                          */
/* -------------------------------------------------------------------- */
    bool bModified = false;
    do
    {
        bModified = false;
        for( int iAttr = 0; iAttr < nAttrCount-1; iAttr++ )
        {
            if( strcmp(aoAttrInfos[anAttrIndex[iAttr]]->osAcronym,
                       aoAttrInfos[anAttrIndex[iAttr+1]]->osAcronym) > 0 )
            {
                int nTemp = anAttrIndex[iAttr];
                anAttrIndex[iAttr] = anAttrIndex[iAttr+1];
                anAttrIndex[iAttr+1] = nTemp;
                bModified = true;
            }
        }
    } while( bModified );

    return true;
}

/************************************************************************/
/*                         SelectClassByIndex()                         */
/************************************************************************/

bool S57ClassContentExplorer::SelectClassByIndex( int nNewIndex )

{
    if( nNewIndex < 0 || nNewIndex >= poRegistrar->nClasses )
        return false;

/* -------------------------------------------------------------------- */
/*      Do we have our cache of class information field lists?          */
/* -------------------------------------------------------------------- */
    if( papapszClassesFields == nullptr )
    {
        papapszClassesFields = (char ***) CPLCalloc(sizeof(void*),poRegistrar->nClasses);
    }

/* -------------------------------------------------------------------- */
/*      Has this info been parsed yet?                                  */
/* -------------------------------------------------------------------- */
    if( papapszClassesFields[nNewIndex] == nullptr )
        papapszClassesFields[nNewIndex] =
            CSLTokenizeStringComplex( poRegistrar->apszClassesInfo[nNewIndex],
                                      ",", TRUE, TRUE );

    papszCurrentFields = papapszClassesFields[nNewIndex];

    iCurrentClass = nNewIndex;

    return true;
}

/************************************************************************/
/*                             SelectClass()                            */
/************************************************************************/

bool S57ClassContentExplorer::SelectClass( int nOBJL )

{
    for( int i = 0; i < poRegistrar->nClasses; i++ )
    {
        if( atoi(poRegistrar->apszClassesInfo[i]) == nOBJL )
            return SelectClassByIndex( i );
    }

    return FALSE;
}

/************************************************************************/
/*                            SelectClass()                             */
/************************************************************************/

bool S57ClassContentExplorer::SelectClass( const char *pszAcronym )

{
    for( int i = 0; i < poRegistrar->nClasses; i++ )
    {
        if( !SelectClassByIndex( i ) )
            continue;

        const char* pszClassAcronym = GetAcronym();
        if( pszClassAcronym != nullptr && strcmp(pszClassAcronym,pszAcronym) == 0 )
            return true;
    }

    return false;
}

/************************************************************************/
/*                              GetOBJL()                               */
/************************************************************************/

int S57ClassContentExplorer::GetOBJL()

{
    if( iCurrentClass >= 0 )
        return atoi(poRegistrar->apszClassesInfo[iCurrentClass]);

    return -1;
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

const char * S57ClassContentExplorer::GetDescription() const

{
    if( iCurrentClass >= 0 && papszCurrentFields[0] != nullptr )
        return papszCurrentFields[1];

    return nullptr;
}

/************************************************************************/
/*                             GetAcronym()                             */
/************************************************************************/

const char * S57ClassContentExplorer::GetAcronym() const

{
    if( iCurrentClass >= 0
        && papszCurrentFields[0] != nullptr
        && papszCurrentFields[1] != nullptr )
        return papszCurrentFields[2];

    return nullptr;
}

/************************************************************************/
/*                          GetAttributeList()                          */
/*                                                                      */
/*      The passed string can be "a", "b", "c" or NULL for all.  The    */
/*      returned list remained owned by this object, not the caller.    */
/************************************************************************/

char **S57ClassContentExplorer::GetAttributeList( const char * pszType )

{
    if( iCurrentClass < 0 )
        return nullptr;

    CSLDestroy( papszTempResult );
    papszTempResult = nullptr;

    for( int iColumn = 3; iColumn < 6; iColumn++ )
    {
        if( pszType != nullptr && iColumn == 3 && !EQUAL(pszType,"a") )
            continue;

        if( pszType != nullptr && iColumn == 4 && !EQUAL(pszType,"b") )
            continue;

        if( pszType != nullptr && iColumn == 5 && !EQUAL(pszType,"c") )
            continue;

        char **papszTokens =
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

char S57ClassContentExplorer::GetClassCode() const

{
    if( iCurrentClass >= 0
        && papszCurrentFields[0] != nullptr
        && papszCurrentFields[1] != nullptr
        && papszCurrentFields[2] != nullptr
        && papszCurrentFields[3] != nullptr
        && papszCurrentFields[4] != nullptr
        && papszCurrentFields[5] != nullptr
        && papszCurrentFields[6] != nullptr )
        return papszCurrentFields[6][0];

    return '\0';
}

/************************************************************************/
/*                           GetPrimitives()                            */
/************************************************************************/

char **S57ClassContentExplorer::GetPrimitives()

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

    return nullptr;
}

/************************************************************************/
/*                            GetAttrInfo()                             */
/************************************************************************/

const S57AttrInfo *S57ClassRegistrar::GetAttrInfo(int iAttr)
{
    if( iAttr < 0 || iAttr >= (int) aoAttrInfos.size() )
        return nullptr;

    return aoAttrInfos[iAttr];
}

/************************************************************************/
/*                         FindAttrByAcronym()                          */
/************************************************************************/

int    S57ClassRegistrar::FindAttrByAcronym( const char * pszName )

{
    int iStart = 0;
    int iEnd = nAttrCount-1;

    while( iStart <= iEnd )
    {
        const int iCandidate = (iStart + iEnd)/2;
        int nCompareValue =
            strcmp(pszName, aoAttrInfos[anAttrIndex[iCandidate]]->osAcronym);

        if( nCompareValue < 0 )
        {
            iEnd = iCandidate-1;
        }
        else if( nCompareValue > 0 )
        {
            iStart = iCandidate+1;
        }
        else
            return anAttrIndex[iCandidate];
    }

    return -1;
}
