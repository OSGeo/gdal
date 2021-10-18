/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Allow a user to dissolve geometries based on an attribute.
 * Author:   Howard Butler, hobu.inc@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Howard Butler
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

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "commonutils.h"
#include <map>
#include <list>
CPL_CVSID("$Id$")

static void Usage();

static int DissolveLayer(  OGRDataSource *poSrcDS,
                           OGRLayer * poSrcLayer,
                           OGRDataSource *poDstDS,
                           char ** papszLSCO,
                           const char *pszNewLayerName,
                           int bTransform,
                           OGRSpatialReference *poOutputSRS,
                           OGRSpatialReference *poSourceSRS,
                           char **papszSelFields,
                           int bAppend, int eGType,
                           int bOverwrite );

static int bSkipFailures = FALSE;
static int nGroupTransactions = 200;
static int bPreserveFID = FALSE;
static int nFIDToFetch = OGRNullFID;

typedef std::multimap<CPLString, OGRGeometry*> StringGeometryMMap;
typedef std::map<CPLString, OGRGeometryCollection*> StringGeometryColMap;
typedef std::map<CPLString, OGRGeometry*> StringGeometryMap;
typedef std::list<OGRGeometry*> GeometriesList;

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(nArgc, papszArgv)

{
    const char  *pszFormat = "ESRI Shapefile";
    const char  *pszDataSource = NULL;
    const char  *pszDestDataSource = NULL;
    char        **papszLayers = NULL;
    char        **papszDSCO = NULL, **papszLCO = NULL;
    int         bTransform = FALSE;
    int         bAppend = FALSE, bUpdate = FALSE, bOverwrite = FALSE;
    const char  *pszOutputSRSDef = NULL;
    const char  *pszSourceSRSDef = NULL;
    OGRSpatialReference *poOutputSRS = NULL;
    OGRSpatialReference *poSourceSRS = NULL;
    const char  *pszNewLayerName = NULL;
    const char  *pszWHERE = NULL;
    OGRGeometry *poSpatialFilter = NULL;
    const char  *pszSelect;
    char        **papszSelFields = NULL;
    const char  *pszSQLStatement = NULL;
    int         eGType = -2;

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
        if( (EQUAL(papszArgv[iArg],"-f") || EQUAL(papszArgv[iArg],"-of")) && iArg < nArgc-1 )
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
        else if( EQUAL(papszArgv[iArg],"-preserve_fid") )
        {
            bPreserveFID = TRUE;
        }
        else if( STARTS_WITH_CI(papszArgv[iArg], "-skip") )
        {
            bSkipFailures = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-append") )
        {
            bAppend = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-overwrite") )
        {
            bOverwrite = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-update") )
        {
            bUpdate = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-fid") && papszArgv[iArg+1] != NULL )
        {
            nFIDToFetch = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-sql") && papszArgv[iArg+1] != NULL )
        {
            pszSQLStatement = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-nln") && iArg < nArgc-1 )
        {
            pszNewLayerName = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-nlt") && iArg < nArgc-1 )
        {
            int bIs3D = FALSE;
            CPLString osGeomName = papszArgv[iArg+1];
            if (strlen(papszArgv[iArg+1]) > 3 &&
                STARTS_WITH_CI(papszArgv[iArg+1] + strlen(papszArgv[iArg+1]) - 3, "25D"))
            {
                bIs3D = TRUE;
                osGeomName.resize(osGeomName.size() - 3);
            }
            else if (strlen(papszArgv[iArg+1]) > 1 &&
                STARTS_WITH_CI(papszArgv[iArg+1] + strlen(papszArgv[iArg+1]) - 1, "Z"))
            {
                bIs3D = TRUE;
                osGeomName.resize(osGeomName.size() - 1);
            }
            if( EQUAL(osGeomName,"NONE") )
                eGType = wkbNone;
            else if( EQUAL(osGeomName,"GEOMETRY") )
                eGType = wkbUnknown;
            else
            {
                eGType = OGRFromOGCGeomType(osGeomName);
                if (eGType == wkbUnknown)
                {
                    fprintf( stderr, "-nlt %s: type not recognised.\n",
                            papszArgv[iArg+1] );
                    exit( 1 );
                }
            }
            if (eGType != wkbNone && bIs3D)
                eGType = wkbSetZ((OGRwkbGeometryType)eGType);
            iArg++;
        }
        else if( EQUAL(papszArgv[iArg],"-tg") && iArg < nArgc-1 )
        {
            nGroupTransactions = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-s_srs") && iArg < nArgc-1 )
        {
            pszSourceSRSDef = papszArgv[++iArg];
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

            oRing.addPoint( CPLAtof(papszArgv[iArg+1]), CPLAtof(papszArgv[iArg+2]) );
            oRing.addPoint( CPLAtof(papszArgv[iArg+1]), CPLAtof(papszArgv[iArg+4]) );
            oRing.addPoint( CPLAtof(papszArgv[iArg+3]), CPLAtof(papszArgv[iArg+4]) );
            oRing.addPoint( CPLAtof(papszArgv[iArg+3]), CPLAtof(papszArgv[iArg+2]) );
            oRing.addPoint( CPLAtof(papszArgv[iArg+1]), CPLAtof(papszArgv[iArg+2]) );

            poSpatialFilter = new OGRPolygon();
            poSpatialFilter->toPolygon()->addRing( &oRing );
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
/*      Try opening the output datasource as an existing, writable      */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poODS;

    if( bUpdate )
    {
        poODS = OGRSFDriverRegistrar::Open( pszDestDataSource, TRUE );
        if( poODS == NULL )
        {
            printf( "FAILURE:\n"
                    "Unable to open existing output datasource `%s'.\n",
                    pszDestDataSource );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    else
    {
        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();
        OGRSFDriver          *poDriver = NULL;
        int                  iDriver;

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
/*      Create the output data source.                                  */
/* -------------------------------------------------------------------- */
        poODS = poDriver->CreateDataSource( pszDestDataSource, papszDSCO );
        if( poODS == NULL )
        {
            printf( "%s driver failed to create %s\n",
                    pszFormat, pszDestDataSource );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse the output SRS definition if possible.                    */
/* -------------------------------------------------------------------- */
    if( pszOutputSRSDef != NULL )
    {
        poOutputSRS = new OGRSpatialReference();
        poOutputSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( poOutputSRS->SetFromUserInput( pszOutputSRSDef ) != OGRERR_NONE )
        {
            printf( "Failed to process SRS definition: %s\n",
                    pszOutputSRSDef );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse the source SRS definition if possible.                    */
/* -------------------------------------------------------------------- */
    if( pszSourceSRSDef != NULL )
    {
        poSourceSRS = new OGRSpatialReference();
        poSourceSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( poSourceSRS->SetFromUserInput( pszSourceSRSDef ) != OGRERR_NONE )
        {
            printf( "Failed to process SRS definition: %s\n",
                    pszSourceSRSDef );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for -sql clause.  No source layers required.       */
/* -------------------------------------------------------------------- */
    if( pszSQLStatement != NULL )
    {
        OGRLayer *poResultSet;

        if( pszWHERE != NULL )
            printf( "-where clause ignored in combination with -sql.\n" );
        if( CSLCount(papszLayers) > 0 )
            printf( "layer names ignored in combination with -sql.\n" );

        poResultSet = poDS->ExecuteSQL( pszSQLStatement, poSpatialFilter,
                                        NULL );

        if( poResultSet != NULL )
        {
            if( !DissolveLayer( poDS, poResultSet, poODS, papszLCO,
                                 pszNewLayerName, bTransform, poOutputSRS,
                                 poSourceSRS, papszSelFields, bAppend, eGType,
                                 bOverwrite ))
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Terminating translation prematurely after failed\n"
                          "translation from sql statement." );

                exit( 1 );
            }
            poDS->ReleaseResultSet( poResultSet );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0;
         pszSQLStatement == NULL && iLayer < poDS->GetLayerCount();
         iLayer++ )
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

            if( !DissolveLayer( poDS, poLayer, poODS, papszLCO,
                                 pszNewLayerName, bTransform, poOutputSRS,
                                 poSourceSRS, papszSelFields, bAppend, eGType,
                                 bOverwrite )
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
    OGRSpatialReference::DestroySpatialReference( poOutputSRS );
    OGRSpatialReference::DestroySpatialReference( poSourceSRS ) ;
    OGRDataSource::DestroyDataSource( poODS );
    OGRDataSource::DestroyDataSource( poDS );

    CSLDestroy(papszSelFields);
    CSLDestroy( papszArgv );
    CSLDestroy( papszLayers );
    CSLDestroy( papszDSCO );
    CSLDestroy( papszLCO );

    OGRCleanupAll();

#ifdef DBMALLOC
    malloc_dump(1);
#endif

    return 0;
}
MAIN_END

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    OGRSFDriverRegistrar        *poR = OGRSFDriverRegistrar::GetRegistrar();

    printf( "Usage: ogr2ogr [--help-general] [-skipfailures] [-append] [-update]\n"
            "               [-select field_list] [-where restricted_where] \n"
            "               [-sql <sql statement>] \n"
            "               [-spat xmin ymin xmax ymax] [-preserve_fid] [-fid FID]\n"
            "               [-a_srs srs_def] [-t_srs srs_def] [-s_srs srs_def]\n"
            "               [-f format_name] [-overwrite] [[-dsco NAME=VALUE] ...]\n"
            "               dst_datasource_name src_datasource_name\n"
            "               [-lco NAME=VALUE] [-nln name] [-nlt type] [layer [layer ...]]\n"
            "\n"
            " -f format_name: output file format name, possible values are:\n");

    for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
    {
        OGRSFDriver *poDriver = poR->GetDriver(iDriver);

        if( poDriver->TestCapability( ODrCCreateDataSource ) )
            printf( "     -f \"%s\"\n", poDriver->GetName() );
    }

    printf( " -append: Append to existing layer instead of creating new if it exists\n"
            " -overwrite: delete the output layer and recreate it empty\n"
            " -update: Open existing output datasource in update mode\n"
            " -select field_list: Comma-delimited list of fields from input layer to\n"
            "                     copy to the new layer (defaults to all)\n"
            " -where restricted_where: Attribute query (like SQL WHERE)\n"
            " -sql statement: Execute given SQL statement and save result.\n"
            " -skipfailures: skip features or layers that fail to convert\n"
            " -spat xmin ymin xmax ymax: spatial query extents\n"
            " -dsco NAME=VALUE: Dataset creation option (format specific)\n"
            " -lco  NAME=VALUE: Layer creation option (format specific)\n"
            " -nln name: Assign an alternate name to the new layer\n"
            " -nlt type: Force a geometry type for new layer.  One of NONE, GEOMETRY,\n"
            "      POINT, LINESTRING, POLYGON, GEOMETRYCOLLECTION, MULTIPOINT, MULTILINE,\n"
            "      MULTIPOLYGON, or MULTILINESTRING.  Add \"25D\" for 3D layers.\n"
            "      Default is type of source layer.\n" );

    printf(" -a_srs srs_def: Assign an output SRS\n"
           " -t_srs srs_def: Reproject/transform to this SRS on output\n"
           " -s_srs srs_def: Override source SRS\n"
           "\n"
           " Srs_def can be a full WKT definition (hard to escape properly),\n"
           " or a well known definition (i.e. EPSG:4326) or a file with a WKT\n"
           " definition.\n" );

    exit( 1 );
}

StringGeometryMap* CollectGeometries(   OGRLayer* poSrcLayer,
                                        const char** papszFields) {

/* -------------------------------------------------------------------- */
/*      CollectGeometries returns a dictionary where the keys are the   */
/*      values in the fields that the user has selected and the values  */
/*      are a GeometryCollection of all of the geometries for records   */
/*      with that value.                                                */
/* -------------------------------------------------------------------- */

    StringGeometryMMap poGeometriesMap;

    poSrcLayer->ResetReading();

    int iField;
/* -------------------------------------------------------------------- */
/*      Get all of the features and put them in a multi map.  This may   */
/*      include values for which the selected fields is NULL.           */
/* -------------------------------------------------------------------- */

    for( auto& poFeature: poSrcLayer )
    {
        CPLString poKey("");

        for( iField=0; papszFields[iField] != NULL; iField++) {
            int nField = poFeature->GetFieldIndex(papszFields[iField]);
            poKey = poKey + poFeature->GetFieldAsString(nField);
        }

        if (poFeature->GetGeometryRef()->IsValid()) {
        poGeometriesMap.insert(std::make_pair(
                        CPLString(  poKey),
                                    poFeature->GetGeometryRef()
                                 )
                     );
        } else {
            CPLDebug("CollectGeometries", "Geometry was invalid not adding!!!!");
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop through our features map and get a unique list of field    */
/*      values.  This could be done using something other than a map    */
/*      of course, but it was convenient.                               */
/* -------------------------------------------------------------------- */

    typedef std::map<CPLString, int> StringIntMap;
    StringIntMap::const_iterator ipos;
    StringIntMap poFieldsmap;

    StringGeometryMMap::const_iterator pos;

    for (pos = poGeometriesMap.begin();
         pos != poGeometriesMap.end();
         ++pos) {

             /* we currently throw out any null field values at this time */
           //  if (!(pos->first.empty())) {
                 poFieldsmap[CPLString(pos->first.c_str())] = 1;
         //    }
    }

/* -------------------------------------------------------------------- */
/*      Make a new map of GeometryCollection for each value in the     */
/*      poFieldsmap.  This is a 1:1 relationship, and all of the        */
/*      geometries for a given field are all put into the same          */
/*      GeometryCollection.  After we build the poCollections, we will  */
/*      use GEOS' buffer(0) trick to have GEOS perform the segmentation */
/* -------------------------------------------------------------------- */

    StringGeometryColMap poCollections;

    CPLDebug("CollectGeometries", "Field map size: %d", poFieldsmap.size());

    for (ipos = poFieldsmap.begin();
         ipos != poFieldsmap.end();
         ++ipos)
         {

              CPLString fid = ipos->first;
              CPLDebug ("CollectGeometries", "First %s Second %d", ipos->first.c_str(), ipos->second);

             OGRGeometryCollection* geom = new OGRGeometryCollection;

             for (pos = poGeometriesMap.lower_bound(fid);
                  pos != poGeometriesMap.upper_bound(fid);
                  ++pos) {
                      geom->addGeometry(pos->second);
             }
             poCollections.insert(std::make_pair(fid, geom));
    }

    CPLDebug("CollectGeometries", "Geo map size: %d", poCollections.size());

/* -------------------------------------------------------------------- */
/*      Loop through our poCollections map and buffer(0) each           */
/*      GeometryCollection.  GEOS will collapse the geometries down     */
/* -------------------------------------------------------------------- */

    StringGeometryMap* buffers = new StringGeometryMap;

    StringGeometryColMap::const_iterator collections_i;

    for (   collections_i = poCollections.begin();
            collections_i != poCollections.end();
            ++collections_i){
                CPLDebug(   "CollectGeometries",
                            "poCollections Geometry size %d",
                            collections_i->second->getNumGeometries());
                OGRGeometry* buffer = collections_i->second->Buffer(0);
                buffers->insert(std::make_pair(collections_i->first, buffer));
    }

    for (collections_i = poCollections.begin();
          collections_i != poCollections.end();
          ++collections_i) {
              delete collections_i->second;
    }

    return buffers;
}

GeometriesList* FlattenGeometries(GeometriesList* input) {

    GeometriesList::const_iterator geometry_i;
    GeometriesList* output = new GeometriesList;

    CPLDebug("CollectGeometries", "Input geometries in FlattenGeometries size: %d", input->size());
    for (   geometry_i = input->begin();
            geometry_i != input->end();
            ++geometry_i){

                OGRGeometry* buffer = (*geometry_i);
                // int nGeometries = buffer->getNumGeometries();
                OGRwkbGeometryType iGType = buffer->getGeometryType();

                if (iGType == wkbPolygon) {
                    output->push_back(buffer);
                    CPLDebug(   "CollectGeometries",
                                "Collapsing wkbPolygon geometries......"
                                );
                }
                if (iGType == wkbMultiPolygon) {
                    OGRMultiPolygon* geom = buffer->toMultiPolygon();
                    for (int i=0; i< geom->getNumGeometries(); i++) {
                        OGRPolygon* g = geom->getGeometryRef(i)->toPolygon();
                        output->push_back((OGRGeometry*)g);
                    }

                    CPLDebug(   "CollectGeometries",
                                "Collapsing wkbMultiPolygon geometries......"
                                );
                }
                if (iGType == wkbGeometryCollection)
                {
                    OGRGeometryCollection* geom = buffer->toGeometryCollection();
                    GeometriesList* collection = new GeometriesList;
                    GeometriesList::const_iterator g_i;
                    for (int i=0; i< geom->getNumGeometries(); i++)
                    {
                        OGRGeometry* g = geom->getGeometryRef(i);
                        collection->push_back(g);
                    }
                    GeometriesList* collapsed = FlattenGeometries(collection);
                    for( g_i = collapsed->begin();
                         g_i != collapsed->end();
                         g_i++ )
                    {
                        output->push_back((OGRGeometry*)(*g_i));
                        CPLDebug(
                            "CollectGeometries",
                            "Collapsing wkbGeometryCollection geometries." );
                    }
                }
                // CPLDebug(   "CollectGeometries",
                //             "Buffered Geometry size %d",
                //             nGeometries);
    }

    return output;
}
/************************************************************************/
/*                           DissolveLayer()                            */
/************************************************************************/

static int DissolveLayer( OGRDataSource *poSrcDS,
                           OGRLayer * poSrcLayer,
                           OGRDataSource *poDstDS,
                           char **papszLCO,
                           const char *pszNewLayerName,
                           int bTransform,
                           OGRSpatialReference *poOutputSRS,
                           OGRSpatialReference *poSourceSRS,
                           char **papszSelFields,
                           int bAppend, int eGType, int bOverwrite )

{

    OGRLayer    *poDstLayer;
    OGRFeatureDefn *poFDefn;
    OGRErr      eErr;
    int         bForceToPolygon = FALSE;
    int         bForceToMultiPolygon = FALSE;

    if( pszNewLayerName == NULL )
        pszNewLayerName = poSrcLayer->GetLayerDefn()->GetName();

    if( wkbFlatten(eGType) == wkbPolygon )
        bForceToPolygon = TRUE;
    else if( wkbFlatten(eGType) == wkbMultiPolygon )
        bForceToMultiPolygon = TRUE;

/* -------------------------------------------------------------------- */
/*      Setup coordinate transformation if we need it.                  */
/* -------------------------------------------------------------------- */
    OGRCoordinateTransformation *poCT = NULL;

    if( bTransform )
    {
        if( poSourceSRS == NULL )
            poSourceSRS = poSrcLayer->GetSpatialRef();

        if( poSourceSRS == NULL )
        {
            printf( "Can't transform coordinates, source layer has no\n"
                    "coordinate system.  Use -s_srs to set one.\n" );
            exit( 1 );
        }

        CPLAssert( NULL != poSourceSRS );
        CPLAssert( NULL != poOutputSRS );

        poCT = OGRCreateCoordinateTransformation( poSourceSRS, poOutputSRS );
        if( poCT == NULL )
        {
            char        *pszWKT = NULL;

            printf("Failed to create coordinate transformation between the\n"
                   "following coordinate systems.  This may be because they\n"
                   "are not transformable, or because projection services\n"
                   "(PROJ.4 DLL/.so) could not be loaded.\n" );

            poSourceSRS->exportToPrettyWkt( &pszWKT, FALSE );
            printf( "Source:\n%s\n", pszWKT );

            poOutputSRS->exportToPrettyWkt( &pszWKT, FALSE );
            printf( "Target:\n%s\n", pszWKT );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Get other info.                                                 */
/* -------------------------------------------------------------------- */
    poFDefn = poSrcLayer->GetLayerDefn();

    if( poOutputSRS == NULL )
        poOutputSRS = poSrcLayer->GetSpatialRef();

/* -------------------------------------------------------------------- */
/*      Find the layer.                                                 */
/* -------------------------------------------------------------------- */
    int iLayer = -1;
    poDstLayer = NULL;

    for( iLayer = 0; iLayer < poDstDS->GetLayerCount(); iLayer++ )
    {
        OGRLayer        *poLayer = poDstDS->GetLayer(iLayer);

        if( poLayer != NULL
            && EQUAL(poLayer->GetLayerDefn()->GetName(),pszNewLayerName) )
        {
            poDstLayer = poLayer;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      If the user requested overwrite, and we have the layer in       */
/*      question we need to delete it now so it will get recreated      */
/*      (overwritten).                                                  */
/* -------------------------------------------------------------------- */
    if( poDstLayer != NULL && bOverwrite )
    {
        if( poDstDS->DeleteLayer( iLayer ) != OGRERR_NONE )
        {
            fprintf( stderr,
                     "DeleteLayer() failed when overwrite requested.\n" );
            return FALSE;
        }
        poDstLayer = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If the layer does not exist, then create it.                    */
/* -------------------------------------------------------------------- */
    if( poDstLayer == NULL )
    {
        if( eGType == -2 )
            eGType = poFDefn->GetGeomType();

        if( !poDstDS->TestCapability( ODsCCreateLayer ) )
        {
            fprintf( stderr,
              "Layer %s not found, and CreateLayer not supported by driver.",
                     pszNewLayerName );
            return FALSE;
        }

        CPLErrorReset();

        poDstLayer = poDstDS->CreateLayer( pszNewLayerName, poOutputSRS,
                                           (OGRwkbGeometryType) eGType,
                                           papszLCO );

        if( poDstLayer == NULL )
            return FALSE;

        bAppend = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we will append to it, if append was requested.        */
/* -------------------------------------------------------------------- */
    else if( !bAppend )
    {
        printf( "FAILED: Layer %s already exists, and -append not specified.\n"
                "        Consider using -append, or -overwrite.\n",
                pszNewLayerName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add fields.  Default to copy all field.                         */
/*      If only a subset of all fields requested, then output only      */
/*      the selected fields, and in the order that they were            */
/*      selected.                                                       */
/* -------------------------------------------------------------------- */
    int         iField;

    if (papszSelFields && !bAppend )
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
    else if( !bAppend )
    {
        for( iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
            poDstLayer->CreateField( poFDefn->GetFieldDefn(iField) );
    }

/* -------------------------------------------------------------------- */
/*      Transfer features.                                              */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature;
    int         nFeaturesInTransaction = 0;

    poSrcLayer->ResetReading();

    if( nGroupTransactions )
        poDstLayer->StartTransaction();

    StringGeometryMap* buffers = CollectGeometries(poSrcLayer,
                                                   (const char**)papszSelFields);

    StringGeometryMap::const_iterator buffers_i;
    GeometriesList* input = new GeometriesList;

    CPLDebug("CollectGeometries", "Buffers size: %d", buffers->size());
    for (   buffers_i = buffers->begin();
            buffers_i != buffers->end();
            ++buffers_i){
                input->push_back(buffers_i->second);
    }
    GeometriesList* geometries = FlattenGeometries(input);

    GeometriesList::const_iterator g_i;
    for (g_i=geometries->begin();
    g_i!=geometries->end();
    g_i++) {
        OGRFeature* feature = new OGRFeature(poFDefn);
        feature->SetGeometry((*g_i));
        feature->SetField("TAXDIST","fid");
        poDstLayer->CreateFeature(feature);
    }

    if( nGroupTransactions )
        poDstLayer->CommitTransaction();

      //  getGeometryType
//     if( pszNewLayerName == NULL )
//         pszNewLayerName = poSrcLayer->GetLayerDefn()->GetName();
//
//     if( wkbFlatten(eGType) == wkbPolygon )
//         bForceToPolygon = TRUE;
//     else if( wkbFlatten(eGType) == wkbMultiPolygon )
//         bForceToMultiPolygon = TRUE;
//
// /* -------------------------------------------------------------------- */
// /*      Setup coordinate transformation if we need it.                  */
// /* -------------------------------------------------------------------- */
//     OGRCoordinateTransformation *poCT = NULL;
//
//     if( bTransform )
//     {
//         if( poSourceSRS == NULL )
//             poSourceSRS = poSrcLayer->GetSpatialRef();
//
//         if( poSourceSRS == NULL )
//         {
//             printf( "Can't transform coordinates, source layer has no\n"
//                     "coordinate system.  Use -s_srs to set one.\n" );
//             exit( 1 );
//         }
//
//         CPLAssert( NULL != poSourceSRS );
//         CPLAssert( NULL != poOutputSRS );
//
//         poCT = OGRCreateCoordinateTransformation( poSourceSRS, poOutputSRS );
//         if( poCT == NULL )
//         {
//             char        *pszWKT = NULL;
//
//             printf("Failed to create coordinate transformation between the\n"
//                    "following coordinate systems.  This may be because they\n"
//                    "are not transformable, or because projection services\n"
//                    "(PROJ.4 DLL/.so) could not be loaded.\n" );
//
//             poSourceSRS->exportToPrettyWkt( &pszWKT, FALSE );
//             printf( "Source:\n%s\n", pszWKT );
//
//             poOutputSRS->exportToPrettyWkt( &pszWKT, FALSE );
//             printf( "Target:\n%s\n", pszWKT );
//             exit( 1 );
//         }
//     }
//
// /* -------------------------------------------------------------------- */
// /*      Get other info.                                                 */
// /* -------------------------------------------------------------------- */
//     poFDefn = poSrcLayer->GetLayerDefn();
//
//     if( poOutputSRS == NULL )
//         poOutputSRS = poSrcLayer->GetSpatialRef();
//
// /* -------------------------------------------------------------------- */
// /*      Find the layer.                                                 */
// /* -------------------------------------------------------------------- */
//     int iLayer = -1;
//     poDstLayer = NULL;
//
//     for( iLayer = 0; iLayer < poDstDS->GetLayerCount(); iLayer++ )
//     {
//         OGRLayer        *poLayer = poDstDS->GetLayer(iLayer);
//
//         if( poLayer != NULL
//             && EQUAL(poLayer->GetLayerDefn()->GetName(),pszNewLayerName) )
//         {
//             poDstLayer = poLayer;
//             break;
//         }
//     }
//
// /* -------------------------------------------------------------------- */
// /*      If the user requested overwrite, and we have the layer in       */
// /*      question we need to delete it now so it will get recreated      */
// /*      (overwritten).                                                  */
// /* -------------------------------------------------------------------- */
//     if( poDstLayer != NULL && bOverwrite )
//     {
//         if( poDstDS->DeleteLayer( iLayer ) != OGRERR_NONE )
//         {
//             fprintf( stderr,
//                      "DeleteLayer() failed when overwrite requested.\n" );
//             return FALSE;
//         }
//         poDstLayer = NULL;
//     }
//
// /* -------------------------------------------------------------------- */
// /*      If the layer does not exist, then create it.                    */
// /* -------------------------------------------------------------------- */
//     if( poDstLayer == NULL )
//     {
//         if( eGType == -2 )
//             eGType = poFDefn->GetGeomType();
//
//         if( !poDstDS->TestCapability( ODsCCreateLayer ) )
//         {
//             fprintf( stderr,
//               "Layer %s not found, and CreateLayer not supported by driver.",
//                      pszNewLayerName );
//             return FALSE;
//         }
//
//         CPLErrorReset();
//
//         poDstLayer = poDstDS->CreateLayer( pszNewLayerName, poOutputSRS,
//                                            (OGRwkbGeometryType) eGType,
//                                            papszLCO );
//
//         if( poDstLayer == NULL )
//             return FALSE;
//
//         bAppend = FALSE;
//     }
//
// /* -------------------------------------------------------------------- */
// /*      Otherwise we will append to it, if append was requested.        */
// /* -------------------------------------------------------------------- */
//     else if( !bAppend )
//     {
//         printf( "FAILED: Layer %s already exists, and -append not specified.\n"
//                 "        Consider using -append, or -overwrite.\n",
//                 pszNewLayerName );
//         return FALSE;
//     }
//
// /* -------------------------------------------------------------------- */
// /*      Add fields.  Default to copy all field.                         */
// /*      If only a subset of all fields requested, then output only      */
// /*      the selected fields, and in the order that they were            */
// /*      selected.                                                       */
// /* -------------------------------------------------------------------- */
//     int         iField;
//
//     if (papszSelFields && !bAppend )
//     {
//         for( iField=0; papszSelFields[iField] != NULL; iField++)
//         {
//             int iSrcField = poFDefn->GetFieldIndex(papszSelFields[iField]);
//             if (iSrcField >= 0)
//                 poDstLayer->CreateField( poFDefn->GetFieldDefn(iSrcField) );
//             else
//             {
//                 printf( "Field '%s' not found in source layer.\n",
//                         papszSelFields[iField] );
//                 if( !bSkipFailures )
//                     return FALSE;
//             }
//         }
//     }
//     else if( !bAppend )
//     {
//         for( iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
//             poDstLayer->CreateField( poFDefn->GetFieldDefn(iField) );
//     }
//
// /* -------------------------------------------------------------------- */
// /*      Transfer features.                                              */
// /* -------------------------------------------------------------------- */
//     OGRFeature  *poFeature;
//     int         nFeaturesInTransaction = 0;
//
//     poSrcLayer->ResetReading();
//
//     if( nGroupTransactions )
//         poDstLayer->StartTransaction();
//
//     while( true )
//     {
//         OGRFeature      *poDstFeature = NULL;
//
//         if( nFIDToFetch != OGRNullFID )
//         {
//             // Only fetch feature on first pass.
//             if( nFeaturesInTransaction == 0 )
//                 poFeature = poSrcLayer->GetFeature(nFIDToFetch);
//             else
//                 poFeature = NULL;
//         }
//         else
//             poFeature = poSrcLayer->GetNextFeature();
//
//         if( poFeature == NULL )
//             break;
//
//         if( ++nFeaturesInTransaction == nGroupTransactions )
//         {
//             poDstLayer->CommitTransaction();
//             poDstLayer->StartTransaction();
//             nFeaturesInTransaction = 0;
//         }
//
//         CPLErrorReset();
//            poFDefn = poSrcLayer->GetLayerDefn();
//
//         if( poDstFeature->SetFrom( poFeature, TRUE ) != OGRERR_NONE )
//         {
//             if( nGroupTransactions )
//                 poDstLayer->CommitTransaction();
//
//             CPLError( CE_Failure, CPLE_AppDefined,
//                       "Unable to translate feature %d from layer %s.\n",
//                       poFeature->GetFID(), poFDefn->GetName() );
//
//             OGRFeature::DestroyFeature( poFeature );
//             OGRFeature::DestroyFeature( poDstFeature );
//             return FALSE;
//         }
//
//         if( bPreserveFID )
//             poDstFeature->SetFID( poFeature->GetFID() );
//
//         if( poCT && poDstFeature->GetGeometryRef() != NULL )
//         {
//             eErr = poDstFeature->GetGeometryRef()->transform( poCT );
//             if( eErr != OGRERR_NONE )
//             {
//                 if( nGroupTransactions )
//                     poDstLayer->CommitTransaction();
//
//                 printf( "Failed to transform feature %d.\n",
//                         static_cast<int>(poFeature->GetFID()) );
//                 if( !bSkipFailures )
//                 {
//                     OGRFeature::DestroyFeature( poFeature );
//                     OGRFeature::DestroyFeature( poDstFeature );
//                     return FALSE;
//                 }
//             }
//         }
//
//         if( poDstFeature->GetGeometryRef() != NULL && bForceToPolygon )
//         {
//             poDstFeature->SetGeometryDirectly(
//                 OGRGeometryFactory::forceToPolygon(
//                     poDstFeature->StealGeometry() ) );
//         }
//
//         if( poDstFeature->GetGeometryRef() != NULL && bForceToMultiPolygon )
//         {
//             poDstFeature->SetGeometryDirectly(
//                 OGRGeometryFactory::forceToMultiPolygon(
//                     poDstFeature->StealGeometry() ) );
//         }
//
//         OGRFeature::DestroyFeature( poFeature );
//
//         CPLErrorReset();
//         if( poDstLayer->CreateFeature( poDstFeature ) != OGRERR_NONE
//             && !bSkipFailures )
//         {
//             if( nGroupTransactions )
//                 poDstLayer->RollbackTransaction();
//
//             OGRFeature::DestroyFeature( poDstFeature );
//             return FALSE;
//         }
//
//         OGRFeature::DestroyFeature( poDstFeature );
//     }
//
//     if( nGroupTransactions )
//         poDstLayer->CommitTransaction();
//
// /* -------------------------------------------------------------------- */
// /*      Cleaning                                                        */
// /* -------------------------------------------------------------------- */
//     delete poCT;

    return TRUE;
}
