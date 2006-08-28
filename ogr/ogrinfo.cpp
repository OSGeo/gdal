/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing OGR driver data.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.36  2006/08/28 14:00:02  mloskot
 * Added stronger test of Shapefile reading failures, e.g. truncated files.
 * The problem was discovered by Tim Sutton and reported here https://svn.qgis.org/trac/ticket/200
 *
 * Revision 1.35  2006/07/14 16:18:40  fwarmerdam
 * --formats now handled by generic argument processor.
 *
 * Revision 1.34  2006/05/23 23:05:20  mloskot
 * Added CPL_DEBUG diagnostics and variables initialization to ogrinfo.cpp.
 *
 * Revision 1.33  2006/05/15 18:28:48  fwarmerdam
 * allow attribute filter on -sql requests
 *
 * Revision 1.32  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.31  2006/03/02 15:42:52  fwarmerdam
 * Accept -so or -summary.
 *
 * Revision 1.30  2006/02/02 01:19:34  fwarmerdam
 * Use EQUAL, not EQUALN for -so and -al as per bug 1052.
 *
 * Revision 1.29  2006/01/27 00:08:36  fwarmerdam
 * added Get{FID,Geometry}Column() support
 *
 * Revision 1.28  2005/11/25 02:16:53  fwarmerdam
 * Added --help-general.
 *
 * Revision 1.27  2005/11/19 00:41:31  fwarmerdam
 * added general commandline processor
 *
 * Revision 1.26  2005/10/20 19:54:18  fwarmerdam
 * added better cleanup logic
 *
 * Revision 1.25  2005/09/05 19:30:06  fwarmerdam
 * fix usage message
 *
 * Revision 1.24  2004/10/12 19:27:17  fwarmerdam
 * avoid passing local stdout into GDAL library
 *
 * Revision 1.23  2004/07/10 04:45:20  warmerda
 * added dialect option
 *
 * Revision 1.22  2004/02/19 17:17:42  warmerda
 * clean finder for easier memory leak testing
 *
 * Revision 1.21  2003/11/19 20:41:17  warmerda
 * avoid memory leak
 *
 * Revision 1.20  2003/03/19 20:28:30  warmerda
 * destroy registrar on exit to help find memory leaks
 *
 * Revision 1.19  2003/03/05 15:36:22  warmerda
 * Added -summary to Usage().
 *
 * Revision 1.18  2003/02/25 14:56:25  warmerda
 * Added the -so (summary only) switch.
 *
 * Revision 1.17  2002/11/17 17:41:39  warmerda
 * added --formats option
 *
 * Revision 1.16  2002/08/08 13:02:01  warmerda
 * added the -al commandline switch
 *
 * Revision 1.15  2002/04/25 03:40:41  warmerda
 * fixed to pass spatial query
 *
 * Revision 1.14  2002/04/24 20:00:43  warmerda
 * added the -sql command line switch
 *
 * Revision 1.13  2002/03/27 22:50:16  warmerda
 * improve quiet support, and make WKT output be pretty
 *
 * Revision 1.12  2001/11/19 21:59:33  warmerda
 * added repeat count for memory leak testing
 *
 * Revision 1.11  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.10  2001/07/17 15:00:21  danmo
 * Report layer extent in ReportOnLayer().
 *
 * Revision 1.9  2001/06/26 20:58:45  warmerda
 * added spatial query option
 *
 * Revision 1.8  2001/06/19 15:48:36  warmerda
 * added feature attribute query support
 *
 * Revision 1.7  2000/03/14 21:37:49  warmerda
 * report layer geometry type
 *
 * Revision 1.6  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.5  1999/11/04 21:07:22  warmerda
 * Changed to OGRRegisterAll().
 *
 * Revision 1.4  1999/09/29 16:36:41  warmerda
 * added srs reporting
 *
 * Revision 1.3  1999/09/22 13:31:48  warmerda
 * added sdts
 *
 * Revision 1.2  1999/09/13 14:34:20  warmerda
 * added feature reporting
 *
 * Revision 1.1  1999/09/09 20:40:19  warmerda
 * New
 *
 */

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

int     bReadOnly = FALSE;
int     bVerbose = TRUE;
int     bSummaryOnly = FALSE;
int     nFetchFID = OGRNullFID;

static void Usage();

static void ReportOnLayer( OGRLayer *, const char *, OGRGeometry * );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    const char *pszWHERE = NULL;
    const char  *pszDataSource = NULL;
    char        **papszLayers = NULL;
    OGRGeometry *poSpatialFilter = NULL;
    int         nRepeatCount = 1, bAllLayers = FALSE;
    const char  *pszSQLStatement = NULL;
    const char  *pszDialect = NULL;
    
/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();
    
/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    nArgc = OGRGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );
    
    if( nArgc < 1 )
        exit( -nArgc );

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-ro") )
            bReadOnly = TRUE;
        else if( EQUAL(papszArgv[iArg],"-q") )
            bVerbose = FALSE;
        else if( EQUAL(papszArgv[iArg],"-fid") && iArg < nArgc-1 )
            nFetchFID = atoi(papszArgv[++iArg]);
        else if( EQUAL(papszArgv[iArg],"-spat") 
                 && papszArgv[iArg+1] != NULL 
                 && papszArgv[iArg+2] != NULL 
                 && papszArgv[iArg+3] != NULL 
                 && papszArgv[iArg+4] != NULL )
        {
            OGRLinearRing  oRing;

            oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+2]) );
            oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+4]) );
            oRing.addPoint( atof(papszArgv[iArg+3]), atof(papszArgv[iArg+4]) );
            oRing.addPoint( atof(papszArgv[iArg+3]), atof(papszArgv[iArg+2]) );
            oRing.addPoint( atof(papszArgv[iArg+1]), atof(papszArgv[iArg+2]) );

            poSpatialFilter = new OGRPolygon();
            ((OGRPolygon *) poSpatialFilter)->addRing( &oRing );
            iArg += 4;
        }
        else if( EQUAL(papszArgv[iArg],"-where") && papszArgv[iArg+1] != NULL )
        {
            pszWHERE = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-sql") && papszArgv[iArg+1] != NULL )
        {
            pszSQLStatement = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-dialect") 
                 && papszArgv[iArg+1] != NULL )
        {
            pszDialect = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-rc") && papszArgv[iArg+1] != NULL )
        {
            nRepeatCount = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-al") )
        {
            bAllLayers = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-so") 
                 || EQUAL(papszArgv[iArg],"-summary")  )
        {
            bSummaryOnly = TRUE;
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage();
        }
        else if( pszDataSource == NULL )
            pszDataSource = papszArgv[iArg];
        else
        {
            papszLayers = CSLAddString( papszLayers, papszArgv[iArg] );
            bAllLayers = FALSE;
        }
    }

    if( pszDataSource == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poDS = NULL;
    OGRSFDriver         *poDriver = NULL;

    poDS = OGRSFDriverRegistrar::Open( pszDataSource, !bReadOnly, &poDriver );
    if( poDS == NULL && !bReadOnly )
    {
        poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE, &poDriver );
        if( poDS != NULL && bVerbose )
        {
            printf( "Had to open data source read-only.\n" );
            bReadOnly = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
    {
        OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
        
        printf( "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers.\n",
                pszDataSource );

        for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            printf( "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
        }

        exit( 1 );
    }

    CPLAssert( poDriver != NULL);

/* -------------------------------------------------------------------- */
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
    if( bVerbose )
        printf( "INFO: Open of `%s'\n"
                "      using driver `%s' successful.\n",
                pszDataSource, poDriver->GetName() );

    if( bVerbose && !EQUAL(pszDataSource,poDS->GetName()) )
    {
        printf( "INFO: Internal data source name `%s'\n"
                "      different from user name `%s'.\n",
                poDS->GetName(), pszDataSource );
    }

/* -------------------------------------------------------------------- */
/*      Special case for -sql clause.  No source layers required.       */
/* -------------------------------------------------------------------- */
    if( pszSQLStatement != NULL )
    {
        OGRLayer *poResultSet;

        nRepeatCount = 0;  // skip layer reporting.

        if( CSLCount(papszLayers) > 0 )
            printf( "layer names ignored in combination with -sql.\n" );
        
        poResultSet = poDS->ExecuteSQL( pszSQLStatement, poSpatialFilter, 
                                        pszDialect );

        if( pszWHERE != NULL )
            poResultSet->SetAttributeFilter( pszWHERE );

        if( poResultSet != NULL )
        {
            ReportOnLayer( poResultSet, NULL, NULL );
            poDS->ReleaseResultSet( poResultSet );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
    CPLDebug( "OGR", "GetLayerCount() = %d\n", poDS->GetLayerCount() );

    for( int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++ )
    {
        for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
        {
            OGRLayer        *poLayer = poDS->GetLayer(iLayer);

            if( poLayer == NULL )
            {
                printf( "FAILURE: Couldn't fetch advertised layer %d!\n",
                        iLayer );
                exit( 1 );
            }

            if( CSLCount(papszLayers) == 0 && !bAllLayers )
            {
                printf( "%d: %s",
                        iLayer+1,
                        poLayer->GetLayerDefn()->GetName() );

                if( poLayer->GetLayerDefn()->GetGeomType() != wkbUnknown )
                    printf( " (%s)", 
                            OGRGeometryTypeToName( 
                                poLayer->GetLayerDefn()->GetGeomType() ) );

                printf( "\n" );
            }
            else if( bAllLayers 
                     || CSLFindString( papszLayers,
                                   poLayer->GetLayerDefn()->GetName() ) != -1 )
            {
                if( iRepeat != 0 )
                    poLayer->ResetReading();

                ReportOnLayer( poLayer, pszWHERE, poSpatialFilter );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszArgv );
    CSLDestroy( papszLayers );
    delete poDS;

    delete OGRSFDriverRegistrar::GetRegistrar();
    OSRCleanup();
    CPLFinderClean();
    VSICleanupFileManager();
    CPLFreeConfig();
    CPLCleanupTLS();

    return 0;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: ogrinfo [--help-general] [-ro] [-q] [-where restricted_where]\n"
            "               [-spat xmin ymin xmax ymax] [-fid fid]\n"
            "               [-sql statement] [-al] [-so] [--formats]\n"
            "               datasource_name [layer [layer ...]]\n");
    exit( 1 );
}

/************************************************************************/
/*                           ReportOnLayer()                            */
/************************************************************************/

static void ReportOnLayer( OGRLayer * poLayer, const char *pszWHERE, 
                           OGRGeometry *poSpatialFilter )

{
    OGRFeatureDefn      *poDefn = poLayer->GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      Set filters if provided.                                        */
/* -------------------------------------------------------------------- */
    if( pszWHERE != NULL )
        poLayer->SetAttributeFilter( pszWHERE );

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );

/* -------------------------------------------------------------------- */
/*      Report various overall information.                             */
/* -------------------------------------------------------------------- */
    CPLDebug("INFO", "---------- General Layer Report ----------"); 

    printf( "\n" );
    
    printf( "Layer name: %s\n", poDefn->GetName() );

    if( bVerbose )
    {
        printf( "Geometry: %s\n", 
                OGRGeometryTypeToName( poDefn->GetGeomType() ) );
        
        printf( "Feature Count: %d\n", poLayer->GetFeatureCount() );
        
        OGREnvelope oExt;
        if (poLayer->GetExtent(&oExt, TRUE) == OGRERR_NONE)
        {
            printf("Extent: (%f, %f) - (%f, %f)\n", 
                   oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY);
        }

        char    *pszWKT;
        
        if( poLayer->GetSpatialRef() == NULL )
            pszWKT = CPLStrdup( "(unknown)" );
        else
        {
            poLayer->GetSpatialRef()->exportToPrettyWkt( &pszWKT );
        }            

        printf( "Layer SRS WKT:\n%s\n", pszWKT );
        CPLFree( pszWKT );

    
        CPLDebug("\nINFO", "---------- Geometry Column Report ----------\n"); 

        if( strlen(poLayer->GetFIDColumn()) > 0 )
            printf( "FID Column = %s\n", 
                    poLayer->GetFIDColumn() );
    
        if( strlen(poLayer->GetGeometryColumn()) > 0 )
            printf( "Geometry Column = %s\n", 
                    poLayer->GetGeometryColumn() );

        for( int iAttr = 0; iAttr < poDefn->GetFieldCount(); iAttr++ )
        {
            OGRFieldDefn    *poField = poDefn->GetFieldDefn( iAttr );
            
            printf( "%s: %s (%d.%d)\n",
                    poField->GetNameRef(),
                    poField->GetFieldTypeName( poField->GetType() ),
                    poField->GetWidth(),
                    poField->GetPrecision() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Read, and dump features.                                        */
/* -------------------------------------------------------------------- */
    CPLDebug("\nINFO", "---------- Features Dump ----------\n"); 

    OGRFeature  *poFeature = NULL;

    if( nFetchFID == OGRNullFID && !bSummaryOnly )
    {
        while( (poFeature = poLayer->GetNextFeature()) != NULL )
        {
            poFeature->DumpReadable( NULL );
            delete poFeature;
        }
    }
    else if( nFetchFID != OGRNullFID )
    {
        poFeature = poLayer->GetFeature( nFetchFID );
        if( poFeature == NULL )
        {
            printf( "Unable to locate feature id %d on this layer.\n", 
                    nFetchFID );
        }
        else
        {
            poFeature->DumpReadable( NULL );
            delete poFeature;
        }
    }
}
