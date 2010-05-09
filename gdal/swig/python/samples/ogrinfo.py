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
from osgeo import gdal
from osgeo import ogr

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
    nRet = 0

    if argv is None:
        argv = sys.argv

    argv = ogr.GeneralCmdLineProcessor( argv )

#/* -------------------------------------------------------------------- */
#/*      Processing command line arguments.                              */
#/* -------------------------------------------------------------------- */
    if argv is None:
        sys.exit( -1 )

    nArgc = len(argv)

    iArg = 1
    while iArg < nArgc:

        if EQUAL(argv[iArg],"--utility_version"):
            print("%s is running against GDAL %s" %
                   (argv[0], gdal.VersionInfo("RELEASE_NAME")))
            sys.exit(0)

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

        #elif( EQUALN(argv[iArg],"-fields=", strlen("-fields=")) )
        #{
        #    char* pszTemp = (char*)CPLMalloc(32 + strlen(argv[iArg]));
        #    sprintf(pszTemp, "DISPLAY_FIELDS=%s", argv[iArg] + strlen("-fields="));
        #    papszOptions = CSLAddString(papszOptions, pszTemp);
        #    CPLFree(pszTemp);
        #}
        #elif( EQUALN(argv[iArg],"-geom=", strlen("-geom=")) )
        #{
        #    char* pszTemp = (char*)CPLMalloc(32 + strlen(argv[iArg]));
        #    sprintf(pszTemp, "DISPLAY_GEOMETRY=%s", argv[iArg] + strlen("-geom="));
        #    papszOptions = CSLAddString(papszOptions, pszTemp);
        #    CPLFree(pszTemp);
        #}
        elif argv[iArg][0] == '-':
            Usage()

        elif pszDataSource is None:
            pszDataSource = argv[iArg]
        else:
            if papszLayers is None:
                papszLayers = []
            papszLayers.append( argv[iArg] )
            bAllLayers = False

        iArg = iArg + 1

    if pszDataSource is None:
        Usage()

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

        nRet = 1
        sys.exit(nRet)

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

            ReportOnLayer( poResultSet, None, None )
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
                    sys.exit( 1 )

                if not bAllLayers:
                    line = "%d: %s" % (iLayer+1, poLayer.GetLayerDefn().GetName())

                    if poLayer.GetLayerDefn().GetGeomType() != ogr.wkbUnknown:
                        line = line + " (%s)" % ogr.GeometryTypeToName( poLayer.GetLayerDefn().GetGeomType() )

                    print(line)
                else:
                    if iRepeat != 0:
                        poLayer.ResetReading()

                    ReportOnLayer( poLayer, pszWHERE, poSpatialFilter )

        else:
#/* -------------------------------------------------------------------- */ 
#/*      Process specified data source layers.                           */ 
#/* -------------------------------------------------------------------- */ 
            for papszIter in papszLayers:
                poLayer = poDS.GetLayerByName(papszIter)

                if poLayer is None:
                    print( "FAILURE: Couldn't fetch requested layer %s!" % papszIter )
                    sys.exit( 1 )

                if iRepeat != 0:
                    poLayer.ResetReading()

                ReportOnLayer( poLayer, pszWHERE, poSpatialFilter )

#/* -------------------------------------------------------------------- */
#/*      Close down.                                                     */
#/* -------------------------------------------------------------------- */
    poDS.Destroy()

    return nRet

#/************************************************************************/
#/*                               Usage()                                */
#/************************************************************************/

def Usage():

    print( "Usage: ogrinfo [--help-general] [-ro] [-q] [-where restricted_where]\n"
            "               [-spat xmin ymin xmax ymax] [-fid fid]\n"
            "               [-sql statement] [-al] [-so] [-fields={YES/NO}]\n"
            "               [-geom={YES/NO/SUMMARY}][--formats]\n"
            "               datasource_name [layer [layer ...]]")
    sys.exit( 1 )

    return

#/************************************************************************/
#/*                           ReportOnLayer()                            */
#/************************************************************************/

def ReportOnLayer( poLayer, pszWHERE, poSpatialFilter ):

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

    # Python 3.1 needs explicit flushing
    sys.stdout.flush()

    if nFetchFID == ogr.NullFID and not bSummaryOnly:

        poFeature = poLayer.GetNextFeature()
        while poFeature is not None:
            poFeature.DumpReadable( )
            poFeature = poLayer.GetNextFeature()

    elif nFetchFID != ogr.NullFID:

        poFeature = poLayer.GetFeature( nFetchFID )
        if poFeature is None:
            print( "Unable to locate feature id %d on this layer." % nFetchFID )

        else:
            poFeature.DumpReadable()

    return


version_num = int(gdal.VersionInfo('VERSION_NUM'))
if version_num < 1800: # because of ogr.GetFieldTypeName
    print('ERROR: Python bindings of GDAL 1.8.0 or later required')
    sys.exit(1)

main( sys.argv )