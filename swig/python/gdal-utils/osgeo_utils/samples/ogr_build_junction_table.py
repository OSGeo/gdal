#!/usr/bin/env python3
#
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:
#  Purpose:  Build a junction table from _href fields
#  Author:   Even Rouault, <even dot rouault at spatialys.com>
#
# ******************************************************************************
#  Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
# ******************************************************************************

import sys

from osgeo import gdal, ogr


def Usage():
    print(
        "Usage: ogr_build_junction_table.py [-append|-overwrite] datasource_name [layer_name]"
    )
    print("")
    print(
        "This utility is aimed at creating junction tables for layers coming from GML datasources"
    )
    print("that reference other objects in _href fields")
    print("")
    return 2


def build_junction_table(ds, lyr, ifield, bAppend, bOverwrite):

    first_table = lyr.GetName()
    second_table = lyr.GetLayerDefn().GetFieldDefn(ifield).GetName()[0:-5]
    junction_table_name = first_table + "_" + second_table

    gdal.PushErrorHandler("CPLQuietErrorHandler")
    junction_lyr = ds.GetLayerByName(junction_table_name)
    gdal.PopErrorHandler()
    if junction_lyr is not None:
        if bOverwrite:
            for i in range(ds.GetLayerCount()):
                if ds.GetLayer(i).GetName() == junction_table_name:
                    if ds.DeleteLayer(i) != 0:
                        print(
                            "Cannot delete layer %s for recreation"
                            % junction_table_name
                        )
                        return False
                    else:
                        junction_lyr = None
                    break
        elif not bAppend:
            print("Layer %s already exists" % junction_table_name)
            return False

    if junction_lyr is None:
        junction_lyr = ds.CreateLayer(junction_table_name, geom_type=ogr.wkbNone)
        if junction_lyr is None:
            print("Cannot create layer %s" % junction_table_name)
            return False
        print("Creating layer %s..." % junction_table_name)

        fld_defn = ogr.FieldDefn(first_table + "_gml_id", ogr.OFTString)
        if junction_lyr.CreateField(fld_defn) != 0:
            print("Cannot create field %s" % fld_defn.GetName())
            return False

        fld_defn = ogr.FieldDefn(second_table + "_gml_id", ogr.OFTString)
        if junction_lyr.CreateField(fld_defn) != 0:
            print("Cannot create field %s" % fld_defn.GetName())
            return False

        gdal.PushErrorHandler("CPLQuietErrorHandler")
        ds.ExecuteSQL(
            "CREATE INDEX idx_%s_gml_id ON %s(gml_id)" % (first_table, first_table)
        )
        ds.ExecuteSQL(
            "CREATE INDEX idx_%s_gml_id ON %s(gml_id)" % (second_table, second_table)
        )
        ds.ExecuteSQL(
            "CREATE INDEX idx_%s_gml_id ON %s(%s)"
            % (
                junction_table_name + "_" + first_table,
                junction_table_name,
                first_table + "_gml_id",
            )
        )
        ds.ExecuteSQL(
            "CREATE INDEX idx_%s_gml_id ON %s(%s)"
            % (
                junction_table_name + "_" + second_table,
                junction_table_name,
                second_table + "_gml_id",
            )
        )
        gdal.PopErrorHandler()

    lyr.ResetReading()
    field_type = lyr.GetLayerDefn().GetFieldDefn(ifield).GetType()

    junction_lyr.StartTransaction()
    count_features = 0

    for feat in lyr:
        gml_id = feat.GetFieldAsString("gml_id")
        if field_type == ogr.OFTStringList:
            href_list = feat.GetFieldAsStringList(ifield)
        else:
            href = feat.GetFieldAsString(ifield)
            if href[0] == "(" and href.find(":") > 0 and href[-1] == ")":
                href_list = href[href.find(":") + 1 : -1].split(",")
            else:
                href_list = [href]

        for href in href_list:
            target_feature = ogr.Feature(junction_lyr.GetLayerDefn())
            target_feature.SetField(0, gml_id)
            if href[0] == "#":
                href = href[1:]
            target_feature.SetField(1, href)
            junction_lyr.CreateFeature(target_feature)
            target_feature = None

            count_features = count_features + 1
            if count_features == 200:
                junction_lyr.CommitTransaction()
                junction_lyr.StartTransaction()
                count_features = 0

    junction_lyr.CommitTransaction()

    return True


def process_layer(ds, lyr_name, bAppend, bOverwrite):
    lyr = ds.GetLayerByName(lyr_name)
    if lyr is None:
        print("Cannot find layer %s in datasource" % lyr_name)
        return False
    lyr_defn = lyr.GetLayerDefn()

    if lyr_defn.GetFieldIndex("gml_id") < 0:
        return True

    ret = True
    for ifield in range(lyr_defn.GetFieldCount()):
        if lyr_defn.GetFieldDefn(ifield).GetName().endswith("_href"):
            if not build_junction_table(ds, lyr, ifield, bAppend, bOverwrite):
                ret = False
    return ret


def main(argv=sys.argv):
    argv = ogr.GeneralCmdLineProcessor(argv)

    ds_name = None
    lyr_name = None
    bAppend = False
    bOverwrite = False

    nArgc = len(argv)
    iArg = 1
    while iArg < nArgc:

        if argv[iArg] == "-append":
            bAppend = True

        elif argv[iArg] == "-overwrite":
            bOverwrite = True

        elif argv[iArg][0] == "-":
            return Usage()

        elif ds_name is None:
            ds_name = argv[iArg]

        elif lyr_name is None:
            lyr_name = argv[iArg]

        iArg = iArg + 1

    if ds_name is None:
        return Usage()

    if bAppend and bOverwrite:
        print("Only one of -append or -overwrite can be used")
        return 1

    ds = ogr.Open(ds_name, update=1)
    if ds is None:
        print("Cannot open %s in update mode" % ds_name)
        return 1

    ret = True
    if lyr_name is not None:
        ret = process_layer(ds, lyr_name, bAppend, bOverwrite)
    else:
        for i in range(ds.GetLayerCount()):
            if not process_layer(ds, ds.GetLayer(i).GetName(), bAppend, bOverwrite):
                ret = False
    ds = None

    if ret:
        return 0
    else:
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
