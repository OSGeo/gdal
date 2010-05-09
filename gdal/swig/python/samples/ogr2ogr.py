#!/usr/bin/env python
#/******************************************************************************
# * $Id$
# *
# * Project:  OpenGIS Simple Features Reference Implementation
# * Purpose:  Python port of a simple client for translating between formats.
# * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# *
# * Port from ogr2ogr.cpp whose author is Frank Warmerdam
# *
# ******************************************************************************
# * Copyright (c) 2010, Even Rouault
# * Copyright (c) 1999, Frank Warmerdam
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

# Note : this is the most direct port of ogr2ogr.cpp possible
# It could be made much more Python'ish !

import sys
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

class ScaledProgressObject:
    def __init__(self, min, max, cbk, cbk_data = None):
        self.min = min
        self.max = max
        self.cbk = cbk
        self.cbk_data = cbk_data

def ScaledProgressFunc(pct, msg, data):
    if data.cbk is None:
        return True
    return data.cbk(data.min + pct * (data.max - data.min), msg, data.cbk_data)


bSkipFailures = False
nGroupTransactions = 200
bPreserveFID = False
nFIDToFetch = ogr.NullFID


def EQUAL(a, b):
    return a.lower() == b.lower()

#/************************************************************************/
#/*                                main()                                */
#/************************************************************************/

def main(args = None, progress_func = gdal.TermProgress_nocb, progress_data = None):
    
    global bSkipFailures
    global nGroupTransactions
    global bPreserveFID
    global nFIDToFetch
    
    pszFormat = "ESRI Shapefile"
    pszDataSource = None
    pszDestDataSource = None
    papszLayers = []
    papszDSCO = []
    papszLCO = []
    bTransform = False
    bAppend = False
    bUpdate = False
    bOverwrite = False
    pszOutputSRSDef = None
    pszSourceSRSDef = None
    poOutputSRS = None
    poSourceSRS = None
    pszNewLayerName = None
    pszWHERE = None
    poSpatialFilter = None
    pszSelect = None
    papszSelFields = []
    pszSQLStatement = None
    eGType = -2
    dfMaxSegmentLength = 0
    papszFieldTypesToString = []
    bDisplayProgress = False
    pfnProgress = None
    pProgressData = None
    bClipSrc = False
    poClipSrc = None
    pszClipSrcDS = None
    pszClipSrcSQL = None
    pszClipSrcLayer = None
    pszClipSrcWhere = None
    poClipDst = None
    pszClipDstDS = None
    pszClipDstSQL = None
    pszClipDstLayer = None
    pszClipDstWhere = None
    pszSrcEncoding = None
    pszDstEncoding = None

    if args is None:
        args = sys.argv

    args = ogr.GeneralCmdLineProcessor( args )

#/* -------------------------------------------------------------------- */
#/*      Processing command line arguments.                              */
#/* -------------------------------------------------------------------- */
    if args is None:
        return False

    nArgc = len(args)

    iArg = 1
    while iArg < nArgc:
        if EQUAL(args[iArg],"-f") and iArg < nArgc-1:
            iArg = iArg + 1
            pszFormat = args[iArg]

        elif EQUAL(args[iArg],"-dsco") and iArg < nArgc-1:
            iArg = iArg + 1
            papszDSCO.append(args[iArg] )

        elif EQUAL(args[iArg],"-lco") and iArg < nArgc-1:
            iArg = iArg + 1
            papszLCO.append(args[iArg] )

        elif EQUAL(args[iArg],"-preserve_fid"):
            bPreserveFID = True

        elif len(args[iArg]) >= 5 and EQUAL(args[iArg][0:5], "-skip"):
            bSkipFailures = True
            nGroupTransactions = 1 # /* #2409 */

        elif EQUAL(args[iArg],"-append"):
            bAppend = True

        elif EQUAL(args[iArg],"-overwrite"):
            bOverwrite = True

        elif EQUAL(args[iArg],"-update"):
            bUpdate = True

        elif EQUAL(args[iArg],"-fid") and iArg < nArgc-1:
            iArg = iArg + 1
            nFIDToFetch = int(args[iArg])

        elif EQUAL(args[iArg],"-sql") and iArg < nArgc-1:
            iArg = iArg + 1
            pszSQLStatement = args[iArg]

        elif EQUAL(args[iArg],"-nln") and iArg < nArgc-1:
            iArg = iArg + 1
            pszNewLayerName = args[iArg]

        elif EQUAL(args[iArg],"-nlt") and iArg < nArgc-1:

            if EQUAL(args[iArg+1],"NONE"):
                eGType = ogr.wkbNone
            elif EQUAL(args[iArg+1],"GEOMETRY"):
                eGType = ogr.wkbUnknown
            elif EQUAL(args[iArg+1],"POINT"):
                eGType = ogr.wkbPoint
            elif EQUAL(args[iArg+1],"LINESTRING"):
                eGType = ogr.wkbLineString
            elif EQUAL(args[iArg+1],"POLYGON"):
                eGType = ogr.wkbPolygon
            elif EQUAL(args[iArg+1],"GEOMETRYCOLLECTION"):
                eGType = ogr.wkbGeometryCollection
            elif EQUAL(args[iArg+1],"MULTIPOINT"):
                eGType = ogr.wkbMultiPoint
            elif EQUAL(args[iArg+1],"MULTILINESTRING"):
                eGType = ogr.wkbMultiLineString
            elif EQUAL(args[iArg+1],"MULTIPOLYGON"):
                eGType = ogr.wkbMultiPolygon
            elif EQUAL(args[iArg+1],"GEOMETRY25D"):
                eGType = ogr.wkbUnknown | ogr.wkb25DBit
            elif EQUAL(args[iArg+1],"POINT25D"):
                eGType = ogr.wkbPoint25D
            elif EQUAL(args[iArg+1],"LINESTRING25D"):
                eGType = ogr.wkbLineString25D
            elif EQUAL(args[iArg+1],"POLYGON25D"):
                eGType = ogr.wkbPolygon25D
            elif EQUAL(args[iArg+1],"GEOMETRYCOLLECTION25D"):
                eGType = ogr.wkbGeometryCollection25D
            elif EQUAL(args[iArg+1],"MULTIPOINT25D"):
                eGType = ogr.wkbMultiPoint25D
            elif EQUAL(args[iArg+1],"MULTILINESTRING25D"):
                eGType = ogr.wkbMultiLineString25D
            elif EQUAL(args[iArg+1],"MULTIPOLYGON25D"):
                eGType = ogr.wkbMultiPolygon25D
            else:
                print("-nlt %s: type not recognised." % args[iArg+1])
                return False

            iArg = iArg + 1

        elif (EQUAL(args[iArg],"-tg") or \
                EQUAL(args[iArg],"-gt")) and iArg < nArgc-1:
            iArg = iArg + 1
            nGroupTransactions = int(args[iArg])

        elif EQUAL(args[iArg],"-s_srs") and iArg < nArgc-1:
            iArg = iArg + 1
            pszSourceSRSDef = args[iArg]

        elif EQUAL(args[iArg],"-a_srs") and iArg < nArgc-1:
            iArg = iArg + 1
            pszOutputSRSDef = args[iArg]

        elif EQUAL(args[iArg],"-t_srs") and iArg < nArgc-1:
            iArg = iArg + 1
            pszOutputSRSDef = args[iArg]
            bTransform = True

        elif EQUAL(args[iArg],"-spat") and iArg + 4 < nArgc:
            oRing = ogr.Geometry(ogr.wkbLinearRing)

            oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+2]) )
            oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+4]) )
            oRing.AddPoint_2D( float(args[iArg+3]), float(args[iArg+4]) )
            oRing.AddPoint_2D( float(args[iArg+3]), float(args[iArg+2]) )
            oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+2]) )

            poSpatialFilter = ogr.Geometry(ogr.wkbPolygon)
            poSpatialFilter.AddGeometry(oRing)
            iArg = iArg + 4

        elif EQUAL(args[iArg],"-where") and iArg < nArgc-1:
            iArg = iArg + 1
            pszWHERE = args[++iArg]

        elif EQUAL(args[iArg],"-select") and iArg < nArgc-1:
            iArg = iArg + 1
            pszSelect = args[iArg]
            if pszSelect.find(',') != -1:
                papszSelFields = pszSelect.split(',')
            else:
                papszSelFields = pszSelect.split(' ')

        elif EQUAL(args[iArg],"-segmentize") and iArg < nArgc-1:
            iArg = iArg + 1
            dfMaxSegmentLength = float(args[iArg])

        elif EQUAL(args[iArg],"-fieldTypeToString") and iArg < nArgc-1:
            iArg = iArg + 1
            pszFieldTypeToString = args[iArg]
            if pszFieldTypeToString.find(',') != -1:
                tokens = pszFieldTypeToString.split(',')
            else:
                tokens = pszFieldTypeToString.split(' ')

            for token in tokens:
                if EQUAL(token,"Integer") or \
                    EQUAL(token,"Real") or \
                    EQUAL(token,"String") or \
                    EQUAL(token,"Date") or \
                    EQUAL(token,"Time") or \
                    EQUAL(token,"DateTime") or \
                    EQUAL(token,"Binary") or \
                    EQUAL(token,"IntegerList") or \
                    EQUAL(token,"RealList") or \
                    EQUAL(token,"StringList"):

                    papszFieldTypesToString.append(token)

                elif EQUAL(token,"All"):
                    papszFieldTypesToString = [ 'All' ]
                    break

                else:
                    print("Unhandled type for fieldtypeasstring option : %s " % token)
                    return Usage()

        elif EQUAL(args[iArg],"-progress"):
            bDisplayProgress = True

        #/*elif EQUAL(args[iArg],"-wrapdateline") )
        #{
        #    bWrapDateline = True;
        #}
        #*/
        elif EQUAL(args[iArg],"-clipsrc") and iArg < nArgc-1:

            bClipSrc = True
            if IsNumber(args[iArg+1]) and iArg < nArgc - 4:
                oRing = ogr.Geometry(ogr.wkbLinearRing)

                oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+2]) )
                oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+4]) )
                oRing.AddPoint_2D( float(args[iArg+3]), float(args[iArg+4]) )
                oRing.AddPoint_2D( float(args[iArg+3]), float(args[iArg+2]) )
                oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+2]) )

                poClipSrc = ogr.Geometry(ogr.wkbPolygon)
                poClipSrc.AddGeometry(oRing)
                iArg = iArg + 4

            elif (len(args[iArg+1]) >= 7 and EQUAL(args[iArg+1][0:7],"POLYGON") ) or \
                  (len(args[iArg+1]) >= 12 and EQUAL(args[iArg+1][0:12],"MULTIPOLYGON") ) :
                poClipSrc = ogr.CreateGeometryFromWkt(args[iArg+1])
                if poClipSrc is None:
                    print("FAILURE: Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT\n")
                    return Usage()

                iArg = iArg + 1

            elif EQUAL(args[iArg+1],"spat_extent"):
                iArg = iArg + 1

            else:
                pszClipSrcDS = args[iArg+1]
                iArg = iArg + 1

        elif EQUAL(args[iArg],"-clipsrcsql") and iArg < nArgc-1:
            pszClipSrcSQL = args[iArg+1]
            iArg = iArg + 1

        elif EQUAL(args[iArg],"-clipsrclayer") and iArg < nArgc-1:
            pszClipSrcLayer = args[iArg+1]
            iArg = iArg + 1

        elif EQUAL(args[iArg],"-clipsrcwhere") and iArg < nArgc-1:
            pszClipSrcWhere = args[iArg+1]
            iArg = iArg + 1

        elif EQUAL(args[iArg],"-clipdst") and iArg < nArgc-1:

            if IsNumber(args[iArg+1]) and iArg < nArgc - 4:
                oRing = ogr.Geometry(ogr.wkbLinearRing)

                oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+2]) )
                oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+4]) )
                oRing.AddPoint_2D( float(args[iArg+3]), float(args[iArg+4]) )
                oRing.AddPoint_2D( float(args[iArg+3]), float(args[iArg+2]) )
                oRing.AddPoint_2D( float(args[iArg+1]), float(args[iArg+2]) )

                poClipDst = ogr.Geometry(ogr.wkbPolygon)
                poClipDst.AddGeometry(oRing)
                iArg = iArg + 4

            elif (len(args[iArg+1]) >= 7 and EQUAL(args[iArg+1][0:7],"POLYGON") ) or \
                  (len(args[iArg+1]) >= 12 and EQUAL(args[iArg+1][0:12],"MULTIPOLYGON") ) :
                poClipDst = ogr.CreateGeometryFromWkt(args[iArg+1])
                if poClipDst is None:
                    print("FAILURE: Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT\n")
                    return Usage()

                iArg = iArg + 1

            elif EQUAL(args[iArg+1],"spat_extent"):
                iArg = iArg + 1

            else:
                pszClipDstDS = args[iArg+1]
                iArg = iArg + 1

        elif EQUAL(args[iArg],"-clipdstsql") and iArg < nArgc-1:
            pszClipDstSQL = args[iArg+1]
            iArg = iArg + 1

        elif EQUAL(args[iArg],"-clipdstlayer") and iArg < nArgc-1:
            pszClipDstLayer = args[iArg+1]
            iArg = iArg + 1

        elif EQUAL(args[iArg],"-clipdstwhere") and iArg < nArgc-1:
            pszClipDstWhere = args[iArg+1]
            iArg = iArg + 1
        elif args[iArg][0] == '-':
            return Usage()

        elif pszDestDataSource is None:
            pszDestDataSource = args[iArg]
        elif pszDataSource is None:
            pszDataSource = args[iArg]
        else:
            papszLayers.append (args[iArg] )

        iArg = iArg + 1

    if pszDataSource is None:
        return Usage()

    if bClipSrc and pszClipSrcDS is not None:
        poClipSrc = LoadGeometry(pszClipSrcDS, pszClipSrcSQL, pszClipSrcLayer, pszClipSrcWhere)
        if poClipSrc is None:
            print("FAILURE: cannot load source clip geometry\n" )
            return Usage()

    elif bClipSrc and poClipSrc is None:
        if poSpatialFilter is not None:
            poClipSrc = poSpatialFilter.Clone()
        if poClipSrc is None:
            print("FAILURE: -clipsrc must be used with -spat option or a\n" + \
                  "bounding box, WKT string or datasource must be specified\n")
            return Usage()

    if pszClipDstDS is not None:
        poClipDst = LoadGeometry(pszClipDstDS, pszClipDstSQL, pszClipDstLayer, pszClipDstWhere)
        if poClipDst is None:
            print("FAILURE: cannot load dest clip geometry\n" )
            return Usage()

#/* -------------------------------------------------------------------- */
#/*      Open data source.                                               */
#/* -------------------------------------------------------------------- */
    poDS = ogr.Open( pszDataSource, False )

#/* -------------------------------------------------------------------- */
#/*      Report failure                                                  */
#/* -------------------------------------------------------------------- */
    if poDS is None:
        print("FAILURE:\n" + \
                "Unable to open datasource `%s' with the following drivers." % pszDataSource)

        for iDriver in range(ogr.GetDriverCount()):
            print("  ->  " + ogr.GetDriver(iDriver).GetName() )

        return False

#/* -------------------------------------------------------------------- */
#/*      Try opening the output datasource as an existing, writable      */
#/* -------------------------------------------------------------------- */
    poODS = None

    if bUpdate:
        poODS = ogr.Open( pszDestDataSource, True )
        if poODS is None:
            print("FAILURE:\n" +
                    "Unable to open existing output datasource `%s'." % pszDestDataSource)
            return False

        if len(papszDSCO) > 0:
            print("WARNING: Datasource creation options ignored since an existing datasource\n" + \
                    "         being updated." )

#/* -------------------------------------------------------------------- */
#/*      Find the output driver.                                         */
#/* -------------------------------------------------------------------- */
    else:
        poDriver = None

        for iDriver in range(ogr.GetDriverCount()):
            if EQUAL(ogr.GetDriver(iDriver).GetName(),pszFormat):
                poDriver = ogr.GetDriver(iDriver)
                break

        if poDriver is None:
            print("Unable to find driver `%s'." % pszFormat)
            print( "The following drivers are available:" )

            for iDriver in range(ogr.GetDriverCount()):
                print("  ->  %s" % ogr.GetDriver(iDriver).GetName() )

            return False

        if poDriver.TestCapability( ogr.ODrCCreateDataSource ) == False:
            print( "%s driver does not support data source creation." % pszFormat)
            return False

#/* -------------------------------------------------------------------- */
#/*      Create the output data source.                                  */
#/* -------------------------------------------------------------------- */
        poODS = poDriver.CreateDataSource( pszDestDataSource, options = papszDSCO )
        if poODS is None:
            print( "%s driver failed to create %s" % (pszFormat, pszDestDataSource ))
            return False

#/* -------------------------------------------------------------------- */
#/*      Parse the output SRS definition if possible.                    */
#/* -------------------------------------------------------------------- */
    if pszOutputSRSDef is not None:
        poOutputSRS = osr.SpatialReference()
        if poOutputSRS.SetFromUserInput( pszOutputSRSDef ) != 0:
            print( "Failed to process SRS definition: %s" % pszOutputSRSDef )
            return False

#/* -------------------------------------------------------------------- */
#/*      Parse the source SRS definition if possible.                    */
#/* -------------------------------------------------------------------- */
    if pszSourceSRSDef is not None:
        poSourceSRS = osr.SpatialReference()
        if poSourceSRS.SetFromUserInput( pszSourceSRSDef ) != 0:
            print( "Failed to process SRS definition: %s" % pszSourceSRSDef )
            return False

#/* -------------------------------------------------------------------- */
#/*      Special case for -sql clause.  No source layers required.       */
#/* -------------------------------------------------------------------- */
    if pszSQLStatement is not None:
        if pszWHERE is not None:
            print( "-where clause ignored in combination with -sql." )
        if len(papszLayers) > 0:
            print( "layer names ignored in combination with -sql." )

        poResultSet = poDS.ExecuteSQL( pszSQLStatement, poSpatialFilter, \
                                        None )

        if poResultSet is not None:
            nCountLayerFeatures = 0
            if bDisplayProgress:
                if not poResultSet.TestCapability(ogr.OLCFastFeatureCount):
                    print( "Progress turned off as fast feature count is not available.")
                    bDisplayProgress = False

                else:
                    nCountLayerFeatures = poResultSet.GetFeatureCount()
                    pfnProgress = progress_func
                    pProgressData = progress_data

            if not TranslateLayer( poDS, poResultSet, poODS, papszLCO, \
                                pszNewLayerName, bTransform, poOutputSRS, \
                                poSourceSRS, papszSelFields, bAppend, eGType, \
                                bOverwrite, dfMaxSegmentLength, papszFieldTypesToString, \
                                nCountLayerFeatures, poClipSrc, poClipDst, pfnProgress, pProgressData ):
                print(
                        "Terminating translation prematurely after failed\n" + \
                        "translation from sql statement." )

                return False

            poDS.ReleaseResultSet( poResultSet )

    else:

        nLayerCount = 0
        papoLayers = []

#/* -------------------------------------------------------------------- */
#/*      Process each data source layer.                                 */
#/* -------------------------------------------------------------------- */
        if len(papszLayers) == 0:
            nLayerCount = poDS.GetLayerCount()
            papoLayers = [None for i in range(nLayerCount)]
            iLayer = 0

            for iLayer in range(nLayerCount):
                poLayer = poDS.GetLayer(iLayer)

                if poLayer is None:
                    print("FAILURE: Couldn't fetch advertised layer %d!" % iLayer)
                    return False

                papoLayers[iLayer] = poLayer
                iLayer = iLayer + 1

#/* -------------------------------------------------------------------- */
#/*      Process specified data source layers.                           */
#/* -------------------------------------------------------------------- */
        else:
            nLayerCount = len(papszLayers)
            papoLayers = [None for i in range(nLayerCount)]
            iLayer = 0

            for layername in papszLayers:
                poLayer = poDS.GetLayerByName(layername)

                if poLayer is None:
                    print("FAILURE: Couldn't fetch advertised layer %s!" % layername)
                    return False

                papoLayers[iLayer] = poLayer
                iLayer = iLayer + 1

        panLayerCountFeatures = [0 for i in range(nLayerCount)]
        nCountLayersFeatures = 0
        nAccCountFeatures = 0

        #/* First pass to apply filters and count all features if necessary */
        for iLayer in range(nLayerCount):
            poLayer = papoLayers[iLayer]

            if pszWHERE is not None:
                poLayer.SetAttributeFilter( pszWHERE )

            if poSpatialFilter is not None:
                poLayer.SetSpatialFilter( poSpatialFilter )

            if bDisplayProgress:
                if not poLayer.TestCapability(ogr.OLCFastFeatureCount):
                    print("Progress turned off as fast feature count is not available.")
                    bDisplayProgress = False
                else:
                    panLayerCountFeatures[iLayer] = poLayer.GetFeatureCount()
                    nCountLayersFeatures += panLayerCountFeatures[iLayer]

        #/* Second pass to do the real job */
        for iLayer in range(nLayerCount):
            poLayer = papoLayers[iLayer]

            if bDisplayProgress:
                pfnProgress = ScaledProgressFunc
                pProgressData = ScaledProgressObject( \
                        nAccCountFeatures * 1.0 / nCountLayersFeatures, \
                        (nAccCountFeatures + panLayerCountFeatures[iLayer]) * 1.0 / nCountLayersFeatures, \
                        progress_func, progress_data)

            nAccCountFeatures += panLayerCountFeatures[iLayer]

            if not TranslateLayer( poDS, poLayer, poODS, papszLCO,  \
                                pszNewLayerName, bTransform, poOutputSRS, \
                                poSourceSRS, papszSelFields, bAppend, eGType, \
                                bOverwrite, dfMaxSegmentLength, papszFieldTypesToString, \
                                panLayerCountFeatures[iLayer], poClipSrc, poClipDst, pfnProgress, pProgressData)  \
                and not bSkipFailures:
                print(
                        "Terminating translation prematurely after failed\n" + \
                        "translation of layer " + poLayer.GetLayerDefn().GetName() + " (use -skipfailures to skip errors)")

                return False

#/* -------------------------------------------------------------------- */
#/*      Close down.                                                     */
#/* -------------------------------------------------------------------- */
    #/* We must explicetely destroy the output dataset in order the file */
    #/* to be properly closed ! */
    poODS.Destroy()
    poDS.Destroy()

    return True

#/************************************************************************/
#/*                               Usage()                                */
#/************************************************************************/

def Usage():

    print( "Usage: ogr2ogr [--help-general] [-skipfailures] [-append] [-update] [-gt n]\n" + \
            "               [-select field_list] [-where restricted_where] \n" + \
            "               [-progress] [-sql <sql statement>] \n" + \
            "               [-spat xmin ymin xmax ymax] [-preserve_fid] [-fid FID]\n" + \
            "               [-a_srs srs_def] [-t_srs srs_def] [-s_srs srs_def]\n" + \
            "               [-f format_name] [-overwrite] [[-dsco NAME=VALUE] ...]\n" + \
            #// "               [-segmentize max_dist] [-fieldTypeToString All|(type1[,type2]*)]\n" + \
            "               [-fieldTypeToString All|(type1[,type2]*)]\n" + \
            "               dst_datasource_name src_datasource_name\n" + \
            "               [-lco NAME=VALUE] [-nln name] [-nlt type] [layer [layer ...]]\n" + \
            "\n" + \
            " -f format_name: output file format name, possible values are:")

    for iDriver in range(ogr.GetDriverCount()):
        poDriver = ogr.GetDriver(iDriver)

        if poDriver.TestCapability( ogr.ODrCCreateDataSource ):
            print( "     -f \"" + poDriver.GetName() + "\"" )

    print( " -append: Append to existing layer instead of creating new if it exists\n" + \
            " -overwrite: delete the output layer and recreate it empty\n" + \
            " -update: Open existing output datasource in update mode\n" + \
            " -progress: Display progress on terminal. Only works if input layers have the \"fast feature count\" capability\n" + \
            " -select field_list: Comma-delimited list of fields from input layer to\n" + \
            "                     copy to the new layer (defaults to all)\n" + \
            " -where restricted_where: Attribute query (like SQL WHERE)\n" + \
            " -sql statement: Execute given SQL statement and save result.\n" + \
            " -skipfailures: skip features or layers that fail to convert\n" + \
            " -gt n: group n features per transaction (default 200)\n" + \
            " -spat xmin ymin xmax ymax: spatial query extents\n" + \
            #//" -segmentize max_dist: maximum distance between 2 nodes.\n" + \
            #//"                       Used to create intermediate points\n" + \
            " -dsco NAME=VALUE: Dataset creation option (format specific)\n" + \
            " -lco  NAME=VALUE: Layer creation option (format specific)\n" + \
            " -nln name: Assign an alternate name to the new layer\n" + \
            " -nlt type: Force a geometry type for new layer.  One of NONE, GEOMETRY,\n" + \
            "      POINT, LINESTRING, POLYGON, GEOMETRYCOLLECTION, MULTIPOINT,\n" + \
            "      MULTIPOLYGON, or MULTILINESTRING.  Add \"25D\" for 3D layers.\n" + \
            "      Default is type of source layer.\n" + \
            " -fieldTypeToString type1,...: Converts fields of specified types to\n" + \
            "      fields of type string in the new layer. Valid types are : \n" + \
            "      Integer, Real, String, Date, Time, DateTime, Binary, IntegerList, RealList,\n" + \
            "      StringList. Special value All can be used to convert all fields to strings.")

    print(" -a_srs srs_def: Assign an output SRS\n" + \
        " -t_srs srs_def: Reproject/transform to this SRS on output\n" + \
        " -s_srs srs_def: Override source SRS\n" + \
        "\n" + \
        " Srs_def can be a full WKT definition (hard to escape properly),\n" + \
        " or a well known definition (ie. EPSG:4326) or a file with a WKT\n" + \
        " definition." )

    return False

def CSLFindString(v, mystr):
    i = 0
    for strIter in v:
        if EQUAL(strIter, mystr):
            return i
        i = i + 1
    return -1

def IsNumber( pszStr):
    try:
        (float)(pszStr)
        return True
    except:
        return False

def LoadGeometry( pszDS, pszSQL, pszLyr, pszWhere):
    poGeom = None

    poDS = ogr.Open( pszDS, False )
    if poDS is None:
        return None

    if pszSQL is not None:
        poLyr = poDS.ExecuteSQL( pszSQL, None, None )
    elif pszLyr is not None:
        poLyr = poDS.GetLayerByName(pszLyr)
    else:
        poLyr = poDS.GetLayer(0)

    if poLyr is None:
        print("Failed to identify source layer from datasource.")
        poDS.Destroy()
        return None

    if pszWhere is not None:
        poLyr.SetAttributeFilter(pszWhere)

    poFeat = poLyr.GetNextFeature()
    while poFeat is not None:
        poSrcGeom = poFeat.GetGeometryRef()
        if poSrcGeom is not None:
            eType = poSrcGeom.GetGeometryType() & (~ogrConstants.wkb25DBit)

            if poGeom is None:
                poGeom = ogr.Geometry( ogr.wkbMultiPolygon )

            if eType == ogr.wkbPolygon:
                poGeom.AddGeometry( poSrcGeom )
            elif eType == ogr.wkbMultiPolygon:
                for iGeom in range(poSrcGeom.GetGeometryCount()):
                    poGeom.AddGeometry(poSrcGeom.GetGeometryRef(iGeom) )

            else:
                print("ERROR: Geometry not of polygon type." )
                if pszSQL is not None:
                    poDS.ReleaseResultSet( poLyr )
                poDS.Destroy()
                return None

        poFeat = poLyr.GetNextFeature()

    if pszSQL is not None:
        poDS.ReleaseResultSet( poLyr )
    poDS.Destroy()

    return poGeom

#/************************************************************************/
#/*                           TranslateLayer()                           */
#/************************************************************************/

def TranslateLayer( poSrcDS, poSrcLayer, poDstDS, papszLCO, pszNewLayerName, \
                    bTransform,  poOutputSRS, poSourceSRS, papszSelFields, \
                    bAppend, eGType, bOverwrite, dfMaxSegmentLength, \
                    papszFieldTypesToString, nCountLayerFeatures, \
                    poClipSrc, poClipDst, pfnProgress, pProgressData) :

    bForceToPolygon = False
    bForceToMultiPolygon = False

    if pszNewLayerName is None:
        pszNewLayerName = poSrcLayer.GetLayerDefn().GetName()

    if (eGType & (~ogr.wkb25DBit)) == ogr.wkbPolygon:
        bForceToPolygon = True
    elif (eGType & (~ogr.wkb25DBit)) == ogr.wkbMultiPolygon:
        bForceToMultiPolygon = True

#/* -------------------------------------------------------------------- */
#/*      Setup coordinate transformation if we need it.                  */
#/* -------------------------------------------------------------------- */
    poCT = None

    if bTransform:
        if poSourceSRS is None:
            poSourceSRS = poSrcLayer.GetSpatialRef()

        if poSourceSRS is None:
            print("Can't transform coordinates, source layer has no\n" + \
                    "coordinate system.  Use -s_srs to set one." )
            return False

        poCT = osr.CoordinateTransformation( poSourceSRS, poOutputSRS )
        if poCT is None:
            pszWKT = None

            print("Failed to create coordinate transformation between the\n" + \
                "following coordinate systems.  This may be because they\n" + \
                "are not transformable, or because projection services\n" + \
                "(PROJ.4 DLL/.so) could not be loaded." )

            pszWKT = poSourceSRS.ExportToPrettyWkt( 0 )
            print( "Source:\n" + pszWKT )

            pszWKT = poOutputSRS.ExportToPrettyWkt( 0 )
            print( "Target:\n" + pszWKT )
            return False

#/* -------------------------------------------------------------------- */
#/*      Get other info.                                                 */
#/* -------------------------------------------------------------------- */
    poFDefn = poSrcLayer.GetLayerDefn()

    if poOutputSRS is None:
        poOutputSRS = poSrcLayer.GetSpatialRef()

#/* -------------------------------------------------------------------- */
#/*      Find the layer.                                                 */
#/* -------------------------------------------------------------------- */
    iLayer = -1
    poDstLayer = None

    for iLayer in range(poDstDS.GetLayerCount()):
        poLayer = poDstDS.GetLayer(iLayer)

        if poLayer is not None \
            and EQUAL(poLayer.GetLayerDefn().GetName(), pszNewLayerName):
            poDstLayer = poLayer
            break

#/* -------------------------------------------------------------------- */
#/*      If the user requested overwrite, and we have the layer in       */
#/*      question we need to delete it now so it will get recreated      */
#/*      (overwritten).                                                  */
#/* -------------------------------------------------------------------- */
    if poDstLayer is not None and bOverwrite:
        if poDstDS.DeleteLayer( iLayer ) != 0:
            print("DeleteLayer() failed when overwrite requested." )
            return False

        poDstLayer = None

#/* -------------------------------------------------------------------- */
#/*      If the layer does not exist, then create it.                    */
#/* -------------------------------------------------------------------- */
    if poDstLayer is None:
        if eGType == -2:
            eGType = poFDefn.GetGeomType()

        if poDstDS.TestCapability( ogr.ODsCCreateLayer ) == False:
            print("Layer " + pszNewLayerName + "not found, and CreateLayer not supported by driver.")
            return False

        gdal.ErrorReset()

        poDstLayer = poDstDS.CreateLayer( pszNewLayerName, poOutputSRS, \
                                            eGType, papszLCO )

        if poDstLayer is None:
            return False

        bAppend = False

#/* -------------------------------------------------------------------- */
#/*      Otherwise we will append to it, if append was requested.        */
#/* -------------------------------------------------------------------- */
    elif not bAppend:
        print("FAILED: Layer " + pszNewLayerName + "already exists, and -append not specified.\n" + \
                            "        Consider using -append, or -overwrite.")
        return False
    else:
        if len(papszLCO) > 0:
            print("WARNING: Layer creation options ignored since an existing layer is\n" + \
                    "         being appended to." )

#/* -------------------------------------------------------------------- */
#/*      Add fields.  Default to copy all field.                         */
#/*      If only a subset of all fields requested, then output only      */
#/*      the selected fields, and in the order that they were            */
#/*      selected.                                                       */
#/* -------------------------------------------------------------------- */

    if len(papszSelFields) > 0 and not bAppend:

        for iField in range(len(papszSelFields)):

            iSrcField = poFDefn.GetFieldIndex(papszSelFields[iField])
            if iSrcField >= 0:
                if papszFieldTypesToString is not None and \
                    (CSLFindString(papszFieldTypesToString, "All") != -1 or \
                    CSLFindString(papszFieldTypesToString, \
                                ogr.GetFieldTypeName(poFDefn.GetFieldDefn(iSrcField).GetFieldType())) != -1):

                    oFieldDefn = ogr.FieldDefn( poFDefn.GetFieldDefn(iSrcField).GetName() )
                    oFieldDefn.SetType(ogr.OFTString)
                    poDstLayer.CreateField( oFieldDefn )

                else:
                    poDstLayer.CreateField( poFDefn.GetFieldDefn(iSrcField) )

            else:
                print("Field '" + papszSelFields[iField] + "' not found in source layer.")
                if not bSkipFailures:
                    return False

    elif not bAppend:

        for iField in range(poFDefn.GetFieldCount()):

            if papszFieldTypesToString is not None and \
                (CSLFindString(papszFieldTypesToString, "All") != -1 or \
                CSLFindString(papszFieldTypesToString, \
                            ogr.GetFieldTypeName(poFDefn.GetFieldDefn(iField).GetType())) != -1):

                oFieldDefn = ogr.FieldDefn( poFDefn.GetFieldDefn(iField).GetName() )
                oFieldDefn.SetType(ogr.OFTString)
                poDstLayer.CreateField( oFieldDefn )

            else:
                poDstLayer.CreateField( poFDefn.GetFieldDefn(iField) )

#/* -------------------------------------------------------------------- */
#/*      Transfer features.                                              */
#/* -------------------------------------------------------------------- */
    nFeaturesInTransaction = 0
    nCount = 0

    poSrcLayer.ResetReading()

    if nGroupTransactions > 0:
        poDstLayer.StartTransaction()

    while True:
        poDstFeature = None

        if nFIDToFetch != ogr.NullFID:

            #// Only fetch feature on first pass.
            if nFeaturesInTransaction == 0:
                poFeature = poSrcLayer.GetFeature(nFIDToFetch)
            else:
                poFeature = None

        else:
            poFeature = poSrcLayer.GetNextFeature()

        if poFeature is None:
            break

        nFeaturesInTransaction = nFeaturesInTransaction + 1
        if nFeaturesInTransaction == nGroupTransactions:
            poDstLayer.CommitTransaction()
            poDstLayer.StartTransaction()
            nFeaturesInTransaction = 0

        gdal.ErrorReset()
        poDstFeature = ogr.Feature( poDstLayer.GetLayerDefn() )

        if poDstFeature.SetFrom( poFeature, 1 ) != 0:

            if nGroupTransactions > 0:
                poDstLayer.CommitTransaction()

            print("Unable to translate feature %d from layer %s" % (poFeature.GetFID() , poFDefn.GetName() ))

            poFeature.Destroy()
            poFeature = None
            poDstFeature.Destroy()
            poDstFeature = None
            return False

        if bPreserveFID:
            poDstFeature.SetFID( poFeature.GetFID() )

        #/*if (poDstFeature.GetGeometryRef() is not None and dfMaxSegmentLength > 0)
        #    poDstFeature.GetGeometryRef().segmentize(dfMaxSegmentLength);*/

        poDstGeometry = poDstFeature.GetGeometryRef()
        if poDstGeometry is not None:
            if poClipSrc is not None:
                poClipped = poDstGeometry.Intersection(poClipSrc)
                if poClipped is None or poClipped.IsEmpty():
                    #/* Report progress */
                    nCount = nCount +1
                    if pfnProgress is not None:
                        pfnProgress(nCount * 1.0 / nCountLayerFeatures, "", pProgressData)
                    poFeature.Destroy()
                    poDstFeature.Destroy()
                    continue

                poDstFeature.SetGeometryDirectly(poClipped)
                poDstGeometry = poClipped

            if poCT is not None:
                eErr = poDstGeometry.Transform( poCT )
                if eErr != 0:
                    if nGroupTransactions > 0:
                        poDstLayer.CommitTransaction()

                    print("Failed to reproject feature %d (geometry probably out of source or destination SRS)." % poFeature.GetFID())
                    if not bSkipFailures:
                        poFeature.Destroy()
                        poFeature = None
                        poDstFeature.Destroy()
                        poDstFeature = None
                        return False

            elif poOutputSRS is not None:
                poDstGeometry.AssignSpatialReference(poOutputSRS)

            if poClipDst is not None:
                poClipped = poDstGeometry.Intersection(poClipDst)
                if poClipped is None or poClipped.IsEmpty():
                    #/* Report progress */
                    nCount = nCount +1
                    if pfnProgress is not None:
                        pfnProgress(nCount * 1.0 / nCountLayerFeatures, "", pProgressData)
                    poFeature.Destroy()
                    poDstFeature.Destroy()
                    continue

                poDstFeature.SetGeometryDirectly(poClipped)
                poDstGeometry = poClipped

            if bForceToPolygon:
                poDstFeature.SetGeometryDirectly(ogr.ForceToPolygon(poDstGeometry))

            if bForceToMultiPolygon:
                poDstFeature.SetGeometryDirectly(ogr.ForceToMultiPolygon(poDstGeometry))

        poFeature.Destroy()
        poFeature = None

        gdal.ErrorReset()
        if poDstLayer.CreateFeature( poDstFeature ) != 0 and not bSkipFailures:
            if nGroupTransactions > 0:
                poDstLayer.RollbackTransaction()

            poDstFeature.Destroy()
            poDstFeature = None
            return False

        poDstFeature.Destroy()
        poDstFeature = None

        #/* Report progress */
        nCount = nCount  + 1
        if pfnProgress is not None:
            pfnProgress(nCount * 1.0 / nCountLayerFeatures, "", pProgressData)

    if nGroupTransactions > 0:
        poDstLayer.CommitTransaction()

    return True

if __name__ == '__main__':
    version_num = int(gdal.VersionInfo('VERSION_NUM'))
    if version_num < 1800: # because of ogr.GetFieldTypeName
        print('ERROR: Python bindings of GDAL 1.8.0 or later required')
        sys.exit(1)

    if not main(sys.argv):
        sys.exit(1)
    else:
        sys.exit(0)

