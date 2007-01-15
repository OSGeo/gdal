/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Test mainline for translating EPSG definitions into WKT.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.10  2006/03/29 01:08:34  fwarmerdam
 * added validation and xml reporting (optional)
 *
 * Revision 1.9  2004/09/23 16:11:14  fwarmerdam
 * Cleanup memory at end.
 *
 * Revision 1.8  2002/12/13 06:36:01  warmerda
 * report PROJ.4 form
 *
 * Revision 1.7  2002/11/29 22:11:05  warmerda
 * added ESRIified reporting, and ifdef out xml for now
 *
 * Revision 1.6  2002/01/25 20:47:36  warmerda
 * added simplified WKT output
 *
 * Revision 1.5  2002/01/18 15:30:57  warmerda
 * add usage reporting
 *
 * Revision 1.4  2001/12/06 18:18:47  warmerda
 * added preliminary xml srs support
 *
 * Revision 1.3  2001/01/19 21:10:47  warmerda
 * replaced tabs
 *
 * Revision 1.2  2000/03/20 14:57:27  warmerda
 * added -t for transform
 *
 * Revision 1.1  2000/03/16 19:05:07  warmerda
 * New
 *
 */

#include "ogr_spatialref.h"
#include "cpl_conv.h"

void Usage()

{
    printf( "testepsg [-xml] [-t src_def trg_def x y z]* [def]*\n" );
    printf( "  -t: transform a coordinate from source GCS/PCS to target GCS/PCS\n" );
    printf( "\n" );
    printf( "def's  on their own are translated to WKT & XML and printed.\n" );
    printf( "def's may be of any user input format, a WKT def, an\n" ); 
    printf( "EPSG:n definition or the name of a file containing WKT/XML.\n");
}

int main( int nArgc, char ** papszArgv )

{
    OGRSpatialReference oSRS;
    int i;
    int bReportXML = FALSE;

    if( nArgc < 2 )
        Usage();

    for( i = 1; i < nArgc; i++ )
    {
        if( EQUAL(papszArgv[i],"-xml") )
            bReportXML = TRUE;
        
        else if( EQUAL(papszArgv[i],"-t") && i < nArgc - 4 )
        {
            OGRSpatialReference oSourceSRS, oTargetSRS;
            OGRCoordinateTransformation *poCT;
            double                      x, y, z_orig, z;
            int                         nArgsUsed = 4;
            
            if( oSourceSRS.SetFromUserInput(papszArgv[i+1]) != OGRERR_NONE )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "SetFromUserInput(%s) failed.", 
                          papszArgv[i+1] );
                continue;
            }
            if( oTargetSRS.SetFromUserInput(papszArgv[i+2]) != OGRERR_NONE )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "SetFromUserInput(%s) failed.", 
                          papszArgv[i+2] );
                continue;
            }
            
            poCT = OGRCreateCoordinateTransformation( &oSourceSRS,
                                                      &oTargetSRS );
            x = atof( papszArgv[i+3] );
            y = atof( papszArgv[i+4] );
            if( i < nArgc - 5 
                && (atof(papszArgv[i+5]) > 0.0 || papszArgv[i+5][0] == '0') )
            {
                z_orig = z = atof(papszArgv[i+5]);
                nArgsUsed++;
            }
            else
                z_orig = z = 0;
            
            if( poCT == NULL || !poCT->Transform( 1, &x, &y, &z ) )
                printf( "Transformation failed.\n" );
            else
                printf( "(%f,%f,%f) -> (%f,%f,%f)\n", 
                        atof( papszArgv[i+3] ),
                        atof( papszArgv[i+4] ),
                        z_orig, 
                        x, y, z );
            
            i += nArgsUsed;
        }
        else 
        {
            if( oSRS.SetFromUserInput(papszArgv[i]) != OGRERR_NONE )
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Error occured translating %s.\n", 
                          papszArgv[i] );
            else
            {
                char  *pszWKT = NULL;

                if( oSRS.Validate() != OGRERR_NONE )
                    printf( "Validate Fails.\n" );
                else
                    printf( "Validate Succeeds.\n" );
                
                oSRS.exportToPrettyWkt( &pszWKT, FALSE );
                printf( "WKT[%s] =\n%s\n", 
                        papszArgv[i], pszWKT );
                CPLFree( pszWKT );

                printf( "\n" );

                oSRS.exportToPrettyWkt( &pszWKT, TRUE );
                printf( "Simplified WKT[%s] =\n%s\n", 
                        papszArgv[i], pszWKT );
                CPLFree( pszWKT );

                printf( "\n" );

                OGRSpatialReference *poSRS2;

                poSRS2 = oSRS.Clone();
                poSRS2->StripCTParms();
                poSRS2->exportToWkt( &pszWKT );
                printf( "Old Style WKT[%s] = %s\n", 
                        papszArgv[i], pszWKT );
                CPLFree( pszWKT );
                delete poSRS2;

                poSRS2 = oSRS.Clone();
                poSRS2->morphToESRI();
                poSRS2->exportToPrettyWkt( &pszWKT, FALSE );
                printf( "ESRI'ified WKT[%s] = \n%s\n", 
                        papszArgv[i], pszWKT );
                CPLFree( pszWKT );
                delete poSRS2;

                oSRS.exportToProj4( &pszWKT );
                printf( "PROJ.4 rendering of [%s] = %s\n", 
                        papszArgv[i], pszWKT );
                CPLFree( pszWKT );


                if( bReportXML )
                {
                    char       *pszRawXML;
                    if( oSRS.exportToXML(&pszRawXML) == OGRERR_NONE )
                    {
                        printf( "XML[%s] =\n%s\n", 
                                papszArgv[i], pszRawXML );
                        CPLFree( pszRawXML );
                    }
                    else
                    {
                        printf( "XML translation failed\n" );
                    }
                }

                printf( "\n" );
            }
        }
    }

    OSRCleanup();
    CPLFinderClean();
}
