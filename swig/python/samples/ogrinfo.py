#!/usr/bin/env python
#/******************************************************************************
# * $Id$
# *
# * Project:  OpenGIS Simple Features Reference Implementation
# * Purpose:  Python port of a simple client for viewing OGR driver data.
# * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# *
# * Port from ogrinfo.cpp whose author is Frank Warmerdam
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

# Note : this is the most direct port of ogrinfo.cpp possible
# It could be made much more Python'ish !

import sys

try:
    from osgeo import gdal
    from osgeo import ogr
except:
    import gdal
    import ogr

bReadOnly = False
bVerbose = True
bSummaryOnly = False
nFetchFID = ogr.NullFID
papszOptions = None

def EQUAL(a, b):
    return a.lower() == b.lower()

#/************************************************************************/
#/*                                main()                                */
#/************************************************************************/

def main(argv = None):
    
    global bReadOnly
    global bVerbose
    global bSummaryOnly
    global nFetchFID
    global papszOptions
    
    pszWHERE = None
    pszDataSource = None
    papszLayers = None
    poSpatialFilter = None
    nRepeatCount = 1
    bAllLayers = False
    pszSQLStatement = None
    pszDialect = None
    options = {}

    if argv is None:
        argv = sys.argv

    argv = ogr.GeneralCmdLineProcessor( argv )

#/* -------------------------------------------------------------------- */
#/*      Processing command line arguments.                              */
#/* -------------------------------------------------------------------- */
    if argv is None:
        return 1

    nArgc = len(argv)

    iArg = 1
    while iArg < nArgc:

        if EQUAL(argv[iArg],"--utility_version"):
            print("%s is running against GDAL %s" %
                   (argv[0], gdal.VersionInfo("RELEASE_NAME")))
            return 0

        elif EQUAL(argv[iArg],"-ro"):
            bReadOnly = True
        elif EQUAL(argv[iArg],"-q") or EQUAL(argv[iArg],"-quiet"):
            bVerbose = False
        elif EQUAL(argv[iArg],"-fid") and iArg < nArgc-1:
            iArg = iArg + 1
            nFetchFID = int(argv[iArg])
        elif EQUAL(argv[iArg],"-spat") and iArg + 4 < nArgc:
            oRing = ogr.Geometry(ogr.wkbLinearRing)

            oRing.AddPoint( float(argv[iArg+1]), float(argv[iArg+2]) )
            oRing.AddPoint( float(argv[iArg+1]), float(argv[iArg+4]) )
            oRing.AddPoint( float(argv[iArg+3]), float(argv[iArg+4]) )
            oRing.AddPoint( float(argv[iArg+3]), float(argv[iArg+2]) )
            oRing.AddPoint( float(argv[iArg+1]), float(argv[iArg+2]) )

            poSpatialFilter = ogr.Geometry(ogr.wkbPolygon)
            poSpatialFilter.AddGeometry(oRing)
            iArg = iArg + 4

        elif EQUAL(argv[iArg],"-where") and iArg < nArgc-1:
            iArg = iArg + 1
            pszWHERE = argv[iArg]

        elif EQUAL(argv[iArg],"-sql") and iArg < nArgc-1:
            iArg = iArg + 1
            pszSQLStatement = argv[iArg]

        elif EQUAL(argv[iArg],"-dialect") and iArg < nArgc-1:
            iArg = iArg + 1
            pszDialect = argv[iArg]

        elif EQUAL(argv[iArg],"-rc") and iArg < nArgc-1:
            iArg = iArg + 1
            nRepeatCount = int(argv[iArg])

        elif EQUAL(argv[iArg],"-al"):
            bAllLayers = True

        elif EQUAL(argv[iArg],"-so") or EQUAL(argv[iArg],"-summary"):
            bSummaryOnly = True

        elif len(argv[iArg]) > 8 and EQUAL(argv[iArg][0:8],"-fields="):
            options['DISPLAY_FIELDS'] = argv[iArg][7:len(argv[iArg])]

        elif len(argv[iArg]) > 6 and EQUAL(argv[iArg][0:6],"-geom="):
            options['DISPLAY_GEOMETRY'] = argv[iArg][6:len(argv[iArg])]

        elif argv[iArg][0] == '-':
            return Usage()

        elif pszDataSource is None:
            pszDataSource = argv[iArg]
        else:
            if papszLayers is None:
                papszLayers = []
            papszLayers.append( argv[iArg] )
            bAllLayers = False

        iArg = iArg + 1

    if pszDataSource is None:
        return Usage()

#/* -------------------------------------------------------------------- */
#/*      Open data source.                                               */
#/* -------------------------------------------------------------------- */
    poDS = None
    poDriver = None

    poDS = ogr.Open( pszDataSource, not bReadOnly )
    if poDS is None and not bReadOnly:
        poDS = ogr.Open( pszDataSource, False )
        if poDS is not None and bVerbose:
            print( "Had to open data source read-only." )
            bReadOnly = True
    poDriver = poDS.GetDriver()

#/* -------------------------------------------------------------------- */
#/*      Report failure                                                  */
#/* -------------------------------------------------------------------- */
    if poDS is None:
        print( "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers." % pszDataSource )

        for iDriver in range(ogr.GetDriverCount()):
            print( "  . %s" % ogr.GetDriver(iDriver).GetName() )

        return 1

#/* -------------------------------------------------------------------- */
#/*      Some information messages.                                      */
#/* -------------------------------------------------------------------- */
    if bVerbose:
        print( "INFO: Open of `%s'\n"
                "      using driver `%s' successful." % (pszDataSource, poDriver.GetName()) )

    if bVerbose and pszDataSource != poDS.GetName():
        print( "INFO: Internal data source name `%s'\n"
                "      different from user name `%s'." % (poDS.GetName(), pszDataSource ))

#/* -------------------------------------------------------------------- */
#/*      Special case for -sql clause.  No source layers required.       */
#/* -------------------------------------------------------------------- */
    if pszSQLStatement is not None:
        poResultSet = None

        nRepeatCount = 0  #// skip layer reporting.

        if papszLayers is not None:
            print( "layer names ignored in combination with -sql." )

        poResultSet = poDS.ExecuteSQL( pszSQLStatement, poSpatialFilter, 
                                        pszDialect )

        if poResultSet is not None:
            if pszWHERE is not None:
                poResultSet.SetAttributeFilter( pszWHERE )

            ReportOnLayer( poResultSet, None, None, options )
            poDS.ReleaseResultSet( poResultSet )

    #gdal.Debug( "OGR", "GetLayerCount() = %d\n", poDS.GetLayerCount() )

    for iRepeat in range(nRepeatCount):
        if papszLayers is None:
#/* -------------------------------------------------------------------- */ 
#/*      Process each data source layer.                                 */ 
#/* -------------------------------------------------------------------- */ 
            for iLayer in range(poDS.GetLayerCount()):
                poLayer = poDS.GetLayer(iLayer)

                if poLayer is None:
                    print( "FAILURE: Couldn't fetch advertised layer %d!" % iLayer )
                    return 1

                if not bAllLayers:
                    line = "%d: %s" % (iLayer+1, poLayer.GetLayerDefn().GetName())

                    if poLayer.GetLayerDefn().GetGeomType() != ogr.wkbUnknown:
                        line = line + " (%s)" % ogr.GeometryTypeToName( poLayer.GetLayerDefn().GetGeomType() )

                    print(line)
                else:
                    if iRepeat != 0:
                        poLayer.ResetReading()

                    ReportOnLayer( poLayer, pszWHERE, poSpatialFilter, options )

        else:
#/* -------------------------------------------------------------------- */ 
#/*      Process specified data source layers.                           */ 
#/* -------------------------------------------------------------------- */ 
            for papszIter in papszLayers:
                poLayer = poDS.GetLayerByName(papszIter)

                if poLayer is None:
                    print( "FAILURE: Couldn't fetch requested layer %s!" % papszIter )
                    return 1

                if iRepeat != 0:
                    poLayer.ResetReading()

                ReportOnLayer( poLayer, pszWHERE, poSpatialFilter, options )

#/* -------------------------------------------------------------------- */
#/*      Close down.                                                     */
#/* -------------------------------------------------------------------- */
    poDS.Destroy()

    return 0

#/************************************************************************/
#/*                               Usage()                                */
#/************************************************************************/

def Usage():

    print( "Usage: ogrinfo [--help-general] [-ro] [-q] [-where restricted_where]\n"
            "               [-spat xmin ymin xmax ymax] [-fid fid]\n"
            "               [-sql statement] [-al] [-so] [-fields={YES/NO}]\n"
            "               [-geom={YES/NO/SUMMARY}][--formats]\n"
            "               datasource_name [layer [layer ...]]")
    return 1

#/************************************************************************/
#/*                           ReportOnLayer()                            */
#/************************************************************************/

def ReportOnLayer( poLayer, pszWHERE, poSpatialFilter, options ):

    poDefn = poLayer.GetLayerDefn()

#/* -------------------------------------------------------------------- */
#/*      Set filters if provided.                                        */
#/* -------------------------------------------------------------------- */
    if pszWHERE is not None:
        poLayer.SetAttributeFilter( pszWHERE )

    if poSpatialFilter is not None:
        poLayer.SetSpatialFilter( poSpatialFilter )

#/* -------------------------------------------------------------------- */
#/*      Report various overall information.                             */
#/* -------------------------------------------------------------------- */
    print( "" )
    
    print( "Layer name: %s" % poDefn.GetName() )

    if bVerbose:
        print( "Geometry: %s" % ogr.GeometryTypeToName( poDefn.GetGeomType() ) )
        
        print( "Feature Count: %d" % poLayer.GetFeatureCount() )
        
        oExt = poLayer.GetExtent(True)
        if oExt is not None:
            print("Extent: (%f, %f) - (%f, %f)" % (oExt[0], oExt[1], oExt[2], oExt[3]))

        if poLayer.GetSpatialRef() is None:
            pszWKT = "(unknown)"
        else:
            pszWKT = poLayer.GetSpatialRef().ExportToPrettyWkt()

        print( "Layer SRS WKT:\n%s" % pszWKT )
    
        if len(poLayer.GetFIDColumn()) > 0:
            print( "FID Column = %s" % poLayer.GetFIDColumn() )
    
        if len(poLayer.GetGeometryColumn()) > 0:
            print( "Geometry Column = %s" % poLayer.GetGeometryColumn() )

        for iAttr in range(poDefn.GetFieldCount()):
            poField = poDefn.GetFieldDefn( iAttr )
            
            print( "%s: %s (%d.%d)" % ( \
                    poField.GetNameRef(), \
                    poField.GetFieldTypeName( poField.GetType() ), \
                    poField.GetWidth(), \
                    poField.GetPrecision() ))

#/* -------------------------------------------------------------------- */
#/*      Read, and dump features.                                        */
#/* -------------------------------------------------------------------- */
    poFeature = None

    if nFetchFID == ogr.NullFID and not bSummaryOnly:

        poFeature = poLayer.GetNextFeature()
        while poFeature is not None:
            DumpReadableFeature(poFeature, options)
            poFeature = poLayer.GetNextFeature()

    elif nFetchFID != ogr.NullFID:

        poFeature = poLayer.GetFeature( nFetchFID )
        if poFeature is None:
            print( "Unable to locate feature id %d on this layer." % nFetchFID )

        else:
            DumpReadableFeature(poFeature, options)

    return


def DumpReadableFeature( poFeature, options = None ):

    poDefn = poFeature.GetDefnRef()
    print("OGRFeature(%s):%ld" % (poDefn.GetName(), poFeature.GetFID() ))

    if 'DISPLAY_FIELDS' not in options or EQUAL(options['DISPLAY_FIELDS'], 'yes'):
        for iField in range(poDefn.GetFieldCount()):

            poFDefn = poDefn.GetFieldDefn(iField)

            line =  "  %s (%s) = " % ( \
                    poFDefn.GetNameRef(), \
                    ogr.GetFieldTypeName(poFDefn.GetType()) )

            if poFeature.IsFieldSet( iField ):
                line = line + "%s" % (poFeature.GetFieldAsString( iField ) )
            else:
                line = line + "(null)"

            print(line)


    if poFeature.GetStyleString() is not None:

        if 'DISPLAY_STYLE' not in options or EQUAL(options['DISPLAY_STYLE'], 'yes'):
            print("  Style = %s" % GetStyleString() )

    poGeometry = poFeature.GetGeometryRef()
    if poGeometry is not None:
        if 'DISPLAY_GEOMETRY' not in options or not EQUAL(options['DISPLAY_GEOMETRY'], 'no'):
            DumpReadableGeometry( poGeometry, "  ", options)

    print('')

    return


def DumpReadableGeometry( poGeometry, pszPrefix, options ):

    if pszPrefix == None:
        pszPrefix = ""

    if 'DISPLAY_GEOMETRY' in options and EQUAL(options['DISPLAY_GEOMETRY'], 'SUMMARY'):

        line = ("%s%s : " % (pszPrefix, poGeometry.GetGeometryName() ))
        eType = poGeometry.GetGeometryType()
        if eType == ogr.wkbLineString or eType == ogr.wkbLineString25D:
            line = line + ("%d points" % poGeometry.GetPointCount())
            print(line)
        elif eType == ogr.wkbPolygon or eType == ogr.wkbPolygon25D:
            nRings = poGeometry.GetGeometryCount()
            if nRings == 0:
                line = line + "empty"
            else:
                poRing = poGeometry.GetGeometryRef(0)
                line = line + ("%d points" % poRing.GetPointCount())
                if nRings > 1:
                    line = line + (", %d inner rings (" % (nRings - 1))
                    for ir in range(0,nRings-1):
                        if ir > 0:
                            line = line + ", "
                        poRing = poGeometry.GetGeometryRef(ir+1)
                        line = line + ("%d points" % poRing.GetPointCount())
                    line = line + ")"
            print(line)

        elif eType == ogr.wkbMultiPoint or \
            eType == ogr.wkbMultiPoint25D or \
            eType == ogr.wkbMultiLineString or \
            eType == ogr.wkbMultiLineString25D or \
            eType == ogr.wkbMultiPolygon or \
            eType == ogr.wkbMultiPolygon25D or \
            eType == ogr.wkbGeometryCollection or \
            eType == ogr.wkbGeometryCollection25D:

                line = line + "%d geometries:" % poGeometry.GetGeometryCount()
                print(line)
                for ig in range(poGeometry.GetGeometryCount()):
                    subgeom = poGeometry.GetGeometryRef(ig)
                    from sys import version_info
                    if version_info >= (3,0,0):
                        exec('print("", end=" ")')
                    else:
                        exec('print "", ')
                    DumpReadableGeometry( subgeom, pszPrefix, options)
        else:
            print(line)

    elif 'DISPLAY_GEOMETRY' not in options or EQUAL(options['DISPLAY_GEOMETRY'], 'yes') \
            or EQUAL(options['DISPLAY_GEOMETRY'], 'WKT'):

        print("%s%s" % (pszPrefix, poGeometry.ExportToWkt() ))

    return

if __name__ == '__main__':
    version_num = int(gdal.VersionInfo('VERSION_NUM'))
    if version_num < 1800: # because of ogr.GetFieldTypeName
        print('ERROR: Python bindings of GDAL 1.8.0 or later required')
        sys.exit(1)

    sys.exit(main( sys.argv ))
