#!/usr/bin/env python3
###############################################################################
# $Id$
#
# Project:  OGR Python samples
# Purpose:  Load ODBC table to an ODBC datastore.  Uses direct SQL
#           since the ODBC driver is read-only for OGR.
# Author:   Frank Warmerdam, warmerdam@pobox.com
#
###############################################################################
# Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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

import sys

from osgeo import ogr

#############################################################################


def Usage():
    print('Usage: load2odbc.py [-where attr_filter] infile odbc_dsn layer')
    print('')
    return 1


def main(argv):
    extents_flag = 1
    infile = None
    odbc_dsn = None
    layername = None
    attr_filter = None

    i = 1
    while i < len(argv):
        if argv[i] == '-where':
            i = i + 1
            attr_filter = argv[i]
        elif infile is None:
            infile = argv[i]
        elif odbc_dsn is None:
            odbc_dsn = argv[i]
        elif layername is None:
            layername = argv[i]
        else:
            return Usage()

        i = i + 1

    if layername is None:
        return Usage()

    #############################################################################
    # Open the datasource to operate on.

    in_ds = ogr.Open(infile, update=0)

    in_layer = in_ds.GetLayerByName(layername)

    if in_layer is None:
        print('Did not find layer: ', layername)
        return 1

    if attr_filter is not None:
        in_layer.SetAttributeFilter(attr_filter)

    #############################################################################
    # Connect to ODBC DSN.

    if odbc_dsn == 'stdout':
        out_ds = None
    else:
        if len(odbc_dsn) < 6 or odbc_dsn[:5] != 'ODBC:':
            odbc_dsn = 'ODBC:' + odbc_dsn

        out_ds = ogr.Open(odbc_dsn)

        if out_ds is None:
            print('Unable to connect to ' + odbc_dsn)
            return 1

    #############################################################################
    # Fetch layer definition, and defined output table on the same basis.

    try:
        cmd = 'drop table ' + layername
        if out_ds is None:
            print(cmd)
        else:
            out_ds.ExecuteSQL(cmd)
    except:
        pass

    defn = in_layer.GetLayerDefn()

    cmd = 'CREATE TABLE ' + layername + '( OGC_FID INTEGER, WKT_GEOMETRY MEMO'

    if extents_flag:
        cmd = cmd + ', XMIN NUMBER, YMIN NUMBER, XMAX NUMBER, YMAX NUMBER'

    for iField in range(defn.GetFieldCount()):
        fielddef = defn.GetFieldDefn(iField)
        cmd = cmd + ', ' + fielddef.GetName()
        if fielddef.GetType() == ogr.OFTInteger:
            cmd = cmd + ' INTEGER'
        elif fielddef.GetType() == ogr.OFTString:
            cmd = cmd + ' TEXT'
        elif fielddef.GetType() == ogr.OFTReal:
            cmd = cmd + ' NUMBER'
        else:
            cmd = cmd + ' TEXT'

    cmd = cmd + ')'

    if out_ds is None:
        print(cmd)
    else:
        print('ExecuteSQL: ', cmd)
        result = out_ds.ExecuteSQL(cmd)
        if result is not None:
            out_ds.ReleaseResultSet(result)

    #############################################################################
    # Read all features in the line layer, holding just the geometry in a hash
    # for fast lookup by TLID.

    in_layer.ResetReading()
    feat = in_layer.GetNextFeature()
    while feat is not None:
        cmd_start = 'INSERT INTO ' + layername + ' ( OGC_FID '
        cmd_end = ') VALUES (%d' % feat.GetFID()

        geom = feat.GetGeometryRef()
        if geom is not None:
            cmd_start = cmd_start + ', WKT_GEOMETRY'
            cmd_end = cmd_end + ", '" + geom.ExportToWkt() + "'"

        if extents_flag and geom is not None:
            extent = geom.GetEnvelope()
            cmd_start = cmd_start + ', XMIN, XMAX, YMIN, YMAX'
            cmd_end = cmd_end + (', %.7f, %.7f, %.7f, %.7f' % extent)

        for iField in range(defn.GetFieldCount()):
            fielddef = defn.GetFieldDefn(iField)
            if feat.IsFieldSet(iField) != 0:
                cmd_start = cmd_start + ', ' + fielddef.GetName()

            if fielddef.GetType() == ogr.OFTInteger:
                cmd_end = cmd_end + ', ' + feat.GetFieldAsString(iField)
            elif fielddef.GetType() == ogr.OFTString:
                cmd_end = cmd_end + ", '" + feat.GetFieldAsString(iField) + "'"
            elif fielddef.GetType() == ogr.OFTReal:
                cmd_end = cmd_end + ', ' + feat.GetFieldAsString(iField)
            else:
                cmd_end = cmd_end + ", '" + feat.GetFieldAsString(iField) + "'"

        cmd = cmd_start + cmd_end + ')'

        if out_ds is None:
            print(cmd)
        else:
            print('ExecuteSQL: ', cmd)
            out_ds.ExecuteSQL(cmd)

        feat.Destroy()
        feat = in_layer.GetNextFeature()

    #############################################################################
    # Cleanup

    in_ds.Destroy()
    if out_ds is not None:
        out_ds.Destroy()

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
