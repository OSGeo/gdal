import os

from osgeo import ogr

ds = ogr.GetDriverByName("PARQUET").CreateDataSource(
    os.path.join(os.path.dirname(__file__), "refs.0.parq")
)
lyr = ds.CreateLayer("refs.0.parq", geom_type=ogr.wkbNone)
lyr.CreateField(ogr.FieldDefn("path", ogr.OFTString))
lyr.CreateField(ogr.FieldDefn("offset", ogr.OFTInteger64))
lyr.CreateField(ogr.FieldDefn("size", ogr.OFTInteger64))
# lyr.CreateField(ogr.FieldDefn("raw", ogr.OFTBinary))
f = ogr.Feature(lyr.GetLayerDefn())
lyr.CreateFeature(f)
