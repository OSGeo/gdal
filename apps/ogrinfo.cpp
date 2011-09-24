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
 ****************************************************************************/

#include "ogr_api.h"
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
char**  papszOptions = NULL;

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
    int          nRet = 0;
    
    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(papszArgv[0]))
        exit(1);
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
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"-ro") )
            bReadOnly = TRUE;
        else if( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet"))
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
        else if( EQUALN(papszArgv[iArg],"-fields=", strlen("-fields=")) )
        {
            char* pszTemp = (char*)CPLMalloc(32 + strlen(papszArgv[iArg]));
            sprintf(pszTemp, "DISPLAY_FIELDS=%s", papszArgv[iArg] + strlen("-fields="));
            papszOptions = CSLAddString(papszOptions, pszTemp);
            CPLFree(pszTemp);
        }
        else if( EQUALN(papszArgv[iArg],"-geom=", strlen("-geom=")) )
        {
            char* pszTemp = (char*)CPLMalloc(32 + strlen(papszArgv[iArg]));
            sprintf(pszTemp, "DISPLAY_GEOMETRY=%s", papszArgv[iArg] + strlen("-geom="));
            papszOptions = CSLAddString(papszOptions, pszTemp);
            CPLFree(pszTemp);
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

        nRet = 1;
        goto end;
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
        OGRLayer *poResultSet = NULL;

        nRepeatCount = 0;  // skip layer reporting.

        if( CSLCount(papszLayers) > 0 )
            printf( "layer names ignored in combination with -sql.\n" );
        
        poResultSet = poDS->ExecuteSQL( pszSQLStatement, poSpatialFilter, 
                                        pszDialect );

        if( poResultSet != NULL )
        {
            if( pszWHERE != NULL )
            {
                if (poResultSet->SetAttributeFilter( pszWHERE ) != OGRERR_NONE )
                {
                    printf( "FAILURE: SetAttributeFilter(%s) failed.\n", pszWHERE );
                    exit(1);
                }
            }

            ReportOnLayer( poResultSet, NULL, NULL );
            poDS->ReleaseResultSet( poResultSet );
        }
    }

    CPLDebug( "OGR", "GetLayerCount() = %d\n", poDS->GetLayerCount() );

    for( int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++ )
    {
        if ( CSLCount(papszLayers) == 0 )
        {
/* -------------------------------------------------------------------- */ 
/*      Process each data source layer.                                 */ 
/* -------------------------------------------------------------------- */ 
            for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == NULL )
                {
                    printf( "FAILURE: Couldn't fetch advertised layer %d!\n",
                            iLayer );
                    exit( 1 );
                }

                if (!bAllLayers)
                {
                    printf( "%d: %s",
                            iLayer+1,
                            poLayer->GetName() );

                    if( poLayer->GetGeomType() != wkbUnknown )
                        printf( " (%s)", 
                                OGRGeometryTypeToName( 
                                    poLayer->GetGeomType() ) );

                    printf( "\n" );
                }
                else
                {
                    if( iRepeat != 0 )
                        poLayer->ResetReading();

                    ReportOnLayer( poLayer, pszWHERE, poSpatialFilter );
                }
            }
        }
        else
        {
/* -------------------------------------------------------------------- */ 
/*      Process specified data source layers.                           */ 
/* -------------------------------------------------------------------- */ 
            char** papszIter = papszLayers;
            for( ; *papszIter != NULL; papszIter++ )
            {
                OGRLayer        *poLayer = poDS->GetLayerByName(*papszIter);

                if( poLayer == NULL )
                {
                    printf( "FAILURE: Couldn't fetch requested layer %s!\n",
                            *papszIter );
                    exit( 1 );
                }

                if( iRepeat != 0 )
                    poLayer->ResetReading();

                ReportOnLayer( poLayer, pszWHERE, poSpatialFilter );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
end:
    CSLDestroy( papszArgv );
    CSLDestroy( papszLayers );
    CSLDestroy( papszOptions );
    OGRDataSource::DestroyDataSource( poDS );
    if (poSpatialFilter)
        OGRGeometryFactory::destroyGeometry( poSpatialFilter );

    OGRCleanupAll();

    return nRet;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: ogrinfo [--help-general] [-ro] [-q] [-where restricted_where]\n"
            "               [-spat xmin ymin xmax ymax] [-fid fid]\n"
            "               [-sql statement] [-dialect sql_dialect] [-al] [-so] [-fields={YES/NO}]\n"
            "               [-geom={YES/NO/SUMMARY}][--formats]\n"
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
    {
        if (poLayer->SetAttributeFilter( pszWHERE ) != OGRERR_NONE )
        {
            printf( "FAILURE: SetAttributeFilter(%s) failed.\n", pszWHERE );
            exit(1);
        }
    }

    if( poSpatialFilter != NULL )
        poLayer->SetSpatialFilter( poSpatialFilter );

/* -------------------------------------------------------------------- */
/*      Report various overall information.                             */
/* -------------------------------------------------------------------- */
    printf( "\n" );
    
    printf( "Layer name: %s\n", poLayer->GetName() );

    if( bVerbose )
    {
        printf( "Geometry: %s\n", 
                OGRGeometryTypeToName( poLayer->GetGeomType() ) );
        
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
    OGRFeature  *poFeature = NULL;

    if( nFetchFID == OGRNullFID && !bSummaryOnly )
    {
        while( (poFeature = poLayer->GetNextFeature()) != NULL )
        {
            poFeature->DumpReadable( NULL, papszOptions );
            OGRFeature::DestroyFeature( poFeature );
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
            poFeature->DumpReadable( NULL, papszOptions );
            OGRFeature::DestroyFeature( poFeature );
        }
    }
}
