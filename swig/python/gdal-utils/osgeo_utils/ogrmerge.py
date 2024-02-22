#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR samples
# Purpose:  Merge the content of several vector datasets into a single one.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even dot rouault at spatialys dot com>
# Copyright (c) 2021, Idan Miara <idan@miara.com>
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

import glob
import os
import os.path
import sys
from typing import Optional, Sequence

from osgeo import gdal, ogr, osr
from osgeo_utils.auxiliary.base import PathLikeOrStr
from osgeo_utils.auxiliary.util import GetOutputDriverFor


def Usage(isError):
    f = sys.stderr if isError else sys.stdout
    print("Usage: ogrmerge.py [--help] [--help-general]", file=f)
    print("            -o <out_dsname> <src_dsname> [<src_dsname>]...", file=f)
    print("            [-f <format>] [-single] [-nln <layer_name_template>]", file=f)
    print("            [-update | -overwrite_ds] [-append | -overwrite_layer]", file=f)
    print("            [-src_geom_type <geom_type_name>[,<geom_type_name>]...]", file=f)
    print("            [-dsco <NAME>=<VALUE>]... [-lco <NAME>=<VALUE>]...", file=f)
    print("            [-s_srs <srs_def>] [-t_srs <srs_def>|-a_srs <srs_def>]", file=f)
    print("            [-progress] [-skipfailures] [--help-general]", file=f)
    print("", file=f)
    print("Options specific to -single:", file=f)
    print("            [-field_strategy {FirstLayer|Union|Intersection}]", file=f)
    print("            [-src_layer_field_name <name>]", file=f)
    print("            [-src_layer_field_content <layer_name_template>]", file=f)
    print("", file=f)
    print(
        "* layer_name_template can contain the following substituable " "variables:",
        file=f,
    )
    print(
        "     {AUTO_NAME}  : {DS_BASENAME}_{LAYER_NAME} if they are " "different",
        file=f,
    )
    print("                    or {LAYER_NAME} if they are identical", file=f)
    print("     {DS_NAME}    : name of the source dataset", file=f)
    print("     {DS_BASENAME}: base name of the source dataset", file=f)
    print("     {DS_INDEX}   : index of the source dataset", file=f)
    print("     {LAYER_NAME} : name of the source layer", file=f)
    print("     {LAYER_INDEX}: index of the source layer", file=f)

    return 2 if isError else 0


#############################################################################


def _VSIFPrintfL(f, s):
    gdal.VSIFWriteL(s, 1, len(s), f)


#############################################################################


def EQUAL(x, y):
    return x.lower() == y.lower()


#############################################################################


def _GetGeomType(src_geom_type_name):
    if EQUAL(src_geom_type_name, "GEOMETRY"):
        return ogr.wkbUnknown
    try:
        max_geom_type = ogr.wkbTriangle
    except Exception:
        # GDAL 2.1 compat
        max_geom_type = ogr.wkbSurface
    for i in range(max_geom_type + 1):
        if EQUAL(src_geom_type_name, ogr.GeometryTypeToName(i).replace(" ", "")):
            return i
    return None


#############################################################################


def _Esc(x):
    return gdal.EscapeString(x, gdal.CPLES_XML).decode("UTF-8")


class XMLWriter(object):
    def __init__(self, f):
        self.f = f
        self.inc = 0
        self.elements = []

    def _indent(self):
        return "  " * self.inc

    def open_element(self, name, attrs=None):
        xml_attrs = ""
        if attrs is not None:
            for key in attrs:
                xml_attrs = xml_attrs + ' %s="%s"' % (
                    key,
                    _Esc(attrs[key].encode("utf-8")),
                )
        x = "%s<%s%s>\n" % (self._indent(), name, xml_attrs)
        x = x.encode("utf-8")
        _VSIFPrintfL(self.f, x)
        self.inc = self.inc + 1
        self.elements.append(name)

    def write_element_value(self, name, value, attrs=None):
        xml_attrs = ""
        if attrs is not None:
            for key in attrs:
                xml_attrs = xml_attrs + ' %s="%s"' % (
                    key,
                    _Esc(attrs[key].encode("utf-8")),
                )
        x = "%s<%s%s>%s</%s>\n" % (
            self._indent(),
            name,
            xml_attrs,
            _Esc(value.encode("utf-8")),
            name,
        )
        x = x.encode("utf-8")
        _VSIFPrintfL(self.f, x)

    def close_element(self, closing_name=None):
        self.inc = self.inc - 1
        name = self.elements[-1]
        if closing_name is not None:
            assert name == closing_name
        self.elements = self.elements[0:-1]
        _VSIFPrintfL(self.f, "%s</%s>\n" % (self._indent(), name))


###############################################################
# process()


def process(argv, progress=None, progress_arg=None):

    if not argv:
        return Usage(isError=True)

    dst_filename = None
    driver_name = None
    src_datasets = []
    overwrite_ds = False
    overwrite_layer = False
    update = False
    append = False
    single_layer = False
    layer_name_template = None
    skip_failures = False
    src_geom_types = []
    field_strategy = None
    src_layer_field_name = None
    src_layer_field_content = None
    a_srs = None
    s_srs = None
    t_srs = None
    dsco = []
    lco = []
    # WARNING: if adding a new option, make sure to update _gpkg_ogrmerge()
    # optimized code path, or use the general case.

    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == "--help":
            return Usage(isError=False)
        elif (arg == "-f" or arg == "-of") and i + 1 < len(argv):
            i = i + 1
            driver_name = argv[i]
        elif arg == "-o" and i + 1 < len(argv):
            i = i + 1
            dst_filename = argv[i]
        elif arg == "-progress":
            progress = ogr.TermProgress_nocb
            progress_arg = None
        elif arg == "-q" or arg == "-quiet":
            pass
        elif arg[0:5] == "-skip":
            skip_failures = True
        elif arg == "-update":
            update = True
        elif arg == "-overwrite_ds":
            overwrite_ds = True
        elif arg == "-overwrite_layer":
            overwrite_layer = True
            update = True
        elif arg == "-append":
            append = True
            update = True
        elif arg == "-single":
            single_layer = True
        elif arg == "-a_srs" and i + 1 < len(argv):
            i = i + 1
            a_srs = argv[i]
        elif arg == "-s_srs" and i + 1 < len(argv):
            i = i + 1
            s_srs = argv[i]
        elif arg == "-t_srs" and i + 1 < len(argv):
            i = i + 1
            t_srs = argv[i]
        elif arg == "-nln" and i + 1 < len(argv):
            i = i + 1
            layer_name_template = argv[i]
        elif arg == "-field_strategy" and i + 1 < len(argv):
            i = i + 1
            field_strategy = argv[i]
        elif arg == "-src_layer_field_name" and i + 1 < len(argv):
            i = i + 1
            src_layer_field_name = argv[i]
        elif arg == "-src_layer_field_content" and i + 1 < len(argv):
            i = i + 1
            src_layer_field_content = argv[i]
        elif arg == "-dsco" and i + 1 < len(argv):
            i = i + 1
            dsco.append(argv[i])
        elif arg == "-lco" and i + 1 < len(argv):
            i = i + 1
            lco.append(argv[i])
        elif arg == "-src_geom_type" and i + 1 < len(argv):
            i = i + 1
            src_geom_type_names = argv[i].split(",")
            for src_geom_type_name in src_geom_type_names:
                src_geom_type = _GetGeomType(src_geom_type_name)
                if src_geom_type is None:
                    print(
                        "ERROR: Unrecognized geometry type: %s" % src_geom_type_name,
                        file=sys.stderr,
                    )
                    return 1
                src_geom_types.append(src_geom_type)
        elif arg[0] == "-":
            print("ERROR: Unrecognized argument : %s" % arg, file=sys.stderr)
            return Usage(isError=True)
        else:
            if "*" in arg:
                src_datasets += glob.glob(arg)
            else:
                src_datasets.append(arg)
        i = i + 1

    if dst_filename is None:
        print("Missing -o", file=sys.stderr)
        return 1

    return ogrmerge(
        src_datasets=src_datasets,
        dst_filename=dst_filename,
        driver_name=driver_name,
        overwrite_ds=overwrite_ds,
        overwrite_layer=overwrite_layer,
        update=update,
        append=append,
        single_layer=single_layer,
        layer_name_template=layer_name_template,
        skip_failures=skip_failures,
        src_geom_types=src_geom_types,
        field_strategy=field_strategy,
        src_layer_field_name=src_layer_field_name,
        src_layer_field_content=src_layer_field_content,
        a_srs=a_srs,
        s_srs=s_srs,
        t_srs=t_srs,
        dsco=dsco,
        lco=lco,
        progress_callback=progress,
        progress_arg=progress_arg,
    )


#############################################################################


def _build_layer_name_non_single_mode(
    layer_name_template,
    src_ds_idx,
    src_dsname,
    src_lyr_idx,
    src_lyr_name,
    skip_failures,
):
    layer_name = layer_name_template
    basename = None
    if os.path.exists(src_dsname):
        basename = os.path.basename(src_dsname)
        if "." in basename:
            basename = ".".join(basename.split(".")[0:-1])

    if basename == src_lyr_name:
        layer_name = layer_name.replace("{AUTO_NAME}", basename)
    elif basename is None:
        layer_name = layer_name.replace(
            "{AUTO_NAME}", "Dataset%d_%s" % (src_ds_idx, src_lyr_name)
        )
    else:
        layer_name = layer_name.replace("{AUTO_NAME}", basename + "_" + src_lyr_name)

    if basename is not None:
        layer_name = layer_name.replace("{DS_BASENAME}", basename)
    elif "{DS_BASENAME}" in layer_name:
        if skip_failures:
            if "{DS_INDEX}" not in layer_name:
                layer_name = layer_name.replace(
                    "{DS_BASENAME}", "Dataset%d" % src_ds_idx
                )
        else:
            print(
                "ERROR: Layer name template %s "
                "includes {DS_BASENAME} "
                "but %s is not a file" % (layer_name_template, src_dsname),
                file=sys.stderr,
            )
            return None
    layer_name = layer_name.replace("{DS_NAME}", "%s" % src_dsname)
    layer_name = layer_name.replace("{DS_INDEX}", "%d" % src_ds_idx)
    layer_name = layer_name.replace("{LAYER_NAME}", src_lyr_name)
    layer_name = layer_name.replace("{LAYER_INDEX}", "%d" % src_lyr_idx)
    return layer_name


#############################################################################


def _quote_literal(x):
    return x.replace("'", "''")


#############################################################################


def _quote_id(x):
    return x.replace('"', '""')


#############################################################################


def _gpkg_get_src_table_size(src_ds, table_name):
    try:
        # sqlite >= 3.31
        sql_lyr = src_ds.ExecuteSQL(
            "SELECT pgsize FROM temp.stat WHERE name = '%s' AND aggregate = TRUE"
            % _quote_literal(table_name)
        )
    except Exception:
        sql_lyr = src_ds.ExecuteSQL(
            "SELECT SUM(pgsize) FROM temp.stat WHERE name = '%s'"
            % _quote_literal(table_name)
        )
    f = sql_lyr.GetNextFeature()
    src_table_size = f.GetField(0)
    src_ds.ReleaseResultSet(sql_lyr)
    return src_table_size


#############################################################################


def _gpkg_has_spatial_index(ds, lyr):
    has_spatial_index = False
    if lyr.GetGeomType() != ogr.wkbNone:
        sql_lyr = ds.ExecuteSQL(
            "SELECT HasSpatialIndex('%s', '%s')"
            % (_quote_literal(lyr.GetName()), _quote_literal(lyr.GetGeometryColumn()))
        )
        f = sql_lyr.GetNextFeature()
        has_spatial_index = f.GetField(0)
        ds.ReleaseResultSet(sql_lyr)
    return has_spatial_index


#############################################################################
# Estimate the final .gpkg file size from the contributing source layers


def _gpkg_get_estimated_final_size(
    src_datasets, src_geom_types, can_reuse_spatial_index
):

    estimated_final_size = 0

    for src_dsname in src_datasets:
        src_ds = ogr.Open(src_dsname)
        if src_ds is None:
            continue

        dbstat_available = False
        try:
            src_ds.ExecuteSQL("CREATE VIRTUAL TABLE temp.stat USING dbstat(main)")
            dbstat_available = True
        except Exception:
            pass

        use_src_file_size = True
        if dbstat_available:
            for src_lyr in src_ds:
                if src_geom_types:
                    gt = ogr.GT_Flatten(src_lyr.GetGeomType())
                    if gt not in src_geom_types:
                        use_src_file_size = False
                        break

                if not can_reuse_spatial_index and _gpkg_has_spatial_index(
                    src_ds, src_lyr
                ):
                    use_src_file_size = False
                    break

        if use_src_file_size:
            # Wrong if not all layers are selected...
            estimated_final_size += gdal.VSIStatL(src_dsname).size
        else:
            for src_lyr in src_ds:
                if src_geom_types:
                    gt = ogr.GT_Flatten(src_lyr.GetGeomType())
                    if gt not in src_geom_types:
                        continue

                estimated_final_size += _gpkg_get_src_table_size(
                    src_ds, src_lyr.GetName()
                )
                if can_reuse_spatial_index and _gpkg_has_spatial_index(src_ds, src_lyr):
                    src_rtree_prefix = "rtree_%s_%s" % (
                        src_lyr.GetName(),
                        src_lyr.GetGeometryColumn(),
                    )
                    estimated_final_size += _gpkg_get_src_table_size(
                        src_ds, src_rtree_prefix + "_node"
                    )
                    estimated_final_size += _gpkg_get_src_table_size(
                        src_ds, src_rtree_prefix + "_rowid"
                    )
                    estimated_final_size += _gpkg_get_src_table_size(
                        src_ds, src_rtree_prefix + "_parent"
                    )

    return estimated_final_size


#############################################################################


def _gpkg_get_srs_id(ds, lyr):
    sql = (
        "SELECT srs_id FROM gpkg_geometry_columns WHERE table_name = '%s'"
        % _quote_literal(lyr.GetName())
    )
    sql_lyr = ds.ExecuteSQL(sql)
    f = sql_lyr.GetNextFeature()
    srs_id = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)
    return srs_id


#############################################################################
# Optimized implementation of general case for geopackage output, that can be used only:
# - in non-single mode
# - when all sources are geopackages
# - for a newly created dataset


def _gpkg_ogrmerge(
    src_datasets: Optional[Sequence[str]] = None,
    dst_filename: Optional[PathLikeOrStr] = None,
    driver_name: Optional[str] = None,
    layer_name_template: Optional[str] = None,
    skip_failures: bool = False,
    src_geom_types: Optional[Sequence[int]] = None,
    a_srs: Optional[str] = None,
    s_srs: Optional[str] = None,
    t_srs: Optional[str] = None,
    dsco: Optional[Sequence[str]] = None,
    lco: Optional[Sequence[str]] = None,
    progress_callback: Optional = None,
    progress_arg: Optional = None,
):

    driver_name = "GPKG"
    drv = gdal.GetDriverByName(driver_name)
    if drv is None:
        print("ERROR: Invalid driver: %s" % driver_name, file=sys.stderr)
        return 1
    dst_ds = drv.Create(dst_filename, 0, 0, 0, gdal.GDT_Unknown, dsco)
    if dst_ds is None:
        return 1

    ogr.UseExceptions()
    gdal.UseExceptions()
    osr.UseExceptions()

    class ThreadedProgress:
        def __init__(self, dst_filename, estimated_final_size):
            self.stop_thread = False

            import time
            from threading import Thread

            def myfunc():
                while not self.stop_thread:
                    dst_file_size = gdal.VSIStatL(dst_filename).size
                    pct = min(1.0, dst_file_size / estimated_final_size)
                    progress_callback(pct, "", progress_arg)
                    time.sleep(0.1)

            t = Thread(target=myfunc)
            t.start()

        def stop(self):
            self.stop_thread = True

    create_spatial_index = "SPATIAL_INDEX=NO" not in [x.upper() for x in lco]
    can_reuse_spatial_index = t_srs is None and create_spatial_index

    estimated_final_size = 0
    if progress_callback:
        estimated_final_size = _gpkg_get_estimated_final_size(
            src_datasets, src_geom_types, can_reuse_spatial_index
        )

    for src_ds_idx, src_dsname in enumerate(src_datasets):
        src_ds = ogr.Open(src_dsname)
        if src_ds is None:
            print("ERROR: Cannot open %s" % src_dsname, file=sys.stderr)
            if skip_failures:
                continue
            return 1

        for src_lyr_idx, src_lyr in enumerate(src_ds):

            if src_geom_types:
                gt = ogr.GT_Flatten(src_lyr.GetGeomType())
                if gt not in src_geom_types:
                    continue

            has_geom = src_lyr.GetGeomType() != ogr.wkbNone
            if has_geom and not any(
                opt.upper().startswith("GEOMETRY_NAME=") for opt in lco
            ):
                modified_lco = ["GEOMETRY_NAME=" + src_lyr.GetGeometryColumn()] + lco
            else:
                modified_lco = lco

            if t_srs and has_geom:
                srs = osr.SpatialReference()
                srs.SetFromUserInput(t_srs)
            elif a_srs and has_geom:
                srs = osr.SpatialReference()
                srs.SetFromUserInput(a_srs)
            else:
                srs = src_lyr.GetSpatialRef()

            layer_name = _build_layer_name_non_single_mode(
                layer_name_template,
                src_ds_idx,
                src_dsname,
                src_lyr_idx,
                src_lyr.GetName(),
                skip_failures,
            )
            if layer_name is None:
                return 1

            lyr = dst_ds.CreateLayer(
                layer_name,
                geom_type=src_lyr.GetGeomType(),
                srs=srs,
                options=modified_lco,
            )
            for field_idx in range(src_lyr.GetLayerDefn().GetFieldCount()):
                lyr.CreateField(src_lyr.GetLayerDefn().GetFieldDefn(field_idx))

            md = src_lyr.GetMetadata()
            if md:
                lyr.SetMetadata(md)

            lyr.SyncToDisk()

            if has_geom:
                src_srs_id = _gpkg_get_srs_id(src_ds, src_lyr)
                dst_srs_id = _gpkg_get_srs_id(dst_ds, lyr)
                rtree_prefix = "rtree_%s_%s" % (lyr.GetName(), lyr.GetGeometryColumn())
            else:
                rtree_prefix = "__invalid__"
                src_srs_id = -1
                dst_srs_id = -1

            # Collect triggers that we can safely temporary disable, and drop them
            sql = (
                "SELECT name, sql FROM sqlite_master WHERE type = 'trigger' AND (name LIKE '%s_%%' OR name LIKE 'trigger_insert_feature_count_%s' OR name LIKE 'trigger_delete_feature_count_%s')"
                % (
                    _quote_literal(rtree_prefix),
                    _quote_literal(lyr.GetName()),
                    _quote_literal(lyr.GetName()),
                )
            )
            triggers = []
            sql_lyr = dst_ds.ExecuteSQL(sql)
            for f in sql_lyr:
                trigger_name = f["name"]
                dst_ds.ExecuteSQL('DROP TRIGGER "%s"' % _quote_id(trigger_name))
                triggers.append(f["sql"])
            dst_ds.ReleaseResultSet(sql_lyr)

            def get_normalized_field_names(ds, lyr):
                fields = []
                sql_lyr = ds.ExecuteSQL(
                    "PRAGMA table_info('%s')" % _quote_literal(lyr.GetName())
                )
                for f in sql_lyr:
                    col_name = f["name"]
                    if col_name == lyr.GetFIDColumn():
                        fields.append("__FID__")
                    elif col_name == lyr.GetGeometryColumn():
                        fields.append("__GEOMETRY_COLUMN__")
                    else:
                        fields.append(col_name)
                ds.ReleaseResultSet(sql_lyr)
                return fields

            # Build list of field names to select
            if (
                src_srs_id == dst_srs_id
                and s_srs is None
                and t_srs is None
                and get_normalized_field_names(src_ds, src_lyr)
                == get_normalized_field_names(dst_ds, lyr)
            ):
                # If fields in source and target layers are ordered the same, using * is slightly faster
                # than selecting individual fields
                fields = "*"
            else:
                fields = '"%s"' % _quote_id(src_lyr.GetFIDColumn())
                if has_geom:
                    if src_srs_id == dst_srs_id:
                        fields += ', "%s"' % _quote_id(src_lyr.GetGeometryColumn())
                    elif t_srs:
                        # Reproject
                        if s_srs:
                            s_srs_obj = osr.SpatialReference()
                            s_srs_obj.SetFromUserInput(s_srs)
                            assert s_srs_obj.GetAuthorityName(None) == "EPSG"
                            src_srs_id = int(s_srs_obj.GetAuthorityCode(None))
                            fields += ', ST_Transform(SetSRID("%s", %d), %d)' % (
                                _quote_id(src_lyr.GetGeometryColumn()),
                                src_srs_id,
                                dst_srs_id,
                            )
                        else:
                            fields += ', ST_Transform("%s", %d)' % (
                                _quote_id(src_lyr.GetGeometryColumn()),
                                dst_srs_id,
                            )
                    else:
                        # Just remap the geometry SRID to the one of the destination dataset
                        fields += ', SetSRID("%s", %d)' % (
                            _quote_id(src_lyr.GetGeometryColumn()),
                            dst_srs_id,
                        )
                for field_idx in range(src_lyr.GetLayerDefn().GetFieldCount()):
                    fields += ', "%s"' % _quote_id(
                        src_lyr.GetLayerDefn().GetFieldDefn(field_idx).GetName()
                    )

            dst_ds.ExecuteSQL(
                "ATTACH DATABASE '%s' AS source_db" % _quote_literal(src_dsname)
            )

            threaded_progress = None
            if progress_callback:
                threaded_progress = ThreadedProgress(dst_filename, estimated_final_size)

            # Copy features
            try:
                dst_ds.ExecuteSQL(
                    'INSERT INTO "%s" SELECT %s FROM source_db."%s"'
                    % (_quote_id(lyr.GetName()), fields, _quote_id(src_lyr.GetName()))
                )
            finally:
                if threaded_progress:
                    threaded_progress.stop()

            # Update gpkg_ogr_contents
            sql_lyr = dst_ds.ExecuteSQL("SELECT changes()")
            f = sql_lyr.GetNextFeature()
            num_rows_inserted = f.GetField(0)
            f = None
            dst_ds.ReleaseResultSet(sql_lyr)
            src_feature_count = src_lyr.GetFeatureCount(force=0)
            if src_feature_count >= 0 and num_rows_inserted != src_feature_count:
                print(
                    "Warning: %d rows inserted into %s whereas %d expected"
                    % (num_rows_inserted, lyr.GetName(), src_feature_count),
                    file=sys.stderr,
                )
            dst_ds.ExecuteSQL(
                "INSERT OR REPLACE INTO gpkg_ogr_contents VALUES('%s',%d)"
                % (_quote_literal(lyr.GetName()), num_rows_inserted)
            )

            recreateSpatialIndex = False
            if has_geom and create_spatial_index:
                if can_reuse_spatial_index and _gpkg_has_spatial_index(src_ds, src_lyr):
                    # print("Copying spatial index")

                    src_rtree_prefix = "rtree_%s_%s" % (
                        src_lyr.GetName(),
                        src_lyr.GetGeometryColumn(),
                    )

                    if progress_callback:
                        threaded_progress = ThreadedProgress(
                            dst_filename, estimated_final_size
                        )
                    try:
                        dst_ds.ExecuteSQL(
                            'DELETE FROM "%s_node" ' % _quote_id(rtree_prefix)
                        )
                        dst_ds.ExecuteSQL(
                            'INSERT INTO "%s_node" SELECT * FROM source_db."%s_node"'
                            % (_quote_id(rtree_prefix), _quote_id(src_rtree_prefix))
                        )
                        dst_ds.ExecuteSQL(
                            'INSERT INTO "%s_rowid" SELECT * FROM source_db."%s_rowid"'
                            % (_quote_id(rtree_prefix), _quote_id(src_rtree_prefix))
                        )
                        dst_ds.ExecuteSQL(
                            'INSERT INTO "%s_parent" SELECT * FROM source_db."%s_parent"'
                            % (_quote_id(rtree_prefix), _quote_id(src_rtree_prefix))
                        )
                    finally:
                        if threaded_progress:
                            threaded_progress.stop()

                else:
                    recreateSpatialIndex = True

            dst_ds.ExecuteSQL("DETACH DATABASE source_db")

            # Manually register gpkg_geom_* extensions, if not already done
            # at layer creation time.
            sql_lyr = src_ds.ExecuteSQL(
                "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'gpkg_extensions'"
            )
            has_gpkg_extensions = sql_lyr.GetFeatureCount() == 1
            src_ds.ReleaseResultSet(sql_lyr)
            if has_geom and has_gpkg_extensions:
                sql = (
                    "SELECT extension_name FROM gpkg_extensions WHERE table_name = '%s' AND column_name = '%s' AND extension_name LIKE 'gpkg_geom_%%'"
                    % (
                        _quote_literal(src_lyr.GetName()),
                        _quote_literal(src_lyr.GetGeometryColumn()),
                    )
                )
                sql_lyr = src_ds.ExecuteSQL(sql)
                for f in sql_lyr:
                    geom_type = f.GetField(0)[len("gpkg_geom_") :]
                    dst_ds.ReleaseResultSet(
                        dst_ds.ExecuteSQL(
                            "SELECT RegisterGeometryExtension('%s', '%s', '%s')"
                            % (lyr.GetName(), lyr.GetGeometryColumn(), geom_type)
                        )
                    )
                src_ds.ReleaseResultSet(sql_lyr)

            # Update extent
            if has_geom:
                res = src_lyr.GetExtent(force=1, can_return_null=True)
                if res:
                    minx, maxx, miny, maxy = res
                    sql = (
                        "UPDATE gpkg_contents SET min_x=%.18g, min_y=%.18g, max_x=%.18g, max_y=%.18g WHERE table_name = '%s'"
                        % (minx, miny, maxx, maxy, _quote_literal(lyr.GetName()))
                    )
                    dst_ds.ExecuteSQL(sql)

            # Re-install triggers
            for sql in triggers:
                dst_ds.ExecuteSQL(sql)

            if recreateSpatialIndex:
                # print("Recreating spatial index")
                dst_ds.ReleaseResultSet(
                    dst_ds.ExecuteSQL(
                        "SELECT DisableSpatialIndex('%s', '%s')"
                        % (
                            _quote_literal(lyr.GetName()),
                            _quote_literal(lyr.GetGeometryColumn()),
                        )
                    )
                )
                dst_ds.ReleaseResultSet(
                    dst_ds.ExecuteSQL(
                        "SELECT CreateSpatialIndex('%s', '%s')"
                        % (
                            _quote_literal(lyr.GetName()),
                            _quote_literal(lyr.GetGeometryColumn()),
                        )
                    )
                )

    if progress_callback:
        progress_callback(1.0, "", progress_arg)

    return 0


def ogrmerge(
    src_datasets: Optional[Sequence[str]] = None,
    dst_filename: Optional[PathLikeOrStr] = None,
    driver_name: Optional[str] = None,
    overwrite_ds: bool = False,
    overwrite_layer: bool = False,
    update: bool = False,
    append: bool = False,
    single_layer: bool = False,
    layer_name_template: Optional[str] = None,
    skip_failures: bool = False,
    src_geom_types: Optional[Sequence[int]] = None,
    field_strategy: Optional[str] = None,
    src_layer_field_name: Optional[str] = None,
    src_layer_field_content: Optional[str] = None,
    a_srs: Optional[str] = None,
    s_srs: Optional[str] = None,
    t_srs: Optional[str] = None,
    dsco: Optional[Sequence[str]] = None,
    lco: Optional[Sequence[str]] = None,
    progress_callback: Optional = None,
    progress_arg: Optional = None,
):

    src_datasets = src_datasets or []
    src_geom_types = src_geom_types or []
    dsco = dsco or []
    lco = lco or []
    if update:
        if driver_name is not None:
            print("ERROR: -f incompatible with -update", file=sys.stderr)
            return 1
        if dsco:
            print("ERROR: -dsco incompatible with -update", file=sys.stderr)
            return 1
        driver_name = ""
    else:
        if driver_name is None:
            driver_name = GetOutputDriverFor(dst_filename, is_raster=False)

    if src_layer_field_content is None:
        src_layer_field_content = "{AUTO_NAME}"
    elif src_layer_field_name is None:
        src_layer_field_name = "source_ds_lyr"

    if (
        not single_layer
        and driver_name == "ESRI Shapefile"
        and dst_filename.lower().endswith(".shp")
    ):
        print(
            "ERROR: Non-single layer mode incompatible with non-directory "
            "shapefile output",
            file=sys.stderr,
        )
        return 1

    if not src_datasets:
        print("ERROR: No source datasets", file=sys.stderr)
        return 1

    if layer_name_template is None:
        if single_layer:
            layer_name_template = "merged"
        else:
            layer_name_template = "{AUTO_NAME}"

    def get_vector_file_in_update_no_exception(filename):
        with gdal.ExceptionMgr(useExceptions=False), gdal.quiet_errors():
            return gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)

    if (
        not single_layer
        and EQUAL(driver_name, "GPKG")
        and get_vector_file_in_update_no_exception(dst_filename) is None
        and EQUAL(gdal.GetConfigOption("OGR_MERGE_ENABLE_GPKG_OPTIM", "YES"), "YES")
    ):

        def are_sources_gpkg():
            for src_dsname in src_datasets:
                if src_dsname.lower().endswith(".gpkg"):
                    src_ds = ogr.Open(src_dsname)
                    if src_ds is None:
                        return False
                    for src_lyr in src_ds:
                        if src_lyr.GetLayerDefn().GetGeomFieldCount() > 1:
                            # shouldn't happen for now...
                            print(
                                "Code is not ready for multi-geometry column GPKG",
                                file=sys.stderr,
                            )
                            return False
                else:
                    return False
            return True

        compat_of_gpkg_optim = are_sources_gpkg()
        if compat_of_gpkg_optim:
            if s_srs:
                s_srs_obj = osr.SpatialReference()
                s_srs_obj.SetFromUserInput(s_srs)
                if s_srs_obj.GetAuthorityName(None) != "EPSG":
                    compat_of_gpkg_optim = False

        if compat_of_gpkg_optim:
            return _gpkg_ogrmerge(
                src_datasets,
                dst_filename,
                driver_name,
                layer_name_template,
                skip_failures,
                src_geom_types,
                a_srs,
                s_srs,
                t_srs,
                dsco,
                lco,
                progress_callback,
                progress_arg,
            )

    vrt_filename = None
    if not EQUAL(driver_name, "VRT"):
        dst_ds = get_vector_file_in_update_no_exception(dst_filename)
        if dst_ds is not None:
            if not update and not overwrite_ds:
                print(
                    "ERROR: Destination dataset already exists, "
                    + "but -update nor -overwrite_ds are specified",
                    file=sys.stderr,
                )
                return 1
            if overwrite_ds:
                drv = dst_ds.GetDriver()
                dst_ds = None
                if drv.GetDescription() == "OGR_VRT":
                    # We don't want to destroy the sources of the VRT
                    gdal.Unlink(dst_filename)
                else:
                    drv.Delete(dst_filename)
        elif update:
            print("ERROR: Destination dataset does not exist", file=sys.stderr)
            return 1
        if dst_ds is None:
            drv = gdal.GetDriverByName(driver_name)
            if drv is None:
                print("ERROR: Invalid driver: %s" % driver_name, file=sys.stderr)
                return 1
            dst_ds = drv.Create(dst_filename, 0, 0, 0, gdal.GDT_Unknown, dsco)
            if dst_ds is None:
                return 1

        vrt_filename = "/vsimem/_ogrmerge_.vrt"
    else:
        if gdal.VSIStatL(dst_filename) and not overwrite_ds:
            print(
                "ERROR: Destination dataset already exists, "
                + "but -overwrite_ds are specified",
                file=sys.stderr,
            )
            return 1
        vrt_filename = dst_filename

    f = gdal.VSIFOpenL(vrt_filename, "wb")
    if f is None:
        print("ERROR: Cannot create %s" % vrt_filename, file=sys.stderr)
        return 1

    writer = XMLWriter(f)
    writer.open_element("OGRVRTDataSource")

    if single_layer:

        ogr_vrt_union_layer_written = False

        for src_ds_idx, src_dsname in enumerate(src_datasets):
            src_ds = ogr.Open(src_dsname)
            if src_ds is None:
                print("ERROR: Cannot open %s" % src_dsname, file=sys.stderr)
                if skip_failures:
                    continue
                gdal.VSIFCloseL(f)
                gdal.Unlink(vrt_filename)
                return 1
            for src_lyr_idx, src_lyr in enumerate(src_ds):
                if src_geom_types:
                    gt = ogr.GT_Flatten(src_lyr.GetGeomType())
                    if gt not in src_geom_types:
                        continue

                if not ogr_vrt_union_layer_written:
                    ogr_vrt_union_layer_written = True
                    writer.open_element(
                        "OGRVRTUnionLayer", attrs={"name": layer_name_template}
                    )

                    if src_layer_field_name is not None:
                        writer.write_element_value(
                            "SourceLayerFieldName", src_layer_field_name
                        )

                    if field_strategy is not None:
                        writer.write_element_value("FieldStrategy", field_strategy)

                layer_name = src_layer_field_content

                src_lyr_name = src_lyr.GetName()
                try:
                    src_lyr_name = src_lyr_name.decode("utf-8")
                except AttributeError:
                    pass

                basename = None
                if os.path.exists(src_dsname):
                    basename = os.path.basename(src_dsname)
                    if "." in basename:
                        basename = ".".join(basename.split(".")[0:-1])

                if basename == src_lyr_name:
                    layer_name = layer_name.replace("{AUTO_NAME}", basename)
                elif basename is None:
                    layer_name = layer_name.replace(
                        "{AUTO_NAME}", "Dataset%d_%s" % (src_ds_idx, src_lyr_name)
                    )
                else:
                    layer_name = layer_name.replace(
                        "{AUTO_NAME}", basename + "_" + src_lyr_name
                    )

                if basename is not None:
                    layer_name = layer_name.replace("{DS_BASENAME}", basename)
                else:
                    layer_name = layer_name.replace("{DS_BASENAME}", src_dsname)
                layer_name = layer_name.replace("{DS_NAME}", "%s" % src_dsname)
                layer_name = layer_name.replace("{DS_INDEX}", "%d" % src_ds_idx)
                layer_name = layer_name.replace("{LAYER_NAME}", src_lyr_name)
                layer_name = layer_name.replace("{LAYER_INDEX}", "%d" % src_lyr_idx)

                if t_srs is not None:
                    writer.open_element("OGRVRTWarpedLayer")

                writer.open_element("OGRVRTLayer", attrs={"name": layer_name})
                attrs = {}
                if (
                    EQUAL(driver_name, "VRT")
                    and os.path.exists(src_dsname)
                    and not os.path.isabs(src_dsname)
                    and "/" not in vrt_filename
                    and "\\" not in vrt_filename
                ):
                    attrs["relativeToVRT"] = "1"
                if single_layer:
                    attrs["shared"] = "1"
                writer.write_element_value("SrcDataSource", src_dsname, attrs=attrs)
                writer.write_element_value("SrcLayer", src_lyr.GetName())

                if a_srs is not None:
                    writer.write_element_value("LayerSRS", a_srs)

                writer.close_element("OGRVRTLayer")

                if t_srs is not None:
                    if s_srs is not None:
                        writer.write_element_value("SrcSRS", s_srs)

                    writer.write_element_value("TargetSRS", t_srs)

                    writer.close_element("OGRVRTWarpedLayer")

        if ogr_vrt_union_layer_written:
            writer.close_element("OGRVRTUnionLayer")

    else:

        for src_ds_idx, src_dsname in enumerate(src_datasets):
            src_ds = ogr.Open(src_dsname)
            if src_ds is None:
                print("ERROR: Cannot open %s" % src_dsname, file=sys.stderr)
                if skip_failures:
                    continue
                gdal.VSIFCloseL(f)
                gdal.Unlink(vrt_filename)
                return 1
            for src_lyr_idx, src_lyr in enumerate(src_ds):
                if src_geom_types:
                    gt = ogr.GT_Flatten(src_lyr.GetGeomType())
                    if gt not in src_geom_types:
                        continue

                src_lyr_name = src_lyr.GetName()
                try:
                    src_lyr_name = src_lyr_name.decode("utf-8")
                except AttributeError:
                    pass

                layer_name = _build_layer_name_non_single_mode(
                    layer_name_template,
                    src_ds_idx,
                    src_dsname,
                    src_lyr_idx,
                    src_lyr.GetName(),
                    skip_failures,
                )
                if layer_name is None:
                    gdal.VSIFCloseL(f)
                    gdal.Unlink(vrt_filename)
                    return 1

                if t_srs is not None:
                    writer.open_element("OGRVRTWarpedLayer")

                writer.open_element("OGRVRTLayer", attrs={"name": layer_name})
                attrs = {}
                if (
                    EQUAL(driver_name, "VRT")
                    and os.path.exists(src_dsname)
                    and not os.path.isabs(src_dsname)
                    and "/" not in vrt_filename
                    and "\\" not in vrt_filename
                ):
                    attrs["relativeToVRT"] = "1"
                if single_layer:
                    attrs["shared"] = "1"
                writer.write_element_value("SrcDataSource", src_dsname, attrs=attrs)
                writer.write_element_value("SrcLayer", src_lyr_name)

                if a_srs is not None:
                    writer.write_element_value("LayerSRS", a_srs)

                writer.close_element("OGRVRTLayer")

                if t_srs is not None:
                    if s_srs is not None:
                        writer.write_element_value("SrcSRS", s_srs)

                    writer.write_element_value("TargetSRS", t_srs)

                    writer.close_element("OGRVRTWarpedLayer")

    writer.close_element("OGRVRTDataSource")

    gdal.VSIFCloseL(f)

    ret = 0
    if not EQUAL(driver_name, "VRT"):
        accessMode = None
        if append:
            accessMode = "append"
        elif overwrite_layer:
            accessMode = "overwrite"
        ret = gdal.VectorTranslate(
            dst_ds,
            vrt_filename,
            accessMode=accessMode,
            layerCreationOptions=lco,
            skipFailures=skip_failures,
            callback=progress_callback,
            callback_data=progress_arg,
        )
        if ret == 1:
            ret = 0
        else:
            ret = 1
        gdal.Unlink(vrt_filename)

    return ret


def main(argv=sys.argv):
    argv = ogr.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0
    return process(argv[1:])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
