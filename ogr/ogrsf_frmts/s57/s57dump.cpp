/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing S57 driver data.
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
 * Revision 1.5  1999/11/18 19:01:25  warmerda
 * expanded tabs
 *
 * Revision 1.4  1999/11/18 18:57:45  warmerda
 * utilize s57filecollector
 *
 * Revision 1.3  1999/11/16 21:47:32  warmerda
 * updated class occurance collection
 *
 * Revision 1.2  1999/11/08 22:23:00  warmerda
 * added object class support
 *
 * Revision 1.1  1999/11/03 22:12:43  warmerda
 * New
 *
 */

#include "s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    if( nArgc < 2 )
    {
        printf( "Usage: s57dump filename\n" );
        exit( 1 );
    }
    
/* -------------------------------------------------------------------- */
/*      Load the class definitions into the registrar.                  */
/* -------------------------------------------------------------------- */
    S57ClassRegistrar   oRegistrar;
    int                 bRegistrarLoaded;

    bRegistrarLoaded = oRegistrar.LoadInfo( "/home/warmerda/data/s57", TRUE );

/* -------------------------------------------------------------------- */
/*      Get a list of candidate files.                                  */
/* -------------------------------------------------------------------- */
    char        **papszFiles;
    int         iFile;

    papszFiles = S57FileCollector( papszArgv[1] );

    for( iFile = 0; papszFiles != NULL && papszFiles[iFile] != NULL; iFile++ )
    {
        printf( "Found: %s\n", papszFiles[iFile] );
    }

    for( iFile = 0; papszFiles != NULL && papszFiles[iFile] != NULL; iFile++ )
    {
        printf( "<------------------------------------------------------------------------->\n" );
        printf( "\nFile: %s\n\n", papszFiles[iFile] );
        
        S57Reader       oReader( papszFiles[iFile] );

        if( !oReader.Open( FALSE ) )
            continue;

        if( bRegistrarLoaded )
        {
            int i, anClassList[MAX_CLASSES];
            
            for( i = 0; i < MAX_CLASSES; i++ )
                anClassList[i] = 0;
        
            oReader.CollectClassList(anClassList, MAX_CLASSES);

            oReader.SetClassBased( &oRegistrar );

            printf( "Classes found:\n" );
            for( i = 0; i < MAX_CLASSES; i++ )
            {
                if( anClassList[i] == 0 )
                    continue;
                
                oRegistrar.SelectClass( i );
                printf( "%d: %s/%s\n",
                        i,
                        oRegistrar.GetAcronym(),
                        oRegistrar.GetDescription() );
            
                oReader.AddFeatureDefn(
                    S57Reader::GenerateObjectClassDefn( &oRegistrar, i ) );
            }
        }
        else
        {
            oReader.AddFeatureDefn(
                S57Reader::GenerateGeomFeatureDefn( wkbPoint ) );
            oReader.AddFeatureDefn(
                S57Reader::GenerateGeomFeatureDefn( wkbLineString ) );
            oReader.AddFeatureDefn(
                S57Reader::GenerateGeomFeatureDefn( wkbPolygon ) );
            oReader.AddFeatureDefn(
                S57Reader::GenerateGeomFeatureDefn( wkbNone ) );
        }
    
        OGRFeature      *poFeature;
        int             nFeatures = 0;
    
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

