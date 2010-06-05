/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Program to generate a UMN MapServer compatible tile index for a
 *           set of OGR data sources. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"

#include <cassert>

CPL_CVSID("$Id$");

static void Usage();

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    int   nFirstSourceDataset = -1, bLayersWildcarded = TRUE, iArg;
    const char *pszFormat = "ESRI Shapefile";
    const char *pszTileIndexField = "LOCATION";
    const char *pszOutputName = NULL;
    int write_absolute_path = FALSE;
    int skip_different_projection = FALSE;
    char* current_path = NULL;
    int accept_different_schemas = FALSE;
    int bFirstWarningForNonMatchingAttributes = TRUE;
    
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
    for( iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"-f") && iArg < nArgc-1 )
        {
            pszFormat = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-write_absolute_path"))
        {
            write_absolute_path = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-skip_different_projection"))
        {
            skip_different_projection = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-accept_different_schemas"))
        {
            accept_different_schemas = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-tileindex") && iArg < nArgc-1 )
        {
            pszTileIndexField = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-lnum") 
                 || EQUAL(papszArgv[iArg],"-lname") )
        {
            iArg++;
            bLayersWildcarded = FALSE;
        }
        else if( papszArgv[iArg][0] == '-' )
            Usage();
        else if( pszOutputName == NULL )
            pszOutputName = papszArgv[iArg];
        else if( nFirstSourceDataset == -1 )
            nFirstSourceDataset = iArg;
    }

    if( pszOutputName == NULL || nFirstSourceDataset == -1 )
        Usage();

/* -------------------------------------------------------------------- */
/*      Try to open as an existing dataset for update access.           */
/* -------------------------------------------------------------------- */
    OGRDataSource *poDstDS;
    OGRLayer *poDstLayer = NULL;

    poDstDS = OGRSFDriverRegistrar::Open( pszOutputName, TRUE );

/* -------------------------------------------------------------------- */
/*      If that failed, find the driver so we can create the tile index.*/
/* -------------------------------------------------------------------- */
    if( poDstDS == NULL )
    {
        OGRSFDriverRegistrar     *poR = OGRSFDriverRegistrar::GetRegistrar();
        OGRSFDriver              *poDriver = NULL;
        int                      iDriver;

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
            fprintf( stderr, "Unable to find driver `%s'.\n", pszFormat );
            fprintf( stderr, "The following drivers are available:\n" );
        
            for( iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf( stderr, "  -> `%s'\n", poR->GetDriver(iDriver)->GetName() );
            }
            exit( 1 );
        }

        if( !poDriver->TestCapability( ODrCCreateDataSource ) )
        {
            fprintf( stderr, "%s driver does not support data source creation.\n",
                    pszFormat );
            exit( 1 );
        }

/* -------------------------------------------------------------------- */
/*      Now create it.                                                  */
/* -------------------------------------------------------------------- */
        
        poDstDS = poDriver->CreateDataSource( pszOutputName, NULL );
        if( poDstDS == NULL )
        {
            fprintf( stderr, "%s driver failed to create %s\n", 
                    pszFormat, pszOutputName );
            exit( 1 );
        }

        if( poDstDS->GetLayerCount() == 0 )
        {
            OGRFieldDefn oLocation( pszTileIndexField, OFTString );
            
            oLocation.SetWidth( 200 );
            
            if( nFirstSourceDataset < nArgc && papszArgv[nFirstSourceDataset][0] == '-' )
            {
                nFirstSourceDataset++;
            }
            
            OGRSpatialReference* poSrcSpatialRef = NULL;
            
            /* Fetches the SRS of the first layer and use it when creating the tileindex layer */
            if (nFirstSourceDataset < nArgc)
            {
                OGRDataSource* poDS = OGRSFDriverRegistrar::Open( papszArgv[nFirstSourceDataset], 
                                           FALSE );
                                           
                if (poDS)
                {
                    int iLayer;

                    for( iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
                    {
                        int bRequested = bLayersWildcarded;
                        OGRLayer *poLayer = poDS->GetLayer(iLayer);

                        for( iArg = 1; iArg < nArgc && !bRequested; iArg++ )
                        {
                            if( EQUAL(papszArgv[iArg],"-lnum") 
                                && atoi(papszArgv[iArg+1]) == iLayer )
                                bRequested = TRUE;
                            else if( EQUAL(papszArgv[iArg],"-lname") 
                                     && EQUAL(papszArgv[iArg+1],
                                              poLayer->GetLayerDefn()->GetName()) )
                                bRequested = TRUE;
                        }

                        if( !bRequested )
                            continue;
                            
                        if ( poLayer->GetSpatialRef() )
                            poSrcSpatialRef = poLayer->GetSpatialRef()->Clone();
                        break;
                    }
                }
                
                OGRDataSource::DestroyDataSource( poDS );
            }

            poDstLayer = poDstDS->CreateLayer( "tileindex", poSrcSpatialRef );
            poDstLayer->CreateField( &oLocation, OFTString );
            
            OGRSpatialReference::DestroySpatialReference( poSrcSpatialRef );
        }
    }

/* -------------------------------------------------------------------- */
/*      Identify target layer and field.                                */
/* -------------------------------------------------------------------- */
    int   iTileIndexField;

    poDstLayer = poDstDS->GetLayer(0);
    if( poDstLayer == NULL )
    {
        fprintf( stderr, "Can't find any layer in output tileindex!\n" );
        exit( 1 );
    }

    iTileIndexField = 
        poDstLayer->GetLayerDefn()->GetFieldIndex( pszTileIndexField );
    if( iTileIndexField == -1 )
    {
        fprintf( stderr, "Can't find %s field in tile index dataset.\n", 
                pszTileIndexField );
        exit( 1 );
    }

    OGRFeatureDefn* poFeatureDefn = NULL;

    /* Load in memory existing file names in SHP */
    int nExistingLayers = 0;
    char** existingLayersTab = NULL;
    OGRSpatialReference* alreadyExistingSpatialRef = NULL;
    int alreadyExistingSpatialRefValid = FALSE;
    nExistingLayers = poDstLayer->GetFeatureCount();
    if (nExistingLayers)
    {
        int i;
        existingLayersTab = (char**)CPLMalloc(nExistingLayers * sizeof(char*));
        for(i=0;i<nExistingLayers;i++)
        {
            OGRFeature* feature = poDstLayer->GetNextFeature();
            existingLayersTab[i] = CPLStrdup(feature->GetFieldAsString( iTileIndexField));
            if (i == 0)
            {
                OGRDataSource       *poDS;
                char* filename = CPLStrdup(existingLayersTab[i]);
                int j;
                for(j=strlen(filename)-1;j>=0;j--)
                {
                    if (filename[j] == ',')
                        break;
                }
                if (j >= 0)
                {
                    int iLayer = atoi(filename + j + 1);
                    filename[j] = 0;
                    poDS = OGRSFDriverRegistrar::Open(filename, 
                                                    FALSE );
                    if (poDS)
                    {
                        OGRLayer *poLayer = poDS->GetLayer(iLayer);
                        if (poLayer)
                        {
                            alreadyExistingSpatialRefValid = TRUE;
                            alreadyExistingSpatialRef =
                                    (poLayer->GetSpatialRef()) ? poLayer->GetSpatialRef()->Clone() : NULL;
                                    
                            if (poFeatureDefn == NULL)
                                poFeatureDefn = poLayer->GetLayerDefn()->Clone();
                        }
                        OGRDataSource::DestroyDataSource( poDS );
                    }
                }
            }
        }
    }


    if (write_absolute_path)
    {
        current_path = CPLGetCurrentDir();
        if (current_path == NULL)
        {
            fprintf( stderr, "This system does not support the CPLGetCurrentDir call. "
                             "The option -write_absolute_path will have no effect\n");
            write_absolute_path = FALSE;
        }
    }
/* ==================================================================== */
/*      Process each input datasource in turn.                          */
/* ==================================================================== */

	for(; nFirstSourceDataset < nArgc; nFirstSourceDataset++ )
    {
        int i;
        OGRDataSource       *poDS;

        if( papszArgv[nFirstSourceDataset][0] == '-' )
        {
            nFirstSourceDataset++;
            continue;
        }
        
        char* fileNameToWrite;
        VSIStatBuf sStatBuf;

        if (write_absolute_path && CPLIsFilenameRelative( papszArgv[nFirstSourceDataset] ) &&
            VSIStat( papszArgv[nFirstSourceDataset], &sStatBuf ) == 0)
        {
            fileNameToWrite = CPLStrdup(CPLProjectRelativeFilename(current_path,papszArgv[nFirstSourceDataset]));
        }
        else
        {
            fileNameToWrite = CPLStrdup(papszArgv[nFirstSourceDataset]);
        }

        poDS = OGRSFDriverRegistrar::Open( papszArgv[nFirstSourceDataset], 
                                           FALSE );

        if( poDS == NULL )
        {
            fprintf( stderr, "Failed to open dataset %s, skipping.\n", 
                    papszArgv[nFirstSourceDataset] );
            CPLFree(fileNameToWrite);
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Check all layers, and see if they match requests.               */
/* -------------------------------------------------------------------- */
        int iLayer;

        for( iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
        {
            int bRequested = bLayersWildcarded;
            OGRLayer *poLayer = poDS->GetLayer(iLayer);

            for( iArg = 1; iArg < nArgc && !bRequested; iArg++ )
            {
                if( EQUAL(papszArgv[iArg],"-lnum") 
                    && atoi(papszArgv[iArg+1]) == iLayer )
                    bRequested = TRUE;
                else if( EQUAL(papszArgv[iArg],"-lname") 
                         && EQUAL(papszArgv[iArg+1],
                                  poLayer->GetLayerDefn()->GetName()) )
                    bRequested = TRUE;
            }

            if( !bRequested )
                continue;

            /* Checks that the layer is not already in tileindex */
            for(i=0;i<nExistingLayers;i++)
            {
                char        szLocation[5000];
                sprintf( szLocation, "%s,%d", 
                        fileNameToWrite, iLayer );
                if (EQUAL(szLocation, existingLayersTab[i]))
                {
                    fprintf(stderr, "Layer %d of %s is already in tileindex. Skipping it.\n",
                            iLayer, papszArgv[nFirstSourceDataset]);
                    break;
                }
            }
            if (i != nExistingLayers)
            {
                continue;
            }

            OGRSpatialReference* spatialRef = poLayer->GetSpatialRef();
            if (alreadyExistingSpatialRefValid)
            {
                if ((spatialRef != NULL && alreadyExistingSpatialRef != NULL &&
                     spatialRef->IsSame(alreadyExistingSpatialRef) == FALSE) ||
                    ((spatialRef != NULL) != (alreadyExistingSpatialRef != NULL)))
                {
                    fprintf(stderr, "Warning : layer %d of %s is not using the same projection system as "
                                "other files in the tileindex. This may cause problems when "
                                "using it in MapServer for example.%s\n", iLayer, papszArgv[nFirstSourceDataset],
                                (skip_different_projection) ? " Skipping it" : "");
                    if (skip_different_projection)
                    {
                        continue;
                    }
                }
            }
            else
            {
                alreadyExistingSpatialRefValid = TRUE;
                alreadyExistingSpatialRef = (spatialRef) ? spatialRef->Clone() : NULL;
            }

/* -------------------------------------------------------------------- */
/*		Check if all layers in dataset have the same attributes	schema. */
/* -------------------------------------------------------------------- */
			if( poFeatureDefn == NULL )
			{
				poFeatureDefn = poLayer->GetLayerDefn()->Clone();
			}
			else if ( !accept_different_schemas )
			{
				OGRFeatureDefn* poFeatureDefnCur = poLayer->GetLayerDefn();
				assert(NULL != poFeatureDefnCur);

				int fieldCount = poFeatureDefnCur->GetFieldCount();

				if( fieldCount != poFeatureDefn->GetFieldCount())
				{
					fprintf( stderr, "Number of attributes of layer %s of %s does not match ... skipping it.\n",
                             poLayer->GetLayerDefn()->GetName(), papszArgv[nFirstSourceDataset]);
                    if (bFirstWarningForNonMatchingAttributes)
                    {
                        fprintf( stderr, "Note : you can override this behaviour with -accept_different_schemas option\n"
                                         "but this may result in a tileindex incompatible with MapServer\n");
                        bFirstWarningForNonMatchingAttributes = FALSE;
                    }
					continue;
				}
				
                int bSkip = FALSE;
				for( int fn = 0; fn < poFeatureDefnCur->GetFieldCount(); fn++ )
				{
 					OGRFieldDefn* poField = poFeatureDefn->GetFieldDefn(fn);
 					OGRFieldDefn* poFieldCur = poFeatureDefnCur->GetFieldDefn(fn);

					/* XXX - Should those pointers be checked against NULL? */ 
					assert(NULL != poField);
					assert(NULL != poFieldCur);

					if( poField->GetType() != poFieldCur->GetType() 
						|| poField->GetWidth() != poFieldCur->GetWidth() 
						|| poField->GetPrecision() != poFieldCur->GetPrecision() 
						|| !EQUAL( poField->GetNameRef(), poFieldCur->GetNameRef() ) )
					{
						fprintf( stderr, "Schema of attributes of layer %s of %s does not match ... skipping it.\n",
                                 poLayer->GetLayerDefn()->GetName(), papszArgv[nFirstSourceDataset]);
                        if (bFirstWarningForNonMatchingAttributes)
                        {
                            fprintf( stderr, "Note : you can override this behaviour with -accept_different_schemas option\n"
                                             "but this may result in a tileindex incompatible with MapServer\n");
                            bFirstWarningForNonMatchingAttributes = FALSE;
                        }
                        bSkip = TRUE; 
                        break;
					}
				}
                
                if (bSkip)
                    continue;
			}


/* -------------------------------------------------------------------- */
/*      Get layer extents, and create a corresponding polygon           */
/*      geometry.                                                       */
/* -------------------------------------------------------------------- */
            OGREnvelope sExtents;
            OGRPolygon oRegion;
            OGRLinearRing oRing;

            if( poLayer->GetExtent( &sExtents, TRUE ) != OGRERR_NONE )
            {
                fprintf( stderr, "GetExtent() failed on layer %s of %s, skipping.\n", 
                        poLayer->GetLayerDefn()->GetName(), 
                        papszArgv[nFirstSourceDataset] );
                continue;
            }
            
            oRing.addPoint( sExtents.MinX, sExtents.MinY );
            oRing.addPoint( sExtents.MinX, sExtents.MaxY );
            oRing.addPoint( sExtents.MaxX, sExtents.MaxY );
            oRing.addPoint( sExtents.MaxX, sExtents.MinY );
            oRing.addPoint( sExtents.MinX, sExtents.MinY );

            oRegion.addRing( &oRing );

/* -------------------------------------------------------------------- */
/*      Add layer to tileindex.                                         */
/* -------------------------------------------------------------------- */
            char        szLocation[5000];
            OGRFeature  oTileFeat( poDstLayer->GetLayerDefn() );

            sprintf( szLocation, "%s,%d", 
                     fileNameToWrite, iLayer );
            oTileFeat.SetGeometry( &oRegion );
            oTileFeat.SetField( iTileIndexField, szLocation );

            if( poDstLayer->CreateFeature( &oTileFeat ) != OGRERR_NONE )
            {
                fprintf( stderr, "Failed to create feature on tile index ... terminating." );
                OGRDataSource::DestroyDataSource( poDstDS );
                exit( 1 );
            }
        }

/* -------------------------------------------------------------------- */
/*      Cleanup this data source.                                       */
/* -------------------------------------------------------------------- */
        CPLFree(fileNameToWrite);
        OGRDataSource::DestroyDataSource( poDS );
    }

/* -------------------------------------------------------------------- */
/*      Close tile index and clear buffers.                             */
/* -------------------------------------------------------------------- */
    OGRDataSource::DestroyDataSource( poDstDS );
	OGRFeatureDefn::DestroyFeatureDefn( poFeatureDefn );
  
    if (alreadyExistingSpatialRef != NULL)
        OGRSpatialReference::DestroySpatialReference( alreadyExistingSpatialRef );
  
    CPLFree(current_path);
    
    if (nExistingLayers)
    {
        int i;
        for(i=0;i<nExistingLayers;i++)
        {
            CPLFree(existingLayersTab[i]);
        }
        CPLFree(existingLayersTab);
    }

    OGRCleanupAll();

    return 0;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: ogrtindex [-lnum n]... [-lname name]... [-f output_format]\n"
            "                 [-write_absolute_path] [-skip_different_projection]\n"
            "                 [-accept_different_schemas]\n"
            "                 output_dataset src_dataset...\n" );
    printf( "\n" );
    printf( "  -lnum n: Add layer number 'n' from each source file\n"
            "           in the tile index.\n" );
    printf( "  -lname name: Add the layer named 'name' from each source file\n"
            "               in the tile index.\n" );
    printf( "  -f output_format: Select an output format name.  The default\n"
            "                    is to create a shapefile.\n" );
    printf( "  -tileindex field_name: The name to use for the dataset name.\n"
            "                         Defaults to LOCATION.\n" );
    printf( "  -write_absolute_path: Filenames are written with absolute paths.\n" );
    printf( "  -skip_different_projection: Only layers with same projection ref \n"
            "        as layers already inserted in the tileindex will be inserted.\n" );
    printf( "  -accept_different_schemas: by default ogrtindex checks that all layers inserted\n"
            "                             into the index have the same attribute schemas. If you\n"
            "                             specify this option, this test will be disabled. Be aware that\n"
            "                             resulting index may be incompatible with MapServer!\n" );
    printf( "\n" );
    printf( "If no -lnum or -lname arguments are given it is assumed that\n"
            "all layers in source datasets should be added to the tile index\n"
            "as independent records.\n" );
    exit( 1 );
}
