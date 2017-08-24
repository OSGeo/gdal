/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Program to generate a UMN MapServer compatible tile index for a
 *           set of OGR data sources.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_api.h"
#include "ogrsf_frmts.h"

#include <cassert>

CPL_CVSID("$Id$")

static void Usage();

typedef enum
{
    FORMAT_AUTO,
    FORMAT_WKT,
    FORMAT_EPSG,
    FORMAT_PROJ
} SrcSRSFormat;

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char **papszArgv )

{
    // Check strict compilation and runtime library version as we use C++ API.
    if( !GDAL_CHECK_VERSION(papszArgv[0]) )
        exit(1);
/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    int nFirstSourceDataset = -1;
    bool bLayersWildcarded = true;
    const char *pszFormat = "ESRI Shapefile";
    const char *pszTileIndexField = "LOCATION";
    const char *pszOutputName = NULL;
    bool write_absolute_path = false;
    bool skip_different_projection = false;
    char* current_path = NULL;
    bool accept_different_schemas = false;
    bool bFirstWarningForNonMatchingAttributes = true;
    const char *pszTargetSRS = "";
    bool bSetTargetSRS = false;
    const char* pszSrcSRSName = NULL;
    int i_SrcSRSName = -1;
    bool bSrcSRSFormatSpecified = false;
    SrcSRSFormat eSrcSRSFormat = FORMAT_AUTO;
    size_t nMaxFieldSize = 254;

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME,
                   GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( iArg < nArgc-1 && (EQUAL(papszArgv[iArg],"-f") || EQUAL(papszArgv[iArg],"-of")) )
        {
            pszFormat = papszArgv[++iArg];
            if( !EQUAL(pszFormat, "ESRI Shapefile") )
                nMaxFieldSize = 0;
        }
        else if( EQUAL(papszArgv[iArg],"-write_absolute_path"))
        {
            write_absolute_path = true;
        }
        else if( EQUAL(papszArgv[iArg],"-skip_different_projection"))
        {
            skip_different_projection = true;
        }
        else if( EQUAL(papszArgv[iArg],"-accept_different_schemas"))
        {
            accept_different_schemas = true;
        }
        else if( iArg < nArgc-1 && EQUAL(papszArgv[iArg],"-tileindex") )
        {
            pszTileIndexField = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-lnum")
                 || EQUAL(papszArgv[iArg],"-lname") )
        {
            iArg++;
            bLayersWildcarded = false;
        }
        else if( iArg < nArgc-1 && strcmp(papszArgv[iArg],"-t_srs") == 0 )
        {
            pszTargetSRS = papszArgv[++iArg];
            bSetTargetSRS = true;
        }
        else if( iArg < nArgc-1 && strcmp(papszArgv[iArg], "-src_srs_name") == 0 )
        {
            pszSrcSRSName = papszArgv[++iArg];
        }
        else if( iArg < nArgc-1 && strcmp(papszArgv[iArg], "-src_srs_format") == 0 )
        {
            const char* pszSRSFormat;
            bSrcSRSFormatSpecified = true;
            pszSRSFormat = papszArgv[++iArg];
            if( EQUAL(pszSRSFormat, "AUTO") )
                eSrcSRSFormat = FORMAT_AUTO;
            else if( EQUAL(pszSRSFormat, "WKT") )
                eSrcSRSFormat = FORMAT_WKT;
            else if( EQUAL(pszSRSFormat, "EPSG") )
                eSrcSRSFormat = FORMAT_EPSG;
            else if( EQUAL(pszSRSFormat, "PROJ") )
                eSrcSRSFormat = FORMAT_PROJ;
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

    if( bSrcSRSFormatSpecified && pszSrcSRSName == NULL )
    {
        fprintf( stderr,"-src_srs_name must be specified when -src_srs_format is "
                "specified.\n" );
        Usage();
    }

/* -------------------------------------------------------------------- */
/*      Create and validate target SRS if given.                        */
/* -------------------------------------------------------------------- */
    OGRSpatialReference* poTargetSRS = NULL;
    if( bSetTargetSRS )
    {
        if( skip_different_projection )
        {
            fprintf( stderr,
                     "Warning : -skip_different_projection does not apply "
                     "when -t_srs is requested.\n" );
        }
        poTargetSRS = new OGRSpatialReference();
        // coverity[tainted_data]
        if( poTargetSRS->SetFromUserInput( pszTargetSRS ) != CE_None )
        {
            delete poTargetSRS;
            fprintf( stderr, "Invalid target SRS `%s'.\n",
                     pszTargetSRS );
            exit(1);
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to open as an existing dataset for update access.           */
/* -------------------------------------------------------------------- */
    OGRLayer *poDstLayer = NULL;

    GDALDataset *poDstDS = reinterpret_cast<GDALDataset*>(
        OGROpen( pszOutputName, TRUE, NULL ) );

/* -------------------------------------------------------------------- */
/*      If that failed, find the driver so we can create the tile index.*/
/* -------------------------------------------------------------------- */
    if( poDstDS == NULL )
    {
        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();
        GDALDriver *poDriver = NULL;

        for( int iDriver = 0;
             iDriver < poR->GetDriverCount() && poDriver == NULL;
             iDriver++ )
        {
            if( EQUAL(poR->GetDriver(iDriver)->GetDescription(),pszFormat) )
            {
                poDriver = poR->GetDriver(iDriver);
            }
        }

        if( poDriver == NULL )
        {
            fprintf( stderr, "Unable to find driver `%s'.\n", pszFormat );
            fprintf( stderr, "The following drivers are available:\n" );

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf( stderr, "  -> `%s'\n",
                         poR->GetDriver(iDriver)->GetDescription() );
            }
            exit( 1 );
        }

        if( !CPLTestBool( CSLFetchNameValueDef(poDriver->GetMetadata(),
                                               GDAL_DCAP_CREATE, "FALSE") ) )
        {
            fprintf( stderr,
                     "%s driver does not support data source creation.\n",
                     pszFormat );
            exit( 1 );
        }

/* -------------------------------------------------------------------- */
/*      Now create it.                                                  */
/* -------------------------------------------------------------------- */

        poDstDS = poDriver->Create( pszOutputName, 0, 0, 0, GDT_Unknown, NULL );
        if( poDstDS == NULL )
        {
            fprintf( stderr, "%s driver failed to create %s\n",
                    pszFormat, pszOutputName );
            exit( 1 );
        }

        if( poDstDS->GetLayerCount() == 0 )
        {
            if( nFirstSourceDataset < nArgc &&
                papszArgv[nFirstSourceDataset][0] == '-' )
            {
                nFirstSourceDataset++;
            }

            OGRSpatialReference* poSrcSpatialRef = NULL;
            if( bSetTargetSRS )
            {
            // Fetches the SRS from target SRS (if set), or from the SRS of
            // the first layer and use it when creating the
            // tileindex layer.
                poSrcSpatialRef = poTargetSRS->Clone();
            }
            else if( nFirstSourceDataset < nArgc )
            {
                GDALDataset* poDS = reinterpret_cast<GDALDataset*>(
                    OGROpen(papszArgv[nFirstSourceDataset], FALSE, NULL));
                if( poDS != NULL )
                {
                    for( int iLayer = 0;
                         iLayer < poDS->GetLayerCount();
                         iLayer++ )
                    {
                        bool bRequested = bLayersWildcarded;
                        OGRLayer *poLayer = poDS->GetLayer(iLayer);

                        for( int iArg = 1; iArg < nArgc && !bRequested; iArg++ )
                        {
                            if( EQUAL(papszArgv[iArg],"-lnum")
                                && atoi(papszArgv[iArg+1]) == iLayer )
                                bRequested = true;
                            else if( EQUAL(papszArgv[iArg],"-lname") &&
                                     EQUAL(papszArgv[iArg+1],
                                           poLayer->GetLayerDefn()->GetName()) )
                                bRequested = true;
                        }

                        if( !bRequested )
                            continue;

                        if( poLayer->GetSpatialRef() )
                            poSrcSpatialRef = poLayer->GetSpatialRef()->Clone();
                        break;
                    }
                }

                GDALClose(poDS);
            }

            poDstLayer = poDstDS->CreateLayer( "tileindex", poSrcSpatialRef );

            OGRFieldDefn oLocation( pszTileIndexField, OFTString );
            oLocation.SetWidth( 200 );
            poDstLayer->CreateField( &oLocation );

            if( pszSrcSRSName != NULL )
            {
                OGRFieldDefn oSrcSRSNameField( pszSrcSRSName, OFTString );
                poDstLayer->CreateField( &oSrcSRSNameField );
            }

            OGRSpatialReference::DestroySpatialReference( poSrcSpatialRef );
        }
    }

/* -------------------------------------------------------------------- */
/*      Identify target layer and field.                                */
/* -------------------------------------------------------------------- */

    poDstLayer = poDstDS->GetLayer(0);
    if( poDstLayer == NULL )
    {
        fprintf( stderr, "Can't find any layer in output tileindex!\n" );
        exit( 1 );
    }

    int iTileIndexField =
        poDstLayer->GetLayerDefn()->GetFieldIndex( pszTileIndexField );
    if( iTileIndexField == -1 )
    {
        fprintf( stderr, "Can't find %s field in tile index dataset.\n",
                pszTileIndexField );
        exit( 1 );
    }

    if( pszSrcSRSName != NULL )
        i_SrcSRSName = poDstLayer->GetLayerDefn()->GetFieldIndex( pszSrcSRSName );

    OGRFeatureDefn* poFeatureDefn = NULL;

    // Load in memory existing file names in SHP.
    char **existingLayersTab = NULL;
    OGRSpatialReference* alreadyExistingSpatialRef = NULL;
    bool alreadyExistingSpatialRefValid = false;
    const int nExistingLayers = static_cast<int>(poDstLayer->GetFeatureCount());
    if( nExistingLayers )
    {
        existingLayersTab = static_cast<char **>(
            CPLMalloc(nExistingLayers * sizeof(char*)));
        for( int i = 0; i < nExistingLayers; i++ )
        {
            OGRFeature* feature = poDstLayer->GetNextFeature();
            existingLayersTab[i] =
                CPLStrdup(feature->GetFieldAsString( iTileIndexField));
            if( i == 0 )
            {
                char* filename = CPLStrdup(existingLayersTab[i]);
                // j used after for.
                int j = static_cast<int>(strlen(filename)) - 1;
                for( ; j >= 0; j-- )
                {
                    if( filename[j] == ',' )
                        break;
                }
                GDALDataset *poDS = NULL;
                if( j >= 0 )
                {
                    const int iLayer = atoi(filename + j + 1);
                    filename[j] = 0;
                    poDS = reinterpret_cast<GDALDataset *>(
                        OGROpen(filename, FALSE, NULL));
                    if( poDS != NULL )
                    {
                        OGRLayer *poLayer = poDS->GetLayer(iLayer);
                        if( poLayer )
                        {
                            alreadyExistingSpatialRefValid = true;
                            alreadyExistingSpatialRef =
                                poLayer->GetSpatialRef() ?
                                poLayer->GetSpatialRef()->Clone() : NULL;

                            if( poFeatureDefn == NULL )
                                poFeatureDefn = poLayer->GetLayerDefn()->Clone();
                        }
                        GDALClose( poDS );
                    }
                }
            }
        }
    }

    if( write_absolute_path )
    {
        current_path = CPLGetCurrentDir();
        if( current_path == NULL )
        {
            fprintf( stderr,
                     "This system does not support the CPLGetCurrentDir call. "
                     "The option -write_absolute_path will have no effect\n" );
            write_absolute_path = false;
        }
    }
/* ==================================================================== */
/*      Process each input datasource in turn.                          */
/* ==================================================================== */
    for( ; nFirstSourceDataset < nArgc; nFirstSourceDataset++ )
    {
        if( papszArgv[nFirstSourceDataset][0] == '-' )
        {
            nFirstSourceDataset++;
            continue;
        }

        char* fileNameToWrite = NULL;
        VSIStatBuf sStatBuf;

        if( write_absolute_path &&
            CPLIsFilenameRelative( papszArgv[nFirstSourceDataset] ) &&
            VSIStat( papszArgv[nFirstSourceDataset], &sStatBuf ) == 0 )
        {
            fileNameToWrite =
                CPLStrdup(CPLProjectRelativeFilename(
                    current_path, papszArgv[nFirstSourceDataset]));
        }
        else
        {
            fileNameToWrite = CPLStrdup(papszArgv[nFirstSourceDataset]);
        }

        GDALDataset *poDS = reinterpret_cast<GDALDataset*>(
            OGROpen( papszArgv[nFirstSourceDataset], FALSE, NULL ) );

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
        for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
        {
            bool bRequested = bLayersWildcarded;
            OGRLayer *poLayer = poDS->GetLayer(iLayer);

            for( int iArg = 1; iArg < nArgc && !bRequested; iArg++ )
            {
                if( EQUAL(papszArgv[iArg],"-lnum")
                    && atoi(papszArgv[iArg+1]) == iLayer )
                    bRequested = true;
                else if( EQUAL(papszArgv[iArg], "-lname" )
                         && EQUAL(papszArgv[iArg+1],
                                  poLayer->GetLayerDefn()->GetName()) )
                    bRequested = true;
            }

            if( !bRequested )
                continue;

            // Checks that the layer is not already in tileindex.
            int i = 0;  // Used after for.
            for( ; i < nExistingLayers; i++ )
            {
                char szLocation[5000] = {};
                snprintf( szLocation, sizeof(szLocation), "%s,%d",
                          fileNameToWrite, iLayer );
                if( EQUAL(szLocation, existingLayersTab[i]) )
                {
                    fprintf(stderr, "Layer %d of %s is already in tileindex. "
                            "Skipping it.\n",
                            iLayer, papszArgv[nFirstSourceDataset]);
                    break;
                }
            }
            if( i != nExistingLayers )
            {
                continue;
            }

            OGRSpatialReference* spatialRef = poLayer->GetSpatialRef();
            // If not set target srs, test that the current file uses same
            // projection as others.
            if( !bSetTargetSRS )
            {
                if( alreadyExistingSpatialRefValid )
                {
                    if( (spatialRef != NULL && alreadyExistingSpatialRef != NULL &&
                        spatialRef->IsSame(alreadyExistingSpatialRef) == FALSE) ||
                        ((spatialRef != NULL) !=
                        (alreadyExistingSpatialRef != NULL)) )
                    {
                        fprintf(
                            stderr,
                            "Warning : layer %d of %s is not using the same "
                            "projection system as other files in the tileindex. "
                            "This may cause problems when using it in MapServer "
                            "for example.%s\n",
                            iLayer, papszArgv[nFirstSourceDataset],
                            skip_different_projection ? " Skipping it" : "");
                        if( skip_different_projection )
                        {
                            continue;
                        }
                    }
                }
                else
                {
                    alreadyExistingSpatialRefValid = true;
                    alreadyExistingSpatialRef =
                        spatialRef ? spatialRef->Clone() : NULL;
                }
            }

/* -------------------------------------------------------------------- */
/*      Check if all layers in dataset have the same attributes schema. */
/* -------------------------------------------------------------------- */
            if( poFeatureDefn == NULL )
            {
                poFeatureDefn = poLayer->GetLayerDefn()->Clone();
            }
            else if( !accept_different_schemas )
            {
                OGRFeatureDefn* poFeatureDefnCur = poLayer->GetLayerDefn();
                assert(NULL != poFeatureDefnCur);

                const int fieldCount = poFeatureDefnCur->GetFieldCount();

                if( fieldCount != poFeatureDefn->GetFieldCount())
                {
                    fprintf( stderr, "Number of attributes of layer %s of %s "
                             "does not match ... skipping it.\n",
                             poLayer->GetLayerDefn()->GetName(),
                             papszArgv[nFirstSourceDataset]);
                    if( bFirstWarningForNonMatchingAttributes )
                    {
                        fprintf(
                            stderr, "Note : you can override this "
                            "behaviour with -accept_different_schemas option\n"
                            "but this may result in a tileindex incompatible "
                            "with MapServer\n");
                        bFirstWarningForNonMatchingAttributes = false;
                    }
                    continue;
                }

                bool bSkip = false;
                for( int fn = 0; fn < poFeatureDefnCur->GetFieldCount(); fn++ )
                {
                    OGRFieldDefn* poField = poFeatureDefn->GetFieldDefn(fn);
                    OGRFieldDefn* poFieldCur =
                        poFeatureDefnCur->GetFieldDefn(fn);

                    // XXX - Should those pointers be checked against NULL?
                    assert(NULL != poField);
                    assert(NULL != poFieldCur);

                    if( poField->GetType() != poFieldCur->GetType()
                        || poField->GetWidth() != poFieldCur->GetWidth()
                        || poField->GetPrecision() != poFieldCur->GetPrecision()
                        || !EQUAL( poField->GetNameRef(),
                                   poFieldCur->GetNameRef() ) )
                    {
                        fprintf(
                            stderr, "Schema of attributes of layer %s of %s "
                            "does not match. Skipping it.\n",
                            poLayer->GetLayerDefn()->GetName(),
                            papszArgv[nFirstSourceDataset]);
                        if( bFirstWarningForNonMatchingAttributes )
                        {
                            fprintf(
                                stderr, "Note : you can override this "
                                "behaviour with -accept_different_schemas "
                                "option,\nbut this may result in a tileindex "
                                "incompatible with MapServer\n");
                            bFirstWarningForNonMatchingAttributes = false;
                        }
                        bSkip = true;
                        break;
                    }
                }

                if( bSkip )
                    continue;
            }

/* -------------------------------------------------------------------- */
/*      Get layer extents, and create a corresponding polygon           */
/*      geometry.                                                       */
/* -------------------------------------------------------------------- */
            OGREnvelope sExtents;

            if( poLayer->GetExtent( &sExtents, TRUE ) != OGRERR_NONE )
            {
                fprintf( stderr,
                         "GetExtent() failed on layer %s of %s, skipping.\n",
                         poLayer->GetLayerDefn()->GetName(),
                         papszArgv[nFirstSourceDataset] );
                continue;
            }

            OGRLinearRing oRing;
            oRing.addPoint( sExtents.MinX, sExtents.MinY );
            oRing.addPoint( sExtents.MinX, sExtents.MaxY );
            oRing.addPoint( sExtents.MaxX, sExtents.MaxY );
            oRing.addPoint( sExtents.MaxX, sExtents.MinY );
            oRing.addPoint( sExtents.MinX, sExtents.MinY );

            OGRPolygon oRegion;
            oRegion.addRing( &oRing );

            // If set target srs, do the forward transformation of all points.
            if( bSetTargetSRS && spatialRef != NULL )
            {
                OGRCoordinateTransformation* poCT = NULL;
                if( !spatialRef->IsSame( poTargetSRS ) )
                {
                    poCT = OGRCreateCoordinateTransformation( spatialRef, poTargetSRS );
                    if( poCT == NULL || oRegion.transform(poCT) == OGRERR_FAILURE )
                    {
                        char* pszSourceWKT = NULL;
                        spatialRef->exportToWkt(&pszSourceWKT);
                        fprintf(
                            stderr,
                            "Warning : unable to transform points from source "
                            "SRS `%s' to target SRS `%s'\n"
                            "for file `%s' - file skipped\n",
                            pszSourceWKT, pszTargetSRS, papszArgv[nFirstSourceDataset] );
                        CPLFree(pszSourceWKT);
                        delete poCT;
                        continue;
                    }
                    delete poCT;
                }
            }

/* -------------------------------------------------------------------- */
/*      Add layer to tileindex.                                         */
/* -------------------------------------------------------------------- */
            OGRFeature  oTileFeat( poDstLayer->GetLayerDefn() );

            char szLocation[5000] = {};
            snprintf( szLocation, sizeof(szLocation), "%s,%d",
                      fileNameToWrite, iLayer );
            oTileFeat.SetGeometry( &oRegion );
            oTileFeat.SetField( iTileIndexField, szLocation );

            if( i_SrcSRSName >= 0 && spatialRef != NULL )
            {
                const char* pszAuthorityCode =
                    spatialRef->GetAuthorityCode(NULL);
                const char* pszAuthorityName =
                    spatialRef->GetAuthorityName(NULL);
                char* pszWKT = NULL;
                spatialRef->exportToWkt(&pszWKT);
                if( eSrcSRSFormat == FORMAT_AUTO )
                {
                    if( pszAuthorityName != NULL && pszAuthorityCode != NULL )
                    {
                        oTileFeat.SetField(i_SrcSRSName,
                            CPLSPrintf("%s:%s",
                                    pszAuthorityName, pszAuthorityCode) );
                    }
                    else if( nMaxFieldSize == 0 ||
                            strlen(pszWKT) <= nMaxFieldSize )
                    {
                        oTileFeat.SetField(i_SrcSRSName, pszWKT);
                    }
                    else
                    {
                        char* pszProj4 = NULL;
                        if( spatialRef->exportToProj4(&pszProj4) == OGRERR_NONE )
                        {
                            oTileFeat.SetField(i_SrcSRSName, pszProj4 );
                            CPLFree(pszProj4);
                        }
                        else
                        {
                            oTileFeat.SetField(i_SrcSRSName, pszWKT);
                        }
                    }
                }
                else if( eSrcSRSFormat == FORMAT_WKT )
                {
                    if( nMaxFieldSize == 0 ||
                        strlen(pszWKT) <= nMaxFieldSize )
                    {
                        oTileFeat.SetField(i_SrcSRSName, pszWKT );
                    }
                    else
                    {
                        fprintf(stderr,
                                "Cannot write WKT for file %s as it is too long!\n",
                                fileNameToWrite);
                    }
                }
                else if( eSrcSRSFormat == FORMAT_PROJ )
                {
                    char* pszProj4 = NULL;
                    if( spatialRef->exportToProj4(&pszProj4) == OGRERR_NONE )
                    {
                        oTileFeat.SetField(i_SrcSRSName, pszProj4 );
                        CPLFree(pszProj4);
                    }
                }
                else if( eSrcSRSFormat == FORMAT_EPSG )
                {
                    if( pszAuthorityName != NULL && pszAuthorityCode != NULL )
                        oTileFeat.SetField(i_SrcSRSName,
                            CPLSPrintf("%s:%s",
                                    pszAuthorityName, pszAuthorityCode) );
                }
                CPLFree(pszWKT);
            }
            if( poDstLayer->CreateFeature( &oTileFeat ) != OGRERR_NONE )
            {
                fprintf( stderr,
                         "Failed to create feature on tile index. "
                         "Terminating." );
                GDALClose( poDstDS );
                exit( 1 );
            }
        }

/* -------------------------------------------------------------------- */
/*      Cleanup this data source.                                       */
/* -------------------------------------------------------------------- */
        CPLFree(fileNameToWrite);
        GDALClose( poDS );
    }

/* -------------------------------------------------------------------- */
/*      Close tile index and clear buffers.                             */
/* -------------------------------------------------------------------- */
    GDALClose( poDstDS );
    OGRFeatureDefn::DestroyFeatureDefn( poFeatureDefn );

    if( alreadyExistingSpatialRef != NULL )
        OGRSpatialReference::DestroySpatialReference(
            alreadyExistingSpatialRef );
    delete poTargetSRS;

    CPLFree(current_path);

    if( nExistingLayers )
    {
        for( int i = 0; i < nExistingLayers; i++ )
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
            "                 [-t_srs target_srs]\n"
            "                 [-src_srs_name field_name] [-src_srs_format [AUTO|WKT|EPSG|PROJ]\n"
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
    printf(
            "  - If -t_srs is specified, geometries of input files will be transformed to the desired\n"
            "    target coordinate reference system.\n"
            "    Note that using this option generates files that are NOT compatible with MapServer < 7.2.\n"
            "  - Simple rectangular polygons are generated in the same coordinate reference system\n"
            "    as the vectors, or in target reference system if the -t_srs option is used.\n");
    printf( "\n" );
    printf( "If no -lnum or -lname arguments are given it is assumed that\n"
            "all layers in source datasets should be added to the tile index\n"
            "as independent records.\n" );
    exit( 1 );
}
