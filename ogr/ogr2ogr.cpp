/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
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
 * Revision 1.12  2001/10/25 22:33:16  danmo
 * Added support for -select option
 *
 * Revision 1.11  2001/09/27 14:50:10  warmerda
 * added untested -spat and -where support
 *
 * Revision 1.10  2001/09/21 16:19:50  warmerda
 * added support for -a_srs and -t_srs options
 *
 * Revision 1.9  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.8  2001/06/26 20:58:20  warmerda
 * added -nln switch
 *
 * Revision 1.7  2000/12/05 23:09:05  warmerda
 * improved error testing, added lots of CPLResetError calls
 *
 * Revision 1.6  2000/10/17 02:23:33  warmerda
 * improve error reporting
 *
 * Revision 1.5  2000/06/19 18:46:59  warmerda
 * added -skipfailures
 *
 * Revision 1.4  2000/06/09 21:15:19  warmerda
 * made CreateField and SetFrom forgiving
 *
 * Revision 1.3  2000/03/14 21:37:35  warmerda
 * added support for passing options to create methods
 *
 * Revision 1.2  1999/11/18 19:02:19  warmerda
 * expanded tabs
 *
 * Revision 1.1  1999/11/04 21:07:53  warmerda
 * New
 *
 */

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static void Usage();

static int TranslateLayer( OGRDataSource *poSrcDS, 
                           OGRLayer * poSrcLayer,
                           OGRDataSource *poDstDS,
                           char ** papszLSCO,
                           const char *pszNewLayerName,
                           int bTransform, 
                           OGRSpatialReference *poOutputSRS,
                           char **papszSelFields );

static int bSkipFailures = FALSE;

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    const char  *pszFormat = "ESRI Shapefile";
    const char  *pszDataSource = NULL;
    const char  *pszDestDataSource = NULL;
    char        **papszLayers = NULL;
    char        **papszDSCO = NULL, **papszLCO = NULL;
    int		bTransform = FALSE;
    const char  *pszOutputSRSDef = NULL;
    OGRSpatialReference *poOutputSRS = NULL;
    const char  *pszNewLayerName = NULL;
    const char  *pszWHERE = NULL;
    OGRGeometry *poSpatialFilter = NULL;
    const char  *pszSelect;
    char        **papszSelFields = NULL;

/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-f") && iArg < nArgc-1 )
        {
            pszFormat = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-dsco") && iArg < nArgc-1 )
        {
            papszDSCO = CSLAddString(papszDSCO, papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg],"-lco") && iArg < nArgc-1 )
        {
            papszLCO = CSLAddString(papszLCO, papszArgv[++iArg] );
        }
        else if( EQUALN(papszArgv[iArg],"-skip",5) )
        {
            bSkipFailures = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-nln") && iArg < nArgc-1 )
        {
            pszNewLayerName = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-a_srs") && iArg < nArgc-1 )
        {
            pszOutputSRSDef = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-t_srs") && iArg < nArgc-1 )
        {
            pszOutputSRSDef = papszArgv[++iArg];
            bTransform = TRUE;
        }
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
        else if( EQUAL(papszArgv[iArg],"-select") && papszArgv[iArg+1] != NULL)
        {
            pszSelect = papszArgv[++iArg];
            papszSelFields = CSLTokenizeStringComplex(pszSelect, " ,", 
                                                           FALSE, FALSE );
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage();
        }
        else if( pszDestDataSource == NULL )
            pszDestDataSource = papszArgv[iArg];
        else if( pszDataSource == NULL )
            pszDataSource = papszArgv[iArg];
        else
            papszLayers = CSLAddString( papszLayers, papszArgv[iArg] );
    }

    if( pszDataSource == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poDS;

    poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE );

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

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    OGRSFDriverRegistrar        *poR = OGRSFDriverRegistrar::GetRegistrar();
    OGRSFDriver                 *poDriver = NULL;
    int                         iDriver;

    for( iDriver = 0;
         iDriver < poR->GetDriverCount() && poDriver == NULL;
         iDriver++ )
    {
        if( EQUAL(poR->GetDriver(iDriver)->GetName(),pszFormat) )
        {
            poDriver = poR->GetDriver(iDriver);
        }
    }

    if( poDriver == NULL )
    {
        printf( "Unable to find driver `%s'.\n", pszFormat );
        printf( "The following drivers are available:\n" );
        
        for( iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            printf( "  -> `%s'\n", poR->GetDriver(iDriver)->GetName() );
        }
        exit( 1 );
    }

    if( !poDriver->TestCapability( ODrCCreateDataSource ) )
    {
        printf( "%s driver does not support data source creation.\n",
                pszFormat );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Parse the output SRS definition if possible.                    */
/* -------------------------------------------------------------------- */
    if( pszOutputSRSDef != NULL )
    {
        poOutputSRS = new OGRSpatialReference();
        if( poOutputSRS->SetFromUserInput( pszOutputSRSDef ) != OGRERR_NONE )
        {
            printf( "Failed to process SRS definition: %s\n", 
                    pszOutputSRSDef );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the output data source.                                  */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poODS;
    
    poODS = poDriver->CreateDataSource( pszDestDataSource, papszDSCO );
    if( poODS == NULL )
    {
        printf( "%s driver failed to create %s\n", 
                pszFormat, pszDestDataSource );
        exit( 1 );
    }

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

        if( CSLCount(papszLayers) == 0
            || CSLFindString( papszLayers,
                              poLayer->GetLayerDefn()->GetName() ) != -1 )
        {
            if( pszWHERE != NULL )
                poLayer->SetAttributeFilter( pszWHERE );
            
            if( poSpatialFilter != NULL )
                poLayer->SetSpatialFilter( poSpatialFilter );
            
            if( !TranslateLayer( poDS, poLayer, poODS, papszLCO, 
                                 pszNewLayerName, bTransform, poOutputSRS,
                                 papszSelFields ) 
                && !bSkipFailures )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Terminating translation prematurely after failed\n"
                          "translation of layer %s\n", 
                          poLayer->GetLayerDefn()->GetName() );

                exit( 1 );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    delete poODS;
    delete poDS;

    CSLDestroy(papszSelFields);

#ifdef DBMALLOC
    malloc_dump(1);
#endif
    
    return 0;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    OGRSFDriverRegistrar        *poR = OGRSFDriverRegistrar::GetRegistrar();

    printf( "Usage: ogr2ogr [-skipfailures] [-f format_name]\n"
            "               [-select field_list] [-where restricted_where]\n"
            "               [-spat xmin ymin xmax ymax]\n"
            "               [-t_srs srs_def] [-a_srs srs_def]\n"
            "               [[-dsco NAME=VALUE] ...] dst_datasource_name\n"
            "               src_datasource_name\n"
            "               [-lco NAME=VALUE] [-nln name] layer [layer ...]]\n"
            "\n"
            " -f format_name: output file format name, possible values are:\n");
    
    for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
    {
        OGRSFDriver *poDriver = poR->GetDriver(iDriver);

        if( poDriver->TestCapability( ODrCCreateDataSource ) )
            printf( "     -f \"%s\"\n", poDriver->GetName() );
    }

    printf( " -select field_list: Comma-delimited list of fields from input layer to\n"
            "                     copy to the new layer (defaults to all)\n" 
            " -where restricted_where: Attribute query (like SQL WHERE)\n" 
            " -spat xmin ymin xmax ymax: spatial query extents\n"
            " -dsco NAME=VALUE: Dataset creation option (format specific)\n"
            " -lco  NAME=VALUE: Layer creation option (format specific)\n"
            " -nln name: Assign an alternate name to the new layer\n" );

    printf(" -a_srs srs_def: Assign an output SRS\n"
           " -t_srs srs_def: Reproject/transform to this SRS on output\n"
           "\n" 
           " Srs_def can be a full WKT definition (hard to escape properly),\n"
           " or a well known definition (ie. EPSG:4326) or a file with a WKT\n"
           " definition.\n" );

    exit( 1 );
}

/************************************************************************/
/*                           TranslateLayer()                           */
/************************************************************************/

static int TranslateLayer( OGRDataSource *poSrcDS, 
                           OGRLayer * poSrcLayer,
                           OGRDataSource *poDstDS,
                           char **papszLCO,
                           const char *pszNewLayerName,
                           int bTransform, 
                           OGRSpatialReference *poOutputSRS,
                           char **papszSelFields )

{
    OGRLayer    *poDstLayer;
    OGRFeatureDefn *poFDefn;
    OGRErr      eErr;

    if( pszNewLayerName == NULL )
        pszNewLayerName = poSrcLayer->GetLayerDefn()->GetName();

/* -------------------------------------------------------------------- */
/*      Setup coordinate transformation if we need it.                  */
/* -------------------------------------------------------------------- */
    OGRCoordinateTransformation *poCT = NULL;

    if( bTransform )
    {
        if( poSrcLayer->GetSpatialRef() == NULL )
        {
            printf( "Can't transform coordinates, source layer has no\n"
                    "coordinate system.\n" );
            exit( 1 );
        }

        poCT = OGRCreateCoordinateTransformation( poSrcLayer->GetSpatialRef(),
                                                  poOutputSRS );
        if( poCT == NULL )
        {
            char	*pszWKT = NULL;

            printf("Failed to create coordinate transformation between the\n"
                   "following coordinate systems.  This may be because they\n"
                   "are not transformable, or because projection services\n"
                   "(PROJ.4 DLL/.so) could not be loaded.\n" );
            
            poSrcLayer->GetSpatialRef()->exportToPrettyWkt( &pszWKT, FALSE );
            printf( "Source:\n%s\n", pszWKT );
            
            poOutputSRS->exportToPrettyWkt( &pszWKT, FALSE );
            printf( "Target:\n%s\n", pszWKT );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the layer.                                               */
/* -------------------------------------------------------------------- */
    CPLAssert( poDstDS->TestCapability( ODsCCreateLayer ) );
    poFDefn = poSrcLayer->GetLayerDefn();

    if( poOutputSRS == NULL )
        poOutputSRS = poSrcLayer->GetSpatialRef();

    CPLErrorReset();

    poDstLayer = poDstDS->CreateLayer( pszNewLayerName, poOutputSRS,
                                       poFDefn->GetGeomType(), papszLCO );

    if( poDstLayer == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Add fields.  Default to copy all field.                         */
/*      If only a subset of all fields requested, then output only      */
/*      the selected fields, and in the order that they were            */
/*      selected.                                                       */
/* -------------------------------------------------------------------- */
    int         iField;
    if (papszSelFields)
    {
        for( iField=0; papszSelFields[iField] != NULL; iField++)
        {
            int iSrcField = poFDefn->GetFieldIndex(papszSelFields[iField]);
            if (iSrcField >= 0)
                poDstLayer->CreateField( poFDefn->GetFieldDefn(iSrcField) );
            else
            {
                printf( "Field '%s' not found in source layer.\n", 
                         papszSelFields[iField] );
                if( !bSkipFailures )
                    return FALSE;
            }
        }
    }
    else
    {
        for( iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
            poDstLayer->CreateField( poFDefn->GetFieldDefn(iField) );
    }

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature;
    
    poSrcLayer->ResetReading();
    
    while( (poFeature = poSrcLayer->GetNextFeature()) != NULL )
    {
        OGRFeature      *poDstFeature;

        CPLErrorReset();
        poDstFeature = new OGRFeature( poDstLayer->GetLayerDefn() );

        if( poDstFeature->SetFrom( poFeature, TRUE ) != OGRERR_NONE )
        {
            delete poFeature;
            
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to translate feature %d from layer %s.\n",
                      poFeature->GetFID(), poFDefn->GetName() );
            return FALSE;
        }
        
        if( poCT && poDstFeature->GetGeometryRef() != NULL )
        {
            eErr = poDstFeature->GetGeometryRef()->transform( poCT );
            if( eErr != OGRERR_NONE )
            {
                printf( "Failed to transform feature %d.\n", 
                        (int) poFeature->GetFID() );
                if( !bSkipFailures )
                    return FALSE;
            }
        }
        
        delete poFeature;

        CPLErrorReset();
        if( poDstLayer->CreateFeature( poDstFeature ) != OGRERR_NONE 
            && !bSkipFailures )
        {
            delete poDstFeature;
            return FALSE;
        }

        delete poDstFeature;
    }

    return TRUE;
}

