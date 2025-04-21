import sys
from osgeo import gdal
from osgeo import ogr

gdal.UseExceptions()
driverName = "ESRI Shapefile"
drv = gdal.GetDriverByName(driverName)
if drv is None:
    print("%s driver not available." % driverName)
    sys.exit(1)

ds = drv.Create("point_out.shp", 0, 0, 0, gdal.GDT_Unknown)

lyr = ds.CreateLayer("point_out", None, ogr.wkbPoint)
if lyr is None:
    print("Layer creation failed.\n")
    sys.exit(1)

field_defn = ogr.FieldDefn("Name", ogr.OFTString)
field_defn.SetWidth(32)

if lyr.CreateField(field_defn) != 0:
    print("Creating Name field failed.\n")
    sys.exit(1)

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

    if lyr.CreateFeature(feat) != 0:
        print("Failed to create feature in shapefile.\n")
        sys.exit(1)

    feat.Destroy()

    linestring = input()
    linelist = linestring.split()

ds = None
