#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# *****************************************************************************
# $Id$
#
# Project:  OpenGIS Simple Features Reference Implementation
# Purpose:  Python port of a simple client for translating between formats.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
# Port from ogr2ogr.cpp whose author is Frank Warmerdam
#
# *****************************************************************************
# Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 1999, Frank Warmerdam
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
# **************************************************************************

# Note : this is the most direct port of ogr2ogr.cpp possible
# It could be made much more Python'ish !

import os
import stat
import sys

from osgeo import gdal, ogr, osr

###############################################################################


class ScaledProgressObject(object):
    def __init__(self, mini, maxi, cbk, cbk_data=None):
        self.min = mini
        self.max = maxi
        self.cbk = cbk
        self.cbk_data = cbk_data


###############################################################################


def ScaledProgressFunc(pct, msg, data):
    if data.cbk is None:
        return True
    return data.cbk(data.min + pct * (data.max - data.min), msg, data.cbk_data)


###############################################################################


def EQUAL(a, b):
    return a.lower() == b.lower()


###############################################################################
# Redefinition of GDALTermProgress, so that test_ogr2ogr_py.py
# can check that the progress bar is displayed


nLastTick = -1


def TermProgress(dfComplete, pszMessage, pProgressArg):
    # pylint: disable=unused-argument

    global nLastTick
    nThisTick = int(dfComplete * 40.0)

    if nThisTick < 0:
        nThisTick = 0
    if nThisTick > 40:
        nThisTick = 40

    # Have we started a new progress run?
    if nThisTick < nLastTick and nLastTick >= 39:
        nLastTick = -1

    if nThisTick <= nLastTick:
        return True

    while nThisTick > nLastTick:
        nLastTick = nLastTick + 1
        if (nLastTick % 4) == 0:
            sys.stdout.write("%d" % ((nLastTick / 4) * 10))
        else:
            sys.stdout.write(".")

    if nThisTick == 40:
        print(" - done.")
    else:
        sys.stdout.flush()

    return True


class TargetLayerInfo(object):
    def __init__(self):
        self.poDstLayer = None
        self.poCT = None
        # self.papszTransformOptions = None
        self.panMap = None
        self.iSrcZField = None


class AssociatedLayers(object):
    def __init__(self):
        self.poSrcLayer = None
        self.psInfo = None


# **********************************************************************
#                                main()
# **********************************************************************


bSkipFailures = False
nGroupTransactions = 200
bPreserveFID = False
nFIDToFetch = ogr.NullFID


class Enum(set):
    def __getattr__(self, name):
        if name in self:
            return name
        raise AttributeError


GeomOperation = Enum(["NONE", "SEGMENTIZE", "SIMPLIFY_PRESERVE_TOPOLOGY"])


def main(args=None, progress_func=TermProgress, progress_data=None):

    global bSkipFailures
    global nGroupTransactions
    global bPreserveFID
    global nFIDToFetch

    version_num = int(gdal.VersionInfo("VERSION_NUM"))
    if version_num < 1800:  # because of ogr.GetFieldTypeName
        print("ERROR: Python bindings of GDAL 1.8.0 or later required")
        return 1

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
    bNullifyOutputSRS = False
    poSourceSRS = None
    pszNewLayerName = None
    pszWHERE = None
    poSpatialFilter = None
    pszSelect = None
    papszSelFields = None
    pszSQLStatement = None
    eGType = -2
    bPromoteToMulti = False
    eGeomOp = GeomOperation.NONE
    dfGeomOpParam = 0
    papszFieldTypesToString = []
    bDisplayProgress = False
    pfnProgress = None
    pProgressArg = None
    bClipSrc = False
    bWrapDateline = False
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
    # pszSrcEncoding = None
    # pszDstEncoding = None
    bWrapDateline = False
    bExplodeCollections = False
    pszZField = None
    nCoordDim = -1

    if args is None:
        args = sys.argv

    args = ogr.GeneralCmdLineProcessor(args)

    # --------------------------------------------------------------------
    #      Processing command line arguments.
    # --------------------------------------------------------------------
    if args is None:
        return 2

    nArgc = len(args)

    iArg = 1
    while iArg < nArgc:
        if EQUAL(args[iArg], "-f") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszFormat = args[iArg]

        elif EQUAL(args[iArg], "-dsco") and iArg < nArgc - 1:
            iArg = iArg + 1
            papszDSCO.append(args[iArg])

        elif EQUAL(args[iArg], "-lco") and iArg < nArgc - 1:
            iArg = iArg + 1
            papszLCO.append(args[iArg])

        elif EQUAL(args[iArg], "-preserve_fid"):
            bPreserveFID = True

        elif len(args[iArg]) >= 5 and EQUAL(args[iArg][0:5], "-skip"):
            bSkipFailures = True
            nGroupTransactions = 1  # 2409

        elif EQUAL(args[iArg], "-append"):
            bAppend = True
            bUpdate = True

        elif EQUAL(args[iArg], "-overwrite"):
            bOverwrite = True
            bUpdate = True

        elif EQUAL(args[iArg], "-update"):
            bUpdate = True

        elif EQUAL(args[iArg], "-fid") and iArg < nArgc - 1:
            iArg = iArg + 1
            nFIDToFetch = int(args[iArg])

        elif EQUAL(args[iArg], "-sql") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszSQLStatement = args[iArg]

        elif EQUAL(args[iArg], "-nln") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszNewLayerName = args[iArg]

        elif EQUAL(args[iArg], "-nlt") and iArg < nArgc - 1:

            if EQUAL(args[iArg + 1], "NONE"):
                eGType = ogr.wkbNone
            elif EQUAL(args[iArg + 1], "GEOMETRY"):
                eGType = ogr.wkbUnknown
            elif EQUAL(args[iArg + 1], "PROMOTE_TO_MULTI"):
                bPromoteToMulti = True
            elif EQUAL(args[iArg + 1], "POINT"):
                eGType = ogr.wkbPoint
            elif EQUAL(args[iArg + 1], "LINESTRING"):
                eGType = ogr.wkbLineString
            elif EQUAL(args[iArg + 1], "POLYGON"):
                eGType = ogr.wkbPolygon
            elif EQUAL(args[iArg + 1], "GEOMETRYCOLLECTION"):
                eGType = ogr.wkbGeometryCollection
            elif EQUAL(args[iArg + 1], "MULTIPOINT"):
                eGType = ogr.wkbMultiPoint
            elif EQUAL(args[iArg + 1], "MULTILINESTRING"):
                eGType = ogr.wkbMultiLineString
            elif EQUAL(args[iArg + 1], "MULTIPOLYGON"):
                eGType = ogr.wkbMultiPolygon
            elif EQUAL(args[iArg + 1], "GEOMETRY25D"):
                eGType = ogr.wkbUnknown | ogr.wkb25DBit
            elif EQUAL(args[iArg + 1], "POINT25D"):
                eGType = ogr.wkbPoint25D
            elif EQUAL(args[iArg + 1], "LINESTRING25D"):
                eGType = ogr.wkbLineString25D
            elif EQUAL(args[iArg + 1], "POLYGON25D"):
                eGType = ogr.wkbPolygon25D
            elif EQUAL(args[iArg + 1], "GEOMETRYCOLLECTION25D"):
                eGType = ogr.wkbGeometryCollection25D
            elif EQUAL(args[iArg + 1], "MULTIPOINT25D"):
                eGType = ogr.wkbMultiPoint25D
            elif EQUAL(args[iArg + 1], "MULTILINESTRING25D"):
                eGType = ogr.wkbMultiLineString25D
            elif EQUAL(args[iArg + 1], "MULTIPOLYGON25D"):
                eGType = ogr.wkbMultiPolygon25D
            else:
                print("-nlt %s: type not recognised." % args[iArg + 1])
                return 1

            iArg = iArg + 1

        elif EQUAL(args[iArg], "-dim") and iArg < nArgc - 1:

            nCoordDim = int(args[iArg + 1])
            if nCoordDim != 2 and nCoordDim != 3:
                print("-dim %s: value not handled." % args[iArg + 1])
                return 1
            iArg = iArg + 1

        elif (
            EQUAL(args[iArg], "-tg") or EQUAL(args[iArg], "-gt")
        ) and iArg < nArgc - 1:
            iArg = iArg + 1
            nGroupTransactions = int(args[iArg])

        elif EQUAL(args[iArg], "-s_srs") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszSourceSRSDef = args[iArg]

        elif EQUAL(args[iArg], "-a_srs") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszOutputSRSDef = args[iArg]
            if EQUAL(pszOutputSRSDef, "NULL") or EQUAL(pszOutputSRSDef, "NONE"):
                pszOutputSRSDef = None
                bNullifyOutputSRS = True

        elif EQUAL(args[iArg], "-t_srs") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszOutputSRSDef = args[iArg]
            bTransform = True

        elif EQUAL(args[iArg], "-spat") and iArg + 4 < nArgc:
            oRing = ogr.Geometry(ogr.wkbLinearRing)

            oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 2]))
            oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 4]))
            oRing.AddPoint_2D(float(args[iArg + 3]), float(args[iArg + 4]))
            oRing.AddPoint_2D(float(args[iArg + 3]), float(args[iArg + 2]))
            oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 2]))

            poSpatialFilter = ogr.Geometry(ogr.wkbPolygon)
            poSpatialFilter.AddGeometry(oRing)
            iArg = iArg + 4

        elif EQUAL(args[iArg], "-where") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszWHERE = args[iArg]

        elif EQUAL(args[iArg], "-select") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszSelect = args[iArg]
            if pszSelect.find(",") != -1:
                papszSelFields = pszSelect.split(",")
            else:
                papszSelFields = pszSelect.split(" ")
            if papszSelFields[0] == "":
                papszSelFields = []

        elif EQUAL(args[iArg], "-simplify") and iArg < nArgc - 1:
            iArg = iArg + 1
            eGeomOp = GeomOperation.SIMPLIFY_PRESERVE_TOPOLOGY
            dfGeomOpParam = float(args[iArg])

        elif EQUAL(args[iArg], "-segmentize") and iArg < nArgc - 1:
            iArg = iArg + 1
            eGeomOp = GeomOperation.SEGMENTIZE
            dfGeomOpParam = float(args[iArg])

        elif EQUAL(args[iArg], "-fieldTypeToString") and iArg < nArgc - 1:
            iArg = iArg + 1
            pszFieldTypeToString = args[iArg]
            if pszFieldTypeToString.find(",") != -1:
                tokens = pszFieldTypeToString.split(",")
            else:
                tokens = pszFieldTypeToString.split(" ")

            for token in tokens:
                if (
                    EQUAL(token, "Integer")
                    or EQUAL(token, "Real")
                    or EQUAL(token, "String")
                    or EQUAL(token, "Date")
                    or EQUAL(token, "Time")
                    or EQUAL(token, "DateTime")
                    or EQUAL(token, "Binary")
                    or EQUAL(token, "IntegerList")
                    or EQUAL(token, "RealList")
                    or EQUAL(token, "StringList")
                ):

                    papszFieldTypesToString.append(token)

                elif EQUAL(token, "All"):
                    papszFieldTypesToString = ["All"]
                    break

                else:
                    print("Unhandled type for fieldtypeasstring option : %s " % token)
                    return Usage()

        elif EQUAL(args[iArg], "-progress"):
            bDisplayProgress = True

        # elif EQUAL(args[iArg],"-wrapdateline") )
        # {
        #    bWrapDateline = True;
        # }
        #
        elif EQUAL(args[iArg], "-clipsrc") and iArg < nArgc - 1:

            bClipSrc = True
            if IsNumber(args[iArg + 1]) and iArg < nArgc - 4:
                oRing = ogr.Geometry(ogr.wkbLinearRing)

                oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 2]))
                oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 4]))
                oRing.AddPoint_2D(float(args[iArg + 3]), float(args[iArg + 4]))
                oRing.AddPoint_2D(float(args[iArg + 3]), float(args[iArg + 2]))
                oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 2]))

                poClipSrc = ogr.Geometry(ogr.wkbPolygon)
                poClipSrc.AddGeometry(oRing)
                iArg = iArg + 4

            elif (
                len(args[iArg + 1]) >= 7 and EQUAL(args[iArg + 1][0:7], "POLYGON")
            ) or (
                len(args[iArg + 1]) >= 12
                and EQUAL(args[iArg + 1][0:12], "MULTIPOLYGON")
            ):
                poClipSrc = ogr.CreateGeometryFromWkt(args[iArg + 1])
                if poClipSrc is None:
                    print(
                        "FAILURE: Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT\n"
                    )
                    return Usage()

                iArg = iArg + 1

            elif EQUAL(args[iArg + 1], "spat_extent"):
                iArg = iArg + 1

            else:
                pszClipSrcDS = args[iArg + 1]
                iArg = iArg + 1

        elif EQUAL(args[iArg], "-clipsrcsql") and iArg < nArgc - 1:
            pszClipSrcSQL = args[iArg + 1]
            iArg = iArg + 1

        elif EQUAL(args[iArg], "-clipsrclayer") and iArg < nArgc - 1:
            pszClipSrcLayer = args[iArg + 1]
            iArg = iArg + 1

        elif EQUAL(args[iArg], "-clipsrcwhere") and iArg < nArgc - 1:
            pszClipSrcWhere = args[iArg + 1]
            iArg = iArg + 1

        elif EQUAL(args[iArg], "-clipdst") and iArg < nArgc - 1:

            if IsNumber(args[iArg + 1]) and iArg < nArgc - 4:
                oRing = ogr.Geometry(ogr.wkbLinearRing)

                oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 2]))
                oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 4]))
                oRing.AddPoint_2D(float(args[iArg + 3]), float(args[iArg + 4]))
                oRing.AddPoint_2D(float(args[iArg + 3]), float(args[iArg + 2]))
                oRing.AddPoint_2D(float(args[iArg + 1]), float(args[iArg + 2]))

                poClipDst = ogr.Geometry(ogr.wkbPolygon)
                poClipDst.AddGeometry(oRing)
                iArg = iArg + 4

            elif (
                len(args[iArg + 1]) >= 7 and EQUAL(args[iArg + 1][0:7], "POLYGON")
            ) or (
                len(args[iArg + 1]) >= 12
                and EQUAL(args[iArg + 1][0:12], "MULTIPOLYGON")
            ):
                poClipDst = ogr.CreateGeometryFromWkt(args[iArg + 1])
                if poClipDst is None:
                    print(
                        "FAILURE: Invalid geometry. Must be a valid POLYGON or MULTIPOLYGON WKT\n"
                    )
                    return Usage()

                iArg = iArg + 1

            elif EQUAL(args[iArg + 1], "spat_extent"):
                iArg = iArg + 1

            else:
                pszClipDstDS = args[iArg + 1]
                iArg = iArg + 1

        elif EQUAL(args[iArg], "-clipdstsql") and iArg < nArgc - 1:
            pszClipDstSQL = args[iArg + 1]
            iArg = iArg + 1

        elif EQUAL(args[iArg], "-clipdstlayer") and iArg < nArgc - 1:
            pszClipDstLayer = args[iArg + 1]
            iArg = iArg + 1

        elif EQUAL(args[iArg], "-clipdstwhere") and iArg < nArgc - 1:
            pszClipDstWhere = args[iArg + 1]
            iArg = iArg + 1

        elif EQUAL(args[iArg], "-explodecollections"):
            bExplodeCollections = True

        elif EQUAL(args[iArg], "-zfield") and iArg < nArgc - 1:
            pszZField = args[iArg + 1]
            iArg = iArg + 1

        elif args[iArg][0] == "-":
            return Usage()

        elif pszDestDataSource is None:
            pszDestDataSource = args[iArg]
        elif pszDataSource is None:
            pszDataSource = args[iArg]
        else:
            papszLayers.append(args[iArg])

        iArg = iArg + 1

    if pszDataSource is None:
        return Usage()

    if bPreserveFID and bExplodeCollections:
        print(
            "FAILURE: cannot use -preserve_fid and -explodecollections at the same time\n\n"
        )
        return Usage()

    if bClipSrc and pszClipSrcDS is not None:
        poClipSrc = LoadGeometry(
            pszClipSrcDS, pszClipSrcSQL, pszClipSrcLayer, pszClipSrcWhere
        )
        if poClipSrc is None:
            print("FAILURE: cannot load source clip geometry\n")
            return Usage()

    elif bClipSrc and poClipSrc is None:
        if poSpatialFilter is not None:
            poClipSrc = poSpatialFilter.Clone()
        if poClipSrc is None:
            print(
                "FAILURE: -clipsrc must be used with -spat option or a\n"
                + "bounding box, WKT string or datasource must be specified\n"
            )
            return Usage()

    if pszClipDstDS is not None:
        poClipDst = LoadGeometry(
            pszClipDstDS, pszClipDstSQL, pszClipDstLayer, pszClipDstWhere
        )
        if poClipDst is None:
            print("FAILURE: cannot load dest clip geometry\n")
            return Usage()

    # --------------------------------------------------------------------
    #      Open data source.
    # --------------------------------------------------------------------
    poDS = ogr.Open(pszDataSource, False)

    # --------------------------------------------------------------------
    #      Report failure
    # --------------------------------------------------------------------
    if poDS is None:
        print(
            "FAILURE:\n"
            + "Unable to open datasource `%s' with the following drivers."
            % pszDataSource
        )

        for iDriver in range(ogr.GetDriverCount()):
            print("  ->  " + ogr.GetDriver(iDriver).GetName())

        return 1

    # --------------------------------------------------------------------
    #      Try opening the output datasource as an existing, writable
    # --------------------------------------------------------------------
    poODS = None
    poDriver = None

    if bUpdate:
        poODS = ogr.Open(pszDestDataSource, True)
        if poODS is None:

            if bOverwrite or bAppend:
                poODS = ogr.Open(pszDestDataSource, False)
                if poODS is None:
                    # the datasource doesn't exist at all
                    bUpdate = False
                else:
                    poODS.delete()
                    poODS = None

            if bUpdate:
                print(
                    "FAILURE:\n"
                    + "Unable to open existing output datasource `%s'."
                    % pszDestDataSource
                )
                return 1

        elif papszDSCO:
            print(
                "WARNING: Datasource creation options ignored since an existing datasource\n"
                + "         being updated."
            )

        if poODS is not None:
            poDriver = poODS.GetDriver()

    # --------------------------------------------------------------------
    #      Find the output driver.
    # --------------------------------------------------------------------
    if not bUpdate:
        poDriver = ogr.GetDriverByName(pszFormat)
        if poDriver is None:
            print("Unable to find driver `%s'." % pszFormat)
            print("The following drivers are available:")

            for iDriver in range(ogr.GetDriverCount()):
                print("  ->  %s" % ogr.GetDriver(iDriver).GetName())

            return 1

        if not poDriver.TestCapability(ogr.ODrCCreateDataSource):
            print("%s driver does not support data source creation." % pszFormat)
            return 1

        # --------------------------------------------------------------------
        #      Special case to improve user experience when translating
        #      a datasource with multiple layers into a shapefile. If the
        #      user gives a target datasource with .shp and it does not exist,
        #      the shapefile driver will try to create a file, but this is not
        #      appropriate because here we have several layers, so create
        #      a directory instead.
        # --------------------------------------------------------------------
        if (
            EQUAL(poDriver.GetName(), "ESRI Shapefile")
            and pszSQLStatement is None
            and (len(papszLayers) > 1 or (not papszLayers and poDS.GetLayerCount() > 1))
            and pszNewLayerName is None
            and EQUAL(os.path.splitext(pszDestDataSource)[1], ".SHP")
        ):

            try:
                os.stat(pszDestDataSource)
            except OSError:
                try:
                    # decimal 493 = octal 0755. Python 3 needs 0o755, but
                    # this syntax is only supported by Python >= 2.6
                    os.mkdir(pszDestDataSource, 493)
                except OSError:
                    print(
                        "Failed to create directory %s\n"
                        "for shapefile datastore.\n" % pszDestDataSource
                    )
                    return 1

        # --------------------------------------------------------------------
        #      Create the output data source.
        # --------------------------------------------------------------------
        poODS = poDriver.CreateDataSource(pszDestDataSource, options=papszDSCO)
        if poODS is None:
            print("%s driver failed to create %s" % (pszFormat, pszDestDataSource))
            return 1

    # --------------------------------------------------------------------
    #      Parse the output SRS definition if possible.
    # --------------------------------------------------------------------
    if pszOutputSRSDef is not None:
        poOutputSRS = osr.SpatialReference()
        poOutputSRS.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
        if poOutputSRS.SetFromUserInput(pszOutputSRSDef) != 0:
            print("Failed to process SRS definition: %s" % pszOutputSRSDef)
            return 1

    # --------------------------------------------------------------------
    #      Parse the source SRS definition if possible.
    # --------------------------------------------------------------------
    if pszSourceSRSDef is not None:
        poSourceSRS = osr.SpatialReference()
        poSourceSRS.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
        if poSourceSRS.SetFromUserInput(pszSourceSRSDef) != 0:
            print("Failed to process SRS definition: %s" % pszSourceSRSDef)
            return 1

    # --------------------------------------------------------------------
    #      For OSM file.
    # --------------------------------------------------------------------
    bSrcIsOSM = poDS.GetDriver() is not None and poDS.GetDriver().GetName() == "OSM"
    nSrcFileSize = 0
    if bSrcIsOSM and poDS.GetName() != "/vsistdin/":
        sStat = gdal.VSIStatL(poDS.GetName())
        if sStat is not None:
            nSrcFileSize = sStat.size

    # --------------------------------------------------------------------
    #      Special case for -sql clause.  No source layers required.
    # --------------------------------------------------------------------
    if pszSQLStatement is not None:
        if pszWHERE is not None:
            print("-where clause ignored in combination with -sql.")
        if papszLayers:
            print("layer names ignored in combination with -sql.")

        poResultSet = poDS.ExecuteSQL(pszSQLStatement, poSpatialFilter, None)

        if poResultSet is not None:
            nCountLayerFeatures = 0
            if bDisplayProgress:
                if bSrcIsOSM:
                    pfnProgress = progress_func
                    pProgressArg = progress_data

                elif not poResultSet.TestCapability(ogr.OLCFastFeatureCount):
                    print("Progress turned off as fast feature count is not available.")
                    bDisplayProgress = False

                else:
                    nCountLayerFeatures = poResultSet.GetFeatureCount()
                    pfnProgress = progress_func
                    pProgressArg = progress_data

            # --------------------------------------------------------------------
            #      Special case to improve user experience when translating into
            #      single file shapefile and source has only one layer, and that
            #      the layer name isn't specified
            # --------------------------------------------------------------------
            if EQUAL(poDriver.GetName(), "ESRI Shapefile") and pszNewLayerName is None:
                try:
                    mode = os.stat(pszDestDataSource).st_mode
                    if (mode & stat.S_IFDIR) == 0:
                        pszNewLayerName = os.path.splitext(
                            os.path.basename(pszDestDataSource)
                        )[0]
                except OSError:
                    pass

            psInfo = SetupTargetLayer(
                poDS,
                poResultSet,
                poODS,
                papszLCO,
                pszNewLayerName,
                bTransform,
                poOutputSRS,
                bNullifyOutputSRS,
                poSourceSRS,
                papszSelFields,
                bAppend,
                eGType,
                bPromoteToMulti,
                nCoordDim,
                bOverwrite,
                papszFieldTypesToString,
                bWrapDateline,
                bExplodeCollections,
                pszZField,
                pszWHERE,
            )

            poResultSet.ResetReading()

            if psInfo is None or not TranslateLayer(
                psInfo,
                poDS,
                poResultSet,
                poODS,
                poOutputSRS,
                bNullifyOutputSRS,
                eGType,
                bPromoteToMulti,
                nCoordDim,
                eGeomOp,
                dfGeomOpParam,
                nCountLayerFeatures,
                poClipSrc,
                poClipDst,
                bExplodeCollections,
                nSrcFileSize,
                None,
                pfnProgress,
                pProgressArg,
            ):
                print(
                    "Terminating translation prematurely after failed\n"
                    + "translation from sql statement."
                )

                return 1

            poDS.ReleaseResultSet(poResultSet)

    # --------------------------------------------------------------------
    #      Special case for layer interleaving mode.
    # --------------------------------------------------------------------
    elif bSrcIsOSM and gdal.GetConfigOption("OGR_INTERLEAVED_READING", None) is None:

        gdal.SetConfigOption("OGR_INTERLEAVED_READING", "YES")

        # if (bSplitListFields)
        # {
        #    fprintf( stderr, "FAILURE: -splitlistfields not supported in this mode\n" );
        #    exit( 1 );
        # }

        nSrcLayerCount = poDS.GetLayerCount()
        pasAssocLayers = [AssociatedLayers() for _ in range(nSrcLayerCount)]

        # --------------------------------------------------------------------
        #      Special case to improve user experience when translating into
        #      single file shapefile and source has only one layer, and that
        #      the layer name isn't specified
        # --------------------------------------------------------------------

        if (
            EQUAL(poDriver.GetName(), "ESRI Shapefile")
            and (len(papszLayers) == 1 or nSrcLayerCount == 1)
            and pszNewLayerName is None
        ):
            try:
                mode = os.stat(pszDestDataSource).st_mode
                if (mode & stat.S_IFDIR) == 0:
                    pszNewLayerName = os.path.splitext(
                        os.path.basename(pszDestDataSource)
                    )[0]
            except OSError:
                pass

        if bDisplayProgress and bSrcIsOSM:
            pfnProgress = progress_func
            pProgressArg = progress_data

        # --------------------------------------------------------------------
        #      If no target layer specified, use all source layers.
        # --------------------------------------------------------------------
        if not papszLayers:
            papszLayers = [None] * nSrcLayerCount
            for iLayer in range(nSrcLayerCount):
                poLayer = poDS.GetLayer(iLayer)
                if poLayer is None:
                    print("FAILURE: Couldn't fetch advertised layer %d!" % iLayer)
                    return 1

                papszLayers[iLayer] = poLayer.GetName()
        else:
            if bSrcIsOSM:
                osInterestLayers = "SET interest_layers ="
                for iLayer, papszLayer in enumerate(papszLayers):
                    if iLayer != 0:
                        osInterestLayers = osInterestLayers + ","
                    osInterestLayers = osInterestLayers + papszLayer

                poDS.ExecuteSQL(osInterestLayers, None, None)

        # --------------------------------------------------------------------
        #      First pass to set filters and create target layers.
        # --------------------------------------------------------------------
        for iLayer in range(nSrcLayerCount):
            poLayer = poDS.GetLayer(iLayer)
            if poLayer is None:
                print("FAILURE: Couldn't fetch advertised layer %d!" % iLayer)
                return 1

            pasAssocLayers[iLayer].poSrcLayer = poLayer

            if CSLFindString(papszLayers, poLayer.GetName()) >= 0:
                if pszWHERE is not None:
                    if poLayer.SetAttributeFilter(pszWHERE) != 0:
                        print(
                            "FAILURE: SetAttributeFilter(%s) on layer '%s' failed.\n"
                            % (pszWHERE, poLayer.GetName())
                        )
                        if not bSkipFailures:
                            return 1

                if poSpatialFilter is not None:
                    poLayer.SetSpatialFilter(poSpatialFilter)

                psInfo = SetupTargetLayer(
                    poDS,
                    poLayer,
                    poODS,
                    papszLCO,
                    pszNewLayerName,
                    bTransform,
                    poOutputSRS,
                    bNullifyOutputSRS,
                    poSourceSRS,
                    papszSelFields,
                    bAppend,
                    eGType,
                    bPromoteToMulti,
                    nCoordDim,
                    bOverwrite,
                    papszFieldTypesToString,
                    bWrapDateline,
                    bExplodeCollections,
                    pszZField,
                    pszWHERE,
                )

                if psInfo is None and not bSkipFailures:
                    return 1

                pasAssocLayers[iLayer].psInfo = psInfo
            else:
                pasAssocLayers[iLayer].psInfo = None

        # --------------------------------------------------------------------
        #      Second pass to process features in a interleaved layer mode.
        # --------------------------------------------------------------------
        bHasLayersNonEmpty = True
        while bHasLayersNonEmpty:
            bHasLayersNonEmpty = False

            for iLayer in range(nSrcLayerCount):
                poLayer = pasAssocLayers[iLayer].poSrcLayer
                psInfo = pasAssocLayers[iLayer].psInfo
                anReadFeatureCount = [0]

                if psInfo is not None:
                    if (
                        not TranslateLayer(
                            psInfo,
                            poDS,
                            poLayer,
                            poODS,
                            poOutputSRS,
                            bNullifyOutputSRS,
                            eGType,
                            bPromoteToMulti,
                            nCoordDim,
                            eGeomOp,
                            dfGeomOpParam,
                            0,
                            poClipSrc,
                            poClipDst,
                            bExplodeCollections,
                            nSrcFileSize,
                            anReadFeatureCount,
                            pfnProgress,
                            pProgressArg,
                        )
                        and not bSkipFailures
                    ):
                        print(
                            "Terminating translation prematurely after failed\n"
                            + "translation of layer "
                            + poLayer.GetName()
                            + " (use -skipfailures to skip errors)"
                        )

                        return 1
                else:
                    # No matching target layer : just consumes the features

                    poFeature = poLayer.GetNextFeature()
                    while poFeature is not None:
                        anReadFeatureCount[0] = anReadFeatureCount[0] + 1
                        poFeature = poLayer.GetNextFeature()

                if anReadFeatureCount[0] != 0:
                    bHasLayersNonEmpty = True

    else:

        nLayerCount = 0
        papoLayers = []

        # --------------------------------------------------------------------
        #      Process each data source layer.
        # --------------------------------------------------------------------
        if not papszLayers:
            nLayerCount = poDS.GetLayerCount()
            papoLayers = [None] * nLayerCount
            iLayer = 0

            for iLayer in range(nLayerCount):
                poLayer = poDS.GetLayer(iLayer)

                if poLayer is None:
                    print("FAILURE: Couldn't fetch advertised layer %d!" % iLayer)
                    return 1

                papoLayers[iLayer] = poLayer
                iLayer = iLayer + 1

        # --------------------------------------------------------------------
        #      Process specified data source layers.
        # --------------------------------------------------------------------
        else:
            nLayerCount = len(papszLayers)
            papoLayers = [None] * nLayerCount
            iLayer = 0

            for layername in papszLayers:
                poLayer = poDS.GetLayerByName(layername)

                if poLayer is None:
                    print("FAILURE: Couldn't fetch advertised layer %s!" % layername)
                    return 1

                papoLayers[iLayer] = poLayer
                iLayer = iLayer + 1

        panLayerCountFeatures = [0] * nLayerCount
        nCountLayersFeatures = 0
        nAccCountFeatures = 0

        # First pass to apply filters and count all features if necessary
        for iLayer in range(nLayerCount):
            poLayer = papoLayers[iLayer]

            if pszWHERE is not None:
                if poLayer.SetAttributeFilter(pszWHERE) != 0:
                    print("FAILURE: SetAttributeFilter(%s) failed." % pszWHERE)
                    if not bSkipFailures:
                        return 1

            if poSpatialFilter is not None:
                poLayer.SetSpatialFilter(poSpatialFilter)

            if bDisplayProgress and not bSrcIsOSM:
                if not poLayer.TestCapability(ogr.OLCFastFeatureCount):
                    print("Progress turned off as fast feature count is not available.")
                    bDisplayProgress = False
                else:
                    panLayerCountFeatures[iLayer] = poLayer.GetFeatureCount()
                    nCountLayersFeatures += panLayerCountFeatures[iLayer]

        # Second pass to do the real job
        for iLayer in range(nLayerCount):
            poLayer = papoLayers[iLayer]

            if bDisplayProgress:
                if bSrcIsOSM:
                    pfnProgress = progress_func
                    pProgressArg = progress_data
                else:
                    pfnProgress = ScaledProgressFunc
                    pProgressArg = ScaledProgressObject(
                        nAccCountFeatures * 1.0 / nCountLayersFeatures,
                        (nAccCountFeatures + panLayerCountFeatures[iLayer])
                        * 1.0
                        / nCountLayersFeatures,
                        progress_func,
                        progress_data,
                    )

            nAccCountFeatures += panLayerCountFeatures[iLayer]

            # --------------------------------------------------------------------
            #      Special case to improve user experience when translating into
            #      single file shapefile and source has only one layer, and that
            #      the layer name isn't specified
            # --------------------------------------------------------------------
            if (
                EQUAL(poDriver.GetName(), "ESRI Shapefile")
                and nLayerCount == 1
                and pszNewLayerName is None
            ):
                try:
                    mode = os.stat(pszDestDataSource).st_mode
                    if (mode & stat.S_IFDIR) == 0:
                        pszNewLayerName = os.path.splitext(
                            os.path.basename(pszDestDataSource)
                        )[0]
                except OSError:
                    pass

            psInfo = SetupTargetLayer(
                poDS,
                poLayer,
                poODS,
                papszLCO,
                pszNewLayerName,
                bTransform,
                poOutputSRS,
                bNullifyOutputSRS,
                poSourceSRS,
                papszSelFields,
                bAppend,
                eGType,
                bPromoteToMulti,
                nCoordDim,
                bOverwrite,
                papszFieldTypesToString,
                bWrapDateline,
                bExplodeCollections,
                pszZField,
                pszWHERE,
            )

            poLayer.ResetReading()

            if (
                psInfo is None
                or not TranslateLayer(
                    psInfo,
                    poDS,
                    poLayer,
                    poODS,
                    poOutputSRS,
                    bNullifyOutputSRS,
                    eGType,
                    bPromoteToMulti,
                    nCoordDim,
                    eGeomOp,
                    dfGeomOpParam,
                    panLayerCountFeatures[iLayer],
                    poClipSrc,
                    poClipDst,
                    bExplodeCollections,
                    nSrcFileSize,
                    None,
                    pfnProgress,
                    pProgressArg,
                )
            ) and not bSkipFailures:
                print(
                    "Terminating translation prematurely after failed\n"
                    + "translation of layer "
                    + poLayer.GetLayerDefn().GetName()
                    + " (use -skipfailures to skip errors)"
                )

                return 1

    # --------------------------------------------------------------------
    #      Close down.
    # --------------------------------------------------------------------
    # We must explicitly destroy the output dataset in order the file
    # to be properly closed !
    poODS.Destroy()
    poDS.Destroy()

    return 0


# **********************************************************************
#                               Usage()
# **********************************************************************


def Usage():

    print(
        "Usage: ogr2ogr [--help-general] [-skipfailures] [-append] [-update] [-gt n]\n"
        + "               [-select field_list] [-where restricted_where] \n"
        + "               [-progress] [-sql <sql statement>] \n"
        + "               [-spat xmin ymin xmax ymax] [-preserve_fid] [-fid FID]\n"
        + "               [-a_srs srs_def] [-t_srs srs_def] [-s_srs srs_def]\n"
        + "               [-f format_name] [-overwrite] [[-dsco NAME=VALUE] ...]\n"
        + "               [-simplify tolerance]\n"
        + "               [-fieldTypeToString All|(type1[,type2]*)] [-explodecollections] \n"  # // "               [-segmentize max_dist] [-fieldTypeToString All|(type1[,type2]*)]\n" + \
        + "               dst_datasource_name src_datasource_name\n"
        + "               [-lco NAME=VALUE] [-nln name] [-nlt type] [-dim 2|3] [layer [layer ...]]\n"
        + "\n"
        + " -f format_name: output file format name, possible values are:"
    )

    for iDriver in range(ogr.GetDriverCount()):
        poDriver = ogr.GetDriver(iDriver)

        if poDriver.TestCapability(ogr.ODrCCreateDataSource):
            print('     -f "' + poDriver.GetName() + '"')

    print(
        " -append: Append to existing layer instead of creating new if it exists\n"
        + " -overwrite: delete the output layer and recreate it empty\n"
        + " -update: Open existing output datasource in update mode\n"
        + ' -progress: Display progress on terminal. Only works if input layers have the "fast feature count" capability\n'
        + " -select field_list: Comma-delimited list of fields from input layer to\n"
        + "                     copy to the new layer (defaults to all)\n"
        + " -where restricted_where: Attribute query (like SQL WHERE)\n"
        + " -sql statement: Execute given SQL statement and save result.\n"
        + " -skipfailures: skip features or layers that fail to convert\n"
        + " -gt n: group n features per transaction (default 200)\n"
        + " -spat xmin ymin xmax ymax: spatial query extents\n"
        + " -simplify tolerance: distance tolerance for simplification.\n"
        +  # //" -segmentize max_dist: maximum distance between 2 nodes.\n" + \
        # //"                       Used to create intermediate points\n" + \
        " -dsco NAME=VALUE: Dataset creation option (format specific)\n"
        + " -lco  NAME=VALUE: Layer creation option (format specific)\n"
        + " -nln name: Assign an alternate name to the new layer\n"
        + " -nlt type: Force a geometry type for new layer.  One of NONE, GEOMETRY,\n"
        + "      POINT, LINESTRING, POLYGON, GEOMETRYCOLLECTION, MULTIPOINT,\n"
        + '      MULTIPOLYGON, or MULTILINESTRING.  Add "25D" for 3D layers.\n'
        + "      Default is type of source layer.\n"
        + " -dim dimension: Force the coordinate dimension to the specified value.\n"
        + " -fieldTypeToString type1,...: Converts fields of specified types to\n"
        + "      fields of type string in the new layer. Valid types are : \n"
        + "      Integer, Real, String, Date, Time, DateTime, Binary, IntegerList, RealList,\n"
        + "      StringList. Special value All can be used to convert all fields to strings."
    )

    print(
        " -a_srs srs_def: Assign an output SRS\n"
        " -t_srs srs_def: Reproject/transform to this SRS on output\n"
        " -s_srs srs_def: Override source SRS\n"
        "\n"
        " Srs_def can be a full WKT definition (hard to escape properly),\n"
        " or a well known definition (i.e. EPSG:4326) or a file with a WKT\n"
        " definition."
    )

    return 2


def CSLFindString(v, mystr):
    i = 0
    for strIter in v:
        if EQUAL(strIter, mystr):
            return i
        i = i + 1
    return -1


def IsNumber(pszStr):
    try:
        float(pszStr)
        return True
    except ValueError:
        return False


def LoadGeometry(pszDS, pszSQL, pszLyr, pszWhere):
    poGeom = None

    poDS = ogr.Open(pszDS, False)
    if poDS is None:
        return None

    if pszSQL is not None:
        poLyr = poDS.ExecuteSQL(pszSQL, None, None)
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
            eType = wkbFlatten(poSrcGeom.GetGeometryType())

            if poGeom is None:
                poGeom = ogr.Geometry(ogr.wkbMultiPolygon)

            if eType == ogr.wkbPolygon:
                poGeom.AddGeometry(poSrcGeom)
            elif eType == ogr.wkbMultiPolygon:
                for iGeom in range(poSrcGeom.GetGeometryCount()):
                    poGeom.AddGeometry(poSrcGeom.GetGeometryRef(iGeom))

            else:
                print("ERROR: Geometry not of polygon type.")
                if pszSQL is not None:
                    poDS.ReleaseResultSet(poLyr)
                poDS.Destroy()
                return None

        poFeat = poLyr.GetNextFeature()

    if pszSQL is not None:
        poDS.ReleaseResultSet(poLyr)
    poDS.Destroy()

    return poGeom


def wkbFlatten(x):
    return x & (~ogr.wkb25DBit)


# **********************************************************************
#                               SetZ()
# **********************************************************************


def SetZ(poGeom, dfZ):

    if poGeom is None:
        return

    eGType = wkbFlatten(poGeom.GetGeometryType())
    if eGType == ogr.wkbPoint:
        poGeom.SetPoint(0, poGeom.GetX(), poGeom.GetY(), dfZ)

    elif eGType == ogr.wkbLineString or eGType == ogr.wkbLinearRing:
        for i in range(poGeom.GetPointCount()):
            poGeom.SetPoint(i, poGeom.GetX(i), poGeom.GetY(i), dfZ)

    elif (
        eGType == ogr.wkbPolygon
        or eGType == ogr.wkbMultiPoint
        or eGType == ogr.wkbMultiLineString
        or eGType == ogr.wkbMultiPolygon
        or eGType == ogr.wkbGeometryCollection
    ):
        for i in range(poGeom.GetGeometryCount()):
            SetZ(poGeom.GetGeometryRef(i), dfZ)


# **********************************************************************
#                         SetupTargetLayer()
# **********************************************************************


def SetupTargetLayer(
    poSrcDS,
    poSrcLayer,
    poDstDS,
    papszLCO,
    pszNewLayerName,
    bTransform,
    poOutputSRS,
    bNullifyOutputSRS,
    poSourceSRS,
    papszSelFields,
    bAppend,
    eGType,
    bPromoteToMulti,
    nCoordDim,
    bOverwrite,
    papszFieldTypesToString,
    bWrapDateline,
    bExplodeCollections,
    pszZField,
    pszWHERE,
):
    # pylint: disable=unused-argument
    if pszNewLayerName is None:
        pszNewLayerName = poSrcLayer.GetLayerDefn().GetName()

    # --------------------------------------------------------------------
    #      Setup coordinate transformation if we need it.
    # --------------------------------------------------------------------
    poCT = None

    if bTransform:
        if poSourceSRS is None:
            poSourceSRS = poSrcLayer.GetSpatialRef()

        if poSourceSRS is None:
            print(
                "Can't transform coordinates, source layer has no\n"
                + "coordinate system.  Use -s_srs to set one."
            )
            return None

        poCT = osr.CoordinateTransformation(poSourceSRS, poOutputSRS)
        if poCT is None:
            pszWKT = None

            print(
                "Failed to create coordinate transformation between the\n"
                + "following coordinate systems.  This may be because they\n"
                + "are not transformable."
            )

            pszWKT = poSourceSRS.ExportToPrettyWkt(0)
            print("Source:\n" + pszWKT)

            pszWKT = poOutputSRS.ExportToPrettyWkt(0)
            print("Target:\n" + pszWKT)
            return None

    # --------------------------------------------------------------------
    #      Get other info.
    # --------------------------------------------------------------------
    poSrcFDefn = poSrcLayer.GetLayerDefn()

    if poOutputSRS is None and not bNullifyOutputSRS:
        poOutputSRS = poSrcLayer.GetSpatialRef()

    # --------------------------------------------------------------------
    #      Find the layer.
    # --------------------------------------------------------------------

    # GetLayerByName() can instantiate layers that would have been
    # 'hidden' otherwise, for example, non-spatial tables in a
    # PostGIS-enabled database, so this apparently useless command is
    # not useless. (#4012)
    gdal.PushErrorHandler("CPLQuietErrorHandler")
    poDstLayer = poDstDS.GetLayerByName(pszNewLayerName)
    gdal.PopErrorHandler()
    gdal.ErrorReset()

    iLayer = -1
    if poDstLayer is not None:
        nLayerCount = poDstDS.GetLayerCount()
        for iLayer in range(nLayerCount):
            poLayer = poDstDS.GetLayer(iLayer)
            # The .cpp version compares on pointers directly, but we cannot
            # do this with swig object, so just compare the names.
            if poLayer is not None and poLayer.GetName() == poDstLayer.GetName():
                break

        if iLayer == nLayerCount:
            # Shouldn't happen with an ideal driver
            poDstLayer = None

    # --------------------------------------------------------------------
    #      If the user requested overwrite, and we have the layer in
    #      question we need to delete it now so it will get recreated
    #      (overwritten).
    # --------------------------------------------------------------------
    if poDstLayer is not None and bOverwrite:
        if poDstDS.DeleteLayer(iLayer) != 0:
            print("DeleteLayer() failed when overwrite requested.")
            return None

        poDstLayer = None

    # --------------------------------------------------------------------
    #      If the layer does not exist, then create it.
    # --------------------------------------------------------------------
    if poDstLayer is None:
        if eGType == -2:
            eGType = poSrcFDefn.GetGeomType()

            n25DBit = eGType & ogr.wkb25DBit
            if bPromoteToMulti:
                if wkbFlatten(eGType) == ogr.wkbLineString:
                    eGType = ogr.wkbMultiLineString | n25DBit
                elif wkbFlatten(eGType) == ogr.wkbPolygon:
                    eGType = ogr.wkbMultiPolygon | n25DBit

            if bExplodeCollections:
                if wkbFlatten(eGType) == ogr.wkbMultiPoint:
                    eGType = ogr.wkbPoint | n25DBit
                elif wkbFlatten(eGType) == ogr.wkbMultiLineString:
                    eGType = ogr.wkbLineString | n25DBit
                elif wkbFlatten(eGType) == ogr.wkbMultiPolygon:
                    eGType = ogr.wkbPolygon | n25DBit
                elif wkbFlatten(eGType) == ogr.wkbGeometryCollection:
                    eGType = ogr.wkbUnknown | n25DBit

            if pszZField is not None:
                eGType = eGType | ogr.wkb25DBit

        if nCoordDim == 2:
            eGType = eGType & ~ogr.wkb25DBit
        elif nCoordDim == 3:
            eGType = eGType | ogr.wkb25DBit

        if not poDstDS.TestCapability(ogr.ODsCCreateLayer):
            print(
                "Layer "
                + pszNewLayerName
                + "not found, and CreateLayer not supported by driver."
            )
            return None

        gdal.ErrorReset()

        poDstLayer = poDstDS.CreateLayer(pszNewLayerName, poOutputSRS, eGType, papszLCO)

        if poDstLayer is None:
            return None

        bAppend = False

    # --------------------------------------------------------------------
    #      Otherwise we will append to it, if append was requested.
    # --------------------------------------------------------------------
    elif not bAppend:
        print(
            "FAILED: Layer "
            + pszNewLayerName
            + "already exists, and -append not specified.\n"
            + "        Consider using -append, or -overwrite."
        )
        return None
    else:
        if papszLCO:
            print(
                "WARNING: Layer creation options ignored since an existing layer is\n"
                + "         being appended to."
            )

    # --------------------------------------------------------------------
    #      Add fields.  Default to copy all field.
    #      If only a subset of all fields requested, then output only
    #      the selected fields, and in the order that they were
    #      selected.
    # --------------------------------------------------------------------

    # Initialize the index-to-index map to -1's
    nSrcFieldCount = poSrcFDefn.GetFieldCount()
    panMap = [-1] * nSrcFieldCount

    poDstFDefn = poDstLayer.GetLayerDefn()

    if papszSelFields is not None and not bAppend:

        nDstFieldCount = 0
        if poDstFDefn is not None:
            nDstFieldCount = poDstFDefn.GetFieldCount()

        for papszSelField in papszSelFields:

            iSrcField = poSrcFDefn.GetFieldIndex(papszSelField)
            if iSrcField >= 0:
                poSrcFieldDefn = poSrcFDefn.GetFieldDefn(iSrcField)
                oFieldDefn = ogr.FieldDefn(
                    poSrcFieldDefn.GetNameRef(), poSrcFieldDefn.GetType()
                )
                oFieldDefn.SetWidth(poSrcFieldDefn.GetWidth())
                oFieldDefn.SetPrecision(poSrcFieldDefn.GetPrecision())

                if papszFieldTypesToString is not None and (
                    CSLFindString(papszFieldTypesToString, "All") != -1
                    or CSLFindString(
                        papszFieldTypesToString,
                        ogr.GetFieldTypeName(poSrcFieldDefn.GetType()),
                    )
                    != -1
                ):

                    oFieldDefn.SetType(ogr.OFTString)

                # The field may have been already created at layer creation
                iDstField = -1
                if poDstFDefn is not None:
                    iDstField = poDstFDefn.GetFieldIndex(oFieldDefn.GetNameRef())
                if iDstField >= 0:
                    panMap[iSrcField] = iDstField
                elif poDstLayer.CreateField(oFieldDefn) == 0:
                    # now that we've created a field, GetLayerDefn() won't return NULL
                    if poDstFDefn is None:
                        poDstFDefn = poDstLayer.GetLayerDefn()

                    # Sanity check : if it fails, the driver is buggy
                    if (
                        poDstFDefn is not None
                        and poDstFDefn.GetFieldCount() != nDstFieldCount + 1
                    ):
                        print(
                            "The output driver has claimed to have added the %s field, but it did not!"
                            % oFieldDefn.GetNameRef()
                        )
                    else:
                        panMap[iSrcField] = nDstFieldCount
                        nDstFieldCount = nDstFieldCount + 1

            else:
                print("Field '" + papszSelField + "' not found in source layer.")
                if not bSkipFailures:
                    return None

        # --------------------------------------------------------------------
        # Use SetIgnoredFields() on source layer if available
        # --------------------------------------------------------------------

        # Here we differ from the ogr2ogr.cpp implementation since the OGRFeatureQuery
        # isn't mapped to swig. So in that case just don't use SetIgnoredFields()
        # to avoid issue raised in #4015
        if poSrcLayer.TestCapability(ogr.OLCIgnoreFields) and pszWHERE is None:
            papszIgnoredFields = []
            for iSrcField in range(nSrcFieldCount):
                pszFieldName = poSrcFDefn.GetFieldDefn(iSrcField).GetNameRef()
                bFieldRequested = False
                for papszSelField in papszSelFields:
                    if EQUAL(pszFieldName, papszSelField):
                        bFieldRequested = True
                        break

                if pszZField is not None and EQUAL(pszFieldName, pszZField):
                    bFieldRequested = True

                # If source field not requested, add it to ignored files list
                if not bFieldRequested:
                    papszIgnoredFields.append(pszFieldName)

            poSrcLayer.SetIgnoredFields(papszIgnoredFields)

    elif not bAppend:

        nDstFieldCount = 0
        if poDstFDefn is not None:
            nDstFieldCount = poDstFDefn.GetFieldCount()

        for iField in range(nSrcFieldCount):

            poSrcFieldDefn = poSrcFDefn.GetFieldDefn(iField)
            oFieldDefn = ogr.FieldDefn(
                poSrcFieldDefn.GetNameRef(), poSrcFieldDefn.GetType()
            )
            oFieldDefn.SetWidth(poSrcFieldDefn.GetWidth())
            oFieldDefn.SetPrecision(poSrcFieldDefn.GetPrecision())

            if papszFieldTypesToString is not None and (
                CSLFindString(papszFieldTypesToString, "All") != -1
                or CSLFindString(
                    papszFieldTypesToString,
                    ogr.GetFieldTypeName(poSrcFieldDefn.GetType()),
                )
                != -1
            ):

                oFieldDefn.SetType(ogr.OFTString)

            # The field may have been already created at layer creation
            iDstField = -1
            if poDstFDefn is not None:
                iDstField = poDstFDefn.GetFieldIndex(oFieldDefn.GetNameRef())
            if iDstField >= 0:
                panMap[iField] = iDstField
            elif poDstLayer.CreateField(oFieldDefn) == 0:
                # now that we've created a field, GetLayerDefn() won't return NULL
                if poDstFDefn is None:
                    poDstFDefn = poDstLayer.GetLayerDefn()

                # Sanity check : if it fails, the driver is buggy
                if (
                    poDstFDefn is not None
                    and poDstFDefn.GetFieldCount() != nDstFieldCount + 1
                ):
                    print(
                        "The output driver has claimed to have added the %s field, but it did not!"
                        % oFieldDefn.GetNameRef()
                    )
                else:
                    panMap[iField] = nDstFieldCount
                    nDstFieldCount = nDstFieldCount + 1

    else:
        # For an existing layer, build the map by fetching the index in the destination
        # layer for each source field
        if poDstFDefn is None:
            print("poDstFDefn == NULL.\n")
            return None

        for iField in range(nSrcFieldCount):
            poSrcFieldDefn = poSrcFDefn.GetFieldDefn(iField)
            iDstField = poDstFDefn.GetFieldIndex(poSrcFieldDefn.GetNameRef())
            if iDstField >= 0:
                panMap[iField] = iDstField

    iSrcZField = -1
    if pszZField is not None:
        iSrcZField = poSrcFDefn.GetFieldIndex(pszZField)

    psInfo = TargetLayerInfo()
    psInfo.poDstLayer = poDstLayer
    psInfo.poCT = poCT
    # psInfo.papszTransformOptions = papszTransformOptions
    psInfo.panMap = panMap
    psInfo.iSrcZField = iSrcZField

    return psInfo


# **********************************************************************
#                           TranslateLayer()
# **********************************************************************


def TranslateLayer(
    psInfo,
    poSrcDS,
    poSrcLayer,
    poDstDS,
    poOutputSRS,
    bNullifyOutputSRS,
    eGType,
    bPromoteToMulti,
    nCoordDim,
    eGeomOp,
    dfGeomOpParam,
    nCountLayerFeatures,
    poClipSrc,
    poClipDst,
    bExplodeCollections,
    nSrcFileSize,
    pnReadFeatureCount,
    pfnProgress,
    pProgressArg,
):
    # pylint: disable=unused-argument
    bForceToPolygon = False
    bForceToMultiPolygon = False
    bForceToMultiLineString = False

    poDstLayer = psInfo.poDstLayer
    # papszTransformOptions = psInfo.papszTransformOptions
    poCT = psInfo.poCT
    panMap = psInfo.panMap
    iSrcZField = psInfo.iSrcZField

    if poOutputSRS is None and not bNullifyOutputSRS:
        poOutputSRS = poSrcLayer.GetSpatialRef()

    if wkbFlatten(eGType) == ogr.wkbPolygon:
        bForceToPolygon = True
    elif wkbFlatten(eGType) == ogr.wkbMultiPolygon:
        bForceToMultiPolygon = True
    elif wkbFlatten(eGType) == ogr.wkbMultiLineString:
        bForceToMultiLineString = True

    # --------------------------------------------------------------------
    #      Transfer features.
    # --------------------------------------------------------------------
    nFeaturesInTransaction = 0
    nCount = 0

    if nGroupTransactions > 0:
        poDstLayer.StartTransaction()

    while True:
        poDstFeature = None

        if nFIDToFetch != ogr.NullFID:

            # // Only fetch feature on first pass.
            if nFeaturesInTransaction == 0:
                poFeature = poSrcLayer.GetFeature(nFIDToFetch)
            else:
                poFeature = None

        else:
            poFeature = poSrcLayer.GetNextFeature()

        if poFeature is None:
            break

        nParts = 0
        nIters = 1
        if bExplodeCollections:
            poSrcGeometry = poFeature.GetGeometryRef()
            if poSrcGeometry is not None:
                eSrcType = wkbFlatten(poSrcGeometry.GetGeometryType())
                if (
                    eSrcType == ogr.wkbMultiPoint
                    or eSrcType == ogr.wkbMultiLineString
                    or eSrcType == ogr.wkbMultiPolygon
                    or eSrcType == ogr.wkbGeometryCollection
                ):
                    nParts = poSrcGeometry.GetGeometryCount()
                    nIters = nParts
                    if nIters == 0:
                        nIters = 1

        for iPart in range(nIters):
            nFeaturesInTransaction = nFeaturesInTransaction + 1
            if nFeaturesInTransaction == nGroupTransactions:
                poDstLayer.CommitTransaction()
                poDstLayer.StartTransaction()
                nFeaturesInTransaction = 0

            gdal.ErrorReset()
            poDstFeature = ogr.Feature(poDstLayer.GetLayerDefn())

            if poDstFeature.SetFromWithMap(poFeature, 1, panMap) != 0:

                if nGroupTransactions > 0:
                    poDstLayer.CommitTransaction()

                print(
                    "Unable to translate feature %d from layer %s"
                    % (poFeature.GetFID(), poSrcLayer.GetName())
                )

                return False

            if bPreserveFID:
                poDstFeature.SetFID(poFeature.GetFID())

            poDstGeometry = poDstFeature.GetGeometryRef()
            if poDstGeometry is not None:

                if nParts > 0:
                    # For -explodecollections, extract the iPart(th) of the geometry
                    poPart = poDstGeometry.GetGeometryRef(iPart).Clone()
                    poDstFeature.SetGeometryDirectly(poPart)
                    poDstGeometry = poPart

                if iSrcZField != -1:
                    SetZ(poDstGeometry, poFeature.GetFieldAsDouble(iSrcZField))
                    # This will correct the coordinate dimension to 3
                    poDupGeometry = poDstGeometry.Clone()
                    poDstFeature.SetGeometryDirectly(poDupGeometry)
                    poDstGeometry = poDupGeometry

                if nCoordDim == 2 or nCoordDim == 3:
                    poDstGeometry.SetCoordinateDimension(nCoordDim)

                if eGeomOp == GeomOperation.SEGMENTIZE:
                    pass
                    # if (poDstFeature.GetGeometryRef() is not None and dfGeomOpParam > 0)
                    #    poDstFeature.GetGeometryRef().segmentize(dfGeomOpParam);
                elif (
                    eGeomOp == GeomOperation.SIMPLIFY_PRESERVE_TOPOLOGY
                    and dfGeomOpParam > 0
                ):
                    poNewGeom = poDstGeometry.SimplifyPreserveTopology(dfGeomOpParam)
                    if poNewGeom is not None:
                        poDstFeature.SetGeometryDirectly(poNewGeom)
                        poDstGeometry = poNewGeom

                if poClipSrc is not None:
                    poClipped = poDstGeometry.Intersection(poClipSrc)
                    if poClipped is None or poClipped.IsEmpty():
                        # Report progress
                        nCount = nCount + 1
                        if pfnProgress is not None:
                            pfnProgress(
                                nCount * 1.0 / nCountLayerFeatures, "", pProgressArg
                            )
                        continue

                    poDstFeature.SetGeometryDirectly(poClipped)
                    poDstGeometry = poClipped

                if poCT is not None:
                    eErr = poDstGeometry.Transform(poCT)
                    if eErr != 0:
                        if nGroupTransactions > 0:
                            poDstLayer.CommitTransaction()

                        print(
                            "Failed to reproject feature %d (geometry probably out of source or destination SRS)."
                            % poFeature.GetFID()
                        )
                        if not bSkipFailures:
                            return False

                elif poOutputSRS is not None:
                    poDstGeometry.AssignSpatialReference(poOutputSRS)

                if poClipDst is not None:
                    poClipped = poDstGeometry.Intersection(poClipDst)
                    if poClipped is None or poClipped.IsEmpty():
                        continue

                    poDstFeature.SetGeometryDirectly(poClipped)
                    poDstGeometry = poClipped

                if bForceToPolygon:
                    poDstFeature.SetGeometryDirectly(ogr.ForceToPolygon(poDstGeometry))

                elif bForceToMultiPolygon or (
                    bPromoteToMulti
                    and wkbFlatten(poDstGeometry.GetGeometryType()) == ogr.wkbPolygon
                ):
                    poDstFeature.SetGeometryDirectly(
                        ogr.ForceToMultiPolygon(poDstGeometry)
                    )

                elif bForceToMultiLineString or (
                    bPromoteToMulti
                    and wkbFlatten(poDstGeometry.GetGeometryType()) == ogr.wkbLineString
                ):
                    poDstFeature.SetGeometryDirectly(
                        ogr.ForceToMultiLineString(poDstGeometry)
                    )

            gdal.ErrorReset()
            if poDstLayer.CreateFeature(poDstFeature) != 0 and not bSkipFailures:
                if nGroupTransactions > 0:
                    poDstLayer.RollbackTransaction()

                return False

        # Report progress
        nCount = nCount + 1
        if pfnProgress is not None:
            if nSrcFileSize != 0:
                if (nCount % 1000) == 0:
                    poFCLayer = poSrcDS.ExecuteSQL("GetBytesRead()", None, None)
                    if poFCLayer is not None:
                        poFeat = poFCLayer.GetNextFeature()
                        if poFeat is not None:
                            pszReadSize = poFeat.GetFieldAsString(0)
                            nReadSize = int(pszReadSize)
                            pfnProgress(
                                nReadSize * 1.0 / nSrcFileSize, "", pProgressArg
                            )
                    poSrcDS.ReleaseResultSet(poFCLayer)
            else:
                pfnProgress(nCount * 1.0 / nCountLayerFeatures, "", pProgressArg)

        if pnReadFeatureCount is not None:
            pnReadFeatureCount[0] = nCount

    if nGroupTransactions > 0:
        poDstLayer.CommitTransaction()

    return True


if __name__ == "__main__":
    sys.exit(main(sys.argv))
