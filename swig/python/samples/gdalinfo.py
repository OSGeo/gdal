#!/usr/bin/env python
#/******************************************************************************
# * $Id$
# *
# * Project:  GDAL Utilities
# * Purpose:  Python port of Commandline application to list info about a file.
# * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# *
# * Port from gdalinfo.c whose author is Frank Warmerdam
# *
# ******************************************************************************
# * Copyright (c) 2010, Even Rouault
# * Copyright (c) 1998, Frank Warmerdam
# *
# * Permission is hereby granted, free of charge, to any person obtaining a
# * copy of this software and associated documentation files (the "Software"),
# * to deal in the Software without restriction, including without limitation
# * the rights to use, copy, modify, merge, publish, distribute, sublicense,
# * and/or sell copies of the Software, and to permit persons to whom the
# * Software is furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included
# * in all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# * DEALINGS IN THE SOFTWARE.
# ****************************************************************************/

import sys
from osgeo import gdal
from osgeo import osr

#/************************************************************************/
#/*                               Usage()                                */
#/************************************************************************/

def Usage():
    print( "Usage: gdalinfo [--help-general] [-mm] [-stats] [-hist] [-nogcp] [-nomd]\n" + \
            "                [-norat] [-noct] [-checksum] [-mdd domain]* datasetname" )
    return 1


def EQUAL(a, b):
    return a.lower() == b.lower()

#/************************************************************************/
#/*                                main()                                */
#/************************************************************************/

def main( argv = None ):

    bComputeMinMax = False
    bSample = False
    bShowGCPs = True
    bShowMetadata = True
    bShowRAT=True
    bStats = False
    bApproxStats = True
    bShowColorTable = True
    bComputeChecksum = False
    bReportHistograms = False
    pszFilename = None
    papszExtraMDDomains = [ ]
    pszProjection = None
    hTransform = None

    #/* Must process GDAL_SKIP before GDALAllRegister(), but we can't call */
    #/* GDALGeneralCmdLineProcessor before it needs the drivers to be registered */
    #/* for the --format or --formats options */
    #for( i = 1; i < argc; i++ )
    #{
    #    if EQUAL(argv[i],"--config") and i + 2 < argc and EQUAL(argv[i + 1], "GDAL_SKIP"):
    #    {
    #        CPLSetConfigOption( argv[i+1], argv[i+2] );
    #
    #        i += 2;
    #    }
    #}
    #
    #GDALAllRegister();

    if argv is None:
        argv = sys.argv

    argv = gdal.GeneralCmdLineProcessor( argv )

    if argv is None:
        return 1

    nArgc = len(argv)
#/* -------------------------------------------------------------------- */
#/*      Parse arguments.                                                */
#/* -------------------------------------------------------------------- */
    i = 1
    while i < nArgc:

        if EQUAL(argv[i], "--utility_version"):
            print("%s is running against GDAL %s" %
                   (argv[0], gdal.VersionInfo("RELEASE_NAME")))
            return 0
        elif EQUAL(argv[i], "-mm"):
            bComputeMinMax = True
        elif EQUAL(argv[i], "-hist"):
            bReportHistograms = True
        elif EQUAL(argv[i], "-stats"):
            bStats = True
            bApproxStats = False
        elif EQUAL(argv[i], "-approx_stats"):
            bStats = True
            bApproxStats = True
        elif EQUAL(argv[i], "-sample"):
            bSample = True
        elif EQUAL(argv[i], "-checksum"):
            bComputeChecksum = True
        elif EQUAL(argv[i], "-nogcp"):
            bShowGCPs = False
        elif EQUAL(argv[i], "-nomd"):
            bShowMetadata = False
        elif EQUAL(argv[i], "-norat"):
            bShowRAT = False
        elif EQUAL(argv[i], "-noct"):
            bShowColorTable = False
        elif EQUAL(argv[i], "-mdd") and i < argc-1:
            i = i + 1
            papszExtraMDDomains.append( argv[i] )
        elif argv[i][0] == '-':
            Usage()
        elif pszFilename is None:
            pszFilename = argv[i]
        else:
            Usage()

        i = i + 1

    if pszFilename is None:
        Usage()

#/* -------------------------------------------------------------------- */
#/*      Open dataset.                                                   */
#/* -------------------------------------------------------------------- */
    hDataset = gdal.Open( pszFilename, gdal.GA_ReadOnly )

    if hDataset is None:

        print("gdalinfo failed - unable to open '%s'." % pszFilename )

        return 1
    
#/* -------------------------------------------------------------------- */
#/*      Report general info.                                            */
#/* -------------------------------------------------------------------- */
    hDriver = hDataset.GetDriver();
    print( "Driver: %s/%s" % ( \
            hDriver.ShortName, \
            hDriver.LongName ))

    papszFileList = hDataset.GetFileList();
    if papszFileList is None or len(papszFileList) == 0:
        print( "Files: none associated" )
    else:
        print( "Files: %s" % papszFileList[0] )
        for i in range(1, len(papszFileList)):
            print( "       %s" % papszFileList[i] )

    print( "Size is %d, %d" % (hDataset.RasterXSize, hDataset.RasterYSize))

#/* -------------------------------------------------------------------- */
#/*      Report projection.                                              */
#/* -------------------------------------------------------------------- */
    pszProjection = hDataset.GetProjectionRef()
    if pszProjection is not None:

        hSRS = osr.SpatialReference()
        if hSRS.ImportFromWkt(pszProjection ) == gdal.CE_None:
            pszPrettyWkt = hSRS.ExportToPrettyWkt(False)

            print( "Coordinate System is:\n%s" % pszPrettyWkt )
        else:
            print( "Coordinate System is `%s'" % pszProjection )

#/* -------------------------------------------------------------------- */
#/*      Report Geotransform.                                            */
#/* -------------------------------------------------------------------- */
    adfGeoTransform = hDataset.GetGeoTransform(can_return_null = True)
    if adfGeoTransform is not None:

        if adfGeoTransform[2] == 0.0 and adfGeoTransform[4] == 0.0:
            print( "Origin = (%.15f,%.15f)" % ( \
                    adfGeoTransform[0], adfGeoTransform[3] ))

            print( "Pixel Size = (%.15f,%.15f)" % ( \
                    adfGeoTransform[1], adfGeoTransform[5] ))

        else:
            print( "GeoTransform =\n" \
                    "  %.16g, %.16g, %.16g\n" \
                    "  %.16g, %.16g, %.16g" % ( \
                    adfGeoTransform[0], \
                    adfGeoTransform[1], \
                    adfGeoTransform[2], \
                    adfGeoTransform[3], \
                    adfGeoTransform[4], \
                    adfGeoTransform[5] ))

#/* -------------------------------------------------------------------- */
#/*      Report GCPs.                                                    */
#/* -------------------------------------------------------------------- */
    if bShowGCPs and hDataset.GetGCPCount() > 0:

        pszProjection = hDataset.GetGCPProjection()
        if pszProjection is not None:

            hSRS = osr.SpatialReference()
            if hSRS.ImportFromWkt(pszProjection ) == gdal.CE_None:
                pszPrettyWkt = hSRS.ExportToPrettyWkt(False)
                print( "GCP Projection = \n%s" % pszPrettyWkt )

            else:
                print( "GCP Projection = %s" % \
                        pszProjection )

        gcps = hDataset.GetGCPs()
        i = 0
        for gcp in gcps:

            print( "GCP[%3d]: Id=%s, Info=%s\n" \
                    "          (%.15g,%.15g) -> (%.15g,%.15g,%.15g)" % ( \
                    i, gcp.Id, gcp.Info, \
                    gcp.GCPPixel, gcp.GCPLine, \
                    gcp.GCPX, gcp.GCPY, gcp.GCPZ ))
            i = i + 1

#/* -------------------------------------------------------------------- */
#/*      Report metadata.                                                */
#/* -------------------------------------------------------------------- */
    if bShowMetadata:
        papszMetadata = hDataset.GetMetadata_List()
    else:
        papszMetadata = None
    if bShowMetadata and papszMetadata is not None and len(papszMetadata) > 0 :
        print( "Metadata:" )
        for metadata in papszMetadata:
            print( "  %s" % metadata )

    if bShowMetadata:
        for extra_domain in papszExtraMDDomains:
            papszMetadata = hDataset.GetMetadata_List(extra_domain)
            if papszMetadata is not None and len(papszMetadata) > 0 :
                print( "Metadata (%s):" % extra_domain)
                for metadata in papszMetadata:
                  print( "  %s" % metadata )

#/* -------------------------------------------------------------------- */
#/*      Report "IMAGE_STRUCTURE" metadata.                              */
#/* -------------------------------------------------------------------- */
    if bShowMetadata:
        papszMetadata = hDataset.GetMetadata_List("IMAGE_STRUCTURE")
    else:
        papszMetadata = None
    if bShowMetadata and papszMetadata is not None and len(papszMetadata) > 0 :
        print( "Image Structure Metadata:" )
        for metadata in papszMetadata:
            print( "  %s" % metadata )

#/* -------------------------------------------------------------------- */
#/*      Report subdatasets.                                             */
#/* -------------------------------------------------------------------- */
    papszMetadata = hDataset.GetMetadata_List("SUBDATASETS")
    if papszMetadata is not None and len(papszMetadata) > 0 :
        print( "Subdatasets:" )
        for metadata in papszMetadata:
            print( "  %s" % metadata )

#/* -------------------------------------------------------------------- */
#/*      Report geolocation.                                             */
#/* -------------------------------------------------------------------- */
    if bShowMetadata:
        papszMetadata = hDataset.GetMetadata_List("GEOLOCATION")
    else:
        papszMetadata = None
    if bShowMetadata and papszMetadata is not None and len(papszMetadata) > 0 :
        print( "Geolocation:" )
        for metadata in papszMetadata:
            print( "  %s" % metadata )

#/* -------------------------------------------------------------------- */
#/*      Report RPCs                                                     */
#/* -------------------------------------------------------------------- */
    if bShowMetadata:
        papszMetadata = hDataset.GetMetadata_List("RPC")
    else:
        papszMetadata = None
    if bShowMetadata and papszMetadata is not None and len(papszMetadata) > 0 :
        print( "RPC Metadata:" )
        for metadata in papszMetadata:
            print( "  %s" % metadata )

#/* -------------------------------------------------------------------- */
#/*      Setup projected to lat/long transform if appropriate.           */
#/* -------------------------------------------------------------------- */
    if pszProjection is not None and len(pszProjection) > 0:
        hProj = osr.SpatialReference( pszProjection )
        if hProj is not None:
            hLatLong = hProj.CloneGeogCS()

        if hLatLong is not None:
            gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
            hTransform = osr.CoordinateTransformation( hProj, hLatLong )
            gdal.PopErrorHandler()

#/* -------------------------------------------------------------------- */
#/*      Report corners.                                                 */
#/* -------------------------------------------------------------------- */
    print( "Corner Coordinates:" )
    GDALInfoReportCorner( hDataset, hTransform, "Upper Left", \
                          0.0, 0.0 );
    GDALInfoReportCorner( hDataset, hTransform, "Lower Left", \
                          0.0, hDataset.RasterYSize);
    GDALInfoReportCorner( hDataset, hTransform, "Upper Right", \
                          hDataset.RasterXSize, 0.0 );
    GDALInfoReportCorner( hDataset, hTransform, "Lower Right", \
                          hDataset.RasterXSize, \
                          hDataset.RasterYSize );
    GDALInfoReportCorner( hDataset, hTransform, "Center", \
                          hDataset.RasterXSize/2.0, \
                          hDataset.RasterYSize/2.0 );

#/* ==================================================================== */
#/*      Loop over bands.                                                */
#/* ==================================================================== */
    for iBand in range(hDataset.RasterCount):

        hBand = hDataset.GetRasterBand(iBand+1 )

        #if( bSample )
        #{
        #    float afSample[10000];
        #    int   nCount;
        #
        #    nCount = GDALGetRandomRasterSample( hBand, 10000, afSample );
        #    print( "Got %d samples.\n", nCount );
        #}

        (nBlockXSize, nBlockYSize) = hBand.GetBlockSize()
        print( "Band %d Block=%dx%d Type=%s, ColorInterp=%s" % ( iBand+1, \
                nBlockXSize, nBlockYSize, \
                gdal.GetDataTypeName(hBand.DataType), \
                gdal.GetColorInterpretationName( \
                    hBand.GetRasterColorInterpretation()) ))

        if hBand.GetDescription() is not None \
            and len(hBand.GetDescription()) > 0 :
            print( "  Description = %s", hBand.GetDescription() )

        dfMin = hBand.GetMinimum()
        dfMax = hBand.GetMaximum()
        if dfMin is not None or dfMax is not None or bComputeMinMax:

            line =  "  "
            if dfMin is not None:
                line = line + ("Min=%.3f " % dfMin)
            if dfMax is not None:
                line = line + ("Max=%.3f " % dfMax)

            if bComputeMinMax:
                gdal.ErrorReset()
                adfCMinMax = hBand.ComputeRasterMinMax(False)
                if gdal.GetLastErrorType() == gdal.CE_None:
                  line = line + ( "  Computed Min/Max=%.3f,%.3f" % ( \
                          adfCMinMax[0], adfCMinMax[1] ))

            print( line )

        stats = hBand.GetStatistics( bApproxStats, bStats)
        # Dirty hack to recognize if stats are valid. If invalid, the returned
        # stddev is negative
        if stats[3] >= 0.0:
            print( "  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f" % ( \
                    stats[0], stats[1], stats[2], stats[3] ))

        if bReportHistograms:

            hist = hBand.GetDefaultHistogram(True, callback = gdal.TermProgress)
            if hist is not None:
                dfMin = hist[0]
                dfMax = hist[1]
                nBucketCount = hist[2]
                panHistogram = hist[3]

                print( "  %d buckets from %g to %g:  " % ( \
                        nBucketCount, dfMin, dfMax ))
                line = ''
                for bucket in panHistogram:
                    line = line + ("%d " % bucket)

                print(line)

        if bComputeChecksum:
            print( "  Checksum=%d" % hBand.Checksum())

        dfNoData = hBand.GetNoDataValue()
        if dfNoData is not None:
            print( "  NoData Value=%.18g" % dfNoData )

        if hBand.GetOverviewCount() > 0:

            line = "  Overviews: "
            for iOverview in range(hBand.GetOverviewCount()):

                if iOverview != 0 :
                    line = line +  ", "

                hOverview = hBand.GetOverview( iOverview );
                if hOverview is not None:

                    line = line + ( "%dx%d" % (hOverview.XSize, hOverview.YSize))

                    pszResampling = \
                        hOverview.GetMetadataItem( "RESAMPLING", "" )

                    if pszResampling is not None \
                       and len(pszResampling) >= 12 \
                       and EQUAL(pszResampling[0:12],"AVERAGE_BIT2"):
                        line = line + "*"

                else:
                    line = line + "(null)"

            print(line)

            if bComputeChecksum:

                line = "  Overviews checksum: "
                for iOverview in range(hBand.GetOverviewCount()):

                    if iOverview != 0:
                        line = line +  ", "

                    hOverview = hBand.GetOverview( iOverview );
                    if hOverview is not None:
                        line = line + ( "%d" % hOverview.Checksum())
                    else:
                        line = line + "(null)"
                print(line)

        if hBand.HasArbitraryOverviews():
            print( "  Overviews: arbitrary" )

        nMaskFlags = hBand.GetMaskFlags()
        if (nMaskFlags & (gdal.GMF_NODATA|gdal.GMF_ALL_VALID)) == 0:

            hMaskBand = hBand.GetMaskBand()

            line = "  Mask Flags: "
            if (nMaskFlags & gdal.GMF_PER_DATASET) != 0:
                line = line + "PER_DATASET "
            if (nMaskFlags & gdal.GMF_ALPHA) != 0:
                line = line + "ALPHA "
            if (nMaskFlags & gdal.GMF_NODATA) != 0:
                line = line + "NODATA "
            if (nMaskFlags & gdal.GMF_ALL_VALID) != 0:
                line = line + "ALL_VALID "
            print(line)

            if hMaskBand is not None and \
                hMaskBand.GetOverviewCount() > 0:

                line = "  Overviews of mask band: "
                for iOverview in range(hMaskBand.GetOverviewCount()):

                    if iOverview != 0:
                        line = line +  ", "

                    hOverview = hMaskBand.GetOverview( iOverview );
                    if hOverview is not None:
                        line = line + ( "%d" % hOverview.Checksum())
                    else:
                        line = line + "(null)"

        if len(hBand.GetUnitType()) > 0:
            print( "  Unit Type: %s" % hBand.GetUnitType())

        papszCategories = hBand.GetRasterCategoryNames()
        if papszCategories is not None:

            print( "  Categories:" );
            i = 0
            for category in papszCategories:
                print( "    %3d: %s" % (i, category) )
                i = i + 1

        if hBand.GetScale() != 1.0 or hBand.GetOffset() != 0.0:
            print( "  Offset: %.15g,   Scale:%.15g" % \
                        ( hBand.GetOffset(), hBand.GetScale()))

        if bShowMetadata:
            papszMetadata = hBand.GetMetadata_List()
        else:
            papszMetadata = None
        if bShowMetadata and papszMetadata is not None and len(papszMetadata) > 0 :
            print( "  Metadata:" )
            for metadata in papszMetadata:
                print( "    %s" % metadata )


        if bShowMetadata:
            papszMetadata = hBand.GetMetadata_List("IMAGE_STRUCTURE")
        else:
            papszMetadata = None
        if bShowMetadata and papszMetadata is not None and len(papszMetadata) > 0 :
            print( "  Image Structure Metadata:" )
            for metadata in papszMetadata:
                print( "    %s" % metadata )


        hTable = hBand.GetRasterColorTable()
        if hBand.GetRasterColorInterpretation() == gdal.GCI_PaletteIndex  \
            and hTable is not None:

            print( "  Color Table (%s with %d entries)" % (\
                    gdal.GetPaletteInterpretationName( \
                        hTable.GetPaletteInterpretation(  )), \
                    hTable.GetCount() ))

            if bShowColorTable:

                for i in range(hTable.GetCount()):
                    sEntry = hTable.GetColorEntry(i)
                    print( "  %3d: %d,%d,%d,%d" % ( \
                            i, \
                            sEntry[0],\
                            sEntry[1],\
                            sEntry[2],\
                            sEntry[3] ))

        if bShowRAT:
            hRAT = hBand.GetDefaultRAT()

            #GDALRATDumpReadable( hRAT, None );

    return 0

#/************************************************************************/
#/*                        GDALInfoReportCorner()                        */
#/************************************************************************/

def GDALInfoReportCorner( hDataset, hTransform, corner_name, x, y ):


    line = "%-11s " % corner_name

#/* -------------------------------------------------------------------- */
#/*      Transform the point into georeferenced coordinates.             */
#/* -------------------------------------------------------------------- */
    adfGeoTransform = hDataset.GetGeoTransform(can_return_null = True)
    if adfGeoTransform is not None:
        dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x \
            + adfGeoTransform[2] * y
        dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x \
            + adfGeoTransform[5] * y

    else:
        line = line + ("(%7.1f,%7.1f)" % (x, y ))
        print(line)
        return False

#/* -------------------------------------------------------------------- */
#/*      Report the georeferenced coordinates.                           */
#/* -------------------------------------------------------------------- */
    if abs(dfGeoX) < 181 and abs(dfGeoY) < 91:
        line = line + ( "(%12.7f,%12.7f) " % (dfGeoX, dfGeoY ))

    else:
        line = line + ( "(%12.3f,%12.3f) " % (dfGeoX, dfGeoY ))

#/* -------------------------------------------------------------------- */
#/*      Transform to latlong and report.                                */
#/* -------------------------------------------------------------------- */
    if hTransform is not None:
        pnt = hTransform.TransformPoint(dfGeoX, dfGeoY, 0)
        if pnt is not None:
            line = line + ( "(%s," % gdal.DecToDMS( pnt[0], "Long", 2 ) )
            line = line + ( "%s)" % gdal.DecToDMS( pnt[1], "Lat", 2 ) )

    print(line)

    return True

if __name__ == '__main__':
    version_num = int(gdal.VersionInfo('VERSION_NUM'))
    if version_num < 1800: # because of GetGeoTransform(can_return_null)
        print('ERROR: Python bindings of GDAL 1.8.0 or later required')
        sys.exit(1)

    sys.exit(main(sys.argv))
