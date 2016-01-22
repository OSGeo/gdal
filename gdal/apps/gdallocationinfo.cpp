/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Commandline raster query tool.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "cpl_minixml.h"
#include <vector>

CPL_CVSID("$Id$");

/******************************************************************************/
/*! \page gdallocationinfo gdallocationinfo

raster query tool

\section gdallocationinfo_synopsis SYNOPSIS

\htmlonly
Usage: 
\endhtmlonly

\verbatim
Usage: gdallocationinfo [--help-general] [-xml] [-lifonly] [-valonly]
                        [-b band]* [-overview overview_level]
                        [-l_srs srs_def] [-geoloc] [-wgs84]
                        srcfile [x y]
\endverbatim

\section gdallocationinfo_description DESCRIPTION

<p>
The gdallocationinfo utility provide a mechanism to query information about
a pixel given it's location in one of a variety of coordinate systems.  Several
reporting options are provided.

<p>
<dl>
<dt> <b>-xml</b>:</dt>
<dd> The output report will be XML formatted for convenient post processing.</dd>

<dt> <b>-lifonly</b>:</dt> 
<dd>The only output is filenames production from the LocationInfo request
against the database (ie. for identifying impacted file from VRT).</dd>

<dt> <b>-valonly</b>:</dt> 
<dd>The only output is the pixel values of the selected pixel on each of
the selected bands.</dd>

<dt> <b>-b</b> <em>band</em>:</dt> 
<dd>Selects a band to query.  Multiple bands can be listed.  By default all
bands are queried.</dd>

<dt> <b>-overview</b> <em>overview_level</em>:</dt>
<dd>Query the (overview_level)th overview (overview_level=1 is the 1st overview),
instead of the base band. Note that the x,y location (if the coordinate system is
pixel/line) must still be given with respect to the base band.</dd>

<dt> <b>-l_srs</b> <em>srs def</em>:</dt>
<dd> The coordinate system of the input x, y location.</dd>

<dt> <b>-geoloc</b>:</dt>
<dd> Indicates input x,y points are in the georeferencing system of the image.</dd>

<dt> <b>-wgs84</b>:</dt>
<dd> Indicates input x,y points are WGS84 long, lat.</dd>

<dt> <em>srcfile</em>:</dt><dd> The source GDAL raster datasource name.</dd>

<dt> <em>x</em>:</dt><dd> X location of target pixel.  By default the 
coordinate system is pixel/line unless -l_srs, -wgs84 or -geoloc supplied. </dd>

<dt> <em>y</em>:</dt><dd> Y location of target pixel.  By default the 
coordinate system is pixel/line unless -l_srs, -wgs84 or -geoloc supplied. </dd>

</dl>

This utility is intended to provide a variety of information about a 
pixel.  Currently it reports three things:

<ul>
<li> The location of the pixel in pixel/line space.
<li> The result of a LocationInfo metadata query against the datasource - 
currently this is only implemented for VRT files which will report the
file(s) used to satisfy requests for that pixel.
<li> The raster pixel value of that pixel for all or a subset of the bands.
<li> The unscaled pixel value if a Scale and/or Offset apply to the band.
</ul>

The pixel selected is requested by x/y coordinate on the commandline, or read
from stdin. More than one coordinate pair can be supplied when reading
coordinatesis from stdin. By default pixel/line coordinates are expected.
However with use of the -geoloc, -wgs84, or -l_srs switches it is possible
to specify the location in other coordinate systems.

The default report is in a human readable text format.  It is possible to 
instead request xml output with the -xml switch.

For scripting purposes, the -valonly and -lifonly switches are provided to
restrict output to the actual pixel values, or the LocationInfo files 
identified for the pixel.

It is anticipated that additional reporting capabilities will be added to 
gdallocationinfo in the future. 

<p>
\section gdallocationinfo_example EXAMPLE

Simple example reporting on pixel (256,256) on the file utm.tif.

\verbatim
$ gdallocationinfo utm.tif 256 256
Report:
  Location: (256P,256L)
  Band 1:
    Value: 115
\endverbatim

Query a VRT file providing the location in WGS84, and getting the result in xml.

\verbatim
$ gdallocationinfo -xml -wgs84 utm.vrt -117.5 33.75
<Report pixel="217" line="282">
  <BandReport band="1">
    <LocationInfo>
      <File>utm.tif</File>
    </LocationInfo>
    <Value>16</Value>
  </BandReport>
</Report>
\endverbatim

\if man
\section gdallocationinfo_author AUTHORS
Frank Warmerdam <warmerdam@pobox.com>
\endif
*/

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: gdallocationinfo [--help-general] [-xml] [-lifonly] [-valonly]\n"
            "                        [-b band]* [-overview overview_level]\n"
            "                        [-l_srs srs_def] [-geoloc] [-wgs84]\n"
            "                        srcfile x y\n" 
            "\n" );
    exit( 1 );
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

char *SanitizeSRS( const char *pszUserInput )

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = NULL;

    CPLErrorReset();
    
    hSRS = OSRNewSpatialReference( NULL );
    if( OSRSetFromUserInput( hSRS, pszUserInput ) == OGRERR_NONE )
        OSRExportToWkt( hSRS, &pszResult );
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Translating source or target SRS failed:\n%s",
                  pszUserInput );
        exit( 1 );
    }
    
    OSRDestroySpatialReference( hSRS );

    return pszResult;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    const char         *pszLocX = NULL, *pszLocY = NULL;
    const char         *pszSrcFilename = NULL;
    char               *pszSourceSRS = NULL;
    std::vector<int>   anBandList;
    bool               bAsXML = false, bLIFOnly = false;
    bool               bQuiet = false, bValOnly = false;
    int                nOverview = -1;

    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i],"-b") && i < argc-1 )
        {
            anBandList.push_back( atoi(argv[++i]) );
        }
        else if( EQUAL(argv[i],"-overview") && i < argc-1 )
        {
            nOverview = atoi(argv[++i]) - 1;
        }
        else if( EQUAL(argv[i],"-l_srs") && i < argc-1 )
        {
            CPLFree(pszSourceSRS);
            pszSourceSRS = SanitizeSRS(argv[++i]);
        }
        else if( EQUAL(argv[i],"-geoloc") )
        {
            CPLFree(pszSourceSRS);
            pszSourceSRS = CPLStrdup("-geoloc");
        }
        else if( EQUAL(argv[i],"-wgs84") )
        {
            CPLFree(pszSourceSRS);
            pszSourceSRS = SanitizeSRS("WGS84");
        }
        else if( EQUAL(argv[i],"-xml") )
        {
            bAsXML = true;
        }
        else if( EQUAL(argv[i],"-lifonly") )
        {
            bLIFOnly = true;
            bQuiet = true;
        }
        else if( EQUAL(argv[i],"-valonly") )
        {
            bValOnly = true;
            bQuiet = true;
        }
        else if( argv[i][0] == '-' && !isdigit(argv[i][1]) )
            Usage();

        else if( pszSrcFilename == NULL )
            pszSrcFilename = argv[i];

        else if( pszLocX == NULL )
            pszLocX = argv[i];

        else if( pszLocY == NULL )
            pszLocY = argv[i];

        else
            Usage();
    }

    if( pszSrcFilename == NULL || (pszLocX != NULL && pszLocY == NULL) )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open source file.                                               */
/* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS = NULL;

    hSrcDS = GDALOpen( pszSrcFilename, GA_ReadOnly );
    if( hSrcDS == NULL )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Setup coordinate transformation, if required                    */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hSrcSRS = NULL, hTrgSRS = NULL;
    OGRCoordinateTransformationH hCT = NULL;
    if( pszSourceSRS != NULL && !EQUAL(pszSourceSRS,"-geoloc") )
    {

        hSrcSRS = OSRNewSpatialReference( pszSourceSRS );
        hTrgSRS = OSRNewSpatialReference( GDALGetProjectionRef( hSrcDS ) );

        hCT = OCTNewCoordinateTransformation( hSrcSRS, hTrgSRS );
        if( hCT == NULL )
            exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      If no bands were requested, we will query them all.             */
/* -------------------------------------------------------------------- */
    if( anBandList.size() == 0 )
    {
        for( i = 0; i < GDALGetRasterCount( hSrcDS ); i++ )
            anBandList.push_back( i+1 );
    }
    
/* -------------------------------------------------------------------- */
/*      Turn the location into a pixel and line location.               */
/* -------------------------------------------------------------------- */
    int inputAvailable = 1;
    double dfGeoX;
    double dfGeoY;
    CPLString osXML;

    if( pszLocX == NULL && pszLocY == NULL )
    {
        if (fscanf(stdin, "%lf %lf", &dfGeoX, &dfGeoY) != 2)
        {
            inputAvailable = 0;
        }
    }
    else
    {
        dfGeoX = CPLAtof(pszLocX);
        dfGeoY = CPLAtof(pszLocY);
    }

    while (inputAvailable)
    {
        int iPixel, iLine;

        if (hCT)
        {
            if( !OCTTransform( hCT, 1, &dfGeoX, &dfGeoY, NULL ) )
                exit( 1 );
        }
    
        if( pszSourceSRS != NULL )
        {
            double adfGeoTransform[6], adfInvGeoTransform[6];
    
            if( GDALGetGeoTransform( hSrcDS, adfGeoTransform ) != CE_None )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot get geotransform");
                exit( 1 );
            }
    
            if( !GDALInvGeoTransform( adfGeoTransform, adfInvGeoTransform ) )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
                exit( 1 );
            }
    
            iPixel = (int) floor(
                adfInvGeoTransform[0] 
                + adfInvGeoTransform[1] * dfGeoX
                + adfInvGeoTransform[2] * dfGeoY );
            iLine = (int) floor(
                adfInvGeoTransform[3] 
                + adfInvGeoTransform[4] * dfGeoX
                + adfInvGeoTransform[5] * dfGeoY );
        }
        else
        {
            iPixel = (int) floor(dfGeoX);
            iLine  = (int) floor(dfGeoY);
        }

    /* -------------------------------------------------------------------- */
    /*      Prepare report.                                                 */
    /* -------------------------------------------------------------------- */
        CPLString osLine;
    
        if( bAsXML )
        {
            osLine.Printf( "<Report pixel=\"%d\" line=\"%d\">", 
                          iPixel, iLine );
            osXML += osLine;
        }
        else if( !bQuiet )
        {
            printf( "Report:\n" );
            printf( "  Location: (%dP,%dL)\n", iPixel, iLine );
        }

        int bPixelReport = TRUE;

        if( iPixel < 0 || iLine < 0 
            || iPixel >= GDALGetRasterXSize( hSrcDS )
            || iLine  >= GDALGetRasterYSize( hSrcDS ) )
        {
            if( bAsXML )
                osXML += "<Alert>Location is off this file! No further details to report.</Alert>";
            else if( bValOnly )
                printf("\n");
            else if( !bQuiet )
                printf( "\nLocation is off this file! No further details to report.\n");
            bPixelReport = FALSE;
        }

    /* -------------------------------------------------------------------- */
    /*      Process each band.                                              */
    /* -------------------------------------------------------------------- */
        for( i = 0; bPixelReport && i < (int) anBandList.size(); i++ )
        {
            GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, anBandList[i] );

            int iPixelToQuery = iPixel;
            int iLineToQuery = iLine;

            if (nOverview >= 0 && hBand != NULL)
            {
                GDALRasterBandH hOvrBand = GDALGetOverview(hBand, nOverview);
                if (hOvrBand != NULL)
                {
                    int nOvrXSize = GDALGetRasterBandXSize(hOvrBand);
                    int nOvrYSize = GDALGetRasterBandYSize(hOvrBand);
                    iPixelToQuery = (int)(0.5 + 1.0 * iPixel / GDALGetRasterXSize( hSrcDS ) * nOvrXSize);
                    iLineToQuery = (int)(0.5 + 1.0 * iLine / GDALGetRasterYSize( hSrcDS ) * nOvrYSize);
                    if (iPixelToQuery >= nOvrXSize)
                        iPixelToQuery = nOvrXSize - 1;
                    if (iLineToQuery >= nOvrYSize)
                        iLineToQuery = nOvrYSize - 1;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot get overview %d of band %d",
                             nOverview + 1, anBandList[i] );
                }
                hBand = hOvrBand;
            }

            if (hBand == NULL)
                continue;

            if( bAsXML )
            {
                osLine.Printf( "<BandReport band=\"%d\">", anBandList[i] );
                osXML += osLine;
            }
            else if( !bQuiet )
            {
                printf( "  Band %d:\n", anBandList[i] );
            }
    
    /* -------------------------------------------------------------------- */
    /*      Request location info for this location.  It is possible        */
    /*      only the VRT driver actually supports this.                     */
    /* -------------------------------------------------------------------- */
            CPLString osItem;
            
            osItem.Printf( "Pixel_%d_%d", iPixelToQuery, iLineToQuery );
            
            const char *pszLI = GDALGetMetadataItem( hBand, osItem, "LocationInfo");
    
            if( pszLI != NULL )
            {
                if( bAsXML )
                    osXML += pszLI;
                else if( !bQuiet )
                    printf( "    %s\n", pszLI );
                else if( bLIFOnly )
                {
                    /* Extract all files, if any. */
                 
                    CPLXMLNode *psRoot = CPLParseXMLString( pszLI );
                    
                    if( psRoot != NULL 
                        && psRoot->psChild != NULL
                        && psRoot->eType == CXT_Element
                        && EQUAL(psRoot->pszValue,"LocationInfo") )
                    {
                        CPLXMLNode *psNode;
    
                        for( psNode = psRoot->psChild;
                             psNode != NULL;
                             psNode = psNode->psNext )
                        {
                            if( psNode->eType == CXT_Element
                                && EQUAL(psNode->pszValue,"File") 
                                && psNode->psChild != NULL )
                            {
                                char* pszUnescaped = CPLUnescapeString(
                                    psNode->psChild->pszValue, NULL, CPLES_XML);
                                printf( "%s\n", pszUnescaped );
                                CPLFree(pszUnescaped);
                            }
                        }
                    }
                    CPLDestroyXMLNode( psRoot );
                }
            }
    
    /* -------------------------------------------------------------------- */
    /*      Report the pixel value of this band.                            */
    /* -------------------------------------------------------------------- */
            double adfPixel[2];
    
            if( GDALRasterIO( hBand, GF_Read, iPixelToQuery, iLineToQuery, 1, 1, 
                              adfPixel, 1, 1, GDT_CFloat64, 0, 0) == CE_None )
            {
                CPLString osValue;
    
                if( GDALDataTypeIsComplex( GDALGetRasterDataType( hBand ) ) )
                    osValue.Printf( "%.15g+%.15gi", adfPixel[0], adfPixel[1] );
                else
                    osValue.Printf( "%.15g", adfPixel[0] );
    
                if( bAsXML )
                {
                    osXML += "<Value>";
                    osXML += osValue;
                    osXML += "</Value>";
                }
                else if( !bQuiet )
                    printf( "    Value: %s\n", osValue.c_str() );
                else if( bValOnly )
                    printf( "%s\n", osValue.c_str() );
    
                // Report unscaled if we have scale/offset values.
                int bSuccess;
                
                double dfOffset = GDALGetRasterOffset( hBand, &bSuccess );
                double dfScale  = GDALGetRasterScale( hBand, &bSuccess );
    
                if( dfOffset != 0.0 || dfScale != 1.0 )
                {
                    adfPixel[0] = adfPixel[0] * dfScale + dfOffset;
                    adfPixel[1] = adfPixel[1] * dfScale + dfOffset;
    
                    if( GDALDataTypeIsComplex( GDALGetRasterDataType( hBand ) ) )
                        osValue.Printf( "%.15g+%.15gi", adfPixel[0], adfPixel[1] );
                    else
                        osValue.Printf( "%.15g", adfPixel[0] );
    
                    if( bAsXML )
                    {
                        osXML += "<DescaledValue>";
                        osXML += osValue;
                        osXML += "</DescaledValue>";
                    }
                    else if( !bQuiet )
                        printf( "    Descaled Value: %s\n", osValue.c_str() );
                }
            }
    
            if( bAsXML )
                osXML += "</BandReport>";
        }

        osXML += "</Report>";
    
        if( (pszLocX != NULL && pszLocY != NULL)  ||
            (fscanf(stdin, "%lf %lf", &dfGeoX, &dfGeoY) != 2) )
        {
            inputAvailable = 0;
        }
            
    }

/* -------------------------------------------------------------------- */
/*      Finalize xml report and print.                                  */
/* -------------------------------------------------------------------- */
    if( bAsXML )
    {
        CPLXMLNode *psRoot;
        char *pszFormattedXML;


        psRoot = CPLParseXMLString( osXML );
        pszFormattedXML = CPLSerializeXMLTree( psRoot );
        CPLDestroyXMLNode( psRoot );

        printf( "%s", pszFormattedXML );
        CPLFree( pszFormattedXML );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if (hCT) {
        OSRDestroySpatialReference( hSrcSRS );
        OSRDestroySpatialReference( hTrgSRS );
        OCTDestroyCoordinateTransformation( hCT );
    }

    if (hSrcDS)
        GDALClose(hSrcDS);

    GDALDumpOpenDatasets( stderr );
    GDALDestroyDriverManager();
    CPLFree(pszSourceSRS);

    CSLDestroy( argv );

    return 0;
}
