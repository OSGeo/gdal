#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR
# Purpose:  Test compliance of GeoParquet file
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
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

import json
import sys

from osgeo import gdal, ogr, osr

geoparquet_schemas = {}

map_ogr_geom_type_to_geoparquet = {
    ogr.wkbPoint: "Point",
    ogr.wkbLineString: "LineString",
    ogr.wkbPolygon: "Polygon",
    ogr.wkbMultiPoint: "MultiPoint",
    ogr.wkbMultiLineString: "MultiLineString",
    ogr.wkbMultiPolygon: "MultiPolygon",
    ogr.wkbGeometryCollection: "GeometryCollection",
    ogr.wkbPoint25D: "Point",
    ogr.wkbLineString25D: "LineString Z",
    ogr.wkbPolygon25D: "Polygon Z",
    ogr.wkbMultiPoint25D: "MultiPoint Z",
    ogr.wkbMultiLineString25D: "MultiLineString Z",
    ogr.wkbMultiPolygon25D: "MultiPolygon Z",
    ogr.wkbGeometryCollection25D: "GeometryCollection Z",
}

map_remote_resources = {}


class GeoParquetValidator(object):
    def __init__(self, filename, check_data=False, local_schema=None):
        self.filename = filename
        self.check_data = check_data
        self.local_schema = local_schema
        self.ds = None
        self.errors = []

    def check(self):
        with gdal.ExceptionMgr(useExceptions=True), ogr.ExceptionMgr(
            useExceptions=True
        ), osr.ExceptionMgr(useExceptions=True):
            self._check()
            self.ds = None

    def _error(self, msg):
        if len(self.errors) == 100:
            self.errors.append(
                f"{self.filename}: too many errors. No longer emitting ones"
            )
        elif len(self.errors) < 100:
            self.errors.append(f"{self.filename}: {msg}")

    def _check_counterclockwise(self, g, row):
        gt = ogr.GT_Flatten(g.GetGeometryType())
        if gt == ogr.wkbPolygon:
            for idx in range(g.GetGeometryCount()):
                ring = g.GetGeometryRef(idx)
                if idx == 0 and ring.IsClockwise():
                    self._error(
                        f"Exterior ring of geometry at row {row} has invalid orientation"
                    )
                elif idx > 0 and not ring.IsClockwise():
                    self._error(
                        f"Interior ring of geometry at row {row} has invalid orientation"
                    )
        elif gt in (ogr.wkbMultiPolygon, ogr.wkbGeometryCollection):
            for idx in range(g.GetGeometryCount()):
                subgeom = g.GetGeometryRef(idx)
                self._check_counterclockwise(subgeom, row)

    def _validate(self, schema, instance):
        import jsonschema

        if sys.version_info >= (3, 8):
            from importlib.metadata import version

            jsonschema_version = version("jsonschema")
        else:
            from pkg_resources import get_distribution

            jsonschema_version = get_distribution("jsonschema").version

        def versiontuple(v):
            return tuple(map(int, (v.split("."))))

        # jsonschema 4.18 deprecates automatic resolution of "$ref" for security
        # reason. Use a custom retrieve method.
        # Cf https://python-jsonschema.readthedocs.io/en/latest/referencing/#automatically-retrieving-resources-over-http
        if versiontuple(jsonschema_version) >= (4, 18):
            from referencing import Registry, Resource

            def retrieve_remote_file(uri: str):
                if not uri.startswith("http://") and not uri.startswith("https://"):
                    raise Exception(f"Cannot retrieve {uri}")
                import urllib

                global map_remote_resources
                if uri not in map_remote_resources:
                    response = urllib.request.urlopen(uri).read()
                    map_remote_resources[uri] = response
                else:
                    response = map_remote_resources[uri]
                return Resource.from_contents(json.loads(response))

            registry = Registry(retrieve=retrieve_remote_file)
            validator_cls = jsonschema.validators.validator_for(schema)
            validator_cls(schema, registry=registry).validate(instance)

        else:
            # jsonschema < 4.18
            jsonschema.validate(instance=instance, schema=schema)

    def _check(self):

        global geoparquet_schemas

        if gdal.GetDriverByName("Parquet") is None:
            return self._error("Parquet driver not available")

        try:
            import jsonschema

            jsonschema.validate
        except ImportError:
            return self._error(
                "jsonschema Python module not available. Try 'pip install jsonschema'"
            )

        try:
            self.ds = gdal.OpenEx(self.filename, allowed_drivers=["Parquet"])
        except Exception as e:
            return self._error("is not a Parquet file: %s" % str(e))

        lyr = self.ds.GetLayer(0)
        geo = lyr.GetMetadataItem("geo", "_PARQUET_METADATA_")
        if geo is None:
            return self._error("'geo' file metadata item missing")

        try:
            geo_j = json.loads(geo)
        except Exception as e:
            return self._error(
                "'geo' metadata item is not valid JSON. Value of 'geo' = '%s'. Exception = '%s' "
                % (geo, str(e))
            )

        if "version" not in geo_j:
            self._error(
                "'geo' metadata item lacks a 'version' member. Value of 'geo' = '%s'"
                % geo
            )
            if not self.local_schema:
                return

        if self.local_schema:
            schema_j = json.loads(open(self.local_schema, "rb").read())
        else:
            version = geo_j["version"]
            if not isinstance(version, str):
                return self._error(
                    "'geo[\"version\"]' is not a string. Value of 'geo' = '%s'" % geo
                )

            schema_url = f"https://github.com/opengeospatial/geoparquet/releases/download/v{version}/schema.json"
            if schema_url not in geoparquet_schemas:
                import urllib

                try:
                    response = urllib.request.urlopen(schema_url).read()
                except Exception as e:
                    return self._error(
                        f"Cannot download GeoParquet JSON schema from {schema_url}. Exception = {repr(e)}"
                    )

                try:
                    geoparquet_schemas[schema_url] = json.loads(response)
                except Exception as e:
                    return self._error(
                        f"Failed to read GeoParquet schema at {schema_url} as JSON. Schema content = '{response}'. Exception = {repr(e)}"
                    )

            schema_j = geoparquet_schemas[schema_url]

        try:
            self._validate(schema_j, geo_j)
        except Exception as e:
            self._error(
                "'geo' metadata item fails to validate its schema: %s'" % str(e)
            )

        if "primary_column" not in geo_j:
            return self._error('geo["primary_column"] missing')
        primary_column = geo_j["primary_column"]

        if "columns" not in geo_j:
            return self._error('geo["columns"] missing')
        columns = geo_j["columns"]

        if primary_column not in columns:
            self._error(
                f'geo["primary_column"] = {primary_column} is not in listed in geo["columns"'
            )

        ogr_geom_field_names = [
            lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName()
            for i in range(lyr.GetLayerDefn().GetGeomFieldCount())
        ]

        for column_name, column_def in columns.items():
            if column_name not in ogr_geom_field_names:
                self._error(
                    f'geo["columns"] lists a {column_name} column which is not found in the Parquet fields'
                )

            self._check_column_metadata(column_name, column_def)

        if self.check_data:
            self._check_data(lyr, columns)

    def _check_column_metadata(self, column_name, column_def):
        srs = None
        if "crs" in column_def:
            crs = column_def["crs"]
            if crs and (
                osr.GetPROJVersionMajor() * 100 + osr.GetPROJVersionMinor() >= 602
            ):
                srs = osr.SpatialReference()
                try:
                    if isinstance(crs, dict):
                        srs.SetFromUserInput(json.dumps(crs))
                    elif isinstance(crs, str):
                        srs.SetFromUserInput(crs)
                except Exception as e:
                    self._error(f"crs {crs} cannot be parsed: %s" % str(e))
        else:
            srs = osr.SpatialReference()
            srs.ImportFromEPSG(4326)

        if "bbox" in column_def:
            bbox = column_def["bbox"]
            if bbox:
                if len(bbox) == 4:
                    minx_idx = 0
                    miny_idx = 1
                    maxx_idx = 2
                    maxy_idx = 3

                elif len(bbox) == 6:
                    minx_idx = 0
                    miny_idx = 1
                    minz_idx = 2
                    maxx_idx = 3
                    maxy_idx = 4
                    maxz_idx = 5

                    if bbox[maxz_idx] < bbox[minz_idx]:
                        self._error(f"bbox[{maxz_idx}] < bbox[{minz_idx}]")

                else:
                    assert False, "Unexpected len(bbox)"

                if srs and srs.IsGeographic():
                    if abs(bbox[minx_idx]) > 180:
                        self._error(f"abs(bbox[{minx_idx}]) > 180")
                    if abs(bbox[miny_idx]) > 90:
                        self._error(f"abs(bbox[{miny_idx}]) > 90")
                    if abs(bbox[maxx_idx]) > 180:
                        self._error(f"abs(bbox[{maxx_idx}]) > 180")
                    if abs(bbox[maxy_idx]) > 90:
                        self._error(f"abs(bbox[{maxy_idx}]) > 90")
                if srs and srs.IsProjected():
                    if bbox[maxx_idx] < bbox[minx_idx]:
                        self._error(f"bbox[{maxx_idx}] < bbox[{minx_idx}]")

                if bbox[maxy_idx] < bbox[miny_idx]:
                    self._error(f"bbox[{maxy_idx}] < bbox[{miny_idx}]")

            if "geometry_types" in column_def:
                geometry_types = column_def["geometry_types"]
                set_geometry_types = set()
                for geometry_type in geometry_types:
                    if geometry_type in set_geometry_types:
                        self._error(
                            f"{geometry_type} is declared several times in geometry_types[]"
                        )

    def _check_data(self, lyr, columns):
        lyr.SetIgnoredFields(
            [
                lyr.GetLayerDefn().GetFieldDefn(i).GetName()
                for i in range(lyr.GetLayerDefn().GetFieldCount())
            ]
        )

        stream = lyr.GetArrowStreamAsNumPy(options=["USE_MASKED_ARRAYS=NO"])
        row = 0
        for batch in stream:
            row_backup = row
            rows_in_batch = 0
            for column_name, column_def in columns.items():
                if "geometry_types" in column_def:
                    geometry_types = column_def["geometry_types"]
                    set_geometry_types = set(geometry_types)
                else:
                    set_geometry_types = set()

                if "orientation" in column_def:
                    orientation = column_def["orientation"]
                else:
                    orientation = None

                row = row_backup
                rows_in_batch = len(batch[column_name])
                for geom in batch[column_name]:
                    g = None
                    try:
                        if geom is not None:
                            g = ogr.CreateGeometryFromWkb(geom)
                    except Exception as e:
                        self._error(
                            f"Invalid WKB geometry at row {row} for column {column_name}: %s"
                            % str(e)
                        )

                    if g:
                        ogr_geom_type = g.GetGeometryType()
                        if ogr_geom_type not in map_ogr_geom_type_to_geoparquet:
                            self._error(
                                f"Geometry at row {row} is of unexpected type for GeoParquet: %s"
                                % g.GetGeometryName()
                            )
                        elif set_geometry_types:
                            geoparquet_geom_type = map_ogr_geom_type_to_geoparquet[
                                ogr_geom_type
                            ]
                            if geoparquet_geom_type not in set_geometry_types:
                                self._error(
                                    f"Geometry at row {row} is of type {geoparquet_geom_type}, but not listed in geometry_types[]"
                                )

                        # ogr.Geometry.IsClockwise() added in GDAL 3.8
                        if (
                            hasattr(g, "IsClockwise")
                            and orientation == "counterclockwise"
                        ):
                            self._check_counterclockwise(g, row)

                    row += 1

            row += rows_in_batch


def check(filename, check_data=False, local_schema=None):
    """Validate a file against GeoParquet specification

    Parameters
    ----------
    filename:
        File to validate
    check_data:
        Set to True to check geometry content in addition to metadata.
    local_schema:
        Path to local schema (if not specified, it will be retrieved at https://github.com/opengeospatial/geoparquet)

    Returns
    -------
    A list of error messages, or an empty list if no error
    """
    checker = GeoParquetValidator(
        filename, check_data=check_data, local_schema=local_schema
    )
    checker.check()
    return checker.errors


def Usage():
    print(
        "Usage: validate_geoparquet.py [--check-data] [--schema FILENAME] my_geo.parquet"
    )
    print("")
    print("--check-data: validate data in addition to metadata")
    print(
        "--schema: path to GeoParquet JSON schema. If not specified, retrieved from the network"
    )
    return 2


def main(argv=sys.argv):
    filename = None
    if len(argv) == 1:
        return Usage()
    check_data = False
    local_schema = None
    i = 1
    while i < len(argv):
        arg = argv[i]
        if arg == "--check-data":
            check_data = True
        elif arg == "--schema":
            local_schema = argv[i + 1]
            i += 1
        elif arg[0] == "-":
            return Usage()
        else:
            filename = arg

        i += 1

    if filename is None:
        return Usage()
    errors = check(filename, check_data=check_data, local_schema=local_schema)
    if errors:
        for msg in errors:
            print(msg)
        return 1
    return 0


if __name__ == "__main__":
    gdal.UseExceptions()
    sys.exit(main(sys.argv))
