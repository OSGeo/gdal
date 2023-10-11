# SPDX-License-Identifier: MIT
# Copyright 2023 Even Rouault

import sys

from osgeo import gdal, ogr
from osgeo_utils.auxiliary.util import GetOutputDriverFor

ogr.UseExceptions()


def copy_layer(src_lyr, out_filename, out_format, lcos={}):
    stream = src_lyr.GetArrowStream()
    schema = stream.GetSchema()

    # If the source layer has a FID column and the output driver supports
    # a FID layer creation option, set it to the source FID column name.
    if src_lyr.GetFIDColumn():
        creationOptions = gdal.GetDriverByName(out_format).GetMetadataItem(
            "DS_LAYER_CREATIONOPTIONLIST"
        )
        if creationOptions and '"FID"' in creationOptions:
            lcos["FID"] = src_lyr.GetFIDColumn()

    with ogr.GetDriverByName(out_format).CreateDataSource(out_filename) as out_ds:
        if src_lyr.GetLayerDefn().GetGeomFieldCount() > 1:
            out_lyr = out_ds.CreateLayer(
                src_lyr.GetName(), geom_type=ogr.wkbNone, options=lcos
            )
            for i in range(src_lyr.GetLayerDefn().GetGeomFieldCount()):
                out_lyr.CreateGeomField(src_lyr.GetLayerDefn().GetGeomFieldDefn(i))
        else:
            out_lyr = out_ds.CreateLayer(
                src_lyr.GetName(),
                geom_type=src_lyr.GetGeomType(),
                srs=src_lyr.GetSpatialRef(),
                options=lcos,
            )

        success, error_msg = out_lyr.IsArrowSchemaSupported(schema)
        assert success, error_msg

        src_geom_field_names = [
            src_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName()
            for i in range(src_lyr.GetLayerDefn().GetGeomFieldCount())
        ]
        for i in range(schema.GetChildrenCount()):
            # GetArrowStream() may return "OGC_FID" for a unnamed source FID
            # column and "wkb_geometry" for a unnamed source geometry column.
            # Also test GetFIDColumn() and src_geom_field_names if they are
            # named.
            if (
                schema.GetChild(i).GetName()
                not in ("OGC_FID", "wkb_geometry", src_lyr.GetFIDColumn())
                and schema.GetChild(i).GetName() not in src_geom_field_names
            ):
                out_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

        write_options = []
        if src_lyr.GetFIDColumn():
            write_options.append("FID=" + src_lyr.GetFIDColumn())
        if (
            src_lyr.GetLayerDefn().GetGeomFieldCount() == 1
            and src_lyr.GetGeometryColumn()
        ):
            write_options.append("GEOMETRY_NAME=" + src_lyr.GetGeometryColumn())

        while True:
            array = stream.GetNextRecordBatch()
            if array is None:
                break
            out_lyr.WriteArrowBatch(schema, array, write_options)


def Usage():
    print("ogr2ogr_arrow.py [-spat <xmin> <ymin> <xmax> <ymax>] [-where <cond>]")
    print("                 [-f <format>] [-lco <NAME>=<VALUE>]...")
    print("                 <out_filename> <src_filename> [<layer_name>]")
    sys.exit(1)


if __name__ == "__main__":

    i = 1
    driver_name = None
    out_filename = None
    filename = None
    where = None
    minx = None
    miny = None
    maxx = None
    maxy = None
    layer_name = None
    lcos = {}
    while i < len(sys.argv):
        if sys.argv[i] == "-spat":
            minx = float(sys.argv[i + 1])
            miny = float(sys.argv[i + 2])
            maxx = float(sys.argv[i + 3])
            maxy = float(sys.argv[i + 4])
            i += 4
        elif sys.argv[i] == "-where":
            where = sys.argv[i + 1]
            i += 1
        elif sys.argv[i] == "-f":
            driver_name = sys.argv[i + 1]
            i += 1
        elif sys.argv[i] == "-lco":
            key, value = sys.argv[i + 1].split("=")
            lcos[key] = value
            i += 1
        elif sys.argv[i][0] == "-":
            Usage()
        elif out_filename is None:
            out_filename = sys.argv[i]
        elif filename is None:
            filename = sys.argv[i]
        elif layer_name is None:
            layer_name = sys.argv[i]
        else:
            Usage()
        i += 1

    if not filename:
        Usage()
    if not driver_name:
        driver_name = GetOutputDriverFor(out_filename, is_raster=False)

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(layer_name if layer_name else 0)
    if minx:
        lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
    if where:
        lyr.SetAttributeFilter(where)

    copy_layer(lyr, out_filename, driver_name, lcos)
