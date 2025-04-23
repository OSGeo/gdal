import struct

from osgeo import gdal, ogr, osr

gdal.RmdirRecursive("test.gdb")
ds = ogr.GetDriverByName("FileGDB").CreateDataSource("test.gdb")
srs = osr.SpatialReference()
# srs.SetFromUserInput("+proj=tmerc +lon_0=-40 +lat_0=60 +k=0.9 +x_0=00000 +y_0=00000 +datum=WGS84")
srs.SetFromUserInput(
    "+proj=merc +lon_0=-40 +lat_0=60 +k=0.9 +x_0=00000 +y_0=00000 +datum=WGS84"
)
srs_lonlat = srs.CloneGeogCS()
srs_lonlat.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
ct = osr.CoordinateTransformation(srs, srs_lonlat)
grid_step = 0.1
lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, srs=srs)
# Add 2 dummy features to set the grid_step to what we want
f = ogr.Feature(lyr.GetLayerDefn())
f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(0 0)"))
lyr.CreateFeature(f)
f = ogr.Feature(lyr.GetLayerDefn())
y = (2**0.5) * grid_step
f.SetGeometry(ogr.CreateGeometryFromWkt("POINT(%f %f)" % (y, y)))
lyr.CreateFeature(f)
ds = None
ds = ogr.GetDriverByName("FileGDB").Open("test.gdb", update=1)
lyr = ds.GetLayer(0)
lyr.DeleteFeature(1)
lyr.DeleteFeature(2)
y = 1e9  # some big value
f = ogr.Feature(lyr.GetLayerDefn())
g = ogr.CreateGeometryFromWkt("POINT(0 %f)" % y)
f.SetGeometry(g)
lyr.CreateFeature(f)
f = ogr.Feature(lyr.GetLayerDefn())
g = ogr.CreateGeometryFromWkt("POINT(0 %f)" % -y)
f.SetGeometry(g)
lyr.CreateFeature(f)
ds = None
f = open("test.gdb/a00000009.spx", "rb")
f.seek(4)
n = ord(f.read(1))
for x in range(n):
    f.seek(1372 + x * 8, 0)
    v = struct.unpack("Q", f.read(8))[0]
    x = v >> 31 & 0x7FFFFFFF - (1 << 29)
    y = (v & 0x7FFFFFFF) - (1 << 29)
    print(x, y)  # the y value will be clamped
    print(
        ct.TransformPoint(srs.GetProjParm(osr.SRS_PP_FALSE_EASTING, 0.0), y * grid_step)
    )
