import sys

from osgeo import gdal, ogr

gdal.UseExceptions()
driverName = "ESRI Shapefile"
drv = gdal.GetDriverByName(driverName)
if drv is None:
    print("%s driver not available." % driverName)
    sys.exit(1)

ds = drv.Create("point_out.shp", 0, 0, 0, gdal.GDT_Unknown)

lyr = ds.CreateLayer("point_out", None, ogr.wkbPoint)

field_defn = ogr.FieldDefn("Name", ogr.OFTString)
field_defn.SetWidth(32)

lyr.CreateField(field_defn)

# Expected format of user input: x y name
linestring = input()
linelist = linestring.split()

while len(linelist) == 3:
    x = float(linelist[0])
    y = float(linelist[1])
    name = linelist[2]

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("Name", name)

    pt = ogr.Geometry(ogr.wkbPoint)
    pt.SetPoint_2D(0, x, y)

    feat.SetGeometry(pt)

    lyr.CreateFeature(feat)

    linestring = input()
    linelist = linestring.split()

ds.Close()
