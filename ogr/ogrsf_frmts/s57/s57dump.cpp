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
    
    S57ClassRegistrar	oRegistrar;
    S57Reader  	oReader( papszArgv[1] );

    if( !oReader.Open( FALSE ) )
        exit( 1 );


    if( oRegistrar.LoadInfo( "/home/warmerda/data/s57", TRUE ) )
    {
        int	i, *panClassList = oReader.CollectClassList();

        oReader.SetClassBased( &oRegistrar );

        printf( "Classes found:\n" );
        for( i = 0; panClassList[i] != -1; i++ )
        {
            oRegistrar.SelectClass( panClassList[i] );
            printf( "%d: %s/%s\n",
                    panClassList[i],
                    oRegistrar.GetAcronym(),
                    oRegistrar.GetDescription() );
            
            oReader.AddFeatureDefn(
                S57Reader::GenerateObjectClassDefn( &oRegistrar,
                                                    panClassList[i] ) );
        }

        CPLFree( panClassList );
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
    
    OGRFeature	*poFeature;
    while( (poFeature = oReader.ReadNextFeature()) != NULL )
    {
        poFeature->DumpReadable( stdout );
        delete poFeature;
    }

    return 0;
}

