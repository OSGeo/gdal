#!/usr/bin/env python
# -*- coding: utf-8 -*-
#******************************************************************************
#  $Id$
# 
#  Project:  GDAL Python Interface
#  Purpose:  Application for executing OGR layer algebra operations
#  Author:   Even Rouault, even dot rouault at mines-paris dot org
# 
#******************************************************************************
#  Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
# 
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
#******************************************************************************

from osgeo import gdal, ogr, osr
import os
import sys

###############################################################################

def Usage():
    print("""
Usage: ogr_layer_algebra.py Union|Intersection|SymDifference|Identity|Update|Clip|Erase
                            -input_ds name [-input_lyr name]
                            -method_ds [-method_lyr name]
                            -output_ds name [-output_lyr name] [-overwrite]
                            [-opt NAME=VALUE]*
                            [-f format_name] [-dsco NAME=VALUE]* [-lco NAME=VALUE]*
                            [-input_fields NONE|ALL|fld1,fl2,...fldN] [-method_fields NONE|ALL|fld1,fl2,...fldN]
                            [-nlt geom_type] [-a_srs srs_def]""")
    return 1

###############################################################################

def EQUAL(a, b):
    return a.lower() == b.lower()

###############################################################################

def CreateLayer(output_ds, output_lyr_name, srs, geom_type, lco, \
                input_lyr, input_fields, \
                method_lyr, method_fields, opt):

    output_lyr = output_ds.CreateLayer(output_lyr_name, srs, geom_type, lco)
    if output_lyr is None:
        print('Cannot create layer "%s"' % output_lyr_name)
        return None

    input_prefix = ''
    method_prefix = ''
    for val in opt:
        if val.lower().find('input_prefix=') == 0:
            input_prefix = val[len('input_prefix='):]
        elif val.lower().find('method_prefix=') == 0:
            method_prefix = val[len('method_prefix='):]

    if input_fields == 'ALL':
        layer_defn = input_lyr.GetLayerDefn()
        for idx in range(layer_defn.GetFieldCount()):
            fld_defn = layer_defn.GetFieldDefn(idx)
            fld_defn = ogr.FieldDefn(input_prefix + fld_defn.GetName(), fld_defn.GetType())
            if output_lyr.CreateField(fld_defn) != 0:
                print('Cannot create field "%s" in layer "%s"' % (fld_defn.GetName(), output_lyr.GetName()))

    elif input_fields != 'NONE':
        layer_defn = input_lyr.GetLayerDefn()
        for fld in input_fields:
            idx = layer_defn.GetFieldIndex(fld)
            if idx < 0:
                print('Cannot find field "%s" in layer "%s"' % (fld, layer_defn.GetName()))
                continue
            fld_defn = layer_defn.GetFieldDefn(idx)
            fld_defn = ogr.FieldDefn(input_prefix + fld_defn.GetName(), fld_defn.GetType())
            if output_lyr.CreateField(fld_defn) != 0:
                print('Cannot create field "%s" in layer "%s"' % (fld, output_lyr.GetName()))

    if method_fields == 'ALL':
        layer_defn = method_lyr.GetLayerDefn()
        for idx in range(layer_defn.GetFieldCount()):
            fld_defn = layer_defn.GetFieldDefn(idx)
            fld_defn = ogr.FieldDefn(method_prefix + fld_defn.GetName(), fld_defn.GetType())
            if output_lyr.CreateField(fld_defn) != 0:
                print('Cannot create field "%s" in layer "%s"' % (fld_defn.GetName(), output_lyr.GetName()))

    elif method_fields != 'NONE':
        layer_defn = method_lyr.GetLayerDefn()
        for fld in method_fields:
            idx = layer_defn.GetFieldIndex(fld)
            if idx < 0:
                print('Cannot find field "%s" in layer "%s"' % (fld, layer_defn.GetName()))
                continue
            fld_defn = layer_defn.GetFieldDefn(idx)
            fld_defn = ogr.FieldDefn(method_prefix + fld_defn.GetName(), fld_defn.GetType())
            if output_lyr.CreateField(fld_defn) != 0:
                print('Cannot create field "%s" in layer "%s"' % (fld, output_lyr.GetName()))

    return output_lyr

###############################################################################

def main(argv = None):

    format = 'ESRI Shapefile'
    quiet_flag = 0
    input_ds_name = None
    input_lyr_name = None
    method_ds_name = None
    method_lyr_name = None
    output_ds_name = None
    output_lyr_name = None
    op_str = None
    dsco = []
    lco = []
    opt = []
    overwrite = False
    input_fields = 'ALL'
    method_fields = 'ALL'
    geom_type = ogr.wkbUnknown
    srs_name = None
    srs = None

    argv = ogr.GeneralCmdLineProcessor( sys.argv )
    if argv is None:
        return 1

    # Parse command line arguments.
    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-f' and i + 1 < len(argv):
            i = i + 1
            format = argv[i]

        elif arg == '-input_ds' and i + 1 < len(argv):
            i = i + 1
            input_ds_name = argv[i]

        elif arg == '-input_lyr' and i + 1 < len(argv):
            i = i + 1
            input_lyr_name = argv[i]

        elif arg == '-method_ds' and i + 1 < len(argv):
            i = i + 1
            method_ds_name = argv[i]

        elif arg == '-method_lyr' and i + 1 < len(argv):
            i = i + 1
            method_lyr_name = argv[i]

        elif arg == '-output_ds' and i + 1 < len(argv):
            i = i + 1
            output_ds_name = argv[i]

        elif arg == '-output_lyr' and i + 1 < len(argv):
            i = i + 1
            output_lyr_name = argv[i]

        elif arg == '-input_fields' and i + 1 < len(argv):
            i = i + 1
            if EQUAL(argv[i], "NONE"):
                input_fields = "NONE"
            elif EQUAL(argv[i], "ALL"):
                input_fields = "ALL"
            else:
                input_fields = argv[i].split(',')

        elif arg == '-method_fields' and i + 1 < len(argv):
            i = i + 1
            if EQUAL(argv[i], "NONE"):
                method_fields = "NONE"
            elif EQUAL(argv[i], "ALL"):
                method_fields = "ALL"
            else:
                method_fields = argv[i].split(',')

        elif arg == '-dsco' and i + 1 < len(argv):
            i = i + 1
            dsco.append(argv[i])

        elif arg == '-lco' and i + 1 < len(argv):
            i = i + 1
            lco.append(argv[i])

        elif arg == '-opt' and i + 1 < len(argv):
            i = i + 1
            opt.append(argv[i])

        elif arg == "-nlt" and i + 1 < len(argv):
            i = i + 1
            val = argv[i]

            if EQUAL(val,"NONE"):
                geom_type = ogr.wkbNone
            elif EQUAL(val,"GEOMETRY"):
                geom_type = ogr.wkbUnknown
            elif EQUAL(val,"POINT"):
                geom_type = ogr.wkbPoint
            elif EQUAL(val,"LINESTRING"):
                geom_type = ogr.wkbLineString
            elif EQUAL(val,"POLYGON"):
                geom_type = ogr.wkbPolygon
            elif EQUAL(val,"GEOMETRYCOLLECTION"):
                geom_type = ogr.wkbGeometryCollection
            elif EQUAL(val,"MULTIPOINT"):
                geom_type = ogr.wkbMultiPoint
            elif EQUAL(val,"MULTILINESTRING"):
                geom_type = ogr.wkbMultiLineString
            elif EQUAL(val,"MULTIPOLYGON"):
                geom_type = ogr.wkbMultiPolygon
            elif EQUAL(val,"GEOMETRY25D"):
                geom_type = ogr.wkbUnknown | ogr.wkb25DBit
            elif EQUAL(val,"POINT25D"):
                geom_type = ogr.wkbPoint25D
            elif EQUAL(val,"LINESTRING25D"):
                geom_type = ogr.wkbLineString25D
            elif EQUAL(val,"POLYGON25D"):
                geom_type = ogr.wkbPolygon25D
            elif EQUAL(val,"GEOMETRYCOLLECTION25D"):
                geom_type = ogr.wkbGeometryCollection25D
            elif EQUAL(val,"MULTIPOINT25D"):
                geom_type = ogr.wkbMultiPoint25D
            elif EQUAL(val,"MULTILINESTRING25D"):
                geom_type = ogr.wkbMultiLineString25D
            elif EQUAL(val,"MULTIPOLYGON25D"):
                geom_type = ogr.wkbMultiPolygon25D
            else:
                print("-nlt %s: type not recognised." % val)
                return 1

        elif arg == "-a_srs" and i + 1 < len(argv):
            i = i + 1
            srs_name = argv[i]

        elif EQUAL(arg, "Union"):
            op_str = "Union"

        elif EQUAL(arg, "Intersection"):
            op_str = "Intersection"

        elif EQUAL(arg, "SymDifference"):
            op_str = "SymDifference"

        elif EQUAL(arg, "Identity"):
            op_str = "Identity"

        elif EQUAL(arg, "Update"):
            op_str = "Update"

        elif EQUAL(arg, "Clip"):
            op_str = "Clip"

        elif EQUAL(arg, "Erase"):
            op_str = "Erase"

        elif arg == "-overwrite":
            overwrite = True

        elif arg == '-q' or arg == '-quiet':
            quiet_flag = 1

        else:
            return Usage()

        i = i + 1

    if input_ds_name is None or \
       method_ds_name is None or \
       output_ds_name is None or \
       op_str is None:
           return Usage()

    if input_fields == 'NONE' and method_fields == 'NONE':
        print('Warning: -input_fields NONE and -method_fields NONE results in all fields being added')

    # Input layer
    input_ds = ogr.Open(input_ds_name)
    if input_ds is None:
        print('Cannot open input dataset : %s' % input_ds_name)
        return 1

    if input_lyr_name is None:
        cnt = input_ds.GetLayerCount()
        if cnt != 1:
            print('Input datasource has not a single layer, so you should specify its name with -input_lyr')
            return 1
        input_lyr = input_ds.GetLayer(0)
    else:
        input_lyr = input_ds.GetLayerByName(input_lyr_name)
    if input_lyr is None:
        print('Cannot find input layer "%s"' % input_lyr_name)
        return 1

    # Method layer
    method_ds = ogr.Open(method_ds_name)
    if method_ds is None:
        print('Cannot open method dataset : %s' % method_ds_name)
        return 1

    if method_lyr_name is None:
        cnt = method_ds.GetLayerCount()
        if cnt != 1:
            print('Method datasource has not a single layer, so you should specify its name with -method_lyr')
            return 1
        method_lyr = method_ds.GetLayer(0)
    else:
        method_lyr = method_ds.GetLayerByName(method_lyr_name)
    if method_lyr is None:
        print('Cannot find method layer "%s"' % method_lyr_name)
        return 1

    # SRS
    if srs_name is not None:
        if not EQUAL(srs_name, "NULL") and not EQUAL(srs_name, "NONE"):
            srs = osr.SpatialReference()
            if srs.SetFromUserInput( srs_name ) != 0:
                print( "Failed to process SRS definition: %s" % srs_name )
                return 1
    else:
        srs = input_lyr.GetSpatialRef()
        srs2 = method_lyr.GetSpatialRef()
        if srs is None and srs2 is not None:
            print('Warning: input layer has no SRS defined, but method layer has one.')
        elif srs is not None and srs2 is None:
            print('Warning: input layer has a SRS defined, but method layer has not one.')
        elif srs is not None and srs2 is not None and srs.IsSame(srs2) != 1:
            print('Warning: input and method layers have SRS defined, but they are not identical. No on-the-fly reprojection will be done.')

    # Result layer
    output_ds = ogr.Open(output_ds_name, update = 1)
    if output_ds is None:
        output_ds = ogr.Open(output_ds_name)
        if output_ds is not None:
            print('Output datasource "%s" exists, but cannot be opened in update mode' % output_ds_name)
            return 1

        drv = ogr.GetDriverByName(format)
        if drv is None:
            print('Cannot find driver %s' % format)
            return 1

        output_ds = drv.CreateDataSource(output_ds_name, options = dsco)
        if output_ds is None:
            print('Cannot create datasource "%s"' % output_ds_name)
            return 1

        # Special case
        if EQUAL(drv.GetName(), "ESRI Shapefile") and output_lyr_name is None \
           and EQUAL(os.path.splitext(output_ds_name)[1], ".SHP"):
            output_lyr_name = os.path.splitext(os.path.basename(output_ds_name))[0]

        if output_lyr_name is None:
            print('-output_lyr should be specified')
            return 1

        output_lyr = CreateLayer(output_ds, output_lyr_name, srs, geom_type, lco, input_lyr, input_fields, method_lyr, method_fields, opt)
        if output_lyr is None:
            return 1
    else:
        drv = output_ds.GetDriver()

        if output_lyr_name is None:
            cnt = output_ds.GetLayerCount()
            if cnt != 1:
                print('Result datasource has not a single layer, so you should specify its name with -output_lyr')
                return 1
            output_lyr = output_ds.GetLayer(0)
            output_lyr_name = output_lyr.GetName()
        else:
            output_lyr = output_ds.GetLayerByName(output_lyr_name)

            if output_lyr is None:
                if EQUAL(drv.GetName(), "ESRI Shapefile") and \
                   EQUAL(os.path.splitext(output_ds_name)[1], ".SHP") and \
                   not EQUAL(output_lyr_name, os.path.splitext(os.path.basename(output_ds_name))[0]):
                       print('Cannot create layer "%s" in a shapefile called "%s"' % (output_lyr_name, output_ds_name))
                       return 1

                output_lyr = CreateLayer(output_ds, output_lyr_name, srs, geom_type, lco, input_lyr, input_fields, method_lyr, method_fields, opt)
                if output_lyr is None:
                    return 1

        if overwrite:
            cnt = output_ds.GetLayerCount()
            for iLayer in range(cnt):
                poLayer = output_ds.GetLayer(iLayer)
                if poLayer is not None \
                    and poLayer.GetName() == output_lyr_name:
                    break
            if iLayer != cnt:
                if output_ds.DeleteLayer(iLayer) != 0:
                    print("DeleteLayer() failed when overwrite requested." )
                    return 1

                output_lyr = CreateLayer(output_ds, output_lyr_name, srs, geom_type, lco, input_lyr, input_fields, method_lyr, method_fields, opt)
                if output_lyr is None:
                    return 1

    op = getattr(input_lyr, op_str)
    if quiet_flag == 0:
        ret = op(method_lyr, output_lyr, options = opt, callback = gdal.TermProgress_nocb)
    else:
        ret = op(method_lyr, output_lyr, options = opt)

    input_ds = None
    method_ds = None
    output_ds = None

    if ret != 0:
        print('An error occured during %s operation' % op_str)
        return 1

    return 0

###############################################################################

if __name__ == '__main__':
    version_num = int(gdal.VersionInfo('VERSION_NUM'))
    if version_num < 1100000:
        print('ERROR: Python bindings of GDAL 1.10 or later required')
        sys.exit(1)

    sys.exit(main( sys.argv ))
