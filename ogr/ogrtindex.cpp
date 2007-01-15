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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  2006/05/09 22:48:15  mloskot
 * Fixed Bug 1144 in ogrtindex utility.
 *
 * Revision 1.3  2005/07/15 15:06:52  fwarmerdam
 * Set default location field width to 200.
 *
 * Revision 1.2  2002/04/25 20:56:29  warmerda
 * expanded tabs
 *
 * Revision 1.1  2002/03/27 18:34:43  warmerda
 * New
 *
 */

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_string.h"

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
    
/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();
    
/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    for( iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-f") && iArg < nArgc-1 )
        {
            pszFormat = papszArgv[++iArg];
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
/*      Now create it.                                                  */
/* -------------------------------------------------------------------- */
        
        poDstDS = poDriver->CreateDataSource( pszOutputName, NULL );
        if( poDstDS == NULL )
        {
            printf( "%s driver failed to create %s\n", 
                    pszFormat, pszOutputName );
            exit( 1 );
        }

        if( poDstDS->GetLayerCount() == 0 )
        {
            OGRFieldDefn oLocation( pszTileIndexField, OFTString );
            
            oLocation.SetWidth( 200 );

            poDstLayer = poDstDS->CreateLayer( "tileindex" );
            poDstLayer->CreateField( &oLocation, OFTString );
        }
    }

/* -------------------------------------------------------------------- */
/*      Identify target layer and field.                                */
/* -------------------------------------------------------------------- */
    int   iTileIndexField;

    poDstLayer = poDstDS->GetLayer(0);
    if( poDstLayer == NULL )
    {
        printf( "Can't find any layer in output tileindex!\n" );
        exit( 1 );
    }

    iTileIndexField = 
        poDstLayer->GetLayerDefn()->GetFieldIndex( pszTileIndexField );
    if( iTileIndexField == -1 )
    {
        printf( "Can't find %s field in tile index dataset.\n", 
                pszTileIndexField );
        exit( 1 );
    }

/* ==================================================================== */
/*      Process each input datasource in turn.                          */
/* ==================================================================== */
	OGRFeatureDefn* poFeatureDefn = NULL;

	for(; nFirstSourceDataset < nArgc; nFirstSourceDataset++ )
    {
        OGRDataSource       *poDS;

        if( papszArgv[nFirstSourceDataset][0] == '-' )
        {
            nFirstSourceDataset++;
            continue;
        }

        poDS = OGRSFDriverRegistrar::Open( papszArgv[nFirstSourceDataset], 
                                           FALSE );

        if( poDS == NULL )
        {
            printf( "Failed to open dataset %s, skipping.\n", 
                    papszArgv[nFirstSourceDataset] );
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

/* -------------------------------------------------------------------- */
/*		Check if all layers in dataset have the same attributes	schema. */
/* -------------------------------------------------------------------- */
			if( poFeatureDefn == NULL )
			{
				poFeatureDefn = poLayer->GetLayerDefn()->Clone();
			}
			else
			{
				OGRFeatureDefn* poFeatureDefnCur = poLayer->GetLayerDefn();
				assert(NULL != poFeatureDefnCur);

				int fieldCount = poFeatureDefnCur->GetFieldCount();

				if( fieldCount != poFeatureDefn->GetFieldCount())
				{
					printf( "Number of attributes of subsequent layers does not match ... terminating.\n" );
					delete poDstDS;
					exit( 1 );
				}
				
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
						printf( "Schema of attributes of subsequent layers does not match ... terminating.\n" );
						delete poDstDS;
						exit( 1 );
					}
				}
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
                printf( "GetExtent() failed on layer %s of %s, skipping.\n", 
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
                     papszArgv[nFirstSourceDataset], iLayer );
            oTileFeat.SetGeometry( &oRegion );
            oTileFeat.SetField( iTileIndexField, szLocation );

            if( poDstLayer->CreateFeature( &oTileFeat ) != OGRERR_NONE )
            {
                printf( "Failed to create feature on tile index ... terminating." );
                delete poDstDS;
                exit( 1 );
            }
        }

/* -------------------------------------------------------------------- */
/*      Cleanup this data source.                                       */
/* -------------------------------------------------------------------- */
        delete poDS;
    }

/* -------------------------------------------------------------------- */
/*      Close tile index and clear buffers.                             */
/* -------------------------------------------------------------------- */
    delete poDstDS;
	delete poFeatureDefn;

    return 0;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: ogrtindex [-lnum n]... [-lname name]... [-f output_format]\n"
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
    printf( "\n" );
    printf( "If no -lnum or -lname arguments are given it is assumed that\n"
            "all layers in source datasets should be added to the tile index\n"
            "as independent records.\n" );
    exit( 1 );
}
