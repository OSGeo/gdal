/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline application to list info about a file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * ****************************************************************************
 *
 * $Log$
 * Revision 1.45  2006/11/08 16:34:55  fwarmerdam
 * Added -stats to usage message.
 *
 * Revision 1.44  2006/06/08 16:40:53  fwarmerdam
 * Added support for reporting extra metadata domains
 *
 * Revision 1.43  2006/03/21 21:34:43  fwarmerdam
 * cleanup headers
 *
 * Revision 1.42  2005/12/21 05:31:56  fwarmerdam
 * added reporting of band image_structure metadata
 *
 * Revision 1.41  2005/12/21 00:38:05  fwarmerdam
 * Report image_structure metadata.
 *
 * Revision 1.40  2005/10/11 11:29:09  dron
 * Include cpl_multiproc.h for CPLCleanupTLS() function.
 *
 * Revision 1.39  2005/09/11 17:18:33  fwarmerdam
 * added CPLCleanupTLS()
 *
 * Revision 1.38  2005/07/29 04:27:42  fwarmerdam
 * Don't crash if a "paletted" band has no color table object.
 *
 * Revision 1.37  2005/05/11 14:39:43  fwarmerdam
 * added option to report stats
 *
 * Revision 1.36  2004/08/24 20:13:28  warmerda
 * ensure -nomd works for bands too
 *
 * Revision 1.35  2004/08/09 14:39:07  warmerda
 * added shared list dump
 *
 * Revision 1.34  2004/04/29 13:44:25  warmerda
 * added raster category and scale/offset support
 *
 * Revision 1.33  2004/04/13 15:09:41  warmerda
 * clean up arguments
 *
 * Revision 1.32  2004/04/02 17:33:22  warmerda
 * added GDALGeneralCmdLineProcessor()
 *
 * Revision 1.31  2004/03/19 06:30:06  warmerda
 * Changed to compute exact min/max for -mm instead of approximate.
 *
 * Revision 1.30  2003/11/24 17:46:58  warmerda
 * Report full geotransform when image is not northup.
 *
 * Revision 1.29  2003/05/01 13:16:37  warmerda
 * dont generate error reports if instantiating coordinate transform fails
 *
 * Revision 1.28  2003/04/22 16:02:08  dron
 * New switches: -nogcp and -nomd to suppress printing out GCPs list and metadata
 * strings respectively.
 *
 * Revision 1.27  2002/09/20 14:32:15  warmerda
 * don't build gdalinfo statically any more
 *
 * Revision 1.26  2002/09/04 06:53:42  warmerda
 * added GDALDestroyDriverManager() for cleanup
 *
 * Revision 1.25  2002/05/29 15:53:14  warmerda
 * added GDALDumpOpenDatasets
 *
 * Revision 1.24  2002/04/16 14:00:25  warmerda
 * added GDALVersionInfo
 *
 * Revision 1.23  2002/03/25 13:50:07  warmerda
 * only report min/max values if fetch successfully
 *
 * Revision 1.22  2002/01/13 01:42:37  warmerda
 * add -sample test
 *
 * Revision 1.21  2001/11/17 21:40:58  warmerda
 * converted to use OGR projection services
 *
 * Revision 1.20  2001/11/02 22:21:36  warmerda
 * fixed memory leak
 *
 * Revision 1.19  2001/07/18 05:05:12  warmerda
 * added CPL_CSVID
 *
 * Revision 1.18  2001/07/05 13:12:40  warmerda
 * added UnitType support
 *
 * Revision 1.17  2001/06/28 19:40:12  warmerda
 * added subdatset reporting
 *
 * Revision 1.16  2000/11/29 20:52:53  warmerda
 * Add pretty printing of projection.
 *
 * Revision 1.15  2000/08/25 14:26:02  warmerda
 * added nodata, and arbitrary overview reporting
 *
 * Revision 1.14  2000/06/12 14:21:43  warmerda
 * Fixed min/max printf.
 *
 * Revision 1.13  2000/05/15 14:06:26  warmerda
 * Added -mm for computing min/max, and fixed ovewriting of i.
 *
 * Revision 1.12  2000/04/21 21:57:33  warmerda
 * updated the way metadata is handled
 *
 * Revision 1.11  2000/03/31 13:43:21  warmerda
 * added code to report all corners and gcps
 *
 * Revision 1.10  2000/03/29 15:33:32  warmerda
 * added block size
 *
 * Revision 1.9  2000/03/06 21:50:37  warmerda
 * added min/max support
 *
 * Revision 1.8  2000/03/06 02:18:13  warmerda
 * added overviews, and colour table
 *
 * Revision 1.6  1999/12/30 02:40:17  warmerda
 * Report driver used.
 *
 * Revision 1.5  1999/10/21 13:22:59  warmerda
 * Print band type symbolically rather than numerically.
 *
 * Revision 1.4  1999/10/01 14:45:14  warmerda
 * prettied up
 *
 * Revision 1.3  1999/03/02 21:12:01  warmerda
 * add DMS reporting of lat/long
 *
 * Revision 1.2  1999/01/11 15:27:59  warmerda
 * Add projection support
 *
 * Revision 1.1  1998/12/03 18:35:06  warmerda
 * New
 *
 */

#include "gdal.h"
#include "ogr_srs_api.h"
#include "cpl_string.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

static int 
GDALInfoReportCorner( GDALDatasetH hDataset, 
                      const char * corner_name,
                      double x, double y );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage()

{
    printf( "Usage: gdalinfo [--help-general] [-mm] [-stats] [-nogcp] [-nomd]\n"
            "                [-mdd domain]* datasetname\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    GDALDatasetH	hDataset;
    GDALRasterBandH	hBand;
    int			i, iBand;
    double		adfGeoTransform[6];
    GDALDriverH		hDriver;
    char		**papszMetadata;
    int                 bComputeMinMax = FALSE, bSample = FALSE;
    int                 bShowGCPs = TRUE, bShowMetadata = TRUE ;
    int                 bStats = FALSE, iMDD;
    const char          *pszFilename = NULL;
    char              **papszExtraMDDomains = NULL;

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "-mm") )
            bComputeMinMax = TRUE;
        else if( EQUAL(argv[i], "-stats") )
            bStats = TRUE;
        else if( EQUAL(argv[i], "-sample") )
            bSample = TRUE;
        else if( EQUAL(argv[i], "-nogcp") )
            bShowGCPs = FALSE;
        else if( EQUAL(argv[i], "-nomd") )
            bShowMetadata = FALSE;
        else if( EQUAL(argv[i], "-mdd") && i < argc-1 )
            papszExtraMDDomains = CSLAddString( papszExtraMDDomains,
                                                argv[++i] );
        else if( argv[i][0] == '-' )
            Usage();
        else if( pszFilename == NULL )
            pszFilename = argv[i];
        else
            Usage();
    }

    if( pszFilename == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open dataset.                                                   */
/* -------------------------------------------------------------------- */
    hDataset = GDALOpen( pszFilename, GA_ReadOnly );
    
    if( hDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );

        CSLDestroy( argv );
    
        GDALDumpOpenDatasets( stderr );

        GDALDestroyDriverManager();

        CPLDumpSharedList( NULL );

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
            CPLFree( pszPrettyWkt );
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
        if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 )
        {
            printf( "Origin = (%.6f,%.6f)\n",
                    adfGeoTransform[0], adfGeoTransform[3] );

            printf( "Pixel Size = (%.8f,%.8f)\n",
                    adfGeoTransform[1], adfGeoTransform[5] );
        }
        else
            printf( "GeoTransform =\n"
                    "  %.16g, %.16g, %.16g\n"
                    "  %.16g, %.16g, %.16g\n", 
                    adfGeoTransform[0],
                    adfGeoTransform[1],
                    adfGeoTransform[2],
                    adfGeoTransform[3],
                    adfGeoTransform[4],
                    adfGeoTransform[5] );
    }

/* -------------------------------------------------------------------- */
/*      Report GCPs.                                                    */
/* -------------------------------------------------------------------- */
    if( bShowGCPs && GDALGetGCPCount( hDataset ) > 0 )
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
    if( bShowMetadata && CSLCount(papszMetadata) > 0 )
    {
        printf( "Metadata:\n" );
        for( i = 0; papszMetadata[i] != NULL; i++ )
        {
            printf( "  %s\n", papszMetadata[i] );
        }
    }

    for( iMDD = 0; iMDD < CSLCount(papszExtraMDDomains); iMDD++ )
    {
        papszMetadata = GDALGetMetadata( hDataset, papszExtraMDDomains[iMDD] );
        if( bShowMetadata && CSLCount(papszMetadata) > 0 )
        {
            printf( "Metadata (%s):\n", papszExtraMDDomains[iMDD]);
            for( i = 0; papszMetadata[i] != NULL; i++ )
            {
                printf( "  %s\n", papszMetadata[i] );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Report "IMAGE_STRUCTURE" metadata.                              */
/* -------------------------------------------------------------------- */
    papszMetadata = GDALGetMetadata( hDataset, "IMAGE_STRUCTURE" );
    if( bShowMetadata && CSLCount(papszMetadata) > 0 )
    {
        printf( "Image Structure Metadata:\n" );
        for( i = 0; papszMetadata[i] != NULL; i++ )
        {
            printf( "  %s\n", papszMetadata[i] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Report subdatasets.                                             */
/* -------------------------------------------------------------------- */
    papszMetadata = GDALGetMetadata( hDataset, "SUBDATASETS" );
    if( CSLCount(papszMetadata) > 0 )
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
        int         bGotMin, bGotMax, bGotNodata, bSuccess;
        int         nBlockXSize, nBlockYSize;
        double      dfMean, dfStdDev;
        GDALColorTableH	hTable;
        CPLErr      eErr;

        hBand = GDALGetRasterBand( hDataset, iBand+1 );

        if( bSample )
        {
            float afSample[10000];
            int   nCount;

            nCount = GDALGetRandomRasterSample( hBand, 10000, afSample );
            printf( "Got %d samples.\n", nCount );
        }
        
        GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
        printf( "Band %d Block=%dx%d Type=%s, ColorInterp=%s\n", iBand+1,
                nBlockXSize, nBlockYSize,
                GDALGetDataTypeName(
                    GDALGetRasterDataType(hBand)),
                GDALGetColorInterpretationName(
                    GDALGetRasterColorInterpretation(hBand)) );

        if( GDALGetDescription( hBand ) != NULL 
            && strlen(GDALGetDescription( hBand )) > 0 )
            printf( "  Description = %s\n", GDALGetDescription(hBand) );

        dfMin = GDALGetRasterMinimum( hBand, &bGotMin );
        dfMax = GDALGetRasterMaximum( hBand, &bGotMax );
        if( bGotMin || bGotMax || bComputeMinMax )
        {
            printf( "  " );
            if( bGotMin )
                printf( "Min=%.3f ", dfMin );
            if( bGotMax )
                printf( "Max=%.3f ", dfMax );
        
            if( bComputeMinMax )
            {
                GDALComputeRasterMinMax( hBand, FALSE, adfCMinMax );
                printf( "  Computed Min/Max=%.3f,%.3f", 
                        adfCMinMax[0], adfCMinMax[1] );
            }

            printf( "\n" );
        }

        eErr = GDALGetRasterStatistics( hBand, FALSE, bStats, 
                                        &dfMin, &dfMax, &dfMean, &dfStdDev );
        if( eErr == CE_None )
        {
            printf( "  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f\n",
                    dfMin, dfMax, dfMean, dfStdDev );
        }

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

        if( GDALHasArbitraryOverviews( hBand ) )
        {
            printf( "  Overviews: arbitrary\n" );
        }

        if( strlen(GDALGetRasterUnitType(hBand)) > 0 )
        {
            printf( "  Unit Type: %s\n", GDALGetRasterUnitType(hBand) );
        }

        if( GDALGetRasterCategoryNames(hBand) != NULL )
        {
            char **papszCategories = GDALGetRasterCategoryNames(hBand);
            int i;

            printf( "  Categories:\n" );
            for( i = 0; papszCategories[i] != NULL; i++ )
                printf( "    %3d: %s\n", i, papszCategories[i] );
        }

        if( GDALGetRasterScale( hBand, &bSuccess ) != 1.0 
            || GDALGetRasterOffset( hBand, &bSuccess ) != 0.0 )
            printf( "  Offset: %g,   Scale:%g\n",
                    GDALGetRasterOffset( hBand, &bSuccess ),
                    GDALGetRasterScale( hBand, &bSuccess ) );

        papszMetadata = GDALGetMetadata( hBand, NULL );
        if( bShowMetadata && CSLCount(papszMetadata) > 0 )
        {
            printf( "  Metadata:\n" );
            for( i = 0; papszMetadata[i] != NULL; i++ )
            {
                printf( "    %s\n", papszMetadata[i] );
            }
        }

        papszMetadata = GDALGetMetadata( hBand, "IMAGE_STRUCTURE" );
        if( bShowMetadata && CSLCount(papszMetadata) > 0 )
        {
            printf( "  Image Structure Metadata:\n" );
            for( i = 0; papszMetadata[i] != NULL; i++ )
            {
                printf( "    %s\n", papszMetadata[i] );
            }
        }

        if( GDALGetRasterColorInterpretation(hBand) == GCI_PaletteIndex 
            && (hTable = GDALGetRasterColorTable( hBand )) != NULL )
        {
            int			i;

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

        if( GDALGetDefaultRAT( hBand ) != NULL )
        {
            GDALRasterAttributeTableH hRAT = GDALGetDefaultRAT( hBand );
            
            GDALRATDumpReadable( hRAT, NULL );
        }
    }

    GDALClose( hDataset );
    
    CSLDestroy( papszExtraMDDomains );
    CSLDestroy( argv );
    
    GDALDumpOpenDatasets( stderr );

    GDALDestroyDriverManager();

    CPLDumpSharedList( NULL );
    CPLCleanupTLS();

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
    if( ABS(dfGeoX) < 181 && ABS(dfGeoY) < 91 )
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
            CPLPushErrorHandler( CPLQuietErrorHandler );
            hTransform = OCTNewCoordinateTransformation( hProj, hLatLong );
            CPLPopErrorHandler();
            
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

