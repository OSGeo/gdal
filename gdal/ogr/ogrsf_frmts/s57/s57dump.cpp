/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing S57 driver data.
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "s57.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    if( nArgc < 2 )
    {
        printf( "Usage: s57dump [-pen] [-split] [-lnam] [-return-prim] "
                "[-no-update]\n"
                "               [-return-link] [-data <dirpath>] filename\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Process commandline arguments.                                  */
/* -------------------------------------------------------------------- */
    char **papszOptions = NULL;
    bool bReturnPrimitives = false;
    char *pszDataPath = NULL;

    for( int iArg = 1; iArg < nArgc-1; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-split") )
            papszOptions =
                CSLSetNameValue( papszOptions, S57O_SPLIT_MULTIPOINT, "ON" );
        else if( EQUAL(papszArgv[iArg],"-data") )
            pszDataPath = papszArgv[++iArg];
        else if( EQUAL(papszArgv[iArg],"-no-update") )
            papszOptions =
                CSLSetNameValue( papszOptions, S57O_UPDATES, "OFF" );
        else if( EQUAL(papszArgv[iArg],"-pen") )
            papszOptions =
                CSLSetNameValue( papszOptions, S57O_PRESERVE_EMPTY_NUMBERS,
                                 "ON" );
        else if( STARTS_WITH_CI(papszArgv[iArg], "-return-prim") )
        {
            papszOptions =
                CSLSetNameValue( papszOptions, S57O_RETURN_PRIMITIVES,
                                 "ON" );
            bReturnPrimitives = true;
        }
        else if( STARTS_WITH_CI(papszArgv[iArg],"-lnam") )
            papszOptions =
                CSLSetNameValue( papszOptions, S57O_LNAM_REFS, "ON" );
        else if( STARTS_WITH_CI(papszArgv[iArg], "-return-link") )
            papszOptions =
                CSLSetNameValue( papszOptions, S57O_RETURN_LINKAGES, "ON" );
    }

/* -------------------------------------------------------------------- */
/*      Load the class definitions into the registrar.                  */
/* -------------------------------------------------------------------- */
    S57ClassRegistrar oRegistrar;
    bool bRegistrarLoaded = oRegistrar.LoadInfo( pszDataPath, NULL, true );

    S57ClassContentExplorer *poClassContentExplorer = NULL;
    if (bRegistrarLoaded)
        poClassContentExplorer = new S57ClassContentExplorer(&oRegistrar);

/* -------------------------------------------------------------------- */
/*      Get a list of candidate files.                                  */
/* -------------------------------------------------------------------- */
    char **papszFiles = S57FileCollector( papszArgv[nArgc-1] );

    for( int iFile = 0;
         papszFiles != NULL && papszFiles[iFile] != NULL;
         iFile++ )
    {
        printf( "Found: %s\n", papszFiles[iFile] );
    }

    for( int iFile = 0;
         papszFiles != NULL && papszFiles[iFile] != NULL;
         iFile++ )
    {
        printf( "<------------------------------------------------------------"
                "------------->\n" );
        printf( "\nFile: %s\n\n", papszFiles[iFile] );

        S57Reader oReader( papszFiles[iFile] );

        oReader.SetOptions( papszOptions );

        if( !oReader.Open( FALSE ) )
            continue;

        int nOptionFlags = oReader.GetOptionFlags();

        if( bRegistrarLoaded )
        {
            bool bGeneric = false;
            std::vector<int> anClassList;

            oReader.CollectClassList(anClassList);

            oReader.SetClassBased( &oRegistrar, poClassContentExplorer );

            printf( "Classes found:\n" );
            for( unsigned int i = 0; i < anClassList.size(); i++ )
            {
                if( anClassList[i] == 0 )
                    continue;

                if( poClassContentExplorer->SelectClass( i ) )
                {
                    printf( "%u: %s/%s\n",
                            i,
                            poClassContentExplorer->GetAcronym(),
                            poClassContentExplorer->GetDescription() );

                    oReader.AddFeatureDefn(
                        S57GenerateObjectClassDefn( &oRegistrar,
                                                    poClassContentExplorer,
                                                    i, nOptionFlags ) );
                }
                else
                {
                    printf( "%u: unrecognized ... treat as generic.\n", i );
                    bGeneric = true;
                }
            }

            if( bGeneric )
            {
                oReader.AddFeatureDefn(
                    S57GenerateGeomFeatureDefn( wkbUnknown, nOptionFlags ) );
            }
        }
        else
        {
            oReader.AddFeatureDefn(
                S57GenerateGeomFeatureDefn( wkbPoint, nOptionFlags ) );
            oReader.AddFeatureDefn(
                S57GenerateGeomFeatureDefn( wkbLineString, nOptionFlags ) );
            oReader.AddFeatureDefn(
                S57GenerateGeomFeatureDefn( wkbPolygon, nOptionFlags ) );
            oReader.AddFeatureDefn(
                S57GenerateGeomFeatureDefn( wkbNone, nOptionFlags ) );
        }

        if( bReturnPrimitives )
        {
            oReader.AddFeatureDefn(
                S57GenerateVectorPrimitiveFeatureDefn( RCNM_VI, nOptionFlags));
            oReader.AddFeatureDefn(
                S57GenerateVectorPrimitiveFeatureDefn( RCNM_VC, nOptionFlags));
            oReader.AddFeatureDefn(
                S57GenerateVectorPrimitiveFeatureDefn( RCNM_VE, nOptionFlags));
            oReader.AddFeatureDefn(
                S57GenerateVectorPrimitiveFeatureDefn( RCNM_VF, nOptionFlags));
        }

        oReader.AddFeatureDefn( S57GenerateDSIDFeatureDefn() );

        OGRFeature *poFeature = NULL;
        int nFeatures = 0;

        while( (poFeature = oReader.ReadNextFeature()) != NULL )
        {
            poFeature->DumpReadable( stdout );
            nFeatures++;
            delete poFeature;
        }

        printf( "Feature Count: %d\n", nFeatures );
    }

    return 0;
}
