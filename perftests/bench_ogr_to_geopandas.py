# SPDX-License-Identifier: MIT
# Copyright 2022,2023 Even Rouault

import sys

import pyarrow as pa

from osgeo import ogr

ogr.UseExceptions()


def layer_as_geopandas(lyr):

    stream = lyr.GetArrowStreamAsPyArrow()
    schema = stream.schema

    geom_field_name = None
    is_wkb = True
    for field in schema:
        field_md = field.metadata
        arrow_extension_name = None
        if field_md:
            arrow_extension_name = field_md.get(b"ARROW:extension:name", None)
            # print(arrow_extension_name)
        if arrow_extension_name and arrow_extension_name in (b"WKT", b"ogc.wkt"):
            geom_field_name = field.name
            is_wkb = False
            break
        elif arrow_extension_name and arrow_extension_name in (b"WKB", b"ogc.wkb"):
            geom_field_name = field.name
            break
        elif arrow_extension_name is None and field.name == lyr.GetGeometryColumn():
            geom_field_name = field.name
            break

    fields = [field for field in schema]
    schema_without_geom = pa.schema(
        list(filter(lambda f: f.name != geom_field_name, fields))
    )
    batches_without_geom = []
    non_geom_field_names = [
        f.name for f in filter(lambda f: f.name != geom_field_name, fields)
    ]
    if geom_field_name:
        schema_geom = pa.schema(
            list(filter(lambda f: f.name == geom_field_name, fields))
        )
        batches_with_geom = []
    for record_batch in stream:
        arrays_without_geom = [
            record_batch.field(field_name) for field_name in non_geom_field_names
        ]
        batch_without_geom = pa.RecordBatch.from_arrays(
            arrays_without_geom, schema=schema_without_geom
        )
        batches_without_geom.append(batch_without_geom)
        if geom_field_name:
            batch_with_geom = pa.RecordBatch.from_arrays(
                [record_batch.field(geom_field_name)], schema=schema_geom
            )
            batches_with_geom.append(batch_with_geom)

    table = pa.Table.from_batches(batches_without_geom)
    df = table.to_pandas()
    if geom_field_name:
        import geopandas as gp
        from geopandas.array import from_wkb, from_wkt

        if is_wkb:
            geometry = from_wkb(pa.Table.from_batches(batches_with_geom)[0])
        else:
            geometry = from_wkt(pa.Table.from_batches(batches_with_geom)[0])
        gdf = gp.GeoDataFrame(df, geometry=geometry)
        return gdf
    else:
        return df


def Usage():
    print("bench_ogr_to_geopandas.py [-spat xmin ymin xmax ymax] [-where cond]")
    print("                          filename [layer_name]")
    sys.exit(1)


if __name__ == "__main__":

    i = 1
    filename = None
    where = None
    minx = None
    miny = None
    maxx = None
    maxy = None
    layer_name = None
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
        elif sys.argv[i][0] == "-":
            Usage()
        elif filename is None:
            filename = sys.argv[i]
        elif layer_name is None:
            layer_name = sys.argv[i]
        else:
            Usage()
        i += 1

    if not filename:
        Usage()

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(layer_name if layer_name else 0)
    if minx:
        lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
    if where:
        lyr.SetAttributeFilter(where)
    print(layer_as_geopandas(lyr))
