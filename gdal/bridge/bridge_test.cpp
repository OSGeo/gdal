/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Bridge
 * Purpose:  Simple mainline for testing bridge.  This is a slightly modified
 *           apps/gdalinfo.c.
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
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gdalbridge.h"

static int 
GDALInfoReportCorner( GDALDatasetH hDataset, 
                      const char * corner_name,
                      double x, double y );

int main( int argc, char ** argv )

{
    GDALDatasetH	hDataset;
    GDALRasterBandH	hBand;
    int			i, iBand;
    double		adfGeoTransform[6];
    GDALDriverH		hDriver;
    char		**papszMetadata;
    int                 bComputeMinMax = FALSE;

    if( !GDALBridgeInitialize( "..", stderr ) )
    {
        fprintf( stderr, "Unable to intiailize GDAL bridge.\n" );
        exit( 10 );
    }

    if( argc > 1 && strcmp(argv[1],"-mm") == 0 )
    {
        bComputeMinMax = TRUE;
        argv++;
    }

    GDALAllRegister();

    hDataset = GDALOpen( argv[1], GA_ReadOnly );
    
    if( hDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        exit( 1 );
    }
    
/* -------------------------------------------------------------------- */
/*      Report general info.                                            */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDatasetDriver( hDataset );
    printf( "Driver: %s/%s\n",
            GDALGetDriverShortName( hDriver ),
            GDALGetDriverLongName( hDriver ) );

    printf( "Size is %d, %d\n",
            GDALGetRasterXSize( hDataset ), 
            GDALGetRasterYSize( hDataset ) );

/* -------------------------------------------------------------------- */
/*      Report projection.                                              */
/* -------------------------------------------------------------------- */
    if( GDALGetProjectionRef( hDataset ) != NULL )
    {
        OGRSpatialReferenceH  hSRS;
        char		      *pszProjection;

        pszProjection = (char *) GDALGetProjectionRef( hDataset );

        hSRS = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hSRS, &pszProjection ) == CE_None )
        {
            char	*pszPrettyWkt = NULL;

            OSRExportToPrettyWkt( hSRS, &pszPrettyWkt, FALSE );
            printf( "Coordinate System is:\n%s\n", pszPrettyWkt );
        }
        else
            printf( "Coordinate System is `%s'\n",
                    GDALGetProjectionRef( hDataset ) );

        OSRDestroySpatialReference( hSRS );
    }

/* -------------------------------------------------------------------- */
/*      Report Geotransform.                                            */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        printf( "Origin = (%.6f,%.6f)\n",
                adfGeoTransform[0], adfGeoTransform[3] );

        printf( "Pixel Size = (%.6f,%.6f)\n",
                adfGeoTransform[1], adfGeoTransform[5] );
    }

/* -------------------------------------------------------------------- */
/*      Report GCPs.                                                    */
/* -------------------------------------------------------------------- */
    if( GDALGetGCPCount( hDataset ) > 0 )
    {
        printf( "GCP Projection = %s\n", GDALGetGCPProjection(hDataset) );
        for( i = 0; i < GDALGetGCPCount(hDataset); i++ )
        {
            const GDAL_GCP	*psGCP;
            
            psGCP = GDALGetGCPs( hDataset ) + i;

            printf( "GCP[%3d]: Id=%s, Info=%s\n"
                    "          (%g,%g) -> (%g,%g,%g)\n", 
                    i, psGCP->pszId, psGCP->pszInfo, 
                    psGCP->dfGCPPixel, psGCP->dfGCPLine, 
                    psGCP->dfGCPX, psGCP->dfGCPY, psGCP->dfGCPZ );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report metadata.                                                */
/* -------------------------------------------------------------------- */
    papszMetadata = GDALGetMetadata( hDataset, NULL );
    if( papszMetadata != NULL )
    {
        printf( "Metadata:\n" );
        for( i = 0; papszMetadata[i] != NULL; i++ )
        {
            printf( "  %s\n", papszMetadata[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report subdatasets.                                             */
/* -------------------------------------------------------------------- */
    papszMetadata = GDALGetMetadata( hDataset, "SUBDATASETS" );
    if( papszMetadata != NULL )
    {
        printf( "Subdatasets:\n" );
        for( i = 0; papszMetadata[i] != NULL; i++ )
        {
            printf( "  %s\n", papszMetadata[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report corners.                                                 */
/* -------------------------------------------------------------------- */
    printf( "Corner Coordinates:\n" );
    GDALInfoReportCorner( hDataset, "Upper Left", 
                          0.0, 0.0 );
    GDALInfoReportCorner( hDataset, "Lower Left", 
                          0.0, GDALGetRasterYSize(hDataset));
    GDALInfoReportCorner( hDataset, "Upper Right", 
                          GDALGetRasterXSize(hDataset), 0.0 );
    GDALInfoReportCorner( hDataset, "Lower Right", 
                          GDALGetRasterXSize(hDataset), 
                          GDALGetRasterYSize(hDataset) );
    GDALInfoReportCorner( hDataset, "Center", 
                          GDALGetRasterXSize(hDataset)/2.0, 
                          GDALGetRasterYSize(hDataset)/2.0 );

/* ==================================================================== */
/*      Loop over bands.                                                */
/* ==================================================================== */
    for( iBand = 0; iBand < GDALGetRasterCount( hDataset ); iBand++ )
    {
        double      dfMin, dfMax, adfCMinMax[2], dfNoData;
        int         bGotMin, bGotMax, bGotNodata;
        int         nBlockXSize, nBlockYSize;

        hBand = GDALGetRasterBand( hDataset, iBand+1 );
        GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
        printf( "Band %d Block=%dx%d Type=%d, ColorInterp=%d\n", iBand+1,
                nBlockXSize, nBlockYSize,
                GDALGetRasterDataType(hBand),
                GDALGetRasterColorInterpretation(hBand) );

        dfMin = GDALGetRasterMinimum( hBand, &bGotMin );
        dfMax = GDALGetRasterMaximum( hBand, &bGotMax );
        printf( "  Min=%.3f/%d, Max=%.3f/%d",  dfMin, bGotMin, dfMax, bGotMax);
        
        if( bComputeMinMax )
        {
            GDALComputeRasterMinMax( hBand, TRUE, adfCMinMax );
            printf( ", Computed Min/Max=%.3f,%.3f", 
                    adfCMinMax[0], adfCMinMax[1] );
        }
        printf( "\n" );

        dfNoData = GDALGetRasterNoDataValue( hBand, &bGotNodata );
        if( bGotNodata )
        {
            printf( "  NoData Value=%g\n", dfNoData );
        }

        if( GDALGetOverviewCount(hBand) > 0 )
        {
            int		iOverview;

            printf( "  Overviews: " );
            for( iOverview = 0; 
                 iOverview < GDALGetOverviewCount(hBand);
                 iOverview++ )
            {
                GDALRasterBandH	hOverview;

                if( iOverview != 0 )
                    printf( ", " );

                hOverview = GDALGetOverview( hBand, iOverview );
                printf( "%dx%d", 
                        GDALGetRasterBandXSize( hOverview ),
                        GDALGetRasterBandYSize( hOverview ) );
            }
            printf( "\n" );
        }

        papszMetadata = GDALGetMetadata( hBand, NULL );
        if( papszMetadata != NULL )
        {
            printf( "Metadata:\n" );
            for( i = 0; papszMetadata[i] != NULL; i++ )
            {
                printf( "  %s\n", papszMetadata[i] );
            }
        }

        if( GDALGetRasterColorInterpretation(hBand) == GCI_PaletteIndex )
        {
            GDALColorTableH	hTable;
            int			i;

            hTable = GDALGetRasterColorTable( hBand );
            printf( "  Color Table (%s with %d entries)\n", 
                    GDALGetPaletteInterpretationName(
                        GDALGetPaletteInterpretation( hTable )), 
                    GDALGetColorEntryCount( hTable ) );

            for( i = 0; i < GDALGetColorEntryCount( hTable ); i++ )
            {
                GDALColorEntry	sEntry;

                GDALGetColorEntryAsRGB( hTable, i, &sEntry );
                printf( "  %3d: %d,%d,%d,%d\n", 
                        i, 
                        sEntry.c1,
                        sEntry.c2,
                        sEntry.c3,
                        sEntry.c4 );
            }
        }
    }

    GDALClose( hDataset );
    
    exit( 0 );
}

/************************************************************************/
/*                        GDALInfoReportCorner()                        */
/************************************************************************/

static int 
GDALInfoReportCorner( GDALDatasetH hDataset, 
                      const char * corner_name,
                      double x, double y )

{
    double	dfGeoX, dfGeoY;
    const char  *pszProjection;
    double	adfGeoTransform[6];
    OGRCoordinateTransformationH hTransform = NULL;
        
    printf( "%-11s ", corner_name );
    
/* -------------------------------------------------------------------- */
/*      Transform the point into georeferenced coordinates.             */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        pszProjection = GDALGetProjectionRef(hDataset);

        dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x
            + adfGeoTransform[2] * y;
        dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x
            + adfGeoTransform[5] * y;
    }

    else
    {
        printf( "(%7.1f,%7.1f)\n", x, y );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Report the georeferenced coordinates.                           */
/* -------------------------------------------------------------------- */
    if( fabs(dfGeoX) < 181 && fabs(dfGeoY) < 91 )
    {
        printf( "(%12.7f,%12.7f) ", dfGeoX, dfGeoY );

    }
    else
    {
        printf( "(%12.3f,%12.3f) ", dfGeoX, dfGeoY );
    }

/* -------------------------------------------------------------------- */
/*      Setup transformation to lat/long.                               */
/* -------------------------------------------------------------------- */
    if( pszProjection != NULL && strlen(pszProjection) > 0 )
    {
        OGRSpatialReferenceH hProj, hLatLong = NULL;

        hProj = OSRNewSpatialReference( pszProjection );
        if( hProj != NULL )
            hLatLong = OSRCloneGeogCS( hProj );

        if( hLatLong != NULL )
        {
            hTransform = OCTNewCoordinateTransformation( hProj, hLatLong );
            OSRDestroySpatialReference( hLatLong );
        }

        if( hProj != NULL )
            OSRDestroySpatialReference( hProj );
    }

/* -------------------------------------------------------------------- */
/*      Transform to latlong and report.                                */
/* -------------------------------------------------------------------- */
    if( hTransform != NULL 
        && OCTTransform(hTransform,1,&dfGeoX,&dfGeoY,NULL) )
    {
        
        printf( "(%s,", GDALDecToDMS( dfGeoX, "Long", 2 ) );
        printf( "%s)", GDALDecToDMS( dfGeoY, "Lat", 2 ) );
    }

    if( hTransform != NULL )
        OCTDestroyCoordinateTransformation( hTransform );
    
    printf( "\n" );

    return TRUE;
}

