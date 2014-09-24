#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR samples
# Purpose:  Dispatch features into layers according to the value of some fields
#           or the geometry type.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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
###############################################################################

from osgeo import ogr
from osgeo import osr
import sys

###############################################################
# Usage()

def Usage():
    print('ogr_dispatch.py [-f format] -src name -dst name [-field field]+')
    print('                [-25D_as_2D] [-multi_as_single]')
    print('                [-remove_dispatch_fields] [-prefix_with_layer_name]')
    print('                [-dsco KEY=VALUE]* [-lco KEY=VALUE]* [-a_srs srs_def]')
    print('                [-style_as_field] [-where restricted_where] [-gt n] [-quiet]')
    print('')
    print('Dispatch features into layers according to the value of some fields or the')
    print('geometry type.')
    print('')
    print('Arguments:')
    print(' -f format: name of the driver to use to create the destination dataset')
    print('            (default \'ESRI Shapefile\').')
    print(' -src name: name of the source dataset.')
    print(' -dst name: name of the destination dataset (existing or to be created).')
    print(' -field field: name of a field to use to dispatch features. May also')
    print('                        be \'OGR_GEOMETRY\'. At least, one occurence needed.')
    print(' -25D_as_2D: for dispatching, consider 2.5D geometries as 2D.')
    print(' -multi_as_single: for dispatching, consider MULTIPOLYGON as POLYGON and')
    print('                   MULTILINESTRING as LINESTRING.')
    print(' -remove_dispatch_fields: remove the dispatch fields from the target layer definitions.')
    print(' -prefix_with_layer_name: prefix the target layer name with the source layer name.')
    print(' -dsco KEY=VALUE: dataset creation option. May be repeated.')
    print(' -lco KEY=VALUE: layer creation option. May be repeated.')
    print(' -a_srs srs_def: assign a SRS to the target layers. Source layer SRS is otherwise used.')
    print(' -style_as_field: add a OGR_STYLE field with the content of the feature style string.')
    print(' -where restricted_where: where clause to filter source features.')
    print(' -gt n: group n features per transaction (default 200).')
    print('')
    print('Example :')
    print('  ogr_dispatch.py -src in.dxf -dst out -field Layer -field OGR_GEOMETRY')
    print('')

    return 1

###############################################################################

def EQUAL(a, b):
    return a.lower() == b.lower()

###############################################################
# wkbFlatten()

def wkbFlatten(x):
    return x & (~ogr.wkb25DBit)

###############################################################

class Options:
    def __init__(self):
        self.lco = []
        self.dispatch_fields = []
        self.b25DAs2D = False
        self.bMultiAsSingle = False
        self.bRemoveDispatchFields = False
        self.poOutputSRS = None
        self.bNullifyOutputSRS = False
        self.bPrefixWithLayerName = False
        self.bStyleAsField = False
        self.nGroupTransactions = 200
        self.bQuiet = False

###############################################################
# GeometryTypeToName()

def GeometryTypeToName(eGeomType, options):

    if options.b25DAs2D:
        eGeomType = wkbFlatten(eGeomType)

    if eGeomType == ogr.wkbPoint:
        return 'POINT'
    elif eGeomType == ogr.wkbLineString:
        return 'LINESTRING'
    elif eGeomType == ogr.wkbPolygon:
        return 'POLYGON'
    elif eGeomType == ogr.wkbMultiPoint:
        return 'MULTIPOINT'
    elif eGeomType == ogr.wkbMultiLineString:
        if options.bMultiAsSingle:
            return 'LINESTRING'
        else:
            return 'MULTILINESTRING'
    elif eGeomType == ogr.wkbMultiPolygon:
        if options.bMultiAsSingle:
            return 'POLYGON'
        else:
            return 'MULTIPOLYGON'
    elif eGeomType == ogr.wkbGeometryCollection:
        return 'GEOMETRYCOLLECTION'
    elif eGeomType == ogr.wkbPoint25D:
        return 'POINT25D'
    elif eGeomType == ogr.wkbLineString25D:
        return 'LINESTRING25D'
    elif eGeomType == ogr.wkbPolygon25D:
        return 'POLYGON25D'
    elif eGeomType == ogr.wkbMultiPoint25D:
        return 'MULTIPOINT25D'
    elif eGeomType == ogr.wkbMultiLineString25D:
        if options.bMultiAsSingle:
            return 'LINESTRING25D'
        else:
            return 'MULTILINESTRING25D'
    elif eGeomType == ogr.wkbMultiPolygon25D:
        if options.bMultiAsSingle:
            return 'POLYGON25D'
        else:
            return 'MULTIPOLYGON25D'
    elif eGeomType == ogr.wkbGeometryCollection25D:
        return 'GEOMETRYCOLLECTION25D'
    else:
        # Shouldn't happen
        return 'UNKNOWN'

###############################################################
# get_out_lyr_name()

def get_out_lyr_name(src_lyr, feat, options):
    if options.bPrefixWithLayerName:
        out_lyr_name = src_lyr.GetName()
    else:
        out_lyr_name = ''

    for dispatch_field in options.dispatch_fields:
        if EQUAL(dispatch_field, 'OGR_GEOMETRY'):
            geom = feat.GetGeometryRef()
            if geom is None:
                val = 'NONE'
            else:
                val = GeometryTypeToName(geom.GetGeometryType(), options)
        else:
            if feat.IsFieldSet(dispatch_field):
                val = feat.GetFieldAsString(dispatch_field)
            else:
                val = 'null'

        if out_lyr_name == '':
            out_lyr_name = val
        else:
            out_lyr_name = out_lyr_name + '_' + val

    return out_lyr_name

###############################################################
# get_layer_and_map()

def get_layer_and_map(out_lyr_name, src_lyr, dst_ds, layerMap, geom_type, options):

    if out_lyr_name not in layerMap:
        if options.poOutputSRS is not None or options.bNullifyOutputSRS:
            srs = options.poOutputSRS
        else:
            srs = src_lyr.GetSpatialRef()
        out_lyr = dst_ds.GetLayerByName(out_lyr_name)
        if out_lyr is None:
            if not options.bQuiet:
                print('Creating layer %s' % out_lyr_name)
            out_lyr = dst_ds.CreateLayer(out_lyr_name, srs = srs, \
                                geom_type = geom_type, options = options.lco)
            if out_lyr is None:
                return 1
            src_field_count = src_lyr.GetLayerDefn().GetFieldCount()
            panMap = [ -1 for i in range(src_field_count) ]
            for i in range(src_field_count):
                field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
                if options.bRemoveDispatchFields:
                    found = False
                    for dispatch_field in options.dispatch_fields:
                        if EQUAL(dispatch_field, field_defn.GetName()):
                            found = True
                            break
                    if found:
                        continue
                idx = out_lyr.GetLayerDefn().GetFieldIndex(field_defn.GetName())
                if idx >= 0:
                    panMap[i] = idx
                elif out_lyr.CreateField(field_defn) == 0:
                    panMap[i] = out_lyr.GetLayerDefn().GetFieldCount() - 1
            if options.bStyleAsField:
                out_lyr.CreateField(ogr.FieldDefn('OGR_STYLE', ogr.OFTString))
        else:
            panMap = None
        layerMap[out_lyr_name] = [out_lyr, panMap]
    else:
        out_lyr = layerMap[out_lyr_name][0]
        panMap = layerMap[out_lyr_name][1]

    return (out_lyr, panMap)

###############################################################
# convert_layer()

def convert_layer(src_lyr, dst_ds, layerMap, options):

    current_out_lyr = None
    nFeaturesInTransaction = 0

    for feat in src_lyr:

        out_lyr_name = get_out_lyr_name(src_lyr, feat, options)

        geom = feat.GetGeometryRef()
        if geom is not None:
            geom_type = geom.GetGeometryType()
        else:
            geom_type = ogr.wkbUnknown

        (out_lyr, panMap) = get_layer_and_map(out_lyr_name, src_lyr, dst_ds, \
                                              layerMap, geom_type, options)

        if options.nGroupTransactions > 0:
            if out_lyr != current_out_lyr:
                if current_out_lyr is not None:
                    current_out_lyr.CommitTransaction()
                current_out_lyr = out_lyr
                current_out_lyr.StartTransaction()
                nFeaturesInTransaction = 0

            if nFeaturesInTransaction == options.nGroupTransactions:
                current_out_lyr.CommitTransaction()
                current_out_lyr.StartTransaction()
        else:
            current_out_lyr = out_lyr

        out_feat = ogr.Feature(out_lyr.GetLayerDefn())
        if panMap is not None:
            out_feat.SetFromWithMap( feat, 1, panMap )
        else:
            out_feat.SetFrom(feat)
        if options.bStyleAsField:
            style = feat.GetStyleString()
            if style is not None:
                out_feat.SetField('OGR_STYLE', style)
        out_lyr.CreateFeature(out_feat)

        nFeaturesInTransaction = nFeaturesInTransaction + 1

    if options.nGroupTransactions > 0 and current_out_lyr is not None:
        current_out_lyr.CommitTransaction()

    return 1

###############################################################
# ogr_dispatch()

def ogr_dispatch(argv, progress = None, progress_arg = None):

    src_filename = None
    dst_filename = None
    format = "ESRI Shapefile"
    options = Options()
    dsco = []
    pszWHERE = None

    if len(argv) == 0:
        return Usage()

    i = 0
    while i < len(argv):
        arg = argv[i]
        if EQUAL(arg, '-src') and i+1 < len(argv):
            i = i + 1
            src_filename = argv[i]
        elif EQUAL(arg, '-dst') and i+1 < len(argv):
            i = i + 1
            dst_filename = argv[i]
        elif EQUAL(arg, '-f') and i+1 < len(argv):
            i = i + 1
            format = argv[i]

        elif EQUAL(arg,'-a_srs') and i+1 < len(argv):
            i = i + 1
            pszOutputSRSDef = argv[i]
            if EQUAL(pszOutputSRSDef, "NULL") or \
               EQUAL(pszOutputSRSDef, "NONE"):
                options.bNullifyOutputSRS = True
            else:
                options.poOutputSRS = osr.SpatialReference()
                if options.poOutputSRS.SetFromUserInput( pszOutputSRSDef ) != 0:
                    print( "Failed to process SRS definition: %s" % pszOutputSRSDef )
                    return 1
        elif EQUAL(arg, '-dsco') and i+1 < len(argv):
            i = i + 1
            dsco.append(argv[i])
        elif EQUAL(arg, '-lco') and i+1 < len(argv):
            i = i + 1
            lco.append(argv[i])
        elif EQUAL(arg, '-field') and i+1 < len(argv):
            i = i + 1
            options.dispatch_fields.append(argv[i])
        elif EQUAL(arg, '-25D_as_2D'):
            options.b25DAs2D = True
        elif EQUAL(arg, '-multi_as_single'):
            options.bMultiAsSingle = True
        elif EQUAL(arg, '-remove_dispatch_fields'):
            options.bRemoveDispatchFields = True
        elif EQUAL(arg, '-prefix_with_layer_name'):
            options.bPrefixWithLayerName = True
        elif EQUAL(arg, '-style_as_field'):
            options.bStyleAsField = True
        elif (EQUAL(arg,"-tg") or \
                EQUAL(arg,"-gt"))  and i+1 < len(argv):
            i = i + 1
            options.nGroupTransactions = int(argv[i])
        elif EQUAL(arg,"-where") and i+1 < len(argv):
            i = i + 1
            pszWHERE = argv[i]
        elif EQUAL(arg, '-quiet'):
            options.bQuiet = True
        else:
            print('Unrecognized argument : %s' % arg)
            return Usage()
        i = i + 1

    if src_filename is None:
        print('Missing -src')
        return 1

    if dst_filename is None:
        print('Missing -dst')
        return 1

    if len(options.dispatch_fields) == 0:
        print('Missing -dispatch_field')
        return 1

    src_ds = ogr.Open(src_filename)
    if src_ds is None:
        print('Cannot open source datasource %s' % src_filename)
        return 1

    dst_ds = ogr.Open(dst_filename, update = 1)
    if dst_ds is not None:
        if len(options.dsco) != 0:
            print('-dsco should not be specified for an existing datasource')
            return 1
    else:
        dst_ds = ogr.GetDriverByName(format).CreateDataSource(dst_filename, options = dsco)
    if dst_ds is None:
        print('Cannot open or create target datasource %s' % dst_filename)
        return 1

    layerMap = {}

    for src_lyr in src_ds:
        if pszWHERE is not None:
            src_lyr.SetAttributeFilter(pszWHERE)
        ret = convert_layer(src_lyr, dst_ds, layerMap, options)
        if ret != 0:
            return ret


    return 0

###############################################################
# Entry point

if __name__ == '__main__':
    argv = ogr.GeneralCmdLineProcessor( sys.argv )
    sys.exit(ogr_dispatch(argv[1:]))
