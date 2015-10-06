/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline application to list info about a file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2015, Even Rouault <even.rouault at spatialys.com>
 * Copyright (c) 2015, Faza Mahamood
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

#include "gdal.h"
#include "gdal_alg.h"
#include "ogr_srs_api.h"
#include "cpl_string.h"
#include "cpl_conv.h"
#include "commonutils.h"
#include "json.h"
#include "ogrgeojsonwriter.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"
#include "gdal_rat.h"

CPL_CVSID("$Id$");

/*! output format */
typedef enum {
    /*! output in text format */ GDALINFO_FORMAT_TEXT = 0,
    /*! output in json format */ GDALINFO_FORMAT_JSON = 1
} GDALInfoFormat;

/************************************************************************/
/*                           GDALInfoOptions                            */
/************************************************************************/

/** Options for use with GDALInfo(). GDALInfoOptions* must be allocated and freed with GDALInfoOptionsNew() and 
 * GDALInfoOptionsFree() respectively.
 */
struct GDALInfoOptions
{
    /*! output format */
    GDALInfoFormat eFormat;

    int bComputeMinMax;

    /*! report histogram information for all bands */
    int bReportHistograms;

    /*! report a PROJ.4 string corresponding to the file's coordinate system */
    int bReportProj4;

    /*! read and display image statistics. Force computation if no statistics are stored in an image */
    int bStats;

    /*! read and display image statistics. Force computation if no statistics are stored in an image.
        However, they may be computed based on overviews or a subset of all tiles. Useful if you are
        in a hurry and don't want precise stats. */
    int bApproxStats;

    int bSample;

    /*! force computation of the checksum for each band in the dataset */
    int bComputeChecksum;

    /*! allow or suppress ground control points list printing. It may be useful for datasets with huge amount
        of GCPs, such as L1B AVHRR or HDF4 MODIS which contain thousands of them. */
    int bShowGCPs;

    /*! allow or suppress metadata printing. Some datasets may contain a lot of metadata strings. */
    int bShowMetadata;

    /*! allow or suppress printing of raster attribute table */
    int bShowRAT;

    /*! allow or suppress printing of color table */
    int bShowColorTable;

    /*! list all metadata domains available for the dataset */
    int bListMDD;

    /*! display the file list or the first file of the file list */
    int bShowFileList;

    /*! report metadata for the specified domains. "all" can be used to report metadata
        in all domains.
        */
    char **papszExtraMDDomains;
};


static int 
GDALInfoReportCorner( GDALDatasetH hDataset, 
                      OGRCoordinateTransformationH hTransform, OGRCoordinateTransformationH hTransformWGS84,
                      const char * corner_name,
                      double x, double y,
                      int bJson, json_object *poCornerCoordinates,
                      json_object *poWGS84ExtentCoordinates,
                      CPLString& osStr );

static void
GDALInfoReportMetadata( GDALMajorObjectH hObject,
                        int bListMDD,
                        int bShowMetadata,
                        char **papszExtraMDDomains,
                        int bIsBand,
                        int bJson,
                        json_object *poMetadata,
                        CPLString& osStr );

/************************************************************************/
/*                             GDALInfo()                               */
/************************************************************************/

/**
 * Lists various information about a GDAL supported raster dataset.
 *
 * This is the equivalent of the <a href="gdalinfo.html">gdalinfo</a> utility.
 *
 * GDALInfoOptions* must be allocated and freed with GDALInfoOptionsNew()
 * and GDALInfoOptionsFree() respectively.
 *
 * @param hDataset the dataset handle.
 * @param psOptions the options structure returned by GDALInfoOptionsNew() or NULL.
 * @return string corresponding to the information about the raster dataset.
 * It must be freed using CPLFree().
 *
 * @since GDAL 2.1
 */

char *GDALInfo( GDALDatasetH hDataset, const GDALInfoOptions *psOptions )
{
    if( hDataset == NULL )
        return NULL;

    bool bFreeOptions = false;
    GDALInfoOptions* psOptionsToFree = NULL;
    if( psOptions == NULL )
    {
        bFreeOptions = true;
        psOptionsToFree = GDALInfoOptionsNew(NULL, NULL);
        psOptions = psOptionsToFree;
    }

    GDALRasterBandH hBand = NULL;
    double      adfGeoTransform[6];
    GDALDriverH     hDriver;
    char **papszFileList;
    const char  *pszProjection = NULL;
    OGRCoordinateTransformationH hTransform = NULL, hTransformWGS84 = NULL;
    CPLString osStr;
    int bJson = FALSE;
    json_object *poJsonObject = NULL, *poBands = NULL, *poMetadata = NULL;

    if(psOptions->eFormat == GDALINFO_FORMAT_JSON)
        bJson = TRUE;

/* -------------------------------------------------------------------- */
/*      Report general info.                                            */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDatasetDriver( hDataset );
    if(bJson)
    {
        json_object *poDescription = json_object_new_string(GDALGetDescription(hDataset));
        json_object *poDriverShortName = json_object_new_string(GDALGetDriverShortName(hDriver));
        json_object *poDriverLongName = json_object_new_string(GDALGetDriverLongName(hDriver));
        poJsonObject = json_object_new_object();
        poBands = json_object_new_array();
        poMetadata = json_object_new_object();

        json_object_object_add(poJsonObject, "description", poDescription);
        json_object_object_add(poJsonObject, "driverShortName", poDriverShortName);
        json_object_object_add(poJsonObject, "driverLongName", poDriverLongName);
    }
    else
    {
        osStr += CPLOPrintf( "Driver: %s/%s\n", GDALGetDriverShortName( hDriver ), 
            GDALGetDriverLongName( hDriver ) );
    }

    papszFileList = GDALGetFileList( hDataset );

    if( CSLCount(papszFileList) == 0 )
    {   
        if(bJson)
        {
            json_object *poFiles = json_object_new_array();   
            json_object_object_add(poJsonObject, "files", poFiles);
        }
        else
            osStr += CPLOPrintf( "Files: none associated\n" );
    }
    else
    {
        if(bJson)
        {
            if( psOptions->bShowFileList )
            {
                json_object *poFiles = json_object_new_array();

                for(int i = 0; papszFileList[i] != NULL; i++)
                {
                    json_object *poFile = json_object_new_string(papszFileList[i]);

                    json_object_array_add(poFiles, poFile);
                }

                json_object_object_add(poJsonObject, "files", poFiles);
            }
        }
        else
        {
            osStr += CPLOPrintf( "Files: %s\n", papszFileList[0] );
            if( psOptions->bShowFileList )
            {
                for( int i = 1; papszFileList[i] != NULL; i++ )
                    osStr += CPLOPrintf( "       %s\n", papszFileList[i] );
            }
        }

    }
    CSLDestroy( papszFileList );

    if(bJson)
    {
        json_object *poSize = json_object_new_array();
        json_object *poSizeX = json_object_new_int(GDALGetRasterXSize(hDataset));
        json_object *poSizeY = json_object_new_int(GDALGetRasterYSize(hDataset));

        json_object_array_add(poSize, poSizeX);
        json_object_array_add(poSize, poSizeY);
        json_object_object_add(poJsonObject, "size", poSize);
    }
    else
        osStr += CPLOPrintf( "Size is %d, %d\n", GDALGetRasterXSize( hDataset ), 
            GDALGetRasterYSize( hDataset ) );

/* -------------------------------------------------------------------- */
/*      Report projection.                                              */
/* -------------------------------------------------------------------- */
    if( GDALGetProjectionRef( hDataset ) != NULL )
    {
        json_object *poCoordinateSystem = NULL;

        if(bJson)
            poCoordinateSystem = json_object_new_object();

        char *pszProjection
            = (char *) GDALGetProjectionRef( hDataset );

        OGRSpatialReferenceH hSRS
            = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hSRS, &pszProjection ) == CE_None )
        {
            char    *pszPrettyWkt = NULL;

            OSRExportToPrettyWkt( hSRS, &pszPrettyWkt, FALSE );

            if(bJson)
            {
                json_object *poWkt = json_object_new_string(pszPrettyWkt);
                json_object_object_add(poCoordinateSystem, "wkt", poWkt);
            }
            else
                osStr += CPLOPrintf( "Coordinate System is:\n%s\n", pszPrettyWkt );
            CPLFree( pszPrettyWkt );
        }
        else
        {
            if(bJson)
            {
                json_object *poWkt = json_object_new_string(GDALGetProjectionRef(hDataset));
                json_object_object_add(poCoordinateSystem, "wkt", poWkt);
            }
            else
                osStr += CPLOPrintf( "Coordinate System is `%s'\n", 
                    GDALGetProjectionRef( hDataset ) );

        }

        if ( psOptions->bReportProj4 )
        {
            char *pszProj4 = NULL;
            OSRExportToProj4( hSRS, &pszProj4 );

            if(bJson)
            {
                json_object *proj4 = json_object_new_string(pszProj4);
                json_object_object_add(poCoordinateSystem, "proj4", proj4);
            }
            else
                osStr += CPLOPrintf("PROJ.4 string is:\n\'%s\'\n",pszProj4);
            CPLFree( pszProj4 );
        }

        if(bJson)
            json_object_object_add(poJsonObject, "coordinateSystem", poCoordinateSystem);

        OSRDestroySpatialReference( hSRS );
    }

/* -------------------------------------------------------------------- */
/*      Report Geotransform.                                            */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        if(bJson)
        {
            json_object *poGeoTransform = json_object_new_array();

            for( int i = 0; i < 6; i++ )
            {
                json_object *poGeoTransformCoefficient = json_object_new_double_with_precision(adfGeoTransform[i], 16);
                json_object_array_add(poGeoTransform, poGeoTransformCoefficient);
            }

            json_object_object_add(poJsonObject, "geoTransform", poGeoTransform);
        }
        else
        {
            if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 )
            {
                osStr += CPLOPrintf( "Origin = (%.15f,%.15f)\n", 
                    adfGeoTransform[0], adfGeoTransform[3] );

                osStr += CPLOPrintf( "Pixel Size = (%.15f,%.15f)\n", 
                    adfGeoTransform[1], adfGeoTransform[5] );
            }
            else
                osStr += CPLOPrintf( "GeoTransform =\n"
                    "  %.16g, %.16g, %.16g\n"
                    "  %.16g, %.16g, %.16g\n",
                    adfGeoTransform[0],
                    adfGeoTransform[1],
                    adfGeoTransform[2],
                    adfGeoTransform[3],
                    adfGeoTransform[4],
                    adfGeoTransform[5] );
        }

    }

/* -------------------------------------------------------------------- */
/*      Report GCPs.                                                    */
/* -------------------------------------------------------------------- */
    if( psOptions->bShowGCPs && GDALGetGCPCount( hDataset ) > 0 )
    {
        json_object *poGCPs = NULL, *poGCPList = NULL;

        if(bJson)
            poGCPs = json_object_new_object();

        if (GDALGetGCPProjection(hDataset) != NULL)
        {
            json_object *poGCPCoordinateSystem = NULL;

            char *pszProjection
                = (char *) GDALGetGCPProjection( hDataset );

            OGRSpatialReferenceH hSRS
                = OSRNewSpatialReference(NULL);
            if( OSRImportFromWkt( hSRS, &pszProjection ) == CE_None )
            {
                char    *pszPrettyWkt = NULL;

                OSRExportToPrettyWkt( hSRS, &pszPrettyWkt, FALSE );
                if(bJson)
                {
                    json_object *poWkt = json_object_new_string(pszPrettyWkt);
                    poGCPCoordinateSystem = json_object_new_object();

                    json_object_object_add(poGCPCoordinateSystem, "wkt", poWkt);
                }
                else
                    osStr += CPLOPrintf( "GCP Projection = \n%s\n", pszPrettyWkt );
                CPLFree( pszPrettyWkt );
            }
            else
            {
                if(bJson)
                {
                    json_object *poWkt = json_object_new_string(GDALGetGCPProjection(hDataset));
                    poGCPCoordinateSystem = json_object_new_object();

                    json_object_object_add(poGCPCoordinateSystem, "wkt", poWkt);
                }
                else
                    osStr += CPLOPrintf( "GCP Projection = %s\n",
                        GDALGetGCPProjection( hDataset ) );

            }

            if(bJson)
                json_object_object_add(poGCPs, "coordinateSystem", poGCPCoordinateSystem);
            OSRDestroySpatialReference( hSRS );
        }

        if(bJson)
            poGCPList = json_object_new_array();

        for( int i = 0; i < GDALGetGCPCount(hDataset); i++ )
        {
            const GDAL_GCP *psGCP = GDALGetGCPs( hDataset ) + i;
            if(bJson)
            {
                json_object *poGCP = json_object_new_object();
                json_object *poId = json_object_new_string(psGCP->pszId);
                json_object *poInfo = json_object_new_string(psGCP->pszInfo);
                json_object *poPixel = json_object_new_double_with_precision(psGCP->dfGCPPixel, 15);
                json_object *poLine = json_object_new_double_with_precision(psGCP->dfGCPLine, 15);
                json_object *poX = json_object_new_double_with_precision(psGCP->dfGCPX, 15);
                json_object *poY = json_object_new_double_with_precision(psGCP->dfGCPY, 15);
                json_object *poZ = json_object_new_double_with_precision(psGCP->dfGCPZ, 15);

                json_object_object_add(poGCP, "id", poId);
                json_object_object_add(poGCP, "info", poInfo);
                json_object_object_add(poGCP, "pixel", poPixel);
                json_object_object_add(poGCP, "line", poLine);
                json_object_object_add(poGCP, "x", poX);
                json_object_object_add(poGCP, "y", poY);
                json_object_object_add(poGCP, "z", poZ);
                json_object_array_add(poGCPList, poGCP);
            }
            else
                osStr += CPLOPrintf( "GCP[%3d]: Id=%s, Info=%s\n"
                    "          (%.15g,%.15g) -> (%.15g,%.15g,%.15g)\n",
                    i, psGCP->pszId, psGCP->pszInfo,
                    psGCP->dfGCPPixel, psGCP->dfGCPLine,
                    psGCP->dfGCPX, psGCP->dfGCPY, psGCP->dfGCPZ );
        }
        if(bJson)
        {
            json_object_object_add(poGCPs, "gcpList", poGCPList);
            json_object_object_add(poJsonObject, "gcps", poGCPs);
        }
    }

/* -------------------------------------------------------------------- */
/*      Report metadata.                                                */
/* -------------------------------------------------------------------- */

    GDALInfoReportMetadata( hDataset, psOptions->bListMDD, psOptions->bShowMetadata, 
        psOptions->papszExtraMDDomains, FALSE, bJson, poMetadata, osStr );
    if(bJson && psOptions->bShowMetadata)
        json_object_object_add( poJsonObject, "metadata", poMetadata );

/* -------------------------------------------------------------------- */
/*      Setup projected to lat/long transform if appropriate.           */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
        pszProjection = GDALGetProjectionRef(hDataset);

    if( pszProjection != NULL && strlen(pszProjection) > 0 )
    {
        OGRSpatialReferenceH hProj, hLatLong = NULL, hLatLongWGS84 = NULL;

        hProj = OSRNewSpatialReference( pszProjection );
        if( hProj != NULL )
        {
            hLatLong = OSRCloneGeogCS( hProj );

            if(bJson)
            {
                hLatLongWGS84 = OSRNewSpatialReference( NULL );
                OSRSetWellKnownGeogCS( hLatLongWGS84, "WGS84" );
            }
        }

        if( hLatLong != NULL )
        {
            CPLPushErrorHandler( CPLQuietErrorHandler );
            hTransform = OCTNewCoordinateTransformation( hProj, hLatLong );
            CPLPopErrorHandler();

            OSRDestroySpatialReference( hLatLong );
        }

        if( hLatLongWGS84 != NULL )
        {
            CPLPushErrorHandler( CPLQuietErrorHandler );
            hTransformWGS84 = OCTNewCoordinateTransformation( hProj, hLatLongWGS84 );
            CPLPopErrorHandler();

            OSRDestroySpatialReference( hLatLongWGS84 );
        }

        if( hProj != NULL )
            OSRDestroySpatialReference( hProj );
    }

/* -------------------------------------------------------------------- */
/*      Report corners.                                                 */
/* -------------------------------------------------------------------- */
    if(bJson)
    {
        json_object *poLinearRing = json_object_new_array();
        json_object *poCornerCoordinates = json_object_new_object();
        json_object *poWGS84Extent = json_object_new_object();
        json_object *poWGS84ExtentType = json_object_new_string("Polygon");
        json_object *poWGS84ExtentCoordinates = json_object_new_array();

        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "upperLeft",
                              0.0, 0.0, bJson, poCornerCoordinates, poWGS84ExtentCoordinates, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "lowerLeft",
                              0.0, GDALGetRasterYSize(hDataset), bJson, poCornerCoordinates, poWGS84ExtentCoordinates, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "upperRight",
                              GDALGetRasterXSize(hDataset), 0.0, bJson, poCornerCoordinates, poWGS84ExtentCoordinates, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "lowerRight",
                              GDALGetRasterXSize(hDataset), GDALGetRasterYSize(hDataset),
                              bJson, poCornerCoordinates, poWGS84ExtentCoordinates, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "center",
                              GDALGetRasterXSize(hDataset)/2.0, GDALGetRasterYSize(hDataset)/2.0,
                              bJson, poCornerCoordinates, poWGS84ExtentCoordinates, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "upperLeft",
                              0.0, 0.0, bJson, poCornerCoordinates, poWGS84ExtentCoordinates, osStr );

        json_object_object_add( poJsonObject, "cornerCoordinates", poCornerCoordinates );
        json_object_object_add( poWGS84Extent, "type", poWGS84ExtentType );
        json_object_array_add( poLinearRing, poWGS84ExtentCoordinates );
        json_object_object_add( poWGS84Extent, "coordinates", poLinearRing );
        json_object_object_add( poJsonObject, "wgs84Extent", poWGS84Extent );
    }
    else
    {
        osStr += CPLOPrintf( "Corner Coordinates:\n" );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Upper Left",
                              0.0, 0.0, bJson, NULL, NULL, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Lower Left",
                              0.0, GDALGetRasterYSize(hDataset), bJson, NULL, NULL, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Upper Right",
                              GDALGetRasterXSize(hDataset), 0.0, bJson, NULL, NULL, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Lower Right",
                              GDALGetRasterXSize(hDataset),
                              GDALGetRasterYSize(hDataset), bJson, NULL, NULL, osStr );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Center",
                              GDALGetRasterXSize(hDataset)/2.0,
                              GDALGetRasterYSize(hDataset)/2.0, bJson, NULL, NULL, osStr );

    }

    if( hTransform != NULL )
    {
        OCTDestroyCoordinateTransformation( hTransform );
        hTransform = NULL;
    }

    if( hTransformWGS84 != NULL )
    {
        OCTDestroyCoordinateTransformation( hTransformWGS84 );
        hTransformWGS84 = NULL;
    }

/* ==================================================================== */
/*      Loop over bands.                                                */
/* ==================================================================== */
    for( int iBand = 0; iBand < GDALGetRasterCount( hDataset ); iBand++ )
    {
        double      dfMin, dfMax, adfCMinMax[2], dfNoData;
        int         bGotMin, bGotMax, bGotNodata, bSuccess;
        int         nBlockXSize, nBlockYSize, nMaskFlags;
        double      dfMean, dfStdDev;
        GDALColorTableH hTable;
        CPLErr      eErr;
        json_object *poBand = NULL, *poBandMetadata = NULL;

        if(bJson)
        {
            poBand = json_object_new_object();
            poBandMetadata = json_object_new_object();
        }

        hBand = GDALGetRasterBand( hDataset, iBand+1 );

        if( psOptions->bSample )
        {
            float afSample[10000];
            int nCount = GDALGetRandomRasterSample( hBand, 10000, afSample );
            if(!bJson)
                osStr += CPLOPrintf( "Got %d samples.\n", nCount );
        }

        GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
        if(bJson)
        {
            json_object *poBandNumber = json_object_new_int(iBand+1);
            json_object *poBlock = json_object_new_array();
            json_object *poType = json_object_new_string(GDALGetDataTypeName(GDALGetRasterDataType(hBand)));
            json_object *poColorInterp = json_object_new_string(GDALGetColorInterpretationName(
                GDALGetRasterColorInterpretation(hBand)));

            json_object_array_add(poBlock, json_object_new_int(nBlockXSize));
            json_object_array_add(poBlock, json_object_new_int(nBlockYSize));
            json_object_object_add(poBand, "band", poBandNumber);
            json_object_object_add(poBand, "block", poBlock);
            json_object_object_add(poBand, "type", poType);
            json_object_object_add(poBand, "colorInterpretation", poColorInterp);
        }
        else
            osStr += CPLOPrintf( "Band %d Block=%dx%d Type=%s, ColorInterp=%s\n", iBand+1,
                nBlockXSize, nBlockYSize,
                GDALGetDataTypeName(
                    GDALGetRasterDataType(hBand)),
                GDALGetColorInterpretationName(
                    GDALGetRasterColorInterpretation(hBand)) );

        if( GDALGetDescription( hBand ) != NULL 
            && strlen(GDALGetDescription( hBand )) > 0 )
        {
            if(bJson)
            {
                json_object *poBandDescription = json_object_new_string(GDALGetDescription(hBand));
                json_object_object_add(poBand, "description", poBandDescription);
            }
            else
                osStr += CPLOPrintf( "  Description = %s\n", GDALGetDescription(hBand) );
        }

        dfMin = GDALGetRasterMinimum( hBand, &bGotMin );
        dfMax = GDALGetRasterMaximum( hBand, &bGotMax );
        if( bGotMin || bGotMax || psOptions->bComputeMinMax )
        {
            if(!bJson)
                osStr += CPLOPrintf( "  " );
            if( bGotMin )
            {
                if(bJson)
                {
                    json_object *poMin = json_object_new_double_with_precision(dfMin, 3);
                    json_object_object_add(poBand, "min", poMin);
                }
                else
                    osStr += CPLOPrintf( "Min=%.3f ", dfMin );
            }
            if( bGotMax )
            {
                if(bJson)
                {
                    json_object *poMax = json_object_new_double_with_precision(dfMax, 3);
                    json_object_object_add(poBand, "max", poMax);
                }
                else
                    osStr += CPLOPrintf( "Max=%.3f ", dfMax );
            }

            if( psOptions->bComputeMinMax )
            {
                CPLErrorReset();
                GDALComputeRasterMinMax( hBand, FALSE, adfCMinMax );
                if (CPLGetLastErrorType() == CE_None)
                {
                    if(bJson)
                    {
                        json_object *poComputedMin = json_object_new_double_with_precision(adfCMinMax[0], 3);
                        json_object *poComputedMax = json_object_new_double_with_precision(adfCMinMax[1], 3);
                        json_object_object_add(poBand, "computedMin", poComputedMin);
                        json_object_object_add(poBand, "computedMax", poComputedMax);
                    }
                    else
                        osStr += CPLOPrintf( "  Computed Min/Max=%.3f,%.3f",
                          adfCMinMax[0], adfCMinMax[1] );
                }
            }
            if(!bJson)
                osStr += CPLOPrintf( "\n" );
        }

        eErr = GDALGetRasterStatistics( hBand, psOptions->bApproxStats, psOptions->bStats, 
                                        &dfMin, &dfMax, &dfMean, &dfStdDev );
        if( eErr == CE_None )
        {
            if(bJson)
            {
                json_object *poMinimum = json_object_new_double_with_precision(dfMin, 3);
                json_object *poMaximum = json_object_new_double_with_precision(dfMax, 3);
                json_object *poMean = json_object_new_double_with_precision(dfMean, 3);
                json_object *poStdDev = json_object_new_double_with_precision(dfStdDev, 3);

                json_object_object_add(poBand, "minimum", poMinimum);
                json_object_object_add(poBand, "maximum", poMaximum);
                json_object_object_add(poBand, "mean", poMean);
                json_object_object_add(poBand, "stdDev", poStdDev);
            }
            else
                osStr += CPLOPrintf( "  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f\n",
                    dfMin, dfMax, dfMean, dfStdDev );
        }

        if( psOptions->bReportHistograms )
        {
            int nBucketCount;
            GUIntBig *panHistogram = NULL;

            if(bJson)
                eErr = GDALGetDefaultHistogramEx( hBand, &dfMin, &dfMax, 
                                            &nBucketCount, &panHistogram, 
                                            TRUE, GDALDummyProgress, NULL );
            else
                eErr = GDALGetDefaultHistogramEx( hBand, &dfMin, &dfMax, 
                                            &nBucketCount, &panHistogram, 
                                            TRUE, GDALTermProgress, NULL );
            if( eErr == CE_None )
            {
                json_object *poHistogram = NULL, *poBuckets = NULL;

                if(bJson)
                {
                    json_object *poCount = json_object_new_int(nBucketCount);
                    json_object *poMin = json_object_new_double(dfMin);
                    json_object *poMax = json_object_new_double(dfMax);

                    poBuckets = json_object_new_array();
                    poHistogram = json_object_new_object();
                    json_object_object_add(poHistogram, "count", poCount);
                    json_object_object_add(poHistogram, "min", poMin);
                    json_object_object_add(poHistogram, "max", poMax);
                }
                else
                    osStr += CPLOPrintf( "  %d buckets from %g to %g:\n  ",
                        nBucketCount, dfMin, dfMax );

                for( int iBucket = 0; iBucket < nBucketCount; iBucket++ )
                {
                    if(bJson)
                    {
                        json_object *poBucket = json_object_new_int64(panHistogram[iBucket]);
                        json_object_array_add(poBuckets, poBucket);
                    }
                    else
                        osStr += CPLOPrintf( CPL_FRMT_GUIB " ", panHistogram[iBucket] );
                }
                if(bJson)
                {
                    json_object_object_add(poHistogram, "buckets", poBuckets);
                    json_object_object_add(poBand, "histogram", poHistogram);
                }
                else
                    osStr += CPLOPrintf( "\n" );
                CPLFree( panHistogram );
            }
        }

        if ( psOptions->bComputeChecksum)
        {
            int nBandChecksum = GDALChecksumImage(hBand, 0, 0,
                                      GDALGetRasterXSize(hDataset),
                                      GDALGetRasterYSize(hDataset));
            if(bJson)
            {
                json_object *poChecksum = json_object_new_int(nBandChecksum);
                json_object_object_add(poBand, "checksum", poChecksum);
            }
            else
                osStr += CPLOPrintf( "  Checksum=%d\n", nBandChecksum );
        }

        dfNoData = GDALGetRasterNoDataValue( hBand, &bGotNodata );
        if( bGotNodata )
        {
            if (CPLIsNan(dfNoData))
            {
                if(bJson)
                {
                    json_object *poNoDataValue = json_object_new_string("nan");
                    json_object_object_add(poBand, "noDataValue", poNoDataValue);
                }
                else
                    osStr += CPLOPrintf( "  NoData Value=nan\n" );
            }
            else
            {
                if(bJson)
                {
                    json_object *poNoDataValue = json_object_new_double_with_precision(dfNoData, 18);
                    json_object_object_add(poBand, "noDataValue", poNoDataValue);
                }
                else
                    osStr += CPLOPrintf( "  NoData Value=%.18g\n", dfNoData );
            }
        }

        if( GDALGetOverviewCount(hBand) > 0 )
        {
            json_object *poOverviews = NULL;

            if(bJson)
                poOverviews = json_object_new_array();
            else
                osStr += CPLOPrintf( "  Overviews: " );

            for( int iOverview = 0;
                 iOverview < GDALGetOverviewCount(hBand);
                 iOverview++ )
            {
                json_object *poOverview;

                if(!bJson)
                    if( iOverview != 0 )
                        osStr += CPLOPrintf( ", " );

                GDALRasterBandH hOverview = GDALGetOverview( hBand, iOverview );
                if (hOverview != NULL)
                {
                    if(bJson)
                    {
                        json_object *poOverviewSize = json_object_new_array();
                        json_object *poOverviewSizeX = json_object_new_int( GDALGetRasterBandXSize( hOverview) );
                        json_object *poOverviewSizeY = json_object_new_int( GDALGetRasterBandYSize( hOverview) );

                        poOverview = json_object_new_object();
                        json_object_array_add( poOverviewSize, poOverviewSizeX );
                        json_object_array_add( poOverviewSize, poOverviewSizeY );
                        json_object_object_add( poOverview, "size", poOverviewSize );

                        if(psOptions->bComputeChecksum)
                        {
                            int nOverviewChecksum = GDALChecksumImage(hOverview, 0, 0,
                                        GDALGetRasterBandXSize(hOverview),
                                        GDALGetRasterBandYSize(hOverview));
                            json_object *poOverviewChecksum = json_object_new_int(nOverviewChecksum);
                            json_object_object_add(poOverview, "checksum", poOverviewChecksum);
                        }
                        json_object_array_add(poOverviews, poOverview);
                    }
                    else
                        osStr += CPLOPrintf( "%dx%d", 
                            GDALGetRasterBandXSize( hOverview ),
                            GDALGetRasterBandYSize( hOverview ) );

                    const char *pszResampling
                        = GDALGetMetadataItem( hOverview, "RESAMPLING", "" );

                    if( pszResampling != NULL && !bJson 
                        && EQUALN(pszResampling,"AVERAGE_BIT2",12) )
                        osStr += CPLOPrintf( "*" );
                }
                else
                    if(!bJson)
                        osStr += CPLOPrintf( "(null)" );
            }
            if(bJson)
                json_object_object_add(poBand, "overviews", poOverviews);
            else
                osStr += CPLOPrintf( "\n" );

            if ( psOptions->bComputeChecksum && !bJson )
            {
                osStr += CPLOPrintf( "  Overviews checksum: " );

                for( int iOverview = 0;
                     iOverview < GDALGetOverviewCount(hBand);
                     iOverview++ )
                {
                    GDALRasterBandH hOverview;

                    if( iOverview != 0 )
                        osStr += CPLOPrintf( ", " );

                    hOverview = GDALGetOverview( hBand, iOverview );
                    if (hOverview)
                    {
                        osStr += CPLOPrintf( "%d",
                                GDALChecksumImage(hOverview, 0, 0,
                                        GDALGetRasterBandXSize(hOverview),
                                        GDALGetRasterBandYSize(hOverview)));
                    }
                    else
                    {
                        osStr += CPLOPrintf( "(null)" );
                    }
                }
                osStr += CPLOPrintf( "\n" );
            }
        }

        if( GDALHasArbitraryOverviews( hBand ) && !bJson )
        {
            osStr += CPLOPrintf( "  Overviews: arbitrary\n" );
        }

        nMaskFlags = GDALGetMaskFlags( hBand );
        if( (nMaskFlags & (GMF_NODATA|GMF_ALL_VALID)) == 0 )
        {
            GDALRasterBandH hMaskBand = GDALGetMaskBand(hBand) ;
            json_object *poMask = NULL, *poFlags = NULL, *poMaskOverviews = NULL;

            if(bJson)
            {
                poMask = json_object_new_object();
                poFlags = json_object_new_array(); 
            }
            else
                osStr += CPLOPrintf( "  Mask Flags: " );
            if( nMaskFlags & GMF_PER_DATASET )
            {
                if(bJson)
                {
                    json_object *poFlag = json_object_new_string( "PER_DATASET" );
                    json_object_array_add( poFlags, poFlag );
                }
                else
                    osStr += CPLOPrintf( "PER_DATASET " );
            }
            if( nMaskFlags & GMF_ALPHA )
            {
                if(bJson)
                {
                    json_object *poFlag = json_object_new_string( "ALPHA" );
                    json_object_array_add( poFlags, poFlag );
                }
                else
                    osStr += CPLOPrintf( "ALPHA " );
            }
            if( nMaskFlags & GMF_NODATA )
            {
                if(bJson)
                {
                    json_object *poFlag = json_object_new_string( "NODATA" );
                    json_object_array_add( poFlags, poFlag );
                }
                else
                    osStr += CPLOPrintf( "NODATA " );
            }
            if( nMaskFlags & GMF_ALL_VALID )
            {
                if(bJson)
                {
                    json_object *poFlag = json_object_new_string( "ALL_VALID" );
                    json_object_array_add( poFlags, poFlag );
                }
                else
                    osStr += CPLOPrintf( "ALL_VALID " );
            }
            if(bJson)
                json_object_object_add( poMask, "flags", poFlags );
            else
                osStr += CPLOPrintf( "\n" );

            if(bJson)
                poMaskOverviews = json_object_new_array();

            if( hMaskBand != NULL &&
                GDALGetOverviewCount(hMaskBand) > 0 )
            {
                if(!bJson)
                    osStr += CPLOPrintf( "  Overviews of mask band: " );

                for( int iOverview = 0;
                     iOverview < GDALGetOverviewCount(hMaskBand);
                     iOverview++ )
                {
                    GDALRasterBandH hOverview;
                    json_object *poMaskOverviewSizeX, *poMaskOverviewSizeY,
                                *poMaskOverview = NULL, *poMaskOverviewSize = NULL;

                    if(bJson)
                    {
                        poMaskOverview = json_object_new_object();
                        poMaskOverviewSize = json_object_new_array();
                    }
                    else
                    {
                        if( iOverview != 0 )
                            osStr += CPLOPrintf( ", " );
                    }

                    hOverview = GDALGetOverview( hMaskBand, iOverview );
                    if(bJson)
                    {
                        poMaskOverviewSizeX = json_object_new_int(GDALGetRasterBandXSize(hOverview));
                        poMaskOverviewSizeY = json_object_new_int(GDALGetRasterBandYSize(hOverview));

                        json_object_array_add(poMaskOverviewSize, poMaskOverviewSizeX);
                        json_object_array_add(poMaskOverviewSize, poMaskOverviewSizeY);
                        json_object_object_add(poMaskOverview, "size", poMaskOverviewSize);
                        json_object_array_add(poMaskOverviews, poMaskOverview);
                    }
                    else
                        osStr += CPLOPrintf( "%dx%d", 
                            GDALGetRasterBandXSize( hOverview ),
                            GDALGetRasterBandYSize( hOverview ) );
                }
                if(!bJson)
                    osStr += CPLOPrintf( "\n" );
            }
            if(bJson)
            {
                json_object_object_add(poMask, "overviews", poMaskOverviews);
                json_object_object_add(poBand, "mask", poMask);
            }    
        }

        if( strlen(GDALGetRasterUnitType(hBand)) > 0 )
        {
            if(bJson)
            {
                json_object *poUnit = json_object_new_string(GDALGetRasterUnitType(hBand));
                json_object_object_add(poBand, "unit", poUnit);
            }
            else
                osStr += CPLOPrintf( "  Unit Type: %s\n", GDALGetRasterUnitType(hBand) );
        }

        if( GDALGetRasterCategoryNames(hBand) != NULL )
        {
            char **papszCategories = GDALGetRasterCategoryNames(hBand);
            json_object *poCategories = NULL;

            if(bJson)
                poCategories = json_object_new_array();           
            else
                osStr += CPLOPrintf( "  Categories:\n" );

            for( int i = 0; papszCategories[i] != NULL; i++ )
            {
                if(bJson)
                {
                    json_object *poCategoryName = json_object_new_string(papszCategories[i]);
                    json_object_array_add(poCategories, poCategoryName);
                }
                else
                    osStr += CPLOPrintf( "    %3d: %s\n", i, papszCategories[i] );
            }
            if(bJson)
                json_object_object_add(poBand, "categories", poCategories);
        }

        if( GDALGetRasterScale( hBand, &bSuccess ) != 1.0 
            || GDALGetRasterOffset( hBand, &bSuccess ) != 0.0 )
        {
            if(bJson)
            {
                json_object *poOffset = json_object_new_double_with_precision(GDALGetRasterOffset(hBand, &bSuccess), 15);
                json_object *poScale = json_object_new_double_with_precision(GDALGetRasterScale(hBand, &bSuccess), 15);
                json_object_object_add(poBand, "offset", poOffset);
                json_object_object_add(poBand, "scale", poScale);
            }
            else
                osStr += CPLOPrintf( "  Offset: %.15g,   Scale:%.15g\n",
                    GDALGetRasterOffset( hBand, &bSuccess ),
                    GDALGetRasterScale( hBand, &bSuccess ) );
        }

        GDALInfoReportMetadata( hBand, psOptions->bListMDD, psOptions->bShowMetadata, psOptions->papszExtraMDDomains, 
            TRUE, bJson, poBandMetadata, osStr );
        if(bJson && psOptions->bShowMetadata)  
            json_object_object_add( poBand, "metadata", poBandMetadata );

        if( GDALGetRasterColorInterpretation(hBand) == GCI_PaletteIndex 
            && (hTable = GDALGetRasterColorTable( hBand )) != NULL )
        {
            if(!bJson)
                osStr += CPLOPrintf( "  Color Table (%s with %d entries)\n",
                    GDALGetPaletteInterpretationName(
                        GDALGetPaletteInterpretation( hTable )), 
                    GDALGetColorEntryCount( hTable ) );

            if (psOptions->bShowColorTable)
            {
                json_object *poEntries = NULL;

                if(bJson)
                {
                    json_object *poPalette = json_object_new_string(GDALGetPaletteInterpretationName(
                        GDALGetPaletteInterpretation(hTable)));
                    json_object *poCount = json_object_new_int(GDALGetColorEntryCount(hTable));

                    json_object *poColorTable = json_object_new_object();

                    json_object_object_add(poColorTable, "palette", poPalette);
                    json_object_object_add(poColorTable, "count", poCount);

                    poEntries = json_object_new_array();
                    json_object_object_add(poColorTable, "entries", poEntries);
                    json_object_object_add(poBand, "colorTable", poColorTable);
                }

                for( int i = 0; i < GDALGetColorEntryCount( hTable ); i++ )
                {
                    GDALColorEntry  sEntry;

                    GDALGetColorEntryAsRGB( hTable, i, &sEntry );

                    if(bJson)
                    {
                        json_object *poEntry = json_object_new_array();
                        json_object *poC1 = json_object_new_int(sEntry.c1);
                        json_object *poC2 = json_object_new_int(sEntry.c2);
                        json_object *poC3 = json_object_new_int(sEntry.c3);
                        json_object *poC4 = json_object_new_int(sEntry.c4);

                        json_object_array_add(poEntry, poC1);
                        json_object_array_add(poEntry, poC2);
                        json_object_array_add(poEntry, poC3);
                        json_object_array_add(poEntry, poC4);
                        json_object_array_add(poEntries, poEntry);
                    }
                    else
                        osStr += CPLOPrintf( "  %3d: %d,%d,%d,%d\n",
                            i,
                            sEntry.c1,
                            sEntry.c2,
                            sEntry.c3,
                            sEntry.c4 );
                }
            }   
        }

        if( psOptions->bShowRAT && GDALGetDefaultRAT( hBand ) != NULL )
        {
            GDALRasterAttributeTableH hRAT = GDALGetDefaultRAT( hBand );

            if(bJson)
            {
                json_object *poRAT = (json_object*) GDALRATSerializeJSON( hRAT );
                json_object_object_add( poJsonObject, "rat", poRAT );
            }
            else
            {
                CPLXMLNode *psTree = ((GDALRasterAttributeTable *) hRAT)->Serialize();
                char *pszXMLText = CPLSerializeXMLTree( psTree );
                CPLDestroyXMLNode( psTree );
                osStr += CPLOPrintf( "%s\n", pszXMLText );
                CPLFree( pszXMLText );
            }
        }
        if(bJson)
            json_object_array_add(poBands, poBand);
    }

    if(bJson)
    {
        json_object_object_add(poJsonObject, "bands", poBands);
        osStr += json_object_to_json_string_ext(poJsonObject, JSON_C_TO_STRING_PRETTY);
        json_object_put(poJsonObject);
    }

    if( bFreeOptions )
        GDALInfoOptionsFree(psOptionsToFree);
    return CPLStrdup(osStr);

}

/************************************************************************/
/*                        GDALInfoReportCorner()                        */
/************************************************************************/

static int 
GDALInfoReportCorner( GDALDatasetH hDataset, 
                      OGRCoordinateTransformationH hTransform, OGRCoordinateTransformationH hTransformWGS84,
                      const char * corner_name,
                      double x, double y,
                      int bJson, json_object *poCornerCoordinates, json_object *poWGS84ExtentCoordinates,
                      CPLString& osStr )

{
    double  dfGeoX, dfGeoY;
    double  adfGeoTransform[6];
    json_object *poCorner, *poX, *poY;

    if(!bJson)
        osStr += CPLOPrintf( "%-11s ", corner_name );

/* -------------------------------------------------------------------- */
/*      Transform the point into georeferenced coordinates.             */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x
            + adfGeoTransform[2] * y;
        dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x
            + adfGeoTransform[5] * y;
    }

    else
    {
        if( bJson )
        {
            poCorner = json_object_new_array();
            poX = json_object_new_double_with_precision( x, 1 );
            poY = json_object_new_double_with_precision( y, 1 );
            json_object_array_add( poCorner, poX );
            json_object_array_add( poCorner, poY );
            json_object_object_add( poCornerCoordinates, corner_name, poCorner );
        }
        else
            osStr += CPLOPrintf( "(%7.1f,%7.1f)\n", x, y );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Report the georeferenced coordinates.                           */
/* -------------------------------------------------------------------- */
    if( ABS(dfGeoX) < 181 && ABS(dfGeoY) < 91 )
    {
        if(bJson)
        {
            poCorner = json_object_new_array();
            poX = json_object_new_double_with_precision( dfGeoX, 7 );
            poY = json_object_new_double_with_precision( dfGeoY, 7 );
            json_object_array_add( poCorner, poX );
            json_object_array_add( poCorner, poY );
            json_object_object_add( poCornerCoordinates, corner_name, poCorner );
        }
        else
            osStr += CPLOPrintf( "(%12.7f,%12.7f) ", dfGeoX, dfGeoY );
    }
    else
    {
        if(bJson)
        {
            poCorner = json_object_new_array();
            poX = json_object_new_double_with_precision( dfGeoX, 3 );
            poY = json_object_new_double_with_precision( dfGeoY, 3 );
            json_object_array_add( poCorner, poX );
            json_object_array_add( poCorner, poY );
            json_object_object_add( poCornerCoordinates, corner_name, poCorner );
        }
        else
            osStr += CPLOPrintf( "(%12.3f,%12.3f) ", dfGeoX, dfGeoY );
    }

/* -------------------------------------------------------------------- */
/*      Transform to latlong and report.                                */
/* -------------------------------------------------------------------- */
    if(bJson)
    {
        double dfZ = 0.0;
        if( hTransformWGS84 != NULL && !EQUAL( corner_name, "center" ) 
        && OCTTransform(hTransformWGS84,1,&dfGeoX,&dfGeoY,&dfZ) )
        {
            poCorner = json_object_new_array();
            poX = json_object_new_double_with_precision( dfGeoX, 7 );
            poY = json_object_new_double_with_precision( dfGeoY, 7 );
            json_object_array_add( poCorner, poX );
            json_object_array_add( poCorner, poY );
            json_object_array_add( poWGS84ExtentCoordinates , poCorner );
        }
    }
    else
    {
        double dfZ = 0.0;
        if( hTransform != NULL 
        && OCTTransform(hTransform,1,&dfGeoX,&dfGeoY,&dfZ) )
        {
            osStr += CPLOPrintf( "(%s,", GDALDecToDMS( dfGeoX, "Long", 2 ) );
            osStr += CPLOPrintf( "%s)", GDALDecToDMS( dfGeoY, "Lat", 2 ) );
        }
        osStr += CPLOPrintf( "\n" );
    }

    return TRUE;
}


/************************************************************************/
/*                       GDALInfoPrintMetadata()                        */
/************************************************************************/
static void GDALInfoPrintMetadata( GDALMajorObjectH hObject,
                                   const char *pszDomain,
                                   const char *pszDisplayedname,
                                   const char *pszIndent,
                                   int bJson,
                                   json_object *poMetadata,
                                   CPLString& osStr )
{
    bool bIsxml = false;
    char *pszKey = NULL;
    const char *pszValue;
    json_object *poDomain = NULL, *poValue = NULL;

    if (pszDomain != NULL && EQUALN(pszDomain, "xml:", 4))
        bIsxml = true;

    char **papszMetadata = GDALGetMetadata( hObject, pszDomain );
    if( CSLCount(papszMetadata) > 0 )
    {
        if(bJson && !bIsxml)
            poDomain = json_object_new_object();

        if(!bJson)
            osStr += CPLOPrintf( "%s%s:\n", pszIndent, pszDisplayedname );

        for( int i = 0; papszMetadata[i] != NULL; i++ )
        {
            if(bJson)
            {
                if(bIsxml)
                {
                    poValue = json_object_new_string( papszMetadata[i] );
                    break;
                }
                else
                {
                    pszKey = NULL;
                    pszValue = CPLParseNameValue( papszMetadata[i], &pszKey );
                    if( pszKey )
                    {
                        poValue = json_object_new_string( pszValue );
                        json_object_object_add( poDomain, pszKey, poValue );
                        CPLFree( pszKey );
                    }
                }
            }
            else
            {
                if (bIsxml)
                    osStr += CPLOPrintf( "%s%s\n", pszIndent, papszMetadata[i] );
                else
                    osStr += CPLOPrintf( "%s  %s\n", pszIndent, papszMetadata[i] );

            }
        }
        if(bJson)
        {
            if(bIsxml)
                json_object_object_add( poMetadata, pszDomain, poValue );
            else
            {
                if(pszDomain == NULL)
                    json_object_object_add( poMetadata, "", poDomain );
                else
                    json_object_object_add( poMetadata, pszDomain, poDomain );
            }
        }
    }

}

/************************************************************************/
/*                       GDALInfoReportMetadata()                       */
/************************************************************************/
static void GDALInfoReportMetadata( GDALMajorObjectH hObject,
                                    int bListMDD,
                                    int bShowMetadata,
                                    char **papszExtraMDDomains,
                                    int bIsBand,
                                    int bJson,
                                    json_object *poMetadata,
                                    CPLString& osStr )
{
    const char* pszIndent = "";
    if( bIsBand )
        pszIndent = "  ";

    /* -------------------------------------------------------------------- */
    /*      Report list of Metadata domains                                 */
    /* -------------------------------------------------------------------- */
    if( bListMDD )
    {
        char** papszMDDList = GDALGetMetadataDomainList( hObject );
        char** papszIter = papszMDDList;
        json_object *poMDD = NULL;
        json_object *poListMDD = NULL;
        if( bJson )
            poListMDD = json_object_new_array();

        if( papszMDDList != NULL )
        {
            if(!bJson)
                osStr += CPLOPrintf( "%sMetadata domains:\n", pszIndent );
        }

        while( papszIter != NULL && *papszIter != NULL )
        {
            if( EQUAL(*papszIter, "") )
            {
                if(bJson)
                    poMDD = json_object_new_string( *papszIter );
                else
                    osStr += CPLOPrintf( "%s  (default)\n", pszIndent);
            }
            else
            {
                if(bJson)
                    poMDD = json_object_new_string( *papszIter );
                else
                    osStr += CPLOPrintf( "%s  %s\n", pszIndent, *papszIter );
            }
            if(bJson)
                json_object_array_add( poListMDD, poMDD );
            papszIter ++;
        }
        if(bJson)
            json_object_object_add( poMetadata, "metadataDomains", poListMDD );
        CSLDestroy(papszMDDList);
    }

    if (!bShowMetadata)
        return;

    /* -------------------------------------------------------------------- */
    /*      Report default Metadata domain.                                 */
    /* -------------------------------------------------------------------- */
    GDALInfoPrintMetadata( hObject, NULL, "Metadata", pszIndent, bJson, poMetadata, osStr );

    /* -------------------------------------------------------------------- */
    /*      Report extra Metadata domains                                   */
    /* -------------------------------------------------------------------- */
    if (papszExtraMDDomains != NULL) {
        char **papszExtraMDDomainsExpanded = NULL;

        if( EQUAL(papszExtraMDDomains[0], "all") &&
            papszExtraMDDomains[1] == NULL )
        {
            char** papszMDDList = GDALGetMetadataDomainList( hObject );
            char** papszIter = papszMDDList;

            while( papszIter != NULL && *papszIter != NULL )
            {
                if( !EQUAL(*papszIter, "") &&
                    !EQUAL(*papszIter, "IMAGE_STRUCTURE") &&
                    !EQUAL(*papszIter, "SUBDATASETS") &&
                    !EQUAL(*papszIter, "GEOLOCATION") &&
                    !EQUAL(*papszIter, "RPC") )
                {
                    papszExtraMDDomainsExpanded = CSLAddString(papszExtraMDDomainsExpanded, *papszIter);
                }
                papszIter ++;
            }
            CSLDestroy(papszMDDList);
        }
        else
        {
            papszExtraMDDomainsExpanded = CSLDuplicate(papszExtraMDDomains);
        }

        for( int iMDD = 0; iMDD < CSLCount(papszExtraMDDomainsExpanded); iMDD++ )
        {
            char pszDisplayedname[256];
            snprintf(pszDisplayedname, 256, "Metadata (%s)", papszExtraMDDomainsExpanded[iMDD]);
            if(bJson)
                GDALInfoPrintMetadata( hObject, papszExtraMDDomainsExpanded[iMDD], papszExtraMDDomainsExpanded[iMDD], pszIndent, bJson, poMetadata, osStr );
            else
                GDALInfoPrintMetadata( hObject, papszExtraMDDomainsExpanded[iMDD], pszDisplayedname, pszIndent, bJson, poMetadata, osStr );
        }

        CSLDestroy(papszExtraMDDomainsExpanded);
    }

    /* -------------------------------------------------------------------- */
    /*      Report various named metadata domains.                          */
    /* -------------------------------------------------------------------- */
    GDALInfoPrintMetadata( hObject, "IMAGE_STRUCTURE", "Image Structure Metadata", pszIndent, bJson, poMetadata, osStr );

    if (!bIsBand)
    {
        GDALInfoPrintMetadata( hObject, "SUBDATASETS", "Subdatasets", pszIndent, bJson, poMetadata, osStr );
        GDALInfoPrintMetadata( hObject, "GEOLOCATION", "Geolocation", pszIndent, bJson, poMetadata, osStr );
        GDALInfoPrintMetadata( hObject, "RPC", "RPC Metadata", pszIndent, bJson, poMetadata, osStr );
    }

}

/************************************************************************/
/*                             GDALInfoOptionsNew()                     */
/************************************************************************/

/**
 * Allocates a GDALInfoOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="gdalinfo.html">gdalinfo</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdalinfo_bin.cpp use case) must be allocated with
 *                           GDALInfoOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options, subdataset number...
 * @return pointer to the allocated GDALInfoOptions struct. Must be freed with GDALInfoOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALInfoOptions *GDALInfoOptionsNew(char** papszArgv,
                                    GDALInfoOptionsForBinary* psOptionsForBinary)
{
    bool bGotFilename = false;
    GDALInfoOptions *psOptions = (GDALInfoOptions *) CPLCalloc( 1, sizeof(GDALInfoOptions) );

    psOptions->eFormat = GDALINFO_FORMAT_TEXT;
    psOptions->bComputeMinMax = FALSE;
    psOptions->bReportHistograms = FALSE;
    psOptions->bReportProj4 = FALSE;
    psOptions->bStats = FALSE;
    psOptions->bApproxStats = TRUE;
    psOptions->bSample = FALSE;
    psOptions->bComputeChecksum = FALSE;
    psOptions->bShowGCPs = TRUE;
    psOptions->bShowMetadata = TRUE;
    psOptions->bShowRAT = TRUE;
    psOptions->bShowColorTable = TRUE;
    psOptions->bListMDD = FALSE;
    psOptions->bShowFileList = TRUE;

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( int i = 0; papszArgv != NULL && papszArgv[i] != NULL; i++ )
    {
        if( EQUAL(papszArgv[i],"-json") )
            psOptions->eFormat = GDALINFO_FORMAT_JSON;
        else if( EQUAL(papszArgv[i], "-mm") )
            psOptions->bComputeMinMax = TRUE;
        else if( EQUAL(papszArgv[i], "-hist") )
            psOptions->bReportHistograms = TRUE;
        else if( EQUAL(papszArgv[i], "-proj4") )
            psOptions->bReportProj4 = TRUE;
        else if( EQUAL(papszArgv[i], "-stats") )
        {
            psOptions->bStats = TRUE;
            psOptions->bApproxStats = FALSE;
        }
        else if( EQUAL(papszArgv[i], "-approx_stats") )
        {
            psOptions->bStats = TRUE;
            psOptions->bApproxStats = TRUE;
        }
        else if( EQUAL(papszArgv[i], "-sample") )
            psOptions->bSample = TRUE;
        else if( EQUAL(papszArgv[i], "-checksum") )
            psOptions->bComputeChecksum = TRUE;
        else if( EQUAL(papszArgv[i], "-nogcp") )
            psOptions->bShowGCPs = FALSE;
        else if( EQUAL(papszArgv[i], "-nomd") )
            psOptions->bShowMetadata = FALSE;
        else if( EQUAL(papszArgv[i], "-norat") )
            psOptions->bShowRAT = FALSE;
        else if( EQUAL(papszArgv[i], "-noct") )
            psOptions->bShowColorTable = FALSE;
        else if( EQUAL(papszArgv[i], "-listmdd") )
            psOptions->bListMDD = TRUE;
        else if( EQUAL(papszArgv[i], "-mdd") && papszArgv[i+1] != NULL )
        {
            psOptions->papszExtraMDDomains = CSLAddString(
                psOptions->papszExtraMDDomains, papszArgv[++i] );
        }
        else if( EQUAL(papszArgv[i], "-oo") && papszArgv[i+1] != NULL )
        {
            i++;
            if( psOptionsForBinary )
            {
                psOptionsForBinary->papszOpenOptions = CSLAddString(
                     psOptionsForBinary->papszOpenOptions, papszArgv[i] );
            }
        }
        else if( EQUAL(papszArgv[i], "-nofl") )
            psOptions->bShowFileList = FALSE;
        else if( EQUAL(papszArgv[i], "-sd") && papszArgv[i+1] != NULL )
        {
            i++;
            if( psOptionsForBinary )
            {
                psOptionsForBinary->nSubdataset = atoi(papszArgv[i]);
            }
        }
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALInfoOptionsFree(psOptions);
            return NULL;
        }
        else if( !bGotFilename )
        {
            bGotFilename = true;
            if( psOptionsForBinary )
                psOptionsForBinary->pszFilename = CPLStrdup(papszArgv[i]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            GDALInfoOptionsFree(psOptions);
            return NULL;
        }
    }

    return psOptions;
}

/************************************************************************/
/*                             GDALInfoOptionsFree()                    */
/************************************************************************/

/**
 * Frees the GDALInfoOptions struct.
 *
 * @param psOptions the options struct for GDALInfo().
 *
 * @since GDAL 2.1
 */

void GDALInfoOptionsFree( GDALInfoOptions *psOptions )
{
    if( psOptions != NULL )
    {
        CSLDestroy( psOptions->papszExtraMDDomains );

        CPLFree(psOptions);
    }
}
